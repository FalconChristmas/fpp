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
function channel_input_get_stats()
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
function channel_input_delete_stats()
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
 * Get serial devices
 *
 * Lists the serial devices on the system and, for each, a name for it that
 * survives a reboot. USB adapters are numbered in the order they are found, so
 * ttyUSB0 can be a different adapter after a reboot; `stable` is its
 * /dev/serial/by-id link where the adapter has a USB serial number, else its
 * by-path link, which follows the USB socket. Any of these names can be used as
 * the `device` of a serial channel output.
 *
 * @route GET /api/channel/output/serialDevices
 * @response 200 Serial devices
 * ```json
 * {
 *   "status": "OK",
 *   "devices": [
 *     {
 *       "device": "ttyUSB0",
 *       "stable": "serial/by-id/usb-FTDI_USB__-__Serial_Converter_A1B2C3D4-if00-port0",
 *       "byId": "serial/by-id/usb-FTDI_USB__-__Serial_Converter_A1B2C3D4-if00-port0",
 *       "byPath": "serial/by-path/platform-xhci-hcd.0-usb-0:1.3:1.0-port0",
 *       "usbSerial": "A1B2C3D4",
 *       "description": "FTDI USB <-> Serial Converter"
 *     }
 *   ]
 * }
 * ```
 */
function channel_get_serial_devices()
{
    return json(array("status" => "OK", "devices" => GetSerialDevices()));
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
function channel_get_output_processors()
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
function channel_save_output_processors()
{
    global $settings;
    global $args;

    // Never stripslashes() the body: its backslashes are JSON escapes from
    // JSON.stringify(), and stripping them turned a quote or backslash typed into
    // a description into an unparseable file.  Refuse anything that is not a JSON
    // object before it replaces the working file.  It is only decoded to check it;
    // the text is written as sent, so {} stays {} and 1.0 stays 1.0.
    $data = file_get_contents('php://input');
    if (!is_object(json_decode($data))) {
        http_response_code(400);
        return json(array("status" => "ERROR: body is not a JSON object"));
    }
    $data = prettyPrintJSON($data);

    // Atomic write so a reader never observes a truncated/partial file mid-save.
    WriteFileAtomic($settings['outputProcessorsFile'], $data);

    //Trigger a JSON Configuration Backup
    GenerateBackupViaAPI('Channel Output Processor was modified.');

    return channel_get_output_processors();
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
function channel_get_output()
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
        //IPv6 literals must be bracketed to be usable in a URL
        $curl = curl_init("http://" . fppUrlHost($ip) . "/api/channel/output/" . urlencode($file));
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
        // The response is a re-encode of this one file plus a status key, so
        // the file's stat is a complete validator for it. Only on GET: the
        // save handler below calls this to echo the config back, and that
        // reply must never come back bodiless.
        if ($_SERVER['REQUEST_METHOD'] === 'GET'
            && fppSendFileCacheValidators($settings[$file])) {
            return '';
        }
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
function channel_save_output()
{
    global $settings;

    $file = params("file");
    if (isset($settings[$file])) {
        // Same rules as channel_save_output_processors(): no stripslashes(), and
        // nothing but a JSON object may replace the file -- fppd only loads an
        // object, and the page shows no outputs at all for a file it cannot parse.
        $data = file_get_contents('php://input');
        if (!is_object(json_decode($data))) {
            http_response_code(400);
            return json(array("status" => "ERROR: body is not a JSON object"));
        }
        $data = prettyPrintJSON($data);
        // Atomic write so fppd's inotify-driven reload (and any other reader)
        // never sees a truncated/partial file -- e.g. co-universes.json, which
        // ChannelOutputSetup and MultiSync both watch.
        WriteFileAtomic($settings[$file], $data);

        //Trigger a JSON Configuration Backup
        GenerateBackupViaAPI('Channel output ' . $file . ' was modified.');

        return channel_get_output();
    } else {
        return json(array("status" => "ERROR file not supported: " . $file));
    }
}
