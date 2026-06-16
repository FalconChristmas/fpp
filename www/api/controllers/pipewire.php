<?php
/////////////////////////////////////////////////////////////////////////////
// PipeWire Audio Groups & AES67 API Controller
//
// Manages audio output groups — collections of sound cards/channels that
// form combined PipeWire sinks. Each group becomes a combine-stream module
// instance with its own virtual sink node.
//
// Also manages multi-instance AES67 (audio-over-IP) configurations.
// Each AES67 instance becomes a PipeWire rtp-sink or rtp-source node
// that can be used standalone or as a member of an audio group.
//
// Config files:
//   $mediaDirectory/config/pipewire-audio-groups.json
//   $mediaDirectory/config/pipewire-aes67-instances.json
/////////////////////////////////////////////////////////////////////////////

require_once '../commandsocket.php';

/////////////////////////////////////////////////////////////////////////////
// Helper: Stop fppd playback with a timeout to prevent deadlocks.
// Returns array('wasPlaying' => bool, 'playlist' => string, 'repeat' => bool)
// Uses stream context timeout so PHP doesn't hang if fppd's HTTP handler
// blocks during GStreamer pipeline teardown.
function StopFppdPlaybackSafe($timeoutSec = 3)
{
    $result = array('wasPlaying' => false, 'playlist' => '', 'repeat' => false);

    $ctx = stream_context_create(array('http' => array('timeout' => $timeoutSec)));
    $statusJson = @file_get_contents('http://localhost:32322/fppd/status', false, $ctx);
    if ($statusJson === false)
        return $result;

    $status = json_decode($statusJson, true);
    if (!is_array($status) || !isset($status['status']) || $status['status'] != 1)
        return $result;

    $result['wasPlaying'] = true;
    $cp = isset($status['current_playlist']) ? $status['current_playlist'] : array();
    $result['playlist'] = isset($cp['playlist']) ? $cp['playlist'] : '';
    $result['repeat'] = isset($cp['count']) && $cp['count'] === '0';

    // Stop playback with timeout — if fppd hangs during GStreamer teardown
    // the context timeout prevents PHP from blocking forever.
    @file_get_contents('http://localhost:32322/command/Stop%20Now', false, $ctx);

    // Wait for fppd to release PipeWire streams
    usleep(500000);

    return $result;
}

/////////////////////////////////////////////////////////////////////////////
// Helper: Send a setting to fppd, tolerating a non-responsive daemon.
// Writes to the settings file (always works) then best-effort sends via
// the command socket (1-second timeout built into SendCommand).
function SetFppdSetting($key, $value)
{
    WriteSettingToFile($key, $value);
    // SendCommand may fail if fppd is deadlocked/restarting — that's OK
    // because fppd will read the setting from the file on next pipeline start.
    @SendCommand("setSetting,$key,$value");
}

/////////////////////////////////////////////////////////////////////////////
// GET /api/pipewire/audio/groups
function GetPipeWireAudioGroups()
{
    global $settings;
    $configFile = $settings['mediaDirectory'] . "/config/pipewire-audio-groups.json";

    if (file_exists($configFile)) {
        $data = json_decode(file_get_contents($configFile), true);
        if ($data === null) {
            $data = array("groups" => array());
        }
    } else {
        $data = array("groups" => array());
    }

    return json($data);
}

/////////////////////////////////////////////////////////////////////////////
// POST /api/pipewire/audio/groups
function SavePipeWireAudioGroups()
{
    global $settings;
    $configFile = $settings['mediaDirectory'] . "/config/pipewire-audio-groups.json";

    $data = file_get_contents('php://input');
    $decoded = json_decode($data, true);

    if ($decoded === null) {
        http_response_code(400);
        return json(array("status" => "ERROR", "message" => "Invalid JSON"));
    }

    // Validate structure
    if (!isset($decoded['groups']) || !is_array($decoded['groups'])) {
        http_response_code(400);
        return json(array("status" => "ERROR", "message" => "Missing 'groups' array"));
    }

    // Assign IDs if missing
    $maxId = 0;
    foreach ($decoded['groups'] as &$group) {
        if (isset($group['id']) && $group['id'] > $maxId) {
            $maxId = $group['id'];
        }
    }
    unset($group);
    foreach ($decoded['groups'] as &$group) {
        if (!isset($group['id']) || $group['id'] <= 0) {
            $maxId++;
            $group['id'] = $maxId;
        }
    }
    unset($group);

    $data = json_encode($decoded, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES);
    file_put_contents($configFile, $data);

    // Trigger a JSON Configuration Backup
    GenerateBackupViaAPI('PipeWire audio groups were modified.');

    return json(array("status" => "OK", "data" => $decoded));
}

/////////////////////////////////////////////////////////////////////////////
// POST /api/pipewire/audio/groups/apply
// Generates PipeWire config files and restarts PipeWire services
function ApplyPipeWireAudioGroups($overrideData = null, $skipRestart = false)
{
    global $settings, $SUDO;

    $configFile = $settings['mediaDirectory'] . "/config/pipewire-audio-groups.json";
    $useOverride = ($overrideData !== null);

    if ($useOverride) {
        $data = $overrideData;
    } else {
        if (!file_exists($configFile)) {
            return json(array("status" => "OK", "message" => "No audio groups configured"));
        }
        $data = json_decode(file_get_contents($configFile), true);
    }
    if ($data === null || !isset($data['groups']) || empty($data['groups'])) {
        // Remove any existing combine config
        $confPath = "/etc/pipewire/pipewire.conf.d/97-fpp-audio-groups.conf";
        if (file_exists($confPath)) {
            exec($SUDO . " rm -f " . escapeshellarg($confPath));
        }
        // Remove cached copy too
        $cachedConf = $settings['mediaDirectory'] . "/config/pipewire-audio-groups.conf";
        if (file_exists($cachedConf)) {
            unlink($cachedConf);
        }
        // Restart PipeWire to pick up removal (order matters — pulse depends on pipewire socket)
        if (!$skipRestart) {
            exec($SUDO . " /usr/bin/systemctl restart fpp-pipewire.service 2>&1");
            usleep(500000);
            exec($SUDO . " /usr/bin/systemctl restart fpp-wireplumber.service 2>&1");
            usleep(500000);
            exec($SUDO . " /usr/bin/systemctl restart fpp-pipewire-pulse.service 2>&1");
        }
        return json(array("status" => "OK", "message" => "Audio groups cleared, PipeWire restarted"));
    }

    // Generate PipeWire config
    $genResult = GeneratePipeWireGroupsConfig($data['groups'], true);
    $conf = $genResult['conf'];
    $resolvedCardMap = $genResult['cardNodeMap'];

    // Store resolved PipeWire node names (nodeTarget) back into the JSON
    // so future regenerations (including boot-time) work even when
    // PipeWire hasn't enumerated all devices yet.  WirePlumber node names
    // are deterministic (based on USB VID/PID/serial), so they remain
    // valid across reboots and USB re-plugs.
    $jsonDirty = false;
    foreach ($data['groups'] as &$grp) {
        if (!isset($grp['members']))
            continue;
        foreach ($grp['members'] as &$mbr) {
            $cid = isset($mbr['cardId']) ? $mbr['cardId'] : '';
            if (!empty($cid) && isset($resolvedCardMap[$cid])) {
                $newTarget = $resolvedCardMap[$cid];
                if (!isset($mbr['nodeTarget']) || $mbr['nodeTarget'] !== $newTarget) {
                    $mbr['nodeTarget'] = $newTarget;
                    $jsonDirty = true;
                }
            }
        }
        unset($mbr);
    }
    unset($grp);
    if ($jsonDirty && !$useOverride) {
        $configFile = $settings['mediaDirectory'] . "/config/pipewire-audio-groups.json";
        file_put_contents($configFile, json_encode($data, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES));
    }

    // Ensure directory exists
    exec($SUDO . " /bin/mkdir -p /etc/pipewire/pipewire.conf.d");

    // Simple PipeWire mode has no user-defined input groups — the simple
    // synthetic groups won't have ids matching the advanced input-groups
    // JSON, so skip input-groups regeneration and remove any leftover conf.
    if ($useOverride) {
        $igConfPath = "/etc/pipewire/pipewire.conf.d/96-fpp-input-groups.conf";
        if (file_exists($igConfPath)) {
            exec($SUDO . " rm -f " . escapeshellarg($igConfPath));
        }
        $cachedIgConf = $settings['mediaDirectory'] . "/config/pipewire-input-groups.conf";
        if (file_exists($cachedIgConf)) {
            @unlink($cachedIgConf);
        }
    } else {
        // Also regenerate input group config (96-) so it stays in sync
        $igFile = $settings['mediaDirectory'] . "/config/pipewire-input-groups.json";
        if (file_exists($igFile)) {
            $igData = json_decode(file_get_contents($igFile), true);
            if (is_array($igData) && isset($igData['inputGroups']) && !empty($igData['inputGroups'])) {
                $igConf = GeneratePipeWireInputGroupsConfig($igData['inputGroups'], $data['groups']);
                $igConfPath = "/etc/pipewire/pipewire.conf.d/96-fpp-input-groups.conf";
                $igTmpFile = tempnam(sys_get_temp_dir(), 'fpp_pw_ig_');
                file_put_contents($igTmpFile, $igConf);
                exec($SUDO . " cp " . escapeshellarg($igTmpFile) . " " . escapeshellarg($igConfPath));
                exec($SUDO . " chmod 644 " . escapeshellarg($igConfPath));
                unlink($igTmpFile);
                file_put_contents($settings['mediaDirectory'] . "/config/pipewire-input-groups.conf", $igConf);
            }
        }
    }

    // Write via temp file + sudo cp (directory is root-owned)
    $confPath = "/etc/pipewire/pipewire.conf.d/97-fpp-audio-groups.conf";
    $tmpFile = tempnam(sys_get_temp_dir(), 'fpp_pw_');
    file_put_contents($tmpFile, $conf);
    exec($SUDO . " cp " . escapeshellarg($tmpFile) . " " . escapeshellarg($confPath));
    exec($SUDO . " chmod 644 " . escapeshellarg($confPath));
    unlink($tmpFile);

    // Cache a copy in the media config directory so FPPINIT can restore it at boot
    $cachedConf = $settings['mediaDirectory'] . "/config/pipewire-audio-groups.conf";
    file_put_contents($cachedConf, $conf);

    // Install WirePlumber hook to prevent rogue default-target fallback links.
    // Without this, combine-stream output nodes can get linked to the default
    // sink (e.g. Sound Blaster) in addition to their intended filter-chain
    // targets, causing doubled audio.
    InstallWirePlumberFppLinkingHook($SUDO);

    // When called with $skipRestart=true (e.g. from a MediaBackend mode switch),
    // config files are already written; the caller backgrounds the service restarts
    // so the HTTP response returns immediately.  Write PipeWireSinkName to file
    // now so it is correct on the next fppd start — even before the restarted
    // PipeWire graph is up and SetFppdSetting can be called.
    if ($skipRestart) {
        if ($useOverride) {
            // Simple mode: no input groups, route directly to the first enabled output group.
            $simpleActiveGroup = isset($data['activeGroup']) ? $data['activeGroup'] : '';
            if (empty($simpleActiveGroup)) {
                foreach ($data['groups'] as $grp) {
                    if (isset($grp['enabled']) && $grp['enabled'] && !empty($grp['members'])) {
                        $simpleActiveGroup = "fpp_group_" . preg_replace('/[^a-zA-Z0-9_]/', '_', strtolower($grp['name']));
                        break;
                    }
                }
            }
            if (!empty($simpleActiveGroup)) {
                WriteSettingToFile('PipeWireSinkName', $simpleActiveGroup);
            }
            // Clear any stale per-slot sink names from advanced mode.
            for ($s = 2; $s <= 5; $s++) {
                WriteSettingToFile("PipeWireSinkName_$s", '');
            }
        }
        return;
    }

    // Stop fppd playback before restarting PipeWire to avoid race conditions
    // where WirePlumber creates rogue links to orphaned streams during the
    // service restart window.  Uses timeout to prevent deadlock if fppd's
    // GStreamer teardown blocks on PipeWire.
    $playbackState = StopFppdPlaybackSafe(3);
    $wasPlaying = $playbackState['wasPlaying'];
    $resumePlaylist = $playbackState['playlist'];
    $resumeRepeat = $playbackState['repeat'];

    // Determine the PipeWire sink target BEFORE restarting PipeWire.
    // Write settings to file now so fppd reads them when it creates new
    // pipelines — even if fppd's command socket is temporarily unresponsive.
    $igSlotTargets = array();
    $igSlotGroupCount = array();
    $igSlotSourceIds = array();
    $igFile2 = $settings['mediaDirectory'] . "/config/pipewire-input-groups.json";
    // Simple mode has no input groups — route fppd streams direct to the
    // synthesised group sink. Skip the advanced input-groups lookup.
    if (!$useOverride && file_exists($igFile2)) {
        $igData2 = json_decode(file_get_contents($igFile2), true);
        if (is_array($igData2) && isset($igData2['inputGroups'])) {
            foreach ($igData2['inputGroups'] as $ig) {
                if (!isset($ig['enabled']) || !$ig['enabled'])
                    continue;
                if (!isset($ig['members']) || empty($ig['members']))
                    continue;
                $igNodeName2 = "fpp_input_" . preg_replace('/[^a-zA-Z0-9_]/', '_', strtolower($ig['name']));
                foreach ($ig['members'] as $mbr) {
                    if (isset($mbr['type']) && $mbr['type'] === 'fppd_stream') {
                        $sourceId = isset($mbr['sourceId']) ? $mbr['sourceId'] : 'fppd_stream_1';
                        $slotNum = 1;
                        if (preg_match('/fppd_stream_(\d+)/', $sourceId, $m)) {
                            $slotNum = intval($m[1]);
                        }
                        if (!isset($igSlotGroupCount[$slotNum])) {
                            $igSlotGroupCount[$slotNum] = 0;
                        }
                        $igSlotGroupCount[$slotNum]++;
                        // First input group wins for single-group case
                        if (!isset($igSlotTargets[$slotNum])) {
                            $igSlotTargets[$slotNum] = $igNodeName2;
                            $igSlotSourceIds[$slotNum] = $sourceId;
                        }
                    }
                }
            }
            // Redirect to tee when a stream slot is claimed by multiple groups
            foreach ($igSlotGroupCount as $slotNum => $cnt) {
                if ($cnt > 1 && isset($igSlotSourceIds[$slotNum])) {
                    $sourceId = $igSlotSourceIds[$slotNum];
                    $igSlotTargets[$slotNum] = "fpp_tee_" . preg_replace('/[^a-zA-Z0-9_]/', '_', strtolower($sourceId));
                }
            }
        }
    }

    // Write PipeWireSinkName to file BEFORE PipeWire restart
    if (!empty($igSlotTargets)) {
        $fppdTarget = isset($igSlotTargets[1]) ? $igSlotTargets[1] : '';
        if (!empty($fppdTarget)) {
            WriteSettingToFile('PipeWireSinkName', $fppdTarget);
        }
        for ($s = 2; $s <= 5; $s++) {
            $key = "PipeWireSinkName_$s";
            if (isset($igSlotTargets[$s])) {
                WriteSettingToFile($key, $igSlotTargets[$s]);
            }
        }
    } else {
        $activeGroup = isset($data['activeGroup']) ? $data['activeGroup'] : '';
        if (empty($activeGroup)) {
            foreach ($data['groups'] as $group) {
                if (isset($group['enabled']) && $group['enabled'] && !empty($group['members'])) {
                    $activeGroup = "fpp_group_" . preg_replace('/[^a-zA-Z0-9_]/', '_', strtolower($group['name']));
                    break;
                }
            }
        }
        if (!empty($activeGroup)) {
            WriteSettingToFile('PipeWireSinkName', $activeGroup);
        }
    }

    // Restart PipeWire services to apply (order matters — pulse depends on pipewire socket)
    exec($SUDO . " /usr/bin/systemctl restart fpp-pipewire.service 2>&1");
    usleep(500000);
    exec($SUDO . " /usr/bin/systemctl restart fpp-wireplumber.service 2>&1");
    // Wait for PipeWire core socket to be ready before starting pulse
    for ($i = 0; $i < 10; $i++) {
        if (file_exists('/run/pipewire-fpp/pipewire-0'))
            break;
        usleep(250000);
    }
    exec($SUDO . " /usr/bin/systemctl restart fpp-pipewire-pulse.service 2>&1");

    // Wait for PulseAudio compat socket to be ready
    for ($i = 0; $i < 10; $i++) {
        if (file_exists('/run/pipewire-fpp/pulse/native'))
            break;
        usleep(250000);
    }

    // Restore ALSA hardware mixer levels to 100% for every member card.
    // WirePlumber auto-detects ALSA devices and may restore saved volume
    // state that zeros the hardware mixer, even though our audio chain uses
    // the custom fpp_card sinks.  Setting the mixer to full here prevents
    // silent outputs after Apply / restart.
    foreach ($data['groups'] as $grp) {
        if (!isset($grp['enabled']) || !$grp['enabled'] || empty($grp['members']))
            continue;
        foreach ($grp['members'] as $mbr) {
            $cId = isset($mbr['cardId']) ? $mbr['cardId'] : '';
            if (empty($cId))
                continue;
            // Resolve ALSA card number and mixer controls from /proc/asound
            $cardLinks = glob('/proc/asound/card[0-9]*');
            foreach ($cardLinks as $cl) {
                $cNum = basename($cl);
                $cNum = preg_replace('/^card/', '', $cNum);
                $idLine = @file_get_contents("/proc/asound/card$cNum/id");
                if ($idLine !== false && trim($idLine) === $cId) {
                    // Set every playback mixer on this card to 100%
                    $mixers = array();
                    exec($SUDO . " amixer -c $cNum scontrols 2>/dev/null | cut -f2 -d\"'\"", $mixers);
                    foreach ($mixers as $mx) {
                        $mx = trim($mx);
                        if (!empty($mx) && stripos($mx, 'Mic') === false && stripos($mx, 'Capture') === false) {
                            exec($SUDO . " amixer -c $cNum sset " . escapeshellarg($mx) . " 100% 2>/dev/null");
                        }
                    }
                    break;
                }
            }
        }
    }

    // Set PipeWire default sink and push setting to fppd (best-effort)
    $env = "PIPEWIRE_RUNTIME_DIR=/run/pipewire-fpp XDG_RUNTIME_DIR=/run/pipewire-fpp PULSE_RUNTIME_PATH=/run/pipewire-fpp/pulse";
    if (!empty($igSlotTargets)) {
        $fppdTarget = isset($igSlotTargets[1]) ? $igSlotTargets[1] : '';
        if (!empty($fppdTarget)) {
            exec($SUDO . " " . $env . " pactl set-default-sink " . escapeshellarg($fppdTarget) . " 2>&1");
            SetFppdSetting('PipeWireSinkName', $fppdTarget);
        }
        for ($s = 2; $s <= 5; $s++) {
            $key = "PipeWireSinkName_$s";
            if (isset($igSlotTargets[$s])) {
                SetFppdSetting($key, $igSlotTargets[$s]);
            }
        }
    } else {
        if (!empty($activeGroup)) {
            exec($SUDO . " " . $env . " pactl set-default-sink " . escapeshellarg($activeGroup) . " 2>&1");
            SetFppdSetting('PipeWireSinkName', $activeGroup);
        }
    }

    // Restore configured volume levels to PipeWire sinks.
    // WirePlumber may have restored stale volume state after restart.
    RestorePipeWireGroupVolumes($data['groups']);

    // Resume playback if it was active before the restart
    if ($wasPlaying && !empty($resumePlaylist)) {
        usleep(500000);
        $repeat = $resumeRepeat ? 'true' : 'false';
        $ctx = stream_context_create(array('http' => array('timeout' => 5)));
        @file_get_contents('http://localhost:32322/command/Start%20Playlist/'
            . rawurlencode($resumePlaylist) . '/' . $repeat, false, $ctx);
    }

    return json(array(
        "status" => "OK",
        "message" => "Audio groups applied, PipeWire restarted"
            . ($wasPlaying ? ", playback resumed" : ""),
        "activeGroup" => !empty($igSlotTargets) ? (isset($igSlotTargets[1]) ? $igSlotTargets[1] : '') : (isset($activeGroup) ? $activeGroup : ''),
        "routedViaInputGroup" => !empty($igSlotTargets),
        "restartRequired" => true
    ));
}

/////////////////////////////////////////////////////////////////////////////
// GET /api/pipewire/audio/sinks
// Returns available PipeWire sinks (for volume control targets, etc.)
function GetPipeWireSinks()
{
    global $SUDO;

    $sinks = array();
    $env = "PIPEWIRE_RUNTIME_DIR=/run/pipewire-fpp XDG_RUNTIME_DIR=/run/pipewire-fpp PULSE_RUNTIME_PATH=/run/pipewire-fpp/pulse";

    exec($SUDO . " " . $env . " pactl list sinks short 2>/dev/null", $output, $return_val);
    if (!$return_val && !empty($output)) {
        foreach ($output as $line) {
            $parts = preg_split('/\s+/', trim($line));
            if (count($parts) >= 2) {
                $sinks[] = array(
                    "index" => $parts[0],
                    "name" => $parts[1],
                    "driver" => isset($parts[2]) ? $parts[2] : "",
                    "format" => isset($parts[3]) ? $parts[3] : "",
                    "state" => isset($parts[4]) ? $parts[4] : ""
                );
            }
        }
    }

    return json($sinks);
}

/////////////////////////////////////////////////////////////////////////////
// Helper: Resolve an ALSA card ID (e.g. "S3", "vc4hdmi0") to its current
// card number by reading /proc/asound/<cardId> symlink.
// Returns the card number as int, or -1 if not found.
function ResolveCardIdToNumber($cardId)
{
    $symlink = "/proc/asound/" . $cardId;
    if (is_link($symlink)) {
        $target = readlink($symlink);
        // target is like "card0", "card1", etc.
        if ($target !== false && preg_match('/^card(\d+)$/', $target, $m)) {
            return intval($m[1]);
        }
    }
    // Fallback: scan /proc/asound/cards
    $cardsFile = @file_get_contents('/proc/asound/cards');
    if ($cardsFile) {
        // Format: " 0 [S3             ]: USB-Audio - ..."
        if (preg_match('/^\s*(\d+)\s*\[' . preg_quote($cardId, '/') . '\s/m', $cardsFile, $m)) {
            return intval($m[1]);
        }
    }
    return -1;
}

/////////////////////////////////////////////////////////////////////////////
// Helper: Query a hardware ALSA PCM sink's natively supported sample rates
// and return the best match from $allowedRates (sorted ascending).
//
// We run `aplay --dump-hw-params` which prints ALSA HW parameters even
// when no audio data exists to play.  Typical output:
//   RATE: 48000
//   RATE: { 44100 48000 96000 }
//   RATE: [8000 192000]   (continuous range)
//
// Returns $fallbackRate if the device cannot be queried.
function QueryAlsaCardBestRate($cardId, $allowedRates, $fallbackRate)
{
    // Only alphanumeric + underscore card IDs are safe as hw: path components.
    if (!preg_match('/^[a-zA-Z0-9_]+$/', $cardId)) {
        return $fallbackRate;
    }

    // aplay exits non-zero when /dev/zero has no PCM header, but it still
    // prints the HW params before failing.  Use a short timeout to avoid
    // blocking if the device is busy.
    $out = array();
    exec('timeout 2 aplay -D ' . escapeshellarg('hw:' . $cardId)
        . ' --dump-hw-params /dev/zero 2>&1 | head -40', $out);
    $text = implode("\n", $out);

    $deviceRates = array();
    if (preg_match('/\bRATE\b[^\n]*\{([^}]+)\}/i', $text, $m)) {
        // Discrete list: { 44100 48000 96000 }
        preg_match_all('/\d+/', $m[1], $dm);
        foreach ($dm[0] as $r) {
            $deviceRates[] = intval($r);
        }
    } elseif (preg_match('/\bRATE\b[^\n]*\[(\d+)[^\d]+(\d+)\]/i', $text, $m)) {
        // Continuous range [min max] — accept any allowed rate in range
        $rmin = intval($m[1]);
        $rmax = intval($m[2]);
        foreach ($allowedRates as $r) {
            if ($r >= $rmin && $r <= $rmax) {
                $deviceRates[] = $r;
            }
        }
    } elseif (preg_match('/\bRATE(?:\[\d+\])?:\s+(\d+)/i', $text, $m)) {
        // Single rate
        $deviceRates[] = intval($m[1]);
    }

    if (empty($deviceRates)) {
        return $fallbackRate;
    }

    // Prefer the PipeWire graph clock rate (48000) when the device supports
    // it — matching the graph clock avoids a graph-wide resample and is the
    // safe choice for onboard analog DACs (e.g. Pi bcm2835), which advertise
    // a continuous 8000–192000 range but are happiest at the graph rate.
    // Only step up to a higher rate if 48000 is not supported.
    if (in_array(48000, $deviceRates)) {
        return 48000;
    }
    // Otherwise pick the highest allowed rate the device supports.
    foreach (array_reverse($allowedRates) as $ar) {
        if (in_array($ar, $deviceRates)) {
            return $ar;
        }
    }
    return $fallbackRate;
}

