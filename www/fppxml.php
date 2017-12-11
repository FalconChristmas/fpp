<?php

$skipJSsettings = 1;
require_once('common.php');

require_once('playlistentry.php');
require_once('universeentry.php');
require_once('scheduleentry.php');
require_once('pixelnetdmxentry.php');
require_once('commandsocket.php');

error_reporting(E_ALL);

// Commands defined here which return something other
// than XML need to return their own Content-type header.
$nonXML = Array(
	"getFile" => 1,
	"getGitOriginLog" => 1,
	"gitStatus" => 1,
	"getVideoInfo" => 1,
	"viewReleaseNotes" => 1,
	"viewRemoteScript" => 1
	);

$a = session_id();
if(empty($a))
{
	session_start();
}
$_SESSION['session_id'] = session_id();


$command_array = Array(
	"getMusicFiles" => 'GetMusicFiles',
	"getPlayLists" => 'GetPlaylists',
	"getPlayListSettings" => 'GetPlayListSettings',
	"getFiles" => 'GetFiles',
	"getZip" => 'GetZip',
	"getSequences" => 'GetSequenceFiles',
	"getPlayListEntries" => 'GetPlaylistEntries',
	"setPlayListFirstLast" => 'SetPlayListFirstLast',
	"addPlayList" => 'AddPlaylist',
	"getUniverseReceivedBytes" => 'GetUniverseReceivedBytes',
	"sort" => 'PlaylistEntryPositionChanged',
	"savePlaylist" => 'SavePlaylist',
	"deletePlaylist" => 'DeletePlaylist',
	"deleteEntry" => 'DeleteEntry',
	"deleteFile" => 'DeleteFile',
	"convertFile" => 'ConvertFile',
	"addPlaylistEntry" => 'AddPlayListEntry',
	"setUniverseCount" => 'SetUniverseCount',
	"getUniverses" => 'GetUniverses',
	"getPixelnetDMXoutputs" => 'GetPixelnetDMXoutputs',
	"deleteUniverse" => 'DeleteUniverse',
	"cloneUniverse" => 'CloneUniverse',
	"getSchedule" => 'GetSchedule',
	"addScheduleEntry" => 'AddScheduleEntry',
	"deleteScheduleEntry" => 'DeleteScheduleEntry',
	"viewReleaseNotes" => 'ViewReleaseNotes',
	"viewRemoteScript" => 'ViewRemoteScript',
	"installRemoteScript" => 'InstallRemoteScript',
	"moveFile" => 'MoveFile',
	"isFPPDrunning" => 'IsFPPDrunning',
	"getFPPstatus" => 'GetFPPstatus',
	"stopGracefully" => 'StopGracefully',
	"stopNow" => 'StopNow',
	"stopFPPD" => 'StopFPPD',
	"startFPPD" => 'StartFPPD',
	"restartFPPD" => 'RestartFPPD',
	"startPlaylist" => 'StartPlaylist',
	"rebootPi" => 'RebootPi',
	"shutdownPi" => 'ShutdownPi',
	"manualGitUpdate" => 'ManualGitUpdate',
	"changeGitBranch" => 'ChangeGitBranch',
	"upgradeFPPVersion" => 'UpgradeFPPVersion',
	"getGitOriginLog" => 'GetGitOriginLog',
	"gitStatus" => 'GitStatus',
	"resetGit" => 'ResetGit',
	"setAutoUpdate" => 'SetAutoUpdate',
	"setDeveloperMode" => 'SetDeveloperMode',
	"setVolume" => 'SetVolume',
	"setFPPDmode" => 'SetFPPDmode',
	"getVolume" => 'GetVolume',
	"getFPPDmode" => 'GetFPPDmode',
	"setE131interface" => 'SetE131interface',
	"playEffect" => 'PlayEffect',
	"stopEffect" => 'StopEffect',
	"stopEffectByName" => 'StopEffectByName',
	"deleteEffect" => 'DeleteEffect',
	"getRunningEffects" => 'GetRunningEffects',
	"triggerEvent" => 'TriggerEvent',
	"saveEvent" => 'SaveEvent',
	"deleteEvent" => 'DeleteEvent',
	"getFile" => 'GetFile',
	"getVideoInfo" => 'GetVideoInfo',
	"saveUSBDongle" => 'SaveUSBDongle',
	"getInterfaceInfo" => 'GetInterfaceInfo',
	"setPiLCDenabled" => 'SetPiLCDenabled',
    "setBBBTether" => 'SetBBBTether',
	"updatePlugin" => 'UpdatePlugin',
	"uninstallPlugin" => 'UninstallPlugin',
	"installPlugin" => 'InstallPlugin',
	"setupExtGPIO" => 'SetupExtGPIO',
	"extGPIO" => 'ExtGPIO'
);

if (isset($_GET['command']) && !isset($nonXML[$_GET['command']]))
	header('Content-type: text/xml');



if ( isset($_GET['command']) && !empty($_GET['command']) )
{
	global $debug;

	if ( array_key_exists($_GET['command'],$command_array) )
	{
		if ($debug)
			error_log("Calling ".$_GET['command']);
		call_user_func($command_array[$_GET['command']]);
	}
	return;
}
else if(!empty($_POST['command']) && $_POST['command'] == "saveUniverses")
{
	SetUniverses();
}
else if(!empty($_POST['command']) && $_POST['command'] == "saveSchedule")
{
	SaveSchedule();
}
else if(!empty($_POST['command']) && $_POST['command'] == "saveHardwareConfig")
{
	SaveHardwareConfig();
}

/////////////////////////////////////////////////////////////////////////////

function EchoStatusXML($status)
{
	$doc = new DomDocument('1.0');
	$root = $doc->createElement('Status');
	$root = $doc->appendChild($root);
	$value = $doc->createTextNode($status);
	$value = $root->appendChild($value);
	echo $doc->saveHTML();
}

/////////////////////////////////////////////////////////////////////////////

function RebootPi()
{
	global $SUDO;

	$status=exec($SUDO . " shutdown -r now");
	EchoStatusXML($status);
}

function ManualGitUpdate()
{
	global $fppDir, $SUDO;

	exec($SUDO . " $fppDir/scripts/fppd_stop");
	exec("$fppDir/scripts/git_pull");
	exec($SUDO . " $fppDir/scripts/fppd_start");

	EchoStatusXML("OK");
}

function UpgradeFPPVersion()
{
	$version = $_GET['version'];
	check($version, "version", __FUNCTION__);

	global $fppDir;
	exec("$fppDir/scripts/upgrade_FPP $version");

	EchoStatusXML("OK");
}

function ChangeGitBranch()
{
	$branch = $_GET['branch'];
	check($branch, "branch", __FUNCTION__);

	global $fppDir;
	exec("$fppDir/scripts/git_branch $branch");

	EchoStatusXML("OK");
}

function GetGitOriginLog()
{
	header('Content-type: text/plain');

	global $fppDir;
	$fullLog = "";
	exec("$fppDir/scripts/git_fetch", $log);
	exec("$fppDir/scripts/git_origin_log", $log);
	$fullLog .= implode("\n", $log);

	echo $fullLog;
}

function GitStatus()
{
	global $fppDir;

	$fullLog = "";
	exec("$fppDir/scripts/git_status", $log);
	$fullLog .= implode("\n", $log);

	echo $fullLog;
}

function ResetGit()
{
	global $fppDir;
	exec("$fppDir/scripts/git_reset");

	EchoStatusXML("OK");
}

function SetAutoUpdate()
{
	$enabled = $_GET['enabled'];
	check($enabled, "enabled", __FUNCTION__);

	global $mediaDirectory;
	if ($enabled)
		unlink("$mediaDirectory/.auto_update_disabled");
	else
		exec("touch $mediaDirectory/.auto_update_disabled");
}



function SetDeveloperMode()
{
	$enabled = $_GET['enabled'];
	check($enabled, "enabled", __FUNCTION__);

	global $mediaDirectory;
	if ($enabled)
		exec("touch $mediaDirectory/.developer_mode");
	else
		unlink("$mediaDirectory/.developer_mode");
}

function SetVolume()
{
	global $SUDO;

	$volume = $_GET['volume'];
	check($volume, "volume", __FUNCTION__);

	if ($volume == "NaN")
		$volume = 75;

	WriteSettingToFile("volume",$volume);

	$vol = intval ($volume);
	if ($vol>100)
		$vol = "100";

	$status=SendCommand('v,' . $vol . ',');

	$card = 0;
	if (isset($settings['AudioOutput']))
	{
		$card = $settings['AudioOutput'];
	}
	else
	{
		exec($SUDO . " grep card /root/.asoundrc | head -n 1 | cut -d' ' -f 2", $output, $return_val);
		if ( $return_val )
		{
			// Should we error here, or just move on?
			// Technically this should only fail on non-pi
			// and pre-0.3.0 images
			error_log("Error retrieving current sound card, using default of '0'!");
		}
		else
			$card = $output[0];

		WriteSettingToFile("AudioOutput", $card);
	}

	if ( $card == 0 )
		$vol = 50 + ($vol/2.0);

	$mixerDevice = "PCM";
	if (isset($settings['AudioMixerDevice']))
	{
		$mixerDevice = $settings['AudioMixerDevice'];
	}
	else
	{
		unset($output);
		exec($SUDO . " amixer -c $card scontrols | head -1 | cut -f2 -d\"'\"", $output, $return_val);
		$mixerDevice = $output[0];
		WriteSettingToFile("AudioMixerDevice", $mixerDevice);
	}

	// Why do we do this here and in fppd's settings.c
	$status=exec($SUDO . " amixer -c $card set $mixerDevice -- " . $vol . "%");

	EchoStatusXML($status);
}

function SetBBBTether()
{
    global $SUDO;
    
    $enabled = $_GET['enabled'];
    check($enabled, "enabled", __FUNCTION__);
    
    if ($enabled == "true")
    {
        $status = exec($SUDO . " sed -i -e \"s/TETHER_ENABLED=.*/TETHER_ENABLED=yes/\" /etc/default/bb-wl18xx");
    }
    else
    {
        $status = exec($SUDO . " sed -i -e \"s/TETHER_ENABLED=.*/TETHER_ENABLED=no/\" /etc/default/bb-wl18xx");
    }
    
    EchoStatusXML($status);
}

function SetPiLCDenabled()
{
	global $SUDO;

	$enabled = $_GET['enabled'];
	check($enabled, "enabled", __FUNCTION__);

	if ($enabled == "true")
	{
		$status = exec($SUDO . " " . dirname(dirname(__FILE__)) . "/scripts/lcd/fppLCD start");
	}
	else
	{
		$status = exec($SUDO . " " . dirname(dirname(__FILE__)) . "/scripts/lcd/fppLCD stop");
	}

	EchoStatusXML($status);
}

function SetFPPDmode()
{
	$mode_string['0'] = "unknown";
	$mode_string['1'] = "bridge";
	$mode_string['2'] = "player";
	$mode_string['6'] = "master";
	$mode_string['8'] = "remote";
	$mode = $_GET['mode'];
	check($mode, "mode", __FUNCTION__);
	WriteSettingToFile("fppMode",$mode_string["$mode"]);
	EchoStatusXML("true");
}

function SetE131interface()
{
	$iface = $_GET['iface'];
	check($iface, "iface", __FUNCTION__);
	WriteSettingToFile("E131interface",$iface);
	EchoStatusXML("true");
}

