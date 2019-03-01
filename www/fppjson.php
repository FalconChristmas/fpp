<?php

$skipJSsettings = 1;
require_once('common.php');
require_once('commandsocket.php');
require_once('universeentry.php');
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
	"getOutputProcessors" => 'GetOutputProcessors',
	"setOutputProcessors" => 'SetOutputProcessors',
	"getChannelOutputs"   => 'GetChannelOutputs',
	"setChannelOutputs"   => 'SetChannelOutputs',
	"setUniverses"        => 'SetUniverses',
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
	"getPlayListInfo"     => 'GetPlayListInfo',
	"getSequenceInfo"     => 'GetSequenceInfo',
	"getMediaDuration"    => 'getMediaDurationInfo',
	"getFileSize"         => 'getFileSize',
	"savePlaylist"        => 'SavePlaylist',
	"copyPlaylist"        => 'CopyPlaylist',
	"copyFile"            => 'CopyFile',
	"renameFile"          => 'RenameFile',
	"convertPlaylists"    => 'ConvertPlaylistsToJSON',
	"getPluginSetting"    => 'GetPluginSetting',
	"setPluginSetting"    => 'SetPluginSetting',
	"saveScript"          => 'SaveScript',
	"setTestMode"         => 'SetTestMode',
	"getTestMode"         => 'GetTestMode',
	"setupExtGPIO"        => 'SetupExtGPIOJson',
	"extGPIO"             => 'ExtGPIOJson',
    "getSysInfo"          => 'GetSystemInfoJson',
    "getHostNameInfo"     => 'GetSystemHostInfo'
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
	//preemptively close the session
    session_write_close();

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
        if ($settings['Platform'] == "BeagleBone Black") {
            exec('findmnt -n -o SOURCE / | colrm 1 5', $output, $return_val);
            $rootDevice = $output[0];
            unset($output);
        } else {
            exec('lsblk -l | grep " /$" | cut -f1 -d" "', $output, $return_val);
            $rootDevice = $output[0];
            unset($output);
        }
        if ($value == "--none--") {
            $value = $rootDevice;
        } else {
            $fsckOrder = "0";
            exec( $SUDO . " file -sL /dev/$value | grep FAT", $output, $return_val );
            if ($output[0] == "") {
                unset($output);
                exec( $SUDO . " file -sL /dev/$value | grep BTRFS", $output, $return_val );

                if ($output[0] == "") {
                    unset($output);
                    exec( $SUDO . " file -sL /dev/$value | grep DOS", $output, $return_val );
                    if ($output[0] == "") {
                        # probably ext4
                        $options = "defaults,noatime,nodiratime,nofail";
                        $fsckOrder = "2";
                    } else {
                        # exFAT probably
                        $options = "defaults,noatime,nodiratime,exec,nofail,flush,uid=500,gid=500,nonempty";
                        $fsckOrder = "2";
                    }
                } else {
                    # BTRFS, turn on compression since fseq files are very compressible
                    $options = "defaults,noatime,nodiratime,compress=zstd,nofail";
                    $fsckOrder = "0";
                }
            } else {
                # FAT filesystem
                $options = "defaults,noatime,nodiratime,exec,nofail,flush,uid=500,gid=500";
                $fsckOrder = "2";
            }
        }
        if (preg_match("/$rootDevice/", $value)) {
            exec(   $SUDO . " sed -i 's/.*home\/fpp\/media/#\/dev\/sda1    \/home\/fpp\/media/' /etc/fstab", $output, $return_val );
        } else {
            exec(   $SUDO . " sed -i 's/.*home\/fpp\/media.*/\/dev\/$value	\/home\/fpp\/media	auto	$options	0	$fsckOrder /' /etc/fstab", $output, $return_val );
        }
        unset($output);
	} else if ($setting == "AudioOutput") {
		SetAudioOutput($value);
    } else if ($setting == "wifiDrivers") {
        if ($value == "Kernel") {
            exec(   $SUDO . " rm -f /etc/modprobe.d/blacklist-native-wifi.conf", $output, $return_val );
            exec(   $SUDO . " rm -f /etc/modprobe.d/rtl8723bu-blacklist.conf", $output, $return_val );
        } else {
            exec(   $SUDO . " cp /opt/fpp/etc/blacklist-native-wifi.conf /etc/modprobe.d", $output, $return_val );
        }
    } else if ($setting == "EnableTethering") {
        if ($value == "1") {
            $ssid = ReadSettingFromFile("TetherSSID");
            $psk = ReadSettingFromFile("TetherPSK");
            if ($ssid == "") {
                $ssid = "FPP";
                WriteSettingToFile("TetherSSID", $ssid);
            }
            if ($psk == "") {
                $psk = "Christmas";
                WriteSettingToFile("TetherPSK", $psk);
            }
            exec(   $SUDO . " systemctl disable dnsmasq", $output, $return_val );
            exec(   $SUDO . " connmanctl tether wifi on $ssid $psk", $output, $return_val );
        } else {
            exec(   $SUDO . " connmanctl tether wifi off", $output, $return_val );
            exec(   $SUDO . " systemctl enable dnsmasq", $output, $return_val );
        }
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

	//close the session before we start, this removes the session lock and lets other scripts run
    session_write_close();

    //Default json to be returned
    $default_return_json = array(
        'fppd' => 'unknown',
        'status' => -1,
        'status_name' => 'unknown',
        'current_playlist' =>
            [
                'playlist' => '',
                'type' => '',
                'index' => '0',
                'count' => '0'
            ],
        'current_sequence' => '',
        'current_song' => '',
        'seconds_played' => '0',
        'seconds_remaining' => '0',
        'time_elapsed' => '00:00',
        'time_remaining' => '00:00'
    );

    //if the ip= argument supplied
    if (isset($args['ip']))
	{
		header( "Content-Type: application/json");

		//validate IP address is a valid IPv4 address - possibly overkill but have seen IPv6 addresses polled
        if (filter_var($args['ip'], FILTER_VALIDATE_IP)) {
        	$do_expert = (isset($args['advancedView']) && $args['advancedView'] == true) ? "&advancedView=true" : "";

        	//Make the request - also send across whether advancedView data is requested so it's returned all in 1 request
            $request_content = @file_get_contents("http://" . $args['ip'] . "/fppjson.php?command=getFPPstatus" . $do_expert);
            //check we have valid data
            if ($request_content === FALSE) {
            	//check the response header
				//check for a 401 - Unauthorized response
				if(stristr($http_response_header[0],'401')){
					//set a reason so we can inform the user
                    $default_return_json['reason'] = "Cannot Access - Web GUI Password Set";
				}
				error_log("GetFPPStatusJson failed for IP: " . $args['ip'] . " -> " . $do_expert . " - " . json_encode($http_response_header));
                //error return default response
				echo json_encode($default_return_json);
            } else {
            	//Work around for older versioned devices where the advanced data was pulled in separately rather than being
				//included (when requested) with the standard data via getFPPStatus
				//If were in advanced view and the request_content doesn't have the 'advancedView' key (this is included when requested with the standard data) then we're dealing with a older version
				//that's using the expertView key and was being obtained separately
				if ((isset($args['advancedView']) && ($args['advancedView'] == true || strtolower($args['advancedView']) == "true")) && strpos($request_content, 'advancedView') === FALSE) {
					$request_content_arr = json_decode($request_content, true);
					//
					$request_expert_content = @file_get_contents("http://" . $args['ip'] . "/fppjson.php?command=getSysInfo");
					//check we have valid data
					if ($request_expert_content === FALSE) {
						$request_expert_content = array();
					}
					//Add data into the final response, since getFPPStatus returns JSON, decode into array, add data, encode back to json
					//Add a new key for the advanced data, also decode it as it's an array
					$request_content_arr['advancedView'] = json_decode($request_expert_content, true);
					//Re-encode everything back to a string
					$request_content = json_encode($request_content_arr);
				}

				//return the actual FPP Status of the device
				//this will already be a JSON string so we don't have to anything
				echo $request_content;
            }
        } else {
            error_log("GetFPPStatusJson failed for IP: " . $args['ip'] . " ");
            //IPv6 (in rare case it happens) return default response
            echo json_encode($default_return_json);
        }

        exit(0);
	}
	else
	{
        //go through the new API to get the status
        $request_content = @file_get_contents("http://localhost/api/fppd/status");
        if ($request_content === FALSE) {
            $status=exec("if ps cax | grep -q git_pull; then echo \"updating\"; else echo \"false\"; fi");
            
            $default_return_json['fppd'] = "Not Running";
            $default_return_json['status_name'] = $status == 'updating' ? $status : 'stopped';
            
            returnJSON($default_return_json);
            exit(0);
        }
		$data = json_decode($request_content, TRUE);

		//Check to see if we should also get the systemInfo for multiSync Expert view
		if (isset($args['advancedView']) && ($args['advancedView'] == true || strtolower($args['advancedView']) == "true")) {
			//Get the advanced info directly as an array
			$request_expert_content = GetSystemInfoJson(true);
			//check we have valid data
			if ($request_expert_content === FALSE) {
				$request_expert_content = array();
			}
			//Add data into the final response, since we have the status as an array already then just add the expert view
			//Add a new key for the expert data to the original data array
			$data['advancedView'] = $request_expert_content;
		}

        returnJSON($data);
	}
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
        $majorID = intval(ltrim($entry->majorID, 0));
        if ($majorID < 10)
			$majorID = '0' . $majorID;

        $minorID = intval(ltrim($entry->minorID, 0));
		if ($minorID < 10)
			$minorID = '0' . $minorID;

		$id = $majorID . '_' . $minorID;

		AddEventDesc($entry);

		return new PlaylistEntry($entry->type,'', '', 0, '', $entry->desc, $id,'', $entry, $index);
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

	$majorID = intval(ltrim($entry->majorID, 0));
	if ($majorID < 10)
		$majorID = '0' . $majorID;

	$minorID = intval(ltrim($entry->minorID, 0));
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

function GetPlaylist($playlistName, $fromMemory)
{
	global $settings;

	if ($fromMemory)
		$jsonStr = file_get_contents('http://127.0.0.1:32322/fppd/playlist/config/');
	else
		$jsonStr = file_get_contents($settings['playlistDirectory'] . '/' . $playlistName . ".json");

	$data = json_decode($jsonStr);

	return $data;
}

function LoadSubPlaylist(&$playlist, &$i, $plentry)
{
	global $settings;

	$data = GetPlaylist($plentry->name, 0);

	$sections = Array();

	if (isset($data->leadIn))
		array_push($sections, $data->leadIn);
	if (isset($data->mainPlaylist))
		array_push($sections, $data->mainPlaylist);
	if (isset($data->leadOut))
		array_push($sections, $data->leadOut);

	foreach ($sections as $section)
	{
		foreach ($section as $entry)
		{
			if ($entry->type == "playlist")
			{
				LoadSubPlaylist($playlist, $i, $entry);
			}
			else
			{
				$playlist[$i] = GetJSONPlaylistEntry($entry, $i);
				$i++;
			}
		}
	}
}

function LoadPlayListDetails($file, $mergeSubs, $fromMemory)
{
	global $settings;

	$playListEntriesLeadIn = NULL;
	$playListEntriesMainPlaylist = NULL;
	$playListEntriesLeadOut = NULL;
	$playlistInfo = NULL;
	$_SESSION['playListEntriesLeadIn']=NULL;
	$_SESSION['playListEntriesMainPlaylist']=NULL;
	$_SESSION['playListEntriesLeadOut']=NULL;
	$_SESSION['playlistInfo']=NULL;
	$_SESSION['currentPlaylist'] = '';

	$jsonStr = "";

	if (!file_exists($settings['playlistDirectory'] . '/' . $file . ".json"))
	{
		if (file_exists($settings['playlistDirectory'] . '/' . $file))
			LoadCSVPlaylistDetails($file);

		return "";
	}

	$data = GetPlaylist($file, $fromMemory);

	if ($fromMemory)
		return $data;

	if (isset($data->leadIn))
	{
		$i = 0;
		foreach ($data->leadIn as $entry)
		{
			if ($mergeSubs && $entry->type == "playlist")
			{
				LoadSubPlaylist($playListEntriesLeadIn, $i, $entry);
			}
			else
			{
				$playListEntriesLeadIn[$i] = GetJSONPlaylistEntry($entry, $i);
				$i++;
			}
		}
	}

	if (isset($data->mainPlaylist))
	{
		$i = 0;
		foreach ($data->mainPlaylist as $entry)
		{
			if ($mergeSubs && $entry->type == "playlist")
			{
				LoadSubPlaylist($playListEntriesMainPlaylist, $i, $entry);
			}
			else
			{
				$playListEntriesMainPlaylist[$i] = GetJSONPlaylistEntry($entry, $i);
				$i++;
			}
		}
	}

	if (isset($data->leadOut))
	{
		$i = 0;
		foreach ($data->leadOut as $entry)
		{
			if ($mergeSubs && $entry->type == "playlist")
			{
				LoadSubPlaylist($playListEntriesLeadOut, $i, $entry);
			}
			else
			{
				$playListEntriesLeadOut[$i] = GetJSONPlaylistEntry($entry, $i);
				$i++;
			}
		}
	}

	if (isset($data->playlistInfo))
	{
		$playlistInfo = $data->playlistInfo;
	}

	$_SESSION['playListEntriesLeadIn'] = $playListEntriesLeadIn;
	$_SESSION['playListEntriesMainPlaylist'] = $playListEntriesMainPlaylist;
	$_SESSION['playListEntriesLeadOut'] = $playListEntriesLeadOut;
	$_SESSION['playlistInfo'] = $playlistInfo;

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

function CopyFile()
{
	$filename = $_POST['filename'];
	check($filename, "filename", __FUNCTION__);

	$newfilename = $_POST['newfilename'];
	check($newfilename, "newfilename", __FUNCTION__);

	$dir = $_POST['dir'];
	check($dir, "dir", __FUNCTION__);

	$dir = GetDirSetting($dir);

	if ($dir == "")
		return;

	$result = Array();

	if (copy($dir . '/' . $filename, $dir . '/' . $newfilename))
		$result['status'] = 'success';
	else
		$result['status'] = 'failure';

	$result['original'] = $_POST['filename'];
	$result['new'] = $_POST['newfilename'];

	returnJSON($result);
}

function RenameFile()
{
	$filename = $_POST['filename'];
	check($filename, "filename", __FUNCTION__);

	$newfilename = $_POST['newfilename'];
	check($newfilename, "newfilename", __FUNCTION__);

	$dir = $_POST['dir'];
	check($dir, "dir", __FUNCTION__);

	$dir = GetDirSetting($dir);

	if ($dir == "")
		return;

	$result = Array();

	if (rename($dir . '/' . $filename, $dir . '/' . $newfilename))
		$result['status'] = 'success';
	else
		$result['status'] = 'failure';

	$result['original'] = $_POST['filename'];
	$result['new'] = $_POST['newfilename'];

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
				LoadPlayListDetails($playlist, 0, 0);

				SavePlaylistRaw($playlist);

				$result['playlists'][] = $playlist;
			}
		}
	}

	$_SESSION['currentPlaylist'] = '';

	returnJSON($result);
}

