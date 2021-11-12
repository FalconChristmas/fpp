<?php
$skipHTMLCodeOutput = false;
//Check if this script is being called directly for is being included on another script
//If included we'll skip printing the HTML code for the page as it's likely we just want to access functions to generate a backup
function checkDirectScriptExecution()
{
	global $skipJSsettings, $skipHTMLCodeOutput;
	$incl_files = get_included_files();
	//script in the first index should be the script were executing
	if ($incl_files[0] != __FILE__) {
		$skipJSsettings = 1;
		$skipHTMLCodeOutput = true;
	}
}
checkDirectScriptExecution();

//Stop config.php spitting out any JS used in the UI, not needed on the backup page, if left as is the JSON download will have the <script> tag in it due to
//that data already being in the buffer
//so change the flag so it doesn't get output when the request is a post request (which is the user submitting the backup form)
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
   $skipJSsettings = 1;
}
//error_reporting(E_ALL);
//ini_set('display_errors', 1);

//Include other scripts
require_once('common.php');
require_once('common/settings.php');
require_once('commandsocket.php');
//Includes for API access
//require_once('fppjson.php');
//require_once('fppxml.php');

//Define a map of backup/restore areas and setting locations, this is also used to populate the area select lists
$system_config_areas = array(
    'all' => array('friendly_name' => 'All', 'file' => false),
    'channelInputs' => array('friendly_name' => 'Channel Inputs (E1.31 Bridge)', 'file' => $settings['universeInputs']),
    'channelOutputs' =>
        array(
            'friendly_name' => 'Channel Outputs (Universe, Falcon, LED Panels, etc.)',
            'file' => array(
                'universes' => array('type' => 'file', 'location' => $settings['universeOutputs']),
                'falcon_pixelnet_DMX' => array('type' => 'function', 'location' => array('backup' => 'LoadPixelnetDMXFile_FPDv1', 'restore' => 'SavePixelnetDMXFile_FPDv1')),
                'pixel_strings' => array('type' => 'file', 'location' => $settings['co-pixelStrings']),
                'bbb_strings' => array('type' => 'file', 'location' => $settings['co-bbbStrings']),
                'led_panels' => array('type' => 'file', 'location' => $settings['channelOutputsJSON']),
                'other' => array('type' => 'file', 'location' => $settings['co-other']),
            ),
            'special' => true
        ),
    'channelmemorymaps' => array('friendly_name' => 'Pixel Overlay Models Old(Channel Memory Maps)', 'file' => $settings['mediaDirectory'] . "/channelmemorymaps"),
	'gpio-input' => array('friendly_name' => 'GPIO Input Triggers ', 'file' => $settings['configDirectory'] . "/gpio.json"),
	'model-overlays' => array('friendly_name' => 'Pixel Overlay Models', 'file' => $settings['model-overlays']),
	'misc_configs' =>
		array(
			'friendly_name' => 'Miscellaneous Settings (Additional Plugins Configs, System Settings, etc.)',
			'file' => array(
				'configs' => array('type' => 'function', 'location' => array('backup' => 'BackupConfigFolderConfigs', 'restore' => 'RestoreConfigFolderConfigs')),
			),
			'special' => true
	),
    'outputProcessors' => array('friendly_name' => 'Output Processors', 'file' => $settings['outputProcessorsFile']),
	'show_setup' =>
        array(
            'friendly_name' => 'Show Setup (Events, Playlists, Schedule, Scripts)',
            'file' => array(
                'events' => array('type' => 'dir', 'location' => $eventDirectory),
                'playlist' => array('type' => 'dir', 'location' => $playlistDirectory),
                'schedule' => array('type' => 'file', 'location' => $settings['scheduleJsonFile']),
                'scripts' => array('type' => 'dir', 'location' => $scriptDirectory),
            ),
            'special' => true
        ),
    'settings' =>
        array(
            'friendly_name' => 'System Settings (incl. Command Presets, Email, Proxies, Timezone)',
            'file' => array(
                'system_settings' => array('type' => 'file', 'location' => $settingsFile),
				'commandPresets' => array('type' => 'file', 'location' => $settings['configDirectory'] . "/commandPresets.json"),
				'proxies' => array('type' => 'file', 'location' => $settings['configDirectory'] . "/proxies"),
				'email' => array('type' => 'file', 'location' => false),
				'timezone' => array('type' => 'function', 'location' => array('backup' => 'ReadTimeZone', 'restore' => '')) //We'll handle restore ourselves
            ),
            'special' => true
        ),
    'plugins' => array(
        'friendly_name' => 'Plugin Settings',
        'file' => array(),
        'special' => true
    ),
    'network' => array(
		'friendly_name' => 'Network Settings (Wired & WiFi, DNS)',
		'file' => array(
            //Std. Network Interfaces
			'wired_network' => array('type' => 'file', 'location' =>  $settings['configDirectory']. "/interface.eth0"),
			'wifi_network' => array('type' => 'file', 'location' =>  $settings['configDirectory']. "/interface.wlan0"),
			//Manual/Custom DNS Settings stored here
			'dns' => array('type' => 'file', 'location' => $settings['configDirectory']. "/dns"),
		),
		'special' => true
	)
);

//FPP Backup version
$fpp_backup_version = "5";
//FPP Backup files directory
$fpp_backup_location = $settings['configDirectory'] . "/backups";
//Flag whether the users browser will download the JSON file
$fpp_backup_prompt_download = true;
//Max age for stored settings backup files, Default: 90 days
$fpp_backup_max_age = 90;
//Minimum number of backups we want to keep, this works with max age to ensure we have at least some backups left and not wipe out all of them if they were all old. Default: 14
$fpp_backup_min_number_kept = 14;
//Hold any backup error messages here
$backup_errors = array();

//Array of known plugins - initially empty, populated dynamically
$system_active_plugins = array();

//NOTE: - ONLY FOR PLUGINS AT CURRENT
//Extra backup locations act as a map between any extra files and a plugin it's related to.
//there no way to programmatically know or discover what configurations files a plugin is using outside the normal "plugin.<plugin-name>.json" configuration file
//$extra_backup_locations = json_decode(file_get_contents($settings['fppDir'] . '/www/backup_locations.json'), true);

//Populate plugins by looking for their config files (which is what we want to backup anyway, if a config file exists therefore configuration is set for that plugin)
$system_config_areas['plugins']['file'] = retrievePluginList();

//Populate extra network interfaces, this will help bring in extra configured interfaces like USB eth or extra USB Wifi
retrieveNetworkInterfaces();

//Preserve some existing settings by default
$keepMasterSlaveSettings = true;
$keepNetworkSettings = true;
//Encrypt/obfuscate passwords and other sensitive data
$protectSensitiveData = true;
//The final value is read from the uploaded file on restore, used to skip some parts of the restore operation
$uploadDataProtected = true;

//list of settings restored
$settings_restored = array();
//restore done state
$restore_done = false;
//Restored Network Setting tracking
$network_settings_restored = false;
$network_settings_restored_post_apply = array('wired_network' => "", 'wifi_network' => "");
$network_settings_restored_applied_ips = array('wired_network' => array(), 'wifi_network' => array());

//Array of settings by name/key name, that are considered sensitive/taboo
$sensitive_data = array('emailgpass', 'password', 'secret');

//Lookup arrays for what is a json and a ini file
$known_json_config_files = array('channelInputs', 'gpio-input', 'channelOutputs', 'commandPresets', 'outputProcessors', 'universes', 'pixel_strings', 'bbb_strings', 'led_panels', 'other', 'model-overlays');
$known_ini_config_files = array('settings', 'system_settings', 'network', 'wired', 'wifi');

//Remove BBB Strings from the system areas if we're on a Pi or any other platform that isn't a BBB
if ($settings['Platform'] != "BeagleBone Black") {
    unset($system_config_areas['channelOutputs']['file']['bbb_strings']);
}

/**
 * Handle POST for download or restore
 * Check which submit button was pressed
 */
if (isset($_POST['btnDownloadConfig'])) {
    //////
    /// BACKUP
    /////
    if (isset($_POST['backuparea']) && !empty($_POST['backuparea'])) {
        if (isset($_POST['protectSensitive'])) {
            $protectSensitiveData = true;
        } else {
            $protectSensitiveData = false;
        }

        //this value *SHOULD* directly match a key in $system_config_areas
        $area = $_POST['backuparea'];

        //Do the work to backup settings
		performBackup($area, true);
    }

} else if (isset($_POST['btnRestoreConfig'])) {
    //////
    /// RESTORE
    /////
    if (isset($_POST['restorearea']) && !empty($_POST['restorearea'])) {
        $restore_area = $_POST['restorearea'];
        $restore_area_main = "";

        if (isset($_POST['keepExitingNetwork'])) {
            $keepNetworkSettings = true;
        } else {
            $keepNetworkSettings = false;
        }

        if (isset($_POST['keepMasterSlave'])) {
            $keepMasterSlaveSettings = true;
        } else {
            $keepMasterSlaveSettings = false;
        }

        //if restore area contains a forward slash, then we want to restore into a sub-area
        //split string to get the main area and sub-area
        if (stripos($restore_area, "/") !== false) {
            $restore_area_arr = explode("/", $restore_area);
            $restore_area_main = $restore_area_arr[0];//main area is first
        } else {
            $restore_area_main = $restore_area;
        }

        //this value *SHOULD* directly match a key in $system_config_areas
        if (array_key_exists($restore_area_main, $system_config_areas)) {
            //Do something with the uploaded file to restore it
            if (array_key_exists('conffile', $_FILES)) {
                //data is stored by area and keyed the same as $system_config_areas
                //read file, decode json
                //parse each area and write settings/restore data

                $rstfname = $_FILES['conffile']['name'];
                $rstftmp_name = $_FILES['conffile']['tmp_name'];
                //file contents
                $file_contents = file_get_contents($rstftmp_name);
                //decode back into an array
                $file_contents_decoded = json_decode($file_contents, true);

                //successful decode
                if ($file_contents_decoded !== FALSE && is_array($file_contents_decoded)) {
                    //Get value of protected state and remove it from the array
                    if (array_key_exists('protected', $file_contents_decoded)) {
                        $uploadDataProtected = $file_contents_decoded['protected'];
                        unset($file_contents_decoded['protected']);
                    }

                    //work out of backup file is version 2 or not
                    //if it's not a version 2 file, then we can only really restore settings
                    //email can be restored because it's contained in the settings

                    //Version 2 backups need to restore the schedule file to the old locations (auto converted on FPPD restart)
					//Version 3 backups need to restore the schedule to a different file
					$is_version_2_backup = false;
					$is_version_3_backup = false;
                    //Check backup version
					if (array_key_exists('fpp_backup_version', $file_contents_decoded)) {
						$_fpp_backup_version = $file_contents_decoded['fpp_backup_version']; //Minimum version is 2

						if ($file_contents_decoded['fpp_backup_version'] == 2) {
							$is_version_2_backup = true;
						} else if ($file_contents_decoded['fpp_backup_version'] = $fpp_backup_version) {
							$is_version_3_backup = true;
						}
					}

                    //Remove the platform key as it's not used for anything yet
                    unset($file_contents_decoded['platform']);
                    unset($file_contents_decoded['fpp_backup_version']);

                    //Restore all areas
                    if (strtolower($restore_area) == "all" && ($is_version_2_backup || $is_version_3_backup)) {
                        // ALL SETTING RESTORE
                        //read each area and process it
                        foreach ($file_contents_decoded as $restore_area_key => $area_data) {
                            //Pass the restore area and data to the restore function
                            $restore_done = processRestoreData($restore_area_key, $area_data, $_fpp_backup_version);
                        }

//                    } else if (strtolower($restore_area) == "email" && $is_version_2_backup) {
//                        //get email settings, from the actual system settings
//                        $area_data = $file_contents_decoded['settings']['system_settings'];
//                        //Pass the restore area and data to the restore function
//                        $restore_done = processRestoreData("email", $area_data);
//                    }
                        //Restore all other areas, settings can be restored regardless of version
                        //if the area is not settings or it's not a v2 backup then nothing will be done
                    } else {
                        // ALL OTHER SETTING RESTORE
                        //Process specific restore areas, this work almost like the 'all' area
                        //general settings, but only a matching area is cherry picked

                        //If the key exists in the decoded data then we can process
                        if (array_key_exists($restore_area_main, $file_contents_decoded)) {
                            $restore_area_key = $restore_area_main;
                            $area_data = $file_contents_decoded[$restore_area_main];

                            //If we're restoring channelOutputs, we might need the system settings.. eg when restoring the LED panel data we need to set the layout in settings
                            if ($restore_area_key == "channelOutputs" && array_key_exists('settings', $file_contents_decoded)) {
                                $system_settings = array();
                                if (array_key_exists('system_settings', $file_contents_decoded['settings'])) {
                                    $system_settings = $file_contents_decoded['settings']['system_settings'];
                                    //modify the area data
                                    $area_data_new = array('area_data' => $area_data, 'system_settings' => $system_settings);
                                    $area_data = $area_data_new;
                                }
                            }

                            //Pass the restore area and data to the restore function
                            $restore_done = processRestoreData($restore_area, $area_data, $_fpp_backup_version);
                        }
                    }

                    //All processed
//                    $restore_done = true;
                } else {
					$backup_error_string = "RESTORE: The backup " . $rstfname . " data could not be decoded properly. Is it a valid backup file?";
					$backup_errors[] = $backup_error_string;
                    error_log($backup_error_string);
                }
            }
        }
    }
}

