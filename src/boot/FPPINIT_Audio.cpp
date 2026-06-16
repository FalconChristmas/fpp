/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2024 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include <algorithm>
#include <chrono>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "FPPINIT.h"

// Resolve ALSA card number to stable card ID (e.g., 3 -> "S3", 0 -> "ICUSBAUDIO7D")
// Reads /proc/asound/cards: " 3 [S3             ]: USB-Audio - ..."
// Falls back to the card number as string if not found.
// Read the USB product name for an ALSA card from sysfs (e.g. "USB Sound Device").
// Falls back to the ALSA card type name if sysfs is unavailable.
static std::string getAlsaCardProductName(int cardNum, const std::string& fallback) {
    std::string devicePath = "/sys/class/sound/card" + std::to_string(cardNum) + "/device";
    char resolved[PATH_MAX];
    if (realpath(devicePath.c_str(), resolved)) {
        // The ALSA device sysfs node points to a USB interface.
        // The USB product string is in the parent USB device node.
        std::string parentDir = resolved;
        auto pos = parentDir.rfind('/');
        if (pos != std::string::npos) {
            std::string productFile = parentDir.substr(0, pos) + "/product";
            std::string product = GetFileContents(productFile);
            TrimWhiteSpace(product);
            if (!product.empty()) {
                return product;
            }
        }
    }
    return fallback;
}

static std::string getAlsaCardId(int cardNum) {
    std::string cardsContent = GetFileContents("/proc/asound/cards");
    if (!cardsContent.empty()) {
        std::istringstream iss(cardsContent);
        std::string line;
        while (std::getline(iss, line)) {
            // Match: " 3 [S3             ]: ..."
            auto bracket = line.find('[');
            auto closeBracket = line.find(']');
            if (bracket != std::string::npos && closeBracket != std::string::npos && closeBracket > bracket) {
                std::string numStr = line.substr(0, bracket);
                TrimWhiteSpace(numStr);
                try {
                    int num = std::stoi(numStr);
                    if (num == cardNum) {
                        std::string cardId = line.substr(bracket + 1, closeBracket - bracket - 1);
                        TrimWhiteSpace(cardId);
                        if (!cardId.empty()) {
                            return cardId;
                        }
                    }
                } catch (...) {}
            }
        }
    }
    return std::to_string(cardNum);
}

// Inverse of getAlsaCardId: resolve a stable ALSA card ID (e.g. "S3",
// "bcm2835ALSA") to its current card number by scanning /proc/asound/cards.
// Returns -1 if no currently-present card matches that ID.
static int getAlsaCardNumForId(const std::string& cardId) {
    if (cardId.empty()) {
        return -1;
    }
    std::istringstream iss(GetFileContents("/proc/asound/cards"));
    std::string line;
    while (std::getline(iss, line)) {
        auto bracket = line.find('[');
        auto closeBracket = line.find(']');
        if (bracket != std::string::npos && closeBracket != std::string::npos && closeBracket > bracket) {
            std::string id = line.substr(bracket + 1, closeBracket - bracket - 1);
            TrimWhiteSpace(id);
            if (id == cardId) {
                std::string numStr = line.substr(0, bracket);
                TrimWhiteSpace(numStr);
                try {
                    return std::stoi(numStr);
                } catch (...) {}
            }
        }
    }
    return -1;
}

// Returns true if every ALSA card referenced by the PipeWire audio-groups JSON
// is currently present (by stable card ID, as listed in /proc/asound/cards).
// Used to decide whether the cached PipeWire group config is still valid for the
// current hardware without forking the (expensive) regeneration PHP. A removed
// or swapped card -> some referenced cardId is absent -> returns false so the
// regen runs and strips/re-resolves it. The regen resolves by stable card ID
// (not card number), so a card that's still present but renumbered keeps a valid
// config -- which is why presence-by-ID is a sufficient check.
static bool pipewireConfigCardsPresent(const std::string& jsonPath) {
    if (!FileExists(jsonPath)) {
        return false;
    }
    Json::Value root;
    if (!LoadJsonFromString(GetFileContents(jsonPath), root)) {
        return false;
    }
    std::vector<std::string> present;
    std::istringstream iss(GetFileContents("/proc/asound/cards"));
    std::string line;
    while (std::getline(iss, line)) {
        auto b = line.find('[');
        auto e = line.find(']');
        if (b != std::string::npos && e != std::string::npos && e > b) {
            std::string id = line.substr(b + 1, e - b - 1);
            TrimWhiteSpace(id);
            if (!id.empty()) {
                present.push_back(id);
            }
        }
    }
    for (const auto& grp : root["groups"]) {
        for (const auto& mbr : grp["members"]) {
            std::string cid = mbr.get("cardId", "").asString();
            if (!cid.empty() && std::find(present.begin(), present.end(), cid) == present.end()) {
                return false;
            }
        }
    }
    return true;
}

// Normalise an ALSA card ID for use in a PipeWire node name: lowercase, with
// every non-alphanumeric (and non-underscore) character replaced by '_'. Must
// match the convention used by the 95-fpp-alsa-sink.conf generation above and
// by GeneratePipeWireGroupsConfig() in pipewire.php.
static std::string normalizeCardIdForNode(const std::string& cardId) {
    std::string n = cardId;
    std::transform(n.begin(), n.end(), n.begin(), ::tolower);
    for (auto& ch : n) {
        if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_') ch = '_';
    }
    return n;
}

// Return the single-group member cardId recorded in a Simple-mode groups JSON,
// or "" if the file is absent/empty/unparseable or has no member. Simple mode
// always has exactly one group with one member (the selected output card).
static std::string simpleConfigCardId(const std::string& jsonPath) {
    if (!FileExists(jsonPath)) return "";
    Json::Value root;
    if (!LoadJsonFromString(GetFileContents(jsonPath), root)) return "";
    const Json::Value& groups = root["groups"];
    if (!groups.isArray() || groups.empty()) return "";
    const Json::Value& members = groups[0]["members"];
    if (!members.isArray() || members.empty()) return "";
    return members[0].get("cardId", "").asString();
}

