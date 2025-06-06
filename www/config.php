<?php

require_once "common.php";

setlocale(LC_CTYPE, "en_US.UTF-8");

$SUDO = "sudo";
$debug = false;
$fppRfsVersion = "Unknown";

if (file_exists(__DIR__ . "/media_root.txt")) {
    $mediaDirectory = trim(file_get_contents(__DIR__ . "/media_root.txt"));
    $fppHome = dirname($mediaDirectory);
} else if (is_dir("/home/fpp")) {
    $fppHome = "/home/fpp";
    $mediaDirectory = "/home/fpp/media";
} else {
    $fppHome = sys_get_temp_dir() . "/fpp";
    if (!is_dir($fppHome)) {
        mkdir($fppHome);
    }
    $mediaDirectory = $fppHome . "/fpp";
}
if (!is_dir($mediaDirectory)) {
    mkdir($mediaDirectory);
}
$settingsFile = $mediaDirectory . "/settings";

if (file_exists("/etc/fpp/rfs_version")) {
    $fppRfsVersion = trim(file_get_contents("/etc/fpp/rfs_version"));
}

if (file_exists(__DIR__ . "/fppversion.php")) {
    include_once __DIR__ . "/fppversion.php";
} else {
    include_once __DIR__ . "/fppunknown_versions.php";
}

// Allow overrides that we'll ignore from the git repository to make it
// easier to develop on machines configured differently than our current
// Pi image.
if (file_exists('.config.php')) {
    @include '.config.php';
}

// Settings array so we can stop making individual variables for each new setting
$settings = array();
$pluginSettings = array();
$themeInfo = array();

$themeInfo["FOOTER_URL"] = "https://www.falconplayer.com";
$themeInfo["FOOTER_URL_TEXT"] = "www.falconplayer.com";

$themeInfo["HEADER_LOGO1"] = "images/redesign/falconlogo.svg";
$themeInfo["HEADER_LOGO2"] = "images/redesign/fpp-logo.svg";

// FIXME, need to convert other settings below to use this array
$settings['fppMode'] = "player";

// Helper function for accessing the global settings array
function GetSettingValue($setting, $default = '', $prefix = '', $suffix = '')
{
    global $settings;

    if (isset($settings[$setting])) {
        if ($settings[$setting] != '') {
            return $prefix . $settings[$setting] . $suffix;
        } else {
            return $default;
        }

    }

    return $default;
}

$settingGroups = array();
$settingInfos = array();
$pluginSettingInfosLoaded = 0;

function LoadSettingInfos()
{
    global $settings;
    global $settingInfos;
    global $settingGroups;

    if (empty($settingInfos) || empty($settingGroups)) {
        $data = json_decode(file_get_contents($settings['fppDir'] . '/www/settings.json'), true);
        $settingInfos = $data['settings'];
        $settingGroups = $data['settingGroups'];
    }
}

function LoadLocale()
{
    global $settings;
    $locale = array();

    if (!isset($settings['Locale'])) {
        $settings['Locale'] = 'Global';
    }

    $localeFile = $settings['fppDir'] . '/etc/locale/Global.json';
    if (file_exists($localeFile)) {
        $localeStr = file_get_contents($localeFile);
        $locale = json_decode($localeStr, true);
    }

    if ($settings['Locale'] != 'Global') {
        $localeFile = $settings['fppDir'] . '/etc/locale/' . $settings['Locale'] . '.json';
        if (file_exists($localeFile)) {
            $localeStr = file_get_contents($localeFile);
            $tmpLocale = json_decode($localeStr, true);
            $locale = array_merge($locale, $tmpLocale);
        }
    }

    $settings['locale'] = $locale;
}

function ApprovedCape($v)
{
    if (isset($v["vendor"])) {
        if (
            isset($v["vendor"]["name"]) && ($v["vendor"]["name"] == "Upgrade")
            && isset($v["designer"]) && ($v["designer"] == "Unknown")
        ) {
            return true;
        }
        if (isset($v["vendor"]["name"]) && ($v["vendor"]["name"] == 'FalconChristmas.com')) {
            return true;
        }
    }
    if (isset($v["verifiedKeyId"]) && $v["verifiedKeyId"] != "") {
        return true;
    }
    return false;
}

