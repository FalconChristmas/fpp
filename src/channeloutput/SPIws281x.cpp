/*
 *   WS281x SPI handler for Falcon Player (FPP)
 *      Developed by Bill Porter (madsci1016)  www.billporter.info
 *        Using example by penfold42
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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include "common.h"
#include "log.h"
#include "settings.h"

/******   SPI Access mode. Pick one  ********/
//#define SPIWS281x_USE_WIRINGPI
#define SPIWS281x_USE_IOCTL
/********************************************/

#if defined(SPIWS281x_USE_WIRINGPI)
#include <wiringPiSPI.h>
#elif defined(SPIWS281x_USE_IOCTL)
#include <sys/ioctl.h>
#endif
#include "SPIws281x.h"

#define SPIWS281x_SPEED 3333333  //May need to be 3,800,000 for some models or 2857142 But Bill's optimization says Lucky 3's
#define SPIWS281x_MAX_CHANNELS  3000

// each pair of WS2812 bits is sent as 1 spi byte
// looks up 2 bits of led data to get the spi pattern to send
// A ws281x '0' is sent as 1000
// A ws281x '1' is sent as 1100
// A ws281x 'reset/latch' is sent as 3 bytes of 00000000
const uint8_t bitpair_to_byte[] = {
    0b10001000,
    0b10001100,
    0b11001000,
    0b11001100,
};

/*
 *
 */
SPIws281xOutput::SPIws281xOutput(unsigned int startChannel, unsigned int channelCount)
  : ChannelOutputBase(startChannel, channelCount),
	m_port(-1)
{
	LogDebug(VB_CHANNELOUT, "SPIws281xOutput::SPIws281xOutput(%u, %u)\n",
		startChannel, channelCount);

	m_maxChannels = SPIWS281x_MAX_CHANNELS;
}

/*
 *
 */
SPIws281xOutput::~SPIws281xOutput()
{
	LogDebug(VB_CHANNELOUT, "SPIws281xOutput::~SPIws281xOutput()\n");
}

/*
 *
 */
int SPIws281xOutput::Init(char *configStr)
{
  
#if defined(SPIWS281x_USE_WIRINGPI)
  LogDebug(VB_CHANNELOUT, "SPIws281xOutput::Init('%s') WiringPi\n", configStr);
#elif defined(SPIWS281x_USE_IOCTL)
	LogDebug(VB_CHANNELOUT, "SPIws281xOutput::Init('%s') IOCTL\n", configStr);
#endif

	std::vector<std::string> configElems = split(configStr, ';');

	for (int i = 0; i < configElems.size(); i++)
	{
		std::vector<std::string> elem = split(configElems[i], '=');
		if (elem.size() < 2)
			continue;

		if (elem[0] == "device")
		{
			LogDebug(VB_CHANNELOUT, "Using %s for SPI output\n",
				elem[1].c_str());

			if (elem[1] == "spidev0.0"){
				m_port = 0;
#if defined(SPIWS281x_USE_IOCTL)        
        //create stream, open device
        m_fd = open("/dev/spidev0.0", O_RDWR);
#endif
      }
			else if (elem[1] == "spidev0.1"){
				m_port = 1;
#if defined(SPIWS281x_USE_IOCTL)        
        //create stream, open device
        m_fd = open("/dev/spidev0.1", O_RDWR);
#endif
      }
      else if (elem[1] == "spidev1.0"){
				m_port = 1;
#if defined(SPIWS281x_USE_IOCTL)        
        //create stream, open device
        m_fd = open("/dev/spidev1.0", O_RDWR);
#endif
      }
		}
	}

	if (m_port == -1)
	{
		LogDebug(VB_CHANNELOUT, "Invalid Config String: %s\n", configStr);
		return 0;
	}

	LogDebug(VB_CHANNELOUT, "Using SPI Port %d\n", m_port);

#if defined(SPIWS281x_USE_WIRINGPI)
	if (wiringPiSPISetup(m_port, SPIWS281x_SPEED) < 0)
#elif defined(SPIWS281x_USE_IOCTL)        
  if (m_fd < 0)
#endif
	{
		LogErr(VB_CHANNELOUT, "Unable to open SPI device\n") ;
		return 0;
	}
  
  // create SPI buffer
  // each channel byte needs 4 SPI bytes
  // +3 for the reset/latch pulse
  // +1 leading byte to clear start glitch
  m_size = m_channelCount*4+4;
  bufq = (uint8_t *)(malloc(m_size));
  
  if(!bufq){
    LogErr(VB_CHANNELOUT, "SPI Buffer Creation Fail");
		return 0;
  }
  
   bufq[0] = 0; //clear pad byte

	return ChannelOutputBase::Init(configStr);
}


/*
 *
 */
int SPIws281xOutput::Close(void)
{
	LogDebug(VB_CHANNELOUT, "SPIws281xOutput::Close()\n");
  
  free(bufq);

	return ChannelOutputBase::Close();
}

/*
 *
 */
int SPIws281xOutput::RawSendData(unsigned char *channelData)
{
	LogDebug(VB_CHANNELOUT, "SPIws281xOutput::RawSendData(%p)\n", channelData);

   uint16_t spi_ptr = 1; //leading byte will be x00 to avoid start glitch
  
  
  for(int i = 0; i< m_channelCount; i++ ){
    
    bufq[spi_ptr++] = bitpair_to_byte[(channelData[i]&0b11000000)>>6];
    bufq[spi_ptr++] = bitpair_to_byte[(channelData[i]&0b00110000)>>4];
    bufq[spi_ptr++] = bitpair_to_byte[(channelData[i]&0b00001100)>>2];
    bufq[spi_ptr++] = bitpair_to_byte[(channelData[i]&0b00000011)];
  }
  
  //last three bytes latch the data
  bufq[spi_ptr++] = 0;
  bufq[spi_ptr++] = 0;
  bufq[spi_ptr++] = 0;
  /*

  
  */
  
#if defined(SPIWS281x_USE_WIRINGPI)
  wiringPiSPIDataRW(m_port, (unsigned char *)bufq, m_size);
#elif defined(SPIWS281x_USE_IOCTL)
    //load SPI buffer
  int ret;
 
  struct spi_ioc_transfer tr = 
  {
      tx_buf: (unsigned long)bufq,
      rx_buf: 0,
      len:  m_size,
      speed_hz: SPIWS281x_SPEED,
      delay_usecs: 0,
      bits_per_word: 8,
      cs_change: 0,
//      tx_nbits: 0,
//      rx_nbits: 0,
//      pad: 0
  };

  ret = ioctl(m_fd, SPI_IOC_MESSAGE(1), &tr);
#endif	

	return m_channelCount;
}

/*
 *
 */
void SPIws281xOutput::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "SPIws281xOutput::DumpConfig()\n");

	LogDebug(VB_CHANNELOUT, "    port    : %d\n", m_port);

	ChannelOutputBase::DumpConfig();
}

