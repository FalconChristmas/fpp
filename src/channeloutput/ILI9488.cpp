/*
 *   ILI9488 Channel Output driver for Falcon Player (FPP)
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
#include "fpp-pch.h"

#include <fcntl.h>
#include <sys/mman.h>

#include "ILI9488.h"

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

const int ILI_allPins[12] = { ILI_PIN_CSX, ILI_PIN_WRX, ILI_PIN_DCX, ILI_PIN_D1, ILI_PIN_D2, ILI_PIN_D3, ILI_PIN_D4, ILI_PIN_D5, ILI_PIN_D6, ILI_PIN_D7, ILI_PIN_D8, ILI_PIN_RST };
const int ILI_dataPins[8] = { ILI_PIN_D1, ILI_PIN_D2, ILI_PIN_D3, ILI_PIN_D4, ILI_PIN_D5, ILI_PIN_D6, ILI_PIN_D7, ILI_PIN_D8 };


extern "C" {
    ILI9488Output *createOutputILI9488(unsigned int startChannel,
                                       unsigned int channelCount) {
        return new ILI9488Output(startChannel, channelCount);
    }
}
/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
ILI9488Output::ILI9488Output(unsigned int startChannel, unsigned int channelCount)
  : ThreadedChannelOutputBase(startChannel, channelCount),
	m_initialized(0),
	m_rows(480),
	m_cols(320),
	m_pixels(0),
	m_bpp(24),
	m_gpio_map(NULL),
	m_gpio(NULL),
	m_clearWRXDataBits(0),
	m_bitDCX(0),
	m_bitWRX(0)
{
	LogDebug(VB_CHANNELOUT, "ILI9488Output::ILI9488Output(%u, %u)\n",
		startChannel, channelCount);

	m_pixels = m_rows * m_cols;
	m_useDoubleBuffer = 1;
}

/*
 *
 */
ILI9488Output::~ILI9488Output()
{
	LogDebug(VB_CHANNELOUT, "ILI9488Output::~ILI9488Output()\n");
}

/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
int ILI9488Output::Init(Json::Value config)
{
	LogDebug(VB_CHANNELOUT, "ILI9488Output::Init(JSON)\n");

	if (config.isMember("bpp"))
		m_bpp = config["bpp"].asInt();

	ILI9488_Init();

	return ThreadedChannelOutputBase::Init(config);
}

/*
 *
 */
int ILI9488Output::Close(void)
{
	LogDebug(VB_CHANNELOUT, "ILI9488Output::Close()\n");

	ILI9488_Cleanup();

	return ThreadedChannelOutputBase::Close();
}

/*
 *
 */
int ILI9488Output::RawSendData(unsigned char *channelData)
{
	LogExcess(VB_CHANNELOUT, "ILI9488Output::RawSendData(%p)\n", channelData);

	if (!m_initialized)
		return 0;

	unsigned char *c = channelData;

	ILI9488_Command(0x2c); // Write memory start

	GPIO_SET = m_bitDCX;   // Set the 'data mode' bit

	unsigned long RGBx;
	unsigned int rgb565;

	for (int p = 0; p < m_pixels; p++)
	{
		if (m_bpp == 16)
		{
			RGBx = *(unsigned long *)c;
			rgb565 = (((RGBx & 0x000000FF) <<  8) & 0b1111100000000000) |
					 (((RGBx & 0x0000FF00) >>  5) & 0b0000011111100000) |
					 (((RGBx & 0x00FF0000) >> 19)); // & 0b0000000000011111);

			ILI9488_SendByte(rgb565 >> 8);
			ILI9488_SendByte(rgb565);

			c += 3;
		}
		else
		{
			ILI9488_SendByte(*c++);
			ILI9488_SendByte(*c++);
			ILI9488_SendByte(*c++);
		}
	}

	ILI9488_Command( 0x00 );     // Null command, ends write

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

	ThreadedChannelOutputBase::DumpConfig();
}

/*
 *
 */
