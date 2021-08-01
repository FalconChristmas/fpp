/*
 *   KiNet Channel Output driver for Falcon Player (FPP)
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
 *
 *   ***********************************************************************
 *
 */

#include "fpp-pch.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>

#include "KiNet.h"

#define KINET_V1_PACKET_HEADERLEN 21
#define KINET_V2_PACKET_HEADERLEN 24

#define KINET_V1_TYPE 6
#define KINET_V2_TYPE 7

static const uint8_t V1_HEADER[KINET_V1_PACKET_HEADERLEN] = {
    0x04, 0x01, 0xdc, 0x4a, // magic[4] 0401dc4a
    0x01, 0x00,             // ver[2]  0100
    0x01, 0x01,             // type[2]
    0x00, 0x00, 0x00, 0x00, // seq[4]
    0x00,                   // port
    0x00,                   // padding
    0x00, 0x00,             // flags
    0xFF, 0xFF, 0xFF, 0xFF, // time
    0x00                    // uni
};
static const uint8_t V2_HEADER[KINET_V2_PACKET_HEADERLEN] = {
    0x04, 0x01, 0xdc, 0x4a, // magic[4] 0401dc4a
    0x01, 0x00,             // ver[2]  0100
    0x08, 0x01,             // type[2]
    0x00, 0x00, 0x00, 0x00, // seq[4]
    0xFF, 0xFF, 0xFF, 0xFF, // time
    0x00,                   // port
    0x00,                   // sequenceNum
    0x00, 0x00,             // flags?
    0x00, 0x02,
    0x00, 0x00
};

static const std::string KINETTYPE = "KiNet";

const std::string& KiNetOutputData::GetOutputTypeString() const {
    return KINETTYPE;
}

KiNetOutputData::KiNetOutputData(const Json::Value& config) :
    UDPOutputData(config) {
    memset((char*)&kinetAddress, 0, sizeof(sockaddr_in));
    kinetAddress.sin_family = AF_INET;
    kinetAddress.sin_port = htons(KINET_PORT);
    kinetAddress.sin_addr.s_addr = toInetAddr(ipAddress, valid);

    if (!valid && active) {
        WarningHolder::AddWarning("Could not resolve host name " + ipAddress + " - disabling output");
        active = false;
    }

    port = config["id"].asInt();
    if (config.isMember("universeCount")) {
        portCount = config["universeCount"].asInt();
    }
    if (portCount < 1) {
        portCount = 1;
    }
    if (portCount > 255) {
        portCount = 255;
    }
    if (channelCount > 512) {
        channelCount = 512;
    }

    kinetIovecs = (struct iovec*)calloc(portCount * 2, sizeof(struct iovec));
    kinetBuffers = (unsigned char**)calloc(portCount, sizeof(unsigned char*));

    int chan = startChannel - 1;
    for (int x = 0; x < portCount; x++) {
        if (type == KINET_V1_TYPE) {
            kinetBuffers[x] = (unsigned char*)malloc(KINET_V1_PACKET_HEADERLEN);
            memcpy(kinetBuffers[x], V1_HEADER, KINET_V1_PACKET_HEADERLEN);
            kinetBuffers[x][12] = port + x;
        } else {
            kinetBuffers[x] = (unsigned char*)malloc(KINET_V2_PACKET_HEADERLEN);
            memcpy(kinetBuffers[x], V2_HEADER, KINET_V2_PACKET_HEADERLEN);
            kinetBuffers[x][16] = port + x;
        }

        // use scatter/gather for the packet.   One IOV will contain
        // the header, the second will point into the raw channel data
        // and will be set at output time.   This avoids any memcpy.
        kinetIovecs[x * 2].iov_base = kinetBuffers[x];
        kinetIovecs[x * 2].iov_len = type == KINET_V1_TYPE ? KINET_V1_PACKET_HEADERLEN : KINET_V2_PACKET_HEADERLEN;
        kinetIovecs[x * 2 + 1].iov_base = nullptr;
        kinetIovecs[x * 2 + 1].iov_len = channelCount;

        chan += channelCount;
    }
}
KiNetOutputData::~KiNetOutputData() {
    for (int x = 0; x < portCount; x++) {
        free(kinetBuffers[x]);
    }
    free(kinetBuffers);
    free(kinetIovecs);
}

void KiNetOutputData::GetRequiredChannelRange(int& min, int& max) {
    min = startChannel - 1;
    max = startChannel + (channelCount * portCount) - 1;
}

void KiNetOutputData::PrepareData(unsigned char* channelData, UDPOutputMessages& msgs) {
    if (valid && active) {
        int start = 0;
        struct mmsghdr msg;
        memset(&msg, 0, sizeof(msg));

        msg.msg_hdr.msg_name = &kinetAddress;
        msg.msg_hdr.msg_namelen = sizeof(sockaddr_in);
        bool skipped = false;
        bool allSkipped = true;
        for (int p = 0; p < portCount; p++) {
            bool nto = NeedToOutputFrame(channelData, startChannel - 1, start, kinetIovecs[p * 2 + 1].iov_len);
            if (nto) {
                msg.msg_hdr.msg_iov = &kinetIovecs[p * 2];
                msg.msg_hdr.msg_iovlen = 2;
                msg.msg_len = kinetIovecs[p * 2 + 1].iov_len + kinetIovecs[p * 2].iov_len;

                msgs[kinetAddress.sin_addr.s_addr].push_back(msg);

                if (type == KINET_V2_TYPE) {
                    kinetBuffers[p][17]++;
                }
                // set the pointer to the channelData for the universe
                kinetIovecs[p * 2 + 1].iov_base = (void*)(&channelData[startChannel - 1 + start]);
                allSkipped = false;
            } else {
                skipped = true;
            }
            start += kinetIovecs[p * 2 + 1].iov_len;
        }
        if (skipped) {
            skippedFrames++;
        }
        if (!allSkipped) {
            SaveFrame(&channelData[startChannel - 1], start);
        }
    }
}
void KiNetOutputData::DumpConfig() {
    LogDebug(VB_CHANNELOUT, "KiNet: %s   %d:%d:%d:%d  %s\n",
             description.c_str(),
             active,
             startChannel,
             channelCount,
             type,
             ipAddress.c_str());
}
