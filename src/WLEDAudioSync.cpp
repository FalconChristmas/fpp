/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2026 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include "fpp-pch.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstring>

#include "WLEDAudioSync.h"

#include "common.h"
#include "log.h"
#include "settings.h"

#include "overlays/wled/wled.h"   // AudioFrame, fetchAudioSamples, computeAudioFrame

WLEDAudioSync WLEDAudioSync::INSTANCE;

namespace {

// WLED V2 audio-sync packet on the wire. 44 bytes, big-endian floats?
// No — WLED runs on little-endian ESP32/ESP8266 and emits the bytes
// raw, so we transmit native floats. All fields are documented in
// /opt/WLED/usermods/audioreactive/audio_reactive.cpp around line 794.
#pragma pack(push, 1)
struct AudioSyncPacketV2 {
    char     header[6];       //  6  "00002\0"
    uint8_t  reserved1[2];    //  2  unused, present for compiler padding parity
    float    sampleRaw;       //  4
    float    sampleSmth;      //  4
    uint8_t  samplePeak;      //  1  0=no peak, >=1=beat
    uint8_t  reserved2;       //  1
    uint8_t  fftResult[16];   // 16  16 freq bins, 0-255 each
    uint16_t reserved3;       //  2
    float    FFT_Magnitude;   //  4
    float    FFT_MajorPeak;   //  4
};                            // 44 total
#pragma pack(pop)
static_assert(sizeof(AudioSyncPacketV2) == 44,
              "WLED audiosync V2 packet must be exactly 44 bytes");

constexpr const char* WLED_HEADER_V2 = "00002";

uint64_t nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(
        steady_clock::now().time_since_epoch()).count();
}

} // namespace

WLEDAudioSync::WLEDAudioSync() = default;

WLEDAudioSync::~WLEDAudioSync() {
    Cleanup();
}

void WLEDAudioSync::Initialize() {
    // Live-reload: any of the three settings can be edited at runtime
    // (e.g. via the FPP settings UI) and we'll reconfigure without an
    // fppd restart. The handler funnels into Reconfigure() which is
    // mutex-protected against concurrent edits.
    auto cb = [this](const std::string&) { Reconfigure(); };
    registerSettingsListener("WLEDAudioSync", "WLEDAudioSyncMode", cb);
    registerSettingsListener("WLEDAudioSync", "WLEDAudioSyncDest", cb);
    registerSettingsListener("WLEDAudioSync", "WLEDAudioSyncPort", cb);

    Reconfigure();
}

void WLEDAudioSync::Cleanup() {
    // Idempotent: main() calls this during shutdown, and ~WLEDAudioSync() calls
    // it again at static-destruction time. The second call must be a no-op --
    // unregisterSettingsListener() reaches into the global SettingsConfig, which
    // may already be destroyed by then (cross-TU static destruction order),
    // and locking its destroyed mutex throws EINVAL out of the noexcept dtor.
    if (m_cleanedUp.exchange(true)) {
        return;
    }
    unregisterSettingsListener("WLEDAudioSync", "WLEDAudioSyncMode");
    unregisterSettingsListener("WLEDAudioSync", "WLEDAudioSyncDest");
    unregisterSettingsListener("WLEDAudioSync", "WLEDAudioSyncPort");
    StopAllThreads();
}

void WLEDAudioSync::Reconfigure() {
    std::unique_lock<std::mutex> lock(m_reconfigureLock);

    Mode newMode = Off;
    std::string modeStr = getSetting("WLEDAudioSyncMode");
    if (modeStr == "send"    || modeStr == "Send"    || modeStr == "1") newMode = Send;
    else if (modeStr == "receive" || modeStr == "Receive" || modeStr == "2") newMode = Receive;

    int newPort = 11988;
    int p = getSettingInt("WLEDAudioSyncPort", 0);
    if (p > 0 && p < 65536) newPort = p;

    std::string newDest = getSetting("WLEDAudioSyncDest");
    if (newDest.empty()) newDest = "239.0.0.1";

    // Tear down anything currently running before re-applying. Cheap
    // and unambiguous; sockets reopen in <50 ms.
    StopAllThreads();

    // Threads are stopped, so nothing can re-raise these — clear any stale
    // warnings from a previous configuration. The (re)attempt below re-raises
    // the relevant one if it still fails.
    WarningHolder::RemoveWarning(46, "WLED Audio Sync: could not open the send socket");
    WarningHolder::RemoveWarning(46, "WLED Audio Sync: could not open the receive socket");
    WarningHolder::RemoveWarning(46, "WLED Audio Sync: invalid destination address");

    m_mode = newMode;
    m_port = newPort;
    m_destAddr = newDest;

    if (m_mode == Off) {
        LogInfo(VB_GENERAL, "WLEDAudioSync: disabled\n");
        return;
    }

    CacheLocalIPs();
    m_running = true;

    if (m_mode == Send) {
        if (!OpenSendSocket()) {
            LogErr(VB_GENERAL, "WLEDAudioSync: send socket open failed\n");
            WarningHolder::AddWarning(46, "WLED Audio Sync: could not open the send socket");
            m_mode = Off;
            m_running = false;
            return;
        }
        m_sendThread = new std::thread([this]() {
            SetThreadName("FPP-WLEDAudio-S");
            SendLoop();
        });
        LogInfo(VB_GENERAL, "WLEDAudioSync: SEND mode, %s:%d\n",
                m_destAddr.c_str(), m_port);
    } else if (m_mode == Receive) {
        if (!OpenReceiveSocket()) {
            LogErr(VB_GENERAL, "WLEDAudioSync: receive socket open failed\n");
            WarningHolder::AddWarning(46, "WLED Audio Sync: could not open the receive socket");
            m_mode = Off;
            m_running = false;
            return;
        }
        m_recvThread = new std::thread([this]() {
            SetThreadName("FPP-WLEDAudio-R");
            ReceiveLoop();
        });
        LogInfo(VB_GENERAL, "WLEDAudioSync: RECEIVE mode, port %d\n", m_port);
    }
}

