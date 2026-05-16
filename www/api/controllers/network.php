<?

require_once "../common.php";

/**
 * Get network interface details
 *
 * Returns detailed information about network interfaces, their IP addresses,
 * and Wi-Fi signal strength.
 *
 * @route GET /api/network/interface
 * @response 200 Network interface details
 * ```json
 * [
 *   {
 *     "ifindex": 2,
 *     "ifname": "wlan0",
 *     "flags": ["BROADCAST", "MULTICAST", "UP", "LOWER_UP"],
 *     "mtu": 1500,
 *     "operstate": "UP",
 *     "addr_info": [
 *       {"family": "inet", "local": "192.168.50.146", "prefixlen": 24}
 *     ],
 *     "wifi": {
 *       "interface": "wlan0",
 *       "link": 52,
 *       "level": -58,
 *       "noise": -256,
 *       "desc": "good"
 *     }
 *   }
 * ]
 * ```
 */
function NetworkListInterfaces()
{
    return json(network_list_interfaces_obj());
}

/**
 * Get all wifi signal strenths
 *
 * Returns signal strength information for wireless network interfaces.
 *
 * @route GET /api/network/wifi/strength
 * @response 200 Wi-Fi signal strength per interface
 * ```json
 * [
 *   {
 *     "interface": "wlan0",
 *     "link": 45,
 *     "level": -65,
 *     "noise": -256
 *   }
 * ]
 * ```
 */
function NetworkWiFiStrength()
{
    return json(network_wifi_strength_obj());
}

/**
 * Get discoverable wifi networks
 *
 * Returns information about Wi-Fi networks discoverable via the specified
 * `{interface}`. Networks without an SSID may appear in the list.
 *
 * @route GET /api/network/wifi/scan/{interface}
 * @response 200 Discoverable Wi-Fi networks
 * ```json
 * {
 *   "status": "OK",
 *   "networks": [
 *     {
 *       "lastSeen": "0 ms ago",
 *       "freq": 2437,
 *       "signal": "-61.00 dBm",
 *       "SSID": "Christmas"
 *     }
 *   ]
 * }
 * ```
 */
function NetworkWiFiScan()
{
    $networks = array();
    $current = array();
    $interface = params('interface');

    # Validate interface.   -- Important because of SUDO
    $interfaces = json_decode(NetworkListInterfaces(), true);
    $found = false;

    foreach ($interfaces as $row) {
        if ($row["ifname"] == $interface) {
            $found = true;
        }
    }

    if (!$found) {
        return json(array("status" => "Invalid Interface", "networks" => array()));
    }

    exec("sudo /sbin/ip link set $interface up", $output);
    $output = array();
    $cmd = "sudo /sbin/iw dev $interface scan";
    exec($cmd, $output);

    foreach ($output as $row) {
        if (str_starts_with($row, "BSS")) {
            array_push($networks, $current);
            $current = array();
        } else if (preg_match('/freq: (.*)/', $row, $matches)) {
            $current['freq'] = intval($matches[1]);
        } else if (preg_match('/last seen: (.*)/', $row, $matches)) {
            $current['lastSeen'] = $matches[1];
        } else if (preg_match('/SSID: (.*)/', $row, $matches)) {
            $current['SSID'] = $matches[1];
        } else if (preg_match('/signal: (.*)/', $row, $matches)) {
            $current['signal'] = $matches[1];
        } else if (preg_match('/capability:.*Privacy/', $row)) {
            // Basic encryption detection (WEP or WPA/WPA2)
            $current['encrypted'] = true;
        } else if (preg_match('/RSN:/', $row)) {
            // WPA2/WPA3 detected
            $current['encrypted'] = true;
            $current['security'] = isset($current['security']) ? $current['security'] . ' WPA2' : 'WPA2';
        } else if (preg_match('/WPA:/', $row)) {
            // WPA detected
            $current['encrypted'] = true;
            $current['security'] = isset($current['security']) ? $current['security'] . ' WPA' : 'WPA';
        } else if (preg_match('/Authentication suites:.*PSK/', $row)) {
            // Personal/PSK authentication
            $current['auth'] = 'PSK';
        } else if (preg_match('/Authentication suites:.*802\.1x/', $row)) {
            // Enterprise authentication
            $current['auth'] = 'Enterprise';
        } else if (preg_match('/Group cipher: (.*)/', $row, $matches)) {
            $current['cipher'] = trim($matches[1]);
        }
    }
    if (count($current) > 0) {
        array_push($networks, $current);
    }

    return json(array("status" => "OK", "networks" => $networks));
}

