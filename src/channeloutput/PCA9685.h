/*
 *   Copyright (C) 2013-2018 the Falcon Player Developers
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

#ifndef _PCA9685_H
#define _PCA9685_H

#include "ChannelOutputBase.h"
#include "util/I2CUtils.h"

class PCA9685Output : public ChannelOutputBase {
  public:
	PCA9685Output(unsigned int startChannel, unsigned int channelCount);
	virtual ~PCA9685Output();

	virtual int Init(Json::Value config) override;
	virtual int Close(void) override;

	virtual int SendData(unsigned char *channelData) override;

	virtual void DumpConfig(void) override;
    

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)> &addRange) override {
        addRange(m_startChannel, m_startChannel + m_channelCount - 1);
    }


  private:
	I2CUtils *i2c;
	int m_deviceID;
    std::string m_i2cDevice;
    int m_frequency;
    
    
    int m_min[16];
    int m_max[16];
    int m_dataType[16];
    unsigned short m_lastChannelData[16];
};

#endif
