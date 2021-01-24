<?

require_once("../common.php");

function network_list_interfaces()
{
	$output = array();
	$cmd = "ip --json -4 address show";
	exec($cmd, $output);
	return json(json_decode(join(" ", $output)));
	#return json(join(" ", $output));
}

function network_wifi_strength()
{
	return json(network_wifi_strength_obj());
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

	$interfaces = explode("\n",trim(shell_exec("/sbin/ifconfig -a | cut -f1 -d' ' | grep -v ^$ | grep -v lo | grep -v eth0:0 | grep -v usb | grep -v SoftAp | grep -v 'can.' | grep -v tether ")));
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
    


/////////////////////////////////////////////////////////////////////////////
?>