/**
 * Delete interface persistent names
 *
 * Removes interface persistent names by deleting systemd `.link` files and
 * restoring any USB ethernet adapter config files back to `eth*` names.
 *
 * @route DELETE /api/network/persistentNames
 * @response 200 Persistent names removed
 * ```json
 * {"status": "OK"}
 * ```
 */
function NetworkPersistentNamesDelete()
{
    global $settings;

    shell_exec("sudo rm -f /etc/systemd/network/5?-fpp-*.link");
    shell_exec("sudo ln -sf /dev/null /etc/systemd/network/99-default.link");
    shell_exec("sudo ln -sf /dev/null /etc/systemd/network/73-usb-net-by-mac.link");

    // Rename any enx* interface config files back to eth* names
    // This restores configs when clearing persistent names for USB ethernet adapters
    $configDir = $settings['configDirectory'];
    $interfaces = network_list_interfaces_array();

    foreach ($interfaces as $iface) {
        $iface = preg_replace("/:$/", "", $iface);

        // Check if this is a USB ethernet adapter with an enx name
        if (substr_compare($iface, "enx", 0, 3) == 0) {
            $enxConfigFile = $configDir . "/interface." . $iface;
            if (file_exists($enxConfigFile)) {
                // Find the next available eth number
                $ethNum = 1; // Start with eth1 since eth0 is usually built-in
                while (file_exists($configDir . "/interface.eth" . $ethNum)) {
                    $ethNum++;
                }

                $newName = "eth" . $ethNum;
                $ifaceConfig = file_get_contents($enxConfigFile);
                $ifaceConfig = str_replace($iface, $newName, $ifaceConfig);
                file_put_contents($configDir . "/interface." . $newName, $ifaceConfig);
                unlink($enxConfigFile);
            }
        }
    }

    $output = array("status" => "OK");
    return json($output);
}

/**
 * Set interface persistent names
 *
 * Creates interface persistent names by writing systemd `.link` files that
 * pin each interface's name to its MAC address.
 *
 * @route POST /api/network/persistentNames
 * @response 200 Persistent names created
 * ```json
 * {"status": "OK", "interfaceCnt": 2}
 * ```
 */
