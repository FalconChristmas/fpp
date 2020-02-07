#include <fcntl.h>
#include <unistd.h>
#include <stropts.h>

#include <vector>

#include "SPIUtils.h"
#include "MCP23x17Utils.h"


// SPI Command codes
#define CMD_WRITE   0x40
#define CMD_READ    0x41

// MCP23x17 Registers
#define    MCP23x17_IODIRA     0x00
#define    MCP23x17_IPOLA      0x02
#define    MCP23x17_GPINTENA   0x04
#define    MCP23x17_DEFVALA    0x06
#define    MCP23x17_INTCONA    0x08
#define    MCP23x17_IOCON      0x0A
#define    MCP23x17_GPPUA      0x0C
#define    MCP23x17_INTFA      0x0E
#define    MCP23x17_INTCAPA    0x10
#define    MCP23x17_GPIOA      0x12
#define    MCP23x17_OLATA      0x14

#define    MCP23x17_IODIRB     0x01
#define    MCP23x17_IPOLB      0x03
#define    MCP23x17_GPINTENB   0x05
#define    MCP23x17_DEFVALB    0x07
#define    MCP23x17_INTCONB    0x09
#define    MCP23x17_IOCONB     0x0B
#define    MCP23x17_GPPUB      0x0D
#define    MCP23x17_INTFB      0x0F
#define    MCP23x17_INTCAPB    0x11
#define    MCP23x17_GPIOB      0x13
#define    MCP23x17_OLATB      0x15

// Bits in the IOCON register
#define    IOCON_UNUSED    0x01
#define    IOCON_INTPOL    0x02
#define    IOCON_ODR       0x04
#define    IOCON_HAEN      0x08
#define    IOCON_DISSLW    0x10
#define    IOCON_SEQOP     0x20
#define    IOCON_MIRROR    0x40
#define    IOCON_BANK_MODE 0x80

// Default initialisation mode
#define    IOCON_INIT    (IOCON_SEQOP)



static SPIUtils *MCP23x17_SPI = nullptr;
static uint8_t status_port_a;
static uint8_t status_port_b;

static int writeByte(uint8_t reg, uint8_t data, uint8_t devId = 0)
{
    if (MCP23x17_SPI) {
        uint8_t spiData [4] ;
    
        spiData [0] = CMD_WRITE | ((devId & 7) << 1) ;
        spiData [1] = reg ;
        spiData [2] = data ;
        return MCP23x17_SPI->xfer(spiData, spiData, 3);
    }
    return -1;
}
static int32_t readByte(uint8_t reg, uint8_t devId = 0)
{
    if (MCP23x17_SPI) {
        uint8_t spiData [4] = {0, 0, 0, 0} ;
        spiData [0] = CMD_READ | ((devId & 7) << 1) ;
        spiData [1] = reg ;
        int i = MCP23x17_SPI->xfer(spiData, spiData, 3);
        if (i == -1) {
            return -1;
        }
        return spiData[2] ;
    }
    return -1;
}


MCP23x17PinCapabilities::MCP23x17PinCapabilities(const std::string &n, uint32_t kg, int base)
: PinCapabilitiesFluent(n, kg), gpioBase(base) {
}


int MCP23x17PinCapabilities::configPin(const std::string& mode,
                                     bool directionOut) const {
    if (mode == "pwm" || mode == "uart") {
        return 0;
    }
    int pin = this->kernelGpio - gpioBase;
    int reg = (pin < 8) ? MCP23x17_IODIRA : MCP23x17_IODIRB;
    int pureg = (pin < 8) ? MCP23x17_GPPUA : MCP23x17_GPPUB;
    if (pin > 7) {
        pin &= 0x07;
    }
    int mask = 1 << pin;
    int old = readByte(reg) ;
    
    if (directionOut) {
        old &= (~mask);
    } else {
        old |= mask;
    }
    writeByte(reg, old);
    
    
    old = readByte(pureg) ;
    if (mode == "gpio_pu") {
        old |= mask;
    } else {
        old &= (~mask);
    }
    writeByte(pureg, old);
    return 0;
}

