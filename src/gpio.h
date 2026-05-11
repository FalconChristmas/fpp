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
#include <vector>
#include "fpphttp.h"
#include "config.h"

constexpr uint32_t DEFAULT_GPIO_DEBOUNCE_TIME = 100000;

class PinCapabilities;

class GPIOManager {
public:
    static GPIOManager INSTANCE;
    std::map<std::string, bool> fppCommandLastValue;
    HttpResponsePtr render_GET(const HttpRequestPtr& req);
    HttpResponsePtr render_POST(const HttpRequestPtr& req);
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
            file(-1),
            holdTime(0),
            holdFired(false),
            holdStartTime(0),
            ledActiveHigh(true),
            ledIdleMode(LEDIdleMode::Off),
            ledPulseRateMs(500),
            ledTriggerMode(LEDTriggerMode::FollowInput),
            ledTriggerParam(0),
            ledPulseState(false) {}

        const PinCapabilities* pin;
        int lastValue;
        long long lastTriggerTime;
        int futureValue;
        int pendingValue;       // candidate value waiting to settle
        long long pendingTime;  // usec timestamp when pendingValue was first seen
        int file;

        // Multiple commands per edge — old single-object format is normalised
        // into these vectors during SetupGPIOInput.
        std::vector<Json::Value> risingActions;
        std::vector<Json::Value> fallingActions;

        // Long-press / hold: commands fired after holdTime ms of continuous press.
        std::vector<Json::Value> holdActions;
        uint32_t holdTime;         // ms; 0 = disabled
        bool holdFired;            // true once hold actions have fired this press
        long long holdStartTime;   // GetTimeMS() when the current press began

        // ── LED output associated with this illuminated button ──────────────
        std::string ledPin;
        bool ledActiveHigh;    // true → HIGH means LED on

        // What the LED does when idle (not in a trigger effect)
        enum class LEDIdleMode {
            Off,    // LED off at rest
            On,     // LED on at rest
            Pulse   // LED blinks at ledPulseRateMs interval
        };
        LEDIdleMode ledIdleMode;
        uint32_t ledPulseRateMs;    // ms per half-period when pulsing
        bool ledPulseState;         // current toggle state for pulsing

        // What the LED does when a trigger fires (rising edge)
        enum class LEDTriggerMode {
            None,           // no LED change on trigger; idle mode only
            FollowInput,    // LED mirrors input state (on when pressed, off when released)
            Flash,          // flash N times (ledTriggerParam = flash count) then restore idle
            TimedOn         // stay on for ledTriggerParam ms then restore idle
        };
        LEDTriggerMode ledTriggerMode;
        uint32_t ledTriggerParam;   // flash count  OR  duration ms

        // ── LED helpers ─────────────────────────────────────────────────────
        void initLED();
        void setLEDState(bool on);
        void startLEDIdle();
        void stopLEDIdle();
        void triggerLEDEffect(int v);
        void doLEDFlash(int halfPeriodsRemaining);

        // ── Callback (for non-command uses) ─────────────────────────────────
        std::function<bool(int)> callback;
        bool hasCallback = false;

        // ── Debounce ────────────────────────────────────────────────────────
        uint32_t debounceTime = DEFAULT_GPIO_DEBOUNCE_TIME;
        enum class DebounceEdge {
            Both,    // debounce rising and falling (default)
            Rising,  // debounce only rising; falling fires immediately
            Falling  // debounce only falling; rising fires immediately
        };
        DebounceEdge debounceEdge = DebounceEdge::Both;

        // ── Re-enable after trigger ──────────────────────────────────────────
        // Controls how long the input is suppressed after commands fire.
        enum class ReEnableMode {
            Always,       // always enabled — no suppression (default)
            Timed,        // re-enable after reEnableDelay ms
            OnPlayerIdle  // re-enable once the player returns to idle
        };
        ReEnableMode reEnableMode = ReEnableMode::Always;
        uint32_t reEnableDelay = 0;  // ms; used by Timed mode
        bool inputDisabled = false;  // true while the input is suppressed post-trigger

        void scheduleReEnable();
        void checkReEnable(long long startMs, bool playerHasStarted);

        bool shouldDebounce(int v) const;
        void doAction(int newVal);
        void checkHoldTimer(long long scheduledStart);
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
