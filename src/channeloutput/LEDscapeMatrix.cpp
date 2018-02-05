/*
 *   LEDscape Matrix handler for Falcon Pi Player (FPP)
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
#include <strings.h>
#include <unistd.h>
#include <stdio.h>
#include <vector>
#include <sys/wait.h>

#include "pru.h"

#include "common.h"
#include "log.h"
#include "LEDscapeMatrix.h"
#include "settings.h"
#include "BBBUtils.h"

/////////////////////////////////////////////////////////////////////////////


/*
 *
 */
LEDscapeMatrixOutput::LEDscapeMatrixOutput(unsigned int startChannel,
	unsigned int channelCount)
  : ChannelOutputBase(startChannel, channelCount),
	m_config(NULL),
	m_leds(NULL),
	m_colorOrder(RGB),
	m_dataSize(0),
	m_data(NULL),
	m_invertedData(0),
        m_pinout(V1)
{
	LogDebug(VB_CHANNELOUT, "LEDscapeMatrixOutput::LEDscapeMatrixOutput(%u, %u)\n",
		startChannel, channelCount);
}

/*
 *
 */
LEDscapeMatrixOutput::~LEDscapeMatrixOutput()
{
	LogDebug(VB_CHANNELOUT, "LEDscapeMatrixOutput::~LEDscapeMatrixOutput()\n");

	delete m_matrix;
}


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
    configBBBPin("P1_33", 3, 15, "gpio");  //CLOCK

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
static void calcBrightness(ledscape_t *leds, int brightness, int maxPanel, int maxOutput, int rowsPerOutput,
                           int panelHeight, int panelWidth, LEDscapeMatrixOutput::ScollerPinout ver) {
    LogDebug(VB_CHANNELOUT, "Calc Brightness:   maxPanel:  %d    maxOutput: %d     Brightness: %d    rpo: %d    ph:  %d    pw:  %d\n", maxPanel, maxOutput, brightness, rowsPerOutput, panelHeight, panelWidth);
    
    
    uint32_t max = 0xB00;
    if (ver == LEDscapeMatrixOutput::V1) {
        if (maxOutput == 1) {
            max = 0x500;
        } else if (maxOutput < 4) {
            max = 0xA00;
        } else if (maxOutput < 6) {
            max = 0xB00;
        } else {
            max = 0xD00;
        }
    } else if (ver == LEDscapeMatrixOutput::V2) {
        max = (maxOutput < 4) ? 0xB00 : 0xD00;
    } else if (ver == LEDscapeMatrixOutput::POCKETSCROLLERv1) {
        max = 0xB00;
    }
    max *= maxPanel;
    max *= panelWidth;
    max /= 32;
    max += maxOutput * 0x200;
    if (ver == LEDscapeMatrixOutput::V1) {
        if (maxOutput > 3) {
            //jumping above output 3 adds increased delay
            max += 0x300;
        }
    } else if (ver == LEDscapeMatrixOutput::V2) {
        if (maxOutput > 1) {
            //jumping above output 1 adds increased delay
            max += 0x300;
        }
        if (maxOutput > 3) {
            //jumping above output 3 adds increased delay
            max += 0x300;
        }
        if (maxOutput > 5) {
            //jumping above output 3 adds increased delay
            max += 0x300;
        }
    } else if (ver == LEDscapeMatrixOutput::POCKETSCROLLERv1) {
        if (maxOutput == 1) {
            max /= 2;
        }
        if (maxOutput > 1) {
            //jumping above output 1 adds increased delay
            max += 0x300;
        }
        if (maxOutput > 2) {
            //jumping above output 2 adds increased delay
            max += 0x300;
        }
    }

    // 1/4 scan we need to double the time since we have twice the number of pixels to clock out
    max *= panelHeight;
    max /= (rowsPerOutput * 2);
    uint32_t origMax = max * 90 / 100; //max's are calced at roughly 10% over needed

    if (max < 0x2000) {
        //if max is too low, the low bit time is too short and
        //extra ghosting occurs
        // At this point, framerate will be supper high anyway >100fps
        max = 0x2000;
    }
    uint32_t origMax2 = max;
    
    max *= brightness;
    max /= 10;
    
    uint32_t delay = origMax2 - max;
    if (max < origMax) {
        delay = origMax2 - origMax;
    }
    
    for (int x = 0; x < 8; x++) {
        LogDebug(VB_CHANNELOUT, "Brightness %d:  %X\n", x, max);
        
        leds->ws281x->brightInfo[2*x + 1] = delay;
        leds->ws281x->brightInfo[2*x] = max + delay;
        max >>= 1;
        origMax >>= 1;
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
                    leds->ws281x->statEnable = atoi(line);
                    count++;
                } else {
                    uint32_t d1, d2;
                    sscanf(line, "%X %X", &d1, &d2);
                    leds->ws281x->brightInfo[2 * ( count - 1)] = d1;
                    leds->ws281x->brightInfo[2 * ( count - 1) + 1] = d2;
                    count++;
                }
                line = buf;
                len = 100;
            }
            fclose(file);
        }
    }
    //leds->ws281x->statEnable = 1;
}