/////////////////////////////////////////////////////////////////////////////
// GET /api/pipewire/audio/cards
// Returns available ALSA cards with channel info for group membership UI.
// Uses stable ALSA card IDs (from /proc/asound/) as primary identifiers
// instead of card numbers which can change between reboots.
function GetPipeWireAudioCards()
{
    global $SUDO, $settings;

    $cards = array();

    // User-defined sound card aliases (issue #2586) keyed by ALSA card ID
    $audioCardAliases = LoadAudioCardAliases();

    // Query running PipeWire sinks to map to actual node names
    $pwSinkNames = array(); // substring -> full node name
    $pwEnv = "PIPEWIRE_RUNTIME_DIR=/run/pipewire-fpp XDG_RUNTIME_DIR=/run/pipewire-fpp PULSE_RUNTIME_PATH=/run/pipewire-fpp/pulse";
    exec($SUDO . " " . $pwEnv . " pactl list sinks short 2>/dev/null", $sinkLines);
    if (!empty($sinkLines)) {
        foreach ($sinkLines as $sl) {
            $sp = preg_split('/\s+/', trim($sl));
            if (count($sp) >= 2) {
                $sName = $sp[1];
                // Index by identifiable substrings: e.g. alsa_output.usb-Creative_...
                // Extract the middle portion after "alsa_output."
                if (preg_match('/^alsa_output\.(.+?)\.[^.]+$/', $sName, $sm)) {
                    $pwSinkNames[$sm[1]] = $sName;
                }
                // Also index by full name for fpp_card patterns
                if (preg_match('/alsa_output\.(fpp_card\d+)/', $sName, $sm)) {
                    $pwSinkNames[$sm[1]] = $sName;
                }
            }
        }
    }
    unset($sinkLines);

    // Build a map of PipeWire card identifier -> best output profile name
    // This lets us derive expected sink names for cards without active sinks
    // Card name: alsa_card.{identifier}  ->  Sink name: alsa_output.{identifier}.{profile}
    $pwCardProfiles = array(); // identifier -> profile name (e.g. "hdmi-stereo", "analog-stereo")
    $cardOutput = array();
    exec($SUDO . " " . $pwEnv . " pactl list cards 2>/dev/null", $cardOutput);
    $currentCardId = '';
    $currentProfiles = array();
    $currentCardBusPath = '';
    foreach ($cardOutput as $cline) {
        if (preg_match('/^\s+Name:\s+alsa_card\.(.+)/', $cline, $cm)) {
            // Save previous card's profiles
            if ($currentCardId !== '' && !empty($currentProfiles)) {
                $pwCardProfiles[$currentCardId] = $currentProfiles;
            }
            $currentCardId = trim($cm[1]);
            $currentProfiles = array();
            $currentCardBusPath = '';
        }
        // Track bus path for type detection
        if (preg_match('/device\.bus_path\s*=\s*"(.+)"/', $cline, $bm)) {
            $currentCardBusPath = trim($bm[1]);
        }
        // Collect output profiles: "output:hdmi-stereo: Digital Stereo (HDMI) Output (sinks: 1, ...)"
        if (preg_match('/^\s+output:([^\s:]+).*\(sinks:\s*(\d+)/', $cline, $pm)) {
            if (intval($pm[2]) > 0) {
                $currentProfiles[] = $pm[1];
            }
        }
        // If card has pro-audio but no output: profiles (e.g. disconnected HDMI),
        // infer a profile from the bus path / card identifier
        if (preg_match('/^\s+Active Profile:\s+(.+)/', $cline, $am)) {
            if (empty($currentProfiles) && !empty($currentCardId)) {
                // HDMI cards: bus path contains ".hdmi" -> would be hdmi-stereo when connected
                if (preg_match('/\.hdmi$/', $currentCardId) || preg_match('/\.hdmi$/', $currentCardBusPath)) {
                    $currentProfiles[] = 'hdmi-stereo';
                }
                // USB/analog cards would typically be analog-stereo
                elseif (preg_match('/^usb-/', $currentCardId)) {
                    $currentProfiles[] = 'analog-stereo';
                }
            }
        }
    }
    if ($currentCardId !== '' && !empty($currentProfiles)) {
        $pwCardProfiles[$currentCardId] = $currentProfiles;
    }
    unset($cardOutput);

    exec($SUDO . " aplay -l 2>/dev/null | grep '^card'", $output, $return_val);

    // Build by-path and by-id lookup tables for stable hardware identifiers
    $byPathMap = array();  // controlCN -> path string
    $byIdMap = array();    // controlCN -> id string
    exec("ls -la /dev/snd/by-path/ 2>/dev/null", $pathOutput);
    if (!empty($pathOutput)) {
        foreach ($pathOutput as $pline) {
            // lrwxrwxrwx 1 root root 12 Jan  9 15:08 platform-xhci-hcd.1-usb-0:1:1.0 -> ../controlC0
            if (preg_match('/([^\s]+)\s+->\s+\.\.\/controlC(\d+)/', $pline, $pm)) {
                $byPathMap['controlC' . $pm[2]] = $pm[1];
            }
        }
    }
    unset($pathOutput);
    exec("ls -la /dev/snd/by-id/ 2>/dev/null", $idOutput);
    if (!empty($idOutput)) {
        foreach ($idOutput as $iline) {
            if (preg_match('/([^\s]+)\s+->\s+\.\.\/controlC(\d+)/', $iline, $im)) {
                $byIdMap['controlC' . $im[2]] = $im[1];
            }
        }
    }
    unset($idOutput);

    // Build direct alsa-card-number → PipeWire-node-name map via pw-dump.
    // This is more reliable than by-id/by-path heuristics for identical USB
    // cards where Linux only assigns one by-id symlink (e.g. two ICUSBAUDIO7D
    // get one by-id entry pointing to one of them, leaving the other unresolvable).
    $pwSinkByAlsaCardNum = array(); // alsa card number (int) => PW sink node name
    $pwSinkByAlsaId = array(); // ALSA card ID string => PW sink node name (for fpp_alsa_* sinks)
    $pwDumpOutput = shell_exec($SUDO . ' ' . $pwEnv . ' pw-dump 2>/dev/null');
    if ($pwDumpOutput) {
        $pwObjects = json_decode($pwDumpOutput, true);
        if (is_array($pwObjects)) {
            foreach ($pwObjects as $pwObj) {
                $pwProps = isset($pwObj['info']['props']) ? $pwObj['info']['props'] : null;
                if (!$pwProps)
                    continue;
                $pwClass = isset($pwProps['media.class']) ? $pwProps['media.class'] : '';
                if ($pwClass !== 'Audio/Sink')
                    continue;
                $pwName = isset($pwProps['node.name']) ? $pwProps['node.name'] : '';
                if ($pwName === '')
                    continue;
                // Strategy 1: WirePlumber-created sinks have alsa.card property
                $pwAlsaCard = isset($pwProps['alsa.card']) ? strval($pwProps['alsa.card']) : '';
                if ($pwAlsaCard !== '') {
                    $pwCardNumInt = intval($pwAlsaCard);
                    // Prefer non-fpp_fx sinks (raw card sinks over filter-chain nodes)
                    if (
                        !isset($pwSinkByAlsaCardNum[$pwCardNumInt]) ||
                        strpos($pwName, 'fpp_fx') === false
                    ) {
                        $pwSinkByAlsaCardNum[$pwCardNumInt] = $pwName;
                    }
                }
                // Strategy 2: FPP-created sinks (fpp_alsa_{cardIdNorm} or legacy
                // alsa_output.fpp_card{N}) — resolve via api.alsa.path or name pattern.
                if (strpos($pwName, 'fpp_alsa_') === 0) {
                    // Resolve card number from api.alsa.path (e.g. "hw:S3" → card 5)
                    $fppAlsaPath = isset($pwProps['api.alsa.path']) ? $pwProps['api.alsa.path'] : '';
                    if (preg_match('/^hw:(.+)$/', $fppAlsaPath, $hwM)) {
                        $fppCardNum = ResolveCardIdToNumber(trim($hwM[1]));
                        if ($fppCardNum >= 0) {
                            $pwSinkByAlsaCardNum[$fppCardNum] = $pwName;
                        }
                    }
                } elseif (preg_match('/^alsa_output\.fpp_card(\d+)$/', $pwName, $fppMatch)) {
                    $pwCardNumInt = intval($fppMatch[1]);
                    // Legacy FPP sinks take priority
                    $pwSinkByAlsaCardNum[$pwCardNumInt] = $pwName;
                }
                // Strategy 3: FPP-created ALSA adapter sinks (fpp_alsa_*) have
                // api.alsa.path = "hw:{cardId}" — index by ALSA card ID so we
                // can resolve them in the per-card loop below.
                if (strpos($pwName, 'fpp_alsa_') === 0) {
                    $alsaPath = isset($pwProps['api.alsa.path']) ? $pwProps['api.alsa.path'] : '';
                    if (preg_match('/^hw:(.+)$/', $alsaPath, $hwMatch)) {
                        $pwSinkByAlsaId[trim($hwMatch[1])] = $pwName;
                    }
                }
            }
        }
    }

    if (!$return_val && !empty($output)) {
        $seenCards = array();
        foreach ($output as $line) {
            // Parse: card 0: S3 [Sound Blaster Play! 3], device 0: USB Audio [USB Audio]
            if (preg_match('/^card (\d+):\s*(.+?)\s*\[([^\]]+)\],\s*device\s*(\d+):\s*(.+?)\s*\[([^\]]+)\]/', $line, $matches)) {
                $cardNum = $matches[1];
                $cardId = $matches[2];
                $cardName = $matches[3];
                $deviceNum = $matches[4];
                $deviceId = $matches[5];
                $deviceName = $matches[6];

                if (!isset($seenCards[$cardId])) {
                    $seenCards[$cardId] = true;

                    // Get channel info for this card
                    $channels = 2; // Default stereo
                    $channelInfo = array();
                    exec($SUDO . " amixer -c $cardNum scontrols 2>/dev/null | cut -f2 -d\"'\"", $mixerOutput, $mixerRet);
                    if (!$mixerRet && !empty($mixerOutput)) {
                        foreach ($mixerOutput as $mixer) {
                            $channelInfo[] = trim($mixer);
                        }
                    }
                    unset($mixerOutput);

                    // Try to detect channel count from hw params
                    exec("cat /proc/asound/card$cardNum/pcm0p/sub0/hw_params 2>/dev/null", $hwOutput, $hwRet);
                    if (!$hwRet && !empty($hwOutput)) {
                        foreach ($hwOutput as $hwLine) {
                            if (preg_match('/^channels:\s*(\d+)/', trim($hwLine), $chMatch)) {
                                $channels = intval($chMatch[1]);
                            }
                        }
                    }
                    unset($hwOutput);

                    // Also check codec info for max channels
                    exec("cat /proc/asound/card$cardNum/codec* 2>/dev/null | grep -i 'max channels' | head -1", $codecOutput);
                    if (!empty($codecOutput)) {
                        if (preg_match('/(\d+)/', $codecOutput[0], $chMatch)) {
                            $maxCh = intval($chMatch[1]);
                            if ($maxCh > $channels) {
                                $channels = $maxCh;
                            }
                        }
                    }
                    unset($codecOutput);

                    // Stable identifiers
                    $controlKey = 'controlC' . $cardNum;
                    $byPath = isset($byPathMap[$controlKey]) ? $byPathMap[$controlKey] : '';
                    $byId = isset($byIdMap[$controlKey]) ? $byIdMap[$controlKey] : '';

                    // Resolve actual PipeWire sink node name
                    $pwNodeName = '';
                    // Determine the PipeWire card identifier for this ALSA card
                    $pwCardIdentifier = '';

                    // PRIMARY: pw-dump alsa.card → node.name mapping.
                    // Most reliable for identical USB cards where by-id symlinks
                    // may only exist for one of the two devices.
                    if (isset($pwSinkByAlsaCardNum[intval($cardNum)])) {
                        $pwNodeName = $pwSinkByAlsaCardNum[intval($cardNum)];
                    }

                    // FALLBACK: by-id / by-path heuristics (for cards not resolved above)
                    if (empty($pwNodeName)) {
                        if ($byId && isset($pwSinkNames[$byId])) {
                            $pwNodeName = $pwSinkNames[$byId];
                            $pwCardIdentifier = $byId;
                        } elseif ($byPath) {
                            // Strip trailing -audio if present for matching
                            $byPathBase = preg_replace('/-audio$/', '', $byPath);
                            if (isset($pwSinkNames[$byPathBase])) {
                                $pwNodeName = $pwSinkNames[$byPathBase];
                                $pwCardIdentifier = $byPathBase;
                            } elseif (isset($pwSinkNames[$byPath])) {
                                $pwNodeName = $pwSinkNames[$byPath];
                                $pwCardIdentifier = $byPath;
                            } else {
                                $pwCardIdentifier = $byPathBase;
                            }
                        }
                        // Fallback: try fpp_card pattern
                        if (empty($pwNodeName) && isset($pwSinkNames['fpp_card' . $cardNum])) {
                            $pwNodeName = $pwSinkNames['fpp_card' . $cardNum];
                        }
                        // Fallback: try fpp_alsa_* adapter by ALSA card ID
                        if (empty($pwNodeName) && isset($pwSinkByAlsaId[$cardId])) {
                            $pwNodeName = $pwSinkByAlsaId[$cardId];
                        }
                        // If still no active sink, derive expected sink name from PipeWire card profiles
                        // Card: alsa_card.{id} -> Sink: alsa_output.{id}.{profile}
                        if (empty($pwNodeName) && !empty($pwCardIdentifier) && isset($pwCardProfiles[$pwCardIdentifier])) {
                            $profiles = $pwCardProfiles[$pwCardIdentifier];
                            if (!empty($profiles)) {
                                $pwNodeName = 'alsa_output.' . $pwCardIdentifier . '.' . $profiles[0];
                            }
                        }
                    }

                    $cards[] = array(
                        "cardNum" => intval($cardNum),
                        "cardId" => $cardId,
                        "cardName" => $cardName,
                        "device" => intval($deviceNum),
                        "deviceName" => $deviceName,
                        "channels" => $channels,
                        "mixerControls" => $channelInfo,
                        "alsaPath" => "hw:" . $cardNum,
                        "byPath" => $byPath,
                        "byId" => $byId,
                        "pwNodeName" => $pwNodeName,
                        "alias" => isset($audioCardAliases[$cardId]) ? $audioCardAliases[$cardId] : ""
                    );
                }
            }
        }
    }

    // --- Also include AES67 virtual sinks as selectable cards ---
    $aes67File = $settings['mediaDirectory'] . "/config/pipewire-aes67-instances.json";
    if (file_exists($aes67File)) {
        $aes67Data = json_decode(file_get_contents($aes67File), true);
        if ($aes67Data && isset($aes67Data['instances']) && is_array($aes67Data['instances'])) {
            foreach ($aes67Data['instances'] as $inst) {
                if (!isset($inst['enabled']) || !$inst['enabled'])
                    continue;
                $mode = isset($inst['mode']) ? $inst['mode'] : 'send';
                // Only sinks (send mode) can be used as group members
                if ($mode !== 'send' && $mode !== 'both')
                    continue;

                $nodeSafeName = 'aes67_' . preg_replace('/[^a-zA-Z0-9_]/', '_', strtolower($inst['name']));
                $sinkNodeName = $nodeSafeName . '_send';
                $instChannels = isset($inst['channels']) ? intval($inst['channels']) : 2;

                // Try to find actual PipeWire node name from running sinks
                $pwNodeName = '';
                if (!empty($sinkLines)) {
                    // sinkLines may have been unset, so re-query
                }
                // Search the running sinks for this node
                $sinkSearch = array();
                exec($SUDO . " " . $pwEnv . " pactl list sinks short 2>/dev/null | grep " . escapeshellarg($sinkNodeName), $sinkSearch);
                if (!empty($sinkSearch)) {
                    $sp = preg_split('/\s+/', trim($sinkSearch[0]));
                    if (count($sp) >= 2)
                        $pwNodeName = $sp[1];
                }

                $cards[] = array(
                    "cardNum" => -1,
                    "cardId" => 'aes67_' . $inst['id'],
                    "cardName" => $inst['name'] . ' (AES67 Send)',
                    "device" => 0,
                    "deviceName" => "AES67 RTP Sink",
                    "channels" => $instChannels,
                    "mixerControls" => array(),
                    "alsaPath" => "",
                    "byPath" => "",
                    "byId" => "",
                    "pwNodeName" => !empty($pwNodeName) ? $pwNodeName : $sinkNodeName,
                    "isAES67" => true,
                    "aes67InstanceId" => $inst['id'],
                    "multicastIP" => isset($inst['multicastIP']) ? $inst['multicastIP'] : '',
                    "port" => isset($inst['port']) ? $inst['port'] : 5004
                );
            }
        }
    }

    // --- Also include Opus RTP virtual sinks as selectable cards ---
    $opusFile = $settings['mediaDirectory'] . "/config/pipewire-opus-rtp-instances.json";
    if (file_exists($opusFile)) {
        $opusData = json_decode(file_get_contents($opusFile), true);
        if ($opusData && isset($opusData['instances']) && is_array($opusData['instances'])) {
            foreach ($opusData['instances'] as $inst) {
                if (!isset($inst['enabled']) || !$inst['enabled'])
                    continue;
                $mode = isset($inst['mode']) ? $inst['mode'] : 'send';
                if ($mode !== 'send' && $mode !== 'both')
                    continue;

                $nodeSafeName = 'opusrtp_' . preg_replace('/[^a-zA-Z0-9_]/', '_', strtolower($inst['name']));
                $sinkNodeName = $nodeSafeName . '_send';
                $instChannels = isset($inst['channels']) ? intval($inst['channels']) : 2;

                $pwNodeName = '';
                $sinkSearch = array();
                exec($SUDO . " " . $pwEnv . " pactl list sinks short 2>/dev/null | grep " . escapeshellarg($sinkNodeName), $sinkSearch);
                if (!empty($sinkSearch)) {
                    $sp = preg_split('/\s+/', trim($sinkSearch[0]));
                    if (count($sp) >= 2)
                        $pwNodeName = $sp[1];
                }

                $cards[] = array(
                    "cardNum" => -1,
                    "cardId" => 'opusrtp_' . $inst['id'],
                    "cardName" => $inst['name'] . ' (Opus RTP Send)',
                    "device" => 0,
                    "deviceName" => "Opus RTP Sink",
                    "channels" => $instChannels,
                    "mixerControls" => array(),
                    "alsaPath" => "",
                    "byPath" => "",
                    "byId" => "",
                    "pwNodeName" => !empty($pwNodeName) ? $pwNodeName : $sinkNodeName,
                    "isOpusRTP" => true,
                    "opusrtpInstanceId" => $inst['id'],
                    "destIP" => isset($inst['destIP']) ? $inst['destIP'] : '',
                    "port" => isset($inst['port']) ? $inst['port'] : 5005
                );
            }
        }
    }

    return json($cards);
}

/////////////////////////////////////////////////////////////////////////////
// POST /api/pipewire/audio/group/volume
// Set volume for a specific group or member sink
function SetPipeWireGroupVolume()
{
    global $SUDO;

    $data = json_decode(file_get_contents('php://input'), true);
    if (!isset($data['sink']) || !isset($data['volume'])) {
        http_response_code(400);
        return json(array("status" => "ERROR", "message" => "Missing sink or volume"));
    }

    $sink = escapeshellarg($data['sink']);
    $volume = intval($data['volume']);
    if ($volume < 0)
        $volume = 0;
    if ($volume > 150)
        $volume = 150;

    $env = "PIPEWIRE_RUNTIME_DIR=/run/pipewire-fpp XDG_RUNTIME_DIR=/run/pipewire-fpp PULSE_RUNTIME_PATH=/run/pipewire-fpp/pulse";

    exec($SUDO . " " . $env . " pactl set-sink-volume $sink {$volume}% 2>&1", $output, $return_val);

    if ($return_val) {
        return json(array("status" => "ERROR", "message" => "Failed to set volume", "output" => implode("\n", $output)));
    }

    return json(array("status" => "OK"));
}

/////////////////////////////////////////////////////////////////////////////
// POST /api/pipewire/audio/eq/update
// Real-time EQ parameter update via pw-cli set-param
// Adjusts running filter-chain biquad controls without restarting PipeWire
function UpdatePipeWireEQRealtime()
{
    global $SUDO;

    $data = json_decode(file_get_contents('php://input'), true);
    $groupId = isset($data['groupId']) ? intval($data['groupId']) : 0;
    $cardId = isset($data['cardId']) ? $data['cardId'] : '';
    $bands = isset($data['bands']) ? $data['bands'] : array();
    $channels = isset($data['channels']) ? intval($data['channels']) : 2;

    if (empty($cardId) || empty($bands)) {
        http_response_code(400);
        return json(array("status" => "ERROR", "message" => "Missing cardId or bands"));
    }

    $nodeId = FindFXFilterChainNodeId($groupId, $cardId);

    if ($nodeId === null) {
        // Filter-chain not running — needs Apply first
        return json(array("status" => "NOT_RUNNING", "message" => "EQ filter not active — Save & Apply first"));
    }

    // Build named control key-value pairs for pw-cli set-param.
    // Filter-chain exposes params as: "eq_<ch>_<band>:Freq", "eq_<ch>_<band>:Q", "eq_<ch>_<band>:Gain"
    $channelLabels = array("l", "r", "c", "lfe", "rl", "rr", "sl", "sr");
    $numCh = min($channels, count($channelLabels));

    $paramPairs = array();
    for ($ch = 0; $ch < $numCh; $ch++) {
        $chLabel = $channelLabels[$ch];
        foreach ($bands as $bi => $band) {
            $prefix = "eq_{$chLabel}_{$bi}";
            $freq = floatval(isset($band['freq']) ? $band['freq'] : 1000);
            $q = floatval(isset($band['q']) ? $band['q'] : 1.0);
            $gain = floatval(isset($band['gain']) ? $band['gain'] : 0);
            $paramPairs[] = "\"$prefix:Freq\" $freq";
            $paramPairs[] = "\"$prefix:Q\" $q";
            $paramPairs[] = "\"$prefix:Gain\" $gain";
        }
    }

    $paramStr = implode(' ', $paramPairs);
    $env = "PIPEWIRE_RUNTIME_DIR=/run/pipewire-fpp XDG_RUNTIME_DIR=/run/pipewire-fpp";
    $cmd = $SUDO . " " . $env . " pw-cli set-param " . intval($nodeId) . " Props '{ params = [ $paramStr ] }' 2>&1";
    exec($cmd, $output, $ret);

    if ($ret) {
        return json(array("status" => "ERROR", "message" => "pw-cli set-param failed", "output" => implode("\n", $output)));
    }

    return json(array("status" => "OK"));
}

/////////////////////////////////////////////////////////////////////////////
// POST /api/pipewire/audio/delay/update
// Real-time delay adjustment via pw-cli set-param
// Adjusts running filter-chain delay controls without restarting PipeWire
function UpdatePipeWireDelayRealtime()
{
    global $SUDO;

    $data = json_decode(file_get_contents('php://input'), true);
    $groupId = isset($data['groupId']) ? intval($data['groupId']) : 0;
    $cardId = isset($data['cardId']) ? $data['cardId'] : '';
    $delayMs = isset($data['delayMs']) ? floatval($data['delayMs']) : 0;
    $channels = isset($data['channels']) ? intval($data['channels']) : 2;

    if (empty($cardId)) {
        http_response_code(400);
        return json(array("status" => "ERROR", "message" => "Missing cardId"));
    }

    $nodeId = FindFXFilterChainNodeId($groupId, $cardId);

    if ($nodeId === null) {
        return json(array("status" => "NOT_RUNNING", "message" => "Filter chain not active — Save & Apply first"));
    }

    $delaySec = max(0, $delayMs / 1000.0);
    $channelLabels = array("l", "r", "c", "lfe", "rl", "rr", "sl", "sr");
    $numCh = min($channels, count($channelLabels));

    $paramPairs = array();
    for ($ch = 0; $ch < $numCh; $ch++) {
        $chLabel = $channelLabels[$ch];
        $paramPairs[] = "\"delay_{$chLabel}:Delay (s)\" $delaySec";
    }

    $paramStr = implode(' ', $paramPairs);
    $env = "PIPEWIRE_RUNTIME_DIR=/run/pipewire-fpp XDG_RUNTIME_DIR=/run/pipewire-fpp";
    $cmd = $SUDO . " " . $env . " pw-cli set-param " . intval($nodeId) . " Props '{ params = [ $paramStr ] }' 2>&1";
    exec($cmd, $output, $ret);

    if ($ret) {
        return json(array("status" => "ERROR", "message" => "pw-cli set-param failed", "output" => implode("\n", $output)));
    }

    return json(array("status" => "OK"));
}

/////////////////////////////////////////////////////////////////////////////
// Helper: resolve the PipeWire combine-sink node name for an audio group
// by index (as ordered in pipewire-audio-groups.json).
function GetSyncCalibrationSinkForGroup($groupIndex)
{
    global $settings;

    $configFile = $settings['mediaDirectory'] . "/config/pipewire-audio-groups.json";
    if (!file_exists($configFile)) {
        return null;
    }
    $data = json_decode(file_get_contents($configFile), true);
    if (!is_array($data) || !isset($data['groups']) || !is_array($data['groups'])) {
        return null;
    }
    if (!isset($data['groups'][$groupIndex])) {
        return null;
    }
    $group = $data['groups'][$groupIndex];
    if (!isset($group['name']) || trim($group['name']) === '') {
        return null;
    }
    // Mirror the JS EscapeNodeName(): lowercase, non [a-z0-9_] -> _
    $norm = preg_replace('/[^a-z0-9_]/', '_', strtolower($group['name']));
    return "fpp_group_" . $norm;
}

/////////////////////////////////////////////////////////////////////////////
// POST /api/pipewire/audio/sync/start
// Start sync calibration mode: generate a click track and play it on loop
// directly to the selected group's combine sink (bypassing fppd routing).
function StartSyncCalibration()
{
    global $settings;

    $data = json_decode(file_get_contents('php://input'), true);
    $groupIndex = isset($data['groupIndex']) ? intval($data['groupIndex']) : 0;
    $mediaFile = isset($data['mediaFile']) ? trim($data['mediaFile']) : '';

    $sinkName = GetSyncCalibrationSinkForGroup($groupIndex);
    if (!$sinkName) {
        return json(array("status" => "ERROR", "message" => "Could not resolve target sink for group index " . $groupIndex . " — make sure the group has been saved/applied."));
    }

    // Stop any existing calibration playback first
    StopSyncCalibrationInternal();

    // If the user picked a media file, play that to the group sink; otherwise
    // generate (if needed) and loop the click track.
    if ($mediaFile !== '') {
        // Resolve the file under the music directory; reject anything escaping it.
        $musicDir = $settings['mediaDirectory'] . "/music";
        $resolved = realpath($musicDir . "/" . $mediaFile);
        if ($resolved === false || strpos($resolved, realpath($musicDir)) !== 0 || !is_file($resolved)) {
            return json(array("status" => "ERROR", "message" => "Media file not found: " . $mediaFile));
        }
        return StartSyncCalibrationPlayback($sinkName, $resolved, false);
    }

    $clickFile = $settings['mediaDirectory'] . "/music/fpp_sync_click.wav";
    if (!file_exists($clickFile)) {
        // Generate a 60-second WAV with alternating high/low clicks for easier sync matching
        $cmd = "ffmpeg -y -f lavfi -i \"sine=frequency=1000:duration=0.02,apad=pad_dur=0.98\""
            . " -f lavfi -i \"sine=frequency=600:duration=0.02,apad=pad_dur=0.98\""
            . " -filter_complex \"[0][1]concat=n=2:v=0:a=1,aloop=loop=29:size=88200\""
            . " -t 60 " . escapeshellarg($clickFile) . " 2>&1";
        exec($cmd, $genOutput, $genRet);
        if ($genRet !== 0) {
            $cmd = "sox -n " . escapeshellarg($clickFile) . " synth 60 sine 1000 pad 0 0.98 repeat 59 2>&1";
            exec($cmd, $genOutput2, $genRet2);
            if ($genRet2 !== 0) {
                return json(array("status" => "ERROR", "message" => "Failed to generate click track (tried ffmpeg and sox)"));
            }
        }
    }

    return StartSyncCalibrationPlayback($sinkName, $clickFile, true);
}

// Spawn pw-cat (fed by ffmpeg for non-WAV files) targeted at the group sink.
// $loop: if true, restart playback indefinitely until stopped.
function StartSyncCalibrationPlayback($sinkName, $absFile, $loop)
{
    global $SUDO;

    $pidFile = "/tmp/fpp_sync_cal.pid";
    $logFile = "/tmp/fpp_sync_cal.log";

    $env = "PIPEWIRE_RUNTIME_DIR=/run/pipewire-fpp XDG_RUNTIME_DIR=/run/pipewire-fpp";
    $sinkArg = escapeshellarg($sinkName);
    $fileArg = escapeshellarg($absFile);

    // Decode any container/codec via ffmpeg, pipe raw WAV into pw-cat targeted
    // at the group's combine sink.  Wrap in a shell loop for indefinite repeat.
    $playOnce = "ffmpeg -hide_banner -loglevel error -nostdin -i $fileArg -f wav - 2>>" . escapeshellarg($logFile)
        . " | $env pw-cat --playback --target $sinkArg - 2>>" . escapeshellarg($logFile);

    if ($loop) {
        $inner = "while [ -f " . escapeshellarg($pidFile) . " ]; do $playOnce; sleep 0.1; done";
    } else {
        $inner = $playOnce;
    }

    // Launch via setsid in background; record the supervisor PID so Stop can kill the whole group.
    $shell = "setsid sh -c " . escapeshellarg($inner) . " >/dev/null 2>>" . escapeshellarg($logFile) . " & echo \$!";
    $cmd = $SUDO . " $env sh -c " . escapeshellarg($shell);

    $pid = trim(shell_exec($cmd));
    if (!ctype_digit($pid)) {
        return json(array("status" => "ERROR", "message" => "Failed to start playback to sink '$sinkName'"));
    }
    @file_put_contents($pidFile, $pid);

    return json(array(
        "status" => "OK",
        "message" => "Sync calibration started on sink $sinkName",
        "sink" => $sinkName,
        "pid" => intval($pid),
    ));
}

/////////////////////////////////////////////////////////////////////////////
// POST /api/pipewire/audio/sync/stop
// Stop sync calibration playback
function StopSyncCalibration()
{
    StopSyncCalibrationInternal();
    return json(array("status" => "OK", "message" => "Sync calibration stopped"));
}

function StopSyncCalibrationInternal()
{
    global $SUDO;

    $pidFile = "/tmp/fpp_sync_cal.pid";

    // Remove PID file first so the loop script exits its `while` after the
    // current iteration even if signals race.
    $pid = null;
    if (file_exists($pidFile)) {
        $pid = intval(trim(@file_get_contents($pidFile)));
        @unlink($pidFile);
    }

    if ($pid > 0) {
        // Kill the entire process group (setsid'd) — supervisor + ffmpeg + pw-cat.
        @exec($SUDO . " kill -TERM -" . $pid . " 2>/dev/null");
        usleep(150000);
        @exec($SUDO . " kill -KILL -" . $pid . " 2>/dev/null");
    }

    // Belt-and-suspenders: nuke any straggler pw-cat/ffmpeg fed by us.
    @exec($SUDO . " pkill -f 'pw-cat --playback --target fpp_group_' 2>/dev/null");

    // Also stop any legacy fppd-driven click track in case an older session
    // started one before this fix shipped.
    $url = 'http://localhost/api/command/Stop%20Media/fpp_sync_click.wav';
    $ctx = stream_context_create(array('http' => array('method' => 'GET', 'timeout' => 5)));
    @file_get_contents($url, false, $ctx);
}

/////////////////////////////////////////////////////////////////////////////
// Helper: Find the PipeWire node ID for a member's filter-chain
// Looks for "fpp_fx_g<groupId>_<cardId>" first, falls back to legacy "fpp_eq_g<groupId>_<cardId>"
function FindFXFilterChainNodeId($groupId, $cardId)
{
    global $SUDO;

    $cardIdNorm = preg_replace('/[^a-zA-Z0-9_]/', '_', strtolower($cardId));
    $fxNodeName = "fpp_fx_g" . intval($groupId) . "_" . $cardIdNorm;
    // Legacy name for backward compatibility
    $eqNodeName = "fpp_eq_g" . intval($groupId) . "_" . $cardIdNorm;

    $env = "PIPEWIRE_RUNTIME_DIR=/run/pipewire-fpp XDG_RUNTIME_DIR=/run/pipewire-fpp";
    $pwOutput = array();
    exec($SUDO . " " . $env . " pw-cli list-objects Node 2>/dev/null", $pwOutput);

    $nodeId = null;
    $currentId = null;
    foreach ($pwOutput as $line) {
        if (preg_match('/^\s+id (\d+),/', $line, $m)) {
            $currentId = $m[1];
        }
        if (preg_match('/node\.name\s*=\s*"(.+?)"/', $line, $m)) {
            if ($m[1] === $fxNodeName || $m[1] === $eqNodeName) {
                $nodeId = $currentId;
                break;
            }
        }
    }

    return $nodeId;
}

/////////////////////////////////////////////////////////////////////////////
// Helper: Install WirePlumber Lua hook that prevents default-target fallback
// for FPP combine-stream and filter-chain output nodes.
// Without this hook, WirePlumber may create rogue links from combine outputs
// to the default ALSA sink (e.g. Sound Blaster), causing doubled audio, and
// may link filter-chain outputs back to the combine sink, creating loops.
function InstallWirePlumberFppLinkingHook($SUDO)
{
    // The hook is shipped as static files in the repo (/opt/fpp/etc) and copied
    // into place at image build; this just (re)deploys them into WirePlumber's
    // search paths. Kept in sync with FPPINIT's C++ boot path
    // (ensureWirePlumberLinkingHook), which installs the same files at boot.
    $luaSrc = "/opt/fpp/etc/wireplumber/scripts/linking/fpp-block-combine-fallback.lua";
    $luaPath = "/usr/share/wireplumber/scripts/linking/fpp-block-combine-fallback.lua";
    if (file_exists($luaSrc)) {
        exec($SUDO . " /bin/mkdir -p /usr/share/wireplumber/scripts/linking");
        exec($SUDO . " cp " . escapeshellarg($luaSrc) . " " . escapeshellarg($luaPath));
        exec($SUDO . " chmod 644 " . escapeshellarg($luaPath));
    }

    $confSrc = "/opt/fpp/etc/wireplumber/wireplumber.conf.d/60-fpp-block-combine-fallback.conf";
    $wpConfPath = "/etc/wireplumber/wireplumber.conf.d/60-fpp-block-combine-fallback.conf";
    if (file_exists($confSrc)) {
        exec($SUDO . " /bin/mkdir -p /etc/wireplumber/wireplumber.conf.d");
        exec($SUDO . " cp " . escapeshellarg($confSrc) . " " . escapeshellarg($wpConfPath));
        exec($SUDO . " chmod 644 " . escapeshellarg($wpConfPath));
    }
}

/////////////////////////////////////////////////////////////////////////////
//  INPUT GROUPS (MIX BUSES)
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// GET /api/pipewire/audio/input-groups
function GetPipeWireInputGroups()
{
    global $settings;
    $configFile = $settings['mediaDirectory'] . "/config/pipewire-input-groups.json";

    if (file_exists($configFile)) {
        $data = json_decode(file_get_contents($configFile), true);
        if ($data === null) {
            $data = array("inputGroups" => array());
        }
    } else {
        $data = array("inputGroups" => array());
    }

    return json($data);
}

/////////////////////////////////////////////////////////////////////////////
// POST /api/pipewire/audio/input-groups
function SavePipeWireInputGroups()
{
    global $settings;
    $configFile = $settings['mediaDirectory'] . "/config/pipewire-input-groups.json";

    $data = file_get_contents('php://input');
    $decoded = json_decode($data, true);

    if ($decoded === null) {
        http_response_code(400);
        return json(array("status" => "ERROR", "message" => "Invalid JSON"));
    }

    // Validate structure
    if (!isset($decoded['inputGroups']) || !is_array($decoded['inputGroups'])) {
        http_response_code(400);
        return json(array("status" => "ERROR", "message" => "Missing 'inputGroups' array"));
    }

    // Assign IDs if missing
    $maxId = 0;
    foreach ($decoded['inputGroups'] as &$group) {
        if (isset($group['id']) && $group['id'] > $maxId) {
            $maxId = $group['id'];
        }
    }
    unset($group);
    foreach ($decoded['inputGroups'] as &$group) {
        if (!isset($group['id']) || $group['id'] <= 0) {
            $maxId++;
            $group['id'] = $maxId;
        }
    }
    unset($group);

    $data = json_encode($decoded, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES);
    file_put_contents($configFile, $data);

    // Trigger a JSON Configuration Backup
    GenerateBackupViaAPI('PipeWire input groups were modified.');

    return json(array("status" => "OK", "data" => $decoded));
}

