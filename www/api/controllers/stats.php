<?php

/**
 * Generates the statistics payload by calling all registered stat collector
 * functions and writing the result as a `JSON` file at the given path.
 *
 * @param string $statsFile Absolute path to write the generated stats JSON.
 * @return void
 */
function stats_generate($statsFile)
{
    //////////// MAIN ////////////
    $tasks = array(
        "uuid" => 'stats_getUUID',
        "uuidSource" => 'stats_getUUIDSource',
        "systemInfo" => 'stats_getSystemInfo',
        "capeInfo" => 'stats_getCapeInfo',
        "outputProcessors" => 'stats_getOutputProcessors',
        "files" => 'stats_getFiles',
        "models" => 'stats_getModels',
        "multisync" => 'stats_getMultiSync',
        "multisyncShape" => 'stats_getMultiSyncShape',
        "plugins" => 'stats_getPlugins',
        "schedule" => 'stats_getSchedule',
        "settings" => 'stats_getSettings',
        "network" => 'stats_network',
        "memory" => 'stats_memory',
        "universe_input" => 'stats_universe_in',
        "output_e131" => 'stats_universe_out',
        "output_panel" => 'stats_panel_out',
        "output_other" => 'stats_other_out',
        "output_pixel_pi" => 'stats_pixel_pi_out',
        "output_pixel_bbb" => 'stats_pixel_bbb_out',
        "output_pwm" => 'stats_pwm_out',
        "timezone" => 'stats_timezone',
        "sequenceShape" => 'stats_getSequenceShape',
        "installAge" => 'stats_getInstallAge',
    );

    $obj = array();
    foreach ($tasks as $key => $fun) {
        try {
            $obj[$key] = call_user_func($fun);
        } catch (Throwable $e) {
            // Never echo from inside an API request: it writes straight into the
            // HTTP response body, so one failing collector puts a bare string
            // ahead of the JSON and breaks parsing for every caller.  Throwable
            // rather than Exception so a TypeError in one collector costs that
            // collector only, instead of the whole payload.
            error_log("stats_generate: collector '" . $key . "' failed: " . $e->getMessage());
        }
    }
    if (file_exists($statsFile)) {
        unlink($statsFile);
    }

    $data = json_encode($obj, JSON_PRETTY_PRINT);
    file_put_contents($statsFile, $data);
}

/**
 * Get statistics
 *
 * Returns the statistics file that will be shared with the development team
 * if sharing statistics is enabled. A cached file is returned unless it is
 * more than 2 hours old or `?force=1` is passed, in which case it is regenerated.
 *
 * @route GET /api/statistics/usage
 * @param int force bypass cache
 * @response 200 Usage statistics payload
 * ```json
 * {
 *   "uuid": "6ba176e7-da7f-49f4-8b27-edb5bd9ff616",
 *   "systemInfo": {
 *     "mqtt": {"configured": true, "connected": true},
 *     "fppdStatus": "running",
 *     "fppdMode": "player",
 *     "fppdUptimeSeconds": 3436,
 *     "platform": "Debian",
 *     "version": "4.x-master-914-gebda8520",
 *     "majorVersion": 4,
 *     "minorVersion": 1000,
 *     "typeId": 1,
 *     "branch": "master",
 *     "utilization": {"CPU": 2.2, "Memory": 15.9, "Uptime": "7 days"}
 *   },
 *   "capeInfo": {"type": "None"},
 *   "files": {"sequences": {"cnt": 2, "bytes": 19025632}},
 *   "models": {"count": 0}
 * }
 * ```
 */
function stats_get_last_file()
{
    global $_GET;
    $statsFile = stats_get_filename();
    $reason = "unknown";
    if (isset($_GET["reason"])) {
        $reason = $_GET["reason"];
    }

    if (file_exists($statsFile)) {
        // No reason to regenereate if less than 2 hours old
        if (time() - filemtime($statsFile) > 2 * 3600) {
            stats_generate($statsFile);
        } else if (isset($_GET['force']) && $_GET['force'] == 1) {
            stats_generate($statsFile);
        }
    } else {
        stats_generate($statsFile);
    }

    $obj = json_decode(file_get_contents($statsFile), true);
    $obj["statsReason"] = $reason;
    return json($obj, JSON_PRETTY_PRINT);
}

/**
 * Collects network statistics including GitHub reachability, Wi-Fi signal
 * strength, and the operational state of each network interface.
 *
 * @return array Associative array with github_access, wifi, and interfaces keys.
 */
function stats_network()
{
    $rc = array();

    // Statistics generation makes no outbound request.  This used to shell out to
    // `curl https://github.com/...` for a `github_access` boolean, which meant the
    // Preview button -- the one place a cautious user checks what would be sent
    // before agreeing to send anything -- itself contacted a third party, on a box
    // that may have statistics disabled entirely.  If an "can this box reach the
    // internet" metric is wanted, it has to come from the update check, which
    // already makes that request for its own reasons, not from here.
    $rc['wifi'] = json_decode(file_get_contents("http://localhost/api/network/wifi/strength"), true);

    $interfaces = json_decode(file_get_contents("http://localhost/api/network/interface"), true);
    $anyV4 = false;
    $anyV6 = false;
    foreach ($interfaces as $i) {
        $name = $i['ifname'];
        if (isset($i['operstate'])) {
            $rc['interfaces'][$name]['operstate'] = $i['operstate'];
        }

        // Which address families this interface actually carries.  Counted by
        // SCOPE, not by family: an IPv6 link-local address (fe80::/10) is
        // autoconfigured on every IPv6-capable interface whether or not the
        // network carries any IPv6 at all, so "has an inet6 address" would read
        // as near-100% dual-stack everywhere and measure nothing.  Only global
        // and unique-local addresses say anything about deployment.
        //
        // No address is transmitted -- these are three booleans and an enum.
        if (isset($i['addr_info']) && is_array($i['addr_info'])) {
            $hasV4 = false;
            $v6Scope = 'none';
            foreach ($i['addr_info'] as $a) {
                $family = isset($a['family']) ? $a['family'] : '';
                $scope = isset($a['scope']) ? $a['scope'] : '';
                if ($family === 'inet') {
                    if ($scope === 'global') {
                        $hasV4 = true;
                    }
                } else if ($family === 'inet6') {
                    if ($scope === 'global') {
                        // Distinguish a routable address from a ULA; both count
                        // as deployed, but they are different deployments.
                        $local = isset($a['local']) ? strtolower($a['local']) : '';
                        $isUla = (strncmp($local, 'fc', 2) === 0 || strncmp($local, 'fd', 2) === 0);
                        if ($isUla) {
                            if ($v6Scope !== 'global') {
                                $v6Scope = 'ula';
                            }
                        } else {
                            $v6Scope = 'global';
                        }
                    } else if ($scope === 'link' && $v6Scope === 'none') {
                        $v6Scope = 'linklocal';
                    }
                }
            }
            $rc['interfaces'][$name]['ipv4'] = $hasV4;
            $rc['interfaces'][$name]['ipv6'] = $v6Scope;
            $anyV4 = $anyV4 || $hasV4;
            $anyV6 = $anyV6 || ($v6Scope === 'global' || $v6Scope === 'ula');
        }
        // This tested $rc -- the array being built -- rather than $i, so it could
        // never be true and has emitted nothing since it was written.
        //
        // It is deliberately not switched on as-written.  The unset() list it
        // carried covers PSK and SSID but not BACKUPPSK and BACKUPSSID, which are
        // a wifi passphrase and a network name, so simply fixing the condition
        // would have started uploading credentials.  A denylist fails open every
        // time a new key is added to the interface config; an allowlist fails
        // closed, which is the only safe default for a payload that leaves the
        // device.  Nothing here identifies a household or a network.
        if (isset($i['config']) && is_array($i['config'])) {
            // Driven by www/interface-settings.json, which describes the
            // interface config fields the same way settings.json describes the
            // rest of FPP: a field is disclosed only where it declares
            // "gatherStats": true.  scripts/generate_crash_report reads the same
            // file, so the statistics payload and a crash report cannot drift on
            // what an interface config may reveal.  Falls back to the inline list
            // if the file is unreadable; both are allowlists, so either way a
            // field nobody has classified stays on the device.
            $allowed = array();
            $fieldsFile = $settings['wwwDir'] . "/interface-settings.json";
            if (is_readable($fieldsFile)) {
                $meta = json_decode(file_get_contents($fieldsFile), true);
                if (isset($meta['fields']) && is_array($meta['fields'])) {
                    foreach ($meta['fields'] as $fieldName => $fieldMeta) {
                        if (is_array($fieldMeta) && !empty($fieldMeta['gatherStats'])) {
                            $allowed[] = $fieldName;
                        }
                    }
                }
            }
            if (!count($allowed)) {
                $allowed = array(
                    'PROTO',            // dhcp vs static
                    'HIDDEN',
                    'WPA3',
                    'BACKUPHIDDEN',
                    'BACKUPWPA3',
                    'IPFORWARDING',
                    'DHCPSERVER',
                    'DHCPPOOLSIZE',
                    'DHCPOFFSET',
                    'ROUTEMETRIC',
                );
            }
            $config = array();
            foreach ($allowed as $key) {
                if (isset($i['config'][$key])) {
                    $config[$key] = $i['config'][$key];
                }
            }
            if (count($config)) {
                $rc['interfaces'][$name]['config'] = $config;
            }
        }
    }

    // Box-level rollup.  This is the IPv6 rollout number: it works on a
    // standalone player, which the peer-derived view in stats_getMultiSync()
    // cannot -- most shows have no discoverable peer at all.
    $rc['stack'] = $anyV4 ? ($anyV6 ? 'dual' : 'v4') : ($anyV6 ? 'v6' : 'none');

    return $rc;
}