// Build the contents of 97-fpp-audio-groups.conf for the Simple-mode synthetic
// group: a single 2-channel "Default" group whose one member is the selected
// sound card.  This reproduces, byte-for-byte for the simple case, what
// GeneratePipeWireGroupsConfig() in www/api/controllers/pipewire.php emits:
//
//   * an optional context.objects ALSA adapter — only when the card has no
//     boot-time fpp_alsa_* node from 95-fpp-alsa-sink.conf (the snd-dummy
//     fallback, which is detected after that conf was written),
//   * a per-member delay filter-chain (delay always present at 0s so the UI
//     can adjust it live during calibration — see $hasDelay = true in the PHP),
//   * a combine-stream group sink named fpp_group_default targeting the
//     filter-chain's virtual sink.
//
// Generating this here avoids forking PHP (apply_pipewire_simple_config /
// regenerate_pipewire_groups) on first boot, which costs ~10-30s of
// interpreter + common.php/pipewire.php startup on a single-core SBC.
static std::string buildSimplePipeWireGroupsConf(int card, const std::string& cId,
                                                 bool cardHasBootAdapter, int perSize,
                                                 int pipewireSampleRate) {
    const std::string cidNorm = normalizeCardIdForNode(cId);
    const std::string nodeName = "fpp_alsa_" + cidNorm; // adapter (boot-time 95 conf or custom below)
    const std::string fxNode = "fpp_fx_g1_" + cidNorm;  // filter-chain virtual sink
    const std::string fxOut = fxNode + "_out";

    std::ostringstream c;
    c << "# Auto-generated by FPP - PipeWire Audio Output Groups\n";
    c << "# Do not edit manually - managed via FPP UI\n\n";

    if (!cardHasBootAdapter) {
        // No boot-time adapter exists for this card (snd-dummy: the dummy is
        // loaded after 95-fpp-alsa-sink.conf is generated, so it never gets a
        // node there). Create the adapter inline so the filter-chain/combine
        // playback has a real sink to target. Detect the best PCM format the
        // device advertises, defaulting to the universally-safe S16LE.
        std::string fmt = "S16LE";
        std::string hwParams = execAndReturn("timeout 2 /usr/bin/aplay -D hw:" + cId +
                                             " --dump-hw-params /dev/zero 2>&1 | head -40");
        std::smatch fmtMatch;
        if (std::regex_search(hwParams, fmtMatch, std::regex(R"(FORMAT[^:]*:\s+(.+))"))) {
            std::string fmtLine = fmtMatch[1].str();
            if (fmtLine.find("S32_LE") != std::string::npos) {
                fmt = "S32LE";
            } else if (fmtLine.find("S24_LE") != std::string::npos) {
                fmt = "S24_32LE";
            } else if (fmtLine.find("S24_3LE") != std::string::npos) {
                fmt = "S24LE";
            }
        }
        const std::string desc = getAlsaCardProductName(card, cId) + " (" + cId + ")";
        c << "# Custom FPP ALSA adapter nodes\n";
        c << "# These provide sinks for cards with no WirePlumber node or needing extra channels\n";
        c << "context.objects = [\n";
        c << "  { factory = adapter\n";
        c << "    args = {\n";
        c << "      factory.name = api.alsa.pcm.sink\n";
        c << "      node.name = \"" << nodeName << "\"\n";
        c << "      node.description = \"" << desc << "\"\n";
        c << "      media.class = \"Audio/Sink\"\n";
        c << "      api.alsa.path = \"hw:" << cId << "\"\n";
        c << "      api.alsa.period-size = " << perSize << "\n";
        c << "      api.alsa.headroom = 256\n";
        c << "      audio.format = \"" << fmt << "\"\n";
        c << "      audio.rate = " << pipewireSampleRate << "\n";
        c << "      audio.channels = 2\n";
        c << "      audio.position = [ FL FR ]\n";
        c << "    }\n";
        c << "  }\n";
        c << "]\n\n";
    }

    c << "context.modules = [\n";
    // Delay filter-chain (0s) — always emitted, matching the PHP, so the
    // running graph has the fpp_fx_g1_* nodes the UI adjusts during calibration.
    c << "  # Filter chain (Delay) for: " << cId << " (Group 1)\n";
    c << "  { name = libpipewire-module-filter-chain\n";
    c << "    args = {\n";
    c << "      node.description = \"Delay: " << cId << "\"\n";
    c << "      filter.graph = {\n";
    c << "        nodes = [\n";
    c << "          { type = builtin label = delay name = delay_l config = { \"max-delay\" = 5 } control = { \"Delay (s)\" = 0 } }\n";
    c << "          { type = builtin label = delay name = delay_r config = { \"max-delay\" = 5 } control = { \"Delay (s)\" = 0 } }\n";
    c << "        ]\n";
    c << "        links = [\n";
    c << "        ]\n";
    c << "        inputs = [ \"delay_l:In\" \"delay_r:In\" ]\n";
    c << "        outputs = [ \"delay_l:Out\" \"delay_r:Out\" ]\n";
    c << "      }\n";
    c << "      capture.props = {\n";
    c << "        node.name = \"" << fxNode << "\"\n";
    c << "        media.class = Audio/Sink\n";
    c << "        audio.channels = 2\n";
    c << "        audio.position = [ FL FR ]\n";
    c << "      }\n";
    c << "      playback.props = {\n";
    c << "        node.name = \"" << fxOut << "\"\n";
    c << "        node.target = \"" << nodeName << "\"\n";
    c << "        stream.dont-remix = true\n";
    c << "        audio.channels = 2\n";
    c << "        audio.position = [ FL FR ]\n";
    c << "      }\n";
    c << "    }\n";
    c << "  }\n";
    // combine-stream group sink (the node fppd targets via PipeWireSinkName).
    c << "  { name = libpipewire-module-combine-stream\n";
    c << "    args = {\n";
    c << "      combine.mode = sink\n";
    c << "      node.name = \"fpp_group_default\"\n";
    c << "      node.description = \"Default\"\n";
    c << "      combine.latency-compensate = false\n";
    c << "      combine.props = {\n";
    c << "        audio.position = [ FL FR ]\n";
    c << "      }\n";
    c << "      stream.props = {\n";
    c << "        stream.dont-remix = true\n";
    c << "      }\n";
    c << "      stream.rules = [\n";
    c << "        { matches = [\n";
    c << "            { media.class = \"Audio/Sink\"\n";
    c << "              node.name = \"" << fxNode << "\"\n";
    c << "            }\n";
    c << "          ]\n";
    c << "          actions = {\n";
    c << "            create-stream = {\n";
    c << "              node.target = \"" << fxNode << "\"\n";
    c << "              combine.audio.position = [ FL FR ]\n";
    c << "              audio.position = [ FL FR ]\n";
    c << "            }\n";
    c << "          }\n";
    c << "        }\n";
    c << "      ]\n";
    c << "    }\n";
    c << "  }\n";
    c << "]\n";
    return c.str();
}

// Ensure the WirePlumber "block combine fallback" hook is installed. It is a
// static, hardware-independent script shipped in the repo under /opt/fpp/etc;
// copy it into WirePlumber's search paths if absent. Without it, combine-stream
// output nodes can get rogue links to the default sink (doubled audio). A fresh
// image does not yet have it in /etc + /usr/share, so the Simple-mode C++ path
// installs it here rather than forking the PHP just to bootstrap it.
// Returns true if the hook is present (or was just installed from the shipped
// copy), false only if the shipped source is missing (a broken checkout — the
// files are committed to the repo, so this normally can't happen) so the caller
// can fall back to the PHP apply path for the rest of the work.
static bool ensureWirePlumberLinkingHook() {
    const std::string luaSrc = "/opt/fpp/etc/wireplumber/scripts/linking/fpp-block-combine-fallback.lua";
    const std::string luaDst = "/usr/share/wireplumber/scripts/linking/fpp-block-combine-fallback.lua";
    const std::string confSrc = "/opt/fpp/etc/wireplumber/wireplumber.conf.d/60-fpp-block-combine-fallback.conf";
    const std::string confDst = "/etc/wireplumber/wireplumber.conf.d/60-fpp-block-combine-fallback.conf";
    if (!FileExists(luaSrc) || !FileExists(confSrc)) {
        return false; // shipped copies not present (pre-upgrade install) — let PHP handle it
    }
    if (!FileExists(luaDst)) {
        exec("/bin/mkdir -p /usr/share/wireplumber/scripts/linking");
        CopyFileContents(luaSrc, luaDst);
        chmod(luaDst.c_str(), 0644); // world-readable to match the PHP installer (WirePlumber may run non-root)
    }
    if (!FileExists(confDst)) {
        exec("/bin/mkdir -p /etc/wireplumber/wireplumber.conf.d");
        CopyFileContents(confSrc, confDst);
        chmod(confDst.c_str(), 0644);
    }
    return true;
}