// Set some defaults
$fppMode = "player";
$fppDir = dirname(dirname(__FILE__));
$bootDirectory = file_exists("/boot/firmware") ? "/boot/firmware" : "/boot";
$pluginDirectory = $mediaDirectory . "/plugins";
$configDirectory = $mediaDirectory . "/config";
$docsDirectory = $fppDir . "/docs";
$musicDirectory = $mediaDirectory . "/music";
$sequenceDirectory = $mediaDirectory . "/sequences";
$playlistDirectory = $mediaDirectory . "/playlists";
$eventDirectory = $mediaDirectory . "/events";
$videoDirectory = $mediaDirectory . "/videos";
$imageDirectory = $mediaDirectory . "/images";
$effectDirectory = $mediaDirectory . "/effects";
$scriptDirectory = $mediaDirectory . "/scripts";
$logDirectory = $mediaDirectory . "/logs";
$uploadDirectory = $mediaDirectory . "/upload";
$universeFile = $mediaDirectory . "/universes";
$pixelnetFile = $mediaDirectory . "/pixelnetDMX";
$scheduleFile = $mediaDirectory . "/schedule";
$bytesFile = $mediaDirectory . "/bytesReceived";
$outputProcessorsFile = $mediaDirectory . "/config/outputprocessors.json";
$exim4Directory = $mediaDirectory . "/exim4";
$timezoneFile = $mediaDirectory . "/timezone";
$volume = 0;
$emailenable = "0";
$emailguser = "";
$emailgpass = "";
$emailfromtext = "";
$emailtoemail = "";

if ($debug) {
    error_log("DEFAULTS:");
    error_log("fppDir: $fppDir");
    error_log("fppMode: $fppMode");
    error_log("settings: $settingsFile");
    error_log("media: $mediaDirectory");
    error_log("music: $musicDirectory");
    error_log("event: $eventDirectory");
    error_log("video: $videoDirectory");
    error_log("sequence: $sequenceDirectory");
    error_log("playlist: $playlistDirectory");
    error_log("effects: $effectDirectory");
    error_log("scripts: $scriptDirectory");
    error_log("logs: $logDirectory");
    error_log("uploads: $uploadDirectory");
    error_log("plugins: $pluginDirectory");
    error_log("universe: $universeFile");
    error_log("pixelnet: $pixelnetFile");
    error_log("schedule: $scheduleFile");
    error_log("outputProcessors: $outputProcessorsFile");
    error_log("bytes: $bytesFile");
    error_log("volume: $volume");
    error_log("emailenable: $emailenable");
    error_log("emailguser: $emailguser");
    error_log("emailfromtext: $emailfromtext");
    error_log("emailtoemail: $emailtoemail");
}

$settings['HostDescription'] = '';
$settings['fppBinDir'] = $fppDir . '/src';
$settings['wwwDir'] = $fppDir . '/www';

$settings['Platform'] = false;
if (file_exists("/etc/fpp/platform")) {
    $settings['Platform'] = trim(file_get_contents("/etc/fpp/platform"));
}

if (file_exists($mediaDirectory . "/tmp/cape-info.json")) {
    $cape_info = json_decode(file_get_contents($mediaDirectory . "/tmp/cape-info.json"), true);
    $settings['cape-info'] = $cape_info;
}

$settings['statsPublishUrl'] = "https://fppstats.falconchristmas.com/api/upload";
$settings['statsFile'] = "/tmp/fpp_stats.json";
$settings['Variant'] = $settings['Platform'];
$settings['SubPlatform'] = "";
$settings['OSImagePrefix'] = "";

