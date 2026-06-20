<?

require_once "../common/settings.php";
require_once '../commandsocket.php';

/**
 * Reboot the operating system
 *
 * Reboots the operating system.
 *
 * @route GET /api/system/reboot
 * @response 200 Reboot initiated
 * ```json
 * {"status": "OK"}
 * ```
 */
function RebootDevice()
{
    global $settings;
    global $SUDO;

    if (file_exists("/etc/fpp/container")) {
        // apache is container's long-running process, send it a SIGTERM
        $status = exec($SUDO . " killall -15 apache2");
    } else {
        $status = exec($SUDO . " bash -c '{ sleep 1; shutdown -r now; }  > /dev/null 2>&1 &'");
    }

    $output = array("status" => "OK");
    return json($output);
}

/**
 * Shutdown the operating system
 *
 * Executes a clean shutdown of the operating system.
 *
 * @route GET /api/system/shutdown
 * @response 200 Shutdown initiated
 * ```json
 * {"status": "OK"}
 * ```
 */
function SystemShutdownOS()
{
    global $SUDO;

    if (file_exists("/etc/fpp/container")) {
        // apache is container's long-running process, send it a SIGTERM
        $status = exec($SUDO . " killall -15 apache2");
    } else {
        $status = exec($SUDO . " bash -c '{ sleep 1; shutdown -h now; }  > /dev/null 2>&1 &'");
    }

    $output = array("status" => "OK");
    return json($output);
}

/**
 * Start fppd
 *
 * Starts the `fppd` process idempotently (if it isn't already running).
 *
 * @route GET /api/system/fppd/start
 * @response 200 fppd started
 * ```json
 * {"status": "OK"}
 * ```
 */
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

/**
 * Sends stop commands to `fppd` and kills the process if it does not exit cleanly.
 * Used internally by `StopFPPD()` and `RestartFPPD()`; returns no output.
 */
