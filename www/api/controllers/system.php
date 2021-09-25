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
    global $settingsFile, $SUDO;
    $rc = "Already Running";
    $status = exec("if ps cax | grep -q fppd; then echo \"true\"; else echo \"false\"; fi");
    if ($status == 'false') {
        exec($SUDO . " /opt/fpp/scripts/fppd_start");
        $rc = "OK";
    }
    $output = array("status" => $rc);
    return json($output);
}

function StopFPPDNoStatus()
{
    global $SUDO;

    // Stop Playing
    SendCommand('d');

    // Shutdown
    SendCommand('q'); // Ignore return and just kill if 'q' doesn't work...
    // wait half a second for shutdown and outputs to close

    if (file_exists("/.dockerenv")) {
        usleep(500000);
        // kill it if it's still running
        exec($SUDO . " " . dirname(dirname(__FILE__)) . "/scripts/fppd_stop");
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
     * NOTE: There are some weird things here to provide backwards compatability
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

    //if the ip= argument supplied
    if (isset($_GET['ip'])) {
        $ipAddresses = $_GET['ip'];
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
                //Make the request - also send across whether advancedView data is requested so it's returned all in 1 request

                // TODO: For FPPD 6.0, this should be moved to the new API.
                // On 5.0 Boxes it is just a redirect back to http://localhost/api/system/status
                // Retained for backwards compatibility
                $curl = curl_init("http://" . $ip . "/fppjson.php?command=getFPPstatus&advancedView=true");
                curl_setopt($curl, CURLOPT_FAILONERROR, true);
                curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
                curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
                curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 500);
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

            if ($request_content === false || $request_content == null || $request_content == "") {
                $responseCode = curl_getinfo($curl, CURLINFO_RESPONSE_CODE);
                $result[$ip] = array_merge(array(), $default_return_json);
                if ($responseCode == 401) {
                    $result[$ip]['reason'] = "Cannot Access - Web GUI Password Set";
                    $result[$ip]["status_name"] = "password";
                } else if ($responseCode == 0) {
                    $result[$ip]["status_name"] = "unreachable";
                }
            } else if (strpos($request_content, 'Not Running') !== false) {
                $result[$ip]["status_name"] = "not running";
            } else {
                $content = json_decode($request_content);

                if (strpos($request_content, 'advancedView') === false) {
                    //Work around for older versioned devices where the advanced data was pulled in separately rather than being
                    //included (when requested) with the standard data via getFPPStatus
                    //If were in advanced view and the request_content doesn't have the 'advancedView' key (this is included when requested with the standard data) then we're dealing with a older version
                    //that's using the expertView key and was being obtained separately

                    $curl2 = curl_init("http://" . $ip . "/fppjson.php?command=getSysInfo");
                    curl_setopt($curl2, CURLOPT_FAILONERROR, true);
                    curl_setopt($curl2, CURLOPT_FOLLOWLOCATION, true);
                    curl_setopt($curl2, CURLOPT_RETURNTRANSFER, true);
                    curl_setopt($curl2, CURLOPT_CONNECTTIMEOUT_MS, 250);
                    curl_setopt($curl2, CURLOPT_TIMEOUT_MS, 3000);
                    $request_expert_content = curl_exec($curl2);
                    curl_close($curl2);
                    //check we have valid data
                    if ($request_expert_content === false) {
                        $request_expert_content = array();
                    }
                    //Add data into the final response, since getFPPStatus returns JSON, decode into array, add data, encode back to json
                    //Add a new key for the advanced data, also decode it as it's an array
                    $content['advancedView'] = json_decode($request_expert_content, true);
                }
                $result[$ip] = $content;
            }
            curl_multi_remove_handle($curlmulti, $curl);
        }
        curl_multi_close($curlmulti);
        if (!$isArray) {
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

//
// This function adds some local information to the multi-sync result
// That doesn't come from fppd
//
function finalizeStatusJson($obj)
{
    $obj['wifi'] = network_wifi_strength_obj();
    $obj['interfaces'] = network_list_interfaces_obj();

    //Get the advanced info directly as an array
    $request_expert_content = GetSystemInfoJsonInternal(true, false);
    //check we have valid data
    if ($request_expert_content === false) {
        $request_expert_content = array();
    }
    //Add data into the final response, since we have the status as an array already then just add the expert view
    //Add a new key for the expert data to the original data array
    $obj['advancedView'] = $request_expert_content;

    return $obj;
}

function GetSystemInfoJsonInternal($return_array = false, $simple = false)
{
    global $settings;

    //close the session before we start, this removes the session lock and lets other scripts run
    session_write_close();

    //Default json to be returned
    $result = array();
    $result['HostName'] = $settings['HostName'];
    $result['HostDescription'] = !empty($settings['HostDescription']) ? $settings['HostDescription'] : "";
    $result['Platform'] = $settings['Platform'];
    $result['Variant'] = isset($settings['Variant']) ? $settings['Variant'] : '';
    $result['Mode'] = $settings['fppMode'];
    $result['multisync'] = isset($settings['MultiSyncEnabled']) ? ($settings['MultiSyncEnabled'] == '1') : false;
    if ($result['Mode'] == "master") {
        $result['Mode'] = "player";
        $result['multisync'] = true;
    }
    $result['Version'] = getFPPVersion();
    $result['Branch'] = getFPPBranch();
    $result['OSVersion'] = trim(file_get_contents('/etc/fpp/rfs_version'));

    $os_release = "Unknown";
    if (file_exists("/etc/os-release")) {
        $info = parse_ini_file("/etc/os-release");
        if (isset($info["PRETTY_NAME"])) {
            $os_release = $info["PRETTY_NAME"];
        }

        unset($output);
    }
    $result['OSRelease'] = $os_release;

    if (file_exists($settings['mediaDirectory'] . "/fpp-info.json")) {
        $content = file_get_contents($settings['mediaDirectory'] . "/fpp-info.json");
        $json = json_decode($content, true);
        $result['channelRanges'] = $json['channelRanges'];
        $result['majorVersion'] = $json['majorVersion'];
        $result['minorVersion'] = $json['minorVersion'];
        $result['typeId'] = $json['typeId'];
    }

    if (!$simple) {
        //Get CPU & memory usage before any heavy processing to try get relatively accurate stat
        $result['Utilization']['CPU'] = get_server_cpu_usage();
        $result['Utilization']['Memory'] = get_server_memory_usage();
        $result['Utilization']['Uptime'] = get_server_uptime(true);

        $uploadDir = GetDirSetting("uploads"); // directory under media
        $result['Utilization']['Disk']["Media"]['Free'] = disk_free_space($uploadDir);
        $result['Utilization']['Disk']["Media"]['Total'] = disk_total_space($uploadDir);
        $result['Utilization']['Disk']["Root"]['Free'] = disk_free_space("/");
        $result['Utilization']['Disk']["Root"]['Total'] = disk_total_space("/");

        $result['Kernel'] = get_kernel_version();
        $result['LocalGitVersion'] = get_local_git_version();
        $result['RemoteGitVersion'] = get_remote_git_version(getFPPBranch());

        if (isset($settings['UpgradeSource'])) {
            $result['UpgradeSource'] = $settings['UpgradeSource'];
        } else {
            $result['UpgradeSource'] = 'github.com';
        }

        $output = array();
        $IPs = array();
        exec("ip --json -4 address show", $output);
        //print(join("", $output));
        $ipAddresses = json_decode(join("", $output), true);
        foreach ($ipAddresses as $key => $value) {
            if ($value["ifname"] != "lo" && strpos($value["ifname"], 'usb') === false) {
                foreach ($value["addr_info"] as $key2 => $value2) {
                    $IPs[] = $value2["local"];
                }
            }
        }

        $result['IPs'] = $IPs;
    }

    //Return just the array if requested
    if ($return_array == true) {
        return $result;
    } else {
        return json($result);
    }
}
