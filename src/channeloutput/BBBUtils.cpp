
#include "BBBUtils.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <vector>

static const std::vector<PinCapabilities> PB_PINS = {
    PinCapabilities("P1-02", 87, 1),
    PinCapabilities("P1-04", 89, 1),
    PinCapabilities("P1-06", 5),
    PinCapabilities("P1-08", 2),
    PinCapabilities("P1-10", 3),
    PinCapabilities("P1-12", 4),
    PinCapabilities("P1-20", 20),
    PinCapabilities("P1-26", 12),
    PinCapabilities("P1-28", 13),
    PinCapabilities("P1-29", 117, 0),
    PinCapabilities("P1-30", 43, 1),
    PinCapabilities("P1-31", 114, 0),
    PinCapabilities("P1-32", 42, 1),
    PinCapabilities("P1-33", 111, 0),
    PinCapabilities("P1-34", 26),
    PinCapabilities("P1-35", 88, 1),
    PinCapabilities("P1-36", 110, 0),
    PinCapabilities("P2-01", 50),
    PinCapabilities("P2-02", 59),
    PinCapabilities("P2-03", 23),
    PinCapabilities("P2-04", 58),
    PinCapabilities("P2-05", 30),
    PinCapabilities("P2-06", 57),
    PinCapabilities("P2-07", 31),
    PinCapabilities("P2-08", 60),
    PinCapabilities("P2-09", 15),
    PinCapabilities("P2-10", 52),
    PinCapabilities("P2-11", 14),
    PinCapabilities("P2-17", 65),
    PinCapabilities("P2-18", 47),
    PinCapabilities("P2-19", 27),
    PinCapabilities("P2-20", 64),
    PinCapabilities("P2-22", 46),
    PinCapabilities("P2-24", 44, 0),
    PinCapabilities("P2-25", 41),
    PinCapabilities("P2-27", 40),
    PinCapabilities("P2-28", 116, 0),
    PinCapabilities("P2-29", 7),
    PinCapabilities("P2-30", 113, 0),
    PinCapabilities("P2-31", 19),
    PinCapabilities("P2-32", 112, 0),
    PinCapabilities("P2-33", 45, 0),
    PinCapabilities("P2-34", 115, 0),
    PinCapabilities("P2-35", 86, 1)
};
static const std::vector<PinCapabilities> BBB_PINS = {
    PinCapabilities("P8-03", 38),
    PinCapabilities("P8-04", 39),
    PinCapabilities("P8-05", 34),
    PinCapabilities("P8-06", 35),
    PinCapabilities("P8-07", 66),
    PinCapabilities("P8-08", 67),
    PinCapabilities("P8-09", 69),
    PinCapabilities("P8-10", 68),
    PinCapabilities("P8-11", 45, 0),
    PinCapabilities("P8-12", 44, 0),
    PinCapabilities("P8-13", 23),
    PinCapabilities("P8-14", 26),
    PinCapabilities("P8-15", 47),
    PinCapabilities("P8-16", 46),
    PinCapabilities("P8-17", 27),
    PinCapabilities("P8-18", 65),
    PinCapabilities("P8-19", 22),
    PinCapabilities("P8-20", 63, 1),
    PinCapabilities("P8-21", 62, 1),
    PinCapabilities("P8-22", 37),
    PinCapabilities("P8-23", 36),
    PinCapabilities("P8-24", 33),
    PinCapabilities("P8-25", 32),
    PinCapabilities("P8-26", 61),
    PinCapabilities("P8-27", 86, 1),
    PinCapabilities("P8-28", 88, 1),
    PinCapabilities("P8-29", 87, 1),
    PinCapabilities("P8-30", 89, 1),
    PinCapabilities("P8-31", 10),
    PinCapabilities("P8-32", 11),
    PinCapabilities("P8-33", 9),
    PinCapabilities("P8-34", 81),
    PinCapabilities("P8-35", 8),
    PinCapabilities("P8-36", 80),
    PinCapabilities("P8-37", 78),
    PinCapabilities("P8-38", 79),
    PinCapabilities("P8-39", 76, 1),
    PinCapabilities("P8-40", 77, 1),
    PinCapabilities("P8-41", 74, 1),
    PinCapabilities("P8-42", 75, 1),
    PinCapabilities("P8-43", 72, 1),
    PinCapabilities("P8-44", 73, 1),
    PinCapabilities("P8-45", 70, 1),
    PinCapabilities("P8-46", 71, 1),
    PinCapabilities("P9-11", 30),
    PinCapabilities("P9-12", 60),
    PinCapabilities("P9-13", 31),
    PinCapabilities("P9-14", 50),
    PinCapabilities("P9-15", 48),
    PinCapabilities("P9-16", 51),
    PinCapabilities("P9-17", 5),
    PinCapabilities("P9-18", 4),
    PinCapabilities("P9-19", 13),
    PinCapabilities("P9-20", 12),
    PinCapabilities("P9-21", 3),
    PinCapabilities("P9-22", 2),
    PinCapabilities("P9-23", 49),
    PinCapabilities("P9-24", 15),
    PinCapabilities("P9-25", 117, 0),
    PinCapabilities("P9-26", 14),
    PinCapabilities("P9-27", 115, 0),
    PinCapabilities("P9-28", 113, 0),
    PinCapabilities("P9-29", 111, 0),
    PinCapabilities("P9-30", 112, 0),
    PinCapabilities("P9-31", 110, 0),
    PinCapabilities("P9-41", 20),
    PinCapabilities("P9-91", 116, 0),
    PinCapabilities("P9-42", 7),
    PinCapabilities("P9-92", 114, 0),
};

