<?php

$skipJSsettings = 1;
require_once('common.php');

require_once('playlistentry.php');
require_once('universeentry.php');
require_once('scheduleentry.php');
require_once('pixelnetdmxentry.php');
require_once('commandsocket.php');

//define('debug', true);

// Commands defined here which return something other
// than XML need to return their own Content-type header.
$nonXML = Array(
	"getLog" => 1,
	"getUpload" => 1,
	"getGitOriginLog" => 1
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
	"getSequences" => 'GetSequenceFiles',
	"getPlayListEntries" => 'GetPlaylistEntries',
	"setPlayListFirstLast" => 'SetPlayListFirstLast',
	"addPlayList" => 'AddPlaylist',
	"getUniverseReceivedBytes" => 'GetUniverseReceivedBytes',
	"sort" => 'PlaylistEntryPositionChanged',
	"savePlaylist" => 'SavePlaylist',
	"deletePlaylist" => 'DeletePlaylist',
	"deleteEntry" => 'DeleteEntry',
	"deleteSequence" => 'DeleteSequence',
	"deleteMusic" => 'DeleteMusic',
	"deleteVideo" => 'DeleteVideo',
	"deleteScript" => 'DeleteScript',
	"deleteLog" => 'DeleteLog',
	"deleteUpload" => 'DeleteUpload',
	"addPlaylistEntry" => 'AddPlayListEntry',
	"setUniverseCount" => 'SetUniverseCount',
	"getUniverses" => 'GetUniverses',
	"getPixelnetDMXoutputs" => 'GetPixelnetDMXoutputs',
	"deleteUniverse" => 'DeleteUniverse',
	"cloneUniverse" => 'CloneUniverse',
	"getSchedule" => 'GetSchedule',
	"addScheduleEntry" => 'AddScheduleEntry',
	"deleteScheduleEntry" => 'DeleteScheduleEntry',
	"moveFile" => 'MoveFile',
	"isFPPDrunning" => 'IsFPPDrunning',
	"getFPPstatus" => 'GetFPPstatus',
	"stopGracefully" => 'StopGracefully',
	"stopNow" => 'StopNow',
	"stopFPPD" => 'StopFPPD',
	"startFPPD" => 'StartFPPD',
	"startPlaylist" => 'StartPlaylist',
	"rebootPi" => 'RebootPi',
	"shutdownPi" => 'ShutdownPi',
	"manualGitUpdate" => 'ManualGitUpdate',
	"changeGitBranch" => 'ChangeGitBranch',
	"getGitOriginLog" => 'GetGitOriginLog',
	"setAutoUpdate" => 'SetAutoUpdate',
	"setDeveloperMode" => 'SetDeveloperMode',
	"setVolume" => 'SetVolume',
	"setFPPDmode" => 'SetFPPDmode',
	"getVolume" => 'GetVolume',
	"getFPPDmode" => 'GetFPPDmode',
	"setE131interface" => 'SetE131interface',
	"playEffect" => 'PlayEffect',
	"stopEffect" => 'StopEffect',
	"deleteEffect" => 'DeleteEffect',
	"getRunningEffects" => 'GetRunningEffects',
	"triggerEvent" => 'TriggerEvent',
	"saveEvent" => 'SaveEvent',
	"deleteEvent" => 'DeleteEvent',
	"getLog" => 'GetLog',
	"getUpload" => 'GetUpload',
	"saveUSBDongle" => 'SaveUSBDongle',
	"getInterfaceInfo" => 'GetInterfaceInfo',
	"setPiLCDenabled" => 'SetPiLCDenabled',
	"updatePlugin" => 'UpdatePlugin',
	"uninstallPlugin" => 'UninstallPlugin',
	"installPlugin" => 'InstallPlugin'
);

if (isset($_GET['command']) && !isset($nonXML[$_GET['command']]))
	header('Content-type: text/xml');



if ( isset($_GET['command']) && !empty($_GET['command']) )
{
	if ( array_key_exists($_GET['command'],$command_array) )
	{
		if ( defined('debug') )
			error_log("Calling ".$_GET['command']);
		call_user_func($command_array[$_GET['command']]);
	}
	return;
}

