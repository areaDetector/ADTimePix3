/*
 * ADTimePix3 - immutable Serval destination allowlist
 *
 * SPDX-License-Identifier: MIT
 */

#include "destination_policy.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>

namespace ADTimePix3Destination {
namespace {

std::string trim(const std::string& value)
{
    auto first = std::find_if_not(value.begin(), value.end(),
                                  [](unsigned char c) { return std::isspace(c); });
    auto last = std::find_if_not(value.rbegin(), value.rend(),
                                 [](unsigned char c) { return std::isspace(c); }).base();
    return first < last ? std::string(first, last) : std::string();
}

bool isFileDestination(const std::string& value)
{
    return value.rfind("file:/", 0) == 0 && value.rfind("file://", 0) != 0;
}

bool supportedScheme(const std::string& value)
{
    return isFileDestination(value) || value.rfind("tcp://", 0) == 0 ||
           value.rfind("http://", 0) == 0;
}

bool safeFileDestination(const std::string& value)
{
    if (!isFileDestination(value)) return true;
    if (value.find('%') != std::string::npos ||
        value.find('?') != std::string::npos ||
        value.find('#') != std::string::npos ||
        value.find('\\') != std::string::npos) {
        return false;
    }

    const std::filesystem::path path(value.substr(5));
    if (!path.is_absolute()) return false;
    for (const auto& component : path) {
        if (component == "." || component == "..") return false;
    }
    return true;
}

}  // namespace

PolicyStatus Policy::configure(const std::string& commaSeparatedEntries)
{
    entries_.clear();
    description_ = commaSeparatedEntries;
    if (trim(commaSeparatedEntries).empty()) return PolicyStatus::NotConfigured;

    std::istringstream input(commaSeparatedEntries);
    std::string token;
    while (std::getline(input, token, ',')) {
        token = trim(token);
        if (token.empty()) {
            entries_.clear();
            return PolicyStatus::InvalidEntry;
        }
        const std::size_t wildcard = token.find('*');
        const bool prefix = wildcard != std::string::npos;
        if ((prefix && (wildcard != token.size() - 1 || token.find('*', wildcard + 1) !=
                       std::string::npos)) || !supportedScheme(token)) {
            entries_.clear();
            return PolicyStatus::InvalidEntry;
        }
        if (prefix) token.pop_back();
        if (!safeFileDestination(token) ||
            (prefix && isFileDestination(token) && token.back() != '/')) {
            entries_.clear();
            return PolicyStatus::InvalidEntry;
        }
        if (token.empty()) {
            entries_.clear();
            return PolicyStatus::InvalidEntry;
        }
        entries_.push_back(Entry{token, prefix});
    }
    return entries_.empty() ? PolicyStatus::NotConfigured : PolicyStatus::Ok;
}

bool Policy::allows(const std::string& destination) const
{
    if (!supportedScheme(destination) || !safeFileDestination(destination)) return false;
    for (const Entry& entry : entries_) {
        if ((!entry.prefix && destination == entry.value) ||
            (entry.prefix && destination.rfind(entry.value, 0) == 0)) {
            return true;
        }
    }
    return false;
}

bool Policy::configured() const
{
    return !entries_.empty();
}

bool Policy::permissive() const
{
    bool file = false;
    bool tcp = false;
    bool http = false;
    for (const Entry& entry : entries_) {
        if (!entry.prefix) continue;
        file = file || entry.value == "file:/";
        tcp = tcp || entry.value == "tcp://";
        http = http || entry.value == "http://";
    }
    return file && tcp && http;
}

const std::string& Policy::description() const
{
    return description_;
}

const char* statusMessage(PolicyStatus status)
{
    switch (status) {
    case PolicyStatus::Ok: return "ok";
    case PolicyStatus::NotConfigured: return "destination allowlist is not configured";
    case PolicyStatus::InvalidEntry: return "destination allowlist contains an invalid entry";
    }
    return "unknown destination policy error";
}

}  // namespace ADTimePix3Destination
