#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <netinet/in.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <string.h>
#include <strings.h>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>

#include <sys/mman.h>
#include <fcntl.h>

#include <fcntl.h>
#include <poll.h>

#include "common.h"
#include "I2C.h"
#include "SSD1306_OLED.h"

#include "FPPOLEDUtils.h"
#include "util/BBBUtils.h"

#include <linux/wireless.h>
#include <sys/ioctl.h>
#include <jsoncpp/json/json.h>
#include <gpiod.h>

#include "OLEDPages.h"
#include "FPPStatusOLEDPage.h"


//shared memory area so other processes can see if the display is on
//as well as let fppoled know to force it off (if the pins need to be
//reconfigured so I2C no longer will work)

struct DisplayStatus {
    unsigned int i2cBus;
    volatile unsigned int displayOn;
    volatile unsigned int forceOff;
};
static DisplayStatus *currentStatus;
static std::string controlPin;

extern I2C_DeviceT I2C_DEV_2;



FPPOLEDUtils::FPPOLEDUtils(int ledType)
    : _ledType(ledType), gpiodChips(10)
{
    int smfd = shm_open("fppoled", O_CREAT | O_RDWR, 0);
    ftruncate(smfd, 1024);
    currentStatus = (DisplayStatus *)mmap(0, 1024, PROT_WRITE | PROT_READ, MAP_SHARED, smfd, 0);
    close(smfd);
    int i2cb = I2C_DEV_2.i2c_dev_path[strlen(I2C_DEV_2.i2c_dev_path) - 1] - '0';
    currentStatus->i2cBus = i2cb;
    currentStatus->displayOn = true;
    currentStatus->forceOff = false;

    for (auto &a : gpiodChips) {
        a = nullptr;
    }
    if (_ledType == 2 || _ledType == 4 || _ledType == 6 || _ledType == 8 || _ledType == 10) {
        setRotation(2);
    } else if (_ledType) {
        setRotation(0);
    }
    statusPage = nullptr;
}
FPPOLEDUtils::~FPPOLEDUtils() {
    cleanup();
    delete statusPage;
}
void FPPOLEDUtils::cleanup() {
    for (auto &a : actions) {
        delete a;
    }
    actions.clear();
    for (auto a : gpiodChips) {
        if (a) {
            gpiod_chip_close(a);
        }
    }
}




static const std::string EMPTY_STRING = "";

FPPOLEDUtils::InputAction::~InputAction() {
    for (auto &a : actions) {
        delete a;
    }
    actions.clear();
    
    if (gpiodLine) {
        gpiod_line_release(gpiodLine);
    }
}


class GPIODExtenderAction : public FPPOLEDUtils::InputAction::Action {
public:
    GPIODExtenderAction(const std::string &a, int min, int max, long long mai, struct gpiod_line *line)
        : FPPOLEDUtils::InputAction::Action(a, min, max, mai), gpiodLine(line) {}
    virtual ~GPIODExtenderAction() {
        if (gpiodLine) {
            printf("Release %s\n", action.c_str());
            gpiod_line_release(gpiodLine);
        }
    }

    virtual bool checkAction(int i, long long ntimeus) override {
        int v = gpiod_line_get_value(gpiodLine);
        return FPPOLEDUtils::InputAction::Action::checkAction(v, ntimeus);
    }

    struct gpiod_line *gpiodLine = nullptr;
};


bool FPPOLEDUtils::InputAction::Action::checkAction(int i, long long ntimeus) {
    if (i <= actionValueMax && i >= actionValueMin
        && (ntimeus > nextActionTime)) {
        //at least 10ms since last action.  Should cover any debounce time
        nextActionTime = ntimeus + minActionInterval;
        return true;
    }
    return false;
}
const std::string &FPPOLEDUtils::InputAction::checkAction(int i, long long ntimeus) {
    for (auto &a : actions) {
        if (a->checkAction(i, ntimeus)) {
            return a->action;
        }
    }
    return EMPTY_STRING;
}
bool FPPOLEDUtils::checkStatusAbility() {
#ifdef PLATFORM_BBB
    char buf[128] = {0};
    FILE *fd = fopen("/sys/firmware/devicetree/base/model", "r");
    if (fd) {
        fgets(buf, 127, fd);
        fclose(fd);
    }
    std::string model = buf;
    if (model == "TI AM335x PocketBeagle") {
        return true;
    }
    std::string file = "/home/fpp/media/tmp/cape-info.json";
    if (FileExists(file)) {
        Json::Value root;
        if (LoadJsonFromFile(file, root)) {
            if (root["id"].asString() == "Unsupported") {
                return false;
            }
        }
    }
    return false;
#else
    return true;
#endif
}

