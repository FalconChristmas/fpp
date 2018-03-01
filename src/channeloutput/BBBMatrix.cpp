/*
 *   BeagleBone Black PRU Matrix handler for Falcon Player (FPP)
 *
 *   Copyright (C) 2018 the Falcon Player Developers
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

#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "BBBMatrix.h"
#include "BBBUtils.h"
#include "common.h"
#include "log.h"

//run on PRU 0
#define BBB_PRU  0
#define BBB_COPY_PRU  1


// These are the number of clock cycles it takes to clock out a single "row" of bits (1 bit) for 32x16 1/8 P10 scan panels.  Other
// panel types and scan rates and stuff are proportional to these
uint32_t v1Timings[8][16] = {
    { 0xA65, 0x14EA, 0x1F6F, 0x29ED, 0x346E, 0x3EEE, 0x496C, 0x53EB, 0x5E6B, 0x68ED, 0x7374, 0x7DF0, 0x887B, 0x92F4, 0x9D75, 0xA7FB},
    { 0xA69, 0x14EA, 0x1F6F, 0x29ED, 0x346E, 0x3EEE, 0x496C, 0x53EB, 0x5E6B, 0x68ED, 0x7374, 0x7DF0, 0x887B, 0x92F4, 0x9D75, 0xA7FB},
    { 0xA6A, 0x14EA, 0x1F6F, 0x29ED, 0x346E, 0x3EEE, 0x496C, 0x53EB, 0x5E6B, 0x68ED, 0x7374, 0x7DF0, 0x887B, 0x92F4, 0x9D75, 0xA7FB},
    { 0xC88, 0x1906, 0x2588, 0x320D, 0x3E8D, 0x4B10, 0x5790, 0x6410, 0x7090, 0x7D10, 0x8990, 0x9610, 0xA291, 0xAF0D, 0xBB90, 0xC80D},
    { 0xC88, 0x1906, 0x2588, 0x320D, 0x3E8D, 0x4B10, 0x5790, 0x6410, 0x7090, 0x7D10, 0x8990, 0x9610, 0xA291, 0xAF0D, 0xBB90, 0xC80D},
    { 0xC88, 0x1906, 0x2588, 0x320D, 0x3E8D, 0x4B10, 0x5790, 0x6410, 0x7090, 0x7D10, 0x8990, 0x9610, 0xA291, 0xAF0D, 0xBB90, 0xC80D},
    { 0xC88, 0x1906, 0x2588, 0x320D, 0x3E8D, 0x4B10, 0x5790, 0x6410, 0x7090, 0x7D10, 0x8990, 0x9610, 0xA291, 0xAF0D, 0xBB90, 0xC80D},
    { 0xC90, 0x190D, 0x2590, 0x320D, 0x3E8D, 0x4B10, 0x5790, 0x6410, 0x7090, 0x7D10, 0x8990, 0x9610, 0xA291, 0xAF0D, 0xBB90, 0xC80D}
};
uint32_t v2Timings[8][16] = {
    { 0x347,  0x64F,  0x95F,  0xC70,  0xF80, 0x129E, 0x15B3, 0x18CB, 0x1BE4, 0x1EFB, 0x2211, 0x252C, 0x2786, 0x2A88, 0x2D85, 0x3088},
    { 0x864, 0x10ED, 0x196C, 0x21EA, 0x2A6C, 0x32E9, 0x3B6A, 0x43EC, 0x4C6A, 0x54EA, 0x5D6B, 0x65EB, 0x6E6D, 0x76ED, 0x7F6E, 0x87EE},
    { 0x864, 0x10ED, 0x196C, 0x21EA, 0x2A6C, 0x32E9, 0x3B6A, 0x43EC, 0x4C6A, 0x54EA, 0x5D6B, 0x65EB, 0x6E6D, 0x76ED, 0x7F6E, 0x87EE},
    { 0xA69, 0x14EA, 0x1F6F, 0x29ED, 0x346E, 0x3EEE, 0x496C, 0x53EB, 0x5E6B, 0x68ED, 0x7374, 0x7DF0, 0x887B, 0x92F4, 0x9D75, 0xA7FB},
    { 0xA6A, 0x14EA, 0x1F6F, 0x29ED, 0x346E, 0x3EEE, 0x496C, 0x53EB, 0x5E6B, 0x68ED, 0x7374, 0x7DF0, 0x887B, 0x92F4, 0x9D75, 0xA7FB},
    { 0xC88, 0x1906, 0x2588, 0x320D, 0x3E8D, 0x4B10, 0x5790, 0x6410, 0x7090, 0x7D10, 0x8990, 0x9610, 0xA291, 0xAF0D, 0xBB90, 0xC80D},
    { 0xC88, 0x1906, 0x2588, 0x320D, 0x3E8D, 0x4B10, 0x5790, 0x6410, 0x7090, 0x7D10, 0x8990, 0x9610, 0xA291, 0xAF0D, 0xBB90, 0xC80D},
    { 0xC88, 0x1906, 0x2588, 0x320D, 0x3E8D, 0x4B10, 0x5790, 0x6410, 0x7090, 0x7D10, 0x8990, 0x9610, 0xA291, 0xAF0D, 0xBB90, 0xC80D},
};
uint32_t psTimings[6][16] = {
    { 0x440,  0x820,  0xC30, 0x1000, 0x1400, 0x17F1, 0x1C00, 0x2000, 0x2400, 0x2800, 0x2C00, 0x3000, 0x3400, 0x3800, 0x3C00, 0x4000},
    { 0x940, 0x12C0, 0x1C80, 0x25C0, 0x2F40, 0x3900, 0x4240, 0x4C00, 0x5540, 0x5EC0, 0x6850, 0x71C0, 0x7B40, 0x84C0, 0x8E80, 0x97CE},
    { 0xB70, 0x1700, 0x2280, 0x2E00, 0x3A00, 0x4500, 0x5080, 0x5C00, 0x6780, 0x7320, 0x7E80, 0x8A00, 0x9580, 0xA100, 0xAC80, 0xB7F0},
    { 0xB80, 0x1700, 0x2280, 0x2E00, 0x3A00, 0x4500, 0x5080, 0x5C00, 0x6780, 0x7320, 0x7E80, 0x8A00, 0x9580, 0xA100, 0xAC80, 0xB800},
    { 0xB90, 0x1700, 0x2280, 0x2E00, 0x3A00, 0x4500, 0x5080, 0x5C00, 0x6780, 0x7320, 0x7E80, 0x8A00, 0x9580, 0xA100, 0xAC80, 0xB800},
    { 0xB90, 0x1700, 0x2280, 0x2E00, 0x3A00, 0x4500, 0x5080, 0x5C00, 0x6780, 0x7320, 0x7E80, 0x8A00, 0x9580, 0xA100, 0xAC80, 0xB800},
};


static void compilePRUMatrixCode(std::vector<std::string> &sargs) {
    pid_t compilePid = fork();
    if (compilePid == 0) {
        char * args[sargs.size() + 3];
        args[0] = "/bin/bash";
        args[1] = "/opt/fpp/src/pru/compileMatrix.sh";
        
        for (int x = 0; x < sargs.size(); x++) {
            args[x + 2] = (char*)sargs[x].c_str();
        }
        args[sargs.size() + 2] = NULL;
        
        execvp("/bin/bash", args);
    } else {
        wait(NULL);
    }
}

static void configurePSPins() {
    configBBBPin("P1_29", 3, 21, "pruout");  //OE
    configBBBPin("P1_36", 3, 14, "pruout");  //LATCH
    configBBBPin("P1_33", 3, 15, "gpio");    //CLOCK
    configBBBPin("P2_32", 3, 16, "pruout");  //SEL0
    configBBBPin("P2_30", 3, 17, "pruout");  //SEL1
    configBBBPin("P1_31", 3, 18, "pruout");  //SEL2
    configBBBPin("P2_34", 3, 19, "pruout");  //SEL3
}
static void resetPSPins() {
    configBBBPin("P1_29", 3, 21, "gpio");  //OE
    configBBBPin("P1_36", 3, 14, "gpio");  //LATCH
    configBBBPin("P1_33", 3, 15, "gpio");  //CLOCK
    configBBBPin("P2_32", 3, 16, "gpio");  //SEL0
    configBBBPin("P2_30", 3, 17, "gpio");  //SEL1
    configBBBPin("P1_31", 3, 18, "gpio");  //SEL2
    configBBBPin("P2_34", 3, 19, "gpio");  //SEL3
}
void BBBMatrix::calcBrightnessFlags(std::vector<std::string> &sargs) {
    
    LogDebug(VB_CHANNELOUT, "Calc Brightness:   maxPanel:  %d    maxOutput: %d     Brightness: %d    rpo: %d    ph:  %d    pw:  %d\n", m_longestChain, m_outputs, m_brightness, m_panelScan, m_panelHeight, m_panelWidth);
    
    
    uint32_t max = 0xB00;
    if (m_pinout == BBBMatrix::POCKETSCROLLERv1) {
        max = psTimings[m_outputs-1][m_longestChain-1];
    } else if (m_pinout == BBBMatrix::V1) {
        max = v1Timings[m_outputs-1][m_longestChain-1];
    } else if (m_pinout == BBBMatrix::V2) {
        max = v2Timings[m_outputs-1][m_longestChain-1];
    }
    
    //timings are based on 32 pixel wide panels
    max *= m_panelWidth;
    max /= 32;
    
    // 1/4 scan we need to double the time since we have twice the number of pixels to clock out
    max *= m_panelHeight;
    max /= (m_panelScan * 2);

    if (max < 0x4000) {
        //boost up a bit more
        max *= 2;
    }
    
    uint32_t origMax = max;
    if (max < 0x3500) {
        //if max is too low, the low bit time is too short and
        //extra ghosting occurs
        // At this point, framerate will be supper high anyway >100fps
        max = 0x3500;
    }
    uint32_t origMax2 = max;

    max *= m_brightness;
    max /= 10;

    uint32_t delay = origMax2 - max;
    if (max < origMax) {
        delay = origMax2 - origMax;
    }
    
    for (int x = 0; x < 8; x++) {
        LogDebug(VB_CHANNELOUT, "Brightness %d:  %X\n", x, max);
        delayValues[x] = delay;
        brightnessValues[x] = max + delay;
        max >>= 1;
        origMax >>= 1;
    }
    
    m_printStats = false;
    if (FileExists("/home/fpp/media/config/ledscape_dimming")) {
        FILE *file = fopen("/home/fpp/media/config/ledscape_dimming", "r");
        
        if (file != NULL) {
            char buf[100];
            char *line = buf;
            size_t len = 100;
            ssize_t read;
            int count = 0;
            
            while (((read = getline(&line, &len, file)) != -1) && (count < 10))
            {
                if (( ! line ) || ( ! read ) || ( read == 1 ))
                continue;
                
                LogDebug(VB_CHANNELOUT, "Line %d: %s\n", count, line);
                if (count == 0) {
                    m_printStats = atoi(line);
                    count++;
                } else {
                    uint32_t d1, d2;
                    sscanf(line, "%X %X", &d1, &d2);
                    brightnessValues[count - 1] = d1;
                    delayValues[count - 1] = d2;
                    count++;
                }
                line = buf;
                len = 100;
            }
            fclose(file);
        }
    }
    
    for (int x = 0; x < m_colorDepth; x++) {
        char buf[100];
        sprintf(buf, "-DBRIGHTNESS%d=%d", (m_colorDepth-x), brightnessValues[x]);
        sargs.push_back(buf);
        sprintf(buf, "-DDELAY%d=%d", (m_colorDepth-x), delayValues[x]);
        sargs.push_back(buf);
    }
}



BBBMatrix::BBBMatrix(unsigned int startChannel, unsigned int channelCount)
  : ChannelOutputBase(startChannel, channelCount),
    m_pru(nullptr),
    m_pruCopy(nullptr),
	m_matrix(nullptr),
	m_panelMatrix(nullptr),
    m_outputs(0),
    m_outputFrame(nullptr),
    m_tmpFrame(nullptr),
    m_longestChain(0),
    m_panelWidth(32),
    m_panelHeight(16),
    m_invertedData(0),
    m_brightness(10),
    m_colorDepth(8),
    m_interleave(0),
    m_panelScan(8),
    m_pinout(V1),
    m_printStats(false)
{
	LogDebug(VB_CHANNELOUT, "BBBMatrix::BBBMatrix(%u, %u)\n",
		startChannel, channelCount);
}

BBBMatrix::~BBBMatrix()
{
    LogDebug(VB_CHANNELOUT, "BBBMatrix::~BBBMatrix()\n");
    if (m_outputFrame) delete [] m_outputFrame;
    if (m_tmpFrame) delete [] m_tmpFrame;
    if (m_pru) delete m_pru;
    if (m_pruCopy) delete m_pruCopy;
}

int BBBMatrix::Init(Json::Value config)
{
    LogDebug(VB_CHANNELOUT, "BBBMatrix::Init(JSON)\n");
    
    m_panelWidth  = config["panelWidth"].asInt();
    m_panelHeight = config["panelHeight"].asInt();
    if (!m_panelWidth)
        m_panelWidth = 32;
    
    if (!m_panelHeight)
        m_panelHeight = 16;

    m_invertedData = config["invertedData"].asInt();
    m_colorOrder = ColorOrderFromString(config["colorOrder"].asString());

    m_panelMatrix = new PanelMatrix(m_panelWidth, m_panelHeight, m_invertedData);
    if (!m_panelMatrix) {
        LogErr(VB_CHANNELOUT, "BBBMatrix: Unable to create PanelMatrix\n");
        return 0;
    }
    for (int i = 0; i < config["panels"].size(); i++) {
        Json::Value p = config["panels"][i];
        char orientation = 'N';
        const char *o = p["orientation"].asString().c_str();
        
        if (o && *o)
            orientation = o[0];
        
        if (p["colorOrder"].asString() == "")
            p["colorOrder"] = ColorOrderToString(m_colorOrder);
        
        m_panelMatrix->AddPanel(p["outputNumber"].asInt(),
                                p["panelNumber"].asInt(), orientation,
                                p["xOffset"].asInt(), p["yOffset"].asInt(),
                                ColorOrderFromString(p["colorOrder"].asString()));
        
        if (p["outputNumber"].asInt() > m_outputs)
            m_outputs = p["outputNumber"].asInt();
        
        if (p["panelNumber"].asInt() > m_longestChain)
            m_longestChain = p["panelNumber"].asInt();
    }
    // Both of these are 0-based, so bump them up by 1 for comparisons
    m_outputs++;
    m_longestChain++;
    
    //get the dimensions of the matrix
    m_panels = m_panelMatrix->PanelCount();
    m_rows = m_outputs * m_panelHeight;
    m_width  = m_panelMatrix->Width();
    m_height = m_panelMatrix->Height();

    if (config.isMember("brightness")) {
        m_brightness = config["brightness"].asInt();
    }
    if (m_brightness < 1 || m_brightness > 10) {
        m_brightness = 10;
    }
    if (config.isMember("panelColorDepth")) {
        m_colorDepth = config["panelColorDepth"].asInt();
    }
    if (m_colorDepth > 8 || m_colorDepth < 6) {
        m_colorDepth = 8;
    }
    if (config.isMember("panelInterleave")) {
        m_interleave = config["panelInterleave"].asInt();
    } else {
        m_interleave = 0;
    }
    m_panelScan = config["panelScan"].asInt();
    if (m_panelScan == 0) {
        // 1/8 scan by default
        m_panelScan = 8;
    }
    if (((m_panelScan * 2) != m_panelHeight) && m_interleave == 0) {
        m_interleave = 8;
    }
    
    m_channelCount = m_width * m_height * 3;

    m_matrix = new Matrix(m_startChannel, m_width, m_height);
    
    if (config.isMember("subMatrices")) {
        for (int i = 0; i < config["subMatrices"].size(); i++) {
            Json::Value sm = config["subMatrices"][i];
            
            m_matrix->AddSubMatrix(
                                   sm["enabled"].asInt(),
                                   sm["startChannel"].asInt() - 1,
                                   sm["width"].asInt(),
                                   sm["height"].asInt(),
                                   sm["xOffset"].asInt(),
                                   sm["yOffset"].asInt());
        }
    }
    
    m_rowSize = m_longestChain * m_panelWidth * 3;
    m_outputFrame = new uint8_t[m_outputs * m_longestChain * m_panelHeight * m_panelWidth * 3];
    m_tmpFrame = new uint8_t[m_outputs * m_longestChain * m_panelHeight * m_panelWidth * 3];

    std::vector<std::string> compileArgs;
    
    m_pinout = V1;
    if (config["wiringPinout"] == "v2") {
        m_pinout = V2;
        compileArgs.push_back("-DOCTO_V2");
    } else if (config["wiringPinout"] == "PocketScroller1x") {
        m_pinout = POCKETSCROLLERv1;
        compileArgs.push_back("-DPOCKETSCROLLER_V1");
        configurePSPins();
    } else {
        compileArgs.push_back("-DOCTO_V1");
    }
    char buf[200];
    sprintf(buf, "-DRUNNING_ON_PRU%d", BBB_PRU);
    compileArgs.push_back(buf);
    sprintf(buf, "-DOUTPUTS=%d", m_outputs);
    compileArgs.push_back(buf);
    sprintf(buf, "-DROWS=%d", m_panelScan);
    compileArgs.push_back(buf);
    sprintf(buf, "-DBITS=%d", m_colorDepth);
    compileArgs.push_back(buf);
    int tmp = m_longestChain * m_panelWidth / 8;
    if (m_panelScan * 4 == m_panelHeight) {
        tmp *= 2;
    } else if (m_panelScan * 8 == m_panelHeight) {
        tmp *= 4;
    }
    sprintf(buf, "-DROW_LEN=%d", tmp);
    compileArgs.push_back(buf);

    
    calcBrightnessFlags(compileArgs);
    if (m_printStats) {
        sprintf(buf, "-DENABLESTATS=1", m_outputs);
        compileArgs.push_back("-DENABLESTATS=1");
    }

    compilePRUMatrixCode(compileArgs);
    std::string pru_program = "/tmp/FalconMatrix.bin";

    m_pruCopy = new BBBPru(BBB_COPY_PRU);
    memset(m_pruCopy->data_ram, 0, 24);

    m_pru = new BBBPru(BBB_PRU);
    m_pruData = (BBBPruMatrixData*)m_pru->data_ram;
    m_pruData->address_dma = m_pru->ddr_addr;
    m_pruData->command = 0;
    m_pruData->response = 0;

    for (int x = 0; x < MAX_STATS; x++) {
        m_pruData->stats[x * 3] = 0;
        m_pruData->stats[x * 3 + 1] = 0;
        m_pruData->stats[x * 3 + 1] = 0;
    }
    
    m_pruCopy->run("/tmp/FalconMatrixPRUCpy.bin");
    m_pru->run(pru_program);
    
    return ChannelOutputBase::Init(config);
}

int BBBMatrix::Close(void)
{
    LogDebug(VB_CHANNELOUT, "BBBMatrix::Close()\n");
    
    if (m_pru) {
        m_pru->stop();
        delete m_pru;
        m_pru = nullptr;
    }
    
    if (m_pruCopy) {
        m_pruCopy->stop();
        delete m_pruCopy;
        m_pruCopy = nullptr;
    }
    
    return ChannelOutputBase::Close();
}

static inline uint8_t mapColor(uint8_t v, uint8_t colorDepth) {
    if (colorDepth == 6 && (v == 3 || v == 2)) {
        return 4;
    }
    if (colorDepth == 7 && v == 1) {
        return 2;
    }
    return v;
}

static int fcount = 0;


void BBBMatrix::printStats() {
    FILE *rfile;
    rfile=fopen("/tmp/framerates.txt","w");
    for (int x = 0; x < m_colorDepth; ++x) {
        fprintf(rfile, "DV: %d    %8X   %8X\n", (m_colorDepth-x), brightnessValues[x], delayValues[x]);
    }
    int off = 0;
    uint32_t total = 0;
    int count = 0;
    for (int x = 0; x < m_panelScan; ++x) {
        for (int y = m_colorDepth; y > 0; --y) {
            fprintf(rfile, "r%2d  b%2d:   %8X   %8X   %8X\n", x, y, m_pruData->stats[off], m_pruData->stats[off + 1], m_pruData->stats[off + 2]);
            total += m_pruData->stats[off];
            count++;
            off += 3;
        }
    }
    fprintf(rfile, "Average Per Row/Bit:   %8X\n", (total / count));
    //printf("0x%X\n", (total / count));
    fclose(rfile);
}


void BBBMatrix::PrepData(unsigned char *channelData)
{
    m_matrix->OverlaySubMatrices(channelData);
    
    fcount++;
    
    if (fcount == 20) {
        //every 20 frames or so, save stats
        fcount = 0;
        if (m_printStats) {
            printStats();
        }
    }

    channelData += m_startChannel; // FIXME, this function gets offset 0
    
    size_t rowLen = m_panelWidth * m_longestChain * m_outputs * 3 * 2;
    rowLen /= 8;
    
    memset(m_outputFrame, 0, m_outputs * m_longestChain * m_panelHeight * m_panelWidth * 3);
    for (int output = 0; output < m_outputs; output++) {
        int panelsOnOutput = m_panelMatrix->m_outputPanels[output].size();
        
        for (int i = 0; i < panelsOnOutput; i++) {
            int panel = m_panelMatrix->m_outputPanels[output][i];
            int chain = m_panelMatrix->m_panels[panel].chain;
            
            for (int y = 0; y < (m_panelHeight / 2); y++) {
                int yw1 = y * m_panelWidth * 3;
                int yw2 = (y + (m_panelHeight / 2)) * m_panelWidth * 3;

                int offset = y * rowLen * m_colorDepth + output * 2 * 3 + (m_longestChain - chain - 1) * m_panelWidth/8 * m_outputs * 3 * 2;
                
                for (int x = 0; x < m_panelWidth; ++x) {
                    uint8_t r1 = mapColor(channelData[m_panelMatrix->m_panels[panel].pixelMap[yw1 + x*3]], m_colorDepth);
                    uint8_t g1 = mapColor(channelData[m_panelMatrix->m_panels[panel].pixelMap[yw1 + x*3 + 1]], m_colorDepth);
                    uint8_t b1 = mapColor(channelData[m_panelMatrix->m_panels[panel].pixelMap[yw1 + x*3 + 2]], m_colorDepth);
                    
                    uint8_t r2 = mapColor(channelData[m_panelMatrix->m_panels[panel].pixelMap[yw2 + x*3]], m_colorDepth);
                    uint8_t g2 = mapColor(channelData[m_panelMatrix->m_panels[panel].pixelMap[yw2 + x*3 + 1]], m_colorDepth);
                    uint8_t b2 = mapColor(channelData[m_panelMatrix->m_panels[panel].pixelMap[yw2 + x*3 + 2]], m_colorDepth);

                    int bitPos = 1 << (x % 8);
                    int xOff = x / 8 * (m_outputs * 2 * 3);
                    
                    for (int bit = 8; bit > (8-m_colorDepth); ) {
                        --bit;
                        uint8_t mask = 1 << bit;
                        if (r1 & mask) {
                            m_outputFrame[offset + xOff] |= bitPos;
                        }
                        if (g1 & mask) {
                            m_outputFrame[offset + xOff + 1] |= bitPos;
                        }
                        if (b1 & mask) {
                            m_outputFrame[offset + xOff + 2] |= bitPos;
                        }
                        if (r2 & mask) {
                            m_outputFrame[offset + xOff + 3] |= bitPos;
                        }
                        if (g2 & mask) {
                            m_outputFrame[offset + xOff + 4] |= bitPos;
                        }
                        if (b2 & mask) {
                            m_outputFrame[offset + xOff + 5] |= bitPos;
                        }
                        xOff += rowLen;
                    }
                }
            }
        }
    }
    
    if ((m_panelScan * 2) != m_panelHeight) {
        //need to interleave the data, we'll do it by copying blocks to tmpFrame
        //as needed then swap.  This only supports interleaves that are divisible by 8
        //due to the bit packing
        int blockSize = m_interleave * m_outputs * 3 * 2 / 8;
        size_t totalSize = m_outputs * m_longestChain * m_panelHeight * m_panelWidth * 3;
        size_t fullRowSize = m_outputs * m_longestChain * m_panelWidth * 3 * 2 * m_colorDepth / 8;
        size_t offsetToNextBlock = fullRowSize * m_panelScan;
        
        //printf("%d  %d  %d  %d\n", blockSize, fullRowSize, offsetToNextBlock, totalSize);
        
        memset(m_tmpFrame, 0, totalSize);
        int curOut = 0;
        int curIn = 0;
        while (curOut < totalSize) {
            memcpy(&m_tmpFrame[curOut], &m_outputFrame[curIn + offsetToNextBlock], blockSize);
            curOut += blockSize;
            memcpy(&m_tmpFrame[curOut], &m_outputFrame[curIn], blockSize);
            curIn += blockSize;
            curOut += blockSize;
        }
        
        std::swap(m_tmpFrame, m_outputFrame);
    }
}
int BBBMatrix::RawSendData(unsigned char *channelData)
{
    LogExcess(VB_CHANNELOUT, "BBBMatrix::RawSendData(%p)\n", channelData);
    
    int len = m_outputs * m_longestChain * m_panelHeight * m_panelWidth * 3;
    if (len < (m_pru->ddr_size / 2)) {
        //we can flip frames
        memcpy(m_pru->ddr + (m_evenFrame ? (m_pru->ddr_size / 2) : 0), m_outputFrame, len);
        m_pruData->address_dma = m_pru->ddr_addr + (m_evenFrame ? (m_pru->ddr_size / 2) : 0);
        m_evenFrame = !m_evenFrame;
    } else {
        memcpy(m_pru->ddr, m_outputFrame, len);
        m_pruData->address_dma = m_pru->ddr_addr;
    }
    m_pruData->command = 1;
    
    /*
    if (fcount == 0) {
        printf("%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x \n",
           m_outputFrame[0], m_outputFrame[1], m_outputFrame[2],
           m_outputFrame[3], m_outputFrame[4], m_outputFrame[5],
           m_outputFrame[6], m_outputFrame[7], m_outputFrame[8],
           m_outputFrame[9], m_outputFrame[10], m_outputFrame[11],
           m_outputFrame[12], m_outputFrame[13], m_outputFrame[15]
           );
        
        uint32_t *t = (uint32_t *)m_pruCopy->data_ram;
        printf("     %d   %X   %X    %X %X %X\n", t[0], t[1], t[2], t[3], t[4], t[5]);
    }
    */
    
    return m_channelCount;
}


void BBBMatrix::DumpConfig(void)
{
    LogDebug(VB_CHANNELOUT, "BBBMatrix::DumpConfig()\n");
    
    LogDebug(VB_CHANNELOUT, "    Width          : %d\n", m_width);
    LogDebug(VB_CHANNELOUT, "    Height         : %d\n", m_height);
    LogDebug(VB_CHANNELOUT, "    Rows           : %d\n", m_rows);
    LogDebug(VB_CHANNELOUT, "    Row Size       : %d\n", m_rowSize);
    LogDebug(VB_CHANNELOUT, "    Color Depth    : %d\n", m_colorDepth);
    LogDebug(VB_CHANNELOUT, "    Outputs        : %d\n", m_outputs);
    LogDebug(VB_CHANNELOUT, "    Longest Chain  : %d\n", m_longestChain);
    LogDebug(VB_CHANNELOUT, "    Inverted Data  : %d\n", m_invertedData);
    
    ChannelOutputBase::DumpConfig();
}
