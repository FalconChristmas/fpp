<?

/////////////////////////////////////////////////////////////////////////////
// GET /api/cape
function GetCapeInfo()
{
    global $settings;
    if (isset($settings['cape-info'])) {
        return json($settings['cape-info']);
    }

    halt(404, "No Cape!");
}

/////////////////////////////////////////////////////////////////////////////
// GET /api/cape/options
function GetCapeOptions()
{
    global $settings;
    $js = array();
    $js[] = "--None--";

    $directory = $settings["fppDir"] . "/capes";
    if ($settings['Platform'] == "Raspberry Pi") {
        $directory = $directory . "/pi";
    } else if (strpos($settings['SubPlatform'], 'PocketBeagle') !== false) {
        $directory = $directory . "/pb";
    } else {
        $directory = $directory . "/bbb";
    }

    foreach (scandir($directory) as $file) {
        if (strlen($file) > 11 && substr($file, -11) === "-eeprom.bin") { //at least ends in "-eeprom.bin"
            $js[] = substr($file, 0, strlen($file) - 11);
        }
    }

    return json($js);
}

/////////////////////////////////////////////////////////////////////////////
function GetEEPROMFilename()
{
    global $settings;

    $eepromFile = "/sys/bus/i2c/devices/1-0050/eeprom";
    if ($settings['Platform'] == "BeagleBone Black") {
        $eepromFile = "/sys/bus/i2c/devices/2-0050/eeprom";
        if (!file_exists($eepromFile)) {
            $eepromFile = "/sys/bus/i2c/devices/1-0050/eeprom";
        }
    } else if (file_exists("/home/fpp/media/config/cape-eeprom.bin")) {
        $eepromFile = "/home/fpp/media/config/cape-eeprom.bin";
    }

    return $eepromFile;
}

function GetSigningDataHelper($returnArray = false)
{
    $key = strtoupper(params('key'));
    $order = params('order');

    $result = Array();

    if (preg_match("/[^-A-Z0-9]/", $key)) {
        $result['Status'] = 'ERROR';
        $result['Message'] = 'Invalid Key.  Key must contain only letters, numbers, and hyphens.';
        return json($result);
    }

    if (!preg_match('/^[1-9][0-9]*$/', $order)) {
        $result['Status'] = 'ERROR';
        $result['Message'] = 'Invalid Order ID.  Order ID must contain only numbers.';
        return json($result);
    }

    $eepromData = '';
    $eepromFile = GetEEPROMFilename();
    if (file_exists($eepromFile)) {
        if (!$fh = fopen($eepromFile, 'rb')) {
            $result['Status'] = 'ERROR';
            $result['Message'] = 'Could not open EEPROM ' . $eepromFile;
            return json($result);
        }

        $eepromData = fread($fh, 32768);
        fclose($fh);
    } else {
        $result['Status'] = 'ERROR';
        $result['Message'] = 'Could not locate EEPROM or virtual EEPROM.';
        return json($result);
    }

    $serialNumber = "";
    if (file_exists("/proc/cpuinfo")) {
        $serialNumber = exec("sed -n 's/^Serial.*: //p' /proc/cpuinfo", $output, $return_val);
        if ($return_val != 0) {
            $serialNumber = "";
        }
    }

    $data = Array();
    $data['key'] = $key;
    $data['orderID'] = $order;
    $data['serial'] = $serialNumber;
    $data['eeprom'] = base64_encode($eepromData);

    return $data;
}

/////////////////////////////////////////////////////////////////////////////
// GET /api/cape/eeprom/signingData/:key/:order
function GetSigningData()
{
    $data = GetSigningDataHelper(true);

    if (!isset($data['key']))
        return $data;

    return json($data);
}

