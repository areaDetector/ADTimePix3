/*
 * ADTimePix3 - consume-once Serval TCP stream framing
 *
 * Copyright (c) 2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SERVAL_STREAM_FRAMING_H
#define SERVAL_STREAM_FRAMING_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace ADTimePix3Stream {

struct FramedMessage {
    std::string header;
    std::vector<std::uint8_t> payload;
};

enum class FrameResult {
    NeedMoreData,
    FrameReady,
    InvalidHeader,
    InvalidTrailer,
    HeaderTooLarge,
    PayloadTooLarge,
    BufferLimitExceeded
};

enum class EndOfStreamStatus {
    Clean,
    TruncatedHeader,
    TruncatedPayload,
    TruncatedTrailer
};

using PayloadSizeResolver = std::function<bool(
    const std::string& header, std::size_t& payloadBytes, std::string& error)>;

/**
 * Owns unconsumed TCP bytes for one newline-header/binary-payload stream.
 *
 * The payload size resolver runs exactly once per header. Once resolved, no
 * payload byte is examined for a delimiter. A completed Serval frame consumes
 * its header, header newline, declared payload, and payload newline atomically
 * while retaining only bytes belonging to later messages.
 */
class FrameBuffer {
public:
    FrameBuffer(std::size_t maxHeaderBytes,
                std::size_t maxPayloadBytes,
                std::size_t maxTrailingBytes);

    FrameResult append(const char* data, std::size_t size);

    FrameResult next(const PayloadSizeResolver& resolvePayloadSize,
                     FramedMessage& frame,
                     std::string& error);

    EndOfStreamStatus endOfStreamStatus() const;

    std::size_t bufferedBytes() const { return bytes_.size(); }
    bool awaitingPayload() const { return payloadKnown_; }
    void clear();

private:
    void resetCurrentFrame();

    std::size_t maxHeaderBytes_;
    std::size_t maxPayloadBytes_;
    std::size_t maxBufferedBytes_;
    std::vector<std::uint8_t> bytes_;
    bool payloadKnown_;
    std::size_t headerBytes_;
    std::size_t payloadBytes_;
    std::string header_;
};

const char* frameResultMessage(FrameResult result);
const char* endOfStreamStatusMessage(EndOfStreamStatus status);

}  // namespace ADTimePix3Stream

#endif
