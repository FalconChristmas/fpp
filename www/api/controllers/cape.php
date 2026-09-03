<?

/**
 * Get cape information
 *
 * Returns the cape information for the currently detected hardware cape
 * (from `cape-info` settings).
 *
 * @route GET /api/cape
 * @response 200 Cape hardware information
 * ```json
 * {
 *   "id": "K16A-Bv1",
 *   "name": "K16A-B",
 *   "description": "K16A-B is a cape for the BeagleBone Black...",
 *   "version": "1.0",
 *   "designer": "Daniel Kulp",
 *   "vendor": {
 *     "name": "Kulp Lights",
 *     "url": "https://kulplights.com/",
 *     "email": "sales@kulplights.com",
 *     "image": "https://kulplights.com/images/kulplights_small.png"
 *   },
 *   "provides": ["strings"],
 *   "serialNumber": "XXXXXXXXXXXXXX",
 *   "validEepromLocation": true,
 *   "verifiedKeyId": "dk",
 *   "eepromLocation": "/sys/bus/i2c/devices/2-0050/eeprom",
 *   "modules": ["gpio_pcf857x", "pcm5102a", "lm75"],
 *   "i2cDevices": ["pca9675 0x20", "pcf8523 0x68", "lm75 0x48"],
 *   "defaultSettings": {
 *     "LEDDisplayType": "1",
 *     "piRTC": "4",
 *     "showAllOptions": "0"
 *   }
 * }
 * ```
 * @response 404 No cape detected
 * ```json
 * {"id": "No Cape!"}
 * ```
 */
function GetCapeInfo()
{
    global $settings;
    if (isset($settings['cape-info'])) {
        return json($settings['cape-info']);
    }
    http_response_code(404);
    header("Content-Type: application/json");
    echo "{\"id\": \"No Cape!\"}";
}

/**
 * Get the downloadable EEPROM lists of every known cape vendor
 *
 * Fetches the vendor index from fpp-data and then each vendor's own list, all
 * server-side, so the browser makes one same-origin request.  Doing this from
 * the browser needed every vendor host in the CSP connect-src AND sending CORS
 * headers, and most vendor hosts do neither (issue seen with a vendor list
 * hosted on the bare domain while the CSP carried the www. form).  The eeprom
 * file itself was already downloaded server-side by upgradeCapeFirmware.
 *
 * @route GET /api/cape/eeprom/vendors
 * @response 200 Vendor lists, each in the vendor's own file format
 * ```json
 * {"vendors": [{"name": "...", "url": "...", "capes": {...}}], "errors": ["..."]}
 * ```
 */
