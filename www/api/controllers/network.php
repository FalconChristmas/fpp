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
    $output = array("status" => "OK");
    return json($output);
}

function network_presisentNames_create()
{
    global $settings;
    network_presisentNames_delete();

    $interfaces = explode("\n", trim(shell_exec("/sbin/ifconfig -a | cut -f1 -d' ' | grep -v ^$ | grep -v lo | grep -v eth0:0 | grep -v usb | grep -v SoftAp | grep -v 'can.' | grep -v tether ")));
    $count = 0;
    foreach ($interfaces as $iface) {
        $iface = preg_replace("/:$/", "", $iface);
        $cmd = "ip link show " . $iface . " | grep ether | awk '{split($0, a,\" \"); print a[2];}'";
        exec($cmd, $output, $return_val);

        $cont = "[Match]\nMACAddress=" . $output[0];
        if (substr($iface, 0, strlen("wlan")) === "wlan") {
            $cont = $cont . "\nOriginalName=wlan*";
        }
        $cont = $cont . "\n[Link]\nName=" . $iface . "\n";
        file_put_contents("/tmp/5" . strval($count) . "-fpp-" . $iface . ".link", $cont);
        shell_exec("sudo mv /tmp/5" . strval($count) . "-fpp-" . $iface . ".link /etc/systemd/network/");

        unset($output);
        $count = $count + 1;
    }

    $output = array("status" => "OK", "interfaceCnt" => $count);
    return json($output);

}

// GET /api/network/dns
function network_get_dns() {
	global $settings;

	$cfgFile = $settings['configDirectory'] . "/dns";
	if (file_exists($cfgFile)) {
		$obj = parse_ini_file($cfgFile);
		$obj['status'] = "OK";
		return json($obj);
	}
	return json (Array("status"=> "Not Configured"));
}

// POST /api/network/dns
function network_save_dns() {
	global $settings;

	$data = json_decode(file_get_contents('php://input'), true);

	$cfgFile = $settings['configDirectory'] . "/dns";

	$f = fopen($cfgFile, "w");
	if ($f == FALSE) {
		return;
	}

	fprintf($f,
		"DNS1=\"%s\"\n" .
		"DNS2=\"%s\"\n",
		$data['DNS1'], $data['DNS2']);
	fclose($f);

	return json(array("status"=>"OK", "DNS"=> $data));
}


/////////////////////////////////////////////////////////////////////////////
