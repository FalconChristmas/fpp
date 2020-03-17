<?php
$skipJSsettings = 1;
//error_reporting(E_ALL);
//ini_set('display_errors', 1);

//Include other scripts
require_once('common.php');
require_once('common/settings.php');
//Includes for API access
//require_once('fppjson.php');
require_once('fppxml.php');

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
    'model-overlays' => array('friendly_name' => 'Pixel Overlay Models', 'file' => $settings['model-overlays']),
    'outputProcessors' => array('friendly_name' => 'Output Processors', 'file' => $settings['outputProcessorsFile']),
    'show_setup' =>
        array(
            'friendly_name' => 'Show Setup (Events, Playlists, Schedule, Scripts)',
            'file' => array(
                'events' => array('type' => 'dir', 'location' => $eventDirectory),
                'playlist' => array('type' => 'dir', 'location' => $playlistDirectory),
                'schedule' => array('type' => 'file', 'location' => $scheduleFile),
                'scripts' => array('type' => 'dir', 'location' => $scriptDirectory),
            ),
            'special' => true
        ),
    'settings' =>
        array(
            'friendly_name' => 'System Settings (incl. GPIO Input, Email, Timezone)',
            'file' => array(
                'system_settings' => array('type' => 'file', 'location' => $settingsFile),
                'proxies' => array('type' => 'file', 'location' => "$mediaDirectory/config/proxies"),
                'email' => array('type' => 'file', 'location' => false),
                'timezone' => array('type' => 'file', 'location' => $timezoneFile)
            ),
            'special' => true
        ),
    'plugins' => array(
        'friendly_name' => 'Plugin Settings',
        'file' => array(),
        'special' => true
    ),
    'network' => array(
		'friendly_name' => 'Network Settings (Wired & WiFi)',
		'file' => array(
			'wired_network' => array('type' => 'file', 'location' =>  $settings['configDirectory']. "/interface.eth0"),
			'wifi_network' => array('type' => 'file', 'location' =>  $settings['configDirectory']. "/interface.wlan0"),
		),
		'special' => true
	)
);

//FPP Backup version
$fpp_backup_version = "2";
//FPP Backup files directory
$fpp_backup_location = $settings['configDirectory'] . "/backups";

//Array of plugins
$system_active_plugins = array();

//Populate plugins
$system_config_areas['plugins']['file'] = retrievePluginList();

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

//Lookup arrays for whats json and a ini file
$known_json_config_files = array('channelInputs', 'channelOutputs', 'outputProcessors', 'universes', 'pixel_strings', 'bbb_strings', 'led_panels', 'other', 'model-overlays');
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

        //temporary collection area for settings
        $tmp_settings_data = array();

        $file_data = null;
        $special = false;

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

                                $file_data = array($backup_file_data);
                            } else if ($sfd['type'] == "function") {
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
                            //Get the data out of the file
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
                            $file_data = array($backup_file_data);
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
            }

            //DO IT!
            if (!empty($tmp_settings_data)) {
                doBackupDownload($tmp_settings_data, $area);
            }
        }
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
                    $is_version_2_backup = false;
                    if (array_key_exists('fpp_backup_version', $file_contents_decoded) && ($file_contents_decoded['fpp_backup_version'] == $fpp_backup_version)) {
                        $is_version_2_backup = true;
                    }

                    //Remove the platform key as it's not used for anything yet
                    unset($file_contents_decoded['platform']);
                    unset($file_contents_decoded['fpp_backup_version']);

                    //Restore all areas
                    if (strtolower($restore_area) == "all" && $is_version_2_backup) {
                        // ALL SETTING RESTORE
                        //read each area and process it
                        foreach ($file_contents_decoded as $restore_area_key => $area_data) {
                            //Pass the restore area and data to the restore function
                            $restore_done = process_restore_data($restore_area_key, $area_data);
                        }

//                    } else if (strtolower($restore_area) == "email" && $is_version_2_backup) {
//                        //get email settings, from the actual system settings
//                        $area_data = $file_contents_decoded['settings']['system_settings'];
//                        //Pass the restore area and data to the restore function
//                        $restore_done = process_restore_data("email", $area_data);
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
                            $restore_done = process_restore_data($restore_area, $area_data);
                        }
                    }

                    //All processed
//                    $restore_done = true;
                } else {
                    error_log("The backup " . $rstfname . " data could not be decoded properly. Is it a valid backup file?");
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
 * @return array Array of file names and respective data
 */
function read_directory_files($directory, $return_data = true)
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
    return $file_list;
}