/*
 *
 */
int LEDscapeMatrixOutput::Init(Json::Value config)
{
	LogDebug(VB_CHANNELOUT, "LEDscapeMatrixOutput::Init(JSON)\n");

	m_config = reinterpret_cast<ledscape_config_t*>(calloc(1, sizeof(ledscape_config_t)));
	if (!m_config)
	{
		LogErr(VB_CHANNELOUT, "Unable to allocate LEDscape config\n");
		return 0;
	}

	int maxWidth = 0;
	int maxHeight = 0;
    int maxOutput = 0;
    int maxPanel = 0;

	ledscape_matrix_config_t * const lmconfig = &m_config->matrix_config;
    for (int chain = 0; chain < LEDSCAPE_MATRIX_PANELS; chain++) {
        for (int output = 0; output < LEDSCAPE_MATRIX_OUTPUTS; output++) {
            ledscape_matrix_panel_t * const pconfig =
                &lmconfig->panels[output][chain];
            pconfig->enabled = 0;
        }
    }

	lmconfig->type         = LEDSCAPE_MATRIX;
	lmconfig->panel_width  = config["panelWidth"].asInt();
	lmconfig->panel_height = config["panelHeight"].asInt();

	m_invertedData = config["invertedData"].asInt();
	m_colorOrder = mapColorOrder(config["colorOrder"].asString());

	if (!lmconfig->panel_width)
		lmconfig->panel_width = 32;

	if (!lmconfig->panel_height)
		lmconfig->panel_height = 16;

	lmconfig->leds_height  = lmconfig->panel_height * 8;
    
    for (int i = 0; i < config["panels"].size(); i++)
    {
        Json::Value p = config["panels"][i];
        if (p["panelNumber"].asInt() > maxPanel)
            maxPanel = p["panelNumber"].asInt();
    }
    if (maxPanel >= LEDSCAPE_MATRIX_PANELS) {
        lmconfig->maxPanel = LEDSCAPE_MATRIX_PANELS;
        maxPanel = LEDSCAPE_MATRIX_PANELS - 1;
    } else if (maxPanel < LEDSCAPE_MATRIX_PANELS) {
        lmconfig->maxPanel = maxPanel + 1;
        if (lmconfig->maxPanel < LEDSCAPE_MATRIX_PANELS) {
            // have one blank panel if possible, helps with the low bits
            lmconfig->maxPanel++;
        }
    }
    lmconfig->leds_width = lmconfig->panel_width * lmconfig->maxPanel;

    maxPanel = 0;
	for (int i = 0; i < config["panels"].size(); i++)
	{
		char orientation = 'N';
		int  pWidth  = lmconfig->panel_width;
		int  pHeight = lmconfig->panel_height;

		Json::Value p = config["panels"][i];

		int  output  = p["outputNumber"].asInt();
		int  chain   = lmconfig->maxPanel - 1 - p["panelNumber"].asInt(); // 0 is last
		int  xOffset = p["xOffset"].asInt();
		int  yOffset = p["yOffset"].asInt();
        
        if (output > maxOutput)
            maxOutput = output;

        if (p["panelNumber"].asInt() > maxPanel)
            maxPanel = p["panelNumber"].asInt();

		ledscape_matrix_panel_t * const pconfig =
			&lmconfig->panels[output][chain];

		pconfig->x   = xOffset;
		pconfig->y   = yOffset;
		pconfig->rot = 0; // Default, normal rotation
        pconfig->enabled = true;

		const char *o = p["orientation"].asString().c_str();
		if (o && *o)
			orientation = o[0];

		switch (orientation)
		{
			case 'N':
				pconfig->rot = 0;
				break;
			case 'L':
				pconfig->rot = 1;
				pWidth = lmconfig->panel_height;
				pHeight = lmconfig->panel_width;
				break;
			case 'R':
				pconfig->rot = 2;
				pWidth = lmconfig->panel_height;
				pHeight = lmconfig->panel_width;
				break;
			case 'U':
				pconfig->rot = 3;
				break;
		}

		if ((xOffset + pWidth) > maxWidth)
			maxWidth = xOffset + pWidth;

		if ((yOffset + pHeight) > maxHeight)
			maxHeight = yOffset + pHeight;
	}

	lmconfig->width = maxWidth;
	lmconfig->height = maxHeight;
    
    int brightness = 7;
    if (config.isMember("brightness"))
        brightness = config["brightness"].asInt();
    if (brightness < 1 || brightness > 10) {
        brightness = 7;
    }
    lmconfig->bitsToOutput = 8;
    if (config.isMember("panelColorDepth")) {
        LogDebug(VB_CHANNELOUT, "LEDscapeMatrixOutput::Init(JSON) has cd\n");
        lmconfig->bitsToOutput = config["panelColorDepth"].asInt();
        LogDebug(VB_CHANNELOUT, "LEDscapeMatrixOutput::Init(JSON)  cd: %d\n", lmconfig->bitsToOutput);
    }
    if (lmconfig->bitsToOutput > 8 || lmconfig->bitsToOutput < 6) {
        lmconfig->bitsToOutput = 8;
    }
    lmconfig->rowsPerOutput = config["panelScan"].asInt();
    
    if (config.isMember("panelInterleave")) {
        lmconfig->interleavePixels = config["panelInterleave"].asInt();
    } else {
        lmconfig->interleavePixels = 0;
    }
    
    lmconfig->initialSkip = (lmconfig->maxPanel - maxPanel - 1) * 6 * lmconfig->panel_width;
    if (lmconfig->rowsPerOutput == 0) {
        lmconfig->rowsPerOutput = 8;
    }
    if ((lmconfig->rowsPerOutput * 4) == lmconfig->panel_height) {
        lmconfig->initialSkip *= 2;
    } else if ((lmconfig->rowsPerOutput * 8) == lmconfig->panel_height) {
        lmconfig->initialSkip *= 4;
    }

	m_dataSize = lmconfig->width * lmconfig->height * 4;
	m_data = (uint8_t *)malloc(m_dataSize);

	if (!m_data)
	{
		LogErr(VB_CHANNELOUT, "Unable to initialize data buffer\n");

		return 0;
	}

    
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

 
    char outputString[10];
    sprintf(outputString, "%d", (maxOutput + 1));

    char outputStringCompiler[200];
    sprintf(outputStringCompiler, "-DOUTPUTS=%d", (maxOutput + 1));
    compileArgs.push_back(outputStringCompiler);

    compilePRUMatrixCode(compileArgs);

    std::string pru_program = "/tmp/FalconMatrix.bin";

    LogDebug(VB_CHANNELOUT, "Using program %s with brightness %d\n", pru_program.c_str(), brightness);

	m_leds = ledscape_matrix_init(m_config, 0, 0, pru_program.c_str());

    LogDebug(VB_CHANNELOUT, "   pru dram:  %X  %d       %X   %d      %d\n",
             m_leds->pru->data_ram, m_leds->pru->data_ram_size,
             m_leds->pru->ddr, m_leds->pru->ddr_size,
             m_leds->frame_size);

    m_leds->ws281x->statEnable = 0;
    calcBrightness(m_leds, brightness, maxPanel + 1, maxOutput + 1, lmconfig->rowsPerOutput,
                   lmconfig->panel_height, lmconfig->panel_width,
                   m_pinout);
    
    m_leds->ws281x->num_pixels = lmconfig->maxPanel * lmconfig->panel_width / 8;
    if ((lmconfig->rowsPerOutput * 4) ==  lmconfig->panel_height) {
        // 1/4 scan output 2 rows at once in a strange 8 then 8 then 8 then 8... pattern
        m_leds->ws281x->num_pixels *= 2;
    } else if ((lmconfig->rowsPerOutput * 8) ==  lmconfig->panel_height) {
        // 1/2 scan output 4 rows at once in a strange 8 then 8 then 8 then 8... pattern
        m_leds->ws281x->num_pixels *= 4;
    }

    LogDebug(VB_CHANNELOUT, "Pixels per row %d,    rowsPerOutput: %d     panelHeight: %d    colorDepth: %d   initialSkip: %d\n",
             m_leds->ws281x->num_pixels, lmconfig->rowsPerOutput, lmconfig->panel_height,
             lmconfig->bitsToOutput, lmconfig->initialSkip);

	if (!m_leds)
	{
		LogErr(VB_CHANNELOUT, "Unable to initialize LEDscape\n");
		return 0;
	}
    LogDebug(VB_CHANNELOUT, "Program started %s\n", pru_program.c_str());

    
	m_matrix = new Matrix(m_startChannel, maxWidth, maxHeight);

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

	return ChannelOutputBase::Init(config);
}

