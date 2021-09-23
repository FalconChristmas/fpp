<?
/////////////////////////////////////////////////////////////////////////////
function GetAudioCurrentCard() {
    global $SUDO;

    exec($SUDO . " grep card /root/.asoundrc | head -n 1 | awk '{print $2}'", $output, $return_val);
    if ( $return_val )
    {
        error_log("Error getting currently selected alsa card used!");
    }
    else
    {
        if (isset($output[0]))
            $CurrentCard = $output[0];
        else
            $CurrentCard = "0";
    }
    unset($output);

    return $CurrentCard;
}

/////////////////////////////////////////////////////////////////////////////
function GetOptions_AudioMixerDevice() {
    global $SUDO;
    global $settings;

    $CurrentCard = GetAudioCurrentCard();

    $MixerDevices = Array();
    exec($SUDO . " amixer -c $CurrentCard scontrols | cut -f2 -d\"'\"", $output, $return_val);
    if ( $return_val || strpos($output[0], "Usage:") === 0) {
        error_log("Error getting mixer devices!");
        $AudioMixerDevice = "PCM";
    } else {
        foreach($output as $device)
        {
            $MixerDevices[$device] = $device;
        }
    }
    unset($output);

    return json($MixerDevices);
}

/////////////////////////////////////////////////////////////////////////////
function GetOptions_AudioOutputDevice($fulllist = false) {
    global $SUDO, $GET;

    $CurrentCard = GetAudioCurrentCard();

    $AlsaCards = Array();
    if ($fulllist) {
        exec("sudo aplay -l | grep '^card' | sed -e 's/^card //' -e 's/.*\[\(.*\)\].*\[\(.*\)\]/\\1, \\2/'", $output, $return_val);
    } else {
        exec($SUDO . " aplay -l | grep '^card' | sed -e 's/^card //' -e 's/:[^\[]*\[/:/' -e 's/\].*\[.*\].*//' | uniq", $output, $return_val);
    }
    if ($return_val) {
        error_log("Error getting alsa cards for output!");
    } else {
        $foundOurCard = 0;
        foreach($output as $card) {
            $values = explode(':', $card);

            if ($values[0] == $CurrentCard)
                $foundOurCard = 1;

            if ($fulllist) {
                $AlsaCards[] = $card ;
            } else {
                if ($values[1] == "bcm2835 ALSA")
                    $AlsaCards[$values[1] . " (Pi Onboard Audio)"] = $values[0];
                else if ($values[1] == "CD002")
                    $AlsaCards[$values[1] . " (FM Transmitter)"] = $values[0];
                else
                    $AlsaCards[$values[1]] = $values[0];
            }
        }

        if (!$foundOurCard) {
            if (!$fulllist) {
                $AlsaCards['-- Select an Audio Device --'] = $CurrentCard;
            }
        } 
    }
    unset($output);

    return json($AlsaCards);
}

/////////////////////////////////////////////////////////////////////////////
function GetOptions_AudioInputDevice($fulllist = false) {
    global $SUDO;

    $AlsaCards = Array();
    if ($fulllist) {
        exec("sudo arecord -l | grep '^card' | sed -e 's/^card //' -e 's/.*\[\(.*\)\].*\[\(.*\)\]/\\1, \\2/'", $output, $return_val);
    } else {
        exec("sudo arecord -l | grep '^card' | sed -e 's/^card //' -e 's/:[^\[]*\[/:/' -e 's/\].*\[.*\].*//' | uniq", $output, $return_val);
    }
    if ( $return_val ) {
        error_log("Error getting alsa cards for input!");
    } else {
        foreach($output as $card) {
            if ($fulllist) {
                $AlsaCards[] = $card;
            } else {
                $values = explode(':', $card);
                $AlsaCards[$values[1]] = $values[0];
            }
        }
    }
    unset($output);

    return json($AlsaCards);
}

/////////////////////////////////////////////////////////////////////////////
function GetOptions_FrameBuffer() {
    $framebuffers = Array();

    if (file_exists('/dev/fb0'))
        $framebuffers['/dev/fb0'] = '/dev/fb0';
    if (file_exists('/dev/fb1'))
        $framebuffers['/dev/fb1'] = '/dev/fb1';

    return json($framebuffers);
}

/////////////////////////////////////////////////////////////////////////////
function GetOptions_Locale() {
    global $settings;

    $locales = Array();
    foreach (scandir($settings['fppDir'] . '/etc/locale') as $file)
    {
        if (preg_match('/.json$/', $file))
        {
            $file = preg_replace('/.json$/', '', $file);
            $locales[$file] = $file;
        }
    }
    ksort($locales);

    return json($locales);
}

/////////////////////////////////////////////////////////////////////////////
function GetOptions_RTC() {
    global $settings;
    $rtcOptions = Array();

    $rtcOptions['None'] = 'N';
    $rtcOptions['DS1305 / DS1307 / DS3231 (PiCap)'] = '2';
    $rtcOptions['pcf8523 (Kulp / Adafruit PiRTC)'] = '4';
    $rtcOptions['pcf85363 (Kulp)'] = '5';
    $rtcOptions['mcp7941x (PiFace)'] = '3';
    $rtcOptions['pcf2127 (RasClock)'] = '1';

    return json($rtcOptions);
}

/////////////////////////////////////////////////////////////////////////////
function GetOptions_TimeZone() {
    global $settings;
    $zones = Array();

    exec("find /usr/share/zoneinfo ! -type d | sed 's/\/usr\/share\/zoneinfo\///' | grep -v ^right | grep -v ^posix | grep -v ^\\/ | grep -v \\\\. | sort", $output, $return_val);

    if ($return_val != 0)
        return json($zones);

    foreach ($output as $zone) {
        array_push($zones, $zone);
    }

    return json($zones);
}

/////////////////////////////////////////////////////////////////////////////
function GetOptions_VideoOutput($playlist) {
    global $settings;

    $VideoOutputModels = Array();
    if ($playlist) {
        $VideoOutputModels['--Default--'] = "--Default--";
    } else {
        if ($settings['Platform'] != "BeagleBone Black") {
            $VideoOutputModels['HDMI'] = "HDMI";
        }
    }
    $VideoOutputModels['Disabled'] = "Disabled";

    $curl = curl_init('http://localhost:32322/models');
    curl_setopt($curl, CURLOPT_FAILONERROR, true);
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 200);
    $request_content = curl_exec($curl);
    curl_close($curl);
    if ($request_content !== false) {
        $data = json_decode($request_content);
        foreach ($data as $value) {
            $VideoOutputModels[$value->Name] = $value->Name;
        }
    }
    return json($VideoOutputModels);
}

/////////////////////////////////////////////////////////////////////////////
// GET /api/options/:SettingName
function GetOptions() {
    $SettingName = params('SettingName');

    switch ($SettingName) {
        case 'AudioMixerDevice':    return GetOptions_AudioMixerDevice();
        case 'AudioOutput':         return GetOptions_AudioOutputDevice(false);
        case 'AudioInput':          return GetOptions_AudioInputDevice(false);
        case 'AudioOutputList':     return GetOptions_AudioOutputDevice(true);
        case 'AudioInputList':      return GetOptions_AudioInputDevice(true);
        case 'FrameBuffer':         return GetOptions_FrameBuffer();
        case 'Locale':              return GetOptions_Locale();
        case 'RTC':                 return GetOptions_RTC();
        case 'PlaylistVideoOutput': return GetOptions_VideoOutput(1);
        case 'TimeZone':            return GetOptions_TimeZone();
        case 'VideoOutput':         return GetOptions_VideoOutput(0);
    }

    return json("{}");
}

?>
