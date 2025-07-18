/**
 * This is a driver for the TimePix3 pixel array detector 
 * 
 * Author: Kazimierz Gofron
 * Created On: June, 2022
 * Last EDited: July 20, 2022
 * Copyright (c): 2022 Brookhaven National Laboratory
 * 
 */

// Standard includes
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/stat.h>

// EPICS includes
#include <epicsTime.h>
#include <epicsThread.h>
#include <epicsExit.h>
#include <epicsString.h>
#include <epicsStdio.h>
#include <iocsh.h>
#include <epicsExport.h>


// Area Detector include
#include "ADTimePix.h"

// Error message formatters
#define ERR(msg)                                                                                 \
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "ERROR | %s::%s: %s\n", driverName, functionName, \
              msg)

#define ERR_ARGS(fmt, ...)                                                              \
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "ERROR | %s::%s: " fmt "\n", driverName, \
              functionName, __VA_ARGS__)

// Warning message formatters
#define WARN(msg) \
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "WARN | %s::%s: %s\n", driverName, functionName, msg)

#define WARN_ARGS(fmt, ...)                                                            \
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "WARN | %s::%s: " fmt "\n", driverName, \
              functionName, __VA_ARGS__)

// Log message formatters
#define LOG(msg) \
    asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER, "%s::%s: %s\n", driverName, functionName, msg)

#define LOG_ARGS(fmt, ...)                                                                       \
    asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER, "%s::%s: " fmt "\n", driverName, functionName, \
              __VA_ARGS__)

// Flow message formatters
#define FLOW(msg) \
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, "%s::%s: %s\n", driverName, functionName, msg)

#define FLOW_ARGS(fmt,...) \
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, "%s::%s: " fmt "\n", driverName, functionName, __VA_ARGS__)

#define delim "/"

using namespace std;
using json = nlohmann::json;
using namespace Magick;

// Add any additional namespaces here


const char* driverName = "ADTimePix";

// Add any driver constants here


// -----------------------------------------------------------------------
// ADTimePix Utility Functions (Reporting/Logging/ExternalC)
// -----------------------------------------------------------------------


/*
 * External configuration function for ADTimePix.
 * Envokes the constructor to create a new ADTimePix object
 * This is the function that initializes the driver, and is called in the IOC startup script
 * 
 * NOTE: When implementing a new driver with ADDriverTemplate, your camera may use a different connection method than
 * a const char* serverURL. Just edit the param to fit your device, and make sure to make the same edit to the constructor below
 *
 * @params[in]: all passed into constructor
 * @return:     status
 */
extern "C" int ADTimePixConfig(const char* portName, const char* serverURL, int maxBuffers, size_t maxMemory, int priority, int stackSize){
    new ADTimePix(portName, serverURL, maxBuffers, maxMemory, priority, stackSize);
    return(asynSuccess);
}


/*
 * Callback function called when IOC is terminated.
 * Deletes created object
 *
 * @params[in]: pPvt -> pointer to the ADDRIVERNAMESTANDATD object created in ADTimePixConfig
 * @return:     void
 */
static void exitCallbackC(void* pPvt){
    ADTimePix* pTimePix = (ADTimePix*) pPvt;
    delete(pTimePix);
}


static void timePixCallbackC(void* pPvt){
    ADTimePix* pTimePix = (ADTimePix*) pPvt;
    pTimePix->timePixCallback();
}


/**
 * Simple function that prints all information about a connected camera
 * 
 * @return: void
 */
void ADTimePix::printConnectedDeviceInfo(){
    printf("--------------------------------------\n");
    printf("Connected to ADTimePix device\n");
    printf("--------------------------------------\n");
    // Add any information you wish to print about the device here
    printf("--------------------------------------\n");
}

// -----------------------------------------------------------------------
// std strip quotes around string Functions
// ../ADTimePix.cpp:97:6: error: specializing member ‘std::__cxx11::basic_string<char>::quotes’ requires ‘template<>’ syntax
// -----------------------------------------------------------------------
static string strip_quotes(string str) {
    if (str.length() > 1 ) {
        return str.substr(1, str.length() - 2);
    }
    else
    return str;
}   



/** Checks whether the directory specified exists.
  *
  * This is a convenience function that determines the directory specified exists.
  * It adds a trailing '/' or '\' character to the path if one is not present.
  * It returns true if the directory exists and false if it does not
  */
bool ADTimePix::checkPath(std::string &filePath)
{
    char lastChar;
    struct stat buff;
    int istat;
    size_t len;
    int isDir=0;
    bool pathExists=false;

    len = filePath.size();
    if (len == 0) return false;
    /* If the path contains a trailing '/' or '\' remove it, because Windows won't find
     * the directory if it has that trailing character */
    lastChar = filePath[len-1];

    if (lastChar == '/') {
        filePath.resize(len-1);
    }
    istat = stat(filePath.c_str(), &buff);
    if (!istat) isDir = (S_IFDIR & buff.st_mode);
    if (!istat && isDir) {
        pathExists = true;
    }
    /* Add a terminator even if it did not have one originally */
    filePath.append(delim);
    return pathExists;
}


/** Checks whether the directory specified BCP/DACS/Raw parameter exists.
  *
  * This is a convenience function that determines the directory specified BCP/DACS/Raw Path parameter exists.
  * It sets the value of xxxPathExists to 0 (does not exist) or 1 (exists).
  * It also adds a trailing '/' character to the path if one is not present.
  * Returns a error status if the directory does not exist.
  */
asynStatus ADTimePix::checkBPCPath()
{
    asynStatus status;
    std::string filePath;
    int pathExists;

    getStringParam(ADTimePixBPCFilePath, filePath);
    if (filePath.size() == 0) return asynSuccess;
    pathExists = checkPath(filePath);
    status = pathExists ? asynSuccess : asynError;
    setStringParam(ADTimePixBPCFilePath, filePath);
    setIntegerParam(ADTimePixBPCFilePathExists, pathExists);
    return status;
}

asynStatus ADTimePix::checkDACSPath()
{
    asynStatus status;
    std::string filePath;
    int pathExists;

    getStringParam(ADTimePixDACSFilePath, filePath);
    if (filePath.size() == 0) return asynSuccess;
    pathExists = checkPath(filePath);
    status = pathExists ? asynSuccess : asynError;
    setStringParam(ADTimePixDACSFilePath, filePath);
    setIntegerParam(ADTimePixDACSFilePathExists, pathExists);
    return status;
}

asynStatus ADTimePix::checkRawPath()
{
    asynStatus status;
    std::string filePath, fileOrStream;
    int pathExists = 0;

    getStringParam(ADTimePixRawBase, filePath);
    if (filePath.size() == 0) return asynSuccess;

    if (filePath.size() > 6) {
        if (filePath.compare(0,6,"file:/") == 0) {        // writing raw .tpx3 data to disk
            setIntegerParam(ADTimePixRawStream, 0);
            fileOrStream = filePath.substr(5);
            pathExists = checkPath(fileOrStream);
        }
        else if (filePath.substr(0,7) == "http://") {       // streaming, http://localhost:8085
            setIntegerParam(ADTimePixRawStream, 1);
            fileOrStream = filePath.substr(6);
            pathExists = 1;
        }   
        else if (filePath.substr(0,6) == "tcp://") {       // streaming, tcp://localhost:8085
            setIntegerParam(ADTimePixRawStream, 2);
            fileOrStream = filePath.substr(5);
            pathExists = 1;
        }
        else {
            printf("Raw file path must be file://path_to_raw_folder, or tcp://localhost:8085\n");
            setIntegerParam(ADTimePixRawStream, 3);
            pathExists = 0;
        }
    }

    status = pathExists ? asynSuccess : asynError;
    setStringParam(ADTimePixRawBase, filePath);
    setIntegerParam(ADTimePixRawFilePathExists, pathExists);
    return status;
}

/* Serval 3.3.0 allows writing raw .tpx3 file, and stream data. This is the 2nd Base channel
* to either write .tpx3 file, or stream to socket.
* Operator decides which Raw or Raw1 is used for streaming, and which for .tpx3 file.
*/
asynStatus ADTimePix::checkRaw1Path()
{
    asynStatus status;
    std::string filePath, fileOrStream;
    int pathExists = 0;

    getStringParam(ADTimePixRaw1Base, filePath);
    if (filePath.size() == 0) return asynSuccess;

    if (filePath.size() > 6) {
        if (filePath.compare(0,6,"file:/") == 0) {        // writing raw .tpx3 data to disk
            setIntegerParam(ADTimePixRaw1Stream, 0);
            fileOrStream = filePath.substr(5);
            pathExists = checkPath(fileOrStream);
        }
        else if (filePath.substr(0,7) == "http://") {       // streaming, http://localhost:8085
            setIntegerParam(ADTimePixRaw1Stream, 1);
            fileOrStream = filePath.substr(6);
            pathExists = 1;
        }
        else if (filePath.substr(0,6) == "tcp://") {       // streaming, tcp://localhost:8085
            setIntegerParam(ADTimePixRaw1Stream, 2);
            fileOrStream = filePath.substr(5);
            pathExists = 1;
        }
        else {
            printf("Raw file path must be file://path_to_raw_folder, or tcp://localhost:8085\n");
            setIntegerParam(ADTimePixRaw1Stream, 3);
            pathExists = 0;
        }
    }

    status = pathExists ? asynSuccess : asynError;
    setStringParam(ADTimePixRaw1Base, filePath);
    setIntegerParam(ADTimePixRaw1FilePathExists, pathExists);
    return status;
}

asynStatus ADTimePix::checkImgPath()
{
    asynStatus status;
    std::string filePath, fileOrStream;
    int pathExists = 0;

    getStringParam(ADTimePixImgBase, filePath);
    if (filePath.size() == 0) return asynSuccess;

    if (filePath.size() > 6) {
        if (filePath.compare(0,6,"file:/") == 0) {        // writing images to disk
            fileOrStream = filePath.substr(5);
            pathExists = checkPath(fileOrStream);
        }
        else if (filePath.substr(0,7) == "http://") {       // streaming, http://localhost:8081
            fileOrStream = filePath.substr(6);
            pathExists = 1;
        }        
        else if (filePath.substr(0,6) == "tcp://") {       // streaming, tcp://localhost:8085
            fileOrStream = filePath.substr(5);
            pathExists = 1;
        }
        else {
            printf("Img file path must be file://path_to_img_folder, or tcp://localhost:8088\n");
            pathExists = 0;
        }
    }

    status = pathExists ? asynSuccess : asynError;
    setStringParam(ADTimePixImgBase, filePath);
    setIntegerParam(ADTimePixImgFilePathExists, pathExists);
    return status;
}

asynStatus ADTimePix::checkPrvImgPath()
{
    asynStatus status;
    std::string filePath, fileOrStream;
    int pathExists = 0;

    getStringParam(ADTimePixPrvImgBase, filePath);
    if (filePath.size() == 0) return asynSuccess;

    if (filePath.size() > 6) {
        if (filePath.compare(0,6,"file:/") == 0) {        // writing raw .tpx3 data
            fileOrStream = filePath.substr(5);
            pathExists = checkPath(fileOrStream);
        }
        else if (filePath.substr(0,7) == "http://") {       // streaming, http://localhost:8081
            fileOrStream = filePath.substr(6);
            pathExists = 1;
        }   
        else if (filePath.substr(0,6) == "tcp://") {       // streaming, tcp://localhost:8085
            fileOrStream = filePath.substr(5);
            pathExists = 1;
        }
        else {
            printf("Prv file path must be file://path_to_img_folder, or tcp://localhost:8088\n");
            pathExists = 0;
        }
    }

    status = pathExists ? asynSuccess : asynError;
    setStringParam(ADTimePixPrvImgBase, filePath);
    setIntegerParam(ADTimePixPrvImgFilePathExists, pathExists);
    return status;
}