/*
 *
 */
int LEDscapeMatrixOutput::Close(void)
{
	LogDebug(VB_CHANNELOUT, "LEDscapeMatrixOutput::Close()\n");

	ledscape_close(m_leds);

        if (m_pinout == POCKETSCROLLERv1) {
            resetPSPins();
        }
	free(m_data);
	m_data = NULL;

	free(m_config);
	m_config = NULL;

	return ChannelOutputBase::Close();
}

/*
 *
 */
void LEDscapeMatrixOutput::PrepData(unsigned char *channelData)
{
	m_matrix->OverlaySubMatrices(channelData);
}


LEDscapeMatrixOutput::ColorOrder LEDscapeMatrixOutput::mapColorOrder(const std::string &colorOrder) {
    if (colorOrder == "RGB")
        return RGB;
    if (colorOrder == "RBG")
        return RBG;
    if (colorOrder == "GRB")
        return GRB;
    if (colorOrder == "GBR")
        return GBR;
    if (colorOrder == "BRG")
        return BRG;
    if (colorOrder == "BGR")
        return BGR;
    if (colorOrder == "BRG")
        return BRG;
    return RGB;
}


/*
 *
 */
int LEDscapeMatrixOutput::RawSendData(unsigned char *channelData)
{
	LogExcess(VB_CHANNELOUT, "LEDscapeMatrixOutput::RawSendData(%p)\n",
		channelData);

	bzero(m_data, m_dataSize);

	ledscape_matrix_config_t *config = reinterpret_cast<ledscape_matrix_config_t*>(m_config);

	unsigned char *c = m_data;
	unsigned char *r = channelData;
	unsigned char *g = r + 1;
	unsigned char *b = r + 2;

	for (int y = 0; y < config->height; y++)
	{
		if (m_invertedData)
		{
			r = channelData + ((config->height - 1 - y) * config->width * 3);
		}
		else
		{
			r = channelData + (y * config->width * 3);
		}

		g = r + 1;
		b = r + 2;

		for (int x = 0; x < config->width; x++)
		{
            switch (m_colorOrder) {
                case RGB:
                    *(c++) = *b;
                    *(c++) = *g;
                    *(c++) = *r;
                    break;
                case RBG:
                    *(c++) = *g;
                    *(c++) = *b;
                    *(c++) = *r;
                    break;
                case GRB:
                    *(c++) = *b;
                    *(c++) = *r;
                    *(c++) = *g;
                    break;
                case GBR:
                    *(c++) = *r;
                    *(c++) = *b;
                    *(c++) = *g;
                    break;
                case BRG:
                    *(c++) = *g;
                    *(c++) = *r;
                    *(c++) = *b;
                    break;
                case BGR:
                    *(c++) = *r;
                    *(c++) = *g;
                    *(c++) = *b;
                    break;
                default:
                    *(c++) = *b;
                    *(c++) = *g;
                    *(c++) = *r;
                    break;
            }

			c++;
			r += 3;
			g += 3;
			b += 3;
		}
	}

	//long long drawTime = 0;
	//long long waitTime = 0;
	//long long waitStartTime = 0;
	//long long startTime = GetTime();

	ledscape_draw(m_leds, m_data);
	//drawTime = GetTime();
	//usleep(5000);
	//waitStartTime = GetTime();
	//const uint32_t response = ledscape_wait(m_leds);
	//waitTime = GetTime();

	//LogDebug(VB_CHANNELOUT, "V: %02x, Draw Time: %lldus, Wait Time: %lldus (+ 5ms)\n",
	//	channelData[0], drawTime - startTime, waitTime - waitStartTime);

	return m_channelCount;
}

/*
 *
 */
void LEDscapeMatrixOutput::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "LEDscapeMatrixOutput::DumpConfig()\n");

	ledscape_matrix_config_t *config = reinterpret_cast<ledscape_matrix_config_t*>(m_config);
	
	LogDebug(VB_CHANNELOUT, "    width   : %d\n", config->width);
	LogDebug(VB_CHANNELOUT, "    height  : %d\n", config->height);

//	LogDebug(VB_CHANNELOUT, "    panels : %d (%dx%d)\n",
//		m_panels, m_panelsWide, m_panelsHigh);
//	LogDebug(VB_CHANNELOUT, "    rows   : %d\n", m_rows);
}

