/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2025 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include "fpp-pch.h"

#include "fpp-json.h"
#include "fpphttp.h" // drogon/HTTP helpers used here; no longer pulled transitively (see fpphttp_types.h)

#include "Warnings.h" // WarningHolder -- needed directly for NOPCH builds

#include "AES67Manager.h"

#ifdef HAS_AES67_GSTREAMER

#include <gst/gst.h>

#if __has_include(<samplerate.h>)
#include <samplerate.h>
#define FPP_HAVE_SAMPLERATE 1
#endif

#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <deque>
#include <fstream>
#include <memory>
#include <sstream>

#include "common.h"
#include "log.h"
#include "settings.h"

// ──────────────────────────────────────────────────────────────────────────────
// AES67 namespace helpers
// ──────────────────────────────────────────────────────────────────────────────
namespace AES67 {

const char* GetSDPChannelNames(int channels) {
    switch (channels) {
        case 1:  return "M";
        case 2:  return "FL, FR";
        case 4:  return "FL, FR, RL, RR";
        case 6:  return "FL, FR, FC, LFE, RL, RR";
        case 8:  return "FL, FR, FC, LFE, RL, RR, SL, SR";
        default: return "";
    }
}

} // namespace AES67

// ──────────────────────────────────────────────────────────────────────────────
// Singleton instance
// ──────────────────────────────────────────────────────────────────────────────
static AES67Manager s_aes67Manager;
AES67Manager& AES67Manager::INSTANCE = s_aes67Manager;

AES67Manager::AES67Manager() {
    m_configPath = getFPPMediaDir("/config/pipewire-aes67-instances.json");
}

AES67Manager::~AES67Manager() {
    Shutdown();
}

// ──────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ──────────────────────────────────────────────────────────────────────────────
bool AES67Manager::Init() {
    if (m_initialized.load()) {
        return true;
    }

    // AES67 uses pipewiresrc/pipewiresink which require the full PipeWire graph.
    // Only proceed when MediaBackend is "pipewire" (the advanced PipeWire mode).
    // - ALSA / no backend: PipeWire daemon is not running; gst_parse_launch with
    //   pipewiresrc crashes in gst_value_deserialize (NULL deref at addr 0x4).
    // - pipewire-simple: the graph lacks the node connections needed for audio
    //   format negotiation, causing the state change to block indefinitely.
    std::string mediaBackend = toLowerCopy(getSetting("MediaBackend"));
    if (mediaBackend != "pipewire") {
        LogDebug(VB_MEDIAOUT, "AES67Manager: MediaBackend='%s' (need 'pipewire'), skipping AES67 init\n",
                 mediaBackend.c_str());
        return true;
    }

    // Check if the config file exists
    if (!FileExists(m_configPath)) {
        LogDebug(VB_MEDIAOUT, "AES67Manager: No config file at %s, skipping init\n",
                 m_configPath.c_str());
        return true;  // not an error — just no AES67 configured
    }

    // Ensure GStreamer is initialized
    if (!gst_is_initialized()) {
        gst_init(nullptr, nullptr);
    }

    // Set PipeWire env vars so pipewiresrc/pipewiresink can find the FPP PipeWire runtime
    setenv("PIPEWIRE_RUNTIME_DIR", "/run/pipewire-fpp", 0);
    setenv("XDG_RUNTIME_DIR", "/run/pipewire-fpp", 0);
    setenv("PULSE_RUNTIME_PATH", "/run/pipewire-fpp/pulse", 0);

    m_initialized.store(true);
    LogInfo(VB_MEDIAOUT, "AES67Manager: Initialized\n");
    return true;
}

void AES67Manager::Shutdown() {
    if (!m_initialized.load()) {
        return;
    }

    LogInfo(VB_MEDIAOUT, "AES67Manager: Shutting down\n");

    // Clear the initialized flag up front so the watchdog rebuild thread
    // (spawned by SAPAnnounceLoop(), see m_rebuildThread) observes
    // shutdown-in-progress and skips calling ApplyConfig() instead of
    // resurrecting pipelines after we tear them down below.
    m_initialized.store(false);

    // Join any in-flight watchdog rebuild thread FIRST, before touching
    // m_sapAnnounceThread/m_sapRecvThread.  The rebuild thread internally
    // calls ApplyConfig(), which itself joins those two threads -- joining
    // them here concurrently with the rebuild thread would be a double
    // join on the same std::thread object (undefined behavior).
    if (m_rebuildThread.joinable()) {
        m_rebuildThread.join();
    }

    // Only now take the apply lock -- m_rebuildThread calls ApplyConfig(),
    // so holding this across the join above would deadlock.  Taking it here
    // still blocks until any in-flight ApplyConfig() (e.g. from a command
    // thread) has finished, so we never tear down threads it is mid-rebuild.
    std::lock_guard<std::mutex> applyLock(m_applyMutex);

    // Stop SAP threads
    m_sapAnnounceRunning.store(false);
    m_sapRecvRunning.store(false);
    m_driftRunning.store(false);
    if (m_driftThread.joinable()) {
        m_driftThread.join();
    }
    if (m_sapAnnounceThread.joinable()) {
        m_sapAnnounceThread.join();
    }
    if (m_sapRecvThread.joinable()) {
        m_sapRecvThread.join();
    }

    StopAllPipelines();
    ShutdownPTP();
    ReleaseMediaClock();

    m_active.store(false);
    LogInfo(VB_MEDIAOUT, "AES67Manager: Shutdown complete\n");
}

// ──────────────────────────────────────────────────────────────────────────────
// Config loading — reads pipewire-aes67-instances.json
// ──────────────────────────────────────────────────────────────────────────────
// Accessors -- see m_configMutex.  Each copies out under the lock so callers
// never hold a reference into a vector or string LoadConfig() may reallocate.
AES67Config AES67Manager::GetConfigSnapshot() {
    std::lock_guard<std::mutex> lock(m_configMutex);
    return m_config;
}

bool AES67Manager::IsPtpEnabled() {
    std::lock_guard<std::mutex> lock(m_configMutex);
    return m_config.ptpEnabled;
}

int AES67Manager::GetPtpDomain() {
    std::lock_guard<std::mutex> lock(m_configMutex);
    return m_config.ptpDomain;
}

std::string AES67Manager::GetPtpInterface() {
    std::lock_guard<std::mutex> lock(m_configMutex);
    return m_config.ptpInterface;
}

bool AES67Manager::LoadConfig() {
    Json::Value root;
    if (!LoadJsonFromFile(m_configPath, root, JsonRoot::Object)) {
        LogWarn(VB_MEDIAOUT, "AES67Manager: Failed to load config from %s\n",
                m_configPath.c_str());
        return false;
    }

    // Parse into a local config and publish it in one locked swap at the end.
    // Filling m_config in place would let a status query on another thread
    // observe a half-built instance list, or iterate the vector while
    // push_back() reallocates it.
    // Every fallback below is the member's own default rather than a literal.
    // Spelling them out twice does not stay in step: nativeSourceRate and
    // sourceBufferCopy were changed to false in the struct and kept a literal
    // true here, so this function -- which always runs -- quietly put them
    // back.  The shipped behaviour was then the opposite of both the struct
    // and the commit message that changed it.
    AES67Config cfg;
    static const AES67Config kDefault;
    cfg.ptpEnabled = root.get("ptpEnabled", kDefault.ptpEnabled).asBool();
    cfg.ptpInterface = root.get("ptpInterface", kDefault.ptpInterface).asString();
    cfg.ptpDomain = root.get("ptpDomain", kDefault.ptpDomain).asInt();
    cfg.ptpRole = root.get("ptpRole", kDefault.ptpRole).asString();
    cfg.ptpMediaClock = root.get("ptpMediaClock", kDefault.ptpMediaClock).asBool();
    cfg.sourcePacing = root.get("sourcePacing", kDefault.sourcePacing).asBool();
    cfg.sinkPacing = root.get("sinkPacing", kDefault.sinkPacing).asBool();
    cfg.sinkPacingMs = root.get("sinkPacingMs", kDefault.sinkPacingMs).asInt();
    cfg.driftResample = root.get("driftResample", kDefault.driftResample).asBool();
    cfg.pipelineStats = root.get("pipelineStats", kDefault.pipelineStats).asBool();
    cfg.nativeSourceRate =
        root.get("nativeSourceRate", kDefault.nativeSourceRate).asBool();
    cfg.sourceBufferCopy =
        root.get("sourceBufferCopy", kDefault.sourceBufferCopy).asBool();
    cfg.sourceMinBuffers =
        root.get("sourceMinBuffers", kDefault.sourceMinBuffers).asInt();
    cfg.rateMatch = root.get("rateMatch", kDefault.rateMatch).asBool();
    cfg.rateMatchToleranceNs =
        (guint64)root.get("rateMatchToleranceNs",
                          (Json::UInt64)kDefault.rateMatchToleranceNs)
            .asUInt64();
    cfg.adaptiveResample = root.get("adaptiveResample", false).asBool();

    // A domain outside 0-127 is not representable in the PTP header; an
    // unknown role would silently fall through to the "auto" branch below,
    // so normalise both here where we can tell the user about it.
    if (cfg.ptpDomain < 0 || cfg.ptpDomain > 127) {
        LogWarn(VB_MEDIAOUT, "AES67Manager: Invalid PTP domain %d, using %d\n",
                cfg.ptpDomain, AES67::DEFAULT_PTP_DOMAIN);
        cfg.ptpDomain = AES67::DEFAULT_PTP_DOMAIN;
    }
    if (cfg.ptpRole != "auto" && cfg.ptpRole != "follower" &&
        cfg.ptpRole != "master") {
        LogWarn(VB_MEDIAOUT, "AES67Manager: Unknown PTP role '%s', using 'auto'\n",
                cfg.ptpRole.c_str());
        cfg.ptpRole = "auto";
    }

    if (root.isMember("instances") && root["instances"].isArray()) {
        for (const auto& instJson : root["instances"]) {
            AES67Instance inst;
            inst.id = instJson.get("id", 0).asInt();
            inst.name = instJson.get("name", "AES67").asString();
            inst.enabled = instJson.get("enabled", true).asBool();
            inst.mode = instJson.get("mode", "send").asString();
            inst.multicastIP = instJson.get("multicastIP", AES67::DEFAULT_MULTICAST_IP).asString();
            inst.port = instJson.get("port", AES67::DEFAULT_PORT).asInt();
            inst.channels = instJson.get("channels", AES67::DEFAULT_CHANNELS).asInt();
            inst.interface = instJson.get("interface", "").asString();
            inst.sessionName = instJson.get("sessionName", inst.name).asString();
            inst.latency = instJson.get("latency", AES67::DEFAULT_LATENCY_MS).asInt();
            inst.sapEnabled = instJson.get("sapEnabled", true).asBool();
            inst.ptime = instJson.get("ptime", AES67::DEFAULT_PTIME_MS).asInt();

            // Validate ptime
            if (!AES67::IsValidPtime(inst.ptime)) {
                inst.ptime = AES67::DEFAULT_PTIME_MS;
            }

            cfg.instances.push_back(inst);
        }
    }

    LogInfo(VB_MEDIAOUT, "AES67Manager: Loaded config with %d instances, PTP=%s interface=%s domain=%d role=%s\n",
            (int)cfg.instances.size(),
            cfg.ptpEnabled ? "enabled" : "disabled",
            cfg.ptpInterface.c_str(),
            cfg.ptpDomain,
            cfg.ptpRole.c_str());

    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        m_config = std::move(cfg);
    }
    return true;
}

// ──────────────────────────────────────────────────────────────────────────────
// ApplyConfig — called from PHP API and boot sequence
// ──────────────────────────────────────────────────────────────────────────────
bool AES67Manager::ApplyConfig() {
    // Serialize against concurrent ApplyConfig()/Shutdown()/Cleanup() calls -
    // see m_applyMutex.  Without this, two callers can both get past the SAP
    // thread joins below and both reach the std::thread assignments at the
    // end, where assigning over a joinable thread calls std::terminate().
    std::lock_guard<std::mutex> applyLock(m_applyMutex);

    if (!m_initialized.load()) {
        if (!Init()) {
            return false;
        }
        // Init() may return true but skip initialization (e.g. wrong backend).
        // If m_initialized is still false, there's nothing to do.
        if (!m_initialized.load()) {
            return true;
        }
    }

    // Stop existing pipelines and SAP threads
    m_sapAnnounceRunning.store(false);
    m_sapRecvRunning.store(false);
    m_driftRunning.store(false);
    if (m_driftThread.joinable()) {
        m_driftThread.join();
    }
    if (m_sapAnnounceThread.joinable()) {
        m_sapAnnounceThread.join();
    }
    if (m_sapRecvThread.joinable()) {
        m_sapRecvThread.join();
    }
    StopAllPipelines();
    ShutdownPTP();
    // Re-resolved below against the new config: the PTP interface (and so the
    // PHC backing the media clock) may have changed.
    ReleaseMediaClock();

    if (!FileExists(m_configPath)) {
        LogInfo(VB_MEDIAOUT, "AES67Manager: No config file, nothing to apply\n");
        m_active.store(false);
        return true;
    }

    if (!LoadConfig()) {
        return false;
    }

    // Count enabled instances
    int enabledCount = 0;
    for (const auto& inst : m_config.instances) {
        if (inst.enabled) enabledCount++;
    }

    if (enabledCount == 0) {
        LogInfo(VB_MEDIAOUT, "AES67Manager: No enabled instances\n");
        m_active.store(false);
        return true;
    }

    // Initialize PTP if enabled
    if (m_config.ptpEnabled) {
        if (!InitPTP()) {
            LogWarn(VB_MEDIAOUT, "AES67Manager: PTP init failed, continuing without PTP clock\n");
        }
    }

    // Create pipelines for each enabled instance
    bool anySend = false;
    bool anyRecv = false;
    bool anySAP = false;

    for (const auto& inst : m_config.instances) {
        if (!inst.enabled) continue;

        bool wantSend = (inst.mode == "send" || inst.mode == "both");
        bool wantRecv = (inst.mode == "receive" || inst.mode == "both");

        if (wantSend) {
            if (CreateSendPipeline(inst)) {
                anySend = true;
            }
        }

        if (wantRecv) {
            if (CreateRecvPipeline(inst)) {
                anyRecv = true;
            }
        }

        if (inst.sapEnabled) {
            anySAP = true;
        }
    }

    // Start the drift control loop if anything is sending on the PTP clock
    if (anySend && m_config.ptpMediaClock && m_config.adaptiveResample) {
        m_driftRunning.store(true);
        m_driftThread = std::thread(&AES67Manager::DriftControlLoop, this);
    }

    // Start SAP announcer if any send instances have SAP enabled
    if (anySAP && anySend) {
        m_sapAnnounceRunning.store(true);
        m_sapAnnounceThread = std::thread(&AES67Manager::SAPAnnounceLoop, this);
        LogInfo(VB_MEDIAOUT, "AES67Manager: SAP announcer started\n");
    }

    // Start SAP receiver if any receive instances or SAP enabled
    if (anySAP) {
        m_sapRecvRunning.store(true);
        m_sapRecvThread = std::thread(&AES67Manager::SAPReceiveLoop, this);
        LogInfo(VB_MEDIAOUT, "AES67Manager: SAP receiver started\n");
    }

    m_active.store(true);
    LogInfo(VB_MEDIAOUT, "AES67Manager: Applied config — %d send, %d receive pipelines\n",
            (int)m_sendPipelines.size(), (int)m_recvPipelines.size());
    return true;
}

void AES67Manager::Cleanup() {
    LogInfo(VB_MEDIAOUT, "AES67Manager: Cleanup\n");

    std::lock_guard<std::mutex> applyLock(m_applyMutex);

    m_sapAnnounceRunning.store(false);
    m_sapRecvRunning.store(false);
    m_driftRunning.store(false);
    if (m_driftThread.joinable()) {
        m_driftThread.join();
    }
    if (m_sapAnnounceThread.joinable()) {
        m_sapAnnounceThread.join();
    }
    if (m_sapRecvThread.joinable()) {
        m_sapRecvThread.join();
    }
    StopAllPipelines();
    ShutdownPTP();
    ReleaseMediaClock();

    m_active.store(false);
}

