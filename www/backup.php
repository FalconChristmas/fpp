<?php
$skipJSsettings = 1;
//error_reporting(E_ALL);
//ini_set('display_errors', 1);

//Include other scripts
require_once('common.php');
//Includes for API access
//require_once('fppjson.php');
require_once('fppxml.php');

//Define a map of backup/restore areas and setting locations, this is also used to populate the area select lists
$system_config_areas = array(
    'all' => array('friendly_name' => 'All', 'file' => false),
    'channelInputs' => array('friendly_name' => 'Channel Inputs', 'file' => $settings['universeInputs']),
    'channelOutputs' =>
        array(
            'friendly_name' => 'Channel Outputs (Universe, Falcon, LED Panels, etc.)',
            'file' => array(
                'universes' => array('type' => 'file', 'location' => $settings['universeOutputs']),
                'pixelnet_DMX' => array('type' => 'file', 'location' => $pixelnetFile),
                'pixel_strings' => array('type' => 'file', 'location' => $settings['co-pixelStrings']),
                'bbb_strings' => array('type' => 'file', 'location' => $settings['co-bbbStrings']),
                'led_panels' => array('type' => 'file', 'location' => $settings['channelOutputsJSON']),
                'other' => array('type' => 'file', 'location' => $settings['co-other']),
            ),
            'special' => true
        ),
    'channelmemorymaps' => array('friendly_name' => 'Pixel Overlay Models (Channel Memory Maps)', 'file' => $settings['channelMemoryMapsFile']),
    'channelRemaps' => array('friendly_name' => 'Remap Channels (Channel Remaps)', 'file' => $settings['remapFile']),
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
//    'wlan' => array('friendly_name' => 'WLAN', 'file' => $settings['configDirectory']. "/interface.wlan0")
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

//Array of settings by name/key name, that are considered sensitive/taboo
$sensitive_data = array('emailgpass', 'psk', 'password', 'secret');

//Lookup arrays for whats json and a ini file
$known_json_config_files = array('channelInputs', 'channelOutputs', 'channelRemaps', 'universes', 'pixel_strings', 'bbb_strings', 'led_panels', 'other');
$known_ini_config_files = array('settings', 'system_settings');

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
            if ($file != "." && $file != "..") {
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
 * @param $restore_area String Area to restore
 * @param $restore_area_data array Area data as an array
 * @return boolean Save result
 */
function process_restore_data($restore_area, $restore_area_data)
{
    global $settings, $system_config_areas, $keepMasterSlaveSettings, $keepNetworkSettings, $uploadDataProtected, $settings_restored,
           $known_ini_config_files, $known_json_config_files;
    global $args;

    $restore_area_key = $restore_area_sub_key = "";
    $save_result = false;

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

    //////////////////////////////////
    //CHANNEL REMAPS - REMAP CHANNELS
    if ($restore_area_key == "channelRemaps") {
        $channel_remaps_json_filepath = $system_config_areas['channelRemaps']['file'];
        //PrettyPrint the JSON data and save it
        $json_pp_data = prettyPrintJSON(json_encode($restore_area_data));

        if (file_put_contents($channel_remaps_json_filepath, $json_pp_data) === FALSE) {
            $save_result = false;
        } else {
            $save_result = true;
        }
        $settings_restored[$restore_area_key] = $save_result;

        //Set FPPD restart flag
        WriteSettingToFile('restartFlag', 1);
    }

    //CHANNEL MEMORY MAPS - PIXEL OVERLAYS
    if ($restore_area_key == "channelmemorymaps") {
        //Overwrite channel outputs JSON
        $channelmemorymaps_filepath = $system_config_areas['channelmemorymaps']['file'];
        $data = implode("\n", $restore_area_data);

        if (file_put_contents($channelmemorymaps_filepath, $data) === FALSE) {
            $save_result = false;
        } else {
            $save_result = true;
        }
        $settings_restored[$restore_area_key] = $save_result;

        //Set FPPD restart flag
        WriteSettingToFile('restartFlag', 1);
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
                                if (in_array($restore_area_data_index, $known_json_config_files)) {
                                    //JSON
                                    $final_file_restore_data = $fn_data;
                                } else {
                                    //everything else
                                    //line separate the lines
                                    $final_file_restore_data = implode("\n", $fn_data);
                                }

                                //if restore sub-area is scripts, capture the file names so we can pass those along through RestoreScripts which will perform any InstallActions
                                if (strtolower($restore_areas_idx) == "scripts") {
                                    $script_filenames[] = $fn_to_restore;
                                }

                                //If backup/restore type of a sub-area ia  folder, then build the full path to where the file will be restored
                                if ($restore_type == "dir") {
                                    $restore_location .= "/" . $fn_to_restore;
                                }

                                //If we have data then write to where it needs to go
                                if (!empty($final_file_restore_data)) {
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
                                } else {
                                    $save_result = false;
                                }
                            }
                        }
                        $settings_restored[$restore_area_key][$restore_area_data_index] = $save_result;
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
                $plugin_settings_path = $plugin_settings_path_base . "/plugin." . $plugin_name;
                $data = implode("\n", $plugin_data[0]);

                if (file_put_contents($plugin_settings_path, $data) === FALSE) {
                    $save_result = false;
                } else {
                    $save_result = true;
                }

                $settings_restored[$restore_area_key][$plugin_name] = $save_result;
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

            //If $restore_area_sub_key is empty then no sub-area has been chosen -- restore as normal and restore all sub areas
            //Or if $restore_area_sub_key is equal to the $restore_areas_idx we're on, then restore just this area, because it matches user selection
            //and break the loop
            if ($restore_area_sub_key == "" || ($restore_area_sub_key == $restore_areas_idx)) {
                //SYSTEM SETTING RESTORATION
                if ($restore_areas_idx == "system_settings") {
                    if (is_array($restore_area_data)) {
                        //get data out of nested array
                        $restore_area_data = $restore_area_data['system_settings'][0];

                        foreach ($restore_area_data as $setting_name => $setting_value) {
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
                            if ($setting_name == 'piRTC') {
                                SetPiRTC($setting_value);
                            } else if ($setting_name == "PI_LCD_Enabled") {
                                //DO a weird work around and set our request params and then call
                                //the function to enable the LCD
                                if ($setting_value == 1) {
                                    $_GET['enabled'] = "true";
                                } else {
                                    $_GET['enabled'] = "false";
                                }
                                SetPiLCDenabled();
                            } else if ($setting_name == "AudioOutput") {
                                $args['value'] = $setting_value;
                                SetAudioOutput($setting_value);
                            } else if ($setting_name == "volume") {
                                $_GET['volume'] = trim($setting_value);
                                SetVolume();
                            } else if ($setting_name == "ntpServer") {
                                SetNtpServer($setting_value);
                                NtpServiceRestart();//Restart NTP client so changes take effect
                            } else if ($setting_name == "NTP") {
                                SetNtpState($setting_value);
                            }
                        }

                        //Restart FFPD so any changes can take effect
//                        RestartFPPD();
                        //Write setting out so the user can restart FPPD, this is a work around for Pi Pixel Strings causing a reboot when it's enabled (FPPD is restated, the Pi is rebooted)
                        WriteSettingToFile('restartFlag', 1);

                        $save_result = true;
                    } else {
                        error_log("RESTORE: Cannot read Settings INI settings. Attempted to parse " . json_encode($restore_area_data));
                    }
                }

                //EMAIL SETTING RESTORATION
                if ($restore_areas_idx == "email") {
                    //get data out of nested array
                    $restore_area_data = $restore_area_data['system_settings'][0];

                    //TODO rework this so it will work future email system implementation, were different providers are used
                    if (is_array($restore_area_data)) {
                        $emailenable = $restore_area_data['emailenable'];
                        $emailguser = $restore_area_data['emailguser'];
                        $emailgpass = $restore_area_data['emailgpass'];
                        $emailfromtext = $restore_area_data['emailfromtext'];
                        $emailtoemail = $restore_area_data['emailtoemail'];

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
                    $restore_area_data = $restore_area_data['timezone'][0];
                    $data = $restore_area_data[0];//first index has the timezone, index 1 is empty to due carriage return in file when its backed up
                    SetTimezone($data);

                    $save_result = true;
                }

                $settings_restored[$restore_area_key][$restore_areas_idx] = $save_result;
            }
        }
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
 * Sets the timezone (taken from timeconfig.php)
 * @param $timezone_setting String Timezone in correct format
 * @see https://en.wikipedia.org/wiki/List_of_tz_database_time_zones For a list of timezones
 */
function SetTimezone($timezone_setting)
{
    global $SUDO, $mediaDirectory;
    //TODO: Check timezone for validity
    $timezone = $timezone_setting;
    error_log("RESTORE: Changing timezone to '" . $timezone . "'.");
    if (file_exists('/usr/bin/timedatectl')) {
        exec($SUDO . " timedatectl set-timezone $timezone", $output, $return_val);
        unset($output);
    } else {
        exec($SUDO . " bash -c \"echo $timezone > /etc/timezone\"", $output, $return_val);
        unset($output);
        //TODO: check return
        exec($SUDO . " dpkg-reconfigure -f noninteractive tzdata", $output, $return_val);
        unset($output);
        //TODO: check return
    }
    exec(" bash -c \"echo $timezone > $mediaDirectory/timezone\"", $output, $return_val);
    unset($output);
    //TODO: check return
}

/**
 * Sets the PiRTC setting exec's the appropriate command (taken from timeconfig.php)
 * @param $pi_rtc_setting String PIRTC setting
 */
function SetPiRTC($pi_rtc_setting)
{
    global $SUDO, $fppDir;

    $piRTC = $pi_rtc_setting;
    WriteSettingToFile("piRTC", $piRTC);
    exec($SUDO . " $fppDir/scripts/piRTC set");
    error_log("RESTORE: Set RTC: " . $piRTC);
}

/**
 * Sets the Audio Output device (extracted from fppjson.php)
 * @param $card String soundcard
 * @return  string
 */
function SetAudioOutput($card)
{
    global $args, $SUDO, $debug;

    if ($card != 0 && file_exists("/proc/asound/card$card")) {
        exec($SUDO . " sed -i 's/card [0-9]/card " . $card . "/' /root/.asoundrc", $output, $return_val);
        unset($output);
        if ($return_val) {
            error_log("Failed to set audio to card $card!");
            return;
        }
        if ($debug)
            error_log("Setting to audio output $card");
    } else if ($card == 0) {
        exec($SUDO . " sed -i 's/card [0-9]/card " . $card . "/' /root/.asoundrc", $output, $return_val);
        unset($output);
        if ($return_val) {
            error_log("Failed to set audio back to default!");
            return;
        }
        if ($debug)
            error_log("Setting default audio");
    }

    return $card;
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
        $backup_fname = $settings['HostName'] . "_" . $area . "-backup_";
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
        //Write data into backup file
        file_put_contents($backup_local_fpath, json_encode($settings_data));

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
                        $disabled = "disabled";
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
 */
function retrievePluginList()
{
    global $settings;

    $config_files = read_directory_files($settings['configDirectory'], false);
    $plugin_names = array();

    //find the plugin configs
    foreach ($config_files as $fname => $fdata) {
        if ((stripos(strtolower($fname), "plugin") !== false) && (stripos(strtolower($fname), ".json") == false)) {
            //split the string to get jsut the plugin name
            $plugin_name = explode(".", $fname);
            $plugin_name = $plugin_name[1];
            $plugin_names[$plugin_name] = array('type' => 'file', 'location' => $settings['configDirectory'] . "/" . $fname);
            //array('name' => $plugin_name, 'config' => $fname);
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

//Move backup files
moveBackupFiles_ToBackupDirectory();
?>
<html>
<head>
    <?php require_once 'common/menuHead.inc'; ?>
    <title><? echo $pageTitle; ?></title>
    <script>var helpPage = "help/backup.php";</script>
    <script type="text/javascript">
        var settings = new Array();
        <?
        foreach ($settings as $key => $value) {
            printf("	settings['%s'] = \"%s\";\n", $key, $value);
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
    </script>
</head>
<body>
<div id="bodyWrapper">
    <?php include 'menu.inc'; ?>
    <br/>
    <form action="backup.php" method="post" name="frmBackup" enctype="multipart/form-data">
        <div id="global" class="settings">
            <fieldset>
                <legend>FPP Settings Backup</legend>
                <ul>
                    <li>
                        <span style="color: #AA0000"><b>Backups made from FPP v1.x are incompatible with the FPP 2.x system.</b></span>
                    </li>
                </ul>
                <?php if ($restore_done == true) {
                    ?>
                    <div id="rebootFlag" style="display: block;">Backup Restored, FPPD Restart or Reboot my be required.
                    </div>
                    <div id="restoreSuccessFlag">What was restored: <br>
                        <?php
                        foreach ($settings_restored as $area_restored => $success) {
                            $success_str = "";
                            if (is_array($success)) {
                                //process internal array for areas with sub areas
                                foreach ($success as $success_area_idx => $success_area_data) {
                                    if ($success_area_data == true) {
                                        $success_str = "Success";
                                    } else {
                                        $success_str = "Failed";
                                    }

                                    echo ucwords(str_replace("_", " ", $success_area_idx)) . " - " . $success_str . "<br/>";
                                }
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
                    Select settings to restore and then choose backup file containing backup data.
                    <br/>
                    <table width="100%">
                        <!--                        <tr>-->
                        <!--                            <td width="35%"><b>Keep Existing Network Settings</b></td>-->
                        <!--                            <td width="65%">-->
                        <!--                                <input name="keepExitingNetwork"-->
                        <!--                                       type="checkbox"-->
                        <!--                                       checked="true">-->
                        <!--                            </td>-->
                        <!--                        <tr/>-->
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
                                       value="Restore  Configuration">
                            </td>
                        </tr>
                    </table>
                </fieldset>
            </fieldset>
        </div>
    </form>
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
    </script>
    <?php include 'common/footer.inc'; ?>
</div>
</body>
</html>
