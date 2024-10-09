<?

function SetTimeZone($timezone)
{
    if (file_exists("/.dockerenv")) {
        exec("sudo ln -s -f /usr/share/zoneinfo/$timezone /etc/localtime");
        exec("sudo bash -c \"echo $timezone > /etc/timezone\"");
    } else if (file_exists('/usr/bin/timedatectl')) {
        exec("sudo timedatectl set-timezone $timezone");
    } else {
        exec("sudo bash -c \"echo $timezone > /etc/timezone\"");
    }
    exec("sudo dpkg-reconfigure -f noninteractive tzdata");
}

function SetHWClock()
{
    global $fppDir;
    exec("sudo $fppDir/src/fpprtc set");
}

function SetDate($date)
{
    // Need to pass in the current time or it gets reset to 00:00:00
    exec("sudo date +\"%Y-%m-%d %H:%M:%S\" -s \"$date \$(date +%H:%M:%S)\"");
    SetHWClock();
}

function SetTime($time)
{
    exec("sudo date +%k:%M:%S -s \"$time\"");
    SetHWClock();
}

function SetRTC($rtc)
{
    SetHWClock();
}

function RestartNTPD()
{
    exec("sudo service ntp restart");
}

function SetNTPServer($value)
{
    $ntpConfigFile = '/etc/ntpsec/ntp.conf';
    if ($value != '') {
        // Remove existing server and pool lines, then add the new server line
        exec("sudo sed -i '/^server.*/d' $ntpConfigFile ; sudo sed -i '/^pool.*/d' $ntpConfigFile ; sudo sed -i '\$a\\server $value iburst' $ntpConfigFile");
        exec("sudo sed -i -e 's/minsane 3/minsane 1/' $ntpConfigFile");
    } else {
        // Remove existing server and pool lines, then add the custom pool line
        exec("sudo sed -i '/^server.*/d' $ntpConfigFile ; sudo sed -i '/^pool.*/d' $ntpConfigFile ; sudo sed -i '\$a\\pool falconplayer.pool.ntp.org iburst minpoll 8 maxpoll 12 prefer' $ntpConfigFile");
        exec("sudo sed -i -e 's/minsane 1/minsane 3/' $ntpConfigFile");
    }

    // Note: Assume NTP is always enabled now.
    RestartNTPD();
}

