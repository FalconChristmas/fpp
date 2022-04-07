#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the GPL v2 as described in the
 * included LICENSE.GPL file.
 */

#include <arpa/inet.h>
#include <net/if.h>
#include <string>
#include <vector>

#include "ChannelOutput.h"
#include "ColorOrder.h"
#include "Matrix.h"
#include "PanelMatrix.h"

#define LINSNRV9_BUFFER_SIZE 1486
#define LINSNRV9_HEADER_SIZE 32
#define LINSNRV9_DATA_SIZE 1440

class LinsnRV9Output : public ChannelOutput {
public:
    LinsnRV9Output(unsigned int startChannel, unsigned int channelCount);
    virtual ~LinsnRV9Output();

    virtual int Init(Json::Value config) override;
    virtual int Close(void) override;

    virtual void PrepData(unsigned char* channelData) override;
    virtual int SendData(unsigned char* channelData) override;

    virtual void DumpConfig(void) override;

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) override;

private:
    void HandShake(void);

    void GetSrcMAC(void);
    void SetHostMACs(void* data);
    void SetDiscoveryMACs(void* data);
    int Send(char* buffer, int len);

    int m_width;
    int m_height;
    std::string m_ifName;

    FPPColorOrder m_colorOrder;

    int m_fd;

    char m_buffer[LINSNRV9_BUFFER_SIZE];
    char* m_header;
    char* m_data;
    char* m_rowData;
    int m_pktSize;
    int m_framePackets;
    int m_frameNumber;

    struct ifreq m_if_idx;
    struct ifreq m_if_mac;
    struct ether_header* m_eh;
    struct sockaddr_ll m_sock_addr;

    int m_panelWidth;
    int m_panelHeight;
    int m_panels;
    int m_rows;
    int m_outputs;
    int m_longestChain;
    int m_invertedData;
    char* m_outputFrame;
    int m_outputFrameSize;
    Matrix* m_matrix;
    PanelMatrix* m_panelMatrix;
    int m_formatIndex;
    uint8_t m_gammaCurve[256];

    struct FormatCode {
        unsigned char code;
        int width;
        int height;
        int dataOffset;
        int d27;
    };

    std::vector<struct FormatCode> m_formatCodes;

    unsigned char m_srcMAC[6];
    unsigned char m_dstMAC[6];
};
