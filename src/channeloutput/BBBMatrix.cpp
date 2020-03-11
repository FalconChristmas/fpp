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
#include <string>
#include <cmath>
#include <iostream>
#include <fstream>


#include "BBBMatrix.h"
#include "common.h"
#include "log.h"

#include "util/BBBUtils.h"

extern "C" {
    BBBMatrix *createOutputLEDscapeMatrix(unsigned int startChannel,
                                  unsigned int channelCount) {
        return new BBBMatrix(startChannel, channelCount);
    }
}

// These are the number of clock cycles it takes to clock out a single "row" of bits (1 bit) for 32x16 1/8 P10 scan panels.  Other
// panel types and scan rates and stuff are proportional to these
static const uint32_t v1Timings[8][16] = {
    { 0xA65, 0x14EA, 0x1F6F, 0x29ED, 0x346E, 0x3EEE, 0x496C, 0x53EB, 0x5E6B, 0x68ED, 0x7374, 0x7DF0, 0x887B, 0x92F4, 0x9D75, 0xA7FB},
    { 0xA69, 0x14EA, 0x1F6F, 0x29ED, 0x346E, 0x3EEE, 0x496C, 0x53EB, 0x5E6B, 0x68ED, 0x7374, 0x7DF0, 0x887B, 0x92F4, 0x9D75, 0xA7FB},
    { 0xA6A, 0x14EA, 0x1F6F, 0x29ED, 0x346E, 0x3EEE, 0x496C, 0x53EB, 0x5E6B, 0x68ED, 0x7374, 0x7DF0, 0x887B, 0x92F4, 0x9D75, 0xA7FB},
    { 0xC88, 0x1906, 0x2588, 0x340D, 0x3E8D, 0x4B10, 0x5790, 0x6410, 0x7090, 0x7D10, 0x8990, 0x9610, 0xA291, 0xAF0D, 0xBB90, 0xC80D},
    { 0xC88, 0x1906, 0x2588, 0x340D, 0x3E8D, 0x4B10, 0x5790, 0x6410, 0x7090, 0x7D10, 0x8990, 0x9610, 0xA291, 0xAF0D, 0xBB90, 0xC80D},
    { 0xC88, 0x1906, 0x2588, 0x340D, 0x3E8D, 0x4B10, 0x5790, 0x6410, 0x7090, 0x7D10, 0x8990, 0x9610, 0xA291, 0xAF0D, 0xBB90, 0xC80D},
    { 0xC88, 0x1906, 0x2588, 0x340D, 0x3E8D, 0x4B10, 0x5790, 0x6410, 0x7090, 0x7D10, 0x8990, 0x9610, 0xA291, 0xAF0D, 0xBB90, 0xC80D},
    { 0xC90, 0x190D, 0x2590, 0x340D, 0x3E8D, 0x4B10, 0x5790, 0x6410, 0x7090, 0x7D10, 0x8990, 0x9610, 0xA291, 0xAF0D, 0xBB90, 0xC80D}
};
static const uint32_t v2Timings[8][16] = {
    { 0x347,  0x64F,  0x95F,  0xC70,  0xF80, 0x129E, 0x15B3, 0x18CB, 0x1BE4, 0x1EFB, 0x2211, 0x252C, 0x2786, 0x2A88, 0x2D85, 0x3088},
    { 0x864, 0x10ED, 0x196C, 0x21EA, 0x2A6C, 0x32E9, 0x3B6A, 0x43EC, 0x4C6A, 0x54EA, 0x5D6B, 0x65EB, 0x6E6D, 0x76ED, 0x7F6E, 0x87EE},
    { 0x864, 0x10ED, 0x196C, 0x21EA, 0x2A6C, 0x32E9, 0x3B6A, 0x43EC, 0x4C6A, 0x54EA, 0x5D6B, 0x65EB, 0x6E6D, 0x76ED, 0x7F6E, 0x87EE},
    { 0xA69, 0x14EA, 0x1F6F, 0x29ED, 0x346E, 0x3EEE, 0x496C, 0x53EB, 0x5E6B, 0x68ED, 0x7374, 0x7DF0, 0x887B, 0x92F4, 0x9D75, 0xA7FB},
    { 0xA6A, 0x14EA, 0x1F6F, 0x29ED, 0x346E, 0x3EEE, 0x496C, 0x53EB, 0x5E6B, 0x68ED, 0x7374, 0x7DF0, 0x887B, 0x92F4, 0x9D75, 0xA7FB},
    { 0xC88, 0x1906, 0x2588, 0x340D, 0x3E8D, 0x4B10, 0x5790, 0x6410, 0x7090, 0x7D10, 0x8990, 0x9610, 0xA291, 0xAF0D, 0xBB90, 0xC80D},
    { 0xC88, 0x1906, 0x2588, 0x340D, 0x3E8D, 0x4B10, 0x5790, 0x6410, 0x7090, 0x7D10, 0x8990, 0x9610, 0xA291, 0xAF0D, 0xBB90, 0xC80D},
    { 0xC88, 0x1906, 0x2588, 0x340D, 0x3E8D, 0x4B10, 0x5790, 0x6410, 0x7090, 0x7D10, 0x8990, 0x9610, 0xA291, 0xAF0D, 0xBB90, 0xC80D},
};
static const uint32_t psTimings[8][16] = {
    { 0x440,  0x820,  0xC30, 0x1000, 0x1400, 0x17F1, 0x1C00, 0x2000, 0x2400, 0x2800, 0x2C00, 0x3000, 0x3400, 0x3800, 0x3C00, 0x4000},
    { 0x940, 0x12C0, 0x1C80, 0x25C0, 0x2F40, 0x3900, 0x4240, 0x4C00, 0x5540, 0x5EC0, 0x6850, 0x71C0, 0x7B40, 0x84C0, 0x8E80, 0x97CE},
    { 0xB70, 0x1700, 0x2280, 0x2E00, 0x3A00, 0x4500, 0x5080, 0x5C00, 0x6780, 0x7320, 0x7E80, 0x8A00, 0x9580, 0xA100, 0xAC80, 0xB7F0},
    { 0xB80, 0x1700, 0x2280, 0x2E00, 0x3A00, 0x4500, 0x5080, 0x5C00, 0x6780, 0x7320, 0x7E80, 0x8A00, 0x9580, 0xA100, 0xAC80, 0xB800},
    { 0xB90, 0x1700, 0x2280, 0x2E00, 0x3A00, 0x4500, 0x5080, 0x5C00, 0x6780, 0x7320, 0x7E80, 0x8A00, 0x9580, 0xA100, 0xAC80, 0xB800},
    { 0xB90, 0x1700, 0x2280, 0x2E00, 0x3A00, 0x4500, 0x5080, 0x5C00, 0x6780, 0x7320, 0x7E80, 0x8A00, 0x9580, 0xA100, 0xAC80, 0xB800},
    { 0xC88, 0x1906, 0x2588, 0x340D, 0x3E8D, 0x4B10, 0x5790, 0x6410, 0x7090, 0x7D10, 0x8990, 0x9610, 0xA291, 0xAF0D, 0xBB90, 0xC80D},
    { 0xC88, 0x1906, 0x2588, 0x340D, 0x3E8D, 0x4B10, 0x5790, 0x6410, 0x7090, 0x7D10, 0x8990, 0x9610, 0xA291, 0xAF0D, 0xBB90, 0xC80D},
};

