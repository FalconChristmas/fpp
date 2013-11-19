#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "log.h"
#include "settings.h"

extern char fileData[65536];

enum {
	kDMXUSBDongle,
	kPixelnetUSBDongle
};

int usbDongleType = kDMXUSBDongle;
int usbSerialPortFd = -1;
char dmxHeader[5];
char dmxFooter[1];

//////////////////////////////////////////////////////////////////////////////
// The set_interface_attribs and set_blocking functions at the bottom of this
// file are courtesy of a post from 'wallyk' on StackOverflow.com.  Minor
// modifications have been made to use our logging, indention, etc..
//////////////////////////////////////////////////////////////////////////////
int set_interface_attribs (int fd, int speed, int parity);
void set_blocking (int fd, int should_block);
//////////////////////////////////////////////////////////////////////////////

int InitializeUSBDongle(void)
{
	char deviceFilename[38];

	if (!strcmp(getUSBDonglePort(), "DISABLED"))
		return(0);

	LogWrite("Initializing %s USB Dongle on %s\n",
		getUSBDongleType(), getUSBDonglePort());

	strcpy(deviceFilename, "/dev/");
	strncat(deviceFilename, getUSBDonglePort(), 32);
	usbSerialPortFd = open(deviceFilename, O_RDWR | O_NOCTTY | O_SYNC);
	if (usbSerialPortFd < 0)
	{
		LogWrite("Error %d opening %s: %s\n",
			errno, getUSBDonglePort(), strerror (errno));
		return(-1);
	}

	// set speed to 115,200 bps, 8n1 (no parity)
	set_interface_attribs(usbSerialPortFd, B115200, 0);

	// set non-blocking (if we ever need to read)
	set_blocking(usbSerialPortFd, 0);

	if (!strcmp(getUSBDongleType(), "DMX"))
	{
		// Hard-coded to sending 512 bytes for 2013
		int len = 512;

		usbDongleType = kDMXUSBDongle;
		dmxHeader[0] = 0x7E;
		dmxHeader[1] = 0x06;
		dmxHeader[2] = len & 0xFF;
		dmxHeader[3] = (len >> 8) & 0xFF;
		dmxHeader[4] = 0x00;

		dmxFooter[0] = 0xE7;
	}
	else if (!strcmp(getUSBDongleType(), "Pixelnet"))
	{
		usbDongleType = kPixelnetUSBDongle;
	}
}

void ShutdownUSBDongle(void)
{
	close(usbSerialPortFd);
	usbSerialPortFd = -1;
}

void SendUSBDongle(void)
{
	int i;

	if (usbDongleType == kDMXUSBDongle)
	{
		write(usbSerialPortFd, dmxHeader, sizeof(dmxHeader));
		write(usbSerialPortFd, fileData, 512); // Send DMX Universe #1
		write(usbSerialPortFd, dmxFooter, sizeof(dmxFooter));
	}
	else if (usbDongleType == kPixelnetUSBDongle)
	{
		// 0xAA is start of Pixelnet packet, so convert 0xAA (170) to 0xAB (171)
		for( i = 0; i < 4096; i++ )
		{
			if (fileData[i] == '\xAA')
				fileData[i] = '\xAB';
		}

		write(usbSerialPortFd, "\xAA", 1);      // Send start of packet byte
		write(usbSerialPortFd, fileData, 4096); // Send Pixelnet Universe #1
	}
}

int IsUSBDongleActive(void)
{
	if (usbSerialPortFd != -1)
		return 1;
	
	return 0;
}

//////////////////////////////////////////////////////////////////////////////
// The set_interface_attribs and set_blocking code functions are courtesy
// of a post from 'wallyk' on StackOverflow.com.  Minor modifications have
// been made to use our logging, indention, etc..
//////////////////////////////////////////////////////////////////////////////
int set_interface_attribs(int fd, int speed, int parity)
{
	struct termios tty;
	memset (&tty, 0, sizeof tty);
	if (tcgetattr (fd, &tty) != 0)
	{
		LogWrite("Error %d from tcgetattr\n", errno);
		return -1;
	}

	cfsetospeed (&tty, speed);
	cfsetispeed (&tty, speed);

	tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
	// disable IGNBRK for mismatched speed tests; otherwise receive break
	// as \000 chars
	tty.c_iflag &= ~IGNBRK;         // ignore break signal
	tty.c_lflag = 0;                // no signaling chars, no echo,
	                                // no canonical processing
	tty.c_oflag = 0;                // no remapping, no delays
	tty.c_cc[VMIN]  = 0;            // read doesn't block
	tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

	tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

	tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
	                                // enable reading
	tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
	tty.c_cflag |= parity;
	tty.c_cflag &= ~CSTOPB;
	tty.c_cflag &= ~CRTSCTS;

	if (tcsetattr (fd, TCSANOW, &tty) != 0)
	{
		LogWrite("Error %d from tcsetattr\n", errno);
		return -1;
	}
	return 0;
}

void set_blocking(int fd, int should_block)
{
	struct termios tty;
	memset (&tty, 0, sizeof tty);
	if (tcgetattr (fd, &tty) != 0)
	{
		LogWrite("Error %d from tggetattr\n", errno);
		return;
	}

	tty.c_cc[VMIN]  = should_block ? 1 : 0;
	tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

	if (tcsetattr (fd, TCSANOW, &tty) != 0)
		LogWrite("Error %d setting term attributes\n", errno);
}



