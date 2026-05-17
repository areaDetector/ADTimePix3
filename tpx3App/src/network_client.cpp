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

// NetworkClient class implementation
NetworkClient::NetworkClient() : socket_fd_(-1), connected_(false) {}

NetworkClient::~NetworkClient() {
    disconnect();
}

// Move constructor
NetworkClient::NetworkClient(NetworkClient&& other) noexcept
    : socket_fd_(other.socket_fd_), connected_(other.connected_) {
    other.socket_fd_ = -1;
    other.connected_ = false;
}

NetworkClient& NetworkClient::operator=(NetworkClient&& other) noexcept {
    if (this != &other) {
        disconnect();
        socket_fd_ = other.socket_fd_;
        connected_ = other.connected_;
        other.socket_fd_ = -1;
        other.connected_ = false;
    }
    return *this;
}

bool NetworkClient::connect(const std::string& host, int port) {
    // Close existing connection if any
    if (socket_fd_ >= 0) {
        ::close(socket_fd_);
        socket_fd_ = -1;
    }
    connected_ = false;
    
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        std::cerr << "Socket creation failed: " << strerror(errno) << std::endl;
        return false;
    }

    // Enable TCP keepalive
    int opt = 1;
    if (setsockopt(socket_fd_, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt)) < 0) {
        std::cerr << "Failed to set SO_KEEPALIVE: " << strerror(errno) << std::endl;
    }
    
    // Configure TCP keepalive parameters
    int keepidle = 60;
    int keepintvl = 10;
    int keepcnt = 3;
    
    if (setsockopt(socket_fd_, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle)) < 0) {
        // Not critical
    }
    if (setsockopt(socket_fd_, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl)) < 0) {
        // Not critical
    }
    if (setsockopt(socket_fd_, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt)) < 0) {
        // Not critical
    }

    // Set socket buffers
    int rcvbuf = 64 * 1024;
    if (setsockopt(socket_fd_, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf)) < 0) {
        std::cerr << "Failed to set receive buffer size: " << strerror(errno) << std::endl;
    }
    
    // Set SO_LINGER
    struct linger linger_opt;
    linger_opt.l_onoff = 1;
    linger_opt.l_linger = 5;
    if (setsockopt(socket_fd_, SOL_SOCKET, SO_LINGER, &linger_opt, sizeof(linger_opt)) < 0) {
        // Not critical
    }

    struct sockaddr_in server_addr{};
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    // Try to parse as IP address first
    if (inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr) <= 0) {
        // If not an IP address, try to resolve as hostname using getaddrinfo
        struct addrinfo hints;
        struct addrinfo *result = NULL;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        
        char port_str[16];
        snprintf(port_str, sizeof(port_str), "%d", port);
        
        int err = getaddrinfo(host.c_str(), port_str, &hints, &result);
        if (err != 0 || result == NULL) {
            std::cerr << "Invalid address or hostname: " << host;
            if (err != 0) {
                std::cerr << " (" << gai_strerror(err) << ")";
            }
            std::cerr << std::endl;
            ::close(socket_fd_);
            socket_fd_ = -1;
            return false;
        }
        
        // Use the first result (IPv4 address)
        struct sockaddr_in *addr_in = (struct sockaddr_in *)result->ai_addr;
        server_addr.sin_addr = addr_in->sin_addr;
        
        freeaddrinfo(result);
    }
    
    if (::connect(socket_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Connection failed: " << strerror(errno) << std::endl;
        ::close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }
    
    connected_ = true;
    return true;
}

void NetworkClient::disconnect() {
    if (socket_fd_ >= 0) {
        ::close(socket_fd_);
        socket_fd_ = -1;
    }
    connected_ = false;
}

ssize_t NetworkClient::receive(char* buffer, size_t max_size) {
    if (!connected_ || socket_fd_ < 0) {
        return -1;
    }
    
    ssize_t bytes_read = recv(socket_fd_, buffer, max_size, 0);
    
    if (bytes_read == 0) {
        // Connection closed by peer - let the caller log this with context (channel name)
        connected_ = false;
    } else if (bytes_read < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            // Socket error - let the caller log this with context (channel name)
            connected_ = false;
        }
    }
    
    return bytes_read;
}

bool NetworkClient::receive_exact(char* buffer, size_t size) {
    size_t total_received = 0;
    
    while (total_received < size) {
        ssize_t bytes = receive(buffer + total_received, size - total_received);
        
        if (bytes <= 0) {
            return false;
        }
        
        total_received += bytes;
    }
    
    return true;
}
