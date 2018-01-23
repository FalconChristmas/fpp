<?php

$skipJSsettings = 1;
require_once('common.php');
require_once('commandsocket.php');
require_once('playlistentry.php');

$a = session_id();
if(empty($a))
{
	session_start();
}
$_SESSION['session_id'] = session_id();


$command_array = Array(
	"getChannelMemMaps"   => 'GetChannelMemMaps',
	"setChannelMemMaps"   => 'SetChannelMemMaps',
	"getChannelRemaps"    => 'GetChannelRemaps',
	"setChannelRemaps"    => 'SetChannelRemaps',
	"getChannelOutputs"   => 'GetChannelOutputs',
	"setChannelOutputs"   => 'SetChannelOutputs',
	"getChannelOutputsJSON" => 'GetChannelOutputsJSON',
	"setChannelOutputsJSON" => 'SetChannelOutputsJSON',
	"applyDNSInfo"        => 'ApplyDNSInfo',
	"getDNSInfo"          => 'GetDNSInfo',
	"setDNSInfo"          => 'SetDNSInfo',
	"getFPPDUptime"       => 'GetFPPDUptime',
	"applyInterfaceInfo"  => 'ApplyInterfaceInfo',
	"getInterfaceInfo"    => 'GetInterfaceInfoJson',
	"setInterfaceInfo"    => 'SetInterfaceInfo',
	"getFPPSystems"       => 'GetFPPSystems',
	"getFPPstatus"		  => 'GetFPPStatusJson',
	"getSetting"          => 'GetSetting',
	"setSetting"          => 'SetSetting',
	"startSequence"       => 'StartSequence',
	"stopSequence"        => 'StopSequence',
	"toggleSequencePause" => 'ToggleSequencePause',
	"singleStepSequence"  => 'SingleStepSequence',
	"singleStepSequenceBack" => 'SingleStepSequenceBack',
	"addPlaylistEntry"    => 'AddPlayListEntry',
	"getPlayListEntries"  => 'GetPlayListEntries',
	"savePlaylist"        => 'SavePlaylist',
	"convertPlaylists"    => 'ConvertPlaylistsToJSON',
	"getPluginSetting"    => 'GetPluginSetting',
	"setPluginSetting"    => 'SetPluginSetting',
	"saveScript"          => 'SaveScript',
	"setTestMode"         => 'SetTestMode',
	"getTestMode"         => 'GetTestMode',
	"setupExtGPIO"        => 'SetupExtGPIOJson',
	"extGPIO"             => 'ExtGPIOJson'
);

$command = "";
$args = Array();

if ( isset($_GET['command']) && !empty($_GET['command']) ) {
	$command = $_GET['command'];
	$args = $_GET;
} else if ( isset($_POST['command']) && !empty($_POST['command']) ) {
	$command = $_POST['command'];
	$args = $_POST;
}

if (array_key_exists($command,$command_array) )
{
	global $debug;

	if ( $debug )
		error_log("Calling " .$command);

	call_user_func($command_array[$command]);
}
return;

/////////////////////////////////////////////////////////////////////////////

function returnJSON($arr) {
	header( "Content-Type: application/json");

	echo json_encode($arr);

	exit(0);
}

/////////////////////////////////////////////////////////////////////////////

function GetSetting()
{
	global $args;

	$setting = $args['key'];
	check($setting, "setting", __FUNCTION__);

	$value = ReadSettingFromFile($setting);

	$result = Array();
	$result[$setting] = $value;

	returnJSON($result);
}

function SetSetting()
{
	global $args, $SUDO;

	$setting = $args['key'];
	$value   = $args['value'];

	check($setting, "setting", __FUNCTION__);
	check($value, "value", __FUNCTION__);

	WriteSettingToFile($setting, $value);

	if ($setting == "LogLevel") {
		SendCommand("LogLevel,$value,");
	} else if ($setting == "LogMask") {
		$newValue = "";
		if ($value != "")
			$newValue = preg_replace("/,/", ";", $value);
		SendCommand("LogMask,$newValue,");
	} else if ($setting == "HostName") {
		$value = preg_replace("/[^a-zA-Z0-9]/", "", $value);
		exec(	$SUDO . " sed -i 's/^.*\$/$value/' /etc/hostname ; " .
			$SUDO . " sed -i '/^127.0.1.1[^0-9]/d' /etc/hosts ; " .
			$SUDO . " sed -i '\$a127.0.1.1 $value' /etc/hosts ; " .
			$SUDO . " hostname $value ; " .
			$SUDO . " /etc/init.d/avahi-daemon restart ;" .
			$SUDO . " systemctl restart avahi-daemon.service",
			$output, $return_val);
		sleep(1); // Give Avahi time to restart before we return
	} else if ($setting == "EnableRouting") {
		if ($value != "1")
		{
			$value = "0";
		}
		exec(	$SUDO . " sed -i '/net.ipv4.ip_forward/d' /etc/sysctl.conf; " .
			$SUDO . " sed -i '\$anet.ipv4.ip_forward = $value' /etc/sysctl.conf ; " .
			$SUDO . " sysctl --system",
			$output, $return_val);
	} else if ($setting == "storageDevice") {
		exec('mount | grep boot | cut -f1 -d" " | sed -e "s/\/dev\///" -e "s/p[0-9]$//"', $output, $return_val);
		$bootDevice = $output[0];
		unset($output);

		if (preg_match("/$bootDevice/", $value)) {
			exec(	$SUDO . " sed -i 's/.*home\/fpp\/media/#\/dev\/sda1    \/home\/fpp\/media/' /etc/fstab", $output, $return_val );
		} else {
			exec(	$SUDO . " sed -i 's/.*home\/fpp\/media/\/dev\/$value    \/home\/fpp\/media/' /etc/fstab", $output, $return_val );
		}
	} else if ($setting == "AudioOutput") {
		SetAudioOutput($value);
	} else if ($setting == "ForceHDMI") {
		if ($value)
		{
			exec( $SUDO . " sed -i '/hdmi_force_hotplug/d' /boot/config.txt; " .
				$SUDO . " sed -i '\$ahdmi_force_hotplug=1' /boot/config.txt",
				$output, $return_val);
		}
		else
		{
			exec( $SUDO . " sed -i '/hdmi_force_hotplug/d' /boot/config.txt; " .
				$SUDO . " sed -i '\$a#hdmi_force_hotplug=1' /boot/config.txt",
				$output, $return_val);
		}
	} else {
		SendCommand("SetSetting,$setting,$value,");
	}

	GetSetting();
}

