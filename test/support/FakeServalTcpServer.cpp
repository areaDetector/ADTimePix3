/*
 * ADTimePix3 deterministic fake Serval TCP peer
 *
 * SPDX-License-Identifier: MIT
 */

#include "FakeServalTcpServer.h"

#include "BoundedWait.h"

#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace adtimepix_test {
namespace {

int makeLoopbackListener(unsigned short& port)
{
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        throw std::runtime_error(std::string("socket: ") + std::strerror(errno));
    }

    int reuse = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
        listen(fd, 1) != 0) {
        const int error = errno;
        close(fd);
        throw std::runtime_error(std::string("loopback listen: ") + std::strerror(error));
    }

    socklen_t length = sizeof(address);
    if (getsockname(fd, reinterpret_cast<sockaddr*>(&address), &length) != 0) {
        const int error = errno;
        close(fd);
        throw std::runtime_error(std::string("getsockname: ") + std::strerror(error));
    }
    port = ntohs(address.sin_port);
    return fd;
}

bool sendAll(int fd, const std::string& bytes)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    std::size_t sent = 0;
    while (sent < bytes.size() && std::chrono::steady_clock::now() < deadline) {
        pollfd descriptor{fd, POLLOUT, 0};
        if (poll(&descriptor, 1, 25) < 0 && errno != EINTR) {
            return false;
        }
        const ssize_t count = send(fd, bytes.data() + sent, bytes.size() - sent,
                                   MSG_NOSIGNAL | MSG_DONTWAIT);
        if (count > 0) {
            sent += static_cast<std::size_t>(count);
        } else if (count < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
            return false;
        }
    }
    return sent == bytes.size();
}

}  // namespace

struct FakeServalTcpServer::Impl {
    explicit Impl(const std::vector<std::string>& scriptedChunks, bool closeAfter)
        : chunks(scriptedChunks), closeAfterLastChunk(closeAfter), listenFd(-1), port(0),
          stop(false), connected(false), permits(0), chunksSent(0)
    {
        listenFd = makeLoopbackListener(port);
        worker = std::thread(&Impl::run, this);
    }

    ~Impl()
    {
        {
            std::lock_guard<std::mutex> guard(mutex);
            stop = true;
        }
        condition.notify_all();
        if (worker.joinable()) {
            worker.join();
        }
        close(listenFd);
    }

    void run()
    {
        int clientFd = -1;
        while (true) {
            {
                std::lock_guard<std::mutex> guard(mutex);
                if (stop) {
                    return;
                }
            }
            pollfd descriptor{listenFd, POLLIN, 0};
            const int ready = poll(&descriptor, 1, 25);
            if (ready > 0 && (descriptor.revents & POLLIN)) {
                clientFd = accept(listenFd, nullptr, nullptr);
                break;
            }
            if (ready < 0 && errno != EINTR) {
                return;
            }
        }
        if (clientFd < 0) {
            return;
        }

        {
            std::lock_guard<std::mutex> guard(mutex);
            connected = true;
        }
        condition.notify_all();

        for (std::size_t index = 0; index < chunks.size(); ++index) {
            std::unique_lock<std::mutex> lock(mutex);
            condition.wait(lock, [this]() { return stop || permits > 0; });
            if (stop) {
                lock.unlock();
                shutdown(clientFd, SHUT_RDWR);
                close(clientFd);
                return;
            }
            --permits;
            lock.unlock();

            if (!sendAll(clientFd, chunks[index])) {
                shutdown(clientFd, SHUT_RDWR);
                close(clientFd);
                return;
            }
            {
                std::lock_guard<std::mutex> guard(mutex);
                ++chunksSent;
            }
            condition.notify_all();
        }

        if (closeAfterLastChunk) {
            shutdown(clientFd, SHUT_WR);
        } else {
            std::unique_lock<std::mutex> lock(mutex);
            condition.wait(lock, [this]() { return stop; });
        }
        shutdown(clientFd, SHUT_RDWR);
        close(clientFd);
    }

    std::vector<std::string> chunks;
    bool closeAfterLastChunk;
    int listenFd;
    unsigned short port;
    std::thread worker;
    std::mutex mutex;
    std::condition_variable condition;
    bool stop;
    bool connected;
    std::size_t permits;
    std::size_t chunksSent;
};

FakeServalTcpServer::FakeServalTcpServer(const std::vector<std::string>& chunks,
                                         bool closeAfterLastChunk)
    : impl_(new Impl(chunks, closeAfterLastChunk))
{}

FakeServalTcpServer::~FakeServalTcpServer() = default;

unsigned short FakeServalTcpServer::port() const
{
    return impl_->port;
}

bool FakeServalTcpServer::waitForClient(std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> lock(impl_->mutex);
    return boundedWait(impl_->condition, lock, timeout,
                       [this]() { return impl_->connected; });
}

void FakeServalTcpServer::releaseNextChunk()
{
    {
        std::lock_guard<std::mutex> guard(impl_->mutex);
        ++impl_->permits;
    }
    impl_->condition.notify_all();
}

void FakeServalTcpServer::releaseAllChunks()
{
    {
        std::lock_guard<std::mutex> guard(impl_->mutex);
        impl_->permits += impl_->chunks.size() - impl_->chunksSent;
    }
    impl_->condition.notify_all();
}

bool FakeServalTcpServer::waitForChunksSent(std::size_t count,
                                            std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> lock(impl_->mutex);
    return boundedWait(impl_->condition, lock, timeout,
                       [this, count]() { return impl_->chunksSent >= count; });
}

}  // namespace adtimepix_test