static std::map<int, std::vector<int>> BIT_ORDERS =
{
    {6, {5, 2, 1, 4, 3, 0}},
    {7, {6, 2, 1, 4, 5, 3, 0}},
    {8, {7, 3, 5, 1, 2, 6, 4, 0}},
    //{8, {7, 6, 5, 4, 3, 2, 1, 0}},
    {9, {8, 3, 5, 1, 7, 2, 6, 4, 0}},
    {10, {9, 4, 1, 6, 3, 8, 2, 7, 5, 0}},
    {11, {10, 4, 7, 2, 3, 1, 6, 9, 8, 5, 0}},
    {12, {11, 5, 8, 2, 4, 1, 7, 10, 3, 9, 6, 0}}
};
static void compilePRUMatrixCode(std::vector<std::string> &sargs) {
    pid_t compilePid = fork();
    if (compilePid == 0) {
        char * args[sargs.size() + 3];
        args[0] = (char *)"/bin/bash";
        args[1] = (char *)"/opt/fpp/src/pru/compileMatrix.sh";
        
        for (int x = 0; x < sargs.size(); x++) {
            args[x + 2] = (char*)sargs[x].c_str();
        }
        args[sargs.size() + 2] = NULL;
        
        execvp("/bin/bash", args);
    } else {
        wait(NULL);
    }
}

