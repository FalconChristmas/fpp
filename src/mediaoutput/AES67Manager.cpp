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

#include "AES67Manager.h"

#ifdef HAS_AES67_GSTREAMER

#include <gst/gst.h>

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
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

    // Stop SAP threads
    m_sapAnnounceRunning.store(false);
    m_sapRecvRunning.store(false);
    if (m_sapAnnounceThread.joinable()) {
        m_sapAnnounceThread.join();
    }
    if (m_sapRecvThread.joinable()) {
        m_sapRecvThread.join();
    }

    StopAllPipelines();
    ShutdownPTP();

    m_active.store(false);
    m_initialized.store(false);
    LogInfo(VB_MEDIAOUT, "AES67Manager: Shutdown complete\n");
}

// ──────────────────────────────────────────────────────────────────────────────
// Config loading — reads pipewire-aes67-instances.json
// ──────────────────────────────────────────────────────────────────────────────
bool AES67Manager::LoadConfig() {
    Json::Value root;
    if (!LoadJsonFromFile(m_configPath, root)) {
        LogWarn(VB_MEDIAOUT, "AES67Manager: Failed to load config from %s\n",
                m_configPath.c_str());
        return false;
    }

    m_config.instances.clear();
    m_config.ptpEnabled = root.get("ptpEnabled", true).asBool();
    m_config.ptpInterface = root.get("ptpInterface", "eth0").asString();

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

            m_config.instances.push_back(inst);
        }
    }

    LogInfo(VB_MEDIAOUT, "AES67Manager: Loaded config with %d instances, PTP=%s interface=%s\n",
            (int)m_config.instances.size(),
            m_config.ptpEnabled ? "enabled" : "disabled",
            m_config.ptpInterface.c_str());
    return true;
}