/**
 * Collects memory usage statistics from `/proc/meminfo` (Linux) or
 * `memory_pressure` (macOS).
 *
 * @return array Memory stats including MemTotal, MemFree, MemAvailable,
 *               Active, Inactive, and Cached (in kB), plus meminfoAvailable flag.
 */
function stats_memory()
{
    global $settings;
    $rc = array('meminfoAvailable' => false);
    if (file_exists("/proc/meminfo")) {
        $interesting = array('MemTotal', 'MemFree', 'MemAvailable', 'Active', 'Inactive', 'Cached');
        $output = array();
        exec("cat /proc/meminfo", $output, $exitCode);

        if ($exitCode == 0) {
            $rc['meminfoAvailable'] = true;
            $key = 'unknown';
            $value = 0;
            foreach ($output as $row) {
                $matches = array();
                if (preg_match("/^(.*):/", $row, $matches) == 1) {
                    $key = $matches[1];
                }

                if (preg_match("/\s+([0-9]*) kB/", $row, $matches) == 1) {
                    $value = $matches[1];
                }

                if (in_array($key, $interesting)) {
                    $rc[$key] = $value;
                }
            }
        }
    } else if ($settings["Platform"] == "MacOS") {
        $output = array();
        exec("memory_pressure", $output, $exitCode);
        if ($exitCode == 0) {
            $rc['meminfoAvailable'] = true;
            $key = 'unknown';
            $value = 0;
            $pageSize = 4096;
            $totalPages = 0;
            foreach ($output as $row) {
                $matches = array();
                if (preg_match("/([0-9]*) pages with a page size of ([0-9]*).*/", $row, $matches) == 1) {
                    $totalPages = intval($matches[1]);
                    $pageSize = intval($matches[2]);
                    $rc['MemTotal'] = strval($pageSize * $totalPages / 1024);
                } else if (preg_match("/^(.*): ([0-9]*)/", $row, $matches) == 1) {
                    $key = $matches[1];
                    $value = intval($matches[2]);

                    if ($key == "Pages active") {
                        $rc["Active"] = strval($pageSize * $value / 1024);
                    } else if ($key == "Pages inactive") {
                        $rc["Inactive"] = strval($pageSize * $value / 1024);
                    } else if ($key == "Pages purgeable") {
                        $rc["Cached"] = strval($pageSize * $value / 1024);
                    } else if ($key == "Pages free") {
                        $rc["MemFree"] = strval($pageSize * $value / 1024);
                        $rc['MemAvailable'] = strval($pageSize * $value / 1024);
                    }
                }
            }
        }
    }

    return $rc;
}

/**
 * Publsh statistics
 *
 * Transmits the statistics payload to the remote stats server configured in
 * the `statsPublishUrl` setting.
 *
 * @route POST /api/statistics/usage
 * @response 200 Statistics transmitted
 * ```json
 * {"status": "OK", "uuid": "M2-xxxxxxxx-f67f-930d-56ee-7xxxxxxxxxx"}
 * ```
 */
/**
 * The parts of a statistics payload that describe how a device is CONFIGURED,
 * as opposed to what it happens to be doing this second.
 *
 * This drives publish-on-change.  Everything excluded here is excluded because
 * it moves on its own: CPU load, uptime, sensor temperatures, Wi-Fi signal,
 * free memory, peer lastSeen timestamps.  If any of those were included the
 * signature would differ on every boot, every restart would publish again, and
 * nothing would report an error -- the fix would silently become the bug it
 * replaced.
 *
 * It is an allowlist rather than everything-minus-a-denylist because the two
 * fail in opposite directions.  A denylist that misses a new volatile field
 * fails open and restores publish-on-every-restart; an allowlist that misses a
 * new configuration field fails closed and delays that field's first report by
 * at most the periodic interval.
 *
 * @param array $obj full payload from stats_generate()
 * @return array the significant subset, canonically ordered
 */
function stats_significantSubset($obj)
{
    // Whole blocks that are configuration through and through.
    $wholeBlocks = array(
        'uuid', 'uuidSource', 'capeInfo', 'settings', 'plugins',
        'outputProcessors', 'schedule', 'timezone', 'installAge', 'models',
        'universe_input', 'output_panel', 'output_other',
        'output_pixel_pi', 'output_pixel_bbb', 'output_pwm',
    );
    // Named keys from blocks that mix configuration with live state.
    $partialBlocks = array(
        'systemInfo' => array(
            'platform', 'platformVariant', 'version', 'majorVersion',
            'minorVersion', 'branch', 'osVersion', 'osRelease', 'Kernel',
            'typeId', 'channelRanges', 'wifiInterfaceCount', 'fppdMode',
        ),
        // Deliberately not 'wifi' (signal levels move constantly).  The
        // interface block itself is filtered below -- it is a map, not a list.
        'network' => array('stack'),
    );

    $rc = array();
    foreach ($wholeBlocks as $key) {
        if (isset($obj[$key])) {
            $rc[$key] = $obj[$key];
        }
    }
    foreach ($partialBlocks as $block => $keys) {
        if (!isset($obj[$block]) || !is_array($obj[$block])) {
            continue;
        }
        foreach ($keys as $key) {
            if (isset($obj[$block][$key])) {
                $rc[$block][$key] = $obj[$block][$key];
            }
        }
    }

    // Per-interface: link state and address families are configuration; nothing
    // else in there is.
    if (isset($obj['network']['interfaces']) && is_array($obj['network']['interfaces'])) {
        foreach ($obj['network']['interfaces'] as $name => $iface) {
            foreach (array('operstate', 'ipv4', 'ipv6', 'config') as $key) {
                if (isset($iface[$key])) {
                    $rc['network']['interfaces'][$name][$key] = $iface[$key];
                }
            }
        }
    }

    // output_e131 minus `targets`.  The rest of the block is read straight from
    // co-universes.json, but `targets` splits destinations into discovered and
    // undiscovered, which depends on what answered discovery this boot -- the
    // same peer churn that keeps `multisync` out of here.
    if (isset($obj['output_e131']) && is_array($obj['output_e131'])) {
        foreach ($obj['output_e131'] as $key => $value) {
            if ($key !== 'targets') {
                $rc['output_e131'][$key] = $value;
            }
        }
    }

    // Deliberately absent, and each for a stated reason:
    //   multisync, multisyncShape -- the peer set churns as other devices boot,
    //     and at the start of a season every box on a show would see it move.
    //   sequenceShape, files -- move whenever media is added, which is real but
    //     frequent; the periodic publish picks them up.
    //   memory, systemInfo.utilization/sensors/fppdUptimeSeconds, network.wifi
    //     -- live state, different every second.
    //   statsReason -- describes the upload, not the device.

    stats_ksortRecursive($rc);
    return $rc;
}

/**
 * Sorts keys at every level so an unchanged configuration always serialises
 * identically.  Without this a reordered map hashes differently and publishes
 * for no reason.
 */
function stats_ksortRecursive(&$arr)
{
    if (!is_array($arr)) {
        return;
    }
    foreach ($arr as &$v) {
        stats_ksortRecursive($v);
    }
    unset($v);
    // Only sort maps; reordering a list would change its meaning.
    if (count($arr) && count(array_filter(array_keys($arr), 'is_string'))) {
        ksort($arr);
    }
}

/**
 * Signature of the configuration described by a payload.
 *
 * @param array $obj full payload
 * @return string 64 hex characters
 */
function stats_significantHash($obj)
{
    return hash('sha256', json_encode(stats_significantSubset($obj)));
}

/**
 * Where the last publish's time and signature are remembered.
 */
function stats_publishStateFile()
{
    global $settings;
    return $settings['configDirectory'] . "/stats_publish_state.json";
}

function stats_readPublishState()
{
    $f = stats_publishStateFile();
    if (is_readable($f)) {
        $state = json_decode(file_get_contents($f), true);
        if (is_array($state)) {
            return $state;
        }
    }
    return array();
}

function stats_writePublishState($hash)
{
    $f = stats_publishStateFile();
    @file_put_contents($f, json_encode(array(
        "lastPublish" => time(),
        "hash" => $hash,
    )), LOCK_EX);
}

/**
 * How often a device publishes when nothing about it has changed.
 *
 * This is a floor, not a ceiling: a device that changes publishes sooner.  It
 * exists so that a box which never changes still reports in, because "this
 * device is still alive" is one of the things the statistics are for -- without
 * it a retired device and a stable one would look identical.
 *
 * fppd asks far more often than this (STATS_PUBLISH_ASK_HOURS, daily) and this
 * side decides.  The two must not be equal: fppd's counter resets on restart, so
 * if it matched this interval a box rebooting just before the interval elapsed
 * would be told "not yet", spend its guard, and go quiet for nearly two
 * intervals.
 */
define('STATS_PUBLISH_INTERVAL_DAYS', 7);