function GetVolume()
{
	$volume = ReadSettingFromFile("volume");
	if ($volume == "")
		$volume = 75;
	$doc = new DomDocument('1.0');
	$root = $doc->createElement('Volume');
	$root = $doc->appendChild($root);
	$value = $doc->createTextNode($volume);
	$value = $root->appendChild($value);
	echo $doc->saveHTML();
}

function GetFPPDmode()
{
	global $settings;
	$mode = $settings['fppMode'];
	$fppMode = 0;
	switch ($mode) {
		case "bridge": $fppMode = 1;
									 break;
		case "player": $fppMode = 2;
									 break;
		case "master": $fppMode = 6;
									 break;
		case "remote": $fppMode = 8;
									 break;
	}

	$doc = new DomDocument('1.0');
	$root = $doc->createElement('mode');
	$root = $doc->appendChild($root);
	$value = $doc->createTextNode($fppMode);
	$value = $root->appendChild($value);
	echo $doc->saveHTML();
}

function ShutdownPi()
{
	global $SUDO;

	$status=exec($SUDO . " shutdown -h now");
	EchoStatusXML($status);
}

function ViewReleaseNotes()
{
	$version = $_GET['version'];
	check($version, "version", __FUNCTION__);

	ini_set('user_agent','Mozilla/4.0 (compatible; MSIE 6.0)');
	$json = file_get_contents("https://api.github.com/repos/FalconChristmas/fpp/releases/tags/" . $version);

	$data = json_decode($json, true);

	echo $data["body"];
}

function ViewRemoteScript()
{
	$category = $_GET['category'];
	check($category, "category", __FUNCTION__);

	$filename = $_GET['filename'];
	check($filename, "filename", __FUNCTION__);

	$script = file_get_contents("https://raw.githubusercontent.com/FalconChristmas/fpp-scripts/master/" . $category . "/" . $filename);

	echo $script;
}

function InstallRemoteScript()
{
	global $fppDir, $SUDO;
	global $scriptDirectory;

	$category = $_GET['category'];
	check($category, "category", __FUNCTION__);

	$filename = $_GET['filename'];
	check($filename, "filename", __FUNCTION__);

	exec("$SUDO $fppDir/scripts/installScript $category $filename");

	EchoStatusXML('Success');
}

function MoveFile()
{
	global $mediaDirectory, $uploadDirectory, $musicDirectory, $sequenceDirectory, $videoDirectory, $effectDirectory, $scriptDirectory;

	$file = $_GET['file'];
	check($file, "file", __FUNCTION__);

	// Fix double quote uploading by simply moving the file first, if we find it with URL encoding
	if ( strstr($file, '"') )
	{
		if (!rename($uploadDirectory."/" . preg_replace('/"/', '%22', $file), $uploadDirectory."/" . $file))
		{
			error_log("Couldn't remove double quote from filename");
			exit(1);
		}	
	}
	if(file_exists($uploadDirectory."/" . $file))
	{
		if (preg_match("/\.(fseq)$/i", $file))
		{
			if ( !rename($uploadDirectory."/" . $file, $sequenceDirectory . '/' . $file) )
			{
				error_log("Couldn't move sequence file");
				exit(1);
			}
		}
		else if (preg_match("/\.(eseq)$/i", $file))
		{
			if ( !rename($uploadDirectory."/" . $file, $effectDirectory . '/' . $file) )
			{
				error_log("Couldn't move effect file");
				exit(1);
			}
		}
		else if (preg_match("/\.(mp4|mkv)$/i", $file))
		{
			if ( !rename($uploadDirectory."/" . $file, $videoDirectory . '/' . $file) )
			{
				error_log("Couldn't move video file");
				exit(1);
			}
		}
		else if (preg_match("/\.(sh|pl|pm|php|py)$/i", $file))
		{
			// Get rid of any DOS newlines
			$contents = file_get_contents($uploadDirectory."/".$file);
			$contents = str_replace("\r", "", $contents);
			file_put_contents($uploadDirectory."/".$file, $contents);

			if ( !rename($uploadDirectory."/" . $file, $scriptDirectory . '/' . $file) )
			{
				error_log("Couldn't move script file");
				exit(1);
			}
		}
		else if (preg_match("/\.(mp3|ogg)$/i", $file))
		{
			if ( !rename($uploadDirectory."/" . $file, $musicDirectory . '/' . $file) )
			{
				error_log("Couldn't move music file");
				exit(1);
			}
		}
	}
	else
	{
		error_log("Couldn't find file '" . $file . "' in upload directory");
		exit(1);
	}
	EchoStatusXML('Success');
}

function IsFPPDrunning()
{
	$status=exec("if ps cax | grep -q fppd; then echo \"true\"; else echo \"false\"; fi");
	if ($status == "false")
		$status=exec("if ps cax | grep -q git_pull; then echo \"updating\"; else echo \"false\"; fi");
	EchoStatusXML($status);
}

function StartPlaylist()
{
	$playlist = $_GET['playList'];
	$repeat = $_GET['repeat'];
	$playEntry = $_GET['playEntry'];

	check($playlist, "playlist", __FUNCTION__);
	check($repeat, "repeat", __FUNCTION__);
	check($playEntry, "playEntry", __FUNCTION__);

	if ($playEntry == "undefined")
		$playEntry = "0";

	if($repeat == "checked")
	{
		$status=SendCommand("p," . $playlist . "," . $playEntry . ",");
	}
	else
	{
		$status=SendCommand("P," . $playlist . "," . $playEntry . ",");
	}
	EchoStatusXML('true');
}

function PlayEffect()
{
	$effect = $_GET['effect'];
	check($effect, "effect", __FUNCTION__);
	$startChannel = $_GET['startChannel'];

	$loop = 0;
	if (isset($_GET['loop']))
		$loop = $_GET['loop'];

	check($startChannel, "startChannel", __FUNCTION__);
	$status = SendCommand("e," . $effect . "," . $startChannel . "," . $loop. ",");
	EchoStatusXML('Success');
}

function StopEffect()
{
	$id = $_GET['id'];
	check($id, "id", __FUNCTION__);
	$status = SendCommand("StopEffect," . $id . ",");
	EchoStatusXML('Success');
}

function StopEffectByName()
{
	$effect = $_GET['effect'];
	check($effect, "effect", __FUNCTION__);
	$status = SendCommand("StopEffectByName," . $effect . ",");
	EchoStatusXML('Success');
}

function GetRunningEffects()
{
	$status = SendCommand("GetRunningEffects");

	$result = "";
	$first = 1;
	$status = preg_replace('/\n/', '', $status);

	$doc = new DomDocument('1.0');
	// Running Effects
	$root = $doc->createElement('RunningEffects');
	$root = $doc->appendChild($root);
	foreach(preg_split('/;/', $status) as $line)
	{
		if ($first)
		{
			$first = 0;
			continue;
		}

		$info = preg_split('/,/', $line);

		$runningEffect = $doc->createElement('RunningEffect');
		$runningEffect = $root->appendChild($runningEffect);

		// Running Effect ID
		$id = $doc->createElement('ID');
		$id = $runningEffect->appendChild($id);
		$value = $doc->createTextNode($info[0]);
		$value = $id->appendChild($value);

		// Effect Name
		$name = $doc->createElement('Name');
		$name = $runningEffect->appendChild($name);
		$value = $doc->createTextNode($info[1]);
		$value = $name->appendChild($value);
	}

	echo $doc->saveHTML();
}

function GetExpandedEventID()
{
	$id = $_GET['id'];
	check($id, "id", __FUNCTION__);

	$majorID = preg_replace('/_.*/', '', $id);
	$minorID = preg_replace('/.*_/', '', $id);

	$filename = sprintf("%02d_%02d", $majorID, $minorID);

	return $filename;
}

function TriggerEvent()
{
	$id = GetExpandedEventID();

	$status = SendCommand("t," . $id . ",");

	EchoStatusXML($status);
}

function SaveEvent()
{
	global $eventDirectory;

	$id = $_GET['id'];
	check($id, "id", __FUNCTION__);

	$ids = preg_split('/_/', $id);

	if (count($ids) < 2)
		return;

	$id = GetExpandedEventID();
	$filename = $id . ".fevt";

	$name = $_GET['event'];
	check($name, "name", __FUNCTION__);

	if (isset($_GET['effect']) && $_GET['effect'] != "")
		$eseq = $_GET['effect'] . ".eseq";
	else
		$eseq = "";

	$f=fopen($eventDirectory . '/' . $filename,"w") or exit("Unable to open file! : " . $event);
	$eventDefinition = sprintf(
		"majorID=%d\n" .
		"minorID=%d\n" .
		"name='%s'\n" .
		"effect='%s'\n" .
		"startChannel=%s\n" .
		"script='%s'\n",
		$ids[0], $ids[1], $name,
		$eseq, $_GET['startChannel'], $_GET['script']);
	fwrite($f, $eventDefinition);
	fclose($f);

	EchoStatusXML('Success');
}

function DeleteEvent()
{
	global $eventDirectory;

	$filename = GetExpandedEventID() . ".fevt";

	unlink($eventDirectory . '/' . $filename);

	EchoStatusXML('Success');
}

function GetUniverseReceivedBytes()
{
	global $bytesFile;

	$status=SendCommand('r');
	$file = file($bytesFile);
	$doc = new DomDocument('1.0');
	if($file != FALSE)
	{
		$root = $doc->createElement('receivedBytes');
		$root = $doc->appendChild($root);  
		for($i=0;$i<count($file);$i++)
		{
			$receivedBytes = explode(",",$file[$i]);
			$receivedInfo = $doc->createElement('receivedInfo');
			$receivedInfo = $root->appendChild($receivedInfo); 
			// universe
			$universe = $doc->createElement('universe');
			$universe = $receivedInfo->appendChild($universe);
			$value = $doc->createTextNode($receivedBytes[0]);
			$value = $universe->appendChild($value);
			// startChannel
			$startChannel = $doc->createElement('startChannel');
			$startChannel = $receivedInfo->appendChild($startChannel);
			$value = $doc->createTextNode($receivedBytes[1]);
			$value = $startChannel->appendChild($value);
			// bytes received
			$bytesReceived = $doc->createElement('bytesReceived');
			$bytesReceived = $receivedInfo->appendChild($bytesReceived);
			$value = $doc->createTextNode($receivedBytes[2]);
			$value = $bytesReceived->appendChild($value);
			//Add it to receivedBytes 
		}
	}
	else
	{
		$root = $doc->createElement('Status');
		$root = $doc->appendChild($root);  
		$value = $doc->createTextNode('false');
		$value = $root->appendChild($value);
	}
	echo $doc->saveHTML();	
}

function StopGracefully()
{
	$status=SendCommand('S');
	EchoStatusXML('true');
}

function StopNow()
{
	$status=SendCommand('d');
	EchoStatusXML('true');
}

function StopFPPDNoStatus()
{
    global $SUDO;
    
    // Stop Playing
    SendCommand('d');
    
    // Shutdown
    SendCommand('q'); // Ignore return and just kill if 'q' doesn't work...
    // wait half a second for shutdown and outputs to close
    usleep(500000);
    // kill it if it's still running
    exec($SUDO . " " . dirname(dirname(__FILE__)) . "/scripts/fppd_stop");
}
function StopFPPD()
{
    StopFPPDNoStatus();
    EchoStatusXML('true');
}

function StartFPPD()
{
	global $settingsFile, $SUDO;

	$status=exec("if ps cax | grep -q fppd; then echo \"true\"; else echo \"false\"; fi");
	if($status == 'false')
	{
		$status=exec($SUDO . " " . dirname(dirname(__FILE__)) . "/scripts/fppd_start");
	}
	EchoStatusXML($status);
}

