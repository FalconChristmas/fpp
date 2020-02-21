/*
 *   librgbmatrix handler for Falcon Player (FPP)
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
#include <cmath>

#include "common.h"
#include "log.h"
#include "RGBMatrix.h"
#include "settings.h"


extern "C" {
    RGBMatrixOutput *createOutputRGBMatrix(unsigned int startChannel,
                                          unsigned int channelCount) {
        return new RGBMatrixOutput(startChannel, channelCount);
    }
}

/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
RGBMatrixOutput::RGBMatrixOutput(unsigned int startChannel,
	unsigned int channelCount)
  : ChannelOutputBase(startChannel, channelCount),
	m_gpio(NULL),
	m_canvas(NULL),
	m_rgbmatrix(NULL),
	m_colorOrder("RGB"),
	m_panelWidth(32),
	m_panelHeight(16),
	m_panels(0),
	m_width(0),
	m_height(0),
	m_rows(0),
	m_outputs(0),
	m_longestChain(0),
	m_invertedData(0)
{
	LogDebug(VB_CHANNELOUT, "RGBMatrixOutput::RGBMatrixOutput(%u, %u)\n",
		startChannel, channelCount);
}

/*
 *
 */
RGBMatrixOutput::~RGBMatrixOutput()
{
	LogDebug(VB_CHANNELOUT, "RGBMatrixOutput::~RGBMatrixOutput()\n");

	delete m_rgbmatrix;
	delete m_matrix;
	delete m_panelMatrix;
}

/*
 *
 */
