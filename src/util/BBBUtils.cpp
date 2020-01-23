
#include "BBBUtils.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <vector>
#include <thread>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>


static BeagleBoneType BBB_TYPE = Unknown;
BeagleBoneType getBeagleBoneType() {
    if (BBB_TYPE == Unknown) {
        FILE *file = fopen("/proc/device-tree/model", "r");
        if (file) {
            char buf[256];
            fgets(buf , 256 , file);
            fclose(file);
            if (strcmp(buf, "SanCloud BeagleBone Enhanced") == 0) {
                BBB_TYPE = SanCloudEnhanced;
            } else if (strcmp(&buf[10], "PocketBeagle") == 0) {
                BBB_TYPE = PocketBeagle;
            } else if (strcmp(&buf[10], "BeagleBone Black Wireless") == 0) {
                BBB_TYPE = BlackWireless;
            } else if (strcmp(&buf[10], "BeagleBone Green Wireless") == 0) {
                BBB_TYPE = GreenWireless;
            } else if (strcmp(&buf[10], "BeagleBone Green") == 0) {
                BBB_TYPE = Green;
            } else {
                BBB_TYPE = Black;
            }
        } else {
            BBB_TYPE = Black;
        }
    }
    return BBB_TYPE;
}

static const std::vector<BBBPinCapabilities> PB_PINS = {
    BBBPinCapabilities("P1-02", 87).setPRU(1, 9),
    BBBPinCapabilities("P1-04", 89).setPRU(1, 11),
    BBBPinCapabilities("P1-06", 5),
    BBBPinCapabilities("P1-08", 2).setPwm(0, 0).setUART("ttyS2-rx"),
    BBBPinCapabilities("P1-10", 3).setPwm(0, 1).setUART("ttyS2-tx"),
    BBBPinCapabilities("P1-12", 4),
    BBBPinCapabilities("P1-20", 20),
    BBBPinCapabilities("P1-26", 12).setI2C(2),
    BBBPinCapabilities("P1-28", 13).setI2C(2),
    BBBPinCapabilities("P1-29", 117).setPRU(0, 7),
    BBBPinCapabilities("P1-30", 43).setPRU(1, 15).setUART("ttyS0-tx"),
    BBBPinCapabilities("P1-31", 114).setPRU(0, 4),
    BBBPinCapabilities("P1-32", 42).setPRU(1, 14).setUART("ttyS0-rx"),
    BBBPinCapabilities("P1-33", 111).setPRU(0, 1).setPwm(0, 1),
    BBBPinCapabilities("P1-34", 26),
    BBBPinCapabilities("P1-35", 88).setPRU(1, 10),
    BBBPinCapabilities("P1-36", 110).setPRU(0, 0).setPwm(0, 0),
    BBBPinCapabilities("P2-01", 50).setPwm(1, 0),
    BBBPinCapabilities("P2-02", 59),
    BBBPinCapabilities("P2-03", 23).setPwm(2, 1),
    BBBPinCapabilities("P2-04", 58),
    BBBPinCapabilities("P2-05", 30).setUART("ttyS4-rx"),
    BBBPinCapabilities("P2-06", 57),
    BBBPinCapabilities("P2-07", 31).setUART("ttyS4-tx"),
    BBBPinCapabilities("P2-08", 60),
    BBBPinCapabilities("P2-09", 15).setI2C(1),
    BBBPinCapabilities("P2-10", 52),
    BBBPinCapabilities("P2-11", 14).setI2C(1),
    BBBPinCapabilities("P2-17", 65),
    BBBPinCapabilities("P2-18", 47),
    BBBPinCapabilities("P2-19", 27),
    BBBPinCapabilities("P2-20", 64),
    BBBPinCapabilities("P2-22", 46),
    BBBPinCapabilities("P2-24", 44).setPRU(0, 14),
    BBBPinCapabilities("P2-25", 41),
    BBBPinCapabilities("P2-27", 40),
    BBBPinCapabilities("P2-28", 116).setPRU(0, 6),
    BBBPinCapabilities("P2-29", 7),
    BBBPinCapabilities("P2-30", 113).setPRU(0, 3),
    BBBPinCapabilities("P2-31", 19),
    BBBPinCapabilities("P2-32", 112).setPRU(0, 2),
    BBBPinCapabilities("P2-33", 45).setPRU(0, 15),
    BBBPinCapabilities("P2-34", 115).setPRU(0, 5),
    BBBPinCapabilities("P2-35", 86).setPRU(1, 8)
};
static const std::vector<BBBPinCapabilities> BBB_PINS = {
    BBBPinCapabilities("P8-03", 38),
    BBBPinCapabilities("P8-04", 39),
    BBBPinCapabilities("P8-05", 34),
    BBBPinCapabilities("P8-06", 35),
    BBBPinCapabilities("P8-07", 66),
    BBBPinCapabilities("P8-08", 67),
    BBBPinCapabilities("P8-09", 69),
    BBBPinCapabilities("P8-10", 68),
    BBBPinCapabilities("P8-11", 45).setPRU(0, 15),
    BBBPinCapabilities("P8-12", 44).setPRU(0, 14),
    BBBPinCapabilities("P8-13", 23).setPwm(2, 1),
    BBBPinCapabilities("P8-14", 26),
    BBBPinCapabilities("P8-15", 47),
    BBBPinCapabilities("P8-16", 46),
    BBBPinCapabilities("P8-17", 27),
    BBBPinCapabilities("P8-18", 65),
    BBBPinCapabilities("P8-19", 22).setPwm(2, 0),
    BBBPinCapabilities("P8-20", 63).setPRU(1, 13),
    BBBPinCapabilities("P8-21", 62).setPRU(1, 12),
    BBBPinCapabilities("P8-22", 37),
    BBBPinCapabilities("P8-23", 36),
    BBBPinCapabilities("P8-24", 33),
    BBBPinCapabilities("P8-25", 32),
    BBBPinCapabilities("P8-26", 61),
    BBBPinCapabilities("P8-27", 86).setPRU(1, 8),
    BBBPinCapabilities("P8-28", 88).setPRU(1, 10),
    BBBPinCapabilities("P8-29", 87).setPRU(1, 9),
    BBBPinCapabilities("P8-30", 89).setPRU(1, 11),
    BBBPinCapabilities("P8-31", 10),
    BBBPinCapabilities("P8-32", 11),
    BBBPinCapabilities("P8-33", 9),
    BBBPinCapabilities("P8-34", 81).setPwm(1, 1),
    BBBPinCapabilities("P8-35", 8),
    BBBPinCapabilities("P8-36", 80).setPwm(1, 0),
    BBBPinCapabilities("P8-37", 78).setUART("ttyS5-tx"),
    BBBPinCapabilities("P8-38", 79).setUART("ttyS5-rx"),
    BBBPinCapabilities("P8-39", 76).setPRU(1, 6),
    BBBPinCapabilities("P8-40", 77).setPRU(1, 7),
    BBBPinCapabilities("P8-41", 74).setPRU(1, 4),
    BBBPinCapabilities("P8-42", 75).setPRU(1, 5),
    BBBPinCapabilities("P8-43", 72).setPRU(1, 2),
    BBBPinCapabilities("P8-44", 73).setPRU(1, 3),
    BBBPinCapabilities("P8-45", 70).setPRU(1, 0).setPwm(2, 0),
    BBBPinCapabilities("P8-46", 71).setPRU(1, 1).setPwm(2, 1),
    BBBPinCapabilities("P9-11", 30).setUART("ttyS4-rx"),
    BBBPinCapabilities("P9-12", 60),
    BBBPinCapabilities("P9-13", 31).setUART("ttyS4-tx"),
    BBBPinCapabilities("P9-14", 50).setPwm(1, 0),
    BBBPinCapabilities("P9-15", 48),
    BBBPinCapabilities("P9-16", 51).setPwm(1, 1),
    BBBPinCapabilities("P9-17", 5).setI2C(1),
    BBBPinCapabilities("P9-18", 4).setI2C(1),
    BBBPinCapabilities("P9-19", 13).setI2C(2),
    BBBPinCapabilities("P9-20", 12).setI2C(2),
    BBBPinCapabilities("P9-21", 3).setPwm(0, 1).setUART("ttyS2-tx"),
    BBBPinCapabilities("P9-22", 2).setPwm(0, 0).setUART("ttyS2-rx"),
    BBBPinCapabilities("P9-23", 49),
    BBBPinCapabilities("P9-24", 15).setUART("ttyS1-tx"),
    BBBPinCapabilities("P9-25", 117).setPRU(0, 7),
    BBBPinCapabilities("P9-26", 14).setUART("ttyS1-rx"),
    BBBPinCapabilities("P9-27", 115).setPRU(0, 5),
    BBBPinCapabilities("P9-28", 113).setPRU(0, 3),
    BBBPinCapabilities("P9-29", 111).setPRU(0, 1).setPwm(0, 1),
    BBBPinCapabilities("P9-30", 112).setPRU(0, 2),
    BBBPinCapabilities("P9-31", 110).setPRU(0, 0).setPwm(0, 0),
    BBBPinCapabilities("P9-41", 20),
    BBBPinCapabilities("P9-91", 116).setPRU(0, 6),
    BBBPinCapabilities("P9-42", 7).setUART("ttyS3-tx"),
    BBBPinCapabilities("P9-92", 114).setPRU(0, 4),
};


