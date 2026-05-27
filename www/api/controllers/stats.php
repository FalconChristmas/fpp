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
    );

    foreach ($tasks as $key => $fun) {
        try {
            $obj[$key] = call_user_func($fun);
        } catch (exception $e) {
            echo ("Call to $t failed");
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
 * @route-v1 GET /statistics/usage
 * @route-v2 GET /statistics/usage
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
        if (isset($rc['interfaces'][$name]['config'])) {
            $rc['interfaces'][$name]['config'] = $i['config'];
            unset($rc['interfaces'][$name]['config']['PSK']);
            unset($rc['interfaces'][$name]['config']['SSID']);
            unset($rc['interfaces'][$name]['config']['ADDRESS']);
            unset($rc['interfaces'][$name]['config']['NETMASK']);
            unset($rc['interfaces'][$name]['config']['GATEWAY']);
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
 * @route-v1 POST /statistics/usage
 * @route-v2 POST /statistics/usage
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
 * @route-v1 DELETE /statistics/usage
 * @route-v2 DELETE /statistics/usage
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
 * @param array &$data MultiSync data array containing a "systems" key.
 * @return void
 */
function addMultiSyncUUID(&$data)
{
    if (!isset($data["systems"])) {
        return;
    }
    $missing = array();
    foreach ($data["systems"] as $system) {
        $rec = array();
        validateAndAdd($rec, $system, $mapping);
        if (!isset($system['uuid']) || $system['uuid'] === "") {
            $missing[$system['address']] = $system['typeId'];
        }
    }
    // Find missing UUIDs
    if (count($missing) > 0) {
        $curlmulti = curl_multi_init();
        $curls = array();
        foreach ($missing as $ip => $tid) {
            if ($tid >= 160 && $tid < 170) {
                $curl = curl_init("http://" . $ip . "/update/identity");
            } else {
                $curl = curl_init("http://" . $ip . "/api/fppd/status");
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

            if ($request_content === false || $request_content == null || $request_content == "") {
                $responseCode = curl_getinfo($curl, CURLINFO_RESPONSE_CODE);
                $missing[$ip] = "Failed";
            } else {
                $content = json_decode($request_content, true);
                $missing[$ip] = "Not Set";
                if (isset($content['uuid'])) {
                    $missing[$ip] = $content['uuid'];
                } else if (isset($content['id'])) {
                    if (isset($content['hardware'])) {
                        $missing[$ip] = $content['hardware'] . "-" . $content['id'];
                    } else {
                        $missing[$ip] = $content['id'];
                    }
                }
            }
            curl_multi_remove_handle($curlmulti, $curl);
        }
        curl_multi_close($curlmulti);

        // Add them back
        foreach ($data["systems"] as &$system) {
            $ip = $system['address'];
            if (!isset($system['uuid']) || $system['uuid'] === "") {
                if (isset($missing[$ip])) {
                    $system['uuid'] = $missing[$ip];
                }
            }
        }
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
    );

    $data = json_decode(file_get_contents("http://localhost/api/fppd/multiSyncSystems"), true);
    $rc = array();
    if (isset($data["systems"])) {
        addMultiSyncUUID($data);
        foreach ($data["systems"] as $system) {
            $rec = array();
            validateAndAdd($rec, $system, $mapping);
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
 * Returns the system UUID.
 *
 * @return string UUID string.
 */
function stats_getUUID()
{
    return getSystemUUID();
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
            if ($row["active"] == 1) {
                ++$activeRowCount;
                $universeCount += $row["universeCount"];
                $channelCount += ($row["universeCount"] * $row["channelCount"]);
                $type = "type_" . strval($row['type']);
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

    return $rc;
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
    $data = $data["channelOutputs"][0];
    $mapping = array(
        "enabled" => "enabled",
        "threaded" => "threaded",
        "type" => "type",
    );
    validateAndAdd($rc, $data["channelOutputs"], $mapping);

    $universeCount = 0;
    $rowCount = 0;
    $activeRowCount = 0;
    $channelCount = 0;
    $monitorCount = 0;
    $deDupeCount = 0;
    $rowType = array();
    if (isset($data["universes"])) {
        foreach ($data["universes"] as $row) {
            ++$rowCount;
            if ($row["active"] == 1) {
                ++$activeRowCount;
                $universeCount += $row["universeCount"];
                $channelCount += ($row["universeCount"] * $row["channelCount"]);
                if (isset($row["deDuplicate"])) {
                    $deDupeCount += $row["deDuplicate"];
                }
                if (isset($row["monitor"])) {
                    $monitorCount += $row["monitor"];
                }
                $type = "type_" . strval($row['type']);
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
    $rc['deDupeCount'] = $deDupeCount;
    $rc['monitorCount'] = $monitorCount;

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
    $types = array();
    foreach ($data["channelOutputs"] as $row) {
        if (isset($row['enabled']) && $row['enabled'] == 1) {
            if (isset($row['type'])) {
                array_push($types, $row['type']);
            }
        }
    }

    $rc['types'] = $types;

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
    if (isset($data['outputs'])) {
        foreach ($data['outputs'] as $row) {
            if (isset($row['protocol'])) {
                $protocols[$row['protocol']] = 1;
            }

            if (isset($row['virtualStrings'])) {
                foreach ($row['virtualStrings'] as $line) {
                    if (isset($line['pixelCount'])) {
                        $pixelCount += $line["pixelCount"];
                    }
                }
            }
        }
    }

    $rc['pixelCount'] = $pixelCount;
    $rc['protocols'] = $protocols;
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