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


/////////////////////////////////////////////////////////////////////////////
?>