// ──────────────────────────────────────────────────────────────────────────────
// ApplyConfig — called from PHP API and boot sequence
// ──────────────────────────────────────────────────────────────────────────────
bool AES67Manager::ApplyConfig() {
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
    if (m_sapAnnounceThread.joinable()) {
        m_sapAnnounceThread.join();
    }
    if (m_sapRecvThread.joinable()) {
        m_sapRecvThread.join();
    }
    StopAllPipelines();
    ShutdownPTP();

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

    m_sapAnnounceRunning.store(false);
    m_sapRecvRunning.store(false);
    if (m_sapAnnounceThread.joinable()) {
        m_sapAnnounceThread.join();
    }
    if (m_sapRecvThread.joinable()) {
        m_sapRecvThread.join();
    }
    StopAllPipelines();
    ShutdownPTP();

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
//   - Domain 0, two-step, 8 announces/sec, 8 syncs/sec
//   - Hardware timestamping when available (/dev/ptp0)
//   - priority1=128 (default BMCA priority)
//
// phc2sys is also launched to synchronize the system clock to the PTP
// hardware clock, so that GStreamer pipelines (which use the system clock)
// stay in sync with PTP time.
// ──────────────────────────────────────────────────────────────────────────────
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

    // Write AES67-profile PTP config to a temp file
    m_ptpConfPath = "/tmp/fpp-ptp4l.conf";
    {
        std::ofstream conf(m_ptpConfPath);
        if (!conf.is_open()) {
            LogErr(VB_MEDIAOUT, "AES67Manager: Cannot write PTP config to %s\n",
                   m_ptpConfPath.c_str());
            WarningHolder::AddWarning(45, "AES67: could not write PTP configuration file");
            return false;
        }
        // AES67 PTP profile: domain 0, two-step, high announce/sync rate
        conf << "[global]\n"
             << "domainNumber\t\t0\n"
             << "twoStepFlag\t\t1\n"
             << "priority1\t\t128\n"
             << "priority2\t\t128\n"
             << "clockClass\t\t248\n"
             << "clockAccuracy\t\t0xFE\n"
             << "offsetScaledLogVariance\t0xFFFF\n"
             << "logAnnounceInterval\t-3\n"    // 8/sec (AES67 recommends -3)
             << "logSyncInterval\t\t-3\n"      // 8/sec (AES67 recommends -3)
             << "logMinDelayReqInterval\t-3\n"
             << "announceReceiptTimeout\t3\n"
             << "syncReceiptTimeout\t0\n"
             << "transportSpecific\t0x0\n"
             << "network_transport\tUDPv4\n"
             << "delay_mechanism\t\tE2E\n"
             << "time_stamping\t\thardware\n"
             << "# AES67 uses L2 multicast on 224.0.1.129/224.0.0.107\n";
        conf.close();
    }

    LogInfo(VB_MEDIAOUT, "AES67Manager: Starting ptp4l on %s (AES67 profile, domain 0)\n",
            m_config.ptpInterface.c_str());

    // Fork and exec ptp4l
    pid_t pid = fork();
    if (pid < 0) {
        LogErr(VB_MEDIAOUT, "AES67Manager: fork() failed for ptp4l: %s\n", strerror(errno));
        WarningHolder::AddWarning(45, "AES67: could not start the ptp4l clock-sync process");
        return false;
    }
    if (pid == 0) {
        // Child process — exec ptp4l
        // ptp4l -i eth0 -f /tmp/fpp-ptp4l.conf -m (log to stderr)
        execlp("ptp4l", "ptp4l",
               "-i", m_config.ptpInterface.c_str(),
               "-f", m_ptpConfPath.c_str(),
               "-m",      // log to stderr
               nullptr);
        // If exec fails
        _exit(127);
    }
    m_ptp4lPid = pid;
    LogInfo(VB_MEDIAOUT, "AES67Manager: ptp4l started (PID %d) on %s\n",
            (int)m_ptp4lPid, m_config.ptpInterface.c_str());

    // Give ptp4l a moment to start
    usleep(500000);  // 500ms

    // Check if it's still running (might have failed immediately)
    if (!IsPtp4lRunning()) {
        LogErr(VB_MEDIAOUT, "AES67Manager: ptp4l exited immediately — "
               "check hardware timestamping support on %s\n",
               m_config.ptpInterface.c_str());

        // Try again with software timestamping
        LogInfo(VB_MEDIAOUT, "AES67Manager: Retrying ptp4l with software timestamping\n");
        {
            std::ofstream conf(m_ptpConfPath);
            if (conf.is_open()) {
                conf << "[global]\n"
                     << "domainNumber\t\t0\n"
                     << "twoStepFlag\t\t1\n"
                     << "priority1\t\t128\n"
                     << "priority2\t\t128\n"
                     << "clockClass\t\t248\n"
                     << "clockAccuracy\t\t0xFE\n"
                     << "offsetScaledLogVariance\t0xFFFF\n"
                     << "logAnnounceInterval\t-3\n"
                     << "logSyncInterval\t\t-3\n"
                     << "logMinDelayReqInterval\t-3\n"
                     << "announceReceiptTimeout\t3\n"
                     << "syncReceiptTimeout\t0\n"
                     << "transportSpecific\t0x0\n"
                     << "network_transport\tUDPv4\n"
                     << "delay_mechanism\t\tE2E\n"
                     << "time_stamping\t\tsoftware\n";
                conf.close();
            }
        }

        pid = fork();
        if (pid < 0) {
            LogErr(VB_MEDIAOUT, "AES67Manager: fork() failed for ptp4l retry: %s\n", strerror(errno));
            return false;
        }
        if (pid == 0) {
            execlp("ptp4l", "ptp4l",
                   "-i", m_config.ptpInterface.c_str(),
                   "-f", m_ptpConfPath.c_str(),
                   "-m",
                   nullptr);
            _exit(127);
        }
        m_ptp4lPid = pid;
        LogInfo(VB_MEDIAOUT, "AES67Manager: ptp4l retry started (PID %d) software timestamping\n",
                (int)m_ptp4lPid);

        usleep(500000);
        if (!IsPtp4lRunning()) {
            LogErr(VB_MEDIAOUT, "AES67Manager: ptp4l failed even with software timestamping\n");
            WarningHolder::AddWarning(45, "AES67: PTP clock sync (ptp4l) could not start on the configured interface");
            m_ptp4lPid = -1;
            return false;
        }
    }

    // Start phc2sys to synchronize system clock to PTP hardware clock
    // Only needed with hardware timestamping
    if (FileExists("/dev/ptp0") && FileExists("/usr/sbin/phc2sys")) {
        pid_t phcPid = fork();
        if (phcPid < 0) {
            LogWarn(VB_MEDIAOUT, "AES67Manager: fork() failed for phc2sys: %s\n", strerror(errno));
        } else if (phcPid == 0) {
            // phc2sys -s /dev/ptp0 -c CLOCK_REALTIME -O 0 -m
            // -s: source clock (PTP hardware clock)
            // -c: target clock (system)  
            // -O 0: zero UTC offset
            // -m: log to stderr
            execlp("phc2sys", "phc2sys",
                   "-s", "/dev/ptp0",
                   "-c", "CLOCK_REALTIME",
                   "-O", "0",
                   "-m",
                   nullptr);
            _exit(127);
        } else {
            m_phc2sysPid = phcPid;
            LogInfo(VB_MEDIAOUT, "AES67Manager: phc2sys started (PID %d)\n", (int)m_phc2sysPid);
        }
    }

    m_ptpInitialized = true;
    LogInfo(VB_MEDIAOUT, "AES67Manager: PTP initialized — ptp4l PID %d on %s\n",
            (int)m_ptp4lPid, m_config.ptpInterface.c_str());
    return true;
}