function GetFPPDUptime()
{
	$result = Array();

	$uptimeStr = SendCommand("GetFPPDUptime");
	$uptime = explode(',', $uptimeStr)[3];

	$days = $uptime / 86400;
	$hours = $uptime % 86400 / 3600;
	$seconds = $uptime % 86400 % 3600;
	$minutes = $seconds / 60;
	$seconds = $seconds % 60;

	$result['uptime'] = $uptime;
	$result['days'] = $days;
	$result['hours'] = $hours;
	$result['minutes'] = $minutes;
	$result['seconds'] = $seconds;

	$result['uptimeStr'] = 
		sprintf($f, "%d days, %d hours, %d minutes, %d seconds\n",
		$days, $hours, $minutes, $seconds );

	returnJSON($result);
}

function GetFPPStatusJson()
{
	global $args;

	if (isset($args['ip']))
	{
		header( "Content-Type: application/json");

		echo file_get_contents("http://" . $args['ip'] . "/fppjson.php?command=getFPPstatus");

		exit(0);
	}
	else
	{
		$status = SendCommand('s');
  
		if($status == false || $status == 'false') {
     	
			$status=exec("if ps cax | grep -q git_pull; then echo \"updating\"; else echo \"false\"; fi");
     
			returnJSON([
					'fppd' => 'Not Running',
					'status' => -1,
					'status_name' => $status == 'updating' ? $status : 'stopped',
					'current_playlist' => [
						'playlist' => '',
						'type'     => '',
						'index'    => '0',
						'count'    => '0'
					],
					'current_sequence'  => '',
					'current_song'      => '',
					'seconds_played'    => '0',
					'seconds_remaining' => '0',
					'time_elapsed'      => '00:00',
					'time_remaining'    => '00:00',
				]);
		}

		$data = parseStatus($status);

		returnJson($data);
	}
}

function parseStatus($status) 
{
	$modes = [
		0 => 'unknown',
        1 => 'bridge',
        2 => 'player',
        6 => 'master',
        8 => 'remote'
        ];

    $statuses = [
    	0 => 'idle',
    	1 => 'playing',
    	2 => 'stopping gracefully'
    ];

	$status = explode(',', $status, 14);
	$mode = (int) $status[0]; 
	$fppStatus = (int) $status[1];

	if($mode == 1) {
		return [
			'fppd'   => 'running',
			'mode'   => $mode,
			'mode_name' => $modes[$mode],
			'status' => $fppStatus,
		];
	}

	$baseData = [
		'fppd'        => 'running',
		'mode'        => $mode,
		'mode_name'   => $modes[$mode],
		'status'      => $fppStatus,
		'status_name' => $statuses[$fppStatus],
		'volume'      => (int) $status[2],
		'time'        => exec('date'),
		'current_playlist' => [
			'playlist' => '',
			'type'     => '',
			'index'    => '0',
			'count'    => '0'
		],
		'current_sequence'  => '',
		'current_song'      => '',
		'seconds_played'    => '0',
		'seconds_remaining' => '0',
		'time_elapsed'      => '00:00',
		'time_remaining'    => '00:00',
   ];

	if($mode == 8) {
		$data = [
			'playlist'          => $status[3],
			'sequence_filename' => $status[3],
			'media_filename'    => $status[4],
			'seconds_elapsed'   => $status[5],
			'seconds_remaining' => $status[6],
			'time_elapsed' 		=> parseTimeFromSeconds((int)$status[5]),
			'time_remaining'	=> parseTimeFromSeconds((int)$status[6]),
	    ];

	} else {

		if($fppStatus == 0) {
			$data = [
				'next_playlist'     => [
					'playlist'   => $status[3],
					'start_time' => $status[4]
				],
				'repeat_mode' => 0,
			];
		} else {

			if ($status[4] == 's')
				$status[6] = '';

			$data = [
				'current_playlist' => [
					'playlist' => pathinfo($status[3])['filename'],
					'type'     => $status[4],
					'index'    => $status[7],
					'count'    => $status[8]
					],
				'current_sequence'  => $status[5],
				'current_song'      => $status[6],
				'seconds_played'    => $status[9],
				'seconds_remaining' => $status[10],
				'time_elapsed' 		=> parseTimeFromSeconds((int)$status[9]),
				'time_remaining' 	=> parseTimeFromSeconds((int)$status[10]),
				'next_playlist'     => [
					'playlist'   => $status[11],
					'start_time' => $status[12]
				],
				'repeat_mode' => (int)$status[13],
			];
		}
	}

	return array_merge($baseData, $data);

}