/**
 * Returns filesize for the specified file
 */
function getFileSize()
{
	//Get Info
	global $args;
	global $settings;

	$mediaType = $args['type'];
	$mediaName = $args['filename'];
	check($mediaType, "mediaType", __FUNCTION__);
	check($mediaName, "mediaName", __FUNCTION__);

	$returnStr = [];
	$filesize = 0;
	if (strtolower($mediaType) == 'sequence') {
		//Sequences
		if (file_exists($settings['sequenceDirectory'] . "/" . $mediaName)) {
			$filesize = human_filesize($settings['sequenceDirectory'] . "/" . $mediaName);
		}
	} else if (strtolower($mediaType) == 'effect') {
		if (file_exists($settings['effectDirectory'] . "/" . $mediaName)) {
			$filesize = human_filesize($settings['effectDirectory'] . "/" . $mediaName);
		}
	} else if (strtolower($mediaType) == 'music') {
		if (file_exists($settings['musicDirectory'] . "/" . $mediaName)) {
			$filesize = human_filesize($settings['musicDirectory'] . "/" . $mediaName);
		}
	} else if (strtolower($mediaType) == 'video') {
		if (file_exists($settings['videoDirectory'] . "/" . $mediaName)) {
			$filesize = human_filesize($settings['videoDirectory'] . "/" . $mediaName);
		}
	}

	$returnStr[$mediaName]['filesize '] = $filesize;

	$returnStr = json_encode($returnStr, JSON_PRETTY_PRINT);

	header("Content-Type: application/json");
	echo $returnStr;
}