function GetEEPROMVendorLists()
{
    $indexURL = 'https://raw.githubusercontent.com/FalconChristmas/fpp-data/master/eepromVendors.json';
    $result = array('vendors' => array(), 'errors' => array());

    $mkHandle = function ($url) {
        $c = curl_init($url);
        curl_setopt($c, CURLOPT_HEADER, 0);
        curl_setopt($c, CURLOPT_CONNECTTIMEOUT, 5);
        curl_setopt($c, CURLOPT_TIMEOUT, 15);
        curl_setopt($c, CURLOPT_USERAGENT, getFPPVersion());
        curl_setopt($c, CURLOPT_RETURNTRANSFER, true);
        curl_setopt($c, CURLOPT_FOLLOWLOCATION, true);
        curl_setopt($c, CURLOPT_MAXREDIRS, 3);
        curl_setopt($c, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
        return $c;
    };

    $c = $mkHandle($indexURL);
    $body = curl_exec($c);
    $rc = curl_getinfo($c, CURLINFO_RESPONSE_CODE);
    curl_close($c);
    $index = ($body !== false && $rc == 200) ? json_decode($body, true) : null;
    if (!is_array($index) || !isset($index['vendors']) || !is_array($index['vendors'])) {
        $result['errors'][] = "Could not fetch the vendor index from $indexURL (HTTP $rc)";
        $result['index'] = null;
        return json($result);
    }
    $result['index'] = $index;

    // Cache the index for the CSP generator, which adds every vendor list host
    // to connect-src so the browser-side fallback (an offline box with an
    // online browser) is allowed to fetch them directly.  Only when it changed,
    // since applying it means a graceful apache reload.
    global $settings, $fppDir, $SUDO;
    $cacheFile = $settings['mediaDirectory'] . '/tmp/eepromVendors.json';
    $old = file_exists($cacheFile) ? file_get_contents($cacheFile) : '';
    if ($old !== $body) {
        if (file_put_contents($cacheFile, $body) !== false) {
            exec($SUDO . ' ' . escapeshellarg($fppDir . '/scripts/ManageApacheContentPolicy.sh') . ' regenerate > /dev/null 2>&1 &');
        }
    }

    // One handle per vendor list, fetched concurrently so a slow vendor does not
    // serialise behind the others; the per-handle timeout bounds the whole call.
    $multi = curl_multi_init();
    $handles = array();
    foreach ($index['vendors'] as $vendor) {
        $url = isset($vendor['url']) ? trim($vendor['url']) : '';
        if ($url == '' || !preg_match('#^https?://#i', $url)) {
            continue;
        }
        $h = $mkHandle($url);
        curl_multi_add_handle($multi, $h);
        $handles[] = array('url' => $url, 'handle' => $h);
    }
    do {
        $status = curl_multi_exec($multi, $running);
        if ($running) {
            curl_multi_select($multi, 1.0);
        }
    } while ($running && $status == CURLM_OK);

    foreach ($handles as $entry) {
        $h = $entry['handle'];
        $body = curl_multi_getcontent($h);
        $rc = curl_getinfo($h, CURLINFO_RESPONSE_CODE);
        $err = curl_error($h);
        curl_multi_remove_handle($multi, $h);
        curl_close($h);
        $list = ($body !== false && $body !== null && $rc == 200) ? json_decode($body, true) : null;
        if (!is_array($list) || !isset($list['capes'])) {
            $result['errors'][] = $entry['url'] . ': ' . ($err != '' ? $err : "HTTP $rc or not a cape list");
            continue;
        }
        $list['listURL'] = $entry['url'];
        $result['vendors'][] = $list;
    }
    curl_multi_close($multi);

    return json($result);
}

/**
 * Get cape options
 *
 * Returns a list of available cape EEPROM options for the current platform.
 *
 * @route GET /api/cape/options
 * @response 200 Available cape EEPROM options
 * ```json
 * ["--None--", "F16-B", "F32-B", "F4-B", "F8-B", "F8-Bv2", "RGB-123"]
 * ```
 */
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

/**
 * Get EEPROM filename
 *
 * Returns the path to the cape EEPROM file, checking several known locations
 * and creating a virtual EEPROM device if necessary.
 *
 * @return string Absolute path to the EEPROM file, or empty string if not found.
 */
function GetEEPROMFilename()
{
    global $settings;

    $eepromFile = "";
    if (file_exists("/home/fpp/media/tmp/eeprom_location.txt")) {
        $eepromFile = file_get_contents("/home/fpp/media/tmp/eeprom_location.txt");
    }
    if (!file_exists($eepromFile) && str_starts_with($eepromFile, "/sys/bus/i2c/devices/")) {
        $target = "/sys/bus/i2c/devices/i2c-1/new_device";
        if ($settings['BeaglePlatform']) {
            $target = "/sys/bus/i2c/devices/i2c-2/new_device";
        }
        system("sudo bash -c \"echo '24c256 0x50' > $target\"");
    }

    if (!file_exists($eepromFile) && file_exists("/home/fpp/media/config/cape-eeprom.bin")) {
        $eepromFile = "/home/fpp/media/config/cape-eeprom.bin";
    }

    return $eepromFile;
}

/**
 * Reads the cape EEPROM and assembles the signing data payload.
 * Used by `GetSigningData()`, `GetSigningFile()`, and `SignEEPROM()`.
 *
 * @param bool   $returnArray Return data as array (true) or JSON response (false).
 * @param string $key         Signing key; read from request params if empty.
 * @param string $order       Order ID; read from request params if empty.
 * @return array|string       Signing data array, or a JSON error response string.
 */
function GetSigningDataHelper($returnArray = false, $key = '', $order = '')
{
    if (($key == '') && ($order == '')) {
        $key = strtoupper(params('key'));
        $order = params('order');
    } else {
        $key = strtoupper($key);
    }

    $result = array();

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
    $origEEPROM = $eepromFile;
    $tmpEEPROM = '/home/fpp/media/config/tmpEEPROM.bin';
    if (file_exists($eepromFile)) {
        if ($eepromFile != '/home/fpp/media/config/cape-eeprom.bin') {
            exec("sudo dd bs=32K if=$eepromFile of=$tmpEEPROM && sudo chown fpp:fpp $tmpEEPROM");
            $eepromFile = $tmpEEPROM;
        }
        if (!$fh = fopen($eepromFile, 'rb')) {
            $result['Status'] = 'ERROR';
            $result['Message'] = 'Could not open EEPROM ' . $origEEPROM;
            return json($result);
        }

        $eepromData = fread($fh, 32768);
        fclose($fh);

        if (file_exists($tmpEEPROM)) {
            unlink($tmpEEPROM);
        }

        $length = strlen($eepromData);
        if ($length < 58) {
            $result['Status'] = 'ERROR';
            $result['Message'] = 'Invalid EEPROM, length too short.';
            return json($result);
        }

        // Determine the actual data size so we can truncate our copy
        $pos = 58;
        $slen = intval(substr($eepromData, $pos, 6));
        while (($slen != 0) && ($pos < $length)) {
            $pos += 6;
            $flag = intval(substr($eepromData, $pos, 2));
            $pos += 2;
            if ($flag < 50) {
                $pos += 64;
            }
            $pos += $slen;

            // Read the length of the next section
            $slen = intval(substr($eepromData, $pos, 6));
        }
        $length = $pos + 6;

        $eepromData = substr($eepromData, 0, $length);
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
    if (strpos($settings['SubPlatform'], 'PocketBeagle2') !== false && $serialNumber == "") {
        $serialNumber = exec("dd if=/sys/bus/i2c/devices/0-0050/eeprom count=16 skip=40 bs=1 2>/dev/null", $output, $return_val);
        if ($return_val != 0) {
            $serialNumber = "";
        }
    }

    $data = array();
    $data['key'] = $key;
    $data['orderID'] = $order;
    $data['serial'] = $serialNumber;
    $data['eeprom'] = base64_encode($eepromData);

    return $data;
}

/**
 * Get signing data as text
 *
 * Returns the cape EEPROM signing data payload for use with an external signing service.
 *
 * @route GET /api/cape/eeprom/signingData/{key}/{order}
 * @response 200 EEPROM signing data payload
 * ```json
 * {
 *   "key": "ABCD-1234",
 *   "orderID": "42",
 *   "serial": "1000000012345678",
 *   "eeprom": "<base64-encoded binary>"
 * }
 * ```
 */
function GetSigningData()
{
    $data = GetSigningDataHelper(true);

    if (!isset($data['key'])) {
        return $data;
    }

    return json($data);
}

/**
 * Get signing data as binary
 *
 * Downloads the cape EEPROM signing data as a binary file attachment.
 *
 * @route GET /api/cape/eeprom/signingFile/{key}/{order}
 * @response 200 EEPROM signing data as binary file attachment
 * ```bytes
 * [Content-Type: application/octet-stream]
 * ```
 */
function GetSigningFile()
{
    global $settings;

    $data = GetSigningDataHelper(true);

    if (!isset($data['key'])) {
        return $data;
    }

    header('Content-type: application/binary');
    header('Content-disposition: attachment;filename="cape-signing-' . $settings['HostName'] . '.bin"');

    return json_encode($data);
}

/**
 * Writes signed EEPROM data back to the cape, backing up the original first.
 * Used by `PostSigningData()` and `SignEEPROM()`.
 *
 * @param array $data Signed payload containing a base64-encoded 'eeprom' key.
 * @return string JSON response with Status OK or ERROR.
 */
function SignEEPROMHelper($data)
{
    # Backup the current EEPROM
    $date = new DateTime();
    $timestamp = date_format($date, 'Ymd-His');
    $eepromFile = GetEEPROMFilename();
    $backup = '/home/fpp/media/upload/cape-eeprom-Backup-' . $timestamp . '.bin';
    $newFile = '/home/fpp/media/upload/cape-eeprom-Signed-' . $timestamp . '.bin';

    exec("sudo dd bs=32K if=$eepromFile of=$backup && sudo chown fpp:fpp $backup");

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

        exec('sudo /opt/fpp/scripts/detect_cape');

        $result['Status'] = 'OK';
        $result['Message'] = 'EEPROM Signed.';
    } else {
        $result['Status'] = 'ERROR';
        $result['Message'] = "Could not open backup EEPROM file $backup, unable to sign EEPROM.";
        return json($result);
    }

    return json($result);
}

/**
 * Sign EEPROM
 *
 * Signs the cape EEPROM by sending its data to the FalconPlayer.com signing API
 * using the provided `key` and order ID.
 *
 * @route POST /api/cape/eeprom/sign/{key}/{order}
 * @response 200 EEPROM signed successfully
 * ```json
 * {"Status": "OK", "Message": "EEPROM Signed."}
 * ```
 */
function SignEEPROM($key = '', $order = '')
{
    global $settings;

    $APIhost = 'api.FalconPlayer.com';
    if (isset($settings['SigningAPIHost'])) {
        $APIhost = $settings['SigningAPIHost'];
    }

    $url = "https://$APIhost/api/fpp/eeprom/sign";
    $result = array();

    $data = GetSigningDataHelper(true, $key, $order);

    if (!isset($data['key'])) {
        return $data;
    }

    $curl = curl_init($url);
    curl_setopt($curl, CURLOPT_HEADER, 0);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT, 5);
    curl_setopt($curl, CURLOPT_TIMEOUT, 30);
    curl_setopt($curl, CURLOPT_USERAGENT, getFPPVersion());
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
    curl_setopt($curl, CURLOPT_HTTPHEADER, array('Content-Type:application/json'));
    curl_setopt($curl, CURLOPT_POSTFIELDS, json_encode($data));

    $replyStr = curl_exec($curl);
    $rc = curl_getinfo($curl, CURLINFO_RESPONSE_CODE);
    curl_close($curl);

    if ($rc != 200) {
        $replyStr = false;
    }

    if ($replyStr === false) {
        $result['Status'] = 'ERROR';
        $result['Message'] = "Could not contact signing website https://$APIhost.  RC: " . $rc;
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

/**
 * Upload signed EEPROM
 *
 * Accepts a signed EEPROM data payload and writes it back to the cape EEPROM.
 * Accepts either a multipart file upload (`signingPacket`) or a raw JSON body.
 *
 * @route POST /api/cape/eeprom/signingData
 * @body {"key": "ABCD-1234", "orderID": "42", "serial": "1000000012345678", "eeprom": "<base64-encoded binary>"}
 * @response 200 Signed EEPROM written successfully
 * ```json
 * {"Status": "OK", "Message": "EEPROM Signed."}
 * ```
 */
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
        while ($data = fread($postdata, 1024 * 16)) {
            $postJSON .= $data;
        }
        fclose($postdata);
    }

    $data = json_decode($postJSON, true);

    return SignEEPROMHelper($data);
}

