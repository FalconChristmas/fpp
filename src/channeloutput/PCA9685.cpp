

#include <stdlib.h>
#include <chrono>
#include <thread>

#include "common.h"
#include "PCA9685.h"
#include "log.h"



extern "C" {
    PCA9685Output *createPCA9685Output(unsigned int startChannel,
                                  unsigned int channelCount) {
        return new PCA9685Output(startChannel, channelCount);
    }
}

/*
 *
 */
PCA9685Output::PCA9685Output(unsigned int startChannel, unsigned int channelCount)
  : ChannelOutputBase(startChannel, channelCount),
	i2c(nullptr)
{
	LogDebug(VB_CHANNELOUT, "PCA9685Output::PCA9685Output(%u, %u)\n",
		startChannel, channelCount);
}

/*
 *
 */
PCA9685Output::~PCA9685Output()
{
	LogDebug(VB_CHANNELOUT, "PCA9685Output::~PCA9685Output()\n");
    if (i2c) {
        delete i2c;
    }
}

/*
 *
 */
int PCA9685Output::Init(Json::Value config)
{
	LogDebug(VB_CHANNELOUT, "PCA9685Output::Init(JSON)\n");

    if (config["deviceID"].isString()) {
        m_deviceID = std::atoi(config["deviceID"].asString().c_str());
    } else {
        m_deviceID = config["deviceID"].asInt();
    }

    m_i2cDevice = config["device"].asString();
    if (m_i2cDevice == "") {
        m_i2cDevice = "isc-1";
    }
    i2c = new I2CUtils(m_i2cDevice.c_str(), m_deviceID);
	if (!i2c->isOk()) {
		LogErr(VB_CHANNELOUT, "Error opening I2C device for MCP23017 output\n");
		return 0;
	}
    
    m_frequency = config["deviceID"].asInt();
    
    for (int x = 0; x < 16; x++) {
        m_min[x] = config["ports"][x]["min"].asInt();
        m_max[x] = config["ports"][x]["max"].asInt();
        m_dataType[x] = config["ports"][x]["dataType"].asInt();

        m_lastChannelData[x] = 0xFFFF;
    }

	// Initialize


    int oldmode = i2c->readByteData(0x00);
    
    int m0 = (oldmode & 0x7f) | 0x10;
    i2c->writeByteData(0x00, m0);
    
    int fs = 25000000;
    fs /= 4096;
    fs /= m_frequency;
    fs -= 1;
    
    i2c->writeByteData(0xFE, fs);
    i2c->writeByteData(0x00, oldmode);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    m0 = oldmode | 0xa1; // Mode 1, autoincrement on
    m0 &= 0xEF;
    i2c->writeByteData(0x00, m0);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    
    oldmode = i2c->readByteData(0x00);

    oldmode = i2c->readByteData(0xfe);
    
	return ChannelOutputBase::Init(config);
}

/*
 *
 */
int PCA9685Output::Close(void)
{
	LogDebug(VB_CHANNELOUT, "PCA9685Output::Close()\n");

	return ChannelOutputBase::Close();
}

/*
 *
 */
int PCA9685Output::SendData(unsigned char *channelData)
{
	LogExcess(VB_CHANNELOUT, "PCA9685Output::SendData(%p)\n", channelData);

    
	unsigned char *c = channelData;
    c += m_startChannel;
    
	int ch = 0;
	for (int x = 0; ch < m_channelCount && x < 16; x++, ch++) {
        int scl = m_max[x] - m_min[x] + 1;
        unsigned short val;
        int tmp;
        switch (m_dataType[x]) {
            case 0:
                tmp = scl * c[ch] / 256;
                val = tmp + m_min[x];
                break;
            case 1:
                tmp = c[ch + 1];
                tmp = tmp << 8;
                tmp = c[ch];
                ch++;
                tmp *= scl;
                tmp /= 0xffff;
                val = tmp + m_min[x];
                break;
            case 2:
                val = c[ch];
                break;
            case 3:
                val = c[ch + 1];
                val = val << 8;
                val = c[ch];
                ch++;
                break;
        }
        if (val > m_max[x]) {
            val = m_max[x];
        }
        if (val  < m_min[x]) {
            val = m_min[x];
        }
        if (m_lastChannelData[x] != val) {
            m_lastChannelData[x] = val;
            uint8_t a = val & 0xFF;
            uint8_t b = ((val & 0xFF00) >> 8);
            uint8_t bytes[4] = {0, 0, a, b};
            i2c->writeI2CBlockData(0x06 + (x * 4), bytes, 4);
        }
	}
	return m_channelCount;
}

/*
 *
 */
void PCA9685Output::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "PCA9685Outputs::DumpConfig()\n");

	LogDebug(VB_CHANNELOUT, "    deviceID: %X\n", m_deviceID);

	ChannelOutputBase::DumpConfig();
}