void AES67Manager::OnPipeWireReady() {
    // PipeWire has been restarted — if we have a config, apply it
    if (FileExists(m_configPath)) {
        LogInfo(VB_MEDIAOUT, "AES67Manager: PipeWire ready, applying AES67 config\n");
        ApplyConfig();
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// PTP clock management — uses ptp4l (linuxptp) as an IEEE 1588 PTP daemon
//
// GStreamer's gst-ptp-helper is a PTP *client only* — it listens for PTP
// Announce/Sync messages but never originates them.  For AES67, FPP must
// participate in PTP as either grandmaster or follower (via BMCA).
//
// We launch ptp4l as a managed subprocess with an AES67-appropriate config:
//   - Configurable domain (default 0), two-step, announce every 2s, 8 syncs/sec
//   - Hardware timestamping when available (/dev/ptp0)
//   - BMCA priority driven by the "ptpRole" setting -- see AES67Config
//   - DSCP EF (46) on PTP event/general messages -- see AES67::PTP_DSCP
//
// phc2sys is deliberately not launched -- see the note in InitPTP().  The
// pipeline reads PTP time directly rather than via the system clock.
// ──────────────────────────────────────────────────────────────────────────────

// Absolute path so we do not depend on whatever PATH systemd handed fppd --
// the ptp4l presence check below is absolute for the same reason.
static const char* PMC_BINARY = "/usr/sbin/pmc";

bool AES67Manager::WritePtpConf(const std::string& path, bool hwTimestamping, bool includeDscp) {
    std::ofstream conf(path);
    if (!conf.is_open()) {
        LogErr(VB_MEDIAOUT, "AES67Manager: Cannot write PTP config to %s\n", path.c_str());
        WarningHolder::AddWarning(45, "AES67: could not write PTP configuration file");
        return false;
    }

    // BMCA behaviour.  "auto" deliberately runs at a worse priority1 than the
    // 128 that professional gear ships with: a tie on priority is broken by
    // clock identity (lowest MAC wins), which is how an FPP box ends up
    // grandmastering a Q-SYS or Yamaha domain it should have been following.
    int priority1 = AES67::PTP_PRIORITY_AUTO;
    bool slaveOnly = false;
    if (m_config.ptpRole == "master") {
        priority1 = AES67::PTP_PRIORITY_PREFER_MASTER;
    } else if (m_config.ptpRole == "follower") {
        slaveOnly = true;
    }

    // AES67 media profile: announce every 2s, sync 8/sec, delay req 1/sec.
    conf << "[global]\n"
         << "domainNumber\t\t" << m_config.ptpDomain << "\n"
         << "twoStepFlag\t\t1\n"
         << "slaveOnly\t\t" << (slaveOnly ? 1 : 0) << "\n"
         << "priority1\t\t" << priority1 << "\n"
         << "priority2\t\t128\n"
         << "clockClass\t\t248\n"
         << "clockAccuracy\t\t0xFE\n"
         << "offsetScaledLogVariance\t0xFFFF\n"
         << "logAnnounceInterval\t1\n"     // 1 announce/2s — matches announceReceiptTimeout cadence
         << "logSyncInterval\t\t-3\n"      // 8/sec (AES67 recommends -3)
         << "logMinDelayReqInterval\t0\n"
         << "announceReceiptTimeout\t3\n"
         << "syncReceiptTimeout\t0\n"
         << "transportSpecific\t0x0\n"
         << "network_transport\tUDPv4\n"
         << "delay_mechanism\t\tE2E\n"
         << "time_stamping\t\t" << (hwTimestamping ? "hardware" : "software") << "\n";

    if (includeDscp) {
        conf << "dscp_event\t\t" << AES67::PTP_DSCP << "\n"   // EF (46) -- Sync/Delay_Req
             << "dscp_general\t\t" << AES67::PTP_DSCP << "\n"; // EF (46) -- Announce/Follow_Up/etc.
    }
    conf << "# AES67 uses L2 multicast on 224.0.1.129/224.0.0.107\n";
    conf.close();
    return true;
}

bool AES67Manager::StartPtp4l(bool hwTimestamping, bool includeDscp) {
    if (!WritePtpConf(m_ptpConfPath, hwTimestamping, includeDscp)) {
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        LogErr(VB_MEDIAOUT, "AES67Manager: fork() failed for ptp4l: %s\n", FPPstrerror(errno));
        WarningHolder::AddWarning(45, "AES67: could not start the ptp4l clock-sync process");
        return false;
    }
    if (pid == 0) {
        // Child process — exec ptp4l (-m: log to stderr)
        execlp("ptp4l", "ptp4l",
               "-i", m_config.ptpInterface.c_str(),
               "-f", m_ptpConfPath.c_str(),
               "-m",
               nullptr);
        // If exec fails
        _exit(127);
    }
    m_ptp4lPid = pid;

    // Give ptp4l a moment to start, then check it did not exit immediately
    // (bad config key, no hardware timestamping, interface down, ...).
    usleep(500000);  // 500ms
    if (!IsPtp4lRunning()) {
        m_ptp4lPid = -1;
        return false;
    }

    LogInfo(VB_MEDIAOUT, "AES67Manager: ptp4l started (PID %d) on %s — %s timestamping%s, domain %d, role %s\n",
            (int)m_ptp4lPid, m_config.ptpInterface.c_str(),
            hwTimestamping ? "hardware" : "software",
            includeDscp ? "" : ", DSCP disabled",
            m_config.ptpDomain, m_config.ptpRole.c_str());
    return true;
}

bool AES67Manager::InitPTP() {
    if (m_ptpInitialized) {
        return true;
    }

    // Check if ptp4l binary exists
    if (!FileExists("/usr/sbin/ptp4l")) {
        LogErr(VB_MEDIAOUT, "AES67Manager: ptp4l not found — install linuxptp package\n");
        WarningHolder::AddWarning(45, "AES67: ptp4l not found — install the linuxptp package");
        return false;
    }

    m_ptpConfPath = "/tmp/fpp-ptp4l.conf";

    LogInfo(VB_MEDIAOUT, "AES67Manager: Starting ptp4l on %s (AES67 profile, domain %d, role %s)\n",
            m_config.ptpInterface.c_str(), m_config.ptpDomain, m_config.ptpRole.c_str());

    // Start attempts, most capable first.  The last one drops the DSCP keys:
    // they are only understood by linuxptp >= 2.0, and an unknown key is a
    // hard config-parse failure, which would otherwise take PTP down entirely
    // on an older install rather than just losing the QoS marking.
    if (!StartPtp4l(true, true)) {
        LogErr(VB_MEDIAOUT, "AES67Manager: ptp4l exited immediately — "
               "check hardware timestamping support on %s\n",
               m_config.ptpInterface.c_str());
        LogInfo(VB_MEDIAOUT, "AES67Manager: Retrying ptp4l with software timestamping\n");

        if (!StartPtp4l(false, true)) {
            LogInfo(VB_MEDIAOUT, "AES67Manager: Retrying ptp4l without DSCP marking "
                    "(linuxptp may predate dscp_event/dscp_general)\n");

            if (!StartPtp4l(false, false)) {
                LogErr(VB_MEDIAOUT, "AES67Manager: ptp4l failed to start\n");
                WarningHolder::AddWarning(45, "AES67: PTP clock sync (ptp4l) could not start on the configured interface");
                m_ptp4lPid = -1;
                return false;
            }
        }
    }

    // phc2sys is deliberately NOT started.
    //
    // Its only job was to copy PTP time onto the system clock, and nothing
    // needs that any more: the media clock reads the PTP time source directly
    // (the PHC with hardware timestamping, CLOCK_REALTIME with software), so
    // the pipeline no longer depends on the system clock tracking PTP.
    //
    // Running it is actively harmful against real grandmasters.  PTP's
    // timescale is whatever the grandmaster distributes, and Dante/Brooklyn
    // devices commonly distribute an arbitrary epoch rather than wall time --
    // one measured at ~4411 seconds, i.e. time since the device booted.
    // "phc2sys -a -r" faithfully slaved CLOCK_REALTIME to that and dragged the
    // Pi's clock back to 1970 (reported on issue #2848), taking the scheduler,
    // logs and anything else on wall time with it.
    //
    // A box that wants PTP-disciplined system time should run phc2sys from
    // systemd with an offset appropriate to its grandmaster; that is a
    // deliberate site decision, not something an audio stream should impose.

    m_ptpInitialized = true;
    LogInfo(VB_MEDIAOUT, "AES67Manager: PTP initialized — ptp4l PID %d on %s\n",
            (int)m_ptp4lPid, m_config.ptpInterface.c_str());
    return true;
}

// SIGTERM then SIGKILL, polling until the process is really gone.
//
// fppd installs SIGCHLD with SA_NOCLDWAIT (see fppd.cpp), so children are
// reaped by init and waitpid() here fails with ECHILD immediately -- it never
// actually waited.  That let ApplyConfig() launch a replacement ptp4l while
// the old one still held /var/run/ptp4l and the PTP ports, leaving two
// daemons briefly running BMCA against each other on one interface.
static void StopChildProcess(pid_t& pid, const char* name) {
    if (pid <= 0) {
        return;
    }
    LogInfo(VB_MEDIAOUT, "AES67Manager: Stopping %s (PID %d)\n", name, (int)pid);
    kill(pid, SIGTERM);

    // Up to ~3s for a clean exit, then insist.
    for (int i = 0; i < 60; i++) {
        // Harmless no-op under SA_NOCLDWAIT; reaps the child if a future
        // change turns that off.
        int status = 0;
        waitpid(pid, &status, WNOHANG);
        if (kill(pid, 0) != 0) {
            pid = -1;
            return;
        }
        usleep(50000);
    }

    LogWarn(VB_MEDIAOUT, "AES67Manager: %s (PID %d) did not exit, sending SIGKILL\n",
            name, (int)pid);
    kill(pid, SIGKILL);
    for (int i = 0; i < 20; i++) {
        int status = 0;
        waitpid(pid, &status, WNOHANG);
        if (kill(pid, 0) != 0) {
            break;
        }
        usleep(50000);
    }
    pid = -1;
}

void AES67Manager::ShutdownPTP() {
    StopChildProcess(m_phc2sysPid, "phc2sys");
    StopChildProcess(m_ptp4lPid, "ptp4l");

    if (!m_ptpConfPath.empty()) {
        unlink(m_ptpConfPath.c_str());
        m_ptpConfPath.clear();
    }
    m_ptpInitialized = false;

    std::lock_guard<std::mutex> lock(m_ptpCacheMutex);
    m_ptpCache = PtpQueryCache();
}

bool AES67Manager::IsPtp4lRunning() const {
    if (m_ptp4lPid <= 0) return false;
    // kill(pid, 0) checks if process exists without sending a signal.  That
    // alone is not enough here: SA_NOCLDWAIT means a dead ptp4l leaves no
    // zombie holding its slot, so the PID can be recycled by an unrelated
    // process and we would report a long-dead daemon as healthy.  Confirm the
    // name too.
    if (kill(m_ptp4lPid, 0) != 0) {
        return false;
    }
    std::string comm = "/proc/" + std::to_string((int)m_ptp4lPid) + "/comm";
    std::ifstream f(comm);
    if (!f.is_open()) {
        // No procfs (macOS) — fall back to the signal probe alone.
        return true;
    }
    std::string name;
    std::getline(f, name);
    return name == "ptp4l";
}

// Runs a `pmc` management query against ptp4l's UDS socket and returns the
// raw text output, or an empty string if ptp4l isn't running / pmc fails.
static std::string RunPmcQuery(const std::string& tlv, int domain) {
    struct PipeCloser {
        void operator()(FILE* f) const {
            // pclose() returns -1 under fppd's SA_NOCLDWAIT (init already
            // reaped the child); the output has been read by then, so the
            // status is of no use to us either way.
            if (f) pclose(f);
        }
    };
    // ptp4l silently drops a management message whose domainNumber does not
    // match its own -- even over the UDS socket -- so -d is not optional once
    // the domain is configurable.
    std::string cmd = std::string(PMC_BINARY) + " -u -b 0 -d " +
                      std::to_string(domain) + " '" + tlv + "' 2>/dev/null";
    std::unique_ptr<FILE, PipeCloser> pipe(popen(cmd.c_str(), "r"));
    if (!pipe) {
        return "";
    }
    std::string output;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
        output += buffer;
    }
    return output;
}

// Reformats a pmc clockIdentity like "aabbcc.fffe.ddeeff" into the dashed
// EUI-64 form used elsewhere in this file ("AA-BB-CC-FF-FE-DD-EE-FF").
static std::string FormatPmcClockId(const std::string& raw) {
    std::string hex;
    for (char c : raw) {
        if (c != '.') hex += c;
    }
    if (hex.length() != 16) {
        return raw;
    }
    std::string dashed;
    for (size_t i = 0; i < hex.length(); i += 2) {
        if (i) dashed += '-';
        dashed += (char)toupper((unsigned char)hex[i]);
        dashed += (char)toupper((unsigned char)hex[i + 1]);
    }
    return dashed;
}

// True if a ptp4l portState string means "this node is the domain master".
// PRE_MASTER is deliberately excluded -- it is a transitional state on the
// way to MASTER, not a settled BMCA outcome.
static bool IsGrandmasterPortState(const std::string& portState) {
    return portState == "MASTER" || portState == "GRAND_MASTER";
}

// Runs both management queries at most once per PTP_QUERY_CACHE_MS and keeps
// the parsed results.  /aes67/status is HTTP-facing and the SAP thread polls
// once a second while BMCA settles; without this each of those forks a pmc.
void AES67Manager::RefreshPtpCache(bool force) {
    std::lock_guard<std::mutex> lock(m_ptpCacheMutex);

    auto now = std::chrono::steady_clock::now();
    if (!force && m_ptpCache.valid) {
        auto age = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_ptpCache.when);
        if (age.count() < AES67::PTP_QUERY_CACHE_MS) {
            return;
        }
    }

    PtpQueryCache fresh;
    fresh.when = now;

    if (!IsPtp4lRunning()) {
        fresh.valid = true;
        fresh.portState = "not running";
        m_ptpCache = fresh;
        return;
    }

    // Grandmaster / offset
    int domain = GetPtpDomain();
    std::string output = RunPmcQuery("GET TIME_STATUS_NP", domain);
    std::istringstream iss(output);
    std::string line;
    while (std::getline(iss, line)) {
        std::istringstream ls(line);
        std::string key;
        ls >> key;
        if (key == "gmPresent") {
            std::string val;
            ls >> val;
            fresh.gmPresent = (val == "true");
            fresh.valid = true;
        } else if (key == "gmIdentity") {
            std::string val;
            ls >> val;
            if (!val.empty()) {
                fresh.gmIdentity = FormatPmcClockId(val);
                fresh.valid = true;
            }
        } else if (key == "master_offset") {
            std::string val;
            ls >> val;
            try {
                fresh.offsetNs = std::stoll(val);
            } catch (...) {
                // leave offsetNs at 0 on parse failure
            }
        }
    }

    // Port state
    std::string portOutput = RunPmcQuery("GET PORT_DATA_SET", domain);
    std::istringstream pss(portOutput);
    while (std::getline(pss, line)) {
        std::istringstream ls(line);
        std::string key;
        ls >> key;
        if (key == "portState") {
            std::string val;
            ls >> val;
            if (!val.empty()) {
                fresh.portState = val;
                break;
            }
        }
    }
    if (fresh.portState.empty()) {
        fresh.portState = "running (state unknown)";
    }

    m_ptpCache = fresh;
}

std::string AES67Manager::GetPtp4lState() {
    RefreshPtpCache();
    std::lock_guard<std::mutex> lock(m_ptpCacheMutex);
    return m_ptpCache.portState;
}

// Queries the *actual* domain grandmaster via `pmc GET TIME_STATUS_NP` —
// this is the remote/upstream clock ptp4l has selected via BMCA, which may
// or may not be this node.  GetPTPClockId() (below) only ever returns this
// node's own identity and must not be used to report "who is the master".
bool AES67Manager::QueryPtp4lTimeStatus(bool& gmPresent, std::string& gmIdentity, int64_t& offsetNs) {
    if (!IsPtp4lRunning()) {
        return false;
    }
    RefreshPtpCache();

    std::lock_guard<std::mutex> lock(m_ptpCacheMutex);
    if (!m_ptpCache.valid || m_ptpCache.gmIdentity.empty()) {
        return false;
    }
    gmPresent = m_ptpCache.gmPresent;
    gmIdentity = m_ptpCache.gmIdentity;
    offsetNs = m_ptpCache.offsetNs;
    return true;
}

// The clock identity to advertise/report: the upstream grandmaster we follow,
// or our own identity when we hold the role.  Empty while ptp4l is still
// converging, so callers can avoid announcing a refclk that is about to
// change.
std::string AES67Manager::GetActiveGrandmasterId() {
    if (!IsPtpEnabled() || !IsPtp4lRunning()) {
        return "";
    }

    bool gmPresent = false;
    std::string gmId;
    int64_t offsetNs = 0;
    if (QueryPtp4lTimeStatus(gmPresent, gmId, offsetNs) && gmPresent && !gmId.empty()) {
        return gmId;
    }
    if (IsGrandmasterPortState(GetPtp4lState())) {
        // We won the BMCA -- we are the refclk.
        return GetPTPClockId();
    }
    return "";
}

std::string AES67Manager::GetPTPClockId() {
    // Derive EUI-64 clock ID from interface MAC address
    // Read /sys/class/net/<iface>/address → AA:BB:CC:DD:EE:FF
    // Insert FF:FE → AA-BB-CC-FF-FE-DD-EE-FF
    std::string macPath = "/sys/class/net/" + GetPtpInterface() + "/address";
    std::ifstream macFile(macPath);
    if (!macFile.is_open()) {
        LogWarn(VB_MEDIAOUT, "AES67Manager: Cannot read MAC from %s\n", macPath.c_str());
        return "00-00-00-FF-FE-00-00-00";
    }

    std::string mac;
    std::getline(macFile, mac);
    macFile.close();

    // Parse MAC: "aa:bb:cc:dd:ee:ff"
    unsigned int m[6] = {0};
    if (sscanf(mac.c_str(), "%x:%x:%x:%x:%x:%x",
               &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) != 6) {
        return "00-00-00-FF-FE-00-00-00";
    }

    // EUI-64: m0-m1-m2-FF-FE-m3-m4-m5
    char eui64[24];
    snprintf(eui64, sizeof(eui64), "%02X-%02X-%02X-FF-FE-%02X-%02X-%02X",
             m[0], m[1], m[2], m[3], m[4], m[5]);
    return std::string(eui64);
}

// ──────────────────────────────────────────────────────────────────────────────
// AES67 media clock
//
// AES67 requires the RTP timestamp to be the PTP media clock count, and the SDP
// we publish asserts exactly that with "a=mediaclk:direct=0".  Honouring it
// means the pipeline has to be driven by PTP time itself, not by GStreamer's
// default monotonic system clock.
//
// Which clock actually holds PTP time depends on how ptp4l is running, and it
// has to be right in BOTH roles:
//
//   hardware timestamping -- ptp4l's clock is the NIC's PHC.  As a follower it
//       disciplines the PHC to the grandmaster; as grandmaster it distributes
//       the PHC's own time.  Either way the PHC is the domain's time, so we
//       read the PHC.
//
//   software timestamping -- there is no PHC; ptp4l uses CLOCK_REALTIME as its
//       clock, disciplining it as a follower and distributing it as
//       grandmaster.  So CLOCK_REALTIME is the domain's time.
//
// Note what we deliberately do NOT use:
//
//   CLOCK_TAI / GST_CLOCK_TYPE_TAI -- only correct while something is setting
//       the kernel TAI offset, which phc2sys does as a follower and not at all
//       as grandmaster.  Measured on a grandmaster box: adjtimex offset 0 and
//       CLOCK_TAI == CLOCK_REALTIME, i.e. silently not PTP time.
//
//   GstPtpClock (gst_ptp_init) -- would run gst-ptp-helper as a second PTP
//       client on the same domain alongside our ptp4l, adding a participant to
//       a network we just stopped FPP from disrupting.
// ──────────────────────────────────────────────────────────────────────────────

// Linux exposes a dynamic POSIX clock for an open /dev/ptpN fd.
#define FPP_FD_TO_CLOCKID(fd) ((clockid_t)((((unsigned int)~(fd)) << 3) | 3))

// Resolve the PHC backing an interface via ETHTOOL_GET_TS_INFO.  Returns -1
// when the NIC has no PHC (software timestamping), which is not an error.
static int PhcIndexForInterface(const std::string& iface) {
    if (iface.empty()) {
        return -1;
    }
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return -1;
    }
    struct ethtool_ts_info tsi;
    memset(&tsi, 0, sizeof(tsi));
    tsi.cmd = ETHTOOL_GET_TS_INFO;

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", iface.c_str());
    ifr.ifr_data = (char*)&tsi;

    int idx = -1;
    if (ioctl(sock, SIOCETHTOOL, &ifr) == 0) {
        idx = tsi.phc_index;
    }
    close(sock);
    return idx;
}

// GstClock reading PTP time.  Subclasses GstSystemClock so that all of its
// wait/scheduling machinery keeps working -- only the time source changes,
// since GstSystemClock routes its waits through the virtual get_internal_time.
struct FppPtpClock {
    GstSystemClock parent;
    clockid_t clkid;   // PHC dynamic clockid, or CLOCK_REALTIME
    int phcFd;         // open /dev/ptpN, or -1 when using CLOCK_REALTIME
};
struct FppPtpClockClass {
    GstSystemClockClass parent_class;
};

G_DEFINE_TYPE(FppPtpClock, fpp_ptp_clock, GST_TYPE_SYSTEM_CLOCK)

static GstClockTime fpp_ptp_clock_get_internal_time(GstClock* clock) {
    FppPtpClock* self = (FppPtpClock*)clock;
    struct timespec ts;
    if (clock_gettime(self->clkid, &ts) != 0) {
        // Losing the clock mid-stream would wedge every waiting element, so
        // fall back rather than returning an error the caller cannot act on.
        clock_gettime(CLOCK_REALTIME, &ts);
    }
    return GST_TIMESPEC_TO_TIME(ts);
}

static void fpp_ptp_clock_finalize(GObject* object) {
    FppPtpClock* self = (FppPtpClock*)object;
    if (self->phcFd >= 0) {
        close(self->phcFd);
        self->phcFd = -1;
    }
    G_OBJECT_CLASS(fpp_ptp_clock_parent_class)->finalize(object);
}

static void fpp_ptp_clock_class_init(FppPtpClockClass* klass) {
    GST_CLOCK_CLASS(klass)->get_internal_time = fpp_ptp_clock_get_internal_time;
    G_OBJECT_CLASS(klass)->finalize = fpp_ptp_clock_finalize;
}

static void fpp_ptp_clock_init(FppPtpClock* self) {
    self->clkid = CLOCK_REALTIME;
    self->phcFd = -1;
}


// Opens the PTP time source and wraps it in a GstClock.  One clock is shared by
// every send pipeline: they must all carry the same media timeline, and a
// second clock object would mean a second set of rate estimates.
GstClock* AES67Manager::GetOrCreateMediaClock() {
    std::lock_guard<std::mutex> lock(m_ptpClockMutex);
    if (m_ptpClock) {
        return m_ptpClock;
    }
    if (!IsPtpEnabled()) {
        return nullptr;
    }

    FppPtpClock* clock = (FppPtpClock*)g_object_new(fpp_ptp_clock_get_type(),
                                                    "name", "fppaes67ptpclock", NULL);
    if (!clock) {
        return nullptr;
    }

    std::string iface = GetPtpInterface();
    int phcIndex = PhcIndexForInterface(iface);
    if (phcIndex >= 0) {
        std::string dev = "/dev/ptp" + std::to_string(phcIndex);
        int fd = open(dev.c_str(), O_RDONLY);
        if (fd >= 0) {
            clock->phcFd = fd;
            clock->clkid = FPP_FD_TO_CLOCKID(fd);
            // Prove it is readable before we hand it to a pipeline -- a PHC
            // that exists but will not answer would stall every element
            // waiting on it.
            struct timespec ts;
            if (clock_gettime(clock->clkid, &ts) == 0) {
                LogInfo(VB_MEDIAOUT, "AES67 media clock: using PHC %s (interface %s)\n",
                        dev.c_str(), iface.c_str());
                m_ptpClock = GST_CLOCK(clock);
                return m_ptpClock;
            }
            LogWarn(VB_MEDIAOUT, "AES67 media clock: %s is not readable (%s), "
                    "falling back to CLOCK_REALTIME\n", dev.c_str(), FPPstrerror(errno));
            close(fd);
            clock->phcFd = -1;
        } else {
            LogWarn(VB_MEDIAOUT, "AES67 media clock: cannot open %s (%s), "
                    "falling back to CLOCK_REALTIME\n", dev.c_str(), FPPstrerror(errno));
        }
    }

    // Software-timestamping path: ptp4l has no PHC and uses CLOCK_REALTIME as
    // its own clock, so that is the domain's time in both roles.
    clock->clkid = CLOCK_REALTIME;
    LogInfo(VB_MEDIAOUT, "AES67 media clock: using CLOCK_REALTIME "
            "(no PHC on %s — software timestamping)\n", iface.c_str());
    m_ptpClock = GST_CLOCK(clock);
    return m_ptpClock;
}

