<?php

$fd = fopen(dirname(dirname(__FILE__))."/bin/config", "r");

do
{
	global $fppMode, $volume, $settingsFile;
	global $mediaDirectory, $musicDirectory, $sequenceDirectory, $playlistDirectory;
	global $universeFile, $pixelnetFile, $scheduleFile, $bytesFile;

	// Set some defaults
	$settingsFile = "/home/pi/media/settings";
	$mediaDirectory = "/home/pi/media";
	$musicDirectory = "/home/pi/media/music";
	$sequenceDirectory = "/home/pi/media/sequences";
	$playlistDirectory = "/home/pi/media/playlists";
	$universeFile = "/home/pi/media/universes";
	$pixelnetFile = "/home/pi/media/pixelnetDMX";
	$scheduleFile = "/home/pi/media/schedule";
	$bytesFile = "/home/pi/media/bytesReceived";


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
			print "FPP MODE";
			$fppMode = trim($split[1]);
			break;

		case "volume":
			print "VOLUME";
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
		case "sequenceDirectory":
			$sequenceDirectory = trim($split[1]) . "/";
			break;
		case "playlistDirectory":
			$playlistDirectory = trim($split[1]) . "/";
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
}
while ( $data != NULL );

fclose($fd);

?>