/**
 * Returns duration info about the specified media file
 *
 * @param string $mediaName
 * @param bool $returnArray
 * @return array|string
 * @throws getid3_exception
 */
function getMediaDurationInfo($mediaName = "", $returnArray = false)
{
	//Get Info
	global $args;
	global $settings;

	$returnStr = array();
	$total_duration = 0;

	if (empty($mediaName)) {
		$mediaName = $args['media'];
	}

	check($mediaName, "mediaName", __FUNCTION__);

	if (file_exists($settings['musicDirectory'] . "/" . $mediaName)) {
		//Music Directory

		//Check the cache for the media name first
		$media_filesize = filesize($settings['musicDirectory'] . "/" . $mediaName);
		$cache_duration = media_duration_cache($mediaName, null, $media_filesize);
		//cache duration will be null if not in cache, then retrieve it
		if ($cache_duration == NULL) {
			//Include our getid3 library for media
			require_once('./lib/getid3/getid3.php');

			//Instantiate getID3 object
			$getID3 = new getID3;

			$ThisFileInfo = $getID3->analyze($settings['musicDirectory'] . "/" . $mediaName);
			//cache it
			media_duration_cache($mediaName, $ThisFileInfo['playtime_seconds'], $media_filesize);
		} else {
			$ThisFileInfo['playtime_seconds'] = $cache_duration;
		}

		$total_duration = $ThisFileInfo['playtime_seconds'] + $total_duration;
	} else if (file_exists($settings['videoDirectory'] . "/" . $mediaName)) {
		//Check video directory

		//Check the cache for the media name first
		$media_filesize = filesize($settings['videoDirectory'] . "/" . $mediaName);
		$cache_duration = media_duration_cache($mediaName, null, $media_filesize);
		//cache duration will be null if not in cache, then retrieve it
		if ($cache_duration == NULL) {
			//Include our getid3 library for media
			require_once('./lib/getid3/getid3.php');

			//Instantiate getID3 object
			$getID3 = new getID3;

			$ThisFileInfo = $getID3->analyze($settings['videoDirectory'] . "/" . $mediaName);
			//cache it
			media_duration_cache($mediaName, $ThisFileInfo['playtime_seconds'], $media_filesize);
		} else {
			$ThisFileInfo['playtime_seconds'] = $cache_duration;
		}

		$total_duration = $ThisFileInfo['playtime_seconds'] + $total_duration;
	} else if (file_exists($settings['sequenceDirectory'] . "/" . $mediaName)) {
		//Check Sequence directory
		$sequence_info = get_sequence_file_info($mediaName);
//		$media_filesize =$sequence_info['seqFileSize'];

		if (array_key_exists('seqDuration', $sequence_info)) {
			$total_duration = $sequence_info['seqDuration'];

		} else {
			$total_duration = 0;
		}

		//This doesn't take very long so no need to cache
//		media_duration_cache($mediaName, $total_duration, $media_filesize);
	} else {
		error_log("getMediaDurationInfo:: Could not find media file - " . $mediaName);
	}

	if ($total_duration !== 0) {
		//If return array is false then return the human readable duration, else raw seconds
		if ($returnArray == false) {
			$returnStr[$mediaName]['duration'] = human_playtime($total_duration);
		} else {
			$returnStr[$mediaName]['duration'] = $total_duration;
		}
	}

	if ($returnArray == true) {
		return $returnStr;
	} else {
		$returnStr = json_encode($returnStr, JSON_PRETTY_PRINT);

		header("Content-Type: application/json");
		echo $returnStr;
	}
}

