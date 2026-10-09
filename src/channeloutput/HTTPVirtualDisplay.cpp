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

#include "fpp-json.h"

#include "Warnings.h" // WarningHolder -- needed directly for NOPCH builds

#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>

#include <ctime>
#include <iomanip>

#include "../fppversion.h"
#include "../log.h"
#include "../common.h"

#include "HTTPVirtualDisplay.h"

#include "Plugin.h"
class HTTPVirtualDisplayPlugin : public FPPPlugins::Plugin, public FPPPlugins::ChannelOutputPlugin {
public:
    HTTPVirtualDisplayPlugin() :
        FPPPlugins::Plugin("HTTPVirtualDisplay") {
    }
    virtual ChannelOutput* createChannelOutput(unsigned int startChannel, unsigned int channelCount) override {
        return new HTTPVirtualDisplayOutput(startChannel, channelCount);
    }
};

extern "C" {
FPPPlugins::Plugin* createPlugin() {
    return new HTTPVirtualDisplayPlugin();
}
}
/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
HTTPVirtualDisplayOutput::HTTPVirtualDisplayOutput(unsigned int startChannel,
                                                   unsigned int channelCount) :
    VirtualDisplayBaseOutput(startChannel, channelCount),
    m_port(HTTPVIRTUALDISPLAYPORT),
    m_screenSize(0),
    m_updateInterval(25),  // Default: 25ms (40fps), about all a browser can render
    m_nextSendMS(0),
    m_socket(-1),
    m_running(true),
    m_connListChanged(true),
    m_connThread(nullptr),
    m_selectThread(nullptr) {
    LogDebug(VB_CHANNELOUT, "HTTPVirtualDisplayOutput::HTTPVirtualDisplayOutput(%u, %u)\n",
             startChannel, channelCount);

    m_bytesPerPixel = 3;
    m_bpp = 24;
}

/*
 *
 */
