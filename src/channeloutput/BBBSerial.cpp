/*
 *   BeagleBone Black PRU Serial DMX/Pixelnet handler for Falcon Player (FPP)
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
#include <fstream>
#include <thread>
#include <chrono>

#define BBB_PRU  0
//  #define USING_PRU_RAM

#include <pruss_intc_mapping.h>
#include <prussdrv.h>

// FPP includes
#include "common.h"
#include "log.h"
#include "BBBSerial.h"
#include "settings.h"

#include "util/BBBUtils.h"


//reserve the TOP 84K for DMX/PixelNet data
#define DDR_RESERVED 84*1024


extern "C" {
    BBBSerialOutput *createOutputBBBSerial(unsigned int startChannel,
                                  unsigned int channelCount) {
        return new BBBSerialOutput(startChannel, channelCount);
    }
}


/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
BBBSerialOutput::BBBSerialOutput(unsigned int startChannel,
	unsigned int channelCount)
    : ThreadedChannelOutputBase(startChannel, channelCount),
    m_pixelnet(0),
    m_lastData(NULL),
    m_curData(NULL),
    m_curFrame(0),
    m_pru(NULL),
    m_serialData(NULL)
{
    LogDebug(VB_CHANNELOUT, "BBBSerialOutput::BBBSerialOutput(%u, %u)\n",
            startChannel, channelCount);
}

/*
 *
 */
BBBSerialOutput::~BBBSerialOutput()
{
    LogDebug(VB_CHANNELOUT, "BBBSerialOutput::~BBBSerialOutput()\n");
    
    if (m_lastData) free(m_lastData);
    if (m_curData) free(m_curData);
    if (m_pru) delete m_pru;
}


