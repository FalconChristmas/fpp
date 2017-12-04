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
#include <sys/wait.h>

#include <ctime>
#include <set>
#include <thread>
#include <chrono>
#include <iostream>
#include <fstream>

#define BBB_PRU  1
#define PRU_ARM_INTERRUPT PRU1_ARM_INTERRUPT

//  #define PRINT_STATS


#include <pruss_intc_mapping.h>
#define PRUSS0_PRU0_DATARAM     0
#define PRUSS0_PRU1_DATARAM     1
#define PRUSS0_SHARED_DATARAM   4
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
    m_otherPruRam(NULL),
    m_sharedPruRam(NULL)
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


static void compilePRUCode(std::vector<std::string> &sargs) {
    pid_t compilePid = fork();
    if (compilePid == 0) {
        char * args[sargs.size() + 3];
        args[0] = "/bin/bash";
        args[1] = "/opt/fpp/src/pru/compileWS2811x.sh";
        
        for (int x = 0; x < sargs.size(); x++) {
            args[x + 2] = (char*)sargs[x].c_str();
        }
        args[sargs.size() + 2] = NULL;

        execvp("/bin/bash", args);
    } else {
        wait(NULL);
    }
}

inline void mapSize(int max, int maxString, int &newHeight, std::vector<std::string> &args) {
    if (maxString > max) {
        maxString = max;
    }
    //make sure it's a multiple of 4 to keep things aligned in memory
    if (maxString <= 4) {
        newHeight = 4;
    } else if (maxString <= 8) {
        newHeight = 8;
    } else if (maxString <= 12) {
        newHeight = 12;
    } else if (maxString <= 16) {
        newHeight = 16;
    } else if (maxString <= 20) {
        newHeight = 20;
    } else if (maxString <= 24) {
        newHeight = 24;
    } else if (maxString <= 28) {
        newHeight = 28;
    } else if (maxString <= 32) {
        newHeight = 32;
    } else if (maxString <= 36) {
        newHeight = 36;
    } else if (maxString <= 40) {
        newHeight = 40;
    } else if (maxString <= 44) {
        newHeight = 44;
    } else {
        newHeight = 48;
    }
    args.push_back("-DOUTPUTS=" + std::to_string(newHeight));
}
static void createOutputLengths(std::vector<PixelString*> &m_strings,
                                int maxStringLen) {
    
    printf("createOutputLengths   %d\n", maxStringLen);
    std::ofstream outputFile;
    outputFile.open("/tmp/OutputLengths.hp", std::ofstream::out | std::ofstream::trunc);
    
#ifdef PRINT_STATS
    outputFile << "#define RECORD_STATS\n\n";
#endif
    std::set<int> sizes;
    for (int x = 0; x < m_strings.size(); x++) {
        int pc = m_strings[x]->m_pixelCount + m_strings[x]->m_nullNodes;
        if (pc != 0) {
            sizes.insert(pc);
        }
    }
    
    outputFile << ".macro CheckOutputLengths\n";
    outputFile << "    QBNE skip_end, cur_data, next_check\n";
    auto i = sizes.begin();
    while (i != sizes.end()) {
        int min = *i;
        if (min != maxStringLen) {
            if ((min*3) <= 255) {
                outputFile << "    QBNE skip_"
                << std::to_string(min)
                << ", cur_data, "
                << std::to_string(min * 3)
                << "\n";
            } else {
                outputFile << "    LDI r8, " << std::to_string(min * 3) << "\n";
                outputFile << "    QBNE skip_"
                << std::to_string(min)
                << ", cur_data, r8\n";
            }
            
            for (int y = 0; y < m_strings.size(); y++) {
                int pc = m_strings[y]->m_pixelCount + m_strings[y]->m_nullNodes;
                if (pc == min) {
                    std::string o = std::to_string(y + 1);
                    outputFile << "        CLR GPIO_MASK(o" << o << "_gpio), o" << o << "_pin\n";
                }
            }
            i++;
            int next = *i;
            next *= 3;
            outputFile << "    LDI next_check, " << std::to_string(next) << "\n";
            outputFile << "    skip_"
            << std::to_string(min)
            << ":\n";
        } else {
            i++;
        }
    }
    outputFile << "    skip_end:\n";
    outputFile << ".endm\n";
    if (sizes.empty()) {
        outputFile << "#define SET_FIRST_CHECK \\\n    LDI next_check, 10000\n";
    } else {
        int sz = *sizes.begin();
        outputFile << "#define SET_FIRST_CHECK \\\n    LDI next_check, " << std::to_string(sz*3) << "\n";
    }

    outputFile.close();
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
        float gamma = 1.0f;
        if (!s["brightness"].isNull()) {
            brightness = s["brightness"].asInt();
        }
        if (!s["gamma"].isNull()) {
            gamma = s["gamma"].asFloat();
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
            brightness,
            gamma))
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
    
    std::vector<std::string> args;
    
    if (m_subType == "F4-B") {
        args.push_back("-DF4B");
        mapSize(4, maxString, lsconfig->leds_height, args);
    } else if (m_subType == "F16-B") {
        args.push_back("-DF16B");
        mapSize(16, maxString, lsconfig->leds_height, args);
    } else if (m_subType == "F16-B-32" || m_subType == "F16-B-40") {
        args.push_back("-DF16B");
        mapSize(40, maxString, lsconfig->leds_height, args);
    } else if (m_subType == "F32-B") {
        args.push_back("-DF32B");
        mapSize(40, maxString, lsconfig->leds_height, args);
    } else if (m_subType == "F32-B-48") {
        args.push_back("-DF32B");
        mapSize(48, maxString, lsconfig->leds_height, args);
    } else if (m_subType == "F16-B-48") {
        args.push_back("-DF16B");
        mapSize(48, maxString, lsconfig->leds_height, args);
	} else if (m_subType == "F8-B") {
        args.push_back("-DF8B");
        mapSize(12, maxString, lsconfig->leds_height, args);
    } else if (m_subType == "F8-B-16") {
        args.push_back("-DF8B");
        args.push_back("-DPORTA");
        mapSize(16, maxString, lsconfig->leds_height, args);
    } else if (m_subType == "F8-B-20") {
        args.push_back("-DF8B");
        args.push_back("-DPORTA");
        args.push_back("-DPORTB");
        mapSize(20, maxString, lsconfig->leds_height, args);
    } else if (m_subType == "F8-B-EXP") {
        args.push_back("-DF8B");
        args.push_back("-DF8B_EXP=1");
        mapSize(28, maxString, lsconfig->leds_height, args);
    } else if (m_subType == "F8-B-EXP-32") {
        args.push_back("-DF8B");
        args.push_back("-DF8B_EXP=1");
        args.push_back("-DPORTA");
        mapSize(32, maxString, lsconfig->leds_height, args);
    } else if (m_subType == "F8-B-EXP-36") {
        args.push_back("-DF8B");
        args.push_back("-DF8B_EXP=1");
        args.push_back("-DPORTA");
        args.push_back("-DPORTB");
        mapSize(36, maxString, lsconfig->leds_height, args);
    } else if (m_subType == "RGBCape24") {
        args.push_back("-DRGBCape24=1");
        mapSize(24, maxString, lsconfig->leds_height, args);
    } else if (m_subType == "RGBCape48C") {
        args.push_back("-DRGBCape48C=1");
        mapSize(48, maxString, lsconfig->leds_height, args);
    } else if (m_subType == "RGBCape48F") {
        args.push_back("-DRGBCape48F=1");
        mapSize(48, maxString, lsconfig->leds_height, args);
	}
    
    for (int x = 0; x < lsconfig->leds_height; x++) {
        if (x >= m_strings.size() || m_strings[x]->m_pixelCount == 0) {
            std::string v = "-DNOOUT";
            v += std::to_string(x+1);
            args.push_back(v);
        }
    }
    createOutputLengths(m_strings, m_maxStringLen);
    
    if (m_maxStringLen < 321) {
        //if there is plenty of time to output the GPIO0 stuff
        //after the other GPIO's, let's do that
        args.push_back("-DSPLIT_GPIO0");
    }
    
    compilePRUCode(args);
    m_pruProgram = "/tmp/FalconWS281x.bin";
    if (!StartPRU()) {
        return 0;
    }
    m_lastData = (uint8_t*)calloc(1, m_leds->frame_size);
    m_curData = (uint8_t*)calloc(1, m_leds->frame_size);
    
    uint32_t *timings = (uint32_t *)m_leds->ws281x;
    for (int x = 0; x < 60*3; x++) {
        timings[x + 17] = 0;
    }

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
                        &other);
    m_otherPruRam = (uint8_t*)other;
    
    other = nullptr;
    prussdrv_map_prumem(PRUSS0_SHARED_DATARAM,
                        &other);
    m_sharedPruRam = (uint8_t*)other;
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

    
    uint8_t *data = (uint8_t *)m_leds->ws281x;
    
