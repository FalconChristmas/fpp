/*
 *   Pixel Overlay handler for Falcon Player (FPP)
 *
 *   Copyright (C) 2013-2018 the Falcon Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Player (FPP) is free software; you can redistribute it
 *   and/or modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include <memory>

#include <boost/algorithm/string/replace.hpp>
#include <jsoncpp/json/json.h>

#include <GraphicsMagick/wand/magick_wand.h>

#include "common.h"
#include "log.h"
#include "PixelOverlay.h"
#include "PixelOverlayControl.h"
#include "Sequence.h"
#include "settings.h"
#include "channeloutputthread.h"

PixelOverlayManager PixelOverlayManager::INSTANCE;

PixelOverlayModel::PixelOverlayModel(FPPChannelMemoryMapControlBlock *b,
                                     const std::string &n,
                                     char         *cdm,
                                     long long    *pm)
: block(b), name(n), chanDataMap(cdm), pixelMap(pm) {
}
PixelOverlayModel::~PixelOverlayModel() {
    
}

int PixelOverlayModel::getWidth() const {
    int h, w;
    getSize(w, h);
    return w;
}
int PixelOverlayModel::getHeight() const {
    int h, w;
    getSize(w, h);
    return h;
}
void PixelOverlayModel::getSize(int &w, int &h) const {
    h = block->stringCount * block->strandsPerString;
    w = block->channelCount / 3;
    w /= h;

    if (block->orientation == 'V') {
        std::swap(w, h);
    }
}
PixelOverlayState PixelOverlayModel::getState() const {
    int i = block->isActive;
    return PixelOverlayState(i);
}
void PixelOverlayModel::setState(const PixelOverlayState &state) {
    block->isActive = state.getState();
}

void PixelOverlayModel::setData(const uint8_t *data) {
    for (int c = 0; c < block->channelCount; c++) {
        chanDataMap[pixelMap[block->startChannel + c - 1]] = data[c];
    }
}
void PixelOverlayModel::fill(int r, int g, int b) {
    int start = block->startChannel - 1;
    int end = block->startChannel + block->channelCount - 2;
    
    for (int c = start; c <= end;) {
        chanDataMap[c++] = r;
        chanDataMap[c++] = g;
        chanDataMap[c++] = b;
    }
}
void PixelOverlayModel::setValue(uint8_t value, int startChannel, int endChannel) {
    int start;
    int end;
    
    if (startChannel != -1) {
        start = startChannel >= block->startChannel ? startChannel : block->startChannel;
    } else {
        start = block->startChannel;
    }
    
    int modelEnd = block->startChannel + block->channelCount - 1;
    if (endChannel != -1) {
        end = endChannel <= modelEnd ? endChannel : modelEnd;
    } else {
        end = modelEnd;
    }
    
    // Offset for zero-based arrays
    start--;
    end--;
    
    for (int c = start; c <= end; c++) {
        chanDataMap[c] = value;
    }
}

int PixelOverlayModel::getStartChannel() const {
    return block->startChannel;
}
int PixelOverlayModel::getChannelCount() const {
    return block->channelCount;
}
bool PixelOverlayModel::isHorizontal() const {
    return block->orientation == 'H';
}
int PixelOverlayModel::getNumStrings() const {
    return block->stringCount;
}
int PixelOverlayModel::getStrandsPerString() const {
    return block->strandsPerString;
}
std::string PixelOverlayModel::getStartCorner() const {
    return block->startCorner;
}



PixelOverlayManager::PixelOverlayManager() {
    
}
PixelOverlayManager::~PixelOverlayManager() {
    for (auto a : models) {
        delete a.second;
    }
    models.clear();
    if (chanDataMap) {
        munmap(chanDataMap, FPPD_MAX_CHANNELS);
    }
    if (ctrlMap) {
        munmap(ctrlMap, FPPCHANNELMEMORYMAPSIZE);
    }
    if (pixelMap) {
        munmap(pixelMap, FPPD_MAX_CHANNELS * sizeof(long long));
    }
}
void PixelOverlayManager::Initialize() {
    if (!createChannelDataMap()) {
        return;
    }
    if (!createControlMap()) {
        return;
    }
    if (!createPixelMap()) {
        return;
    }
    loadModelMap();
    if (ctrlHeader->totalBlocks) {
        StartChannelOutputThread();
    }
}

bool PixelOverlayManager::createChannelDataMap() {
    LogDebug(VB_CHANNELOUT, "PixelOverlayManager::createChannelDataMap()\n");
    
    // Block of of raw channel data used to overlay data
    int chanDataMapFD = open(FPPCHANNELMEMORYMAPDATAFILE, O_CREAT | O_TRUNC | O_RDWR, 0666);
    if (chanDataMapFD < 0) {
        LogErr(VB_CHANNELOUT, "Error opening %s memory map file: %s\n",
               FPPCHANNELMEMORYMAPDATAFILE, strerror(errno));
        return false;
    }
    chmod(FPPCHANNELMEMORYMAPDATAFILE, 0666);
    uint8_t * tmpData = (uint8_t*)calloc(1, FPPD_MAX_CHANNELS);
    if (write(chanDataMapFD, (void *)tmpData, FPPD_MAX_CHANNELS) != FPPD_MAX_CHANNELS) {
        LogErr(VB_CHANNELOUT, "Error populating %s memory map file: %s\n",
               FPPCHANNELMEMORYMAPDATAFILE, strerror(errno));
        free(tmpData);
        close(chanDataMapFD);
        return false;
    }
    free(tmpData);
    
    chanDataMap = (char *)mmap(0, FPPD_MAX_CHANNELS, PROT_READ|PROT_WRITE, MAP_SHARED, chanDataMapFD, 0);
    close(chanDataMapFD);
    if (!chanDataMap) {
        LogErr(VB_CHANNELOUT, "Error mapping %s memory map file: %s\n",
               FPPCHANNELMEMORYMAPDATAFILE, strerror(errno));
        return false;
    }
    return true;
}

bool PixelOverlayManager::createControlMap() {
    // Control file to turn on/off blocks and get block info
    int ctrlFD = open(FPPCHANNELMEMORYMAPCTRLFILE, O_CREAT | O_TRUNC | O_RDWR, 0666);
    if (ctrlFD < 0) {
        LogErr(VB_CHANNELOUT, "Error opening %s memory map file: %s\n",
               FPPCHANNELMEMORYMAPCTRLFILE, strerror(errno));
        close(ctrlFD);
        return false;
    }
    chmod(FPPCHANNELMEMORYMAPCTRLFILE, 0666);
    
    uint8_t* tmpData = (uint8_t*)calloc(1, FPPCHANNELMEMORYMAPSIZE);
    if (write(ctrlFD, (void *)tmpData, FPPCHANNELMEMORYMAPSIZE) != FPPCHANNELMEMORYMAPSIZE) {
        LogErr(VB_CHANNELOUT, "Error populating %s memory map file: %s\n",
               FPPCHANNELMEMORYMAPCTRLFILE, strerror(errno));
        close(ctrlFD);
        free(tmpData);
        return false;
    }
    free(tmpData);
    
    ctrlMap = (char *)mmap(0, FPPCHANNELMEMORYMAPSIZE, PROT_READ|PROT_WRITE, MAP_SHARED, ctrlFD, 0);
    close(ctrlFD);
    if (!ctrlMap) {
        LogErr(VB_CHANNELOUT, "Error mapping %s memory map file: %s\n",
               FPPCHANNELMEMORYMAPCTRLFILE, strerror(errno));
        return false;
    }
    ctrlHeader = (FPPChannelMemoryMapControlHeader *)ctrlMap;
    ctrlHeader->majorVersion = FPPCHANNELMEMORYMAPMAJORVER;
    ctrlHeader->minorVersion = FPPCHANNELMEMORYMAPMINORVER;
    ctrlHeader->totalBlocks  = 0;
    ctrlHeader->testMode     = 0;

    return true;
}

bool PixelOverlayManager::createPixelMap() {
    // Pixel Map to map channels to matrix positions
    int pixelFD =
        open(FPPCHANNELMEMORYMAPPIXELFILE, O_CREAT | O_TRUNC | O_RDWR, 0666);
    
    if (pixelFD < 0) {
        LogErr(VB_CHANNELOUT, "Error opening %s memory map file: %s\n",
               FPPCHANNELMEMORYMAPPIXELFILE, strerror(errno));
        return false;
    }
    
    chmod(FPPCHANNELMEMORYMAPPIXELFILE, 0666);
    uint8_t* tmpData = (uint8_t*)calloc(FPPD_MAX_CHANNELS, sizeof(long long));
    if (write(pixelFD, (void *)tmpData,
              FPPD_MAX_CHANNELS * sizeof(long long)) != (FPPD_MAX_CHANNELS * sizeof(long long))) {
        LogErr(VB_CHANNELOUT, "Error populating %s memory map file: %s\n",
               FPPCHANNELMEMORYMAPPIXELFILE, strerror(errno));
        close(pixelFD);
        free(tmpData);
        return false;
    }
    free(tmpData);
    
    pixelMap = (long long *)mmap(0, FPPD_MAX_CHANNELS * sizeof(long long), PROT_READ|PROT_WRITE, MAP_SHARED, pixelFD, 0);
    close(pixelFD);
    if (!pixelMap) {
        LogErr(VB_CHANNELOUT, "Error mapping %s memory map file: %s\n",
               FPPCHANNELMEMORYMAPPIXELFILE, strerror(errno));
        return false;
    }
    for (int i = 0; i < FPPD_MAX_CHANNELS; i++) {
        pixelMap[i] = i;
    }
    return true;
}


/*
 * Display list of defined memory mapped channel blocks
 */
