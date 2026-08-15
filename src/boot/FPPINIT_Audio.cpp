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
#include "fpp-json.h"
#include <chrono>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
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

// Number of registered cards that aren't the synthetic snd-dummy. Reads
// /proc/asound/cards, which is pure kernel state -- unlike "aplay -l", it needs
// no /dev/snd node and so is visible the instant the card registers.
static int countRealAlsaCards() {
    int count = 0;
    std::istringstream iss(GetFileContents("/proc/asound/cards"));
    std::string line;
    while (std::getline(iss, line)) {
        auto bracket = line.find('[');
        auto closeBracket = line.find(']');
        if (bracket != std::string::npos && closeBracket != std::string::npos && closeBracket > bracket) {
            std::string id = line.substr(bracket + 1, closeBracket - bracket - 1);
            TrimWhiteSpace(id);
            if (id != "Dummy") {
                ++count;
            }
        }
    }
    return count;
}

// True if the device tree declares a sound card -- i.e. this board is *expected*
// to have audio, whether or not the driver has bound yet. Used to decide whether
// "no cards yet" means "still coming" or "there is genuinely no audio here", so
// only the ambiguous case ever waits. A board with no sound node (a BeagleBone
// with no audio cape) costs one failed opendir.
static bool deviceTreeDeclaresSoundCard() {
    DIR* dp = opendir("/proc/device-tree");
    if (!dp) {
        return false;
    }
    bool found = false;
    struct dirent* ep = nullptr;
    while (!found && (ep = readdir(dp)) != nullptr) {
        std::string name = ep->d_name;
        // the node is "sound" or, when addressed, "sound@<unit>"
        if (name == "sound" || name.starts_with("sound@")) {
            found = true;
        }
    }
    closedir(dp);
    return found;
}

// True if a USB audio device is plugged in, whether or not snd-usb-audio has
// bound to it yet. A USB DAC has no device-tree node, so it needs its own check:
// the device appears in sysfs at enumeration, then udev autoloads snd-usb-audio,
// then the card registers -- and setupAudio can land in that gap.
//
// Read from sysfs rather than lsusb: only "lsusb -v" reports interface class, and
// it costs ~200ms on a BeagleBone (it walks and wakes every device) against a
// handful of open() calls here.
//
// bInterfaceClass 01 is USB Audio. Subclass 03 is MIDIStreaming, which yields a
// card with no playback device, so it can never satisfy the probe below -- skip
// it and don't wait on a MIDI controller.
static bool usbAudioDevicePresent() {
    DIR* dp = opendir("/sys/bus/usb/devices");
    if (!dp) {
        return false;
    }
    bool found = false;
    struct dirent* ep = nullptr;
    while (!found && (ep = readdir(dp)) != nullptr) {
        std::string dev = ep->d_name;
        if (dev == "." || dev == "..") {
            continue;
        }
        std::string base = "/sys/bus/usb/devices/" + dev;
        std::string cls = GetFileContents(base + "/bInterfaceClass");
        TrimWhiteSpace(cls);
        if (cls != "01") {
            continue;
        }
        std::string sub = GetFileContents(base + "/bInterfaceSubClass");
        TrimWhiteSpace(sub);
        if (sub != "03") {
            found = true;
        }
    }
    closedir(dp);
    return found;
}

