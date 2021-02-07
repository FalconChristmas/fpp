<?


/////////////////////////////////////////////////////////////////////////////
// GET /api/system/reboot
function RebootDevice() {
    global $settings;
    global $SUDO;

	$status=exec($SUDO . " shutdown -r now");
    $output = array("status" => "OK");    
    return json($output);
}

// GET /api/system/shutdown
function SystemShutdownOS()
{
	global $SUDO;

	$status=exec($SUDO . " shutdown -h now");
    $output = array("status" => "OK");    
    return json($output);
}


?>