function NetworkPersistentNamesCreate()
{
    global $settings;
    NetworkPersistentNamesDelete();

    $interfaces = network_list_interfaces_array();
    $count = 0;
    foreach ($interfaces as $iface) {
        $iface = preg_replace("/:$/", "", $iface);
        $cmd = "ip link show " . $iface . " | grep ether | awk '{split($0, a,\" \"); print a[2];}'";
        exec($cmd, $output, $return_val);
        $macAddress = $output[0];

        $cont = "[Match]\nMACAddress=" . $macAddress;
        $cont = $cont . "\nPermanentMACAddress=" . $macAddress;
        if (substr($iface, 0, strlen("wl")) === "wl") {
            $cont = $cont . "\nOriginalName=wl*";
        }

        $mod = shell_exec("cat /sys/class/net/" . $iface . "/device/modalias");
        if (substr_compare($iface, "eth", 0, 3) == 0 && substr_compare($mod, "usb", 0, 3) == 0) {
            //USB ethernet adapter, the iface name is going to end up changing, we need to rename the
            //FPP config file
            $new_iface = "enx" . str_replace(':', '', $macAddress);
            if (file_exists($settings['configDirectory'] . "/interface." . $iface)) {
                $ifaceConfig = file_get_contents($settings['configDirectory'] . "/interface." . $iface);
                $ifaceConfig = str_replace($iface, $new_iface, $ifaceConfig);
                file_put_contents($settings['configDirectory'] . "/interface." . $new_iface, $ifaceConfig);
            }
            $cont = $cont . "\n\n[Link]\nNamePolicy=\nName=" . $new_iface . "\n";
        } else {
            $cont = $cont . "\n\n[Link]\nNamePolicy=\nName=" . $iface . "\n";
        }
        file_put_contents("/tmp/5" . strval($count) . "-fpp-" . $iface . ".link", $cont);
        shell_exec("sudo mv /tmp/5" . strval($count) . "-fpp-" . $iface . ".link /etc/systemd/network/");

        unset($output);
        $count = $count + 1;
    }
    shell_exec("sudo  rm -f /etc/systemd/network/99-default.link");
    shell_exec("sudo  rm -f /etc/systemd/network/73-usb-net-by-mac.link");

    $output = array("status" => "OK", "interfaceCnt" => $count);
    return json($output);

}

/**
 * Get DNS configuration
 *
 * Returns the current DNS configuration. If not configured, `status` will
 * be `Not Configured`.
 *
 * @route GET /api/network/dns
 * @response 200 Current DNS configuration
 * ```json
 * {
 *   "DNS1": "192.168.50.1",
 *   "DNS2": "192.168.1.1",
 *   "status": "OK"
 * }
 * ```
 */
function NetworkGetDNS()
{
    global $settings;

    $cfgFile = $settings['configDirectory'] . "/dns";
    if (file_exists($cfgFile)) {
        $obj = parse_ini_file($cfgFile);
        $obj['status'] = "OK";
        return json($obj);
    }
    return json(array("status" => "Not Configured"));
}

/**
 * Set DNS configuration
 *
 * Updates the DNS configuration.
 *
 * @route POST /api/network/dns
 * @body {"DNS1": "192.168.50.1", "DNS2": "192.168.1.1"}
 * @response 200 DNS configuration updated
 * ```json
 * {
 *   "status": "OK",
 *   "DNS": {
 *     "DNS1": "192.168.50.1",
 *     "DNS2": "192.168.1.1"
 *   }
 * }
 * ```
 */
function NetworkSaveDNS()
{
    global $settings;

    $data = json_decode(file_get_contents('php://input'), true);

    $cfgFile = $settings['configDirectory'] . "/dns";

    $f = fopen($cfgFile, "w");
    if ($f == false) {
        return;
    }

    fprintf(
        $f,
        "DNS1=\"%s\"\n" .
        "DNS2=\"%s\"\n",
        $data['DNS1'],
        $data['DNS2']
    );
    fclose($f);

    //Trigger a JSON Configuration Backup
    GenerateBackupViaAPI('DNS Configuration was modified.');

    return json(array("status" => "OK", "DNS" => $data));
}

/**
 * Get default gateway
 *
 * Returns the currently configured default gateway IP address. May be empty
 * when using DHCP.
 *
 * @route GET /api/network/gateway
 * @response 200 Current default gateway
 * ```json
 * {"GATEWAY": "192.168.1.1"}
 * ```
 */
function NetworkGetGateway()
{
    global $settings;

    $cfgFile = $settings['configDirectory'] . "/gateway";
    $result = array("GATEWAY" => "");

    if (file_exists($cfgFile)) {
        $content = file_get_contents($cfgFile);
        $lines = explode("\n", $content);

        foreach ($lines as $line) {
            $line = trim($line);
            if (strpos($line, 'GATEWAY=') === 0) {
                $result['GATEWAY'] = trim(str_replace('GATEWAY=', '', $line), '"');
                break;
            }
        }
    }

    return json($result);
}

