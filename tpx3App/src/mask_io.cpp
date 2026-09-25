/*
 * ADTimePix3 - BPC / mask I/O and chip-image coordinate mapping (mask_io)
 *
 * Timepix3: one config byte per chip pixel (65536 B/chip at 256x256).
 * Medipix3 (dual counter): one big-endian 16-bit word per pixel (131072 B/chip).
 * Bit 0 is the per-pixel mask for both families and is updated without changing
 * adjustment, test-pulse, or unknown bits. MPX3 image mapping uses Serval Layout
 * metadata rather than the TPX3 pelIndex() mappings.
 *
 * Copyright (c) 2022 Brookhaven Science Associates, Brookhaven National Laboratory
 * Copyright (c) 2022-2026 UT-Battelle, LLC, Oak Ridge National Laboratory
 *
 * SPDX-License-Identifier: MIT
 */

#include <sys/stat.h>
#include <algorithm>
#include <climits>
#include <limits>
#include <stdexcept>
// Area Detector include
#include "ADTimePix.h"
#include "ADTimePixLog.h"
#include "bpc_file_io.h"
#include "bpc_mask_semantics.h"
#include "serval_pixel_config.h"

extern const char* driverName;  // defined in ADTimePix.cpp (same as histogram_io.cpp)

asynStatus ADTimePix::readInt32Array(asynUser *pasynUser, epicsInt32 *value,
                                size_t nElements, size_t *nIn)
{   
	int reason = pasynUser->reason;

    if (reason == ADTimePixPixelConfigDiff) {
        epicsMutexLock(pixelConfigDiffMutex_);
        size_t n = pixelConfigDiff_.size();
        size_t ncpy = std::min(nElements, n);
        for (size_t i = 0; i < ncpy; ++i) {
            value[i] = pixelConfigDiff_[i];
        }
        for (size_t i = ncpy; i < nElements; ++i) {
            value[i] = 0;
        }
        epicsMutexUnlock(pixelConfigDiffMutex_);
        *nIn = nElements;
        setParamStatus(0, ADTimePixPixelConfigDiff, asynSuccess);
        return asynSuccess;
    }
    
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
    std::vector<std::uint8_t> bpcData;

    // write new mask
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

        if (maskReset_val == 1) {            
            FLOW_ARGS("MaskBPC: reset (waveform nElements=%zu)", nElements);
            maskReset(value, maskOnOff_val);              
        }
        else if (maskRectangle_val == 1) {
            FLOW_ARGS("MaskBPC: rectangle (waveform nElements=%zu)", nElements);
            maskRectangle(value, maskRectangle_MinX, maskRectangle_SizeX, maskRectangle_MinY, maskRectangle_SizeY, maskOnOff_val);
        }
        else if (maskCircle_val == 1) {
            FLOW_ARGS("MaskBPC: circle (waveform nElements=%zu)", nElements);
            maskCircle(value, maskRectangle_MinX, maskRectangle_MinY, maskCircle_Radius, maskOnOff_val);
        }
        else if (maskBPCfile_val == 1) {
            if (!ADTimePix3BpcMask::operatorMaskSupported(detectorFamily_)) {
                std::string message = std::string(detectorFamilyName(detectorFamily_)) +
                    " mask read unavailable: pixel mask encoding is not documented";
                setStringParam(ADTimePixWriteMsg, message.c_str());
                setParamStatus(0, ADTimePixMaskBPC, asynError);
                callParamCallbacks();
                *nIn = 0;
                ERR("MaskBPC: refusing unsupported detector-family mask read");
                return asynError;
            }
            if (readBPCfile(bpcData) != asynSuccess) {
                *nIn = 0;
                return asynError;
            }
            std::vector<std::size_t> imageToBpc;
            if (bpcImageByteOffsets(imageToBpc) != asynSuccess) {
                *nIn = 0;
                return asynError;
            }
            if (!bpcData.empty()) {
                for (size_t v = 0; v < nElements; ++v) {
                    value[v] = 0;
                }
                const std::size_t mapped = std::min(nElements, imageToBpc.size());
                for (std::size_t imageIndex = 0; imageIndex < mapped; ++imageIndex) {
                    const std::size_t byteOffset = imageToBpc[imageIndex];
                    if (byteOffset != std::numeric_limits<std::size_t>::max() &&
                        ADTimePix3BpcMask::isMasked(
                            detectorFamily_, bpcData, byteOffset)) {
                        value[imageIndex] |= 1 << 1;
                    }
                }
            }
        }
        else if (maskWrite_val == 1) {
            if (!ADTimePix3BpcMask::operatorMaskSupported(detectorFamily_)) {
                std::string message = std::string(detectorFamilyName(detectorFamily_)) +
                    " mask write blocked: pixel mask encoding is not documented";
                setStringParam(ADTimePixWriteMsg, message.c_str());
                setParamStatus(0, ADTimePixMaskBPC, asynError);
                callParamCallbacks();
                *nIn = 0;
                ERR("MaskBPC: refusing unsupported detector-family mask write");
                return asynError;
            }
            if (readBPCfile(bpcData) != asynSuccess) {
                *nIn = 0;
                return asynError;
            }
            std::vector<std::size_t> imageToBpc;
            if (bpcImageByteOffsets(imageToBpc) != asynSuccess) {
                *nIn = 0;
                return asynError;
            }
            const std::size_t mapped = std::min(nElements, imageToBpc.size());
            for (std::size_t imageIndex = 0; imageIndex < mapped; ++imageIndex) {
                const std::size_t byteOffset = imageToBpc[imageIndex];
                if (byteOffset != std::numeric_limits<std::size_t>::max() &&
                    (value[imageIndex] & (1 << 0))) {
                    ADTimePix3BpcMask::setMasked(
                        detectorFamily_, bpcData, byteOffset, true);
                }
            }
            if (writeBPCfile(bpcData) != asynSuccess) {
                *nIn = 0;
                return asynError;
            }
        }
        else {
            FLOW_ARGS("MaskBPC: no draw op (nElements=%zu)", nElements);
        }
    }
    else if (reason == ADTimePixBPC) {
        if (readBPCfile(bpcData) != asynSuccess) {
            *nIn = 0;
            return asynError;
        }
        if (!bpcData.empty()) {
            const std::size_t width = ADTimePix3BpcMask::bytesPerPixel(detectorFamily_);
            const std::size_t pixels = width == 0 ? 0 : bpcData.size() / width;
            const std::size_t copied = std::min(nElements, pixels);
            for (std::size_t pixel = 0; pixel < copied; ++pixel) {
                const std::size_t byteOffset = pixel * width;
                std::uint16_t packed = 0;
                ADTimePix3BpcMask::pixelValue(
                    detectorFamily_, bpcData, byteOffset, packed);
                value[pixel] = static_cast<epicsInt32>(packed);
                if (ADTimePix3BpcMask::isMasked(
                        detectorFamily_, bpcData, byteOffset)) {
                    value[pixel] |= width == 1 ? (1 << 8) : (1 << 16);
                }
            }
            for (size_t i = copied; i < nElements; ++i) value[i] = 0;
        }
    }

    if (reason == ADTimePixMaskBPC) {
        setParamStatus(0, ADTimePixMaskBPC, asynSuccess);
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

asynStatus ADTimePix::bpcImageByteOffsets(std::vector<std::size_t>& offsets)
{
    int rows = 0, cols = 0, xChips = 0, yChips = 0, pelWidth = 0;
    rowsCols(&rows, &cols, &xChips, &yChips, &pelWidth);
    (void)xChips;
    (void)yChips;
    const std::size_t invalid = std::numeric_limits<std::size_t>::max();
    if (rows <= 0 || cols <= 0 || pelWidth <= 0 ||
        static_cast<std::size_t>(rows) >
            std::numeric_limits<std::size_t>::max() / static_cast<std::size_t>(cols)) {
        setStringParam(ADTimePixWriteMsg, "Invalid BPC image geometry");
        callParamCallbacks();
        return asynError;
    }
    offsets.assign(static_cast<std::size_t>(rows) * static_cast<std::size_t>(cols), invalid);

    const std::size_t pixelsPerChip = static_cast<std::size_t>(pelWidth) *
                                      static_cast<std::size_t>(pelWidth);
    const auto store = [&](std::size_t imageIndex, std::size_t logicalIndex) -> bool {
        std::size_t byteOffset = 0;
        if (imageIndex >= offsets.size() ||
            !ADTimePix3ServalPixelConfig::selectedSliceIndex(
                logicalIndex, pixelsPerChip,
                detectorCapabilities_.bpcBytesPerPel,
                detectorCapabilities_.bpcThresholdSlices,
                0, byteOffset) || offsets[imageIndex] != invalid) {
            return false;
        }
        offsets[imageIndex] = byteOffset;
        return true;
    };

    if (detectorFamily_ != DetectorFamily::MPX3) {
        for (int imageY = 0; imageY < rows; ++imageY) {
            for (int imageX = 0; imageX < cols; ++imageX) {
                const int logical = pelIndex(imageX, imageY);
                const std::size_t image = static_cast<std::size_t>(imageY) *
                    static_cast<std::size_t>(cols) + static_cast<std::size_t>(imageX);
                if (logical < 0 || !store(image, static_cast<std::size_t>(logical))) {
                    setStringParam(ADTimePixWriteMsg, "BPC image mapping is incomplete");
                    callParamCallbacks();
                    return asynError;
                }
            }
        }
        return asynSuccess;
    }

    int nChips = 0;
    getIntegerParam(ADTimePixNumberOfChips, &nChips);
    for (int chip = 0; chip < nChips; ++chip) {
        std::string layoutText;
        getStringParam(chip, ADTimePixLayout, layoutText);
        try {
            const json chipLayout = json::parse(layoutText);
            if (!chipLayout.is_object() ||
                !chipLayout.contains("Chip") || !chipLayout["Chip"].is_number_integer() ||
                !chipLayout.contains("X") || !chipLayout["X"].is_number_integer() ||
                !chipLayout.contains("Y") || !chipLayout["Y"].is_number_integer() ||
                !chipLayout.contains("Orientation") || !chipLayout["Orientation"].is_string() ||
                chipLayout["Chip"].get<int>() != chip) {
                throw std::runtime_error("incomplete chip layout");
            }
            const int originX = chipLayout["X"].get<int>();
            const int originY = chipLayout["Y"].get<int>();
            const std::string orientation = chipLayout["Orientation"].get<std::string>();
            for (int localY = 0; localY < pelWidth; ++localY) {
                for (int localX = 0; localX < pelWidth; ++localX) {
                    int imageX = 0;
                    int imageY = 0;
                    if (!ADTimePix3ServalPixelConfig::mpx3LayoutCoordinates(
                            localX, localY, pelWidth, originX, originY, rows,
                            orientation, imageX, imageY) ||
                        imageX < 0 || imageX >= cols || imageY < 0 || imageY >= rows) {
                        throw std::runtime_error("unsupported or out-of-range chip layout");
                    }
                    const std::size_t local = static_cast<std::size_t>(localY) *
                        static_cast<std::size_t>(pelWidth) + static_cast<std::size_t>(localX);
                    const std::size_t logical = static_cast<std::size_t>(chip) *
                        pixelsPerChip + local;
                    const std::size_t image = static_cast<std::size_t>(imageY) *
                        static_cast<std::size_t>(cols) + static_cast<std::size_t>(imageX);
                    if (!store(image, logical)) {
                        throw std::runtime_error("duplicate chip layout coordinate");
                    }
                }
            }
        } catch (const std::exception& e) {
            char message[256];
            epicsSnprintf(message, sizeof(message),
                          "MPX3 chip %d layout unavailable: %s", chip, e.what());
            setStringParam(ADTimePixWriteMsg, message);
            ERR_ARGS("%s", message);
            callParamCallbacks();
            return asynError;
        }
    }
    if (std::find(offsets.begin(), offsets.end(), invalid) != offsets.end()) {
        setStringParam(ADTimePixWriteMsg, "MPX3 chip layout does not cover the full image");
        callParamCallbacks();
        return asynError;
    }
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
    std::vector<std::uint8_t> validatedData;
    if (readBPCfile(validatedData) != asynSuccess) {
        ERR("uploadBPC: local BPC validation failed; request not sent");
        return asynError;
    }

    asynStatus status = asynSuccess;
    std::string bpc_file, filePath, fileName;

    getStringParam(ADTimePixBPCFilePath, filePath);
    getStringParam(ADTimePixBPCFileName, fileName);
    bpc_file = this->serverURL + std::string("/config/load?format=pixelconfig&file=") + std::string(filePath) + std::string(fileName);

    cpr::Response r = servalHttpGetAuthOnly(bpc_file);
    if (r.status_code != 200) {
        logHttpFailure("uploadBPC GET /config/load pixelconfig", "GET", bpc_file, (long)r.status_code, r.text);
        status = asynError;
    }
    LOG_ARGS("uploadBPC: http_code=%ld file=%s%s", (long)r.status_code, filePath.c_str(), fileName.c_str());
    setIntegerParam(ADTimePixHttpCode, r.status_code);
    setStringParam(ADTimePixWriteMsg, r.text.c_str());

    return status;
}

asynStatus ADTimePix::expectedBPCSize(std::size_t& size)
{
    int pixelCount = 0;
    getIntegerParam(ADTimePixPixCount, &pixelCount);
    if (!ADTimePix3BpcFile::expectedSize(pixelCount,
                                         detectorCapabilities_.bpcBytesPerPel,
                                         detectorCapabilities_.bpcThresholdSlices,
                                         size)) {
        char message[192];
        epicsSnprintf(message, sizeof(message),
                      "Invalid BPC geometry: pixels=%d bytes/pixel=%d slices=%d",
                      pixelCount, detectorCapabilities_.bpcBytesPerPel,
                      detectorCapabilities_.bpcThresholdSlices);
        setStringParam(ADTimePixWriteMsg, message);
        ERR_ARGS("%s", message);
        callParamCallbacks();
        return asynError;
    }
    return asynSuccess;
}

asynStatus ADTimePix::readBPCfile(std::vector<std::uint8_t>& data) {
    std::size_t expectedSize = 0;

    // BPC calibration mask file to read, and write new mask.
    std::string filePath, fileName, fullFileName;

    getStringParam(ADTimePixBPCFilePath, filePath);
    getStringParam(ADTimePixBPCFileName, fileName);
    fullFileName = filePath + fileName;

//    printf("ReadBPC: name=%s\n", fullFileName.c_str());

    if (expectedBPCSize(expectedSize) != asynSuccess) return asynError;
    const ADTimePix3BpcFile::Status fileStatus =
        ADTimePix3BpcFile::readExact(fullFileName, expectedSize, data);
    if (fileStatus != ADTimePix3BpcFile::Status::Ok) {
        char message[256];
        epicsSnprintf(message, sizeof(message), "ReadBPC: %s: %s",
                      ADTimePix3BpcFile::statusMessage(fileStatus), fullFileName.c_str());
        setStringParam(ADTimePixWriteMsg, message);
        ERR_ARGS("%s", message);
        callParamCallbacks();
        return asynError;
    }

    const bool maskSupported =
        ADTimePix3BpcMask::operatorMaskSupported(detectorFamily_);
    const std::size_t maskedCount =
        ADTimePix3BpcMask::countMasked(detectorFamily_, data);
    const int nMaskedPel = !maskSupported ? -1 :
        (maskedCount > static_cast<std::size_t>(INT_MAX) ?
         INT_MAX : static_cast<int>(maskedCount));
    setIntegerParam(ADTimePixBPCn, nMaskedPel);
    if (maskSupported) {
        LOG_ARGS("ReadBPC: file=\"%s\" bytes=%zu masked %s pixels=%d",
                 fullFileName.c_str(), data.size(),
                 detectorFamilyName(detectorFamily_), nMaskedPel);
    } else {
        LOG_ARGS("ReadBPC: file=\"%s\" bytes=%zu; mask count unavailable for family %s",
                 fullFileName.c_str(), data.size(), detectorFamilyName(detectorFamily_));
    }

    callParamCallbacks();

    return asynSuccess;
}

/*
* Write bpc file containing mask created.
*/
asynStatus ADTimePix::writeBPCfile(const std::vector<std::uint8_t>& data) {
    if (!ADTimePix3BpcMask::operatorMaskSupported(detectorFamily_)) {
        std::string message = std::string(detectorFamilyName(detectorFamily_)) +
            " mask write blocked: pixel mask encoding is not documented";
        setStringParam(ADTimePixWriteMsg, message.c_str());
        ERR("WriteBPC: refusing unsupported detector-family mask write");
        callParamCallbacks();
        return asynError;
    }
    asynStatus status = asynSuccess;
    int pathExists = 0, maskExists = 0;
    std::size_t expectedSize = 0;

    // BPC calibration mask file to write new mask.
    std::string maskFile, filePath, fullFileName, bpcFileName;

    getStringParam(ADTimePixBPCFilePath, filePath);
    getStringParam(ADTimePixMaskFileName, maskFile);
    fullFileName = filePath + maskFile;

    /* checkFile: 0=missing, 1=directory, 2=file, ... (see checkFile) */
    LOG_ARGS("Mask write: BPCFilePath type=%d (1=dir) path=\"%s\" maskName=\"%s\"",
             checkFile(filePath), filePath.c_str(), maskFile.c_str());

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
            LOG_ARGS("Mask: overwriting file \"%s\" (pathExists=%d, maskFileExistsAsFile=%d)", fullFileName.c_str(),
                     pathExists, maskExists);
        }
        else {
            LOG_ARGS("Mask: writing new file \"%s\" (pathExists=%d, maskStat=%d)", fullFileName.c_str(), pathExists,
                     maskExists);
        }
    }
    else {
        char message[256];
        epicsSnprintf(message, sizeof(message),
                      "Mask: BPC file path is not a directory: %s", filePath.c_str());
        setStringParam(ADTimePixWriteMsg, message);
        ERR_ARGS("Mask: BPCFilePath is not a directory (pathExists=%d), aborting write to \"%s\"",
                 pathExists, filePath.c_str());
        callParamCallbacks();
        return asynError;
    }

    if (expectedBPCSize(expectedSize) != asynSuccess) return asynError;
    const ADTimePix3BpcFile::Status fileStatus =
        ADTimePix3BpcFile::writeExact(fullFileName, data, expectedSize);
    if (fileStatus != ADTimePix3BpcFile::Status::Ok) {
        char message[256];
        epicsSnprintf(message, sizeof(message), "WriteBPC: %s: %s",
                      ADTimePix3BpcFile::statusMessage(fileStatus), fullFileName.c_str());
        setStringParam(ADTimePixWriteMsg, message);
        ERR_ARGS("%s", message);
        callParamCallbacks();
        return asynError;
    }

    // Write mask file to TimePix3 chip
    getStringParam(ADTimePixBPCFileName, bpcFileName);
    setStringParam(ADTimePixBPCFileName,maskFile.c_str());
    status = uploadBPC();
    setStringParam(ADTimePixBPCFileName,bpcFileName.c_str());

    callParamCallbacks();

    if (status) {
        ERR("WriteBPC: uploadBPC to SERVAL failed");
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
// int ADTimePix::bpc2ImgIndex(int *x, int *y, int bpcIndexIn, int chipPelWidthIn) {
int ADTimePix::bpc2ImgIndex(int bpcIndexIn, int chipPelWidthIn) {
    int i=0, j=0;
    int detOrientation=0, imgIndex=0, bpcIndex=0;
    int numChips=0, chip=0, chipPelCount=0, chipPelWidth=0;

    getIntegerParam(ADTimePixDetectorOrientation, &detOrientation);
    getIntegerParam(ADTimePixNumberOfChips, &numChips);
    chipPelWidth = chipPelWidthIn;
    bpcIndex = bpcIndexIn;
    chipPelCount = (chipPelWidth) * (chipPelWidth);
    /* 0-based chip from linear BPC index: [0,chipPelCount) -> 0, etc. Do not use (bpcIndex+1)/chipPelCount;
     * that maps the last pel of each chip (e.g. 65535) to the next chip and bpcIndex 262143 -> chip 4,
     * falsely tripping "detector larger than 2x2" and misplacing boundary pixels. */
    chip = (chipPelCount > 0) ? (bpcIndex / chipPelCount) : 0;

    if (numChips == 1) { // single chip TimePix3
        if (detOrientation == 0) {  // UP detector orientation
            i = bpcIndex % chipPelWidth;
            j = chipPelWidth - 1 - bpcIndex / chipPelWidth;
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
            WARN("bpc2ImgIndex: single-chip orientation not supported");
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
                WARN("bpc2ImgIndex: UP layout chip index out of range for 2x2");
                imgIndex = -1;
            } else {
                WARN_ARGS("bpc2ImgIndex: UP unspecified chip tile i=%d j=%d", i, j);
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
                WARN("bpc2ImgIndex: LEFT layout chip index out of range for 2x2");
                imgIndex = -1;
            } else {
                WARN_ARGS("bpc2ImgIndex: LEFT unspecified chip tile i=%d j=%d", i, j);
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
                WARN("bpc2ImgIndex: RIGHT layout chip index out of range for 2x2");
                imgIndex = -1;
            } else {
                WARN_ARGS("bpc2ImgIndex: RIGHT unspecified chip tile i=%d j=%d", i, j);
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
                WARN("bpc2ImgIndex: DOWN layout chip index out of range for 2x2");
                imgIndex = -1;
            } else {
                WARN_ARGS("bpc2ImgIndex: DOWN unspecified chip tile i=%d j=%d", i, j);
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
                WARN("bpc2ImgIndex: UP MIRRORED layout chip index out of range for 2x2");
                imgIndex = -1;
            } else {
                WARN_ARGS("bpc2ImgIndex: UP MIRRORED unspecified chip tile i=%d j=%d", i, j);
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
                WARN("bpc2ImgIndex: RIGHT MIRRORED layout chip index out of range for 2x2");
                imgIndex = -1;
            } else {
                WARN_ARGS("bpc2ImgIndex: RIGHT MIRRORED unspecified chip tile i=%d j=%d", i, j);
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
                WARN("bpc2ImgIndex: DOWN MIRRORED layout chip index out of range for 2x2");
                imgIndex = -1;
            } else {
                WARN_ARGS("bpc2ImgIndex: DOWN MIRRORED unspecified chip tile i=%d j=%d", i, j);
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
                WARN("bpc2ImgIndex: LEFT MIRRORED layout chip index out of range for 2x2");
                imgIndex = -1;
            } else {
                WARN_ARGS("bpc2ImgIndex: LEFT MIRRORED unspecified chip tile i=%d j=%d", i, j);
            }
        } else {
            WARN("bpc2ImgIndex: quad 2x2 orientation not supported");
        }
        imgIndex = i + 2*chipPelWidth*j;
    } else if (numChips == 8) {
        /* 2×4 (or 4×2) mosaic: uniform grid, BPC chip order chip = Y_CHIP * xChips + X_CHIP,
         * intra-chip mapping matches single-chip UP (same convention as one tile of the 2×2 case).
         * Only DetectorOrientation UP (0); other orientations need per-layout tables like 2×2. */
        if (detOrientation != 0) {
            WARN("bpc2ImgIndex: 8-chip mapping only for DetectorOrientation UP (0)");
            return -1;
        }
        int ROWS = 0, COLS = 0, xChips = 0, yChips = 0, w = 0;
        rowsCols(&ROWS, &COLS, &xChips, &yChips, &w);
        if (xChips * yChips != 8 || chipPelCount <= 0 || chip < 0 || chip > 7) {
            WARN("bpc2ImgIndex: 8-chip requires xChips*yChips==8 and chip index 0..7");
            return -1;
        }
        int local = bpcIndex - chip * chipPelCount;
        int lx = local % w;
        int ly = local / w;
        int X_CHIP = chip % xChips;
        int Y_CHIP = chip / xChips;
        i = X_CHIP * w + lx;
        j = Y_CHIP * w + (w - 1 - ly);
        imgIndex = i + (xChips * w) * j;
    } else {
        WARN_ARGS("bpc2ImgIndex: chip count %d not supported (use 1, 4, or 8)", numChips);
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
            WARN("pelIndex: single-chip orientation not supported");
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
                WARN("pelIndex: UP layout chip index out of range for 2x2");
                index = -1;
            } else {
                WARN_ARGS("pelIndex: UP unspecified tile i=%d j=%d xChip=%d yChip=%d", i, j, X_CHIP, Y_CHIP);
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
                WARN("pelIndex: LEFT layout chip index out of range for 2x2");
                index = -1;
            } else {
                WARN_ARGS("pelIndex: LEFT unspecified tile i=%d j=%d xChip=%d yChip=%d", i, j, X_CHIP, Y_CHIP);
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
                WARN("pelIndex: RIGHT layout chip index out of range for 2x2");
                index = -1;
            } else {
                WARN_ARGS("pelIndex: RIGHT unspecified tile i=%d j=%d xChip=%d yChip=%d", i, j, X_CHIP, Y_CHIP);
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
                WARN("pelIndex: DOWN layout chip index out of range for 2x2");
                index = -1;
            } else {
                WARN_ARGS("pelIndex: DOWN unspecified tile i=%d j=%d xChip=%d yChip=%d", i, j, X_CHIP, Y_CHIP);
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
                WARN("pelIndex: UP MIRRORED layout chip index out of range for 2x2");
                index = -1;
            } else {
                WARN_ARGS("pelIndex: UP MIRRORED unspecified tile i=%d j=%d xChip=%d yChip=%d", i, j, X_CHIP, Y_CHIP);
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
                WARN("pelIndex: RIGHT MIRRORED layout chip index out of range for 2x2");
                index = -1;
            } else {
                WARN_ARGS("pelIndex: RIGHT MIRRORED unspecified tile i=%d j=%d xChip=%d yChip=%d", i, j, X_CHIP, Y_CHIP);
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
                WARN("pelIndex: DOWN MIRRORED layout chip index out of range for 2x2");
                index = -1;
            } else {
                WARN_ARGS("pelIndex: DOWN MIRRORED unspecified tile i=%d j=%d xChip=%d yChip=%d", i, j, X_CHIP, Y_CHIP);
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
                WARN("pelIndex: LEFT MIRRORED layout chip index out of range for 2x2");
                index = -1;
            } else {
                WARN_ARGS("pelIndex: LEFT MIRRORED unspecified tile i=%d j=%d xChip=%d yChip=%d", i, j, X_CHIP, Y_CHIP);
            }

        } else {
            WARN("pelIndex: quad orientation not supported for this layout");
        }
    } else if (numChips == 8) {
        if (detOrientation != 0) {
            WARN("pelIndex: 8-chip mapping only for DetectorOrientation UP (0)");
            index = -1;
        } else {
            int ROWS = 0, COLS = 0, xChips = 0, yChips = 0, Pel = 0;
            rowsCols(&ROWS, &COLS, &xChips, &yChips, &Pel);
            if (xChips * yChips != 8 || Pel <= 0 || X_CHIP >= xChips || Y_CHIP >= yChips) {
                WARN("pelIndex: 8-chip geometry mismatch (expect xChips*yChips==8)");
                index = -1;
            } else {
                int lx = i - X_CHIP * Pel;
                int ly = j - Y_CHIP * Pel;
                int chipIdx = Y_CHIP * xChips + X_CHIP;
                index = chipIdx * Pel * Pel + lx + ((Pel - 1) - ly) * Pel;
            }
        }
    } else {
        WARN_ARGS("pelIndex: chip count %d not supported (use 1, 4, or 8)", numChips);
    }

    return index;
}