bool FPPOLEDUtils::setupControlPin(const std::string &file) {
    if (FileExists(file)) {
        Json::Value root;
        if (LoadJsonFromFile(file, root)) {
            if (root.isMember("controls")
                && root["controls"].isMember("I2CEnable")) {
                controlPin = root["controls"]["I2CEnable"].asString();
                printf("Using control pin %s\n", controlPin.c_str());
                
                const PinCapabilities &pin = PinCapabilities::getPinByName(controlPin);
                pin.configPin("gpio", true);
                pin.setValue(1);
                return true;
            }
        }
    }
    return false;
}

FPPOLEDUtils::InputAction *FPPOLEDUtils::configureGPIOPin(const std::string &pinName,
                                                          const std::string &mode,
                                                          const std::string &edge) {
    InputAction *action = new InputAction();
    action->pin = pinName;
    action->mode = mode;
    const PinCapabilities &pin = PinCapabilities::getPinByName(action->pin);
    pin.configPin(action->mode, false);

    action->gpiodLine = nullptr;
    if (!gpiodChips[pin.gpioIdx]) {
        gpiodChips[pin.gpioIdx] = gpiod_chip_open_by_number(pin.gpioIdx);
    }
    action->gpiodLine = gpiod_chip_get_line(gpiodChips[pin.gpioIdx], pin.gpio);
    
    struct gpiod_line_request_config lineConfig;
    lineConfig.consumer = "FPPOLED";
    lineConfig.request_type = GPIOD_LINE_REQUEST_DIRECTION_INPUT;
    lineConfig.flags = 0;
    if (gpiod_line_request(action->gpiodLine, &lineConfig, 0) == -1) {
        printf("Could not config line as input.  Line %d.\n", action->gpiodLine);
    }
    gpiod_line_release(action->gpiodLine);
    if (edge == "falling") {
        lineConfig.request_type = GPIOD_LINE_REQUEST_EVENT_FALLING_EDGE;
    } else if (edge == "rising") {
        lineConfig.request_type = GPIOD_LINE_REQUEST_EVENT_RISING_EDGE;
    } else {
        lineConfig.request_type = GPIOD_LINE_REQUEST_EVENT_BOTH_EDGES;
    }
    lineConfig.flags = 0;
    if (gpiod_line_request(action->gpiodLine, &lineConfig, 0) == -1) {
        printf("Could not config line edge for line %d\n", action->gpiodLine);
    }
    action->file = gpiod_line_event_get_fd(action->gpiodLine);
    action->kernelGPIO = pin.kernelGpio;
    action->gpioChipIdx = pin.gpioIdx;
    action->gpioChipLine = pin.gpio;
    return action;
}

bool FPPOLEDUtils::parseInputActionFromGPIO(const std::string &file) {
    char vbuffer[256];
    bool needsPolling = false;
    if (FileExists(file)) {
        Json::Value root;
        if (LoadJsonFromFile(file, root)) {
            for (int x = 0; x < root.size(); x++) {
                if (!root[x]["enabled"].asBool()) {
                    continue;
                }
                std::string edge = "";
                std::string actionValue = "";
                std::string mode = root[x]["mode"].asString();
                std::string pinName = root[x]["pin"].asString();
                std::string fallingAction = "";
                std::string risingAction = "";
                if (root[x].isMember("falling") && root[x]["falling"]["command"].asString() == "OLED Navigation") {
                    edge = "falling";
                    fallingAction = root[x]["falling"]["args"][0].asString();
                }
                if (root[x].isMember("rising") && root[x]["rising"]["command"].asString() == "OLED Navigation") {
                    if (edge == "falling") {
                        edge = "both";
                    } else {
                        edge = "rising";
                    }
                    risingAction = root[x]["rising"]["args"][0].asString();
                }
                if (edge != "") {
                    InputAction *action = configureGPIOPin(pinName, mode, edge);
                    if (risingAction != "") {
                        printf("Configuring pin %s as input of type %s   (mode: %s, gpio: %d,  file: %d)\n", action->pin.c_str(), risingAction.c_str(), action->mode.c_str(), action->kernelGPIO, action->file);
                        action->actions.push_back(new FPPOLEDUtils::InputAction::Action(risingAction, 1, 1, 100000));
                    }
                    if (fallingAction != "") {
                        printf("Configuring pin %s as input of type %s   (mode: %s, gpio: %d,  file: %d)\n", action->pin.c_str(), fallingAction.c_str(), action->mode.c_str(), action->kernelGPIO, action->file);
                        action->actions.push_back(new FPPOLEDUtils::InputAction::Action(fallingAction, 0, 0, 100000));
                    }
                    actions.push_back(action);
                }
            }
        }
    }
    return needsPolling;
}