static void PrintChannelMapBlocks(FPPChannelMemoryMapControlHeader *ctrlHeader) {
    if (!ctrlHeader) {
        return;
    }
    
    int i = 0;
    char * ctrlMap = (char *)ctrlHeader;
    FPPChannelMemoryMapControlBlock *cb =
    (FPPChannelMemoryMapControlBlock*)(ctrlMap +
                                       sizeof(FPPChannelMemoryMapControlHeader));
    
    LogInfo(VB_CHANNELOUT, "Channel Memory Map Blocks\n");
    LogInfo(VB_CHANNELOUT, "Control File Version: %d.%d\n",
            (int)ctrlHeader->majorVersion, (int)ctrlHeader->minorVersion);
    LogInfo(VB_CHANNELOUT, "Total Blocks Defined: %d\n",
            ctrlHeader->totalBlocks);
    
    for (i = 0; i < ctrlHeader->totalBlocks; i++, cb++) {
        LogExcess(VB_CHANNELOUT, "Block %3d Name         : %s\n", i, cb->blockName);
        LogExcess(VB_CHANNELOUT, "Block %3d Start Channel: %d\n", i, cb->startChannel);
        LogExcess(VB_CHANNELOUT, "Block %3d Channel Count: %d\n", i, cb->channelCount);
        LogExcess(VB_CHANNELOUT, "Block %3d Orientation  : %c\n", i, cb->orientation);
        LogExcess(VB_CHANNELOUT, "Block %3d Start Corner : %s\n", i, cb->startCorner);
        LogExcess(VB_CHANNELOUT, "Block %3d String Count : %d\n", i, cb->stringCount);
        LogExcess(VB_CHANNELOUT, "Block %3d Strand Count : %d\n", i, cb->strandsPerString);
    }
}