function SetupHtaccess($enablePW)
{
    global $settings;
    $filename = $settings['mediaDirectory'] . "/config/.htaccess";

    if (file_exists($filename)) {
        unlink($filename);
    }

    $data = "";

    if ($enablePW) {
        $data .= "AuthUserFile " . $settings['mediaDirectory'] . "/config/.htpasswd\nAuthType Basic\nAuthName \"Falcon Player\"\n";
        $data .= "<RequireAny>\n  <RequireAll>\n    Require local\n  </RequireAll>\n  <RequireAll>\n    Require valid-user\n  </RequireAll>\n</RequireAny>\n";
    } else {
        $data .= "Allow from All\nSatisfy Any\n";
    }

    // can use env vars in child .htaccess using mod_setenvif
    $data .= "SetEnvIf Host ^ LOCAL_PROTECT=" . $enablePW . "\n";

    // Don't block Fav icon
    if ($enablePW) {
        $data .= "<FilesMatch \"^(favicon)\.ico$\">\nAllow from All\nSatisfy Any\n</FilesMatch>\n";
    }

    file_put_contents($filename, $data, LOCK_EX);
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

function SetUIPassword($value)
{
    global $settings;

    if ($value == '') {
        $value = 'falcon';
    }

    // Write a new password file, replacing odl one if exists.
    // users fpp and admin
    // BCRYPT requires apache 2.4+
    $encrypted_password = password_hash($value, PASSWORD_BCRYPT);
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

    system("echo 'fpp:$password' | sudo chpasswd");
}

function SetOSPassword($value)
{
    if ($value == '') {
        $value = 'falcon';
    }

    system("echo 'fpp:$value' | sudo chpasswd");
}

function SetForceHDMI($value)
{
    global $settings;
    if ($settings["Platform"] == "Raspberry Pi") {
        if ($value == 0) {
            $cmd = "sudo sed -i -e 's/^c\(.*\)vc4.force_hotplug=[x0-9]*\(.*\)/c\\1 \\2/' " . GetDirSetting('boot') . "/cmdline.txt";
        } else {
            $cmd = "sudo sed -i -e 's/^c\(.*\) */c\\1 vc4.force_hotplug=0x03 /' " . GetDirSetting('boot') . "/cmdline.txt";
        }
        exec($cmd, $output, $return_val);
        exec("sudo sed -i -e 's/  */ /g' " . GetDirSetting('boot') . "/cmdline.txt", $output, $return_val);
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

    if ($settings["Platform"] == "BeagleBone Black") {
        exec("sudo sed -i -e 's/^cmdline\(.*\) video=HDMI-A-1:\([A-Za-z@0-9]*\)\(.*\)/cmdline\\1 \\3/' " . GetDirSetting('boot') . "/uEnv.txt", $output, $return_val);
        exec("sudo sed -i 's/[ \\t]*$//' " . GetDirSetting('boot') . "/uEnv.txt", $output, $return_val);
        if ($value != 'Default') {
            exec("sudo sed -i -e 's/^cmdline\(.*\)/cmdline\\1 video=HDMI-A-1:" . $value . "/' " . GetDirSetting('boot') . "/uEnv.txt", $output, $return_val);
        }
    } else {
        $parts = explode("@", $value);
        $numParts = count($parts);
        file_put_contents("/home/fpp/foo.txt", $postfix . " " . $value . " " . $numParts);
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

function SetWifiDrivers($value)
{
    global $settings;
    exec("sudo rm -f /etc/modules-load.d/fpp-network.conf", $output, $return_val);
    exec("sudo rm -f /etc/modprobe.d/50-8188eu.conf", $output, $return_val);
    if ($value == "Kernel") {
        exec("sudo rm -f /etc/modprobe.d/blacklist-native-wifi.conf", $output, $return_val);
        exec("sudo rm -f /etc/modprobe.d/rtl8723bu-blacklist.conf", $output, $return_val);
        if (file_exists("/etc/fpp/wifi/blacklist-external-wifi.conf")) {
            exec("sudo cp /etc/fpp/wifi/blacklist-external-wifi.conf /etc/modprobe.d", $output, $return_val);
        }
    } else {
        exec("sudo rm -f /etc/modprobe.d/blacklist-external-wifi.conf", $output, $return_val);
        exec("sudo rm -f /etc/modprobe.d/blacklist-8192cu.conf", $output, $return_val);
        if (file_exists("/etc/fpp/wifi/blacklist-native-wifi.conf")) {
            exec("sudo cp /etc/fpp/wifi/blacklist-native-wifi.conf /etc/modprobe.d", $output, $return_val);
        } else if (file_exists("/etc/modprobe.d/wifi-disable-power-management.conf")) {
            exec("sudo cp " . $settings["fppDir"] . "/etc/blacklist-native-wifi.conf /etc/modprobe.d", $output, $return_val);
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
        exec("sudo cp -f " . $directory . $value . "-eeprom.bin " . $settings['configDirectory'] . "/cape-eeprom.bin", $output, $return_val);
        exec("sudo chown fpp:fpp " . $settings['configDirectory'] . "/cape-eeprom.bin", $output, $return_val);
    }
    exec("sudo " . $settings['fppBinDir'] . "/fppcapedetect", $output, $return_val);
}
/////////////////////////////////////////////////////////////////////////////
function ApplyServiceSetting($setting, $value, $now)
{
    global $settings;
    if ($settings["Platform"] != "MacOS") {
        if (preg_match('/^Service_(rsync|smbd_nmbd|vsftpd)$/', $setting)) {
            $services = preg_split('/_/', preg_replace("/^Service_/", "", $setting));
            foreach ($services as $service) {
                //enabling/disabling a service that is already in that state is very slow
                //so query the current state and only set it if needed
                $isEnabled = trim(shell_exec("sudo systemctl is-enabled $service"));
                if ($value == '0') {
                    if ($isEnabled != "disabled") {
                        exec("sudo systemctl " . $now . " disable $service >/dev/null &");
                    }
                } else if ($value == '1') {
                    if ($isEnabled != "enabled") {
                        exec("sudo systemctl " . $now . " enable $service >/dev/null &");
                    }
                }
            }
        }
    }
}
function SetGPIOFanProperties()
{
    global $settings;
    $fanOn = ReadSettingFromFile('GPIOFan');
    $fanTemp = ReadSettingFromFile('GPIOFanTemperature') . "000";
    $pfx = "";

    if ($fanTemp == '000') {
        $fanTemp = '70000';
    }
    if ($fanOn == '0') {
        $pfx = "#";
    }
    $contents = file_get_contents(GetDirSetting('boot') . "/config.txt");
    if (strpos($contents, "dtoverlay=gpio-fan") === false) {
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

function SetupScreenBlanking($value)
{
    global $settings;
    $timeout = ReadSettingFromFile('screensaverTimeout');
    if (!$timeout || $timeout == '') {
        $timeout = "1";
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
        case 'wifiDrivers':
            SetWifiDrivers($value);
            break;
        case 'InstalledCape':
            SetInstalledCape($value);
            break;
        case 'GPIOFanTemperature':
        case 'GPIOFan':
            SetGPIOFanProperties();
            break;
        case 'screensaver':
            SetupScreenBlanking($value);
            break;
        default:
            ApplyServiceSetting($setting, $value, "--now");
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
        $status = exec($SUDO . " amixer -c $card set '$mixerDevice' -- " . $vol . "%");
    }
}
