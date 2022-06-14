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
#define ADTimePixFreeSpaceString           "TPX3_FREE_SPACE"             // (asynFloat64,       r)
#define ADTimePixWriteSpeedString          "TPX3_WRITE_SPEED"            // (asynFloat64,       r)
#define ADTimePixHttpCodeString            "TPX3_HTTP_CODE"              // (asynInt32,         r)      200/OK, 204/NoContent, 302/MovedTemporarly, 400/BadRequest, 404/NotFound, 409/Conflict, 500/InternalError, 503/ServiceUnavailable

// dashboard
#define ADTimePixServalVerString           "TPX3_SERVAL_VER"             // (asynOctet,         r)      Serval Version

// Place any required inclues here

#include "ADDriver.h"
#include "frozen.h"
#include "cpr/cpr.h"


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
        int ADTimePixWriteSpeed;

        int ADTimePixHttpCode;
        int ADTimePixServalVer;

        int ADTimePixFreeSpace;
        #define ADTIMEPIX_LAST_PARAM ADTimePixFreeSpace

    private:

        // Some data variables
        epicsEventId startEventId;
        epicsEventId endEventId;
        

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
        asynStatus initialServerCheckConnection(const char* serverURL);

        void printConnectedDeviceInfo();

        //function that begins image aquisition
        asynStatus acquireStart();

        //function that stops aquisition
        asynStatus acquireStop();

        asynStatus getDashboard(const char* serverURL);

};

// Stores number of additional PV parameters are added by the driver
#define NUM_TIMEPIX_PARAMS ((int)(&ADTIMEPIX_LAST_PARAM - &ADTIMEPIX_FIRST_PARAM + 1))

#endif
