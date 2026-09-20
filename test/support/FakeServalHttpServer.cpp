/*
 * ADTimePix3 deterministic fake Serval HTTP peer
 *
 * SPDX-License-Identifier: MIT
 */

#include "FakeServalHttpServer.h"

#include "BoundedWait.h"

#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cerrno>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace adtimepix_test {
namespace {

const std::size_t kMaximumRequestBytes = 1024 * 1024;

std::string lowerCase(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return value;
}

std::string trim(const std::string& value)
{
    const std::string whitespace = " \t\r\n";
    const std::size_t first = value.find_first_not_of(whitespace);
    if (first == std::string::npos) {
        return std::string();
    }
    const std::size_t last = value.find_last_not_of(whitespace);
    return value.substr(first, last - first + 1);
}

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

bool parseRequest(const std::string& raw, FakeHttpRequest& request)
{
    const std::size_t headerEnd = raw.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        return false;
    }
    std::istringstream stream(raw.substr(0, headerEnd));
    std::string line;
    if (!std::getline(stream, line)) {
        return false;
    }
    line = trim(line);
    std::istringstream requestLine(line);
    std::string version;
    if (!(requestLine >> request.method >> request.target >> version)) {
        return false;
    }
    while (std::getline(stream, line)) {
        const std::size_t separator = line.find(':');
        if (separator != std::string::npos) {
            request.headers[lowerCase(trim(line.substr(0, separator)))] =
                trim(line.substr(separator + 1));
        }
    }
    request.body = raw.substr(headerEnd + 4);
    return true;
}

std::size_t expectedRequestSize(const std::string& raw)
{
    const std::size_t headerEnd = raw.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        return 0;
    }
    FakeHttpRequest request;
    if (!parseRequest(raw, request)) {
        return headerEnd + 4;
    }
    std::size_t contentLength = 0;
    const auto found = request.headers.find("content-length");
    if (found != request.headers.end()) {
        std::istringstream value(found->second);
        value >> contentLength;
    }
    return headerEnd + 4 + contentLength;
}

}  // namespace

FakeHttpResponse::FakeHttpResponse() : status(200), reason("OK") {}

struct FakeServalHttpServer::Impl {
    Impl(const FakeHttpResponse& configuredResponse, bool hold)
        : response(configuredResponse), listenFd(-1), port(0), stop(false),
          requestReady(false), responseReleased(!hold)
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

        std::string raw;
        std::size_t expected = 0;
        while (raw.size() < kMaximumRequestBytes) {
            {
                std::lock_guard<std::mutex> guard(mutex);
                if (stop) {
                    break;
                }
            }
            pollfd descriptor{clientFd, POLLIN, 0};
            const int ready = poll(&descriptor, 1, 25);
            if (ready > 0 && (descriptor.revents & (POLLIN | POLLHUP))) {
                char buffer[4096];
                const ssize_t count = recv(clientFd, buffer, sizeof(buffer), 0);
                if (count <= 0) {
                    break;
                }
                raw.append(buffer, static_cast<std::size_t>(count));
                expected = expectedRequestSize(raw);
                if (expected > 0 && raw.size() >= expected) {
                    raw.resize(expected);
                    break;
                }
            } else if (ready < 0 && errno != EINTR) {
                break;
            }
        }

        FakeHttpRequest parsed;
        if (parseRequest(raw, parsed)) {
            std::lock_guard<std::mutex> guard(mutex);
            recordedRequest = parsed;
            requestReady = true;
            condition.notify_all();
        }

        {
            std::unique_lock<std::mutex> lock(mutex);
            condition.wait(lock, [this]() { return stop || responseReleased; });
            if (stop) {
                lock.unlock();
                shutdown(clientFd, SHUT_RDWR);
                close(clientFd);
                return;
            }
        }

        std::ostringstream wire;
        wire << "HTTP/1.1 " << response.status << ' ' << response.reason << "\r\n";
        for (const auto& header : response.headers) {
            wire << header.first << ": " << header.second << "\r\n";
        }
        wire << "Content-Length: " << response.body.size() << "\r\n"
             << "Connection: close\r\n\r\n" << response.body;
        sendAll(clientFd, wire.str());
        shutdown(clientFd, SHUT_RDWR);
        close(clientFd);
    }

    FakeHttpResponse response;
    int listenFd;
    unsigned short port;
    std::thread worker;
    mutable std::mutex mutex;
    std::condition_variable condition;
    bool stop;
    bool requestReady;
    bool responseReleased;
    FakeHttpRequest recordedRequest;
};

FakeServalHttpServer::FakeServalHttpServer(const FakeHttpResponse& response,
                                           bool holdResponse)
    : impl_(new Impl(response, holdResponse))
{}

FakeServalHttpServer::~FakeServalHttpServer() = default;

unsigned short FakeServalHttpServer::port() const
{
    return impl_->port;
}

std::string FakeServalHttpServer::baseUrl() const
{
    std::ostringstream url;
    url << "http://127.0.0.1:" << port();
    return url.str();
}

bool FakeServalHttpServer::waitForRequest(std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> lock(impl_->mutex);
    return boundedWait(impl_->condition, lock, timeout,
                       [this]() { return impl_->requestReady; });
}

FakeHttpRequest FakeServalHttpServer::request() const
{
    std::lock_guard<std::mutex> guard(impl_->mutex);
    return impl_->recordedRequest;
}

void FakeServalHttpServer::releaseResponse()
{
    {
        std::lock_guard<std::mutex> guard(impl_->mutex);
        impl_->responseReleased = true;
    }
    impl_->condition.notify_all();
}

}  // namespace adtimepix_test
