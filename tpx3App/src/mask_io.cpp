#include <sys/stat.h>
// Area Detector include
#include "ADTimePix.h"

#define XCHIPS 2  // Set the number of horizontal chips
#define YCHIPS 2  // Set the number of vertical chips
#define X_PEL  256  // Set the number of rows in chip
#define Y_PEL  256  // Set the number of columns in chip
#define ROWS (YCHIPS * Y_PEL)  // Set the number of horizontal pixels
#define COLS (XCHIPS * X_PEL)  // Set the number of vertical pixels

asynStatus ADTimePix::readInt32Array(asynUser *pasynUser, epicsInt32 *value,
                                size_t nElements, size_t *nIn)
{   
    ADDriver::readInt32Array(pasynUser, value, nElements, nIn);
	int reason = pasynUser->reason;
    int maskOnOff_val, maskReset_val, maskRectangle_val, maskCircle_val;
    int maskRectangle_MinX, maskRectangle_SizeX, maskRectangle_MinY, maskRectangle_SizeY, maskCircle_Radius;
    int maskBPCfile_val, maskWrite_val;
    int pathExists=0;

    std::string BPCFilePath, BPCFileName, maskFileName, fullFileName;

    // buffer for reading BPC file
    char *bufBPC;
    int bufBPCSize = 0;
 
	if(reason == ADTimePixMaskBPC){
        getIntegerParam(ADTimePixMaskReset,&maskReset_val);
        getIntegerParam(ADTimePixMaskOnOffPel,&maskOnOff_val);
        getIntegerParam(ADTimePixMaskRectangle,&maskRectangle_val);
        getIntegerParam(ADTimePixMaskCircle,&maskCircle_val);

        getIntegerParam(ADTimePixMaskMinX,&maskRectangle_MinX);
        getIntegerParam(ADTimePixMaskSizeX,&maskRectangle_SizeX);
        getIntegerParam(ADTimePixMaskMinY,&maskRectangle_MinY);
        getIntegerParam(ADTimePixMaskSizeY,&maskRectangle_SizeY);
        getIntegerParam(ADTimePixMaskRadius,&maskCircle_Radius);
        getIntegerParam(ADTimePixMaskPel,&maskBPCfile_val);
        getIntegerParam(ADTimePixMaskWrite,&maskWrite_val);

        getStringParam(ADTimePixBPCFilePath, BPCFilePath);
        getStringParam(ADTimePixBPCFileName, BPCFileName);
        getStringParam(ADTimePixMaskFileName, maskFileName);

        if (maskReset_val == 1) {            
            printf("The readInt32Array reset 0, %ld\n", nElements);
            maskReset(value, maskOnOff_val);              
        }
        else if (maskRectangle_val == 1) {
            printf("The readInt32Array rectangle, %ld\n", nElements);
            maskRectangle(value, maskRectangle_MinX, maskRectangle_SizeX, maskRectangle_MinY, maskRectangle_SizeY, maskOnOff_val);
        }
        else if (maskCircle_val == 1) {
            printf("The readInt32Array circle, %ld\n", nElements);
            maskCircle(value, maskRectangle_MinX, maskRectangle_MinY, maskCircle_Radius, maskOnOff_val);
        }
        else if (maskBPCfile_val == 1) {
            fullFileName = BPCFilePath + BPCFileName;
            printf("The readInt32Array bcp file read, %ld\n", nElements);
            pathExists = checkFile(fullFileName);
            printf("BPC file Exists=%d\n",pathExists);
            if (pathExists == 2) {  // BPC file exists, read and process it
    //            readBPCfile(value, bufSize);  // write ADTimePix:readOctet ??
            }
        }
        else if (maskWrite_val == 1) {
            fullFileName = BPCFilePath + maskFileName;
            printf("The readInt32Array write mask, %ld\n", nElements);
            pathExists = checkFile(BPCFilePath);
            printf("BPC pathExists=%d\n",pathExists);
            pathExists = checkFile(fullFileName);
            printf("mask fileExists=%d\n",pathExists);
        }
        else {
            printf("The readInt32Array no mask, %ld\n", nElements);
        }
    }
    else if (reason == ADTimePixBPC) {
        printf("The readInt8, nElements=%ld\n", nElements);
        readBPCfile(&bufBPC, &bufBPCSize);
        printf("bufPBCSize=%d, strLen_bufBPC=%ld\n",bufBPCSize, strlen(bufBPC));
        if (bufBPCSize > 0) {
            for(size_t i = 0; i < nElements; i++){
                value[i] = bufBPC[i];
            //    if (i > 262100) {
            //       printf("%x ",value[i]);
            //    }
            }
        }
    }
 
    callParamCallbacks();

    *nIn=nElements;

    return asynSuccess;
}