/////////////////////////////////////////////////////////////////////////////
// POST /api/pipewire/audio/input-groups/apply
// Generates PipeWire input group config and restarts PipeWire services
function ApplyPipeWireInputGroups($skipRestart = false)
{
    global $settings, $SUDO;

    $configFile = $settings['mediaDirectory'] . "/config/pipewire-input-groups.json";
    $confPath = "/etc/pipewire/pipewire.conf.d/96-fpp-input-groups.conf";
    $cachedConf = $settings['mediaDirectory'] . "/config/pipewire-input-groups.conf";

    if (!file_exists($configFile)) {
        // No input groups — clean up and reapply output groups only
        if (file_exists($confPath)) {
            exec($SUDO . " rm -f " . escapeshellarg($confPath));
        }
        if (file_exists($cachedConf)) {
            unlink($cachedConf);
        }
        // Restart PipeWire
        if (!$skipRestart) {
            exec($SUDO . " /usr/bin/systemctl restart fpp-pipewire.service 2>&1");
            usleep(500000);
            exec($SUDO . " /usr/bin/systemctl restart fpp-wireplumber.service 2>&1");
            usleep(500000);
            exec($SUDO . " /usr/bin/systemctl restart fpp-pipewire-pulse.service 2>&1");
        }
        return json(array("status" => "OK", "message" => "Input groups cleared, PipeWire restarted"));
    }

    $data = json_decode(file_get_contents($configFile), true);
    if ($data === null || !isset($data['inputGroups']) || empty($data['inputGroups'])) {
        // Remove any existing config
        if (file_exists($confPath)) {
            exec($SUDO . " rm -f " . escapeshellarg($confPath));
        }
        if (file_exists($cachedConf)) {
            unlink($cachedConf);
        }
        if (!$skipRestart) {
            exec($SUDO . " /usr/bin/systemctl restart fpp-pipewire.service 2>&1");
            usleep(500000);
            exec($SUDO . " /usr/bin/systemctl restart fpp-wireplumber.service 2>&1");
            usleep(500000);
            exec($SUDO . " /usr/bin/systemctl restart fpp-pipewire-pulse.service 2>&1");
        }
        return json(array("status" => "OK", "message" => "Input groups cleared, PipeWire restarted"));
    }

    // Load output groups config to determine routing
    $outputGroupsFile = $settings['mediaDirectory'] . "/config/pipewire-audio-groups.json";
    $outputGroups = array();
    if (file_exists($outputGroupsFile)) {
        $ogData = json_decode(file_get_contents($outputGroupsFile), true);
        if (is_array($ogData) && isset($ogData['groups'])) {
            $outputGroups = $ogData['groups'];
        }
    }

    // Generate PipeWire config
    $conf = GeneratePipeWireInputGroupsConfig($data['inputGroups'], $outputGroups);

    // Ensure directory exists
    exec($SUDO . " /bin/mkdir -p /etc/pipewire/pipewire.conf.d");

    // Write via temp file + sudo cp
    $tmpFile = tempnam(sys_get_temp_dir(), 'fpp_pw_ig_');
    file_put_contents($tmpFile, $conf);
    exec($SUDO . " cp " . escapeshellarg($tmpFile) . " " . escapeshellarg($confPath));
    exec($SUDO . " chmod 644 " . escapeshellarg($confPath));
    unlink($tmpFile);

    // Cache a copy
    file_put_contents($cachedConf, $conf);

    // Update WirePlumber hook to include input group patterns
    InstallWirePlumberFppLinkingHook($SUDO);

    // Stop fppd playback before restarting PipeWire (with timeout protection)
    $playbackState = StopFppdPlaybackSafe(3);
    $wasPlaying = $playbackState['wasPlaying'];
    $resumePlaylist = $playbackState['playlist'];
    $resumeRepeat = $playbackState['repeat'];

    // Build slot targets and write settings to file BEFORE PipeWire restart.
    // fppd reads PipeWireSinkName from settings file when creating new
    // pipelines, so this ensures the correct target even if fppd's command
    // socket is temporarily unresponsive.
    $slotTargets = array();
    $slotGroupCount = array();
    $slotSourceIds = array();
    foreach ($data['inputGroups'] as $ig) {
        if (!isset($ig['enabled']) || !$ig['enabled'])
            continue;
        if (!isset($ig['members']) || empty($ig['members']))
            continue;
        $igNodeName = "fpp_input_" . preg_replace('/[^a-zA-Z0-9_]/', '_', strtolower($ig['name']));
        foreach ($ig['members'] as $mbr) {
            if (isset($mbr['type']) && $mbr['type'] === 'fppd_stream') {
                $sourceId = isset($mbr['sourceId']) ? $mbr['sourceId'] : 'fppd_stream_1';
                $slotNum = 1;
                if (preg_match('/fppd_stream_(\d+)/', $sourceId, $m)) {
                    $slotNum = intval($m[1]);
                }
                // Track how many groups claim this slot
                if (!isset($slotGroupCount[$slotNum])) {
                    $slotGroupCount[$slotNum] = 0;
                }
                $slotGroupCount[$slotNum]++;
                // First input group wins for single-group case
                if (!isset($slotTargets[$slotNum])) {
                    $slotTargets[$slotNum] = $igNodeName;
                    $slotSourceIds[$slotNum] = $sourceId;
                }
            }
        }
    }
    // If a stream slot is claimed by multiple groups, redirect fppd to
    // the null-sink tee instead of the first input group directly.
    foreach ($slotGroupCount as $slotNum => $cnt) {
        if ($cnt > 1 && isset($slotSourceIds[$slotNum])) {
            $sourceId = $slotSourceIds[$slotNum];
            $slotTargets[$slotNum] = "fpp_tee_" . preg_replace('/[^a-zA-Z0-9_]/', '_', strtolower($sourceId));
        }
    }

    // Write PipeWireSinkName to file BEFORE PipeWire restart
    $fppdTarget = isset($slotTargets[1]) ? $slotTargets[1] : '';
    if (!empty($fppdTarget)) {
        WriteSettingToFile('PipeWireSinkName', $fppdTarget);
    }
    for ($s = 2; $s <= 5; $s++) {
        $key = "PipeWireSinkName_$s";
        if (isset($slotTargets[$s])) {
            WriteSettingToFile($key, $slotTargets[$s]);
        } else {
            WriteSettingToFile($key, '');
        }
    }

    // Restart PipeWire services (skipped when $skipRestart=true — caller backgrounds it)
    if (!$skipRestart) {
        exec($SUDO . " /usr/bin/systemctl restart fpp-pipewire.service 2>&1");
        usleep(500000);
        exec($SUDO . " /usr/bin/systemctl restart fpp-wireplumber.service 2>&1");
        for ($i = 0; $i < 10; $i++) {
            if (file_exists('/run/pipewire-fpp/pipewire-0'))
                break;
            usleep(250000);
        }
        exec($SUDO . " /usr/bin/systemctl restart fpp-pipewire-pulse.service 2>&1");
        for ($i = 0; $i < 10; $i++) {
            if (file_exists('/run/pipewire-fpp/pulse/native'))
                break;
            usleep(250000);
        }
    }

    // Set PipeWire default sink and push setting to fppd (best-effort)
    if (!empty($fppdTarget)) {
        $env = "PIPEWIRE_RUNTIME_DIR=/run/pipewire-fpp XDG_RUNTIME_DIR=/run/pipewire-fpp PULSE_RUNTIME_PATH=/run/pipewire-fpp/pulse";
        exec($SUDO . " " . $env . " pactl set-default-sink " . escapeshellarg($fppdTarget) . " 2>&1");
        SetFppdSetting('PipeWireSinkName', $fppdTarget);
    }
    for ($s = 2; $s <= 5; $s++) {
        $key = "PipeWireSinkName_$s";
        if (isset($slotTargets[$s])) {
            SetFppdSetting($key, $slotTargets[$s]);
        } else {
            SetFppdSetting($key, '');
        }
    }

    // Resume playback if it was active
    if ($wasPlaying && !empty($resumePlaylist)) {
        usleep(500000);
        $repeat = $resumeRepeat ? 'true' : 'false';
        $ctx = stream_context_create(array('http' => array('timeout' => 5)));
        @file_get_contents('http://localhost:32322/command/Start%20Playlist/'
            . rawurlencode($resumePlaylist) . '/' . $repeat, false, $ctx);
    }

    return json(array(
        "status" => "OK",
        "message" => "Input groups applied, PipeWire restarted"
            . ($wasPlaying ? ", playback resumed" : ""),
        "fppdTarget" => $fppdTarget,
        "restartRequired" => true
    ));
}

/////////////////////////////////////////////////////////////////////////////
// POST /api/pipewire/audio/input-groups/volume
// Real-time volume control for input group loopback nodes
// Body: { "groupId": 1, "memberIndex": 0, "volume": 75 }
// Sets channelmix.volume on the running PipeWire loopback node without restart
function SetInputGroupMemberVolume()
{
    global $SUDO, $settings;

    $body = json_decode(file_get_contents('php://input'), true);
    if (!$body || !isset($body['groupId']) || !isset($body['memberIndex']) || !isset($body['volume'])) {
        return json(array("status" => "error", "message" => "Missing groupId, memberIndex, or volume"));
    }

    $groupId = intval($body['groupId']);
    $memberIndex = intval($body['memberIndex']);
    $volumePct = max(0, min(100, intval($body['volume'])));
    $volumeLinear = round($volumePct / 100.0, 3);

    // Load input groups config to resolve the node name
    $configFile = $settings['mediaDirectory'] . "/config/pipewire-input-groups.json";
    if (!file_exists($configFile)) {
        return json(array("status" => "error", "message" => "No input groups configured"));
    }

    $data = json_decode(file_get_contents($configFile), true);
    if (!is_array($data) || !isset($data['inputGroups'])) {
        return json(array("status" => "error", "message" => "Invalid input groups config"));
    }

    // Find the input group and member
    $targetGroup = null;
    foreach ($data['inputGroups'] as $ig) {
        if (isset($ig['id']) && intval($ig['id']) === $groupId) {
            $targetGroup = $ig;
            break;
        }
    }

    if (!$targetGroup) {
        return json(array("status" => "error", "message" => "Input group $groupId not found"));
    }

    if (!isset($targetGroup['members'][$memberIndex])) {
        return json(array("status" => "error", "message" => "Member index $memberIndex not found"));
    }

    $mbr = $targetGroup['members'][$memberIndex];
    $mbrType = isset($mbr['type']) ? $mbr['type'] : '';

    // Build the expected loopback node name (must match GeneratePipeWireInputGroupsConfig)
    $groupName = isset($targetGroup['name']) ? $targetGroup['name'] : "Input Group";
    $mbrName = isset($mbr['name']) ? $mbr['name'] : "Member $memberIndex";

    if ($mbrType === 'fppd_stream') {
        // fppd_stream members: primary group has no loopback (volume via fppd),
        // non-primary groups have a fan-out loopback whose name uses sourceId
        $sourceId = isset($mbr['sourceId']) ? $mbr['sourceId'] : 'fppd_stream_1';
        $streamSlug = preg_replace('/[^a-zA-Z0-9_]/', '_', strtolower($sourceId));
        $loopbackNodeName = "fpp_loopback_ig{$groupId}_{$streamSlug}";
    } else {
        $loopbackNodeName = "fpp_loopback_ig{$groupId}_" . preg_replace('/[^a-zA-Z0-9_]/', '_', strtolower($mbrName));
    }

    // Find the PipeWire node ID for this loopback
    $env = "PIPEWIRE_RUNTIME_DIR=/run/pipewire-fpp XDG_RUNTIME_DIR=/run/pipewire-fpp";
    $raw = shell_exec($SUDO . " " . $env . " pw-dump 2>/dev/null");
    if (empty($raw)) {
        return json(array("status" => "error", "message" => "Cannot connect to PipeWire"));
    }

    $objects = json_decode($raw, true);
    if (!is_array($objects)) {
        return json(array("status" => "error", "message" => "Invalid PipeWire dump"));
    }

    // Find all nodes that belong to this loopback (capture + playback sides)
    // PipeWire loopback modules create sub-nodes named input.NAME and output.NAME
    // (there is no bare parent node), so we match both patterns.
    $nodeIds = array();
    foreach ($objects as $obj) {
        $type = isset($obj['type']) ? $obj['type'] : '';
        if ($type !== 'PipeWire:Interface:Node')
            continue;
        $props = isset($obj['info']['props']) ? $obj['info']['props'] : array();
        $nm = isset($props['node.name']) ? $props['node.name'] : '';
        if (
            $nm === $loopbackNodeName ||
            $nm === 'input.' . $loopbackNodeName ||
            $nm === 'output.' . $loopbackNodeName
        ) {
            $nodeIds[] = $obj['id'];
        }
    }

    if (empty($nodeIds)) {
        if ($mbrType === 'fppd_stream') {
            // Primary group — no loopback exists; volume controlled via fppd
            return json(array("status" => "error", "message" => "This stream's primary group volume is controlled via fppd, not PipeWire loopback"));
        }
        return json(array("status" => "error", "message" => "Loopback node '$loopbackNodeName' not found in PipeWire (is it muted or not applied?)"));
    }

    // Set volume on the playback side using pw-cli set-param
    // The channelmix.volume prop is on the node's Props param
    $success = false;
    foreach ($nodeIds as $nid) {
        $cmd = $SUDO . " " . $env . " pw-cli set-param $nid Props '{ channelmix.volume: $volumeLinear }' 2>&1";
        $output = shell_exec($cmd);
        if (strpos($output, 'Error') === false) {
            $success = true;
        }
    }

    // Also update the saved config for persistence
    // If this is a mute toggle, persist the mute flag but don't overwrite the saved volume
    $isMuteToggle = isset($body['mute']);
    foreach ($data['inputGroups'] as &$ig) {
        if (isset($ig['id']) && intval($ig['id']) === $groupId) {
            if ($isMuteToggle) {
                $ig['members'][$memberIndex]['mute'] = (bool) $body['mute'];
            } else {
                $ig['members'][$memberIndex]['volume'] = $volumePct;
                $ig['members'][$memberIndex]['mute'] = false;
            }
            break;
        }
    }
    unset($ig);
    file_put_contents($configFile, json_encode($data, JSON_PRETTY_PRINT));

    return json(array(
        "status" => $success ? "OK" : "error",
        "message" => $success ? "Volume set to {$volumePct}% on $loopbackNodeName" : "Failed to set volume on PipeWire node",
        "nodeName" => $loopbackNodeName,
        "volume" => $volumePct,
        "volumeLinear" => $volumeLinear
    ));
}

/////////////////////////////////////////////////////////////////////////////
// POST /api/pipewire/audio/stream/volume
// Set volume on a specific fppd stream slot (1-5).
// Body: { "slot": 1, "volume": 80 }
// Uses fppd's own volume control via HTTP command API.
function SetStreamSlotVolume()
{
    $body = json_decode(file_get_contents('php://input'), true);
    if (!$body || !isset($body['slot']) || !isset($body['volume'])) {
        return json(array("status" => "error", "message" => "Missing slot or volume"));
    }

    $slot = max(1, min(5, intval($body['slot'])));
    $volume = max(0, min(100, intval($body['volume'])));

    // Use fppd's volume command — for slot 1 this maps to the global volume
    // For other slots, fppd must handle per-slot volume via StreamSlotManager
    $url = "http://127.0.0.1:32322/api/command";
    $cmd = array(
        "command" => "Volume Set",
        "args" => array(strval($volume))
    );

    // For slot > 1, use the stream-slot-specific endpoint
    if ($slot > 1) {
        // Direct PipeWire volume control on the stream node
        $env = "PIPEWIRE_RUNTIME_DIR=/run/pipewire-fpp XDG_RUNTIME_DIR=/run/pipewire-fpp";
        $nodeName = "fppd_stream_$slot";

        // Find node ID for this stream
        global $SUDO;
        $raw = shell_exec($SUDO . " " . $env . " pw-dump 2>/dev/null");
        if (!empty($raw)) {
            $objects = json_decode($raw, true);
            if (is_array($objects)) {
                $volumeLinear = round($volume / 100.0, 3);
                foreach ($objects as $obj) {
                    $type = isset($obj['type']) ? $obj['type'] : '';
                    if ($type !== 'PipeWire:Interface:Node')
                        continue;
                    $props = isset($obj['info']['props']) ? $obj['info']['props'] : array();
                    $nm = isset($props['node.name']) ? $props['node.name'] : '';
                    if ($nm === $nodeName) {
                        $cmd2 = $SUDO . " " . $env . " pw-cli set-param " . $obj['id'] . " Props '{ channelmix.volume: $volumeLinear }' 2>&1";
                        shell_exec($cmd2);
                        return json(array("status" => "OK", "slot" => $slot, "volume" => $volume));
                    }
                }
            }
        }
        return json(array("status" => "error", "message" => "Stream node $nodeName not found in PipeWire"));
    }

    // Slot 1: use fppd's built-in volume command
    $ch = curl_init($url);
    curl_setopt($ch, CURLOPT_POST, 1);
    curl_setopt($ch, CURLOPT_POSTFIELDS, json_encode($cmd));
    curl_setopt($ch, CURLOPT_HTTPHEADER, array('Content-Type: application/json'));
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($ch, CURLOPT_TIMEOUT, 3);
    $result = curl_exec($ch);
    curl_close($ch);

    return json(array("status" => "OK", "slot" => $slot, "volume" => $volume));
}

/////////////////////////////////////////////////////////////////////////////
// GET /api/pipewire/audio/stream/status
// Returns status for all 5 stream slots (active/idle, media filename, timing).
function GetStreamSlotStatus()
{
    $result = array();

    // Query fppd status
    $ch = curl_init("http://127.0.0.1:32322/api/fppd/status");
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($ch, CURLOPT_TIMEOUT, 3);
    $raw = curl_exec($ch);
    curl_close($ch);

    $fppStatus = !empty($raw) ? json_decode($raw, true) : array();

    // Build slot status from PipeWire graph
    global $SUDO;
    $env = "PIPEWIRE_RUNTIME_DIR=/run/pipewire-fpp XDG_RUNTIME_DIR=/run/pipewire-fpp";
    $pwRaw = shell_exec($SUDO . " " . $env . " pw-dump 2>/dev/null");
    $pwObjects = !empty($pwRaw) ? json_decode($pwRaw, true) : array();

    for ($slot = 1; $slot <= 5; $slot++) {
        $nodeName = "fppd_stream_$slot";
        $nodeDesc = "FPP Media Stream $slot";
        $slotInfo = array(
            "slot" => $slot,
            "nodeName" => $nodeName,
            "nodeDescription" => $nodeDesc,
            "status" => "idle",
            "mediaFilename" => "",
        );

        // Check if the PipeWire node exists (i.e. stream is active)
        if (is_array($pwObjects)) {
            foreach ($pwObjects as $obj) {
                $type = isset($obj['type']) ? $obj['type'] : '';
                if ($type !== 'PipeWire:Interface:Node')
                    continue;
                $props = isset($obj['info']['props']) ? $obj['info']['props'] : array();
                $nm = isset($props['node.name']) ? $props['node.name'] : '';
                if ($nm === $nodeName) {
                    $slotInfo['status'] = 'playing';
                    break;
                }
            }
        }

        // Slot 1 gets extra info from fppd status
        if ($slot === 1 && !empty($fppStatus['media_filename'])) {
            $slotInfo['status'] = 'playing';
            $slotInfo['mediaFilename'] = $fppStatus['media_filename'];
            if (isset($fppStatus['seconds_elapsed']))
                $slotInfo['secondsElapsed'] = intval($fppStatus['seconds_elapsed']);
            if (isset($fppStatus['seconds_remaining']))
                $slotInfo['secondsRemaining'] = intval($fppStatus['seconds_remaining']);
        }

        $result[] = $slotInfo;
    }

    return json($result);
}

/////////////////////////////////////////////////////////////////////////////
// GET /api/pipewire/audio/routing
// Returns the full routing matrix: input groups × output groups with per-path settings
function GetRoutingMatrix()
{
    global $settings;

    $inputGroupsFile = $settings['mediaDirectory'] . "/config/pipewire-input-groups.json";
    $outputGroupsFile = $settings['mediaDirectory'] . "/config/pipewire-audio-groups.json";

    $inputGroups = array();
    if (file_exists($inputGroupsFile)) {
        $data = json_decode(file_get_contents($inputGroupsFile), true);
        if (is_array($data) && isset($data['inputGroups'])) {
            $inputGroups = $data['inputGroups'];
        }
    }

    $outputGroups = array();
    if (file_exists($outputGroupsFile)) {
        $data = json_decode(file_get_contents($outputGroupsFile), true);
        if (is_array($data) && isset($data['groups'])) {
            $outputGroups = $data['groups'];
        }
    }

    // Build matrix
    $matrix = array();
    foreach ($inputGroups as $ig) {
        if (!isset($ig['enabled']) || !$ig['enabled'])
            continue;
        $igId = isset($ig['id']) ? intval($ig['id']) : 0;
        $outputs = isset($ig['outputs']) ? $ig['outputs'] : array();
        $routing = isset($ig['routing']) ? $ig['routing'] : array();
        $hasEffects = isset($ig['effects']['eq']['enabled']) && $ig['effects']['eq']['enabled']
            && isset($ig['effects']['eq']['bands']) && !empty($ig['effects']['eq']['bands']);

        $paths = array();
        foreach ($outputGroups as $og) {
            if (!isset($og['enabled']) || !$og['enabled'])
                continue;
            $ogId = isset($og['id']) ? intval($og['id']) : 0;
            $connected = in_array($ogId, $outputs);
            $pathKey = strval($ogId);
            $volume = 100;
            $mute = false;
            if (isset($routing[$pathKey])) {
                $volume = isset($routing[$pathKey]['volume']) ? intval($routing[$pathKey]['volume']) : 100;
                $mute = isset($routing[$pathKey]['mute']) && $routing[$pathKey]['mute'];
            }
            $paths[] = array(
                'outputGroupId' => $ogId,
                'outputGroupName' => isset($og['name']) ? $og['name'] : 'Group ' . $ogId,
                'connected' => $connected,
                'volume' => $volume,
                'mute' => $mute
            );
        }

        $matrix[] = array(
            'inputGroupId' => $igId,
            'inputGroupName' => isset($ig['name']) ? $ig['name'] : 'Input ' . $igId,
            'channels' => isset($ig['channels']) ? intval($ig['channels']) : 2,
            'hasEffects' => $hasEffects,
            'effects' => isset($ig['effects']) ? $ig['effects'] : array(),
            'paths' => $paths
        );
    }

    return json(array(
        'inputGroups' => array_map(function ($ig) {
            return array(
                'id' => isset($ig['id']) ? intval($ig['id']) : 0,
                'name' => isset($ig['name']) ? $ig['name'] : '',
                'enabled' => isset($ig['enabled']) && $ig['enabled']
            );
        }, $inputGroups),
        'outputGroups' => array_map(function ($og) {
            return array(
                'id' => isset($og['id']) ? intval($og['id']) : 0,
                'name' => isset($og['name']) ? $og['name'] : '',
                'enabled' => isset($og['enabled']) && $og['enabled']
            );
        }, $outputGroups),
        'matrix' => $matrix
    ));
}

/////////////////////////////////////////////////////////////////////////////
// POST /api/pipewire/audio/routing
// Save routing matrix (per-path volume, mute, connections)
// Body: { "routes": [{ "inputGroupId": 1, "outputGroupId": 2, "connected": true, "volume": 80, "mute": false }] }
function SaveRoutingMatrix()
{
    global $settings;
    $configFile = $settings['mediaDirectory'] . "/config/pipewire-input-groups.json";

    $body = json_decode(file_get_contents('php://input'), true);
    if (!$body || !isset($body['routes'])) {
        http_response_code(400);
        return json(array("status" => "error", "message" => "Missing routes array"));
    }

    if (!file_exists($configFile)) {
        return json(array("status" => "error", "message" => "No input groups configured"));
    }

    $data = json_decode(file_get_contents($configFile), true);
    if (!is_array($data) || !isset($data['inputGroups'])) {
        return json(array("status" => "error", "message" => "Invalid input groups config"));
    }

    // Group routes by input group
    $routesByIg = array();
    foreach ($body['routes'] as $route) {
        $igId = intval($route['inputGroupId']);
        if (!isset($routesByIg[$igId])) {
            $routesByIg[$igId] = array();
        }
        $routesByIg[$igId][] = $route;
    }

    // Update each input group's outputs and routing
    foreach ($data['inputGroups'] as &$ig) {
        $igId = isset($ig['id']) ? intval($ig['id']) : 0;
        if (!isset($routesByIg[$igId]))
            continue;

        $outputs = array();
        $routing = array();
        foreach ($routesByIg[$igId] as $route) {
            $ogId = intval($route['outputGroupId']);
            $connected = isset($route['connected']) && $route['connected'];
            $volume = isset($route['volume']) ? max(0, min(100, intval($route['volume']))) : 100;
            $mute = isset($route['mute']) && $route['mute'];

            if ($connected) {
                $outputs[] = $ogId;
            }
            // Always store per-path settings (even if not connected, preserve volume)
            $routing[strval($ogId)] = array('volume' => $volume, 'mute' => $mute);
        }

        $ig['outputs'] = $outputs;
        $ig['routing'] = $routing;
    }
    unset($ig);

    file_put_contents($configFile, json_encode($data, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES));
    GenerateBackupViaAPI('Routing matrix was modified.');

    return json(array("status" => "OK", "message" => "Routing matrix saved"));
}

/////////////////////////////////////////////////////////////////////////////
// POST /api/pipewire/audio/routing/volume
// Real-time per-path volume adjustment
// Body: { "inputGroupId": 1, "outputGroupId": 2, "volume": 75 }
function SetRoutingPathVolume()
{
    global $SUDO, $settings;

    $body = json_decode(file_get_contents('php://input'), true);
    if (!$body || !isset($body['inputGroupId']) || !isset($body['outputGroupId']) || !isset($body['volume'])) {
        return json(array("status" => "error", "message" => "Missing inputGroupId, outputGroupId, or volume"));
    }

    $igId = intval($body['inputGroupId']);
    $ogId = intval($body['outputGroupId']);
    $volumePct = max(0, min(100, intval($body['volume'])));
    $volumeLinear = round($volumePct / 100.0, 3);

    // Load configs to resolve node names
    $configFile = $settings['mediaDirectory'] . "/config/pipewire-input-groups.json";
    $outputGroupsFile = $settings['mediaDirectory'] . "/config/pipewire-audio-groups.json";

    if (!file_exists($configFile) || !file_exists($outputGroupsFile)) {
        return json(array("status" => "error", "message" => "Config files not found"));
    }

    $igData = json_decode(file_get_contents($configFile), true);
    $ogData = json_decode(file_get_contents($outputGroupsFile), true);

    // Find input group
    $igName = '';
    $hasEffects = false;
    foreach ($igData['inputGroups'] as $ig) {
        if (intval($ig['id']) === $igId) {
            $igName = isset($ig['name']) ? $ig['name'] : '';
            $hasEffects = isset($ig['effects']['eq']['enabled']) && $ig['effects']['eq']['enabled']
                && isset($ig['effects']['eq']['bands']) && !empty($ig['effects']['eq']['bands']);
            break;
        }
    }

    // Find output group node name
    $ogNodeName = '';
    foreach ($ogData['groups'] as $og) {
        if (intval($og['id']) === $ogId) {
            $ogName = isset($og['name']) ? $og['name'] : 'Group';
            $ogNodeName = "fpp_group_" . preg_replace('/[^a-zA-Z0-9_]/', '_', strtolower($ogName));
            break;
        }
    }

    if (empty($ogNodeName)) {
        return json(array("status" => "error", "message" => "Output group not found"));
    }

    // The combine-stream that routes to output groups is either:
    // - fpp_input_<name> (no effects) or fpp_route_ig_<id> (with effects)
    $routingNodeName = $hasEffects ? "fpp_route_ig_$igId"
        : "fpp_input_" . preg_replace('/[^a-zA-Z0-9_]/', '_', strtolower($igName));

    // Find the internal combine-stream output that targets this output group
    // The internal stream name pattern: <combine_name>.<target_name>
    $env = "PIPEWIRE_RUNTIME_DIR=/run/pipewire-fpp XDG_RUNTIME_DIR=/run/pipewire-fpp";
    $raw = shell_exec($SUDO . " " . $env . " pw-dump 2>/dev/null");
    if (empty($raw)) {
        return json(array("status" => "error", "message" => "Cannot connect to PipeWire"));
    }

    $objects = json_decode($raw, true);
    if (!is_array($objects)) {
        return json(array("status" => "error", "message" => "Invalid PipeWire dump"));
    }

    // Find nodes that belong to the routing combine-stream and target this output group
    $success = false;
    foreach ($objects as $obj) {
        if (!isset($obj['type']) || $obj['type'] !== 'PipeWire:Interface:Node')
            continue;
        $props = isset($obj['info']['props']) ? $obj['info']['props'] : array();
        $nm = isset($props['node.name']) ? $props['node.name'] : '';
        $target = isset($props['node.target']) ? $props['node.target'] : '';

        // Match internal stream: node.name starts with routing node name and targets output group
        if (
            ($nm === $routingNodeName || strpos($nm, $routingNodeName . '.') === 0)
            && $target === $ogNodeName
        ) {
            $cmd = $SUDO . " " . $env . " pw-cli set-param " . $obj['id'] . " Props '{ channelmix.volume: $volumeLinear }' 2>&1";
            $output = shell_exec($cmd);
            if (strpos($output, 'Error') === false) {
                $success = true;
            }
        }
    }

    // Also update saved config for persistence
    foreach ($igData['inputGroups'] as &$ig) {
        if (intval($ig['id']) === $igId) {
            if (!isset($ig['routing']))
                $ig['routing'] = array();
            $pathKey = strval($ogId);
            if (!isset($ig['routing'][$pathKey]))
                $ig['routing'][$pathKey] = array();
            $ig['routing'][$pathKey]['volume'] = $volumePct;
            break;
        }
    }
    unset($ig);
    file_put_contents($configFile, json_encode($igData, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES));

    return json(array(
        "status" => $success ? "OK" : "warning",
        "message" => $success ? "Route volume set to {$volumePct}%" : "Volume saved but real-time update may need Apply",
        "volume" => $volumePct
    ));
}

/////////////////////////////////////////////////////////////////////////////
// GET /api/pipewire/audio/routing/presets
// List saved routing presets
function GetRoutingPresets()
{
    global $settings;
    $presetsDir = $settings['mediaDirectory'] . "/config/routing-presets";
    $presets = array();

    if (is_dir($presetsDir)) {
        $files = glob($presetsDir . "/*.json");
        foreach ($files as $file) {
            $name = basename($file, '.json');
            $data = json_decode(file_get_contents($file), true);
            $presets[] = array(
                'name' => $name,
                'description' => isset($data['description']) ? $data['description'] : '',
                'created' => date('Y-m-d H:i:s', filemtime($file))
            );
        }
    }

    return json(array('presets' => $presets));
}

/////////////////////////////////////////////////////////////////////////////
// GET /api/pipewire/audio/routing/presets/names
// Returns a simple JSON array of preset names (for command dropdowns)
function GetRoutingPresetNames()
{
    global $settings;
    $presetsDir = $settings['mediaDirectory'] . "/config/routing-presets";
    $names = array();

    if (is_dir($presetsDir)) {
        $files = glob($presetsDir . "/*.json");
        foreach ($files as $file) {
            $names[] = basename($file, '.json');
        }
    }
    sort($names);
    return json($names);
}

/////////////////////////////////////////////////////////////////////////////
// POST /api/pipewire/audio/routing/presets
// Save current routing config as a preset
// Body: { "name": "Christmas Show", "description": "..." }
function SaveRoutingPreset()
{
    global $settings;
    $configFile = $settings['mediaDirectory'] . "/config/pipewire-input-groups.json";
    $presetsDir = $settings['mediaDirectory'] . "/config/routing-presets";

    $body = json_decode(file_get_contents('php://input'), true);
    if (!$body || !isset($body['name']) || empty(trim($body['name']))) {
        http_response_code(400);
        return json(array("status" => "error", "message" => "Missing preset name"));
    }

    $name = preg_replace('/[^a-zA-Z0-9_ -]/', '', trim($body['name']));
    if (empty($name)) {
        http_response_code(400);
        return json(array("status" => "error", "message" => "Invalid preset name"));
    }

    if (!file_exists($configFile)) {
        return json(array("status" => "error", "message" => "No input groups configured"));
    }

    // Create presets directory
    if (!is_dir($presetsDir)) {
        mkdir($presetsDir, 0775, true);
    }

    // Read current config and extract routing data
    $data = json_decode(file_get_contents($configFile), true);
    $preset = array(
        'name' => $name,
        'description' => isset($body['description']) ? $body['description'] : '',
        'savedAt' => date('Y-m-d H:i:s'),
        'routing' => array()
    );

    if (is_array($data) && isset($data['inputGroups'])) {
        foreach ($data['inputGroups'] as $ig) {
            $igId = isset($ig['id']) ? intval($ig['id']) : 0;
            $preset['routing'][] = array(
                'inputGroupId' => $igId,
                'inputGroupName' => isset($ig['name']) ? $ig['name'] : '',
                'outputs' => isset($ig['outputs']) ? $ig['outputs'] : array(),
                'routing' => isset($ig['routing']) ? $ig['routing'] : array(),
                'effects' => isset($ig['effects']) ? $ig['effects'] : array()
            );
        }
    }

    // Snapshot video routing (which source feeds which video output group)
    $videoGroupsFile = $settings['mediaDirectory'] . "/config/pipewire-video-groups.json";
    if (file_exists($videoGroupsFile)) {
        $vData = json_decode(file_get_contents($videoGroupsFile), true);
        if (is_array($vData) && isset($vData['videoOutputGroups'])) {
            $videoRouting = array();
            foreach ($vData['videoOutputGroups'] as $vg) {
                if (!isset($vg['enabled']) || !$vg['enabled'])
                    continue;
                $videoRouting[] = array(
                    'groupId' => intval($vg['id']),
                    'groupName' => isset($vg['name']) ? $vg['name'] : '',
                    'videoSource' => isset($vg['videoSource']) ? $vg['videoSource'] : '',
                );
            }
            if (!empty($videoRouting)) {
                $preset['videoRouting'] = $videoRouting;
            }
        }
    }

    $presetFile = $presetsDir . "/" . $name . ".json";
    file_put_contents($presetFile, json_encode($preset, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES));

    GenerateBackupViaAPI("Routing preset '$name' saved.");

    return json(array("status" => "OK", "message" => "Preset '$name' saved", "name" => $name));
}

/////////////////////////////////////////////////////////////////////////////
// POST /api/pipewire/audio/routing/presets/load
// Load a routing preset and apply it to current input groups
// Body: { "name": "Christmas Show" }
function LoadRoutingPreset()
{
    global $settings;
    $configFile = $settings['mediaDirectory'] . "/config/pipewire-input-groups.json";
    $presetsDir = $settings['mediaDirectory'] . "/config/routing-presets";

    $body = json_decode(file_get_contents('php://input'), true);
    if (!$body || !isset($body['name'])) {
        http_response_code(400);
        return json(array("status" => "error", "message" => "Missing preset name"));
    }

    $name = preg_replace('/[^a-zA-Z0-9_ -]/', '', trim($body['name']));
    $presetFile = $presetsDir . "/" . $name . ".json";

    if (!file_exists($presetFile)) {
        http_response_code(404);
        return json(array("status" => "error", "message" => "Preset '$name' not found"));
    }

    if (!file_exists($configFile)) {
        return json(array("status" => "error", "message" => "No input groups configured"));
    }

    $preset = json_decode(file_get_contents($presetFile), true);
    $data = json_decode(file_get_contents($configFile), true);

    if (!is_array($preset) || !isset($preset['routing']) || !is_array($data) || !isset($data['inputGroups'])) {
        return json(array("status" => "error", "message" => "Invalid preset or config data"));
    }

    // Apply preset routing to matching input groups (by ID)
    $applied = 0;
    foreach ($preset['routing'] as $presetIg) {
        $presetIgId = intval($presetIg['inputGroupId']);
        foreach ($data['inputGroups'] as &$ig) {
            if (intval($ig['id']) === $presetIgId) {
                $ig['outputs'] = $presetIg['outputs'];
                $ig['routing'] = $presetIg['routing'];
                if (isset($presetIg['effects'])) {
                    $ig['effects'] = $presetIg['effects'];
                }
                $applied++;
                break;
            }
        }
        unset($ig);
    }

    file_put_contents($configFile, json_encode($data, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES));

    // Apply video routing from preset (if present)
    $videoApplied = 0;
    if (isset($preset['videoRouting']) && is_array($preset['videoRouting'])) {
        $videoGroupsFile = $settings['mediaDirectory'] . "/config/pipewire-video-groups.json";
        if (file_exists($videoGroupsFile)) {
            $vData = json_decode(file_get_contents($videoGroupsFile), true);
            if (is_array($vData) && isset($vData['videoOutputGroups'])) {
                foreach ($preset['videoRouting'] as $vr) {
                    $gid = intval($vr['groupId']);
                    foreach ($vData['videoOutputGroups'] as &$vg) {
                        if (intval($vg['id']) === $gid) {
                            $vg['videoSource'] = isset($vr['videoSource']) ? $vr['videoSource'] : '';
                            $videoApplied++;
                            break;
                        }
                    }
                    unset($vg);
                }
                file_put_contents($videoGroupsFile, json_encode($vData, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES));
            }
        }
    }

    GenerateBackupViaAPI("Routing preset '$name' loaded.");

    $msg = "Preset '$name' loaded ($applied audio group(s)";
    if ($videoApplied > 0) {
        $msg .= ", $videoApplied video group(s)";
    }
    $msg .= " updated). Apply to activate.";

    return json(array(
        "status" => "OK",
        "message" => $msg,
        "applied" => $applied,
        "videoApplied" => $videoApplied,
        "needsApply" => true
    ));
}