function parseTimeFromSeconds($seconds) {
	
	if(!is_numeric($seconds)) {
		return;
	}
	
	$minutes = (int) floor($seconds/60);
	$seconds = (int) $seconds % 60;


	return sprintf('%s:%s', str_pad($minutes, 2, 0, STR_PAD_LEFT), str_pad($seconds, 2, 0, STR_PAD_LEFT));
}

function getTimeRemaining($seconds) {

}


function StartSequence()
{
	global $args;

	$sequence = $args['sequence'];
	$startSecond = $args['startSecond'];

	check($sequence, "sequence", __FUNCTION__);
	check($startSecond, "startSecond", __FUNCTION__);

	SendCommand(sprintf("StartSequence,%s,%d", $sequence, $startSecond));
}

function StopSequence()
{
	SendCommand("StopSequence");
}

function ToggleSequencePause()
{
	SendCommand("ToggleSequencePause");
}

function SingleStepSequence()
{
	SendCommand("SingleStepSequence");
}

function SingleStepSequenceBack()
{
	SendCommand("SingleStepSequenceBack");
}

function GetJSONPlaylistEntry($entry, $index)
{
	if ($entry->type == 'both')
		return new PlaylistEntry($entry->type,$entry->mediaName,$entry->sequenceName,0,'','','','', $entry, $index);
	else if ($entry->type == 'media')
		return new PlaylistEntry($entry->type,$entry->mediaName,'',0,'','','','', $entry, $index);
	else if ($entry->type == 'sequence')
		return new PlaylistEntry($entry->type,'',$entry->sequenceName,0,'','','','', $entry, $index);
	else if ($entry->type == 'pause')
		return new PlaylistEntry($entry->type,'', '',$entry->duration,'','','','', $entry, $index);
	else if ($entry->type == 'script')
		return new PlaylistEntry($entry->type,'', '', 0, $entry->scriptName,'','','', $entry, $index);
	else if ($entry->type == 'event')
	{
		$majorID = $entry->majorID;
		if ($majorID < 10)
			$majorID = '0' . $majorID;

		$minorID = $entry->minorID;
		if ($minorID < 10)
			$minorID = '0' . $minorID;

		$id = $majorID . '_' . $minorID;

		AddEventDesc($entry);

		return new PlaylistEntry($entry->type,'', '', 0, '', $id, $entry->desc, '', $entry, $index);
	}
	else if ($entry->type == 'plugin')
		return new PlaylistEntry($entry->type, '', '', 0, '', '', '', $entry->data, $entry, $index);
	else
		return new PlaylistEntry($entry->type,'', '', 0, '','','','', $entry, $index);
}

function AddEventDesc(&$entry)
{
	global $settings;

	if ($entry->type != 'event')
		return;

	$majorID = $entry->majorID;
	if ($majorID < 10)
		$majorID = '0' . $majorID;

	$minorID = $entry->minorID;
	if ($minorID < 10)
		$minorID = '0' . $minorID;

	$id = $majorID . '_' . $minorID;

	$eventFile = $settings['eventDirectory'] . "/" . $id . ".fevt";
	if ( file_exists($eventFile)) {
		$eventInfo = parse_ini_file($eventFile);
		$entry->desc = $eventInfo['name'];
	} else {
		$entry->desc = "ERROR: Undefined Event: " . $id;
	}
}

