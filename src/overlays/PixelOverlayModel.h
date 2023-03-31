#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
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

    int getStartChannel() const;
    int getChannelCount() const;

    void toJson(Json::Value& v);
    void getDataJson(Json::Value& v, bool rle = false);
    
    
    // Access to the chanelData.  The channelData is a minimal array of bytes
    // If the model is a "custom" model or using singleChannel nodes or similar,
    // then the channelData will be significantly smaller than WxHx3
    void saveOverlayAsImage(std::string filename = "");
    virtual void setData(const uint8_t* data); // full RGB data, width*height*3
    virtual void setData(const uint8_t* data, int xOffset, int yOffset, int w, int h, const PixelOverlayState &st = PixelOverlayState(PixelOverlayState::Enabled));
    void setScaledData(uint8_t* data, int w, int h);
    void setPixelValue(int x, int y, int r, int g, int b);
    void getPixelValue(int x, int y, int &r, int &g, int &b);
    void clearData();
    void fillData(int r, int g, int b);
    void setBufferIsDirty(bool dirty = true);
    bool needRefresh();

    
    // The overlay buffer is a full continuous width*height*3 buffer that can be used to
    // construct the frame as a full RGB image prior to flushing to the channelData.
    // The overlay buffer is also mmapped so external programs can have easy
    // access to the continuous width*height*3 buffer
    uint8_t* getOverlayBuffer();
    void setOverlayBufferDirty(bool dirty = true);
    bool overlayBufferIsDirty();
    void clearOverlayBuffer();
    void setOverlayBufferScaledData(uint8_t* data, int w, int h);
    void fillOverlayBuffer(int r, int g, int b);
    void setOverlayPixelValue(int x, int y, int r, int g, int b);
    void getOverlayPixelValue(int x, int y, int& r, int& g, int& b);
    void flushOverlayBuffer();

    
    // Operate on both the overlay buffer (if mapped) and the channelData
    void clear();
    void fill(int r, int g, int b);

    bool applyEffect(const std::string& autoState, const std::string& effect, const std::vector<std::string>& args);
    void setRunningEffect(RunningEffect* r, int32_t firstUpdateMS);
    RunningEffect* getRunningEffect() const { return runningEffect; }
    int32_t updateRunningEffects();

    void setChildState(const std::string &n, const PixelOverlayState& state, int ox, int oy, int w, int h);
protected:
    void setValue(uint8_t v, int startChannel = -1, int endChannel = -1);
    bool flushChildren(uint8_t* dst);

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
    
    class ChildModelState {
    public:
        std::string name;
        PixelOverlayState state = PixelOverlayState::Disabled;
        int xoffset = 0;
        int yoffset = 0;
        int width = 0;
        int height = 0;
    };
    std::list<ChildModelState> children;
};