void WLEDAudioSync::StopAllThreads() {
    m_running = false;

    if (m_recvSocket >= 0) {
        // shutdown() before close() so the listener's blocking
        // recvfrom() returns immediately (same lesson as the WLED
        // API responder — close alone doesn't wake recvfrom).
        ::shutdown(m_recvSocket, SHUT_RDWR);
        ::close(m_recvSocket);
        m_recvSocket = -1;
    }
    if (m_recvThread) {
        if (m_recvThread->joinable()) m_recvThread->join();
        delete m_recvThread;
        m_recvThread = nullptr;
    }
    if (m_sendThread) {
        if (m_sendThread->joinable()) m_sendThread->join();
        delete m_sendThread;
        m_sendThread = nullptr;
    }
    if (m_sendSocket >= 0) {
        ::close(m_sendSocket);
        m_sendSocket = -1;
    }
}

bool WLEDAudioSync::OpenSendSocket() {
    m_sendSocket = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (m_sendSocket < 0) return false;

    // Enable broadcast (allows 255.255.255.255 / x.x.x.255 destinations).
    int yes = 1;
    ::setsockopt(m_sendSocket, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));

    // Multicast TTL of 1 keeps multicast packets link-local; same
    // semantics WLED uses on its own audiosync transmitter.
    unsigned char ttl = 1;
    ::setsockopt(m_sendSocket, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));

    return true;
}

bool WLEDAudioSync::OpenReceiveSocket() {
    m_recvSocket = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (m_recvSocket < 0) return false;

    int yes = 1;
    ::setsockopt(m_recvSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    // SO_REUSEPORT intentionally NOT set: it triggers the kernel's
    // load-balancing path even with a single binder, and on some
    // kernels causes UDP packets to be silently dropped when the
    // sender and receiver are both on loopback. SO_REUSEADDR alone
    // gives us the bind-restart-without-TIME_WAIT semantics we want.

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(m_port);
    if (::bind(m_recvSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        LogErr(VB_GENERAL, "WLEDAudioSync: bind(%d) failed: %s\n",
               m_port, strerror(errno));
        ::close(m_recvSocket);
        m_recvSocket = -1;
        return false;
    }

    // If the destination address is multicast, join the group so
    // multicast packets are delivered to us. Unicast/broadcast needs
    // no extra setup beyond the bind above.
    in_addr a{};
    if (inet_pton(AF_INET, m_destAddr.c_str(), &a) == 1
        && IN_MULTICAST(ntohl(a.s_addr))) {
        ip_mreq mreq{};
        mreq.imr_multiaddr = a;
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);
        if (::setsockopt(m_recvSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                         &mreq, sizeof(mreq)) < 0) {
            LogWarn(VB_GENERAL, "WLEDAudioSync: IP_ADD_MEMBERSHIP %s failed: %s\n",
                    m_destAddr.c_str(), strerror(errno));
        }
    }
    return true;
}

void WLEDAudioSync::CacheLocalIPs() {
    m_localIPs.clear();
    ifaddrs* ifa = nullptr;
    if (getifaddrs(&ifa) != 0) return;
    for (auto* p = ifa; p; p = p->ifa_next) {
        if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET) continue;
        auto* in = reinterpret_cast<sockaddr_in*>(p->ifa_addr);
        m_localIPs.insert(in->sin_addr.s_addr);
    }
    freeifaddrs(ifa);
}

bool WLEDAudioSync::IsLocalAddress(uint32_t ipv4_n) const {
    return m_localIPs.count(ipv4_n) > 0;
}