BBBPinCapabilities::BBBPinCapabilities(const std::string &n, uint32_t k)
: PinCapabilitiesFluent(n, k) {
    gpioIdx = k / 32;
    gpio = k % 32;
}

// ------------------------------------------------------------------------

#define GPIO_DATAIN     0x138
#define GPIO_DATAOUT    0x13C
#define GPIO_SETDATAOUT 0x194
#define GPIO_CLEARDATAOUT 0x190

//GPIO locations
#define GPIO0_BASE 0x44E07000
#define GPIO1_BASE 0x4804C000
#define GPIO2_BASE 0x481AC000
#define GPIO3_BASE 0x481AE000
#define GPIO_SIZE  0x00001000


// 1MHz frequence
// #define HZ 1000000.0f
#define HZ 10000.0f


static uint8_t *bbGPIOMap[] = {0, 0, 0, 0};

static int bbbPWMChipNums[] = { 1, 3, 6};

static const char *bbbPWMDeviceNames[] = {
    "/sys/devices/platform/ocp/48300000.epwmss/48300200.pwm/pwm/pwmchip",
    "/sys/devices/platform/ocp/48302000.epwmss/48302200.pwm/pwm/pwmchip",
    "/sys/devices/platform/ocp/48304000.epwmss/48304200.pwm/pwm/pwmchip",
};

