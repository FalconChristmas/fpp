<?

/**
 * Returns the name of the currently selected audio output card.
 *
 * @return string Card name or card number string.
 */
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
        // AudioOutput is persisted as a stable ALSA card ID. Normalize so a
        // legacy numeric value still resolves to the matching card ID, and an
        // empty value resolves to the first present card.
        $stored = isset($settings['AudioOutput']) ? $settings['AudioOutput'] : '';
        $CurrentCard = NormalizeAudioOutputToCardId($stored);
        if ($CurrentCard === '') {
            $CurrentCard = "0";
        }
    }

    return $CurrentCard;
}

/**
 * Returns the available audio mixer device options for the current card.
 *
 * @return string JSON object mapping mixer device names to themselves.
 */
function GetOptions_AudioMixerDevice()
{
    global $SUDO;
    global $settings;

    $CurrentCard = GetAudioCurrentCard();
    $MixerDevices = array();
    if ($settings["Platform"] == "MacOS") {
        $MixerDevices[$CurrentCard] = $CurrentCard;
    } else {
        // GetAudioCurrentCard() returns a stable ALSA card ID; amixer needs the
        // numeric index, so resolve it (fall back to the value if already numeric).
        $cardNum = ResolveAlsaCardIdToNumber($CurrentCard);
        if ($cardNum === '') {
            $cardNum = ctype_digit((string) $CurrentCard) ? $CurrentCard : '0';
        }
        exec($SUDO . " amixer -c $cardNum scontrols | cut -f2 -d\"'\"", $output, $return_val);
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

/**
 * Returns the available audio output device options.
 *
 * @param bool $fulllist When true, returns detailed card/subdevice list; otherwise returns unique card names.
 * @return string JSON object or array of available audio output devices.
 */
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
            // First pass: collect all card names to detect duplicates
            $cardNameCounts = array();
            foreach ($output as $card) {
                $values = explode(':', $card);
                if (count($values) >= 2) {
                    $name = $values[1];
                    if (!isset($cardNameCounts[$name])) {
                        $cardNameCounts[$name] = 0;
                    }
                    $cardNameCounts[$name]++;
                }
            }

            // Build the ALSA card ID map from /proc/asound/cards
            // Format: " 0 [S3             ]: USB-Audio - ..."
            $cardIdMap = array(); // card number -> ALSA card ID
            $cardsFile = @file_get_contents('/proc/asound/cards');
            if ($cardsFile) {
                if (preg_match_all('/^\s*(\d+)\s*\[([^\]]+)\]/m', $cardsFile, $cmatches, PREG_SET_ORDER)) {
                    foreach ($cmatches as $cm) {
                        $cardIdMap[$cm[1]] = trim($cm[2]);
                    }
                }
            }

            // User-defined sound card aliases (issue #2586)
            $audioCardAliases = LoadAudioCardAliases();

            foreach ($output as $card) {
                $values = explode(':', $card);
                $cardNum = $values[0];
                // The stable ALSA card ID is the persisted/selected value now,
                // so identify "our card" by ID rather than the volatile number.
                $thisCardId = isset($cardIdMap[$cardNum]) ? $cardIdMap[$cardNum] : '';

                if (($thisCardId !== '' && $thisCardId == $CurrentCard) || $cardNum == $CurrentCard) {
                    $foundOurCard = 1;
                }

                if ($fulllist) {
                    $AlsaCards[] = $card;
                } else {
                    $cardName = $values[1];
                    // Disambiguate duplicate names by appending ALSA card ID
                    $displayName = $cardName;
                    if (isset($cardNameCounts[$cardName]) && $cardNameCounts[$cardName] > 1) {
                        $cardId = $thisCardId !== '' ? $thisCardId : $cardNum;
                        $displayName = $cardName . ' [' . $cardId . ']';
                    }
                    if ($cardName == "bcm2835 ALSA") {
                        $displayName = $cardName . " (Pi Onboard Audio)";
                    } else if ($cardName == "CD002") {
                        $displayName = $cardName . " (FM Transmitter)";
                    }
                    // Apply user-defined alias (issue #2586) keyed by ALSA card ID
                    $displayName = ApplyAudioCardAlias($thisCardId, $displayName, $audioCardAliases);
                    // Store the stable card ID as the value so the selection
                    // survives card-number reordering (fall back to the number
                    // only when the ID can't be resolved).
                    $AlsaCards[$displayName] = $thisCardId !== '' ? $thisCardId : $cardNum;
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

/**
 * Returns the available audio input device options.
 *
 * @param bool $fulllist    When true, returns detailed card/subdevice list; otherwise returns unique card names.
 * @param bool $allowMedia  When true, prepends a "Playing Media" pseudo-device to the list.
 * @return string JSON array or object of available audio input devices.
 */
function GetOptions_AudioInputDevice($fulllist = false, $allowMedia = false)
{

    global $SUDO, $settings;

    $AlsaCards = array();

    // In PipeWire mode with fulllist, enumerate PipeWire Audio/Source nodes via pw-dump.
    // Stored value = node.name (stable PipeWire identifier), display = node.description.
    if ($fulllist && IsPipeWireBackend($settings)) {
        if ($allowMedia) {
            $AlsaCards['-- Playing Media --'] = '-- Playing Media --';
        }
        $pwCmd = $SUDO . ' PIPEWIRE_RUNTIME_DIR=/run/pipewire-fpp XDG_RUNTIME_DIR=/run/pipewire-fpp pw-dump 2>/dev/null';
        $pwJson = shell_exec($pwCmd);
        if ($pwJson) {
            $objects = json_decode($pwJson, true);
            if (is_array($objects)) {
                // First pass: collect all Audio/Source nodes
                $sources = array();
                foreach ($objects as $obj) {
                    $props = isset($obj['info']['props']) ? $obj['info']['props'] : null;
                    if (
                        $props &&
                        isset($props['media.class']) && $props['media.class'] === 'Audio/Source' &&
                        isset($props['node.name'])
                    ) {
                        $sources[] = array(
                            'nodeName' => $props['node.name'],
                            'nodeDesc' => isset($props['node.description']) ? $props['node.description'] : $props['node.name'],
                            'nodeNick' => isset($props['node.nick']) ? $props['node.nick'] : ''
                        );
                    }
                }

                // Count descriptions to detect duplicates
                $descCounts = array();
                foreach ($sources as $src) {
                    $d = $src['nodeDesc'];
                    $descCounts[$d] = isset($descCounts[$d]) ? $descCounts[$d] + 1 : 1;
                }

                // User-defined sound card aliases (issue #2586)
                $aliases = LoadAudioCardAliases();

                // Build labels: always append nick (ALSA card ID) so users
                // can identify identical hardware across different naming
                // sources (WirePlumber SPA database vs USB product string).
                foreach ($sources as $src) {
                    $label = $src['nodeDesc'];
                    $nick = $src['nodeNick'];
                    if ($nick && strpos($label, $nick) === false) {
                        $label .= ' (' . $nick . ')';
                    }
                    // Apply user-defined alias (issue #2586). The PipeWire
                    // node.nick is the ALSA card ID, which is our alias key.
                    if ($nick) {
                        $label = ApplyAudioCardAlias($nick, $label, $aliases);
                    }
                    $AlsaCards[$label] = $src['nodeName'];
                }
            }
        }
        return json($AlsaCards);
    }

    // ALSA mode (or non-fulllist)
    if ($allowMedia) {
        if ($fulllist) {
            $AlsaCards[] = '-- Playing Media --';
        } else {
            $AlsaCards[] = '-- Playing Media --';
            $AlsaCards['-- Playing Media --'] = '-- Playing Media --';
        }
    }

    if ($fulllist) {
        // Extract ALSA short card name (before '[') + device description (last '[...]').
        // Using the short name rather than the long description means identical-model cards
        // are distinguished (e.g. ICUSBAUDIO7D vs ICUSBAUDIO7D_1) via ALSA's own dedup
        // suffix, which is more stable than volatile card numbers.
        // awk splits on [] chars: field 1 = "card N: SHORTNAME ", field 2 = "card desc", etc.
        // Last word of field 1 = ALSA short name; $(NF-1) = last bracketed value = device name.
        exec($SUDO . " arecord -l | grep '^card' | awk -F'[][]+' '{gsub(/ *\$/, \"\", \$1); split(\$1, a, \" \"); print a[length(a)]\", \"\$(NF-1)}'", $output, $return_val);
    } else {
        exec($SUDO . " arecord -l | grep '^card' | sed -e 's/^card //' -e 's/:[^\[]*\[/:/' -e 's/\].*\[.*\].*//' | uniq", $output, $return_val);
    }
    if ($return_val) {
        error_log("Error getting alsa cards for input!");
    } else {
        // User-defined sound card aliases (issue #2586) keyed by ALSA card ID
        $audioCardAliases = LoadAudioCardAliases();
        $cardIdMap = array(); // cardNum -> ALSA card ID
        $cardsFile = @file_get_contents('/proc/asound/cards');
        if ($cardsFile && preg_match_all('/^\s*(\d+)\s*\[([^\]]+)\]/m', $cardsFile, $cm, PREG_SET_ORDER)) {
            foreach ($cm as $row) {
                $cardIdMap[$row[1]] = trim($row[2]);
            }
        }

        foreach ($output as $card) {
            if ($fulllist) {
                $AlsaCards[] = $card;
            } else {
                $values = explode(':', $card);
                $cardNum = $values[0];
                $cardName = $values[1];
                $thisCardId = isset($cardIdMap[$cardNum]) ? $cardIdMap[$cardNum] : '';
                $cardName = ApplyAudioCardAlias($thisCardId, $cardName, $audioCardAliases);
                if (isset($AlsaCards[$cardName])) {
                    // Duplicate name — rename the existing entry to include its card number
                    $existingNum = $AlsaCards[$cardName];
                    unset($AlsaCards[$cardName]);
                    $AlsaCards[$cardName . ' (Card ' . $existingNum . ')'] = $existingNum;
                    $AlsaCards[$cardName . ' (Card ' . $cardNum . ')'] = $cardNum;
                } else {
                    $AlsaCards[$cardName] = $cardNum;
                }
            }
        }
    }
    unset($output);

    return json($AlsaCards);
}

/**
 * Returns the available framebuffer device options from `fppd`.
 *
 * @return string Raw output string from the fpp -FB command.
 */
function GetOptions_FrameBuffer()
{
    global $settings, $SUDO;

    exec($SUDO . " " . $settings["fppBinDir"] . "/fpp -FB", $output, $return_val);
    return $output[0];
}

/**
 * Returns the available locale options from the FPP `etc/locale` directory.
 *
 * @return string JSON object mapping locale names to themselves, sorted alphabetically.
 */
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

/**
 * Returns the available real-time clock (RTC) module options.
 *
 * @return string JSON object mapping RTC chip names to their driver identifiers.
 */
function GetOptions_RTC()
{
    global $settings;
    $rtcOptions = array();

    $rtcOptions['None/Built In'] = 'N';
    $rtcOptions['DS1305 / DS1307 / DS3231 (PiCap)'] = '2';
    $rtcOptions['pcf8523 (Kulp / Adafruit PiRTC)'] = '4';
    $rtcOptions['pcf85363 (Kulp)'] = '5';
    $rtcOptions['mcp7941x (PiFace)'] = '3';
    $rtcOptions['pcf2127 (RasClock)'] = '1';
    $rtcOptions['pcf8563'] = '6';

    return json($rtcOptions);
}

/**
 * Returns all available timezone strings from `/usr/share/zoneinfo`.
 *
 * @return string JSON array of timezone name strings.
 */
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

/**
 * Returns the available video output options from DRM and the `fppd` model list.
 *
 * @param bool $playlist When true, prepends a "--Default--" entry for playlist use.
 * @return string JSON object mapping output names to themselves.
 */
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

/**
 * Returns the available BeagleBone Black LED trigger mode options.
 *
 * @return string JSON object mapping human-readable LED mode names to their trigger identifiers.
 */
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

/**
 * Returns available GPIO pins, optionally filtered by `cape-info` show/hide rules.
 *
 * @param bool $list When true, returns a name => label map; when false, returns the raw GPIO objects.
 * @return string JSON object or array of GPIO pin data.
 */
function GetOptions_GPIOS($list)
{
    global $settings;
    $data = json_decode(file_get_contents('http://127.0.0.1:32322/gpio'));
    $ret = array();
    $includeFilters = array(
    );
    $excludeFilters = array(
    );
    if (!isset($settings["showAllOptions"]) || $settings["showAllOptions"] == 0) {
        if (isset($settings['cape-info']['show']) && isset($settings['cape-info']['show']['gpio'])) {
            $includeFilters = $settings['cape-info']['show']['gpio'];
        }
        if (isset($settings['cape-info']['hide']) && isset($settings['cape-info']['hide']['gpio'])) {
            $excludeFilters = $settings['cape-info']['hide']['gpio'];
        }
    }
    foreach ($data as $gpio) {
        $pn = $gpio->pin;
        $hide = false;
        if (count($includeFilters) > 0) {
            $hide = true;
            foreach ($includeFilters as $value) {
                if (preg_match("/" . $value . "/", $pn) == 1) {
                    $hide = false;
                }
            }
        }
        if (count($excludeFilters) > 0) {
            foreach ($excludeFilters as $value) {
                if (preg_match("/" . $value . "/", $pn) == 1) {
                    $hide = true;
                }
            }
        }
        if (!$hide) {
            if ($list) {
                $ret[$gpio->pin] = $gpio->pin . " (GPIO " . $gpio->gpioChip . "/" . $gpio->gpioLine . ")";
            } else {
                $ret[] = $gpio;
            }
        }
    }

    return json($ret);
}

/////////////////////////////////////////////////////////////////////////////
function GetOptions_AES67Interface()
{
    $interfaces = array();
    $interfaces['(Default)'] = '';
    exec("ip -o link show | awk -F': ' '{print \$2}' | grep -v lo", $output, $return_val);
    if (!$return_val && !empty($output)) {
        foreach ($output as $iface) {
            $iface = trim($iface);
            if (!empty($iface)) {
                $interfaces[$iface] = $iface;
            }
        }
    }
    return json($interfaces);
}

/**
 * Get a setting's options
 *
 * Returns the available options for the specified setting.
 * Supports `AudioMixerDevice`, `AudioOutput`, `AudioInput`, and other platform-specific option sets.
 *
 * @route GET /api/options/{SettingName}
 * @response 200 Available options for the setting
 * ```json
 * {"Dummy": "0"}
 * ```
 */
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
        case 'AudioInputListAllowMedia':
            return GetOptions_AudioInputDevice(true, true);
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
        case 'GPIOS':
            return GetOptions_GPIOS(false);
        case 'GPIOLIST':
            return GetOptions_GPIOS(true);
        case 'AES67Interface':
            return GetOptions_AES67Interface();

    }

    return json("{}");
}