// Standalone broadcast loop. Runs whenever WLEDAudioSync is in Send
// mode, independent of any local reactive effect — so FPP can be a
// pure audio source for a network of WLED reactive devices even with
// no overlay effects active locally. Cadence matches WLED's own
// audioreactive transmit rate (10ms / 100Hz).
void WLEDAudioSync::SendLoop() {
    sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_port = htons(m_port);
    if (inet_pton(AF_INET, m_destAddr.c_str(), &dst.sin_addr) != 1) {
        LogErr(VB_GENERAL, "WLEDAudioSync: invalid destination '%s'\n",
               m_destAddr.c_str());
        WarningHolder::AddWarning(46, "WLED Audio Sync: invalid destination address");
        return;
    }

    std::array<float, NUM_SAMPLES> samples;
    AudioFrame frame{};

    while (m_running) {
        int sampleRate = 0;
        if (!fetchAudioSamples(samples, sampleRate)) {
            // No audio source producing right now (nothing playing,
            // mic unconfigured, etc.). Wait a bit and re-check rather
            // than spinning.
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        computeAudioFrame(samples, sampleRate, frame);

        AudioSyncPacketV2 pkt{};
        std::memcpy(pkt.header, WLED_HEADER_V2, 5);  // header[6] keeps the trailing NUL
        pkt.sampleRaw     = static_cast<float>(frame.volumeRaw);
        pkt.sampleSmth    = frame.volumeSmth;
        pkt.samplePeak    = frame.samplePeak;
        std::memcpy(pkt.fftResult, frame.fftResult, 16);
        pkt.FFT_Magnitude = frame.FFT_Magnitude;
        pkt.FFT_MajorPeak = frame.FFT_MajorPeak;

        ::sendto(m_sendSocket, &pkt, sizeof(pkt), 0,
                 reinterpret_cast<sockaddr*>(&dst), sizeof(dst));

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void WLEDAudioSync::ReceiveLoop() {
    AudioSyncPacketV2 pkt;
    sockaddr_in from{};
    socklen_t fromLen = sizeof(from);
    bool firstPacketLogged = false;

    while (m_running) {
        fromLen = sizeof(from);
        ssize_t n = ::recvfrom(m_recvSocket, &pkt, sizeof(pkt), 0,
                               reinterpret_cast<sockaddr*>(&from), &fromLen);
        if (n <= 0) {
            // Socket closed by Cleanup(), or transient error — bail
            // out cleanly when we're shutting down.
            if (!m_running) break;
            continue;
        }

        // Loopback suppression: ignore packets from ourselves. Without
        // this, a node in `send` mode that hears its own multicast back
        // via host routing would inject its own FFT data on top of the
        // locally-computed FFT, oscillating.
        if (IsLocalAddress(from.sin_addr.s_addr)) continue;

        // V1 ("00001", 88-byte) packets exist for backwards-compat with
        // very old WLED firmware; we accept the V2 form only since that's
        // what every current WLED build emits.
        if (n != sizeof(AudioSyncPacketV2)) continue;
        if (std::memcmp(pkt.header, WLED_HEADER_V2, 5) != 0) continue;

        if (!firstPacketLogged) {
            char fromBuf[INET_ADDRSTRLEN] = {};
            inet_ntop(AF_INET, &from.sin_addr, fromBuf, sizeof(fromBuf));
            // Use raw fprintf rather than LogInfo: observed that the
            // FPPLogger may swallow messages from a brand-new thread on
            // its very first call.
            fprintf(stderr, "WLEDAudioSync: receiving audio sync from %s\n",
                    fromBuf);
            fflush(stderr);
            firstPacketLogged = true;
        }

        std::unique_lock<std::mutex> lock(m_cacheLock);
        m_cacheTimestampMs   = nowMs();
        std::memcpy(m_cachedFFT.data(), pkt.fftResult, 16);
        m_cachedVolumeSmth   = pkt.sampleSmth;
        m_cachedVolumeRaw    = static_cast<uint16_t>(pkt.sampleRaw);
        m_cachedSamplePeak   = pkt.samplePeak;
        m_cachedFFTMagnitude = pkt.FFT_Magnitude;
        m_cachedFFTMajorPeak = pkt.FFT_MajorPeak;
    }
}

bool WLEDAudioSync::InjectCachedFrame(uint8_t fftResult[16],
                                      float& volumeSmth,
                                      uint16_t& volumeRaw,
                                      uint8_t& samplePeak,
                                      float& FFT_Magnitude,
                                      float& FFT_MajorPeak) {
    if (m_mode != Receive) return false;

    std::unique_lock<std::mutex> lock(m_cacheLock);
    if (m_cacheTimestampMs == 0) return false;
    if (nowMs() - m_cacheTimestampMs > kFrameTtlMs) return false;

    std::memcpy(fftResult, m_cachedFFT.data(), 16);
    volumeSmth    = m_cachedVolumeSmth;
    volumeRaw     = m_cachedVolumeRaw;
    samplePeak    = m_cachedSamplePeak;
    FFT_Magnitude = m_cachedFFTMagnitude;
    FFT_MajorPeak = m_cachedFFTMajorPeak;
    return true;
}