asynStatus ADTimePix::checkPrvImg1Path()
{
    asynStatus status;
    std::string filePath, fileOrStream;
    int pathExists = 0;

    getStringParam(ADTimePixPrvImg1Base, filePath);
    if (filePath.size() == 0) return asynSuccess;

    if (filePath.size() > 6) {
        if (filePath.compare(0,6,"file:/") == 0) {        // writing raw .tpx3 data
            fileOrStream = filePath.substr(5);
            pathExists = checkPath(fileOrStream);
        }
        else if (filePath.substr(0,7) == "http://") {       // streaming, http://localhost:8081
            fileOrStream = filePath.substr(6);
            pathExists = 1;
        }   
        else if (filePath.substr(0,6) == "tcp://") {       // streaming, tcp://localhost:8085
            fileOrStream = filePath.substr(5);
            pathExists = 1;
        }
        else {
            printf("Prv1 file path must be file://path_to_img_folder, or tcp://localhost:8088\n");
            pathExists = 0;
        }
    }

    status = pathExists ? asynSuccess : asynError;
    setStringParam(ADTimePixPrvImg1Base, filePath);
    setIntegerParam(ADTimePixPrvImg1FilePathExists, pathExists);
    return status;
}

asynStatus ADTimePix::checkPrvHstPath()
{
    asynStatus status;
    std::string filePath, fileOrStream;
    int pathExists = 0;

    getStringParam(ADTimePixPrvHstBase, filePath);
    if (filePath.size() == 0) return asynSuccess;

    if (filePath.size() > 6) {
        if (filePath.compare(0,6,"file:/") == 0) {        // writing raw .tpx3 data
            fileOrStream = filePath.substr(5);
            pathExists = checkPath(fileOrStream);
        }
            else if (filePath.substr(0,7) == "http://") {       // streaming, http://localhost:8081
            fileOrStream = filePath.substr(6);
            pathExists = 1;
        }       
        else if (filePath.substr(0,6) == "tcp://") {       // streaming, tcp://localhost:8085
            fileOrStream = filePath.substr(5);
            pathExists = 1;
        }
        else {
            printf("Prv Hstogram file path must be file://path_to_img_folder, or tcp://localhost:8088\n");
            pathExists = 0;
        }
    }
    
    status = pathExists ? asynSuccess : asynError;
    setStringParam(ADTimePixPrvHstBase, filePath);
    setIntegerParam(ADTimePixPrvHstFilePathExists, pathExists);
    return status;
}

// -----------------------------------------------------------------------
// ADTimePix Connect/Disconnect Functions
// -----------------------------------------------------------------------


/*
 * Function that is used to initialize and connect to the device.
 * 
 * NOTE: Again, it is possible that for your camera, a different connection type is used (such as a product ID [int])
 * Make sure you use the same connection type as passed in the ADTimePixConfig function and in the constructor.
 * 
 * @params[in]: serverURL    -> serial number of camera to connect to. Passed through IOC shell
 * @return:     status          -> success if connected, error if not connected
 */
asynStatus ADTimePix::initialServerCheckConnection(){
    const char* functionName = "initialServerCheckConnection";
    bool connected = false;


    // Implement connecting to the camera here: check welcome URL
    // Usually the vendor provides examples of how to do this with the library/SDK
    // Use GET request and compare if URI status response code is 200.
    cpr::Response r = cpr::Get(cpr::Url{this->serverURL},
                               cpr::Authentication{"user", "pass", cpr::AuthMode::BASIC},
                               cpr::Parameters{{"anon", "true"}, {"key", "value"}});
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
        printf("\n\nCONNECTED to Welcom URI! (Serval running), http_code = %li\n", r.status_code);

        // Check if detector is connected to serval from dashboard URL
        // Both serval, and connection to Tpx3 detector must be successful
        std::string dashboard;

        dashboard = this->serverURL + std::string("/dashboard");
        printf("ServerURL/dashboard =%s\n", dashboard.c_str());
        r = cpr::Get(cpr::Url{dashboard},
                                   cpr::Authentication{"user", "pass", cpr::AuthMode::BASIC},
                                   cpr::Parameters{{"anon", "true"}, {"key", "value"}});

        printf("Status code: %li\n", r.status_code);
        printf("Text:\n %s\n", r.text.c_str());

        json dashboard_j = json::parse(r.text.c_str());
    //    printf("Dashboard text JSON: %s\n", dashboard_j.dump(3,' ', true).c_str());

        // strip double quote from beginning and end of string
        std::string API_Ver, API_TS;
        API_Ver = strip_quotes(dashboard_j["Server"]["SoftwareVersion"].dump()).c_str();
        API_TS = strip_quotes(dashboard_j["Server"]["SoftwareTimestamp"].dump()).c_str();

        //sets Serval Firmware Version, manufacturer, TimeStamp PV
        setStringParam(ADSDKVersion, API_Ver.c_str());
        setStringParam(ADManufacturer, "ASI");
        setStringParam(ADTimePixFWTimeStamp, API_TS.c_str());

        // printf("After Serval Version!\n");

        // Is Detector connected?
        std::string Detector, DetType;
        Detector = dashboard_j["Detector"].dump().c_str();

        if (strcmp(Detector.c_str(), "null")) {
            DetType = strip_quotes(dashboard_j["Detector"]["DetectorType"].dump()).c_str();
            setStringParam(ADTimePixDetType, DetType.c_str());
            setStringParam(ADModel, DetType.c_str());
            setIntegerParam(ADTimePixDetConnected,0);
            printf("Detector CONNECTED, Detector=%s, %d\n", Detector.c_str(), strcmp(Detector.c_str(), "null"));
        }
        else {
            printf("Detector NOT CONNECTED, Detector=%s\n", Detector.c_str());
            setStringParam(ADTimePixDetType, "null");
            setIntegerParam(ADTimePixDetConnected,1);
            connected = false;
        }
    } else {
        setIntegerParam(ADTimePixServalConnected,0);
        setIntegerParam(ADTimePixDetConnected,0);
    }

    if(connected) return asynSuccess;
    else{
        ERR_ARGS("ERROR: Failed to connect to server %s",this->serverURL.c_str());
        return asynError;
    }
    callParamCallbacks();   // Apply to EPICS, at end of file
}

/**
 * Function that updates PV values of dashboard with camera information
 * 
 * @return: status
 */
asynStatus ADTimePix::getDashboard(){
    const char* functionName = "getDashboard";
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
    cpr::Response r = cpr::Get(cpr::Url{dashboard},
                               cpr::Authentication{"user", "pass", cpr::AuthMode::BASIC},
                               cpr::Parameters{{"anon", "true"}, {"key", "value"}});

    if(r.status_code == 200) {
        setIntegerParam(ADTimePixServalConnected,1);
        setIntegerParam(ADTimePixDetConnected,1);

        json dashboard_j = json::parse(r.text.c_str());
        // dashboard_j["Server"]["SoftwareVersion"] = "2.4.2";
        // printf("Text JSON: %s\n", dashboard_j.dump(3,' ', true).c_str());

        // DiskSpace is an empty array until raw file writing selected, and acquisition starts
        if (!dashboard_j["Server"]["DiskSpace"].empty()) {
            setInteger64Param(ADTimePixFreeSpace,   dashboard_j["Server"]["DiskSpace"][0]["FreeSpace"].get<long>());
            setDoubleParam(ADTimePixWriteSpeed,     dashboard_j["Server"]["DiskSpace"][0]["WriteSpeed"].get<double>());
            setInteger64Param(ADTimePixLowerLimit,  dashboard_j["Server"]["DiskSpace"][0]["LowerLimit"].get<long>());
            setIntegerParam(ADTimePixLLimReached,   int(dashboard_j["Server"]["DiskSpace"][0]["DiskLimitReached"]));   // bool->int true->1, falue->0
        }
    } else { // Serval not running
        setIntegerParam(ADTimePixServalConnected,0);
        setIntegerParam(ADTimePixDetConnected,0);
    }
    return status;
}

