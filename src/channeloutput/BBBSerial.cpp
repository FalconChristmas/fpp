/*
 *   BeagleBone Black PRU Serial DMX/Pixelnet handler for Falcon Pi Player (FPP)
 *
 *   Copyright (C) 2013 the Falcon Pi Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Pi Player (FPP) is free software; you can redistribute it
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

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/wait.h>


#define BBB_PRU  0
#define PRU_ARM_INTERRUPT PRU0_ARM_INTERRUPT
//  #define USING_PRU_RAM

#include <pruss_intc_mapping.h>
extern "C" {
    extern int prussdrv_pru_clear_event(unsigned int eventnum);
    extern int prussdrv_pru_wait_event(unsigned int pru_evtout_num);
    extern int prussdrv_pru_disable(unsigned int prunum);
    extern int prussdrv_exit();
}

// LEDscape includes
#include "pru.h"

// FPP includes
#include "common.h"
#include "log.h"
#include "BBBSerial.h"
#include "BBBUtils.h"
#include "settings.h"


/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
BBBSerialOutput::BBBSerialOutput(unsigned int startChannel,
	unsigned int channelCount)
    : ChannelOutputBase(startChannel, channelCount),
	m_config(NULL),
	m_leds(NULL),
    m_pixelnet(0),
    m_lastData(NULL),
    m_curData(NULL),
    m_curFrame(0)
{
    LogDebug(VB_CHANNELOUT, "BBBSerialOutput::BBBSerialOutput(%u, %u)\n",
		startChannel, channelCount);
    m_useOutputThread = 1;
}

/*
 *
 */
BBBSerialOutput::~BBBSerialOutput()
{
    LogDebug(VB_CHANNELOUT, "BBBSerialOutput::~BBBSerialOutput()\n");
    prussdrv_exit();
    
    if (m_lastData) free(m_lastData);
    if (m_curData) free(m_curData);
}

static int pinGPIOs[] = {
    21,
    19,
    17,
    15,
    16,
    14,
    20,
    18
};
static const char * bbPinNames[] = {
    "P9_25",
    "P9_27",
    "P9_28",
    "P9_29",
    "P9_30",
    "P9_31",
    "P9_91",
    "P9_92",
};

static const char * pbPinNames[] = {
    "P1_29",
    "P2_34",
    "P2_30",
    "P1_33",
    "P2_32",
    "P1_36",
    "P2_28",
    "P1_31",
};


static void configurePRUPins(int start, int end, const char *mode) {
    const char ** pinNames = bbPinNames;
    if (getBeagleBoneType() == PocketBeagle) {
        pinNames = pbPinNames;
    }
    for (int x = start; x < end; x++) {
        configBBBPin(pinNames[x], 3, pinGPIOs[x], mode);
    }
}

static void compileSerialPRUCode(std::vector<std::string> &sargs) {
    pid_t compilePid = fork();
    if (compilePid == 0) {
        char * args[sargs.size() + 3];
        args[0] = "/bin/bash";
        args[1] = "/opt/fpp/src/pru/compileSerial.sh";
        
        for (int x = 0; x < sargs.size(); x++) {
            args[x + 2] = (char*)sargs[x].c_str();
        }
        args[sargs.size() + 2] = NULL;
        
        execvp("/bin/bash", args);
    } else {
        wait(NULL);
    }
}

/*
 *
 */