void AES67Manager::ShutdownPTP() {
    if (m_phc2sysPid > 0) {
        LogInfo(VB_MEDIAOUT, "AES67Manager: Stopping phc2sys (PID %d)\n", (int)m_phc2sysPid);
        kill(m_phc2sysPid, SIGTERM);
        int status;
        waitpid(m_phc2sysPid, &status, 0);
        m_phc2sysPid = -1;
    }
    if (m_ptp4lPid > 0) {
        LogInfo(VB_MEDIAOUT, "AES67Manager: Stopping ptp4l (PID %d)\n", (int)m_ptp4lPid);
        kill(m_ptp4lPid, SIGTERM);
        int status;
        waitpid(m_ptp4lPid, &status, 0);
        m_ptp4lPid = -1;
    }
    if (!m_ptpConfPath.empty()) {
        unlink(m_ptpConfPath.c_str());
        m_ptpConfPath.clear();
    }
    m_ptpInitialized = false;
}

bool AES67Manager::IsPtp4lRunning() const {
    if (m_ptp4lPid <= 0) return false;
    // kill(pid, 0) checks if process exists without sending a signal
    return (kill(m_ptp4lPid, 0) == 0);
}

std::string AES67Manager::GetPtp4lState() {
    if (!IsPtp4lRunning()) {
        return "not running";
    }
    // Quick check via /proc — if ptp4l is in the process list, it's active.
    // Full pmc (PTP management client) queries could be added later for
    // detailed state (MASTER, SLAVE, LISTENING, etc.)
    return "running";
}