bool PixelOverlayManager::loadModelMap() {
    LogDebug(VB_CHANNELOUT, "PixelOverlayManager::loadModelMap()\n");
    
    for (auto a : models) {
        delete a.second;
    }
    models.clear();
    
    FPPChannelMemoryMapControlBlock *cb = NULL;
    
    FILE *fp;
    char filename[1024];
    char buf[64];
    char *s;
    int startChannel;
    int channelCount;
    
    if (!ctrlMap) {
        LogErr(VB_CHANNELOUT, "Error, trying to load memory map data when "
               "memory map is not configured.");
        return false;
    }
    
    strcpy(filename, getMediaDirectory());
    strcat(filename, "/channelmemorymaps");
    
    if (!FileExists(filename)) {
        return true;
    }
    
    LogDebug(VB_CHANNELOUT, "Loading Channel Memory Map data.\n");
    fp = fopen(filename, "r");
    if (fp == NULL) {
        LogErr(VB_CHANNELOUT, "Could not open Channel Memory Map config file %s\n", filename);
        return 0;
    }
    
    cb = (FPPChannelMemoryMapControlBlock*)(ctrlMap +
                                            sizeof(FPPChannelMemoryMapControlHeader));
    while (fgets(buf, 64, fp) != NULL) {
        if (buf[0] == '#') {
            // Allow # comments for testing
            continue;
        }
        
        // Name
        s = strtok(buf, ",");
        std::string modelName = s;
        if (s) {
            strncpy(cb->blockName, s, 32);
        } else {
            continue;
        }
        
        // Start Channel
        s = strtok(NULL, ",");
        if (!s)  continue;
        startChannel = strtol(s, NULL, 10);
        if ((startChannel <= 0) ||
            (startChannel > FPPD_MAX_CHANNELS)) {
            continue;
        }
        cb->startChannel = startChannel;
        
        // Channel Count
        s=strtok(NULL,",");
        if (!s) continue;
        channelCount = strtol(s, NULL, 10);
        if ((channelCount <= 0) ||
            ((startChannel + channelCount) > FPPD_MAX_CHANNELS))
        continue;
        cb->channelCount = channelCount;
        
        // Orientation
        s=strtok(NULL,",");
        if (!s) continue;
        if (!strcmp(s, "vertical")) {
            cb->orientation = 'V';
        } else {
            cb->orientation = 'H';
        }
        
        // Start Corner
        s=strtok(NULL,",");
        if (!s) continue;
        strncpy(cb->startCorner, s, 2);
        
        // String Count
        s=strtok(NULL,",");
        if (!s) continue;
        cb->stringCount = strtol(s, NULL, 10);
        
        // Strands Per String
        s=strtok(NULL,",");
        if (!s) continue;
        cb->strandsPerString = strtol(s, NULL, 10);
        
        // Sanity check our string count
        if (cb->stringCount > (cb->channelCount / 3))
        cb->stringCount = cb->channelCount / 3;
        
        
        SetupPixelMapForBlock(cb);
        
        PixelOverlayModel *model = new PixelOverlayModel(cb, modelName, chanDataMap, pixelMap);
        models[modelName] = model;
        
        cb++;
        ctrlHeader->totalBlocks++;
    }
    fclose(fp);
    
    if ((logLevel >= LOG_INFO) &&
        (logMask & VB_CHANNELOUT)) {
        PrintChannelMapBlocks(ctrlHeader);
    }
    return true;
}

