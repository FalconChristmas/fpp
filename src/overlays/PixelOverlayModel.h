#pragma once
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

#include <mutex>
#include <thread>

class RunningEffect;

class PixelOverlayState {
public:
    enum PixelState : uint8_t {
        Disabled,
        Enabled,
        Transparent,
        TransparentRGB
    };

    PixelOverlayState() :
        state(PixelState::Disabled) {}
    constexpr PixelOverlayState(PixelState v) :
        state(v) {}
    constexpr PixelOverlayState(int v) :
        state((PixelState)v) {}
    PixelOverlayState(const std::string& v) {
        if (v == "Disabled" || v == "false" || v == "False" || v == "0") {
            state = PixelState::Disabled;
        } else if (v == "Enabled" || v == "true" || v == "True" || v == "1") {
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
    PixelOverlayModel(const Json::Value& config);
    virtual ~PixelOverlayModel();

    const std::string& getName() const { return name; };
    const std::string& getType() const { return type; };

    int getWidth() const { return width; }
    int getHeight() const { return height; }
    void getSize(int& w, int& h) const {
        w = width;
        h = height;
    }

    PixelOverlayState getState() const;
    virtual void setState(const PixelOverlayState& state);

    virtual void doOverlay(uint8_t* channels);

    virtual void setData(const uint8_t* data); // full RGB data, width*height*3
    virtual void setData(const uint8_t* data, int xOffset, int yOffset, int w, int h);

    int getStartChannel() const;
    int getChannelCount() const;

    void toJson(Json::Value& v);
    void getDataJson(Json::Value& v, bool rle = false);

    uint8_t* getOverlayBuffer();
    void clearOverlayBuffer();
    void setOverlayBufferScaledData(uint8_t* data, int w, int h);
    void fillOverlayBuffer(int r, int g, int b);
    void setOverlayPixelValue(int x, int y, int r, int g, int b);
    void getOverlayPixelValue(int x, int y, int& r, int& g, int& b);
    virtual void setOverlayBufferDirty(bool dirty = true);
    void flushOverlayBuffer();

    virtual bool overlayBufferIsDirty();

    void setBufferIsDirty(bool dirty = true);
    bool needRefresh();

    void clear() {
        clearOverlayBuffer();
        flushOverlayBuffer();
    }
    void fill(int r, int g, int b) {
        fillOverlayBuffer(r, g, b);
        flushOverlayBuffer();
    }
    void setPixelValue(int x, int y, int r, int g, int b);

    bool applyEffect(const std::string& autoState, const std::string& effect, const std::vector<std::string>& args);
    void setRunningEffect(RunningEffect* r, int32_t firstUpdateMS);
    RunningEffect* getRunningEffect() const { return runningEffect; }

    int32_t updateRunningEffects();

protected:
    void setValue(uint8_t v, int startChannel = -1, int endChannel = -1);

    Json::Value config;
    std::string name;
    std::string type;
    int width, height;
    PixelOverlayState state;
    int startChannel;
    int channelCount;
    int channelsPerNode;

    std::vector<uint32_t> channelMap;
    uint8_t* channelData;

    volatile bool dirtyBuffer = false;

    struct OverlayBufferData {
        uint32_t width;
        uint32_t height;
        uint32_t flags;
        uint8_t data[4];
    } __attribute__((__packed__));
    OverlayBufferData* overlayBufferData;

    std::mutex effectLock;
    RunningEffect* runningEffect;
};