function RestartFPPD()
{
    StopFPPDNoStatus();
	StartFPPD();
}

function GetFPPstatus()
{
	$status = SendCommand('s');
	if($status == false || $status == 'false')
	{
		$doc = new DomDocument('1.0');
		$root = $doc->createElement('Status');
		$root = $doc->appendChild($root);

		$temp = $doc->createElement('fppStatus');
		$temp = $root->appendChild($temp);

		$value = $doc->createTextNode('-1');
		$value = $temp->appendChild($value);
		echo $doc->saveHTML();
		return;
	}

	$entry = explode(",",$status,14);
	$fppMode = $entry[0];
	if($fppMode == 8)
	{
		$fppStatus = $entry[1];
		$volume = $entry[2];
		$seqFilename = $entry[3];
		$mediaFilename = $entry[4];
		$secsElapsed = $entry[5];
		$secsRemaining = $entry[6];
		$fppCurrentDate = GetLocalTime();

		$doc = new DomDocument('1.0');
		$root = $doc->createElement('Status');
		$root = $doc->appendChild($root);

		//FPPD Mode
		$temp = $doc->createElement('fppMode');
		$temp = $root->appendChild($temp);
		$value = $doc->createTextNode($fppMode);
		$value = $temp->appendChild($value);

		//FPPD Status
		$temp = $doc->createElement('fppStatus');
		$temp = $root->appendChild($temp);
		$value = $doc->createTextNode($fppStatus);
		$value = $temp->appendChild($value);

		//FPPD Volume
		$temp = $doc->createElement('volume');
		$temp = $root->appendChild($temp);
		$value = $doc->createTextNode($volume);
		$value = $temp->appendChild($value);

		// seqFilename
		$temp = $doc->createElement('seqFilename');
		$temp = $root->appendChild($temp);
		$value = $doc->createTextNode($seqFilename);
		$value = $temp->appendChild($value);

		// mediaFilename
		$temp = $doc->createElement('mediaFilename');
		$temp = $root->appendChild($temp);
		$value = $doc->createTextNode($mediaFilename);
		$value = $temp->appendChild($value);

		// secsElapsed
		$temp = $doc->createElement('secsElapsed');
		$temp = $root->appendChild($temp);
		$value = $doc->createTextNode($secsElapsed);
		$value = $temp->appendChild($value);

		// secsRemaining
		$temp = $doc->createElement('secsRemaining');
		$temp = $root->appendChild($temp);
		$value = $doc->createTextNode($secsRemaining);
		$value = $temp->appendChild($value);

		//fppCurrentDate
		$temp = $doc->createElement('fppCurrentDate');
		$temp = $root->appendChild($temp);
		$value = $doc->createTextNode($fppCurrentDate);
		$value = $temp->appendChild($value);

		echo $doc->saveHTML();
		return;
	}
	else if($fppMode != 1)
	{
		$fppStatus = $entry[1];
		if($fppStatus == '0')
		{
			$volume = $entry[2];
			$nextPlaylist = $entry[3];
			$nextPlaylistStartTime = $entry[4];
			$fppCurrentDate = GetLocalTime();
			$repeatMode = 0;

			$doc = new DomDocument('1.0');
			$root = $doc->createElement('Status');
			$root = $doc->appendChild($root);

			//FPPD Mode
			$temp = $doc->createElement('fppMode');
			$temp = $root->appendChild($temp);
			$value = $doc->createTextNode($fppMode);
			$value = $temp->appendChild($value);
			//FPPD Status
			$temp = $doc->createElement('fppStatus');
			$temp = $root->appendChild($temp);
			$value = $doc->createTextNode($fppStatus);
			$value = $temp->appendChild($value);

			//FPPD Volume
			$temp = $doc->createElement('volume');
			$temp = $root->appendChild($temp);
			$value = $doc->createTextNode($volume);
			$value = $temp->appendChild($value);

			// nextPlaylist
			$temp = $doc->createElement('NextPlaylist');
			$temp = $root->appendChild($temp);
			$value = $doc->createTextNode($nextPlaylist);
			$value = $temp->appendChild($value);

			// nextPlaylistStartTime
			$temp = $doc->createElement('NextPlaylistStartTime');
			$temp = $root->appendChild($temp);
			$value = $doc->createTextNode($nextPlaylistStartTime);
			$value = $temp->appendChild($value);

			//fppCurrentDate
			$temp = $doc->createElement('fppCurrentDate');
			$temp = $root->appendChild($temp);
			$value = $doc->createTextNode($fppCurrentDate);
			$value = $temp->appendChild($value);

			//repeatMode
			$temp = $doc->createElement('repeatMode');
			$temp = $root->appendChild($temp);
			$value = $doc->createTextNode($repeatMode);
			$value = $temp->appendChild($value);

			echo $doc->saveHTML();
			return;
		}
		else
		{
			$fppdMode = $entry[1];
			$volume = $entry[2];
			$currentPlaylist = $entry[3];
			$currentPlaylistType = $entry[4];
			$currentSeqName = $entry[5];
			$currentSongName = $entry[6];
			$currentPlaylistIndex = $entry[7];
			$currentPlaylistCount = $entry[8];
			$secondsPlayed = $entry[9];
			$secondsRemaining = $entry[10];
			$nextPlaylist = $entry[11];
			$nextPlaylistStartTime = $entry[12];
			$fppCurrentDate = GetLocalTime();
			$repeatMode = $entry[13];


			$doc = new DomDocument('1.0');
			$root = $doc->createElement('Status');
			$root = $doc->appendChild($root);

			// fppdMode
			$temp = $doc->createElement('fppMode');
			$temp = $root->appendChild($temp);
			$value = $doc->createTextNode($fppMode);
			$value = $temp->appendChild($value);

			// fppdStatus
			$temp = $doc->createElement('fppStatus');
			$temp = $root->appendChild($temp);
			$value = $doc->createTextNode($fppStatus);
			$value = $temp->appendChild($value);


			// volume
			$temp = $doc->createElement('volume');
			$temp = $root->appendChild($temp);
			$value = $doc->createTextNode($volume);
			$value = $temp->appendChild($value);

			// currentPlaylist
			$path_parts = pathinfo($currentPlaylist);
			$temp = $doc->createElement('CurrentPlaylist');
			$temp = $root->appendChild($temp);
			$value = $doc->createTextNode($path_parts['filename']);
			$value = $temp->appendChild($value);
			// currentPlaylistType
			$temp = $doc->createElement('CurrentPlaylistType');
			$temp = $root->appendChild($temp);
			$value = $doc->createTextNode($currentPlaylistType);
			$value = $temp->appendChild($value);
			// currentSeqName
			$temp = $doc->createElement('CurrentSeqName');
			$temp = $root->appendChild($temp);
			$value = $doc->createTextNode($currentSeqName);
			$value = $temp->appendChild($value);
			// currentSongName
			$temp = $doc->createElement('CurrentSongName');
			$temp = $root->appendChild($temp);
			$value = $doc->createTextNode($currentSongName);
			$value = $temp->appendChild($value);
			// currentPlaylistIndex
			$temp = $doc->createElement('CurrentPlaylistIndex');
			$temp = $root->appendChild($temp);
			$value = $doc->createTextNode($currentPlaylistIndex);
			$value = $temp->appendChild($value);
			// currentPlaylistCount
			$temp = $doc->createElement('CurrentPlaylistCount');
			$temp = $root->appendChild($temp);
			$value = $doc->createTextNode($currentPlaylistCount);
			$value = $temp->appendChild($value);
			// secondsPlayed
			$temp = $doc->createElement('SecondsPlayed');
			$temp = $root->appendChild($temp);
			$value = $doc->createTextNode($secondsPlayed);
			$value = $temp->appendChild($value);

			// secondsRemaining
			$temp = $doc->createElement('SecondsRemaining');
			$temp = $root->appendChild($temp);
			$value = $doc->createTextNode($secondsRemaining);
			$value = $temp->appendChild($value);

			// nextPlaylist
			$temp = $doc->createElement('NextPlaylist');
			$temp = $root->appendChild($temp);
			$value = $doc->createTextNode($nextPlaylist);
			$value = $temp->appendChild($value);

			// nextPlaylistStartTime
			$temp = $doc->createElement('NextPlaylistStartTime');
			$temp = $root->appendChild($temp);
			$value = $doc->createTextNode($nextPlaylistStartTime);
			$value = $temp->appendChild($value);

			//fppCurrentDate
			$temp = $doc->createElement('fppCurrentDate');
			$temp = $root->appendChild($temp);
			$value = $doc->createTextNode($fppCurrentDate);
			$value = $temp->appendChild($value);

			//repeatMode
			$temp = $doc->createElement('repeatMode');
			$temp = $root->appendChild($temp);
			$value = $doc->createTextNode($repeatMode);
			$value = $temp->appendChild($value);


			echo $doc->saveHTML();
			return;
		}

	}
  else
  {
 			$doc = new DomDocument('1.0');
			$root = $doc->createElement('Status');
			$root = $doc->appendChild($root);

      $fppStatus = $entry[1];

  			//FPPD Mode
			$temp = $doc->createElement('fppMode');
			$temp = $root->appendChild($temp);
			$value = $doc->createTextNode($fppMode);
			$value = $temp->appendChild($value);
			//FPPD Status
			$temp = $doc->createElement('fppStatus');
			$temp = $root->appendChild($temp);
			$value = $doc->createTextNode($fppStatus);
			$value = $temp->appendChild($value);
			echo $doc->saveHTML();
  }
  

}

function GetLocalTime()
{
	return exec("date");
}

function DeleteScheduleEntry()
{
	$index = $_GET['index'];
	check($index, "index", __FUNCTION__);

	if($index < count($_SESSION['ScheduleEntries']) && count($_SESSION['ScheduleEntries']) > 0 )
	{
		unset($_SESSION['ScheduleEntries'][$index]);
		$_SESSION['ScheduleEntries'] = array_values($_SESSION['ScheduleEntries']);

	}
	EchoStatusXML('Success');
}

function AddScheduleEntry()
{
	$_SESSION['ScheduleEntries'][] = new ScheduleEntry(1,'',7,0,0,0,24,0,0,1,"2014-01-01","2099-12-31");
	EchoStatusXML('Success');
}

