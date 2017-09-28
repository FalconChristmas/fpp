/*
 *   BeagleBone Black PRU 48-string handler for Falcon Pi Player (FPP)
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
#include <ctime>
#include <thread>
#include <chrono>

#define BBB_PRU  1
#define PRU_ARM_INTERRUPT PRU1_ARM_INTERRUPT

//  #define PRINT_STATS


#include <pruss_intc_mapping.h>
#define PRUSS0_PRU0_DATARAM     0
#define PRUSS0_PRU1_DATARAM     1
extern "C" {
    extern int prussdrv_pru_clear_event(unsigned int eventnum);
    extern int prussdrv_pru_wait_event(unsigned int pru_evtout_num);
    extern int prussdrv_pru_disable(unsigned int prunum);
    extern int prussdrv_map_prumem(unsigned int pru_ram_id, void **address);
    extern int prussdrv_exit();
}

// LEDscape includes
#include "pru.h"

// FPP includes
#include "common.h"
#include "log.h"
#include "BBB48String.h"
#include "settings.h"

/////////////////////////////////////////////////////////////////////////////
// Map the 48 ports to positions in LEDscape code
// - Array index is output port number configured in UI
// - Value is position to use in LEDscape code

static const int PinMapLEDscape[] = {
	 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
	10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
	30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
	40, 41, 42, 43, 44, 45, 46, 47
};

static const int PinMapRGBCape48C[] = {
	14, 15,  1,  2,  5,  6,  9, 10, 32, 33,
	34, 35, 36, 37, 38, 39, 40, 41, 42, 43,
	44, 45, 46, 47, 11,  8,  7,  4,  3,  0,
	16, 20, 19, 17, 22, 23, 12, 25, 13, 30,
	31, 28, 29, 26, 27, 24, 21, 18
};

/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
BBB48StringOutput::BBB48StringOutput(unsigned int startChannel,
	unsigned int channelCount)
  : ChannelOutputBase(startChannel, channelCount),
	m_config(NULL),
	m_leds(NULL),
	m_maxStringLen(0),
    m_lastData(NULL),
    m_curData(NULL),
    m_curFrame(0),
    m_otherPruRam(NULL)
{
	LogDebug(VB_CHANNELOUT, "BBB48StringOutput::BBB48StringOutput(%u, %u)\n",
		startChannel, channelCount);
    m_useOutputThread = 0;
}

/*
 *
 */
BBB48StringOutput::~BBB48StringOutput()
{
	LogDebug(VB_CHANNELOUT, "BBB48StringOutput::~BBB48StringOutput()\n");
	m_strings.clear();
    prussdrv_exit();
    if (m_lastData) free(m_lastData);
    if (m_curData) free(m_curData);
}


inline std::string mapF8Size(int max, int maxString, int &newHeight) {
    printf("%d  %d\n", max, maxString);
    if (maxString > max) {
        maxString = max;
    }
    if (maxString <= 4) {
        newHeight = 4;
        return "FalconWS281x_F8_4.bin";
    } else if (maxString <= 6) {
        newHeight = 6;
        return "FalconWS281x_F8_6.bin";
    } else if (maxString <= 8) {
        newHeight = 8;
        return "FalconWS281x_F8_8.bin";
    } else if (maxString <= 12) {
        newHeight = 12;
        return "FalconWS281x_F8_12.bin";
    } else if (maxString <= 16) {
        newHeight = 16;
        return "FalconWS281x_F8_16.bin";
    }
    newHeight = 20;
    return "FalconWS281x_F8_20.bin";
}


/*
 *
 */