static FILE * bbbPWMDutyFiles[] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

static volatile bool registersMemMapped = false;

static int getBBBPWMChipNum(int pwm) {
    char buf[256];
    for (int x = 0; x < 10; x++) {
        sprintf(buf, "%s%d", bbbPWMDeviceNames[pwm], x);
        if (access(buf, F_OK) == 0) {
            return x;
        }
    }
    return bbbPWMChipNums[pwm];
}

static void setupBBBMemoryMap() {
    if (registersMemMapped) {
        return;
    }
    int fd = open("/dev/mem", O_RDWR);
    bbGPIOMap[0] =  (uint8_t*)mmap(0, GPIO_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, GPIO0_BASE);
    bbGPIOMap[1] =  (uint8_t*)mmap(0, GPIO_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, GPIO1_BASE);
    bbGPIOMap[2] =  (uint8_t*)mmap(0, GPIO_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, GPIO2_BASE);
    bbGPIOMap[3] =  (uint8_t*)mmap(0, GPIO_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, GPIO3_BASE);


    registersMemMapped = 1;
}

// ------------------------------------------------------------------------


bool BBBPinCapabilities::supportPWM() const {
    return pwm != -1;
}

int BBBPinCapabilities::configPin(const std::string& mode,
                                  bool directionOut) const {
    bool enableI2C = (mode == "i2c" || mode == "default");
    if (i2cBus >= 0 && !enableI2C) {
        enableOledScreen(i2cBus, false);
    }
    
    char pinName[16];
    strcpy(pinName, name.c_str());
    if (pinName[2] == '-') {
        pinName[2] = '_';
    }
    
    char dir_name[128];
    
    sprintf(dir_name, "/sys/class/gpio/gpio%u/direction", kernelGpio);
    if (access(dir_name, F_OK) == -1) {
        // not exported, we need to export it
        FILE *dir = fopen("/sys/class/gpio/export", "w");
        fprintf(dir, "%d", kernelGpio);
        fclose(dir);
    }
    snprintf(dir_name, sizeof(dir_name),
             "/sys/devices/platform/ocp/ocp:%s_pinmux/state",
             pinName
             );
    FILE *dir = fopen(dir_name, "w");
    if (!dir) {
        return -1;
    }
    fprintf(dir, "%s\n", mode.c_str());
    fclose(dir);
    
    if (mode != "pwm" && mode != "i2c" && mode != "uart" && mode != "default") {
        snprintf(dir_name, sizeof(dir_name),
                 "/sys/class/gpio/gpio%u/direction",
                 kernelGpio
                 );
        
        dir = fopen(dir_name, "w");
        if (!dir) {
            return -1;
        }
        fprintf(dir, "%s\n", directionOut ? "out" : "in");
        fclose(dir);
    }
    if (i2cBus >= 0 && enableI2C) {
        enableOledScreen(i2cBus, true);
    }
    return 0;
}
bool BBBPinCapabilities::getValue() const {
    setupBBBMemoryMap();
    int set = 1 << gpio;
    uint32_t *base = (uint32_t*)(bbGPIOMap[gpioIdx] + GPIO_DATAIN);
    return (*base & set) ? true : false;
}
void BBBPinCapabilities::setValue(bool v) const {
    setupBBBMemoryMap();
    int set = 1 << gpio;
    uint8_t *base = bbGPIOMap[gpioIdx];
    if (v) {
        base += GPIO_SETDATAOUT;
    } else {
        base += GPIO_CLEARDATAOUT;
    }
    uint32_t* base32 = (uint32_t*)base;
    *base32 = set;
}