function SaveSchedule()
{
	for($i=0;$i<count($_SESSION['ScheduleEntries']);$i++)
	{
		if(isset($_POST['chkEnable'][$i]))
		{
			$_SESSION['ScheduleEntries'][$i]->enable = 1;
		}
		else
		{
			$_SESSION['ScheduleEntries'][$i]->enable = 0;
		}
		$_SESSION['ScheduleEntries'][$i]->playlist = 	$_POST['selPlaylist'][$i];
		$_SESSION['ScheduleEntries'][$i]->startDay = intval($_POST['selDay'][$i]);

		$dayMask = 0;
		if (isset($_POST['maskSunday'][$i]))
			$dayMask |= 0x4000;
		if (isset($_POST['maskMonday'][$i]))
			$dayMask |= 0x2000;
		if (isset($_POST['maskTuesday'][$i]))
			$dayMask |= 0x1000;
		if (isset($_POST['maskWednesday'][$i]))
			$dayMask |= 0x0800;
		if (isset($_POST['maskThursday'][$i]))
			$dayMask |= 0x0400;
		if (isset($_POST['maskFriday'][$i]))
			$dayMask |= 0x0200;
		if (isset($_POST['maskSaturday'][$i]))
			$dayMask |= 0x0100;

		$_SESSION['ScheduleEntries'][$i]->startDay |= $dayMask;

		$startTime = 		$entry = explode(":",$_POST['txtStartTime'][$i],3);
		$_SESSION['ScheduleEntries'][$i]->startHour = $startTime[0];
		$_SESSION['ScheduleEntries'][$i]->startMinute = $startTime[1];
		$_SESSION['ScheduleEntries'][$i]->startSecond = $startTime[2];

		$endTime = 		$entry = explode(":",$_POST['txtEndTime'][$i],3);
		$_SESSION['ScheduleEntries'][$i]->endHour = $endTime[0];
		$_SESSION['ScheduleEntries'][$i]->endMinute = $endTime[1];
		$_SESSION['ScheduleEntries'][$i]->endSecond = $endTime[2];

		if(isset($_POST['chkRepeat'][$i]))
		{
			$_SESSION['ScheduleEntries'][$i]->repeat = 1;
		}
		else
		{
			$_SESSION['ScheduleEntries'][$i]->repeat = 0;
		}

		$_SESSION['ScheduleEntries'][$i]->startDate =	$_POST['txtStartDate'][$i];
		$_SESSION['ScheduleEntries'][$i]->endDate   =	$_POST['txtEndDate'][$i];

		if (trim($_SESSION['ScheduleEntries'][$i]->startDate) == "")
			$_SESSION['ScheduleEntries'][$i]->startDate = "2014-01-01";
		if (trim($_SESSION['ScheduleEntries'][$i]->endDate) == "")
			$_SESSION['ScheduleEntries'][$i]->endDate = "2099-12-31";
	}

	SaveScheduleToFile();
	FPPDreloadSchedule();

	EchoStatusXML('Success');
}

function FPPDreloadSchedule()
{
	$status=SendCommand('R');
}

function SaveScheduleToFile()
{
	global $scheduleFile;

	$entries = "";
	$f=fopen($scheduleFile,"w") or exit("Unable to open file! : " . $scheduleFile);
	for($i=0;$i<count($_SESSION['ScheduleEntries']);$i++)
	{
			if($i==0)
			{
			$entries .= sprintf("%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,",
						$_SESSION['ScheduleEntries'][$i]->enable,
						$_SESSION['ScheduleEntries'][$i]->playlist,
						$_SESSION['ScheduleEntries'][$i]->startDay,
						$_SESSION['ScheduleEntries'][$i]->startHour,
						$_SESSION['ScheduleEntries'][$i]->startMinute,
						$_SESSION['ScheduleEntries'][$i]->startSecond,
						$_SESSION['ScheduleEntries'][$i]->endHour,
						$_SESSION['ScheduleEntries'][$i]->endMinute,
						$_SESSION['ScheduleEntries'][$i]->endSecond,
						$_SESSION['ScheduleEntries'][$i]->repeat,
						$_SESSION['ScheduleEntries'][$i]->startDate,
						$_SESSION['ScheduleEntries'][$i]->endDate
						);
			}
			else
			{
			$entries .= sprintf("\n%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,",
						$_SESSION['ScheduleEntries'][$i]->enable,
						$_SESSION['ScheduleEntries'][$i]->playlist,
						$_SESSION['ScheduleEntries'][$i]->startDay,
						$_SESSION['ScheduleEntries'][$i]->startHour,
						$_SESSION['ScheduleEntries'][$i]->startMinute,
						$_SESSION['ScheduleEntries'][$i]->startSecond,
						$_SESSION['ScheduleEntries'][$i]->endHour,
						$_SESSION['ScheduleEntries'][$i]->endMinute,
						$_SESSION['ScheduleEntries'][$i]->endSecond,
						$_SESSION['ScheduleEntries'][$i]->repeat,
						$_SESSION['ScheduleEntries'][$i]->startDate,
						$_SESSION['ScheduleEntries'][$i]->endDate
						);
			}

	}
	fwrite($f,$entries);
	fclose($f);
}

function LoadScheduleFile()
{
	global $scheduleFile;

	$_SESSION['ScheduleEntries']=NULL;

	$f=fopen($scheduleFile,"r");
	if($f == FALSE)
	{
		die();
	}

	while (!feof($f))
	{
		$line=fgets($f);
		$line = trim($line);
		$entry = explode(",",$line);
		if ( ! empty($entry) && count($entry) > 1 )
		{
			$enable = $entry[0];
			$playlist = $entry[1];
			$startDay = $entry[2];
			$startHour = $entry[3];
			$startMinute = $entry[4];
			$startSecond = $entry[5];
			$endHour = $entry[6];
			$endMinute = $entry[7];
			$endSecond = $entry[8];
			$repeat = $entry[9];
			$startDate = "2014-01-01";
			$endDate = "2099-12-31";

			if ((count($entry) >= 11) && $entry[10] != "")
				$startDate = $entry[10];

			if ((count($entry) >= 12) && $entry[11] != "")
				$endDate = $entry[11];

			$_SESSION['ScheduleEntries'][] = new ScheduleEntry($enable,$playlist,$startDay,$startHour,$startMinute,$startSecond,
				$endHour, $endMinute, $endSecond, $repeat,$startDate,$endDate);
		}
	}
	fclose($f);
}

function GetSchedule()
{
	$reload = $_GET['reload'];
	check($reload, "reload", __FUNCTION__);

	if($reload == "TRUE")
	{
		LoadScheduleFile();
	}

	$doc = new DomDocument('1.0');
	$root = $doc->createElement('ScheduleEntries');
	$root = $doc->appendChild($root);
	for($i=0;$i<count($_SESSION['ScheduleEntries']);$i++)
	{
		$ScheduleEntry = $doc->createElement('ScheduleEntry');
		$ScheduleEntry = $root->appendChild($ScheduleEntry);
		// enable
		$enable = $doc->createElement('enable');
		$enable = $ScheduleEntry->appendChild($enable);
		$value = $doc->createTextNode($_SESSION['ScheduleEntries'][$i]->enable);
		$value = $enable->appendChild($value);
		// day
		$day = $doc->createElement('day');
		$day = $ScheduleEntry->appendChild($day);
		$value = $doc->createTextNode($_SESSION['ScheduleEntries'][$i]->startDay);
		$value = $day->appendChild($value);
		// playlist
		$playlist = $doc->createElement('playlist');
		$playlist = $ScheduleEntry->appendChild($playlist);
		$value = $doc->createTextNode($_SESSION['ScheduleEntries'][$i]->playlist);
		$value = $playlist->appendChild($value);

		// startTime
		$startTime = $doc->createElement('startTime');
		$startTime = $ScheduleEntry->appendChild($startTime);
		$sStartTime = sprintf("%02s:%02s:%02s", $_SESSION['ScheduleEntries'][$i]->startHour,
												$_SESSION['ScheduleEntries'][$i]->startMinute,
												$_SESSION['ScheduleEntries'][$i]->startSecond);
		$value = $doc->createTextNode($sStartTime);
		$value = $startTime->appendChild($value);
		// endTime
		$endTime = $doc->createElement('endTime');
		$endTime = $ScheduleEntry->appendChild($endTime);
		$sEndTime = sprintf("%02s:%02s:%02s", $_SESSION['ScheduleEntries'][$i]->endHour,
											$_SESSION['ScheduleEntries'][$i]->endMinute,
											$_SESSION['ScheduleEntries'][$i]->endSecond);
		$value = $doc->createTextNode($sEndTime);
		$value = $endTime->appendChild($value);
		// repeat
		$repeat = $doc->createElement('repeat');
		$repeat = $ScheduleEntry->appendChild($repeat);
		$value = $doc->createTextNode($_SESSION['ScheduleEntries'][$i]->repeat);
		$value = $repeat->appendChild($value);

		// startDate
		$startDate = $doc->createElement('startDate');
		$startDate = $ScheduleEntry->appendChild($startDate);
		$value = $doc->createTextNode($_SESSION['ScheduleEntries'][$i]->startDate);
		$value = $startDate->appendChild($value);

		// endDate
		$endDate = $doc->createElement('endDate');
		$endDate = $ScheduleEntry->appendChild($endDate);
		$value = $doc->createTextNode($_SESSION['ScheduleEntries'][$i]->endDate);
		$value = $endDate->appendChild($value);

	}
	echo $doc->saveHTML();
}

function SaveHardwareConfig()
{
	if (!isset($_POST['model']))
	{
		EchoStatusXML('Failure, no model supplied');
		return;
	}

	$model = $_POST['model'];
	$firmware = $_POST['firmware'];

	if ($model == "F16V2-alpha")
	{
		SaveF16v2Alpha();
	}
	else if ($model == "FPDv1")
	{
		SaveFPDv1();
	}
	else
	{
		EchoStatusXML('Failure, unknown model: ' . $model);
		return;
	}
	EchoStatusXML('Success');
}

function SaveF16v2Alpha()
{
    global $settings;
		$outputCount = 16;
  
		$carr = array();
		for ($i = 0; $i < 1024; $i++)
		{
			$carr[$i] = 0x0;
		}

		$i = 0;

		// Header
		$carr[$i++] = 0x55;
		$carr[$i++] = 0x55;
		$carr[$i++] = 0x55;
		$carr[$i++] = 0x55;
		$carr[$i++] = 0x55;
		$carr[$i++] = 0xCD;

		// Some byte
		$carr[$i++] = 0x01;


		for ($o = 0; $o < $outputCount; $o++)
		{
			$nodeCount = $_POST['nodeCount'][$o];
			$carr[$i++] = intval($nodeCount % 256);
			$carr[$i++] = intval($nodeCount / 256);

			$startChannel = $_POST['startChannel'][$o] - 1; // 0-based values in config file
			$carr[$i++] = intval($startChannel % 256);
			$carr[$i++] = intval($startChannel / 256);

			// Node Type is set on groups of 4 ports
			$carr[$i++] = intval($_POST['nodeType'][intval($o / 4) * 4]);

			$carr[$i++] = intval($_POST['rgbOrder'][$o]);
			$carr[$i++] = intval($_POST['direction'][$o]);
			$carr[$i++] = intval($_POST['groupCount'][$o]);
			$carr[$i++] = intval($_POST['nullNodes'][$o]);
		}

		$f = fopen($settings['configDirectory'] . "/Falcon.F16V2-alpha", "wb");
		fwrite($f, implode(array_map("chr", $carr)), 1024);
		fclose($f);
 		SendCommand('w');
}

function SaveFPDv1()
{
  global $settings;
  $outputCount = 12;

	$carr = array();
	for ($i = 0; $i < 1024; $i++)
	{
		$carr[$i] = 0x0;
	}

	$i = 0;
	// Header
	$carr[$i++] = 0x55;
	$carr[$i++] = 0x55;
	$carr[$i++] = 0x55;
	$carr[$i++] = 0x55;
	$carr[$i++] = 0x55;
	$carr[$i++] = 0xCC;

	$_SESSION['PixelnetDMXentries']=NULL;
	for ($o = 0; $o < $outputCount; $o++)
	{
    // Active Output
 		if( isset($_POST['FPDchkActive'][$o]))
		{
      $active = 1;
      $carr[$i++] = 1;
		}
		else
		{
      $active = 0;
      $carr[$i++] = 0;
		}
    // Start Address
    $startAddress = intval($_POST['FPDtxtStartAddress'][$o]);
    $carr[$i++] = $startAddress%256;
    $carr[$i++] = $startAddress/256;
    // Type
    $type = intval($_POST['pixelnetDMXtype'][$o]);
    $carr[$i++] = $type;
    $_SESSION['PixelnetDMXentries'][] = new PixelnetDMXentry($active,$type,$startAddress);
  }
  $f = fopen($settings['configDirectory'] . "/Falcon.FPDV1", "wb");
	fwrite($f, implode(array_map("chr", $carr)), 1024);

	fclose($f);
	SendCommand('w');
}

