<?php

require_once '../pixelnetdmxentry.php';

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


//POST /api/channel/output/PixelnetDMX
function channel_put_pixelnetDMX()
{
    $json = strval(file_get_contents('php://input'));
    $input = json_decode($json, true);
    $status = "OK";

    if (!isset($input['model'])) {
        $status = 'Failure, no model supplied';
        return json(array("status" => $status));
    }

    $model = $input['model'];
    $firmware = $input['firmware'];

    if ($model == "F16V2-alpha") {
        SaveF16v2Alpha($input["pixels"]);
    } else if ($model == "FPDv1") {
        SaveFPDv1($input["pixels"]);
    } else {
        $status = 'Failure, Unknown Model';
        return json(array("status" => $status));
    }

    return json(array("status" => "OK"));
}

function SaveF16v2Alpha($pixels)
{
    global $settings;
    $outputCount = 16;

    $carr = array();
    for ($i = 0; $i < 1024; $i++) {
        $carr[$i] = 0x0;
    }

    $i = 0;

    // Header
    $carr[$i++] = 0x55;
    $carr[$i++] = 0x55;
    $carr[$i++] = 0x55;
    $carr[$i++] = 0x55;
    $carr[$i++] = 0x55;
    $carr[$i++] = 0xCD;

    // Some byte
    $carr[$i++] = 0x01;

    for ($o = 0; $o < $outputCount; $o++) {
        $cur = $pixels[$o];
        $nodeCount = $cur['nodeCount'];
        $carr[$i++] = intval($nodeCount % 256);
        $carr[$i++] = intval($nodeCount / 256);

        $startChannel = $cur['startChannel'] - 1; // 0-based values in config file
        $carr[$i++] = intval($startChannel % 256);
        $carr[$i++] = intval($startChannel / 256);

        // Node Type is set on groups of 4 ports
        $carr[$i++] = intval($cur['nodeType']);

        $carr[$i++] = intval($cur['rgbOrder']);
        $carr[$i++] = intval($cur['direction']);
        $carr[$i++] = intval($cur['groupCount']);
        $carr[$i++] = intval($cur['nullNodes']);
    }

    $f = fopen($settings['configDirectory'] . "/Falcon.F16V2-alpha", "wb");
    fwrite($f, implode(array_map("chr", $carr)), 1024);
    fclose($f);
    SendCommand('w');
}

function SaveFPDv1($pixels)
{
    global $settings;
    $outputCount = 12;

    $carr = array();
    for ($i = 0; $i < 1024; $i++) {
        $carr[$i] = 0x0;
    }

    $i = 0;
    // Header
    $carr[$i++] = 0x55;
    $carr[$i++] = 0x55;
    $carr[$i++] = 0x55;
    $carr[$i++] = 0x55;
    $carr[$i++] = 0x55;
    $carr[$i++] = 0xCC;

    for ($o = 0; $o < $outputCount; $o++) {
        $cur = $pixels[$o];

        // Active
        $active = 0;
        $carr[$i++] = 0;
        if ($cur['active']) {
            $active = 1;
            $carr[$i - 1] = 1;
        }

        // Start Address
        $startAddress = intval($cur['address']);
        $carr[$i++] = $startAddress % 256;
        $carr[$i++] = $startAddress / 256;

        // Type
        $type = intval($cur['type']);
        $carr[$i++] = $type;
    }
    $f = fopen($settings['configDirectory'] . "/Falcon.FPDV1", "wb");
    fwrite($f, implode(array_map("chr", $carr)), 1024);

    fclose($f);
    SendCommand('w');
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
    $rc = array("status" => "ERROR: File not found");

    $jsonStr = "";

    if (!isset($settings[$file])) {
        $rc['status'] = "Invalid file $file";
    } else if (file_exists($settings[$file])) {
        $rc = json_decode(file_get_contents($settings[$file]), true);
        $rc["status"] = "OK";
    }

    return json($rc);
}

// GET /api/channel/output/PixelnetDMX
function channel_get_pixelnetDMX()
{
    global $settings;
    $f = fopen($settings['configDirectory'] . "/Falcon.FPDV1", "rb");
    $dataFile = null;

    if ($f == false) {
        fclose($f);
        //No file exists add one and save to new file.
        $address = 1;
        for ($i; $i < 12; $i++) {
            $dataFile[] = new PixelnetDMXentry(1, 0, $address);
            $address += 4096;
        }
    } else {
        $s = fread($f, 1024);
        fclose($f);
        $sarr = unpack("C*", $s);

        $dataOffset = 7;

        $i = 0;
        for ($i = 0; $i < 12; $i++) {
            $outputOffset = $dataOffset + (4 * $i);
            $active = $sarr[$outputOffset + 0];
            $startAddress = $sarr[$outputOffset + 1];
            $startAddress += $sarr[$outputOffset + 2] * 256;
            $type = $sarr[$outputOffset + 3];
            $dataFile[] = new PixelnetDMXentry($active, $type, $startAddress);
        }
    }

    $rc = array();
    $i = 0;
    for ($i = 0; $i < count($dataFile); $i++) {
        $cur = array();
        $cur["active"] = $dataFile[$i]->active;
        $cur["type"] = $dataFile[$i]->type;
        $cur["startAddress"] = $dataFile[$i]->startAddress;
        array_push($rc, $cur);
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