/*
 * Setup the pixel map for this channel block
 */
void PixelOverlayManager::SetupPixelMapForBlock(FPPChannelMemoryMapControlBlock *cb) {
    LogInfo(VB_CHANNELOUT, "Initializing Channel Memory Map Pixel Map '%s'\n", cb->blockName);
    
    if ((!cb->channelCount) ||
        (!cb->strandsPerString) ||
        (!cb->stringCount)) {
        LogErr(VB_CHANNELOUT, "Invalid config for '%s' Memory Map Block\n",
               cb->blockName);
        return;
    }
    
    if ((cb->channelCount % 3) != 0) {
        //        LogInfo(VB_CHANNELOUT, "Memory Map Block '%s' channel count is not divisible by 3\n", cb->blockName);
        //        LogInfo(VB_CHANNELOUT, "unable to configure pixel map array.\n");
        return;
    }
    
    int TtoB = (cb->startCorner[0] == 'T') ? 1 : 0;
    int LtoR = (cb->startCorner[1] == 'L') ? 1 : 0;
    int stringSize = cb->channelCount / 3 / cb->stringCount;
    int width = 0;
    int height = 0;
    
    if (cb->orientation == 'H') {
        // Horizontal Orientation
        width = stringSize / cb->strandsPerString;
        height = cb->channelCount / 3 / width;
        
        int y = 0;
        for (y = 0; y < height; y++) {
            int segment = y % cb->strandsPerString;
            int x = 0;
            for (x = 0; x < width; x++) {
                // Pixel Position in a TL Horizontal wrapping layout
                // 0 1 2
                // 3 4 6
                // 7 8 9
                int ppos = y * width + x;
                // Relative Input Pixel 'R' channel
                int inCh = (cb->startChannel - 1) + (ppos * 3);
                
                // X position in output
                int outX = (LtoR != ((segment % 2) != TtoB)) ? width - x - 1 : x;
                // Y position in output
                int outY = (TtoB) ? y : height - y - 1;
                
                // Relative Mapped Output Pixel 'R' channel
                int mpos = outY * width + outX;
                int outCh = (cb->startChannel - 1) + (mpos * 3);
                
                // Map the pixel's triplet
                pixelMap[inCh    ] = outCh;
                pixelMap[inCh + 1] = outCh + 1;
                pixelMap[inCh + 2] = outCh + 2;
            }
        }
    } else {
        // Vertical Orientation
        height = stringSize / cb->strandsPerString;
        width = cb->channelCount / 3 / height;
        
        int x = 0;
        for (x = 0; x < width; x++) {
            int segment = x % cb->strandsPerString;
            int y = 0;
            for (y = 0; y < height; y++) {
                // Pixel Position in a TL Horizontal wrapping layout
                // 0 1 2
                // 3 4 6
                // 7 8 9
                int ppos = y * width + x;
                // Relative Input Pixel 'R' channel
                int inCh = (cb->startChannel - 1) + (ppos * 3);
                
                // X position in output
                int outX = (LtoR) ? x : width - x - 1;
                // Y position in output
                int outY = (TtoB != ((segment % 2) != LtoR)) ? height - y - 1 : y;
                
                // Relative Mapped Output Pixel 'R' channel
                int mpos = outX * height + outY;
                int outCh = (cb->startChannel - 1) + (mpos * 3);
                
                // Map the pixel's triplet
                pixelMap[inCh    ] = outCh;
                pixelMap[inCh + 1] = outCh + 1;
                pixelMap[inCh + 2] = outCh + 2;
            }
        }
    }
    LogInfo(VB_CHANNELOUT, "Initialization complete for block '%s'\n", cb->blockName);
}