// First-boot fast path for Simple PipeWire mode: synthesise the same audio
// config that apply_pipewire_simple_config (PHP) would, but entirely in C++ so
// no PHP interpreter is forked.  Writes the simple groups JSON, the
// 97-fpp-audio-groups.conf (both /etc dest and the media-dir cache so it
// survives reboot), removes the input-groups conf (simple mode has none), and
// sets PipeWireSinkName.  The caller restarts PipeWire once afterwards.
static void generateSimplePipeWireAudioConfig(int card, const std::string& cId,
                                              bool cardHasBootAdapter, int perSize,
                                              int pipewireSampleRate) {
    const std::string simpleGroupsJsonPath = FPP_MEDIA_DIR + "/config/pipewire-audio-groups-simple.json";
    const std::string videoSimpleJsonPath = FPP_MEDIA_DIR + "/config/pipewire-video-groups-simple.json";
    const std::string groupsConfCache = FPP_MEDIA_DIR + "/config/pipewire-audio-groups.conf";
    const std::string groupsConfDest = "/etc/pipewire/pipewire.conf.d/97-fpp-audio-groups.conf";
    const std::string igConfCache = FPP_MEDIA_DIR + "/config/pipewire-input-groups.conf";
    const std::string igConfDest = "/etc/pipewire/pipewire.conf.d/96-fpp-input-groups.conf";

    printf("FPP - pipewire-simple: generating audio config in C++ (card %s)\n", cId.c_str());

    // Simple groups JSON — mirrors BuildSimpleAudioGroupsData() in pipewire.php.
    Json::Value root, group, member;
    group["id"] = 1;
    group["name"] = "Default";
    group["enabled"] = true;
    group["channels"] = 2;
    group["volume"] = 100;
    group["activeGroup"] = true;
    member["cardId"] = cId;
    member["channels"] = 2;
    member["delayMs"] = 0;
    member["volume"] = 100;
    group["members"] = Json::Value(Json::arrayValue);
    group["members"].append(member);
    root["groups"] = Json::Value(Json::arrayValue);
    root["groups"].append(group);
    PutFileContents(simpleGroupsJsonPath, SaveJsonToString(root));

    // 97-fpp-audio-groups.conf — write the /etc dest and the media-dir cache
    // identically so the boot-time skip-regen fast path (cache == dest) holds.
    std::string conf = buildSimplePipeWireGroupsConf(card, cId, cardHasBootAdapter, perSize, pipewireSampleRate);
    exec("/bin/mkdir -p /etc/pipewire/pipewire.conf.d");
    PutFileContents(groupsConfDest, conf);
    PutFileContents(groupsConfCache, conf);

    // Simple mode has no input groups — remove any leftover 96 conf + cache.
    if (FileExists(igConfDest)) unlink(igConfDest.c_str());
    if (FileExists(igConfCache)) unlink(igConfCache.c_str());

    // Route fppd at the group sink; clear stale advanced-mode per-slot names.
    setRawSetting("PipeWireSinkName", "fpp_group_default");
    for (int s = 2; s <= 5; s++) {
        setRawSetting("PipeWireSinkName_" + std::to_string(s), "");
    }

    // Record the (empty) simple video config so the UI/video path matches what
    // ApplyPipeWireVideoGroups would leave for an unconfigured VideoOutput.
    if (!FileExists(videoSimpleJsonPath)) {
        PutFileContents(videoSimpleJsonPath, "{\n    \"videoOutputGroups\": []\n}");
    }
}