function LoadCSVPlayListDetails($file)
{
	global $settings;

	$playListEntriesLeadIn = NULL;
	$playListEntriesMainPlaylist = NULL;
	$playListEntriesLeadOut = NULL;
	$_SESSION['playListEntriesLeadIn']=NULL;
	$_SESSION['playListEntriesMainPlaylist']=NULL;
	$_SESSION['playListEntriesLeadOut']=NULL;

	$f = fopen($settings['playlistDirectory'] . '/' . $file, "rx")
		or exit("Unable to open file! : " . $settings['playlistDirectory'] . '/' . $file);

	$firstIsLeadIn = 0;
	$lastIsLeadOut = 0;
	$index = 0;
	$sectionIndex = 0;
	$i=0;

	$line = fgets($f);
	if(strlen($line)) {
		$entry = explode(",",$line,50);
		$firstIsLeadIn = $entry[0];
		$lastIsLeadOut = $entry[1];
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

		$e = new \stdClass;
		$e->enabled = 1;
		$e->playOnce = 0;

		switch($entry[0])
		{
			case 'b':
				$e->type = 'both';
				$e->sequenceName = $entry[1];
				$e->mediaName = $entry[2];
				break;
			case 'm':
				$e->type = 'media';
				$e->mediaName = $entry[1];
				break;
			case 's':
				$e->type = 'sequence';
				$e->sequenceName = $entry[1];
				break;
			case 'p':
				$e->type = 'pause';
				$e->duration = (int)$entry[1];
				break;
			case 'P':
				$e->type = 'plugin';
				$e->data = $entry[1];
				break;
			case 'e':
				$e->type = 'event';
				$e->majorID = (int)substr($entry[1],0,2);
				$e->minorID = (int)substr($entry[1],3,2);
				break;
			default:
				break;
		}

		if (($index == 0) && ($firstIsLeadIn))
		{
			$playListEntriesLeadIn[0] = GetJSONPlaylistEntry($e, $sectionIndex);
			$sectionIndex = 0;
		}
		else
		{
			$playListEntriesMainPlaylist[$sectionIndex]
				= GetJSONPlaylistEntry($e, $sectionIndex);
			$sectionIndex++;
		}

		$index++;
	}
	fclose($f);

	if ($lastIsLeadOut)
	{
		$e = array_pop($playListEntriesMainPlaylist);
		$playListEntriesLeadOut[0] = $e;
	}

	$_SESSION['playListEntriesLeadIn'] = $playListEntriesLeadIn;
	$_SESSION['playListEntriesMainPlaylist'] = $playListEntriesMainPlaylist;
	$_SESSION['playListEntriesLeadOut'] = $playListEntriesLeadOut;
}

function LoadPlayListDetails($file)
{
	global $settings;

	$playListEntriesLeadIn = NULL;
	$playListEntriesMainPlaylist = NULL;
	$playListEntriesLeadOut = NULL;
	$_SESSION['playListEntriesLeadIn']=NULL;
	$_SESSION['playListEntriesMainPlaylist']=NULL;
	$_SESSION['playListEntriesLeadOut']=NULL;
	$_SESSION['currentPlaylist'] = '';

	$jsonStr = "";

	if (!file_exists($settings['playlistDirectory'] . '/' . $file . ".json"))
	{
		if (file_exists($settings['playlistDirectory'] . '/' . $file))
			LoadCSVPlaylistDetails($file);

		return;
	}

	$jsonStr = file_get_contents($settings['playlistDirectory'] . '/' . $file . ".json");

	$data = json_decode($jsonStr);

	if (isset($data->leadIn))
	{
		$i = 0;
		foreach ($data->leadIn as $entry)
		{
			$playListEntriesLeadIn[$i] = GetJSONPlaylistEntry($entry, $i);
			$i++;
		}
	}

	if (isset($data->mainPlaylist))
	{
		$i = 0;
		foreach ($data->mainPlaylist as $entry)
		{
			$playListEntriesMainPlaylist[$i] = GetJSONPlaylistEntry($entry, $i);
			$i++;
		}
	}

	if (isset($data->leadOut))
	{
		$i = 0;
		foreach ($data->leadOut as $entry)
		{
			$playListEntriesLeadOut[$i] = GetJSONPlaylistEntry($entry, $i);
			$i++;
		}
	}

	$_SESSION['playListEntriesLeadIn'] = $playListEntriesLeadIn;
	$_SESSION['playListEntriesMainPlaylist'] = $playListEntriesMainPlaylist;
	$_SESSION['playListEntriesLeadOut'] = $playListEntriesLeadOut;
	$_SESSION['currentPlaylist'] = $file;
}

function AddPlayListEntry()
{
	$entry = json_decode($_POST['data']);

	$_SESSION['playListEntriesMainPlaylist'][] =
		new PlaylistEntry($entry->type,'', '', 0, '','','','', $entry,
			count($_SESSION['playListEntriesMainPlaylist']));

	$result = Array();
	$result['status'] = 'success';
	$result['json'] = $_POST['data'];

	returnJSON($result);
}

function ConvertPlaylistsToJSON()
{
	global $settings;

	$result = Array();
	$playlistDirectory = $settings['playlistDirectory'];

	if (!file_exists($playlistDirectory))
	{
		$result['status'] = 'Error';
		$result['msg'] = 'Playlist Directory ' + $playlistDirectory + ' does not exist.';

		returnJSON($result);
		return;
	}

	$result['status'] = 'Ok';
	$result['playlists'] = Array();

	foreach(scandir($playlistDirectory) as $playlist)
	{
		if ($playlist != "." && $playlist != "..")
		{
			if ((!preg_match("/-CSV$/", $playlist)) &&
				(!preg_match("/\.json$/", $playlist)))
			{
				$playlist = preg_replace("/\.json/", "", $playlist);

				$_SESSION['currentPlaylist'] = $playlist;
				LoadPlayListDetails($playlist);

				SavePlaylistRaw($playlist);

				$result['playlists'][] = $playlist;
			}
		}
	}

	$_SESSION['currentPlaylist'] = '';

	returnJSON($result);
}

function GetPlayListEntries()
{
	global $args;
	global $settings;

	$playlist = $args['pl'];
	check($playlist, "playlist", __FUNCTION__);
	$reload = $args['reload'];
	check($playlist, "reload", __FUNCTION__);

	if ($reload == 'true')
		LoadPlayListDetails($playlist);

	$jsonStr = GenerateJSONPlaylist($playlist);

	$jsonStr = json_encode(json_decode($jsonStr), JSON_PRETTY_PRINT);

	header( "Content-Type: application/json");
	echo $jsonStr;
}

