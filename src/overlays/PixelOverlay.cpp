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

#include <stdio.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <Magick++.h>
#include <magick/type.h>
#include <jsoncpp/json/json.h>
#include <boost/algorithm/string.hpp>

#include <chrono>

#include "effects.h"
#include "channeloutput/channeloutputthread.h"
#include "mqtt.h"
#include "common.h"
#include "settings.h"
#include "Sequence.h"
#include "log.h"
#include "PixelOverlay.h"
#include "PixelOverlayEffects.h"
#include "PixelOverlayModel.h"
#include "commands/Commands.h"

PixelOverlayManager PixelOverlayManager::INSTANCE;

uint32_t PixelOverlayManager::mapColor(const std::string &c) {
    if (c[0] == '#') {
        std::string color = "0x" + c.substr(1);
        return std::stoul(color, nullptr, 0);
    } else if (c == "red") {
        return 0xFF0000;
    } else if (c == "green") {
        return 0x00FF00;
    } else if (c == "blue") {
        return 0x0000FF;
    } else if (c == "yellow") {
        return 0xFFFF00;
    } else if (c == "black") {
        return 0x000000;
    } else if (c == "white") {
        return 0xFFFFFF;
    } else if (c == "cyan") {
        return 0x00FFFF;
    } else if (c == "magenta") {
        return 0xFF00FF;
    } else if (c == "gray") {
        return 0x808080;
    } else if (c == "grey") {
        return 0x808080;
    }
    return std::stoul(c, nullptr, 0);
}


