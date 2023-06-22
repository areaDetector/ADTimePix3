/*
 * Header file for the ADTimePix3 EPICS driver
 *
 * This file contains the definitions of PV params, and the definition of the ADTimePix3 class and functions.
 *
 * Author:  Kazimierz Gofron
 * Created: June, 2022
 * Last edited: March 28, 2023
 * Copyright (c) : Oak Ridge National Laboratory
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

// Detector Health
#define ADTimePixLocalTempString            "TPX3_LOCAL_TEMP"            // (asynFloat64,       r)      Local Temperature
#define ADTimePixFPGATempString             "TPX3_FPGA_TEMP"             // (asynFloat64,       r)      FPGA Temperature
#define ADTimePixFan1SpeedString            "TPX3_FAN1_SPEED"            // (asynFloat64,       r)      Fan1 Speed
#define ADTimePixFan2SpeedString            "TPX3_FAN2_SPEED"            // (asynFloat64,       r)      Fan Speed
#define ADTimePixBiasVoltageString          "TPX3_BIAS_VOLT_H"           // (asynFloat64,       r)      Bias Voltage
#define ADTimePixHumidityString             "TPX3_HUMIDITY"              // (asynInt32,         r)      Humidity
#define ADTimePixChipTemperatureString      "TPX3_CHIP_TEMPS"            // (asynOctet,         r)      Chip temperature list
#define ADTimePixVDDString                  "TPX3_VDD"                   // (asynOctet,         r)      VDD list
#define ADTimePixAVDDString                 "TPX3_AVDD"                  // (asynOctet,         r)      AVDD list
#define ADTimePixHealthString               "TPX3_HEALTH"                // (asynInt32,         r)      Scan detector/health

    // Detector Info
#define ADTimePixIfaceNameString            "TPX3_IFACE"            // (asynOctet,         r)      IfaceName
#define ADTimePixSW_versionString           "TPX3_SW_VER"           // (asynOctet,         r)      SW_version
#define ADTimePixFW_versionString           "TPX3_FW_VER"           // (asynOctet,         r)      FW_versionS
#define ADTimePixPixCountString             "TPX3_PEL_CNT"          // (asynInt32,         r)      PixCount
#define ADTimePixRowLenString               "TPX3_ROWLEN"           // (asynInt32,         r)      RowLen
#define ADTimePixNumberOfChipsString        "TPX3_NUM_CHIPS"        // (asynInt32,         r)      NumberOfChip
#define ADTimePixNumberOfRowsString         "TPX3_NUM_ROWS"         // (asynInt32,         r)      NumberOfRows
#define ADTimePixMpxTypeString              "TPX3_MPX_TYPE"         // (asynInt32,         r)      MpxType

#define ADTimePixBoardsIDString             "TPX3_BOARDS_ID"        // (asynOctet,         r)      Boards->ChipboardId
#define ADTimePixBoardsIPString             "TPX3_BOARDS_IP"        // (asynOctet,         r)      Boards->IpAddress
#define ADTimePixBoardsCh1String            "TPX3_BOARDS_CH1"       // (asynOctet,         r)      Boards->Chip1{Id,Name}
#define ADTimePixBoardsCh2String            "TPX3_BOARDS_CH2"       // (asynOctet,         r)      Boards->Chip2{Id,Name}
#define ADTimePixBoardsCh3String            "TPX3_BOARDS_CH3"       // (asynOctet,         r)      Boards->Chip3{Id,Name}
#define ADTimePixBoardsCh4String            "TPX3_BOARDS_CH4"       // (asynOctet,         r)      Boards->Chip4{Id,Name}

#define ADTimePixSuppAcqModesString         "TPX3_ACQ_MODES"        // (asynInt32,         r)      SuppAcqModes
#define ADTimePixClockReadoutString         "TPX3_CLOCK_READ"       // (asynFloat64,       r)      ClockReadout
#define ADTimePixMaxPulseCountString        "TPX3_PULSE_CNT"        // (asynInt32,         r)      MaxPulseCount
#define ADTimePixMaxPulseHeightString       "TPX3_PULSE_HIGHT"      // (asynFloat64,       r)      MaxPulseHeight
#define ADTimePixMaxPulsePeriodString       "TPX3_PULSE_PERIOD"     // (asynFloat64,       r)      MaxPulsePeriod
#define ADTimePixTimerMaxValString          "TPX3_TIME_MAX"         // (asynFloat64,       r)      TimerMaxVal
#define ADTimePixTimerMinValString          "TPX3_TIME_MIN"         // (asynFloat64,       r)      TimerMinVal
#define ADTimePixTimerStepString            "TPX3_TIME_STEP"        // (asynFloat64,       r)      TimerStep
#define ADTimePixClockTimepixString         "TPX3_CLOCK"            // (asynFloat64,       r)      ClockTimepix

    // Detector Config
#define ADTimePixFan1PWMString                  "TPX3_FAN1PWM"           // (asynInt32,         r)   Fan1PWM   
#define ADTimePixFan2PWMString                  "TPX3_FAN2PWM"           // (asynInt32,         r)   Fan2PWM   
#define ADTimePixBiasVoltString                 "TPX3_BIAS_VOLT_R"       // (asynInt32,         r)   BiasVoltage
#define ADTimePixBiasEnableString               "TPX3_BIAS_ENBL"         // (asynInt32,         r)   BiasEnabled
#define ADTimePixChainModeString                "TPX3_CHAIN_MODE"        // (asynInt32,         r)   ChainMode
#define ADTimePixTriggerInString                "TPX3_TRIGGER_IN"        // (asynInt32,         r)   TriggerIn
#define ADTimePixTriggerOutString               "TPX3_TRIGGER_OUT"       // (asynInt32,         r)   TriggerOut
#define ADTimePixPolarityString                 "TPX3_POLARITY"          // (asynInt32,         r)   Polarity
#define ADTimePixTriggerModeString              "TPX3_TRIGGER_MODE"      // (asynOctet,         r)   TriggerMode
#define ADTimePixExposureTimeString             "TPX3_EXPOSURE_TIME"     // (asynFloat64,       r)   ExposureTime
#define ADTimePixTriggerPeriodString            "TPX3_TRIGGER_PERIOD"    // (asynFloat64,       r)   TriggerPeriod
#define ADTimePixnTriggersString                "TPX3_NTRIGGERS"         // (asynInt32,         r)   nTriggers
#define ADTimePixPeriphClk80String              "TPX3_PERIPH_CLK80"      // (asynInt32,         r)   PeriphClk80
#define ADTimePixTriggerDelayString             "TPX3_TRIG_DELAY"        // (asynFloat64,       r)   TriggerDelay
#define ADTimePixTdcString                      "TPX3_TDC"               // (asynOctet,         r)   Tdc
#define ADTimePixTdc0String                      "TPX3_TDC0"             // (asynInt32,         r)   Tdc0
#define ADTimePixTdc1String                      "TPX3_TDC1"             // (asynInt32,         r)   Tdc1
#define ADTimePixGlobalTimestampIntervalString  "TPX3_GL_TIMESTAMP_INT"  // (asynFloat64,       r)   GlobalTimestampInterval
#define ADTimePixExternalReferenceClockString   "TPX3_EXT_REF_CLOCK"     // (asynInt32,         r)   ExternalReferenceClock
#define ADTimePixLogLevelString                 "TPX3_LOG_LEVEL"         // (asynInt32,         r)   LogLevel

    // Detector Chips: Chip0; Chips count from 0-3.
#define ADTimePixChip0CP_PLLString              "TPX3_CHIP0_CP_PLL"           // (asynInt32,         r)      DACs->Ibias_CP_PLL
#define ADTimePixChip0DiscS1OFFString           "TPX3_CHIP0_DISCS1OFF"        // (asynInt32,         r)      DACs->Ibias_DiscS1_OFF
#define ADTimePixChip0DiscS1ONString            "TPX3_CHIP0_DISCS1ON"         // (asynInt32,         r)      DACs->Ibias_DiscS1_ON
#define ADTimePixChip0DiscS2OFFString           "TPX3_CHIP0_DISCS2OFF"        // (asynInt32,         r)      DACs->Ibias_DiscS2_OFF
#define ADTimePixChip0DiscS2ONString            "TPX3_CHIP0_DISCS2ON"         // (asynInt32,         r)      DACs->Ibias_DiscS2_ON
#define ADTimePixChip0IkrumString               "TPX3_CHIP0_IKRUM"            // (asynInt32,         r)      DACs->Ibias_Ikrum
#define ADTimePixChip0PixelDACString            "TPX3_CHIP0_PIXELDAC"         // (asynInt32,         r)      DACs->Ibias_PixelDAC
#define ADTimePixChip0PreampOFFString           "TPX3_CHIP0_PREAMPOFF"        // (asynInt32,         r)      DACs->Ibias_Preamp_OFF
#define ADTimePixChip0PreampONString            "TPX3_CHIP0_PREAMPON"         // (asynInt32,         r)      DACs->Ibias_Preamp_ON
#define ADTimePixChip0TPbufferInString          "TPX3_CHIP0_TPBUFFERIN"       // (asynInt32,         r)      DACs->Ibias_TPbufferIn
#define ADTimePixChip0TPbufferOutString         "TPX3_CHIP0_TPBUFFEROUT"      // (asynInt32,         r)      DACs->Ibias_TPbufferOut
#define ADTimePixChip0PLL_VcntrlString          "TPX3_CHIP0_PLL_VCNTRL"       // (asynInt32,         r)      DACs->PLL_Vcntrl
#define ADTimePixChip0VPreampNCASString         "TPX3_CHIP0_VPREAMPNCAS"      // (asynInt32,         r)      DACs->VPreamp_NCAS
#define ADTimePixChip0VTPcoarseString           "TPX3_CHIP0_VTP_COARSE"       // (asynInt32,         r)      DACs->VTP_coarse
#define ADTimePixChip0VTPfineString             "TPX3_CHIP0_VTP_FINE"         // (asynInt32,         r)      DACs->VTP_fine
#define ADTimePixChip0VfbkString                "TPX3_CHIP0_VFBK"             // (asynInt32,         r)      DACs->Vfbk
#define ADTimePixChip0VthresholdCoarseString    "TPX3_CHIP0_VTH_COARSE"       // (asynInt32,         r)      DACs->Vthreshold_coarse
#define ADTimePixChip0VTthresholdFineString     "TPX3_CHIP0_VTH_FINE"         // (asynInt32,         r)      DACs->Vthreshold_fine
#define ADTimePixChip0AdjustString              "TPX3_CHIP0_ADJUST"           // (asynInt32,         r)      DACs->Adjust
    // Detector Chips: Chip1
#define ADTimePixChip1CP_PLLString              "TPX3_CHIP1_CP_PLL"           // (asynInt32,         r)      DACs->Ibias_CP_PLL
#define ADTimePixChip1DiscS1OFFString           "TPX3_CHIP1_DISCS1OFF"        // (asynInt32,         r)      DACs->Ibias_DiscS1_OFF
#define ADTimePixChip1DiscS1ONString            "TPX3_CHIP1_DISCS1ON"         // (asynInt32,         r)      DACs->Ibias_DiscS1_ON
#define ADTimePixChip1DiscS2OFFString           "TPX3_CHIP1_DISCS2OFF"        // (asynInt32,         r)      DACs->Ibias_DiscS2_OFF
#define ADTimePixChip1DiscS2ONString            "TPX3_CHIP1_DISCS2ON"         // (asynInt32,         r)      DACs->Ibias_DiscS2_ON
#define ADTimePixChip1IkrumString               "TPX3_CHIP1_IKRUM"            // (asynInt32,         r)      DACs->Ibias_Ikrum
#define ADTimePixChip1PixelDACString            "TPX3_CHIP1_PIXELDAC"         // (asynInt32,         r)      DACs->Ibias_PixelDAC
#define ADTimePixChip1PreampOFFString           "TPX3_CHIP1_PREAMPOFF"        // (asynInt32,         r)      DACs->Ibias_Preamp_OFF
#define ADTimePixChip1PreampONString            "TPX3_CHIP1_PREAMPON"         // (asynInt32,         r)      DACs->Ibias_Preamp_ON
#define ADTimePixChip1TPbufferInString          "TPX3_CHIP1_TPBUFFERIN"       // (asynInt32,         r)      DACs->Ibias_TPbufferIn
#define ADTimePixChip1TPbufferOutString         "TPX3_CHIP1_TPBUFFEROUT"      // (asynInt32,         r)      DACs->Ibias_TPbufferOut
#define ADTimePixChip1PLL_VcntrlString          "TPX3_CHIP1_PLL_VCNTRL"       // (asynInt32,         r)      DACs->PLL_Vcntrl
#define ADTimePixChip1VPreampNCASString         "TPX3_CHIP1_VPREAMPNCAS"      // (asynInt32,         r)      DACs->VPreamp_NCAS
#define ADTimePixChip1VTPcoarseString           "TPX3_CHIP1_VTP_COARSE"       // (asynInt32,         r)      DACs->VTP_coarse
#define ADTimePixChip1VTPfineString             "TPX3_CHIP1_VTP_FINE"         // (asynInt32,         r)      DACs->VTP_fine
#define ADTimePixChip1VfbkString                "TPX3_CHIP1_VFBK"             // (asynInt32,         r)      DACs->Vfbk
#define ADTimePixChip1VthresholdCoarseString    "TPX3_CHIP1_VTH_COARSE"       // (asynInt32,         r)      DACs->Vthreshold_coarse
#define ADTimePixChip1VTthresholdFineString     "TPX3_CHIP1_VTH_FINE"         // (asynInt32,         r)      DACs->Vthreshold_fine
#define ADTimePixChip1AdjustString              "TPX3_CHIP1_ADJUST"           // (asynInt32,         r)      DACs->Adjust
    // Detector Chips: Chip2
#define ADTimePixChip2CP_PLLString              "TPX3_CHIP2_CP_PLL"           // (asynInt32,         r)      DACs->Ibias_CP_PLL
#define ADTimePixChip2DiscS1OFFString           "TPX3_CHIP2_DISCS1OFF"        // (asynInt32,         r)      DACs->Ibias_DiscS1_OFF
#define ADTimePixChip2DiscS1ONString            "TPX3_CHIP2_DISCS1ON"         // (asynInt32,         r)      DACs->Ibias_DiscS1_ON
#define ADTimePixChip2DiscS2OFFString           "TPX3_CHIP2_DISCS2OFF"        // (asynInt32,         r)      DACs->Ibias_DiscS2_OFF
#define ADTimePixChip2DiscS2ONString            "TPX3_CHIP2_DISCS2ON"         // (asynInt32,         r)      DACs->Ibias_DiscS2_ON
#define ADTimePixChip2IkrumString               "TPX3_CHIP2_IKRUM"            // (asynInt32,         r)      DACs->Ibias_Ikrum
#define ADTimePixChip2PixelDACString            "TPX3_CHIP2_PIXELDAC"         // (asynInt32,         r)      DACs->Ibias_PixelDAC
#define ADTimePixChip2PreampOFFString           "TPX3_CHIP2_PREAMPOFF"        // (asynInt32,         r)      DACs->Ibias_Preamp_OFF
#define ADTimePixChip2PreampONString            "TPX3_CHIP2_PREAMPON"         // (asynInt32,         r)      DACs->Ibias_Preamp_ON
#define ADTimePixChip2TPbufferInString          "TPX3_CHIP2_TPBUFFERIN"       // (asynInt32,         r)      DACs->Ibias_TPbufferIn
#define ADTimePixChip2TPbufferOutString         "TPX3_CHIP2_TPBUFFEROUT"      // (asynInt32,         r)      DACs->Ibias_TPbufferOut
#define ADTimePixChip2PLL_VcntrlString          "TPX3_CHIP2_PLL_VCNTRL"       // (asynInt32,         r)      DACs->PLL_Vcntrl
#define ADTimePixChip2VPreampNCASString         "TPX3_CHIP2_VPREAMPNCAS"      // (asynInt32,         r)      DACs->VPreamp_NCAS
#define ADTimePixChip2VTPcoarseString           "TPX3_CHIP2_VTP_COARSE"       // (asynInt32,         r)      DACs->VTP_coarse
#define ADTimePixChip2VTPfineString             "TPX3_CHIP2_VTP_FINE"         // (asynInt32,         r)      DACs->VTP_fine
#define ADTimePixChip2VfbkString                "TPX3_CHIP2_VFBK"             // (asynInt32,         r)      DACs->Vfbk
#define ADTimePixChip2VthresholdCoarseString    "TPX3_CHIP2_VTH_COARSE"       // (asynInt32,         r)      DACs->Vthreshold_coarse
#define ADTimePixChip2VTthresholdFineString     "TPX3_CHIP2_VTH_FINE"         // (asynInt32,         r)      DACs->Vthreshold_fine
#define ADTimePixChip2AdjustString              "TPX3_CHIP2_ADJUST"           // (asynInt32,         r)      DACs->Adjust
    // Detector Chips: Chip3
#define ADTimePixChip3CP_PLLString              "TPX3_CHIP3_CP_PLL"           // (asynInt32,         r)      DACs->Ibias_CP_PLL
#define ADTimePixChip3DiscS1OFFString           "TPX3_CHIP3_DISCS1OFF"        // (asynInt32,         r)      DACs->Ibias_DiscS1_OFF
#define ADTimePixChip3DiscS1ONString            "TPX3_CHIP3_DISCS1ON"         // (asynInt32,         r)      DACs->Ibias_DiscS1_ON
#define ADTimePixChip3DiscS2OFFString           "TPX3_CHIP3_DISCS2OFF"        // (asynInt32,         r)      DACs->Ibias_DiscS2_OFF
#define ADTimePixChip3DiscS2ONString            "TPX3_CHIP3_DISCS2ON"         // (asynInt32,         r)      DACs->Ibias_DiscS2_ON
#define ADTimePixChip3IkrumString               "TPX3_CHIP3_IKRUM"            // (asynInt32,         r)      DACs->Ibias_Ikrum
#define ADTimePixChip3PixelDACString            "TPX3_CHIP3_PIXELDAC"         // (asynInt32,         r)      DACs->Ibias_PixelDAC
#define ADTimePixChip3PreampOFFString           "TPX3_CHIP3_PREAMPOFF"        // (asynInt32,         r)      DACs->Ibias_Preamp_OFF
#define ADTimePixChip3PreampONString            "TPX3_CHIP3_PREAMPON"         // (asynInt32,         r)      DACs->Ibias_Preamp_ON
#define ADTimePixChip3TPbufferInString          "TPX3_CHIP3_TPBUFFERIN"       // (asynInt32,         r)      DACs->Ibias_TPbufferIn
#define ADTimePixChip3TPbufferOutString         "TPX3_CHIP3_TPBUFFEROUT"      // (asynInt32,         r)      DACs->Ibias_TPbufferOut
#define ADTimePixChip3PLL_VcntrlString          "TPX3_CHIP3_PLL_VCNTRL"       // (asynInt32,         r)      DACs->PLL_Vcntrl
#define ADTimePixChip3VPreampNCASString         "TPX3_CHIP3_VPREAMPNCAS"      // (asynInt32,         r)      DACs->VPreamp_NCAS
#define ADTimePixChip3VTPcoarseString           "TPX3_CHIP3_VTP_COARSE"       // (asynInt32,         r)      DACs->VTP_coarse
#define ADTimePixChip3VTPfineString             "TPX3_CHIP3_VTP_FINE"         // (asynInt32,         r)      DACs->VTP_fine
#define ADTimePixChip3VfbkString                "TPX3_CHIP3_VFBK"             // (asynInt32,         r)      DACs->Vfbk
#define ADTimePixChip3VthresholdCoarseString    "TPX3_CHIP3_VTH_COARSE"       // (asynInt32,         r)      DACs->Vthreshold_coarse
#define ADTimePixChip3VTthresholdFineString     "TPX3_CHIP3_VTH_FINE"         // (asynInt32,         r)      DACs->Vthreshold_fine
#define ADTimePixChip3AdjustString              "TPX3_CHIP3_ADJUST"           // (asynInt32,         r)      DACs->Adjust

    // Chip Layout
#define ADTimePixDetectorOrientationString  "TPX3_DET_ORIENTATION"            // (asynOctet,         r)      DetectorOrientation, in Detector/Layout since 3.0.0
#define ADTimePixChip0LayoutString          "TPX3_CHIP0_LAYTOUT"              // (asynOctet,         r)      Chip 0 layout
#define ADTimePixChip1LayoutString          "TPX3_CHIP1_LAYTOUT"              // (asynOctet,         r)      Chip 1 layout
#define ADTimePixChip2LayoutString          "TPX3_CHIP2_LAYTOUT"              // (asynOctet,         r)      Chip 2 layout
#define ADTimePixChip3LayoutString          "TPX3_CHIP3_LAYTOUT"              // (asynOctet,         r)      Chip 3 layout

    // Absolute path to the binary pixel configuration, absolute path to the text chips configuration
#define ADTimePixBPCFilePathString          "BPC_FILE_PATH"             /**< (asynOctet,    r/w) The file path Binary Pixel Configuration */
#define ADTimePixBPCFilePathExistsString    "BPC_FILE_PATH_EXISTS"      /**< (asynInt32,    r/w) File path exists? */
#define ADTimePixBPCFileNameString          "BPC_FILE_NAME"             /**< (asynOctet,    r/w) The BPC file name */    
#define ADTimePixDACSFilePathString         "DACS_FILE_PATH"            /**< (asynOctet,    r/w) The file path  Chip configuration*/
#define ADTimePixDACSFilePathExistsString   "DACS_FILE_PATH_EXISTS"     /**< (asynInt32,    r/w) File path exists? */
#define ADTimePixDACSFileNameString         "DACS_FILE_NAME"            /**< (asynOctet,    r/w) The file name */    
#define ADTimePixWriteMsgString             "WRITE_FILE_MESSAGE"        /**< (asynOctet,    r  ) Config File write message */
#define ADTimePixWriteBPCFileString         "WRITE_BPC_FILE"            /**< (asynInt32,    r/w) Manually upload BPC file to detector when value=1 */
#define ADTimePixWriteDACSFileString        "WRITE_DACS_FILE"           /**< (asynInt32,    r/w) Manually upload Chips/DACS file to detector when value=1 */


    // Server, write data channels