static void compileSerialPRUCode(std::vector<std::string> &sargs) {

    pid_t compilePid = fork();
    if (compilePid == 0) {
        std::string log;
        char * args[sargs.size() + 3];
        args[0] = (char *)"/bin/bash";
        args[1] = (char *)"/opt/fpp/src/pru/compileSerial.sh";

        log = args[1];
        
        for (int x = 0; x < sargs.size(); x++) {
            args[x + 2] = (char*)sargs[x].c_str();
            log += " " + sargs[x];
        }
        args[sargs.size() + 2] = NULL;
        LogDebug(VB_CHANNELOUT, "BBBSerial::compilePRUCode() args: %s\n", log.c_str());

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

    m_startChannels.resize(m_outputs);

    // Initialize the ouputs
    for (int i = 0; i < m_outputs; i++) {
        m_startChannels[i] = -1;
    }

    int maxChannel = 0;
    int maxLen = 0;
    for (int i = 0; i < config["outputs"].size(); i++) {
        Json::Value s = config["outputs"][i];

        m_startChannels[s["outputNumber"].asInt()] = s["startChannel"].asInt() - 1;
        int l = s["channelCount"].asInt();
        if (l > maxLen) {
            maxLen = l;
        }
    }

    m_channelCount = 0;
    for (int i = 0; i < m_outputs; i++) {
        if (m_startChannels[i] >= 0 && m_channelCount < m_startChannels[i]) {
            m_channelCount = m_startChannels[i];
        }
    }
    m_channelCount += (m_pixelnet ? 4096 : 512);
    m_channelCount -= m_startChannel;
    
    m_channelCount = config["channelCount"].asInt();

    int pruNumber = BBB_PRU;

    string pru_program = "/tmp/FalconSerial.bin";

    const char *mode = BBB_PRU ? "gpio" : "pruout";
    if (BBB_PRU) {
        args.push_back("-DRUNNING_ON_PRU1");
    } else {
        args.push_back("-DRUNNING_ON_PRU0");
    }
    
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
    Json::Value root;
    char filename[256];
    std::string device = config["device"].asString();
    sprintf(filename, "/home/fpp/media/tmp/strings/%s%s.json", device.c_str(), verPostf.c_str());
    if (!FileExists(filename)) {
        sprintf(filename, "/opt/fpp/capes/%s/strings/%s%s.json", dirname.c_str(), device.c_str(), verPostf.c_str());
    }
    int maxOut = 8;
    int countOut = 0;
    if (FileExists(filename)) {
        if (!LoadJsonFromFile(filename, root)) {
            LogErr(VB_CHANNELOUT, "Could not read pin configuration for %s%s\n", device.c_str(), verPostf.c_str());
            return 0;
        }
        std::ofstream outputFile;
        outputFile.open("/tmp/SerialPinConfiguration.hp", std::ofstream::out | std::ofstream::trunc);

        
        maxOut = root["serial"].size();
        for (int x = 0; x < root["serial"].size(); x++)  {
            const PinCapabilities &pin = PinCapabilities::getPinByName(root["serial"][x]["pin"].asString());
            if (m_startChannels[x] >= 0) {
                pin.configPin(mode);
                countOut++;
            }
            outputFile << "#define ser" << std::to_string(x + 1) << "_gpio  " << std::to_string(pin.gpioIdx) << "\n";
            outputFile << "#define ser" << std::to_string(x + 1) << "_pin  " << std::to_string(pin.gpio) << "\n\n";
            outputFile << "#define ser" << std::to_string(x + 1) << "_pru30  " << std::to_string(pin.pruPin) << "\n\n";
        }
        
        outputFile.close();
    } else {
        LogErr(VB_CHANNELOUT, "No output pin configuration for %s%s\n", device.c_str(), verPostf.c_str());
        return 0;
    }
    
    for (int xx = maxOut; xx < m_outputs; xx++) {
        m_startChannels[xx] = -1;
    }
    
    if (!countOut) {
        LogInfo(VB_CHANNELOUT, "No Serial Ouput Data needed\n");
        return 1;
    }
    char buf[256];
    if (!m_pixelnet) {
        if (maxLen < 1 || maxLen > 512) {
            maxLen = 512;
        }
        sprintf(buf,"-DDATALEN=%d", (maxLen + 1));
        args.push_back(buf);
    }
    sprintf(buf,"-DNUMOUT=%d", maxOut);
    args.push_back(buf);

    compileSerialPRUCode(args);
    if (!FileExists(pru_program.c_str())) {
        LogErr(VB_CHANNELOUT, "%s does not exist!\n", pru_program.c_str());
        return 0;
    }
    LogDebug(VB_CHANNELOUT, "Using program %s\n", pru_program.c_str());
    
    m_pru = new BBBPru(BBB_PRU);
    m_serialData = (BBBSerialData*)m_pru->data_ram;
    size_t offset = m_pru->ddr_size - DDR_RESERVED;
    m_serialData->address_dma = m_pru->ddr_addr + offset;
    m_serialData->command = 0;
    m_serialData->response = 0;
    m_pru->run(pru_program);

    int sz = m_pixelnet ? (4096 + 6): (512 + 1);
    
    m_lastData = (uint8_t*)malloc(m_outputs * sz);
    m_curData = (uint8_t*)malloc(m_outputs * sz);
    
    for (int i = 0; i < m_outputs; i++) {
        if (m_pixelnet) {
            m_curData[i + (0 * m_outputs)] = '\xAA';
            m_curData[i + (1 * m_outputs)] = '\x55';
            m_curData[i + (2 * m_outputs)] = '\x55';
            m_curData[i + (3 * m_outputs)] = '\xAA';
            m_curData[i + (4 * m_outputs)] = '\x15';
            m_curData[i + (5 * m_outputs)] = '\x5D';
        } else {
            m_curData[i] = '\x00';
        }
    }
    memcpy(m_lastData, m_curData, m_outputs * sz);
    return ThreadedChannelOutputBase::Init(config);
}

void BBBSerialOutput::GetRequiredChannelRanges(const std::function<void(int, int)> &addRange) {
    for (int i = 0; i < m_outputs; i++) {
        if (m_startChannels[i] >= 0) {
            addRange(m_startChannels[i] + 1, m_pixelnet ? m_startChannels[i] + 4095 : m_startChannels[i] + 511);
        }
    }
}

/*
 *
 */
int BBBSerialOutput::Close(void)
{
    LogDebug(VB_CHANNELOUT, "BBBSerialOutput::Close()\n");

    if (m_serialData && m_pru) {
        // Send the stop command
        m_serialData->response = 0;
        m_serialData->command = 0xFF;
        
        __asm__ __volatile__("":::"memory");
        int cnt = 0;
        while (wait && cnt < 25 && m_serialData->response != 0xFFFF) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            cnt++;
        }
        m_pru->stop(m_serialData->response != 0xFFFF ? 0 : 1);
        
        delete m_pru;
        m_pru = NULL;
    }
    
    LogDebug(VB_CHANNELOUT, "BBBSerialOutput::Close() done\n");
    return ThreadedChannelOutputBase::Close();
}

