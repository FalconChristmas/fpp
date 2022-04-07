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

#ifdef HAS_GPIOD
#include <gpiod.h>
#else 
struct gpiod_line {};
struct gpiod_chip {};
#endif

class PinCapabilities;
class GPIOManager : public httpserver::http_resource {
public:
    static GPIOManager INSTANCE;

    virtual const std::shared_ptr<httpserver::http_response> render_GET(const httpserver::http_request& req) override;

    void Initialize(std::map<int, std::function<bool(int)>>& callbacks);
    void CheckGPIOInputs(void);
    void Cleanup();

private:
    class GPIOState {
    public:
        GPIOState() :
            pin(nullptr),
            lastValue(0),
            lastTriggerTime(0),
            file(-1) {}
        const PinCapabilities* pin;
        int lastValue;
        long long lastTriggerTime;
        int futureValue;

        struct gpiod_line* gpiodLine = nullptr;
        int file;
        Json::Value fallingAction;
        Json::Value risingAction;

        void doAction(int newVal);
    };

    GPIOManager();
    ~GPIOManager();
    void SetupGPIOInput(std::map<int, std::function<bool(int)>>& callbacks);

    std::array<gpiod_chip*, 5> gpiodChips;
    std::vector<GPIOState> pollStates;
    std::vector<GPIOState> eventStates;

    bool checkDebounces;

    friend class GPIOCommand;
};

int SetupExtGPIO(int gpio, char* mode);
int ExtGPIO(int gpio, char* mode, int value);
