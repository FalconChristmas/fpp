/*
 *   Pixel Overlay Model for Falcon Player (FPP)
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

#ifndef _PIXELOVERLAY_MODEL_H
#define _PIXELOVERLAY_MODEL_H

#include <thread>
#include <mutex>
#include "PixelOverlayControl.h"

class RunningEffect;

class PixelOverlayState {
public:
    enum PixelState: uint8_t {
        Disabled,
        Enabled,
        Transparent,
        TransparentRGB
    };

    PixelOverlayState() = default;
    constexpr PixelOverlayState(PixelState v) : state(v) {}
    constexpr PixelOverlayState(int v) : state((PixelState)v) {}
    PixelOverlayState(const std::string &v) {
        if (v == "Disabled") {
            state = PixelState::Disabled;
        } else if (v == "Enabled") {
            state = PixelState::Enabled;
        } else if (v == "Transparent") {
            state = PixelState::Transparent;
        } else if (v == "TransparentRGB" || v == "Transparent RGB") {
            state = PixelState::TransparentRGB;
        } else {
            state = PixelState::Disabled;
        }
    }

    PixelState getState() const { return state; }
    bool operator==(PixelOverlayState a) const { return state == a.state; }
    bool operator!=(PixelOverlayState a) const { return state != a.state; }
private:
    PixelState state;
};


class PixelOverlayModel {
public:
    PixelOverlayModel(FPPChannelMemoryMapControlBlock *block,
                      const std::string &name,
                      char         *chanDataMap,
                      uint32_t     *pixelMap);
    ~PixelOverlayModel();

    const std::string &getName() const {return name;};

    int getWidth() const { return width; }
    int getHeight() const { return height; }
    void getSize(int &w, int &h) const { w = width; h = height; }
    
    PixelOverlayState getState() const;
    void setState(const PixelOverlayState &state);
    
    void setData(const uint8_t *data);
    void clear() { fill(0, 0, 0); }
    void fill(int r, int g, int b);

    void setValue(uint8_t v, int startChannel = -1, int endChannel = -1);
    void setPixelValue(int x, int y, int r, int g, int b);    
    
    int getStartChannel() const;
    int getChannelCount() const;
    bool isHorizontal() const;
    int getNumStrings() const;
    int getStrandsPerString() const;
    std::string getStartCorner() const;
    
    void toJson(Json::Value &v);
    void getDataJson(Json::Value &v);
    
    
    uint8_t *getOverlayBuffer();
    void clearOverlayBuffer();
    void setOverlayPixelValue(int x, int y, int r, int g, int b);
    void flushOverlayBuffer();

    FPPChannelMemoryMapControlBlock *getBlock() { return block; }
    
    bool applyEffect(bool autoEnable, const std::string &effect, const std::vector<std::string> &args);
    void setRunningEffect(RunningEffect *r, int32_t firstUpdateMS);
    RunningEffect *getRunningEffect() const { return runningEffect; }
    
    int32_t updateRunningEffects();

private:
    std::string name;
    int width, height;
    FPPChannelMemoryMapControlBlock *block;
    char         *chanDataMap;
    uint32_t     *pixelMap;
    uint8_t *overlayBuffer;

    std::mutex   effectLock;
    RunningEffect *runningEffect;
};

#endif
