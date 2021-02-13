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

// GET /api/system/fppd/restart
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

// GET /api/system/releaseNotes/:version
function ViewReleaseNotes()
{
    $version = params('version');

    ini_set('user_agent', 'Mozilla/4.0 (compatible; MSIE 6.0)');
    $json = file_get_contents("https://api.github.com/repos/FalconChristmas/fpp/releases/tags/" . $version);

    $data = json_decode($json, true);
    if (isset($data['body'])) {
        $keys = array("draft", "prerelease", "body", "published_at");
        $rc = array("status" => "OK");
        foreach ($keys as $key) {
            $rc[$key] = $data[$key];
        }
        return json($rc);
    } else {
        return json(array("status" => "Release not found"));
    }
}

// PUT /system/volume
function SystemSetAudio()
{
    global $SUDO;
    global $settings;

    $rc = "OK";
    $json = strval(file_get_contents('php://input'));
    $input = json_decode($json, true);
    $vol = 75;
    if (isset($input['volume'])) {
        $vol = intval($input['volume']);
    }

    if ($vol < 0) {
        $vol = 0;
    }
    if ($vol > 100) {
        $vol = 100;
    }

    WriteSettingToFile("volume", $vol);

    $status = SendCommand('v,' . $vol . ',');

    $card = 0;
    if (isset($settings['AudioOutput'])) {
        $card = $settings['AudioOutput'];
    } else {
        exec($SUDO . " grep card /root/.asoundrc | head -n 1 | awk '{print $2}'", $output, $return_val);
        if ($return_val) {
            // Should we error here, or just move on?
            // Technically this should only fail on non-pi
            // and pre-0.3.0 images
            $rc = "Error retrieving current sound card, using default of '0'!";
        } else {
            $card = $output[0];
        }

        WriteSettingToFile("AudioOutput", $card);
    }

    $mixerDevice = "PCM";
    if (isset($settings['AudioMixerDevice'])) {
        $mixerDevice = $settings['AudioMixerDevice'];
    } else {
        unset($output);
        exec($SUDO . " amixer -c $card scontrols | head -1 | cut -f2 -d\"'\"", $output, $return_val);
        $mixerDevice = $output[0];
        WriteSettingToFile("AudioMixerDevice", $mixerDevice);
    }

    if ($card == 0 && $settings['Platform'] == "Raspberry Pi" && $settings['AudioCard0Type'] == "bcm2") {
        $vol = 50 + ($vol / 2.0);
    }

    // Why do we do this here and in fppd's settings.c
    $status = exec($SUDO . " amixer -c $card set $mixerDevice -- " . $vol . "%");

    return json(array("status" => $rc, "volume" => $vol));
}

// GET /system/volume
function SystemGetAudio()
{
    global $settings;

    $curl = curl_init('http://localhost:32322/fppd/status');
    curl_setopt($curl, CURLOPT_FAILONERROR, true);
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 200);
    $request_content = curl_exec($curl);
    curl_close($curl);

    $method = "FPPD";
    $vol = 75;
    if ($request_content === false) {
        $method = "Default";
        if (isset($settings['volume'])) {
            $vol = $settings['volume'];
            $method = "Settings";
        }
    } else {
        $data = json_decode($request_content, true);
        $vol = $data['volume'];
    }

    return json(array("status" => "OK", "method" => $method, "volume" => $vol));
}
