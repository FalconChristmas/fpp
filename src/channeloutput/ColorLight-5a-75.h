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
#include <netinet/in.h>
#include <sys/uio.h>
#include <string>
#include <vector>

#include "ChannelOutput.h"
#include "ColorOrder.h"
#include "Matrix.h"
#include "PanelMatrix.h"

#define CL_MAX_PIXL_PER_PACKET 497
#define CL_MAX_CHAN_PER_PACKET (CL_MAX_PIXL_PER_PACKET * 3)

#define CL_PACKET_TYPE_OFFSET 12
#define CL_PACKET_DATA_OFFSET 13

#define CL_SYNC_PACKET_TYPE 0x01 // sync memory to outputs
#define CL_SYNC_PACKET_SIZE 112
#define CL_DISC_PACKET_TYPE 0x07 // discover receivers
#define CL_DISC_PACKET_SIZE 284
#define CL_RESP_PACKET_TYPE 0x08 // receiver response to discovery
#define CL_RESP_PACKET_SIZE 1070
#define CL_BRIG_PACKET_TYPE 0x0A // brightness
#define CL_BRIG_PACKET_SIZE 77
#define CL_PIXL_PACKET_TYPE 0x55 // row data
#define CL_PIXL_HEADER_SIZE 8

typedef struct {
    int model;
    int id;
    int majorFirmwareVersion;
    int minorFirmwareVersion;
    int width;
    int height;
    int xOffset;
    int yOffset;

    unsigned int uptime;
    unsigned int packets;
} ColorLightReceiver;

class ColorLight5a75Output : public ChannelOutput {
public:
    ColorLight5a75Output(unsigned int startChannel, unsigned int channelCount);
    virtual ~ColorLight5a75Output();

    virtual std::string GetOutputType() const override {
        return "ColorLight Panels";
    }

    virtual int Init(Json::Value config) override;
    virtual int Close(void) override;

    virtual void PrepData(unsigned char* channelData) override;
    virtual int SendData(unsigned char* channelData) override;

    virtual void DumpConfig(void) override;

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) override;

    virtual void OverlayTestData(unsigned char* channelData, int cycleNum, float percentOfCycle, int testType, const Json::Value& config) override;
    virtual bool SupportsTesting() const override { return true; }

    void GetReceiverInfo();

private:
    void SetHostMACs(void* data);
    int SendMessagesHelper(struct mmsghdr* msgs, int cnt);
    int SendMessages(std::vector<struct mmsghdr>& msgsToSend);

    int m_width;
    int m_height;
    std::string m_layout;
    std::string m_ifName;
    std::string ifspeed_src;
    int ifspeed;

    FPPColorOrder m_colorOrder;

    int m_fd;
    int m_rfd;
    int m_rowSize;

    int m_slowCount;

    int m_panelWidth;
    int m_panelHeight;
    int m_panels;
    int m_rows;
    int m_outputs;
    int m_longestChain;
    int m_invertedData;
    char* m_outputFrame;
    Matrix* m_matrix;
    PanelMatrix* m_panelMatrix;
    uint8_t m_gammaCurve[256];
    int m_flippedLayout;
    bool m_colorlightDisable;

    int m_highestFirmwareVersion;

    std::vector<ColorLightReceiver> m_receivers;

    // Display Frame
    std::vector<struct mmsghdr> m_syncMsgs;
    std::vector<struct iovec> m_synciovecs;

    // Pixel Data, etc.
    std::vector<struct mmsghdr> m_msgs;
    std::vector<struct iovec> m_iovecs;

    struct ifreq m_if_idx;
    struct ifreq m_if_mac;
    struct ether_header* m_eh;
    struct sockaddr_ll m_sock_addr;

    std::string m_autoCreatedModelName;
    std::list<std::string> m_warnings;
};
