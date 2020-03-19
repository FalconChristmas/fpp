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
    if ($settings['Platform'] == "BeagleBone Black")
        $rtcDevice = "/dev/rtc1";

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
    $ntp = ReadSettingFromFile('ntp');

    if ($value != '') {
        exec("sudo sed -i '/^server.*/d' /etc/ntp.conf ; sudo sed -i '\$s/\$/\\nserver $value iburst/' /etc/ntp.conf");
    } else {
        exec("sudo sed -i '/^server.*/d' /etc/ntp.conf ; sudo sed -i '\$s/\$/\\nserver 0.debian.pool.ntp.org iburst\\nserver 1.debian.pool.ntp.org iburst\\nserver 2.debian.pool.ntp.org iburst\\nserver 3.debian.pool.ntp.org iburst\\n/' /etc/ntp.conf");
    }

    if ($ntp == "1")
        RestartNTPD();
}

function SetupHtaccess($enablePW)
{
    global $settings;
    $filename = $settings['mediaDirectory'] . "/config/.htaccess";

    if (file_exists($filename))
        unlink($filename);

    $data = $settings['htaccessContents'];
    if ($enablePW) {
        $data .= "AuthUserFile " . $settings['mediaDirectory'] . "/config/.htpasswd\nAuthType Basic\nAuthName \"Falcon Player\"\n<If \"%{HTTP_HOST} != '127.0.0.1'\">\nRequire valid-user\n</If>\n";
    }

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

    // First clear file and set admin, then without clear for adding fpp user
    shell_exec("sudo htpasswd -cbd " . $settings['mediaDirectory'] . "/config/.htpasswd admin " . $value);
    shell_exec("sudo htpasswd -bd " . $settings['mediaDirectory'] . "/config/.htpasswd fpp " . $value);
}

/////////////////////////////////////////////////////////////////////////////
function ApplySetting($setting, $value) {
    switch ($setting) {
        case 'ClockDate':       SetDate($value);              break;
        case 'ClockTime':       SetTime($value);              break;
        case 'ntp':             SetNTP($value);               break;
        case 'ntpServer':       SetNTPServer($value);         break;
        case 'passwordEnable':  EnableUIPassword($value);     break;
        case 'password':        SetUIPassword($value);        break;
        case 'piRTC':           SetRTC($value);               break;
        case 'TimeZone':        SetTimeZone($value);          break;
    }
}

?>