std::string AES67Manager::GetPTPClockId() {
    // Derive EUI-64 clock ID from interface MAC address
    // Read /sys/class/net/<iface>/address → AA:BB:CC:DD:EE:FF
    // Insert FF:FE → AA-BB-CC-FF-FE-DD-EE-FF
    std::string macPath = "/sys/class/net/" + m_config.ptpInterface + "/address";
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
// Pipeline creation — Send
// ──────────────────────────────────────────────────────────────────────────────
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
    //   ! audioconvert ! audio/x-raw,format=S24BE,rate=48000,channels=N
    //   ! rtpL24pay pt=96 min-ptime=<ns> max-ptime=<ns>
    //   ! application/x-rtp,clock-rate=48000
    //   ! udpsink host=<multicast> port=<port> multicast-iface=<iface> ttl-mc=4 auto-multicast=true sync=true

    std::ostringstream oss;
    oss << "pipewiresrc name=pwsrc min-buffers=2 "
        << "! audioconvert "
        << "! audio/x-raw,format=S24BE,rate=" << AES67::AUDIO_RATE
        << ",channels=" << inst.channels << " "
        << "! rtpL24pay pt=" << AES67::RTP_PAYLOAD_TYPE
        << " min-ptime=" << ptimeNs
        << " max-ptime=" << ptimeNs << " "
        << "! application/x-rtp,clock-rate=" << AES67::AUDIO_RATE << " "
        << "! udpsink name=usink host=" << inst.multicastIP
        << " port=" << inst.port
        << " ttl-mc=" << AES67::AUDIO_RTP_TTL
        << " auto-multicast=true sync=false";

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
        return false;
    }

    // Set stream-properties post-launch — inline GstStructure values in
    // gst_parse_launch can crash gst_value_deserialize on some platforms.
    GstElement* pwsrc = gst_bin_get_by_name(GST_BIN(pipeline), "pwsrc");
    if (pwsrc) {
        GstStructure* props = gst_structure_new("props",
            "node.name", G_TYPE_STRING, nodeName.c_str(),
            "node.autoconnect", G_TYPE_BOOLEAN, FALSE,
            NULL);
        g_object_set(pwsrc, "stream-properties", props, NULL);
        gst_structure_free(props);
        gst_object_unref(pwsrc);
    }

    // Note: The pipeline uses the system clock.  PTP synchronization is
    // handled externally by ptp4l + phc2sys, which keeps the system clock
    // aligned with PTP time.  We do NOT call gst_pipeline_use_clock() here.

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
        gst_object_unref(bus);
        gst_object_unref(pipeline);
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_pipelineMutex);
        m_sendPipelines.erase(inst.id);
        auto [it, ok] = m_sendPipelines.try_emplace(inst.id);
        it->second.instanceId = inst.id;
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

    // Note: PTP synchronization is handled externally by ptp4l + phc2sys.
    // The system clock is kept in sync with PTP, so the default pipeline
    // clock (system clock) inherits PTP accuracy.

    // Start pipeline OUTSIDE the mutex — GStreamer state changes can block
    // waiting for PipeWire.
    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        LogErr(VB_MEDIAOUT, "AES67 recv pipeline [%d] failed to start\n", inst.id);
        WarningHolder::AddWarning(44, "AES67: audio receive stream failed to start");
        gst_object_unref(bus);
        gst_object_unref(pipeline);
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_pipelineMutex);
        m_recvPipelines.erase(inst.id);
        auto [it, ok] = m_recvPipelines.try_emplace(inst.id);
        it->second.instanceId = inst.id;
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
uint16_t AES67Manager::ComputeSAPHash(const AES67Instance& inst) {
    // Stable hash from "multicastIP:port:name" — matches fpp_aes67_sap
    std::string key = inst.multicastIP + ":" + std::to_string(inst.port) + ":" + inst.name;

    // Simple FNV-1a hash truncated to 16 bits
    uint32_t hash = 2166136261u;
    for (char c : key) {
        hash ^= (uint32_t)c;
        hash *= 16777619u;
    }
    return (uint16_t)(hash & 0xFFFF);
}

