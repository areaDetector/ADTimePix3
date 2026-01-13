/**
 * This is a driver for the TimePix3 pixel array detector 
 * 
 * Author: Kazimierz Gofron
 * Created On: June, 2022
 * Last Edited: July 20, 2025
 * Copyright (c): 2022 Brookhaven National Laboratory
 * Copyright (c): 2025 Oak Ridge National Laboratory
 */

// Standard includes
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cstring>

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
// Add any additional namespaces here

// NetworkClient class implementation
NetworkClient::NetworkClient() : socket_fd_(-1), connected_(false) {}

NetworkClient::~NetworkClient() {
    disconnect();
}

// Move constructor
NetworkClient::NetworkClient(NetworkClient&& other) noexcept
    : socket_fd_(other.socket_fd_), connected_(other.connected_) {
    other.socket_fd_ = -1;
    other.connected_ = false;
}

NetworkClient& NetworkClient::operator=(NetworkClient&& other) noexcept {
    if (this != &other) {
        disconnect();
        socket_fd_ = other.socket_fd_;
        connected_ = other.connected_;
        other.socket_fd_ = -1;
        other.connected_ = false;
    }
    return *this;
}

bool NetworkClient::connect(const std::string& host, int port) {
    // Close existing connection if any
    if (socket_fd_ >= 0) {
        ::close(socket_fd_);
        socket_fd_ = -1;
    }
    connected_ = false;
    
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        std::cerr << "Socket creation failed: " << strerror(errno) << std::endl;
        return false;
    }

    // Enable TCP keepalive
    int opt = 1;
    if (setsockopt(socket_fd_, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt)) < 0) {
        std::cerr << "Failed to set SO_KEEPALIVE: " << strerror(errno) << std::endl;
    }
    
    // Configure TCP keepalive parameters
    int keepidle = 60;
    int keepintvl = 10;
    int keepcnt = 3;
    
    if (setsockopt(socket_fd_, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle)) < 0) {
        // Not critical
    }
    if (setsockopt(socket_fd_, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl)) < 0) {
        // Not critical
    }
    if (setsockopt(socket_fd_, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt)) < 0) {
        // Not critical
    }

    // Set socket buffers
    int rcvbuf = 64 * 1024;
    if (setsockopt(socket_fd_, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf)) < 0) {
        std::cerr << "Failed to set receive buffer size: " << strerror(errno) << std::endl;
    }
    
    // Set SO_LINGER
    struct linger linger_opt;
    linger_opt.l_onoff = 1;
    linger_opt.l_linger = 5;
    if (setsockopt(socket_fd_, SOL_SOCKET, SO_LINGER, &linger_opt, sizeof(linger_opt)) < 0) {
        // Not critical
    }

    struct sockaddr_in server_addr{};
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    // Try to parse as IP address first
    if (inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr) <= 0) {
        // If not an IP address, try to resolve as hostname using getaddrinfo
        struct addrinfo hints;
        struct addrinfo *result = NULL;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        
        char port_str[16];
        snprintf(port_str, sizeof(port_str), "%d", port);
        
        int err = getaddrinfo(host.c_str(), port_str, &hints, &result);
        if (err != 0 || result == NULL) {
            std::cerr << "Invalid address or hostname: " << host;
            if (err != 0) {
                std::cerr << " (" << gai_strerror(err) << ")";
            }
            std::cerr << std::endl;
            ::close(socket_fd_);
            socket_fd_ = -1;
            return false;
        }
        
        // Use the first result (IPv4 address)
        struct sockaddr_in *addr_in = (struct sockaddr_in *)result->ai_addr;
        server_addr.sin_addr = addr_in->sin_addr;
        
        freeaddrinfo(result);
    }
    
    if (::connect(socket_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Connection failed: " << strerror(errno) << std::endl;
        ::close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }
    
    connected_ = true;
    return true;
}

void NetworkClient::disconnect() {
    if (socket_fd_ >= 0) {
        ::close(socket_fd_);
        socket_fd_ = -1;
    }
    connected_ = false;
}

ssize_t NetworkClient::receive(char* buffer, size_t max_size) {
    if (!connected_ || socket_fd_ < 0) {
        return -1;
    }
    
    ssize_t bytes_read = recv(socket_fd_, buffer, max_size, 0);
    
    if (bytes_read == 0) {
        // Connection closed by peer - let the caller log this with context (channel name)
        connected_ = false;
    } else if (bytes_read < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            // Socket error - let the caller log this with context (channel name)
            connected_ = false;
        }
    }
    
    return bytes_read;
}

bool NetworkClient::receive_exact(char* buffer, size_t size) {
    size_t total_received = 0;
    
    while (total_received < size) {
        ssize_t bytes = receive(buffer + total_received, size - total_received);
        
        if (bytes <= 0) {
            return false;
        }
        
        total_received += bytes;
    }
    
    return true;
}


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
// ../ADTimePix.cpp:97:6: error: specializing member 'std::__cxx11::basic_string<char>::quotes' requires 'template<>' syntax
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

/**
 * Unified path checking function for all channel types
 * Replaces individual checkRawPath(), checkRaw1Path(), checkImgPath(), etc.
 */
asynStatus ADTimePix::checkChannelPath(int baseParam, int streamParam, int filePathExistsParam, 
                                      const std::string& channelName, const std::string& errorMessage) {
    asynStatus status;
    std::string filePath, fileOrStream;
    int pathExists = 0;

    getStringParam(baseParam, filePath);
    if (filePath.size() == 0) return asynSuccess;

    if (filePath.size() > 6) {
        // Check for file:/ protocol with strict single slash enforcement
        if (filePath.compare(0, 5, "file:") == 0) {
            // Check that the character at position 5 is exactly one '/'
            // The format should be 'file:/path' where the '/' is at position 5
            // Additional '/' characters in the path are valid
            if (filePath.length() > 5 && filePath[5] == '/' && (filePath.length() == 6 || filePath[6] != '/')) {
                // Valid format: 'file:/path' - single '/' at position 5, not followed by another '/'
                if (streamParam != -1) {  // Only set stream parameter if it exists
                    setIntegerParam(streamParam, 0);
                }
                fileOrStream = filePath.substr(5);  // Remove "file:" prefix, keep the "/"
                pathExists = checkPath(fileOrStream);
            } else {
                printf("Invalid file path format: '%s'. Expected format: 'file:/path'.\n", filePath.c_str());
                printf("Debug: length=%zu, char[5]='%c', char[6]='%c'\n", 
                       filePath.length(), 
                       filePath.length() > 5 ? filePath[5] : '?',
                       filePath.length() > 6 ? filePath[6] : '?');
                if (streamParam != -1) {
                    setIntegerParam(streamParam, 3);
                }
                pathExists = 0;
            }
        }
        // Check for http:// protocol using find() instead of substr()
        else if (filePath.find("http://") == 0) {       // streaming, http://localhost:8081
            if (streamParam != -1) {  // Only set stream parameter if it exists
                setIntegerParam(streamParam, 1);
            }
            fileOrStream = filePath.substr(7);  // Remove "http://" prefix
            pathExists = 1;
        }   
        // Check for tcp:// protocol using find() instead of substr()
        else if (filePath.find("tcp://") == 0) {       // streaming, tcp://localhost:8085
            if (streamParam != -1) {  // Only set stream parameter if it exists
                setIntegerParam(streamParam, 2);
            }
            fileOrStream = filePath.substr(6);  // Remove "tcp://" prefix
            pathExists = 1;
        }
        else {
            printf("%s\n", errorMessage.c_str());
            if (streamParam != -1) {  // Only set stream parameter if it exists
                setIntegerParam(streamParam, 3);
            }
            pathExists = 0;
        }
    }

    status = pathExists ? asynSuccess : asynError;
    setStringParam(baseParam, filePath);
    setIntegerParam(filePathExistsParam, pathExists);
    return status;
}

asynStatus ADTimePix::checkRawPath()
{
    return checkChannelPath(ADTimePixRawBase, ADTimePixRawStream, ADTimePixRawFilePathExists,
                          "Raw", "Raw file path must be file:/path_to_raw_folder, http://localhost:8081, or tcp://localhost:8085");
}

/* Serval 3.3.0 allows writing raw .tpx3 file, and stream data. This is the 2nd Base channel
* to either write .tpx3 file, or stream to socket.
* Operator decides which Raw or Raw1 is used for streaming, and which for .tpx3 file.
*/
asynStatus ADTimePix::checkRaw1Path()
{
    return checkChannelPath(ADTimePixRaw1Base, ADTimePixRaw1Stream, ADTimePixRaw1FilePathExists,
                          "Raw1", "Raw1 file path must be file:/path_to_raw_folder, http://localhost:8081, or tcp://localhost:8085");
}

asynStatus ADTimePix::checkImgPath()
{
    static const char* functionName = "checkImgPath";
    asynStatus status = checkChannelPath(ADTimePixImgBase, -1, ADTimePixImgFilePathExists,
                          "Img", "Img file path must be file:/path_to_img_folder, http://localhost:8081, or tcp://listen@hostname:port");
    
    // If TCP path, parse host and port
    if (status == asynSuccess) {
        std::string filePath;
        getStringParam(ADTimePixImgBase, filePath);
        if (filePath.find("tcp://") == 0) {
            std::string host;
            int port;
            if (parseTcpPath(filePath, host, port)) {
                epicsMutexLock(imgMutex_);
                imgHost_ = host;
                imgPort_ = port;
                getIntegerParam(ADTimePixImgFormat, &imgFormat_);
                epicsMutexUnlock(imgMutex_);
                LOG_ARGS("Parsed Img TCP path: host=%s, port=%d", host.c_str(), port);
            } else {
                ERR_ARGS("Failed to parse Img TCP path: %s", filePath.c_str());
                setIntegerParam(ADTimePixImgFilePathExists, 0);
                return asynError;
            }
        }
    }
    
    return status;
}

asynStatus ADTimePix::checkImg1Path()
{
    return checkChannelPath(ADTimePixImg1Base, -1, ADTimePixImg1FilePathExists,
                          "Img1", "Img1 file path must be file:/path_to_img_folder, http://localhost:8081, or tcp://localhost:8085");
}

asynStatus ADTimePix::checkPrvImgPath()
{
    static const char* functionName = "checkPrvImgPath";
    asynStatus status = checkChannelPath(ADTimePixPrvImgBase, -1, ADTimePixPrvImgFilePathExists,
                          "PrvImg", "PrvImg file path must be file:/path_to_img_folder, http://localhost:8081, or tcp://listen@hostname:port");
    
    // If TCP path, parse host and port
    if (status == asynSuccess) {
        std::string filePath;
        getStringParam(ADTimePixPrvImgBase, filePath);
        if (filePath.find("tcp://") == 0) {
            std::string host;
            int port;
            if (parseTcpPath(filePath, host, port)) {
                epicsMutexLock(prvImgMutex_);
                prvImgHost_ = host;
                prvImgPort_ = port;
                getIntegerParam(ADTimePixPrvImgFormat, &prvImgFormat_);
                epicsMutexUnlock(prvImgMutex_);
                LOG_ARGS("Parsed PrvImg TCP path: host=%s, port=%d", host.c_str(), port);
            } else {
                ERR_ARGS("Failed to parse PrvImg TCP path: %s", filePath.c_str());
                setIntegerParam(ADTimePixPrvImgFilePathExists, 0);
                return asynError;
            }
        }
    }
    
    return status;
}

bool ADTimePix::parseTcpPath(const std::string& filePath, std::string& host, int& port) {
    static const char* functionName = "parseTcpPath";
    // Parse tcp://listen@hostname:port or tcp://hostname:port
    // Examples: tcp://listen@localhost:8089, tcp://127.0.0.1:8089
    
    if (filePath.find("tcp://") != 0) {
        return false;
    }
    
    std::string remaining = filePath.substr(6); // Remove "tcp://"
    
    // Handle "listen@hostname:port" format
    size_t at_pos = remaining.find('@');
    if (at_pos != std::string::npos) {
        remaining = remaining.substr(at_pos + 1); // Skip "listen@"
    }
    
    // Parse hostname:port
    size_t colon_pos = remaining.find(':');
    if (colon_pos == std::string::npos) {
        return false;
    }
    
    host = remaining.substr(0, colon_pos);
    std::string port_str = remaining.substr(colon_pos + 1);
    
    try {
        port = std::stoi(port_str);
        if (port < 1 || port > 65535) {
            ERR_ARGS("Invalid port number: %d (must be 1-65535)", port);
            return false;
        }
    } catch (const std::exception& e) {
        ERR_ARGS("Failed to parse port: %s", port_str.c_str());
        return false;
    }
    
    return true;
}

asynStatus ADTimePix::checkPrvImg1Path()
{
    return checkChannelPath(ADTimePixPrvImg1Base, -1, ADTimePixPrvImg1FilePathExists,
                          "PrvImg1", "PrvImg1 file path must be file:/path_to_img_folder, http://localhost:8081, or tcp://localhost:8085");
}

