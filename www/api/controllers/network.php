<?

require_once "../common.php";

function network_list_interfaces()
{
    return json(network_list_interfaces_obj());
}

function network_wifi_strength()
{
    return json(network_wifi_strength_obj());
}

function network_wifi_scan()
{
    $networks = array();
    $current = array();
    $interface = params('interface');

    # Validate interface.   -- Important because of SUDO
    $interfaces = json_decode(network_list_interfaces(), true);
    $found = false;

    foreach ($interfaces as $row) {
        if ($row["ifname"] == $interface) {
            $found = true;
        }
    }

    if (!$found) {
        return json(array("status" => "Invalid Interface", "networks" => array()));
    }

    $cmd = "sudo /sbin/iw dev $interface scan";
    exec($cmd, $output);
    $outputLine1 = $output;
    //if (!is_string($output)) {
    //    $outputLine1 = $output[0];
    //}
    if (strpos($outputLine1, "Network is down") !== false) {
        exec("sudo /sbin/ifconfig $interface up", $output);

        $output = array();
        $cmd = "sudo /sbin/iw dev $interface scan";
        exec($cmd, $output);
    }
    foreach ($output as $row) {
        if (startsWith($row, "BSS")) {
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
        }
    }
    if (count($current) > 0) {
        array_push($networks, $current);
    }

    return json(array("status" => "OK", "networks" => $networks));
}

function network_presisentNames_delete()
{
    shell_exec("sudo rm -f /etc/systemd/network/5?-fpp-*.link");
    shell_exec("sudo ln -sf /dev/null /etc/systemd/network/99-default.link");
    shell_exec("sudo ln -sf /dev/null /etc/systemd/network/73-usb-net-by-mac.link");

    $output = array("status" => "OK");
    return json($output);
}

function network_presisentNames_create()
{
    global $settings;
    network_presisentNames_delete();

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
                $cont = file_get_contents($settings['configDirectory'] . "/interface." . $iface);
                $cont = str_replace($iface, $new_iface, $cont);
                file_put_contents($settings['configDirectory'] . "/interface." . $new_iface, $cont);
            }
        } else {
            $cont = $cont . "\n\n[Link]\nNamePolicy=\nName=" . $iface . "\n";
            file_put_contents("/tmp/5" . strval($count) . "-fpp-" . $iface . ".link", $cont);
            shell_exec("sudo mv /tmp/5" . strval($count) . "-fpp-" . $iface . ".link /etc/systemd/network/");
        }

        shell_exec("sudo  rm -f /etc/systemd/network/99-default.link");
        shell_exec("sudo  rm -f /etc/systemd/network/73-usb-net-by-mac.link");

        unset($output);
        $count = $count + 1;
    }

    $output = array("status" => "OK", "interfaceCnt" => $count);
    return json($output);

}

// GET /api/network/dns
function network_get_dns()
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

// POST /api/network/dns
function network_save_dns()
{
    global $settings;

    $data = json_decode(file_get_contents('php://input'), true);

    $cfgFile = $settings['configDirectory'] . "/dns";

    $f = fopen($cfgFile, "w");
    if ($f == false) {
        return;
    }

    fprintf($f,
        "DNS1=\"%s\"\n" .
        "DNS2=\"%s\"\n",
        $data['DNS1'], $data['DNS2']);
    fclose($f);

    //Trigger a JSON Configuration Backup
    GenerateBackupViaAPI('DNS Configuration was modified.');

    return json(array("status" => "OK", "DNS" => $data));
}

// GET /network/interface/:interface
function network_get_interface()
{
    global $settings;

    $interface = params('interface');
    $result = array("status" => "ERROR: Interface not found");

    $cfgFile = $settings['configDirectory'] . "/interface." . $interface;
    if (file_exists($cfgFile)) {
        $result = parse_ini_file($cfgFile);
        $result['status'] = 'OK';
    }

    exec("/sbin/ifconfig $interface", $output);
    foreach ($output as $line) {
        if (preg_match('/inet /', $line)) {
            $result['CurrentAddress'] = preg_replace('/.*inet ([0-9\.]+) .*/', '$1', $line);
            $result['CurrentNetmask'] = preg_replace('/.*netmask ([0-9\.]+).*/', '$1', $line);
        }
        if (preg_match('/flags=/', $line)) {
            $result['status'] = 'OK';
        }
    }
    unset($output);

    if (substr($interface, 0, 2) == "wl") {
        exec("/sbin/iwconfig $interface", $output);
        foreach ($output as $line) {
            if (preg_match('/ESSID:/', $line) && !preg_match('/ESSID:off/', $line)) {
                $result['CurrentSSID'] = preg_replace('/.*ESSID:"([^"]+)".*/', '$1', $line);
            }
            if (preg_match('/Rate=/', $line)) {
                $result['CurrentRate'] = preg_replace('/.*Bit Rate=([0-9\.]+) .*/', '$1', $line);
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
                    } else if (startsWith($line, "MACAddress=")) {
                        $mac = substr($line, 11);
                    } else if (startsWith($line, "Address=")) {
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

            if (!$inLeases && startsWith($line, "Offered DHCP leases")) {
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

// GET /network/interface/add/:interface
function network_add_interface()
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

// POST /network/interface/:interface
function network_set_interface()
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
        fprintf($f,
            "INTERFACE=\"%s\"\n" .
            "PROTO=\"static\"\n" .
            "ADDRESS=\"%s\"\n" .
            "NETMASK=\"%s\"\n" .
            "GATEWAY=\"%s\"\n",
            $data['INTERFACE'], $data['ADDRESS'], $data['NETMASK'],
            $data['GATEWAY']);

    } else if ($data['PROTO'] == "dhcp") {
        fprintf($f,
            "INTERFACE=%s\n" .
            "PROTO=dhcp\n",
            $data['INTERFACE']);
    }

    if (substr($data['INTERFACE'], 0, 2) == "wl") {
        fprintf($f,
            "SSID=\"%s\"\n" .
            "PSK=\"%s\"\n" .
            "HIDDEN=%s\n",
            $data['SSID'], $data['PSK'], $data['HIDDEN']);
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
/////////////////////////////////////////////////////////////////////////////

function network_apply_interface()
{
    global $settings, $SUDO;

    $interface = params('interface');

    exec($SUDO . " " . $settings['fppDir'] . "/scripts/config_network  $interface", $output);
    return json(array("status" => "OK", "output" => $output));
}
