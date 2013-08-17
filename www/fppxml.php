<?php
require_once('playlistentry.php');
require_once('universeentry.php');
require_once('scheduleentry.php');
require_once('pixelnetdmxentry.php');
header('Content-type: text/xml');

$a = session_id();
if(empty($a)) 
{
	session_start();
}
$_SESSION['session_id'] = session_id();


if($_GET['command'] == "getMusicFiles")
{
	GetMusicFiles();	
}
else if($_GET['command'] == "getPlayLists")
{
	GetPlaylists();
}
else if($_GET['command'] == "getSequences")
{
	GetSequenceFiles();	
}

else if($_GET['command'] == "getPlayListSettings")
{
	GetPlaylistSettings($_GET['pl']);
}
else if($_GET['command'] == "getPlayListEntries")
{
	GetPlaylistEntries($_GET['pl'],$_GET['reload']);
}

else if($_GET['command'] == "setPlayListFirstLast")
{
	SetPlayListFirstLast($_GET['first'],$_GET['last']);
}

else if($_GET['command'] == "addPlayList")
{
	AddPlaylist($_GET['pl']);
}
else if($_GET['command'] == "sort")
{
	PlaylistEntryPositionChanged($_GET['newIndex'],$_GET['oldIndex']);	
}
else if($_GET['command'] == "savePlaylist")
{
	SavePlaylist($_GET['name'],$_GET['first'],$_GET['last']);	
}
else if($_GET['command'] == "deletePlaylist")
{
	DeletePlaylist($_GET['name']);	
}
else if ($_GET['command'] == "deleteEntry")
{
	DeleteEntry($_GET['index']);	
}
else if ($_GET['command'] == "deleteSequence")
{
	DeleteSequence($_GET['name']);	
}
else if ($_GET['command'] == "deleteMusic")
{
	DeleteMusic($_GET['name']);	
}
else if ($_GET['command'] == "addPlaylistEntry")
{
	AddPlayListEntry($_GET['type'],addslashes($_GET['seqFile']),addslashes($_GET['songFile']),$_GET['pause']);	
	$doc = new DomDocument('1.0');
    $root = $doc->createElement('Status');
	$root = $doc->appendChild($root);  
	$value = $doc->createTextNode('Success');
	$value = $root->appendChild($value);
	echo $doc->saveHTML();
}
else if($_GET['command'] == "setUniverseCount")
{
	SetUniverseCount($_GET['count']);
}
else if($_GET['command'] == "getUniverses")
{
	GetUniverses($_GET['reload']);
} 
else if($_GET['command'] == "getPixelnetDMXoutputs")
{
	GetPixelnetDMXoutputs($_GET['reload']);
} 
else if($_GET['command'] == "deleteUniverse")
{
	DeleteUniverse($_GET['index']);
} 
else if($_GET['command'] == "cloneUniverse")
{
	CloneUniverse($_GET['index'],$_GET['numberToClone']);
} 
else if($_GET['command'] == "getSchedule")
{
	GetSchedule($_GET['reload']);
}

else if($_GET['command'] == "addScheduleEntry")
{
	addScheduleEntry();
}

else if ($_GET['command'] == "deleteScheduleEntry")
{
	DeleteScheduleEntry($_GET['index']);
} 

else if($_GET['command'] == "moveFile") 
{
	MoveFile($_GET['file']);
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
	SaveSchedule($_POST['reload']);
}
else if($_GET['command'] == "isFPPDrunning")
{
	IsFPPDrunning();		
}
else if($_GET['command'] == "getFPPstatus")
{
	GetFPPstatus();		
}
else if($_GET['command'] == "stopGracefully")
{
	StopGracefully();		
}
else if($_GET['command'] == "stopNow")
{
	StopNow();		
}
else if($_GET['command'] == "stopFPPD")
{
	StopFPPD();		
}
else if($_GET['command'] == "startFPPD")
{
	StartFPPD();		
}
else if($_GET['command'] == "startPlaylist")
{
	StartPlaylist($_GET['playList'],$_GET['repeat'],$_GET['playEntry']);		
}
else if($_GET['command'] == "rebootPi")
{
	RebootPi();
}
else if($_GET['command'] == "shutdownPi")
{
	ShutdownPi();
}