#define ADTimePixWriteDataString           "TPX3_WRITE_DATA"          // (asynInt32,         w)      Write Data output channels (raw,img,preview)
#define ADTimePixWriteRawString            "TPX3_WRITE_RAW"           // (asynInt32,         w)      Write Data output channels (raw)
#define ADTimePixWriteImgString            "TPX3_WRITE_IMG"           // (asynInt32,         w)      Write Data output channels (img)
#define ADTimePixWritePrvImgString         "TPX3_WRITE_PRVIMG"        // (asynInt32,         w)      Write Data output channels (preview->img)
#define ADTimePixWritePrvImg1String        "TPX3_WRITE_PRVIMG1"       // (asynInt32,         w)      Write Data output channels (preview->img1)
#define ADTimePixWritePrvHstString         "TPX3_WRITE_PRVHST"        // (asynInt32,         w)      Write Data output channels (preview->hst)

    // Server, raw
#define ADTimePixRawBaseString              "TPX3_RAW_BASE"             // (asynOctet,         w)      Raw Destination Base
#define ADTimePixRawFilePatString           "TPX3_RAW_FILEPAT"          // (asynOctet,         w)      Raw Destination File Pattern
#define ADTimePixRawSplitStrategyString     "TPX3_RAW_SPLITSTG"         // (asynOctet,         w)      Raw Destination Split Strategy
#define ADTimePixRawQueueSizeString         "TPX3_RAW_QUEUESIZE"        // (asynInt32,         w)      Raw Destination Queue Size
#define ADTimePixRawFilePathExistsString    "RAW_FILE_PATH_EXISTS"      // (asynInt32,       r/w) File path exists? */

    // Server, Image, ImageChannels[0]
