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

#ifndef _BBBMATRIX_H
#define _BBBMATRIX_H

#include <string>

#include "Matrix.h"
#include "PanelMatrix.h"
#include "BBBPruUtils.h"

#include "ChannelOutputBase.h"

//16 rows (1/16 scan) * 8bits per row
#define MAX_STATS 16 * 8


class InterleaveHandler;

// structure of the data at the start of the PRU ram
// that the pru program expects to see
typedef struct {
    // in the DDR shared with the PRU
    uintptr_t address_dma;
    
    // write 1 to start, 0xFF to abort. will be cleared when started
    volatile unsigned command;
    volatile unsigned response;
    
    uint32_t stats[3 * MAX_STATS]; //3 values per collection
} __attribute__((__packed__)) BBBPruMatrixData;


class BBBMatrix : public ChannelOutputBase {
  public:
    enum ScollerPinout {
        V1 = 1,
        V2,
        POCKETSCROLLERv1
    };
    
    BBBMatrix(unsigned int startChannel, unsigned int channelCount);
    virtual ~BBBMatrix();
    
    int Init(Json::Value config);
    int Close(void);
    
    void PrepData(unsigned char *channelData);
    
    int RawSendData(unsigned char *channelData);
    
    void DumpConfig(void);
    
    virtual void GetRequiredChannelRange(int &min, int & max);

  private:
    void calcBrightnessFlags(std::vector<std::string> &sargs);
    void printStats();
    
    BBBPru      *m_pru;
    BBBPru      *m_pruCopy;
    BBBPruMatrixData *m_pruData;
    Matrix      *m_matrix;
    PanelMatrix *m_panelMatrix;
    
    int          m_panelWidth;
    int          m_panelHeight;
    int          m_outputs;
    int          m_longestChain;
    int          m_invertedData;
    int          m_brightness;
    int          m_colorDepth;
    int          m_interleave;
    bool         m_printStats;
    int          m_panelScan; 
    FPPColorOrder m_colorOrder;

    uint8_t      *m_outputFrame;
    int          m_panels;
    int          m_rows;
    int          m_width;
    int          m_height;
    int          m_rowSize;
    
    bool         m_evenFrame;
    
    ScollerPinout m_pinout;
    InterleaveHandler *m_handler;
    
    uint32_t     brightnessValues[8];
    uint32_t     delayValues[8];
    uint8_t      gammaCurve[256];
    
};

#endif