/**
 * Decides whether a publish is warranted, and publishes if so.
 *
 * fppd asks on start and then periodically.  Answering "yes" every time is what
 * made 91% of the stored corpus restart records -- a timestamped power-on log
 * per household that answers no question the project asks.  Answering "no" to
 * every restart would be worse in the other direction: a cape swap, an output
 * change or an upgrade would go unreported for a week while the stored record
 * said something untrue.
 *
 * So the trigger is content.  A restart publishes when the configuration
 * signature has moved, and stays quiet when it has not; a device that never
 * changes still checks in on the periodic interval.
 */
function stats_publish_stats_file()
{
    global $settings;
    global $_GET;

    // Consent gate.  This route is unauthenticated and CSRF-free by design, so
    // without this a cross-origin form POST from any page the user visits would
    // upload the payload after they had opted out.  The check used to live only
    // in fppd.
    if (!isset($settings['statsPublish']) || $settings['statsPublish'] !== 'Enabled') {
        return json(array("status" => "disabled"));
    }

    $force = isset($_GET['force']) && $_GET['force'];
    $jsonString = stats_get_last_file();
    $payload = json_decode($jsonString, true);
    $hash = is_array($payload) ? stats_significantHash($payload) : '';

    if (!$force) {
        $state = stats_readPublishState();
        $unchanged = ($hash !== '' && isset($state['hash']) && $state['hash'] === $hash);
        $lastPublish = isset($state['lastPublish']) ? intval($state['lastPublish']) : 0;
        $elapsed = time() - $lastPublish;
        $due = ($elapsed >= (STATS_PUBLISH_INTERVAL_DAYS * 24 * 60 * 60));

        // Publish when the configuration moved, OR when the interval elapsed.
        // The second half is the liveness beacon and is not optional: skipping
        // it would make a device that never changes indistinguishable from one
        // that was switched off for good.
        if ($unchanged && !$due) {
            return json(array(
                "status" => "skipped",
                "reason" => "configuration unchanged, next periodic publish in "
                    . max(0, (STATS_PUBLISH_INTERVAL_DAYS * 24 * 60 * 60) - $elapsed) . "s",
            ));
        }
    }

    $ch = curl_init($settings['statsPublishUrl']);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($ch, CURLOPT_HTTPHEADER, array('Content-Type:application/json'));
    curl_setopt($ch, CURLOPT_POSTFIELDS, $jsonString);
    curl_setopt($ch, CURLOPT_CONNECTTIMEOUT_MS, 800);
    curl_setopt($ch, CURLOPT_TIMEOUT_MS, 3000);
    // execute!
    $raw = curl_exec($ch);
    $httpCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
    $response = json_decode($raw);

    // close the connection, release resources used
    curl_close($ch);

    // Only remember it if it actually landed, or a failed upload would suppress
    // the retry for a week.
    if ($raw !== false && $httpCode >= 200 && $httpCode < 300 && $hash !== '') {
        stats_writePublishState($hash);
    }

    return json($response);
}

/**
 * Resets statistics cache
 *
 * Deletes the cached statistics file.
 *
 * @route DELETE /api/statistics/usage
 * @response 200 Statistics cache cleared
 * ```json
 * {"status": "OK"}
 * ```
 */
function stats_delete_last_file()
{
    $statsFile = stats_get_filename();
    if (file_exists($statsFile)) {
        unlink($statsFile);
    }
    return json(array("status" => "OK"));
}

/**
 * Returns the absolute path to the cached statistics file from `settings`.
 *
 * @return string Absolute path to the stats file.
 */
function stats_get_filename()
{
    global $settings;

    return $settings['statsFile'];
}

/**
 * Copies selected keys from an input array into an output array using a
 * key-name mapping. Only copies keys that exist in the input.
 *
 * @param array &$obj     Destination array to write into.
 * @param array &$input   Source array to read values from.
 * @param array &$mapping Map of output key => input key names.
 * @return void
 */
function validateAndAdd(&$obj, &$input, &$mapping)
{
    foreach ($mapping as $newKey => $oldKey) {
        if (isset($input[$oldKey])) {
            $obj[$newKey] = $input[$oldKey];
        }
    }
}

/**
 * Collects system information from the local `/api/system/status` endpoint,
 * including MQTT status, `fppd` mode, uptime, platform, version, and utilization.
 *
 * @return array System info key-value pairs.
 */
function stats_getSystemInfo()
{
    $rc = array();
    $data = json_decode(file_get_contents("http://localhost/api/system/status"), true);
    $mapping = array(
        "mqtt" => "MQTT",
        "fppdStatus" => "fppd",
        "fppdMode" => "mode_name",
        "sensors" => "sensors",
        "fppdUptimeSeconds" => "uptimeTotalSeconds",
    );
    validateAndAdd($rc, $data, $mapping);

    $rc["wifiInterfaceCount"] = count($data["wifi"]);

    if (isset($data["advancedView"])) {
        $mapping = array(
            "platform" => "Platform",
            "platformVariant" => "Variant",
            "version" => "Version",
            "majorVersion" => "majorVersion",
            "minorVersion" => "minorVersion",
            "typeId" => "typeId",
            "branch" => "Branch",
            "osVersion" => "OSVersion",
            "Kernel" => "Kernel",
            "osRelease" => "OSRelease",
            "channelRanges" => "channelRanges",
            "utilization" => "Utilization",
        );
        validateAndAdd($rc, $data['advancedView'], $mapping);

    }
    return $rc;
}

/**
 * Collects a summary of configured output processors, counting active and
 * total instances of each processor type.
 *
 * @return array Map of processor type => {activeCnt, totalCnt}.
 */
function stats_getOutputProcessors()
{
    $rc = array();
    $data = json_decode(file_get_contents("http://localhost/api/channel/output/processors"), true);
    if (isset($data['outputProcessors'])) {
        foreach ($data['outputProcessors'] as $obj) {
            $type = $obj['type'];
            if (!isset($rc[$type])) {
                $rc[$type] = array("activeCnt" => 0, "totalCnt" => 0);
            }
            $rc[$type]["totalCnt"] += 1;
            if ($obj['active'] === 1) {
                $rc[$type]["activeCnt"] += 1;
            }
        }
    }

    return $rc;
}

/**
 * Collects file counts and total byte sizes for sequences, effects, music,
 * and video media directories.
 *
 * @return array Map of media type => {cnt, bytes}.
 */
function stats_getFiles()
{
    $types = array("sequences", "effects", "music", "videos");
    $rc = array();
    foreach ($types as $type) {
        $data = json_decode(file_get_contents("http://localhost/api/files/$type"), true);
        if (isset($data['files'])) {
            $cnt = 0;
            $bytes = 0;
            foreach ($data['files'] as $file) {
                $cnt += 1;
                $bytes += $file["sizeBytes"];
            }
            $rc[$type] = array("cnt" => $cnt, "bytes" => $bytes);
        }
    }
    return $rc;
}

/**
 * Classifies a discovery address by family and SCOPE.
 *
 * Scope is the part that matters.  An IPv6 link-local address (fe80::/10) is
 * autoconfigured on every IPv6-capable interface whether or not the network
 * carries any IPv6, so counting "has an IPv6 address" would report near-100%
 * dual-stack everywhere.  Only global and unique-local addresses indicate a
 * deployment.  Nothing here is transmitted; only the derived counts are.
 *
 * @param string $addr peer address as discovery reported it
 * @return string one of ipv4, ipv4-linklocal, ipv6-global, ipv6-ula,
 *                ipv6-linklocal, ipv6-loopback, unknown
 */
function stats_addressClass($addr)
{
    $a = strtolower(trim((string) $addr));
    if ($a === '') {
        return 'unknown';
    }
    if (strpos($a, ':') === false) {
        return (strncmp($a, '169.254.', 8) === 0) ? 'ipv4-linklocal' : 'ipv4';
    }
    if (strncmp($a, 'fe80', 4) === 0) {
        return 'ipv6-linklocal';
    }
    if (strncmp($a, 'fc', 2) === 0 || strncmp($a, 'fd', 2) === 0) {
        return 'ipv6-ula';
    }
    if ($a === '::1' || $a === '::') {
        return 'ipv6-loopback';
    }
    return 'ipv6-global';
}

/**
 * Reduces a device's address classes to the stack it is actually running.
 *
 * @param array $classes values from stats_addressClass()
 * @return string v4, v6, dual or none
 */
function stats_stackOf($classes)
{
    $v4 = false;
    $v6 = false;
    foreach ($classes as $c) {
        if ($c === 'ipv4') {
            $v4 = true;
        } else if ($c === 'ipv6-global' || $c === 'ipv6-ula') {
            $v6 = true;
        }
    }
    if ($v4 && $v6) {
        return 'dual';
    }
    return $v6 ? 'v6' : ($v4 ? 'v4' : 'none');
}

/**
 * Derives a peer identifier for a device whose own UUID could not be read.
 *
 * Keyed on this host's UUID plus the peer address, so the same peer keeps the
 * same identifier across uploads from this show while being unguessable and
 * distinct from the identifier any other show would derive for the same
 * address.  The address itself is not recoverable from the result and is
 * never transmitted.
 *
 * @param string $ip peer address, used only as hash input
 * @return string 16 hex characters, prefixed to mark it as derived
 */
function localPeerIdentity($ip)
{
    return "X-" . substr(hash('sha256', getSystemUUID() . '|' . $ip), 0, 16);
}