/*
 *
 */
int BBBSerialOutput::RawSendData(unsigned char *channelData)
{
    LogExcess(VB_CHANNELOUT, "BBBSerialOutput::RawSendData(%p)\n",
            channelData);
    
    if (!m_serialData) {
        return m_channelCount;
    }

     m_curFrame++;
    
    // Bypass LEDscape draw routine and format data for PRU ourselves
    
    uint8_t * const out = m_curData;
    uint8_t *c = out;
    uint8_t *s = (uint8_t*)channelData;
    int chCount = m_pixelnet ? 4096 : 512;

    for (int i = 0; i < m_outputs; i++) {
        if (m_startChannels[i] < 0) {
            continue;
        }
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
        for (int ch = 0; ch < chCount; ch++) {
            *c = *s;
            s++;
            c += m_outputs;
        }
    }

    // Wait for the previous draw to finish
    while (m_serialData->command);

    int frame_size = m_pixelnet ? (4096 + 6): (512 + 1);
    frame_size *= m_outputs;
    
    unsigned frame = 0;
    //unsigned frame = m_curFrame & 1;
    if (m_curFrame == 1 || memcmp(m_lastData, m_curData, frame_size)) {
        //don't copy to DMA memory unless really needed to avoid bus contention on the DMA bus
        int sz = m_pixelnet ? (4096 + 6): (512 + 1);
        sz *= m_outputs;
#ifdef USING_PRU_RAM
        uint8_t * const realout = (uint8_t *)m_pru->data_ram + 512;
        memcpy(realout, m_curData, sz);
#else
        size_t offset = m_pru->ddr_size - DDR_RESERVED;
        uint8_t * const realout = (uint8_t *)m_pru->ddr + offset;
        memcpy(realout, m_curData, sz);

        m_serialData->address_dma = m_pru->ddr_addr + offset;
#endif
        
        uint8_t *tmp = m_lastData;
        m_lastData = m_curData;
        m_curData = tmp;
    }

    //make sure memory is flushed before command is set to 1
    __asm__ __volatile__("":::"memory");

    // Send the start command
    m_serialData->command = 1;

    return m_channelCount;
}

/*
 *
 */
void BBBSerialOutput::DumpConfig(void)
{
    LogDebug(VB_CHANNELOUT, "BBBSerialOutput::DumpConfig()\n");

    LogDebug(VB_CHANNELOUT, "    Pixelnet      : %d\n", m_pixelnet);
    LogDebug(VB_CHANNELOUT, "    Outputs       :\n" );

    for (int i = 0; i < m_outputs; i++) {
        LogDebug(VB_CHANNELOUT, "        #%d: %d-%d (%d Ch)\n",
                 i + 1,
                 m_startChannels[i] + 1,
                 m_pixelnet ? m_startChannels[i] + 4096 : m_startChannels[i] + 512,
                 m_pixelnet ? 4096 : 512);
    }

    ThreadedChannelOutputBase::DumpConfig();
}