/**
 * Removes any sensitive data from the input array
 *
 * @param $input_array
 * @return mixed
 */
function remove_sensitive_data($input_array)
{
    global $sensitive_data, $protectSensitiveData;

    //Remove any sensitive data
    if ($protectSensitiveData == true) {
        //loop over the areas

        foreach ($input_array as $set_key => $data_arr) {
            //If there is a sub array or an array in $data_arr, then loop into it
            if (is_array($data_arr)) {
                foreach ($data_arr as $key_name => $key_data) {
                    if (in_array(strtolower($key_name), $sensitive_data) && is_string($key_name)) {
                        $input_array[$set_key][$key_name] = '';
                    }
                }
            } else {
                //Else process keys in this array
                if (in_array(strtolower($set_key), $sensitive_data) && is_string($set_key)) {
                    $input_array[$set_key] = '';
                }
            }
        }
    }

    return $input_array;
}

/**
 * Looks in a directory and reads file contents of files within it
 * @param $directory String directory to search in
 * @param $return_data Boolean switch to include file data
 * @param bool $sort_by_date Boolean sort results by date oldest to newest
 * @return array Array of file names and respective data
 */
function read_directory_files($directory, $return_data = true, $sort_by_date = false)
{
    $file_list = array();
    $file_data = false;

    if ($handle = opendir($directory)) {
        while (false !== ($file = readdir($handle))) {
            // do something with the file
            // note that '.' and '..' is returned even
            // if file isn't this directory or its parent, add it to the results
            // also must include ._* files as binary files that OSX may create
            if ($file[0] != "." || (strlen($file) > 1 && $file[1] != "." && $file[1] != "_")) {
                // collect the filenames & data
                if ($return_data == true) {
                    $file_data = explode("\n", file_get_contents($directory . '/' . $file));
                }

                $file_list[$file] = $file_data;
            }
        }
        closedir($handle);
    }

	//Sort the results if sort flag  is true
	if ($sort_by_date == true) {
		//Modified version of https://stackoverflow.com/questions/2667065/sort-files-by-date-in-php
		//uksort to sort by key & insert out directory variable
		uksort($file_list, function ($a, $b) use ($directory) {
			return filemtime($directory . "/" . $a) - filemtime($directory . "/" . $b);
		});
	}

    return $file_list;
}

/**
 * Function to look after backup restorations
 * @param $restore_area String  Area to restore
 * @param $restore_area_data array  Area data as an array
 * @param $backup_version boolean Version of the backup
 * @return boolean Save result
 */
