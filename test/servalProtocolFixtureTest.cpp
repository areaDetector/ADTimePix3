/*
 * ADTimePix3 deterministic fake Serval fixture tests
 *
 * SPDX-License-Identifier: MIT
 */

#include "FakeServalHttpServer.h"
#include "FakeServalTcpServer.h"
#include "network_client.h"
#include "serval_config.h"
#include "serval_stream_validation.h"

#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <string>
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

void testBiasEnabledSerialization()
{
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
               nlohmann::json{{"width", 512}, {"height", 512}, {"pixelFormat", "uint8"}},
               limits, layout) == ImageHeaderError::UnsupportedPixelFormat,
           "production validator rejects unknown pixel formats");
    const ADTimePix3Stream::ImageFrameLimits byteLimited{1024U, 1024U * 512U, 1024U};
    testOk(ADTimePix3Stream::validateJsonImageHeader(
               nlohmann::json{{"width", 512}, {"height", 512}, {"pixelFormat", "uint16"}},
               byteLimited, layout) == ImageHeaderError::PayloadLimitExceeded,
           "production validator enforces the configured payload-byte budget");

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

}  // namespace

MAIN(servalProtocolFixtureTest)
{
    testPlan(42);
    testTcpScript();
    testTcpSilenceIsBounded();
    testProductionNetworkClient();
    testHttpRequestAndResponse();
    testBiasEnabledSerialization();
    testStreamHeaderValidation();
    return testDone();
}