/**
 * Set default gateway
 *
 * Saves the default gateway IP address to the `gateway` configuration file.
 *
 * @route POST /api/network/gateway
 * @body {"GATEWAY": "192.168.1.1"}
 * @response 200 Default gateway saved
 * ```json
 * {"status": "OK", "GATEWAY": "192.168.1.1"}
 * ```
 */
function NetworkSaveGateway()
{
    global $settings;

    $data = json_decode(file_get_contents('php://input'), true);

    $cfgFile = $settings['configDirectory'] . "/gateway";

    $f = fopen($cfgFile, "w");
    if ($f == false) {
        return json(array("status" => "ERROR", "message" => "Unable to create gateway configuration file"));
    }

    fprintf(
        $f,
        "GATEWAY=\"%s\"\n",
        $data['GATEWAY']
    );
    fclose($f);

    //Trigger a JSON Configuration Backup
    GenerateBackupViaAPI('Global Gateway Configuration was modified.');

    return json(array("status" => "OK", "GATEWAY" => $data['GATEWAY']));
}

/**
 * Get network interface configuration
 *
 * Retrieves the current network interface configuration.
 *
 * @route GET /api/network/interface/{interface}
 * @response 200 Network interface configuration
 * ```json
 * {
 *   "INTERFACE": "eth0",
 *   "PROTO": "static",
 *   "ADDRESS": "192.168.1.149",
 *   "NETMASK": "255.255.255.0",
 *   "status": "OK",
 *   "CurrentAddress": "192.168.1.149",
 *   "CurrentNetmask": "255.255.255.0"
 * }
 * ```
 */
function NetworkGetInterface()
{
    global $settings;

    $interface = params('interface');
    $result = array("status" => "ERROR: Interface not found");

    $cfgFile = $settings['configDirectory'] . "/interface." . $interface;
    if (file_exists($cfgFile)) {
        $result = parse_ini_file($cfgFile);
        $result['status'] = 'OK';

        // Remove GATEWAY from interface configuration since it's now handled globally
        if (isset($result['GATEWAY'])) {
            unset($result['GATEWAY']);
        }
    }

    // Use `ip` (netlink) rather than ifconfig to avoid WEXT calls on
    // wireless interfaces.
    exec("/sbin/ip -o -4 addr show dev $interface 2>/dev/null", $output);
    foreach ($output as $line) {
        if (preg_match('/\binet (\d+\.\d+\.\d+\.\d+)\/(\d+)/', $line, $m)) {
            $result['CurrentAddress'] = $m[1];
            $result['CurrentNetmask'] = long2ip(~((1 << (32 - $m[2])) - 1));
        }
    }
    unset($output);
    exec("/sbin/ip -o link show dev $interface 2>/dev/null", $output);
    foreach ($output as $line) {
        if (preg_match('/<[^>]*UP[^>]*>/', $line)) {
            $result['status'] = 'OK';
        }
    }
    unset($output);

    if (substr($interface, 0, 2) == "wl") {
        exec("/sbin/iw dev $interface link 2>/dev/null", $output);
        foreach ($output as $line) {
            if (preg_match('/SSID: (.+)/', $line, $m)) {
                $result['CurrentSSID'] = $m[1];
            }
            // For Legacy Purpose (TX Bitrate)
            if (preg_match('/^\s*tx bitrate:\s*([0-9\.]+)\s*MBit\/s/', $line, $m)) {
                $result['CurrentRate'] = $m[1];
            }
            // TX bitrate
            if (preg_match('/^\s*tx bitrate:\s*([0-9\.]+)\s*MBit\/s/', $line, $m)) {
                $result['CurrentTXRate'] = $m[1];
            }

            // RX bitrate
            if (preg_match('/^\s*rx bitrate:\s*([0-9\.]+)\s*MBit\/s/', $line, $m)) {
                $result['CurrentRXRate'] = $m[1];
            }
        }
        unset($output);
    }
    if (isset($result["DHCPSERVER"]) && $result["DHCPSERVER"] == "1") {
        $result["StaticLeases"] = array();
        $result["CurrentLeases"] = array();
        $leasesFile = $settings['configDirectory'] . "/leases." . $interface;
        if (file_exists($leasesFile)) {
            $handle = fopen($leasesFile, "r");
            if ($handle) {
                $mac = "";
                $ip = "";
                while (($line = fgets($handle)) !== false) {
                    $line = trim($line);
                    if ($line == "[DHCPServerStaticLease]") {
                        $mac = "";
                        $ip = "";
                    } else if (str_starts_with($line, "MACAddress=")) {
                        $mac = substr($line, 11);
                    } else if (str_starts_with($line, "Address=")) {
                        $ip = substr($line, 8);
                    }
                    if ($mac != "" && $ip != "") {
                        $result["StaticLeases"][$ip] = $mac;
                        $mac = "";
                        $ip = "";
                    }
                }
            }

            fclose($handle);
        }

        $out = shell_exec("networkctl --no-legend -l -n 0 status " . $interface);
        $lines = explode("\n", trim($out));
        $inLeases = false;
        $result["CurrentLeases"] = array();
        foreach ($lines as $line) {
            $line = trim($line);
            //echo $line . "\n";

            if (!$inLeases && str_starts_with($line, "Offered DHCP leases")) {
                $inLeases = true;
                $line = trim(substr($line, 20));
            }
            if ($inLeases) {
                $pos = strpos($line, "(to ");
                if ($pos === false) {
                    $inLeases = false;
                } else {
                    $ip = trim(substr($line, 0, $pos));

                    $mac = trim(shell_exec("arp -a  " . $ip . " | awk '{print $4;}'"));
                    if (!isset($result["StaticLeases"][$ip]) || $result["StaticLeases"][$ip] != $mac) {
                        $result["CurrentLeases"][$ip] = $mac;
                    }
                }
            }
        }
    }

    return json($result);
}