/////////////////////////////////////////////////////////////////////////////
// GET /api/cape/eeprom/signingFile/:key/:order
function GetSigningFile()
{
    global $settings;

    $data = GetSigningDataHelper(true);

    if (!isset($data['key']))
        return $data;

    header('Content-type: application/binary');
    header('Content-disposition: attachment;filename="cape-signing-' . $settings['HostName'] . '.bin"');

    return json_encode($data);
}

/////////////////////////////////////////////////////////////////////////////
function SignEEPROMHelper($data)
{
    # Backup the current EEPROM
    $date = new DateTime();
    $timestamp = date_format($date, 'Ymd-His');
    $eepromFile = GetEEPROMFilename();
    $backup = '/home/fpp/media/upload/cape-eeprom-Backup-' . $timestamp . '.bin';
    $newFile = '/home/fpp/media/upload/cape-eeprom-Signed-' . $timestamp . '.bin';

    exec("dd bs=32K if=$eepromFile of=$backup");

    if (file_exists($backup)) {
        # Write out new eeprom
        if (!$fh = fopen($newFile, 'w+b')) {
            $result['Status'] = 'ERROR';
            $result['Message'] = "Could not open new EEPROM file $newFile, unable to sign EEPROM.";
            return json($result);
        }

        $eepromData = base64_decode($data['eeprom']);

        fwrite($fh, $eepromData);
        fclose($fh);

        # Copy the new EEPROM data into place
        exec("sudo dd bs=32K if=$newFile of=$eepromFile");

        unlink($newFile);

        exec('sudo /opt/fpp/src/fppcapedetect');
# FIXME, uncomment before commit
#        exec('sudo systemctl restart fppd');

        $result['Status'] = 'OK';
        $result['Message'] = 'EEPROM Signed.';
    } else {
        $result['Status'] = 'ERROR';
        $result['Message'] = "Could not open backup EEPROM file $backup, unable to sign EEPROM.";
        return json($result);
    }

    return json($result);
}

/////////////////////////////////////////////////////////////////////////////
// POST /api/cape/eeprom/sign/:key/:order
function SignEEPROM()
{
    $APIhost = 'api.FalconPlayer.com';
    if (isset($settings['SigningAPIHost']))
        $APIhost = $settings['SigningAPIHost'];

    $url = "https://$APIhost/api/fpp/eeprom/sign";
    $result = Array();

    $data = GetSigningDataHelper(true);

    if (!isset($data['key']))
        return $data;

    $options = Array(
        'http' => Array(
            'method' => 'POST',
            'header' => 'Content-Type: application/json',
            'content' => json_encode($data)
        )
    );

    $context = stream_context_create($options);
    $replyStr = file_get_contents($url, false, $context);

    if ($replyStr === FALSE) {
        $result['Status'] = 'ERROR';
        $result['Message'] = "Could not contact signing website https://$APIhost";
        return json($result);
    }

    $reply = json_decode($replyStr, true);

    if ($reply['Status'] != 'OK') {
        $result['Status'] = 'ERROR';
        $result['Message'] = $reply['Message'];
        return json($result);
    }

    return SignEEPROMHelper($reply);
}

/////////////////////////////////////////////////////////////////////////////
// POST /api/cape/eeprom/signingData
function PostSigningData()
{
    $dataFile = '';
    $postJSON = '';

    if (isset($_FILES['signingPacket']) && isset($_FILES['signingPacket']['tmp_name'])) {
        $postdata = fopen($_FILES['signingPacket']['tmp_name'], "rb");
        $postJSON = fread($postdata, 32768);
        fclose($postdata);

        if (isset($_FILES['signingPacket']['name'])) {
            $dataFile = $_FILES['signingPacket']['name'];
        } else {
            $dataFile = 'cape-eeprom.bin';
        }
    } else {
        $postdata = fopen("php://input", "r");
        while ($data = fread($postdata, 1024*16)) {
            $postJSON .= $data;
        }
        fclose($postdata);
    }

    $data = json_decode($postJSON, true);

    return SignEEPROMHelper($data);
}

?>
