/*
 * ADTimePix3 deterministic fake Serval HTTP peer
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef ADTIMEPIX_TEST_FAKE_SERVAL_HTTP_SERVER_H
#define ADTIMEPIX_TEST_FAKE_SERVAL_HTTP_SERVER_H

#include <chrono>
#include <map>
#include <memory>
#include <string>

namespace adtimepix_test {

struct FakeHttpRequest {
    std::string method;
    std::string target;
    std::map<std::string, std::string> headers;
    std::string body;
};

struct FakeHttpResponse {
    FakeHttpResponse();

    int status;
    std::string reason;
    std::map<std::string, std::string> headers;
    std::string body;
};

class FakeServalHttpServer {
public:
    explicit FakeServalHttpServer(const FakeHttpResponse& response,
                                  bool holdResponse = false);
    ~FakeServalHttpServer();

    FakeServalHttpServer(const FakeServalHttpServer&) = delete;
    FakeServalHttpServer& operator=(const FakeServalHttpServer&) = delete;

    unsigned short port() const;
    std::string baseUrl() const;
    bool waitForRequest(std::chrono::milliseconds timeout);
    FakeHttpRequest request() const;
    void releaseResponse();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace adtimepix_test

#endif
