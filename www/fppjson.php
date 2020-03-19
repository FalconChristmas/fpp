<?php

$skipJSsettings = 1;
require_once('common.php');
require_once('common/metadata.php');
require_once('common/settings.php');
require_once('commandsocket.php');
require_once('universeentry.php');
require_once('fppversion.php');


$a = session_id();
if(empty($a))
{
	session_start();
}
$_SESSION['session_id'] = session_id();


$command_array = Array(
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
	"getPlayListEntries"  => 'GetPlayListEntries',
	"getSequenceInfo"     => 'GetSequenceInfo',
	"getMediaDuration"    => 'getMediaDurationInfo',
	"getFileSize"         => 'getFileSize',
	"copyFile"            => 'CopyFile',
	"renameFile"          => 'RenameFile',
	"getPluginSetting"    => 'GetPluginSetting',
	"setPluginSetting"    => 'SetPluginSetting',
    "getPluginJSON"       => 'GetPluginJSON',
    "setPluginJSON"       => 'SetPluginJSON',
	"saveScript"          => 'SaveScript',
	"setTestMode"         => 'SetTestMode',
	"getTestMode"         => 'GetTestMode',
	"setupExtGPIO"        => 'SetupExtGPIOJson',
	"extGPIO"             => 'ExtGPIOJson',
    "getSysInfo"          => 'GetSystemInfoJson',
    "getHostNameInfo"     => 'GetSystemHostInfo',
    "clearPersistentNetNames" => 'ClearPersistentNetNames',
    "createPersistentNetNames" => 'CreatePersistentNetNames'
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
	returnJSONStr(json_encode($arr));
}

function returnJSONStr($str) {
	//preemptively close the session
    session_write_close();

    header( "Content-Type: application/json");
	header( "Access-Control-Allow-Origin: *");

	echo $str;

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
            exec(   $SUDO . " rm -f /etc/modprobe.d/blacklist-8192cu.conf", $output, $return_val );
        }
    } else if ($setting == "EnableTethering") {
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
    } else if ($setting == "BBBLeds0" || $setting == "BBBLeds1" || $setting == "BBBLeds2" || $setting == "BBBLeds3" || $setting == "BBBLedPWR") {
        SetBBBLeds();
	} else if ($setting == "scheduling") {
		SendCommand("EnableScheduling,$value,");
	} else {
        ApplySetting($setting, $value);
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
		$result = array();

		//validate IP address is a valid IPv4 address - possibly overkill but have seen IPv6 addresses polled
        if (filter_var($args['ip'], FILTER_VALIDATE_IP)) {
        	$do_expert = (isset($args['advancedView']) && $args['advancedView'] == true) ? "&advancedView=true" : "";

        	//Make the request - also send across whether advancedView data is requested so it's returned all in 1 request
            $request_content = @file_get_contents("http://" . $args['ip'] . "/fppjson.php?command=getFPPstatus" . $do_expert);
            //check we have valid data
            if ($request_content === FALSE) {
            	//check the response header
				//check for a 401 - Unauthorized response
                if (isset($http_response_header)) {
                    if (stristr($http_response_header[0], '401')){
                        //set a reason so we can inform the user
                        $default_return_json['reason'] = "Cannot Access - Web GUI Password Set";
                    }
                    //error return default response
                    $result = $default_return_json;
                    $result["status_name"] = "password";
                    error_log("GetFPPStatusJson failed for IP: " . $args['ip'] . " -> " . $do_expert . " - " . json_encode($http_response_header));
                } else {
                    //error return default response
                    $result = $default_return_json;
                    $result["status_name"] = "unreachable";
                    error_log("GetFPPStatusJson failed for IP: " . $args['ip'] . " -> " . $do_expert);
                }
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

				$result = json_decode($request_content);
            }
        } else {
            error_log("GetFPPStatusJson failed for IP: " . $args['ip'] . " ");
            //IPv6 (in rare case it happens) return default response
			$result = $default_return_json;
        }

		returnJSON($result);

        exit(0);
	}
	else
	{
        //go through the new API to get the status
        // use curl so we can set a low connect timeout so if fppd isn't running we detect that quickly
        $curl = curl_init('http://localhost:32322/fppd/status');
        curl_setopt($curl, CURLOPT_FAILONERROR, true);
        curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
        curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
        curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 200);
        $request_content = curl_exec($curl);
        curl_close($curl);
        
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
			$request_expert_content = GetSystemInfoJsonInternal(true, false);
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
	$data = GetPlaylist($plentry->name, 0);

    $subPlaylist = Array();

	if (isset($data->leadIn))
		$subPlaylist = array_merge($subPlaylist, $data->leadIn);
	if (isset($data->mainPlaylist))
		$subPlaylist = array_merge($subPlaylist, $data->mainPlaylist);
	if (isset($data->leadOut))
		$subPlaylist = array_merge($subPlaylist, $data->leadOut);

    $li = 0;
	foreach ($subPlaylist as $entry)
	{
		if ($entry->type == "playlist")
		{
			LoadSubPlaylist($subPlaylist, $li, $entry);
		}

        $li++;
	}

    array_splice($playlist, $i, 1, $subPlaylist);
}

