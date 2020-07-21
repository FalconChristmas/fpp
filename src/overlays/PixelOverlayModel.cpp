/*
*   Pixel Overlay Models for Falcon Player (FPP)
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

#include "fpp-pch.h"

#include <sys/mman.h>
#include <fcntl.h>

#include "channeloutput/channeloutputthread.h"
#include "effects.h"
#include "Plugins.h"
#include "PixelOverlayModel.h"
#include "PixelOverlay.h"
#include "PixelOverlayEffects.h"


static uint8_t* createChannelDataMemory(const std::string &dataName, uint32_t size) {
    mode_t mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH;
    int f = shm_open(dataName.c_str(), O_RDWR | O_CREAT, mode);
    ftruncate(f, size);
    uint8_t *channelData = (uint8_t*)mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED, f, 0);
    memset(channelData, 0, size);
    close(f);
    return channelData;
}

PixelOverlayModel::PixelOverlayModel(const Json::Value &c)
    : config(c), overlayBufferData(nullptr), channelData(nullptr), runningEffect(nullptr)
{
    name = config["Name"].asString();
    startChannel = config["StartChannel"].asInt();
    startChannel--; //need to be 0 based
    channelCount = config["ChannelCount"].asInt();
    int strings = config["StringCount"].asInt();
    int sps = config["StrandsPerString"].asInt();
    
    std::string orientation = config["Orientation"].asString();
    std::string startCorner = config["StartCorner"].asString();
    
    bool TtoB = (startCorner[0] == 'T');
    bool LtoR = (startCorner[1] == 'L');

    channelsPerNode = 3;
    if (config.isMember("ChannelCountPerNode")) {
        channelsPerNode = config["ChannelCountPerNode"].asInt();
    }

    height = strings * sps;
    if (height == 0) {
        height = 1;
    }
    width = channelCount / channelsPerNode;
    width /= height;
    if (width == 0) {
        width = 1;
    }
    
    std::string dataName = "/FPP-Model-Data-" + name;
    channelData = createChannelDataMemory(dataName, channelCount);

    if (orientation == "V" || orientation == "vertical") {
        channelMap.resize(width*height*3);

        std::swap(width, height);
        for (int x = 0; x < width; x++) {
            int segment = x % sps;
            for (int y = 0; y < height; y++) {
                //pos in the normal overlay buffer
                int ppos = y * width + x;
                // Relative Input Pixel 'R' channel
                int inCh = (ppos * 3);
                
                // X position in output
                int outX = (LtoR) ? x : width - x - 1;
                // Y position in output
                int outY = (TtoB == (segment % 2)) ? height - y - 1 : y;
                
                // Relative Mapped Output Pixel 'R' channel
                int mpos = outX * height + outY;
                int outCh = (mpos * channelsPerNode);
                
                // Map the pixel's triplet
                for (int cho = 0; cho < 3; cho++) {
                    if (cho < channelsPerNode) {
                        channelMap[inCh + cho] = outCh + cho;
                    } else {
                        channelMap[inCh + cho] = FPPD_OFF_CHANNEL;
                    }
                }
            }
        }
    } else if (orientation == "C" || orientation == "custom") {
        std::string customData = ",;,";
        if (config.isMember("data")) {
            customData = config["data"].asString();
        }
        std::vector<std::string> layers = split(customData, '|');
        std::vector<std::vector<std::string>> allData;
        for (auto&layer : layers) {
            std::vector<std::string> lines = split(layer, ';');
            for (auto &l : lines) {
                allData.push_back(split(l, ','));
            }
            lines.clear();
        }
        width = 1;
        height = allData.size();
        for (auto &l : allData) {
            width = std::max(width, (int)l.size());
        }
        if (height < 1) {
            height = 1;
        }
        channelMap.resize(width*height*3);
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int node = -1;
                if (y < allData.size() && x < allData[y].size()) {
                    std::string &s = allData[y][x];
                    if (s == "") {
                        node = -1;
                    } else {
                        node = std::stoi(s);
                    }
                }
                int inChan = node * channelsPerNode;
                
                for (int cho = 0; cho < 3; cho++) {
                    if (node != -1 && cho < channelsPerNode) {
                        channelMap[y*3 + x + cho] = node * channelsPerNode + cho;
                    } else {
                        channelMap[y*3 + x + cho] = FPPD_OFF_CHANNEL;
                    }
                }
            }
        }
    } else {
        channelMap.resize(width*height*3);
        for (int y = 0; y < height; y++) {
            int segment = y % sps;
            for (int x = 0; x < width; x++) {
                //pos in the normal overlay buffer
                int ppos = y * width + x;
                // Input Pixel 'R' channel
                int inCh = (ppos * 3);
                
                // X position in output
                int outX = (LtoR == (segment % 2)) ? width - x - 1 : x;
                // Y position in output
                int outY = (TtoB) ? y : height - y - 1;
                
                // Relative Mapped Output Pixel 'R' channel
                int mpos = outY * width + outX;
                int outCh = (mpos * channelsPerNode);
                
                // Map the pixel's triplet
                for (int cho = 0; cho < 3; cho++) {
                    if (cho < channelsPerNode) {
                        channelMap[inCh + cho] = outCh + cho;
                    } else {
                        channelMap[inCh + cho] = FPPD_OFF_CHANNEL;
                    }
                }
            }
        }
    }
}
PixelOverlayModel::~PixelOverlayModel() {
    if (channelData) {
        munmap(channelData, channelCount);
        std::string dataName = "/FPP-Model-Data-" + name;
        shm_unlink(dataName.c_str());
    }
    if (overlayBufferData) {
        munmap(overlayBufferData, width * height * 3 + sizeof(OverlayBufferData));
        std::string overlayBufferName = "/FPP-Model-Overlay-Buffer-" + name;
        shm_unlink(overlayBufferName.c_str());
    }
}

PixelOverlayState PixelOverlayModel::getState() const {
    return state;
}

void PixelOverlayModel::setState(const PixelOverlayState &st) {
    if (st != state) {
        PixelOverlayState old = state;
        state = st;
        PixelOverlayManager::INSTANCE.modelStateChanged(this, old, state);
    }
}
void PixelOverlayModel::doOverlay(uint8_t *channels) {
    if (overlayBufferData && (overlayBufferData->flags & 0x1)) {
        //overlay buffer is dirty and needs to be flushed
        flushOverlayBuffer();
    }
    
    int st = state.getState();
    if (((st == 2) || (st == 3)) &&
        (!IsEffectRunning()) &&
        (!sequence->IsSequenceRunning()) &&
        !PluginManager::INSTANCE.hasPlugins()) {
        //there is nothing running that we would be overlaying so do a straight copy
        st = 1;
    }
    
    uint8_t *src = channelData;
    uint8_t *dst = &channels[startChannel];
    switch (st) {
        case 1: //Active - Opaque
            memcpy(dst, src, channelCount);
            break;
        case 2: //Active Transparent
            for (int j = 0; j < channelCount; j++, src++, dst++) {
                if (*src) {
                    *dst = *src;
                }
            }
            break;
        case 3: //Active Transparent RGB {
            for (int j = 0; j < channelCount; j += 3, src += 3, dst += 3) {
                if (src[0] || src[1] || src[2]) {
                    dst[0] = src[0];
                    dst[1] = src[1];
                    dst[2] = src[2];
                }
            }
            break;
    }
}

void PixelOverlayModel::setData(const uint8_t *data) {
    for (int c = 0; c < (width*height*3); c++) {
        if (channelMap[c] != FPPD_OFF_CHANNEL) {
            channelData[channelMap[c]] = data[c];
        }
    }
}

void PixelOverlayModel::setValue(uint8_t value, int startChannel, int endChannel) {
    int start;
    int end;
    
    start = startChannel - this->startChannel;
    if (startChannel == -1) {
        start = 0;
    }
    if (start > channelCount) {
        return;
    }
    
    end = endChannel - startChannel + 1;
    if (end >= channelCount) {
        end = channelCount - 1;
    }
    for (int c = start; c <= end; c++) {
        if (channelMap[c] != FPPD_OFF_CHANNEL) {
            channelData[channelMap[c]] = value;
        }
    }
}
void PixelOverlayModel::setPixelValue(int x, int y, int r, int g, int b) {
    int c = (y*getWidth()*3) + x*3;
    if (channelMap[c] != FPPD_OFF_CHANNEL) {
        channelData[channelMap[c++]] = r;
    }
    if (channelsPerNode > 1 && channelMap[c] != FPPD_OFF_CHANNEL) {
        channelData[channelMap[c++]] = g;
    }
    if (channelsPerNode > 2 && channelMap[c] != FPPD_OFF_CHANNEL) {
        channelData[channelMap[c++]] = b;
    }
}
void PixelOverlayModel::getDataJson(Json::Value &v) {
    for (int c = 0; c < height*width*3; c++) {
        unsigned char i = 0;
        if (channelMap[c] != FPPD_OFF_CHANNEL) {
            i = channelData[channelMap[c]];
        }
        v.append(i);
    }
}


uint8_t *PixelOverlayModel::getOverlayBuffer() {
    if (!overlayBufferData) {
        std::string overlayBuferName = "/FPP-Model-Overlay-Buffer-" + name;
        mode_t mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH;
        int f = shm_open(overlayBuferName.c_str(), O_RDWR | O_CREAT, mode);
        int size = width * height * 3 + sizeof(OverlayBufferData);
        ftruncate(f, size);
        overlayBufferData = (OverlayBufferData*)mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED, f, 0);
        memset(overlayBufferData, 0, size);
        overlayBufferData->width = width;
        overlayBufferData->height = height;
        close(f);
    }
    return overlayBufferData->data;
}

void PixelOverlayModel::clearOverlayBuffer() {
    memset(getOverlayBuffer(), 0, width*height*3);
}
void PixelOverlayModel::fillOverlayBuffer(int r, int g, int b) {
    uint8_t *data = getOverlayBuffer();
    for (int w = 0; w < (width*height); w++) {
        data[0] = r;
        data[1] = g;
        data[2] = b;
        data += 3;
    }
}

void PixelOverlayModel::setOverlayPixelValue(int x, int y, int r, int g, int b) {
    if (y >= height || x >= width || x < 0 || y < 0) {
        return;
    }
    int idx = y * width * 3 + x * 3;
    uint8_t *buf = getOverlayBuffer();
    buf[idx++] = r;
    buf[idx++] = g;
    buf[idx] = b;
}

void PixelOverlayModel::flushOverlayBuffer() {
    setData(getOverlayBuffer());
    //make sure the dirty flag is unset
    overlayBufferData->flags &= ~0x1;
}
void PixelOverlayModel::setOverlayBufferDirty() {
    getOverlayBuffer();
    overlayBufferData->flags |= 0x1;
}

void PixelOverlayModel::setOverlayBufferScaledData(uint8_t *data, int w, int h) {
    float ydiff = (float)h / (float)height;
    float xdiff = (float)w / (float)width;
    uint8_t *buf = getOverlayBuffer();
    
    float newy = 0.0f;
    float newx = 0.0f;
    int x, y;

    for (y = 0, newy = 0.0f; y < height; y++, newy += ydiff) {
        int srcY = newy;
        for (x = 0, newx = 0.0f; x < width; x++, newx += xdiff) {
            int srcX = newx;
            
            int idx = y * width * 3 + x * 3;
            int srcidx = srcY * w * 3 + srcX * 3;
            
            buf[idx++] = data[srcidx++];
            buf[idx++] = data[srcidx++];
            buf[idx] = data[srcidx];
        }
    }
}



int PixelOverlayModel::getStartChannel() const {
    return startChannel;
}
int PixelOverlayModel::getChannelCount() const {
    return channelCount;
}
void PixelOverlayModel::toJson(Json::Value &result) {
    result = config;
}


int32_t PixelOverlayModel::updateRunningEffects() {
    std::unique_lock<std::mutex> l(effectLock);
    if (runningEffect) {
        int32_t v = runningEffect->update();
        if (v == 0) {
            delete runningEffect;
            runningEffect = nullptr;
        }
        return v;
    }
    return 0;
}

void PixelOverlayModel::setRunningEffect(RunningEffect *ef, int32_t firstUpdateMS) {
    std::unique_lock<std::mutex> l(effectLock);
    if (runningEffect) {
        if (runningEffect != ef) {
            delete runningEffect;
        }
        PixelOverlayManager::INSTANCE.removePeriodicUpdate(this);
    }
    runningEffect = ef;
    PixelOverlayManager::INSTANCE.addPeriodicUpdate(firstUpdateMS, this);
}

bool PixelOverlayModel::applyEffect(const std::string &autoState, const std::string &effect, const std::vector<std::string> &args) {
    PixelOverlayEffect *pe = PixelOverlayEffect::GetPixelOverlayEffect(effect);
    if (pe) {
        return pe->apply(this, autoState, args);
    }
    return false;
}
