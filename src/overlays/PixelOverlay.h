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
#include <cstdint>
#include "fpp-json-fwd.h"
#include <condition_variable>
#include "fpphttp_types.h"
#include <list>
#include <map>
#include <mutex>
#include <string>
#include <thread>

class PixelOverlayState;
class PixelOverlayModel;
class OverlayRange;

class PixelOverlayManager {
public:
    static PixelOverlayManager INSTANCE;

    HttpResponsePtr render_GET(const HttpRequestPtr& req);
    HttpResponsePtr render_POST(const HttpRequestPtr& req);
    HttpResponsePtr render_PUT(const HttpRequestPtr& req);
    HttpResponsePtr render_HEAD(const HttpRequestPtr& req);

    bool hasActiveOverlays();
    void doOverlays(uint8_t* channels);
    void modelStateChanged(PixelOverlayModel*, const PixelOverlayState& old, const PixelOverlayState& state);

    void addModel(Json::Value config);
    PixelOverlayModel* getModel(const std::string& name);
    // Non-blocking lookup for callers that may hold a model's effectLock (the
    // effect update threads).  modelsLock -> effectLock is the sanctioned
    // order everywhere else; blocking on modelsLock from under an effectLock
    // is the reverse edge and deadlocks against any HTTP/command path.  This
    // try-acquires modelsLock instead: returns false with model untouched if
    // the lock is contended (caller retries on a later call), true with the
    // lookup result (possibly nullptr for an unknown name) otherwise.  A
    // thread already holding modelsLock always succeeds (recursive).
    bool tryGetModel(const std::string& name, PixelOverlayModel*& model);

    void addModelListener(const std::string& name, const std::string& id, std::function<void(PixelOverlayModel*)> listener);
    void removeModelListener(const std::string& name, const std::string& id);

    void Initialize();

    bool isAutoCreatePixelOverlayModels() const {
        return autoCreate;
    }
    // colorOrder is ignored - the model's channel order follows from channelPerNode.
    // The parameter is retained so plugins built against older headers still link.
    void addAutoOverlayModel(const std::string& name,
                             uint32_t startChannel, uint32_t channelCount, uint32_t channelPerNode,
                             const std::string& orientation, const std::string& startLocation,
                             uint32_t strings, uint32_t strands, const std::string& colorOrder = "RGB");
    void removeAutoOverlayModel(const std::string& name);

    const std::string& mapFont(const std::string& f);
    static uint32_t mapColor(const std::string& c);

    void addPeriodicUpdate(int32_t initialDelayMS, PixelOverlayModel* m);
    // Takes the model out of the update schedule and stamps any update already
    // in flight so it cannot requeue.  Does NOT wait for a running update to
    // finish, and must not: setRunningEffect() calls this holding the model's
    // effectLock, while an in-flight entry is registered *before*
    // updateRunningEffects() acquires that same lock -- so waiting here would
    // block the holder of effectLock on a thread that cannot proceed without
    // it.  Waiting is also unnecessary from there: the caller holds effectLock,
    // so nothing else is inside updateRunningEffects() on this model, and the
    // stamp already prevents a stale requeue.
    void removePeriodicUpdate(PixelOverlayModel* m);
    // As above, but also waits until no other thread is inside
    // updateRunningEffects() on the model.  Only for a caller that is about to
    // delete it (removeAutoOverlayModel()).  MUST NOT be called while holding
    // effectLock or modelsLock -- see the note on the definition.
    void removePeriodicUpdateAndWait(PixelOverlayModel* m);
    void resetChildParent(const std::string& name);

    Json::Value getModelsAsJson();

    Json::Value getActiveOverlayEffects();

    const std::list<std::string>& getModelNames() const { return modelNames; };

