/*
 * ADTimePix3 deterministic fake Serval TCP peer
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef ADTIMEPIX_TEST_FAKE_SERVAL_TCP_SERVER_H
#define ADTIMEPIX_TEST_FAKE_SERVAL_TCP_SERVER_H

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace adtimepix_test {

class FakeServalTcpServer {
public:
    explicit FakeServalTcpServer(const std::vector<std::string>& chunks,
                                 bool closeAfterLastChunk = true);
    ~FakeServalTcpServer();

    FakeServalTcpServer(const FakeServalTcpServer&) = delete;
    FakeServalTcpServer& operator=(const FakeServalTcpServer&) = delete;

    unsigned short port() const;
    bool waitForClient(std::chrono::milliseconds timeout);
    void releaseNextChunk();
    void releaseAllChunks();
    bool waitForChunksSent(std::size_t count, std::chrono::milliseconds timeout);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace adtimepix_test

#endif
