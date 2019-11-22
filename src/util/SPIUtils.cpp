#include <cstring>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#include "SPIUtils.h"

#if !defined(USEWIRINGPI)

SPIUtils::SPIUtils(int c, int baud) {
    channel = c;
    speed = baud;
    bitsPerWord = 8;
    char spiFileName[64] ;
    sprintf(spiFileName, "/dev/spidev0.%d", channel) ;
    file = open(spiFileName, O_RDWR);
    if (file >= 0) {
        int mode = 0;
        ioctl(file, SPI_IOC_WR_MODE, &mode);
        ioctl(file, SPI_IOC_WR_BITS_PER_WORD, &bitsPerWord);
        ioctl(file, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
    }
}
SPIUtils::~SPIUtils() {
    if (file != -1) {
        close(file);
    }
}

int SPIUtils::xfer(uint8_t *tx, uint8_t *rx, int count) {
    if (file != -1) {
        if (rx == nullptr) {
            rx = tx;
        }
        struct spi_ioc_transfer spi ;
        channel &= 1 ;
        memset(&spi, 0, sizeof (spi));
        
        spi.tx_buf        = (unsigned long)tx ;
        spi.rx_buf        = (unsigned long)rx ;
        spi.len           = count ;
        spi.delay_usecs   = 0 ;
        spi.speed_hz      = speed;
        spi.bits_per_word = bitsPerWord ;
        
        return ioctl(file, SPI_IOC_MESSAGE(1), &spi) ;
    }
    return -1;
}

#else
#include "wiringPi.h"
#include "wiringPiSPI.h"

SPIUtils::SPIUtils(int channel, int baud) {
    this->channel = channel;
    file = channel;
    speed = baud;
    bitsPerWord = 8;
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


