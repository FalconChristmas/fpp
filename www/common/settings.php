<?

function SetTimeZone($timezone)
{
    // The value goes into root shell commands, one of them inside an outer
    // "..." where escapeshellarg()'s single quotes do not protect it.  A known
    // zone name has no shell metacharacters, so accept nothing else.
    if (!IsValidTimeZone($timezone)) {
        error_log("SetTimeZone: ignoring unknown time zone");
        return;
    }
    $timezone = escapeshellarg($timezone);
    if (file_exists("/etc/fpp/container")) {
        exec("sudo ln -s -f /usr/share/zoneinfo/$timezone /etc/localtime");
        exec("sudo bash -c \"echo $timezone > /etc/timezone\"");
        // We wrote /etc/timezone by hand; sync tzdata's caches/symlink.
        exec("sudo dpkg-reconfigure -f noninteractive tzdata");
    } else if (file_exists('/usr/bin/timedatectl')) {
        // timedatectl set-timezone already updates /etc/localtime and the system
        // timezone, so the (slow, ~5s on a BBB) dpkg-reconfigure tzdata step that
        // used to run here is redundant and just made saving the setting hang.
        exec("sudo timedatectl set-timezone $timezone");
    } else {
        exec("sudo bash -c \"echo $timezone > /etc/timezone\"");
        // No timedatectl available; dpkg-reconfigure applies /etc/timezone.
        exec("sudo dpkg-reconfigure -f noninteractive tzdata");
    }
}

function SetHWClock()
{
    global $fppDir;
    exec("sudo $fppDir/src/fpprtc set");
}

function SetDate($date)
{
    // Strip any characters that are not part of a date so the value cannot
    // be used as a shell metacharacter when interpolated below.
    $date = preg_replace('/[^0-9\/\-\sA-Za-z:]/', '', $date);
    // Need to pass in the current time or it gets reset to 00:00:00
    exec("sudo date +\"%Y-%m-%d %H:%M:%S\" -s \"$date \$(date +%H:%M:%S)\"");
    SetHWClock();
}

function SetTime($time)
{
    // Strip everything that is not part of an HH:MM:SS time string.
    $time = preg_replace('/[^0-9:]/', '', $time);
    exec("sudo date +%k:%M:%S -s \"$time\"");
    SetHWClock();
}

function SetRTC($rtc)
{
    SetHWClock();
}

function RestartNTPD()
{
    exec("sudo systemctl restart chrony");
}

function SetNTPServer($value)
{
    // FPP uses chrony as its NTP daemon. Only the time-source line (pool/server)
    // is managed here; the server-mode directives (allow/local/etc) written at
    // install time are left in place so this controller keeps serving time to
    // any other Pis pointed at it. chrony's default minsources is 1, so pointing
    // at a single server (e.g. another FPP) syncs without extra tuning.
    $ntpConfigFile = '/etc/chrony/chrony.conf';
    // Keep only hostname/IP characters so the value can't break out of the
    // sed expression below.
    $value = preg_replace('/[^A-Za-z0-9.\-]/', '', $value);
    // Remove existing pool and server source lines.
    exec("sudo sed -i '/^pool .*/d' $ntpConfigFile ; sudo sed -i '/^server .*/d' $ntpConfigFile");
    if ($value != '') {
        // Sync from the explicitly configured server (often another FPP/RTC Pi).
        exec("sudo sed -i '\$a\\server $value iburst' $ntpConfigFile");
    } else {
        // Fall back to FPP's public pool.
        exec("sudo sed -i '\$a\\pool falconplayer.pool.ntp.org iburst minpoll 8 maxpoll 12' $ntpConfigFile");
    }

    // Add default gateway as a fallback NTP server reachable on the local
    // subnet (no NAT boundary). Safe even if the gateway does not run NTP;
    // chrony simply marks it as unreachable.
    $gateway = exec("ip route show default 2>/dev/null | awk '{print \$3}'");
    if (!empty($gateway) && filter_var($gateway, FILTER_VALIDATE_IP, FILTER_FLAG_IPV4)) {
        $gwInConf = exec("grep -c 'server $gateway' $ntpConfigFile 2>/dev/null");
        if ($gwInConf == 0) {
            exec("sudo sh -c \"echo >> $ntpConfigFile\"");
            exec("sudo sh -c \"echo '# Auto-detected default gateway fallback' >> $ntpConfigFile\"");
            exec("sudo sh -c \"echo server $gateway minpoll 10 maxpoll 12 iburst >> $ntpConfigFile\"");
        }
    }
    // Note: Assume NTP is always enabled now.
    RestartNTPD();
}