if ($settings['Platform'] == false) {
    $settings['Platform'] = exec("uname -s");
    $settings['Variant'] = $settings['Platform'];
}
$settings['HostName'] = 'FPP';
$settings["IsDesktop"] = false;
if (file_exists("/etc/fpp/desktop")) {
    $settings["IsDesktop"] = true;
}
$settings['BeaglePlatform'] = false;
if ($settings['Platform'] == "Raspberry Pi") {
    $settings['OSImagePrefix'] = "Pi";
    $settings['LogoLink'] = "http://raspberrypi.org/";
    $settings['SubPlatform'] = trim(file_get_contents("/sys/firmware/devicetree/base/model"));

    if (preg_match('/Pi Model A Rev/', $settings['SubPlatform'])) {
        $settings['Variant'] = "Model A";
        $settings['Logo'] = "Raspberry_Pi_A.png";
    } else if (preg_match('/Pi Model B Rev/', $settings['SubPlatform'])) {
        $settings['Variant'] = "Model B";
        $settings['Logo'] = "Raspberry_Pi_B.png";
    } else if (preg_match('/Pi Model A Plus/', $settings['SubPlatform'])) {
        $settings['Variant'] = "Model A+";
        $settings['Logo'] = "Raspberry_Pi_A+.png";
    } else if (preg_match('/Pi Model B Plus/', $settings['SubPlatform'])) {
        $settings['Variant'] = "Model B+";
        $settings['Logo'] = "Raspberry_Pi_B+.png";
    } else if (preg_match('/Pi 2 Model B/', $settings['SubPlatform'])) {
        $settings['Variant'] = "Pi 2 Model B";
        $settings['Logo'] = "Raspberry_Pi_2.png";
    } else if (preg_match('/Pi 3 Model B Plus/', $settings['SubPlatform'])) {
        $settings['Variant'] = "Pi 3 Model B+";
        $settings['Logo'] = "Raspberry_Pi_3B+.png";
    } else if (preg_match('/Pi 3 Model A Plus/', $settings['SubPlatform'])) {
        $settings['Variant'] = "Pi 3 Model A+";
        $settings['Logo'] = "Raspberry_Pi_3A+.png";
    } else if (preg_match('/Pi 3 Model B/', $settings['SubPlatform'])) {
        $settings['Variant'] = "Pi 3 Model B";
        $settings['Logo'] = "Raspberry_Pi_3.png";
    } else if (preg_match('/Pi 4/', $settings['SubPlatform']) || preg_match('/Pi Compute Module 4/', $settings['SubPlatform'])) {
        $settings['Variant'] = "Pi 4";
        $settings['Logo'] = "Raspberry_Pi_4.png";
    } else if (preg_match('/Pi 5/', $settings['SubPlatform']) || preg_match('/Pi Compute Module 5/', $settings['SubPlatform'])) {
        $settings['Variant'] = "Pi 5";
        $settings['Logo'] = "Raspberry_Pi_5.png";
    } else if (preg_match('/Pi Zero 2/', $settings['SubPlatform'])) {
        $settings['Variant'] = "PiZero 2";
        $settings['Logo'] = "Raspberry_Pi_Zero2.png";
    } else if (preg_match('/Pi Zero W/', $settings['SubPlatform'])) {
        $settings['Variant'] = "PiZero W";
        $settings['Logo'] = "Raspberry_Pi_ZeroW.png";
    } else if (preg_match('/Pi Zero/', $settings['SubPlatform'])) {
        $settings['Variant'] = "PiZero";
        $settings['Logo'] = "Raspberry_Pi_Zero.png";
    } else if ($settings['SubPlatform'] == "V2P-CA15") {
        $settings['Variant'] = "qemu";
        $settings['Logo'] = "QEMU_Logo.png";
        $settings['LogoLink'] = "http://qemu.org/";
    } else {
        $settings['Variant'] = "UNKNOWN";
        $settings['Logo'] = "Raspberry_Pi_Logo.png";
    }
} else if ($settings['Platform'] == "BeagleBone 64") {
    $settings['OSImagePrefix'] = "BB64";
    $settings['LogoLink'] = "http://beagleboard.org/";
    $settings['BBB_Tethering'] = "1";
    $settings['SubPlatform'] = trim(file_get_contents("/proc/device-tree/model"));
    $settings['BeaglePlatform'] = true;
    if (preg_match('/PocketBeagle2/', $settings['SubPlatform'])) {
        $settings['Variant'] = "PocketBeagle2";
        $settings['Logo'] = "beagle_pocket.png";
    } else {
        // for now, eventually support others?
        $settings['Variant'] = "PocketBeagle2";
        $settings['Logo'] = "beagle_pocket.png";
    }
} else if ($settings['Platform'] == "BeagleBone Black") {
    $settings['OSImagePrefix'] = "BBB";
    $settings['LogoLink'] = "http://beagleboard.org/";
    $settings['BBB_Tethering'] = "1";
    $settings['SubPlatform'] = trim(file_get_contents("/proc/device-tree/model"));
    $settings['BeaglePlatform'] = true;
    if (preg_match('/PocketBeagle/', $settings['SubPlatform'])) {
        $settings['Variant'] = "PocketBeagle";
        $settings['Logo'] = "beagle_pocket.png";
    } else if (preg_match('/Green Wireless/', $settings['SubPlatform'])) {
        $settings['Variant'] = "BeagleBone Green Wireless";
        $settings['Logo'] = "beagle_greenwifi.png";
    } else if (preg_match('/Green/', $settings['SubPlatform'])) {
        $settings['Variant'] = "BeagleBone Green";
        $settings['Logo'] = "beagle_green.png";
    } else if (preg_match('/Black Wireless/', $settings['SubPlatform'])) {
        $settings['Variant'] = "BeagleBone Black Wireless";
        $settings['Logo'] = "beagle_blackwifi.png";
    } else if (preg_match('/BeagleBone Black/', $settings['SubPlatform'])) {
        $settings['Variant'] = "BeagleBone Black";
        $settings['Logo'] = "beagle_black.png";
    } else if (preg_match('/SanCloud BeagleBone Enhanced/', $settings['SubPlatform'])) {
        $settings['Variant'] = "SanCloud BeagleBone Enhanced";
        $settings['Logo'] = "beagle_sancloud.png";
    } else {
        $settings['Variant'] = "UNKNOWN";
        $settings['Logo'] = "beagle_logo.png";
    }
} else if ($settings['Platform'] == "Debian" || $settings['Platform'] == "Ubuntu" || $settings['Platform'] == "Mint" || $settings['Platform'] == "Armbian" || $settings['Platform'] == "OrangePi") {
    if (file_exists("/.dockerenv")) {
        $settings['SubPlatform'] = "Docker";
    } else {
        if (file_exists("/proc/device-tree/model")) {
            $settings['SubPlatform'] = trim(file_get_contents("/proc/device-tree/model"));
        }
    }
    $settings['Variant'] = $settings['SubPlatform'];
    if (preg_match('/Orange/', $settings['SubPlatform'])) {
        $settings['Logo'] = "orangepi_logo.png";
        $settings['LogoLink'] = "http://www.orangepi.org/";
    } else if (preg_match('/Pine/', $settings['SubPlatform'])) {
        $settings['Logo'] = "pine64_logo.png";
        $settings['LogoLink'] = "https://www.pine64.org/";
    } else if (preg_match('/ODROID/', $settings['SubPlatform'])) {
        $settings['Logo'] = "odroid_logo.gif";
        $settings['LogoLink'] = "http://www.hardkernel.com/main/main.php";
    } else if (preg_match('/Banana/', $settings['SubPlatform'])) {
        $settings['Logo'] = "debian_logo.png";
        $settings['LogoLink'] = "http://www.banana-pi.com/eindex.asp";
    } else if ($settings['Platform'] == "Debian") {
        $settings['Logo'] = "debian_logo.png";
        $settings['LogoLink'] = "https://www.debian.org/";
    } else if ($settings['Platform'] == "Ubuntu") {
        $settings['Logo'] = "ubuntu_logo.png";
        $settings['LogoLink'] = "https://ubuntu.com/";
    } else if ($settings['Platform'] == "Mint") {
        $settings['Logo'] = "mint_logo.png";
        $settings['LogoLink'] = "https://linuxmint.com/";
    } else {
        $settings['Logo'] = "debian_logo.png";
        $settings['LogoLink'] = "https://www.armbian.com/";
    }
} else if ($settings['Platform'] == "PogoPlug") {
    $settings['Logo'] = "pogoplug_logo.png";
    $settings['LogoLink'] = "";
} else if ($settings['Platform'] == "ODROID") {
    $settings['Logo'] = "odroid_logo.gif";
    $settings['LogoLink'] = "http://www.hardkernel.com/main/main.php";
} else if ($settings['Platform'] == "Pine64") {
    $settings['Logo'] = "pine64_logo.png";
    $settings['LogoLink'] = "https://www.pine64.org/";
} else if ($settings['Platform'] == "CHIP") {
    $settings['Logo'] = "chip_logo.png";
    $settings['LogoLink'] = "http://www.getchip.com/";
} else if (file_exists("/.dockerenv")) {
    $settings['SubPlatform'] = $settings['Platform'];
    $settings['Variant'] = $settings['Platform'];
    $settings['Platform'] = "Docker";
    $settings['Logo'] = "debian_logo.png";
    $settings['LogoLink'] = "https://www.debian.org/";
} else if ($settings['Platform'] == "Fedora") {
    $settings['Logo'] = "fedora_logo.png";
    $settings['LogoLink'] = "https://getfedora.org/";
} else if ($settings['Platform'] == "Linux") {
    $settings['Logo'] = "tux_logo.png";
    $settings['LogoLink'] = "http://www.linux.com/";
} else if ($settings['Platform'] == "FreeBSD") {
    $settings['Logo'] = "freebsd_logo.png";
    $settings['LogoLink'] = "http://www.freebsd.org/";
} else if ($settings['Platform'] == "qemu") {
    $settings['Logo'] = "QEMU_Logo.png";
    $settings['LogoLink'] = "http://qemu.org/";
} else if ($settings['Platform'] == "Darwin") {
    $settings['Platform'] = "MacOS";
    $settings['Logo'] = "Apple-Logo.png";
    $settings['LogoLink'] = "http://apple.com/";
    $settings['Variant'] = trim(shell_exec("sw_vers -productVersion"));
    $SUDO = "";
    $settings["IsDesktop"] = true;
} else {
    $settings['Logo'] = "";
    $settings['LogoLink'] = "";
}