/**
 * Function to look after backup restorations
 * @param $restore_area String  Area to restore
 * @param $restore_area_data array  Area data as an array
 * @return boolean Save result
 */
function process_restore_data($restore_area, $restore_area_data)
{
    global $SUDO, $settings, $mediaDirectory, $system_config_areas, $keepMasterSlaveSettings, $keepNetworkSettings, $uploadDataProtected, $settings_restored,
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

//    $is_empty = is_array_empty($restore_area_data);

    //////////////////////////////////
    //OutputProcessors - OutputProcessors
    if ($restore_area_key == "outputProcessors") {
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
    if ($restore_area_key == "channelmemorymaps") {
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
	if ($restore_area_key == "channelInputs") {
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

    //SHOW SETUP & CHANNEL OUTPUT RESTORATION
    if ($restore_area_key == "show_setup" || $restore_area_key == "channelOutputs") {
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
    if ($restore_area_key == "plugins") {
        if (is_array($restore_area_data)) {
            //Just overwrite the universes file
            $plugin_settings_path_base = $settings['configDirectory'];

            //loop over the data and get the plugin name and then write the settings out
            foreach ($restore_area_data as $plugin_name => $plugin_data) {
				$settings_restored[$restore_area_key][$plugin_name]['ATTEMPT']  = true;

				$plugin_settings_path = $plugin_settings_path_base . "/plugin." . $plugin_name;
                $data = implode("\n", $plugin_data[0]);

                if (file_put_contents($plugin_settings_path, $data) === FALSE) {
                    $save_result = false;
                } else {
                    $save_result = true;
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
                                $_GET['volume'] = trim($setting_value);
                                SetVolume();
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
                            SaveEmailConfig($emailguser, $emailgpass, $emailfromtext, $emailtoemail);
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
                    if (is_array($restore_data)) {
                        $data = $restore_data[0];//first index has the timezone, index 1 is empty to due carriage return in file when its backed up
                        if (!empty($data)) {
							$settings_restored[$restore_area_key][$restore_areas_idx]['ATTEMPT'] = true;

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
					if (file_put_contents("$mediaDirectory/config/proxies", $data) === FALSE) {
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
	if ($restore_area_key == "network") {//If the user doesn't want to keep the existing network settings, we can overwrite them
		if ($keepNetworkSettings == false) {
			//Overwrite existing network settings
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

				$network_data = $restore_area_data[$network_type][0];
				//Check to make sure we have data, so we don't accidentally wipe out the network
				if (!empty($network_data)) {

					$ini_string = "";
					foreach ($network_data as $ini_key => $ini_value) {
//						$ini_string .= "$ini_key='$ini_value'\n";
						$ini_string .= "$ini_value\n";
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

    //wrote message out
    if (!$save_result) {
        error_log("RESTORE: Failed to restore " . $restore_area . " - " . json_encode($settings_restored));
    }

    //Return save result
    return $save_result;
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
    
    
    if (filesize($settings['configDirectory'] . "/Falcon.FPDV1") < 1024) {
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
 * Starts the download in the browser
 * @param $settings_data array Assoc. Array of settings data
 * @param $area String Area the download was for
 */
function doBackupDownload($settings_data, $area)
{
    global $settings, $protectSensitiveData, $fpp_backup_version, $fpp_backup_location;

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
            mkdir($fpp_backup_location);
        }


        //Write a copy locally as well
        $backup_local_fpath = $fpp_backup_location . '/' . $backup_fname;
        $json = json_encode($settings_data);
        
        //Write data into backup file
        file_put_contents($backup_local_fpath, $json);

        ///Generate the headers to prompt browser to start download
        header("Content-Disposition: attachment; filename=\"" . $backup_fname . "\"");
        header("Content-Type: application/json");
        header("Content-Length: " . filesize($backup_local_fpath));
        header("Connection: close");
        //Output the file
        readfile($backup_local_fpath);
        //die
        exit;
    } else {
        //no data supplied
        error_log("BACKUP: Something went wrong while generating backup file for " . $area . ", no data was supplied.");
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
 * Returns a list of plugin Config files
 *
 * @return array Array of plugins and respective config file data
 */
function retrievePluginList()
{
    global $settings;

    $config_files = read_directory_files($settings['configDirectory'], false);
    $plugin_names = array();

	//find the plugin configs, ignore JSON files
	foreach ($config_files as $fname => $fdata) {
		//Make sure we pickup plugin config files, plugin config files are prepended with plugin.
		if ((stripos(strtolower($fname), "plugin.") !== false)) {
		    //Fine out the MINE type of the file we're backing up
			$finfo_open_handle = finfo_open(FILEINFO_MIME_TYPE);
			$fileInfo = finfo_file($finfo_open_handle, $settings['configDirectory'] . "/" . $fname);

			//If it's a plain text file then we can back it up, things like databases are skipped
			if (stripos(strtolower($fileInfo), "text") !== FALSE) {
				//split the string to get just the plugin name
				$plugin_name = explode(".", $fname);
				$plugin_name = $plugin_name[1];
				$plugin_names[$plugin_name] = array('type' => 'file', 'location' => $settings['configDirectory'] . "/" . $fname);
				//array('name' => $plugin_name, 'config' => $fname);
			}
		}
	}

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
<!DOCTYPE html>
<html>
<head>
    <?php require_once 'common/menuHead.inc'; ?>
    <title>FPP - <? echo gethostname(); ?></title>
    <!--    <script>var helpPage = "help/backup.php";</script>-->
<script>
</script>

<?
$backupHosts = Array();
$data = file_get_contents('http://localhost/api/remotes');
$arr = json_decode($data, true);

foreach ($arr as $host => $desc) {
    $backupHosts[$desc] = $host;
}
ksort($backupHosts);

?>
    <script type="text/javascript">
        var settings = new Array();
        <?
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
        var pageName = "<? echo str_ireplace('.php', '', basename($_SERVER['PHP_SELF'])) ?>";

        var helpPage = "<? echo basename($_SERVER['PHP_SELF']) ?>";
        if (pageName == "plugin") {
            var pluginPage = "<? echo preg_replace('/.*page=/', '', $_SERVER['REQUEST_URI']); ?>";
            var pluginBase = "<? echo preg_replace("/^\//", "", preg_replace('/page=.*/', '', $_SERVER['REQUEST_URI'])); ?>";
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
            $.jGrowl("Restore canceled.");
            return;
        }
    }


    window.location.href = url;
}

function GetBackupDevices() {
    $('#backup\\.USBDevice').html('<option>Loading...</option>');
    $.get("/api/backups/devices"
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
    $.get("/api/backups/list/" + dev
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
    </script>
</head>
<body>
<style>
.copyHost {
    display: none;
}
.copyPathSelect {
    display: none;
}
</style>
<div id="bodyWrapper">
    <?php include 'menu.inc'; ?>
    <br/>
        <div id="global" class="settings">
            <div class='title'>FPP Backups</div>
            <div id='tabs'>
                <ul>
                    <li><a href='#tab-jsonBackup'>JSON Configuration Backup</a></li>
                    <li><a href='#tab-fileCopy'>File Copy Backup</a></li>
                </ul>
            <div id='tab-jsonBackup'>
                <form action="backup.php" method="post" name="frmBackup" enctype="multipart/form-data">
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
                <fieldset>
                    <legend>Backup Configuration</legend>
                    Select settings to backup, then download the selected system configuration in JSON format.
                    <br/>
                    <table width="100%">
                        <tr>
                            <td width="35%"><b>Protect sensitive data?</b></td>
                            <td width="65%">
                                <input id="dataProtect" name="protectSensitive"
                                       type="checkbox"
                                       checked="true">
                            </td>
                        </tr>
                        <tr>
                            <td width="25%">Backup Area</td>
                            <td width="75%"><?php echo genSelectList('backuparea'); ?></td>
                        </tr>
                        <tr>
                            <td width="25%"></td>
                            <td width="75%">
                                <input name="btnDownloadConfig" type="Submit" style="width:30%" class="buttons"
                                       value="Download Configuration">
                            </td>
                        </tr>
                    </table>
                </fieldset>
                <br/>
                <fieldset>
                    <legend>Restore Configuration</legend>
                    <center>
                        <span style="color: #AA0000"><b>JSON Backups made from FPP v1.x are incompatible with the FPP 3.x system.</b><br><br></span>
                    </center>
                    Select settings to restore and then choose backup file containing backup data.
                    <br/>
                    <table width="100%">
                        <tr>
                            <td width="35%"><b>Keep Existing Network Settings</b></td>
                            <td width="65%">
                                <input name="keepExitingNetwork"
                                       type="checkbox"
                                       checked="true">
                            </td>
                        </tr>
                        <tr>
                            <td width="35%"><b>Keep Existing Master/Slave Settings</b></td>
                            <td width="65%">
                                <input name="keepMasterSlave"
                                       type="checkbox"
                                       checked="true">
                            </td>
                        </tr>
                        <tr>
                            <td width="25%">Restore Area</td>
                            <td width="75%">
                                <?php echo genSelectList('restorearea'); ?></td>
                        </tr>
                        <tr>
                            <td width="25%"></td>
                            <td width="75%">
                                <input name="conffile" type="file" accept=".json" class="formbtn" id="conffile"
                                       size="50"
                                       autocomplete="off"></td>
                        </tr>
                        <tr>
                            <td width="25%"></td>
                            <td width="75%">
                                <input name="btnRestoreConfig" type="Submit" style="width:30%" class="buttons"
                                       value="Restore Configuration">
                            </td>
                        </tr>
                    </table>
                </fieldset>
                </form>
                </div>
                <div id='tab-fileCopy'>
                <fieldset><legend>File Copy Backup/Restore</legend>
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
<tr class='copyHost'><td>Remote Host:</td><td><? PrintSettingSelect('Backup Host', 'backup.Host', 0, 0, '', $backupHosts, '', 'GetBackupHostBackupDirs'); ?></td></tr>
<tr class='copyPath'><td>Backup Path:</td><td><? PrintSettingTextSaved('backup.Path', 0, 0, 128, 64, '', gethostname()); ?></td></tr>
<tr class='copyPathSelect'><td>Backup Path:</td><td><select name='backup.PathSelect' id='backup.PathSelect'></select></td></tr>
<tr><td>What to copy:</td><td>
<table id="CopyFlagsTable">
<tr><td>
    <? PrintSettingCheckbox('Backup Configuration', 'backup.Configuration', 0, 0, 1, 0, "", "", 1, 'Configuration'); ?><br>
    <? PrintSettingCheckbox('Backup Playlists', 'backup.Playlists', 0, 0, 1, 0, "", "", 1, 'Playlists'); ?><br>
    </td><td width='10px'></td><td>
    <? PrintSettingCheckbox('Backup Events', 'backup.Events', 0, 0, 1, 0, "", "", 1, 'Events'); ?><br>
    <? PrintSettingCheckbox('Backup Plugins', 'backup.Plugins', 0, 0, 1, 0, "", "", 1, 'Plugins'); ?><br>
    </td><td width='10px'></td><td>
    <? PrintSettingCheckbox('Backup Sequences', 'backup.Sequences', 0, 0, 1, 0, "", "", 1, 'Sequences'); ?><span style="color: #AA0000">*</span><br>
    <? PrintSettingCheckbox('Backup Images', 'backup.Images', 0, 0, 1, 0, "", "", 1, 'Images'); ?><br>
    </td><td width='10px'></td><td>
    <? PrintSettingCheckbox('Backup Scripts', 'backup.Scripts', 0, 0, 1, 0, "", "", 1, 'Scripts'); ?><br>
    <? PrintSettingCheckbox('Backup Effects', 'backup.Effects', 0, 0, 1, 0, "", "", 1, 'Effects'); ?><br>
    </td><td width='10px'></td><td>
    <? PrintSettingCheckbox('Backup Music', 'backup.Music', 0, 0, 1, 0, "", "", 1, 'Music'); ?><br>
    <? PrintSettingCheckbox('Backup Videos', 'backup.Videos', 0, 0, 1, 0, "", "", 1, 'Videos'); ?><br>
    </td><td width='10px'></td><td valign='top' class='copyBackups'>
    <input type='checkbox' id='backup.Backups'>Backups <span style="color: #AA0000">*</span><br>
</td></tr></table>
</td></tr>
<tr><td>Delete extras:</td><td><input type='checkbox' id='backup.DeleteExtra'> (Delete extra files on destination that do not exist on the source)</td></tr>
                        <tr><td></td><td>
                                <input type='button' class="buttons" value="Copy" onClick="PerformCopy();"></input>
                        </table>
                        <br>
                        <span style="color: #AA0000"><b>* Sequence backups may not work correctly when restored on other FPP systems if the sequences are FSEQ v2 files and the Channel Output configurations of the two systems do not match.</b></span><br>
                        <span style="color: #AA0000" class='copyBackups'><b>* Backing up Backups will copy all local backups to the USB device.</b></span><br>
                </fieldset>
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
    <script>
        $('#dataProtect').click(function () {
            var checked = $(this).is(':checked');
            if (!checked) {
                $("#dialog").dialog({
                    width: 580,
                    height: 280
                });
            }
        });

            var activeTabNumber =
<?php
    if (isset($_GET['tab']))
        print $_GET['tab'];
    else
        print "0";
?>;

        $("#tabs").tabs({cache: true, active: activeTabNumber, spinner: "", fx: { opacity: 'toggle', height: 'toggle' } });

    </script>
    <?php include 'common/footer.inc'; ?>
</div>
</body>
</html>