function SetupHtaccess($enablePW)
{
    global $settings;
    $filename = $settings['mediaDirectory'] . "/config/ui-password-config.conf";

    if (!file_exists($filename)) {
        // Use sudo to create the file and set ownership to fpp:fpp
        exec("sudo touch " . escapeshellarg($filename));
        exec("sudo chown fpp:fpp " . escapeshellarg($filename));
    }

    $data = "";

    if ($enablePW) {
        $data .= "AuthUserFile " . $settings['mediaDirectory'] . "/config/.htpasswd\nAuthType Basic\nAuthName \"Falcon Player\"\n";
        // "Require local" also matches a request whose client address is the
        // server's own, which is what /proxy/<this player's IP>/ produces:
        // without the X-FPP-Proxied test (the header /proxy adds), anyone on
        // the LAN skipped the password that way.
        $data .= "<RequireAny>\n  <RequireAll>\n    Require local\n    Require expr \"%{HTTP:X-FPP-Proxied} == ''\"\n  </RequireAll>\n  <RequireAll>\n    Require valid-user\n  </RequireAll>\n</RequireAny>\n\n";
    } else {
        $data .= "<RequireAny>\n  Require all granted\n</RequireAny>\n\n";
    }

    // can use env vars in child .htaccess using mod_setenvif
    $data .= "SetEnvIf Host ^ LOCAL_PROTECT=" . $enablePW . "\n\n";

    // Don't block Fav icon
    if ($enablePW) {
        $data .= "<FilesMatch \"^(favicon)\.ico$\">\nAllow from All\nSatisfy Any\n</FilesMatch>\n";
    }

    file_put_contents($filename, $data, LOCK_EX);

    //reload apache config.  exec(), not system(): this runs inside API calls
    //and backup restores, whose JSON reply the service's "Reloading ..." line
    //was being written into.
    exec("sudo service apache2 reload 2>&1", $output);
}

function EnableUIPassword($value)
{
    global $settings;

    if ($value == '0') {
        SetupHtaccess(0);
    } else if ($value == '1') {
        $password = ReadSettingFromFile('password');

        SetUIPassword($password);
        SetupHtaccess(1);
    }
}

/**
 * The htpasswd hash for the UI password.
 *
 * Apache mod_auth_basic re-verifies the hash on EVERY request, with no
 * caching, and a page fires dozens of requests.  bcrypt at PHP's default cost
 * (12) takes ~1.4s per verify on a BBB, which stacked up to ~10s per page.
 *
 * - One core (BBB): unsalted SHA-1 ({SHA}), which verifies in microseconds.
 *   Weak against a stolen .htpasswd, but every request queues on that core.
 * - More than one core: bcrypt, at the highest cost this board verifies in
 *   about 10ms (cost 6 on a Pi 5, 5 on a PocketBeagle2 when measured), so a
 *   page's requests spread over the cores and the delay stays small.  A board
 *   too slow even for cost 5 keeps {SHA}.
 *
 * The cost is chosen when the password is set, so moving a card to faster
 * hardware keeps the old cost until the password is saved again.
 */
