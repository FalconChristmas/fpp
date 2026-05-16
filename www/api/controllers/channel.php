<?php

/**
 * Get E1.31 stats
 *
 * Returns the E1.31 or DDP statistics for inbound packets.
 * Returns a meaningful error if the connection to `fppd` fails.
 *
 * @badge "FPP REQUIRED" critical
 * @route GET /api/channel/input/stats
 * @response 200 E1.31/DDP channel input statistics
 * ```json
 * {
 *   "status": "OK",
 *   "universes": [
 *     {
 *       "bytesReceived": "325632",
 *       "errors": "1",
 *       "id": 1,
 *       "packetsReceived": "636",
 *       "startChannel": 1
 *     }
 *   ]
 * }
 * ```
 */
function ChannelInputGetStats()
{
    $data = file_get_contents('http://127.0.0.1:32322/fppd/e131stats');
    $rc = array();
    if ($data === false) {
        $rc['status'] = 'ERROR: FPPD may be down';
        $rc['universes'] = array();
    } else {
        $stats = json_decode($data);
        $rc['status'] = 'OK';
        $rc['universes'] = $stats->universes;
    }

    return json($rc);
}

/**
 * Reset E1.31 stats
 *
 * Resets the E1.31/DDP channel input statistics counters.
 *
 * @badge "FPP REQUIRED" critical
 * @route DELETE /api/channel/input/stats
 * @response 200 Statistics reset
 * ```json
 * {"status": "OK"}
 * ```
 */
function ChannelInputDeleteStats()
{
    $url = 'http://127.0.0.1:32322/fppd/e131stats';
    $ch = curl_init();
    curl_setopt($ch, CURLOPT_URL, $url);
    curl_setopt($ch, CURLOPT_CUSTOMREQUEST, "DELETE");
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($ch, CURLOPT_FAILONERROR, true);
    curl_setopt($ch, CURLOPT_CONNECTTIMEOUT_MS, 400);
    $result = curl_exec($ch);
    $result = json_decode($result);
    curl_close($ch);

    return json($result);
}

/**
 * Get output processors
 *
 * Returns the current configuration of any output processors.
 *
 * @route GET /api/channel/output/processors
 * @response 200 Current output processor configuration
 * ```json
 * {
 *   "outputProcessors": [
 *     {
 *       "type": "Brightness",
 *       "active": 0,
 *       "description": "",
 *       "start": 1,
 *       "count": 10,
 *       "brightness": 50,
 *       "gamma": 1
 *     }
 *   ],
 *   "status": "OK"
 * }
 * ```
 */
function ChannelGetOutputProcessors()
{
    global $settings;

    $rc = array("status" => "OK", "outputProcessors" => array());

    if (file_exists($settings['outputProcessorsFile'])) {
        $jsonStr = file_get_contents($settings['outputProcessorsFile']);
        $rc = json_decode($jsonStr, true);
        $rc["status"] = "OK";
    }

    return json($rc);

}

/**
 * Set output processors
 *
 * Overwrites the output processor settings file with a new configuration and
 * returns the saved configuration.
 *
 * @route POST /api/channel/output/processors
 * @body {"outputProcessors": [{"type": "Brightness", "active": 0, "description": "", "start": 1, "count": 10, "brightness": 50, "gamma": 1}]}
 * @response 200 Current output processor configuration
 * ```json
 * {
 *   "outputProcessors": [
 *     {
 *       "type": "Brightness",
 *       "active": 0,
 *       "description": "",
 *       "start": 1,
 *       "count": 10,
 *       "brightness": 50,
 *       "gamma": 1
 *     }
 *   ],
 *   "status": "OK"
 * }
 * ```
 */
function ChannelSaveOutputProcessors()
{
    global $settings;
    global $args;

    $data = file_get_contents('php://input');
    $data = prettyPrintJSON(stripslashes($data));

    file_put_contents($settings['outputProcessorsFile'], $data);

    //Trigger a JSON Configuration Backup
    GenerateBackupViaAPI('Channel Output Processor was modified.');

    return ChannelGetOutputProcessors();
}

/**
 * Get channel output
 *
 * Returns the current configuration of the specified output file in JSON format.
 * Common values of `{file}` include `universeOutputs`, `universeInputs`, `co-other`, `dmxInputs`,
 * `co-pwm`, and `co-bbbStrings`. Supports an optional `?ip=` query parameter to fetch from
 * a remote FPP instance.
 *
 * @route GET /api/channel/output/{file}
 * @response 200 Channel output configuration file contents
 * ```json
 * {}
 * ```
 * @response 404 File not found
 * ```json
 * {"status": "ERROR: File not found"}
 * ```
 */
function ChannelGetOutput()
{
    global $settings;

    $file = params("file");

    // If ip parameter is provided, fetch from remote system via server-side curl
    // This avoids CSP restrictions on cross-origin browser requests
    if (isset($_GET['ip'])) {
        $ip = $_GET['ip'];
        if (!filter_var($ip, FILTER_VALIDATE_IP)) {
            return json(array("status" => "ERROR: Invalid IP address"));
        }
        $curl = curl_init("http://" . $ip . "/api/channel/output/" . urlencode($file));
        curl_setopt($curl, CURLOPT_FAILONERROR, true);
        curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
        curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
        curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 500);
        curl_setopt($curl, CURLOPT_TIMEOUT_MS, 3000);
        $request_content = curl_exec($curl);
        $responseCode = curl_getinfo($curl, CURLINFO_RESPONSE_CODE);
        curl_close($curl);
        if ($request_content === false || $responseCode != 200) {
            return json(array("status" => "ERROR: Could not reach remote system"));
        }
        $data = json_decode($request_content, true);
        if ($data === null) {
            return json(array("status" => "ERROR: Invalid response from remote system"));
        }
        return json($data);
    }

    $rc = array("status" => "ERROR: File not found");

    $jsonStr = "";

    if (!isset($settings[$file])) {
        $rc['status'] = "Invalid file $file";
    } else if (file_exists($settings[$file])) {
        $rc = json_decode(file_get_contents($settings[$file]), true);
        $rc["status"] = "OK";
    } else {
        http_response_code(404);
    }

    return json($rc);
}

/**
 * Set channel output
 *
 * Overwrites the specified output configuration file with the `POST` body
 * and returns the saved configuration.
 *
 * @route POST /api/channel/output/{file}
 * @body "Format varies based on file"
 * @response 200 Saved configuration echoed back
 * ```json
 * {}
 * ```
 */
function ChannelSaveOutput()
{
    global $settings;

    $file = params("file");
    if (isset($settings[$file])) {
        $data = file_get_contents('php://input');
        $data = prettyPrintJSON(stripslashes($data));
        file_put_contents($settings[$file], $data);

        //Trigger a JSON Configuration Backup
        GenerateBackupViaAPI('Channel output ' . $file . ' was modified.');

        return ChannelGetOutput();
    } else {
        return json(array("status" => "ERROR file not supported: " . $file));
    }
}