function StopFPPDNoStatus()
{
    global $SUDO, $settings;

    // Stop Playing
    SendCommand('d');

    // Shutdown
    SendCommand('q'); // Ignore return and just kill if 'q' doesn't work...
    // wait half a second for shutdown and outputs to close

    if (file_exists("/etc/fpp/container")) {
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

/**
 * Stop fppd
 *
 * Stops the `fppd` process if it is running.
 *
 * @route GET /api/system/fppd/stop
 * @response 200 fppd stopped
 * ```json
 * {"status": "OK"}
 * ```
 */
function StopFPPD()
{
    StopFPPDNoStatus();
    $output = array("status" => "OK");
    return json($output);
}

/**
 * Restart fppd process
 *
 * Restarts the `fppd` process. Pass `?quick=1` to reload some configuration without
 * a full restart.
 *
 * @route GET /api/system/fppd/restart
 * @response 200 fppd restarted
 * ```json
 * {"status": "OK"}
 * ```
 */
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

/**
 * Get release notes
 *
 * Returns release notes for the specified FPP version tag from the GitHub releases API.
 *
 * @route GET /api/system/releaseNotes/{version}
 * @response 200 Release notes
 * ```json
 * {
 *   "status": "OK",
 *   "draft": false,
 *   "prerelease": false,
 *   "body": "...",
 *   "published_at": "2026-01-08T03:09:40Z"
 * }
 * ```
 */
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

/**
 * Get fpp upgrade status
 *
 * Returns the current FPP update/upgrade status, including whether a newer version is available,
 * the current commit, and any major version or end-of-life warnings.
 *
 * @route GET /api/system/updateStatus
 * @response 200 FPP upgrade status
 * ```json
 * {
 *   "status": "OK",
 *   "branchUpgradeAvailable": false,
 *   "branchUpgradeTarget": "",
 *   "branchUpgradeVersion": "",
 *   "isMajorVersionUpgrade": false,
 *   "commitUpdateAvailable": false,
 *   "remoteCommit": "ece480e86b7dd8f2d013248e8f99bb0e8baac197",
 *   "currentBranch": "master",
 *   "localCommit": "ece480e86",
 *   "isEndOfLife": false,
 *   "latestMajorVersion": 9
 * }
 * ```
 */
function GetUpdateStatus()
{
    // Test mode: simulate different upgrade scenarios
    // Usage: api/system/updateStatus?test=branch (or commit, both, uptodate, major/eol, osonly)
    // This allows testing the upgrade UI without being on an actual old version
    if (isset($_GET['test'])) {
        $test = $_GET['test'];
        $localCommit = get_local_git_version();
        $currentMajor = getFPPMajorVersion();

        if ($test === 'branch') {
            // Simulate: on v9.4, v9.5 available (minor version upgrade)
            return json(array(
                "status" => "OK",
                "branchUpgradeAvailable" => true,
                "branchUpgradeTarget" => "v9.5",
                "branchUpgradeVersion" => "9.5",
                "isMajorVersionUpgrade" => false,
                "commitUpdateAvailable" => false,
                "remoteCommit" => "",
                "currentBranch" => "v9.4",
                "localCommit" => $localCommit,
                "isEndOfLife" => false,
                "latestMajorVersion" => $currentMajor
            ));
        } else if ($test === 'major' || $test === 'eol') {
            // Simulate: major version upgrade available (e.g., v9.x → v10.0)
            // This also triggers EOL since current major < latest major
            $nextMajor = intval($currentMajor) + 1;
            return json(array(
                "status" => "OK",
                "branchUpgradeAvailable" => true,
                "branchUpgradeTarget" => "v" . $nextMajor . ".0",
                "branchUpgradeVersion" => $nextMajor . ".0",
                "isMajorVersionUpgrade" => true,
                "commitUpdateAvailable" => false,
                "remoteCommit" => "",
                "currentBranch" => "v" . $currentMajor . ".5",
                "localCommit" => $localCommit,
                "isEndOfLife" => true,
                "latestMajorVersion" => $nextMajor
            ));
        } else if ($test === 'commit') {
            // Simulate: on current branch, commits behind
            return json(array(
                "status" => "OK",
                "branchUpgradeAvailable" => false,
                "branchUpgradeTarget" => "",
                "branchUpgradeVersion" => "",
                "isMajorVersionUpgrade" => false,
                "commitUpdateAvailable" => true,
                "remoteCommit" => "def456789",
                "currentBranch" => getFPPBranch(),
                "localCommit" => $localCommit,
                "isEndOfLife" => false,
                "latestMajorVersion" => $currentMajor
            ));
        } else if ($test === 'both') {
            // Simulate: FPP branch upgrade AND OS upgrade both available
            // (exercises the "Recommended: Upgrade OS First" recommendation banner)
            return json(array(
                "status" => "OK",
                "branchUpgradeAvailable" => true,
                "branchUpgradeTarget" => "v9.5",
                "branchUpgradeVersion" => "9.5",
                "isMajorVersionUpgrade" => false,
                "commitUpdateAvailable" => true,
                "remoteCommit" => "def456789",
                "currentBranch" => "v9.4",
                "localCommit" => $localCommit,
                "forceOsUpgradeAvailable" => true,
                "isEndOfLife" => false,
                "latestMajorVersion" => $currentMajor
            ));
        } else if ($test === 'uptodate') {
            // Simulate: everything up to date
            return json(array(
                "status" => "OK",
                "branchUpgradeAvailable" => false,
                "branchUpgradeTarget" => "",
                "branchUpgradeVersion" => "",
                "isMajorVersionUpgrade" => false,
                "commitUpdateAvailable" => false,
                "remoteCommit" => $localCommit,
                "currentBranch" => getFPPBranch(),
                "localCommit" => $localCommit,
                "isEndOfLife" => false,
                "latestMajorVersion" => $currentMajor
            ));
        } else if ($test === 'osonly') {
            // Simulate: FPP up to date, but OS upgrade available
            return json(array(
                "status" => "OK",
                "branchUpgradeAvailable" => false,
                "branchUpgradeTarget" => "",
                "branchUpgradeVersion" => "",
                "isMajorVersionUpgrade" => false,
                "commitUpdateAvailable" => false,
                "remoteCommit" => $localCommit,
                "currentBranch" => getFPPBranch(),
                "localCommit" => $localCommit,
                "forceOsUpgradeAvailable" => true,
                "isEndOfLife" => false,
                "latestMajorVersion" => $currentMajor
            ));
        }
    }

    // Get the latest release from GitHub (cached). releases/latest only returns
    // full, published releases, and the response already includes the asset
    // list, so we record both the version and whether a downloadable OS image
    // (.fppos) exists for this device without making a second request.
    $latestRelease = file_cache('github_latest_release', function () {
        global $settings;

        $ch = curl_init('https://api.github.com/repos/FalconChristmas/fpp/releases/latest');
        curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
        curl_setopt($ch, CURLOPT_USERAGENT, 'FPP');
        curl_setopt($ch, CURLOPT_TIMEOUT, 5);
        $response = curl_exec($ch);
        curl_close($ch);

        $result = ['version' => '', 'hasDeviceAsset' => false];
        if ($response) {
            $releaseData = json_decode($response, true);
            if (isset($releaseData['tag_name'])) {
                $result['version'] = $releaseData['tag_name'];
            }
            if (isset($releaseData['assets']) && is_array($releaseData['assets'])) {
                foreach ($releaseData['assets'] as $asset) {
                    if (MatchesDeviceOSImage($asset['name'] ?? '', $settings)) {
                        $result['hasDeviceAsset'] = true;
                        break;
                    }
                }
            }
        }
        return json_encode($result);
    }, 300, 60);

    $latestReleaseData = json_decode($latestRelease, true) ?: [];
    $latestReleaseVersion = $latestReleaseData['version'] ?? '';
    $latestReleaseHasDeviceAsset = !empty($latestReleaseData['hasDeviceAsset']);

    // Get unified update status
    $updateStatus = check_fppstats_updates($latestReleaseVersion, $latestReleaseHasDeviceAsset);

    // Get current major version
    $currentMajor = getFPPMajorVersion();

    // Determine latest major version from the release tag
    $latestMajorVersion = $currentMajor; // Default to current if we can't determine
    if ($latestReleaseVersion && preg_match('/^v?(\d+)/', $latestReleaseVersion, $matches)) {
        $latestMajorVersion = intval($matches[1]);
    }

    // Check if this is a major version upgrade (e.g., v9.x → v10.x)
    $isMajorVersionUpgrade = false;
    if ($updateStatus['branchUpgradeAvailable']) {
        if (preg_match('/^v?(\d+)/', $updateStatus['branchUpgradeTarget'], $matches)) {
            $targetMajor = intval($matches[1]);
            $isMajorVersionUpgrade = ($targetMajor > intval($currentMajor));
        }
    }

    // Check if current version is End of Life
    // EOL = current major version is older than the latest major version
    $isEndOfLife = (intval($currentMajor) < $latestMajorVersion);

    return json(array(
        "status" => "OK",
        "branchUpgradeAvailable" => $updateStatus['branchUpgradeAvailable'],
        "branchUpgradeTarget" => $updateStatus['branchUpgradeTarget'],
        "branchUpgradeVersion" => $updateStatus['branchUpgradeVersion'],
        "isMajorVersionUpgrade" => $isMajorVersionUpgrade,
        "commitUpdateAvailable" => $updateStatus['commitUpdateAvailable'],
        "remoteCommit" => $updateStatus['remoteCommit'],
        "currentBranch" => $updateStatus['currentBranch'],
        "localCommit" => $updateStatus['localCommit'],
        "isEndOfLife" => $isEndOfLife,
        "latestMajorVersion" => $latestMajorVersion
    ));
}

/**
 * Set volume
 *
 * Sets the system volume. The new level should be passed as a JSON body.
 *
 * @route POST /api/system/volume
 * @body {"volume": 34}
 * @response 200 Volume set
 * ```json
 * {"status": "OK", "volume": 34}
 * ```
 */
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

/**
 * Get volume
 *
 * Returns the current volume if `fppd` is running, or the `Volume` setting value if not.
 *
 * @route GET /api/system/volume
 * @response 200 Current volume
 * ```json
 * {"status": "OK", "method": "FPPD", "volume": 70}
 * ```
 */
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

/**
 * Get system status
 *
 * Returns `fppd`, network, current playlist, schedule, utilization, host, version, and MQTT status.
 * Pass an optional array of IP addresses (e.g. `&ip[]=192.168.0.1&ip[]=192.168.0.2`) to query
 * remote instances instead.
 *
 * @route GET /api/system/status
 * @response 200 System status
 * ```json
 * {
 *   "fppd": "running",
 *   "status": 1,
 *   "status_name": "playing",
 *   "mode": 2,
 *   "mode_name": "player",
 *   "current_playlist": {
 *     "count": "4",
 *     "playlist": "Test1",
 *     "type": "pause",
 *     "index": "2"
 *   },
 *   "volume": 70,
 *   "wifi": [],
 *   "interfaces": []
 * }
 * ```
 */
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
                } else if ($type == "Baldrick") {
                    $curl = curl_init("http://" . $ip . "/system_state");
                } else if ($type == "FV3") {
                    $curl = curl_init("http://" . $ip . "/status.xml");
                } else if ($type == "FV4") {
                    $curl = curl_init("http://" . $ip . "/api");
                    $data = '{"T":"Q","M":"ST","B":0,"E":0,"I":0,"P":{}}';
                    curl_setopt($curl, CURLOPT_POSTFIELDS, $data);
                    curl_setopt($curl, CURLOPT_HTTPHEADER, array('Content-Type:application/json'));
                } else if ($type == "ESPixelStick") {
                    $curl = curl_init("http://$ip/ws"); // WebSocket URL (use wss:// for secure connections)
                    curl_setopt($curl, CURLOPT_URL, "http://$ip/ws");
                    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
                    curl_setopt($curl, CURLOPT_HTTPHEADER, [
                        "Connection: Upgrade",
                        "Upgrade: websocket",
                        "Host: $ip",
                        "Origin: http://$ip",
                        "Sec-WebSocket-Key: " . base64_encode(random_bytes(16)),
                        "Sec-WebSocket-Version: 13"
                    ]);
                } else {
                    $curl = curl_init("http://" . $ip . "/api/system/status");
                }
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

/**
 * Get system info
 *
 * Returns basic information about the system.
 *
 * @route GET /api/system/info
 * @response 200 System information
 * ```json
 * {
 *   "HostName": "FPPPi",
 *   "HostDescription": "",
 *   "Platform": "Raspberry Pi",
 *   "Variant": "Pi 4",
 *   "Mode": "player",
 *   "Version": "6.0",
 *   "Branch": "master",
 *   "OSVersion": "v2022-02",
 *   "OSRelease": "Raspbian GNU/Linux 11 (bullseye)",
 *   "channelRanges": "1545-84479",
 *   "majorVersion": 6,
 *   "minorVersion": 1000,
 *   "typeId": 13,
 *   "uuid": "M1-10000000AAAAAAA",
 *   "Utilization": {"CPU": 0.12, "Memory": 1.96, "Uptime": "11 days"},
 *   "Kernel": "5.10.92-v7l+",
 *   "LocalGitVersion": "b998f65",
 *   "RemoteGitVersion": "ed62c12",
 *   "UpgradeSource": "github.com",
 *   "IPs": ["192.168.3.84"]
 * }
 * ```
 */
function SystemGetInfo()
{
    $result = GetSystemInfoJsonInternal(isset($_GET['simple']));
    return json($result);
}

/**
 * Adds network interfaces, reboot/restart flags, boot delay status, advanced system info,
 * plugin header indicators, and crash warnings to the `fppd` status array.
 *
 * @param array $obj The base status array returned from fppd or the default stub.
 * @return array The enriched status array.
 */
function finalizeStatusJson($obj)
{
    global $settings;

    if (!isset($_GET['nonetwork'])) {
        $obj['wifi'] = network_wifi_strength_obj();
        $obj['interfaces'] = network_list_interfaces_obj();
    }

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

    // Check if boot delay is in progress
    $bootDelayFile = $settings['mediaDirectory'] . '/tmp/boot_delay';
    if (file_exists($bootDelayFile)) {
        $obj['bootDelayActive'] = 1;
        $delayInfo = trim(file_get_contents($bootDelayFile));
        $parts = explode(',', $delayInfo);
        if (count($parts) == 2) {
            $obj['bootDelayStart'] = intval($parts[0]);
            $obj['bootDelayDuration'] = $parts[1]; // Could be number or "auto"
        }
    } else {
        $obj['bootDelayActive'] = 0;
    }

    //Get the advanced info directly as an array
    $request_expert_content = GetSystemInfoJsonInternal(isset($_GET['simple']), !isset($_GET['nonetwork']));
    //check we have valid data
    if ($request_expert_content === false) {
        $request_expert_content = array();
    }
    //Add data into the final response, since we have the status as an array already then just add the expert view
    //Add a new key for the expert data to the original data array
    $obj['advancedView'] = $request_expert_content;
    // Add plugin header indicators
    if (!isset($_GET['noplugins'])) {
        $obj['pluginHeaderIndicators'] = json_decode(GetPluginHeaderIndicators(), true);
    }

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
                $crWarning = "There $verb $num <a href='filemanager.php#tab-crashes'>crash report$plural</a> available, please submit to FPP developers or delete.";
            } else {
                $crWarning = "There $verb $num <a href='filemanager.php#tab-crashes'>crash report$plural</a> available. " .
                    "This system is configured to automatically upload these to the FPP developers, you may delete the old reports at any time.";
            }
            $obj["warnings"][] = $crWarning;
            $wi["message"] = $crWarning;
            $wi["id"] = 0;
            $obj["warningInfo"][] = $wi;
        }
    }

    return $obj;
}