/**
 * Create DHCP interface
 *
 * Creates a new blank DHCP interface configuration file for the specified
 * network interface (e.g. `eth1`, `wlan0`).
 *
 * @route POST /api/network/interface/add/{interface}
 * @response 200 DHCP interface created
 * ```json
 * {"status": "New Blank Interface created"}
 * ```
 */
function NetworkAddInterface()
{
    global $settings;

    $interface = params('interface');

    if (preg_match('/^[a-z]+[0-9]+$/', $interface)) {
        $cfgFile = $settings['configDirectory'] . "/interface." . $interface;
        if (!file_exists($cfgFile)) {
            $contents = "INTERFACE=" . $interface . "\nPROTO=dhcp\n";
            if (file_put_contents($cfgFile, $contents)) {
                $result = array("status" => "New Blank Interface created");
            } else {
                $result = array("status" => "ERROR: could not create new Interface");
            }
        } else {
            $result = array("status" => "ERROR: Interface already exists");
        }
    } else {
        $result = array("status" => "ERROR: must be in format <a-z><0-9> eg wlan0 or eth1 etc");
    }

    return json($result);
}

/**
 * Set network interface configuration
 *
 * Updates the saved configuration for the specified `{interface}` but does
 * not restart the network.
 *
 * @route POST /api/network/interface/{interface}
 * @body {"INTERFACE": "eth0", "PROTO": "static", "ADDRESS": "192.168.1.149", "NETMASK": "255.255.255.0", "GATEWAY": "192.168.1.1"}
 * @response 200 Interface configuration saved
 * ```json
 * {"status": "OK"}
 * ```
 */
