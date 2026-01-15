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

#include "fpp-pch.h"

#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <thread>

#include "GPIOUtils.h"
#include "../Warnings.h"
#include "../log.h"
#include "commands/Commands.h"

// No platform information on how to control pins
static NoPinCapabilities NULL_PIN_INSTANCE("-none-", 0);

const NoPinCapabilities& NoPinCapabilitiesProvider::getPinByName(const std::string& name) { return NULL_PIN_INSTANCE; }
const NoPinCapabilities& NoPinCapabilitiesProvider::getPinByGPIO(int chip, int gpio) { return NULL_PIN_INSTANCE; }
const NoPinCapabilities& NoPinCapabilitiesProvider::getPinByUART(const std::string& n) { return NULL_PIN_INSTANCE; }

Json::Value PinCapabilities::toJSON() const {
    Json::Value ret;
    if (name != "" && name != "-non-") {
        ret["pin"] = name;
        ret["gpioChip"] = gpioIdx;
        ret["gpioLine"] = gpio;
        if (gpioIdx == 0) {
            // somewhat for compatibility with the old kernel GPIO numbers on the Pi's
            ret["gpio"] = gpio;
        }
        if (pwm != -1) {
            ret["pwm"] = pwm;
            ret["subPwm"] = subPwm;
        }
        if (i2cBus != -1) {
            ret["i2c"] = i2cBus;
        }
        if (uart != "") {
            ret["uart"] = uart;
        }
        ret["supportsPullUp"] = supportsPullUp();
        ret["supportsPullDown"] = supportsPullDown();
    }
    return ret;
}