/////////////////////////////////////////////////////////////////////////////
// DELETE /api/pipewire/audio/routing/presets/:name
// Delete a routing preset
function DeleteRoutingPreset()
{
    global $settings;
    $presetsDir = $settings['mediaDirectory'] . "/config/routing-presets";

    $name = params('name');
    if (empty($name)) {
        http_response_code(400);
        return json(array("status" => "error", "message" => "Missing preset name"));
    }

    $name = preg_replace('/[^a-zA-Z0-9_ -]/', '', trim($name));
    $presetFile = $presetsDir . "/" . $name . ".json";

    if (!file_exists($presetFile)) {
        http_response_code(404);
        return json(array("status" => "error", "message" => "Preset '$name' not found"));
    }

    unlink($presetFile);
    return json(array("status" => "OK", "message" => "Preset '$name' deleted"));
}

/////////////////////////////////////////////////////////////////////////////
// POST /api/pipewire/audio/routing/presets/live-apply
// Load a routing preset and apply it in real-time without stopping playback.
//
// Live-applied changes (no PipeWire restart):
//   - Routing path volume / mute changes
//   - EQ band parameter changes (freq, gain, Q)
//
// Changes that require a PipeWire restart (topology changes):
//   - Adding / removing output group targets
//   - Enabling / disabling EQ
//   - Changing number of EQ bands
//
// Body: { "name": "Christmas Show" }
function LiveApplyRoutingPreset()
{
    global $settings, $SUDO;
    $configFile = $settings['mediaDirectory'] . "/config/pipewire-input-groups.json";
    $presetsDir = $settings['mediaDirectory'] . "/config/routing-presets";
    $outputGroupsFile = $settings['mediaDirectory'] . "/config/pipewire-audio-groups.json";

    $body = json_decode(file_get_contents('php://input'), true);
    if (!$body || !isset($body['name'])) {
        http_response_code(400);
        return json(array("status" => "error", "message" => "Missing preset name"));
    }

    $name = preg_replace('/[^a-zA-Z0-9_ -]/', '', trim($body['name']));
    $presetFile = $presetsDir . "/" . $name . ".json";

    if (!file_exists($presetFile)) {
        http_response_code(404);
        return json(array("status" => "error", "message" => "Preset '$name' not found"));
    }

    if (!file_exists($configFile)) {
        return json(array("status" => "error", "message" => "No input groups configured"));
    }

    $preset = json_decode(file_get_contents($presetFile), true);
    $data = json_decode(file_get_contents($configFile), true);

    if (
        !is_array($preset) || !isset($preset['routing']) ||
        !is_array($data) || !isset($data['inputGroups'])
    ) {
        return json(array("status" => "error", "message" => "Invalid preset or config data"));
    }

    // Load output groups for node name resolution
    $outputGroups = array();
    if (file_exists($outputGroupsFile)) {
        $ogData = json_decode(file_get_contents($outputGroupsFile), true);
        if (is_array($ogData) && isset($ogData['groups'])) {
            $outputGroups = $ogData['groups'];
        }
    }

    // Build output group lookup: id → node name
    $ogNodeNames = array();
    foreach ($outputGroups as $og) {
        $ogId = intval($og['id']);
        $ogName = isset($og['name']) ? $og['name'] : 'Group';
        $ogNodeNames[$ogId] = "fpp_group_" . preg_replace('/[^a-zA-Z0-9_]/', '_', strtolower($ogName));
    }

    // Get PipeWire dump once for all lookups
    $env = "PIPEWIRE_RUNTIME_DIR=/run/pipewire-fpp XDG_RUNTIME_DIR=/run/pipewire-fpp";
    $raw = shell_exec($SUDO . " " . $env . " pw-dump 2>/dev/null");
    $pwObjects = !empty($raw) ? json_decode($raw, true) : null;
    if (!is_array($pwObjects)) {
        $pwObjects = array();
    }

    $topologyChanged = false;
    $volumeChanges = 0;
    $eqChanges = 0;
    $applied = 0;
    $channelLabels = array("l", "r", "c", "lfe", "rl", "rr", "sl", "sr");

    // ── Detect topology changes and apply live changes ──────────────────
    foreach ($preset['routing'] as $presetIg) {
        $presetIgId = intval($presetIg['inputGroupId']);

        // Find matching current input group
        $currentIg = null;
        $currentIgIdx = null;
        foreach ($data['inputGroups'] as $idx => $ig) {
            if (intval($ig['id']) === $presetIgId) {
                $currentIg = $ig;
                $currentIgIdx = $idx;
                break;
            }
        }
        if ($currentIg === null)
            continue;

        $igId = $presetIgId;
        $igName = isset($currentIg['name']) ? $currentIg['name'] : '';

        // ── Check: output group topology changed? ──
        $currentOutputs = isset($currentIg['outputs']) ? $currentIg['outputs'] : array();
        $presetOutputs = isset($presetIg['outputs']) ? $presetIg['outputs'] : array();
        $curSorted = $currentOutputs;
        sort($curSorted);
        $preSorted = $presetOutputs;
        sort($preSorted);
        if ($curSorted !== $preSorted) {
            $topologyChanged = true;
        }

        // ── Check: EQ topology changed (enabled/disabled, band count)? ──
        $currentEq = isset($currentIg['effects']['eq']) ? $currentIg['effects']['eq'] : array();
        $presetEq = isset($presetIg['effects']['eq']) ? $presetIg['effects']['eq'] : array();
        $curEqOn = !empty($currentEq['enabled']) && !empty($currentEq['bands']);
        $preEqOn = !empty($presetEq['enabled']) && !empty($presetEq['bands']);

        if ($curEqOn !== $preEqOn) {
            $topologyChanged = true;
        } elseif ($curEqOn && $preEqOn) {
            if (count($currentEq['bands']) !== count($presetEq['bands'])) {
                $topologyChanged = true;
            }
        }

        $applied++;
    }

    // ── If no topology change, apply everything live ────────────────────
    if (!$topologyChanged) {
        foreach ($preset['routing'] as $presetIg) {
            $presetIgId = intval($presetIg['inputGroupId']);
            $currentIg = null;
            $currentIgIdx = null;
            foreach ($data['inputGroups'] as $idx => $ig) {
                if (intval($ig['id']) === $presetIgId) {
                    $currentIg = $ig;
                    $currentIgIdx = $idx;
                    break;
                }
            }
            if ($currentIg === null)
                continue;

            $igId = $presetIgId;
            $igName = isset($currentIg['name']) ? $currentIg['name'] : '';

            $curEqOn = !empty($currentIg['effects']['eq']['enabled'])
                && !empty($currentIg['effects']['eq']['bands']);

            // Determine routing combine-stream name (with or without EQ)
            $routingNodeName = $curEqOn
                ? "fpp_route_ig_$igId"
                : "fpp_input_" . preg_replace('/[^a-zA-Z0-9_]/', '_', strtolower($igName));

            // ── Live-apply routing path volumes ──
            $presetOutputs = isset($presetIg['outputs']) ? $presetIg['outputs'] : array();
            $presetRouting = isset($presetIg['routing']) ? $presetIg['routing'] : array();

            foreach ($presetOutputs as $ogId) {
                $ogId = intval($ogId);
                if (!isset($ogNodeNames[$ogId]))
                    continue;

                $ogTarget = $ogNodeNames[$ogId];
                $pathKey = strval($ogId);
                $volumePct = 100;
                $mute = false;

                if (isset($presetRouting[$pathKey])) {
                    $volumePct = isset($presetRouting[$pathKey]['volume'])
                        ? intval($presetRouting[$pathKey]['volume']) : 100;
                    $mute = !empty($presetRouting[$pathKey]['mute']);
                }

                $volumeLinear = $mute ? 0.0 : round($volumePct / 100.0, 3);

                // Find the combine-stream output member targeting this OG
                foreach ($pwObjects as $obj) {
                    if (
                        !isset($obj['type']) ||
                        $obj['type'] !== 'PipeWire:Interface:Node'
                    )
                        continue;
                    $props = isset($obj['info']['props']) ? $obj['info']['props'] : array();
                    $nm = isset($props['node.name']) ? $props['node.name'] : '';
                    $target = isset($props['node.target']) ? $props['node.target'] : '';

                    if (
                        ($nm === $routingNodeName ||
                            strpos($nm, $routingNodeName . '.') === 0)
                        && $target === $ogTarget
                    ) {
                        $cmd = $SUDO . " " . $env
                            . " pw-cli set-param " . $obj['id']
                            . " Props '{ channelmix.volume: $volumeLinear }' 2>&1";
                        shell_exec($cmd);
                        $volumeChanges++;
                    }
                }
            }

            // ── Live-apply EQ band parameters ──
            $presetEq = isset($presetIg['effects']['eq'])
                ? $presetIg['effects']['eq'] : array();
            $preEqOn = !empty($presetEq['enabled'])
                && !empty($presetEq['bands']);

            if ($curEqOn && $preEqOn) {
                $fxNodeName = "fpp_fx_ig_" . $igId;
                $fxNodeId = null;

                foreach ($pwObjects as $obj) {
                    if (
                        !isset($obj['type']) ||
                        $obj['type'] !== 'PipeWire:Interface:Node'
                    )
                        continue;
                    $props = isset($obj['info']['props']) ? $obj['info']['props'] : array();
                    $nm = isset($props['node.name']) ? $props['node.name'] : '';
                    if ($nm === $fxNodeName) {
                        $fxNodeId = $obj['id'];
                        break;
                    }
                }

                if ($fxNodeId !== null) {
                    $igChannels = isset($currentIg['channels'])
                        ? intval($currentIg['channels']) : 2;
                    $numCh = min($igChannels, count($channelLabels));

                    foreach ($presetEq['bands'] as $bi => $band) {
                        $freq = floatval(isset($band['freq']) ? $band['freq'] : 1000);
                        $gain = floatval(isset($band['gain']) ? $band['gain'] : 0);
                        $q = floatval(isset($band['q']) ? $band['q'] : 1.0);

                        for ($ch = 0; $ch < $numCh; $ch++) {
                            $chLabel = $channelLabels[$ch];
                            $p = "eq_{$chLabel}_{$bi}";
                            shell_exec($SUDO . " " . $env
                                . " pw-cli set-param $fxNodeId Props"
                                . " '{ \"$p:Freq\": $freq }' 2>&1");
                            shell_exec($SUDO . " " . $env
                                . " pw-cli set-param $fxNodeId Props"
                                . " '{ \"$p:Gain\": $gain }' 2>&1");
                            shell_exec($SUDO . " " . $env
                                . " pw-cli set-param $fxNodeId Props"
                                . " '{ \"$p:Q\": $q }' 2>&1");
                        }
                        $eqChanges++;
                    }
                }
            }

            // Update config for persistence
            $data['inputGroups'][$currentIgIdx]['outputs'] = $presetIg['outputs'];
            $data['inputGroups'][$currentIgIdx]['routing'] =
                isset($presetIg['routing']) ? $presetIg['routing'] : array();
            if (isset($presetIg['effects'])) {
                $data['inputGroups'][$currentIgIdx]['effects'] = $presetIg['effects'];
            }
        }

        // Persist config
        file_put_contents(
            $configFile,
            json_encode($data, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES)
        );

        // Apply video routing from preset if present (video changes always
        // need a consumer restart, so we save + apply separately)
        $videoMsg = '';
        if (isset($preset['videoRouting']) && is_array($preset['videoRouting'])) {
            $videoGroupsFile = $settings['mediaDirectory'] . "/config/pipewire-video-groups.json";
            if (file_exists($videoGroupsFile)) {
                $vData = json_decode(file_get_contents($videoGroupsFile), true);
                if (is_array($vData) && isset($vData['videoOutputGroups'])) {
                    $vChanged = 0;
                    foreach ($preset['videoRouting'] as $vr) {
                        $gid = intval($vr['groupId']);
                        foreach ($vData['videoOutputGroups'] as &$vg) {
                            if (intval($vg['id']) === $gid) {
                                $newSrc = isset($vr['videoSource']) ? $vr['videoSource'] : '';
                                $curSrc = isset($vg['videoSource']) ? $vg['videoSource'] : '';
                                if ($newSrc !== $curSrc) {
                                    $vg['videoSource'] = $newSrc;
                                    $vChanged++;
                                }
                                break;
                            }
                        }
                        unset($vg);
                    }
                    if ($vChanged > 0) {
                        file_put_contents($videoGroupsFile, json_encode($vData, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES));
                        // Re-apply video groups to update consumers
                        ApplyPipeWireVideoGroups();
                        $videoMsg = ", $vChanged video source change(s) applied";
                    }
                }
            }
        }

        return json(array(
            "status" => "OK",
            "message" => "Preset '$name' live-applied ($applied groups,"
                . " $volumeChanges volume changes, $eqChanges EQ updates$videoMsg)",
            "preset" => $name,
            "applied" => $applied,
            "volumeChanges" => $volumeChanges,
            "eqChanges" => $eqChanges,
            "liveApplied" => true,
            "restarted" => false,
        ));
    }

    // ── Topology changed — must update config and do full apply ─────────
    // First update the config with the preset data
    foreach ($preset['routing'] as $presetIg) {
        $presetIgId = intval($presetIg['inputGroupId']);
        foreach ($data['inputGroups'] as $idx => &$ig) {
            if (intval($ig['id']) === $presetIgId) {
                $ig['outputs'] = $presetIg['outputs'];
                $ig['routing'] = isset($presetIg['routing']) ? $presetIg['routing'] : array();
                if (isset($presetIg['effects'])) {
                    $ig['effects'] = $presetIg['effects'];
                }
                break;
            }
        }
        unset($ig);
    }
    file_put_contents(
        $configFile,
        json_encode($data, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES)
    );

    // Also apply video routing from preset if present
    if (isset($preset['videoRouting']) && is_array($preset['videoRouting'])) {
        $videoGroupsFile = $settings['mediaDirectory'] . "/config/pipewire-video-groups.json";
        if (file_exists($videoGroupsFile)) {
            $vData = json_decode(file_get_contents($videoGroupsFile), true);
            if (is_array($vData) && isset($vData['videoOutputGroups'])) {
                foreach ($preset['videoRouting'] as $vr) {
                    $gid = intval($vr['groupId']);
                    foreach ($vData['videoOutputGroups'] as &$vg) {
                        if (intval($vg['id']) === $gid) {
                            $vg['videoSource'] = isset($vr['videoSource']) ? $vr['videoSource'] : '';
                            break;
                        }
                    }
                    unset($vg);
                }
                file_put_contents($videoGroupsFile, json_encode($vData, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES));
            }
        }
    }

    // Delegate to the full apply mechanism (handles PipeWire restart +
    // playback stop/resume internally)
    return ApplyPipeWireInputGroups();
}

/////////////////////////////////////////////////////////////////////////////
// POST /api/pipewire/audio/input-groups/effects
// Save input group effects config (EQ)
// Body: { "groupId": 1, "effects": { "eq": { "enabled": true, "bands": [...] } } }
function SaveInputGroupEffects()
{
    global $settings;
    $configFile = $settings['mediaDirectory'] . "/config/pipewire-input-groups.json";

    $body = json_decode(file_get_contents('php://input'), true);
    if (!$body || !isset($body['groupId']) || !isset($body['effects'])) {
        http_response_code(400);
        return json(array("status" => "error", "message" => "Missing groupId or effects"));
    }

    if (!file_exists($configFile)) {
        return json(array("status" => "error", "message" => "No input groups configured"));
    }

    $data = json_decode(file_get_contents($configFile), true);
    $groupId = intval($body['groupId']);
    $found = false;

    foreach ($data['inputGroups'] as &$ig) {
        if (intval($ig['id']) === $groupId) {
            $ig['effects'] = $body['effects'];
            $found = true;
            break;
        }
    }
    unset($ig);

    if (!$found) {
        return json(array("status" => "error", "message" => "Input group $groupId not found"));
    }

    file_put_contents($configFile, json_encode($data, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES));
    GenerateBackupViaAPI("Input group effects updated.");

    return json(array("status" => "OK", "message" => "Effects saved. Apply to activate.", "needsApply" => true));
}

/////////////////////////////////////////////////////////////////////////////
// POST /api/pipewire/audio/input-groups/eq/update
// Real-time EQ adjustment on input group filter-chain
// Body: { "groupId": 1, "band": 0, "freq": 1000, "gain": 3, "q": 1.4 }
function UpdateInputGroupEQRealtime()
{
    global $SUDO, $settings;

    $body = json_decode(file_get_contents('php://input'), true);
    if (!$body || !isset($body['groupId']) || !isset($body['band'])) {
        return json(array("status" => "error", "message" => "Missing groupId or band"));
    }

    $groupId = intval($body['groupId']);
    $bandIdx = intval($body['band']);
    $fxNodeName = "fpp_fx_ig_" . $groupId;

    // Find the filter-chain node ID
    $env = "PIPEWIRE_RUNTIME_DIR=/run/pipewire-fpp XDG_RUNTIME_DIR=/run/pipewire-fpp";
    $raw = shell_exec($SUDO . " " . $env . " pw-dump 2>/dev/null");
    if (empty($raw)) {
        return json(array("status" => "error", "message" => "Cannot connect to PipeWire"));
    }

    $objects = json_decode($raw, true);
    $nodeId = null;
    if (is_array($objects)) {
        foreach ($objects as $obj) {
            if (!isset($obj['type']) || $obj['type'] !== 'PipeWire:Interface:Node')
                continue;
            $props = isset($obj['info']['props']) ? $obj['info']['props'] : array();
            $nm = isset($props['node.name']) ? $props['node.name'] : '';
            if ($nm === $fxNodeName) {
                $nodeId = $obj['id'];
                break;
            }
        }
    }

    if ($nodeId === null) {
        return json(array("status" => "error", "message" => "Filter-chain node '$fxNodeName' not found. Apply input groups first."));
    }

    // Load input group config to get channel count
    $configFile = $settings['mediaDirectory'] . "/config/pipewire-input-groups.json";
    $igChannels = 2;
    if (file_exists($configFile)) {
        $data = json_decode(file_get_contents($configFile), true);
        if (is_array($data) && isset($data['inputGroups'])) {
            foreach ($data['inputGroups'] as $ig) {
                if (intval($ig['id']) === $groupId) {
                    $igChannels = isset($ig['channels']) ? intval($ig['channels']) : 2;
                    break;
                }
            }
        }
    }

    // Set EQ params on all channels for this band
    $channelLabels = array("l", "r", "c", "lfe", "rl", "rr", "sl", "sr");
    $numCh = min($igChannels, count($channelLabels));
    $success = true;

    for ($ch = 0; $ch < $numCh; $ch++) {
        $chLabel = $channelLabels[$ch];
        $paramName = "eq_{$chLabel}_{$bandIdx}";

        if (isset($body['freq'])) {
            $cmd = $SUDO . " " . $env . " pw-cli set-param $nodeId Props '{ \"$paramName:Freq\": " . floatval($body['freq']) . " }' 2>&1";
            $output = shell_exec($cmd);
            if (strpos($output, 'Error') !== false)
                $success = false;
        }
        if (isset($body['gain'])) {
            $cmd = $SUDO . " " . $env . " pw-cli set-param $nodeId Props '{ \"$paramName:Gain\": " . floatval($body['gain']) . " }' 2>&1";
            $output = shell_exec($cmd);
            if (strpos($output, 'Error') !== false)
                $success = false;
        }
        if (isset($body['q'])) {
            $cmd = $SUDO . " " . $env . " pw-cli set-param $nodeId Props '{ \"$paramName:Q\": " . floatval($body['q']) . " }' 2>&1";
            $output = shell_exec($cmd);
            if (strpos($output, 'Error') !== false)
                $success = false;
        }
    }

    // Also save to config for persistence
    if (file_exists($configFile)) {
        $data = json_decode(file_get_contents($configFile), true);
        foreach ($data['inputGroups'] as &$ig) {
            if (intval($ig['id']) === $groupId) {
                if (isset($ig['effects']['eq']['bands'][$bandIdx])) {
                    if (isset($body['freq']))
                        $ig['effects']['eq']['bands'][$bandIdx]['freq'] = floatval($body['freq']);
                    if (isset($body['gain']))
                        $ig['effects']['eq']['bands'][$bandIdx]['gain'] = floatval($body['gain']);
                    if (isset($body['q']))
                        $ig['effects']['eq']['bands'][$bandIdx]['q'] = floatval($body['q']);
                }
                break;
            }
        }
        unset($ig);
        file_put_contents($configFile, json_encode($data, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES));
    }

    return json(array(
        "status" => $success ? "OK" : "error",
        "message" => $success ? "EQ band $bandIdx updated" : "Some EQ params failed to update"
    ));
}

/////////////////////////////////////////////////////////////////////////////
// GET /api/pipewire/audio/sources
// Returns available PipeWire audio capture sources (ALSA Audio/Source nodes)
function GetPipeWireAudioSources()
{
    global $SUDO;

    $sources = array();
    $env = "PIPEWIRE_RUNTIME_DIR=/run/pipewire-fpp XDG_RUNTIME_DIR=/run/pipewire-fpp";
    $raw = shell_exec($SUDO . " " . $env . " pw-dump 2>/dev/null");
    if (empty($raw)) {
        return json($sources);
    }

    $objects = json_decode($raw, true);
    if (!is_array($objects)) {
        return json($sources);
    }

    foreach ($objects as $obj) {
        $type = isset($obj['type']) ? $obj['type'] : '';
        if ($type !== 'PipeWire:Interface:Node')
            continue;

        $props = isset($obj['info']['props']) ? $obj['info']['props'] : array();
        $mc = isset($props['media.class']) ? $props['media.class'] : '';

        // Only include Audio/Source (capture devices)
        if ($mc !== 'Audio/Source')
            continue;

        $name = isset($props['node.name']) ? $props['node.name'] : '';
        $desc = isset($props['node.description']) ? $props['node.description'] : $name;
        $nick = isset($props['node.nick']) ? $props['node.nick'] : '';

        // Skip PipeWire internal monitors and virtual sources
        if (strpos($name, '.monitor') !== false)
            continue;

        // Get card ID from alsa properties
        $cardId = '';
        if (isset($props['alsa.card'])) {
            // Resolve to stable ID via /proc/asound
            $cardNum = intval($props['alsa.card']);
            $idFile = @file_get_contents("/proc/asound/card$cardNum/id");
            if ($idFile !== false) {
                $cardId = trim($idFile);
            }
        }

        $channels = isset($props['audio.channels']) ? intval($props['audio.channels']) : 2;
        $rate = isset($props['audio.rate']) ? intval($props['audio.rate']) : 48000;

        $sources[] = array(
            'nodeId' => $obj['id'],
            'name' => $name,
            'description' => $desc,
            'nick' => $nick,
            'cardId' => $cardId,
            'channels' => $channels,
            'sampleRate' => $rate,
            'mediaClass' => $mc,
            'state' => isset($obj['info']['state']) ? $obj['info']['state'] : '',
        );
    }

    return json($sources);
}

/////////////////////////////////////////////////////////////////////////////
// Helper: Resolve ALSA card ID to exact PipeWire capture node name
// Queries pw-dump to find the Audio/Source node matching the given card ID.
function ResolveAlsaCaptureNodeName($cardId)
{
    global $SUDO;

    $env = "PIPEWIRE_RUNTIME_DIR=/run/pipewire-fpp XDG_RUNTIME_DIR=/run/pipewire-fpp";
    $raw = shell_exec($SUDO . " " . $env . " pw-dump 2>/dev/null");
    if (empty($raw))
        return '';

    $objects = json_decode($raw, true);
    if (!is_array($objects))
        return '';

    foreach ($objects as $obj) {
        $type = isset($obj['type']) ? $obj['type'] : '';
        if ($type !== 'PipeWire:Interface:Node')
            continue;

        $props = isset($obj['info']['props']) ? $obj['info']['props'] : array();
        $mc = isset($props['media.class']) ? $props['media.class'] : '';
        if ($mc !== 'Audio/Source')
            continue;

        $name = isset($props['node.name']) ? $props['node.name'] : '';
        if (empty($name) || strpos($name, 'alsa_input') !== 0)
            continue;

        // Match by ALSA card ID
        if (isset($props['alsa.card'])) {
            $cardNum = intval($props['alsa.card']);
            $idFile = @file_get_contents("/proc/asound/card$cardNum/id");
            if ($idFile !== false && trim($idFile) === $cardId) {
                return $name;
            }
        }
    }

    return '';
}

