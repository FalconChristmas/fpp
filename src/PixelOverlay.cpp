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

#include <dirent.h>
#include <dlfcn.h>
#include <sys/stat.h>

#include <boost/algorithm/string/replace.hpp>
#include <jsoncpp/json/json.h>

#include <Magick++.h>
#include <magick/type.h>

#include "common.h"
#include "effects.h"
#include "log.h"
#include "PixelOverlay.h"
#include "PixelOverlayControl.h"
#include "Sequence.h"
#include "settings.h"
#include "channeloutput/channeloutputthread.h"
#include "commands/Commands.h"

PixelOverlayManager PixelOverlayManager::INSTANCE;

PixelOverlayModel::PixelOverlayModel(FPPChannelMemoryMapControlBlock *b,
                                     const std::string &n,
                                     char         *cdm,
                                     uint32_t     *pm)
    : block(b), name(n), chanDataMap(cdm), pixelMap(pm),
    updateThread(nullptr),threadKeepRunning(false), overlayBuffer(nullptr),
    imageData(nullptr), imageDataRows(0), imageDataCols(0)
{
}
PixelOverlayModel::~PixelOverlayModel() {
    if (updateThread) {
        threadKeepRunning = false;
        updateThread->join();
        delete updateThread;
    }
    if (imageData) {
        free(imageData);
    }
    if (overlayBuffer) {
        free(overlayBuffer);
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
        chanDataMap[pixelMap[c++]] = r;
        chanDataMap[pixelMap[c++]] = g;
        chanDataMap[pixelMap[c++]] = b;
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
        chanDataMap[pixelMap[c]] = value;
    }
}
void PixelOverlayModel::setPixelValue(int x, int y, int r, int g, int b) {
    int c = block->startChannel - 1 + (y*getWidth()*3) + x*3;
    chanDataMap[pixelMap[c++]] = r;
    chanDataMap[pixelMap[c++]] = g;
    chanDataMap[pixelMap[c++]] = b;
}

void PixelOverlayModel::doText(const std::string &msg,
                               int r, int g, int b,
                               const std::string &font,
                               int fontSize,
                               bool antialias,
                               const std::string &position,
                               int pixelsPerSecond,
                               bool autoEnable) {
    if (updateThread) {
        threadKeepRunning = false;
        updateThread->join();
        delete updateThread;
    }
    updateThread = nullptr;

    Magick::Image image(Magick::Geometry(getWidth(),getHeight()), Magick::Color("black"));
    image.quiet(true);
    image.depth(8);
    image.font(font);
    image.fontPointsize(fontSize);
    image.antiAlias(antialias);

    bool disableWhenDone = false;
    if ((autoEnable) && (getState().getState() == PixelOverlayState::PixelState::Disabled))
    {
        setState(PixelOverlayState(PixelOverlayState::PixelState::Enabled));
        disableWhenDone = true;
    }
    
    int maxWid = 0;
    int totalHi = 0;
    
    int lines = 1;
    int last = 0;
    for (int x = 0; x < msg.length(); x++) {
        if (msg[x] == '\n' || ((x < msg.length() - 1) && msg[x] == '\\' && msg[x+1] == 'n')) {
            lines++;
            std::string newM = msg.substr(last, x);
            Magick::TypeMetric metrics;
            image.fontTypeMetrics(newM, &metrics);
            maxWid = std::max(maxWid,  (int)metrics.textWidth());
            totalHi += (int)metrics.textHeight();
            if (msg[x] == '\n') {
                last = x + 1;
            } else {
                last = x + 2;
            }
        }
    }
    std::string newM = msg.substr(last);
    Magick::TypeMetric metrics;
    image.fontTypeMetrics(newM, &metrics);
    maxWid = std::max(maxWid,  (int)metrics.textWidth());
    totalHi += (int)metrics.textHeight();
    
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

        if (disableWhenDone)
            setState(PixelOverlayState(PixelOverlayState::PixelState::Disabled));
    } else {
        //movement
        double rr = r;
        double rg = g;
        double rb = b;
        rr /= 255.0f;
        rg /= 255.0f;
        rb /= 255.0f;
                
        Magick::Image image2(Magick::Geometry(maxWid, totalHi), Magick::Color("black"));
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

        double y = (getHeight() / 2.0) - ((totalHi) / 2.0);
        double x = (getWidth() / 2.0) - (maxWid / 2.0);
        if (position == "R2L") {
            x = getWidth();
        } else if (position == "L2R") {
            x = -maxWid;
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

        if (!overlayBuffer)
            overlayBuffer = (uint8_t*)malloc(block->channelCount);

        copyImageData(x, y);
        lock();
        threadKeepRunning = true;
        updateThread = new std::thread(&PixelOverlayModel::doImageMovementThread, this, position, (int)x, (int)y, pixelsPerSecond, disableWhenDone);
    }
}
void PixelOverlayModel::doImageMovementThread(const std::string &direction, int x, int y, int speed, bool disableWhenDone) {
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
    if (disableWhenDone)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        setState(PixelOverlayState(PixelOverlayState::PixelState::Disabled));
    }

    unlock();
}