asynStatus ADTimePix::checkPrvHstPath() // file:/, http://, tcp:// format
{
    return checkChannelPath(ADTimePixPrvHstBase, ADTimePixPrvHstStream, ADTimePixPrvHstFilePathExists,
                          "PrvHst", "Prv Histogram path must be file:/path_to_folder, http://localhost:8081, or tcp://localhost:8085");
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
        printf("\n\nCONNECTED to Welcome URI! (Serval running), http_code = %li\n", r.status_code);

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
            setIntegerParam(ADTimePixLLimReached,   int(dashboard_j["Server"]["DiskSpace"][0]["DiskLimitReached"]));   // bool->int true->1, false->0
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

/**
 * Initialize detector - upload Binary Pixel Configuration
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
 * Initialize detector - upload Chips DACS
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
    const char* functionName = "getParameterSafely";
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
    const char* functionName = "getParameterSafely";
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
    const char* functionName = "getParameterSafely";
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
    const char* functionName = "configureRawChannel";
    
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
    const char* functionName = "configureImageChannel";
    
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
    const char* functionName = "configurePreviewSettings";
    
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
    const char* functionName = "configureHistogramChannel";
    
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
    const char* functionName = "sendConfiguration";
    
    std::string server = this->serverURL + "/server/destination";
    
    // Log configuration for debugging
    printf("server=%s\n", config.dump(3, ' ', true).c_str());
    
    // Send HTTP request with timeout
    cpr::Response r = cpr::Put(cpr::Url{server},
                               cpr::Body{config.dump().c_str()},
                               cpr::Header{{"Content-Type", "application/json"}},
                               cpr::Timeout{10000}); // 10 second timeout
    
    setIntegerParam(ADTimePixHttpCode, r.status_code);
    setStringParam(ADTimePixWriteMsg, r.text.c_str());
    
    if (r.status_code != 200) {
        ERR_ARGS("HTTP request failed with status code: %li, response: %s", 
                 r.status_code, r.text.c_str());
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
    const char* functionName = "fileWriter";
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
    printf("\n\ninitCamera2: http_code = %li\n", r.status_code);
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
# The next two functions can be used when a separate image acquisition thread is required by the driver. 
# Some vendor software already creates its own acquisition thread for asynchronous use, but if not this
# must be used. By default, the acquireStart() function is written to not use these. If they are needed, 
# find the call to tpx3Callback in acquireStart(), and change it to startImageAcquisitionThread
#
#####################################################################################################################
*/




/**
 * Function responsible for starting camera image acquisition. First, check if there is a
 * camera connected. Then, set camera values by reading from PVs. Then, we execute the 
 * Acquire Start command. if this command was successful, image acquisition started.
 * 
 * @return: status  -> error if no device, camera values not set, or execute command fails. Otherwise, success
 */
asynStatus ADTimePix::acquireStart(){
    static const char* functionName = "acquireStart";
    asynStatus status = asynSuccess;

    // Ensure any existing PrvImg TCP connection is disconnected before starting new measurement
    // This prevents port conflicts
    if (prvImgMutex_) {
        epicsMutexLock(prvImgMutex_);
        prvImgRunning_ = false;
        epicsMutexUnlock(prvImgMutex_);
    }
    if (prvImgWorkerThreadId_ != NULL && prvImgWorkerThreadId_ != epicsThreadGetIdSelf()) {
        epicsThreadMustJoin(prvImgWorkerThreadId_);
        prvImgWorkerThreadId_ = NULL;
    }
    prvImgDisconnect();
    
    // Ensure any existing Img TCP connection is disconnected before starting new measurement
    // This prevents port conflicts
    if (imgMutex_) {
        epicsMutexLock(imgMutex_);
        imgRunning_ = false;
        epicsMutexUnlock(imgMutex_);
    }
    if (imgWorkerThreadId_ != NULL && imgWorkerThreadId_ != epicsThreadGetIdSelf()) {
        epicsThreadMustJoin(imgWorkerThreadId_);
        imgWorkerThreadId_ = NULL;
    }
    imgDisconnect();

    setIntegerParam(ADStatus, ADStatusAcquire);

    epicsThreadOpts opts = EPICS_THREAD_OPTS_INIT;
    opts.joinable = 1;

    // Check if measurement is already running and stop it first to free ports
    string measurementURL = this->serverURL + std::string("/measurement");
    cpr::Response r = cpr::Get(cpr::Url{measurementURL},
                           cpr::Authentication{"user", "pass", cpr::AuthMode::BASIC},
                           cpr::Parameters{{"anon", "true"}, {"key", "value"}});
    
    if (r.status_code == 200 && !r.text.empty()) {
        try {
            json measurement_j;
            try {
                measurement_j = json::parse(r.text.c_str());
            } catch (const json::parse_error& e) {
                WARN_ARGS("Failed to parse measurement JSON: %s, continuing anyway", e.what());
                // Continue without checking status
                measurement_j = json::object();
            }
            // Safely check if Info and Status exist and are not null
            if (measurement_j.contains("Info") && measurement_j["Info"].is_object()) {
                if (measurement_j["Info"].contains("Status") && measurement_j["Info"]["Status"].is_string()) {
                    std::string status = measurement_j["Info"]["Status"].get<std::string>();
                    if (status != "DA_IDLE" && status != "DA_STOPPED") {
                        LOG_ARGS("Measurement is running (status: %s), stopping it first", status.c_str());
                        string stopMeasurementURL = this->serverURL + std::string("/measurement/stop");
                        cpr::Response stop_r = cpr::Get(cpr::Url{stopMeasurementURL},
                                                   cpr::Authentication{"user", "pass", cpr::AuthMode::BASIC},
                                                   cpr::Parameters{{"anon", "true"}, {"key", "value"}});
                        if (stop_r.status_code == 200) {
                            // Wait for Serval to release ports (minimum: 100ms)
                            epicsThreadSleep(0.1);  // 100ms - allows port release
                        } else {
                            WARN_ARGS("Failed to stop existing measurement (status: %ld), continuing anyway", stop_r.status_code);
                        }
                    }
                } else {
                    // Status field is missing or null, assume measurement is not running
                    LOG("Measurement status field is missing or null, assuming not running");
                }
            } else {
                // Info field is missing or not an object
                LOG("Measurement Info field is missing or invalid, assuming not running");
            }
        } catch (const json::parse_error& e) {
            WARN_ARGS("Failed to parse measurement JSON: %s, continuing anyway", e.what());
        } catch (const json::type_error& e) {
            WARN_ARGS("JSON type error when checking measurement status: %s, continuing anyway", e.what());
        } catch (const std::exception& e) {
            WARN_ARGS("Failed to check measurement status: %s, continuing anyway", e.what());
        }
    }

    string startMeasurementURL = this->serverURL + std::string("/measurement/start");
    r = cpr::Get(cpr::Url{startMeasurementURL},
                           cpr::Authentication{"user", "pass", cpr::AuthMode::BASIC},
                           cpr::Parameters{{"anon", "true"}, {"key", "value"}});

    if (r.status_code != 200){
        ERR_ARGS("Failed to start measurement! Status code: %ld", r.status_code);
        // Ensure any partially started worker thread is stopped
        epicsMutexLock(prvImgMutex_);
        prvImgRunning_ = false;
        epicsMutexUnlock(prvImgMutex_);
        if (prvImgWorkerThreadId_ != NULL && prvImgWorkerThreadId_ != epicsThreadGetIdSelf()) {
            epicsThreadMustJoin(prvImgWorkerThreadId_);
            prvImgWorkerThreadId_ = NULL;
        }
        prvImgDisconnect();
        
        epicsMutexLock(imgMutex_);
        imgRunning_ = false;
        epicsMutexUnlock(imgMutex_);
        if (imgWorkerThreadId_ != NULL && imgWorkerThreadId_ != epicsThreadGetIdSelf()) {
            epicsThreadMustJoin(imgWorkerThreadId_);
            imgWorkerThreadId_ = NULL;
        }
        imgDisconnect();
        return asynError;
    }

    this->callbackThreadId = epicsThreadCreateOpt("timePixCallback", timePixCallbackC, this, &opts);
    this->acquiring = true;
    
    // Start PrvImg TCP streaming worker thread if WritePrvImg is enabled and path is TCP
    // Wait a bit for Serval to bind to the port before trying to connect
    int writePrvImg;
    getIntegerParam(ADTimePixWritePrvImg, &writePrvImg);
    if (writePrvImg != 0) {
        std::string prvImgPath;
        getStringParam(ADTimePixPrvImgBase, prvImgPath);
        if (prvImgPath.find("tcp://") == 0) {
            // Give Serval time to bind to the TCP port (minimum: 200ms)
            epicsThreadSleep(0.2);  // 200ms - allows Serval to bind TCP port and start server
            
            epicsMutexLock(prvImgMutex_);
            if (!prvImgRunning_ && !prvImgWorkerThreadId_) {
                prvImgRunning_ = true;
                prvImgWorkerThreadId_ = epicsThreadCreateOpt("prvImgWorker", prvImgWorkerThreadC, this, &opts);
                if (!prvImgWorkerThreadId_) {
                    ERR("Failed to create PrvImg worker thread");
                    prvImgRunning_ = false;
                } else {
                    LOG("Started PrvImg TCP worker thread in acquireStart");
                }
            }
            epicsMutexUnlock(prvImgMutex_);
        }
    }
    
    // Start Img TCP streaming worker thread if WriteImg is enabled, path is TCP, and accumulation is enabled
    // If accumulation is disabled, don't connect to TCP port so other clients can connect
    int writeImg;
    getIntegerParam(ADTimePixWriteImg, &writeImg);
    if (writeImg != 0) {
        std::string imgPath;
        getStringParam(ADTimePixImgBase, imgPath);
        if (imgPath.find("tcp://") == 0) {
            int accumulationEnable;
            getIntegerParam(ADTimePixImgAccumulationEnable, &accumulationEnable);
            if (accumulationEnable) {
                // Give Serval time to bind to the TCP port (minimum: 200ms)
                epicsThreadSleep(0.2);  // 200ms - allows Serval to bind TCP port and start server
                
                epicsMutexLock(imgMutex_);
                if (!imgRunning_ && !imgWorkerThreadId_) {
                    imgRunning_ = true;
                    imgWorkerThreadId_ = epicsThreadCreateOpt("imgWorker", imgWorkerThreadC, this, &opts);
                    if (!imgWorkerThreadId_) {
                        ERR("Failed to create Img worker thread");
                        imgRunning_ = false;
                    } else {
                        LOG("Started Img TCP worker thread in acquireStart");
                    }
                }
                epicsMutexUnlock(imgMutex_);
            } else {
                LOG("ImgAccumulationEnable is disabled - not connecting to TCP port (other clients can connect)");
            }
        }
    }
    
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

    // NDArray* pImage; // Not used with TCP streaming - worker thread handles image processing
    int arrayCallbacks;
    epicsTimeStamp startTime, endTime;
//    double elapsedTime;
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

    // Safely extract measurement info with null checks
    if (measurement_j.contains("Info") && measurement_j["Info"].is_object()) {
        if (measurement_j["Info"].contains("PixelEventRate") && measurement_j["Info"]["PixelEventRate"].is_number()) {
            setIntegerParam(ADTimePixPelRate, measurement_j["Info"]["PixelEventRate"].get<int>());
        }

        getStringParam(ADSDKVersion, API_Ver);
        if ((API_Ver[0] == '4') || ((API_Ver[0] == '3') && ((API_Ver[2] - '0') >= 2))) {    // Serval 4.0.0 and later; Serval 3.2.0 and later
            if (measurement_j["Info"].contains("Tdc1EventRate") && measurement_j["Info"]["Tdc1EventRate"].is_number()) {
                setIntegerParam(ADTimePixTdc1Rate, measurement_j["Info"]["Tdc1EventRate"].get<int>());
            }
            if (measurement_j["Info"].contains("Tdc2EventRate") && measurement_j["Info"]["Tdc2EventRate"].is_number()) {
                setIntegerParam(ADTimePixTdc2Rate, measurement_j["Info"]["Tdc2EventRate"].get<int>());
            }
        } else if ((API_Ver[0] == '3') && ((API_Ver[2] - '0') <= 1)) {   // Serval 3.1.0 and 3.0.0
            if (measurement_j["Info"].contains("TdcEventRate") && measurement_j["Info"]["TdcEventRate"].is_number()) {
                setIntegerParam(ADTimePixTdc1Rate, measurement_j["Info"]["TdcEventRate"].get<int>());
            }
        } else {
            printf ("Serval Version not compared, event rate not read\n");
        }

        if (measurement_j["Info"].contains("StartDateTime") && measurement_j["Info"]["StartDateTime"].is_number()) {
            setInteger64Param(ADTimePixStartTime, measurement_j["Info"]["StartDateTime"].get<long>());
        }
        if (measurement_j["Info"].contains("ElapsedTime") && measurement_j["Info"]["ElapsedTime"].is_number()) {
            setDoubleParam(ADTimePixElapsedTime, measurement_j["Info"]["ElapsedTime"].get<double>());
        }
        if (measurement_j["Info"].contains("TimeLeft") && measurement_j["Info"]["TimeLeft"].is_number()) {
            setDoubleParam(ADTimePixTimeLeft, measurement_j["Info"]["TimeLeft"].get<double>());
        }
        if (measurement_j["Info"].contains("FrameCount") && measurement_j["Info"]["FrameCount"].is_number()) {
            setIntegerParam(ADTimePixFrameCount, measurement_j["Info"]["FrameCount"].get<int>());
        }
        if (measurement_j["Info"].contains("DroppedFrames") && measurement_j["Info"]["DroppedFrames"].is_number()) {
            setIntegerParam(ADTimePixDroppedFrames, measurement_j["Info"]["DroppedFrames"].get<int>());
        }
        if (measurement_j["Info"].contains("Status")) {
            // Status might be null, so use dump() which handles null safely
            setStringParam(ADTimePixStatus, measurement_j["Info"]["Status"].dump().c_str());
        }
    }   
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
            
            // Safely extract measurement info with null checks
            if (measurement_j.contains("Info") && measurement_j["Info"].is_object()) {
                if (measurement_j["Info"].contains("PixelEventRate") && measurement_j["Info"]["PixelEventRate"].is_number()) {
                    setIntegerParam(ADTimePixPelRate, measurement_j["Info"]["PixelEventRate"].get<int>());
                }
                
                if ((API_Ver[0] == '4') || ((API_Ver[0] == '3') && ((API_Ver[2] - '0') >= 2))) {    // Serval 4.0.0 and later; Serval 3.2.0 and later
                    if (measurement_j["Info"].contains("Tdc1EventRate") && measurement_j["Info"]["Tdc1EventRate"].is_number()) {
                        setIntegerParam(ADTimePixTdc1Rate, measurement_j["Info"]["Tdc1EventRate"].get<int>());
                    }
                    if (measurement_j["Info"].contains("Tdc2EventRate") && measurement_j["Info"]["Tdc2EventRate"].is_number()) {
                        setIntegerParam(ADTimePixTdc2Rate, measurement_j["Info"]["Tdc2EventRate"].get<int>());
                    }
                } else if ((API_Ver[0] == '3') && ((API_Ver[2] - '0') <= 1)) {   // Serval 3.1.0 and 3.0.0
                    if (measurement_j["Info"].contains("TdcEventRate") && measurement_j["Info"]["TdcEventRate"].is_number()) {
                        setIntegerParam(ADTimePixTdc1Rate, measurement_j["Info"]["TdcEventRate"].get<int>());
                    }
                } else {
                    printf ("Serval Version event rate not specified in while loop.\n");
                }

                if (measurement_j["Info"].contains("StartDateTime") && measurement_j["Info"]["StartDateTime"].is_number()) {
                    setInteger64Param(ADTimePixStartTime, measurement_j["Info"]["StartDateTime"].get<long>());
                }
                if (measurement_j["Info"].contains("ElapsedTime") && measurement_j["Info"]["ElapsedTime"].is_number()) {
                    setDoubleParam(ADTimePixElapsedTime, measurement_j["Info"]["ElapsedTime"].get<double>());
                }
                if (measurement_j["Info"].contains("TimeLeft") && measurement_j["Info"]["TimeLeft"].is_number()) {
                    setDoubleParam(ADTimePixTimeLeft, measurement_j["Info"]["TimeLeft"].get<double>());
                }
                if (measurement_j["Info"].contains("FrameCount") && measurement_j["Info"]["FrameCount"].is_number()) {
                    setIntegerParam(ADTimePixFrameCount, measurement_j["Info"]["FrameCount"].get<int>());
                    new_frame_num = measurement_j["Info"]["FrameCount"].get<int>();
                }
                if (measurement_j["Info"].contains("DroppedFrames") && measurement_j["Info"]["DroppedFrames"].is_number()) {
                    setIntegerParam(ADTimePixDroppedFrames, measurement_j["Info"]["DroppedFrames"].get<int>());
                }
                if (measurement_j["Info"].contains("Status")) {
                    // Status might be null, so use dump() which handles null safely
                    setStringParam(ADTimePixStatus, measurement_j["Info"]["Status"].dump().c_str());
                    // Check if status is "DA_IDLE" (only if it's a string)
                    if (measurement_j["Info"]["Status"].is_string() && 
                        measurement_j["Info"]["Status"].get<std::string>() == "DA_IDLE") {
                        isIdle = true;
                    }
                }
            }
            callParamCallbacks();
            
            if (isIdle || this->acquiring == false) {
                break;
            }

            epicsTimeGetCurrent(&endTime);
            // elapsedTime = epicsTimeDiffInSeconds(&endTime, &startTime);     // 0.0006->0.97 s
            // elapsedTime = r.elapsed;                                      // 0.00035 s
            // printf("Elapsed Time = %f\n", elapsedTime);

            epicsThreadSleep(0.01);
        //    epicsThreadSleep(0);
        }
        frameCounter = new_frame_num;

        getIntegerParam(ADTimePixWritePrvImg, &writeChannel);
        if (writeChannel != 0) {
            // Preview, ImageChannels[0]

            if(this->acquiring){
                // Check if we're using TCP streaming
                std::string prvImgPath;
                getStringParam(ADTimePixPrvImgBase, prvImgPath);
                bool usingTcp = (prvImgPath.find("tcp://") == 0);
                
                if (usingTcp) {
                    // For TCP streaming, the worker thread handles everything
                    // Just ensure it's running - no need to process image here
                    readImageFromTCP(); // Worker thread handles image processing
                    // Worker thread will update pArrays[0] and trigger callbacks asynchronously
                    // We just update counters based on frame count from measurement endpoint
                    setIntegerParam(ADNumImagesCounter, frameCounter);
                    callParamCallbacks();
                } else {
                    // Non-TCP path: TCP streaming is required for preview images
                    // GraphicsMagick HTTP method has been removed - use TCP streaming instead
                    WARN("PrvImg requires TCP streaming (tcp:// format). GraphicsMagick HTTP method no longer supported.");
                    // Worker thread handles TCP streaming, so just update counters
                    setIntegerParam(ADNumImagesCounter, frameCounter);
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
    static const char* functionName = "acquireStop";
    asynStatus status;
    std::string API_Ver;

    this->acquiring=false;
    
    // Stop callback thread first
    if(this->callbackThreadId != NULL && this->callbackThreadId != epicsThreadGetIdSelf())
        epicsThreadMustJoin(this->callbackThreadId);

    this->callbackThreadId = NULL;

    // Stop Serval measurement FIRST to tell Serval to stop sending new data
    // This MUST happen before we signal worker threads to exit, otherwise
    // worker threads will close the socket while Serval is still trying to write
    string stopMeasurementURL = this->serverURL + std::string("/measurement/stop");
    cpr::Response r = cpr::Get(cpr::Url{stopMeasurementURL},
                           cpr::Authentication{"user", "pass", cpr::AuthMode::BASIC},
                           cpr::Parameters{{"anon", "true"}, {"key", "value"}});

    if (r.status_code != 200){
        ERR("Failed to stop measurement!");
        return asynError;
    }
    
    // Wait for Serval to process the stop command and stop its TcpSender threads
    // Serval needs time to:
    // 1. Process the stop command
    // 2. Signal its TcpSender threads to stop
    // 3. Allow TcpSender threads to finish sending any buffered data
    // 4. Close Serval's side of the socket gracefully
    // Delay to prevent "Broken pipe" errors (minimum: 300ms for reliable operation)
    epicsThreadSleep(0.3);  // 300ms - allows Serval TcpSender threads to stop cleanly
    
    // NOW signal worker threads to stop (after Serval has stopped sending)
    // Worker threads will detect the closed connection (bytes_read <= 0) and exit cleanly
    if (prvImgMutex_) {
        epicsMutexLock(prvImgMutex_);
        prvImgRunning_ = false;
        // Reset metadata tracking for next acquisition
        prvImgFirstFrameReceived_ = false;
        prvImgAcquisitionRate_ = 0.0;
        prvImgRateSamples_.clear();
        setDoubleParam(ADTimePixPrvImgAcqRate, 0.0);
        epicsMutexUnlock(prvImgMutex_);
    }
    
    if (imgMutex_) {
        epicsMutexLock(imgMutex_);
        imgRunning_ = false;
        // Reset metadata tracking for next acquisition
        imgFirstFrameReceived_ = false;
        imgAcquisitionRate_ = 0.0;
        imgRateSamples_.clear();
        setDoubleParam(ADTimePixImgAcqRate, 0.0);
        // Reset accumulation data
        resetImgAccumulation();
        epicsMutexUnlock(imgMutex_);
    }
    
    // Join worker threads - they will detect closed connection (bytes_read <= 0) and exit
    // or exit when they see prvImgRunning_/imgRunning_ is false
    if (prvImgWorkerThreadId_ != NULL && prvImgWorkerThreadId_ != epicsThreadGetIdSelf()) {
        epicsThreadMustJoin(prvImgWorkerThreadId_);
        prvImgWorkerThreadId_ = NULL;
    }
    
    if (imgWorkerThreadId_ != NULL && imgWorkerThreadId_ != epicsThreadGetIdSelf()) {
        epicsThreadMustJoin(imgWorkerThreadId_);
        imgWorkerThreadId_ = NULL;
    }
    
    // Explicitly disconnect to ensure clean state
    // Worker threads may have already disconnected when they detected the closed connection
    prvImgDisconnect();
    imgDisconnect();

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

    // Safely extract measurement info with null checks
    if (measurement_j.contains("Info") && measurement_j["Info"].is_object()) {
        if (measurement_j["Info"].contains("PixelEventRate") && measurement_j["Info"]["PixelEventRate"].is_number()) {
            setIntegerParam(ADTimePixPelRate, measurement_j["Info"]["PixelEventRate"].get<int>());
        }

        getStringParam(ADSDKVersion, API_Ver);
        if ((API_Ver[0] == '4') || ((API_Ver[0] == '3') && ((API_Ver[2] - '0') >= 2))) {    // Serval 4.0.0 and later; Serval 3.2.0 and later
            if (measurement_j["Info"].contains("Tdc1EventRate") && measurement_j["Info"]["Tdc1EventRate"].is_number()) {
                setIntegerParam(ADTimePixTdc1Rate, measurement_j["Info"]["Tdc1EventRate"].get<int>());
            }
            if (measurement_j["Info"].contains("Tdc2EventRate") && measurement_j["Info"]["Tdc2EventRate"].is_number()) {
                setIntegerParam(ADTimePixTdc2Rate, measurement_j["Info"]["Tdc2EventRate"].get<int>());
            }
        } else if ((API_Ver[0] == '3') && ((API_Ver[2] - '0') <= 1)) {   // Serval 3.1.0 and 3.0.0
            if (measurement_j["Info"].contains("TdcEventRate") && measurement_j["Info"]["TdcEventRate"].is_number()) {
                setIntegerParam(ADTimePixTdc1Rate, measurement_j["Info"]["TdcEventRate"].get<int>());
            }
        } else {
            printf ("Serval Version not compared, event rate not read in acquireStop\n");
        }

        if (measurement_j["Info"].contains("StartDateTime") && measurement_j["Info"]["StartDateTime"].is_number()) {
            setInteger64Param(ADTimePixStartTime, measurement_j["Info"]["StartDateTime"].get<long>());
        }
        if (measurement_j["Info"].contains("ElapsedTime") && measurement_j["Info"]["ElapsedTime"].is_number()) {
            setDoubleParam(ADTimePixElapsedTime, measurement_j["Info"]["ElapsedTime"].get<double>());
        }
        if (measurement_j["Info"].contains("TimeLeft") && measurement_j["Info"]["TimeLeft"].is_number()) {
            setDoubleParam(ADTimePixTimeLeft, measurement_j["Info"]["TimeLeft"].get<double>());
        }
        if (measurement_j["Info"].contains("FrameCount") && measurement_j["Info"]["FrameCount"].is_number()) {
            setIntegerParam(ADTimePixFrameCount, measurement_j["Info"]["FrameCount"].get<int>());
        }
        if (measurement_j["Info"].contains("DroppedFrames") && measurement_j["Info"]["DroppedFrames"].is_number()) {
            setIntegerParam(ADTimePixDroppedFrames, measurement_j["Info"]["DroppedFrames"].get<int>());
        }
        if (measurement_j["Info"].contains("Status")) {
            // Status might be null, so use dump() which handles null safely
            setStringParam(ADTimePixStatus, measurement_j["Info"]["Status"].dump().c_str());
        }
    }
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
    } else if (function == ADTimePixImg1Base) {
        status = this->checkImg1Path();      
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
        int currentADStatus;
        getIntegerParam(ADStatus, &currentADStatus);
        printf("ACQUIRE CHANGE: ADAcquire=%d (was %d), current ADStatus=%d\n", value, acquiring, currentADStatus);
        if(value && !acquiring){
            FLOW("Entering acquire start\n");
            status = acquireStart();  // Start acquisition
            if(status < 0) {
                return asynError;
            }
            // After acquireStart, ADStatus should be ADStatusAcquire (1)
            getIntegerParam(ADStatus, &currentADStatus);
            printf("After acquireStart: ADStatus=%d\n", currentADStatus);
        }
        if(!value && acquiring){
            FLOW("Entering acquire stop");
            acquireStop();  // Stop acquisition
            // After acquireStop, ADStatus should be ADStatusIdle (0)
            getIntegerParam(ADStatus, &currentADStatus);
            printf("After acquireStop: ADStatus=%d\n", currentADStatus);
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
        || function == ADTimePixWriteImg1 || function == ADTimePixWritePrvImg || function == ADTimePixWritePrvImg1 || function == ADTimePixWritePrvHst) {
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

    else if(function == ADTimePixBiasVolt || function == ADTimePixBiasEnable || function == ADTimePixTriggerIn || function == ADTimePixTriggerOut || function == ADTimePixLogLevel \
                || function == ADTimePixExternalReferenceClock || function == ADTimePixChainMode) {  // set and enable bias, log level
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

    else if(function == ADTimePixImgFramesToSum) {
        epicsMutexLock(imgMutex_);
        imgFramesToSum_ = value;
        if (imgFramesToSum_ < 1) imgFramesToSum_ = 1;
        if (imgFramesToSum_ > 100000) imgFramesToSum_ = 100000;
        setIntegerParam(ADTimePixImgFramesToSum, imgFramesToSum_);
        
        // Trim frame buffer if new limit is smaller
        while (imgFrameBuffer_.size() > static_cast<size_t>(imgFramesToSum_)) {
            imgFrameBuffer_.pop_front();
        }
        
        // Prepare to recalculate sum of N frames immediately if buffer has frames
        size_t sum_pixel_count = 0;
        bool should_recalc_sum = false;
        if (!imgFrameBuffer_.empty()) {
            sum_pixel_count = imgFrameBuffer_[0].get_pixel_count();
            size_t frame_width = imgFrameBuffer_[0].get_width();
            size_t frame_height = imgFrameBuffer_[0].get_height();
            
            if (imgSumArray64WorkBuffer_.size() < sum_pixel_count) {
                imgSumArray64WorkBuffer_.resize(sum_pixel_count);
                imgSumArray64Buffer_.resize(sum_pixel_count);
            }
            
            std::memset(imgSumArray64WorkBuffer_.data(), 0, sum_pixel_count * sizeof(uint64_t));
            
            for (const auto& frame : imgFrameBuffer_) {
                if (frame.get_width() == frame_width && 
                    frame.get_height() == frame_height) {
                    if (frame.get_pixel_format() == ImageData::PixelFormat::UINT16) {
                        const uint16_t* pixels = frame.get_pixels_16_ptr();
                        for (size_t i = 0; i < sum_pixel_count; ++i) {
                            imgSumArray64WorkBuffer_[i] += pixels[i];
                        }
                    } else {
                        const uint32_t* pixels = frame.get_pixels_32_ptr();
                        for (size_t i = 0; i < sum_pixel_count; ++i) {
                            imgSumArray64WorkBuffer_[i] += pixels[i];
                        }
                    }
                }
            }
            
            // Convert to epicsInt64
            for (size_t i = 0; i < sum_pixel_count; ++i) {
                imgSumArray64Buffer_[i] = static_cast<epicsInt64>(imgSumArray64WorkBuffer_[i]);
            }
            
            // Reset update counter to trigger immediate update on next frame
            imgFramesSinceLastSumUpdate_ = imgSumUpdateIntervalFrames_;
            should_recalc_sum = true;
        }
        
        // Recalculate memory usage immediately since buffer size may have changed
        imgMemoryUsage_ = calculateImgMemoryUsageMB();
        setDoubleParam(ADTimePixImgMemoryUsage, imgMemoryUsage_);
        epicsMutexUnlock(imgMutex_);
        
        // Trigger callbacks outside mutex
        callParamCallbacks(ADTimePixImgFramesToSum);
        callParamCallbacks(ADTimePixImgMemoryUsage);
        
        // Trigger sum callback if we recalculated it (outside mutex)
        if (should_recalc_sum && sum_pixel_count > 0) {
            // Copy buffer data while holding mutex
            std::vector<epicsInt64> temp_buffer(sum_pixel_count);
            epicsMutexLock(imgMutex_);
            for (size_t i = 0; i < sum_pixel_count; ++i) {
                temp_buffer[i] = imgSumArray64Buffer_[i];
            }
            epicsMutexUnlock(imgMutex_);
            
            // Trigger callback outside mutex
            doCallbacksInt64Array(temp_buffer.data(), sum_pixel_count,
                                  ADTimePixImgImageSumNFrames, 0);
        }
    }
    
    else if(function == ADTimePixImgSumUpdateIntervalFrames) {
        epicsMutexLock(imgMutex_);
        imgSumUpdateIntervalFrames_ = value;
        if (imgSumUpdateIntervalFrames_ < 1) imgSumUpdateIntervalFrames_ = 1;
        if (imgSumUpdateIntervalFrames_ > 10000) imgSumUpdateIntervalFrames_ = 10000;
        setIntegerParam(ADTimePixImgSumUpdateIntervalFrames, imgSumUpdateIntervalFrames_);
        epicsMutexUnlock(imgMutex_);
        callParamCallbacks(ADTimePixImgSumUpdateIntervalFrames);
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


asynStatus ADTimePix::readInt64Array(asynUser *pasynUser, epicsInt64 *value, size_t nElements, size_t *nIn) {
    int function = pasynUser->reason;
    
    if (function == ADTimePixImgImageData) {
        epicsMutexLock(imgMutex_);
        if (imgRunningSum_) {
            size_t pixel_count = imgRunningSum_->get_pixel_count();
            size_t elements_to_copy = std::min(nElements, pixel_count);
            const uint64_t* pixels = imgRunningSum_->get_pixels_64_ptr();
            for (size_t i = 0; i < elements_to_copy; ++i) {
                value[i] = static_cast<epicsInt64>(pixels[i]);
            }
            // Zero out remaining elements
            for (size_t i = elements_to_copy; i < nElements; ++i) {
                value[i] = 0;
            }
            *nIn = nElements;
        } else {
            // No running sum yet, return zeros
            for (size_t i = 0; i < nElements; ++i) {
                value[i] = 0;
            }
            *nIn = nElements;
        }
        epicsMutexUnlock(imgMutex_);
        return asynSuccess;
    } else if (function == ADTimePixImgImageSumNFrames) {
        epicsMutexLock(imgMutex_);
        if (!imgFrameBuffer_.empty()) {
            size_t pixel_count = imgFrameBuffer_[0].get_pixel_count();
            size_t elements_to_copy = std::min(nElements, pixel_count);
            
            // Calculate sum of frames in buffer
            if (imgSumArray64WorkBuffer_.size() < pixel_count) {
                imgSumArray64WorkBuffer_.resize(pixel_count);
            }
            std::memset(imgSumArray64WorkBuffer_.data(), 0, pixel_count * sizeof(uint64_t));
            
            size_t frame_width = imgFrameBuffer_[0].get_width();
            size_t frame_height = imgFrameBuffer_[0].get_height();
            
            for (const auto& frame : imgFrameBuffer_) {
                if (frame.get_width() == frame_width && 
                    frame.get_height() == frame_height) {
                    if (frame.get_pixel_format() == ImageData::PixelFormat::UINT16) {
                        const uint16_t* pixels = frame.get_pixels_16_ptr();
                        for (size_t i = 0; i < pixel_count; ++i) {
                            imgSumArray64WorkBuffer_[i] += pixels[i];
                        }
                    } else {
                        const uint32_t* pixels = frame.get_pixels_32_ptr();
                        for (size_t i = 0; i < pixel_count; ++i) {
                            imgSumArray64WorkBuffer_[i] += pixels[i];
                        }
                    }
                }
            }
            
            // Copy to output buffer
            for (size_t i = 0; i < elements_to_copy; ++i) {
                value[i] = static_cast<epicsInt64>(imgSumArray64WorkBuffer_[i]);
            }
            // Zero out remaining elements
            for (size_t i = elements_to_copy; i < nElements; ++i) {
                value[i] = 0;
            }
            *nIn = nElements;
        } else {
            // No frames in buffer, return zeros
            for (size_t i = 0; i < nElements; ++i) {
                value[i] = 0;
            }
            *nIn = nElements;
        }
        epicsMutexUnlock(imgMutex_);
        return asynSuccess;
    }
    
    return asynPortDriver::readInt64Array(pasynUser, value, nElements, nIn);
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
 * Function used for reporting ADTimePix device and library information to an external
 * log file. The function first prints all ADTimePix specific information to the file,
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


// readImage() method removed - GraphicsMagick HTTP method no longer supported
// Use TCP streaming (tcp:// format) for preview images instead
// GraphicsMagick implementation preserved in preserve/graphicsmagick-preview branch

//----------------------------------------------------------------------------
// TCP Streaming Implementation for PrvImg Channel
//----------------------------------------------------------------------------

void ADTimePix::prvImgWorkerThreadC(void *pPvt) {
    ADTimePix *pPvtClass = (ADTimePix *)pPvt;
    pPvtClass->prvImgWorkerThread();
}

void ADTimePix::prvImgWorkerThread() {
    static const char* functionName = "prvImgWorkerThread";
    constexpr double RECONNECT_DELAY_SEC = 1.0;
    
    if (!prvImgMutex_) {
        ERR("PrvImg worker thread: Mutex not initialized");
        return;
    }
    
    prvImgLineBuffer_.resize(MAX_BUFFER_SIZE);
    prvImgTotalRead_ = 0;
    
    while (prvImgRunning_) {
        epicsMutexLock(prvImgMutex_);
        bool should_connect = prvImgRunning_ && !prvImgConnected_;
        std::string host = prvImgHost_;
        int port = prvImgPort_;
        epicsMutexUnlock(prvImgMutex_);
        
        if (should_connect && !host.empty() && port > 0) {
            prvImgConnect();
        }
        
        if (!prvImgRunning_) {
            break;
        }
        
        epicsMutexLock(prvImgMutex_);
        bool connected = prvImgConnected_;
        epicsMutexUnlock(prvImgMutex_);
        
        if (connected && prvImgNetworkClient_) {
            try {
                epicsMutexLock(prvImgMutex_);
                ssize_t bytes_read = prvImgNetworkClient_->receive(
                    prvImgLineBuffer_.data() + prvImgTotalRead_,
                    MAX_BUFFER_SIZE - prvImgTotalRead_ - 1
                );
                epicsMutexUnlock(prvImgMutex_);
                
                if (bytes_read <= 0) {
                    if (bytes_read == 0) {
                        epicsMutexLock(prvImgMutex_);
                        prvImgConnected_ = false;
                        prvImgRunning_ = false;
                        epicsMutexUnlock(prvImgMutex_);
                        printf("PrvImg TCP connection closed by peer\n");
                        break;
                    } else {
                        epicsMutexLock(prvImgMutex_);
                        if (prvImgConnected_) {
                            prvImgConnected_ = false;
                            prvImgRunning_ = false;
                            LOG_ARGS("PrvImg TCP socket error: %s", strerror(errno));
                        }
                        epicsMutexUnlock(prvImgMutex_);
                        break;
                    }
                }
                
                epicsMutexLock(prvImgMutex_);
                prvImgTotalRead_ += bytes_read;
                prvImgLineBuffer_[prvImgTotalRead_] = '\0';
                
                // Look for newline to find complete JSON line
                char* newline_pos = static_cast<char*>(memchr(prvImgLineBuffer_.data(), '\n', prvImgTotalRead_));
                
                if (newline_pos) {
                    // Found a newline - check if there's valid JSON before it
                    char* json_start = nullptr;
                    
                    // Try to find {" pattern (most reliable indicator of JSON)
                    for (char* p = prvImgLineBuffer_.data(); p < newline_pos - 1; ++p) {
                        if (*p == '{' && p[1] == '"') {
                            json_start = p;
                            break;
                        }
                    }
                    
                    // If we didn't find {", try finding { followed by valid JSON structure
                    if (!json_start) {
                        for (char* p = prvImgLineBuffer_.data(); p < newline_pos - 2; ++p) {
                            if (*p == '{') {
                                bool looks_like_json = false;
                                size_t check_len = std::min(size_t(newline_pos - p - 1), size_t(100));
                                
                                int json_chars = 0;
                                for (size_t i = 1; i < check_len; ++i) {
                                    char c = p[i];
                                    if (c == '"' || c == ':' || c == ',' || c == '}' || c == '[' || c == ']') {
                                        looks_like_json = true;
                                        break;
                                    }
                                    if (std::isalnum(c) || c == ' ' || c == '_' || c == '-' || c == '.') {
                                        json_chars++;
                                    } else if (c < 32 && c != '\n' && c != '\r' && c != '\t') {
                                        break;
                                    }
                                }
                                
                                if (looks_like_json || json_chars > 5) {
                                    json_start = p;
                                    break;
                                }
                            }
                        }
                    }
                    
                    bool valid_json_start = (json_start != nullptr);
                    
                    if (valid_json_start) {
                        // Try to parse the JSON to verify it's valid
                        bool is_valid_json = false;
                        try {
                            std::string json_str(json_start, newline_pos - json_start);
                            json test_json = json::parse(json_str);
                            if (test_json.contains("width") || test_json.contains("frameNumber") ||
                                test_json.contains("height") || test_json.contains("timeAtFrame")) {
                                is_valid_json = true;
                            }
                        } catch (...) {
                            is_valid_json = false;
                        }
                        
                        if (is_valid_json) {
                            *newline_pos = '\0';
                            
                            // Process the JSON line
                            if (!processPrvImgDataLine(json_start, newline_pos, prvImgTotalRead_)) {
                                epicsMutexUnlock(prvImgMutex_);
                                break;
                            }
                            
                            // Move remaining data to start of buffer
                            size_t remaining = prvImgTotalRead_ - (newline_pos - prvImgLineBuffer_.data() + 1);
                            if (remaining > 0) {
                                memmove(prvImgLineBuffer_.data(), newline_pos + 1, remaining);
                            }
                            prvImgTotalRead_ = remaining;
                        } else {
                            // Found { but it's not valid JSON - skip this newline
                            size_t remaining = prvImgTotalRead_ - (newline_pos - prvImgLineBuffer_.data() + 1);
                            if (remaining > 0) {
                                memmove(prvImgLineBuffer_.data(), newline_pos + 1, remaining);
                            }
                            prvImgTotalRead_ = remaining;
                        }
                    } else {
                        // Found newline but no valid JSON - might be binary data
                        size_t remaining = prvImgTotalRead_ - (newline_pos - prvImgLineBuffer_.data() + 1);
                        if (remaining > 0) {
                            memmove(prvImgLineBuffer_.data(), newline_pos + 1, remaining);
                        }
                        prvImgTotalRead_ = remaining;
                    }
                } else {
                    // No newline found yet - check if buffer is getting too full
                    if (prvImgTotalRead_ >= MAX_BUFFER_SIZE - 1) {
                        LOG("PrvImg TCP buffer full without finding newline, resetting");
                        prvImgTotalRead_ = 0;
                    }
                }
                
                if (prvImgTotalRead_ >= MAX_BUFFER_SIZE - 1) {
                    LOG("PrvImg TCP buffer full, resetting");
                    prvImgTotalRead_ = 0;
                }
                
                epicsMutexUnlock(prvImgMutex_);
                
            } catch (const std::exception& e) {
                epicsMutexUnlock(prvImgMutex_);
                ERR_ARGS("Error in PrvImg worker thread: %s", e.what());
            }
        } else {
            epicsThreadSleep(RECONNECT_DELAY_SEC);
        }
    }
    
    prvImgDisconnect();
    LOG("PrvImg worker thread exiting");
}

void ADTimePix::imgWorkerThreadC(void *pPvt) {
    ADTimePix *pPvtClass = (ADTimePix *)pPvt;
    pPvtClass->imgWorkerThread();
}

void ADTimePix::imgWorkerThread() {
    static const char* functionName = "imgWorkerThread";
    constexpr double RECONNECT_DELAY_SEC = 1.0;
    
    if (!imgMutex_) {
        ERR("Img worker thread: Mutex not initialized");
        return;
    }
    
    imgLineBuffer_.resize(MAX_BUFFER_SIZE);
    imgTotalRead_ = 0;
    
    while (imgRunning_) {
        epicsMutexLock(imgMutex_);
        bool should_connect = imgRunning_ && !imgConnected_;
        std::string host = imgHost_;
        int port = imgPort_;
        epicsMutexUnlock(imgMutex_);
        
        if (should_connect && !host.empty() && port > 0) {
            imgConnect();
        }
        
        if (!imgRunning_) {
            break;
        }
        
        epicsMutexLock(imgMutex_);
        bool connected = imgConnected_;
        epicsMutexUnlock(imgMutex_);
        
        if (connected && imgNetworkClient_) {
            try {
                epicsMutexLock(imgMutex_);
                ssize_t bytes_read = imgNetworkClient_->receive(
                    imgLineBuffer_.data() + imgTotalRead_,
                    MAX_BUFFER_SIZE - imgTotalRead_ - 1
                );
                epicsMutexUnlock(imgMutex_);
                
                if (bytes_read <= 0) {
                    if (bytes_read == 0) {
                        epicsMutexLock(imgMutex_);
                        imgConnected_ = false;
                        imgRunning_ = false;
                        epicsMutexUnlock(imgMutex_);
                        printf("Img TCP connection closed by peer\n");
                        break;
                    } else {
                        epicsMutexLock(imgMutex_);
                        if (imgConnected_) {
                            imgConnected_ = false;
                            imgRunning_ = false;
                            LOG_ARGS("Img TCP socket error: %s", strerror(errno));
                        }
                        epicsMutexUnlock(imgMutex_);
                        break;
                    }
                }
                
                epicsMutexLock(imgMutex_);
                imgTotalRead_ += bytes_read;
                imgLineBuffer_[imgTotalRead_] = '\0';
                
                // Look for newline to find complete JSON line
                char* newline_pos = static_cast<char*>(memchr(imgLineBuffer_.data(), '\n', imgTotalRead_));
                
                if (newline_pos) {
                    // Found a newline - check if there's valid JSON before it
                    char* json_start = nullptr;
                    
                    // Try to find {" pattern (most reliable indicator of JSON)
                    for (char* p = imgLineBuffer_.data(); p < newline_pos - 1; ++p) {
                        if (*p == '{' && p[1] == '"') {
                            json_start = p;
                            break;
                        }
                    }
                    
                    // If we didn't find {", try finding { followed by valid JSON structure
                    if (!json_start) {
                        for (char* p = imgLineBuffer_.data(); p < newline_pos - 2; ++p) {
                            if (*p == '{') {
                                bool looks_like_json = false;
                                size_t check_len = std::min(size_t(newline_pos - p - 1), size_t(100));
                                
                                int json_chars = 0;
                                for (size_t i = 1; i < check_len; ++i) {
                                    char c = p[i];
                                    if (c == '"' || c == ':' || c == ',' || c == '}' || c == '[' || c == ']') {
                                        looks_like_json = true;
                                        break;
                                    }
                                    if (std::isalnum(c) || c == ' ' || c == '_' || c == '-' || c == '.') {
                                        json_chars++;
                                    } else if (c < 32 && c != '\n' && c != '\r' && c != '\t') {
                                        break;
                                    }
                                }
                                
                                if (looks_like_json || json_chars > 5) {
                                    json_start = p;
                                    break;
                                }
                            }
                        }
                    }
                    
                    bool valid_json_start = (json_start != nullptr);
                    
                    if (valid_json_start) {
                        // Try to parse the JSON to verify it's valid
                        bool is_valid_json = false;
                        try {
                            std::string json_str(json_start, newline_pos - json_start);
                            json test_json = json::parse(json_str);
                            if (test_json.contains("width") || test_json.contains("frameNumber") ||
                                test_json.contains("height") || test_json.contains("timeAtFrame")) {
                                is_valid_json = true;
                            }
                        } catch (...) {
                            is_valid_json = false;
                        }
                        
                        if (is_valid_json) {
                            *newline_pos = '\0';
                            
                            // Process the JSON line
                            if (!processImgDataLine(json_start, newline_pos, imgTotalRead_)) {
                                epicsMutexUnlock(imgMutex_);
                                break;
                            }
                            
                            // Move remaining data to start of buffer
                            size_t remaining = imgTotalRead_ - (newline_pos - imgLineBuffer_.data() + 1);
                            if (remaining > 0) {
                                memmove(imgLineBuffer_.data(), newline_pos + 1, remaining);
                            }
                            imgTotalRead_ = remaining;
                        } else {
                            // Found { but it's not valid JSON - skip this newline
                            size_t remaining = imgTotalRead_ - (newline_pos - imgLineBuffer_.data() + 1);
                            if (remaining > 0) {
                                memmove(imgLineBuffer_.data(), newline_pos + 1, remaining);
                            }
                            imgTotalRead_ = remaining;
                        }
                    } else {
                        // Found newline but no valid JSON - might be binary data
                        size_t remaining = imgTotalRead_ - (newline_pos - imgLineBuffer_.data() + 1);
                        if (remaining > 0) {
                            memmove(imgLineBuffer_.data(), newline_pos + 1, remaining);
                        }
                        imgTotalRead_ = remaining;
                    }
                } else {
                    // No newline found yet - check if buffer is getting too full
                    if (imgTotalRead_ >= MAX_BUFFER_SIZE - 1) {
                        LOG("Img TCP buffer full without finding newline, resetting");
                        imgTotalRead_ = 0;
                    }
                }
                
                if (imgTotalRead_ >= MAX_BUFFER_SIZE - 1) {
                    LOG("Img TCP buffer full, resetting");
                    imgTotalRead_ = 0;
                }
                
                epicsMutexUnlock(imgMutex_);
                
            } catch (const std::exception& e) {
                epicsMutexUnlock(imgMutex_);
                ERR_ARGS("Error in Img worker thread: %s", e.what());
            }
        } else {
            epicsThreadSleep(RECONNECT_DELAY_SEC);
        }
    }
    
    imgDisconnect();
    LOG("Img worker thread exiting");
}

bool ADTimePix::processImgDataLine(char* line_buffer, char* newline_pos, size_t total_read) {
    const char* functionName = "processImgDataLine";
    
    // Skip any leading whitespace or binary data
    char* json_start = line_buffer;
    
    // Skip non-printable characters until we find '{'
    while (*json_start != '\0' && *json_start != '{' &&
           (*json_start < 32 || *json_start > 126)) {
        json_start++;
    }
    
    if (*json_start == '\0' || *json_start != '{') {
        return true;
    }
    
    json j;
    try {
        j = json::parse(json_start);
    } catch (const json::parse_error& e) {
        if (*json_start == '{') {
            ERR_ARGS("JSON parse error: %s", e.what());
        }
        return true;
    }
    
    try {
        // Extract header information for jsonimage
        int width = j["width"];
        int height = j["height"];
        std::string pixel_format_str = j.value("pixelFormat", "uint16");
        
        // Extract additional frame data
        int frame_number = j.value("frameNumber", 0);
        double time_at_frame = j.value("timeAtFrame", 0.0);
        
        // Determine pixel format
        bool is_uint32 = (pixel_format_str == "uint32" || pixel_format_str == "UINT32");
        NDDataType_t dataType = is_uint32 ? NDUInt32 : NDUInt16;
        
        // Calculate pixel data size
        size_t pixel_count = width * height;
        size_t bytes_per_pixel = is_uint32 ? sizeof(uint32_t) : sizeof(uint16_t);
        size_t binary_needed = pixel_count * bytes_per_pixel;
        
        // Validate dimensions
        if (width <= 0 || height <= 0 || width > 100000 || height > 100000) {
            ERR_ARGS("Invalid image dimensions: width=%d, height=%d", width, height);
            return false;
        }
        
        // Create NDArray - check if pool is available
        if (!this->pNDArrayPool) {
            ERR("NDArray pool is not available");
            return false;
        }
        
        size_t dims[3];
        dims[0] = width;
        dims[1] = height;
        dims[2] = 0;
        
        NDArray *pImage = nullptr;
        // Use pArrays[1] for Img channel to avoid conflict with PrvImg (pArrays[0])
        if (this->pArrays && this->pArrays[1]) {
            pImage = this->pArrays[1];
            pImage->release();
        }
        
        this->pArrays[1] = this->pNDArrayPool->alloc(2, dims, dataType, 0, NULL);
        pImage = this->pArrays[1];
        
        if (!pImage || !pImage->pData) {
            ERR("Failed to allocate NDArray or NDArray has no data pointer");
            return false;
        }
        
        // Copy any binary data we already have after the newline
        size_t remaining = total_read - (newline_pos - line_buffer + 1);
        size_t binary_read = 0;
        
        std::vector<char> pixel_buffer(binary_needed);
        
        if (remaining > 0) {
            size_t to_copy = std::min(remaining, binary_needed);
            memcpy(pixel_buffer.data(), newline_pos + 1, to_copy);
            binary_read = to_copy;
        }
        
        // Read any remaining binary data needed
        epicsMutexLock(imgMutex_);
        if (binary_read < binary_needed && imgNetworkClient_ && imgNetworkClient_->is_connected()) {
            if (!imgNetworkClient_->receive_exact(
                pixel_buffer.data() + binary_read,
                binary_needed - binary_read)) {
                epicsMutexUnlock(imgMutex_);
                ERR("Failed to read binary pixel data");
                return false;
            }
        }
        epicsMutexUnlock(imgMutex_);
        
        // Validate pixel buffer size
        if (pixel_buffer.size() < binary_needed) {
            ERR_ARGS("Pixel buffer too small: have %zu, need %zu", pixel_buffer.size(), binary_needed);
            return false;
        }
        
        // Convert network byte order to host byte order and copy to NDArray
        if (!pImage->pData) {
            ERR("NDArray pData is null");
            return false;
        }
        
        if (is_uint32) {
            uint32_t* pixels = reinterpret_cast<uint32_t*>(pixel_buffer.data());
            uint32_t* pData = reinterpret_cast<uint32_t*>(pImage->pData);
            if (!pixels || !pData) {
                ERR("Invalid pixel data pointers");
                return false;
            }
            for (size_t i = 0; i < pixel_count; ++i) {
                pData[i] = __builtin_bswap32(pixels[i]);
            }
        } else {
            uint16_t* pixels = reinterpret_cast<uint16_t*>(pixel_buffer.data());
            uint16_t* pData = reinterpret_cast<uint16_t*>(pImage->pData);
            if (!pixels || !pData) {
                ERR("Invalid pixel data pointers");
                return false;
            }
            for (size_t i = 0; i < pixel_count; ++i) {
                pData[i] = __builtin_bswap16(pixels[i]);
            }
        }
        
        // Set image parameters (thread-safe via asynPortDriver)
        setIntegerParam(ADSizeX, width);
        setIntegerParam(NDArraySizeX, width);
        setIntegerParam(ADSizeY, height);
        setIntegerParam(NDArraySizeY, height);
        
        // Set data type
        int dataTypeValue = (int)dataType;
        setIntegerParam(NDDataType, dataTypeValue);
        setIntegerParam(NDColorMode, NDColorModeMono);
        
        NDArrayInfo_t arrayInfo;
        pImage->getInfo(&arrayInfo);
        setIntegerParam(NDArraySize, (int)arrayInfo.totalBytes);
        
        // Increment array counter (thread-safe)
        int imagesAcquired = 0;
        getIntegerParam(NDArrayCounter, &imagesAcquired);
        imagesAcquired++;
        setIntegerParam(NDArrayCounter, imagesAcquired);
        
        // Set timestamp
        pImage->uniqueId = frame_number;
        epicsTimeStamp timestamp;
        epicsTimeGetCurrent(&timestamp);
        pImage->timeStamp = timestamp.secPastEpoch + timestamp.nsec / 1.e9;
        updateTimeStamp(&pImage->epicsTS);
        
        // Set Img metadata PVs
        setIntegerParam(ADTimePixImgFrameNumber, frame_number);
        setDoubleParam(ADTimePixImgTimeAtFrame, time_at_frame);
        
        // Calculate acquisition rate
        epicsTimeStamp current_time;
        epicsTimeGetCurrent(&current_time);
        double current_time_seconds = current_time.secPastEpoch + current_time.nsec / 1e9;
        
        if (!imgFirstFrameReceived_) {
            imgPreviousFrameNumber_ = frame_number;
            imgPreviousTimeAtFrame_ = current_time_seconds;
            imgFirstFrameReceived_ = true;
            imgAcquisitionRate_ = 0.0;
        } else {
            int frame_diff = frame_number - imgPreviousFrameNumber_;
            double time_diff_seconds = current_time_seconds - imgPreviousTimeAtFrame_;
            
            if (frame_diff > 1) {
                LOG_ARGS("Img frame loss detected! Expected frame %d, got frame %d (lost %d frames)", 
                         imgPreviousFrameNumber_ + 1, frame_number, frame_diff - 1);
            }
            
            if (frame_diff > 0 && time_diff_seconds > 0.0) {
                double current_rate = frame_diff / time_diff_seconds;
                
                imgRateSamples_.push_back(current_rate);
                if (imgRateSamples_.size() > IMG_MAX_RATE_SAMPLES) {
                    imgRateSamples_.erase(imgRateSamples_.begin());
                }
                
                double sum = 0.0;
                for (size_t i = 0; i < imgRateSamples_.size(); ++i) {
                    sum += imgRateSamples_[i];
                }
                imgAcquisitionRate_ = sum / imgRateSamples_.size();
                
                if (current_time_seconds - imgLastRateUpdateTime_ >= 1.0) {
                    setDoubleParam(ADTimePixImgAcqRate, imgAcquisitionRate_);
                    imgLastRateUpdateTime_ = current_time_seconds;
                }
            }
            
            imgPreviousFrameNumber_ = frame_number;
            imgPreviousTimeAtFrame_ = current_time_seconds;
        }
        
        // Get attributes
        if (pImage->pAttributeList) {
            this->getAttributes(pImage->pAttributeList);
        }
        
        // NEW: Create ImageData from frame for accumulation
        ImageData::PixelFormat imgDataFormat = is_uint32 ? ImageData::PixelFormat::UINT32 : ImageData::PixelFormat::UINT16;
        ImageData frame_image(width, height, imgDataFormat, ImageData::DataType::FRAME_DATA);
        
        // Copy pixel data from NDArray to ImageData
        if (is_uint32) {
            uint32_t* pData = reinterpret_cast<uint32_t*>(pImage->pData);
            for (size_t y = 0; y < static_cast<size_t>(height); ++y) {
                for (size_t x = 0; x < static_cast<size_t>(width); ++x) {
                    size_t idx = y * width + x;
                    frame_image.set_pixel_32(x, y, pData[idx]);
                }
            }
        } else {
            uint16_t* pData = reinterpret_cast<uint16_t*>(pImage->pData);
            for (size_t y = 0; y < static_cast<size_t>(height); ++y) {
                for (size_t x = 0; x < static_cast<size_t>(width); ++x) {
                    size_t idx = y * width + x;
                    frame_image.set_pixel_16(x, y, pData[idx]);
                }
            }
        }
        
        // NEW: Process frame for accumulation (only if enabled)
        int accumulationEnable = 0;
        getIntegerParam(ADTimePixImgAccumulationEnable, &accumulationEnable);
        if (accumulationEnable) {
            processImgFrame(frame_image);
        }
        
        // Call parameter callbacks to update EPICS PVs (thread-safe)
        callParamCallbacks();
        
        // Trigger NDArray callbacks (thread-safe)
        int arrayCallbacks = 0;
        getIntegerParam(NDArrayCallbacks, &arrayCallbacks);
        if (arrayCallbacks && pImage) {
            doCallbacksGenericPointer(pImage, NDArrayData, 0);
        }
        
        LOG_ARGS("Processed Img frame: width=%d, height=%d, format=%s, frame=%d, counter=%d", 
                 width, height, pixel_format_str.c_str(), frame_number, imagesAcquired);
        
    } catch (const std::exception& e) {
        ERR_ARGS("Error processing Img frame: %s", e.what());
        return false;
    }
    
    return true;
}

void ADTimePix::processImgFrame(const ImageData& frame_data) {
    const char* functionName = "processImgFrame";
    epicsTimeStamp processing_start_time;
    epicsTimeGetCurrent(&processing_start_time);
    
    epicsMutexLock(imgMutex_);
    
    // Initialize running sum if needed
    if (!imgRunningSum_) {
        imgRunningSum_.reset(new ImageData(
            frame_data.get_width(), 
            frame_data.get_height(),
            frame_data.get_pixel_format(),
            ImageData::DataType::RUNNING_SUM
        ));
    }
    
    // Check for dimension mismatch
    if (imgRunningSum_->get_width() != frame_data.get_width() || 
        imgRunningSum_->get_height() != frame_data.get_height()) {
        WARN_ARGS("Img image size mismatch! Running sum has %zux%zu, frame has %zux%zu. Reinitializing running sum.",
                  imgRunningSum_->get_width(), imgRunningSum_->get_height(),
                  frame_data.get_width(), frame_data.get_height());
        
        imgRunningSum_.reset(new ImageData(
            frame_data.get_width(), 
            frame_data.get_height(),
            frame_data.get_pixel_format(),
            ImageData::DataType::RUNNING_SUM
        ));
        
        // Reinitialize current frame with new dimensions
        imgCurrentFrame_ = frame_data;
        
        imgTotalCounts_ = 0;
        setInteger64Param(ADTimePixImgTotalCounts, 0);
    }
    
    // Add frame to running sum
    try {
        imgRunningSum_->add_image(frame_data);
    } catch (const std::exception& e) {
        ERR_ARGS("Failed to add image to running sum: %s", e.what());
        epicsMutexUnlock(imgMutex_);
        return;
    }
    
    // Store current frame for IMAGE_FRAME PV
    imgCurrentFrame_ = frame_data;
    
    // Calculate total counts for this frame
    size_t pixel_count = frame_data.get_pixel_count();
    uint64_t frame_total = 0;
    
    if (frame_data.get_pixel_format() == ImageData::PixelFormat::UINT16) {
        const uint16_t* frame_pixels = frame_data.get_pixels_16_ptr();
        for (size_t i = 0; i < pixel_count; ++i) {
            frame_total += frame_pixels[i];
        }
    } else {
        const uint32_t* frame_pixels = frame_data.get_pixels_32_ptr();
        for (size_t i = 0; i < pixel_count; ++i) {
            frame_total += frame_pixels[i];
        }
    }
    imgTotalCounts_ += frame_total;
    
    // Add to frame buffer (must be done BEFORE checking update condition)
    imgFrameBuffer_.push_back(frame_data);
    while (imgFrameBuffer_.size() > static_cast<size_t>(imgFramesToSum_)) {
        imgFrameBuffer_.pop_front();
    }
    
    // Increment frame counter for sum update interval
    imgFramesSinceLastSumUpdate_++;
    
    // Prepare data for callbacks (while holding mutex)
    // Copy data to buffers that will be used for callbacks
    size_t image_data_size = 0;
    size_t image_frame_size = 0;
    size_t image_sum_size = 0;
    bool has_running_sum = (imgRunningSum_ != nullptr);
    bool has_current_frame = (imgCurrentFrame_.get_pixel_count() > 0);
    bool should_update_sum = (imgFramesSinceLastSumUpdate_ >= imgSumUpdateIntervalFrames_ && !imgFrameBuffer_.empty());
    
    if (has_running_sum) {
        image_data_size = imgRunningSum_->get_pixel_count();
        if (imgArrayData64Buffer_.size() < image_data_size) {
            imgArrayData64Buffer_.resize(image_data_size);
        }
        const uint64_t* pixels = imgRunningSum_->get_pixels_64_ptr();
        for (size_t i = 0; i < image_data_size; ++i) {
            imgArrayData64Buffer_[i] = static_cast<epicsInt64>(pixels[i]);
        }
    }
    
    if (has_current_frame) {
        image_frame_size = imgCurrentFrame_.get_pixel_count();
        if (imgFrameArrayDataBuffer_.size() < image_frame_size) {
            imgFrameArrayDataBuffer_.resize(image_frame_size);
        }
        if (imgCurrentFrame_.get_pixel_format() == ImageData::PixelFormat::UINT16) {
            const uint16_t* pixels = imgCurrentFrame_.get_pixels_16_ptr();
            for (size_t i = 0; i < image_frame_size; ++i) {
                imgFrameArrayDataBuffer_[i] = static_cast<epicsInt32>(pixels[i]);
            }
        } else {
            const uint32_t* pixels = imgCurrentFrame_.get_pixels_32_ptr();
            for (size_t i = 0; i < image_frame_size; ++i) {
                imgFrameArrayDataBuffer_[i] = static_cast<epicsInt32>(pixels[i]);
            }
        }
    }
    
    if (should_update_sum) {
        imgFramesSinceLastSumUpdate_ = 0;
        
        // Use dimensions from first frame in buffer (should match current frame)
        size_t sum_pixel_count = imgFrameBuffer_[0].get_pixel_count();
        size_t sum_frame_width = imgFrameBuffer_[0].get_width();
        size_t sum_frame_height = imgFrameBuffer_[0].get_height();
        
        if (imgSumArray64WorkBuffer_.size() < sum_pixel_count) {
            imgSumArray64WorkBuffer_.resize(sum_pixel_count);
            imgSumArray64Buffer_.resize(sum_pixel_count);
        }
        
        // Initialize sum array to zero
        std::memset(imgSumArray64WorkBuffer_.data(), 0, sum_pixel_count * sizeof(uint64_t));
        
        // Sum all frames in buffer
        size_t frames_summed = 0;
        for (const auto& frame : imgFrameBuffer_) {
            if (frame.get_width() == sum_frame_width && 
                frame.get_height() == sum_frame_height) {
                frames_summed++;
                if (frame.get_pixel_format() == ImageData::PixelFormat::UINT16) {
                    const uint16_t* pixels = frame.get_pixels_16_ptr();
                    for (size_t i = 0; i < sum_pixel_count; ++i) {
                        imgSumArray64WorkBuffer_[i] += pixels[i];
                    }
                } else {
                    const uint32_t* pixels = frame.get_pixels_32_ptr();
                    for (size_t i = 0; i < sum_pixel_count; ++i) {
                        imgSumArray64WorkBuffer_[i] += pixels[i];
                    }
                }
            }
        }
        
        // Convert to epicsInt64
        for (size_t i = 0; i < sum_pixel_count; ++i) {
            imgSumArray64Buffer_[i] = static_cast<epicsInt64>(imgSumArray64WorkBuffer_[i]);
        }
        image_sum_size = sum_pixel_count;
    }
    
    // Calculate processing time
    epicsTimeStamp processing_end_time;
    epicsTimeGetCurrent(&processing_end_time);
    double processing_time_ms = ((processing_end_time.secPastEpoch - processing_start_time.secPastEpoch) * 1000.0) +
                                 ((processing_end_time.nsec - processing_start_time.nsec) / 1e6);
    
    imgProcessingTimeSamples_.push_back(processing_time_ms);
    if (imgProcessingTimeSamples_.size() > IMG_MAX_PROCESSING_TIME_SAMPLES) {
        imgProcessingTimeSamples_.erase(imgProcessingTimeSamples_.begin());
    }
    
    // Calculate average processing time from samples
    if (imgProcessingTimeSamples_.size() > 0) {
        double sum = 0.0;
        for (size_t i = 0; i < imgProcessingTimeSamples_.size(); ++i) {
            sum += imgProcessingTimeSamples_[i];
        }
        imgProcessingTime_ = sum / imgProcessingTimeSamples_.size();
    } else {
        imgProcessingTime_ = 0.0;
    }
    
    // Update EPICS PV periodically (every 1 second) or when we have enough samples
    double current_time_seconds = processing_end_time.secPastEpoch + processing_end_time.nsec / 1e9;
    if (current_time_seconds - imgLastProcessingTimeUpdate_ >= 1.0 || 
        imgProcessingTimeSamples_.size() >= IMG_MAX_PROCESSING_TIME_SAMPLES) {
        setDoubleParam(ADTimePixImgProcessingTime, imgProcessingTime_);
        callParamCallbacks(ADTimePixImgProcessingTime);
        imgLastProcessingTimeUpdate_ = current_time_seconds;
    }
    
    // Update performance metrics
    updateImgPerformanceMetrics();
    
    epicsMutexUnlock(imgMutex_);
    
    // Trigger callbacks OUTSIDE mutex to avoid deadlocks
    if (has_running_sum && image_data_size > 0) {
        doCallbacksInt64Array(imgArrayData64Buffer_.data(), image_data_size, 
                              ADTimePixImgImageData, 0);
    }
    
    if (has_current_frame && image_frame_size > 0) {
        doCallbacksInt32Array(imgFrameArrayDataBuffer_.data(), image_frame_size,
                              ADTimePixImgImageFrame, 0);
    }
    
    if (should_update_sum && image_sum_size > 0) {
        doCallbacksInt64Array(imgSumArray64Buffer_.data(), image_sum_size,
                              ADTimePixImgImageSumNFrames, 0);
    }
}

void ADTimePix::updateImgDisplayData() {
    const char* functionName = "updateImgDisplayData";
    
    // Update IMAGE_DATA (running sum)
    if (imgRunningSum_) {
        size_t pixel_count = imgRunningSum_->get_pixel_count();
        // Resize buffer if needed
        if (imgArrayData64Buffer_.size() < pixel_count) {
            imgArrayData64Buffer_.resize(pixel_count);
        }
        // Copy running sum to buffer
        const uint64_t* pixels = imgRunningSum_->get_pixels_64_ptr();
        for (size_t i = 0; i < pixel_count; ++i) {
            imgArrayData64Buffer_[i] = static_cast<epicsInt64>(pixels[i]);
        }
        // Trigger callback - must be done while holding mutex to ensure data consistency
        asynStatus status = doCallbacksInt64Array(imgArrayData64Buffer_.data(), pixel_count, 
                                                   ADTimePixImgImageData, 0);
        if (status != asynSuccess) {
            ERR_ARGS("Failed to trigger callback for IMAGE_DATA: status=%d", status);
        }
    } else {
        // No running sum yet - trigger callback with zeros to initialize the array
        size_t default_pixel_count = 512 * 512; // Default detector size
        if (imgArrayData64Buffer_.size() < default_pixel_count) {
            imgArrayData64Buffer_.resize(default_pixel_count, 0);
        }
        doCallbacksInt64Array(imgArrayData64Buffer_.data(), default_pixel_count,
                              ADTimePixImgImageData, 0);
    }
    
    // Update IMAGE_FRAME (current frame)
    size_t pixel_count = imgCurrentFrame_.get_pixel_count();
    if (pixel_count > 0) {
        if (imgFrameArrayDataBuffer_.size() < pixel_count) {
            imgFrameArrayDataBuffer_.resize(pixel_count);
        }
        // Copy current frame to buffer
        if (imgCurrentFrame_.get_pixel_format() == ImageData::PixelFormat::UINT16) {
            const uint16_t* pixels = imgCurrentFrame_.get_pixels_16_ptr();
            for (size_t i = 0; i < pixel_count; ++i) {
                imgFrameArrayDataBuffer_[i] = static_cast<epicsInt32>(pixels[i]);
            }
        } else {
            const uint32_t* pixels = imgCurrentFrame_.get_pixels_32_ptr();
            for (size_t i = 0; i < pixel_count; ++i) {
                imgFrameArrayDataBuffer_[i] = static_cast<epicsInt32>(pixels[i]);
            }
        }
        asynStatus status = doCallbacksInt32Array(imgFrameArrayDataBuffer_.data(), pixel_count,
                                                   ADTimePixImgImageFrame, 0);
        if (status != asynSuccess) {
            ERR_ARGS("Failed to trigger callback for IMAGE_FRAME: status=%d", status);
        }
    } else {
        // No current frame yet - trigger callback with zeros to initialize the array
        size_t default_pixel_count = 512 * 512; // Default detector size
        if (imgFrameArrayDataBuffer_.size() < default_pixel_count) {
            imgFrameArrayDataBuffer_.resize(default_pixel_count, 0);
        }
        doCallbacksInt32Array(imgFrameArrayDataBuffer_.data(), default_pixel_count,
                              ADTimePixImgImageFrame, 0);
    }
    
    // Update IMAGE_SUM_N_FRAMES (sum of last N frames)
    imgFramesSinceLastSumUpdate_++;
    if (imgFramesSinceLastSumUpdate_ >= imgSumUpdateIntervalFrames_ && !imgFrameBuffer_.empty()) {
        imgFramesSinceLastSumUpdate_ = 0;
        
        // Calculate sum of frames in buffer
        size_t pixel_count = imgFrameBuffer_[0].get_pixel_count();
        size_t frame_width = imgFrameBuffer_[0].get_width();
        size_t frame_height = imgFrameBuffer_[0].get_height();
        
        if (imgSumArray64WorkBuffer_.size() < pixel_count) {
            imgSumArray64WorkBuffer_.resize(pixel_count);
            imgSumArray64Buffer_.resize(pixel_count);
        }
        
        std::memset(imgSumArray64WorkBuffer_.data(), 0, pixel_count * sizeof(uint64_t));
        
        for (const auto& frame : imgFrameBuffer_) {
            if (frame.get_width() == frame_width && 
                frame.get_height() == frame_height) {
                if (frame.get_pixel_format() == ImageData::PixelFormat::UINT16) {
                    const uint16_t* pixels = frame.get_pixels_16_ptr();
                    for (size_t i = 0; i < pixel_count; ++i) {
                        imgSumArray64WorkBuffer_[i] += pixels[i];
                    }
                } else {
                    const uint32_t* pixels = frame.get_pixels_32_ptr();
                    for (size_t i = 0; i < pixel_count; ++i) {
                        imgSumArray64WorkBuffer_[i] += pixels[i];
                    }
                }
            }
        }
        
        // Convert to epicsInt64
        for (size_t i = 0; i < pixel_count; ++i) {
            imgSumArray64Buffer_[i] = static_cast<epicsInt64>(imgSumArray64WorkBuffer_[i]);
        }
        
        // Trigger callback
        asynStatus status = doCallbacksInt64Array(imgSumArray64Buffer_.data(), pixel_count,
                                                   ADTimePixImgImageSumNFrames, 0);
        if (status != asynSuccess) {
            ERR_ARGS("Failed to trigger callback for IMAGE_SUM_N_FRAMES: status=%d", status);
        }
    } else if (imgFrameBuffer_.empty()) {
        // No frames in buffer yet - trigger callback with zeros to initialize the array
        size_t default_pixel_count = 512 * 512; // Default detector size
        if (imgSumArray64Buffer_.size() < default_pixel_count) {
            imgSumArray64Buffer_.resize(default_pixel_count, 0);
        }
        doCallbacksInt64Array(imgSumArray64Buffer_.data(), default_pixel_count,
                              ADTimePixImgImageSumNFrames, 0);
    }
}

void ADTimePix::updateImgPerformanceMetrics() {
    epicsTimeStamp current_time;
    epicsTimeGetCurrent(&current_time);
    double current_time_seconds = current_time.secPastEpoch + current_time.nsec / 1e9;
    
    // Update total counts
    setInteger64Param(ADTimePixImgTotalCounts, imgTotalCounts_);
    
    // Calculate memory usage periodically (every 5 seconds) or more frequently if buffer is growing
    // Update more frequently if frame buffer is near capacity to catch memory growth
    bool should_update_memory = (current_time_seconds - imgLastMemoryUpdateTime_ >= IMG_MEMORY_UPDATE_INTERVAL_SEC) ||
                                 (imgFrameBuffer_.size() >= static_cast<size_t>(imgFramesToSum_) * 0.9);
    
    if (should_update_memory) {
        imgMemoryUsage_ = calculateImgMemoryUsageMB();
        setDoubleParam(ADTimePixImgMemoryUsage, imgMemoryUsage_);
        callParamCallbacks(ADTimePixImgMemoryUsage);
        imgLastMemoryUpdateTime_ = current_time_seconds;
    }
    
    // Call parameter callbacks for updated values
    callParamCallbacks(ADTimePixImgTotalCounts);
}

double ADTimePix::calculateImgMemoryUsageMB() {
    double total_mb = 0.0;
    
    // Memory for running sum (64-bit pixels)
    if (imgRunningSum_) {
        total_mb += imgRunningSum_->get_pixel_count() * sizeof(uint64_t) / (1024.0 * 1024.0);
    }
    
    // Memory for current frame
    size_t current_frame_pixels = imgCurrentFrame_.get_pixel_count();
    if (current_frame_pixels > 0) {
        if (imgCurrentFrame_.get_pixel_format() == ImageData::PixelFormat::UINT16) {
            total_mb += current_frame_pixels * sizeof(uint16_t) / (1024.0 * 1024.0);
        } else {
            total_mb += current_frame_pixels * sizeof(uint32_t) / (1024.0 * 1024.0);
        }
    }
    
    // Memory for frame buffer (actual frames stored, up to imgFramesToSum_)
    for (const auto& frame : imgFrameBuffer_) {
        if (frame.get_pixel_format() == ImageData::PixelFormat::UINT16) {
            total_mb += frame.get_pixel_count() * sizeof(uint16_t) / (1024.0 * 1024.0);
        } else {
            total_mb += frame.get_pixel_count() * sizeof(uint32_t) / (1024.0 * 1024.0);
        }
    }
    
    // Memory for EPICS array buffers (use maximum potential size based on imgFramesToSum_)
    // Calculate maximum pixels based on current frame dimensions or default
    size_t max_pixels = 0;
    if (imgRunningSum_) {
        max_pixels = imgRunningSum_->get_pixel_count();
    } else if (current_frame_pixels > 0) {
        max_pixels = current_frame_pixels;
    } else {
        max_pixels = 512 * 512; // Default detector size
    }
    
    // EPICS waveform buffers: IMAGE_DATA and IMAGE_SUM_N_FRAMES use 64-bit, IMAGE_FRAME uses 32-bit
    // Account for maximum potential allocation
    total_mb += max_pixels * sizeof(epicsInt64) / (1024.0 * 1024.0); // IMAGE_DATA (64-bit)
    total_mb += max_pixels * sizeof(epicsInt64) / (1024.0 * 1024.0); // IMAGE_SUM_N_FRAMES (64-bit)
    total_mb += max_pixels * sizeof(epicsInt32) / (1024.0 * 1024.0); // IMAGE_FRAME (32-bit)
    
    // Internal work buffers
    total_mb += (imgSumArray64WorkBuffer_.size() * sizeof(uint64_t)) / (1024.0 * 1024.0);
    
    // Add overhead for std::vector and std::deque structures (approximate)
    // Each ImageData object has some overhead, and std::deque has overhead per element
    size_t frame_buffer_overhead = imgFrameBuffer_.size() * 64; // Approximate overhead per frame in deque
    total_mb += frame_buffer_overhead / (1024.0 * 1024.0);
    
    // Add small overhead for other structures
    total_mb += 0.1; // Overhead for rate samples, processing time samples, etc.
    
    return total_mb;
}

void ADTimePix::resetImgAccumulation() {
    imgRunningSum_ = nullptr;
    imgFrameBuffer_.clear();
    imgTotalCounts_ = 0;
    imgFramesSinceLastSumUpdate_ = 0;
    imgProcessingTime_ = 0.0;
    imgProcessingTimeSamples_.clear();
    setInteger64Param(ADTimePixImgTotalCounts, 0);
    setDoubleParam(ADTimePixImgProcessingTime, 0.0);
    callParamCallbacks();
}

bool ADTimePix::processPrvImgDataLine(char* line_buffer, char* newline_pos, size_t total_read) {
    const char* functionName = "processPrvImgDataLine";
    
    // Skip any leading whitespace or binary data
    char* json_start = line_buffer;
    
    // Skip non-printable characters until we find '{'
    while (*json_start != '\0' && *json_start != '{' &&
           (*json_start < 32 || *json_start > 126)) {
        json_start++;
    }
    
    if (*json_start == '\0' || *json_start != '{') {
        return true;
    }
    
    json j;
    try {
        j = json::parse(json_start);
    } catch (const json::parse_error& e) {
        if (*json_start == '{') {
            ERR_ARGS("JSON parse error: %s", e.what());
        }
        return true;
    }
    
    try {
        // Extract header information for jsonimage
        int width = j["width"];
        int height = j["height"];
        std::string pixel_format_str = j.value("pixelFormat", "uint16");
        
        // Extract additional frame data
        int frame_number = j.value("frameNumber", 0);
        double time_at_frame = j.value("timeAtFrame", 0.0);
        
        // Determine pixel format
        bool is_uint32 = (pixel_format_str == "uint32" || pixel_format_str == "UINT32");
        NDDataType_t dataType = is_uint32 ? NDUInt32 : NDUInt16;
        
        // Calculate pixel data size
        size_t pixel_count = width * height;
        size_t bytes_per_pixel = is_uint32 ? sizeof(uint32_t) : sizeof(uint16_t);
        size_t binary_needed = pixel_count * bytes_per_pixel;
        
        // Validate dimensions
        if (width <= 0 || height <= 0 || width > 100000 || height > 100000) {
            ERR_ARGS("Invalid image dimensions: width=%d, height=%d", width, height);
            return false;
        }
        
        // Create NDArray - check if pool is available
        if (!this->pNDArrayPool) {
            ERR("NDArray pool is not available");
            return false;
        }
        
        size_t dims[3];
        dims[0] = width;
        dims[1] = height;
        dims[2] = 0;
        
        NDArray *pImage = nullptr;
        if (this->pArrays && this->pArrays[0]) {
            pImage = this->pArrays[0];
            pImage->release();
        }
        
        this->pArrays[0] = this->pNDArrayPool->alloc(2, dims, dataType, 0, NULL);
        pImage = this->pArrays[0];
        
        if (!pImage || !pImage->pData) {
            ERR("Failed to allocate NDArray or NDArray has no data pointer");
            return false;
        }
        
        // Copy any binary data we already have after the newline
        size_t remaining = total_read - (newline_pos - line_buffer + 1);
        size_t binary_read = 0;
        
        std::vector<char> pixel_buffer(binary_needed);
        
        if (remaining > 0) {
            size_t to_copy = std::min(remaining, binary_needed);
            memcpy(pixel_buffer.data(), newline_pos + 1, to_copy);
            binary_read = to_copy;
        }
        
        // Read any remaining binary data needed
        epicsMutexLock(prvImgMutex_);
        if (binary_read < binary_needed && prvImgNetworkClient_ && prvImgNetworkClient_->is_connected()) {
            if (!prvImgNetworkClient_->receive_exact(
                pixel_buffer.data() + binary_read,
                binary_needed - binary_read)) {
                epicsMutexUnlock(prvImgMutex_);
                ERR("Failed to read binary pixel data");
                return false;
            }
        }
        epicsMutexUnlock(prvImgMutex_);
        
        // Validate pixel buffer size
        if (pixel_buffer.size() < binary_needed) {
            ERR_ARGS("Pixel buffer too small: have %zu, need %zu", pixel_buffer.size(), binary_needed);
            return false;
        }
        
        // Convert network byte order to host byte order and copy to NDArray
        if (!pImage->pData) {
            ERR("NDArray pData is null");
            return false;
        }
        
        if (is_uint32) {
            uint32_t* pixels = reinterpret_cast<uint32_t*>(pixel_buffer.data());
            uint32_t* pData = reinterpret_cast<uint32_t*>(pImage->pData);
            if (!pixels || !pData) {
                ERR("Invalid pixel data pointers");
                return false;
            }
            for (size_t i = 0; i < pixel_count; ++i) {
                pData[i] = __builtin_bswap32(pixels[i]);
            }
        } else {
            uint16_t* pixels = reinterpret_cast<uint16_t*>(pixel_buffer.data());
            uint16_t* pData = reinterpret_cast<uint16_t*>(pImage->pData);
            if (!pixels || !pData) {
                ERR("Invalid pixel data pointers");
                return false;
            }
            for (size_t i = 0; i < pixel_count; ++i) {
                pData[i] = __builtin_bswap16(pixels[i]);
            }
        }
        
        // Set image parameters (thread-safe via asynPortDriver)
        setIntegerParam(ADSizeX, width);
        setIntegerParam(NDArraySizeX, width);
        setIntegerParam(ADSizeY, height);
        setIntegerParam(NDArraySizeY, height);
        
        // Set data type
        int dataTypeValue = (int)dataType;
        setIntegerParam(NDDataType, dataTypeValue);
        setIntegerParam(NDColorMode, NDColorModeMono);
        
        NDArrayInfo_t arrayInfo;
        pImage->getInfo(&arrayInfo);
        setIntegerParam(NDArraySize, (int)arrayInfo.totalBytes);
        
        // Increment array counter (thread-safe)
        int imagesAcquired = 0;
        getIntegerParam(NDArrayCounter, &imagesAcquired);
        imagesAcquired++;
        setIntegerParam(NDArrayCounter, imagesAcquired);
        
        // Set timestamp
        pImage->uniqueId = frame_number;
        epicsTimeStamp timestamp;
        epicsTimeGetCurrent(&timestamp);
        pImage->timeStamp = timestamp.secPastEpoch + timestamp.nsec / 1.e9;
        updateTimeStamp(&pImage->epicsTS);
        
        // Set PrvImg metadata PVs
        setIntegerParam(ADTimePixPrvImgFrameNumber, frame_number);
        setDoubleParam(ADTimePixPrvImgTimeAtFrame, time_at_frame);
        
        // Calculate acquisition rate
        epicsTimeStamp current_time;
        epicsTimeGetCurrent(&current_time);
        double current_time_seconds = current_time.secPastEpoch + current_time.nsec / 1e9;
        
        if (!prvImgFirstFrameReceived_) {
            prvImgPreviousFrameNumber_ = frame_number;
            prvImgPreviousTimeAtFrame_ = current_time_seconds;
            prvImgFirstFrameReceived_ = true;
            prvImgAcquisitionRate_ = 0.0;
        } else {
            int frame_diff = frame_number - prvImgPreviousFrameNumber_;
            double time_diff_seconds = current_time_seconds - prvImgPreviousTimeAtFrame_;
            
            if (frame_diff > 1) {
                LOG_ARGS("PrvImg frame loss detected! Expected frame %d, got frame %d (lost %d frames)", 
                         prvImgPreviousFrameNumber_ + 1, frame_number, frame_diff - 1);
            }
            
            if (frame_diff > 0 && time_diff_seconds > 0.0) {
                double current_rate = frame_diff / time_diff_seconds;
                
                prvImgRateSamples_.push_back(current_rate);
                if (prvImgRateSamples_.size() > PRVIMG_MAX_RATE_SAMPLES) {
                    prvImgRateSamples_.erase(prvImgRateSamples_.begin());
                }
                
                double sum = 0.0;
                for (size_t i = 0; i < prvImgRateSamples_.size(); ++i) {
                    sum += prvImgRateSamples_[i];
                }
                prvImgAcquisitionRate_ = sum / prvImgRateSamples_.size();
                
                if (current_time_seconds - prvImgLastRateUpdateTime_ >= 1.0) {
                    setDoubleParam(ADTimePixPrvImgAcqRate, prvImgAcquisitionRate_);
                    prvImgLastRateUpdateTime_ = current_time_seconds;
                }
            }
            
            prvImgPreviousFrameNumber_ = frame_number;
            prvImgPreviousTimeAtFrame_ = current_time_seconds;
        }
        
        // Get attributes
        if (pImage->pAttributeList) {
            this->getAttributes(pImage->pAttributeList);
        }
        
        // Call parameter callbacks to update EPICS PVs (thread-safe)
        callParamCallbacks();
        
        // Trigger NDArray callbacks (thread-safe)
        int arrayCallbacks = 0;
        getIntegerParam(NDArrayCallbacks, &arrayCallbacks);
        if (arrayCallbacks && pImage) {
            doCallbacksGenericPointer(pImage, NDArrayData, 0);
        }
        
        LOG_ARGS("Processed PrvImg frame: width=%d, height=%d, format=%s, frame=%d, counter=%d", 
                 width, height, pixel_format_str.c_str(), frame_number, imagesAcquired);
        
    } catch (const std::exception& e) {
        ERR_ARGS("Error processing PrvImg frame: %s", e.what());
        return false;
    }
    
    return true;
}

void ADTimePix::imgConnect() {
    static const char* functionName = "imgConnect";
    
    if (!imgMutex_) {
        ERR("Img TCP: Mutex not initialized");
        return;
    }
    
    epicsMutexLock(imgMutex_);
    std::string host = imgHost_;
    int port = imgPort_;
    epicsMutexUnlock(imgMutex_);
    
    if (host.empty() || port <= 0) {
        ERR("Img TCP: Invalid host or port");
        return;
    }
    
    imgDisconnect(); // Ensure clean state
    
    imgNetworkClient_.reset(new NetworkClient());
    if (!imgNetworkClient_) {
        ERR("Img TCP: Failed to create NetworkClient");
        return;
    }
    
    if (imgNetworkClient_->connect(host, port)) {
        epicsMutexLock(imgMutex_);
        imgConnected_ = true;
        epicsMutexUnlock(imgMutex_);
        LOG_ARGS("Img TCP connected to %s:%d", host.c_str(), port);
    } else {
        ERR_ARGS("Img TCP failed to connect to %s:%d", host.c_str(), port);
        imgNetworkClient_.reset();
    }
}

void ADTimePix::imgDisconnect() {
    epicsMutexLock(imgMutex_);
    imgConnected_ = false;
    epicsMutexUnlock(imgMutex_);
    
    if (imgNetworkClient_) {
        imgNetworkClient_->disconnect();
        imgNetworkClient_.reset();
    }
}

void ADTimePix::prvImgConnect() {
    static const char* functionName = "prvImgConnect";
    
    if (!prvImgMutex_) {
        ERR("PrvImg TCP: Mutex not initialized");
        return;
    }
    
    epicsMutexLock(prvImgMutex_);
    std::string host = prvImgHost_;
    int port = prvImgPort_;
    epicsMutexUnlock(prvImgMutex_);
    
    if (host.empty() || port <= 0) {
        ERR("PrvImg TCP: Invalid host or port");
        return;
    }
    
    prvImgDisconnect(); // Ensure clean state
    
    prvImgNetworkClient_.reset(new NetworkClient());
    if (!prvImgNetworkClient_) {
        ERR("PrvImg TCP: Failed to create NetworkClient");
        return;
    }
    
    if (prvImgNetworkClient_->connect(host, port)) {
        epicsMutexLock(prvImgMutex_);
        prvImgConnected_ = true;
        epicsMutexUnlock(prvImgMutex_);
        LOG_ARGS("PrvImg TCP connected to %s:%d", host.c_str(), port);
    } else {
        ERR_ARGS("PrvImg TCP failed to connect to %s:%d", host.c_str(), port);
        prvImgNetworkClient_.reset();
    }
}

void ADTimePix::prvImgDisconnect() {
    const char* functionName = "prvImgDisconnect";
    
    epicsMutexLock(prvImgMutex_);
    prvImgConnected_ = false;
    epicsMutexUnlock(prvImgMutex_);
    
    if (prvImgNetworkClient_) {
        prvImgNetworkClient_->disconnect();
        prvImgNetworkClient_.reset();
    }
    
    LOG("PrvImg TCP disconnected");
}

asynStatus ADTimePix::readImageFromTCP() {
    const char* functionName = "readImageFromTCP";
    
    // Check if we should use TCP streaming
    std::string filePath;
    getStringParam(ADTimePixPrvImgBase, filePath);
    
    if (filePath.find("tcp://") != 0) {
        // Not TCP: TCP streaming is required for preview images
        // GraphicsMagick HTTP method has been removed
        ERR("PrvImg requires TCP streaming (tcp:// format). GraphicsMagick HTTP method no longer supported.");
        return asynError;
    }
    
    // Check format - must be jsonimage (format index 3)
    int format;
    getIntegerParam(ADTimePixPrvImgFormat, &format);
    if (format != 3) {
        ERR_ARGS("PrvImg TCP streaming requires jsonimage format (3), got %d", format);
        return asynError;
    }
    
    // The worker thread is started in acquireStart() when WritePrvImg is enabled
    // This function just checks if TCP streaming should be used
    // The worker thread processes frames and updates pArrays[0] asynchronously
    
    // Wait briefly for a frame to be available (worker thread processes it)
    epicsMutexLock(prvImgMutex_);
    bool connected = prvImgConnected_;
    epicsMutexUnlock(prvImgMutex_);
    
    if (!connected) {
        // Connection not established yet - this is OK, worker thread will connect
        return asynSuccess;
    }
    
    // Frame will be processed by worker thread and pArrays[0] will be updated
    return asynSuccess;
}


//----------------------------------------------------------------------------
// ADTimePix Constructor/Destructor
//----------------------------------------------------------------------------

ADTimePix::ADTimePix(const char* portName, const char* serverURL, int maxBuffers, size_t maxMemory, int priority, int stackSize)
    : ADDriver(portName, 4, (int)NUM_TIMEPIX_PARAMS, maxBuffers, maxMemory,
        asynInt64Mask | asynEnumMask | asynInt32ArrayMask | asynInt64ArrayMask,
        asynInt64Mask | asynEnumMask | asynInt32ArrayMask | asynInt64ArrayMask,
        ASYN_MULTIDEVICE | ASYN_CANBLOCK,
        1,
        priority,
        stackSize),
      imgCurrentFrame_(512, 512, ImageData::PixelFormat::UINT16, ImageData::DataType::FRAME_DATA)
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

    // GraphicsMagick initialization removed - TCP streaming is used instead
    // GraphicsMagick implementation preserved in preserve/graphicsmagick-preview branch

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
    createParam(ADTimePixWriteImg1String,                  asynParamInt32,  &ADTimePixWriteImg1);
    createParam(ADTimePixWritePrvImgString,                asynParamInt32,  &ADTimePixWritePrvImg);
    createParam(ADTimePixWritePrvImg1String,               asynParamInt32,  &ADTimePixWritePrvImg1);
    createParam(ADTimePixWritePrvHstString,                asynParamInt32,  &ADTimePixWritePrvHst);
    // Server, Read back channels from Serval
    createParam(ADTimePixWriteRawReadString,                   asynParamInt32,  &ADTimePixWriteRawRead);
    createParam(ADTimePixWriteRaw1ReadString,                  asynParamInt32,  &ADTimePixWriteRaw1Read);   // Serval 3.3.0
    createParam(ADTimePixWriteImgReadString,                   asynParamInt32,  &ADTimePixWriteImgRead);
    createParam(ADTimePixWriteImg1ReadString,                  asynParamInt32,  &ADTimePixWriteImg1Read);
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
    // Server, Image, ImageChannels[1]
    createParam(ADTimePixImg1BaseString,                  asynParamOctet,    &ADTimePixImg1Base);              
    createParam(ADTimePixImg1FilePatString,               asynParamOctet,    &ADTimePixImg1FilePat);            
    createParam(ADTimePixImg1FormatString,                asynParamInt32,    &ADTimePixImg1Format);            
    createParam(ADTimePixImg1ModeString,                  asynParamInt32,    &ADTimePixImg1Mode);              
    createParam(ADTimePixImg1ThsString,                   asynParamOctet,    &ADTimePixImg1Ths);             
    createParam(ADTimePixImg1IntSizeString,               asynParamInt32,    &ADTimePixImg1IntSize); 
    createParam(ADTimePixImg1IntModeString,               asynParamInt32,    &ADTimePixImg1IntMode);            
    createParam(ADTimePixImg1StpOnDskLimString,           asynParamInt32,    &ADTimePixImg1StpOnDskLim);        
    createParam(ADTimePixImg1QueueSizeString,             asynParamInt32,    &ADTimePixImg1QueueSize);         
    createParam(ADTimePixImg1FilePathExistsString,        asynParamInt32,    &ADTimePixImg1FilePathExists);    
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
    // PrvImg TCP streaming metadata
    createParam(ADTimePixPrvImgFrameNumberString,            asynParamInt32, &ADTimePixPrvImgFrameNumber);
    createParam(ADTimePixPrvImgTimeAtFrameString,            asynParamFloat64, &ADTimePixPrvImgTimeAtFrame);
    createParam(ADTimePixPrvImgAcqRateString,                asynParamFloat64, &ADTimePixPrvImgAcqRate);
    // Img TCP streaming metadata
    createParam(ADTimePixImgFrameNumberString,               asynParamInt32, &ADTimePixImgFrameNumber);
    createParam(ADTimePixImgTimeAtFrameString,               asynParamFloat64, &ADTimePixImgTimeAtFrame);
    createParam(ADTimePixImgAcqRateString,                   asynParamFloat64, &ADTimePixImgAcqRate);
    // Img channel accumulation and display data
    createParam(ADTimePixImgImageDataString,                 asynParamInt64Array, &ADTimePixImgImageData);
    createParam(ADTimePixImgImageFrameString,                asynParamInt32Array, &ADTimePixImgImageFrame);
    createParam(ADTimePixImgImageSumNFramesString,           asynParamInt64Array, &ADTimePixImgImageSumNFrames);
    createParam(ADTimePixImgAccumulationEnableString,        asynParamInt32, &ADTimePixImgAccumulationEnable);
    createParam(ADTimePixImgFramesToSumString,               asynParamInt32, &ADTimePixImgFramesToSum);
    createParam(ADTimePixImgSumUpdateIntervalString,         asynParamInt32, &ADTimePixImgSumUpdateIntervalFrames);
    createParam(ADTimePixImgTotalCountsString,               asynParamInt64, &ADTimePixImgTotalCounts);
    createParam(ADTimePixImgProcessingTimeString,            asynParamFloat64, &ADTimePixImgProcessingTime);
    createParam(ADTimePixImgMemoryUsageString,               asynParamFloat64, &ADTimePixImgMemoryUsage);
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
    createParam(ADTimePixPrvHstStreamString,    asynParamInt32,     &ADTimePixPrvHstStream);

    //sets driver version
    char versionString[25];
    epicsSnprintf(versionString, sizeof(versionString), "%d.%d.%d", ADTIMEPIX_VERSION, ADTIMEPIX_REVISION, ADTIMEPIX_MODIFICATION);
    setStringParam(NDDriverVersion, versionString);
    setStringParam(ADTimePixServerName, serverURL);

    // Initialize TCP streaming for PrvImg channel
    prvImgNetworkClient_.reset();
    prvImgHost_ = "";
    prvImgPort_ = 0;
    prvImgConnected_ = false;
    prvImgRunning_ = false;
    prvImgWorkerThreadId_ = nullptr;
    prvImgMutex_ = epicsMutexMustCreate();
    if (!prvImgMutex_) {
        ERR("Failed to create PrvImg mutex");
    }
    prvImgLineBuffer_.resize(MAX_BUFFER_SIZE);
    prvImgTotalRead_ = 0;
    prvImgFormat_ = 0;
    
    // Initialize PrvImg metadata tracking
    prvImgPreviousFrameNumber_ = 0;
    prvImgPreviousTimeAtFrame_ = 0.0;
    prvImgAcquisitionRate_ = 0.0;
    prvImgLastRateUpdateTime_ = 0.0;
    prvImgFirstFrameReceived_ = false;
    
    // Initialize TCP streaming for Img channel
    imgNetworkClient_.reset();
    imgHost_ = "";
    imgPort_ = 0;
    imgConnected_ = false;
    imgRunning_ = false;
    imgWorkerThreadId_ = nullptr;
    imgMutex_ = epicsMutexMustCreate();
    if (!imgMutex_) {
        ERR("Failed to create Img mutex");
    }
    imgLineBuffer_.resize(MAX_BUFFER_SIZE);
    imgTotalRead_ = 0;
    imgFormat_ = 0;
    
    // Initialize Img metadata tracking
    imgPreviousFrameNumber_ = 0;
    imgPreviousTimeAtFrame_ = 0.0;
    imgAcquisitionRate_ = 0.0;
    imgLastRateUpdateTime_ = 0.0;
    imgFirstFrameReceived_ = false;
    
    // Initialize Img channel accumulation and frame buffer
    imgRunningSum_.reset();
    imgFrameBuffer_.clear();
    // imgCurrentFrame_ is initialized in constructor initialization list
    imgFramesToSum_ = 10;
    imgSumUpdateIntervalFrames_ = 1;
    imgFramesSinceLastSumUpdate_ = 0;
    
    // Initialize Img channel performance tracking
    imgProcessingTimeSamples_.clear();
    imgLastProcessingTimeUpdate_ = 0.0;
    imgLastMemoryUpdateTime_ = 0.0;
    imgProcessingTime_ = 0.0;
    imgMemoryUsage_ = 0.0;
    imgTotalCounts_ = 0;
    
    // Set initial parameter values
    setIntegerParam(ADTimePixImgAccumulationEnable, 1);  // Default: enabled
    setIntegerParam(ADTimePixImgFramesToSum, imgFramesToSum_);
    setIntegerParam(ADTimePixImgSumUpdateIntervalFrames, imgSumUpdateIntervalFrames_);
    setInteger64Param(ADTimePixImgTotalCounts, 0);
    setDoubleParam(ADTimePixImgProcessingTime, 0.0);
    // Calculate initial memory usage (will be 0.0 initially since buffers are empty)
    imgMemoryUsage_ = calculateImgMemoryUsageMB();
    setDoubleParam(ADTimePixImgMemoryUsage, imgMemoryUsage_);

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
    
    // Stop PrvImg TCP streaming
    epicsMutexLock(prvImgMutex_);
    prvImgRunning_ = false;
    epicsMutexUnlock(prvImgMutex_);
    
    if (prvImgWorkerThreadId_ != NULL) {
        epicsThreadMustJoin(prvImgWorkerThreadId_);
        prvImgWorkerThreadId_ = NULL;
    }
    prvImgDisconnect();
    
    if (prvImgMutex_) {
        epicsMutexDestroy(prvImgMutex_);
        prvImgMutex_ = NULL;
    }
    
    // Stop Img TCP streaming
    epicsMutexLock(imgMutex_);
    imgRunning_ = false;
    epicsMutexUnlock(imgMutex_);
    
    if (imgWorkerThreadId_ != NULL) {
        epicsThreadMustJoin(imgWorkerThreadId_);
        imgWorkerThreadId_ = NULL;
    }
    imgDisconnect();
    
    if (imgMutex_) {
        epicsMutexDestroy(imgMutex_);
        imgMutex_ = NULL;
    }
    
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
