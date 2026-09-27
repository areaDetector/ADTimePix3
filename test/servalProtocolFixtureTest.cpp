/*
 * ADTimePix3 deterministic fake Serval fixture tests
 *
 * SPDX-License-Identifier: MIT
 */

#include "FakeServalHttpServer.h"
#include "FakeServalTcpServer.h"
#include "acquisition_coordinator.h"
#include "bpc_file_io.h"
#include "bpc_mask_semantics.h"
#include "mask_geometry.h"
#include "network_client.h"
#include "numeric_range.h"
#include "one_shot_action.h"
#include "rolling_window_sum.h"
#include "serval_config.h"
#include "serval_dacs.h"
#include "serval_dashboard.h"
#include "serval_detector.h"
#include "serval_destination.h"
#include "serval_health.h"
#include "serval_http.h"
#include "serval_measurement.h"
#include "serval_pixel_config.h"
#include "serval_reconnect.h"
#include "serval_stream_framing.h"
#include "serval_stream_validation.h"
#include "stream_worker_state.h"

#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <deque>
#include <fstream>
#include <limits>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <epicsUnitTest.h>
#include <testMain.h>

#include <json.hpp>

using adtimepix_test::FakeHttpRequest;
using adtimepix_test::FakeHttpResponse;
using adtimepix_test::FakeServalHttpServer;
using adtimepix_test::FakeServalTcpServer;

namespace {

const std::chrono::milliseconds kFixtureDeadline(1000);

class TemporaryFile {
public:
    TemporaryFile()
    {
        char path[] = "/tmp/adtimepix3-bpc-XXXXXX";
        const int fd = mkstemp(path);
        if (fd >= 0) close(fd);
        path_ = path;
    }

    ~TemporaryFile() { unlink(path_.c_str()); }