void AES67Manager::ReleaseMediaClock() {
    std::lock_guard<std::mutex> lock(m_ptpClockMutex);
    if (m_ptpClock) {
        gst_object_unref(m_ptpClock);
        m_ptpClock = nullptr;
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Pipeline creation — Send
// ──────────────────────────────────────────────────────────────────────────────

#ifdef FPP_HAVE_SAMPLERATE
// ─────────────────────────────────────────────────────────────────────────────
// Media clock drift correction
//
// The audio is produced on the sound card's crystal and the RTP timeline runs
// on the NIC's PHC; on the reference hardware those differ by ~56ppm, which is
// 3.4ms per minute.  That empties any receiver's buffer, and it empties the
// sinkPacing queue, so it has to be corrected in the sender.
//
// Three GStreamer elements were tried first and all three are recorded as dead
// ends in AES67Config: "speed" and "pitch" accept a rate property and never
// apply it to a live stream, and "audiorate" reconciles per-buffer timestamp
// rounding rather than drift and then runs away.  libsamplerate is used
// instead because it is built for exactly this: src_process() takes a ratio per
// block, interpolates it across the block so there is no zipper, and keeps a
// fractional read pointer across calls.
//
// The control law deliberately leads with a direct measurement rather than a
// feedback loop, because every previous attempt here oscillated:
//
//   feedforward -- the card's true rate is (input frames / PHC seconds), which
//     is two crystals and therefore almost perfectly constant.  The ratio that
//     cancels it is just AUDIO_RATE / that.  This carries essentially all of
//     the correction and needs no gain.
//   feedback -- a small proportional term bleeds off accumulated offset, so a
//     startup transient or a rounding residue does not persist.  It is
//     deliberately weak; it is a trim, not the controller.
//
// The failure that killed audiorate is handled explicitly: on DISCONT the
// counters re-anchor rather than treating the gap as drift to be made up.
struct DriftResampleState {
    SRC_STATE* src = nullptr;
    int channels = 2;
    GstClock* clock = nullptr;
    int instanceId = 0;

    // Output timeline: monotonic, only re-anchored on a real discontinuity.
    bool anchored = false;
    GstClockTime anchorPts = 0;
    guint64 ptsFrames = 0;

    // Control counters: zeroed again once the pipeline is up to speed.  Keeping
    // these separate is the point -- a pipeline takes a moment to start
    // flowing, and counting that startup gap as drift makes the loop chase a
    // deficit it did not cause (measured: -188ms, which pinned the trim at the
    // clamp and ran the stream 2000ppm fast for minutes).
    bool warmed = false;
    GstClockTime ctlClock = 0;
    guint64 ctlIn = 0;
    guint64 ctlOut = 0;
    double ratio = 1.0;

    std::vector<float> out;
    guint64 buffers = 0;
    guint64 shortReads = 0;

    // Sliding window of (clock, input frames, output frames) used to measure
    // the card's rate.  A cumulative average was used here first and it is what
    // made the loop oscillate: it is itself an integrator, so with the phase
    // feedback -- which integrates too -- the loop had two in series.  It also
    // grew steadily less responsive as the run went on, which showed up as a
    // trim still crawling towards its answer 26 minutes in.
    struct Sample {
        GstClockTime t;
        guint64 in;
        guint64 out;
    };
    std::deque<Sample> window;
};

static void DestroyDriftResampleState(gpointer data) {
    auto* st = static_cast<DriftResampleState*>(data);
    if (st->src) {
        src_delete(st->src);
    }
    if (st->clock) {
        gst_object_unref(st->clock);
    }
    delete st;
}

static GstPadProbeReturn DriftResampleProbe(GstPad* pad, GstPadProbeInfo* info,
                                            gpointer user) {
    auto* st = static_cast<DriftResampleState*>(user);
    GstBuffer* buf = GST_PAD_PROBE_INFO_BUFFER(info);
    if (!buf || !st->src || !st->clock) {
        return GST_PAD_PROBE_OK;
    }

    // A gap is not drift.  Re-anchor and let the resampler start clean, rather
    // than trying to make up time that was never ours to make up.
    if (GST_BUFFER_FLAG_IS_SET(buf, GST_BUFFER_FLAG_DISCONT)) {
        // Worth logging: this resets the resampler and restarts the rate
        // estimate, and it was happening every few minutes with nothing in the
        // log to say so -- the only visible sign was the loop's own elapsed
        // counter starting over.
        LogInfo(VB_MEDIAOUT,
                "AES67 drift [%d]: source discontinuity, resetting\n",
                st->instanceId);
        src_reset(st->src);
        st->anchored = false;
    }

    GstMapInfo map;
    if (!gst_buffer_map(buf, &map, GST_MAP_READ)) {
        return GST_PAD_PROBE_OK;
    }
    const int ch = st->channels;
    const long inFrames = (long)(map.size / (sizeof(float) * ch));
    if (inFrames <= 0) {
        gst_buffer_unmap(buf, &map);
        return GST_PAD_PROBE_OK;
    }

    const GstClockTime now = gst_clock_get_time(st->clock);
    if (!st->anchored) {
        st->anchored = true;
        st->anchorPts = GST_BUFFER_PTS_IS_VALID(buf) ? GST_BUFFER_PTS(buf) : 0;
        st->ptsFrames = 0;
        st->warmed = false;
        st->ctlClock = now;
        st->ctlIn = 0;
        st->ctlOut = 0;
        st->ratio = 1.0;
    }

    // A PTP clock can step backwards, which happens when ptp4l first locks to
    // a grandmaster -- i.e. only when this device is a follower, never when it
    // is the grandmaster itself.  Without this the elapsed time pins at zero
    // until the clock catches back up to where it was, and for a large step
    // (a TAI/UTC correction is 37 seconds) the loop would sit idle that whole
    // time.  Re-anchoring costs nothing: the learned trim is kept, and only
    // the measurement window restarts.
    if (now < st->ctlClock) {
        LogInfo(VB_MEDIAOUT,
                "AES67 drift [%d]: clock stepped back, re-anchoring\n",
                st->instanceId);
        st->ctlClock = now;
        st->ctlIn = 0;
        st->ctlOut = 0;
        st->warmed = false;
        st->window.clear();
    }

    double elapsed =
        (now > st->ctlClock) ? (double)(now - st->ctlClock) / GST_SECOND : 0.0;

    // Start controlling from a steady state, not from the pipeline's first
    // gasp.  Everything before this is discarded rather than corrected.
    if (!st->warmed && elapsed > 5.0) {
        st->warmed = true;
        st->ctlClock = now;
        st->ctlIn = 0;
        st->ctlOut = 0;
        st->window.clear();
        elapsed = 0.0;
    }

    // A large offset is a gap, not drift, and chasing it is the exact mistake
    // that made audiorate unusable.  Absorb it instead: realign the output
    // timeline to PTP, restart the rate estimate, and keep the trim already
    // learned.  Measured before this was added: a ~200ms startup gap held the
    // trim at its clamp for minutes, audibly pitching the stream while it
    // clawed back time that was never lost to drift.
    constexpr double MAX_OFFSET_S = 0.05;
    bool resync = false;
    double err = st->warmed
                     ? (double)st->ctlOut - (double)AES67::AUDIO_RATE * elapsed
                     : 0.0;
    if (st->warmed && std::fabs(err) > MAX_OFFSET_S * AES67::AUDIO_RATE) {
        LogInfo(VB_MEDIAOUT,
                "AES67 drift [%d]: %+.0f ms gap absorbed, re-anchoring\n",
                st->instanceId, err * 1000.0 / AES67::AUDIO_RATE);
        st->ctlClock = now;
        st->ctlIn = 0;
        st->ctlOut = 0;
        st->window.clear();
        if (GST_BUFFER_PTS_IS_VALID(buf)) {
            st->anchorPts = GST_BUFFER_PTS(buf);
        }
        st->ptsFrames = 0;
        elapsed = 0.0;
        err = 0.0;
        resync = true;
    }

    // Hold the trim already learned rather than snapping back to 1.0 whenever
    // there is not yet enough data.  The rate being corrected is a property of
    // two crystals, so the last good value is always a better guess than "no
    // correction" -- and resetting it is what turned a burst of absorbed gaps
    // into a death spiral: trim collapsed to 0, the pacing queue then drained
    // for want of correction, the drain starved the source into more gaps, and
    // those gaps absorbed away every chance to re-learn the rate.  Measured:
    // 9 minutes of correct operation, then trim pinned at 0.0ppm with the send
    // rate falling from 250/s to 155/s and absorptions climbing past 3500.
    double target = st->ratio;
    double cardRate = 0.0;
    if (st->warmed) {
        constexpr double WINDOW_S = 60.0;
        st->window.push_back({now, st->ctlIn, st->ctlOut});
        while (st->window.size() > 1 &&
               (double)(now - st->window.front().t) / GST_SECOND > WINDOW_S) {
            st->window.pop_front();
        }

        const auto& a = st->window.front();
        const double span = (double)(now - a.t) / GST_SECOND;
        if (span > 5.0) {
            // Feedforward over the window, not since the anchor.  The card's
            // rate against PTP is two crystals, so this is near-constant; the
            // window only keeps the estimate from being anchored to whatever
            // the first few seconds happened to look like.
            cardRate = (double)(st->ctlIn - a.in) / span;
        }
    }

    if (cardRate > 1000.0) {
        const double ff = (double)AES67::AUDIO_RATE / cardRate;

        // Phase feedback: pull the accumulated offset back towards zero with a
        // first-order lag.  The plant is already an integrator (a rate error
        // accumulates into offset), so proportional is the right shape and a
        // second integrator here is what previously rang.  Clamping it well
        // below the feedforward guarantees it can trim but never take over.
        // Gain set by measurement, not by theory.  At 3000 the loop settled
        // with a standing 23ms offset it would not remove; 30000 drives it to
        // +/-0.05ms and holds there, with the trim still sitting on the
        // feedforward value.  Offset then decays with a ~33s time constant.
        constexpr double KP_PPM_PER_SEC = 30000.0;
        // 100ppm is 0.17 cents -- inaudible -- and well under the ~55ppm
        // feedforward, so this can trim but never take over.
        constexpr double MAX_FEEDBACK_PPM = 100.0;
        double fbPpm = -KP_PPM_PER_SEC * err / (double)AES67::AUDIO_RATE;
        fbPpm = std::clamp(fbPpm, -MAX_FEEDBACK_PPM, MAX_FEEDBACK_PPM);

        target = ff * (1.0 + fbPpm * 1e-6);
    }

    // Hard clamp, then slew limit.  300ppm is far beyond any real crystal pair
    // (the reference hardware needs 56ppm), so hitting the clamp means the
    // estimate is wrong and the right response is to refuse to act on it
    // rather than to pitch-shift the audio chasing it.
    constexpr double MAX_TRIM = 0.0003;   // +/-300ppm
    constexpr double MAX_STEP = 0.000002; // 2ppm per buffer
    target = std::clamp(target, 1.0 - MAX_TRIM, 1.0 + MAX_TRIM);
    st->ratio = std::clamp(target, st->ratio - MAX_STEP, st->ratio + MAX_STEP);

    const size_t capacity = (size_t)(inFrames / st->ratio) + 32;
    if (st->out.size() < capacity * ch) {
        st->out.resize(capacity * ch);
    }

    SRC_DATA d;
    memset(&d, 0, sizeof(d));
    d.data_in = reinterpret_cast<const float*>(map.data);
    d.input_frames = inFrames;
    d.data_out = st->out.data();
    d.output_frames = (long)capacity;
    d.src_ratio = st->ratio;
    d.end_of_input = 0;

    const int rc = src_process(st->src, &d);
    gst_buffer_unmap(buf, &map);

    if (rc != 0 || d.output_frames_gen <= 0) {
        if (rc != 0) {
            LogWarn(VB_MEDIAOUT, "AES67 drift [%d]: src_process: %s\n",
                    st->instanceId, src_strerror(rc));
        }
        return GST_PAD_PROBE_OK;
    }
    if (d.input_frames_used < inFrames) {
        // Would mean silently discarding input, so it is counted rather than
        // ignored; the output buffer is sized so this should never happen.
        st->shortReads++;
    }

    const size_t bytes = (size_t)d.output_frames_gen * ch * sizeof(float);
    GstBuffer* out = gst_buffer_new_allocate(nullptr, bytes, nullptr);
    if (!out) {
        return GST_PAD_PROBE_OK;
    }
    gst_buffer_fill(out, 0, st->out.data(), bytes);

    // Timestamp from the output sample count, so the timeline downstream
    // advances at exactly AUDIO_RATE.  Producing AUDIO_RATE samples per PHC
    // second is what the loop above enforces, so media time tracks PTP.
    GST_BUFFER_PTS(out) =
        st->anchorPts + gst_util_uint64_scale(st->ptsFrames, GST_SECOND,
                                              AES67::AUDIO_RATE);
    GST_BUFFER_DURATION(out) = gst_util_uint64_scale(
        (guint64)d.output_frames_gen, GST_SECOND, AES67::AUDIO_RATE);
    if (resync || GST_BUFFER_FLAG_IS_SET(buf, GST_BUFFER_FLAG_DISCONT)) {
        GST_BUFFER_FLAG_SET(out, GST_BUFFER_FLAG_DISCONT);
    }

    st->ptsFrames += (guint64)d.output_frames_gen;
    st->ctlIn += (guint64)d.input_frames_used;
    st->ctlOut += (guint64)d.output_frames_gen;

    if ((++st->buffers % 2000) == 0) {
        LogInfo(VB_MEDIAOUT,
                "AES67 drift [%d]: trim %+.1f ppm, card %.1f/s (%+.1f ppm), offset %+.2f ms over %.0fs%s\n",
                st->instanceId, (st->ratio - 1.0) * 1e6, cardRate,
                cardRate > 1000.0
                    ? (cardRate / (double)AES67::AUDIO_RATE - 1.0) * 1e6
                    : 0.0,
                // err as the controller saw it.  Recomputing here instead
                // reads one buffer high -- a whole graph quantum, 23.2ms on a
                // 1024/44100 graph -- which looked exactly like a stuck offset
                // the loop was failing to correct.
                err * 1000.0 / AES67::AUDIO_RATE,
                elapsed,
                st->shortReads ? " (SHORT READS)" : "");
    }

    gst_buffer_unref(buf);
    GST_PAD_PROBE_INFO_DATA(info) = out;
    return GST_PAD_PROBE_OK;
}
#endif // FPP_HAVE_SAMPLERATE


// Byte counters at both ends of the send pipeline.  See
// AES67Config::pipelineStats.
static int PipeWireGraphRate();

struct PipelineStatsState {
    int instanceId = 0;
    guint64 inBytes = 0;
    guint64 outBytes = 0;
    GstClockTime start = 0;
    GstClockTime lastLog = 0;
    guint64 inAtLast = 0;
    guint64 outAtLast = 0;
};

static void DestroyPipelineStatsState(gpointer data) {
    delete static_cast<PipelineStatsState*>(data);
}

static GstPadProbeReturn PipelineStatsIn(GstPad*, GstPadProbeInfo* info,
                                         gpointer user) {
    auto* st = static_cast<PipelineStatsState*>(user);
    GstBuffer* b = GST_PAD_PROBE_INFO_BUFFER(info);
    if (!b) {
        return GST_PAD_PROBE_OK;
    }
    st->inBytes += gst_buffer_get_size(b);

    const GstClockTime now = gst_util_get_timestamp();
    if (st->start == 0) {
        st->start = now;
        st->lastLog = now;
        return GST_PAD_PROBE_OK;
    }
    if (now - st->lastLog < 30 * GST_SECOND) {
        return GST_PAD_PROBE_OK;
    }
    const double dt = (double)(now - st->lastLog) / GST_SECOND;
    LogInfo(VB_MEDIAOUT,
            "AES67 stats [%d]: pipewiresrc %.0f B/s, udpsink %.0f B/s, "
            "ratio %.3f, totals in %llu out %llu\n",
            st->instanceId, (double)(st->inBytes - st->inAtLast) / dt,
            (double)(st->outBytes - st->outAtLast) / dt,
            (st->inBytes > st->inAtLast)
                ? (double)(st->outBytes - st->outAtLast) /
                      (double)(st->inBytes - st->inAtLast)
                : 0.0,
            (unsigned long long)st->inBytes, (unsigned long long)st->outBytes);
    st->lastLog = now;
    st->inAtLast = st->inBytes;
    st->outAtLast = st->outBytes;
    return GST_PAD_PROBE_OK;
}

static GstPadProbeReturn PipelineStatsOut(GstPad*, GstPadProbeInfo* info,
                                          gpointer user) {
    auto* st = static_cast<PipelineStatsState*>(user);
    GstBuffer* b = GST_PAD_PROBE_INFO_BUFFER(info);
    if (b) {
        st->outBytes += gst_buffer_get_size(b);
    }
    return GST_PAD_PROBE_OK;
}

bool AES67Manager::CreateSendPipeline(const AES67Instance& inst) {
    std::string nodeName = SafeNodeName(inst.name) + "_send";
    std::string sourceIP = GetInterfaceIP(inst.interface);

    // Calculate ptime in nanoseconds for rtpL24pay
    int64_t ptimeNs = (int64_t)inst.ptime * 1000000LL;

    // Build pipeline string
    // pipewiresrc registers itself as a PipeWire node with the expected name
    // so that the audio group's filter-chain output (which has
    // node.target=<nodeName>) can connect to it.  We do NOT use
    // target-object because this node is the DESTINATION, not the source.
    //
    // Pipeline:
    //   pipewiresrc stream-properties="props,node.name=<node>"
    //   ! audioconvert ! audioresample ! audioconvert
    //   ! audio/x-raw,format=S24BE,rate=48000,channels=N
    //   ! rtpL24pay pt=96 min-ptime=<ns> max-ptime=<ns>
    //   ! application/x-rtp,clock-rate=48000
    //   ! udpsink host=<multicast> port=<port> multicast-iface=<iface> ttl-mc=4 auto-multicast=true sync=true
    //
    // The resampler is not optional.  AES67 mandates 48 kHz on the wire, and the
    // PipeWire graph no longer runs there: since the graph clock started
    // following the output card it sits at whatever that card is configured for,
    // 44100 by default.  pipewiresrc cannot satisfy a caps filter that asks for
    // a different rate AND a format PipeWire does not carry natively (S24BE, the
    // big-endian packing L24 needs) -- it does not fail, it simply never
    // negotiates, so set_state() blocks and eventually returns FAILURE, which is
    // the "AES67: audio send stream failed to start" warning.
    //
    // Measured against the live 44100 graph:
    //   audioconvert ! S24BE@48000                            hangs
    //   audioconvert ! S24BE@44100                             works (no rate change)
    //   audioresample ! audioconvert ! S24BE@48000            hangs
    //   audioconvert ! audioresample ! audioconvert ! S24BE@48000   works
    //
    // So the conversion has to be split: reach a format the resampler is happy
    // to work in, resample, and only then pack to S24BE.  This costs nothing
    // when the graph already runs at 48000, where the resampler passes through.

    std::ostringstream oss;
    // Media clock -- see GetOrCreateMediaClock().  Everything below that makes
    // the RTP timestamps mean something depends on having it.
    GstClock* ptpClock = m_config.ptpMediaClock ? GetOrCreateMediaClock() : nullptr;
    // Correcting the rate only means anything when the timeline it is being
    // corrected against is PTP.
    const bool driftControl = (ptpClock != nullptr) && m_config.adaptiveResample;
    const bool sinkPacing = m_config.sinkPacing;
    const bool rateMatch = m_config.rateMatch;
#ifdef FPP_HAVE_SAMPLERATE
    const bool driftResample = m_config.driftResample && (ptpClock != nullptr);
#else
    const bool driftResample = false;
    if (m_config.driftResample) {
        LogWarn(VB_MEDIAOUT,
                "AES67 send [%d]: driftResample requested but this build has "
                "no libsamplerate\n", inst.id);
    }
#endif
    if (!m_config.ptpMediaClock) {
        LogInfo(VB_MEDIAOUT, "AES67 send [%d]: PTP media clock disabled by config\n", inst.id);
    }

    // Ask PipeWire for the graph's own rate.  Without this the AES67 rate
    // propagates all the way up and PipeWire resamples for this node alone,
    // which is what degrades after ~11 minutes.  See
    // AES67Config::nativeSourceRate.  audioresample below then does the
    // conversion, which is what it is there for.
    const int graphRate = m_config.nativeSourceRate ? PipeWireGraphRate() : 0;
    if (graphRate > 0 && graphRate != AES67::AUDIO_RATE) {
        LogInfo(VB_MEDIAOUT,
                "AES67 send [%d]: taking source at graph rate %d, converting "
                "to %d in the pipeline\n",
                inst.id, graphRate, AES67::AUDIO_RATE);
    }

    oss << "pipewiresrc name=pwsrc"
        << " min-buffers=" << m_config.sourceMinBuffers
        << " always-copy=" << (m_config.sourceBufferCopy ? "true" : "false")
        << " "
        << ((graphRate > 0 && graphRate != AES67::AUDIO_RATE)
                ? ("! audio/x-raw,rate=" + std::to_string(graphRate) + " ")
                : "")
        << "! audioconvert "
        << "! audioresample "
        << "! audioconvert "
        // Rate trim for the drift control loop.  Sits in the float domain
        // because that is all this element accepts, and ahead of the S24BE
        // conversion and the packet split so both still see a clean stream.
        // Pin the format AND channel count across the rate trim.  "speed"
        // advertises channels [1, MAX] and nothing downstream forces a count
        // until the S24BE capsfilter, so negotiation happily settles on mono
        // and audioconvert quietly upmixes back to stereo afterwards: correct
        // packet sizes, correct timestamps, valid SDP, and a mono stream.
        // Nothing at the packet level catches it -- it showed up as a mono
        // feed in the PipeWire graph.
        << (driftControl
                ? ("! audio/x-raw,format=F32LE,channels=" + std::to_string(inst.channels) +
                   " ! pitch name=drift "
                   "! audio/x-raw,format=F32LE,channels=" + std::to_string(inst.channels) + " ")
                : "")
        // Tap point for drift correction.  identity does nothing itself; the
        // resampling happens in a pad probe on its src pad, in the float
        // domain and before the audio is cut into packets, so every packet
        // downstream is still exactly one ptime long.
        << (driftResample
                ? ("! audio/x-raw,format=F32LE,rate=" +
                   std::to_string(AES67::AUDIO_RATE) + ",channels=" +
                   std::to_string(inst.channels) +
                   " ! identity name=driftpoint ")
                : "")
        << "! audioconvert "
        << "! audio/x-raw,format=S24BE,rate=" << AES67::AUDIO_RATE
        << ",channels=" << inst.channels << " "
        // Re-block the audio into exactly one packet per buffer, on a timeline
        // aligned to sample boundaries.  pipewiresrc hands us whatever the graph
        // quantum produced, with timestamps that do not land on packet
        // boundaries -- and the payloader then has to choose between following
        // those timestamps (RTP increments wobble +/-1 sample, audibly
        // distorted) or counting samples itself (increments exact, but the
        // timeline drifts away from PTP on every dropped buffer, measured at
        // -180ms).  Splitting first removes the choice: buffers are exactly
        // ptime long and correctly timestamped, so the payloader can follow the
        // running time (= PTP time) and still step exactly one packet each time.
        // Reconcile the card's sample count with PTP time before the audio is
        // cut into packets, so the payloader sees a timeline that already
        // advances at the PTP rate.  See AES67Config::rateMatch.
        << (rateMatch ? "! audiorate name=ratematch " : "")
        // Also required by sinkPacing, and for a second reason: the payloader
        // stamps every packet it emits from one input buffer with that
        // buffer's timestamp, so a whole quantum's worth of packets come out
        // sharing a PTS.  udpsink then has nothing to pace against and sends
        // them together -- sync=true on its own leaves the burst untouched
        // (measured: 5.8 packets per 23.22ms, identical to sync=false).
        // Splitting first gives each packet its own send time.
        << ((ptpClock || sinkPacing)
                ? ("! audiobuffersplit output-buffer-duration=" +
                   std::to_string(inst.ptime) + "/1000 ") : "")
        << "! rtpL24pay name=pay pt=" << AES67::RTP_PAYLOAD_TYPE
        << " min-ptime=" << ptimeNs
        << " max-ptime=" << ptimeNs
        // timestamp-offset=0 anchors the RTP timeline to PTP (see the media
        // clock note below).  perfect-rtptime stays TRUE -- deriving each
        // packet's timestamp from the buffer running time instead made the
        // increments wobble by +/-1 sample (191/193/194 rather than a clean
        // 192), and a receiver that places samples by RTP timestamp has to
        // absorb that wobble on every single packet.  With it true the
        // payloader counts samples, so the timeline is anchored to PTP at
        // start and then advances exactly one packet at a time.
        // perfect-rtptime=false: follow the (now exactly aligned) running time
        // so the RTP timeline stays anchored to PTP instead of free-running
        // off a sample counter.  See the audiobuffersplit note above.
        << (ptpClock ? " timestamp-offset=0 perfect-rtptime=false" : "")
        << " "
        << "! application/x-rtp,clock-rate=" << AES67::AUDIO_RATE << " "
        // A queue here is what makes sink pacing safe: it runs the sink on its
        // own thread, so the sink blocking until a packet's send time cannot
        // push back into the live pipewiresrc.  Non-leaky on purpose -- in
        // steady state it drains exactly as fast as it fills, and dropping a
        // packet to keep up would defeat the point.  See sinkPacing.
        // Bounded by time only.  A buffer-count cap is a ptime trap: 64 buffers
        // is 256ms at 4ms ptime but only 64ms at 1ms, and the queue has to hold
        // the sink latency plus a whole graph quantum's burst -- about 61
        // buffers at 1ms.  That put the cap right on top of normal steady
        // state, the queue ran permanently full, and the backpressure silenced
        // the stream entirely on a Yamaha MRX7-D at 1ms ptime (reported on
        // #2848: "queue 64 buffers / 64ms" repeating, with no audio).
        << (sinkPacing ? "! queue name=sinkq max-size-bytes=0 "
                         "max-size-buffers=0 max-size-time=200000000 " : "")
        << "! udpsink name=usink host=" << inst.multicastIP
        << " port=" << inst.port
        << " ttl-mc=" << AES67::AUDIO_RTP_TTL
        << " qos-dscp=" << AES67::AUDIO_DSCP
        << " auto-multicast=true sync=" << (sinkPacing ? "true" : "false");

    if (!inst.interface.empty()) {
        oss << " multicast-iface=" << inst.interface;
    }

    std::string pipelineStr = oss.str();
    LogInfo(VB_MEDIAOUT, "AES67 send pipeline [%d] %s: %s\n",
            inst.id, inst.name.c_str(), pipelineStr.c_str());

    GError* error = nullptr;
    GstElement* pipeline = gst_parse_launch(pipelineStr.c_str(), &error);
    if (error) {
        LogErr(VB_MEDIAOUT, "AES67 send pipeline error [%d]: %s\n",
               inst.id, error->message);
        g_error_free(error);
        // gst_parse_launch() can return a non-null (partial) pipeline even
        // when it also reports an error -- release it so we don't leak.
        if (pipeline) {
            gst_object_unref(pipeline);
        }
        return false;
    }

    // Set stream-properties post-launch — inline GstStructure values in
    // gst_parse_launch can crash gst_value_deserialize on some platforms.
    GstElement* pwsrc = gst_bin_get_by_name(GST_BIN(pipeline), "pwsrc");
    if (pwsrc) {
        // Ask PipeWire for a quantum no larger than one RTP packet.
        //
        // This is what makes the stream evenly paced.  At the stock 1024-sample
        // quantum pipewiresrc hands us ~21ms of audio at once and the packets
        // for it leave back-to-back in microseconds, then nothing for 21ms --
        // which is far outside the receive window of a Dante/AES67 receiver.
        //
        // Pacing it at the sink instead (udpsink sync=true) does not work here:
        // buffers arrive with a PTS already one quantum in the past, so the
        // sink blocks, backpressure reaches the live pipewiresrc, and it drops
        // audio rather than stalling.  Measured during playback that cost ~36%
        // of the stream.  Fixing the cadence at the source has no such failure
        // mode -- and it cuts sender latency, which the receiver has to absorb
        // as link offset.
        //
        // PipeWire clamps this to its min-quantum, and the graph quantum is the
        // smallest any node asks for, so this does raise CPU for the whole
        // graph.  That is the trade for a stream that is actually usable.
        std::string nodeLatency = std::to_string(inst.ptime * AES67::AUDIO_RATE / 1000) +
                                  "/" + std::to_string(AES67::AUDIO_RATE);
        GstStructure* props = gst_structure_new("props",
            "node.name", G_TYPE_STRING, nodeName.c_str(),
            "node.autoconnect", G_TYPE_BOOLEAN, FALSE,
            NULL);
        if (m_config.sourcePacing) {
            gst_structure_set(props, "node.latency", G_TYPE_STRING, nodeLatency.c_str(), NULL);
        } else {
            LogInfo(VB_MEDIAOUT, "AES67 send [%d]: source pacing disabled by config\n", inst.id);
        }
        g_object_set(pwsrc, "stream-properties", props, NULL);
        gst_structure_free(props);
        gst_object_unref(pwsrc);
    }

    // Put the pipeline on PTP time, and line the RTP timeline up with it.
    //
    // "a=mediaclk:direct=0" in our SDP asserts that the RTP timestamp IS the
    // media clock count on the reference clock, with zero offset.  Three things
    // have to be true together for that to hold:
    //
    //   1. the pipeline clock is PTP time            (use_clock below)
    //   2. base time is zero, so a buffer's running time IS absolute PTP time
    //      rather than time-since-this-pipeline-started
    //   3. the payloader adds no offset of its own    (timestamp-offset=0)
    //
    // rtpL24pay computes RTP ts = timestamp-offset + running_time * 48000 / 1e9,
    // so with (2) and (3) that is exactly PTP nanoseconds scaled to 48 kHz
    // samples and wrapped at 2^32 -- which is what a receiver reconstructs from
    // its own PTP time.  Previously the offset was random and the running time
    // was relative to pipeline start, so the mapping was wrong by an arbitrary
    // amount up to 2^32 samples (~24 hours) and no conformant receiver could
    // place the audio.
    //
    // Setting the start time to NONE stops GStreamer recalculating base time on
    // every PAUSED->PLAYING.  That is what keeps one continuous media timeline
    // across track changes and flushes: RTP timestamps stay tied to wall-clock
    // PTP instead of jumping whenever the pipeline is disturbed, which is how a
    // Dante/AES67 receiver expects a transmitter to behave.
    if (ptpClock) {
        gst_pipeline_use_clock(GST_PIPELINE(pipeline), ptpClock);
        gst_element_set_start_time(pipeline, GST_CLOCK_TIME_NONE);
        gst_element_set_base_time(pipeline, 0);
    }

    if (m_config.pipelineStats) {
        GstElement* src = gst_bin_get_by_name(GST_BIN(pipeline), "pwsrc");
        GstElement* snk = gst_bin_get_by_name(GST_BIN(pipeline), "usink");
        GstPad* sp = src ? gst_element_get_static_pad(src, "src") : nullptr;
        GstPad* kp = snk ? gst_element_get_static_pad(snk, "sink") : nullptr;
        if (sp && kp) {
            auto* ps = new PipelineStatsState();
            ps->instanceId = inst.id;
            gst_pad_add_probe(kp, GST_PAD_PROBE_TYPE_BUFFER, PipelineStatsOut,
                              ps, nullptr);
            gst_pad_add_probe(sp, GST_PAD_PROBE_TYPE_BUFFER, PipelineStatsIn,
                              ps, DestroyPipelineStatsState);
            LogInfo(VB_MEDIAOUT, "AES67 send [%d]: pipeline stats on\n",
                    inst.id);
        }
        if (sp) gst_object_unref(sp);
        if (kp) gst_object_unref(kp);
        if (src) gst_object_unref(src);
        if (snk) gst_object_unref(snk);
    }

#ifdef FPP_HAVE_SAMPLERATE
    if (driftResample) {
        GstElement* dp = gst_bin_get_by_name(GST_BIN(pipeline), "driftpoint");
        GstPad* dpad = dp ? gst_element_get_static_pad(dp, "src") : nullptr;
        int err = 0;
        auto* st = new DriftResampleState();
        st->channels = inst.channels;
        st->instanceId = inst.id;
        st->clock = GST_CLOCK(gst_object_ref(ptpClock));
        // SINC_FASTEST is bandlimited and far above what a 56ppm correction
        // needs; the cost is a few percent of one core on a Pi.
        st->src = src_new(SRC_SINC_FASTEST, inst.channels, &err);

        if (dpad && st->src) {
            gst_pad_add_probe(dpad, GST_PAD_PROBE_TYPE_BUFFER,
                              DriftResampleProbe, st,
                              DestroyDriftResampleState);
            LogInfo(VB_MEDIAOUT,
                    "AES67 send [%d]: drift correction on (libsamplerate)\n",
                    inst.id);
        } else {
            LogWarn(VB_MEDIAOUT,
                    "AES67 send [%d]: drift correction unavailable (%s)\n",
                    inst.id, st->src ? "no driftpoint pad" : src_strerror(err));
            DestroyDriftResampleState(st);
        }
        if (dpad) {
            gst_object_unref(dpad);
        }
        if (dp) {
            gst_object_unref(dp);
        }
    }
#endif

    if (rateMatch) {
        GstElement* rm = gst_bin_get_by_name(GST_BIN(pipeline), "ratematch");
        if (rm) {
            // skip-to-first is mandatory here, not a tuning choice.  With the
            // PTP media clock the base time is 0, so a buffer's timestamp is
            // absolute PTP nanoseconds -- roughly 1.8e18 at present.  Without
            // this audiorate treats that as a gap starting at zero and fills
            // it with silence: measured "in 1081, added 343632000" within a
            // minute, i.e. it was manufacturing hours of silence.
            g_object_set(rm,
                         "tolerance", m_config.rateMatchToleranceNs,
                         "skip-to-first", TRUE,
                         NULL);
            gst_object_unref(rm);
            LogInfo(VB_MEDIAOUT,
                    "AES67 send [%d]: rate matching on, tolerance %lluns\n",
                    inst.id,
                    (unsigned long long)m_config.rateMatchToleranceNs);
        }
    }

    if (sinkPacing) {
        // Hold each packet back so it is still in the future when the sink
        // gets it.  Without the delay every buffer arrives past its running
        // time, udpsink renders each one immediately, and the burst is
        // unchanged -- pacing that never waits is not pacing.
        //
        // This has to be ts-offset on the sink rather than
        // gst_pipeline_set_latency(): the pipeline latency is recomputed from
        // a LATENCY query when the pipeline goes to PLAYING, so a value set
        // beforehand is replaced by the automatic one.  That is what made an
        // earlier attempt pace for the first few seconds and then burst for
        // the rest of the run.  ts-offset is applied per-buffer and survives.
        // Shift the buffer timestamps forward rather than offsetting the sink.
        //
        // ts-offset delays only the transmission; the RTP timestamps stay
        // where they were, so every packet goes out after the playout deadline
        // it declares and a conformant receiver drops the lot -- measured at
        // -23.7ms with pacing on against +7.8ms without it, and reported on
        // #2848 as a Yamaha MRX7-D sitting subscribed, green and silent on a
        // stream carrying perfectly paced, valid stereo L24.
        //
        // Moving the PTS instead carries both with it: the payloader derives
        // the RTP timestamp from the shifted PTS, and the sink renders at that
        // same shifted time.  The lead over transmission therefore stays
        // whatever it was without pacing, whatever latency we choose, instead
        // of needing a correction factor fitted to one machine.
        GstElement* pay = gst_bin_get_by_name(GST_BIN(pipeline), "pay");
        GstPad* payPad = pay ? gst_element_get_static_pad(pay, "sink") : nullptr;
        if (payPad) {
            gst_pad_add_probe(
                payPad, GST_PAD_PROBE_TYPE_BUFFER,
                [](GstPad*, GstPadProbeInfo* info, gpointer user) {
                    GstBuffer* b = GST_PAD_PROBE_INFO_BUFFER(info);
                    if (!b) {
                        return GST_PAD_PROBE_OK;
                    }
                    const GstClockTime shift = (GstClockTime)(guintptr)user;
                    b = gst_buffer_make_writable(b);
                    if (GST_BUFFER_PTS_IS_VALID(b)) {
                        GST_BUFFER_PTS(b) += shift;
                    }
                    if (GST_BUFFER_DTS_IS_VALID(b)) {
                        GST_BUFFER_DTS(b) += shift;
                    }
                    GST_PAD_PROBE_INFO_DATA(info) = b;
                    return GST_PAD_PROBE_OK;
                },
                (gpointer)(guintptr)((GstClockTime)m_config.sinkPacingMs *
                                     GST_MSECOND),
                nullptr);
            gst_object_unref(payPad);

            // Cancel the shift again at the sink.  The payloader has already
            // taken its RTP timestamp from the shifted PTS, so undoing it here
            // moves only the transmission time back, leaving the timestamp
            // ahead of the wire by the shift.
            //
            // This is what the earlier attempts each got wrong.  ts-offset
            // alone moved transmission but not the timestamp; shifting the PTS
            // alone moved both, so the lead did not change at all (-23.73ms
            // before, -23.73ms after -- identical, which is what gave it
            // away).  The -23.2ms is GStreamer's own latency compensation:
            // sinks render at PTS + pipeline latency, and the pipeline latency
            // here is one graph quantum.
            GstElement* usink2 = gst_bin_get_by_name(GST_BIN(pipeline), "usink");
            if (usink2) {
                g_object_set(usink2, "ts-offset",
                             -((gint64)m_config.sinkPacingMs * GST_MSECOND),
                             NULL);
                gst_object_unref(usink2);
            }
            LogInfo(VB_MEDIAOUT,
                    "AES67 send [%d]: sink pacing on, %dms timestamp lead\n",
                    inst.id, m_config.sinkPacingMs);
        } else {
            LogWarn(VB_MEDIAOUT,
                    "AES67 send [%d]: sink pacing on but payloader not found\n",
                    inst.id);
        }
        if (pay) {
            gst_object_unref(pay);
        }
    }

    // Store the bus for polling in the watchdog thread.
    // gst_bus_add_watch() requires a running GLib main loop which fppd does
    // not have — use gst_bus_pop() polling instead.
    GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));

    // Start pipeline in PLAYING state OUTSIDE the mutex — this can block
    // waiting for PipeWire and must not hold m_pipelineMutex.
    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        LogErr(VB_MEDIAOUT, "AES67 send pipeline [%d] failed to start\n", inst.id);
        WarningHolder::AddWarning(44, "AES67: audio send stream failed to start");
        // Drop back to NULL before unreffing -- elements may already hold
        // READY/PAUSED resources (sockets, threads, PipeWire connections)
        // that gst_object_unref() alone will not release.
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(bus);
        gst_object_unref(pipeline);
        return false;
    }

    {
        std::unique_lock<std::mutex> lock(m_pipelineMutex);
        // If a pipeline already exists for this instance id (e.g. two
        // ApplyConfig() runs interleaved), stop it properly instead of
        // erasing the map entry directly -- a raw erase() would destroy
        // the AES67Pipeline (dropping its GstElement* refs) without ever
        // calling gst_element_set_state(NULL), leaking a still-PLAYING
        // pipeline (refs, threads, sockets).
        auto node = m_sendPipelines.extract(inst.id);
        if (!node.empty()) {
            // StopPipeline() can block on a GStreamer state change to
            // PipeWire -- release the lock while it runs, matching the
            // no-block-while-locked policy used elsewhere in this file
            // (see StopAllPipelines()).
            lock.unlock();
            StopPipeline(node.mapped());
            lock.lock();
        }
        auto [it, ok] = m_sendPipelines.try_emplace(inst.id);
        it->second.instanceId = inst.id;
        it->second.channels = inst.channels;
        it->second.isSend = true;
        it->second.pipeline = pipeline;
        it->second.bus = bus;
        it->second.running = true;
    }

    LogInfo(VB_MEDIAOUT, "AES67 send pipeline [%d] %s started → %s:%d (%dch, %dms ptime)\n",
            inst.id, inst.name.c_str(), inst.multicastIP.c_str(), inst.port,
            inst.channels, inst.ptime);
    return true;
}