std::string AES67Manager::BuildSDP(const AES67Instance& inst,
                                    const std::string& sourceIP,
                                    const std::string& ptpClockId) {
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

    std::ostringstream sdp;
    sdp << "v=0\r\n"
        << "o=- " << sessionId << " " << sessionId << " IN IP4 " << sourceIP << "\r\n"
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
        LogErr(VB_MEDIAOUT, "AES67 SAP: Failed to create socket: %s\n", strerror(errno));
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

    std::string ptpClockId = GetPTPClockId();

    // Build all SAP packets for send instances
    struct SAPEntry {
        uint16_t hash;
        std::vector<uint8_t> announcePacket;
        std::vector<uint8_t> deletePacket;
    };
    std::vector<SAPEntry> entries;

    for (const auto& inst : m_config.instances) {
        if (!inst.enabled) continue;
        if (!inst.sapEnabled) continue;
        if (inst.mode != "send" && inst.mode != "both") continue;

        std::string sourceIP = GetInterfaceIP(inst.interface.empty() ?
                                              m_config.ptpInterface : inst.interface);
        uint16_t hash = ComputeSAPHash(inst);
        std::string sdp = BuildSDP(inst, sourceIP, ptpClockId);

        SAPEntry entry;
        entry.hash = hash;
        entry.announcePacket = BuildSAPPacket(sourceIP, hash, sdp, false);
        entry.deletePacket = BuildSAPPacket(sourceIP, hash, sdp, true);
        entries.push_back(entry);
    }

    if (entries.empty()) {
        LogWarn(VB_MEDIAOUT, "AES67 SAP: No SAP-enabled send instances — announcer has nothing to send\n");
    } else {
        LogInfo(VB_MEDIAOUT, "AES67 SAP: Announcing %d stream(s) to %s:%d every %ds\n",
                (int)entries.size(), AES67::SAP_MCAST_ADDRESS, AES67::SAP_PORT,
                AES67::SAP_ANNOUNCE_INTERVAL_S);
    }

    // Announce loop
    while (m_sapAnnounceRunning.load()) {
        for (const auto& entry : entries) {
            ssize_t sent = sendto(sock, entry.announcePacket.data(), entry.announcePacket.size(), 0,
                   (struct sockaddr*)&sapAddr, sizeof(sapAddr));
            if (sent < 0) {
                LogErr(VB_MEDIAOUT, "AES67 SAP: sendto failed: %s\n", strerror(errno));
            }
        }

        // Sleep for SAP_ANNOUNCE_INTERVAL_S, checking shutdown flag every second
        for (int i = 0; i < AES67::SAP_ANNOUNCE_INTERVAL_S && m_sapAnnounceRunning.load(); i++) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        // Poll pipeline bus messages and recover any crashed pipelines.
        // Runs every SAP_ANNOUNCE_INTERVAL_S (30s) — fast enough to detect
        // silent failures without adding significant overhead.
        if (m_sapAnnounceRunning.load()) {
            if (PollPipelinesWatchdog()) {
                // A full pipeline rebuild is needed.  We cannot call
                // ApplyConfig() from this thread because ApplyConfig()
                // joins m_sapAnnounceThread — which IS this thread — causing
                // a deadlock ("Resource deadlock avoided" / SIGABRT).
                // Instead, stop this loop and spawn a short-lived thread
                // that calls ApplyConfig() after we exit.
                LogWarn(VB_MEDIAOUT, "AES67 watchdog: rebuild needed, stopping SAP loop and scheduling ApplyConfig\n");
                m_sapAnnounceRunning.store(false);
                std::thread([this]() {
                    // Brief delay to let the SAP announce thread finish
                    // exiting and become joinable.
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    ApplyConfig();
                }).detach();
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
        LogErr(VB_MEDIAOUT, "AES67 SAP recv: Failed to create socket: %s\n", strerror(errno));
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
        LogErr(VB_MEDIAOUT, "AES67 SAP recv: bind failed: %s\n", strerror(errno));
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
        LogWarn(VB_MEDIAOUT, "AES67 SAP recv: join multicast failed: %s\n", strerror(errno));
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
        GstPad* queueSinkPad = gst_element_get_static_pad(queue, "sink");

        if (gst_pad_link(teeSrcPad, queueSinkPad) != GST_PAD_LINK_OK) {
            LogErr(VB_MEDIAOUT, "AES67 zero-hop: Failed to link tee to queue for instance %d\n", inst.id);
            gst_object_unref(teeSrcPad);
            gst_object_unref(queueSinkPad);
            continue;
        }
        gst_object_unref(queueSinkPad);

        // NOTE: Do NOT call gst_pipeline_use_clock() here.
        // The playback pipeline's own clock (e.g. system clock) must remain.
        // An unsynced PTP clock returns GST_CLOCK_TIME_NONE which crashes
        // the pipeline.  PTP clock is only set on standalone AES67 send
        // pipelines via CreateSendPipeline().

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
AES67Manager::Status AES67Manager::GetStatus() {
    Status status;

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

                for (const auto& inst : m_config.instances) {
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

                for (const auto& inst : m_config.instances) {
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
    // PTP status — check if ptp4l is running
    status.ptpSynced = IsPtp4lRunning();
    status.ptpGrandmasterId = GetPTPClockId();
    status.ptpOffsetNs = 0;  // detailed offset from pmc could be added later

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
        r.passed = !m_config.instances.empty();
        r.message = r.passed
            ? std::to_string(m_config.instances.size()) + " instance(s) configured"
            : "No instances configured";
        results.push_back(r);
    }

    // Test 6: Network interface availability
    {
        TestResult r;
        r.testName = "network_interface";
        std::string ip = GetInterfaceIP(m_config.ptpInterface);
        r.passed = !ip.empty();
        r.message = r.passed
            ? "Interface " + m_config.ptpInterface + " has IP: " + ip
            : "Interface " + m_config.ptpInterface + " not found or has no IP";
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
        if (!m_config.instances.empty()) {
            std::string sourceIP = GetInterfaceIP(m_config.ptpInterface);
            std::string clockId = GetPTPClockId();
            std::string sdp = BuildSDP(m_config.instances[0], sourceIP, clockId);
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
