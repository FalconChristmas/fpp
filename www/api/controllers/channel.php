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

    return channel_get_output_processors();
}

function channel_get_output()
{
    global $settings;

    $file = params("file");
    $rc = array("Status" => "ERROR: File not found");

    $jsonStr = "";

    if (file_exists($settings[$file])) {
        $rc = json_decode(file_get_contents($settings[$file]), true);
        $rc["status"] = "OK";
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
        return channel_get_output();
    } else {
        return json(array("status" => "ERROR file not supported: " . $file));
    }
}
