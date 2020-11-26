<?

/////////////////////////////////////////////////////////////////////////////
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
	$rc = array();
	$lines = file("/proc/net/wireless");
	#
	# Expected format
	# 
	# face | tus | link level noise |  nwid  crypt   frag  retry   misc | beacon | 22
	# wlan0: 0000   41.  -69.  -256        0      0      0   2042      0        0
	#
	foreach($lines as $cnt=>$line) {
		if ($cnt > 1) {
			$parts = preg_split("/\s+/", trim($line));
			$obj = new \stdClass();
			$obj->interface = rtrim($parts[0], ":");
			$obj->link      = intval($parts[2]);
			$obj->level     = intval($parts[3]);
			$obj->noise     = intval($parts[4]);

			if ($obj->level > -50) {
			   $obj->desc="excellent";
			} elseif ($obj->level > -60) {
			   $obj->desc="good";
			} elseif ($obj->level > -70) {
			   $obj->desc="fair";
			} else {
			   $obj->desc="weak";
			}
			array_push($rc, $obj);
		}
	}
	return json($rc);
}


/////////////////////////////////////////////////////////////////////////////
?>
