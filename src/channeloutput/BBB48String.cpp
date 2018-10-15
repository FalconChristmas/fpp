/*
 *   BeagleBone Black PRU 48-string handler for Falcon Player (FPP)
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

//  #define PRINT_STATS


#include <pruss_intc_mapping.h>
#include <prussdrv.h>

// FPP includes
#include "common.h"
#include "log.h"
#include "BBBUtils.h"
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
    m_maxStringLen(0),
    m_lastData(NULL),
    m_curData(NULL),
    m_curFrame(0),
    m_pru(NULL),
    m_pruData(NULL),
    m_stallCount(0)
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
    if (m_lastData) free(m_lastData);
    if (m_curData) free(m_curData);
    if (m_pru) delete m_pru;
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
    
    std::ofstream outputFile;
    outputFile.open("/tmp/OutputLengths.hp", std::ofstream::out | std::ofstream::trunc);
    
#ifdef PRINT_STATS
    outputFile << "#define RECORD_STATS\n\n";
#endif
    std::set<int> sizes;
    for (int x = 0; x < m_strings.size(); x++) {
        int pc = m_strings[x]->m_outputChannels;
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
            if (min <= 255) {
                outputFile << "    QBNE skip_"
                << std::to_string(min)
                << ", cur_data, "
                << std::to_string(min)
                << "\n";
            } else {
                outputFile << "    LDI r8, " << std::to_string(min) << "\n";
                outputFile << "    QBNE skip_"
                << std::to_string(min)
                << ", cur_data, r8\n";
            }
            
            for (int y = 0; y < m_strings.size(); y++) {
                int pc = m_strings[y]->m_outputChannels;
                if (pc == min) {
                    std::string o = std::to_string(y + 1);
                    outputFile << "        CLR GPIO_MASK(o" << o << "_gpio), o" << o << "_pin\n";
                }
            }
            i++;
            int next = *i;
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
        outputFile << "#define SET_FIRST_CHECK \\\n    LDI next_check, " << std::to_string(sz) << "\n";
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

    
    for (int i = 0; i < config["outputs"].size(); i++) {
        Json::Value s = config["outputs"][i];
        PixelString *newString = new PixelString;

        if (!newString->Init(s))
            return 0;

        if (newString->m_outputChannels > m_maxStringLen) {
            m_maxStringLen = newString->m_outputChannels;
        }

        m_strings.push_back(newString);
    }
    
    if (!MapPins()) {
        LogErr(VB_CHANNELOUT, "Unable to map pins\n");
        return 0;
    }

    if (m_maxStringLen == 0) {
        LogErr(VB_CHANNELOUT, "No pixels configured in any string\n");
        return 0;
    }
    m_numStrings = 48;

    int retVal = ChannelOutputBase::Init(config);
    if (retVal == 0) {
        return 0;
    }
    int maxString = -1;
    for (int s = 0; s < m_strings.size(); s++) {
        PixelString *ps = m_strings[s];
        if (ps->m_outputChannels != 0) {
            maxString = s;
        }
    }
    maxString++;
    
    std::vector<std::string> args;
    std::string postf = "B";
    if (getBeagleBoneType() == PocketBeagle) {
        postf = "PB";
    } else if (config["pinoutVersion"].asString() == "2.x") {
        postf = "Bv2";
    }
    
    if (m_subType == "F4-B") {
        args.push_back("-DF4B");
        mapSize(4, maxString, m_numStrings, args);
    } else if (m_subType == "F16-B") {
        args.push_back("-DF16B");
        mapSize(16, maxString, m_numStrings, args);
    } else if (m_subType == "F16-B-32" || m_subType == "F16-B-40") {
        args.push_back("-DF16B");
        mapSize(40, maxString, m_numStrings, args);
    } else if (m_subType == "F32-B") {
        args.push_back("-DF32B");
        mapSize(40, maxString, m_numStrings, args);
    } else if (m_subType == "F32-B-48") {
        args.push_back("-DF32B");
        mapSize(48, maxString, m_numStrings, args);
    } else if (m_subType == "F16-B-48") {
        args.push_back("-DF16B");
        mapSize(48, maxString, m_numStrings, args);
	} else if (m_subType == "F8-B") {
        args.push_back("-DF8" + postf);
        mapSize(12, maxString, m_numStrings, args);
    } else if (m_subType == "F8-B-16") {
        args.push_back("-DF8" + postf);
        args.push_back("-DPORTA");
        mapSize(16, maxString, m_numStrings, args);
    } else if (m_subType == "F8-B-20") {
        args.push_back("-DF8" + postf);
        args.push_back("-DPORTA");
        args.push_back("-DPORTB");
        mapSize(20, maxString, m_numStrings, args);
    } else if (m_subType == "F8-B-EXP") {
        args.push_back("-DF8" + postf);
        args.push_back("-DF8B_EXP=1");
        mapSize(28, maxString, m_numStrings, args);
    } else if (m_subType == "F8-B-EXP-32") {
        args.push_back("-DF8" + postf);
        args.push_back("-DF8B_EXP=1");
        args.push_back("-DPORTA");
        mapSize(32, maxString, m_numStrings, args);
    } else if (m_subType == "F8-B-EXP-36") {
        args.push_back("-DF8" + postf);
        args.push_back("-DF8B_EXP=1");
        args.push_back("-DPORTA");
        args.push_back("-DPORTB");
        mapSize(36, maxString, m_numStrings, args);
    } else if (m_subType == "RGBCape24") {
        args.push_back("-DRGBCape24=1");
        mapSize(24, maxString, m_numStrings, args);
    } else if (m_subType == "RGBCape48C") {
        args.push_back("-DRGBCape48C=1");
        mapSize(48, maxString, m_numStrings, args);
    } else if (m_subType == "RGBCape48F") {
        args.push_back("-DRGBCape48F=1");
        mapSize(48, maxString, m_numStrings, args);
    }
    
    for (int x = 0; x < m_numStrings; x++) {
        if (x >= m_strings.size() || m_strings[x]->m_outputChannels == 0) {
            std::string v = "-DNOOUT";
            v += std::to_string(x+1);
            args.push_back(v);
        }
    }
    createOutputLengths(m_strings, m_maxStringLen);
    
    if (m_maxStringLen < 1000) {
        //if there is plenty of time to output the GPIO0 stuff
        //after the other GPIO's, let's do that
        args.push_back("-DSPLIT_GPIO0");
    }
    //set the total data length (bytes, pixels * 3)
    std::string v = "-DDATA_LEN=";
    v += std::to_string(m_maxStringLen);
    args.push_back(v);
    
    compilePRUCode(args);
    m_pruProgram = "/tmp/FalconWS281x.bin";
    if (!StartPRU()) {
        return 0;
    }
    m_frameSize = m_maxStringLen * m_numStrings;
    m_lastData = (uint8_t*)calloc(1, m_frameSize);
    m_curData = (uint8_t*)calloc(1, m_frameSize);

    for (int x = 0; x < MAX_WS2811_TIMINGS; x++) {
        m_pruData->timings[x] = 0;
    }
    return retVal;
}

int BBB48StringOutput::StartPRU()
{
    m_curFrame = 0;
    int pruNumber = BBB_PRU;
    
    m_pru = new BBBPru(pruNumber, true, true);
    m_pruData = (BBB48StringData*)m_pru->data_ram;
    m_pruData->command = 0;
    m_pruData->address_dma = m_pru->ddr_addr;
    m_pru->run(m_pruProgram);
    return 1;
}
void BBB48StringOutput::StopPRU(bool wait)
{
    // Send the stop command
    m_pruData->command = 0xFF;
    
    if (wait) {
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    m_pru->stop(!wait);
    delete m_pru;
    m_pru = NULL;
}
/*
 *
 */
