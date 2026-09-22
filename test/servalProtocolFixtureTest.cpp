/*
 * ADTimePix3 deterministic fake Serval fixture tests
 *
 * SPDX-License-Identifier: MIT
 */

#include "FakeServalHttpServer.h"
#include "FakeServalTcpServer.h"
#include "bpc_file_io.h"
#include "network_client.h"
#include "one_shot_action.h"
#include "serval_config.h"
#include "serval_dacs.h"
#include "serval_dashboard.h"
#include "serval_detector.h"
#include "serval_destination.h"
#include "serval_health.h"
#include "serval_http.h"
#include "serval_measurement.h"
#include "serval_stream_validation.h"

#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <limits>
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
               "{\"Info\":null}", snapshot) == StatusResponseError::InvalidInfo,
           "measurement status parser rejects a non-object Info field");
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
    testOk(ADTimePix3BpcFile::expectedSize(4 * 256 * 256, 1, 2, size) &&
               size == 524288U,
           "production BPC sizing preserves the two-slice MPX3 layout");
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

}  // namespace

MAIN(servalProtocolFixtureTest)
{
    testPlan(183);
    testTcpScript();
    testTcpSilenceIsBounded();
    testProductionNetworkClient();
    testHttpRequestAndResponse();
    testProductionHttpClient();
    testMeasurementResponseValidation();
    testDetectorConfigBooleanSerialization();
    testDetectorResponseValidation();
    testDacUpdateValidation();
    testDestinationResponseValidation();
    testDashboardResponseValidation();
    testHealthResponseValidation();
    testStreamHeaderValidation();
    testBpcFileBounds();
    testOneShotActions();
    return testDone();
}
