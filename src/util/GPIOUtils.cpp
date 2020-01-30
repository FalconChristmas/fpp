#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <thread>

#include "GPIOUtils.h"
#include "commands/Commands.h"

#if defined(PLATFORM_BBB)
#include "BBBUtils.h"
#define PLAT_GPIO_CLASS BBBPinCapabilities
#elif defined(USEPIGPIO)
#include "PiGPIOUtils.h"
#define PLAT_GPIO_CLASS PiGPIOPinCapabilities
#elif defined(USEWIRINGPI)
#include "WiringPiGPIO.h"
#define PLAT_GPIO_CLASS WPPinCapabilities
#else
// No platform information on how to control pins
class NoPinCapabilities : public PinCapabilitiesFluent<NoPinCapabilities> {
public:
    NoPinCapabilities(const std::string &n, uint32_t kg) : PinCapabilitiesFluent(n, kg)
    {}
    
    
    virtual int configPin(const std::string& mode = "gpio",
                          bool directionOut = true) const override { return 0;}
    
    virtual bool getValue() const override { return false; }
    virtual void setValue(bool i) const override {}
    
    virtual bool setupPWM(int maxValueNS = 25500) const override {return false;}
    virtual void setPWMValue(int valueNS) const override {}
    
    virtual int getPWMRegisterAddress() const override { return 0;};
    virtual bool supportPWM() const override { return true; };
    
    static void Init() {}
    static const NoPinCapabilities &getPinByName(const std::string &name);
    static const NoPinCapabilities &getPinByGPIO(int i);
    static const NoPinCapabilities &getPinByUART(const std::string &n);
    static std::vector<std::string> getPinNames();

};
class NullNoPinCapabilities : public NoPinCapabilities {
public:
    NullNoPinCapabilities() : NoPinCapabilities("-none-", 0) {}
    virtual const PinCapabilities *ptr() const override { return nullptr; }
};
static NullNoPinCapabilities NULL_PIN_INSTANCE;

const NoPinCapabilities &NoPinCapabilities::getPinByName(const std::string &name) {
    return NULL_PIN_INSTANCE;
}
const NoPinCapabilities &NoPinCapabilities::getPinByGPIO(int i) {
    return NULL_PIN_INSTANCE;
}
const NoPinCapabilities &NoPinCapabilities::getPinByUART(const std::string &n) {
    return NULL_PIN_INSTANCE;
}

std::vector<std::string> NoPinCapabilities::getPinNames() {
    return std::vector<std::string>();
}


#define PLAT_GPIO_CLASS NoPinCapabilities
#endif

Json::Value PinCapabilities::toJSON() const {
    Json::Value ret;
    if (name != "" && name != "-non-") {
        ret["pin"] = name;
        ret["gpio"] = kernelGpio;
        ret["gpioChip"] = gpioIdx;
        ret["gpioLine"] = gpio;
        if (pru != -1) {
            ret["pru"] = pru;
            ret["pruPin"] = pruPin;
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
    }
    return ret;
}


void PinCapabilities::enableOledScreen(int i2cBus, bool enable) {
    //this pin is i2c, we may need to tell fppoled to turn off the display
    //before we shutdown this pin because once we re-configure, i2c will
    //be unavailable and the display won't update
    int smfd = shm_open("fppoled", O_CREAT | O_RDWR, 0);
    ftruncate(smfd, 1024);
    unsigned int *status = (unsigned int *)mmap(0, 1024, PROT_WRITE | PROT_READ, MAP_SHARED, smfd, 0);
    if (i2cBus == status[0]) {
        printf("Signal to fppoled to enable/disable I2C:   Bus: %d   Enable: %d\n", i2cBus, enable);
        if (!enable) {
            //force the display off
            status[2] = 1;
            int count = 0;
            while (status[1] != 0 && count < 150) {
                count++;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        } else {
            //allow the display to come back on
            status[2] = 0;
        }
    }
    close(smfd);
    munmap(status, 1024);
}



void PinCapabilities::InitGPIO() {
    PLAT_GPIO_CLASS::Init();
}
std::vector<std::string> PinCapabilities::getPinNames() {
    return PLAT_GPIO_CLASS::getPinNames();
}

const PinCapabilities &PinCapabilities::getPinByName(const std::string &n) {
    return PLAT_GPIO_CLASS::getPinByName(n);
}
const PinCapabilities &PinCapabilities::getPinByGPIO(int i) {
    return PLAT_GPIO_CLASS::getPinByGPIO(i);
}
const PinCapabilities &PinCapabilities::getPinByUART(const std::string &n) {
    return PLAT_GPIO_CLASS::getPinByUART(n);
}
