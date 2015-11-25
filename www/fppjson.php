<?php

$skipJSsettings = 1;
require_once('common.php');
require_once('commandsocket.php');

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
	"getInterfaceInfo"    => 'GetInterfaceInfo',
	"setInterfaceInfo"    => 'SetInterfaceInfo',
	"getFPPSystems"       => 'GetFPPSystems',
	"getFPPstatus"		  => 'GetFPPStatus',
	"getSetting"          => 'GetSetting',
	"setSetting"          => 'SetSetting',
	"startSequence"       => 'StartSequence',
	"stopSequence"        => 'StopSequence',
	"toggleSequencePause" => 'ToggleSequencePause',
	"singleStepSequence"  => 'SingleStepSequence',
	"singleStepSequenceBack" => 'SingleStepSequenceBack',
	"getPluginSetting"    => 'GetPluginSetting',
	"setPluginSetting"    => 'SetPluginSetting',
	"setTestMode"         => 'SetTestMode',
	"getTestMode"         => 'GetTestMode'
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
			$SUDO . " /etc/init.d/avahi-daemon restart",
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

function GetFPPStatus()
{
	
    $status = SendCommand('s');
  
    if($status == false || $status == 'false') { 
     	
		$status=exec("if ps cax | grep -q git_pull; then echo \"updating\"; else echo \"false\"; fi");
     
     	returnJSON([
     			'fppd' => 'Not Running',
     			'status' => -1,
     			'status_name' => $status == 'updating' ? $status : 'stopped',
     		]);
     }

     $data = parseStatus($status);

     returnJson($data);
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
			'mode'   => $modes[$mode],
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
				'next_playlist' => $status[3],
				'next_playlist_start_time' => $status[4],
				'repeat_mode' => 0,
			];
		} else {

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

	exec("avahi-browse -artp | sort", $rmtSysOut);

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

		$result[] = $elem;
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


function GetInterfaceInfo()
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

?>
