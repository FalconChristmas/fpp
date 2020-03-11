/*
/Volumes/opt/fpp/src/makefiles/fpp_so.mk *   GPIO Input/Output handler for Falcon Player (FPP)
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
 
#ifndef __GPIO_H__
#define __GPIO_H__

#include <map>
#include <functional>

#include <httpserver.hpp>
#include <gpiod.h>
#include <jsoncpp/json/json.h>

class PinCapabilities;
class GPIOManager : public httpserver::http_resource {
public:
    static GPIOManager INSTANCE;
    
    virtual const std::shared_ptr<httpserver::http_response> render_GET(const httpserver::http_request &req) override;
    
    static void ConvertOldSettings();
    
    
    void Initialize(std::map<int, std::function<bool(int)>> &callbacks);
    void CheckGPIOInputs(void);
    void Cleanup();
    
private:
    class GPIOState {
    public:
        GPIOState() : pin(nullptr), lastValue(0), lastTriggerTime(0), file(-1) {}
        const PinCapabilities *pin;
        int lastValue;
        long long lastTriggerTime;
        int futureValue;
        
        struct gpiod_line * gpiodLine = nullptr;
        int file;
        Json::Value fallingAction;
        Json::Value risingAction;
        
        void doAction(int newVal);
    };
    
    GPIOManager();
    ~GPIOManager();
    void SetupGPIOInput(std::map<int, std::function<bool(int)>> &callbacks);

    
    std::array<gpiod_chip*, 5> gpiodChips;
    std::vector<GPIOState> pollStates;
    std::vector<GPIOState> eventStates;
    
    bool checkDebounces;
    
    friend class GPIOCommand;
};

int SetupExtGPIO(int gpio, char *mode);
int ExtGPIO(int gpio, char *mode, int value);

#endif //__GPIO_H__
