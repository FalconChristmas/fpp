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

#include <list>
#include <map>
#include "fpp-json.h"
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "../AtomicSharedPtr.h"
#include "PolarBufferMap.h"

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

    // How this model's BUFFER is laid out, when it is not the plain
    // wiring-order rectangle. Read from the model definition's "BufferStyle"
    // and reported in toJson(), so a producer (an xLights bridge script, a
    // plugin) can declare a model whose axes already mean something and the UI
    // can group and label it accordingly rather than filing it under whatever
    // file it happened to arrive in. Empty for an ordinary model.
    //
    // Known values: "" (ordinary) and "polar" (x/y are radius and angle).
    const std::string& getBufferStyle() const { return bufferStyle; }

    int getWidth() const { return width; }
    int getHeight() const { return height; }
    void getSize(int& w, int& h) const {
        w = width;
        h = height;
    }

    PixelOverlayState getState() const;
    virtual void setState(const PixelOverlayState& state);

    virtual void doOverlay(uint8_t* channels);

    // A polar (radius x angle) view of this model's buffer, for the Radial/
    // Angular buffer mappings. Built on first use from the model's geometry in
    // config/virtualdisplaymap and cached; the result is owned by the model.
    //
    // Returns nullptr whenever a polar layout cannot be built -- no layout file
    // (a player that has never used xLights), this model absent from it, or too
    // little geometry. Callers MUST fall back to the ordinary buffer mapping,
    // never treat it as an error.
    //
    // Not thread safe against itself: call it from the command path that starts
    // an effect, never from the channel output thread.
    const PolarBufferMap* getPolarMap(PolarMode mode);

    int getStartChannel() const;
    int getChannelCount() const;

    void toJson(Json::Value& v);
    void getDataJson(Json::Value& v, bool rle = false);
    // Binary equivalent of getDataJson(): the current pixel data as a flat
    // width*height*bytesPerPixel buffer.
    void getData(std::vector<uint8_t>& out);

    // Access to the channelData.  The channelData is a minimal array of bytes
    // If the model is a "custom" model or using singleChannel nodes or similar,
    // then the channelData will be significantly smaller than WxHxBPP
    int getBytesPerPixel() const { return bytesPerPixel; }
    void saveOverlayAsImage(std::string filename = "");
    virtual void setData(const uint8_t* data); // full pixel data, width*height*bytesPerPixel
    virtual void setData(const uint8_t* data, int xOffset, int yOffset, int w, int h, const PixelOverlayState& st = PixelOverlayState(PixelOverlayState::Enabled));

    // Blit externally supplied pixel data onto the model. src is srcW*srcH
    // pixels of srcBpp bytes each (1 = greyscale, 3 = RGB, 4 = RGBW); it is
    // converted to this model's bytesPerPixel and CLIPPED to the model bounds,
    // so a rect hanging off an edge draws the part that is visible instead of
    // being rejected the way setData()'s rect form is. That is what lets a
    // caller slide a sprite off the edge by reposting it at moving offsets.
    // Returns false only when nothing landed inside the model at all.
    //
    // blitData() writes the output channel data (honoring st as the per-blit
    // blend); blitOverlayBuffer() writes the mmapped overlay image buffer and
    // marks it dirty, so the write composites into the same persistent canvas
    // external mmap clients use and is flushed on the next output pass.
    bool blitData(const uint8_t* src, int srcW, int srcH, int srcBpp,
                  int xOffset, int yOffset, const PixelOverlayState& st);
    bool blitOverlayBuffer(const uint8_t* src, int srcW, int srcH, int srcBpp,
                           int xOffset, int yOffset);

    void setScaledData(uint8_t* data, int w, int h);
    void setPixelValue(int x, int y, int r, int g, int b);
    void setPixelValue(int x, int y, int r, int g, int b, int w);
    void getPixelValue(int x, int y, int& r, int& g, int& b);
    void getPixelValue(int x, int y, int& r, int& g, int& b, int& w);
    void clearData();
    void fillData(int r, int g, int b);
    void fillData(int r, int g, int b, int w);
    void setBufferIsDirty(bool dirty = true);
    bool needRefresh();

    // The overlay buffer is a full continuous width*height*bytesPerPixel buffer
    // that can be used to construct the frame as a full RGB(W) image prior to
    // flushing to the channelData.  The overlay buffer is also mmapped so
    // external programs can have easy access to it.
    uint8_t* getOverlayBuffer();
    void setOverlayBufferDirty(bool dirty = true);
    bool overlayBufferIsDirty();
    void clearOverlayBuffer();
    void setOverlayBufferScaledData(uint8_t* data, int w, int h);
    void fillOverlayBuffer(int r, int g, int b);
    void fillOverlayBuffer(int r, int g, int b, int w);
    void setOverlayPixelValue(int x, int y, int r, int g, int b);
    void setOverlayPixelValue(int x, int y, int r, int g, int b, int w);
    void getOverlayPixelValue(int x, int y, int& r, int& g, int& b);
    void getOverlayPixelValue(int x, int y, int& r, int& g, int& b, int& w);
    void flushOverlayBuffer();

    // Operate on both the overlay buffer (if mapped) and the channelData
    void clear();
    void fill(int r, int g, int b);
    void fill(int r, int g, int b, int w);

    bool applyEffect(const std::string& autoState, const std::string& effect, const std::vector<std::string>& args);
    void setRunningEffect(RunningEffect* r, int32_t firstUpdateMS);

    // Retire the running effect WITHOUT clearing what it last drew.  This is
    // the "nothing takes over" case setRunningEffect() cannot express, and the
    // Text effect needs it: a still, centred message writes its pixels
    // directly, so an effect left repainting from an earlier run (a scroll, or
    // an animated colour mode) would paint straight back over it.
    void clearRunningEffect();

    std::recursive_mutex& getRunningEffectMutex() { return effectLock; }
    RunningEffect* getRunningEffect() const { return runningEffect; } // make sure you have the mutex locked
    int32_t updateRunningEffects();

    void setChildState(const std::string& n, const PixelOverlayState& state, int ox, int oy, int w, int h);
    bool isAutoCreated();

