
#include <string.h>

#include "UDPOutput.h"
#include "GenericUDPOutput.h"
#include "log.h"
#include "Warnings.h"

extern "C" {
    GenericUDPOutput *createGenericUDPOutput(unsigned int startChannel,
                            unsigned int channelCount) {
        return new GenericUDPOutput(startChannel, channelCount);
    }
}

static const std::string GENERIC_UDP_TYPE = "GenericUDP";

class GenericUDPOutputData : public UDPOutputData {
public:
    GenericUDPOutputData(const Json::Value &config) : UDPOutputData(config), count(0), isMulticast(false), isBroadcast(false) {
        tokens = config["tokens"].asString();
        port = config["port"].asInt();

        udpAddress.sin_family = AF_INET;
        udpAddress.sin_port = htons(port);
        udpAddress.sin_addr.s_addr = toInetAddr(ipAddress, valid);
        if (!valid) {
            WarningHolder::AddWarning("Could not resolve host name " + ipAddress + " - disabling output");
            active = false;
        }
        
        int fp = (udpAddress.sin_addr.s_addr >> 24) & 0xFF;
        printf("ip: %s    %X\n", ipAddress.c_str(), udpAddress.sin_addr.s_addr);
        printf("fp: %d\n", fp);
        printf("fp2: %d\n", udpAddress.sin_addr.s_addr & 0xFF);
        if (fp >= 239 && fp <=  224) {
            isMulticast = true;
        } else if (fp == 255) {
            isBroadcast = true;
        }
        
        std::string nt = tokens;
        
        std::string token;
        std::vector<uint8_t> bytes;
        while (!nt.empty()) {
            std::string::size_type pos = nt.find_first_of(' ');
            if (pos < nt.size()) {
                token = nt.substr(0, pos);
                nt = nt.substr(pos + 1);
            } else {
                token = nt;
                nt = "";
            }
            if (token.size() > 2) {
                if (token[0] == '{') {
                    if (token == "{data}") {
                        if (bytes.size() > 0) {
                            udpBuffers.push_back(bytes);
                        }
                        channelIovecs.push_back(udpBuffers.size());
                        bytes.clear();
                        udpBuffers.push_back(bytes);
                    } else if (token == "{P}" || token == "{p}" || token == "{P1}" || token == "{p1}") {
                        bytes.push_back(channelCount / 3);
                    } else if (token == "{S}" || token == "{s}" || token == "{S1}" || token == "{s1}") {
                        bytes.push_back(channelCount);
                    } else if (token == "{S2}") {
                        addBigEndian(bytes, channelCount);
                    } else if (token == "{s2}") {
                        addLittleEndian(bytes, channelCount);
                    } else if (token == "{P2}") {
                        addBigEndian(bytes, channelCount / 3);
                    } else if (token == "{p2}") {
                        addLittleEndian(bytes, channelCount / 3);
                    } else {
                        printf("Unknown token -%s-", token.c_str());
                        for (int x = 0; x < token.size(); x++) {
                            bytes.push_back((uint8_t)token[x]);
                        }
                    }
                } else if (token[0] == '0' && token[1] == 'x') {
                    int i = std::strtoul(token.c_str(), 0, 16);
                    bytes.push_back(i);
                } else if (token[0] == '#') {
                    int i = std::strtoul(token.substr(1).c_str(), 0, 16);
                    bytes.push_back(i);
                }
            } else {
                for (int x = 0; x < token.size(); x++) {
                    bytes.push_back((uint8_t)token[x]);
                }
            }
        }
        if (!bytes.empty()) {
            udpBuffers.push_back(bytes);
        }
        udpIovecs.resize(udpBuffers.size());
        for (int x = 0; x < udpBuffers.size(); x++) {
            udpIovecs[x].iov_base = &udpBuffers[x][0];
            udpIovecs[x].iov_len = udpBuffers[x].size();
        }
        for (auto idx : channelIovecs) {
            udpIovecs[idx].iov_len = channelCount;
        }
    }
    virtual ~GenericUDPOutputData() {
    }
    
    virtual const std::string &GetOutputTypeString() const override {
        return GENERIC_UDP_TYPE;
    }

    void addLittleEndian(std::vector<uint8_t> &bytes, int v) {
        int a = v & 0xFF;
        int b = (v >> 8) & 0xFF;
        bytes.push_back(b);
        bytes.push_back(a);
    }
    void addBigEndian(std::vector<uint8_t> &bytes, int v) {
        int a = v & 0xFF;
        int b = (v >> 8) & 0xFF;
        bytes.push_back(a);
        bytes.push_back(b);
    }
    virtual bool IsPingable() override {
        return !isMulticast && !isBroadcast;
    }

    virtual void PrepareData(unsigned char *channelData,
                             UDPOutputMessages &msgs) {
        if (valid && active && NeedToOutputFrame(channelData, startChannel - 1, 0 , channelCount)) {
            struct mmsghdr msg;
            memset(&msg, 0, sizeof(msg));

            msg.msg_hdr.msg_name = &udpAddress;
            msg.msg_hdr.msg_namelen = sizeof(sockaddr_in);
            msg.msg_hdr.msg_iov = &udpIovecs[0];
            msg.msg_hdr.msg_iovlen = udpIovecs.size();
            msg.msg_len = 0;
            for (int x = 0; x < udpIovecs.size(); x++) {
                msg.msg_len += udpIovecs[x].iov_len;
            }
            
            count++;
            int start = startChannel - 1;
            for (auto idx : channelIovecs) {
                udpIovecs[idx].iov_base = (void*)(channelData + start);
            }

            if (isBroadcast) {
                msgs[BROADCAST_MESSAGES_KEY].push_back(msg);
            } else {
                msgs[udpAddress.sin_addr.s_addr].push_back(msg);
            }
            SaveFrame(channelData);
        } else {
            skippedFrames++;
        }
    }

    virtual void DumpConfig() override {
    }
    
    int port;
    int count;
    
    bool isMulticast;
    bool isBroadcast;
    
    std::string tokens;
    
    sockaddr_in udpAddress;
    std::vector<int> channelIovecs;
    std::vector<struct iovec> udpIovecs;
    std::vector<std::vector<uint8_t>> udpBuffers;
};


GenericUDPOutput::GenericUDPOutput(unsigned int startChannel, unsigned int channelCount)
: ChannelOutputBase(startChannel, channelCount)
{
}
GenericUDPOutput::~GenericUDPOutput() {
    
}

int GenericUDPOutput::Init(Json::Value config) {
    if (UDPOutput::INSTANCE) {
        config.removeMember("type");
        GenericUDPOutputData *od = new GenericUDPOutputData(config);
        UDPOutput::INSTANCE->addOutput(od);
    } else {
        LogErr(VB_CHANNELOUT, "GenericUDPOutput Requires the UDP outputs to be enabled\n");
    }
    return ChannelOutputBase::Init(config);
}
int GenericUDPOutput::Close(void) {
    return ChannelOutputBase::Close();
}

void GenericUDPOutput::GetRequiredChannelRanges(const std::function<void(int, int)> &addRange) {
    addRange(m_startChannel, m_startChannel + m_channelCount - 1);
}

int GenericUDPOutput::SendData(unsigned char *channelData) {
    return m_channelCount;
}
