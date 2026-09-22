/*
 * Typed validation for Serval GET /detector/chips/<chip>/PixelConfig responses.
 *
 * SPDX-License-Identifier: MIT
 */

#include "serval_pixel_config.h"

#include <cctype>
#include <limits>
#include <utility>

#include <json.hpp>

namespace ADTimePix3ServalPixelConfig {
namespace {

int base64Value(unsigned char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

bool decodeBase64Strict(const std::string& encoded, std::vector<std::uint8_t>& decoded)
{
    std::string compact;
    compact.reserve(encoded.size());
    for (unsigned char c : encoded) {
        if (!std::isspace(c)) compact.push_back(static_cast<char>(c));
    }
    decoded.clear();
    if (compact.empty() || compact.size() % 4 != 0) return false;

    decoded.reserve((compact.size() / 4) * 3);
    for (std::size_t pos = 0; pos < compact.size(); pos += 4) {
        const bool last = pos + 4 == compact.size();
        const unsigned char c0 = static_cast<unsigned char>(compact[pos]);
        const unsigned char c1 = static_cast<unsigned char>(compact[pos + 1]);
        const unsigned char c2 = static_cast<unsigned char>(compact[pos + 2]);
        const unsigned char c3 = static_cast<unsigned char>(compact[pos + 3]);
        const int a = base64Value(c0);
        const int b = base64Value(c1);
        if (a < 0 || b < 0) return false;

        if (c2 == '=') {
            if (!last || c3 != '=' || (b & 0x0f) != 0) return false;
            decoded.push_back(static_cast<std::uint8_t>((a << 2) | (b >> 4)));
            continue;
        }
        const int c = base64Value(c2);
        if (c < 0) return false;
        decoded.push_back(static_cast<std::uint8_t>((a << 2) | (b >> 4)));

        if (c3 == '=') {
            if (!last || (c & 0x03) != 0) return false;
            decoded.push_back(static_cast<std::uint8_t>((b << 4) | (c >> 2)));
            continue;
        }
        const int d = base64Value(c3);
        if (d < 0) return false;
        decoded.push_back(static_cast<std::uint8_t>((b << 4) | (c >> 2)));
        decoded.push_back(static_cast<std::uint8_t>((c << 6) | d));
    }
    return true;
}

}  // namespace

ResponseError parseResponse(long statusCode, const std::string& body,
                            std::size_t expectedBytes,
                            std::vector<std::uint8_t>& decoded)
{
    decoded.clear();
    if (statusCode != 200) return ResponseError::HttpFailure;
    if (body.empty()) return ResponseError::EmptyBody;
    const nlohmann::json parsed = nlohmann::json::parse(body, nullptr, false);
    if (parsed.is_discarded()) return ResponseError::MalformedJson;
    if (!parsed.is_string()) return ResponseError::InvalidRoot;

    if (expectedBytes == 0 || expectedBytes > std::numeric_limits<std::size_t>::max() - 2) {
        return ResponseError::LengthMismatch;
    }
    const std::string encoded = parsed.get<std::string>();
    std::size_t encodedCharacters = 0;
    for (unsigned char c : encoded) {
        if (!std::isspace(c)) ++encodedCharacters;
    }
    const std::size_t encodedGroups = (expectedBytes + 2) / 3;
    if (encodedGroups > std::numeric_limits<std::size_t>::max() / 4) {
        return ResponseError::LengthMismatch;
    }
    const std::size_t expectedEncodedCharacters = encodedGroups * 4;
    if (encodedCharacters != expectedEncodedCharacters) return ResponseError::LengthMismatch;

    std::vector<std::uint8_t> candidate;
    if (!decodeBase64Strict(encoded, candidate)) {
        return ResponseError::InvalidBase64;
    }
    if (candidate.size() != expectedBytes) {
        return ResponseError::LengthMismatch;
    }
    decoded = std::move(candidate);
    return ResponseError::None;
}

std::size_t bytesPerChip(std::size_t pixelsPerChip, int bytesPerPixel,
                         int thresholdSlices)
{
    if (pixelsPerChip == 0 || bytesPerPixel <= 0 || thresholdSlices <= 0) return 0;
    const std::size_t multiplier = static_cast<std::size_t>(bytesPerPixel) *
                                   static_cast<std::size_t>(thresholdSlices);
    if (static_cast<std::size_t>(bytesPerPixel) != 0 &&
        multiplier / static_cast<std::size_t>(bytesPerPixel) !=
            static_cast<std::size_t>(thresholdSlices)) return 0;
    if (pixelsPerChip > std::numeric_limits<std::size_t>::max() / multiplier) return 0;
    return pixelsPerChip * multiplier;
}

bool selectedSliceIndex(std::size_t logicalIndex, std::size_t pixelsPerChip,
                        int bytesPerPixel, int thresholdSlices,
                        int selectedSlice, std::size_t& physicalIndex)
{
    physicalIndex = 0;
    const std::size_t chipBytes = bytesPerChip(pixelsPerChip, bytesPerPixel,
                                               thresholdSlices);
    if (chipBytes == 0 || selectedSlice < 0 || selectedSlice >= thresholdSlices) return false;
    const std::size_t chip = logicalIndex / pixelsPerChip;
    const std::size_t localPixel = logicalIndex % pixelsPerChip;
    const std::size_t sliceBytes = pixelsPerChip * static_cast<std::size_t>(bytesPerPixel);
    const std::size_t sliceOffset = static_cast<std::size_t>(selectedSlice) * sliceBytes;
    const std::size_t pixelOffset = localPixel * static_cast<std::size_t>(bytesPerPixel);
    if (chip > std::numeric_limits<std::size_t>::max() / chipBytes) return false;
    const std::size_t chipOffset = chip * chipBytes;
    if (chipOffset > std::numeric_limits<std::size_t>::max() - sliceOffset) return false;
    const std::size_t selectedOffset = chipOffset + sliceOffset;
    if (selectedOffset > std::numeric_limits<std::size_t>::max() - pixelOffset) return false;
    physicalIndex = selectedOffset + pixelOffset;
    return true;
}

const char* responseErrorMessage(ResponseError error)
{
    switch (error) {
    case ResponseError::None: return "OK";
    case ResponseError::HttpFailure: return "HTTP request failed";
    case ResponseError::EmptyBody: return "Empty response body";
    case ResponseError::MalformedJson: return "Malformed JSON response";
    case ResponseError::InvalidRoot: return "JSON response is not a string";
    case ResponseError::InvalidBase64: return "Invalid base64 payload";
    case ResponseError::LengthMismatch: return "Decoded PixelConfig length mismatch";
    }
    return "Unknown PixelConfig response error";
}

}  // namespace ADTimePix3ServalPixelConfig
