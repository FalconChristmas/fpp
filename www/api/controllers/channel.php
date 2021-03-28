<?php

require_once('../pixelnetdmxentry.php');


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
    $dataFile = NULL;

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
    for($i=0; $i<count($dataFile); $i++) {
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
