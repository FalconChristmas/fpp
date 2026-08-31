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
    $output = array();

    exec("curl -s -m 2 https://github.com/FalconChristmas/fpp/blob/master/README.md", $output, $exitCode);
    $rc['github_access'] = ($exitCode == 0 ? true : false);

    $rc['wifi'] = json_decode(file_get_contents("http://localhost/api/network/wifi/strength"), true);

    $interfaces = json_decode(file_get_contents("http://localhost/api/network/interface"), true);
    foreach ($interfaces as $i) {
        $name = $i['ifname'];
        if (isset($i['operstate'])) {
            $rc['interfaces'][$name]['operstate'] = $i['operstate'];
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
function stats_publish_stats_file()
{
    global $settings;
    $jsonString = stats_get_last_file();

    $ch = curl_init($settings['statsPublishUrl']);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($ch, CURLOPT_HTTPHEADER, array('Content-Type:application/json'));
    curl_setopt($ch, CURLOPT_POSTFIELDS, $jsonString);
    curl_setopt($ch, CURLOPT_CONNECTTIMEOUT_MS, 800);
    curl_setopt($ch, CURLOPT_TIMEOUT_MS, 3000);
    // execute!
    $response = json_decode(curl_exec($ch));

    // close the connection, release resources used
    curl_close($ch);
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
 * Queries each MultiSync system that has no UUID and attempts to fill in
 * the missing value by probing the remote device's status or identity endpoint.
 *
 * A peer that cannot be probed, or that answers with an unusable value, gets
 * a locally stable but globally unique substitute rather than a shared
 * sentinel string.  Sentinels such as "Failed" collide across every install
 * that emits them, which merges unrelated shows into a single identity.
 *
 * @param array &$data MultiSync data array containing a "systems" key.
 * @return void
 */
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

function addMultiSyncUUID(&$data)
{
    if (!isset($data["systems"])) {
        return;
    }
    $missing = array();
    foreach ($data["systems"] as $system) {
        if (!isset($system['uuid']) || !isValidSystemUUID($system['uuid'])) {
            $missing[$system['address']] = array(
                'typeId' => $system['typeId'],
                // Kept so the fallback below can prefer a MAC-derived stand-in
                // over a per-reporter hash.
                'prior' => isset($system['uuid']) ? $system['uuid'] : '',
            );
        }
    }
    // Find missing UUIDs
    if (count($missing) > 0) {
        $curlmulti = curl_multi_init();
        $curls = array();
        foreach ($missing as $ip => $info) {
            $tid = $info['typeId'];
            //IPv6 literals must be bracketed to be usable in a URL
            $urlHost = fppUrlHost($ip);
            if ($tid >= 160 && $tid < 170) {
                $curl = curl_init("http://" . $urlHost . "/update/identity");
            } else {
                $curl = curl_init("http://" . $urlHost . "/api/fppd/status");
            }
            curl_setopt($curl, CURLOPT_FAILONERROR, true);
            curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
            curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
            curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 500);
            curl_setopt($curl, CURLOPT_TIMEOUT_MS, 3000);
            $curls[$ip] = $curl;
            curl_multi_add_handle($curlmulti, $curl);
        }
        $running = null;
        do {
            curl_multi_exec($curlmulti, $running);
        } while ($running > 0);

        foreach ($curls as $ip => $curl) {
            $request_content = curl_multi_getcontent($curl);
            $uuid = "";

            if ($request_content !== false && $request_content !== null && $request_content !== "") {
                $content = json_decode($request_content, true);
                if (isset($content['uuid'])) {
                    $uuid = $content['uuid'];
                } else if (isset($content['id'])) {
                    if (isset($content['hardware'])) {
                        $uuid = $content['hardware'] . "-" . $content['id'];
                    } else {
                        $uuid = $content['id'];
                    }
                }
            }

            $resolved = isValidSystemUUID($uuid) ? $uuid : "";
            if ($resolved === "") {
                // A "MAC:" value is not an identity the device chose, which is
                // why isValidSystemUUID() rejects it and why we ask the
                // controller for a real one first.  But it is the same value
                // for a given device no matter which player reports it, and
                // that is exactly what makes deduplicating a show and drawing
                // its network possible.  localPeerIdentity() is salted with the
                // reporting host, so two players describing one controller
                // produce two unrelated rows -- use it only when there is no
                // MAC to fall back on.
                $prior = $missing[$ip]['prior'];
                $resolved = (stripos($prior, 'MAC:') === 0)
                    ? peerMacIdentity($prior)
                    : localPeerIdentity($ip);
            }
            $missing[$ip] = $resolved;
            curl_multi_remove_handle($curlmulti, $curl);
        }
        curl_multi_close($curlmulti);

        // Add them back
        foreach ($data["systems"] as &$system) {
            $ip = $system['address'];
            if (!isset($system['uuid']) || !isValidSystemUUID($system['uuid'])) {
                if (isset($missing[$ip])) {
                    $system['uuid'] = $missing[$ip];
                }
            }
        }
        unset($system);
    }
}

/**
 * Collects a sanitized list of MultiSync peer systems, filling in missing
 * UUIDs where possible.
 *
 * @return array Array of per-system records with version, type, and UUID info.
 */
function stats_getMultiSync()
{
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

    $data = json_decode(file_get_contents("http://localhost/api/fppd/multiSyncSystems"), true);
    $rc = array();
    if (isset($data["systems"])) {
        addMultiSyncUUID($data);
        foreach ($data["systems"] as $system) {
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

            array_push($rc, $rec);
        }
    }
    return $rc;
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
 * Collects cape hardware information. If SendVendorSerial is enabled, the
 * serial number is included; otherwise it is omitted for privacy.
 *
 * @return array Cape info with type, id, name, designer, and vendor fields.
 */
function stats_getCapeInfo()
{
    global $settings;
    $rc = array("name" => "None");
    if ($settings['SendVendorSerial'] == 1) {
        $mapping = array(
            "type" => "type",
            "cs" => "cs",
            "id" => "id",
            "name" => "name",
            "serialNumber" => "serialNumber",
            "designer" => "designer",
            "verifiedKeyId" => "verifiedKeyId",
            "vendor" => "vendor"
        );
    } else {
        $mapping = array(
            "type" => "type",
            "id" => "id",
            "name" => "name",
            "designer" => "designer",
            "verifiedKeyId" => "verifiedKeyId",
            "vendor" => "vendor"
        );
    }

    $data = json_decode(file_get_contents("http://localhost/api/cape"), true);
    if (($data != false) && ((!isset($data['sendStats'])) || ($data['sendStats'] == 1))) {
        validateAndAdd($rc, $data, $mapping);
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
        if (isset($config['gatherStats']) && $config['gatherStats']) {
            $safeSettings[$name] = $name;
        }
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