void BBBMatrix::calcBrightnessFlags(std::vector<std::string> &sargs) {
    
    LogDebug(VB_CHANNELOUT, "Calc Brightness:   maxPanel:  %d    maxOutput: %d     Brightness: %d    rpo: %d    ph:  %d    pw:  %d\n", m_longestChain, m_outputs, m_brightness, m_panelScan, m_panelHeight, m_panelWidth);
    
    
    uint32_t max = 0xB00;
    switch (m_timing) {
        case 2:
            max = psTimings[m_outputs-1][m_longestChain-1];
        break;
        case 1:
            max = v2Timings[m_outputs-1][m_longestChain-1];
        break;
        default:
            max = v1Timings[m_outputs-1][m_longestChain-1];
        break;
    }
    
    //timings are based on 32 pixel wide panels
    max *= m_panelWidth;
    max /= 32;

    // 1/4 scan we need to double the time since we have twice the number of pixels to clock out
    max *= m_panelHeight;
    max /= (m_panelScan * 2);

    uint32_t origMax = max;
    if (m_colorDepth >= 11 && max < 0x9000) {
        //for depth 10, we'll need a little more on time
        //or the last bit will be on far too short
        max = 0x9000;
    } else if (m_colorDepth >= 10 && max < 0x8000) {
        //for depth 10, we'll need a little more on time
        //or the last bit will be on far too short
        max = 0x8000;
    } else if (m_colorDepth == 9 && max < 0x6800) {
        //for depth 9, we'll need a little more on time
        max = 0x6800;
    } else if (max < 0x4500) {
        //if max is too low, the low bit time is too short and
        //extra ghosting occurs
        // At this point, framerate will be supper high anyway >100fps
        max = 0x4500;
    }

    uint32_t origMax2 = max;

    max *= m_brightness;
    max /= 10;

    uint32_t delay = origMax2 - max;
    if ((origMax2 > origMax) && (origMax > max)) {
        delay = origMax - max;
    }
    
    int maxBit = 8;
    if (m_colorDepth > 8) {
        maxBit = m_colorDepth;
    }
    //printf("Delay : %d      Max:  %d       OrigMax:   %d      OrigMax2:  %d\n", delay, max, origMax, origMax2);
    for (int x = 0; x < maxBit; x++) {
        LogDebug(VB_CHANNELOUT, "Brightness %d:  %X\n", x, max);
        delayValues[x] = delay;
        brightnessValues[x] = max;
        max >>= 1;
        origMax >>= 1;
    }
    // low value cannot be less than 20 or ghosting
    if (brightnessValues[maxBit - 1] < 20) {
        brightnessValues[maxBit - 1] = 20;
    }
    if (brightnessValues[maxBit - 2] < brightnessValues[maxBit - 1]) {
        brightnessValues[maxBit - 2] = brightnessValues[maxBit - 2] + 5;
    }

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
    
    char buf[255];
    
    int x = m_colorDepth;
    m_bitOrder.clear();
    if (m_outputByRow) {
        //if outputing by row, we have to keep in decending order
        for (int x = m_colorDepth; x > 0; --x) {
            m_bitOrder.push_back(x-1);
        }
    } else {
        m_bitOrder = BIT_ORDERS[m_colorDepth];
    }
    
    for (auto b : m_bitOrder) {
        int idx = m_colorDepth - b - 1;
        sprintf(buf, "-DBRIGHTNESS%d=%d", x, brightnessValues[idx]);
        
        sargs.push_back(buf);
        sprintf(buf, "-DDELAY%d=%d", x, delayValues[idx]);
        sargs.push_back(buf);
        x--;
    }
}

class InterleaveHandler {
protected:
    InterleaveHandler() {}
    
public:
    virtual ~InterleaveHandler() {}

    virtual void mapRow(int &y) = 0;
    virtual void mapCol(int y, int &x) = 0;

private:
};

class NoInterleaveHandler : public InterleaveHandler {
public:
    NoInterleaveHandler() {}
    virtual ~NoInterleaveHandler() {}
    
    virtual void mapRow(int &y) override {}
    virtual void mapCol(int y, int &x) override {}
};
class SimpleInterleaveHandler : public InterleaveHandler {
public:
    SimpleInterleaveHandler(int interleave, int ph, int pw, int ps, bool flip)
        : InterleaveHandler(), m_interleave(interleave), m_panelHeight(ph), m_panelWidth(pw), m_panelScan(ps), m_flipRows(flip) {}
    virtual ~SimpleInterleaveHandler() {}

    virtual void mapRow(int &y) override {
        while (y >= m_panelScan) {
            y -= m_panelScan;
        }
    }
    virtual void mapCol(int y, int &x) override {
        int whichInt = x / m_interleave;
        if (m_flipRows) {
            if (y & m_panelScan) {
                y &= !m_panelScan;
            } else {
                y |= m_panelScan;
            }
        }
        int offInInt = x % m_interleave;
        int mult = (m_panelHeight / m_panelScan / 2) - 1 - y / m_panelScan;
        x = m_interleave * (whichInt * m_panelHeight / m_panelScan / 2 + mult)  + offInInt;
    }
    
private:
    const int m_interleave;
    const int m_panelWidth;
    const int m_panelHeight;
    const int m_panelScan;
    const bool m_flipRows;
};

class ZigZagInterleaveHandler : public InterleaveHandler {
public:
    ZigZagInterleaveHandler(int interleave, int ph, int pw, int ps)
        : InterleaveHandler(), m_interleave(interleave), m_panelHeight(ph), m_panelWidth(pw), m_panelScan(ps) {}
    virtual ~ZigZagInterleaveHandler() {}
    
