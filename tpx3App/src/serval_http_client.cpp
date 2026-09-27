/*
 * ADTimePix3 - bounded Serval HTTP client helpers
 *
 * Copyright (c) 2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#include "serval_http.h"

#include <cstdlib>

namespace {

struct ServalCredentials {
    std::string username;
    std::string password;
    bool configured = false;
};

const ServalCredentials& servalCredentials()
{
    static const ServalCredentials credentials = [] {
        ServalCredentials value;
        const char* username = std::getenv("ADTIMEPIX_SERVAL_USERNAME");
        const char* password = std::getenv("ADTIMEPIX_SERVAL_PASSWORD");
        if (username != nullptr && password != nullptr &&
            username[0] != '\0' && password[0] != '\0') {
            value.username = username;
            value.password = password;
            value.configured = true;
        }
        return value;
    }();
    return credentials;
}
const cpr::Parameters kServalParams{{"anon", "true"}, {"key", "value"}};
const cpr::Header kJsonHeader{{"Content-Type", "application/json"}};

}  // namespace

namespace ADTimePix3ServalHttp {

cpr::Response get(const std::string& url, int timeout_ms)
{
    const ServalCredentials& credentials = servalCredentials();
    if (credentials.configured) {
        return cpr::Get(cpr::Url{url},
                        cpr::Authentication{credentials.username, credentials.password,
                                            cpr::AuthMode::BASIC},
                        kServalParams, cpr::Timeout{timeout_ms});
    }
    return cpr::Get(cpr::Url{url}, kServalParams, cpr::Timeout{timeout_ms});
}

cpr::Response getAuthOnly(const std::string& url, int timeout_ms)
{
    const ServalCredentials& credentials = servalCredentials();
    if (credentials.configured) {
        return cpr::Get(cpr::Url{url},
                        cpr::Authentication{credentials.username, credentials.password,
                                            cpr::AuthMode::BASIC},
                        cpr::Timeout{timeout_ms});
    }
    return cpr::Get(cpr::Url{url}, cpr::Timeout{timeout_ms});
}

cpr::Response getJson(const std::string& url, int timeout_ms)
{
    const ServalCredentials& credentials = servalCredentials();
    if (credentials.configured) {
        return cpr::Get(cpr::Url{url}, kJsonHeader,
                        cpr::Authentication{credentials.username, credentials.password,
                                            cpr::AuthMode::BASIC},
                        cpr::Timeout{timeout_ms});
    }
    return cpr::Get(cpr::Url{url}, kJsonHeader, cpr::Timeout{timeout_ms});
}


cpr::Response putJson(const std::string& url, const std::string& body,
                      int timeout_ms)
{
    const ServalCredentials& credentials = servalCredentials();
    if (credentials.configured) {
        return cpr::Put(cpr::Url{url}, cpr::Body{body}, kJsonHeader,
                        cpr::Authentication{credentials.username, credentials.password,
                                            cpr::AuthMode::BASIC},
                        cpr::Timeout{timeout_ms});
    }
    return cpr::Put(cpr::Url{url}, cpr::Body{body}, kJsonHeader,
                    cpr::Timeout{timeout_ms});
}

void configureSessionAuthentication(cpr::Session& session)
{
    const ServalCredentials& credentials = servalCredentials();
    if (credentials.configured) {
        session.SetOption(cpr::Authentication{credentials.username, credentials.password,
                                              cpr::AuthMode::BASIC});
    }
}

bool credentialsConfigured()
{
    return servalCredentials().configured;
}

}  // namespace ADTimePix3ServalHttp
