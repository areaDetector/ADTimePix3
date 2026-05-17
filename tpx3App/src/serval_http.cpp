/*
 * ADTimePix3 - Serval REST/HTTP (dashboard, config, DACs, file writer, init)
 *
 * Copyright (c) 2022 Brookhaven Science Associates, Brookhaven National Laboratory
 * Copyright (c) 2022-2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#include "ADTimePix.h"
#include "ADTimePixLog.h"
#include "serval_http.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#include <epicsThread.h>
#include <epicsTime.h>

#include <json.hpp>

using json = nlohmann::json;
using std::string;

extern const char* driverName;

namespace {

constexpr size_t kPixelConfigBytes = 65536;
const cpr::Authentication kServalAuth{"user", "pass", cpr::AuthMode::BASIC};
const cpr::Parameters kServalParams{{"anon", "true"}, {"key", "value"}};
const cpr::Header kJsonHeader{{"Content-Type", "application/json"}};

bool decodeBase64(const std::string& in, std::vector<uint8_t>& out) {
    static const char kChars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    out.clear();
    int val = 0;
    int valb = -8;
    for (unsigned char c : in) {
        if (std::isspace(static_cast<unsigned char>(c))) continue;
        if (c == '=') break;
        const char* p = std::strchr(kChars, static_cast<char>(c));
        if (!p) return false;
        val = (val << 6) + static_cast<int>(p - kChars);
        valb += 6;
        if (valb >= 0) {
            out.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return true;
}

}  // namespace

namespace ADTimePix3ServalHttp {

cpr::Response get(const std::string& url) {
    return cpr::Get(cpr::Url{url}, kServalAuth, kServalParams);
}

cpr::Response get(const std::string& url, int timeout_ms) {
    return cpr::Get(cpr::Url{url}, kServalAuth, kServalParams, cpr::Timeout{timeout_ms});
}

cpr::Response getAuthOnly(const std::string& url) {
    return cpr::Get(cpr::Url{url}, kServalAuth);
}

cpr::Response getJson(const std::string& url, int timeout_ms) {
    return cpr::Get(cpr::Url{url}, kJsonHeader, cpr::Timeout{timeout_ms});
}

cpr::Response putJson(const std::string& url, const std::string& body) {
    return cpr::Put(cpr::Url{url}, cpr::Body{body}, kJsonHeader);
}

cpr::Response putJson(const std::string& url, const std::string& body, int timeout_ms) {
    return cpr::Put(cpr::Url{url}, cpr::Body{body}, kJsonHeader, cpr::Timeout{timeout_ms});
}

}  // namespace ADTimePix3ServalHttp

static string strip_quotes(string str) {
    if (str.length() > 1)
        return str.substr(1, str.length() - 2);
    return str;
}

cpr::Response ADTimePix::servalHttpGetAuthOnly(const std::string& url) {
    return ADTimePix3ServalHttp::getAuthOnly(url);
}

std::string ADTimePix::trimHttpBodyForLog(const std::string& body, size_t maxLen) {
    std::string out;
    const size_t n = std::min(body.size(), maxLen);
    out.reserve(n + 4);
    for (size_t i = 0; i < n; ++i) {
        unsigned char c = static_cast<unsigned char>(body[i]);
        if (c < 32 || c == 127)
            out += ' ';
        else
            out += static_cast<char>(c);
    }
    if (body.size() > maxLen) out += "...";
    return out;
}

void ADTimePix::logHttpFailure(const char* context, const char* method, const std::string& endpoint, long status,
                               const std::string& body) {
    ERR_ARGS("%s: %s %s HTTP %ld: %s", context ? context : "HTTP", method ? method : "?", endpoint.c_str(), status,
             trimHttpBodyForLog(body).c_str());
}

void ADTimePix::logHttpFailure(const std::string& context, const char* method, const std::string& endpoint,
                               long status, const std::string& body) {
    logHttpFailure(context.c_str(), method, endpoint, status, body);
}

void ADTimePix::logHttpWarning(const char* context, const char* method, const std::string& endpoint, long status,
                               const std::string& body) {
    WARN_ARGS("%s: %s %s HTTP %ld: %s", context ? context : "HTTP", method ? method : "?", endpoint.c_str(), status,
              trimHttpBodyForLog(body).c_str());
}

asynStatus ADTimePix::initialServerCheckConnection(){
    bool connected = false;


    // Implement connecting to the camera here: check welcome URL
    // Usually the vendor provides examples of how to do this with the library/SDK
    // Use GET request and compare if URI status response code is 200.
    cpr::Response r = ADTimePix3ServalHttp::get(this->serverURL);
    // printf("Status code: %li\n", r.status_code);
    // printf("Header:\n");
    // for (const pair<string, string>& kv : r.header) {
    //     printf("\t%s:%s\n",kv.first.c_str(),kv.second.c_str());
    // }
    // printf("Text: %s\n", r.text.c_str());
    setIntegerParam(ADTimePixHttpCode, r.status_code);

    if(r.status_code == 200) {
        connected = true;
        setIntegerParam(ADTimePixServalConnected,1);
        printf("\n\nCONNECTED to Welcome URI! (Serval running), http_code = %li\n", r.status_code);

        // Check if detector is connected to serval from dashboard URL
        // Both serval, and connection to Tpx3 detector must be successful
        std::string dashboard;

        dashboard = this->serverURL + std::string("/dashboard");
        printf("ServerURL/dashboard =%s\n", dashboard.c_str());
        r = ADTimePix3ServalHttp::get(dashboard);

        printf("Status code: %li\n", r.status_code);
        printf("Text:\n %s\n", r.text.c_str());

        if (r.status_code != 200) {
            logHttpFailure("initialServerCheckConnection Dashboard", "GET", dashboard, (long)r.status_code, r.text);
            setIntegerParam(ADTimePixDetConnected, 0);
            connected = false;
        } else {
            try {
                json dashboard_j = json::parse(r.text.c_str());

                // strip double quote from beginning and end of string
                std::string API_Ver, API_TS;
                API_Ver = strip_quotes(dashboard_j["Server"]["SoftwareVersion"].dump()).c_str();
                API_TS = strip_quotes(dashboard_j["Server"]["SoftwareTimestamp"].dump()).c_str();

                setStringParam(ADSDKVersion, API_Ver.c_str());
                setStringParam(ADManufacturer, "ASI");
                setStringParam(ADTimePixFWTimeStamp, API_TS.c_str());

                std::string Detector, DetType;
                Detector = dashboard_j["Detector"].dump().c_str();

                if (strcmp(Detector.c_str(), "null")) {
                    DetType = strip_quotes(dashboard_j["Detector"]["DetectorType"].dump()).c_str();
                    setStringParam(ADTimePixDetType, DetType.c_str());
                    setStringParam(ADModel, DetType.c_str());
                    setIntegerParam(ADTimePixDetConnected, 1);
                    printf("Detector CONNECTED, Detector=%s, %d\n", Detector.c_str(),
                           strcmp(Detector.c_str(), "null"));
                } else {
                    printf("Detector NOT CONNECTED, Detector=%s\n", Detector.c_str());
                    setStringParam(ADTimePixDetType, "null");
                    setIntegerParam(ADTimePixDetConnected, 0);
                    connected = false;
                }
            } catch (const std::exception& e) {
                ERR_ARGS("Dashboard JSON parse failed: %s", e.what());
                setIntegerParam(ADTimePixDetConnected, 0);
                connected = false;
            }
        }
    } else {
        setIntegerParam(ADTimePixServalConnected,0);
        setIntegerParam(ADTimePixDetConnected,0);
    }

    callParamCallbacks();
    if(connected) return asynSuccess;
    else{
        ERR_ARGS("ERROR: Failed to connect to server %s",this->serverURL.c_str());
        return asynError;
    }
}

/**
 * Lightweight connection check: updates ServalConnected_RBV, DetConnected_RBV, and ADStatusMessage.
 * Uses GET /dashboard as the single source of truth so ServalConnected means "SERVAL reachable"
 * even when the root URL returns 404/302. Does not call getServer() or full getDashboard().
 * Used by connection poll and RefreshConnection PV.
 * @return asynSuccess if SERVAL and detector are connected, asynError otherwise
 */
asynStatus ADTimePix::checkConnection(){
    bool servalOk = false;
    bool detOk = false;

    std::string dashboard = this->serverURL + std::string("/dashboard");
    cpr::Response r = ADTimePix3ServalHttp::get(dashboard, 5000);
    setIntegerParam(ADTimePixHttpCode, r.status_code);

    if (r.status_code == 200) {
        servalOk = true;
        setIntegerParam(ADTimePixServalConnected, 1);
        try {
            json dashboard_j = json::parse(r.text.c_str());
            std::string Detector = dashboard_j["Detector"].dump();
            if (strcmp(Detector.c_str(), "null") != 0) {
                detOk = true;
                setIntegerParam(ADTimePixDetConnected, 1);
                std::string DetType = strip_quotes(dashboard_j["Detector"]["DetectorType"].dump());
                setStringParam(ADTimePixDetType, DetType.c_str());
                setStringParam(ADModel, DetType.c_str());
            } else {
                setIntegerParam(ADTimePixDetConnected, 0);
                setStringParam(ADTimePixDetType, "null");
            }
        } catch (...) {
            setIntegerParam(ADTimePixDetConnected, 0);
            setStringParam(ADTimePixDetType, "null");
        }
    } else {
        setIntegerParam(ADTimePixServalConnected, 0);
        setIntegerParam(ADTimePixDetConnected, 0);
        setStringParam(ADTimePixDetType, "null");
    }

    if (servalOk && detOk) {
        setStringParam(ADStatusMessage, "OK");
    } else {
        setStringParam(ADStatusMessage, "SERVAL or detector disconnected");
    }
    callParamCallbacks();
    return (servalOk && detOk) ? asynSuccess : asynError;
}

void ADTimePix::connectionPollThreadC(void* pPvt) {
    ADTimePix* pDriver = (ADTimePix*)pPvt;
    if (pDriver) pDriver->connectionPollThread();
}

void ADTimePix::connectionPollThread() {
    while (connectionPollEnable_) {
        epicsEventWaitWithTimeout(connectionPollEvent_, connectionPollPeriodSec_);
        if (!connectionPollEnable_) break;
        if (connectionPollSkipOnce_) {
            connectionPollSkipOnce_ = 0;
            continue;
        }
        (void)checkConnection();
        int servalNow = 0, detNow = 0;
        getIntegerParam(ADTimePixServalConnected, &servalNow);
        getIntegerParam(ADTimePixDetConnected, &detNow);
        // On reconnect: push current PV config to SERVAL (single source of truth), then refresh from SERVAL
        if (servalNow && detNow && (lastServalConnected_ == 0 || lastDetConnected_ == 0)) {
            (void)fileWriter();       // Push channel config (Raw/Img/PrvImg/PrvHst) from PVs to SERVAL
            (void)initAcquisition();  // Push detector config (TriggerMode, NumImages, etc.) from PVs to SERVAL
            (void)getServer();        // Refresh channel state from SERVAL
        }
        lastServalConnected_ = servalNow;
        lastDetConnected_ = detNow;
    }
}

/**
 * Function that updates PV values of dashboard with camera information
 * 
 * @return: status
 */
