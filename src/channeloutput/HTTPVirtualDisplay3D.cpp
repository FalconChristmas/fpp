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

#include "fpp-pch.h"

#include <netinet/in.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>

#include <ctime>
#include <iomanip>

#include "../fppversion.h"
#include "../log.h"
#include "../common.h"

#include "HTTPVirtualDisplay3D.h"

#include "Plugin.h"
class HTTPVirtualDisplay3DPlugin : public FPPPlugins::Plugin, public FPPPlugins::ChannelOutputPlugin {
public:
    HTTPVirtualDisplay3DPlugin() :
        FPPPlugins::Plugin("HTTPVirtualDisplay3D") {
    }
    virtual ChannelOutput* createChannelOutput(unsigned int startChannel, unsigned int channelCount) override {
        return new HTTPVirtualDisplay3DOutput(startChannel, channelCount);
    }
};

extern "C" {
FPPPlugins::Plugin* createPlugin() {
    return new HTTPVirtualDisplay3DPlugin();
}
}
/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
HTTPVirtualDisplay3DOutput::HTTPVirtualDisplay3DOutput(unsigned int startChannel,
                                                       unsigned int channelCount) :
    VirtualDisplayBaseOutput(startChannel, channelCount),
    m_port(HTTPVIRTUALDISPLAY3DPORT),
    m_screenSize(0),
    m_updateInterval(1),  // Default: send every frame for smooth playback
    m_socket(-1),
    m_running(true),
    m_connListChanged(true),
    m_connThread(nullptr),
    m_selectThread(nullptr) {
    LogDebug(VB_CHANNELOUT, "HTTPVirtualDisplay3DOutput::HTTPVirtualDisplay3DOutput(%u, %u)\n",
             startChannel, channelCount);

    m_bytesPerPixel = 3;
    m_bpp = 24;
}

/*
 *
 */
HTTPVirtualDisplay3DOutput::~HTTPVirtualDisplay3DOutput() {
    LogDebug(VB_CHANNELOUT, "HTTPVirtualDisplay3DOutput::~HTTPVirtualDisplay3DOutput()\n");

    Close();

    m_running = false;

    if (m_connThread) {
        m_connThread->join();
        delete m_connThread;
    }

    if (m_selectThread) {
        m_selectThread->join();
        delete m_selectThread;
    }

    if (m_connList.size()) {
        for (int i = m_connList.size() - 1; i >= 0; i--) {
            close(m_connList[i]);
            m_connList.erase(m_connList.begin() + i);
        }
    }
}

/*
 *
 */
void RunConnectionThread3D(HTTPVirtualDisplay3DOutput* VirtualDisplay) {
    LogDebug(VB_CHANNELOUT, "Started 3D ConnectionThread()\n");
    VirtualDisplay->ConnectionThread();
}

/*
 *
 */
void RunSelectThread3D(HTTPVirtualDisplay3DOutput* VirtualDisplay) {
    LogDebug(VB_CHANNELOUT, "Started 3D SelectThread()\n");
    VirtualDisplay->SelectThread();
}

/*
 *
 */
int HTTPVirtualDisplay3DOutput::Init(Json::Value config) {
    LogDebug(VB_CHANNELOUT, "HTTPVirtualDisplay3DOutput::Init()\n");

    if (!VirtualDisplayBaseOutput::Init(config))
        return 0;

    if (config.isMember("port"))
        m_port = config["port"].asInt();

    if (config.isMember("updateInterval"))
        m_updateInterval = config["updateInterval"].asInt();
    
    // Clamp to reasonable values (1-10 frames)
    if (m_updateInterval < 1) m_updateInterval = 1;
    if (m_updateInterval > 10) m_updateInterval = 10;

    // Signal base class to allocate m_virtualDisplay buffer
    m_width = -1;
    m_height = -1;

    if (!InitializePixelMap()) {
        LogErr(VB_CHANNELOUT, "Error, unable to initialize pixel map\n");
        return 0;
    }

    m_screenSize = m_width * m_height * 3;

    bzero(m_virtualDisplay, m_screenSize);

    m_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket < 0) {
        LogErr(VB_CHANNELOUT, "Could not create socket: %s\n", strerror(errno));
        return 0;
    }

    fcntl(m_socket, F_SETFL, O_NONBLOCK);

    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(m_port);

    int optval = 1;
    if (setsockopt(m_socket, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) < 0) {
        LogErr(VB_CHANNELOUT, "Error turning on SO_REUSEPORT; %s\n", strerror(errno));
        return 0;
    }

    int rc = bind(m_socket, (struct sockaddr*)&addr, sizeof(addr));
    if (rc < 0) {
        LogErr(VB_CHANNELOUT, "Could not bind socket: %s\n", strerror(errno));
        return 0;
    }

    rc = listen(m_socket, 5);
    if (rc < 0) {
        LogErr(VB_CHANNELOUT, "Could not listen on socket: %s\n", strerror(errno));
        return 0;
    }

    m_connThread = new std::thread(RunConnectionThread3D, this);
    m_selectThread = new std::thread(RunSelectThread3D, this);
    
    LogInfo(VB_CHANNELOUT, "HTTPVirtualDisplay3D listening on port %d\n", m_port);
    return 1;
}

