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
#include <fstream>
#include <sstream>
#include <string>
#include <chrono>

#include <boost/algorithm/string/replace.hpp>
#include <jsoncpp/json/json.h>

#include <Magick++.h>
#include <magick/type.h>

#include "common.h"
#include "log.h"
#include "PixelOverlay.h"
#include "PixelOverlayControl.h"
#include "Sequence.h"
#include "settings.h"
#include "channeloutput/channeloutputthread.h"

PixelOverlayManager PixelOverlayManager::INSTANCE;

PixelOverlayModel::PixelOverlayModel(FPPChannelMemoryMapControlBlock *b,
                                     const std::string &n,
                                     char         *cdm,
                                     uint32_t     *pm)
    : block(b), name(n), chanDataMap(cdm), pixelMap(pm),
    updateThread(nullptr),threadKeepRunning(false),
    imageData(nullptr), imageDataRows(0), imageDataCols(0)
{
}
PixelOverlayModel::~PixelOverlayModel() {
    while (updateThread) {
        threadKeepRunning = false;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (imageData) {
        free(imageData);
    }
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
    int i = (int)state.getState();
    block->isActive = i;
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
void PixelOverlayModel::setPixelValue(int x, int y, int r, int g, int b) {
    int c = block->startChannel - 1 + (y*getWidth()*3) + x*3;
    chanDataMap[c++] = r;
    chanDataMap[c++] = g;
    chanDataMap[c++] = b;
}

void PixelOverlayModel::doText(const std::string &msg,
                               int r, int g, int b,
                               const std::string &font,
                               int fontSize,
                               bool antialias,
                               const std::string &position,
                               int pixelsPerSecond) {
    while (updateThread) {
        threadKeepRunning = false;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    updateThread = nullptr;

    Magick::Image image(Magick::Geometry(getWidth(),getHeight()), Magick::Color("black"));
    image.quiet(true);
    image.depth(8);
    image.font(font);
    image.fontPointsize(fontSize);
    image.antiAlias(antialias);


    Magick::TypeMetric metrics;
    image.fontTypeMetrics(msg, &metrics);
    
    
    if (position == "Centered" || position == "Center") {
        image.magick("RGB");
        //one shot, just draw the text and return
        double rr = r;
        double rg = g;
        double rb = b;
        rr /= 255.0f;
        rg /= 255.0f;
        rb /= 255.0f;
        
        image.fillColor(Magick::Color(Magick::Color::scaleDoubleToQuantum(rr),
                                      Magick::Color::scaleDoubleToQuantum(rg),
                                      Magick::Color::scaleDoubleToQuantum(rb)));
        image.antiAlias(antialias);
        image.strokeAntiAlias(antialias);
        image.annotate(msg, Magick::CenterGravity);
        Magick::Blob blob;
        image.write( &blob );
        
        setData((uint8_t*)blob.data());
    } else {
        //movement
        double rr = r;
        double rg = g;
        double rb = b;
        rr /= 255.0f;
        rg /= 255.0f;
        rb /= 255.0f;
        
        Magick::Image image2(Magick::Geometry(metrics.textWidth(), metrics.textHeight()), Magick::Color("black"));
        image2.quiet(true);
        image2.depth(8);
        image2.font(font);
        image2.fontPointsize(fontSize);
        image2.antiAlias(antialias);

        image2.fillColor(Magick::Color(Magick::Color::scaleDoubleToQuantum(rr),
                                      Magick::Color::scaleDoubleToQuantum(rg),
                                      Magick::Color::scaleDoubleToQuantum(rb)));
        image2.antiAlias(antialias);
        image2.strokeAntiAlias(antialias);
        image2.annotate(msg, Magick::CenterGravity);

        double y = (getHeight() / 2.0) - (metrics.textHeight() / 2.0);
        double x = (getWidth() / 2.0) - (metrics.textWidth() / 2.0);
        if (position == "R2L") {
            x = getWidth();
        } else if (position == "L2R") {
            x = -metrics.textWidth();
        } else if (position == "B2T") {
            y = getHeight();
        } else if (position == "T2B") {
            y =  -metrics.ascent();
        }
        image2.modifyImage();
        
        const MagickLib::PixelPacket *pixel_cache = image2.getConstPixels(0,0, image2.columns(), image2.rows());
        if (imageData) {
            free(imageData);
        }
        imageData = (uint8_t*)malloc(image2.columns() * image2.rows() * 3);
        imageDataCols = image2.columns();
        imageDataRows = image2.rows();
        for (int yi = 0; yi < imageDataRows; yi++) {
            int idx = yi * imageDataCols;
            int nidx = yi * imageDataCols * 3;

            for (int xi = 0; xi < image2.columns(); xi++) {
                const MagickLib::PixelPacket *ptr2 = &pixel_cache[idx + xi];
                uint8_t *np = &imageData[nidx + (xi*3)];
                
                float r = Magick::Color::scaleQuantumToDouble(ptr2->red);
                float g = Magick::Color::scaleQuantumToDouble(ptr2->green);
                float b = Magick::Color::scaleQuantumToDouble(ptr2->blue);
                r *= 255;
                g *= 255;
                b *= 255;
                np[0] = r;
                np[1] = g;
                np[2] = b;
            }
        }
        copyImageData(x, y);
        lock();
        threadKeepRunning = true;
        updateThread = new std::thread(&PixelOverlayModel::doImageMovementThread, this, position, (int)x, (int)y, pixelsPerSecond);
        
    }
}
void PixelOverlayModel::doImageMovementThread(const std::string direction, int x, int y, int speed) {
    int msDelay = 1000 / speed;
    bool done = false;
    while (threadKeepRunning && !done) {
        std::this_thread::sleep_for(std::chrono::milliseconds(msDelay));
        if (direction == "R2L") {
            --x;
            if (x <= (-imageDataCols)) {
                done = true;
            }
        } else if (direction == "L2R") {
            ++x;
            if (x >= getWidth()) {
                done = true;
            }
        } else if (direction == "B2T") {
            --y;
            if (y <= (-imageDataRows)) {
                done = true;
            }
        } else if (direction == "T2B") {
            ++y;
            if (y >= getHeight()) {
                done = true;
            }
        }
        copyImageData(x, y);
    }
    unlock();
    updateThread = nullptr;
    threadKeepRunning = false;
}


void PixelOverlayModel::copyImageData(int xoff, int yoff) {
    if (imageData) {
        clear();
        int h, w;
        getSize(w, h);
        for (int y = 0; y < imageDataRows; ++y)  {
            int ny = yoff + y;
            if (ny < 0 || ny >= h) {
                continue;
            }
            int idx = y * imageDataCols * 3;
            for (int x = 0; x < imageDataCols; ++x) {
                int nx = xoff + x;
                if (nx < 0 || nx >= w) {
                    continue;
                }
                uint8_t *p = &imageData[idx + (x*3)];
                setPixelValue(nx, ny, p[0], p[1], p[2]);
            }
        }
    }
}



void PixelOverlayModel::getDataJson(Json::Value &v) {
    for (int c = 0; c < block->channelCount; c++) {
        int i = chanDataMap[pixelMap[block->startChannel + c - 1]];
        v.append(i);
    }
}
bool PixelOverlayModel::isLocked() {
    return block->isLocked;
}
void PixelOverlayModel::lock(bool l) {
    block->isLocked = l ? 1 : 0;
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
void PixelOverlayModel::toJson(Json::Value &result) {
    result["Name"] = getName();
    result["StartChannel"] = getStartChannel();
    result["ChannelCount"] = getChannelCount();
    result["Orientation"] = isHorizontal() ? "horizontal" : "vertical";
    result["StartCorner"] = getStartCorner();
    result["StringCount"] = getNumStrings();
    result["StrandsPerString"] = getStrandsPerString();
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
        munmap(pixelMap, FPPD_MAX_CHANNELS * sizeof(uint32_t));
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
    symlink(FPPCHANNELMEMORYMAPDATAFILE, FPPCHANNELMEMORYMAPDATAFILE_OLD);
    symlink(FPPCHANNELMEMORYMAPCTRLFILE, FPPCHANNELMEMORYMAPCTRLFILE_OLD);
    symlink(FPPCHANNELMEMORYMAPPIXELFILE, FPPCHANNELMEMORYMAPPIXELFILE_OLD);

    if (ctrlHeader->totalBlocks) {
        StartChannelOutputThread();
    }
}

bool PixelOverlayManager::createChannelDataMap() {
    LogDebug(VB_CHANNELOUT, "PixelOverlayManager::createChannelDataMap()\n");
    
    mkdir(FPPCHANNELMEMORYMAPPATH, 0777);
    chmod(FPPCHANNELMEMORYMAPPATH, 0777);
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
    uint8_t* tmpData = (uint8_t*)calloc(FPPD_MAX_CHANNELS, sizeof(uint32_t));
    if (write(pixelFD, (void *)tmpData,
              FPPD_MAX_CHANNELS * sizeof(uint32_t)) != (FPPD_MAX_CHANNELS * sizeof(uint32_t))) {
        LogErr(VB_CHANNELOUT, "Error populating %s memory map file: %s\n",
               FPPCHANNELMEMORYMAPPIXELFILE, strerror(errno));
        close(pixelFD);
        free(tmpData);
        return false;
    }
    free(tmpData);
    
    pixelMap = (uint32_t *)mmap(0, FPPD_MAX_CHANNELS * sizeof(uint32_t), PROT_READ|PROT_WRITE, MAP_SHARED, pixelFD, 0);
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


void PixelOverlayManager::loadModelMap() {
    LogDebug(VB_CHANNELOUT, "PixelOverlayManager::loadModelMap()\n");
    
    for (auto a : models) {
        delete a.second;
    }
    models.clear();
    ConvertCMMFileToJSON();
    
    if (!ctrlMap) {
        LogErr(VB_CHANNELOUT, "Error, trying to load memory map data when "
               "memory map is not configured.");
        return;
    }
    
    char filename[1024];
    strcpy(filename, getMediaDirectory());
    strcat(filename, "/config/model-overlays.json");
    if (FileExists(filename)) {
        std::ifstream t(filename);
        std::stringstream buffer;
        
        buffer << t.rdbuf();
        
        std::string config = buffer.str();

        Json::Value root;
        Json::Reader reader;
        bool success = reader.parse(buffer.str(), root);
        if (!success) {
            LogErr(VB_CHANNELOUT, "Error parsing %s\n", filename);
            return;
        }
        const Json::Value models = root["models"];
        FPPChannelMemoryMapControlBlock *cb = NULL;
        cb = (FPPChannelMemoryMapControlBlock*)(ctrlMap +
                                                sizeof(FPPChannelMemoryMapControlHeader));
        for (int c = 0; c < models.size(); c++) {
            std::string modelName = models[c]["Name"].asString();
            strncpy(cb->blockName, modelName.c_str(), 32);
            cb->startChannel = models[c]["StartChannel"].asInt();
            cb->channelCount = models[c]["ChannelCount"].asInt();;
            if (models[c]["Orientation"].asString() == "vertical") {
                cb->orientation = 'V';
            } else {
                cb->orientation = 'H';
            }
            strncpy(cb->startCorner, models[c]["StartCorner"].asString().c_str(), 2);
            cb->stringCount = models[c]["StringCount"].asInt();
            cb->strandsPerString = models[c]["StrandsPerString"].asInt();
            // Sanity check our string count
            if (cb->stringCount > (cb->channelCount / 3)) {
                cb->stringCount = cb->channelCount / 3;
            }
            
            SetupPixelMapForBlock(cb);
            
            PixelOverlayModel *pmodel = new PixelOverlayModel(cb, modelName, chanDataMap, pixelMap);
            this->models[modelName] = pmodel;
            
            cb++;
            ctrlHeader->totalBlocks++;

        }
        if ((logLevel >= LOG_INFO) &&
            (logMask & VB_CHANNELOUT)) {
            PrintChannelMapBlocks(ctrlHeader);
        }
    }
}
void PixelOverlayManager::ConvertCMMFileToJSON() {
    
    FILE *fp;
    char filename[1024];
    char buf[64];
    char *s;
    int startChannel;
    int channelCount;
    
    strcpy(filename, getMediaDirectory());
    strcat(filename, "/channelmemorymaps");
    if (!FileExists(filename)) {
        return;
    }
    Json::Value result;
    
    LogDebug(VB_CHANNELOUT, "Loading Channel Memory Map data.\n");
    fp = fopen(filename, "r");
    if (fp == NULL) {
        LogErr(VB_CHANNELOUT, "Could not open Channel Memory Map config file %s\n", filename);
        return;
    }
    
    while (fgets(buf, 64, fp) != NULL) {
        if (buf[0] == '#') {
            // Allow # comments for testing
            continue;
        }
        Json::Value model;
        
        // Name
        s = strtok(buf, ",");
        if (s) {
            std::string modelName = s;
            model["Name"] = modelName;
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
        model["StartChannel"] = startChannel;
        
        // Channel Count
        s=strtok(NULL,",");
        if (!s) continue;
        channelCount = strtol(s, NULL, 10);
        if ((channelCount <= 0) ||
            ((startChannel + channelCount) > FPPD_MAX_CHANNELS)) {
            continue;
        }
        model["ChannelCount"] = channelCount;
        
        // Orientation
        s=strtok(NULL,",");
        if (!s) continue;
        model["Orientation"] = !strcmp(s, "vertical") ? "vertical" : "horizontal";

        
        // Start Corner
        s=strtok(NULL,",");
        if (!s) continue;
        model["StartCorner"] = s;
        
        
        // String Count
        s=strtok(NULL,",");
        if (!s) continue;
        model["StringCount"] = (int)strtol(s, NULL, 10);
        
        // Strands Per String
        s=strtok(NULL,",");
        if (!s) continue;
        model["StrandsPerString"] = (int)strtol(s, NULL, 10);
        result["models"].append(model);
    }
    fclose(fp);
    remove(filename);
    strcpy(filename, getMediaDirectory());
    strcat(filename, "/config/model-overlays.json");
    Json::StyledWriter writer;
    std::string resultStr = writer.write(result);
    fp = fopen(filename, "w");
    fwrite(resultStr.c_str(), resultStr.length(), 1, fp);
    fclose(fp);
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
    FPPChannelMemoryMapControlBlock *cb =
        (FPPChannelMemoryMapControlBlock*)(ctrlMap +
                                       sizeof(FPPChannelMemoryMapControlHeader));

    for (int i = 0; i < ctrlHeader->totalBlocks; i++, cb++) {
        if (cb->isActive) { // Active
            return 1;
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
        bool empty = true;
        if (plen == 1) {
            std::unique_lock<std::mutex> lock(modelsLock);
            for (auto & m : models) {
                Json::Value model;
                m.second->toJson(model);
                result.append(model);
                empty = false;
            }
        } else {
            std::string model = req.get_path_pieces()[1];
            std::string type;
            std::unique_lock<std::mutex> lock(modelsLock);
            auto m = getModel(model);
            if (m) {
                m->toJson(result);
            } else {
                return httpserver::http_response_builder("Model not found: " + req.get_path_pieces()[1], 404);
            }
        }
        if (empty && plen == 1) {
            return httpserver::http_response_builder("[]", 200, "application/json").string_response();
        } else {
            Json::FastWriter fastWriter;
            std::string resultStr = fastWriter.write(result);
            return httpserver::http_response_builder(resultStr, 200, "application/json").string_response();
        }
    } else if (p1 == "overlays") {
        std::string p2 = req.get_path_pieces()[1];
        std::string p3 = req.get_path_pieces().size() > 2 ? req.get_path_pieces()[2] : "";
        std::string p4 = req.get_path_pieces().size() > 3 ? req.get_path_pieces()[3] : "";
        Json::Value result;
        if (p2 == "fonts") {
            long unsigned int i = 0;
            char **fonts = MagickLib::GetTypeList("*", &i);
            //char **fonts = MagickQueryFonts("*", &i);
            for (int x = 0; x < i; x++) {
                result.append(fonts[x]);
            }
            free(fonts);
            
        } else if (p2 == "models") {
            std::unique_lock<std::mutex> lock(modelsLock);
            for (auto & m : models) {
                Json::Value model;
                m.second->toJson(model);
                model["isActive"] = (int)m.second->getState().getState();
                result.append(model);
            }
        } else if (p2 == "model") {
            std::unique_lock<std::mutex> lock(modelsLock);
            auto m = getModel(p3);
            if (m) {
                if (p4 == "data") {
                    Json::Value data;
                    m->getDataJson(data);
                    result["data"] = data;
                    result["isLocked"] = m->isLocked();
                } else if (p4 == "clear") {
                    m->clear();
                    return httpserver::http_response_builder("OK", 200).string_response();
                } else {
                    m->toJson(result);
                    result["isActive"] = (int)m->getState().getState();
                }
            }
        }
        Json::FastWriter fastWriter;
        std::string resultStr = fastWriter.write(result);
        return httpserver::http_response_builder(resultStr, 200, "application/json")
            .string_response();
    }
    return httpserver::http_response_builder("Not found: " + p1, 404);
}
const httpserver::http_response PixelOverlayManager::render_POST(const httpserver::http_request &req) {
    std::string p1 = req.get_path_pieces()[0];
    if (p1 == "models") {
        std::string p2 = req.get_path_pieces().size() > 1 ? req.get_path_pieces()[1] : "";
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
            std::unique_lock<std::mutex> lock(modelsLock);
            loadModelMap();
            return httpserver::http_response_builder("OK", 200);
        } else if (req.get_path_pieces().size() == 1) {
            char filename[512];
            strcpy(filename, getMediaDirectory());
            strcat(filename, "/config/model-overlays.json");
            
            int fp = open(filename, O_CREAT | O_TRUNC | O_RDWR, 0666);
            if (fp == -1) {
                LogErr(VB_CHANNELOUT, "Could not open Channel Memory Map config file %s\n", filename);
                return httpserver::http_response_builder("Could not open Channel Memory Map config file", 500);
            }
            write(fp, req.get_content().c_str(), req.get_content().length());
            close(fp);
            std::unique_lock<std::mutex> lock(modelsLock);
            loadModelMap();
            return httpserver::http_response_builder("OK", 200);
        }
    }
    return httpserver::http_response_builder("POST Not found " + req.get_path(), 404);
}
const httpserver::http_response PixelOverlayManager::render_PUT(const httpserver::http_request &req) {
    std::string p1 = req.get_path_pieces()[0];
    std::string p2 = req.get_path_pieces().size() > 1 ? req.get_path_pieces()[1] : "";
    std::string p3 = req.get_path_pieces().size() > 2 ? req.get_path_pieces()[2] : "";
    std::string p4 = req.get_path_pieces().size() > 3 ? req.get_path_pieces()[3] : "";
    if (p1 == "overlays") {
        if (p2 == "model") {
            std::unique_lock<std::mutex> lock(modelsLock);
            auto m = getModel(p3);
            if (m) {
                if (p4 == "state") {
                    Json::Value root;
                    Json::Reader reader;
                    if (reader.parse(req.get_content(), root)) {
                        if (root.isMember("State")) {
                            m->setState(PixelOverlayState(root["State"].asInt()));
                            return httpserver::http_response_builder("OK", 200);
                        } else {
                            return httpserver::http_response_builder("Invalid request " + req.get_content(), 500);
                        }
                    } else {
                        return httpserver::http_response_builder("Could not parse request " + req.get_content(), 500);
                    }
                } else if (p4 == "fill") {
                    Json::Value root;
                    Json::Reader reader;
                    if (reader.parse(req.get_content(), root)) {
                        if (root.isMember("RGB")) {
                            m->fill(root["RGB"][0].asInt(),
                                    root["RGB"][1].asInt(),
                                    root["RGB"][2].asInt());
                            return httpserver::http_response_builder("OK", 200);
                        }
                    }
                } else if (p4 == "pixel") {
                    Json::Value root;
                    Json::Reader reader;
                    if (reader.parse(req.get_content(), root)) {
                        if (root.isMember("RGB")) {
                            m->setPixelValue(root["X"].asInt(),
                                             root["Y"].asInt(),
                                             root["RGB"][0].asInt(),
                                             root["RGB"][1].asInt(),
                                             root["RGB"][2].asInt());
                            return httpserver::http_response_builder("OK", 200);
                        }
                    }
                } else if (p4 == "text") {
                    Json::Value root;
                    Json::Reader reader;
                    if (reader.parse(req.get_content(), root)) {
                        if (root.isMember("Message")) {
                            std::string color = root["Color"].asString();
                            if (color[0] == '#') {
                                color = "0x" + color.substr(1);
                            }
                            unsigned int x = std::stoul(color, nullptr, 16);

                            std::string msg = root["Message"].asString();

                            std::string font = root["Font"].asString();
                            std::string position = root["Position"].asString();
                            int fontSize = root["FontSize"].asInt();
                            bool aa = root["AntiAlias"].asBool();
                            int pps = root["PixelsPerSecond"].asInt();
                            m->doText(msg,
                                      (x >> 16) & 0xFF,
                                      (x >> 8) & 0xFF,
                                      x & 0xFF,
                                      font,
                                      fontSize,
                                      aa,
                                      position,
                                      pps);
                            return httpserver::http_response_builder("OK", 200);
                        }
                    }
                }
            }
        }
    }
    return httpserver::http_response_builder("PUT Not found " + req.get_path(), 404);
}



