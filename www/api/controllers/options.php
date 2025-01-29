<?
/////////////////////////////////////////////////////////////////////////////
if (!function_exists('str_ends_with')) {
    function str_ends_with(string $haystack, string $needle): bool
    {
        $needle_len = strlen($needle);
        return ($needle_len === 0 || 0 === substr_compare($haystack, $needle, -$needle_len));
    }
}

function GetAudioCurrentCard()
{
    global $SUDO, $settings;

    if ($settings["Platform"] == "MacOS") {
        exec("system_profiler SPAudioDataType", $output);
        $cards = array();
        $curCard = "";
        foreach ($output as $line) {
            $tline = trim($line);
            if ($tline != "" && $tline != "Audio:" && $tline != "Device:") {
                if (str_ends_with($tline, ":")) {
                    $curCard = rtrim($tline, ":");
                } else {
                    $values = explode(':', $tline);
                    $key = trim($values[0]);
                    if ($key == "Default Output Device") {
                        return $curCard;
                    }
                }
            }
        }
    } else {
        exec($SUDO . " grep card /root/.asoundrc | head -n 1 | awk '{print $2}'", $output, $return_val);
        if ($return_val) {
            error_log("Error getting currently selected alsa card used!");
        } else {
            if (isset($output[0])) {
                $CurrentCard = $output[0];
            } else {
                $CurrentCard = "0";
            }
        }
        unset($output);
    }

    return $CurrentCard;
}

/////////////////////////////////////////////////////////////////////////////
function GetOptions_AudioMixerDevice()
{
    global $SUDO;
    global $settings;

    $CurrentCard = GetAudioCurrentCard();
    $MixerDevices = array();
    if ($settings["Platform"] == "MacOS") {
        $MixerDevices[$CurrentCard] = $CurrentCard;
    } else {
        exec($SUDO . " amixer -c $CurrentCard scontrols | cut -f2 -d\"'\"", $output, $return_val);
        if ($return_val || strpos($output[0], "Usage:") === 0) {
            error_log("Error getting mixer devices!");
            $AudioMixerDevice = "PCM";
        } else {
            foreach ($output as $device) {
                $MixerDevices[$device] = $device;
            }
        }
        unset($output);
    }

    return json($MixerDevices);
}

