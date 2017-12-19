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

#ifndef _LEDSCAPEMATRIX_H
#define _LEDSCAPEMATRIX_H

#include <string>

#include "ledscape.h"

#include "ChannelOutputBase.h"
#include "Matrix.h"

class LEDscapeMatrixOutput : public ChannelOutputBase {
    enum ColorOrder {
        RGB,
        RBG,
        GRB,
        GBR,
        BGR,
        BRG
    };
    
    
  public:
	LEDscapeMatrixOutput(unsigned int startChannel, unsigned int channelCount);
	~LEDscapeMatrixOutput();

	int Init(Json::Value config);
	int Close(void);

	void PrepData(unsigned char *channelData);

	int RawSendData(unsigned char *channelData);

	void DumpConfig(void);

  private:
    static ColorOrder mapColorOrder(const std::string &v);
	ledscape_config_t  *m_config;
	ledscape_t         *m_leds;

    ColorOrder          m_colorOrder;

	int                 m_dataSize;
	uint8_t            *m_data;
	uint8_t             m_invertedData;

	Matrix             *m_matrix;
};

#endif /* _LEDSCAPEMATRIX_H */