int BBB48StringOutput::Init(Json::Value config)
{
	LogDebug(VB_CHANNELOUT, "BBB48StringOutput::Init(JSON)\n");

	m_subType = config["subType"].asString();
	m_channelCount = config["channelCount"].asInt();
	m_config = &ledscape_matrix_default;

	for (int i = 0; i < config["outputs"].size(); i++)
	{
		Json::Value s = config["outputs"][i];
		PixelString *newString = new PixelString;

        int brightness = 100;
        if (!s["brightness"].isNull()) {
            brightness = s["brightness"].asInt();
        }
        
		if (!newString->Init(s["portNumber"].asInt(),
			m_startChannel,
			s["startChannel"].asInt() - 1,
			s["pixelCount"].asInt(),
			s["colorOrder"].asString(),
			s["nullNodes"].asInt(),
			s["hybridMode"].asInt(),
			s["reverse"].asInt(),
			s["grouping"].asInt(),
			s["zigZag"].asInt(),
            brightness))
			return 0;

		if ((newString->m_pixelCount + newString->m_nullNodes) > m_maxStringLen)
			m_maxStringLen = newString->m_pixelCount + newString->m_nullNodes;

		m_strings.push_back(newString);
	}

	if (!MapPins())
	{
		LogErr(VB_CHANNELOUT, "Unable to map pins\n");
		return 0;
	}

    if (m_maxStringLen == 0) {
        LogErr(VB_CHANNELOUT, "No pixels configured in any string\n");
        return 0;
    }
	m_config = reinterpret_cast<ledscape_config_t*>(calloc(1, sizeof(ledscape_config_t)));
	if (!m_config)
	{
		LogErr(VB_CHANNELOUT, "Unable to allocate LEDscape config\n");
		return 0;
	}

	ledscape_strip_config_t * const lsconfig = &m_config->strip_config;

	lsconfig->type         = LEDSCAPE_STRIP;
	lsconfig->leds_width   = m_maxStringLen;

	//lsconfig->leds_height  = m_strings.size();
	// LEDscape always drives 48 ports, so use this instead of # of strings
	lsconfig->leds_height = 48;


    int retVal = ChannelOutputBase::Init(config);
    if (retVal == 0) {
        return 0;
    }
    int maxString = -1;
    for (int s = 0; s < m_strings.size(); s++) {
        PixelString *ps = m_strings[s];
        if (ps->m_pixelCount != 0) {
            maxString = s;
        }
    }
    maxString++;
    
	std::string pru_program(getBinDirectory());

	if (tail(pru_program, 4) == "/src")
		pru_program += "/pru/";
	else
		pru_program += "/../lib/";

    if (m_subType == "F4-B")
    {
        pru_program += "FalconWS281x_F4.bin";
        lsconfig->leds_height = 4;
    }
    else if (m_subType == "F16-B")
    {
        pru_program += "FalconWS281x_16.bin";
        lsconfig->leds_height = 16;
    }
    else if (m_subType == "F16-B-32" || m_subType == "F16-B-40" || m_subType == "F32-B")
    {
        pru_program += "FalconWS281x_40.bin";
        lsconfig->leds_height = 40;
    }
    else if (m_subType == "F16-B-48")
	{
		pru_program += "FalconWS281x_48.bin";
	}
    else if (m_subType == "F8-B")
    {
        pru_program += mapF8Size(12, maxString, lsconfig->leds_height);
    }
    else if (m_subType == "F8-B-16")
    {
        pru_program += mapF8Size(16, maxString, lsconfig->leds_height);
    }
    else if (m_subType == "F8-B-20")
    {
        pru_program += mapF8Size(20, maxString, lsconfig->leds_height);
    }
    else if (m_subType == "F8-B-EXP")
    {
        pru_program += "FalconWS281x_F8_EXP.bin";
        lsconfig->leds_height = 28;
    }
    else if (m_subType == "F8-B-EXP-32")
    {
        pru_program += "FalconWS281x_F8_EXP_32.bin";
        lsconfig->leds_height = 32;
    }
    else if (m_subType == "F8-B-EXP-36")
    {
        pru_program += "FalconWS281x_F8_EXP_36.bin";
        lsconfig->leds_height = 36;
    }
    else if (m_subType == "RGBCape48C")
    {
        pru_program += "FalconWS281x_RGBCape48C.bin";
        lsconfig->leds_height = 48;
    }
    else if (m_subType == "RGBCape48F")
    {
        pru_program += "FalconWS281x_RGBCape48F.bin";
        lsconfig->leds_height = 48;
    }
	else
	{
		pru_program += m_subType + ".bin";
	}
    m_pruProgram = pru_program;
    if (!StartPRU()) {
        return 0;
    }
    m_lastData = (uint8_t*)calloc(1, m_leds->frame_size);
    m_curData = (uint8_t*)calloc(1, m_leds->frame_size);
    
    return retVal;
}