/////////////////////////////////////////////////////////////////////////////
function GetOptions_AudioOutputDevice($fulllist = false)
{
    global $SUDO, $GET, $settings;

    if ($settings["Platform"] == "MacOS") {
        exec("system_profiler SPAudioDataType", $output);
        $cards = array();
        $cards["--System Default--"] = "--System Default--";
        $curCard = "";
        foreach ($output as $line) {
            $tline = trim($line);
            if ($tline != "" && $tline != "Audio:" && $tline != "Device:") {
                if (str_ends_with($tline, ":")) {
                    $curCard = rtrim($tline, ":");
                } else {
                    $values = explode(':', $tline);
                    $key = trim($values[0]);
                    if ($key == "Output Source") {
                        $cards[$curCard] = $curCard;
                    } else if ($key == "Default Output Device") {

                    }
                }
            }
        }
        unset($output);
        return json($cards);
    } else {
        $CurrentCard = GetAudioCurrentCard();
        $AlsaCards = array();
        if ($fulllist) {
            exec($SUDO . " aplay -l | grep '^card' | sed -e 's/^card //' -e 's/.*\[\(.*\)\].*\[\(.*\)\]/\\1, \\2/'", $output, $return_val);
        } else {
            exec($SUDO . " aplay -l | grep '^card' | sed -e 's/^card //' -e 's/:[^\[]*\[/:/' -e 's/\].*\[.*\].*//' | uniq", $output, $return_val);
        }
        if ($return_val) {
            error_log("Error getting alsa cards for output!");
        } else {
            $foundOurCard = 0;
            foreach ($output as $card) {
                $values = explode(':', $card);

                if ($values[0] == $CurrentCard) {
                    $foundOurCard = 1;
                }

                if ($fulllist) {
                    $AlsaCards[] = $card;
                } else {
                    if ($values[1] == "bcm2835 ALSA") {
                        $AlsaCards[$values[1] . " (Pi Onboard Audio)"] = $values[0];
                    } else if ($values[1] == "CD002") {
                        $AlsaCards[$values[1] . " (FM Transmitter)"] = $values[0];
                    } else {
                        $AlsaCards[$values[1]] = $values[0];
                    }

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
}

/////////////////////////////////////////////////////////////////////////////
function GetOptions_AudioInputDevice($fulllist = false)
{
    global $SUDO;

    $AlsaCards = array();
    if ($fulllist) {
        exec($SUDO . " arecord -l | grep '^card' | sed -e 's/^card //' -e 's/.*\[\(.*\)\].*\[\(.*\)\]/\\1, \\2/'", $output, $return_val);
    } else {
        exec($SUDO . " arecord -l | grep '^card' | sed -e 's/^card //' -e 's/:[^\[]*\[/:/' -e 's/\].*\[.*\].*//' | uniq", $output, $return_val);
    }
    if ($return_val) {
        error_log("Error getting alsa cards for input!");
    } else {
        foreach ($output as $card) {
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
function GetOptions_FrameBuffer()
{
    global $settings, $SUDO;

    exec($SUDO . " " . $settings["fppBinDir"] . "/fpp -FB", $output, $return_val);
    return $output[0];
}

/////////////////////////////////////////////////////////////////////////////
function GetOptions_Locale()
{
    global $settings;

    $locales = array();
    foreach (scandir($settings['fppDir'] . '/etc/locale') as $file) {
        if (preg_match('/.json$/', $file)) {
            $file = preg_replace('/.json$/', '', $file);
            $locales[$file] = $file;
        }
    }
    ksort($locales);

    return json($locales);
}

/////////////////////////////////////////////////////////////////////////////
function GetOptions_RTC()
{
    global $settings;
    $rtcOptions = array();

    $rtcOptions['None'] = 'N';
    $rtcOptions['DS1305 / DS1307 / DS3231 (PiCap)'] = '2';
    $rtcOptions['pcf8523 (Kulp / Adafruit PiRTC)'] = '4';
    $rtcOptions['pcf85363 (Kulp)'] = '5';
    $rtcOptions['mcp7941x (PiFace)'] = '3';
    $rtcOptions['pcf2127 (RasClock)'] = '1';

    return json($rtcOptions);
}

/////////////////////////////////////////////////////////////////////////////
function GetOptions_TimeZone()
{
    global $settings;
    $zones = array();

    exec("find /usr/share/zoneinfo ! -type d | sed 's/\/usr\/share\/zoneinfo\///' | grep -v ^right | grep -v ^posix | grep -v ^\\/ | grep -v \\\\. | sort", $output, $return_val);

    if ($return_val != 0) {
        return json($zones);
    }

    foreach ($output as $zone) {
        array_push($zones, $zone);
    }

    return json($zones);
}

/////////////////////////////////////////////////////////////////////////////
function GetOptions_VideoOutput($playlist)
{
    global $settings;

    $VideoOutputModels = array();
    if ($playlist) {
        $VideoOutputModels['--Default--'] = "--Default--";
    }
    $path = "/sys/class/drm/";
    $files = array_diff(scandir($path), array('.', '..'));
    foreach ($files as $file) {
        if (str_contains($file, "HDMI")) {
            $file = substr($file, stripos($file, "HDMI"));
            $VideoOutputModels[$file] = $file;
        } else if (str_contains($file, "DSI-")) {
            $file = substr($file, stripos($file, "DSI-"));
            $VideoOutputModels[$file] = $file;
        } else if (str_contains($file, "Composite-")) {
            $file = substr($file, stripos($file, "Composite-"));
            $VideoOutputModels[$file] = $file;
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
            if ($value->Type != "FB" || !$value->autoCreated) {
                $VideoOutputModels[$value->Name] = $value->Name;
            }
        }
    }
    return json($VideoOutputModels);
}


function GetOptions_BBBLeds()
{
    global $settings;
    $options = array();
    $options["Disabled"] = "none";
    $options["Heartbeat"] = "heartbeat";
    $options["CPU Activity"] = "cpu";
    if ($settings["Platform"] == "BeagleBone 64") {
        $options["SD Card Activity"] = "mmc1";
        $options["eMMC Activity"] = "mmc0";
    } else {
        $options["SD Card Activity"] = "mmc0";
        $options["eMMC Activity"] = "mmc1";
    }
    return json($options);
}

/////////////////////////////////////////////////////////////////////////////
// GET /api/options/:SettingName
function GetOptions()
{
    $SettingName = params('SettingName');

    switch ($SettingName) {
        case 'AudioMixerDevice':
            return GetOptions_AudioMixerDevice();
        case 'AudioOutput':
            return GetOptions_AudioOutputDevice(false);
        case 'AudioInput':
            return GetOptions_AudioInputDevice(false);
        case 'AudioOutputList':
            return GetOptions_AudioOutputDevice(true);
        case 'AudioInputList':
            return GetOptions_AudioInputDevice(true);
        case 'FrameBuffer':
            return GetOptions_FrameBuffer();
        case 'BBBLeds':
            return GetOptions_BBBLeds();
        case 'Locale':
            return GetOptions_Locale();
        case 'RTC':
            return GetOptions_RTC();
        case 'PlaylistVideoOutput':
            return GetOptions_VideoOutput(1);
        case 'TimeZone':
            return GetOptions_TimeZone();
        case 'VideoOutput':
            return GetOptions_VideoOutput(0);
    }

    return json("{}");
}
