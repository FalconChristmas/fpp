/*
 *   WS281x SPI handler for Falcon Player (FPP)
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
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

#include "common.h"
#include "log.h"
#include "settings.h"

#include "SPIws281x.h"



/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
SPIws281xOutput::SPIws281xOutput(unsigned int startChannel, unsigned int channelCount)
  : ChannelOutputBase(startChannel, channelCount),
	m_port(-1)
{
	LogDebug(VB_CHANNELOUT, "SPIws281xOutput::SPIws281xOutput(%u, %u)\n",
		startChannel, channelCount);

	m_maxChannels = SPIws281x_MAX_CHANNELS;
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
	LogDebug(VB_CHANNELOUT, "SPIws281xOutput::Init('%s')\n", configStr);

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
        //create stream, open device
        m_fd = open("/dev/spidev0.0", O_RDWR);
      }
			else if (elem[1] == "spidev0.1"){
				m_port = 1;
        //create stream, open device
        m_fd = open("/dev/spidev0.1", O_RDWR);
      }
      else if (elem[1] == "spidev1.0"){
				m_port = 2;
        //create stream, open device
        m_fd = open("/dev/spidev1.0", O_RDWR);
      }
		}
		
	}

	if (m_port == -1)
	{
		LogErr(VB_CHANNELOUT, "Invalid Config String: %s\n", configStr);
		return 0;
	}

	LogDebug(VB_CHANNELOUT, "Using SPI Port %d\n", m_port);

  
  
	if (m_fd < 0)
	{
		LogErr(VB_CHANNELOUT, "Unable to open SPI device\n") ;
		return 0;
	}

	return ChannelOutputBase::Init(configStr);
}


/*
 *
 */
int SPIws281xOutput::Close(void)
{
	LogDebug(VB_CHANNELOUT, "SPIws281xOutput::Close()\n");
  
  close(m_fd);

	return ChannelOutputBase::Close();
}

/*
 *
 */
int SPIws281xOutput::RawSendData(unsigned char *channelData)
{
	LogDebug(VB_CHANNELOUT, "SPIws281xOutput::RawSendData(%p)\n", channelData);
  
  // create SPI buffer
  // each channel byte needs 4 SPI bytes
  // +3 for the reset/latch pulse
  // +1 leading byte to clear start glitch
  uint16_t s_size = m_channelCount*4+4;
  
  uint8_t* bufq = (uint8_t *)(malloc(s_size));
  
  if(!bufq){
    LogErr(VB_CHANNELOUT, "SPI Buffer Creation Fail"");
		return 0;
  }
  
   uint16_t spi_ptr = 1; //leading byte will be x00 to avoid start glitch
   bufq[0] = 0;
  
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
  
  //load SPI buffer
  int ret;
 
  struct spi_ioc_transfer tr = {
      .tx_buf = (unsigned long)bufq,
      .rx_buf = 0,
      .len = s_size,
      .speed_hz = SPIWS281x_SPEED,
      .bits_per_word = 8,
  };

  ret = ioctl(m_fd, SPI_IOC_MESSAGE(1), &tr);
  
  free(bufq);
  
  if (ret < 1){
    LogErr(VB_CHANNELOUT, "SPI Data Send Fail"");
    return 0;
  }
  
  
	//wiringPiSPIDataRW(m_port, (unsigned char *)channelData, m_channelCount);
	

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