function GenerateJSONPlaylistEntry($entry)
{
	$result = json_encode($entry->entry, JSON_PRETTY_PRINT);
	return $result;

	$result = "";

	// FIXME Playlist, delete this stuff below here
	if ($entry->type == 'both')
	{
		$result = sprintf(
			'		{' . "\n" .
			'			"type": "both",' . "\n" .
			'			"enabled": 1,' . "\n" .
			'			"playOnce": %d,' . "\n" .
			'			"sequenceName": "%s",' . "\n" .
			'			"mediaName": "%s"' . "\n" .
			'		}',
			0, // Play Once
			$entry->seqFile, $entry->songFile
			);
	}
	else if ($entry->type == 'sequence')
	{
		$result = sprintf(
			'		{' . "\n" .
			'			"type": "sequence",' . "\n" .
			'			"enabled": 1,' . "\n" .
			'			"playOnce": %d,' . "\n" .
			'			"sequenceName": "%s"' . "\n" .
			'		}',
			0, // Play Once
			$entry->seqFile
			);
	}
	else if ($entry->type == 'media')
	{
		$result = sprintf(
			'		{' . "\n" .
			'			"type": "media",' . "\n" .
			'			"enabled": 1,' . "\n" .
			'			"playOnce": %d,' . "\n" .
			'			"mediaName": "%s"' . "\n" .
			'		}',
			0, // Play Once
			$entry->songFile
			);
	}
	else if ($entry->type == 'pause')
	{
		$result = sprintf(
			'		{' . "\n" .
			'			"type": "pause",' . "\n" .
			'			"enabled": 1,' . "\n" .
			'			"playOnce": %d,' . "\n" .
			'			"duration": %d' . "\n" .
			'		}',
			0, // Play Once
			$entry->pause
			);
	}
	else if ($entry->type == 'script')
	{
		$result = sprintf(
			'		{' . "\n" .
			'			"type": "script",' . "\n" .
			'			"enabled": 1,' . "\n" .
			'			"playOnce": %d,' . "\n" .
			'			"scriptName": "%s",' . "\n" .
			'			"blocking": %d' . "\n" .
			'		}',
			0, // Play Once
			$entry->scriptName,
			0  // Blocking
			);
	}
	else if ($entry->type == 'volume')
	{
		$result = sprintf(
			'		{' . "\n" .
			'			"type": "volume",' . "\n" .
			'			"enabled": 1,' . "\n" .
			'			"playOnce": %d,' . "\n" .
			'			"volume": %d' . "\n" .
			'		}',
			0, // Play Once
			$entry->pause
			);
	}
	else if ($entry->type == 'brightness')
	{
		$result = sprintf(
			'		{' . "\n" .
			'			"type": "brightness",' . "\n" .
			'			"enabled": 1,' . "\n" .
			'			"playOnce": %d,' . "\n" .
			'			"brightness": %d' . "\n" .
			'		}',
			0, // Play Once
			$entry->pause
			);
	}
	else if ($entry->type == 'event')
	{
		$majorID = intval(preg_replace('/_.*/', '', $entry->eventID));
		$minorID = intval(preg_replace('/.*_/', '', $entry->eventID));

		$result = sprintf(
			'		{' . "\n" .
			'			"type": "event",' . "\n" .
			'			"enabled": 1,' . "\n" .
			'			"playOnce": %d,' . "\n" .
			'			"majorID": %d,' . "\n" .
			'			"minorID": %d,' . "\n" .
			'			"blocking": %d' . "\n" .
			'		}',
			0, // Play Once
			$majorID, $minorID,
			0  // Blocking
			);
	}
	else if ($entry->type == 'plugin')
	{
		$result = sprintf(
			'		{' . "\n" .
			'			"type": "plugin",' . "\n" .
			'			"enabled": 1,' . "\n" .
			'			"data": "%s"' . "\n" .
			'		}',
			0, // Play Once
			$entry->pluginData
			);
	}
	else
	{
		$result = json_encode($entry->entry);
	}

	return $result;
}

function GenerateJSONPlaylistSection($section, $list)
{
	if (!count($_SESSION[$list]))
		return "";

	$result = sprintf(',' . "\n" .
		'	"' . $section . '": [' . "\n"
		);

	$i = 0;
	$firstEntry = 1;
	while ($i < count($_SESSION[$list]))
	{
		if ($firstEntry)
		{
			$firstEntry = 0;
		}
		else
		{
			$result .= sprintf(",\n");
		}
		$result .= GenerateJSONPlaylistEntry($_SESSION[$list][$i++]);
	}

	$result .= sprintf("\n" .
		'	]'
		);

	return $result;
}

function GenerateJSONPlaylist($name)
{
	$result = "";

	$result .= sprintf(
		'{' . "\n" .
		'	"name": "%s",' . "\n" .
		'	"repeat": %d,' . "\n" .
		'	"loopCount": %d',
		$name,
		0, // Repeat
		0  // Loop Count
		);

	$result .= GenerateJSONPlaylistSection('leadIn', 'playListEntriesLeadIn');
	$result .= GenerateJSONPlaylistSection('mainPlaylist', 'playListEntriesMainPlaylist');
	$result .= GenerateJSONPlaylistSection('leadOut', 'playListEntriesLeadOut');

	$result .= sprintf("\n}\n");

	return $result;
}