int RGBMatrixOutput::Init(Json::Value config)
{
	LogDebug(VB_CHANNELOUT, "RGBMatrixOutput::Init(JSON)\n");

	m_panelWidth  = config["panelWidth"].asInt();
	m_panelHeight = config["panelHeight"].asInt();

	if (!m_panelWidth)
		m_panelWidth = 32;

	if (!m_panelHeight)
		m_panelHeight = 16;

	m_invertedData = config["invertedData"].asInt();
	m_colorOrder = config["colorOrder"].asString();

	m_panelMatrix =
		new PanelMatrix(m_panelWidth, m_panelHeight, m_invertedData);

	if (!m_panelMatrix)
	{
		LogErr(VB_CHANNELOUT, "Unable to create PanelMatrix\n");

		return 0;
	}

	for (int i = 0; i < config["panels"].size(); i++)
	{
		Json::Value p = config["panels"][i];
		char orientation = 'N';
		const char *o = p["orientation"].asString().c_str();

		if (o && *o)
			orientation = o[0];

		if (p["colorOrder"].asString() == "")
			p["colorOrder"] = m_colorOrder;

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

	m_panels = m_panelMatrix->PanelCount();

	m_gpio = new GPIO();
	if (!m_gpio)
	{
		LogErr(VB_CHANNELOUT, "Unable to create GPIO instance\n");

		return 0;
	}

	int gpioSlowdown = 1;
	if (config.isMember("gpioSlowdown"))
		gpioSlowdown = config["gpioSlowdown"].asInt();

	if (!m_gpio->Init(gpioSlowdown))
	{
		LogErr(VB_CHANNELOUT, "GPIO->Init() failed\n");

		delete m_gpio;
		return 0;
	}

	m_rows = m_panelHeight;

	m_width  = m_panelMatrix->Width();
	m_height = m_panelMatrix->Height();

	m_channelCount = m_width * m_height * 3;

	RGBMatrix::Options options;

	if (config["wiringPinout"].asString() != "")
		options.hardware_mapping = strdup(config["wiringPinout"].asString().c_str());

	options.chain_length = m_longestChain;
	options.parallel = m_outputs;
	options.rows = m_panelHeight;
	options.cols = m_panelWidth;

	if (config.isMember("brightness"))
		options.brightness = config["brightness"].asInt();
	else
		options.brightness = 100;
    
    int colorDepth = 8;
    if (config.isMember("panelColorDepth")) {
        colorDepth = config["panelColorDepth"].asInt();
    }
    if (colorDepth > 11 || colorDepth < 6) {
        colorDepth = 11;
    }
    options.pwm_bits = colorDepth;

    LogDebug(VB_CHANNELOUT, "  chain: %d    parallel: %d   rows: %d     cols: %d    brightness: %d   colordepth: %d\n",
             options.chain_length, options.parallel, options.rows, options.cols,
             options.brightness, options.pwm_bits);

    if (config.isMember("cpuPWM")) {
        options.disable_hardware_pulsing = config["cpuPWM"].asBool();
        if (options.disable_hardware_pulsing) {
            LogDebug(VB_CHANNELOUT, "Disabling use of Hardware PWM for OE pin\n");
        }
    }
    if (config.isMember("panelInterleave")) {
        options.multiplexing = std::atoi(config["panelInterleave"].asString().c_str());
        
        int panelScan = config["panelScan"].asInt();
        if (panelScan == 0) {
            // 1/8 scan by default
            panelScan = m_panelHeight / 2;
        }
        if (panelScan == (m_panelHeight / 2)) {
            options.multiplexing = 0;
        }
    }
	m_rgbmatrix = new RGBMatrix(m_gpio, options);
	if (!m_rgbmatrix)
	{
		LogErr(VB_CHANNELOUT, "Unable to create RGBMatrix instance\n");

		delete m_gpio;
		m_gpio = NULL;

		return 0;
	}

	m_canvas = m_rgbmatrix->CreateFrameCanvas();
    FrameCanvas *c2 = m_rgbmatrix->CreateFrameCanvas();
    m_rgbmatrix->SwapOnVSync(c2);

	m_matrix = new Matrix(m_startChannel, m_width, m_height);

	if (config.isMember("subMatrices"))
	{
		for (int i = 0; i < config["subMatrices"].size(); i++)
		{
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
    
    float gamma = 2.2;
    if (config.isMember("gamma")) {
        gamma = atof(config["gamma"].asString().c_str());
    }
    if (gamma < 0.01 || gamma > 50.0) {
        gamma = 2.2;
    }
    for (int x = 0; x < 256; x++) {
        float f = x;
        f = 255.0 * pow(f / 255.0f, gamma);
        if (f > 255.0) {
            f = 255.0;
        }
        if (f < 0.0) {
            f = 0.0;
        }
        m_gammaCurve[x] = round(f);
    }

	return ChannelOutputBase::Init(config);
}
void RGBMatrixOutput::GetRequiredChannelRanges(const std::function<void(int, int)> &addRange) {
    addRange(m_startChannel, m_startChannel + m_channelCount - 1);
}

/*
 *
 */
int RGBMatrixOutput::Close(void)
{
	LogDebug(VB_CHANNELOUT, "RGBMatrixOutput::Close()\n");


	delete m_rgbmatrix;
    m_rgbmatrix = nullptr;
	m_canvas = nullptr;

	delete m_gpio;
	m_gpio = nullptr;

	return ChannelOutputBase::Close();
}

/*
 *
 */
void RGBMatrixOutput::PrepData(unsigned char *channelData)
{
    LogExcess(VB_CHANNELOUT, "RGBMatrixOutput::PrepData(%p)\n",
              channelData);
	m_matrix->OverlaySubMatrices(channelData);
    
    unsigned char r;
    unsigned char g;
    unsigned char b;

    channelData += m_startChannel;

    for (int output = 0; output < m_outputs; output++)
    {
        int panelsOnOutput = m_panelMatrix->m_outputPanels[output].size();
        
        for (int i = 0; i < panelsOnOutput; i++)
        {
            int panel = m_panelMatrix->m_outputPanels[output][i];
            
            int chain = (m_longestChain - 1) - m_panelMatrix->m_panels[panel].chain;
            for (int y = 0; y < m_panelHeight; y++)
            {
                int px = chain * m_panelWidth;
                for (int x = 0; x < m_panelWidth; x++)
                {
                    r = m_gammaCurve[channelData[m_panelMatrix->m_panels[panel].pixelMap[(y * m_panelWidth + x) * 3]]];
                    g = m_gammaCurve[channelData[m_panelMatrix->m_panels[panel].pixelMap[(y * m_panelWidth + x) * 3 + 1]]];
                    b = m_gammaCurve[channelData[m_panelMatrix->m_panels[panel].pixelMap[(y * m_panelWidth + x) * 3 + 2]]];
                    
                    m_canvas->SetPixel(px, y + (output * m_panelHeight), r, g, b);
                    
                    px++;
                }
            }
        }
    }
}

/*
 *
 */
int RGBMatrixOutput::SendData(unsigned char *channelData)
{
	LogExcess(VB_CHANNELOUT, "RGBMatrixOutput::RawSendData(%p)\n",
		channelData);

    m_canvas = m_rgbmatrix->SwapOnVSync(m_canvas);
	return m_channelCount;
}

/*
 *
 */
void RGBMatrixOutput::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "RGBMatrixOutput::DumpConfig()\n");
	LogDebug(VB_CHANNELOUT, "    panels : %d\n", m_panels);
	LogDebug(VB_CHANNELOUT, "    width  : %d\n", m_width);
	LogDebug(VB_CHANNELOUT, "    height : %d\n", m_height);
}

