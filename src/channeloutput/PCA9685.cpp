

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
    
    m_frequency = config["frequency"].asInt();
    bool asUsec = false;
    if (config.isMember("asUsec")) {
        asUsec = config["asUsec"].asInt();
    }

    for (int x = 0; x < 16; x++) {
        m_ports[x].m_min = config["ports"][x]["min"].asInt();
        m_ports[x].m_max = config["ports"][x]["max"].asInt();
        if (config["ports"][x].isMember("center")) {
            m_ports[x].m_center = config["ports"][x]["center"].asInt();
        } else {
            m_ports[x].m_center = (m_ports[x].m_min + m_ports[x].m_max) / 2;
        }
        
        if (asUsec) {
            int z = m_ports[x].m_min;
            z *= m_frequency;
            z *= 4095;
            z /= 1000000;
            m_ports[x].m_min = z;
            
            z = m_ports[x].m_max;
            z *= m_frequency;
            z *= 4095;
            z /= 1000000;
            m_ports[x].m_max = z;
            
            z = m_ports[x].m_center;
            z *= m_frequency;
            z *= 4095;
            z /= 1000000;
            m_ports[x].m_center = z;
        }
        
        m_ports[x].m_dataType = config["ports"][x]["dataType"].asInt();
        if (config["ports"][x].isMember("zeroBehavior")) {
            m_ports[x].m_zeroBehavior = config["ports"][x]["zeroBehavior"].asInt();
        } else {
            m_ports[x].m_zeroBehavior = 0;
        }
        m_ports[x].m_lastValue = 0xFFFF;
    }

	// Initialize


    int oldmode = i2c->readByteData(0x00);
    
    int m0 = (oldmode & 0x7f) | 0x10;
    i2c->writeByteData(0x00, m0);
    
    float fs = 25000000.0f;
    fs /= 4096.f;
    fs /= m_frequency;
    fs += 0.5;
    
    int fsi = (int)fs;
    if (fsi < 3) {
        fsi = 3;
    }

    i2c->writeByteData(0xFE, fsi);
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


enum DataTypeEnum {
    SCALED_8BIT,
    SCALED_8BIT_REVERSE,
    SCALED_16BIT,
    SCALED_16BIT_REVERSE,
    ABSOLUTE_8BIT,
    ABSOLUTE_16BIT

};
enum ZeroTypeEnum {
    ZERO_HOLD,
    ZERO_NORMAL,
    ZERO_CENTER,
    ZERO_OFF
};

static inline unsigned short readVal(unsigned char *channelData, int dt, int tp, int &pos) {
    unsigned short val = channelData[pos];
    pos++;
    switch (dt) {
        case SCALED_16BIT:
        case SCALED_16BIT_REVERSE:
        case ABSOLUTE_16BIT:
            {
                unsigned short t = channelData[pos];
                pos++;
                t = t << 8;
                val += t;
            }
            break;
        case ABSOLUTE_8BIT:
            break;
        default:
            val *= 256;
            break;
    }
    if (tp != ZERO_NORMAL && val == 0) {
        return val;
    }
    switch (dt) {
        case SCALED_16BIT_REVERSE:
        case SCALED_8BIT_REVERSE:
            val = 65535 - val;
            break;
    }
    return val;
}

/*
 *
 */
int PCA9685Output::SendData(unsigned char *channelData)
{
	LogExcess(VB_CHANNELOUT, "PCA9685Output::SendData(%p)\n", channelData);

	unsigned char *c = channelData;
    c += (m_startChannel - 1);
    
	int ch = 0;
	for (int x = 0; ch < m_channelCount && x < 16; x++) {
        unsigned short val = readVal(channelData, m_ports[x].m_dataType, m_ports[x].m_zeroBehavior, ch);
        if (val != 0 || m_ports[x].m_zeroBehavior != ZERO_HOLD) {
            if (val == 0 && m_ports[x].m_zeroBehavior == ZERO_CENTER) {
                val = m_ports[x].m_center;
            } else if (val == 0 && m_ports[x].m_zeroBehavior == ZERO_OFF) {
                val = 0;
            } else {
                if (val >= 32767) {
                    int scale = m_ports[x].m_max - m_ports[x].m_center;
                    val -= 32767;
                    scale *= val;
                    scale /= 32767;
                    val = scale;
                    val += m_ports[x].m_center;
                } else {
                    int scale = m_ports[x].m_center - m_ports[x].m_min;
                    scale *= val;
                    scale /= 32767;
                    val = scale;
                    val += m_ports[x].m_min;
                }
                if (val > m_ports[x].m_max) {
                    val = m_ports[x].m_max;
                }
                if (val < m_ports[x].m_min) {
                    val = m_ports[x].m_min;
                }
            }
            if (m_ports[x].m_lastValue != val) {
                m_ports[x].m_lastValue = val;
                uint8_t a = val & 0xFF;
                uint8_t b = ((val & 0xFF00) >> 8);
                uint8_t bytes[4] = {0, 0, a, b};
                i2c->writeI2CBlockData(0x06 + (x * 4), bytes, 4);
            }
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
    LogDebug(VB_CHANNELOUT, "    Frequency: %d\n", m_frequency);
    for (int x = 0; x < 16; x++) {
        LogDebug(VB_CHANNELOUT, "    Port %d:    Min:    %d\n", x, m_ports[x].m_min);
        LogDebug(VB_CHANNELOUT, "                Center: %d\n", m_ports[x].m_center);
        LogDebug(VB_CHANNELOUT, "                Max:    %d\n", m_ports[x].m_max);
        LogDebug(VB_CHANNELOUT, "                DataType: %d\n", m_ports[x].m_dataType);
        LogDebug(VB_CHANNELOUT, "                ZeroType: %d\n", m_ports[x].m_zeroBehavior);
    }

	ChannelOutputBase::DumpConfig();
}