function NetworkSetInterface()
{
    global $settings;

    $interface = params('interface');
    $raw = file_get_contents('php://input');
    $data = json_decode($raw, true);
    if (!isset($data['INTERFACE'])) {
        echo ("WTF");
        return json(array("status" => "Invalid Name is required", "debug" => $raw));
    }

    $cfgFile = $settings['configDirectory'] . "/interface." . $data['INTERFACE'];
    $leasesFile = $settings['configDirectory'] . "/leases." . $data['INTERFACE'];

    $f = fopen($cfgFile, "w");
    if ($f == false) {
        return json(array("status" => "Unable to create file for interface"));
    }

    if ($data['PROTO'] == "static") {
        fprintf(
            $f,
            "INTERFACE=\"%s\"\n" .
            "PROTO=\"static\"\n" .
            "ADDRESS=\"%s\"\n" .
            "NETMASK=\"%s\"\n",
            $data['INTERFACE'],
            $data['ADDRESS'],
            $data['NETMASK']
        );

    } else if ($data['PROTO'] == "dhcp") {
        fprintf(
            $f,
            "INTERFACE=%s\n" .
            "PROTO=dhcp\n",
            $data['INTERFACE']
        );
    }

    if (substr($data['INTERFACE'], 0, 2) == "wl") {
        fprintf(
            $f,
            "SSID=\"%s\"\n" .
            "PSK=\"%s\"\n" .
            "HIDDEN=%s\n" .
            "WPA3=%s\n",
            $data['SSID'],
            $data['PSK'],
            $data['HIDDEN'],
            $data['WPA3']
        );
        fprintf(
            $f,
            "BACKUPSSID=\"%s\"\n" .
            "BACKUPPSK=\"%s\"\n" .
            "BACKUPHIDDEN=%s\n" .
            "BACKUPWPA3=%s\n",
            $data['BACKUPSSID'],
            $data['BACKUPPSK'],
            $data['BACKUPHIDDEN'],
            $data['BACKUPWPA3']
        );
    }
    if (isset($data['DHCPSERVER'])) {
        fprintf($f, "DHCPSERVER=%d\n", $data['DHCPSERVER'] ? "1" : 0);
        if ($data['DHCPSERVER']) {
            if (isset($data['Leases'])) {
                $lf = fopen($leasesFile, "w");
                foreach ($data['Leases'] as $ip => $mac) {
                    fprintf($lf, "[DHCPServerStaticLease]\n");
                    fprintf($lf, "MACAddress=%s\n", $mac);
                    fprintf($lf, "Address=%s\n\n", $ip);
                }
                fclose($lf);
            } else if (file_exists($leasesFile)) {
                unlink($leasesFile);
            }
        }
    }
    if (isset($data['DHCPOFFSET'])) {
        fprintf($f, "DHCPOFFSET=%d\n", $data['DHCPOFFSET']);
    }
    if (isset($data['DHCPPOOLSIZE'])) {
        fprintf($f, "DHCPPOOLSIZE=%d\n", $data['DHCPPOOLSIZE']);
    }
    if (isset($data['ROUTEMETRIC'])) {
        fprintf($f, "ROUTEMETRIC=%d\n", $data['ROUTEMETRIC']);
    }
    if (isset($data['IPFORWARDING'])) {
        fprintf($f, "IPFORWARDING=%d\n", $data['IPFORWARDING']);
    }
    fclose($f);

    return json(array("status" => "OK"));

}

/**
 * Set networking configuration
 *
 * Applies the networking settings for the specified `{interface}` at the OS
 * level and restarts the interface.
 *
 * @route POST /api/network/interface/{interface}/apply
 * @response 200 Networking configuration applied
 * ```json
 * {"status": "OK", "output": []}
 * ```
 */
function NetworkApplyInterface()
{
    global $settings, $SUDO;

    $interface = params('interface');

    exec($SUDO . " " . $settings['fppDir'] . "/src/fppinit setupNetwork $interface", $output);
    return json(array("status" => "OK", "output" => $output));
}
