<?php

$skipJSsettings = 1;
require_once('common.php');
require_once('common/metadata.php');
require_once('common/settings.php');
require_once('commandsocket.php');
require_once('universeentry.php');


$a = session_id();
if(empty($a))
{
	session_start();
}
$_SESSION['session_id'] = session_id();


$command_array = Array(
	//"getOutputProcessors" => 'GetOutputProcessors', // replaced by GET /api/channel/output/processors
	//"setOutputProcessors" => 'SetOutputProcessors', // replaced by POST /api/channel/output/processors
	"getChannelOutputs"   => 'GetChannelOutputs', // Kept for xLights
	"setChannelOutputs"   => 'SetChannelOutputs', // Kept for xLights
	//"setUniverses"        => 'SetUniverses', // Deprecated, moved to UI.
	// "getDNSInfo"          => 'GetDNSInfo', // Replaced by GET /api/network/dns
	// "setDNSInfo"          => 'SetDNSInfo', // Replaced by POST /api/network/dns
	//"getFPPDUptime"       => 'GetFPPDUptime', // replaced by  /api/fppd/status
	// "applyInterfaceInfo"  => 'ApplyInterfaceInfo', // replaced by POST /api/network/interface/:interface/apply
	//"getInterfaceInfo"    => 'GetInterfaceInfoJson', // replaced by GET /api/network/interface/:interface
	//"setInterfaceInfo"    => 'SetInterfaceInfo', // replaced by POST /api/network/interface/:interface
	"getFPPSystems"       => 'GetFPPSystems', // NOT USED in UI. - Kept for Multisync 3.x,4.x but replacement is /api/system/status
	"getFPPstatus"		  => 'GetFPPStatusJson', // Kept for Multisync but replacement is /api/system/status
	"getSetting"          => 'GetSetting', // GET /api/settings:settingKey -- Restored. Somethings still use this
	"setSetting"          => 'SetSetting', // PUT /api/settings:settingKey -- Restored. Somethings still use this 
	// "startSequence"       => 'StartSequence', // GET /api/sequence/:SequenceName/start/:startSecond
	// "stopSequence"        => 'StopSequence', // GET /api/sequence/current/stop
	//"toggleSequencePause" => 'ToggleSequencePause', // GET /api/sequence/current/togglePause
	//"singleStepSequence"  => 'SingleStepSequence', // GET /api/sequence/current/step
	// "singleStepSequenceBack" => 'SingleStepSequenceBack', // GET /api/sequence/current/stepBack
	// "getPlayListEntries"  => 'GetPlayListEntries', // GET /api/playlist/:PlaylistName
	// "getSequenceInfo"     => 'GetSequenceInfo', // GET /api/sequence/:SequenceName/meta
	//"getMediaDuration"    => 'getMediaDurationInfo', // GET /api/media/:MediaName/duration
	// "getFileSize"         => 'getFileSize', // GET /media/:MediaName/meta
	//"copyFile"            => 'CopyFile', // POST /file/:DirName/copy/:source/:dest
	// "renameFile"          => 'RenameFile', // POST /file/:DirName/rename/:source/:dest
	"getPluginSetting"    => 'GetPluginSetting',
	"setPluginSetting"    => 'SetPluginSetting',
    "getPluginJSON"       => 'GetPluginJSON',
    "setPluginJSON"       => 'SetPluginJSON',
	// "saveScript"          => 'SaveScript', // POST /api/scripts/:scriptName
	// "setTestMode"         => 'SetTestMode', // PUT /testmode
	// "getTestMode"         => 'GetTestMode', // GET //testmode
	"setupExtGPIO"        => 'SetupExtGPIOJson',
	"extGPIO"             => 'ExtGPIOJson',
    "getSysInfo"          => 'GetSystemInfoJson', // Still used by MultiSync
    "getHostNameInfo"     => 'GetSystemHostInfo', // Still used by MultiSync
    // "clearPersistentNetNames" => 'ClearPersistentNetNames', // use DELETE /api/network/presisentName
    // "createPersistentNetNames" => 'CreatePersistentNetNames' // use POST /api/network/presisentName
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

// This is now just a redirect to new API.
function GetFPPStatusJson()
{
    global $_SERVER;

    //close the session before we start, this removes the session lock and lets other scripts run
    session_write_close();

    //QUERY_STRING is always populated because this is getFPPStatus()
    $url = "http://localhost/api/system/status?". $_SERVER['QUERY_STRING'];

    $json = file_get_contents($url);
    if ($json === false) {
        returnJSON(array("status" => "ERROR: Unable to redirect", "url" => $url));
    } else {
        returnJSON(json_decode($json, true));
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

	$setting = htmlspecialchars($args['key']);
	$value   = htmlspecialchars($args['value']);
	$plugin  = htmlspecialchars($args['plugin']);

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
        $elem['typeId'] = $system['typeId'];
        $elem['majorVersion'] = $system['majorVersion'];
        $elem['minorVersion'] = $system['minorVersion'];
        $elem['model'] = $system['model'];
        $elem['version'] = $system['version'];
        $elem['lastSeen'] = $system['lastSeen'];
        $elem['lastSeenStr'] = $system['lastSeenStr'];
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
    $result['OSVersion'] = trim(file_get_contents('/etc/fpp/rfs_version'));

    $os_release = "Unknown";
    if (file_exists("/etc/os-release"))
    {
        $info = parse_ini_file("/etc/os-release");
        if (isset($info["PRETTY_NAME"]))
            $os_release = $info["PRETTY_NAME"];
        unset($output);
    }
    $result['OSRelease'] = $os_release;
    
    if (file_exists($settings['mediaDirectory'] . "/fpp-info.json")) {
        $content = file_get_contents($settings['mediaDirectory'] . "/fpp-info.json");
        $json = json_decode($content, true);
        $result['channelRanges'] = $json['channelRanges'];
        $result['majorVersion'] = $json['majorVersion'];
        $result['minorVersion'] = $json['minorVersion'];
        $result['typeId'] = $json['typeId'];
    }

    $output = array();
    exec("/opt/fpp/scripts/get_uuid", $output);
    $result['uuid'] = $output[0];
    
    if (! $simple) {
        //Get CPU & memory usage before any heavy processing to try get relatively accurate stat
        $result['Utilization']['CPU'] =  get_server_cpu_usage();
        $result['Utilization']['Memory'] = get_server_memory_usage();
        $result['Utilization']['Uptime'] = get_server_uptime(true);
        
        $result['Kernel'] = get_kernel_version();
        $result['LocalGitVersion'] = get_local_git_version();
        $result['RemoteGitVersion'] = get_remote_git_version(getFPPBranch());

        if (isset($settings['UpgradeSource']))
            $result['UpgradeSource'] = $settings['UpgradeSource'];
        else
            $result['UpgradeSource'] = 'github.com';
        
        $output = array();
        $IPs = array();
        exec("ip --json -4 address show", $output);
        //print(join("", $output));
        $ipAddresses = json_decode(join("", $output), true);
        foreach($ipAddresses as $key => $value) {
            if ($value["ifname"] != "lo" && strpos($value["ifname"], 'usb') === false) {
                foreach($value["addr_info"] as $key2 => $value2) {
                    $IPs[] = $value2["local"];
                }
            }
        }

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
        if (substr($iface, 0, strlen("wl")) === "wl") {
            $cont = $cont . "\nOriginalName=wl*";
        }
        $cont = $cont . "\n[Link]\nName=" . $iface . "\n";
        file_put_contents("/tmp/5" . strval($count) . "-fpp-" . $iface . ".link", $cont);
        shell_exec("sudo mv /tmp/5" . strval($count) . "-fpp-" . $iface . ".link /etc/systemd/network/");

        unset($output);
        $count = $count + 1;
    }
    
}

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

	$setting = htmlspecialchars($args['key']);
	$value   = htmlspecialchars($args['value']);

	check($setting, "setting", __FUNCTION__);
	check($value, "value", __FUNCTION__);

	WriteSettingToFile($setting, $value);

	if (startsWith($setting, "LogLevel")) {
		SendCommand("LogLevel,$setting,$value,");
	} else if ($setting == "HostName") {
		$value = preg_replace("/[^-a-zA-Z0-9]/", "", $value);
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

?>