#define ADTimePixImgBaseString               "TPX3_IMG_IMGBASE"          // (asynOctet,         w)      ImageChannels Base file (Place raw files) 
#define ADTimePixImgFilePatString            "TPX3_IMG_IMGPAT"           // (asynOctet,         w)      ImageChannels FilePattern 
#define ADTimePixImgFormatString             "TPX3_IMG_IMGFORMAT"        // (asynInt32,         w)      ImageChannels Format
#define ADTimePixImgModeString               "TPX3_IMG_IMGMODE"          // (asynInt32,         w)      ImageChannels Mode
#define ADTimePixImgThsString                "TPX3_IMG_IMGTHS"           // (asynOctet,         w)      ImageChannels Thresholds
#define ADTimePixImgIntSizeString            "TPX3_IMG_INTSIZE"          // (asynInt32,         w)      ImageChannels IntegrationSize
#define ADTimePixImgIntModeString            "TPX3_IMG_INTMODE"          // (asynInt32,         w)      ImageChannels IntegrationMode
#define ADTimePixImgStpOnDskLimString        "TPX3_IMG_STPONDSK"         // (asynInt32,         w)      ImageChannels StopMeasurementOnDiskLimit
#define ADTimePixImgQueueSizeString          "TPX3_IMG_QUEUESIZE"        // (asynInt32,         w)      ImageChannels QueueSize
#define ADTimePixImgFilePathExistsString     "IMG_FILE_PATH_EXISTS"      // (asynInt32,       r/w)      File path exists? */

    // Server, Preview
