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

#include "Warnings.h"       // WarningHolder -- needed directly for NOPCH builds
#include "../common_mini.h" // FPPstrerror() -- needed directly for NOPCH builds

#include <sys/ioctl.h>
#include <algorithm>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdlib.h>
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

// The kernel console carries boot messages and a login prompt.  On a
// PocketBeagle 2 it is ttyS2, the name a PocketBeagle cape used for DMX, so a
// config carried over from one would otherwise mix lighting data into it.
bool SerialIsConsole(const char* device) {
#ifdef PLATFORM_OSX
    return false;
#else
    char resolved[PATH_MAX];
    if (!realpath(device, resolved)) {
        return false;
    }
    const char* name = strrchr(resolved, '/');
    name = name ? name + 1 : resolved;
    FILE* f = fopen("/sys/class/tty/console/active", "r");
    if (!f) {
        return false;
    }
    char con[64];
    bool found = false;
    while (!found && fscanf(f, "%63s", con) == 1) {
        found = strcmp(con, name) == 0;
    }
    fclose(f);
    return found;
#endif
}

/*
 *
 * Example: SerialOpen("/dev/ttyUSB0", 115200, "N81");
 */
int SerialOpen(const char* device, int baud, const char* mode, bool output, bool configurePin, const char* desc) {
    if (SerialIsConsole(device)) {
        // permanent, like the other open failures here: the console is set
        // on the kernel command line and the config has to change
        LogErr(VB_CHANNELOUT, "%s: is the system console, not opening it\n", device);
        WarningHolder::AddWarning(38, std::string(device) + ": is the system console and cannot be used for channel data");
        return -1;
    }
    // some devices may multiplex pins and need to
    // specifically configure the output pin as a uart instead of gpio
    char buf[256];
    if (output) {
        snprintf(buf, sizeof(buf), "%s-tx", &device[5]); // "ttyS1-tx" or "ttyUSB0-tx"
    } else {
        snprintf(buf, sizeof(buf), "%s-rx", &device[5]); // "ttyS1-rx" or "ttyUSB0-rx"
    }
    if (configurePin && strncmp(device, "/dev/ttyUSB", 11) != 0) {
        // don't attempt to configure USB uarts  as they wouldn't have a pin.
        // Nor anything else with no UART pin: a USB adapter named by its
        // stable /dev/serial/by-id path, or a ttyACM, finds no pin, and
        // configuring the empty placeholder raises a warning banner.
        const PinCapabilities& pin = PinCapabilities::getPinByUART(buf);
        if (pin.ptr()) {
            pin.configPin("uart", output, desc);
        }
    }

    int fd = 0;
    struct termios tty;
    speed_t adjustedBaud = SerialGetBaudRate(baud);

    if (strlen(mode) != 3) {
        LogErr(VB_CHANNELOUT, "%s: Invalid Serial Port Mode: %s\n", device, mode);
        WarningHolder::AddWarning(38, std::string(device) + ": invalid serial port mode '" + mode + "'");
        return -1;
    }

    fd = open(device, (output ? O_RDWR : O_RDONLY) | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        // errno has to be captured before LogErr, which can clobber it
        int err = errno;
        LogErr(VB_CHANNELOUT, "%s: Error opening serial port: %s\n", device, FPPstrerror(err));
        WarningHolder::AddWarning(38, std::string(device) + ": could not open serial port (" + FPPstrerror(err) + ")");
        return -1;
    }

    if (ioctl(fd, TIOCEXCL) == -1) {
        LogErr(VB_CHANNELOUT, "%s: Error setting port to exclusive mode\n", device);
        WarningHolder::AddWarning(38, std::string(device) + ": could not set serial port to exclusive mode (in use by another process?)");
        close(fd);
        return -1;
    }

    if (tcgetattr(fd, &tty) == -1) {
        LogErr(VB_CHANNELOUT, "%s: Error getting port attributes\n", device);
        WarningHolder::AddWarning(38, std::string(device) + ": could not read serial port attributes");
        close(fd);
        return -1;
    }

    if (cfsetspeed(&tty, adjustedBaud) == -1) {
        LogErr(VB_CHANNELOUT, "%s: Error setting port speed\n", device);
        WarningHolder::AddWarning(38, std::string(device) + ": could not set serial port speed");
        close(fd);
        return -1;
    }

    // Raw mode first: cfmakeraw() resets the character size to 8 bits and
    // clears parity, so applied after the mode below it turned "7E1" or "8E1"
    // into 8N1.
    cfmakeraw(&tty);

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

    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;
    if (!output) {
        tty.c_iflag |= BRKINT;
    }

    if (tcsetattr(fd, TCSANOW, &tty) == -1) {
        LogErr(VB_CHANNELOUT, "%s: Error setting port attributes\n", device);
        WarningHolder::AddWarning(38, std::string(device) + ": could not set serial port attributes");
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
            WarningHolder::AddWarning(38, std::string(device) + ": could not set custom baud rate");
            close(fd);
            return -1;
        }
#endif
    }

    return fd;
}
int SerialOpen(const char* device, int baud, const char* mode, bool output) {
    return SerialOpen(device, baud, mode, output, true, "");
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

int SerialWriteAll(int fd, const void* data, int len, int timeoutMs) {
    if (fd < 0) {
        return -1;
    }
    const char* buf = (const char*)data;
    uint64_t deadline = GetMonotonicTimeMS() + timeoutMs;
    int sent = 0;
    while (sent < len) {
        ssize_t w = write(fd, buf + sent, len - sent);
        if (w > 0) {
            sent += w;
            continue;
        }
        if (w < 0 && errno == EINTR) {
            continue;
        }
        if (w < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            break;
        }
        long long left = (long long)deadline - (long long)GetMonotonicTimeMS();
        if (left <= 0) {
            break;
        }
        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLOUT;
        pfd.revents = 0;
        poll(&pfd, 1, (int)std::min<long long>(left, 100));
    }
    return sent;
}

bool SerialWaitDrained(int fd, int timeoutMs) {
    uint64_t deadline = GetMonotonicTimeMS() + timeoutMs;
    while (true) {
        int queued = 0;
        if (ioctl(fd, TIOCOUTQ, &queued) < 0 || queued <= 0) {
            return true;
        }
        if (GetMonotonicTimeMS() >= deadline) {
            return false;
        }
        usleep(500);
    }
}
