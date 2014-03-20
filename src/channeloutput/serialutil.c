#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <linux/serial.h>

#include "../log.h"

/*
 * Portions of code based on work found in xLights/serial_posix.cpp
 * by Matt Brown and Joachim Buermann.
 */

speed_t SerialGetBaudRate(int baud)
{
	switch (baud)
	{
		case   9600:	return B9600;
		case  19200:	return B19200;
		case  57600:	return B57600;
		case 115200:	return B115200;
		case 230400:	return B230400;
		default    :	return B38400;
	}
}

/*
 *
 * Example: SerialOpen("/dev/ttyUSB0", 115200, "N81");
 */
int SerialOpen(char *device, int baud, char *mode)
{
	int fd = 0;
	struct termios tty;
	speed_t adjustedBaud = SerialGetBaudRate(baud);

	if (strlen(mode) != 3)
	{
		LogErr(VB_CHANNELOUT, "Invalid Serial Port Mode: %s\n", mode);
		return -1;
	}

	fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
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

