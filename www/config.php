<?php

//define('debug', true);
define('CONFIG_FILE', '/home/pi/media/settings');
define('SUDO', 'sudo');

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

// Set some defaults
$fppMode = "player";
$fppDir = dirname(dirname(__FILE__));
$settingsFile = CONFIG_FILE;
$pluginDirectory   = "/opt/fpp/plugins";
$mediaDirectory    = "/home/pi/media";
$docsDirectory     = $fppDir . "/docs";
$musicDirectory    = $mediaDirectory . "/music";
$sequenceDirectory = $mediaDirectory . "/sequences";
$playlistDirectory = $mediaDirectory . "/playlists";
$eventDirectory    = $mediaDirectory . "/events";
$videoDirectory    = $mediaDirectory . "/videos";
$effectDirectory   = $mediaDirectory . "/effects";
$scriptDirectory   = $mediaDirectory . "/scripts";
$logDirectory      = $mediaDirectory . "/logs";
$uploadDirectory   = $mediaDirectory . "/upload";
$universeFile      = $mediaDirectory . "/universes";
$pixelnetFile      = $mediaDirectory . "/pixelnetDMX";
$scheduleFile      = $mediaDirectory . "/schedule";
$bytesFile         = $mediaDirectory . "/bytesReceived";
$remapFile         = $mediaDirectory . "/channelremap";
$volume = 0;

if (defined('debug'))
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
	error_log("remaps: $remapFile");
	error_log("bytes: $bytesFile");
	error_log("volume: $volume");
}

$fd = @fopen(CONFIG_FILE, "r");
if ( ! $fd )
{
  error_log("Couldn't open config file " . CONFIG_FILE);
  return(1);
}

$settings['HostName'] = 'FPP';
$settings['Title'] = "Falcon Player - FPP";

$settings['Platform'] = FALSE;
if (file_exists("/etc/fpp/platform"))
	$settings['Platform'] = trim(file_get_contents("/etc/fpp/platform"));

if ($settings['Platform'] == FALSE)
{
	$settings['Platform'] = exec("uname -s");
}

if ($settings['Platform'] == "Raspberry Pi")
{
	$settings['Logo'] = "large_Raspberry_Pi_Logo_4.png";
	$settings['LogoLink'] = "http://raspberrypi.org/";
	$settings['Title'] = "Falcon (Pi) Player - FPP";
}
else if ($settings['Platform'] == "BeagleBone Black")
{
	$settings['Logo'] = "beagle_logo.png";
	$settings['LogoLink'] = "http://beagleboard.org/";
}
else if ($settings['Platform'] == "PogoPlug")
{
	$settings['Logo'] = "pogoplug_logo.png";
	$settings['LogoLink'] = "";
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
else
{
	$settings['Logo'] = "";
	$settings['LogoLink'] = "";
}

do
{
	global $settings;
	global $fppMode, $volume, $settingsFile;
	global $mediaDirectory, $musicDirectory, $sequenceDirectory, $playlistDirectory;
	global $eventDirectory, $videoDirectory, $scriptDirectory, $logDirectory;
	global $pluginDirectory;
	global $universeFile, $pixelnetFile, $scheduleFile, $bytesFile, $remapFile;

	// Parse the file, assuming it exists
	$data = fgets($fd);
	$split = explode("=", $data);
	if ( count($split) < 2 )
	{
//		error_log("not long enough");
		continue;
	}

	$key   = trim($split[0]);
	$value = trim($split[1]);

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
			$fppMode = trim($split[1]);
			break;
		case "volume":
			$volume = trim($split[1]);
			break;
		case "settingsFile":
			$settingsFile = trim($split[1]);
			break;
		case "mediaDirectory":
			$mediaDirectory = trim($split[1]) . "/";
			break;
		case "musicDirectory":
			$musicDirectory = trim($split[1]) . "/";
			break;
		case "eventDirectory":
			$eventDirectory = trim($split[1]) . "/";
			break;
		case "videoDirectory":
			$videoDirectory = trim($split[1]) . "/";
			break;
		case "sequenceDirectory":
			$sequenceDirectory = trim($split[1]) . "/";
			break;
		case "playlistDirectory":
			$playlistDirectory = trim($split[1]) . "/";
			break;
		case "effectDirectory":
			$effectDirectory = trim($split[1]) . "/";
			break;
		case "logDirectory":
			$logDirectory = trim($split[1]) . "/";
			break;
		case "uploadDirectory":
			$uploadDirectory = trim($split[1]) . "/";
			break;
		case "pluginDirectory":
			$pluginDirectory = trim($split[1]) . "/";
			break;
		case "scriptDirectory":
			$scriptDirectory = trim($split[1]) . "/";
			break;
		case "universeFile":
			$universeFile = trim($split[1]);
			break;
		case "pixelnetFile":
			$pixelnetFile = trim($split[1]);
			break;
		case "scheduleFile":
			$scheduleFile = trim($split[1]);
			break;
		case "bytesFile":
			$bytesFile = trim($split[1]);
			break;
		case "remapFile":
			$remapFile = trim($split[1]);
			break;
	}
}
while ( $data != NULL );

fclose($fd);

$pageTitle = "FPP - " . $settings['HostName'];
if ($settings['HostName'] == "FPP")
	$pageTitle = "Falcon (Pi) Player - FPP";

$settings['fppMode'] = $fppMode;
$settings['fppDir'] = $fppDir;
$settings['mediaDirectory'] = $mediaDirectory;
$settings['configDirectory'] = $mediaDirectory . "/config";
$settings['channelOutputsFile'] = $mediaDirectory . "/channeloutputs";
$settings['channelMemoryMapsFile'] = $mediaDirectory . "/channelmemorymaps";
$settings['scriptDirectory'] = $scriptDirectory;
$settings['sequenceDirectory'] = $sequenceDirectory;
$settings['musicDirectory'] = $musicDirectory;
$settings['videoDirectory'] = $videoDirectory;
$settings['effectDirectory'] = $effectDirectory;
$settings['logDirectory'] = $logDirectory;
$settings['uploadDirectory'] = $uploadDirectory;
$settings['docsDirectory'] = $docsDirectory;

putenv("SCRIPTDIR=$scriptDirectory");
putenv("MEDIADIR=$mediaDirectory");
putenv("LOGDIR=$logDirectory");
putenv("SETTINGSFILE=$settingsFile");

if (defined('debug'))
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
	error_log("remaps: $remapFile");
	error_log("bytes: $bytesFile");
	error_log("volume: $volume");
}

function GetDirSetting($dir)
{
	if ($dir == "Sequences")        { return GetSettingValue('sequenceDirectory'); }
	else if ($dir == "Music")       { return GetSettingValue('musicDirectory'); }
	else if ($dir == "Videos")      { return GetSettingValue('videoDirectory'); }
	else if ($dir == "Effects")     { return GetSettingValue('effectDirectory'); }
	else if ($dir == "Scripts")     { return GetSettingValue('scriptDirectory'); }
	else if ($dir == "Logs")        { return GetSettingValue('logDirectory'); }
	else if ($dir == "Uploads")     { return GetSettingValue('uploadDirectory'); }
	else if ($dir == "Docs")        { return GetSettingValue('docsDirectory'); }

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
		printf("	settings['%s'] = \"%s\";\n", $key, $value);
	}
?>

	var pageName = "<? echo str_ireplace('.php', '', basename($_SERVER['PHP_SELF'])) ?>";
</script>
<?
}
?>