$fd = @fopen($settingsFile, "r");
if ($fd) {
    flock($fd, LOCK_SH);
    do {
        global $settings;
        global $fppMode, $volume, $settingsFile;
        global $mediaDirectory, $musicDirectory, $sequenceDirectory, $playlistDirectory;
        global $eventDirectory, $videoDirectory, $scriptDirectory, $logDirectory, $exim4Directory;
        global $pluginDirectory, $emailenable, $emailguser, $emailgpass, $emailfromtext, $emailtoemail;
        global $universeFile, $pixelnetFile, $scheduleFile, $bytesFile, $outputProcessorsFile;

        // Parse the file, assuming it exists
        $data = fgets($fd);
        $split = explode("=", $data, 2);
        if (count($split) < 2) {
            if ($debug) {
                error_log("not long enough");
            }

            continue;
        }

        $key = trim($split[0]);
        $value = trim($split[1]);

        if (!json_object_validate($value)) {
            $value = preg_replace("/\"/", "", $value);
        }

        if ($key != "") {
            // If we have a Directory setting that doesn't
            // end in a slash, then add one
            if (
                (preg_match("/Directory$/", $key)) &&
                (!preg_match("/\/$/", $value))
            ) {
                $value .= "/";
            }

            $settings[$key] = $value;
        }

        switch ($key) {
            case "fppMode":
                if ($value == "master") {
                    $fppMode = "player";
                    $settings["MultiSyncEnabled"] = "1";
                    $settings[$key] = "player";
                } else if ($value == "bridge") {
                    $fppMode = "player";
                    $settings[$key] = "player";
                } else {
                    $fppMode = $value;
                }
                break;
            case "volume":
                $volume = $value;
                break;
            case "settingsFile":
                $settingsFile = $value;
                break;
            case "mediaDirectory":
                $mediaDirectory = $value . "/";
                break;
            case "musicDirectory":
                $musicDirectory = $value . "/";
                break;
            case "eventDirectory":
                $eventDirectory = $value . "/";
                break;
            case "videoDirectory":
                $videoDirectory = $value . "/";
                break;
            case "sequenceDirectory":
                $sequenceDirectory = $value . "/";
                break;
            case "playlistDirectory":
                $playlistDirectory = $value . "/";
                break;
            case "effectDirectory":
                $effectDirectory = $value . "/";
                break;
            case "logDirectory":
                $logDirectory = $value . "/";
                break;
            case "uploadDirectory":
                $uploadDirectory = $value . "/";
                break;
            case "pluginDirectory":
                $pluginDirectory = $value . "/";
                break;
            case "scriptDirectory":
                $scriptDirectory = $value . "/";
                break;
            case "universeFile":
                $universeFile = $value;
                break;
            case "pixelnetFile":
                $pixelnetFile = $value;
                break;
            case "scheduleFile":
                $scheduleFile = $value;
                break;
            case "bytesFile":
                $bytesFile = $value;
                break;
            case "outputProcessorsFile":
                $outputProcessorsFile = $value;
                break;
            case "exim4Directory":
                $exim4Directory = $value . "/";
                break;
            case "emailenable":
                $emailenable = $value;
                break;
            case "emailguser":
                $emailguser = $value;
                break;
            case "emailfromtext":
                $emailfromtext = $value;
                break;
            case "emailtoemail":
                $emailtoemail = $value;
                break;
        }
    } while ($data != null);
    flock($fd, LOCK_UN);
    fclose($fd);
}