// An ASoC card cannot bind until *every* component it references is present --
// the codec, the CPU DAI (e.g. McASP) and the machine driver (simple-audio-card).
// A cape EEPROM only names the codec, and the rest are device-tree platform
// devices whose drivers udev autoloads from their modalias. On a slow single-core
// board udev's coldplug can land tens of seconds into boot, long after we would
// have given up and taken snd-dummy -- measured ~35s after the codec on a
// BeagleBone, with the card arriving 10s behind snd-dummy.
//
// So do udev's job for the audio devices, early: modprobe the modalias of any
// still-unbound platform device whose compatible looks audio-related. Driving it
// off the kernel's own modalias means no module or SoC name is hardcoded, so this
// works for any cape/codec/CPU-DAI combination.
static void loadPendingSoundDrivers() {
    const std::string base = "/sys/bus/platform/devices";
    DIR* dp = opendir(base.c_str());
    if (!dp) {
        return;
    }
    struct dirent* ep = nullptr;
    while ((ep = readdir(dp)) != nullptr) {
        std::string dev = ep->d_name;
        if (dev == "." || dev == "..") {
            continue;
        }
        std::string path = base + "/" + dev;
        if (FileExists(path + "/driver")) {
            continue; // a driver is already bound
        }
        std::string modalias = GetFileContents(path + "/modalias");
        TrimWhiteSpace(modalias);
        // the modalias embeds the DT compatible, so it is also what we match on.
        // A quote would let the string escape the shell below; nothing the kernel
        // emits contains one, but refuse rather than trust that.
        if (modalias.empty() || modalias.find('\'') != std::string::npos) {
            continue;
        }
        std::string lower = modalias;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (!lower.contains("audio") && !lower.contains("sound") && !lower.contains("mcasp") && !lower.contains("i2s")) {
            continue;
        }
        printf("FPP - Loading sound driver for %s (%s)\n", dev.c_str(), modalias.c_str());
        exec("/sbin/modprobe '" + modalias + "' > /dev/null 2>&1");
    }
    closedir(dp);
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

// Known multi-channel I2S cards whose drivers advertise a continuous channel
// range (e.g. "CHANNELS: [2 8]") that the non-USB range heuristic below would
// clamp to stereo, and whose live /proc hw_params may show a stale 2-channel
// negotiation left over from a previous config.  Returns the minimum channel
// count the card must be configured with, or 0 when no quirk applies.  Matched
// against the card's full `aplay -l` line (issue #2620).
// Mirrored in DetectAlsaCardMaxChannels() in www/api/controllers/pipewire.php.
static int quirkMinChannelsForCard(const std::string& aplayLine) {
    static const std::pair<const char*, int> quirks[] = {
        { "hifiberry_dac8x", 8 }, // HiFiBerry DAC8x / Raspiaudio 8xOUT: 4x stereo DACs on one I2S bus
    };
    std::string lineLc = aplayLine;
    std::transform(lineLc.begin(), lineLc.end(), lineLc.begin(), ::tolower);
    for (const auto& [needle, ch] : quirks) {
        if (lineLc.find(needle) != std::string::npos) {
            return ch;
        }
    }
    return 0;
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

// Open the device in one PCM format and report the sample rate the driver
// actually settled on -- 0 if the format is unusable or the probe could not run.
//
// This has to be an open.  `aplay --dump-hw-params` reports the unrefined
// capability space and ignores both -f and -r, so a fixed-bit-clock I2S card
// (TI McASP driving a PCM5102A on a BeagleBone cape) lists "FORMAT: S16_LE
// S32_LE" and "RATE: [11025 44100]" while in fact only S16_LE reaches 44100:
// the bit clock is the constant, not the rate, so 32 bits/frame x 44100 ==
// 64 bits/frame x 22050.  Nothing in the advertised space shows that coupling.
//
// Feeding 8KB of zeros opens the device, negotiates, and exits in ~250ms.  The
// payload is silence, so nothing audible is emitted.
//
// Two different failures have to be told apart, and neither can be read off the
// banner: aplay prints "Playing raw data ..." from header() *before*
// set_params() negotiates anything, so it appears even for a format the card
// then rejects.  An unusable format exits non-zero ("Sample format non
// available"); a rate the driver refines to something else only warns
// ("rate is not accurate (requested = X, got = Y)") and still exits 0.  So:
// judge usability by exit status, and take the achieved rate from the warning.
// Keep in sync with PipeWireProbeFormatRate() in www/api/controllers/pipewire.php.
static constexpr char kProbeRcTag[] = "fpp-audio-probe-rc:";

static int achievedRateForFormat(const std::string& alsaPath, const std::string& pwFmt,
                                 int rate, int channels) {
    std::string alsaFmt;
    if (pwFmt == "S32LE") {
        alsaFmt = "S32_LE";
    } else if (pwFmt == "S24_32LE") {
        alsaFmt = "S24_LE";
    } else if (pwFmt == "S24LE") {
        alsaFmt = "S24_3LE";
    } else if (pwFmt == "S16LE") {
        alsaFmt = "S16_LE";
    } else {
        return 0;
    }
    if (rate <= 0) rate = 44100;
    if (channels <= 0) channels = 2;
    // $? after the pipeline is aplay's own status; echo it so the exit code
    // survives execAndReturn(), which only hands back stdout.
    std::string out = execAndReturn("head -c 8192 /dev/zero | timeout 2 /usr/bin/aplay -D " +
                                    alsaPath + " -t raw -f " + alsaFmt + " -r " +
                                    std::to_string(rate) + " -c " + std::to_string(channels) +
                                    " - 2>&1; echo \"" + kProbeRcTag + "$?\"");
    // Unverifiable (device busy, probe timed out, aplay missing, shell never ran).
    const size_t tagPos = out.rfind(kProbeRcTag);
    if (tagPos == std::string::npos) {
        return 0;
    }
    if (std::strtol(out.c_str() + tagPos + (sizeof(kProbeRcTag) - 1), nullptr, 10) != 0) {
        return 0;
    }
    std::smatch m;
    if (std::regex_search(out, m, std::regex(R"(not accurate \(requested = \d+Hz, got = (\d+)Hz\))"))) {
        return std::stoi(m[1].str());
    }
    return rate; // negotiated exactly what was asked for
}

// Widest first, so the first match in a FORMAT line is the best one. ALSA spells
// these with an underscore before LE, PipeWire without.
static const std::pair<const char*, const char*> kPcmFormatNames[] = {
    { "S32_LE", "S32LE" },
    { "S24_LE", "S24_32LE" }, // 24-bit in a 32-bit container
    { "S24_3LE", "S24LE" },   // packed 24-bit (3 bytes)
    { "S16_LE", "S16LE" },
};

// ALSA's spelling of a single PCM format -> PipeWire's. Anything unrecognised
// (an exotic or big-endian format) becomes S16LE, which every card supports.
static std::string pipewireFormatName(const std::string& alsaFmt) {
    for (const auto& [alsaName, pwName] : kPcmFormatNames) {
        if (alsaFmt == alsaName) {
            return pwName;
        }
    }
    return "S16LE";
}

// Pick the widest PCM format the card advertises that costs no sample rate
// relative to the universally-safe S16LE fallback.
//
// The question is NOT "does this format hold the rate we asked for".  A card can
// be unable to deliver the requested rate in ANY format -- an AM62x PCM5102A cape
// clocks no lower than 88200, so a 44100 request is refined upward for S16_LE and
// S32_LE alike.  Treating any refinement as a rejection there throws away S32 for
// a card that pays nothing to provide it.  Meanwhile the AM335x cape refines
// S32_LE from 44100 down to 22050 while S16_LE holds 44100 exactly, and taking
// S32 there is what makes PipeWire fail adapter creation, abort context creation,
// exit, and get restarted by systemd every few seconds forever (issue #2811).
//
// Both are the same rule once the comparison is made against what the fallback
// actually achieves rather than against what we asked for: widen only when it is
// free.  Measured on both capes; see the achievedRateForFormat() probe.
static std::string bestFormatForRate(const std::string& fmtLine, const std::string& alsaPath,
                                     int rate, int channels) {
    // Nothing wider is even advertised -- skip the baseline probe entirely.
    bool anyWider = false;
    for (size_t i = 0; i + 1 < std::size(kPcmFormatNames); ++i) {
        anyWider |= fmtLine.find(kPcmFormatNames[i].first) != std::string::npos;
    }
    if (!anyWider) {
        return "S16LE";
    }
    // The rate to beat. If this cannot be established (device busy, probe timed
    // out) there is nothing to compare against, so decline to widen: a needlessly
    // narrow format costs only bit depth, a wrongly wide one costs all audio.
    const int baselineRate = achievedRateForFormat(alsaPath, "S16LE", rate, channels);
    if (baselineRate <= 0) {
        return "S16LE";
    }
    // Stop before the last entry: S16LE is the fallback, already probed above.
    for (size_t i = 0; i + 1 < std::size(kPcmFormatNames); ++i) {
        const auto& [alsaName, pwName] = kPcmFormatNames[i];
        if (fmtLine.find(alsaName) == std::string::npos) {
            continue;
        }
        if (achievedRateForFormat(alsaPath, pwName, rate, channels) >= baselineRate) {
            return pwName;
        }
    }
    return "S16LE";
}

// Version of the rules the ALSA probe below applies when it derives an adapter
// from a card (format, rate, channels, headroom, quirks).
//
// 95-fpp-alsa-sink.conf lives in /etc and so survives every upgrade, and the
// "adapters already match present cards" fast path re-validates only which cards
// it names -- not how it described them.  Without a version, an install keeps a
// conf that FPP's own code would no longer generate, forever: fixing a probe rule
// only ever helps boxes flashed after the fix, never the ones already broken by
// it.  That is not hypothetical.  Generation 1 (untagged) could pick S32LE for a
// fixed-bit-clock I2S cape that only reaches S16LE at the target rate; PipeWire
// then failed adapter creation, aborted context creation and exited, and systemd
// restarted it every few seconds indefinitely -- thousands of restarts and no
// audio on a box that had already installed the fix (issue #2811).
//
// Bump this whenever the probe can produce a different conf for unchanged
// hardware.  Costs one extra probe on the first boot after the upgrade.
static constexpr int kAlsaSinkConfGeneration = 3;

static std::string alsaSinkConfGenerationTag() {
    return "# FPP ALSA sink adapters (generation " + std::to_string(kAlsaSinkConfGeneration) + ")";
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

    // No real sound card: the selection resolved to the synthetic snd-dummy.
    // Building the normal graph here (hw:Dummy adapter -> filter-chain ->
    // combine-stream) pins the dummy PCM open, and because the filter-chain's
    // playback link is not passive the sink never reaches idle, so PipeWire
    // cycles the graph at the quantum rate (~47 wakeups/s) forever -- measured
    // at 4% of a core on a single-core K8-PB that can never produce sound.
    //
    // fppd still needs *a* sink: media playback drives sequence position, and
    // with no sink at all pipewiresink fails with "no target node available"
    // and returns instantly instead of playing in real time, which would break
    // sequence timing on exactly these boards.  (WirePlumber ships
    // scripts/fallback-sink.lua for this, but it is not registered in any
    // profile, so no auto_null is ever created.)
    //
    // A null-audio-sink satisfies both: it consumes in real time so timing is
    // preserved, and it suspends when idle so the graph parks completely.
    // It must be wrapped in the `adapter` factory (as WirePlumber's own
    // fallback-sink does) -- created via spa-node-factory directly it gets no
    // usable ports and linking fails the same way as having no sink at all.
    if (cId == "Dummy") {
        c << "# No sound card detected - a null sink keeps media playback (and so\n";
        c << "# sequence timing) working while letting the graph park when idle.\n";
        c << "context.objects = [\n";
        c << "  { factory = adapter\n";
        c << "    args = {\n";
        c << "      factory.name = support.null-audio-sink\n";
        c << "      node.name = \"fpp_group_default\"\n";
        c << "      node.description = \"FPP Audio (no sound card)\"\n";
        c << "      media.class = \"Audio/Sink\"\n";
        c << "      audio.rate = " << pipewireSampleRate << "\n";
        c << "      audio.channels = 2\n";
        c << "      audio.position = [ FL FR ]\n";
        c << "      monitor.channel-volumes = true\n";
        c << "    }\n";
        c << "  }\n";
        c << "]\n";
        return c.str();
    }

    if (!cardHasBootAdapter) {
        // No boot-time adapter exists for this card (snd-dummy: the dummy is
        // loaded after 95-fpp-alsa-sink.conf is generated, so it never gets a
        // node there). Create the adapter inline so the filter-chain/combine
        // playback has a real sink to target. Detect the best PCM format the
        // device advertises, defaulting to the universally-safe S16LE.
        // Capture the advertised format list here, but defer choosing one until
        // alsaPath is resolved below: the rate-holding probe has to open the
        // device we will actually configure (hw: or sysdefault:), not a guess.
        std::string fmt = "S16LE";
        std::string fmtLine;
        std::string hwParams = execAndReturn("timeout 2 /usr/bin/aplay -D hw:" + cId +
                                                " --dump-hw-params /dev/zero 2>&1 | head -40");
        std::smatch fmtMatch;
        if (std::regex_search(hwParams, fmtMatch, std::regex(R"(FORMAT[^:]*:\s+(.+))"))) {
            fmtLine = fmtMatch[1].str();
        }
        // Some cards expose only IEC958_SUBFRAME_LE passthrough on raw hw: with
        // no standard PCM format (e.g. the Pi Zero W2 / Pi 3 vc4-hdmi card under
        // KMS). PipeWire's SPA ALSA plugin cannot open a passthrough-only hw:
        // device, so no sink is created and the fpp_group_default combine-stream
        // ends up with no target, making GStreamer fail with "Failed to connect".
        // sysdefault: routes through ALSA's dmix/plug layer (which downconverts
        // IEC958 to normal PCM), so use it whenever hw: lacks a PCM format but
        // sysdefault: provides one.
        std::string alsaPath = "hw:" + cId;
        bool hasPcmFmt = hwParams.find("S16_LE") != std::string::npos ||
                         hwParams.find("S24_LE") != std::string::npos ||
                         hwParams.find("S32_LE") != std::string::npos;
        if (!hasPcmFmt && hwParams.find("IEC958_SUBFRAME_LE") != std::string::npos) {
            std::string sysParams = execAndReturn("timeout 2 /usr/bin/aplay -D sysdefault:" + cId +
                                                    " --dump-hw-params /dev/zero 2>&1 | head -40");
            if (sysParams.find("S16_LE") != std::string::npos ||
                sysParams.find("S24_LE") != std::string::npos ||
                sysParams.find("S32_LE") != std::string::npos) {
                alsaPath = "sysdefault:" + cId;
            }
        }
        // Now that the target device is known, pick the widest advertised format
        // that can actually hold the configured rate on it.
        if (!fmtLine.empty()) {
            fmt = bestFormatForRate(fmtLine, alsaPath, pipewireSampleRate, 2);
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
        c << "      api.alsa.path = \"" << alsaPath << "\"\n";
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

// Bounded wait for FPP's sinks to exist in the graph. pactl against a graph that
// is up but has not finished loading its modules silently sets nothing, so the
// volume restore needs the nodes to be there first. Replaces a flat sleep on the
// paths that skip the regeneration: normally returns on the first poll, and it
// cannot outlast maxSeconds if the graph never comes up.
//
// Returns false on timeout, which is also the audio stack's failure signal: see
// the recovery block at the end of runAudioSetup().
static bool waitForPipeWireSinks(int maxSeconds) {
    const std::string cmd = "PIPEWIRE_RUNTIME_DIR=/run/pipewire-fpp XDG_RUNTIME_DIR=/run/pipewire-fpp "
                            "PULSE_RUNTIME_PATH=/run/pipewire-fpp/pulse "
                            "/usr/bin/pactl list short sinks 2>/dev/null";
    for (int i = 0; i < maxSeconds * 4; i++) {
        if (execAndReturn(cmd).find("fpp_") != std::string::npos) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
    return false;
}

static void setPipeWireSinkVolume(const std::string& node, int volPct) {
    // Node names are generated by normalizeCardIdForNode() (alphanumerics and
    // underscore only), but nodeTarget comes straight out of the user's JSON --
    // refuse anything that could escape the single quotes rather than handing it
    // to a shell.
    if (node.empty() || node.find('\'') != std::string::npos) {
        return;
    }
    if (volPct < 0) volPct = 0;
    if (volPct > 200) volPct = 200;
    exec("PIPEWIRE_RUNTIME_DIR=/run/pipewire-fpp XDG_RUNTIME_DIR=/run/pipewire-fpp "
         "PULSE_RUNTIME_PATH=/run/pipewire-fpp/pulse "
         "/usr/bin/pactl set-sink-volume '" +
         node + "' " + std::to_string(volPct) + "% 2>/dev/null");
}

// Reapply the user's configured group/member volumes after the PipeWire stack has
// been (re)started -- WirePlumber restores its own persisted state on every start,
// which is not necessarily what the FPP UI has configured.
//
// This is a direct C++ port of RestorePipeWireGroupVolumes() in
// www/api/controllers/pipewire.php, which remains the implementation used by the
// UI's interactive applies (where the interpreter is already warm). Forking it at
// boot cost php + common.php + the API controller plus sudo'd pactl calls --
// measured 6s uncontended and 12s under boot load on a single-core BBB -- to emit
// a handful of pactl calls, and all of it landed on the critical path to fppd, and
// therefore to the moment the web UI is actually usable. fppinit already runs as
// root, so the sudo hops go away too.
//
// KEEP IN SYNC with the PHP. The node-naming convention is shared with
// GeneratePipeWireGroupsConfig(); normalizeCardIdForNode() already mirrors its
// slugging rule, and this reuses it rather than re-deriving it.
static void restorePipeWireVolumes() {
    // Advanced-mode config only, matching the PHP: Simple mode has a single
    // auto-generated group created at full volume, and the PHP reads this same
    // path unconditionally, so a simple-mode box is a no-op here either way.
    const std::string groupsJsonPath = FPP_MEDIA_DIR + "/config/pipewire-audio-groups.json";
    if (!FileExists(groupsJsonPath)) {
        return;
    }
    Json::Value root;
    if (!LoadJsonFromString(GetFileContents(groupsJsonPath), root) || !root.isMember("groups")) {
        return;
    }
    // The filter-chain/combine-stream nodes appear a moment after the daemon
    // itself, and pactl against a half-built graph silently sets nothing.
    if (!waitForPipeWireSinks(30)) {
        printf("FPP - Timed out waiting for PipeWire sinks; restoring volumes anyway\n");
    }
    printf("FPP - Restoring PipeWire audio group volumes...\n");
    for (const auto& grp : root["groups"]) {
        if (!grp.get("enabled", false).asBool() || !grp.isMember("members") || grp["members"].empty()) {
            continue;
        }
        int groupId = grp.get("id", 0).asInt();
        setPipeWireSinkVolume("fpp_group_" + normalizeCardIdForNode(grp.get("name", "Group").asString()),
                              grp.get("volume", 100).asInt());
        for (const auto& mbr : grp["members"]) {
            std::string cardId = mbr.get("cardId", "").asString();
            if (cardId.empty()) {
                continue;
            }
            setPipeWireSinkVolume("fpp_fx_g" + std::to_string(groupId) + "_" + normalizeCardIdForNode(cardId),
                                  mbr.get("volume", 100).asInt());
            // A member targeting a WirePlumber-managed node (e.g. an HDMI output)
            // must have that node pinned to 100%: WirePlumber initialises them at
            // ~40%, which would silently attenuate everything the filter chain
            // delivers. FPP-owned nodes are already created at full volume.
            std::string nodeTarget = mbr.get("nodeTarget", "").asString();
            if (!nodeTarget.empty() && !startsWith(nodeTarget, "fpp_") && !startsWith(nodeTarget, "aes67_")) {
                setPipeWireSinkVolume(nodeTarget, 100);
            }
        }
    }
}

// WirePlumber's ALSA monitor enumerates every card independently of the static
// fpp_alsa_* adapters PipeWire loads from 95-fpp-alsa-sink.conf, so a card FPP
// already owns ALSO gets a monitor-created node (alsa_output.<sysfs>.<profile>).
// Both want the same PCM, and the monitor's node -- backed by a real device
// object, unlike a bare context.objects adapter -- wins the default-sink
// election.  Every untargeted stream then lands on a node whose snd_pcm_open()
// returns EBUSY: the node goes to error and the stream neither starts nor ends,
// so the client blocks in poll() forever.  That is not a hypothetical -- it is
// how the flite IP announcement wedged a Type=oneshot boot service and held
// multi-user.target unreachable indefinitely.
//
// FPP declares both sink and source adapters for every present card, so the
// monitor has nothing left to contribute for these: disable its device outright.
// Keyed on the ALSA card short-name because api.alsa.card.name is populated by
// the time the monitor evaluates monitor.alsa.rules, whereas alsa.id /
// alsa.card_name are attached to the device only afterwards -- a rule keyed on
// those silently never matches (verified on hardware).
//
// Returns true if the file changed.  WirePlumber reads rules only at startup,
// so the caller must restart it for a change to take effect.
static bool ensureWirePlumberAlsaDupeSuppression(const std::set<std::string>& cardNames) {
    const std::string confPath = "/etc/wireplumber/wireplumber.conf.d/50-fpp-suppress-alsa-dupes.conf";
    if (cardNames.empty()) {
        // Nothing FPP owns statically (no usable card, or the PipeWire backend is
        // off): drop any stale rule rather than leaving cards suppressed with no
        // adapter to replace them.
        if (!FileExists(confPath)) {
            return false;
        }
        unlink(confPath.c_str());
        return true;
    }
    std::ostringstream out;
    out << "# Auto-generated by FPP - do not edit manually\n"
        << "# Suppresses WirePlumber's duplicate ALSA device for every card that already\n"
        << "# has a static fpp_alsa_* adapter in 95-fpp-alsa-sink.conf.  Two nodes on one\n"
        << "# PCM means the loser fails to open with EBUSY and hangs its clients forever.\n"
        << "monitor.alsa.rules = [\n";
    for (const auto& name : cardNames) {
        out << "  {\n"
            << "    matches = [ { api.alsa.card.name = \"" << name << "\" } ]\n"
            << "    actions = { update-props = { device.disabled = true } }\n"
            << "  }\n";
    }
    out << "]\n";
    const std::string conf = out.str();
    if (FileExists(confPath) && GetFileContents(confPath) == conf) {
        return false;
    }
    exec("/bin/mkdir -p /etc/wireplumber/wireplumber.conf.d");
    PutFileContents(confPath, conf);
    chmod(confPath.c_str(), 0644);
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

// `recoveryPass` is set only by the self-heal at the very end of this function,
// which discards the cached audio config and re-runs the whole setup once when
// the stack it just started produced no sink at all.  It exists solely to bound
// that retry to one attempt.
static void runAudioSetup(bool recoveryPass) {
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

    // AudioOutput is persisted as a stable ALSA card ID string (e.g. "S3",
    // "bcm2835ALSA") rather than a card index, so a probe-order change (USB
    // add/remove, slow-probing cape, kernel update) doesn't repoint us at the
    // wrong device.
    //   - empty/unset  -> first present card (index 0), as before
    //   - all-digits   -> legacy index; used as-is and migrated to an ID below
    //   - otherwise    -> card ID; matched against /proc/asound/cards
    std::string audioOutputId;
    getRawSetting("AudioOutput", audioOutputId);
    TrimWhiteSpace(audioOutputId);
    bool legacyNumeric = !audioOutputId.empty() && audioOutputId.find_first_not_of("0123456789") == std::string::npos;

    // A card selected by ID may not have registered yet: cape modules are
    // modprobed back at cape detect, but an ASoC card (e.g. the K32Max's
    // CapeAudio-pcm5102a) binds asynchronously and can land after we get here.
    // Wait for it before probing, otherwise the aplay snapshot below misses it,
    // snd-dummy gets loaded in its place, and the selection resolves to Dummy.
    // Only a selected-but-absent card waits, so a board with no selection (or
    // one whose card is already up) pays nothing.
    if (!audioOutputId.empty() && !legacyNumeric && getAlsaCardNumForId(audioOutputId) < 0) {
        printf("FPP - Waiting for audio device '%s' to register...\n", audioOutputId.c_str());
        int waited = 0;
        while (waited < 100 && getAlsaCardNumForId(audioOutputId) < 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            ++waited;
        }
        if (getAlsaCardNumForId(audioOutputId) < 0) {
            printf("FPP - Audio device '%s' did not appear after %d seconds\n", audioOutputId.c_str(), waited / 10);
        } else {
            printf("FPP - Audio device '%s' registered after %d.%d seconds\n", audioOutputId.c_str(), waited / 10, waited % 10);
        }
    }

    std::string aplay;
    std::vector<std::string> lines;
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
    // Snapshot the cards "aplay -l" can see. Wrapped so it can be re-run after
    // waiting for a late-binding card below.
    auto probeCards = [&]() {
        aplay = execAndReturn("/usr/bin/aplay -l 2>&1");
        lines = split(aplay, '\n');
        cards.clear();
        cardLines.clear();
        hasNonHDMI = false;
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
    };
    probeCards();
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
    // Loading snd-dummy is a one-way door: it takes the next free card number, so
    // a real card that binds a moment later lands behind it and the default
    // selection resolves to Dummy. Before committing, give hardware that is
    // physically here -- declared in the device tree, or a plugged-in USB audio
    // device -- a bounded chance to finish binding. Cape modules are modprobed
    // back in fppinit's cape detect (an earlier systemd unit) and an ASoC card
    // binds synchronously as its last component registers, so this is normally
    // already satisfied -- it only pays out when the driver is still being
    // autoloaded. Gated so a board with neither (a BeagleBone with no audio cape,
    // the common case) waits zero.
    if (noRealSoundcard && countRealAlsaCards() == 0 && (deviceTreeDeclaresSoundCard() || usbAudioDevicePresent())) {
        printf("FPP - No soundcard yet, but audio hardware is present; waiting...\n");
        // Don't just wait on udev -- it may not have autoloaded the CPU DAI or
        // machine driver yet, and without them the card can never bind no matter
        // how long we sit here.
        loadPendingSoundDrivers();
        int waited = 0;
        while (waited < 50 && countRealAlsaCards() == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            ++waited;
        }
        if (countRealAlsaCards() > 0) {
            // The card is registered in the kernel, but "aplay -l" reads
            // /dev/snd/controlC*, which udev creates a beat later (~115ms measured
            // on a BeagleBone). This is the one part udev genuinely owns, so settle
            // for it rather than re-probing into the gap.
            exec("/usr/bin/udevadm settle --timeout=5 > /dev/null 2>&1");
            printf("FPP - Soundcard registered after %d.%ds\n", waited / 10, waited % 10);
            probeCards();
            noRealSoundcard = (!hasNonHDMI || contains(aplay, "no soundcards"));
        } else {
            printf("FPP - No soundcard appeared after %d seconds\n", waited / 10);
        }
    }
    if (noRealSoundcard) {
        printf("FPP - No Soundcard Detected, loading snd-dummy\n");
        modprobe("snd-dummy");
    }
    // Resolve the selected card ID (read above) to the current card number;
    // everything below continues to work with the numeric index.
    // True when the selected card ID names a device that isn't present, so we
    // are running this boot on a fallback card. The selection is the user's and
    // the absence may be transient (a card still probing, a USB device
    // unplugged for the evening), so a fallback must not be written back over
    // it -- doing so silently loses the selection across a reboot.
    bool selectedCardMissing = false;
    int card = 0;
    if (audioOutputId.empty()) {
        card = 0;
    } else if (legacyNumeric) {
        card = std::stoi(audioOutputId);
    } else {
        card = getAlsaCardNumForId(audioOutputId);
        if (card < 0) {
            printf("FPP - Audio device '%s' not currently present; falling back to card 0 for this boot\n", audioOutputId.c_str());
            card = 0;
            selectedCardMissing = true;
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
            // hdmiStatus is keyed by a synthesized connector index ("vc4hdmi0",
            // "vc4hdmi1" on dual-HDMI boards). Single-HDMI boards (e.g. Pi Zero
            // 2 W, Pi 3) register the card as plain "vc4hdmi", which never
            // matches an "NN" index key; the std::map read then yields false and
            // the card is wrongly declared "no HDMI connected" even when a
            // display is attached, causing setupAudio to permanently revert
            // AudioOutput back to card 0. When there is no per-port status for
            // this card, fall back to the any-HDMI-connected signal (same
            // semantics as the legacy bcm2835 HDMI branch below).
            auto it = hdmiStatus.find(cards[k]);
            if (it != hdmiStatus.end()) {
                return !it->second;
            }
            return !anyHDMIConnected;
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
            if (!selectedCardMissing) {
                setRawSetting("AudioOutput", getAlsaCardId(card));
            }
        } else {
            card = cards.size();
            found = true;
            if (!selectedCardMissing) {
                setRawSetting("AudioOutput", getAlsaCardId(card));
            }
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
        if (!selectedCardMissing) {
            setRawSetting("AudioOutput", getAlsaCardId(0));
        }
    } else {
        printf("FPP - Waited for %d seconds for audio device\n", (count / 5));
        // Persist the selection as the stable ALSA card ID. This migrates a
        // legacy numeric value and canonicalizes the stored ID so the next
        // boot matches by name regardless of probe order. Skipped when we're on
        // a fallback card: that would overwrite the user's choice with whatever
        // happened to be present (typically the synthetic Dummy).
        std::string canonicalId = selectedCardMissing ? "" : getAlsaCardId(card);
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
    std::set<std::string> adapterCardNames;     // ...and their ALSA short-names, for the WirePlumber rule
    for (const auto& [key, cardId] : cards) {
        std::string cId = getAlsaCardId(std::stoi(key.substr(5)));
        if (cId.empty()) cId = cardId;
        if (cId == "Dummy" || cardIsDeadHDMI(key)) continue;
        adapterCandidateCids.insert(normalizeCardIdForNode(cId));
        // "card N: <id> [<short name>], device M: ..." -- that first bracketed
        // field is exactly what the ALSA monitor exposes as api.alsa.card.name,
        // which is what ensureWirePlumberAlsaDupeSuppression() matches on.
        if (cardLines.count(key)) {
            const std::string& l = cardLines[key];
            auto b = l.find('[');
            auto e = l.find(']');
            if (b != std::string::npos && e != std::string::npos && e > b) {
                std::string nm = l.substr(b + 1, e - b - 1);
                TrimWhiteSpace(nm);
                // A quote would break out of the generated SPA-JSON string; such a
                // card name has never been seen, but a malformed rules file would
                // take down the whole monitor, so skip rather than risk it.
                if (!nm.empty() && nm.find('"') == std::string::npos) {
                    adapterCardNames.insert(nm);
                }
            }
        }
    }
    std::set<std::string> existingAdapterCids; // fpp_alsa_* sink adapters already in the conf
    std::regex alsaAdapterRe(R"RE(node\.name = "fpp_alsa_([a-z0-9_]+)")RE");
    for (auto it = std::sregex_iterator(existingSinkConf.begin(), existingSinkConf.end(), alsaAdapterRe);
         it != std::sregex_iterator(); ++it) {
        existingAdapterCids.insert((*it)[1].str());
    }
    // A conf written by an older FPP describes the same cards by rules that have
    // since been corrected, and the card-set comparison below cannot see that.
    // Treat a missing or older generation tag as reason enough to re-probe.
    const bool sinkConfGenerationCurrent = startsWith(existingSinkConf, alsaSinkConfGenerationTag());
    if (usePipeWireBackend && !existingSinkConf.empty() && !sinkConfGenerationCurrent) {
        printf("FPP - PipeWire: ALSA sink config predates the current probe rules; re-probing\n");
    }
    bool sinkConfStillValid = !existingSinkConf.empty() && sinkConfGenerationCurrent &&
                              existingAdapterCids == adapterCandidateCids;
    // A conf written before a card's channel-count quirk existed (or while the
    // card was held open at a stale stereo negotiation) can cover all cards yet
    // still pin a known multi-channel card to too few channels.  Verify each
    // quirk card's adapter declares at least the quirk count; otherwise reprobe.
    if (usePipeWireBackend && sinkConfStillValid) {
        for (const auto& [key, cardName] : cards) {
            int quirkCh = cardLines.count(key) ? quirkMinChannelsForCard(cardLines[key]) : 0;
            if (quirkCh <= 0) continue;
            std::string cId = getAlsaCardId(std::stoi(key.substr(5)));
            if (cId.empty()) cId = cardName;
            std::regex chRe("node\\.name = \"fpp_alsa_" + normalizeCardIdForNode(cId) +
                            "\"[\\s\\S]*?audio\\.channels = (\\d+)");
            std::smatch chM;
            if (std::regex_search(existingSinkConf, chM, chRe) && std::stoi(chM[1].str()) < quirkCh) {
                printf("FPP - PipeWire: %s adapter has %s channels but card needs %d; regenerating\n",
                       cId.c_str(), chM[1].str().c_str(), quirkCh);
                sinkConfStillValid = false;
                break;
            }
        }
    }
    // The persisted conf pins the graph clock rate (and each adapter's
    // audio.rate) to whatever AudioFormat was set when it was written.  Changing
    // that setting has to regenerate, which the card-set comparison above cannot
    // see -- the same cards are still present, just at a different rate.
    if (usePipeWireBackend && sinkConfStillValid) {
        std::smatch rateM;
        if (!std::regex_search(existingSinkConf, rateM, std::regex(R"(default\.clock\.rate = (\d+))")) ||
            std::stoi(rateM[1].str()) != pipewireSampleRate) {
            printf("FPP - PipeWire: configured sample rate is now %d; regenerating\n", pipewireSampleRate);
            sinkConfStillValid = false;
        }
    }
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
        // First line, and matched as a prefix by the generation check above.
        pipewireSink << alsaSinkConfGenerationTag() << "\n";
        // Run the graph at the rate the cards are actually configured for.  The
        // daemon default in 90-fpp.conf is 48000, and whenever that differs from
        // the device rate the ALSA sink adapter resamples on every single cycle
        // for as long as the graph runs -- not just while media is playing.
        // Measured on a single-core AM335x board driving a 44100-only I2S cape:
        // sink work per cycle 478us -> 171us, which is ~2 points of the core off
        // both idle and playback.  This file sorts after 90-fpp.conf, so the
        // property set here wins.  allowed-rates is left to 90-fpp.conf, which
        // already lists every rate the switch below can select.
        pipewireSink << "context.properties = {\n"
                     << "    default.clock.rate = " << pipewireSampleRate << "\n"
                     << "}\n";
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
            // Set when hwParams below is synthesised from /proc rather than read
            // from the device, which changes how much the format line is worth.
            bool paramsFromLiveDevice = false;
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
                paramsFromLiveDevice = true;
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
            // Quirk override: known multi-channel I2S cards report a continuous
            // range and would be clamped to stereo above; a busy card can also
            // synthesise a stale 2ch value from /proc.  Either way the quirk
            // count wins (issue #2620: HiFiBerry DAC8x stuck at 2 channels).
            if (cardLines.count(key)) {
                int quirkCh = quirkMinChannelsForCard(cardLines[key]);
                if (quirkCh > maxChannels) {
                    printf("FPP - PipeWire: card %d (%s) quirk raises channels %d -> %d\n",
                           cardNum, cId.c_str(), maxChannels, quirkCh);
                    maxChannels = quirkCh;
                }
            }
            if (maxChannels > 8) maxChannels = 8; // cap at 7.1

            // Detect best audio format from ALSA HW params
            // FORMAT line examples: "FORMAT: S16_LE S24_3LE", "FORMAT: S16_LE S24_LE S32_LE"
            // Priority: S32 > S24 > S16.  ALSA uses _ (S24_3LE), PipeWire drops it (S24LE).
            // A wider format only counts if the card can still reach the target
            // rate in it — fixed-bit-clock I2S cards advertise S32_LE but reach
            // it only at half the rate (see bestFormatForRate).
            std::string audioFormat = "S16LE"; // safe default all cards support
            std::smatch fmtMatch;
            if (paramsFromLiveDevice) {
                // The single format in /proc is not an advertisement, it is what
                // the hardware is running right now -- the very thing the probe
                // exists to establish, already established.  Probing it again is
                // worse than redundant: formatHoldsRate() has to open the device,
                // gets the same EBUSY that forced this synthesis in the first
                // place, and (correctly, by its own bias-to-safe rule) reports
                // "unverifiable" -- silently rewriting a working S32 card as
                // S16LE.  PipeWire holds the card on every postNetwork run, so
                // that is not an edge case, it is the normal path for the
                // selected output; measured on an AM62x PCM5102A cape running
                // S32_LE, which the probe demoted on every regeneration.
                if (std::regex_search(hwParams, fmtMatch, std::regex(R"(FORMAT[^:]*:\s+(\S+))"))) {
                    audioFormat = pipewireFormatName(fmtMatch[1].str());
                }
            } else if (std::regex_search(hwParams, fmtMatch, std::regex(R"(FORMAT[^:]*:\s+(.+))"))) {
                audioFormat = bestFormatForRate(fmtMatch[1].str(), "hw:" + cId,
                                                pipewireSampleRate, maxChannels);
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
    // Stop WirePlumber from double-opening the cards we just declared adapters
    // for.  Runs on the skip-probe path too: the rules file is what makes the
    // static adapters the sole owner of each PCM, and an existing install has
    // never had it written.  Rules are read only at WirePlumber start, so fold a
    // change into sinkConfigChanged to pick up the restart below.
    if (usePipeWireBackend && ensureWirePlumberAlsaDupeSuppression(adapterCardNames)) {
        printf("FPP - PipeWire: suppressing WirePlumber duplicate ALSA devices for %d card(s)\n",
               (int)adapterCardNames.size());
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
    // Upgrade case for boards with no sound card: an install configured by an
    // older FPP has a cached config holding the full hw:Dummy adapter ->
    // filter-chain -> combine-stream graph, which never reaches idle and so
    // cycles PipeWire at the quantum rate forever (~47 wakeups/s, ~4% of a core
    // on a single-core PocketBeagle) for audio that can never be heard.
    // None of the tests above catch it: the cached card ID is still "Dummy" so
    // it matches selCid, and snd-dummy was modprobed earlier this boot so
    // /proc/asound/cards lists [Dummy] and the cards-present test passes too.
    // The config is therefore never regenerated and the board keeps the old
    // graph on every boot.  Force exactly one regeneration when the active conf
    // predates the null-sink form.  Self-limiting: the regenerated conf contains
    // support.null-audio-sink, so this cannot drive a regen/restart loop.
    if (!simpleConfigStale && selCid == "Dummy" &&
        GetFileContents("/etc/pipewire/pipewire.conf.d/97-fpp-audio-groups.conf")
                .find("support.null-audio-sink") == std::string::npos) {
        printf("FPP - No sound card: audio config predates the null sink, regenerating\n");
        simpleConfigStale = true;
    }
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
    // Note the unlinks are all evaluated -- no short-circuiting -- so every stale
    // file goes even if an earlier one was already absent.
    bool hadLegacyAES67 = (unlink("/etc/pipewire/pipewire.conf.d/96-fpp-aes67-rtp.conf") == 0);
    hadLegacyAES67 |= (unlink("/etc/pipewire/pipewire.conf.d/96-fpp-aes67-sap.conf") == 0);
    hadLegacyAES67 |= (unlink("/etc/ptp4l-fpp.conf") == 0);
    // Kill any leftover legacy daemons -- but only when this boot actually found
    // legacy config to remove. The daemons were only ever launched from those
    // files, so once they are gone there is nothing to kill, and two `pkill -f`
    // scans (each a /bin/sh plus a walk of every /proc/*/cmdline) were costing
    // real time on a single-core board on every boot, forever, on the critical
    // path to fppd -- to hunt for processes belonging to a removed feature.
    if (hadLegacyAES67) {
        exec("pkill -f fpp_aes67_sap 2>/dev/null || true");
        exec("pkill -f 'ptp4l.*ptp4l-fpp' 2>/dev/null || true");
    }

    // PipeWire is already running (started at boot with the persisted configs),
    // so only restart it when the config actually changed. A restart is
    // synchronous and triggers an expensive pipewire-pulse/WirePlumber/
    // session-bus cascade, so skipping it on an unchanged boot (always the case
    // on a no-soundcard board) saves ~10-15s on a single-core SBC.
    if (usePipeWireBackend && !runningInDocker) {
        // Keep the FPP PipeWire systemd units current.  install_pipewire.sh
        // copies them once at install time and nothing ever refreshed them
        // afterwards, so unit fixes (e.g. the RestartSec/StartLimitIntervalSec
        // backoff that stops PipeWire from permanently giving up when a USB
        // sound card enumerates a few seconds late) never reached
        // already-installed systems.  Only refresh units that are already
        // installed -- a missing unit means install_pipewire.sh hasn't run yet.
        bool unitsChanged = false;
        for (const char* svc : { "fpp-pipewire.service", "fpp-wireplumber.service", "fpp-pipewire-pulse.service" }) {
            std::string unitSrc = std::string("/opt/fpp/etc/systemd/") + svc;
            std::string unitDst = std::string("/lib/systemd/system/") + svc;
            if (FileExists(unitSrc) && FileExists(unitDst) && GetFileContents(unitSrc) != GetFileContents(unitDst)) {
                printf("FPP - Updating systemd unit %s\n", svc);
                CopyFileContents(unitSrc, unitDst);
                unitsChanged = true;
            }
        }
        if (unitsChanged) {
            exec("/usr/bin/systemctl daemon-reload");
        }

        // The WirePlumber linking hook (blocks rogue default-target fallback
        // links for fpp_* nodes) is needed in ALL PipeWire modes, but was only
        // installed by the Simple-mode C++ path or a groups Apply from the UI.
        // A groups-mode box that never re-applied since the hook shipped runs
        // without it: a member stream whose target is missing (e.g. an fx
        // chain for a disconnected HDMI output) gets fallback-linked to the
        // default sink, double-opening that device and stalling the whole
        // graph mid-playback.  Install it here and restart the stack when it
        // was newly installed (WirePlumber only reads hooks at startup).
        bool linkingHookPresent = FileExists("/usr/share/wireplumber/scripts/linking/fpp-block-combine-fallback.lua")
                               && FileExists("/etc/wireplumber/wireplumber.conf.d/60-fpp-block-combine-fallback.conf");
        if (!linkingHookPresent && ensureWirePlumberLinkingHook()) {
            printf("FPP - Installed WirePlumber FPP linking hook\n");
            audioConfigChanged = true;
        }

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
        // Set when the cheap C++ check below proves the cached group conf is
        // complete, already loaded, and references only present cards. Survives
        // that block so the post-start path can skip re-deriving the same answer
        // with a forked, forced PHP regeneration.
        bool cachedGroupsConfValidated = false;
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
                    // Remember this for the post-start block further down, which
                    // otherwise re-runs the same resolution as a forced PHP regen.
                    cachedGroupsConfValidated = true;
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

        // The skip-restart optimisation below assumes PipeWire is already
        // running (started at boot with the persisted configs).  Verify that
        // assumption: with a pre-backoff unit file, a USB sound card that
        // enumerates a few seconds late makes PipeWire exhaust its start-limit
        // burst and give up early in boot (adapter creation is fatal on a
        // missing ALSA device), leaving the whole stack down.  Skipping the
        // restart then silences all audio even though every config matches.
        std::string pwState = execAndReturn("/usr/bin/systemctl is-active fpp-pipewire.service");
        TrimWhiteSpace(pwState);
        bool pipewireRunning = (pwState == "active");
        // "inactive" here is a COLD START, not the crashed-stack case above: the FPP
        // PipeWire units ship disabled (nothing in the boot transaction pulls them
        // in -- postNetwork is what starts them), so on a stock box pwState is
        // "inactive" every single boot. Conflating that with failure made the
        // skip-restart fast path below dead code on exactly the boards it was
        // written for: every boot took the full restart + 3s settle + forced PHP
        // regenerate + PHP volume restore tail (~11s on a single-core BBB) purely to
        // re-derive a config the cheap C++ validation above had already proven
        // byte-identical and card-complete a few seconds earlier.
        // Starting the daemons now is in fact a *stronger* guarantee than the
        // already-running case: they read the configs that are on disk right now.
        // "failed"/"activating"/"deactivating" remain genuine recovery and still
        // take the slow path.
        bool pipewireColdStart = !pipewireRunning && (pwState == "inactive");
        bool pipewireRestarted = false;
        if (!pipewireRunning && !pipewireColdStart) {
            printf("FPP - PipeWire service is '%s'; recovering audio stack\n", pwState.c_str());
            // Clear any start-limit lockout so the restart below can succeed.
            exec("/usr/bin/systemctl reset-failed fpp-pipewire.service fpp-wireplumber.service fpp-pipewire-pulse.service 2>/dev/null");
        }
        bool pipewireColdStarted = false;
        if (!audioConfigChanged && pipewireRunning) {
            // Config matches what PipeWire loaded at boot -- restarting would
            // change nothing (and the volume-restore below only exists to undo a
            // restart's reset of WirePlumber state, so it's unneeded too).
            printf("FPP - PipeWire audio config unchanged since boot; skipping restart\n");
        } else if (!audioConfigChanged && pipewireColdStart) {
            // Cold start with a config nothing has touched: bring the stack up and
            // stop there. A daemon that has not started yet cannot be holding a
            // stale config, so a restart-to-reload would be pure ceremony.
            exec("/usr/bin/systemctl start fpp-pipewire.service fpp-wireplumber.service fpp-pipewire-pulse.service");
            pipewireColdStarted = true;
            printf("FPP - PipeWire started with the unchanged on-disk config\n");
        } else if (hasGroupsConfig && noRealSoundcard) {
            // Config changed but there's no real sound card: a restart is needed
            // to load it, but there are no USB cards to enumerate and no
            // card-number shifts to resolve, so skip the enumerate/regen dance.
            exec("/usr/bin/systemctl restart fpp-pipewire.service fpp-wireplumber.service fpp-pipewire-pulse.service");
            pipewireRestarted = true;
            printf("FPP - No real sound card present; skipping PipeWire device enumeration/regeneration\n");
        } else {
            exec("/usr/bin/systemctl restart fpp-pipewire.service fpp-wireplumber.service fpp-pipewire-pulse.service");
            pipewireRestarted = true;
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
        //
        // Also skipped when the cheap C++ validation already proved the cached
        // conf byte-identical to what is being loaded and every card it names
        // present: the forced regen re-resolves precisely the card→node mapping
        // that check verified, so it can only ever conclude "unchanged" -- at a
        // cost of 3s of settle plus ~4.5s of PHP on a single-core BBB, on every
        // boot. That was the bulk of a 24s fpp_postnetwork.
        bool audioStackTouched = audioConfigChanged || pipewireRestarted || pipewireColdStarted;
        if (audioStackTouched && hasGroupsConfig && !noRealSoundcard && !cppGeneratedSimpleConfig && !cachedGroupsConfValidated) {
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
        }

        // A board with a real sound card that reaches this point with no sink in
        // the graph is not merely slow: PipeWire aborts context creation and
        // exits when it cannot build a node the config declares, and systemd then
        // restarts it every few seconds forever.  Nothing else notices -- every
        // check above was satisfied by a config that is internally consistent and
        // names only present cards; it is simply one the hardware rejects.  Until
        // now the only trace at this level was a "timed out waiting for sinks"
        // line we then ignored, so the box booted clean and played no audio, on
        // every boot, until someone deleted the file by hand.
        //
        // The graph is the ground truth the config checks cannot reach, so use it:
        // discard the derived config and re-run the setup, which re-probes the
        // hardware from scratch.  Bounded to a single retry by recoveryPass, and
        // cheap where it matters -- a healthy box gets one extra pactl on the
        // first poll, and only a box that is already silent pays the timeout.
        //
        // Only derived files go.  pipewire-audio-groups.json is the user's own
        // configuration and is what the regeneration reads to rebuild the rest.
        if (!noRealSoundcard && (audioStackTouched || pipewireRunning) && !waitForPipeWireSinks(30)) {
            if (!recoveryPass) {
                printf("FPP - PipeWire produced no sinks; discarding the derived audio config and re-probing\n");
                unlink(pipewireSinkConfPath.c_str());
                unlink((FPP_MEDIA_DIR + "/config/pipewire-audio-groups-simple.json").c_str());
                unlink(groupsConfCache.c_str());
                runAudioSetup(true);
                return;
            }
            printf("FPP - PipeWire still has no sinks after re-probing the audio hardware; audio will not work.\n");
            printf("FPP - Check 'journalctl -u fpp-pipewire' for the reason the daemon is failing to start.\n");
        }

        // Per-group/per-member volume restore is needed whenever the stack came up
        // or changed under us. Deliberately NOT gated on the regeneration above:
        // this is orthogonal to whether the config resolved correctly. WirePlumber
        // reapplies its own persisted volume state every time it starts, so a cold
        // start needs the user's configured levels pushed back just as much as a
        // restart does -- gating it on the regen would silently leave a box booting
        // at whatever level WirePlumber last remembered.
        //
        if (audioStackTouched && hasGroupsConfig && !noRealSoundcard && !cppGeneratedSimpleConfig) {
            restorePipeWireVolumes();
        }
    } else if (!runningInDocker) {
        exec("/usr/bin/systemctl stop fpp-pipewire-pulse.service fpp-wireplumber.service fpp-pipewire.service");
    }

    // AES67 is now initialized by AES67Manager in fppd after PipeWire is running.
    // No external daemons to start here.
}

void setupAudio() {
    runAudioSetup(false);
}