#define ADTimePixPrvPeriodString            "TPX3_PRV_PERIOD"           // (asynFloat64,       w)      Preview Period
#define ADTimePixPrvSamplingModeString      "TPX3_PRV_SAMPLMODE"        // (asynOctet,         w)      Preview Sampling Mode
    // Server, Preview, ImageChannels[0]
#define ADTimePixPrvImgBaseString               "TPX3_PRV_IMGBASE"          // (asynOctet,         w)      Preview ImageChannels Base file (Place raw files) 
#define ADTimePixPrvImgFilePatString            "TPX3_PRV_IMGPAT"           // (asynOctet,         w)      Preview ImageChannels FilePattern 
#define ADTimePixPrvImgFormatString             "TPX3_PRV_IMGFORMAT"        // (asynInt32,         w)      Preview ImageChannels Format
#define ADTimePixPrvImgModeString               "TPX3_PRV_IMGMODE"          // (asynInt32,         w)      Preview ImageChannels Mode
#define ADTimePixPrvImgThsString                "TPX3_PRV_IMGTHS"           // (asynOctet,         w)      Preview ImageChannels Thresholds
#define ADTimePixPrvImgIntSizeString            "TPX3_PRV_INTSIZE"          // (asynInt32,         w)      Preview ImageChannels IntegrationSize
#define ADTimePixPrvImgIntModeString            "TPX3_PRV_INTMODE"          // (asynInt32,         w)      Preview ImageChannels IntegrationMode
#define ADTimePixPrvImgStpOnDskLimString        "TPX3_PRV_STPONDSK"         // (asynInt32,         w)      Preview ImageChannels StopMeasurementOnDiskLimit
#define ADTimePixPrvImgQueueSizeString          "TPX3_PRV_QUEUESIZE"        // (asynInt32,         w)      Preview ImageChannels QueueSize
#define ADTimePixPrvImgFilePathExistsString     "PRV_IMG_FILE_PATH_EXISTS"  // (asynInt32,       r/w)      File path exists? */
    // Server, Preview, ImageChannels[1]
