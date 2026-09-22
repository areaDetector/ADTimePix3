/*
 * ADTimePix3 - Serval destination response validation
 *
 * Copyright (c) 2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#include "serval_destination.h"

#include <json.hpp>

namespace ADTimePix3ServalDestination {

namespace {

bool destinationNotConfigured(const std::string& body)
{
    return body.find("Destination is not set") != std::string::npos;
}

}  // namespace

ResponseError parseResponse(long httpStatus, const std::string& body,
                            Snapshot& snapshot)
{
    snapshot = Snapshot{};
    if (httpStatus != 200) {
        return destinationNotConfigured(body) ? ResponseError::NotConfigured
                                               : ResponseError::HttpFailure;
    }
    if (body.empty()) return ResponseError::EmptyBody;

    const nlohmann::json parsed = nlohmann::json::parse(body, nullptr, false);
    if (parsed.is_discarded()) return ResponseError::MalformedJson;
    if (!parsed.is_object()) return ResponseError::InvalidRoot;

    const nlohmann::json* destination = &parsed;
    if (parsed.contains("Destination")) {
        if (!parsed["Destination"].is_object()) {
            return ResponseError::InvalidDestination;
        }
        destination = &parsed["Destination"];
    }

    if (destination->contains("Raw")) {
        if (!(*destination)["Raw"].is_array()) {
            return ResponseError::InvalidRawChannels;
        }
        snapshot.rawChannels = (*destination)["Raw"].size();
    }
    if (destination->contains("Image")) {
        if (!(*destination)["Image"].is_array()) {
            return ResponseError::InvalidImageChannels;
        }
        snapshot.imageChannels = (*destination)["Image"].size();
    }
    if (destination->contains("Preview")) {
        const nlohmann::json& preview = (*destination)["Preview"];
        if (!preview.is_object()) return ResponseError::InvalidPreview;
        if (preview.contains("ImageChannels")) {
            if (!preview["ImageChannels"].is_array()) {
                return ResponseError::InvalidPreviewImageChannels;
            }
            snapshot.previewImageChannels = preview["ImageChannels"].size();
        }
        if (preview.contains("HistogramChannels")) {
            if (!preview["HistogramChannels"].is_array()) {
                return ResponseError::InvalidPreviewHistogramChannels;
            }
            snapshot.previewHistogramChannels = preview["HistogramChannels"].size();
        }
    }

    return ResponseError::None;
}

const char* responseErrorMessage(ResponseError error)
{
    switch (error) {
        case ResponseError::None:
            return "valid server destination response";
        case ResponseError::NotConfigured:
            return "Serval destination is not configured";
        case ResponseError::HttpFailure:
            return "failed to read Serval destination";
        case ResponseError::EmptyBody:
            return "empty server destination response";
        case ResponseError::MalformedJson:
            return "malformed server destination JSON";
        case ResponseError::InvalidRoot:
            return "server destination JSON root is not an object";
        case ResponseError::InvalidDestination:
            return "server destination wrapper is not an object";
        case ResponseError::InvalidRawChannels:
            return "server destination Raw field is not an array";
        case ResponseError::InvalidImageChannels:
            return "server destination Image field is not an array";
        case ResponseError::InvalidPreview:
            return "server destination Preview field is not an object";
        case ResponseError::InvalidPreviewImageChannels:
            return "server destination preview ImageChannels field is not an array";
        case ResponseError::InvalidPreviewHistogramChannels:
            return "server destination preview HistogramChannels field is not an array";
    }
    return "unknown server destination response error";
}

}  // namespace ADTimePix3ServalDestination
