#include <sys/stat.h>
// Area Detector include
#include "ADTimePix.h"

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

    // write new mask
    int ROWS = 0, COLS = 0, xCHIPS = 0, yCHIPS = 0, PelWidth = 0;
 
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
        //    printf("The readInt32Array bcp file read, %ld\n", nElements);
            pathExists = checkFile(fullFileName);
            if (pathExists == 2) {  // BPC file exists, read and process it
        //        printf("mask,BPC file Exists=%d\n",pathExists);
                readBPCfile(&bufBPC, &bufBPCSize);
                if (bufBPCSize > 0) {
                    for(size_t i = 0; i < nElements; i++){
                        if (bufBPC[i] & (1 << 0)) {
                            value[i] |= 1 << 1;     // masked pel -> 2
                //            printf("mask bufBPC[%ld]=%d\n",i,value[i]);
                        }
                    }
                }
            }
        }
        else if (maskWrite_val == 1) {
            fullFileName = BPCFilePath + BPCFileName;
            pathExists = checkFile(fullFileName);
            if (pathExists == 2) {  // BPC file exists, read it into bufBPC, and apply mask

                readBPCfile(&bufBPC, &bufBPCSize);
                rowsCols(&ROWS, &COLS, &xCHIPS, &yCHIPS, &PelWidth);
                for (int i = 0; i < ROWS; ++i) {
                    for (int j = 0; j < COLS; ++j) {
                        if (value[i*ROWS + j]  & (1 << 0)) {
                            bufBPC[pelIndex(i, j)] |= (1 << 0);
                        }
                    }
                }
                writeBPCfile(&bufBPC, &bufBPCSize);
            }
            else {
                    printf("Mask Write: BPC directory does not exist,%d,%s\n", pathExists, BPCFilePath.c_str());
                }
        }
        else {
            printf("The readInt32Array no mask, %ld\n", nElements);
        }
    }
    else if (reason == ADTimePixBPC) {
        getStringParam(ADTimePixBPCFilePath, BPCFilePath);
        getStringParam(ADTimePixBPCFileName, BPCFileName);
        fullFileName = BPCFilePath + BPCFileName;

    //    printf("calib,The readInt32Array bcp file read, %ld\n", nElements);
        pathExists = checkFile(fullFileName);
    //    printf("calibration, BPC file Exists=%d\n",pathExists);
        if (pathExists == 2) {  // BPC file exists, read and process it
            readBPCfile(&bufBPC, &bufBPCSize);
    //        printf("bufPBCSize=%d, strLen_bufBPC=%ld\n",bufBPCSize, strlen(bufBPC));
            if (bufBPCSize > 0) {
                for(size_t i = 0; i < nElements; i++){
                    value[i] = bufBPC[i];
                    if (bufBPC[i] & (1 << 0)) {
                        value[i] |= 1 << 8;     // masked pel, 31-> 287
                //        printf("bufBPC[%ld]=%d\n",i,value[i]);
                    }
                }
            }
        }
    }
 
    callParamCallbacks();

    *nIn=nElements;

    return asynSuccess;
}

asynStatus ADTimePix::rowsCols(int *rows, int *cols, int *xChips, int *yChips, int *chipPelWidth) {
    int pixCount, rowLength, numChips, numRows;

    getIntegerParam(ADTimePixPixCount, &pixCount);
    getIntegerParam(ADTimePixRowLen, &rowLength);
    getIntegerParam(ADTimePixNumberOfChips, &numChips);
    getIntegerParam(ADTimePixNumberOfRows, &numRows);

    *xChips = rowLength;
    *yChips = numChips / rowLength;
    *chipPelWidth = numRows / rowLength;
    *rows = numRows;
    *cols = (*yChips) * (*chipPelWidth);

//    printf("rows=%d, cols=%d, xChips=%d, yChips=%d, chipPelWidth=%d\n\n", *rows, *cols, *xChips, *yChips, *chipPelWidth);

    return asynSuccess;
}

