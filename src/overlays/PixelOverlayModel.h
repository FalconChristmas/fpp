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
#include <jsoncpp/json/json.h>

class RunningEffect;

class PixelOverlayState {
public:
    enum PixelState: uint8_t {
        Disabled,
        Enabled,
        Transparent,
        TransparentRGB
    };

    PixelOverlayState() : state(PixelState::Disabled) {}
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
    PixelOverlayModel(const Json::Value &config);
    ~PixelOverlayModel();

    const std::string &getName() const {return name;};

    int getWidth() const { return width; }
    int getHeight() const { return height; }
    void getSize(int &w, int &h) const { w = width; h = height; }
    
    PixelOverlayState getState() const;
    void setState(const PixelOverlayState &state);
    
    void doOverlay(uint8_t *channels);
    
    void setData(const uint8_t *data); // full RGB data, width*height*3

    
    int getStartChannel() const;
    int getChannelCount() const;

    
    void toJson(Json::Value &v);
    void getDataJson(Json::Value &v);
    
    
    uint8_t *getOverlayBuffer();
    void clearOverlayBuffer();
    void fillOverlayBuffer(int r, int g, int b);
    void setOverlayPixelValue(int x, int y, int r, int g, int b);
    void flushOverlayBuffer();

    void clear() { clearOverlayBuffer(); flushOverlayBuffer(); }
    void fill(int r, int g, int b) {
        fillOverlayBuffer(r, g, b);
        flushOverlayBuffer();
    }
    void setPixelValue(int x, int y, int r, int g, int b);

    
    bool applyEffect(bool autoEnable, const std::string &effect, const std::vector<std::string> &args);
    void setRunningEffect(RunningEffect *r, int32_t firstUpdateMS);
    RunningEffect *getRunningEffect() const { return runningEffect; }
    
    int32_t updateRunningEffects();

private:
    void setValue(uint8_t v, int startChannel = -1, int endChannel = -1);

    
    
    Json::Value config;
    std::string name;
    int width, height;
    PixelOverlayState state;
    int startChannel;
    int channelCount;
    
    std::vector<uint32_t> channelMap;
    uint8_t      *channelData;
    
    struct OverlayBufferData {
        uint32_t width;
        uint32_t height;
        uint32_t flags;
        uint8_t  data[4];
    } __attribute__((__packed__));
    OverlayBufferData *overlayBufferData;

    std::mutex   effectLock;
    RunningEffect *runningEffect;
};

#endif
