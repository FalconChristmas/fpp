<?php

$SUDO = "sudo";
$debug = false;
$fppRfsVersion = "Unknown";
$fppHome = "/home/fpp";

$settingsFile = $fppHome . "/media/settings";

if (file_exists("/etc/fpp/rfs_version"))
	$fppRfsVersion = trim(file_get_contents("/etc/fpp/rfs_version"));

// Allow overrides that we'll ignore from the git repository to make it
// easier to develop on machines configured differently than our current
// Pi image.
@include('.config.php');

// Settings array so we can stop making individual variables for each new setting
$settings = array();
$pluginSettings = array();

// FIXME, need to convert other settings below to use this array
$settings['fppMode'] = "player";

// Helper function for accessing the global settings array
function GetSettingValue($setting) {
	global $settings;

	if (isset($settings[$setting]))
		return $settings[$setting];

	return;  // FIXME, should we do this or return something else
}
function ApprovedVendor($v) {
    if (isSet($v["name"]) && ($v["name"] == "Daniel Kulp")) {
        return true;
    }
    return false;
}
function ApprovedDesigner($v) {
    if (isSet($v["designer"]) && ($v["designer"] == "Daniel Kulp")) {
        return true;
    }
    return false;
}

// Set some defaults
$fppMode = "player";
$fppDir = dirname(dirname(__FILE__));
$mediaDirectory    = $fppHome . "/media";
$pluginDirectory   = $mediaDirectory . "/plugins";
$configDirectory   = $mediaDirectory . "/config";
$docsDirectory     = $fppDir . "/docs";
$musicDirectory    = $mediaDirectory . "/music";
$sequenceDirectory = $mediaDirectory . "/sequences";
$playlistDirectory = $mediaDirectory . "/playlists";
$eventDirectory    = $mediaDirectory . "/events";
$videoDirectory    = $mediaDirectory . "/videos";
$imageDirectory    = $mediaDirectory . "/images";
$effectDirectory   = $mediaDirectory . "/effects";
$scriptDirectory   = $mediaDirectory . "/scripts";
$logDirectory      = $mediaDirectory . "/logs";
$uploadDirectory   = $mediaDirectory . "/upload";
$universeFile      = $mediaDirectory . "/universes";
$pixelnetFile      = $mediaDirectory . "/pixelnetDMX";
$scheduleFile      = $mediaDirectory . "/schedule";
$bytesFile         = $mediaDirectory . "/bytesReceived";
$outputProcessorsFile = $mediaDirectory . "/config/outputprocessors.json";
$exim4Directory    = $mediaDirectory . "/exim4";
$timezoneFile      = $mediaDirectory . "/timezone";
$volume            = 0;
$emailenable       = "0";
$emailguser		   = "";
$emailgpass        = "";
$emailfromtext     = "";
$emailtoemail      = "";