function SavePlaylistRaw($name)
{
	global $playlistDirectory;

	$json = GenerateJSONPlaylist($name);

	// Make sure the whole string is pretty
	$json = json_encode(json_decode($json), JSON_PRETTY_PRINT);

	// Rename any old CSV style playlist if it exists
	if (file_exists($playlistDirectory . '/' . $name))
		rename($playlistDirectory . '/' . $name, $playlistDirectory . '/' . $name . '-CSV');

	$f = fopen($playlistDirectory . '/' . $name . ".json","w") or exit("Unable to open file! : " . $playlistDirectory . '/' . $name . ".json");
	fprintf($f, $json);
	fclose($f);

	if(isset($_SESSION['currentPlaylist']) && $name != $_SESSION['currentPlaylist'])
	{
		 unlink($playlistDirectory . '/' . $_SESSION['currentPlaylist'] . ".json");
	}
	$_SESSION['currentPlaylist'] = $name;
}

function SavePlaylist()
{
	$name = $_GET['name'];
	check($name, "name", __FUNCTION__);

	SavePlaylistRaw($name);

	$result = Array();
	$result['status'] = 'Ok';
	returnJSON($result);
}

function GetPluginSetting()
{
	global $args;

	$setting = $args['key'];
	$plugin  = $args['plugin'];
	check($setting, "setting", __FUNCTION__);
	check($plugin, "plugin", __FUNCTION__);

	$value = ReadSettingFromFile($setting, $plugin);

	$result = Array();
	$result[$setting] = $value;

	returnJSON($result);
}

function SetPluginSetting()
{
	global $args;

	$setting = $args['key'];
	$value   = $args['value'];
	$plugin  = $args['plugin'];

	check($setting, "setting", __FUNCTION__);
	check($value, "value", __FUNCTION__);
	check($plugin, "plugin", __FUNCTION__);

	WriteSettingToFile($setting, $value, $plugin);

	GetPluginSetting();
}

/////////////////////////////////////////////////////////////////////////////

function GetFPPSystems()
{
	exec("ip addr show up | grep 'inet ' | awk '{print $2}' | cut -f1 -d/ | grep -v '^127'", $localIPs);

	exec("avahi-browse -artp | grep -v 'IPv6' | sort", $rmtSysOut);

	$result = Array();

	foreach ($rmtSysOut as $system)
	{
		if (!preg_match("/^=.*fpp-fppd/", $system))
			continue;
		if (!preg_match("/fppMode/", $system))
			continue;

		$parts = explode(';', $system);

		$elem = Array();
		$elem['HostName']        = $parts[3];
		$elem['IP']     = $parts[7];
		$elem['fppMode'] = "Unknown";
		$elem['Local'] = 0;
		$elem['Platform'] = "Unknown";

		$matches = preg_grep("/" . $elem['IP'] . "/", $localIPs);
		if (count($matches))
			$elem['Local'] = 1;

		if (count($parts) > 8)
		{
			$elem['txtRecord'] = $parts[9];
			$txtParts = explode(',', preg_replace("/\"/", "", $parts[9]));
			foreach ($txtParts as $txtPart)
			{
				$kvPair = explode('=', $txtPart);
				if ($kvPair[0] == "fppMode")
					$elem['fppMode'] = $kvPair[1];
				else if ($kvPair[0] == "platform")
					$elem['Platform'] = $kvPair[1];
			}
	 }

		if (!((($elem['IP'] == "192.168.7.2") || ($elem['IP'] == "192.168.6.2")) && ($elem['Platform'] == "BeagleBone Black")))
		{
			$result[] = $elem;
		}
	}

	returnJSON($result);
}

/////////////////////////////////////////////////////////////////////////////

function SetAudioOutput($card)
{
	global $args, $SUDO, $debug;

	if ($card != 0 && file_exists("/proc/asound/card$card"))
	{
		exec($SUDO . " sed -i 's/card [0-9]/card ".$card."/' /root/.asoundrc", $output, $return_val);
		unset($output);
		if ($return_val)
		{
			error_log("Failed to set audio to card $card!");
			return;
		}
		if ( $debug )
			error_log("Setting to audio output $card");
	}
	else if ($card == 0)
	{
		exec($SUDO . " sed -i 's/card [0-9]/card ".$card."/' /root/.asoundrc", $output, $return_val);
		unset($output);
		if ($return_val)
		{
			error_log("Failed to set audio back to default!");
			return;
		}
		if ( $debug )
			error_log("Setting default audio");
	}

	return $card;
}

/////////////////////////////////////////////////////////////////////////////

function GetChannelMemMaps()
{
	global $settings;
	$memmapFile = $settings['channelMemoryMapsFile'];

	$result = Array();

	$f = fopen($memmapFile, "r");
	if($f == FALSE)
	{
		fclose($f);
		returnJSON($result);
	}

	while (!feof($f))
	{
		$line = trim(fgets($f));

		if ($line == "")
			continue;
		
		if (substr($line, 0, 1) == "#")
			continue;

		$memmap = explode(",",$line,10);

		$elem = Array();
		$elem['BlockName']        = $memmap[0];
		$elem['StartChannel']     = $memmap[1];
		$elem['ChannelCount']     = $memmap[2];
		$elem['Orientation']      = $memmap[3];
		$elem['StartCorner']      = $memmap[4];
		$elem['StringCount']      = $memmap[5];
		$elem['StrandsPerString'] = $memmap[6];

		$result[] = $elem;
	}
	fclose($f);

	returnJSON($result);
}