bool MCP23x17PinCapabilities::getValue() const {
    int pin = this->kernelGpio - gpioBase;
    int gpio = (pin < 8) ? MCP23x17_GPIOA : MCP23x17_GPIOB;
    if (pin > 7) {
        pin &= 0x07;
    }
    int mask = 1 << pin;
    int value = readByte(gpio);
    return ((value & mask) != 0);
}
void MCP23x17PinCapabilities::setValue(bool i) const {
    int pin = this->kernelGpio - gpioBase;
    int bit = 1 << (pin & 7) ;
    int old = (pin < 8) ? status_port_a : status_port_b;
    if (i == 0) {
        old &= (~bit) ;
    } else {
        old |= bit ;
    }
    
    if (pin < 8) {
        writeByte(MCP23x17_GPIOA, old) ;
        status_port_a = old ;
    } else {
        writeByte(MCP23x17_GPIOB, old) ;
        status_port_b = old ;
    }
}



class NullMCP23x17GPIOPinCapabilities : public MCP23x17PinCapabilities {
public:
    NullMCP23x17GPIOPinCapabilities() : MCP23x17PinCapabilities("-none-", 0, 0) {}
    virtual const PinCapabilities *ptr() const override { return nullptr; }
};
static NullMCP23x17GPIOPinCapabilities NULL_WP_INSTANCE;


static std::vector<MCP23x17PinCapabilities> MCP23x17_PINS;

void MCP23x17PinCapabilities::Init(int base) {
    MCP23x17_SPI = new SPIUtils(0, 4000000);
    if (MCP23x17_SPI->isOk()) {
        writeByte(MCP23x17_IOCON,  IOCON_INIT | IOCON_HAEN) ;
        writeByte(MCP23x17_IOCONB, IOCON_INIT | IOCON_HAEN) ;

        int iA = readByte(MCP23x17_OLATA);
        int iB = readByte(MCP23x17_OLATB);
        
        if (iA == -1 || iB == -1) {
            delete MCP23x17_SPI;
            MCP23x17_SPI = nullptr;
            return;
        }
        status_port_a = iA;
        status_port_b = iB;
        
        MCP23x17PinCapabilities testPin("MCP23x17-1", 215, 215);
        testPin.configPin("gpio");
        int a = testPin.getValue();
        testPin.setValue(1);
        int b = testPin.getValue();
        testPin.setValue(0);
        int c = testPin.getValue();

        if (a == b && b == c) {
            //could not change the value, assume no PiFace found
            delete MCP23x17_SPI;
            MCP23x17_SPI = nullptr;
            return;
        }

        for (int x = 0; x < 16; x++) {
            std::string name = "MCP23x17-" + std::to_string(x + 1);
            MCP23x17_PINS.push_back(MCP23x17PinCapabilities(name, base + x, base));
        }
    } else {
        delete MCP23x17_SPI;
        MCP23x17_SPI = nullptr;
    }
}
void MCP23x17PinCapabilities::getPinNames(std::vector<std::string> &ret) {
    for (auto &a : MCP23x17_PINS) {
        ret.push_back(a.name);
    }
}

const PinCapabilities &MCP23x17PinCapabilities::getPinByName(const std::string &name) {
    for (auto &a : MCP23x17_PINS) {
        if (a.name == name) {
            return a;
        }
    }
    return NULL_WP_INSTANCE;
}
const PinCapabilities &MCP23x17PinCapabilities::getPinByGPIO(int i) {
    for (auto &a : MCP23x17_PINS) {
        if (a.kernelGpio == i) {
            return a;
        }
    }
    return NULL_WP_INSTANCE;
}

const PinCapabilities &MCP23x17PinCapabilities::getPinByUART(const std::string &n) {
    return NULL_WP_INSTANCE;
}
