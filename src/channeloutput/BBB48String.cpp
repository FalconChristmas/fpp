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
#include <sstream>

#define BBB_PRU  1

//  #define PRINT_STATS


#include <pruss_intc_mapping.h>
#include <prussdrv.h>

// FPP includes
#include "common.h"
#include "log.h"
#include "BBB48String.h"
#include "settings.h"
#include "Sequence.h"

#include "util/BBBUtils.h"


extern "C" {
    BBB48StringOutput *createOutputBBB48String(unsigned int startChannel,
                            unsigned int channelCount) {
        return new BBB48StringOutput(startChannel, channelCount);
    }
}

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
    m_pru0(NULL),
    m_pru0Data(NULL),
    m_stallCount(0)
{
    LogDebug(VB_CHANNELOUT, "BBB48StringOutput::BBB48StringOutput(%u, %u)\n",
            startChannel, channelCount);
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
    if (m_pru0) delete m_pru0;
}


static void compilePRUCode(const char * program, const std::vector<std::string> &sargs, const std::vector<std::string> &args1) {
    std::string log;
    
    char * args[sargs.size() + 3 + args1.size()];
    int idx = 0;
    args[idx++] = (char *)"/bin/bash";
    args[idx++] = (char *)program;
    log = args[1];
    for (int x = 0; x < sargs.size(); x++) {
        args[idx++] = (char*)sargs[x].c_str();
        log += " " + sargs[x];
    }
    for (int x = 0; x < args1.size(); x++) {
        args[idx++] = (char*)args1[x].c_str();
        log += " " + args1[x];
    }
    args[idx] = NULL;
    LogDebug(VB_CHANNELOUT, "BBB48StringOutput::compilePRUCode() args: %s\n", log.c_str());
    
    pid_t compilePid = fork();
    if (compilePid == 0) {
        execvp("/bin/bash", args);
    } else {
        wait(NULL);
    }
}

static void compilePRUCode(const std::vector<std::string> &sargs,
                           const std::vector<std::string> &args0,
                           const std::vector<std::string> &args1,
                           bool split) {
    
    if (!split) {
        compilePRUCode("/opt/fpp/src/pru/compileWS2811x.sh", sargs, std::vector<std::string>());
    } else {
        compilePRUCode("/opt/fpp/src/pru/compileWS2811x.sh", sargs, args1);
        compilePRUCode("/opt/fpp/src/pru/compileWS2811x_gpio0.sh", sargs, args0);
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
                                int maxStringLen,
                                std::vector<std::string> &args) {
    
    std::ofstream outputFile;
    outputFile.open("/tmp/OutputLengths.hp", std::ofstream::out | std::ofstream::trunc);
    
    std::map<int, std::vector<GPIOCommand>> sizes;
    for (int x = 0; x < m_strings.size(); x++) {
        int pc = m_strings[x]->m_outputChannels;
        if (pc != 0) {
            for (auto &a : m_strings[x]->m_gpioCommands) {
                sizes[a.channelOffset].push_back(a);
            }
        }
    }
    
    auto i = sizes.begin();
    while (i != sizes.end()) {
        int min = i->first;
        outputFile << "\nCHECK_" << std::to_string(min) << ":\n";
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
            
            for (auto &cmd : i->second) {
                int y = cmd.port;
                std::string o = std::to_string(y + 1);
                if (cmd.type) {
                    outputFile << "        SET GPIO_MASK(o" << o << "_gpio), o" << o << "_pin\n";
                } else {
                    outputFile << "        CLR GPIO_MASK(o" << o << "_gpio), o" << o << "_pin\n";
                }
            }
            i++;
            int next = i->first;
            outputFile << "        LDI next_check, #CHECK_" << std::to_string(next) << "\n";
            outputFile << "    skip_" << std::to_string(min) << ":\n        RET\n";
            
        } else {
            outputFile << "    RET\n\n";
            i++;
        }
    }
    
    if (sizes.empty()) {
        args.push_back("-DFIRST_CHECK=NO_PIXELS_CHECK");
    } else {
        int sz = sizes.begin()->first;
        std::string v = "-DFIRST_CHECK=CHECK_";
        v += std::to_string(sz);
        args.push_back(v);
    }
    outputFile.close();
}

static int getMaxChannelsPerPort() {
    char buf[128] = {0};
    FILE *fd = fopen("/sys/firmware/devicetree/base/model", "r");
    if (fd) {
        fgets(buf, 127, fd);
        fclose(fd);
    }
    std::string model = buf;
    if (model == "TI AM335x PocketBeagle") {
        return 999999;
    }

    Json::Value root;
    if (LoadJsonFromFile("/home/fpp/media/tmp/cape-info.json", root)) {
        if (root["id"].asString() == "Unsupported") {
            return 600;
        }
    }

    return 999999;
}


