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

extern I2C_DeviceT I2C_DEV_2;



FPPOLEDUtils::FPPOLEDUtils(int ledType)
    : _ledType(ledType)
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
        if (a.gpiodLine) {
            gpiod_line_release(a.gpiodLine);
        }
    }
    for (auto a : gpiodChips) {
        if (a) {
            gpiod_chip_close(a);
        }
    }
}

static const std::string EMPTY_STRING = "";
const std::string &FPPOLEDUtils::InputAction::checkAction(int i, long long ntimeus) {
    for (auto &a : actions) {
        if (i <= a.actionValueMax && i >= a.actionValueMin
            && (ntimeus > a.nextActionTime)) {
            //at least 10ms since last action.  Should cover any debounce time
            a.nextActionTime = ntimeus + a.minActionInterval;
            return a.action;
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
        std::ifstream t(file);
        std::stringstream buffer;
        buffer << t.rdbuf();
        std::string config = buffer.str();
        Json::Value root;
        Json::Reader reader;
        bool success = reader.parse(buffer.str(), root);
        if (success) {
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


bool FPPOLEDUtils::parseInputActions(const std::string &file) {
    char vbuffer[256];
    bool needsPolling = false;
    if (FileExists(file)) {
        std::ifstream t(file);
        std::stringstream buffer;
        buffer << t.rdbuf();
        std::string config = buffer.str();
        Json::Value root;
        Json::Reader reader;
        bool success = reader.parse(buffer.str(), root);
        if (success) {
            for (int x = 0; x < root["inputs"].size(); x++) {
                InputAction action;
                action.pin = root["inputs"][x]["pin"].asString();
                action.mode = root["inputs"][x]["mode"].asString();
                action.gpiodLine = nullptr;
                if (action.mode.find("gpio") != std::string::npos) {
                    std::string buttonaction = root["inputs"][x]["type"].asString();
                    std::string edge = root["inputs"][x]["edge"].asString();
                    int actionValue = (edge == "falling" ? 0 : 1);
                    const PinCapabilities &pin = PinCapabilities::getPinByName(action.pin);
                    printf("Configuring pin %s as input of type %s   (mode: %s, gpio: %d)\n", action.pin.c_str(), buttonaction.c_str(), action.mode.c_str(), pin.kernelGpio);

                    pin.configPin(action.mode, false);
                    if (!gpiodChips[pin.gpioIdx]) {
                        gpiodChips[pin.gpioIdx] = gpiod_chip_open_by_number(pin.gpioIdx);
                    }
                    
                    action.gpiodLine = gpiod_chip_get_line(gpiodChips[pin.gpioIdx], pin.gpio);

                    struct gpiod_line_request_config lineConfig;
                    lineConfig.consumer = "FPPOLED";
                    lineConfig.request_type = GPIOD_LINE_REQUEST_DIRECTION_INPUT;
                    lineConfig.flags = 0;
                    if (gpiod_line_request(action.gpiodLine, &lineConfig, 0) == -1) {
                        printf("Could not config line as input\n");
                    }
                    gpiod_line_release(action.gpiodLine);
                    if (edge == "falling") {
                        lineConfig.request_type = GPIOD_LINE_REQUEST_EVENT_FALLING_EDGE;
                    } else {
                        lineConfig.request_type = GPIOD_LINE_REQUEST_EVENT_RISING_EDGE;
                    }
                    lineConfig.flags = 0;
                    if (gpiod_line_request(action.gpiodLine, &lineConfig, 0) == -1) {
                        printf("Could not config line edge\n");
                    }

                    action.file = gpiod_line_event_get_fd(action.gpiodLine);
                    action.actions.push_back(FPPOLEDUtils::InputAction::Action(buttonaction, actionValue, actionValue, 10000));
                    actions.push_back(action);
                } else if (action.mode == "ain") {
                    char path[256];
                    sprintf(path, "/sys/bus/iio/devices/iio:device0/in_voltage%d_raw", root["inputs"][x]["input"].asInt());
                    if (FileExists(path)) {
                        action.file = open(path, O_RDONLY);
                        for (int a = 0; a < root["inputs"][x]["actions"].size(); a++) {
                            std::string buttonaction = root["inputs"][x]["actions"][a]["action"].asString();
                            int minValue = root["inputs"][x]["actions"][a]["minValue"].asInt();
                            int maxValue = root["inputs"][x]["actions"][a]["maxValue"].asInt();
                            printf("Configuring AIN input of type %s  with range %d-%d\n", buttonaction.c_str(), minValue, maxValue);
                            action.actions.push_back(FPPOLEDUtils::InputAction::Action(buttonaction, minValue, maxValue, 250000));
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


void FPPOLEDUtils::run() {
    char vbuffer[256];
    bool needsPolling = parseInputActions("/home/fpp/media/tmp/cape-inputs.json");
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
        OLEDPage::SetForcedOff(forcedOff);
        if (currentStatus->displayOn && forcedOff) {
            if (OLEDPage::GetOLEDType() != OLEDPage::OLEDType::NONE) {
                clearDisplay();
                Display();
            }
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
                if (actions[x].mode != "ain") {
                    fdset[actionCount].fd = actions[x].file;
                    fdset[actionCount].events = POLLIN | POLLPRI;
                    actions[x].pollIndex = actionCount;
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
                if (actions[x].mode == "ain") {
                    lseek(actions[x].file, 0, SEEK_SET);
                    int len = read(actions[x].file, vbuffer, 255);
                    int v = atoi(vbuffer);
                    action = actions[x].checkAction(v, ntime);
                } else if (fdset[actions[x].pollIndex].revents) {
                    struct gpiod_line_event event;
                    if (gpiod_line_event_read_fd(fdset[actions[x].pollIndex].fd, &event) >= 0) {
                        int v = gpiod_line_get_value(actions[x].gpiodLine);
                        action = actions[x].checkAction(v, ntime);
                    }
                }
                if (action != "" && !currentStatus->displayOn) {
                    //just turn the display on if button is hit
                    if (!currentStatus->forceOff) {
                        currentStatus->displayOn = true;
                    }
                    OLEDPage::SetCurrentPage(statusPage);
                    lastUpdateTime = 0;
                    lastActionTime = ntime;
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