#ifdef PRINT_STATS
    uint32_t *timings = (uint32_t *)(data + 64);
    int max = 0;
    for (int x = 0; x < 30*3; x++) {
        if (max < timings[x]) {
            max = timings[x];
        }
    }
    if (max > 300 || (m_curFrame % 10) == 1) {
        for (int x = 0; x < 48; ) {
            printf("%8X ", timings[x]);
            ++x;
            if ((x) % 16 == 0) {
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
        uint8_t * const pruMem = (uint8_t *)m_leds->ws281x;
        memcpy(pruMem + 512, m_curData, mx);
        if (fullsize > 7628) {
            fullsize -= 7628;
            int outsize = fullsize;
            if (outsize > (8*1024 - 512)) {
                outsize = 8*1024 - 512;
            }
            // second 7.5K to other PRU ram
            memcpy(m_otherPruRam + 512, m_curData + 7628, outsize);
            fullsize -= outsize;
        }
        if (fullsize > 0) {
            int outsize = fullsize;
            if (outsize > (12*1024)) {
                outsize = 12*1024;
            }
            memcpy(m_sharedPruRam, m_curData + 7628 + 7628, outsize);
        }
        int off = 7628 * 2 + 12188;
        if (off < frameSize) {
            // more than what fits in the SRAMs
            //don't need to copy the first part as that's in sram, just copy the last parts
            off -= 100;
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

