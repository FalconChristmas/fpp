/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the GPL v2 as described in the
 * included LICENSE.GPL file.
 */

#include "fpp-pch.h"

#include <errno.h>
#include <fcntl.h>

#include "../log.h"


#include "GenericSPI.h"

#define MAX_CHANNELS 16777215

#ifdef PLATFORM_PI
#define MIN_SPI_SPEED_HZ 32000
#define MAX_SPI_SPEED_HZ 125000000
#elif PLATFORM_BBB
//TODO need to confirm these
#define MIN_SPI_SPEED_HZ 1500
#define MAX_SPI_SPEED_HZ 48000000
#elif PLATFORM_ARMBIAN
#define MIN_SPI_SPEED_HZ 50000
#define MAX_SPI_SPEED_HZ 2500000
#else
//not sure what other platforms could be used, using safe values here
#define MIN_SPI_SPEED_HZ 50000
#define MAX_SPI_SPEED_HZ 100000
#endif

#include "Plugin.h"
class GenericSPIPlugin : public FPPPlugins::Plugin, public FPPPlugins::ChannelOutputPlugin {
public:
    GenericSPIPlugin() :
        FPPPlugins::Plugin("GenericSPI") {
    }
    virtual ChannelOutput* createChannelOutput(unsigned int startChannel, unsigned int channelCount) override {
        return new GenericSPIOutput(startChannel, channelCount);
    }
};

extern "C" {
FPPPlugins::Plugin* createPlugin() {
    return new GenericSPIPlugin();
}
}

/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
GenericSPIOutput::GenericSPIOutput(unsigned int startChannel, unsigned int channelCount) :
    ThreadedChannelOutput(startChannel, channelCount),
    m_port(-1),
    m_speed_hz(0),
    m_spi(nullptr) {
    LogDebug(VB_CHANNELOUT, "GenericSPIOutput::GenericSPIOutput(%u, %u)\n",
             startChannel, channelCount);
}

/*
 *
 */
GenericSPIOutput::~GenericSPIOutput() {
    LogDebug(VB_CHANNELOUT, "GenericSPIOutput::~GenericSPIOutput()\n");
    if (m_spi) {
        delete m_spi;
    }
}

/*
 *
 */

int GenericSPIOutput::Init(Json::Value config) {
    LogDebug(VB_CHANNELOUT, "GenericSPIOutput::Init()\n");

    if (config.isMember("device")) {
        std::string device = config["device"].asString();
        if (device == "spidev0.0")
            m_port = 0;
        else if (device == "spidev0.1")
            m_port = 1;
    }
    if (config.isMember("speed")) {
        //input page ask for speed in khz
        std::string speedStr = config["speed"].asString();
        int config_speed = 1000 * std::atoi(speedStr.c_str());
        if (config_speed < MIN_SPI_SPEED_HZ)
            m_speed_hz = MIN_SPI_SPEED_HZ;
        else if (config_speed > MAX_SPI_SPEED_HZ)
            m_speed_hz = MAX_SPI_SPEED_HZ;
        else
            m_speed_hz = config_speed;
    }

    if (m_port == -1) {
        LogErr(VB_CHANNELOUT, "Invalid Config for SPI.  Invalid port.\n");
        return 0;
    }

    LogDebug(VB_CHANNELOUT, "Using SPI Port %d\n", m_port);

    m_spi = new SPIUtils(m_port, m_speed_hz);
    if (!m_spi->isOk()) {
        LogErr(VB_CHANNELOUT, "Unable to open SPI device\n");
        delete m_spi;
        m_spi = nullptr;
        return 0;
    }

    return ThreadedChannelOutput::Init(config);
}

/*
 *
 */
int GenericSPIOutput::Close(void) {
    LogDebug(VB_CHANNELOUT, "GenericSPIOutput::Close()\n");

    return ThreadedChannelOutput::Close();
}
void GenericSPIOutput::GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) {
    addRange(m_startChannel, m_startChannel + m_channelCount - 1);
}

/*
 *
 */
int GenericSPIOutput::RawSendData(unsigned char* channelData) {
    LogDebug(VB_CHANNELOUT, "GenericSPIOutput::RawSendData(%p)\n", channelData);

    if (!m_spi) {
        return 0;
    }

    m_spi->xfer(channelData, nullptr, m_channelCount);

    return m_channelCount;
}

/*
 *
 */
void GenericSPIOutput::DumpConfig(void) {
    LogDebug(VB_CHANNELOUT, "GenericSPIOutput::DumpConfig()\n");

    LogDebug(VB_CHANNELOUT, "    port    : %d\n", m_port);

    ThreadedChannelOutput::DumpConfig();
}
