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

#ifndef _PIXELOVERLAY_H
#define _PIXELOVERLAY_H

#include <string>
#include <httpserver.hpp>
#include <map>
#include <mutex>
#include <thread>
#include <jsoncpp/json/json.h>

#include "PixelOverlayControl.h"


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

    int getWidth() const;
    int getHeight() const;
    void getSize(int &w, int &h) const;
    
    PixelOverlayState getState() const;
    void setState(const PixelOverlayState &state);
    
    bool isLocked();
    void lock(bool lock = true);
    void unlock() { lock(false); }

    void setData(const uint8_t *data);
    void clear() { fill(0, 0, 0); }
    void fill(int r, int g, int b);
    
    void setValue(uint8_t v, int startChannel = -1, int endChannel = -1);
    void setPixelValue(int x, int y, int r, int g, int b);
    void doText(const std::string &msg,
                int r, int g, int b,
                const std::string &font,
                int fontSize,
                bool antialias,
                const std::string &position,
                int pixelsPerSecond,
                bool autoEnable);
    
    
    int getStartChannel() const;
    int getChannelCount() const;
    bool isHorizontal() const;
    int getNumStrings() const;
    int getStrandsPerString() const;
    std::string getStartCorner() const;
    
    void toJson(Json::Value &v);
    void getDataJson(Json::Value &v);
    
private:
    void copyImageData(int xoff, int yoff);
    void doImageMovementThread(const std::string &direction, int x, int y, int speed, bool disableWhenDone);
    
    std::string name;
    FPPChannelMemoryMapControlBlock *block;
    char         *chanDataMap;
    uint32_t     *pixelMap;
    
    std::thread *updateThread;
    volatile bool threadKeepRunning;
    uint8_t *overlayBuffer;
    uint8_t *imageData;
    int imageDataRows;
    int imageDataCols;
};


class PixelOverlayManager : public httpserver::http_resource {
public:
    static PixelOverlayManager INSTANCE;

    virtual const httpserver::http_response render_GET(const httpserver::http_request &req) override;
    virtual const httpserver::http_response render_POST(const httpserver::http_request &req) override;
    virtual const httpserver::http_response render_PUT(const httpserver::http_request &req) override;

    void OverlayMemoryMap(char *channelData);
    int UsingMemoryMapInput();
    
    PixelOverlayModel* getModel(const std::string &name);
    
    void Initialize();
    
    const std::string &mapFont(const std::string &f);
    void LightMessageHandler(const std::string &topic, const std::string &payload);

private:
    PixelOverlayManager();
    ~PixelOverlayManager();
    
    bool createChannelDataMap();
    bool createControlMap();
    bool createPixelMap();
    void loadModelMap();
    void SetupPixelMapForBlock(FPPChannelMemoryMapControlBlock *b);
    void ConvertCMMFileToJSON();
    void RegisterCommands();
    
    std::map<std::string, PixelOverlayModel*> models;
    std::map<std::string, std::string> fonts;
    bool fontsLoaded = false;
    std::mutex   modelsLock;
    char         *ctrlMap = nullptr;
    char         *chanDataMap = nullptr;
    uint32_t     *pixelMap = nullptr;
    
    FPPChannelMemoryMapControlHeader *ctrlHeader = nullptr;
    
    void loadFonts();
    
    friend class OverlayCommand;
};


#endif /* _PIXELOVERLAY_H */
