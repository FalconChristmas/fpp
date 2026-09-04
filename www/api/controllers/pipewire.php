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
// Helper: wait until fppd answers again after a restart.
//
// Callers that resume playback or push volumes immediately after restarting the
// stack would otherwise race a daemon that is still coming back, and their
// requests would be silently dropped.  Uses the same status endpoint
// StopFppdPlaybackSafe() probes.
function WaitForFppdReady($timeoutSec = 20)
{
    $deadline = time() + $timeoutSec;
    $ctx = stream_context_create(array('http' => array('timeout' => 1)));
    while (time() < $deadline) {
        if (@file_get_contents('http://localhost:32322/fppd/status', false, $ctx) !== false) {
            return true;
        }
        usleep(250000);
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////
// Helper: restart the PipeWire daemons in dependency order, then bring fppd
// back on top of them.
//
// The fppd restart is not optional and not a separate concern: fppd cannot
// survive the daemons restarting under it.  The GStreamer PipeWire plugin
// caches its connection process-wide, so every later pipewiresink reuses a dead
// one -- the pipeline reports PLAYING, nothing is posted on the bus, and
// playback is silent with nothing logged.  Measured directly: after a bare
// daemon restart with fppd left alone, the card never opens and hw_ptr never
// advances; fppd's own preroll watchdog eventually notices and self-restarts,
// but only after three wedges of 15s each.  Restarting it here turns 45s of
// silent failure into a controlled restart.
//
// SendCommand('restart') rather than `systemctl restart fppd`: fppd re-execs
// itself, which is an equally fresh process image (it is what clears the cached
// connection), and it resumes a playlist it was running, which systemctl cannot.
// It also keeps the PID, so repeated applies do not burn the unit's start limit.
//
// Playback is deliberately NOT stopped here.  Callers that resume afterwards
// have to stop it themselves, because stopping is how they capture what to
// resume; doing it here as well would double-stop and lose that state.
function RestartPipeWireStack($restartFppd = true)
{
    global $SUDO;

    $services = array();
    exec($SUDO . " /usr/bin/systemctl restart fpp-pipewire.service 2>&1", $o1, $r1);
    $services['fpp-pipewire'] = ($r1 === 0);
    usleep(500000);

    exec($SUDO . " /usr/bin/systemctl restart fpp-wireplumber.service 2>&1", $o2, $r2);
    $services['fpp-wireplumber'] = ($r2 === 0);

    // Wait for the PipeWire core socket before starting pulse, which depends on it.
    for ($i = 0; $i < 10; $i++) {
        if (file_exists('/run/pipewire-fpp/pipewire-0'))
            break;
        usleep(250000);
    }
    exec($SUDO . " /usr/bin/systemctl restart fpp-pipewire-pulse.service 2>&1", $o3, $r3);
    $services['fpp-pipewire-pulse'] = ($r3 === 0);

    // And for the PulseAudio compat socket: most callers run pactl-based volume
    // restores immediately after this.
    for ($i = 0; $i < 10; $i++) {
        if (file_exists('/run/pipewire-fpp/pulse/native'))
            break;
        usleep(250000);
    }

    if ($restartFppd) {
        @SendCommand('restart');
        WaitForFppdReady();
    }

    return $services;
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
// POST /api/pipewire/audio/services/restart
//
// Recovery action surfaced from the System Health page when the PipeWire
// health check is degraded. Bounces the three PipeWire systemd services and
// then restarts fppd so its in-process GStreamer pipelines reconnect to the
// freshly-started daemons.
//
// Order matters:
//   1. Quiesce fppd playback first, so it releases its pipewiresink client
//      before we yank PipeWire out from under an active pipeline (avoids the
//      non-idempotent GStreamer teardown path).
//   2. Restart pipewire -> wireplumber -> pipewire-pulse (pulse depends on the
//      pipewire core socket).
//   3. Restart fppd (full re-exec) to rebuild the media stack cleanly.
function RestartPipeWireServices()
{
    global $settings;

    // The services only exist on real FPP platforms, not macOS dev boxes.
    if (isset($settings['Platform']) && $settings['Platform'] == "MacOS") {
        http_response_code(400);
        return json(array("status" => "ERROR", "message" => "Not supported on this platform"));
    }

    // 1. Stop playback so fppd releases its PipeWire client cleanly.
    $playback = StopFppdPlaybackSafe();

    // 2/3. Restart the services in dependency order, then fppd on top of them.
    $services = RestartPipeWireStack();

    $allOk = $services['fpp-pipewire'] && $services['fpp-wireplumber'] && $services['fpp-pipewire-pulse'];

    return json(array(
        "status" => $allOk ? "OK" : "ERROR",
        "message" => $allOk
            ? "PipeWire services restarted and FPPD restart requested"
            : "One or more PipeWire services failed to restart",
        "services" => $services,
        "wasPlaying" => $playback['wasPlaying']
    ));
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
        // Restart the stack to pick up the removal.  Stop playback first so fppd
        // releases its client cleanly; RestartPipeWireStack() brings fppd back.
        if (!$skipRestart) {
            StopFppdPlaybackSafe();
            RestartPipeWireStack();
        }
        return json(array("status" => "OK", "message" => "Audio groups cleared, PipeWire restarted"));
    }

    // Generate PipeWire config
    $genResult = GeneratePipeWireGroupsConfig($data['groups'], true);
    $conf = $genResult['conf'];
    $resolvedCardMap = $genResult['cardNodeMap'];

    // Does this apply actually change the graph?
    //
    // PipeWire only reads these confs at startup, so a change means a stack
    // restart, which means stopping the show and restarting fppd.  An apply that
    // produces byte-identical config needs none of that -- and most do, because
    // Apply is pressed after looking at a page as often as after editing it.
    // Comparing here, before playback is stopped, means a no-op apply costs
    // nothing at all rather than a silent gap in the show.
    $graphUnchanged = ($conf === @file_get_contents("/etc/pipewire/pipewire.conf.d/97-fpp-audio-groups.conf"));

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
                // This conf is loaded at daemon startup too, so a change here
                // needs the restart just as much as a change to the 97 conf.
                if ($igConf !== @file_get_contents($igConfPath)) {
                    $graphUnchanged = false;
                }
                $igTmpFile = tempnam(sys_get_temp_dir(), 'fpp_pw_ig_');
                file_put_contents($igTmpFile, $igConf);
                exec($SUDO . " cp " . escapeshellarg($igTmpFile) . " " . escapeshellarg($igConfPath));
                exec($SUDO . " chmod 644 " . escapeshellarg($igConfPath));
                unlink($igTmpFile);
                file_put_contents($settings['mediaDirectory'] . "/config/pipewire-input-groups.conf", $igConf);
            }
        }
    }

    // Group membership decides the USB headroom in the boot-time sink conf too,
    // and that conf is loaded at daemon startup like the others, so a change
    // there needs the restart just as much as a change to 97.
    if (SyncBootAdapterUsbHeadroom($data['groups'], $SUDO)) {
        $graphUnchanged = false;
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

    // Nothing PipeWire loads at startup has changed, so there is nothing for a
    // restart to pick up.  Leave the running graph and the show alone; the
    // volume restore below still runs, since that applies live.
    if ($graphUnchanged) {
        RestorePipeWireGroupVolumes($data['groups']);
        return json(array(
            "status" => "OK",
            "message" => "Audio groups already match the running graph; nothing to restart"
        ));
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

    // Restart the stack, and fppd with it.  fppd was NOT being restarted here,
    // which left it holding a dead PipeWire connection after every audio-groups
    // Apply -- silent playback with nothing logged, until its own preroll
    // watchdog gave up 45s later and restarted it anyway.  Playback was already
    // stopped above, and is resumed at the end of this function.
    RestartPipeWireStack();

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
// Detect the maximum playback channel count an ALSA card supports, without
// opening the device.  Reading only the live-negotiated count from
// /proc hw_params just echoes however PipeWire happened to open the card
// (usually 2) and reads "closed" when idle, so it can never discover that a
// card is really multi-channel (issue #2620).
// Mirrors the boot-time detection in src/boot/FPPINIT_Audio.cpp.
function DetectAlsaCardMaxChannels($cardNum, $aplayLine, $isUsbCard)
{
    $channels = 2; // Default stereo

    // Live negotiated params — only meaningful while the device is open,
    // and only ever raises the count (a stereo negotiation on an 8ch card
    // must not mask capability info from the sources below).
    exec("cat /proc/asound/card$cardNum/pcm0p/sub0/hw_params 2>/dev/null", $hwOutput, $hwRet);
    if (!$hwRet && !empty($hwOutput)) {
        foreach ($hwOutput as $hwLine) {
            if (preg_match('/^channels:\s*(\d+)/', trim($hwLine), $chMatch)) {
                $channels = max($channels, intval($chMatch[1]));
            }
        }
    }
    unset($hwOutput);

    // USB cards: /proc/asound/cardN/stream0 lists every playback altset with
    // its exact channel count — full capability without opening the card.
    if ($isUsbCard) {
        $stream0 = @file_get_contents("/proc/asound/card$cardNum/stream0");
        if (!empty($stream0)) {
            $capturePos = strpos($stream0, "\nCapture:");
            $playSection = ($capturePos !== false) ? substr($stream0, 0, $capturePos) : $stream0;
            if (preg_match_all('/\bChannels:\s+(\d+)/', $playSection, $chMatches)) {
                foreach ($chMatches[1] as $ch) {
                    $channels = max($channels, intval($ch));
                }
            }
        }
    }

    // HDA codecs (HDMI etc.) publish max channels in /proc codec info.
    exec("cat /proc/asound/card$cardNum/codec* 2>/dev/null | grep -i 'max channels' | head -1", $codecOutput);
    if (!empty($codecOutput) && preg_match('/(\d+)/', $codecOutput[0], $chMatch)) {
        $channels = max($channels, intval($chMatch[1]));
    }
    unset($codecOutput);

    // Known multi-channel I2S cards whose drivers only advertise a continuous
    // channel range and expose none of the capability info above.
    $channels = max($channels, PipeWireCardChannelQuirk($aplayLine));

    return min($channels, 8); // cap at 7.1
}

/////////////////////////////////////////////////////////////////////////////
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
// Keep in sync with achievedRateForFormat() in src/boot/FPPINIT_Audio.cpp.
function PipeWireProbeFormatRate($alsaPath, $pwFmt, $rate, $channels)
{
    $alsaFmt = array(
        'S32LE' => 'S32_LE',
        'S24_32LE' => 'S24_LE',
        'S24LE' => 'S24_3LE',
        'S16LE' => 'S16_LE',
    );
    if (!isset($alsaFmt[$pwFmt]))
        return 0;
    $rate = intval($rate) > 0 ? intval($rate) : 44100;
    $channels = intval($channels) > 0 ? intval($channels) : 2;
    $out = array();
    $rc = -1;
    exec('head -c 8192 /dev/zero | timeout 2 aplay -D ' . escapeshellarg($alsaPath)
        . ' -t raw -f ' . $alsaFmt[$pwFmt] . ' -r ' . $rate . ' -c ' . $channels . ' - 2>&1',
        $out, $rc);
    // Unverifiable (device busy, probe timed out, aplay missing).
    if ($rc !== 0)
        return 0;
    if (preg_match('/not accurate \(requested = \d+Hz, got = (\d+)Hz\)/i', implode("\n", $out), $m))
        return intval($m[1]);
    return $rate; // negotiated exactly what was asked for
}

/////////////////////////////////////////////////////////////////////////////
// Pick the widest PCM format in $fmtLine that costs no sample rate relative to
// the universally-safe S16LE fallback.
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
// free.  Keep in sync with bestFormatForRate() in src/boot/FPPINIT_Audio.cpp.
function PipeWireBestFormatForRate($fmtLine, $alsaPath, $rate, $channels)
{
    $wider = array('S32_LE' => 'S32LE', 'S24_LE' => 'S24_32LE', 'S24_3LE' => 'S24LE');
    $anyWider = false;
    foreach ($wider as $alsaName => $pwName) {
        if (strpos($fmtLine, $alsaName) !== false)
            $anyWider = true;
    }
    if (!$anyWider)
        return 'S16LE';
    // The rate to beat. If this cannot be established (device busy, probe timed
    // out) there is nothing to compare against, so decline to widen: a needlessly
    // narrow format costs only bit depth, a wrongly wide one costs all audio.
    $baselineRate = PipeWireProbeFormatRate($alsaPath, 'S16LE', $rate, $channels);
    if ($baselineRate <= 0)
        return 'S16LE';
    foreach ($wider as $alsaName => $pwName) {
        if (strpos($fmtLine, $alsaName) === false)
            continue;
        if (PipeWireProbeFormatRate($alsaPath, $pwName, $rate, $channels) >= $baselineRate)
            return $pwName;
    }
    return 'S16LE';
}

/////////////////////////////////////////////////////////////////////////////
// ALSA HW params for a card, as an aplay-style block.
//
// A device PipeWire already holds cannot be opened, so aplay returns nothing at
// all.  It is still readable through /proc, which reports what the hardware is
// running *right now* -- observed fact, and a better answer than the probe could
// give even if it could run.  Returns '' when the device is neither openable nor
// open (genuinely dead, e.g. HDMI with nothing connected).  $live is set true
// when the result came from /proc, which tells the caller the format is already
// established and must not be re-probed.
// Keep in sync with the same fallback in setupAudio(), src/boot/FPPINIT_Audio.cpp.
function PipeWireCardHwParams($cidSafe, $cardNum, &$live)
{
    $live = false;
    $out = shell_exec('timeout 2 aplay -D hw:' . escapeshellarg($cidSafe)
        . ' --dump-hw-params /dev/zero 2>&1 | head -40');
    if ($out !== null && strpos($out, 'HW Params') !== false)
        return $out;
    if ($cardNum < 0)
        return '';
    $procHw = @file_get_contents('/proc/asound/card' . intval($cardNum) . '/pcm0p/sub0/hw_params');
    if ($procHw === false || $procHw === '' || strpos($procHw, 'closed') !== false)
        return '';
    $fmt = 'S16_LE';
    $ch = '2';
    if (preg_match('/format:\s*(\S+)/', $procHw, $m))
        $fmt = $m[1];
    if (preg_match('/channels:\s*(\d+)/', $procHw, $m))
        $ch = $m[1];
    $live = true;
    return "HW Params of device (busy — synthesised from /proc)\nFORMAT:  $fmt\nCHANNELS: $ch\n";
}

/////////////////////////////////////////////////////////////////////////////
// Quirk table for multi-channel I2S cards whose drivers advertise only a
// continuous channel range (e.g. "[2 8]"), which the conservative non-USB
// heuristics clamp to stereo (issue #2620).  $cardDesc is any descriptive
// text for the card (aplay -l line, /proc/asound/cards entry, card name).
// Returns the minimum channel count the card must be configured with, or 0.
// Keep in sync with quirkMinChannelsForCard() in src/boot/FPPINIT_Audio.cpp.
function PipeWireCardChannelQuirk($cardDesc)
{
    $quirks = array(
        'hifiberry_dac8x' => 8, // HiFiBerry DAC8x / Raspiaudio 8xOUT: 4x stereo DACs on one I2S bus
    );
    $descLc = strtolower($cardDesc);
    foreach ($quirks as $needle => $qch) {
        if (strpos($descLc, $needle) !== false) {
            return $qch;
        }
    }
    return 0;
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
    $pwChannelsByNodeName = array(); // PW sink node name => configured channel count
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
                if (isset($pwProps['audio.channels'])) {
                    $pwChannelsByNodeName[$pwName] = intval($pwProps['audio.channels']);
                }
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
                    $channelInfo = array();
                    exec($SUDO . " amixer -c $cardNum scontrols 2>/dev/null | cut -f2 -d\"'\"", $mixerOutput, $mixerRet);
                    if (!$mixerRet && !empty($mixerOutput)) {
                        foreach ($mixerOutput as $mixer) {
                            $channelInfo[] = trim($mixer);
                        }
                    }
                    unset($mixerOutput);

                    $isUsbCard = strpos($line, 'USB Audio') !== false;
                    $channels = DetectAlsaCardMaxChannels($cardNum, $line, $isUsbCard);

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

                    // The resolved PipeWire sink knows how many channels the
                    // adapter was configured with (boot-time detection in
                    // FPPINIT_Audio.cpp) — trust it when it exceeds what we
                    // detected here without opening the device.
                    if (!empty($pwNodeName) && isset($pwChannelsByNodeName[$pwNodeName])) {
                        $channels = max($channels, min($pwChannelsByNodeName[$pwNodeName], 8));
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
// Helper: find the USB Transaction Translator (TT) that a full/low-speed
// device's isochronous traffic is scheduled through.
//
// Full-speed (12M) USB audio devices behind a high-speed hub share that
// hub's TT. A single-TT hub (bDeviceProtocol 01 — e.g. the VL805's internal
// hub that all four USB-A ports on a Pi 4 hang off) has one ~12Mbps
// periodic budget for ALL downstream full-speed devices, so two sound
// cards behind it fail with kernel "Not enough bandwidth for altsetting"
// errors (issue #2673). A multi-TT hub (bDeviceProtocol 02 — most decent
// powered hubs) has one TT per port, so each card gets its own budget.
//
// Returns null when the device is attached directly to an xHCI root port
// (no TT involved — the controller schedules full-speed natively), else:
//   array('hubPath', 'hubProduct', 'multiTT', 'port', 'ttKey')
// Devices sharing the same non-empty ttKey compete for one 12Mbps budget.
function UsbFindTTOwner($devPath, $sysfs = '/sys/bus/usb/devices')
{
    $path = $devPath;
    // Walk up the topology: "3-2.4.2" -> hub "3-2.4" (port 2) -> hub "3-2" ...
    // A path without '.' ("3-1") is attached directly to a root port.
    while (($dotPos = strrpos($path, '.')) !== false) {
        $port = substr($path, $dotPos + 1);
        $parent = substr($path, 0, $dotPos);
        $pSpeed = trim(@file_get_contents("$sysfs/$parent/speed"));
        if ($pSpeed === '480') {
            // Nearest high-speed hub upstream — this hub's TT carries the
            // device's full-speed traffic.
            $proto = trim(@file_get_contents("$sysfs/$parent/bDeviceProtocol"));
            $multiTT = ($proto === '2' || $proto === '02');
            return array(
                'hubPath' => $parent,
                'hubProduct' => trim(@file_get_contents("$sysfs/$parent/product")),
                'multiTT' => $multiTT,
                'port' => $port,
                // Multi-TT: one TT per port, so key on hub+port.
                // Single-TT: every downstream device shares one TT.
                'ttKey' => $multiTT ? ($parent . ':' . $port) : $parent,
            );
        }
        // Full-speed hub in between — the TT is further upstream.
        $path = $parent;
    }
    return null;
}

/////////////////////////////////////////////////////////////////////////////
// GET /api/pipewire/audio/usb-check
// Detect USB sound cards that share a single Transaction Translator and
// therefore cannot all stream audio at once (Pi 4 onboard ports, issue
// #2673). Also surfaces any "Not enough bandwidth" kernel log entries so
// the UI can report the failure even if the topology heuristic misses it.
function GetUsbAudioBandwidthCheck()
{
    global $SUDO;

    $sysfs = '/sys/bus/usb/devices';

    // Map USB "busnum:devnum" -> sysfs topology path (e.g. "3-2.4.2")
    $usbByBusDev = array();
    foreach (glob($sysfs . '/*') as $entry) {
        $name = basename($entry);
        // Skip interface entries ("3-2.4:1.0") and root hubs ("usb3")
        if (strpos($name, ':') !== false || !preg_match('/^\d+-[\d.]+$/', $name))
            continue;
        $busnum = trim(@file_get_contents("$entry/busnum"));
        $devnum = trim(@file_get_contents("$entry/devnum"));
        if ($busnum !== '' && $devnum !== '')
            $usbByBusDev[intval($busnum) . ':' . intval($devnum)] = $name;
    }

    // Enumerate USB-attached ALSA cards via /proc/asound/card*/usbbus
    $devices = array();
    $byTTKey = array();
    foreach (glob('/proc/asound/card*/usbbus') as $usbbusFile) {
        if (!preg_match('#/card(\d+)/#', $usbbusFile, $cm))
            continue;
        $cardNum = intval($cm[1]);
        $busDev = trim(@file_get_contents($usbbusFile)); // "003/006"
        if (!preg_match('#^(\d+)/(\d+)$#', $busDev, $bm))
            continue;
        $key = intval($bm[1]) . ':' . intval($bm[2]);
        if (!isset($usbByBusDev[$key]))
            continue;
        $usbPath = $usbByBusDev[$key];
        $speed = trim(@file_get_contents("$sysfs/$usbPath/speed"));

        $device = array(
            'cardNum' => $cardNum,
            'cardId' => trim(@file_get_contents("/proc/asound/card$cardNum/id")),
            'product' => trim(@file_get_contents("$sysfs/$usbPath/product")),
            'usbPath' => $usbPath,
            'speed' => $speed,
            'ttKey' => '',
            'hubProduct' => '',
            'singleTT' => false,
        );

        // Only full/low-speed devices consume shared TT bandwidth;
        // high-speed (480M+) audio devices are not affected.
        if ($speed === '12' || $speed === '1.5') {
            $tt = UsbFindTTOwner($usbPath);
            if ($tt !== null) {
                $device['ttKey'] = $tt['ttKey'];
                $device['hubProduct'] = $tt['hubProduct'];
                $device['singleTT'] = !$tt['multiTT'];
                $byTTKey[$tt['ttKey']][] = count($devices);
            }
        }
        $devices[] = $device;
    }

    // Any TT domain carrying 2+ full-speed sound cards is oversubscribed
    $conflicts = array();
    foreach ($byTTKey as $ttKey => $indexes) {
        if (count($indexes) < 2)
            continue;
        $cards = array();
        foreach ($indexes as $i) {
            $cards[] = array(
                'cardNum' => $devices[$i]['cardNum'],
                'cardId' => $devices[$i]['cardId'],
                'product' => $devices[$i]['product'],
                'usbPath' => $devices[$i]['usbPath'],
            );
        }
        $conflicts[] = array(
            'ttKey' => $ttKey,
            'hubProduct' => $devices[$indexes[0]]['hubProduct'],
            'cards' => $cards,
        );
    }

    // Kernel log evidence of the failure (needs root — dmesg is restricted)
    $kernelErrors = array();
    exec($SUDO . " dmesg 2>/dev/null | grep -iE 'not enough bandwidth|usb_set_interface failed' | tail -n 6", $kernelErrors);

    $model = trim(str_replace("\0", '', @file_get_contents('/proc/device-tree/model')));

    return json(array(
        'status' => (!empty($conflicts) || !empty($kernelErrors)) ? 'warning' : 'ok',
        'model' => $model,
        'devices' => $devices,
        'conflicts' => $conflicts,
        'kernelErrors' => $kernelErrors,
    ));
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
        // Synthesize the click track in pure PHP (no ffmpeg/sox) — see
        // GenerateSyncClickTrack, which reproduces the original alternating
        // high/low click waveform bit-for-bit.
        if (!GenerateSyncClickTrack($clickFile)) {
            return json(array("status" => "ERROR", "message" => "Failed to generate click track"));
        }
    }

    return StartSyncCalibrationPlayback($sinkName, $clickFile, true);
}

// Synthesize the sync calibration click track as a 16-bit mono 44100 Hz WAV.
// Pattern, per 2-second block repeated 30x => 60s:
//   [20ms @ 1000Hz][980ms silence][20ms @ 600Hz][980ms silence]
// This is identical to the track the old ffmpeg pipeline produced
// (sine 0.02s + apad 0.98s, concat high/low, looped to 60s) but needs no
// external tools.
function GenerateSyncClickTrack($path)
{
    $rate = 44100;
    $clickSamples = (int) round(0.02 * $rate);          // 882 samples (20 ms)
    $silenceSamples = $rate - $clickSamples;            // remainder of the 1s window
    $amplitude = 32767;

    // Build the two 20 ms click bursts, sample by sample.
    $click1000 = '';
    $click600 = '';
    for ($i = 0; $i < $clickSamples; $i++) {
        $t = $i / $rate;
        $v1 = (int) round($amplitude * sin(2 * M_PI * 1000 * $t));
        $v6 = (int) round($amplitude * sin(2 * M_PI * 600 * $t));
        // signed 16-bit little-endian
        $click1000 .= pack('v', $v1 & 0xFFFF);
        $click600 .= pack('v', $v6 & 0xFFFF);
    }
    $silence = str_repeat("\x00\x00", $silenceSamples);

    // One 2-second block (high click + silence + low click + silence), x30.
    $block = $click1000 . $silence . $click600 . $silence;
    $pcm = str_repeat($block, 30);

    $dataLen = strlen($pcm);
    $byteRate = $rate * 2;   // mono, 16-bit => 2 bytes/sample
    $header = 'RIFF' . pack('V', 36 + $dataLen) . 'WAVE'
        . 'fmt ' . pack('V', 16) . pack('v', 1) . pack('v', 1)
        . pack('V', $rate) . pack('V', $byteRate) . pack('v', 2) . pack('v', 16)
        . 'data' . pack('V', $dataLen);

    return @file_put_contents($path, $header . $pcm) !== false;
}

// Spawn a GStreamer pipeline (decodebin -> pipewiresink) targeted at the
// group sink.  $loop: if true, restart playback indefinitely until stopped.
function StartSyncCalibrationPlayback($sinkName, $absFile, $loop)
{
    global $SUDO;

    $pidFile = "/tmp/fpp_sync_cal.pid";
    $logFile = "/tmp/fpp_sync_cal.log";

    $env = "PIPEWIRE_RUNTIME_DIR=/run/pipewire-fpp XDG_RUNTIME_DIR=/run/pipewire-fpp";
    $sinkArg = escapeshellarg($sinkName);
    $fileArg = escapeshellarg($absFile);

    // Decode any container/codec with GStreamer and route straight to the
    // group's combine sink via pipewiresink (matches fppd's media engine; no
    // ffmpeg/pw-cat).
    $playOnce = "$env gst-launch-1.0 -q filesrc location=$fileArg ! decodebin ! audioconvert ! audioresample"
        . " ! pipewiresink target-object=$sinkArg 2>>" . escapeshellarg($logFile);

    $runFile = null;
    if ($loop) {
        // Repeat indefinitely via a shell loop gated on a per-session guard
        // file.  The guard MUST be created before launch — otherwise the loop's
        // first `[ -f ... ]` test can win the race against the write below and
        // the loop exits before ever playing (this happens reliably under the
        // sudo'd web context, which is why the click track "wouldn't restart").
        // It is also uniquely named per session so a stale loop can never be
        // revived by a later start re-creating a shared file.
        $token = bin2hex(random_bytes(6));
        $runFile = "/tmp/fpp_sync_cal.{$token}.run";
        @file_put_contents($runFile, "1");
        $inner = "while [ -f " . escapeshellarg($runFile) . " ]; do $playOnce; sleep 0.1; done";
    } else {
        $inner = $playOnce;
    }

    // Launch via setsid in background; record the supervisor PID so Stop can kill the whole group.
    $shell = "setsid sh -c " . escapeshellarg($inner) . " >/dev/null 2>>" . escapeshellarg($logFile) . " & echo \$!";
    $cmd = $SUDO . " $env sh -c " . escapeshellarg($shell);

    $pid = trim(shell_exec($cmd));
    if (!ctype_digit($pid)) {
        if ($runFile !== null) {
            @unlink($runFile);
        }
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

    // Invalidate every loop guard first so any running loop exits after its
    // current iteration even if the PID-group kill below misses it.
    foreach (glob("/tmp/fpp_sync_cal.*.run") as $runFile) {
        @unlink($runFile);
    }

    $pid = null;
    if (file_exists($pidFile)) {
        $pid = intval(trim(@file_get_contents($pidFile)));
        @unlink($pidFile);
    }

    if ($pid > 0) {
        // Kill the entire process group (setsid'd) — supervisor + gst-launch.
        @exec($SUDO . " kill -TERM -" . $pid . " 2>/dev/null");
        usleep(150000);
        @exec($SUDO . " kill -KILL -" . $pid . " 2>/dev/null");
    }

    // Belt-and-suspenders: nuke any straggler gst-launch playback fed by us.
    @exec($SUDO . " pkill -f 'gst-launch-1.0.*target-object=fpp_' 2>/dev/null");

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
        // Stop playback first so fppd releases its client cleanly, and let
        // RestartPipeWireStack() bring fppd back -- it was left holding a dead
        // PipeWire connection here, so clearing input groups silently killed
        // audio until something else restarted it.
        if (!$skipRestart) {
            StopFppdPlaybackSafe();
            RestartPipeWireStack();
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
        // Stop playback first so fppd releases its client cleanly, and let
        // RestartPipeWireStack() bring fppd back -- it was left holding a dead
        // PipeWire connection here, so clearing input groups silently killed
        // audio until something else restarted it.
        if (!$skipRestart) {
            StopFppdPlaybackSafe();
            RestartPipeWireStack();
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

    // Restart the stack, and fppd with it (skipped when $skipRestart=true — the
    // caller owns the sequence).  fppd was not being restarted here either, so
    // applying input groups left it on a dead PipeWire connection.
    if (!$skipRestart) {
        StopFppdPlaybackSafe();
        RestartPipeWireStack();
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

/**
 * List PipeWire Audio/Source nodes registered by plugins with fppd's
 * AudioSourceRegistry (via AudioSourceRegistry::INSTANCE.registerSource()).
 * Passthrough to fppd's HTTP server; returns an empty list when fppd is
 * not running. The registry is in-fppd-memory only -- saved input-group
 * members store the nodeName themselves, so nothing needs to persist here.
 *
 * @route GET /api/pipewire/audio/plugin-sources
 * @response 200 Object with a `sources` array.
 */
function GetPipeWirePluginSources()
{
    $result = array("sources" => array());

    $ch = curl_init("http://127.0.0.1:32322/pipewire/audio/plugin-sources");
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($ch, CURLOPT_CONNECTTIMEOUT, 1);
    curl_setopt($ch, CURLOPT_TIMEOUT, 3);
    $raw = curl_exec($ch);
    curl_close($ch);

    if ($raw !== false) {
        $data = json_decode($raw, true);
        if (is_array($data) && isset($data['sources']) && is_array($data['sources'])) {
            $result['sources'] = $data['sources'];
        }
    }

    return json($result);
}

/////////////////////////////////////////////////////////////////////////////
// Helper: PipeWire channel position array for a channel count.
// Same table the input-mixing UI uses, so a mapping synthesised here and one
// picked by hand in the UI describe the same layout.
function PipeWireChannelPositions($channels)
{
    static $positions = array(
        1 => array("MONO"),
        2 => array("FL", "FR"),
        3 => array("FL", "FR", "FC"),
        4 => array("FL", "FR", "RL", "RR"),
        5 => array("FL", "FR", "FC", "RL", "RR"),
        6 => array("FL", "FR", "FC", "LFE", "RL", "RR"),
        7 => array("FL", "FR", "FC", "LFE", "RL", "RR", "RC"),
        8 => array("FL", "FR", "FC", "LFE", "RL", "RR", "SL", "SR")
    );
    $channels = intval($channels);
    return isset($positions[$channels]) ? $positions[$channels] : $positions[2];
}

/////////////////////////////////////////////////////////////////////////////
// Helper: Channel count of a PipeWire node, by node.name.
// Returns 0 when the node is not in the graph -- which is normal for nodes
// fppd publishes, because PipeWire is started before fppd.  Callers must treat
// 0 as "unknown", not as "no channels".
// pw-dump is cached for the life of the request: config generation asks about
// several nodes and the graph cannot change underneath a single generation.
function ResolvePipeWireNodeChannels($nodeName)
{
    global $SUDO;
    static $channelsByNode = null;

    if (empty($nodeName)) {
        return 0;
    }

    if ($channelsByNode === null) {
        $channelsByNode = array();
        $env = "PIPEWIRE_RUNTIME_DIR=/run/pipewire-fpp XDG_RUNTIME_DIR=/run/pipewire-fpp";
        $raw = shell_exec($SUDO . " " . $env . " pw-dump 2>/dev/null");
        $objects = $raw ? json_decode($raw, true) : null;
        if (is_array($objects)) {
            foreach ($objects as $obj) {
                $props = isset($obj['info']['props']) ? $obj['info']['props'] : null;
                if (!$props || !isset($props['node.name'])) {
                    continue;
                }
                $ch = 0;
                if (isset($props['audio.channels'])) {
                    $ch = intval($props['audio.channels']);
                } elseif (isset($obj['info']['params']['Format'][0]['channels'])) {
                    $ch = intval($obj['info']['params']['Format'][0]['channels']);
                }
                if ($ch > 0) {
                    $channelsByNode[$props['node.name']] = $ch;
                }
            }
        }
    }

    return isset($channelsByNode[$nodeName]) ? $channelsByNode[$nodeName] : 0;
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
                // PipeWire Audio/Source node -- a video input's extracted audio,
                // or a node published by a plugin through fppd's
                // AudioSourceRegistry (e.g. the SMPTE plugin's LTC timecode).
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

            // Channel layout for the two ends of the loopback.
            //
            // An explicit mapping picked in the UI wins.  Failing that, when we
            // know the source's channel count and it differs from the group's,
            // pin each end to its own layout.  Without this the two streams are
            // whatever module-loopback defaults to (stereo [FL FR]) while the
            // capture side carries stream.dont-remix, which disables up/downmix
            // -- so a mono source lines up with neither FL nor FR and the member
            // passes silence instead of audio.  That is what made the SMPTE
            // plugin's mono LTC node inaudible in a stereo mix bus (issue #2754).
            $srcChannels = isset($mbr['channels']) ? intval($mbr['channels']) : 0;
            if ($srcChannels <= 0) {
                $srcChannels = ResolvePipeWireNodeChannels($sourceTarget);
            }
            $capturePos = null;
            $playbackPos = null;
            $map = isset($mbr['channelMapping']) ? $mbr['channelMapping'] : null;
            // The config file is user-editable JSON, so validate rather than
            // letting a malformed mapping throw out of the apply.
            if (is_array($map) &&
                isset($map['sourceChannels']) && is_array($map['sourceChannels']) && count($map['sourceChannels']) &&
                isset($map['groupChannels']) && is_array($map['groupChannels']) && count($map['groupChannels'])) {
                $capturePos = $map['sourceChannels'];
                $playbackPos = $map['groupChannels'];
            } elseif ($srcChannels > 0 && $srcChannels !== $groupChannels) {
                $capturePos = PipeWireChannelPositions($srcChannels);
                $playbackPos = PipeWireChannelPositions($groupChannels);
            }

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

            if ($capturePos !== null) {
                $conf .= "        audio.channels = " . count($capturePos) . "\n";
                $conf .= "        audio.position = [ " . implode(" ", $capturePos) . " ]\n";
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

            if ($playbackPos !== null) {
                $conf .= "        audio.channels = " . count($playbackPos) . "\n";
                $conf .= "        audio.position = [ " . implode(" ", $playbackPos) . " ]\n";
                // Widening the capture end's layout (mono LTC into a stereo bus)
                // needs an explicit upmix.  dont-remix is deliberately absent on
                // this end so the mix can happen at all.
                if (count($playbackPos) > count($capturePos)) {
                    $conf .= "        channelmix.upmix = true\n";
                }
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
// Helper: true when the board has no real sound card -- i.e. there is at least
// one enabled group with members, and every one of those members is the
// synthetic snd-dummy card.  Used to substitute a null sink for the full
// adapter/filter-chain/combine-stream graph.  Matches 'Dummy' exactly, the
// same test buildSimplePipeWireGroupsConf() uses, so the boot-time C++ path
// and this one always agree about which config to write.
function GroupsAreDummyOnly($groups)
{
    if (!is_array($groups) || empty($groups)) {
        return false;
    }
    $sawMember = false;
    foreach ($groups as $group) {
        if (!isset($group['enabled']) || !$group['enabled']) {
            continue;
        }
        if (!isset($group['members']) || !is_array($group['members'])) {
            continue;
        }
        foreach ($group['members'] as $member) {
            $cardId = isset($member['cardId']) ? trim(strval($member['cardId'])) : '';
            if ($cardId === '') {
                continue;
            }
            if ($cardId !== 'Dummy') {
                return false;
            }
            $sawMember = true;
        }
    }
    return $sawMember;
}

/////////////////////////////////////////////////////////////////////////////
// Helper: Generate PipeWire combine-stream config from groups
// Re-decide the USB headroom in the boot-time 95-fpp-alsa-sink.conf for the
// groups being applied here, and rewrite it if the answer changed.
//
// That conf is written once per boot by fppinit, which picks each USB card's
// api.alsa.headroom from the audio groups as they stood at the time -- see
// pipewireGroupSizeByCard() in src/boot/FPPINIT_Audio.cpp for why the answer
// depends on the groups at all.  Editing a group here can invalidate it, and in
// the direction that matters: moving a second card into a USB card's group makes
// that card a follower, but its boot adapter would keep the sole-sink headroom
// until the next reboot, which is exactly the crackle the headroom exists to
// prevent.  Rewriting it now keeps the conf and the graph in agreement, and the
// caller's restart is what makes PipeWire read it.
//
// Only fpp_alsa_* sink adapters already carrying a USB headroom value are
// touched.  Non-USB adapters are written with 256 and are never a candidate, so
// the value on the line is itself a reliable marker of which cards are in scope
// -- which matters because the conf records normalised node names, not card IDs,
// and a card dropped from every group must still be walked back down.
//
// Returns true if the file changed, i.e. the stack needs a restart to pick it up.
function SyncBootAdapterUsbHeadroom($groups, $SUDO)
{
    $confPath = "/etc/pipewire/pipewire.conf.d/95-fpp-alsa-sink.conf";
    $conf = @file_get_contents($confPath);
    if ($conf === false || $conf === '')
        return false;

    // Normalised node suffixes of cards sharing a group with another sink.
    // Same rule as $cardSharesGroup in GeneratePipeWireGroupsConfig().
    $sharedNorm = array();
    foreach ($groups as $group) {
        if (!isset($group['enabled']) || !$group['enabled'])
            continue;
        if (!isset($group['members']) || count($group['members']) < 2)
            continue;
        foreach ($group['members'] as $member) {
            if (empty($member['cardId']))
                continue;
            $sharedNorm[preg_replace('/[^a-zA-Z0-9_]/', '_', strtolower($member['cardId']))] = true;
        }
    }

    $seenShared = array();
    $updated = preg_replace_callback(
        '/(node\.name = "fpp_alsa_([a-z0-9_]+)"(?:(?!node\.name)[\s\S])*?api\.alsa\.headroom = )(\d+)/',
        function ($m) use ($sharedNorm, &$seenShared) {
            $current = intval($m[3]);
            if ($current != 1024 && $current != 4096)
                return $m[0]; // non-USB (256) — not ours to touch
            $shared = !empty($sharedNorm[$m[2]]);
            if ($shared)
                $seenShared[$m[2]] = true;
            return $m[1] . ($shared ? 4096 : 1024);
        },
        $conf
    );
    if ($updated === null)
        return false;

    // Keep the marker fppinit compares against in step with the values above,
    // so its boot-time fast path does not see a mismatch it would answer with a
    // full (slow) re-probe.  See alsaSinkConfUsbHeadroomTag() in FPPINIT_Audio.cpp.
    $sharedList = array_keys($seenShared);
    sort($sharedList);
    $tag = "# usb shared-group cards:" . (empty($sharedList) ? "" : " " . implode(" ", $sharedList));
    $updated = preg_replace('/^# usb shared-group cards:.*$/m', $tag, $updated, 1, $tagCount);
    if (!$tagCount) {
        // Conf predates the marker (generation <= 6): fppinit will rewrite the
        // whole file on the next boot because the generation tag no longer
        // matches, so leave the header alone rather than half-upgrading it.
    }

    if ($updated === $conf)
        return false;

    $tmpFile = tempnam(sys_get_temp_dir(), 'fpp_pw_sink_');
    file_put_contents($tmpFile, $updated);
    exec($SUDO . " cp " . escapeshellarg($tmpFile) . " " . escapeshellarg($confPath));
    exec($SUDO . " chmod 644 " . escapeshellarg($confPath));
    unlink($tmpFile);
    return true;
}

function GeneratePipeWireGroupsConfig($groups, $returnCardMap = false)
{
    global $SUDO, $settings;

    // Passive links let the graph reach idle so PipeWire can suspend the card.
    // A non-passive playback link keeps the ALSA sink running forever, so the
    // whole chain (combine -> filter-chain -> sink, plus any resampling) is
    // recomputed every quantum even with nothing playing -- measured at 4-5% of
    // a core on single-core boards.  Kept in sync with the Simple-mode C++
    // generator in FPPINIT_Audio.cpp; defaults on, settable so it can be turned
    // off if a card misbehaves coming out of suspend.
    $passiveLine = (!isset($settings['PipeWirePassiveSinks']) || $settings['PipeWirePassiveSinks'] != '0')
        ? "        node.passive = true\n"
        : "";

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

    // No real sound card: every enabled member resolved to the synthetic
    // snd-dummy.  Emit a null sink instead of the normal
    // hw:Dummy adapter -> filter-chain -> combine-stream graph, which would
    // pin the dummy PCM open and cycle the graph at the quantum rate forever
    // (~47 wakeups/s, 4% of a core on a single-core board) for audio that can
    // never be heard.  fppd still needs a sink so media playback runs in real
    // time and sequence timing holds, and a null sink suspends when idle.
    // Must use the `adapter` factory -- spa-node-factory yields no usable
    // ports and linking fails.  Mirrors buildSimplePipeWireGroupsConf() in
    // src/boot/FPPINIT_Audio.cpp; keep the two in sync.
    if (GroupsAreDummyOnly($groups)) {
        $rate = isset($settings['PipeWireSampleRate']) ? intval($settings['PipeWireSampleRate']) : 48000;
        if ($rate <= 0) {
            $rate = 48000;
        }
        $conf .= "# No sound card detected - a null sink keeps media playback (and so\n";
        $conf .= "# sequence timing) working while letting the graph park when idle.\n";
        $conf .= "context.objects = [\n";
        $conf .= "  { factory = adapter\n";
        $conf .= "    args = {\n";
        $conf .= "      factory.name = support.null-audio-sink\n";
        $conf .= "      node.name = \"fpp_group_default\"\n";
        $conf .= "      node.description = \"FPP Audio (no sound card)\"\n";
        $conf .= "      media.class = \"Audio/Sink\"\n";
        $conf .= "      audio.rate = $rate\n";
        $conf .= "      audio.channels = 2\n";
        $conf .= "      audio.position = [ FL FR ]\n";
        $conf .= "      monitor.channel-volumes = true\n";
        $conf .= "    }\n";
        $conf .= "  }\n";
        $conf .= "]\n";
        if ($returnCardMap) {
            return array('conf' => $conf, 'cardNodeMap' => array());
        }
        return $conf;
    }

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
    // Boot-time fpp_alsa_* adapters, read from the conf that declares them
    // rather than from the live graph.
    //
    // Whether this function must emit a custom adapter for a card cannot be
    // decided from whether the node currently exists, because an fpp_alsa_* node
    // exists precisely when FPP declared it -- and one of the two places it can
    // be declared is the conf this function is generating.  Deciding from live
    // state therefore feeds back on itself: the adapter is present, so it is
    // judged unnecessary and dropped; the next restart removes it, so it is
    // judged necessary and re-added.  The config oscillated between two shapes,
    // every apply differed from the last, and in the half of the cycle where the
    // adapter was dropped its member targeted a node that did not exist -- a
    // silent output.  (Observed on a vc4hdmi0 the boot probe skips as IEC958.)
    //
    // 95-fpp-alsa-sink.conf is unaffected by what happens here, so asking it
    // "does something else already provide this adapter" is stable.
    $bootAdapterChannels = array(); // node.name => declared audio.channels
    $bootConf = @file_get_contents("/etc/pipewire/pipewire.conf.d/95-fpp-alsa-sink.conf");
    if ($bootConf) {
        if (preg_match_all('/node\.name\s*=\s*"(fpp_alsa_[a-z0-9_]+)"(.*?)audio\.channels\s*=\s*(\d+)/s',
                           $bootConf, $bm, PREG_SET_ORDER)) {
            foreach ($bm as $row) {
                $bootAdapterChannels[$row[1]] = intval($row[3]);
            }
        }
    }

    // What the custom adapters in the CURRENT graph were built with.
    //
    // Deciding between hw: and sysdefault: needs the card's capability list,
    // which is only readable while the card is free.  With it busy -- which it
    // is whenever a show is playing -- /proc reports the single format the
    // device is RUNNING, never the IEC958-only capability that made sysdefault:
    // necessary, so the decision silently flips back to hw:.  The next apply,
    // with the card idle, flips it back.  That made every apply during playback
    // differ from the one before it, so the gate below could never conclude
    // "nothing changed" and every apply restarted the stack mid-show.
    //
    // The running graph was built from a probe taken when the card was free, so
    // it is the best evidence available; carry it forward rather than re-deriving
    // it from a device that cannot answer.
    $prevAdapters = array(); // node.name => ['path' => ..., 'format' => ...]
    $prevConf = @file_get_contents("/etc/pipewire/pipewire.conf.d/97-fpp-audio-groups.conf");
    if ($prevConf) {
        if (preg_match_all('/node\.name\s*=\s*"(fpp_alsa_[a-z0-9_]+)".*?api\.alsa\.path\s*=\s*"([^"]+)".*?audio\.format\s*=\s*"([^"]+)"/s',
                           $prevConf, $pm, PREG_SET_ORDER)) {
            foreach ($pm as $row) {
                $prevAdapters[$row[1]] = array('path' => $row[2], 'format' => $row[3]);
            }
        }
    }

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
                        // Known multi-channel I2S cards override the stereo
                        // range clamp above (issue #2620).  Match against the
                        // card's /proc/asound/cards entry, which carries the
                        // driver name (the card ID alone does not).
                        $procCards = @file_get_contents('/proc/asound/cards');
                        if (!empty($procCards)
                            && preg_match('/^\s*' . intval($cardNum) . '\s+\[[^\]]*\]:\s*(.*)$/m', $procCards, $pcM)) {
                            $unresolvedMaxCh = min(8, max($unresolvedMaxCh, PipeWireCardChannelQuirk($pcM[1])));
                        }
                        // Detect best audio format: widest that costs no rate
                        // against the S16LE fallback (see PipeWireBestFormatForRate).
                        $unresolvedFmt = 'S16LE';
                        if (preg_match('/FORMAT[^:]*:\s+(.+)/i', $testOutput, $fmtM)) {
                            $fmtProbePath = ($needsSysdefault ? 'sysdefault:' : 'hw:') . $cardId;
                            $unresolvedFmt = PipeWireBestFormatForRate($fmtM[1], $fmtProbePath, 44100, $unresolvedMaxCh);
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

    // Which cards share a group with another sink, which decides their USB
    // headroom below.  KEEP IN SYNC with pipewireGroupSizeByCard() in
    // src/boot/FPPINIT_Audio.cpp, which makes the same call for the boot-time
    // adapters in 95-fpp-alsa-sink.conf; that function's comment carries the
    // reasoning and the measurements.  Built up front rather than inside the
    // member loop because a card can appear in more than one group and sharing
    // any of them is enough.
    $cardSharesGroup = array();
    foreach ($groups as $group) {
        if (!isset($group['enabled']) || !$group['enabled'])
            continue;
        if (!isset($group['members']) || count($group['members']) < 2)
            continue;
        foreach ($group['members'] as $member) {
            if (!empty($member['cardId']))
                $cardSharesGroup[$member['cardId']] = true;
        }
    }

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
            // Judged against the boot conf, not the live graph: see
            // $bootAdapterChannels above for why live state cannot answer this.
            $targetIsMissingFppAdapter = (strpos($resolvedTarget, 'fpp_alsa_') === 0)
                && !isset($bootAdapterChannels[$resolvedTarget]);
            // If an fpp_alsa_* boot-time adapter already exists with enough channels,
            // don't create a duplicate — the boot-time node already covers this card.
            $existingAdapterChannels = isset($bootAdapterChannels[$resolvedTarget])
                ? $bootAdapterChannels[$resolvedTarget] : 0;
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
                    // A card PipeWire already holds cannot be opened, so this
                    // falls back to /proc, and $fmtFromLiveDevice says so.
                    $fmtFromLiveDevice = false;
                    $fmtOut = PipeWireCardHwParams($cidSafe, ResolveCardIdToNumber($cid), $fmtFromLiveDevice);
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
                    if ($fmtOut && $fmtFromLiveDevice && isset($prevAdapters['fpp_alsa_' . $cidNorm])) {
                        // Busy card, and we already know what the running graph
                        // was built with.  Neither the format nor the hw: vs
                        // sysdefault: choice can be re-derived from /proc, so
                        // reuse both rather than letting them flip.  See
                        // $prevAdapters above.
                        $prev = $prevAdapters['fpp_alsa_' . $cidNorm];
                        $adapterNeedsSysdefault = (strpos($prev['path'], 'sysdefault:') === 0);
                        $adapterFmt = $prev['format'];
                    } elseif ($fmtOut && $fmtFromLiveDevice) {
                        // The single format in /proc is not an advertisement, it
                        // is what the hardware is running right now.  Re-probing
                        // it would only reopen a device we already know is busy,
                        // get EBUSY, and (by the bias-to-safe rule) demote a
                        // working S32 card to S16LE.  Take the live format.
                        if (preg_match('/FORMAT[^:]*:\s+(\S+)/i', $fmtOut, $fmtM)) {
                            $pwNames = array('S32_LE' => 'S32LE', 'S24_LE' => 'S24_32LE',
                                'S24_3LE' => 'S24LE', 'S16_LE' => 'S16LE');
                            $adapterFmt = isset($pwNames[$fmtM[1]]) ? $pwNames[$fmtM[1]] : 'S16LE';
                        }
                    } elseif ($fmtOut && preg_match('/FORMAT[^:]*:\s+(.+)/i', $fmtOut, $fmtM)) {
                        $fmtProbePath = ($adapterNeedsSysdefault ? 'sysdefault:' : 'hw:') . $cidSafe;
                        $adapterFmt = PipeWireBestFormatForRate($fmtM[1], $fmtProbePath, $memberRate, $memberCh);
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
            // A USB card's oscillator is its own, so when it shares a driver
            // graph with another sink it may end up following that sink's clock
            // and needs headroom for the adaptive resampler to absorb the drift.
            // Alone in its group it *is* the graph driver and the extra frames
            // are latency bought for nothing.  See pipewireGroupSizeByCard() in
            // src/boot/FPPINIT_Audio.cpp for the reasoning and the measurements;
            // these values must match kUsbHeadroom* there.
            $cardNum = ResolveCardIdToNumber($cid);
            $isUsb = false;
            if ($cardNum >= 0) {
                $driverLink = @readlink("/sys/class/sound/card$cardNum/device/driver");
                $isUsb = ($driverLink !== false && str_contains(basename($driverLink), 'usb'));
            }
            $adapterHeadroom = 256;
            if ($isUsb)
                $adapterHeadroom = !empty($cardSharesGroup[$cid]) ? 4096 : 1024;
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
            $conf .= $passiveLine;
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
        $conf .= $passiveLine;
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

            // Channel mapping for this member within the group.
            //
            // combine-stream pairs combine.audio.position[i] with
            // audio.position[i] positionally, matching the former against the
            // group's channel labels.  Two failure modes must be filtered out
            // here (issue #2620):
            //   - "" (the UI's "-- None --"): an empty token silently vanishes
            //     when PipeWire parses the array, shifting every later
            //     channel's mapping by one.
            //   - a label not present in the group's layout (stale mapping
            //     after the group's channel count was reduced): combine-stream
            //     falls back to SAME-INDEX routing, scrambling the mapping
            //     instead of muting the channel.
            // Dropping the pair from BOTH arrays leaves that card channel with
            // no stream channel bearing its label, which plays silence.
            $memberChannels = isset($member['channels']) ? intval($member['channels']) : 2;
            $memberPos = isset($channelPositions[$memberChannels]) ? $channelPositions[$memberChannels] : "[ FL FR ]";
            $memberPosList = isset($channelPositionArrays[$memberChannels]) ? $channelPositionArrays[$memberChannels] : $channelPositionArrays[2];
            $groupPosList = isset($channelPositionArrays[$groupChannels]) ? $channelPositionArrays[$groupChannels] : $channelPositionArrays[2];

            $mappingProvided = isset($member['channelMapping']) && !empty($member['channelMapping'])
                && isset($member['channelMapping']['cardChannels'])
                && isset($member['channelMapping']['groupChannels']);
            if ($mappingProvided) {
                $mapCard = $member['channelMapping']['cardChannels'];
                $mapGroup = $member['channelMapping']['groupChannels'];
            } else {
                // Identity mapping — same filter applies so member channels
                // without a matching group label go silent rather than being
                // index-scrambled (e.g. an 8ch card in a stereo group).
                $mapCard = $memberPosList;
                $mapGroup = $memberPosList;
            }
            $validCard = array();
            $validGroup = array();
            $pairCount = min(count($mapCard), count($mapGroup));
            for ($pi = 0; $pi < $pairCount; $pi++) {
                $gLabel = trim(strval($mapGroup[$pi]));
                if ($gLabel === '' || !in_array($gLabel, $groupPosList))
                    continue;
                $validCard[] = trim(strval($mapCard[$pi]));
                $validGroup[] = $gLabel;
            }
            if (!empty($validCard)) {
                $combinePos = "[ " . implode(" ", $validGroup) . " ]";
                $streamPos = "[ " . implode(" ", $validCard) . " ]";
            } elseif ($mappingProvided) {
                // Every channel mapped to None — the member is intentionally
                // silent; don't create a stream for it at all.
                $conf .= "        # Member $memberNodeName skipped — all channels mapped to None\n";
                continue;
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
        // Match the running graph, not whichever file happens to exist.  Simple
        // mode's groups are in pipewire-audio-groups-simple.json and its single
        // group is "Default" (node fpp_group_default); the advanced file holds a
        // different set under different names, so reading it in Simple mode
        // pactl'd nodes that do not exist and restored nothing.  Kept in sync
        // with restorePipeWireVolumes() in FPPINIT_Audio.cpp.
        $backend = isset($settings['MediaBackend']) ? strtolower($settings['MediaBackend']) : 'pipewire-simple';
        $configFile = $settings['mediaDirectory'] . "/config/" .
            ($backend === 'pipewire-simple' ? "pipewire-audio-groups-simple.json" : "pipewire-audio-groups.json");
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
    return json(array("instances" => array(), "ptpEnabled" => true, "ptpInterface" => "",
        "ptpDomain" => 0, "ptpRole" => "auto"));
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

    // Global PTP settings.  fppd clamps these too, but rejecting here means
    // the user gets told rather than silently having the value changed.
    if (!isset($parsed['ptpEnabled']))
        $parsed['ptpEnabled'] = true;
    if (!isset($parsed['ptpInterface']))
        $parsed['ptpInterface'] = "";
    if (!isset($parsed['ptpDomain'])) {
        $parsed['ptpDomain'] = 0;
    } else {
        $domain = intval($parsed['ptpDomain']);
        if ($domain < 0 || $domain > 127) {
            http_response_code(400);
            return json(array("status" => "ERROR",
                "message" => "PTP domain must be between 0 and 127"));
        }
        $parsed['ptpDomain'] = $domain;
    }
    if (!isset($parsed['ptpRole'])) {
        $parsed['ptpRole'] = "auto";
    } else if (!in_array($parsed['ptpRole'], array("auto", "follower", "master"))) {
        http_response_code(400);
        return json(array("status" => "ERROR",
            "message" => "PTP role must be auto, follower or master"));
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
// Rebuild the audio group graph if an AES67/Opus RTP change has altered it.
//
// A group member targets a network sender by a node name derived from the
// instance -- aes67_<slug of instance name>_send.  Enabling, disabling or
// renaming an instance therefore changes what the group config must contain,
// and GeneratePipeWireGroupsConfig() drops a member whose instance is disabled
// ("# WARNING: Could not find PipeWire sinks for: aes67_1").
//
// Applying the instance without rebuilding leaves those two disagreeing: fppd
// starts the send pipeline, but no filter chain targets its node, and the
// pipeline uses node.autoconnect=false so it cannot preroll with nothing
// feeding it.  gst_element_set_state() then blocks and returns FAILURE, which
// surfaces only as "AES67: audio send stream failed to start" -- with no hint
// that the audio groups are the thing that is stale.  A user enabling a stream
// has no way to know they must also re-apply the audio groups, so do it here.
//
// Generation is pure, so compare first and only pay for the rebuild (which
// restarts the PipeWire stack and fppd) when the graph actually changes.
// Editing an already-wired instance's bitrate, ptime or multicast address does
// not alter the group config and so does not restart anything.
//
// Returns true if the graph was rebuilt, in which case fppd has been restarted
// and will apply the instance config itself as it initialises.
function RebuildAudioGraphForSenderChange()
{
    global $settings;

    $groupsFile = $settings['mediaDirectory'] . "/config/pipewire-audio-groups.json";
    if (!file_exists($groupsFile)) {
        return false;
    }
    $gd = json_decode(file_get_contents($groupsFile), true);
    if (!is_array($gd) || empty($gd['groups'])) {
        return false;
    }

    ob_start();
    $gen = GeneratePipeWireGroupsConfig($gd['groups'], true);
    ob_end_clean();
    $newConf = is_array($gen) ? (isset($gen['conf']) ? $gen['conf'] : '') : $gen;
    if ($newConf === '') {
        return false;
    }

    $destConf = "/etc/pipewire/pipewire.conf.d/97-fpp-audio-groups.conf";
    $curConf = file_exists($destConf) ? file_get_contents($destConf) : '';
    if ($newConf === $curConf) {
        return false; // wiring unchanged -- nothing to restart
    }

    // Writes the conf, restarts the stack and brings fppd back on top of it,
    // preserving and resuming playback around the whole thing.  fppd coming back
    // is what starts the sender against the rebuilt graph.
    ob_start();
    ApplyPipeWireAudioGroups();
    ob_end_clean();
    return true;
}

// Run an fppd command over fppd's own HTTP API and report what happened.
//
// The AES67/Opus RTP applies used to POST to http://localhost/api/command with
// a 10s timeout, which was wrong twice over.  That URL loops the request back
// through Apache into a second PHP worker just to reach fppd, when fppd serves
// the same commands directly on 32322 (as the rest of this file already does).
// And 10s is not enough: applying an AES67 config tears down the GStreamer
// pipelines and joins the SAP/PTP threads, each of which waits out a 1-2s
// socket timeout, so a real apply regularly runs longer than that -- longer
// still when it queues behind another apply on AES67Manager's mutex.
// file_get_contents() returns false on timeout exactly as it does when nothing
// is listening, so a slow but perfectly successful apply came back to the user
// as "Failed to signal fppd - is it running?".
//
// ignore_errors keeps the body of a non-2xx reply, so fppd's own error text
// (it answers 500 when a command fails) survives instead of collapsing to
// false and being reported as an unreachable daemon.
//
// Returns array(bool ok, string message).
function SignalFPPDCommand($command, $timeoutSeconds = 120)
{
    $url = 'http://localhost:32322/command/' . rawurlencode($command);
    $ctx = stream_context_create(array(
        'http' => array(
            'method' => 'GET',
            'timeout' => $timeoutSeconds,
            'ignore_errors' => true
        )
    ));

    $body = @file_get_contents($url, false, $ctx);
    if ($body === false) {
        return array(false, "Could not reach fppd on port 32322, or '" . $command .
            "' did not finish within " . $timeoutSeconds . "s");
    }

    // file_get_contents() populates $http_response_header in this scope.
    $code = 0;
    if (isset($http_response_header) && is_array($http_response_header)) {
        foreach ($http_response_header as $hdr) {
            if (preg_match('#^HTTP/\S+\s+(\d+)#', $hdr, $m)) {
                $code = (int) $m[1];
            }
        }
    }
    if ($code >= 400) {
        return array(false, "fppd could not run '" . $command . "' (HTTP " . $code . "): " . trim($body));
    }

    return array(true, trim($body));
}

function ApplyAES67Instances()
{
    global $settings;

    // AES67 is managed by AES67Manager in fppd (GStreamer-based).
    // Signal fppd to reload AES67 config via the command API.
    $configFile = $settings['mediaDirectory'] . "/config/pipewire-aes67-instances.json";

    if (!file_exists($configFile)) {
        // Signal cleanup
        SignalFPPDCommand('AES67 Cleanup');
        return json(array("status" => "OK", "message" => "No AES67 instances configured"));
    }

    // If this change alters which sender nodes the groups feed, the graph has to
    // be rebuilt first -- otherwise the send pipeline starts with nothing
    // connected to it and fails.  See RebuildAudioGraphForSenderChange().
    if (RebuildAudioGraphForSenderChange()) {
        return json(array(
            "status" => "OK",
            "message" => "AES67 configuration applied; audio graph rebuilt and FPPD restarted",
            // Same flag the audio/input group applies return, so callers can tell
            // the slow path (stack restarted underneath them) from the cheap one.
            "restartRequired" => true
        ));
    }

    // Signal fppd to apply config
    list($ok, $msg) = SignalFPPDCommand('AES67 Apply');
    if (!$ok) {
        return json(array("status" => "ERROR", "message" => $msg));
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

    // Fallback: fppd not running or endpoint not available.
    // Shape must match AES67Manager::render_GET() so the page does not have to
    // handle two different layouts -- it previously returned a flat ptpSynced
    // while fppd returns a nested ptp{} object.
    return json(array(
        "pipelines" => array(),
        "ptp" => array(
            "synced" => false,
            "offsetNs" => 0,
            "grandmasterId" => "",
            "grandmasterAddress" => "",
            "grandmasterViaBoundary" => false,
            "portState" => "fppd not responding",
            "isGrandmaster" => false,
            "enabled" => false,
            "domain" => 0,
            "role" => "auto"
        ),
        "discoveredStreams" => array(),
        "active" => false
    ));
}

// Ask fppd for the SDP of every send instance.  Generated there rather than
// here on purpose: the same BuildSDP() that feeds the SAP announcer also feeds
// this, so an exported file cannot drift from what is actually being
// announced, and it carries the live PTP grandmaster in ts-refclk -- neither
// of which this layer can know.  Returns null when fppd is not reachable.
function AES67FetchSDP()
{
    $result = @file_get_contents('http://localhost:32322/aes67/sdp');
    if ($result === false) {
        return null;
    }
    $data = json_decode($result, true);
    if ($data === null || !isset($data['streams'])) {
        return null;
    }
    return $data['streams'];
}

/**
 * Get the SDP session descriptions for the configured AES67 send streams
 *
 * Returns one entry per send instance, each carrying the RFC 4566 session
 * description fppd is announcing for it.  Intended for handing a stream to a
 * tool that cannot see FPP's SAP announcements -- Stream Monitor
 * (https://aes67.app), VLC, or an analyser on another VLAN.
 *
 * @route GET /api/pipewire/aes67/sdp
 * @response 200 Session descriptions for every send instance
 * ```json
 * {"status":"OK","streams":[{"instanceId":1,"name":"AES67 Stream 1","sessionName":"AES67 Stream 1","enabled":true,"sapEnabled":true,"multicastIP":"239.69.0.1","port":5004,"channels":2,"ptime":4,"filename":"aes67_stream_1.sdp","sdp":"v=0\r\n..."}]}
 * ```
 * @response 503 fppd is not running, so there is no applied configuration to describe
 */
function GetAES67SDP()
{
    $streams = AES67FetchSDP();
    if ($streams === null) {
        // Not an error the caller can fix by retrying: without fppd there is
        // no applied config and no PTP grandmaster, so any SDP built here
        // would describe a stream that is not on the wire.
        http_response_code(503);
        return json(array(
            "status" => "ERROR",
            "message" => "fppd is not responding — start fppd and apply the AES67 configuration first.",
            "streams" => array()
        ));
    }
    return json(array("status" => "OK", "streams" => $streams));
}

/**
 * Download one AES67 send stream as a .sdp file
 *
 * The same description as the endpoint above, served as application/sdp with
 * a Content-Disposition filename so it can be saved and opened directly in
 * VLC.  The web UI builds its download from the JSON; this exists so a stream
 * can be pulled with curl from the machine running the monitor.
 *
 * @route GET /api/pipewire/aes67/sdp/{InstanceId}
 * @response 200 The .sdp file for that instance
 * @response 404 No send instance with that id
 * @response 503 fppd is not running
 */
function GetAES67SDPFile()
{
    $id = intval(params('InstanceId'));
    $streams = AES67FetchSDP();
    if ($streams === null) {
        http_response_code(503);
        return json(array("status" => "ERROR", "message" => "fppd is not responding"));
    }
    foreach ($streams as $s) {
        if (isset($s['instanceId']) && intval($s['instanceId']) === $id) {
            $name = isset($s['filename']) ? basename($s['filename']) : ('aes67_' . $id . '.sdp');
            header("Content-Type: application/sdp");
            header("Content-Disposition: attachment; filename=\"" . $name . "\"");
            // application/sdp has no ExpiresByType rule, so Apache's
            // ExpiresDefault ("access plus 1 year") would apply here and a
            // browser would keep serving this description long after the
            // multicast address or channel count changed.  Setting Expires
            // ourselves also stops mod_expires from overwriting it.
            header("Cache-Control: no-store, must-revalidate");
            header("Expires: 0");
            ob_clean();
            flush();
            echo $s['sdp'];
            return;
        }
    }
    http_response_code(404);
    return json(array("status" => "ERROR", "message" => "No send instance with id " . $id));
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
        SignalFPPDCommand('Opus RTP Cleanup');
        return json(array("status" => "OK", "message" => "No Opus RTP instances configured"));
    }

    // Same coupling as AES67: an Opus RTP instance being enabled, disabled or
    // renamed changes the opusrtp_*_send node a group member targets, and the
    // send pipeline cannot start until a filter chain feeds it.
    if (RebuildAudioGraphForSenderChange()) {
        return json(array(
            "status" => "OK",
            "message" => "Opus RTP configuration applied; audio graph rebuilt and FPPD restarted"
        ));
    }

    list($ok, $msg) = SignalFPPDCommand('Opus RTP Apply');
    if (!$ok) {
        return json(array("status" => "ERROR", "message" => $msg));
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
/////////////////////////////////////////////////////////////////////////////
// Translate a member's region selection into the fractional crop rectangle
// fppd consumes.  Regions are stored as fractions of the source rather than
// pixels so the same configuration splits any resolution -- fppd resolves them
// against the negotiated caps at playback time.
//
// Returns null for a full-frame (or unset) region so the key stays out of the
// consumer config entirely, which is what the C++ default already means.
function VideoRegionToCrop($member)
{
    $region = isset($member['region']) ? $member['region'] : 'full';

    switch ($region) {
        case 'left':
            return array('x' => 0.0, 'y' => 0.0, 'width' => 0.5, 'height' => 1.0);
        case 'right':
            return array('x' => 0.5, 'y' => 0.0, 'width' => 0.5, 'height' => 1.0);
        case 'top':
            return array('x' => 0.0, 'y' => 0.0, 'width' => 1.0, 'height' => 0.5);
        case 'bottom':
            return array('x' => 0.0, 'y' => 0.5, 'width' => 1.0, 'height' => 0.5);
        case 'custom':
            // Stored as percentages in the UI; clamped here so a hand-edited
            // config can't produce a region that crops the frame to nothing.
            $x = isset($member['cropX']) ? floatval($member['cropX']) : 0.0;
            $y = isset($member['cropY']) ? floatval($member['cropY']) : 0.0;
            $w = isset($member['cropWidth']) ? floatval($member['cropWidth']) : 100.0;
            $h = isset($member['cropHeight']) ? floatval($member['cropHeight']) : 100.0;

            $x = max(0.0, min(99.0, $x));
            $y = max(0.0, min(99.0, $y));
            $w = max(1.0, min(100.0 - $x, $w));
            $h = max(1.0, min(100.0 - $y, $h));

            if ($x == 0.0 && $y == 0.0 && $w == 100.0 && $h == 100.0)
                return null;

            return array('x' => $x / 100.0, 'y' => $y / 100.0,
                'width' => $w / 100.0, 'height' => $h / 100.0);
        case 'full':
        default:
            return null;
    }
}

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
                    $crop = VideoRegionToCrop($member);
                    if ($crop !== null)
                        $entry['crop'] = $crop;
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
// Returns available V4L2 video capture devices.
//
// Only single-planar capture devices are returned.  That is deliberate:
// GStreamer's v4l2src (what VideoInputManager builds its pipelines around)
// only handles single-planar V4L2 capture, and every UVC webcam / USB
// capture dongle sets V4L2_CAP_VIDEO_CAPTURE (0x1).  Filtering on a
// substring match of "Video Capture" instead — as this used to — matched
// the Pi's internal m2m nodes (rpi-hevc-dec's "Format Video Capture
// Multiplanar:" heading, and pispbe's CAPTURE_MPLANE nodes), so the
// dropdown was full of decoder/ISP devices that can never produce a
// picture, with no way for the user to tell which entry was their camera.
function GetV4L2Devices()
{
    $devices = array();

    // Enumerate /dev/video* devices
    $videoDevs = glob('/dev/video*');
    if ($videoDevs === false) {
        return json(array("devices" => array()));
    }

    // V4L2 capability bits (linux/videodev2.h)
    $V4L2_CAP_VIDEO_CAPTURE = 0x00000001;
    $V4L2_CAP_VIDEO_OUTPUT = 0x00000002;
    $V4L2_CAP_VIDEO_M2M = 0x00008000;

    foreach ($videoDevs as $devPath) {
        // Use v4l2-ctl to get device capabilities
        $output = array();
        $ret = 0;
        exec("v4l2-ctl -d " . escapeshellarg($devPath) . " --all 2>/dev/null", $output, $ret);
        if ($ret !== 0)
            continue;

        $info = implode("\n", $output);

        // Prefer "Device Caps" (what this node can do) over "Capabilities"
        // (what the whole physical device can do across all its nodes).
        if (!preg_match('/Device Caps\s*:\s*0x([0-9a-fA-F]+)/', $info, $m) &&
            !preg_match('/Capabilities\s*:\s*0x([0-9a-fA-F]+)/', $info, $m)) {
            continue;
        }
        $caps = hexdec($m[1]);

        // Must be a single-planar capture node, and must not be a
        // memory-to-memory (decoder/encoder/ISP) or output node.
        if (!($caps & $V4L2_CAP_VIDEO_CAPTURE))
            continue;
        if ($caps & ($V4L2_CAP_VIDEO_OUTPUT | $V4L2_CAP_VIDEO_M2M))
            continue;

        // Extract device name
        $name = $devPath;
        if (preg_match('/Card type\s*:\s*(.+)/', $info, $m)) {
            $name = trim($m[1]);
        }

        // Bus info distinguishes two identical cameras from each other.
        $bus = '';
        if (preg_match('/Bus info\s*:\s*(.+)/', $info, $m)) {
            $bus = trim($m[1]);
        }

        $devices[] = array(
            'device' => $devPath,
            'name' => $name,
            'busInfo' => $bus,
            'modes' => GetV4L2DeviceModes($devPath),
        );
    }

    return json(array("devices" => $devices));
}

/////////////////////////////////////////////////////////////////////////////
// Return the discrete capture modes a V4L2 device actually supports, as
// [ {format, width, height, framerates[]}, ... ].
//
// The UI uses this to offer real resolutions instead of free-text boxes:
// a webcam only negotiates its native sizes, so an arbitrary value like
// 240x135 @ 10fps fails to start with nothing but "failed to start" in
// the log.
function GetV4L2DeviceModes($devPath)
{
    $modes = array();
    $output = array();
    $ret = 0;
    exec("v4l2-ctl -d " . escapeshellarg($devPath) . " --list-formats-ext 2>/dev/null", $output, $ret);
    if ($ret !== 0)
        return $modes;

    $curFormat = '';
    $curSize = null;
    foreach ($output as $line) {
        if (preg_match("/\[\d+\]:\s*'(\w+)'/", $line, $m)) {
            $curFormat = $m[1];
            $curSize = null;
        } else if (preg_match('/Size:\s*Discrete\s*(\d+)x(\d+)/', $line, $m)) {
            if ($curSize !== null)
                $modes[] = $curSize;
            $curSize = array(
                'format' => $curFormat,
                'width' => intval($m[1]),
                'height' => intval($m[2]),
                'framerates' => array(),
            );
        } else if ($curSize !== null &&
                   preg_match('/Interval:\s*Discrete\s*[\d.]+s\s*\(([\d.]+)\s*fps\)/', $line, $m)) {
            $fps = intval(round(floatval($m[1])));
            if ($fps > 0 && !in_array($fps, $curSize['framerates']))
                $curSize['framerates'][] = $fps;
        }
    }
    if ($curSize !== null)
        $modes[] = $curSize;

    return $modes;
}

/////////////////////////////////////////////////////////////////////////////
// GET /api/pipewire/video/input-sources/:id/preview
// Returns a JPEG snapshot of a video input source, for the config page's
// live preview.  Two paths, because a capture device can only be opened once:
//   - source running in fppd -> ask fppd, which taps its intervideo channel
//   - source stopped/disabled -> grab straight off the device ourselves, so
//     the operator can confirm they picked the right camera before enabling it
function GetVideoInputPreview()
{
    global $settings;

    $id = intval(params('id'));
    $width = isset($_GET['width']) ? intval($_GET['width']) : 320;
    if ($width < 32)
        $width = 32;
    if ($width > 1280)
        $width = 1280;

    // Try fppd first — works whenever the source is actually running.
    $ctx = stream_context_create(array('http' => array('timeout' => 4)));
    $jpeg = @file_get_contents('http://localhost:32322/videoinput/preview?id=' . $id .
                               '&width=' . $width, false, $ctx);
    if ($jpeg !== false && strlen($jpeg) > 2 && substr($jpeg, 0, 2) === "\xFF\xD8") {
        header('Content-Type: image/jpeg');
        header('Cache-Control: no-store');
        echo $jpeg;
        exit(0);
    }

    // Source isn't running — fall back to opening the device directly.
    $sourcesFile = $settings['mediaDirectory'] . "/config/pipewire-video-input-sources.json";
    $device = '';
    $srcType = '';
    if (file_exists($sourcesFile)) {
        $data = json_decode(file_get_contents($sourcesFile), true);
        if (is_array($data) && isset($data['videoInputSources'])) {
            foreach ($data['videoInputSources'] as $src) {
                if (intval($src['id']) === $id) {
                    $srcType = isset($src['type']) ? $src['type'] : '';
                    $device = isset($src['device']) ? $src['device'] : '';
                    break;
                }
            }
        }
    }

    // Only V4L2 devices are grabbable without fppd; network sources would
    // mean opening a second RTSP/HTTP session, which is not what a preview
    // button should quietly do.
    if ($srcType !== 'v4l2src' || $device === '' || !preg_match('#^/dev/video\d+$#', $device)) {
        header('HTTP/1.1 503 Service Unavailable');
        header('Content-Type: application/json');
        echo json_encode(array('status' => 'error',
                               'message' => 'No preview available. Enable and save the source, then retry.'));
        exit(0);
    }

    $tmp = tempnam('/tmp', 'fppvidprev') . '.jpg';
    // decodebin + videoscale for the same reason the capture pipeline needs
    // them: cameras hand back MJPEG or a native size we didn't ask for.
    // num-buffers=8 discards the first few frames, which are often black or
    // mid-auto-exposure on a freshly opened webcam.
    $cmd = 'timeout 8 gst-launch-1.0 -q'
         . ' v4l2src device=' . escapeshellarg($device) . ' num-buffers=8'
         . ' ! decodebin ! videoconvert ! videoscale'
         . ' ! video/x-raw,width=' . $width . ',pixel-aspect-ratio=1/1'
         . ' ! jpegenc quality=70'
         . ' ! multifilesink location=' . escapeshellarg($tmp) . ' 2>/dev/null';
    exec($cmd, $out, $ret);

    if (file_exists($tmp) && filesize($tmp) > 0) {
        header('Content-Type: image/jpeg');
        header('Cache-Control: no-store');
        readfile($tmp);
        unlink($tmp);
        exit(0);
    }
    if (file_exists($tmp))
        unlink($tmp);

    header('HTTP/1.1 503 Service Unavailable');
    header('Content-Type: application/json');
    echo json_encode(array('status' => 'error',
                           'message' => 'Could not read a frame from ' . $device .
                                        '. Check the device is connected and not in use.'));
    exit(0);
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