if ($settings["IsDesktop"]) {
    $settings["HostName"] = explode(".", gethostname())[0];
}

$pageTitle = "FPP - " . $settings['HostName'];
if ($settings['HostName'] == "FPP") {
    $pageTitle = "Falcon Player - FPP";
}

$settings['fppMode'] = $fppMode;
$settings['fppDir'] = $fppDir;
$settings['playlistDirectory'] = $playlistDirectory;
$settings['pluginDirectory'] = $pluginDirectory;
$settings['mediaDirectory'] = $mediaDirectory;
$settings['bootDirectory'] = $bootDirectory;
$settings['configDirectory'] = $mediaDirectory . "/config";
$settings['channelOutputsFile'] = $mediaDirectory . "/channeloutputs";
$settings['channelOutputsJSON'] = $mediaDirectory . "/config/channeloutputs.json";
$settings['co-other'] = $mediaDirectory . "/config/co-other.json";
$settings['co-pixelStrings'] = $mediaDirectory . "/config/co-pixelStrings.json";
$settings['co-bbbStrings'] = $mediaDirectory . "/config/co-bbbStrings.json";
$settings['co-pwm'] = $mediaDirectory . "/config/co-pwm.json";
$settings['universeOutputs'] = $mediaDirectory . "/config/co-universes.json";
$settings['universeInputs'] = $mediaDirectory . "/config/ci-universes.json";
$settings['dmxInputs'] = $mediaDirectory . "/config/ci-dmx.json";
$settings['model-overlays'] = $mediaDirectory . "/config/model-overlays.json";
$settings['scheduleJsonFile'] = $mediaDirectory . "/config/schedule.json";
$settings['scriptDirectory'] = $scriptDirectory;
$settings['sequenceDirectory'] = $sequenceDirectory;
$settings['musicDirectory'] = $musicDirectory;
$settings['videoDirectory'] = $videoDirectory;
$settings['imageDirectory'] = $imageDirectory;
$settings['effectDirectory'] = $effectDirectory;
$settings['eventDirectory'] = $eventDirectory;
$settings['logDirectory'] = $logDirectory;
$settings['uploadDirectory'] = $uploadDirectory;
$settings['docsDirectory'] = $docsDirectory;
$settings['fppRfsVersion'] = $fppRfsVersion;
$settings['exim4Directory'] = $exim4Directory;
$settings['emailenable'] = $emailenable;
$settings['emailguser'] = $emailguser;
$settings['emailfromtext'] = $emailfromtext;
$settings['emailtoemail'] = $emailtoemail;
$settings['outputProcessorsFile'] = $outputProcessorsFile;