void PinCapabilities::enableOledScreen(int i2cBus, bool enable) {
    // this pin is i2c, we may need to tell fppoled to turn off the display
    // before we shutdown this pin because once we re-configure, i2c will
    // be unavailable and the display won't update
    int smfd = shm_open("fppoled", O_CREAT | O_RDWR, 0);
    ftruncate(smfd, 1024);
    unsigned int* status = (unsigned int*)mmap(0, 1024, PROT_WRITE | PROT_READ, MAP_SHARED, smfd, 0);
    if (i2cBus == status[0]) {
        if (!enable) {
            // force the display off
            status[2] = 1;
            int count = 0;
            while (status[1] != 0 && count < 150) {
                count++;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        } else {
            // allow the display to come back on
            status[2] = 0;
        }
    }
    close(smfd);
    munmap(status, 1024);
}

// the built in GPIO chips that are handled by the more optimized
// platform specific GPIO drivers
static const std::set<std::string> PLATFORM_IGNORES{
    "pinctrl-bcm2835", // raspberry pi's
    "pinctrl-bcm2711", // Pi4
    "raspberrypi-exp-gpio",
    "brcmvirt-gpio",
    "gpio-0-31", // beagles
    "gpio-32-63",
    "gpio-64-95",
    "gpio-96-127",
    "gpio-brcmstb@107d508500", // Pi5's internal GPIO chips
    "gpio-brcmstb@107d508520",
    "gpio-brcmstb@107d517c00",
    "gpio-brcmstb@107d517c20",
    "pinctrl-rp1",   // Pi5's external GPIO chip
    "tps65219-gpio", // AM62x
    "4201000.gpio",
    "600000.gpio",
    "601000.gpio"
};
// No platform information on how to control pins
static std::string PROCESS_NAME = "FPPD";

bool GPIODCapabilities::supportsPullUp() const {
    return true;
}
bool GPIODCapabilities::supportsPullDown() const {
    return true;
}

#ifdef HASGPIOD
constexpr int MAX_GPIOD_CHIPS = 32;
class GPIOChipHolder {
public:
    static GPIOChipHolder INSTANCE;

#ifdef IS_GPIOD_CXX_V2
    std::array<std::shared_ptr<gpiod::chip>, MAX_GPIOD_CHIPS> chips;
#else
    std::array<gpiod::chip, MAX_GPIOD_CHIPS> chips;
#endif
};
GPIOChipHolder GPIOChipHolder::INSTANCE;
#endif
int GPIODCapabilities::configPin(const std::string& mode,
                                 bool directionOut,
                                 const std::string& desc) const {
#ifdef HASGPIOD
    // printf("Configuring %s %d %d %s\n", name.c_str(), gpioIdx, gpio, mode.c_str());
#ifdef IS_GPIOD_CXX_V2
    if (chip == nullptr) {
        if (!GPIOChipHolder::INSTANCE.chips[gpioIdx]) {
            try {
                GPIOChipHolder::INSTANCE.chips[gpioIdx] = std::make_shared<gpiod::chip>("/dev/gpiochip" + std::to_string(gpioIdx));
            } catch (const std::exception& ex) {
                std::string w = "Could not open GPIO chip " + (gpioName.empty() ? std::to_string(gpioIdx) : gpioName) +
                                " (" + ex.what() + ")";
                WarningHolder::AddWarning(w);
                LogWarn(VB_GPIO, "%s\n", w.c_str());
                return -1;
            }
        }
        chip = GPIOChipHolder::INSTANCE.chips[gpioIdx];
    }

    try {
        gpiod::line_settings settings;

        if (directionOut) {
            settings.set_direction(gpiod::line::direction::OUTPUT);
        } else {
            settings.set_direction(gpiod::line::direction::INPUT);
            if (mode == "gpio_pu") {
                settings.set_bias(gpiod::line::bias::PULL_UP);
            } else if (mode == "gpio_pd") {
                settings.set_bias(gpiod::line::bias::PULL_DOWN);
            } else {
                settings.set_bias(gpiod::line::bias::DISABLED);
            }
        }

        std::string consumer = PROCESS_NAME;
        if (!desc.empty()) {
            consumer += "-" + desc;
        }
        lastDesc = consumer;

        gpiod::request_builder builder = chip->prepare_request();
        builder.add_line_settings(gpio, settings);
        builder.set_consumer(consumer);

        if (!request || lastRequestType != (directionOut ? 1 : 0)) {
            request = std::make_shared<gpiod::line_request>(builder.do_request());
            lastRequestType = directionOut ? 1 : 0;
        } else {
            // Update existing request with new settings
            auto new_req = builder.do_request();
            request = std::make_shared<gpiod::line_request>(std::move(new_req));
        }
    } catch (const std::exception& ex) {
        std::string w = "Could not configure pin " + name + "(" + desc + ") as " + mode + " (" + ex.what() + ")";
        WarningHolder::AddWarning(w);
        LogWarn(VB_GPIO, "%s\n", w.c_str());
        lastRequestType = 0;
    }
#else
    // printf("Configuring %s %d %d %s\n", name.c_str(), gpioIdx, gpio, mode.c_str());
    if (chip == nullptr) {
        if (!GPIOChipHolder::INSTANCE.chips[gpioIdx]) {
            if (gpioName.empty()) {
                std::string n = std::to_string(gpioIdx);
                GPIOChipHolder::INSTANCE.chips[gpioIdx].open(n, gpiod::chip::OPEN_BY_NUMBER);
            } else {
                GPIOChipHolder::INSTANCE.chips[gpioIdx].open(gpioName, gpiod::chip::OPEN_BY_NAME);
            }
        }
        chip = &GPIOChipHolder::INSTANCE.chips[gpioIdx];
        line = chip->get_line(gpio);
    }
    gpiod::line_request req;
    req.consumer = PROCESS_NAME;
    if (!desc.empty()) {
        req.consumer += "-" + desc;
    }
    lastDesc = req.consumer;
    if (directionOut) {
        req.request_type = gpiod::line_request::DIRECTION_OUTPUT;
    } else {
        req.request_type = gpiod::line_request::DIRECTION_INPUT;
        if (mode == "gpio_pu") {
            req.flags |= gpiod::line_request::FLAG_BIAS_PULL_UP;
        } else if (mode == "gpio_pd") {
            req.flags |= gpiod::line_request::FLAG_BIAS_PULL_DOWN;
        } else {
            req.flags |= gpiod::line_request::FLAG_BIAS_DISABLE;
        }
    }
    if (req.request_type != lastRequestType) {
        if (line.is_requested()) {
            line.release();
        }
        try {
            line.request(req, 1);
            lastRequestType = req.request_type;
        } catch (const std::exception& ex) {
            std::string w = "Could not configure pin " + name + "(" + desc + ") as " + mode + " (" + ex.what() + ")";
            WarningHolder::AddWarning(w);
            LogWarn(VB_GPIO, "%s\n", w.c_str());
            lastRequestType = 0;
        }
    }
#endif
#endif
    return 0;
}
void GPIODCapabilities::releaseGPIOD() const {
#ifdef HASGPIOD
#ifdef IS_GPIOD_CXX_V2
    if (request) {
        request->release();
        request = nullptr;
        lastRequestType = 0;
        lastDesc.clear();
    }
#else
    if (line.is_requested()) {
        line.release();
        lastRequestType = 0;
        lastDesc.clear();
    }
#endif
#endif
}

int GPIODCapabilities::readEventFromFile() const {
#ifdef HASGPIOD
#ifdef IS_GPIOD_CXX_V2
    if (request) {
        try {
            gpiod::edge_event_buffer buffer(1);
            int count = request->read_edge_events(buffer);
            if (count == 1) {
                auto& event = buffer.get_event(0);
                return event.type() == gpiod::edge_event::event_type::RISING_EDGE ? 1 : 0;
            }
        } catch (const std::exception& ex) {
            LogWarn(VB_GPIO, "Error reading event for pin %s: %s\n", name.c_str(), ex.what());
        }
    }
#else
    struct gpiod_line_event event;
    if (gpiod_line_event_read_fd(line.event_get_fd(), &event) >= 0) {
        return event.event_type == GPIOD_LINE_EVENT_RISING_EDGE;
    }
#endif
#endif
    return -1;
}

int GPIODCapabilities::requestEventFile(bool risingEdge, bool fallingEdge) const {
    int fd = -1;
#ifdef HASGPIOD
#ifdef IS_GPIOD_CXX_V2
    try {
        releaseGPIOD();

        gpiod::line_settings settings;
        settings.set_direction(gpiod::line::direction::INPUT);

        if (risingEdge && fallingEdge) {
            settings.set_edge_detection(gpiod::line::edge::BOTH);
        } else if (risingEdge) {
            settings.set_edge_detection(gpiod::line::edge::RISING);
        } else if (fallingEdge) {
            settings.set_edge_detection(gpiod::line::edge::FALLING);
        }

        std::string consumer = lastDesc.empty() ? PROCESS_NAME : lastDesc;

        gpiod::request_builder builder = chip->prepare_request();
        builder.add_line_settings(gpio, settings);
        builder.set_consumer(consumer);

        request = std::make_shared<gpiod::line_request>(builder.do_request());

        fd = request->fd();
        if (fd < 0) {
            WarningHolder::AddWarning("Could not get event file descriptor for pin " + name);
            request = nullptr;
        }
    } catch (const std::exception& ex) {
        WarningHolder::AddWarning("Could not configure pin " + name + " for events (" + ex.what() + ") for edges Rising:" +
                                  std::to_string(risingEdge) + "   Falling:" + std::to_string(fallingEdge));
    }
#else
    gpiod::line_request req;
    req.consumer = lastDesc.empty() ? PROCESS_NAME : lastDesc;
    req.request_type = lastRequestType;
    if (risingEdge && fallingEdge) {
        req.request_type |= gpiod::line_request::EVENT_BOTH_EDGES;
    } else if (risingEdge) {
        req.request_type |= gpiod::line_request::EVENT_RISING_EDGE;
    } else if (fallingEdge) {
        req.request_type |= gpiod::line_request::EVENT_FALLING_EDGE;
    }
    bool wasRequested = line.is_requested();
    if (wasRequested) {
        line.release();
    }
    try {
        line.request(req, 0);
    } catch (const std::exception& ex) {
        if (!wasRequested) {
            WarningHolder::AddWarning("Could not configure pin " + name + " for events (" + ex.what() + ") for edges Rising:" +
                                      std::to_string(risingEdge) + "   Falling:" + std::to_string(fallingEdge));
        }
    }
    if (line.is_requested()) {
        fd = line.event_get_fd();
        if (fd < 0) {
            WarningHolder::AddWarning("Could not get event file descriptor for pin " + name);
        }
    } else if (wasRequested) {
        req.request_type = lastRequestType;
        line.request(req, 0);
        if (!line.is_requested()) {
            WarningHolder::AddWarning("Could not re-request pin " + name + " after failing to request events");
        }
    } else {
        WarningHolder::AddWarning("Could not request event for pin " + name);
    }
#endif
#endif
    return fd;
}

bool GPIODCapabilities::getValue() const {
#ifdef HASGPIOD
    if (lastRequestType == 1) { // OUTPUT
        return lastValue;
    }
#ifdef IS_GPIOD_CXX_V2
    if (lastRequestType == 0 || !request) {
        return 0;
    }
    try {
        auto v = request->get_value(gpio);
        return v == gpiod::line::value::ACTIVE;
    } catch (const std::exception& ex) {
        LogWarn(VB_GPIO, "Error getting value for pin %s: %s\n", name.c_str(), ex.what());
        return 0;
    }
#else
    if (lastRequestType == 0 && !line.is_requested()) {
        return 0;
    }
    return line.get_value();
#endif
#else
    return 0;
#endif
}
void GPIODCapabilities::setValue(bool i) const {
#ifdef HASGPIOD
    lastValue = i;
#ifdef IS_GPIOD_CXX_V2
    if (request) {
        try {
            request->set_value(gpio, i ? gpiod::line::value::ACTIVE : gpiod::line::value::INACTIVE);
        } catch (const std::exception& ex) {
            LogWarn(VB_GPIO, "Error setting value for pin %s: %s\n", name.c_str(), ex.what());
        }
    }
#else
    line.set_value(i ? 1 : 0);
#endif
#endif
}
static std::vector<GPIODCapabilities> GPIOD_PINS;
static PinCapabilitiesProvider* PIN_PROVIDER = nullptr;

void PinCapabilities::InitGPIO(const std::string& process, PinCapabilitiesProvider* p) {
    PROCESS_NAME = process;
    if (p == nullptr) {
        p = new NoPinCapabilitiesProvider();
    }
    if (PIN_PROVIDER != nullptr) {
        delete PIN_PROVIDER;
    }
    PIN_PROVIDER = p;
#ifdef HASGPIOD
    int chipCount = 0;
    int pinCount = 0;
    try {
        if (GPIOD_PINS.empty()) {
            // has at least one chip
            std::set<std::string> found;
#ifdef IS_GPIOD_CXX_V2
            for (const auto& entry : std::filesystem::directory_iterator("/dev/")) {
                if (startsWith(entry.path().filename().string(), "gpiochip")) {
                    auto chip = gpiod::chip(entry.path().string());
                    auto info = chip.get_info();
                    std::string name = info.name();
                    std::string label = info.label();
                    int chipNum = std::stoi(entry.path().filename().string().substr(8));

                    if (PLATFORM_IGNORES.find(label) == PLATFORM_IGNORES.end()) {
                        char ci = 'b';
                        while (found.find(label) != found.end()) {
                            label = chip.get_info().label() + ci;
                            ci++;
                        }
                        found.insert(label);
                        int num_lines = chip.get_info().num_lines();
                        for (int x = 0; x < num_lines; x++) {
                            std::string n = label + "-" + std::to_string(x);
                            auto line = chip.get_line_info(x);
                            std::string plabel = line.name();
                            if (plabel.empty()) {
                                plabel = n;
                            }
                            GPIOD_PINS.push_back(GPIODCapabilities(plabel, pinCount + x, name).setGPIO(chipNum, x));
                        }
                    }
                    pinCount += chip.get_info().num_lines();
                    chipCount++;
                }
            }
#else
            ::gpiod_chip* chip = gpiod_chip_open_by_number(0);
            if (chip != nullptr) {
                ::gpiod_chip_close(chip);
                if (GPIOD_PINS.empty()) {
                    // has at least one chip
                    std::set<std::string> found;
                    for (auto& a : gpiod::make_chip_iter()) {
                        std::string name = a.name();
                        std::string label = a.label();

                        if (PLATFORM_IGNORES.find(label) == PLATFORM_IGNORES.end()) {
                            char i = 'b';
                            while (found.find(label) != found.end()) {
                                label = a.label() + i;
                                i++;
                            }
                            found.insert(label);
                            for (int x = 0; x < a.num_lines(); x++) {
                                std::string n = label + "-" + std::to_string(x);
                                std::string plabel = a.get_line(x).name();
                                if (plabel.empty()) {
                                    plabel = n;
                                }
                                GPIOD_PINS.push_back(GPIODCapabilities(plabel, pinCount + x, name).setGPIO(chipCount, x));
                            }
                        }
                        pinCount += a.num_lines();
                        chipCount++;
                    }
                }
            }
#endif
        }
    } catch (const std::exception& ex) {
        LogWarn(VB_GPIO, "Error initializing GPIO: %s\n", ex.what());
    }
#endif
    PIN_PROVIDER->Init();
}
std::vector<std::string> PinCapabilities::getPinNames() {
    std::vector<std::string> pn = PIN_PROVIDER->getPinNames();
    for (auto& a : GPIOD_PINS) {
        pn.push_back(a.name);
    }
    return pn;
}

const PinCapabilities& PinCapabilities::getPinByName(const std::string& n) {
    for (auto& a : GPIOD_PINS) {
        if (n == a.name) {
            return a;
        }
    }
    return PIN_PROVIDER->getPinByName(n);
}
const PinCapabilities& PinCapabilities::getPinByGPIO(int chip, int gpio) {
    for (auto& a : GPIOD_PINS) {
        if (a.gpioIdx == chip && a.gpio == gpio) {
            return a;
        }
    }
    return PIN_PROVIDER->getPinByGPIO(chip, gpio);
}
const PinCapabilities& PinCapabilities::getPinByUART(const std::string& n) {
    return PIN_PROVIDER->getPinByUART(n);
}

void PinCapabilities::SetMultiPinValue(const std::list<const PinCapabilities*>& pins, int v) {
    if (pins.empty()) {
        return;
    }
    for (auto p : pins) {
        p->setValue(v);
    }
}
