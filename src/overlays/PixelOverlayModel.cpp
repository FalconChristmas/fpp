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


#include <stdio.h>
#include <cstring>

#include <jsoncpp/json/json.h>
#include "channeloutput/channeloutputthread.h"

#include "PixelOverlayModel.h"
#include "PixelOverlay.h"
#include "PixelOverlayEffects.h"


PixelOverlayModel::PixelOverlayModel(FPPChannelMemoryMapControlBlock *b,
                                     const std::string &n,
                                     char         *cdm,
                                     uint32_t     *pm)
    : block(b), name(n), chanDataMap(cdm), pixelMap(pm),
    overlayBuffer(nullptr), runningEffect(nullptr)
{
    height = block->stringCount * block->strandsPerString;
    width = block->channelCount / 3;
    width /= height;

    if (block->orientation == 'V') {
        std::swap(width, height);
    }
    
}
PixelOverlayModel::~PixelOverlayModel() {
    if (overlayBuffer) {
        free(overlayBuffer);
    }
}

PixelOverlayState PixelOverlayModel::getState() const {
    int i = block->isActive;
    return PixelOverlayState(i);
}

void PixelOverlayModel::setState(const PixelOverlayState &state) {
    int i = (int)state.getState();
    block->isActive = i;
    if (i) {
        // we are going active, make sure teh output thread is started up
        StartChannelOutputThread();
    }
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

uint8_t *PixelOverlayModel::getOverlayBuffer() {
    if (!overlayBuffer)
        overlayBuffer = (uint8_t*)calloc(1, block->channelCount);
    return overlayBuffer;
}

void PixelOverlayModel::clearOverlayBuffer() {
    memset(getOverlayBuffer(), 0, block->channelCount);
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
}


void PixelOverlayModel::getDataJson(Json::Value &v) {
    for (int c = 0; c < block->channelCount; c++) {
        unsigned char i = chanDataMap[pixelMap[block->startChannel + c - 1]];
        v.append(i);
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
void PixelOverlayModel::toJson(Json::Value &result) {
    result["Name"] = getName();
    result["StartChannel"] = getStartChannel();
    result["ChannelCount"] = getChannelCount();
    result["Orientation"] = isHorizontal() ? "horizontal" : "vertical";
    result["StartCorner"] = getStartCorner();
    result["StringCount"] = getNumStrings();
    result["StrandsPerString"] = getStrandsPerString();
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

bool PixelOverlayModel::applyEffect(bool autoEnable, const std::string &effect, const std::vector<std::string> &args) {
    PixelOverlayEffect *pe = PixelOverlayEffect::GetPixelOverlayEffect(effect);
    if (pe) {
        return pe->apply(this, autoEnable, args);
    }
    return false;
}
