/*
 * ADTimePix3 - TCP socket client for jsonimage/jsonhisto streaming
 *
 * Copyright (c) 2022 Brookhaven Science Associates, Brookhaven National Laboratory
 * Copyright (c) 2022-2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#include "network_client.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/time.h>

// NetworkClient class implementation
NetworkClient::NetworkClient() : socket_fd_(-1), connected_(false) {}

NetworkClient::~NetworkClient() {
    disconnect();
}

NetworkClient::NetworkClient(NetworkClient&& other) noexcept
    : socket_fd_(-1), connected_(false)
{
    std::lock_guard<std::mutex> guard(other.lifecycleMutex_);
    const int socketFd = other.socket_fd_.exchange(-1, std::memory_order_acq_rel);
    const bool connected = other.connected_.exchange(false, std::memory_order_acq_rel);
    socket_fd_.store(socketFd, std::memory_order_release);
    connected_.store(connected && socketFd >= 0, std::memory_order_release);
}

NetworkClient& NetworkClient::operator=(NetworkClient&& other) noexcept
{
    if (this == &other) {
        return *this;
    }

    std::scoped_lock<std::mutex, std::mutex> guard(lifecycleMutex_, other.lifecycleMutex_);
    connected_.store(false, std::memory_order_release);
    const int oldFd = socket_fd_.exchange(-1, std::memory_order_acq_rel);
    if (oldFd >= 0) {
        ::shutdown(oldFd, SHUT_RDWR);
        ::close(oldFd);
    }

    const int socketFd = other.socket_fd_.exchange(-1, std::memory_order_acq_rel);
    const bool connected = other.connected_.exchange(false, std::memory_order_acq_rel);
    socket_fd_.store(socketFd, std::memory_order_release);
    connected_.store(connected && socketFd >= 0, std::memory_order_release);
    return *this;
}

bool NetworkClient::connect(const std::string& host, int port) {
    std::lock_guard<std::mutex> guard(lifecycleMutex_);

    const int oldFd = socket_fd_.exchange(-1, std::memory_order_acq_rel);
    connected_.store(false, std::memory_order_release);
    if (oldFd >= 0) {
        ::shutdown(oldFd, SHUT_RDWR);
        ::close(oldFd);
    }

    const int socketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFd < 0) {
        std::cerr << "Socket creation failed: " << strerror(errno) << std::endl;
        return false;
    }
    socket_fd_.store(socketFd, std::memory_order_release);

    int opt = 1;
    if (setsockopt(socketFd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt)) < 0) {
        std::cerr << "Failed to set SO_KEEPALIVE: " << strerror(errno) << std::endl;
    }

    int keepidle = 60;
    int keepintvl = 10;
    int keepcnt = 3;
    setsockopt(socketFd, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle));
    setsockopt(socketFd, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl));
    setsockopt(socketFd, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt));

    int rcvbuf = 64 * 1024;
    if (setsockopt(socketFd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf)) < 0) {
        std::cerr << "Failed to set receive buffer size: " << strerror(errno) << std::endl;
    }

    struct timeval receiveTimeout{};
    receiveTimeout.tv_sec = kReceivePollTimeoutMs / 1000;
    receiveTimeout.tv_usec = (kReceivePollTimeoutMs % 1000) * 1000;
    if (setsockopt(socketFd, SOL_SOCKET, SO_RCVTIMEO,
                   &receiveTimeout, sizeof(receiveTimeout)) < 0) {
        std::cerr << "Failed to set receive timeout: " << strerror(errno) << std::endl;
    }

    struct linger lingerOpt{};
    lingerOpt.l_onoff = 1;
    lingerOpt.l_linger = 5;
    setsockopt(socketFd, SOL_SOCKET, SO_LINGER, &lingerOpt, sizeof(lingerOpt));

    struct sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr) <= 0) {
        struct addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        char portString[16];
        snprintf(portString, sizeof(portString), "%d", port);

        struct addrinfo* result = nullptr;
        const int error = getaddrinfo(host.c_str(), portString, &hints, &result);
        if (error != 0 || result == nullptr) {
            std::cerr << "Invalid address or hostname: " << host;
            if (error != 0) {
                std::cerr << " (" << gai_strerror(error) << ")";
            }
            std::cerr << std::endl;
            ::close(socketFd);
            socket_fd_.store(-1, std::memory_order_release);
            return false;
        }

        const auto* address = reinterpret_cast<struct sockaddr_in*>(result->ai_addr);
        serverAddr.sin_addr = address->sin_addr;
        freeaddrinfo(result);
    }

    if (::connect(socketFd, reinterpret_cast<struct sockaddr*>(&serverAddr),
                  sizeof(serverAddr)) < 0) {
        std::cerr << "Connection failed: " << strerror(errno) << std::endl;
        ::close(socketFd);
        socket_fd_.store(-1, std::memory_order_release);
        return false;
    }

    connected_.store(true, std::memory_order_release);
    return true;
}

namespace {

bool resolveHostIpv4(const std::string& host, struct in_addr& out) {
    if (inet_pton(AF_INET, host.c_str(), &out) > 0) {
        return true;
    }

    struct addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[] = "0";
    struct addrinfo* result = nullptr;
    const int err = getaddrinfo(host.c_str(), port_str, &hints, &result);
    if (err != 0 || result == nullptr) {
        return false;
    }

    const auto* addr_in = reinterpret_cast<struct sockaddr_in*>(result->ai_addr);
    out = addr_in->sin_addr;
    freeaddrinfo(result);
    return true;
}

}  // namespace

bool NetworkClient::isTcpPortInUse(const std::string& host, int port) {
    struct in_addr bind_addr{};
    if (!resolveHostIpv4(host, bind_addr)) {
        return false;
    }

    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return false;
    }

    int reuse = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in sa{};
    sa.sin_family = AF_INET;
    sa.sin_port = htons(static_cast<uint16_t>(port));
    sa.sin_addr = bind_addr;

    const bool in_use = (bind(fd, reinterpret_cast<struct sockaddr*>(&sa), sizeof(sa)) != 0 &&
                         errno == EADDRINUSE);
    ::close(fd);
    return in_use;
}

void NetworkClient::disconnect() {
    std::lock_guard<std::mutex> guard(lifecycleMutex_);
    connected_.store(false, std::memory_order_release);
    const int socketFd = socket_fd_.exchange(-1, std::memory_order_acq_rel);
    if (socketFd >= 0) {
        ::shutdown(socketFd, SHUT_RDWR);
        ::close(socketFd);
    }
}

void NetworkClient::interrupt() {
    std::lock_guard<std::mutex> guard(lifecycleMutex_);
    connected_.store(false, std::memory_order_release);
    const int socketFd = socket_fd_.load(std::memory_order_acquire);
    if (socketFd >= 0) {
        ::shutdown(socketFd, SHUT_RDWR);
    }
}

ssize_t NetworkClient::receive(char* buffer, size_t maxSize) {
    const int socketFd = socket_fd_.load(std::memory_order_acquire);
    if (!connected_.load(std::memory_order_acquire) || socketFd < 0) {
        return -1;
    }

    const ssize_t bytesRead = recv(socketFd, buffer, maxSize, 0);
    if (bytesRead == 0) {
        connected_.store(false, std::memory_order_release);
    } else if (bytesRead < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        connected_.store(false, std::memory_order_release);
    }
    return bytesRead;
}

bool NetworkClient::isReceiveTimeout(int errorCode) {
    return errorCode == EAGAIN || errorCode == EWOULDBLOCK;
}

bool NetworkClient::receive_exact(char* buffer, size_t size) {
    size_t total_received = 0;
    int consecutive_timeouts = 0;
    constexpr int max_consecutive_timeouts = 4;
    
    while (total_received < size) {
        ssize_t bytes = receive(buffer + total_received, size - total_received);

        if (bytes < 0 && isReceiveTimeout(errno) &&
            ++consecutive_timeouts < max_consecutive_timeouts) {
            continue;
        }
        if (bytes <= 0) {
            return false;
        }

        consecutive_timeouts = 0;
        total_received += bytes;
    }
    
    return true;
}