#define ADTimePixPrvImg1BaseString            "TPX3_PRV_IMG1BASE"          // (asynOctet,         w)      Preview ImageChannels Preview files Base
#define ADTimePixPrvImg1FormatString          "TPX3_PRV_IMG1FORMAT"        // (asynInt32,         w)      Preview ImageChannels Preview files Format
#define ADTimePixPrvImg1ModeString            "TPX3_PRV_IMG1MODE"          // (asynInt32,         w)      Preview ImageChannels Preview files Mode
#define ADTimePixPrvImg1ThsString             "TPX3_PRV_IMG1THS"           // (asynOctet,         w)      Preview ImageChannels Preview files Thresholds
#define ADTimePixPrvImg1IntSizeString         "TPX3_PRV_IMG1INTSIZE"       // (asynInt32,         w)      Preview ImageChannels Preview files IntegrationSize
#define ADTimePixPrvImg1IntModeString         "TPX3_PRV_IMG1INTMODE"       // (asynInt32,         w)      Preview ImageChannels IntegrationMode
#define ADTimePixPrvImg1StpOnDskLimString     "TPX3_PRV_IMG1STPONDSK"      // (asynInt32,         w)      Preview ImageChannels Preview files StopMeasurementOnDiskLimit
#define ADTimePixPrvImg1QueueSizeString       "TPX3_PRV_IMG1QUEUESIZE"     // (asynInt32,         w)      Preview ImageChannels Preview files QueueSize
//#define ADTimePixPrvImg1FilePathExistsString  "PRV_IMG1_FILE_PATH_EXISTS"  // (asynInt32,       r/w)      File path exists? */

    // Server, Preview, HistogramChannels[0]
