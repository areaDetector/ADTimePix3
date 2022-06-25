/*
 * Header file for the ADTimePix EPICS driver
 *
 * This file contains the definitions of PV params, and the definition of the ADTimePix class and functions.
 *
 * Author: 
 * Created:
 *
 * Copyright (c) :
 *
 */

// header guard
#ifndef ADTIMEPIX_H
#define ADTIMEPIX_H

// version numbers
#define ADTIMEPIX_VERSION      0
#define ADTIMEPIX_REVISION     1
#define ADTIMEPIX_MODIFICATION 0


// Driver-specific PV string definitions here
/*                                         String                        asyn interface         access  Description  */
#define ADTimePixServerNameString          "TPX3_SERVER_NAME"            // (asynOctet,         r)      Server Name
#define ADTimePixDetTypeString             "TPX3_DETECTOR_TYPE"          // (asynOctet,         r)      Detector Type, should be binary DetConnected
#define ADTimePixFWTimeStampString         "TPX3_FW_TIMESTAMP"           // (asynOctet,         r)      Firmware TimeStamp
//#define ADTimePixDetConnectedString       "TPX3_DETECTOR_CONNECTED"     // (asynOctet,         r)      Detector Connected, TODO
#define ADTimePixFreeSpaceString           "TPX3_FREE_SPACE"             // (asynFloat64,       r)
#define ADTimePixWriteSpeedString          "TPX3_WRITE_SPEED"            // (asynFloat64,       r)
#define ADTimePixHttpCodeString            "TPX3_HTTP_CODE"              // (asynInt32,         r)      200/OK, 204/NoContent, 302/MovedTemporarly, 400/BadRequest, 404/NotFound, 409/Conflict, 500/InternalError, 503/ServiceUnavailable

// Health
#define ADTimePixLocalTempString            "TPX3_LOCAL_TEMP"            // (asynFloat64,       r)      Local Temperature
#define ADTimePixFPGATempString             "TPX3_FPGA_TEMP"             // (asynFloat64,       r)      FPGA Temperature
#define ADTimePixFan1SpeedString            "TPX3_FAN1_SPEED"            // (asynFloat64,       r)      Fan1 Speed
#define ADTimePixFan2SpeedString            "TPX3_FAN2_SPEED"            // (asynFloat64,       r)      Fan Speed
#define ADTimePixBiasVoltageString          "TPX3_BIAS_VOLT"             // (asynFloat64,       r)      Bias Voltage
#define ADTimePixChipTemperatureString      "TPX3_CHIP_TEMPS"            // (asynOctet,         r)      Chip temperature list
#define ADTimePixVDDString                  "TPX3_VDD"                   // (asynOctet,         r)      VDD list
#define ADTimePixAVDDString                 "TPX3_AVDD"                  // (asynOctet,         r)      AVDD list
#define ADTimePixHealthString               "TPX3_HEALTH"                // (asynInt32,         r)      Scan detector/health


// dashboard

// Place any required inclues here

#include "ADDriver.h"
#include "frozen.h"
#include "cpr/cpr.h"
#include "nlohmann/json.hpp"


// ----------------------------------------
// DRIVERNAMESTANDARD Data Structures
//-----------------------------------------

// Place any in use Data structures here



/*
 * Class definition of the ADTimePix driver. It inherits from the base ADDriver class
 *
 * Includes constructor/destructor, PV params, function defs and variable defs
 *
 */
class ADTimePix : ADDriver{

    public:

        // Constructor - NOTE THERE IS A CHANCE THAT YOUR CAMERA DOESNT CONNECT WITH SERIAL # AND THIS MUST BE CHANGED
        ADTimePix(const char* portName, const char* serial, int maxBuffers, size_t maxMemory, int priority, int stackSize);


        // ADDriver overrides
        virtual asynStatus writeInt32(asynUser* pasynUser, epicsInt32 value);
        virtual asynStatus writeFloat64(asynUser* pasynUser, epicsFloat64 value);


        // destructor. Disconnects from camera, deletes the object
        ~ADTimePix();

    protected:

        int ADTimePixServer;
        #define ADTIMEPIX_FIRST_PARAM ADTimePixServer
        int ADTimePixDetType;
        int ADTimePixFWTimeStamp;
    //    int ADTimePixDetConnected;    // TODO
        int ADTimePixWriteSpeed;
        int ADTimePixHttpCode;

    // Health
        int ADTimePixLocalTemp;
        int ADTimePixFPGATemp;
        int ADTimePixFan1Speed;
        int ADTimePixFan2Speed;
        int ADTimePixBiasVoltage;
        int ADTimePixChipTemperature;
        int ADTimePixVDD;
        int ADTimePixAVDD;
        int ADTimePixHealth;

        int ADTimePixFreeSpace;
        #define ADTIMEPIX_LAST_PARAM ADTimePixFreeSpace

    private:

        // Some data variables
        epicsEventId startEventId;
        epicsEventId endEventId;
        

        std::string serverURL;

        // ----------------------------------------
        // DRIVERNAMESTANDARD Global Variables

        // ----------------------------------------
        // DRIVERNAMESTANDARD Functions - Logging/Reporting
        //-----------------------------------------

        // reports device and driver info into a log file
        void report(FILE* fp, int details);

        // writes to ADStatus PV
        void updateStatus(const char* status);

        //function used for connecting to a TimePix3 serval URL device
        // NOTE - THIS MAY ALSO NEED TO CHANGE IF SERIAL # NOT USED
        asynStatus initialServerCheckConnection();

        void printConnectedDeviceInfo();

        //function that begins image aquisition
        asynStatus acquireStart();

        //function that stops aquisition
        asynStatus acquireStop();

        asynStatus getDashboard();
        asynStatus getServer();
        asynStatus getHealth();

};

// Stores number of additional PV parameters are added by the driver
#define NUM_TIMEPIX_PARAMS ((int)(&ADTIMEPIX_LAST_PARAM - &ADTIMEPIX_FIRST_PARAM + 1))

#endif