function processRestoreData($restore_area, $restore_area_data, $backup_version)
{
    global $SUDO, $settings, $mediaDirectory, $scheduleFile, $system_config_areas, $keepMasterSlaveSettings, $keepNetworkSettings, $uploadDataProtected, $settings_restored,
           $network_settings_restored, $network_settings_restored_post_apply, $network_settings_restored_applied_ips,
           $known_ini_config_files, $known_json_config_files;
    global $args;

    $restore_area_key = $restore_area_sub_key = "";
    $save_result = false;
    $restore_area_system_settings = array();

    //if restore area contains a forward slash, then we want to restore into a sub-area
    //split string to get the main area and sub-area
    if (stripos($restore_area, "/") !== false) {
        $restore_area_arr = explode("/", $restore_area);
        $restore_area_key = $restore_area_arr[0];//main area is first
        $restore_area_sub_key = $restore_area_arr[1];//sub-area is 2nd
    } else {
        $restore_area_key = $restore_area;
    }

    //set the initial value
    $settings_restored[$restore_area_key] = $save_result;

    //Check to see if the area data contains the area_data or system_settings keys
    //break them out if so & overwrite $restore_area_data with the actual area data
    if (array_key_exists('area_data', $restore_area_data) && array_key_exists('system_settings', $restore_area_data)) {
        $restore_area_system_settings = $restore_area_data['system_settings'];
        $restore_area_data = $restore_area_data['area_data'];
    }

    //Should probably skip restoring data if the restore data is actually empty (maybe no config existed when the backup was made)
    //this is also avoid false negatives of restore failures for areas where there wasn't any data to restore
	$restore_data_is_empty = (empty($restore_area_data) || is_null($restore_area_data)) ? true : false;

	//////////////////////////////////
	//// Handle processing of each restore area
    //////////////////////////////////
    //OutputProcessors - OutputProcessors
    if ($restore_area_key == "outputProcessors" && !$restore_data_is_empty) {
        //Just overwrite the Output processors file
		$settings_restored[$restore_area_key]['ATTEMPT'] = true;
		$channel_remaps_json_filepath = $system_config_areas['outputProcessors']['file'];
		//PrettyPrint the JSON data and save it
		$outputProcessors_data = prettyPrintJSON(json_encode($restore_area_data));

		if (file_put_contents($channel_remaps_json_filepath, $outputProcessors_data) === FALSE) {
			$save_result = false;
		} else {
			$save_result = true;
		}
        $settings_restored[$restore_area_key]['SUCCESS'] = $save_result;

        //Set FPPD restart flag
        WriteSettingToFile('restartFlag', 1);
    }

    //CHANNEL MEMORY MAPS - PIXEL OVERLAYS
    if ($restore_area_key == "channelmemorymaps" && !$restore_data_is_empty) {
        //Overwrite channel outputs JSON
        $channelmemorymaps_filepath = $system_config_areas['channelmemorymaps']['file'];
		$settings_restored[$restore_area_key]['ATTEMPT'] = true;

		$data = implode("\n", $restore_area_data);

		if (file_put_contents($channelmemorymaps_filepath, $data) === FALSE) {
			$save_result = false;
		} else {
			$save_result = true;
		}

        $overlays_json_filepath = $system_config_areas['model-overlays']['file'];
        //PrettyPrint the JSON data and save it
        $outputProcessors_data = prettyPrintJSON(json_encode($restore_area_data));

        if (file_put_contents($overlays_json_filepath, $outputProcessors_data) === FALSE) {
            $save_result = false;
        } else {
            $save_result = true;
        }
        $settings_restored[$restore_area_key]['SUCCESS'] = $save_result;

        //Set FPPD restart flag
        WriteSettingToFile('restartFlag', 1);
    }

    //CHANNEL INPUTS - E1.31 BRIDGE
	if ($restore_area_key == "channelInputs" && !$restore_data_is_empty) {
		//Just overwrite the channeInputs file
		$channelInputs_filepath = $system_config_areas['channelInputs']['file'];

		$settings_restored[$restore_area_key]['ATTEMPT'] = true;
		//PrettyPrint the JSON data and save it
		$channelInputs_data = prettyPrintJSON(json_encode($restore_area_data));
		if (file_put_contents($channelInputs_filepath, $channelInputs_data) === FALSE) {
			$save_result = false;
		} else {
			$save_result = true;
		}

		$settings_restored[$restore_area_key]['SUCCESS'] = $save_result;
	}

	//GPIO INPUTS - GPIO Input Triggers
	if ($restore_area_key == "gpio-input" && !$restore_data_is_empty) {
		//Just write strain to the gpio.json file
		$gpioInputs_filepath = $system_config_areas['gpio-input']['file'];

		$settings_restored[$restore_area_key]['ATTEMPT'] = true;
		//PrettyPrint the JSON data and save it
		$gpioInputs_data = prettyPrintJSON(json_encode($restore_area_data));
		if (file_put_contents($gpioInputs_filepath, $gpioInputs_data) === FALSE) {
			$save_result = false;
		} else {
			$save_result = true;
		}

		$settings_restored[$restore_area_key]['SUCCESS'] = $save_result;
	}

    //SHOW SETUP & CHANNEL OUTPUT RESTORATION
    if (($restore_area_key == "show_setup" || $restore_area_key == "channelOutputs" || $restore_area_key == "misc_configs") && !$restore_data_is_empty) {
        $settings_restored[$restore_area_key] = array();

        $script_filenames = array();
        $restore_areas = $system_config_areas[$restore_area_key]['file'];

        //search through the files that should of been backed up
        //and then loop over the restore data and match up the data and restore it
        foreach ($restore_areas as $restore_areas_idx => $restore_areas_data) {
            $restore_location = $restore_areas_data['location'];
            $restore_type = $restore_areas_data['type'];
            $final_file_restore_data = "";

            //If $restore_area_sub_key is empty then no sub-area has been chosen -- restore as normal and restore all sub areas
            //Or if $restore_area_sub_key is equal to the $show_setup_area_index we're on, then restore just this area because it matches users selection
            //and break the loop
            if ($restore_area_sub_key == "" || ($restore_area_sub_key == $restore_areas_idx)) {
                //if the restore key and the $system_config_areas key match then restore data to whatever location it is
                //eg. if we are on events, then look for events in the restore data, when found restore data to the events location (eg look at the location)
                foreach ($restore_area_data as $restore_area_data_index => $restore_area_data_data) {
                    $save_result = false;

                    //$restore_area_data_data is an array representing the file contents
                    //$restore_area_data_index represents the filename (used to key the array)
                    if ($restore_areas_idx == $restore_area_data_index) {
                        //data is an array then we can go
                        if (is_array($restore_area_data_data)) {
                            //loop over all the files and their data and restore each
                            //$restore_area_data_data array will look like
                            //array ('event'=> array('01_01.fevt' => array(data), '21_10.fevt' => array(data)))
                            foreach ($restore_area_data_data as $fn_to_restore => $fn_data) {
                                $save_result = false;

                                $restore_location = $restore_areas_data['location']; //reset
                                $final_file_restore_data = ""; //store restore data in variable

                                //Work out what method we need to use to get the data back into an array
                                if ($restore_type == "dir" || $restore_type == "file") {
                                    if (in_array($restore_area_data_index, $known_json_config_files)) {
                                        //JSON
                                        $final_file_restore_data = $fn_data;
                                    } else {
                                        //everything else
                                        //line separate the lines
                                        $final_file_restore_data = implode("\n", $fn_data);
                                    }
                                } else if ($restore_type == "function") {
                                    //get the data as in without any modification so we can pass it into the restore function
                                    $final_file_restore_data = $fn_data;
                                }

                                //If backup/restore type of a sub-area is folder, then build the full path to where the file will be restored
                                if ($restore_type == "dir") {
                                    $restore_location .= "/" . $fn_to_restore;
                                }

                                //if restore sub-area is scripts, capture the file names so we can pass those along through RestoreScripts which will perform any InstallActions
                                if (strtolower($restore_areas_idx) == "scripts") {
                                    $script_filenames[] = $fn_to_restore;
                                }

                                //if restore sub-area is LED panels, we need write the matrix size / layout setting to the settings file in case it's different to the backup
                                if (strtolower($restore_areas_idx) == "led_panels") {
                                    $panel_layout = null;
                                    if (!empty($restore_area_system_settings)) {
                                        $panel_layout = $restore_area_system_settings[0]['LEDPanelsLayout'];
                                        if ($panel_layout != null) {
                                            //LEDPanelsLayout = "4x4"
                                            WriteSettingToFile('LEDPanelsLayout', $panel_layout);
                                        }
                                    }
                                }

								//if restore sub-area is the schedule, determine how to restore it based on the $backup_version
								//Version 2 backups need to restore the schedule file to the old locations (auto converted on FPPD restart)
								//Version 3 backups need to restore the schedule to it's new json location
								if (strtolower($restore_areas_idx) == "schedule") {
									if ($backup_version == 2)
									{
										//Override the restore location so we write to the old schedule file, FPPD will convert this to the new json file
										$restore_location = $scheduleFile;
									}
//									else if ($backup_version == 3){
//										//restore it to the new json file - don't adjust the path it'll go to configured path
//									}
								}

                                //If we have data then write to where it needs to go
                                if (!empty($final_file_restore_data) && ($restore_type == "dir" || $restore_type == "file")) {
									$settings_restored[$restore_area_key][$restore_area_data_index]['ATTEMPT'] = true;

									//Work out what method we need to use to get the data back out into the correct format
                                    if (in_array($restore_area_data_index, $known_json_config_files)) {
                                        //JSON
                                        $final_file_restore_data = prettyPrintJSON(json_encode($final_file_restore_data));
                                    }
                                    //Save out the file
                                    if (file_put_contents($restore_location, $final_file_restore_data) === FALSE) {
                                        $save_result = false;
                                    } else {
                                        $save_result = true;
                                    }
                                } //If restore type is function
                                else if ($restore_type == "function") {
									$settings_restored[$restore_area_key][$restore_area_data_index]['ATTEMPT'] = true;

									$restore_function = $restore_areas_data['location']['restore'];
                                    //if we have valid ata and the function exists, call it
                                    if (function_exists($restore_function) && !empty($final_file_restore_data)) {
                                        $save_result = $restore_function($final_file_restore_data);
                                    }
                                } else {
                                    $save_result = false;
                                }
                            }
                        }
                        $settings_restored[$restore_area_key][$restore_area_data_index]['SUCCESS'] = $save_result;
                        break;//break after data is restored for this section
                    }
                }
                //Don't break if area key is empty, because we want to process all the sub-areas
                if ($restore_area_sub_key != "") {
                    //we found the sub-area, stop looking
                    break;
                }
            }
        }

        //Cause any InstallActions to be run for the restored scripts
        RestoreScripts($script_filenames);

        //Set FPPD Restart flag
        WriteSettingToFile('restartFlag', 1);
    }

    //PLUGIN SETTING RESTORATION
	if ($restore_area_key == "plugins" && !$restore_data_is_empty) {
		if (is_array($restore_area_data)) {
			//Just overwrite the plugin config file
			$plugin_settings_path_base = $settings['configDirectory'];

			//loop over the data and get the plugin name and then write the settings out
			foreach ($restore_area_data as $plugin_name => $plugin_data) {
				$settings_restored[$restore_area_key][$plugin_name]['ATTEMPT'] = true;

				//Version 3 backups need to restore the config file to the config folder generating the filename form the JSON structure
				//Version 4 backups need to restore the config file(s) to the config folder using the parents/node key as the filename
				if ($backup_version <= 3) {
					//Write older plugin configurations to tha assumed filename (normally plugin.<plugin-name>)
					$plugin_settings_path = $plugin_settings_path_base . "/plugin." . $plugin_name;
					$data = implode("\n", $plugin_data[0]);

					//Write the data out
					if (file_put_contents($plugin_settings_path, $data) === FALSE) {
						$save_result = false;
					} else {
						$save_result = true;
					}
				} else {
					//Backup versions 4 and above
					//plugin data is keyed into what file the data is from to make it easier to backup multiple config files per plugin
                    //and plugin config data is written back to the right file

					//Get the filename (to restore data into) and file data to go into that file
					foreach ($plugin_data as $p_data_filename => $p_data_data) {
						//Adjust the path too (use the key as the filename instead of generating it as we did in v3 backups
						$plugin_settings_path = $plugin_settings_path_base . "/" . $p_data_filename;
						//data is also in a different array location
						$data = implode("\n", $p_data_data[0]);

						//Write the data out each time
						if (file_put_contents($plugin_settings_path, $data) === FALSE) {
							$save_result = false;
						} else {
							$save_result = true;
						}
					}
				}

				$settings_restored[$restore_area_key][$plugin_name]['SUCCESS'] = $save_result;
			}
		}
	}

    //SYSTEM - EMAIL - TIMEZONE SETTING RESTORATION
    if ($restore_area_key == "settings") {
        $restore_areas = $system_config_areas[$restore_area_key]['file'];

        //Check what subkey was selected
        //search through the files that should of been backed up
        //and then loop over the restore data and match up the data and restore it
        foreach ($restore_areas as $restore_areas_idx => $restore_areas_data) {
            $restore_data = array();
            //If $restore_area_sub_key is empty then no sub-area has been chosen -- restore as normal and restore all sub areas
            //Or if $restore_area_sub_key is equal to the $restore_areas_idx we're on, then restore just this area, because it matches user selection
            //and break the loop
            if ($restore_area_sub_key == "" || ($restore_area_sub_key == $restore_areas_idx)) {
				$save_result = false;

				//SYSTEM SETTING RESTORATION
                if ($restore_areas_idx == "system_settings") {
                    if (is_array($restore_area_data)) {
						$settings_restored[$restore_area_key][$restore_areas_idx]['ATTEMPT'] = true;
						//get data out of nested array
                        $restore_data = $restore_area_data['system_settings'][0];

                        foreach ($restore_data as $setting_name => $setting_value) {
                            //check if we can change it (default value is checked - true)
                            if ($setting_name == "fppMode") {
                                if ($keepMasterSlaveSettings == false) {
                                    WriteSettingToFile($setting_name, $setting_value);
                                }
                            } else {
                                WriteSettingToFile($setting_name, $setting_value);
                            }

                            //Do special things that require some sort of system change
                            //eg. changing piRTC via GUI will fire off a shell command to set it up
                            //we'll also do this to keep consistency
                            if ($setting_name == "AudioOutput") {
                                $args['value'] = $setting_value;
                                SetAudioOutput($setting_value);
                            } else if ($setting_name == "volume") {
                                SetVolume(trim($setting_value));
                            } else {
                                ApplySetting($setting_name, $setting_value);
                            }
                        }

                        //Restart FFPD so any changes can take effect
//                        RestartFPPD();
                        //Write setting out so the user can restart FPPD, this is a work around for Pi Pixel Strings causing a reboot when it's enabled (FPPD is restated, the Pi is rebooted)
                        WriteSettingToFile('restartFlag', 1);

                        $save_result = true;
                    } else {
                        error_log("RESTORE: Cannot read Settings INI settings. Attempted to parse " . json_encode($restore_data));
                    }
                }

				//COMMAND PRESET RESTORE
				if ($restore_areas_idx == "commandPresets") {
					$settings_restored[$restore_area_key][$restore_areas_idx]['ATTEMPT'] = true;

					//Get the restore data out of the array
					$restore_data = $restore_area_data['commandPresets'][0];

					//Generate the filepath for the config file to be restored
					$commandPresets_filepath = $settings['configDirectory'] . "/commandPresets.json";

					//PrettyPrint the JSON data and save it
					$commandPresets_data_restore = prettyPrintJSON(json_encode($restore_data));
					if (file_put_contents($commandPresets_filepath, $commandPresets_data_restore) === FALSE) {
						$save_result = false;
					} else {
						$save_result = true;
					}
				}

                //EMAIL SETTING RESTORATION
                if ($restore_areas_idx == "email") {
                    //get data out of nested array
                    $restore_data = $restore_area_data['system_settings'][0];

                    //TODO rework this so it will work future email system implementation, were different providers are used
                    if (is_array($restore_data)
                        && array_key_exists('emailenable', $restore_data)
                        && array_key_exists('emailguser', $restore_data)
                        && array_key_exists('emailgpass', $restore_data)
                        && array_key_exists('emailfromtext', $restore_data)
                        && array_key_exists('emailtoemail', $restore_data)
                    ) {
						$settings_restored[$restore_area_key][$restore_areas_idx]['ATTEMPT'] = true;

						$emailenable = $restore_data['emailenable'];
                        $emailguser = $restore_data['emailguser'];
                        $emailgpass = $restore_data['emailgpass'];
                        $emailfromtext = $restore_data['emailfromtext'];
                        $emailtoemail = $restore_data['emailtoemail'];

                        //Write them out
                        WriteSettingToFile('emailenable', $emailenable);
                        WriteSettingToFile('emailguser', $emailguser);
                        WriteSettingToFile('emailfromtext', $emailfromtext);
                        WriteSettingToFile('emailtoemail', $emailtoemail);

                        //Only save password and generate exim config if upload data is unprotected
                        //meaning the password was included in the backup,
                        //otherwise existing (valid) config may be overwritten
                        if ($uploadDataProtected == false && $emailgpass != "") {
                            WriteSettingToFile('emailgpass', $emailgpass);
                            //Update the email config (writes out exim config)
                            $settings['emailserver'] = $restore_data['emailserver'];
                            $settings['emailport'] = $restore_data['emailport'];
                            $settings['emailuser'] = $restore_data['emailuser'];
                            $settings['emailpass'] = $restore_data['emailpass'];
                            $settings['emailfromuser'] = $restore_data['emailfromuser'];
                            $settings['emailfromtext'] = $restore_data['emailfromtext'];
                            $settings['emailtoemail'] = $restore_data['emailtoemail'];
                            ApplyEmailConfig();
                            $save_result = true;
                        }
                        $save_result = true;
                    }
                }

                //TIMEZONE RESTORATION
				if ($restore_areas_idx == "timezone") {
					//get data out of nested array
					$restore_data = $restore_area_data['timezone'][0];

					//Make sure we have an array, there will be 2 indexes, 0 the timezone and 1 a linebreak
					if (is_array($restore_data) || !is_null($restore_data) ) {
						//Version 2 backups need to locate the TimeZone data slightly differently due to how it was stored back then
						if ($backup_version == 2) {
							$data = $restore_data[0];//first index has the timezone, index 1 is empty to due carriage return in file when its backed up
						} else {
							//Version 3 backups - TimeZone data is located at the first index
							$data = $restore_data;
						}

						if (!empty($data)) {
							$settings_restored[$restore_area_key][$restore_areas_idx]['ATTEMPT'] = true;
							//Timezone isn't stored in a seperate file anymore, it's now a setting
							WriteSettingToFile('TimeZone', $data);
							//Update the timezone on the system
							SetTimezone($data);
							$save_result = true;
						}
					}
				}

				//PROXY CONFIG RESTORATION
				if ($restore_areas_idx == "proxies") {
					$settings_restored[$restore_area_key][$restore_areas_idx]['ATTEMPT'] = true;

					//Get the restore data out of the array
					$restore_data = $restore_area_data['proxies'][0];
					$data = implode("\n", $restore_data);

					//This is a standard file with webserver config, just write it out
					if (file_put_contents( $settings['configDirectory'] . "/proxies", $data) === FALSE) {
						$save_result = false;
					} else {
						$save_result = true;
					}
				}

                $settings_restored[$restore_area_key][$restore_areas_idx]['SUCCESS'] = $save_result;
            }
        }
    }

	//Network Settings (Wired and WiFi)
	if ($restore_area_key == "network"  && !$restore_data_is_empty) {//If the user doesn't want to keep the existing network settings, we can overwrite them
		if ($keepNetworkSettings == false) {

            //check the supplied network data for any extra interfaces over the std eth0 and wlan0
            // all them to the network config area, then process as normal, if the interface key exits in the network config area then
            // it will match upon restoration and the file will get written out
            // this is a cheat to get around not knowing if there is more than the standard interfaces were backed up
            //$network_data = $restore_area_data[$network_type][0];
			foreach ($restore_area_data as $net_type => $net_data) {
				//Check if the net_type in the network config area
				//the new type will be wired_network, wifi_network or like interface.eth1 etc for an extra interface(s)
				//the net_type is also the filename of the interfaces file when it was backed up
				if (array_key_exists($net_type, $system_config_areas['network']['file']) == false) {
					//Set it's location for restore
					$system_config_areas['network']['file'][$net_type] = array('type' => 'file', 'location' => $settings['configDirectory'] . "/" . $net_type);

					//Also add interface to the array used to track which network interfaces were restored and what their settings are
					$network_settings_restored_post_apply[$net_type] = "";
					$network_settings_restored_applied_ips[$net_type] = array();
				}
			}
			//Remove unused vars so they don't linger
            unset($net_type);
            unset($net_data);

			//Overwrite existing network settings with a fresh copy from the area map
			$network_config_filepath = $system_config_areas['network']['file'];

			foreach ($network_config_filepath as $network_type => $network_setting_filepath) {
				//If there is a sub_key (being either wired or wifi, then keep breaking the loop until we're on the one specified in the sub key
				//this will match the users selection. Eg. if they select wired then only the wired config is restored
				//if there is no sub key, eg the user chose Network or ALL, then this doesn't apply
				if (!empty($restore_area_sub_key)) {
					if ($network_type != $restore_area_sub_key) {
						//break the loop
						continue;
					}
				}

				$settings_restored[$restore_area_key][$network_type]['ATTEMPT'] = true;
				//Get the actual location for the network setting
				$network_setting_filepath = $network_setting_filepath['location'];
                //Get the interface config data
				$network_data = $restore_area_data[$network_type][0];
				//Check to make sure we have data, so we don't accidentally wipe out the network
				if (!empty($network_data)) {

					$ini_string = "";
					//The wired and wifi network interfaces which don't have a configuration have empty file data in the backup file
                    //to save any issues, check the first "line" of data, if its empty then there was no configuration for the interface
                    //else it will hold the first line of the interface configuration e.g. "INTERFACE=wlan0",
					if (!empty($network_data[0])) {
						foreach ($network_data as $ini_key => $ini_value) {
//						$ini_string .= "$ini_key='$ini_value'\n";
							$ini_string .= "$ini_value\n";
						}
					}

					//If we can parse out generated INI string, then it's value.
					//Save it to the network file
					if ((parse_ini_string($ini_string) !== FALSE) && !empty($ini_string)) {

						if (file_put_contents($network_setting_filepath, $ini_string) === FALSE) {
							$save_result = false;
						} else {
							$save_result = true;
							$network_settings_restored = true;
                            //Map out the interface wired network and wifi network
                            $parsed_ini_string = parse_ini_string($ini_string);
//							$interface = $parsed_ini_string['INTERFACE'];
//
//							$network_settings_restored_post_apply[$network_type] = ($SUDO . " " . $settings['fppDir'] . "/scripts/config_network $interface");
							$network_settings_restored_applied_ips[$network_type] = $parsed_ini_string;

							//Reboot required for network settings to be applied.
							WriteSettingToFile('rebootFlag', 1);
						}
						$settings_restored[$restore_area_key][$network_type]['SUCCESS'] = $save_result;
					} else {
						error_log("RESTORE: Failed to restore " . $restore_area_key . " - Could not generate network settings - " . $ini_string);
					}
				} else {
					error_log("RESTORE: Failed to restore " . $restore_area_key . " - Network settings empty - Found: " . json_encode($network_data));
				}
			}
		} else {
			error_log("RESTORE: Failed to restore " . $restore_area_key . " - 'Keep Existing Network Settings' selected - NOT OVERWRITING: ");
			//no attempt was made so remove the key for tracking attempts
			unset($settings_restored[$restore_area_key]);
		}

//		$settings_restored[$restore_area_key]['SUCCESS'] = $save_result;
	}

    //PIXELNET/DMX (FPD) RESTORATION
//    if ($restore_area_key == "pixelnet_DMX") {
//        //Just overwrite the universes file
//        $pixlnet_filepath = $system_config_areas['pixelnetDMX']['file'];
//        $data = implode("\n", $restore_area_data);
//        $save_result = file_put_contents($pixlnet_filepath, $data);
//        $settings_restored[$restore_area_key] = $save_result;
//    }
//
//    //DEPRECATED - UNIVERSE RESTORATION
//    if ($restore_area_key == "universes") {
//        //Just overwrite the universes file
//        $universe_filepath = $system_config_areas['universes']['file'];
//        $data = implode("\n", $restore_area_data);
//        $save_result = file_put_contents($universe_filepath, $data);
//
//        $settings_restored[$restore_area_key] = $save_result;
//    }

	//finally if there was no data for the restore area remove it's key for tracking attemps, this is fine here as the seperate restore area tests above will avoid restore areas with no data
	if ($restore_data_is_empty) {
		unset($settings_restored[$restore_area_key]);
	}

    //wrote message out
    if (!$save_result) {
        error_log("RESTORE: Failed to restore " . $restore_area . " - " . json_encode($settings_restored));
    }

    //Return save result
    return true;
}