void PixelOverlayManager::OverlayMemoryMap(char *chanData) {
    if ((!ctrlHeader) ||
        (!ctrlHeader->totalBlocks && !ctrlHeader->testMode)) {
        return;
    }
    
    int i = 0;
    FPPChannelMemoryMapControlBlock *cb =
    (FPPChannelMemoryMapControlBlock*)(ctrlMap +
                                       sizeof(FPPChannelMemoryMapControlHeader));
    
    if (ctrlHeader->testMode) {
        memcpy(chanData, chanDataMap, FPPD_MAX_CHANNELS);
    } else {
        for (i = 0; i < ctrlHeader->totalBlocks; i++, cb++) {
            if (cb->isActive == 1) { // Active - Opaque
                memcpy(chanData + cb->startChannel - 1,
                       chanDataMap + cb->startChannel - 1, cb->channelCount);
            } else if (cb->isActive == 2) { // Active - Transparent
                char *src = chanDataMap + cb->startChannel - 1;
                char *dst = chanData + cb->startChannel - 1;
                
                int j = 0;
                for (j = 0; j < cb->channelCount; j++) {
                    if (*src)
                    *dst = *src;
                    src++;
                    dst++;
                }
            } else if (cb->isActive == 3) { // Active - Transparent RGB
                char *src = chanDataMap + cb->startChannel - 1;
                char *dst = chanData + cb->startChannel - 1;
                
                int j = 0;
                for (j = 0; j < cb->channelCount; j += 3) {
                    if (src[0] || src[1] || src[2])
                    {
                        dst[0] = src[0];
                        dst[1] = src[1];
                        dst[2] = src[2];
                    }
                    src += 3;
                    dst += 3;
                }
            }
        }
    }
}


