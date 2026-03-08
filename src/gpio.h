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
#include <functional>
#include <map>
#include <httpserver.hpp>
#include "config.h"

constexpr uint32_t DEFAULT_GPIO_DEBOUNCE_TIME = 100000;

class PinCapabilities;

class GPIOManager : public httpserver::http_resource {
public:
    static GPIOManager INSTANCE;
    std::map<std::string, bool> fppCommandLastValue;
    virtual HTTP_RESPONSE_CONST std::shared_ptr<httpserver::http_response> render_GET(const httpserver::http_request& req) override;
    virtual HTTP_RESPONSE_CONST std::shared_ptr<httpserver::http_response> render_POST(const httpserver::http_request& req) override;
    void Initialize(std::map<int, std::function<bool(int)>>& callbacks);
    void CheckGPIOInputs(void);
    void Cleanup();
    void AddGPIOCallback(const PinCapabilities* pin, const std::function<bool(int)>& cb);
    void RemoveGPIOCallback(const PinCapabilities* pin);
private:
    class GPIOState {
    public:
        GPIOState() :
            pin(nullptr),
            lastValue(0),
            lastTriggerTime(0),
            futureValue(0),
            pendingValue(0),
            pendingTime(0),
            file(-1) {}
        const PinCapabilities* pin;
        int lastValue;
        long long lastTriggerTime;
        int futureValue;
        int pendingValue;       // candidate value waiting to settle
        long long pendingTime;  // usec timestamp when pendingValue was first seen
        int file;
        Json::Value fallingAction;
        Json::Value risingAction;
        std::function<bool(int)> callback;
        bool hasCallback = false;
        uint32_t debounceTime = DEFAULT_GPIO_DEBOUNCE_TIME;

        // Controls which edges have debounce applied
        enum class DebounceEdge {
            Both,    // debounce rising and falling (default)
            Rising,  // debounce only rising; falling fires immediately
            Falling  // debounce only falling; rising fires immediately
        };
        DebounceEdge debounceEdge = DebounceEdge::Both;

        bool shouldDebounce(int v) const;
        void doAction(int newVal);
    };
    GPIOManager();
    ~GPIOManager();
    void SetupGPIOInput(std::map<int, std::function<bool(int)>>& callbacks);
    void addState(GPIOState* state);
    void addGPIOCallback(GPIOState* state);
    void checkDebounceTimers();
    void scheduleDebounceCheck(GPIOState* state);
    std::list<GPIOState*> pollStates;
    std::list<GPIOState*> eventStates;
    friend class GPIOCommand;
};
