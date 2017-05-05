/*
 *   ILI9488 Channel Output driver for Falcon Player (FPP)
 *
 *   Copyright (C) 2013 the Falcon Player Developers
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

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "common.h"
#include "ILI9488.h"
#include "log.h"

#define BCM2708_PERI_BASE        0x20000000
#define GPIO_BASE                (BCM2708_PERI_BASE + 0x200000) /* GPIO controller */

#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)

// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
#define INP_GPIO(g) *(m_gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(m_gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) *(m_gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))

#define GPIO_SET *(m_gpio+7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR *(m_gpio+10) // clears bits which are 1 ignores bits which are 0

#define GET_GPIO(g) (*(m_gpio+13)&(1<<g)) // 0 if LOW, (1<<g) if HIGH

#define GPIO_PULL *(m_gpio+37) // Pull up/pull down
#define GPIO_PULLCLK0 *(m_gpio+38) // Pull up/pull down clock

#define ILI_PIN_CSX  4
#define ILI_PIN_WRX 17
#define ILI_PIN_DCX 18
#define ILI_PIN_D1  22
#define ILI_PIN_D2  23
#define ILI_PIN_D3  24
#define ILI_PIN_D4  10
#define ILI_PIN_D5  25
#define ILI_PIN_D6   9
#define ILI_PIN_D7  11
#define ILI_PIN_D8   8
#define ILI_PIN_RST  7

const unsigned ILI_dataBits = \
        1 << ILI_PIN_D1 | \
        1 << ILI_PIN_D2 | \
        1 << ILI_PIN_D3 | \
        1 << ILI_PIN_D4 | \
        1 << ILI_PIN_D5 | \
        1 << ILI_PIN_D6 | \
        1 << ILI_PIN_D7 | \
        1 << ILI_PIN_D8;

unsigned ILI_dataValues[256];

const int ILI_allPins[12] = { ILI_PIN_CSX, ILI_PIN_WRX, ILI_PIN_DCX, ILI_PIN_D1, ILI_PIN_D2, ILI_PIN_D3, ILI_PIN_D4, ILI_PIN_D5, ILI_PIN_D6, ILI_PIN_D7, ILI_PIN_D8, ILI_PIN_RST };
const int ILI_dataPins[8] = { ILI_PIN_D1, ILI_PIN_D2, ILI_PIN_D3, ILI_PIN_D4, ILI_PIN_D5, ILI_PIN_D6, ILI_PIN_D7, ILI_PIN_D8 };

/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
ILI9488Output::ILI9488Output(unsigned int startChannel, unsigned int channelCount)
  : ChannelOutputBase(startChannel, channelCount),
	m_rows(320),
	m_cols(480),
	m_pixels(0),
	m_gpio_map(NULL),
	m_gpio(NULL)
{
	LogDebug(VB_CHANNELOUT, "ILI9488Output::ILI9488Output(%u, %u)\n",
		startChannel, channelCount);

	m_pixels = m_rows * m_cols;
	m_maxChannels = m_pixels * 3;
}

/*
 *
 */
ILI9488Output::~ILI9488Output()
{
	LogDebug(VB_CHANNELOUT, "ILI9488Output::~ILI9488Output()\n");
}

/*
 *
 */
int ILI9488Output::Init(Json::Value config)
{
	LogDebug(VB_CHANNELOUT, "ILI9488Output::Init(JSON)\n");

	ILI9488_Init();

	return ChannelOutputBase::Init(config);
}

/*
 *
 */
int ILI9488Output::Close(void)
{
	LogDebug(VB_CHANNELOUT, "ILI9488Output::Close()\n");

	ILI9488_Cleanup();

	return ChannelOutputBase::Close();
}

/*
 *
 */
int ILI9488Output::RawSendData(unsigned char *channelData)
{
	LogExcess(VB_CHANNELOUT, "ILI9488Output::RawSendData(%p)\n", channelData);

	if (!m_gpio)
		return 0;

	unsigned char *color24c = channelData;
	unsigned int *color24 = (unsigned int*)color24c;
	int i = 0;
	int x = 0;
	int y = 0;
	unsigned int color16 = 0x00;

	ILI9488_Command(0x2c); // Write_memory_start

	for (int y = 0; y < m_rows; y++)
	{
		for (int x = 0; x < m_cols; x++)
		{
			// Use only the upper 3 bytes of color24 for RGB
			color16  = (*color24 >> 16) & 0b1111100000000000;
			color16 |= (*color24 >> 13) & 0b0000011111100000;
			color16 |= (*color24 >> 11) & 0b0000000000011111;

       			ILI9488_Data(color16 & 0x00FF);  // low part of word
		        ILI9488_Data(color16 >> 8);      // height part of word

			color24c += 3;
			color24 = (unsigned int*)color24c;
		}
	}

	return m_channelCount;
}

