#include "fpp-pch.h"

#include "fpp-json.h"

#include "../Warnings.h"
#include "../log.h"

#include "GenericUDPOutput.h"
#include "UDPOutput.h"
#include "ChannelOutputSetup.h"

#include "Plugin.h"
class GenericUDPPlugin : public FPPPlugins::Plugin, public FPPPlugins::ChannelOutputPlugin {
public:
    GenericUDPPlugin() :
        FPPPlugins::Plugin("GenericUDP") {
    }
    virtual ChannelOutput* createChannelOutput(unsigned int startChannel, unsigned int channelCount) override {
        return new GenericUDPOutput(startChannel, channelCount);
    }
};

extern "C" {
FPPPlugins::Plugin* createPlugin() {
    return new GenericUDPPlugin();
}
}

static const std::string GENERIC_UDP_TYPE = "GenericUDP";

class GenericUDPOutputData : public UDPOutputData {
public:
    GenericUDPOutputData(const Json::Value& config) :
        UDPOutputData(config),
        count(0),
        isMulticast(false),
        isBroadcast(false) {
        tokens = config["tokens"].asString();
        port = config["port"].asInt();

        // s_addr is left unset when the output is inactive, so it has to start
        // zeroed: the address classification below reads it either way.
        memset(&udpAddress, 0, sizeof(udpAddress));
        udpAddress.sin_family = AF_INET;
        udpAddress.sin_port = htons(port);
        if (active) {
            udpAddress.sin_addr.s_addr = toInetAddr(ipAddress, valid);
            if (!valid) {
                AddOutputConfigWarning(14, "Could not resolve host name " + ipAddress + " - disabling output");
                active = false;
            }
        }

        // s_addr is network byte order, so the octets have to be read out of the
        // host-order value: on a little-endian host the top byte of the raw word
        // is the LAST octet, not the first.
        uint32_t hostAddr = ntohl(udpAddress.sin_addr.s_addr);
        int firstOctet = (hostAddr >> 24) & 0xFF;
        int lastOctet = hostAddr & 0xFF;
        if (firstOctet >= 224 && firstOctet <= 239) {
            isMulticast = true;
        } else if (lastOctet == 255) {
            // Directed (x.y.z.255) and limited (255.255.255.255) broadcast.  This
            // is matched deliberately - the old byte-swapped math happened to
            // catch the same addresses, so dropping it silently would take
            // working broadcast configs with it.
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
                        // Upper case is low byte first and lower case high
                        // byte first, as these have always gone on the wire;
                        // the helpers they called were named the other way
                        // round.
                        addLittleEndian(bytes, channelCount);
                    } else if (token == "{s2}") {
                        addBigEndian(bytes, channelCount);
                    } else if (token == "{P2}") {
                        addLittleEndian(bytes, channelCount / 3);
                    } else if (token == "{p2}") {
                        addBigEndian(bytes, channelCount / 3);
                    } else {
                        LogWarn(VB_CHANNELOUT, "Unknown token -%s-\n", token.c_str());
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
            // data(), not &[0]: the {data} placeholder is an empty vector
            udpIovecs[x].iov_base = udpBuffers[x].data();
            udpIovecs[x].iov_len = udpBuffers[x].size();
        }
        for (auto idx : channelIovecs) {
            udpIovecs[idx].iov_len = channelCount;
        }
    }
    virtual ~GenericUDPOutputData() {
    }

    virtual const std::string& GetOutputTypeString() const override {
        return GENERIC_UDP_TYPE;
    }

    void addLittleEndian(std::vector<uint8_t>& bytes, int v) {
        bytes.push_back(v & 0xFF);
        bytes.push_back((v >> 8) & 0xFF);
    }
    void addBigEndian(std::vector<uint8_t>& bytes, int v) {
        bytes.push_back((v >> 8) & 0xFF);
        bytes.push_back(v & 0xFF);
    }
    virtual bool IsPingable() override {
        return !isMulticast && !isBroadcast;
    }

    virtual void PrepareData(unsigned char* channelData,
                             UDPOutputMessages& msgs) override {
        if (valid && active && NeedToOutputFrame(channelData, startChannel - 1, 0, channelCount)) {
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
            SaveFrame(&channelData[startChannel - 1], channelCount);
            skippedFrames = 0;
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

GenericUDPOutput::GenericUDPOutput(unsigned int startChannel, unsigned int channelCount) :
    ChannelOutput(startChannel, channelCount) {
}
GenericUDPOutput::~GenericUDPOutput() {
    // deleted without Close() when Init() fails; a no-op otherwise
    UDPOutput::UnregisterExternalOutput(this);
}

int GenericUDPOutput::Init(Json::Value config) {
    if (!UDPOutput::INSTANCE) {
        LogErr(VB_CHANNELOUT, "GenericUDPOutput Requires the UDP outputs to be enabled\n");
    }
    config.removeMember("type");
    // Registered by owner, not handed over as data: the UDP output is rebuilt
    // whenever the E1.31/DDP config is saved and has to recreate this, and
    // Close() has to be able to take it back out.
    UDPOutput::RegisterExternalOutput(this, [config]() -> UDPOutputData* {
        return new GenericUDPOutputData(config);
    });
    return ChannelOutput::Init(config);
}
int GenericUDPOutput::Close(void) {
    UDPOutput::UnregisterExternalOutput(this);
    return ChannelOutput::Close();
}

void GenericUDPOutput::GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) {
    addRange(m_startChannel, m_startChannel + m_channelCount - 1);
}

int GenericUDPOutput::SendData(unsigned char* channelData) {
    return m_channelCount;
}
