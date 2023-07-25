<?

require_once "../common/settings.php";
require_once '../commandsocket.php';

/////////////////////////////////////////////////////////////////////////////
// GET /api/system/reboot
function RebootDevice()
{
    global $settings;
    global $SUDO;

    if (file_exists("/.dockerenv")) {
        // apache is container's long-running process, send it a SIGTERM
        $status = exec($SUDO . " killall -15 apache2");
    } else {
        $status = exec($SUDO . " bash -c '{ sleep 1; shutdown -r now; }  > /dev/null 2>&1 &'");
    }

    $output = array("status" => "OK");
    return json($output);
}

// GET /api/system/shutdown
function SystemShutdownOS()
{
    global $SUDO;

    if (file_exists("/.dockerenv")) {
        // apache is container's long-running process, send it a SIGTERM
        $status = exec($SUDO . " killall -15 apache2");
    } else {
        $status = exec($SUDO . " bash -c '{ sleep 1; shutdown -h now; }  > /dev/null 2>&1 &'");
    }

    $output = array("status" => "OK");
    return json($output);
}

// GET /api/system/fppd/start
function StartFPPD()
{
    global $settingsFile, $SUDO, $fppDir, $settings;
    $rc = "Already Running";
    $status = exec("if ps cax | grep -q fppd; then echo \"true\"; else echo \"false\"; fi");
    if ($status == 'false') {
        if ($settings["Platform"] == "MacOS") {
            exec("launchctl start falconchristmas.fppd");
        } else {
            exec($SUDO . " " . $fppDir . "/scripts/fppd_start");
        }
        $rc = "OK";
    }
    $output = array("status" => $rc);
    return json($output);
}

function StopFPPDNoStatus()
{
    global $SUDO, $settings;

    // Stop Playing
    SendCommand('d');

    // Shutdown
    SendCommand('q'); // Ignore return and just kill if 'q' doesn't work...
    // wait half a second for shutdown and outputs to close

    if (file_exists("/.dockerenv")) {
        usleep(500000);
        // kill it if it's still running
        exec($SUDO . " " . dirname(dirname(__FILE__)) . "/scripts/fppd_stop");
    } else if ($settings["Platform"] == "MacOS") {
        exec("launchctl stop falconchristmas.fppd");
    } else {
        // systemctl uses fppd_stop to stop fppd, but we need systemctl to know
        exec($SUDO . " systemctl stop fppd");
    }
}

// GET /api/system/fppd/start

function StopFPPD()
{
    StopFPPDNoStatus();
    $output = array("status" => "OK");
    return json($output);
}

// GET /api/system/fppd/restart
function RestartFPPD()
{
    global $_GET;
    if ((isset($_GET['quick'])) && ($_GET['quick'] == 1)) {
        $status = exec("if ps cax | grep -q fppd; then echo \"true\"; else echo \"false\"; fi");
        if ($status == 'true') {
            SendCommand('restart');

            $output = array("status" => "OK");
            return json($output);
        }
    }
    StopFPPDNoStatus();
    return StartFPPD();
}

// GET /api/system/releaseNotes/:version
function ViewReleaseNotes()
{
    $version = params('version');

    ini_set('user_agent', 'Mozilla/4.0 (compatible; MSIE 6.0)');
    $json = file_get_contents("https://api.github.com/repos/FalconChristmas/fpp/releases/tags/" . $version);

    $data = json_decode($json, true);
    if (isset($data['body'])) {
        $keys = array("draft", "prerelease", "body", "published_at");
        $rc = array("status" => "OK");
        foreach ($keys as $key) {
            $rc[$key] = $data[$key];
        }
        return json($rc);
    } else {
        return json(array("status" => "Release not found"));
    }
}

// PUT /system/volume
function SystemSetAudio()
{
    $rc = "OK";
    $json = strval(file_get_contents('php://input'));
    $input = json_decode($json, true);
    $vol = 75;
    if (isset($input['volume'])) {
        $vol = intval($input['volume']);
    }

    if ($vol < 0) {
        $vol = 0;
    }
    if ($vol > 100) {
        $vol = 100;
    }
    setVolume($vol);
    return json(array("status" => $rc, "volume" => $vol));
}

// GET /system/volume
function SystemGetAudio()
{
    global $settings;

    $curl = curl_init('http://localhost:32322/fppd/status');
    curl_setopt($curl, CURLOPT_FAILONERROR, true);
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_TCP_FASTOPEN, true);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 200);
    $request_content = curl_exec($curl);
    curl_close($curl);

    $method = "FPPD";
    $vol = 75;
    if ($request_content === false) {
        $method = "Default";
        if (isset($settings['volume'])) {
            $vol = $settings['volume'];
            $method = "Settings";
        }
    } else {
        $data = json_decode($request_content, true);
        $vol = $data['volume'];
    }

    return json(array("status" => "OK", "method" => $method, "volume" => $vol));
}

