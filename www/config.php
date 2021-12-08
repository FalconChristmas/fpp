<?php

$SUDO = "sudo";
$debug = false;
$fppRfsVersion = "Unknown";
$fppHome = "/home/fpp";

$settingsFile = $fppHome . "/media/settings";

if (file_exists("/etc/fpp/rfs_version")) {
    $fppRfsVersion = trim(file_get_contents("/etc/fpp/rfs_version"));
}

if (file_exists("/opt/fpp/www/fppversion.php")) {
    include_once "/opt/fpp/www/fppversion.php";
} else {
    include_once "/opt/fpp/www/fppunknown_versions.php";
}

// Allow overrides that we'll ignore from the git repository to make it
// easier to develop on machines configured differently than our current
// Pi image.
@include '.config.php';

// Settings array so we can stop making individual variables for each new setting
$settings = array();
$pluginSettings = array();

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
        if (isset($v["vendor"]["name"]) && ($v["vendor"]["name"] == "Upgrade")
            && isset($v["designer"]) && ($v["designer"] == "Unknown")) {
            return true;
        }
    }
    if (isset($v["verifiedKeyId"]) && file_exists("/opt/fpp/scripts/keys/" . $v["verifiedKeyId"] . "_pub.pem")) {
        return true;
    }
    return false;
}

// Set some defaults
$fppMode = "player";
$fppDir = dirname(dirname(__FILE__));
$mediaDirectory = $fppHome . "/media";
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

$settings['HostName'] = 'FPP';
$settings['HostDescription'] = '';
$settings['Title'] = "Falcon Player - FPP";
$settings['fppBinDir'] = '/opt/fpp/src';
$settings['wwwDir'] = '/opt/fpp/www';

$settings['Platform'] = false;
if (file_exists("/etc/fpp/platform")) {
    $settings['Platform'] = trim(file_get_contents("/etc/fpp/platform"));
}

if (file_exists($mediaDirectory . "/tmp/cape-info.json")) {
    $cape_info = json_decode(file_get_contents($mediaDirectory . "/tmp/cape-info.json"), true);
    if (isset($cape_info["vendor"])) {
        if (!ApprovedCape($cape_info)) {
            unset($cape_info["vendor"]);
        }
    }
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
    } else if (preg_match('/Pi 4/', $settings['SubPlatform'])) {
        $settings['Variant'] = "Pi 4";
        $settings['Logo'] = "Raspberry_Pi_4.png";
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
} else if ($settings['Platform'] == "BeagleBone Black") {
    $settings['OSImagePrefix'] = "BBB";
    $settings['LogoLink'] = "http://beagleboard.org/";
    $settings['BBB_Tethering'] = "1";
    $settings['SubPlatform'] = trim(file_get_contents("/proc/device-tree/model"));
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
} else if ($settings['Platform'] == "PogoPlug") {
    $settings['Logo'] = "pogoplug_logo.png";
    $settings['LogoLink'] = "";
} else if ($settings['Platform'] == "ODROID") {
    $settings['Logo'] = "odroid_logo.gif";
    $settings['LogoLink'] = "http://www.hardkernel.com/main/main.php";
} else if ($settings['Platform'] == "OrangePi") {
    $settings['Logo'] = "orangepi_logo.png";
    $settings['LogoLink'] = "http://www.orangepi.org/";
    $settings['SubPlatform'] = trim(file_get_contents("/sys/firmware/devicetree/base/model"));
    if (preg_match('/Orange Pi Zero/', $settings['SubPlatform'])) {
        $settings['Logo'] = "orangepi_zero_logo.png";
    }
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
} else if ($settings['Platform'] == "Debian") {
    $settings['Logo'] = "debian_logo.png";
    $settings['LogoLink'] = "https://www.debian.org/";
} else if ($settings['Platform'] == "Ubuntu") {
    $settings['Logo'] = "ubuntu_logo.png";
    $settings['LogoLink'] = "https://ubuntu.com/";
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
} else {
    $settings['Logo'] = "";
    $settings['LogoLink'] = "";
}