void setupAudio() {
    if (!FileExists("/root/.libao")) {
        PutFileContents("/root/.libao", "dev=default");
    }
    std::string mediaBackend = "pipewire-simple";
    getRawSetting("MediaBackend", mediaBackend);
    std::string mediaBackendLower = mediaBackend;
    std::transform(mediaBackendLower.begin(), mediaBackendLower.end(), mediaBackendLower.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    // The ALSA ("Hardware Direct") backend has been retired -- FPP10 always uses
    // a PipeWire backend. Migrate any device still set to "alsa" to the default
    // pipewire-simple and persist it, so we never take the (being-removed) ALSA
    // path and every other MediaBackend reader sees the new value. Done here, the
    // earliest audio entry point, so the rest of setupAudio runs as PipeWire.
    if (mediaBackendLower == "alsa") {
        printf("FPP - MediaBackend 'alsa' is retired; migrating to 'pipewire-simple'\n");
        setRawSetting("MediaBackend", "pipewire-simple");
        mediaBackend = "pipewire-simple";
        mediaBackendLower = "pipewire-simple";
    }
    bool usePipeWireBackend = (mediaBackendLower == "pipewire" || mediaBackendLower == "pipewire-simple");
    bool runningInDocker = FileExists("/.dockerenv");
    const std::string audioEnvPath = "/run/fppd/fpp-audio.env";
    printf("FPP - Audio backend: %s\n", usePipeWireBackend ? "PipeWire" : "ALSA");
    std::string aplay = execAndReturn("/usr/bin/aplay -l 2>&1");
    std::vector<std::string> lines = split(aplay, '\n');
    std::map<std::string, std::string> cards;
    std::map<std::string, std::string> cardLines; // full aplay line per "card N"
    // Normalised card IDs that got an fpp_alsa_* sink node in 95-fpp-alsa-sink.conf
    // below; consulted by the Simple-mode C++ config generator to know whether a
    // card already has a boot-time adapter or needs a custom one inline.
    std::set<std::string> bootAdapterCids;
    // True once the Simple-mode config has been generated in C++ this run, so the
    // post-restart PHP regenerate/volume-restore dance can be skipped (the config
    // is already complete and self-contained).
    bool cppGeneratedSimpleConfig = false;
    std::map<std::string, bool> hdmiStatus;
    bool hasNonHDMI = false;
    auto lineHasHDMI = [](const std::string& l) {
        std::string lc = l;
        std::transform(lc.begin(), lc.end(), lc.begin(), ::tolower);
        return lc.find("hdmi") != std::string::npos;
    };
    for (auto& l : lines) {
        if (l.starts_with("card ")) {
            std::string k = l.substr(0, 6);
            std::string v = l.substr(8);
            int idx = v.find(' ');
            v = v.substr(0, idx);
            cards[k] = v;
            cardLines[k] = l;
            hasNonHDMI |= !lineHasHDMI(l);
        }
    }
    int hdmiIdx = 0;
    bool anyHDMIConnected = false;
    for (int x = 0; x < 4; x++) {
        std::string cstr = "/sys/class/drm/card" + std::to_string(x) + "-HDMI-A-1/status";
        if (FileExists(cstr)) {
            for (int p = 1; p < 5; p++) {
                std::string cstr = "/sys/class/drm/card" + std::to_string(x) + "-HDMI-A-" + std::to_string(p) + "/status";
                std::string c = GetFileContents(cstr);
                std::string k = "vc4hdmi" + std::to_string(hdmiIdx);
                bool conn = c.contains("connected") && !c.contains("disconnected");
                hdmiStatus[k] = conn;
                anyHDMIConnected |= conn;
                hdmiIdx++;
            }
        }
    }
    // True when the only audio device is the synthetic snd-dummy (no real,
    // non-HDMI sound card present) -- the common case on a BeagleBone with no
    // audio cape. Used later to skip the expensive PipeWire device-enumeration
    // and group-regeneration dance, which only matters for real/hot-pluggable
    // hardware.
    bool noRealSoundcard = (!hasNonHDMI || contains(aplay, "no soundcards"));
    if (noRealSoundcard) {
        printf("FPP - No Soundcard Detected, loading snd-dummy\n");
        modprobe("snd-dummy");
    }
    // AudioOutput is persisted as a stable ALSA card ID string (e.g. "S3",
    // "bcm2835ALSA") rather than a card index, so a probe-order change (USB
    // add/remove, slow-probing cape, kernel update) doesn't repoint us at the
    // wrong device. Resolve it to the current card number here; everything
    // below continues to work with the numeric index.
    //   - empty/unset  -> first present card (index 0), as before
    //   - all-digits   -> legacy index; used as-is and migrated to an ID below
    //   - otherwise    -> card ID; matched against /proc/asound/cards
    std::string audioOutputId;
    getRawSetting("AudioOutput", audioOutputId);
    TrimWhiteSpace(audioOutputId);
    bool legacyNumeric = !audioOutputId.empty() && audioOutputId.find_first_not_of("0123456789") == std::string::npos;
    int card = 0;
    if (audioOutputId.empty()) {
        card = 0;
    } else if (legacyNumeric) {
        card = std::stoi(audioOutputId);
    } else {
        card = getAlsaCardNumForId(audioOutputId);
        if (card < 0) {
            printf("FPP - Audio device '%s' not currently present; falling back to card 0\n", audioOutputId.c_str());
            card = 0;
        }
    }
    std::string cstr = "card " + std::to_string(card);
    bool found = false;
    int count = 0;

    // Treat the selected card as unusable if it's any HDMI card (vc4hdmi OR
    // legacy bcm2835 HDMI) with no display connected. Playing to an
    // unconnected HDMI audio device panics the kernel on some Pis.
    auto cardIsDeadHDMI = [&](const std::string& k) {
        if (!lineHasHDMI(cardLines[k])) return false;
        if (cards[k].starts_with("vc4hdmi")) {
            return !hdmiStatus[cards[k]];
        }
        // legacy bcm2835 HDMI shares the physical port; if any HDMI is
        // connected, assume this device may work, otherwise treat as dead.
        return !anyHDMIConnected;
    };

    if (cardIsDeadHDMI(cstr)) {
        // Walk cards in index order and pick the first non-HDMI, or HDMI with
        // display connected. Falls through to the snd-dummy case otherwise.
        int fallback = -1;
        for (const auto& [k, _] : cards) {
            if (!cardIsDeadHDMI(k)) {
                fallback = std::stoi(k.substr(5));
                break;
            }
        }
        if (fallback >= 0 && fallback != card) {
            printf("FPP - Audio device %d has no HDMI connected, switching to card %d (%s)\n",
                   card, fallback, cards["card " + std::to_string(fallback)].c_str());
            card = fallback;
            setRawSetting("AudioOutput", getAlsaCardId(card));
        } else {
            card = cards.size();
            found = true;
            setRawSetting("AudioOutput", getAlsaCardId(card));
        }
    }
    while (!found && count < 50) {
        std::string amixer = execAndReturn("/usr/bin/amixer -c " + std::to_string(card) + " cset numid=3 1  2>&1");
        if (contains(amixer, "Invalid ")) {
            ++count;
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        } else {
            found = true;
        }
    }
    if (!found) {
        printf("FPP - Could not find audio device %d, defaulting to device 0.\n", card);
        CopyFileContents("/opt/fpp/etc/asoundrc.plain", "/root/.asoundrc");
        setRawSetting("AudioOutput", getAlsaCardId(0));
    } else {
        printf("FPP - Waited for %d seconds for audio device\n", (count / 5));
        // Persist the selection as the stable ALSA card ID. This migrates a
        // legacy numeric value and canonicalizes the stored ID so the next
        // boot matches by name regardless of probe order.
        std::string canonicalId = getAlsaCardId(card);
        if (!canonicalId.empty() && canonicalId != audioOutputId) {
            printf("FPP - Persisting selected audio device as ID '%s'\n", canonicalId.c_str());
            setRawSetting("AudioOutput", canonicalId);
        }
        // Point ALSA's default at the chosen card so root-context playback
        // (e.g. flite from fppinit) doesn't land on a dead HDMI device.
        std::string arc = GetFileContents("/opt/fpp/etc/asoundrc.plain");
        std::string cardLine = "card " + std::to_string(card);
        size_t pos = 0;
        while ((pos = arc.find("card 0", pos)) != std::string::npos) {
            arc.replace(pos, 6, cardLine);
            pos += cardLine.size();
        }
        PutFileContents("/root/.asoundrc", arc);
    }
    int v = getRawSettingInt("volume", -1);
    if (v == -1) {
        setRawSetting("volume", "70");
        v = 70;
    }
    std::string origAudio0CardType;
    getRawSetting("AudioCard0Type", origAudio0CardType);
    std::string audioCardType = "unknown";
    std::string aplayl = execAndReturn("/usr/bin/aplay -l | grep 'card " + std::to_string(card) + "'");
#ifdef PLATFORM_PI
    // Pi headphone jack needs a volume adjustment, in reality a lot of sound cards do, but we
    // don't want to put in a lot of special cases here so only handle the Pi
    if (card == 0) {
        if (contains(aplayl, "[bcm2") && !contains(aplayl, "-i2s")) {
            v = (v / 2) + 50;
            audioCardType = "bcm2";
        }
    }
#endif
    if (audioCardType != origAudio0CardType) {
        setRawSetting("AudioCard0Type", audioCardType);
    }
    std::string cardType = aplayl.substr(aplayl.find("[") + 1);
    cardType = cardType.substr(0, cardType.find("]"));
    printf("FPP - Found sound card of type %s\n", cardType.c_str());
    std::string asoundrc;
    if (usePipeWireBackend) {
        if (!FileExists("/etc/pipewire/client.conf") && FileExists("/usr/share/pipewire/client.conf")) {
            CopyFileContents("/usr/share/pipewire/client.conf", "/etc/pipewire/client.conf");
        }
        asoundrc = GetFileContents("/opt/fpp/etc/asoundrc.pipewire");
    } else {
        if (FileExists("/home/fpp/media/tmp/asoundrc")) {
            if (contains(cardType, "pcm510")) {
                printf("FPP - Using Cape Provided asoundrc\n");
                asoundrc = GetFileContents("/home/fpp/media/tmp/asoundrc");
            } else {
                asoundrc = GetFileContents("/opt/fpp/etc/asoundrc.dmix");
            }
        }
        if (asoundrc.empty()) {
            if (contains(cardType, "vc4-hdmi")) {
                replaceAll(cardType, "-", "");
                asoundrc = GetFileContents("/opt/fpp/etc/asoundrc.hdmi");
            } else if (contains(cardType, "bcm2")) {
                asoundrc = GetFileContents("/opt/fpp/etc/asoundrc.plain");
            } else {
                asoundrc = GetFileContents("/opt/fpp/etc/asoundrc.dmix");
            }
        }
    }
    int bufSize = getRawSettingInt("AudioBufferSize", 3072);
    int perSize = getRawSettingInt("AudioPeriodSize", 1024);
    replaceAll(asoundrc, "CARDTYPE", cardType);
    replaceAll(asoundrc, "BUFFERSIZE", std::to_string(bufSize));
    replaceAll(asoundrc, "PERIODSIZE", std::to_string(perSize));
    for (int x = 0; x < 10; x++) {
        if (x != card) {
            replaceAll(asoundrc, "card " + std::to_string(x), "card " + std::to_string(card));
        }
    }
    int rate = getRawSettingInt("AudioFormat", 0);
    int pipewireSampleRate = 44100;
    switch (rate) {
    case 1:
    case 2:
    case 3:
        // replaceAll(asoundrc, "rate 44100", "rate 44100");
        break;
    case 4:
    case 5:
    case 6:
        replaceAll(asoundrc, "rate 44100", "rate 48000");
        pipewireSampleRate = 48000;
        break;
    case 7:
    case 8:
    case 9:
        replaceAll(asoundrc, "rate 44100", "rate 96000");
        pipewireSampleRate = 96000;
        break;
    default:
        break;
    }
    PutFileContents("/root/.asoundrc", asoundrc);
    const std::string pipewireSinkConfPath = "/etc/pipewire/pipewire.conf.d/95-fpp-alsa-sink.conf";
    // Snapshot what PipeWire loaded at boot so we can tell whether the freshly
    // generated sink config actually differs (and thus needs a restart below).
    const std::string existingSinkConf = GetFileContents(pipewireSinkConfPath);
    bool sinkConfigChanged = false;
    // Cheap pre-check: is the persisted 95-fpp-alsa-sink.conf still valid for the
    // cards present right now? It is when its fpp_alsa_* sink adapters cover
    // exactly the present, usable (non-dummy, non-dead-HDMI) cards. When so, skip
    // the whole per-card ALSA probe below -- aplay/amixer/arecord plus a 2s
    // hw-params dump each, all via popen, which is the slowest part of audio
    // setup -- since it only changes anything when a card was added or removed.
    // The conf persists in /etc across reboots, so the common boot hits this.
    std::set<std::string> adapterCandidateCids; // present cards that would get an adapter
    for (const auto& [key, cardName] : cards) {
        std::string cId = getAlsaCardId(std::stoi(key.substr(5)));
        if (cId.empty()) cId = cardName;
        if (cId == "Dummy" || cardIsDeadHDMI(key)) continue;
        adapterCandidateCids.insert(normalizeCardIdForNode(cId));
    }
    std::set<std::string> existingAdapterCids; // fpp_alsa_* sink adapters already in the conf
    std::regex alsaAdapterRe(R"RE(node\.name = "fpp_alsa_([a-z0-9_]+)")RE");
    for (auto it = std::sregex_iterator(existingSinkConf.begin(), existingSinkConf.end(), alsaAdapterRe);
         it != std::sregex_iterator(); ++it) {
        existingAdapterCids.insert((*it)[1].str());
    }
    bool sinkConfStillValid = !existingSinkConf.empty() && existingAdapterCids == adapterCandidateCids;
    if (usePipeWireBackend && sinkConfStillValid) {
        printf("FPP - PipeWire: ALSA sink adapters already match present cards; skipping probe\n");
        // bootAdapterCids must still reflect the conf's adapters for the
        // Simple-mode group generator below; take them from the existing conf.
        bootAdapterCids = existingAdapterCids;
    } else if (usePipeWireBackend) {
        exec("/bin/mkdir -p /etc/pipewire/pipewire.conf.d");
        // Create FPP ALSA adapter nodes for ALL playback-capable cards present
        // at boot.  This ensures consistent naming (fpp_alsa_{cardIdNorm}) and
        // prevents WirePlumber from creating confusingly-named duplicates.
        // USB devices plugged in after boot get adapters created at Apply time
        // by GeneratePipeWireGroupsConfig() in pipewire.php.
        std::string arecordAll = execAndReturn("/usr/bin/arecord -l 2>/dev/null");
        std::ostringstream pipewireSink;
        pipewireSink << "context.objects = [\n";
        for (const auto& [key, cardName] : cards) {
            int cardNum = std::stoi(key.substr(5)); // "card N" -> N
            std::string cId = getAlsaCardId(cardNum);
            if (cId.empty()) cId = cardName;
            // Never create a boot-time adapter for the synthetic snd-dummy card.
            // It is only present here when snd-dummy happened to be loaded before
            // the aplay probe (e.g. a re-run of setupAudio within one boot); on a
            // clean boot it is loaded *after* this probe and so isn't in `cards`.
            // Letting it slip in would make the 95-conf non-deterministic and
            // leave the cached 97 group config targeting an fpp_alsa_dummy that
            // vanishes on the next clean reboot. The dummy is always handled
            // inline by the Simple-mode group config (buildSimplePipeWireGroupsConf).
            if (cId == "Dummy") continue;
            // Normalise card ID for node name: lowercase, non-alnum → underscore
            std::string cidNorm = cId;
            std::transform(cidNorm.begin(), cidNorm.end(), cidNorm.begin(), ::tolower);
            for (auto& ch : cidNorm) {
                if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_') ch = '_';
            }

            // Probe ALSA HW params with an exclusive open. This can fail for two
            // very different reasons that must NOT be conflated:
            //   1. The device is genuinely dead — e.g. HDMI with nothing
            //      connected (error 524/ENOMEDIUM). Skip it.
            //   2. The device is merely busy because PipeWire/WirePlumber already
            //      grabbed it. This code runs in postNetwork, AFTER
            //      fpp-pipewire.service has started and opened every card it knows
            //      about, so the *selected* card is routinely busy here. Skipping
            //      it would drop the adapter for the very device that's playing
            //      and (once PipeWire is restarted for any reason) silence it.
            // A busy device reports EBUSY and, because it is open, exposes its
            // live negotiated params in /proc; a dead device reports ENOMEDIUM and
            // its /proc hw_params reads "closed".
            std::string hwParams = execAndReturn("timeout 2 /usr/bin/aplay -D hw:" + cId + " --dump-hw-params /dev/zero 2>&1 | head -40");
            if (!contains(hwParams, "HW Params")) {
                std::string procHw = GetFileContents("/proc/asound/card" + std::to_string(cardNum) + "/pcm0p/sub0/hw_params");
                bool busy = contains(hwParams, "resource busy") || contains(hwParams, "Resource busy");
                bool procOpen = !procHw.empty() && !contains(procHw, "closed");
                if (!busy && !procOpen) {
                    printf("FPP - PipeWire: skipping card %d (%s) — device cannot be opened\n",
                           cardNum, cId.c_str());
                    continue;
                }
                // Present but busy: synthesise an aplay-style "HW Params" block
                // from the live /proc values so the channel/format detection below
                // works unchanged. /proc reports the format PipeWire actually
                // negotiated (always a real PCM, never IEC958-only). Fall back to
                // safe stereo S16_LE if /proc lacks the fields.
                std::string synthFmt = "S16_LE";
                std::string synthCh = "2";
                std::smatch pm;
                if (std::regex_search(procHw, pm, std::regex(R"(format:\s*(\S+))"))) {
                    synthFmt = pm[1].str();
                }
                if (std::regex_search(procHw, pm, std::regex(R"(channels:\s*(\d+))"))) {
                    synthCh = pm[1].str();
                }
                hwParams = "HW Params of device (busy — synthesised from /proc)\n"
                           "FORMAT:  " + synthFmt + "\n"
                           "CHANNELS: " + synthCh + "\n";
                printf("FPP - PipeWire: card %d (%s) busy (held by PipeWire); using live params FORMAT=%s CHANNELS=%s\n",
                       cardNum, cId.c_str(), synthFmt.c_str(), synthCh.c_str());
            }
            // Verify card supports at least one standard PCM format
            bool hasPcmFormat = contains(hwParams, "S16_LE") || contains(hwParams, "S24_LE")
                             || contains(hwParams, "S24_3LE") || contains(hwParams, "S32_LE");
            if (!hasPcmFormat) {
                printf("FPP - PipeWire: skipping card %d (%s) — no standard PCM format (IEC958 only?)\n",
                       cardNum, cId.c_str());
                continue;
            }

            std::string productName = getAlsaCardProductName(cardNum, cardName);
            std::string desc = productName + " (" + cId + ")";

            // USB audio cards need extra headroom and use different channel-detection logic.
            bool isUsbCard = cardLines.count(key) && cardLines[key].find("USB Audio") != std::string::npos;

            // Detect playback channels from ALSA HW params.
            // Formats: "CHANNELS: 8" (fixed), "CHANNELS[2]: 2 8" (discrete
            // list), "CHANNELS: [1 8]" (continuous range).
            //
            // A continuous range like "[1 8]" means the driver accepts any
            // count in that range — it does NOT imply that many physical
            // outputs.  The Pi onboard analog (bcm2835) reports "[1 8]"
            // because it has 8 hardware mixer subdevices, but it is a
            // 2-channel stereo jack.  Opening it as 8ch feeds interleaved
            // 8-channel data to a stereo DAC → garbled, high-pitched noise.
            // So for non-USB cards with a range we default to stereo;
            // only fixed/discrete declarations use the largest value.
            int maxChannels = 2;
            std::smatch chMatch;
            if (std::regex_search(hwParams, chMatch, std::regex(R"(CHANNELS\[?\d*\]?:\s+(.+))"))) {
                std::string chLine = chMatch[1].str();
                bool isRange = chLine.find('[') != std::string::npos;
                std::regex numRe(R"(\d+)");
                std::sregex_iterator it(chLine.begin(), chLine.end(), numRe);
                std::sregex_iterator end;
                std::vector<int> nums;
                for (; it != end; ++it) {
                    nums.push_back(std::stoi((*it)[0].str()));
                }
                if (isRange && nums.size() >= 2) {
                    int lo = nums.front();
                    int hi = nums.back();
                    if (isUsbCard) {
                        // USB audio: a range [lo hi] covers actual multi-channel
                        // altsets — use the maximum.
                        maxChannels = hi;
                    } else {
                        // Non-USB (e.g. bcm2835 analog): range is misleading,
                        // default to stereo clamped into the range.
                        maxChannels = std::min(std::max(2, lo), hi);
                    }
                } else {
                    // Fixed or discrete list: take the largest declared count.
                    for (int ch : nums) {
                        if (ch > maxChannels) maxChannels = ch;
                    }
                }
            }
            // For USB audio also read /proc/asound/cardN/stream0 which lists
            // every playback altset with its exact channel count.  This catches
            // cases where aplay reports a range or probes only one altset.
            if (isUsbCard) {
                std::string stream0 = GetFileContents("/proc/asound/card" + std::to_string(cardNum) + "/stream0");
                if (!stream0.empty()) {
                    // Only look in the Playback section (stop before Capture:)
                    size_t capturePos = stream0.find("\nCapture:");
                    std::string playSection = (capturePos != std::string::npos) ? stream0.substr(0, capturePos) : stream0;
                    std::regex chRe(R"(\bChannels:\s+(\d+))");
                    std::sregex_iterator it(playSection.begin(), playSection.end(), chRe);
                    std::sregex_iterator end;
                    for (; it != end; ++it) {
                        int ch = std::stoi((*it)[1].str());
                        if (ch > maxChannels) maxChannels = ch;
                    }
                }
            }
            if (maxChannels > 8) maxChannels = 8; // cap at 7.1

            // Detect best audio format from ALSA HW params
            // FORMAT line examples: "FORMAT: S16_LE S24_3LE", "FORMAT: S16_LE S24_LE S32_LE"
            // Priority: S32 > S24 > S16.  ALSA uses _ (S24_3LE), PipeWire drops it (S24LE).
            std::string audioFormat = "S16LE"; // safe default all cards support
            std::smatch fmtMatch;
            if (std::regex_search(hwParams, fmtMatch, std::regex(R"(FORMAT[^:]*:\s+(.+))"))) {
                std::string fmtLine = fmtMatch[1].str();
                if (fmtLine.find("S32_LE") != std::string::npos) {
                    audioFormat = "S32LE";
                } else if (fmtLine.find("S24_LE") != std::string::npos) {
                    audioFormat = "S24_32LE"; // 24-bit in 32-bit container
                } else if (fmtLine.find("S24_3LE") != std::string::npos) {
                    audioFormat = "S24LE"; // packed 24-bit (3 bytes)
                }
            }

            // Channel position arrays matching PipeWire convention
            static const char* positionArrays[] = {
                nullptr,
                "[ MONO ]",
                "[ FL FR ]",
                "[ FL FR FC ]",
                "[ FL FR RL RR ]",
                "[ FL FR FC RL RR ]",
                "[ FL FR FC LFE RL RR ]",
                "[ FL FR FC LFE RL RR RC ]",
                "[ FL FR FC LFE RL RR SL SR ]"
            };
            const char* posStr = (maxChannels >= 1 && maxChannels <= 8) ? positionArrays[maxChannels] : "[ FL FR ]";

            // USB audio cards need extra headroom: their independent oscillators
            // drift relative to the PipeWire graph driver clock, causing resyncs.
            int headroom = isUsbCard ? 4096 : 256;

            printf("FPP - PipeWire: creating adapter fpp_alsa_%s for card %d (%s) [%dch %s]%s\n",
                   cidNorm.c_str(), cardNum, cId.c_str(), maxChannels, audioFormat.c_str(),
                   isUsbCard ? " (USB, headroom=4096)" : "");
            pipewireSink << "  { factory = adapter\n"
                         << "    args = {\n"
                         << "      factory.name = api.alsa.pcm.sink\n"
                         << "      node.name = \"fpp_alsa_" << cidNorm << "\"\n"
                         << "      node.description = \"" << desc << "\"\n"
                         << "      media.class = \"Audio/Sink\"\n"
                         << "      api.alsa.path = \"hw:" << cId << "\"\n"
                         << "      api.alsa.period-size = " << perSize << "\n"
                         << "      api.alsa.headroom = " << headroom << "\n"
                         << "      audio.format = \"" << audioFormat << "\"\n"
                         << "      audio.rate = " << pipewireSampleRate << "\n"
                         << "      audio.channels = " << maxChannels << "\n"
                         << "      audio.position = " << posStr << "\n"
                         << "    }\n"
                         << "  }\n";
            bootAdapterCids.insert(cidNorm);
            // If this card has capture capability, also create an Audio/Source node.
            if (arecordAll.find("card " + std::to_string(cardNum) + ":") != std::string::npos) {
                // Detect capture channel count from ALSA HW params
                int capChannels = 2; // safe default — most USB cards need at least 2
                std::string capParams = execAndReturn("timeout 2 /usr/bin/arecord -D hw:" + cId + " --dump-hw-params /dev/zero 2>&1 | head -40");
                if (contains(capParams, "HW Params")) {
                    std::smatch capChMatch;
                    if (std::regex_search(capParams, capChMatch, std::regex(R"(CHANNELS\[?\d*\]?:\s+(.+))"))) {
                        std::string capChLine = capChMatch[1].str();
                        // Find the smallest number (minimum channels)
                        std::regex capNumRe(R"(\d+)");
                        std::sregex_iterator capIt(capChLine.begin(), capChLine.end(), capNumRe);
                        std::sregex_iterator capEnd;
                        int minCap = 99;
                        for (; capIt != capEnd; ++capIt) {
                            int c = std::stoi((*capIt)[0].str());
                            if (c > 0 && c < minCap) minCap = c;
                        }
                        if (minCap > 0 && minCap < 99) capChannels = minCap;
                    }
                }
                printf("FPP - PipeWire: card %d (%s) has capture [%dch], creating source node\n",
                       cardNum, cId.c_str(), capChannels);
                pipewireSink << "  { factory = adapter\n"
                             << "    args = {\n"
                             << "      factory.name = api.alsa.pcm.source\n"
                             << "      node.name = \"fpp_alsain_" << cidNorm << "\"\n"
                             << "      node.description = \"" << desc << "\"\n"
                             << "      node.nick = \"" << cId << "\"\n"
                             << "      media.class = \"Audio/Source\"\n"
                             << "      api.alsa.path = \"hw:" << cId << "\"\n"
                             << "      audio.format = \"" << audioFormat << "\"\n"
                             << "      audio.rate = 44100\n"
                             << "      audio.channels = " << capChannels << "\n"
                             << "    }\n"
                             << "  }\n";
            }
        }
        pipewireSink << "]\n";
        if (pipewireSink.str() != existingSinkConf) {
            PutFileContents(pipewireSinkConfPath, pipewireSink.str());
            // PipeWire only reads this file at startup; a card add/remove (or a
            // capability change) rewrites it, so flag a restart below to load it.
            sinkConfigChanged = true;
        }
    } else if (FileExists(pipewireSinkConfPath)) {
        unlink(pipewireSinkConfPath.c_str());
        sinkConfigChanged = true;
    }
    std::string mixers = execAndReturn("/usr/bin/amixer -c " + std::to_string(card) + " scontrols | cut -f2 -d\"'\"");
    if (mixers.empty()) {
        // for some sound cards, the mixer devices won't show up
        // until something is played.  Play one second of silence
        exec("/usr/bin/aplay -d 1 /opt/fpp/media/silence_5sec.wav >> /dev/null 2>&1  &");
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        mixers = execAndReturn("/usr/bin/amixer -c " + std::to_string(card) + " scontrols | cut -f2 -d\"'\"");
    }
    TrimWhiteSpace(mixers);
    std::string mixer;
    getRawSetting("AudioMixerDevice", mixer);
    std::string origMixer = mixer;
    if (mixer.empty() || !contains(mixers, mixer)) {
        mixer = mixers;
        if (mixer.find("\n") != std::string::npos) {
            mixer = mixer.substr(0, mixer.find("\n"));
        }
    }
    if (mixer != origMixer) {
        printf("FPP - Setting mixer device to %s\n", mixer.c_str());
        setRawSetting("AudioMixerDevice", mixer);
    }
    exec("/usr/bin/amixer -c " + std::to_string(card) + " set " + mixer + " " + std::to_string(v) + "% > /dev/null 2>&1");
    setRawSetting("AudioCardType", cardType);

    if (!runningInDocker) {
        mkdir("/run/fppd", 0755);
        std::ostringstream audioEnv;
        audioEnv << "SDL_AUDIODRIVER=" << (usePipeWireBackend ? "pulse" : "alsa") << "\n";
        // Keep OpenAL on the same backend as the rest of FPP audio.
        // In hardware (ALSA) mode this prevents OpenAL from defaulting to
        // PulseAudio and aborting during teardown if Pulse is unavailable.
        audioEnv << "ALSOFT_DRIVERS=" << (usePipeWireBackend ? "pulse" : "alsa") << "\n";
        if (usePipeWireBackend) {
            audioEnv << "PIPEWIRE_RUNTIME_DIR=/run/pipewire-fpp\n"
                     << "XDG_RUNTIME_DIR=/run/pipewire-fpp\n"
                     << "PIPEWIRE_CONFIG_DIR=/etc/pipewire\n"
                     << "PULSE_RUNTIME_PATH=/run/pipewire-fpp/pulse\n";
        }
        PutFileContents(audioEnvPath, audioEnv.str());
    } else if (FileExists(audioEnvPath)) {
        unlink(audioEnvPath.c_str());
    }

    // --- Audio Output Groups Configuration ---
    // On first PipeWire setup, create default output and input group configs
    // that mirror the legacy ALSA configuration: one output group with the
    // configured sound card, and one input group with fppd_stream_1 routed
    // to it.  This ensures audio works immediately when switching to PipeWire.
    const std::string groupsJsonPath = FPP_MEDIA_DIR + "/config/pipewire-audio-groups.json";
    const std::string simpleGroupsJsonPath = FPP_MEDIA_DIR + "/config/pipewire-audio-groups-simple.json";
    const std::string igJsonPath = FPP_MEDIA_DIR + "/config/pipewire-input-groups.json";

    // Simple PipeWire mode: the active config lives in pipewire-audio-groups-simple.json
    // and PipeWireSinkName is written by ApplyPipeWireSimpleConfig().  If that file is
    // missing (e.g. OS upgrade, fresh flash, or settings migrated without an explicit
    // UI save) synthesise it now from the current AudioOutput/VideoOutput settings so
    // PipeWireSinkName is populated before fppd starts.
    // Regenerate the simple groups config when it's missing OR when it
    // references a sound card that's no longer present. The latter is the
    // card-changed case (e.g. a USB card added to a previously dummy-only board,
    // or removed): the stale config would otherwise leave an unresolved device,
    // producing a "# WARNING:" conf that (a) is wrong and (b) defeats the
    // skip-regen fast path, forcing the full regen+restart dance on every boot.
    // Regenerating here re-points it at the current AudioOutput card.
    // The selected output card as a stable ALSA card ID (derived from the
    // AudioOutput setting above). The Simple config must point at this card.
    std::string selCid = getAlsaCardId(card);
    if (selCid.empty()) selCid = "Dummy"; // an empty ID only happens with no real card
    // Regenerate the simple config when it is missing, references an absent card,
    // OR points at a different card than the currently-selected output. The last
    // case is the post-upgrade / settings-changed scenario: e.g. AudioOutput is a
    // USB card (card 1) but the cached config still names the onboard card 0,
    // which is present so the cards-present check alone would wrongly skip it.
    bool simpleConfigStale = !FileExists(simpleGroupsJsonPath)
                          || !pipewireConfigCardsPresent(simpleGroupsJsonPath)
                          || simpleConfigCardId(simpleGroupsJsonPath) != selCid;
    if (usePipeWireBackend && !runningInDocker && mediaBackendLower == "pipewire-simple"
        && simpleConfigStale) {
        // The simple config is missing or points at an absent card (fresh flash,
        // OS upgrade, or a USB card added/removed). It's always the auto-generated
        // single-group/single-card shape, so synthesise it directly in C++ rather
        // than forking the PHP (apply_pipewire_simple_config), which costs ~10-30s
        // of interpreter startup on a single-core SBC.
        //
        // Two cases keep the PHP for correctness rather than speed:
        //   * VideoOutput is configured — apply_pipewire_simple_config also builds
        //     the video output groups, which the C++ path doesn't cover.
        //   * The shipped WirePlumber combine-fallback hook source isn't present
        //     to copy (a broken checkout — normally can't happen) — defer to the
        //     PHP apply path rather than generating a config without the hook.
        std::string videoOutput;
        getRawSetting("VideoOutput", videoOutput);
        TrimWhiteSpace(videoOutput);
        bool videoConfigured = !(videoOutput.empty() || videoOutput == "Disabled" || videoOutput == "--Default--");
        if (!videoConfigured && ensureWirePlumberLinkingHook()) {
            bool cardHasBootAdapter = bootAdapterCids.count(normalizeCardIdForNode(selCid)) > 0;
            generateSimplePipeWireAudioConfig(card, selCid, cardHasBootAdapter, perSize, pipewireSampleRate);
            cppGeneratedSimpleConfig = true;
        } else {
            printf("FPP - pipewire-simple: simple groups config missing or references an absent card; regenerating from AudioOutput setting...\n");
            system("/usr/bin/php /opt/fpp/scripts/apply_pipewire_simple_config");
        }
    }

    if (usePipeWireBackend && !runningInDocker && !FileExists(groupsJsonPath)) {
        std::string defCardId = getAlsaCardId(card);
        printf("FPP - First PipeWire setup: creating default output group (card %s) and input group\n", defCardId.c_str());

        // Default output group: one group with the legacy audio device
        Json::Value ogRoot;
        Json::Value group;
        group["id"] = 1;
        group["name"] = "Main Output";
        group["enabled"] = true;
        Json::Value member;
        member["cardId"] = defCardId;
        member["channels"] = 2;
        member["delayMs"] = 0.0;
        group["members"] = Json::Value(Json::arrayValue);
        group["members"].append(member);
        ogRoot["groups"] = Json::Value(Json::arrayValue);
        ogRoot["groups"].append(group);

        PutFileContents(groupsJsonPath, SaveJsonToString(ogRoot));

        // Default input group: fppd_stream_1 routed to output group 1
        Json::Value igRoot;
        Json::Value ig;
        ig["id"] = 1;
        ig["name"] = "Mix Bus 1";
        ig["enabled"] = true;
        ig["channels"] = 2;
        ig["volume"] = 100;
        Json::Value igMember;
        igMember["type"] = "fppd_stream";
        igMember["sourceId"] = "fppd_stream_1";
        igMember["name"] = "Media Playback";
        igMember["volume"] = 100;
        igMember["mute"] = false;
        ig["members"] = Json::Value(Json::arrayValue);
        ig["members"].append(igMember);
        ig["outputs"] = Json::Value(Json::arrayValue);
        ig["outputs"].append(1);
        igRoot["inputGroups"] = Json::Value(Json::arrayValue);
        igRoot["inputGroups"].append(ig);

        PutFileContents(igJsonPath, SaveJsonToString(igRoot));
    }

    // Restore cached combine-stream and input group configs so group sinks
    // survive reboot.  These are initially copied as-is so PipeWire starts
    // with the right module layout.  After PipeWire is up and WirePlumber
    // has enumerated devices, a regeneration script re-resolves card→sink
    // mappings to handle card-number changes.
    const std::string groupsConfCache = FPP_MEDIA_DIR + "/config/pipewire-audio-groups.conf";
    const std::string groupsConfDest = "/etc/pipewire/pipewire.conf.d/97-fpp-audio-groups.conf";
    const std::string igConfCache = FPP_MEDIA_DIR + "/config/pipewire-input-groups.conf";
    const std::string igConfDest = "/etc/pipewire/pipewire.conf.d/96-fpp-input-groups.conf";
    bool hasGroupsConfig = false;
    // Track whether the effective PipeWire config actually changes from what it
    // already loaded at boot. PipeWire (fpp-pipewire.service) starts during boot
    // with the persisted /etc/pipewire configs, so a restart in postNetwork is
    // only needed when something actually differs -- otherwise the costly
    // pipewire-pulse/WirePlumber/session-bus restart cascade buys nothing. On a
    // no-soundcard board the config is identical every boot, so this skips it.
    // A changed ALSA-sink adapter config (95-fpp-alsa-sink.conf, handled above)
    // needs the same restart as a changed groups config: PipeWire reads both only
    // at start. Seed the flag from it so a card add/remove isn't silently ignored.
    // A C++-generated Simple config (just written above) always needs the one
    // restart below to take effect — PipeWire started at boot without it.
    bool audioConfigChanged = sinkConfigChanged || cppGeneratedSimpleConfig;
    if (usePipeWireBackend && !runningInDocker && FileExists(groupsConfCache)) {
        if (!FileExists(groupsConfDest) || GetFileContents(groupsConfCache) != GetFileContents(groupsConfDest)) {
            printf("FPP - Restoring PipeWire audio output groups config\n");
            exec("/bin/cp " + groupsConfCache + " " + groupsConfDest);
            audioConfigChanged = true;
        }
        hasGroupsConfig = true;
    } else if (usePipeWireBackend && !runningInDocker && FileExists(groupsJsonPath)) {
        // JSON exists but no cached .conf yet (e.g. first-time default creation).
        // The regeneration script will generate the .conf from the JSON.
        hasGroupsConfig = true;
        audioConfigChanged = true;
    } else if (usePipeWireBackend && !runningInDocker && FileExists(groupsConfDest)) {
        // JSON config was deleted but stale conf remains — clean up
        if (!FileExists(groupsJsonPath)) {
            unlink(groupsConfDest.c_str());
            audioConfigChanged = true;
        }
    }
    if (usePipeWireBackend && !runningInDocker && FileExists(igConfCache)) {
        if (!FileExists(igConfDest) || GetFileContents(igConfCache) != GetFileContents(igConfDest)) {
            printf("FPP - Restoring PipeWire input groups config\n");
            exec("/bin/cp " + igConfCache + " " + igConfDest);
            audioConfigChanged = true;
        }
    }

    // --- AES67 cleanup ---
    // AES67 is now managed by AES67Manager in fppd (GStreamer-based).
    // Remove any leftover PipeWire RTP module configs from the legacy Python approach.
    unlink("/etc/pipewire/pipewire.conf.d/96-fpp-aes67-rtp.conf");
    unlink("/etc/pipewire/pipewire.conf.d/96-fpp-aes67-sap.conf");
    unlink("/etc/ptp4l-fpp.conf");
    // Kill any leftover legacy daemons
    exec("pkill -f fpp_aes67_sap 2>/dev/null || true");
    exec("pkill -f 'ptp4l.*ptp4l-fpp' 2>/dev/null || true");

    // PipeWire is already running (started at boot with the persisted configs),
    // so only restart it when the config actually changed. A restart is
    // synchronous and triggers an expensive pipewire-pulse/WirePlumber/
    // session-bus cascade, so skipping it on an unchanged boot (always the case
    // on a no-soundcard board) saves ~10-15s on a single-core SBC.
    if (usePipeWireBackend && !runningInDocker) {
        // Validate/regenerate the audio group config from the JSON, and use its
        // exit code (2 == it changed/created the .conf) to decide whether a
        // restart is needed. Run it whenever there's a groups config -- NOT gated
        // on the dest already existing (chicken-and-egg: the regen is what creates
        // it).
        //
        // No real sound card: the synthetic snd-dummy can't go missing or shift
        // cards, so the cached .conf is permanently valid. Run WITHOUT --force so
        // the regen fast-exits (no pw-dump, no rewrite) when the cache is already
        // clean -- avoiding a full regenerate + PipeWire restart on every boot.
        // (--force is non-deterministic and reports "changed" every run, so it
        // would restart PipeWire every boot.) With a real card we keep --force so
        // a removed/shifted device is stripped before PipeWire opens it (PipeWire
        // crashes fatally on a missing ALSA device).
        if (hasGroupsConfig) {
            // Decide whether the cached PipeWire group config is still valid in
            // C++ (cheap file reads) so we only fork the regeneration PHP -- which
            // costs ~3s of php/common.php startup, ~6s under boot contention --
            // when something actually changed. This covers BOTH no-soundcard and
            // real-card boards (e.g. BBB capes with onboard PCM5012A): skip the
            // PHP when the cached conf is complete (no unresolved-device warning),
            // matches what PipeWire loaded at boot (dest), and every sound card it
            // references is still present. The regen only runs on a genuine change
            // (first boot, settings edit, or a card added/removed), which is rare.
            bool needRegen = true;
            if (FileExists(groupsConfCache) && FileExists(groupsConfDest)) {
                std::string cached = GetFileContents(groupsConfCache);
                const std::string& activeJson =
                    (mediaBackendLower == "pipewire-simple") ? simpleGroupsJsonPath : groupsJsonPath;
                if (cached.find("# WARNING:") == std::string::npos
                    && cached == GetFileContents(groupsConfDest)
                    && pipewireConfigCardsPresent(activeJson)) {
                    needRegen = false;
                    printf("FPP - PipeWire audio config valid, loaded, and all referenced cards present; skipping regeneration\n");
                }
            }
            if (needRegen) {
                std::string regenCmd = "/usr/bin/php /opt/fpp/scripts/regenerate_pipewire_groups";
                if (!noRealSoundcard) {
                    regenCmd += " --force";
                }
                printf("FPP - Validating PipeWire audio group config against current hardware...\n");
                int rc = system(regenCmd.c_str());
                if (WEXITSTATUS(rc) == 2) {
                    audioConfigChanged = true;
                }
            }
        }

        if (!audioConfigChanged) {
            // Config matches what PipeWire loaded at boot -- restarting would
            // change nothing (and the volume-restore below only exists to undo a
            // restart's reset of WirePlumber state, so it's unneeded too).
            printf("FPP - PipeWire audio config unchanged since boot; skipping restart\n");
        } else if (hasGroupsConfig && noRealSoundcard) {
            // Config changed but there's no real sound card: a restart is needed
            // to load it, but there are no USB cards to enumerate and no
            // card-number shifts to resolve, so skip the enumerate/regen dance.
            exec("/usr/bin/systemctl restart fpp-pipewire.service fpp-wireplumber.service fpp-pipewire-pulse.service");
            printf("FPP - No real sound card present; skipping PipeWire device enumeration/regeneration\n");
        } else {
            exec("/usr/bin/systemctl restart fpp-pipewire.service fpp-wireplumber.service fpp-pipewire-pulse.service");
        }

        // Wait for WirePlumber to enumerate ALSA devices before regenerating
        // configs.  WirePlumber needs a moment to discover USB sound cards
        // and create their PipeWire sink nodes.
        //
        // Skipped when we just generated the Simple config in C++: it's
        // self-contained (combine-stream/filter-chain target the static
        // fpp_alsa_* adapter loaded from 95/97 conf), so there are no card→sink
        // mappings to re-resolve via pw-dump and no need to fork the PHP
        // regenerate/restore-volume scripts. The single restart above is enough.
        if (audioConfigChanged && hasGroupsConfig && !noRealSoundcard && !cppGeneratedSimpleConfig) {
            printf("FPP - Waiting for WirePlumber to enumerate devices...\n");
            std::this_thread::sleep_for(std::chrono::seconds(3));

            // Regenerate output/input group configs using current device state.
            // This fixes card-number shifts after reboot and resolves any
            // previously unresolved members.  Exit code 2 = config changed.
            printf("FPP - Regenerating PipeWire audio group configs...\n");
            int regenResult = system("/usr/bin/php /opt/fpp/scripts/regenerate_pipewire_groups --force");
            regenResult = WEXITSTATUS(regenResult);
            if (regenResult == 2) {
                // Config was changed — restart PipeWire to pick up the new config
                printf("FPP - PipeWire group config changed, restarting PipeWire...\n");
                exec("/usr/bin/systemctl restart fpp-pipewire.service fpp-wireplumber.service fpp-pipewire-pulse.service");
                // Wait for WirePlumber & combine-stream/filter-chain nodes to come up
                std::this_thread::sleep_for(std::chrono::seconds(3));
            }

            // Restore per-group and per-member volume levels from the JSON config.
            // WirePlumber may restore stale volume state on startup.
            printf("FPP - Restoring PipeWire audio group volumes...\n");
            system("/usr/bin/php /opt/fpp/scripts/restore_pipewire_volumes");
        }
    } else if (!runningInDocker) {
        exec("/usr/bin/systemctl stop fpp-pipewire-pulse.service fpp-wireplumber.service fpp-pipewire.service");
    }

    // AES67 is now initialized by AES67Manager in fppd after PipeWire is running.
    // No external daemons to start here.
}