HTTPVirtualDisplayOutput::~HTTPVirtualDisplayOutput() {
    LogDebug(VB_CHANNELOUT, "HTTPVirtualDisplayOutput::~HTTPVirtualDisplayOutput()\n");

    m_running = false;

    if (m_connThread) {
        m_connThread->join();
        delete m_connThread;
        m_connThread = nullptr;
    }

    if (m_selectThread) {
        m_selectThread->join();
        delete m_selectThread;
        m_selectThread = nullptr;
    }

    if (m_socket >= 0) {
        close(m_socket);
        m_socket = -1;
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
void RunConnectionThread(HTTPVirtualDisplayOutput* VirtualDisplay) {
    LogDebug(VB_CHANNELOUT, "Started ConnectionThread()\n");
    VirtualDisplay->ConnectionThread();
}

/*
 *
 */
void RunSelectThread(HTTPVirtualDisplayOutput* VirtualDisplay) {
    LogDebug(VB_CHANNELOUT, "Started SelectThread()\n");
    VirtualDisplay->SelectThread();
}

/*
 *
 */
int HTTPVirtualDisplayOutput::Init(Json::Value config) {
    LogDebug(VB_CHANNELOUT, "HTTPVirtualDisplayOutput::Init()\n");

    if (!VirtualDisplayBaseOutput::Init(config))
        return 0;

    if (config.isMember("port"))
        m_port = config["port"].asInt();

    if (config.isMember("updateInterval"))
        m_updateInterval = config["updateInterval"].asInt();

    // Clamp to reasonable values (10-100ms between updates)
    // Typical: 25ms for 40fps, 16ms for 60fps, 50ms for 20fps
    if (m_updateInterval < 10) m_updateInterval = 10;
    if (m_updateInterval > 100) m_updateInterval = 100;

    // Signal base class to allocate m_virtualDisplay buffer
    m_width = -1;
    m_height = -1;

    if (!InitializePixelMap()) {
        LogErr(VB_CHANNELOUT, "Error, unable to initialize pixel map\n");
        WarningHolder::AddWarning(37, "Virtual Display preview: could not initialize pixel map");
        return 0;
    }

    // An empty map parses "successfully" but allocates nothing (and absurd map
    // dimensions fail the allocation above the same way): refuse to run with
    // no buffer or dimensions that cannot form one, instead of crashing on
    // the first draw/status poll.
    if (m_virtualDisplay == nullptr ||
        virtualDisplayBufferBytes(m_width, m_height, m_bytesPerPixel) == 0) {
        LogErr(VB_CHANNELOUT, "Error, virtual display has no usable buffer (%dx%d)\n",
               m_width, m_height);
        WarningHolder::AddWarning(37, "Virtual Display preview: could not initialize pixel map");
        return 0;
    }

    m_screenSize = (int)virtualDisplayBufferBytes(m_width, m_height, m_bytesPerPixel);

    // Worst case a frame touches every pixel; size this once so the hot loop
    // never reallocates
    m_cacheRollback.reserve(m_pixels.size() * 3);

    bzero(m_virtualDisplay, m_screenSize);

    // Bind a dual-stack socket (IPv6 with V6ONLY off) so the SSE feed
    // is reachable on both IPv4 and IPv6 clients without needing two
    // listeners. Apache's mod_proxy talks to us on localhost; both
    // 127.0.0.1 and ::1 land here.
    m_socket = socket(AF_INET6, SOCK_STREAM, 0);
    if (m_socket < 0) {
        LogErr(VB_CHANNELOUT, "Could not create socket: %s\n", FPPstrerror(errno));
        WarningHolder::AddWarning(37, "Virtual Display preview: could not create socket");
        return 0;
    }

    fcntl(m_socket, F_SETFL, O_NONBLOCK);

    int optval = 1;
    if (setsockopt(m_socket, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) < 0) {
        LogErr(VB_CHANNELOUT, "Error turning on SO_REUSEPORT; %s\n", FPPstrerror(errno));
        return 0;
    }
    int v6only = 0;
    setsockopt(m_socket, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only));

    struct sockaddr_in6 addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_addr = in6addr_any;
    addr.sin6_port = htons(m_port);

    int rc = bind(m_socket, (struct sockaddr*)&addr, sizeof(addr));
    if (rc < 0) {
        LogErr(VB_CHANNELOUT, "Could not bind socket: %s\n", FPPstrerror(errno));
        WarningHolder::AddWarning(37, "Virtual Display preview: could not bind to its port (already in use?)");
        return 0;
    }

    rc = listen(m_socket, 5);
    if (rc < 0) {
        LogErr(VB_CHANNELOUT, "Could not listen on socket: %s\n", FPPstrerror(errno));
        WarningHolder::AddWarning(37, "Virtual Display preview: could not listen on its socket");
        return 0;
    }

    m_connThread = new std::thread(RunConnectionThread, this);
    m_selectThread = new std::thread(RunSelectThread, this);
    return 1;
}

/*
 *
 */
int HTTPVirtualDisplayOutput::Close(void) {
    LogDebug(VB_CHANNELOUT, "HTTPVirtualDisplayOutput::Close()\n");

    return VirtualDisplayBaseOutput::Close();
}

/*
 *
 */
void HTTPVirtualDisplayOutput::ConnectionThread(void) {
    SetThreadName("FPP-HTTPVD-Con");
    int client;

    while (m_running) {
        client = accept(m_socket, NULL, NULL);

        if (client >= 0) {
            // Enable TCP keepalive to detect dead connections
            int keepalive = 1;
            setsockopt(client, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));

            // Accepted sockets don't inherit O_NONBLOCK from the listener, so
            // set it explicitly - otherwise a browser that stops draining its
            // socket blocks write() on the channel output thread, which stalls
            // the actual light output, not just this preview.
            int flags = fcntl(client, F_GETFL, 0);
            fcntl(client, F_SETFL, (flags < 0 ? 0 : flags) | O_NONBLOCK);

            auto t = std::time(nullptr);
            struct tm tm;
            localtime_r(&t, &tm);
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
        }

        usleep(100000);
    }
}

/*
 *
 */
