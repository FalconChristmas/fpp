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

?>