/**
 * Redeem signing voucher
 *
 * Redeems a voucher code against the FalconPlayer.com signing API to obtain
 * a signing `key` and order ID.
 *
 * @route POST /api/cape/eeprom/voucher
 * @body {"voucher": "XXXX-XXXX-XXXX-XXXX", "first_name": "John", "last_name": "Doe", "email": "john@example.com", "password": "secret"}
 * @response 200 Voucher redeemed successfully
 * ```json
 * {
 *   "Status": "OK",
 *   "Message": "",
 *   "key": "ABCD-1234",
 *   "order": "42"
 * }
 * ```
 */
function RedeemVoucher()
{
    global $settings;

    $APIhost = 'api.FalconPlayer.com';
    if (isset($settings['SigningAPIHost'])) {
        $APIhost = $settings['SigningAPIHost'];
    }

    $url = "https://$APIhost/api/fpp/voucher/redeem";

    $postJSON = '';
    $postdata = fopen("php://input", "r");
    while ($data = fread($postdata, 1024 * 16)) {
        $postJSON .= $data;
    }
    fclose($postdata);

    $data = json_decode($postJSON, true);

    if (
        (!isset($data['voucher'])) ||
        (!isset($data['first_name'])) ||
        (!isset($data['last_name'])) ||
        (!isset($data['email'])) ||
        (!isset($data['password']))
    ) {
        $result = array();
        $result['Status'] = 'ERROR';
        $result['Message'] = 'Missing input data.  Must include voucher, first_name, last_name, email, password';
        return json($result);
    }

    $curl = curl_init($url);
    curl_setopt($curl, CURLOPT_HEADER, 0);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT, 5);
    curl_setopt($curl, CURLOPT_TIMEOUT, 30);
    curl_setopt($curl, CURLOPT_USERAGENT, getFPPVersion());
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
    curl_setopt($curl, CURLOPT_HTTPHEADER, array('Content-Type:application/json'));
    curl_setopt($curl, CURLOPT_POSTFIELDS, $postJSON);

    $replyStr = curl_exec($curl);
    $rc = curl_getinfo($curl, CURLINFO_RESPONSE_CODE);
    curl_close($curl);

    if ($rc != 200) {
        $replyStr = false;
    }

    if ($replyStr === false) {
        $result['Status'] = 'ERROR';
        $result['Message'] = "Could not contact signing website https://$APIhost.  RC: " .$rc;
        return json($result);
    }

    $reply = json_decode($replyStr, true);

    if ($reply['Status'] != 'OK') {
        $result['Status'] = 'ERROR';
        $result['Message'] = "Signing website returned an error: " . $reply['Message'];
        return json($result);
    }

    $result['Status'] = 'OK';
    $result['Message'] = '';
    $result['key'] = $reply['key'];
    $result['order'] = $reply['order'];

    return json($result);
}