function UIPasswordHash($value)
{
    $cores = intval(trim((string) @shell_exec('nproc 2>/dev/null')));
    if ($cores > 1) {
        // Time a cost-5 verify (best of three; each step up doubles it).
        $probe = password_hash('calibrate', PASSWORD_BCRYPT, array('cost' => 5));
        $best = PHP_FLOAT_MAX;
        for ($i = 0; $i < 3; $i++) {
            $t = microtime(true);
            password_verify('calibrate', $probe);
            $best = min($best, (microtime(true) - $t) * 1000.0);
        }
        if ($best <= 25.0) {
            $cost = 5 + (int) floor(log(10.0 / max($best, 0.01), 2));
            $cost = max(5, min(10, $cost));
            return password_hash($value, PASSWORD_BCRYPT, array('cost' => $cost));
        }
    }
    return "{SHA}" . base64_encode(sha1($value, true));
}

function SetUIPassword($value)
{
    global $settings;

    if ($value == '') {
        $value = 'falcon';
    }

    // Write a new password file, replacing old one if exists; users fpp and admin.
    $encrypted_password = UIPasswordHash($value);
    $data = "admin:$encrypted_password\nfpp:$encrypted_password\n";
    $filename = $settings['mediaDirectory'] . "/config/.htpasswd";

    // Old file may have been ownedby root so file_put_contents will fail.
    if (file_exists($filename)) {
        unlink($filename);
    }

    file_put_contents($filename, $data, LOCK_EX);
}

function EnableOSPassword($value)
{
    global $settings;

    $password = '';

    if ($value == '1') {
        $password = ReadSettingFromFile('osPassword');
    }

    if ($password == '') {
        $password = 'falcon';
    }

    // Escape the whole 'user:password' line so it is passed to chpasswd via
    // stdin without any shell interpretation.
    system("echo " . escapeshellarg('fpp:' . $password) . " | sudo chpasswd");
}

function SetOSPassword($value)
{
    if ($value == '') {
        $value = 'falcon';
    }

    system("echo " . escapeshellarg('fpp:' . $value) . " | sudo chpasswd");
}

// cmdline.txt's text with every vc4.force_hotplug= option removed, and one
// added back when $force.
function CmdlineWithForceHotplug($cmdline, $force)
{
    $options = array_values(array_filter(
        preg_split('/\s+/', trim($cmdline), -1, PREG_SPLIT_NO_EMPTY),
        function ($o) {
            return strpos($o, 'vc4.force_hotplug=') !== 0;
        }
    ));
    if ($force) {
        $options[] = 'vc4.force_hotplug=0x03';
    }
    return implode(' ', $options) . "\n";
}

