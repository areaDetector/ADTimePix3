/*
 * ADTimePix3 - Serval configuration serialization helpers
 *
 * Copyright (c) 2022 Brookhaven Science Associates, Brookhaven National Laboratory
 * Copyright (c) 2022-2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#include "serval_config.h"

namespace ADTimePix3ServalConfig {

bool isBiasEnabledValue(int value)
{
    return value == 0 || value == 1;
}

bool setBiasEnabled(nlohmann::json& config, int value)
{
    if (!isBiasEnabledValue(value)) {
        return false;
    }

    config["BiasEnabled"] = (value != 0);
    return true;
}

}  // namespace ADTimePix3ServalConfig