void PixelOverlayModel::copyImageData(int xoff, int yoff) {
    if (imageData) {
        memset(overlayBuffer, 0, block->channelCount);
        int h, w;
        getSize(w, h);
        for (int y = 0; y < imageDataRows; ++y)  {
            int ny = yoff + y;
            if (ny < 0 || ny >= h) {
                continue;
            }

            uint8_t *src = imageData + (y * imageDataCols * 3);
            uint8_t *dst = overlayBuffer + (ny * w * 3);
            int pixelsToCopy = imageDataCols;
            int c = w * 3;

            if (xoff < 0) {
                src -= xoff * 3;
                pixelsToCopy += xoff;
                if (pixelsToCopy >= w)
                    pixelsToCopy = w;
            } else if (xoff > 0) {
                dst += xoff * 3;
                if (pixelsToCopy > (w - xoff))
                    pixelsToCopy = w - xoff;
            } else {
                if (pixelsToCopy >= w)
                    pixelsToCopy = w;
            }

            if (pixelsToCopy > 0)
                memcpy(dst, src, pixelsToCopy * 3);
        }
        setData(overlayBuffer);
    }
}



void PixelOverlayModel::getDataJson(Json::Value &v) {
    for (int c = 0; c < block->channelCount; c++) {
        unsigned char i = chanDataMap[pixelMap[block->startChannel + c - 1]];
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

    RegisterCommands();

    if (ctrlHeader->totalBlocks && !getRestarted()) {
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
    
    int channelsPerNode = 3;
    if (cb->channelCount == cb->stringCount) {
        //single channel model.. we really don't support this, but let's not crash
        channelsPerNode = 1;
    }
    int stringSize = cb->channelCount / channelsPerNode / cb->stringCount;
    if (stringSize < 1) {
        stringSize = 1;
    }
    int width = 0;
    int height = 0;
    
    if (cb->orientation == 'H') {
        // Horizontal Orientation
        width = stringSize / cb->strandsPerString;
        if (width == 0) {
            width = 1;
        }
        height = cb->channelCount / channelsPerNode / width;
        
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
                int outX = (LtoR == (segment % 2)) ? width - x - 1 : x;
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
        if (height == 0) {
            height = 1;
        }
        width = cb->channelCount / channelsPerNode / height;
        
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
                int outY = (TtoB == (segment % 2)) ? height - y - 1 : y;
                
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
            int active = cb->isActive;

            if (((active == 2) || (active == 3)) &&
                (!IsEffectRunning()) &&
                (!sequence->IsSequenceRunning())) {
                active = 1;
            }

            if (active == 1) { // Active - Opaque
                memcpy(chanData + cb->startChannel - 1,
                       chanDataMap + cb->startChannel - 1, cb->channelCount);
            } else if (active == 2) { // Active - Transparent
                char *src = chanDataMap + cb->startChannel - 1;
                char *dst = chanData + cb->startChannel - 1;
                
                int j = 0;
                for (j = 0; j < cb->channelCount; j++) {
                    if (*src)
                    *dst = *src;
                    src++;
                    dst++;
                }
            } else if (active == 3) { // Active - Transparent RGB
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




static bool isTTF(const std::string &mainStr)
{
    return (mainStr.size() >= 4) && (mainStr.compare(mainStr.size() - 4, 4, ".ttf") == 0);
}
static void findFonts(const std::string &dir, std::map<std::string, std::string> &fonts) {
    DIR *dp;
    struct dirent *ep;
    
    dp = opendir(dir.c_str());
    if (dp != NULL) {
        while (ep = readdir(dp)) {
            int location = strstr(ep->d_name, ".") - ep->d_name;
            // We're one of ".", "..", or hidden, so let's skip
            if (location == 0) {
                continue;
            }
            
            struct stat statbuf;
            std::string dname = dir;
            dname += ep->d_name;
            lstat(dname.c_str(), &statbuf);
            if (S_ISLNK(statbuf.st_mode)) {
                //symlink, skip
                continue;
            } else if (S_ISDIR(statbuf.st_mode)) {
                findFonts(dname + "/", fonts);
            } else if (isTTF(ep->d_name)) {
                std::string fname = ep->d_name;
                fname.resize(fname.size() - 4);
                fonts[fname] = dname;
            }
        }
        closedir(dp);
    }
}

void PixelOverlayManager::loadFonts() {
    if (!fontsLoaded) {
        long unsigned int i = 0;
        char **mlfonts = MagickLib::GetTypeList("*", &i);
        for (int x = 0; x < i; x++) {
            fonts[mlfonts[x]] = "";
            free(mlfonts[x]);
        }
        findFonts("/usr/share/fonts/truetype/", fonts);
        findFonts("/usr/local/share/fonts/", fonts);
        free(mlfonts);
        fontsLoaded = true;
    }
}

const httpserver::http_response PixelOverlayManager::render_GET(const httpserver::http_request &req) {
    std::string p1 = req.get_path_pieces()[0];
    int plen = req.get_path_pieces().size();
    if (p1 == "models") {
        Json::Value result;
        bool empty = true;
        if (plen == 1) {
            std::unique_lock<std::mutex> lock(modelsLock);
            bool simple = false;
            if (req.get_arg("simple") == "true") {
                simple = true;
            }
            for (auto & m : models) {
                if (simple) {
                    result.append(m.second->getName());
                    empty = false;
                } else {
                    Json::Value model;
                    m.second->toJson(model);
                    result.append(model);
                    empty = false;
                }
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
            loadFonts();
            for (auto & a : fonts) {
                result.append(a.first);
            }
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
                    loadFonts();
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
                            bool autoEnable = false;
                            if (root.isMember("AutoEnable")) {
                                autoEnable = root["AutoEnable"].asBool();
                            }
                            
                            std::string f = fonts[font];
                            if (f == "") {
                                f = font;
                            }
                            
                            m->doText(msg,
                                      (x >> 16) & 0xFF,
                                      (x >> 8) & 0xFF,
                                      x & 0xFF,
                                      f,
                                      fontSize,
                                      aa,
                                      position,
                                      pps,
                                      autoEnable);
                            return httpserver::http_response_builder("OK", 200);
                        }
                    }
                }
            }
        }
    }
    return httpserver::http_response_builder("PUT Not found " + req.get_path(), 404);
}

const std::string &PixelOverlayManager::mapFont(const std::string &f) {
    if (fonts[f] != "") {
        return fonts[f];
    }
    return f;
}


class OverlayCommand : public Command {
public:
    OverlayCommand(const std::string &s, PixelOverlayManager *m) : Command(s), manager(m) {}
    virtual ~OverlayCommand() {}
    
    
    std::mutex &getLock() { return manager->modelsLock;}
    PixelOverlayManager *manager;
};


class EnableOverlayCommand : public OverlayCommand {
public:
    EnableOverlayCommand(PixelOverlayManager *m) : OverlayCommand("Overlay Model State", m) {
        args.push_back(CommandArg("Model", "string", "Model").setContentListUrl("api/models?simple=true", false));
        args.push_back(CommandArg("State", "string", "State").setContentList({"Disabled", "Enabled", "Transparent", "TransparentRGB"}));
    }
    
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override {
        if (args.size() != 2) {
            return std::make_unique<Command::ErrorResult>("Command needs 2 arguments, found " + std::to_string(args.size()));
        }
        std::unique_lock<std::mutex> lock(getLock());
        auto m = manager->getModel(args[0]);
        if (m) {
            m->setState(PixelOverlayState(args[1]));
            return std::make_unique<Command::Result>("Model State Set");
        }
        return std::make_unique<Command::ErrorResult>("No model found: " + args[0]);
    }
};
class ClearOverlayCommand : public OverlayCommand {
public:
    ClearOverlayCommand(PixelOverlayManager *m) : OverlayCommand("Overlay Model Clear", m) {
        args.push_back(CommandArg("Model", "string", "Model").setContentListUrl("api/models?simple=true", false));
    }
    
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override {
        if (args.size() != 1) {
            return std::make_unique<Command::ErrorResult>("Command needs 1 argument, found " + std::to_string(args.size()));
        }
        std::unique_lock<std::mutex> lock(getLock());
        auto m = manager->getModel(args[0]);
        if (m) {
            m->clear();
            return std::make_unique<Command::Result>("Model Cleared");
        }
        return std::make_unique<Command::ErrorResult>("No model found: " + args[0]);
    }
};
class FillOverlayCommand : public OverlayCommand {
public:
    FillOverlayCommand(PixelOverlayManager *m) : OverlayCommand("Overlay Model Fill", m) {
        args.push_back(CommandArg("Model", "string", "Model").setContentListUrl("api/models?simple=true", false));
        args.push_back(CommandArg("Color", "color", "Color").setDefaultValue("#FF0000"));
    }
    
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override {
        if (args.size() != 2) {
            return std::make_unique<Command::ErrorResult>("Command needs 2 arguments, found " + std::to_string(args.size()));
        }
        std::unique_lock<std::mutex> lock(getLock());
        auto m = manager->getModel(args[0]);
        if (m) {
            std::string color = args[1];
            if (color[0] == '#') {
                color = "0x" + color.substr(1);
            
            }
            unsigned int x = std::stoul(color, nullptr, 16);
            m->fill((x >> 16) & 0xFF,
                    (x >> 8) & 0xFF,
                    x & 0xFF);
            return std::make_unique<Command::Result>("Model Filled");
        }
        return std::make_unique<Command::ErrorResult>("No model found: " + args[0]);
    }
};
class TextOverlayCommand : public OverlayCommand {
public:
    TextOverlayCommand(PixelOverlayManager *m) : OverlayCommand("Overlay Model Text", m) {
        args.push_back(CommandArg("Model", "string", "Model").setContentListUrl("api/models?simple=true", false));
        args.push_back(CommandArg("Color", "color", "Color").setDefaultValue("#FF0000"));
        args.push_back(CommandArg("Font", "string", "Font").setContentListUrl("api/overlays/fonts", false));
        args.push_back(CommandArg("FontSize", "int", "FontSize").setRange(4, 100).setDefaultValue("18"));
        args.push_back(CommandArg("FontAntiAlias", "bool", "Anti-Aliased").setDefaultValue("false"));
        args.push_back(CommandArg("Position", "string", "Position").setContentList({"Center", "Right to Left", "Left to Right", "Bottom to Top", "Top to Bottom"}));
        args.push_back(CommandArg("Speed", "int", "Scroll Speed").setRange(0, 200).setDefaultValue("10"));
        args.push_back(CommandArg("AutoEnable", "bool", "Auto Enable/Disable Model").setDefaultValue("false"));
        
        // keep text as last argument if possible as the MQTT commands will, by default, use the payload of the mqtt
        // msg as the last argument.  Thus, this allows all of the above to be topic paths, but the text to be
        // sent in the payload
        args.push_back(CommandArg("Text", "string", "Text").setAdjustable());
    }
    
    const std::string mapPosition(const std::string &p) {
        if (p == "Center") {
            return p;
        } else if (p == "Right to Left") {
            return "R2L";
        } else if (p == "Left to Right") {
            return "L2R";
        } else if (p == "Bottom to Top") {
            return "B2T";
        } else if (p == "Top to Bottom") {
            return "T2B";
        }
        return "Center";
    }
    
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override {
        if (args.size() < 8) {
            return std::make_unique<Command::ErrorResult>("Command needs 9 arguments, found " + std::to_string(args.size()));
        }
        std::unique_lock<std::mutex> lock(getLock());
        auto m = manager->getModel(args[0]);
        if (m) {
            std::string color = args[1];
            if (color[0] == '#') {
                color = "0x" + color.substr(1);
            
            }
            unsigned int cint = std::stoul(color, nullptr, 16);
            std::string font = manager->mapFont(args[2]);
            int fontSize = std::atoi(args[3].c_str());
            if (fontSize < 4) {
                fontSize = 12;
            }
            bool aa = args[4] == "true" || args[4] == "1";
            std::string position = mapPosition(args[5]);
            int pps = std::atoi(args[6].c_str());
            
            std::string msg = args[7];
            bool autoEnable = false;
            if (args.size() == 9) {
                autoEnable = (msg == "true" || msg == "1");
                msg = args[8];
            }
            
            m->doText(msg,
                      (cint >> 16) & 0xFF,
                      (cint >> 8) & 0xFF,
                      cint & 0xFF,
                      font,
                      fontSize,
                      aa,
                      position,
                      pps,
                      autoEnable);
        }
        return std::make_unique<Command::Result>("Model Filled");
    }
};

void PixelOverlayManager::RegisterCommands() {
    if (models.empty()) {
        return;
    }
    
    CommandManager::INSTANCE.addCommand(new EnableOverlayCommand(this));
    CommandManager::INSTANCE.addCommand(new FillOverlayCommand(this));
    CommandManager::INSTANCE.addCommand(new TextOverlayCommand(this));
    CommandManager::INSTANCE.addCommand(new ClearOverlayCommand(this));
}