else if(!empty($_POST['command']) && $_POST['command'] == "saveUniverses")
{
	SetUniverses();
}
else if(!empty($_POST['command']) && $_POST['command'] == "savePixelnetDMX")
{
	SavePixelnetDMX();
}
else if(!empty($_POST['command']) && $_POST['command'] == "saveSchedule")
{
	SaveSchedule();
}

/////////////////////////////////////////////////////////////////////////////

function check($var)
{
	if ( empty($var) || !isset($var) )
	{
		error_log("WARNING: Variable we checked in function '".$_GET['command']."' was empty");
//		die();
	}
}

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
	$status=exec(SUDO . " shutdown -r now");
	EchoStatusXML($status);
}

function ManualGitUpdate()
{
	global $fppDir;
	exec(SUDO . " $fppDir/scripts/fppd_stop");
	exec("$fppDir/scripts/git_pull");
	exec(SUDO . " $fppDir/scripts/fppd_start");
}

function ChangeGitBranch()
{
	$branch = $_GET['branch'];
	check($branch);

	global $fppDir;
	exec("$fppDir/scripts/git_branch $branch");
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

function SetAutoUpdate()
{
	$enabled = $_GET['enabled'];
	check($enabled);

	global $mediaDirectory;
	if ($enabled)
		unlink("$mediaDirectory/.auto_update_disabled");
	else
		exec("touch $mediaDirectory/.auto_update_disabled");
}



function SetDeveloperMode()
{
	$enabled = $_GET['enabled'];
	check($enabled);

	global $mediaDirectory;
	if ($enabled)
		exec("touch $mediaDirectory/.developer_mode");
	else
		unlink("$mediaDirectory/.developer_mode");
}

function SetVolume()
{
	$volume = $_GET['volume'];
	check($volume);
  WriteSettingToFile("volume",$volume);
	$vol = intval ($volume);
	if($vol>=100)
	{
		$vol = "99";	
	}
		
	$status=SendCommand('v,' . $vol . ',');

	$vol = 50 + ($vol/2);

	$status=exec("amixer set PCM -- " . $vol . "%");

	EchoStatusXML($status);
}

function SetPiLCDenabled()
{
	$enabled = $_GET['enabled'];
	check($enabled);
  WriteSettingToFile("PI_LCD_Enabled",$enabled);
  if ($enabled == "true")
  {
    $status = exec(SUDO . " " . dirname(dirname(__FILE__)) . "/scripts/lcd/fppLCD start");
  }
  else
  {
    $status = exec(SUDO . " " . dirname(dirname(__FILE__)) . "/scripts/lcd/fppLCD stop");
  }
	EchoStatusXML($status);
}

function SetFPPDmode()
{
	$mode_string[0] = "unknown";
	$mode_string[1] = "bridge";
	$mode_string[2] = "player";
	$mode = $_GET['mode'];
	check($mode);
  WriteSettingToFile("fppMode",$mode_string[$mode]);
	EchoStatusXML("true");
}

function SetE131interface()
{
	$iface = $_GET['iface'];
	check($iface);
	WriteSettingToFile("E131interface",$iface);
	EchoStatusXML("true");
}

function GetVolume()
{
	$volume = ReadSettingFromFile("volume");
	$doc = new DomDocument('1.0');
	$root = $doc->createElement('Volume');
	$root = $doc->appendChild($root);
	$value = $doc->createTextNode($volume);
	$value = $root->appendChild($value);
	echo $doc->saveHTML();
}

function GetFPPDmode()
{
	$mode = ReadSettingFromFile("fppMode");
	$fppMode = $mode == "bridge" ? "1":"2";
	$doc = new DomDocument('1.0');
	$root = $doc->createElement('mode');
	$root = $doc->appendChild($root);
	$value = $doc->createTextNode($fppMode);
	$value = $root->appendChild($value);
	echo $doc->saveHTML();
}

function ShutdownPi()
{
	$status=exec(SUDO . " shutdown -h now");
	EchoStatusXML($status);
}

function MoveFile()
{
	global $mediaDirectory, $musicDirectory, $sequenceDirectory, $videoDirectory, $effectDirectory, $scriptDirectory;

	$file = $_GET['file'];
	check($file);

	if(file_exists($mediaDirectory."/upload/" . $file))
	{
		if (strpos(strtolower($file),".fseq") !== false)
		{
			if ( !rename($mediaDirectory."/upload/" . $file, $sequenceDirectory . '/' . $file) )
			{
				error_log("Couldn't move sequence file");
				exit(1);
			}
		}
		else if (strpos(strtolower($file),".eseq") !== false)
		{
			if ( !rename($mediaDirectory."/upload/" . $file, $effectDirectory . '/' . $file) )
			{
				error_log("Couldn't move effect file");
				exit(1);
			}
		}
		else if (strpos(strtolower($file),".mp4") !== false)
		{
			if ( !rename($mediaDirectory."/upload/" . $file, $videoDirectory . '/' . $file) )
			{
				error_log("Couldn't move video file");
				exit(1);
			}
		}
		else if (strpos(strtolower($file),".sh") !== false)
		{
			// Attempt to get rid of DOS newlines
			exec("perl -pi -e 's@\r\n@\n@g' ".$mediaDirectory."/upload/".$file);

			if ( !rename($mediaDirectory."/upload/" . $file, $scriptDirectory . '/' . $file) )
			{
				error_log("Couldn't move script file");
				exit(1);
			}
		}
		else if ((strpos(strtolower($file),".mp3") !== false) ||
		         (strpos(strtolower($file),".ogg") !== false))
		{
			if ( !rename($mediaDirectory."/upload/" . $file, $musicDirectory . '/' . $file) )
			{
				error_log("Couldn't move music file");
				exit(1);
			}
		}
		else
		{
			unlink($mediaDirectory."/upload/" . $file);
			error_log("Unknown file type, removing upload");
		}
	}
	else
	{
		error_log("Couldn't find file in upload directory");
		exit(1);
	}
	EchoStatusXML('Success');
}

function IsFPPDrunning()
{
	$status=exec("if ps cax | grep -q fppd; then echo \"true\"; else echo \"false\"; fi");
	EchoStatusXML($status);
}

function StartPlaylist()
{
	$playlist = $_GET['playList'];
	$repeat = $_GET['repeat'];
	$playEntry = $_GET['playEntry'];
	check($playlist);
	check($repeat);
	check($playEntry);

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
	check($effect);
	$startChannel = $_GET['startChannel'];
	check($startChannel);
	$status = SendCommand("e," . $effect . "," . $startChannel . ",");
	EchoStatusXML('Success');
}

function StopEffect()
{
	$id = $_GET['id'];
	check($id);
	$status = SendCommand("StopEffect," . $id . ",");
	EchoStatusXML('Success');
}

function DeleteEffect()
{
	global $effectDirectory;

	$effect = $_GET['effect'];
	check($effect);

	unlink($effectDirectory . '/' . $effect . ".eseq");

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
	check($id);

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
	check($id);

	$ids = preg_split('/_/', $id);

	if (count($ids) < 2)
		return;

	$id = GetExpandedEventID();
	$filename = $id . ".fevt";

	$name = $_GET['event'];
	check($name);

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

function StopFPPD()
{
	SendCommand('d'); // Ignore return and just kill if 'd' doesn't work...
	$status=exec(SUDO . " " . dirname(dirname(__FILE__)) . "/scripts/fppd_stop");
	EchoStatusXML('true');
}


function StartFPPD()
{
	global $settingsFile;

	$status=exec("if ps cax | grep -q fppd; then echo \"true\"; else echo \"false\"; fi");
	if($status == 'false')
	{
		$status=exec(SUDO . " " . dirname(dirname(__FILE__)) . "/scripts/fppd_start");
	}
	EchoStatusXML($status);
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

	$entry = explode(",",$status,13);
	$fppMode = $entry[0];
	if($fppMode == 2)
	{
		$fppStatus = $entry[1];
		if($fppStatus == '0')
		{
			$volume = $entry[2];
			$nextPlaylist = $entry[3];
			$nextPlaylistStartTime = $entry[4];
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
	check($index);

	if($index < count($_SESSION['ScheduleEntries']) && count($_SESSION['ScheduleEntries']) > 1 )
	{
		unset($_SESSION['ScheduleEntries'][$index]);
		$_SESSION['ScheduleEntries'] = array_values($_SESSION['ScheduleEntries']);

	}
	EchoStatusXML('Success');
}

function AddScheduleEntry()
{
	$_SESSION['ScheduleEntries'][] = new ScheduleEntry(1,'',7,0,0,0,0,0,0,1);
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
			$entries .= sprintf("%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,",
						$_SESSION['ScheduleEntries'][$i]->enable,
						$_SESSION['ScheduleEntries'][$i]->playlist,
						$_SESSION['ScheduleEntries'][$i]->startDay,
						$_SESSION['ScheduleEntries'][$i]->startHour,
						$_SESSION['ScheduleEntries'][$i]->startMinute,
						$_SESSION['ScheduleEntries'][$i]->startSecond,
						$_SESSION['ScheduleEntries'][$i]->endHour,
						$_SESSION['ScheduleEntries'][$i]->endMinute,
						$_SESSION['ScheduleEntries'][$i]->endSecond,
						$_SESSION['ScheduleEntries'][$i]->repeat
									);
			}
			else
			{
			$entries .= sprintf("\n%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,",
						$_SESSION['ScheduleEntries'][$i]->enable,
						$_SESSION['ScheduleEntries'][$i]->playlist,
						$_SESSION['ScheduleEntries'][$i]->startDay,
						$_SESSION['ScheduleEntries'][$i]->startHour,
						$_SESSION['ScheduleEntries'][$i]->startMinute,
						$_SESSION['ScheduleEntries'][$i]->startSecond,
						$_SESSION['ScheduleEntries'][$i]->endHour,
						$_SESSION['ScheduleEntries'][$i]->endMinute,
						$_SESSION['ScheduleEntries'][$i]->endSecond,
						$_SESSION['ScheduleEntries'][$i]->repeat);
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
		$entry = explode(",",$line,12);
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
		$_SESSION['ScheduleEntries'][] = new ScheduleEntry($enable,$playlist,$startDay,$startHour,$startMinute,$startSecond,
				$endHour, $endMinute, $endSecond, $repeat);
	}
	fclose($f);
}

function GetSchedule()
{
	$reload = $_GET['reload'];
	check($reload);

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

	}
	echo $doc->saveHTML();
}

function CloneUniverse()
{
	$index = $_GET['index'];
	$numberToClone = $_GET['numberToClone'];
	check($index);
	check($numberToClone);

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
	check($index);

	if($index < count($_SESSION['UniverseEntries']) && count($_SESSION['UniverseEntries']) > 1 )
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

function SavePixelnetDMX()
{
	for($i=0;$i<count($_SESSION['PixelnetDMXentries']);$i++)
	{
		if( isset($_POST['chkActive'][$i]))
		{
			$_SESSION['PixelnetDMXentries'][$i]->active = 1;
		}
		else
		{
			$_SESSION['PixelnetDMXentries'][$i]->active = 0;
		}
		$_SESSION['PixelnetDMXentries'][$i]->startAddress = 	intval($_POST['txtStartAddress'][$i]);
		$_SESSION['PixelnetDMXentries'][$i]->type = 	intval($_POST['pixelnetDMXtype'][$i]);
	}

	SavePixelnetDMXoutputsToFile();

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
	global $pixelnetFile;

	$_SESSION['PixelnetDMXentries']=NULL;

	$f=fopen($pixelnetFile,"r");
	if($f == FALSE)
	{
		fclose($f);
		//No file exists add one and save to new file.
	$_SESSION['PixelnetDMXentries'] = NULL;
		$address=1;
		for($i;$i<12;$i++)
		{
			$_SESSION['PixelnetDMXentries'][] = new PixelnetDMXentry(1,0,$address);
			$address+=4096;
		}
		SavePixelnetDMXoutputsToFile();
		return;
	}

	while (!feof($f))
	{
		$line=fgets($f);
		$entry = explode(",",$line,5);
		$active = $entry[0];
		$type = $entry[1];
		$startAddress = $entry[2];
		$_SESSION['PixelnetDMXentries'][] = new PixelnetDMXentry($active,$type,$startAddress);
	}
	fclose($f);
}

function SaveUniversesToFile()
{
	global $universeFile;

	$universes = "";
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
	check($reload);

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
	check($reload);

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
	check($count);

	if($count > 0 && $count <= 128)
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
	$videoFile = $_GET['videoFile'];
	$eventName = $_GET['eventName'];
	$eventID = $_GET['eventID'];
	check($type);
	check($seqFile);
	check($songFile);
	check($pause);
	check($videoFile);
	check($eventName);
	check($eventID);

	$_SESSION['playListEntries'][] = new PlaylistEntry($type,$songFile,$seqFile,$pause,$videoFile,$eventName,$eventID,$index,count($_SESSION['playListEntries']));
	EchoStatusXML($_GET['songFile']);
}

function GetPlaylists()
{
	global $playlistDirectory;

	$FirstList=TRUE;
	$doc = new DomDocument('1.0');
	$root = $doc->createElement('Playlists');
	$root = $doc->appendChild($root);
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

	$dirName = $_GET['dir'];
	check($dirName);
	if ($dirName == "Sequences")        { $dirName = $sequenceDirectory; }
	else if ($dirName == "Music")       { $dirName = $musicDirectory; }
	else if ($dirName == "Videos")      { $dirName = $videoDirectory; }
	else if ($dirName == "Effects")     { $dirName = $effectDirectory; }
	else if ($dirName == "Scripts")     { $dirName = $scriptDirectory; }
	else if ($dirName == "Logs")        { $dirName = $logDirectory; }
	else if ($dirName == "Uploads")     { $dirName = $uploadDirectory; }
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
	echo $doc->saveHTML();
}

function GetSequenceFiles()
{
	global $sequenceDirectory;

	$sequenceDirectry = $sequenceDirectory;
	$doc = new DomDocument('1.0');
	$root = $doc->createElement('Sequences');
	$root = $doc->appendChild($root);

	foreach(scandir($sequenceDirectry) as $seqFile)
	{
		if($seqFile != '.' && $seqFile != '..')
		{
			$seqFileFullName = $sequenceDirectry . '/' . $seqFile;

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
	check($name);

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
		$filename = $playlistDirectory . $name;
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
	check($first);
	check($last);

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
	$entry = explode(",",$line,50);
	$_SESSION['playlist_first']=$entry[0];
	$_SESSION['playlist_last']=$entry[1];
	while (!feof($f))
	{
		$line=fgets($f);
		$entry = explode(",",$line,50);
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
				$videoFile = "";
				$eventName = "";
				$eventID = "";
				break;
			default:
				break;
			case 'm':
				$songFile = $entry[1];
				$seqFile = "";
				$pause = 0;
				$index = $i;
				$videoFile = "";
				$eventName = "";
				$eventID = "";
				break;
			case 's':
				$songFile = "";
				$seqFile = $entry[1];
				$pause = 0;
				$index = $i;
				$videoFile = "";
				$eventName = "";
				$eventID = "";
				break;
			case 'p':
				$songFile = "";
				$seqFile = "";
				$pause = $entry[1];
				$index = $i;
				$videoFile = "";
				$eventName = "";
				$eventID = "";
				break;
			case 'v':
				$seqFile = "";
				$songFile = "";
				$pause = $entry[2];
				$index = $i;
				$videoFile = $entry[1];
				$eventName = "";
				$eventID = "";
				break;
			case 'e':
				$seqFile = "";
				$songFile = "";
				$pause = 0;
				$index = $i;
				$videoFile = "";
				$eventID = $entry[1];

				$eventInfo = parse_ini_file($eventDirectory . "/" . $eventID . ".fevt");
				$eventName = $eventInfo['name'];
				break;
		}
		$playListEntries[$i] = new PlaylistEntry($type,$songFile,$seqFile,$pause,$videoFile,$eventName,$eventID,$index);
		$i++;
	}
	fclose($f);
	$_SESSION['playListEntries'] = $playListEntries;
//	Print_r($_SESSION['playListEntries']);
}

function GetPlayListSettings()
{
	$file = $_GET['pl'];
	check($file);

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
	check($file);
	check($reloadFile);

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
		// videoFile
		$videoFile = $doc->createElement('videoFile');
		$videoFile = $playListEntry->appendChild($videoFile);
		$value = $doc->createTextNode($_SESSION['playListEntries'][$i]->videoFile);
		$value = $videoFile->appendChild($value);
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
	}
	echo $doc->saveHTML();
}

function PlaylistEntryPositionChanged()
{
	$newIndex = $_GET['newIndex'];
	$oldIndex = $_GET['oldIndex'];
	check($newIndex);
	check($oldIndex);

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
	check($name);
	check($first);
	check($last);

	$f=fopen($playlistDirectory . $name,"w") or exit("Unable to open file! : " . $playlistDirectory . $name);
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
		else if($_SESSION['playListEntries'][$i]->type == 'v')
		{
			$entries .= sprintf("%s,%s,%d,\n",$_SESSION['playListEntries'][$i]->type,$_SESSION['playListEntries'][$i]->videoFile,
								$_SESSION['playListEntries'][$i]->pause);
		}
		else if($_SESSION['playListEntries'][$i]->type == 'e')
		{
			$entries .= sprintf("%s,%s,\n",$_SESSION['playListEntries'][$i]->type,
				$_SESSION['playListEntries'][$i]->eventID);
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
	check($name);

	unlink($playlistDirectory . '/' . $name);
	EchoStatusXML('Success');
}

function DeleteSequence()
{
	global $sequenceDirectory;

	$name = $_GET['name'];
	check($name);

	unlink($sequenceDirectory . '/' . $name);
	EchoStatusXML('Success');
}

function DeleteMusic()
{
	global $musicDirectory;

	$name = $_GET['name'];
	check($name);

	unlink($musicDirectory . '/' . $name);
	EchoStatusXML('Success');
}

function DeleteVideo()
{
	global $videoDirectory;

	$name = $_GET['name'];
	check($name);

	unlink($videoDirectory . '/' . $name);
	EchoStatusXML('Success');
}

function DeleteScript()
{
	global $scriptDirectory;

	$name = $_GET['name'];
	check($name);

	unlink($scriptDirectory . '/' . $name);
	EchoStatusXML('Success');
}

function DeleteLog()
{
	global $logDirectory;

	$name = $_GET['name'];
	check($name);

	if (substr($name, 0, 1) != "/")
	{
		unlink($logDirectory . '/' . $name);
		EchoStatusXML('Success');
	}
	else
		EchoStatusXML('Failure');
}

function DeleteUpload()
{
	global $uploadDirectory;

	$name = $_GET['name'];
	check($name);

	if (substr($name, 0, 1) != "/")
	{
		unlink($uploadDirectory . '/' . $name);
		EchoStatusXML('Success');
	}
	else
		EchoStatusXML('Failure');
}


function DeleteEntry()
{
	$index = $_GET['index'];
	check($index);

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

function GetLog()
{
	global $logDirectory;

	$filename = $_GET['filename'];
	check($filename);

	header('Content-type: text/plain');

	if (substr($filename, 0, 9) == "/var/log/")
	{
		header('Content-disposition: attachment;filename=' .
			basename($filename));
		readfile($filename);
	}
	else
	{
		header('Content-disposition: attachment;filename=' . $filename);
		readfile($logDirectory . '/' . $filename);
	}
}

function GetUpload()
{
	global $uploadDirectory;

	$filename = $_GET['filename'];
	check($filename);

	header('Content-type: application/binary');

	header('Content-disposition: attachment;filename=' . $filename);

	readfile($uploadDirectory . '/' . $filename);
}

function SaveUSBDongle()
{
	$usbDonglePort = $_GET['port'];
	check($usbDonglePort);

	$usbDongleType = $_GET['type'];
	check($usbDongleType);

	$usbDongleBaud = $_GET['baud'];
	check($usbDongleBaud);

	WriteSettingToFile("USBDonglePort", $usbDonglePort);
	WriteSettingToFile("USBDongleType", $usbDongleType);
	WriteSettingToFile("USBDongleBaud", $usbDongleBaud);
}

function GetInterfaceInfo()
{
	$interface = $_GET['interface'];
	check($interface);

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
	check($plugin);

	global $fppDir, $pluginDirectory;

	if ( !file_exists("$pluginDirectory/$plugin") )
	{
		EchoStatusXML('Failure');
		error_log("Failure, no plugin to uninstall");
		return;
	}

	exec("$fppDir/scripts/uninstall_plugin $plugin", $output, $return_val);
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
	check($plugin);

	global $fppDir, $pluginDirectory;

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
			exec("$fppDir/scripts/install_plugin $plugin \"" . $available_plugin['sourceUrl'] . "\"", $output, $return_val);
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

?>
