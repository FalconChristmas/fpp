#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <thread>

#include "GPIOUtils.h"


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
                          bool directionOut = true) const { return 0;}
    
    virtual bool getValue() const { return false; }
    virtual void setValue(bool i) const {}
    
    virtual bool setupPWM(int maxValueNS = 25500) const {return false;}
    virtual void setPWMValue(int valueNS) const {}
    
    virtual int getPWMRegisterAddress() const { return 0;};
    virtual bool supportPWM() const { return true; };
    
    static void Init() {}
    static const NoPinCapabilities &getPinByName(const std::string &name);
    static const NoPinCapabilities &getPinByGPIO(int i);
};
class NullNoPinCapabilities : public NoPinCapabilities {
public:
    NullNoPinCapabilities() : NoPinCapabilities("-none-", 0) {}
    virtual const PinCapabilities *ptr() const { return nullptr; }
};
static NullNoPinCapabilities NULL_PIN_INSTANCE;

const NoPinCapabilities &NoPinCapabilities::getPinByName(const std::string &name) {
    return NULL_PIN_INSTANCE;
}
const NoPinCapabilities &NoPinCapabilities::getPinByGPIO(int i) {
    return NULL_PIN_INSTANCE;
}

#define PLAT_GPIO_CLASS NoPinCapabilities
#endif



void PinCapabilities::enableOledScreen(int i2cBus, bool enable) {
    //this pin is i2c, we may need to tell fppoled to turn off the display
    //before we shutdown this pin because once we re-configure, i2c will
    //be unavailable and the display won't update
    int smfd = shm_open("fppoled", O_CREAT | O_RDWR, 0);
    ftruncate(smfd, 1024);
    unsigned int *status = (unsigned int *)mmap(0, 1024, PROT_WRITE | PROT_READ, MAP_SHARED, smfd, 0);
    if (i2cBus == status[0]) {
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


const PinCapabilities &PinCapabilities::getPinByName(const std::string &n) {
    return PLAT_GPIO_CLASS::getPinByName(n);
}
const PinCapabilities &PinCapabilities::getPinByGPIO(int i) {
    return PLAT_GPIO_CLASS::getPinByGPIO(i);
}