// GET /api/system/status
function SystemGetStatus()
{
    global $_GET;

    /*
     * NOTE: There are some weird things here to provide backwards compatibility
     *  - wifi data is returned in both "wifi"  and "interfaces" s
     *  - IP addresses are listed in both "interfaces" and "advanceView"."IP"
     */

    //close the session before we start, this removes the session lock and lets other scripts run
    session_write_close();

    //Default json to be returned
    $default_return_json = array(
        'fppd' => 'unknown',
        'status' => 'stopped',
        'wifi' => [],
        'interfaces' => [],
        'status_name' => 'unknown',
        'current_playlist' =>
        [
            'playlist' => '',
            'type' => '',
            'index' => '0',
            'count' => '0',
        ],
        'current_sequence' => '',
        'current_song' => '',
        'seconds_played' => '0',
        'seconds_remaining' => '0',
        'time_elapsed' => '00:00',
        'time_remaining' => '00:00'
    );

    $default_return_json['uuid'] = stats_getUUID();

    //if the ip= argument supplied
    if (isset($_GET['ip'])) {
        $ipAddresses = $_GET['ip'];
        $type = "FPP";
        if (isset($_GET['type'])) {
            $type = $_GET['type'];
        }
        $isArray = true;
        if (!is_array($ipAddresses)) {
            $ipAddresses = array();
            $ipAddresses[] = $_GET['ip'];
            $isArray = false;
        }

        $result = array();

        $curlmulti = curl_multi_init();
        $curls = array();
        foreach ($ipAddresses as $ip) {
            //validate IP address is a valid IPv4 address - possibly overkill but have seen IPv6 addresses polled
            if (filter_var($ip, FILTER_VALIDATE_IP)) {
                if ($type == "Genius") {
                    $curl = curl_init("http://" . $ip . "/api/state");
                } else if ($type == "WLED") {
                    $curl = curl_init("http://" . $ip . "/json/info");
                } else if ($type == "FV3") {
                    $curl = curl_init("http://" . $ip . "/status.xml");
                } else if ($type == "FV4") {
                    $curl = curl_init("http://" . $ip . "/api");
                    $data = '{"T":"Q","M":"ST","B":0,"E":0,"I":0,"P":{}}';
                    curl_setopt($curl, CURLOPT_POSTFIELDS, $data);
                    curl_setopt($curl, CURLOPT_HTTPHEADER, array('Content-Type:application/json'));
                } else {
                    $curl = curl_init("http://" . $ip . "/api/system/status");
                }
                curl_setopt($curl, CURLOPT_FAILONERROR, true);
                curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
                curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
                curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 500);
                curl_setopt($curl, CURLOPT_TCP_FASTOPEN, true);
                curl_setopt($curl, CURLOPT_TIMEOUT_MS, 3000);
                $curls[$ip] = $curl;
                curl_multi_add_handle($curlmulti, $curl);
            }
        }
        $running = null;
        do {
            curl_multi_exec($curlmulti, $running);
        } while ($running > 0);

        foreach ($curls as $ip => $curl) {
            $request_content = curl_multi_getcontent($curl);
            curl_multi_remove_handle($curlmulti, $curl);
            $responseCode = 200;
            if ($request_content === false || $request_content == null || $request_content == "") {
                $responseCode = curl_getinfo($curl, CURLINFO_RESPONSE_CODE);
                $result[$ip] = array_merge(array(), $default_return_json);
                if ($responseCode == 401) {
                    $result[$ip]['reason'] = "Cannot Access - Web GUI Password Set";
                    $result[$ip]["status_name"] = "password";
                } else if ($responseCode == 404 && $type == "FPP") {
                    // old device, possibly ESPixelStick, maybe FPP4
                    $curl = curl_init("http://" . $ip . "/fppjson.php?command=getFPPstatus&advancedView=true");
                    curl_setopt($curl, CURLOPT_FAILONERROR, true);
                    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
                    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
                    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 500);
                    curl_setopt($curl, CURLOPT_TCP_FASTOPEN, true);
                    curl_setopt($curl, CURLOPT_TIMEOUT_MS, 3000);
                    $request_content = curl_exec($curl);
                    $responseCode = curl_getinfo($curl, CURLINFO_RESPONSE_CODE);
                } else if ($responseCode == 0) {
                    $result[$ip]["status_name"] = "unreachable";
                }
            }
            if ($responseCode == 200) {
                if (strpos($request_content, 'Not Running') !== false) {
                    $result[$ip]["status_name"] = "not running";
                } else {
                    if ($type == "FV3") {
                        $content = simplexml_load_string($request_content);
                    } else {
                        $content = json_decode($request_content, true);
                    }
                    if (!isset($content["status_name"])) {
                        $content["status_name"] = "Running";
                    }
                    $result[$ip] = $content;
                }
            }
        }
        curl_multi_close($curlmulti);
        if (!$isArray && count($ipAddresses) > 0 && $ipAddresses[0] != "") {
            $result = $result[$ipAddresses[0]];
        }
        return json($result);
    } else {
        // If IP[] was not provided, then status of the local machine is desired.
        //go through the new API to get the status
        // use curl so we can set a low connect timeout so if fppd isn't running we detect that quickly
        $curl = curl_init('http://localhost:32322/fppd/status');
        curl_setopt($curl, CURLOPT_FAILONERROR, true);
        curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
        curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
        curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 200);
        curl_setopt($curl, CURLOPT_TCP_FASTOPEN, true);
        curl_setopt($curl, CURLOPT_TIMEOUT_MS, 3000);
        $request_content = curl_exec($curl);
        curl_close($curl);

        if ($request_content === false) {
            $status = exec("if ps cax | grep -q git_pull; then echo \"updating\"; else echo \"false\"; fi");

            $default_return_json['fppd'] = "Not Running";
            $default_return_json['status_name'] = $status == 'updating' ? $status : 'stopped';

            return json(finalizeStatusJson($default_return_json));
        }
        $data = json_decode($request_content, true);
        return json(finalizeStatusJson($data));
    }
}
// GET /api/system/info
function SystemGetInfo()
{
    $result = GetSystemInfoJsonInternal(isset($_GET['simple']));
    return json($result);
}