// ──────────────────────────────────────────────────────────────────────────────
// Pipeline creation — Receive
// ──────────────────────────────────────────────────────────────────────────────
bool AES67Manager::CreateRecvPipeline(const AES67Instance& inst) {
    std::string nodeName = SafeNodeName(inst.name) + "_recv";

    // Build pipeline:
    //   udpsrc multicast-group=<ip> port=<port> auto-multicast=true
    //   ! application/x-rtp,media=audio,clock-rate=48000,encoding-name=L24,channels=N,payload=96
    //   ! rtpjitterbuffer latency=<ms>
    //   ! rtpL24depay
    //   ! audioconvert
    //   ! pipewiresink target-object=<node>
    //     stream-properties="props,media.class=Audio/Source,node.name=<node>"

    std::ostringstream oss;
    oss << "udpsrc multicast-group=" << inst.multicastIP
        << " port=" << inst.port
        << " auto-multicast=true";

    if (!inst.interface.empty()) {
        oss << " multicast-iface=" << inst.interface;
    }

    oss << " ! application/x-rtp,media=audio,clock-rate=" << AES67::AUDIO_RATE
        << ",encoding-name=L24,channels=" << inst.channels
        << ",payload=" << AES67::RTP_PAYLOAD_TYPE << " "
        << "! rtpjitterbuffer latency=" << inst.latency << " "
        << "! rtpL24depay "
        << "! audioconvert "
        << "! pipewiresink name=pwsink";

    std::string pipelineStr = oss.str();
    LogInfo(VB_MEDIAOUT, "AES67 recv pipeline [%d] %s: %s\n",
            inst.id, inst.name.c_str(), pipelineStr.c_str());

    GError* error = nullptr;
    GstElement* pipeline = gst_parse_launch(pipelineStr.c_str(), &error);
    if (error) {
        LogErr(VB_MEDIAOUT, "AES67 recv pipeline error [%d]: %s\n",
               inst.id, error->message);
        g_error_free(error);
        // gst_parse_launch() can return a non-null (partial) pipeline even
        // when it also reports an error -- release it so we don't leak.
        if (pipeline) {
            gst_object_unref(pipeline);
        }
        return false;
    }

    // Set stream-properties post-launch — gst_parse_launch cannot
    // deserialize GstStructure values containing '/' (e.g. Audio/Source)
    // which crashes gst_value_deserialize.
    std::string recvNodeDesc = SafeNodeName(inst.name) + "_recv";
    GstElement* pwsink = gst_bin_get_by_name(GST_BIN(pipeline), "pwsink");
    if (pwsink) {
        GstStructure* props = gst_structure_new("props",
            "media.class", G_TYPE_STRING, "Audio/Source",
            "node.name", G_TYPE_STRING, nodeName.c_str(),
            "node.description", G_TYPE_STRING, recvNodeDesc.c_str(),
            NULL);
        g_object_set(pwsink, "stream-properties", props, NULL);
        gst_structure_free(props);
        gst_object_unref(pwsink);
    }

    // Store the bus for polling in the watchdog thread (no GLib main loop).
    GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));

    // Note: the receive path runs on GStreamer's default clock.  Only the send
    // path is driven by PTP time (see CreateSendPipeline).

    // Start pipeline OUTSIDE the mutex — GStreamer state changes can block
    // waiting for PipeWire.
    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        LogErr(VB_MEDIAOUT, "AES67 recv pipeline [%d] failed to start\n", inst.id);
        WarningHolder::AddWarning(44, "AES67: audio receive stream failed to start");
        // Drop back to NULL before unreffing -- elements may already hold
        // READY/PAUSED resources (sockets, threads, PipeWire connections)
        // that gst_object_unref() alone will not release.
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(bus);
        gst_object_unref(pipeline);
        return false;
    }

    {
        std::unique_lock<std::mutex> lock(m_pipelineMutex);
        // See CreateSendPipeline() for why we stop any existing pipeline
        // for this id instead of erasing the map entry directly.
        auto node = m_recvPipelines.extract(inst.id);
        if (!node.empty()) {
            lock.unlock();
            StopPipeline(node.mapped());
            lock.lock();
        }
        auto [it, ok] = m_recvPipelines.try_emplace(inst.id);
        it->second.instanceId = inst.id;
        it->second.channels = inst.channels;
        it->second.isSend = false;
        it->second.pipeline = pipeline;
        it->second.bus = bus;
        it->second.running = true;
    }

    LogInfo(VB_MEDIAOUT, "AES67 recv pipeline [%d] %s started ← %s:%d (%dch, %dms latency)\n",
            inst.id, inst.name.c_str(), inst.multicastIP.c_str(), inst.port,
            inst.channels, inst.latency);
    return true;
}

