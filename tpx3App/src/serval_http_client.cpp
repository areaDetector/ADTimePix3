/*
 * ADTimePix3 - bounded Serval HTTP client helpers
 *
 * Copyright (c) 2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#include "serval_http.h"

namespace {

const cpr::Authentication kServalAuth{"user", "pass", cpr::AuthMode::BASIC};
const cpr::Parameters kServalParams{{"anon", "true"}, {"key", "value"}};
const cpr::Header kJsonHeader{{"Content-Type", "application/json"}};

}  // namespace

namespace ADTimePix3ServalHttp {

cpr::Response get(const std::string& url, int timeout_ms)
{
    return cpr::Get(cpr::Url{url}, kServalAuth, kServalParams,
                    cpr::Timeout{timeout_ms});
}

cpr::Response getAuthOnly(const std::string& url, int timeout_ms)
{
    return cpr::Get(cpr::Url{url}, kServalAuth, cpr::Timeout{timeout_ms});
}

cpr::Response getJson(const std::string& url, int timeout_ms)
{
    return cpr::Get(cpr::Url{url}, kJsonHeader, cpr::Timeout{timeout_ms});
}

cpr::Response putJson(const std::string& url, const std::string& body,
                      int timeout_ms)
{
    return cpr::Put(cpr::Url{url}, cpr::Body{body}, kJsonHeader,
                    cpr::Timeout{timeout_ms});
}

}  // namespace ADTimePix3ServalHttp
