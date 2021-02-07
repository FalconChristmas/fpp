<?

/////////////////////////////////////////////////////////////////////////////
// GET /api/system/reboot
function RebootDevice()
{
    global $settings;
    global $SUDO;

    $status = exec($SUDO . " shutdown -r now");
    $output = array("status" => "OK");
    return json($output);
}

// GET /api/system/shutdown
function SystemShutdownOS()
{
    global $SUDO;

    $status = exec($SUDO . " shutdown -h now");
    $output = array("status" => "OK");
    return json($output);
}

// GET /api/system/fppd/start
function StartFPPD()
{
    global $settingsFile, $SUDO;
    $rc = "Already Running";
    $status = exec("if ps cax | grep -q fppd; then echo \"true\"; else echo \"false\"; fi");
    if ($status == 'false') {
        exec($SUDO . " /opt/fpp/scripts/fppd_start");
        $rc = "OK";
    }
    $output = array("status" => $rc);
    return json($output);
}

function StopFPPDNoStatus()
{
    global $SUDO;

    // Stop Playing
    SendCommand('d');

    // Shutdown
    SendCommand('q'); // Ignore return and just kill if 'q' doesn't work...
    // wait half a second for shutdown and outputs to close

    if (file_exists("/.dockerenv")) {
        usleep(500000);
        // kill it if it's still running
        exec($SUDO . " " . dirname(dirname(__FILE__)) . "/scripts/fppd_stop");
    } else {
        // systemctl uses fppd_stop to stop fppd, but we need systemctl to know
        exec($SUDO . " systemctl stop fppd");
    }
}

// GET /api/system/fppd/start

function StopFPPD()
{
    StopFPPDNoStatus();
    $output = array("status" => "OK");
    return json($output);
}

function RestartFPPD()
{

    if ((isset($_GET['quick'])) && ($_GET['quick'] == 1)) {
        $status = exec("if ps cax | grep -q fppd; then echo \"true\"; else echo \"false\"; fi");
        if ($status == 'true') {
            SendCommand('restart');
            
            $output = array("status" => "OK");
            return json($output);
        }
    }

    StopFPPDNoStatus();
    return StartFPPD();
}
