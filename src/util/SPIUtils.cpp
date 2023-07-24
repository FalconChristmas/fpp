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

#include <fcntl.h>
#include <thread>
#include <unistd.h>

#include "../config.h"

#ifdef HAS_SPI
#include <linux/spi/spidev.h>
#endif
#include <sys/ioctl.h>

#include "SPIUtils.h"

SPIUtils::SPIUtils(int c, int baud) {
    channel = c;
    speed = baud;
    bitsPerWord = 8;
    file = -1;
#ifdef HAS_SPI
    char spiFileName[64];
    snprintf(spiFileName, sizeof(spiFileName), "/dev/spidev0.%d", channel);
    file = open(spiFileName, O_RDWR);
    if (file >= 0) {
        int mode = 0;
        ioctl(file, SPI_IOC_WR_MODE, &mode);
        ioctl(file, SPI_IOC_WR_BITS_PER_WORD, &bitsPerWord);
        ioctl(file, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
    }
#endif
}
SPIUtils::~SPIUtils() {
    if (file != -1) {
        close(file);
    }
}

int SPIUtils::xfer(uint8_t* tx, uint8_t* rx, int count) {
    if (file != -1) {
#ifdef HAS_SPI
        if (rx == nullptr) {
            rx = tx;
        }
        struct spi_ioc_transfer spi;
        channel &= 1;
        memset(&spi, 0, sizeof(spi));

        spi.tx_buf = (unsigned long)tx;
        spi.rx_buf = (unsigned long)rx;
        spi.len = count;
        spi.delay_usecs = 0;
        spi.speed_hz = speed;
        spi.bits_per_word = bitsPerWord;

        return ioctl(file, SPI_IOC_MESSAGE(1), &spi);
#endif
    }
    return -1;
}
