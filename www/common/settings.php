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

// Although the setting is removed, this is still used by 
// scripts/handle_boot_actions
function SetNTP($value)
{
    if ($value == "1"){
        exec("sudo systemctl enable ntp");
        exec("sudo systemctl start ntp");
    } else if ($value == "0"){
        exec("sudo systemctl stop ntp");
        exec("sudo systemctl disable ntp");
    }
}

function SetNTPServer($value)
{
    if ($value != '') {
        exec("sudo sed -i '/^server.*/d' /etc/ntp.conf ; sudo sed -i '\$s/\$/\\nserver $value iburst/' /etc/ntp.conf");
    } else {
        exec("sudo sed -i '/^server.*/d' /etc/ntp.conf ; sudo sed -i '\$s/\$/\\nserver 0.debian.pool.ntp.org iburst\\nserver 1.debian.pool.ntp.org iburst\\nserver 2.debian.pool.ntp.org iburst\\nserver 3.debian.pool.ntp.org iburst\\n/' /etc/ntp.conf");
    }

    // Note: Assume NTP is always enabled now.
    RestartNTPD();
}

function SetupHtaccess($enablePW)
{
    global $settings;
    $filename = $settings['mediaDirectory'] . "/config/.htaccess";

    if (file_exists($filename))
        unlink($filename);

    $data = "";
    if (PHP_SAPI != 'fpm-fcgi') // If updating this string of PHP options, also update /opt/fpp/www/.user.ini for php-fpm
        $data = "php_value max_input_vars 5000\nphp_value upload_max_filesize 4G\nphp_value post_max_size 4G\n";

    if ($enablePW) {
        $data .= "AuthUserFile " . $settings['mediaDirectory'] . "/config/.htpasswd\nAuthType Basic\nAuthName \"Falcon Player\"\nRequire valid-user\n";
    }

    // can use env vars in child .htaccess using mod_setenvif
    $data .= "SetEnvIf Host ^ LOCAL_PROTECT=" . $enablePW ."\n";

    // Allow open access for fppxml & fppjson
    $data .= "<FilesMatch \"^(fppjson|fppxml)\.php$\">\n";
    if ($enablePW) {
        $data .= "<RequireAny>\n  <RequireAll>\n    Require local\n  </RequireAll>\n  <RequireAll>\n    Require valid-user\n  </RequireAll>\n</RequireAny>\n";
    } else {
        $data .= "Allow from All\nSatisfy Any\n";
    }
    $data .= "</FilesMatch>\n";

    // Don't block Fav icon
    $data .= "<FilesMatch \"^(favicon)\.ico$\">\nAllow from All\nSatisfy Any\n</FilesMatch>\n";

    file_put_contents($filename, $data);
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

    if ($value == '')
        $value = 'falcon';

    // Write a new password file, replacing odl one if exists.
    // users fpp and admin
    // BCRYPT requires apache 2.4+
    $encrypted_password = password_hash($value, PASSWORD_BCRYPT);
    $data = "admin:$encrypted_password\nfpp:$encrypted_password\n";
    $filename =  $settings['mediaDirectory'] . "/config/.htpasswd";

    // Old file may have been ownedby root so file_put_contents will fail.
    if (file_exists($filename)) {
	    unlink($filename);
    }

    file_put_contents($filename, $data);
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
function SetForceHDMIResolution($value, $postfix)
{
    $parts = explode(":", $value);

    if (strpos(file_get_contents("/boot/config.txt"), "hdmi_group:1") == false) {
        exec("sudo sed -i -e 's/hdmi_group=\(.*\)$/hdmi_group=\\1\\nhdmi_group:1=0/' /boot/config.txt", $output, $return_val);
        exec("sudo sed -i -e 's/hdmi_mode=\(.*\)$/hdmi_mode=\\1\\nhdmi_mode:1=0/' /boot/config.txt", $output, $return_val);
        exec("sudo sed -i -e 's/^hdmi_group:1/#hdmi_group:1/' /boot/config.txt", $output, $return_val);
        exec("sudo sed -i -e 's/^hdmi_mode:1/#hdmi_mode:1/' /boot/config.txt", $output, $return_val);
    }

    if ($parts[0] == '0') {
        exec("sudo sed -i -e 's/^hdmi_group".$postfix."=/#hdmi_group".$postfix."=/' /boot/config.txt", $output, $return_val);
        exec("sudo sed -i -e 's/^hdmi_mode".$postfix."=/#hdmi_mode".$postfix."=/' /boot/config.txt", $output, $return_val);
    } else {
        exec("sudo sed -i -e 's/^#hdmi_group".$postfix."=/hdmi_group".$postfix."=/' /boot/config.txt", $output, $return_val);
        exec("sudo sed -i -e 's/^#hdmi_mode".$postfix."=/hdmi_mode".$postfix."=/' /boot/config.txt", $output, $return_val);
        exec("sudo sed -i -e 's/^hdmi_group".$postfix."=.*/hdmi_group".$postfix."=".$parts[0]."/' /boot/config.txt", $output, $return_val);
        exec("sudo sed -i -e 's/^hdmi_mode".$postfix."=.*/hdmi_mode".$postfix."=".$parts[1]."/' /boot/config.txt", $output, $return_val);
    }
}

function SetWifiDrivers($value) {
    if ($value == "Kernel") {
        exec("sudo rm -f /etc/modprobe.d/blacklist-native-wifi.conf", $output, $return_val );
        exec("sudo rm -f /etc/modprobe.d/rtl8723bu-blacklist.conf", $output, $return_val );
        exec("sudo rm -f /etc/modprobe.d/50-8188eu.conf", $output, $return_val);
    } else {
        exec("sudo cp /opt/fpp/etc/blacklist-native-wifi.conf /etc/modprobe.d", $output, $return_val );
        exec("sudo rm -f /etc/modprobe.d/blacklist-8192cu.conf", $output, $return_val );
        exec("sudo rm -f /etc/modprobe.d/50-8188eu.conf", $output, $return_val);
    }
}

/////////////////////////////////////////////////////////////////////////////
function ApplySetting($setting, $value) {
    switch ($setting) {
        case 'ClockDate':       SetDate($value);              break;
        case 'ClockTime':       SetTime($value);              break;
        case 'ntp':             SetNTP($value);               break; // Still used by handle_boot_actions
        case 'ntpServer':       SetNTPServer($value);         break;
        case 'passwordEnable':  EnableUIPassword($value);     break;
        case 'password':        SetUIPassword($value);        break;
        case 'piRTC':           SetRTC($value);               break;
        case 'TimeZone':        SetTimeZone($value);          break;
        case 'ForceHDMI':       SetForceHDMI($value);         break;
        case 'ForceHDMIResolution':  SetForceHDMIResolution($value, "");         break;
        case 'ForceHDMIResolutionPort2':  SetForceHDMIResolution($value, ":1");         break;
        case 'wifiDrivers':     SetWifiDrivers($value);       break;
    }
}

function setVolume($vol) {
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

}

?>