else if($_GET['command'] == "setVolume")
{
	SetVolume($_GET['volume']);
}




function RebootPi()
{
	$status=exec("sudo shutdown -r now");
	$doc = new DomDocument('1.0');
  $root = $doc->createElement('Status');
	$root = $doc->appendChild($root);  
	$value = $doc->createTextNode($status);
	$value = $root->appendChild($value);
	echo $doc->saveHTML();	
}

function SetVolume($volume)
{
	$status=exec("sudo fpp -v " . $volume);
	$doc = new DomDocument('1.0');
  $root = $doc->createElement('Status');
	$root = $doc->appendChild($root);  
	$value = $doc->createTextNode($status);
	$value = $root->appendChild($value);
	echo $doc->saveHTML();	
}

function ShutdownPi()
{
	$status=exec("sudo shutdown -h now");
	$doc = new DomDocument('1.0');
  $root = $doc->createElement('Status');
	$root = $doc->appendChild($root);  
	$value = $doc->createTextNode($status);
	$value = $root->appendChild($value);
	echo $doc->saveHTML();	
}

function MoveFile($file)
{
	if(file_exists("/home/pi/media/upload/" . $file))
	{
		if (strpos(strtolower($file),".mp3") !== false) 
		{
			if ( !rename("/home/pi/media/upload/" . $file,	"/home/pi/media/music/" . $file) )
			{
				error_log("Couldn't move music file");
				exit(1);
			}
		}
		else
		{
			if ( !rename("/home/pi/media/upload/" . $file,	"/home/pi/media/sequences/" . $file) )
			{
				error_log("Couldn't move music file");
				exit(1);
			}
		}
	}
	else
	{
		error_log("Couldn't find file in upload directory");
		exit(1);
	}
	$doc = new DomDocument('1.0');
	$root = $doc->createElement('Status');
	$root = $doc->appendChild($root);  
	$value = $doc->createTextNode('Success');
	$value = $root->appendChild($value);
	echo $doc->saveHTML();	
}

function IsFPPDrunning()
{
	$status=exec("if ps cax | grep -q fppd; then echo \"true\"; else echo \"false\"; fi");
	$doc = new DomDocument('1.0');
  $root = $doc->createElement('Status');
	$root = $doc->appendChild($root);  
	$value = $doc->createTextNode($status);
	$value = $root->appendChild($value);
	echo $doc->saveHTML();	
}

function StartPlaylist($playlist,$repeat,$playEntry)
{
	if($repeat == "checked")
	{
		$status=exec("sudo fpp -p '" . $playlist . "," . $playEntry . "'");
	}
	else
	{
		$status=exec("sudo fpp -P '" . $playlist . "," . $playEntry . "'");
	}
	$doc = new DomDocument('1.0');
	$root = $doc->createElement('Status');
	$root = $doc->appendChild($root);  
	$value = $doc->createTextNode('true');
	$value = $root->appendChild($value);
	echo $doc->saveHTML();	
}

function StopGracefully()
{
	$status=exec("sudo fpp -S");
	$doc = new DomDocument('1.0');
	$root = $doc->createElement('Status');
	$root = $doc->appendChild($root);  
	$value = $doc->createTextNode('true');
	$value = $root->appendChild($value);
	echo $doc->saveHTML();	
}

function StopNow()
{
	$status=exec("sudo fpp -d");
	$doc = new DomDocument('1.0');
	$root = $doc->createElement('Status');
	$root = $doc->appendChild($root);  
	$value = $doc->createTextNode('true');
	$value = $root->appendChild($value);
	echo $doc->saveHTML();	
}

