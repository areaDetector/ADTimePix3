/*
 * Typed validation for Serval GET /dashboard responses.
 *
 * SPDX-License-Identifier: MIT
 */

#include "serval_dashboard.h"

#include <cstdint>
#include <limits>

namespace ADTimePix3ServalDashboard {
namespace {

bool validOptionalNumber(const nlohmann::json& object, const char* key)
{
    return !object.contains(key) || object[key].is_null() || object[key].is_number();
}

bool validOptionalInt64(const nlohmann::json& object, const char* key)
{
    if (!object.contains(key) || object[key].is_null()) return true;
    const nlohmann::json& value = object[key];
    if (value.is_number_unsigned()) {
        return value.get<std::uint64_t>() <=
               static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    }
    return value.is_number_integer();
}

bool validOptionalBooleanInteger(const nlohmann::json& object, const char* key)
{
    if (!object.contains(key) || object[key].is_null() || object[key].is_boolean()) return true;
    const nlohmann::json& value = object[key];
    if (value.is_number_unsigned()) {
        return value.get<std::uint64_t>() <=
               static_cast<std::uint64_t>(std::numeric_limits<int>::max());
    }
    if (value.is_number_integer()) {
        const std::int64_t integer = value.get<std::int64_t>();
        return integer >= std::numeric_limits<int>::min() &&
               integer <= std::numeric_limits<int>::max();
    }
    return false;
}

}  // namespace

ResponseError parseResponse(long statusCode, const std::string& body, Snapshot& snapshot)
{
    snapshot = Snapshot{};
    if (statusCode != 200) return ResponseError::HttpFailure;
    if (body.empty()) return ResponseError::EmptyBody;

    nlohmann::json parsed = nlohmann::json::parse(body, nullptr, false);
    if (parsed.is_discarded()) return ResponseError::MalformedJson;
    if (!parsed.is_object()) return ResponseError::InvalidRoot;
    if (!parsed.contains("Server")) return ResponseError::MissingServer;
    if (!parsed["Server"].is_object()) return ResponseError::InvalidServer;
    if (!parsed.contains("Detector")) return ResponseError::MissingDetector;

    const nlohmann::json& detector = parsed["Detector"];
    if (!detector.is_null() && !detector.is_object()) return ResponseError::InvalidDetector;
    if (detector.is_object()) {
        if (!detector.contains("DetectorType") || !detector["DetectorType"].is_string() ||
            detector["DetectorType"].get<std::string>().empty()) {
            return ResponseError::InvalidDetectorType;
        }
        snapshot.detectorConnected = true;
        snapshot.detectorType = detector["DetectorType"].get<std::string>();
    }

    if (parsed.contains("Measurement") && !parsed["Measurement"].is_null() &&
        !parsed["Measurement"].is_object()) {
        return ResponseError::InvalidMeasurement;
    }

    const nlohmann::json& server = parsed["Server"];
    if (server.contains("SoftwareVersion") && !server["SoftwareVersion"].is_string())
        return ResponseError::InvalidSoftwareVersion;
    if (server.contains("SoftwareTimestamp") && !server["SoftwareTimestamp"].is_string())
        return ResponseError::InvalidSoftwareTimestamp;
    if (server.contains("DiskSpace")) {
        if (!server["DiskSpace"].is_array()) return ResponseError::InvalidDiskSpace;
        for (const auto& disk : server["DiskSpace"]) {
            if (!disk.is_object()) return ResponseError::InvalidDiskEntry;
            if (!validOptionalInt64(disk, "FreeSpace") ||
                !validOptionalNumber(disk, "WriteSpeed") ||
                !validOptionalInt64(disk, "LowerLimit") ||
                !validOptionalBooleanInteger(disk, "DiskLimitReached")) {
                return ResponseError::InvalidDiskValue;
            }
        }
    }

    snapshot.response = std::move(parsed);
    return ResponseError::None;
}

const char* responseErrorMessage(ResponseError error)
{
    switch (error) {
        case ResponseError::None: return "valid dashboard response";
        case ResponseError::HttpFailure: return "dashboard GET failed";
        case ResponseError::EmptyBody: return "empty dashboard response";
        case ResponseError::MalformedJson: return "malformed dashboard JSON";
        case ResponseError::InvalidRoot: return "dashboard root is not an object";
        case ResponseError::MissingServer: return "dashboard Server field is missing";
        case ResponseError::InvalidServer: return "dashboard Server field is not an object";
        case ResponseError::MissingDetector: return "dashboard Detector field is missing";
        case ResponseError::InvalidDetector: return "dashboard Detector field is neither null nor an object";
        case ResponseError::InvalidDetectorType: return "dashboard detector type is missing or invalid";
        case ResponseError::InvalidMeasurement: return "dashboard Measurement field is invalid";
        case ResponseError::InvalidSoftwareVersion: return "dashboard software version is invalid";
        case ResponseError::InvalidSoftwareTimestamp: return "dashboard software timestamp is invalid";
        case ResponseError::InvalidDiskSpace: return "dashboard DiskSpace field is not an array";
        case ResponseError::InvalidDiskEntry: return "dashboard DiskSpace entry is not an object";
        case ResponseError::InvalidDiskValue: return "dashboard DiskSpace value has an invalid type";
    }
    return "unknown dashboard response error";
}

}  // namespace ADTimePix3ServalDashboard