asynStatus ADTimePix::getHealth(){
    const char* functionName = "getHealth";
    asynStatus status = asynSuccess;
    FLOW("Checking Health");
    std::string health;

    health = this->serverURL + std::string("/detector/health");
    // printf("Health, %s\n", health.c_str());
    cpr::Response r = cpr::Get(cpr::Url{health},
                           cpr::Authentication{"user", "pass", cpr::AuthMode::BASIC},
                           cpr::Parameters{{"anon", "true"}, {"key", "value"}});   

    json health_j = json::parse(r.text.c_str());
    // printf("Text JSON: %s\n", health_j.dump(3,' ', true).c_str());
    // printf("%lf\n", health_j["ChipTemperatures"].get<double>());
    // printf("Chip Temperatures %s, %s\n", health_j["ChipTemperatures"].dump().c_str(), health_j["VDD"][1].dump().c_str());
    
    setDoubleParam(ADTimePixLocalTemp, health_j["LocalTemperature"].get<double>());
    setDoubleParam(ADTimePixFPGATemp, health_j["FPGATemperature"].get<double>());
    setDoubleParam(ADTimePixFan1Speed, health_j["Fan1Speed"].get<double>());
    setDoubleParam(ADTimePixFan2Speed, health_j["Fan2Speed"].get<double>());
    setDoubleParam(ADTimePixBiasVoltage, health_j["BiasVoltage"].get<double>());

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

    cpr::Response r = cpr::Get(cpr::Url{layout_url},
                       cpr::Authentication{"user", "pass", cpr::AuthMode::BASIC});

//    json layout_j = json::parse(r.text.c_str());
//    printf("layout=%s\n",layout_j.dump(3,' ', true).c_str());

//    cpr::Response r = cpr::Put(
//        cpr::Url{layout_url},
//        cpr::Body{json_data},
//        cpr::Header{{"Content-Type", "application/json"}}
//    );

    if (r.status_code != 200) {
        std::cerr << "Request failed with status code: " << r.status_code << std::endl;
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
    cpr::Response r = cpr::Get(cpr::Url{dac_url},
                           cpr::Authentication{"user", "pass", cpr::AuthMode::BASIC},
                           cpr::Parameters{{"anon", "true"}, {"key", "value"}});   

    if (r.status_code != 200) {
        std::cerr << "Request failed with status code: " << r.status_code << std::endl;
        status = asynError;
    }

    json dacsRead_j = json::parse(r.text.c_str());
    dacsRead_j[dac] = value;
    // printf("dacs=%s\n",dacsRead_j.dump(3,' ', true).c_str());
    std::string json_data = dacsRead_j.dump(3,' ', true).c_str();

    r = cpr::Put(
        cpr::Url{dac_url},
        cpr::Body{json_data},
        cpr::Header{{"Content-Type", "application/json"}}
    );

    if (r.status_code != 200) {
        std::cerr << "Request failed with status code: " << r.status_code << std::endl;
        status = asynError;
    }

    return status;
}

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

    if (API_Ver[0] == '4') {    // Serval version 4, Health is an array
        setIntegerParam(chip, ADTimePixChipNTemperature,   data["Health"][0]["ChipTemperatures"][chip].get<int>());
    }
    else {  // Serval version 3 or 2, Health is a dictionary
        setIntegerParam(chip, ADTimePixChipNTemperature,   data["Health"]["ChipTemperatures"][chip].get<int>());
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

asynStatus ADTimePix::getDetector(){
    const char* functionName = "getDetector";
    asynStatus status = asynSuccess;
    FLOW("Reading Detector Health, info, config, layout, chips");
    std::string detector;
    std::string API_Ver;

    getStringParam(ADSDKVersion, API_Ver);

    detector = this->serverURL + std::string("/detector");
    cpr::Response r = cpr::Get(cpr::Url{detector},
                           cpr::Authentication{"user", "pass", cpr::AuthMode::BASIC},
                           cpr::Parameters{{"anon", "true"}, {"key", "value"}});   

    if (r.status_code != 200) {
        setIntegerParam(ADTimePixDetConnected,0);
        setStringParam(ADTimePixWriteMsg, r.text.c_str());
    }
    else {
        setIntegerParam(ADTimePixDetConnected,1);

        json detector_j = json::parse(r.text.c_str());

        // printf("Number of chips=%d\n", detector_j["Info"]["NumberOfChips"].get<int>());

        // Detector health PVs
        if (API_Ver[0] == '4') {    // Serval version 4
//            printf("Serval version 4, %s,%s\n", API_Ver.c_str(), detector_j.dump(3,' ', true).c_str());
            setDoubleParam(ADTimePixLocalTemp, detector_j["Health"][0]["LocalTemperature"].get<double>());
            setDoubleParam(ADTimePixFPGATemp, detector_j["Health"][0]["FPGATemperature"].get<double>());
            setDoubleParam(ADTimePixFan1Speed, detector_j["Health"][0]["Fan1Speed"].get<double>());
            setDoubleParam(ADTimePixFan2Speed, detector_j["Health"][0]["Fan2Speed"].get<double>());
            setDoubleParam(ADTimePixBiasVoltage, detector_j["Health"][0]["BiasVoltage"].get<double>());
            setIntegerParam(ADTimePixHumidity,   detector_j["Health"][0]["Humidity"].get<int>());
            setStringParam(ADTimePixChipTemperature, detector_j["Health"][0]["ChipTemperatures"].dump().c_str());
            setStringParam(ADTimePixVDD, detector_j["Health"][0]["VDD"].dump().c_str());
            setStringParam(ADTimePixAVDD, detector_j["Health"][0]["AVDD"].dump().c_str());

            // VDD, AVDD voltages
            for (int i = 0; i < 3; i++){
                setDoubleParam(i, ADTimePixChipN_VDD,        detector_j["Health"][0]["VDD"][i].get<double>());
                setDoubleParam(i, ADTimePixChipN_AVDD,       detector_j["Health"][0]["AVDD"][i].get<double>());
            }

        }
        else {  // Serval version 3 or 2
            setDoubleParam(ADTimePixLocalTemp, detector_j["Health"]["LocalTemperature"].get<double>());
            setDoubleParam(ADTimePixFPGATemp, detector_j["Health"]["FPGATemperature"].get<double>());
            setDoubleParam(ADTimePixFan1Speed, detector_j["Health"]["Fan1Speed"].get<double>());
            setDoubleParam(ADTimePixFan2Speed, detector_j["Health"]["Fan2Speed"].get<double>());
            setDoubleParam(ADTimePixBiasVoltage, detector_j["Health"]["BiasVoltage"].get<double>());
            setIntegerParam(ADTimePixHumidity,   detector_j["Health"]["Humidity"].get<int>());
            setStringParam(ADTimePixChipTemperature, detector_j["Health"]["ChipTemperatures"].dump().c_str());
            setStringParam(ADTimePixVDD, detector_j["Health"]["VDD"].dump().c_str());
            setStringParam(ADTimePixAVDD, detector_j["Health"]["AVDD"].dump().c_str());

            // VDD, AVDD voltages
            for (int i = 0; i < 3; i++){
                setDoubleParam(i, ADTimePixChipN_VDD,        detector_j["Health"]["VDD"][i].get<double>());
                setDoubleParam(i, ADTimePixChipN_AVDD,       detector_j["Health"]["AVDD"][i].get<double>());
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

        setStringParam(ADTimePixBoardsID,   strip_quotes(detector_j["Info"]["Boards"][0]["ChipboardId"].dump().c_str()));
        setStringParam(ADTimePixBoardsIP,   strip_quotes(detector_j["Info"]["Boards"][0]["IpAddress"].dump().c_str()));
        setStringParam(ADTimePixBoardsCh1,  strip_quotes(detector_j["Info"]["Boards"][0]["Chips"][0].dump().c_str()));
        setStringParam(ADTimePixBoardsCh2,  strip_quotes(detector_j["Info"]["Boards"][0]["Chips"][1].dump().c_str()));
        setStringParam(ADTimePixBoardsCh3,  strip_quotes(detector_j["Info"]["Boards"][0]["Chips"][2].dump().c_str()));
        setStringParam(ADTimePixBoardsCh4,  strip_quotes(detector_j["Info"]["Boards"][0]["Chips"][3].dump().c_str()));

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
        setIntegerParam(ADTimePixBiasEnable,             int(detector_j["Config"]["BiasEnabled"]));         // bool->int true->1, falue->0
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
        setIntegerParam(ADTimePixPeriphClk80,            int(detector_j["Config"]["PeriphClk80"]));          // bool->int true->1, falue->0
        setDoubleParam(ADTimePixTriggerDelay,            detector_j["Config"]["TriggerDelay"].get<double>());
        setStringParam(ADTimePixTdc,                     strip_quotes(detector_j["Config"]["Tdc"].dump().c_str()));
        setDoubleParam(ADTimePixGlobalTimestampInterval, detector_j["Config"]["GlobalTimestampInterval"].get<double>());
        setIntegerParam(ADTimePixExternalReferenceClock, int(detector_j["Config"]["ExternalReferenceClock"]));   // bool->int true->1, falue->0
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
    const char* functionName = "getServer";
    asynStatus status = asynSuccess;
    FLOW("Reading detector streams");
    std::string server;

    // Use the vendor library to collect information about the camera format here, and set the appropriate PVs
    /*
        {
          "Destination" : {
            "Raw" : [ {
              "Base" : "file:///home/kgofron/Downloads",
              "FilePattern" : "raw%Hms_",
              "SplitStrategy" : "SINGLE_FILE",
              "QueueSize" : 16384
            } ],
            "Image" : [ {
              "Base" : "file:///home/kgofron/Downloads/TimePix/20220105-asi-server-236-tpx3/examples/tpx3/data",
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
    cpr::Response r = cpr::Get(cpr::Url{server},
                           cpr::Authentication{"user", "pass", cpr::AuthMode::BASIC},
                           cpr::Parameters{{"anon", "true"}, {"key", "value"}});   

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
                break; // No Image channels
            case 1:
                setIntegerParam(ADTimePixWriteImgRead, 1);
                break; // One Image channel
            default:
                printf("More than two Image channels\n");
                setIntegerParam(ADTimePixWriteImgRead, 1);
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

/**
 * Initialize detector - uplaad Binary Pixel Configuration
 * 
 * serverURL:       the URL of the running SERVAL (string)
 * bpc_file:        an absolute path to the binary pixel configuration file (string), tpx3-demo.bpc
 * 
 * @return: status
 */
asynStatus ADTimePix::uploadBPC(){
    const char* functionName = "uploadBPC";
    asynStatus status = asynSuccess;
    FLOW("Initializing BPC detector information");
    std::string bpc_file, filePath, fileName;

//    bpc_file = this->serverURL + std::string("/config/load?format=pixelconfig&file=") + std::string("/epics/src/RHEL8/support/areaDetector/ADTimePix/vendor/tpx3-demo.bpc");
    getStringParam(ADTimePixBPCFilePath, filePath);
    getStringParam(ADTimePixBPCFileName, fileName);
    bpc_file = this->serverURL + std::string("/config/load?format=pixelconfig&file=") + std::string(filePath) + std::string(fileName);

    cpr::Response r = cpr::Get(cpr::Url{bpc_file},
                           cpr::Authentication{"user", "pass", cpr::AuthMode::BASIC});
    printf("\n\nuploadBPC: http_code = %li\n", r.status_code);
    printf("Text bpc_file: %s\n", r.text.c_str());
    setIntegerParam(ADTimePixHttpCode, r.status_code); 
    setStringParam(ADTimePixWriteMsg, r.text.c_str());

    return status;
}

/**
 * Initialize detector - uplaad Chips DACS
 * 
 * serverURL:       the URL of the running SERVAL (string)
 * dacs_file:       an absolute path to the text chips configuration file (string), tpx3-demo.dacs 
 * 
 * @return: status
 */
asynStatus ADTimePix::uploadDACS(){
    const char* functionName = "uploadDACS";
    asynStatus status = asynSuccess;
    FLOW("Initializing Chips/DACS detector information");
    std::string dacs_file, filePath, fileName;

//    dacs_file = this->serverURL + std::string("/config/load?format=dacs&file=") + std::string("/epics/src/RHEL8/support/areaDetector/ADTimePix/vendor/tpx3-demo.dacs");
    getStringParam(ADTimePixDACSFilePath, filePath);
    getStringParam(ADTimePixDACSFileName, fileName);
    dacs_file = this->serverURL + std::string("/config/load?format=dacs&file=") + std::string(filePath) + std::string(fileName);

    cpr::Response r = cpr::Get(cpr::Url{dacs_file},
                           cpr::Authentication{"user", "pass", cpr::AuthMode::BASIC});

    printf("\nuploadDACS: http_code = %li\n", r.status_code);
    printf("Text dacs_file: %s\n", r.text.c_str()); 
    setIntegerParam(ADTimePixHttpCode, r.status_code);
    setStringParam(ADTimePixWriteMsg, r.text.c_str());   

    return status;
}

/**
 * FileWriter server channels
 * 
 * serverURL:       the URL of the running SERVAL (string)
 * detectorConfig:  the Detector Config to upload (dictionary)
 * 
 * @return: status
 */
asynStatus ADTimePix::fileWriter(){
    const char* functionName = "fileWriter";
    asynStatus status = asynSuccess;
    FLOW("Initializing detector information");
    
    std::string fileStr;
    int intNum, writeChannel, rawStream;
    double doubleNum;

    std::string server;
    server = this->serverURL + std::string("/server/destination");

    // cpr::Response r = cpr::Get(cpr::Url{server},
    //                        cpr::Authentication{"user", "pass", cpr::AuthMode::BASIC},
    //                        cpr::Parameters{{"anon", "true"}, {"key", "value"}});   
    //printf("Text server: %s\n", r.text.c_str());
    // json server_j = json::parse(r.text.c_str());
    //printf("server=%s\n",server_j.dump(3,' ', true).c_str());

    json server_j;
    json imgFormat = {"tiff","pgm","png","jsonimage","jsonhisto"};
    json imgMode = {"count","tot","toa","tof","count_fb"};
    json samplingMode = {"skipOnFrame","skipOnPeriod"};
    json stopOnDiskLimit = {"false","true"};
    json integrationMode = {"sum", "average", "last"};

    getIntegerParam(ADTimePixWriteRaw, &writeChannel);
    getIntegerParam(ADTimePixRawStream, &rawStream);
    if (writeChannel != 0) {
        // Raw
        getStringParam(ADTimePixRawBase, fileStr);
        server_j["Raw"][0]["Base"] = fileStr;
        getStringParam(ADTimePixRawFilePat, fileStr);
        server_j["Raw"][0]["FilePattern"] = fileStr;

        if (rawStream == 0) {       // file:/
            getIntegerParam(ADTimePixRawSplitStrategy, &intNum);
            json splitStrategy = {"single_file","frame"};
            server_j["Raw"][0]["SplitStrategy"] = splitStrategy[intNum];
            getIntegerParam(ADTimePixRawQueueSize, &intNum);
            server_j["Raw"][0]["QueueSize"] = intNum;
        }
    }   

// Serval 3.3.0
    getIntegerParam(ADTimePixWriteRaw1, &writeChannel);
    getIntegerParam(ADTimePixRaw1Stream, &rawStream);
    if (writeChannel != 0) {
        // Raw
        getStringParam(ADTimePixRaw1Base, fileStr);
        server_j["Raw"][1]["Base"] = fileStr;
        getStringParam(ADTimePixRaw1FilePat, fileStr);
        server_j["Raw"][1]["FilePattern"] = fileStr;

        if (rawStream == 0) {       // file:/
            getIntegerParam(ADTimePixRaw1SplitStrategy, &intNum);
            json splitStrategy = {"single_file","frame"};
            server_j["Raw"][1]["SplitStrategy"] = splitStrategy[intNum];
            getIntegerParam(ADTimePixRaw1QueueSize, &intNum);
            server_j["Raw"][1]["QueueSize"] = intNum;
        }
    }

    getIntegerParam(ADTimePixWriteImg, &writeChannel);
    if (writeChannel != 0) {
        // Image
        getStringParam(ADTimePixImgBase, fileStr);
        server_j["Image"][0]["Base"] = fileStr;
        getStringParam(ADTimePixImgFilePat, fileStr);
        server_j["Image"][0]["FilePattern"] = fileStr;

        getIntegerParam(ADTimePixImgFormat, &intNum);
        server_j["Image"][0]["Format"] = imgFormat[intNum];

        getIntegerParam(ADTimePixImgMode, &intNum);
        server_j["Image"][0]["Mode"] = imgMode[intNum];

        getIntegerParam(ADTimePixImgIntSize, &intNum);  // IntegrationSize can only be -1,0,1,..32; 0,1 -> No Integration
        if ((intNum <= 32) && (intNum >= -1)) {
            server_j["Image"][0]["IntegrationSize"] = intNum;
        }
        if ((intNum != 0) && (intNum != 1)) {  // Integration Mode is disabled for IntegrationSize = 0,1
    //        json integrationMode = {"sum", "average", "last"};
            getIntegerParam(ADTimePixImgIntMode, &intNum);
            server_j["Image"][0]["IntegrationMode"] = integrationMode[intNum];
        }

        getIntegerParam(ADTimePixImgStpOnDskLim, &intNum);
        server_j["Image"][0]["StopMeasurementOnDiskLimit"] = stopOnDiskLimit[intNum];

        getIntegerParam(ADTimePixImgQueueSize, &intNum);
        server_j["Image"][0]["QueueSize"] = intNum;
    }

    // Preview
//    getDoubleParam(ADTimePixPrvPeriod, &doubleNum);
//    server_j["Preview"]["Period"] = doubleNum;
//
//    getIntegerParam(ADTimePixPrvSamplingMode, &intNum);
//    server_j["Preview"]["SamplingMode"] = samplingMode[intNum];


    getIntegerParam(ADTimePixWritePrvImg, &writeChannel);
    if (writeChannel != 0) {
        // Preview, ImageChannels[0]

        getDoubleParam(ADTimePixPrvPeriod, &doubleNum);
        server_j["Preview"]["Period"] = doubleNum;

        getIntegerParam(ADTimePixPrvSamplingMode, &intNum);
        server_j["Preview"]["SamplingMode"] = samplingMode[intNum]; 

        getStringParam(ADTimePixPrvImgBase, fileStr);
        server_j["Preview"]["ImageChannels"][0]["Base"] = fileStr;
        getStringParam(ADTimePixPrvImgFilePat, fileStr);
        server_j["Preview"]["ImageChannels"][0]["FilePattern"] = fileStr;

        getIntegerParam(ADTimePixPrvImgFormat, &intNum);
        server_j["Preview"]["ImageChannels"][0]["Format"] = imgFormat[intNum];

        getIntegerParam(ADTimePixPrvImgMode, &intNum);
        server_j["Preview"]["ImageChannels"][0]["Mode"] = imgMode[intNum];

        getIntegerParam(ADTimePixPrvImgIntSize, &intNum);  // IntegrationSize can only be -1,0,1,..32; 0,1 -> No Integration
        if ((intNum <= 32) && (intNum >= -1)) {
            server_j["Preview"]["ImageChannels"][0]["IntegrationSize"] = intNum;
        }
        if ((intNum != 0) && (intNum != 1)) {  // Integration Mode is disabled for IntegrationSize = 0,1
    //        json integrationMode = {"sum", "average", "last"};
            getIntegerParam(ADTimePixPrvImgIntMode, &intNum);
            server_j["Preview"]["ImageChannels"][0]["IntegrationMode"] = integrationMode[intNum];
        }

        getIntegerParam(ADTimePixPrvImgStpOnDskLim, &intNum);
        server_j["Preview"]["ImageChannels"][0]["StopMeasurementOnDiskLimit"] = stopOnDiskLimit[intNum];

        getIntegerParam(ADTimePixPrvImgQueueSize, &intNum);
        server_j["Preview"]["ImageChannels"][0]["QueueSize"] = intNum;
    }

    getIntegerParam(ADTimePixWritePrvImg1, &writeChannel);
    if (writeChannel != 0) {
        // Preview, ImageChannels[1]
        getStringParam(ADTimePixPrvImg1Base, fileStr);
        server_j["Preview"]["ImageChannels"][1]["Base"] = fileStr;
        getStringParam(ADTimePixPrvImg1FilePat, fileStr);
        server_j["Preview"]["ImageChannels"][1]["FilePattern"] = fileStr;

        getIntegerParam(ADTimePixPrvImg1Format, &intNum);
        server_j["Preview"]["ImageChannels"][1]["Format"] = imgFormat[intNum];

        getIntegerParam(ADTimePixPrvImg1Mode, &intNum);
        server_j["Preview"]["ImageChannels"][1]["Mode"] = imgMode[intNum];

        getIntegerParam(ADTimePixPrvImg1IntSize, &intNum);  // IntegrationSize can only be -1,0,1,..32; 0,1 -> No Integration
        if ((intNum <= 32) && (intNum >= -1)) {
            server_j["Preview"]["ImageChannels"][1]["IntegrationSize"] = intNum;
        }
        if ((intNum != 0) && (intNum != 1)) {  // Integration Mode is disabled for IntegrationSize = 0,1
    //        json integrationMode = {"sum", "average", "last"};
            getIntegerParam(ADTimePixPrvImg1IntMode, &intNum);
            server_j["Preview"]["ImageChannels"][1]["IntegrationMode"] = integrationMode[intNum];
        }

        getIntegerParam(ADTimePixPrvImg1StpOnDskLim, &intNum);
        server_j["Preview"]["ImageChannels"][1]["StopMeasurementOnDiskLimit"] = stopOnDiskLimit[intNum];

        getIntegerParam(ADTimePixPrvImg1QueueSize, &intNum);
        server_j["Preview"]["ImageChannels"][1]["QueueSize"] = intNum;
    }

    getIntegerParam(ADTimePixWritePrvHst, &writeChannel);
    if (writeChannel != 0) {
        // Preview Period and SamplingMode are needed if only HistogramChannels are used.
        getDoubleParam(ADTimePixPrvPeriod, &doubleNum);
        server_j["Preview"]["Period"] = doubleNum;

        getIntegerParam(ADTimePixPrvSamplingMode, &intNum);
        server_j["Preview"]["SamplingMode"] = samplingMode[intNum]; 

        // Preview, HistogramChannels[0]
        getStringParam(ADTimePixPrvHstBase, fileStr);
        server_j["Preview"]["HistogramChannels"][0]["Base"] = fileStr;
        getStringParam(ADTimePixPrvHstFilePat, fileStr);
        server_j["Preview"]["HistogramChannels"][0]["FilePattern"] = fileStr;

        getIntegerParam(ADTimePixPrvHstFormat, &intNum);
        server_j["Preview"]["HistogramChannels"][0]["Format"] = imgFormat[intNum];

        getIntegerParam(ADTimePixPrvHstMode, &intNum);
        server_j["Preview"]["HistogramChannels"][0]["Mode"] = imgMode[intNum];

        getIntegerParam(ADTimePixPrvHstIntSize, &intNum);  // IntegrationSize can only be -1,0,1,..32; 0,1 -> No Integration
        if ((intNum <= 32) && (intNum >= -1)) {
            server_j["Preview"]["HistogramChannels"][0]["IntegrationSize"] = intNum;
        }
        if ((intNum != 0) && (intNum != 1)) {  // Integration Mode is disabled for IntegrationSize = 0,1
    //        json integrationMode = {"sum", "average", "last"};
            getIntegerParam(ADTimePixPrvHstIntMode, &intNum);
            server_j["Preview"]["HistogramChannels"][0]["IntegrationMode"] = integrationMode[intNum];
        }

        getIntegerParam(ADTimePixPrvHstStpOnDskLim, &intNum);
        server_j["Preview"]["HistogramChannels"][0]["StopMeasurementOnDiskLimit"] = stopOnDiskLimit[intNum];

        getIntegerParam(ADTimePixPrvHstQueueSize, &intNum);
        server_j["Preview"]["HistogramChannels"][0]["QueueSize"] = intNum;

        getIntegerParam(ADTimePixPrvHstNumBins, &intNum);
        server_j["Preview"]["HistogramChannels"][0]["NumberOfBins"] = intNum;

        getDoubleParam(ADTimePixPrvHstBinWidth, &doubleNum);
        server_j["Preview"]["HistogramChannels"][0]["BinWidth"] = doubleNum;

        getDoubleParam(ADTimePixPrvHstOffset, &doubleNum);
        server_j["Preview"]["HistogramChannels"][0]["Offset"] = doubleNum;
    }    

    printf("server=%s\n",server_j.dump(3,' ', true).c_str());

    cpr::Response r = cpr::Put(cpr::Url{server},
                cpr::Body{server_j.dump().c_str()},                      
                cpr::Header{{"Content-Type", "application/json"}});

    setIntegerParam(ADTimePixHttpCode, r.status_code);
    setStringParam(ADTimePixWriteMsg, r.text.c_str()); 

    return status;
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
    const char* functionName = "initCamera";
    asynStatus status = asynSuccess;
    FLOW("Initializing detector");
    
    std::string config, bpc_file, dacs_file;

    config = this->serverURL + std::string("/detector/config");
    bpc_file = this->serverURL + std::string("/config/load?format=pixelconfig&file=") + std::string("/epics/src/RHEL8/support/areaDetector/ADTimePix/vendor/tpx3-demo.bpc");
    dacs_file = this->serverURL + std::string("/config/load?format=dacs&file=") + std::string("/epics/src/RHEL8/support/areaDetector/ADTimePix/vendor/tpx3-demo.dacs");

    printf("\n\ninitCamera0: http_code = \n");
    cpr::Response r = cpr::Get(cpr::Url{bpc_file},
                           cpr::Authentication{"user", "pass", cpr::AuthMode::BASIC});
    printf("\n\ninitCamera1: http_code = %li\n", r.status_code);
    printf("Status code bpc_file: %li\n", r.status_code);
    printf("Text bpc_file: %s\n", r.text.c_str());
    setIntegerParam(ADTimePixHttpCode, r.status_code); 
    setStringParam(ADTimePixWriteMsg, r.text.c_str());
    

    r = cpr::Get(cpr::Url{dacs_file},
                           cpr::Authentication{"user", "pass", cpr::AuthMode::BASIC});
    printf("\n\ninitCamra2: http_code = %li\n", r.status_code);
    printf("Status code dacs_file: %li\n", r.status_code);
    printf("Text dacs_file: %s\n", r.text.c_str()); 
    setIntegerParam(ADTimePixHttpCode, r.status_code);
    setStringParam(ADTimePixWriteMsg, r.text.c_str());   

    // Detector configuration file 
    r = cpr::Get(cpr::Url{config},
                           cpr::Authentication{"user", "pass", cpr::AuthMode::BASIC},
                           cpr::Parameters{{"anon", "true"}, {"key", "value"}});
    printf("\n\ninitCamera3: http_code = %li\n", r.status_code);
    json config_j = json::parse(r.text.c_str());
    config_j["BiasVoltage"] = 103;
    config_j["BiasEnabled"] = true;

    //config_j["Destination"]["Raw"][0]["Base"] = "file:///home/kgofron/Downloads";
    //printf("Text JSON server: %s\n", config_j.dump(3,' ', true).c_str());    

    r = cpr::Put(cpr::Url{config},
                           cpr::Body{config_j.dump().c_str()},                      
                           cpr::Header{{"Content-Type", "application/json"}});
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
    const char* functionName = "initAcquisition";
    asynStatus status = asynSuccess;
    FLOW("Initializing Acquisition");
    
    std::string det_config;
    int intNum;
    double doubleNum, doubleTmp;

    det_config = this->serverURL + std::string("/detector/config");
    cpr::Response r = cpr::Get(cpr::Url{det_config},
                           cpr::Authentication{"user", "pass", cpr::AuthMode::BASIC},
                           cpr::Parameters{{"anon", "true"}, {"key", "value"}});

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

        r = cpr::Put(cpr::Url{det_config},
                    cpr::Body{config_j.dump().c_str()},
                    cpr::Header{{"Content-Type", "application/json"}});

        setStringParam(ADTimePixWriteMsg, r.text.c_str());
    }

    callParamCallbacks();

    return status;
}

// -----------------------------------------------------------------------
// ADTimePix Acquisition Functions
// -----------------------------------------------------------------------


/*
#####################################################################################################################
#
# The next two functions can be used when a seperate image acquisition thread is required by the driver. 
# Some vendor software already creates its own acquisition thread for asynchronous use, but if not this
# must be used. By default, the acquireStart() function is written to not use these. If they are needed, 
# find the call to tpx3Callback in acquireStart(), and change it to startImageAcquisitionThread
#
#####################################################################################################################
*/




/**
 * Function responsible for starting camera image acqusition. First, check if there is a
 * camera connected. Then, set camera values by reading from PVs. Then, we execute the 
 * Acquire Start command. if this command was successful, image acquisition started.
 * 
 * @return: status  -> error if no device, camera values not set, or execute command fails. Otherwise, success
 */
asynStatus ADTimePix::acquireStart(){
    const char* functionName = "acquireStart";
    asynStatus status = asynSuccess;

    setIntegerParam(ADStatus, ADStatusAcquire);

    epicsThreadOpts opts = EPICS_THREAD_OPTS_INIT;
    opts.joinable = 1;

    string startMeasurementURL = this->serverURL + std::string("/measurement/start");
    cpr::Response r = cpr::Get(cpr::Url{startMeasurementURL},
                           cpr::Authentication{"user", "pass", cpr::AuthMode::BASIC},
                           cpr::Parameters{{"anon", "true"}, {"key", "value"}});

    if (r.status_code != 200){
        ERR("Failed to start measurement!");
        return asynError;
    }

    this->callbackThreadId = epicsThreadCreateOpt("timePixCallback", timePixCallbackC, this, &opts);
    this->acquiring = true;
    
    return status;
}


void ADTimePix::timePixCallback(){

    const char* functionName = "timePixCallback";

    int numImages;
    int imageCounter;
    int imagesAcquired;
    int mode;
    int frameCounter = 0;
    int new_frame_num = 0;
    bool isIdle = false;
    int writeChannel;

    NDArray* pImage;
    int arrayCallbacks;
    epicsTimeStamp startTime, endTime;
    double elapsedTime;
    std::string API_Ver;

    getIntegerParam(ADImageMode, &mode);
    getIntegerParam(NDArrayCallbacks, &arrayCallbacks);

    string measurement = this->serverURL + std::string("/measurement");   
    cpr::Url url = cpr::Url{measurement};
    cpr::Session session;       //  Stateful piece of the library is needed for while loop. This avoids open connections otherwise generated in while loop below.
    session.SetOption(url);
    cpr::ReserveSize reserveSize = cpr::ReserveSize{1024 * 1024 * 4};   // Reserve space for at least 1 million characters
    session.SetOption(reserveSize);
    //session.SetReserveSize(reserveSize);
    cpr::Authentication authentication = cpr::Authentication("user", "pass", cpr::AuthMode::BASIC);
    session.SetOption(authentication);
    cpr::Parameters parameters = cpr::Parameters{{"anon", "true"}, {"key", "value"}};
    session.SetOption(parameters);
    cpr::Response r = session.Get();

    json measurement_j = json::parse(r.text.c_str());

    setIntegerParam(ADTimePixPelRate,           measurement_j["Info"]["PixelEventRate"].get<int>());

    getStringParam(ADSDKVersion, API_Ver);
    if ((API_Ver[0] == '4') || ((API_Ver[0] == '3') && ((API_Ver[2] - '0') >= 2))) {    // Serval 4.0.0 and later; Serval 3.2.0 and later
        setIntegerParam(ADTimePixTdc1Rate,           measurement_j["Info"]["Tdc1EventRate"].get<int>());
        setIntegerParam(ADTimePixTdc2Rate,           measurement_j["Info"]["Tdc2EventRate"].get<int>());
    } else if ((API_Ver[0] == '3') && ((API_Ver[2] - '0') <= 1)) {   // Serval 3.1.0 and 3.0.0
        setIntegerParam(ADTimePixTdc1Rate,           measurement_j["Info"]["TdcEventRate"].get<int>());
    } else {
        printf ("Serval Version not compared, event rate not read\n");
    }

    setInteger64Param(ADTimePixStartTime,       measurement_j["Info"]["StartDateTime"].get<long>());
    setDoubleParam(ADTimePixElapsedTime,        measurement_j["Info"]["ElapsedTime"].get<double>());
    setDoubleParam(ADTimePixTimeLeft,           measurement_j["Info"]["TimeLeft"].get<double>());
    setIntegerParam(ADTimePixFrameCount,        measurement_j["Info"]["FrameCount"].get<int>());
    setIntegerParam(ADTimePixDroppedFrames,     measurement_j["Info"]["DroppedFrames"].get<int>());
    setStringParam(ADTimePixStatus,             measurement_j["Info"]["Status"].dump().c_str());   
    callParamCallbacks();

    while(this->acquiring){

        getIntegerParam(ADNumImages, &numImages);
        getIntegerParam(ADNumImagesCounter, &imageCounter);
        getIntegerParam(NDArrayCounter, &imagesAcquired);
        epicsTimeGetCurrent(&startTime);

        // Wait for new frame
        while(frameCounter == new_frame_num){
            r = session.Get();      // use stateful read to avoid TIME_WAIT "multiplication" of sessions
                        
            measurement_j = json::parse(r.text.c_str());
            setIntegerParam(ADTimePixPelRate,           measurement_j["Info"]["PixelEventRate"].get<int>());
            
            if ((API_Ver[0] == '4') || ((API_Ver[0] == '3') && ((API_Ver[2] - '0') >= 2))) {    // Serval 4.0.0 and later; Serval 3.2.0 and later
                setIntegerParam(ADTimePixTdc1Rate,           measurement_j["Info"]["Tdc1EventRate"].get<int>());
                setIntegerParam(ADTimePixTdc2Rate,           measurement_j["Info"]["Tdc2EventRate"].get<int>());
            } else if ((API_Ver[0] == '3') && ((API_Ver[2] - '0') <= 1)) {   // Serval 3.1.0 and 3.0.0
                setIntegerParam(ADTimePixTdc1Rate,           measurement_j["Info"]["TdcEventRate"].get<int>());
            } else {
                printf ("Serval Version event rate not specified in while loop.\n");
            }

            setInteger64Param(ADTimePixStartTime,       measurement_j["Info"]["StartDateTime"].get<long>());
            setDoubleParam(ADTimePixElapsedTime,        measurement_j["Info"]["ElapsedTime"].get<double>());
            setDoubleParam(ADTimePixTimeLeft,           measurement_j["Info"]["TimeLeft"].get<double>());
            setIntegerParam(ADTimePixFrameCount,        measurement_j["Info"]["FrameCount"].get<int>());
            setIntegerParam(ADTimePixDroppedFrames,     measurement_j["Info"]["DroppedFrames"].get<int>());
            setStringParam(ADTimePixStatus,             measurement_j["Info"]["Status"].dump().c_str());
            callParamCallbacks();
            
            new_frame_num = measurement_j["Info"]["FrameCount"].get<int>();
            if (measurement_j["Info"]["Status"] == "DA_IDLE" || this->acquiring == false) {
                isIdle = true;
                break;
            }

            epicsTimeGetCurrent(&endTime);
            elapsedTime = epicsTimeDiffInSeconds(&endTime, &startTime);     // 0.0006->0.97 s
            // elapsedTime = r.elapsed;                                      // 0.00035 s
            // printf("Elapsed Time = %f\n", elapsedTime);

            epicsThreadSleep(0.1);
        //    epicsThreadSleep(0);
        }
        frameCounter = new_frame_num;

        getIntegerParam(ADTimePixWritePrvImg, &writeChannel);
        if (writeChannel != 0) {
            // Preview, ImageChannels[0]

            if(this->acquiring){
                asynStatus imageStatus = readImage();
    
                callParamCallbacks();
    
                if (imageStatus == asynSuccess) {
                    imageCounter++;
                    imagesAcquired++;
                    pImage = this->pArrays[0];
    
                    /* Put the frame number and time stamp into the buffer */
                    pImage->uniqueId = imageCounter;
                    pImage->timeStamp = startTime.secPastEpoch + startTime.nsec / 1.e9;
                    updateTimeStamp(&pImage->epicsTS);
    
                    /* Get any attributes that have been defined for this driver */
                     this->getAttributes(pImage->pAttributeList);
    
                     if (arrayCallbacks) {
                         /* Call the NDArray callback */
                         FLOW("calling imageData callback");
                         doCallbacksGenericPointer(pImage, NDArrayData, 0);
                     }
                     if(mode == 0){
                         acquireStop();
                     }
                     else if(mode == 1){
                         if (numImages == imageCounter){
                             acquireStop();
                         }
                     }
                     setIntegerParam(ADNumImagesCounter, frameCounter);
                     setIntegerParam(NDArrayCounter, imagesAcquired);
                     callParamCallbacks();
                }
            }
        }

        if (isIdle)  acquireStop();

    }
}



/**
 * Function responsible for stopping camera image acquisition. First check if the camera is connected.
 * If it is, execute the 'AcquireStop' command. Then set the appropriate PV values, and callParamCallbacks
 * 
 * @return: status  -> error if no camera or command fails to execute, success otherwise
 */ 
asynStatus ADTimePix::acquireStop(){
    const char* functionName = "acquireStop";
    asynStatus status;
    std::string API_Ver;

    this->acquiring=false;
    if(this->callbackThreadId != NULL)
        epicsThreadMustJoin(this->callbackThreadId);

    this->callbackThreadId = NULL;

    string stopMeasurementURL = this->serverURL + std::string("/measurement/stop");
    cpr::Response r = cpr::Get(cpr::Url{stopMeasurementURL},
                           cpr::Authentication{"user", "pass", cpr::AuthMode::BASIC},
                           cpr::Parameters{{"anon", "true"}, {"key", "value"}});

    if (r.status_code != 200){
        ERR("Failed to stop measurement!");
        return asynError;
    }

    setIntegerParam(ADStatus, ADStatusIdle);
    setIntegerParam(ADAcquire, 0);
    callParamCallbacks();
    FLOW("Stopping Image Acquisition");

    // Update end measurement values
    string measurementURL = this->serverURL + std::string("/measurement");
    r = cpr::Get(cpr::Url{measurementURL},
            cpr::Authentication{"user", "pass", cpr::AuthMode::BASIC},
            cpr::Parameters{{"anon", "true"}, {"key", "value"}});

    if (r.status_code != 200){
        ERR("Failed to stop measurement!");
        return asynError;
    }

    json measurement_j = json::parse(r.text.c_str());

    setIntegerParam(ADTimePixPelRate,           measurement_j["Info"]["PixelEventRate"].get<int>());

    getStringParam(ADSDKVersion, API_Ver);
    if ((API_Ver[0] == '4') || ((API_Ver[0] == '3') && ((API_Ver[2] - '0') >= 2))) {    // Serval 4.0.0 and later; Serval 3.2.0 and later
        setIntegerParam(ADTimePixTdc1Rate,           measurement_j["Info"]["Tdc1EventRate"].get<int>());
        setIntegerParam(ADTimePixTdc2Rate,           measurement_j["Info"]["Tdc2EventRate"].get<int>());
    } else if ((API_Ver[0] == '3') && ((API_Ver[2] - '0') <= 1)) {   // Serval 3.1.0 and 3.0.0
        setIntegerParam(ADTimePixTdc1Rate,           measurement_j["Info"]["TdcEventRate"].get<int>());
    } else {
        printf ("Serval Version not compared, event rate not read in acquireStop\n");
    }

    setInteger64Param(ADTimePixStartTime,       measurement_j["Info"]["StartDateTime"].get<long>());
    setDoubleParam(ADTimePixElapsedTime,        measurement_j["Info"]["ElapsedTime"].get<double>());
    setDoubleParam(ADTimePixTimeLeft,           measurement_j["Info"]["TimeLeft"].get<double>());
    setIntegerParam(ADTimePixFrameCount,        measurement_j["Info"]["FrameCount"].get<int>());
    setIntegerParam(ADTimePixDroppedFrames,     measurement_j["Info"]["DroppedFrames"].get<int>());
    setStringParam(ADTimePixStatus,             measurement_j["Info"]["Status"].dump().c_str());
    callParamCallbacks();

    return status;
}


// ADD ANY OTHER SETTER CAMERA FUNCTIONS HERE, ADD CALL THEM IN WRITE INT32/FLOAT64


//-------------------------------------------------------------------------
// ADDriver function overwrites
//-------------------------------------------------------------------------

/** Called when asyn clients call pasynOctet->write().
  * This function performs actions for some parameters, including BPC, and Chips/DACS.
  * For all parameters it sets the value in the parameter library and calls any registered callbacks..
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[in] value Address of the string to write.
  * \param[in] nChars Number of characters to write.
  * \param[out] nActual Number of characters actually written. */
asynStatus ADTimePix::writeOctet(asynUser *pasynUser, const char *value,
                                    size_t nChars, size_t *nActual)
{
    int addr;
    int function;
    const char *paramName;
    asynStatus status = asynSuccess;
    const char *functionName = "writeOctet";

    status = parseAsynUser(pasynUser, &function, &addr, &paramName);
    if (status != asynSuccess) return status;

    /* Set the parameter in the parameter library. */
    status = (asynStatus)setStringParam(addr, function, (char *)value);

    if (function == ADTimePixBPCFilePath)  {
        status = this->checkBPCPath();        
    } else if (function == ADTimePixDACSFilePath) {
        status = this->checkDACSPath();
    } else if (function == ADTimePixRawBase) {
        status = this->checkRawPath();
    } else if (function == ADTimePixRaw1Base) {
        status = this->checkRaw1Path();
    } else if (function == ADTimePixImgBase) {
        status = this->checkImgPath();      
    } else if (function == ADTimePixPrvImgBase) {
        status = this->checkPrvImgPath();
    } else if (function == ADTimePixPrvImg1Base) {
        status = this->checkPrvImg1Path();    
    } else if (function == ADTimePixPrvHstBase) {
        status = this->checkPrvHstPath();
    }
     /* Do callbacks so higher layers see any changes */
    status = (asynStatus)callParamCallbacks(addr, addr);

    if (status)
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                  "%s:%s: status=%d, function=%d, paramName=%s, value=%s",
                  driverName, functionName, status, function, paramName, value);
    else
        LOG_ARGS("function=%d, paramName=%s, value=%s", function, paramName, value);
    *nActual = nChars;
    return status;
}

/*
 * Function overwriting ADDriver base function.
 * Takes in a function (PV) changes, and a value it is changing to, and processes the input
 *
 * @params[in]: pasynUser       -> asyn client who requests a write
 * @params[in]: value           -> int32 value to write
 * @return: asynStatus      -> success if write was successful, else failure
 */
asynStatus ADTimePix::writeInt32(asynUser* pasynUser, epicsInt32 value){
    int function = pasynUser->reason;
    int acquiring;
    int status = asynSuccess;
    static const char* functionName = "writeInt32";
    int addr = 0;
    this->getAddress(pasynUser, &addr);

    getIntegerParam(ADAcquire, &acquiring);

    status = setIntegerParam(addr, function, value);
    // start/stop acquisition
    if(function == ADAcquire){
        printf("SAW ACQUIRE CHANGE!, status=%d\n", status);
        if(value && !acquiring){
            FLOW("Entering aquire\n");
            status = acquireStart();
            if(status < 0){
                return asynError;
            }
        }
        if(!value && acquiring){
            FLOW("Entering acquire stop");
            acquireStop();
        }
    }

    else if(function == ADImageMode){
        if(acquiring == 1) acquireStop();
        if(value == 0) {
            setIntegerParam(ADNumImages,1);  // Set number of images to 1 for single mode
            status = initAcquisition();
        }
    }

    else if(function == ADTimePixVthresholdFine) {
        status = writeDac(addr, "Vthreshold_fine", value);
    }

    else if(function == ADTimePixVthresholdCoarse) {
        status = writeDac(addr, "Vthreshold_coarse", value);
    }

    else if (function == ADTimePixWriteRaw || function == ADTimePixWriteRaw1 || function == ADTimePixWriteImg \
        || function == ADTimePixWritePrvImg || function == ADTimePixWritePrvImg1 || function == ADTimePixWritePrvHst) {
       status = getServer();    // Read configured channels from Serval
    }

    else if(function == ADTimePixHealth) { 
        // status = getHealth();
        status = getDashboard();
        status = getDetector();
    //    status = getServer();
    }
    else if(function == ADTimePixWriteBPCFile) { 
        status = uploadBPC();
    }

    else if(function == ADTimePixWriteDACSFile) { 
        status = uploadDACS();
    }

    else if(function == ADTimePixWriteData) { 
        status = fileWriter();
        status = getServer();    // Read configured channels from Serval
    }

    else if(function == ADTimePixDetectorOrientation) {
        status = rotateLayout();
    }

    else if(function == ADTimePixBiasVolt || ADTimePixBiasEnable || ADTimePixTriggerIn || ADTimePixTriggerOut || ADTimePixLogLevel \
                || ADTimePixExternalReferenceClock || ADTimePixChainMode) {  // set and enable bias, log level
        status = initAcquisition();
    }    

    else if(function == ADNumImages || function == ADTriggerMode) { 
        if(function == ADNumImages) {
            int imageMode;
            getIntegerParam(ADImageMode,&imageMode);
            if (imageMode == 0 && value != 1) {status = asynError;
                ERR("Cannot set numImages in single mode");
            }
        }
        if (status == asynSuccess) status = initAcquisition();
    }

    else{
        if (function < ADTIMEPIX_FIRST_PARAM) {
            status = ADDriver::writeInt32(pasynUser, value);
        }
    }

    /* Do callbacks so higher layers see any changes */
	callParamCallbacks(addr);

    // Log status updates
    if(status){
        ERR_ARGS("ERROR status=%d function=%d, value=%d", status, function, value);
        return asynError;
    }
    else LOG_ARGS("function=%d value=%d", function, value);
    return asynSuccess;
}


/*
 * Function overwriting ADDriver base function.
 * Takes in a function (PV) changes, and a value it is changing to, and processes the input
 * This is the same functionality as writeInt32, but for processing doubles.
 *
 * @params[in]: pasynUser       -> asyn client who requests a write
 * @params[in]: value           -> int32 value to write
 * @return: asynStatus      -> success if write was successful, else failure
 */
asynStatus ADTimePix::writeFloat64(asynUser* pasynUser, epicsFloat64 value){
    int function = pasynUser->reason;
    int acquiring;
    int status = asynSuccess;
    static const char* functionName = "writeFloat64";
    getIntegerParam(ADAcquire, &acquiring);

    status = setDoubleParam(function, value);

    if(function == ADAcquireTime || function == ADAcquirePeriod || ADTimePixTriggerDelay || ADTimePixGlobalTimestampInterval){
        if(acquiring) acquireStop();
        status = initAcquisition();
    }

    else{
        if(function < ADTIMEPIX_FIRST_PARAM){
            status = ADDriver::writeFloat64(pasynUser, value);
        }
    }
    callParamCallbacks();

    if(status){
        ERR_ARGS("ERROR status = %d, function =%d, value = %f", status, function, value);
        return asynError;
    }
    else LOG_ARGS("function=%d value=%f", function, value);
    return asynSuccess;
}


/*
 * Function used for reporting ADUVC device and library information to a external
 * log file. The function first prints all libuvc specific information to the file,
 * then continues on to the base ADDriver 'report' function
 * 
 * @params[in]: fp      -> pointer to log file
 * @params[in]: details -> number of details to write to the file
 * @return: void
 */
void ADTimePix::report(FILE* fp, int details){
    const char* functionName = "report";
    int height;
    int width;
    ERR("reporting to external log file");
    if(details > 0){
        fprintf(fp, " -------------------------------------------------------------------\n");
        fprintf(fp, " Connected Device Information\n");
        // GET CAMERA INFORMATION HERE AND PRINT IT TO fp
        getIntegerParam(ADSizeX, &width);
        getIntegerParam(ADSizeY, &height);
        fprintf(fp, " Image Width           ->      %d\n", width);
        fprintf(fp, " Image Height          ->      %d\n", height);
        fprintf(fp, " -------------------------------------------------------------------\n");
        fprintf(fp, "\n");
        
        ADDriver::report(fp, details);
    }
}


asynStatus ADTimePix::readImage()
{
//    Image image;
//    static const string imageEndpoint = "/measurement/image";  // Cache the endpoint
//    static const string URLString = this->serverURL + imageEndpoint;  // Cache the full URL
    static const string URLString = this->serverURL + "/measurement/image";  // Cache the full URL
//    string URLString=this->serverURL + std::string("/measurement/image");

    size_t dims[3];
    int ndims;
    int nrows, ncols;
    ImageType imageType;
    StorageType storageType;
    NDDataType_t dataType;
    NDColorMode_t colorMode;
    NDArrayInfo_t arrayInfo;
    NDArray *pImage = this->pArrays[0];
    int depth;
    const char *map;
    static const char *functionName = "readImage";
    
    try {
        image.read(URLString);
        imageType = image.type();
        depth = image.depth();
        nrows = image.rows();
        ncols = image.columns();
        switch(imageType) {
        case GrayscaleType:
            ndims = 2;
            dims[0] = ncols;
            dims[1] = nrows;
            dims[2] = 0;
            map = "R";
            colorMode = NDColorModeMono;
            break;
        case TrueColorType:
            ndims = 3;
            dims[0] = 3;
            dims[1] = ncols;
            dims[2] = nrows;
            map = "RGB";
            colorMode = NDColorModeRGB1;
            break;
        default:
            ERR_ARGS("unknown ImageType=%d",imageType);    
            return(asynError);
            break;
        }
        switch(depth) {
        case 1:
        case 8:
            dataType = NDUInt8;
            storageType = CharPixel;
            break;
        case 16:
            dataType = NDUInt16;
            storageType = ShortPixel;
            break;
        case 32:
            dataType = NDUInt32;
            storageType = IntegerPixel;
            break;
        default:
            ERR_ARGS("unsupported depth=%d",depth);
            return(asynError);
            break;
        }
        if (pImage) pImage->release();
        this->pArrays[0] = this->pNDArrayPool->alloc(ndims, dims, dataType, 0, NULL);
        pImage = this->pArrays[0];  
        LOG_ARGS("reading URL=%s, dimensions=[%lu,%lu,%lu], ImageType=%d, depth=%d", URLString.c_str(), 
            (unsigned long)dims[0], (unsigned long)dims[1], (unsigned long)dims[2], imageType, depth);
        image.write(0, 0, ncols, nrows, map, storageType, pImage->pData);
        pImage->pAttributeList->add("ColorMode", "Color mode", NDAttrInt32, &colorMode);
        setIntegerParam(ADSizeX, ncols);
        setIntegerParam(NDArraySizeX, ncols);
        setIntegerParam(ADSizeY, nrows);
        setIntegerParam(NDArraySizeY, nrows);
        pImage->getInfo(&arrayInfo);
        setIntegerParam(NDArraySize,  (int)arrayInfo.totalBytes);
        setIntegerParam(NDDataType, dataType);
        setIntegerParam(NDColorMode, colorMode);
    }
    catch(std::exception &error)
    {
        ERR_ARGS("ERROR reading URL=%s",error.what());
        return(asynError);
    }
         
    return(asynSuccess);
}




//----------------------------------------------------------------------------
// ADTimePix Constructor/Destructor
//----------------------------------------------------------------------------

ADTimePix::ADTimePix(const char* portName, const char* serverURL, int maxBuffers, size_t maxMemory, int priority, int stackSize)
    : ADDriver(portName, 4, (int)NUM_TIMEPIX_PARAMS, maxBuffers, maxMemory,
        asynInt64Mask | asynEnumMask,
        asynInt64Mask | asynEnumMask,
        ASYN_MULTIDEVICE | ASYN_CANBLOCK,
        1,
        priority,
        stackSize)
{
    static const char* functionName = "ADTimePix";

    mDetOrientationMap["UP"] =      0;
    mDetOrientationMap["RIGHT"] =   1;
    mDetOrientationMap["DOWN"] =    2;
    mDetOrientationMap["LEFT"] =    3;
    mDetOrientationMap["UP_MIRRORED"] =     4;
    mDetOrientationMap["RIGHT_MIRRORED"] =  5;
    mDetOrientationMap["DOWN_MIRRORED"] =   6;
    mDetOrientationMap["LEFT_MIRRORED"] =   7;

    /* Initialize GraphicsMagick */
    InitializeMagick(NULL);

    this->serverURL = string(serverURL);

    // Call createParam here
  
    // FW timestamp, Detector Type
    createParam(ADTimePixFWTimeStampString,     asynParamOctet,&ADTimePixFWTimeStamp);
    createParam(ADTimePixDetTypeString,         asynParamOctet,&ADTimePixDetType);
    //sets URI http code PV
    createParam(ADTimePixHttpCodeString,        asynParamInt32, &ADTimePixHttpCode);

    // API serval version
    createParam(ADTimePixServerNameString,      asynParamOctet, &ADTimePixServerName);
    createParam(ADTimePixDetConnectedString,    asynParamInt32, &ADTimePixDetConnected);
    createParam(ADTimePixServalConnectedString, asynParamInt32, &ADTimePixServalConnected);

    // Dashboard
    createParam(ADTimePixFreeSpaceString,       asynParamInt64,   &ADTimePixFreeSpace);
    createParam(ADTimePixWriteSpeedString,      asynParamFloat64, &ADTimePixWriteSpeed);
    createParam(ADTimePixLowerLimitString,      asynParamInt64,   &ADTimePixLowerLimit); 
    createParam(ADTimePixLLimReachedString,     asynParamInt32,   &ADTimePixLLimReached);

    // Detector Health (detector/health, detector)
    createParam(ADTimePixLocalTempString,       asynParamFloat64, &ADTimePixLocalTemp);
    createParam(ADTimePixFPGATempString,        asynParamFloat64, &ADTimePixFPGATemp);
    createParam(ADTimePixFan1SpeedString,       asynParamFloat64, &ADTimePixFan1Speed);
    createParam(ADTimePixFan2SpeedString,       asynParamFloat64, &ADTimePixFan2Speed);
    createParam(ADTimePixBiasVoltageString,     asynParamFloat64, &ADTimePixBiasVoltage);
    createParam(ADTimePixHumidityString,        asynParamInt32, &ADTimePixHumidity);
    createParam(ADTimePixChipTemperatureString, asynParamOctet, &ADTimePixChipTemperature);
    createParam(ADTimePixVDDString,             asynParamOctet, &ADTimePixVDD);
    createParam(ADTimePixAVDDString,            asynParamOctet, &ADTimePixAVDD);
    createParam(ADTimePixHealthString,          asynParamInt32, &ADTimePixHealth);

        // Detector Info (detector)
    createParam(ADTimePixIfaceNameString,       asynParamOctet, &ADTimePixIfaceName);
    createParam(ADTimePixSW_versionString,      asynParamOctet, &ADTimePixSW_version);
    createParam(ADTimePixFW_versionString,      asynParamOctet, &ADTimePixFW_version);
    createParam(ADTimePixPixCountString,        asynParamInt32, &ADTimePixPixCount);
    createParam(ADTimePixRowLenString,          asynParamInt32, &ADTimePixRowLen);
    createParam(ADTimePixNumberOfChipsString,   asynParamInt32, &ADTimePixNumberOfChips);
    createParam(ADTimePixNumberOfRowsString,    asynParamInt32, &ADTimePixNumberOfRows);
    createParam(ADTimePixMpxTypeString,         asynParamInt32, &ADTimePixMpxType);

    createParam(ADTimePixBoardsIDString,        asynParamOctet, &ADTimePixBoardsID);
    createParam(ADTimePixBoardsIPString,        asynParamOctet, &ADTimePixBoardsIP);
    createParam(ADTimePixBoardsCh1String,       asynParamOctet, &ADTimePixBoardsCh1);
    createParam(ADTimePixBoardsCh2String,       asynParamOctet, &ADTimePixBoardsCh2);
    createParam(ADTimePixBoardsCh3String,       asynParamOctet, &ADTimePixBoardsCh3);
    createParam(ADTimePixBoardsCh4String,       asynParamOctet, &ADTimePixBoardsCh4);

    createParam(ADTimePixSuppAcqModesString,    asynParamInt32,     &ADTimePixSuppAcqModes);
    createParam(ADTimePixClockReadoutString,    asynParamFloat64,   &ADTimePixClockReadout);
    createParam(ADTimePixMaxPulseCountString,   asynParamInt32,     &ADTimePixMaxPulseCount);
    createParam(ADTimePixMaxPulseHeightString,  asynParamFloat64,   &ADTimePixMaxPulseHeight);
    createParam(ADTimePixMaxPulsePeriodString,  asynParamFloat64,   &ADTimePixMaxPulsePeriod);
    createParam(ADTimePixTimerMaxValString,     asynParamFloat64,   &ADTimePixTimerMaxVal);
    createParam(ADTimePixTimerMinValString,     asynParamFloat64,   &ADTimePixTimerMinVal);
    createParam(ADTimePixTimerStepString,       asynParamFloat64,   &ADTimePixTimerStep);
    createParam(ADTimePixClockTimepixString,    asynParamFloat64,   &ADTimePixClockTimepix);

            // Detector Config
    createParam(ADTimePixFan1PWMString,                     asynParamInt32,     &ADTimePixFan1PWM);
    createParam(ADTimePixFan2PWMString,                     asynParamInt32,     &ADTimePixFan2PWM);      
    createParam(ADTimePixBiasVoltString,                    asynParamInt32,     &ADTimePixBiasVolt);
    createParam(ADTimePixBiasEnableString,                  asynParamInt32,     &ADTimePixBiasEnable);
    createParam(ADTimePixChainModeString,                   asynParamInt32,     &ADTimePixChainMode);
    createParam(ADTimePixTriggerInString,                   asynParamInt32,     &ADTimePixTriggerIn);
    createParam(ADTimePixTriggerOutString,                  asynParamInt32,     &ADTimePixTriggerOut);
    createParam(ADTimePixPolarityString,                    asynParamInt32,     &ADTimePixPolarity);
    createParam(ADTimePixTriggerModeString,                 asynParamOctet,     &ADTimePixTriggerMode);
    createParam(ADTimePixExposureTimeString,                asynParamFloat64,   &ADTimePixExposureTime);  
    createParam(ADTimePixTriggerPeriodString,               asynParamFloat64,   &ADTimePixTriggerPeriod);  
    createParam(ADTimePixnTriggersString,                   asynParamInt32,     &ADTimePixnTriggers);
    createParam(ADTimePixDetectorOrientationString,         asynParamInt32,     &ADTimePixDetectorOrientation);
    createParam(ADTimePixPeriphClk80String,                 asynParamInt32,     &ADTimePixPeriphClk80);
    createParam(ADTimePixTriggerDelayString,                asynParamFloat64,   &ADTimePixTriggerDelay);  
    createParam(ADTimePixTdcString,                         asynParamOctet,     &ADTimePixTdc);
    createParam(ADTimePixTdc0String,                        asynParamInt32,     &ADTimePixTdc0);
    createParam(ADTimePixTdc1String,                        asynParamInt32,     &ADTimePixTdc1);
    createParam(ADTimePixGlobalTimestampIntervalString,     asynParamFloat64,   &ADTimePixGlobalTimestampInterval);             
    createParam(ADTimePixExternalReferenceClockString,      asynParamInt32,     &ADTimePixExternalReferenceClock);          
    createParam(ADTimePixLogLevelString,                    asynParamInt32,     &ADTimePixLogLevel);

    // Detector Chips
    createParam(ADTimePixCP_PLLString,             asynParamInt32, &ADTimePixCP_PLL);
    createParam(ADTimePixDiscS1OFFString,          asynParamInt32, &ADTimePixDiscS1OFF);
    createParam(ADTimePixDiscS1ONString,           asynParamInt32, &ADTimePixDiscS1ON);
    createParam(ADTimePixDiscS2OFFString,          asynParamInt32, &ADTimePixDiscS2OFF);
    createParam(ADTimePixDiscS2ONString,           asynParamInt32, &ADTimePixDiscS2ON);
    createParam(ADTimePixIkrumString,              asynParamInt32, &ADTimePixIkrum);
    createParam(ADTimePixPixelDACString,           asynParamInt32, &ADTimePixPixelDAC);
    createParam(ADTimePixPreampOFFString,          asynParamInt32, &ADTimePixPreampOFF);
    createParam(ADTimePixPreampONString,           asynParamInt32, &ADTimePixPreampON);
    createParam(ADTimePixTPbufferInString,         asynParamInt32, &ADTimePixTPbufferIn);
    createParam(ADTimePixTPbufferOutString,        asynParamInt32, &ADTimePixTPbufferOut);
    createParam(ADTimePixPLL_VcntrlString,         asynParamInt32, &ADTimePixPLL_Vcntrl);
    createParam(ADTimePixVPreampNCASString,        asynParamInt32, &ADTimePixVPreampNCAS);
    createParam(ADTimePixVTPcoarseString,          asynParamInt32, &ADTimePixVTPcoarse);
    createParam(ADTimePixVTPfineString,            asynParamInt32, &ADTimePixVTPfine);
    createParam(ADTimePixVfbkString,               asynParamInt32, &ADTimePixVfbk);
    createParam(ADTimePixVthresholdCoarseString,   asynParamInt32, &ADTimePixVthresholdCoarse);
    createParam(ADTimePixVthresholdFineString,     asynParamInt32, &ADTimePixVthresholdFine);
    createParam(ADTimePixAdjustString,             asynParamInt32, &ADTimePixAdjust);
             
    // Detector Chip Layout
    createParam(ADTimePixLayoutString,              asynParamOctet, &ADTimePixLayout);
    // Detector Chip Temperature, VDD, AVDD
    createParam(ADTimePixChipNTemperatureString,    asynParamInt32, &ADTimePixChipNTemperature);
    createParam(ADTimePixChipN_VDDString,           asynParamFloat64, &ADTimePixChipN_VDD);
    createParam(ADTimePixChipN_AVDDString,          asynParamFloat64, &ADTimePixChipN_AVDD);

    // Files BPC, Chip/DACS
    createParam(ADTimePixBPCFilePathString,                asynParamOctet,  &ADTimePixBPCFilePath);            
    createParam(ADTimePixBPCFilePathExistsString,          asynParamInt32,  &ADTimePixBPCFilePathExists);          
    createParam(ADTimePixBPCFileNameString,                asynParamOctet,  &ADTimePixBPCFileName);            
    createParam(ADTimePixDACSFilePathString,               asynParamOctet,  &ADTimePixDACSFilePath);           
    createParam(ADTimePixDACSFilePathExistsString,         asynParamInt32,  &ADTimePixDACSFilePathExists);             
    createParam(ADTimePixDACSFileNameString,               asynParamOctet,  &ADTimePixDACSFileName); 
    createParam(ADTimePixWriteMsgString,                   asynParamOctet,  &ADTimePixWriteMsg); 
    createParam(ADTimePixWriteBPCFileString,               asynParamInt32,  &ADTimePixWriteBPCFile);     
    createParam(ADTimePixWriteDACSFileString,              asynParamInt32,  &ADTimePixWriteDACSFile); 

    // Server, File Writer channels
    createParam(ADTimePixWriteDataString,                  asynParamInt32,  &ADTimePixWriteData);
    createParam(ADTimePixWriteRawString,                   asynParamInt32,  &ADTimePixWriteRaw);
    createParam(ADTimePixWriteRaw1String,                  asynParamInt32,  &ADTimePixWriteRaw1);   // Serval 3.3.0
    createParam(ADTimePixWriteImgString,                   asynParamInt32,  &ADTimePixWriteImg);
    createParam(ADTimePixWritePrvImgString,                asynParamInt32,  &ADTimePixWritePrvImg);
    createParam(ADTimePixWritePrvImg1String,               asynParamInt32,  &ADTimePixWritePrvImg1);
    createParam(ADTimePixWritePrvHstString,                asynParamInt32,  &ADTimePixWritePrvHst);
    // Server, Read back channels from Serval
    createParam(ADTimePixWriteRawReadString,                   asynParamInt32,  &ADTimePixWriteRawRead);
    createParam(ADTimePixWriteRaw1ReadString,                  asynParamInt32,  &ADTimePixWriteRaw1Read);   // Serval 3.3.0
    createParam(ADTimePixWriteImgReadString,                   asynParamInt32,  &ADTimePixWriteImgRead);
    createParam(ADTimePixWritePrvImgReadString,                asynParamInt32,  &ADTimePixWritePrvImgRead);
    createParam(ADTimePixWritePrvImg1ReadString,               asynParamInt32,  &ADTimePixWritePrvImg1Read);
    createParam(ADTimePixWritePrvHstReadString,                asynParamInt32,  &ADTimePixWritePrvHstRead);

    // Server, Raw
    createParam(ADTimePixRawBaseString,                    asynParamOctet,  &ADTimePixRawBase);               
    createParam(ADTimePixRawFilePatString,                 asynParamOctet,  &ADTimePixRawFilePat);             
    createParam(ADTimePixRawSplitStrategyString,           asynParamInt32,  &ADTimePixRawSplitStrategy);         
    createParam(ADTimePixRawQueueSizeString,               asynParamInt32,  &ADTimePixRawQueueSize);
    createParam(ADTimePixRawFilePathExistsString,          asynParamInt32,  &ADTimePixRawFilePathExists); 
    // Server, Raw; Serval 3.3.0 allows writing raw file and stream.
    createParam(ADTimePixRaw1BaseString,                   asynParamOctet,  &ADTimePixRaw1Base);
    createParam(ADTimePixRaw1FilePatString,                asynParamOctet,  &ADTimePixRaw1FilePat);
    createParam(ADTimePixRaw1SplitStrategyString,          asynParamInt32,  &ADTimePixRaw1SplitStrategy);
    createParam(ADTimePixRaw1QueueSizeString,              asynParamInt32,  &ADTimePixRaw1QueueSize);
    createParam(ADTimePixRaw1FilePathExistsString,         asynParamInt32,  &ADTimePixRaw1FilePathExists);
    // Server, Image
    createParam(ADTimePixImgBaseString,                   asynParamOctet,    &ADTimePixImgBase);              
    createParam(ADTimePixImgFilePatString,                asynParamOctet,    &ADTimePixImgFilePat);            
    createParam(ADTimePixImgFormatString,                 asynParamInt32,    &ADTimePixImgFormat);            
    createParam(ADTimePixImgModeString,                   asynParamInt32,    &ADTimePixImgMode);              
    createParam(ADTimePixImgThsString,                    asynParamOctet,    &ADTimePixImgThs);             
    createParam(ADTimePixImgIntSizeString,                asynParamInt32,    &ADTimePixImgIntSize); 
    createParam(ADTimePixImgIntModeString,                asynParamInt32,    &ADTimePixImgIntMode);            
    createParam(ADTimePixImgStpOnDskLimString,            asynParamInt32,    &ADTimePixImgStpOnDskLim);        
    createParam(ADTimePixImgQueueSizeString,              asynParamInt32,    &ADTimePixImgQueueSize);         
    createParam(ADTimePixImgFilePathExistsString,         asynParamInt32,    &ADTimePixImgFilePathExists);    
    // Server, Preview   
    createParam(ADTimePixPrvPeriodString,                  asynParamFloat64,  &ADTimePixPrvPeriod);        
    createParam(ADTimePixPrvSamplingModeString,            asynParamInt32,    &ADTimePixPrvSamplingMode);  
    // Server, Preview, ImageChannels[0]  
    createParam(ADTimePixPrvImgBaseString,                   asynParamOctet, &ADTimePixPrvImgBase);            
    createParam(ADTimePixPrvImgFilePatString,                asynParamOctet, &ADTimePixPrvImgFilePat);         
    createParam(ADTimePixPrvImgFormatString,                 asynParamInt32, &ADTimePixPrvImgFormat);          
    createParam(ADTimePixPrvImgModeString,                   asynParamInt32, &ADTimePixPrvImgMode);            
    createParam(ADTimePixPrvImgThsString,                    asynParamOctet, &ADTimePixPrvImgThs);            
    createParam(ADTimePixPrvImgIntSizeString,                asynParamInt32, &ADTimePixPrvImgIntSize);
    createParam(ADTimePixPrvImgIntModeString,                asynParamInt32, &ADTimePixPrvImgIntMode);        
    createParam(ADTimePixPrvImgStpOnDskLimString,            asynParamInt32, &ADTimePixPrvImgStpOnDskLim);    
    createParam(ADTimePixPrvImgQueueSizeString,              asynParamInt32, &ADTimePixPrvImgQueueSize);
    createParam(ADTimePixPrvImgFilePathExistsString,         asynParamInt32, &ADTimePixPrvImgFilePathExists);          
    // Server, Preview, ImageChannels[1]   
    createParam(ADTimePixPrvImg1BaseString,                asynParamOctet, &ADTimePixPrvImg1Base);
    createParam(ADTimePixPrvImg1FilePatString,             asynParamOctet, &ADTimePixPrvImg1FilePat);             
    createParam(ADTimePixPrvImg1FormatString,              asynParamInt32, &ADTimePixPrvImg1Format);        
    createParam(ADTimePixPrvImg1ModeString,                asynParamInt32, &ADTimePixPrvImg1Mode);          
    createParam(ADTimePixPrvImg1ThsString,                 asynParamOctet, &ADTimePixPrvImg1Ths);         
    createParam(ADTimePixPrvImg1IntSizeString,             asynParamInt32, &ADTimePixPrvImg1IntSize);    
    createParam(ADTimePixPrvImg1IntModeString,             asynParamInt32, &ADTimePixPrvImg1IntMode);
    createParam(ADTimePixPrvImg1StpOnDskLimString,         asynParamInt32, &ADTimePixPrvImg1StpOnDskLim); 
    createParam(ADTimePixPrvImg1QueueSizeString,           asynParamInt32, &ADTimePixPrvImg1QueueSize);   
    createParam(ADTimePixPrvImg1FilePathExistsString,      asynParamInt32, &ADTimePixPrvImg1FilePathExists); 
    // Server, Preview, HistogramChannels[0]  
    createParam(ADTimePixPrvHstBaseString,                   asynParamOctet,    &ADTimePixPrvHstBase);
    createParam(ADTimePixPrvHstFilePatString,                asynParamOctet,    &ADTimePixPrvHstFilePat);
    createParam(ADTimePixPrvHstFormatString,                 asynParamInt32,    &ADTimePixPrvHstFormat);
    createParam(ADTimePixPrvHstModeString,                   asynParamInt32,    &ADTimePixPrvHstMode);
    createParam(ADTimePixPrvHstThsString,                    asynParamOctet,    &ADTimePixPrvHstThs);
    createParam(ADTimePixPrvHstIntSizeString,                asynParamInt32,    &ADTimePixPrvHstIntSize);
    createParam(ADTimePixPrvHstIntModeString,                asynParamInt32,    &ADTimePixPrvHstIntMode);
    createParam(ADTimePixPrvHstStpOnDskLimString,            asynParamInt32,    &ADTimePixPrvHstStpOnDskLim);
    createParam(ADTimePixPrvHstQueueSizeString,              asynParamInt32,    &ADTimePixPrvHstQueueSize);
    createParam(ADTimePixPrvHstNumBinsString,                asynParamInt32,    &ADTimePixPrvHstNumBins);
    createParam(ADTimePixPrvHstBinWidthString,               asynParamFloat64,  &ADTimePixPrvHstBinWidth);
    createParam(ADTimePixPrvHstOffsetString,                 asynParamFloat64,  &ADTimePixPrvHstOffset);
    createParam(ADTimePixPrvHstFilePathExistsString,         asynParamInt32,    &ADTimePixPrvHstFilePathExists);

    // Measurement
    createParam(ADTimePixPelRateString,                     asynParamInt32,     &ADTimePixPelRate);      
    createParam(ADTimePixTdc1RateString,                    asynParamInt32,     &ADTimePixTdc1Rate);
    createParam(ADTimePixTdc2RateString,                    asynParamInt32,     &ADTimePixTdc2Rate);      
    createParam(ADTimePixStartTimeString,                   asynParamInt64,     &ADTimePixStartTime);    
    createParam(ADTimePixElapsedTimeString,                 asynParamFloat64,   &ADTimePixElapsedTime);  
    createParam(ADTimePixTimeLeftString,                    asynParamFloat64,   &ADTimePixTimeLeft);     
    createParam(ADTimePixFrameCountString,                  asynParamInt32,     &ADTimePixFrameCount);   
    createParam(ADTimePixDroppedFramesString,               asynParamInt32,     &ADTimePixDroppedFrames);
    createParam(ADTimePixStatusString,                      asynParamOctet,     &ADTimePixStatus);       
    
    // BPC Mask
    createParam(ADTimePixBPCString,                  asynParamInt32, &ADTimePixBPC);
    createParam(ADTimePixBPCnString,                 asynParamInt32, &ADTimePixBPCn);
    createParam(ADTimePixBPCmaskedString,            asynParamInt32, &ADTimePixBPCmasked);
    createParam(ADTimePixMaskBPCString,              asynParamInt32, &ADTimePixMaskBPC);
    createParam(ADTimePixMaskOnOffPelString,         asynParamInt32, &ADTimePixMaskOnOffPel);
    createParam(ADTimePixMaskResetString,            asynParamInt32, &ADTimePixMaskReset);
    createParam(ADTimePixMaskMinXString,             asynParamInt32, &ADTimePixMaskMinX);
    createParam(ADTimePixMaskSizeXString,            asynParamInt32, &ADTimePixMaskSizeX);
    createParam(ADTimePixMaskMinYString,             asynParamInt32, &ADTimePixMaskMinY);
    createParam(ADTimePixMaskSizeYString,            asynParamInt32, &ADTimePixMaskSizeY);
    createParam(ADTimePixMaskRadiusString,           asynParamInt32, &ADTimePixMaskRadius);
    createParam(ADTimePixMaskRectangleString,        asynParamInt32, &ADTimePixMaskRectangle);
    createParam(ADTimePixMaskCircleString,           asynParamInt32, &ADTimePixMaskCircle);
    createParam(ADTimePixMaskFileNameString,         asynParamOctet, &ADTimePixMaskFileName);
    createParam(ADTimePixMaskPelString,              asynParamInt32, &ADTimePixMaskPel);
    createParam(ADTimePixMaskWriteString,            asynParamInt32, &ADTimePixMaskWrite);

    // Controls
    createParam(ADTimePixRawStreamString,       asynParamInt32,     &ADTimePixRawStream);
    createParam(ADTimePixRaw1StreamString,      asynParamInt32,     &ADTimePixRaw1Stream);

    //sets driver version
    char versionString[25];
    epicsSnprintf(versionString, sizeof(versionString), "%d.%d.%d", ADTIMEPIX_VERSION, ADTIMEPIX_REVISION, ADTIMEPIX_MODIFICATION);
    setStringParam(NDDriverVersion, versionString);
    setStringParam(ADTimePixServerName, serverURL);

//    callParamCallbacks();   // Apply to EPICS, at end of file

    if(strlen(serverURL) <= 0){
        ERR("Connection failed, abort");
    }
// asynSuccess = 0, so use !0 for true/connected    
    else{
        asynStatus connected = initialServerCheckConnection();
 //       if(!connected){   // readability: in UNIX 0 is success for a command, but in C++ 0 is "false"
        if(connected == asynSuccess) {
            FLOW("Acquiring device information");
        //    getDashboard(serverURL); 
            printf("Dashboard done HERE!\n\n");
        //    getServer();
            printf("Server done HERE!\n\n");
        //    initCamera(); /* Used for testing and emulator, replaced with loadFile for BPC and Chip/DACS */
            printf("initCamera done HERE!\n\n");
        }
    }

     // when epics is exited, delete the instance of this class
    epicsAtExit(exitCallbackC, this);
}


ADTimePix::~ADTimePix(){
    static const char* functionName = "~ADTimePix";
    FLOW("ADTimePix driver exiting");
    disconnect(this->pasynUserSelf);
}


//-------------------------------------------------------------
// ADTimePix ioc shell registration
//-------------------------------------------------------------

/* ADTimePixConfig -> These are the args passed to the constructor in the epics config function */
static const iocshArg ADTimePixConfigArg0 = { "Port name",        iocshArgString };
static const iocshArg ADTimePixConfigArg1 = { "Server URL",       iocshArgString };
static const iocshArg ADTimePixConfigArg2 = { "maxBuffers",       iocshArgInt };
static const iocshArg ADTimePixConfigArg3 = { "maxMemory",        iocshArgInt };
static const iocshArg ADTimePixConfigArg4 = { "priority",         iocshArgInt };
static const iocshArg ADTimePixConfigArg5 = { "stackSize",        iocshArgInt };


/* Array of config args */
static const iocshArg * const ADTimePixConfigArgs[] =
        { &ADTimePixConfigArg0, &ADTimePixConfigArg1, &ADTimePixConfigArg2,
        &ADTimePixConfigArg3, &ADTimePixConfigArg4, &ADTimePixConfigArg5 };


/* what function to call at config */
static void configADTimePixCallFunc(const iocshArgBuf *args){
    ADTimePixConfig(args[0].sval, args[1].sval, args[2].ival, args[3].ival, args[4].ival, args[5].ival);
}


/* information about the configuration function */
static const iocshFuncDef configADTimePix = { "ADTimePixConfig", 6, ADTimePixConfigArgs };


/* IOC register function */
static void ADTimePixRegister(void) {
    iocshRegister(&configADTimePix, configADTimePixCallFunc);
}


/* external function for IOC register */
extern "C" {
    epicsExportRegistrar(ADTimePixRegister);
}
