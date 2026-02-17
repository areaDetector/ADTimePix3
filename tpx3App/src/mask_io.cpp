#include <sys/stat.h>
// Area Detector include
#include "ADTimePix.h"

asynStatus ADTimePix::readInt32Array(asynUser *pasynUser, epicsInt32 *value,
                                size_t nElements, size_t *nIn)
{   
	int reason = pasynUser->reason;
    
    // Handle Img channel accumulation arrays
    if (reason == ADTimePixImgImageFrame) {
        epicsMutexLock(imgMutex_);
        // Check if imgCurrentFrame_ has valid data (pixel_count > 0)
        size_t pixel_count = imgCurrentFrame_.get_pixel_count();
        if (pixel_count == 0) {
            // Not initialized yet, return zeros
            for (size_t i = 0; i < nElements; ++i) {
                value[i] = 0;
            }
            *nIn = nElements;
            epicsMutexUnlock(imgMutex_);
            return asynSuccess;
        }
        size_t elements_to_copy = std::min(nElements, pixel_count);
        
        if (imgCurrentFrame_.get_pixel_format() == ImageData::PixelFormat::UINT16) {
            const uint16_t* pixels = imgCurrentFrame_.get_pixels_16_ptr();
            for (size_t i = 0; i < elements_to_copy; ++i) {
                value[i] = static_cast<epicsInt32>(pixels[i]);
            }
        } else {
            const uint32_t* pixels = imgCurrentFrame_.get_pixels_32_ptr();
            for (size_t i = 0; i < elements_to_copy; ++i) {
                value[i] = static_cast<epicsInt32>(pixels[i]);
            }
        }
        
        // Zero out remaining elements
        for (size_t i = elements_to_copy; i < nElements; ++i) {
            value[i] = 0;
        }
        *nIn = nElements;
        epicsMutexUnlock(imgMutex_);
        return asynSuccess;
    }
    
    // Handle mask-related arrays (existing functionality)
    ADDriver::readInt32Array(pasynUser, value, nElements, nIn);
    int maskOnOff_val, maskReset_val, maskRectangle_val, maskCircle_val;
    int maskRectangle_MinX, maskRectangle_SizeX, maskRectangle_MinY, maskRectangle_SizeY, maskCircle_Radius;
    int maskBPCfile_val, maskWrite_val;
    int pathExists=0;

    std::string BPCFilePath, BPCFileName, maskFileName, fullFileName;

    // buffer for reading BPC file (caller must free when done)
    char *bufBPC = NULL;
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
                rowsCols(&ROWS, &COLS, &xCHIPS, &yCHIPS, &PelWidth);
                if (bufBPCSize > 0) {
                    for(size_t i = 0; i < nElements; i++){
                        if (bufBPC[i] & (1 << 0)) {
                        //    value[i] |= 1 << 1;     // masked pel -> 2; bcpIndex
                            value[bcp2ImgIndex(i, PelWidth)] |= 1 << 1;     // masked pel -> 2; imgIndex
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
                for (int j = 0; j < COLS; ++j) {
                    for (int i = 0; i < ROWS; ++i) {
                        if (value[j*COLS + i]  & (1 << 0)) {
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

    // Free BPC buffer if readBPCfile() allocated it (avoids memory leak)
    if (bufBPC) {
        free(bufBPC);
        bufBPC = NULL;
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

/**
 * Check BPC file path exists and set BPC_FILE_PATH_EXISTS.
 * Implemented here with other BPC/mask path logic (see ADTimePix.cpp checkPath).
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

/**
 * Upload Binary Pixel Configuration to SERVAL.
 * serverURL + /config/load?format=pixelconfig&file= path + fileName.
 */
asynStatus ADTimePix::uploadBPC(){
    asynStatus status = asynSuccess;
    std::string bpc_file, filePath, fileName;

    getStringParam(ADTimePixBPCFilePath, filePath);
    getStringParam(ADTimePixBPCFileName, fileName);
    bpc_file = this->serverURL + std::string("/config/load?format=pixelconfig&file=") + std::string(filePath) + std::string(fileName);

    cpr::Response r = cpr::Get(cpr::Url{bpc_file},
                           cpr::Authentication{"user", "pass", cpr::AuthMode::BASIC});
    printf("uploadBPC: http_code=%li\n", r.status_code);
    setIntegerParam(ADTimePixHttpCode, r.status_code);
    setStringParam(ADTimePixWriteMsg, r.text.c_str());

    return status;
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

/* bpc vector index into AD image index;  x/y (i,j) are image coordinates
* i,j coordinates of the AD image with pixel starting from top left in image mask PV
* bpcIndex - index of pixel in bpc file
* imgIndex - index of pixel in AD image
*/
// int ADTimePix::bcp2ImgIndex(int *x, int *y, int bpcIndexIn, int chipPelWidthIn) {
int ADTimePix::bcp2ImgIndex(int bpcIndexIn, int chipPelWidthIn) {
    int i=0, j=0;
    int detOrientation=0, imgIndex=0, bpcIndex=0;
    int numChips=0, chip=0, chipPelCount=0, chipPelWidth=0;

    getIntegerParam(ADTimePixDetectorOrientation, &detOrientation);
    getIntegerParam(ADTimePixNumberOfChips, &numChips);
    chipPelWidth = chipPelWidthIn;
    bpcIndex = bpcIndexIn;
    chipPelCount = (chipPelWidth) * (chipPelWidth);
    chip = (bpcIndex + 1) / chipPelCount;

    if (numChips == 1) { // single chip TimePix3
        if (detOrientation == 0) {  // UP detector orientation
            i = bpcIndex % chipPelWidth;
            j = chipPelWidth - 1 - bpcIndex / chipPelWidth;
            printf("remainder=%d,quotient=%d,imgIndex=%d\n", i, j, imgIndex);
        } else if (detOrientation == 1) {  // RIGHT
            i = bpcIndex / chipPelWidth;
            j = bpcIndex % chipPelWidth;
        } else if (detOrientation == 2) {  // DOWN
            i = chipPelWidth - 1 - bpcIndex % chipPelWidth;
            j = bpcIndex / chipPelWidth;
        } else if (detOrientation == 3) {  // LEFT
            i = chipPelWidth - 1 - bpcIndex / chipPelWidth;
            j = chipPelWidth - 1 - bpcIndex % chipPelWidth;
        } else if (detOrientation == 4) {  // UP MIRRORED
            i = chipPelWidth - 1 - bpcIndex % chipPelWidth;
            j = chipPelWidth - 1 - bpcIndex / chipPelWidth;
        } else if (detOrientation == 5) {  // RIGHT MIRRORED
            i = chipPelWidth - 1 - bpcIndex / chipPelWidth;
            j = bpcIndex % chipPelWidth;
        } else if (detOrientation == 6) {  // DOWN MIRRORED
            i = bpcIndex % chipPelWidth;
            j = bpcIndex / chipPelWidth;
        } else if (detOrientation == 7) {  // LEFT MIRRORED
            i = bpcIndex / chipPelWidth;
            j = chipPelWidth - 1 - bpcIndex % chipPelWidth;
        } else {
            printf("bpc2Img: One chip not met conditions\n");
        }
        imgIndex = i + chipPelWidth*j;
    } else if (numChips == 4) {  // quad chip 2x2 TimePix3
        if (detOrientation == 0) {  // UP detector orientation
            if (chip == 0) { // tile (1,1), chip 0
                i = chipPelWidth + bpcIndex % chipPelWidth;
                j = 2*chipPelWidth - 1 - bpcIndex / chipPelWidth;
            } else if (chip == 1) { // tile (1,0), chip 1
                i = 2*chipPelWidth - 1 - (bpcIndex - chipPelCount) % chipPelWidth;
                j = (bpcIndex - chipPelCount) / chipPelWidth;
            } else if (chip == 2) { // tile (0,0), chip 2
                i = chipPelWidth - 1 - (bpcIndex - 2*chipPelCount) % chipPelWidth;
                j = (bpcIndex - 2*chipPelCount) / chipPelWidth;
            } else if (chip == 3) { // tile (0,1), chip 3
                i = (bpcIndex - 3*chipPelCount) % chipPelWidth;
                j = 2*chipPelWidth - 1 - (bpcIndex - 3*chipPelCount) / chipPelWidth;
            } else if (chip > 3) {
                printf("UP index: detector larger than 2x2\n");
                imgIndex = -1;
            } else {
                printf("UP:Unspecified chip sizes\n,i=%d,j=%d\n", i, j);
            }
        } else if (detOrientation == 3) {  // LEFT detector orientation
            if (chip == 3) { // tile (1,1), chip 3
                i = 2*chipPelWidth - 1 - (bpcIndex - 3*chipPelCount) / chipPelWidth;
                j = 2*chipPelWidth - 1 - (bpcIndex - 3*chipPelCount) % chipPelWidth;
            } else if (chip == 0) { // tile (1,0), chip 0
                i = 2*chipPelWidth - 1 - bpcIndex / chipPelWidth;
                j = bpcIndex % chipPelWidth;
            } else if (chip == 1) { // tile (0,0), chip 1
                i = (bpcIndex - chipPelCount) / chipPelWidth;
                j = (bpcIndex - chipPelCount) % chipPelWidth;
            } else if (chip == 2) { // tile (0,1), chip 2
                i = (bpcIndex - 2*chipPelCount) / chipPelWidth;
                j = chipPelWidth + (bpcIndex - 2*chipPelCount) % chipPelWidth;
            } else if (chip > 3) {
                printf("LEFT: detector larger than 2x2\n");
                imgIndex = -1;
            } else {
                printf("LEFT:Unspecified chip sizes\n,i=%d,j=%d\n", i, j);
            }
        } else if (detOrientation == 1) {  // RIGHT detector orientation
            if (chip == 1) { // tile (1,1), chip 1
                i = 2*chipPelWidth - 1 - (bpcIndex - chipPelCount) / chipPelWidth;
                j = 2*chipPelWidth - 1 - (bpcIndex - chipPelCount) % chipPelWidth;
            } else if (chip == 2) { // tile (1,0), chip 2
                i = 2*chipPelWidth - 1 - (bpcIndex - 2*chipPelCount) / chipPelWidth;
                j = chipPelWidth - 1 - (bpcIndex - 2*chipPelCount) % chipPelWidth;
            } else if (chip == 3) { // tile (0,0), chip 3
                i = (bpcIndex - 3*chipPelCount) / chipPelWidth;
                j = (bpcIndex - 3*chipPelCount) % chipPelWidth;
            } else if (chip == 0) { // tile (0,1), chip 0
                i = (bpcIndex) / chipPelWidth;
                j = chipPelWidth + (bpcIndex) % chipPelWidth;
            } else if (chip > 3) {
                printf("RIGHT: detector larger than 2x2\n");
                imgIndex = -1;
            } else {
                printf("RIGHT:Unspecified chip sizes\n,i=%d,j=%d\n", i, j);
            }
        } else if (detOrientation == 2) {  // DOWN detector orientation
            if (chip == 2) { // tile (1,1), chip 2
                i = chipPelWidth + (bpcIndex - 2*chipPelCount) % chipPelWidth;
                j = 2*chipPelWidth - 1 - (bpcIndex - 2*chipPelCount) / chipPelWidth;
            } else if (chip == 3) { // tile (1,0), chip 3
                i = 2*chipPelWidth - 1 - (bpcIndex - 3*chipPelCount) % chipPelWidth;
                j = (bpcIndex - 3*chipPelCount) / chipPelWidth;
            } else if (chip == 0) { // tile (0,0), chip 0
                i = chipPelWidth - 1 - (bpcIndex) % chipPelWidth;
                j = (bpcIndex) / chipPelWidth;
            } else if (chip == 1) { // tile (0,1), chip 1
                i = (bpcIndex - chipPelCount) % chipPelWidth;
                j = 2*chipPelWidth - 1 - (bpcIndex - chipPelCount) / chipPelWidth;
            } else if (chip > 3) {
                printf("DOWN: detector larger than 2x2\n");
                imgIndex = -1;
            } else {
                printf("DOWN:Unspecified chip sizes\n,i=%d,j=%d\n", i, j);
            }
        } else if (detOrientation == 4) {  // UP MIRRORED detector orientation
            if (chip == 3) { // tile (1,1), chip 3
                i = 2*chipPelWidth - 1 - (bpcIndex - 3*chipPelCount) % chipPelWidth;
                j = 2*chipPelWidth - 1 - (bpcIndex - 3*chipPelCount) / chipPelWidth;
            } else if (chip == 2) { // tile (1,0), chip 2
                i = chipPelWidth + (bpcIndex - 2*chipPelCount) % chipPelWidth;
                j = (bpcIndex - 2*chipPelCount) / chipPelWidth;
            } else if (chip == 1) { // tile (0,0), chip 1
                i = (bpcIndex - chipPelCount) % chipPelWidth;
                j = (bpcIndex - chipPelCount) / chipPelWidth;
            } else if (chip == 0) { // tile (0,1), chip 0
                i = chipPelWidth - 1 - (bpcIndex) % chipPelWidth;
                j = 2*chipPelWidth - 1 - (bpcIndex) / chipPelWidth;
            } else if (chip > 3) {
                printf("UP MIRRORED: detector larger than 2x2\n");
                imgIndex = -1;
            } else {
                printf("UP MIRRORED:Unspecified chip sizes\n,i=%d,j=%d\n", i, j);
            }
        } else if (detOrientation == 5) {  // RIGHT MIRRORED detector orientation
            if (chip == 0) { // tile (1,1), chip 0
                i = 2*chipPelWidth - 1 - (bpcIndex) / chipPelWidth;
                j = chipPelWidth + (bpcIndex) % chipPelWidth;
            } else if (chip == 3) { // tile (1,0), chip 3
                i = 2*chipPelWidth - 1 - (bpcIndex - 3*chipPelCount) / chipPelWidth;
                j = (bpcIndex - 3*chipPelCount) % chipPelWidth;
            } else if (chip == 2) { // tile (0,0), chip 2
                i = (bpcIndex - 2*chipPelCount) / chipPelWidth;
                j = chipPelWidth - 1 - (bpcIndex - 2*chipPelCount) % chipPelWidth;
            } else if (chip == 1) { // tile (0,1), chip 1
                i = (bpcIndex - chipPelCount) / chipPelWidth;
                j = 2*chipPelWidth - 1 - (bpcIndex - chipPelCount) % chipPelWidth;
            } else if (chip > 3) {
                printf("RIGHT MIRRORED: detector larger than 2x2\n");
                imgIndex = -1;
            } else {
                printf("RIGHT MIRRORED:Unspecified chip sizes\n,i=%d,j=%d\n", i, j);
            }
        } else if (detOrientation == 6) {  // DOWN MIRRORED detector orientation
            if (chip == 1) { // tile (1,1), chip 1
                i = 2*chipPelWidth - 1 - (bpcIndex - chipPelCount) % chipPelWidth;
                j = 2*chipPelWidth - 1 - (bpcIndex - chipPelCount) / chipPelWidth;
            } else if (chip == 0) { // tile (1,0), chip 0
                i = chipPelWidth + (bpcIndex) % chipPelWidth;
                j = (bpcIndex) / chipPelWidth;
            } else if (chip == 3) { // tile (0,0), chip 3
                i = (bpcIndex - 3*chipPelCount) % chipPelWidth;
                j = (bpcIndex - 3*chipPelCount) / chipPelWidth;
            } else if (chip == 2) { // tile (0,1), chip 2
                i = chipPelWidth - 1 - (bpcIndex - 2*chipPelCount) % chipPelWidth;
                j = 2*chipPelWidth - 1 - (bpcIndex - 2*chipPelCount) / chipPelWidth;
            } else if (chip > 3) {
                printf("DOWN MIRRORED: detector larger than 2x2\n");
                imgIndex = -1;
            } else {
                printf("DOWN MIRRORED:Unspecified chip sizes\n,i=%d,j=%d\n", i, j);
            }
        } else if (detOrientation == 7) {  // LEFT MIRRORED detector orientation
            if (chip == 2) { // tile (1,1), chip 2
                i = 2*chipPelWidth - 1 - (bpcIndex - 2*chipPelCount) / chipPelWidth;
                j = chipPelWidth + (bpcIndex - 2*chipPelCount) % chipPelWidth;
            } else if (chip == 1) { // tile (1,0), chip 1
                i = 2*chipPelWidth - 1 - (bpcIndex - chipPelCount) / chipPelWidth;
                j = (bpcIndex - chipPelCount) % chipPelWidth;
            } else if (chip == 0) { // tile (0,0), chip 0
                i = (bpcIndex) / chipPelWidth;
                j = chipPelWidth - 1 - (bpcIndex) % chipPelWidth;
            } else if (chip == 3) { // tile (0,1), chip 3
                i = (bpcIndex - 3*chipPelCount) / chipPelWidth;
                j = 2*chipPelWidth - 1 - (bpcIndex - 3*chipPelCount) % chipPelWidth;
            } else if (chip > 3) {
                printf("LEFT MIRRORED: detector larger than 2x2\n");
                imgIndex = -1;
            } else {
                printf("LEFT MIRRORED:Unspecified chip sizes\n,i=%d,j=%d\n", i, j);
            }
        } else {
            printf("bpc2ImgIndex: Quad chip 2x2 not met mask conditions\n");
        }
        imgIndex = i + 2*chipPelWidth*j;
    } else { // octet 2x4 TimePix3, or another arangement. Not 1 or 2x2 chip(s).
        printf("TODO: Octet 2x4 or another chip arangement\n");
        imgIndex = -1;
    }

    return imgIndex;
}

/*
* Index of mask vector
* i,j coordinates of the pixel starting from top left in image mask PV
*/
int ADTimePix::pelIndex(int i, int j) {
    int index=0, ii=0,jj=0;
    int X_CHIP=0, Y_CHIP=0, PelWidth=0, detOrientation=0;
    int numChips=0;

    getIntegerParam(ADTimePixDetectorOrientation, &detOrientation);
    getIntegerParam(ADTimePixNumberOfChips, &numChips);
    findChip(i, j, &X_CHIP, &Y_CHIP, &PelWidth);

    callParamCallbacks();

    // TODO: 2x4 chip detector
    // One-chip TimePix3 detector
    if (numChips == 1) {
        if (detOrientation == 0) {  // UP detector orientation
            index = i + ((PelWidth - 1) - j)*PelWidth;
        } else if (detOrientation == 1) {  // RIGHT
            index = j + i*PelWidth;
        } else if (detOrientation == 2) {  // DOWN
            index = ((PelWidth - 1) - i) + j*PelWidth;
        } else if (detOrientation == 3) {  // LEFT
            index =  ((PelWidth - 1) - j) +  ((PelWidth - 1) - i)*PelWidth;
        } else if (detOrientation == 4) {  // UP MIRRORED
            index =  ((PelWidth - 1) - i) +  ((PelWidth - 1) - j)*PelWidth;
        } else if (detOrientation == 5) {  // RIGHT MIRRORED
            index = j + ((PelWidth - 1) - i)*PelWidth;
        } else if (detOrientation == 6) {  // DOWN MIRRORED
            index = i + j*PelWidth;
        } else if (detOrientation == 7) {  // LEFT MIRRORED
            index = ((PelWidth - 1) - j) + i*PelWidth;
        } else {
            printf("One chip not met conditions\n");
        }

    } else if (numChips == 4) {
        // Four chip 2x2 TimePix3 detector orientations
        ii = i - PelWidth;
        jj = j - PelWidth;

        if (detOrientation == 0) {  // UP detector orientation
            if ((X_CHIP == 1) && (Y_CHIP == 1)) { // tile (1,1), chip 0
                index = ii - (jj - (PelWidth - 1))*PelWidth;
            } else if ((X_CHIP == 1) && (Y_CHIP == 0)) { // tile (1,0), chip 1
                index = PelWidth*PelWidth + ((PelWidth - 1) - ii) + j * PelWidth;
            } else if ((X_CHIP == 0) && (Y_CHIP == 0)) { // tile (0,0), chip 2
                index = 2*PelWidth*PelWidth + ((PelWidth - 1) - i) + j * PelWidth;
            } else if ((X_CHIP == 0) && (Y_CHIP == 1)) { // tile (0,1), chip 3
                index = 3*PelWidth*PelWidth + i - (jj - (PelWidth - 1))*PelWidth;
            } else if ((X_CHIP > 1) || (Y_CHIP > 1)) {
                printf("UP index: detector larger than 2x2\n");
                index = -1;
            } else {
                printf("UP:Unspecified chip sizes\n,i=%d,j=%d,xChip=%d,yChip=%d\n", i, j, X_CHIP, Y_CHIP);
            }
        } else if (detOrientation == 3) {  // LEFT detector orientation
            if ((X_CHIP == 1) && (Y_CHIP == 1)) { // tile (1,1), chip 3
                index = 3*PelWidth*PelWidth + ((PelWidth - 1) - jj) + ((PelWidth - 1) - ii)*PelWidth;
            } else if ((X_CHIP == 1) && (Y_CHIP == 0)) { // tile (1,0), chip 0
                index = ((PelWidth - 1) - j) +  ((PelWidth - 1) - ii) * PelWidth;
            } else if ((X_CHIP == 0) && (Y_CHIP == 0)) { // tile (0,0), chip 1
                index = PelWidth*PelWidth + j + i * PelWidth;
            } else if ((X_CHIP == 0) && (Y_CHIP == 1)) { // tile (0,1), chip 2
                index = 2*PelWidth*PelWidth + jj + i * PelWidth;
            } else if ((X_CHIP > 1) || (Y_CHIP > 1)) {
                printf("LEFT: detector larger than 2x2\n");
                index = -1;
            } else {
                printf("LEFT:Unspecified chip sizes\n,i=%d,j=%d,xChip=%d,yChip=%d\n", i, j, X_CHIP, Y_CHIP);
            }
        } else if (detOrientation == 1) {  // RIGHT detector orientation
            if ((X_CHIP == 1) && (Y_CHIP == 1)) { // tile (1,1), chip 1
                index = PelWidth*PelWidth + ((PelWidth - 1) - jj) + ((PelWidth - 1) - ii)*PelWidth;
            } else if ((X_CHIP == 1) && (Y_CHIP == 0)) { // tile (1,0), chip 2
                index = 2*PelWidth*PelWidth + ((PelWidth - 1) - j) +  ((PelWidth - 1) - ii) * PelWidth;
            } else if ((X_CHIP == 0) && (Y_CHIP == 0)) { // tile (0,0), chip 3
                index = 3*PelWidth*PelWidth + j + i * PelWidth;
            } else if ((X_CHIP == 0) && (Y_CHIP == 1)) { // tile (0,1), chip 0
                index = jj +  i * PelWidth;
            } else if ((X_CHIP > 1) || (Y_CHIP > 1)) {
                printf("RIGHT: detector larger than 2x2\n");
                index = -1;
            } else {
                printf("RIGHT:Unspecified chip sizes\n,i=%d,j=%d,xChip=%d,yChip=%d\n", i, j, X_CHIP, Y_CHIP);
            }
        } else if (detOrientation == 2) {  // DOWN detector orientation
            if ((X_CHIP == 1) && (Y_CHIP == 1)) { // tile (1,1), chip 2
                index = 2*PelWidth*PelWidth + ii + ((PelWidth - 1) - jj)*PelWidth;
            } else if ((X_CHIP == 1) && (Y_CHIP == 0)) { // tile (1,0), chip 3
                index = 3*PelWidth*PelWidth + ((PelWidth - 1) - ii) +  j * PelWidth;
            } else if ((X_CHIP == 0) && (Y_CHIP == 0)) { // tile (0,0), chip 0
                index =  ((PelWidth - 1) - i) + j * PelWidth;
            } else if ((X_CHIP == 0) && (Y_CHIP == 1)) { // tile (0,1), chip 1
                index = PelWidth*PelWidth + i +  ((PelWidth - 1) - jj) * PelWidth;
            } else if ((X_CHIP > 1) || (Y_CHIP > 1)) {
                printf("DOWN: detector larger than 2x2\n");
                index = -1;
            } else {
                printf("DOWN:Unspecified chip sizes\n,i=%d,j=%d,xChip=%d,yChip=%d\n", i, j, X_CHIP, Y_CHIP);
            }
        } else if (detOrientation == 4) {  // UP MIRRORED detector orientation
            if ((X_CHIP == 1) && (Y_CHIP == 1)) { // tile (1,1), chip 3
                index = 3*PelWidth*PelWidth + ((PelWidth - 1) - ii) + ((PelWidth - 1) - jj)*PelWidth;
            } else if ((X_CHIP == 1) && (Y_CHIP == 0)) { // tile (1,0), chip 2
                index = 2*PelWidth*PelWidth + ii + j * PelWidth;
            } else if ((X_CHIP == 0) && (Y_CHIP == 0)) { // tile (0,0), chip 1
                index =  PelWidth*PelWidth + i + j * PelWidth;
            } else if ((X_CHIP == 0) && (Y_CHIP == 1)) { // tile (0,1), chip 0
                index = ((PelWidth - 1) - i) + ((PelWidth - 1) - jj) * PelWidth;
            } else if ((X_CHIP > 1) || (Y_CHIP > 1)) {
                printf("UP MIRRORED: detector larger than 2x2\n");
                index = -1;
            } else {
                printf("UP MIRRORED:Unspecified chip sizes\n,i=%d,j=%d,xChip=%d,yChip=%d\n", i, j, X_CHIP, Y_CHIP);
            }
        } else if (detOrientation == 5) {  // RIGHT MIRRORED detector orientation
            if ((X_CHIP == 1) && (Y_CHIP == 1)) { // tile (1,1), chip 0
                index = jj + ((PelWidth - 1) - ii)*PelWidth;
            } else if ((X_CHIP == 1) && (Y_CHIP == 0)) { // tile (1,0), chip 3
                index = 3*PelWidth*PelWidth + j + ((PelWidth - 1) - ii) * PelWidth;
            } else if ((X_CHIP == 0) && (Y_CHIP == 0)) { // tile (0,0), chip 2
                index =  2*PelWidth*PelWidth + ((PelWidth - 1) - j) + i * PelWidth;
            } else if ((X_CHIP == 0) && (Y_CHIP == 1)) { // tile (0,1), chip 1
                index = PelWidth*PelWidth + ((PelWidth - 1) - jj) + i * PelWidth;
            } else if ((X_CHIP > 1) || (Y_CHIP > 1)) {
                printf("RIGHT MIRRORED: detector larger than 2x2\n");
                index = -1;
            } else {
                printf("RIGHT MIRRORED:Unspecified chip sizes\n,i=%d,j=%d,xChip=%d,yChip=%d\n", i, j, X_CHIP, Y_CHIP);
            }
        } else if (detOrientation == 6) {  // DOWN MIRRORED detector orientation
            if ((X_CHIP == 1) && (Y_CHIP == 1)) { // tile (1,1), chip 1
                index = PelWidth*PelWidth + ((PelWidth - 1) - ii) + ((PelWidth - 1) - jj)*PelWidth;
            } else if ((X_CHIP == 1) && (Y_CHIP == 0)) { // tile (1,0), chip 0
                index = ii + j * PelWidth;
            } else if ((X_CHIP == 0) && (Y_CHIP == 0)) { // tile (0,0), chip 3
                index =  3*PelWidth*PelWidth + i + j * PelWidth;
            } else if ((X_CHIP == 0) && (Y_CHIP == 1)) { // tile (0,1), chip 2
                index = 2*PelWidth*PelWidth + ((PelWidth - 1) - i) + ((PelWidth - 1) - jj) * PelWidth;
            } else if ((X_CHIP > 1) || (Y_CHIP > 1)) {
                printf("DOWN MIRRORED: detector larger than 2x2\n");
                index = -1;
            } else {
                printf("DOWN MIRRORED:Unspecified chip sizes\n,i=%d,j=%d,xChip=%d,yChip=%d\n", i, j, X_CHIP, Y_CHIP);
            }
        } else if (detOrientation == 7) {  // LEFT MIRRORED detector orientation
            if ((X_CHIP == 1) && (Y_CHIP == 1)) { // tile (1,1), chip 2
                index = 2*PelWidth*PelWidth + jj + ((PelWidth - 1) - ii)*PelWidth;
            } else if ((X_CHIP == 1) && (Y_CHIP == 0)) { // tile (1,0), chip 1
                index = PelWidth*PelWidth + j + ((PelWidth - 1) - ii) * PelWidth;
            } else if ((X_CHIP == 0) && (Y_CHIP == 0)) { // tile (0,0), chip 0
                index =  ((PelWidth - 1) - j) + i * PelWidth;
            } else if ((X_CHIP == 0) && (Y_CHIP == 1)) { // tile (0,1), chip 3
                index = 3*PelWidth*PelWidth + ((PelWidth - 1) - jj) + i * PelWidth;
            } else if ((X_CHIP > 1) || (Y_CHIP > 1)) {
                printf("LEFT MIRRORED: detector larger than 2x2\n");
                index = -1;
            } else {
                printf("LEFT MIRRORED:Unspecified chip sizes\n,i=%d,j=%d,xChip=%d,yChip=%d\n", i, j, X_CHIP, Y_CHIP);
            }

        } else {
            printf("Mask computations are for 2x2 detector only\n");
        }
    } else {
        printf("Mask: Neither 1,4 -chip mask.\n");
    }

    return index;
}