/*
 * Load setting info and default values from www/settings.json
 */
LoadSettingInfos();

foreach ($settingInfos as $key => $obj) {
    if (isset($obj['default'])) {
        if (!isset($settings[$key])) {
            #error_log($key . " " . $obj['default']);
            $settings[$key] = $obj['default'];
        }
    }
}

if ((isset($settings['masqUIPlatform'])) && ($settings['masqUIPlatform'] != '')) {
    $settings['Platform'] = $settings['masqUIPlatform'];
}

if (!isset($settings['restartFlag'])) {
    $settings['restartFlag'] = 0;
}

if (!isset($settings['rebootFlag'])) {
    $settings['rebootFlag'] = 0;
}

$settings['hideExternalURLs'] = false;
$localIps = array('127.0.0.1', "::1");

if ((isset($settings['Kiosk'])) && ($settings['Kiosk'] == 1) && in_array($_SERVER['REMOTE_ADDR'], $localIps)) {
    $settings['LogoLink'] = '';
    $settings['hideExternalURLs'] = true;
}

if (isset($_SERVER['HTTP_HIDE_EXTERNAL_LINKS']) && ($_SERVER['HTTP_HIDE_EXTERNAL_LINKS'] == "true")) {
    $settings['LogoLink'] = '';
    $settings['hideExternalURLs'] = true;
}