//
// This function adds some local information to the multi-sync result
// That doesn't come from fppd
//
function finalizeStatusJson($obj)
{
    global $settings;

    $obj['wifi'] = network_wifi_strength_obj();
    $obj['interfaces'] = network_list_interfaces_obj();

    if (isset($settings['rebootFlag'])) {
        $obj['rebootFlag'] = intval($settings['rebootFlag']);
    } else {
        $obj['rebootFlag'] = 0;
    }

    if (isset($settings['restartFlag'])) {
        $obj['restartFlag'] = intval($settings['restartFlag']);
    } else {
        $obj['restartFlag'] = 0;
    }

    //Get the advanced info directly as an array
    $request_expert_content = GetSystemInfoJsonInternal(false);
    //check we have valid data
    if ($request_expert_content === false) {
        $request_expert_content = array();
    }
    //Add data into the final response, since we have the status as an array already then just add the expert view
    //Add a new key for the expert data to the original data array
    $obj['advancedView'] = $request_expert_content;

    if (is_dir($settings['mediaDirectory'] . "/crashes")) {
        $num = count(glob($settings['mediaDirectory'] . "/crashes/*.zip"));
        if ($num > 0) {
            $plural = 's';
            $verb = 'are';
            if ($num == 1) {
                $plural = '';
                $verb = 'is';
            }

            if ($settings['ShareCrashData'] == '0') {
                $obj["warnings"][] = "There $verb $num <a href='uploadfile.php#tab-crashes'>crash report$plural</a> available, please submit to FPP developers or delete.";
            } else {
                $obj["warnings"][] = "There $verb $num <a href='uploadfile.php#tab-crashes'>crash report$plural</a> available. " .
                    "This system is configured to automatically upload these to the FPP developers, you may delete the old reports at any time.";
            }
        }
    }

    return $obj;
}

// POST api/system/proxies
function SystemSetProxies()
{
    global $mediaDirectory;
    $json = file_get_contents('php://input');
    $data = json_decode($json);
    $newht = "RewriteEngine on\nRewriteBase /proxy/\n\n";

    foreach ($data as $host => $desc) {
        if ($desc != "") {
            $newht = $newht . "# D:" . $desc . "\n";
        }
        $newht = $newht . "RewriteRule ^" . $host . "$  " . $host . "/  [R,L]\n";
        $newht = $newht . "RewriteRule ^" . $host . "/(.*)$  http://" . $host . "/$1  [P,L]\n\n";
    }

    $newht = $newht . "RewriteRule ^(.*)/(.*)$  http://$1/$2  [P,L]\n";
    $newht = $newht . "RewriteRule ^(.*)$  $1/  [R,L]\n\n";

    file_put_contents("$mediaDirectory/config/proxies", $newht);

    //Trigger a JSON Configuration Backup
    GenerateBackupViaAPI('Proxy hosts were modified.');
    return "OK";
}