/**
 * Turns MultiSync's "MAC:<address>" stand-in into the form the statistics
 * upload carries: "MH-" plus a truncated SHA-256 of the address, matching the
 * dash used by the other identities here ("X-", "M1-").
 *
 * The point is to keep hardware addresses out of the payload, not to anonymise
 * them -- with the model named alongside, the vendor prefix is known and the
 * remaining 24 bits fall to a few seconds of brute force.  Treat the result as
 * a hardware address that is merely inconvenient to read, and note that it is
 * reversible if that ever matters for a retention decision.
 *
 * What it does preserve is the property the raw value had: it is derived only
 * from the device, so every player reporting the same controller produces the
 * same token, which is what deduplication and network graphs depend on.
 */
function peerMacIdentity($macUuid)
{
    $mac = strtoupper(substr($macUuid, strlen('MAC:')));
    if ($mac === '') {
        return '';
    }
    return "MH-" . substr(hash('sha256', $mac), 0, 16);
}

/**
 * Fills in an identifier for every MultiSync system that did not announce a
 * usable one.
 *
 * Never writes a shared sentinel.  Earlier versions stored the literal strings
 * "Failed" and "Not Set" here, which collide across every install that emits
 * them and merge unrelated shows into one identity -- 21% of the peer entries in
 * the historical corpus are one of those two strings.
 *
 * @param array &$data MultiSync data array containing a "systems" key.
 * @return void
 */
function addMultiSyncUUID(&$data)
{
    if (!isset($data["systems"])) {
        return;
    }

    // This used to open an HTTP connection to every peer whose announced UUID
    // was missing or malformed -- /api/fppd/status for FPP peers,
    // /update/identity for Falcon controllers -- purely to fetch an identifier
    // for the statistics upload.  That probing is gone: generating statistics
    // should not put traffic on the LAN, and discovery already carries what is
    // needed.  A peer with no usable UUID is identified from its MAC, which is
    // stable across every player that can see it, and only falls back to a
    // per-reporter hash when there is no MAC either.
    foreach ($data["systems"] as &$system) {
        if (isset($system['uuid']) && isValidSystemUUID($system['uuid'])) {
            continue;
        }
        $prior = isset($system['uuid']) ? $system['uuid'] : '';
        // A "MAC:" value is not an identity the device chose, which is why
        // isValidSystemUUID() rejects it.  But it is the same value for a given
        // device no matter which player reports it, and that is exactly what
        // makes deduplicating a show possible.  localPeerIdentity() is salted
        // with the reporting host, so two players describing one controller
        // produce two unrelated rows -- use it only when there is no MAC.
        if (stripos($prior, 'MAC:') === 0) {
            $system['uuid'] = peerMacIdentity($prior);
        } else {
            $system['uuid'] = localPeerIdentity(isset($system['address']) ? $system['address'] : '');
        }
    }
    unset($system);
}

/**
 * Collects a sanitized list of MultiSync peer systems, filling in missing
 * UUIDs where possible.
 *
 * @return array Array of per-system records with version, type, and UUID info.
 */
function stats_multiSyncCollect($injected = null)
{
    static $cached = null;
    if ($injected === null && $cached !== null) {
        return $cached;
    }

    $mapping = array(
        "fppModeString" => "fppModeString",
        "channelRanges" => "channelRanges",
        "lastSeen" => "lastSeen",
        "version" => "version",
        "majorVersion" => "majorVersion",
        "minorVersion" => "minorVersion",
        "type" => "type",
        "typeId" => "typeId",
        "uuid" => "uuid",
        // Already on the wire, previously dropped by this table.
        "model" => "model",                                 // board revision detail "type" flattens away
        "local" => "local",                                 // marks the reporting host in its own list
        "multiSyncCapable" => "multiSyncCapable",           // true participant vs discovered-but-passive
        "channelOutputsEnabled" => "channelOutputsEnabled", // drives pixels, or player/spare only
        "channelInputsEnabled" => "channelInputsEnabled",   // bridge/receiver role
    );
    // Deliberately still excluded: address, hostname, HostDescription.

    // fppd now caches a subset of each remote's /api/system/info and /api/cape
    // on the peer record. Cape identity per peer is the single biggest gap in
    // the hardware picture: capeInfo used to be collected only for the
    // uploading device, so a cape in someone else's yard was just "BeagleBone
    // Black" unless that specific box opted in.
    //
    // Allowlists, not denylists. The peer record carries the remote's IP list
    // and its background colour, and will carry whatever is added to it next;
    // an allowlist is the only form that stays safe when the source grows.
    $systemInfoMapping = array(
        "platform" => "Platform",
        "variant" => "Variant",
        "subPlatform" => "SubPlatform",
        "osVersion" => "OSVersion",
        "osRelease" => "OSRelease",
        "kernel" => "Kernel",
        "branch" => "Branch",
    );
    // Excluded on purpose: IPs (addresses), HostDescription and backgroundColor
    // (user-entered/user-chosen, no analytical value), UpgradeSource (can be a
    // private mirror), Local/RemoteGitVersion (version already carries this).
    $capeInfoMapping = array(
        "id" => "id",
        "name" => "name",
        "version" => "version",
        "designer" => "designer",
    );
    // Excluded on purpose: description (long free text), vendor url/email/image
    // (contact details, no analytical value). The serial number is never
    // carried on the peer record at all.

    // $injected is for tests: the deduplication and address-family accounting
    // are the parts worth exercising, and they should not need a running fppd.
    $data = ($injected !== null)
        ? $injected
        : json_decode(file_get_contents("http://localhost/api/fppd/multiSyncSystems"), true);
    $peers = array();
    $shape = array("rows" => 0, "devices" => 0, "multiAddress" => 0,
        "addressesPerDevice" => array(), "stacks" => array());

    if (isset($data["systems"])) {
        addMultiSyncUUID($data);

        // Discovery reports one row per ADDRESS, not per device: a dual-stack
        // box appears once on IPv4 and once on IPv6, a multi-homed one once per
        // subnet, and the local host also on loopback.  Left alone that inflates
        // every peer count -- on a modest network the raw row count runs close
        // to twice the true device count.
        //
        // Collapse on uuid, and keep what the duplicate rows were saying:
        // addressCount, the per-family counts, and the resulting stack.  No
        // address is transmitted.  addressCount also lets a consumer recover the
        // pre-deduplication row count, so counts recorded before this change
        // stay comparable with counts recorded after it.
        $byUuid = array();
        $classes = array();
        foreach ($data["systems"] as $system) {
            $uuid = isset($system['uuid']) ? $system['uuid'] : '';
            $class = stats_addressClass(isset($system['address']) ? $system['address'] : '');
            $shape["rows"]++;

            if ($uuid === '') {
                // No identity at all: cannot be deduplicated, keep the row.
                $rec = array();
                validateAndAdd($rec, $system, $mapping);
                $peers[] = $rec;
                continue;
            }

            $classes[$uuid][] = $class;
            if (isset($byUuid[$uuid])) {
                continue;
            }

            $rec = array();
            validateAndAdd($rec, $system, $mapping);

            if (isset($system['systemInfo']) && is_array($system['systemInfo'])) {
                $info = array();
                validateAndAdd($info, $system['systemInfo'], $systemInfoMapping);
                if (count($info)) {
                    $rec['systemInfo'] = $info;
                }
            }

            if (isset($system['capeInfo']) && is_array($system['capeInfo'])) {
                $rec['capeInfo'] = stats_peerCapeRecord($system['capeInfo'], $capeInfoMapping);
            }

            $byUuid[$uuid] = count($peers);
            $peers[] = $rec;
        }

        foreach ($classes as $uuid => $seen) {
            $idx = $byUuid[$uuid];
            $peers[$idx]['addressCount'] = count($seen);
            $peers[$idx]['ipv4Count'] = count(array_filter($seen, function ($c) {
                return $c === 'ipv4';
            }));
            $peers[$idx]['ipv6Count'] = count(array_filter($seen, function ($c) {
                return $c === 'ipv6-global' || $c === 'ipv6-ula';
            }));
            $peers[$idx]['stack'] = stats_stackOf($seen);

            $n = (string) count($seen);
            if (!isset($shape["addressesPerDevice"][$n])) {
                $shape["addressesPerDevice"][$n] = 0;
            }
            $shape["addressesPerDevice"][$n]++;
            if (count($seen) > 1) {
                $shape["multiAddress"]++;
            }

            $stack = $peers[$idx]['stack'];
            if (!isset($shape["stacks"][$stack])) {
                $shape["stacks"][$stack] = 0;
            }
            $shape["stacks"][$stack]++;
        }
        $shape["devices"] = count($peers);
    }

    $result = array("peers" => $peers, "shape" => $shape);
    if ($injected === null) {
        $cached = $result;
    }
    return $result;
}

/**
 * Collects a sanitized list of MultiSync peer systems, one entry per device
 * rather than one per address.
 *
 * @return array Array of per-system records with version, type, UUID and the
 *               address-family counts the deduplication derived.
 */
function stats_getMultiSync()
{
    $collected = stats_multiSyncCollect();
    return $collected["peers"];
}

/**
 * Shape of the MultiSync view: how many rows collapsed into how many devices,
 * how many answered on more than one address, and the IPv4/IPv6 split.
 *
 * Counts only.  This is the peer-side view of IPv6 rollout; the box's own stack
 * is in network.stack, and that is the one that works on a standalone player.
 *
 * @return array
 */
function stats_getMultiSyncShape()
{
    $collected = stats_multiSyncCollect();
    return $collected["shape"];
}

/**
 * Collects schedule statistics including whether the scheduler is enabled and
 * a count of active entries broken down by type.
 *
 * @return array Schedule stats with enabled flag and types map.
 */
