<?

function SetTimeZone($timezone)
{
    if (file_exists("/.dockerenv")) {
        exec("sudo ln -s -f /usr/share/zoneinfo/$timezone /etc/localtime");
        exec("sudo bash -c \"echo $timezone > /etc/timezone\"");
        exec("sudo dpkg-reconfigure -f noninteractive tzdata");
    } else if (file_exists('/usr/bin/timedatectl')) {
        exec("sudo timedatectl set-timezone $timezone");
    } else {
        exec("sudo bash -c \"echo $timezone > /etc/timezone\"");
        exec("sudo dpkg-reconfigure -f noninteractive tzdata");
    }
}

function SetHWClock()
{
    global $settings;

    $rtcDevice = "/dev/rtc0";
    if ($settings['Platform'] == "BeagleBone Black") {
        if (file_exists("/sys/class/rtc/rtc0/name")) {
            $rtcname = file_get_contents("/sys/class/rtc/rtc0/name");
            if (strpos($rtcname, "omap_rtc") !== false) {
                $rtcDevice = "/dev/rtc1";
            }
        }
    }
    exec("sudo hwclock -w -f $rtcDevice");
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
    global $fppDir;

    exec("sudo $fppDir/scripts/piRTC set");
}

function RestartNTPD()
{
    exec("sudo service ntp restart");
}