function SetChannelMemMaps()
{
	global $args;
	global $settings;

	$memmapFile = $settings['channelMemoryMapsFile'];

	$data = json_decode($args['data'], true);

	$f = fopen($memmapFile, "w");
	if($f == FALSE)
	{
		fclose($f);
		returnJSON($result);
	}

	foreach ($data as $memmap) {
		fprintf($f, "%s,%d,%d,%s,%s,%d,%d\n",
			$memmap['BlockName'], $memmap['StartChannel'],
			$memmap['ChannelCount'], $memmap['Orientation'],
			$memmap['StartCorner'], $memmap['StringCount'],
			$memmap['StrandsPerString']);
	}
	fclose($f);

	GetChannelMemMaps();
}

/////////////////////////////////////////////////////////////////////////////

function GetChannelRemaps()
{
	global $remapFile;

	$result = Array();

	$f = fopen($remapFile, "r");
	if($f == FALSE)
	{
		fclose($f);
		returnJSON($result);
	}

	while (!feof($f))
	{
		$line = trim(fgets($f));

		if ($line == "")
			continue;
		
		if (substr($line, 0, 1) == "#")
			continue;

		$remap = explode(",",$line,10);

		$elem = Array();
		$elem['Source'] = $remap[0];
		$elem['Destination'] = $remap[1];
		$elem['Count'] = $remap[2];

		$result[] = $elem;
	}
	fclose($f);

	returnJSON($result);
}

function SetChannelRemaps()
{
	global $remapFile;
	global $args;

	$data = json_decode($args['data'], true);

	$f = fopen($remapFile, "w");
	if($f == FALSE)
	{
		fclose($f);
		returnJSON($result);
	}

	foreach ($data as $remap) {
		fprintf($f, "%d,%d,%d\n",
			$remap['Source'], $remap['Destination'], $remap['Count']);
	}
	fclose($f);

	GetChannelRemaps();
}

/////////////////////////////////////////////////////////////////////////////
function GetChannelOutputsJSON()
{
	global $settings;

	$jsonStr = "";

	if (file_exists($settings['channelOutputsJSON'])) {
		$jsonStr = file_get_contents($settings['channelOutputsJSON']);
	}

	header( "Content-Type: application/json");
	echo $jsonStr;
}

function SetChannelOutputsJSON()
{
	global $settings;
	global $args;

	$data = stripslashes($args['data']);
	$data = prettyPrintJSON(substr($data, 1, strlen($data) - 2));

	file_put_contents($settings['channelOutputsJSON'], $data);

	GetChannelOutputsJSON();
}

/////////////////////////////////////////////////////////////////////////////
function GetChannelOutputs()
{
	global $settings;

	$result = Array();
	$result['Outputs'] = Array();

	if (!file_exists($settings['channelOutputsFile']))
		return returnJSON($result);

	$f = fopen($settings['channelOutputsFile'], "r");
	if($f == FALSE)
	{
		fclose($f);
		returnJSON($result);
	}

	while (!feof($f))
	{
		$line = trim(fgets($f));

		if ($line == "")
			continue;

		array_push($result['Outputs'], $line);
	}
	fclose($f);

	return returnJSON($result);
}

function SetChannelOutputs()
{
	global $settings;
	global $args;

	$data = json_decode($args['data'], true);

	$f = fopen($settings['channelOutputsFile'], "w");
	if($f == FALSE)
	{
		fclose($f);
		returnJSON($result);
	}

	foreach ($data['Outputs'] as $output) {
		fprintf($f, "%s\n", $output);
	}
	fclose($f);

	GetChannelOutputs();
}

/////////////////////////////////////////////////////////////////////////////
// Network Interface configuration
function ApplyInterfaceInfo()
{
	global $settings, $args, $SUDO;

	$interface = $args['interface'];

	exec($SUDO . " " . $settings['fppDir'] . "/scripts/config_network  $interface");
}


function GetInterfaceInfoJson()
{
	global $settings;
	global $args;

	$interface = $args['interface'];

	check($interface);

	$result = Array();

	$cfgFile = $settings['configDirectory'] . "/interface." . $interface;
	if (file_exists($cfgFile)) {
		$result = parse_ini_file($cfgFile);
	}

	exec("/sbin/ifconfig $interface", $output);
	foreach ($output as $line)
	{
		if (preg_match('/addr:/', $line))
		{
			$result['CurrentAddress'] = preg_replace('/.*addr:([0-9\.]+) .*/', '$1', $line);
			$result['CurrentNetmask'] = preg_replace('/.*Mask:([0-9\.]+).*/', '$1', $line);
		}
	}
	unset($output);

	if (substr($interface, 0, 4) == "wlan")
	{
		exec("/sbin/iwconfig $interface", $output);
		foreach ($output as $line)
		{
			if (preg_match('/ESSID:/', $line))
				$result['CurrentSSID'] = preg_replace('/.*ESSID:"([^"]+)".*/', '$1', $line);

			if (preg_match('/Rate:/', $line))
				$result['CurrentRate'] = preg_replace('/.*Bit Rate:([0-9\.]+) .*/', '$1', $line);
		}
		unset($output);
	}

	returnJSON($result);
}

