<?php

//define('debug', true);
define('CONFIG_FILE', '/home/pi/media/settings');
define('SUDO', 'sudo');

// Set some defaults
$fppMode = "player";
$fppDir = dirname(dirname(__FILE__));
$settingsFile = CONFIG_FILE;
$mediaDirectory    = "/home/pi/media/";
$musicDirectory    = $mediaDirectory . "/music/";
$sequenceDirectory = $mediaDirectory . "/sequences/";
$playlistDirectory = $mediaDirectory . "/playlists/";
$eventDirectory    = $mediaDirectory . "/events/";
$videoDirectory    = $mediaDirectory . "/videos/";
$effectDirectory   = $mediaDirectory . "/effects/";
$scriptDirectory   = $mediaDirectory . "/scripts/";
$logDirectory      = $mediaDirectory . "/logs/";
$uploadDirectory   = $mediaDirectory . "/upload/";
$universeFile      = $mediaDirectory . "/universes";
$pixelnetFile      = $mediaDirectory . "/pixelnetDMX";
$scheduleFile      = $mediaDirectory . "/schedule";
$bytesFile         = $mediaDirectory . "/bytesReceived";
$volume = 0;

if (defined('debug'))
{
	error_log("DEFAULTS:");
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
	error_log("universe: $universeFile");
	error_log("pixelnet: $pixelnetFile");
	error_log("schedule: $scheduleFile");
	error_log("bytes: $bytesFile");
	error_log("volume: $volume");
}

$fd = @fopen(CONFIG_FILE, "r");
if ( ! $fd )
{
  error_log("Couldn't open config file " . CONFIG_FILE);
  return(1);
}

do
{
	global $fppMode, $volume, $settingsFile;
	global $mediaDirectory, $musicDirectory, $sequenceDirectory, $playlistDirectory;
	global $eventDirectroy, $videoDirectory;
	global $universeFile, $pixelnetFile, $scheduleFile, $bytesFile;

	// Parse the file, assuming it exists
	$data = fgets($fd);
	$split = explode("=", $data);
	if ( count($split) < 2 )
	{
//		error_log("not long enough");
		continue;
	}

	switch (trim($split[0]))
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
	}

	global $rds_enabled;
	$rds_enabled = false;
}
while ( $data != NULL );

fclose($fd);

if (defined('debug'))
{
	error_log("SET:");
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
	error_log("universe: $universeFile");
	error_log("pixelnet: $pixelnetFile");
	error_log("schedule: $scheduleFile");
	error_log("bytes: $bytesFile");
	error_log("volume: $volume");
}

?>
