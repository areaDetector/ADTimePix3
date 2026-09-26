/*
 * ADTimePix3 - consume-once Serval TCP stream framing
 *
 * Copyright (c) 2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#include "serval_stream_framing.h"

#include <algorithm>
#include <exception>
#include <limits>

namespace ADTimePix3Stream {
namespace {

std::size_t saturatedAdd(std::size_t first, std::size_t second)
{
    const std::size_t maximum = std::numeric_limits<std::size_t>::max();
    return first > maximum - second ? maximum : first + second;
}

}  // namespace

FrameBuffer::FrameBuffer(std::size_t maxHeaderBytes,
                         std::size_t maxPayloadBytes,
                         std::size_t maxTrailingBytes)
    : maxHeaderBytes_(maxHeaderBytes),
      maxPayloadBytes_(maxPayloadBytes),
      maxBufferedBytes_(saturatedAdd(
          saturatedAdd(saturatedAdd(maxHeaderBytes, maxPayloadBytes),
                       maxTrailingBytes),
          2)),
      payloadKnown_(false),
      headerBytes_(0),
      payloadBytes_(0)
{
}

FrameResult FrameBuffer::append(const char* data, std::size_t size)
{
    if ((data == nullptr && size != 0) || bytes_.size() > maxBufferedBytes_ ||
        size > maxBufferedBytes_ - bytes_.size()) {
        return FrameResult::BufferLimitExceeded;
    }
    if (size == 0) {
        return FrameResult::NeedMoreData;
    }
    const auto* first = reinterpret_cast<const std::uint8_t*>(data);
    bytes_.insert(bytes_.end(), first, first + size);
    return FrameResult::NeedMoreData;
}

FrameResult FrameBuffer::next(const PayloadSizeResolver& resolvePayloadSize,
                              FramedMessage& frame,
                              std::string& error)
{
    frame = FramedMessage{};
    error.clear();

    if (!payloadKnown_) {
        const auto newline = std::find(bytes_.begin(), bytes_.end(),
                                       static_cast<std::uint8_t>('\n'));
        if (newline == bytes_.end()) {
            return bytes_.size() > maxHeaderBytes_
                ? FrameResult::HeaderTooLarge
                : FrameResult::NeedMoreData;
        }

        const std::size_t headerLength =
            static_cast<std::size_t>(newline - bytes_.begin());
        if (headerLength > maxHeaderBytes_) {
            return FrameResult::HeaderTooLarge;
        }

        std::size_t textLength = headerLength;
        if (textLength != 0 && bytes_[textLength - 1] == '\r') {
            --textLength;
        }
        header_.assign(reinterpret_cast<const char*>(bytes_.data()), textLength);

        std::size_t resolvedPayloadBytes = 0;
        try {
            if (!resolvePayloadSize(header_, resolvedPayloadBytes, error)) {
                return FrameResult::InvalidHeader;
            }
        } catch (const std::exception& exception) {
            error = exception.what();
            return FrameResult::InvalidHeader;
        } catch (...) {
            error = "payload-size resolver threw an unknown exception";
            return FrameResult::InvalidHeader;
        }
        if (resolvedPayloadBytes > maxPayloadBytes_) {
            return FrameResult::PayloadTooLarge;
        }

        headerBytes_ = headerLength + 1;
        payloadBytes_ = resolvedPayloadBytes;
        payloadKnown_ = true;
    }

    if (headerBytes_ > bytes_.size() ||
        payloadBytes_ > bytes_.size() - headerBytes_) {
        return FrameResult::NeedMoreData;
    }

    const std::size_t bodyBytes = headerBytes_ + payloadBytes_;
    if (bytes_.size() == bodyBytes) {
        return FrameResult::NeedMoreData;
    }
    if (bytes_[bodyBytes] != static_cast<std::uint8_t>('\n')) {
        return FrameResult::InvalidTrailer;
    }

    frame.header = header_;
    frame.payload.assign(bytes_.begin() + static_cast<std::ptrdiff_t>(headerBytes_),
                         bytes_.begin() + static_cast<std::ptrdiff_t>(headerBytes_ + payloadBytes_));

    const std::size_t consumed = bodyBytes + 1;
    bytes_.erase(bytes_.begin(), bytes_.begin() + static_cast<std::ptrdiff_t>(consumed));
    resetCurrentFrame();
    return FrameResult::FrameReady;
}

EndOfStreamStatus FrameBuffer::endOfStreamStatus() const
{
    if (bytes_.empty()) {
        return EndOfStreamStatus::Clean;
    }
    if (!payloadKnown_) {
        return EndOfStreamStatus::TruncatedHeader;
    }
    if (headerBytes_ > bytes_.size() ||
        payloadBytes_ > bytes_.size() - headerBytes_) {
        return EndOfStreamStatus::TruncatedPayload;
    }
    return EndOfStreamStatus::TruncatedTrailer;
}

void FrameBuffer::clear()
{
    bytes_.clear();
    resetCurrentFrame();
}

void FrameBuffer::resetCurrentFrame()
{
    payloadKnown_ = false;
    headerBytes_ = 0;
    payloadBytes_ = 0;
    header_.clear();
}

const char* frameResultMessage(FrameResult result)
{
    switch (result) {
    case FrameResult::NeedMoreData:
        return "more stream data is required";
    case FrameResult::FrameReady:
        return "complete frame";
    case FrameResult::InvalidHeader:
        return "invalid JSON frame header";
    case FrameResult::InvalidTrailer:
        return "invalid or missing frame payload terminator";
    case FrameResult::HeaderTooLarge:
        return "frame header exceeds the configured limit";
    case FrameResult::PayloadTooLarge:
        return "frame payload exceeds the configured limit";
    case FrameResult::BufferLimitExceeded:
        return "stream buffer exceeds the configured limit";
    }
    return "unknown frame result";
}

const char* endOfStreamStatusMessage(EndOfStreamStatus status)
{
    switch (status) {
    case EndOfStreamStatus::Clean:
        return "clean frame boundary";
    case EndOfStreamStatus::TruncatedHeader:
        return "truncated frame header";
    case EndOfStreamStatus::TruncatedPayload:
        return "truncated frame payload";
    case EndOfStreamStatus::TruncatedTrailer:
        return "truncated frame payload terminator";
    }
    return "unknown end-of-stream status";
}

}  // namespace ADTimePix3Stream