// ──────────────────────────────────────────────────────────────────────────────
// Pipeline management
// ──────────────────────────────────────────────────────────────────────────────
void AES67Manager::StopPipeline(AES67Pipeline& p) {
    if (p.pipeline) {
        gst_element_set_state(p.pipeline, GST_STATE_NULL);

        // Remove the buffer-drop probe installed by FlushSendPipelines()
        // (if any) and release our ref on the pad it was installed on.
        // Without this, the probe stays registered on the pad with
        // &p.dropCounter as userdata -- a dangling pointer once this
        // AES67Pipeline is erased from its owning map -- and the pad ref
        // taken in FlushSendPipelines() leaks.
        if (p.probeId != 0 && p.probePad) {
            gst_pad_remove_probe(p.probePad, p.probeId);
            p.probeId = 0;
        }
        if (p.probePad) {
            gst_object_unref(p.probePad);
            p.probePad = nullptr;
        }

        // Release the bus ref we stored at pipeline creation
        if (p.bus) {
            gst_object_unref(p.bus);
            p.bus = nullptr;
        }
        gst_object_unref(p.pipeline);
        p.pipeline = nullptr;
        p.running = false;
    }
}

void AES67Manager::StopAllPipelines() {
    // Extract pipelines from the maps under the lock, then release the lock
    // before calling gst_element_set_state(NULL) — state changes can block
    // on PipeWire and must not hold m_pipelineMutex.
    std::map<int, AES67Pipeline> sendCopy, recvCopy;
    {
        std::lock_guard<std::mutex> lock(m_pipelineMutex);
        sendCopy.swap(m_sendPipelines);
        recvCopy.swap(m_recvPipelines);
    }

    for (auto& [id, p] : sendCopy) {
        LogDebug(VB_MEDIAOUT, "AES67Manager: Stopping send pipeline [%d]\n", id);
        StopPipeline(p);
    }

    for (auto& [id, p] : recvCopy) {
        LogDebug(VB_MEDIAOUT, "AES67Manager: Stopping recv pipeline [%d]\n", id);
        StopPipeline(p);
    }
}

void AES67Manager::PauseSendPipelines() {
    // No-op: with node.autoconnect=false the send pipeline always outputs
    // to multicast.  When no media is playing, the filter-chain delivers
    // clean silence — no muting needed.
}

void AES67Manager::ResumeSendPipelines() {
    // No-op: pipeline is always sending to multicast.
}

// Pad probe callback: drops buffers while dropCounter > 0, passes through otherwise.
// Installed once on pipewiresrc's src pad and stays active for the pipeline's lifetime.
static GstPadProbeReturn DropBufferProbe(GstPad* pad, GstPadProbeInfo* info, gpointer userData) {
    std::atomic<int>* counter = static_cast<std::atomic<int>*>(userData);
    if (!(info->type & GST_PAD_PROBE_TYPE_BUFFER))
        return GST_PAD_PROBE_OK;
    if (*counter <= 0)
        return GST_PAD_PROBE_OK;
    (*counter)--;
    return GST_PAD_PROBE_DROP;
}