/*
 *
 */
int BBB48StringOutput::Init(Json::Value config)
{
    LogDebug(VB_CHANNELOUT, "BBB48StringOutput::Init(JSON)\n");

    m_subType = config["subType"].asString();
    m_channelCount = config["channelCount"].asInt();

    int maxPerPort = getMaxChannelsPerPort();
    
    for (int i = 0; i < config["outputs"].size(); i++) {
        Json::Value s = config["outputs"][i];
        PixelString *newString = new PixelString(true);

        if (!newString->Init(s))
            return 0;

        if (newString->m_outputChannels > maxPerPort) {
            newString->m_outputChannels =  maxPerPort;
        }
        if (newString->m_outputChannels > m_maxStringLen) {
            m_maxStringLen = newString->m_outputChannels;
        }

        m_strings.push_back(newString);
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
    std::vector<std::string> split0args;
    split0args.push_back("-DRUNNING_ON_PRU0");
    std::vector<std::string> split1args;
    std::string dirname = "bbb";
    std::string verPostf = "";
    if (getBeagleBoneType() == PocketBeagle) {
        dirname = "pb";
    }
    if (config["pinoutVersion"].asString() == "2.x") {
        verPostf = "-v2";
    }
    if (config["pinoutVersion"].asString() == "3.x") {
        verPostf = "-v3";
    }

    bool hasSerial = true;
    if (config.isMember("serialInUse")) {
        hasSerial = config["serialInUse"].asBool();
    }
    Json::Value root;
    char filename[256];
    sprintf(filename, "/home/fpp/media/tmp/strings/%s%s.json", m_subType.c_str(), verPostf.c_str());
    if (!FileExists(filename)) {
        sprintf(filename, "/opt/fpp/capes/%s/strings/%s%s.json", dirname.c_str(), m_subType.c_str(), verPostf.c_str());
    }
    int maxGPIO0 = 0;
    int maxGPIO13 = 0;
    
    if (FileExists(filename)) {
        if (!LoadJsonFromFile(filename, root)) {
            LogErr(VB_CHANNELOUT, "Could not read pin configuration for %s%s\n", m_subType.c_str(), verPostf.c_str());
            return 0;
        }
        
        mapSize(root["outputs"].size(), maxString, m_numStrings, args);
        std::ofstream outputFile;
        outputFile.open("/tmp/PinConfiguration.hp", std::ofstream::out | std::ofstream::trunc);
        for (int x = 0; x < m_numStrings; x++) {
            if (x >= m_strings.size() || m_strings[x]->m_outputChannels == 0) {
                std::string v = "-DNOOUT";
                v += std::to_string(x+1);
                args.push_back(v);
            } else {
                //need to output this pin, configure it
                const PinCapabilities &pin = PinCapabilities::getPinByName(root["outputs"][x]["pin"].asString());
                pin.configPin();
                if (pin.gpioIdx == 0) {
                    maxGPIO0 = std::max(maxGPIO0, m_strings[x]->m_outputChannels);
                    std::string v = "-DNOOUT";
                    v += std::to_string(x+1);
                    split1args.push_back(v);
                } else {
                    maxGPIO13 = std::max(maxGPIO13, m_strings[x]->m_outputChannels);
                    std::string v = "-DNOOUT";
                    v += std::to_string(x+1);
                    split0args.push_back(v);
                }
                outputFile << "#define o" << std::to_string(x + 1) << "_gpio  " << std::to_string(pin.gpioIdx) << "\n";
                outputFile << "#define o" << std::to_string(x + 1) << "_pin  " << std::to_string(pin.gpio) << "\n\n";
            }
        }
        outputFile.close();
    } else {
        LogErr(VB_CHANNELOUT, "No output pin configuration for %s%s\n", m_subType.c_str(), verPostf.c_str());
        return 0;
    }
#ifdef PRINT_STATS
    args.push_back("-DRECORD_STATS");
#endif

    createOutputLengths(m_strings, m_maxStringLen, args);
    
    if (!maxGPIO0) {
        //no GPIO0 output so no need for the second PRU to be used
        hasSerial = true;
    }
    
    if (hasSerial && ((maxGPIO0 + maxGPIO13) < 2000)) {
        //if there is plenty of time to output the GPIO0 stuff
        //after the other GPIO's, let's do that
        args.push_back("-DSPLIT_GPIO0");
    }
    //set the total data length (bytes, pixels * 3)
    std::string v = "-DDATA_LEN=";
    v += std::to_string(m_maxStringLen);
    args.push_back(v);
    
    compilePRUCode(args, split0args, split1args, !hasSerial);
    if (!StartPRU(!hasSerial)) {
        return 0;
    }
    m_frameSize = m_maxStringLen * m_numStrings;
    m_lastData = (uint8_t*)calloc(1, m_frameSize);
    m_curData = (uint8_t*)calloc(1, m_frameSize);

    for (int x = 0; x < MAX_WS2811_TIMINGS; x++) {
        m_pruData->timings[x] = 0;
        if (m_pru0Data) m_pru0Data->timings[x] = 0;
    }
    return retVal;
}

int BBB48StringOutput::StartPRU(bool both)
{
    m_curFrame = 0;
    int pruNumber = BBB_PRU;
    
    m_pru = new BBBPru(pruNumber, true, true);
    m_pruData = (BBB48StringData*)m_pru->data_ram;
    m_pruData->command = 0;
    m_pruData->address_dma = m_pru->ddr_addr;
    m_pru->run("/tmp/FalconWS281x.bin");
    
    if (both) {
        m_pru0 = new BBBPru(!pruNumber, true, true);
        m_pru0Data = (BBB48StringData*)m_pru0->data_ram;
        m_pru0Data->command = 0;
        m_pru0Data->address_dma = m_pru0->ddr_addr;
        m_pru0->run("/tmp/FalconWS281x_gpio0.bin");
    }
    
    return 1;
}
void BBB48StringOutput::StopPRU(bool wait)
{
    // Send the stop command
    m_pruData->response = 0;
    m_pruData->command = 0xFF;
    if (m_pru0Data) {
        m_pru0Data->response = 0;
        m_pru0Data->command = 0xFF;
    }
    __asm__ __volatile__("":::"memory");
    
    int cnt = 0;
    while (wait && cnt < 25 && m_pruData->response != 0xFFFF) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        cnt++;
    }
    m_pru->stop(m_pruData->response != 0xFFFF ? !wait : 1);
    delete m_pru;
    
    if (m_pru0) {
        cnt = 0;
        while (wait && cnt < 25 && m_pru0Data->response != 0xFFFF) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            cnt++;
        }
        m_pru0->stop(m_pru0Data->response != 0xFFFF ? !wait : 1);
        delete m_pru0;
    }
    m_pru = NULL;
    m_pru0 = NULL;
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