    virtual void mapRow(int &y) override {
        while (y >= m_panelScan) {
            y -= m_panelScan;
        }
    }
    virtual void mapCol(int y, int &x) override {
        int whichInt = x / m_interleave;
        int offInInt = x % m_interleave;
        int mult = y / m_panelScan;
        
        if (m_panelScan == 2) {
            if ((y & 0x2) == 0) {
                offInInt = m_interleave - 1 - offInInt;
            }
        } else if (m_interleave == 4) {
            if ((whichInt & 0x1) == 1) {
                mult = (y < m_panelScan ? y + m_panelScan : y - m_panelScan) / m_panelScan;
            }
        } else {
            int tmp = (y * 2) / m_panelScan;
            if ((tmp & 0x2) == 0) {
                offInInt = m_interleave - 1 - offInInt;
            }
        }
        x = m_interleave * (whichInt * m_panelHeight / m_panelScan / 2 + mult) + offInInt;
    }
    
private:
    const int m_interleave;
    const int m_panelWidth;
    const int m_panelHeight;
    const int m_panelScan;
};


BBBMatrix::BBBMatrix(unsigned int startChannel, unsigned int channelCount)
  : ChannelOutputBase(startChannel, channelCount),
    m_pru(nullptr),
    m_pruCopy(nullptr),
	m_matrix(nullptr),
	m_panelMatrix(nullptr),
    m_outputs(0),
    m_gpioFrame(nullptr),
    m_longestChain(0),
    m_panelWidth(32),
    m_panelHeight(16),
    m_invertedData(0),
    m_brightness(10),
    m_colorDepth(8),
    m_interleave(0),
    m_panelScan(8),
    m_timing(0),
    m_printStats(false),
    m_handler(nullptr),
    m_outputByRow(false),
    m_outputBlankData(false)
{
	LogDebug(VB_CHANNELOUT, "BBBMatrix::BBBMatrix(%u, %u)\n",
		startChannel, channelCount);
}

BBBMatrix::~BBBMatrix()
{
    LogDebug(VB_CHANNELOUT, "BBBMatrix::~BBBMatrix()\n");
    if (m_gpioFrame) delete [] m_gpioFrame;
    if (m_pru) delete m_pru;
    if (m_pruCopy) delete m_pruCopy;
    if (m_handler) delete m_handler;
    if (m_matrix) delete m_matrix;
    if (m_panelMatrix) delete m_panelMatrix;
}

bool BBBMatrix::configureControlPin(const std::string &ctype, Json::Value &root, std::ofstream &outputFile) {
    std::string type = root["controls"][ctype]["type"].asString();
    if (type != "none") {
        std::string pinName = root["controls"][ctype]["pin"].asString();
        const PinCapabilities &pin = PinCapabilities::getPinByName(pinName);
        if (ctype == "oe" && pin.pwm >= 99)  {
            outputFile << "#define oe_pwm_address " << std::to_string(pin.getPWMRegisterAddress()) << "\n";
            outputFile << "#define oe_pwm_output " << std::to_string(pin.subPwm) << "\n";
            int max = 300*255;
            //FIXME - adjust max for brightess
            pin.setupPWM(max);
            pin.setPWMValue(0);
            return true;
        } else {
            pin.configPin(type);
            if (type == "pruout") {
                outputFile << "#define pru_" << ctype << " " << std::to_string(pin.pruPin) << "\n";
            } else {
                outputFile << "#define gpio_" << ctype << " " << std::to_string(pin.gpio) << "\n";
            }
        }
        m_usedPins.push_back(pinName);
    }
    return false;
}

void BBBMatrix::configurePanelPin(int x, const std::string &color, int row, Json::Value &root, std::ofstream &outputFile, int *minPort) {
    std::string pinName = root["outputs"][x]["pins"][color + std::to_string(row)].asString();
    const PinCapabilities &pin = PinCapabilities::getPinByName(pinName);
    pin.configPin();
    m_usedPins.push_back(pinName);
    int gpioIdx = pin.gpioIdx;
    minPort[gpioIdx] = std::min(minPort[gpioIdx], (int)pin.gpio);
    outputFile << "#define " << color << std::to_string(x+1) << std::to_string(row) << "_gpio " << std::to_string(pin.gpioIdx) << "\n";
    outputFile << "#define " << color << std::to_string(x+1) << std::to_string(row) << "_pin  " << std::to_string(pin.gpio) << "\n";
    
    if (color == "r") {
        m_pinInfo[x].row[row-1].r_gpio = pin.gpioIdx;
        m_pinInfo[x].row[row-1].r_pin = 1UL << pin.gpio;
    } else if (color == "g") {
        m_pinInfo[x].row[row-1].g_gpio = pin.gpioIdx;
        m_pinInfo[x].row[row-1].g_pin = 1UL << pin.gpio;
    } else {
        m_pinInfo[x].row[row-1].b_gpio = pin.gpioIdx;
        m_pinInfo[x].row[row-1].b_pin = 1UL << pin.gpio;
    }
}