/////////////////////////////////////////////////////////////////////////////
// Helper: Generate PipeWire input group config (combine-stream + loopback)
function GeneratePipeWireInputGroupsConfig($inputGroups, $outputGroups)
{
    global $settings;
    $channelPositions = array(
        1 => "[ MONO ]",
        2 => "[ FL FR ]",
        3 => "[ FL FR FC ]",
        4 => "[ FL FR RL RR ]",
        5 => "[ FL FR FC RL RR ]",
        6 => "[ FL FR FC LFE RL RR ]",
        7 => "[ FL FR FC LFE RL RR RC ]",
        8 => "[ FL FR FC LFE RL RR SL SR ]"
    );

    $conf = "# Auto-generated by FPP - PipeWire Input Groups (Mix Buses)\n";
    $conf .= "# Do not edit manually - managed via FPP UI\n";
    $conf .= "# Loaded before 97-fpp-audio-groups.conf so input group nodes\n";
    $conf .= "# exist when output groups are created.\n\n";

    // ── Pre-pass: determine which fppd stream slots need a tee (fan-out
    //    to multiple input groups).  When a stream appears in only one
    //    enabled group, fppd's pipewiresink connects directly to that
    //    group's combine-stream.  When it appears in 2+ groups, we create
    //    an intermediate null-audio-sink ("tee") that fppd targets, then
    //    use monitor-capture loopbacks from the tee into each group.
    $streamGroupCount = array(); // sourceId => count of enabled groups
    $streamPrimaryGroup = array(); // sourceId => igNodeName (first-wins, used when count==1)
    foreach ($inputGroups as $ig) {
        if (!isset($ig['enabled']) || !$ig['enabled'])
            continue;
        if (!isset($ig['members']) || empty($ig['members']))
            continue;
        foreach ($ig['members'] as $mbr) {
            if (isset($mbr['type']) && $mbr['type'] === 'fppd_stream') {
                $sourceId = isset($mbr['sourceId']) ? $mbr['sourceId'] : 'fppd_stream_1';
                if (!isset($streamGroupCount[$sourceId])) {
                    $streamGroupCount[$sourceId] = 0;
                    $igName = isset($ig['name']) ? $ig['name'] : 'Input Group';
                    $streamPrimaryGroup[$sourceId] = "fpp_input_" . preg_replace('/[^a-zA-Z0-9_]/', '_', strtolower($igName));
                }
                $streamGroupCount[$sourceId]++;
            }
        }
    }
    $streamNeedsTee = array();
    foreach ($streamGroupCount as $sid => $cnt) {
        if ($cnt > 1) {
            $streamNeedsTee[$sid] = true;
        }
    }

    // ── Null-audio-sink tee nodes for fan-out streams ──
    if (!empty($streamNeedsTee)) {
        $conf .= "context.objects = [\n";
        foreach ($streamNeedsTee as $sourceId => $unused) {
            $teeName = "fpp_tee_" . preg_replace('/[^a-zA-Z0-9_]/', '_', strtolower($sourceId));
            $slotNum = 1;
            if (preg_match('/fppd_stream_(\d+)/', $sourceId, $m)) {
                $slotNum = intval($m[1]);
            }
            $conf .= "  # Tee (null-sink) for $sourceId fan-out\n";
            $conf .= "  { factory = adapter\n";
            $conf .= "    args = {\n";
            $conf .= "      factory.name = support.null-audio-sink\n";
            $conf .= "      node.name = \"$teeName\"\n";
            $conf .= "      node.description = \"FPP Media Stream $slotNum Tee\"\n";
            $conf .= "      media.class = Audio/Sink\n";
            $conf .= "      audio.position = [ FL FR ]\n";
            $conf .= "      monitor.channel-volumes = true\n";
            $conf .= "      monitor.passthrough = true\n";
            $conf .= "    }\n";
            $conf .= "  }\n";
        }
        $conf .= "]\n\n";
    }

    $conf .= "context.modules = [\n";

    foreach ($inputGroups as $ig) {
        if (!isset($ig['enabled']) || !$ig['enabled'])
            continue;
        if (!isset($ig['members']) || empty($ig['members']))
            continue;

        $groupId = isset($ig['id']) ? intval($ig['id']) : 0;
        $groupName = isset($ig['name']) ? $ig['name'] : "Input Group";
        $nodeName = "fpp_input_" . preg_replace('/[^a-zA-Z0-9_]/', '_', strtolower($groupName));
        $groupChannels = isset($ig['channels']) ? intval($ig['channels']) : 2;
        $groupPos = isset($channelPositions[$groupChannels]) ? $channelPositions[$groupChannels] : "[ FL FR ]";

        // ── Combine-stream sink for this input group (mix bus) ──
        //
        // How this works:
        //   - combine.mode=sink creates a virtual sink that MIXES all incoming
        //     streams (fppd, loopback captures, etc. all target this sink)
        //   - stream.rules match OUTPUT TARGETS: Audio/Sink nodes where the
        //     mixed audio gets sent TO.  These are the output group sinks.
        //   - Sources (fppd, loopbacks) connect INTO the sink via target-object.
        //     They do NOT need stream.rules entries.
        //
        // Resolve which output groups this input group routes to:
        $outputs = isset($ig['outputs']) ? $ig['outputs'] : array();
        $routing = isset($ig['routing']) ? $ig['routing'] : array();
        $outputRules = array();
        foreach ($outputs as $outGroupId) {
            // Check per-path routing settings
            $pathKey = strval($outGroupId);
            $pathVolume = 100;
            $pathMute = false;
            if (isset($routing[$pathKey])) {
                $pathVolume = isset($routing[$pathKey]['volume']) ? intval($routing[$pathKey]['volume']) : 100;
                $pathMute = isset($routing[$pathKey]['mute']) && $routing[$pathKey]['mute'];
            }
            // Skip muted paths
            if ($pathMute)
                continue;

            foreach ($outputGroups as $og) {
                if (isset($og['id']) && intval($og['id']) === intval($outGroupId)) {
                    if (!isset($og['enabled']) || !$og['enabled'])
                        continue;
                    $ogName = isset($og['name']) ? $og['name'] : 'Group';
                    $outNodeName = "fpp_group_" . preg_replace('/[^a-zA-Z0-9_]/', '_', strtolower($ogName));
                    $outputRules[] = array(
                        'name' => $outNodeName,
                        'desc' => $ogName,
                        'volume' => $pathVolume,
                        'ogId' => intval($outGroupId),
                        'channels' => isset($og['channels']) ? intval($og['channels']) : 2
                    );
                    break;
                }
            }
        }

        if (empty($outputRules)) {
            $conf .= "  # Input Group: $groupName (SKIPPED — no output groups routed)\n";
            continue;
        }

        // Check if input group has effects (EQ) enabled
        $hasEffects = isset($ig['effects']['eq']['enabled']) && $ig['effects']['eq']['enabled']
            && isset($ig['effects']['eq']['bands']) && !empty($ig['effects']['eq']['bands']);

        $channelLabels = array("l", "r", "c", "lfe", "rl", "rr", "sl", "sr");
        $numCh = min($groupChannels, count($channelLabels));

        // Helper: generate stream.rules block for output groups with per-path volume
        $generateOutputRules = function ($rules) use (&$conf, $channelPositions) {
            $conf .= "      stream.rules = [\n";
            foreach ($rules as $rule) {
                $volLinear = round($rule['volume'] / 100.0, 3);
                $outCh = isset($rule['channels']) ? intval($rule['channels']) : 2;
                $outPos = isset($channelPositions[$outCh]) ? $channelPositions[$outCh] : "[ FL FR ]";
                $conf .= "        { matches = [\n";
                $conf .= "            { media.class = \"Audio/Sink\"\n";
                $conf .= "              node.name = \"" . $rule['name'] . "\"\n";
                $conf .= "            }\n";
                $conf .= "          ]\n";
                $conf .= "          actions = {\n";
                $conf .= "            create-stream = {\n";
                $conf .= "              node.target = \"" . $rule['name'] . "\"\n";
                $conf .= "              combine.audio.position = $outPos\n";
                $conf .= "              audio.position = $outPos\n";
                if ($volLinear < 0.999) {
                    $conf .= "              channelmix.volume = $volLinear\n";
                }
                $conf .= "            }\n";
                $conf .= "          }\n";
                $conf .= "        }\n";
            }
            $conf .= "      ]\n";
        };

        if ($hasEffects) {
            // ── Architecture with effects ──
            // Input sources → combine-stream(input group) → filter-chain(EQ) → routing combine-stream → output groups
            $fxNodeName = "fpp_fx_ig_" . $groupId;
            $fxOutName = $fxNodeName . "_out";
            $routeNodeName = "fpp_route_ig_" . $groupId;

            // 1. Filter-chain for EQ processing
            $bands = $ig['effects']['eq']['bands'];
            $fxDesc = "EQ: $groupName";

            $conf .= "  # Input Group EQ: $groupName\n";
            $conf .= "  { name = libpipewire-module-filter-chain\n";
            $conf .= "    args = {\n";
            $conf .= "      node.description = \"$fxDesc\"\n";
            $conf .= "      filter.graph = {\n";
            $conf .= "        nodes = [\n";

            for ($ch = 0; $ch < $numCh; $ch++) {
                $chLabel = $channelLabels[$ch];
                foreach ($bands as $bi => $band) {
                    $type = isset($band['type']) ? $band['type'] : 'bq_peaking';
                    $freq = floatval(isset($band['freq']) ? $band['freq'] : 1000);
                    $gain = floatval(isset($band['gain']) ? $band['gain'] : 0);
                    $q = floatval(isset($band['q']) ? $band['q'] : 1.0);
                    $conf .= "          { type = builtin label = $type name = eq_{$chLabel}_{$bi} control = { \"Freq\" = $freq \"Q\" = $q \"Gain\" = $gain } }\n";
                }
            }

            $conf .= "        ]\n";

            // Links: chain EQ bands in series per channel
            $conf .= "        links = [\n";
            for ($ch = 0; $ch < $numCh; $ch++) {
                $chLabel = $channelLabels[$ch];
                for ($bi = 1; $bi < count($bands); $bi++) {
                    $prevBi = $bi - 1;
                    $conf .= "          { output = \"eq_{$chLabel}_{$prevBi}:Out\" input = \"eq_{$chLabel}_{$bi}:In\" }\n";
                }
            }
            $conf .= "        ]\n";

            // Inputs: first EQ band of each channel
            $conf .= "        inputs = [";
            for ($ch = 0; $ch < $numCh; $ch++) {
                $chLabel = $channelLabels[$ch];
                $conf .= " \"eq_{$chLabel}_0:In\"";
            }
            $conf .= " ]\n";

            // Outputs: last EQ band of each channel
            $conf .= "        outputs = [";
            $lastBi = count($bands) - 1;
            for ($ch = 0; $ch < $numCh; $ch++) {
                $chLabel = $channelLabels[$ch];
                $conf .= " \"eq_{$chLabel}_{$lastBi}:Out\"";
            }
            $conf .= " ]\n";

            $conf .= "      }\n"; // filter.graph

            $conf .= "      capture.props = {\n";
            $conf .= "        node.name = \"$fxNodeName\"\n";
            $conf .= "        media.class = Audio/Sink\n";
            $conf .= "        audio.channels = $numCh\n";
            $conf .= "        audio.position = $groupPos\n";
            $conf .= "";
            $conf .= "      }\n";
            $conf .= "      playback.props = {\n";
            $conf .= "        node.name = \"$fxOutName\"\n";
            $conf .= "        node.passive = true\n";
            $conf .= "        node.target = \"$routeNodeName\"\n";
            $conf .= "        stream.dont-remix = true\n";
            $conf .= "        audio.channels = $numCh\n";
            $conf .= "        audio.position = $groupPos\n";
            $conf .= "";
            $conf .= "      }\n";

            $conf .= "    }\n"; // args
            $conf .= "  }\n";

            // 2. Routing combine-stream (post-effects fan-out to output groups)
            $conf .= "  # Routing hub (post-EQ): $groupName\n";
            $conf .= "  { name = libpipewire-module-combine-stream\n";
            $conf .= "    args = {\n";
            $conf .= "      combine.mode = sink\n";
            $conf .= "      node.name = \"$routeNodeName\"\n";
            $conf .= "      node.description = \"$groupName (Routing)\"\n";
            $conf .= "      combine.props = {\n";
            $conf .= "        audio.position = $groupPos\n";
            $conf .= "";
            $conf .= "      }\n";
            $conf .= "      stream.props = {\n";
            $conf .= "        stream.dont-remix = true\n";
            $conf .= "      }\n";
            $generateOutputRules($outputRules);
            $conf .= "    }\n";
            $conf .= "  }\n";

            // 3. Main combine-stream routes to filter-chain only
            $conf .= "  # Input Group: $groupName (→ EQ → Routing)\n";
            $conf .= "  { name = libpipewire-module-combine-stream\n";
            $conf .= "    args = {\n";
            $conf .= "      combine.mode = sink\n";
            $conf .= "      node.name = \"$nodeName\"\n";
            $conf .= "      node.description = \"$groupName\"\n";
            $conf .= "      combine.props = {\n";
            $conf .= "        audio.position = $groupPos\n";
            $conf .= "";
            $conf .= "      }\n";
            $conf .= "      stream.props = {\n";
            $conf .= "        stream.dont-remix = true\n";
            $conf .= "      }\n";
            $conf .= "      stream.rules = [\n";
            $conf .= "        { matches = [\n";
            $conf .= "            { media.class = \"Audio/Sink\"\n";
            $conf .= "              node.name = \"$fxNodeName\"\n";
            $conf .= "            }\n";
            $conf .= "          ]\n";
            $conf .= "          actions = {\n";
            $conf .= "            create-stream = {\n";
            $conf .= "              node.target = \"$fxNodeName\"\n";
            $conf .= "            }\n";
            $conf .= "          }\n";
            $conf .= "        }\n";
            $conf .= "      ]\n";
            $conf .= "    }\n";
            $conf .= "  }\n";

        } else {
            // ── Direct routing (no effects) ──
            // Input sources → combine-stream → output groups
            $conf .= "  # Input Group: $groupName\n";
            $conf .= "  { name = libpipewire-module-combine-stream\n";
            $conf .= "    args = {\n";
            $conf .= "      combine.mode = sink\n";
            $conf .= "      node.name = \"$nodeName\"\n";
            $conf .= "      node.description = \"$groupName\"\n";
            $conf .= "      combine.props = {\n";
            $conf .= "        audio.position = $groupPos\n";
            $conf .= "";
            $conf .= "      }\n";
            $conf .= "      stream.props = {\n";
            $conf .= "        stream.dont-remix = true\n";
            $conf .= "      }\n";
            $generateOutputRules($outputRules);
            $conf .= "    }\n";
            $conf .= "  }\n";
        }

        // ── Loopback modules for each capture/AES67 member ──
        foreach ($ig['members'] as $mi => $mbr) {
            $mbrType = isset($mbr['type']) ? $mbr['type'] : '';
            $mbrName = isset($mbr['name']) ? $mbr['name'] : "Member $mi";
            $mbrMute = isset($mbr['mute']) && $mbr['mute'];

            if ($mbrMute)
                continue;  // Don't create loopback for muted sources

            if ($mbrType === 'fppd_stream') {
                $sourceId = isset($mbr['sourceId']) ? $mbr['sourceId'] : 'fppd_stream_1';
                $needsTee = isset($streamNeedsTee[$sourceId]);

                if (!$needsTee) {
                    // Stream used by only one group — direct connection via
                    // pipewiresink target-object.  No loopback needed.
                    continue;
                }

                // Stream fans out to multiple groups via a null-sink tee.
                // Create a loopback that captures the tee's monitor and
                // plays into this input group's combine-stream.
                $teeName = "fpp_tee_" . preg_replace('/[^a-zA-Z0-9_]/', '_', strtolower($sourceId));
                $streamSlug = preg_replace('/[^a-zA-Z0-9_]/', '_', strtolower($sourceId));
                $loopbackName = "fpp_loopback_ig{$groupId}_{$streamSlug}";
                $loopbackDesc = "$mbrName → $groupName";

                $volume = isset($mbr['volume']) ? floatval($mbr['volume']) / 100.0 : 1.0;

                $conf .= "  # Loopback (fppd stream fan-out via tee): $loopbackDesc\n";
                $conf .= "  { name = libpipewire-module-loopback\n";
                $conf .= "    args = {\n";
                $conf .= "      node.name = \"$loopbackName\"\n";
                $conf .= "      node.description = \"$loopbackDesc\"\n";
                $conf .= "      capture.props = {\n";
                $conf .= "        node.target = \"$teeName\"\n";
                $conf .= "        stream.capture.sink = true\n";
                $conf .= "        stream.dont-remix = true\n";
                $conf .= "      }\n";
                $conf .= "      playback.props = {\n";
                $conf .= "        node.target = \"$nodeName\"\n";
                $conf .= "        media.class = Stream/Output/Audio\n";
                if ($volume < 0.999) {
                    $conf .= "        channelmix.volume = " . round($volume, 3) . "\n";
                }
                $conf .= "      }\n";
                $conf .= "    }\n";
                $conf .= "  }\n";
                continue;
            }

            $loopbackName = "fpp_loopback_ig{$groupId}_" . preg_replace('/[^a-zA-Z0-9_]/', '_', strtolower($mbrName));
            $loopbackDesc = "$mbrName → $groupName";

            // Determine the source node target
            $sourceTarget = '';
            if ($mbrType === 'capture') {
                $cardId = isset($mbr['cardId']) ? $mbr['cardId'] : '';
                if (empty($cardId))
                    continue;
                // Use exact PipeWire node name if stored (from source picker)
                if (isset($mbr['nodeName']) && !empty($mbr['nodeName'])) {
                    $sourceTarget = $mbr['nodeName'];
                } else {
                    // Resolve from pw-dump at config generation time
                    $sourceTarget = ResolveAlsaCaptureNodeName($cardId);
                    if (empty($sourceTarget))
                        continue;
                }
            } elseif ($mbrType === 'pw_source') {
                // PipeWire Audio/Source node (e.g. from video input audio extraction)
                $sourceTarget = isset($mbr['nodeName']) ? $mbr['nodeName'] : '';
                if (empty($sourceTarget))
                    continue;
            } elseif ($mbrType === 'aes67_receive') {
                $instanceId = isset($mbr['instanceId']) ? $mbr['instanceId'] : '';
                if (empty($instanceId))
                    continue;
                $sourceTarget = $instanceId;
            } elseif ($mbrType === 'opus_rtp_receive') {
                $opusInstId = isset($mbr['instanceId']) ? intval($mbr['instanceId']) : 0;
                if ($opusInstId <= 0)
                    continue;
                // Resolve instance ID to PipeWire node name
                $opusCfgFile = $settings['mediaDirectory'] . "/config/pipewire-opus-rtp-instances.json";
                $sourceTarget = '';
                if (file_exists($opusCfgFile)) {
                    $opusCfg = json_decode(file_get_contents($opusCfgFile), true);
                    if ($opusCfg && isset($opusCfg['instances'])) {
                        foreach ($opusCfg['instances'] as $oi) {
                            if (isset($oi['id']) && intval($oi['id']) === $opusInstId && !empty($oi['enabled'])) {
                                $sourceTarget = 'opusrtp_' . preg_replace('/[^a-zA-Z0-9_]/', '_', strtolower($oi['name'])) . '_recv';
                                if (empty($mbrName)) {
                                    $mbrName = $oi['name'] . ' (Opus RTP)';
                                    $loopbackName = "fpp_loopback_ig{$groupId}_" . preg_replace('/[^a-zA-Z0-9_]/', '_', strtolower($mbrName));
                                    $loopbackDesc = "$mbrName → $groupName";
                                }
                                break;
                            }
                        }
                    }
                }
                if (empty($sourceTarget))
                    continue;
            } else {
                continue;
            }

            // Per-member volume (0-100 → 0.0-1.0), respecting mute flag
            $isMuted = isset($mbr['mute']) && $mbr['mute'] === true;
            $volume = $isMuted ? 0.0 : (isset($mbr['volume']) ? floatval($mbr['volume']) / 100.0 : 1.0);

            $conf .= "  # Loopback: $loopbackDesc\n";
            $conf .= "  { name = libpipewire-module-loopback\n";
            $conf .= "    args = {\n";
            $conf .= "      node.name = \"$loopbackName\"\n";
            $conf .= "      node.description = \"$loopbackDesc\"\n";
            $conf .= "      capture.props = {\n";
            $conf .= "        node.target = \"$sourceTarget\"\n";
            $conf .= "        media.class = Stream/Input/Audio\n";
            $conf .= "        stream.dont-remix = true\n";
            $conf .= "        resample.disable = false\n";

            // Channel mapping if specified
            if (isset($mbr['channelMapping']) && !empty($mbr['channelMapping'])) {
                $srcCh = $mbr['channelMapping']['sourceChannels'];
                $conf .= "        audio.position = [ " . implode(" ", $srcCh) . " ]\n";
            }

            $conf .= "      }\n";
            $conf .= "      playback.props = {\n";
            $conf .= "        node.target = \"$nodeName\"\n";
            $conf .= "        media.class = Stream/Output/Audio\n";
            $conf .= "        resample.disable = false\n";

            // Per-member volume via channelmix
            if ($volume < 0.999) {
                $conf .= "        channelmix.volume = " . round($volume, 3) . "\n";
            }

            // Channel mapping for the output side
            if (isset($mbr['channelMapping']) && !empty($mbr['channelMapping'])) {
                $grpCh = $mbr['channelMapping']['groupChannels'];
                $conf .= "        audio.position = [ " . implode(" ", $grpCh) . " ]\n";
            }

            $conf .= "      }\n";
            $conf .= "    }\n";
            $conf .= "  }\n";
        }

        // Output routing is handled by the combine-stream's stream.rules
        // above — no separate routing loopback modules needed.
    }

    $conf .= "]\n";

    return $conf;
}

