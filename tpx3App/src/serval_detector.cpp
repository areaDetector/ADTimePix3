/*
 * ADTimePix3 - Serval detector response validation
 *
 * Copyright (c) 2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#include "serval_detector.h"

#include <limits>
#include <utility>

namespace ADTimePix3ServalDetector {

namespace {

bool positiveInt(const nlohmann::json& object, const char* field, int& value)
{
    if (!object.contains(field)) return false;
    const nlohmann::json& candidate = object[field];
    if (candidate.is_number_unsigned()) {
        const unsigned long long parsed = candidate.get<unsigned long long>();
        if (parsed == 0 || parsed > static_cast<unsigned long long>(std::numeric_limits<int>::max())) {
            return false;
        }
        value = static_cast<int>(parsed);
        return true;
    }
    if (candidate.is_number_integer()) {
        const long long parsed = candidate.get<long long>();
        if (parsed <= 0 || parsed > std::numeric_limits<int>::max()) return false;
        value = static_cast<int>(parsed);
        return true;
    }
    return false;
}

bool nonnegativeInt(const nlohmann::json& object, const char* field, int& value)
{
    if (!object.contains(field)) return false;
    const nlohmann::json& candidate = object[field];
    if (candidate.is_number_unsigned()) {
        const unsigned long long parsed = candidate.get<unsigned long long>();
        if (parsed > static_cast<unsigned long long>(std::numeric_limits<int>::max())) return false;
        value = static_cast<int>(parsed);
        return true;
    }
    if (candidate.is_number_integer()) {
        const long long parsed = candidate.get<long long>();
        if (parsed < 0 || parsed > std::numeric_limits<int>::max()) return false;
        value = static_cast<int>(parsed);
        return true;
    }
    return false;
}

}  // namespace

ParseError parseResponse(const std::string& body, Snapshot& snapshot)
{
    snapshot = Snapshot{};
    snapshot.response = nlohmann::json::object();
    if (body.empty()) return ParseError::EmptyBody;

    nlohmann::json parsed = nlohmann::json::parse(body, nullptr, false);
    if (parsed.is_discarded()) return ParseError::MalformedJson;
    if (!parsed.is_object()) return ParseError::InvalidRoot;
    if (!parsed.contains("Info") || !parsed["Info"].is_object()) {
        return ParseError::MissingInfo;
    }
    if (!parsed.contains("Config") || !parsed["Config"].is_object()) {
        return ParseError::MissingConfig;
    }

    const nlohmann::json& info = parsed["Info"];
    if (!positiveInt(info, "PixCount", snapshot.pixelCount) ||
        !positiveInt(info, "RowLen", snapshot.rowLength) ||
        !positiveInt(info, "NumberOfChips", snapshot.numberOfChips) ||
        !positiveInt(info, "NumberOfRows", snapshot.numberOfRows) ||
        !nonnegativeInt(info, "MpxType", snapshot.mpxType) ||
        snapshot.pixelCount % snapshot.numberOfRows != 0) {
        return ParseError::InvalidGeometry;
    }

    snapshot.response = std::move(parsed);
    return ParseError::None;
}

const char* parseErrorMessage(ParseError error)
{
    switch (error) {
        case ParseError::None:
            return "valid detector response";
        case ParseError::EmptyBody:
            return "empty detector response";
        case ParseError::MalformedJson:
            return "malformed detector JSON";
        case ParseError::InvalidRoot:
            return "detector JSON root is not an object";
        case ParseError::MissingInfo:
            return "detector response is missing the Info object";
        case ParseError::MissingConfig:
            return "detector response is missing the Config object";
        case ParseError::InvalidGeometry:
            return "detector response contains invalid geometry";
    }
    return "unknown detector response error";
}

}  // namespace ADTimePix3ServalDetector
