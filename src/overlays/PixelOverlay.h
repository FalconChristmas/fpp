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

#include <atomic>
#include <condition_variable>
#include <httpserver.hpp>
#include <list>
#include <map>
#include <mutex>
#include <string>
#include <thread>

class PixelOverlayState;
class PixelOverlayModel;
class OverlayRange;

class PixelOverlayManager : public httpserver::http_resource {
public:
    static PixelOverlayManager INSTANCE;

    virtual HTTP_RESPONSE_CONST std::shared_ptr<httpserver::http_response> render_GET(const httpserver::http_request& req) override;
    virtual HTTP_RESPONSE_CONST std::shared_ptr<httpserver::http_response> render_POST(const httpserver::http_request& req) override;
    virtual HTTP_RESPONSE_CONST std::shared_ptr<httpserver::http_response> render_PUT(const httpserver::http_request& req) override;

    bool hasActiveOverlays();
    void doOverlays(uint8_t* channels);
    void modelStateChanged(PixelOverlayModel*, const PixelOverlayState& old, const PixelOverlayState& state);

    void addModel(Json::Value config);
    PixelOverlayModel* getModel(const std::string& name);

    void Initialize();

    bool isAutoCreatePixelOverlayModels() const {
        return autoCreate;
    }
    void addAutoOverlayModel(const std::string& name,
                             uint32_t startChannel, uint32_t channelCount, uint32_t channelsPerNode,
                             const std::string& orientation, const std::string& startLocation,
                             uint32_t strings, uint32_t strands);

    const std::string& mapFont(const std::string& f);
    static uint32_t mapColor(const std::string& c);

    void addPeriodicUpdate(int32_t initialDelayMS, PixelOverlayModel* m);
    void removePeriodicUpdate(PixelOverlayModel* m);

    Json::Value getModelsAsJson();

    const std::list<std::string> &getModelNames() const { return modelNames; };

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
    std::list<std::string> modelNames;
    std::map<std::string, std::string> fonts;
    bool fontsLoaded = false;
    std::mutex modelsLock;

    void doOverlayModelEffects();
    std::thread* updateThread = nullptr;
    bool threadKeepRunning = true;
    std::mutex threadLock;
    std::condition_variable threadCV;
    std::map<uint64_t, std::list<PixelOverlayModel*>> updates;
    std::list<PixelOverlayModel*> afterOverlayModels;

    void loadFonts();

    bool autoCreate = true;

    friend class OverlayCommand;
};