/**
 * Get all cape strings
 *
 * Returns a list of available string cape configuration `key` values.
 *
 * @route GET /api/cape/strings
 * @response 200 Available string cape configuration keys
 * ```json
 * ["F16v3-strings", "F8v2-strings"]
 * ```
 */
function GetCapeStringOptions()
{
    global $settings;
    $js = array();
    $directory = $settings["mediaDirectory"] . "/tmp/strings";
    foreach (scandir($directory) as $file) {
        if (strlen($file) > 5) { // must have .json extension
            $js[] = substr($file, 0, strlen($file) - 5);
        }
    }
    return json($js);
}

/**
 * Get all cape panels
 *
 * Returns a list of available LED panel cape configuration `key` values.
 *
 * @route GET /api/cape/panel
 * @response 200 Available LED panel cape configuration keys
 * ```json
 * []
 * ```
 */
function GetCapePanelOptions()
{
    global $settings;
    $js = array();
    $directory = $settings["mediaDirectory"] . "/tmp/panels";
    foreach (scandir($directory) as $file) {
        if (strlen($file) > 5) { // must have .json extension
            $js[] = substr($file, 0, strlen($file) - 5);
        }
    }
    return json($js);
}

/**
 * Get cape string
 *
 * Returns the string cape configuration JSON for the specified `key`.
 *
 * @route GET /api/cape/strings/{key}
 * @response 200 String cape configuration
 * ```json
 * {
 *   "name": "K16A-B",
 *   "longName": "K16A-B",
 *   "pinoutVersion": "1.x",
 *   "numSerial": 0,
 *   "supportsSmartReceivers": true,
 *   "outputs": [{"pin": "P8-45"}],
 *   "groups": [{"start": 1, "count": 16}],
 *   "serial": []
 * }
 * ```
 * @response 404 Key not found
 * ```json
 * ["Not Found!"]
 * ```
 */
function GetCapeStringConfig()
{
    global $settings;
    $fn = $settings["mediaDirectory"] . "/tmp/strings/" . params('key') . ".json";
    if (file_exists($fn)) {
        $js = json_decode(file_get_contents($fn));
        return json($js);
    }
    http_response_code(404);
    header("Content-Type: application/json");
    echo "[\"Not Found!\"]";
}

/**
 * Get cape panel
 *
 * Returns the LED panel cape configuration JSON for the specified `key`.
 *
 * @route GET /api/cape/panel/{key}
 * @response 200 LED panel cape configuration
 * ```json
 * {}
 * ```
 */
function GetCapePanelConfig()
{
    global $settings;
    $fn = $settings["mediaDirectory"] . "/tmp/panels/" . params('key') . ".json";
    $js = json_decode(file_get_contents($fn));
    return json($js);
}