function CloneUniverse()
{
	$index = $_GET['index'];
	$numberToClone = $_GET['numberToClone'];
	check($index, "index", __FUNCTION__);
	check($numberToClone, "numberToClone", __FUNCTION__);

	if($index < count($_SESSION['UniverseEntries']) && ($index + $numberToClone) < count($_SESSION['UniverseEntries']))
	{
			$universe = $_SESSION['UniverseEntries'][$index]->universe+1;
			$size = $_SESSION['UniverseEntries'][$index]->size;
			$startAddress = $_SESSION['UniverseEntries'][$index]->startAddress+$size;
			$type = $_SESSION['UniverseEntries'][$index]->type;
			$unicastAddress = $_SESSION['UniverseEntries'][$index]->unicastAddress;

			for($i=$index+1;$i<$index+1+$numberToClone;$i++,$universe++)
			{
				 	$_SESSION['UniverseEntries'][$i]->universe	= $universe;
				 	$_SESSION['UniverseEntries'][$i]->size	= $size;
				 	$_SESSION['UniverseEntries'][$i]->startAddress	= $startAddress;
				 	$_SESSION['UniverseEntries'][$i]->type	= $type;
					$_SESSION['UniverseEntries'][$i]->unicastAddress	= $unicastAddress;
					$startAddress += $size;
 			}
	}
	EchoStatusXML('Success');
}

function DeleteUniverse()
{
	$index = $_GET['index'];
	check($index, "index", __FUNCTION__);

	if($index < count($_SESSION['UniverseEntries']) && count($_SESSION['UniverseEntries']) > 0 )
	{
		unset($_SESSION['UniverseEntries'][$index]);
		$_SESSION['UniverseEntries'] = array_values($_SESSION['UniverseEntries']);

	}
	EchoStatusXML('Success');
}

function SetUniverses()
{
	for($i=0;$i<count($_SESSION['UniverseEntries']);$i++)
	{
		if( isset($_POST['chkActive'][$i]))
		{
			$_SESSION['UniverseEntries'][$i]->active = 1;
		}
		else
		{
			$_SESSION['UniverseEntries'][$i]->active = 0;
		}
		$_SESSION['UniverseEntries'][$i]->universe = 	intval($_POST['txtUniverse'][$i]);
		$_SESSION['UniverseEntries'][$i]->size = 	intval($_POST['txtSize'][$i]);
		$_SESSION['UniverseEntries'][$i]->startAddress = 	intval($_POST['txtStartAddress'][$i]);
		$_SESSION['UniverseEntries'][$i]->type = 	intval($_POST['universeType'][$i]);
		$_SESSION['UniverseEntries'][$i]->unicastAddress = 	trim($_POST['txtIP'][$i]);
	}

	SaveUniversesToFile();

	EchoStatusXML('Success');
}

function LoadUniverseFile()
{
	global $universeFile;

	$_SESSION['UniverseEntries']=NULL;

	$f=fopen($universeFile,"r");
	if($f == FALSE)
	{
		fclose($f);
		//No file exists add one universe and save to new file.
		$_SESSION['UniverseEntries'][] = new UniverseEntry(1,1,1,512,0,"",0);
		SaveUniversesToFile();
		return;
	}

	while (!feof($f))
	{
		$line=fgets($f);
		if ($line == "")
			continue;

		$entry = explode(",",$line,10);
		$active = $entry[0];
		$universe = $entry[1];
		$startAddress = $entry[2];
		$size = $entry[3];
		$type = $entry[4];
		$unicastAddress = $entry[5];
		$_SESSION['UniverseEntries'][] = new UniverseEntry($active,$universe,$startAddress,$size,$type,$unicastAddress,0);
	}
	fclose($f);
}

function LoadPixelnetDMXFile()
{
  global $settings;

	$_SESSION['PixelnetDMXentries']=NULL;

  $f = fopen($settings['configDirectory'] . "/Falcon.FPDV1", "rb");
	if($f == FALSE)
  {
  	fclose($f);
		//No file exists add one and save to new file.
		$address=1;
		for($i;$i<12;$i++)
		{
			$_SESSION['PixelnetDMXentries'][] = new PixelnetDMXentry(1,0,$address);
			$address+=4096;
		}
		return;
  }

	$s = fread($f, 1024);
	fclose($f);
	$sarr = unpack("C*", $s);

	$dataOffset = 7;

	$i = 0;
	for ($i = 0; $i < 12; $i++)
	{
		$outputOffset  = $dataOffset + (4 * $i);
		$active        = $sarr[$outputOffset + 0];
		$startAddress  = $sarr[$outputOffset + 1];
		$startAddress += $sarr[$outputOffset + 2] * 256;
		$type          = $sarr[$outputOffset + 3];
		$_SESSION['PixelnetDMXentries'][] = new PixelnetDMXentry($active,$type,$startAddress);
  }
}

function SaveUniversesToFile()
{
	global $universeFile;

	$entries = "";
	$f=fopen($universeFile,"w") or exit("Unable to open file! : " . $universeFile);
	for($i=0;$i<count($_SESSION['UniverseEntries']);$i++)
	{
		$entries .= sprintf("%s,%s,%s,%s,%s,%s,\n",
					$_SESSION['UniverseEntries'][$i]->active,
					$_SESSION['UniverseEntries'][$i]->universe,
					$_SESSION['UniverseEntries'][$i]->startAddress,
					$_SESSION['UniverseEntries'][$i]->size,
					$_SESSION['UniverseEntries'][$i]->type,
					$_SESSION['UniverseEntries'][$i]->unicastAddress);
	}
	fwrite($f,$entries);
	fclose($f);

	EchoStatusXML('Success');
}


function SavePixelnetDMXoutputsToFile()
{
	global $pixelnetFile;

	$universes = "";
	$f=fopen($pixelnetFile,"w") or exit("Unable to open file! : " . $pixelnetFile);
	for($i=0;$i<count($_SESSION['PixelnetDMXentries']);$i++)
	{
			if($i==0)
			{
			$entries .= sprintf("%d,%d,%d,",
						$_SESSION['PixelnetDMXentries'][$i]->active,
						$_SESSION['PixelnetDMXentries'][$i]->type,
						$_SESSION['PixelnetDMXentries'][$i]->startAddress);
			}
			else
			{
			$entries .= sprintf("\n%d,%d,%d,",
						$_SESSION['PixelnetDMXentries'][$i]->active,
						$_SESSION['PixelnetDMXentries'][$i]->type,
						$_SESSION['PixelnetDMXentries'][$i]->startAddress);
			}

	}
	fwrite($f,$entries);
	fclose($f);

	EchoStatusXML('Success');
}


function GetUniverses()
{
	$reload = $_GET['reload'];
	check($reload, "reload", __FUNCTION__);

	if($reload == "TRUE")
	{
		LoadUniverseFile();
	}

	$doc = new DomDocument('1.0');
	$root = $doc->createElement('UniverseEntries');
	$root = $doc->appendChild($root);
	for($i=0;$i<count($_SESSION['UniverseEntries']);$i++)
	{
		$UniverseEntry = $doc->createElement('UniverseEntry');
		$UniverseEntry = $root->appendChild($UniverseEntry);
		// active
		$active = $doc->createElement('active');
		$active = $UniverseEntry->appendChild($active);
		$value = $doc->createTextNode($_SESSION['UniverseEntries'][$i]->active);
		$value = $active->appendChild($value);
		// universe
		$universe = $doc->createElement('universe');
		$universe = $UniverseEntry->appendChild($universe);
		$value = $doc->createTextNode($_SESSION['UniverseEntries'][$i]->universe);
		$value = $universe->appendChild($value);
		// startAddress
		$startAddress = $doc->createElement('startAddress');
		$startAddress = $UniverseEntry->appendChild($startAddress);
		$value = $doc->createTextNode($_SESSION['UniverseEntries'][$i]->startAddress);
		$value = $startAddress->appendChild($value);
		// size
		$size = $doc->createElement('size');
		$size = $UniverseEntry->appendChild($size);
		$value = $doc->createTextNode($_SESSION['UniverseEntries'][$i]->size);
		$value = $size->appendChild($value);
		// type
		$type = $doc->createElement('type');
		$type = $UniverseEntry->appendChild($type);
		$value = $doc->createTextNode($_SESSION['UniverseEntries'][$i]->type);
		$value = $type->appendChild($value);
		// unicastAddress
		$unicastAddress = $doc->createElement('unicastAddress');
		$unicastAddress = $UniverseEntry->appendChild($unicastAddress);
		$value = $doc->createTextNode($_SESSION['UniverseEntries'][$i]->unicastAddress);
		$value = $unicastAddress->appendChild($value);

	}
	echo $doc->saveHTML();
}

function GetPixelnetDMXoutputs()
{
	$reload = $_GET['reload'];
	check($reload, "reload", __FUNCTION__);

	if($reload == "TRUE")
	{
		LoadPixelnetDMXFile();
	}

	$doc = new DomDocument('1.0');
	$root = $doc->createElement('PixelnetDMXentries');
	$root = $doc->appendChild($root);
	for($i=0;$i<count($_SESSION['PixelnetDMXentries']);$i++)
	{
		$PixelnetDMXentry = $doc->createElement('PixelnetDMXentry');
		$PixelnetDMXentry = $root->appendChild($PixelnetDMXentry);
		// active
		$active = $doc->createElement('active');
		$active = $PixelnetDMXentry->appendChild($active);
		$value = $doc->createTextNode($_SESSION['PixelnetDMXentries'][$i]->active);
		$value = $active->appendChild($value);
		// type
		$type = $doc->createElement('type');
		$type = $PixelnetDMXentry->appendChild($type);
		$value = $doc->createTextNode($_SESSION['PixelnetDMXentries'][$i]->type);
		$value = $type->appendChild($value);
		// startAddress
		$startAddress = $doc->createElement('startAddress');
		$startAddress = $PixelnetDMXentry->appendChild($startAddress);
		$value = $doc->createTextNode($_SESSION['PixelnetDMXentries'][$i]->startAddress);
		$value = $startAddress->appendChild($value);
	}
	echo $doc->saveHTML();
}

function SetUniverseCount()
{
	$count = $_GET['count'];
	check($count, "count", __FUNCTION__);

	if($count > 0 && $count <= 512)
	{

		$universeCount = count($_SESSION['UniverseEntries']);
		if($universeCount < $count)
		{
			$active = 1;
				$universe = 1;
				$startAddress = 1;
				$size = 512;
				$type = 0;	//Multicast
				$unicastAddress = "";
			if($universeCount == 0)
			{
				$universe = 1;
				$startAddress = 1;
				$size = 512;
				$type = 0;	//Multicast
				$unicastAddress = "";

			}
			else
			{
				$universe = $_SESSION['UniverseEntries'][$universeCount-1]->universe+1;
				$size = $_SESSION['UniverseEntries'][$universeCount-1]->size;
				$startAddress = $_SESSION['UniverseEntries'][$universeCount-1]->startAddress+$size;
				$type = $_SESSION['UniverseEntries'][$universeCount-1]->type;
				$unicastAddress = $_SESSION['UniverseEntries'][$universeCount-1]->unicastAddress;
			}

			for($i=$universeCount;$i<$count;$i++,$universe++)
			{
				$_SESSION['UniverseEntries'][] = new UniverseEntry($active,$universe,$startAddress,$size,$type,$unicastAddress,0);
				$startAddress += $size;
			}
		}
		else
		{
			for($i=$universeCount;$i>=$count;$i--)
			{
				unset($_SESSION['UniverseEntries'][$i]);
			}
		}
	}

	EchoStatusXML('Success');
}