static BeagleBoneType BBB_TYPE = Unknown;

PinCapabilities::PinCapabilities(const std::string &n, uint8_t k, uint8_t pru)
: name(n), kernelGpio(k), pruout(pru)
{
    gpio = k / 32;
    pin = k % 32;
}
PinCapabilities::PinCapabilities(const std::string &n, uint8_t k)
: name(n), kernelGpio(k), pruout(-1) {
    gpio = k / 32;
    pin = k % 32;
}
#define GPIO_DATAIN     0x138
#define GPIO_DATAOUT    0x13C
#define GPIO_SETDATAOUT 0x194
#define GPIO_CLEARDATAOUT 0x190

#define GPIO0_BASE 0x44E07000
#define GPIO1_BASE 0x4804C000
#define GPIO2_BASE 0x481AC000
#define GPIO3_BASE 0x481AE000

#define GPIO_SIZE  0x00001000
static uint32_t ui32Base[] = {GPIO0_BASE, GPIO1_BASE, GPIO2_BASE, GPIO3_BASE};
static uint8_t *bbGPIOMap[] = {0, 0, 0, 0};
static volatile bool gpioMemMapped = false;

static void setupGPIOMemoryMap() {
    if (gpioMemMapped) {
        return;
    }
    int fd = open("/dev/mem", O_RDWR);
    
    for(int ui = 0; ui < 4; ++ui) {
        bbGPIOMap[ui] = (uint8_t*)mmap(0, GPIO_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, ui32Base[ui]);
    }
    
    gpioMemMapped = 1;
}

bool getBBBPinValue(int kio) {
    setupGPIOMemoryMap();
    int gpio = kio / 32;
    int pin = kio % 32;
    int set = 1 << pin;
    
    uint32_t *base = (uint32_t*)(bbGPIOMap[gpio] + GPIO_DATAIN);
    return (*base & set) ? true : false;
}
void setBBBPinValue(int kio, bool v) {
    setupGPIOMemoryMap();
    int gpio = kio / 32;
    int pin = kio % 32;
    int set = 1 << pin;
    
    uint8_t *base = bbGPIOMap[gpio];
    if (v) {
        base += GPIO_SETDATAOUT;
    } else {
        base += GPIO_CLEARDATAOUT;
    }
    uint32_t* base32 = (uint32_t*)base;
    *base32 = set;
}


const PinCapabilities &getBBBPinByName(const std::string &name) {
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
}

const PinCapabilities &getBBBPinKgpio(int i) {
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
}



int configBBBPin(const std::string &name,
                 int gpio, int pin,
                 const std::string &mode,
                 const std::string &direction)
{
    const unsigned pin_num = gpio * 32 + pin;
    
    char pinName[16];
    strcpy(pinName, name.c_str());
    if (pinName[2] == '-') {
        pinName[2] = '_';
    }
    
    char dir_name[128];
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

    snprintf(dir_name, sizeof(dir_name),
             "/sys/class/gpio/gpio%u/direction",
             pin_num
             );
    
    dir = fopen(dir_name, "w");
    if (!dir) {
        return -1;
    }
    fprintf(dir, "%s\n", direction.c_str());
    fclose(dir);
    return 0;
}

int configBBBPin(int kgpio,
                 const std::string& mode,
                 const std::string &direction) {
    const PinCapabilities &pin = getBBBPinKgpio(kgpio);
    return configBBBPin(pin.name, pin.gpio, pin.pin, mode, direction);
}


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