bool FPPOLEDUtils::parseInputActions(const std::string &file) {
    char vbuffer[256];
    bool needsPolling = false;
    if (FileExists(file)) {
        Json::Value root;
        if (LoadJsonFromFile(file, root)) {
            for (int x = 0; x < root["inputs"].size(); x++) {
                InputAction *action = new InputAction();
                action->pin = root["inputs"][x]["pin"].asString();
                action->mode = root["inputs"][x]["mode"].asString();
                action->gpiodLine = nullptr;
                if (action->mode.find("gpio") != std::string::npos) {
                    std::string edge = root["inputs"][x]["edge"].asString();
                    int actionValue = (edge == "falling" ? 0 : 1);
                    const PinCapabilities &pin = PinCapabilities::getPinByName(action->pin);

                    pin.configPin(action->mode, false);
                    if (!gpiodChips[pin.gpioIdx]) {
                        gpiodChips[pin.gpioIdx] = gpiod_chip_open_by_number(pin.gpioIdx);
                    }
                    action->gpiodLine = gpiod_chip_get_line(gpiodChips[pin.gpioIdx], pin.gpio);
                    
                    struct gpiod_line_request_config lineConfig;
                    lineConfig.consumer = "FPPOLED";
                    lineConfig.request_type = GPIOD_LINE_REQUEST_DIRECTION_INPUT;
                    lineConfig.flags = 0;
                    if (gpiod_line_request(action->gpiodLine, &lineConfig, 0) == -1) {
                        printf("Could not config line as input\n");
                    }
                    gpiod_line_release(action->gpiodLine);
                    if (edge == "falling") {
                        lineConfig.request_type = GPIOD_LINE_REQUEST_EVENT_FALLING_EDGE;
                    } else {
                        lineConfig.request_type = GPIOD_LINE_REQUEST_EVENT_RISING_EDGE;
                    }
                    lineConfig.flags = 0;
                    if (gpiod_line_request(action->gpiodLine, &lineConfig, 0) == -1) {
                        printf("Could not config line edge\n");
                    }
                    action->file = gpiod_line_event_get_fd(action->gpiodLine);
                    
                    std::string type = root["inputs"][x]["type"].asString();
                    if (type == "gpiod") {
                        //this is the mode for various gpio extenders like the pca9675 chip that
                        //use a single interrupt line (configured above) fall all the buttons.
                        //The above will trigger an interupt after which we will need to
                        //use libgpiod to get the value of each button to figure out
                        //which triggered the action.  Thus, there are multiple actions
                        
                        std::string label = root["inputs"][x]["chip"].asString();
                        gpiod_chip *chip = getChip(label);
                        if (chip) {
                            int lines = gpiod_chip_num_lines(chip);
                            for (int a = 0; a < root["inputs"][x]["actions"].size(); a++) {
                                std::string buttonaction = root["inputs"][x]["actions"][a]["action"].asString();
                                int gpioline = root["inputs"][x]["actions"][a]["line"].asInt();
                                if (gpioline < lines) {
                                    gpiod_line *l = gpiod_chip_get_line(chip, gpioline);
                                    struct gpiod_line_request_config lineConfig;
                                    lineConfig.consumer = "FPPOLED";
                                    lineConfig.request_type = GPIOD_LINE_REQUEST_DIRECTION_INPUT;
                                    lineConfig.flags = 0;
                                    if (gpiod_line_request(l, &lineConfig, 0) == -1) {
                                        printf("Could not config line as input\n");
                                    }
                                    
                                    printf("Configuring pin %s as input of type %s   (chip: %s, gpio: %d)\n", action->pin.c_str(), buttonaction.c_str(), label.c_str(), gpioline);
                                    int v = gpiod_line_get_value(l);
                                    action->actions.push_back(new GPIODExtenderAction(buttonaction, 0, 0, 100000, l));
                                }
                            }
                        } else {
                            printf("Could not find gpio chip for %s\n", label.c_str());
                        }
                    } else {
                        printf("Configuring pin %s as input of type %s   (mode: %s, gpio: %d)\n", action->pin.c_str(), type.c_str(), action->mode.c_str(), pin.kernelGpio);
                        action->actions.push_back(new FPPOLEDUtils::InputAction::Action(type, actionValue, actionValue, 100000));
                    }
                    actions.push_back(action);
                } else if (action->mode == "ain") {
                    char path[256];
                    sprintf(path, "/sys/bus/iio/devices/iio:device0/in_voltage%d_raw", root["inputs"][x]["input"].asInt());
                    if (FileExists(path)) {
                        action->file = open(path, O_RDONLY);
                        for (int a = 0; a < root["inputs"][x]["actions"].size(); a++) {
                            std::string buttonaction = root["inputs"][x]["actions"][a]["action"].asString();
                            int minValue = root["inputs"][x]["actions"][a]["minValue"].asInt();
                            int maxValue = root["inputs"][x]["actions"][a]["maxValue"].asInt();
                            printf("Configuring AIN input of type %s  with range %d-%d\n", buttonaction.c_str(), minValue, maxValue);
                            action->actions.push_back(new FPPOLEDUtils::InputAction::Action(buttonaction, minValue, maxValue, 250000));
                        }
                        
                        actions.push_back(action);
                        needsPolling = true;
                    }
                }
            }
        }
    }
    fflush(stdout);
    return needsPolling;
}

