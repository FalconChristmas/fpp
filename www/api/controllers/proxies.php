<?
require_once(__DIR__ . "/../../config.php");
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
            if ($host != "$1") {
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
/////////////////////////////////////////////////////////////////////////////
// GET /api/proxies
function GetProxies()
{
    $proxies = LoadProxyList();
    return json($proxies, true);
}

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