protected:
    void setValue(uint8_t v, int startChannel = -1, int endChannel = -1);
    bool flushChildren(uint8_t* dst);

    // The 0-based ABSOLUTE output channel a channelData offset ends up on, or
    // FPPD_OFF_CHANNEL. A plain model owns a contiguous range so it is just an
    // addition; a submodel or group scatters, so those override this. Used only
    // to join buffer cells to their position in the virtual display map.
    virtual uint32_t outputChannelForData(uint32_t dataOffset) const;

    // Cached polar views, keyed by mode AND the layout file's mtime.
    //
    // One entry per key, never replaced or erased: a running effect holds a
    // bare pointer into this for its lifetime, so rebuilding in place would
    // dangle it. std::map node addresses are stable across inserts, which is
    // the property relied on. An entry with valid() == false records
    // "tried, cannot" so a model with no geometry costs one attempt.
    //
    // The mtime in the key is what makes a re-upload from xLights take effect:
    // a model that already built a map would otherwise keep its stale layout
    // until fppd restarted. A superseded entry is left behind rather than
    // erased (a few KB, only when the layout actually changes mid-run) because
    // an effect may still be pointing at it.
    std::map<std::pair<PolarMode, long long>, PolarBufferMap> polarMaps;

    Json::Value config;
    std::string name;
    std::string type;
    int width, height;
    std::string bufferStyle;
    PixelOverlayState state;
    int startChannel;
    int channelCount;
    int channelsPerNode;
    int bytesPerPixel; // 3 for RGB, 4 for RGBW (overlay buffer stride)

    std::vector<uint32_t> channelMap;
    uint8_t* channelData;

    volatile bool dirtyBuffer = false;

    struct OverlayBufferData {
        uint32_t width;
        uint32_t height;
        uint32_t flags; // bit 0: dirty, bits 8-15: bytesPerPixel (0 means 3 for backward compat)
        uint8_t data[4];
    } __attribute__((__packed__));
    OverlayBufferData* overlayBufferData;

    std::recursive_mutex effectLock;
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

    // The child list is copy-on-write, published through an AtomicSharedPtr,
    // and DELIBERATELY not guarded by any of the overlay locks.
    //
    // Readers (flushChildren(), doOverlay(), the FB subclass) run on the
    // channel output thread, which reaches them from doOverlays() holding
    // activeModelsLock.  The writer, setChildState(), is reached from
    // PixelOverlayModelSub::setState() -- an HTTP/command thread under
    // modelsLock, or an effect update thread under that model's effectLock.
    // Guarding `children` with a lock that either side already holds is what
    // makes this area deadlock: activeModelsLock -> X on the reader against
    // modelsLock/effectLock -> X on the writer closes a cycle with the
    // existing modelsLock -> activeModelsLock and modelsLock -> effectLock
    // orders (see PixelOverlayModelSub::foundParent()).
    //
    // Copy-on-write sidesteps the ordering question entirely: a reader takes a
    // snapshot and iterates a list nobody can mutate or free, so it needs no
    // lock at all.  Previously readers walked the live std::list while
    // setChildState() erased nodes out from under them, which faulted in
    // flushChildren() on a freed node's garbage width/height.
    //
    // childrenWriteLock serializes only the read-copy-publish sequence in
    // setChildState() -- writers are genuinely concurrent (HTTP vs. effect
    // thread), so without it one update can be lost.  It is a LEAF lock: it is
    // held across nothing but list copying and the store, never across a call
    // that could reach another lock (notably not modelStateChanged(), which
    // takes activeModelsLock).  Keep it that way and it cannot participate in
    // a cycle.
    typedef std::list<ChildModelState> ChildModelStateList;
    AtomicSharedPtr<const ChildModelStateList> children;
    std::mutex childrenWriteLock;

    // A null snapshot means "no children" -- models without submodels never
    // allocate a list.
    bool hasChildren() const {
        auto snap = children.load();
        return snap && !snap->empty();
    }
};