/*
 *
 */
int HTTPVirtualDisplay3DOutput::Close(void) {
    LogDebug(VB_CHANNELOUT, "HTTPVirtualDisplay3DOutput::Close()\n");

    return VirtualDisplayBaseOutput::Close();
}

/*
 *
 */
void HTTPVirtualDisplay3DOutput::ConnectionThread(void) {
    SetThreadName("FPP-HTTPVD3D-Con");
    int client;

    while (m_running) {
        client = accept(m_socket, NULL, NULL);

        if (client >= 0) {
            auto t = std::time(nullptr);
            auto tm = *std::localtime(&t);
            std::stringstream sstr;
            sstr << std::put_time(&tm, "%a %b %d %H:%M:%S %Z %Y");

            std::string sseResp;
            sseResp =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/event-stream;charset=UTF-8\r\n"
                "Transfer-Encoding: chunked\r\n"
                "Connection: close\r\n"
                "Date: ";
            sseResp += sstr.str();
            sseResp +=
                "\r\n"
                "Server: fppd\r\n"
                "X-Powered-By: FPP/";
            sseResp += getFPPVersion();
            sseResp += "\r\n"
                       "Cache-Control: no-cache, private\r\n"
                       "Access-Control-Allow-Origin: *\r\n"
                       "Access-Control-Allow-Credentials: true\r\n"
                       "\r\n";

            // Reset our display cache so we draw everything needed
            bzero(m_virtualDisplay, m_screenSize);

            std::unique_lock<std::mutex> lock(m_connListLock);
            m_connList.push_back(client);

            write(client, sseResp.c_str(), sseResp.length());

            m_connListChanged = true;
            
            LogDebug(VB_CHANNELOUT, "3D Virtual Display client connected\n");
        }

        usleep(100000);
    }
}

/*
 *
 */
void HTTPVirtualDisplay3DOutput::SelectThread(void) {
    SetThreadName("FPP-HTTPVD3D-Sel");
    fd_set active_fd_set;
    fd_set read_fd_set;
    int selectResult;
    struct timeval timeout;
    char buf[1024];
    int bytesRead;

    FD_ZERO(&active_fd_set);

    while (m_running) {
        if (m_connListChanged) {
            LogDebug(VB_CHANNELOUT, "3D Connection list changed, rebuilding active_fd-set\n");
            FD_ZERO(&active_fd_set);

            std::unique_lock<std::mutex> lock(m_connListLock);

            for (int i = 0; i < m_connList.size(); i++) {
                FD_SET(m_connList[i], &active_fd_set);
            }

            m_connListChanged = 0;
        }

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        read_fd_set = active_fd_set;

        selectResult = select(FD_SETSIZE, &read_fd_set, NULL, NULL, &timeout);
        if (selectResult < 0) {
            if (errno == EINTR) {
                continue;
            } else {
                LogErr(VB_CHANNELOUT, "Main select() failed\n");
            }
        } else if (selectResult == 0) {
            // Nothing to do
            continue;
        }

        std::unique_lock<std::mutex> lock(m_connListLock);
        for (int i = m_connList.size() - 1; i >= 0; i--) {
            if (FD_ISSET(m_connList[i], &read_fd_set)) {
                bytesRead = recv(m_connList[i], buf, sizeof(buf), 0);

                if (bytesRead > 0) {
                    LogDebug(VB_CHANNELOUT, "Data read from 3D socket %d, connection %d \n", m_connList[i], i);
                } else if (bytesRead == 0) {
                    LogDebug(VB_CHANNELOUT, "Closing 3D socket %d, connection %d\n", m_connList[i], i);
                    close(m_connList[i]);
                    m_connList.erase(m_connList.begin() + i);
                    m_connListChanged = true;
                } else {
                    LogErr(VB_CHANNELOUT, "Read failed for 3D socket %d, connection %d.  Closing connection.\n", m_connList[i], i);
                    m_connList.erase(m_connList.begin() + i);
                    m_connListChanged = true;
                }
            }
        }
    }
}

/*
 *
 */
int HTTPVirtualDisplay3DOutput::WriteSSEPacket(int fd, std::string data) {
    int len = data.size();
    std::string sendData;
    
    // Pre-allocate to avoid reallocations
    sendData.reserve(20 + len); // hex length + delimiters + data
    
    // Convert length to hex directly into string
    char hexBuf[16];
    snprintf(hexBuf, sizeof(hexBuf), "%x", len);
    
    sendData = hexBuf;
    sendData += "\r\n";
    sendData += data;
    sendData += "\r\n";

    write(fd, sendData.c_str(), sendData.size());

    return 1;
}

