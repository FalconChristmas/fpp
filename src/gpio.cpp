/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptionsE of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include "fpp-pch.h"

#include <array>
#include <list>
#include <map>
#include <string.h>
#include <string>
#include <vector>

#include "EPollManager.h"
#include "common.h"
#include "log.h"
#include "settings.h"
#include "commands/Commands.h"
#include "util/GPIOUtils.h"

#include "Player.h"
#include "Timers.h"
#include "gpio.h"

GPIOManager GPIOManager::INSTANCE;

class FPPGPIOCommand : public Command {
public:
    FPPGPIOCommand() :
        Command("GPIO") {
        args.push_back(CommandArg("pin", "string", "Pin").setContentListUrl("api/options/GPIOLIST"));
        args.push_back(CommandArg("on", "string", "Action").setContentList({ "On", "Off", "Opposite" }));
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override {
        if (args.size() != 2) {
            return std::make_unique<Command::ErrorResult>("Invalid number of arguments. GPIO needs two arguments.");
        }
        std::string n = args[0];
        std::string v = args[1];
        const PinCapabilities& p = PinCapabilities::getPinByName(n);
        if (p.ptr()) {
            p.configPin();
            if (v == "On" || v == "on" || v == "true" || v == "True" || v == "1") {
                p.setValue(true);
                GPIOManager::INSTANCE.fppCommandLastValue[n] = true;
            } else if (v == "Off" || v == "off" || v == "false" || v == "False" || v == "0") {
                p.setValue(false);
                GPIOManager::INSTANCE.fppCommandLastValue[n] = false;
            } else if (v == "Opposite" || v == "opposite") {
                GPIOManager::INSTANCE.fppCommandLastValue[n] = !GPIOManager::INSTANCE.fppCommandLastValue[n];
                p.setValue(GPIOManager::INSTANCE.fppCommandLastValue[n]);
            } else {
                return std::make_unique<Command::ErrorResult>("Invalid Action" + v);
            }
            return std::make_unique<Command::Result>("OK");
        }
        return std::make_unique<Command::ErrorResult>("No Pin Named " + n);
    }
};

GPIOManager::GPIOManager() {
}
GPIOManager::~GPIOManager() {
}

void GPIOManager::Initialize(std::map<int, std::function<bool(int)>>& callbacks) {
    SetupGPIOInput(callbacks);
    std::vector<std::string> pins = PinCapabilities::getPinNames();
    if (!pins.empty()) {
        CommandManager::INSTANCE.addCommand(new FPPGPIOCommand());
    }
}

// Poll-based pins: detect edges each main loop iteration.
// Non-debounced edges fire immediately. Debounced edges schedule a
// timer via Timers to re-read and confirm after the settle window.
void GPIOManager::CheckGPIOInputs(void) {
    if (pollStates.empty()) {
        return;
    }

    for (auto a : pollStates) {
        // Skip pins that already have a pending debounce timer
        if (a->pendingValue != a->lastValue) {
            continue;
        }
        int val = a->pin->getValue();
        if (val != a->lastValue) {
            if (!a->shouldDebounce(val)) {
                a->doAction(val);
            } else {
                // New transition detected — start the settle timer.
                a->pendingValue = val;
                a->pendingTime = GetTime();
                scheduleDebounceCheck(a);
            }
        }
    }
}

// Timer-driven debounce check for all pins (event-based and poll-based).
// Called when a debounce settle timer fires via Timers/EPollManager.
void GPIOManager::checkDebounceTimers() {
    long long tm = GetTime();
    auto checkList = [&](std::list<GPIOState*>& states) {
        for (auto a : states) {
            if (a->pendingValue != a->lastValue) {
                if ((tm - a->pendingTime) >= (long long)a->debounceTime) {
                    // Settle window elapsed — confirm pin is still at pending value.
                    int val = a->pin->getValue();
                    if (val == a->pendingValue) {
                        a->doAction(val);
                    } else {
                        // Bounced back — cancel.
                        a->pendingValue = a->lastValue;
                        a->pendingTime = 0;
                    }
                } else {
                    // Still within settle window — schedule another check.
                    scheduleDebounceCheck(a);
                }
            }
        }
    };
    checkList(eventStates);
    checkList(pollStates);
}

// Schedule a timer to re-check debounce for an event-based pin.
void GPIOManager::scheduleDebounceCheck(GPIOState* state) {
    long long remainingUS = (long long)state->debounceTime - (GetTime() - state->pendingTime);
    long long remainingMS = (remainingUS + 999) / 1000; // round up to ms
    if (remainingMS < 1) {
        remainingMS = 1;
    }
    std::string timerName = "gpio_db_" + state->pin->name;
    Timers::INSTANCE.addTimer(timerName, GetTimeMS() + remainingMS, [this]() {
        checkDebounceTimers();
    });
}

void GPIOManager::Cleanup() {
    for (auto a : eventStates) {
        if (!a->ledPin.empty())
            Timers::INSTANCE.stopPeriodicTimer("gpio_led_pulse_" + a->pin->name);
        if (a->file != -1)
            a->pin->releaseGPIOD();
        delete a;
    }
    for (auto a : pollStates) {
        if (!a->ledPin.empty())
            Timers::INSTANCE.stopPeriodicTimer("gpio_led_pulse_" + a->pin->name);
        if (a->file != -1)
            a->pin->releaseGPIOD();
        delete a;
    }
    eventStates.clear();
    pollStates.clear();
}

HttpResponsePtr GPIOManager::render_GET(const HttpRequestPtr& req) {
    auto pieces = getPathPieces(req->path());
    int plen = pieces.size();
    std::string p1 = pieces[0];
    if (p1 == "gpio") {
        if (plen <= 1) {
            bool simpleList = false;
            if (getRequestArg(req, "list") == "true") {
                simpleList = true;
            }
            std::vector<std::string> pins = PinCapabilities::getPinNames();
            if (pins.empty()) {
                return makeStringResponse("[]", 200, "application/json");
            }
            Json::Value result;
            for (auto& a : pins) {
                if (simpleList) {
                    result.append(a);
                } else {
                    result.append(PinCapabilities::getPinByName(a).toJSON());
                }
            }
            std::string resultStr = SaveJsonToString(result);
            return makeStringResponse(resultStr, 200, "application/json");
        } else if (plen == 2) {
            // Handle reading individual GPIO pin value: /gpio/{pin_name}
            std::string pinName = pieces[1];
            Json::Value result;

            // Check if we have a tracked value for this pin FIRST (from previous SET operations)
            // This avoids touching the pin hardware which could interfere with output pins
            auto it = GPIOManager::INSTANCE.fppCommandLastValue.find(pinName);
            if (it != GPIOManager::INSTANCE.fppCommandLastValue.end()) {
                // Return the last set value without touching the pin
                LogDebug(VB_HTTP, "GPIO GET: Returning cached value for %s: %d\n", pinName.c_str(), it->second ? 1 : 0);
                result["pin"] = pinName;
                result["value"] = it->second ? 1 : 0;
                result["Status"] = "OK";
                result["respCode"] = 200;

                std::string resultStr = SaveJsonToString(result);
                return makeStringResponse(resultStr, 200, "application/json");
            }

            // No tracked value - pin hasn't been set via API/commands yet
            // Return error rather than trying to read it which could interfere with output pins
            LogWarn(VB_HTTP, "GPIO GET: No cached value for pin %s - cannot read output pins safely\n", pinName.c_str());

            result["pin"] = pinName;
            result["Status"] = "ERROR";
            result["respCode"] = 400;
            result["Message"] = "Pin has no cached value. For output pins, you must SET a value before you can GET it. For input pins, configure them via the GPIO Input configuration page.";

            std::string resultStr = SaveJsonToString(result);
            return makeStringResponse(resultStr, 400, "application/json");
        }
    }
    return makeStringResponse("Not Found", 404, "text/plain");
}

HttpResponsePtr GPIOManager::render_POST(const HttpRequestPtr& req) {
    auto pieces = getPathPieces(req->path());
    int plen = pieces.size();
    std::string p1 = pieces[0];

    if (p1 == "gpio" && plen == 2) {
        // Handle setting individual GPIO pin value: POST /gpio/{pin_name}
        std::string pinName = pieces[1];
        const PinCapabilities& pin = PinCapabilities::getPinByName(pinName);

        if (pin.ptr()) {
            Json::Value data;
            Json::Value result;

            // Parse POST data
            if (getRequestContent(req) != "") {
                if (!LoadJsonFromString(getRequestContent(req), data)) {
                    Json::Value errorResult;
                    errorResult["pin"] = pinName;
                    errorResult["Status"] = "ERROR";
                    errorResult["respCode"] = 400;
                    errorResult["Message"] = "Error parsing POST content";

                    std::string errorStr = SaveJsonToString(errorResult);
                    return makeStringResponse(errorStr, 400, "application/json");
                }
            }

            // Check for required 'value' field
            if (!data.isMember("value")) {
                Json::Value errorResult;
                errorResult["pin"] = pinName;
                errorResult["Status"] = "ERROR";
                errorResult["respCode"] = 400;
                errorResult["Message"] = "'value' field not specified. Use {\"value\": 0} or {\"value\": 1}";

                std::string errorStr = SaveJsonToString(errorResult);
                return makeStringResponse(errorStr, 400, "application/json");
            }

            try {
                // Configure the pin for output
                pin.configPin("gpio", true, "GPIO Output");

                // Set the value (convert to boolean)
                bool value = data["value"].asBool() || (data["value"].isInt() && data["value"].asInt() != 0);
                pin.setValue(value);

                // Update the last value tracking (for Opposite command functionality and GET requests)
                GPIOManager::INSTANCE.fppCommandLastValue[pinName] = value;
                LogDebug(VB_HTTP, "GPIO POST: Set pin %s to %d, cached value\n", pinName.c_str(), value ? 1 : 0);

                result["pin"] = pinName;
                result["value"] = value ? 1 : 0;
                result["Status"] = "OK";
                result["respCode"] = 200;
                result["Message"] = "GPIO pin value set successfully";

                std::string resultStr = SaveJsonToString(result);
                return makeStringResponse(resultStr, 200, "application/json");
            } catch (const std::exception& e) {
                Json::Value errorResult;
                errorResult["pin"] = pinName;
                errorResult["Status"] = "ERROR";
                errorResult["respCode"] = 500;
                errorResult["Message"] = std::string("Error setting GPIO pin: ") + e.what();

                std::string errorStr = SaveJsonToString(errorResult);
                return makeStringResponse(errorStr, 500, "application/json");
            }
        } else {
            Json::Value errorResult;
            errorResult["pin"] = pinName;
            errorResult["Status"] = "ERROR";
            errorResult["respCode"] = 404;
            errorResult["Message"] = "GPIO pin not found: " + pinName;

            std::string errorStr = SaveJsonToString(errorResult);
            return makeStringResponse(errorStr, 404, "application/json");
        }
    }

    return makeStringResponse("Not Found", 404, "text/plain");
}

void GPIOManager::SetupGPIOInput(std::map<int, std::function<bool(int)>>& callbacks) {
    LogDebug(VB_GPIO, "SetupGPIOInput()\n");

    int enabledCount = 0;

    std::string file = FPP_DIR_CONFIG("/gpio.json");
    if (FileExists(file)) {
        Json::Value root;
        if (LoadJsonFromFile(file, root)) {
            for (int x = 0; x < root.size(); x++) {
                Json::Value v = root[x];
                if (!v["enabled"].asBool()) {
                    continue;
                }
                std::string pin = v.isMember("pin") ? v["pin"].asString() : "";
                std::string mode = v.isMember("mode") ? v["mode"].asString() : "gpio";
                const PinCapabilities* pc = PinCapabilities::getPinByName(pin).ptr();
                if (pc) {
                    GPIOState* state = new GPIOState();
                    if (v.isMember("debounceTime")) {
                        state->debounceTime = v["debounceTime"].asInt() * 1000;
                    }

                    // Load debounce edge setting: "both" (default), "rising", or "falling"
                    if (v.isMember("debounceEdge")) {
                        std::string edge = v["debounceEdge"].asString();
                        if (edge == "rising") {
                            state->debounceEdge = GPIOState::DebounceEdge::Rising;
                        } else if (edge == "falling") {
                            state->debounceEdge = GPIOState::DebounceEdge::Falling;
                        } else {
                            state->debounceEdge = GPIOState::DebounceEdge::Both;
                        }
                    }

                    state->pin = pc;

                    // Parse rising/falling: accept both old single-object format and
                    // new array-of-commands format for backward compatibility.
                    auto parseActions = [](const Json::Value& jv, const std::string& key) -> std::vector<Json::Value> {
                        std::vector<Json::Value> result;
                        if (!jv.isMember(key))
                            return result;
                        const Json::Value& actions = jv[key];
                        if (actions.isArray()) {
                            for (int i = 0; i < (int)actions.size(); i++) {
                                std::string cmd = actions[i]["command"].asString();
                                if (!cmd.empty() && cmd != "OLED Navigation")
                                    result.push_back(actions[i]);
                            }
                        } else if (actions.isObject()) {
                            std::string cmd = actions["command"].asString();
                            if (!cmd.empty() && cmd != "OLED Navigation")
                                result.push_back(actions);
                        }
                        return result;
                    };

                    state->risingActions = parseActions(v, "rising");
                    state->fallingActions = parseActions(v, "falling");
                    state->holdActions = parseActions(v, "hold");

                    if (v.isMember("holdTime"))
                        state->holdTime = v["holdTime"].asInt();

                    if (v.isMember("ledPin"))
                        state->ledPin = v["ledPin"].asString();

                    if (v.isMember("ledActiveHigh"))
                        state->ledActiveHigh = v["ledActiveHigh"].asBool();

                    // LED idle mode — what the LED does when not in a trigger effect
                    if (v.isMember("ledIdleMode")) {
                        std::string m = v["ledIdleMode"].asString();
                        if (m == "on")          state->ledIdleMode = GPIOState::LEDIdleMode::On;
                        else if (m == "pulse")  state->ledIdleMode = GPIOState::LEDIdleMode::Pulse;
                        else                    state->ledIdleMode = GPIOState::LEDIdleMode::Off;
                    }
                    if (v.isMember("ledPulseRateMs"))
                        state->ledPulseRateMs = std::max(50u, v["ledPulseRateMs"].asUInt());

                    // LED trigger mode — what happens to the LED when the button fires
                    if (v.isMember("ledTriggerMode")) {
                        std::string m = v["ledTriggerMode"].asString();
                        if (m == "none")              state->ledTriggerMode = GPIOState::LEDTriggerMode::None;
                        else if (m == "flash")        state->ledTriggerMode = GPIOState::LEDTriggerMode::Flash;
                        else if (m == "timed_on")     state->ledTriggerMode = GPIOState::LEDTriggerMode::TimedOn;
                        else                          state->ledTriggerMode = GPIOState::LEDTriggerMode::FollowInput;
                    }
                    if (v.isMember("ledTriggerParam"))
                        state->ledTriggerParam = v["ledTriggerParam"].asUInt();

                    // Re-enable mode — controls how long input is suppressed post-trigger.
                    if (v.isMember("reEnableMode")) {
                        std::string m = v["reEnableMode"].asString();
                        if (m == "timed")
                            state->reEnableMode = GPIOState::ReEnableMode::Timed;
                        else if (m == "player_idle")
                            state->reEnableMode = GPIOState::ReEnableMode::OnPlayerIdle;
                        else
                            state->reEnableMode = GPIOState::ReEnableMode::Always;
                    }
                    if (v.isMember("reEnableDelay"))
                        state->reEnableDelay = v["reEnableDelay"].asUInt();

                    bool hasAnyAction = !state->risingActions.empty() ||
                                       !state->fallingActions.empty() ||
                                       !state->holdActions.empty();
                    if (hasAnyAction) {
                        state->pin->configPin(mode, false, "GPIO Input");

                        addState(state);
                        enabledCount++;
                    } else {
                        delete state;
                    }
                }
            }
        }
    }
    LogDebug(VB_GPIO, "%d GPIO Input(s) enabled\n", enabledCount);

    // Apply initial LED idle state for all configured inputs.
    for (auto* s : eventStates) s->initLED();
    for (auto* s : pollStates)  s->initLED();
}

// Event callback uses settle-then-fire logic.
// On a debounced edge, we record pendingValue/pendingTime and schedule a
// timer to re-check after the settle window. Non-debounced edges fire immediately.
void GPIOManager::addGPIOCallback(GPIOState* a) {
    std::function<bool(int)> f = [a, this](int i) {
        int v = a->pin->readEventFromFile();
        if (v < 0) {
            return false;
        }
        if (v != a->lastValue) {
            if (!a->shouldDebounce(v)) {
                // No debounce on this edge — fire immediately.
                a->doAction(v);
            } else {
                // Settle-then-fire: start the settle timer if this is a new direction.
                if (v != a->pendingValue) {
                    a->pendingValue = v;
                    a->pendingTime = GetTime();
                }
                scheduleDebounceCheck(a);
            }
        }
        return false;
    };
    EPollManager::INSTANCE.addFileDescriptor(a->file, f);
}

void GPIOManager::AddGPIOCallback(const PinCapabilities* pin, const std::function<bool(int)>& cb) {
    GPIOState* state = new GPIOState();
    state->pin = pin;
    state->callback = cb;
    state->hasCallback = true;
    addState(state);
}

void GPIOManager::RemoveGPIOCallback(const PinCapabilities* pin) {
    for (auto it = eventStates.begin(); it != eventStates.end(); ++it) {
        GPIOState* a = *it;
        if (a->pin == pin) {
            if (a->file != -1) {
                EPollManager::INSTANCE.removeFileDescriptor(a->file);
                a->pin->releaseGPIOD();
            }
            eventStates.erase(it);
            delete a;
            return;
        }
    }
    for (auto it = pollStates.begin(); it != pollStates.end(); ++it) {
        GPIOState* a = *it;
        if (a->pin == pin) {
            pollStates.erase(it);
            delete a;
            return;
        }
    }
}

// pendingValue is initialised to match lastValue so no spurious settle
// is pending at boot.
void GPIOManager::addState(GPIOState* state) {
    int currentVal = state->pin->getValue();
    state->lastValue = currentVal;
    state->futureValue = currentVal;   // kept for compatibility
    state->pendingValue = currentVal;  // no pending transition at startup
    state->pendingTime = 0;
    state->lastTriggerTime = 0;
    state->file = -1;

    if (state->pin->supportsGpiod()) {
        // Request events for any edge that has actions or is needed for hold detection.
        bool wantRising = !state->risingActions.empty() || !state->holdActions.empty();
        bool wantFalling = !state->fallingActions.empty() || !state->holdActions.empty();
        state->file = state->pin->requestEventFile(wantRising, wantFalling);
    } else {
        state->file = -1;
    }

    if (state->file > 0) {
        eventStates.push_back(state);
        addGPIOCallback(state);
    } else {
        pollStates.push_back(state);
    }
}

void GPIOManager::GPIOState::doAction(int v) {
    LogDebug(VB_GPIO, "GPIO %s triggered.  Value:  %d\n", pin->name.c_str(), v);

    // When the input is suppressed post-trigger, skip all commands and LED
    // effects — the LED is already parked off by startLEDIdle() and should
    // stay that way until the input is re-enabled.
    if (inputDisabled) {
        lastTriggerTime = GetTime();
        lastValue = v;
        futureValue = v;
        pendingValue = v;
        pendingTime = 0;
        return;
    }

    if (hasCallback) {
        callback(v);
    } else {
        if (v == 1) {
            // Rising edge — run rising commands, then schedule hold timer if needed.
            for (auto& action : risingActions) {
                CommandManager::INSTANCE.run(action);
            }
            holdFired = false;
            holdStartTime = GetTimeMS();
            if (holdTime > 0 && !holdActions.empty()) {
                long long scheduled = holdStartTime;
                Timers::INSTANCE.addTimer(
                    "gpio_hold_" + pin->name,
                    GetTimeMS() + holdTime,
                    [this, scheduled]() { checkHoldTimer(scheduled); });
            }
            // Suppress the input until re-enable condition is met.
            if (reEnableMode != ReEnableMode::Always) {
                inputDisabled = true;
                scheduleReEnable();
            }
        } else {
            // Falling edge — cancel any pending hold (by resetting holdStartTime),
            // then run falling commands only if hold has not already fired.
            bool wasHeld = holdFired;
            holdStartTime = 0;
            holdFired = false;
            if (!wasHeld) {
                for (auto& action : fallingActions) {
                    CommandManager::INSTANCE.run(action);
                }
            }
        }

        // Drive the associated LED output if one is configured.
        triggerLEDEffect(v);
    }
    lastTriggerTime = GetTime();
    lastValue = v;
    futureValue = v;
    // Clear pending state now that the action has fired.
    pendingValue = v;
    pendingTime = 0;
}

// Called by the hold timer.  Only fires hold actions when the button is still
// pressed and belongs to the same press session that scheduled this timer.
void GPIOManager::GPIOState::checkHoldTimer(long long scheduledStart) {
    if (holdStartTime != scheduledStart || holdFired || lastValue != 1)
        return;
    LogDebug(VB_GPIO, "GPIO %s hold timer fired.\n", pin->name.c_str());
    for (auto& action : holdActions) {
        CommandManager::INSTANCE.run(action);
    }
    holdFired = true;
}

// Returns true if debounce should apply to this edge.
// v == 1 is rising, v == 0 is falling.
bool GPIOManager::GPIOState::shouldDebounce(int v) const {
    switch (debounceEdge) {
        case DebounceEdge::Rising:
            return (v == 1);
        case DebounceEdge::Falling:
            return (v == 0);
        case DebounceEdge::Both:
        default:
            return true;
    }
}

// ── LED helpers ──────────────────────────────────────────────────────────────

void GPIOManager::GPIOState::setLEDState(bool on) {
    if (ledPin.empty()) return;
    const PinCapabilities* ledCap = PinCapabilities::getPinByName(ledPin).ptr();
    if (!ledCap) return;
    bool pinState = on ? ledActiveHigh : !ledActiveHigh;
    ledCap->configPin("gpio", true);
    ledCap->setValue(pinState);
    GPIOManager::INSTANCE.fppCommandLastValue[ledPin] = pinState;
}

void GPIOManager::GPIOState::stopLEDIdle() {
    if (ledPin.empty()) return;
    Timers::INSTANCE.stopPeriodicTimer("gpio_led_pulse_" + pin->name);
}

void GPIOManager::GPIOState::startLEDIdle() {
    if (ledPin.empty()) return;
    stopLEDIdle();
    if (inputDisabled) {
        // Input is suppressed — hold the LED off so it visually reflects
        // that the input is not yet ready for another trigger.
        setLEDState(false);
        return;
    }
    switch (ledIdleMode) {
        case LEDIdleMode::Off:
            setLEDState(false);
            break;
        case LEDIdleMode::On:
            setLEDState(true);
            break;
        case LEDIdleMode::Pulse:
            ledPulseState = false;
            setLEDState(false);
            Timers::INSTANCE.addPeriodicTimer(
                "gpio_led_pulse_" + pin->name,
                ledPulseRateMs,
                [this]() {
                    ledPulseState = !ledPulseState;
                    setLEDState(ledPulseState);
                });
            break;
    }
}

void GPIOManager::GPIOState::initLED() {
    if (ledPin.empty()) return;
    // Start in idle state; trigger effects will override when the button fires.
    startLEDIdle();
}

void GPIOManager::GPIOState::triggerLEDEffect(int v) {
    if (ledPin.empty()) return;
    switch (ledTriggerMode) {
        case LEDTriggerMode::None:
            // LED ignores trigger events; idle mode drives it.
            break;
        case LEDTriggerMode::FollowInput:
            stopLEDIdle();
            setLEDState(v == 1);
            if (v == 0) startLEDIdle();  // restore idle when released
            break;
        case LEDTriggerMode::Flash:
            if (v == 1) {
                stopLEDIdle();
                uint32_t count = (ledTriggerParam > 0) ? ledTriggerParam : 3;
                doLEDFlash((int)(count * 2));  // 2 half-periods per flash
            }
            break;
        case LEDTriggerMode::TimedOn:
            if (v == 1) {
                stopLEDIdle();
                setLEDState(true);
                uint32_t dur = (ledTriggerParam > 0) ? ledTriggerParam : 1000;
                Timers::INSTANCE.addTimer(
                    "gpio_led_timed_" + pin->name,
                    GetTimeMS() + dur,
                    [this]() { startLEDIdle(); });
            }
            break;
    }
}

// ── Re-enable helpers ────────────────────────────────────────────────────────

void GPIOManager::GPIOState::scheduleReEnable() {
    std::string timerName = "gpio_reenable_" + pin->name;
    switch (reEnableMode) {
        case ReEnableMode::Always:
            inputDisabled = false;
            break;
        case ReEnableMode::Timed: {
            uint32_t delay = (reEnableDelay > 0) ? reEnableDelay : 1000;
            Timers::INSTANCE.addTimer(timerName, GetTimeMS() + delay, [this]() {
                inputDisabled = false;
                startLEDIdle();
                LogDebug(VB_GPIO, "GPIO %s re-enabled after timed delay.\n", pin->name.c_str());
            });
            break;
        }
        case ReEnableMode::OnPlayerIdle: {
            // Give the player 200 ms to start before polling.
            long long startMs = GetTimeMS();
            Timers::INSTANCE.addTimer(timerName, startMs + 200, [this, startMs]() {
                checkReEnable(startMs, false);
            });
            break;
        }
    }
}

void GPIOManager::GPIOState::checkReEnable(long long startMs, bool playerHasStarted) {
    if (!inputDisabled) return;
    std::string timerName = "gpio_reenable_" + pin->name;
    long long now = GetTimeMS();
    PlaylistStatus status = Player::INSTANCE.GetStatus();

    if (status != FPP_STATUS_IDLE) {
        // Player is active — keep polling every 500 ms.
        Timers::INSTANCE.addTimer(timerName, now + 500, [this, startMs]() {
            checkReEnable(startMs, true);
        });
    } else if (playerHasStarted) {
        // Player started and has returned to idle — safe to re-enable.
        inputDisabled = false;
        startLEDIdle();
        LogDebug(VB_GPIO, "GPIO %s re-enabled: player returned to idle.\n", pin->name.c_str());
    } else if (now - startMs > 2000) {
        // Player never started within 2 s (action wasn't a playlist) — re-enable anyway.
        inputDisabled = false;
        startLEDIdle();
        LogDebug(VB_GPIO, "GPIO %s re-enabled: player-idle timeout.\n", pin->name.c_str());
    } else {
        // Still waiting for player to start — check again soon.
        Timers::INSTANCE.addTimer(timerName, now + 200, [this, startMs]() {
            checkReEnable(startMs, false);
        });
    }
}

// Countdown-based LED flash: halfPeriodsRemaining decrements each 150 ms half-period.
// Even remaining → LED on; odd → LED off.  When 0 → restore idle.
void GPIOManager::GPIOState::doLEDFlash(int halfPeriodsRemaining) {
    if (halfPeriodsRemaining <= 0) {
        startLEDIdle();
        return;
    }
    setLEDState(halfPeriodsRemaining % 2 == 0);
    Timers::INSTANCE.addTimer(
        "gpio_led_flash_" + pin->name,
        GetTimeMS() + 150,
        [this, halfPeriodsRemaining]() { doLEDFlash(halfPeriodsRemaining - 1); });
}