int BBBPinCapabilities::openValueForPoll() const {
    char dir_name[128];
    snprintf(dir_name, sizeof(dir_name),
             "/sys/class/gpio/gpio%u/value",
             kernelGpio
             );
    return open(dir_name, O_RDONLY | O_NONBLOCK);
}
bool BBBPinCapabilities::setupPWM(int maxValue) const {
    if (pwm != -1 && (bbbPWMDutyFiles[pwm * 2 + subPwm] == nullptr)) {
        //set pin
        setupBBBMemoryMap();
        
        configPin("pwm");
        int chipNum = getBBBPWMChipNum(pwm);
        char dir_name[128];
        snprintf(dir_name, sizeof(dir_name), "%s%d/export", bbbPWMDeviceNames[pwm], chipNum);
        FILE *dir = fopen(dir_name, "w");
        fprintf(dir, "%d", subPwm);
        fclose(dir);
        
        
        snprintf(dir_name, sizeof(dir_name), "%s%d/pwm-%d:%d/period",
                 bbbPWMDeviceNames[pwm], chipNum, chipNum, subPwm);
        dir = fopen(dir_name, "w");
        fprintf(dir, "%d", maxValue);
        fclose(dir);
        
        snprintf(dir_name, sizeof(dir_name), "%s%d/pwm-%d:%d/duty_cycle",
                 bbbPWMDeviceNames[pwm], chipNum, chipNum, subPwm);
        dir = fopen(dir_name, "w");
        fprintf(dir, "0");
        fflush(dir);
        bbbPWMDutyFiles[pwm * 2 + subPwm] = dir;
        
        snprintf(dir_name, sizeof(dir_name), "%s%d/pwm-%d:%d/enable",
                 bbbPWMDeviceNames[pwm], chipNum, chipNum, subPwm);
        dir = fopen(dir_name, "w");
        fprintf(dir, "1");
        fclose(dir);
    }
    return false;
}