PixelOverlayManager::PixelOverlayManager() {
}
PixelOverlayManager::~PixelOverlayManager() {
    if (updateThread != nullptr) {
        std::unique_lock<std::mutex> l(threadLock);
        threadKeepRunning = false;
        l.unlock();
        threadCV.notify_all();
        updateThread->join();
        delete updateThread;
        updateThread = nullptr;
    }
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

    if (getSettingInt("MQTTHADiscovery", 0))
        SendHomeAssistantDiscoveryConfig();
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
    if (updateThread != nullptr) {
        std::unique_lock<std::mutex> l(threadLock);
        threadKeepRunning = false;
        updates.clear();
        l.unlock();
        threadCV.notify_all();
        updateThread->join();
        delete updateThread;
        updateThread = nullptr;
    }
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
        Json::Value root;
        bool result = LoadJsonFromFile(filename, root);
        if (!result) {
            LogErr(VB_CHANNELOUT, "Error parsing model-overlays.json.");
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
    std::unique_lock<std::mutex> l(threadLock);
    while (!afterOverlayModels.empty()) {
        PixelOverlayModel *m = afterOverlayModels.front();
        afterOverlayModels.pop_front();
        l.unlock();
        m->updateRunningEffects();
        l.lock();
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
    auto a = models.find(name);
    if (a == models.end()) {
        return nullptr;
    }
    return a->second;
}


void PixelOverlayManager::SendHomeAssistantDiscoveryConfig() {
    std::unique_lock<std::mutex> lock(modelsLock);
    for (auto & m : models) {
        Json::Value s;

        s["schema"] = "json";
        s["qos"] = 0;
        s["brightness"] = true;
        s["rgb"] = true;
        s["effect"] = false;

        mqtt->AddHomeAssistantDiscoveryConfig("light", m.second->getName(), s);
    }
}

void PixelOverlayManager::LightMessageHandler(const std::string &topic, const std::string &payload) {
    static Json::Value cache;
    std::vector<std::string> parts = split(topic, '/'); // "/light/ModelName/cmd"

    std::string model = parts[2];
    auto m = getModel(model);

    if ((parts[3] == "config") && (!m) && (!payload.empty())) {
        mqtt->RemoveHomeAssistantDiscoveryConfig("light", model);
        return;
    }

    if (parts[3] != "cmd")
        return;

    Json::Value s = LoadJsonFromString(payload);
    if (m) {
        std::unique_lock<std::mutex> lock(modelsLock);
        std::string newState = boost::algorithm::to_upper_copy(s["state"].asString());

        if (newState == "OFF") {
            m->fill(0, 0, 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            m->setState(PixelOverlayState("Disabled"));

            if (cache.isMember(model)) {
                s = cache[model];
                s["state"] = "OFF";
            }
        } else if (newState != "ON") {
            return;
        } else {
            if (!s.isMember("color")) {
                if (cache.isMember(model)) {
                    s["color"] = cache[model]["color"];
                } else {
                    Json::Value c;
                    c["r"] = 255;
                    c["g"] = 255;
                    c["b"] = 255;
                    s["color"] = c;
                }
            }

            if (!s.isMember("brightness")) {
                if (cache.isMember(model)) {
                    s["brightness"] = cache[model]["brightness"];
                } else {
                    s["brightness"] = 255;
                }
            }

            int brightness = s["brightness"].asInt();
            int r = (int)(1.0 * s["color"]["r"].asInt() * brightness / 255);
            int g = (int)(1.0 * s["color"]["g"].asInt() * brightness / 255);
            int b = (int)(1.0 * s["color"]["b"].asInt() * brightness / 255);

            m->fill(r, g, b);

            m->setState(PixelOverlayState("Enabled"));
        }

        cache[model] = s;

        lock.unlock();

        Json::StreamWriterBuilder wbuilder;
        wbuilder["indentation"] = "";
        std::string state = Json::writeString(wbuilder, s);
        //std::string state = SaveJsonToString(js);

        std::string topic = "light/";
        topic += model;
        topic += "/state";
        mqtt->Publish(topic, state);
    }
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
        } else if (p2 == "effects") {
            if (p3 == "") {
                for (auto &a : PixelOverlayEffect::GetPixelOverlayEffects()) {
                    result.append(a);
                }
            } else {
                PixelOverlayEffect *e = PixelOverlayEffect::GetPixelOverlayEffect(p3);
                if (e) {
                    result = e->getDescription();
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
                            unsigned int x = mapColor(color);

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
        args.push_back(CommandArg("State", "string", "State").setContentList({"Don't Set", "Enabled", "Transparent", "TransparentRGB"}));
        args.push_back(CommandArg("Color", "color", "Color").setDefaultValue("#FF0000"));
    }
    
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override {
        if (args.size() <  2 || args.size() > 3) {
            return std::make_unique<Command::ErrorResult>("Command needs 2 or 3 arguments, found " + std::to_string(args.size()));
        }
        std::unique_lock<std::mutex> lock(getLock());
        auto m = manager->getModel(args[0]);
        if (m) {
            std::string color;
            std::string state = "Don't Set";
            if (args.size() == 2) {
                color = args[1];
            } else {
                state = args[1];
                color = args[2];
            }
            if (state != "Don't Set") {
                m->setState(PixelOverlayState(state));
            }
            unsigned int x = PixelOverlayManager::mapColor(color);
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
            unsigned int cint = PixelOverlayManager::mapColor(color);
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

class ApplyEffectOverlayCommand : public OverlayCommand {
public:
    ApplyEffectOverlayCommand(PixelOverlayManager *m) : OverlayCommand("Overlay Model Effect", m) {
        args.push_back(CommandArg("Model", "string", "Model").setContentListUrl("api/models?simple=true", false));
        args.push_back(CommandArg("AutoEnable", "bool", "Auto Enable/Disable").setDefaultValue("false"));
        args.push_back(CommandArg("Effect", "subcommand", "Effect").setContentListUrl("api/overlays/effects/", false));
    }
    
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override {
        if (args.size() < 3) {
            return std::make_unique<Command::ErrorResult>("Command needs at least 3 arguments, found " + std::to_string(args.size()));
        }
        std::unique_lock<std::mutex> lock(getLock());
        auto m = manager->getModel(args[0]);
        if (m) {
            bool autoEnable = (args[1] == "true" || args[1] == "1");
            std::string effect = args[2];
            std::vector<std::string> newArgs(args.begin() + 3, args.end());

            if (m->applyEffect(autoEnable, effect, newArgs)) {
                return std::make_unique<Command::Result>("Model Effect Started");
            }
            return std::make_unique<Command::ErrorResult>("Could not start effect: " + effect);
        }
        return std::make_unique<Command::ErrorResult>("No model found: " + args[0]);
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
    CommandManager::INSTANCE.addCommand(new ApplyEffectOverlayCommand(this));
}

void PixelOverlayManager::doOverlayModelEffects() {
    std::unique_lock<std::mutex> l(threadLock);
    while (threadKeepRunning) {
        uint32_t waitTime = 1000;
        if (!updates.empty()) {
            uint64_t curTime = GetTimeMS();
            while (!updates.empty() && updates.begin()->first <= curTime) {
                std::list<PixelOverlayModel*> models = updates.begin()->second;
                uint64_t startTime = updates.begin()->first;
                updates.erase(updates.begin());
                l.unlock();
                
                for (auto m : models) {
                    int32_t ms = m->updateRunningEffects();
                    if (ms != 0) {
                        l.lock();
                        if (ms > 0) {
                            uint64_t t = startTime + ms;
                            updates[t].push_back(m);
                        } else {
                            afterOverlayModels.push_back(m);
                        }
                        l.unlock();
                    }
                }
                l.lock();
                curTime = GetTimeMS();
            }
            if (!updates.empty()) {
                waitTime = updates.begin()->first - curTime;
            }
        }
        threadCV.wait_for(l, std::chrono::milliseconds(waitTime));
    }
}
void PixelOverlayManager::removePeriodicUpdate(PixelOverlayModel*m) {
    std::unique_lock<std::mutex> l(threadLock);
    for (auto &a : updates) {
        updates.begin()->second.remove(m);
    }
    afterOverlayModels.remove(m);
}

void PixelOverlayManager::addPeriodicUpdate(int32_t initialDelayMS, PixelOverlayModel*m) {
    std::unique_lock<std::mutex> l(threadLock);
    if (updateThread == nullptr) {
        threadKeepRunning = true;
        updateThread = new std::thread(&PixelOverlayManager::doOverlayModelEffects, this);
    }
    if (initialDelayMS > 0) {
        uint64_t nextTime = GetTimeMS();
        nextTime += initialDelayMS;
        updates[nextTime].push_back(m);
    } else {
        afterOverlayModels.push_back(m);
    }
    l.unlock();
    threadCV.notify_all();
}
