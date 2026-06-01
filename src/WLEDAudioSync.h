#pragma once
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

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <set>
#include <string>
#include <thread>

// Bridge between FPP's existing WLED-port audio reactive pipeline and
// other WLED nodes on the network using the standard WLED audiosync
// UDP protocol (port 11988, "00002" V2 packet format, 44 bytes).
//
// FPP already computes its own FFT (via kiss_fftr) from currently-
// playing media or a configured audio capture device — see
// WS2812FXExt::processSamples in src/overlays/wled/wled.cpp. This
// singleton:
//
//   - Send mode:    rate-limits and broadcasts the FFT data computed
//                   in processSamples so external WLED reactive
//                   devices can sync to whatever FPP is playing.
//
//   - Receive mode: listens for incoming audiosync packets, caches
//                   the latest, and injects it into WS2812FXExt's
//                   um_data so FPP's own reactive effects are driven
//                   by another device's microphone instead of (or in
//                   addition to) the local FFT.
//
//   - Off:          neither — pure passthrough, identical to the
//                   pre-existing FPP behavior.
//
// Configuration (in /media/settings):
//   WLEDAudioSyncMode    "off" | "send" | "receive"   (default off)
//   WLEDAudioSyncPort    int                          (default 11988)
//   WLEDAudioSyncDest    string IP                    (default 239.0.0.1)
class WLEDAudioSync {
public:
    enum Mode { Off = 0, Send = 1, Receive = 2 };

    static WLEDAudioSync INSTANCE;

    WLEDAudioSync();
    ~WLEDAudioSync();

    void Initialize();
    void Cleanup();

    // Re-read settings (mode, port, dest) and restart the appropriate
    // worker threads. Safe to call at any time; idempotent. Wired to
    // the FPP settings-listener so the user can flip between off / send
    // / receive without restarting fppd.
    void Reconfigure();

    // Called by WS2812FXExt::getAudioSamples before falling back to
    // the local audio source. When in Receive mode and a recent
    // packet is cached, populates the caller-provided fields directly
    // and returns true (caller skips local FFT). Otherwise returns
    // false and the local pipeline runs normally.
    bool InjectCachedFrame(uint8_t fftResult[16],
                           float& volumeSmth,
                           uint16_t& volumeRaw,
                           uint8_t& samplePeak,
                           float& FFT_Magnitude,
                           float& FFT_MajorPeak);

    // For status/debug.
    Mode GetMode() const { return m_mode; }
    int  GetPort() const { return m_port; }

    // Internal entry points for the worker threads (public so the
    // std::thread thunks can call them).
    void ReceiveLoop();
    void SendLoop();

private:
    bool OpenSendSocket();
    bool OpenReceiveSocket();
    void StopAllThreads();   // joins send + receive threads, closes sockets
    void CacheLocalIPs();
    bool IsLocalAddress(uint32_t ipv4_n) const; // network byte order

    // Reconfigure runs on the settings-callback thread; protect against
    // concurrent calls (e.g. user toggles three settings at once).
    std::mutex m_reconfigureLock;

    Mode m_mode = Off;
    int  m_port = 11988;
    std::string m_destAddr = "239.0.0.1";

    // Sender state.
    int  m_sendSocket = -1;
    std::thread* m_sendThread = nullptr;

    // Receiver state.
    int  m_recvSocket = -1;
    std::thread* m_recvThread = nullptr;
    std::atomic<bool> m_running{false};
    // Guards Cleanup() so the destructor can't run it a second time after
    // main() already did. The destructor runs during static teardown, where
    // re-entering the (possibly already-destroyed) global SettingsConfig to
    // unregister listeners throws out of a noexcept dtor -> std::terminate.
    std::atomic<bool> m_cleanedUp{false};

    // Cache of the most recent received audio frame. Refreshed by the
    // listener thread; consulted by InjectCachedFrame from the effect-
    // update thread. Frames older than kFrameTtlMs are ignored so the
    // local pipeline takes over if the source goes silent.
    std::mutex m_cacheLock;
    uint64_t m_cacheTimestampMs = 0;
    std::array<uint8_t, 16> m_cachedFFT{};
    float    m_cachedVolumeSmth = 0;
    uint16_t m_cachedVolumeRaw = 0;
    uint8_t  m_cachedSamplePeak = 0;
    float    m_cachedFFTMagnitude = 0;
    float    m_cachedFFTMajorPeak = 0;
    static constexpr uint64_t kFrameTtlMs = 250;

    // For loopback suppression in receive (and future "both") mode.
    std::set<uint32_t> m_localIPs;
};