void ILI9488Output::ILI9488_Init(void)
{
	LogDebug(VB_CHANNELOUT, "ILI9488Output::ILI9488_Init()\n");

	int mem_fd = 0;

	if ((mem_fd = open("/dev/gpiomem", O_RDWR|O_SYNC) ) < 0) {
		LogErr(VB_CHANNELOUT, "Can't open /dev/mem\n");
		exit(-1);
	}
	LogDebug(VB_CHANNELOUT, "Opened /dev/gpiomem, mem_fd = %d\n", mem_fd);

	off_t gpio_base = 0x200000;

	m_gpio_map = mmap(
		NULL,             //Any adddress in our space will do
		BLOCK_SIZE,       //Map length
		PROT_READ|PROT_WRITE,// Enable reading & writting to mapped memory
		MAP_SHARED,       //Shared with other processes
		mem_fd,           //File to map
		gpio_base         //Offset to GPIO peripheral
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
		m_dataValues[i] = 0;
		for (bit = 0; bit < 8; bit++)
		{
			m_dataValues[i] |= (1 & (i >> bit)) << ILI_dataPins[bit];
		}
	}

	m_clearWRXDataBits = (1 << ILI_PIN_WRX) | ILI_dataBits;
	m_bitWRX = 1 << ILI_PIN_WRX;
	m_bitDCX = 1 << ILI_PIN_DCX;

	GPIO_CLR = 1 << ILI_PIN_CSX;

	ILI9488_Command(0x11); // Exit sleep
	usleep(100000);

//	ILI9488_Command(0x01); // Software Reset
//	usleep(150000);

//	ILI9488_Command(0x13); // Normal Display Mode

	ILI9488_Command(0x36); // Memory Access Control
	ILI9488_Data(0b10001000); // RGB with top closest to Pi GPIO connector
	//ILI9488_Data(0x48);
	// 0x48 = 0b01001000
	// 0 - Row Address Order (vertical flip of frame data)
	// 1 - Column Address Order (horizontal flip of frame data)
	// 0 - Row/Column exchange (1 = 480x320, 0 = 320x480
	// 0 - Vertical Refresh Order
	// 1 - RGB or BGR  (0 = RGB, 1 = BGR)
	// 0 - Horizontal Refresh Order
	// 0 - no hflip
	// 0 - no flip

	ILI9488_Command(0xE0); // Positive Gamma Control
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

	ILI9488_Command(0xE1); // Negative Gamma Control
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

	ILI9488_Command(0xB1); // Frame Rate
	ILI9488_Data(0x00);    // as low as it can go, ~28hz

	ILI9488_Command(0xB0); // Interface mode control
	ILI9488_Data(0x00);

	ILI9488_Command(0x3A); // Set pixel format

	if (m_bpp == 16)
		ILI9488_Data(0x55); // 16bpp RGB565
	else
		ILI9488_Data(0x66); // 18bpp (24bpp w/ lower 2bpp bits ignored)

	SetRegion(0, 0, m_cols - 1, m_rows - 1);

	ILI9488_Command( 0x29 ); //display on

	// Not sure if we need to do this twice or not, but sample python did
	SetRegion(0, 0, m_cols - 1, m_rows - 1);

	m_initialized = 1;

	LogDebug(VB_CHANNELOUT, "Initialization complete\n");
}

/*
 *
 */
inline
void ILI9488Output::ILI9488_SendByte(unsigned char byte)
{
	GPIO_CLR = m_clearWRXDataBits;

	GPIO_SET = m_dataValues[byte];

	GPIO_SET = m_bitWRX;
}

/*
 *
 */
void ILI9488Output::ILI9488_Command(unsigned char cmd)
{
	LogExcess(VB_CHANNELOUT, "ILI9488_Command(0x%02x)\n", cmd);

	GPIO_CLR = m_bitDCX;
	ILI9488_SendByte(cmd);
}

/*
 *
 */
void ILI9488Output::ILI9488_Data(unsigned char data)
{
	LogExcess(VB_CHANNELOUT, "ILI9488_Data(0x%02x)\n", data);

	GPIO_SET = m_bitDCX;
	ILI9488_SendByte(data);
}

/*
 *
 */
void ILI9488Output::ILI9488_Cleanup(void)
{
	LogDebug(VB_CHANNELOUT, "ILI9488_Cleanup()\n");

	GPIO_CLR = 1 << ILI_PIN_CSX;
}

/*
 *
 */
inline
void ILI9488Output::SetColumnRange(unsigned int x1, unsigned int x2)
{
//	ILI9488_Command( 0x2B ); // Use 'row' setting here because we rotate display
	ILI9488_Command( 0x2A );
	ILI9488_Data( x1 >> 8 );
	ILI9488_Data( x1 );
	ILI9488_Data( x2 >> 8 );
	ILI9488_Data( x2 );
}

/*
 *
 */
inline
void ILI9488Output::SetRowRange(unsigned int y1, unsigned int y2)
{
//	ILI9488_Command( 0x2A ); // Use 'column' setting here because we rotate display
	ILI9488_Command( 0x2B );
	ILI9488_Data( y1 >> 8 );
	ILI9488_Data( y1 );
	ILI9488_Data( y2 >> 8 );
	ILI9488_Data( y2 );
}

/*
 *
 */
inline
void ILI9488Output::SetRegion(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2)
{
	SetColumnRange(x1, x2);
	SetRowRange(y1, y2);
}