void BBBPinCapabilities::setPWMValue(int value) const {
    if (pwm != -1) {
        setupBBBMemoryMap();
        
        char val[16];
        fprintf(bbbPWMDutyFiles[pwm * 2 + subPwm], "%d", value);
        fflush(bbbPWMDutyFiles[pwm * 2 + subPwm]);
    }
}
//PWM locations
#define PWMSS0_BASE 0x48300000
#define PWMSS1_BASE 0x48302000
#define PWMSS2_BASE 0x48304000

int BBBPinCapabilities::getPWMRegisterAddress() const {
    //need to add 0x200 to the base address to get the
    //address to the eHRPWM register which is what we really use
    if (pwm == 0)  {
        return PWMSS0_BASE + 0x200;
    } else if (pwm == 1)  {
        return PWMSS1_BASE + 0x200;
    } else if (pwm == 2)  {
        return PWMSS2_BASE + 0x200;
    }
    return 0;
}

class NullBBBPinCapabilities : public BBBPinCapabilities {
public:
    NullBBBPinCapabilities() : BBBPinCapabilities("-none-", 0) {}
    virtual const PinCapabilities *ptr() const override { return nullptr; }
};
static NullBBBPinCapabilities NULL_BBB_INSTANCE;

const BBBPinCapabilities &BBBPinCapabilities::getPinByName(const std::string &name) {
    if (getBeagleBoneType() == PocketBeagle) {
        for (auto &a : PB_PINS) {
            if (a.name == name) {
                return a;
            }
        }
    } else {
        for (auto &a : BBB_PINS) {
            if (a.name == name) {
                return a;
            }
        }
    }
    return NULL_BBB_INSTANCE;
}

const BBBPinCapabilities &BBBPinCapabilities::getPinByGPIO(int i) {
    if (getBeagleBoneType() == PocketBeagle) {
        for (auto &a : PB_PINS) {
            if (a.kernelGpio == i) {
                return a;
            }
        }
    } else {
        for (auto &a : BBB_PINS) {
            if (a.kernelGpio == i) {
                return a;
            }
        }
    }
    return NULL_BBB_INSTANCE;
}

void BBBPinCapabilities::Init() {
    
}

std::vector<std::string> BBBPinCapabilities::getPinNames() {
    std::vector<std::string> ret;
    if (getBeagleBoneType() == PocketBeagle) {
        for (auto &a : PB_PINS) {
            ret.push_back(a.name);
        }
    } else {
        for (auto &a : BBB_PINS) {
            ret.push_back(a.name);
        }
    }
    return ret;
}
const BBBPinCapabilities &BBBPinCapabilities::getPinByUART(const std::string &n) {
    if (getBeagleBoneType() == PocketBeagle) {
        for (auto &a : PB_PINS) {
            if (a.uart == n) {
                return a;
            }
        }
    } else {
        for (auto &a : BBB_PINS) {
            if (a.uart == n) {
                return a;
            }
        }
    }
    return NULL_BBB_INSTANCE;
}
