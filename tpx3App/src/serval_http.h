/*
 * ADTimePix3 - Serval REST/HTTP helpers (shared across translation units)
 *
 * Copyright (c) 2022 Brookhaven Science Associates, Brookhaven National Laboratory
 * Copyright (c) 2022-2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef ADTIMEPIX_SERVAL_HTTP_H
#define ADTIMEPIX_SERVAL_HTTP_H

#include <cpr/cpr.h>
#include <string>

namespace ADTimePix3ServalHttp {

constexpr int kDefaultTimeoutMs = 10000;

cpr::Response get(const std::string& url, int timeout_ms = kDefaultTimeoutMs);
cpr::Response getAuthOnly(const std::string& url, int timeout_ms = kDefaultTimeoutMs);
cpr::Response getJson(const std::string& url, int timeout_ms = kDefaultTimeoutMs);
cpr::Response putJson(const std::string& url, const std::string& body,
                      int timeout_ms = kDefaultTimeoutMs);

}  // namespace ADTimePix3ServalHttp

#endif