function AddPlayListEntry()
{
	$type = $_GET['type'];
	$seqFile = $_GET['seqFile'];
	$songFile = $_GET['mediaFile'];
	$pause = $_GET['pause'];
	$eventName = $_GET['eventName'];
	$eventID = $_GET['eventID'];
	$pluginData = $_GET['pluginData'];
	check($type, "type", __FUNCTION__);
	check($seqFile, "seqFile", __FUNCTION__);
	check($songFile, "songFile", __FUNCTION__);
	check($pause, "pause", __FUNCTION__);
	check($eventName, "eventName", __FUNCTION__);
	check($eventID, "eventID", __FUNCTION__);
	check($pluginData, "pluginData", __FUNCTION__);
	$index = 0;

	$_SESSION['playListEntries'][] = new PlaylistEntry($type,$songFile,$seqFile,$pause,$eventName,$eventID,$pluginData,count($_SESSION['playListEntries']));
	EchoStatusXML($_GET['mediaFile']);
}

function GetPlaylists()
{
	global $playlistDirectory;

	$FirstList=TRUE;
	$doc = new DomDocument('1.0');
	$root = $doc->createElement('Playlists');
	$root = $doc->appendChild($root);

	if (!file_exists($playlistDirectory))
	{
		echo $doc->saveHTML();
		return;
	}

	foreach(scandir($playlistDirectory) as $pFile)
	{
		if ($pFile != "." && $pFile != "..")
		{
			$playList = $doc->createElement('Playlist');
			$playList = $root->appendChild($playList);
			$value = $doc->createTextNode(utf8_encode($pFile));
			$value = $playList->appendChild($value);
	//		if($showFirst == "true")
	//		{
				//LoadPlayListDetails($pFile);
	//		}
		}
	}
	echo $doc->saveHTML();
}

function GetMusicFiles()
{
	global $musicDirectory;

	$musicDirectory = $musicDirectory;
	$doc = new DomDocument('1.0');
	$root = $doc->createElement('Songs');
	$root = $doc->appendChild($root);

	foreach(scandir($musicDirectory) as $songFile)
	{
		if($songFile != '.' && $songFile != '..')
		{
			$songFileFullName = $musicDirectory . '/' . $songFile;

			$song = $doc->createElement('song');
			$song = $root->appendChild($song);

			$name = $doc->createElement('name');
			$name = $song->appendChild($name);

			$value = $doc->createTextNode(utf8_encode($songFile));
			$value = $name->appendChild($value);

			$time = $doc->createElement('time');
			$time = $song->appendChild($time);


			$value = $doc->createTextNode(date('m/d/y  h:i A', filemtime($songFileFullName)));
			$value = $time->appendChild($value);
		}
	}
	echo $doc->saveHTML();

}

function GetFileInfo(&$root, &$doc, $dirName, $fileName)
{
	$fileFullName = $dirName . '/' . $fileName;

	$file = $doc->createElement('File');
	$file = $root->appendChild($file);

	$name = $doc->createElement('Name');
	$name = $file->appendChild($name);
	$value = $doc->createTextNode(utf8_encode($fileName));
	$value = $name->appendChild($value);

	$time = $doc->createElement('Time');
	$time = $file->appendChild($time);
	$value = $doc->createTextNode(date('m/d/y  h:i A', filemtime($fileFullName)));
	$value = $time->appendChild($value);
}

function GetFiles()
{
	global $mediaDirectory;
	global $sequenceDirectory;
	global $musicDirectory;
	global $videoDirectory;
	global $effectDirectory;
	global $scriptDirectory;
	global $logDirectory;
	global $uploadDirectory;
	global $docsDirectory;

	$dirName = $_GET['dir'];
	check($dirName, "dirName", __FUNCTION__);
	if ($dirName == "Sequences")        { $dirName = $sequenceDirectory; }
	else if ($dirName == "Music")       { $dirName = $musicDirectory; }
	else if ($dirName == "Videos")      { $dirName = $videoDirectory; }
	else if ($dirName == "Effects")     { $dirName = $effectDirectory; }
	else if ($dirName == "Scripts")     { $dirName = $scriptDirectory; }
	else if ($dirName == "Logs")        { $dirName = $logDirectory; }
	else if ($dirName == "Uploads")     { $dirName = $uploadDirectory; }
	else if ($dirName == "Docs")        { $dirName = $docsDirectory; }
	else
		return;

	$doc = new DomDocument('1.0');
	$root = $doc->createElement('Files');
	$root = $doc->appendChild($root);

	foreach(scandir($dirName) as $fileName)
	{
		if($fileName != '.' && $fileName != '..')
		{
			GetFileInfo($root, $doc, $dirName, $fileName);
		}
	}

	if ($_GET['dir'] == "Logs")
	{
		if (file_exists("/var/log/messages"))
			GetFileInfo($root, $doc, "", "/var/log/messages");

		if (file_exists("/var/log/syslog"))
			GetFileInfo($root, $doc, "", "/var/log/syslog");
	}
	// Thanks: http://stackoverflow.com/questions/7272938/php-xmlreader-problem-with-htmlentities
	$trans = array_map('utf8_encode', array_flip(array_diff(get_html_translation_table(HTML_ENTITIES), get_html_translation_table(HTML_SPECIALCHARS))));
	echo strtr($doc->saveHTML(), $trans);
}

function GetSequenceFiles()
{
	global $sequenceDirectory;

	$doc = new DomDocument('1.0');
	$root = $doc->createElement('Sequences');
	$root = $doc->appendChild($root);

	foreach(scandir($sequenceDirectory) as $seqFile)
	{
		if($seqFile != '.' && $seqFile != '..')
		{
			$seqFileFullName = $sequenceDirectory . '/' . $seqFile;

			$sequence = $doc->createElement('Sequence');
			$sequence = $root->appendChild($sequence);

			$name = $doc->createElement('name');
			$name = $sequence->appendChild($name);

			$value = $doc->createTextNode(utf8_encode($seqFile));
			$value = $name->appendChild($value);

			$time = $doc->createElement('Time');
			$time = $sequence->appendChild($time);


			$value = $doc->createTextNode(date('m/d/y  h:i A', filemtime($seqFileFullName)));
			$value = $time->appendChild($value);
		}
	}
	echo $doc->saveHTML();
}

function AddPlaylist()
{
	global $playlistDirectory;

	$name = $_GET['pl'];
	check($name, "name", __FUNCTION__);

	$doc = new DomDocument('1.0');
	$name =str_replace(' ', '_', $name);
	$response = $doc->createElement('Response');
	$response = $doc->appendChild($response);
	$successAttribute = $doc->createAttribute('Success');

	if($name != "")
	{
		$successAttribute->value = 'true';
		$successAttribute = $response->appendChild($successAttribute);

		//$_SESSION['currentPlaylist']	= $pl;
		$filename = $playlistDirectory . '/' . $name;
		$file = fopen($filename, "w");
		fwrite($file, "");
		fclose($file);

		$playList = $doc->createElement('Playlist');
		$playList = $response->appendChild($playList);
		$value = $doc->createTextNode($name);
		$value = $playList->appendChild($value);
	}
	else
	{
		//$successAttribute->value = 'false';
	}
	$_SESSION['playListEntries'] = NULL;
	echo $doc->saveHTML();
}

function SetPlayListFirstLast()
{
	$first = $_GET['first'];
	$last = $_GET['last'];
	check($first, "first", __FUNCTION__);
	check($last, "last", __FUNCTION__);

	$_SESSION['playlist_first'] = $first;
	$_SESSION['playlist_last'] = $last;
	EchoStatusXML('Success');
}

function LoadPlayListDetails($file)
{
	global $playlistDirectory;
	global $eventDirectory;

	$playListEntries = NULL;
	$_SESSION['playListEntries']=NULL;

	$f=fopen($playlistDirectory . '/' . $file, "rx") or exit("Unable to open file! : " . $playlistDirectory . '/' . $file);
	$i=0;
	$line=fgets($f);
	if(strlen($line)) {
		$entry = explode(",",$line,50);
		$_SESSION['playlist_first']=$entry[0];
		$_SESSION['playlist_last']=$entry[1];
	}
	while (!feof($f))
	{
		$line=fgets($f);
		$entry = explode(",",$line,50);
		if ($entry[0] == 'v')
			$entry[0] = 'm';
		$type = $entry[0];
		if(strlen($line)==0)
			break;
		switch($entry[0])
		{
			case 'b':
				$seqFile = $entry[1];
				$songFile = $entry[2];
				$pause = 0;
				$index = $i;
				$eventName = "";
				$eventID = "";
				$pluginData = "";
				break;
			default:
				break;
			case 'm':
				$songFile = $entry[1];
				$seqFile = "";
				$pause = 0;
				$index = $i;
				$eventName = "";
				$eventID = "";
				$pluginData = "";
				break;
			case 's':
				$songFile = "";
				$seqFile = $entry[1];
				$pause = 0;
				$index = $i;
				$eventName = "";
				$eventID = "";
				$pluginData = "";
				break;
			case 'p':
				$songFile = "";
				$seqFile = "";
				$pause = $entry[1];
				$index = $i;
				$eventName = "";
				$eventID = "";
				$pluginData = "";
				break;
			case 'P':
				$songFile = "";
				$seqFile = "";
				$pause = 0;
				$index = $i;
				$eventName = "";
				$eventID = "";
				$pluginData = $entry[1];
				break;
			case 'e':
				$seqFile = "";
				$songFile = "";
				$pause = 0;
				$index = $i;
				$eventID = $entry[1];
				$pluginData = "";

				$eventFile = $eventDirectory . "/" . $eventID . ".fevt";
				if ( file_exists($eventFile)) {
					$eventInfo = parse_ini_file($eventFile);
					$eventName = $eventInfo['name'];
				} else {
					$eventName = "ERROR: Event undefined";
				}
				break;
		}
		$playListEntries[$i] = new PlaylistEntry($type,$songFile,$seqFile,$pause,$eventName,$eventID,$pluginData,$index);
		$i++;
	}
	fclose($f);
	$_SESSION['playListEntries'] = $playListEntries;
//	Print_r($_SESSION['playListEntries']);
}

function GetPlayListSettings()
{
	$file = $_GET['pl'];
	check($file, "file", __FUNCTION__);

	$doc = new DomDocument('1.0');
	// Playlist Entries
	$root = $doc->createElement('playlist_settings');
	$root = $doc->appendChild($root);
	// First setting
	$first = $doc->createElement('playlist_first');
	$first = $root->appendChild($first);
	$value = $doc->createTextNode($_SESSION['playlist_first']);
	$value = $first->appendChild($value);
	// Last setting
	$last = $doc->createElement('playlist_last');
	$last = $root->appendChild($last);
	$value = $doc->createTextNode($_SESSION['playlist_last']);
	$value = $last->appendChild($value);

	echo $doc->saveHTML();
}