/*
 * PrepData for 3D - sends pixel index instead of 2D coordinates
 * Format: RGB_COLOR:PIXEL_INDEX;PIXEL_INDEX;PIXEL_INDEX|...
 */
void HTTPVirtualDisplay3DOutput::PrepData(unsigned char* channelData) {
    static int id = 0;
    static bool lastFrameWasBlack = false;

    LogExcess(VB_CHANNELOUT, "HTTPVirtualDisplay3DOutput::PrepData(%p)\n",
              channelData);

    {
        // Short circuit if no current connections
        std::unique_lock<std::mutex> lock(m_connListLock);
        if (!m_connList.size())
            return;
    }

    // Fast black detection - check if all channel data is zero (sequence ended/stopped)
    static int blackCheckCounter = 0;
    bool allBlack = false;
    bool forceBlackUpdate = false;
    
    if (lastFrameWasBlack || (++blackCheckCounter >= 10)) {
        blackCheckCounter = 0;
        allBlack = true;
        for (int i = 0; i < m_pixels.size() && allBlack; i++) {
            unsigned char r, g, b;
            GetPixelRGB(m_pixels[i], channelData, r, g, b);
            if (r != 0 || g != 0 || b != 0) {
                allBlack = false;
            }
        }
        
        forceBlackUpdate = (allBlack && !lastFrameWasBlack);
        lastFrameWasBlack = allBlack;
    }

    std::string data;
    int pixelsChanged = 0;
    unsigned char r, g, b;
    std::map<std::string, std::string> colors;
    std::map<std::string, std::string>::iterator colorIt;
    char color[4];
    std::string colorStr;
    char pixelIdx[16];
    VirtualDisplayPixel pixel;
    static const char* const base64 = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz+/";

    for (int i = 0; i < m_pixels.size(); i++) {
        GetPixelRGB(m_pixels[i], channelData, r, g, b);

        // 18-bit RGB666 pixel data passed over Event Stream in 3 bytes
        // using Base64 allowed characters since SSE is non-binary
        r = r >> 2;
        g = g >> 2;
        b = b >> 2;

        // Send pixels that changed OR when forcing black update (sequence ended)
        if (forceBlackUpdate ||
            (m_virtualDisplay[m_pixels[i].r] != r) ||
            (m_virtualDisplay[m_pixels[i].g] != g) ||
            (m_virtualDisplay[m_pixels[i].b] != b)) {
            m_virtualDisplay[m_pixels[i].r] = r;
            m_virtualDisplay[m_pixels[i].g] = g;
            m_virtualDisplay[m_pixels[i].b] = b;

            snprintf(color, sizeof(color), "%c%c%c", base64[r], base64[g], base64[b]);
            colorStr = color;

            // KEY DIFFERENCE: Use pixel index instead of x,y coordinates
            snprintf(pixelIdx, sizeof(pixelIdx), "%d", i);

            colorIt = colors.find(colorStr);
            if (colorIt != colors.end())
                colorIt->second += std::string(";") + pixelIdx;
            else
                colors[colorStr] = colorStr + ":" + pixelIdx;

            pixelsChanged++;
        }
    }

    if (colors.size()) {
        // Pre-allocate to avoid reallocations during string building
        m_sseData.clear();
        m_sseData.reserve(128 + colors.size() * 16);
        
        m_sseData = "id: ";
        m_sseData += std::to_string(id) + "\r\n";
        m_sseData += "event: message\r\n";
        m_sseData += "data: ";

        std::string data2;
        data2.reserve(colors.size() * 16);
        
        bool first = true;
        for (const auto& pair : colors) {
            if (!first)
                data2 += "|";
            else
                first = false;

            data2 += pair.second;
        }
        m_sseData += data2 + "\r\n\r\n";

        LogExcess(VB_CHANNELOUT, "3D PixelsChanged: %d, Colors: %d, Data Size: %d\n",
                  pixelsChanged, colors.size(), m_sseData.size());
    } else {
        // Send empty update to keep connection alive
        m_sseData = "id: ";
        m_sseData += std::to_string(id) + "\r\n";
        m_sseData += "event: ping\r\n";
        m_sseData += "data: \r\n\r\n";
    }

    id++;
}

/*
 *
 */
int HTTPVirtualDisplay3DOutput::SendData(unsigned char* channelData) {
    if (m_sseData != "") {
        std::unique_lock<std::mutex> lock(m_connListLock);
        for (int i = 0; i < m_connList.size(); i++)
            WriteSSEPacket(m_connList[i], m_sseData);
    }

    return m_channelCount;
}