function LoadPlayListDetails($file, $mergeSubs, $fromMemory)
{
	global $settings;

	$playListEntriesLeadIn = NULL;
	$playListEntriesMainPlaylist = NULL;
	$playListEntriesLeadOut = NULL;
	$playlistInfo = NULL;

	$jsonStr = "";

	if (!$fromMemory && !file_exists($settings['playlistDirectory'] . '/' . $file . ".json"))
	{
		return "";
	}

	$data = GetPlaylist($file, $fromMemory);

	if ($fromMemory || !$mergeSubs)
		return $data;

	if (isset($data->leadIn))
	{
		$i = 0;
		foreach ($data->leadIn as $entry)
		{
			if ($entry->type == "playlist")
			{
				LoadSubPlaylist($data->leadIn, $i, $entry);
			}
            $i++;
		}
	}

	if (isset($data->mainPlaylist))
	{
		$i = 0;
		foreach ($data->mainPlaylist as $entry)
		{
			if ($mergeSubs && $entry->type == "playlist")
			{
				LoadSubPlaylist($data->mainPlaylist, $i, $entry);
			}
            $i++;
		}
	}

	if (isset($data->leadOut))
	{
		$i = 0;
		foreach ($data->leadOut as $entry)
		{
			if ($mergeSubs && $entry->type == "playlist")
			{
				LoadSubPlaylist($data->leadOut, $i, $entry);
			}
            $i++;
		}
	}

    return $data;
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

	returnJSONStr($returnStr);
}