/*
 *
 */
void ILI9488Output::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "ILI9488Output::DumpConfig()\n");
	LogDebug(VB_CHANNELOUT, "    Columns: %d\n", m_cols);
	LogDebug(VB_CHANNELOUT, "    Rows   : %d\n", m_rows);
	LogDebug(VB_CHANNELOUT, "    Pixels : %d\n", m_pixels);

	ChannelOutputBase::DumpConfig();
}

/*
 *
 */
void ILI9488Output::ILI9488_Init(void)
{
	LogDebug(VB_CHANNELOUT, "ILI9488Output::ILI9488_Init()\n");

	int mem_fd = 0;

	/* open /dev/mem */
	if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
		LogErr(VB_CHANNELOUT, "Can't open /dev/mem\n");
		exit(-1);
	}
	LogDebug(VB_CHANNELOUT, "Opened /dev/mem, mem_fd = %d\n", mem_fd);

	/* mmap GPIO */
	m_gpio_map = mmap(
		NULL,             //Any adddress in our space will do
		BLOCK_SIZE,       //Map length
		PROT_READ|PROT_WRITE,// Enable reading & writting to mapped memory
		MAP_SHARED,       //Shared with other processes
		mem_fd,           //File to map
		GPIO_BASE         //Offset to GPIO peripheral
	);

	close(mem_fd); //No need to keep mem_fd open after mmap

	if (m_gpio_map == MAP_FAILED) {
		LogErr(VB_CHANNELOUT, "mmap error %d\n", (int)m_gpio_map);//errno also set!
		exit(-1);
	}

	// Always use volatile pointer!
	m_gpio = (volatile unsigned *)m_gpio_map;

	LogDebug(VB_CHANNELOUT, "Initializing GPIO pins for output\n");
	int i = 0;
	for (i = 0; i < 12; i++)
	{
		INP_GPIO(ILI_allPins[i]);
		OUT_GPIO(ILI_allPins[i]);
	}

	LogDebug(VB_CHANNELOUT, "Initializing data lookup table\n");

	// Initialize our lookup table
	int bit;
	for (i = 0; i < 256; i++)
	{
		ILI_dataValues[i] = 0;
		for (bit = 0; bit < 8; bit++)
		{
			ILI_dataValues[i] |= (1 & (i >> bit)) << ILI_dataPins[bit];
		}
	}

	ILI9488_Command(0xe9);
	ILI9488_Data(0x20);

	ILI9488_Command(0x11); // Exit sleep
	usleep(100000);

	ILI9488_Command(0xd1); // VCOM Control
	ILI9488_Data(0x00);    //     SEL/VCM
	ILI9488_Data(0x0C);    //     VCM
	ILI9488_Data(0x0F);    //     VDV

	ILI9488_Command(0xd0); // Power_Setting
	ILI9488_Data(0x07);    //     VC
	ILI9488_Data(0x04);    //     BT
	ILI9488_Data(0x00);    //     VRH

	ILI9488_Command(0x36); // Set_addressing_mode
	ILI9488_Data(0b00110000);
	//ILI9488_Data(0x48);
	// 0x48 = 0b01001000
	// 0 - top to bottom
	// 1 - right to left
	// 0 - normal mode of columns order
	// 0 - refresh top to bottom
	// 1 - RGB or BGR  (here BGR order)
	// 0 - <skip>
	// 0 - no hflip
	// 0 - no flip

	ILI9488_Command(0xE0);
	ILI9488_Data(0x00);
	ILI9488_Data(0x04);
	ILI9488_Data(0x0E);
	ILI9488_Data(0x08);
	ILI9488_Data(0x17);
	ILI9488_Data(0x0A);
	ILI9488_Data(0x40);
	ILI9488_Data(0x79);
	ILI9488_Data(0x4D);
	ILI9488_Data(0x07);
	ILI9488_Data(0x0E);
	ILI9488_Data(0x0A);
	ILI9488_Data(0x1A);
	ILI9488_Data(0x1D);
	ILI9488_Data(0x0F);

	ILI9488_Command(0xE1);
	ILI9488_Data(0x00);
	ILI9488_Data(0x1B);
	ILI9488_Data(0x1F);
	ILI9488_Data(0x02);
	ILI9488_Data(0x10);
	ILI9488_Data(0x05);
	ILI9488_Data(0x32);
	ILI9488_Data(0x34);
	ILI9488_Data(0x43);
	ILI9488_Data(0x02);
	ILI9488_Data(0x0A);
	ILI9488_Data(0x09);
	ILI9488_Data(0x33);
	ILI9488_Data(0x37);
	ILI9488_Data(0x0F);

	ILI9488_Command(0xB1);
	ILI9488_Data(0xB0);
	ILI9488_Data(0x11);

	ILI9488_Command(0xB4);
	ILI9488_Data(0x02);

	ILI9488_Command(0xF7);
	ILI9488_Data(0xA9);
	ILI9488_Data(0x51);
	ILI9488_Data(0x2C);
	ILI9488_Data(0x82);

	ILI9488_Command(0XB0);
	ILI9488_Data(0x00);

	ILI9488_Command( 0x3A );     // Set pixel format
	ILI9488_Data( 0x55 );
	ILI9488_Command( 0xC1 );     // Display timing setting for normal/partial mode
	ILI9488_Data( 0x41 );
	ILI9488_Command( 0xC0 );     // Set Default Gamma
	ILI9488_Data( 0x18 );
	ILI9488_Data( 0x16 );
	ILI9488_Command( 0xC5 );     // Set frame rate
	ILI9488_Data( 0x00 );
	ILI9488_Data( 0x1E );
	ILI9488_Data( 0x80 );
	ILI9488_Command( 0xD2 );     // Power setting
	ILI9488_Data( 0x01 );
	ILI9488_Data( 0x44 );
	ILI9488_Command( 0xC8 );     // Set Gamma
	ILI9488_Data( 0x04 );
	ILI9488_Data( 0x67 );
	ILI9488_Data( 0x35 );
	ILI9488_Data( 0x04 );
	ILI9488_Data( 0x08 );
	ILI9488_Data( 0x06 );
	ILI9488_Data( 0x24 );
	ILI9488_Data( 0x01 );
	ILI9488_Data( 0x37 );
	ILI9488_Data( 0x40 );
	ILI9488_Data( 0x03 );
	ILI9488_Data( 0x10 );
	ILI9488_Data( 0x08 );
	ILI9488_Data( 0x80 );
	ILI9488_Data( 0x00 );
	ILI9488_Command( 0x2A );
	ILI9488_Data( 0x00 );
	ILI9488_Data( 0x00 );
	ILI9488_Data( 0x01 );
	ILI9488_Data( 0x3F );
	ILI9488_Command( 0x2B );
	ILI9488_Data( 0x00 );
	ILI9488_Data( 0x00 );
	ILI9488_Data( 0x01 );
	ILI9488_Data( 0xDF );
	ILI9488_Command( 0x29 ); //display on
	ILI9488_Command( 0x2C ); //display on

	// Setup the range area we want to write to
	int x1 = 0;
	int x2 = 479;
	int y1 = 0;
	int y2 = 319;

	ILI9488_Command(0x2a); // Set_column_address
	ILI9488_Data(x1>>8);
	ILI9488_Data(x1);
	ILI9488_Data(x2>>8);
	ILI9488_Data(x2);

	ILI9488_Command(0x2b); // Set_page_address
	ILI9488_Data(y1>>8);
	ILI9488_Data(y1);
	ILI9488_Data(y2>>8);
	ILI9488_Data(y2);

	LogDebug(VB_CHANNELOUT, "Initialization complete\n");
}

/*
 *
 */
void ILI9488Output::ILI9488_SendByte(unsigned int cs, unsigned char byte)
{
	if (cs)
		GPIO_SET = 1 << ILI_PIN_DCX;
	else
		GPIO_CLR = 1 << ILI_PIN_DCX;

	GPIO_CLR = (1 << ILI_PIN_WRX) | ILI_dataBits;

	GPIO_SET = ILI_dataValues[byte];

	GPIO_SET = 1 << ILI_PIN_WRX;
}

/*
 *
 */
void ILI9488Output::ILI9488_Command(unsigned char cmd)
{
	LogExcess(VB_CHANNELOUT, "ILI9488_Command(0x%02x)\n", cmd);
	ILI9488_SendByte(0, cmd);
}

/*
 *
 */
void ILI9488Output::ILI9488_Data(unsigned char data)
{
	LogExcess(VB_CHANNELOUT, "ILI9488_Data(0x%02x)\n", data);
	ILI9488_SendByte(1, data);
}

/*
 *
 */
void ILI9488Output::ILI9488_Cleanup(void)
{
	LogDebug(VB_CHANNELOUT, "ILI9488_Cleanup()\n");

	GPIO_CLR = 1 << ILI_PIN_CSX;
}