putenv("SCRIPTDIR=$scriptDirectory");
putenv("MEDIADIR=$mediaDirectory");
putenv("LOGDIR=$logDirectory");
putenv("SETTINGSFILE=$settingsFile");

if ($debug) {
    error_log("SET:");
    error_log("fppDir: $fppDir");
    error_log("fppMode: $fppMode");
    error_log("settings: $settingsFile");
    error_log("media: $mediaDirectory");
    error_log("music: $musicDirectory");
    error_log("event: $eventDirectory");
    error_log("video: $videoDirectory");
    error_log("sequence: $sequenceDirectory");
    error_log("playlist: $playlistDirectory");
    error_log("effects: $effectDirectory");
    error_log("scripts: $scriptDirectory");
    error_log("logs: $logDirectory");
    error_log("uploads: $uploadDirectory");
    error_log("plugins: $pluginDirectory");
    error_log("universe: $universeFile");
    error_log("pixelnet: $pixelnetFile");
    error_log("schedule: $scheduleFile");
    error_log("outputProcessors: $outputProcessorsFile");
    error_log("bytes: $bytesFile");
    error_log("volume: $volume");
    error_log("emailenable: $emailenable");
    error_log("emailguser: $emailguser");
    error_log("emailfromtext: $emailfromtext");
    error_log("emailtoemail: $emailtoemail");
}

$uiLevel = $settings['uiLevel'];

//Set the default timezone in PHP to match the currently set system timezone
date_default_timezone_set($settings['TimeZone']);

//
// Didn't want to wait and check if ntp is synced
// (takes too long). These dates will periodically need updated
//
$year = date('Y');
if ($year < 2024 || $year > 2030) {
    # Probably no internet
    define('MINYEAR', date('Y') - 20);
    define('MAXYEAR', date('Y') + 40);
} else {
    define('MINYEAR', date('Y') - 2);
    define('MAXYEAR', date('Y') + 5);
}

//Load the Interface locale
LoadLocale();

/////////////////////////////////////////////////////////////////////////////
function GetDirSetting($dir)
{
    $dir = strtolower($dir); // Support both cases

    if ($dir == "sequences") {
        return GetSettingValue('sequenceDirectory');
    } else if ($dir == "music") {
        return GetSettingValue('musicDirectory');
    } else if ($dir == "videos") {
        return GetSettingValue('videoDirectory');
    } else if ($dir == "images") {
        return GetSettingValue('imageDirectory');
    } else if ($dir == "effects") {
        return GetSettingValue('effectDirectory');
    } else if ($dir == "scripts") {
        return GetSettingValue('scriptDirectory');
    } else if ($dir == "logs") {
        return GetSettingValue('logDirectory');
    } else if ($dir == "uploads" || $dir == "upload") {
        return GetSettingValue('uploadDirectory');
    } else if ($dir == "docs") {
        return GetSettingValue('docsDirectory');
    } else if ($dir == "boot") {
        return GetSettingValue('bootDirectory');
    } else if ($dir == "config") {
        return GetSettingValue('configDirectory');
    } else if ($dir == "jsonbackups") {
        return GetSettingValue('configDirectory') . "/backups";
    } else if ($dir == "jsonbackupsalternate") {
        //Folder Location on the alternate store under which the backups are stored
        $fileCopy_BackupPath = 'Automatic_Backups';

        //build out the path to the alternative location as it's slightly custom
        return "/mnt/tmp/" . $fileCopy_BackupPath . "/config/backups";
    } else if ($dir == 'tmp') {
        return GetSettingValue('mediaDirectory') . '/tmp';
    } else if ($dir == 'crashes') {
        return GetSettingValue('mediaDirectory') . '/crashes';
    } else if ($dir == 'backups') {
        return GetSettingValue('mediaDirectory') . '/backups';
    } else if ($dir == 'playlists') {
        return GetSettingValue('playlistDirectory');
    } else if ($dir == 'plugins') {
        return GetSettingValue('pluginDirectory');
    }
    return "";
}