// FIXME, this should use the API instead, need to add the subplaylist ability to the existing endpoint
function GetPlayListEntries()
{
	global $args;
	global $settings;

	$playlist = $args['pl'];
	check($playlist, "playlist", __FUNCTION__);

	$mergeSubs = 0;
	if (isset($args['mergeSubs']) && $args['mergeSubs'] == 1)
		$mergeSubs = 1;

	$fromMemory = 0;
	if (isset($args['fromMemory']) && $args['fromMemory'] == 1)
		$fromMemory = 1;

	$data = LoadPlayListDetails($playlist, $mergeSubs, $fromMemory);

    if (isset($data->playlistInfo->total_duration))
        $data->playlistInfo->total_duration = human_playtime($data->playlistInfo->total_duration);

	returnJSON($data);
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

    
function GetPluginJSON()
{
	global $args;
    global $settings;

	$plugin  = $args['plugin'];
    
    $cfgFile = $settings['configDirectory'] . "/plugin." . $plugin . ".json";
    if (file_exists($cfgFile)) {
        $js = file_get_contents($cfgFile);
        returnJSON($js);
    }
	$result = Array();
	returnJSON($result);
}

function SetPluginJSON()
{
	global $args;
    global $settings;
	$plugin  = $args['plugin'];
    
    $cfgFile = $settings['configDirectory'] . "/plugin." . $plugin . ".json";
    $js = json_decode(file_get_contents("php://input"), true);
    file_put_contents($cfgFile, json_encode($js, JSON_PRETTY_PRINT));

    return GetPluginJSON();
}
/////////////////////////////////////////////////////////////////////////////

function GetFPPSystems()
{
    global $settings;

    $result = Array();
    exec("ip addr show up | grep 'inet ' | awk '{print $2}' | cut -f1 -d/ | grep -v '^127'", $localIPs);

    $found = Array();
    
    $discovered = json_decode(@file_get_contents("http://localhost:32322/fppd/multiSyncSystems"), true);
    foreach ($discovered['systems'] as $system) {

        if (preg_match('/^169\.254/', $system['address']))
            continue;

        $elem = Array();
        $elem['HostName'] = $system['hostname'];
        $found[$system['address']] = 1;

        $elem['HostDescription'] = ''; # will be filled in later via AJAX
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

	if ((!isset($settings['AvahiDiscovery'])) || ($settings['AvahiDiscovery'] == '0'))
		returnJSON($result);

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
        
		$elem = Array();
		$elem['HostName'] = $parts[3];
		$elem['HostDescription'] = ''; # will be filled in later via AJAX
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
	global $args, $SUDO, $debug, $settings;

	if ($card != 0 && file_exists("/proc/asound/card$card")) {
		exec($SUDO . " sed -i 's/card [0-9]/card ".$card."/' /root/.asoundrc", $output, $return_val);
		unset($output);
		if ($return_val) {
			error_log("Failed to set audio to card $card!");
			return;
		}
        if ( $debug ) {
			error_log("Setting to audio output $card");
        }
	} else if ($card == 0) {
		exec($SUDO . " sed -i 's/card [0-9]/card ".$card."/' /root/.asoundrc", $output, $return_val);
		unset($output);
		if ($return_val) {
			error_log("Failed to set audio back to default!");
			return;
		}
		if ( $debug )
			error_log("Setting default audio");
	}
    // need to also reset mixer device
    $AudioMixerDevice = exec("sudo amixer -c $card scontrols | head -1 | cut -f2 -d\"'\"", $output, $return_val);
    unset($output);
    if ($return_val == 0) {
        WriteSettingToFile("AudioMixerDevice", $AudioMixerDevice);
        if ($settings['Platform'] == "Raspberry Pi" && $card == 0) {
            $type = exec("sudo aplay -l | grep \"card $card\"", $output, $return_val);
            if (strpos($type, '[bcm') !== false) {
                WriteSettingToFile("AudioCard0Type", "bcm");
            } else {
                WriteSettingToFile("AudioCard0Type", "unknown");
            }
            unset($output);
        } else {
            WriteSettingToFile("AudioCard0Type", "unknown");
        }
    }
	return $card;
}

/////////////////////////////////////////////////////////////////////////////

function GetOutputProcessors()
{
	global $settings;

	$jsonStr = "";

	if (file_exists($settings['outputProcessorsFile'])) {
		$jsonStr = file_get_contents($settings['outputProcessorsFile']);
	}

	returnJSONStr($jsonStr);
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

	returnJSONStr($jsonStr);
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

	returnJSONStr($jsonStr);
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
			"SSID=\"%s\"\n" .
			"PSK=\"%s\"\n" .
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
	returnJSONStr(SendCommand("GetTestMode"));
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
function GetSystemInfoJson() {
    global $args;
    return GetSystemInfoJsonInternal(false, isset($args['simple']));
}

function GetSystemInfoJsonInternal($return_array = false, $simple = false)
{
    global $settings;

    //close the session before we start, this removes the session lock and lets other scripts run
    session_write_close();

    //Default json to be returned
    $result = array();
    $result['HostName'] = $settings['HostName'];
	$result['HostDescription'] = !empty($settings['HostDescription']) ? $settings['HostDescription'] : "";
	$result['Platform'] = $settings['Platform'];
    $result['Variant'] = isset($settings['Variant']) ? $settings['Variant'] : '';
    $result['Mode'] = $settings['fppMode'];
    $result['Version'] = getFPPVersion();
    $result['Branch'] = getFPPBranch();
    
    if (file_exists($settings['mediaDirectory'] . "/fpp-info.json")) {
        $content = file_get_contents($settings['mediaDirectory'] . "/fpp-info.json");
        $json = json_decode($content, true);
        $result['channelRanges'] = $json['channelRanges'];
        $result['majorVersion'] = $json['majorVersion'];
        $result['minorVersion'] = $json['minorVersion'];
    }
    
    if (! $simple) {
        //Get CPU & memory usage before any heavy processing to try get relatively accurate stat
        $result['Utilization']['CPU'] =  get_server_cpu_usage();
        $result['Utilization']['Memory'] = get_server_memory_usage();
        $result['Utilization']['Uptime'] = get_server_uptime(true);

        $IPs = explode("\n",trim(shell_exec("/sbin/ifconfig -a | cut -f1 -d' ' | grep -v ^$ | grep -v lo | grep -v eth0:0 | grep -v usb | grep -v SoftAp | grep -v 'can.' | sed -e 's/://g' | while read iface ; do /sbin/ifconfig \$iface | grep 'inet ' | awk '{print \$2}'; done")));

        $result['Kernel'] = get_kernel_version();
        $result['LocalGitVersion'] = get_local_git_version();
        $result['RemoteGitVersion'] = get_remote_git_version(getFPPBranch());
        $result['IPs'] = $IPs;
    }

    //Return just the array if requested
	if ($return_array == true) {
		return $result;
	} else {
		returnJSON($result);
	}
}
    
function SetBBBLeds() {
    file_put_contents("/tmp/setBBBLeds", "#!/bin/bash\n. /opt/fpp/scripts/common\n. /opt/fpp/scripts/functions\n configureBBBLeds");
    shell_exec("sudo bash /tmp/setBBBLeds");
    unlink("/tmp/setBBBLeds");
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
    
    
function ClearPersistentNetNames()
{
    shell_exec("sudo rm -f /etc/systemd/network/5?-fpp-*.link");
}
function CreatePersistentNetNames()
{
	global $settings;
    shell_exec("sudo rm -f /etc/systemd/network/5?-fpp-*.link");
    $interfaces = explode("\n",trim(shell_exec("/sbin/ifconfig -a | cut -f1 -d' ' | grep -v ^$ | grep -v lo | grep -v eth0:0 | grep -v usb | grep -v SoftAp | grep -v 'can.' | grep -v tether ")));
    $count = 0;
    foreach ($interfaces as $iface) {
        $iface = preg_replace("/:$/", "", $iface);
        $cmd = "ip link show " . $iface . " | grep ether | awk '{split($0, a,\" \"); print a[2];}'";
        exec($cmd, $output, $return_val);
        
        $cont = "[Match]\nMACAddress=" . $output[0];
        if (substr($iface, 0, strlen("wlan")) === "wlan") {
            $cont = $cont . "\nOriginalName=wlan*";
        }
        $cont = $cont . "\n[Link]\nName=" . $iface . "\n";
        file_put_contents("/tmp/5" . strval($count) . "-fpp-" . $iface . ".link", $cont);
        shell_exec("sudo mv /tmp/5" . strval($count) . "-fpp-" . $iface . ".link /etc/systemd/network/");

        unset($output);
        $count = $count + 1;
    }
    
}
?>