/**
 * Returns sequence header info
 *
 * @return array
 */
function GetSequenceInfo()
{
	global $args;
	global $settings;
	$return_arr = array();

	$sequence = $args['seq'];
	//if the file extension is missing, add it on
	if (strpos($sequence, '.fseq') === FALSE) {
		$sequence = $sequence . ".fseq";
	}

	if (file_exists($settings['sequenceDirectory'] . '/' . $sequence)) {
		$return_arr = get_sequence_file_info($sequence);
	} else {
		$return_arr[$sequence]['error'] = "GetSequenceInfo:: Unable find sequence :: " . $sequence;
		error_log("GetSequenceInfo:: Unable find sequence :: " . $sequence);
	}

	$return_arr = json_encode($return_arr, JSON_PRETTY_PRINT);

	header("Content-Type: application/json");
	echo $return_arr;
}

/**
 * Returns the total playlist duration & number of items in the body/main playlist
 * @throws getid3_exception
 */
function GetPlayListInfo()
{
	global $args;
	global $settings;
	$reload = false;

	$playlist = $args['pl'];
	check($playlist, "playlist", __FUNCTION__);

	if (isset($args['reload'])) {
		$reload = $args['reload'];
		check($reload, "reload", __FUNCTION__);
	}

	$returnStr = array();
	//load in the playlist
	if (file_exists($settings['playlistDirectory'] . '/' . $playlist . ".json")) {
		$jsonStr = file_get_contents($settings['playlistDirectory'] . '/' . $playlist . ".json");
		//decode it
		$data = json_decode($jsonStr);

		$total_duration = 0;
		$total_items = 0;

		//Determine if there is a fileInfo node
		if(array_key_exists('playlistInfo',$data)){
			//If it exists and we're not reloading then just extract the info
			$fileInfo_data = $data->playlistInfo;
			$fileinfo_Duration = $fileInfo_data->total_duration;
			$fileinfo_Items = $fileInfo_data->total_items;

			//Then use this value, it's stored in raw seconds so make it readable
			$returnStr['total_duration'] = human_playtime($fileinfo_Duration);
			$returnStr['total_items'] = $fileinfo_Items;
		}
//		else if (!array_key_exists('playlistInfo',$data) || $reload == true){
			//generate the total duration, also save the duration into the playlist
			//find the main playlist / body
//			if (isset($data->mainPlaylist)) {
//				//Loop over the contents, checking for media and build up a total duration
//				foreach ($data->mainPlaylist as $idx => $playlistEntry) {
//					$mediaName = "";
//
//					$type = $playlistEntry->type;
//					if (isset($playlistEntry->mediaName)) {
//						$mediaName = $playlistEntry->mediaName;
//					}
//					//If entry is either both sequence + audio or just media we can look at the media name and extract info
//					if (($type == "both" || $type == "media") && !empty($mediaName)) {
//						//Get media duration
//						$ThisFileInfo = getMediaDurationInfo($mediaName, true);
//
//						$total_duration = $ThisFileInfo[$mediaName]['duration'] + $total_duration;
//						$total_items++;
//					}
//					//Consider pause duration also
//					if ($type == "pause") {
//						$total_duration = $playlistEntry->duration + $total_duration;
//					}
//				}
//
//				//Build up the formatted string
//				$returnStr['total_duration'] = human_playtime($total_duration);
//				$returnStr['total_items'] = $total_items;
//				unset($getID3);
//			}
//		}
	} else {
		error_log("GetPlayListInfo:: Playlist doesn't exist - " . $playlist);
	}

	$returnStr = json_encode($returnStr, JSON_PRETTY_PRINT);

	header("Content-Type: application/json");
	echo $returnStr;
}