int BBB48StringOutput::StartPRU()
{
    m_curFrame = 0;
    int pruNumber = BBB_PRU;
    ledscape_strip_config_t * const lsconfig = &m_config->strip_config;

    LogInfo(VB_CHANNELOUT, "Initializing PRU: %d     Num strings: %d    Using program %s\n", pruNumber, lsconfig->leds_height, m_pruProgram.c_str());
    m_leds = ledscape_strip_init(m_config, 0, pruNumber, m_pruProgram.c_str());
    LogDebug(VB_CHANNELOUT, "Init Command: %d      Data:   %X\n", m_leds->ws281x->command, m_leds->pru->ddr);
    if (!m_leds)
    {
        LogErr(VB_CHANNELOUT, "Unable to initialize LEDscape\n");
        
        return 0;
    }
    //get the other PRU's sram pointer as well
    void *other = nullptr;
    prussdrv_map_prumem(pruNumber == 0 ? PRUSS0_PRU1_DATARAM :PRUSS0_PRU0_DATARAM,
                        &other
                        );
    m_otherPruRam = (uint8_t*)other;
    uint32_t *timings = (uint32_t *)m_leds->ws281x;
    for (int x = 0; x < 30*3; x++) {
        timings[x + 17] = 0;
    }

    return 1;
}
void BBB48StringOutput::StopPRU(bool wait)
{
    // Send the stop command
    m_leds->ws281x->command = 0xFF;
    
    if (wait) {
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    //prussdrv_pru_wait_event(BBB_PRU); //PRU_EVTOUT_1;
    prussdrv_pru_clear_event(PRU_ARM_INTERRUPT);
    prussdrv_pru_disable(m_leds->pru->pru_num);

    //ledscape_close only checks PRU0 events and then unmaps the memory that
    //may be used by the other pru
    //ledscape_close(m_leds);
}
/*
 *
 */
int BBB48StringOutput::Close(void)
{
	LogDebug(VB_CHANNELOUT, "BBB48StringOutput::Close()\n");

    StopPRU();

	free(m_config);
	m_config = NULL;

	return ChannelOutputBase::Close();
}

/*
 *
 */
int BBB48StringOutput::RawSendData(unsigned char *channelData)
{
	LogExcess(VB_CHANNELOUT, "BBB48StringOutput::RawSendData(%p)\n",
		channelData);

	ledscape_strip_config_t *config = reinterpret_cast<ledscape_strip_config_t*>(m_config);
    
    m_curFrame++;

#ifdef PRINT_STATS
    uint16_t *timings = (uint16_t *)m_leds->ws281x;
    int max = 0;
    for (int x = 0; x < 30*3; x++) {
        if (max < timings[x + 33]) {
            max = timings[x + 33];
        }
    }
    if (max > 300 || (m_curFrame % 10) == 1) {
        for (int x = 33; x < 208; ) {
            printf("%d ", timings[x]);
            ++x;
            if ((x - 33) % 30 == 0) {
                printf("\n");
            }
        }
        printf("\n%d:  max %d\n", m_curFrame,  max);
    }
#endif

	// Bypass LEDscape draw routine and format data for PRU ourselves
    uint8_t * out = m_curData;

	PixelString *ps = NULL;
	uint8_t *c = NULL;
	int inCh;

    int numStrings = m_config->strip_config.leds_height;

	for (int s = 0; s < m_strings.size(); s++)
	{
		ps = m_strings[s];
        uint8_t *brightness = ps->m_brightnessMap;
		c = out + (ps->m_nullNodes * numStrings * 3) + ps->m_portNumber;

		if ((ps->m_hybridMode) &&
			((channelData[ps->m_outputMap[0]]) ||
			 (channelData[ps->m_outputMap[1]]) ||
			 (channelData[ps->m_outputMap[2]])))
		{
			for (int p = 0; p < ps->m_pixelCount; p++)
			{
				*c = brightness[channelData[ps->m_outputMap[0]]];
				c += numStrings;

				*c = brightness[channelData[ps->m_outputMap[1]]];
				c += numStrings;

				*c = brightness[channelData[ps->m_outputMap[2]]];
				c += numStrings;
			}
		}
		else
		{
			if (ps->m_hybridMode)
				inCh = 3;
			else
				inCh = 0;

			for (int p = 0; p < ps->m_pixelCount; p++)
			{
				*c = brightness[channelData[ps->m_outputMap[inCh++]]];
				c += numStrings;

				*c = brightness[channelData[ps->m_outputMap[inCh++]]];
				c += numStrings;

				*c = brightness[channelData[ps->m_outputMap[inCh++]]];
				c += numStrings;
			}
		}
		
	}
	// Wait for the previous draw to finish
    std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float, std::milli> duration = std::chrono::high_resolution_clock::now() - t1;
    while (m_leds->ws281x->command && duration.count() < 20) {
        pthread_yield();
        duration = std::chrono::high_resolution_clock::now() - t1;
    }
    if (m_leds->ws281x->command) {
        StopPRU(false);
        StartPRU();
    }
    
    int frameSize = numStrings * m_leds->ws281x->num_pixels * 3;

    unsigned frame = 0;
    //unsigned frame = m_curFrame & 1;
    if (m_curFrame == 1 || memcmp(m_lastData, m_curData, frameSize)) {
        //don't copy to DMA memory unless really needed to avoid bus contention on the DMA bus

        //copy first 7.5K into PRU mem directly
        int fullsize = frameSize;
        int mx = frameSize;
        if (mx > (8*1024 - 512)) {
            mx = 8*1024 - 512;
        }
        
        //first 7.5K to main PRU ram
        uint8_t * const pruMem = (uint8_t *)m_leds->ws281x + 512;
        memcpy(pruMem, m_curData, mx);
        if (fullsize > 7630) {
            fullsize -= 7630;
            if (fullsize > (8*1024 - 512)) {
                fullsize = 8*1024 - 512;
            }
            // second 7.5K to other PRU ram
            memcpy(m_otherPruRam + 512, m_curData + 7630, fullsize);
        }
        if ((7630 * 2) < frameSize) {
            // more than what fits in the SRAMs
            //don't need to copy the first part as that's in sram, just copy the last parts
            int off = 7630 * 2 - 50;
            uint8_t * const realout = (uint8_t *)m_leds->pru->ddr + m_leds->frame_size * frame + off;
            memcpy(realout, m_curData + off, frameSize - off);
        }

        uint8_t *tmp = m_lastData;
        m_lastData = m_curData;
        m_curData = tmp;
    }
    
	// Map
	m_leds->ws281x->pixels_dma = m_leds->pru->ddr_addr + m_leds->frame_size * frame;

	// Send the start command
	m_leds->ws281x->command = 1;

	return m_channelCount;
}

/*
 *
 */
void BBB48StringOutput::ApplyPinMap(const int *map)
{
	int origPortNumber = 0;

	for (int i = 0; i < m_strings.size(); i++)
	{
		origPortNumber = m_strings[i]->m_portNumber;
		m_strings[i]->m_portNumber = map[origPortNumber];
	}
}

/*
 *
 */
int BBB48StringOutput::MapPins(void)
{
	if (m_subType == "RGBCape48C")
	{
		ApplyPinMap(PinMapRGBCape48C);
	}

	return 1;
}

/*
 *
 */
void BBB48StringOutput::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "BBB48StringOutput::DumpConfig()\n");

	ledscape_strip_config_t *config = reinterpret_cast<ledscape_strip_config_t*>(m_config);
    LogDebug(VB_CHANNELOUT, "    type          : %s\n", m_subType.c_str());
	LogDebug(VB_CHANNELOUT, "    strings       : %d\n", m_strings.size());
	LogDebug(VB_CHANNELOUT, "    longest string: %d pixels\n", m_maxStringLen);

	for (int i = 0; i < m_strings.size(); i++)
	{
		LogDebug(VB_CHANNELOUT, "    string #%02d\n", i);
		m_strings[i]->DumpConfig();
	}

	ChannelOutputBase::DumpConfig();
}