/**
 * Looks after script restorations and running of associated InstallActions
 * @param $file_names array of script file names
 */
function RestoreScripts($file_names)
{
    global $SUDO, $fppDir;
    global $scriptDirectory;

    if (!empty($file_names)) {
        foreach ($file_names as $filename) {
            exec("$SUDO $fppDir/scripts/restoreScript $filename");
        }
    }
}

/**
 * Sets the Audio Output device (extracted from fppjson.php)
 * @param $card String soundcard
 * @return  string
 */
function SetAudioOutput($card)
{
    global $args, $SUDO, $debug;

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

/**
 * Helper function to read the TimeZone setting for backups (no longer stored in the TimeZone file
 *
 * @return string
 */
function ReadTimeZone(){
    $TimeZone_setting = ReadSettingFromFile('TimeZone');
    return $TimeZone_setting;
}

/**
 * Reads any JSON files in the config folder and returns an array keyed with the filename and it's contents
 * the keys, attempts to ignore any files specified as belonging to other areas
 *
 * @return array
 */
function BackupConfigFolderConfigs()
{
	global $settings, $system_config_areas;

	//Get a list of files in the config directory
	$json_config_files = read_directory_files($settings['configDirectory'], false);

	//Loop over the files we have found and weed out the non .json files
	foreach ($json_config_files as $filename => $fn_bool) {
		//If the filename has the extension of .json, then we want to keep it, discard all others
		//eg no .htaccess, .htpassword or .db files
		if (stripos(strtolower($filename), ".json") === false) {
			unset($json_config_files[$filename]);
		}
		//else true so we keep the filename
	}

	//One we have the list, we need to remove any that other areas, e.g co-universes.json etc
	$system_config_areas_local = $system_config_areas;
	unset($system_config_areas_local['all']);
	unset($system_config_areas_local['network']);
	unset($system_config_areas_local['misc_configs']);//to avoid something weird happening
	//remove email as its in the general settings file and not a separate section
	unset($system_config_areas_local['settings']['file']['email']);

	//loop over the preconfigured areas and process them
    //This loop will remove any files we picked up in the config folder which may already be known about and mapped to a existing area (e.g co-universes.json)
    //any files left after this are extra config files we need to backup
	foreach ($system_config_areas_local as $config_key => $config_data) {
		$setting_file_to_backup = $config_data['file'];

		//if setting file value is an array then there are one or more setting files related to this backup
		if (is_array($setting_file_to_backup)) {
			//loop over the array
			foreach ($setting_file_to_backup as $settings_file => $settings_file_data) {
				//We only want files, so check the type first and only process files further
				if ($settings_file_data['type'] == "file") {
					//Check if the file location(s) is an array, if so there will likely be more than 1 file location to read
					//All other times location should just be a simple string for the file location
					if (is_array($settings_file_data['location'])) {
						//loop over the locations to read them all in
						foreach ($settings_file_data['location'] as $location_filename => $location_path) {
							//Get just the filename from the path
							$path_parts = pathinfo($location_path);
							if (!empty($path_parts['extension'])) {
								$path_parts_filename = $path_parts['filename'] . '.' . $path_parts['extension'];
							} else {
								$path_parts_filename = $path_parts['filename'];
							}
							//Check if the file is in our misc/extra json config files list
							if (array_key_exists($path_parts_filename, $json_config_files)) {
								//remove it from the list of misc json config files as it will get backed up in another area
								unset($json_config_files[$path_parts_filename]);
//								echo "Excluding file from Extra Config backup" . $path_parts_filename;
							}
						}//end loop
					} else {
						//Not an array so just a single file or filepath
						//Do the same as above, check if the file is in our misc/extra json config files list

						//Get just the filename from the path
						$path_parts = pathinfo($settings_file_data['location']);
						if (!empty($path_parts['extension'])) {
							$path_parts_filename = $path_parts['filename'] . '.' . $path_parts['extension'];
						} else {
							$path_parts_filename = $path_parts['filename'];
						}

						if (array_key_exists($path_parts_filename, $json_config_files)) {
							//remove it from the list of misc json config files as it will get backed up in another area
							unset($json_config_files[$path_parts_filename]);
//							echo "Excluding file from Extra Config backup" . $path_parts_filename;
						}
					}//end else
				}
			}
		} else {
			//Location is not an array, so likely just holds the filename,
			//check if the file is in our misc/extra json config files list
			if ($setting_file_to_backup !== false) {
				$path_parts = pathinfo($setting_file_to_backup);

				if (!empty($path_parts['extension'])) {
					$path_parts_filename = $path_parts['filename'] . '.' . $path_parts['extension'];
				} else {
					$path_parts_filename = $path_parts['filename'];
				}

				if (!empty($path_parts['filename'])) {
					if (array_key_exists($path_parts_filename, $json_config_files)) {
						unset($json_config_files[$path_parts_filename]);
//						echo "Excluding file from Extra Config backup" . $path_parts_filename;
					}
				}
			}
		}
	}

	//Once we have our final list of extra config files, read them in
	//file reading them in, we'll try decoding the string which will test if it's actually a json file
	//if the string cannot be successfully decoded, the the contents is likely not a json file
	//unset these because want to reuse them
	unset($filename);
	unset($fn_bool);
	//
	foreach ($json_config_files as $filename => $fn_bool) {
		if (is_string($filename)) {
			//Read in the config file
			$file_data = file_get_contents($settings['configDirectory'] . '/' . $filename);
			//Attempt json_decode on the string, if there is a error then can the data and the filename (json file should have json data)
			$json_result = json_decode($file_data);
			//
			if (json_last_error() === JSON_ERROR_NONE) {
				// JSON is valid
				// Keep the data - decode it so it's kept as an array for encoding
				$json_config_files[$filename] = json_decode($file_data, true);
			} else {
				//Invalid data, don't keep it and remove the filename from the backup
				unset($json_config_files[$filename]);
			}
		}
//       $file_data = parse_ini_string(file_get_contents($setting_file_to_backup));
//       $file_data = json_decode(file_get_contents($setting_file_to_backup), true);
	}

	return $json_config_files;
}

/**
 * Takes the file-named keyed array generated by backup_config_folder_configs and basically loops through
 * restoring data into each of the files
 *
 * @return boolean
 */
function RestoreConfigFolderConfigs($restore_data)
{
	global $settings, $system_config_areas;

	//basically restore the files into the config directory, using the array key as the filename and just write the data into the file
	//pretty print the JSON before doing this
	$save_result = false;
	//make sure data is supplied before doing anything, just in case
	if (!empty($restore_data)) {
		//Loop over the data to restore, the array key is the filename e.g joysticks.json, it's value is a array representing the file contents
		foreach ($restore_data as $restore_filename => $restore_file_data) {
			//Generate the filepath for the config file to be restored
			$misc_data_rest_filepath = $settings['configDirectory'] . '/' . $restore_filename;

			//PrettyPrint the JSON data and save it
			$misc_data_restore = prettyPrintJSON(json_encode($restore_file_data));
			if (file_put_contents($misc_data_rest_filepath, $misc_data_restore) === FALSE) {
				$save_result = false;
			} else {
				$save_result = true;
			}
		}
	}

	return $save_result;
}

/**
 * Reads the Falcon.FPDV1 - Falcon Pixelnet/DMX file
 * extracted & modified from LoadPixelnetDMXFile() in ./fppxml.php
 *
 * @return array|void
 */
function LoadPixelnetDMXFile_FPDv1()
{
    global $settings;
    //Pull in the class
    require_once './pixelnetdmxentry.php';
    //Store data in an array instead of session
    $return_data = array();


    if (@filesize($settings['configDirectory'] . "/Falcon.FPDV1") < 1024) {
        return $return_data;
    }

    $f = fopen($settings['configDirectory'] . "/Falcon.FPDV1", "rb");
    if ($f == FALSE) {
        fclose($f);
        return;
    }

    if ($f != FALSE) {
        $s = fread($f, 1024);
        fclose($f);
        $sarr = unpack("C*", $s);

        $dataOffset = 7;

        $i = 0;
        for ($i = 0; $i < 12; $i++) {
            $outputOffset = $dataOffset + (4 * $i);
            $active = $sarr[$outputOffset + 0];
            $startAddress = $sarr[$outputOffset + 1];
            $startAddress += $sarr[$outputOffset + 2] * 256;
            $type = $sarr[$outputOffset + 3];
            $return_data[$i] = new PixelnetDMXentry($active, $type, $startAddress);
        }
    }

    return $return_data;
}

/**
 * Restores the FPDv1 channel data from the supplied array
 * extracted & modified from LoadPixelnetDMXFile() in ./fppxml.php
 *
 * @param $restore_data array FPDv1 data to restore
 * @return bool Success state file write
 */
function SavePixelnetDMXFile_FPDv1($restore_data)
{
    global $settings;
    $outputCount = 12;
    $write_status = false;
    if (!empty($restore_data) && is_array($restore_data)) {

        $carr = array();
        for ($i = 0; $i < 1024; $i++) {
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

//    $_SESSION['PixelnetDMXentries']=NULL;
        for ($o = 0; $o < $outputCount; $o++) {
            // Active Output
            if (isset($restore_data[$o]['active']) && ($restore_data[$o]['active'] == '1' || intval($restore_data[$o]['active']) == 1)) {
                $active = 1;
                $carr[$i++] = 1;
            } else {
                $active = 0;
                $carr[$i++] = 0;
            }
            // Start Address
            $startAddress = intval($restore_data[$o]['startAddress']);
            $carr[$i++] = $startAddress % 256;
            $carr[$i++] = $startAddress / 256;
            // Type
            $type = intval($restore_data[$o]['type']);
            $carr[$i++] = $type;
//        $_SESSION['PixelnetDMXentries'][] = new PixelnetDMXentry($active,$type,$startAddress);
        }
        $f = fopen($settings['configDirectory'] . "/Falcon.FPDV1", "wb");
//        fwrite($f, implode(array_map("chr", $carr)), 1024);

        if (fwrite($f, implode(array_map("chr", $carr)), 1024) === FALSE) {
            $write_status = false;
        } else {
            $write_status = true;
        }

        fclose($f);
//    SendCommand('w');
    }
    return $write_status;
}

/**
 * Performs all the work to gather files to be backed up for the specified backup area
 * optionally $allowDownload can be set to false to prevent the backup being sent to the users browser and just have the file backed up to the config/backups folder
 *
 * This function can be called directly to perform a backup of settings, $allowDownload should be false in this instance to avoid header issues
 *
 * @param string $area Backup area to be backed up, refer $system_config_areas for the currently defined list of area
 * @param bool $allowDownload Toggle to allow or disallow the backup file being sent to the users browser
 */
function performBackup($area = "all", $allowDownload = true)
{
	global $system_config_areas, $protectSensitiveData, $known_json_config_files, $known_ini_config_files;

	//Toggle the flag to disallow the backup file to be downloaded by the browser
	if ($allowDownload == false){
		preventPromptUserBrowserDownloadBackup();
    }

	if (array_key_exists($area, $system_config_areas)) {
		//Create a copy of the areas array to manipulate
		$tmp_config_areas = $system_config_areas;

		//Generate backup for selected area
		//AREA - ALL
		if (strtolower($area) == "all") {
			//combine all backup areas into a single file

			//remove the 'all' key and do some processing of areas array
			unset($tmp_config_areas['all']);
			//remove email as its in the general settings file and not a separate section
			unset($tmp_config_areas['settings']['file']['email']);

			//loop over the areas and process them
			foreach ($tmp_config_areas as $config_key => $config_data) {
				$setting_file_to_backup = $config_data['file'];
				$file_data = array();
				$backup_file_data = '';
				//if setting file value is an array then there are one or more setting files related to this backup
				if (is_array($setting_file_to_backup)) {
//                    if (array_key_exists('special', $setting_file)) {
//                        $special = $tmp_config_areas[$config_key]['file']['special'];
//                    }

					//loop over the array
					foreach ($setting_file_to_backup as $sfi => $sfd) {
						$file_data = array();
						//           'events' => array('type' => 'dir', 'location' => $eventDirectory),

						//File or directory, read data accordingly
						if ($sfd['type'] == "dir") {
							//read all files in directory and read data
							$file_data = read_directory_files($sfd['location']);
						} else if ($sfd['type'] == "file") {
							//read setting file as normal

							//Check if the file location(s) is an array, if so there will likely be more than 1 file location to read
							//All other times location should just be a simple string for the file location
							if (is_array($sfd['location'])){
								//loop over the locations to read them all in
								foreach ($sfd['location'] as $location_filename => $location_path){
									//Check if the file to be backed up is one of a known type
									if (in_array($sfi, $known_ini_config_files)) {
										//INI
										//parse ini properly
										$backup_file_data = parse_ini_string(@file_get_contents($location_path));
									} else if (in_array($sfi, $known_json_config_files)) {
										//JSON
										//channelOutputsJSON is a formatted (prettyPrint) JSON file, decode it into an assoc. array
										$backup_file_data = json_decode(@file_get_contents($location_path), true);
									} else {
										//all other files are std flat files, process them into an array by splitting at line breaks
										$backup_file_data = explode("\n", @file_get_contents($location_path));
									}

									//Collect the backup data
									$file_data[$location_filename] = array($backup_file_data);
								}//end loop
							}else{
								//Not an array so just a single file or filepath

								//Check if the file to be backed up is one of a known type
								if (in_array($sfi, $known_ini_config_files)) {
									//INI
									//parse ini properly
									$backup_file_data = parse_ini_string(@file_get_contents($sfd['location']));
								} else if (in_array($sfi, $known_json_config_files)) {
									//JSON
									//channelOutputsJSON is a formatted (prettyPrint) JSON file, decode it into an assoc. array
									$backup_file_data = json_decode(@file_get_contents($sfd['location']), true);
								} else {
									//all other files are std flat files, process them into an array by splitting at line breaks
									$backup_file_data = explode("\n", @file_get_contents($sfd['location']));
								}

								//Collect the backup data
								$file_data = array($backup_file_data);
							}//end else

						} else if ($sfd['type'] == "function") {
							//If the type is function, call the function defined under the backup location
							//this function will return the necessary data
							$backup_function = $sfd['location']['backup'];
							if (function_exists($backup_function)) {
								$backup_file_data = $backup_function();
							}
							$file_data = array($backup_file_data);
						}

						//Remove sensitive data
						$tmp_settings_data[$config_key][$sfi] = remove_sensitive_data($file_data);
					}
				} else {
					if ($setting_file_to_backup !== false && file_exists($setting_file_to_backup)) {
						//what type of file are we dealing with? json, ini, csv, flatfile?
						//ultimately the data should be returned as an array
						//it will later be json_encoded

						if (in_array($config_key, $known_ini_config_files)) {
							//INI
							//parse ini properly
							$file_data = parse_ini_string(file_get_contents($setting_file_to_backup));
						} else if (in_array($config_key, $known_json_config_files)) {
							//JSON
							//channelOutputsJSON is a formatted (prettyPrint) JSON file, decode it into an assoc. array
							$file_data = json_decode(file_get_contents($setting_file_to_backup), true);
						} else {
							//all other files are std flat files, process them into an array by splitting at line breaks
							$file_data = explode("\n", file_get_contents($setting_file_to_backup));
						}
						//Remove sensitive data
						$tmp_settings_data[$config_key] = remove_sensitive_data($file_data);
					}
				}
				//End for loop processing each individual "area" in order to get all areas
			}
//            } else if (strtolower($area) == "email") {
//                //AREA - EMAIL BACKUP
//                $emailgpass = '';
//                //Do a quick work around to obtain the emailgpass
//                if ($tmp_config_areas['settings']['file'] !== false && file_exists($tmp_config_areas['settings']['file'])) {
//                    $setting_file_data = parse_ini_string(file_get_contents($tmp_config_areas['settings']['file']));
//                    if (array_key_exists('emailgpass', $setting_file_data)) {
//                        //get pass
//                        $emailgpass = $setting_file_data['emailgpass'];
//                    }
//                }
//
//                //special treatment for email settings as they are inside the general settings file
//                //collect them together here
//                $email_settings = array(
//                    'emailenable' => $settings['emailenable'],
//                    'emailguser' => $settings['emailguser'],
//                    'emailgpass' => $emailgpass,
//                    'emailfromtext' => $settings['emailfromtext'],
//                    'emailtoemail' => $settings['emailtoemail']
//                );
//
//                $tmp_settings_data['email'] = $email_settings;
			//End processing "ALL" backup areas
		} else {
			//AREA - Individual Backup section selections will arrive here
			//All other backup areas for individual selections
			$setting_file_to_backup = $tmp_config_areas[$area]['file'];

			//if setting file value is an array then there are one or more setting files related to this backup
			if (is_array($setting_file_to_backup)) {
//                    if (array_key_exists('special', $tmp_config_areas[$area]['file'])) {
//                        $special = $tmp_config_areas[$area]['file']['special'];
//                    }
				//loop over the array
				foreach ($setting_file_to_backup as $sfi => $sfd) {
					$file_data = array();
					if ($sfd['type'] == "dir") {
						$file_data = read_directory_files($sfd['location']);
					} else if ($sfd['type'] == "file") {

						//Check if the file location(s) is an array, if so there will likely be more than 1 file location to read
						//All other times location should just be a simple string for the file location
						if (is_array($sfd['location'])){
							//loop over the locations to read them all in
							foreach ($sfd['location'] as $location_filename => $location_path){
								//Check if the file to be backed up is one of a known type
								if (in_array($sfi, $known_ini_config_files)) {
									//INI
									//parse ini properly
									$backup_file_data = parse_ini_string(file_get_contents($location_path));
								} else if (in_array($sfi, $known_json_config_files)) {
									//JSON
									//channelOutputsJSON is a formatted (prettyPrint) JSON file, decode it into an assoc. array
									$backup_file_data = json_decode(file_get_contents($location_path), true);
								} else {
									//all other files are std flat files, process them into an array by splitting at line breaks
									$backup_file_data = explode("\n", file_get_contents($location_path));
								}

								//Collect the backup data
								$file_data[$location_filename] = array($backup_file_data);
							}//end loop
						}else{
							//Not an array so just a single file or filepath

							//Check if the file to be backed up is one of a known type
							if (in_array($sfi, $known_ini_config_files)) {
								//INI
								//parse ini properly
								$backup_file_data = parse_ini_string(file_get_contents($sfd['location']));
							} else if (in_array($sfi, $known_json_config_files)) {
								//JSON
								//channelOutputsJSON is a formatted (prettyPrint) JSON file, decode it into an assoc. array
								$backup_file_data = json_decode(file_get_contents($sfd['location']), true);
							} else {
								//all other files are std flat files, process them into an array by splitting at line breaks
								$backup_file_data = explode("\n", file_get_contents($sfd['location']));
							}

							//Collect the backup data
							$file_data = array($backup_file_data);
						}//end else


//                            //Get the data out of the file
//                            if (in_array($sfi, $known_ini_config_files)) {
//                                //INI
//                                //parse ini properly
//                                $backup_file_data = parse_ini_string(file_get_contents($sfd['location']));
//                            } else if (in_array($sfi, $known_json_config_files)) {
//                                //JSON
//                                //channelOutputsJSON is a formatted (prettyPrint) JSON file, decode it into an assoc. array
//                                $backup_file_data = json_decode(file_get_contents($sfd['location']), true);
//                            } else {
//                                //all other files are std flat files, process them into an array by splitting at line breaks
//                                $backup_file_data = explode("\n", file_get_contents($sfd['location']));
//                            }
//                            $file_data = array($backup_file_data);

					} else if ($sfd['type'] == "function") {
						$backup_function = $sfd['location']['backup'];
						$backup_file_data = array();
						if (function_exists($backup_function)) {
							$backup_file_data = $backup_function();
						}
						$file_data = array($backup_file_data);
					}
					//Remove Sensitive data
					$tmp_settings_data[$area][$sfi] = remove_sensitive_data($file_data);
				}
			} else {
				if ($setting_file_to_backup !== false && file_exists($setting_file_to_backup)) {
					if (in_array($area, $known_ini_config_files)) {
						$file_data = parse_ini_string(file_get_contents($setting_file_to_backup));
					} else if (in_array($area, $known_json_config_files)) {
						$file_data = json_decode(file_get_contents($setting_file_to_backup), true);
					} else {
						$file_data = explode("\n", file_get_contents($setting_file_to_backup));
					}
					//Remove sensitive data
					$tmp_settings_data[$area] = remove_sensitive_data($file_data);
				}
			}
			//End individual / specific backup area processing
		}

		//DO IT!
		if (!empty($tmp_settings_data)) {
			doBackupDownload($tmp_settings_data, $area);
		}else{
			$backup_error_string = "SETTINGS BACKUP: Something went wrong while generating backup file for " . ucwords(str_replace("_", " ", $area)) . ", no data was found. Have these settings been configured?";
			$backup_errors[] = $backup_error_string;
		}
	}
}

/**
 * Starts the download in the browser
 * @param $settings_data array Assoc. Array of settings data
 * @param $area String Area the download was for
 */
function doBackupDownload($settings_data, $area)
{
    global $settings, $protectSensitiveData, $fpp_backup_version, $fpp_backup_prompt_download, $fpp_backup_location, $backup_errors;

    if (!empty($settings_data)) {
        //is sensitive data removed (selectively used on restore to skip some processes)
        $settings_data['protected'] = $protectSensitiveData;
        //platform identifier
        $settings_data['platform'] = $settings['Platform'];
        //insert the backup system version
        $settings_data['fpp_backup_version'] = $fpp_backup_version;

        //Once we have all the settings, process the array and dump it back to the user
        //filename
        $backup_fname = $settings['HostName'] . "_" . $area . "-backup_" . "v" . $fpp_backup_version . "_";
        //change filename if sensitive data is not protected
        if ($protectSensitiveData == false) {
            $backup_fname .= "unprotected_";
        }
        $backup_fname .= date("YmdHis") . ".json";

        //check to see fi the backup directory exists
		if (!file_exists($fpp_backup_location)) {
			if (mkdir($fpp_backup_location) == FALSE) {
			    $backup_error_string = "SETTINGS BACKUP: Something went wrong creating the backup file directory '" . $fpp_backup_location . "' , backup file can't be created (permissions error?) and backup download will fail as a result.";
				$backup_errors[] = $backup_error_string;
				error_log($backup_error_string);
			}
		} else {
			//Backup location exists, test if writable
			if (is_writable($fpp_backup_location) == FALSE) {
				$backup_error_string = "SETTINGS BACKUP: Something went wrong, '" . $fpp_backup_location . "'  is not writable.";
				$backup_errors[] = $backup_error_string;
				error_log($backup_error_string);
			}
		}

        //Write a copy locally into "config/backups"
        $backup_local_fpath = $fpp_backup_location . '/' . $backup_fname;
        $json = json_encode($settings_data);

        //Write data into backup file
		if (file_put_contents($backup_local_fpath, $json) !== FALSE) {
		    //Once the backup has been written into our local directory, prune/remove any backups older than the max set age
			pruneOrRemoveAgedBackupFiles();

			//Make the users browser prompt a download and send the file
			if ($fpp_backup_prompt_download == true) {
				///Generate the headers to prompt browser to start download
				header("Content-Disposition: attachment; filename=\"" . basename($backup_fname) . "\"");
				header("Content-Type: application/json");
				header("Content-Length: " . filesize($backup_local_fpath));
				header("Connection: close");
				//Output the file
				readfile($backup_local_fpath);
				//die
				exit;
			}
		} else {
			$backup_error_string = "SETTINGS BACKUP: Something went wrong while writing the backup file to '" . $backup_local_fpath . "', JSON backup file unable to be downloaded.";
			$backup_errors[] = $backup_error_string;
			error_log($backup_error_string);
		}

    } else {
        //no data supplied
		$backup_error_string = "SETTINGS BACKUP: Something went wrong while generating backup file for " .  ucwords(str_replace("_", " ", $area)) . ", no data was supplied. Have these settings been configured?";
		$backup_errors[] = $backup_error_string;
        error_log($backup_error_string);
    }
}

/**
 * Generate backup/restore area select list
 * @param $area_name String Either 'backuparea' or 'restorearea' for either respective section
 * @return string HTML code for a dropdown selection list
 */
function genSelectList($area_name = "backuparea")
{
    global $system_config_areas;

    $select_html = "<select name=\"$area_name\" id=\"$area_name\">";
    foreach ($system_config_areas as $item => $item_options) {
        //print the main key first
        $select_html .= "<option value=" . $item . ">" . $item_options['friendly_name'] . "</option>";
        //If we're restoring and there is a sub array of files, print them out as such they are nested
        if (($area_name == "restorearea" && array_key_exists('file', $item_options)) || ($area_name == "backuparea" && $item == 'plugins')) {
            //make sure file option is an array, meaning this section has sub-areas
            if (is_array($item_options['file'])) {
                foreach ($item_options['file'] as $sub_index => $sub_data) {
                    $disabled = '';
                    if ($area_name == "backuparea") {
                        $disabled = "disabled='disabled'";
                    }
                    $select_html .= "<option value=" . $item . "/" . $sub_index . " " . $disabled . " >--" . ucwords(str_replace("_", " ", $sub_index)) . "</option>";
                }
            }
        }
    }
    $select_html .= "</select>";

    return $select_html;
}

/**
 * Returns filenames of any interface configs in addition ot eth0 and wlan0, maybe the user has a USB ethernet adapter etc.
 * The files and their contents are later read in as part of the backup process
 *
 * @return array Array of extra network interfaces
 */
function retrieveNetworkInterfaces()
{
	global $system_config_areas, $settings;

	//Get a list of files in the config directory
	$network_interfaces = read_directory_files($settings['configDirectory'], false);

	//Loop over the files we have found and weed out the non .json files
	foreach ($network_interfaces as $filename => $fn_bool) {
		//If the filename contains 'interface.', then we want to keep it, discard anything else others
		if (stripos(strtolower($filename), "interface") === false) {
			unset($network_interfaces[$filename]);
		}
		//else true so we keep the file since it's a interface
	}

	//Remove eth0 and wlan0, since the backup system will already include them by default
	//anything remaining is a extra interface
	if (array_key_exists('interface.eth0', $network_interfaces)) {
		unset($network_interfaces['interface.eth0']);
	}
	if (array_key_exists('interface.wlan0', $network_interfaces)) {
		unset($network_interfaces['interface.wlan0']);
	}

	//Loop over the array, put the filenames into the network area so they can be backed up
	foreach ($network_interfaces as $intername_name => $interface_bool) {
		$system_config_areas['network']['file'][$intername_name] = array('type' => 'file', 'location' => $settings['configDirectory'] . '/' . $intername_name);

		//Add into the array that tracks what interface & settings are restored
		$network_settings_restored_post_apply[$intername_name] = "";
		$network_settings_restored_applied_ips[$intername_name] = array();
	}

	return $network_interfaces;
}

/**
 * Returns a list of plugin Config files
 *
 * @return array Array of plugins and respective config file data
 */
function retrievePluginList()
{
    global $settings;

    $config_files = read_directory_files($settings['configDirectory'], false);
    $plugin_names = array();

	//find the plugin configs which are prepended with "plugin.", ignore normal JSON files
	foreach ($config_files as $fname => $fdata) {
		//Make sure we pickup plugin config files, plugin config files are prepended with the word "plugin".
		if ((stripos(strtolower($fname), "plugin.") !== false)) {
		    //Fine out the MINE type of the file we're backing up
			$finfo_open_handle = finfo_open(FILEINFO_MIME_TYPE);
			$fileInfo = finfo_file($finfo_open_handle, $settings['configDirectory'] . "/" . $fname);

			//If it's a plain text file then we can back it up, things like databases are skipped
			if (stripos(strtolower($fileInfo), "text") !== FALSE) {
				//split the string to get just the plugin name
				$plugin_name = explode(".", $fname);
				$plugin_name = $plugin_name[1];

				//Store config file locations here
                $locations = array();
                //Normal Plugin config file location
				$locations[$fname] = ($settings['configDirectory'] . "/" . $fname);
//				//Check the extra backup locations where the plugin name matches this plugin and extra the extra path
//				if (isset($extra_backup_locations['plugins'][$plugin_name]) && !empty($extra_backup_locations['plugins'][$plugin_name]))
//                {
//                    //Loop over the extra locations and add them to the list in case there are more than 1 extra location
//                    foreach ($extra_backup_locations['plugins'][$plugin_name] as $p_extra_filename => $p_extra_file_location){
//						$locations[$p_extra_filename] = $p_extra_file_location;
//					}
//                }

				//Add the expected & extra locations into the final array to be returned
				$plugin_names[$plugin_name] = array('type' => 'file', 'location' => $locations);

//				$plugin_names[$plugin_name] = array('type' => 'file', 'location' => $settings['configDirectory'] . "/" . $fname);

				//array('name' => $plugin_name, 'config' => $fname);
			}
		}
	}
	//Lookup the extra backup locations and merge those files in


    return $plugin_names;
}

/**
 * Moves any backup files in the config directory made by FPP backup to a folder called backups
 */
function moveBackupFiles_ToBackupDirectory()
{
    global $settings, $fpp_backup_location;

    //gets all the files in the config directory
    $config_dir_files = read_directory_files($settings['configDirectory'], false);

    //loop over the backup files we've found
    foreach ($config_dir_files as $backup_filename => $backup_data) {
        if ((stripos(strtolower($backup_filename), "-backup_") !== false) && (stripos(strtolower($backup_filename), ".json") != false)) {
            //move file to the new fpp_backup_locations
            rename($settings['configDirectory'] . '/' . $backup_filename, $fpp_backup_location . '/' . $backup_filename);
        }
    }
}

/**
 * Change the location where the Settings backup file is stored, a copy is always stored locally
 * This just changes where that file is stored in case it needs to be temporarily somewhere else.
 * @param $new_location
 */
function changeLocalFPPBackupFileLocation($new_location)
{
	global $fpp_backup_location;
	if (isset($new_location)) {
		$fpp_backup_location = $new_location;
	}
}

/**
 * Find and removes any JSON settings backup files that are over our specified max age
 */
function pruneOrRemoveAgedBackupFiles(){
	global $fpp_backup_max_age, $fpp_backup_min_number_kept, $fpp_backup_location;

	//gets all the files in the configs/backup directory
	$config_dir_files = read_directory_files($fpp_backup_location, false, true);

	//If the number of backup files that exist IS LESS than what the minimum we want to keep, return and stop procesing
	if (count($config_dir_files) < $fpp_backup_min_number_kept) {
		$aged_backup_removal_message = "SETTINGS BACKUP: Not removing JSON Settings backup files older than $fpp_backup_max_age days. Since there are less than the minimum backups we want to keep ($fpp_backup_min_number_kept)";
		error_log($aged_backup_removal_message);
		return;
	}

	//loop over the backup files we've found
	foreach ($config_dir_files as $backup_filename => $backup_data) {
		if ((stripos(strtolower($backup_filename), "-backup_") !== false) && (stripos(strtolower($backup_filename), ".json") !== false)) {
			//File is one of our backup files, this check is just in case there is other json files in this directory placed there by the user which we want to avoid touching.

			//reconstruct the path
			$backup_file_location = $fpp_backup_location . "/" . $backup_filename;
			//backup max_age time in seconds, since we dealing with UNIX epoc time
			$backup_max_age_seconds = ($fpp_backup_max_age * 86400);

			//Check the age of the file by checking the file modification time compared to now
			if ((time() - filemtime($backup_file_location)) > ($backup_max_age_seconds)) {
				//Not what happened in the logs also
				$aged_backup_removal_message = "SETTINGS BACKUP: Removed old JSON settings backup file ($backup_file_location) since it was " . ceil((time() - filemtime($backup_file_location)) / 86400) . " days old. Max age is $fpp_backup_max_age days.";
				error_log($aged_backup_removal_message);
                //Remove the file
				unlink($backup_file_location);
			}
		}
	}
}

/**
 * Prevent or stop the users browser from showing a download prompt for the backup file
 * Useful for if we just want to trigger a backup and just have it saved locally instead of both saving locally and sending it to the users browser
 */
function preventPromptUserBrowserDownloadBackup(){
    global $fpp_backup_prompt_download;
    //
    $fpp_backup_prompt_download = false;
}

/**
 * Set a new defined max backup age for the existing JSON settings backup files
 * @param $new_max_age
 */
function setBackupMaxAge($new_max_age)
{
	global $fpp_backup_max_age;
	//
    if (!empty($new_max_age) && ($new_max_age != 0)){
		$fpp_backup_max_age = $new_max_age;
	}
}

/**
 * Set a new minimum number of backup files to keep
 * @param $new_backup_count
 */
function setMinimumBackupCount($new_backup_count)
{
	global $fpp_backup_min_number_kept;
	//
	if (!empty($new_backup_count) && ($new_backup_count != 0)){
		$fpp_backup_min_number_kept = $new_backup_count;
	}
}

function is_array_empty($InputVariable)
{
	$Result = true;

	if (is_array($InputVariable) && count($InputVariable) > 0)
	{
		foreach ($InputVariable as $Value)
		{
			$Result = $Result && is_array_empty($Value);
		}
	}
	else
	{
		$Result = empty($InputVariable);
	}

	return $Result;
}

//Move backup files
moveBackupFiles_ToBackupDirectory();

?>
<?php
//See if we should skip spitting out the HTML code below
if ($skipHTMLCodeOutput == false) {
	?>
<!DOCTYPE html>
<html lang="en">
<head>
    <?php require_once 'common/menuHead.inc'; ?>
    <title>FPP - <?php echo gethostname(); ?></title>
    <!--    <script>var helpPage = "help/backup.php";</script>-->
    <meta http-equiv="Content-Type" content="text/html; charset=utf-8" />

	<?php
	$backupHosts = getKnownFPPSystems();
	?>
    <script type="text/javascript">
        var settings = new Array();
		<?php
        ////Override restartFlag setting not reflecting actual value after restoring, just read what's in the settings file
        $settings['restartFlag'] = ReadSettingFromFile('restartFlag');
		$settings['rebootFlag'] = ReadSettingFromFile('rebootFlag');

        foreach ($settings as $key => $value) {
            if (!is_array($value)) {
                if (preg_match('/\n/', $value))
                    continue;

                printf("	settings['%s'] = \"%s\";\n", $key, $value);
            } else {
                $js_array = json_encode($value);
                printf("    settings['%s'] = %s;\n", $key, $js_array);
            }
        }

        ?>
        var pageName = "<?php echo str_ireplace('.php', '', basename($_SERVER['PHP_SELF'])) ?>";

        var helpPage = "<?php echo basename($_SERVER['PHP_SELF']) ?>";
        if (pageName == "plugin") {
            var pluginPage = "<?php echo preg_replace('/.*page=/', '', $_SERVER['REQUEST_URI']); ?>";
            var pluginBase = "<?php echo preg_replace("/^\//", "", preg_replace('/page=.*/', '', $_SERVER['REQUEST_URI'])); ?>";
            helpPage = pluginBase + "nopage=1&page=help/" + pluginPage;
        }
        else {
            helpPage = "help/" + helpPage;
        }

function GetCopyFlags() {
    var flags = "";

    if (document.getElementById("backup.Configuration").checked) {
        flags += " Configuration";
    }
    if (document.getElementById("backup.Playlists").checked) {
        flags += " Playlists";
    }
    if (document.getElementById("backup.Events").checked) {
        flags += " Events";
    }
    if (document.getElementById("backup.Plugins").checked) {
        flags += " Plugins";
    }
    if (document.getElementById("backup.Sequences").checked) {
        flags += " Sequences";
    }
    if (document.getElementById("backup.Effects").checked) {
        flags += " Effects";
    }
    if (document.getElementById("backup.Images").checked) {
        flags += " Images";
    }
    if (document.getElementById("backup.Scripts").checked) {
        flags += " Scripts";
    }
    if (document.getElementById("backup.Music").checked) {
        flags += " Music";
    }
    if (document.getElementById("backup.Videos").checked) {
        flags += " Videos";
    }
    if ((document.getElementById("backup.Backups").checked) &&
        (direction = document.getElementById("backup.Direction").value == 'TOUSB')) {
        flags += " Backups";
    }

    if (flags.length)
        flags = flags.substring(1);

    return flags;
}

function PerformCopy() {
    var dev = document.getElementById("backup.USBDevice").value;
    var path = document.getElementById("backup.Path").value;
    var pathSelect = document.getElementById("backup.PathSelect").value;
    var host = document.getElementById("backup.Host").value;
    var direction = document.getElementById("backup.Direction").value;
    var flags = GetCopyFlags();

    var url = "copystorage.php?direction=" + direction;

    if ((direction == 'TOUSB') ||
        (direction == 'FROMUSB')) {
        storageLocation = document.getElementById("backup.USBDevice").value;
    } else if ((direction == 'TOREMOTE') ||
               (direction == 'FROMREMOTE')) {
        if (host == '') {
            DialogError('Copy Failed', 'No host specified');
            return;
        }

        storageLocation = host;
    } else {
        storageLocation = "/home/fpp/media/backups";
    }

    if (direction.substring(0,4) == 'FROM') {
        if (pathSelect == '') {
            DialogError('Copy Failed', 'No path specified');
            return;
        }

        url += '&path=' + pathSelect;
    } else {
        if (path == '') {
            DialogError('Copy Failed', 'No path specified');
            return;
        }

        url += '&path=' + path;
    }

    url += '&storageLocation=' + storageLocation;
    url += '&flags=' + flags;

    var warningMsg = "Confirm File restore of '" + flags + "' from " + storageLocation + "?\n\nWARNING: This will overwrite any current files with the copies being restored";

    if (document.getElementById("backup.DeleteExtra").checked) {
        url += '&delete=yes';
        warningMsg += " and delete any local files which do not exist in the backup.";
    } else {
        url += '&delete=no';
    }

    if (direction.substring(0,4) == 'FROM')
    {
        if (!confirm(warningMsg)) {
            $.jGrowl("Restore canceled.",{themeState:'success'});
            return;
        }
    }


    window.location.href = url;
}

function GetBackupDevices() {
    $('#backup\\.USBDevice').html('<option>Loading...</option>');
    $.get("api/backups/devices"
        ).done(function(data) {
            var options = "";
            for (var i = 0; i < data.length; i++) {
                var desc = data[i].name;
                if (data[i].vendor != '')
                    desc += ' - ' + data[i].vendor;

                if (data[i].model != '') {
                    if (data[i].vendor != '')
                        desc += ' ';
                    else
                        desc += ' - ';

                    desc += data[i].model;
                }

                desc += ' - ' + data[i].size + 'GB';
                options += "<option value='" + data[i].name + "'>" + desc + "</option>";
            }
            $('#backup\\.USBDevice').html(options);

            if ((options != "") &&
                (document.getElementById("backup.Direction").value == 'FROMUSB'))
                GetBackupDeviceDirectories();
        }).fail(function() {
            $('#backup\\.USBDevice').html('');
        });
}

function GetBackupDeviceDirectories() {
    var dev = document.getElementById("backup.USBDevice").value;

    if (dev == '') {
        $('#backup\\.PathSelect').html("<option value=''>No USB Device Selected</option>");
        return;
    }

    $('#backup\\.PathSelect').html('<option>Loading...</option>');
    $.get("api/backups/list/" + dev
        ).done(function(data) {
            PopulateBackupDirs(data);
        }).fail(function() {
            $('#backup\\.PathSelect').html('');
        });
}

function USBDeviceChanged() {
    var direction = document.getElementById("backup.Direction").value;
    if (direction == 'FROMUSB')
        GetBackupDeviceDirectories();
}

function PopulateBackupDirs(data) {
    var options = "";
    for (var i = 0; i < data.length; i++) {
        if (data[i].substring(0,5) != 'ERROR')
            options += "<option value='" + data[i] + "'>" + data[i] + "</option>";
        else
            options += "<option value=''>" + data[i] + "</option>";
    }
    $('#backup\\.PathSelect').html(options);
}

function GetBackupDirsViaAPI(host) {
    $('#backup\\.PathSelect').html('<option>Loading...</option>');
    $.get("http://" + host + "/api/backups/list"
        ).done(function(data) {
            PopulateBackupDirs(data);
        }).fail(function() {
            $('#backup\\.PathSelect').html('');
        });
}

function GetBackupHostBackupDirs() {
    GetBackupDirsViaAPI($('#backup\\.Host').val());
}

function BackupDirectionChanged() {
    var direction = document.getElementById("backup.Direction").value;

    switch (direction) {
        case 'TOUSB':
            $('.copyUSB').show();
            $('.copyPath').show();
            $('.copyPathSelect').hide();
            $('.copyHost').hide();
            $('.copyBackups').show();
            break;
        case 'FROMUSB':
            $('.copyUSB').show();
            $('.copyPath').hide();
            $('.copyPathSelect').show();
            $('.copyHost').hide();
            $('.copyBackups').hide();
            GetBackupDeviceDirectories();
            break;
        case 'TOLOCAL':
            $('.copyUSB').hide();
            $('.copyPath').show();
            $('.copyPathSelect').hide();
            $('.copyHost').hide();
            $('.copyBackups').hide();
            break;
        case 'FROMLOCAL':
            $('.copyUSB').hide();
            $('.copyPath').hide();
            $('.copyPathSelect').show();
            $('.copyHost').hide();
            $('.copyBackups').hide();
            GetBackupDirsViaAPI('<?php echo $_SERVER['SERVER_ADDR'] ?>');
            break;
        case 'TOREMOTE':
            $('.copyUSB').hide();
            $('.copyPath').show();
            $('.copyPathSelect').hide();
            $('.copyHost').show();
            $('.copyBackups').hide();
            break;
        case 'FROMREMOTE':
            $('.copyUSB').hide();
            $('.copyPath').hide();
            $('.copyPathSelect').show();
            $('.copyHost').show();
            $('.copyBackups').hide();
            GetBackupHostBackupDirs();
            break;
    }
}

GetBackupDevices();

        var activeTabNumber =
			<?php
			if (isset($_GET['tab']) and is_numeric($_GET['tab']))
				print $_GET['tab'];
			else
				print "0";
			?>;
    </script>

    <style>
        .copyHost {
            display: none;
        }
        .copyPathSelect {
            display: none;
        }
    </style>
</head>
<body>
<div id="bodyWrapper">
    <?php
    $activeParentMenuItem = 'status';
    include 'menu.inc'; ?>
    <div class="mainContainer">
        <h1 class='title'>FPP Backups</h1>
        <div class="pageContent">
            <div class="fppTabs">
                <div id="fppBackups">
                    <ul id="fppBackupTabs" class="nav nav-pills pageContent-tabs" role="tablist">

                        <li class="nav-item">
                            <a class="nav-link active" id="backups-jsonBackup-tab" data-toggle="tab" href="#tab-jsonBackup" role="tab" aria-controls="tab-jsonBackup" aria-selected="true">
                            JSON Configuration Backup
                            </a>
                        </li>
                        <li class="nav-item">
                            <a class="nav-link" id="backups-fileCopy-tab" data-toggle="tab" href="#tab-fileCopy" role="tab" aria-controls="tab-fileCopy" aria-selected="false">
                            File Copy Backup
                            </a>
                        </li>
                    </ul>

                    <div id="fppBackupsTabsContent" class="tab-content">
                    <div id="tab-jsonBackup" class="tab-pane fade show active" role="tabpanel" aria-labelledby="backups-jsonBackup-tab">
                        <form action="backup.php" method="post" name="frmBackup" enctype="multipart/form-data">
                            <?php
                            //Spit out the backup errors if the backup_errors array isn't empty
        					if (!is_array_empty($backup_errors)) {
        						?>
                                <div id="rebootFlag" style="display: block; margin-right: auto; margin-left: auto; width: 60%;">Backup failed: <br>
                                    <ul>
        								<?php
        								foreach ($backup_errors as $backup_error) {
        									echo "<li>$backup_error</li>";
        								}
        								?>
                                    </ul>
                                </div>
        						<?php
        					}
                            ?>
                        <?php if ($restore_done == true) {
                            ?>
                            <div id="rebootFlag" style="display: block;">Backup Restored, FPPD Restart or Reboot may be required.
                            </div>
                            <div id="restoreSuccessFlag">What was restored: <br>
                                <?php
        						foreach ($settings_restored as $area_restored => $success) {
                                    $success_str = "";
        							if (is_array($success)) {
        								$success_area_data = false;
        								$success_messages = "";

        								//If the ATTEMPT and SUCCESS keys don't exist in the array, then try to process the internals which will be a sub areas and possibly have them.
        								if (!array_key_exists('ATTEMPT', $success) && !array_key_exists('SUCCESS', $success) && !empty($success)) {
        									//process internal array for areas with sub areas
        									foreach ($success as $success_area_idx => $success_area_data) {
        										if (array_key_exists('ATTEMPT', $success_area_data) && array_key_exists('SUCCESS', $success_area_data)) {
        											$success_area_attempt = $success_area_data['ATTEMPT'];
        											$success_area_success = $success_area_data['SUCCESS'];

        											if ($success_area_attempt == true && $success_area_success == true) {
        												$success_str = "Success";
        											} else {
        												$success_str = "Failed";
        											}

        											$success_messages .= ucwords(str_replace("_", " ", $success_area_idx)) . " - " . $success_str . "<br/>";
        										}
        									}
        								} //There is an ATTEMPT and SUCCESS key, check values of both
        								else if (array_key_exists('ATTEMPT', $success) && array_key_exists('SUCCESS', $success)) {
        									$success_area_attempt = $success['ATTEMPT'];
        									$success_area_success = $success['SUCCESS'];

        									if ($success_area_attempt == true && $success_area_success == true) {
        										$success_str = "Success";
        									} else {
        										$success_str = "Failed";
        									}

        									$success_messages .= ucwords(str_replace("_", " ", $area_restored)) . " - " . $success_str . "<br/>";
        								} // No Attempt key, then we shouldn't print the success
        								else if (!array_key_exists('ATTEMPT', $success) && array_key_exists('SUCCESS', $success)) {
        									//Ignore
        								}
                                        //Print out the restore successes
        								echo $success_messages;
        							} else {
                                        //normal area
                                        if ($success == true) {
                                            $success_str = "Success";
                                        } else {
                                            $success_str = "Failed";
                                        }
                                        echo ucwords(str_replace("_", " ", $area_restored)) . " - " . $success_str . "<br/>";
                                    }
                                }
        						//If network settings have been restored, print out the IP addresses that should come info effect
        						if ($network_settings_restored) {
        							//Print the IP addresses out
        							foreach ($network_settings_restored_applied_ips as $idx => $network_ip_address) {
        								if (!empty($network_ip_address)) {
        									echo ucwords(str_replace("_", " ", $idx)) . " - Network Settings" . "<br/>";
        									//If there is a SSID, print it also
        									if (array_key_exists('SSID', $network_ip_address)) {
        										echo "SSID: " . ($network_ip_address['SSID']) . "<br/>";
        									}
        									//Print out details for static addresses
        									echo "Type: " . ($network_ip_address['PROTO']) . "<br/>";
        									if (strtolower($network_ip_address['PROTO']) == 'static') {
        										echo "IP: " . "<a href='http://" . $network_ip_address['ADDRESS'] . "'>" . $network_ip_address['ADDRESS'] . "</a>" . "<br/>";
        										echo "Netmask: " . ($network_ip_address['NETMASK']) . "<br/>";
        										echo "GW: " . ($network_ip_address['GATEWAY']) . "<br/>";
        									}
        									echo "<br/>";
        								}
        							}

        							echo "REBOOT REQUIRED: Please VERIFY the above settings, if they seem incorrect please adjust them in <a href='./networkconfig.php'>Network Settings</a> BEFORE rebooting.";
        						}
                                ?>
                            </div>
                            <?php
                        }
                        ?>
                        <div class="backdrop">
                            <h2>Backup Configuration</h2>
                            <div class="container-fluid">
                                <div class="row">
                                    <div class="col-md-5">
                                        <span>Protect sensitive data?</span>
                                    </div>
                                    <div class="col-md-7">
                                        <input id="dataProtect" name="protectSensitive"
                                                   type="checkbox"
                                                   checked="true">
                                    </div>
                                </div>
                                <div class="row">
                                    <div class="col-md-5"><span>Backup Area</span></div>
                                    <div class="col-md-7"><?php echo genSelectList('backuparea'); ?></div>
                                </div>
                                <div class="row">
                                    <div class="col-md-5"></div>
                                    <div class="col-md-7"><button name="btnDownloadConfig" type="Submit" class="buttons"
                                               value="Download Configuration"><i class="fas fa-fw fa-nbsp fa-download"></i>Download Configuration</button>
                                    </div>
                                </div>
                            </div>
                        </div>
                        <br/>
                        <div class="backdrop">
                            <h2>Restore Configuration</h2>
                            <div class="callout callout-danger">
                                <b class="text-danger">JSON Backups made from FPP v1.x are not compatible with FPP 3.x and higher.</b>
                            </div>
                            <div class="container-fluid">
                                <div class="row">
                                    <div class="col-md-5">Keep Existing Network Settings</div>
                                    <div class="col-md-7">
                                        <input name="keepExitingNetwork"
                                               type="checkbox"
                                               checked="true">
                                    </div>
                                </div>
                                <div class="row">
                                    <div class="col-md-5">Keep Existing Master/Remote Settings</div>
                                    <div class="col-md-7">
                                        <input name="keepMasterSlave"
                                               type="checkbox"
                                               checked="true">
                                    </div>
                                </div>
                                <div class="row">
                                    <div class="col-md-5">Restore Area</div>
                                    <div class="col-md-7">
                                        <?php echo genSelectList('restorearea'); ?></div>
                                </div>
                                <div class="row">
                                    <div class="col-md-5"></div>
                                    <div class="col-md-7">
                                        <i class="fas fa-fw fa-nbsp fa-upload"></i>
                                        <input id="btnUploadConfig" name="conffile" type="file" accept=".json" id="conffile" autocomplete="off">
                                        <script>
                                            $('#btnUploadConfig').change(function(e){
                                                if (e.target.files[0].name.length > 4) {
                                                    $('#btnRestoreConfig').show();
                                                }
                                            });
                                        </script>
                                    </div>
                                </div>
                                <div class='row'>
                                    <div class="col-md-5"></div>
                                    <div class="col-md-7">
                                        <button id="btnRestoreConfig" name="btnRestoreConfig" type="Submit" class="buttons hidden">
                                            <i class="fas fa-fw fa-nbsp fa-file-import"></i>Restore Configuration</button>
                                    </div>
                                </div>
                            </div>
                    </div>
                        </form>
                    </div>

                    <div id="tab-fileCopy" class="tab-pane fade" role="tabpanel" aria-labelledby="backups-fileCopy-tab">
                        <div class="backdrop"><h2>File Copy Backup/Restore</h2>
                                Copy configuration, sequences, etc... to/from a backup device.
                                <table>
        <tr><td>Copy Type:</td><td><select id="backup.Direction" onChange='BackupDirectionChanged();'>
        <option value="TOUSB" selected>Backup To USB</option>
        <option value="FROMUSB">Restore From USB</option>
        <option value="TOLOCAL">Backup To Local FPP Backups Directory</option>
        <option value="FROMLOCAL">Restore From Local FPP Backups Directory</option>
        <option value="TOREMOTE">Backup To Remote FPP Backups Directory</option>
        <option value="FROMREMOTE">Restore From Remote FPP Backups Directory</option>
        </select></td></tr>
        <tr class='copyUSB'><td>USB Device:</td><td><select name='backup.USBDevice' id='backup.USBDevice' onChange='USBDeviceChanged();'></select> <input type='button' class='buttons' onClick='GetBackupDevices();' value='Refresh List'></td></tr>
        <tr class='copyHost'><td>Remote Host:</td><td><?php PrintSettingSelect('Backup Host', 'backup.Host', 0, 0, '', $backupHosts, '', 'GetBackupHostBackupDirs'); ?></td></tr>
        <tr class='copyPath'><td>Backup Path:</td><td><?php PrintSettingTextSaved('backup.Path', 0, 0, 128, 64, '', gethostname()); ?></td></tr>
        <tr class='copyPathSelect'><td>Backup Path:</td><td><select name='backup.PathSelect' id='backup.PathSelect'></select></td></tr>
        <tr><td>What to copy:</td><td>
        <table id="CopyFlagsTable">
        <tr><td>
				<?php PrintSettingCheckbox('Backup Configuration', 'backup.Configuration', 0, 0, 1, 0, "", "", 1, 'Configuration'); ?><br>
				<?php PrintSettingCheckbox('Backup Playlists', 'backup.Playlists', 0, 0, 1, 0, "", "", 1, 'Playlists'); ?><br>
            </td><td width='10px'></td><td>
				<?php PrintSettingCheckbox('Backup Events', 'backup.Events', 0, 0, 1, 0, "", "", 1, 'Events'); ?><br>
				<?php PrintSettingCheckbox('Backup Plugins', 'backup.Plugins', 0, 0, 1, 0, "", "", 1, 'Plugins'); ?><br>
            </td><td width='10px'></td><td>
				<?php PrintSettingCheckbox('Backup Sequences', 'backup.Sequences', 0, 0, 1, 0, "", "", 1, 'Sequences'); ?><span style="color: #AA0000">*</span><br>
				<?php PrintSettingCheckbox('Backup Images', 'backup.Images', 0, 0, 1, 0, "", "", 1, 'Images'); ?><br>
            </td><td width='10px'></td><td>
				<?php PrintSettingCheckbox('Backup Scripts', 'backup.Scripts', 0, 0, 1, 0, "", "", 1, 'Scripts'); ?><br>
				<?php PrintSettingCheckbox('Backup Effects', 'backup.Effects', 0, 0, 1, 0, "", "", 1, 'Effects'); ?><br>
            </td><td width='10px'></td><td>
				<?php PrintSettingCheckbox('Backup Music', 'backup.Music', 0, 0, 1, 0, "", "", 1, 'Music'); ?><br>
				<?php PrintSettingCheckbox('Backup Videos', 'backup.Videos', 0, 0, 1, 0, "", "", 1, 'Videos'); ?><br>
            </td><td width='10px'></td><td valign='top' class='copyBackups'>
            <input type='checkbox' id='backup.Backups'>Backups <span style="color: #AA0000">*</span><br>
        </td></tr></table>
        </td></tr>
        <tr><td>Delete extras:</td><td><input type='checkbox' id='backup.DeleteExtra'> (Delete extra files on destination that do not exist on the source)</td></tr>
                                <tr><td></td><td>
                                        <input type='button' class="buttons" value="Copy" onClick="PerformCopy();">
                                </table>

                                <div class="callout callout-danger">
                                    <h4>Notes:</h4>
                                    <ul>
                                        <li>Sequence backups may not work correctly when restored on other FPP systems if the sequences are FSEQ v2 files and the Channel Output configurations of the two systems do not match.</li>
                                        <li class='copyBackups'>*Backing up Backups will copy all local backups to the USB device.</li>
                                    </ul>

                                </div>
                          </div>
                        </div>

                    </div>
                    </div>
                </div>
            <div id="dialog" title="Warning!" style="display:none">
                <p>Un-checking this box will disable protection (automatic removal) of sensitive data like passwords.
                    <br/> <br/>
                    <b>ONLY</b> Un-check this if you want to be able make an exact clone of settings to another FPP.
                    <br/> <br/>
                    <b>NOTE:</b> The backup will include passwords in plaintext, you assume full responsibility for this
                    file.
                </p>
            </div>
        </div>
    </div>
    <script>
        $('#dataProtect').click(function () {
            var checked = $(this).is(':checked');
            if (!checked) {
                $("#dialog").fppDialog({
                    width: 580,
                    height: 280
                });
            }
        });

        // $("#tabs").tabs({cache: true, active: activeTabNumber, spinner: "", fx: { opacity: 'toggle', height: 'toggle' } });

    </script>
    <?php include 'common/footer.inc'; ?>
</div>
</body>
</html>
	<?php
}
?>