asynStatus ADTimePix::getDashboard(){
    asynStatus status = asynSuccess;
    FLOW("Collecting detector information");
    std::string dashboard;

    // Use the vendor library to collect information about the connected camera here, and set the appropriate PVs
    // Make sure you check if camera is connected before calling on it for information

    //setStringParam(ADManufacturer,        _____________);
    //setStringParam(ADSerialNumber,        _____________);
    //setStringParam(ADFirmwareVersion,     "Server"->"SoftwareVersion" : "2.3.6",);
    //setStringParam(ADModel,               _____________);
    /*
        "Server" : {
           "SoftwareVersion" : "2.3.6",
           "DiskSpace" : [ ],
           "SoftwareTimestamp" : "2022/01/05 11:07",
           "Notifications" : [ ]
        },
        "Measurement" : null,
        "Detector" : null
    */

    dashboard = this->serverURL + std::string("/dashboard");
    // printf("ServerURL/dashboard=%s\n", dashboard.c_str());
    cpr::Response r = ADTimePix3ServalHttp::get(dashboard, 5000);

    if(r.status_code == 200) {
        setIntegerParam(ADTimePixServalConnected, 1);  // SERVAL reachable (dashboard responded)
        try {
            json dashboard_j = json::parse(r.text.c_str());
            // Detector status: consistent with checkConnection()
            std::string Detector = dashboard_j["Detector"].dump();
            if (strcmp(Detector.c_str(), "null") != 0) {
                setIntegerParam(ADTimePixDetConnected, 1);
                std::string DetType = strip_quotes(dashboard_j["Detector"]["DetectorType"].dump());
                setStringParam(ADTimePixDetType, DetType.c_str());
                setStringParam(ADModel, DetType.c_str());
            } else {
                setIntegerParam(ADTimePixDetConnected, 0);
                setStringParam(ADTimePixDetType, "null");
            }
            // DiskSpace is an empty array until raw file writing selected, and acquisition starts.
            // SERVAL may omit or null individual fields (e.g. WriteSpeed) — avoid .get on null (json type_error.302).
            if (!dashboard_j["Server"]["DiskSpace"].empty()) {
                const json& ds0 = dashboard_j["Server"]["DiskSpace"][0];
                if (ds0.contains("FreeSpace") && ds0["FreeSpace"].is_number_integer())
                    setInteger64Param(ADTimePixFreeSpace, ds0["FreeSpace"].get<long>());
                if (ds0.contains("WriteSpeed") && ds0["WriteSpeed"].is_number())
                    setDoubleParam(ADTimePixWriteSpeed, ds0["WriteSpeed"].get<double>());
                if (ds0.contains("LowerLimit") && ds0["LowerLimit"].is_number_integer())
                    setInteger64Param(ADTimePixLowerLimit, ds0["LowerLimit"].get<long>());
                if (ds0.contains("DiskLimitReached") && !ds0["DiskLimitReached"].is_null()) {
                    const json& dl = ds0["DiskLimitReached"];
                    if (dl.is_boolean())
                        setIntegerParam(ADTimePixLLimReached, dl.get<bool>() ? 1 : 0);
                    else if (dl.is_number_integer())
                        setIntegerParam(ADTimePixLLimReached, dl.get<int>());
                }
            }
        } catch (...) {
            setIntegerParam(ADTimePixDetConnected, 0);
            setStringParam(ADTimePixDetType, "null");
        }
    } else { // Serval not running
        setIntegerParam(ADTimePixServalConnected, 0);
        setIntegerParam(ADTimePixDetConnected, 0);
        setStringParam(ADTimePixDetType, "null");
    }
    return status;
}

asynStatus ADTimePix::getHealth(){
    asynStatus status = asynSuccess;
    FLOW("Checking Health");
    std::string health;

    health = this->serverURL + std::string("/detector/health");
    // printf("Health, %s\n", health.c_str());
    cpr::Response r = ADTimePix3ServalHttp::get(health, 5000);

    if (r.status_code != 200) {
        logHttpFailure("getHealth", "GET", health, (long)r.status_code, r.text);
        return asynError;
    }
    json health_j;
    try {
        health_j = json::parse(r.text.c_str());
    } catch (const std::exception& e) {
        ERR_ARGS("Health JSON parse failed: %s", e.what());
        return asynError;
    }
    // printf("Text JSON: %s\n", health_j.dump(3,' ', true).c_str());
    // printf("%lf\n", health_j["ChipTemperatures"].get<double>());
    // printf("Chip Temperatures %s, %s\n", health_j["ChipTemperatures"].dump().c_str(), health_j["VDD"][1].dump().c_str());
    
    auto hjD = [](const json& j, double def) -> double { return j.is_number() ? j.get<double>() : def; };
    setDoubleParam(ADTimePixLocalTemp, hjD(health_j["LocalTemperature"], 0.0));
    setDoubleParam(ADTimePixFPGATemp, hjD(health_j["FPGATemperature"], 0.0));
    setDoubleParam(ADTimePixFan1Speed, hjD(health_j["Fan1Speed"], 0.0));
    setDoubleParam(ADTimePixFan2Speed, hjD(health_j["Fan2Speed"], 0.0));
    setDoubleParam(ADTimePixBiasVoltage, hjD(health_j["BiasVoltage"], 0.0));

    setStringParam(ADTimePixChipTemperature, health_j["ChipTemperatures"].dump().c_str());
    setStringParam(ADTimePixVDD, health_j["VDD"].dump().c_str());
    setStringParam(ADTimePixAVDD, health_j["AVDD"].dump().c_str());

    callParamCallbacks();

    return status;
}

/**
 * Write detector Layout
 *
 * There are eight possible orientations of the detector
 * The function rotates detector consistent with Serval URL.
 *    /detector/Layout/DetectorOrientation: only reports orientation
 * Use: GET <server>/detector/layout/rotate?reset=true
 *    flip: horizontal, vertical
 *    direction: left, right, 180/half
 *    reset: Not applicable
 * Example: /detector/layout/rotate?reset=true&direction=right&flip=horizontal
 *
 * @return: status
 */
asynStatus ADTimePix::rotateLayout(){
    asynStatus status = asynSuccess;
    int intNum;
    std::string API_Ver;

    std::string layout_url = this->serverURL + std::string("/detector/layout/rotate?reset=true");   // Serval 4.1.1

    getIntegerParam(ADTimePixDetectorOrientation, &intNum);
    json detectorOrientation;
    detectorOrientation[0] = "UP";
    detectorOrientation[1] = "RIGHT";
    detectorOrientation[2] = "DOWN";
    detectorOrientation[3] = "LEFT";
    detectorOrientation[4] = "UP_MIRRORED";
    detectorOrientation[5] = "RIGHT_MIRRORED";
    detectorOrientation[6] = "DOWN_MIRRORED";
    detectorOrientation[7] = "LEFT_MIRRORED";

//    json detOrientation_j;
//    detOrientation_j["DetectorOrientation"] = detectorOrientation[intNum];
//    std::string json_data = detOrientation_j.dump(3,' ', true).c_str();
//    std::string json_data = "{\"DetectorOrientation\":\"" + std::string(detectorOrientation[intNum]) + "\"}";
//    std::string json_data = "{\"" + std::string("DetectorOrientation") + "\":" + std::string(detectorOrientation[intNum]) + "}";

    getStringParam(ADSDKVersion, API_Ver);
    switch (intNum) {
        case 0:
            layout_url += std::string("");
            break;
        case 1:
            layout_url += std::string("&direction=right");
            break;
        case 2:
            if (API_Ver[0] == '3') {
                layout_url += std::string("&direction=180");
            } else { // Serval 4.x.x
                layout_url += std::string("&direction=half");
            }
            break;
        case 3:
            layout_url += std::string("&direction=left");
            break;
        case 4:
            layout_url += std::string("&flip=horizontal");
            break;
        case 5:
            layout_url += std::string("&direction=right&flip=horizontal");
            break;
        case 6:
            layout_url += std::string("&flip=vertical");
            break;
        case 7:
            layout_url += std::string("&direction=right&flip=vertical");
            break;
        default:
            layout_url += std::string("");
            break;
    }

//    printf("Layout,layout_url=%s\n", layout_url.c_str());

    cpr::Response r = ADTimePix3ServalHttp::getAuthOnly(layout_url);

//    json layout_j = json::parse(r.text.c_str());
//    printf("layout=%s\n",layout_j.dump(3,' ', true).c_str());

//    cpr::Response r = cpr::Put(
//        cpr::Url{layout_url},
//        cpr::Body{json_data},
//        cpr::Header{{"Content-Type", "application/json"}}
//    );

    if (r.status_code != 200) {
        logHttpFailure("rotateLayout", "GET", layout_url, (long)r.status_code, r.text);
        status = asynError;
    }

    return status;
}

/**
 * Write value on individual DAC
 * The write for dacs of a chip is atomic: all dac values for specific chip must be written together
 *    - Read dacs for specific chip from serval
 *    - update specific dac value
 *    - write dacs for that chip to serval
 *
 * chip:      Chip number (int)
 * dac:       Dac name (string)
 * value:     Value to be updated on the dac (int)
 *
 * @return: status
 */
asynStatus ADTimePix::writeDac(int chip, const std::string& dac, int value) {
    asynStatus status = asynSuccess;

    std::string dac_url = this->serverURL + std::string("/detector/chips/") + std::to_string(chip) + std::string("/dacs/");
    cpr::Response r = ADTimePix3ServalHttp::get(dac_url);

    if (r.status_code != 200) {
        logHttpFailure(std::string("writeDac GET chip ") + std::to_string(chip), "GET", dac_url,
                       (long)r.status_code, r.text);
        return asynError;
    }

    json dacsRead_j;
    try {
        dacsRead_j = json::parse(r.text.c_str());
    } catch (const std::exception& e) {
        ERR_ARGS("writeDac GET: JSON parse failed: %s", e.what());
        return asynError;
    }
    dacsRead_j[dac] = value;
    // printf("dacs=%s\n",dacsRead_j.dump(3,' ', true).c_str());
    std::string json_data = dacsRead_j.dump(3,' ', true).c_str();

    r = ADTimePix3ServalHttp::putJson(dac_url, json_data);

    if (r.status_code != 200) {
        logHttpFailure("writeDac PUT", "PUT", dac_url, (long)r.status_code, r.text);
        status = asynError;
    }

    return status;
}

namespace {

/**
 * Serval 4: GET /detector returns Health as an array (often one entry per board / connection).
 * Each Health[i].ChipTemperatures only lists chips on that board (e.g. four temps per board).
 * Indexing Health[0].ChipTemperatures[chip] with global chip 4..7 yields null / wrong slot and
 * nlohmann throws type_error.302 on .get<int>(). Walk Health[] and map global chip index.
 */
int chipTemperatureFromHealthV4(const json& detector, int chip) {
    if (chip < 0 || !detector.contains("Health") || !detector["Health"].is_array()) return 0;
    size_t offset = 0;
    for (size_t hi = 0; hi < detector["Health"].size(); ++hi) {
        const json& block = detector["Health"][hi];
        if (!block.is_object() || !block.contains("ChipTemperatures") ||
            !block["ChipTemperatures"].is_array())
            continue;
        const json& ct = block["ChipTemperatures"];
        const size_t n = ct.size();
        if ((size_t)chip < offset + n) {
            const json& tj = ct[chip - offset];
            if (tj.is_number_integer()) return tj.get<int>();
            if (tj.is_number()) return static_cast<int>(tj.get<double>());
            return 0;
        }
        offset += n;
    }
    return 0;
}

/** Serval 3: single Health object; ChipTemperatures may be a flat array (one per chip). */
int chipTemperatureFromHealthV3(const json& detector, int chip) {
    if (chip < 0 || !detector.contains("Health") || !detector["Health"].is_object()) return 0;
    const json& ct = detector["Health"]["ChipTemperatures"];
    if (!ct.is_array() || (size_t)chip >= ct.size()) return 0;
    const json& tj = ct[(size_t)chip];
    if (tj.is_number_integer()) return tj.get<int>();
    if (tj.is_number()) return static_cast<int>(tj.get<double>());
    return 0;
}

} // namespace

/**
 * Fetch DACs values and update PVs readback
 *
 * data:      Json Data (json)
 * chip:      Number of the chip (int)
 *
 * @return: status
*/

asynStatus ADTimePix::fetchDacs(json& data, int chip) {
    std::string API_Ver;

    getStringParam(ADSDKVersion, API_Ver);

    setIntegerParam(chip, ADTimePixCP_PLL,             data["Chips"][chip]["DACs"]["Ibias_CP_PLL"].get<int>());
    setIntegerParam(chip, ADTimePixDiscS1OFF,          data["Chips"][chip]["DACs"]["Ibias_DiscS1_OFF"].get<int>());
    setIntegerParam(chip, ADTimePixDiscS1ON,           data["Chips"][chip]["DACs"]["Ibias_DiscS1_ON"].get<int>());
    setIntegerParam(chip, ADTimePixDiscS2OFF,          data["Chips"][chip]["DACs"]["Ibias_DiscS2_OFF"].get<int>());
    setIntegerParam(chip, ADTimePixDiscS2ON,           data["Chips"][chip]["DACs"]["Ibias_DiscS2_ON"].get<int>());
    setIntegerParam(chip, ADTimePixIkrum,              data["Chips"][chip]["DACs"]["Ibias_Ikrum"].get<int>());
    setIntegerParam(chip, ADTimePixPixelDAC,           data["Chips"][chip]["DACs"]["Ibias_PixelDAC"].get<int>());
    setIntegerParam(chip, ADTimePixPreampOFF,          data["Chips"][chip]["DACs"]["Ibias_Preamp_OFF"].get<int>());
    setIntegerParam(chip, ADTimePixPreampON,           data["Chips"][chip]["DACs"]["Ibias_Preamp_ON"].get<int>());
    setIntegerParam(chip, ADTimePixTPbufferIn,         data["Chips"][chip]["DACs"]["Ibias_TPbufferIn"].get<int>());
    setIntegerParam(chip, ADTimePixTPbufferOut,        data["Chips"][chip]["DACs"]["Ibias_TPbufferOut"].get<int>());
    setIntegerParam(chip, ADTimePixPLL_Vcntrl,         data["Chips"][chip]["DACs"]["PLL_Vcntrl"].get<int>());
    setIntegerParam(chip, ADTimePixVPreampNCAS,        data["Chips"][chip]["DACs"]["VPreamp_NCAS"].get<int>());
    setIntegerParam(chip, ADTimePixVTPcoarse,          data["Chips"][chip]["DACs"]["VTP_coarse"].get<int>());
    setIntegerParam(chip, ADTimePixVTPfine,            data["Chips"][chip]["DACs"]["VTP_fine"].get<int>());
    setIntegerParam(chip, ADTimePixVfbk,               data["Chips"][chip]["DACs"]["Vfbk"].get<int>());
    setIntegerParam(chip, ADTimePixVthresholdCoarse,   data["Chips"][chip]["DACs"]["Vthreshold_coarse"].get<int>());
    setIntegerParam(chip, ADTimePixVthresholdFine,     data["Chips"][chip]["DACs"]["Vthreshold_fine"].get<int>());

    if (API_Ver[0] == '4') {    // Serval version 4, Health is an array (multi-board: temps per board)
        setIntegerParam(chip, ADTimePixChipNTemperature, chipTemperatureFromHealthV4(data, chip));
    } else {  // Serval version 3 or 2, Health is a dictionary
        setIntegerParam(chip, ADTimePixChipNTemperature, chipTemperatureFromHealthV3(data, chip));
    }

    if (data["Chips"][chip]["Adjust"].is_null()) {  // Serval version 3, Adjust is typically not present / null.
        setIntegerParam(chip, ADTimePixAdjust, -1);
    }
    else if (data["Chips"][chip]["Adjust"].is_array()) {    // Serval version 4, Adjust is an array by default
    //    setIntegerParam(chip, ADTimePixAdjust, data["Chips"][chip]["Adjust"].get<int>());
        setIntegerParam(chip, ADTimePixAdjust, -2);
    }
    else {
        setIntegerParam(chip, ADTimePixAdjust, -3);
    }

    return asynSuccess;
}