function stats_getSchedule()
{
    $data = json_decode(file_get_contents("http://localhost/api/fppd/schedule"), true);
    $rc = array();
    if (isset($data["schedule"])) {
        $rc["enabled"] = $data["schedule"]["enabled"];
        $types = array();
        if (isset($data["schedule"]["entries"])) {
            foreach ($data["schedule"]["entries"] as $rec) {
                $type = $rec['type'];
                if (!isset($types[$type])) {
                    $types[$type] = 0;
                }
                if (isset($rec["enabled"]) && $rec['enabled'] == 1) {
                    $types[$type] += 1;
                }
            }
            $rc["types"] = $types;
        }
    }

    return $rc;
}

/**
 * Returns the total count of overlay models configured on the device.
 *
 * @return array Array with a single "count" key.
 */
function stats_getModels()
{
    $raw = fetch_api_with_limit("http://localhost/api/models");
    $data = json_decode($raw, true);
    $rc = array("count" => 0);
    if (is_array($data)) {
        $rc["count"] = count($data);
    }

    return $rc;
}

/**
 * Collects the git commit hash and date for each installed plugin.
 *
 * @return array Map of plugin name => {hash, commitDate}.
 */
function stats_getPlugins()
{
    global $settings;
    $data = json_decode(file_get_contents("http://localhost/api/plugin"), true);
    $rc = array();
    if (is_array($data)) {
        foreach ($data as $plugin) {
            $output = '';
            $cmd = '(cd ' . $settings['pluginDirectory'] . '/' . $plugin . ' && git log -1 --format="%H^%cd")';
            exec($cmd, $output);
            $parts = explode("^", $output[0]);
            $rc[$plugin] = array("hash" => $parts[0], "commitDate" => $parts[1]);
        }
    }
    return $rc;

}

/**
 * Collects the SHAPE of the sequences and playlists on the device -- how they
 * are timed and how big they are -- rather than anything about what they are.
 *
 * files.sequences gives a count and a byte total, which says nothing about
 * whether the fleet has moved to 20ms timing or how large a typical sequence
 * has become.  Everything here comes from the fseq header, so no sequence
 * content is read, and only numbers are kept -- never a name.
 *
 * @return array Histograms of step time, channel count, fseq version and
 *               compression, plus a playlist length histogram.
 */
function stats_getSequenceShape()
{
    $rc = array("sequences" => 0, "read" => 0);
    $raw = @file_get_contents("http://localhost/api/sequence");
    if ($raw === false) {
        return $rc;
    }
    $names = json_decode($raw, true);
    if (!is_array($names)) {
        return $rc;
    }
    $rc['sequences'] = count($names);

    // One header read per sequence.  Capped so a library of thousands cannot
    // stall stats generation, and the cap is reported rather than silently
    // truncating -- a partial sample that looks complete is worse than no
    // sample at all.
    $limit = 250;
    if (count($names) > $limit) {
        $rc['truncated'] = true;
        $names = array_slice($names, 0, $limit);
    }

    $stepTime = array();
    $channels = array();
    $version = array();
    $compression = array();
    $frames = 0;
    foreach ($names as $name) {
        $meta = @file_get_contents("http://localhost/api/sequence/" . rawurlencode($name) . "/meta");
        if ($meta === false) {
            continue;
        }
        $m = json_decode($meta, true);
        if (!is_array($m) || !isset($m['StepTime'])) {
            continue;
        }
        $rc['read'] += 1;

        $key = "ms_" . strval($m['StepTime']);
        $stepTime[$key] = isset($stepTime[$key]) ? $stepTime[$key] + 1 : 1;

        if (isset($m['ChannelCount'])) {
            $c = $m['ChannelCount'];
            $bucket = "ch_" . ($c <= 1024 ? "0-1k" : ($c <= 8192 ? "1k-8k" : ($c <= 32768 ? "8k-32k" : ($c <= 131072 ? "32k-128k" : ($c <= 524288 ? "128k-512k" : "512k+")))));
            $channels[$bucket] = isset($channels[$bucket]) ? $channels[$bucket] + 1 : 1;
        }
        if (isset($m['Version'])) {
            $v = strval($m['Version']);
            $version[$v] = isset($version[$v]) ? $version[$v] + 1 : 1;
        }
        if (isset($m['CompressionType'])) {
            $ct = "type_" . strval($m['CompressionType']);
            $compression[$ct] = isset($compression[$ct]) ? $compression[$ct] + 1 : 1;
        }
        if (isset($m['NumFrames'])) {
            $frames += $m['NumFrames'];
        }
    }
    $rc['stepTime'] = $stepTime;
    $rc['channelCountBuckets'] = $channels;
    $rc['fseqVersion'] = $version;
    $rc['compression'] = $compression;
    $rc['totalFrames'] = $frames;

    // Playlist lengths, read straight off disk rather than over HTTP.
    // $playlistDirectory is a global from www/config.php, not a $settings key.
    global $playlistDirectory;
    global $settings;
    $dir = $playlistDirectory;
    if (empty($dir) && isset($settings['mediaDirectory'])) {
        $dir = $settings['mediaDirectory'] . "/playlists";
    }
    $lengths = array();
    $playlistCount = 0;
    if (!empty($dir) && is_dir($dir)) {
        foreach (glob($dir . "/*.json") as $file) {
            $pl = json_decode(file_get_contents($file), true);
            if (!is_array($pl)) {
                continue;
            }
            $playlistCount += 1;
            $n = 0;
            foreach (array("leadIn", "mainPlaylist", "leadOut") as $section) {
                if (isset($pl[$section]) && is_array($pl[$section])) {
                    $n += count($pl[$section]);
                }
            }
            $bucket = "len_" . ($n == 0 ? "0" : ($n <= 5 ? "1-5" : ($n <= 20 ? "6-20" : ($n <= 50 ? "21-50" : "51+"))));
            $lengths[$bucket] = isset($lengths[$bucket]) ? $lengths[$bucket] + 1 : 1;
        }
    }
    $rc['playlists'] = $playlistCount;
    $rc['playlistLengthBuckets'] = $lengths;

    return $rc;
}

/**
 * Reports how long this install has existed, so retention and upgrade-adoption
 * questions are answerable at all.  Nothing in the payload distinguishes a box
 * set up last week from one that has run for five years.
 *
 * Rounded to the month on purpose: a day-precision install date combined with a
 * timezone is a re-identification handle, and no question worth asking here
 * needs better resolution than a month.
 *
 * @return array Install month as YYYY-MM, and whole months since.
 */
function stats_getInstallAge()
{
    $rc = array();
    // The identity file is written once, when the install first resolves a UUID,
    // and is preserved across OS upgrades -- so its birth time is the closest
    // thing to an install date that already exists.
    $candidates = array("/etc/fpp/fpp_uuid", "/home/fpp/media/config/fpp_uuid");
    $oldest = 0;
    foreach ($candidates as $file) {
        if (file_exists($file)) {
            $t = @filemtime($file);
            if ($t !== false && ($oldest == 0 || $t < $oldest)) {
                $oldest = $t;
            }
        }
    }
    if ($oldest != 0) {
        $rc['installMonth'] = gmdate("Y-m", $oldest);
        $rc['ageMonths'] = (int)floor((time() - $oldest) / (30.44 * 86400));
        $rc['source'] = "uuid-file";
    }

    // Reported separately and never as an install date.  The root filesystem's
    // birth time is when the card was written, which for a prebuilt image is the
    // date the IMAGE was built -- identical across everyone who flashed it.  It
    // answers "which image vintage is this" rather than "how old is this
    // install", and conflating the two would make the fleet look far younger or
    // older than it is depending on how the image was produced.
    $out = array();
    exec("stat -c %W / 2>/dev/null", $out);
    if (isset($out[0]) && is_numeric($out[0]) && $out[0] > 0) {
        $rc['rootfsMonth'] = gmdate("Y-m", (int)$out[0]);
    }

    // Most installs derive identity from a hardware serial and so never create
    // the uuid file, which is why installMonth is absent for them.  A dedicated
    // first-boot marker is what would make this answerable fleet-wide.
    return $rc;
}

/**
 * Returns the system UUID.
 *
 * @return string UUID string.
 */
function stats_getUUID()
{
    return getSystemUUID();
}

/**
 * Returns which method produced the system UUID, so the collector can weight
 * or exclude records by identity quality.
 *
 * @return string source token
 */
function stats_getUUIDSource()
{
    return getSystemUUIDSource();
}

/**
 * Collects cape hardware information.
 *
 * The serial number and `cs` are never included, and this is no longer gated on
 * SendVendorSerial -- that setting governs what goes to the cape vendor from the
 * browser, which is what its description says.
 *
 * @return array Cape info with type, id, name, designer, verifiedKeyId and the
 *               vendor name.
 */
