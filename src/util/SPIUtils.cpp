#include <cstring>

#include "SPIUtils.h"

#if defined(USEPIGPIO)
#include <pigpio.h>

SPIUtils::SPIUtils(int channel, int baud) {
    file = spiOpen(channel, baud, 0);
    if (file < 0) {
        file = -1;
    }
}
SPIUtils::~SPIUtils() {
    spiClose(file);
}

int SPIUtils::xfer(uint8_t *tx, uint8_t *rx, int count) {
    if (file != -1) {
        int i = -1;
        if (rx == nullptr) {
            i = spiWrite(file, (char*)tx, count);
        } else {
            i = spiXfer(file, (char*)tx, (char*)rx, count);
        }
        if (i >= 0) {
            return i;
        }
    }
    return -1;
}

#elif defined(USEWIRINGPI)
#    include "wiringPi.h"
#    include "wiringPiSPI.h"

SPIUtils::SPIUtils(int channel, int baud) {
    file = channel;
    if (wiringPiSPISetup (channel, 8000000) < 0) {
        file = -1;
    }
}
SPIUtils::~SPIUtils() {
    
}

int SPIUtils::xfer(uint8_t *tx, uint8_t *rx, int count) {
    if (file != -1) {
        int i = wiringPiSPIDataRW(file, tx, count);
        if (i > 0 && rx && rx != tx) {
            memcpy(rx, tx, i);
        }
        return i;
    }
    return -1;
}

#endif


