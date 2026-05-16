<?
require_once(__DIR__ . "/../../config.php");

/**
 * Proxy a command to remote FPP (v1 — query parameters)
 *
 * Proxies a named action to a remote FPP instance by IP address.
 * Supported actions: `listUpgrades`, `reboot`, `restartFppd`, `upgradeOS`.
 *
 * @route GET /api/remoteAction
 * @response 400 Invalid action
 * ```json
 * {"error": "Invalid action given: badaction"}
 * ```
 */
function RemoteAction_v1()
{
    global $settings;
    $ip = htmlspecialchars(isset($_GET['ip']) ? $_GET['ip'] : null);
    $action = htmlspecialchars(isset($_GET['action']) ? $_GET['action'] : null);

    $action_map = [
        'listUpgrades' => '/api/git/releases/os',
        'reboot' => '/api/system/reboot',
        'restartFppd' => '/api/system/fppd/restart',
        'upgradeOS' => '/upgradeOS',
    ];

    if (!filter_var($ip, FILTER_VALIDATE_IP, FILTER_FLAG_IPV4)) {
        http_response_code(400);
        echo json_encode(['error' => "Invalid IP address: $ip"]);
        exit(0);
    }

    if (!array_key_exists($action, $action_map)) {
        http_response_code(400);
        return json(['error' => 'HTTP Error: 400', 'details' => "Invalid action given: $action"]);
    }

    $curl = curl_init('http://' . $ip . $action_map[$action]);
    curl_setopt($curl, CURLOPT_FAILONERROR, true);
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 2000);
    $request_content = curl_exec($curl);
    $http_code = curl_getinfo($curl, CURLINFO_HTTP_CODE);
    if ($http_code !== 200) {
        curl_close($curl);
        http_response_code($http_code);
        $details = $request_content ? $request_content : 'No response content';
        if ($http_code < 200) {
            $details = curl_error($curl);
        }
        http_response_code(400);
        return json(['error' => 'HTTP Error: ' . $http_code, 'details' => $request_content]);
    }
    curl_close($curl);

    return($request_content);

}

/**
 * Proxy a command to remote FPP (v2 — JSON body)
 *
 * Proxies a named action to a remote FPP instance by IP address.
 * Supported actions: `listUpgrades`, `reboot`, `restartFppd`, `upgradeOS`.
 *
 * @route POST /api/v2/remoteAction
 * @body {"ip": "192.168.1.100", "action": "reboot"}
 * @response 400 Invalid action
 * ```json
 * {"error": "Invalid action given: badaction"}
 * ```
 */
function RemoteAction()
{
    global $settings;
    $body = getJsonBody();
    $ip = htmlspecialchars(isset($body['ip']) ? $body['ip'] : '');
    $action = htmlspecialchars(isset($body['action']) ? $body['action'] : '');

    $action_map = [
        'listUpgrades' => '/api/git/releases/os',
        'reboot' => '/api/system/reboot',
        'restartFppd' => '/api/system/fppd/restart',
        'upgradeOS' => '/upgradeOS',
    ];

    if (!filter_var($ip, FILTER_VALIDATE_IP, FILTER_FLAG_IPV4)) {
        http_response_code(400);
        echo json_encode(['error' => "Invalid IP address: $ip"]);
        exit(0);
    }

    if (!array_key_exists($action, $action_map)) {
        http_response_code(400);
        return json(['error' => 'HTTP Error: 400', 'details' => "Invalid action given: $action"]);
    }

    $curl = curl_init('http://' . $ip . $action_map[$action]);
    curl_setopt($curl, CURLOPT_FAILONERROR, true);
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 2000);
    $request_content = curl_exec($curl);
    $http_code = curl_getinfo($curl, CURLINFO_HTTP_CODE);
    if ($http_code !== 200) {
        curl_close($curl);
        http_response_code($http_code);
        $details = $request_content ? $request_content : 'No response content';
        if ($http_code < 200) {
            $details = curl_error($curl);
        }
        http_response_code(400);
        return json(['error' => 'HTTP Error: ' . $http_code, 'details' => $request_content]);
    }
    curl_close($curl);

    return($request_content);

}