function SetNTPServer($value)
{
    if ($value != '') {
        exec("sudo sed -i '/^server.*/d' /etc/ntp.conf ; sudo sed -i '/^pool.*/d' /etc/ntp.conf ; sudo sed -i '\$s/\$/\\nserver $value iburst/' /etc/ntp.conf");
    } else {
        exec("sudo sed -i '/^server.*/d' /etc/ntp.conf ; sudo sed -i '/^pool.*/d' /etc/ntp.conf ; sudo sed -i '\$s/\$/\\npool 0.debian.pool.ntp.org iburst\\npool 1.debian.pool.ntp.org iburst\\npool 2.debian.pool.ntp.org iburst\\npool 3.debian.pool.ntp.org iburst\\n/' /etc/ntp.conf");
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

    if (strpos(file_get_contents("/boot/config.txt"), "hdmi_force_hotplug:1") == false) {
        exec("sudo sed -i -e 's/hdmi_force_hotplug=\(.*\)$/hdmi_force_hotplug=\\1\\nhdmi_force_hotplug:1=1/' /boot/config.txt", $output, $return_val);
        exec("sudo sed -i -e 's/^hdmi_force_hotplug:1/#hdmi_force_hotplug:1/' /boot/config.txt", $output, $return_val);
    }
    if (strpos(file_get_contents("/boot/config.txt"), "hdmi_force_hotplug:1=1") == false) {
        exec("sudo sed -i -e 's/hdmi_force_hotplug:1=0/hdmi_force_hotplug:1=1/' /boot/config.txt", $output, $return_val);
    }
    if ($value == '1') {
        exec("sudo sed -i -e 's/#hdmi_force_hotplug/hdmi_force_hotplug/' /boot/config.txt", $output, $return_val);
        exec("sudo sed -i -e 's/#hdmi_force_hotplug/hdmi_force_hotplug/' /boot/config.txt", $output, $return_val);
    } else {
        exec("sudo sed -i -e 's/^hdmi_force_hotplug/#hdmi_force_hotplug/' /boot/config.txt", $output, $return_val);
    }
}
function SetEnableBBBHDMI($value)
{
    if ($value == '1') {
        exec("sudo sed -i -e 's/^disable_uboot_overlay_video/#disable_uboot_overlay_video/' /boot/uEnv.txt", $output, $return_val);
    } else {
        exec("sudo sed -i -e 's/#disable_uboot_overlay_video/disable_uboot_overlay_video/' /boot/uEnv.txt", $output, $return_val);
    }
}
function SetForceHDMIResolution($value, $postfix)
{
    global $settings;

    if ($settings["Platform"] == "BeagleBone Black") {
        exec("sudo sed -i -e 's/^cmdline\(.*\) video=HDMI-A-1:\([A-Za-z@0-9]*\)\(.*\)/cmdline\\1 \\3/' /boot/uEnv.txt", $output, $return_val);
        exec("sudo sed -i 's/[ \\t]*$//' /boot/uEnv.txt", $output, $return_val);
        if ($value != 'Default') {
            exec("sudo sed -i -e 's/^cmdline\(.*\)/cmdline\\1 video=HDMI-A-1:" . $value . "/' /boot/uEnv.txt", $output, $return_val);
        }
    } else {
        $parts = explode(":", $value);
        $numParts = count($parts);
        if (strpos(file_get_contents("/boot/config.txt"), "hdmi_group:1") == false) {
            exec("sudo sed -i -e 's/hdmi_group=\(.*\)$/hdmi_group=\\1\\nhdmi_group:1=0/' /boot/config.txt", $output, $return_val);
            exec("sudo sed -i -e 's/hdmi_mode=\(.*\)$/hdmi_mode=\\1\\nhdmi_mode:1=0/' /boot/config.txt", $output, $return_val);
            exec("sudo sed -i -e 's/^hdmi_cvt=/#hdmi_cvt=/' /boot/config.txt", $output, $return_val);
            exec("sudo sed -i -e 's/^hdmi_group:1/#hdmi_group:1/' /boot/config.txt", $output, $return_val);
            exec("sudo sed -i -e 's/^hdmi_mode:1/#hdmi_mode:1/' /boot/config.txt", $output, $return_val);
            exec("sudo sed -i -e 's/^hdmi_cvt:1/#hdmi_cvt:1/' /boot/config.txt", $output, $return_val);
        }
        if ($parts[0] == '0') {
            exec("sudo sed -i -e 's/^hdmi_group" . $postfix . "=/#hdmi_group" . $postfix . "=/' /boot/config.txt", $output, $return_val);
            exec("sudo sed -i -e 's/^hdmi_mode" . $postfix . "=/#hdmi_mode" . $postfix . "=/' /boot/config.txt", $output, $return_val);
        } else {
            exec("sudo sed -i -e 's/^#hdmi_group" . $postfix . "=/hdmi_group" . $postfix . "=/' /boot/config.txt", $output, $return_val);
            exec("sudo sed -i -e 's/^#hdmi_mode" . $postfix . "=/hdmi_mode" . $postfix . "=/' /boot/config.txt", $output, $return_val);
            exec("sudo sed -i -e 's/^hdmi_group" . $postfix . "=.*/hdmi_group" . $postfix . "=" . $parts[0] . "/' /boot/config.txt", $output, $return_val);
            exec("sudo sed -i -e 's/^hdmi_mode" . $postfix . "=.*/hdmi_mode" . $postfix . "=" . $parts[1] . "/' /boot/config.txt", $output, $return_val);
            if ($numParts == 3) {
                if (strpos(file_get_contents("/boot/config.txt"), "hdmi_cvt". $postfix) == false) {
                    error_log("adding cvt");
                    exec("sudo sed -i -e 's/^hdmi_mode" . $postfix . "/hdmi_cvt" . $postfix . "=\\nhdmi_mode/' /boot/config.txt", $output, $return_val);
                }    
                exec("sudo sed -i -e 's/^#hdmi_cvt" . $postfix . "=/hdmi_cvt" . $postfix . "=/' /boot/config.txt", $output, $return_val);
                exec("sudo sed -i -e 's/^hdmi_cvt" . $postfix . "=.*/hdmi_cvt" . $postfix . "=" . $parts[2] . "/' /boot/config.txt", $output, $return_val);    
            } else {
                exec("sudo sed -i -e 's/^hdmi_cvt" . $postfix . "=/#hdmi_cvt" . $postfix . "=/' /boot/config.txt", $output, $return_val);    
            }
        }    
    }
}

function SetWifiDrivers($value)
{
    global $settings;
    if ($value == "Kernel") {
        exec("sudo rm -f /etc/modprobe.d/blacklist-native-wifi.conf", $output, $return_val);
        exec("sudo rm -f /etc/modprobe.d/rtl8723bu-blacklist.conf", $output, $return_val);
        exec("sudo rm -f /etc/modprobe.d/50-8188eu.conf", $output, $return_val);
    } else {
        exec("sudo cp " . $settings["fppDir"] . "/etc/blacklist-native-wifi.conf /etc/modprobe.d", $output, $return_val);
        exec("sudo rm -f /etc/modprobe.d/blacklist-8192cu.conf", $output, $return_val);
        exec("sudo rm -f /etc/modprobe.d/50-8188eu.conf", $output, $return_val);
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
                if ($value == '0') {
                    exec( "sudo systemctl " . $now . " disable $service");
                } else if ($value == '1') {
                    exec( "sudo systemctl " . $now . " enable $service");
                }
            }
        }
    }
}
function SetGPIOFanProperties() {
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
    $contents = file_get_contents("/boot/config.txt");
    if (strpos($contents, "dtoverlay=gpio-fan") === false) {
        $contents = $contents . "\n";
        exec("echo \"\" | sudo tee -a /boot/config.txt");
        exec("echo \"[all]\" | sudo tee -a /boot/config.txt");
        exec("echo \"" . $pfx . "dtoverlay=gpio-fan,gpiopin=14,temp=" . $fanTemp . "\" | sudo tee -a /boot/config.txt");
        exec("echo \"\" | sudo tee -a /boot/config.txt");
    } else {
        exec("sudo sed -i -e 's/^dtoverlay=gpio-fan\(.*\)$/" . $pfx . "dtoverlay=gpio-fan,gpiopin=14,temp=" . $fanTemp . "/g' /boot/config.txt", $output, $return_val);
        exec("sudo sed -i -e 's/^#dtoverlay=gpio-fan\(.*\)$/" . $pfx . "dtoverlay=gpio-fan,gpiopin=14,temp=" . $fanTemp . "/g' /boot/config.txt", $output, $return_val);
    }
}

function ApplySetting($setting, $value)
{
    global $settings;
    switch ($setting) {
        case 'ClockDate':SetDate($value);
            break;
        case 'ClockTime':SetTime($value);
            break;
        case 'ntpServer':SetNTPServer($value);
            break;
        case 'passwordEnable':EnableUIPassword($value);
            break;
        case 'password':SetUIPassword($value);
            break;
        case 'osPasswordEnable':EnableOSPassword($value);
            break;
        case 'osPassword':SetOSPassword($value);
            break;
        case 'piRTC':SetRTC($value);
            break;
        case 'TimeZone':SetTimeZone($value);
            break;
        case 'ForceHDMI':SetForceHDMI($value);
            break;
        case 'EnableBBBHDMI':SetEnableBBBHDMI($value);
            break;
        case 'ForceHDMIResolution':SetForceHDMIResolution($value, "");
            break;
        case 'ForceHDMIResolutionPort2':SetForceHDMIResolution($value, ":1");
            break;
        case 'wifiDrivers':SetWifiDrivers($value);
            break;
        case 'InstalledCape':SetInstalledCape($value);
            break;
        case 'GPIOFanTemperature':
        case 'GPIOFan':
            SetGPIOFanProperties();
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