function stats_getCapeInfo()
{
    // The cape serial and `cs` are not sent.  Together they are the per-unit key
    // into a vendor's or the shop's order records, and no handler on the
    // statistics server reads either -- so they were a join key to a purchase,
    // collected for nothing.  They are also not what SendVendorSerial is about:
    // that switch says "send cape serial numbers to vendors", and it used to
    // silently govern this payload too.  It no longer does.
    //
    // verifiedKeyId stays.  It is the signing key id, one of a handful of values
    // fixed by the compiled-in key table in CapeUtils, and removed outright when
    // a signature does not verify -- so an EEPROM cannot put an arbitrary string
    // here.  It is coarser than vendor.name, which is kept, and it is the only
    // field that shows a cape signed by a key that does not match its claimed
    // vendor, which is what licence-abuse detection needs.
    $mapping = array(
        "type" => "type",
        "id" => "id",
        "name" => "name",
        "designer" => "designer",
        "verifiedKeyId" => "verifiedKeyId",
        "vendor" => "vendor"
    );

    $rc = array("name" => "None");
    $data = json_decode(file_get_contents("http://localhost/api/cape"), true);
    if (($data != false) && ((!isset($data['sendStats'])) || ($data['sendStats'] == 1))) {
        validateAndAdd($rc, $data, $mapping);
        // Reduce the vendor block to the name, matching what peer cape records
        // already carry.  The e-mail, logo URL and site add nothing to a chart,
        // and across the corpus that block is where sole traders' personal names
        // and one street address live.
        if (isset($rc['vendor']) && is_array($rc['vendor'])) {
            $rc['vendor'] = isset($rc['vendor']['name']) ? $rc['vendor']['name'] : '';
        }
    }

    return $rc;
}

/**
 * Collects the subset of FPP settings that have `gatherStats` enabled in
 * `settings.json` metadata.
 *
 * @return array Map of setting name => value for stats-eligible settings.
 */
function stats_getSettings()
{
    global $settings;
    global $settingsFile;
    $rc = array();
    $safeSettings = array();
    $allSettings = json_decode(file_get_contents($settings['wwwDir'] . "/settings.json"), true);
    foreach ($allSettings['settings'] as $name => $config) {
        if (!isset($config['gatherStats']) || !$config['gatherStats']) {
            continue;
        }
        // Declared sensitivity wins over gatherStats.  A setting marked
        // "pii": true or "type": "password" never leaves the device, whatever
        // else it says -- the same declarations scripts/generate_crash_report
        // redacts on, so the two cannot drift on what counts as personal data.
        //
        // This deliberately does NOT extend to device names such as AudioOutput
        // or ForceAudioId.  Those are hardware identifiers and the specific
        // string is the point: it says which kernel module an image needs and
        // which USB device to support.  Their problem was that the server
        // republished raw values on a public endpoint, which is fixed there, not
        // by withholding them here.
        if (!empty($config['pii']) || (isset($config['type']) && $config['type'] === 'password')) {
            continue;
        }
        $safeSettings[$name] = $name;
    }

    $fd = @fopen($settingsFile, "c+");
    flock($fd, LOCK_SH);
    $tmpSettings = parse_ini_file($settingsFile);
    flock($fd, LOCK_UN);
    fclose($fd);
    validateAndAdd($rc, $tmpSettings, $safeSettings);

    return $rc;
}

/**
 * Collects E1.31/ArtNet universe input statistics from the channel inputs
 * configuration file, counting active rows, universes, and channels by type.
 *
 * @return array Universe input stats including universeCount, rowCount, channelCount, and rowType.
 */
/**
 * True for the universe-output types that address a flat channel range rather
 * than a run of universes: DDP (4 and 5) and Twinkly (8).
 *
 * fppd reads channelCount alone for these -- DDP.cpp and Twinkly.cpp never look
 * at universeCount -- but the UI disables the universe-count input for exactly
 * these types while still saving whatever value was left sitting in the
 * disabled field.  The stored universeCount is therefore a leftover: 1 on some
 * installs, a stale count on others.
 *
 * @param int $type Row type as stored in the config.
 * @return bool True when channelCount alone is the row's channel span.
 */
function stats_universeRowIsFlat($type)
{
    return $type == 4 || $type == 5 || $type == 8;
}

/**
 * Channel span of one universe row.
 *
 * Multiplying by universeCount unconditionally made the span wrong for DDP and
 * Twinkly rows in a way that differed per install rather than uniformly -- zero
 * where the leftover was zero, correct where it happened to be one, inflated
 * otherwise.  A uniform error can be corrected for in aggregate; this one could
 * not, so it is fixed at the source rather than annotated.
 *
 * @param array $row One entry from a universes list.
 * @return int Channels covered by the row.
 */
function stats_universeRowChannels($row)
{
    $channels = isset($row["channelCount"]) ? intval($row["channelCount"]) : 0;
    if (stats_universeRowIsFlat(isset($row["type"]) ? $row["type"] : -1)) {
        return $channels;
    }
    return $channels * stats_universeRowUniverses($row);
}

/**
 * Universes covered by one row.  A flat row has none -- the count stored on it
 * is the leftover described above -- so it contributes zero rather than noise.
 * rowType already reports how many rows of each type there were.
 *
 * @param array $row One entry from a universes list.
 * @return int Universes covered by the row.
 */
function stats_universeRowUniverses($row)
{
    if (stats_universeRowIsFlat(isset($row["type"]) ? $row["type"] : -1)) {
        return 0;
    }
    $count = isset($row["universeCount"]) ? intval($row["universeCount"]) : 1;
    return $count < 1 ? 1 : $count;
}

function stats_universe_in()
{
    global $settings;
    $rc = array("file" => $settings['universeInputs']);
    if (!file_exists($settings['universeInputs'])) {
        return $rc;
    }

    $data = json_decode(file_get_contents($settings['universeInputs']), true);
    if (!isset($data["channelInputs"])) {
        return $rc;
    }
    $data = $data["channelInputs"][0];
    $rc['enabled'] = 0;
    if (isset($data['enabled'])) {
        $rc['enabled'] = $data['enabled'];
    }

    $universeCount = 0;
    $rowCount = 0;
    $activeRowCount = 0;
    $channelCount = 0;
    $rowType = array();
    if (isset($data["universes"])) {
        foreach ($data["universes"] as $row) {
            ++$rowCount;
            if (isset($row["active"]) && $row["active"] == 1) {
                ++$activeRowCount;
                $universeCount += stats_universeRowUniverses($row);
                $channelCount += stats_universeRowChannels($row);
                $type = "type_" . strval(isset($row['type']) ? $row['type'] : "unknown");
                if (!isset($rowType[$type])) {
                    $rowType[$type] = 0;
                }
                $rowType[$type] += 1;
            }
        }
    }
    $rc['universeCount'] = $universeCount;
    $rc['rowCount'] = $rowCount;
    $rc['activeRowCount'] = $activeRowCount;
    $rc['channelCount'] = $channelCount;
    $rc['rowType'] = $rowType;
    // universeCount and channelCount changed meaning for DDP/Twinkly rows here;
    // see stats_universeRowChannels.  Bump on any further change to how these
    // are derived so a consumer can tell a corrected payload from an old one
    // without inferring it from the FPP version.
    $rc['countsVersion'] = 2;

    return $rc;
}

/**
 * Reduces a peer's cape record to the allowlisted, non-identifying fields.
 *
 * Honours the flag of the cape being described rather than the flag of the host
 * doing the reporting: a cape that opted out of hardware detail is reported as
 * present and nothing more, so it still counts toward "how many devices have a
 * cape" without being identified.
 *
 * @param array $cape capeInfo as relayed on the peer record.
 * @param array $mapping allowlist of cape fields to copy.
 * @return array Allowlisted cape record.
 */
function stats_peerCapeRecord($cape, $mapping)
{
    $rc = array("present" => isset($cape['present']) ? $cape['present'] : false);
    if (!$rc['present']) {
        return $rc;
    }
    if (isset($cape['sendStats']) && $cape['sendStats'] == 0) {
        return $rc;
    }
    validateAndAdd($rc, $cape, $mapping);
    if (isset($cape['vendor']['name'])) {
        $rc['vendor'] = $cape['vendor']['name'];
    }
    return $rc;
}

/**
 * Builds the lookup from a configured destination name to the peer discovery
 * found there.
 *
 * A device is keyed under every name discovery knows it by -- each of its
 * addresses and its hostname -- because an output row may name its destination
 * either way, and a hostname-configured row that only matched on address would
 * read as an unknown, undiscovered target.  Each entry also carries a stable
 * identity so two rows naming one box under two different names are recognised
 * as a single destination rather than two.
 *
 * @return array Map of lowercased address/hostname => array(type, id).
 */
function stats_multiSyncPeerLookup()
{
    static $map = null;
    if ($map !== null) {
        return $map;
    }
    $map = array();
    $raw = @file_get_contents("http://localhost/api/fppd/multiSyncSystems");
    if ($raw === false) {
        return $map;
    }
    $data = json_decode($raw, true);
    if (!isset($data["systems"])) {
        return $map;
    }
    foreach ($data["systems"] as $system) {
        if (!isset($system['type'])) {
            continue;
        }
        // A box with several addresses is announced once per address, so the
        // uuid is what collapses those back into one device.  Falling back to
        // the address keeps a peer with no uuid distinct from every other peer
        // instead of merging them all under one empty identity.
        $id = "";
        if (isset($system['uuid']) && $system['uuid'] !== "") {
            $id = "uuid:" . $system['uuid'];
        } elseif (isset($system['address'])) {
            $id = "addr:" . strtolower($system['address']);
        }
        $entry = array("type" => $system['type'], "id" => $id);

        if (isset($system['address']) && $system['address'] !== "") {
            $map[strtolower($system['address'])] = $entry;
        }
        if (isset($system['hostname']) && $system['hostname'] !== "") {
            $host = strtolower($system['hostname']);
            foreach (array($host, $host . ".local") as $key) {
                if (!isset($map[$key])) {
                    $map[$key] = $entry;
                }
            }
        }
    }
    return $map;
}