#define ADTimePixPrvHstBaseString               "TPX3_PRV_HSTBASE"          // (asynOctet,         w)      Preview HistogramChannels Base file (Place raw files) 
#define ADTimePixPrvHstFilePatString            "TPX3_PRV_HSTPAT"           // (asynOctet,         w)      Preview HistogramChannels FilePattern 
#define ADTimePixPrvHstFormatString             "TPX3_PRV_HSTFORMAT"        // (asynInt32,         w)      Preview HistogramChannels Format
#define ADTimePixPrvHstModeString               "TPX3_PRV_HSTMODE"          // (asynInt32,         w)      Preview HistogramChannels Mode
#define ADTimePixPrvHstThsString                "TPX3_PRV_HSTTHS"           // (asynOctet,         w)      Preview HistogramChannels Thresholds
#define ADTimePixPrvHstIntSizeString            "TPX3_PRV_HSTINTSIZE"       // (asynInt32,         w)      Preview HistogramChannels IntegrationSize
#define ADTimePixPrvHstIntModeString            "TPX3_PRV_HSTINTMODE"       // (asynInt32,         w)      Preview HistogramChannels IntegrationMode
#define ADTimePixPrvHstStpOnDskLimString        "TPX3_PRV_HSTSTPONDSK"      // (asynInt32,         w)      Preview HistogramChannels StopMeasurementOnDiskLimit
#define ADTimePixPrvHstQueueSizeString          "TPX3_PRV_HSTQUEUESIZE"     // (asynInt32,         w)      Preview HistogramChannels QueueSize
#define ADTimePixPrvHstFilePathExistsString     "PRV_HST_FILE_PATH_EXISTS"  // (asynInt32,       r/w)      File path exists? */

    // Measurement
#define ADTimePixPelRateString               "TPX3_PEL_RATE"          // (asynInt32,         w)      PixelEventRate
#define ADTimePixTdc1RateString              "TPX3_TDC1_RATE"         // (asynInt32,         w)      Tdc1EventRate
#define ADTimePixTdc2RateString              "TPX3_TDC2_RATE"         // (asynInt32,         w)      Tdc2EventRate
#define ADTimePixStartTimeString             "TPX3_START_TIME"        // (asynInt64,         w)      StartDateTime
#define ADTimePixElapsedTimeString           "TPX3_ELAPSED_TIME"      // (asynFloat64,       w)      ElapsedTime
#define ADTimePixTimeLeftString              "TPX3_TIME_LEFT"         // (asynFloat64,       w)      TimeLeft
#define ADTimePixFrameCountString            "TPX3_FRAME_COUNT"       // (asynInt32,         w)      FrameCount
#define ADTimePixDroppedFramesString         "TPX3_DROPPED_FRAMES"    // (asynInt32,         w)      DroppedFrames
#define ADTimePixStatusString                "TPX3_MSMT_STATUS"       // (asynOctet,         w)      Status

// Place any required inclues here

#include "ADDriver.h"
#include "cpr/cpr.h"
#include "nlohmann/json.hpp"
#include <Magick++.h>
using namespace Magick;



