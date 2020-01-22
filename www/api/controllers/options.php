<?

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

function GetOptions_AudioOutputDevice() {
    global $SUDO;

    $CurrentCard = GetAudioCurrentCard();

    $AlsaCards = Array();
    exec($SUDO . " aplay -l | grep '^card' | sed -e 's/^card //' -e 's/:[^\[]*\[/:/' -e 's/\].*\[.*\].*//' | uniq", $output, $return_val);
    if ( $return_val )
    {
        error_log("Error getting alsa cards for output!");
    }
    else
    {
        $foundOurCard = 0;
        foreach($output as $card)
        {
            $values = explode(':', $card);

            if ($values[0] == $CurrentCard)
                $foundOurCard = 1;

            if ($values[1] == "bcm2835 ALSA")
                $AlsaCards[$values[1] . " (Pi Onboard Audio)"] = $values[0];
            else if ($values[1] == "CD002")
                $AlsaCards[$values[1] . " (FM Transmitter)"] = $values[0];
            else
                $AlsaCards[$values[1]] = $values[0];
        }

        if (!$foundOurCard)
            $AlsaCards['-- Select an Audio Device --'] = $CurrentCard;
    }
    unset($output);

    return json($AlsaCards);
}

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

function GetOptions_VideoOutput() {
    global $settings;

    $VideoOutputModels = Array();
    if ($settings['Platform'] != "BeagleBone Black") {
        $VideoOutputModels['HDMI'] = "--HDMI--";
    }
    $VideoOutputModels['Disabled'] = "--Disabled--";
    if (file_exists($settings['model-overlays'])) {
        $json = json_decode(file_get_contents($settings['model-overlays']));
        foreach ($json->models as $value) {
            $VideoOutputModels[$value->Name] = $value->Name;
        }
    }

    return json($VideoOutputModels);
}

?>