    const std::string& path() const { return path_; }

private:
    std::string path_;
};

void replaceFileContents(const std::string& path, std::size_t size)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    const std::vector<char> bytes(size, static_cast<char>(0x5a));
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

int connectLoopback(unsigned short port)
{
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    if (connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

bool sendAll(int fd, const std::string& bytes)
{
    std::size_t sent = 0;
    while (sent < bytes.size()) {
        const ssize_t count = send(fd, bytes.data() + sent, bytes.size() - sent, MSG_NOSIGNAL);
        if (count <= 0) {
            return false;
        }
        sent += static_cast<std::size_t>(count);
    }
    return true;
}

int waitReadable(int fd, std::chrono::milliseconds timeout)
{
    pollfd descriptor{fd, POLLIN, 0};
    return poll(&descriptor, 1, static_cast<int>(timeout.count()));
}

bool receiveExactly(int fd, std::size_t size, std::string& output,
                    std::chrono::milliseconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    output.clear();
    while (output.size() < size) {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
        if (remaining.count() <= 0 || waitReadable(fd, remaining) <= 0) {
            return false;
        }
        char buffer[256];
        const std::size_t wanted = std::min(sizeof(buffer), size - output.size());
        const ssize_t count = recv(fd, buffer, wanted, 0);
        if (count <= 0) {
            return false;
        }
        output.append(buffer, static_cast<std::size_t>(count));
    }
    return true;
}

bool receiveToEof(int fd, std::string& output, std::chrono::milliseconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    output.clear();
    while (std::chrono::steady_clock::now() < deadline) {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
        if (waitReadable(fd, remaining) <= 0) {
            return false;
        }
        char buffer[512];
        const ssize_t count = recv(fd, buffer, sizeof(buffer), 0);
        if (count == 0) {
            return true;
        }
        if (count < 0) {
            return false;
        }
        output.append(buffer, static_cast<std::size_t>(count));
    }
    return false;
}

void testTcpScript()
{
    const std::string header = "{\"width\":2,\"height\":1}\n";
    const std::string pixels("\x00\x01\x00\x02", 4);
    FakeServalTcpServer server({header.substr(0, 7), header.substr(7), pixels});
    const int client = connectLoopback(server.port());

    testOk(client >= 0, "TCP fixture accepts a loopback client");
    testOk(server.waitForClient(kFixtureDeadline), "TCP fixture observes the client before deadline");

    server.releaseNextChunk();
    std::string received;
    testOk(receiveExactly(client, 7, received, kFixtureDeadline) && received == header.substr(0, 7),
           "TCP fixture releases one exact scripted fragment");

    server.releaseAllChunks();
    std::string remainder;
    const std::string expected = header.substr(7) + pixels;
    testOk(receiveExactly(client, expected.size(), remainder, kFixtureDeadline) && remainder == expected,
           "TCP fixture preserves coalesced header and binary bytes");
    testOk(server.waitForChunksSent(3, kFixtureDeadline),
           "TCP fixture reports all scripted chunks sent");
    char eofProbe = 0;
    testOk(waitReadable(client, kFixtureDeadline) > 0 && recv(client, &eofProbe, 1, 0) == 0,
           "TCP fixture closes after its final chunk");
    close(client);
}

void testTcpSilenceIsBounded()
{
    const auto started = std::chrono::steady_clock::now();
    {
        FakeServalTcpServer server({"not released"}, false);
        const int client = connectLoopback(server.port());
        testOk(client >= 0 && server.waitForClient(kFixtureDeadline),
               "silent TCP fixture accepts a loopback client");
        testOk(waitReadable(client, std::chrono::milliseconds(75)) == 0,
               "silent TCP fixture sends no unsolicited data");
        close(client);
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started);
    testOk(elapsed < kFixtureDeadline, "silent TCP fixture teardown is bounded");
}

void testProductionNetworkClient()
{
    const std::string expected("header\n\x00\x01\x02\x03", 11);
    FakeServalTcpServer server({expected.substr(0, 3), expected.substr(3, 4),
                                expected.substr(7)});
    NetworkClient client;

    testOk(client.connect("127.0.0.1", server.port()),
           "production NetworkClient connects to the fake TCP peer");
    testOk(server.waitForClient(kFixtureDeadline),
           "fake TCP peer observes the production client before deadline");
    server.releaseAllChunks();
    std::vector<char> received(expected.size());
    testOk(client.receive_exact(received.data(), received.size()) &&
               std::string(received.begin(), received.end()) == expected,
           "production receive_exact joins deterministic TCP fragments byte-for-byte");
    testOk(server.waitForChunksSent(3, kFixtureDeadline),
           "production client consumes the complete scripted TCP payload");

    FakeServalTcpServer silentServer({"not released"}, false);
    NetworkClient silentClient;
    testOk(silentClient.connect("127.0.0.1", silentServer.port()) &&
               silentServer.waitForClient(kFixtureDeadline),
           "production NetworkClient connects to a silent TCP peer");
    char byte = 0;
    const auto started = std::chrono::steady_clock::now();
    errno = 0;
    const ssize_t silentRead = silentClient.receive(&byte, 1);
    const int receiveError = errno;
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started);
    testOk(silentRead < 0 && NetworkClient::isReceiveTimeout(receiveError) &&
               elapsed < kFixtureDeadline,
           "production NetworkClient bounds a receive from a silent peer");
    testOk(NetworkClient::kReceivePollTimeoutMs == 250,
           "production stream receive polling uses the documented interval");

    FakeServalTcpServer interruptServer({"not released"}, false);
    NetworkClient interruptClient;
    testOk(interruptClient.connect("127.0.0.1", interruptServer.port()) &&
               interruptServer.waitForClient(kFixtureDeadline),
           "production NetworkClient connects before blocked-receive interruption");

    std::atomic<bool> receiveStarted(false);
    ssize_t interruptRead = 1;
    int interruptError = 0;
    std::thread receiver([&]() {
        char interruptByte = 0;
        receiveStarted.store(true, std::memory_order_release);
        errno = 0;
        interruptRead = interruptClient.receive(&interruptByte, 1);
        interruptError = errno;
    });
    while (!receiveStarted.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    const auto interruptStarted = std::chrono::steady_clock::now();
    interruptClient.interrupt();
    receiver.join();
    const auto interruptElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - interruptStarted);
    testOk(interruptRead <= 0 && !interruptClient.is_connected(),
           "interrupt terminates a receive from a silent peer and clears connection state");
    testOk(!NetworkClient::isReceiveTimeout(interruptError) &&
               interruptElapsed < kFixtureDeadline,
           "interrupt wakes a blocked receive without a polling timeout");
    interruptClient.interrupt();
    interruptClient.disconnect();
    testOk(!interruptClient.is_connected(),
           "repeated interrupt and disconnect are idempotent");
}

void testAcquisitionCoordinator()
{
    using ADTimePix3Acquisition::Coordinator;
    using ADTimePix3Acquisition::Phase;
    using ADTimePix3Acquisition::Resource;

    Coordinator incomplete;
    testOk(incomplete.snapshot().phase == Phase::Idle,
           "acquisition coordinator starts idle");
    testOk(incomplete.beginStart() && !incomplete.beginStart() &&
               incomplete.snapshot().phase == Phase::Starting,
           "acquisition coordinator admits one start transition");
    testOk(!incomplete.commitStart(),
           "acquisition start cannot commit without remote and monitor resources");

    const Resource resources[] = {
        Resource::RemoteMeasurement,
        Resource::PreviewWorker,
        Resource::IntegratedPreviewWorker,
        Resource::ImageWorker,
        Resource::HistogramWorker,
        Resource::MonitorThread};
    bool faultMatrixPassed = true;
    for (std::size_t failure = 0;
         failure < sizeof(resources) / sizeof(resources[0]); ++failure) {
        Coordinator partial;
        faultMatrixPassed = faultMatrixPassed && partial.beginStart();
        for (std::size_t resource = 0; resource <= failure; ++resource) {
            faultMatrixPassed =
                faultMatrixPassed && partial.noteResource(resources[resource]);
        }
        const auto cleanup = partial.beginStop(true);
        faultMatrixPassed = faultMatrixPassed && cleanup.owner && cleanup.fault;
        for (std::size_t resource = 0; resource <= failure; ++resource) {
            faultMatrixPassed =
                faultMatrixPassed && cleanup.snapshot.has(resources[resource]);
        }
        faultMatrixPassed =
            faultMatrixPassed && partial.finishStop(true, true) == Phase::Fault &&
            partial.snapshot().resources == 0 && partial.beginStart();
    }
    testOk(faultMatrixPassed,
           "every injected partial-start failure owns complete rollback and permits retry");

    Coordinator running;
    testOk(running.beginStart() &&
               running.noteResource(Resource::RemoteMeasurement) &&
               running.noteResource(Resource::MonitorThread) &&
               running.commitStart() && running.requestedAcquire(),
           "remote and monitor readiness commit one running acquisition");
    testOk(running.waitForRunningFor(std::chrono::milliseconds(1)),
           "monitor readiness gate observes committed running state");
    const auto cleanup = running.beginStop(false);
    testOk(cleanup.owner && !cleanup.fault &&
               cleanup.snapshot.has(Resource::RemoteMeasurement) &&
               cleanup.snapshot.has(Resource::MonitorThread),
           "normal stop claims every committed acquisition resource");
    testOk(!running.beginStop(false).owner,
           "duplicate stop is idempotent while cleanup is owned");
    testOk(running.finishStop(true, false) == Phase::Idle &&
               running.snapshot().resources == 0 &&
               !running.requestedAcquire(),
           "successful cleanup returns to truthful idle state");

    Coordinator uncertainRemote;
    testOk(uncertainRemote.beginStart() &&
               uncertainRemote.noteResource(Resource::RemoteMeasurement) &&
               uncertainRemote.noteResource(Resource::MonitorThread) &&
               uncertainRemote.commitStart(),
           "remote-stop fault fixture reaches running state");
    testOk(uncertainRemote.beginStop(false).owner,
           "remote-stop fault fixture claims cleanup");
    testOk(uncertainRemote.finishStop(false, false) == Phase::Fault &&
               uncertainRemote.snapshot().has(Resource::RemoteMeasurement),
           "unconfirmed remote stop retains ownership in fault state");
    testOk(std::string(ADTimePix3Acquisition::connectedStatusMessage(
               uncertainRemote.snapshot())) ==
               "Acquisition fault; write Acquire=0 to retry cleanup",
           "connected unresolved fault reports the explicit cleanup action");
    testOk(!uncertainRemote.beginStart(),
           "new start is rejected while remote acquisition ownership is unresolved");
    testOk(uncertainRemote.beginStop(false).owner,
           "explicit stop retries unresolved remote cleanup");
    testOk(uncertainRemote.finishStop(true, false) == Phase::Idle,
           "successful retry reconciles unresolved remote state to idle");

    Coordinator concurrent;
    (void)concurrent.beginStart();
    (void)concurrent.noteResource(Resource::RemoteMeasurement);
    (void)concurrent.noteResource(Resource::MonitorThread);
    (void)concurrent.commitStart();
    std::atomic<int> owners(0);
    std::thread stopA([&]() {
        if (concurrent.beginStop(false).owner) ++owners;
    });
    std::thread stopB([&]() {
        if (concurrent.beginStop(true).owner) ++owners;
    });
    stopA.join();
    stopB.join();
    testOk(owners.load() == 1,
           "concurrent stop and fault requests elect exactly one cleanup owner");
    testOk(concurrent.finishStop(true, false) == Phase::Fault,
           "a racing fault request remains latched through cleanup completion");
    testOk(std::string(ADTimePix3Acquisition::connectedStatusMessage(
               concurrent.snapshot())) ==
               "Acquisition fault; retry acquisition",
           "connected resolved fault does not retain a stale disconnect message");
}

void testStreamWorkerState()
{
    using ADTimePix3StreamWorker::State;

    State state;
    testOk(!state.running() && !state.connected(),
           "stream worker state starts stopped and disconnected");
    testOk(state.start() && !state.start() && state.running(),
           "stream worker state admits exactly one start per generation");
    state.markConnected();
    testOk(state.connected(),
           "running stream worker can publish connected state");
    testOk(state.waitForConnectedFor(std::chrono::milliseconds(1)),
           "worker readiness wait observes a connected stream");

    std::atomic<bool> waiterStarted(false);
    bool stopObserved = false;
    std::thread waiter([&]() {
        waiterStarted.store(true, std::memory_order_release);
        stopObserved = state.waitForStopFor(kFixtureDeadline);
    });
    while (!waiterStarted.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    state.requestStop();
    waiter.join();
    testOk(stopObserved, "stop request wakes an interruptible worker wait");
    testOk(!state.running() && !state.connected(),
           "stop request atomically leaves the worker stopped and disconnected");
    testOk(!state.waitForConnectedFor(std::chrono::milliseconds(1)),
           "worker readiness wait rejects a stopped stream");

    testOk(state.start(), "stream worker state permits a new generation after stop");
    state.markConnected();
    state.fail();
    testOk(!state.running() && !state.connected(),
           "worker failure clears running and connected state");

    bool concurrentStopPassed = true;
    for (int cycle = 0; cycle < 100; ++cycle) {
        concurrentStopPassed = concurrentStopPassed && state.start();
        std::atomic<bool> publish(false);
        std::thread publisher([&]() {
            while (!publish.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            state.markConnected();
        });
        publish.store(true, std::memory_order_release);
        state.requestStop();
        publisher.join();
        concurrentStopPassed = concurrentStopPassed &&
                               !state.running() && !state.connected();
    }
    testOk(concurrentStopPassed,
           "concurrent connection publication cannot revive a stopped worker");

    bool cyclesPassed = true;
    for (int cycle = 0; cycle < 100; ++cycle) {
        cyclesPassed = cyclesPassed && state.start();
        state.markConnected();
        state.requestStop();
        cyclesPassed = cyclesPassed && !state.running() && !state.connected();
    }
    testOk(cyclesPassed, "stream worker state survives repeated start-stop cycles");
}

void testConsumeOnceStreamFraming()
{
    using ADTimePix3Stream::EndOfStreamStatus;
    using ADTimePix3Stream::FrameBuffer;
    using ADTimePix3Stream::FramedMessage;
    using ADTimePix3Stream::FrameResult;

    int resolverCalls = 0;
    const auto resolver = [&resolverCalls](const std::string& header,
                                           std::size_t& payloadBytes,
                                           std::string& error) {
        ++resolverCalls;
        const nlohmann::json parsed = nlohmann::json::parse(
            header, nullptr, false);
        if (!parsed.is_object() || !parsed.contains("payloadBytes") ||
            !parsed["payloadBytes"].is_number_unsigned()) {
            error = "missing unsigned payloadBytes";
            return false;
        }
        payloadBytes = parsed["payloadBytes"].get<std::size_t>();
        return true;
    };

    const std::string header1 = "{\"payloadBytes\":4}\n";
    const std::string payload1("\n{\x00\xff", 4);
    const std::string header2 = "{\"payloadBytes\":2}\n";
    const std::string payload2("}\n", 2);
    FrameBuffer frames(128, 16, 128);
    FramedMessage frame;
    std::string error;

    testOk(frames.append(header1.data(), 7) == FrameResult::NeedMoreData &&
               frames.next(resolver, frame, error) == FrameResult::NeedMoreData &&
               resolverCalls == 0,
           "consume-once framing waits for a fragmented JSON header");

    const std::string firstRemainder = header1.substr(7) + payload1.substr(0, 2);
    frames.append(firstRemainder.data(), firstRemainder.size());
    testOk(frames.next(resolver, frame, error) == FrameResult::NeedMoreData &&
               frames.awaitingPayload() && resolverCalls == 1,
           "consume-once framing resolves one header then waits for its declared payload");

    const std::string coalesced = payload1.substr(2) + "\n" +
                                  header2 + payload2 + "\n";
    frames.append(coalesced.data(), coalesced.size());
    testOk(frames.next(resolver, frame, error) == FrameResult::FrameReady &&
               frame.header == header1.substr(0, header1.size() - 1) &&
               frames.bufferedBytes() == header2.size() + payload2.size() + 1,
           "consume-once framing extracts one frame and retains the coalesced successor");
    testOk(std::string(reinterpret_cast<const char*>(frame.payload.data()),
                       frame.payload.size()) == payload1,
           "consume-once framing preserves newline, brace, NUL, and high-bit payload bytes");

    testOk(frames.next(resolver, frame, error) == FrameResult::FrameReady &&
               frame.header == header2.substr(0, header2.size() - 1) &&
               std::string(reinterpret_cast<const char*>(frame.payload.data()),
                           frame.payload.size()) == payload2 &&
               frames.bufferedBytes() == 0,
           "consume-once framing extracts the retained second message exactly once");
    testOk(resolverCalls == 2,
           "consume-once framing resolves each coalesced header exactly once");

    FrameBuffer crlfFrame(64, 4, 16);
    const std::string crlf = "{\"payloadBytes\":0}\r\n\n";
    crlfFrame.append(crlf.data(), crlf.size());
    testOk(crlfFrame.next(resolver, frame, error) == FrameResult::FrameReady &&
               frame.header == "{\"payloadBytes\":0}" && frame.payload.empty(),
           "consume-once framing accepts CRLF and a zero-length payload");

    FrameBuffer invalidHeader(64, 4, 16);
    const std::string invalid = "not-json\n";
    invalidHeader.append(invalid.data(), invalid.size());
    testOk(invalidHeader.next(resolver, frame, error) == FrameResult::InvalidHeader &&
               !error.empty(),
           "consume-once framing fails closed when payload size cannot be resolved");

    FrameBuffer longHeader(4, 4, 4);
    const std::string fiveBytes = "12345";
    longHeader.append(fiveBytes.data(), fiveBytes.size());
    testOk(longHeader.next(resolver, frame, error) == FrameResult::HeaderTooLarge,
           "consume-once framing rejects a header beyond its configured limit");

    FrameBuffer largePayload(64, 3, 16);
    largePayload.append(header1.data(), header1.size());
    testOk(largePayload.next(resolver, frame, error) == FrameResult::PayloadTooLarge,
           "consume-once framing rejects a declared payload beyond its configured limit");

    FrameBuffer boundedBuffer(4, 4, 0);
    const std::string elevenBytes(11, 'x');
    testOk(boundedBuffer.append(elevenBytes.data(), elevenBytes.size()) ==
               FrameResult::BufferLimitExceeded,
           "consume-once framing enforces its total retained-byte limit");

    FrameBuffer truncatedHeader(64, 8, 16);
    truncatedHeader.append(header1.data(), 5);
    testOk(truncatedHeader.endOfStreamStatus() == EndOfStreamStatus::TruncatedHeader,
           "consume-once framing identifies EOF in a partial header");

    FrameBuffer truncatedPayload(64, 8, 16);
    truncatedPayload.append(header1.data(), header1.size());
    testOk(truncatedPayload.next(resolver, frame, error) == FrameResult::NeedMoreData &&
               truncatedPayload.endOfStreamStatus() == EndOfStreamStatus::TruncatedPayload,
           "consume-once framing identifies EOF in a declared payload");

    FrameBuffer truncatedTrailer(64, 8, 16);
    const std::string frameWithoutTrailer = header1 + payload1;
    truncatedTrailer.append(frameWithoutTrailer.data(), frameWithoutTrailer.size());
    testOk(truncatedTrailer.next(resolver, frame, error) == FrameResult::NeedMoreData &&
               truncatedTrailer.endOfStreamStatus() == EndOfStreamStatus::TruncatedTrailer,
           "consume-once framing identifies EOF before the payload terminator");

    FrameBuffer invalidTrailer(64, 8, 16);
    const std::string frameWithInvalidTrailer = header1 + payload1 + "x";
    invalidTrailer.append(frameWithInvalidTrailer.data(), frameWithInvalidTrailer.size());
    testOk(invalidTrailer.next(resolver, frame, error) == FrameResult::InvalidTrailer,
           "consume-once framing rejects a non-newline payload terminator");

    testOk(frames.endOfStreamStatus() == EndOfStreamStatus::Clean,
           "consume-once framing reports clean EOF after an exact frame boundary");

    const std::string wire = header1 + payload1 + "\n" +
                             header2 + payload2 + "\n";
    const auto drainFrames = [&resolver](FrameBuffer& candidate,
                                         std::vector<FramedMessage>& output) {
        while (true) {
            FramedMessage candidateFrame;
            std::string candidateError;
            const FrameResult result =
                candidate.next(resolver, candidateFrame, candidateError);
            if (result == FrameResult::NeedMoreData) {
                return true;
            }
            if (result != FrameResult::FrameReady) {
                return false;
            }
            output.push_back(std::move(candidateFrame));
        }
    };
    const auto exactMessages = [&header1, &header2, &payload1, &payload2](
                                   const std::vector<FramedMessage>& output) {
        return output.size() == 2 &&
            output[0].header == header1.substr(0, header1.size() - 1) &&
            output[1].header == header2.substr(0, header2.size() - 1) &&
            std::string(reinterpret_cast<const char*>(output[0].payload.data()),
                        output[0].payload.size()) == payload1 &&
            std::string(reinterpret_cast<const char*>(output[1].payload.data()),
                        output[1].payload.size()) == payload2;
    };

    bool everyTwoChunkSplitPassed = true;
    for (std::size_t split = 0; split <= wire.size(); ++split) {
        FrameBuffer candidate(128, 16, 128);
        std::vector<FramedMessage> output;
        everyTwoChunkSplitPassed = everyTwoChunkSplitPassed &&
            candidate.append(wire.data(), split) == FrameResult::NeedMoreData &&
            drainFrames(candidate, output) &&
            candidate.append(wire.data() + split, wire.size() - split) ==
                FrameResult::NeedMoreData &&
            drainFrames(candidate, output) && exactMessages(output) &&
            candidate.bufferedBytes() == 0;
    }
    testOk(everyTwoChunkSplitPassed,
           "consume-once framing handles every two-chunk split across two messages");

    FrameBuffer bytewise(128, 16, 128);
    std::vector<FramedMessage> bytewiseOutput;
    bool bytewisePassed = true;
    for (char byteValue : wire) {
        bytewisePassed = bytewisePassed &&
            bytewise.append(&byteValue, 1) == FrameResult::NeedMoreData &&
            drainFrames(bytewise, bytewiseOutput);
    }
    testOk(bytewisePassed && exactMessages(bytewiseOutput) &&
               bytewise.bufferedBytes() == 0,
           "consume-once framing handles byte-at-a-time delivery without duplication");

    FrameBuffer reconnect(128, 16, 128);
    reconnect.append(header1.data(), header1.size() / 2);
    reconnect.clear();
    const std::string afterReconnect = header2 + payload2 + "\n";
    reconnect.append(afterReconnect.data(), afterReconnect.size());
    std::vector<FramedMessage> reconnectOutput;
    testOk(drainFrames(reconnect, reconnectOutput) &&
               reconnectOutput.size() == 1 &&
               reconnectOutput[0].header == header2.substr(0, header2.size() - 1) &&
               std::string(reinterpret_cast<const char*>(
                              reconnectOutput[0].payload.data()),
                          reconnectOutput[0].payload.size()) == payload2 &&
               reconnect.bufferedBytes() == 0,
           "consume-once framing discards a partial old connection before reconnect data");
}

void testHttpRequestAndResponse()
{
    FakeHttpResponse response;
    response.status = 503;
    response.reason = "Service Unavailable";
    response.headers["Content-Type"] = "application/json";
    response.body = "{malformed";
    FakeServalHttpServer server(response, true);
    const int client = connectLoopback(server.port());
    const std::string body = "{\"FixtureValue\":1}";
    const std::string request =
        "PUT /detector/config HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Type: application/json\r\n" +
        std::string("Content-Length: ") + std::to_string(body.size()) + "\r\n\r\n" + body;

    testOk(client >= 0 && sendAll(client, request), "HTTP fixture accepts a complete request");
    testOk(server.waitForRequest(kFixtureDeadline), "HTTP fixture records the request before deadline");
    const FakeHttpRequest recorded = server.request();
    testOk(recorded.method == "PUT" && recorded.target == "/detector/config",
           "HTTP fixture records method and target");
    const auto contentType = recorded.headers.find("content-type");
    testOk(contentType != recorded.headers.end() && contentType->second == "application/json" &&
               recorded.body == body,
           "HTTP fixture records normalized headers and exact body");
    testOk(server.baseUrl().find("http://127.0.0.1:") == 0,
           "HTTP fixture exposes only a loopback base URL");
    testOk(waitReadable(client, std::chrono::milliseconds(75)) == 0,
           "HTTP fixture can hold a response for timeout tests");

    server.releaseResponse();
    std::string wireResponse;
    testOk(receiveToEof(client, wireResponse, kFixtureDeadline),
           "HTTP fixture response completes before deadline");
    testOk(wireResponse.find("HTTP/1.1 503 Service Unavailable\r\n") == 0 &&
               wireResponse.find("\r\n\r\n{malformed") != std::string::npos,
           "HTTP fixture returns configured status and malformed body verbatim");
    close(client);
}

void testProductionHttpClient()
{
    FakeHttpResponse response;
    response.status = 200;
    response.reason = "OK";
    response.headers["Content-Type"] = "application/json";
    response.body = "{\"ok\":true}";

    FakeServalHttpServer normalServer(response);
    const std::string body = "{\"BiasEnabled\":true}";
    const cpr::Response normal = ADTimePix3ServalHttp::putJson(
        normalServer.baseUrl() + "/detector/config", body, 1000);
    testOk(normal.status_code == 200 && normal.text == response.body,
           "production HTTP helper returns the fake Serval response");
    const FakeHttpRequest recorded = normalServer.request();
    const auto contentType = recorded.headers.find("content-type");
    testOk(recorded.method == "PUT" && recorded.target == "/detector/config" &&
               recorded.body == body && contentType != recorded.headers.end() &&
               contentType->second == "application/json",
           "production HTTP helper sends the exact JSON request");

    FakeServalHttpServer heldServer(response, true);
    const auto started = std::chrono::steady_clock::now();
    const cpr::Response timedOut = ADTimePix3ServalHttp::getAuthOnly(
        heldServer.baseUrl() + "/slow", 100);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started);
    testOk(heldServer.waitForRequest(kFixtureDeadline),
           "fake Serval observes the bounded production HTTP request");
    testOk(timedOut.error.code == cpr::ErrorCode::OPERATION_TIMEDOUT &&
               elapsed < kFixtureDeadline,
           "production HTTP helper returns after its configured timeout");
    testOk(ADTimePix3ServalHttp::kDefaultTimeoutMs == 10000,
           "production HTTP helpers use the documented ten-second default timeout");
}

void testReconnectPolicy()
{
    using ADTimePix3ServalReconnect::Readback;

    FakeHttpResponse response;
    response.body = "{}";
    std::vector<FakeHttpRequest> requests;
    const ADTimePix3ServalReconnect::Result result =
        ADTimePix3ServalReconnect::refresh([&](Readback readback) {
            FakeServalHttpServer server(response);
            const cpr::Response received = ADTimePix3ServalHttp::get(
                server.baseUrl() + ADTimePix3ServalReconnect::path(readback), 1000);
            if (server.waitForRequest(kFixtureDeadline)) {
                requests.push_back(server.request());
            }
            return received.status_code == 200;
        });

    testOk(result.complete() && requests.size() == 3,
           "automatic reconnect completes all required readbacks");
    const auto isGetFor = [](const FakeHttpRequest& request, const char* resource) {
        const std::size_t query = request.target.find('?');
        return request.method == "GET" && request.target.substr(0, query) == resource;
    };
    testOk(requests.size() == 3 &&
               isGetFor(requests[0], "/server/destination") &&
               isGetFor(requests[1], "/detector") &&
               isGetFor(requests[2], "/measurement/config"),
           "automatic reconnect sends only the three documented GET requests");

    std::vector<Readback> attempted;
    const ADTimePix3ServalReconnect::Result partial =
        ADTimePix3ServalReconnect::refresh([&](Readback readback) {
            attempted.push_back(readback);
            return readback != Readback::Detector;
        });
    testOk(!partial.complete() && partial.destination && !partial.detector &&
               partial.measurementConfig && attempted.size() == 3,
           "automatic reconnect reports a partial refresh and still attempts every readback");
}


void testMeasurementResponseValidation()
{
    using ADTimePix3ServalMeasurement::ParseError;

    nlohmann::json measurement;
    testOk(ADTimePix3ServalMeasurement::parseResponse(
               "{\"Info\":{\"Status\":\"DA_RECORDING\",\"FrameCount\":12}}",
               measurement) == ParseError::None &&
               measurement["Info"]["FrameCount"].get<int>() == 12,
           "measurement parser accepts the Serval Info response shape");
    testOk(ADTimePix3ServalMeasurement::parseResponse(
               "{\"Status\":\"DA_IDLE\"}", measurement) == ParseError::None &&
               measurement["Status"].get<std::string>() == "DA_IDLE",
           "measurement parser accepts the legacy top-level status shape");
    testOk(ADTimePix3ServalMeasurement::parseResponse("", measurement) ==
               ParseError::EmptyBody && measurement.is_object() && measurement.empty(),
           "measurement parser rejects an empty HTTP-200 body");
    testOk(ADTimePix3ServalMeasurement::parseResponse("{malformed", measurement) ==
               ParseError::MalformedJson && measurement.is_object() && measurement.empty(),
           "measurement parser rejects malformed JSON without throwing");
    testOk(ADTimePix3ServalMeasurement::parseResponse("[]", measurement) ==
               ParseError::InvalidRoot && measurement.is_object() && measurement.empty(),
           "measurement parser rejects a non-object JSON root");

    using ADTimePix3ServalMeasurement::StatusResponseError;
    using ADTimePix3ServalMeasurement::StatusSnapshot;
    StatusSnapshot snapshot;
    const std::string completeStatus =
        "{\"Info\":{\"PixelEventRate\":120,\"Tdc1EventRate\":10,"
        "\"Tdc2EventRate\":11,\"StartDateTime\":1758542400000,"
        "\"ElapsedTime\":1.5,\"TimeLeft\":2.5,\"FrameCount\":12,"
        "\"DroppedFrames\":0,\"Status\":\"DA_RECORDING\"}}";
    testOk(ADTimePix3ServalMeasurement::parseStatusResponse(completeStatus, snapshot) ==
               StatusResponseError::None && snapshot.hasPixelEventRate &&
               snapshot.pixelEventRate == 120 && snapshot.tdc1EventRate == 10 &&
               snapshot.tdc2EventRate == 11 && snapshot.startDateTime == 1758542400000LL &&
               snapshot.elapsedTime == 1.5 && snapshot.timeLeft == 2.5 &&
               snapshot.frameCount == 12 && snapshot.droppedFrames == 0 &&
               snapshot.status == "DA_RECORDING",
           "measurement status parser extracts one complete validated snapshot");
    testOk(ADTimePix3ServalMeasurement::parseStatusResponse(
               "{\"Info\":{\"TdcEventRate\":9},\"Status\":\"DA_IDLE\"}", snapshot) ==
               StatusResponseError::None && snapshot.hasTdc1EventRate &&
               snapshot.tdc1EventRate == 9 && !snapshot.hasTdc2EventRate &&
               snapshot.status == "DA_IDLE",
           "measurement status parser supports legacy TDC rate and top-level status");
    testOk(ADTimePix3ServalMeasurement::parseStatusResponse(
               "{\"Config\":{},\"Info\":null}", snapshot) ==
               StatusResponseError::None && snapshot.hasStatus &&
               snapshot.status == "DA_IDLE",
           "measurement status parser maps Serval null Info to idle");
    testOk(ADTimePix3ServalMeasurement::parseStatusResponse(
               "{\"Info\":[]}", snapshot) == StatusResponseError::InvalidInfo,
           "measurement status parser rejects a non-null non-object Info field");
    testOk(ADTimePix3ServalMeasurement::parseStatusResponse(
               "{\"Info\":{\"Status\":4}}", snapshot) ==
               StatusResponseError::InvalidStatus,
           "measurement status parser rejects a non-string status");
    testOk(ADTimePix3ServalMeasurement::parseStatusResponse(
               "{\"Info\":{\"FrameCount\":\"12\"}}", snapshot) ==
               StatusResponseError::InvalidMetric,
           "measurement status parser rejects an incorrectly typed metric");
    testOk(ADTimePix3ServalMeasurement::parseStatusResponse(
               "{\"Info\":{\"FrameCount\":18446744073709551615}}", snapshot) ==
               StatusResponseError::InvalidMetric,
           "measurement status parser rejects frame counts beyond the EPICS integer range");
    testOk(ADTimePix3ServalMeasurement::parseStatusResponse(
               "{\"Info\":{\"StartDateTime\":18446744073709551615}}", snapshot) ==
               StatusResponseError::InvalidMetric,
           "measurement status parser rejects timestamps beyond the EPICS integer range");
    testOk(ADTimePix3ServalMeasurement::parseStatusResponse(
               "{malformed", snapshot) == StatusResponseError::MalformedJson,
           "measurement status parser rejects malformed JSON without throwing");

    using ADTimePix3ServalMeasurement::ConfigResponseError;
    nlohmann::json config;
    const std::string completeConfig =
        "{\"Stem\":{},\"Corrections\":{\"Enabled\":true},\"Processing\":{\"Mode\":\"raw\"}}";
    testOk(ADTimePix3ServalMeasurement::parseConfigResponse(200, completeConfig, config) ==
               ConfigResponseError::None && config["Corrections"]["Enabled"] == true &&
               config["Processing"]["Mode"] == "raw",
           "measurement-config parser preserves unmodified merge sections");
    testOk(ADTimePix3ServalMeasurement::parseConfigResponse(500, completeConfig, config) ==
               ConfigResponseError::HttpFailure && config.is_object() && config.empty(),
           "measurement-config parser rejects a failed GET before merge");
    testOk(ADTimePix3ServalMeasurement::parseConfigResponse(200, "", config) ==
               ConfigResponseError::EmptyBody && config.is_object() && config.empty(),
           "measurement-config parser rejects an empty merge base");
    testOk(ADTimePix3ServalMeasurement::parseConfigResponse(200, "{malformed", config) ==
               ConfigResponseError::MalformedJson && config.is_object() && config.empty(),
           "measurement-config parser rejects malformed JSON before merge");
    testOk(ADTimePix3ServalMeasurement::parseConfigResponse(200, "[]", config) ==
               ConfigResponseError::InvalidRoot && config.is_object() && config.empty(),
           "measurement-config parser rejects a non-object merge base");

    using ADTimePix3ServalMeasurement::ConfigReadbackError;
    using ADTimePix3ServalMeasurement::ConfigSnapshot;
    ConfigSnapshot configSnapshot;
    const std::string completeReadback =
        "{\"Corrections\":{\"Multiply\":null},\"Processing\":{\"Binning\":null},"
        "\"TimeOfFlight\":{\"TdcReference\":[\"PN0123\",\"PN0123\"],"
        "\"Min\":0.0,\"Max\":1e99},\"Stem\":{\"Scan\":{\"Width\":1024,"
        "\"Height\":1024,\"DwellTime\":1e-6},\"VirtualDetector\":{"
        "\"RadiusOuter\":1024,\"RadiusInner\":0}}}";
    testOk(ADTimePix3ServalMeasurement::parseConfigReadback(
               200, completeReadback, configSnapshot) == ConfigReadbackError::None &&
               configSnapshot.hasStemScanWidth && configSnapshot.stemScanWidth == 1024 &&
               configSnapshot.stemScanHeight == 1024 &&
               configSnapshot.stemDwellTime == 1e-6 &&
               configSnapshot.stemRadiusOuter == 1024 &&
               configSnapshot.stemRadiusInner == 0 &&
               configSnapshot.tofTdcReference == "PN0123,PN0123" &&
               configSnapshot.tofMin == 0.0 && configSnapshot.tofMax == 1e99,
           "measurement-config readback parser extracts one complete atomic snapshot");
    testOk(ADTimePix3ServalMeasurement::parseConfigReadback(
               200, "{\"Stem\":null,\"TimeOfFlight\":{\"Min\":null}}",
               configSnapshot) == ConfigReadbackError::None &&
               !configSnapshot.hasStemScanWidth && !configSnapshot.hasTofMin,
           "measurement-config readback parser treats null optional values as unavailable");
    testOk(ADTimePix3ServalMeasurement::parseConfigReadback(
               503, "unavailable", configSnapshot) == ConfigReadbackError::HttpFailure,
           "measurement-config readback parser rejects a failed GET");
    testOk(ADTimePix3ServalMeasurement::parseConfigReadback(
               200, "{bad", configSnapshot) == ConfigReadbackError::MalformedJson,
           "measurement-config readback parser rejects malformed JSON without throwing");
    testOk(ADTimePix3ServalMeasurement::parseConfigReadback(
               200, "{\"Stem\":[]}", configSnapshot) ==
               ConfigReadbackError::InvalidStem,
           "measurement-config readback parser rejects an invalid Stem container");
    testOk(ADTimePix3ServalMeasurement::parseConfigReadback(
               200, "{\"Stem\":{\"Scan\":[]}}", configSnapshot) ==
               ConfigReadbackError::InvalidScan,
           "measurement-config readback parser rejects an invalid Scan container");
    testOk(ADTimePix3ServalMeasurement::parseConfigReadback(
               200, "{\"Stem\":{\"VirtualDetector\":false}}", configSnapshot) ==
               ConfigReadbackError::InvalidVirtualDetector,
           "measurement-config readback parser rejects an invalid virtual-detector container");
    testOk(ADTimePix3ServalMeasurement::parseConfigReadback(
               200, "{\"TimeOfFlight\":[]}", configSnapshot) ==
               ConfigReadbackError::InvalidTimeOfFlight,
           "measurement-config readback parser rejects an invalid time-of-flight container");
    testOk(ADTimePix3ServalMeasurement::parseConfigReadback(
               200, "{\"Stem\":{\"Scan\":{\"Width\":1.5}}}", configSnapshot) ==
               ConfigReadbackError::InvalidMetric,
           "measurement-config readback parser rejects a non-integral integer field");
    testOk(ADTimePix3ServalMeasurement::parseConfigReadback(
               200, "{\"Stem\":{\"Scan\":{\"Width\":18446744073709551615}}}",
               configSnapshot) == ConfigReadbackError::InvalidMetric,
           "measurement-config readback parser rejects integers beyond the EPICS range");
    testOk(ADTimePix3ServalMeasurement::parseConfigReadback(
               200, "{\"TimeOfFlight\":{\"TdcReference\":{}}}", configSnapshot) ==
               ConfigReadbackError::InvalidTdcReference,
           "measurement-config readback parser rejects a non-array TDC reference");
    testOk(ADTimePix3ServalMeasurement::parseConfigReadback(
               200, "{\"Stem\":{\"Scan\":{\"Width\":1024}},"
                    "\"TimeOfFlight\":{\"TdcReference\":[\"PN0123\",4]}}",
               configSnapshot) == ConfigReadbackError::InvalidTdcReference &&
               !configSnapshot.hasStemScanWidth,
           "measurement-config readback parser rejects invalid TDC entries without a partial snapshot");
}

void testDetectorConfigBooleanSerialization()
{
    using ADTimePix3ServalConfig::ParseError;

    nlohmann::json parsed;
    testOk(ADTimePix3ServalConfig::parseResponse(
               "{\"BiasEnabled\":false,\"TriggerMode\":\"CONTINUOUS\"}", parsed) ==
               ParseError::None && parsed["BiasEnabled"].is_boolean(),
           "detector-config parser accepts a JSON object response");
    testOk(ADTimePix3ServalConfig::parseResponse("", parsed) == ParseError::EmptyBody &&
               parsed.is_object() && parsed.empty(),
           "detector-config parser rejects an empty HTTP-200 body");
    testOk(ADTimePix3ServalConfig::parseResponse("{malformed", parsed) ==
               ParseError::MalformedJson && parsed.is_object() && parsed.empty(),
           "detector-config parser rejects malformed JSON without throwing");
    testOk(ADTimePix3ServalConfig::parseResponse("[]", parsed) == ParseError::InvalidRoot &&
               parsed.is_object() && parsed.empty(),
           "detector-config parser rejects a non-object JSON root");
    testOk(ADTimePix3ServalConfig::putAccepted(200),
           "detector-config response accepts HTTP 200");
    testOk(!ADTimePix3ServalConfig::putAccepted(400) &&
               !ADTimePix3ServalConfig::putAccepted(500) &&
               !ADTimePix3ServalConfig::putAccepted(0),
           "detector-config response rejects client, server, and transport failures");

    nlohmann::json disabled = nlohmann::json::object();
    testOk(ADTimePix3ServalConfig::setBiasEnabled(disabled, 0),
           "production config builder accepts BiasEnabled=0");
    testOk(disabled.dump() == "{\"BiasEnabled\":false}",
           "production config builder serializes BiasEnabled=0 as JSON false");

    nlohmann::json enabled = nlohmann::json::object();
    testOk(ADTimePix3ServalConfig::setBiasEnabled(enabled, 1),
           "production config builder accepts BiasEnabled=1");
    testOk(enabled.dump() == "{\"BiasEnabled\":true}",
           "production config builder serializes BiasEnabled=1 as JSON true");

    nlohmann::json invalid = {{"BiasEnabled", false}};
    testOk(!ADTimePix3ServalConfig::setBiasEnabled(invalid, 2),
           "production config builder rejects an invalid BiasEnabled value");
    testOk(invalid["BiasEnabled"].is_boolean() && !invalid["BiasEnabled"].get<bool>(),
           "invalid BiasEnabled input leaves the existing configuration unchanged");

    nlohmann::json clocks = nlohmann::json::object();
    testOk(ADTimePix3ServalConfig::setBoolean(clocks, "ExternalReferenceClock", 1) &&
               clocks["ExternalReferenceClock"].is_boolean() &&
               clocks["ExternalReferenceClock"].get<bool>(),
           "production config builder serializes ExternalReferenceClock=1 as JSON true");
    testOk(ADTimePix3ServalConfig::setBoolean(clocks, "PeriphClk80", 0) &&
               clocks["PeriphClk80"].is_boolean() && !clocks["PeriphClk80"].get<bool>(),
           "production config builder serializes PeriphClk80=0 as JSON false");
    testOk(!ADTimePix3ServalConfig::setBoolean(clocks, "PeriphClk80", -1) &&
               clocks["PeriphClk80"].is_boolean() && !clocks["PeriphClk80"].get<bool>(),
           "invalid clock boolean input leaves the existing configuration unchanged");

    nlohmann::json enums = {
        {"ChainMode", "NONE"}, {"Polarity", "Positive"}, {"Tdc", "malformed"}};
    testOk(ADTimePix3ServalConfig::setChainMode(enums, 2) &&
               enums["ChainMode"] == "FOLLOWER",
           "production config builder maps the highest valid ChainMode value");
    testOk(!ADTimePix3ServalConfig::setChainMode(enums, 3) &&
               enums["ChainMode"] == "FOLLOWER",
           "production config builder rejects an out-of-range ChainMode value");
    testOk(ADTimePix3ServalConfig::setPolarity(enums, 1) &&
               enums["Polarity"] == "Negative",
           "production config builder maps the highest valid Polarity value");
    testOk(!ADTimePix3ServalConfig::setPolarity(enums, -1) &&
               enums["Polarity"] == "Negative",
           "production config builder rejects a negative Polarity value");
    testOk(ADTimePix3ServalConfig::setTdc(enums, 3, 5) &&
               enums["Tdc"] == nlohmann::json::array({"P0", "PN0"}),
           "production config builder replaces malformed Tdc metadata with two strings");
    const nlohmann::json validTdc = enums["Tdc"];
    testOk(!ADTimePix3ServalConfig::setTdc(enums, 6, 0) && enums["Tdc"] == validTdc,
           "production config builder rejects an out-of-range first Tdc value");
    testOk(!ADTimePix3ServalConfig::setTdc(enums, 0, -1) && enums["Tdc"] == validTdc,
           "production config builder rejects a negative second Tdc value");

    bool allTdcValuesRoundTrip = true;
    for (int index = 0; index < 6; ++index) {
        nlohmann::json roundTrip;
        int first = -1;
        int second = -1;
        allTdcValuesRoundTrip = allTdcValuesRoundTrip &&
            ADTimePix3ServalConfig::setTdc(roundTrip, index, 5 - index) &&
            ADTimePix3ServalConfig::parseTdc(roundTrip["Tdc"], first, second) &&
            first == index && second == 5 - index;
    }
    testOk(allTdcValuesRoundTrip,
           "production Tdc readback round-trips every supported selector value");
    testOk(ADTimePix3ServalConfig::formatTdc(
               nlohmann::json::array({"P0", "PN0"})) == "[\"P0\",\"PN0\"]",
           "aggregate Tdc readback preserves the complete JSON array");

    int first = 3;
    int second = 5;
    testOk(!ADTimePix3ServalConfig::parseTdc("P0", first, second) &&
               first == 3 && second == 5,
           "Tdc readback rejects a non-array without changing prior values");
    testOk(!ADTimePix3ServalConfig::parseTdc(nlohmann::json::array({"P0"}), first, second) &&
               first == 3 && second == 5,
           "Tdc readback rejects an incomplete array without changing prior values");
    testOk(!ADTimePix3ServalConfig::parseTdc(
               nlohmann::json::array({"unsupported", "PN0"}), first, second) &&
               first == 3 && second == 5,
           "Tdc readback rejects an unknown selector without changing prior values");
    testOk(!ADTimePix3ServalConfig::parseTdc(
               nlohmann::json::array({"P0", 5}), first, second) &&
               first == 3 && second == 5,
           "Tdc readback rejects a non-string selector without changing prior values");

    FakeHttpResponse response;
    FakeServalHttpServer server(response);
    const int client = connectLoopback(server.port());
    const std::string body = enabled.dump();
    const std::string request =
        "PUT /detector/config HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Type: application/json\r\n" +
        std::string("Content-Length: ") + std::to_string(body.size()) + "\r\n\r\n" + body;
    testOk(client >= 0 && sendAll(client, request),
           "production-built BiasEnabled request reaches the fake HTTP peer");
    testOk(server.waitForRequest(kFixtureDeadline) && server.request().body == body,
           "fake HTTP peer records the exact boolean request body");
    close(client);
}

void testDetectorResponseValidation()
{
    using ADTimePix3ServalDetector::ParseError;
    using ADTimePix3ServalDetector::Snapshot;

    const std::string valid =
        "{\"Info\":{\"PixCount\":262144,\"RowLen\":512,\"NumberOfChips\":4,"
        "\"NumberOfRows\":512,\"MpxType\":3},\"Config\":{\"BiasEnabled\":false},"
        "\"Health\":[]}";
    Snapshot snapshot;
    testOk(ADTimePix3ServalDetector::parseResponse(valid, snapshot) == ParseError::None &&
               snapshot.pixelCount == 262144 && snapshot.rowLength == 512 &&
               snapshot.numberOfChips == 4 && snapshot.numberOfRows == 512 &&
               snapshot.mpxType == 3 && snapshot.response["Config"]["BiasEnabled"] == false,
           "detector parser accepts a complete response and extracts bounded geometry");
    testOk(ADTimePix3ServalDetector::parseResponse("", snapshot) == ParseError::EmptyBody &&
               snapshot.response.is_object() && snapshot.response.empty(),
           "detector parser rejects an empty HTTP-200 body");
    testOk(ADTimePix3ServalDetector::parseResponse("{malformed", snapshot) ==
               ParseError::MalformedJson && snapshot.response.is_object() &&
               snapshot.response.empty(),
           "detector parser rejects malformed JSON without throwing");
    testOk(ADTimePix3ServalDetector::parseResponse("[]", snapshot) ==
               ParseError::InvalidRoot,
           "detector parser rejects a non-object JSON root");
    testOk(ADTimePix3ServalDetector::parseResponse("{\"Config\":{}}", snapshot) ==
               ParseError::MissingInfo,
           "detector parser requires the Info object");
    testOk(ADTimePix3ServalDetector::parseResponse(
               "{\"Info\":{\"PixCount\":1,\"RowLen\":1,\"NumberOfChips\":1,"
               "\"NumberOfRows\":1,\"MpxType\":0}}", snapshot) ==
               ParseError::MissingConfig,
           "detector parser requires the Config object");
    testOk(ADTimePix3ServalDetector::parseResponse(
               "{\"Info\":{\"PixCount\":262144,\"RowLen\":512,\"NumberOfChips\":4,"
               "\"NumberOfRows\":0,\"MpxType\":3},\"Config\":{}}", snapshot) ==
               ParseError::InvalidGeometry,
           "detector parser rejects zero rows before geometry division");
    testOk(ADTimePix3ServalDetector::parseResponse(
               "{\"Info\":{\"PixCount\":10,\"RowLen\":5,\"NumberOfChips\":1,"
               "\"NumberOfRows\":3,\"MpxType\":0},\"Config\":{}}", snapshot) ==
               ParseError::InvalidGeometry,
           "detector parser rejects a non-integral rectangular geometry");
    testOk(ADTimePix3ServalDetector::parseResponse(
               "{\"Info\":{\"PixCount\":\"262144\",\"RowLen\":512,"
               "\"NumberOfChips\":4,\"NumberOfRows\":512,\"MpxType\":-1},"
               "\"Config\":{}}", snapshot) == ParseError::InvalidGeometry,
           "detector parser rejects wrong field types and negative detector families");
    testOk(ADTimePix3ServalDetector::parseResponse(
               "{\"Info\":{\"PixCount\":18446744073709551615,\"RowLen\":512,"
               "\"NumberOfChips\":4,\"NumberOfRows\":512,\"MpxType\":3},"
               "\"Config\":{}}", snapshot) == ParseError::InvalidGeometry,
           "detector parser rejects unsigned geometry beyond the EPICS integer range");
}

void testDacUpdateValidation()
{
    using ADTimePix3ServalDacs::UpdateError;

    nlohmann::json updated;
    int current = -1;
    const std::string complete =
        "{\"Vthreshold_coarse\":7,\"Vthreshold_fine\":120,\"Ibias_Preamp_ON\":128}";
    testOk(ADTimePix3ServalDacs::prepareUpdate(
               complete, "Vthreshold_fine", 121, updated, current) == UpdateError::None &&
               current == 120 && updated["Vthreshold_fine"] == 121 &&
               updated["Vthreshold_coarse"] == 7 && updated["Ibias_Preamp_ON"] == 128,
           "DAC update preserves the complete atomic object and returns the accepted value");
    testOk(ADTimePix3ServalDacs::prepareUpdate(
               "{\"Vthreshold_fine\":-1}", "Vthreshold_fine", 0, updated, current) ==
               UpdateError::None && current == -1 && updated["Vthreshold_fine"] == 0,
           "DAC validation leaves value-range policy to Serval");
    testOk(ADTimePix3ServalDacs::prepareUpdate(
               "", "Vthreshold_fine", 1, updated, current) == UpdateError::EmptyBody &&
               updated.is_object() && updated.empty(),
           "DAC update rejects an empty HTTP-200 body");
    testOk(ADTimePix3ServalDacs::prepareUpdate(
               "{malformed", "Vthreshold_fine", 1, updated, current) ==
               UpdateError::MalformedJson && updated.is_object() && updated.empty(),
           "DAC update rejects malformed JSON without throwing");
    testOk(ADTimePix3ServalDacs::prepareUpdate(
               "[]", "Vthreshold_fine", 1, updated, current) == UpdateError::InvalidRoot,
           "DAC update rejects a non-object JSON root");
    testOk(ADTimePix3ServalDacs::prepareUpdate(
               "{\"Vthreshold_coarse\":7}", "Vthreshold_fine", 1, updated, current) ==
               UpdateError::MissingDac,
           "DAC update rejects a response missing the requested field");
    testOk(ADTimePix3ServalDacs::prepareUpdate(
               "{\"Vthreshold_fine\":\"120\"}", "Vthreshold_fine", 1,
               updated, current) == UpdateError::InvalidDacValue,
           "DAC update rejects a non-integer accepted value");
    testOk(ADTimePix3ServalDacs::prepareUpdate(
               "{\"Vthreshold_fine\":18446744073709551615}", "Vthreshold_fine", 1,
               updated, current) == UpdateError::InvalidDacValue,
           "DAC update rejects accepted values beyond the EPICS integer range");
    testOk(ADTimePix3ServalDacs::putAccepted(200),
           "DAC PUT accepts the documented HTTP 200 response");
    testOk(!ADTimePix3ServalDacs::putAccepted(400) &&
               !ADTimePix3ServalDacs::putAccepted(500) &&
               !ADTimePix3ServalDacs::putAccepted(0),
           "DAC PUT rejects client, server, and transport failures");
}

void testDestinationResponseValidation()
{
    using ADTimePix3ServalDestination::ResponseError;
    using ADTimePix3ServalDestination::Snapshot;

    Snapshot snapshot;
    const std::string direct =
        "{\"Raw\":[{},{}],\"Image\":[{}],\"Preview\":{"
        "\"ImageChannels\":[{},{}],\"HistogramChannels\":[{}]}}";
    testOk(ADTimePix3ServalDestination::parseResponse(200, direct, snapshot) ==
               ResponseError::None && snapshot.rawChannels == 2 &&
               snapshot.imageChannels == 1 && snapshot.previewImageChannels == 2 &&
               snapshot.previewHistogramChannels == 1,
           "destination parser accepts the direct Serval response shape");
    testOk(ADTimePix3ServalDestination::parseResponse(
               200, "{\"Destination\":{\"Image\":[{},{}]}}", snapshot) ==
               ResponseError::None && snapshot.rawChannels == 0 &&
               snapshot.imageChannels == 2 && snapshot.previewImageChannels == 0,
           "destination parser accepts the wrapped Serval response shape");
    testOk(ADTimePix3ServalDestination::parseResponse(200, "{}", snapshot) ==
               ResponseError::None && snapshot.rawChannels == 0 &&
               snapshot.imageChannels == 0 && snapshot.previewImageChannels == 0 &&
               snapshot.previewHistogramChannels == 0,
           "destination parser accepts an empty configured destination");
    testOk(ADTimePix3ServalDestination::parseResponse(
               400, "Destination is not set.", snapshot) == ResponseError::NotConfigured,
           "destination parser classifies Serval's expected unconfigured response");
    testOk(ADTimePix3ServalDestination::parseResponse(
               503, "Service unavailable", snapshot) == ResponseError::HttpFailure,
           "destination parser rejects other non-200 responses");
    testOk(ADTimePix3ServalDestination::parseResponse(200, "", snapshot) ==
               ResponseError::EmptyBody,
           "destination parser rejects an empty HTTP-200 body");
    testOk(ADTimePix3ServalDestination::parseResponse(200, "{malformed", snapshot) ==
               ResponseError::MalformedJson,
           "destination parser rejects malformed JSON without throwing");
    testOk(ADTimePix3ServalDestination::parseResponse(200, "[]", snapshot) ==
               ResponseError::InvalidRoot,
           "destination parser rejects a non-object JSON root");
    testOk(ADTimePix3ServalDestination::parseResponse(
               200, "{\"Destination\":[]}", snapshot) ==
               ResponseError::InvalidDestination,
           "destination parser rejects a non-object Destination wrapper");
    testOk(ADTimePix3ServalDestination::parseResponse(
               200, "{\"Raw\":{}}", snapshot) == ResponseError::InvalidRawChannels,
           "destination parser rejects a non-array Raw field");
    testOk(ADTimePix3ServalDestination::parseResponse(
               200, "{\"Image\":null}", snapshot) ==
               ResponseError::InvalidImageChannels,
           "destination parser rejects a non-array Image field");
    testOk(ADTimePix3ServalDestination::parseResponse(
               200, "{\"Preview\":[]}", snapshot) == ResponseError::InvalidPreview,
           "destination parser rejects a non-object Preview field");
    testOk(ADTimePix3ServalDestination::parseResponse(
               200, "{\"Preview\":{\"ImageChannels\":{}}}", snapshot) ==
               ResponseError::InvalidPreviewImageChannels,
           "destination parser rejects non-array preview image channels");
    testOk(ADTimePix3ServalDestination::parseResponse(
               200, "{\"Preview\":{\"HistogramChannels\":null}}", snapshot) ==
               ResponseError::InvalidPreviewHistogramChannels,
           "destination parser rejects non-array preview histogram channels");
}

void testDashboardResponseValidation()
{
    using ADTimePix3ServalDashboard::ResponseError;
    using ADTimePix3ServalDashboard::Snapshot;

    Snapshot snapshot;
    const std::string connected =
        "{\"Server\":{\"SoftwareVersion\":\"4.1.6\","
        "\"SoftwareTimestamp\":\"2026/06/16 10:00\",\"DiskSpace\":[{"
        "\"FreeSpace\":1234,\"WriteSpeed\":4.5,\"LowerLimit\":100,"
        "\"DiskLimitReached\":false}]},\"Measurement\":{\"Status\":\"DA_IDLE\"},"
        "\"Detector\":{\"DetectorType\":\"TPX3\"}}";
    testOk(ADTimePix3ServalDashboard::parseResponse(200, connected, snapshot) ==
               ResponseError::None && snapshot.detectorConnected &&
               snapshot.detectorType == "TPX3" &&
               snapshot.response["Server"]["DiskSpace"].size() == 1,
           "dashboard parser accepts a connected detector and complete server metadata");
    testOk(ADTimePix3ServalDashboard::parseResponse(
               200, "{\"Server\":{},\"Measurement\":null,\"Detector\":null}", snapshot) ==
               ResponseError::None && !snapshot.detectorConnected && snapshot.detectorType.empty(),
           "dashboard parser accepts the documented disconnected state");
    testOk(ADTimePix3ServalDashboard::parseResponse(503, "unavailable", snapshot) ==
               ResponseError::HttpFailure,
           "dashboard parser rejects non-200 responses");
    testOk(ADTimePix3ServalDashboard::parseResponse(200, "", snapshot) ==
               ResponseError::EmptyBody,
           "dashboard parser rejects an empty HTTP-200 body");
    testOk(ADTimePix3ServalDashboard::parseResponse(200, "{bad", snapshot) ==
               ResponseError::MalformedJson,
           "dashboard parser rejects malformed JSON without throwing");
    testOk(ADTimePix3ServalDashboard::parseResponse(200, "[]", snapshot) ==
               ResponseError::InvalidRoot,
           "dashboard parser rejects a non-object root");
    testOk(ADTimePix3ServalDashboard::parseResponse(200, "{\"Detector\":null}", snapshot) ==
               ResponseError::MissingServer,
           "dashboard parser requires the Server object");
    testOk(ADTimePix3ServalDashboard::parseResponse(
               200, "{\"Server\":null,\"Detector\":null}", snapshot) ==
               ResponseError::InvalidServer,
           "dashboard parser rejects a non-object Server field");
    testOk(ADTimePix3ServalDashboard::parseResponse(200, "{\"Server\":{}}", snapshot) ==
               ResponseError::MissingDetector,
           "dashboard parser requires the Detector field");
    testOk(ADTimePix3ServalDashboard::parseResponse(
               200, "{\"Server\":{},\"Detector\":[]}", snapshot) ==
               ResponseError::InvalidDetector,
           "dashboard parser rejects an invalid Detector container");
    testOk(ADTimePix3ServalDashboard::parseResponse(
               200, "{\"Server\":{},\"Detector\":{}}", snapshot) ==
               ResponseError::InvalidDetectorType,
           "dashboard parser requires a detector type for a connected detector");
    testOk(ADTimePix3ServalDashboard::parseResponse(
               200, "{\"Server\":{},\"Detector\":null,\"Measurement\":[]}", snapshot) ==
               ResponseError::InvalidMeasurement,
           "dashboard parser rejects an invalid Measurement container");
    testOk(ADTimePix3ServalDashboard::parseResponse(
               200, "{\"Server\":{\"SoftwareVersion\":4},\"Detector\":null}", snapshot) ==
               ResponseError::InvalidSoftwareVersion,
           "dashboard parser rejects a non-string software version");
    testOk(ADTimePix3ServalDashboard::parseResponse(
               200, "{\"Server\":{\"DiskSpace\":{}},\"Detector\":null}", snapshot) ==
               ResponseError::InvalidDiskSpace,
           "dashboard parser rejects a non-array DiskSpace field");
    testOk(ADTimePix3ServalDashboard::parseResponse(
               200, "{\"Server\":{\"DiskSpace\":[null]},\"Detector\":null}", snapshot) ==
               ResponseError::InvalidDiskEntry,
           "dashboard parser rejects a non-object DiskSpace entry");
    testOk(ADTimePix3ServalDashboard::parseResponse(
               200, "{\"Server\":{\"DiskSpace\":[{\"WriteSpeed\":\"fast\"}]},"
               "\"Detector\":null}", snapshot) == ResponseError::InvalidDiskValue,
           "dashboard parser rejects invalid disk metric types");
    testOk(ADTimePix3ServalDashboard::parseResponse(
               200, "{\"Server\":{\"DiskSpace\":[{"
               "\"FreeSpace\":18446744073709551615}]},\"Detector\":null}", snapshot) ==
               ResponseError::InvalidDiskValue,
           "dashboard parser rejects disk sizes beyond the EPICS integer range");
    testOk(ADTimePix3ServalDashboard::parseResponse(
               200, "{\"Server\":{\"DiskSpace\":[{"
               "\"DiskLimitReached\":18446744073709551615}]},\"Detector\":null}", snapshot) ==
               ResponseError::InvalidDiskValue,
           "dashboard parser rejects disk-limit values beyond the EPICS integer range");
}

void testHealthResponseValidation()
{
    using ADTimePix3ServalHealth::ResponseError;
    using ADTimePix3ServalHealth::Snapshot;

    Snapshot snapshot;
    const std::string complete =
        "[{\"LocalTemperature\":42.5,\"FPGATemperature\":48,"
        "\"Fan1Speed\":1200,\"Fan2Speed\":1195,\"BiasVoltage\":100,"
        "\"Humidity\":80,"
        "\"ChipTemperatures\":[41,42,43,44],\"VDD\":[1.2,1.2,1.2,1.2],"
        "\"AVDD\":[1.8,1.8,1.8,1.8]}]";
    testOk(ADTimePix3ServalHealth::parseResponse(200, complete, snapshot) ==
               ResponseError::None && snapshot.hasLocalTemperature &&
               snapshot.localTemperature == 42.5 && snapshot.hasFpgaTemperature &&
               snapshot.fpgaTemperature == 48.0 && snapshot.hasFan1Speed &&
               snapshot.fan1Speed == 1200.0 && snapshot.hasFan2Speed &&
               snapshot.fan2Speed == 1195.0 && snapshot.hasBiasVoltage &&
               snapshot.biasVoltage == 100.0 && snapshot.hasHumidity &&
               snapshot.humidity == 80 && snapshot.hasChipTemperatures &&
               snapshot.chipTemperatures == "[41,42,43,44]" && snapshot.hasVdd &&
               snapshot.vdd == "[1.2,1.2,1.2,1.2]" && snapshot.hasAvdd &&
               snapshot.avdd == "[1.8,1.8,1.8,1.8]",
           "health parser accepts one complete validated snapshot");
    testOk(ADTimePix3ServalHealth::parseResponse(
               200, "{\"LocalTemperature\":null,\"BiasVoltage\":99}", snapshot) ==
               ResponseError::None && !snapshot.hasLocalTemperature &&
               snapshot.hasBiasVoltage && snapshot.biasVoltage == 99.0,
           "health parser treats null or absent sensors as unavailable");
    testOk(ADTimePix3ServalHealth::parseResponse(
               200, "[{\"ChipTemperatures\":[41,42],\"VDD\":[1,2,3]},"
                    "{\"ChipTemperatures\":[43,44],\"VDD\":[4,5,6]}]", snapshot) ==
               ResponseError::None && snapshot.chipTemperatures == "[41,42,43,44]" &&
               snapshot.vdd == "[[1,2,3],[4,5,6]]",
           "health parser aggregates a multi-block detector response");
    testOk(ADTimePix3ServalHealth::parseResponse(503, "unavailable", snapshot) ==
               ResponseError::HttpFailure,
           "health parser rejects non-200 responses");
    testOk(ADTimePix3ServalHealth::parseResponse(200, "", snapshot) ==
               ResponseError::EmptyBody,
           "health parser rejects an empty HTTP-200 body");
    testOk(ADTimePix3ServalHealth::parseResponse(200, "{bad", snapshot) ==
               ResponseError::MalformedJson,
           "health parser rejects malformed JSON without throwing");
    testOk(ADTimePix3ServalHealth::parseResponse(200, "42", snapshot) ==
               ResponseError::InvalidRoot,
           "health parser rejects a root that is neither an object nor an array");
    testOk(ADTimePix3ServalHealth::parseResponse(200, "[]", snapshot) ==
               ResponseError::EmptyHealthArray,
           "health parser rejects an empty health array");
    testOk(ADTimePix3ServalHealth::parseResponse(200, "[null]", snapshot) ==
               ResponseError::InvalidHealthEntry,
           "health parser rejects a non-object health array entry");
    testOk(ADTimePix3ServalHealth::parseResponse(200, "{}", snapshot) ==
               ResponseError::MissingHealthFields,
           "health parser requires at least one recognized health field");
    testOk(ADTimePix3ServalHealth::parseResponse(
               200, "{\"LocalTemperature\":\"hot\"}", snapshot) ==
               ResponseError::InvalidMetric && !snapshot.hasLocalTemperature,
           "health parser rejects an incorrectly typed metric without a snapshot");
    testOk(ADTimePix3ServalHealth::parseResponse(
               200, "{\"ChipTemperatures\":{}}", snapshot) ==
               ResponseError::InvalidArray,
           "health parser rejects a non-array sensor collection");
    testOk(ADTimePix3ServalHealth::parseResponse(
               200, "{\"LocalTemperature\":42,\"VDD\":[1.2,\"bad\"]}", snapshot) ==
               ResponseError::InvalidArray && !snapshot.hasLocalTemperature,
           "health parser rejects an invalid array without returning a partial snapshot");
}

void testPixelConfigResponseValidation()
{
    using ADTimePix3ServalPixelConfig::ResponseError;

    std::vector<std::uint8_t> decoded;
    testOk(ADTimePix3ServalPixelConfig::parseResponse(
               200, "\"AQID\"", 3, decoded) == ResponseError::None &&
               decoded == std::vector<std::uint8_t>({1, 2, 3}),
           "PixelConfig parser accepts an exact validated base64 payload");
    testOk(ADTimePix3ServalPixelConfig::parseResponse(
               200, "\"AQID\\n\"", 3, decoded) == ResponseError::None &&
               decoded == std::vector<std::uint8_t>({1, 2, 3}),
           "PixelConfig parser accepts base64 whitespace");
    testOk(ADTimePix3ServalPixelConfig::parseResponse(
               500, "failure", 3, decoded) == ResponseError::HttpFailure && decoded.empty(),
           "PixelConfig parser rejects a failed HTTP response");
    testOk(ADTimePix3ServalPixelConfig::parseResponse(
               200, "", 3, decoded) == ResponseError::EmptyBody && decoded.empty(),
           "PixelConfig parser rejects an empty response body");
    testOk(ADTimePix3ServalPixelConfig::parseResponse(
               200, "{bad", 3, decoded) == ResponseError::MalformedJson && decoded.empty(),
           "PixelConfig parser rejects malformed JSON without throwing");
    testOk(ADTimePix3ServalPixelConfig::parseResponse(
               200, "{}", 3, decoded) == ResponseError::InvalidRoot && decoded.empty(),
           "PixelConfig parser requires a JSON string root");
    testOk(ADTimePix3ServalPixelConfig::parseResponse(
               200, "\"AQ?D\"", 3, decoded) == ResponseError::InvalidBase64 && decoded.empty(),
           "PixelConfig parser rejects an invalid base64 alphabet");
    testOk(ADTimePix3ServalPixelConfig::parseResponse(
               200, "\"AQ=Z\"", 3, decoded) == ResponseError::InvalidBase64 && decoded.empty(),
           "PixelConfig parser rejects misplaced base64 padding");
    testOk(ADTimePix3ServalPixelConfig::parseResponse(
               200, "\"AQID\"", 4, decoded) == ResponseError::LengthMismatch && decoded.empty(),
           "PixelConfig parser rejects a family-specific length mismatch atomically");
    testOk(ADTimePix3ServalPixelConfig::parseResponse(
               200, "\"AQIDBA==\"", 3, decoded) == ResponseError::LengthMismatch && decoded.empty(),
           "PixelConfig parser rejects an oversized payload before decoding it");

    testOk(ADTimePix3ServalPixelConfig::bytesPerChip(256U * 256U, 1, 1) == 65536U,
           "PixelConfig geometry selects a 64 KiB TPX3 chip block");
    testOk(ADTimePix3ServalPixelConfig::bytesPerChip(256U * 256U, 2, 1) == 131072U,
           "PixelConfig geometry selects a 128 KiB MPX3 packed-word chip block");
    const DetectorCapabilities mpx3Capabilities =
        capabilitiesForFamily(DetectorFamily::MPX3);
    testOk(mpx3Capabilities.bpcBytesPerPel == 2 &&
               mpx3Capabilities.bpcThresholdSlices == 1,
           "MPX3 capabilities select one packed 16-bit word per pixel");
    testOk(ADTimePix3ServalPixelConfig::bytesPerChip(
               std::numeric_limits<std::size_t>::max(), 2, 2) == 0,
           "PixelConfig geometry rejects size overflow");

    std::size_t physicalIndex = 0;
    testOk(ADTimePix3ServalPixelConfig::selectedSliceIndex(
               65536U + 5U, 65536U, 2, 1, 0, physicalIndex) &&
               physicalIndex == 131082U,
           "PixelConfig indexing selects an MPX3 word within the correct chip block");
    testOk(!ADTimePix3ServalPixelConfig::selectedSliceIndex(
               0, 65536U, 2, 1, 1, physicalIndex) && physicalIndex == 0,
           "PixelConfig indexing rejects a nonexistent MPX3 threshold slice");

    std::uint32_t packedDifference = 99;
    const std::vector<std::uint8_t> packedOriginal = {0x04, 0xdc, 0x01, 0x8a};
    const std::vector<std::uint8_t> packedMasked = {0x04, 0xdd, 0x01, 0x8b};
    testOk(ADTimePix3ServalPixelConfig::absolutePackedDifference(
               packedOriginal, packedMasked, 0, 2, packedDifference) &&
               packedDifference == 1,
           "PixelConfig diff decodes a big-endian MPX3 mask-bit change");
    testOk(ADTimePix3ServalPixelConfig::absolutePackedDifference(
               packedOriginal, packedMasked, 2, 2, packedDifference) &&
               packedDifference == 1,
           "PixelConfig diff advances by one complete MPX3 word");
    testOk(!ADTimePix3ServalPixelConfig::absolutePackedDifference(
               packedOriginal, packedMasked, 3, 2, packedDifference) &&
               packedDifference == 0,
           "PixelConfig diff rejects a truncated packed value");

    struct LayoutTransformCase {
        const char* orientation;
        int expectedX;
        int expectedY;
    };
    const LayoutTransformCase layoutCases[] = {
        {"RtLBtT", 223, 191},
        {"LtRTtB", 32, 64},
        {"BtTLtR", 64, 223},
        {"TtBRtL", 191, 32},
        {"LtRBtT", 32, 191},
        {"RtLTtB", 223, 64},
        {"TtBLtR", 64, 32},
        {"BtTRtL", 191, 223}
    };
    for (const auto& layoutCase : layoutCases) {
        int imageX = -1;
        int imageY = -1;
        const bool mapped = ADTimePix3ServalPixelConfig::mpx3LayoutCoordinates(
            32, 64, 256, 256, 0, 512,
            layoutCase.orientation, imageX, imageY);
        const std::string description = std::string("MPX3 layout maps orientation ") +
            layoutCase.orientation;
        testOk(mapped && imageX == 256 + layoutCase.expectedX &&
                   imageY == 256 + layoutCase.expectedY,
               "%s", description.c_str());
    }
    int topTileX = -1;
    int topTileY = -1;
    testOk(ADTimePix3ServalPixelConfig::mpx3LayoutCoordinates(
               32, 64, 256, 0, 256, 512, "LtRTtB", topTileX, topTileY) &&
               topTileX == 32 && topTileY == 64,
           "MPX3 layout converts Serval bottom-origin tile Y to top-origin image Y");
    int imageX = -1;
    int imageY = -1;
    testOk(!ADTimePix3ServalPixelConfig::mpx3LayoutCoordinates(
               32, 32, 256, 0, 0, 512, "unknown", imageX, imageY) &&
               imageX == 0 && imageY == 0,
           "MPX3 layout rejects an unknown chip orientation");
}

void testStreamHeaderValidation()
{
    using ADTimePix3Stream::ImageFrameLayout;
    using ADTimePix3Stream::ImageHeaderError;

    const ADTimePix3Stream::ImageFrameLimits limits =
        ADTimePix3Stream::detectorImageFrameLimits(1024, 512, 1024 * 512);
    ImageFrameLayout layout;

    const ADTimePix3Stream::ImageFrameLimits clampedLimits =
        ADTimePix3Stream::detectorImageFrameLimits(100000, 100000, 1000000000);
    testOk(clampedLimits.maxDimension == 2048U &&
               clampedLimits.maxPixels == 8U * 256U * 256U,
           "production limits clamp untrusted detector metadata to supported geometry");

    testOk(ADTimePix3Stream::validateJsonImageHeader(
               nlohmann::json{{"width", 512}, {"height", 512}, {"pixelFormat", "uint16"}},
               limits, layout) == ImageHeaderError::None &&
               layout.pixelCount == 512U * 512U && layout.payloadBytes == 512U * 512U * 2U,
           "production validator accepts a bounded uint16 image layout");
    testOk(ADTimePix3Stream::validateJsonImageHeader(
               nlohmann::json{{"width", 1024}, {"height", 512}, {"pixelFormat", "UINT32"}},
               limits, layout) == ImageHeaderError::None &&
               layout.pixelCount == 1024U * 512U && layout.payloadBytes == 1024U * 512U * 4U,
           "production validator accepts the detector pixel limit without overflow");
    testOk(ADTimePix3Stream::validateJsonImageHeader(
               nlohmann::json{{"width", 512}, {"height", 512},
                              {"bitDepth", 32}, {"dataSize", 512 * 512 * 4}},
               limits, layout) == ImageHeaderError::None &&
               layout.pixelFormat == ADTimePix3Stream::PixelFormat::UInt32 &&
               layout.payloadBytes == 512U * 512U * 4U,
           "production validator uses Serval metadata for an MPX3 integrated uint32 image");
    testOk(ADTimePix3Stream::validateJsonImageHeader(
               nlohmann::json{{"width", 2}, {"height", 1},
                              {"bitDepth", 8}, {"dataSize", 2}},
               limits, layout) == ImageHeaderError::None &&
               layout.pixelFormat == ADTimePix3Stream::PixelFormat::UInt8 &&
               layout.payloadBytes == 2U,
           "production validator accepts Serval byte rasters for lower MPX3 pixel depths");
    testOk(ADTimePix3Stream::validateJsonImageHeader(
               nlohmann::json{{"width", 1}, {"height", 1}}, limits, layout) ==
               ImageHeaderError::None &&
               layout.pixelFormat == ADTimePix3Stream::PixelFormat::UInt16 &&
               layout.payloadBytes == 2U,
           "production validator applies the Serval uint16 default when pixelFormat is absent");
    testOk(ADTimePix3Stream::validateJsonImageHeader(
               nlohmann::json{{"height", 1}, {"pixelFormat", "uint16"}}, limits, layout) ==
               ImageHeaderError::MissingField,
           "production validator requires width and height metadata");
    testOk(ADTimePix3Stream::validateJsonImageHeader(
               nlohmann::json{{"width", "512"}, {"height", 512}, {"pixelFormat", "uint16"}},
               limits, layout) == ImageHeaderError::InvalidFieldType,
           "production validator rejects non-integer dimensions");
    testOk(ADTimePix3Stream::validateJsonImageHeader(
               nlohmann::json{{"width", -1}, {"height", 512}, {"pixelFormat", "uint16"}},
               limits, layout) == ImageHeaderError::InvalidDimension,
           "production validator rejects negative dimensions before arithmetic");
    testOk(ADTimePix3Stream::validateJsonImageHeader(
               nlohmann::json{{"width", 1025}, {"height", 1}, {"pixelFormat", "uint16"}},
               limits, layout) == ImageHeaderError::InvalidDimension,
           "production validator rejects dimensions beyond detector geometry");
    testOk(ADTimePix3Stream::validateJsonImageHeader(
               nlohmann::json{{"width", 1024}, {"height", 1024}, {"pixelFormat", "uint16"}},
               limits, layout) == ImageHeaderError::PixelLimitExceeded,
           "production validator rejects oversized total pixel counts");
    testOk(ADTimePix3Stream::validateJsonImageHeader(
               nlohmann::json{{"width", 512}, {"height", 512}, {"pixelFormat", "float32"}},
               limits, layout) == ImageHeaderError::UnsupportedPixelFormat,
           "production validator rejects unknown pixel formats");
    testOk(ADTimePix3Stream::validateJsonImageHeader(
               nlohmann::json{{"width", 2}, {"height", 1},
                              {"bitDepth", 32}, {"dataSize", 4}},
               limits, layout) == ImageHeaderError::InconsistentPixelMetadata,
           "production validator rejects contradictory Serval bitDepth and dataSize");
    testOk(ADTimePix3Stream::validateJsonImageHeader(
               nlohmann::json{{"width", 2}, {"height", 1},
                              {"bitDepth", 32}, {"dataSize", "8"}},
               limits, layout) == ImageHeaderError::InvalidFieldType,
           "production validator rejects non-integer Serval dataSize metadata");
    const ADTimePix3Stream::ImageFrameLimits byteLimited{1024U, 1024U * 512U, 1024U};
    testOk(ADTimePix3Stream::validateJsonImageHeader(
               nlohmann::json{{"width", 512}, {"height", 512}, {"pixelFormat", "uint16"}},
               byteLimited, layout) == ImageHeaderError::PayloadLimitExceeded,
           "production validator enforces the configured payload-byte budget");

    const std::string mpx3Header =
        "{\"width\":2,\"height\":1,\"bitDepth\":32,\"dataSize\":8}\n";
    const std::string mpx3Payload("\x00\x00\x00\x01\n{}\xff", 8);
    const std::string mpx3Wire = mpx3Header + mpx3Payload + "\n";
    ADTimePix3Stream::FrameBuffer mpx3Frames(256, limits.maxPayloadBytes, 16);
    ADTimePix3Stream::FramedMessage mpx3Frame;
    std::string mpx3Error;
    const auto mpx3Resolver = [&limits](const std::string& header,
                                        std::size_t& payloadBytes,
                                        std::string& error) {
        const nlohmann::json parsed = nlohmann::json::parse(header, nullptr, false);
        ADTimePix3Stream::ImageFrameLayout resolvedLayout;
        const ADTimePix3Stream::ImageHeaderError result =
            ADTimePix3Stream::validateJsonImageHeader(parsed, limits, resolvedLayout);
        if (result != ADTimePix3Stream::ImageHeaderError::None) {
            error = ADTimePix3Stream::imageHeaderErrorMessage(result);
            return false;
        }
        payloadBytes = resolvedLayout.payloadBytes;
        return true;
    };
    mpx3Frames.append(mpx3Wire.data(), mpx3Wire.size());
    testOk(mpx3Frames.next(mpx3Resolver, mpx3Frame, mpx3Error) ==
               ADTimePix3Stream::FrameResult::FrameReady &&
               mpx3Frame.payload.size() == mpx3Payload.size() &&
               std::equal(mpx3Frame.payload.begin(), mpx3Frame.payload.end(),
                          reinterpret_cast<const std::uint8_t*>(mpx3Payload.data())) &&
               mpx3Frames.bufferedBytes() == 0,
           "consume-once framing uses MPX3 dataSize and preserves embedded delimiter bytes");

    const std::string invalidHeader =
        "{\"width\":2147483647,\"height\":2147483647,\"pixelFormat\":\"uint32\"}\n";
    FakeServalTcpServer server({invalidHeader.substr(0, 19), invalidHeader.substr(19)});
    NetworkClient client;
    testOk(client.connect("127.0.0.1", server.port()) &&
               server.waitForClient(kFixtureDeadline),
           "overflow header fixture connects before the deadline");
    server.releaseAllChunks();
    std::vector<char> received(invalidHeader.size());
    const bool receivedHeader = client.receive_exact(received.data(), received.size());
    const nlohmann::json parsed = nlohmann::json::parse(
        std::string(received.begin(), received.end() - 1));
    testOk(receivedHeader && ADTimePix3Stream::validateJsonImageHeader(parsed, limits, layout) ==
               ImageHeaderError::InvalidDimension,
           "fragmented overflow header is rejected without reading a payload");
}

void testBpcFileBounds()
{
    using ADTimePix3BpcFile::Status;

    std::size_t size = 0;
    testOk(ADTimePix3BpcFile::expectedSize(256 * 256, 1, 1, size) &&
               size == 65536U,
           "production BPC sizing accepts one TPX3 chip");
    testOk(ADTimePix3BpcFile::expectedSize(4 * 256 * 256, 1, 1, size) &&
               size == 262144U,
           "production BPC sizing accepts a TPX3 quad");
    testOk(ADTimePix3BpcFile::expectedSize(4 * 256 * 256, 2, 1, size) &&
               size == 524288U,
           "production BPC sizing accepts packed-word MPX3 quad geometry");
    testOk(!ADTimePix3BpcFile::expectedSize(0, 1, 1, size) && size == 0,
           "production BPC sizing rejects missing detector geometry");
    testOk(!ADTimePix3BpcFile::expectedSize(
               std::numeric_limits<int>::max(), std::numeric_limits<int>::max(),
               std::numeric_limits<int>::max(), size) && size == 0,
           "production BPC sizing rejects multiplication overflow");

    TemporaryFile file;
    const std::vector<std::uint8_t> written{0x00, 0x01, 0x1f, 0xff};
    std::vector<std::uint8_t> read;
    testOk(ADTimePix3BpcFile::writeExact(file.path(), written, written.size()) == Status::Ok,
           "bounded BPC writer writes an exact-size file");
    testOk(ADTimePix3BpcFile::readExact(file.path(), written.size(), read) == Status::Ok &&
               read == written,
           "bounded BPC reader round-trips exact binary bytes");

    TemporaryFile missing;
    unlink(missing.path().c_str());
    testOk(ADTimePix3BpcFile::readExact(missing.path(), written.size(), read) ==
               Status::OpenFailed && read.empty(),
           "bounded BPC reader rejects a missing file");

    replaceFileContents(file.path(), 0);
    testOk(ADTimePix3BpcFile::readExact(file.path(), written.size(), read) ==
               Status::SizeMismatch && read.empty(),
           "bounded BPC reader rejects an empty file");
    replaceFileContents(file.path(), written.size() - 1);
    testOk(ADTimePix3BpcFile::readExact(file.path(), written.size(), read) ==
               Status::SizeMismatch && read.empty(),
           "bounded BPC reader rejects a truncated file");
    replaceFileContents(file.path(), written.size() + 1);
    testOk(ADTimePix3BpcFile::readExact(file.path(), written.size(), read) ==
               Status::SizeMismatch && read.empty(),
           "bounded BPC reader rejects an oversized file");
    testOk(ADTimePix3BpcFile::readExact(file.path(), 0, read) ==
               Status::InvalidExpectedSize && read.empty(),
           "bounded BPC reader rejects a zero expected size");
    testOk(ADTimePix3BpcFile::writeExact(file.path(), written, written.size() + 1) ==
               Status::InvalidExpectedSize,
           "bounded BPC writer rejects a buffer-size mismatch");
}

void testBpcMaskSemantics()
{
    testOk(ADTimePix3BpcMask::operatorMaskSupported(DetectorFamily::TPX3) &&
               ADTimePix3BpcMask::operatorMaskSupported(DetectorFamily::MPX3) &&
               !ADTimePix3BpcMask::operatorMaskSupported(DetectorFamily::Unknown),
           "operator mask writes are supported only for documented TPX3 and MPX3 families");
    testOk(ADTimePix3BpcMask::bytesPerPixel(DetectorFamily::TPX3) == 1 &&
               ADTimePix3BpcMask::bytesPerPixel(DetectorFamily::MPX3) == 2 &&
               ADTimePix3BpcMask::bytesPerPixel(DetectorFamily::Unknown) == 0,
           "mask semantics report the family-specific packed pixel width");

    std::vector<std::uint8_t> tpx3{0x1e, 0x01, 0x1f, 0xff};
    testOk(!ADTimePix3BpcMask::isMasked(DetectorFamily::TPX3, tpx3, 0) &&
               ADTimePix3BpcMask::isMasked(DetectorFamily::TPX3, tpx3, 1) &&
               ADTimePix3BpcMask::isMasked(DetectorFamily::TPX3, tpx3, 2) &&
               ADTimePix3BpcMask::isMasked(DetectorFamily::TPX3, tpx3, 3),
           "TPX3 classifies bit 0 as the mask independently of adjustment bits");
    testOk(ADTimePix3BpcMask::countMasked(DetectorFamily::TPX3, tpx3) == 3,
           "TPX3 masked-pixel count tests bit 0 of each byte");
    testOk(ADTimePix3BpcMask::setMasked(DetectorFamily::TPX3, tpx3, 0, true) &&
               tpx3[0] == 0x1f &&
               ADTimePix3BpcMask::setMasked(DetectorFamily::TPX3, tpx3, 3, false) &&
               tpx3[3] == 0xfe,
           "TPX3 set and clear preserve all adjustment, test-pulse, and unknown bits");

    std::vector<std::uint8_t> mpx3{0xa8, 0x54, 0x08, 0x01, 0xf0, 0xfe};
    std::uint16_t packed = 0;
    testOk(ADTimePix3BpcMask::pixelValue(DetectorFamily::MPX3, mpx3, 0, packed) &&
               packed == 0xa854 &&
               !ADTimePix3BpcMask::isMasked(DetectorFamily::MPX3, mpx3, 0) &&
               ADTimePix3BpcMask::isMasked(DetectorFamily::MPX3, mpx3, 2),
           "MPX3 decodes big-endian words and classifies their shared bit-0 mask");
    testOk(ADTimePix3BpcMask::countMasked(DetectorFamily::MPX3, mpx3) == 1,
           "MPX3 masked-pixel count advances by complete 16-bit words");
    testOk(ADTimePix3BpcMask::setMasked(DetectorFamily::MPX3, mpx3, 0, true) &&
               mpx3[0] == 0xa8 && mpx3[1] == 0x55 &&
               ADTimePix3BpcMask::setMasked(DetectorFamily::MPX3, mpx3, 2, false) &&
               mpx3[2] == 0x08 && mpx3[3] == 0x00,
           "MPX3 set and clear change only the mask bit and preserve all other word bits");
    testOk(!ADTimePix3BpcMask::pixelValue(DetectorFamily::MPX3, mpx3, mpx3.size() - 1, packed) &&
               !ADTimePix3BpcMask::setMasked(
                   DetectorFamily::Unknown, mpx3, 0, true),
           "mask helpers reject truncated words and unsupported detector families");
}

void testMaskGeometry()
{
    using ADTimePix3MaskGeometry::Status;

    std::vector<std::int32_t> resetBuffer(10, 9);
    const std::vector<std::int32_t> resetExpected{0, 0, 0, 0, 0, 0, 0, 0, 9, 9};
    testOk(ADTimePix3MaskGeometry::reset(resetBuffer.data(), resetBuffer.size(), 4, 2, 0) ==
               Status::Ok &&
               resetBuffer == resetExpected,
           "rectangular mask reset writes exactly width times height elements and preserves guards");

    std::vector<std::int32_t> rectangleBuffer(8, 0);
    const std::vector<std::int32_t> rectangleExpected{0, 0, 0, 0, 0, 0, 1, 1};
    testOk(ADTimePix3MaskGeometry::rectangle(
               rectangleBuffer.data(), rectangleBuffer.size(), 4, 2,
               2, 2, 1, 1, true) == Status::Ok &&
               rectangleBuffer == rectangleExpected,
           "rectangular mask uses y times width plus x row-major indexing");

    std::fill(rectangleBuffer.begin(), rectangleBuffer.end(), 0);
    const std::vector<std::int32_t> clippedExpected{1, 1, 0, 0, 0, 0, 0, 0};
    testOk(ADTimePix3MaskGeometry::rectangle(
               rectangleBuffer.data(), rectangleBuffer.size(), 4, 2,
               -1, 3, -1, 2, true) == Status::Ok &&
               rectangleBuffer == clippedExpected,
           "rectangular mask clips negative coordinates to the image bounds");

    const std::vector<std::int32_t> beforeEmpty = rectangleBuffer;
    testOk(ADTimePix3MaskGeometry::rectangle(
               rectangleBuffer.data(), rectangleBuffer.size(), 4, 2,
               0, 0, 0, 1, true) == Status::Ok && rectangleBuffer == beforeEmpty,
           "zero-area rectangular mask is a no-op");

    std::vector<std::int32_t> circleBuffer(15, 0);
    const std::vector<std::int32_t> circleExpected{
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 1,
        0, 0, 0, 1, 1};
    testOk(ADTimePix3MaskGeometry::circle(
               circleBuffer.data(), circleBuffer.size(), 5, 3,
               4, 2, 1, true) == Status::Ok && circleBuffer == circleExpected,
           "circular mask clips at rectangular image edges with width-based stride");

    testOk(ADTimePix3MaskGeometry::circle(
               circleBuffer.data(), circleBuffer.size(), 5, 3,
               4, 2, 1, false) == Status::Ok &&
               circleBuffer == std::vector<std::int32_t>(15, 0),
           "circular unmask clears only bit zero");

    rectangleBuffer.assign(8, 0x2a);
    testOk(ADTimePix3MaskGeometry::rectangle(
               rectangleBuffer.data(), rectangleBuffer.size(), 4, 2,
               1, 1, 1, 1, true) == Status::Ok && rectangleBuffer[5] == 0x2b,
           "rectangular mask set preserves every non-mask bit");
    testOk(ADTimePix3MaskGeometry::rectangle(
               rectangleBuffer.data(), rectangleBuffer.size(), 4, 2,
               1, 1, 1, 1, false) == Status::Ok && rectangleBuffer[5] == 0x2a,
           "rectangular mask clear preserves every non-mask bit");

    std::vector<std::int32_t> shortBuffer(7, 3);
    const std::vector<std::int32_t> shortBefore = shortBuffer;
    testOk(ADTimePix3MaskGeometry::reset(
               shortBuffer.data(), shortBuffer.size(), 4, 2, 0) ==
               Status::BufferTooSmall && shortBuffer == shortBefore,
           "mask operations reject a waveform smaller than detector geometry");

    testOk(ADTimePix3MaskGeometry::rectangle(
               rectangleBuffer.data(), rectangleBuffer.size(), 0, 2,
               0, 1, 0, 1, true) == Status::InvalidArgument,
           "mask operations reject invalid detector dimensions");

    testOk(ADTimePix3MaskGeometry::circle(
               nullptr, 8, 4, 2, 0, 0, 1, true) == Status::InvalidArgument,
           "mask operations reject a null waveform buffer");
}

void testOneShotActions()
{
    const ADTimePix3Action::OneShotDecision zero =
        ADTimePix3Action::oneShotDecision(0);
    testOk(!zero.execute && zero.storedValue == 0,
           "one-shot action ignores zero and remains reset");

    const ADTimePix3Action::OneShotDecision one =
        ADTimePix3Action::oneShotDecision(1);
    testOk(one.execute && one.storedValue == 0,
           "one-shot action executes one and resets its stored value");

    const ADTimePix3Action::OneShotDecision invalid =
        ADTimePix3Action::oneShotDecision(2);
    testOk(!invalid.execute && invalid.storedValue == 0,
           "one-shot action ignores unsupported values and resets them");
}

void testNumericRangeContract()
{
    using namespace ADTimePix3Numeric;
    const std::uint64_t u64max = std::numeric_limits<std::uint64_t>::max();
    const std::int64_t i64max = std::numeric_limits<std::int64_t>::max();
    const std::uint32_t u32max = std::numeric_limits<std::uint32_t>::max();
    bool changed = false;

    testOk(saturatingAdd(40, 2, changed) == 42 && !changed,
           "unsigned accumulation preserves an in-range result");
    testOk(saturatingAdd(u64max - 1, 1, changed) == u64max && !changed,
           "unsigned accumulation accepts the exact UINT64 maximum");
    testOk(saturatingAdd(u64max, 1, changed) == u64max && changed,
           "unsigned accumulation saturates and reports overflow");
    testOk(clampToInt64(static_cast<std::uint64_t>(i64max), changed) == i64max && !changed,
           "signed scalar conversion accepts the exact INT64 maximum");
    testOk(clampToInt64(static_cast<std::uint64_t>(i64max) + 1, changed) == i64max && changed,
           "signed scalar conversion clamps values above INT64 maximum");
    testOk(clampAverageToUInt32(99, 0, changed) == 0 && !changed,
           "average conversion defines a zero-divisor result");
    testOk(clampAverageToUInt32(static_cast<std::uint64_t>(u32max) * 2, 2, changed) == u32max && !changed,
           "average conversion accepts the exact UINT32 maximum");
    testOk(clampAverageToUInt32(static_cast<std::uint64_t>(u32max) + 1, 1, changed) == u32max && changed,
           "average conversion clamps values above UINT32 maximum");
    const std::uint64_t u64pattern = UINT64_C(0xfedcba9876543210);
    const std::int64_t i64bits = preserveUInt64Bits(u64pattern);
    std::uint64_t u64roundTrip = 0;
    std::memcpy(&u64roundTrip, &i64bits, sizeof(u64roundTrip));
    testOk(u64roundTrip == u64pattern,
           "64-bit waveform callback preserves unsigned payload bits");
    const std::uint32_t u32pattern = UINT32_C(0xfedcba98);
    const std::int32_t i32bits = preserveUInt32Bits(u32pattern);
    std::uint32_t u32roundTrip = 0;
    std::memcpy(&u32roundTrip, &i32bits, sizeof(u32roundTrip));
    testOk(u32roundTrip == u32pattern,
           "32-bit waveform callback preserves unsigned payload bits");
    testOk(promoteRangeState(RangeState::Ok, RangeState::OutputClamped) ==
               RangeState::OutputClamped,
           "range state latches an output clamp");
    testOk(promoteRangeState(RangeState::OutputClamped,
                             RangeState::AccumulatorSaturated) ==
               RangeState::AccumulatorSaturated,
           "range state promotes a clamp to accumulator saturation");
    testOk(promoteRangeState(RangeState::AccumulatorSaturated,
                             RangeState::OutputClamped) ==
               RangeState::AccumulatorSaturated &&
               promoteRangeState(RangeState::AccumulatorSaturated, RangeState::Ok) ==
               RangeState::Ok,
           "range state cannot downgrade except through explicit reset");
}

void testRollingWindowSum()
{
    using ADTimePix3Accumulation::RollingSumStatus;
    using ADTimePix3Accumulation::RollingWindowSum;

    RollingWindowSum sum;
    const auto invalid = sum.configure(0, 2, 1024);
    testOk(invalid.status == RollingSumStatus::InvalidArgument,
           "rolling sum rejects zero-sized geometry");

    const auto tooSmall = sum.configure(3, 2, 35);
    testOk(tooSmall.status == RollingSumStatus::BudgetTooSmall &&
               tooSmall.requiredBytes == 36,
           "rolling sum reports the bytes required for one retained frame");

    const auto configured = sum.configure(3, 4, 48);
    testOk(configured.status == RollingSumStatus::Ok && configured.limited &&
               configured.effectiveFrames == 2 && sum.effectiveFrames() == 2,
           "rolling sum caps the requested window to its geometry-aware byte budget");

    const std::uint16_t first[] = {1, 2, 3};
    const std::uint32_t second[] = {4, 5, 6};
    testOk(sum.push(first, 3) == RollingSumStatus::Ok &&
               sum.push(second, 3) == RollingSumStatus::Ok &&
               sum.frameCount() == 2 &&
               sum.sum() == std::vector<std::uint64_t>({5, 7, 9}),
           "rolling sum accepts uint16 and uint32 frames with exact fill-window totals");

    const std::uint32_t third[] = {10, 20, 30};
    testOk(sum.push(third, 3) == RollingSumStatus::Ok &&
               sum.frameCount() == 2 &&
               sum.sum() == std::vector<std::uint64_t>({14, 25, 36}),
           "rolling sum evicts the oldest frame in one element-wise update");

    const auto shrunk = sum.configure(3, 1, 48);
    testOk(shrunk.status == RollingSumStatus::Ok && sum.frameCount() == 1 &&
               sum.sum() == std::vector<std::uint64_t>({10, 20, 30}),
           "rolling sum shrinking preserves the newest retained frame and exact sum");

    const auto expanded = sum.configure(3, 2, 48);
    const std::uint16_t fourth[] = {1, 1, 1};
    testOk(expanded.status == RollingSumStatus::Ok &&
               sum.push(fourth, 3) == RollingSumStatus::Ok &&
               sum.sum() == std::vector<std::uint64_t>({11, 21, 31}),
           "rolling sum expanding preserves retained history while refilling the window");

    const std::vector<std::uint64_t> beforeMismatch = sum.sum();
    testOk(sum.push(fourth, 2) == RollingSumStatus::FrameSizeMismatch &&
               sum.sum() == beforeMismatch && sum.frameCount() == 2,
           "rolling sum rejects a mismatched frame without changing state");

    testOk(sum.memoryBytes() <= sum.budgetBytes(),
           "rolling sum retained storage remains within the configured byte budget");

    const auto failedReconfigure = sum.configure(3, 2, 35);
    testOk(failedReconfigure.status == RollingSumStatus::BudgetTooSmall &&
               !sum.configured() && sum.frameCount() == 0 && sum.sum().empty(),
           "rolling sum releases retained storage after failed reconfiguration");

    const auto restored = sum.configure(3, 2, 48);
    testOk(restored.status == RollingSumStatus::Ok,
           "rolling sum can be configured again after a failed reconfiguration");

    sum.reset();
    testOk(sum.configured() && sum.frameCount() == 0 &&
               sum.sum() == std::vector<std::uint64_t>({0, 0, 0}),
           "rolling sum reset clears values while retaining its configuration");

    RollingWindowSum longWindow;
    constexpr std::size_t kElements = 17;
    constexpr std::size_t kWindow = 37;
    const std::size_t longBudget =
        kElements * sizeof(std::uint64_t) +
        kElements * sizeof(std::uint32_t) * kWindow;
    const auto longConfigured =
        longWindow.configure(kElements, kWindow, longBudget);
    std::deque<std::vector<std::uint32_t>> referenceFrames;
    std::vector<std::uint64_t> referenceSum(kElements, 0);
    bool longExact = longConfigured.status == RollingSumStatus::Ok;
    for (std::size_t frameIndex = 0; frameIndex < 257; ++frameIndex) {
        std::vector<std::uint32_t> frame(kElements);
        for (std::size_t element = 0; element < kElements; ++element) {
            frame[element] = static_cast<std::uint32_t>(
                (frameIndex * 7919U + element * 104729U) % 100003U);
            referenceSum[element] += frame[element];
        }
        referenceFrames.push_back(frame);
        if (referenceFrames.size() > kWindow) {
            for (std::size_t element = 0; element < kElements; ++element) {
                referenceSum[element] -= referenceFrames.front()[element];
            }
            referenceFrames.pop_front();
        }
        longExact = longExact &&
            longWindow.push(frame.data(), frame.size()) == RollingSumStatus::Ok &&
            longWindow.sum() == referenceSum &&
            longWindow.frameCount() == referenceFrames.size();
    }
    testOk(longExact && longWindow.frameCount() == kWindow,
           "rolling sum matches a naive last-N reference over 257 changing frames");

    const auto longShrunk = longWindow.configure(kElements, 11, longBudget);
    while (referenceFrames.size() > 11) {
        for (std::size_t element = 0; element < kElements; ++element) {
            referenceSum[element] -= referenceFrames.front()[element];
        }
        referenceFrames.pop_front();
    }
    testOk(longShrunk.status == RollingSumStatus::Ok &&
               longWindow.frameCount() == 11 && longWindow.sum() == referenceSum,
           "rolling sum remains exact when a populated long window is reduced");

    const auto overflow = sum.configure(
        std::numeric_limits<std::size_t>::max(), 1,
        std::numeric_limits<std::size_t>::max());
    testOk(overflow.status == RollingSumStatus::SizeOverflow,
           "rolling sum rejects geometry whose byte calculation overflows size_t");
}

}  // namespace

MAIN(servalProtocolFixtureTest)
{
    testPlan(322);
    testTcpScript();
    testTcpSilenceIsBounded();
    testProductionNetworkClient();
    testAcquisitionCoordinator();
    testStreamWorkerState();
    testConsumeOnceStreamFraming();
    testHttpRequestAndResponse();
    testProductionHttpClient();
    testReconnectPolicy();
    testMeasurementResponseValidation();
    testDetectorConfigBooleanSerialization();
    testDetectorResponseValidation();
    testDacUpdateValidation();
    testDestinationResponseValidation();
    testDashboardResponseValidation();
    testHealthResponseValidation();
    testPixelConfigResponseValidation();
    testStreamHeaderValidation();
    testBpcFileBounds();
    testBpcMaskSemantics();
    testMaskGeometry();
    testOneShotActions();
    testRollingWindowSum();
    testNumericRangeContract();
    return testDone();
}