void AES67Manager::FlushSendPipelines() {
    std::lock_guard<std::mutex> lock(m_pipelineMutex);
    for (auto& kv : m_sendPipelines) {
        AES67Pipeline& p = kv.second;
        if (!p.pipeline || !p.running)
            continue;

        // Drop the next ~10 buffers (~53ms at 256-sample quantum / 48kHz)
        // from pipewiresrc's src pad.  This discards any stale audio that
        // was queued in GStreamer elements between the old track stopping
        // and the new one starting, without disrupting the pipeline's
        // event flow (no flush-start/stop, no state change).
        constexpr int DROP_COUNT = 10;
        LogInfo(VB_MEDIAOUT, "AES67 send pipeline [%d]: dropping next %d buffers\n",
                p.instanceId, DROP_COUNT);

        p.dropCounter = DROP_COUNT;

        // Install the probe once; subsequent calls just reset the counter.
        if (p.probeId != 0)
            continue;

        // Find pipewiresrc's src pad and install the permanent probe
        GstIterator* it = gst_bin_iterate_sources(GST_BIN(p.pipeline));
        GValue val = G_VALUE_INIT;
        GstElement* srcElem = nullptr;
        if (gst_iterator_next(it, &val) == GST_ITERATOR_OK) {
            srcElem = GST_ELEMENT(g_value_get_object(&val));
            gst_object_ref(srcElem);
            g_value_unset(&val);
        }
        gst_iterator_free(it);

        if (srcElem) {
            GstPad* srcpad = gst_element_get_static_pad(srcElem, "src");
            if (srcpad) {
                p.probePad = srcpad;   // takes ownership of ref
                p.probeId = gst_pad_add_probe(
                    srcpad,
                    GST_PAD_PROBE_TYPE_BUFFER,
                    DropBufferProbe,
                    &p.dropCounter,
                    nullptr);
            }
            gst_object_unref(srcElem);
        }
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// GStreamer bus callback
// ──────────────────────────────────────────────────────────────────────────────
// ──────────────────────────────────────────────────────────────────────────────
// Pipeline watchdog — polls bus messages and recovers crashed pipelines.
// Called from the SAP announcer thread (every SAP_ANNOUNCE_INTERVAL_S seconds).
// fppd does not run a GLib main loop, so gst_bus_add_watch() callbacks would
// never fire.  We poll the bus manually and promote any ERROR/WARNING messages.
// ──────────────────────────────────────────────────────────────────────────────
// Restart ptp4l/phc2sys if they have died.  Nothing else supervised them: a
// link flap, an OOM kill or a stray `killall ptp4l` left FPP silently running
// with no clock discipline at all, reporting "not running" forever.
//
// Called from the SAP announcer alongside the pipeline watchdog, so it shares
// that thread's cadence.  Note this means PTP is only supervised while the SAP
// announcer runs (i.e. there is at least one SAP-enabled send instance).

// Trims the send stream's sample rate so the media timeline advances at exactly
// PTP rate.  See AES67Config::adaptiveResample for why this is not optional.
//
// The loop is deliberately slow and gentle.  What it is correcting is a crystal
// offset -- constant, order 100ppm -- not a fast disturbance, so it samples
// every couple of seconds and moves in small steps.  Two terms:
//
//   rate    the media clock's measured advance per unit of PTP time.  Feeding
//           that straight back (speed *= measured ratio) nulls the offset in
//           rate, which is the bulk of the correction.
//   offset  a slow pull that burns off however much lag accumulated before the
//           rate term settled, so the stream ends up at the right rate AND back
//           at the latency it started from.
void AES67Manager::DriftControlLoop() {
    LogInfo(VB_MEDIAOUT, "AES67 drift control thread started\n");

    constexpr int SAMPLE_INTERVAL_S = 2;
    // The payloader's last-emitted RTP timestamp only advances when a burst of
    // packets goes out -- one graph quantum at a time -- so sampling it
    // asynchronously quantises the reading by ~21ms.  Over a 30s endpoint
    // measurement that is +/-750ppm of noise on a signal of ~100ppm, and the
    // loop simply hunts.  Fit a slope across the whole window instead, and make
    // the window long: noise falls with both the span and the sample count.
    constexpr int WINDOW_S = 120;          // measurement baseline
    // Actuate far more slowly than we sample.  The measurement is a 120s
    // window, so a correction takes ~60s to show up in it; correcting every 2s
    // means ~30 further corrections are applied before the first is visible,
    // and the loop oscillates (measured: trim swinging +140 to -377ppm, drift
    // swinging +236 to -99ppm).  Classic dead-time instability -- the cure is
    // to make the control interval comparable to the lag, not to lower the
    // gain until the ringing is slow.
    constexpr int ACTUATE_EVERY = 15;      // iterations, i.e. every 30s
    constexpr double KP_RATE = 0.3;        // fraction of the rate error to take per step
    constexpr double KP_OFFSET = 0.00002;  // per second of accumulated lag
    constexpr double MAX_TRIM = 0.0005;    // +/-500ppm, well beyond any crystal
    constexpr double MAX_STEP = 0.00002;   // +/-20ppm per iteration

    struct Sample {
        GstClockTime clock;
        double samples;    // cumulative RTP samples emitted
    };
    struct PipelineState {
        std::deque<Sample> window;
        double cumulative = 0.0;
        guint lastTs = 0;
        GstClockTime lastClock = 0;
        bool primed = false;
        double speed = 1.0;
        int sinceActuate = 0;
    };
    std::map<int, PipelineState> state;

    while (m_driftRunning.load()) {
        for (int i = 0; i < SAMPLE_INTERVAL_S * 10 && m_driftRunning.load(); i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (!m_driftRunning.load()) {
            break;
        }

        std::unique_lock<std::mutex> lock(m_pipelineMutex, std::try_to_lock);
        if (!lock.owns_lock()) {
            continue;
        }

        for (auto& [id, p] : m_sendPipelines) {
            if (!p.running || !p.pipeline) {
                continue;
            }
            GstElement* drift = gst_bin_get_by_name(GST_BIN(p.pipeline), "drift");
            GstElement* pay = gst_bin_get_by_name(GST_BIN(p.pipeline), "pay");
            if (!drift || !pay) {
                if (drift) gst_object_unref(drift);
                if (pay) gst_object_unref(pay);
                continue;
            }
            GstClock* clock = gst_element_get_clock(p.pipeline);
            if (!clock) {
                gst_object_unref(drift);
                gst_object_unref(pay);
                continue;
            }

            // Measure against the RTP timestamps we actually emitted.  These
            // step by exactly one packet, so the only error is a single sample
            // (21us) over the whole window -- unlike a position query, which
            // reports the last buffer the sink handled and therefore jitters by
            // a full graph quantum (+/-10ms measured).  That jitter is ~45x the
            // drift being corrected, and an earlier version of this loop spent
            // its life chasing it into the slew limit.
            guint ts = 0;
            g_object_get(pay, "timestamp", &ts, NULL);
            GstClockTime now = gst_clock_get_time(clock);
            gst_object_unref(clock);
            gst_object_unref(pay);

            PipelineState& st = state[id];
            if (!st.primed) {
                st.lastTs = ts;
                st.lastClock = now;
                st.primed = true;
                st.window.push_back({now, 0.0});
                gst_object_unref(drift);
                continue;
            }

            double emitted = (double)((guint32)(ts - st.lastTs));   // wraps correctly
            st.lastTs = ts;

            // Reject discontinuities rather than measuring them as drift.
            // Between tracks, and whenever FlushSendPipelines() drops buffers,
            // the stream simply stops for a while; the media timeline then
            // legitimately loses time against the wall clock, which is
            // indistinguishable from a very slow clock if taken at face value.
            // Measured with five track restarts in a window it reported
            // -7500ppm and nearly a second of "lag" -- all of it gaps.
            double expected = (double)(now - st.lastClock) / 1e9 * AES67::AUDIO_RATE;
            st.lastClock = now;
            if (expected > 0 && (emitted < expected * 0.9 || emitted > expected * 1.1)) {
                LogDebug(VB_MEDIAOUT,
                         "AES67 drift [%d]: stream discontinuity (%.0f of %.0f samples), "
                         "restarting measurement\n", id, emitted, expected);
                st.window.clear();
                st.cumulative = 0.0;
                st.window.push_back({now, 0.0});
                gst_object_unref(drift);
                continue;
            }

            st.cumulative += emitted;
            st.window.push_back({now, st.cumulative});
            while (st.window.size() > 2 &&
                   (now - st.window.front().clock) > (GstClockTime)WINDOW_S * GST_SECOND) {
                st.window.pop_front();
            }
            if (st.window.size() < 3) {
                gst_object_unref(drift);
                continue;
            }

            const Sample& a = st.window.front();
            const Sample& b = st.window.back();
            double dClockS = (double)(b.clock - a.clock) / 1e9;
            if (dClockS < (double)WINDOW_S * 0.5) {
                // Not enough baseline yet to measure anything at this precision.
                gst_object_unref(drift);
                continue;
            }

            // Least-squares slope of media seconds against PTP seconds across
            // the window.  Averaging down the per-sample quantisation is the
            // whole point; endpoints alone are far too noisy.
            double n = 0, sx = 0, sy = 0, sxx = 0, sxy = 0;
            for (const Sample& w : st.window) {
                double x = (double)(w.clock - a.clock) / 1e9;
                double y = (w.samples - a.samples) / (double)AES67::AUDIO_RATE;
                n += 1; sx += x; sy += y; sxx += x * x; sxy += x * y;
            }
            double denom = n * sxx - sx * sx;
            if (denom <= 0) {
                gst_object_unref(drift);
                continue;
            }
            double ratio = (n * sxy - sx * sy) / denom;   // 1.0 = locked to PTP
            double lagS = dClockS - (b.samples - a.samples) / (double)AES67::AUDIO_RATE;

            // Sign: the "speed" element plays at `speed` x rate, so it emits
            // FEWER samples as speed rises -- media time advances as 1/speed.
            // Media running slow (ratio < 1) therefore needs speed to come
            // DOWN.  Getting this backwards drives the loop straight into the
            // clamp and makes the drift worse, which is exactly what the first
            // rewrite of this loop did (-111ppm became -770ppm at +500ppm trim).
            //
            // Rate term takes a fraction of the error so the loop settles
            // rather than ringing; offset term is tiny and only bleeds off
            // accumulated lag.
            // Keep accumulating the window every iteration, but only move the
            // trim once the previous move has had time to show up.
            if (++st.sinceActuate < ACTUATE_EVERY) {
                gst_object_unref(drift);
                continue;
            }
            st.sinceActuate = 0;

            double next = st.speed * (1.0 + KP_RATE * (ratio - 1.0)) * (1.0 - KP_OFFSET * lagS);
            if (next > st.speed + MAX_STEP) next = st.speed + MAX_STEP;
            if (next < st.speed - MAX_STEP) next = st.speed - MAX_STEP;
            if (next < 1.0 - MAX_TRIM) next = 1.0 - MAX_TRIM;
            if (next > 1.0 + MAX_TRIM) next = 1.0 + MAX_TRIM;

            if (fabs(next - st.speed) > 1e-9) {
                st.speed = next;
                // "pitch" (soundtouch) exposes the rate as "rate".  The
                // previous element, "speed", accepted its property and read it
                // back while doing nothing to a live source -- so verify any
                // replacement by watching the measured drift respond, never by
                // reading the property back.
                g_object_set(drift, "rate", (gfloat)st.speed, NULL);
            }

            LogDebug(VB_MEDIAOUT,
                     "AES67 drift [%d]: over %.0fs media/PTP %.7f (%+.1f ppm), lag %+.2f ms, trim %+.1f ppm\n",
                     id, dClockS, ratio, (ratio - 1.0) * 1e6, lagS * 1000.0,
                     (st.speed - 1.0) * 1e6);
            gst_object_unref(drift);
        }
    }
    LogInfo(VB_MEDIAOUT, "AES67 drift control thread stopped\n");
}

void AES67Manager::CheckPtpWatchdog() {
    if (!m_config.ptpEnabled || !m_ptpInitialized) {
        return;
    }

    if (!IsPtp4lRunning()) {
        LogWarn(VB_MEDIAOUT, "AES67 watchdog: ptp4l is gone — restarting PTP\n");
        // Tear down first: InitPTP() is a no-op while m_ptpInitialized is set,
        // and phc2sys must not be left pointed at a dead daemon.
        ShutdownPTP();
        if (InitPTP()) {
            LogInfo(VB_MEDIAOUT, "AES67 watchdog: ptp4l restarted\n");
        } else {
            LogErr(VB_MEDIAOUT, "AES67 watchdog: ptp4l restart failed\n");
        }
        return;
    }

}

bool AES67Manager::PollPipelinesWatchdog() {
    bool needsRebuild = false;

    {  // scope for pipeline mutex
    std::lock_guard<std::mutex> lock(m_pipelineMutex);

    auto checkPipelines = [this, &needsRebuild](std::map<int, AES67Pipeline>& pipelines, const char* direction) {
        for (auto& kv : pipelines) {
            AES67Pipeline& p = kv.second;
            if (!p.pipeline || !p.running) continue;

            // Drain bus messages (error, warning, state changes)
            if (p.bus) {
                GstMessage* msg;
                while ((msg = gst_bus_pop(p.bus)) != nullptr) {
                    switch (GST_MESSAGE_TYPE(msg)) {
                        case GST_MESSAGE_ERROR: {
                            GError* err = nullptr; gchar* dbg = nullptr;
                            gst_message_parse_error(msg, &err, &dbg);
                            LogErr(VB_MEDIAOUT,
                                   "AES67 %s pipeline [%d] bus error: %s\n",
                                   direction, p.instanceId, err->message);
                            g_error_free(err); g_free(dbg);
                            p.errorMessage = "GStreamer error";
                            break;
                        }
                        case GST_MESSAGE_WARNING: {
                            GError* err = nullptr; gchar* dbg = nullptr;
                            gst_message_parse_warning(msg, &err, &dbg);
                            LogWarn(VB_MEDIAOUT,
                                    "AES67 %s pipeline [%d] bus warning: %s\n",
                                    direction, p.instanceId, err->message);
                            g_error_free(err); g_free(dbg);
                            break;
                        }
                        default: break;
                    }
                    gst_message_unref(msg);
                }
            }

            // Check current pipeline state
            GstState curState = GST_STATE_NULL, pendingState = GST_STATE_VOID_PENDING;
            gst_element_get_state(p.pipeline, &curState, &pendingState, 0 /* no blocking */);

            if (curState != GST_STATE_PLAYING && pendingState != GST_STATE_PLAYING) {
                // Don't attempt recovery while holding m_pipelineMutex —
                // gst_element_set_state can block on PipeWire indefinitely.
                // Just flag for a full rebuild instead.
                LogWarn(VB_MEDIAOUT,
                        "AES67 %s pipeline [%d] is in %s state — flagging for rebuild\n",
                        direction, p.instanceId,
                        gst_element_state_get_name(curState));
                p.running = false;
                p.errorMessage = "Watchdog: not in PLAYING state";
                needsRebuild = true;
            } else if (p.isSend) {
                // Zombie detection for send pipelines: check if udpsink
                // is actually pushing bytes.  pipewiresrc can lose its
                // PipeWire socket connection (e.g. PipeWire restart) while
                // GStreamer still reports PLAYING — producing no output.
                GstElement* usink = gst_bin_get_by_name(GST_BIN(p.pipeline), "usink");
                if (usink) {
                    guint64 bytesSent = 0;
                    g_object_get(usink, "bytes-served", &bytesSent, NULL);
                    gst_object_unref(usink);

                    // Sink-pacing diagnostics.  The queue is the whole reason
                    // pacing can be done without starving the source, so its
                    // depth is what explains a stream that paces correctly for
                    // a minute and then reverts to bursting: empty means the
                    // sink is starved and every buffer arrives past its send
                    // time, full means backpressure has reached pipewiresrc and
                    // audio is being dropped there.  No element by this name
                    // exists unless sink pacing is on, so this costs nothing
                    // and needs no lock on the config.
                    // Whether the rate matcher is actually correcting is a
                    // readable fact here, not an inference -- which is the
                    // whole reason it was chosen over "speed" and "pitch".
                    // At ~56ppm this should climb by ~2.7 samples/s.
                    GstElement* rm = gst_bin_get_by_name(GST_BIN(p.pipeline),
                                                         "ratematch");
                    if (rm) {
                        guint64 added = 0, dropped = 0, in = 0, out = 0;
                        g_object_get(rm, "add", &added, "drop", &dropped,
                                     "in", &in, "out", &out, NULL);
                        gst_object_unref(rm);
                        LogInfo(VB_MEDIAOUT,
                                "AES67 send [%d] ratematch: in %llu out %llu, added %llu dropped %llu\n",
                                p.instanceId, (unsigned long long)in,
                                (unsigned long long)out,
                                (unsigned long long)added,
                                (unsigned long long)dropped);
                    }

                    GstElement* sinkq = gst_bin_get_by_name(GST_BIN(p.pipeline),
                                                            "sinkq");
                    if (sinkq) {
                        guint qBuffers = 0;
                        guint64 qTime = 0;
                        g_object_get(sinkq, "current-level-buffers", &qBuffers,
                                     "current-level-time", &qTime, NULL);
                        gst_object_unref(sinkq);
                        LogInfo(VB_MEDIAOUT,
                                "AES67 send [%d] pacing: queue %u buffers / %llums, +%llu bytes since last check\n",
                                p.instanceId, qBuffers,
                                (unsigned long long)(qTime / GST_MSECOND),
                                (unsigned long long)(bytesSent - p.lastByteCount));
                    }

                    // Compare throughput against what this instance should
                    // be emitting.  Payload is ptime worth of 24-bit frames,
                    // and at nominal that is AUDIO_RATE * 3 * channels bytes a
                    // second regardless of ptime.
                    const auto nowT = std::chrono::steady_clock::now();
                    if (p.lastByteTime.time_since_epoch().count() != 0 &&
                        bytesSent > p.lastByteCount) {
                        const double secs =
                            std::chrono::duration<double>(nowT - p.lastByteTime)
                                .count();
                        const double expected =
                            (double)AES67::AUDIO_RATE * 3.0 * p.channels;
                        if (secs > 5.0 && expected > 0) {
                            const double got =
                                (double)(bytesSent - p.lastByteCount) / secs;
                            // 95%, not something looser: the degradation
                            // settles at 89-94% of nominal, so an 85% trigger
                            // sat below every case actually observed and would
                            // never have fired.  A healthy stream measures
                            // 100.0% consistently, so the margin is real, and
                            // three consecutive checks means ~90s of sustained
                            // under-delivery -- far longer than the brief dip a
                            // track change produces.
                            if (got < expected * 0.95) {
                                p.lowRateCount++;
                                LogWarn(VB_MEDIAOUT,
                                        "AES67 %s pipeline [%d] under-delivering: %.0f of %.0f B/s (%.0f%%), check %d\n",
                                        direction, p.instanceId, got, expected,
                                        100.0 * got / expected, p.lowRateCount);
                                if (p.lowRateCount >= 3) {
                                    p.running = false;
                                    p.errorMessage =
                                        "Watchdog: sustained under-delivery";
                                    needsRebuild = true;
                                }
                            } else {
                                p.lowRateCount = 0;
                            }
                        }
                    }
                    p.lastByteTime = nowT;

                    if (bytesSent == p.lastByteCount) {
                        p.stallCount++;
                        if (p.stallCount >= 2) {
                            LogWarn(VB_MEDIAOUT,
                                    "AES67 %s pipeline [%d] stalled (bytes-sent=%llu for %d checks) — scheduling rebuild\n",
                                    direction, p.instanceId, (unsigned long long)bytesSent, p.stallCount);
                            p.running = false;
                            p.errorMessage = "Watchdog: pipeline stalled";
                            needsRebuild = true;
                        }
                    } else {
                        p.stallCount = 0;
                    }
                    p.lastByteCount = bytesSent;
                }
            }
        }
    };

    checkPipelines(m_sendPipelines, "send");
    checkPipelines(m_recvPipelines, "recv");

    // If any pipeline needs rebuilding, re-apply config.
    // ApplyConfig() tears down all pipelines and recreates them with fresh
    // PipeWire connections.
    if (needsRebuild) {
        LogWarn(VB_MEDIAOUT, "AES67 watchdog: triggering full pipeline rebuild\n");
    }
    }  // end pipeline mutex scope

    return needsRebuild;
}

gboolean AES67Manager::OnBusMessage(GstBus* bus, GstMessage* msg, gpointer userData) {
    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR: {
            GError* err = nullptr;
            gchar* debug = nullptr;
            gst_message_parse_error(msg, &err, &debug);
            LogErr(VB_MEDIAOUT, "AES67 pipeline error: %s (debug: %s)\n",
                   err->message, debug ? debug : "none");
            g_error_free(err);
            g_free(debug);
            break;
        }
        case GST_MESSAGE_WARNING: {
            GError* err = nullptr;
            gchar* debug = nullptr;
            gst_message_parse_warning(msg, &err, &debug);
            LogWarn(VB_MEDIAOUT, "AES67 pipeline warning: %s\n", err->message);
            g_error_free(err);
            g_free(debug);
            break;
        }
        case GST_MESSAGE_STATE_CHANGED: {
            if (GST_MESSAGE_SRC(msg) == GST_OBJECT(msg)) {
                GstState oldState, newState, pending;
                gst_message_parse_state_changed(msg, &oldState, &newState, &pending);
                LogDebug(VB_MEDIAOUT, "AES67 pipeline state: %s → %s\n",
                         gst_element_state_get_name(oldState),
                         gst_element_state_get_name(newState));
            }
            break;
        }
        default:
            break;
    }
    return TRUE;  // keep watching
}

// ──────────────────────────────────────────────────────────────────────────────
// Network helpers
// ──────────────────────────────────────────────────────────────────────────────
std::string AES67Manager::GetInterfaceIP(const std::string& iface) {
    struct ifaddrs* addrs = nullptr;
    std::string result = "0.0.0.0";

    if (getifaddrs(&addrs) != 0) {
        return result;
    }

    for (struct ifaddrs* ifa = addrs; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
        if (iface.empty() || iface == ifa->ifa_name) {
            if (std::string(ifa->ifa_name) == "lo") continue;
            struct sockaddr_in* addr = (struct sockaddr_in*)ifa->ifa_addr;
            char buf[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &addr->sin_addr, buf, sizeof(buf));
            result = buf;
            if (!iface.empty()) break;  // found the specific interface
        }
    }

    freeifaddrs(addrs);
    return result;
}

std::string AES67Manager::SafeNodeName(const std::string& name) {
    std::string result = "aes67_";
    for (char c : name) {
        if (std::isalnum(c) || c == '_') {
            result += std::tolower(c);
        } else {
            result += '_';
        }
    }
    return result;
}

// ──────────────────────────────────────────────────────────────────────────────
// SAP Announcer — RFC 2974 compliant
// Replaces external fpp_aes67_sap Python daemon
// ──────────────────────────────────────────────────────────────────────────────
uint16_t AES67Manager::ComputeSAPHash(const std::string& sdp) {
    // RFC 2974 §6: the message id hash identifies "the precise version of this
    // announcement" and MUST change when the payload changes.  It is therefore
    // computed over the SDP text, not over the instance: an announcement whose
    // refclk we corrected but whose hash stayed put is discarded as a repeat by
    // a compliant receiver, which is how a stale ts-refclk survives forever.
    //
    // Session *identity* is carried by the SDP o= line (stable sess-id, rising
    // sess-version), so a changed hash reads as "this session was modified",
    // not as a second session.
    uint32_t hash = 2166136261u;
    for (char c : sdp) {
        hash ^= (uint32_t)(unsigned char)c;
        hash *= 16777619u;
    }
    return (uint16_t)(hash & 0xFFFF);
}

// Monotonic SDP o= version.  Seeded from the wall clock so it keeps rising
// across fppd restarts -- a receiver that has cached version N ignores a
// re-announcement numbered below it.
void AES67Manager::LoadSDPVersion() {
    if (m_sdpVersionPath.empty()) {
        m_sdpVersionPath = getFPPMediaDir("/config/.aes67-sdp-version");
    }
    std::ifstream f(m_sdpVersionPath);
    if (!f.is_open()) {
        return;
    }
    std::string key;
    uint64_t ver = 0;
    f >> key >> ver;
    if (!key.empty() && ver > 0) {
        m_sdpBodyKey = key;
        m_lastSdpVersion.store((uint32_t)ver, std::memory_order_relaxed);
    }
}

void AES67Manager::SaveSDPVersion() {
    if (m_sdpVersionPath.empty()) {
        return;
    }
    std::ofstream f(m_sdpVersionPath, std::ios::trunc);
    if (f.is_open()) {
        f << m_sdpBodyKey << " " << m_lastSdpVersion.load(std::memory_order_relaxed) << "\n";
    }
}

// Returns the version to stamp on this announcement.  Same body as last time =
// same version, so the SDP (and therefore the SAP msg id hash derived from it)
// is byte-identical and receivers see one continuing session rather than a new
// one per restart.
uint32_t AES67Manager::SDPVersionFor(const std::string& body) {
    if (m_sdpVersionPath.empty()) {
        LoadSDPVersion();
    }

    uint32_t h = 2166136261u;
    for (char c : body) {
        h ^= (uint32_t)(unsigned char)c;
        h *= 16777619u;
    }
    char keyBuf[16];
    snprintf(keyBuf, sizeof(keyBuf), "%08x", h);
    std::string key(keyBuf);

    if (key == m_sdpBodyKey) {
        uint32_t existing = m_lastSdpVersion.load(std::memory_order_relaxed);
        if (existing > 0) {
            return existing;
        }
    }

    uint32_t v = NextSDPVersion();
    m_sdpBodyKey = key;
    SaveSDPVersion();
    LogInfo(VB_MEDIAOUT, "AES67 SAP: announcement content changed, SDP version now %u\n", v);
    return v;
}

uint32_t AES67Manager::NextSDPVersion() {
    uint32_t candidate = (uint32_t)time(nullptr);
    uint32_t prev = m_lastSdpVersion.load(std::memory_order_relaxed);
    uint32_t next;
    do {
        next = (candidate > prev) ? candidate : prev + 1;
    } while (!m_lastSdpVersion.compare_exchange_weak(prev, next, std::memory_order_relaxed));
    return next;
}

std::string AES67Manager::BuildSDP(const AES67Instance& inst,
                                    const std::string& sourceIP,
                                    const std::string& ptpClockId,
                                    uint32_t sdpVersion) {
    // AES67-compliant SDP — unique session ID per device + stream.
    // Combine source IP, stream name, multicast IP, and port so that
    // different FPP boxes (or different streams on the same box) always
    // produce distinct o= lines.
    std::string key = sourceIP + ":" + inst.name + ":" +
                      inst.multicastIP + ":" + std::to_string(inst.port);
    // FNV-1a hash → deterministic, stable across restarts, unique per key
    uint32_t h = 2166136261u;
    for (char c : key) {
        h ^= (uint32_t)(unsigned char)c;
        h *= 16777619u;
    }
    int sessionId = (int)(h & 0x3FFFFFFFu);  // 30-bit positive value

    // o=<user> <sess-id> <sess-version> ...  RFC 4566: sess-id identifies the
    // session and must stay put for its lifetime; sess-version rises each time
    // the description is modified, which is how a receiver knows to re-read a
    // session it already has (e.g. after BMCA changed the ts-refclk).
    std::ostringstream sdp;
    sdp << "v=0\r\n"
        << "o=- " << sessionId << " " << sdpVersion << " IN IP4 " << sourceIP << "\r\n"
        << "s=" << inst.sessionName << "\r\n"
        << "c=IN IP4 " << inst.multicastIP << "/" << AES67::AUDIO_RTP_TTL << "\r\n"
        << "t=0 0\r\n"
        << "m=audio " << inst.port << " RTP/AVP " << AES67::RTP_PAYLOAD_TYPE << "\r\n"
        << "a=rtpmap:" << AES67::RTP_PAYLOAD_TYPE << " L24/"
        << AES67::AUDIO_RATE << "/" << inst.channels << "\r\n"
        << "a=sendonly\r\n"
        << "a=ptime:" << inst.ptime << "\r\n"
        << "a=ts-refclk:ptp=IEEE1588-2008:" << ptpClockId << ":0\r\n"
        << "a=mediaclk:direct=0\r\n";
    // NOTE: AES67 requires a=mediaclk:direct, and "direct=0" asserts that the
    // RTP timestamp is derived directly from the reference clock with zero
    // offset.  That is not yet true here -- the payloader's timestamps come
    // from the pipeline's monotonic clock with its own start offset, so the
    // rate is PTP-locked but the phase is arbitrary.  Making the assertion
    // true needs a GstPtpClock on the send pipeline plus an explicit
    // rtpL24pay timestamp-offset derived from PTP time.

    return sdp.str();
}

std::vector<uint8_t> AES67Manager::BuildSAPPacket(const std::string& sourceIP,
                                                   uint16_t msgIdHash,
                                                   const std::string& sdp,
                                                   bool isDeletion) {
    // RFC 2974 SAP packet:
    //   Byte 0: V=1 (bits 7-5), A=0 (bit 4), R=0 (bit 3), T=isDeletion (bit 2),
    //           E=0 (bit 1), C=0 (bit 0)
    //   Byte 1: Auth length = 0
    //   Bytes 2-3: Message ID hash (network byte order)
    //   Bytes 4-7: Originating source (IPv4)
    //   Payload type: "application/sdp\0"
    //   SDP data

    std::string payloadType = "application/sdp";

    uint8_t header0 = (AES67::SAP_VERSION << 5);  // V=1
    if (isDeletion) {
        header0 |= 0x04;  // T=1 (deletion)
    }

    // Parse source IP
    struct in_addr srcAddr;
    inet_pton(AF_INET, sourceIP.c_str(), &srcAddr);

    std::vector<uint8_t> packet;
    packet.push_back(header0);
    packet.push_back(0);  // auth length
    packet.push_back((msgIdHash >> 8) & 0xFF);
    packet.push_back(msgIdHash & 0xFF);

    // Source IP (4 bytes, network order)
    uint8_t* ipBytes = (uint8_t*)&srcAddr.s_addr;
    packet.push_back(ipBytes[0]);
    packet.push_back(ipBytes[1]);
    packet.push_back(ipBytes[2]);
    packet.push_back(ipBytes[3]);

    // Payload type string + NUL
    for (char c : payloadType) {
        packet.push_back((uint8_t)c);
    }
    packet.push_back(0);

    // SDP data
    for (char c : sdp) {
        packet.push_back((uint8_t)c);
    }

    return packet;
}

void AES67Manager::SAPAnnounceLoop() {
    LogInfo(VB_MEDIAOUT, "AES67 SAP announcer thread started\n");

    // Create UDP socket for SAP multicast
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        LogErr(VB_MEDIAOUT, "AES67 SAP: Failed to create socket: %s\n", FPPstrerror(errno));
        return;
    }

    // Set TTL
    int ttl = AES67::SAP_TTL;
    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));

    // Set multicast interface if specified
    if (!m_config.ptpInterface.empty()) {
        std::string ifIP = GetInterfaceIP(m_config.ptpInterface);
        struct in_addr localAddr;
        inet_pton(AF_INET, ifIP.c_str(), &localAddr);
        setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, &localAddr, sizeof(localAddr));
    }

    struct sockaddr_in sapAddr;
    memset(&sapAddr, 0, sizeof(sapAddr));
    sapAddr.sin_family = AF_INET;
    sapAddr.sin_port = htons(AES67::SAP_PORT);
    inet_pton(AF_INET, AES67::SAP_MCAST_ADDRESS, &sapAddr.sin_addr);

    // SDP's ts-refclk must identify the domain's actual grandmaster (RFC 7273),
    // not this node's own identity — otherwise a follower incorrectly
    // advertises itself as the clock source.  Fall back to our own derived
    // ID only if PTP is disabled or ptp4l hasn't selected a grandmaster yet.
    auto queryPtpClockId = [this]() -> std::string {
        std::string gm = GetActiveGrandmasterId();
        return gm.empty() ? GetPTPClockId() : gm;
    };

    // Build all SAP packets for send instances
    struct SAPEntry {
        uint16_t hash;
        std::vector<uint8_t> announcePacket;
        std::vector<uint8_t> deletePacket;
    };
    auto buildEntries = [this](const std::string& ptpClockId) -> std::vector<SAPEntry> {
        std::vector<SAPEntry> result;

        // Version the announcement by what is in it.  Building the bodies with
        // a fixed placeholder version first gives a stable key to compare
        // against the last announcement -- see SDPVersionFor().
        std::string bodyKey;
        for (const auto& inst : m_config.instances) {
            if (!inst.enabled || !inst.sapEnabled) continue;
            if (inst.mode != "send" && inst.mode != "both") continue;
            std::string sourceIP = GetInterfaceIP(inst.interface.empty() ?
                                                  m_config.ptpInterface : inst.interface);
            bodyKey += BuildSDP(inst, sourceIP, ptpClockId, 0);
        }
        uint32_t sdpVersion = SDPVersionFor(bodyKey);

        for (const auto& inst : m_config.instances) {
            if (!inst.enabled) continue;
            if (!inst.sapEnabled) continue;
            if (inst.mode != "send" && inst.mode != "both") continue;

            std::string sourceIP = GetInterfaceIP(inst.interface.empty() ?
                                                  m_config.ptpInterface : inst.interface);
            std::string sdp = BuildSDP(inst, sourceIP, ptpClockId, sdpVersion);
            uint16_t hash = ComputeSAPHash(sdp);

            SAPEntry entry;
            entry.hash = hash;
            entry.announcePacket = BuildSAPPacket(sourceIP, hash, sdp, false);
            entry.deletePacket = BuildSAPPacket(sourceIP, hash, sdp, true);
            result.push_back(entry);
        }
        return result;
    };

    std::string ptpClockId = queryPtpClockId();
    std::vector<SAPEntry> entries = buildEntries(ptpClockId);

    if (entries.empty()) {
        LogWarn(VB_MEDIAOUT, "AES67 SAP: No SAP-enabled send instances — announcer has nothing to send\n");
    } else {
        LogInfo(VB_MEDIAOUT, "AES67 SAP: Announcing %d stream(s) to %s:%d every %ds\n",
                (int)entries.size(), AES67::SAP_MCAST_ADDRESS, AES67::SAP_PORT,
                AES67::SAP_ANNOUNCE_INTERVAL_S);
    }

    auto announceAll = [&]() {
        for (const auto& entry : entries) {
            ssize_t sent = sendto(sock, entry.announcePacket.data(), entry.announcePacket.size(), 0,
                                  (struct sockaddr*)&sapAddr, sizeof(sapAddr));
            if (sent < 0) {
                LogErr(VB_MEDIAOUT, "AES67 SAP: sendto failed: %s\n", FPPstrerror(errno));
            }
        }
    };

    // Rebuild and re-announce immediately when BMCA changes the refclk, rather
    // than letting a wrong ts-refclk stand for the rest of the announce cycle.
    auto refreshRefclk = [&]() -> bool {
        std::string current = queryPtpClockId();
        if (current == ptpClockId) {
            return false;
        }
        LogInfo(VB_MEDIAOUT, "AES67 SAP: PTP refclk changed (%s -> %s), rebuilding SDP\n",
                ptpClockId.c_str(), current.c_str());
        ptpClockId = current;
        // The rebuilt entries carry a new msg id hash (it is derived from the
        // SDP text).  We deliberately do NOT delete the old hash first: the
        // o= sess-id is unchanged, so a receiver reads this as a modification
        // of a session it already has, whereas a deletion could make it tear
        // the stream down for the moment before the new announcement lands.
        entries = buildEntries(ptpClockId);
        return true;
    };

    // Announce loop
    auto threadStart = std::chrono::steady_clock::now();
    while (m_sapAnnounceRunning.load()) {
        refreshRefclk();
        announceAll();

        // Sleep for SAP_ANNOUNCE_INTERVAL_S, checking shutdown flag every second.
        //
        // ptp4l needs several seconds after startup to finish BMCA and adopt an
        // upstream master; until then it reports itself as its own grandmaster.
        // Poll every second through that window so the corrected refclk goes out
        // within a second of the election settling instead of up to a full
        // announce interval later.  Results are cached (PTP_QUERY_CACHE_MS), so
        // this is one pmc query per second at worst, and only for a minute.
        for (int i = 0; i < AES67::SAP_ANNOUNCE_INTERVAL_S && m_sapAnnounceRunning.load(); i++) {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                               std::chrono::steady_clock::now() - threadStart).count();
            if (elapsed < AES67::PTP_CONVERGENCE_WINDOW_S && m_sapAnnounceRunning.load()) {
                if (refreshRefclk()) {
                    announceAll();
                }
            }
        }

        // Poll pipeline bus messages and recover any crashed pipelines.
        // Runs every SAP_ANNOUNCE_INTERVAL_S (30s) — fast enough to detect
        // silent failures without adding significant overhead.
        if (m_sapAnnounceRunning.load()) {
            CheckPtpWatchdog();
            if (PollPipelinesWatchdog()) {
                // A full pipeline rebuild is needed.  We cannot call
                // ApplyConfig() from this thread because ApplyConfig()
                // joins m_sapAnnounceThread — which IS this thread — causing
                // a deadlock ("Resource deadlock avoided" / SIGABRT).
                // Instead, stop this loop and spawn a short-lived thread
                // that calls ApplyConfig() after we exit.
                LogWarn(VB_MEDIAOUT, "AES67 watchdog: rebuild needed, stopping SAP loop and scheduling ApplyConfig\n");
                m_sapAnnounceRunning.store(false);
                // Track this as m_rebuildThread (rather than detaching) so
                // Shutdown() can join it before tearing anything else down
                // -- a detached thread could otherwise call ApplyConfig()
                // and resurrect pipelines after Shutdown() has already run,
                // and a detached thread racing Shutdown()'s own joins of
                // m_sapAnnounceThread/m_sapRecvThread is undefined behavior.
                if (m_rebuildThread.joinable()) {
                    m_rebuildThread.join();
                }
                m_rebuildThread = std::thread([this]() {
                    // Brief delay to let the SAP announce thread finish
                    // exiting and become joinable.
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    // If Shutdown() ran while we were sleeping, don't
                    // resurrect pipelines -- just exit quietly.
                    if (!m_initialized.load()) {
                        return;
                    }
                    ApplyConfig();
                });
                break;
            }
        }
    }

    // Send deletion packets on shutdown
    for (const auto& entry : entries) {
        sendto(sock, entry.deletePacket.data(), entry.deletePacket.size(), 0,
               (struct sockaddr*)&sapAddr, sizeof(sapAddr));
    }

    close(sock);
    LogInfo(VB_MEDIAOUT, "AES67 SAP announcer thread stopped (deletion packets sent)\n");
}