asynStatus ADTimePix::maskReset(epicsInt32 *buf, int OnOff) {
//    printf("OnOff=%d\n", OnOff);
    for (int j = 0; j < ROWS; ++j) {
        for (int i = 0; i < COLS; ++i) {
            buf[j*ROWS + i] = OnOff;
        }
    }
    return asynSuccess;
}

// Mask a nXsize x nYsize rectangle, or pint (nXsize=nYsize=1), or line respectivly
asynStatus ADTimePix::maskRectangle(epicsInt32 *buf, int nX,int nXsize, int nY, int nYsize, int OnOff) {
    for (int j = nY; j < nY+nYsize; ++j) {
        if ( j < ROWS ) {
            for (int i = nX; i < nX + nXsize; ++i) {
                if (i < COLS) {
                    if (OnOff) {
                        buf[j*ROWS + i] |= (1 << 0);    // set mask bit to 1
                    }
                    else {
                        buf[j*ROWS + i] &= ~(1 << 0);   // set mask bit 0 0
                    }
                }
            }
        }
    }
    return asynSuccess;
}

// Mask a circle: OnOff=1 set bit to 1; OnOff=0, set bit to 0
// 0 -> pixel is counting; 1-> pixel is not counting
asynStatus ADTimePix::maskCircle(epicsInt32 *buf, int nX,int nY, int nRadius, int OnOff) {
    for (int j = nY - nRadius; j <= nY+nRadius; ++j) {
        for (int i = nX - nRadius; i <= nX + nRadius; ++i) {
            if ((j >= 0) && (j < ROWS) && (i >= 0) && (i < COLS) && (((i - nX)*(i - nX) + (j - nY)*(j - nY)) <= nRadius*nRadius)) {
                if (OnOff) {
                    buf[j*ROWS + i] |= (1 << 0);    // set mask bit to 1
                }
                else {
                    buf[j*ROWS + i] &= ~(1 << 0);   // set mask bit 0 0
                }
            }
        }
    }
    return asynSuccess;
}

// filePath can be a full file path name: directory->1, file->2, !exists-> 0
int ADTimePix::checkFile(std::string &filePath) {
    struct stat buff;
    int istat, isDir=0, isFile=0, fileStat=0;

    // Return 0 on success, or -1 if unable to get file properties.
    istat = stat(filePath.c_str(), &buff);
    if (!istat) isDir = (S_IFDIR & buff.st_mode);
    if (!istat && isDir) {
        fileStat = 1;
    //    printf("path success,%s\n", filePath.c_str());
    }

    if (!istat) isFile = (S_IFMT & buff.st_mode);
    if (!istat && isFile) {
        fileStat = 2;
    //    printf("file success,%s\n", filePath.c_str());
    }
    return fileStat;
}

asynStatus ADTimePix::readBPCfile(char **buf, int *bufSize) {   

    FILE *sourceFile;
    long fileSize;
    char *buffer;
    int nMaskedPel=0;
//    int OnOffPel=0;

    *bufSize = 0;

    // BPC calibration mask file to read, and write new mask.
    std::string maskFile, filePath, fileName, fullFileName;

    getStringParam(ADTimePixBPCFilePath, filePath);
    getStringParam(ADTimePixBPCFileName, fileName);
    fullFileName = filePath + fileName;

    printf("Full File Name = %s\n", fullFileName.c_str());

    // Open the source file for reading
    sourceFile = fopen(fullFileName.c_str(), "rb");
    if (sourceFile == NULL) {
        perror("Error opening source file");
        return asynSuccess;
    }

    // Determine the size of the source file
    fseek(sourceFile, 0, SEEK_END);  // Move to the end of the file
    fileSize = ftell(sourceFile);    // Get the current position (file size)
    rewind(sourceFile);              // Go back to the beginning of the file

    // Allocate memory for the buffer
    buffer = (char *)malloc(fileSize);
    if (buffer == NULL) {
        perror("Error allocating memory");
        fclose(sourceFile);
        return asynSuccess;
    }

    // Read the entire file into the buffer
    fread(buffer, 1, fileSize, sourceFile);
    fclose(sourceFile);

    // pass pointer to buffer;
    *buf = buffer;
    *bufSize = fileSize;
    for(int i=0;i<20;i++){
        printf("%x,",buffer[i]);
    }
    printf("\nbufSize=%d, fileSize=%ld, strlen_buf=%ld, strlen_buffer=%ld\n", *bufSize, fileSize, strlen(*buf), strlen(buffer));

    for (int i = 0; i < fileSize; i++) {
        // Check if the bit at position N=0 is set to 1
        if (buffer[i] & (1 << 0)) {
            nMaskedPel += 1;
    //        printf("Bit i=%d, nMaskedPel=%d\n", i, nMaskedPel);
        }
    }
    printf("Number of masked pixels=%d\n", nMaskedPel);

    return asynSuccess;
}