gpiod_chip * FPPOLEDUtils::getChip(const std::string &n) {
    for (auto &a : gpiodChips) {
        if (a) {
            if (n == gpiod_chip_label(a) || n == gpiod_chip_name(a)) {
                return a;
            }
        }
    }
    // did not find
    gpiod_chip *chip = gpiod_chip_open_lookup(n.c_str());
    if (chip) {
        gpiodChips.push_back(chip);
    }
    return chip;
}

void FPPOLEDUtils::run() {
    char vbuffer[256];
    setupControlPin("/home/fpp/media/tmp/cape-info.json");
    bool needsPolling = parseInputActions("/home/fpp/media/tmp/cape-inputs.json");
    needsPolling |= parseInputActionFromGPIO("/home/fpp/media/config/gpio.json");
    std::vector<struct pollfd> fdset(actions.size());
    
    if (actions.size() == 0 && _ledType == 0) {
        //no display and no actions, nothing to do.
        exit(0);
    }

    if (_ledType == 3 || _ledType == 4) {
        OLEDPage::SetOLEDType(OLEDPage::OLEDType::SMALL);
    } else if (_ledType == 7 || _ledType == 8) {
        OLEDPage::SetOLEDType(OLEDPage::OLEDType::TWO_COLOR);
    } else if (_ledType == 0) {
        OLEDPage::SetOLEDType(OLEDPage::OLEDType::NONE);
    } else {
        OLEDPage::SetOLEDType(OLEDPage::OLEDType::SINGLE_COLOR);
    }
    if (_ledType == 2 || _ledType == 4 || _ledType == 6 || _ledType == 8 || _ledType == 10) {
        OLEDPage::SetOLEDOrientationFlipped(true);
    }
    statusPage = new FPPStatusOLEDPage();
    if (!checkStatusAbility()) {
        //statusPage->disableFullStatus();
    }
    OLEDPage::SetCurrentPage(statusPage);
    
    long long lastUpdateTime = 0;
    long long ntime = GetTime();
    long long lastActionTime = GetTime();
    while (true) {
        bool forcedOff = currentStatus->forceOff;
        if (OLEDPage::IsForcedOff() && !forcedOff) {
            //turning back on
            if (controlPin != "") {
                printf("Re-enabling I2C Bus via pin: %s\n", controlPin.c_str());
                const PinCapabilities &pin = PinCapabilities::getPinByName(controlPin);
                pin.configPin("gpio", true);
                pin.setValue(1);
            }
            OLEDPage::SetForcedOff(forcedOff);
            currentStatus->displayOn = true;
            OLEDPage::SetCurrentPage(statusPage);
            lastActionTime = GetTime();
        } else if (!OLEDPage::IsForcedOff() && forcedOff) {
            if (currentStatus->displayOn) {
                if (OLEDPage::GetOLEDType() != OLEDPage::OLEDType::NONE) {
                    clearDisplay();
                    Display();
                }
            }
            if (controlPin != "") {
                printf("Disabling I2C Bus via pin: %s\n", controlPin.c_str());
                const PinCapabilities &pin = PinCapabilities::getPinByName(controlPin);
                pin.configPin("gpio", true);
                pin.setValue(0);
            }
            OLEDPage::SetForcedOff(forcedOff);
            currentStatus->displayOn = false;
            OLEDPage::SetCurrentPage(statusPage);
        }
        if (ntime > (lastUpdateTime + 1000000)) {
            bool displayOn = currentStatus->displayOn;
            if (OLEDPage::GetCurrentPage()
                && OLEDPage::GetCurrentPage()->doIteration(displayOn)) {
                lastActionTime = GetTime();
            }
            currentStatus->displayOn = displayOn;
            lastUpdateTime = ntime;
        }
        if (actions.empty()) {
            sleep(1);
            ntime = GetTime();
        } else {
            memset((void*)&fdset[0], 0, sizeof(struct pollfd) * actions.size());
            int actionCount = 0;
            for (int x = 0; x < actions.size(); x++) {
                if (actions[x]->mode != "ain") {
                    fdset[actionCount].fd = actions[x]->file;
                    fdset[actionCount].events = POLLIN | POLLPRI;
                    actions[x]->pollIndex = actionCount;
                    actionCount++;
                }
            }

            if (actionCount) {
                poll(&fdset[0], actionCount, needsPolling ? 100 : 1000);
            } else {
                usleep(100000);
            }
            ntime = GetTime();
            for (int x = 0; x < actions.size(); x++) {
                std::string action;
                if (actions[x]->mode == "ain") {
                    lseek(actions[x]->file, 0, SEEK_SET);
                    int len = read(actions[x]->file, vbuffer, 255);
                    int v = atoi(vbuffer);
                    action = actions[x]->checkAction(v, ntime);
                } else if (fdset[actions[x]->pollIndex].revents) {
                    struct gpiod_line_event event;
                    if (gpiod_line_event_read_fd(fdset[actions[x]->pollIndex].fd, &event) >= 0) {
                        int v = gpiod_line_get_value(actions[x]->gpiodLine);
                        action = actions[x]->checkAction(v, ntime);
                    }
                }
                if (action != "" && !currentStatus->displayOn) {
                    //just turn the display on if button is hit
                    if (!currentStatus->forceOff) {
                        currentStatus->displayOn = true;
                        OLEDPage::SetCurrentPage(statusPage);
                        lastUpdateTime = 0;
                        lastActionTime = ntime;
                    } else if ((action == "Test" || action == "Test/Down") && ((ntime - lastActionTime) > 70000)){
                        printf("Action: %s\n", action.c_str());
                        lastUpdateTime = 0;
                        lastActionTime = ntime;
                        if (OLEDPage::GetCurrentPage()) {
                            OLEDPage::GetCurrentPage()->doAction(action);
                        }
                    }
                } else if (action != "" && ((ntime - lastActionTime) > 70000)) { //account for some debounce time
                    printf("Action: %s\n", action.c_str());
                    //force immediate update
                    lastUpdateTime = 0;
                    lastActionTime = ntime;
                    if (OLEDPage::GetCurrentPage()) {
                        OLEDPage::GetCurrentPage()->doAction(action);
                    }
                }
            }
            if (ntime > (lastActionTime + 180000000)) {
                if (OLEDPage::GetCurrentPage() != statusPage) {
                    OLEDPage::SetCurrentPage(statusPage);
                }
                currentStatus->displayOn = false;
            }
        }
    }
}