function GetPlaylistEntries()
{
	$file = $_GET['pl'];
	$reloadFile = $_GET['reload'];
	check($file, "file", __FUNCTION__);
	check($reloadFile, "reloadFile", __FUNCTION__);

	$_SESSION['currentPlaylist'] = $file;
	if($reloadFile=='true')
	{
		LoadPlayListDetails($file);
	}
	$doc = new DomDocument('1.0');
	// Playlist Entries
	$root = $doc->createElement('PlaylistEntries');
	$root = $doc->appendChild($root);
//	Print_r($_SESSION['playListEntries']);
	for($i=0;$i<count($_SESSION['playListEntries']);$i++)
	{
		$playListEntry = $doc->createElement('playListEntry');
		$playListEntry = $root->appendChild($playListEntry);
		// type
		$type = $doc->createElement('type');
		$type = $playListEntry->appendChild($type);
		$value = $doc->createTextNode($_SESSION['playListEntries'][$i]->type);
		$value = $type->appendChild($value);
		// seqFile
		$seqFile = $doc->createElement('seqFile');
		$seqFile = $playListEntry->appendChild($seqFile);
		$value = $doc->createTextNode($_SESSION['playListEntries'][$i]->seqFile);
		$value = $seqFile->appendChild($value);
		// songFile
		$songFile = $doc->createElement('songFile');
		$songFile = $playListEntry->appendChild($songFile);
		$value = $doc->createTextNode($_SESSION['playListEntries'][$i]->songFile);
		$value = $songFile->appendChild($value);
		// pause
		$pause = $doc->createElement('pause');
		$pause = $playListEntry->appendChild($pause);
		$value = $doc->createTextNode($_SESSION['playListEntries'][$i]->pause);
		$value = $pause->appendChild($value);
		// index
		$index = $doc->createElement('index');
		$index = $playListEntry->appendChild($index);
		$value = $doc->createTextNode($_SESSION['playListEntries'][$i]->index);
		$value = $index->appendChild($value);
		// eventName
		$eventName = $doc->createElement('eventName');
		$eventName = $playListEntry->appendChild($eventName);
		$value = $doc->createTextNode($_SESSION['playListEntries'][$i]->eventName);
		$value = $eventName->appendChild($value);
		// eventID
		$eventID = $doc->createElement('eventID');
		$eventID = $playListEntry->appendChild($eventID);
		$value = $doc->createTextNode($_SESSION['playListEntries'][$i]->eventID);
		$value = $eventID->appendChild($value);
		// plugin data
		$pluginData = $doc->createElement('pluginData');
		$pluginData = $playListEntry->appendChild($pluginData);
		$value = $doc->createTextNode($_SESSION['playListEntries'][$i]->pluginData);
		$value = $pluginData->appendChild($value);
	}
	echo $doc->saveHTML();
}

function PlaylistEntryPositionChanged()
{
	$newIndex = $_GET['newIndex'];
	$oldIndex = $_GET['oldIndex'];
	check($newIndex, "newIndex", __FUNCTION__);
	check($oldIndex, "oldIndex", __FUNCTION__);

	if(count($_SESSION['playListEntries']) > $oldIndex && count($_SESSION['playListEntries']) > $newIndex)
	{
		$_SESSION['playListEntries'][$oldIndex]->index = $newIndex;
		if($newIndex < 	$oldIndex)
		{
			for($index=$newIndex;$index<$oldIndex;$index++)
			{
				$_SESSION['playListEntries'][$index]->index++;
			}
		}
		if ($oldIndex < $newIndex)
		{
			for($index=$oldIndex+1;$index<=$newIndex;$index++)
			{
				$_SESSION['playListEntries'][$index]->index--;
			}
		}
		usort($_SESSION['playListEntries'],cmp_index);

	}
}


function SavePlaylist()
{
	global $playlistDirectory;

	$name = $_GET['name'];
	$first = $_GET['first'];
	$last = $_GET['last'];
	check($name, "name", __FUNCTION__);
	check($first, "first", __FUNCTION__);
	check($last, "last", __FUNCTION__);

	$f=fopen($playlistDirectory . '/' . $name,"w") or exit("Unable to open file! : " . $playlistDirectory . '/' . $name);
	$entries = sprintf("%s,%s,\n",$first,$last);
	for($i=0;$i<count($_SESSION['playListEntries']);$i++)
	{
		if($_SESSION['playListEntries'][$i]->type == 'b')
		{
			$entries .= sprintf("%s,%s,%s,\n",$_SESSION['playListEntries'][$i]->type,$_SESSION['playListEntries'][$i]->seqFile,
								$_SESSION['playListEntries'][$i]->songFile);
		}
		else if($_SESSION['playListEntries'][$i]->type == 's')
		{
			$entries .= sprintf("%s,%s,\n",$_SESSION['playListEntries'][$i]->type,$_SESSION['playListEntries'][$i]->seqFile);
		}
		else if($_SESSION['playListEntries'][$i]->type == 'm')
		{
			$entries .= sprintf("%s,%s,\n",$_SESSION['playListEntries'][$i]->type,$_SESSION['playListEntries'][$i]->songFile);
		}
		else if($_SESSION['playListEntries'][$i]->type == 'p')
		{
			$entries .= sprintf("%s,%s,\n",$_SESSION['playListEntries'][$i]->type,$_SESSION['playListEntries'][$i]->pause);
		}
		else if($_SESSION['playListEntries'][$i]->type == 'e')
		{
			$entries .= sprintf("%s,%s,\n",$_SESSION['playListEntries'][$i]->type,
				$_SESSION['playListEntries'][$i]->eventID);
		}
		else if($_SESSION['playListEntries'][$i]->type == 'P')
		{
			$entries .= sprintf("%s,%s,\n",$_SESSION['playListEntries'][$i]->type,$_SESSION['playListEntries'][$i]->pluginData);
		}
	}
	fwrite($f,$entries);
	fclose($f);

	EchoStatusXML('Success');

	if($name != $_SESSION['currentPlaylist'])
	{
		 unlink($playlistDirectory . '/' . $_SESSION['currentPlaylist']);
		 $_SESSION['currentPlaylist'] = $name;
	}
}

function DeletePlaylist()
{
	global $playlistDirectory;

	$name = $_GET['name'];
	check($name, "name", __FUNCTION__);

	unlink($playlistDirectory . '/' . $name);
	EchoStatusXML('Success');
}

function DeleteEntry()
{
	$index = $_GET['index'];
	check($index, "index", __FUNCTION__);

	$doc = new DomDocument('1.0');
	$root = $doc->createElement('Status');
	$root = $doc->appendChild($root);
	// Delete from array.
	if($index < count($_SESSION['playListEntries']))
	{
		unset($_SESSION['playListEntries'][$index]);
		$_SESSION['playListEntries'] = array_values($_SESSION['playListEntries']);
		for($i=0;$i<count($_SESSION['playListEntries']);$i++)
		{
			$_SESSION['playListEntries'][$i]->index = $i;
		}
		$value = $doc->createTextNode(("Success"));
	}
	else
	{
		$value = $doc->createTextNode("Failed. Index out of Range.");
	}

	$value = $root->appendChild($value);
	echo $doc->saveHTML();

}



function cmp_index($a, $b)
{
	if ($a->index == $b->index) {
		return 0;
	}
	return ($a->index < $b->index) ? -1 : 1;
}

function DeleteFile()
{
	$filename = $_GET['filename'];
	check($filename, "filename", __FUNCTION__);

	$dir = $_GET['dir'];
	check($dir, "dir", __FUNCTION__);

	$dir = GetDirSetting($dir);

	if ($dir == "")
		return;

	if (substr($filename, 0, 1) != "/")
	{
		unlink($dir . '/' . $filename);
		EchoStatusXML('Success');
	}
	else
		EchoStatusXML('Failure');
}
    
function universe_cmp($a, $b)
{
    if ($a->startAddress == $b->startAddress) {
        return 0;
    }
    return ($a->startAddress < $b->startAddress) ? -1 : 1;
}

function ConvertFile()
{
	global $uploadDirectory, $sequenceDirectory, $effectDirectory, $SUDO, $debug;

	$file = $_GET['filename'];
	$convertTo = $_GET['convertTo'];

	check($file, "file", __FUNCTION__);
	check($convertTo, "convertTo", __FUNCTION__);

	$doc = new DomDocument('1.0');
	$response= $doc->createElement('Response');
	$doc->appendChild($response);
	$status = $doc->createElement('Status');
	$response->appendChild($status);

	if (preg_match("/\.(vix|xseq|lms|las|gled|seq|hlsidata)$/i", $file))
	{
        LoadUniverseFile();
        
        usort($_SESSION['UniverseEntries'], "universe_cmp");
        $universeString = "-";
        $last = 1;
        for($i=1;$i<count($_SESSION['UniverseEntries']);$i++)
        {
            if (strlen($universeString) > 1) {
                $universeString .= ",";
            }
            $universeString .= ($_SESSION['UniverseEntries'][$i]->startAddress - $last);
            $last = $_SESSION['UniverseEntries'][$i]->startAddress;
        }
        if (strlen($universeString) > 1) {
            $universeString .= ",";
        }
        $universeString .= $_SESSION['UniverseEntries'][count($_SESSION['UniverseEntries']) - 1]->size;
        
        
		// The file gets placed where we run fppconvert from, which is
		// typically the www-root.  Instead, let's go where we want it
		// and know we have the space.
		chdir($uploadDirectory);

		exec($SUDO . " " . dirname(dirname(__FILE__)) . '/bin/fppconvert ' . $universeString . ' "' . $uploadDirectory."/".$file.'" 2>&1 | grep -i -e error -e alloc', $output, $return_val);

		$output_string = "";
		foreach ($output as $line)
		{
			$output_string .= $line . '<br />';
			if ($debug)
				error_log("*** FPPCONVERT OUTPUT: $line ***");
		}

		if (strpos($output_string, "rror") || strpos($output_string, "alloc"))
		{
			$fail = $doc->createTextNode('Failure');
			$status->appendChild($fail);

			$error = $doc->createElement('Error');
			$response->appendChild($error);

			if ( strpos($output_string, "alloc") )
				$value = $doc->createTextNode("Out of memory!<br />This file is be too big to convert on the Pi.");
			else
				$value = $doc->createTextNode($output_string);
			$error->appendChild($value);

			echo $doc->saveHTML();
			exit(1);
		}

		$file_strip = substr($file, 0, strrpos($file, "."));
		if ($convertTo == "sequence")
		{
			if (!rename($file_strip.".fseq", $sequenceDirectory . '/' . $file_strip.".fseq"))
			{
				error_log("Couldn't copy sequence file");
				$fail = $doc->createTextNode('Failure');
				$status->appendChild($fail);

				$error = $doc->createElement('Error');
				$response->appendChild($error);

				$value = $doc->createTextNode("Couldn't move sequence file");
				$error->appendChild($value);

				echo $doc->saveHTML();
				exit(1);
			}
		}
		elseif ($convertTo == "effect")
		{
			if (!rename($file_strip.".fseq", $effectDirectory . '/' . $file_strip.".eseq"))
			{
				error_log("Couldn't move effect file");
				$fail = $doc->createTextNode('Failure');
				$status->appendChild($fail);

				$error = $doc->createElement('Error');
				$response->appendChild($error);

				$value = $doc->createTextNode("Couldn't move effect file");
				$error->appendChild($value);

				echo $doc->saveHTML();
				exit(1);
			}
		}
		else
		{
			unlink($file_strip.".fseq");
			$fail = $doc->createTextNode('Failure');
			$status->appendChild($fail);

			$error = $doc->createElement('Error');
			$response->appendChild($error);

			$value = $doc->createTextNode("Invalid conversion type");
			$error->appendChild($value);

			echo $doc->saveHTML();
			exit(1);
		}
	}

	$success = $doc->createTextNode('Success');
	$status->appendChild($success);
	echo $doc->saveHTML();
}