function SetForceHDMI($value)
{
    global $settings;
    if ($settings["Platform"] == "Raspberry Pi") {
        // cmdline.txt is one line of space-separated options.  Every
        // vc4.force_hotplug= option is dropped and, when forcing HDMI, exactly
        // one added back: ticking the box used to append another copy each
        // time, and unticking removed only the last one.
        $file = GetDirSetting('boot') . "/cmdline.txt";
        $current = @file_get_contents($file);
        if ($current === false || trim($current) === '') {
            return;
        }
        $updated = CmdlineWithForceHotplug($current, $value != 0);
        if ($updated !== $current) {
            exec("printf %s " . escapeshellarg($updated) . " | sudo tee " . escapeshellarg($file) . " > /dev/null", $output, $return_val);
        }
    }
}
function SetEnableBBBHDMI($value)
{
    if ($value == '1') {
        exec("sudo sed -i -e 's/^disable_uboot_overlay_video/#disable_uboot_overlay_video/' " . GetDirSetting('boot') . "/uEnv.txt", $output, $return_val);
    } else {
        exec("sudo sed -i -e 's/#disable_uboot_overlay_video/disable_uboot_overlay_video/' " . GetDirSetting('boot') . "/uEnv.txt", $output, $return_val);
    }
}
function SetForceHDMIResolution($value, $postfix)
{
    global $settings;

    // Spliced into a single-quoted sed expression run as root; a mode is only
    // ever letters, digits and '@' (e.g. 1920x1080@60, the same set the sed
    // patterns below match when removing it).
    if (!preg_match('/^[A-Za-z0-9@]+$/', $value)) {
        error_log("SetForceHDMIResolution: ignoring invalid mode");
        return;
    }

    if ($settings["Platform"] == "BeagleBone Black") {
        exec("sudo sed -i -e 's/^cmdline\(.*\) video=HDMI-A-1:\([A-Za-z@0-9]*\)\(.*\)/cmdline\\1 \\3/' " . GetDirSetting('boot') . "/uEnv.txt", $output, $return_val);
        exec("sudo sed -i 's/[ \\t]*$//' " . GetDirSetting('boot') . "/uEnv.txt", $output, $return_val);
        if ($value != 'Default') {
            exec("sudo sed -i -e 's/^cmdline\(.*\)/cmdline\\1 video=HDMI-A-1:" . $value . "/' " . GetDirSetting('boot') . "/uEnv.txt", $output, $return_val);
        }
    } else {
        $parts = explode("@", $value);
        $numParts = count($parts);
        if ($numParts == 2 || $value == "Default") {
            exec("sudo sed -i -e 's/^c\(.*\) video=HDMI-A-" . $postfix . ":\([A-Za-z@0-9]*\)\(.*\)/c\\1 \\3/' " . GetDirSetting('boot') . "/cmdline.txt", $output, $return_val);
            exec("sudo sed -i 's/[ \\t]*$//' " . GetDirSetting('boot') . "/cmdline.txt", $output, $return_val);
            if ($value != 'Default') {
                exec("sudo sed -i -e 's/^c\(.*\)/c\\1 video=HDMI-A-" . $postfix . ":" . $value . "/' " . GetDirSetting('boot') . "/cmdline.txt", $output, $return_val);
            }
        } else {
            //  likely old format, not really easy to migrate
        }
    }
}