namespace {
std::string stripBpcExtForMaskedJson(const std::string& fileName)
{
    if (fileName.size() < 4) return fileName;
    std::string suf = fileName.substr(fileName.size() - 4);
    for (char& c : suf) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    if (suf == ".bpc") return fileName.substr(0, fileName.size() - 4);
    return fileName;
}

std::string utcIso8601Now()
{
    std::time_t t = std::time(nullptr);
    std::tm g;
#if defined(_WIN32)
    gmtime_s(&g, &t);
#else
    gmtime_r(&t, &g);
#endif
    std::ostringstream oss;
    oss << std::put_time(&g, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}
}  // namespace

void ADTimePix::exportMaskedPelsJsonFromBpcBuffer(const char* bpcBuf, int bpcSize)
{
    const char* okEmpty = "Skipped: no BPC buffer";
    if (!bpcBuf || bpcSize <= 0) {
        setStringParam(0, ADTimePixMaskedPelsJsonPath, "");
        setIntegerParam(0, ADTimePixMaskedPelsCount, 0);
        setStringParam(0, ADTimePixMaskedPelsExportStatus, okEmpty);
        callParamCallbacks(0);
        return;
    }

    std::string filePath, fileName;
    getStringParam(ADTimePixBPCFilePath, filePath);
    getStringParam(ADTimePixBPCFileName, fileName);
    if (fileName.empty()) {
        setStringParam(0, ADTimePixMaskedPelsJsonPath, "");
        setIntegerParam(0, ADTimePixMaskedPelsCount, 0);
        setStringParam(0, ADTimePixMaskedPelsExportStatus, "Skipped: BPCFileName empty");
        callParamCallbacks(0);
        return;
    }

    const std::string outPath = filePath + stripBpcExtForMaskedJson(fileName) + "_masked_pels.json";
    int rows = 0, cols = 0, xChips = 0, yChips = 0, pelW = 0;
    rowsCols(&rows, &cols, &xChips, &yChips, &pelW);
    (void)rows;
    (void)xChips;
    (void)yChips;
    int nChips = 1;
    getIntegerParam(ADTimePixNumberOfChips, &nChips);
    int detOr = 0;
    getIntegerParam(ADTimePixDetectorOrientation, &detOr);
    const int chipPelCount = pelW * pelW;

    json root;
    root["format_version"] = 1;
    root["source"]["bpc_path"] = filePath + fileName;
    root["source"]["num_chips"] = nChips;
    root["source"]["detector_orientation"] = detOr;
    root["source"]["chip_pel_width"] = pelW;
    root["source"]["tool"] = "ADTimePix RefreshPixelConfig mask export";
    root["source"]["exported_at_utc"] = utcIso8601Now();

    {
        std::string detType;
        getStringParam(ADTimePixDetType, detType);
        if (!detType.empty()) root["detector"]["type"] = detType;
    }
    json acq;
    acq["BPCFilePath"] = filePath;
    acq["BPCFileName"] = fileName;
    acq["detector_orientation"] = detOr;
    root["acquisition"] = acq;

    json mlist = json::array();
    json badpixels = json::array();
    int counter = 0;
    int skippedUnmapped = 0;

    for (int pos = 0; pos < bpcSize; ++pos) {
        const unsigned char byte = static_cast<unsigned char>(bpcBuf[pos]);
        if ((byte & 1u) == 0) continue;

        int chip = 0;
        int lx = 0, ly = 0;
        if (chipPelCount > 0) {
            chip = pos / chipPelCount;
            const int local = pos - chip * chipPelCount;
            lx = local % pelW;
            ly = local / pelW;
        }

        const int imgIdx = bpc2ImgIndex(pos, pelW);
        if (imgIdx < 0 || cols <= 0) {
            skippedUnmapped++;
            continue;
        }
        const int ii = imgIdx % cols;
        const int jj = imgIdx / cols;

        counter++;
        json row;
        row["index"] = counter;
        row["bpc_index"] = pos;
        row["value"] = static_cast<int>(byte);
        row["chip"] = chip;
        row["lx"] = lx;
        row["ly"] = ly;
        row["i"] = ii;
        row["j"] = jj;
        mlist.push_back(row);

        json bp;
        bp["Pixel"] = json::array({ii, jj});
        bp["Median"] = json::array({1, 1});
        badpixels.push_back(bp);
    }

    root["counts"]["masked_pels"] = counter;
    if (skippedUnmapped > 0) {
        root["counts"]["skipped_unmapped_bpc_index"] = skippedUnmapped;
    }
    root["masked_pels"] = mlist;
    root["Bad pixels"] = badpixels;

    std::ofstream ofs(outPath.c_str(), std::ios::binary | std::ios::trunc);
    if (!ofs) {
        char emsg[256];
        epicsSnprintf(emsg, sizeof(emsg), "Write failed: %s", outPath.c_str());
        setStringParam(0, ADTimePixMaskedPelsJsonPath, "");
        setIntegerParam(0, ADTimePixMaskedPelsCount, 0);
        setStringParam(0, ADTimePixMaskedPelsExportStatus, emsg);
        ERR_ARGS("exportMaskedPelsJson: %s", emsg);
        callParamCallbacks(0);
        return;
    }
    ofs << root.dump(2);
    ofs.close();

    setStringParam(0, ADTimePixMaskedPelsJsonPath, outPath.c_str());
    setIntegerParam(0, ADTimePixMaskedPelsCount, counter);
    {
        char smsg[160];
        if (skippedUnmapped > 0) {
            epicsSnprintf(smsg, sizeof(smsg), "OK: %d masked pels, %d index unmapped", counter,
                          skippedUnmapped);
        } else {
            epicsSnprintf(smsg, sizeof(smsg), "OK: wrote %d masked pels", counter);
        }
        setStringParam(0, ADTimePixMaskedPelsExportStatus, smsg);
    }
    FLOW_ARGS("masked pels JSON %s (%d entries)", outPath.c_str(), counter);
    callParamCallbacks(0);
}

asynStatus ADTimePix::refreshPixelConfigFromServal() {
    FLOW_ARGS("fetch per chip, compare to BPC on disk");
    asynStatus status = asynSuccess;
    char* bpcBuf = NULL;
    int bpcSize = 0;
    readBPCfile(&bpcBuf, &bpcSize);

    int nChips = 1;
    getIntegerParam(ADTimePixNumberOfChips, &nChips);
    if (nChips < 1) nChips = 1;

    /* Image layout must match mask save (pelIndex): bpc2ImgIndex() is not the inverse of pelIndex for all
     * quad orientations (e.g. LEFT), so PixelConfigDiff is filled by (i,j) -> file k = pelIndex(i,j). */
    int rowsLay = 0, colsLay = 0, xChipsLay = 0, yChipsLay = 0, pelWidth = 0;
    rowsCols(&rowsLay, &colsLay, &xChipsLay, &yChipsLay, &pelWidth);
    (void)pelWidth;

    std::vector<uint8_t> serValLinear(262144u, 0);
    std::vector<char> chipDecoded(static_cast<size_t>(nChips > 0 ? nChips : 1), 0);

    for (int chip = 0; chip < nChips; chip++) {
        char statusMsg[256];
        std::string url = serverURL + "/detector/chips/" + std::to_string(chip) + "/PixelConfig";
        cpr::Response r = ADTimePix3ServalHttp::get(url, 5000);

        if (r.status_code != 200) {
            epicsSnprintf(statusMsg, sizeof(statusMsg), "HTTP %ld", (long)r.status_code);
            setIntegerParam(chip, ADTimePixPixelConfigLen, 0);
            setIntegerParam(chip, ADTimePixPixelConfigMatchBPC, -1);
            setInteger64Param(chip, ADTimePixPixelConfigMismatchBytes, 0);
            setStringParam(chip, ADTimePixPixelConfigStatus, statusMsg);
            callParamCallbacks(chip);
            ERR_ARGS("chip %d PixelConfig GET failed: %s", chip, statusMsg);
            continue;
        }

        try {
            json j = json::parse(r.text);
            if (!j.is_string()) {
                setIntegerParam(chip, ADTimePixPixelConfigLen, 0);
                setIntegerParam(chip, ADTimePixPixelConfigMatchBPC, -1);
                setInteger64Param(chip, ADTimePixPixelConfigMismatchBytes, 0);
                epicsSnprintf(statusMsg, sizeof(statusMsg), "JSON is not a string");
                setStringParam(chip, ADTimePixPixelConfigStatus, statusMsg);
                callParamCallbacks(chip);
                ERR_ARGS("chip %d PixelConfig: expected JSON string", chip);
                continue;
            }
            std::string b64 = j.get<std::string>();
            std::vector<uint8_t> decoded;
            if (!decodeBase64(b64, decoded)) {
                setIntegerParam(chip, ADTimePixPixelConfigLen, 0);
                setIntegerParam(chip, ADTimePixPixelConfigMatchBPC, -1);
                setInteger64Param(chip, ADTimePixPixelConfigMismatchBytes, 0);
                setStringParam(chip, ADTimePixPixelConfigStatus, "Base64 decode failed");
                callParamCallbacks(chip);
                ERR_ARGS("chip %d PixelConfig: base64 decode failed", chip);
                continue;
            }

            const int decLen = static_cast<int>(decoded.size());
            setIntegerParam(chip, ADTimePixPixelConfigLen, decLen);

            if (!bpcBuf || bpcSize <= 0) {
                setIntegerParam(chip, ADTimePixPixelConfigMatchBPC, 2);
                setInteger64Param(chip, ADTimePixPixelConfigMismatchBytes, 0);
                setStringParam(chip, ADTimePixPixelConfigStatus, "OK, no BPC file");
                callParamCallbacks(chip);
                continue;
            }

            const size_t offset = static_cast<size_t>(chip) * kPixelConfigBytes;
            if (offset >= static_cast<size_t>(bpcSize)) {
                setIntegerParam(chip, ADTimePixPixelConfigMatchBPC, 3);
                setInteger64Param(chip, ADTimePixPixelConfigMismatchBytes, 0);
                epicsSnprintf(statusMsg, sizeof(statusMsg),
                              "BPC too small for chip (need offset %zu, have %d)", offset,
                              bpcSize);
                setStringParam(chip, ADTimePixPixelConfigStatus, statusMsg);
                callParamCallbacks(chip);
                continue;
            }

            /* One chip = kPixelConfigBytes in file; do not use (bpcSize - offset) alone or a
             * 4×64KiB file makes chip0 "slice" 262144B and decoded 65536B always "length mismatch". */
            const size_t bytesFromOffset = static_cast<size_t>(bpcSize) - offset;
            const size_t chipFileLen = std::min(kPixelConfigBytes, bytesFromOffset);
            const size_t ncmp = decoded.size() < chipFileLen ? decoded.size() : chipFileLen;
            epicsInt64 mismatch = 0;
            for (size_t i = 0; i < ncmp; i++) {
                if (decoded[i] != static_cast<unsigned char>(bpcBuf[offset + i])) mismatch++;
            }
            chipDecoded[static_cast<size_t>(chip)] = 1;
            for (size_t i = 0; i < decoded.size() && i < kPixelConfigBytes && offset + i < serValLinear.size(); ++i) {
                serValLinear[offset + i] = decoded[i];
            }
            if (mismatch > 0) {
                setIntegerParam(chip, ADTimePixPixelConfigMatchBPC, 0);
                setInteger64Param(chip, ADTimePixPixelConfigMismatchBytes, mismatch);
                epicsSnprintf(statusMsg, sizeof(statusMsg), "Mismatch %lld bytes",
                              (long long)mismatch);
                setStringParam(chip, ADTimePixPixelConfigStatus, statusMsg);
            } else if (decoded.size() != chipFileLen) {
                setIntegerParam(chip, ADTimePixPixelConfigMatchBPC, 3);
                setInteger64Param(chip, ADTimePixPixelConfigMismatchBytes, 0);
                epicsSnprintf(statusMsg, sizeof(statusMsg),
                              "Length mismatch (decoded %d, chip file %zu)", decLen, chipFileLen);
                setStringParam(chip, ADTimePixPixelConfigStatus, statusMsg);
            } else {
                setIntegerParam(chip, ADTimePixPixelConfigMatchBPC, 1);
                setInteger64Param(chip, ADTimePixPixelConfigMismatchBytes, 0);
                setStringParam(chip, ADTimePixPixelConfigStatus, "OK, matches BPC");
            }
            callParamCallbacks(chip);
        } catch (const std::exception& e) {
            setIntegerParam(chip, ADTimePixPixelConfigLen, 0);
            setIntegerParam(chip, ADTimePixPixelConfigMatchBPC, -1);
            setInteger64Param(chip, ADTimePixPixelConfigMismatchBytes, 0);
            epicsSnprintf(statusMsg, sizeof(statusMsg), "Parse error: %.200s", e.what());
            setStringParam(chip, ADTimePixPixelConfigStatus, statusMsg);
            callParamCallbacks(chip);
            ERR_ARGS("chip %d PixelConfig: %s", chip, e.what());
        }
    }

    if (bpcBuf && bpcSize > 0) {
        epicsMutexLock(pixelConfigDiffMutex_);
        std::fill(pixelConfigDiff_.begin(), pixelConfigDiff_.end(), 0);
        for (int j = 0; j < rowsLay; ++j) {
            for (int i = 0; i < colsLay; ++i) {
                const size_t imgLin = static_cast<size_t>(j) * static_cast<size_t>(colsLay) + static_cast<size_t>(i);
                if (imgLin >= pixelConfigDiff_.size()) continue;
                const int k = pelIndex(i, j);
                if (k < 0 || k >= bpcSize || static_cast<size_t>(k) >= serValLinear.size()) {
                    pixelConfigDiff_[imgLin] = 0;
                    continue;
                }
                const int chipOfK = k / static_cast<int>(kPixelConfigBytes);
                if (chipOfK < 0 || chipOfK >= nChips ||
                    static_cast<size_t>(chipOfK) >= chipDecoded.size() || !chipDecoded[static_cast<size_t>(chipOfK)]) {
                    pixelConfigDiff_[imgLin] = 0;
                    continue;
                }
                const int a = static_cast<int>(serValLinear[static_cast<size_t>(k)]);
                const int b = static_cast<int>(static_cast<unsigned char>(bpcBuf[k]));
                pixelConfigDiff_[imgLin] = static_cast<epicsInt32>(a > b ? a - b : b - a);
            }
        }
        epicsMutexUnlock(pixelConfigDiffMutex_);
    }

    if (bpcBuf && bpcSize > 0) {
        exportMaskedPelsJsonFromBpcBuffer(bpcBuf, bpcSize);
    }

    if (bpcBuf) {
        free(bpcBuf);
        bpcBuf = NULL;
    }
    /* Waveform PixelConfigDiff: row-major image (j*cols+i), same convention as maskCircle / mask write; file index via pelIndex. */
    const size_t ncb = pixelConfigDiff_.size();
    /* asyn waveform interrupt copies data only if auxStatus is asynSuccess (devAsynXXXArray.cpp). */
    setParamStatus(0, ADTimePixPixelConfigDiff, asynSuccess);
    doCallbacksInt32Array(pixelConfigDiff_.data(), ncb, ADTimePixPixelConfigDiff, 0);

    return status;
}

asynStatus ADTimePix::getDetector(){
    asynStatus status = asynSuccess;
    FLOW("Reading Detector Health, info, config, layout, chips");
    std::string detector;
    std::string API_Ver;

    getStringParam(ADSDKVersion, API_Ver);

    detector = this->serverURL + std::string("/detector");
    cpr::Response r = ADTimePix3ServalHttp::get(detector, 5000);

    if (r.status_code != 200) {
        setIntegerParam(ADTimePixDetConnected,0);
        setStringParam(ADTimePixWriteMsg, r.text.c_str());
    }
    else {
        setIntegerParam(ADTimePixDetConnected,1);

        json detector_j = json::parse(r.text.c_str());

        // printf("Number of chips=%d\n", detector_j["Info"]["NumberOfChips"].get<int>());

        auto jsonDbl = [](const json& j, double def) -> double {
            return j.is_number() ? j.get<double>() : def;
        };
        auto jsonInt = [](const json& j, int def) -> int {
            if (j.is_number_integer()) return j.get<int>();
            if (j.is_number_unsigned()) return static_cast<int>(j.get<unsigned>());
            return def;
        };

        // Detector health PVs — SERVAL may send null for unavailable sensors (e.g. BiasVoltage when BIAS ADC offline).
        if (API_Ver[0] == '4') {    // Serval version 4
//            printf("Serval version 4, %s,%s\n", API_Ver.c_str(), detector_j.dump(3,' ', true).c_str());
            const json& H0 = detector_j["Health"][0];
            setDoubleParam(ADTimePixLocalTemp, jsonDbl(H0["LocalTemperature"], 0.0));
            setDoubleParam(ADTimePixFPGATemp, jsonDbl(H0["FPGATemperature"], 0.0));
            setDoubleParam(ADTimePixFan1Speed, jsonDbl(H0["Fan1Speed"], 0.0));
            setDoubleParam(ADTimePixFan2Speed, jsonDbl(H0["Fan2Speed"], 0.0));
            setDoubleParam(ADTimePixBiasVoltage, jsonDbl(H0["BiasVoltage"], 0.0));
            setIntegerParam(ADTimePixHumidity, jsonInt(H0["Humidity"], 0));

            json chipTempsMerged = json::array();
            if (detector_j["Health"].is_array()) {
                for (const auto& hb : detector_j["Health"]) {
                    if (hb.is_object() && hb.contains("ChipTemperatures") && hb["ChipTemperatures"].is_array()) {
                        for (const auto& te : hb["ChipTemperatures"]) chipTempsMerged.push_back(te);
                    }
                }
            }
            if (!chipTempsMerged.empty())
                setStringParam(ADTimePixChipTemperature, chipTempsMerged.dump().c_str());
            else
                setStringParam(ADTimePixChipTemperature, H0["ChipTemperatures"].dump().c_str());

            if (detector_j["Health"].is_array() && detector_j["Health"].size() > 1) {
                json vddM = json::array();
                json avddM = json::array();
                for (const auto& hb : detector_j["Health"]) {
                    if (hb.is_object() && hb.contains("VDD") && hb["VDD"].is_array()) vddM.push_back(hb["VDD"]);
                    if (hb.is_object() && hb.contains("AVDD") && hb["AVDD"].is_array()) avddM.push_back(hb["AVDD"]);
                }
                setStringParam(ADTimePixVDD, vddM.dump().c_str());
                setStringParam(ADTimePixAVDD, avddM.dump().c_str());
            } else {
                setStringParam(ADTimePixVDD, H0["VDD"].dump().c_str());
                setStringParam(ADTimePixAVDD, H0["AVDD"].dump().c_str());
            }

            // VDD, AVDD per rail: asyn ADDR 0–2 board 0, ADDR 3–5 board 1 (second SPIDR)
            for (int i = 0; i < 3; i++) {
                if (H0["VDD"].is_array() && (size_t)i < H0["VDD"].size() && H0["VDD"][(size_t)i].is_number())
                    setDoubleParam(i, ADTimePixChipN_VDD, H0["VDD"][(size_t)i].get<double>());
                if (H0["AVDD"].is_array() && (size_t)i < H0["AVDD"].size() && H0["AVDD"][(size_t)i].is_number())
                    setDoubleParam(i, ADTimePixChipN_AVDD, H0["AVDD"][(size_t)i].get<double>());
            }
            if (detector_j["Health"].is_array() && detector_j["Health"].size() > 1) {
                const json& H1 = detector_j["Health"][1];
                for (int i = 0; i < 3; i++) {
                    int addr = i + 3;
                    if (H1["VDD"].is_array() && (size_t)i < H1["VDD"].size() && H1["VDD"][(size_t)i].is_number())
                        setDoubleParam(addr, ADTimePixChipN_VDD, H1["VDD"][(size_t)i].get<double>());
                    if (H1["AVDD"].is_array() && (size_t)i < H1["AVDD"].size() && H1["AVDD"][(size_t)i].is_number())
                        setDoubleParam(addr, ADTimePixChipN_AVDD, H1["AVDD"][(size_t)i].get<double>());
                }
            } else {
                for (int i = 0; i < 3; i++) {
                    int addr = i + 3;
                    setDoubleParam(addr, ADTimePixChipN_VDD, 0.0);
                    setDoubleParam(addr, ADTimePixChipN_AVDD, 0.0);
                }
            }

        }
        else {  // Serval version 3 or 2
            const json& H = detector_j["Health"];
            setDoubleParam(ADTimePixLocalTemp, jsonDbl(H["LocalTemperature"], 0.0));
            setDoubleParam(ADTimePixFPGATemp, jsonDbl(H["FPGATemperature"], 0.0));
            setDoubleParam(ADTimePixFan1Speed, jsonDbl(H["Fan1Speed"], 0.0));
            setDoubleParam(ADTimePixFan2Speed, jsonDbl(H["Fan2Speed"], 0.0));
            setDoubleParam(ADTimePixBiasVoltage, jsonDbl(H["BiasVoltage"], 0.0));
            setIntegerParam(ADTimePixHumidity, jsonInt(H["Humidity"], 0));
            setStringParam(ADTimePixChipTemperature, H["ChipTemperatures"].dump().c_str());
            setStringParam(ADTimePixVDD, H["VDD"].dump().c_str());
            setStringParam(ADTimePixAVDD, H["AVDD"].dump().c_str());

            // VDD, AVDD voltages
            for (int i = 0; i < 3; i++) {
                if (H["VDD"].is_array() && (size_t)i < H["VDD"].size() && H["VDD"][(size_t)i].is_number())
                    setDoubleParam(i, ADTimePixChipN_VDD, H["VDD"][(size_t)i].get<double>());
                if (H["AVDD"].is_array() && (size_t)i < H["AVDD"].size() && H["AVDD"][(size_t)i].is_number())
                    setDoubleParam(i, ADTimePixChipN_AVDD, H["AVDD"][(size_t)i].get<double>());
            }
        }

        // Detector Info
        setStringParam(ADTimePixIfaceName,   strip_quotes(detector_j["Info"]["IfaceName"].dump().c_str()));
        //setStringParam(ADTimePixChipboardID, strip_quotes(detector_j["Info"]["ChipboardID"].dump().c_str()));
        setStringParam(ADTimePixSW_version,  strip_quotes(detector_j["Info"]["SW_version"].dump().c_str()));
        setStringParam(ADTimePixFW_version,  strip_quotes(detector_j["Info"]["FW_version"].dump().c_str()));

    //    setStringParam(ADSerialNumber,      strip_quotes(detector_j["Info"]["ChipboardID"].dump().c_str()));
        setStringParam(ADSerialNumber,      strip_quotes(detector_j["Info"]["SW_version"].dump().c_str()));
        setStringParam(ADFirmwareVersion,   strip_quotes(detector_j["Info"]["FW_version"].dump().c_str()));

        setIntegerParam(ADTimePixPixCount,      detector_j["Info"]["PixCount"].get<int>());
        setIntegerParam(ADTimePixRowLen,        detector_j["Info"]["RowLen"].get<int>());
        setIntegerParam(ADTimePixNumberOfChips, detector_j["Info"]["NumberOfChips"].get<int>());
        setIntegerParam(ADTimePixNumberOfRows,  detector_j["Info"]["NumberOfRows"].get<int>());
        setIntegerParam(ADMaxSizeY,     detector_j["Info"]["NumberOfRows"].get<int>());                                             // Sensor Size Y
        setIntegerParam(ADMaxSizeX,     detector_j["Info"]["PixCount"].get<int>() / detector_j["Info"]["NumberOfRows"].get<int>());  // Sensor Size X
        setIntegerParam(ADTimePixMpxType,       detector_j["Info"]["MpxType"].get<int>());

        if (detector_j["Info"].contains("Boards") && detector_j["Info"]["Boards"].is_array() &&
            !detector_j["Info"]["Boards"].empty()) {
            const json& B0 = detector_j["Info"]["Boards"][0];
            setStringParam(ADTimePixBoardsID, strip_quotes(B0["ChipboardId"].dump().c_str()));
            setStringParam(ADTimePixBoardsIP, strip_quotes(B0["IpAddress"].dump().c_str()));
            if (B0.contains("Chips") && B0["Chips"].is_array() && B0["Chips"].size() > 0) {
                setStringParam(ADTimePixBoardsCh1, strip_quotes(B0["Chips"][0].dump().c_str()));
                setStringParam(ADTimePixBoardsCh2, B0["Chips"].size() > 1 ? strip_quotes(B0["Chips"][1].dump().c_str()) : "");
                setStringParam(ADTimePixBoardsCh3, B0["Chips"].size() > 2 ? strip_quotes(B0["Chips"][2].dump().c_str()) : "");
                setStringParam(ADTimePixBoardsCh4, B0["Chips"].size() > 3 ? strip_quotes(B0["Chips"][3].dump().c_str()) : "");
            } else {
                setStringParam(ADTimePixBoardsCh1, "");
                setStringParam(ADTimePixBoardsCh2, "");
                setStringParam(ADTimePixBoardsCh3, "");
                setStringParam(ADTimePixBoardsCh4, "");
            }
            if (detector_j["Info"]["Boards"].size() > 1) {
                const json& B1 = detector_j["Info"]["Boards"][1];
                setStringParam(ADTimePixBoards2ID, strip_quotes(B1["ChipboardId"].dump().c_str()));
                setStringParam(ADTimePixBoards2IP, strip_quotes(B1["IpAddress"].dump().c_str()));
                if (B1.contains("Chips") && B1["Chips"].is_array() && B1["Chips"].size() >= 4) {
                    setStringParam(ADTimePixBoardsCh5, strip_quotes(B1["Chips"][0].dump().c_str()));
                    setStringParam(ADTimePixBoardsCh6, strip_quotes(B1["Chips"][1].dump().c_str()));
                    setStringParam(ADTimePixBoardsCh7, strip_quotes(B1["Chips"][2].dump().c_str()));
                    setStringParam(ADTimePixBoardsCh8, strip_quotes(B1["Chips"][3].dump().c_str()));
                } else {
                    setStringParam(ADTimePixBoardsCh5, "");
                    setStringParam(ADTimePixBoardsCh6, "");
                    setStringParam(ADTimePixBoardsCh7, "");
                    setStringParam(ADTimePixBoardsCh8, "");
                }
            } else {
                setStringParam(ADTimePixBoards2ID, "");
                setStringParam(ADTimePixBoards2IP, "");
                setStringParam(ADTimePixBoardsCh5, "");
                setStringParam(ADTimePixBoardsCh6, "");
                setStringParam(ADTimePixBoardsCh7, "");
                setStringParam(ADTimePixBoardsCh8, "");
            }
        } else {
            setStringParam(ADTimePixBoardsID, "");
            setStringParam(ADTimePixBoardsIP, "");
            setStringParam(ADTimePixBoardsCh1, "");
            setStringParam(ADTimePixBoardsCh2, "");
            setStringParam(ADTimePixBoardsCh3, "");
            setStringParam(ADTimePixBoardsCh4, "");
            setStringParam(ADTimePixBoards2ID, "");
            setStringParam(ADTimePixBoards2IP, "");
            setStringParam(ADTimePixBoardsCh5, "");
            setStringParam(ADTimePixBoardsCh6, "");
            setStringParam(ADTimePixBoardsCh7, "");
            setStringParam(ADTimePixBoardsCh8, "");
        }

        setIntegerParam(ADTimePixSuppAcqModes,  detector_j["Info"]["SuppAcqModes"].get<int>());
        setDoubleParam(ADTimePixClockReadout,   detector_j["Info"]["ClockReadout"].get<double>());
        setIntegerParam(ADTimePixMaxPulseCount, detector_j["Info"]["MaxPulseCount"].get<int>());
        setDoubleParam(ADTimePixMaxPulseHeight, detector_j["Info"]["MaxPulseHeight"].get<double>());
        setDoubleParam(ADTimePixMaxPulsePeriod, detector_j["Info"]["MaxPulsePeriod"].get<double>());
        setDoubleParam(ADTimePixTimerMaxVal,    detector_j["Info"]["TimerMaxVal"].get<double>());
        setDoubleParam(ADTimePixTimerMinVal,    detector_j["Info"]["TimerMinVal"].get<double>());
        setDoubleParam(ADTimePixTimerStep,      detector_j["Info"]["TimerStep"].get<double>());
        setDoubleParam(ADTimePixClockTimepix,   detector_j["Info"]["ClockTimepix"].get<double>());

        // Detector Config Readback
        setIntegerParam(ADTimePixFan1PWM,                detector_j["Config"]["Fan1PWM"].get<int>());
        setIntegerParam(ADTimePixFan2PWM,                detector_j["Config"]["Fan2PWM"].get<int>());
        setIntegerParam(ADTimePixBiasVolt,               detector_j["Config"]["BiasVoltage"].get<int>());
        setIntegerParam(ADTimePixBiasEnable,             int(detector_j["Config"]["BiasEnabled"]));         // bool->int true->1, false->0
    //    setStringParam(ADTimePixChainMode,               strip_quotes(detector_j["Config"]["ChainMode"].dump().c_str()));
        setIntegerParam(ADTimePixTriggerIn,              detector_j["Config"]["TriggerIn"].get<int>());
        setIntegerParam(ADTimePixTriggerOut,             detector_j["Config"]["TriggerOut"].get<int>());
    //    setStringParam(ADTimePixPolarity,                strip_quotes(detector_j["Config"]["Polarity"].dump().c_str()));
        setStringParam(ADTimePixTriggerMode,             strip_quotes(detector_j["Config"]["TriggerMode"].dump().c_str()));
        //setStringParam(ADTriggerMode,             strip_quotes(detector_j["Config"]["TriggerMode"].dump().c_str()));
        setDoubleParam(ADTimePixExposureTime,            detector_j["Config"]["ExposureTime"].get<double>());
        setDoubleParam(ADAcquireTime,                    detector_j["Config"]["ExposureTime"].get<double>());       // Exposure Time RBV
        setDoubleParam(ADTimePixTriggerPeriod,           detector_j["Config"]["TriggerPeriod"].get<double>());
        setDoubleParam(ADAcquirePeriod,                  detector_j["Config"]["TriggerPeriod"].get<double>());     // Exposure Period RBV
        setIntegerParam(ADTimePixnTriggers,              detector_j["Config"]["nTriggers"].get<int>());
        setIntegerParam(ADTimePixPeriphClk80,            int(detector_j["Config"]["PeriphClk80"]));          // bool->int true->1, false->0
        setDoubleParam(ADTimePixTriggerDelay,            detector_j["Config"]["TriggerDelay"].get<double>());
        setStringParam(ADTimePixTdc,                     strip_quotes(detector_j["Config"]["Tdc"].dump().c_str()));
        setDoubleParam(ADTimePixGlobalTimestampInterval, detector_j["Config"]["GlobalTimestampInterval"].get<double>());
        setIntegerParam(ADTimePixExternalReferenceClock, int(detector_j["Config"]["ExternalReferenceClock"]));   // bool->int true->1, false->0
        setIntegerParam(ADTimePixLogLevel,               detector_j["Config"]["LogLevel"].get<int>());


        // Detector Chips: Chip0
        fetchDacs(detector_j, 0);

        // Serval3 - Detector Chip Layout
        setIntegerParam(ADTimePixDetectorOrientation, mDetOrientationMap[strip_quotes(detector_j["Layout"]["DetectorOrientation"].dump().c_str())]);
        setStringParam(0, ADTimePixLayout, detector_j["Layout"]["Original"]["Chips"][0].dump().c_str());
        callParamCallbacks();

        int number_chips = detector_j["Info"]["NumberOfChips"].get<int>();
        for (int chip = 1; chip < number_chips; chip++) {
            fetchDacs(detector_j, chip);
            setStringParam(
                chip,
                ADTimePixLayout,
                detector_j["Layout"]["Original"]["Chips"][chip].dump().c_str()
            );
            callParamCallbacks(chip);
        }
    }
    return status;
}

/**
 * Function that updates PV values of server with camera information
 * 
 * @return: status
 */
asynStatus ADTimePix::getServer(){
    asynStatus status = asynSuccess;
    FLOW("Reading detector streams");
    std::string server;

    // Use the vendor library to collect information about the camera format here, and set the appropriate PVs
    /* Example Serval 4.x Destination JSON (file bases use file:/path — one slash after "file:", not file:///)
        {
          "Destination" : {
            "Raw" : [ {
              "Base" : "file:/media/nvme/raw",
              "FilePattern" : "raw%Hms_",
              "SplitStrategy" : "SINGLE_FILE",
              "QueueSize" : 16384
            } ],
            "Image" : [ {
              "Base" : "file:/media/nvme/img",
              "FilePattern" : "f%Hms_",
              "Format" : "tiff",
              "Mode" : "tot",
              "Thresholds" : [ 0, 1, 2, 3, 4, 5, 6, 7 ],
              "IntegrationSize" : 0,
              "StopMeasurementOnDiskLimit" : true,
              "QueueSize" : 1024
            } ]
          }
        }
    */

    server = this->serverURL + std::string("/server/destination");
    cpr::Response r = ADTimePix3ServalHttp::get(server, 5000);

    if (r.status_code != 200) {
    //    printf("Text server: %s\n", r.text.c_str());
    //    printf("Header server: %s\n", r.header["Content-Type"].c_str());
    //    printf("Error server: %s\n", r.error.message.c_str());
    //    printf("Status code server: %li\n", r.status_code);
    //    printf("Elapsed server: %li\n", r.elapsed);
    //    printf("Reason server: %s\n", r.reason.c_str());
    //    printf("Url server: %s\n", r.url.c_str());

        setIntegerParam(ADTimePixDetConnected,0);
        setStringParam(ADTimePixWriteMsg, r.text.c_str());
    }
    else {
        setIntegerParam(ADTimePixDetConnected,1);
    //    setStringParam(ADTimePixWriteMsg, r.text.c_str());

        json server_j = json::parse(r.text.c_str());
    //    printf("getDetector: Server Destination JSON,%s\n", server_j.dump(3,' ', true).c_str());
    //    printf("Number of channels: %ld\n", server_j.size());
    //    printf("Number of Raw channels: %ld\n", server_j["Raw"].size());
    //    printf("Number of Image channels: %ld\n", server_j["Image"].size());
    //    printf("Number of Preview channels: %ld\n", server_j["Preview"].size());
    //    printf("Number of Preview Image channels: %ld\n", server_j["Preview"]["ImageChannels"].size());
    //    printf("Number of Preview Histogram channels: %ld\n\n", server_j["Preview"]["HistogramChannels"].size());

        switch (server_j["Raw"].size()) {
            case 0:
                setIntegerParam(ADTimePixWriteRawRead, 0);
                setIntegerParam(ADTimePixWriteRaw1Read, 0);
                break; // No Raw channels
            case 1:
                setIntegerParam(ADTimePixWriteRawRead, 1);
                setIntegerParam(ADTimePixWriteRaw1Read, 0);
                break; // One Raw channel
            case 2:
                setIntegerParam(ADTimePixWriteRawRead, 1);
                setIntegerParam(ADTimePixWriteRaw1Read, 1);
                break; // Two Raw channels
            default:
                printf("More than two Raw channels\n");
                setIntegerParam(ADTimePixWriteRawRead, 1);
                setIntegerParam(ADTimePixWriteRaw1Read, 1);
                break; // More than two Raw channels
        }

        switch (server_j["Image"].size()) {
            case 0:
                setIntegerParam(ADTimePixWriteImgRead, 0);
                setIntegerParam(ADTimePixWriteImg1Read, 0);
                break; // No Image channels
            case 1:
                setIntegerParam(ADTimePixWriteImgRead, 1);
                setIntegerParam(ADTimePixWriteImg1Read, 0);
                break; // One Image channel
            case 2:
                setIntegerParam(ADTimePixWriteImgRead, 1);
                setIntegerParam(ADTimePixWriteImg1Read, 1);
                break; // Two Image channels
            default:
                printf("More than two Image channels\n");
                setIntegerParam(ADTimePixWriteImgRead, 1);
                setIntegerParam(ADTimePixWriteImg1Read, 1);
                break; // More than two Image channels
        }

        switch (server_j["Preview"]["ImageChannels"].size()) {
            case 0:
                setIntegerParam(ADTimePixWritePrvImgRead, 0);
                setIntegerParam(ADTimePixWritePrvImg1Read, 0);
                break; // No Preview Image channels
            case 1:
                setIntegerParam(ADTimePixWritePrvImgRead, 1);
                setIntegerParam(ADTimePixWritePrvImg1Read, 0);
                break; // One Preview Image channel
            case 2:
                setIntegerParam(ADTimePixWritePrvImgRead, 1);
                setIntegerParam(ADTimePixWritePrvImg1Read, 1);
                break; // Two Preview Image channels
            default:
                printf("More than two Preview Image channels\n");
                setIntegerParam(ADTimePixWritePrvImgRead, 1);
                setIntegerParam(ADTimePixWritePrvImg1Read, 1);
                break; // More than two Preview Image channels
        }

        switch (server_j["Preview"]["HistogramChannels"].size()) {
            case 0:
                setIntegerParam(ADTimePixWritePrvHstRead, 0);
                break; // No Preview Histogram channels
            case 1:
                setIntegerParam(ADTimePixWritePrvHstRead, 1);
                break; // One Preview Histogram channel
            default:
                printf("More than one Preview Histogram channels\n");
                setIntegerParam(ADTimePixWritePrvHstRead, 1);
                break; // More than one Preview Histogram channels
        }
    }
    callParamCallbacks();

    return status;
}

/** uploadBPC() implemented in mask_io.cpp with other BPC/mask logic */

/**
 * Initialize detector - upload Chips DACS
 * 
 * serverURL:       the URL of the running SERVAL (string)
 * dacs_file:       an absolute path to the text chips configuration file (string), tpx3-demo.dacs 
 * 
 * @return: status
 */
asynStatus ADTimePix::uploadDACS(){
    asynStatus status = asynSuccess;
    FLOW("Initializing Chips/DACS detector information");
    std::string dacs_file, filePath, fileName;

//    dacs_file = this->serverURL + std::string("/config/load?format=dacs&file=") + std::string("/epics/src/RHEL8/support/areaDetector/ADTimePix/vendor/tpx3-demo.dacs");
    getStringParam(ADTimePixDACSFilePath, filePath);
    getStringParam(ADTimePixDACSFileName, fileName);
    dacs_file = this->serverURL + std::string("/config/load?format=dacs&file=") + std::string(filePath) + std::string(fileName);

    cpr::Response r = ADTimePix3ServalHttp::getAuthOnly(dacs_file);
    if (r.status_code != 200) {
        logHttpFailure("uploadDACS GET /config/load dacs", "GET", dacs_file, (long)r.status_code, r.text);
        status = asynError;
    }

    printf("\nuploadDACS: http_code = %li\n", r.status_code);
    printf("Text dacs_file: %s\n", r.text.c_str()); 
    setIntegerParam(ADTimePixHttpCode, r.status_code);
    setStringParam(ADTimePixWriteMsg, r.text.c_str());   

    return status;
}

// Static JSON arrays to avoid repeated allocations
static const json IMG_FORMATS = {"tiff", "pgm", "png", "jsonimage", "jsonhisto"};
static const json IMG_MODES = {"count", "tot", "toa", "tof", "count_fb"};
static const json SAMPLING_MODES = {"skipOnFrame", "skipOnPeriod"};
static const json STOP_ON_DISK_LIMIT = {"false", "true"};
static const json INTEGRATION_MODES = {"sum", "average", "last"};
static const json SPLIT_STRATEGIES = {"single_file", "frame"};

/**
 * Helper function to safely get integer parameters
 */
asynStatus ADTimePix::getParameterSafely(int param, int& value) {
    asynStatus status = getIntegerParam(param, &value);
    if (status != asynSuccess) {
        ERR_ARGS("Failed to get integer parameter %d (status=%d) - parameter may not be initialized", param, status);
        // Set a default value to avoid further errors
        value = 0;
    }
    return status;
}

/**
 * Helper function to safely get string parameters
 */
asynStatus ADTimePix::getParameterSafely(int param, std::string& value) {
    asynStatus status = getStringParam(param, value);
    if (status != asynSuccess) {
        ERR_ARGS("Failed to get string parameter %d", param);
    }
    return status;
}

/**
 * Helper function to safely get double parameters
 */
asynStatus ADTimePix::getParameterSafely(int param, double& value) {
    asynStatus status = getDoubleParam(param, &value);
    if (status != asynSuccess) {
        ERR_ARGS("Failed to get double parameter %d", param);
    }
    return status;
}

/**
 * Validate integration size parameter
 */
bool ADTimePix::validateIntegrationSize(int size) {
    return (size >= -1 && size <= 32);
}

/**
 * Validate array index parameter
 */
bool ADTimePix::validateArrayIndex(int index, int maxSize) {
    return (index >= 0 && index < maxSize);
}

/**
 * Configure a raw data channel
 */
asynStatus ADTimePix::configureRawChannel(int channelIndex, json& server_j) {
    
    int writeChannel, rawStream;
    std::string fileStr;
    
    // Get write channel parameter based on index
    int writeParam = (channelIndex == 0) ? ADTimePixWriteRaw : ADTimePixWriteRaw1;
    int streamParam = (channelIndex == 0) ? ADTimePixRawStream : ADTimePixRaw1Stream;
    
    if (getParameterSafely(writeParam, writeChannel) != asynSuccess) {
        // If parameter retrieval fails, assume channel is disabled
        writeChannel = 0;
    }
    if (writeChannel == 0) return asynSuccess; // Channel not enabled
    
    if (getParameterSafely(streamParam, rawStream) != asynSuccess) {
        // If stream parameter retrieval fails, assume file stream (0)
        rawStream = 0;
    }
    
    // Configure base path and file pattern
    int baseParam = (channelIndex == 0) ? ADTimePixRawBase : ADTimePixRaw1Base;
    int filePatParam = (channelIndex == 0) ? ADTimePixRawFilePat : ADTimePixRaw1FilePat;
    int queueSizeParam = (channelIndex == 0) ? ADTimePixRawQueueSize : ADTimePixRaw1QueueSize;
    
    if (getParameterSafely(baseParam, fileStr) != asynSuccess) return asynError;
    server_j["Raw"][channelIndex]["Base"] = fileStr;
    
    // Check if this is a TCP connection (starts with 'tcp://')
    bool isTcpConnection = (fileStr.find("tcp://") == 0);
    
    if (isTcpConnection) {
        // For TCP connections, only include Base and QueueSize
        int intNum;
        if (getParameterSafely(queueSizeParam, intNum) != asynSuccess) return asynError;
        server_j["Raw"][channelIndex]["QueueSize"] = intNum;
    } else {
        // For file connections, include all parameters
        if (getParameterSafely(filePatParam, fileStr) != asynSuccess) return asynError;
        server_j["Raw"][channelIndex]["FilePattern"] = fileStr;
        
        // Configure file-specific parameters if using file stream
        if (rawStream == 0) {
            int splitStrategyParam = (channelIndex == 0) ? ADTimePixRawSplitStrategy : ADTimePixRaw1SplitStrategy;
            
            int intNum;
            if (getParameterSafely(splitStrategyParam, intNum) != asynSuccess) return asynError;
            if (!validateArrayIndex(intNum, SPLIT_STRATEGIES.size())) {
                ERR_ARGS("Invalid split strategy index: %d", intNum);
                return asynError;
            }
            server_j["Raw"][channelIndex]["SplitStrategy"] = SPLIT_STRATEGIES[intNum];
            
            if (getParameterSafely(queueSizeParam, intNum) != asynSuccess) return asynError;
            server_j["Raw"][channelIndex]["QueueSize"] = intNum;
        }
    }
    
    return asynSuccess;
}

/**
 * Configure an image channel
 */
asynStatus ADTimePix::configureImageChannel(const std::string& jsonPath, json& server_j) {

    int writeChannel, intNum;
    std::string fileStr;

    // Determine which image channel we're configuring
    bool isPreview = (jsonPath.find("Preview") != std::string::npos);
    bool isChannel1 = (jsonPath.find("[1]") != std::string::npos);

    // Get write parameter based on channel type
    int writeParam;
    if (!isPreview) {
        writeParam = isChannel1 ? ADTimePixWriteImg1 : ADTimePixWriteImg;
    } else if (!isChannel1) {
        writeParam = ADTimePixWritePrvImg;
    } else {
        writeParam = ADTimePixWritePrvImg1;
    }

    if (getParameterSafely(writeParam, writeChannel) != asynSuccess) {
        // If parameter retrieval fails, assume channel is disabled
        writeChannel = 0;
    }
    if (writeChannel == 0) return asynSuccess; // Channel not enabled

    // Configure base path and file pattern
    int baseParam, filePatParam;
    if (!isPreview) {
        if (isChannel1) {
            baseParam = ADTimePixImg1Base;
            filePatParam = ADTimePixImg1FilePat;
        } else {
        baseParam = ADTimePixImgBase;
        filePatParam = ADTimePixImgFilePat;
        }
    } else if (!isChannel1) {
        baseParam = ADTimePixPrvImgBase;
        filePatParam = ADTimePixPrvImgFilePat;
    } else {
        baseParam = ADTimePixPrvImg1Base;
        filePatParam = ADTimePixPrvImg1FilePat;
    }

    if (getParameterSafely(baseParam, fileStr) != asynSuccess) return asynError;

    // Use correct JSON structure based on channel type
    if (!isPreview) {
        // Main image channel
        int channelIndex = isChannel1 ? 1 : 0;
        server_j["Image"][channelIndex]["Base"] = fileStr;
    } else {
        // Preview image channels
        int channelIndex = isChannel1 ? 1 : 0;
        server_j["Preview"]["ImageChannels"][channelIndex]["Base"] = fileStr;
    }

    if (getParameterSafely(filePatParam, fileStr) != asynSuccess) return asynError;
    
    if (!isPreview) {
        int channelIndex = isChannel1 ? 1 : 0;
        server_j["Image"][channelIndex]["FilePattern"] = fileStr;
    } else {
        int channelIndex = isChannel1 ? 1 : 0;
        server_j["Preview"]["ImageChannels"][channelIndex]["FilePattern"] = fileStr;
    }

    // Configure format and mode
    int formatParam, modeParam;
    if (!isPreview) {
        if (isChannel1) {
            formatParam = ADTimePixImg1Format;
            modeParam = ADTimePixImg1Mode;
        } else {
        formatParam = ADTimePixImgFormat;
        modeParam = ADTimePixImgMode;
        }
    } else if (!isChannel1) {
        formatParam = ADTimePixPrvImgFormat;
        modeParam = ADTimePixPrvImgMode;
    } else {
        formatParam = ADTimePixPrvImg1Format;
        modeParam = ADTimePixPrvImg1Mode;
    }

    if (getParameterSafely(formatParam, intNum) != asynSuccess) return asynError;
    if (!validateArrayIndex(intNum, IMG_FORMATS.size())) {
        ERR_ARGS("Invalid format index: %d", intNum);
        return asynError;
    }

    if (!isPreview) {
        int channelIndex = isChannel1 ? 1 : 0;
        server_j["Image"][channelIndex]["Format"] = IMG_FORMATS[intNum];
    } else {
        int channelIndex = isChannel1 ? 1 : 0;
        server_j["Preview"]["ImageChannels"][channelIndex]["Format"] = IMG_FORMATS[intNum];
    }

    if (getParameterSafely(modeParam, intNum) != asynSuccess) return asynError;
    if (!validateArrayIndex(intNum, IMG_MODES.size())) {
        ERR_ARGS("Invalid mode index: %d", intNum);
        return asynError;
    }

    if (!isPreview) {
        int channelIndex = isChannel1 ? 1 : 0;
        server_j["Image"][channelIndex]["Mode"] = IMG_MODES[intNum];
    } else {
        int channelIndex = isChannel1 ? 1 : 0;
        server_j["Preview"]["ImageChannels"][channelIndex]["Mode"] = IMG_MODES[intNum];
    }

    // Configure integration settings
    int intSizeParam, intModeParam;
    if (!isPreview) {
        if (isChannel1) {
            intSizeParam = ADTimePixImg1IntSize;
            intModeParam = ADTimePixImg1IntMode;
        } else {
        intSizeParam = ADTimePixImgIntSize;
        intModeParam = ADTimePixImgIntMode;
        }
    } else if (!isChannel1) {
        intSizeParam = ADTimePixPrvImgIntSize;
        intModeParam = ADTimePixPrvImgIntMode;
    } else {
        intSizeParam = ADTimePixPrvImg1IntSize;
        intModeParam = ADTimePixPrvImg1IntMode;
    }

    if (getParameterSafely(intSizeParam, intNum) != asynSuccess) return asynError;
    if (validateIntegrationSize(intNum)) {
        if (!isPreview) {
            int channelIndex = isChannel1 ? 1 : 0;
            server_j["Image"][channelIndex]["IntegrationSize"] = intNum;
        } else {
            int channelIndex = isChannel1 ? 1 : 0;
            server_j["Preview"]["ImageChannels"][channelIndex]["IntegrationSize"] = intNum;
        }
    } else {
        ERR_ARGS("Invalid integration size: %d", intNum);
        return asynError;
    }

    if (intNum != 0 && intNum != 1) {
        if (getParameterSafely(intModeParam, intNum) != asynSuccess) return asynError;
        if (!validateArrayIndex(intNum, INTEGRATION_MODES.size())) {
            ERR_ARGS("Invalid integration mode index: %d", intNum);
            return asynError;
        }
        if (!isPreview) {
            int channelIndex = isChannel1 ? 1 : 0;
            server_j["Image"][channelIndex]["IntegrationMode"] = INTEGRATION_MODES[intNum];
        } else {
            int channelIndex = isChannel1 ? 1 : 0;
            server_j["Preview"]["ImageChannels"][channelIndex]["IntegrationMode"] = INTEGRATION_MODES[intNum];
        }
    }

    // Configure stop on disk limit and queue size
    int stopOnDiskParam, queueSizeParam;
    if (!isPreview) {
        if (isChannel1) {
            stopOnDiskParam = ADTimePixImg1StpOnDskLim;
            queueSizeParam = ADTimePixImg1QueueSize;
        } else {
        stopOnDiskParam = ADTimePixImgStpOnDskLim;
        queueSizeParam = ADTimePixImgQueueSize;
        }
    } else if (!isChannel1) {
        stopOnDiskParam = ADTimePixPrvImgStpOnDskLim;
        queueSizeParam = ADTimePixPrvImgQueueSize;
    } else {
        stopOnDiskParam = ADTimePixPrvImg1StpOnDskLim;
        queueSizeParam = ADTimePixPrvImg1QueueSize;
    }

    if (getParameterSafely(stopOnDiskParam, intNum) != asynSuccess) return asynError;
    if (!validateArrayIndex(intNum, STOP_ON_DISK_LIMIT.size())) {
        ERR_ARGS("Invalid stop on disk limit index: %d", intNum);
        return asynError;
    }
    
    if (!isPreview) {
        int channelIndex = isChannel1 ? 1 : 0;
        server_j["Image"][channelIndex]["StopMeasurementOnDiskLimit"] = STOP_ON_DISK_LIMIT[intNum];
    } else {
        int channelIndex = isChannel1 ? 1 : 0;
        server_j["Preview"]["ImageChannels"][channelIndex]["StopMeasurementOnDiskLimit"] = STOP_ON_DISK_LIMIT[intNum];
    }

    if (getParameterSafely(queueSizeParam, intNum) != asynSuccess) return asynError;
    
    if (!isPreview) {
        int channelIndex = isChannel1 ? 1 : 0;
        server_j["Image"][channelIndex]["QueueSize"] = intNum;
    } else {
        int channelIndex = isChannel1 ? 1 : 0;
        server_j["Preview"]["ImageChannels"][channelIndex]["QueueSize"] = intNum;
    }

    return asynSuccess;
}

/**
 * Configure preview settings
 */
asynStatus ADTimePix::configurePreviewSettings(json& server_j) {

    double doubleNum;
    int intNum;

    // Configure period and sampling mode
    if (getParameterSafely(ADTimePixPrvPeriod, doubleNum) != asynSuccess) return asynError;
    server_j["Preview"]["Period"] = doubleNum;
    
    if (getParameterSafely(ADTimePixPrvSamplingMode, intNum) != asynSuccess) return asynError;
    if (!validateArrayIndex(intNum, SAMPLING_MODES.size())) {
        ERR_ARGS("Invalid sampling mode index: %d", intNum);
        return asynError;
    }
    server_j["Preview"]["SamplingMode"] = SAMPLING_MODES[intNum];

    return asynSuccess;
}

/**
 * Configure histogram channel
 */
asynStatus ADTimePix::configureHistogramChannel(json& server_j) {
    
    int writeChannel, intNum;
    double doubleNum;
    std::string fileStr;
    
    if (getParameterSafely(ADTimePixWritePrvHst, writeChannel) != asynSuccess) return asynError;
    if (writeChannel == 0) return asynSuccess; // Channel not enabled
    
    // Configure base path and check if this is a streaming connection
    if (getParameterSafely(ADTimePixPrvHstBase, fileStr) != asynSuccess) return asynError;
    server_j["Preview"]["HistogramChannels"][0]["Base"] = fileStr;
    
    // Check if this is a streaming connection (http:// or tcp://)
    bool isStreamingConnection = (fileStr.find("http://") == 0) || (fileStr.find("tcp://") == 0);
    
    if (!isStreamingConnection) {
        // For file connections, include file pattern
        if (getParameterSafely(ADTimePixPrvHstFilePat, fileStr) != asynSuccess) return asynError;
        server_j["Preview"]["HistogramChannels"][0]["FilePattern"] = fileStr;
    }
    
    // Configure format and mode
    if (getParameterSafely(ADTimePixPrvHstFormat, intNum) != asynSuccess) return asynError;
    if (!validateArrayIndex(intNum, IMG_FORMATS.size())) {
        ERR_ARGS("Invalid histogram format index: %d", intNum);
        return asynError;
    }
    server_j["Preview"]["HistogramChannels"][0]["Format"] = IMG_FORMATS[intNum];
    
    if (getParameterSafely(ADTimePixPrvHstMode, intNum) != asynSuccess) return asynError;
    if (!validateArrayIndex(intNum, IMG_MODES.size())) {
        ERR_ARGS("Invalid histogram mode index: %d", intNum);
        return asynError;
    }
    server_j["Preview"]["HistogramChannels"][0]["Mode"] = IMG_MODES[intNum];
    
    // Configure integration settings
    if (getParameterSafely(ADTimePixPrvHstIntSize, intNum) != asynSuccess) return asynError;
    if (validateIntegrationSize(intNum)) {
        server_j["Preview"]["HistogramChannels"][0]["IntegrationSize"] = intNum;
    } else {
        ERR_ARGS("Invalid histogram integration size: %d", intNum);
        return asynError;
    }
    
    if (intNum != 0 && intNum != 1) {
        if (getParameterSafely(ADTimePixPrvHstIntMode, intNum) != asynSuccess) return asynError;
        if (!validateArrayIndex(intNum, INTEGRATION_MODES.size())) {
            ERR_ARGS("Invalid histogram integration mode index: %d", intNum);
            return asynError;
        }
        server_j["Preview"]["HistogramChannels"][0]["IntegrationMode"] = INTEGRATION_MODES[intNum];
    }
    
    // Configure stop on disk limit and queue size
    if (!isStreamingConnection) {
        // For file connections, include stop on disk limit
        if (getParameterSafely(ADTimePixPrvHstStpOnDskLim, intNum) != asynSuccess) return asynError;
        if (!validateArrayIndex(intNum, STOP_ON_DISK_LIMIT.size())) {
            ERR_ARGS("Invalid histogram stop on disk limit index: %d", intNum);
            return asynError;
        }
        server_j["Preview"]["HistogramChannels"][0]["StopMeasurementOnDiskLimit"] = STOP_ON_DISK_LIMIT[intNum];
    }
    
    // Queue size is always needed
    if (getParameterSafely(ADTimePixPrvHstQueueSize, intNum) != asynSuccess) return asynError;
    server_j["Preview"]["HistogramChannels"][0]["QueueSize"] = intNum;
    
    // Configure histogram-specific parameters
    if (getParameterSafely(ADTimePixPrvHstNumBins, intNum) != asynSuccess) return asynError;
    server_j["Preview"]["HistogramChannels"][0]["NumberOfBins"] = intNum;
    
    if (getParameterSafely(ADTimePixPrvHstBinWidth, doubleNum) != asynSuccess) return asynError;
    server_j["Preview"]["HistogramChannels"][0]["BinWidth"] = doubleNum;
    
    if (getParameterSafely(ADTimePixPrvHstOffset, doubleNum) != asynSuccess) return asynError;
    server_j["Preview"]["HistogramChannels"][0]["Offset"] = doubleNum;
    
    return asynSuccess;
}

/**
 * Send configuration to server with retry logic
 */
asynStatus ADTimePix::sendConfiguration(const json& config) {
    
    std::string server = this->serverURL + "/server/destination";
    
    // Log configuration for debugging
    printf("server=%s\n", config.dump(3, ' ', true).c_str());
    
    // Send HTTP request with timeout
    cpr::Response r = ADTimePix3ServalHttp::putJson(server, config.dump(), 10000); // 10 second timeout
    
    setIntegerParam(ADTimePixHttpCode, r.status_code);
    setStringParam(ADTimePixWriteMsg, r.text.c_str());
    
    if (r.status_code != 200) {
        logHttpFailure("sendConfiguration PUT /server/destination", "PUT", server, (long)r.status_code, r.text);
        return asynError;
    }

    return asynSuccess;
}

/**
 * Read Measurement.Config from SERVAL (GET /measurement/config).
 * Populates Stem (Scan, VirtualDetector) and TimeOfFlight PVs.
 * SERVAL 4.1.x; no-op or partial if endpoint/config not present.
 */
asynStatus ADTimePix::getMeasurementConfig() {
    std::string url = this->serverURL + std::string("/measurement/config");
    cpr::Response r = ADTimePix3ServalHttp::getJson(url, 5000);
    if (r.status_code != 200) {
        LOG_ARGS("GET %s failed: %li (Measurement.Config may not be supported): %s", url.c_str(), r.status_code,
                 trimHttpBodyForLog(r.text).c_str());
        return asynError;
    }
    try {
        json config_j = json::parse(r.text.c_str());
        if (config_j.contains("Stem") && config_j["Stem"].is_object()) {
            if (config_j["Stem"].contains("Scan") && config_j["Stem"]["Scan"].is_object()) {
                if (config_j["Stem"]["Scan"].contains("Width") && config_j["Stem"]["Scan"]["Width"].is_number_integer())
                    setIntegerParam(ADTimePixStemScanWidth, config_j["Stem"]["Scan"]["Width"].get<int>());
                if (config_j["Stem"]["Scan"].contains("Height") && config_j["Stem"]["Scan"]["Height"].is_number_integer())
                    setIntegerParam(ADTimePixStemScanHeight, config_j["Stem"]["Scan"]["Height"].get<int>());
                if (config_j["Stem"]["Scan"].contains("DwellTime") && config_j["Stem"]["Scan"]["DwellTime"].is_number())
                    setDoubleParam(ADTimePixStemDwellTime, config_j["Stem"]["Scan"]["DwellTime"].get<double>());
            }
            if (config_j["Stem"].contains("VirtualDetector") && config_j["Stem"]["VirtualDetector"].is_object()) {
                if (config_j["Stem"]["VirtualDetector"].contains("RadiusOuter") && config_j["Stem"]["VirtualDetector"]["RadiusOuter"].is_number_integer())
                    setIntegerParam(ADTimePixStemRadiusOuter, config_j["Stem"]["VirtualDetector"]["RadiusOuter"].get<int>());
                if (config_j["Stem"]["VirtualDetector"].contains("RadiusInner") && config_j["Stem"]["VirtualDetector"]["RadiusInner"].is_number_integer())
                    setIntegerParam(ADTimePixStemRadiusInner, config_j["Stem"]["VirtualDetector"]["RadiusInner"].get<int>());
            }
        }
        if (config_j.contains("TimeOfFlight") && config_j["TimeOfFlight"].is_object()) {
            if (config_j["TimeOfFlight"].contains("TdcReference") && config_j["TimeOfFlight"]["TdcReference"].is_array()) {
                std::string refs;
                for (size_t i = 0; i < config_j["TimeOfFlight"]["TdcReference"].size(); ++i) {
                    if (i > 0) refs += ",";
                    if (config_j["TimeOfFlight"]["TdcReference"][i].is_string())
                        refs += config_j["TimeOfFlight"]["TdcReference"][i].get<std::string>();
                }
                setStringParam(ADTimePixTofTdcReference, refs.c_str());
            }
            if (config_j["TimeOfFlight"].contains("Min") && config_j["TimeOfFlight"]["Min"].is_number())
                setDoubleParam(ADTimePixTofMin, config_j["TimeOfFlight"]["Min"].get<double>());
            if (config_j["TimeOfFlight"].contains("Max") && config_j["TimeOfFlight"]["Max"].is_number())
                setDoubleParam(ADTimePixTofMax, config_j["TimeOfFlight"]["Max"].get<double>());
        }
        callParamCallbacks();
    } catch (const std::exception& e) {
        ERR_ARGS("getMeasurementConfig parse error: %s", e.what());
        return asynError;
    }
    return asynSuccess;
}

/**
 * Write Measurement.Config to SERVAL (PUT /measurement/config).
 * Builds config from Stem and TimeOfFlight PVs; merges with existing config so
 * Corrections/Processing are preserved.
 */
asynStatus ADTimePix::sendMeasurementConfig() {
    std::string url_get = this->serverURL + std::string("/measurement/config");
    cpr::Response r = ADTimePix3ServalHttp::getJson(url_get, 5000);
    json config_j;
    if (r.status_code == 200) {
        try {
            config_j = json::parse(r.text.c_str());
        } catch (...) {
            config_j = json::object();
        }
    } else {
        config_j = json::object();
    }
    int iVal;
    double dVal;
    char strVal[256];
    getIntegerParam(ADTimePixStemScanWidth, &iVal);
    config_j["Stem"]["Scan"]["Width"] = iVal;
    getIntegerParam(ADTimePixStemScanHeight, &iVal);
    config_j["Stem"]["Scan"]["Height"] = iVal;
    getDoubleParam(ADTimePixStemDwellTime, &dVal);
    config_j["Stem"]["Scan"]["DwellTime"] = dVal;
    getIntegerParam(ADTimePixStemRadiusOuter, &iVal);
    config_j["Stem"]["VirtualDetector"]["RadiusOuter"] = iVal;
    getIntegerParam(ADTimePixStemRadiusInner, &iVal);
    config_j["Stem"]["VirtualDetector"]["RadiusInner"] = iVal;
    getStringParam(ADTimePixTofTdcReference, sizeof(strVal), strVal);
    std::string refStr(strVal);
    json refArr = json::array();
    size_t start = 0;
    for (;;) {
        size_t pos = refStr.find(',', start);
        std::string part = (pos == std::string::npos) ? refStr.substr(start) : refStr.substr(start, pos - start);
        if (!part.empty()) refArr.push_back(part);
        if (pos == std::string::npos) break;
        start = pos + 1;
    }
    if (refArr.empty()) refArr.push_back("PN0123");
    config_j["TimeOfFlight"]["TdcReference"] = refArr;
    getDoubleParam(ADTimePixTofMin, &dVal);
    config_j["TimeOfFlight"]["Min"] = dVal;
    getDoubleParam(ADTimePixTofMax, &dVal);
    config_j["TimeOfFlight"]["Max"] = dVal;
    cpr::Response put_r = ADTimePix3ServalHttp::putJson(url_get, config_j.dump(), 10000);
    setIntegerParam(ADTimePixHttpCode, put_r.status_code);
    setStringParam(ADTimePixWriteMsg, put_r.text.c_str());
    if (put_r.status_code != 200) {
        logHttpFailure("sendMeasurementConfig PUT /measurement/config", "PUT", url_get, (long)put_r.status_code,
                       put_r.text);
        return asynError;
    }
    return asynSuccess;
}

/**
 * Optimized fileWriter function
 * 
 * This function configures the detector's data output channels (Raw, Image, and Preview) 
 * by building a JSON configuration and sending it to the Serval server via HTTP PUT request.
 * 
 * @return: status
 */
asynStatus ADTimePix::fileWriter(){
    FLOW("Configuring file writer channels");
    
    // Build configuration JSON
    json server_j;
    bool anyChannelConfigured = false;
    
    // Configure raw channels
    asynStatus status = configureRawChannel(0, server_j);
    if (status == asynError) {
        ERR("Failed to configure raw channel 0");
        return asynError;
    }
    if (status == asynSuccess && server_j.contains("Raw")) {
        anyChannelConfigured = true;
    }
    
    status = configureRawChannel(1, server_j);
    if (status == asynError) {
        ERR("Failed to configure raw channel 1");
        return asynError;
    }
    if (status == asynSuccess && server_j.contains("Raw")) {
        anyChannelConfigured = true;
    }
    
    // Configure image channel
    status = configureImageChannel("Image", server_j);
    if (status == asynError) {
        ERR("Failed to configure image channel");
        return asynError;
    }
    if (status == asynSuccess && server_j.contains("Image")) {
        anyChannelConfigured = true;
    }
    
    status = configureImageChannel("Image[1]", server_j);
    if (status == asynError) {
        ERR("Failed to configure image channel 1");
        return asynError;
    }
    if (status == asynSuccess && server_j.contains("Image")) {
        anyChannelConfigured = true;
    }
    
    // Configure preview image channels
    status = configureImageChannel("Preview", server_j);
    if (status == asynError) {
        ERR("Failed to configure preview image channel 0");
        return asynError;
    }
    if (status == asynSuccess && server_j.contains("Preview")) {
        anyChannelConfigured = true;
    }
    
    status = configureImageChannel("Preview[1]", server_j);
    if (status == asynError) {
        ERR("Failed to configure preview image channel 1");
        return asynError;
    }
    if (status == asynSuccess && server_j.contains("Preview")) {
        anyChannelConfigured = true;
    }
    
    // Configure histogram channel
    status = configureHistogramChannel(server_j);
    if (status == asynError) {
        ERR("Failed to configure histogram channel");
        return asynError;
    }
    if (status == asynSuccess && server_j.contains("Preview")) {
        anyChannelConfigured = true;
    }
    
    // Configure preview settings if any preview channel is enabled
    if (server_j.contains("Preview")) {
        status = configurePreviewSettings(server_j);
        if (status == asynError) {
            ERR("Failed to configure preview settings");
            return asynError;
        }
    }
    
    // Check if any channels were configured
    if (!anyChannelConfigured) {
        // During startup, it's normal for no channels to be configured yet
        // Just log an info message and return success
        LOG("No channels are enabled. Skipping file writer configuration.");
        return asynSuccess;
    }
    
    // Send configuration to server
    return sendConfiguration(server_j);
}

/**
 * Initialize detector - used typically for emulator (uploadBPC/uploadDACS instead for real detector if needed)
 * 
 * serverURL:       the URL of the running SERVAL (string)
 * bpc_file:        an absolute path to the binary pixel configuration file (string), tpx3-demo.bpc
 * dacs_file:       an absolute path to the text chips configuration file (string), tpx3-demo.dacs 
 * 
 * @return: status
 */
asynStatus ADTimePix::initCamera(){
    asynStatus status = asynSuccess;
    FLOW("Initializing detector");
    
    std::string config, bpc_file, dacs_file;

    config = this->serverURL + std::string("/detector/config");
    bpc_file = this->serverURL + std::string("/config/load?format=pixelconfig&file=") + std::string("/epics/src/RHEL8/support/areaDetector/ADTimePix/vendor/tpx3-demo.bpc");
    dacs_file = this->serverURL + std::string("/config/load?format=dacs&file=") + std::string("/epics/src/RHEL8/support/areaDetector/ADTimePix/vendor/tpx3-demo.dacs");

    printf("\n\ninitCamera0: http_code = \n");
    cpr::Response r = ADTimePix3ServalHttp::getAuthOnly(bpc_file);
    printf("\n\ninitCamera1: http_code = %li\n", r.status_code);
    printf("Status code bpc_file: %li\n", r.status_code);
    printf("Text bpc_file: %s\n", r.text.c_str());
    setIntegerParam(ADTimePixHttpCode, r.status_code); 
    setStringParam(ADTimePixWriteMsg, r.text.c_str());
    

    r = ADTimePix3ServalHttp::getAuthOnly(dacs_file);
    printf("\n\ninitCamera2: http_code = %li\n", r.status_code);
    printf("Status code dacs_file: %li\n", r.status_code);
    printf("Text dacs_file: %s\n", r.text.c_str()); 
    setIntegerParam(ADTimePixHttpCode, r.status_code);
    setStringParam(ADTimePixWriteMsg, r.text.c_str());   

    // Detector configuration file 
    r = ADTimePix3ServalHttp::get(config);
    printf("\n\ninitCamera3: http_code = %li\n", r.status_code);
    json config_j = json::parse(r.text.c_str());
    config_j["BiasVoltage"] = 103;
    config_j["BiasEnabled"] = true;

    //config_j["Destination"]["Raw"][0]["Base"] = "file:/media/nvme/raw";
    //printf("Text JSON server: %s\n", config_j.dump(3,' ', true).c_str());    

    r = ADTimePix3ServalHttp::putJson(config, config_j.dump());
    printf("\n\ninitCamera4: http_code = %li\n", r.status_code);
    printf("Status code: %li\n", r.status_code);
    printf("Text: %s\n", r.text.c_str());

    return status;
}


/**
 * Timing for acquisition
 * 
 * serverURL:       the URL of the running SERVAL (string)
 * detectorConfig:  the Detector Config to upload (dictionary)
 * bpc_file:        an absolute path to the binary pixel configuration file (string)
 * dacs_file:       an absolute path to the text chips configuration file (string)
 * 
 * @return: status
 */
asynStatus ADTimePix::initAcquisition(){
    asynStatus status = asynSuccess;
    FLOW("Initializing Acquisition");
    
    std::string det_config;
    int intNum;
    double doubleNum, doubleTmp;

    det_config = this->serverURL + std::string("/detector/config");
    cpr::Response r = ADTimePix3ServalHttp::get(det_config);

    if (r.status_code != 200) {
        setIntegerParam(ADTimePixDetConnected,0);
        setStringParam(ADTimePixWriteMsg, r.text.c_str());
    }
    else {
        setIntegerParam(ADTimePixDetConnected,1);
    //    printf("initAcquisition: %s\n", r.text.c_str());

        json config_j = json::parse(r.text.c_str());
        //printf("det_config=%s\n",config_j.dump(3,' ', true).c_str());

        getIntegerParam(ADTriggerMode, &intNum);
        json triggerMode;
        triggerMode[0] = "PEXSTART_NEXSTOP";
        triggerMode[1] = "NEXSTART_PEXSTOP";
        triggerMode[2] = "PEXSTART_TIMERSTOP";
        triggerMode[3] = "NEXSTART_TIMERSTOP";
        triggerMode[4] = "AUTOTRIGSTART_TIMERSTOP";
        triggerMode[5] = "CONTINUOUS";
        triggerMode[6] = "SOFTWARESTART_TIMERSTOP";
        triggerMode[7] = "SOFTWARESTART_SOFTWARESTOP";
        config_j["TriggerMode"] = triggerMode[intNum];

    //    printf("triggerMode=%s\n",triggerMode.dump().c_str());
    //    printf("triggerMode[intNum]=%s, triggMode_num=%d\n",triggerMode[intNum].dump().c_str(), intNum);
        //config_j["TriggerMode"]="AUTOTRIGSTART_TIMERSTOP";


        if (intNum == 5) {    // Continuous mode
            getDoubleParam(ADAcquireTime, &doubleNum);
            config_j["ExposureTime"] = doubleNum;
            config_j["TriggerPeriod"] = doubleNum;
        }
        else {
            getDoubleParam(ADAcquireTime, &doubleNum);
            config_j["ExposureTime"] = doubleNum;
            doubleTmp = doubleNum;
            getDoubleParam(ADAcquirePeriod, &doubleNum);
            if (doubleNum <= doubleTmp + 0.003) {
                doubleNum = doubleTmp + 0.003;
                config_j["TriggerPeriod"] = doubleNum;
            }
            else {
                config_j["TriggerPeriod"] = doubleNum;
            }
        }

        getIntegerParam(ADNumImages, &intNum);
        config_j["nTriggers"] = intNum;

        getIntegerParam(ADTimePixBiasVolt, &intNum);
        config_j["BiasVoltage"] = intNum;

        getIntegerParam(ADTimePixBiasEnable, &intNum);
        json biasEnabled;
        biasEnabled[0] = "false";
        biasEnabled[1] = "true";
        config_j["BiasEnabled"] = biasEnabled[intNum];

        getIntegerParam(ADTimePixChainMode, &intNum);
        json chainMode;
        chainMode[0] = "NONE";
        chainMode[1] = "LEADER";
        chainMode[2] = "FOLLOWER";
        config_j["ChainMode"] = chainMode[intNum];

        getIntegerParam(ADTimePixPolarity, &intNum);
        json polarity;
        polarity[0] = "Positive";
        polarity[1] = "Negative";
        config_j["Polarity"] = polarity[intNum];

        getIntegerParam(ADTimePixTriggerIn, &intNum);
        config_j["TriggerIn"] = intNum;
        getIntegerParam(ADTimePixTriggerOut, &intNum);
        config_j["TriggerOut"] = intNum;

        getDoubleParam(ADTimePixTriggerDelay, &doubleNum);
        config_j["TriggerDelay"] = doubleNum;
        getDoubleParam(ADTimePixGlobalTimestampInterval, &doubleNum);
        config_j["GlobalTimestampInterval"] = doubleNum;

        getIntegerParam(ADTimePixTdc0, &intNum);
        json tdc;
        tdc[0] = "P0123";
        tdc[1] = "N0123";
        tdc[2] = "PN0123";
        tdc[3] = "P0";
        tdc[4] = "N0";
        tdc[5] = "PN0";
        config_j["Tdc"][0] = tdc[intNum];
        getIntegerParam(ADTimePixTdc1, &intNum);
        tdc[0] = "P0123";
        tdc[1] = "N0123";
        tdc[2] = "PN0123";
        tdc[3] = "P0";
        tdc[4] = "N0";
        tdc[5] = "PN0";
        config_j["Tdc"][1] = tdc[intNum];

        getIntegerParam(ADTimePixExternalReferenceClock, &intNum);
        json externalClock;
        externalClock[0] = "false";
        externalClock[1] = "true";
        config_j["ExternalReferenceClock"] = externalClock[intNum];

        getIntegerParam(ADTimePixPeriphClk80, &intNum);
        json peripheralClock80;
        peripheralClock80[0] = "false";
        peripheralClock80[1] = "true";
        config_j["PeriphClk80"] = peripheralClock80[intNum];

        getIntegerParam(ADTimePixLogLevel, &intNum);
        config_j["LogLevel"] = intNum;

        r = ADTimePix3ServalHttp::putJson(det_config, config_j.dump());

        setStringParam(ADTimePixWriteMsg, r.text.c_str());
    }

    callParamCallbacks();

    return status;
}