asynStatus ADTimePix::maskReset(epicsInt32 *buf, int OnOff) {
//    printf("OnOff=%d\n", OnOff);
    int ROWS = 0, COLS = 0, xCHIPS = 0, yCHIPS = 0, PelWidth = 0;
    rowsCols(&ROWS, &COLS, &xCHIPS, &yCHIPS, &PelWidth);

//    printf("Reset:rows=%d, cols=%d, xChips=%d, yChips=%d, chipPelWidth=%d\n\n", ROWS, COLS, xCHIPS, yCHIPS, PelWidth);
    for (int j = 0; j < ROWS; ++j) {
        for (int i = 0; i < COLS; ++i) {
            buf[j*ROWS + i] = OnOff;
        }
    }
    return asynSuccess;
}

// Mask a nXsize x nYsize rectangle, or pint (nXsize=nYsize=1), or line respectivly
asynStatus ADTimePix::maskRectangle(epicsInt32 *buf, int nX,int nXsize, int nY, int nYsize, int OnOff) {
    int ROWS = 0, COLS = 0, xCHIPS = 0, yCHIPS = 0, PelWidth = 0;
    rowsCols(&ROWS, &COLS, &xCHIPS, &yCHIPS, &PelWidth);

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
    int ROWS = 0, COLS = 0, xCHIPS = 0, yCHIPS = 0, PelWidth = 0;
    rowsCols(&ROWS, &COLS, &xCHIPS, &yCHIPS, &PelWidth);

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

/*
* Check type of filePath, and if it exists
* Return type if exists
* directory->1, file->2, !exists-> 0
*/
int ADTimePix::checkFile(std::string &filePath) {
    struct stat buff;
    int istat, fileStat=0;

    // Return 0 on success, or -1 if unable to get file properties.
    istat = stat(filePath.c_str(), &buff);

    if (!istat) {
        switch (buff.st_mode & S_IFMT) // type of file
            {
                case S_IFBLK:  fileStat = 3; break;    // block special
                case S_IFCHR:  fileStat = 6; break;    // character special
                case S_IFDIR:  fileStat = 1; break;    // directory
                case S_IFIFO:  fileStat = 4; break;    // pipe of FIFO
                case S_IFLNK:  fileStat = 5; break;    // symbolic link
                case S_IFREG:  fileStat = 2; break;    // regular file
                case S_IFSOCK: fileStat = 7; break;    // Socket
                default:       fileStat = 8; break;    // Unknown
            }
    }
//    printf("stat,%s,%d,%d,%d,%d\n", filePath.c_str(), istat, fileStat, buff.st_mode, S_IFMT);

    return fileStat;
}

/*
* buf - bpc file read into char array
* bufSize - number of bytes / elements / pixels in the bpc file
*/
asynStatus ADTimePix::readBPCfile(char **buf, int *bufSize) {   

    FILE *sourceFile;
    long fileSize;
    char *buffer;
    int nMaskedPel=0;
    int nRead=0;

    *bufSize = 0;

    // BPC calibration mask file to read, and write new mask.
    std::string maskFile, filePath, fileName, fullFileName;

    getStringParam(ADTimePixBPCFilePath, filePath);
    getStringParam(ADTimePixBPCFileName, fileName);
    fullFileName = filePath + fileName;

//    printf("ReadBCP: name=%s\n", fullFileName.c_str());

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

    // Read the entire BPC file into the buffer
    nRead = fread(buffer, 1, fileSize, sourceFile);
    fclose(sourceFile);

    // pass pointer to buffer;
    *buf = buffer;
    *bufSize = fileSize;
    // printf("\nbufSize=%d, fileSize=%ld, strlen_buf=%ld, strlen_buffer=%ld\n", *bufSize, fileSize, strlen(*buf), strlen(buffer));

    for (int i = 0; i < fileSize; i++) {
        // Check if the bit at position N=0 is set to 1
        if (buffer[i] & (1 << 0)) {
            nMaskedPel += 1;
        //    printf("Bit i=%d, nMaskedPel=%d, buffer[i]=%d\n", i, nMaskedPel, buffer[i]);
        }
    }
    setIntegerParam(ADTimePixBPCn, nMaskedPel);
    printf("ReadBPC: Masked pixels=%d, nRead=%d\n", nMaskedPel, nRead);

    callParamCallbacks();

    return asynSuccess;
}

/*
* Write bpc file containing mask created.
*/
asynStatus ADTimePix::writeBPCfile(char **buf, int *bufSize) {
    int status = asynSuccess;
    int pathExists = 0, maskExists = 0;
    FILE *destFile;

    // BPC calibration mask file to write new mask.
    std::string maskFile, filePath, fileName, fullFileName, bpcFileName;

    getStringParam(ADTimePixBPCFilePath, filePath);
    getStringParam(ADTimePixMaskFileName, maskFile);
    fullFileName = filePath + maskFile;

    printf("Mask:%d,%ld,%ld\n",checkFile(filePath), strlen(filePath.c_str()), strlen(maskFile.c_str()));

    pathExists = checkFile(filePath);
    maskExists = checkFile(fullFileName);
    if ((maskExists != 2) &&  (strlen(maskFile.c_str()) == 0)) {
        setStringParam(ADTimePixMaskFileName,"mask.bpc");
        getStringParam(ADTimePixMaskFileName, maskFile);
        fullFileName = filePath + maskFile;
        maskExists = checkFile(fullFileName);
    }

    if (pathExists == 1) {
        if (maskExists == 2) {
            printf("Mask: overwriting=%s,%d,%d\n", fullFileName.c_str(), pathExists, maskExists);
        }
        else {
            printf("Mask: writing new=%s,%d,%d\n", fullFileName.c_str(), pathExists, maskExists);
        }
    }
    else {
        printf("Mask: Path does not exist, exiting,%d\n", pathExists);
        return asynSuccess;
    }

    destFile = fopen(fullFileName.c_str(), "wb");
    if (destFile == NULL) {
        perror("Error opening destination file");
        return asynError;
    }

    // Write the buffer to the destination file
    fwrite(*buf, 1, *bufSize, destFile);

    // Clean up
    fclose(destFile);

//    printf("File copied successfully.\n");

    // Write mask file to TimePix3 chip
    getStringParam(ADTimePixBPCFileName, bpcFileName);
    setStringParam(ADTimePixBPCFileName,maskFile.c_str());
    status = uploadBPC();
    setStringParam(ADTimePixBPCFileName,bpcFileName.c_str());

    callParamCallbacks();

    if(status){
        printf("WriteBPC: Unable to uplad BPC\n");
        return asynError;
    }

    return asynSuccess;
}

/*
* Returns which chip image tile pixel belongs to. Need to convert to TimePix3 chip order.
* Input: x, y  (pixel coordinate)
* x - mask x pixel
* y - mask y pixel
* Output: xChip, yChip (chip grid)
* xChip - chip x grid location
* yChip - chip y grid number
* (0,0) | (0,1)
* (1,0) | (1,1)
*/
asynStatus ADTimePix::findChip(int x, int y, int *xChip, int *yChip, int *width) {
    int ROWS = 0, COLS = 0, xCHIPS = 0, yCHIPS = 0, PelWidth = 0;
    rowsCols(&ROWS, &COLS, &xCHIPS, &yCHIPS, &PelWidth);

    *xChip = x / PelWidth;
    *yChip = y / PelWidth;
    *width = PelWidth;

    return asynSuccess;
}

/*
* Index of mask vector
* i,j coordinates of the pixel starting from top left in image mask PV
*/
int ADTimePix::pelIndex(int i, int j) {
    int index=0, ii=0,jj=0;
    int X_CHIP=0, Y_CHIP=0, PelWidth=0;

    findChip(i, j, &X_CHIP, &Y_CHIP, &PelWidth);

    // TODO, one-chip TimePix3 detector

    // Four chip 2x2 TimePix3 UP detector
    ii = i - PelWidth;
    jj = j - PelWidth;
    if ((X_CHIP == 1) && (Y_CHIP == 1)) { // tile (1,1), chip 0
        index = ii - (jj - (PelWidth - 1))*PelWidth;
    }
    if ((X_CHIP == 1) && (Y_CHIP == 0)) { // tile (1,0), chip 1
        index = PelWidth*PelWidth + ((PelWidth - 1) - ii) + j * PelWidth;
    }
    if ((X_CHIP == 0) && (Y_CHIP == 0)) { // tile (0,0), chip 2
        index = 2*PelWidth*PelWidth + ((PelWidth - 1) - i) + j * PelWidth;
    }
    if ((X_CHIP == 0) && (Y_CHIP == 1)) { // tile (0,1), chip 3
        index = 3*PelWidth*PelWidth + i - (jj - (PelWidth - 1))*PelWidth;
    }

    if ((X_CHIP > 1) || (Y_CHIP > 1)) {
        printf("ERROR index: detector larger than 2x2\n");
        index = -1;
    }

    return index;
}