    // Bumped whenever the set of models changes -- a (re)load of
    // model-overlays.json, an auto-created model appearing or going away, or a
    // lazy xLights submodel being materialized on first reference. Lets
    // /api/models answer a conditional request without serializing every model
    // again. It deliberately does NOT cover a model's live state or running
    // effect, so it must not be used for /api/overlays/models, which reports
    // both.
    uint64_t getModelsGeneration() const { return modelsGeneration; }

private:
    class PixelOverlayModelHolder {
    public:
        PixelOverlayModel* model;
        std::map<std::string, std::function<void(PixelOverlayModel*)>> listeners;

        void toJson(Json::Value& json) const;
    };

    PixelOverlayManager();
    ~PixelOverlayManager();

    void ConvertCMMFileToJSON();
    void loadModelMap();
    void RegisterCommands();
    PixelOverlayModel* getModelLocked(const std::string& name);

    // Lazy xLights submodel support.  The submodel index (name -> serialized
    // compact config) is parsed from config/xlights-submodels.json on first
    // reference only; individual submodels are materialized into `models` on
    // demand via getModelLocked().  See docs/PixelOverlaySubModels.md.
    void loadSubModelIndex();
    PixelOverlayModel* materializeSubModelLocked(const std::string& name);
    std::map<std::string, std::string> subModelConfigs;
    bool subModelIndexLoaded = false;

    std::atomic_int numActive;
    std::list<PixelOverlayModel*> activeModels;
    std::list<OverlayRange> activeRanges;
    std::recursive_mutex activeModelsLock;

    std::map<std::string, PixelOverlayModelHolder> models;
    std::list<std::string> modelNames;
    std::map<std::string, std::string> fonts;
    bool fontsLoaded = false;
    std::recursive_mutex modelsLock;
    std::atomic<uint64_t> modelsGeneration{ 0 };

    void doOverlayModelEffects();
    std::thread* updateThread = nullptr;
    bool threadKeepRunning = true;
    std::mutex threadLock;
    std::condition_variable threadCV;
    std::map<uint64_t, std::list<PixelOverlayModel*>> updates;
    std::list<PixelOverlayModel*> afterOverlayModels;

    // Models currently inside updateRunningEffects().  Both callers (the
    // FPP-OverlayME thread and doOverlays() on the channel output thread) take
    // the model off `updates`/`afterOverlayModels` and then drop threadLock for
    // the duration of the call, so removing it from those lists is not enough
    // to make deleting it safe -- removeAutoOverlayModel() would free a model
    // that a running effect is still writing into.  Everything that enters the
    // call registers here first.
    //
    // `removed` is what makes the registry an invariant rather than a race.
    // Waiting for the entry to disappear is NOT sufficient: the in-flight
    // thread erases its entry and then requeues the model into
    // `updates`/`afterOverlayModels` while still holding threadLock, so the
    // waiter -- which cannot wake until that lock is released -- observes an
    // empty registry and a model that has just been put back.  It then deletes
    // a model the next tick will dereference.  Both removal variants instead
    // stamp every matching entry, and runEffectUpdateLocked() reports
    // EFFECT_DONE for a stamped entry so the caller never requeues.  Removal is
    // then authoritative for a model that has already started, not only for one
    // that has not -- which is also what lets setRunningEffect() strip the
    // model safely without waiting at all.
    //
    // The stamp also fixes a pre-existing duplicate: setRunningEffect() removes
    // and re-adds the model via addPeriodicUpdate(), and without the stamp the
    // in-flight caller requeued it again on return, ticking it at twice its
    // period until the next removal.
    //
    // `tid` scopes the wait in removePeriodicUpdateAndWait() to *other*
    // threads.  Nothing reaches that variant from inside an update today, so it
    // is not load-bearing, but it keeps the predicate honest if one ever does.
    struct InFlight {
        PixelOverlayModel* m;
        std::thread::id tid;
        bool removed = false;
    };
    std::list<InFlight> inFlightModels;
    std::condition_variable inFlightCV;
    // threadLock must be held.
    void StampAndStripLocked(PixelOverlayModel* m);
    void runEffectUpdateLocked(PixelOverlayModel* m, std::unique_lock<std::mutex>& l,
                               int32_t* msOut);

    void loadFonts();

    bool autoCreate = true;

    friend class OverlayCommand;
};