function GetVideoInfo()
{
	$filename = $_GET['filename'];
	check($filename, "filename", __FUNCTION__);

	header('Content-type: text/plain');

	global $settings;
	$videoInfo = "";
	exec("omxplayer -i '" . $settings['videoDirectory'] . "/" . $filename . "' 2>&1", $info);
	$videoInfo .= implode("\n", $info);

	echo $videoInfo;
}

function GetFile()
{
	$filename = $_GET['filename'];
	check($filename, "filename", __FUNCTION__);

	$dir = $_GET['dir'];
	check($dir, "dir", __FUNCTION__);

	$dir = GetDirSetting($dir);

	if ($dir == "")
		return;

	if (isset($_GET['play']))
	{
		if (preg_match('/mp3$/i', $filename))
			header('Content-type: audio/mp3');
		else if (preg_match('/ogg$/i', $filename))
			header('Content-type: audio/ogg');
		else if (preg_match('/mp4$/i', $filename))
			header('Content-type: video/mp4');
	}
	else
	{
		header('Content-type: application/binary');
		header('Content-disposition: attachment;filename="' . $filename . '"');
	}

	if (($_GET['dir'] == "Logs") &&
		(substr($filename, 0, 9) == "/var/log/")) {
		$dir = "/var/log";
		$filename = basename($filename);
	}

	ob_clean();
	flush();
	readfile($dir . '/' . $filename);
}

function GetZip()
{
	global $SUDO;
	global $settings;
	global $logDirectory;
	global $mediaDirectory;

	// Re-format the file name
	$filename = tempnam("/tmp", "FPP_Logs");

	// Gather troubleshooting commands output
	$cmd = "php " . $settings['fppDir'] . "/www/troubleshootingText.php > " . $settings['mediaDirectory'] . "/logs/troubleshootingCommands.log";
	exec($cmd, $output, $return_val);
	unset($output);

	// Create the object
	$zip = new ZipArchive();
	if ($zip->open($filename, ZIPARCHIVE::CREATE) !== TRUE) {
		exit("Cannot open '$filename'\n");
	}
	foreach(scandir($logDirectory) as $file) {
		if ( $file == "." || $file == ".." ) {
			continue;
		}
		$zip->addFile($logDirectory.'/'.$file, "Logs/".$file);
	}

	if ( is_readable("/var/log/messages") )
		$zip->addFile("/var/log/messages", "Logs/messages.log");
	if ( is_readable("/var/log/syslog") )
		$zip->addFile("/var/log/syslog", "Logs/syslog.log");

	$files = array(
		"channelmemorymaps",
		"channeloutputs",
		"config/channeloutputs.json",
		"pixelnetDMX",
		"schedule",
		"settings",
		"universes"
		);
	foreach($files as $file) {
		if (file_exists("$mediaDirectory/$file"))
			$zip->addFromString("Config/$file", ScrubFile("$mediaDirectory/$file"));
	}

	// /root/.asoundrc is only readable by root, should use /etc/ version
	exec($SUDO . " cat /root/.asoundrc", $output, $return_val);
	if ( $return_val != 0 ) {
		error_log("Unable to read /root/.asoundrc");
	}
	else {
		$zip->addFromString("Config/asoundrc", implode("\n", $output)."\n");
	}
	unset($output);

	exec("cat /proc/asound/cards", $output, $return_val);
	if ( $return_val != 0 ) {
		error_log("Unable to read alsa cards");
	}
	else {
		$zip->addFromString("Logs/asound/cards", implode("\n", $output)."\n");
	}
	unset($output);

	exec("/usr/bin/git --work-tree=".dirname(dirname(__FILE__))."/ status", $output, $return_val);
	if ( $return_val != 0 ) {
		error_log("Unable to get a git status for logs");
	}
	else {
		$zip->addFromString("Logs/git_status.txt", implode("\n", $output)."\n");
	}
	unset($output);

	exec("/usr/bin/git --work-tree=".dirname(dirname(__FILE__))."/ diff", $output, $return_val);
	if ( $return_val != 0 ) {
		error_log("Unable to get a git diff for logs");
	}
	else {
		$zip->addFromString("Logs/fpp_git.diff", implode("\n", $output)."\n");
	}
	unset($output);

	$zip->close();

	$timestamp = gmdate('Ymd.Hi');

	header('Content-type: application/zip');
	header('Content-disposition: attachment;filename=FPP_Logs_' . $timestamp . '.zip');
	ob_clean();
	flush();
	readfile($filename);
	unlink($filename);
	exit();
}


function SaveUSBDongle()
{
	$usbDonglePort = $_GET['port'];
	check($usbDonglePort, "usbDonglePort", __FUNCTION__);

	$usbDongleType = $_GET['type'];
	check($usbDongleType, "usbDongleType", __FUNCTION__);

	$usbDongleBaud = $_GET['baud'];
	check($usbDongleBaud, "usbDongleBaud", __FUNCTION__);

	WriteSettingToFile("USBDonglePort", $usbDonglePort);
	WriteSettingToFile("USBDongleType", $usbDongleType);
	WriteSettingToFile("USBDongleBaud", $usbDongleBaud);
}

function GetInterfaceInfo()
{
	$interface = $_GET['interface'];
	check($interface, "interface", __FUNCTION__);

  $readinterface = shell_exec("./readInterface.awk /etc/network/interfaces device=" . $interface);
  $parseethernet = explode(",", $readinterface);
  if (trim($parseethernet[0], "\"\n\r") == "dhcp" )
  {
    $ethMode = "dhcp";
    // Gateway
    $iproute = shell_exec('/sbin/ip route');
    preg_match('/via ([\d\.]+)/', $iproute, $result);
    $eth_gateway = $result[1];

    // IP Address
    $ifconfig = shell_exec("/sbin/ifconfig " . $interface);
    $success = preg_match('/addr:([\d\.]+)/', $ifconfig, $result);
    $eth_IP = $result[1];
    if ($success == 1) 
    {
      // Netmask
      preg_match('/Mask:([\d\.]+)/', $ifconfig, $result);
      $eth_netmask = $result[1];
      // Broadcast
//      preg_match('/Bcast:([\d\.]+)/', $ifconfig, $result);
//      $eth_broadcast = $result[1];
    }
  }
  
  // Static get info from /etc/network/interfaces
  else
  {
    $ethMode = "static";
    $eth_IP = $parseethernet[1];
    $eth_netmask = $parseethernet[2];
    $eth_gateway = $parseethernet[3];
//    $eth_network = $parseethernet[4];
//    $eth_broadcast = $parseethernet[5];
  }

  // DNS Server
  $ipdns = shell_exec('/bin/cat /etc/resolv.conf | grep nameserver');
  preg_match('/nameserver ([\d\.]+)/', $ipdns, $result);
  $eth_dns = $result[1];

  // Create XML
	$doc = new DomDocument('1.0');
	// Interface
	$root = $doc->createElement('Interface');
	$root = $doc->appendChild($root);
  
	$emode = $doc->createElement('mode');
	$emode = $root->appendChild($emode);
  $value = $doc->createTextNode($ethMode);
	$value = $emode->appendChild($value);
  
	$eAddress = $doc->createElement('address');
	$eAddress = $root->appendChild($eAddress);
  $value = $doc->createTextNode($eth_IP);
	$value = $eAddress->appendChild($value);

	$eNetmask = $doc->createElement('netmask');
	$eNetmask = $root->appendChild($eNetmask);
  $value = $doc->createTextNode($eth_netmask);
	$value = $eNetmask->appendChild($value);

 	$eGateway = $doc->createElement('gateway');
	$eGateway = $root->appendChild($eGateway);
   $value = $doc->createTextNode($eth_gateway);
	$value = $eGateway->appendChild($value);
   
 	//$eNetwork = $doc->createElement('network');
	//$eNetwork = $root->appendChild($eNetwork);
  //$value = $doc->createTextNode($eth_network);
	//$value = $eNetwork->appendChild($value);

 	//$eBroadcast = $doc->createElement('broadcast');
	//$eBroadcast = $root->appendChild($eBroadcast);
  //$value = $doc->createTextNode($eth_broadcast);
	//$value = $eBroadcast->appendChild($value);

   
	echo $doc->saveHTML();
}

function UpdatePlugin()
{
	EchoStatusXML('Failure');
}

function UninstallPlugin()
{
	$plugin = $_GET['plugin'];
	check($plugin, "plugin", __FUNCTION__);

	global $fppDir, $pluginDirectory, $SUDO;

	if ( !file_exists("$pluginDirectory/$plugin") )
	{
		EchoStatusXML('Failure');
		error_log("Failure, no plugin to uninstall");
		return;
	}

	exec("export SUDO=\"".$SUDO."\"; export PLUGINDIR=\"".$pluginDirectory."\"; $fppDir/scripts/uninstall_plugin $plugin", $output, $return_val);
	unset($output);
	if ( $return_val != 0 )
	{
		EchoStatusXML('Failure');
		error_log("Failure with FPP uninstall script");
		return;
	}

	EchoStatusXML('Success');
}

function InstallPlugin()
{
	$plugin = $_GET['plugin'];
	check($plugin, "plugin", __FUNCTION__);

	global $fppDir, $pluginDirectory, $SUDO;

	if ( file_exists("$pluginDirectory/$plugin") )
	{
		EchoStatusXML('Failure');
		error_log("Failure, plugin you're trying to install already exists");
		return;
	}

	require_once("pluginData.inc.php");

	foreach ($plugins as $available_plugin)
	{
		if ( $available_plugin['shortName'] == $plugin )
		{
			exec("export SUDO=\"".$SUDO."\"; export PLUGINDIR=\"".$pluginDirectory."\"; $fppDir/scripts/install_plugin $plugin \"" . $available_plugin['sourceUrl'] .
				"\" \"" . $available_plugin['sha'] ."\"", $output, $return_val);
			unset($output);
			if ( $return_val != 0 )
			{
				EchoStatusXML('Failure');
				error_log("Failure with FPP install script");
				return;
			}
		}
	}
	EchoStatusXML('Success');
}

function SetupExtGPIO()
{
	$gpio = $_GET['gpio'];
	$mode = $_GET['mode'];
	check($gpio, "gpio", __FUNCTION__);
	check($mode, "mode", __FUNCTION__);

	$status = SendCommand(sprintf("SetupExtGPIO,%s,%s", $gpio, $mode));
	$status = explode(',', $status, 14);

	if ((int) $status[1] == 1) {
		EchoStatusXML('Success');
	} else {
		EchoStatusXML('Failed');
	}
}

function ExtGPIO()
{
	$gpio = $_GET['gpio'];
	$mode = $_GET['mode'];
	$val = $_GET['val'];
	check($gpio, "gpio", __FUNCTION__);
	check($mode, "mode", __FUNCTION__);
	check($val, "val", __FUNCTION__);

	$status = SendCommand(sprintf("ExtGPIO,%s,%s,%s", $gpio, $mode, $val));
	$status = explode(',', $status, 14);

	if ((int) $status[1] >= 0) {
		$doc = new DomDocument('1.0');
		$root = $doc->createElement('Status');
		$root = $doc->appendChild($root);

		$temp = $doc->createElement('Success');
		$temp = $root->appendChild($temp);

		$result = $doc->createTextNode((int) $status[6]);
		$result = $temp->appendChild($result);
		
		echo $doc->saveHTML();
	}
	else {
		EchoStatusXML('Failed');
	}
}

?>