int BBBSerialOutput::Init(Json::Value config)
{
    LogDebug(VB_CHANNELOUT, "BBBSerialOutput::Init(JSON)\n");

    std::vector<std::string> args;

    // Always send 8 outputs worth of data to PRU for now
    m_outputs = 8;

    m_config = &ledscape_matrix_default;

    if (config["subType"].asString() == "Pixelnet") {
        args.push_back("-DPIXELNET");
        m_pixelnet = 1;
    } else {
        m_pixelnet = 0;
        args.push_back("-DDMX");
    }
#ifdef USING_PRU_RAM
    args.push_back("-DUSING_PRU_RAM");
#endif
    if (m_pixelnet) {
        //pixelnet takes 45ms to send so we need to
        //use a background thread just in case we have a 25ms
        //sequence.   The main thread would get blocked.
        m_useOutputThread = 1;
    }

	m_startChannels.resize(config["outputs"].size());

	// Initialize the ouputs
	for (int i = 0; i < m_outputs; i++)
	{
		m_startChannels[i] = 0;
	}

    int maxChannel = 0;
    int maxLen = 0;
	for (int i = 0; i < config["outputs"].size(); i++)
	{
		Json::Value s = config["outputs"][i];

		m_startChannels[s["outputNumber"].asInt()] = s["startChannel"].asInt() - 1;
        int l = s["channelCount"].asInt();
        if (l > maxLen) {
            maxLen = l;
        }
	}

    m_channelCount = 0;
    for (int i = 0; i < m_outputs; i++)
    {
        if (m_channelCount < m_startChannels[i]) {
            m_channelCount = m_startChannels[i];
        }
    }
    m_channelCount += (m_pixelnet ? 4096 : 512);
    m_channelCount -= m_startChannel;
    
    m_channelCount = config["channelCount"].asInt();

	m_config = reinterpret_cast<ledscape_config_t*>(calloc(1, sizeof(ledscape_config_t)));
	if (!m_config)
	{
		LogErr(VB_CHANNELOUT, "Unable to allocate LEDscape config\n");
		return 0;
	}

	ledscape_strip_config_t * const lsconfig = &m_config->strip_config;

	int pruNumber = BBB_PRU;

	lsconfig->type         = LEDSCAPE_STRIP;
	lsconfig->leds_width   = (int)((m_pixelnet != 0 ? 4102 : 513) / 3) + 1;
	lsconfig->leds_height  = m_outputs;

	string pru_program = "/tmp/FalconSerial.bin";

    const char *mode = BBB_PRU ? "gpio" : "pruout";
    
    if (BBB_PRU) {
        args.push_back("-DRUNNING_ON_PRU1");
    } else {
        args.push_back("-DRUNNING_ON_PRU0");
    }
    
    if (config["device"] == "F4-B") {
        args.push_back("-DONLYA");
        configurePRUPins(0, 4, mode);
    } else if (config["device"] == "F8-B-16" || config["device"] == "F8-B-EXP-32") {
        args.push_back("-DONLYB");
        configurePRUPins(4, 8, mode);
    } else if (config["device"] == "F32-B") {
        args.push_back("-DF32B");
        configurePRUPins(0, 8, mode);
    } else {
        configurePRUPins(0, 8, mode);
    }
    
    if (!m_pixelnet) {
        char buf[256];
        if (maxLen < 1 || maxLen > 512) {
            maxLen = 512;
        }
        sprintf(buf,"-DDATALEN=%d", (maxLen + 1));
        args.push_back(buf);
    }

    
    compileSerialPRUCode(args);
	if (!FileExists(pru_program.c_str()))
	{
		LogErr(VB_CHANNELOUT, "%s does not exist!\n", pru_program.c_str());
		free(m_config);
		m_config = NULL;

		return 0;
	}
    LogDebug(VB_CHANNELOUT, "Using program %s\n", pru_program.c_str());
	m_leds = ledscape_strip_init(m_config, 0, pruNumber, pru_program.c_str());

	if (!m_leds)
	{
		LogErr(VB_CHANNELOUT, "Unable to initialize LEDscape\n");
		free(m_config);
		m_config = NULL;

		return 0;
	}

    int sz = m_pixelnet ? (4096 + 6): (512 + 1);
    
    m_lastData = (uint8_t*)malloc(m_outputs * sz);
    m_curData = (uint8_t*)malloc(m_outputs * sz);
    
	for (int i = 0; i < m_outputs; i++)
	{
		if (m_pixelnet)
		{
			m_curData[i + (0 * m_outputs)] = '\xAA';
			m_curData[i + (1 * m_outputs)] = '\x55';
			m_curData[i + (2 * m_outputs)] = '\x55';
			m_curData[i + (3 * m_outputs)] = '\xAA';
			m_curData[i + (4 * m_outputs)] = '\x15';
			m_curData[i + (5 * m_outputs)] = '\x5D';
		}
		else
		{
			m_curData[i] = '\x00';
		}
	}
    memcpy(m_lastData, m_curData, m_outputs * sz);
	return ChannelOutputBase::Init(config);
}