// ----------------------------------------
// ADTimePix3 Data Structures
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
        virtual asynStatus writeOctet(asynUser *pasynUser, const char *value, size_t nChars, size_t *nActual);
        virtual asynStatus writeInt32(asynUser* pasynUser, epicsInt32 value);
        virtual asynStatus writeFloat64(asynUser* pasynUser, epicsFloat64 value);


        void timePixCallback();

        // destructor. Disconnects from camera, deletes the object
        ~ADTimePix();

    protected:
        int ADTimePixHttpCode;
        #define ADTIMEPIX_FIRST_PARAM ADTimePixHttpCode
        int ADTimePixWriteSpeed;

        int ADTimePixDetType;
        int ADTimePixFWTimeStamp;
    //    int ADTimePixDetConnected;    // TODO

        int ADTimePixServer;

    // Health
        int ADTimePixLocalTemp;
        int ADTimePixFPGATemp;
        int ADTimePixFan1Speed;
        int ADTimePixFan2Speed;
        int ADTimePixBiasVoltage;
        int ADTimePixHumidity;
        int ADTimePixChipTemperature;
        int ADTimePixVDD;
        int ADTimePixAVDD;
        int ADTimePixHealth;

        // Detector Info
        int ADTimePixIfaceName;            
        int ADTimePixSW_version;           
        int ADTimePixFW_version;           
        int ADTimePixPixCount;             
        int ADTimePixRowLen;               
        int ADTimePixNumberOfChips;        
        int ADTimePixNumberOfRows;         
        int ADTimePixMpxType;
        
        int ADTimePixBoardsID;             
        int ADTimePixBoardsIP;
        int ADTimePixBoardsCh1;            
        int ADTimePixBoardsCh2;            
        int ADTimePixBoardsCh3;           
        int ADTimePixBoardsCh4;            
         
        int ADTimePixSuppAcqModes;         
        int ADTimePixClockReadout;         
        int ADTimePixMaxPulseCount;        
        int ADTimePixMaxPulseHeight;       
        int ADTimePixMaxPulsePeriod;       
        int ADTimePixTimerMaxVal;          
        int ADTimePixTimerMinVal;          
        int ADTimePixTimerStep;            
        int ADTimePixClockTimepix;         
         
             // Detector Config
        int ADTimePixFan1PWM;                  
        int ADTimePixFan2PWM;                  
        int ADTimePixBiasVolt;                 
        int ADTimePixBiasEnable;               
        int ADTimePixChainMode;                
        int ADTimePixTriggerIn;                
        int ADTimePixTriggerOut;               
        int ADTimePixPolarity;                 
        int ADTimePixTriggerMode;
        int ADTimePixExposureTime;             
        int ADTimePixTriggerPeriod;            
        int ADTimePixnTriggers;
        int ADTimePixPeriphClk80;              
        int ADTimePixTriggerDelay;
        int ADTimePixTdc;           
        int ADTimePixTdc0;
        int ADTimePixTdc1;                     
        int ADTimePixGlobalTimestampInterval;  
        int ADTimePixExternalReferenceClock;   
        int ADTimePixLogLevel;                 
         
             // Detector Chips: Chip0
        int ADTimePixChip0CP_PLL;              
        int ADTimePixChip0DiscS1OFF;           
        int ADTimePixChip0DiscS1ON;            
        int ADTimePixChip0DiscS2OFF;           
        int ADTimePixChip0DiscS2ON;            
        int ADTimePixChip0Ikrum;               
        int ADTimePixChip0PixelDAC;            
        int ADTimePixChip0PreampOFF;           
        int ADTimePixChip0PreampON;            
        int ADTimePixChip0TPbufferIn;          
        int ADTimePixChip0TPbufferOut;         
        int ADTimePixChip0PLL_Vcntrl;          
        int ADTimePixChip0VPreampNCAS;         
        int ADTimePixChip0VTPcoarse;           
        int ADTimePixChip0VTPfine;             
        int ADTimePixChip0Vfbk;                
        int ADTimePixChip0VthresholdCoarse;    
        int ADTimePixChip0VTthresholdFine;     
        int ADTimePixChip0Adjust;              
             // Detector Chips: Chip1
        int ADTimePixChip1CP_PLL;              
        int ADTimePixChip1DiscS1OFF;           
        int ADTimePixChip1DiscS1ON;            
        int ADTimePixChip1DiscS2OFF;           
        int ADTimePixChip1DiscS2ON;            
        int ADTimePixChip1Ikrum;               
        int ADTimePixChip1PixelDAC;            
        int ADTimePixChip1PreampOFF;           
        int ADTimePixChip1PreampON;            
        int ADTimePixChip1TPbufferIn;          
        int ADTimePixChip1TPbufferOut;         
        int ADTimePixChip1PLL_Vcntrl;          
        int ADTimePixChip1VPreampNCAS;         
        int ADTimePixChip1VTPcoarse;           
        int ADTimePixChip1VTPfine;             
        int ADTimePixChip1Vfbk;                
        int ADTimePixChip1VthresholdCoarse;    
        int ADTimePixChip1VTthresholdFine;     
        int ADTimePixChip1Adjust;              
             // Detector Chips: Chip2
        int ADTimePixChip2CP_PLL;              
        int ADTimePixChip2DiscS1OFF;           
        int ADTimePixChip2DiscS1ON;            
        int ADTimePixChip2DiscS2OFF;           
        int ADTimePixChip2DiscS2ON;            
        int ADTimePixChip2Ikrum;               
        int ADTimePixChip2PixelDAC;           
        int ADTimePixChip2PreampOFF;           
        int ADTimePixChip2PreampON;            
        int ADTimePixChip2TPbufferIn;          
        int ADTimePixChip2TPbufferOut;         
        int ADTimePixChip2PLL_Vcntrl;          
        int ADTimePixChip2VPreampNCAS;         
        int ADTimePixChip2VTPcoarse;           
        int ADTimePixChip2VTPfine;             
        int ADTimePixChip2Vfbk;                
        int ADTimePixChip2VthresholdCoarse;    
        int ADTimePixChip2VTthresholdFine;     
        int ADTimePixChip2Adjust;              
             // Detector Chips: Chip3
        int ADTimePixChip3CP_PLL;
        int ADTimePixChip3DiscS1OFF;
        int ADTimePixChip3DiscS1ON;
        int ADTimePixChip3DiscS2OFF;
        int ADTimePixChip3DiscS2ON;
        int ADTimePixChip3Ikrum;
        int ADTimePixChip3PixelDAC;
        int ADTimePixChip3PreampOFF;
        int ADTimePixChip3PreampON;
        int ADTimePixChip3TPbufferIn;
        int ADTimePixChip3TPbufferOut;
        int ADTimePixChip3PLL_Vcntrl;
        int ADTimePixChip3VPreampNCAS;
        int ADTimePixChip3VTPcoarse;
        int ADTimePixChip3VTPfine;
        int ADTimePixChip3Vfbk;
        int ADTimePixChip3VthresholdCoarse;
        int ADTimePixChip3VTthresholdFine;
        int ADTimePixChip3Adjust;
            
            //  Detector Chip layout
        int ADTimePixDetectorOrientation;
        int ADTimePixChip0Layout;
        int ADTimePixChip1Layout;
        int ADTimePixChip2Layout;
        int ADTimePixChip3Layout;    

            // Files BPC, Chip/DACS
        int ADTimePixBPCFilePath;          
        int ADTimePixBPCFilePathExists;    
        int ADTimePixBPCFileName;          
        int ADTimePixDACSFilePath;         
        int ADTimePixDACSFilePathExists;   
        int ADTimePixDACSFileName;
        int ADTimePixWriteMsg; 
        int ADTimePixWriteBPCFile;                
        int ADTimePixWriteDACSFile;

            // Server, write output channels/modes
        int ADTimePixWriteData;    
        int ADTimePixWriteRaw;      
        int ADTimePixWriteImg;      
        int ADTimePixWritePrvImg;   
        int ADTimePixWritePrvImg1;  
        int ADTimePixWritePrvHst;

            // Server, raw
        int ADTimePixRawBase;              
        int ADTimePixRawFilePat;           
        int ADTimePixRawSplitStrategy;    
        int ADTimePixRawQueueSize;
        int ADTimePixRawFilePathExists;

            // Server, Image
        int ADTimePixImgBase;           
        int ADTimePixImgFilePat;        
        int ADTimePixImgFormat;         
        int ADTimePixImgMode;           
        int ADTimePixImgThs;            
        int ADTimePixImgIntSize; 
        int ADTimePixImgIntMode;       
        int ADTimePixImgStpOnDskLim;    
        int ADTimePixImgQueueSize;      
        int ADTimePixImgFilePathExists; 

            // Server, Preview
        int ADTimePixPrvPeriod;            
        int ADTimePixPrvSamplingMode;      
            // Server, Preview, ImageChannels[0]
        int ADTimePixPrvImgBase;           
        int ADTimePixPrvImgFilePat;        
        int ADTimePixPrvImgFormat;         
        int ADTimePixPrvImgMode;           
        int ADTimePixPrvImgThs;           
        int ADTimePixPrvImgIntSize; 
        int ADTimePixPrvImgIntMode;      
        int ADTimePixPrvImgStpOnDskLim;   
        int ADTimePixPrvImgQueueSize;
        int ADTimePixPrvImgFilePathExists;     
            // Server, Preview, ImageChannel[1]
        int ADTimePixPrvImg1Base;         
        int ADTimePixPrvImg1Format;       
        int ADTimePixPrvImg1Mode;         
        int ADTimePixPrvImg1Ths;        
        int ADTimePixPrvImg1IntSize; 
        int ADTimePixPrvImg1IntMode;
        int ADTimePixPrvImg1StpOnDskLim;
        int ADTimePixPrvImg1QueueSize;  
    //    int ADTimePixPrvImg1FilePathExists;
            // Server, Preview, HistogramChannels[0]
        int ADTimePixPrvHstBase;           
        int ADTimePixPrvHstFilePat;        
        int ADTimePixPrvHstFormat;         
        int ADTimePixPrvHstMode;           
        int ADTimePixPrvHstThs;           
        int ADTimePixPrvHstIntSize;
        int ADTimePixPrvHstIntMode;      
        int ADTimePixPrvHstStpOnDskLim;   
        int ADTimePixPrvHstQueueSize;
        int ADTimePixPrvHstFilePathExists;    

            // Measurement
        int ADTimePixPelRate;        
        int ADTimePixTdc1Rate;
        int ADTimePixTdc2Rate;        
        int ADTimePixStartTime;      
        int ADTimePixElapsedTime;    
        int ADTimePixTimeLeft;       
        int ADTimePixFrameCount;     
        int ADTimePixDroppedFrames;  
        int ADTimePixStatus;         

        int ADTimePixFreeSpace;
        #define ADTIMEPIX_LAST_PARAM ADTimePixFreeSpace

    private:

        // Some data variables
        epicsEventId startEventId;
        epicsEventId endEventId;
        

        std::string serverURL;
        Image image;

        bool acquiring=false;

        epicsThreadId callbackThreadId;

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

        // TimePix3 specific functions
        asynStatus getDashboard();
        asynStatus getServer();
        asynStatus getHealth();
        asynStatus getDetector();
        asynStatus initCamera();
        asynStatus initAcquisition();
        asynStatus checkBPCPath();
        asynStatus checkDACSPath();
        asynStatus checkRawPath();
        asynStatus checkImgPath();
        asynStatus checkPrvImgPath();
        asynStatus checkPrvHstPath();
        bool checkPath(std::string &filePath);
        asynStatus uploadBPC();
        asynStatus uploadDACS();
        asynStatus readImage();
        asynStatus fileWriter();

};

// Stores number of additional PV parameters are added by the driver
#define NUM_TIMEPIX_PARAMS ((int)(&ADTIMEPIX_LAST_PARAM - &ADTIMEPIX_FIRST_PARAM + 1))

#endif
