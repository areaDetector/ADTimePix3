/**
 * Main source file for the ADTimePix EPICS driver 
 * 
 * This file contains functions for connecting and disconnectiong from the camera,
 * for starting and stopping image acquisition, and for controlling all camera functions through
 * EPICS.
 * 
 * Author: 
 * Created On: 
 * 
 * Copyright (c) : 
 * 
 */

// Standard includes
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

#define delim "/"

using namespace std;
using json = nlohmann::json;

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


/** Checks whether the directory specified NDFilePath parameter exists.
  *
  * This is a convenience function that determines the directory specified NDFilePath parameter exists.
  * It sets the value of NDFilePathExists to 0 (does not exist) or 1 (exists).
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


// -----------------------------------------------------------------------
// ADTimePix Connect/Disconnect Functions
// -----------------------------------------------------------------------


/**
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
    printf("Status code: %li\n", r.status_code);
    printf("Header:\n");
    for (const pair<string, string>& kv : r.header) {
        printf("\t%s:%s\n",kv.first.c_str(),kv.second.c_str());
    }
    printf("Text: %s\n", r.text.c_str());

    if(r.status_code == 200) {
        connected = true;
        printf("CONNECTED to Welcom URI! (Serval running), http_code= %li\n\n", r.status_code);
    //    printf("asynSuccess! %d\n\n", asynSuccess);
    }

    setIntegerParam(ADTimePixHttpCode, r.status_code);

    // Check if detector is connected to serval from dashboard URL
    // Both serval, and connection to Tpx3 detector must be successful
    std::string dashboard;

    dashboard = this->serverURL + std::string("/dashboard");
    printf("ServerURL/dashboard=%s\n", dashboard.c_str());
    r = cpr::Get(cpr::Url{dashboard},
                               cpr::Authentication{"user", "pass", cpr::AuthMode::BASIC},
                               cpr::Parameters{{"anon", "true"}, {"key", "value"}});

    printf("Status code: %li\n", r.status_code);
    printf("Text: %s\n", r.text.c_str());

    json dashboard_j = json::parse(r.text.c_str());
    printf("Dashboard text JSON: %s\n", dashboard_j.dump(3,' ', true).c_str());

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
    }
    else {
        printf("Detector NOT CONNECTED, Detector=%s\n", Detector.c_str());
        setStringParam(ADTimePixDetType, "null");
        connected = false;
    }

    if(connected) return asynSuccess;
    else{
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Error: Failed to connect to server %s\n", driverName, functionName, this->serverURL.c_str());
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
    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s::%s Collecting detector information\n", driverName, functionName);
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
    printf("ServerURL/dashboard=%s\n", dashboard.c_str());
    cpr::Response r = cpr::Get(cpr::Url{dashboard},
                               cpr::Authentication{"user", "pass", cpr::AuthMode::BASIC},
                               cpr::Parameters{{"anon", "true"}, {"key", "value"}});

    printf("Status code: %li\n", r.status_code);
    printf("Text: %s\n", r.text.c_str());

    json dashboard_j = json::parse(r.text.c_str());
    // dashboard_j["Server"]["SoftwareVersion"] = "2.4.2";
    printf("Text JSON: %s\n", dashboard_j.dump(3,' ', true).c_str());

    return status;
}

asynStatus ADTimePix::getHealth(){
    const char* functionName = "getHealth";
    asynStatus status = asynSuccess;
    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s::%s Checking Health\n", driverName, functionName);
    std::string health;

    health = this->serverURL + std::string("/detector/health");
    // printf("Health, %s\n", health.c_str());
    cpr::Response r = cpr::Get(cpr::Url{health},
                           cpr::Authentication{"user", "pass", cpr::AuthMode::BASIC},
                           cpr::Parameters{{"anon", "true"}, {"key", "value"}});   

    //printf("Status code: %li\n", r.status_code);

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

asynStatus ADTimePix::getDetector(){
    const char* functionName = "getDetector";
    asynStatus status = asynSuccess;
    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s::%s Reading Detector Health, inof, config, chipcs\n", driverName, functionName);
    std::string detector;

    detector = this->serverURL + std::string("/detector");
    cpr::Response r = cpr::Get(cpr::Url{detector},
                           cpr::Authentication{"user", "pass", cpr::AuthMode::BASIC},
                           cpr::Parameters{{"anon", "true"}, {"key", "value"}});   

    json detector_j = json::parse(r.text.c_str());
    // Detector health PVs
    setDoubleParam(ADTimePixLocalTemp, detector_j["Health"]["LocalTemperature"].get<double>());
    setDoubleParam(ADTimePixFPGATemp, detector_j["Health"]["FPGATemperature"].get<double>());
    setDoubleParam(ADTimePixFan1Speed, detector_j["Health"]["Fan1Speed"].get<double>());
    setDoubleParam(ADTimePixFan2Speed, detector_j["Health"]["Fan2Speed"].get<double>());
    setDoubleParam(ADTimePixBiasVoltage, detector_j["Health"]["BiasVoltage"].get<double>());

    setStringParam(ADTimePixChipTemperature, detector_j["Health"]["ChipTemperatures"].dump().c_str());
    setStringParam(ADTimePixVDD, detector_j["Health"]["VDD"].dump().c_str());
    setStringParam(ADTimePixAVDD, detector_j["Health"]["AVDD"].dump().c_str());

    // Detector Info
    setStringParam(ADTimePixIfaceName,   strip_quotes(detector_j["Info"]["IfaceName"].dump().c_str()));
    setStringParam(ADTimePixChipboardID, strip_quotes(detector_j["Info"]["ChipboardID"].dump().c_str()));
    setStringParam(ADTimePixSW_version,  strip_quotes(detector_j["Info"]["SW_version"].dump().c_str()));
    setStringParam(ADTimePixFW_version,  strip_quotes(detector_j["Info"]["FW_version"].dump().c_str()));

    setStringParam(ADSerialNumber,      strip_quotes(detector_j["Info"]["ChipboardID"].dump().c_str()));
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
    setIntegerParam(ADTimePixBoardsPort, detector_j["Info"]["Boards"][0]["PortNumber"].get<int>());
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
    setStringParam(ADTimePixChainMode,               strip_quotes(detector_j["Config"]["ChainMode"].dump().c_str()));
    setIntegerParam(ADTimePixTriggerIn,              detector_j["Config"]["TriggerIn"].get<int>());
    setIntegerParam(ADTimePixTriggerOut,             detector_j["Config"]["TriggerOut"].get<int>());
    setStringParam(ADTimePixPolarity,                strip_quotes(detector_j["Config"]["Polarity"].dump().c_str()));
    setStringParam(ADTimePixTriggerMode,             strip_quotes(detector_j["Config"]["TriggerMode"].dump().c_str()));
    setDoubleParam(ADTimePixExposureTime,            detector_j["Config"]["ExposureTime"].get<double>());
    setDoubleParam(ADAcquireTime,            detector_j["Config"]["ExposureTime"].get<double>());       // Exposure Time RBV
    setDoubleParam(ADTimePixTriggerPeriod,           detector_j["Config"]["TriggerPeriod"].get<double>());
    setDoubleParam(ADAcquirePeriod,          detector_j["Config"]["TriggerPeriod"].get<double>());     // Exposure Period RBV
    setIntegerParam(ADTimePixnTriggers,              detector_j["Config"]["nTriggers"].get<int>());
    setStringParam(ADTimePixDetectorOrientation,     strip_quotes(detector_j["Config"]["DetectorOrientation"].dump().c_str()));
    setIntegerParam(ADTimePixPeriphClk80,            int(detector_j["Config"]["PeriphClk80"]));          // bool->int true->1, falue->0
    setDoubleParam(ADTimePixTriggerDelay,            detector_j["Config"]["TriggerDelay"].get<double>());
    setStringParam(ADTimePixTdc,                     strip_quotes(detector_j["Config"]["Tdc"].dump().c_str()));
    setDoubleParam(ADTimePixGlobalTimestampInterval, detector_j["Config"]["GlobalTimestampInterval"].get<double>());
    setIntegerParam(ADTimePixExternalReferenceClock, int(detector_j["Config"]["ExternalReferenceClock"]));   // bool->int true->1, falue->0
    setIntegerParam(ADTimePixLogLevel,               detector_j["Config"]["LogLevel"].get<int>());

    // Detector Chips: Chip0
    setIntegerParam(ADTimePixChip0CP_PLL,             detector_j["Chips"][0]["DACs"]["Ibias_CP_PLL"].get<int>());
    setIntegerParam(ADTimePixChip0DiscS1OFF,          detector_j["Chips"][0]["DACs"]["Ibias_DiscS1_OFF"].get<int>());
    setIntegerParam(ADTimePixChip0DiscS1ON,           detector_j["Chips"][0]["DACs"]["Ibias_DiscS1_ON"].get<int>());
    setIntegerParam(ADTimePixChip0DiscS2OFF,          detector_j["Chips"][0]["DACs"]["Ibias_DiscS2_OFF"].get<int>());
    setIntegerParam(ADTimePixChip0DiscS2ON,           detector_j["Chips"][0]["DACs"]["Ibias_DiscS2_ON"].get<int>());
    setIntegerParam(ADTimePixChip0Ikrum,              detector_j["Chips"][0]["DACs"]["Ibias_Ikrum"].get<int>());
    setIntegerParam(ADTimePixChip0PixelDAC,           detector_j["Chips"][0]["DACs"]["Ibias_PixelDAC"].get<int>());
    setIntegerParam(ADTimePixChip0PreampOFF,          detector_j["Chips"][0]["DACs"]["Ibias_Preamp_OFF"].get<int>());
    setIntegerParam(ADTimePixChip0PreampON,           detector_j["Chips"][0]["DACs"]["Ibias_Preamp_ON"].get<int>());
    setIntegerParam(ADTimePixChip0TPbufferIn,         detector_j["Chips"][0]["DACs"]["Ibias_TPbufferIn"].get<int>());
    setIntegerParam(ADTimePixChip0TPbufferOut,        detector_j["Chips"][0]["DACs"]["Ibias_TPbufferOut"].get<int>());
    setIntegerParam(ADTimePixChip0PLL_Vcntrl,         detector_j["Chips"][0]["DACs"]["PLL_Vcntrl"].get<int>());
    setIntegerParam(ADTimePixChip0VPreampNCAS,        detector_j["Chips"][0]["DACs"]["VPreamp_NCAS"].get<int>());
    setIntegerParam(ADTimePixChip0VTPcoarse,          detector_j["Chips"][0]["DACs"]["VTP_coarse"].get<int>());
    setIntegerParam(ADTimePixChip0VTPfine,            detector_j["Chips"][0]["DACs"]["VTP_fine"].get<int>());
    setIntegerParam(ADTimePixChip0Vfbk,               detector_j["Chips"][0]["DACs"]["Vfbk"].get<int>());
    setIntegerParam(ADTimePixChip0VthresholdCoarse,   detector_j["Chips"][0]["DACs"]["Vthreshold_coarse"].get<int>());
    setIntegerParam(ADTimePixChip0VTthresholdFine,    detector_j["Chips"][0]["DACs"]["Vthreshold_fine"].get<int>());
    //setIntegerParam(ADTimePixChip0Adjust,             detector_j["Chips"][0]["Adjust"].get<int>());

    // Detector Chips: Chip1
    setIntegerParam(ADTimePixChip1CP_PLL,             detector_j["Chips"][1]["DACs"]["Ibias_CP_PLL"].get<int>());
    setIntegerParam(ADTimePixChip1DiscS1OFF,          detector_j["Chips"][1]["DACs"]["Ibias_DiscS1_OFF"].get<int>());
    setIntegerParam(ADTimePixChip1DiscS1ON,           detector_j["Chips"][1]["DACs"]["Ibias_DiscS1_ON"].get<int>());
    setIntegerParam(ADTimePixChip1DiscS2OFF,          detector_j["Chips"][1]["DACs"]["Ibias_DiscS2_OFF"].get<int>());
    setIntegerParam(ADTimePixChip1DiscS2ON,           detector_j["Chips"][1]["DACs"]["Ibias_DiscS2_ON"].get<int>());
    setIntegerParam(ADTimePixChip1Ikrum,              detector_j["Chips"][1]["DACs"]["Ibias_Ikrum"].get<int>());
    setIntegerParam(ADTimePixChip1PixelDAC,           detector_j["Chips"][1]["DACs"]["Ibias_PixelDAC"].get<int>());
    setIntegerParam(ADTimePixChip1PreampOFF,          detector_j["Chips"][1]["DACs"]["Ibias_Preamp_OFF"].get<int>());
    setIntegerParam(ADTimePixChip1PreampON,           detector_j["Chips"][1]["DACs"]["Ibias_Preamp_ON"].get<int>());
    setIntegerParam(ADTimePixChip1TPbufferIn,         detector_j["Chips"][1]["DACs"]["Ibias_TPbufferIn"].get<int>());
    setIntegerParam(ADTimePixChip1TPbufferOut,        detector_j["Chips"][1]["DACs"]["Ibias_TPbufferOut"].get<int>());
    setIntegerParam(ADTimePixChip1PLL_Vcntrl,         detector_j["Chips"][1]["DACs"]["PLL_Vcntrl"].get<int>());
    setIntegerParam(ADTimePixChip1VPreampNCAS,        detector_j["Chips"][1]["DACs"]["VPreamp_NCAS"].get<int>());
    setIntegerParam(ADTimePixChip1VTPcoarse,          detector_j["Chips"][1]["DACs"]["VTP_coarse"].get<int>());
    setIntegerParam(ADTimePixChip1VTPfine,            detector_j["Chips"][1]["DACs"]["VTP_fine"].get<int>());
    setIntegerParam(ADTimePixChip1Vfbk,               detector_j["Chips"][1]["DACs"]["Vfbk"].get<int>());
    setIntegerParam(ADTimePixChip1VthresholdCoarse,   detector_j["Chips"][1]["DACs"]["Vthreshold_coarse"].get<int>());
    setIntegerParam(ADTimePixChip1VTthresholdFine,    detector_j["Chips"][1]["DACs"]["Vthreshold_fine"].get<int>());
    //setIntegerParam(ADTimePixChip1Adjust,             detector_j["Chips"][0]["Adjust"].get<int>());

    // Detector Chips: Chip2
    setIntegerParam(ADTimePixChip2CP_PLL,             detector_j["Chips"][2]["DACs"]["Ibias_CP_PLL"].get<int>());
    setIntegerParam(ADTimePixChip2DiscS1OFF,          detector_j["Chips"][2]["DACs"]["Ibias_DiscS1_OFF"].get<int>());
    setIntegerParam(ADTimePixChip2DiscS1ON,           detector_j["Chips"][2]["DACs"]["Ibias_DiscS1_ON"].get<int>());
    setIntegerParam(ADTimePixChip2DiscS2OFF,          detector_j["Chips"][2]["DACs"]["Ibias_DiscS2_OFF"].get<int>());
    setIntegerParam(ADTimePixChip2DiscS2ON,           detector_j["Chips"][2]["DACs"]["Ibias_DiscS2_ON"].get<int>());
    setIntegerParam(ADTimePixChip2Ikrum,              detector_j["Chips"][2]["DACs"]["Ibias_Ikrum"].get<int>());
    setIntegerParam(ADTimePixChip2PixelDAC,           detector_j["Chips"][2]["DACs"]["Ibias_PixelDAC"].get<int>());
    setIntegerParam(ADTimePixChip2PreampOFF,          detector_j["Chips"][2]["DACs"]["Ibias_Preamp_OFF"].get<int>());
    setIntegerParam(ADTimePixChip2PreampON,           detector_j["Chips"][2]["DACs"]["Ibias_Preamp_ON"].get<int>());
    setIntegerParam(ADTimePixChip2TPbufferIn,         detector_j["Chips"][2]["DACs"]["Ibias_TPbufferIn"].get<int>());
    setIntegerParam(ADTimePixChip2TPbufferOut,        detector_j["Chips"][2]["DACs"]["Ibias_TPbufferOut"].get<int>());
    setIntegerParam(ADTimePixChip2PLL_Vcntrl,         detector_j["Chips"][2]["DACs"]["PLL_Vcntrl"].get<int>());
    setIntegerParam(ADTimePixChip2VPreampNCAS,        detector_j["Chips"][2]["DACs"]["VPreamp_NCAS"].get<int>());
    setIntegerParam(ADTimePixChip2VTPcoarse,          detector_j["Chips"][2]["DACs"]["VTP_coarse"].get<int>());
    setIntegerParam(ADTimePixChip2VTPfine,            detector_j["Chips"][2]["DACs"]["VTP_fine"].get<int>());
    setIntegerParam(ADTimePixChip2Vfbk,               detector_j["Chips"][2]["DACs"]["Vfbk"].get<int>());
    setIntegerParam(ADTimePixChip2VthresholdCoarse,   detector_j["Chips"][2]["DACs"]["Vthreshold_coarse"].get<int>());
    setIntegerParam(ADTimePixChip2VTthresholdFine,    detector_j["Chips"][2]["DACs"]["Vthreshold_fine"].get<int>());
    //setIntegerParam(ADTimePixChip2Adjust,             detector_j["Chips"][0]["Adjust"].get<int>());

    // Detector Chips: Chip3
    setIntegerParam(ADTimePixChip3CP_PLL,             detector_j["Chips"][3]["DACs"]["Ibias_CP_PLL"].get<int>());
    setIntegerParam(ADTimePixChip3DiscS1OFF,          detector_j["Chips"][3]["DACs"]["Ibias_DiscS1_OFF"].get<int>());
    setIntegerParam(ADTimePixChip3DiscS1ON,           detector_j["Chips"][3]["DACs"]["Ibias_DiscS1_ON"].get<int>());
    setIntegerParam(ADTimePixChip3DiscS2OFF,          detector_j["Chips"][3]["DACs"]["Ibias_DiscS2_OFF"].get<int>());
    setIntegerParam(ADTimePixChip3DiscS2ON,           detector_j["Chips"][3]["DACs"]["Ibias_DiscS2_ON"].get<int>());
    setIntegerParam(ADTimePixChip3Ikrum,              detector_j["Chips"][3]["DACs"]["Ibias_Ikrum"].get<int>());
    setIntegerParam(ADTimePixChip3PixelDAC,           detector_j["Chips"][3]["DACs"]["Ibias_PixelDAC"].get<int>());
    setIntegerParam(ADTimePixChip3PreampOFF,          detector_j["Chips"][3]["DACs"]["Ibias_Preamp_OFF"].get<int>());
    setIntegerParam(ADTimePixChip3PreampON,           detector_j["Chips"][3]["DACs"]["Ibias_Preamp_ON"].get<int>());
    setIntegerParam(ADTimePixChip3TPbufferIn,         detector_j["Chips"][3]["DACs"]["Ibias_TPbufferIn"].get<int>());
    setIntegerParam(ADTimePixChip3TPbufferOut,        detector_j["Chips"][3]["DACs"]["Ibias_TPbufferOut"].get<int>());
    setIntegerParam(ADTimePixChip3PLL_Vcntrl,         detector_j["Chips"][3]["DACs"]["PLL_Vcntrl"].get<int>());
    setIntegerParam(ADTimePixChip3VPreampNCAS,        detector_j["Chips"][3]["DACs"]["VPreamp_NCAS"].get<int>());
    setIntegerParam(ADTimePixChip3VTPcoarse,          detector_j["Chips"][3]["DACs"]["VTP_coarse"].get<int>());
    setIntegerParam(ADTimePixChip3VTPfine,            detector_j["Chips"][3]["DACs"]["VTP_fine"].get<int>());
    setIntegerParam(ADTimePixChip3Vfbk,               detector_j["Chips"][3]["DACs"]["Vfbk"].get<int>());
    setIntegerParam(ADTimePixChip3VthresholdCoarse,   detector_j["Chips"][3]["DACs"]["Vthreshold_coarse"].get<int>());
    setIntegerParam(ADTimePixChip3VTthresholdFine,    detector_j["Chips"][3]["DACs"]["Vthreshold_fine"].get<int>());
    //setIntegerParam(ADTimePixChip3Adjust,             detector_j["Chips"][0]["Adjust"].get<int>());

    // Detector Chip Layout
    setStringParam(ADTimePixChip0Layout,    detector_j["Layout"][0].dump().c_str());
    setStringParam(ADTimePixChip1Layout,    detector_j["Layout"][1].dump().c_str());
    setStringParam(ADTimePixChip2Layout,    detector_j["Layout"][2].dump().c_str());
    setStringParam(ADTimePixChip3Layout,    detector_j["Layout"][3].dump().c_str());

    // Refresh PV values
    callParamCallbacks();

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
    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s::%s Collecting detector information\n", driverName, functionName);
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

    server = this->serverURL + std::string("/server");
    cpr::Response r = cpr::Get(cpr::Url{server},
                           cpr::Authentication{"user", "pass", cpr::AuthMode::BASIC},
                           cpr::Parameters{{"anon", "true"}, {"key", "value"}});   
    printf("Status code server: %li\n", r.status_code);
    printf("Text server: %s\n", r.text.c_str()); 

    json server_j = json::parse(r.text.c_str());
    server_j["Destination"]["Raw"][0]["Base"] = "file:///home/kgofron/Downloads";
    printf("Text JSON server: %s\n", server_j.dump(3,' ', true).c_str());    


    cpr::Response r3 = cpr::Put(cpr::Url{server},
                           cpr::Body{server_j.dump().c_str()},                      
                           cpr::Header{{"Content-Type", "text/plain"}});

    printf("Status code: %li\n", r3.status_code);
    printf("Text: %s\n", r3.text.c_str());

    return status;
}

/**
 * Initialize detector - emulator
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
    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s::%s Initializing detector information\n", driverName, functionName);
    
    std::string config, bpc_file, dacs_file;

    config = this->serverURL + std::string("/detector/config");
    bpc_file = this->serverURL + std::string("/config/load?format=pixelconfig&file=") + std::string("/epics/src/RHEL8/support/areaDetector/ADTimePix/vendor/tpx3-demo.bpc");
    dacs_file = this->serverURL + std::string("/config/load?format=dacs&file=") + std::string("/epics/src/RHEL8/support/areaDetector/ADTimePix/vendor/tpx3-demo.dacs");

    cpr::Response r = cpr::Get(cpr::Url{bpc_file},
                           cpr::Authentication{"user", "pass", cpr::AuthMode::BASIC});
    printf("Status code bpc_file: %li\n", r.status_code);
    printf("Text bpc_file: %s\n", r.text.c_str());
    setIntegerParam(ADTimePixHttpCode, r.status_code); 
    setStringParam(ADTimePixWriteFileMsg, r.text.c_str());
    

    r = cpr::Get(cpr::Url{dacs_file},
                           cpr::Authentication{"user", "pass", cpr::AuthMode::BASIC});
    printf("Status code dacs_file: %li\n", r.status_code);
    printf("Text dacs_file: %s\n", r.text.c_str()); 
    setIntegerParam(ADTimePixHttpCode, r.status_code);
    setStringParam(ADTimePixWriteFileMsg, r.text.c_str());   

    // Detector configuration file 
    r = cpr::Get(cpr::Url{config},
                           cpr::Authentication{"user", "pass", cpr::AuthMode::BASIC},
                           cpr::Parameters{{"anon", "true"}, {"key", "value"}});   
    json config_j = json::parse(r.text.c_str());
    config_j["BiasVoltage"] = 103;
    config_j["BiasEnabled"] = true;

    //config_j["Destination"]["Raw"][0]["Base"] = "file:///home/kgofron/Downloads";
    //printf("Text JSON server: %s\n", config_j.dump(3,' ', true).c_str());    

    r = cpr::Put(cpr::Url{config},
                           cpr::Body{config_j.dump().c_str()},                      
                           cpr::Header{{"Content-Type", "text/plain"}});

    printf("Status code: %li\n", r.status_code);
    printf("Text: %s\n", r.text.c_str());

    return status;
}

/**
 * Initialize acquisition
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
    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s::%s Initializing detector information\n", driverName, functionName);
    
    std::string config;

    config = this->serverURL + std::string("/detector/config");
    cpr::Response r = cpr::Get(cpr::Url{config},
                           cpr::Authentication{"user", "pass", cpr::AuthMode::BASIC},
                           cpr::Parameters{{"anon", "true"}, {"key", "value"}});   
    printf("Status code server: %li\n", r.status_code);
    printf("Text server: %s\n", r.text.c_str()); 

    json config_j = json::parse(r.text.c_str());
    config_j["Destination"]["Raw"][0]["Base"] = "file:///home/kgofron/Downloads";
    printf("Text JSON server: %s\n", config_j.dump(3,' ', true).c_str());    


    cpr::Response r3 = cpr::Put(cpr::Url{config},
                           cpr::Body{config_j.dump().c_str()},                      
                           cpr::Header{{"Content-Type", "text/plain"}});

    printf("Status code: %li\n", r3.status_code);
    printf("Text: %s\n", r3.text.c_str());

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
    asynStatus status;
    
    return status;
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

    setIntegerParam(ADStatus, ADStatusIdle);
    setIntegerParam(ADAcquire, 0);
    callParamCallbacks();
    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s::%s Stopping Image Acquisition\n", driverName, functionName);
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
    }
     /* Do callbacks so higher layers see any changes */
    status = (asynStatus)callParamCallbacks(addr, addr);

    if (status)
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                  "%s:%s: status=%d, function=%d, paramName=%s, value=%s",
                  driverName, functionName, status, function, paramName, value);
    else
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
              "%s:%s: function=%d, paramName=%s, value=%s\n",
              driverName, functionName, function, paramName, value);
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
    getIntegerParam(ADAcquire, &acquiring);

    status = setIntegerParam(function, value);
    // start/stop acquisition
    if(function == ADAcquire){
        printf("SAW ACQUIRE CHANGE!\n");
        if(value && !acquiring){
            //asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Entering aquire\n", driverName, functionName);
            status = acquireStart();
            if(status < 0){
                return asynError;
            }
        }
        if(!value && acquiring){
            acquireStop();
        }
    }

    else if(function == ADImageMode){
        if(acquiring == 1) acquireStop();
    }

    else if(function == ADTimePixHealth) { 
        // status = getHealth();
        status = getDetector();
    }

    else{
        if (function < ADTIMEPIX_FIRST_PARAM) {
            status = ADDriver::writeInt32(pasynUser, value);
        }
    }
    callParamCallbacks();

    if(status){
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s ERROR status=%d, function=%d, value=%d\n", driverName, functionName, status, function, value);
        return asynError;
    }
    else asynPrint(this->pasynUserSelf, ASYN_TRACEIO_DRIVER, "%s::%s function=%d value=%d\n", driverName, functionName, function, value);
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

    if(function == ADAcquireTime){
        if(acquiring) acquireStop();
    }
    else{
        if(function < ADTIMEPIX_FIRST_PARAM){
            status = ADDriver::writeFloat64(pasynUser, value);
        }
    }
    callParamCallbacks();

    if(status){
        asynPrint(this-> pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s ERROR status = %d, function =%d, value = %f\n", driverName, functionName, status, function, value);
        return asynError;
    }
    else asynPrint(this->pasynUserSelf, ASYN_TRACEIO_DRIVER, "%s::%s function=%d value=%f\n", driverName, functionName, function, value);
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
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s reporting to external log file\n",driverName, functionName);
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


//----------------------------------------------------------------------------
// ADTimePix Constructor/Destructor
//----------------------------------------------------------------------------

ADTimePix::ADTimePix(const char* portName, const char* serverURL, int maxBuffers, size_t maxMemory, int priority, int stackSize )
    : ADDriver(portName, 1, (int)NUM_TIMEPIX_PARAMS, maxBuffers, maxMemory, asynEnumMask, asynEnumMask, ASYN_CANBLOCK, 1, priority, stackSize){
    static const char* functionName = "ADTimePix";


    this->serverURL = string(serverURL);

    // Call createParam here
  
    // FW timestamp, Detector Type
    createParam(ADTimePixFWTimeStampString,     asynParamOctet,&ADTimePixFWTimeStamp);
    createParam(ADTimePixDetTypeString,         asynParamOctet,&ADTimePixDetType);
    //sets URI http code PV
    createParam(ADTimePixHttpCodeString,        asynParamInt32, &ADTimePixHttpCode);

    // API serval version
    createParam(ADTimePixServerNameString,      asynParamOctet, &ADTimePixServer);

    // Detector Health (detector/health, detector)
    createParam(ADTimePixLocalTempString,       asynParamFloat64, &ADTimePixLocalTemp);
    createParam(ADTimePixFPGATempString,        asynParamFloat64, &ADTimePixFPGATemp);
    createParam(ADTimePixFan1SpeedString,       asynParamFloat64, &ADTimePixFan1Speed);
    createParam(ADTimePixFan2SpeedString,       asynParamFloat64, &ADTimePixFan2Speed);
    createParam(ADTimePixBiasVoltageString,     asynParamFloat64, &ADTimePixBiasVoltage);
    createParam(ADTimePixChipTemperatureString, asynParamOctet, &ADTimePixChipTemperature);
    createParam(ADTimePixVDDString,             asynParamOctet, &ADTimePixVDD);
    createParam(ADTimePixAVDDString,            asynParamOctet, &ADTimePixAVDD);
    createParam(ADTimePixHealthString,          asynParamInt32, &ADTimePixHealth);

        // Detector Info (detector)
    createParam(ADTimePixIfaceNameString,       asynParamOctet, &ADTimePixIfaceName);
    createParam(ADTimePixChipboardIDString,     asynParamOctet, &ADTimePixChipboardID);
    createParam(ADTimePixSW_versionString,      asynParamOctet, &ADTimePixSW_version);
    createParam(ADTimePixFW_versionString,      asynParamOctet, &ADTimePixFW_version);
    createParam(ADTimePixPixCountString,        asynParamInt32, &ADTimePixPixCount);
    createParam(ADTimePixRowLenString,          asynParamInt32, &ADTimePixRowLen);
    createParam(ADTimePixNumberOfChipsString,   asynParamInt32, &ADTimePixNumberOfChips);
    createParam(ADTimePixNumberOfRowsString,    asynParamInt32, &ADTimePixNumberOfRows);
    createParam(ADTimePixMpxTypeString,         asynParamInt32, &ADTimePixMpxType);

    createParam(ADTimePixBoardsIDString,        asynParamOctet, &ADTimePixBoardsID);
    createParam(ADTimePixBoardsIPString,        asynParamOctet, &ADTimePixBoardsIP);
    createParam(ADTimePixBoardsPortString,      asynParamInt32, &ADTimePixBoardsPort); 
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
    createParam(ADTimePixChainModeString,                   asynParamOctet,     &ADTimePixChainMode);
    createParam(ADTimePixTriggerInString,                   asynParamInt32,     &ADTimePixTriggerIn);
    createParam(ADTimePixTriggerOutString,                  asynParamInt32,     &ADTimePixTriggerOut);
    createParam(ADTimePixPolarityString,                    asynParamOctet,     &ADTimePixPolarity);
    createParam(ADTimePixTriggerModeString,                 asynParamOctet,     &ADTimePixTriggerMode);
    createParam(ADTimePixExposureTimeString,                asynParamFloat64,   &ADTimePixExposureTime);  
    createParam(ADTimePixTriggerPeriodString,               asynParamFloat64,   &ADTimePixTriggerPeriod);  
    createParam(ADTimePixnTriggersString,                   asynParamInt32,     &ADTimePixnTriggers);
    createParam(ADTimePixDetectorOrientationString,         asynParamOctet,     &ADTimePixDetectorOrientation);      
    createParam(ADTimePixPeriphClk80String,                 asynParamInt32,     &ADTimePixPeriphClk80);
    createParam(ADTimePixTriggerDelayString,                asynParamFloat64,   &ADTimePixTriggerDelay);  
    createParam(ADTimePixTdcString,                         asynParamOctet,     &ADTimePixTdc);
    createParam(ADTimePixGlobalTimestampIntervalString,     asynParamFloat64,   &ADTimePixGlobalTimestampInterval);             
    createParam(ADTimePixExternalReferenceClockString,      asynParamInt32,     &ADTimePixExternalReferenceClock);          
    createParam(ADTimePixLogLevelString,                    asynParamInt32,     &ADTimePixLogLevel);

        // Detector Chips: Chip0
    createParam(ADTimePixChip0CP_PLLString,             asynParamInt32, &ADTimePixChip0CP_PLL);
    createParam(ADTimePixChip0DiscS1OFFString,          asynParamInt32, &ADTimePixChip0DiscS1OFF);    
    createParam(ADTimePixChip0DiscS1ONString,           asynParamInt32, &ADTimePixChip0DiscS1ON);    
    createParam(ADTimePixChip0DiscS2OFFString,          asynParamInt32, &ADTimePixChip0DiscS2OFF);    
    createParam(ADTimePixChip0DiscS2ONString,           asynParamInt32, &ADTimePixChip0DiscS2ON);    
    createParam(ADTimePixChip0IkrumString,              asynParamInt32, &ADTimePixChip0Ikrum);
    createParam(ADTimePixChip0PixelDACString,           asynParamInt32, &ADTimePixChip0PixelDAC);    
    createParam(ADTimePixChip0PreampOFFString,          asynParamInt32, &ADTimePixChip0PreampOFF);    
    createParam(ADTimePixChip0PreampONString,           asynParamInt32, &ADTimePixChip0PreampON);    
    createParam(ADTimePixChip0TPbufferInString,         asynParamInt32, &ADTimePixChip0TPbufferIn);    
    createParam(ADTimePixChip0TPbufferOutString,        asynParamInt32, &ADTimePixChip0TPbufferOut);        
    createParam(ADTimePixChip0PLL_VcntrlString,         asynParamInt32, &ADTimePixChip0PLL_Vcntrl);    
    createParam(ADTimePixChip0VPreampNCASString,        asynParamInt32, &ADTimePixChip0VPreampNCAS);        
    createParam(ADTimePixChip0VTPcoarseString,          asynParamInt32, &ADTimePixChip0VTPcoarse);    
    createParam(ADTimePixChip0VTPfineString,            asynParamInt32, &ADTimePixChip0VTPfine);    
    createParam(ADTimePixChip0VfbkString,               asynParamInt32, &ADTimePixChip0Vfbk);
    createParam(ADTimePixChip0VthresholdCoarseString,   asynParamInt32, &ADTimePixChip0VthresholdCoarse);            
    createParam(ADTimePixChip0VTthresholdFineString,    asynParamInt32, &ADTimePixChip0VTthresholdFine);            
    createParam(ADTimePixChip0AdjustString,             asynParamOctet, &ADTimePixChip0Adjust);
    // Detector Chips: Chip1
    createParam(ADTimePixChip1CP_PLLString,               asynParamInt32, &ADTimePixChip1CP_PLL);
    createParam(ADTimePixChip1DiscS1OFFString,            asynParamInt32, &ADTimePixChip1DiscS1OFF);    
    createParam(ADTimePixChip1DiscS1ONString,             asynParamInt32, &ADTimePixChip1DiscS1ON);    
    createParam(ADTimePixChip1DiscS2OFFString,            asynParamInt32, &ADTimePixChip1DiscS2OFF);    
    createParam(ADTimePixChip1DiscS2ONString,             asynParamInt32, &ADTimePixChip1DiscS2ON);    
    createParam(ADTimePixChip1IkrumString,                asynParamInt32, &ADTimePixChip1Ikrum);
    createParam(ADTimePixChip1PixelDACString,             asynParamInt32, &ADTimePixChip1PixelDAC);    
    createParam(ADTimePixChip1PreampOFFString,            asynParamInt32, &ADTimePixChip1PreampOFF);    
    createParam(ADTimePixChip1PreampONString,             asynParamInt32, &ADTimePixChip1PreampON);    
    createParam(ADTimePixChip1TPbufferInString,           asynParamInt32, &ADTimePixChip1TPbufferIn);    
    createParam(ADTimePixChip1TPbufferOutString,          asynParamInt32, &ADTimePixChip1TPbufferOut);        
    createParam(ADTimePixChip1PLL_VcntrlString,           asynParamInt32, &ADTimePixChip1PLL_Vcntrl);    
    createParam(ADTimePixChip1VPreampNCASString,          asynParamInt32, &ADTimePixChip1VPreampNCAS);        
    createParam(ADTimePixChip1VTPcoarseString,            asynParamInt32, &ADTimePixChip1VTPcoarse);    
    createParam(ADTimePixChip1VTPfineString,              asynParamInt32, &ADTimePixChip1VTPfine);    
    createParam(ADTimePixChip1VfbkString,                 asynParamInt32, &ADTimePixChip1Vfbk);
    createParam(ADTimePixChip1VthresholdCoarseString,     asynParamInt32, &ADTimePixChip1VthresholdCoarse);    
    createParam(ADTimePixChip1VTthresholdFineString,      asynParamInt32, &ADTimePixChip1VTthresholdFine);    
    createParam(ADTimePixChip1AdjustString,               asynParamOctet, &ADTimePixChip1Adjust);
    // Detector Chips: Chip2
    createParam(ADTimePixChip2CP_PLLString,                asynParamInt32, &ADTimePixChip2CP_PLL);
    createParam(ADTimePixChip2DiscS1OFFString,             asynParamInt32, &ADTimePixChip2DiscS1OFF);
    createParam(ADTimePixChip2DiscS1ONString,              asynParamInt32, &ADTimePixChip2DiscS1ON);
    createParam(ADTimePixChip2DiscS2OFFString,             asynParamInt32, &ADTimePixChip2DiscS2OFF);
    createParam(ADTimePixChip2DiscS2ONString,              asynParamInt32, &ADTimePixChip2DiscS2ON);
    createParam(ADTimePixChip2IkrumString,                 asynParamInt32, &ADTimePixChip2Ikrum);
    createParam(ADTimePixChip2PixelDACString,              asynParamInt32, &ADTimePixChip2PixelDAC);
    createParam(ADTimePixChip2PreampOFFString,             asynParamInt32, &ADTimePixChip2PreampOFF);
    createParam(ADTimePixChip2PreampONString,              asynParamInt32, &ADTimePixChip2PreampON);
    createParam(ADTimePixChip2TPbufferInString,            asynParamInt32, &ADTimePixChip2TPbufferIn);
    createParam(ADTimePixChip2TPbufferOutString,           asynParamInt32, &ADTimePixChip2TPbufferOut);
    createParam(ADTimePixChip2PLL_VcntrlString,            asynParamInt32, &ADTimePixChip2PLL_Vcntrl);
    createParam(ADTimePixChip2VPreampNCASString,           asynParamInt32, &ADTimePixChip2VPreampNCAS);
    createParam(ADTimePixChip2VTPcoarseString,             asynParamInt32, &ADTimePixChip2VTPcoarse);
    createParam(ADTimePixChip2VTPfineString,               asynParamInt32, &ADTimePixChip2VTPfine);
    createParam(ADTimePixChip2VfbkString,                  asynParamInt32, &ADTimePixChip2Vfbk);
    createParam(ADTimePixChip2VthresholdCoarseString,      asynParamInt32, &ADTimePixChip2VthresholdCoarse);
    createParam(ADTimePixChip2VTthresholdFineString,       asynParamInt32, &ADTimePixChip2VTthresholdFine);
    createParam(ADTimePixChip2AdjustString,                asynParamOctet, &ADTimePixChip2Adjust);     
    // Detector Chips: Chip3
    createParam(ADTimePixChip3CP_PLLString,                 asynParamInt32, &ADTimePixChip3CP_PLL);
    createParam(ADTimePixChip3DiscS1OFFString,              asynParamInt32, &ADTimePixChip3DiscS1OFF);
    createParam(ADTimePixChip3DiscS1ONString,               asynParamInt32, &ADTimePixChip3DiscS1ON);
    createParam(ADTimePixChip3DiscS2OFFString,              asynParamInt32, &ADTimePixChip3DiscS2OFF);
    createParam(ADTimePixChip3DiscS2ONString,               asynParamInt32, &ADTimePixChip3DiscS2ON);
    createParam(ADTimePixChip3IkrumString,                  asynParamInt32, &ADTimePixChip3Ikrum);
    createParam(ADTimePixChip3PixelDACString,               asynParamInt32, &ADTimePixChip3PixelDAC);
    createParam(ADTimePixChip3PreampOFFString,              asynParamInt32, &ADTimePixChip3PreampOFF);
    createParam(ADTimePixChip3PreampONString,               asynParamInt32, &ADTimePixChip3PreampON);
    createParam(ADTimePixChip3TPbufferInString,             asynParamInt32, &ADTimePixChip3TPbufferIn);
    createParam(ADTimePixChip3TPbufferOutString,            asynParamInt32, &ADTimePixChip3TPbufferOut);
    createParam(ADTimePixChip3PLL_VcntrlString,             asynParamInt32, &ADTimePixChip3PLL_Vcntrl);
    createParam(ADTimePixChip3VPreampNCASString,            asynParamInt32, &ADTimePixChip3VPreampNCAS);
    createParam(ADTimePixChip3VTPcoarseString,              asynParamInt32, &ADTimePixChip3VTPcoarse);
    createParam(ADTimePixChip3VTPfineString,                asynParamInt32, &ADTimePixChip3VTPfine);
    createParam(ADTimePixChip3VfbkString,                   asynParamInt32, &ADTimePixChip3Vfbk);
    createParam(ADTimePixChip3VthresholdCoarseString,       asynParamInt32, &ADTimePixChip3VthresholdCoarse);
    createParam(ADTimePixChip3VTthresholdFineString,        asynParamInt32, &ADTimePixChip3VTthresholdFine);
    createParam(ADTimePixChip3AdjustString,                 asynParamOctet, &ADTimePixChip3Adjust);
             
    // Detector Chip Layout
    createParam(ADTimePixChip0LayoutString,               asynParamOctet, &ADTimePixChip0Layout);
    createParam(ADTimePixChip1LayoutString,               asynParamOctet, &ADTimePixChip1Layout);
    createParam(ADTimePixChip2LayoutString,               asynParamOctet, &ADTimePixChip2Layout);
    createParam(ADTimePixChip3LayoutString,               asynParamOctet, &ADTimePixChip3Layout);

    // Files BPC, Chip/DACS
    createParam(ADTimePixBPCFilePathString,                asynParamOctet,  &ADTimePixBPCFilePath);            
    createParam(ADTimePixBPCFilePathExistsString,          asynParamInt32,  &ADTimePixBPCFilePathExists);          
    createParam(ADTimePixBPCFileNameString,                asynParamOctet,  &ADTimePixBPCFileName);            
    createParam(ADTimePixDACSFilePathString,               asynParamOctet,  &ADTimePixDACSFilePath);           
    createParam(ADTimePixDACSFilePathExistsString,         asynParamInt32,  &ADTimePixDACSFilePathExists);             
    createParam(ADTimePixDACSFileNameString,               asynParamOctet,  &ADTimePixDACSFileName); 
    createParam(ADTimePixWriteFileMsgString,               asynParamOctet,  &ADTimePixWriteFileMsg); 

    //sets driver version
    char versionString[25];
    epicsSnprintf(versionString, sizeof(versionString), "%d.%d.%d", ADTIMEPIX_VERSION, ADTIMEPIX_REVISION, ADTIMEPIX_MODIFICATION);
    setStringParam(NDDriverVersion, versionString);
    setStringParam(ADTimePixServer, serverURL);

//    callParamCallbacks();   // Apply to EPICS, at end of file

    if(strlen(serverURL) < 0){
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Connection failed, abort\n", driverName, functionName);
    }
// asynSuccess = 0, so use !0 for true/connected    
    else{
        asynStatus connected = initialServerCheckConnection();
 //       if(!connected){   // readability: in UNIX 0 is success for a command, but in C++ 0 is "false"
        if(connected == asynSuccess) {

            asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s::%s Acquiring device information\n", driverName, functionName);
        //    getDashboard(serverURL); 
            printf("Dashboard done HERE!\n\n");
        //    getServer();
            printf("Server done HERE!\n\n");
            initCamera();
            printf("initCamera done HERE!\n\n");
        }
    }

     // when epics is exited, delete the instance of this class
    epicsAtExit(exitCallbackC, this);
}


ADTimePix::~ADTimePix(){
    const char* functionName = "~ADTimePix";
    asynPrint(this->pasynUserSelf, ASYN_TRACEIO_DRIVER,"%s::%s ADTimePix driver exiting\n", driverName, functionName);
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
