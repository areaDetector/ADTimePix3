/*
 * ADTimePix3 - Serval measurement response validation
 *
 * Copyright (c) 2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#include "serval_measurement.h"

#include <cmath>
#include <limits>
#include <utility>

namespace ADTimePix3ServalMeasurement {

ParseError parseResponse(const std::string& body, nlohmann::json& measurement)
{
    measurement = nlohmann::json::object();
    if (body.empty()) {
        return ParseError::EmptyBody;
    }

    nlohmann::json parsed = nlohmann::json::parse(body, nullptr, false);
    if (parsed.is_discarded()) {
        return ParseError::MalformedJson;
    }
    if (!parsed.is_object()) {
        return ParseError::InvalidRoot;
    }

    measurement = std::move(parsed);
    return ParseError::None;
}

const char* parseErrorMessage(ParseError error)
{
    switch (error) {
        case ParseError::None:
            return "valid measurement response";
        case ParseError::EmptyBody:
            return "empty response body";
        case ParseError::MalformedJson:
            return "malformed JSON";
        case ParseError::InvalidRoot:
            return "measurement JSON root is not an object";
    }
    return "unknown measurement response error";
}

namespace {

bool boundedInt(const nlohmann::json& object, const char* field, bool& present, int& value)
{
    present = object.contains(field);
    if (!present) return true;
    const nlohmann::json& candidate = object[field];
    if (!candidate.is_number()) return false;
    const double parsed = candidate.get<double>();
    if (!std::isfinite(parsed) || parsed < std::numeric_limits<int>::min() ||
        parsed > std::numeric_limits<int>::max()) return false;
    value = static_cast<int>(parsed);
    return true;
}

bool boundedInt64(const nlohmann::json& object, const char* field, bool& present,
                  long long& value)
{
    present = object.contains(field);
    if (!present) return true;
    const nlohmann::json& candidate = object[field];
    if (candidate.is_number_unsigned()) {
        const unsigned long long parsed = candidate.get<unsigned long long>();
        if (parsed > static_cast<unsigned long long>(std::numeric_limits<long long>::max()))
            return false;
        value = static_cast<long long>(parsed);
        return true;
    }
    if (!candidate.is_number_integer()) return false;
    value = candidate.get<long long>();
    return true;
}

bool finiteDouble(const nlohmann::json& object, const char* field, bool& present,
                  double& value)
{
    present = object.contains(field);
    if (!present) return true;
    if (!object[field].is_number()) return false;
    value = object[field].get<double>();
    return std::isfinite(value);
}

}  // namespace

StatusResponseError parseStatusResponse(const std::string& body, StatusSnapshot& snapshot)
{
    snapshot = StatusSnapshot{};
    nlohmann::json measurement;
    switch (parseResponse(body, measurement)) {
        case ParseError::EmptyBody: return StatusResponseError::EmptyBody;
        case ParseError::MalformedJson: return StatusResponseError::MalformedJson;
        case ParseError::InvalidRoot: return StatusResponseError::InvalidRoot;
        case ParseError::None: break;
    }

    const nlohmann::json* info = nullptr;
    if (measurement.contains("Info")) {
        if (!measurement["Info"].is_object()) return StatusResponseError::InvalidInfo;
        info = &measurement["Info"];
    }

    if (info) {
        if (!boundedInt(*info, "PixelEventRate", snapshot.hasPixelEventRate,
                        snapshot.pixelEventRate) ||
            !boundedInt(*info, "Tdc1EventRate", snapshot.hasTdc1EventRate,
                        snapshot.tdc1EventRate) ||
            !boundedInt(*info, "Tdc2EventRate", snapshot.hasTdc2EventRate,
                        snapshot.tdc2EventRate) ||
            !boundedInt64(*info, "StartDateTime", snapshot.hasStartDateTime,
                          snapshot.startDateTime) ||
            !finiteDouble(*info, "ElapsedTime", snapshot.hasElapsedTime,
                          snapshot.elapsedTime) ||
            !finiteDouble(*info, "TimeLeft", snapshot.hasTimeLeft,
                          snapshot.timeLeft) ||
            !boundedInt(*info, "FrameCount", snapshot.hasFrameCount,
                        snapshot.frameCount) ||
            !boundedInt(*info, "DroppedFrames", snapshot.hasDroppedFrames,
                        snapshot.droppedFrames)) {
            return StatusResponseError::InvalidMetric;
        }
        if (!snapshot.hasTdc1EventRate && info->contains("TdcEventRate")) {
            if (!boundedInt(*info, "TdcEventRate", snapshot.hasTdc1EventRate,
                            snapshot.tdc1EventRate)) {
                return StatusResponseError::InvalidMetric;
            }
        }
        if (info->contains("Status")) {
            if (!(*info)["Status"].is_string()) return StatusResponseError::InvalidStatus;
            snapshot.hasStatus = true;
            snapshot.status = (*info)["Status"].get<std::string>();
        }
    }

    if (!snapshot.hasStatus && measurement.contains("Status")) {
        if (!measurement["Status"].is_string()) return StatusResponseError::InvalidStatus;
        snapshot.hasStatus = true;
        snapshot.status = measurement["Status"].get<std::string>();
    }
    return StatusResponseError::None;
}

const char* statusResponseErrorMessage(StatusResponseError error)
{
    switch (error) {
        case StatusResponseError::None: return "valid measurement status response";
        case StatusResponseError::EmptyBody: return "empty measurement response";
        case StatusResponseError::MalformedJson: return "malformed measurement JSON";
        case StatusResponseError::InvalidRoot: return "measurement JSON root is not an object";
        case StatusResponseError::InvalidInfo: return "measurement Info field is not an object";
        case StatusResponseError::InvalidStatus: return "measurement Status field is not a string";
        case StatusResponseError::InvalidMetric: return "measurement metric has an invalid value";
    }
    return "unknown measurement status response error";
}

ConfigResponseError parseConfigResponse(long statusCode, const std::string& body,
                                        nlohmann::json& config)
{
    config = nlohmann::json::object();
    if (statusCode != 200) {
        return ConfigResponseError::HttpFailure;
    }

    nlohmann::json parsed;
    switch (parseResponse(body, parsed)) {
        case ParseError::None:
            config = std::move(parsed);
            return ConfigResponseError::None;
        case ParseError::EmptyBody:
            return ConfigResponseError::EmptyBody;
        case ParseError::MalformedJson:
            return ConfigResponseError::MalformedJson;
        case ParseError::InvalidRoot:
            return ConfigResponseError::InvalidRoot;
    }
    return ConfigResponseError::MalformedJson;
}

const char* configResponseErrorMessage(ConfigResponseError error)
{
    switch (error) {
        case ConfigResponseError::None:
            return "valid measurement configuration response";
        case ConfigResponseError::HttpFailure:
            return "measurement configuration GET failed";
        case ConfigResponseError::EmptyBody:
            return "empty measurement configuration response";
        case ConfigResponseError::MalformedJson:
            return "malformed measurement configuration JSON";
        case ConfigResponseError::InvalidRoot:
            return "measurement configuration JSON root is not an object";
    }
    return "unknown measurement configuration response error";
}

}  // namespace ADTimePix3ServalMeasurement