$fd = @fopen($settingsFile, "r");
if ($fd) {
    do {
        global $settings;
        global $fppMode, $volume, $settingsFile;
        global $mediaDirectory, $musicDirectory, $sequenceDirectory, $playlistDirectory;
        global $eventDirectory, $videoDirectory, $scriptDirectory, $logDirectory, $exim4Directory;
        global $pluginDirectory, $emailenable, $emailguser, $emailgpass, $emailfromtext, $emailtoemail;
        global $universeFile, $pixelnetFile, $scheduleFile, $bytesFile, $outputProcessorsFile;

        // Parse the file, assuming it exists
        $data = fgets($fd);
        $split = explode("=", $data);
        if (count($split) < 2) {
            if ($debug) {
                error_log("not long enough");
            }

            continue;
        }

        $key = trim($split[0]);
        $value = trim($split[1]);

        $value = preg_replace("/\"/", "", $value);

        if ($key != "") {
            // If we have a Directory setting that doesn't
            // end in a slash, then add one
            if ((preg_match("/Directory$/", $key)) &&
                (!preg_match("/\/$/", $value))) {
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

    fclose($fd);
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
$settings['configDirectory'] = $mediaDirectory . "/config";
$settings['channelOutputsFile'] = $mediaDirectory . "/channeloutputs";
$settings['channelOutputsJSON'] = $mediaDirectory . "/config/channeloutputs.json";
$settings['co-other'] = $mediaDirectory . "/config/co-other.json";
$settings['co-pixelStrings'] = $mediaDirectory . "/config/co-pixelStrings.json";
$settings['co-bbbStrings'] = $mediaDirectory . "/config/co-bbbStrings.json";
$settings['universeOutputs'] = $mediaDirectory . "/config/co-universes.json";
$settings['universeInputs'] = $mediaDirectory . "/config/ci-universes.json";
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
$settings['playlistDirectory'] = $playlistDirectory;
$settings['pluginDirectory'] = $pluginDirectory;
$settings['docsDirectory'] = $docsDirectory;
$settings['fppRfsVersion'] = $fppRfsVersion;
$settings['exim4Directory'] = $exim4Directory;
$settings['emailenable'] = $emailenable;
$settings['emailguser'] = $emailguser;
$settings['emailfromtext'] = $emailfromtext;
$settings['emailtoemail'] = $emailtoemail;
$settings['outputProcessorsFile'] = $outputProcessorsFile;

/*
 * Load default values from settings.json
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

//
// Didn't want to wait and check if ntp is synced
// (takes too long). These dates will periodically need updated
//
$year = date('Y');
if ($year < 2021 || $year > 2030) {
    # Probably no internet
    define('MINYEAR', date('Y') - 20);
    define('MAXYEAR', date('Y') + 40);
} else {
    define('MINYEAR', date('Y') - 2);
    define('MAXYEAR', date('Y') + 5);
}

LoadLocale();

/////////////////////////////////////////////////////////////////////////////
function GetDirSetting($dir)
{
    $dir = strtolower($dir); // Support both cases

    if ($dir == "sequences") {return GetSettingValue('sequenceDirectory');} else if ($dir == "music") {return GetSettingValue('musicDirectory');} else if ($dir == "videos") {return GetSettingValue('videoDirectory');} else if ($dir == "images") {return GetSettingValue('imageDirectory');} else if ($dir == "effects") {return GetSettingValue('effectDirectory');} else if ($dir == "scripts") {return GetSettingValue('scriptDirectory');} else if ($dir == "logs") {return GetSettingValue('logDirectory');} else if ($dir == "uploads") {return GetSettingValue('uploadDirectory');} else if ($dir == "docs") {return GetSettingValue('docsDirectory');} else if ($dir == "config") {return GetSettingValue('configDirectory');}

    return "";
}
/////////////////////////////////////////////////////////////////////////////

// $skipJSsettings is only set in fppjson.php and fppxml.php
// to prevent this JavaScript from being printed
if (!isset($skipJSsettings)) {
    ?>
<script type="text/javascript">
    MINYEAR = <?echo MINYEAR; ?>;
    MAXYEAR = <?echo MAXYEAR; ?>;
	var settings = new Array();
<?
    foreach ($settings as $key => $value) {
        if (!is_array($value)) {
            printf("	settings['%s'] = \"%s\";\n", $key, $value);
        } else {
            $js_array = json_encode($value);
            printf("    settings['%s'] = %s;\n", $key, $js_array);
        }
    }
    ?>

    var FPP_FULL_VERSION = '<?echo getFPPVersion(); ?>';
    var FPP_VERSION = '<?echo getFPPVersionFloatStr(); ?>';
    var FPP_MAJOR_VERSION = <?echo getFPPMajorVersion(); ?>;
    var FPP_MINOR_VERSION = <?echo getFPPMinorVersion(); ?>;
    <?if (getFPPPatchVersion() == "") {?>
    var FPP_PATCH_VERSION = 0;
    <?} else {?>
    var FPP_PATCH_VERSION = <?echo getFPPPatchVersion(); ?>;
    <?}?>

	var pageName = "<?echo str_ireplace('.php', '', basename($_SERVER['PHP_SELF'])) ?>";

  var helpPage = "<?echo basename($_SERVER['PHP_SELF']) ?>";
  if (pageName == "plugin")
  {
    var pluginPage = "<?echo preg_replace('/.*page=/', '', $_SERVER['REQUEST_URI']); ?>";
    var pluginBase = "<?echo preg_replace("/^\//", "", preg_replace('/page=.*/', '', $_SERVER['REQUEST_URI'])); ?>";
    helpPage = pluginBase + "nopage=1&page=help/" + pluginPage;
  }
  else
  {
    helpPage = "help/" + helpPage;
  }

</script>
<?
}

// Put variables here that we don't want in the JavaScript settings array
// $settings['key'] = "value";

?>