int PixelOverlayManager::UsingMemoryMapInput() {
    if (!ctrlHeader) {
        return 0;
    }
    if (ctrlHeader->testMode) {
        return 1;
    }
    for (auto a : models) {
        if (a.second->getState() != PixelOverlayState::Disabled) {
            return true;
        }
    }
    return false;
}
PixelOverlayModel* PixelOverlayManager::getModel(const std::string &name) {
    return models[name];
}


const httpserver::http_response PixelOverlayManager::render_GET(const httpserver::http_request &req) {
    std::string p1 = req.get_path_pieces()[0];
    int plen = req.get_path_pieces().size();
    if (p1 == "models") {
        Json::Value result;
        if (plen == 1) {
            for (auto & m : models) {
                Json::Value model;
                model["Name"] = m.second->getName();
                model["StartChannel"] = m.second->getStartChannel();
                model["ChannelCount"] = m.second->getChannelCount();
                model["Orientation"] = m.second->isHorizontal() ? "horizontal" : "vertical";
                model["StartCorner"] = m.second->getStartCorner();
                model["StringCount"] = m.second->getNumStrings();
                model["StrandsPerString"] = m.second->getStrandsPerString();
                result.append(model);
            }
        } else {
            std::string model = req.get_path_pieces()[1];
            std::string type;
            if (plen == 3) type = req.get_path_pieces()[2];
            if (type == "data") {
                
            } else {
                auto m = getModel(model);
                if (m) {
                    result["Name"] = m->getName();
                    result["StartChannel"] = m->getStartChannel();
                    result["ChannelCount"] = m->getChannelCount();
                    result["Orientation"] = m->isHorizontal() ? "horizontal" : "vertical";
                    result["StartCorner"] = m->getStartCorner();
                    result["StringCount"] = m->getNumStrings();
                    result["StrandsPerString"] = m->getStrandsPerString();
                }
            }
        }
        Json::FastWriter fastWriter;
        std::string resultStr = fastWriter.write(result);
        
        return httpserver::http_response_builder(resultStr, 200, "application/json").string_response();
    } else if (p1 == "overlays") {
        std::string p2 = req.get_path_pieces()[1];
        if (p2 == "fonts") {
            Json::Value result;
            long unsigned int i = 0;
            char **fonts = MagickQueryFonts("*", &i);
            for (int x = 0; x < i; x++) {
                result.append(fonts[x]);
            }
            free(fonts);
            Json::FastWriter fastWriter;
            std::string resultStr = fastWriter.write(result);
            
            return httpserver::http_response_builder(resultStr, 200, "application/json")
                .string_response();
        } else if (p2 == "data") {
        }
    }
    return httpserver::http_response_builder("Not found: " + p1, 404);
}
const httpserver::http_response PixelOverlayManager::render_POST(const httpserver::http_request &req) {
    std::string p1 = req.get_path_pieces()[0];
    if (p1 == "models") {
        std::string p2 = req.get_path_pieces()[0];
        if (p2 == "raw") {
            //upload of raw file
            char filename[512];
            strcpy(filename, getMediaDirectory());
            strcat(filename, "/channelmemorymaps");
            
            int fp = open(filename, O_CREAT | O_TRUNC | O_RDWR, 0666);
            if (fp == -1) {
                LogErr(VB_CHANNELOUT, "Could not open Channel Memory Map config file %s\n", filename);
                return httpserver::http_response_builder("Could not open Channel Memory Map config file", 500);
            }
            write(fp, req.get_content().c_str(), req.get_content().length());
            close(fp);
            loadModelMap();
            httpserver::http_response_builder("OK", 200);
        }
    }
    return httpserver::http_response_builder("Not found", 404);
}