// ──────────────────────────────────────────────────────────────────────────────
// SAP Receiver — listens for remote AES67 announcements
// ──────────────────────────────────────────────────────────────────────────────
void AES67Manager::SAPReceiveLoop() {
    LogInfo(VB_MEDIAOUT, "AES67 SAP receiver thread started\n");

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        LogErr(VB_MEDIAOUT, "AES67 SAP recv: Failed to create socket: %s\n", FPPstrerror(errno));
        return;
    }

    // Allow address reuse
    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // Bind to SAP port
    struct sockaddr_in bindAddr;
    memset(&bindAddr, 0, sizeof(bindAddr));
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_port = htons(AES67::SAP_PORT);
    bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr*)&bindAddr, sizeof(bindAddr)) < 0) {
        LogErr(VB_MEDIAOUT, "AES67 SAP recv: bind failed: %s\n", FPPstrerror(errno));
        close(sock);
        return;
    }

    // Join SAP multicast group
    struct ip_mreq mreq;
    inet_pton(AF_INET, AES67::SAP_MCAST_ADDRESS, &mreq.imr_multiaddr);

    if (!m_config.ptpInterface.empty()) {
        std::string ifIP = GetInterfaceIP(m_config.ptpInterface);
        inet_pton(AF_INET, ifIP.c_str(), &mreq.imr_interface);
    } else {
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    }

    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        LogWarn(VB_MEDIAOUT, "AES67 SAP recv: join multicast failed: %s\n", FPPstrerror(errno));
    }

    // Set receive timeout so we can check the shutdown flag
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    uint8_t buf[4096];
    while (m_sapRecvRunning.load()) {
        struct sockaddr_in senderAddr;
        socklen_t addrLen = sizeof(senderAddr);

        ssize_t n = recvfrom(sock, buf, sizeof(buf), 0,
                             (struct sockaddr*)&senderAddr, &addrLen);
        if (n <= 0) continue;  // timeout or error

        char senderIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &senderAddr.sin_addr, senderIP, sizeof(senderIP));

        HandleSAPPacket(buf, (size_t)n, senderIP);
    }

    // Leave multicast group
    setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq));
    close(sock);
    LogInfo(VB_MEDIAOUT, "AES67 SAP receiver thread stopped\n");
}