function SetInstalledCape($value)
{
    global $settings;
    if ($value == "--None--") {
        exec("sudo rm -f " . $settings['configDirectory'] . "/cape-eeprom.bin", $output, $return_val);
    } else {
        $directory = $settings["fppDir"] . "/capes";
        if ($settings['Platform'] == "Raspberry Pi") {
            $directory = $directory . "/pi/";
        } else if (strpos($settings['SubPlatform'], 'PocketBeagle') !== false) {
            $directory = $directory . "/pb/";
        } else {
            $directory = $directory . "/bbb/";
        }
        // Only a cape that actually ships an EEPROM image here (the list
        // GetCapeOptions() offers); the name is a path component in a root cp.
        $eeprom = $directory . $value . "-eeprom.bin";
        if (!preg_match('/^[A-Za-z0-9][A-Za-z0-9._+-]*$/', $value) || !is_file($eeprom)) {
            error_log("SetInstalledCape: no EEPROM image for the selected cape");
            return;
        }
        exec("sudo cp -f " . escapeshellarg($eeprom) . " " . escapeshellarg($settings['configDirectory'] . "/cape-eeprom.bin"), $output, $return_val);
        exec("sudo chown fpp:fpp " . $settings['configDirectory'] . "/cape-eeprom.bin", $output, $return_val);
    }
    exec("sudo " . $settings['fppBinDir'] . "/fppcapedetect", $output, $return_val);
}
/////////////////////////////////////////////////////////////////////////////
function ApplyServiceSetting($setting, $value, $now)
{
    global $settings;
    if ($settings["Platform"] != "MacOS") {
        if (preg_match('/^Service_(rsync|smbd_nmbd|vsftpd)$/', $setting) && ($value == '0' || $value == '1')) {
            // enabling/disabling a service is very slow, so it is done in the
            // background.  The script applies whatever value is saved when it
            // runs, serialized per setting, so an off/on toggle can't race
            // (issue #3107); $value has already been written to the file.
            exec("sudo " . $settings['fppDir'] . "/scripts/apply_service_setting " . $setting . ($now == "--now" ? " --now" : "") . " >/dev/null 2>&1 &");
        }
    }
}
function SetGPIOFanProperties()
{
    global $settings;
    $configFile = GetDirSetting('boot') . "/config.txt";
    if (!file_exists($configFile)) {
        return;
    }
    $fanOn = ReadSettingFromFile('GPIOFan');

    // A cape that declares its own fan hides GPIOFan, and hiding it is the cape
    // saying it owns that hardware -- so FPP must not also write its overlay
    // line, whatever value happens to be stored.
    //
    // The cape's removeSettings cannot be relied on to have cleared it first.
    // That check is settingIsSet(), which only asks whether the key is present,
    // so a value an EARLIER eeprom's defaultSettings wrote looks exactly like one
    // the user chose deliberately, and the removal is refused to protect a choice
    // nobody made.  Every board that ran the previous eeprom therefore still has
    // GPIOFan set, which is exactly the population that must not end up with the
    // cape's fan and this one both declaring a cooling device on one pin.
    $capeOwnsFan = isset($settings["cape-info"]["hide"]["settings"]) &&
        in_array("GPIOFan", $settings["cape-info"]["hide"]["settings"]);

    // Absent is not the same as "0".  "0" means a fan is fitted and switched
    // off, and leaves the line behind commented out.  Absent, or owned by the
    // cape, means no fan here is ours to manage and the line goes with it.  Any
    // line-anchored "dtoverlay=gpio-fan" counts as ours: the rewrite below
    // already claims every one it finds and forces it to gpiopin=14.
    if ($fanOn === false || $capeOwnsFan) {
        exec("sudo sed -i -e '/^#\\?dtoverlay=gpio-fan/d' " . escapeshellarg($configFile));
        return;
    }
    // The overlay wants millidegrees.  Multiply rather than appending "000":
    // the setting is degrees C to one decimal place so that whole degrees F
    // survive the conversion both ways (see PutSetting() in
    // www/api/controllers/settings.php), and "29.4" . "000" is not a number.
    $fanTempSetting = ReadSettingFromFile('GPIOFanTemperature');
    $fanTemp = is_numeric($fanTempSetting) ? (string) ((int) round(floatval($fanTempSetting) * 1000)) : '70000';
    $pfx = "";

    if ($fanOn == '0') {
        $pfx = "#";
    }
    $contents = file_get_contents($configFile);
    // Ours is a line-anchored "dtoverlay=gpio-fan", commented out or not: the
    // only kind the sed rewrite below (and the removal above) can touch.  Any
    // other mention -- an indented copy, a comment -- used to count too, and
    // then no line was written at all.
    if (!preg_match('/^#?dtoverlay=gpio-fan/m', $contents)) {
        $contents = $contents . "\n";
        exec("echo \"\" | sudo tee -a " . GetDirSetting('boot') . "/config.txt");
        exec("echo \"[all]\" | sudo tee -a " . GetDirSetting('boot') . "/config.txt");
        exec("echo \"" . $pfx . "dtoverlay=gpio-fan,gpiopin=14,temp=" . $fanTemp . "\" | sudo tee -a " . GetDirSetting('boot') . "/config.txt");
        exec("echo \"\" | sudo tee -a " . GetDirSetting('boot') . "/config.txt");
    } else {
        exec("sudo sed -i -e 's/^dtoverlay=gpio-fan\(.*\)$/" . $pfx . "dtoverlay=gpio-fan,gpiopin=14,temp=" . $fanTemp . "/g' " . GetDirSetting('boot') . "/config.txt", $output, $return_val);
        exec("sudo sed -i -e 's/^#dtoverlay=gpio-fan\(.*\)$/" . $pfx . "dtoverlay=gpio-fan,gpiopin=14,temp=" . $fanTemp . "/g' " . GetDirSetting('boot') . "/config.txt", $output, $return_val);
    }
}

