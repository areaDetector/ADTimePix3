/*
 * Typed validation for Serval GET /detector/health responses.
 *
 * SPDX-License-Identifier: MIT
 */

#include "serval_health.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include <json.hpp>

namespace ADTimePix3ServalHealth {
namespace {

bool optionalFiniteNumber(const nlohmann::json& object, const char* key,
                          bool& present, double& value)
{
    present = false;
    if (!object.contains(key) || object[key].is_null()) return true;
    if (!object[key].is_number()) return false;
    value = object[key].get<double>();
    if (!std::isfinite(value)) return false;
    present = true;
    return true;
}

bool optionalNumericArray(const nlohmann::json& object, const char* key,
                          bool& present, nlohmann::json& value)
{
    present = false;
    if (!object.contains(key) || object[key].is_null()) return true;
    const nlohmann::json& candidate = object[key];
    if (!candidate.is_array()) return false;
    for (const auto& element : candidate) {
        if (!element.is_number()) return false;
        if (!std::isfinite(element.get<double>())) return false;
    }
    value = candidate;
    present = true;
    return true;
}

bool optionalBoundedInt(const nlohmann::json& object, const char* key,
                        bool& present, int& value)
{
    present = false;
    if (!object.contains(key) || object[key].is_null()) return true;
    const nlohmann::json& candidate = object[key];
    if (candidate.is_number_unsigned()) {
        const std::uint64_t parsed = candidate.get<std::uint64_t>();
        if (parsed > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) return false;
        value = static_cast<int>(parsed);
    } else if (candidate.is_number_integer()) {
        const std::int64_t parsed = candidate.get<std::int64_t>();
        if (parsed < std::numeric_limits<int>::min() ||
            parsed > std::numeric_limits<int>::max()) return false;
        value = static_cast<int>(parsed);
    } else {
        return false;
    }
    present = true;
    return true;
}

bool hasRecognizedField(const nlohmann::json& object)
{
    static const char* const fields[] = {
        "LocalTemperature", "FPGATemperature", "Fan1Speed", "Fan2Speed",
        "BiasVoltage", "Humidity", "ChipTemperatures", "VDD", "AVDD",
    };
    for (const char* field : fields) {
        if (object.contains(field)) return true;
    }
    return false;
}

ResponseError parseEntry(const nlohmann::json& entry, Snapshot& snapshot,
                         nlohmann::json& chipTemperatures,
                         nlohmann::json& vdd, nlohmann::json& avdd)
{
    if (!optionalFiniteNumber(entry, "LocalTemperature", snapshot.hasLocalTemperature,
                              snapshot.localTemperature) ||
        !optionalFiniteNumber(entry, "FPGATemperature", snapshot.hasFpgaTemperature,
                              snapshot.fpgaTemperature) ||
        !optionalFiniteNumber(entry, "Fan1Speed", snapshot.hasFan1Speed,
                              snapshot.fan1Speed) ||
        !optionalFiniteNumber(entry, "Fan2Speed", snapshot.hasFan2Speed,
                              snapshot.fan2Speed) ||
        !optionalFiniteNumber(entry, "BiasVoltage", snapshot.hasBiasVoltage,
                              snapshot.biasVoltage) ||
        !optionalBoundedInt(entry, "Humidity", snapshot.hasHumidity,
                            snapshot.humidity)) {
        return ResponseError::InvalidMetric;
    }
    if (!optionalNumericArray(entry, "ChipTemperatures", snapshot.hasChipTemperatures,
                              chipTemperatures) ||
        !optionalNumericArray(entry, "VDD", snapshot.hasVdd, vdd) ||
        !optionalNumericArray(entry, "AVDD", snapshot.hasAvdd, avdd)) {
        return ResponseError::InvalidArray;
    }
    return ResponseError::None;
}

template <typename T>
void takeFirst(bool sourcePresent, const T& source, bool& destinationPresent, T& destination)
{
    if (sourcePresent && !destinationPresent) {
        destinationPresent = true;
        destination = source;
    }
}

}  // namespace

ResponseError parseResponse(long statusCode, const std::string& body, Snapshot& snapshot)
{
    snapshot = Snapshot{};
    if (statusCode != 200) return ResponseError::HttpFailure;
    if (body.empty()) return ResponseError::EmptyBody;

    const nlohmann::json parsed = nlohmann::json::parse(body, nullptr, false);
    if (parsed.is_discarded()) return ResponseError::MalformedJson;
    Snapshot candidate;
    std::vector<const nlohmann::json*> entries;
    if (parsed.is_object()) {
        entries.push_back(&parsed);
    } else if (parsed.is_array()) {
        if (parsed.empty()) return ResponseError::EmptyHealthArray;
        for (const auto& entry : parsed) {
            if (!entry.is_object()) return ResponseError::InvalidHealthEntry;
            entries.push_back(&entry);
        }
    } else {
        return ResponseError::InvalidRoot;
    }

    bool recognized = false;
    nlohmann::json mergedChipTemperatures = nlohmann::json::array();
    nlohmann::json vddCollections = nlohmann::json::array();
    nlohmann::json avddCollections = nlohmann::json::array();
    for (const nlohmann::json* entry : entries) {
        recognized = recognized || hasRecognizedField(*entry);
        Snapshot block;
        nlohmann::json chipTemperatures;
        nlohmann::json vdd;
        nlohmann::json avdd;
        const ResponseError entryError = parseEntry(*entry, block, chipTemperatures, vdd, avdd);
        if (entryError != ResponseError::None) return entryError;

        takeFirst(block.hasLocalTemperature, block.localTemperature,
                  candidate.hasLocalTemperature, candidate.localTemperature);
        takeFirst(block.hasFpgaTemperature, block.fpgaTemperature,
                  candidate.hasFpgaTemperature, candidate.fpgaTemperature);
        takeFirst(block.hasFan1Speed, block.fan1Speed,
                  candidate.hasFan1Speed, candidate.fan1Speed);
        takeFirst(block.hasFan2Speed, block.fan2Speed,
                  candidate.hasFan2Speed, candidate.fan2Speed);
        takeFirst(block.hasBiasVoltage, block.biasVoltage,
                  candidate.hasBiasVoltage, candidate.biasVoltage);
        takeFirst(block.hasHumidity, block.humidity,
                  candidate.hasHumidity, candidate.humidity);
        if (block.hasChipTemperatures) {
            candidate.hasChipTemperatures = true;
            for (const auto& value : chipTemperatures) mergedChipTemperatures.push_back(value);
        }
        if (block.hasVdd) vddCollections.push_back(std::move(vdd));
        if (block.hasAvdd) avddCollections.push_back(std::move(avdd));
    }
    if (!recognized) return ResponseError::MissingHealthFields;

    if (candidate.hasChipTemperatures)
        candidate.chipTemperatures = mergedChipTemperatures.dump();
    if (!vddCollections.empty()) {
        candidate.hasVdd = true;
        candidate.vdd = (vddCollections.size() == 1 ? vddCollections[0] : vddCollections).dump();
    }
    if (!avddCollections.empty()) {
        candidate.hasAvdd = true;
        candidate.avdd =
            (avddCollections.size() == 1 ? avddCollections[0] : avddCollections).dump();
    }

    snapshot = std::move(candidate);
    return ResponseError::None;
}

const char* responseErrorMessage(ResponseError error)
{
    switch (error) {
        case ResponseError::None: return "valid detector health response";
        case ResponseError::HttpFailure: return "detector health GET failed";
        case ResponseError::EmptyBody: return "empty detector health response";
        case ResponseError::MalformedJson: return "malformed detector health JSON";
        case ResponseError::InvalidRoot: return "detector health root is neither an object nor an array";
        case ResponseError::EmptyHealthArray: return "detector health array is empty";
        case ResponseError::InvalidHealthEntry: return "detector health array entry is not an object";
        case ResponseError::MissingHealthFields: return "detector health response has no recognized fields";
        case ResponseError::InvalidMetric: return "detector health metric is not a finite number";
        case ResponseError::InvalidArray: return "detector health array contains an invalid value";
    }
    return "unknown detector health response error";
}

}  // namespace ADTimePix3ServalHealth