/////////////////////////////////////////////////////////////////////////////
// Helper: Generate PipeWire combine-stream config from groups
function GeneratePipeWireGroupsConfig($groups, $returnCardMap = false)
{
    global $SUDO, $settings;

    $channelPositions = array(
        1 => "[ MONO ]",
        2 => "[ FL FR ]",
        3 => "[ FL FR FC ]",
        4 => "[ FL FR RL RR ]",
        5 => "[ FL FR FC RL RR ]",
        6 => "[ FL FR FC LFE RL RR ]",
        7 => "[ FL FR FC LFE RL RR RC ]",
        8 => "[ FL FR FC LFE RL RR SL SR ]"
    );

    $channelPositionArrays = array(
        1 => array("MONO"),
        2 => array("FL", "FR"),
        3 => array("FL", "FR", "FC"),
        4 => array("FL", "FR", "RL", "RR"),
        5 => array("FL", "FR", "FC", "RL", "RR"),
        6 => array("FL", "FR", "FC", "LFE", "RL", "RR"),
        7 => array("FL", "FR", "FC", "LFE", "RL", "RR", "RC"),
        8 => array("FL", "FR", "FC", "LFE", "RL", "RR", "SL", "SR")
    );

    $conf = "# Auto-generated by FPP - PipeWire Audio Output Groups\n";
    $conf .= "# Do not edit manually - managed via FPP UI\n\n";

    // -----------------------------------------------------------
    // Query existing PipeWire sink node names so we can match the
    // combine-stream rules against nodes that already exist
    // (created by WirePlumber or the 95-fpp-alsa-sink config).
    //
    // We build TWO maps for robust resolution:
    //   1. sinkCardIdMap:  ALSA card ID string → PipeWire node.name
    //      (primary — stable across reboots, no card-number dependency)
    //   2. sinkCardNumMap: ALSA card number → PipeWire node.name
    //      (fallback)
    //
    // Card IDs are read from /proc/asound/cardN/id which is the kernel's
    // stable identifier (e.g. "S3", "ICUSBAUDIO7D").
    // -----------------------------------------------------------
    $existingSinks = array(); // node.name => true
    $sinkCardNumMap = array(); // ALSA card number (int) => node.name
    $sinkCardIdMap = array();  // ALSA card ID (string) => node.name
    $sinkCardRateMap = array(); // ALSA card ID (string) => negotiated audio.rate (int)
    $env = "PIPEWIRE_RUNTIME_DIR=/run/pipewire-fpp XDG_RUNTIME_DIR=/run/pipewire-fpp";
    $pwDumpJson = '';
    exec($SUDO . " " . $env . " pw-dump 2>/dev/null", $pwDumpLines);
    $pwDumpJson = implode("\n", $pwDumpLines);
    unset($pwDumpLines);
    $pwDumpData = json_decode($pwDumpJson, true);
    unset($pwDumpJson);
    if (is_array($pwDumpData)) {
        foreach ($pwDumpData as $obj) {
            if (!isset($obj['type']) || $obj['type'] !== 'PipeWire:Interface:Node')
                continue;
            $props = isset($obj['info']['props']) ? $obj['info']['props'] : array();
            $nodeName = isset($props['node.name']) ? $props['node.name'] : '';
            $mediaClass = isset($props['media.class']) ? $props['media.class'] : '';
            if ($nodeName && $mediaClass === 'Audio/Sink') {
                $existingSinks[$nodeName] = isset($props['audio.channels']) ? intval($props['audio.channels']) : 2;
                $cn = -1;

                // WirePlumber-managed sinks have alsa.card set directly.
                if (isset($props['alsa.card'])) {
                    $cn = intval($props['alsa.card']);
                }
                // FPP-created and WP sinks may have api.alsa.path
                // (e.g. "hw:0", "hw:S3", "front:3").
                if ($cn < 0 && isset($props['api.alsa.path'])) {
                    $alsaPath = $props['api.alsa.path'];
                    // Match hw:X, front:X, surround*:X, iec958:X, default:X etc.
                    if (preg_match('/^[a-zA-Z0-9_]+:(\w+)/', $alsaPath, $hm)) {
                        $dev = $hm[1];
                        if (ctype_digit($dev)) {
                            $cn = intval($dev);
                        } else {
                            // Stable card ID — resolve via /proc/asound
                            $cn = ResolveCardIdToNumber($dev);
                        }
                    }
                }

                if ($cn >= 0) {
                    // FPP-managed nodes (fpp_alsa_*) always take priority over
                    // WirePlumber auto-discovered nodes for the same device.
                    $isFppNode = (strpos($nodeName, 'fpp_alsa_') === 0);
                    if ($isFppNode || !isset($sinkCardNumMap[$cn])) {
                        $sinkCardNumMap[$cn] = $nodeName;
                    }
                    // Reverse-resolve card number → stable ALSA card ID
                    // via /proc/asound/cardN/id so we can look up by cardId directly.
                    $cardIdFromProc = @file_get_contents("/proc/asound/card$cn/id");
                    if ($cardIdFromProc !== false) {
                        $cardIdFromProc = trim($cardIdFromProc);
                        if (!empty($cardIdFromProc)) {
                            if ($isFppNode || !isset($sinkCardIdMap[$cardIdFromProc])) {
                                $sinkCardIdMap[$cardIdFromProc] = $nodeName;
                            }
                            // Capture the rate WirePlumber negotiated for this device.
                            $nodeRate = isset($props['audio.rate']) ? intval($props['audio.rate']) : 0;
                            if ($nodeRate > 0 && !isset($sinkCardRateMap[$cardIdFromProc])) {
                                $sinkCardRateMap[$cardIdFromProc] = $nodeRate;
                            }
                        }
                    }
                }
            }
        }
    }
    unset($pwDumpData);

    // Resolve card IDs to PipeWire node names.
    // Priority order:
    //   1. Previously-stored nodeTarget in member JSON (survives PipeWire being down)
    //   2. Direct cardId→nodeName via sinkCardIdMap (no card-number dependency)
    //   3. cardId→cardNum→nodeName via sinkCardNumMap (legacy fallback)
    //   4. Create FPP ALSA adapter if card exists but has no PipeWire sink
    //   5. Skip card with warning (card not physically present)
    $cardNodeMap = array();   // cardId -> PipeWire node name
    $unresolvedCards = array();
    $customAlsaAdaptersForUnresolved = array(); // cardId -> adapter info for cards with no PipeWire sink

    foreach ($groups as $group) {
        if (!isset($group['enabled']) || !$group['enabled'])
            continue;
        if (!isset($group['members']) || empty($group['members']))
            continue;
        foreach ($group['members'] as $member) {
            $cardId = isset($member['cardId']) ? $member['cardId'] : '';
            if (empty($cardId) || isset($cardNodeMap[$cardId]))
                continue;

            // AES67 virtual sinks: cardId starts with "aes67_"
            if (strpos($cardId, 'aes67_') === 0) {
                // Look up the instance from the AES67 config to get node name
                $aes67File = $settings['mediaDirectory'] . "/config/pipewire-aes67-instances.json";
                if (file_exists($aes67File)) {
                    $aes67Json = json_decode(file_get_contents($aes67File), true);
                    if ($aes67Json && isset($aes67Json['instances'])) {
                        $aes67InstId = intval(str_replace('aes67_', '', $cardId));
                        foreach ($aes67Json['instances'] as $ai) {
                            if (isset($ai['id']) && intval($ai['id']) === $aes67InstId && isset($ai['enabled']) && $ai['enabled']) {
                                $aesNodeName = 'aes67_' . preg_replace('/[^a-zA-Z0-9_]/', '_', strtolower($ai['name'])) . '_send';
                                $cardNodeMap[$cardId] = $aesNodeName;
                                break;
                            }
                        }
                    }
                }
                if (!isset($cardNodeMap[$cardId])) {
                    $unresolvedCards[] = $cardId . " (AES67 instance not found or disabled)";
                }
                continue;
            }

            // Opus RTP virtual sinks: cardId starts with "opusrtp_"
            if (strpos($cardId, 'opusrtp_') === 0) {
                $opusFile = $settings['mediaDirectory'] . "/config/pipewire-opus-rtp-instances.json";
                if (file_exists($opusFile)) {
                    $opusJson = json_decode(file_get_contents($opusFile), true);
                    if ($opusJson && isset($opusJson['instances'])) {
                        $opusInstId = intval(str_replace('opusrtp_', '', $cardId));
                        foreach ($opusJson['instances'] as $oi) {
                            if (isset($oi['id']) && intval($oi['id']) === $opusInstId && isset($oi['enabled']) && $oi['enabled']) {
                                $opusNodeName = 'opusrtp_' . preg_replace('/[^a-zA-Z0-9_]/', '_', strtolower($oi['name'])) . '_send';
                                $cardNodeMap[$cardId] = $opusNodeName;
                                break;
                            }
                        }
                    }
                }
                if (!isset($cardNodeMap[$cardId])) {
                    $unresolvedCards[] = $cardId . " (Opus RTP instance not found or disabled)";
                }
                continue;
            }

            // Priority 1: Previously-stored nodeTarget from last successful Apply
            if (isset($member['nodeTarget']) && !empty($member['nodeTarget'])) {
                $storedTarget = $member['nodeTarget'];
                // Verify it still exists in PipeWire; if so, use it directly
                if (isset($existingSinks[$storedTarget])) {
                    $cardNodeMap[$cardId] = $storedTarget;
                    continue;
                }
                // Even if not currently in PipeWire (device unplugged), the
                // WirePlumber node name is deterministic and will be correct
                // when the device reappears.  Use it as a last-resort below.
            }

            // Priority 2: Direct cardId → node name (no card-number dependency)
            if (isset($sinkCardIdMap[$cardId])) {
                $cardNodeMap[$cardId] = $sinkCardIdMap[$cardId];
                continue;
            }

            // Priority 3: cardId → card number → node name
            $cardNum = ResolveCardIdToNumber($cardId);
            if ($cardNum >= 0 && isset($sinkCardNumMap[$cardNum])) {
                $cardNodeMap[$cardId] = $sinkCardNumMap[$cardNum];
                continue;
            }

            // Priority 4: Use stored nodeTarget even though device isn't present
            // WirePlumber names are deterministic (based on USB VID/PID/serial),
            // so the stored name will be correct when the device reappears.
            // However, fpp_alsa_* nodes require a physical ALSA device — if the
            // card isn't present we must NOT create an adapter for it (PipeWire
            // crashes fatally trying to open a missing ALSA device).
            if (isset($member['nodeTarget']) && !empty($member['nodeTarget'])) {
                if (strpos($member['nodeTarget'], 'fpp_alsa_') === 0) {
                    $p4CardNum = ResolveCardIdToNumber($cardId);
                    if ($p4CardNum < 0) {
                        $unresolvedCards[] = $cardId . " (device unplugged — will be restored when reconnected)";
                        continue;
                    }
                }
                $cardNodeMap[$cardId] = $member['nodeTarget'];
                continue;
            }

            // Could not resolve — if the ALSA card is present but has no
            // PipeWire sink (e.g. HDMI with profile=Off, disabled WirePlumber
            // device), create an FPP ALSA adapter node for it.
            // First verify the PCM device can actually be opened (HDMI outputs
            // fail if nothing is connected to the port).
            if ($cardNum >= 0) {
                $testOutput = shell_exec("timeout 2 aplay -D hw:$cardId --dump-hw-params /dev/zero 2>&1");
                $canOpen = (strpos($testOutput, 'HW Params') !== false);
                // Also verify card supports standard PCM formats (not IEC958/passthrough only)
                $hasPcmFmt = (strpos($testOutput, 'S16_LE') !== false || strpos($testOutput, 'S24_LE') !== false
                    || strpos($testOutput, 'S24_3LE') !== false || strpos($testOutput, 'S32_LE') !== false);
                // On some hardware (e.g. Pi Zero 2 W vc4-hdmi with KMS), the
                // raw hw: device only exposes IEC958_SUBFRAME_LE.  PipeWire's
                // SPA ALSA plugin fatally crashes when it encounters IEC958-only
                // hw: devices because it tries to open a passthrough variant
                // (appending "p" to the card name) which doesn't exist.  Using
                // sysdefault: instead routes through ALSA's dmix/plug layer
                // which handles the IEC958-to-PCM format conversion and avoids
                // the passthrough probe crash.
                $needsSysdefault = false;
                if ($canOpen && !$hasPcmFmt
                    && strpos($testOutput, 'IEC958_SUBFRAME_LE') !== false) {
                    $sysOutput = shell_exec("timeout 2 aplay -D sysdefault:$cardId --dump-hw-params /dev/zero 2>&1");
                    $sysCanOpen = (strpos($sysOutput, 'HW Params') !== false);
                    $sysHasPcm = (strpos($sysOutput, 'S16_LE') !== false || strpos($sysOutput, 'S24_LE') !== false
                        || strpos($sysOutput, 'S24_3LE') !== false || strpos($sysOutput, 'S32_LE') !== false);
                    if ($sysCanOpen && $sysHasPcm) {
                        $hasPcmFmt = true;
                        $needsSysdefault = true;
                        $testOutput = $sysOutput;
                    }
                }
                if ($canOpen && $hasPcmFmt) {
                    $cidNorm = preg_replace('/[^a-zA-Z0-9_]/', '_', strtolower($cardId));
                    $adapterName = 'fpp_alsa_' . $cidNorm;
                    $cardNodeMap[$cardId] = $adapterName;
                    if (!isset($customAlsaAdaptersForUnresolved[$cardId])) {
                        // Detect channels from ALSA HW params.  A continuous
                        // range "[lo hi]" (e.g. bcm2835 onboard analog reports
                        // "[1 8]" due to 8 mixer subdevices, not 8 physical
                        // outputs) must default to stereo — opening it as 8ch
                        // feeds garbled high-pitched audio to a stereo DAC.
                        // Only fixed/discrete declarations use the max value.
                        $unresolvedMaxCh = 2;
                        if (preg_match('/CHANNELS\[?\d*\]?:\s+(.+)/i', $testOutput, $chM)) {
                            $chLine = $chM[1];
                            preg_match_all('/\d+/', $chLine, $chNums);
                            if (!empty($chNums[0])) {
                                $cn = array_map('intval', $chNums[0]);
                                if (strpos($chLine, '[') !== false && count($cn) >= 2) {
                                    // Continuous range [lo hi]: prefer stereo within range.
                                    $unresolvedMaxCh = min(max(2, $cn[0]), end($cn));
                                } else {
                                    $unresolvedMaxCh = min(8, max($cn));
                                }
                            }
                        }
                        // Detect best audio format: S32 > S24 > S16
                        $unresolvedFmt = 'S16LE';
                        if (preg_match('/FORMAT[^:]*:\s+(.+)/i', $testOutput, $fmtM)) {
                            $fmtLine = $fmtM[1];
                            if (strpos($fmtLine, 'S32_LE') !== false) {
                                $unresolvedFmt = 'S32LE';
                            } elseif (strpos($fmtLine, 'S24_LE') !== false) {
                                $unresolvedFmt = 'S24_32LE';
                            } elseif (strpos($fmtLine, 'S24_3LE') !== false) {
                                $unresolvedFmt = 'S24LE';
                            }
                        }
                        $customAlsaAdaptersForUnresolved[$cardId] = array(
                            'nodeName' => $adapterName,
                            'channels' => $unresolvedMaxCh,
                            'rate' => 0,
                            'format' => $unresolvedFmt,
                            'needsSysdefault' => $needsSysdefault,
                        );
                    }
                } else {
                    $unresolvedCards[] = $cardId . " (card $cardNum — device cannot be opened)";
                }
            } else {
                $unresolvedCards[] = $cardId . " (ALSA card not present)";
            }
        }
    }

    if (!empty($unresolvedCards)) {
        $conf .= "# WARNING: Could not find PipeWire sinks for: " . implode(', ', $unresolvedCards) . "\n";
        $conf .= "# These cards will be skipped from combine groups.\n\n";
    }

    // ---------------------------------------------------------------
    // Phase 0: Generate custom ALSA adapter nodes for members that
    // need more channels than the default WirePlumber/FPP-boot nodes
    // provide (typically 2ch stereo).  These adapters open the ALSA
    // device with the correct channel count so all physical outputs
    // are driven.  The filter-chain playback then targets our custom
    // adapter instead of the default stereo node.
    // ---------------------------------------------------------------
    $customAlsaAdapters = array(); // cardId -> array('nodeName','channels','rate','periodSize','format')

    foreach ($groups as $group) {
        if (!isset($group['enabled']) || !$group['enabled'])
            continue;
        if (!isset($group['members']) || empty($group['members']))
            continue;
        foreach ($group['members'] as $member) {
            $memberCh = isset($member['channels']) ? intval($member['channels']) : 2;
            $cid = isset($member['cardId']) ? $member['cardId'] : '';
            if (empty($cid))
                continue;
            $memberRate = isset($member['sampleRate']) ? intval($member['sampleRate']) : 0;
            $memberPeriod = isset($member['periodSize']) ? intval($member['periodSize']) : 0;
            // Skip AES67 virtual sinks — they don't use ALSA adapters
            if (strpos($cid, 'aes67_') === 0)
                continue;
            if (!isset($cardNodeMap[$cid]))
                continue;
            // If the resolved target is an fpp_alsa_* adapter name that does
            // not exist as a real PipeWire sink, we MUST create that adapter
            // node — otherwise the filter-chain / combine-stream playback
            // targets a non-existent node and the chain is never linked to
            // the hardware (silent output).  This happens for plain 2ch
            // members whose cardId resolved via the stored-nodeTarget
            // fallback (Priority 4) instead of a live WirePlumber sink.
            $resolvedTarget = isset($cardNodeMap[$cid]) ? $cardNodeMap[$cid] : '';
            $targetIsMissingFppAdapter = (strpos($resolvedTarget, 'fpp_alsa_') === 0)
                && !isset($existingSinks[$resolvedTarget]);
            // If an fpp_alsa_* boot-time adapter already exists with enough channels,
            // don't create a duplicate — the boot-time node already covers this card.
            $existingAdapterChannels = (strpos($resolvedTarget, 'fpp_alsa_') === 0 && isset($existingSinks[$resolvedTarget]))
                ? $existingSinks[$resolvedTarget] : 0;
            $bootAdapterSufficient = ($existingAdapterChannels >= $memberCh);
            // Need a custom adapter if channels >2 (and not already covered by boot node),
            // explicit rate/period override, or the config references an fpp_alsa_* node
            // that nothing else creates.
            $needsCustom = (($memberCh > 2 && !$bootAdapterSufficient) || $memberRate > 0 || $memberPeriod > 0
                || $targetIsMissingFppAdapter);
            if (!$needsCustom)
                continue;
            // Verify ALSA device is still present (may have been unplugged)
            if (ResolveCardIdToNumber($cid) < 0)
                continue;
            // Track max channels needed per card (same card in multiple groups)
            if (!isset($customAlsaAdapters[$cid]) || $memberCh > $customAlsaAdapters[$cid]['channels']) {
                $cidNorm = preg_replace('/[^a-zA-Z0-9_]/', '_', strtolower($cid));
                // Detect best audio format from ALSA HW params
                $adapterFmt = 'S16LE';
                $adapterNeedsSysdefault = false;
                $cidSafe = preg_match('/^[a-zA-Z0-9_]+$/', $cid) ? $cid : strval(ResolveCardIdToNumber($cid));
                if (!empty($cidSafe)) {
                    $fmtOut = shell_exec("timeout 2 aplay -D hw:" . escapeshellarg($cidSafe) . " --dump-hw-params /dev/zero 2>&1 | head -40");
                    // If hw: only exposes IEC958 (e.g. Pi Zero 2 W HDMI with
                    // KMS), fall back to sysdefault: for PCM format conversion.
                    // Using sysdefault: instead of plughw: avoids PipeWire's SPA
                    // ALSA plugin crashing when it probes for a passthrough
                    // variant by appending "p" to the device path.
                    if ($fmtOut && strpos($fmtOut, 'IEC958_SUBFRAME_LE') !== false
                        && strpos($fmtOut, 'S16_LE') === false && strpos($fmtOut, 'S32_LE') === false) {
                        $sysFmtOut = shell_exec("timeout 2 aplay -D sysdefault:" . escapeshellarg($cidSafe) . " --dump-hw-params /dev/zero 2>&1 | head -40");
                        if ($sysFmtOut && (strpos($sysFmtOut, 'S16_LE') !== false || strpos($sysFmtOut, 'S32_LE') !== false)) {
                            $fmtOut = $sysFmtOut;
                            $adapterNeedsSysdefault = true;
                        }
                    }
                    if ($fmtOut && preg_match('/FORMAT[^:]*:\s+(.+)/i', $fmtOut, $fmtM)) {
                        $fmtLine = $fmtM[1];
                        if (strpos($fmtLine, 'S32_LE') !== false) {
                            $adapterFmt = 'S32LE';
                        } elseif (strpos($fmtLine, 'S24_LE') !== false) {
                            $adapterFmt = 'S24_32LE';
                        } elseif (strpos($fmtLine, 'S24_3LE') !== false) {
                            $adapterFmt = 'S24LE';
                        }
                    }
                }
                $customAlsaAdapters[$cid] = array(
                    'nodeName' => 'fpp_alsa_' . $cidNorm,
                    'channels' => $memberCh,
                    'rate' => $memberRate,
                    'periodSize' => $memberPeriod,
                    'format' => $adapterFmt,
                    'needsSysdefault' => $adapterNeedsSysdefault,
                );
            } else {
                // Card already tracked — merge per-card overrides (first non-zero wins)
                if ($memberRate > 0 && $customAlsaAdapters[$cid]['rate'] == 0) {
                    $customAlsaAdapters[$cid]['rate'] = $memberRate;
                }
                if ($memberPeriod > 0 && $customAlsaAdapters[$cid]['periodSize'] == 0) {
                    $customAlsaAdapters[$cid]['periodSize'] = $memberPeriod;
                }
            }
        }
    }

    // Merge adapters for unresolved cards (no PipeWire sink) into the
    // custom adapter list.  Multi-channel adapters take priority — if a card
    // already has a multi-channel adapter, don't downgrade to 2ch.
    foreach ($customAlsaAdaptersForUnresolved as $cid => $info) {
        if (!isset($customAlsaAdapters[$cid])) {
            $customAlsaAdapters[$cid] = $info;
        }
    }

    if (!empty($customAlsaAdapters)) {
        // ── Per-device sample rate resolution ──────────────────────────────
        // Priority (highest to lowest):
        //   1. Rate already negotiated by WirePlumber for that card (from pw-dump)
        //   2. Rate queried directly from ALSA HW params via aplay --dump-hw-params
        //   3. Global FPP AudioFormat setting as last-resort fallback
        //
        // PipeWire's allowed-rates = [ 44100 48000 96000 ] means the graph
        // clock can run at any of these.  We use the same list as candidates.
        $allowedRates = array(44100, 48000, 96000);
        $audioFormat = isset($settings['AudioFormat']) ? intval($settings['AudioFormat']) : 0;
        if ($audioFormat >= 7) {
            $alsaRate = 96000;
        } elseif ($audioFormat >= 4) {
            $alsaRate = 48000;
        } else {
            $alsaRate = 44100;
        }

        $conf .= "# Custom FPP ALSA adapter nodes\n";
        $conf .= "# These provide sinks for cards with no WirePlumber node or needing extra channels\n";
        $conf .= "context.objects = [\n";
        foreach ($customAlsaAdapters as $cid => $info) {
            // Verify ALSA device is physically present before creating adapter.
            // A missing device causes PipeWire to crash fatally on startup.
            if (ResolveCardIdToNumber($cid) < 0) {
                $conf .= "  # SKIPPED: $cid — ALSA card not present (device unplugged?)\n";
                unset($cardNodeMap[$cid]);
                continue;
            }
            $posStr = isset($channelPositions[$info['channels']]) ? $channelPositions[$info['channels']] : "[ FL FR ]";
            $cardLabel = $cid;
            $conf .= "  { factory = adapter\n";
            $conf .= "    args = {\n";
            $conf .= "      factory.name = api.alsa.pcm.sink\n";
            $conf .= "      node.name = \"" . $info['nodeName'] . "\"\n";
            // Read USB product name for consistent description
            $cardNumForDesc = ResolveCardIdToNumber($cid);
            $productNameForDesc = $cid;
            if ($cardNumForDesc >= 0) {
                $sysfsProduct = @file_get_contents("/sys/class/sound/card$cardNumForDesc/device/product");
                if ($sysfsProduct !== false && !empty(trim($sysfsProduct))) {
                    $productNameForDesc = trim($sysfsProduct);
                }
            }
            $descStr = $productNameForDesc . " (" . $cid . ")";
            if ($info['channels'] > 2) {
                $descStr .= " " . $info['channels'] . "ch";
            }
            $conf .= "      node.description = \"$descStr\"\n";
            $conf .= "      media.class = \"Audio/Sink\"\n";
            $alsaPrefix = (!empty($info['needsSysdefault'])) ? 'sysdefault' : 'hw';
            $conf .= "      api.alsa.path = \"$alsaPrefix:$cid\"\n";
            $adapterPeriod = isset($info['periodSize']) && $info['periodSize'] > 0 ? intval($info['periodSize']) : 1024;
            $conf .= "      api.alsa.period-size = $adapterPeriod\n";
            // USB audio cards need extra headroom: their independent oscillators
            // drift relative to the PipeWire graph driver clock, causing resyncs.
            $cardNum = ResolveCardIdToNumber($cid);
            $isUsb = false;
            if ($cardNum >= 0) {
                $driverLink = @readlink("/sys/class/sound/card$cardNum/device/driver");
                $isUsb = ($driverLink !== false && str_contains(basename($driverLink), 'usb'));
            }
            $adapterHeadroom = $isUsb ? 4096 : 256;
            $conf .= "      api.alsa.headroom = $adapterHeadroom\n";
            $adapterFormat = isset($info['format']) ? $info['format'] : 'S16LE';
            $conf .= "      audio.format = \"$adapterFormat\"\n";
            $adapterRate = isset($info['rate']) && $info['rate'] > 0 ? intval($info['rate']) : 0;
            if ($adapterRate == 0) {
                // Fall back to auto-detection: WirePlumber negotiated rate, then ALSA HW query, then global setting
                if (isset($sinkCardRateMap[$cid])) {
                    $adapterRate = $sinkCardRateMap[$cid];
                } else {
                    $adapterRate = QueryAlsaCardBestRate($cid, $allowedRates, $alsaRate);
                }
            }
            $conf .= "      audio.rate = " . $adapterRate . "\n";
            $conf .= "      audio.channels = " . $info['channels'] . "\n";
            $conf .= "      audio.position = $posStr\n";
            $conf .= "    }\n";
            $conf .= "  }\n";
            // Override cardNodeMap so filter-chain targets our multi-channel adapter
            $cardNodeMap[$cid] = $info['nodeName'];
        }
        $conf .= "]\n\n";
    }

    // Create modules array: filter-chain modules first, then combine-stream
    $conf .= "context.modules = [\n";

    // ---------------------------------------------------------------
    // Phase 1: Generate filter-chain modules for members that need
    // EQ processing and/or delay compensation.
    // These must load before combine-stream so their virtual sinks
    // exist when combine-stream scans for matching nodes.
    //
    // Three cases per member:
    //   a) EQ only         → filter-chain with biquad nodes
    //   b) Delay only      → filter-chain with delay nodes
    //   c) EQ + Delay      → single filter-chain: delay → EQ chain
    // ---------------------------------------------------------------
    $filterNodeMap = array();  // "groupId_cardId" -> filter virtual sink node name
    $channelLabels = array("l", "r", "c", "lfe", "rl", "rr", "sl", "sr");

    foreach ($groups as $group) {
        if (!isset($group['enabled']) || !$group['enabled'])
            continue;
        if (!isset($group['members']) || empty($group['members']))
            continue;

        $groupId = isset($group['id']) ? intval($group['id']) : 0;

        foreach ($group['members'] as $member) {
            $cardId = isset($member['cardId']) ? $member['cardId'] : '';
            if (empty($cardId) || !isset($cardNodeMap[$cardId]))
                continue;

            $hasEQ = isset($member['eq']['enabled']) && $member['eq']['enabled']
                && isset($member['eq']['bands']) && !empty($member['eq']['bands']);
            $delayMs = isset($member['delayMs']) ? floatval($member['delayMs']) : 0;
            // Always create delay nodes so real-time adjustment is possible during calibration
            $hasDelay = true;

            $memberChannels = isset($member['channels']) ? intval($member['channels']) : 2;
            $numCh = min($memberChannels, count($channelLabels));
            $positions = isset($channelPositionArrays[$memberChannels]) ? $channelPositionArrays[$memberChannels] : $channelPositionArrays[2];
            $posStr = "[ " . implode(" ", $positions) . " ]";

            $realNodeName = $cardNodeMap[$cardId];
            $cardIdNorm = preg_replace('/[^a-zA-Z0-9_]/', '_', strtolower($cardId));
            // Use "fpp_fx_" prefix for the unified filter-chain node name
            $fxNodeName = "fpp_fx_g" . $groupId . "_" . $cardIdNorm;
            $fxOutName = $fxNodeName . "_out";
            $fxKey = $groupId . "_" . $cardId;

            // Build description
            $cardLabel = isset($member['cardName']) ? $member['cardName'] : $cardId;
            $fxParts = array();
            if ($hasDelay)
                $fxParts[] = "Delay";
            if ($hasEQ)
                $fxParts[] = "EQ";
            $fxDesc = implode("+", $fxParts) . ": " . $cardLabel;

            $conf .= "  # Filter chain (" . implode("+", $fxParts) . ") for: $cardLabel (Group $groupId)\n";
            $conf .= "  { name = libpipewire-module-filter-chain\n";
            $conf .= "    args = {\n";
            $conf .= "      node.description = \"$fxDesc\"\n";
            $conf .= "      filter.graph = {\n";
            $conf .= "        nodes = [\n";

            // --- Delay nodes (one per channel) ---
            if ($hasDelay) {
                $delaySec = $delayMs / 1000.0;
                $maxDelay = max(5.0, $delaySec * 1.5);
                for ($ch = 0; $ch < $numCh; $ch++) {
                    $chLabel = $channelLabels[$ch];
                    $conf .= "          { type = builtin label = delay name = delay_{$chLabel} config = { \"max-delay\" = $maxDelay } control = { \"Delay (s)\" = $delaySec } }\n";
                }
            }

            // --- EQ nodes (one per channel x band) ---
            if ($hasEQ) {
                $bands = $member['eq']['bands'];
                for ($ch = 0; $ch < $numCh; $ch++) {
                    $chLabel = $channelLabels[$ch];
                    foreach ($bands as $bi => $band) {
                        $type = isset($band['type']) ? $band['type'] : 'bq_peaking';
                        $freq = floatval(isset($band['freq']) ? $band['freq'] : 1000);
                        $gain = floatval(isset($band['gain']) ? $band['gain'] : 0);
                        $q = floatval(isset($band['q']) ? $band['q'] : 1.0);
                        $conf .= "          { type = builtin label = $type name = eq_{$chLabel}_{$bi} control = { \"Freq\" = $freq \"Q\" = $q \"Gain\" = $gain } }\n";
                    }
                }
            }

            $conf .= "        ]\n";

            // --- Links: chain delay → EQ in series for each channel ---
            $conf .= "        links = [\n";
            for ($ch = 0; $ch < $numCh; $ch++) {
                $chLabel = $channelLabels[$ch];

                if ($hasDelay && $hasEQ) {
                    // Link delay output → first EQ band input
                    $conf .= "          { output = \"delay_{$chLabel}:Out\" input = \"eq_{$chLabel}_0:In\" }\n";
                }

                if ($hasEQ) {
                    $bands = $member['eq']['bands'];
                    // Chain EQ bands in series
                    for ($bi = 1; $bi < count($bands); $bi++) {
                        $prevBi = $bi - 1;
                        $conf .= "          { output = \"eq_{$chLabel}_{$prevBi}:Out\" input = \"eq_{$chLabel}_{$bi}:In\" }\n";
                    }
                }
            }
            $conf .= "        ]\n";

            // --- Inputs: first node of each channel's chain ---
            $conf .= "        inputs = [";
            for ($ch = 0; $ch < $numCh; $ch++) {
                $chLabel = $channelLabels[$ch];
                if ($hasDelay) {
                    $conf .= " \"delay_{$chLabel}:In\"";
                } else {
                    $conf .= " \"eq_{$chLabel}_0:In\"";
                }
            }
            $conf .= " ]\n";

            // --- Outputs: last node of each channel's chain ---
            $conf .= "        outputs = [";
            for ($ch = 0; $ch < $numCh; $ch++) {
                $chLabel = $channelLabels[$ch];
                if ($hasEQ) {
                    $lastBi = count($member['eq']['bands']) - 1;
                    $conf .= " \"eq_{$chLabel}_{$lastBi}:Out\"";
                } else {
                    $conf .= " \"delay_{$chLabel}:Out\"";
                }
            }
            $conf .= " ]\n";

            $conf .= "      }\n"; // filter.graph

            // Capture props (virtual sink that combine-stream will match)
            $conf .= "      capture.props = {\n";
            $conf .= "        node.name = \"$fxNodeName\"\n";
            $conf .= "        media.class = Audio/Sink\n";
            $conf .= "        audio.channels = $numCh\n";
            $conf .= "        audio.position = $posStr\n";
            $conf .= "";
            $conf .= "      }\n";

            // Playback props (output to real sink)
            $conf .= "      playback.props = {\n";
            $conf .= "        node.name = \"$fxOutName\"\n";
            $conf .= "        node.target = \"$realNodeName\"\n";
            $conf .= "        stream.dont-remix = true\n";
            $conf .= "        audio.channels = $numCh\n";
            $conf .= "        audio.position = $posStr\n";
            $conf .= "";
            $conf .= "      }\n";

            $conf .= "    }\n"; // args
            $conf .= "  }\n";

            $filterNodeMap[$fxKey] = $fxNodeName;
        }
    }

    // Backward compat: $eqNodeMap is now $filterNodeMap
    $eqNodeMap = $filterNodeMap;

    // ---------------------------------------------------------------
    // Phase 2: Generate combine-stream modules for each group
    // ---------------------------------------------------------------
    foreach ($groups as $group) {
        if (!isset($group['enabled']) || !$group['enabled'])
            continue;
        if (!isset($group['members']) || empty($group['members']))
            continue;

        $groupName = isset($group['name']) ? $group['name'] : "Group";
        $nodeName = "fpp_group_" . preg_replace('/[^a-zA-Z0-9_]/', '_', strtolower($groupName));
        $groupChannels = isset($group['channels']) ? intval($group['channels']) : 2;
        $groupPos = isset($channelPositions[$groupChannels]) ? $channelPositions[$groupChannels] : "[ FL FR ]";
        $latencyCompensate = (isset($group['latencyCompensate']) && $group['latencyCompensate']) ? "true" : "false";
        $groupId = isset($group['id']) ? intval($group['id']) : 0;

        // Check if this group has at least one resolvable member
        $hasMembers = false;
        foreach ($group['members'] as $member) {
            $cardId = isset($member['cardId']) ? $member['cardId'] : '';
            if (!empty($cardId) && isset($cardNodeMap[$cardId])) {
                $hasMembers = true;
                break;
            }
        }
        if (!$hasMembers)
            continue;

        $conf .= "  { name = libpipewire-module-combine-stream\n";
        $conf .= "    args = {\n";
        $conf .= "      combine.mode = sink\n";
        $conf .= "      node.name = \"$nodeName\"\n";
        $conf .= "      node.description = \"$groupName\"\n";
        $conf .= "      combine.latency-compensate = $latencyCompensate\n";
        $conf .= "      combine.props = {\n";
        $conf .= "        audio.position = $groupPos\n";
        $conf .= "";
        $conf .= "      }\n";
        $conf .= "      stream.props = {\n";
        $conf .= "        stream.dont-remix = true\n";
        $conf .= "      }\n";
        $conf .= "      stream.rules = [\n";

        foreach ($group['members'] as $member) {
            $cardId = isset($member['cardId']) ? $member['cardId'] : '';
            if (empty($cardId) || !isset($cardNodeMap[$cardId]))
                continue;

            // Use EQ virtual sink if filter-chain was generated for this member
            $eqKey = $groupId . "_" . $cardId;
            if (isset($eqNodeMap[$eqKey])) {
                $memberNodeName = $eqNodeMap[$eqKey];
            } else {
                $memberNodeName = $cardNodeMap[$cardId];
            }

            // Channel mapping for this member within the group
            $memberChannels = isset($member['channels']) ? intval($member['channels']) : 2;
            $memberPos = isset($channelPositions[$memberChannels]) ? $channelPositions[$memberChannels] : "[ FL FR ]";

            // If channel mapping is specified, use it
            if (isset($member['channelMapping']) && !empty($member['channelMapping'])) {
                $combinePos = "[ " . implode(" ", $member['channelMapping']['groupChannels']) . " ]";
                $streamPos = "[ " . implode(" ", $member['channelMapping']['cardChannels']) . " ]";
            } else {
                $combinePos = $memberPos;
                $streamPos = $memberPos;
            }

            $conf .= "        { matches = [\n";
            $conf .= "            { media.class = \"Audio/Sink\"\n";
            $conf .= "              node.name = \"$memberNodeName\"\n";
            $conf .= "            }\n";
            $conf .= "          ]\n";
            $conf .= "          actions = {\n";
            $conf .= "            create-stream = {\n";
            $conf .= "              node.target = \"$memberNodeName\"\n";
            $conf .= "              combine.audio.position = $combinePos\n";
            $conf .= "              audio.position = $streamPos\n";
            $conf .= "            }\n";
            $conf .= "          }\n";
            $conf .= "        }\n";
        }

        $conf .= "      ]\n";
        $conf .= "    }\n";
        $conf .= "  }\n";
    }

    $conf .= "]\n";

    if ($returnCardMap) {
        return array('conf' => $conf, 'cardNodeMap' => $cardNodeMap);
    }
    return $conf;
}

