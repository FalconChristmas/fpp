<?php

//GET /api/channel/input/stats
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

//GET /api/channel/input/stats
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


//GET /api/channel/output/processor
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

//PUSH /api/channel/output/processor
function channel_save_output_processors()
{
    global $settings;
    global $args;

    $data = file_get_contents('php://input');
    $data = prettyPrintJSON(stripslashes($data));

    file_put_contents($settings['outputProcessorsFile'], $data);

    //Trigger a JSON Configuration Backup
    GenerateBackupViaAPI('Channel Output Processor was modified.');

    return channel_get_output_processors();
}

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

function channel_save_output()
{
    global $settings;

    $file = params("file");
    if (isset($settings[$file])) {
        $data = file_get_contents('php://input');
        $data = prettyPrintJSON(stripslashes($data));
        file_put_contents($settings[$file], $data);

        //Trigger a JSON Configuration Backup
        GenerateBackupViaAPI('Channel output ' . $file . ' was modified.');

        return channel_get_output();
    } else {
        return json(array("status" => "ERROR file not supported: " . $file));
    }
}
