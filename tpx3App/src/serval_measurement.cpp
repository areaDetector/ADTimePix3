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

bool optionalConfigInt(const nlohmann::json& object, const char* field,
                       bool& present, int& value)
{
    present = false;
    if (!object.contains(field) || object[field].is_null()) return true;
    const nlohmann::json& candidate = object[field];
    if (candidate.is_number_unsigned()) {
        const unsigned long long parsed = candidate.get<unsigned long long>();
        if (parsed > static_cast<unsigned long long>(std::numeric_limits<int>::max()))
            return false;
        value = static_cast<int>(parsed);
    } else if (candidate.is_number_integer()) {
        const long long parsed = candidate.get<long long>();
        if (parsed < std::numeric_limits<int>::min() ||
            parsed > std::numeric_limits<int>::max()) return false;
        value = static_cast<int>(parsed);
    } else {
        return false;
    }
    present = true;
    return true;
}

bool optionalConfigDouble(const nlohmann::json& object, const char* field,
                          bool& present, double& value)
{
    present = false;
    if (!object.contains(field) || object[field].is_null()) return true;
    if (!object[field].is_number()) return false;
    value = object[field].get<double>();
    if (!std::isfinite(value)) return false;
    present = true;
    return true;
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

ConfigReadbackError parseConfigReadback(long statusCode, const std::string& body,
                                        ConfigSnapshot& snapshot)
{
    snapshot = ConfigSnapshot{};
    nlohmann::json config;
    switch (parseConfigResponse(statusCode, body, config)) {
        case ConfigResponseError::HttpFailure: return ConfigReadbackError::HttpFailure;
        case ConfigResponseError::EmptyBody: return ConfigReadbackError::EmptyBody;
        case ConfigResponseError::MalformedJson: return ConfigReadbackError::MalformedJson;
        case ConfigResponseError::InvalidRoot: return ConfigReadbackError::InvalidRoot;
        case ConfigResponseError::None: break;
    }

    ConfigSnapshot candidate;
    if (config.contains("Stem") && !config["Stem"].is_null()) {
        if (!config["Stem"].is_object()) return ConfigReadbackError::InvalidStem;
        const nlohmann::json& stem = config["Stem"];
        if (stem.contains("Scan") && !stem["Scan"].is_null()) {
            if (!stem["Scan"].is_object()) return ConfigReadbackError::InvalidScan;
            const nlohmann::json& scan = stem["Scan"];
            if (!optionalConfigInt(scan, "Width", candidate.hasStemScanWidth,
                                   candidate.stemScanWidth) ||
                !optionalConfigInt(scan, "Height", candidate.hasStemScanHeight,
                                   candidate.stemScanHeight) ||
                !optionalConfigDouble(scan, "DwellTime", candidate.hasStemDwellTime,
                                      candidate.stemDwellTime)) {
                return ConfigReadbackError::InvalidMetric;
            }
        }
        if (stem.contains("VirtualDetector") && !stem["VirtualDetector"].is_null()) {
            if (!stem["VirtualDetector"].is_object())
                return ConfigReadbackError::InvalidVirtualDetector;
            const nlohmann::json& detector = stem["VirtualDetector"];
            if (!optionalConfigInt(detector, "RadiusOuter", candidate.hasStemRadiusOuter,
                                   candidate.stemRadiusOuter) ||
                !optionalConfigInt(detector, "RadiusInner", candidate.hasStemRadiusInner,
                                   candidate.stemRadiusInner)) {
                return ConfigReadbackError::InvalidMetric;
            }
        }
    }

    if (config.contains("TimeOfFlight") && !config["TimeOfFlight"].is_null()) {
        if (!config["TimeOfFlight"].is_object())
            return ConfigReadbackError::InvalidTimeOfFlight;
        const nlohmann::json& tof = config["TimeOfFlight"];
        if (tof.contains("TdcReference") && !tof["TdcReference"].is_null()) {
            if (!tof["TdcReference"].is_array())
                return ConfigReadbackError::InvalidTdcReference;
            std::string references;
            bool firstReference = true;
            for (const auto& reference : tof["TdcReference"]) {
                if (!reference.is_string()) return ConfigReadbackError::InvalidTdcReference;
                if (!firstReference) references += ',';
                references += reference.get<std::string>();
                firstReference = false;
            }
            candidate.hasTofTdcReference = true;
            candidate.tofTdcReference = std::move(references);
        }
        if (!optionalConfigDouble(tof, "Min", candidate.hasTofMin, candidate.tofMin) ||
            !optionalConfigDouble(tof, "Max", candidate.hasTofMax, candidate.tofMax)) {
            return ConfigReadbackError::InvalidMetric;
        }
    }

    snapshot = std::move(candidate);
    return ConfigReadbackError::None;
}

const char* configReadbackErrorMessage(ConfigReadbackError error)
{
    switch (error) {
        case ConfigReadbackError::None: return "valid measurement configuration readback";
        case ConfigReadbackError::HttpFailure: return "measurement configuration GET failed";
        case ConfigReadbackError::EmptyBody: return "empty measurement configuration response";
        case ConfigReadbackError::MalformedJson: return "malformed measurement configuration JSON";
        case ConfigReadbackError::InvalidRoot: return "measurement configuration root is not an object";
        case ConfigReadbackError::InvalidStem: return "measurement Stem field is not an object";
        case ConfigReadbackError::InvalidScan: return "measurement Stem.Scan field is not an object";
        case ConfigReadbackError::InvalidVirtualDetector: return "measurement Stem.VirtualDetector field is not an object";
        case ConfigReadbackError::InvalidTimeOfFlight: return "measurement TimeOfFlight field is not an object";
        case ConfigReadbackError::InvalidMetric: return "measurement configuration metric has an invalid value";
        case ConfigReadbackError::InvalidTdcReference: return "measurement TdcReference field is invalid";
    }
    return "unknown measurement configuration readback error";
}

}  // namespace ADTimePix3ServalMeasurement