// Apache sends /api's CORS headers while this file exists (etc/apache2.site);
// fppinit puts it in step with the setting at boot.  No reload needed.
function SetupCrossOriginAPI($value)
{
    global $settings;
    $marker = $settings['mediaDirectory'] . '/config/api-cross-origin';
    if ($value == '1') {
        @file_put_contents($marker, "Written by FPP for the AllowCrossOriginAPI setting\n");
    } else {
        @unlink($marker);
    }
}

function SetupLocalMQTTBroker($value)
{
    global $settings;
    if ($settings["Platform"] != "MacOS") {
        if ($value == '0') {
            //remove local broker config and restart the service
            exec("sudo rm -f /etc/mosquitto/conf.d/fpp_local_broker.conf", $output, $return_val);
            exec("sudo systemctl stop mosquitto >/dev/null &");
            exec("sudo systemctl disable mosquitto >/dev/null &");
        } else if ($value == '1') {
            //generate mosquitto password file
            $password = GetSettingValue('MQTTPassword');
            if ($password == '') {
                $password = 'fpp';
            }
            exec("sudo mosquitto_passwd -b -c /etc/mosquitto/passwd fpp " . escapeshellarg($password), $output, $return_val);
            if ($return_val != 0) {
                error_log("Error creating mosquitto password file: " . implode("\n", $output));
                return;
            }
            exec("sudo chown mosquitto:mosquitto /etc/mosquitto/passwd");
            exec("sudo chmod 640 /etc/mosquitto/passwd");

            //generate local broker config and restart the service
            $config = "listener 1883\n";
            $config .= "protocol mqtt\n";
            $config .= "allow_anonymous false\n";
            $config .= "password_file /etc/mosquitto/passwd";
            $command = "echo " . escapeshellarg($config) . " | sudo tee /etc/mosquitto/conf.d/fpp_local_broker.conf > /dev/null";
            exec($command);
            if (exec("sudo systemctl is-enabled mosquitto") != "enabled") {
                exec("sudo systemctl enable mosquitto >/dev/null &");
            }
            exec("sudo systemctl restart mosquitto >/dev/null &");
        }
    }
}
function SetupScreenBlanking($value)
{
    global $settings;
    // Spliced into a root sed command: minutes, so an integer.
    $timeout = intval(ReadSettingFromFile('screensaverTimeout'));
    if ($timeout <= 0) {
        $timeout = 1;
    }

    if ($settings["Platform"] == "Raspberry Pi") {
        if ($value == 0) {
            $cmd = "sudo sed -i -e 's/^c\(.*\)consoleblank=[0-9]*\(.*\)/c\\1 \\2/' " . GetDirSetting('boot') . "/cmdline.txt";
        } else {
            $cmd = "sudo sed -i -e 's/^c\(.*\) */c\\1 consoleblank=" . $timeout . "/' " . GetDirSetting('boot') . "/cmdline.txt";
        }
        exec($cmd, $output, $return_val);
        exec("sudo sed -i -e 's/  */ /g' " . GetDirSetting('boot') . "/cmdline.txt", $output, $return_val);
    }
}