int BBB48StringOutput::Close(void)
{
    LogDebug(VB_CHANNELOUT, "BBB48StringOutput::Close()\n");
    StopPRU();
    return ChannelOutputBase::Close();
}


void BBB48StringOutput::GetRequiredChannelRange(int &min, int & max) {
    min = FPPD_MAX_CHANNELS;
    max = 0;
    
    PixelString *ps = NULL;
    for (int s = 0; s < m_strings.size(); s++) {
        ps = m_strings[s];
        int inCh = 0;
        for (int p = 0; p < ps->m_outputChannels; p++) {
            int ch = ps->m_outputMap[inCh++];
            if (ch < (FPPD_MAX_CHANNELS - 3)) {
                min = std::min(min, ch);
                max = std::max(max, ch);
            }
        }
    }
}

/*
 *
 */
void BBB48StringOutput::PrepData(unsigned char *channelData)
{
    LogExcess(VB_CHANNELOUT, "BBB48StringOutput::PrepData(%p)\n",
              channelData);

    m_curFrame++;

    
#ifdef PRINT_STATS
    int max = 0;
    for (int x = 0; x < MAX_WS2811_TIMINGS; x++) {
        if (max < m_pruData->timings[x]) {
            max = m_pruData->timings[x];
        }
    }
    if (max > 300 || (m_curFrame % 10) == 1) {
        for (int x = 0; x < MAX_WS2811_TIMINGS; ) {
            printf("%8X ", m_pruData->timings[x]);
            ++x;
            if ((x) % 16 == 0) {
                printf("\n");
            }
        }
        printf("\n%d:  max %d\n", m_curFrame,  max);
    }
#endif
    uint8_t * out = m_curData;

    PixelString *ps = NULL;
    uint8_t *c = NULL;
    int inCh;

    int numStrings = m_numStrings;

    for (int s = 0; s < m_strings.size(); s++) {
        ps = m_strings[s];
        c = out + ps->m_portNumber;
        inCh = 0;
        
        for (int p = 0; p < ps->m_outputChannels; p++) {
            uint8_t *brightness = ps->m_brightnessMaps[p];
            *c = brightness[channelData[ps->m_outputMap[inCh++]]];
            c += numStrings;
        }
    }
}
int BBB48StringOutput::RawSendData(unsigned char *channelData)
{
    LogExcess(VB_CHANNELOUT, "BBB48StringOutput::RawSendData(%p)\n",
              channelData);

    if (m_pruData->command) {
        // Wait for the previous draw to finish
        std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float, std::milli> duration = std::chrono::high_resolution_clock::now() - t1;
        while (m_pruData->command && duration.count() < 22) {
            pthread_yield();
            duration = std::chrono::high_resolution_clock::now() - t1;
        }
        if (m_pruData->command) {
            m_stallCount++;
            return m_channelCount;
        }
        if (m_stallCount == 10) {
            LogWarn(VB_CHANNELOUT, "BBB Pru Stalled, restarting PRU\n");
            StopPRU(false);
            StartPRU();
            m_stallCount = 0;
        }
    } else {
        m_stallCount = 0;
    }

    unsigned frame = 0;
    //unsigned frame = m_curFrame & 1;
    if (m_curFrame == 1 || memcmp(m_lastData, m_curData, m_frameSize)) {
        //don't copy to DMA memory unless really needed to avoid bus contention on the DMA bus

        //copy first 7.5K into PRU mem directly
        int fullsize = m_frameSize;
        int mx = m_frameSize;
        if (mx > (8*1024 - 512)) {
            mx = 8*1024 - 512;
        }
        
        //first 7.5K to main PRU ram
        memcpy(m_pru->data_ram + 512, m_curData, mx);
        fullsize -= 7628;
        if (fullsize > 0) {
            int outsize = fullsize;
            if (outsize > (8*1024 - 512)) {
                outsize = 8*1024 - 512;
            }
            // second 7.5K to other PRU ram
            memcpy(m_pru->other_data_ram + 512, m_curData + 7628, outsize);
            fullsize -= outsize;
        }
        if (fullsize > 0) {
            int outsize = fullsize;
            if (outsize > (12*1024)) {
                outsize = 12*1024;
            }
            memcpy(m_pru->shared_ram, m_curData + 7628 + 7628, outsize);
        }
        int off = 7628 * 2 + 12188;
        if (off < m_frameSize) {
            // more than what fits in the SRAMs
            //don't need to copy the first part as that's in sram, just copy the last parts
            off -= 100;
            uint8_t * const realout = (uint8_t *)m_pru->ddr + m_frameSize * frame + off;
            memcpy(realout, m_curData + off, m_frameSize - off);
        }
        
        std::swap(m_lastData, m_curData);
    }
    
    // Map
    m_pruData->address_dma = m_pru->ddr_addr + m_frameSize * frame;

    // Send the start command
    m_pruData->command = 1;

    return m_channelCount;
}

/*
 *
 */
void BBB48StringOutput::ApplyPinMap(const int *map)
{
    int origPortNumber = 0;

    for (int i = 0; i < m_strings.size(); i++) {
        origPortNumber = m_strings[i]->m_portNumber;
        m_strings[i]->m_portNumber = map[origPortNumber];
    }
}

/*
 *
 */
int BBB48StringOutput::MapPins(void)
{
    if (m_subType == "RGBCape48C") {
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
    
    LogDebug(VB_CHANNELOUT, "    type          : %s\n", m_subType.c_str());
    LogDebug(VB_CHANNELOUT, "    strings       : %d\n", m_strings.size());
    LogDebug(VB_CHANNELOUT, "    longest string: %d channels\n", m_maxStringLen);

    for (int i = 0; i < m_strings.size(); i++) {
        LogDebug(VB_CHANNELOUT, "    string #%02d\n", i);
        m_strings[i]->DumpConfig();
    }

    ChannelOutputBase::DumpConfig();
}