/**
 * Reads the proxy config file and merges in active DHCP leases, marking each
 * entry with `dhcp` and `pending` flags.
 *
 * @return array Array of proxy entries, each with host, description, dhcp, and pending keys.
 */
function LoadProxyList()
{
    global $settings;
    $mediaDirectory = $settings['mediaDirectory'];
    if (file_exists("$mediaDirectory/config/proxy-config.conf")) {
        $hta = file("$mediaDirectory/config/proxy-config.conf", FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
    } else {
        $hta = array();
    }
    $proxies = array();
    $description = "";
    foreach ($hta as $line) {
        if (strpos($line, 'http://') !== false) {
            $parts = preg_split("/[\s]+/", $line);
            $host = preg_split("/[\/]+/", $parts[2])[1];
            if (!str_contains($host, "$1")) {
                $proxies[] = array("host" => $host, "description" => $description);
                $description = "";
            }
        } else if (strpos($line, '# D:') !== false) {
            $description = substr($line, 4);
        }
    }

    $leases = getDHCPLeases();
    foreach ($leases as $ip) {
        $alreadyExists = false;
        foreach ($proxies as $proxy) {
            if ($proxy['host'] === $ip) {
                $alreadyExists = true;
                break;
            }
        }
        if (!$alreadyExists) {
            // Not in config, mark as pending
            $proxies[] = array("host" => $ip, "description" => "", "dhcp" => true, "pending" => true);
        }
    }

    // Mark all DHCP hosts in $proxies
    foreach ($proxies as &$proxy) {
        if (in_array($proxy['host'], $leases)) {
            $proxy['dhcp'] = true;
            // Only set pending if not already set (i.e., was just added above)
            if (!isset($proxy['pending'])) {
                $proxy['pending'] = false;
            }
        } else {
            // Not a DHCP lease, ensure pending is false
            $proxy['pending'] = false;
        }
    }
    unset($proxy);

    return $proxies;
}

/**
 * Set proxy list
 *
 * Replaces the proxy list with the submitted array of `host`/`description` objects,
 * validates each entry, and triggers an Apache graceful reload.
 *
 * @route POST /api/proxies
 * @body [{"host": "192.168.1.2", "description": "Mega Tree"}]
 * @response 200 Updated proxy list
 * ```json
 * [
 *   {"host": "192.168.1.2", "description": "Mega Tree"},
 *   {"host": "192.168.1.146", "description": "Yard"},
 *   {"host": "192.168.1.148", "description": "Left House"}
 * ]
 * ```
 * @response 400 No valid proxies provided
 * ```json
 * {"error": "No valid proxies provided"}
 * ```
 */
function PostProxies()
{
    $proxies = $_POST;
    $validProxies = [];

    foreach ($proxies as $proxy) {
        // Validate expected structure
        if (!isset($proxy['host']))
            continue;
        $host = $proxy['host'];
        $description = isset($proxy['description']) ? $proxy['description'] : '';

        // Validate host as IPv4 or hostname
        if (
            !filter_var($host, FILTER_VALIDATE_IP, FILTER_FLAG_IPV4) &&
            !preg_match('/^[a-zA-Z0-9\-\.]+$/', $host)
        ) {
            continue; // skip invalid host
        }

        // Sanitize description (strip tags, limit length)
        $description = strip_tags($description);
        $description = substr($description, 0, 128);

        $validProxies[] = [
            'host' => $host,
            'description' => $description
        ];
    }

    if (!empty($validProxies)) {
        WriteProxyFile($validProxies);
        GenerateBackupViaAPI('Proxies were added.');
        return json($validProxies);
    } else {
        http_response_code(400); // Bad Request
        echo json_encode(['error' => 'No valid proxies provided']);
    }
}

/**
 * Writes the proxy list to `proxy-config.conf` and triggers an Apache
 * graceful reload. DHCP leases are merged in automatically.
 *
 * @param array $proxies Array of proxy entries with host and description keys.
 * @return void
 */
function WriteProxyFile($proxies)
{
    global $settings;
    global $SUDO, $fppDir;
    $mediaDirectory = $settings['mediaDirectory'];

    // Merge in current DHCP leases, avoiding duplicates
    $leases = getDHCPLeases();
    foreach ($leases as $ip) {
        $alreadyExists = false;
        foreach ($proxies as $proxy) {
            if ($proxy['host'] === $ip) {
                $alreadyExists = true;
                break;
            }
        }
        if (!$alreadyExists) {
            $proxies[] = array("host" => $ip, "description" => "", "dhcp" => true);
        }
    }

    $newht = "";
    foreach ($proxies as $item) {
        $host = $item['host'];
        $description = $item['description'];
        // Mark DHCP hosts with a comment if desired
        if (!empty($item['dhcp'])) {
            $newht .= "# DHCP\n";
        }
        if ($description != "") {
            $newht .= "# D:" . $description . "\n";
        }
        $newht .= "RewriteRule ^" . $host . "$  " . $host . "/  [R,L]\n";
        $newht .= "RewriteRule ^" . $host . ":([0-9]*)$  http://" . $host . ":$1/  [P,L]\n";
        $newht .= "RewriteRule ^" . $host . ":([0-9]*)/(.*)$  http://" . $host . ":$1/$2  [P,L]\n";
        $newht .= "RewriteRule ^" . $host . "/(.*)$  http://" . $host . "/$1  [P,L]\n\n";
    }
    file_put_contents("$mediaDirectory/config/proxy-config.conf", $newht);

    // Graceful reload of Apache to apply changes
    system($SUDO . " $fppDir/scripts/common gracefullyReloadApacheConf > /dev/null 2>&1");
    // exec('/opt/fpp/scripts/common gracefullyReloadApacheConf');
}

/**
 * Get list of proxy IPs
 *
 * Returns the list of IP addresses this FPP instance can proxy.
 *
 * @route GET /api/proxies
 * @response 200 Current proxy list
 * ```json
 * [
 *   {"host": "192.168.1.2", "description": "Mega Tree"},
 *   {"host": "192.168.1.146", "description": "Yard"},
 *   {"host": "192.168.1.148", "description": "Left House"}
 * ]
 * ```
 */
function GetProxies()
{
    $proxies = LoadProxyList();
    return json($proxies, true);
}

/**
 * Add proxy
 *
 * Adds a single IP address to the FPP proxy list if it does not already exist.
 *
 * @route POST /api/proxies/{ProxyIp}
 * @response 200 Updated proxy list
 * ```json
 * [
 *   {"host": "192.168.1.2", "description": "Mega Tree"}
 * ]
 * ```
 */
function AddProxy()
{
    $pip = params('ProxyIp');
    $pdesp = params('Description', ''); // Allow description to be passed
    $proxies = LoadProxyList();
    $exists = false;
    foreach ($proxies as $proxy) {
        if ($proxy['host'] === $pip) {
            $exists = true;
            break;
        }
    }
    if (!$exists) {
        $proxies[] = array("host" => $pip, "description" => $pdesp);
        WriteProxyFile($proxies);
        GenerateBackupViaAPI('Proxy ' . $pip . ' was added.');
    }
    return json($proxies);
}

/**
 * Remove proxy
 *
 * Removes a single IP address from the FPP proxy list.
 *
 * @route DELETE /api/proxies/{ProxyIp}
 * @response 200 Updated proxy list
 * ```json
 * []
 * ```
 */
function DeleteProxy()
{
    $pip = params('ProxyIp');
    $proxies = LoadProxyList();
    $newproxies = [];
    foreach ($proxies as $proxy) {
        if ($proxy['host'] !== $pip) {
            $newproxies[] = $proxy;
        }
    }
    WriteProxyFile($newproxies);
    GenerateBackupViaAPI('Proxy ' . $pip . ' was deleted.');
    return json($newproxies);
}

/**
 * Get all remote FPPs
 *
 * Returns the list of known remote FPP systems from `fppd` multiSync discovery.
 *
 * @route GET /api/remotes
 * @response 200 Known remote FPP systems
 * ```json
 * {
 *   "192.168.1.10": "192.168.1.10 - remote-fpp",
 *   "192.168.1.11": "192.168.1.11"
 * }
 * ```
 */
function GetRemotes()
{
    $curl = curl_init('http://localhost:32322/fppd/multiSyncSystems');
    curl_setopt($curl, CURLOPT_FAILONERROR, true);
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 200);
    $request_content = curl_exec($curl);

    $remotes = array();
    $j = json_decode($request_content, true);
    foreach ($j["systems"] as $host) {
        if ($host["address"] != $host["hostname"]) {
            $remotes[$host["address"]] = $host["address"] . " - " . $host["hostname"];
        } else {
            $remotes[$host["address"]] = $host["address"];
        }
    }
    return json($remotes);
}