function StopFPPD()
{
	$status=exec("sudo killall fppd");
	$doc = new DomDocument('1.0');
	$root = $doc->createElement('Status');
	$root = $doc->appendChild($root);  
	$value = $doc->createTextNode('true');
	$value = $root->appendChild($value);
	echo $doc->saveHTML();	
}


function StartFPPD()
{
	$status=exec("if ps cax | grep -q fppd; then echo \"true\"; else echo \"false\"; fi");
	if($status == 'false')
	{
		$status=exec("sudo fppd>/dev/null");
	}
	$doc = new DomDocument('1.0');
	$root = $doc->createElement('Status');
	$root = $doc->appendChild($root);  
	$value = $doc->createTextNode($status);
	$value = $root->appendChild($value);
	echo $doc->saveHTML();	
}
	
	
function GetFPPstatus()
{
	$status=exec("sudo fpp -s");
	if($status == 'false')
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
	if($fppMode == 0)
	{
		$fppStatus = $entry[1];
		if($fppStatus == '0')
		{
			$nextPlaylist = $entry[2];
			$nextPlaylistStartTime = $entry[3];
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
	
}

function GetLocalTime()
{
	return exec("date");
}

function DeleteScheduleEntry($index)
{
	if($index < count($_SESSION['ScheduleEntries']) && count($_SESSION['ScheduleEntries']) > 1 )
	{
		unset($_SESSION['ScheduleEntries'][$index]);
		$_SESSION['ScheduleEntries'] = array_values($_SESSION['ScheduleEntries']);
	
	}
	$doc = new DomDocument('1.0');
  $root = $doc->createElement('Status');
	$root = $doc->appendChild($root);  
	$value = $doc->createTextNode('Success');
	$value = $root->appendChild($value);
	echo $doc->saveHTML();	
}

function addScheduleEntry()
{
		$_SESSION['ScheduleEntries'][] = new ScheduleEntry(1,'',7,0,0,0,7,0,0,0,1);
		$doc = new DomDocument('1.0');
		$root = $doc->createElement('Status');
		$root = $doc->appendChild($root);  
		$value = $doc->createTextNode('Success');
		$value = $root->appendChild($value);
		echo $doc->saveHTML();
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
		$_SESSION['ScheduleEntries'][$i]->endDay = intval($_POST['selDay'][$i]);

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
	
	$doc = new DomDocument('1.0');
  $root = $doc->createElement('Status');
	$root = $doc->appendChild($root);  
	$value = $doc->createTextNode('Success');
	$value = $root->appendChild($value);
	echo $doc->saveHTML();
}

function FPPDreloadSchedule()
{
	$status=exec("sudo fpp -R");
}

function SaveScheduleToFile()
{
	$entries = "";
	$f=fopen("/home/pi/media/schedule","w") or exit("Unable to open file! : " . "/home/pi/media/schedule");
	for($i=0;$i<count($_SESSION['ScheduleEntries']);$i++)
	{
			if($i==0)
			{
			$entries .= sprintf("%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,",
			            $_SESSION['ScheduleEntries'][$i]->enable,
			            $_SESSION['ScheduleEntries'][$i]->playlist,
			            $_SESSION['ScheduleEntries'][$i]->startDay,
			            $_SESSION['ScheduleEntries'][$i]->startHour,
			            $_SESSION['ScheduleEntries'][$i]->startMinute,
			            $_SESSION['ScheduleEntries'][$i]->startSecond,
			            $_SESSION['ScheduleEntries'][$i]->endDay,
			            $_SESSION['ScheduleEntries'][$i]->endHour,
			            $_SESSION['ScheduleEntries'][$i]->endMinute,
			            $_SESSION['ScheduleEntries'][$i]->endSecond,
			            $_SESSION['ScheduleEntries'][$i]->repeat
									);
			}
			else
			{
			$entries .= sprintf("\n%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,",
			            $_SESSION['ScheduleEntries'][$i]->enable,
			            $_SESSION['ScheduleEntries'][$i]->playlist,
			            $_SESSION['ScheduleEntries'][$i]->startDay,
			            $_SESSION['ScheduleEntries'][$i]->startHour,
			            $_SESSION['ScheduleEntries'][$i]->startMinute,
			            $_SESSION['ScheduleEntries'][$i]->startSecond,
			            $_SESSION['ScheduleEntries'][$i]->endDay,
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
	$_SESSION['ScheduleEntries']=NULL;
	
	$f=fopen("/home/pi/media/schedule","r");
	if($f == FALSE)
	{
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
		$endDay = $entry[6];
		$endHour = $entry[7];
		$endMinute = $entry[8];
		$endSecond = $entry[9];
		$repeat = $entry[10];
		$_SESSION['ScheduleEntries'][] = new ScheduleEntry($enable,$playlist,$startDay,$startHour,$startMinute,$startSecond,
		                                      $endDay, $endHour, $endMinute, $endSecond, $repeat);
	}
	fclose($f);
}

function GetSchedule($reload)
{
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

function CloneUniverse($index,$numberToClone)
{
	if($index < count($_SESSION['UniverseEntries']) && ($index + $numberToClone) < count($_SESSION['UniverseEntries']))
	{
  		$universe = $_SESSION['UniverseEntries'][$index]->universe+1;
			$size =  $_SESSION['UniverseEntries'][$index]->size;
			$startAddress =  $_SESSION['UniverseEntries'][$index]->startAddress+$size;
			$type =  $_SESSION['UniverseEntries'][$index]->type;
			$unicastAddress =  $_SESSION['UniverseEntries'][$index]->unicastAddress;
		
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
	$doc = new DomDocument('1.0');
  $root = $doc->createElement('Status');
	$root = $doc->appendChild($root);  
	$value = $doc->createTextNode('Success');
	$value = $root->appendChild($value);
	echo $doc->saveHTML();
}

function DeleteUniverse($index)
{
	if($index < count($_SESSION['UniverseEntries']) && count($_SESSION['UniverseEntries']) > 1 )
	{
		unset($_SESSION['UniverseEntries'][$index]);
		$_SESSION['UniverseEntries'] = array_values($_SESSION['UniverseEntries']);
	
	}
	$doc = new DomDocument('1.0');
  $root = $doc->createElement('Status');
	$root = $doc->appendChild($root);  
	$value = $doc->createTextNode('Success');
	$value = $root->appendChild($value);
	echo $doc->saveHTML();	
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
	
	$doc = new DomDocument('1.0');
  $root = $doc->createElement('Status');
	$root = $doc->appendChild($root);  
	$value = $doc->createTextNode('Success');
	$value = $root->appendChild($value);
	echo $doc->saveHTML();
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
	
	$doc = new DomDocument('1.0');
  $root = $doc->createElement('Status');
	$root = $doc->appendChild($root);  
	$value = $doc->createTextNode('Success');
	$value = $root->appendChild($value);
	echo $doc->saveHTML();
}



function LoadUniverseFile()
{
	$_SESSION['UniverseEntries']=NULL;
	
	$f=fopen("/home/pi/media/universes","r");
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
	$_SESSION['PixelnetDMXentries']=NULL;
	
	$f=fopen("/home/pi/media/pixelnetDMX","r");
	if($f == FALSE)
	{
		fclose($f);
		//No file exists add one  and save to new file.
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
	$universes = "";
	$f=fopen("/home/pi/media/universes","w") or exit("Unable to open file! : " . "/home/pi/media/universes");
	for($i=0;$i<count($_SESSION['UniverseEntries']);$i++)
	{
			if($i==0)
			{
			$entries .= sprintf("%s,%s,%s,%s,%s,%s,",
			            $_SESSION['UniverseEntries'][$i]->active,
			            $_SESSION['UniverseEntries'][$i]->universe,
			            $_SESSION['UniverseEntries'][$i]->startAddress,
			            $_SESSION['UniverseEntries'][$i]->size,
			            $_SESSION['UniverseEntries'][$i]->type,
			            $_SESSION['UniverseEntries'][$i]->unicastAddress);
			}
			else
			{
			$entries .= sprintf("\n%s,%s,%s,%s,%s,%s,",
			            $_SESSION['UniverseEntries'][$i]->active,
			            $_SESSION['UniverseEntries'][$i]->universe,
			            $_SESSION['UniverseEntries'][$i]->startAddress,
			            $_SESSION['UniverseEntries'][$i]->size,
			            $_SESSION['UniverseEntries'][$i]->type,
			            $_SESSION['UniverseEntries'][$i]->unicastAddress);
			}
			                   
	}
	fwrite($f,$entries);	
	fclose($f);

	$doc = new DomDocument('1.0');
	$root = $doc->createElement('Status');
	$root = $doc->appendChild($root); 
	$value = $doc->createTextNode("Success");
	$value = $root->appendChild($value);
	echo $doc->saveHTML();
}


function SavePixelnetDMXoutputsToFile()
{
	$universes = "";
	$f=fopen("/home/pi/media/pixelnetDMX","w") or exit("Unable to open file! : " . "/home/pi/media/pixelnetDMX");
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

	$doc = new DomDocument('1.0');
	$root = $doc->createElement('Status');
	$root = $doc->appendChild($root); 
	$value = $doc->createTextNode("Success");
	$value = $root->appendChild($value);
	echo $doc->saveHTML();
}


function GetUniverses($reload)
{
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

function GetPixelnetDMXoutputs($reload)
{
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

function SetUniverseCount($count)
{
	if($count > 0 && $count <= 128)
	{
			
		$universeCount = count($_SESSION['UniverseEntries']);
		if($universeCount < $count)
		{
			$active = 1;
				$universe = 1;
				$startAddress = 1;
				$size =  512;
				$type =  0;	//Multicast
				$unicastAddress = "";
			if($universeCount == 0)
			{
				$universe = 1;
				$startAddress = 1;
				$size =  512;
				$type =  0;	//Multicast
				$unicastAddress = "";
				
			}
			else
			{
				$universe = $_SESSION['UniverseEntries'][$universeCount-1]->universe+1;
				$size =  $_SESSION['UniverseEntries'][$universeCount-1]->size;
				$startAddress =  $_SESSION['UniverseEntries'][$universeCount-1]->startAddress+$size;
				$type =  $_SESSION['UniverseEntries'][$universeCount-1]->type;
				$unicastAddress =  $_SESSION['UniverseEntries'][$universeCount-1]->unicastAddress;
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
	
	$doc = new DomDocument('1.0');
	$root = $doc->createElement('Status');
	$root = $doc->appendChild($root); 
	$value = $doc->createTextNode("Success");
	$value = $root->appendChild($value);
	echo $doc->saveHTML();
}

function AddPlayListEntry($type,$seqFile,$songFile,$pause)
{
	$_SESSION['playListEntries'][] = new PlaylistEntry($type,$songFile,$seqFile,$pause,$index,count($_SESSION['playListEntries']));
}


function  GetPlaylists()
{
	$FirstList=TRUE;
	$doc = new DomDocument('1.0');
  $root = $doc->createElement('Playlists');
	$root = $doc->appendChild($root);  
	foreach(scandir("/home/pi/media/playlists/") as $pFile) 
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

function  GetMusicFiles()
{
	$musicDirectory = '/home/pi/media/music';
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

function GetSequenceFiles()
{
	$sequenceDirectry = '/home/pi/media/sequences';
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

function AddPlaylist($name)
{
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
		$filename = "/home/pi/media/playlists/" . $name;
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

function SetPlayListFirstLast($first,$last)
{
	$_SESSION['playlist_first'] = $first;
	$_SESSION['playlist_last'] = $last;
	$doc = new DomDocument('1.0');
	$root = $doc->createElement('Status');
	$root = $doc->appendChild($root); 
	$value = $doc->createTextNode("Success");
	$value = $root->appendChild($value);
	echo $doc->saveHTML();
}

function LoadPlayListDetails($file)
{
	$playListEntries = NULL;
	$_SESSION['playListEntries']=NULL;
	
	$f=fopen("/home/pi/media/playlists/" . $file,"rx") or exit("Unable to open file! : " . "/home/pi/media/playlists/" . $file);
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
				break;	
			default:
				break;
			case 'm':
				$songFile = $entry[1];
				$seqFile = "";
				$pause = 0;
				$index = $i;
				break;	
			case 's':
				$songFile = "";
				$seqFile = $entry[1];
				$pause = 0;
				$index = $i;
				break;	
			case 'p':
				$songFile = "";
				$seqFile = "";
				$pause = $entry[1];
				$index = $i;
				break;	
		} 
		$playListEntries[$i] = new PlaylistEntry($type,$songFile,$seqFile,$pause,$index);
		$i++;
	}
	fclose($f);
	$_SESSION['playListEntries'] = $playListEntries;
//	Print_r($_SESSION['playListEntries']);
}

function GetPlayListSettings($file)
{
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

function GetPlaylistEntries($file,$reloadFile)
{
	$_SESSION['currentPlaylist'] = $file;
	if($reloadFile=='true')
	{
		LoadPlayListDetails($file);
	}
	$doc = new DomDocument('1.0');
	// Playlist Entries
  $root = $doc->createElement('PlaylistEntries');
	$root = $doc->appendChild($root);  
//  Print_r($_SESSION['playListEntries']);
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
		// index
		$pause = $doc->createElement('pause');
		$pause = $playListEntry->appendChild($pause);  
		$value = $doc->createTextNode($_SESSION['playListEntries'][$i]->pause);
		$value = $pause->appendChild($value);
		// index
		$index = $doc->createElement('index');
		$index = $playListEntry->appendChild($index);  
		$value = $doc->createTextNode($_SESSION['playListEntries'][$i]->index);
		$value = $index->appendChild($value);

	}			
	echo $doc->saveHTML();
}

function PlaylistEntryPositionChanged($newIndex,$oldIndex)
{
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


function SavePlaylist($name,$first,$last)
{
	$f=fopen("/home/pi/media/playlists/" . $name,"w") or exit("Unable to open file! : " . "/home/pi/media/playlists/" . $name);
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
	}
	fwrite($f,$entries);	
	fclose($f);

	$doc = new DomDocument('1.0');
	$root = $doc->createElement('Status');
	$root = $doc->appendChild($root); 
	$value = $doc->createTextNode("Success");
	$value = $root->appendChild($value);
	echo $doc->saveHTML();


	if($name != $_SESSION['currentPlaylist'])
	{
		 unlink("/home/pi/media/playlists/" . $_SESSION['currentPlaylist']); 	
		 $_SESSION['currentPlaylist'] = $name;
	}
}

function DeletePlaylist($name)
{
	unlink("/home/pi/media/playlists/" . $name);
	$doc = new DomDocument('1.0');
	$root = $doc->createElement('Status');
	$root = $doc->appendChild($root); 
	$value = $doc->createTextNode("Success");
	$value = $root->appendChild($value);
	echo $doc->saveHTML();
}

function DeleteSequence($name)
{
	unlink("/home/pi/media/sequences/" . $name);
	$doc = new DomDocument('1.0');
	$root = $doc->createElement('Status');
	$root = $doc->appendChild($root); 
	$value = $doc->createTextNode("Success");
	$value = $root->appendChild($value);
	echo $doc->saveHTML();
}

function DeleteMusic($name)
{
	unlink("/home/pi/media/music/" . $name);
	$doc = new DomDocument('1.0');
	$root = $doc->createElement('Status');
	$root = $doc->appendChild($root); 
	$value = $doc->createTextNode("Success");
	$value = $root->appendChild($value);
	echo $doc->saveHTML();
}


function DeleteEntry($index)
{
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


?>
