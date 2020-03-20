/*
 *   Pixel Overlay handler for Falcon Player (FPP)
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
#include <list>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>

class PixelOverlayState;
class PixelOverlayModel;
class OverlayRange;

class PixelOverlayManager : public httpserver::http_resource {
public:
    static PixelOverlayManager INSTANCE;

    virtual const std::shared_ptr<httpserver::http_response> render_GET(const httpserver::http_request &req) override;
    virtual const std::shared_ptr<httpserver::http_response> render_POST(const httpserver::http_request &req) override;
    virtual const std::shared_ptr<httpserver::http_response> render_PUT(const httpserver::http_request &req) override;

    bool hasActiveOverlays();
    void doOverlays(uint8_t *channels);
    void modelStateChanged(PixelOverlayModel *, const PixelOverlayState &old, const PixelOverlayState &state);
    
    PixelOverlayModel* getModel(const std::string &name);
    
    void Initialize();
    
    void SendHomeAssistantDiscoveryConfig();
    void LightMessageHandler(const std::string &topic, const std::string &payload);
    
    const std::string &mapFont(const std::string &f);
    static uint32_t mapColor(const std::string &c);
    
    void addPeriodicUpdate(int32_t initialDelayMS, PixelOverlayModel*m);
    void removePeriodicUpdate(PixelOverlayModel*m);
private:
    PixelOverlayManager();
    ~PixelOverlayManager();
    
    void ConvertCMMFileToJSON();
    void loadModelMap();
    void RegisterCommands();
    
    std::atomic_int numActive;
    std::list<PixelOverlayModel*> activeModels;
    std::list<OverlayRange> activeRanges;
    std::mutex activeModelsLock;

    
    
    std::map<std::string, PixelOverlayModel*> models;
    std::map<std::string, std::string> fonts;
    bool fontsLoaded = false;
    std::mutex   modelsLock;
    
    void doOverlayModelEffects();
    std::thread *updateThread = nullptr;
    bool threadKeepRunning = true;
    std::mutex   threadLock;
    std::condition_variable threadCV;
    std::map<uint64_t, std::list<PixelOverlayModel*>> updates;
    std::list<PixelOverlayModel*> afterOverlayModels;
        
    void loadFonts();
    
    friend class OverlayCommand;
};


#endif /* _PIXELOVERLAY_H */