void BBB48StringOutput::GetRequiredChannelRanges(const std::function<void(int, int)> &addRange) {
    PixelString *ps = NULL;
    for (int s = 0; s < m_strings.size(); s++) {
        ps = m_strings[s];
        int inCh = 0;
        int min = FPPD_MAX_CHANNELS;
        int max = -1;
        for (int p = 0; p < ps->m_outputChannels; p++) {
            int ch = ps->m_outputMap[inCh++];
            if (ch < FPPD_MAX_CHANNELS) {
                min = std::min(min, ch);
                max = std::max(max, ch);
            }
        }
        if (min < max) {
            addRange(min, max);
        }
    }
}

/*
 *
 */
void BBB48StringOutput::PrepData(unsigned char *channelData)
{
    LogExcess(VB_CHANNELOUT, "BBB48StringOutput::PrepData(%p)\n", channelData);

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
int BBB48StringOutput::SendData(unsigned char *channelData)
{
    LogExcess(VB_CHANNELOUT, "BBB48StringOutput::SendData(%p)\n",
              channelData);

    /*
    while this would be nice to do, reading from the pruData can take 15-20ms by itself due
    to memory transfer from the PRU.  Will likely need to move this to DDR if we want to 
    be able to do this

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
    */

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
            fullsize -= 7628;
        }
        if (fullsize > 0) {
            int outsize = fullsize;
            if (outsize > (12*1024)) {
                outsize = 12*1024;
            }
            memcpy(m_pru->shared_ram, m_curData + 7628 + 7628, outsize);
        }
        int off = 7628 * 2 + 12188 - 100;
        if (off < m_frameSize) {
            // more than what fits in the SRAMs
            //don't need to copy the first part as that's in sram, just copy the last parts
            uint8_t * const realout = (uint8_t *)m_pru->ddr + m_frameSize * frame + off;
            memcpy(realout, m_curData + off, m_frameSize - off);
        }
        
        std::swap(m_lastData, m_curData);
    }
    
    // Map
    m_pruData->address_dma = m_pru->ddr_addr + m_frameSize * frame;
    if (m_pru0Data) m_pru0Data->address_dma = m_pru->ddr_addr + m_frameSize * frame;
    
    //make sure memory is flushed before command is set to 1
    __asm__ __volatile__("":::"memory");
    
    // Send the start command
    m_pruData->command = 1;
    if (m_pru0Data) m_pru0Data->command = 1;
    
    return m_channelCount;
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