void BBBMatrix::configurePanelPins(int x, Json::Value &root, std::ofstream &outputFile, int *minPort) {
    configurePanelPin(x, "r", 1, root, outputFile, minPort);
    configurePanelPin(x, "g", 1, root, outputFile, minPort);
    configurePanelPin(x, "b", 1, root, outputFile, minPort);
    outputFile << "\n";
    configurePanelPin(x, "r", 2, root, outputFile, minPort);
    configurePanelPin(x, "g", 2, root, outputFile, minPort);
    configurePanelPin(x, "b", 2, root, outputFile, minPort);
    outputFile << "\n";
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

    int addressingType = config["panelAddressing"].asInt();
    
    m_invertedData = config["invertedData"].asInt();
    m_colorOrder = ColorOrderFromString(config["colorOrder"].asString());

    m_panelMatrix = new PanelMatrix(m_panelWidth, m_panelHeight, m_invertedData);
    if (!m_panelMatrix) {
        LogErr(VB_CHANNELOUT, "BBBMatrix: Unable to create PanelMatrix\n");
        return 0;
    }
    bool usesOutput[16] = {
        false, false, false, false,
        false, false, false, false,
        false, false, false, false,
        false, false, false, false
    };
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
        usesOutput[p["outputNumber"].asInt()] = true;
        
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
    if (config.isMember("panelOutputOrder")) {
        m_outputByRow = config["panelOutputOrder"].asBool();
    }
    if (config.isMember("panelOutputBlankRow")) {
        m_outputBlankData = config["panelOutputBlankRow"].asBool();
    }
    if (m_colorDepth < 0) {
        m_colorDepth = -m_colorDepth;
        m_outputBlankData = true;
    }
    if (m_colorDepth > 12 || m_colorDepth < 6) {
        m_colorDepth = 8;
    }
    bool zigZagInterleave = false;
    bool flipRows = false;
    if (config.isMember("panelInterleave")) {
        if (config["panelInterleave"].asString() == "8z") {
            m_interleave = 8;
            zigZagInterleave = true;
        } else if (config["panelInterleave"].asString() == "16z") {
            m_interleave = 16;
            zigZagInterleave = true;
        } else if (config["panelInterleave"].asString() == "4z") {
            m_interleave = 4;
            zigZagInterleave = true;
        } else if (config["panelInterleave"].asString() == "8f") {
            m_interleave = 8;
            flipRows = true;
        } else if (config["panelInterleave"].asString() == "16f") {
            m_interleave = 16;
            flipRows = true;
        } else if (config["panelInterleave"].asString() == "32f") {
            m_interleave = 32;
            flipRows = true;
        } else if (config["panelInterleave"].asString() == "64f") {
            m_interleave = 64;
            flipRows = true;
        } else {
            m_interleave = std::atoi(config["panelInterleave"].asString().c_str());
        }
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
    int maxBits = 8;
    if (m_colorDepth > 8) {
        maxBits = m_colorDepth;
    }
    int gpioFrameLen = m_longestChain * m_panelWidth * maxBits * (m_panelHeight / 2) * 4;
    m_gpioFrame = new uint32_t[gpioFrameLen];
    memset(m_gpioFrame, 0, gpioFrameLen);

    std::vector<std::string> compileArgs;
    
    
    std::string dirname = "bbb";
    std::string name = "Octoscroller";
    if (getBeagleBoneType() == PocketBeagle) {
        dirname = "pb";
        name = "PocketScroller";
    }
    if (config["wiringPinout"] == "v2") {
        name += "-v2";
    }
    if (config["wiringPinout"] == "v3") {
        name += "-v3";
    }
    Json::Value root;
    char filename[256];
    int minPort[4] = {99, 99, 99, 99};
    int pru = 0;
    sprintf(filename, "/home/fpp/media/tmp/panels/%s.json", name.c_str());
    if (!FileExists(filename)) {
        sprintf(filename, "/opt/fpp/capes/%s/panels/%s.json", dirname.c_str(), name.c_str());
    }
    bool isPWM = false;
    if (!FileExists(filename)) {
        LogErr(VB_CHANNELOUT, "No output pin configuration for %s - %s\n", name.c_str(), filename);
        return 0;
    } else {
        if (!LoadJsonFromFile(filename, root)) {
            LogErr(VB_CHANNELOUT, "Could not read pin configuration for %s - %s\n", name.c_str(), filename);
            return 0;
        }
        std::string longName = root["longName"].asString();
        LogDebug(VB_CHANNELOUT, "Using pin configuration for %s from %s\n", longName.c_str(), filename);

        std::ofstream outputFile;
        outputFile.open("/tmp/PanelPinConfiguration.hp", std::ofstream::out | std::ofstream::trunc);
        
        //kind of a hack, ideally the timing info would go into the json as well
        m_timing = root["timing"].asInt();
        
        pru = root["pru"].asInt();
        configureControlPin("latch", root, outputFile);
        outputFile << "\n";
        isPWM = configureControlPin("oe", root, outputFile);
        outputFile << "\n";
        configureControlPin("clock", root, outputFile);
        outputFile << "\n";
        configureControlPin("sel0", root, outputFile);
        configureControlPin("sel1", root, outputFile);
        configureControlPin("sel2", root, outputFile);
        configureControlPin("sel3", root, outputFile);
        if (m_panelScan == 32) {
            //1:32 scan panels need the "E" line
            configureControlPin("sel4", root, outputFile);
            compileArgs.push_back("-DE_SCAN_LINE");
        }
        outputFile << "\n";
        int controlGpio = root["controls"]["gpio"].asInt();
        outputFile << "#define CONTROLS_GPIO_BASE GPIO" << std::to_string(controlGpio) << "\n";
        outputFile << "#define gpio_controls_led_mask gpio" << std::to_string(controlGpio) << "_led_mask\n";
        outputFile << "\n";
        if (m_outputs > root["outputs"].size()) {
            m_outputs = root["outputs"].size();
        }
        for (int x = 0; x < 8; x++) {
            if (usesOutput[x] && x < m_outputs) {
                configurePanelPins(x, root, outputFile, minPort);
            } else {
                outputFile << "#define NO_OUTPUT_" << std::to_string(x + 1) << "\n";
                outputFile << "\n";
            }
        }
        outputFile << "\n";
        for (int x = 0; x < 4; x++) {
            if (minPort[x] != 99) {
                if (x == controlGpio) {
                    outputFile << "#define OUTPUT_GPIO_" << std::to_string(x) << "(a, b, c, d) OUTPUT_GPIO_FORCE_CLEAR a, b, c, d\n";
                } else {
                    outputFile << "#define OUTPUT_GPIO_" << std::to_string(x) << "(a, b, c, d) OUTPUT_GPIO a, b, c, d\n";
                }
            } else {
                outputFile << "#define OUTPUT_GPIO_" << std::to_string(x) << "(a, b, c, d)\n";
            }
        }
        outputFile << "\n";
        if (minPort[controlGpio] == 99) {
            //not outputting anything on the GPIO the controls are using
            //we need to make sure the controls are set/cleared indepentent of the panel data
            outputFile << "#define NO_CONTROLS_WITH_DATA\n";
        }
        outputFile << "\n";

        outputFile.close();
    }
    
    char buf[200];
    sprintf(buf, "-DRUNNING_ON_PRU%d", pru);
    compileArgs.push_back(buf);
    sprintf(buf, "-DOUTPUTS=%d", m_outputs);
    compileArgs.push_back(buf);
    sprintf(buf, "-DROWS=%d", m_panelScan);
    compileArgs.push_back(buf);
    sprintf(buf, "-DBITS=%d", m_colorDepth);
    compileArgs.push_back(buf);
    int tmp = m_longestChain * m_panelWidth;
    if (m_panelScan * 4 == m_panelHeight) {
        tmp *= 2;
    } else if (m_panelScan * 8 == m_panelHeight) {
        tmp *= 4;
    }
    sprintf(buf, "-DROW_LEN=%d", tmp);
    compileArgs.push_back(buf);
    if (isPWM) {
        compileArgs.push_back("-DUSING_PWM");
    }

    if (addressingType == 1) {
        // 1/2 scan panel that uses 2 bits, bit one for scan row 1 and bit two for row 2
        // Normal addressing would be 1 bit, 0 for row 1, 1 for row 2
        compileArgs.push_back("-DADDRESSING_AB=1");
    }
    
    calcBrightnessFlags(compileArgs);
    if (m_printStats) {
        sprintf(buf, "-DENABLESTATS=1", m_outputs);
        compileArgs.push_back("-DENABLESTATS=1");
    }

    if (m_outputByRow) {
        compileArgs.push_back("-DOUTPUTBYROW");
        if (m_outputBlankData) {
            compileArgs.push_back("-DOUTPUTBLANKROW");
        }
    } else {
        compileArgs.push_back("-DOUTPUTBYDEPTH");
    }
    compilePRUMatrixCode(compileArgs);
    std::string pru_program = "/tmp/FalconMatrix.bin";

    m_pruCopy = new BBBPru(pru ? 0 : 1);
    memset(m_pruCopy->data_ram, 0, 24);

    m_pru = new BBBPru(pru);
    m_pruData = (BBBPruMatrixData*)m_pru->data_ram;
    m_pruData->address_dma = m_pru->ddr_addr;
    m_pruData->command = 0;
    m_pruData->response = 0;

    for (int x = 0; x < 8; x++) {
        m_pruData->pwmBrightness[x] = 0;
    }
    
    for (int x = 0; x < MAX_STATS; x++) {
        m_pruData->stats[x * 3] = 0;
        m_pruData->stats[x * 3 + 1] = 0;
        m_pruData->stats[x * 3 + 1] = 0;
    }
    m_pruCopy->run("/tmp/FalconMatrixPRUCpy.bin");
    m_pru->run(pru_program);
    
    if (m_interleave && ((m_panelScan * 2) != m_panelHeight)) {
        if (zigZagInterleave) {
            m_handler = new ZigZagInterleaveHandler(m_interleave, m_panelHeight, m_panelWidth, m_panelScan);
        } else {
            m_handler = new SimpleInterleaveHandler(m_interleave, m_panelHeight, m_panelWidth, m_panelScan, flipRows);
        }
    } else {
        m_handler = new NoInterleaveHandler();
    }
    
    
    float gamma = 2.2;
    if (config.isMember("gamma")) {
        gamma = atof(config["gamma"].asString().c_str());
    }
    if (gamma < 0.01 || gamma > 50.0) {
        gamma = 2.2;
    }
    for (int x = 0; x < 256; x++) {
        int v = x;
        if (m_colorDepth == 6 && (v == 3 || v == 2)) {
            v = 4;
        } else if (m_colorDepth == 7 && v == 1) {
            v = 2;
        }
        float max = 255.0f;
        switch (m_colorDepth) {
            case 12:
                max = 4095.0f;
            break;
            case 11:
                max = 2047.0f;
            break;
            case 10:
                max = 1023.0f;
            break;
            case 9:
                max = 511.0f;
            break;
        }
        float f = v;
        f = max * pow(f / 255.0f, gamma);
        if (f > max) {
            f = max;
        }
        if (f < 0.0) {
            f = 0.0;
        }
        gammaCurve[x] = round(f);
        if (gammaCurve[x] == 0 && f > 0.25)  {
            //don't drop as much of the low end to 0
            gammaCurve[x] = 1;
        }
        //printf("%d   %d  (%f)\n", x, gammaCurve[x], f);
    }

    if (isPWM) {
        //need to calculate the clock counts for the PWM subsystem
        int i = m_pruData->pwmBrightness[0];
        while (i == 0) {
            i = m_pruData->pwmBrightness[0];
        }
        printf("PERIOD: %X\n", i);
        
        int f = i;
        //f *= 300;
        //f /= 500;
        i = f;
        printf("New PERIOD: %X\n", i);

        i *= m_brightness;
        i /= 10;
        for (int x = 0; x < 8; x++) {
            printf("%d: %X\n", x, i);
            m_pruData->pwmBrightness[7-x] = i;
            i /= 2;
        }
        
    }
    /*
    for (int x = 0; x < 8; x++) {
        printf("%d  R1:   %d   %8X\n",x,  m_pinInfo[x].row[0].r_gpio, m_pinInfo[x].row[0].r_pin);
        printf("    G1:   %d   %8X\n", m_pinInfo[x].row[0].g_gpio, m_pinInfo[x].row[0].g_pin);
        printf("    B1:   %d   %8X\n", m_pinInfo[x].row[0].b_gpio, m_pinInfo[x].row[0].b_pin);
        printf("    R2:   %d   %8X\n", m_pinInfo[x].row[1].r_gpio, m_pinInfo[x].row[1].r_pin);
        printf("    G2:   %d   %8X\n", m_pinInfo[x].row[1].g_gpio, m_pinInfo[x].row[1].g_pin);
        printf("    B2:   %d   %8X\n", m_pinInfo[x].row[1].b_gpio, m_pinInfo[x].row[1].b_pin);
    }
    */
    
    //make sure the PRU starts outputting a blank frame to remove any random noise
    //from the panels
    memcpy(m_pru->ddr, m_gpioFrame, gpioFrameLen);
    m_pruData->address_dma = m_pru->ddr_addr;
    //make sure memory is flushed before command is set to 1
    __asm__ __volatile__("":::"memory");
    m_pruData->command = 1;
    return ChannelOutputBase::Init(config);
}

int BBBMatrix::Close(void)
{
    LogDebug(VB_CHANNELOUT, "BBBMatrix::Close()\n");
    // Send the stop command
    m_pruData->command = 0xFF;
    if (m_pru) {
        m_pru->stop(true);
        delete m_pru;
        m_pru = nullptr;
    }
    
    if (m_pruCopy) {
        m_pruCopy->stop(true);
        delete m_pruCopy;
        m_pruCopy = nullptr;
    }
    for (auto &pinName : m_usedPins) {
        
        const PinCapabilities &pin = PinCapabilities::getPinByName(pinName);
        pin.configPin("default", false);
    }
    return ChannelOutputBase::Close();
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

void BBBMatrix::GetRequiredChannelRanges(const std::function<void(int, int)> &addRange) {
    addRange(m_startChannel, m_startChannel + m_channelCount - 1);
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

    channelData += m_startChannel;
    
    
    //number of uint32_t per row for each bit
    size_t rowLen = m_panelWidth * m_longestChain * m_panelHeight / (m_panelScan * 2) * 4; //4 GPIO's
    //number of uint32_t per full row (all bits)
    size_t fullRowLen = rowLen * m_colorDepth;
    memset(m_gpioFrame, 0, sizeof(uint32_t) * fullRowLen * m_panelScan);

    for (int output = 0; output < m_outputs; output++) {
        int panelsOnOutput = m_panelMatrix->m_outputPanels[output].size();
        
        for (int i = 0; i < panelsOnOutput; i++) {
            int panel = m_panelMatrix->m_outputPanels[output][i];
            int chain = m_panelMatrix->m_panels[panel].chain;
            
            for (int y = 0; y < (m_panelHeight / 2); y++) {
                int yw1 = y * m_panelWidth * 3;
                int yw2 = (y + (m_panelHeight / 2)) * m_panelWidth * 3;

                
                int yOut = y;
                m_handler->mapRow(yOut);
                
                int offset = yOut * fullRowLen + (m_longestChain - chain - 1) * 4 * m_panelWidth * m_panelHeight / m_panelScan / 2;
                if (!m_outputByRow) {
                    offset = yOut * rowLen + (m_longestChain - chain - 1) * 4 * m_panelWidth * m_panelHeight / m_panelScan / 2;
                }
                
                for (int x = 0; x < m_panelWidth; ++x) {
                    uint16_t r1 = gammaCurve[channelData[m_panelMatrix->m_panels[panel].pixelMap[yw1 + x*3]]];
                    uint16_t g1 = gammaCurve[channelData[m_panelMatrix->m_panels[panel].pixelMap[yw1 + x*3 + 1]]];
                    uint16_t b1 = gammaCurve[channelData[m_panelMatrix->m_panels[panel].pixelMap[yw1 + x*3 + 2]]];
                    
                    uint16_t r2 = gammaCurve[channelData[m_panelMatrix->m_panels[panel].pixelMap[yw2 + x*3]]];
                    uint16_t g2 = gammaCurve[channelData[m_panelMatrix->m_panels[panel].pixelMap[yw2 + x*3 + 1]]];
                    uint16_t b2 = gammaCurve[channelData[m_panelMatrix->m_panels[panel].pixelMap[yw2 + x*3 + 2]]];

                    int xOut = x;
                    m_handler->mapCol(y, xOut);
                    
                    int xOff = xOut * 4;
                    
                    for (auto bit : m_bitOrder) {

                        uint16_t mask = 1 << bit;
                        if (r1 & mask) {
                            m_gpioFrame[offset + xOff + m_pinInfo[output].row[0].r_gpio] |= m_pinInfo[output].row[0].r_pin;
                        }
                        if (g1 & mask) {
                            m_gpioFrame[offset + xOff + m_pinInfo[output].row[0].g_gpio] |= m_pinInfo[output].row[0].g_pin;
                        }
                        if (b1 & mask) {
                            m_gpioFrame[offset + xOff + m_pinInfo[output].row[0].b_gpio] |= m_pinInfo[output].row[0].b_pin;
                        }
                        if (r2 & mask) {
                            m_gpioFrame[offset + xOff + m_pinInfo[output].row[1].r_gpio] |= m_pinInfo[output].row[1].r_pin;
                        }
                        if (g2 & mask) {
                            m_gpioFrame[offset + xOff + m_pinInfo[output].row[1].g_gpio] |= m_pinInfo[output].row[1].g_pin;
                        }
                        if (b2 & mask) {
                            m_gpioFrame[offset + xOff + m_pinInfo[output].row[1].b_gpio] |= m_pinInfo[output].row[1].b_pin;
                        }
                        if (m_outputByRow) {
                            xOff += rowLen;
                        } else {
                            xOff += rowLen * m_panelScan;
                        }
                    }
                }
            }
        }
    }
}
int BBBMatrix::SendData(unsigned char *channelData)
{
    LogExcess(VB_CHANNELOUT, "BBBMatrix::SendData(%p)\n", channelData);
    
    size_t rowLen = m_panelWidth * m_longestChain * m_panelHeight / (m_panelScan * 2) * 4; //4 GPIO's
    size_t len = sizeof(uint32_t) * rowLen * m_colorDepth * m_panelScan;
    
    if (len < (m_pru->ddr_size / 2)) {
        //we can flip frames
        memcpy(m_pru->ddr + (m_evenFrame ? (m_pru->ddr_size / 2) : 0), m_gpioFrame, len);
        m_pruData->address_dma = m_pru->ddr_addr + (m_evenFrame ? (m_pru->ddr_size / 2) : 0);
        m_evenFrame = !m_evenFrame;
    } else {
        memcpy(m_pru->ddr, m_gpioFrame, len);
        m_pruData->address_dma = m_pru->ddr_addr;
    }
    
    //make sure memory is flushed before command is set to 1
    __asm__ __volatile__("":::"memory");
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
