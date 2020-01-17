/*
 *   serial port utility functions for Falcon Player (FPP)
 *
 *   Copyright (C) 2013-2018 the Falcon Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Player (FPP) is free software; you can redistribute it
 *   and/or modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <linux/serial.h>

#include "log.h"
#include "../util/GPIOUtils.h"

// The following is in asm-generic/termios.h, but including that in a C++
// program causes errors.  The Qt developers worked around this by including
// the following struct definition and #define's manually:
// https://codereview.qt-project.org/#/c/125161/6/src/serialport/qserialport_unix.cpp,unified

#ifndef OLDCUSTOMSPEED
struct termios2 {
    tcflag_t c_iflag;       /* input mode flags */
    tcflag_t c_oflag;       /* output mode flags */
    tcflag_t c_cflag;       /* control mode flags */
    tcflag_t c_lflag;       /* local mode flags */
    cc_t c_line;            /* line discipline */
    cc_t c_cc[19];          /* control characters */
    speed_t c_ispeed;       /* input speed */
    speed_t c_ospeed;       /* output speed */
};

#ifndef TCGETS2
#define TCGETS2     _IOR('T', 0x2A, struct termios2)
#endif

#ifndef TCSETS2
#define TCSETS2     _IOW('T', 0x2B, struct termios2)
#endif

#ifndef BOTHER
#  define BOTHER      0010000
#endif
#endif

/*
 * Portions of code based on work found in xLights/serial_posix.cpp
 * by Matt Brown and Joachim Buermann.
 */

speed_t SerialGetBaudRate(int baud)
{
	switch (baud)
	{
		case    9600: return B9600;
		case   19200: return B19200;
		case   57600: return B57600;
		case  115200: return B115200;
		case  230400: return B230400;
		case 1000000: return B1000000;
		default     : return B38400;
	}
}

/*
 *
 * Example: SerialOpen("/dev/ttyUSB0", 115200, "N81");
 */
int SerialOpen(const char *device, int baud, const char *mode, bool output)
{
    // some devices may multiplex pins and need to
    // specifically configure the output pin as a uart instead of gpio
    char buf[256];
    sprintf(buf, "%s-tx", &device[5]); // "ttyS1-tx" or "ttyUSB0-tx"
    PinCapabilities::getPinByUART(buf).configPin("uart");
    
	int fd = 0;
	struct termios tty;
	speed_t adjustedBaud = SerialGetBaudRate(baud);

	if (strlen(mode) != 3)
	{
		LogErr(VB_CHANNELOUT, "Invalid Serial Port Mode: %s\n", mode);
		return -1;
	}

    fd = open(device, (output ? O_RDWR : O_RDONLY) | O_NOCTTY | O_NONBLOCK);
	if (fd < 0)
		return -1;

	if (ioctl(fd, TIOCEXCL) == -1)
	{
		LogErr(VB_CHANNELOUT, "Error setting port to exclusive mode\n");
		close(fd);
		return -1;
	}

	if (tcgetattr(fd, &tty) == -1)
	{
		LogErr(VB_CHANNELOUT, "Error getting port attributes\n");
		close(fd);
		return -1;
	}

	if (cfsetspeed(&tty, adjustedBaud) == -1)
	{
		LogErr(VB_CHANNELOUT, "Error setting port speed\n");
		close(fd);
		return -1;
	}

	tty.c_cflag &= ~CSIZE;
	switch (mode[0]) {
		case '5':	tty.c_cflag |= CS5;
					break;
		case '6':	tty.c_cflag |= CS6;
					break;
		case '7':	tty.c_cflag |= CS7;
					break;
		default:	tty.c_cflag |= CS8;
					break;
	}

	switch (mode[1]) {
		case 'N':	tty.c_cflag &= ~PARENB;
					break;
		case 'O':	tty.c_cflag |= PARENB;
					tty.c_cflag |= PARODD;
					break;
		case 'E':	tty.c_cflag |= PARENB;
					tty.c_cflag &= ~PARODD;
					break;
	}

	switch (mode[2]) {
		case '2':	tty.c_cflag |= CSTOPB;
					break;
		default:	tty.c_cflag &= ~CSTOPB;
					break;
	}

	cfmakeraw(&tty);

	tty.c_cc[VMIN] = 0;
	tty.c_cc[VTIME] = 0;

	if (tcsetattr(fd, TCSANOW, &tty) == -1)
	{
		LogErr(VB_CHANNELOUT, "Error setting port attributes\n");
		close(fd);
		return -1;
	}

	if ((adjustedBaud == B38400) &&
		(baud != 38400))
	{
		LogInfo(VB_CHANNELOUT, "Using custom baud rate of %d\n", baud);

#ifdef OLDCUSTOMSPEED
		struct serial_struct ss;

		if (ioctl(fd, TIOCGSERIAL, &ss) < 0)
		{
			LogErr(VB_CHANNELOUT, "Error getting serial settings: %s\n",
				strerror(errno));
			close(fd);
			return -1;
		}

		ss.custom_divisor  = ss.baud_base / baud;
		ss.flags          &= ~ASYNC_SPD_MASK;
		ss.flags          |= ASYNC_SPD_CUST;

		if (ioctl(fd, TIOCSSERIAL, &ss) < 0)
		{
			LogErr(VB_CHANNELOUT, "Error setting custom baud rate\n");
			close(fd);
			return -1;
		}
#else
		struct termios2 tio;

		if (ioctl(fd, TCGETS2, &tio) < 0)
		{
			LogErr(VB_CHANNELOUT, "Error getting termios2 settings\n");
			close(fd);
			return -1;
		}
		
		tio.c_cflag &= ~CBAUD;
		tio.c_cflag |= BOTHER;
		tio.c_ispeed = baud;
		tio.c_ospeed = baud;

		if (ioctl(fd, TCSETS2, &tio) < 0)
		{
			LogErr(VB_CHANNELOUT, "Error setting custom baud rate\n");
			close(fd);
			return -1;
		}
#endif
	}

	return fd;
}

int SerialClose(int fd)
{
	if (fd < 0)
		return -1;

	tcflush(fd, TCOFLUSH);

	return close(fd);
}

int SerialSendBreak(int fd, int duration)
{
	ioctl(fd, TIOCSBRK);
	usleep(duration);
	ioctl(fd, TIOCCBRK);

	return 0;
}

int SerialResetRTS(int fd)
{
	int CtrlFlag = TIOCM_RTS;
	if (ioctl(fd, TIOCMBIC, &CtrlFlag ) < 0)
	{
		LogErr(VB_CHANNELOUT, "Error clearing RTS \n");
		return -1;
	}

	return 0;
}

