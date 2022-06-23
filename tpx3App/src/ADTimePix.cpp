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
//auto std::string::quotes(std::string str) {
//    if (str.length() > 2) {
//        str.pop_back();
//        str.erase(str.begin());
//        return asynSuccess;        
//    }
//    return asynError;
//}   

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
asynStatus ADTimePix::initialServerCheckConnection(const char* serverURL){
    const char* functionName = "initialServerCheckConnection";
    bool connected = false;


    // Implement connecting to the camera here
    // Usually the vendor provides examples of how to do this with the library/SDK
    // Use GET request and compare if URI status response code is 200.
    cpr::Response r = cpr::Get(cpr::Url{serverURL},
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
        printf("CONNECTED to Welcom URI!, %li\n", r.status_code);
    //    printf("asynSuccess! %d\n\n", asynSuccess);
    }

    //sets URI http code
    createParam(ADTimePixHttpCodeString, asynParamInt32, &ADTimePixHttpCode);
    setIntegerParam(ADTimePixHttpCode, r.status_code);
    callParamCallbacks();   // Apply to EPICS, at end of file

    if(connected) return asynSuccess;
    else{
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s Error: Failed to connect to server %s\n", driverName, functionName, serverURL);
        return asynError;
    }
}


/**
 * Function that updates PV values of dashboard with camera information
 * 
 * @return: status
 */
asynStatus ADTimePix::getDashboard(const char* serverURL){
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

//  dashboard = std::string("http://localhost:8080") + "/dashboard";
    dashboard = std::string(serverURL) + std::string("/dashboard");
    printf("ServerURL/dashboard=%s\n", dashboard.c_str());
    cpr::Response r = cpr::Get(cpr::Url{dashboard},
                               cpr::Authentication{"user", "pass", cpr::AuthMode::BASIC},
                               cpr::Parameters{{"anon", "true"}, {"key", "value"}});

    printf("Status code: %li\n", r.status_code);
    printf("Text: %s\n", r.text.c_str());

    json dashboard_j = json::parse(r.text.c_str());
    // dashboard_j["Server"]["SoftwareVersion"] = "2.4.2";
    printf("Text JSON: %s\n", dashboard_j.dump(3,' ', true).c_str());

    // strip double quote from beginning and end of string
    std::string SwVer, SwTS;
    SwVer = dashboard_j["Server"]["SoftwareVersion"].dump().c_str();
    if (SwVer.length() > 1) {
        printf("SwVer=%s, SwVerlength=%ld\n", SwVer.c_str(), SwVer.length());
        SwVer.pop_back();
        SwVer.erase(SwVer.begin());
    }
    SwTS = dashboard_j["Server"]["SoftwareTimestamp"].dump().c_str();
    if (SwTS.length() > 1) {
        printf("SwTS=%s, SwTSlength=%ld\n", SwTS.c_str(), SwTS.length());
        SwTS.pop_back();
        SwTS.erase(SwTS.begin());
        printf("SwTS->%s\n", SwTS.c_str());
    }
    //sets Serval Firmware Version, TimeStamp PV
    setStringParam(ADFirmwareVersion, SwVer.c_str());
    setStringParam(ADTimePixFWTimeStamp, SwTS.c_str());
    
    callParamCallbacks();   // Apply to EPICS, at end of file

    printf("After Serval Version!\n");

    // Is Detector connected?
    std::string Detector, DetType;
    Detector = dashboard_j["Detector"].dump().c_str();
    printf("Detector=%s\n", Detector.c_str());
    if (strcmp(Detector.c_str(), "null")) {
        Detector.pop_back();
        Detector.erase(Detector.begin());
        DetType = dashboard_j["Detector"]["DetectorType"].dump().c_str();
        DetType.pop_back();
        DetType.erase(DetType.begin());
        printf("Detector CONNECTED, Detector=%s, DetType=%s\n", Detector.c_str(), DetType.c_str());
        setStringParam(ADTimePixDetType, DetType.c_str());
    }
    else {
        printf("Detector NOT CONNECTED, Detector=%s\n", Detector.c_str());
        setStringParam(ADTimePixDetType, "null");
    }

    return status;
}


/**
 * Function that updates PV values of server with camera information
 * 
 * @return: status
 */
asynStatus ADTimePix::getServer(const char* serverURL){
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

    server = std::string(serverURL) + std::string("/server");
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

    // Call createParam here
    // ex. createParam(ADUVC_UVCComplianceLevelString, asynParamInt32, &ADUVC_UVCComplianceLevel);
    createParam(ADTimePixServerNameString, asynParamOctet, &ADTimePixServer);
    

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
        asynStatus connected = initialServerCheckConnection(serverURL);
 //       if(!connected){   // readability: in UNIX 0 is success for a command, but in C++ 0 is "false"
        if(connected == asynSuccess) {

            asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s::%s Acquiring device information\n", driverName, functionName);
            getDashboard(serverURL); 
            printf("Dashboard done HERE!\n\n");
            getServer(serverURL);
            printf("Server done HERE!\n\n");
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
