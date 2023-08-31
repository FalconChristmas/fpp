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

#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "../log.h"
#include "../util/GPIOUtils.h"

#ifdef PLATFORM_OSX
#include <IOKit/serial/ioss.h>
#else
// The following is in asm-generic/termios.h, but including that in a C++
// program causes errors.  The Qt developers worked around this by including
// the following struct definition and #define's manually:
// https://codereview.qt-project.org/#/c/125161/6/src/serialport/qserialport_unix.cpp,unified

struct termios2 {
    tcflag_t c_iflag; /* input mode flags */
    tcflag_t c_oflag; /* output mode flags */
    tcflag_t c_cflag; /* control mode flags */
    tcflag_t c_lflag; /* local mode flags */
    cc_t c_line;      /* line discipline */
    cc_t c_cc[19];    /* control characters */
    speed_t c_ispeed; /* input speed */
    speed_t c_ospeed; /* output speed */
};

#ifndef TCGETS2
#define TCGETS2 _IOR('T', 0x2A, struct termios2)
#endif

#ifndef TCSETS2
#define TCSETS2 _IOW('T', 0x2B, struct termios2)
#endif

#ifndef BOTHER
#define BOTHER 0010000
#endif
#endif

/*
 * Portions of code based on work found in xLights/serial_posix.cpp
 * by Matt Brown and Joachim Buermann.
 */

speed_t SerialGetBaudRate(int baud) {
    switch (baud) {
    case 9600:
        return B9600;
    case 19200:
        return B19200;
    case 57600:
        return B57600;
    case 115200:
        return B115200;
    case 230400:
        return B230400;
#ifndef PLATFORM_OSX
    case 1000000:
        return B1000000;
#endif
    default:
        return B38400;
    }
}

/*
 *
 * Example: SerialOpen("/dev/ttyUSB0", 115200, "N81");
 */
int SerialOpen(const char* device, int baud, const char* mode, bool output) {
    // some devices may multiplex pins and need to
    // specifically configure the output pin as a uart instead of gpio
    char buf[256];
    if (output) {
        snprintf(buf, sizeof(buf), "%s-tx", &device[5]); // "ttyS1-tx" or "ttyUSB0-tx"
    } else {
        snprintf(buf, sizeof(buf), "%s-rx", &device[5]); // "ttyS1-rx" or "ttyUSB0-rx"
    }
    PinCapabilities::getPinByUART(buf).configPin("uart");

    int fd = 0;
    struct termios tty;
    speed_t adjustedBaud = SerialGetBaudRate(baud);

    if (strlen(mode) != 3) {
        LogErr(VB_CHANNELOUT, "%s: Invalid Serial Port Mode: %s\n", device, mode);
        return -1;
    }

    fd = open(device, (output ? O_RDWR : O_RDONLY) | O_NOCTTY | O_NONBLOCK);
    if (fd < 0)
        return -1;

    if (ioctl(fd, TIOCEXCL) == -1) {
        LogErr(VB_CHANNELOUT, "%s: Error setting port to exclusive mode\n", device);
        close(fd);
        return -1;
    }

    if (tcgetattr(fd, &tty) == -1) {
        LogErr(VB_CHANNELOUT, "%s: Error getting port attributes\n", device);
        close(fd);
        return -1;
    }

    if (cfsetspeed(&tty, adjustedBaud) == -1) {
        LogErr(VB_CHANNELOUT, "%s: Error setting port speed\n", device);
        close(fd);
        return -1;
    }

    tty.c_cflag &= ~CSIZE;
    switch (mode[0]) {
    case '5':
        tty.c_cflag |= CS5;
        break;
    case '6':
        tty.c_cflag |= CS6;
        break;
    case '7':
        tty.c_cflag |= CS7;
        break;
    default:
        tty.c_cflag |= CS8;
        break;
    }

    switch (mode[1]) {
    case 'N':
        tty.c_cflag &= ~PARENB;
        break;
    case 'O':
        tty.c_cflag |= PARENB;
        tty.c_cflag |= PARODD;
        break;
    case 'E':
        tty.c_cflag |= PARENB;
        tty.c_cflag &= ~PARODD;
        break;
    }

    switch (mode[2]) {
    case '2':
        tty.c_cflag |= CSTOPB;
        break;
    default:
        tty.c_cflag &= ~CSTOPB;
        break;
    }

    cfmakeraw(&tty);

    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &tty) == -1) {
        LogErr(VB_CHANNELOUT, "%s: Error setting port attributes\n", device);
        close(fd);
        return -1;
    }

    if ((adjustedBaud == B38400) &&
        (baud != 38400)) {
        LogInfo(VB_CHANNELOUT, "%s: Using custom baud rate of %d\n", device, baud);

#ifdef PLATFORM_OSX
        if (ioctl(fd, IOSSIOSPEED, &adjustedBaud) == -1) {
            LogErr(VB_CHANNELOUT, "Error %d calling ioctl( ..., IOSSIOSPEED, ... )\n", errno);
            close(fd);
            return -1;
        }
#else
        struct termios2 tio;

        if (ioctl(fd, TCGETS2, &tio) < 0) {
            LogErr(VB_CHANNELOUT, "Error getting termios2 settings\n");
            close(fd);
            return -1;
        }

        tio.c_cflag &= ~CBAUD;
        tio.c_cflag |= BOTHER;
        tio.c_ispeed = baud;
        tio.c_ospeed = baud;

        if (ioctl(fd, TCSETS2, &tio) < 0) {
            LogErr(VB_CHANNELOUT, "%s: Error setting custom baud rate\n", device);
            close(fd);
            return -1;
        }
#endif
    }

    return fd;
}

int SerialClose(int fd) {
    if (fd < 0)
        return -1;

    tcflush(fd, TCOFLUSH);

    return close(fd);
}

int SerialSendBreak(int fd, int duration) {
    ioctl(fd, TIOCSBRK);
    usleep(duration);
    ioctl(fd, TIOCCBRK);

    return 0;
}

int SerialResetRTS(int fd) {
    int CtrlFlag = TIOCM_RTS;
    if (ioctl(fd, TIOCMBIC, &CtrlFlag) < 0) {
        LogErr(VB_CHANNELOUT, "Error clearing RTS \n");
        return -1;
    }

    return 0;
}