function SetInterfaceInfo()
{
	global $settings;
	global $args;

	$data = json_decode($args['data'], true);

	$cfgFile = $settings['configDirectory'] . "/interface." . $data['INTERFACE'];

	$f = fopen($cfgFile, "w");
	if ($f == FALSE) {
		return;
	}

	if ($data['PROTO'] == "static") {
		fprintf($f,
			"INTERFACE=\"%s\"\n" .
			"PROTO=\"static\"\n" .
			"ADDRESS=\"%s\"\n" .
			"NETMASK=\"%s\"\n" .
			"GATEWAY=\"%s\"\n",
			$data['INTERFACE'], $data['ADDRESS'], $data['NETMASK'],
			$data['GATEWAY']);

	} else if ($data['PROTO'] == "dhcp") {
		fprintf($f,
			"INTERFACE=%s\n" .
			"PROTO=dhcp\n",
			$data['INTERFACE']);
	}

	if (substr($data['INTERFACE'], 0, 4) == "wlan")
	{
		fprintf($f,
			"SSID='%s'\n" .
			"PSK='%s'\n",
			$data['SSID'], $data['PSK']);
	}

	fclose($f);
}

/////////////////////////////////////////////////////////////////////////////
function ApplyDNSInfo()
{
	global $settings, $SUDO;

	exec($SUDO . " " . $settings['fppDir'] . "/scripts/config_dns");
}

function GetDNSInfo()
{
	global $settings;

	$cfgFile = $settings['configDirectory'] . "/dns";
	if (file_exists($cfgFile)) {
		returnJSON(parse_ini_file($cfgFile));
	}

	returnJSON(Array());
}

function SetDNSInfo()
{
	global $settings;
	global $args;

	$data = json_decode($args['data'], true);

	$cfgFile = $settings['configDirectory'] . "/dns";

	$f = fopen($cfgFile, "w");
	if ($f == FALSE) {
		return;
	}

	fprintf($f,
		"DNS1=\"%s\"\n" .
		"DNS2=\"%s\"\n",
		$data['DNS1'], $data['DNS2']);
	fclose($f);
}

/////////////////////////////////////////////////////////////////////////////

function SaveScript()
{
	global $args;
	global $settings;

	$result = Array();

	if (!isset($args['data']))
	{
		$result['saveStatus'] = "Error, incorrect info";
		returnJSON($result);
	}

	$data = json_decode($args['data'], true);

	if (isset($data['scriptName']) && isset($data['scriptBody']))
	{
		$filename = $settings['scriptDirectory'] . '/' . $data['scriptName'];
		$content = $data['scriptBody'];

		if (file_exists($filename))
		{
			if (@file_put_contents($filename, $content))
			{
				$result['saveStatus'] = "OK";
				$result['scriptName'] = $data['scriptName'];
				$result['scriptBody'] = $data['scriptBody'];
			}
			else
			{
				$result['saveStatus'] = "Error updating file";
			}
		}
		else
		{
			$result['saveStatus'] = "Error, file does not exist";
		}
	}
	else
	{
		if (!isset($data['scriptName']))
			$result['saveStatus'] = "Error, missing scriptName";
		else if (!isset($data['scriptBody']))
			$result['saveStatus'] = "Error, missing scriptBody";
	}

	returnJSON($result);
}

/////////////////////////////////////////////////////////////////////////////

function SetTestMode()
{
	global $args;

	SendCommand(sprintf("SetTestMode,%s", $args['data']));
}

function GetTestMode()
{
	header( "Content-Type: application/json");
	echo SendCommand("GetTestMode");
}

/////////////////////////////////////////////////////////////////////////////

function SetupExtGPIOJson()
{
	global $args;
	$result = Array();

	$gpio = $args['gpio'];
	$mode = $args['mode'];

	check($gpio, "gpio", __FUNCTION__);
	check($mode, "mode", __FUNCTION__);

	$statuses = [
		0 => 'failed',
		1 => 'success'
	];

	$status = SendCommand(sprintf("SetupExtGPIO,%s,%s", $gpio, $mode));

	$status = explode(',', $status, 14);
	$result['status'] = $statuses[(int) $status[1]];

	returnJSON($result);
}

/////////////////////////////////////////////////////////////////////////////
function ExtGPIOJson()
{
	global $args;
	$result = Array();

	$gpio = $args['gpio'];
	$mode = $args['mode'];
	$val = $args['val'];

	check($gpio, "gpio", __FUNCTION__);
	check($mode, "mode", __FUNCTION__);
	check($val, "val", __FUNCTION__);

	$status = SendCommand(sprintf("ExtGPIO,%s,%s,%s", $gpio, $mode, $val));

	$status = explode(',', $status, 14);

	if ((int) $status[1] >= 0) {
		$result['status'] = 'success';
		$result['result'] = $status[6];
	} else {
		$result['status'] = 'failed';
	}

	returnJSON($result);
}

/////////////////////////////////////////////////////////////////////////////

?>