if ($debug)
{
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

$settings['Platform'] = FALSE;
if (file_exists("/etc/fpp/platform"))
	$settings['Platform'] = trim(file_get_contents("/etc/fpp/platform"));

if (file_exists($mediaDirectory . "/tmp/cape-info.json")) {
    $cape_info = json_decode(file_get_contents($mediaDirectory . "/tmp/cape-info.json"), true);
    if (isSet($cape_info["vendor"])) {
        if (!ApprovedVendor($cape_info["vendor"])
            && !ApprovedDesigner($cape_info)) {
            unset($cape_info["vendor"]);
        }
    }
    $settings['cape-info'] = $cape_info;
}

if ($settings['Platform'] == FALSE)
{
	$settings['Platform'] = exec("uname -s");
}

if ($settings['Platform'] == "Raspberry Pi")
{
	$settings['LogoLink'] = "http://raspberrypi.org/";
	$settings['SubPlatform'] = trim(file_get_contents("/sys/firmware/devicetree/base/model"));

	if (preg_match('/Pi Model A Rev/', $settings['SubPlatform']))
	{
		$settings['Variant'] = "Model A";
		$settings['Logo'] = "Raspberry_Pi_A.png";
	}
	else if (preg_match('/Pi Model B Rev/', $settings['SubPlatform']))
	{
		$settings['Variant'] = "Model B";
		$settings['Logo'] = "Raspberry_Pi_B.png";
	}
	else if (preg_match('/Pi Model A Plus/', $settings['SubPlatform']))
	{
		$settings['Variant'] = "Model A+";
		$settings['Logo'] = "Raspberry_Pi_A+.png";
	}
	else if (preg_match('/Pi Model B Plus/', $settings['SubPlatform']))
	{
		$settings['Variant'] = "Model B+";
		$settings['Logo'] = "Raspberry_Pi_B+.png";
	}
	else if (preg_match('/Pi 2 Model B/', $settings['SubPlatform']))
	{
		$settings['Variant'] = "Pi 2 Model B";
		$settings['Logo'] = "Raspberry_Pi_2.png";
	}
        else if (preg_match('/Pi 3 Model B Plus/', $settings['SubPlatform']))
        {
                $settings['Variant'] = "Pi 3 Model B+";
                $settings['Logo'] = "Raspberry_Pi_3B+.png";
        }
	else if (preg_match('/Pi 3 Model B/', $settings['SubPlatform']))
	{
		$settings['Variant'] = "Pi 3 Model B";
		$settings['Logo'] = "Raspberry_Pi_3.png";
	}
        else if (preg_match('/Pi Zero W/', $settings['SubPlatform']))
        {
                $settings['Variant'] = "PiZero W";
                $settings['Logo'] = "Raspberry_Pi_ZeroW.png";
        }
	else if (preg_match('/Pi Zero/', $settings['SubPlatform']))
	{
		$settings['Variant'] = "PiZero";
		$settings['Logo'] = "Raspberry_Pi_Zero.png";
	}
	else if ($settings['SubPlatform'] == "V2P-CA15")
	{
		$settings['Variant'] = "qemu";
		$settings['Logo'] = "QEMU_Logo.png";
		$settings['LogoLink'] = "http://qemu.org/";
	}
	else
	{
		$settings['Variant'] = "UNKNOWN";
		$settings['Logo'] = "Raspberry_Pi_Logo.png";
	}
}
else if ($settings['Platform'] == "BeagleBone Black")
{
	$settings['LogoLink'] = "http://beagleboard.org/";
	$settings['BBB_Tethering'] = "1";
	$settings['SubPlatform'] = trim(file_get_contents("/proc/device-tree/model"));
    if (preg_match('/PocketBeagle/', $settings['SubPlatform']))
    {
        $settings['Variant'] = "PocketBeagle";
        $settings['Logo'] = "beagle_pocket.png";
    }
    else if (preg_match('/Green Wireless/', $settings['SubPlatform']))
    {
        $settings['Variant'] = "BeagleBone Green Wireless";
        $settings['Logo'] = "beagle_greenwifi.png";
    }
    else if (preg_match('/Green/', $settings['SubPlatform']))
    {
        $settings['Variant'] = "BeagleBone Green";
        $settings['Logo'] = "beagle_green.png";
    }
    else if (preg_match('/Black Wireless/', $settings['SubPlatform']))
    {
        $settings['Variant'] = "BeagleBone Black Wireless";
        $settings['Logo'] = "beagle_blackwifi.png";
    }
    else if (preg_match('/BeagleBone Black/', $settings['SubPlatform']))
    {
        $settings['Variant'] = "BeagleBone Black";
        $settings['Logo'] = "beagle_black.png";
    }
    else if (preg_match('/SanCloud BeagleBone Enhanced/', $settings['SubPlatform']))
    {
        $settings['Variant'] = "SanCloud BeagleBone Enhanced";
        $settings['Logo'] = "beagle_sancloud.png";
    }
    else
    {
        $settings['Variant'] = "UNKNOWN";
        $settings['Logo'] = "beagle_logo.png";
    }

    
}
else if ($settings['Platform'] == "PogoPlug")
{
	$settings['Logo'] = "pogoplug_logo.png";
	$settings['LogoLink'] = "";
}
else if ($settings['Platform'] == "ODROID")
{
	$settings['Logo'] = "odroid_logo.gif";
	$settings['LogoLink'] = "http://www.hardkernel.com/main/main.php";
}
else if ($settings['Platform'] == "OrangePi")
{
	$settings['Logo'] = "orangepi_logo.png";
	$settings['LogoLink'] = "http://www.orangepi.org/";
}
else if ($settings['Platform'] == "Pine64")
{
	$settings['Logo'] = "pine64_logo.png";
	$settings['LogoLink'] = "https://www.pine64.org/";
}
else if ($settings['Platform'] == "CHIP")
{
	$settings['Logo'] = "chip_logo.png";
	$settings['LogoLink'] = "http://www.getchip.com/";
}
else if ($settings['Platform'] == "Debian")
{
	$settings['Logo'] = "debian_logo.png";
	$settings['LogoLink'] = "https://www.debian.org/";
}
else if ($settings['Platform'] == "CHIP")
{
	$settings['Logo'] = "chip_logo.png";
	$settings['LogoLink'] = "http://www.getchip.com/";
}
else if ($settings['Platform'] == "Debian")
{
	$settings['Logo'] = "debian_logo.png";
	$settings['LogoLink'] = "https://www.debian.org/";
}
else if ($settings['Platform'] == "Linux")
{
	$settings['Logo'] = "tux_logo.png";
	$settings['LogoLink'] = "http://www.linux.com/";
}
else if ($settings['Platform'] == "FreeBSD")
{
	$settings['Logo'] = "freebsd_logo.png";
	$settings['LogoLink'] = "http://www.freebsd.org/";
}
else if ($settings['Platform'] == "qemu")
{
	$settings['Logo'] = "QEMU_Logo.png";
	$settings['LogoLink'] = "http://qemu.org/";
}
else
{
	$settings['Logo'] = "";
	$settings['LogoLink'] = "";
}

$fd = @fopen($settingsFile, "r");
if ( $fd )
{
	do
	{
		global $settings;
		global $fppMode, $volume, $settingsFile;
		global $mediaDirectory, $musicDirectory, $sequenceDirectory, $playlistDirectory;
		global $eventDirectory, $videoDirectory, $scriptDirectory, $logDirectory, $exim4Directory;
		global $pluginDirectory, $emailenable, $emailguser, $emailgpass, $emailfromtext, $emailtoemail;
		global $universeFile, $pixelnetFile, $scheduleFile, $bytesFile, $outputProcessorsFile;

		// Parse the file, assuming it exists
		$data = fgets($fd);
		$split = explode("=", $data);
		if ( count($split) < 2 )
		{
			if ( $debug )
				error_log("not long enough");
			continue;
		}

		$key   = trim($split[0]);
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

		switch ($key)
		{
			case "fppMode":
				$fppMode = $value;
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
	}
	while ( $data != NULL );

	fclose($fd);
}

$pageTitle = "FPP - " . $settings['HostName'];
if ($settings['HostName'] == "FPP")
	$pageTitle = "Falcon Player - FPP";

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

if (!isset($settings['restartFlag']))
	$settings['restartFlag'] = 0;

if (!isset($settings['rebootFlag']))
	$settings['rebootFlag'] = 0;

putenv("SCRIPTDIR=$scriptDirectory");
putenv("MEDIADIR=$mediaDirectory");
putenv("LOGDIR=$logDirectory");
putenv("SETTINGSFILE=$settingsFile");

if ($debug)
{
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

function GetDirSetting($dir)
{
	if ($dir == "Sequences")        { return GetSettingValue('sequenceDirectory'); }
	else if ($dir == "Music")       { return GetSettingValue('musicDirectory'); }
	else if ($dir == "Videos")      { return GetSettingValue('videoDirectory'); }
	else if ($dir == "Images")      { return GetSettingValue('imageDirectory'); }
	else if ($dir == "Effects")     { return GetSettingValue('effectDirectory'); }
	else if ($dir == "Scripts")     { return GetSettingValue('scriptDirectory'); }
	else if ($dir == "Logs")        { return GetSettingValue('logDirectory'); }
	else if ($dir == "Uploads")     { return GetSettingValue('uploadDirectory'); }
	else if ($dir == "Docs")        { return GetSettingValue('docsDirectory'); }
	else if ($dir == "Config")      { return GetSettingValue('configDirectory'); }

	return "";
}

// $skipJSsettings is only set in fppjson.php and fppxml.php
// to prevent this JavaScript from being printed
if (!isset($skipJSsettings)) {
?>
<script type="text/javascript">
	var settings = new Array();
<?
	foreach ($settings as $key => $value) {
        if (!is_array($value)) {
            printf("	settings['%s'] = \"%s\";\n", $key, $value);
        }
	}
?>

	var pageName = "<? echo str_ireplace('.php', '', basename($_SERVER['PHP_SELF'])) ?>";

  var helpPage = "<? echo basename($_SERVER['PHP_SELF']) ?>";
  if (pageName == "plugin")
  {
    var pluginPage = "<? echo preg_replace('/.*page=/', '', $_SERVER['REQUEST_URI']); ?>";
    var pluginBase = "<? echo preg_replace("/^\//", "", preg_replace('/page=.*/', '', $_SERVER['REQUEST_URI'])); ?>";
    helpPage = pluginBase + "nopage=1&page=help/" + pluginPage;
  }
  else
  {
    helpPage = "help/" + helpPage;
  }

</script>
<?
}
?>