function GetPlayListEntries()
{
	global $args;
	global $settings;

	$playlist = $args['pl'];
	check($playlist, "playlist", __FUNCTION__);
	$reload = $args['reload'];
	check($reload, "reload", __FUNCTION__);

	$mergeSubs = 0;
	if (isset($args['mergeSubs']) && $args['mergeSubs'] == 1)
		$mergeSubs = 1;

	$fromMemory = 0;
	if (isset($args['fromMemory']) && $args['fromMemory'] == 1)
		$fromMemory = 1;

	if ($reload == 'true')
		$jsonStr = LoadPlayListDetails($playlist, $mergeSubs, $fromMemory);

	if (!$fromMemory)
	{
		$jsonStr = GenerateJSONPlaylist($playlist);

		//Quick hack to make the playlist duration human readable (we want to keep the duration in seconds for the actual playlist) so modify here
		$jsonStr = json_decode($jsonStr);
	}

	$jsonStr->playlistInfo->total_duration = human_playtime($jsonStr->playlistInfo->total_duration);

	$jsonStr = json_encode($jsonStr, JSON_PRETTY_PRINT);

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
			'			"scriptArgs": "%s",' . "\n" .
			'			"blocking": %d' . "\n" .
			'		}',
			0, // Play Once
			$entry->scriptName,
			$entry->scriptArgs,
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

function GenerateJSONPlaylistInfo($list)
{
	global $playlistDirectory, $settings;

	$total_duration = $total_items = 0;
	$returnStr = array(
		'playlistInfo' => array(
			'total_duration' => 0,
			'total_items' => 0
		)
	);

	if (isset($_SESSION['playlistInfo'])) {
		//pull the data out of the session
		$session_playlist_info = $_SESSION['playlistInfo'];

		//Build up the formatted string
		$returnStr['playlistInfo']['total_duration'] = ($session_playlist_info->total_duration);
		$returnStr['playlistInfo']['total_items'] = $session_playlist_info->total_items;
	}else if ((is_array($list) && !empty($list)) && !isset($_SESSION['playlistInfo']))  {
		//Could not find the playlist info in the session so lets generate it
		//it should be there if it was in the json file.

		//loop over the lists
		foreach ($list as $listName) {
			//Loop over the contents, checking for media and build up a total duration
			if (isset($_SESSION[$listName])) {
				foreach ($_SESSION[$listName] as $idx => $playlistEntry) {
					$mediaName = "";

					$type = $playlistEntry->entry->type;
					if (isset($playlistEntry->entry->mediaName)) {
						$mediaName = $playlistEntry->entry->mediaName;
					} else if (isset($playlistEntry->entry->sequenceName)){
						$mediaName = $playlistEntry->entry->sequenceName;
					}

					//If entry is either both sequence + audio or just media we can look at the media name and extract info
					if (($type == "both" || $type == "media" || $type == "sequence" ) && !empty($mediaName)) {
						//Get media duration
						$ThisFileInfo = getMediaDurationInfo($mediaName, true);
						//Add it up
						$total_duration = $ThisFileInfo[$mediaName]['duration'] + $total_duration;
						if ($listName == "playListEntriesMainPlaylist") {
							$total_items++;
						}
					}
					//Consider pause duration also
					if ($type == "pause") {
						$total_duration = $playlistEntry->entry->duration + $total_duration;
					}
				}
			}
		}

		//Build up the formatted string
		$returnStr['playlistInfo']['total_duration'] = ($total_duration);
		$returnStr['playlistInfo']['total_items'] = $total_items;
	}

	return ', "playlistInfo":' . json_encode($returnStr['playlistInfo'], JSON_PRETTY_PRINT);
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

	//Generate json for the primary sections of the playlist
	$result .= GenerateJSONPlaylistSection('leadIn', 'playListEntriesLeadIn');
	$result .= GenerateJSONPlaylistSection('mainPlaylist', 'playListEntriesMainPlaylist');
	$result .= GenerateJSONPlaylistSection('leadOut', 'playListEntriesLeadOut');

	//Generate playlist info for all the areas
	$result .= GenerateJSONPlaylistInfo(array('playListEntriesLeadIn','playListEntriesMainPlaylist','playListEntriesLeadOut'));

	$result .= sprintf("\n}\n");

	return $result;
}

function SavePlaylistRaw($name)
{
	global $playlistDirectory;

	//Hack to get have the playlist duration be recalculated before saving
	unset($_SESSION['playlistInfo']);

	$json = GenerateJSONPlaylist($name);

	// Make sure the whole string is pretty
	$json = json_encode(json_decode($json), JSON_PRETTY_PRINT);

	// Rename any old CSV style playlist if it exists
	if (file_exists($playlistDirectory . '/' . $name))
		rename($playlistDirectory . '/' . $name, $playlistDirectory . '/' . $name . '-CSV');

	$f = fopen($playlistDirectory . '/' . $name . ".json","w") or exit("Unable to open file! : " . $playlistDirectory . '/' . $name . ".json");
	//Avoid nuking playlist file if we have no JSON string for some reason
	if(!empty($json)){
		fwrite($f, $json);
	}else{
		error_log("Error saving ".$playlistDirectory . '/' . $name . ".json" . " -- No Playlist (JSON) content to write");
	}
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

function CopyPlaylist()
{
	global $playlistDirectory;

	$from_playlist_name = $_GET['from'];
	$to_playlist_name = $_GET['to'];

	check($from_playlist_name, "name", __FUNCTION__);
	check($to_playlist_name, "name", __FUNCTION__);

	if(empty($to_playlist_name)){
		$to_playlist_name = $from_playlist_name . " - Copy";
	}

	if (file_exists($playlistDirectory . '/' . $from_playlist_name . '.json')) {
		copy($playlistDirectory . '/' . $from_playlist_name . '.json', $playlistDirectory . '/' . $to_playlist_name . '.json');
	}

	$result = Array();
	$result['status'] = 'Ok';
	$result['copyPlaylist'] = $to_playlist_name;
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
    $result = Array();
    exec("ip addr show up | grep 'inet ' | awk '{print $2}' | cut -f1 -d/ | grep -v '^127'", $localIPs);

    $found = Array();
    
    $discovered = json_decode(@file_get_contents("http://localhost:32322/fppd/multiSyncSystems"), true);
    foreach ($discovered['systems'] as $system) {

        if (preg_match('/^169\.254/', $system['address']))
            continue;
        
        $hostNameInfo = @file_get_contents("http://" . $system['address'] . "/fppjson.php?command=getHostNameInfo");
        if ($hostNameInfo != false) {
            $elem = Array();
            $elem['HostName'] = $system['hostname'];
            $sysHostInfo = json_decode($hostNameInfo, true);
            $found[$system['address']] = 1;

            $elem['HostDescription'] = !empty($sysHostInfo['HostDescription']) ? $sysHostInfo['HostDescription'] : "";
            $elem['IP'] = $system['address'];
            $elem['fppMode'] = $system['fppModeString'];
            $elem['Local'] = 0;
            $elem['Platform'] = $system['type'];
            $elem['majorVersion'] = $system['majorVersion'];
            $elem['minorVersion'] = $system['minorVersion'];
            $elem['model'] = $system['model'];
            $elem['version'] = $system['version'];
            $elem['channelRanges'] = !empty($system['channelRanges']) ? $system['channelRanges'] : "";
            $matches = preg_grep("/^" . $elem['IP'] . "$/", $localIPs);
            if (count($matches))
                $elem['Local'] = 1;
            $result[] = $elem;
        }
    }

	exec("avahi-browse -artp | grep  'IPv4' | grep 'fpp-fppd' | sort", $rmtSysOut);

	foreach ($rmtSysOut as $system)
	{
		if (!preg_match("/^=.*fpp-fppd/", $system))
			continue;
		if (!preg_match("/fppMode/", $system))
			continue;

		$parts = explode(';', $system);

        if (preg_match("/usb.*/", $parts[1]))
            continue;
        
        if (isset($found[$parts[7]]) && ($found[$parts[7]] == 1)) {
            continue;
        }
        
        if (isset($found[$parts[7]]) && strlen($parts[7]) > 16) {
            //for some reason, despite the IPv4 grep, it occassionally will return an IPv6 address, filter those out
            continue;
        }
        
		$sysHostInfo = json_decode(@file_get_contents("http://" . $parts[7] . "/fppjson.php?command=getHostNameInfo"), true);

		$elem = Array();
		$elem['HostName'] = $parts[3];
		$elem['HostDescription'] = !empty($sysHostInfo['HostDescription']) ? $sysHostInfo['HostDescription'] : "";
		$elem['IP'] = $parts[7];
		$elem['fppMode'] = "Unknown";
		$elem['Local'] = 0;
		$elem['Platform'] = "Unknown";

		$matches = preg_grep("/^" . $elem['IP'] . "$/", $localIPs);
		if (count($matches))
			$elem['Local'] = 1;

		if (count($parts) > 8) {
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

function GetOutputProcessors()
{
	global $settings;

	$jsonStr = "";

	if (file_exists($settings['outputProcessorsFile'])) {
		$jsonStr = file_get_contents($settings['outputProcessorsFile']);
	}

	header( "Content-Type: application/json");
	echo $jsonStr;
}

function SetOutputProcessors()
{
	global $settings;
	global $args;

	$data = stripslashes($args['data']);
	$data = prettyPrintJSON(substr($data, 1, strlen($data) - 2));

	file_put_contents($settings['outputProcessorsFile'], $data);

	GetOutputProcessors();
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

/////////////////////////////////////////////////////////////////////////////
function SaveUniversesToFile($enabled, $input)
{
	global $settings;

	$universeJSON = sprintf(
		"{\n" .
		"	\"%s\": [\n" .
		"		{\n" .
		"			\"type\": \"universes\",\n" .
		"			\"enabled\": %d,\n" .
		"			\"startChannel\": 1,\n" .
		"			\"channelCount\": -1,\n" .
		"			\"universes\": [\n",
		$input ? "channelInputs" : "channelOutputs", $enabled);

	for($i=0;$i<count($_SESSION['UniverseEntries']);$i++)
	{
		if ($i > 0)
			$universeJSON .= ",\n";

		$universeJSON .= sprintf(
		"				{\n" .
		"					\"active\": %d,\n" .
		"					\"description\": \"%s\",\n" .
		"					\"id\": %d,\n" .
		"					\"startChannel\": %d,\n" .
		"					\"channelCount\": %d,\n" .
		"					\"type\": %d,\n" .
		"					\"address\": \"%s\",\n" .
		"					\"priority\": %d\n" .
		"				}",
			$_SESSION['UniverseEntries'][$i]->active,
			$_SESSION['UniverseEntries'][$i]->desc,
			$_SESSION['UniverseEntries'][$i]->universe,
			$_SESSION['UniverseEntries'][$i]->startAddress,
			$_SESSION['UniverseEntries'][$i]->size,
			$_SESSION['UniverseEntries'][$i]->type,
			$_SESSION['UniverseEntries'][$i]->unicastAddress,
			$_SESSION['UniverseEntries'][$i]->priority);
	}

	$universeJSON .=
		"\n" .
		"			]\n" .
		"		}\n" .
		"	]\n" .
		"}\n";

    $filename = $settings['universeOutputs'];
    if ($input)
        $filename = $settings['universeInputs'];

	$f = fopen($filename,"w") or exit("Unable to open file! : " . $filename);
	fwrite($f, $universeJSON);
	fclose($f);

	return $universeJSON;
}

function SetUniverses()
{
	$enabled = $_POST['E131Enabled'];
	check($enabled);
	$input = $_POST['input'];
	check($input);
    
    $count = count($_SESSION['UniverseEntries']);
    if ( isset($_POST['UniverseCount']) ) {
        $count = intval($_POST['UniverseCount']);
        $_SESSION['UniverseEntries'] = NULL;
        for ($i = 0; $i < $count; $i++) {
            $_SESSION['UniverseEntries'][$i] = new UniverseEntry(1,"",1,1,512,0,"",0,0);
        }
    }

	for($i=0;$i<$count;$i++)
	{
		if( isset($_POST['chkActive'][$i]))
		{
			$_SESSION['UniverseEntries'][$i]->active = 1;
		}
		else
		{
			$_SESSION['UniverseEntries'][$i]->active = 0;
		}
		$_SESSION['UniverseEntries'][$i]->desc = 	$_POST['txtDesc'][$i];
        
        if ( isset($_POST['txtUniverse']) && isset($_POST['txtUniverse'][$i])) {
            $_SESSION['UniverseEntries'][$i]->universe = intval($_POST['txtUniverse'][$i]);
        } else {
            $_SESSION['UniverseEntries'][$i]->universe = 1;
        }
		$_SESSION['UniverseEntries'][$i]->size = 	intval($_POST['txtSize'][$i]);
		$_SESSION['UniverseEntries'][$i]->startAddress = 	intval($_POST['txtStartAddress'][$i]);
		$_SESSION['UniverseEntries'][$i]->type = 	intval($_POST['universeType'][$i]);
		$_SESSION['UniverseEntries'][$i]->unicastAddress = 	trim($_POST['txtIP'][$i]);
		$_SESSION['UniverseEntries'][$i]->priority = 	intval($_POST['txtPriority'][$i]);
	}

	return(SaveUniversesToFile($enabled, $input));
}
/////////////////////////////////////////////////////////////////////////////
function GetChannelOutputs()
{
	global $settings;
	global $args;

	$file = $args['file'];

	$jsonStr = "";

	if (file_exists($settings[$file])) {
		$jsonStr = file_get_contents($settings[$file]);
	}

	header( "Content-Type: application/json");
	echo $jsonStr;
}

function SetChannelOutputs()
{
	global $settings;
	global $args;

	$file = $args['file'];

	$data = stripslashes($args['data']);
	$data = prettyPrintJSON(substr($data, 1, strlen($data) - 2));

	file_put_contents($settings[$file], $data);

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
			"PSK='%s'\n" .
            "HIDDEN=%s\n",
			$data['SSID'], $data['PSK'], $data['Hidden']);
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
            //error output is silenced by @, function returns false on failure, it doesn't return true
            $script_save_result = @file_put_contents($filename, $content);
            //check result is not a error
			if ($script_save_result !== false)
			{
				$result['saveStatus'] = "OK";
				$result['scriptName'] = $data['scriptName'];
				$result['scriptBody'] = $data['scriptBody'];
			}
			else
			{
                $script_writable = is_writable($filename);
                $script_directory_writable = is_writable($settings['scriptDirectory']);

				$result['saveStatus'] = "Error updating file";
                error_log("SaveScript: Error updating file - " . $data['scriptName'] . " ($filename) ");
                error_log("SaveScript: Error updating file - " . $data['scriptName'] . " ($filename) " . " -> isWritable: " . $script_writable);
                error_log("SaveScript: Error updating file - " . $data['scriptName'] . " ($filename) " . " -> Scripts directory is writable: " . $script_directory_writable);
            }
		}
		else
		{
			$result['saveStatus'] = "Error, file does not exist";
            error_log("SaveScript: Error, file does not exist - " . $data['scriptName'] . " -> " . $filename);
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

function GetSystemInfoJson($return_array = false)
{
    global $settings;

    //close the session before we start, this removes the session lock and lets other scripts run
    session_write_close();

    //Default json to be returned
    $result = array();
    $result['HostName'] = $settings['HostName'];
	$result['HostDescription'] = !empty($settings['HostDescription']) ? $settings['HostDescription'] : "";
	$result['Platform'] = $settings['Platform'];
    $result['Variant'] = $settings['Variant'];

    //Get CPU & memory usage before any heavy processing to try get relatively accurate stat
	$result['Utilization']['CPU'] =  get_server_cpu_usage();
	$result['Utilization']['Memory'] = get_server_memory_usage();
	$result['Utilization']['Uptime'] = get_server_uptime(true);

    $IPs = explode("\n",trim(shell_exec("/sbin/ifconfig -a | cut -f1 -d' ' | grep -v ^$ | grep -v lo | grep -v eth0:0 | grep -v usb | grep -v SoftAp | grep -v 'can.' | sed -e 's/://g' | while read iface ; do /sbin/ifconfig \$iface | grep 'inet ' | awk '{print \$2}'; done")));

    $git_branch = get_git_branch();

	$result['Kernel'] = get_kernel_version();
    $result['Version'] = get_fpp_head_version();
    $result['Branch'] = $git_branch;
	$result['LocalGitVersion'] = get_local_git_version();
	$result['RemoteGitVersion'] = get_remote_git_version($git_branch);
	$result['AutoUpdatesDisabled'] = file_exists($settings['mediaDirectory'] . "/.auto_update_disabled") ? true : false;
	$result['IPs'] = $IPs;
    $result['Mode'] = $settings['fppMode'];

    //Return just the array if requested
	if ($return_array == true) {
		return $result;
	} else {
		returnJSON($result);
	}
}

/////////////////////////////////////////////////////////////////////////////

function GetSystemHostInfo()
{
	global $settings;

	//close the session before we start, this removes the session lock and lets other scripts run
	session_write_close();

	//Default json to be returned
	$result = array();
	$result['HostName'] = $settings['HostName'];
	$result['HostDescription'] = !empty($settings['HostDescription']) ? $settings['HostDescription'] : "";

	returnJSON($result);
}
    
?>