/**
 * Maps each known peer name to the CLASS of device at that name, so an output
 * row can be tagged with what it feeds without the address ever leaving the
 * machine.  FPP already holds both halves of this join -- the multisync table
 * knows IP to type -- and the stats payload threw one half away.
 *
 * @return array Map of address/hostname => device type string, e.g. "Falcon F16v4".
 */
function stats_multiSyncPeerTypes()
{
    $map = array();
    foreach (stats_multiSyncPeerLookup() as $key => $entry) {
        $map[$key] = $entry['type'];
    }
    return $map;
}

/**
 * Classifies a configured destination by the FORM of the address, never by its
 * value.  An undiscovered target is far more actionable when we can tell a
 * hostname that never resolved from a literal address on a subnet discovery
 * cannot reach, so the shape of the string is kept even though the string
 * itself never leaves the machine.
 *
 * @param string $address Destination as configured on the output row.
 * @return string One of ipv4, ipv4_broadcast, ipv4_multicast, ipv6, hostname.
 */
function stats_addressForm($address)
{
    if (strpos($address, ':') !== false) {
        return "ipv6";
    }
    if (filter_var($address, FILTER_VALIDATE_IP, FILTER_FLAG_IPV4) === false) {
        return "hostname";
    }
    $octets = explode('.', $address);
    $first = intval($octets[0]);
    if ($first >= 224 && $first <= 239) {
        return "ipv4_multicast";
    }
    // Assumes a /24: the config carries no netmask and the interface it will be
    // sent from is not known here, so a .255 host on a wider subnet is called a
    // broadcast.  It is the common case by a wide margin, and the cost of the
    // misread is one target moving between "count" and "nonUnicast".
    if (intval($octets[3]) == 255 || $address == "255.255.255.255") {
        return "ipv4_broadcast";
    }
    return "ipv4";
}

/**
 * Collects E1.31/ArtNet universe output statistics from the channel outputs
 * configuration file, counting active rows, universes, channels, de-duplicate,
 * and monitor flags by type.
 *
 * @return array Universe output stats including universeCount, rowCount, channelCount, deDupeCount, and monitorCount.
 */
function stats_universe_out()
{
    global $settings;
    $rc = array("file" => $settings['universeOutputs']);
    if (!file_exists($rc['file'])) {
        return $rc;
    }

    $data = json_decode(file_get_contents($rc['file']), true);
    if (!isset($data["channelOutputs"])) {
        return $rc;
    }
    $outputs = $data["channelOutputs"];
    $data = $outputs[0];
    $mapping = array(
        "enabled" => "enabled",
        "threaded" => "threaded",
        "type" => "type",
    );
    // Was validateAndAdd($rc, $data["channelOutputs"], ...) -- $data is already
    // the entry, so that index does not exist and enabled/threaded/type have
    // never been emitted for universe outputs.
    validateAndAdd($rc, $data, $mapping);

    $universeCount = 0;
    $rowCount = 0;
    $activeRowCount = 0;
    $channelCount = 0;
    $monitorCount = 0;
    $deDupeCount = 0;
    $rowType = array();
    $priority = array();
    $destType = array();
    $peers = stats_multiSyncPeerTypes();
    $peerIds = array();
    foreach (stats_multiSyncPeerLookup() as $peerKey => $peerEntry) {
        $peerIds[$peerKey] = $peerEntry['id'];
    }
    // Per-destination roll-up.  rowCount alone cannot tell twelve rows aimed at
    // one controller from twelve controllers, and nothing in the payload said
    // whether a destination was ever discovered, so an install quietly pushing
    // data at an address that answers nothing looked identical to a healthy one.
    $targets = array();

    // Every entry, not just channelOutputs[0].  A second universe-output block
    // was silently invisible, and with it every row it carried.
    foreach ($outputs as $entry) {
        if (!isset($entry["universes"])) {
            continue;
        }
        foreach ($entry["universes"] as $row) {
            ++$rowCount;
            if (!isset($row["active"]) || $row["active"] != 1) {
                continue;
            }
            ++$activeRowCount;
            $rowChannels = stats_universeRowChannels($row);
            $rowUniverses = stats_universeRowUniverses($row);
            $universeCount += $rowUniverses;
            $channelCount += $rowChannels;
            if (isset($row["deDuplicate"])) {
                $deDupeCount += $row["deDuplicate"];
            }
            if (isset($row["monitor"])) {
                $monitorCount += $row["monitor"];
            }
            $type = "type_" . strval(isset($row['type']) ? $row['type'] : "unknown");
            if (!isset($rowType[$type])) {
                $rowType[$type] = 0;
            }
            $rowType[$type] += 1;

            // How many installs layer inputs by priority
            if (isset($row["priority"])) {
                $p = "priority_" . strval($row["priority"]);
                if (!isset($priority[$p])) {
                    $priority[$p] = 0;
                }
                $priority[$p] += 1;
            }

            // Classify the destination, never transmit it.  An address we cannot
            // resolve is "unknown" and an empty one is multicast, which has no
            // single destination to classify.
            $address = isset($row["address"]) ? trim($row["address"]) : "";
            $dest = "multicast";
            if ($address !== "") {
                $key = strtolower($address);
                $dest = isset($peers[$key]) ? $peers[$key] : "unknown";

                // Key the roll-up by device identity when discovery knows one,
                // so the same controller named by address on one row and by
                // hostname on another is one target, not two.
                $tkey = isset($peerIds[$key]) ? $peerIds[$key] : "name:" . $key;
                if (!isset($targets[$tkey])) {
                    $targets[$tkey] = array(
                        "rows" => 0,
                        "universes" => 0,
                        "channels" => 0,
                        "discovered" => isset($peers[$key]),
                        "form" => stats_addressForm($address),
                        "protocol" => array(),
                    );
                }
                $targets[$tkey]["rows"] += 1;
                $targets[$tkey]["universes"] += $rowUniverses;
                $targets[$tkey]["channels"] += $rowChannels;
                if (!isset($targets[$tkey]["protocol"][$type])) {
                    $targets[$tkey]["protocol"][$type] = 0;
                }
                $targets[$tkey]["protocol"][$type] += 1;
            }
            if (!isset($destType[$dest])) {
                $destType[$dest] = array("rows" => 0, "channels" => 0, "protocol" => array());
            }
            $destType[$dest]["rows"] += 1;
            $destType[$dest]["channels"] += $rowChannels;
            if (!isset($destType[$dest]["protocol"][$type])) {
                $destType[$dest]["protocol"][$type] = 0;
            }
            $destType[$dest]["protocol"][$type] += 1;
        }
    }
    $rc['universeCount'] = $universeCount;
    $rc['rowCount'] = $rowCount;
    $rc['activeRowCount'] = $activeRowCount;
    $rc['channelCount'] = $channelCount;
    $rc['rowType'] = $rowType;
    $rc['deDupeCount'] = $deDupeCount;
    $rc['monitorCount'] = $monitorCount;
    $rc['priority'] = $priority;
    $rc['destType'] = $destType;
    $rc['targets'] = stats_summarizeUDPTargets($targets);
    $rc['countsVersion'] = 2;

    return $rc;
}

/**
 * Reduces the per-destination roll-up to counts only -- no address ever appears
 * in the result, only how many there were and what shape they had.
 *
 * Answers two things the payload could not previously express:
 *  - unique UDP destinations, and how many output rows each one carries, so a
 *    one-row-per-controller install is distinguishable from one that splits a
 *    single controller across many rows;
 *  - whether each destination was found by discovery.  An undiscovered target
 *    is either a device FPP cannot see (wrong subnet, discovery blocked, a
 *    controller that does not announce) or a stale address left in the config,
 *    and both are worth knowing about in aggregate.
 *
 * Two limits a consumer cannot see from the numbers alone:
 *
 * "undiscovered" counts are an UPPER BOUND on devices, not a device count.
 * Targets collapse on the peer uuid, which an undiscovered target by definition
 * does not have, so those fall back to keying on the configured string and one
 * controller named by address on one row and by hostname on another counts
 * twice.  The slack is bounded by undiscoveredForm.hostname: an over-count
 * requires a hostname-form target aliasing an address-form one, so with no
 * undiscovered hostname targets the count is exact.
 *
 * Only UNICAST destinations are counted.  A row naming a broadcast or multicast
 * literal has no single destination -- it feeds an unknown number of devices,
 * and can never be "discovered" -- so counting it as one target would both
 * inflate any devices-per-show figure and inflate the undiscovered bucket.
 * Those rows are reported separately under "nonUnicast", which is the same
 * treatment an empty (multicast) address already gets by not being a target.
 *
 * @param array $targets Map of destination key => row/universe/channel counts.
 * @return array Counts by discovery state, address form, and rows-per-target.
 */
