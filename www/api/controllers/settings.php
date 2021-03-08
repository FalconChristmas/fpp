<?

function ReadSettingsJSON() {
    global $settings;
    $json = file_get_contents($settings['fppDir'] . '/www/settings.json');

    return json_decode($json, true);
}

function GetSetting() {
    global $settings;

    $sInfos = ReadSettingsJSON();
    $settingName = params('SettingName');

    if (isset($sInfos['settings'][$settingName])) {
        $sInfo = $sInfos['settings'][$settingName];
    } else {
        $sInfo = Array();
    }

    if (isset($settings[$settingName]))
        $sInfo['value'] = $settings[$settingName];

    if (isset($sInfo['optionsURL'])) {
        $json = "";
        if (preg_match('/^\//', $sInfo['optionsURL'])) {
            $json = file_get_contents($sInfo['optionsURL']);
        } else {
            $json = file_get_contents("http://" . $_SERVER['SERVER_ADDR'] . "/" . $sInfo['optionsURL']);
        }

        $sInfo['options'] = json_decode($json, true);
    }

    return json($sInfo);
}

function PutSetting() {
    global $SUDO;
    $value = file_get_contents('php://input');
    $setting = params('SettingName');

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


    $status = array("status"=> "OK");
    return json($status);
}

function GetSettings() {
    global $settings;
    return file_get_contents($settings['fppDir'] . '/www/settings.json');
}

/////////////////////////////////////////////////////////////////////////////
function GetTime() {
    $result = Array();
    //$result['time'] = date('D M d H:i:s T Y'); // Apache needs restarting after a timezone change
    $result['time'] = exec('date');
    return json($result);
}

?>