void HTTPVirtualDisplayOutput::SelectThread(void) {
    SetThreadName("FPP-HTTPVD-Sel");
    fd_set active_fd_set;
    fd_set read_fd_set;
    int selectResult;
    struct timeval timeout;
    char buf[1024];
    int bytesRead;

    FD_ZERO(&active_fd_set);

    while (m_running) {
        if (m_connListChanged) {
            LogDebug(VB_CHANNELOUT, "Connection list changed, rebuilding active_fd-set\n");
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
                    LogDebug(VB_CHANNELOUT, "Data read from socket %d, connection %d \n", m_connList[i], i);
                } else if (bytesRead == 0) {
                    LogDebug(VB_CHANNELOUT, "Closing socket %d, connection %d\n", m_connList[i], i);
                    close(m_connList[i]);
                    m_connList.erase(m_connList.begin() + i);
                    m_connListChanged = true;
                } else {
                    LogErr(VB_CHANNELOUT, "Read failed for socket %d, connection %d.  Closing connection.\n", m_connList[i], i);
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
int HTTPVirtualDisplayOutput::WriteSSEPacket(int fd, const std::string& data) {
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

    size_t total = sendData.size();
    size_t sent = 0;

    while (sent < total) {
        ssize_t written = write(fd, sendData.c_str() + sent, total - sent);

        if (written > 0) {
            sent += written;
            continue;
        }

        if (written < 0 && errno == EINTR) {
            continue;
        }

        if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            if (sent == 0) {
                // The client isn't draining fast enough and the send buffer is
                // full.  Drop this frame rather than the connection - nothing is
                // on the wire yet.  The caller resyncs the client afterwards,
                // since the pixel changes in this payload are only sent once.
                LogExcess(VB_CHANNELOUT, "SSE send buffer full, dropping frame\n");
                return 0;
            }

            // We're part way through a chunk.  We can't abandon it - the client
            // is counting out the bytes we promised in the chunk header, so a
            // truncated chunk desyncs the stream permanently.  Wait (briefly)
            // for room and finish it.
            struct pollfd pfd;
            pfd.fd = fd;
            pfd.events = POLLOUT;
            int pr = poll(&pfd, 1, 250);
            if (pr > 0) {
                continue;
            }
            if (pr < 0 && errno == EINTR) {
                continue;
            }

            LogDebug(VB_CHANNELOUT, "SSE stalled part way through a chunk (%zu/%zu bytes), closing connection\n",
                     sent, total);
            return -1;
        }

        // EPIPE/ECONNRESET/ENOTCONN and anything else: the connection is gone
        LogDebug(VB_CHANNELOUT, "SSE write failed (connection closed): %s\n", FPPstrerror(errno));
        return -1;
    }

    return 1;
}

/*
 *
 */
void HTTPVirtualDisplayOutput::PrepData(unsigned char* channelData) {
    static int id = 0;
    static bool lastFrameWasBlack = false;

    LogExcess(VB_CHANNELOUT, "HTTPVirtualDisplayOutput::PrepData(%p)\n",
              channelData);

    {
        // Short circuit if no current connections
        std::unique_lock<std::mutex> lock(m_connListLock);
        if (!m_connList.size()) {
            m_sseData.clear();
            return;
        }
    }

    // We run at the sequence frame rate, which can now be well above anything a
    // browser can render.  Sending faster than the client drains just piles
    // frames up in the socket and proxy buffers, so the preview falls further
    // and further behind playback.  Rate limit to m_updateInterval instead.
    //
    // Skipping a frame here is safe even though we send deltas: m_virtualDisplay
    // is only updated for pixels we actually emit, so it always holds what the
    // client was last sent and the next diff simply spans a longer gap.
    //
    // The deadline is advanced by whole intervals rather than reset to "now" so
    // that an update interval which isn't a multiple of the frame time still
    // averages out to the requested rate instead of aliasing down to the next
    // frame boundary (eg. 25ms against a 20ms sequence gives 40fps, not 25fps).
    long long now = GetTimeMS();
    if (now < m_nextSendMS) {
        m_sseData.clear();
        return;
    }
    m_nextSendMS += m_updateInterval;
    if (m_nextSendMS <= now) {
        // First frame, or we've been idle/stalled long enough that the deadline
        // is stale - resync rather than sending a burst to catch up.
        m_nextSendMS = now + m_updateInterval;
    }

    // Start a fresh rollback record for this frame's cache writes
    m_cacheRollback.clear();

    // Fast black detection - check if all channel data is zero (sequence ended/stopped)
    // Only do this check occasionally to reduce CPU overhead
    static int blackCheckCounter = 0;
    bool allBlack = false;
    bool forceBlackUpdate = false;
    
    // Check for black every 10th frame to reduce CPU, but always check if last frame was black
    // to catch the transition back to color quickly
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
        
        // If transitioning to black, force update of all pixels
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
    char loc[7];
    int y;
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
            // Remember the pre-send values so SendData can undo them if this
            // frame never makes it to the client
            m_cacheRollback.emplace_back(m_pixels[i].r, m_virtualDisplay[m_pixels[i].r]);
            m_cacheRollback.emplace_back(m_pixels[i].g, m_virtualDisplay[m_pixels[i].g]);
            m_cacheRollback.emplace_back(m_pixels[i].b, m_virtualDisplay[m_pixels[i].b]);

            m_virtualDisplay[m_pixels[i].r] = r;
            m_virtualDisplay[m_pixels[i].g] = g;
            m_virtualDisplay[m_pixels[i].b] = b;

            snprintf(color, sizeof(color), "%c%c%c", base64[r], base64[g], base64[b]);
            colorStr = color;

            y = m_previewHeight - m_pixels[i].y;
            if (m_pixels[i].x >= 4095)
                snprintf(loc, sizeof(loc), "%c%c%c%c%c%c",
                         base64[(m_pixels[i].x >> 12) & 0x3f],
                         base64[(m_pixels[i].x >> 6) & 0x3f],
                         base64[(m_pixels[i].x) & 0x3f],
                         base64[(y >> 12) & 0x3f],
                         base64[(y >> 6) & 0x3f],
                         base64[(y)&0x3f]);
            else
                snprintf(loc, sizeof(loc), "%c%c%c%c",
                         base64[(m_pixels[i].x >> 6) & 0x3f],
                         base64[(m_pixels[i].x) & 0x3f],
                         base64[(y >> 6) & 0x3f],
                         base64[(y)&0x3f]);

            colorIt = colors.find(colorStr);
            if (colorIt != colors.end())
                colorIt->second += std::string(";") + loc;
            else
                colors[colorStr] = colorStr + ":" + loc;

            pixelsChanged++;
        }
    }

    if (colors.size()) {
        // Pre-allocate to avoid reallocations during string building
        m_sseData.clear();
        m_sseData.reserve(128 + colors.size() * 16); // Estimate: header + avg color data
        
        m_sseData = "id: ";
        m_sseData += std::to_string(id) + "\r\n";
        m_sseData += "event: message\r\n";
        m_sseData += "data: ";

        std::string data2;
        data2.reserve(colors.size() * 16); // Pre-allocate for color data
        
        bool first = true;
        for (const auto& pair : colors) {
            if (!first)
                data2 += "|";
            else
                first = false;

            data2 += pair.second;
        }
        m_sseData += data2 + "\r\n\r\n";

        LogExcess(VB_CHANNELOUT, "PixelsChanged: %d, Colors: %d, Data Size: %d\n",
                  pixelsChanged, colors.size(), m_sseData.size());
    } else {
        // Send empty update to keep connection alive and maintain frame timing
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
int HTTPVirtualDisplayOutput::SendData(unsigned char* channelData) {
    bool dropped = false;

    if (m_sseData != "") {
        std::unique_lock<std::mutex> lock(m_connListLock);
        // Iterate backwards so we can safely remove elements
        for (int i = m_connList.size() - 1; i >= 0; i--) {
            int result = WriteSSEPacket(m_connList[i], m_sseData);
            if (result < 0) {
                // Connection is dead, remove it
                LogDebug(VB_CHANNELOUT, "Removing dead SSE connection %d\n", m_connList[i]);
                close(m_connList[i]);
                m_connList.erase(m_connList.begin() + i);
                m_connListChanged = true;
            } else if (result == 0) {
                dropped = true;
            }
        }
    }

    if (dropped) {
        // The payload held the only copy of this frame's pixel changes, so put
        // the cache back the way it was and let the next frame re-send them.
        for (auto it = m_cacheRollback.rbegin(); it != m_cacheRollback.rend(); ++it) {
            m_virtualDisplay[it->first] = it->second;
        }
        m_cacheRollback.clear();
    }

    return m_channelCount;
}