function ApplySetting($setting, $value)
{
    global $settings;
    switch ($setting) {
        case 'ClockDate':
            SetDate($value);
            break;
        case 'ClockTime':
            SetTime($value);
            break;
        case 'ntpServer':
            SetNTPServer($value);
            break;
        case 'UseNTPFromDHCP':
            // Reconfigure network to apply UseNTP setting
            exec("sudo " . $settings['fppDir'] . "/src/fppinit configureNetwork", $output);
            break;
        case 'passwordEnable':
            EnableUIPassword($value);
            break;
        case 'password':
            SetUIPassword($value);
            break;
        case 'osPasswordEnable':
            EnableOSPassword($value);
            break;
        case 'osPassword':
            SetOSPassword($value);
            break;
        case 'piRTC':
            SetRTC($value);
            break;
        case 'TimeZone':
            SetTimeZone($value);
            break;
        case 'ForceHDMI':
            SetForceHDMI($value);
            break;
        case 'EnableBBBHDMI':
            SetEnableBBBHDMI($value);
            break;
        case 'ForceHDMIResolution':
            SetForceHDMIResolution($value, "1");
            break;
        case 'ForceHDMIResolutionPort2':
            SetForceHDMIResolution($value, "2");
            break;
        case 'InstalledCape':
            SetInstalledCape($value);
            break;
        case 'GPIOFanTemperature':
        case 'GPIOFan':
            SetGPIOFanProperties();
            break;
        case 'DisablePiRTC':
            // fppinit owns the config.txt block (it also reconciles it at boot for
            // capes that ship DisablePiRTC in defaultSettings), so don't duplicate
            // the edit here.  It won't reboot -- the setting is declared
            // "reboot": 1, so the UI is already prompting for that.
            exec("sudo " . $settings['fppDir'] . "/src/fppinit setupPiRTC", $output);
            break;
        case 'DisableHDMICECInit':
            // fppinit owns the config.txt block. The setting is declared
            // "reboot": 1, so the UI already prompts for reboot.
            exec("sudo " . $settings['fppDir'] . "/src/fppinit setupHDMICEC", $output);
            break;
        case 'WifiAntenna':
            // fppinit owns the config.txt block. The setting is declared
            // "reboot": 1, so the UI already prompts for reboot.
            exec("sudo " . $settings['fppDir'] . "/src/fppinit setupWifiAntenna", $output);
            break;
        case 'Pi16KPageKernel':
            // fppinit owns the kernel swap and re-applies it at boot. The setting
            // is declared "reboot": 1, so the UI already prompts for reboot.
            exec("sudo " . $settings['fppDir'] . "/src/fppinit setup16KPageKernel", $output);
            break;
        case 'screensaver':
            SetupScreenBlanking($value);
            break;
        case 'Service_MQTT_localbroker':
            SetupLocalMQTTBroker($value);
            break;
        case 'AllowCrossOriginAPI':
            SetupCrossOriginAPI($value);
            break;
        case 'MQTTPassword':
            if (GetSettingValue('Service_MQTT_localbroker') == '1') {
                // The global $settings array still holds the request-start value;
                // refresh it so SetupLocalMQTTBroker() provisions the broker with
                // the password just saved rather than the previous one.
                $settings['MQTTPassword'] = $value;
                SetupLocalMQTTBroker('1');
            }
            break;
        default:
            if (str_starts_with($setting, 'FanTrip_')) {
                // Dynamic fan thermal trip settings (PrintFanThermalSettings in
                // common.php).  fppinit owns the sysfs writes so the live apply
                // here and the boot-time re-apply share a single implementation.
                exec("sudo " . $settings['fppDir'] . "/src/fppinit applyThermal", $output);
            } else {
                ApplyServiceSetting($setting, $value, "--now");
            }
            break;
    }
}



function setVolume($vol)
{
    global $SUDO;
    global $settings;

    if ($vol < 0) {
        $vol = 0;
    }
    if ($vol > 100) {
        $vol = 100;
    }

    WriteSettingToFile("volume", $vol);

    $status = SendCommand('v,' . $vol . ',');

    if ($settings["Platform"] != "MacOS") {
        // AudioOutput is stored as a stable ALSA card ID; amixer needs the
        // numeric card index, so resolve it (legacy numeric values pass through).
        $cardId = isset($settings['AudioOutput']) ? $settings['AudioOutput'] : '';
        $card = ResolveAlsaCardIdToNumber($cardId);
        if ($card === '') {
            $card = ctype_digit((string) $cardId) ? $cardId : '0';
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

        // PipeWire backend handles volume through fppd, skip direct amixer call.
        // Simple PipeWire mode shares the same runtime pipeline as Advanced.
        if (!IsPipeWireBackend($settings)) {
            // Why do we do this here and in fppd's settings.c
            // Escape the mixer device name so it cannot break out of the
            // single-quoted argument to amixer.
            $status = exec($SUDO . " amixer -c $card set " . escapeshellarg($mixerDevice) . " -- " . $vol . "%");
        }
    }
}