/*
 *
 */
int BBBSerialOutput::Close(void)
{
	LogDebug(VB_CHANNELOUT, "BBBSerialOutput::Close()\n");

    // Send the stop command
    m_leds->ws281x->command = 0xFF;
    
    prussdrv_pru_wait_event(BBB_PRU); //PRU_EVTOUT_0;
    prussdrv_pru_clear_event(PRU_ARM_INTERRUPT);
    prussdrv_pru_disable(m_leds->pru->pru_num);
    
    //ledscape_close only checks PRU0 events and then unmaps the memory that
    //may be used by the other pru
	//ledscape_close(m_leds);

	free(m_config);
	m_config = NULL;
    
    configurePRUPins(0, 8, "gpio");

    LogDebug(VB_CHANNELOUT, "BBBSerialOutput::Close() done\n");

	return ChannelOutputBase::Close();
}

/*
 *
 */
int BBBSerialOutput::RawSendData(unsigned char *channelData)
{
	LogExcess(VB_CHANNELOUT, "BBBSerialOutput::RawSendData(%p)\n",
		channelData);

	ledscape_strip_config_t *config = reinterpret_cast<ledscape_strip_config_t*>(m_config);
    m_curFrame++;
    
	// Bypass LEDscape draw routine and format data for PRU ourselves
    
    uint8_t * const out = m_curData;
	uint8_t *c = out;
	uint8_t *s = (uint8_t*)channelData;
	int chCount = m_pixelnet ? 4096 : 512;

	for (int i = 0; i < m_outputs; i++)
	{
		// Skip the headers (6 bytes per output for Pixelnet and 1 byte per output
		// for DMX) and index into the proper position in the m_outputs number of
		// bytes in each slice
		if (m_pixelnet)
			c = out + i + (m_outputs * 6);
		else
			c = out + i + (m_outputs);

		// Get the start channel for this output
        s = (uint8_t*)(channelData + m_startChannels[i] - m_startChannel);
        
		// Now copy the individual channel data into each slice
		for (int ch = 0; ch < chCount; ch++)
		{
			*c = *s;
            s++;
			c += m_outputs;
		}
	}

	// Wait for the previous draw to finish
	while (m_leds->ws281x->command);

    unsigned frame = 0;
    //unsigned frame = m_curFrame & 1;
    if (m_curFrame == 1 || memcmp(m_lastData, m_curData, m_leds->frame_size)) {
        //don't copy to DMA memory unless really needed to avoid bus contention on the DMA bus
        int sz = m_pixelnet ? (4096 + 6): (512 + 1);
        sz *= m_outputs;
#ifdef USING_PRU_RAM
        uint8_t * const realout = (uint8_t *)m_leds->pru->data_ram + 512;
        memcpy(realout, m_curData, sz);
#else
        size_t offset = m_leds->pru->ddr_size - 84*1024;
        uint8_t * const realout = (uint8_t *)m_leds->pru->ddr + offset;
        memcpy(realout, m_curData, sz);

        // Map
        m_leds->ws281x->pixels_dma = m_leds->pru->ddr_addr + offset;
#endif
        
        uint8_t *tmp = m_lastData;
        m_lastData = m_curData;
        m_curData = tmp;
    }

	// Send the start command
	m_leds->ws281x->command = 1;

	return m_channelCount;
}

/*
 *
 */
void BBBSerialOutput::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "BBBSerialOutput::DumpConfig()\n");

	ledscape_strip_config_t *config = reinterpret_cast<ledscape_strip_config_t*>(m_config);
	
	LogDebug(VB_CHANNELOUT, "    Pixelnet      : %d\n", m_pixelnet);
	LogDebug(VB_CHANNELOUT, "    Outputs       :\n" );

	for (int i = 0; i < m_outputs; i++)
	{
		LogDebug(VB_CHANNELOUT, "        #%d: %d-%d (%d Ch)\n",
			i + 1,
			m_startChannels[i] + 1,
			m_pixelnet ? m_startChannels[i] + 4096 : m_startChannels[i] + 512,
			m_pixelnet ? 4096 : 512);
	}

	ChannelOutputBase::DumpConfig();
}