if (file_exists($pluginDirectory)) {
    $handle = opendir($pluginDirectory);
    if ($handle) {
        $first = 1;
        while (($plugin = readdir($handle)) !== false) {
            if (!in_array($plugin, array('.', '..'))) {
                // Old naming convention ${MENU}_menu.inc
                $themeInc = $pluginDirectory . "/" . $plugin . "/theme.inc.php";
                if (file_exists($themeInc)) {
                    include $themeInc;
                }
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////

// $skipJSsettings is set in a few places
// to prevent this JavaScript from being printed
//
// HTTP discovery looks for the string "Falcon Player - FPP" in the resulting HTML
// so we'll make sure it's in a comment so that discovery will work
if (!isset($skipJSsettings)) {
    ?>
    <!--Falcon Player - FPP-->
    <script type="text/javascript">
        // Standard Common JS Variables
        MINYEAR = <? echo MINYEAR; ?>;
        MAXYEAR = <? echo MAXYEAR; ?>;
        var FPP_FULL_VERSION = '<? echo getFPPVersion(); ?>';
        var FPP_VERSION = '<? echo getFPPVersionFloatStr(); ?>';
        var FPP_MAJOR_VERSION = <? echo getFPPMajorVersion(); ?>;
        var FPP_MINOR_VERSION = <? echo getFPPMinorVersion(); ?>;
        <? if (getFPPPatchVersion() == "") { ?>
            var FPP_PATCH_VERSION = 0;
        <? } else { ?>
            var FPP_PATCH_VERSION = <? echo getFPPPatchVersion(); ?>;
        <? } ?>

        <? $pageName = str_ireplace('.php', '', basename($_SERVER['PHP_SELF'])); ?>
        var pageName = "<? echo $pageName; ?>";

        var helpPage = "<? echo basename($_SERVER['PHP_SELF']) ?>";
        if (pageName == "plugin") {
            var pluginPage = "<? echo preg_replace('/.*page=/', '', $_SERVER['REQUEST_URI']); ?>";
            var pluginBase = "<? echo preg_replace("/^\//", "", preg_replace('/page=.*/', '', $_SERVER['REQUEST_URI'])); ?>";
            helpPage = pluginBase + "nopage=1&page=help/" + pluginPage;
        }
        else {
            helpPage = "help/" + helpPage;
        }


        // Dynamic JS Settings Array with settings which need to be exposed on a page by page basis to browser
        var settings = new Array();
        <?

        foreach ($settings as $key => $value) {

            //Print Out settings which haven't been defined at all in www/settings.json
            if (!isset($settingInfos[$key])) {
                //Print out settings that need to be exposed to the browser in JS settings array - this is temporary until all settings properly defined in json file
                if (!is_array($value)) {
                    printf("	settings['%s'] = \"%s\"; // Needs proper defintion in JSON\n", $key, $value);
                } else {
                    $js_array = json_encode($value);
                    printf("    settings['%s'] = %s; // Needs proper defintion in JSON\n", $key, $js_array);
                }
                // printf("	console.log(\"%s\");\n", $key);     //Debugging
            }

            //Print out settings that are marked to be exposed to the browser via the config flag in www/settings.json
            if (isset($settingInfos[$key]["exposedAsJSToPages"])) {
                if (in_array($pageName, $settingInfos[$key]["exposedAsJSToPages"]) || in_array("all", $settingInfos[$key]["exposedAsJSToPages"])) {
                    //Print out settings to browser
                    if (!is_array($value)) {
                        printf("	settings['%s'] = \"%s\";\n", $key, $value);
                    } else {
                        $js_array = json_encode($value);
                        printf("    settings['%s'] = %s;\n", $key, $js_array);
                    }
                }
            }
        }
        ?>


    </script>
    <?
}

// Put variables here that we don't want in the JavaScript settings array
// $settings['key'] = "value";

?>