/**
 * Get URL from remote FPP
 *
 * Fetches a URL on a remote FPP instance via server-side proxy to avoid CSP restrictions.
 *
 * @route GET /api/proxy/{Ip}/{urlPart}
 * @response 400 Invalid IP address
 * ```json
 * {"error": "Invalid IP address"}
 * ```
 * @response 502 Proxy fetch failed
 * ```json
 * {"error": "Failed to fetch proxied URL"}
 * ```
 */
function GetProxiedURL()
{
    $ip = params('Ip');
    $urlPart = params('urlPart');

    // Basic validation: Only allow IPv4 addresses and safe URL parts
    if (!filter_var($ip, FILTER_VALIDATE_IP, FILTER_FLAG_IPV4)) {
        http_response_code(400);
        echo json_encode(['error' => 'Invalid IP address']);
        return;
    }
    // Prevent directory traversal and unsafe characters
    if (preg_match('/\.\.|[^\w\-\/\.]/', $urlPart)) {
        http_response_code(400);
        echo json_encode(['error' => 'Invalid URL part']);
        return;
    }

    $url = "http://$ip/$urlPart";
    $data = @file_get_contents($url);

    if ($data === false) {
        http_response_code(502); // Bad Gateway
        echo json_encode(['error' => 'Failed to fetch proxied URL']);
        return;
    }

    // Optionally set content-type if known
    header('Content-Type: text/plain');
    echo $data;
    return;
}