/////////////////////////////////////////////////////////////////////////////
// RestorePipeWireGroupVolumes
// Apply per-group and per-member volume levels from the audio groups JSON
// to the running PipeWire sinks via pactl.  Call after PipeWire restart.
function RestorePipeWireGroupVolumes($groups = null)
{
    global $SUDO, $settings;

    if ($groups === null) {
        $configFile = $settings['mediaDirectory'] . "/config/pipewire-audio-groups.json";
        if (!file_exists($configFile))
            return;
        $data = json_decode(file_get_contents($configFile), true);
        if (!is_array($data) || !isset($data['groups']))
            return;
        $groups = $data['groups'];
    }

    $env = "PIPEWIRE_RUNTIME_DIR=/run/pipewire-fpp XDG_RUNTIME_DIR=/run/pipewire-fpp PULSE_RUNTIME_PATH=/run/pipewire-fpp/pulse";

    foreach ($groups as $group) {
        if (!isset($group['enabled']) || !$group['enabled'])
            continue;
        if (!isset($group['members']) || empty($group['members']))
            continue;

        $groupName = isset($group['name']) ? $group['name'] : 'Group';
        $groupNodeName = 'fpp_group_' . preg_replace('/[^a-zA-Z0-9_]/', '_', strtolower($groupName));
        $groupId = isset($group['id']) ? intval($group['id']) : 0;

        // Set group master volume
        $groupVol = isset($group['volume']) ? intval($group['volume']) : 100;
        exec($SUDO . " " . $env . " pactl set-sink-volume " . escapeshellarg($groupNodeName) . " {$groupVol}% 2>/dev/null");

        // Set per-member volumes on the filter-chain sink nodes.
        // Also set the underlying WirePlumber-managed node to 100% when the
        // member targets one (e.g. HDMI outputs like
        // alsa_output.platform-*.hdmi.*).  WirePlumber initialises these at
        // ~40% which would silently attenuate HDMI audio.  FPP-owned nodes
        // (fpp_alsa_*, aes67_*) are already created at full volume and are
        // not touched.
        foreach ($group['members'] as $member) {
            $cardId = isset($member['cardId']) ? $member['cardId'] : '';
            if (empty($cardId))
                continue;
            $memberVol = isset($member['volume']) ? intval($member['volume']) : 100;
            $cardIdNorm = preg_replace('/[^a-zA-Z0-9_]/', '_', strtolower($cardId));
            $fxNodeName = 'fpp_fx_g' . $groupId . '_' . $cardIdNorm;
            exec($SUDO . " " . $env . " pactl set-sink-volume " . escapeshellarg($fxNodeName) . " {$memberVol}% 2>/dev/null");

            // Restore underlying WirePlumber-managed sink to 100% so it does
            // not silently attenuate the audio delivered by the filter chain.
            $nodeTarget = isset($member['nodeTarget']) ? $member['nodeTarget'] : '';
            if (!empty($nodeTarget)
                && strpos($nodeTarget, 'fpp_') !== 0
                && strpos($nodeTarget, 'aes67_') !== 0) {
                exec($SUDO . " " . $env . " pactl set-sink-volume " . escapeshellarg($nodeTarget) . " 100% 2>/dev/null");
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
//  AES67 MULTI-INSTANCE API
/////////////////////////////////////////////////////////////////////////////

// GET /api/pipewire/aes67/instances
function GetAES67Instances()
{
    global $settings;
    $configFile = $settings['mediaDirectory'] . "/config/pipewire-aes67-instances.json";
    if (file_exists($configFile)) {
        $data = json_decode(file_get_contents($configFile), true);
        if ($data !== null) {
            return json($data);
        }
    }
    return json(array("instances" => array(), "ptpEnabled" => true, "ptpInterface" => ""));
}

// POST /api/pipewire/aes67/instances
function SaveAES67Instances()
{
    global $settings;
    $configFile = $settings['mediaDirectory'] . "/config/pipewire-aes67-instances.json";

    $data = file_get_contents('php://input');
    $parsed = json_decode($data, true);
    if ($parsed === null) {
        http_response_code(400);
        return json(array("status" => "ERROR", "message" => "Invalid JSON"));
    }
    // Validate structure
    if (!isset($parsed['instances']) || !is_array($parsed['instances'])) {
        http_response_code(400);
        return json(array("status" => "ERROR", "message" => "Missing instances array"));
    }
    // Validate each instance
    $nextId = 1;
    foreach ($parsed['instances'] as &$inst) {
        if (!isset($inst['id'])) {
            $inst['id'] = $nextId;
        }
        if ($inst['id'] >= $nextId)
            $nextId = $inst['id'] + 1;
        if (empty($inst['name']))
            $inst['name'] = 'AES67 Instance ' . $inst['id'];
        if (empty($inst['mode']))
            $inst['mode'] = 'send';
        if (empty($inst['multicastIP']))
            $inst['multicastIP'] = '239.69.0.' . $inst['id'];
        if (empty($inst['port']))
            $inst['port'] = 5004;
        if (empty($inst['channels']))
            $inst['channels'] = 2;
        if (empty($inst['sessionName']))
            $inst['sessionName'] = $inst['name'];
        if (!isset($inst['ptime']))
            $inst['ptime'] = 4;
        if (!isset($inst['latency']))
            $inst['latency'] = 10;
        if (!isset($inst['sapEnabled']))
            $inst['sapEnabled'] = true;
        if (!isset($inst['enabled']))
            $inst['enabled'] = true;
    }
    unset($inst);

    file_put_contents($configFile, json_encode($parsed, JSON_PRETTY_PRINT));
    return json(array("status" => "OK"));
}

// POST /api/pipewire/aes67/apply
function ApplyAES67Instances()
{
    global $settings;

    // AES67 is managed by AES67Manager in fppd (GStreamer-based).
    // Signal fppd to reload AES67 config via the command API.
    $configFile = $settings['mediaDirectory'] . "/config/pipewire-aes67-instances.json";

    if (!file_exists($configFile)) {
        // Signal cleanup
        $result = @file_get_contents('http://localhost/api/command', false, stream_context_create(array(
            'http' => array(
                'method' => 'POST',
                'header' => 'Content-Type: application/json',
                'content' => json_encode(array('command' => 'AES67 Cleanup')),
                'timeout' => 5
            )
        )));
        return json(array("status" => "OK", "message" => "No AES67 instances configured"));
    }

    // Signal fppd to apply config
    $result = @file_get_contents('http://localhost/api/command', false, stream_context_create(array(
        'http' => array(
            'method' => 'POST',
            'header' => 'Content-Type: application/json',
            'content' => json_encode(array('command' => 'AES67 Apply')),
            'timeout' => 10
        )
    )));

    if ($result === false) {
        return json(array(
            "status" => "ERROR",
            "message" => "Failed to signal fppd — is it running?"
        ));
    }

    return json(array(
        "status" => "OK",
        "message" => "AES67 configuration applied via GStreamer"
    ));
}

// GET /api/pipewire/aes67/status
function GetAES67Status()
{
    // Query AES67Manager in fppd for pipeline and PTP status
    $result = @file_get_contents('http://localhost:32322/aes67/status');

    if ($result !== false) {
        $data = json_decode($result, true);
        if ($data !== null) {
            return json($data);
        }
    }

    // Fallback: fppd not running or endpoint not available
    return json(array(
        "pipelines" => array(),
        "ptpSynced" => false,
        "ptpOffsetNs" => 0,
        "discoveredStreams" => array()
    ));
}

// GET /api/pipewire/aes67/interfaces
function GetAES67NetworkInterfaces()
{
    $interfaces = array();
    exec("ip -o link show | awk -F': ' '{print \$2}' | grep -v lo", $output);
    if (!empty($output)) {
        foreach ($output as $iface) {
            $iface = trim($iface);
            if (!empty($iface))
                $interfaces[] = $iface;
        }
    }
    return json($interfaces);
}


// AES67 audio-over-IP is managed by AES67Manager in fppd (GStreamer-based).
// Config JSON: $mediaDirectory/config/pipewire-aes67-instances.json
// Apply: POST /api/command {"command":"AES67 Apply"} → fppd rebuilds GStreamer pipelines
// Status: GET /api/pipewire/aes67/status → queries AES67Manager in fppd
// PTP: GstPtpClock (replaces external ptp4l daemon)
// SAP: Built-in C++ SAP announcer (replaces fpp_aes67_sap Python daemon)

/////////////////////////////////////////////////////////////////////////////
//  OPUS RTP MULTI-INSTANCE API
/////////////////////////////////////////////////////////////////////////////

// GET /api/pipewire/opusrtp/instances
function GetOpusRTPInstances()
{
    global $settings;
    $configFile = $settings['mediaDirectory'] . "/config/pipewire-opus-rtp-instances.json";
    if (file_exists($configFile)) {
        $data = json_decode(file_get_contents($configFile), true);
        if ($data !== null) {
            return json($data);
        }
    }
    return json(array("instances" => array()));
}

// POST /api/pipewire/opusrtp/instances
function SaveOpusRTPInstances()
{
    global $settings;
    $configFile = $settings['mediaDirectory'] . "/config/pipewire-opus-rtp-instances.json";

    $data = file_get_contents('php://input');
    $parsed = json_decode($data, true);
    if ($parsed === null) {
        http_response_code(400);
        return json(array("status" => "ERROR", "message" => "Invalid JSON"));
    }
    if (!isset($parsed['instances']) || !is_array($parsed['instances'])) {
        http_response_code(400);
        return json(array("status" => "ERROR", "message" => "Missing instances array"));
    }
    $nextId = 1;
    foreach ($parsed['instances'] as &$inst) {
        if (!isset($inst['id'])) {
            $inst['id'] = $nextId;
        }
        if ($inst['id'] >= $nextId)
            $nextId = $inst['id'] + 1;
        if (empty($inst['name']))
            $inst['name'] = 'Opus RTP Instance ' . $inst['id'];
        if (empty($inst['mode']))
            $inst['mode'] = 'send';
        if (empty($inst['destIP']))
            $inst['destIP'] = '239.69.1.' . $inst['id'];
        if (empty($inst['port']))
            $inst['port'] = 5005;
        if (empty($inst['channels']))
            $inst['channels'] = 2;
        if (!isset($inst['bitrate']))
            $inst['bitrate'] = 128000;
        if (!isset($inst['latency']))
            $inst['latency'] = 50;
        if (!isset($inst['fec']))
            $inst['fec'] = true;
        if (!isset($inst['dtx']))
            $inst['dtx'] = false;
        if (!isset($inst['packetLoss']))
            $inst['packetLoss'] = 5;
        if (!isset($inst['enabled']))
            $inst['enabled'] = true;
    }
    unset($inst);

    file_put_contents($configFile, json_encode($parsed, JSON_PRETTY_PRINT));
    return json(array("status" => "OK"));
}

// POST /api/pipewire/opusrtp/apply
function ApplyOpusRTPInstances()
{
    global $settings;
    $configFile = $settings['mediaDirectory'] . "/config/pipewire-opus-rtp-instances.json";

    if (!file_exists($configFile)) {
        $result = @file_get_contents('http://localhost/api/command', false, stream_context_create(array(
            'http' => array(
                'method' => 'POST',
                'header' => 'Content-Type: application/json',
                'content' => json_encode(array('command' => 'Opus RTP Cleanup')),
                'timeout' => 5
            )
        )));
        return json(array("status" => "OK", "message" => "No Opus RTP instances configured"));
    }

    $result = @file_get_contents('http://localhost/api/command', false, stream_context_create(array(
        'http' => array(
            'method' => 'POST',
            'header' => 'Content-Type: application/json',
            'content' => json_encode(array('command' => 'Opus RTP Apply')),
            'timeout' => 10
        )
    )));

    if ($result === false) {
        return json(array(
            "status" => "ERROR",
            "message" => "Failed to signal fppd — is it running?"
        ));
    }

    return json(array(
        "status" => "OK",
        "message" => "Opus RTP configuration applied via GStreamer"
    ));
}

// GET /api/pipewire/opusrtp/status
function GetOpusRTPStatus()
{
    $result = @file_get_contents('http://localhost:32322/opusrtp/status');

    if ($result !== false) {
        $data = json_decode($result, true);
        if ($data !== null) {
            return json($data);
        }
    }

    return json(array(
        "pipelines" => array(),
        "active" => false
    ));
}

// GET /api/pipewire/opusrtp/interfaces
function GetOpusRTPNetworkInterfaces()
{
    $interfaces = array();
    exec("ip -o link show | awk -F': ' '{print \$2}' | grep -v lo", $output);
    if (!empty($output)) {
        foreach ($output as $iface) {
            $iface = trim($iface);
            if (!empty($iface))
                $interfaces[] = $iface;
        }
    }
    return json($interfaces);
}

// Opus RTP audio streaming is managed by OpusRTPManager in fppd (GStreamer-based).
// Config JSON: $mediaDirectory/config/pipewire-opus-rtp-instances.json
// Apply: POST /api/command {"command":"Opus RTP Apply"} → fppd rebuilds GStreamer pipelines
// Status: GET /api/pipewire/opusrtp/status → queries OpusRTPManager in fppd

/////////////////////////////////////////////////////////////////////////////
// GET /api/pipewire/graph
// Returns the live PipeWire graph as { nodes, ports, links } for the
// pipeline visualizer page.  Only audio-related nodes are included by
// default; pass ?all=1 to include everything.
function GetPipeWireGraph()
{
    global $SUDO;

    $env = "PIPEWIRE_RUNTIME_DIR=/run/pipewire-fpp XDG_RUNTIME_DIR=/run/pipewire-fpp";
    $raw = shell_exec($SUDO . " " . $env . " pw-dump 2>/dev/null");
    if (empty($raw)) {
        return json(array('nodes' => array(), 'ports' => array(), 'links' => array()));
    }

    $objects = json_decode($raw, true);
    if (!is_array($objects)) {
        return json(array('nodes' => array(), 'ports' => array(), 'links' => array()));
    }

    $showAll = isset($_GET['all']) && $_GET['all'] == '1';

    // Media classes to include in the graph
    $audioClasses = array(
        'Audio/Sink',
        'Audio/Source',
        'Audio/Duplex',
        'Stream/Output/Audio',
        'Stream/Input/Audio',
        'Video/Source',
        'Video/Sink',
        'Stream/Output/Video',
        'Stream/Input/Video',
    );

    // First pass — collect nodes, ports, links
    $nodes = array();
    $ports = array();
    $links = array();
    $audioNodeIds = array();   // set of node IDs that are audio-related

    foreach ($objects as $obj) {
        $type = isset($obj['type']) ? $obj['type'] : '';
        $info = isset($obj['info']) ? $obj['info'] : array();
        $props = isset($info['props']) ? $info['props'] : array();

        if ($type === 'PipeWire:Interface:Node') {
            $mc = isset($props['media.class']) ? $props['media.class'] : '';
            $name = isset($props['node.name']) ? $props['node.name'] : '';
            $desc = isset($props['node.description']) ? $props['node.description'] : $name;
            $nick = isset($props['node.nick']) ? $props['node.nick'] : '';
            $state = isset($info['state']) ? $info['state'] : '';
            $factoryName = isset($props['factory.name']) ? $props['factory.name'] : '';

            // Fix unhelpful descriptions: GStreamer pipewiresrc/pipewiresink nodes
            // inherit the process name ("fppd") as their description.
            // Use node.name to build a better label for known FPP stream nodes.
            if ($desc === 'fppd' || $desc === $name) {
                // AES67 send/receive: aes67_<name>_send → "AES67: <name> (send)"
                if (preg_match('/^aes67_(.+)_(send|recv)$/', $name, $m)) {
                    $aesLabel = str_replace('_', ' ', $m[1]);
                    $aesLabel = ucwords($aesLabel);
                    $desc = "AES67: $aesLabel (" . $m[2] . ")";
                }
                // fppd stream: fppd_stream_N → "FPP Media Stream N"
                elseif (preg_match('/^fppd_stream_(\d+)$/', $name, $m)) {
                    $desc = "FPP Media Stream " . $m[1];
                }
                // fppd video stream: fppd_video_stream_N → "FPP Video Stream N"
                elseif (preg_match('/^fppd_video_stream_(\d+)$/', $name, $m)) {
                    $desc = "FPP Video Stream " . $m[1];
                }
                // Video output group consumer: fpp_video_group_*_mN_type
                elseif (preg_match('/^fpp_video_group_\d+_(.+?)_m(\d+)_(.+)$/', $name, $m)) {
                    $groupLabel = str_replace('_', ' ', ucwords($m[1], '_'));
                    $typeLabel = strtoupper($m[3]);
                    $desc = "Video Out: $groupLabel #$m[2] ($typeLabel)";
                }
            }

            // For ALSA sink/source nodes, prefer node.nick over the generic
            // PipeWire-derived node.description (e.g. "Built-in Audio Stereo").
            // The nick comes from the ALSA driver and identifies the actual hardware.
            // Skip FPP-managed nodes (*.fpp_*) which already have good descriptions.
            if (!empty($nick) && strpos($name, 'alsa_') === 0 && strpos($name, '.fpp_') === false) {
                $profileDesc = isset($props['device.profile.description']) ? $props['device.profile.description'] : '';
                if (!empty($profileDesc)) {
                    $desc = $nick . ' (' . $profileDesc . ')';
                } else {
                    $desc = $nick;
                }
            }

            // Skip non-audio nodes unless ?all=1
            if (!$showAll) {
                if (empty($mc) || !in_array($mc, $audioClasses)) {
                    // Also keep Midi-Bridge? No — skip it.
                    continue;
                }
            }

            $audioNodeIds[$obj['id']] = true;

            // Stash the raw ALSA device path for de-duplication (not sent to client)
            $alsaPath = isset($props['api.alsa.path']) ? $props['api.alsa.path'] : '';

            $node = array(
                'id' => $obj['id'],
                'name' => $name,
                'description' => $desc,
                'nick' => $nick,
                'mediaClass' => $mc,
                'state' => $state,
                'factory' => $factoryName,
                '_alsaPath' => $alsaPath,
                'properties' => array(),
            );

            // Pick interesting properties for the detail panel
            $interesting = array(
                'audio.channels',
                'audio.format',
                'audio.rate',
                'api.alsa.card',
                'api.alsa.card.name',
                'api.alsa.pcm.card',
                'api.alsa.headroom',
                'api.alsa.period-size',
                'api.alsa.period-num',
                'node.latency',
                'node.group',
                'node.sync-group',
                'media.name',
                'media.type',
                'stream.is-live',
                'node.always-process',
                'application.name',
                'application.process.binary',
                'object.path',
                'video.format',
                'format.dsp',
                'fpp.video.stream',
                'fpp.video.slot',
                'fpp.video.connector',
            );
            foreach ($interesting as $key) {
                if (isset($props[$key])) {
                    $node['properties'][$key] = $props[$key];
                }
            }

            $nodes[] = $node;
        } elseif ($type === 'PipeWire:Interface:Port') {
            $ports[] = array(
                'id' => $obj['id'],
                'nodeId' => isset($props['node.id']) ? (int) $props['node.id'] : 0,
                'name' => isset($props['port.name']) ? $props['port.name'] : '',
                'direction' => isset($info['direction']) ? $info['direction'] : '',
                'channel' => isset($props['audio.channel']) ? $props['audio.channel'] :
                    (isset($props['port.name']) ? $props['port.name'] : ''),
            );
        } elseif ($type === 'PipeWire:Interface:Link') {
            $links[] = array(
                'id' => $obj['id'],
                'outputNodeId' => isset($info['output-node-id']) ? (int) $info['output-node-id'] : 0,
                'outputPortId' => isset($info['output-port-id']) ? (int) $info['output-port-id'] : 0,
                'inputNodeId' => isset($info['input-node-id']) ? (int) $info['input-node-id'] : 0,
                'inputPortId' => isset($info['input-port-id']) ? (int) $info['input-port-id'] : 0,
                'state' => isset($info['state']) ? $info['state'] : '',
            );
        }
    }

    // Filter ports & links to only include those belonging to audio nodes
    if (!$showAll) {
        $ports = array_values(array_filter($ports, function ($p) use ($audioNodeIds) {
            return isset($audioNodeIds[$p['nodeId']]);
        }));
        $links = array_values(array_filter($links, function ($l) use ($audioNodeIds) {
            return isset($audioNodeIds[$l['outputNodeId']]) || isset($audioNodeIds[$l['inputNodeId']]);
        }));
    }

    // ── Remove WirePlumber auto-created ALSA nodes that duplicate FPP-managed hardware ──
    // FPP creates its own ALSA adapter nodes (alsa_output.fpp_card*, fpp_alsa_*) with
    // explicit configuration.  WirePlumber also auto-creates nodes for every ALSA card
    // it discovers, producing duplicates that clutter the graph.  Remove the WirePlumber
    // nodes when FPP already manages the same ALSA device path.
    if (!$showAll) {
        // Collect ALSA device paths claimed by FPP-managed nodes
        $fppAlsaPaths = array();
        foreach ($nodes as $n) {
            $nm = $n['name'];
            if (
                !empty($n['_alsaPath']) &&
                (strpos($nm, '.fpp_') !== false || strpos($nm, 'fpp_alsa_') === 0)
            ) {
                // Normalise: "hw:ICUSBAUDIO7D" → "ICUSBAUDIO7D"
                $devId = preg_replace('/^hw:/', '', $n['_alsaPath']);
                $fppAlsaPaths[$devId] = true;
            }
        }
        if (!empty($fppAlsaPaths)) {
            $removedNodeIds = array();
            $nodes = array_values(array_filter($nodes, function ($n) use ($fppAlsaPaths, &$removedNodeIds) {
                $nm = $n['name'];
                // Only consider WirePlumber-created ALSA nodes (not FPP-managed)
                if (
                    empty($n['_alsaPath']) ||
                    strpos($nm, '.fpp_') !== false ||
                    strpos($nm, 'fpp_alsa_') === 0
                ) {
                    return true;
                }
                // Extract card identifier: strip prefix (hw:, front:, surroundNN:)
                // then remove trailing device number (,N) to get just the card id.
                // e.g. "hw:1,0" → "1", "hw:S3,0" → "S3", "front:0,0" → "0"
                $devId = preg_replace('/^(hw|front|surround[0-9]*):/', '', $n['_alsaPath']);
                $devId = preg_replace('/,\d+$/', '', $devId);
                // Also try extracting card ID from object.path:
                // "alsa:acp:ICUSBAUDIO7D:4:playback" → "ICUSBAUDIO7D"
                $objPath = isset($n['properties']['object.path']) ? $n['properties']['object.path'] : '';
                $cardIds = array($devId);
                if (preg_match('/^alsa:acp:([^:]+):/', $objPath, $m)) {
                    $cardIds[] = $m[1];
                }
                foreach ($cardIds as $cid) {
                    if (isset($fppAlsaPaths[$cid])) {
                        $removedNodeIds[$n['id']] = true;
                        return false;
                    }
                }
                return true;
            }));
            // Remove ports and links belonging to removed nodes
            if (!empty($removedNodeIds)) {
                $ports = array_values(array_filter($ports, function ($p) use ($removedNodeIds) {
                    return !isset($removedNodeIds[$p['nodeId']]);
                }));
                $links = array_values(array_filter($links, function ($l) use ($removedNodeIds) {
                    return !isset($removedNodeIds[$l['outputNodeId']]) && !isset($removedNodeIds[$l['inputNodeId']]);
                }));
                // Also remove from audioNodeIds
                foreach ($removedNodeIds as $rid => $_) {
                    unset($audioNodeIds[$rid]);
                }
            }
        }
    }

    // Enrich delay/effect nodes with audio group config (delay, EQ, volume)
    global $settings;
    $groupsFile = $settings['mediaDirectory'] . "/config/pipewire-audio-groups.json";
    if (file_exists($groupsFile)) {
        $groupsCfg = json_decode(file_get_contents($groupsFile), true);
        if (is_array($groupsCfg) && isset($groupsCfg['groups'])) {
            // Build lookup: normalised cardId → member config
            $memberLookup = array(); // 'g{groupId}_{cardId}' → member
            $groupLookup = array();  // groupId → group
            foreach ($groupsCfg['groups'] as $group) {
                $gid = $group['id'];
                $groupLookup[$gid] = $group;
                if (isset($group['members'])) {
                    foreach ($group['members'] as $member) {
                        $cid = strtolower(preg_replace('/[^a-zA-Z0-9_]/', '_', $member['cardId']));
                        $key = 'g' . $gid . '_' . $cid;
                        $memberLookup[$key] = $member;
                    }
                }
            }
            // Match delay nodes (fpp_fx_g{N}_{cardId}) to their member config
            foreach ($nodes as &$node) {
                $nm = $node['name'];
                if (preg_match('/^fpp_fx_g(\d+)_(.+?)(_out)?$/', $nm, $m)) {
                    $key = 'g' . $m[1] . '_' . $m[2];
                    if (isset($memberLookup[$key])) {
                        $mem = $memberLookup[$key];
                        if (isset($mem['delayMs'])) {
                            $node['properties']['fpp.delay.ms'] = $mem['delayMs'];
                        }
                        if (isset($mem['eq']['enabled'])) {
                            $node['properties']['fpp.eq.enabled'] = $mem['eq']['enabled'];
                        }
                    }
                }
                // Enrich group nodes with member count
                if (preg_match('/^fpp_group_/', $nm)) {
                    // Find the group by matching the slugified name
                    foreach ($groupsCfg['groups'] as $group) {
                        $slug = 'fpp_group_' . strtolower(preg_replace('/[^a-zA-Z0-9]+/', '_', $group['name']));
                        if ($nm === $slug && isset($group['members'])) {
                            $node['properties']['fpp.group.members'] = count($group['members']);
                            if (isset($group['latencyCompensate'])) {
                                $node['properties']['fpp.group.latencyCompensate'] = $group['latencyCompensate'];
                            }
                        }
                    }
                }
            }
            unset($node);
        }
    }

    // Enrich video output group nodes with config data
    $videoGroupsFile = $settings['mediaDirectory'] . "/config/pipewire-video-groups.json";
    if (file_exists($videoGroupsFile)) {
        $vgCfg = json_decode(file_get_contents($videoGroupsFile), true);
        if (is_array($vgCfg) && isset($vgCfg['videoOutputGroups'])) {
            foreach ($nodes as &$node) {
                $nm = $node['name'];
                // Match video stream producer nodes
                if (preg_match('/^fppd_video_stream_(\d+)$/', $nm, $m)) {
                    $node['properties']['fpp.video.stream'] = true;
                    $node['properties']['fpp.video.slot'] = intval($m[1]);
                }
                // Match video output consumer nodes
                if (preg_match('/^fpp_video_group_(\d+)_/', $nm, $m)) {
                    $gid = intval($m[1]);
                    $node['properties']['fpp.video.consumer'] = true;
                    $node['properties']['fpp.video.groupId'] = $gid;
                    foreach ($vgCfg['videoOutputGroups'] as $vg) {
                        if (isset($vg['id']) && $vg['id'] == $gid) {
                            $node['properties']['fpp.video.groupName'] = isset($vg['name']) ? $vg['name'] : '';
                            break;
                        }
                    }
                }
            }
            unset($node);
        }
    }

    // Enrich input group nodes with config data + inject virtual fppd stream placeholders
    $igFile = $settings['mediaDirectory'] . "/config/pipewire-input-groups.json";
    $igCfg = null;
    if (file_exists($igFile)) {
        $igCfg = json_decode(file_get_contents($igFile), true);
        if (is_array($igCfg) && isset($igCfg['inputGroups'])) {
            foreach ($nodes as &$node) {
                $nm = $node['name'];
                // Match input group combine-stream nodes (fpp_input_*)
                if (preg_match('/^fpp_input_/', $nm)) {
                    foreach ($igCfg['inputGroups'] as $ig) {
                        $slug = 'fpp_input_' . strtolower(preg_replace('/[^a-zA-Z0-9]+/', '_', $ig['name']));
                        if ($nm === $slug) {
                            $node['properties']['fpp.inputGroup'] = true;
                            $node['properties']['fpp.inputGroup.id'] = isset($ig['id']) ? intval($ig['id']) : 0;
                            $node['properties']['fpp.inputGroup.members'] = isset($ig['members']) ? count($ig['members']) : 0;
                            $node['properties']['fpp.inputGroup.outputs'] = isset($ig['outputs']) ? count($ig['outputs']) : 0;
                            break;
                        }
                    }
                }
                // Match loopback sub-nodes: input.fpp_loopback_ig* / output.fpp_loopback_ig*
                // PipeWire loopback creates only these two sub-nodes (no bare parent)
                if (preg_match('/(?:^|^(?:input|output)\.)fpp_loopback_ig(\d+)_/', $nm, $m)) {
                    $node['properties']['fpp.inputGroup.loopback'] = true;
                    $node['properties']['fpp.inputGroup.id'] = intval($m[1]);
                }
                // Match routing hub nodes (post-effects fan-out)
                if (preg_match('/^fpp_route_ig_(\d+)$/', $nm, $m)) {
                    $node['properties']['fpp.routingHub'] = true;
                    $node['properties']['fpp.inputGroup.id'] = intval($m[1]);
                }
                // Match input group EQ filter-chain nodes
                if (preg_match('/^fpp_fx_ig_(\d+)(_out)?$/', $nm, $m)) {
                    $node['properties']['fpp.inputGroup.eq'] = true;
                    $node['properties']['fpp.inputGroup.id'] = intval($m[1]);
                }
                // Match tee (null-sink fan-out) nodes for fppd streams
                if (preg_match('/^fpp_tee_fppd_stream_(\d+)$/', $nm, $m)) {
                    $node['properties']['fpp.tee'] = true;
                    $node['properties']['fpp.tee.slot'] = intval($m[1]);
                }
                // (Routing loopback nodes removed — combine-stream handles output routing)
            }
            unset($node);
        }
    }

    // ── Inject virtual fppd media stream placeholder nodes ──
    // Always show all 5 fppd stream slots so the graph reveals configured
    // routing even when nothing is playing.  Live nodes replace their
    // virtual counterparts; inactive slots appear with state "not-running".
    $FPPD_STREAM_COUNT = 5;
    // Build a set of live fppd_stream_N node names
    $liveFppdStreams = array();
    foreach ($nodes as $node) {
        if (preg_match('/^fppd_stream_(\d+)$/', $node['name'])) {
            $liveFppdStreams[$node['name']] = true;
        }
    }
    // Build lookup: which input groups each fppd stream slot targets
    // A single fppd_stream can be a member of multiple input groups.
    // When fan-out (tee) is active, the stream targets the tee node.
    $fppdStreamTargets = array(); // streamName => [targetSlug, ...]
    if ($igCfg && isset($igCfg['inputGroups'])) {
        $fppdStreamGroupCount = array(); // sourceId => count
        foreach ($igCfg['inputGroups'] as $ig) {
            if (!isset($ig['enabled']) || !$ig['enabled'])
                continue;
            if (!isset($ig['members']))
                continue;
            foreach ($ig['members'] as $mbr) {
                if (isset($mbr['type']) && $mbr['type'] === 'fppd_stream') {
                    $sid = isset($mbr['sourceId']) ? $mbr['sourceId'] : 'fppd_stream_1';
                    if (!isset($fppdStreamGroupCount[$sid]))
                        $fppdStreamGroupCount[$sid] = 0;
                    $fppdStreamGroupCount[$sid]++;
                }
            }
        }
        foreach ($igCfg['inputGroups'] as $ig) {
            if (!isset($ig['enabled']) || !$ig['enabled'])
                continue;
            if (!isset($ig['members']))
                continue;
            $igSlug = 'fpp_input_' . strtolower(preg_replace('/[^a-zA-Z0-9]+/', '_', $ig['name']));
            foreach ($ig['members'] as $mbr) {
                if (isset($mbr['type']) && $mbr['type'] === 'fppd_stream') {
                    $sid = isset($mbr['sourceId']) ? $mbr['sourceId'] : 'fppd_stream_1';
                    // When fan-out (tee), stream targets the tee node
                    $needsTee = isset($fppdStreamGroupCount[$sid]) && $fppdStreamGroupCount[$sid] > 1;
                    if ($needsTee) {
                        $teeSlug = 'fpp_tee_' . strtolower(preg_replace('/[^a-zA-Z0-9]+/', '_', $sid));
                        if (!isset($fppdStreamTargets[$sid])) {
                            $fppdStreamTargets[$sid] = array($teeSlug);
                        }
                    } else {
                        if (!isset($fppdStreamTargets[$sid])) {
                            $fppdStreamTargets[$sid] = array();
                        }
                        if (!in_array($igSlug, $fppdStreamTargets[$sid])) {
                            $fppdStreamTargets[$sid][] = $igSlug;
                        }
                    }
                }
            }
        }
    }
    // Determine the PipeWireSinkName (output group target when no input groups)
    $defaultTarget = '';
    if (empty($fppdStreamTargets)) {
        $defaultTarget = ReadSettingFromFile('PipeWireSinkName');
    }

    // Virtual node IDs start above any real PipeWire ID
    $maxId = 0;
    foreach ($nodes as $n) {
        if ($n['id'] > $maxId)
            $maxId = $n['id'];
    }
    foreach ($ports as $p) {
        if ($p['id'] > $maxId)
            $maxId = $p['id'];
    }
    foreach ($links as $l) {
        if ($l['id'] > $maxId)
            $maxId = $l['id'];
    }
    $virtualId = $maxId + 10000;

    for ($i = 1; $i <= $FPPD_STREAM_COUNT; $i++) {
        $streamName = "fppd_stream_$i";
        if (isset($liveFppdStreams[$streamName])) {
            // Live node exists — enrich it with routing info
            foreach ($nodes as &$node) {
                if ($node['name'] === $streamName) {
                    $node['properties']['fpp.stream.slot'] = $i;
                    if (isset($fppdStreamTargets[$streamName])) {
                        $targets = $fppdStreamTargets[$streamName];
                        $node['properties']['fpp.stream.target'] = implode(', ', $targets);
                        $node['properties']['fpp.stream.targets'] = $targets;
                    }
                    break;
                }
            }
            unset($node);
            continue;
        }

        // Determine targets for virtual links
        $targets = array();
        if (isset($fppdStreamTargets[$streamName])) {
            $targets = $fppdStreamTargets[$streamName];
        } elseif ($i === 1 && !empty($defaultTarget)) {
            $targets = array($defaultTarget);
        }

        // Create virtual node
        $vNodeId = $virtualId++;
        $nodes[] = array(
            'id' => $vNodeId,
            'name' => $streamName,
            'description' => "FPP Media Stream $i",
            'nick' => '',
            'mediaClass' => 'Stream/Output/Audio',
            'state' => 'not-running',
            'factory' => 'virtual',
            'properties' => array(
                'fpp.stream.slot' => $i,
                'fpp.stream.virtual' => true,
                'fpp.stream.target' => implode(', ', $targets),
                'fpp.stream.targets' => $targets,
                'audio.channels' => 2,
            ),
        );
        // Create virtual FL/FR output ports
        $portFL = $virtualId++;
        $portFR = $virtualId++;
        $ports[] = array('id' => $portFL, 'nodeId' => $vNodeId, 'name' => 'output_FL', 'direction' => 'output', 'channel' => 'FL');
        $ports[] = array('id' => $portFR, 'nodeId' => $vNodeId, 'name' => 'output_FR', 'direction' => 'output', 'channel' => 'FR');

        // Create virtual links to ALL target nodes
        foreach ($targets as $target) {
            // Find the target node's input ports
            $targetNodeId = null;
            foreach ($nodes as $tn) {
                if ($tn['name'] === $target) {
                    $targetNodeId = $tn['id'];
                    break;
                }
            }
            if ($targetNodeId !== null) {
                // Find FL/FR input ports on target
                $targetPortFL = null;
                $targetPortFR = null;
                foreach ($ports as $p) {
                    if ($p['nodeId'] === $targetNodeId && $p['direction'] === 'input') {
                        $ch = preg_replace('/^(playback|capture|input|output)_/', '', $p['name']);
                        if ($ch === 'FL' && !$targetPortFL)
                            $targetPortFL = $p['id'];
                        if ($ch === 'FR' && !$targetPortFR)
                            $targetPortFR = $p['id'];
                    }
                }
                if ($targetPortFL) {
                    $links[] = array(
                        'id' => $virtualId++,
                        'outputNodeId' => $vNodeId,
                        'outputPortId' => $portFL,
                        'inputNodeId' => $targetNodeId,
                        'inputPortId' => $targetPortFL,
                        'state' => 'not-running',
                    );
                }
                if ($targetPortFR) {
                    $links[] = array(
                        'id' => $virtualId++,
                        'outputNodeId' => $vNodeId,
                        'outputPortId' => $portFR,
                        'inputNodeId' => $targetNodeId,
                        'inputPortId' => $targetPortFR,
                        'state' => 'not-running',
                    );
                }
            }
        }
    }

    // ── Inject synthetic video source and consumer nodes ──────────────
    // Video streams use GStreamer's intervideosink/intervideosrc internally
    // rather than PipeWire, so no video nodes appear in the PipeWire graph.
    // Inject virtual source and consumer nodes from the video config files
    // so the graph visualizer shows the complete video routing path.
    $videoSourcesFile = $settings['mediaDirectory'] . "/config/pipewire-video-input-sources.json";
    $videoConsumersFile = $settings['mediaDirectory'] . "/config/pipewire-video-consumers.json";
    if (file_exists($videoSourcesFile)) {
        $vsCfg = json_decode(file_get_contents($videoSourcesFile), true);
        $vcCfg = array();
        if (file_exists($videoConsumersFile)) {
            $vcCfg = json_decode(file_get_contents($videoConsumersFile), true);
            if (!is_array($vcCfg))
                $vcCfg = array();
        }

        // Build lookup of existing PipeWire node names → state for activity detection
        $liveNodeStates = array();
        foreach ($nodes as $n) {
            $liveNodeStates[$n['name']] = $n['state'];
        }

        if (is_array($vsCfg) && isset($vsCfg['videoInputSources'])) {
            foreach ($vsCfg['videoInputSources'] as $vs) {
                if (!isset($vs['enabled']) || !$vs['enabled'])
                    continue;

                $srcNodeName = isset($vs['pipeWireNodeName']) ? $vs['pipeWireNodeName'] : '';
                if (empty($srcNodeName))
                    continue;

                // Skip if this video source already exists as a PipeWire node
                if (isset($liveNodeStates[$srcNodeName]))
                    continue;

                // Detect running state from the paired audio PipeWire node
                $audioNodeName = isset($vs['audioPipeWireNodeName']) ? $vs['audioPipeWireNodeName'] : '';
                $isRunning = !empty($audioNodeName) && isset($liveNodeStates[$audioNodeName])
                    && in_array($liveNodeStates[$audioNodeName], array('running', 'idle'));

                // Build description
                $srcName = isset($vs['name']) ? $vs['name'] : 'Video Source';
                $srcType = isset($vs['type']) ? $vs['type'] : '';
                $w = isset($vs['width']) ? intval($vs['width']) : 0;
                $h = isset($vs['height']) ? intval($vs['height']) : 0;
                $fps = isset($vs['framerate']) ? intval($vs['framerate']) : 0;
                $typeLabel = '';
                if ($srcType === 'urisrc')
                    $typeLabel = 'YouTube';
                elseif ($srcType === 'videotestsrc')
                    $typeLabel = 'Test Pattern';
                elseif ($srcType === 'v4l2src')
                    $typeLabel = 'USB Camera';
                $desc = $srcName;
                if (!empty($typeLabel))
                    $desc .= ' (' . $typeLabel . ')';

                // Create synthetic video source node
                $srcNodeId = $virtualId++;
                $srcPortId = $virtualId++;
                $nodes[] = array(
                    'id' => $srcNodeId,
                    'name' => $srcNodeName,
                    'description' => $desc,
                    'nick' => '',
                    'mediaClass' => 'Video/Source',
                    'state' => $isRunning ? 'running' : 'not-running',
                    'factory' => 'virtual',
                    'properties' => array(
                        'fpp.video.stream' => true,
                        'fpp.video.slot' => isset($vs['id']) ? intval($vs['id']) : 0,
                        'fpp.video.virtual' => true,
                        'video.format' => ($w > 0 ? $w . 'x' . $h . '@' . $fps . 'fps' : ''),
                    ),
                );
                $ports[] = array(
                    'id' => $srcPortId,
                    'nodeId' => $srcNodeId,
                    'name' => 'output_video',
                    'direction' => 'output',
                    'channel' => 'video',
                );
                $audioNodeIds[$srcNodeId] = true;

                // Find consumers that target this source and inject HDMI sink nodes
                foreach ($vcCfg as $vc) {
                    $consumerSrc = isset($vc['sourceNode']) ? $vc['sourceNode'] : '';
                    if ($consumerSrc !== $srcNodeName)
                        continue;

                    $consumerNodeName = isset($vc['pipeWireNodeName']) ? $vc['pipeWireNodeName'] : '';
                    if (empty($consumerNodeName))
                        continue;
                    if (isset($liveNodeStates[$consumerNodeName]))
                        continue;

                    $connector = isset($vc['connector']) ? $vc['connector'] : '';
                    $cw = isset($vc['width']) ? intval($vc['width']) : 0;
                    $ch2 = isset($vc['height']) ? intval($vc['height']) : 0;
                    $consumerDesc = !empty($connector) ? $connector : 'Video Output';
                    if ($cw > 0 && $ch2 > 0) {
                        $consumerDesc .= ' (' . $cw . 'x' . $ch2 . ')';
                    }

                    $consumerNodeId = $virtualId++;
                    $consumerPortId = $virtualId++;
                    $nodes[] = array(
                        'id' => $consumerNodeId,
                        'name' => $consumerNodeName,
                        'description' => $consumerDesc,
                        'nick' => '',
                        'mediaClass' => 'Video/Sink',
                        'state' => $isRunning ? 'running' : 'not-running',
                        'factory' => 'virtual',
                        'properties' => array(
                            'fpp.video.consumer' => true,
                            'fpp.hdmi.direct' => (isset($vc['type']) && $vc['type'] === 'hdmi'),
                            'fpp.hdmi.connector' => $connector,
                            'fpp.video.groupId' => isset($vc['groupId']) ? intval($vc['groupId']) : 0,
                            'fpp.video.groupName' => isset($vc['groupName']) ? $vc['groupName'] : '',
                        ),
                    );
                    $ports[] = array(
                        'id' => $consumerPortId,
                        'nodeId' => $consumerNodeId,
                        'name' => 'input_video',
                        'direction' => 'input',
                        'channel' => 'video',
                    );
                    $audioNodeIds[$consumerNodeId] = true;

                    // Link source → consumer
                    $links[] = array(
                        'id' => $virtualId++,
                        'outputNodeId' => $srcNodeId,
                        'outputPortId' => $srcPortId,
                        'inputNodeId' => $consumerNodeId,
                        'inputPortId' => $consumerPortId,
                        'state' => $isRunning ? 'active' : 'not-running',
                    );
                }
            }
        }

        // Also inject consumers without a sourceNode (on-demand / stream-slot based)
        // so they appear in the graph even when idle.
        // Build lookup: fppd_video_stream_N node/port IDs for linking
        $videoStreamNodes = array();  // slot => array('nodeId'=>..., 'portId'=>..., 'state'=>...)
        foreach ($nodes as $n) {
            if (preg_match('/^fppd_video_stream_(\d+)$/', $n['name'], $m)) {
                $slot = intval($m[1]);
                // Find output port for this node
                $outPort = null;
                foreach ($ports as $p) {
                    if ($p['nodeId'] === $n['id'] && $p['direction'] === 'output') {
                        $outPort = $p['id'];
                        break;
                    }
                }
                // Video streams use GStreamer intervideo (not PipeWire links) so the
                // PipeWire node may stay "suspended" even while video is playing.
                // Check the paired audio node fppd_stream_N for a reliable running state.
                $audioState = isset($liveNodeStates['fppd_stream_' . $slot])
                    ? $liveNodeStates['fppd_stream_' . $slot] : '';
                $effectiveState = $n['state'];
                if (in_array($audioState, array('running', 'idle')) && !in_array($effectiveState, array('running', 'idle'))) {
                    $effectiveState = $audioState;
                }
                $videoStreamNodes[$slot] = array(
                    'nodeId' => $n['id'],
                    'portId' => $outPort,
                    'state' => $effectiveState,
                );
            }
        }

        foreach ($vcCfg as $vc) {
            $consumerSrc = isset($vc['sourceNode']) ? $vc['sourceNode'] : '';
            if (!empty($consumerSrc))
                continue; // already handled above

            $consumerNodeName = isset($vc['pipeWireNodeName']) ? $vc['pipeWireNodeName'] : '';
            if (empty($consumerNodeName))
                continue;
            if (isset($liveNodeStates[$consumerNodeName]))
                continue;

            $connector = isset($vc['connector']) ? $vc['connector'] : '';
            $cw = isset($vc['width']) ? intval($vc['width']) : 0;
            $ch2 = isset($vc['height']) ? intval($vc['height']) : 0;
            $consumerDesc = !empty($connector) ? $connector : 'Video Output';
            if ($cw > 0 && $ch2 > 0) {
                $consumerDesc .= ' (' . $cw . 'x' . $ch2 . ')';
            }

            // Determine if any linked stream slot is active
            $slots = isset($vc['streamSlots']) ? $vc['streamSlots'] : array();
            $anySlotActive = false;
            foreach ($slots as $s) {
                if (
                    isset($videoStreamNodes[$s]) &&
                    in_array($videoStreamNodes[$s]['state'], array('running', 'idle'))
                ) {
                    $anySlotActive = true;
                    break;
                }
            }

            $consumerNodeId = $virtualId++;
            $consumerPortId = $virtualId++;
            $nodes[] = array(
                'id' => $consumerNodeId,
                'name' => $consumerNodeName,
                'description' => $consumerDesc,
                'nick' => '',
                'mediaClass' => 'Video/Sink',
                'state' => $anySlotActive ? 'running' : 'not-running',
                'factory' => 'virtual',
                'properties' => array(
                    'fpp.video.consumer' => true,
                    'fpp.hdmi.direct' => (isset($vc['type']) && $vc['type'] === 'hdmi'),
                    'fpp.hdmi.connector' => $connector,
                    'fpp.video.groupId' => isset($vc['groupId']) ? intval($vc['groupId']) : 0,
                    'fpp.video.groupName' => isset($vc['groupName']) ? $vc['groupName'] : '',
                ),
            );
            $ports[] = array(
                'id' => $consumerPortId,
                'nodeId' => $consumerNodeId,
                'name' => 'input_video',
                'direction' => 'input',
                'channel' => 'video',
            );
            $audioNodeIds[$consumerNodeId] = true;

            // Link fppd_video_stream_N → consumer for each stream slot
            // If no streamSlots specified, link all active video streams
            $linkSlots = $slots;
            if (empty($linkSlots)) {
                $linkSlots = array_keys($videoStreamNodes);
            }
            foreach ($linkSlots as $s) {
                if (!isset($videoStreamNodes[$s]))
                    continue;
                $vsn = $videoStreamNodes[$s];
                if ($vsn['portId'] === null)
                    continue;
                $isActive = in_array($vsn['state'], array('running', 'idle'));
                $links[] = array(
                    'id' => $virtualId++,
                    'outputNodeId' => $vsn['nodeId'],
                    'outputPortId' => $vsn['portId'],
                    'inputNodeId' => $consumerNodeId,
                    'inputPortId' => $consumerPortId,
                    'state' => $isActive ? 'active' : 'not-running',
                );
            }
        }
    }

    // Strip internal fields before returning
    $outNodes = array_values($nodes);
    foreach ($outNodes as &$n) {
        unset($n['_alsaPath']);
    }
    unset($n);

    return json(array(
        'nodes' => $outNodes,
        'ports' => $ports,
        'links' => $links,
    ));
}

/////////////////////////////////////////////////////////////////////////////
// ═══════════════════════════════════════════════════════════════════════════
// VIDEO OUTPUT GROUPS — Route video through PipeWire graph
// ═══════════════════════════════════════════════════════════════════════════
//
// Config file: $mediaDirectory/config/pipewire-video-outputs.json
//
// Each video output defines a consumer that receives fppd's video stream
// via PipeWire: HDMI displays, pixel overlays, or network streams.
//
// Unlike audio groups (which use combine-stream to mix multiple sinks),
// video outputs are 1:1 consumer pipelines.  fppd's GStreamer pipeline tees
// video to a pipewiresink (Stream/Output/Video), and each video output runs
// a pipewiresrc consumer pipeline to drive its destination.
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// Helper: Enumerate available DRM/KMS video connectors via sysfs.
// Returns array of { connector, card, connectorId, connected, width, height }
function GetVideoConnectors()
{
    $connectors = array();
    $drmDir = '/sys/class/drm';
    if (!is_dir($drmDir))
        return $connectors;

    $entries = scandir($drmDir);
    foreach ($entries as $entry) {
        // Match cardN-ConnectorName (e.g., card1-HDMI-A-1)
        if (!preg_match('/^card(\d+)-(.+)$/', $entry, $m))
            continue;
        $cardNum = $m[1];
        $connName = $m[2];

        // Skip non-display connectors
        if (strpos($connName, 'Writeback') !== false)
            continue;

        $sysBase = $drmDir . '/' . $entry;
        $statusPath = $sysBase . '/status';
        if (!file_exists($statusPath))
            continue;

        $status = trim(file_get_contents($statusPath));
        $connected = ($status === 'connected');

        $connectorId = 0;
        $cidPath = $sysBase . '/connector_id';
        if (file_exists($cidPath)) {
            $connectorId = intval(trim(file_get_contents($cidPath)));
        }

        $width = 0;
        $height = 0;
        $modesPath = $sysBase . '/modes';
        if (file_exists($modesPath)) {
            $modes = file($modesPath, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
            if (!empty($modes)) {
                $parts = explode('x', $modes[0]);
                if (count($parts) === 2) {
                    $width = intval($parts[0]);
                    $height = intval($parts[1]);
                }
            }
        }

        $connectors[] = array(
            'connector' => $connName,
            'card' => intval($cardNum),
            'cardPath' => '/dev/dri/card' . $cardNum,
            'connectorId' => $connectorId,
            'connected' => $connected,
            'width' => $width,
            'height' => $height,
        );
    }

    // Sort: connected first, then by connector name
    usort($connectors, function ($a, $b) {
        if ($a['connected'] !== $b['connected'])
            return $b['connected'] ? 1 : -1;
        return strcmp($a['connector'], $b['connector']);
    });

    return $connectors;
}

/////////////////////////////////////////////////////////////////////////////
// Helper: Enumerate available PixelOverlay models for video output.
function GetVideoOverlayModels()
{
    $models = array();
    $ctx = stream_context_create(array('http' => array('timeout' => 2)));
    $json = @file_get_contents('http://localhost/api/overlays/models', false, $ctx);
    if ($json === false)
        return $models;

    $data = json_decode($json, true);
    if (!is_array($data))
        return $models;

    foreach ($data as $model) {
        if (!is_array($model))
            continue;
        $name = isset($model['Name']) ? $model['Name'] : '';
        if (empty($name))
            continue;
        $models[] = array(
            'name' => $name,
            'width' => isset($model['Width']) ? intval($model['Width']) : 0,
            'height' => isset($model['Height']) ? intval($model['Height']) : 0,
        );
    }

    return $models;
}

/////////////////////////////////////////////////////////////////////////////
// GET /api/pipewire/video/connectors
// Returns available video connectors (HDMI ports) and overlay models.
function GetVideoOutputTargets()
{
    return json(array(
        'connectors' => GetVideoConnectors(),
        'overlayModels' => GetVideoOverlayModels(),
    ));
}

/////////////////////////////////////////////////////////////////////////////
// GET /api/pipewire/video/groups
function GetPipeWireVideoGroups()
{
    global $settings;
    $configFile = $settings['mediaDirectory'] . "/config/pipewire-video-groups.json";

    if (file_exists($configFile)) {
        $data = json_decode(file_get_contents($configFile), true);
        if ($data === null) {
            $data = array("videoOutputGroups" => array());
        }
    } else {
        $data = array("videoOutputGroups" => array());
    }

    return json($data);
}

/////////////////////////////////////////////////////////////////////////////
// POST /api/pipewire/video/groups
function SavePipeWireVideoGroups()
{
    global $settings;
    $configFile = $settings['mediaDirectory'] . "/config/pipewire-video-groups.json";

    $data = file_get_contents('php://input');
    $decoded = json_decode($data, true);

    if ($decoded === null) {
        http_response_code(400);
        return json(array("status" => "ERROR", "message" => "Invalid JSON"));
    }

    if (!isset($decoded['videoOutputGroups']) || !is_array($decoded['videoOutputGroups'])) {
        http_response_code(400);
        return json(array("status" => "ERROR", "message" => "Missing 'videoOutputGroups' array"));
    }

    // Assign group IDs if missing
    $maxId = 0;
    foreach ($decoded['videoOutputGroups'] as &$grp) {
        if (isset($grp['id']) && $grp['id'] > $maxId) {
            $maxId = $grp['id'];
        }
    }
    unset($grp);
    foreach ($decoded['videoOutputGroups'] as &$grp) {
        if (!isset($grp['id']) || $grp['id'] <= 0) {
            $maxId++;
            $grp['id'] = $maxId;
        }
    }
    unset($grp);

    file_put_contents($configFile, json_encode($decoded, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES));

    return json(array("status" => "OK", "message" => "Video output groups saved"));
}

/////////////////////////////////////////////////////////////////////////////
// POST /api/pipewire/video/groups/apply
//
// Generates consumer config from video output groups and sets
// PipeWireVideoSinkName settings for fppd stream slots.
//
// Video output flow:
//   fppd (pipewiresink Stream/Output/Video) → PipeWire graph → consumers
//
// Each enabled group creates one PipeWire video bus name.  Every member of
// that group becomes a consumer pipeline (pipewiresrc → sink).  All members
// of a group receive the same video signal from the producer.
//
// The primary HDMI output is handled directly by GStreamerOut's kmssink
// (not through PipeWire), so the PipeWire video stream is available for
// ADDITIONAL outputs: a second HDMI port, a PixelOverlay, a network stream.
function ApplyPipeWireVideoGroups($overrideData = null)
{
    global $settings, $SUDO;

    $configFile = $settings['mediaDirectory'] . "/config/pipewire-video-groups.json";
    $useOverride = ($overrideData !== null);

    // Helper: clear all video sink settings
    $clearSettings = function () {
        for ($s = 1; $s <= 5; $s++) {
            $key = ($s === 1) ? 'PipeWireVideoSinkName' : "PipeWireVideoSinkName_$s";
            WriteSettingToFile($key, '');
            @SendCommand("setSetting,$key,");
        }
    };

    if ($useOverride) {
        $data = $overrideData;
        if ($data === null || !isset($data['videoOutputGroups']) || empty($data['videoOutputGroups'])) {
            $clearSettings();
            return json(array("status" => "OK", "message" => "No video output groups configured"));
        }
    } else {
        if (!file_exists($configFile)) {
            $clearSettings();
            return json(array("status" => "OK", "message" => "No video output groups configured"));
        }

        $data = json_decode(file_get_contents($configFile), true);
        if ($data === null || !isset($data['videoOutputGroups']) || empty($data['videoOutputGroups'])) {
            $clearSettings();
            return json(array("status" => "OK", "message" => "Video output groups cleared"));
        }
    }

    // Resolve hardware info once
    $connectors = GetVideoConnectors();
    $connectorMap = array();
    foreach ($connectors as $c) {
        $connectorMap[$c['connector']] = $c;
    }

    // Build consumer config from enabled groups + members
    $consumerConfig = array();
    $enabledGroupCount = 0;

    foreach ($data['videoOutputGroups'] as &$grp) {
        if (!isset($grp['enabled']) || !$grp['enabled'])
            continue;
        if (!isset($grp['members']) || empty($grp['members']))
            continue;

        $enabledGroupCount++;

        // Generate stable group PipeWire node name
        $groupSlug = preg_replace('/[^a-zA-Z0-9_]/', '_', strtolower(isset($grp['name']) ? $grp['name'] : 'group'));
        $grp['pipeWireNodeName'] = "fpp_video_group_" . $grp['id'] . "_" . $groupSlug;

        $memberIdx = 0;
        foreach ($grp['members'] as $member) {
            $type = isset($member['type']) ? $member['type'] : '';
            if (empty($type))
                continue;

            $memberIdx++;
            $memberSlug = preg_replace('/[^a-zA-Z0-9_]/', '_', strtolower($type));
            $nodeName = $grp['pipeWireNodeName'] . "_m" . $memberIdx . "_" . $memberSlug;

            $entry = array(
                'groupId' => $grp['id'],
                'groupName' => isset($grp['name']) ? $grp['name'] : 'Group ' . $grp['id'],
                'groupNodeName' => $grp['pipeWireNodeName'],
                'type' => $type,
                'pipeWireNodeName' => $nodeName,
            );

            // If the group targets a persistent video input source, pass it
            // through so VideoOutputManager starts the consumer immediately.
            if (isset($grp['videoSource']) && !empty($grp['videoSource'])) {
                $entry['sourceNode'] = $grp['videoSource'];
            }

            // Pass stream slot filter so VideoOutputManager only starts
            // this consumer for matching fppd media stream slots.
            if (isset($grp['streamSlots']) && is_array($grp['streamSlots']) && !empty($grp['streamSlots'])) {
                $entry['streamSlots'] = array_values(array_map('intval', $grp['streamSlots']));
            }

            switch ($type) {
                case 'hdmi':
                    $conn = isset($member['connector']) ? $member['connector'] : '';
                    if (empty($conn) || !isset($connectorMap[$conn]))
                        continue 2;
                    $c = $connectorMap[$conn];
                    $entry['connector'] = $conn;
                    $entry['cardPath'] = $c['cardPath'];
                    $entry['connectorId'] = $c['connectorId'];
                    $entry['width'] = $c['width'];
                    $entry['height'] = $c['height'];
                    $entry['scaling'] = isset($member['scaling']) ? $member['scaling'] : 'fit';
                    $entry['name'] = $conn;
                    break;

                case 'overlay':
                    $modelName = isset($member['overlayModel']) ? $member['overlayModel'] : '';
                    if (empty($modelName))
                        continue 2;
                    $entry['overlayModel'] = $modelName;
                    $entry['name'] = 'Overlay: ' . $modelName;
                    break;

                case 'rtp':
                    $entry['address'] = isset($member['address']) ? $member['address'] : '239.0.0.1';
                    $entry['port'] = isset($member['port']) ? intval($member['port']) : 5004;
                    $entry['encoding'] = isset($member['encoding']) ? $member['encoding'] : 'h264';
                    $encLabel = strtoupper($entry['encoding']);
                    $entry['name'] = 'RTP ' . $entry['address'] . ':' . $entry['port'] . ' (' . $encLabel . ')';
                    break;

                default:
                    continue 2;
            }

            $consumerConfig[] = $entry;
        }
    }
    unset($grp);

    // Save back with generated node names
    if (!$useOverride) {
        file_put_contents($configFile, json_encode($data, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES));
    }

    // Write consumer config (read by fppd VideoOutputManager)
    $consumerFile = $settings['mediaDirectory'] . "/config/pipewire-video-consumers.json";
    file_put_contents($consumerFile, json_encode($consumerConfig, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES));

    // Set PipeWireVideoSinkName for stream slots.
    // A non-empty value tells fppd to create a pipewiresink for video on
    // that stream slot.  We enable all slots that have at least one group
    // targeting them (or all slots if no group restricts via streamSlots).
    if ($enabledGroupCount > 0) {
        // Determine which stream slots need video enabled
        $slotsNeeded = array();
        $anyGroupUnrestricted = false;
        foreach ($data['videoOutputGroups'] as $grp) {
            if (!isset($grp['enabled']) || !$grp['enabled'])
                continue;
            if (!isset($grp['members']) || empty($grp['members']))
                continue;
            if (isset($grp['streamSlots']) && is_array($grp['streamSlots']) && !empty($grp['streamSlots'])) {
                foreach ($grp['streamSlots'] as $slot) {
                    $slotsNeeded[intval($slot)] = true;
                }
            } else {
                // Group has no slot restriction — enable all slots
                $anyGroupUnrestricted = true;
            }
        }

        $videoSinkName = "fpp_video_bus";
        for ($s = 1; $s <= 5; $s++) {
            $key = ($s === 1) ? 'PipeWireVideoSinkName' : "PipeWireVideoSinkName_$s";
            if ($anyGroupUnrestricted || isset($slotsNeeded[$s])) {
                WriteSettingToFile($key, $videoSinkName);
                SetFppdSetting($key, $videoSinkName);
            } else {
                WriteSettingToFile($key, '');
                @SendCommand("setSetting,$key,");
            }
        }
    } else {
        $clearSettings();
    }

    // Install / update WirePlumber hook (already has video patterns)
    InstallWirePlumberFppLinkingHook($SUDO);

    // Signal fppd to reload video consumer config
    @SendCommand("reloadVideoOutputs");

    return json(array(
        "status" => "OK",
        "message" => $enabledGroupCount . " video output group(s) with " . count($consumerConfig) . " consumer(s) configured",
        "consumers" => $consumerConfig,
    ));
}

/////////////////////////////////////////////////////////////////////////////
// GET /api/pipewire/video/input-sources
function GetPipeWireVideoInputSources()
{
    global $settings;
    $configFile = $settings['mediaDirectory'] . "/config/pipewire-video-input-sources.json";

    if (file_exists($configFile)) {
        $data = json_decode(file_get_contents($configFile), true);
        if ($data === null) {
            $data = array("videoInputSources" => array());
        }
    } else {
        $data = array("videoInputSources" => array());
    }

    // Compute pipeWireNodeName and audioPipeWireNodeName for each source
    if (isset($data['videoInputSources'])) {
        foreach ($data['videoInputSources'] as &$src) {
            if (isset($src['id'])) {
                $nameSlug = preg_replace('/[^a-zA-Z0-9_]/', '_', strtolower(isset($src['name']) ? $src['name'] : 'source'));
                $src['pipeWireNodeName'] = "fpp_video_src_" . $src['id'] . "_" . $nameSlug;
                $src['audioPipeWireNodeName'] = "fpp_audio_src_" . $src['id'] . "_" . $nameSlug;
            }
        }
        unset($src);
    }

    return json($data);
}

/////////////////////////////////////////////////////////////////////////////
// POST /api/pipewire/video/input-sources
function SavePipeWireVideoInputSources()
{
    global $settings;
    $configFile = $settings['mediaDirectory'] . "/config/pipewire-video-input-sources.json";

    $data = file_get_contents('php://input');
    $decoded = json_decode($data, true);

    if ($decoded === null) {
        http_response_code(400);
        return json(array("status" => "ERROR", "message" => "Invalid JSON"));
    }

    if (!isset($decoded['videoInputSources']) || !is_array($decoded['videoInputSources'])) {
        http_response_code(400);
        return json(array("status" => "ERROR", "message" => "Missing 'videoInputSources' array"));
    }

    // Assign source IDs if missing
    $maxId = 0;
    foreach ($decoded['videoInputSources'] as &$src) {
        if (isset($src['id']) && $src['id'] > $maxId) {
            $maxId = $src['id'];
        }
    }
    unset($src);
    foreach ($decoded['videoInputSources'] as &$src) {
        if (!isset($src['id']) || $src['id'] <= 0) {
            $maxId++;
            $src['id'] = $maxId;
        }
    }
    unset($src);

    $jsonOut = json_encode($decoded, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES);
    $writeResult = file_put_contents($configFile, $jsonOut);
    file_put_contents('/tmp/debug_save_vis.txt', date('c') . " WRITE path=$configFile bytes=$writeResult\n", FILE_APPEND);
    file_put_contents('/tmp/debug_save_vis.txt', date('c') . " VERIFY: " . file_get_contents($configFile) . "\n", FILE_APPEND);

    return json(array("status" => "OK", "message" => "Video input sources saved"));
}

/////////////////////////////////////////////////////////////////////////////
// POST /api/pipewire/video/input-sources/apply
//
// Generates the flat source config array read by fppd VideoInputManager
// and signals fppd to reload.
function ApplyPipeWireVideoInputSources()
{
    global $settings;

    $configFile = $settings['mediaDirectory'] . "/config/pipewire-video-input-sources.json";

    if (!file_exists($configFile)) {
        // Remove generated config and signal fppd
        $genFile = $settings['mediaDirectory'] . "/config/pipewire-video-input-sources-gen.json";
        @unlink($genFile);
        @SendCommand("reloadVideoInputs");
        return json(array("status" => "OK", "message" => "No video input sources configured"));
    }

    $data = json_decode(file_get_contents($configFile), true);
    if ($data === null || !isset($data['videoInputSources']) || empty($data['videoInputSources'])) {
        $genFile = $settings['mediaDirectory'] . "/config/pipewire-video-input-sources-gen.json";
        @unlink($genFile);
        @SendCommand("reloadVideoInputs");
        return json(array("status" => "OK", "message" => "Video input sources cleared"));
    }

    // Build flat source array for fppd
    $sourceConfig = array();
    $enabledCount = 0;

    foreach ($data['videoInputSources'] as &$src) {
        $type = isset($src['type']) ? $src['type'] : '';
        if (empty($type))
            continue;

        // Generate stable PipeWire node name
        $nameSlug = preg_replace('/[^a-zA-Z0-9_]/', '_', strtolower(isset($src['name']) ? $src['name'] : 'source'));
        $src['pipeWireNodeName'] = "fpp_video_src_" . $src['id'] . "_" . $nameSlug;
        $src['audioPipeWireNodeName'] = "fpp_audio_src_" . $src['id'] . "_" . $nameSlug;

        $enabled = isset($src['enabled']) ? (bool) $src['enabled'] : true;
        $audioEnabled = isset($src['audioEnabled']) ? (bool) $src['audioEnabled'] : false;

        $entry = array(
            'id' => $src['id'],
            'name' => isset($src['name']) ? $src['name'] : 'Source ' . $src['id'],
            'type' => $type,
            'pipeWireNodeName' => $src['pipeWireNodeName'],
            'enabled' => $enabled,
            'width' => isset($src['width']) ? intval($src['width']) : 320,
            'height' => isset($src['height']) ? intval($src['height']) : 240,
            'framerate' => isset($src['framerate']) ? intval($src['framerate']) : 10,
            'audioEnabled' => $audioEnabled,
            'audioPipeWireNodeName' => $src['audioPipeWireNodeName'],
        );

        switch ($type) {
            case 'videotestsrc':
                $entry['pattern'] = isset($src['pattern']) ? $src['pattern'] : 'smpte';
                break;
            case 'v4l2src':
                $entry['device'] = isset($src['device']) ? $src['device'] : '/dev/video0';
                break;
            case 'rtspsrc':
                $entry['uri'] = isset($src['uri']) ? $src['uri'] : '';
                $entry['latency'] = isset($src['latency']) ? intval($src['latency']) : 200;
                if (empty($entry['uri']))
                    continue 2;
                break;
            case 'urisrc':
                $entry['uri'] = isset($src['uri']) ? $src['uri'] : '';
                $entry['bufferSec'] = isset($src['bufferSec']) ? floatval($src['bufferSec']) : 3.0;
                if (empty($entry['uri']))
                    continue 2;
                break;
            case 'rtpsrc':
                $entry['port'] = isset($src['port']) ? intval($src['port']) : 5004;
                $entry['encoding'] = isset($src['encoding']) ? $src['encoding'] : 'H264';
                $entry['multicastGroup'] = isset($src['multicastGroup']) ? $src['multicastGroup'] : '';
                if ($entry['port'] < 1024 || $entry['port'] > 65535)
                    $entry['port'] = 5004;
                break;
            default:
                continue 2;
        }

        if ($enabled) {
            $enabledCount++;
        }

        $sourceConfig[] = $entry;
    }
    unset($src);

    // Save back with generated node names
    file_put_contents($configFile, json_encode($data, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES));

    // Write generated config (read by fppd VideoInputManager)
    $genFile = $settings['mediaDirectory'] . "/config/pipewire-video-input-sources.json";
    file_put_contents($genFile, json_encode($data, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES));

    // Write flat array read by fppd
    $fppFile = $settings['mediaDirectory'] . "/config/pipewire-video-input-sources-gen.json";
    file_put_contents($fppFile, json_encode($sourceConfig, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES));

    // Signal fppd to reload video input sources
    @SendCommand("reloadVideoInputs");

    return json(array(
        "status" => "OK",
        "message" => $enabledCount . " video input source(s) configured",
        "sources" => $sourceConfig,
    ));
}

/////////////////////////////////////////////////////////////////////////////
// GET /api/pipewire/video/input-sources/v4l2-devices
// Returns available V4L2 video capture devices
function GetV4L2Devices()
{
    $devices = array();

    // Enumerate /dev/video* devices
    $videoDevs = glob('/dev/video*');
    if ($videoDevs === false) {
        return json(array("devices" => array()));
    }

    foreach ($videoDevs as $devPath) {
        // Use v4l2-ctl to get device capabilities
        $output = array();
        $ret = 0;
        exec("v4l2-ctl -d " . escapeshellarg($devPath) . " --all 2>/dev/null", $output, $ret);
        if ($ret !== 0)
            continue;

        $info = implode("\n", $output);

        // Only include capture devices (not output or m2m)
        if (strpos($info, 'Video Capture') === false)
            continue;

        // Extract device name
        $name = $devPath;
        if (preg_match('/Card type\s*:\s*(.+)/', $info, $m)) {
            $name = trim($m[1]);
        }

        $devices[] = array(
            'device' => $devPath,
            'name' => $name,
        );
    }

    return json(array("devices" => $devices));
}

/////////////////////////////////////////////////////////////////////////////
// GET /api/pipewire/video/routing
// Returns a combined view of video input sources and video output groups
// with the current source assignment for each group.
function GetVideoRoutingMatrix()
{
    global $settings;

    $groupsFile = $settings['mediaDirectory'] . "/config/pipewire-video-groups.json";
    $sourcesFile = $settings['mediaDirectory'] . "/config/pipewire-video-input-sources.json";

    // Load video output groups
    $videoGroups = array();
    if (file_exists($groupsFile)) {
        $data = json_decode(file_get_contents($groupsFile), true);
        if (is_array($data) && isset($data['videoOutputGroups'])) {
            $videoGroups = $data['videoOutputGroups'];
        }
    }

    // Load video input sources
    $videoSources = array();
    if (file_exists($sourcesFile)) {
        $data = json_decode(file_get_contents($sourcesFile), true);
        if (is_array($data) && isset($data['videoInputSources'])) {
            $videoSources = $data['videoInputSources'];
        }
    }

    // Compute node names for sources
    $sourceSummary = array();
    foreach ($videoSources as $src) {
        if (!isset($src['enabled']) || !$src['enabled'])
            continue;
        $nameSlug = preg_replace('/[^a-zA-Z0-9_]/', '_', strtolower(isset($src['name']) ? $src['name'] : 'source'));
        $nodeName = "fpp_video_src_" . $src['id'] . "_" . $nameSlug;
        $sourceSummary[] = array(
            'id' => intval($src['id']),
            'name' => isset($src['name']) ? $src['name'] : 'Source ' . $src['id'],
            'type' => isset($src['type']) ? $src['type'] : '',
            'pipeWireNodeName' => $nodeName,
        );
    }

    // Build group summaries with current assignment
    $groupSummary = array();
    foreach ($videoGroups as $grp) {
        if (!isset($grp['enabled']) || !$grp['enabled'])
            continue;
        $memberTypes = array();
        if (isset($grp['members']) && is_array($grp['members'])) {
            foreach ($grp['members'] as $m) {
                if (isset($m['type']))
                    $memberTypes[] = $m['type'];
            }
        }
        $groupSummary[] = array(
            'id' => intval($grp['id']),
            'name' => isset($grp['name']) ? $grp['name'] : 'Group ' . $grp['id'],
            'videoSource' => isset($grp['videoSource']) ? $grp['videoSource'] : '',
            'streamSlots' => isset($grp['streamSlots']) && is_array($grp['streamSlots']) ? array_values(array_map('intval', $grp['streamSlots'])) : array(),
            'memberTypes' => $memberTypes,
        );
    }

    return json(array(
        'videoSources' => $sourceSummary,
        'videoGroups' => $groupSummary,
    ));
}

/////////////////////////////////////////////////////////////////////////////
// POST /api/pipewire/video/routing
// Save video routing assignments (which source feeds which output group).
// Body: { "assignments": [{ "groupId": 1, "videoSource": "fpp_video_src_1_camera" }, ...] }
function SaveVideoRoutingMatrix()
{
    global $settings;
    $configFile = $settings['mediaDirectory'] . "/config/pipewire-video-groups.json";

    $body = json_decode(file_get_contents('php://input'), true);
    if (!$body || !isset($body['assignments']) || !is_array($body['assignments'])) {
        http_response_code(400);
        return json(array("status" => "error", "message" => "Missing assignments array"));
    }

    if (!file_exists($configFile)) {
        return json(array("status" => "error", "message" => "No video output groups configured"));
    }

    $data = json_decode(file_get_contents($configFile), true);
    if (!is_array($data) || !isset($data['videoOutputGroups'])) {
        return json(array("status" => "error", "message" => "Invalid video groups config"));
    }

    // Index assignments by group ID
    $assignmentMap = array();
    foreach ($body['assignments'] as $a) {
        $gid = intval($a['groupId']);
        $assignmentMap[$gid] = isset($a['videoSource']) ? $a['videoSource'] : '';
    }

    // Update each group's videoSource
    $updated = 0;
    foreach ($data['videoOutputGroups'] as &$grp) {
        $gid = isset($grp['id']) ? intval($grp['id']) : 0;
        if (isset($assignmentMap[$gid])) {
            $grp['videoSource'] = $assignmentMap[$gid];
            $updated++;
        }
    }
    unset($grp);

    file_put_contents($configFile, json_encode($data, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES));
    GenerateBackupViaAPI('Video routing matrix was modified.');

    return json(array("status" => "OK", "message" => "$updated video group(s) updated"));
}

// ═══════════════════════════════════════════════════════════════════════════
// SIMPLE PIPEWIRE MODE — single sound card + single video output
// ═══════════════════════════════════════════════════════════════════════════
//
// The Simple PipeWire backend provides the same UI experience as the
// Hardware Direct backend (one AudioOutput card selector, one VideoOutput
// connector selector) while reusing the PipeWire/GStreamer runtime.
//
// On Apply, the user's AudioOutput/VideoOutput selections are translated
// into a single-group PipeWire audio config and a single-output PipeWire
// video config, then handed to ApplyPipeWireAudioGroups/Video via in-memory
// override data.  Advanced-mode JSON files (pipewire-audio-groups.json /
// pipewire-video-groups.json) are NOT touched, so users can switch back to
// Advanced mode without losing their custom configuration.
//
// Records of the synthesised configs are kept at:
//   $mediaDirectory/config/pipewire-audio-groups-simple.json
//   $mediaDirectory/config/pipewire-video-groups-simple.json
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// Helper: Resolve an ALSA card number (e.g. "0", "1") to its stable
// ALSA card ID (read from /proc/asound/cardN/id).  Used by Simple PipeWire
// mode to translate the legacy AudioOutput numeric setting into the cardId
// string consumed by PipeWire audio groups.
function ResolveAlsaCardNumberToId($cardNum)
{
    $cardNum = (string) intval($cardNum);
    $idFile = "/proc/asound/card{$cardNum}/id";
    if (file_exists($idFile)) {
        $id = trim(@file_get_contents($idFile));
        if ($id !== '')
            return $id;
    }
    return '';
}

/////////////////////////////////////////////////////////////////////////////
// Build a single-group audio data structure from the AudioOutput setting.
// Returns an array shaped like the contents of pipewire-audio-groups.json.
function BuildSimpleAudioGroupsData($audioOutput)
{
    // $audioOutput is the persisted AudioOutput value (a stable ALSA card ID,
    // or a legacy numeric index). Normalize either form to a card ID.
    $cardId = NormalizeAudioOutputToCardId($audioOutput);
    if ($cardId === '') {
        return array("groups" => array());
    }

    return array(
        "groups" => array(
            array(
                "id" => 1,
                "name" => "Default",
                "enabled" => true,
                "channels" => 2,
                "volume" => 100,
                "activeGroup" => true,
                "members" => array(
                    array(
                        "cardId" => $cardId,
                        "channels" => 2,
                        "delayMs" => 0,
                        "volume" => 100,
                    ),
                ),
            ),
        ),
    );
}

/////////////////////////////////////////////////////////////////////////////
// Build a single-output video data structure from the VideoOutput setting.
// Returns an array shaped like pipewire-video-groups.json, or null if the
// VideoOutput value does not map to a real DRM connector (Disabled,
// --Default--, etc.) — in which case the video pipeline is skipped.
function BuildSimpleVideoGroupsData($videoOutput)
{
    if (empty($videoOutput) || $videoOutput === 'Disabled' || $videoOutput === '--Default--') {
        return array("videoOutputGroups" => array());
    }

    // Validate the connector exists; bail out if not (e.g. Composite-1 on
    // a board without that connector).
    $connectors = GetVideoConnectors();
    $found = false;
    foreach ($connectors as $c) {
        if ($c['connector'] === $videoOutput) {
            $found = true;
            break;
        }
    }
    if (!$found) {
        return array("videoOutputGroups" => array());
    }

    return array(
        "videoOutputGroups" => array(
            array(
                "id" => 1,
                "name" => "Default",
                "enabled" => true,
                "members" => array(
                    array(
                        "type" => "hdmi",
                        "connector" => $videoOutput,
                        "scaling" => "fit",
                    ),
                ),
            ),
        ),
    );
}

/////////////////////////////////////////////////////////////////////////////
// Apply the Simple PipeWire configuration derived from the AudioOutput and
// VideoOutput settings.  This is invoked automatically by the settings
// save handler whenever MediaBackend, AudioOutput, or VideoOutput changes
// while MediaBackend == 'pipewire-simple'.
//
// Returns a JSON-encoded status response (compatible with the existing
// /api/pipewire/audio/groups/apply contract).
function ApplyPipeWireSimpleConfig($skipRestart = false)
{
    global $settings;

    $audioOutput = isset($settings['AudioOutput']) ? $settings['AudioOutput'] : '0';
    $videoOutput = isset($settings['VideoOutput']) ? $settings['VideoOutput'] : '';

    $audioData = BuildSimpleAudioGroupsData($audioOutput);
    $videoData = BuildSimpleVideoGroupsData($videoOutput);

    // Persist a record of the synthesised config so the boot-time apply
    // (and any future debugging) can see what Simple mode produced.
    $audioRecord = $settings['mediaDirectory'] . "/config/pipewire-audio-groups-simple.json";
    $videoRecord = $settings['mediaDirectory'] . "/config/pipewire-video-groups-simple.json";
    @file_put_contents($audioRecord, json_encode($audioData, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES));
    @file_put_contents($videoRecord, json_encode($videoData, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES));

    // Apply audio first (restarts pipewire services unless $skipRestart), then video.
    // Both functions accept an in-memory override and skip writing to the
    // advanced-mode JSON files when invoked this way.
    ob_start();
    ApplyPipeWireAudioGroups($audioData, $skipRestart);
    ob_end_clean();

    ob_start();
    ApplyPipeWireVideoGroups($videoData);
    ob_end_clean();

    $cardId = NormalizeAudioOutputToCardId($audioOutput);
    return json(array(
        "status" => "OK",
        "message" => "Simple PipeWire config applied",
        "audioCardId" => $cardId,
        "videoConnector" => $videoOutput,
    ));
}