/**
 * Get all system packages
 *
 * Returns a list of all installed and available OS package names via `apt list --all-versions`.
 *
 * @route GET /api/system/packages
 * @response 200 List of OS package names
 * ```json
 * ["apache2", "ffmpeg", "php"]
 * ```
 */
function GetOSPackages()
{
    $packages = [];
    $cmd = 'apt list --all-versions 2>&1'; // Fetch all package names and versions
    $handle = popen($cmd, 'r'); // Open a process for reading the output

    if ($handle) {
        while (($line = fgets($handle)) !== false) {
            // Extract the package name before the slash
            if (preg_match('/^([^\s\/]+)\//', $line, $matches)) {
                $packages[] = $matches[1];
            }
        }
        pclose($handle); // Close the process
    } else {
        error_log("Error: Unable to fetch package list.");
    }

    return json_encode($packages);
}

/**
 * Get system package information
 *
 * Returns description, dependencies, and installation status for the specified OS package.
 *
 * @route GET /api/system/packages/info/{packageName}
 * @response 200 Package information
 * ```json
 * {
 *   "Description": "The FFmpeg multimedia framework",
 *   "Depends": "libavcodec58, libavformat58",
 *   "Installed": "Yes"
 * }
 * ```
 */
function GetOSPackageInfo()
{
    $packageName = params('packageName');

    // Fetch package information using apt-cache show
    $output = shell_exec("apt-cache show " . escapeshellarg($packageName) . " 2>&1");
    if (!$output || strpos($output, 'E:') === 0) {
        // Return error if apt-cache output is empty or contains an error
        error_log("Package '$packageName' not found or invalid: $output");
        return json_encode(['error' => "Package '$packageName' not found or no information available."]);
    }

    // Check installation status using dpkg-query
    $installStatus = shell_exec("/usr/bin/dpkg-query -W -f='\${Status}\n' " . escapeshellarg($packageName) . " 2>&1");
    error_log("Raw dpkg-query output for $packageName: |" . $installStatus . "|");

    // Trim and validate output
    $trimmedStatus = trim($installStatus);
    error_log("Trimmed dpkg-query output for $packageName: |" . $trimmedStatus . "|");

    $isInstalled = ($trimmedStatus === 'install ok installed') ? 'Yes' : 'No';

    // Parse apt-cache output
    $lines = explode("\n", $output);
    $description = '';
    $depends = '';

    foreach ($lines as $line) {
        if (strpos($line, 'Description:') === 0) {
            $description = trim(substr($line, strlen('Description:')));
        } elseif (strpos($line, 'Depends:') === 0) {
            $depends = trim(substr($line, strlen('Depends:')));
        }
    }

    return json_encode([
        'Description' => $description,
        'Depends' => $depends,
        'Installed' => $isInstalled
    ]);
}

/**
 * Skip boot delay
 *
 * Skips the current boot delay by creating a skip flag file, allowing FPP startup to proceed immediately.
 *
 * @route POST /api/system/fppd/skipBootDelay
 * @response 200 Boot delay skip requested
 * ```json
 * {"status": "OK", "message": "Boot delay skip requested"}
 * ```
 */
function SkipBootDelay()
{
    global $settings;

    $bootDelayFile = $settings['mediaDirectory'] . '/tmp/boot_delay';
    $skipFile = $settings['mediaDirectory'] . '/tmp/boot_delay_skip';

    if (!file_exists($bootDelayFile)) {
        return json_encode(['status' => 'error', 'message' => 'No boot delay in progress']);
    }

    // Create skip flag file that FPPINIT will check for
    file_put_contents($skipFile, '1');

    return json_encode(['status' => 'OK', 'message' => 'Boot delay skip requested']);
}
