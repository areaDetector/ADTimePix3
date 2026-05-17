/*
 * ADTimePix3 - TCP socket client for jsonimage/jsonhisto streaming
 *
 * Copyright (c) 2022 Brookhaven Science Associates, Brookhaven National Laboratory
 * Copyright (c) 2022-2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef ADTIMEPIX_NETWORK_CLIENT_H
#define ADTIMEPIX_NETWORK_CLIENT_H

#include <cstddef>
#include <string>
#include <sys/types.h>

/**
 * @brief Network client for TCP socket communication
 */
class NetworkClient {
public:
    NetworkClient();
    ~NetworkClient();

    // Disable copy
    NetworkClient(const NetworkClient&) = delete;
    NetworkClient& operator=(const NetworkClient&) = delete;

    // Allow move
    NetworkClient(NetworkClient&& other) noexcept;
    NetworkClient& operator=(NetworkClient&& other) noexcept;

    /**
     * @brief Connect to server
     * @param host Server hostname/IP
     * @param port Server port
     * @return true if successful, false otherwise
     */
    bool connect(const std::string& host, int port);

    /**
     * @brief Disconnect from server
     */
    void disconnect();

    /**
     * @brief Check if connected
     * @return true if connected
     */
    bool is_connected() const { return connected_; }

    /**
     * @brief Receive data from socket
     * @param buffer Buffer to store received data
     * @param max_size Maximum size to receive
     * @return Number of bytes received, -1 on error, 0 on connection closed
     */
    ssize_t receive(char* buffer, size_t max_size);

    /**
     * @brief Receive exact amount of data
     * @param buffer Buffer to store received data
     * @param size Exact size to receive
     * @return true if successful, false otherwise
     */
    bool receive_exact(char* buffer, size_t size);

private:
    int socket_fd_;
    bool connected_;
};

// Constants for TCP streaming

#endif /* ADTIMEPIX_NETWORK_CLIENT_H */