function stats_summarizeUDPTargets($targets)
{
    $rc = array(
        "count" => 0,
        "discovered" => array("targets" => 0, "rows" => 0, "universes" => 0, "channels" => 0),
        "undiscovered" => array("targets" => 0, "rows" => 0, "universes" => 0, "channels" => 0),
        "nonUnicast" => array("targets" => 0, "rows" => 0, "universes" => 0, "channels" => 0),
        "form" => array(),
        "undiscoveredForm" => array(),
        "rowsPerTarget" => array(),
        "maxRowsPerTarget" => 0,
    );

    foreach ($targets as $t) {
        // Every target contributes its form, unicast or not, so the split
        // between the counted and uncounted destinations stays visible.
        if (!isset($rc["form"][$t["form"]])) {
            $rc["form"][$t["form"]] = 0;
        }
        $rc["form"][$t["form"]] += 1;

        if ($t["form"] == "ipv4_broadcast" || $t["form"] == "ipv4_multicast") {
            $rc["nonUnicast"]["targets"] += 1;
            $rc["nonUnicast"]["rows"] += $t["rows"];
            $rc["nonUnicast"]["universes"] += $t["universes"];
            $rc["nonUnicast"]["channels"] += $t["channels"];
            continue;
        }
        $rc["count"] += 1;

        $bucket = $t["discovered"] ? "discovered" : "undiscovered";
        $rc[$bucket]["targets"] += 1;
        $rc[$bucket]["rows"] += $t["rows"];
        $rc[$bucket]["universes"] += $t["universes"];
        $rc[$bucket]["channels"] += $t["channels"];

        if (!$t["discovered"]) {
            if (!isset($rc["undiscoveredForm"][$t["form"]])) {
                $rc["undiscoveredForm"][$t["form"]] = 0;
            }
            $rc["undiscoveredForm"][$t["form"]] += 1;
        }

        // Histogram rather than a mean: the interesting installs are the tails,
        // and an average of 1.4 rows per target hides the box with 30.
        $b = "rows_" . strval($t["rows"]);
        if (!isset($rc["rowsPerTarget"][$b])) {
            $rc["rowsPerTarget"][$b] = 0;
        }
        $rc["rowsPerTarget"][$b] += 1;
        if ($t["rows"] > $rc["maxRowsPerTarget"]) {
            $rc["maxRowsPerTarget"] = $t["rows"];
        }
    }

    return $rc;
}

/**
 * Collects LED panel output configuration statistics from `channelOutputs.json`,
 * including panel dimensions, scan type, and panel count.
 *
 * @return array Panel output stats including type, panelWidth, panelHeight, panelCount, and channelCount.
 */
function stats_panel_out()
{
    global $settings;
    $rc = array("file" => $settings['channelOutputsJSON']);
    if (!file_exists($rc['file'])) {
        return $rc;
    }

    $data = json_decode(file_get_contents($rc['file']), true);
    if (!isset($data["channelOutputs"])) {
        return $rc;
    }
    $data = $data["channelOutputs"][0];
    $mapping = array(
        "enabled" => "enabled",
        "type" => "type",
        "subType" => "subType",
        "enabled" => "enabled",
        "panelWidth" => "panelWidth",
        "panelHeight" => "panelHeight",
        "panelScan" => "panelScan",
        "cfgVersion" => "cfgVersion",
        "panelOutputBlankRow" => "panelOutputBlankRow",
        "channelCount" => "channelCount",
    );
    validateAndAdd($rc, $data, $mapping);

    if (isset($data["panels"])) {
        $rc["panelCount"] = count($data["panels"]);
    }

    return $rc;
}

/**
 * Collects a list of enabled non-universe, non-panel channel output types
 * from the `co-other` configuration file.
 *
 * @return array Array with a "types" key listing enabled output type strings.
 */
function stats_other_out()
{
    global $settings;
    $rc = array("file" => $settings['co-other']);
    if (!file_exists($rc['file'])) {
        $rc['status'] = "File not found";
        return $rc;
    }

    $data = json_decode(file_get_contents($rc['file']), true);
    if (!isset($data["channelOutputs"])) {
        $rc['status'] = "ChannelOutputs not found";
        return $rc;
    }
    // "types" stayed a bare list of names, so one DMX universe looked identical
    // to twelve.  Kept for compatibility; "typeCounts" carries the shape.
    $types = array();
    $typeCounts = array();
    foreach ($data["channelOutputs"] as $row) {
        if (isset($row['enabled']) && $row['enabled'] == 1) {
            if (isset($row['type'])) {
                $type = $row['type'];
                array_push($types, $type);
                if (!isset($typeCounts[$type])) {
                    $typeCounts[$type] = array("cnt" => 0, "channels" => 0);
                }
                $typeCounts[$type]["cnt"] += 1;
                if (isset($row['channelCount'])) {
                    $typeCounts[$type]["channels"] += $row['channelCount'];
                }
            }
        }
    }

    $rc['types'] = $types;
    $rc['typeCounts'] = $typeCounts;

    return $rc;
}

/**
 * Collects pixel string output statistics from the given config file,
 * counting the total pixel count and the set of protocols in use.
 *
 * @param string $file Absolute path to the pixel strings configuration JSON file.
 * @return array Stats including type, subType, enabled, outputCount, pixelCount, and protocols.
 */
function stats_pixel_or_pi($file)
{
    global $settings;
    $rc = array("file" => $file);
    if (!file_exists($rc['file'])) {
        $rc['status'] = "File not found";
        return $rc;
    }

    $data = json_decode(file_get_contents($rc['file']), true);
    if (!isset($data["channelOutputs"])) {
        $rc['status'] = "ChannelOutputs not found";
        return $rc;
    }

    $data = $data["channelOutputs"][0];

    $mapping = array(
        "type" => "type",
        "subType" => "subType",
        "enabled" => "enabled",
        "pinoutVersion" => "pinoutVersion",
        "outputCount" => "outputCount",
    );
    validateAndAdd($rc, $data, $mapping);

    $pixelCount = 0;
    $protocols = array();
    $usedPortCount = 0;
    $virtualStringCount = 0;
    $portPixels = array();
    if (isset($data['outputs'])) {
        foreach ($data['outputs'] as $row) {
            if (isset($row['protocol'])) {
                // Was $protocols[...] = 1, a set, so a cape with 16 ws2811 ports
                // looked the same as one with a single port in use.
                if (!isset($protocols[$row['protocol']])) {
                    $protocols[$row['protocol']] = 0;
                }
                $protocols[$row['protocol']] += 1;
            }

            $rowPixels = 0;
            if (isset($row['virtualStrings'])) {
                foreach ($row['virtualStrings'] as $line) {
                    // More than one virtual string on a port is what a smart
                    // receiver or a differential split looks like from here.
                    $virtualStringCount += 1;
                    if (isset($line['pixelCount'])) {
                        $rowPixels += $line["pixelCount"];
                    }
                }
            }
            $pixelCount += $rowPixels;
            if ($rowPixels > 0) {
                $usedPortCount += 1;
            }
            // Bucketed rather than per-port, so this says how fully ports get
            // populated without becoming a per-install fingerprint.
            // Prefixed: a bare "0" key becomes an int in PHP, which makes the
            // whole map serialise as a JSON array and drops the labels.
            $bucket = "px_" . ($rowPixels == 0 ? "0" : ($rowPixels <= 50 ? "1-50" : ($rowPixels <= 100 ? "51-100" : ($rowPixels <= 200 ? "101-200" : ($rowPixels <= 400 ? "201-400" : "401+")))));
            if (!isset($portPixels[$bucket])) {
                $portPixels[$bucket] = 0;
            }
            $portPixels[$bucket] += 1;
        }
    }

    $rc['pixelCount'] = $pixelCount;
    $rc['protocols'] = $protocols;
    $rc['usedPortCount'] = $usedPortCount;
    $rc['virtualStringCount'] = $virtualStringCount;
    $rc['portPixelBuckets'] = $portPixels;
    return $rc;
}

/**
 * Collects pixel string output statistics for Raspberry Pi (`co-pixelStrings`).
 *
 * @return array Pixel string output stats (see stats_pixel_or_pi).
 */
function stats_pixel_pi_out()
{
    global $settings;
    return stats_pixel_or_pi($settings['co-pixelStrings']);
}

/**
 * Collects pixel string output statistics for BeagleBone (`co-bbbStrings`).
 *
 * @return array Pixel string output stats (see stats_pixel_or_pi).
 */
function stats_pixel_bbb_out()
{
    global $settings;
    return stats_pixel_or_pi($settings['co-bbbStrings']);
}

/**
 * Returns the current system timezone offset and abbreviation.
 *
 * @return string Timezone string, e.g. "-0500 EST".
 */
function stats_timezone()
{
    $output = [];
    exec("date '+%z %Z'", $output);
    return $output[0];
}

/**
 * Collects PWM output configuration statistics from the `co-pwm` config file,
 * including enabled state, frequency, and counts of `LED` vs `Servo` output types.
 *
 * @return array PWM output stats including type, enabled, frequency, and types map.
 */
function stats_pwm_out()
{
    global $settings;
    global $settings;
    $rc = array("file" => $settings['co-pwm']);
    if (!file_exists($rc['file'])) {
        $rc['status'] = "File not found";
        return $rc;
    }

    $data = json_decode(file_get_contents($rc['file']), true);
    if (!isset($data["channelOutputs"])) {
        $rc['status'] = "ChannelOutputs not found";
        return $rc;
    }

    $data = $data["channelOutputs"][0];

    $mapping = array(
        "type" => "type",
        "subType" => "subType",
        "enabled" => "enabled",
        "frequency" => "frequency"
    );
    validateAndAdd($rc, $data, $mapping);

    $types = array();
    $types["LED"] = 0;
    $types["Servo"] = 0;
    if (isset($data['outputs'])) {
        foreach ($data['outputs'] as $row) {
            if (isset($row['type'])) {
                $types[$row['type']] += 1;
            }
        }
    }
    $rc['types'] = $types;

    return $rc;
}