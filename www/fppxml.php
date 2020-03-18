<?php

$skipJSsettings = 1;
require_once('common.php');

require_once('universeentry.php');
require_once('pixelnetdmxentry.php');
require_once('commandsocket.php');

error_reporting(E_ALL);

// Commands defined here which return something other
// than XML need to return their own Content-type header.
$nonXML = Array(
	"getFile" => 1,
	"getGitOriginLog" => 1,
	"gitStatus" => 1,
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
	"getFiles" => 'GetFiles',
	"getZip" => 'GetZip',
	"getUniverseReceivedBytes" => 'GetUniverseReceivedBytes',
	"deleteFile" => 'DeleteFile',
	"setUniverseCount" => 'SetUniverseCount',
	"getUniverses" => 'GetUniverses',
	"getPixelnetDMXoutputs" => 'GetPixelnetDMXoutputs',
	"deleteUniverse" => 'DeleteUniverse',
	"cloneUniverse" => 'CloneUniverse',
	"viewReleaseNotes" => 'ViewReleaseNotes',
	"viewRemoteScript" => 'ViewRemoteScript',
	"installRemoteScript" => 'InstallRemoteScript',
	"moveFile" => 'MoveFile',
	"isFPPDrunning" => 'IsFPPDrunning',
	"getFPPstatus" => 'GetFPPstatus',
	"stopGracefully" => 'StopGracefully',
	"stopGracefullyAfterLoop" => 'StopGracefullyAfterLoop',
    "playlistNextEntry" => 'PlaylistNextEntry',
    "playlistPrevEntry" => 'PlaylistPrevEntry',
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
	"saveUSBDongle" => 'SaveUSBDongle',
	"getInterfaceInfo" => 'GetInterfaceInfo',
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
	global $fppDir, $SUDO, $mediaDirectory;

    if (file_exists("/.dockerenv")) {
        exec($SUDO . " $fppDir/scripts/fppd_stop");
        touch("$mediaDirectory/tmp/fppd_restarted");
        exec("$fppDir/scripts/git_pull");
        exec($SUDO . " $fppDir/scripts/fppd_start");
    } else {
        exec($SUDO . " systemctl stop fppd");
        touch("$mediaDirectory/tmp/fppd_restarted");
        exec("$fppDir/scripts/git_pull");
        exec($SUDO . " systemctl restart fppd");
    }

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

function SetVolume()
{
	global $SUDO;
	global $settings;

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
		exec($SUDO . " grep card /root/.asoundrc | head -n 1 | awk '{print $2}'", $output, $return_val);
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

    if ( $card == 0 && $settings['Platform'] == "Raspberry Pi" && $settings['AudioCard0Type'] == "bcm2") {
        $vol = 50 + ($vol/2.0);
    }

	// Why do we do this here and in fppd's settings.c
	$status=exec($SUDO . " amixer -c $card set $mixerDevice -- " . $vol . "%");

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
	global $mediaDirectory, $uploadDirectory, $musicDirectory, $sequenceDirectory, $videoDirectory, $effectDirectory, $scriptDirectory, $imageDirectory, $configDirectory, $SUDO;

	$file = $_GET['file'];
	check($file, "file", __FUNCTION__);

	// Fix double quote uploading by simply moving the file first, if we find it with URL encoding
	if ( strstr($file, '"') ) {
		if (!rename($uploadDirectory."/" . preg_replace('/"/', '%22', $file), $uploadDirectory."/" . $file)) {
            //Firefox and xLights will upload with " intact so if the rename doesn't work, it's OK
		}
	}
    
	if (file_exists($uploadDirectory."/" . $file)) {
		if (preg_match("/\.(fseq)$/i", $file)) {
			if ( !rename($uploadDirectory."/" . $file, $sequenceDirectory . '/' . $file) ) {
				error_log("Couldn't move sequence file");
				exit(1);
			}
		} else if (preg_match("/\.(fseq.gz)$/i", $file)) {
            if ( !rename($uploadDirectory."/" . $file, $sequenceDirectory . '/' . $file) ) {
                error_log("Couldn't move sequence file");
                exit(1);
            }
            $nfile = $file;
            $nfile = str_replace('"', '\\"', $nfile);
            exec("$SUDO gunzip -f \"$sequenceDirectory/$nfile\"");
        } else if (preg_match("/\.(eseq)$/i", $file)) {
			if ( !rename($uploadDirectory."/" . $file, $effectDirectory . '/' . $file) ) {
				error_log("Couldn't move effect file");
				exit(1);
			}
		} else if (preg_match("/\.(mp4|mkv|avi|mov|mpg|mpeg)$/i", $file)) {
			if ( !rename($uploadDirectory."/" . $file, $videoDirectory . '/' . $file) ) {
				error_log("Couldn't move video file");
				exit(1);
			}
		} else if (preg_match("/\.(gif|jpg|jpeg|png)$/i", $file)) {
			if ( !rename($uploadDirectory."/" . $file, $imageDirectory . '/' . $file) ) {
				error_log("Couldn't move image file");
				exit(1);
			}
		} else if (preg_match("/\.(sh|pl|pm|php|py)$/i", $file)) {
			// Get rid of any DOS newlines
			$contents = file_get_contents($uploadDirectory."/".$file);
			$contents = str_replace("\r", "", $contents);
			file_put_contents($uploadDirectory."/".$file, $contents);

			if ( !rename($uploadDirectory."/" . $file, $scriptDirectory . '/' . $file) ) {
				error_log("Couldn't move script file");
				exit(1);
			}
        } else if (preg_match("/\.(mp3|ogg|m4a|wav|au|m4p|wma)$/i", $file)) {
			if ( !rename($uploadDirectory."/" . $file, $musicDirectory . '/' . $file) ) {
				error_log("Couldn't move music file");
				exit(1);
			}
        } else if (preg_match("/eeprom\.bin$/i", $file)) {
            if ( !rename($uploadDirectory."/" . $file, $configDirectory . '/cape-eeprom.bin') ) {
                error_log("Couldn't move eeprom file");
                exit(1);
            }
		}
	} else {
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

function GetExpandedEventID($id)
{
	check($id, "id", __FUNCTION__);

	$majorID = preg_replace('/_.*/', '', $id);
	$minorID = preg_replace('/.*_/', '', $id);

	$filename = sprintf("%02d_%02d", $majorID, $minorID);

	return $filename;
}

function TriggerEvent()
{
	$id = GetExpandedEventID($_GET['id']);

	$status = SendCommand("t," . $id . ",");

	EchoStatusXML($status);
}

function SaveEvent()
{
	global $eventDirectory;
    $event = json_decode(file_get_contents("php://input"), true);
    print_r($event);

	$id = $event['id'];
	check($id, "id", __FUNCTION__);

	$ids = preg_split('/_/', $id);

	if (count($ids) < 2)
		return;

    
    $majorID = preg_replace('/_.*/', '', $id);
    $minorID = preg_replace('/.*_/', '', $id);
    $event['majorId'] = (int)$majorID;
    $event['minorId'] = (int)$minorID;
    unset($event['id']);
    
        
	$id = GetExpandedEventID($id);
	$filename = $id . ".fevt";

    file_put_contents($eventDirectory . '/' . $filename, json_encode($event, JSON_PRETTY_PRINT));

	EchoStatusXML('Success');
}

function DeleteEvent()
{
	global $eventDirectory;

	$filename = GetExpandedEventID($_GET['id']) . ".fevt";

	unlink($eventDirectory . '/' . $filename);

	EchoStatusXML('Success');
}

function GetUniverseReceivedBytes()
{
	$data = file_get_contents('http://127.0.0.1:32322/fppd/e131stats');
	$stats = json_decode($data);

	$doc = new DomDocument('1.0');
	if(count($stats->universes))
	{
		$root = $doc->createElement('receivedBytes');
        $root->setAttribute('size', count($stats->universes));
		$root = $doc->appendChild($root);

		for ($i = 0; $i < count($stats->universes); $i++)
		{
			$receivedInfo = $doc->createElement('receivedInfo');
			$receivedInfo = $root->appendChild($receivedInfo); 
			// universe
			$universe = $doc->createElement('universe');
			$universe = $receivedInfo->appendChild($universe);
			$value = $doc->createTextNode($stats->universes[$i]->id);
			$value = $universe->appendChild($value);
			// startChannel
			$startChannel = $doc->createElement('startChannel');
			$startChannel = $receivedInfo->appendChild($startChannel);
			$value = $doc->createTextNode($stats->universes[$i]->startChannel);
			$value = $startChannel->appendChild($value);
			// bytes received
			$bytesReceived = $doc->createElement('bytesReceived');
			$bytesReceived = $receivedInfo->appendChild($bytesReceived);
			$value = $doc->createTextNode($stats->universes[$i]->bytesReceived);
			$value = $bytesReceived->appendChild($value);
			// packets received
			$packetsReceived = $doc->createElement('packetsReceived');
			$packetsReceived = $receivedInfo->appendChild($packetsReceived);
			$value = $doc->createTextNode($stats->universes[$i]->packetsReceived);
			$value = $packetsReceived->appendChild($value);
            
            // errors received
            $errorCount = $doc->createElement('errors');
            $errorCount = $receivedInfo->appendChild($errorCount);
            $value = $doc->createTextNode($stats->universes[$i]->errors);
            $value = $errorCount->appendChild($value);
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
function StopGracefullyAfterLoop()
{
	$status=SendCommand('StopGracefullyAfterLoop');
	EchoStatusXML('true');
}
function PlaylistNextEntry()
{
    $status=SendCommand('NextPlaylistItem');
    EchoStatusXML('true');
}
function PlaylistPrevEntry()
{
    $status=SendCommand('PrevPlaylistItem');
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
    
    if (file_exists("/.dockerenv")) {
        usleep(500000);
        // kill it if it's still running
        exec($SUDO . " " . dirname(dirname(__FILE__)) . "/scripts/fppd_stop");
    } else {
        exec($SUDO . " systemctl stop fppd");
    }
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
        if (file_exists("/.dockerenv")) {
            $status=exec($SUDO . " " . dirname(dirname(__FILE__)) . "/scripts/fppd_start");
        } else {
            exec($SUDO . " systemctl restart fppd");
        }
	}
	EchoStatusXML($status);
}

function RestartFPPD()
{
    if ((isset($_GET['quick'])) && ($_GET['quick'] == 1))
    {
        $status=exec("if ps cax | grep -q fppd; then echo \"true\"; else echo \"false\"; fi");
        if ($status == 'true')
        {
            SendCommand('restart');
            return;
        }
    }

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
			$desc = $_SESSION['UniverseEntries'][$index]->desc;
			$universe = $_SESSION['UniverseEntries'][$index]->universe+1;
			$size = $_SESSION['UniverseEntries'][$index]->size;
			$startAddress = $_SESSION['UniverseEntries'][$index]->startAddress+$size;
			$type = $_SESSION['UniverseEntries'][$index]->type;
			$unicastAddress = $_SESSION['UniverseEntries'][$index]->unicastAddress;
			$priority = $_SESSION['UniverseEntries'][$index]->priority;

			for($i=$index+1;$i<$index+1+$numberToClone;$i++,$universe++)
			{
				 	$_SESSION['UniverseEntries'][$i]->desc	= $desc;
				 	$_SESSION['UniverseEntries'][$i]->universe	= $universe;
				 	$_SESSION['UniverseEntries'][$i]->size	= $size;
				 	$_SESSION['UniverseEntries'][$i]->startAddress	= $startAddress;
				 	$_SESSION['UniverseEntries'][$i]->type	= $type;
					$_SESSION['UniverseEntries'][$i]->unicastAddress	= $unicastAddress;
				 	$_SESSION['UniverseEntries'][$i]->priority	= $priority;
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

function LoadUniverseFile($input)
{
	global $settings;

	$_SESSION['UniverseEntries']=NULL;

	$filename = $settings['universeOutputs'];
	if ($input)
		$filename = $settings['universeInputs'];

	if(!file_exists($filename))
	{
		$_SESSION['UniverseEntries'][] = new UniverseEntry(1,"",1,1,512,0,"",0,0);
		return;
	}

	$jsonStr = file_get_contents($filename);

	$data = json_decode($jsonStr);
	$universes = 0;
	
	if ($input)
		$universes = $data->channelInputs[0]->universes;
	else
		$universes = $data->channelOutputs[0]->universes;

	foreach ($universes as $univ)
	{
		$active = $univ->active;
		$desc = $univ->description;
		$universe = $univ->id;
		$startAddress = $univ->startChannel;
		$size = $univ->channelCount;
		$type = $univ->type;
		$unicastAddress = $univ->address;
		$priority = $univ->priority;
		$_SESSION['UniverseEntries'][] = new UniverseEntry($active,$desc,$universe,$startAddress,$size,$type,$unicastAddress,$priority,0);
	}
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
	$input = $_GET['input'];
	check($input, "input", __FUNCTION__);

	if($reload == "TRUE")
	{
		LoadUniverseFile($input);
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
		// description
		$desc = $doc->createElement('desc');
		$desc = $UniverseEntry->appendChild($desc);
		$value = $doc->createTextNode($_SESSION['UniverseEntries'][$i]->desc);
		$value = $desc->appendChild($value);
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
		// priority
		$priority = $doc->createElement('priority');
		$priority = $UniverseEntry->appendChild($priority);
		$value = $doc->createTextNode($_SESSION['UniverseEntries'][$i]->priority);
		$value = $priority->appendChild($value);

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
			$desc = "";
				$universe = 1;
				$startAddress = 1;
				$size = 512;
				$type = 0;	//Multicast
				$unicastAddress = "";
				$priority = 0;
			if($universeCount == 0)
			{
				$universe = 1;
				$startAddress = 1;
				$size = 512;
				$type = 0;	//Multicast
				$unicastAddress = "";
				$priority = 0;

			}
			else
			{
				$universe = $_SESSION['UniverseEntries'][$universeCount-1]->universe+1;
				$size = $_SESSION['UniverseEntries'][$universeCount-1]->size;
				$startAddress = $_SESSION['UniverseEntries'][$universeCount-1]->startAddress+$size;
				$type = $_SESSION['UniverseEntries'][$universeCount-1]->type;
				$unicastAddress = $_SESSION['UniverseEntries'][$universeCount-1]->unicastAddress;
				$priority = $_SESSION['UniverseEntries'][$universeCount-1]->priority;
			}

			for($i=$universeCount;$i<$count;$i++,$universe++)
			{
				$_SESSION['UniverseEntries'][] = new UniverseEntry($active,$desc,$universe,$startAddress,$size,$type,$unicastAddress,$priority,0);
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

	$fileInfo = $doc->createElement('FileInfo');
	$fileInfo = $file->appendChild($fileInfo);
	//quick and dirty check for music and video dir
	if(strpos(strtolower($dirName),"music") !== FALSE || strpos(strtolower($dirName),"video") !== FALSE ){

		//Check the cache first
		$filesize = filesize($fileFullName);
		$cache_duration = media_duration_cache($fileName, null, $filesize);
		//cache duration will be null if not in cache, then retrieve it
		if ($cache_duration == NULL) {
			//Include our getid3 library for media
			require_once('./lib/getid3/getid3.php');
			//Instantiate getID3 object
			$getID3 = new getID3;
			//Get the media duration from file
			$ThisFileInfo = $getID3->analyze($fileFullName);
			//cache it
			if (isset($ThisFileInfo['playtime_seconds']))
				media_duration_cache($fileName, $ThisFileInfo['playtime_seconds'], $filesize);
		} else {
			$ThisFileInfo['playtime_seconds'] = $cache_duration;
		}

		if (isset($ThisFileInfo['playtime_seconds']))
			$value = $doc->createTextNode(human_playtime($ThisFileInfo['playtime_seconds']));
		else
			$value = $doc->createTextNode("Unknown");
	}else{
		//just return the filesize
		$value = $doc->createTextNode(human_filesize($fileFullName));
	}
	$value = $fileInfo->appendChild($value);
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
	global $imageDirectory;
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
	else if ($dirName == "Images")      { $dirName = $imageDirectory; }
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

function GetFile()
{
	$filename = $_GET['filename'];
	check($filename, "filename", __FUNCTION__);

	$dir = $_GET['dir'];
	check($dir, "dir", __FUNCTION__);

	$isImage = 0;
	if ($dir == 'Images')
		$isImage = 1;

	$dir = GetDirSetting($dir);

	if ($dir == "")
		return;

	if (isset($_GET['play']))
	{
		if (preg_match('/mp3$/i', $filename))
			header('Content-type: audio/mp3');
		else if (preg_match('/ogg$/i', $filename))
			header('Content-type: audio/ogg');
        else if (preg_match('/m4a$/i', $filename))
            header('Content-type: audio/m4a');
        else if (preg_match('/mov$/i', $filename))
            header('Content-type: video/mov');
		else if (preg_match('/mp4$/i', $filename))
			header('Content-type: video/mp4');
		else if (preg_match('/wav$/i', $filename))
			header('Content-type: audio/wav');
		else if (preg_match('/mpg$/i', $filename))
			header('Content-type: video/mpg');
        else if (preg_match('/mpg$/i', $filename))
            header('Content-type: video/mpeg');
        else if (preg_match('/mkv$/i', $filename))
            header('Content-type: video/mkv');
        else if (preg_match('/avi$/i', $filename))
            header('Content-type: video/avi');
	}
	else if ($isImage)
	{
		header('Content-type: ' . mime_content_type($dir . '/' . $filename));

		if (!isset($_GET['attach']) || ($_GET['attach'] == '1'))
			header('Content-disposition: attachment;filename="' . $filename . '"');
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
        "channelremap",
        "config/channeloutputs.json",
        //new v2 config files
        "config/schedule.json",
        "config/outputprocessors.json",
        "config/co-other.json",
        "config/co-pixelStrings.json",
        "config/co-bbbStrings.json",
        "config/co-universes.json",
        "config/ci-universes.json",
        "config/model-overlays.json",
        //
        "pixelnetDMX",
        "settings",
        "universes"
    );
    
    foreach($files as $file) {
        if (file_exists("$mediaDirectory/$file")){
            $fileData='';
            //Handle these files differently, as they are CSV or other, and not a ini or JSON file
            //ScrubFile assumes a INI file for files with the .json extension
            if(in_array($file,array('schedule', 'channelmemorymaps', 'channeloutputs', 'channelremap', 'universes'))){
                $fileData = file_get_contents("$mediaDirectory/$file");
            }else{
                $fileData = ScrubFile("$mediaDirectory/$file");
            }
            $zip->addFromString("Config/$file", $fileData);
        }
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