void AES67Manager::HandleSAPPacket(const uint8_t* data, size_t len,
                                    const std::string& senderAddr) {
    // Minimum SAP header: 8 bytes (4 header + 4 source IP)
    if (len < 8) return;

    uint8_t header0 = data[0];
    int version = (header0 >> 5) & 0x07;
    bool isDeletion = (header0 & 0x04) != 0;
    // bool isIPv6 = (header0 & 0x10) != 0;  // A bit — we only handle IPv4

    if (version != AES67::SAP_VERSION) return;

    uint16_t msgIdHash = ((uint16_t)data[2] << 8) | data[3];

    // Skip auth data
    uint8_t authLen = data[1];
    size_t payloadStart = 8 + authLen * 4;
    if (payloadStart >= len) return;

    // Find end of payload type string (NUL terminated)
    size_t sdpStart = payloadStart;
    while (sdpStart < len && data[sdpStart] != 0) {
        sdpStart++;
    }
    sdpStart++;  // skip NUL
    if (sdpStart >= len) return;

    // Ignore our own announcements
    std::string ourIP = GetInterfaceIP(m_config.ptpInterface);
    if (senderAddr == ourIP) return;

    if (isDeletion) {
        std::lock_guard<std::mutex> lock(m_discoveredMutex);
        auto it = m_discoveredStreams.find(msgIdHash);
        if (it != m_discoveredStreams.end()) {
            LogInfo(VB_MEDIAOUT, "AES67 SAP: Stream deleted: %s\n",
                    it->second.sessionName.c_str());
            m_discoveredStreams.erase(it);
        }
        return;
    }

    // Parse SDP for stream info
    std::string sdp((const char*)data + sdpStart, len - sdpStart);

    SAPDiscoveredStream stream;
    stream.msgIdHash = msgIdHash;
    stream.originAddress = senderAddr;
    stream.lastSeenMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    // Parse SDP fields
    std::istringstream sdpStream(sdp);
    std::string line;
    while (std::getline(sdpStream, line)) {
        // Remove \r
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (line.substr(0, 2) == "s=") {
            stream.sessionName = line.substr(2);
        } else if (line.substr(0, 2) == "c=") {
            // c=IN IP4 239.69.0.1/255
            size_t ip4Pos = line.find("IP4 ");
            if (ip4Pos != std::string::npos) {
                std::string addr = line.substr(ip4Pos + 4);
                size_t slash = addr.find('/');
                if (slash != std::string::npos) addr = addr.substr(0, slash);
                stream.multicastIP = addr;
            }
        } else if (line.substr(0, 8) == "m=audio ") {
            // m=audio 5004 RTP/AVP 96
            int port = 0;
            if (sscanf(line.c_str(), "m=audio %d", &port) == 1) {
                stream.port = port;
            }
        } else if (line.substr(0, 11) == "a=rtpmap:96") {
            // a=rtpmap:96 L24/48000/2
            int ch = 2;
            if (sscanf(line.c_str(), "a=rtpmap:96 L24/%*d/%d", &ch) == 1) {
                stream.channels = ch;
            }
        } else if (line.substr(0, 8) == "a=ptime:") {
            stream.ptime = std::atoi(line.substr(8).c_str());
        } else if (line.substr(0, 26) == "a=ts-refclk:ptp=IEEE1588-") {
            // a=ts-refclk:ptp=IEEE1588-2008:AA-BB-CC-FF-FE-DD-EE-FF:0
            size_t colon1 = line.find(':', 15);
            if (colon1 != std::string::npos) {
                size_t colon2 = line.find(':', colon1 + 1);
                if (colon2 != std::string::npos) {
                    stream.ptpClockId = line.substr(colon1 + 1, colon2 - colon1 - 1);
                }
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_discoveredMutex);
        bool isNew = (m_discoveredStreams.find(msgIdHash) == m_discoveredStreams.end());
        m_discoveredStreams[msgIdHash] = stream;

        if (isNew) {
            LogInfo(VB_MEDIAOUT, "AES67 SAP: Discovered stream '%s' from %s → %s:%d (%dch)\n",
                    stream.sessionName.c_str(), senderAddr.c_str(),
                    stream.multicastIP.c_str(), stream.port, stream.channels);
        }

        // Prune stale entries.  A remote sender that stops announcing
        // without ever transmitting an explicit SAP deletion packet (e.g.
        // it crashes, loses power, or drops off the network) would
        // otherwise leave its stream in m_discoveredStreams forever.  Drop
        // anything not seen within ~10 announce intervals.
        uint64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        constexpr uint64_t staleThresholdMs =
            (uint64_t)AES67::SAP_ANNOUNCE_INTERVAL_S * 1000ULL * 10ULL;
        for (auto sit = m_discoveredStreams.begin(); sit != m_discoveredStreams.end(); ) {
            if (nowMs - sit->second.lastSeenMs > staleThresholdMs) {
                LogInfo(VB_MEDIAOUT, "AES67 SAP: Stream '%s' timed out (no announcement in %llus) — removing\n",
                        sit->second.sessionName.c_str(),
                        (unsigned long long)(staleThresholdMs / 1000));
                sit = m_discoveredStreams.erase(sit);
            } else {
                ++sit;
            }
        }
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Zero-hop inline RTP branches (7.9)
// ──────────────────────────────────────────────────────────────────────────────
bool AES67Manager::HasActiveSendInstances() {
    if (!m_active.load()) return false;
    std::lock_guard<std::mutex> lock(m_pipelineMutex);
    return !m_sendPipelines.empty();
}

std::vector<AES67Manager::InlineRTPBranch> AES67Manager::AttachInlineRTPBranches(
    GstElement* pipeline, GstElement* tee) {

    std::vector<InlineRTPBranch> branches;

    if (!m_active.load() || !pipeline || !tee) {
        return branches;
    }

    // Get the current send config (not the pipelines — those capture from pipewiresrc).
    // For each enabled send instance, we create a parallel RTP branch on the tee.
    AES67Config config;
    {
        // Grab a snapshot of the config
        config = m_config;
    }

    for (const auto& inst : config.instances) {
        if (!inst.enabled) continue;
        if (inst.mode != "send" && inst.mode != "both") continue;

        LogInfo(VB_MEDIAOUT, "AES67 zero-hop: Attaching inline RTP branch for '%s' → %s:%d\n",
                inst.name.c_str(), inst.multicastIP.c_str(), inst.port);

        int64_t ptimeNs = (int64_t)inst.ptime * 1000000LL;
        std::string branchName = "aes67_q_" + std::to_string(inst.id);

        // Create branch elements
        GstElement* queue = gst_element_factory_make("queue", branchName.c_str());
        GstElement* aconv = gst_element_factory_make("audioconvert",
            ("aes67_aconv_" + std::to_string(inst.id)).c_str());
        GstElement* capsf = gst_element_factory_make("capsfilter",
            ("aes67_caps_" + std::to_string(inst.id)).c_str());
        GstElement* rtppay = gst_element_factory_make("rtpL24pay",
            ("aes67_rtppay_" + std::to_string(inst.id)).c_str());
        GstElement* udpsink = gst_element_factory_make("udpsink",
            ("aes67_udpsink_" + std::to_string(inst.id)).c_str());

        if (!queue || !aconv || !capsf || !rtppay || !udpsink) {
            LogErr(VB_MEDIAOUT, "AES67 zero-hop: Failed to create elements for instance %d\n", inst.id);
            if (queue) gst_object_unref(queue);
            if (aconv) gst_object_unref(aconv);
            if (capsf) gst_object_unref(capsf);
            if (rtppay) gst_object_unref(rtppay);
            if (udpsink) gst_object_unref(udpsink);
            continue;
        }

        // Configure caps: S24BE at 48kHz
        GstCaps* caps = gst_caps_new_simple("audio/x-raw",
            "format", G_TYPE_STRING, AES67::AUDIO_FORMAT,
            "rate", G_TYPE_INT, AES67::AUDIO_RATE,
            "channels", G_TYPE_INT, inst.channels, NULL);
        g_object_set(capsf, "caps", caps, NULL);
        gst_caps_unref(caps);

        // Configure rtpL24pay
        g_object_set(rtppay,
            "pt", (guint)AES67::RTP_PAYLOAD_TYPE,
            "min-ptime", ptimeNs,
            "max-ptime", ptimeNs,
            NULL);

        // Configure udpsink — sync=FALSE so packets go out immediately.
        // The playback pipeline's tee delivers data at real-time rate;
        // adding sync=true would cause the udpsink to buffer based on the
        // pipeline clock, which can lag behind the stream position and
        // block output entirely.
        g_object_set(udpsink,
            "host", inst.multicastIP.c_str(),
            "port", inst.port,
            "ttl-mc", AES67::AUDIO_RTP_TTL,
            "qos-dscp", AES67::AUDIO_DSCP,
            "auto-multicast", TRUE,
            "sync", FALSE,
            NULL);
        if (!inst.interface.empty()) {
            g_object_set(udpsink, "multicast-iface", inst.interface.c_str(), NULL);
        }

        // Configure queue: small buffer for low-latency, leaky to avoid
        // blocking the main audio chain if the network stalls
        g_object_set(queue, "max-size-buffers", 3, "leaky", 2 /* downstream */, NULL);

        // Add elements to pipeline bin
        gst_bin_add_many(GST_BIN(pipeline), queue, aconv, capsf, rtppay, udpsink, NULL);

        // Link: queue → audioconvert → capsfilter → rtpL24pay → udpsink
        if (!gst_element_link_many(queue, aconv, capsf, rtppay, udpsink, NULL)) {
            LogErr(VB_MEDIAOUT, "AES67 zero-hop: Failed to link branch for instance %d\n", inst.id);
            // Elements are owned by bin now, cleanup happens when pipeline is destroyed
            continue;
        }

        // Sync element states to pipeline state
        gst_element_sync_state_with_parent(queue);
        gst_element_sync_state_with_parent(aconv);
        gst_element_sync_state_with_parent(capsf);
        gst_element_sync_state_with_parent(rtppay);
        gst_element_sync_state_with_parent(udpsink);

        // Request tee pad and link to queue
        GstPad* teeSrcPad = gst_element_request_pad_simple(tee, "src_%u");
        if (!teeSrcPad) {
            LogErr(VB_MEDIAOUT, "AES67 zero-hop: Failed to request tee src pad for instance %d\n", inst.id);
            continue;
        }
        GstPad* queueSinkPad = gst_element_get_static_pad(queue, "sink");

        if (gst_pad_link(teeSrcPad, queueSinkPad) != GST_PAD_LINK_OK) {
            LogErr(VB_MEDIAOUT, "AES67 zero-hop: Failed to link tee to queue for instance %d\n", inst.id);
            // Release the request pad back to the tee before dropping our
            // ref -- otherwise the tee is left with a pad slot that was
            // never returned via gst_element_release_request_pad().
            gst_element_release_request_pad(tee, teeSrcPad);
            gst_object_unref(teeSrcPad);
            gst_object_unref(queueSinkPad);
            continue;
        }
        gst_object_unref(queueSinkPad);

        // NOTE: Do NOT call gst_pipeline_use_clock() here.
        // The playback pipeline's own clock (e.g. system clock) must remain.
        // An unsynced PTP clock returns GST_CLOCK_TIME_NONE which crashes
        // the pipeline.  (No AES67 pipeline sets a PTP clock today --
        // CreateSendPipeline() does not either, despite what this note used
        // to claim.)

        InlineRTPBranch branch;
        branch.instanceId = inst.id;
        branch.queue = queue;
        branch.teeSrcPad = teeSrcPad;
        branches.push_back(branch);

        // NOTE: We do NOT pause the standalone send pipeline.  Pausing it
        // with pipewiresrc causes preroll issues when trying to resume
        // (PAUSED→PLAYING can hang if pipewiresrc can't re-acquire buffers).
        // Both streams (inline + standalone) will coexist briefly during
        // playback — they have different SSRCs so receivers pick one.

        LogInfo(VB_MEDIAOUT, "AES67 zero-hop: Branch active for '%s' → %s:%d (%dch, %dms)\n",
                inst.name.c_str(), inst.multicastIP.c_str(), inst.port,
                inst.channels, inst.ptime);
    }

    return branches;
}

void AES67Manager::DetachInlineRTPBranches(GstElement* pipeline,
                                            std::vector<InlineRTPBranch>& branches) {
    for (auto& branch : branches) {
        if (branch.teeSrcPad) {
            // Get the tee element from the pad's parent
            GstElement* tee = gst_pad_get_parent_element(branch.teeSrcPad);
            if (tee) {
                gst_element_release_request_pad(tee, branch.teeSrcPad);
                gst_object_unref(tee);
            }
            gst_object_unref(branch.teeSrcPad);
            branch.teeSrcPad = nullptr;
        }
        // The queue and downstream elements are owned by the pipeline bin
        // and will be destroyed when the pipeline is unreffed.
        branch.queue = nullptr;
    }
    branches.clear();
}

// ──────────────────────────────────────────────────────────────────────────────
// Status reporting — for PHP API
// ──────────────────────────────────────────────────────────────────────────────
// The rate the PipeWire graph is actually clocked at.  Mirrors the fallback in
// GStreamerOut.cpp's GetPipeWireGraphRate(), which is file-local there: the
// per-card file carries the rate the hardware really achieved and sorts after
// the defaults, so it wins wherever both exist.
static int PipeWireGraphRate() {
    static const char* confs[] = {
        "/etc/pipewire/pipewire.conf.d/95-fpp-alsa-sink.conf",
        "/etc/pipewire/pipewire.conf.d/90-fpp.conf"
    };
    for (const char* conf : confs) {
        std::string contents = GetFileContents(conf);
        size_t p = contents.find("default.clock.rate");
        if (p == std::string::npos)
            continue;
        p = contents.find('=', p);
        if (p == std::string::npos)
            continue;
        int rate = atoi(contents.c_str() + p + 1);
        if (rate > 0)
            return rate;
    }
    return 0;
}

// The rate pipewiresrc negotiated with the graph for this pipeline, or 0 if it
// has not negotiated yet.  Read from the live pad rather than from any config
// file: per-card and per-group rates sit between the graph clock and what this
// stream is actually fed, so default.clock.rate can say 44100 while the stream
// is getting a clean 48000 (or the reverse).
static int NegotiatedSourceRate(GstElement* pipeline) {
    if (!pipeline) {
        return 0;
    }
    GstElement* pwsrc = gst_bin_get_by_name(GST_BIN(pipeline), "pwsrc");
    if (!pwsrc) {
        return 0;
    }
    int rate = 0;
    GstPad* pad = gst_element_get_static_pad(pwsrc, "src");
    if (pad) {
        GstCaps* caps = gst_pad_get_current_caps(pad);
        if (caps) {
            const GstStructure* st = gst_caps_get_structure(caps, 0);
            if (st) {
                gst_structure_get_int(st, "rate", &rate);
            }
            gst_caps_unref(caps);
        }
        gst_object_unref(pad);
    }
    gst_object_unref(pwsrc);
    return rate;
}

AES67Manager::Status AES67Manager::GetStatus() {
    Status status;

    // Snapshot the config BEFORE taking the pipeline lock, and use the copy
    // from here on.  Reading m_config directly would race LoadConfig(), which
    // reallocates the instance vector on another thread -- and taking the two
    // locks together would create an ordering to get wrong.  See m_configMutex.
    AES67Config config = GetConfigSnapshot();
    status.ptpEnabled = config.ptpEnabled;
    status.ptpDomain = config.ptpDomain;
    status.ptpRole = config.ptpRole;
    status.graphSampleRate = PipeWireGraphRate();

    // Pipeline status — use try_lock to avoid blocking HTTP handlers
    // indefinitely if another thread holds m_pipelineMutex during a
    // long GStreamer state change.
    {
        std::unique_lock<std::mutex> lock(m_pipelineMutex, std::try_to_lock);
        if (lock.owns_lock()) {
            for (const auto& [id, p] : m_sendPipelines) {
                Status::PipelineStatus ps;
                ps.instanceId = id;
                ps.mode = "send";
                ps.running = p.running;
                ps.error = p.errorMessage;
                ps.sourceRate = NegotiatedSourceRate(p.pipeline);

                for (const auto& inst : config.instances) {
                    if (inst.id == id) {
                        ps.name = inst.name;
                        break;
                    }
                }
                status.pipelines.push_back(ps);
            }

            for (const auto& [id, p] : m_recvPipelines) {
                Status::PipelineStatus ps;
                ps.instanceId = id;
                ps.mode = "receive";
                ps.running = p.running;
                ps.error = p.errorMessage;

                for (const auto& inst : config.instances) {
                    if (inst.id == id) {
                        ps.name = inst.name;
                        break;
                    }
                }
                status.pipelines.push_back(ps);
            }
        } else {
            // Could not acquire lock — return partial status
            Status::PipelineStatus ps;
            ps.instanceId = -1;
            ps.name = "(status unavailable — pipeline operation in progress)";
            ps.mode = "unknown";
            ps.running = false;
            status.pipelines.push_back(ps);
        }
    }
    // PTP status — query ptp4l for the actual selected grandmaster (may be a
    // remote/upstream clock) rather than assuming this node is the master.
    {
        bool gmPresent = false;
        std::string gmId;
        int64_t offsetNs = 0;

        status.ptpPortState = IsPtp4lRunning() ? GetPtp4lState() : "not running";
        status.ptpIsGrandmaster = IsGrandmasterPortState(status.ptpPortState);

        if (QueryPtp4lTimeStatus(gmPresent, gmId, offsetNs) && gmPresent && !gmId.empty()) {
            status.ptpSynced = true;
            status.ptpGrandmasterId = gmId;
            status.ptpOffsetNs = offsetNs;
        } else if (status.ptpIsGrandmaster) {
            // We won the BMCA and ARE the domain grandmaster.  pmc reports
            // gmPresent=false in that case because there is no *remote* GM to
            // report -- which is not the same thing as "unsynced".  Being the
            // clock source is a legitimate AES67 configuration (FPP driving
            // the show clock), so report ourselves as the grandmaster with a
            // zero offset rather than showing the user a broken PTP status.
            status.ptpSynced = true;
            status.ptpGrandmasterId = GetPTPClockId();
            status.ptpOffsetNs = 0;
        } else {
            // ptp4l not running, or no grandmaster selected yet (e.g. still
            // in LISTENING/PRE_MASTER) — do not claim we're synced or report
            // our own identity as if it were the grandmaster.
            status.ptpSynced = false;
            status.ptpGrandmasterId = "";
            status.ptpOffsetNs = 0;
        }
    }

    // Discovered streams
    {
        std::lock_guard<std::mutex> lock(m_discoveredMutex);
        for (const auto& [hash, stream] : m_discoveredStreams) {
            status.discoveredStreams.push_back(stream);
        }
    }

    return status;
}

// ──────────────────────────────────────────────────────────────────────────────
// Self-test — validates AES67 subsystem components (7.10)
// ──────────────────────────────────────────────────────────────────────────────
std::vector<AES67Manager::TestResult> AES67Manager::RunSelfTest() {
    // Snapshot once -- this runs on an HTTP/command thread and would otherwise
    // read m_config while ApplyConfig() reloads it.  See m_configMutex.
    AES67Config config = GetConfigSnapshot();
    std::vector<TestResult> results;

    // Test 1: GStreamer initialization
    {
        TestResult r;
        r.testName = "gstreamer_init";
        r.passed = gst_is_initialized();
        r.message = r.passed ? "GStreamer is initialized" : "GStreamer is NOT initialized";
        results.push_back(r);
    }

    // Test 2: Required GStreamer elements available
    {
        const char* requiredElements[] = {
            "rtpL24pay", "rtpL24depay", "rtpjitterbuffer",
            "udpsrc", "udpsink", "audioconvert", "audioresample",
            "pipewiresrc", "pipewiresink", nullptr
        };
        for (int i = 0; requiredElements[i]; i++) {
            TestResult r;
            r.testName = std::string("element_") + requiredElements[i];
            GstElementFactory* factory = gst_element_factory_find(requiredElements[i]);
            r.passed = (factory != nullptr);
            r.message = r.passed
                ? std::string(requiredElements[i]) + " element available"
                : std::string(requiredElements[i]) + " element NOT FOUND";
            if (factory) gst_object_unref(factory);
            results.push_back(r);
        }
    }

    // Test 3: PTP daemon (ptp4l)
    {
        TestResult r;
        r.testName = "ptp_initialized";
        r.passed = m_ptpInitialized;
        r.message = r.passed ? "PTP subsystem initialized" : "PTP subsystem not initialized";
        results.push_back(r);
    }
    {
        TestResult r;
        r.testName = "ptp4l_running";
        r.passed = IsPtp4lRunning();
        std::string state = GetPtp4lState();
        r.message = r.passed
            ? "ptp4l is running (PID " + std::to_string(m_ptp4lPid) + ") — " + state
            : "ptp4l is NOT running";
        results.push_back(r);
    }
    {
        TestResult r;
        r.testName = "ptp4l_binary";
        r.passed = FileExists("/usr/sbin/ptp4l");
        r.message = r.passed ? "ptp4l binary found at /usr/sbin/ptp4l" : "ptp4l binary NOT found — install linuxptp package";
        results.push_back(r);
    }
    {
        // Without pmc every grandmaster query fails, and PTP silently reports
        // "not synced" no matter how well it is actually locked.
        TestResult r;
        r.testName = "pmc_binary";
        r.passed = FileExists(PMC_BINARY);
        r.message = r.passed
            ? std::string("pmc binary found at ") + PMC_BINARY
            : std::string("pmc binary NOT found at ") + PMC_BINARY +
              " — grandmaster status cannot be queried (install linuxptp)";
        results.push_back(r);
    }
    {
        // Surfaces the two settings behind "why did my Pi become the master?"
        // and "why does the Q-SYS core not see us?".
        TestResult r;
        r.testName = "ptp_grandmaster";
        std::string gm = GetActiveGrandmasterId();
        std::string state = IsPtp4lRunning() ? GetPtp4lState() : "not running";
        r.passed = !config.ptpEnabled || !gm.empty();
        if (!config.ptpEnabled) {
            r.message = "PTP is disabled — SDP advertises this node's own clock identity";
        } else if (gm.empty()) {
            r.message = "No grandmaster selected yet (port state " + state + ") — domain " +
                        std::to_string(config.ptpDomain) + ", role " + config.ptpRole;
        } else {
            r.message = "Grandmaster " + gm + " (port state " + state + ") — domain " +
                        std::to_string(config.ptpDomain) + ", role " + config.ptpRole +
                        (IsGrandmasterPortState(state) ? " — this node holds the role" : "");
        }
        results.push_back(r);
    }

    // Test 4: Config file
    {
        TestResult r;
        r.testName = "config_file";
        std::ifstream test(m_configPath);
        r.passed = test.good();
        r.message = r.passed
            ? "Config file found: " + m_configPath
            : "Config file missing: " + m_configPath;
        results.push_back(r);
    }

    // Test 5: Config loaded and has instances
    {
        TestResult r;
        r.testName = "config_instances";
        r.passed = !config.instances.empty();
        r.message = r.passed
            ? std::to_string(config.instances.size()) + " instance(s) configured"
            : "No instances configured";
        results.push_back(r);
    }

    // Test 6: Network interface availability
    {
        TestResult r;
        r.testName = "network_interface";
        std::string ip = GetInterfaceIP(config.ptpInterface);
        r.passed = !ip.empty();
        r.message = r.passed
            ? "Interface " + config.ptpInterface + " has IP: " + ip
            : "Interface " + config.ptpInterface + " not found or has no IP";
        results.push_back(r);
    }

    // Test 7: PTP clock ID derivation (EUI-64 from MAC)
    {
        TestResult r;
        r.testName = "ptp_clock_id";
        std::string clockId = GetPTPClockId();
        r.passed = !clockId.empty() && clockId.length() == 23; // XX-XX-XX-XX-XX-XX-XX-XX
        r.message = r.passed
            ? "PTP Clock ID: " + clockId
            : "Could not derive PTP Clock ID from MAC address";
        results.push_back(r);
    }

    // Test 8: Send pipeline status
    {
        std::lock_guard<std::mutex> lock(m_pipelineMutex);
        for (const auto& [id, p] : m_sendPipelines) {
            TestResult r;
            r.testName = "send_pipeline_" + std::to_string(id);
            r.passed = p.running;
            r.message = p.running
                ? "Send pipeline " + std::to_string(id) + " is running"
                : "Send pipeline " + std::to_string(id) + " is NOT running: " + p.errorMessage;
            results.push_back(r);
        }

        for (const auto& [id, p] : m_recvPipelines) {
            TestResult r;
            r.testName = "recv_pipeline_" + std::to_string(id);
            r.passed = p.running;
            r.message = p.running
                ? "Receive pipeline " + std::to_string(id) + " is running"
                : "Receive pipeline " + std::to_string(id) + " is NOT running: " + p.errorMessage;
            results.push_back(r);
        }
    }

    // Test 9: SAP announcer running
    {
        TestResult r;
        r.testName = "sap_announcer";
        r.passed = m_sapAnnounceRunning.load();
        r.message = r.passed ? "SAP announcer thread running" : "SAP announcer thread not running";
        results.push_back(r);
    }

    // Test 10: SAP receiver running
    {
        TestResult r;
        r.testName = "sap_receiver";
        r.passed = m_sapRecvRunning.load();
        r.message = r.passed ? "SAP receiver thread running" : "SAP receiver thread not running";
        results.push_back(r);
    }

    // Test 11: Multicast socket test — try binding to SAP port (quick check)
    {
        TestResult r;
        r.testName = "multicast_capability";
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock >= 0) {
            int reuse = 1;
            setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
            struct sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = 0;  // any port
            addr.sin_addr.s_addr = INADDR_ANY;
            r.passed = (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0);
            r.message = r.passed ? "UDP socket creation and bind OK" : "Failed to create/bind UDP socket";
            close(sock);
        } else {
            r.passed = false;
            r.message = "Failed to create UDP socket";
        }
        results.push_back(r);
    }

    // Test 12: SDP generation test — verify we can build a valid SDP
    {
        TestResult r;
        r.testName = "sdp_generation";
        if (!config.instances.empty()) {
            std::string sourceIP = GetInterfaceIP(config.ptpInterface);
            std::string clockId = GetPTPClockId();
            // Version 0: this is a rendering for the test output, not an
            // announcement, and must not advance the announced version.
            std::string sdp = BuildSDP(config.instances[0], sourceIP, clockId, 0);
            r.passed = !sdp.empty() && sdp.find("v=0") != std::string::npos
                       && sdp.find("ts-refclk") != std::string::npos
                       && sdp.find("mediaclk") != std::string::npos
                       && sdp.find("L24") != std::string::npos;
            r.message = r.passed
                ? "SDP generation OK (" + std::to_string(sdp.size()) + " bytes)"
                : "SDP generation failed or incomplete";
        } else {
            r.passed = false;
            r.message = "No instances configured — cannot test SDP generation";
        }
        results.push_back(r);
    }

    return results;
}

// ──────────────────────────────────────────────────────────────────────────────
// HTTP API endpoint — registered at /aes67
// ──────────────────────────────────────────────────────────────────────────────
HttpResponsePtr AES67Manager::render_GET(const HttpRequestPtr& req) {

    std::string url(req->path());

    // Strip leading /aes67/
    if (url.find("/aes67/") == 0) {
        url = url.substr(7);
    } else if (url == "/aes67") {
        url = "status";
    }

    if (url == "status") {
        Status st = GetStatus();
        Json::Value result;

        // Pipelines
        Json::Value pipelines(Json::arrayValue);
        for (const auto& p : st.pipelines) {
            Json::Value pj;
            pj["instanceId"] = p.instanceId;
            pj["name"] = p.name;
            pj["mode"] = p.mode;
            pj["running"] = p.running;
            if (p.sourceRate > 0) {
                pj["sourceRate"] = p.sourceRate;
            }
            if (!p.error.empty()) {
                pj["error"] = p.error;
            }
            pipelines.append(pj);
        }
        result["pipelines"] = pipelines;

        // PTP
        Json::Value ptp;
        ptp["synced"] = st.ptpSynced;
        ptp["offsetNs"] = (Json::Int64)st.ptpOffsetNs;
        ptp["grandmasterId"] = st.ptpGrandmasterId;
        ptp["portState"] = st.ptpPortState;
        ptp["isGrandmaster"] = st.ptpIsGrandmaster;
        ptp["enabled"] = st.ptpEnabled;
        ptp["domain"] = st.ptpDomain;
        ptp["role"] = st.ptpRole;
        result["ptp"] = ptp;

        // Discovered streams
        Json::Value discovered(Json::arrayValue);
        for (const auto& s : st.discoveredStreams) {
            Json::Value sj;
            sj["sessionName"] = s.sessionName;
            sj["originAddress"] = s.originAddress;
            sj["multicastIP"] = s.multicastIP;
            sj["port"] = s.port;
            sj["channels"] = s.channels;
            sj["ptime"] = s.ptime;
            sj["ptpClockId"] = s.ptpClockId;
            discovered.append(sj);
        }
        result["discoveredStreams"] = discovered;

        result["graphSampleRate"] = st.graphSampleRate;
        result["active"] = m_active.load();

        Json::StreamWriterBuilder wbuilder;
        wbuilder["indentation"] = "";
        std::string resultStr = Json::writeString(wbuilder, result);

        return makeStringResponse(resultStr, 200, "application/json");
    }

    if (url == "test") {
        auto tests = RunSelfTest();
        Json::Value result;
        Json::Value testArray(Json::arrayValue);
        int passed = 0, failed = 0;

        for (const auto& t : tests) {
            Json::Value tj;
            tj["test"] = t.testName;
            tj["passed"] = t.passed;
            tj["message"] = t.message;
            testArray.append(tj);
            if (t.passed) passed++; else failed++;
        }

        result["tests"] = testArray;
        result["summary"]["total"] = (int)tests.size();
        result["summary"]["passed"] = passed;
        result["summary"]["failed"] = failed;
        result["summary"]["allPassed"] = (failed == 0);

        Json::StreamWriterBuilder wbuilder;
        wbuilder["indentation"] = "  ";
        std::string resultStr = Json::writeString(wbuilder, result);

        return makeStringResponse(resultStr, 200, "application/json");
    }

    return makeStringResponse("{\"error\":\"unknown endpoint\"}", 404, "application/json");
}

#endif // HAS_AES67_GSTREAMER
