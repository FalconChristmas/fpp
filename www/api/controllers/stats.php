<?php

function stats_genereate($statsFile)
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
        "universe_input" => 'stats_universe_in',
        "output_e131" => 'stats_universe_out',
        "output_panel" => 'stats_panel_out',
        "output_other" => 'stats_other_out',
        "output_pixel_pi" => 'stats_pixel_pi_out',
        "output_pixel_bbb" => 'stats_pixel_bbb_out',
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

// GET /api/statistics/usage
function stats_get_last_file()
{
    global $_GET;
    $statsFile = stats_get_filename();

    if (file_exists($statsFile)) {
        // No reason to regenereate if less than 2 hours old
        if (time() - filemtime($statsFile) > 2 * 3600) {
            stats_genereate($statsFile);
        } else if (isset($_GET['force']) && $_GET['force'] == 1) {
            stats_genereate($statsFile);
        }
    } else {
        stats_genereate($statsFile);
    }

    return json(json_decode(file_get_contents($statsFile)), JSON_PRETTY_PRINT);
}

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

// DELETE /api/statistics/usage
function stats_delete_last_file()
{
    $statsFile = stats_get_filename();
    if (file_exists($statsFile)) {
        unlink($statsFile);
    }
    return json(array("status" => "OK"));
}

function stats_get_filename()
{
    return "/tmp/fpp_stats.json";
}

function validateAndAdd(&$obj, &$input, &$mapping)
{
    foreach ($mapping as $newKey => $oldKey) {
        if (isset($input[$oldKey])) {
            $obj[$newKey] = $input[$oldKey];
        }
    }
}

function stats_getSystemInfo()
{
    $rc = array();
    $data = json_decode(file_get_contents("http://localhost/api/system/status"), true);
    $mapping = array(
        "mqtt" => "MQTT",
        "fppdStatus" => "fppd",
        "fppdMode" => "mode_name",
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

// Reviews the multisync records and adds missing UUIDS
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
            $missing[$system['address']] = "1";
        }
        array_push($rc, $rec);
    }
    // Find missing UUIDs
    if (count($missing) > 0) {
        $curlmulti = curl_multi_init();
        $curls = array();
        foreach ($missing as $ip => $value) {
            $curl = curl_init("http://" . $ip . "/api/fppd/status");
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

function stats_getModels()
{
    $data = json_decode(file_get_contents("http://localhost/api/models"), true);
    $rc = array("count" => 0);
    if (is_array($data)) {
        $rc["count"] = count($data);
    }

    return $rc;
}

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

function stats_getUUID()
{
    $output = array();
    exec("/opt/fpp/scripts/get_uuid", $output);

    return $output[0];
}

function stats_getCapeInfo()
{
    $rc = array("type" => "None");
    $mapping = array(
        "type" => "type",
        "cs" => "cs",
        "id" => "id",
        "name" => "name",
        "serialNumber" => "serialNumber",
    );

    $data = json_decode(file_get_contents("http://localhost/api/cape"), true);
    if ($data != false) {
        validateAndAdd($rc, $data, $mapping);
    }

    return $rc;
}

function stats_getSettings()
{
    global $settings;
    $rc = array();
    $safeSettings = array();
    $allSettings = json_decode(file_get_contents($settings['wwwDir'] . "/settings.json"), true);
    foreach ($allSettings['settings'] as $name => $config) {
        if (isset($config['gatherStats']) && $config['gatherStats']) {
            $safeSettings[$name] = $name;
        }
    }
    validateAndAdd($rc, $settings, $safeSettings);

    return $rc;
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
                $deDupeCount += $row["deDuplicate"];
                $monitorCount += $row["monitor"];

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

    return $rc;
}

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
    $protoocols = array();
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

function stats_pixel_pi_out()
{
    global $settings;
    return stats_pixel_or_pi($settings['co-pixelStrings']);
}

function stats_pixel_bbb_out()
{
    global $settings;
    return stats_pixel_or_pi($settings['co-bbbStrings']);
}