/**
 * Returns the list of IP addresses currently offered as DHCP leases by this FPP instance.
 *
 * @return array Array of IPv4 address strings.
 */
function getDHCPLeases()
{
    $dhcpIps = array();
    $interfaces = network_list_interfaces_array();
    foreach ($interfaces as $iface) {
        $iface = rtrim($iface, " :\n\r\t\v\x00");
        $out = shell_exec("networkctl --no-legend -l -n 0 status " . escapeshellarg($iface));
        if ($out == null) {
            $out = "";
        }
        $lines = explode("\n", trim($out));
        $inLeases = false;
        foreach ($lines as $line) {
            $line = trim($line);
            if (!$inLeases && strpos($line, "Offered DHCP leases") === 0) {
                $inLeases = true;
                $line = trim(substr($line, 20));
            }
            if ($inLeases) {
                $pos = strpos($line, "(to ");
                if ($pos === false) {
                    $inLeases = false;
                } else {
                    $line = trim(substr($line, 0, $pos));
                    $dhcpIps[] = $line;
                }
            }
        }
    }
    return $dhcpIps;
}

/**
 * Delete all proxies
 *
 * Deletes all proxy entries by writing an empty `proxy-config.conf` and
 * triggering an Apache graceful reload.
 *
 * @route DELETE /api/proxies
 * @response 200 All proxies deleted
 * ```json
 * []
 * ```
 */
function DeleteAllProxies()
{
    global $settings;
    $mediaDirectory = $settings['mediaDirectory'];

    // Write an empty proxy config file
    file_put_contents("$mediaDirectory/config/proxy-config.conf", "");

    // Graceful reload of Apache to apply changes
    global $SUDO, $fppDir;
    system($SUDO . " $fppDir/scripts/common gracefullyReloadApacheConf > /dev/null 2>&1");

    GenerateBackupViaAPI('All proxies were deleted.');
    return json([]);
}
