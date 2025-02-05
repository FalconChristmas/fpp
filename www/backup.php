<?php
$skipHTMLCodeOutput = false;
//Check if this script is being called directly for is being included on another script
//If included we'll skip printing the HTML code for the page as it's likely we just want to access functions to generate a backup
function checkDirectScriptExecution()
{
    global $skipJSsettings, $skipHTMLCodeOutput;
    $incl_files = get_included_files();

    //Try to detect if the API scripts are have been included, meaning that this being included in a function of one of the API script
    $api_index_script = 'www/api/index.php';
    $api_index_script_found = false;
    $api_library_script = 'www/api/lib/limonade.php';
    $api_library_script_found = false;

    //Search over the array and try find either
    foreach ($incl_files as $incl_idx => $included_file) {
        if (strpos($included_file, $api_index_script) !== false) {
            $api_index_script_found = true;
        } else if (strpos($included_file, $api_library_script) !== false) {
            $api_library_script_found = true;
        }
    }

    //script in the first index should be the script were executing
    if (($incl_files[0] != __FILE__) || ($api_index_script_found === true || $api_library_script_found === true)) {
        $skipJSsettings = 1;
        $skipHTMLCodeOutput = true;
    }
}
checkDirectScriptExecution();

//Stop config.php spitting out any JS used in the UI, not needed on the backup page, if left as is the JSON download will have the <script> tag in it due to
//that data already being in the buffer
//so change the flag so it doesn't get output when the request is a post request (which is the user submitting the backup form)
if ($_SERVER['REQUEST_METHOD'] === 'POST' || $skipHTMLCodeOutput === true) {
    $skipJSsettings = 1;
}
//error_reporting(E_ALL);
//ini_set('display_errors', 1);

//Include other scripts
require_once 'common.php';
require_once 'common/settings.php';
require_once 'commandsocket.php';

//Define a map of backup/restore areas and setting locations, this is also used to populate the area select lists
$system_config_areas = array(
    'all' => array('friendly_name' => 'All', 'file' => false),
    'channelInputs' => array(
            'friendly_name' => 'Channel Inputs (E1.31 Bridge)',
            'file' => array(
                'universe_inputs' => array('type' => 'file', 'location' => $settings['universeInputs']),
                'dmx_inputs' => array('type' => 'file', 'location' => $settings['dmxInputs']),
            ),
		'special' => true,
    ),
    'channelOutputs' => array(
        'friendly_name' => 'Channel Outputs (Universe, Falcon, LED Panels, etc.)',
        'file' => array(
            'universes' => array('type' => 'file', 'location' => $settings['universeOutputs']),
            'falcon_pixelnet_DMX' => array('type' => 'function', 'location' => array('backup' => 'LoadPixelnetDMXFiles', 'restore' => 'SavePixelnetDMXFiles')),
            'pixel_strings' => array('type' => 'file', 'location' => $settings['co-pixelStrings']),
            'bbb_strings' => array('type' => 'file', 'location' => $settings['co-bbbStrings']),
            'pwm' => array('type' => 'file', 'location' => $settings['co-pwm']),
            'led_panels' => array('type' => 'file', 'location' => $settings['channelOutputsJSON']),
            'other' => array('type' => 'file', 'location' => $settings['co-other']),
        ),
        'special' => true,
    ),
    'channelmemorymaps' => array('friendly_name' => 'Pixel Overlay Models Old(Channel Memory Maps)', 'file' => $settings['mediaDirectory'] . "/channelmemorymaps"),
    'gpio-input' => array('friendly_name' => 'GPIO Input Triggers ', 'file' => $settings['configDirectory'] . "/gpio.json"),
    'model-overlays' => array('friendly_name' => 'Pixel Overlay Models', 'file' => $settings['model-overlays']),
    'misc_configs' => array(
        'friendly_name' => 'Miscellaneous Settings (Additional Plugins Configs, System Settings, etc.)',
        'file' => array(
            'configs' => array('type' => 'function', 'location' => array('backup' => 'BackupConfigFolderConfigs', 'restore' => 'RestoreConfigFolderConfigs')),
        ),
        'special' => true,
    ),
    'outputProcessors' => array('friendly_name' => 'Output Processors', 'file' => $settings['outputProcessorsFile']),
    'show_setup' => array(
        'friendly_name' => 'Show Setup (Playlists, Schedule, Scripts)',
        'file' => array(
            'playlist' => array('type' => 'dir', 'location' => $playlistDirectory),
            'schedule' => array('type' => 'file', 'location' => $settings['scheduleJsonFile']),
            'scripts' => array('type' => 'dir', 'location' => $scriptDirectory),
        ),
        'special' => true,
    ),
    'settings' => array(
        'friendly_name' => 'System Settings (incl. Command Presets, Email, Proxies, Timezone)',
        'file' => array(
            'system_settings' => array('type' => 'file', 'location' => $settingsFile),
            'commandPresets' => array('type' => 'file', 'location' => $settings['configDirectory'] . "/commandPresets.json"),
            'proxies' => array('type' => 'file', 'location' => $settings['configDirectory'] . "/proxies"),
            'email' => array('type' => 'file', 'location' => false),
            'timezone' => array('type' => 'function', 'location' => array('backup' => 'ReadTimeZone', 'restore' => '')), //We'll handle restore ourselves
        ),
        'special' => true,
    ),
    'plugins' => array(
        'friendly_name' => 'Plugin Settings',
        'file' => array(),
        'special' => true,
    ),
    'network' => array(
        'friendly_name' => 'Network Settings (Wired & WiFi, DNS)',
        'file' => array(
            //Std. Network Interfaces
            'wired_network' => array('type' => 'file', 'location' => $settings['configDirectory'] . "/interface.eth0"),
            'wifi_network' => array('type' => 'file', 'location' => $settings['configDirectory'] . "/interface.wlan0"),
            //Manual/Custom DNS Settings stored here
            'dns' => array('type' => 'file', 'location' => $settings['configDirectory'] . "/dns"),
        ),
        'special' => true,
    ),
    'virtualEEPROM' => array(
        'friendly_name' => 'Virtual EEPROM',
        'file' => array(
            'cape-eeprom.bin' => array('type' => 'file', 'location' => $settings['configDirectory'] . '/cape-eeprom.bin')),
        'binary' => true),
);

//FPP Backup version - This is used for tracking the CURRENT backup file format or "backup" version as we may move things around and need backwards compatibility when restoring older version
//When restoring, this value is read from the uploaded file and in processRestoreData() used to decide on what extra massaging old data needs to work with the current script
$fpp_backup_format_version = "8.0";

//The v4, v5, vX that appears in backup filename was originally using $fpp_backup_version but to save user confusion will now match the FPP major version
//it was originally intended as a visual aid to help discern between different backup versions.
$fpp_major_version = collectFppMajorVersion();

//Directory where backups will be saved
$fpp_backup_location = $settings['configDirectory'] . "/backups";
//Flag whether the users browser will download the JSON file - default to always download, this is forced off when calling via API for example
$fpp_backup_prompt_download = true;

/////////////////////
/// HOUSE KEEPING ///
//NOT USED - Max age for stored settings backup files, Default: 90 days
$fpp_backup_max_age = 90;
//Max number of backup files we want to keep Default: 45
$fpp_backup_min_number_kept = 60;
//Max number of backup per plugin or by trigger source (eg. /api/config_file/someconfig all backup generated via this endpoint will have a trigger source of  config_file/someconfig we'll limit those backups to the below, default: 5
$fpp_backup_max_plugin_backups_key = 5;

////////////////////
/// DEBUGGING
$backups_verbose_logging = false;

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
$uploadData_IsProtected = true;

//list of settings restored
$settings_restored = array();
//restore done state
$restore_done = false;
//Restored Network Setting tracking
$network_settings_restored = false;
$network_settings_restored_post_apply = array('wired_network' => "", 'wifi_network' => "");
$network_settings_restored_applied_ips = array('wired_network' => array(), 'wifi_network' => array());

//Array of settings by name/key name, that are considered sensitive/taboo
$sensitive_data = array('emailpass', 'password', 'secret');

//Lookup arrays for what is a json and a ini file
$known_json_config_files = array('channelInputs', 'universe_inputs', 'dmx_inputs', 'gpio-input', 'channelOutputs', 'commandPresets', 'outputProcessors', 'universes', 'pixel_strings', 'bbb_strings', 'pwm', 'led_panels', 'other', 'model-overlays');
$known_ini_config_files = array('settings', 'system_settings', 'network', 'wired', 'wifi');

//Remove BBB Strings from the system areas if we're on a Pi or any other platform that isn't a BBB
if (!$settings['BeaglePlatform']) {
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
            $restore_area_main = $restore_area_arr[0]; //main area is first
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
                doRestore($restore_area, $file_contents_decoded, $rstfname, $keepNetworkSettings, $keepMasterSlaveSettings, 'page');
            }
        }
    }
}

/**
 * Does some initial processing before calling processRestoreData(), to do the heavy lifting to restore the backup.
 *
 * @param $restore_Area String The area which we want to restore content for
 * @param $restore_Data Array Contents of the JSON backup file as associative array
 * @param $restore_Filepath String File path to the backup file that was used
 * @param $restore_keepNetworkSettings Bool Whether or not we keep the system's current network settings, DEFAULT TRUE
 * @param $restore_keepMasterSlaveSettings Bool Whether or not we keep the system's current Master/Slave settings, DEFAULT TRUE
 * @param $restore_Source String The source of the call to restore backups, either page or api, this helps changing some logic when restoring via API
 * @return array
 */
function doRestore($restore_Area, $restore_Data, $restore_Filepath, $restore_keepNetworkSettings = true, $restore_keepMasterSlaveSettings = true, $restore_Source = 'page')
{
    global $system_config_areas, $keepNetworkSettings, $keepMasterSlaveSettings, $fpp_backup_format_version, $backup_errors, $restore_done, $uploadData_IsProtected;

    $keepNetworkSettings = $restore_keepNetworkSettings;
    $keepMasterSlaveSettings = $restore_keepMasterSlaveSettings;
    //
    $file_contents_decoded = $restore_Data;
    $_fpp_backup_version = null;

    //if restore area contains a forward slash, then we want to restore into a sub-area
    //split string to get the main area and sub-area
    if (stripos($restore_Area, "/") !== false) {
        $restore_area_arr = explode("/", $restore_Area);
        $restore_area_main = $restore_area_arr[0]; //main area is first
    } else {
        $restore_area_main = $restore_Area;
    }

    //Make sure a valid restore area has been specified
    if (array_key_exists($restore_area_main, $system_config_areas)) {
        //successful decode
        if ($file_contents_decoded !== false && is_array($file_contents_decoded)) {
            //Get value of protected state and remove it from the array
            if (array_key_exists('protected', $file_contents_decoded)) {
                $uploadData_IsProtected = $file_contents_decoded['protected'];
                unset($file_contents_decoded['protected']);
            }

            //work out of backup file is version 2 or not
            //if it's not a version 2 file, then we can only really restore settings
            //email can be restored because it's contained in the settings

            //Version 2 backups need to restore the schedule file to the old locations (auto converted on FPPD restart)
            //Version 3 backups need to restore the schedule to a different file
            //Check backup version
            if (array_key_exists('fpp_backup_version', $file_contents_decoded)) {
                if (!is_null($file_contents_decoded['fpp_backup_version'])) {
                    $_fpp_backup_version = floatval($file_contents_decoded['fpp_backup_version']); //Minimum version is 2
                } else {
                    $_fpp_backup_version = floatval($fpp_backup_format_version);
                }
            }
            //Version 4-6 so far don't need any compatibility fixes when being restored
            //Handling of version differences for specific areas is also done in more detail in processRestoreData()

            //Remove the platform key as it's not used for anything yet
            unset($file_contents_decoded['platform']);
            unset($file_contents_decoded['fpp_backup_version']);
            unset($file_contents_decoded['backup_comment']);
            unset($file_contents_decoded['backup_taken']);

            //Restore all areas
            if (strtolower($restore_Area) == "all") {
                // ALL SETTING RESTORE
                //read each area and process it
                foreach ($file_contents_decoded as $restore_area_key => $area_data) {
                    //Pass the restore area and data to the restore function
                    $restore_result = processRestoreData($restore_area_key, $area_data, $_fpp_backup_version);
                    //Restore is done - results will be printed on the page
                    $restore_done = true;
                }

                //Restore all other areas, settings can be restored regardless of version
                //if the area is not settings or it's not a v2 backup then nothing will be done
            } else {
                // ALL OTHER SETTING RESTORE
                //Process specific restore areas, this work almost like the 'all' area
                //general settings, but only a matching area is cherry picked

                //If the key exists in the decoded data then we can process
                if (is_array($file_contents_decoded) && array_key_exists($restore_area_main, $file_contents_decoded)) {
                    $restore_area_key = $restore_area_main;
                    $area_data = $file_contents_decoded[$restore_area_main];

                    //If we're restoring channelOutputs, we might need the system settings.. eg when restoring the LED panel data we need to set the layout in settings
                    if ($restore_area_key == "channelOutputs" && array_key_exists('settings', $file_contents_decoded)) {
                        $system_settings = array();
                        if (is_array($file_contents_decoded['settings']) && array_key_exists('system_settings', $file_contents_decoded['settings'])) {
                            $system_settings = $file_contents_decoded['settings']['system_settings'];
                            //modify the area data
                            $area_data_new = array('area_data' => $area_data, 'system_settings' => $system_settings);
                            $area_data = $area_data_new;
                        }
                    }

                    //Pass the restore area and data to the restore function
                    $restore_result = processRestoreData($restore_Area, $area_data, $_fpp_backup_version);
                    //Restore is done - results will be printed on the page
                    $restore_done = true;
                }
            }

            //All processed
            //$restore_done = true;
        } else {
            $backup_error_string = "doRestore: The backup " . $restore_Filepath . " data could not be decoded properly. Is it a valid backup file?";
            $backup_errors[] = $backup_error_string;
            error_log($backup_error_string);
        }
    } else {
        $backup_error_string = "doRestore: Invalid restore area specified" . $restore_area_main . " data could not be decoded properly. Is it a valid backup file?";
        $backup_errors[] = $backup_error_string;
        error_log($backup_error_string);
    }

    //$restore_done is set if we got to actually call the function to restore data, if there was some sort of error with the data beforehand it will never get set
    //use this as a simple check so we can return other data (what errors we had)
    return !empty($restore_result) ? array('success' => true, 'message' => $restore_result) : array('success' => false, 'message' => $backup_errors);
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
 * Function to look after backup restorations
 * @param $restore_area String  Area to restore
 * @param $restore_area_data array  Area data as an array
 * @param $backup_version boolean Version of the backup
 * @return array Save result
 */
function processRestoreData($restore_area, $restore_area_data, $backup_version)
{
    global $SUDO, $settings, $mediaDirectory, $scheduleFile, $system_config_areas, $keepMasterSlaveSettings, $keepNetworkSettings, $uploadData_IsProtected, $settings_restored,
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
        $restore_area_key = $restore_area_arr[0]; //main area is first
        $restore_area_sub_key = $restore_area_arr[1]; //sub-area is 2nd
    } else {
        $restore_area_key = $restore_area;
    }

    //Check to see if the area data contains the area_data or system_settings keys
    //break them out if so & overwrite $restore_area_data with the actual area data
    if (is_array($restore_area_data) && array_key_exists('area_data', $restore_area_data) && array_key_exists('system_settings', $restore_area_data)) {
        $restore_area_system_settings = $restore_area_data['system_settings'];
        $restore_area_data = $restore_area_data['area_data'];
    }

    //Should probably skip restoring data if the restore data is actually empty (maybe no config existed when the backup was made)
    //this is also avoid false negatives of restore failures for areas where there wasn't any data to restore
    $restore_data_is_empty = (empty($restore_area_data) || is_null($restore_area_data)) ? true : false;

    //set some initial values
    $settings_restored[$restore_area_key]['SUCCESS'] = $save_result;
    $settings_restored[$restore_area_key]['VALID_DATA'] = !$restore_data_is_empty; //Negate the value as this described is the data is empty or not, so false is valid data, true means data was empty so it isn't valid data

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

        if (file_put_contents($channel_remaps_json_filepath, $outputProcessors_data) === false) {
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

        if (file_put_contents($channelmemorymaps_filepath, $data) === false) {
            $save_result = false;
        } else {
            $save_result = true;
        }

        $overlays_json_filepath = $system_config_areas['model-overlays']['file'];
        //PrettyPrint the JSON data and save it
        $outputProcessors_data = prettyPrintJSON(json_encode($restore_area_data));

        if (file_put_contents($overlays_json_filepath, $outputProcessors_data) === false) {
            $save_result = false;
        } else {
            $save_result = true;
        }
        $settings_restored[$restore_area_key]['SUCCESS'] = $save_result;

        //Set FPPD restart flag
        WriteSettingToFile('restartFlag', 1);
    }

    //CHANNEL INPUTS - E1.31 BRIDGE, DMX
    if ($restore_area_key == "channelInputs" && !$restore_data_is_empty) {
		$settings_restored[$restore_area_key] = array();

		//RESTORE TWEAKS FOR SPECIFIC VERSIONS
		//Version 8.0 backups - channelInputs now holds an array of channel input configurations (E1.31/DDP, DMX etc), instead of a single entry for the default E1.31
		//                    - <8.0 we massage the data so it's in the new format (channelInputs => ['universe_inputs' => "data", "dmx_inputs" => "data"]

		if ($backup_version < 8.0) {
			//Get the existing channel input data
			$restore_area_data_ci['channelInputs'] = $restore_area_data;
			//Remove the old channelInputs key and replace it with universe_inputs (to match the new format for the channel input section
			unset($restore_area_data['channelInputs']);
			$restore_area_data['universe_inputs'] = $restore_area_data_ci;
			unset($restore_area_data_ci);
		}

		$restore_areas = $system_config_areas[$restore_area_key]['file'];
		processRestoreDataArray($restore_area_key, $restore_area_sub_key, $restore_areas, $restore_area_data,$backup_version);
    }

    //GPIO INPUTS - GPIO Input Triggers
    if ($restore_area_key == "gpio-input" && !$restore_data_is_empty) {
        //Just write strain to the gpio.json file
        $gpioInputs_filepath = $system_config_areas['gpio-input']['file'];

        $settings_restored[$restore_area_key]['ATTEMPT'] = true;
        //PrettyPrint the JSON data and save it
        $gpioInputs_data = prettyPrintJSON(json_encode($restore_area_data));
        if (file_put_contents($gpioInputs_filepath, $gpioInputs_data) === false) {
            $save_result = false;
        } else {
            $save_result = true;
        }

        $settings_restored[$restore_area_key]['SUCCESS'] = $save_result;
    }

    //SHOW SETUP, CHANNEL OUTPUT AND MISC CONFIG RESTORATION
    if (($restore_area_key == "show_setup" || $restore_area_key == "channelOutputs" || $restore_area_key == "misc_configs") && !$restore_data_is_empty) {
        $settings_restored[$restore_area_key] = array();

        $script_filenames = array();
        $restore_areas = $system_config_areas[$restore_area_key]['file'];

		processRestoreDataArray($restore_area_key, $restore_area_sub_key, $restore_areas, $restore_area_data,$backup_version);

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
                    if (file_put_contents($plugin_settings_path, $data) === false) {
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
                        if (file_put_contents($plugin_settings_path, $data) === false) {
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
        //search through the files that should have been backed up
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
                    if (file_put_contents($commandPresets_filepath, $commandPresets_data_restore) === false) {
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
//                        && array_key_exists('emailenable', $restore_data)
                        && array_key_exists('emailserver', $restore_data)
                        && array_key_exists('emailuser', $restore_data)
                        && array_key_exists('emailpass', $restore_data)
                        && array_key_exists('emailfromtext', $restore_data)
                        && array_key_exists('emailtoemail', $restore_data)
                    ) {
                        $settings_restored[$restore_area_key][$restore_areas_idx]['ATTEMPT'] = true;

                        if (is_array($restore_data) && array_key_exists('emailenable', $restore_data)) {
                            $emailenable = $restore_data['emailenable'];
                            WriteSettingToFile('emailenable', $emailenable);
                        }
                        //
                        $email_server = $restore_data['emailserver'];
                        //
                        $emailuser = $restore_data['emailuser'];
                        $emailpass = $restore_data['emailpass'];
                        //
                        $email_from_user = $restore_data['emailfromuser'];
                        $emailfromtext = $restore_data['emailfromtext'];
                        $emailtoemail = $restore_data['emailtoemail'];

                        //Write them out
                        //                        WriteSettingToFile('emailenable', $emailenable);
                        //Email Server
                        WriteSettingToFile('emailserver', $email_server);
                        //Email Server Port if set
                        if (array_key_exists('emailport', $restore_data)) {
                            $email_server_port = $restore_data['emailport'];
                            WriteSettingToFile('emailport', $email_server_port);
                        }
                        //Email User Name and Pass + From & To Uemails
                        WriteSettingToFile('emailuser', $emailuser);
                        //
                        WriteSettingToFile('emailfromuser', $email_from_user);
                        WriteSettingToFile('emailfromtext', $emailfromtext);
                        WriteSettingToFile('emailtoemail', $emailtoemail);

                        //Only save password and generate exim config if upload data is unprotected
                        //meaning the password was included in the backup,
                        //otherwise existing (valid) config may be overwritten
                        if ($uploadData_IsProtected == false && $emailpass != "") {
                            WriteSettingToFile('emailpass', $emailpass);
                            //Update the email config in the global settings array,  so can call the fuction that sets up and  writes out exim4 config
                            $settings['emailserver'] = $restore_data['emailserver'];
                            if (array_key_exists('emailport', $restore_data)) {
                                $settings['emailport'] = $restore_data['emailport'];
                            }
                            //Creds
                            $settings['emailuser'] = $restore_data['emailuser'];
                            $settings['emailpass'] = $restore_data['emailpass'];
                            //
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
                    if (is_array($restore_data) || !is_null($restore_data)) {
                        //Version 2 backups need to locate the TimeZone data slightly differently due to how it was stored back then
                        if ($backup_version == 2) {
                            $data = $restore_data[0]; //first index has the timezone, index 1 is empty to due carriage return in file when its backed up
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
                    if (file_put_contents($settings['configDirectory'] . "/proxies", $data) === false) {
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
    if ($restore_area_key == "network" && !$restore_data_is_empty) {
        //If the user doesn't want to keep the existing network settings, we can overwrite them
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
                if (is_array($system_config_areas['network']['file']) && !array_key_exists($net_type, $system_config_areas['network']['file'])) {
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
                            $ini_string .= "$ini_value\n";
                        }
                    }

                    //If we can parse out generated INI string, then it's value.
                    //Save it to the network file
                    if ((parse_ini_string($ini_string) !== false) && !empty($ini_string)) {

                        if (file_put_contents($network_setting_filepath, $ini_string) === false) {
                            $save_result = false;
                        } else {
                            $save_result = true;
                            $network_settings_restored = true;
                            //Map out the interface wired network and wifi network
                            $parsed_ini_string = parse_ini_string($ini_string);

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
            error_log("RESTORE: Not restoring " . $restore_area_key . " - 'Keep Existing Network Settings' selected - NOT OVERWRITING: ");
            //no attempt was made so remove the key for tracking attempts
            unset($settings_restored[$restore_area_key]);
        }

    }

    //Virtual EEPROM for capes
    if ($restore_area_key == "virtualEEPROM" && !$restore_data_is_empty) {
        $settings_restored[$restore_area_key]['ATTEMPT'] = true;
        $save_result = false;
        foreach ($system_config_areas['virtualEEPROM']['file'] as $fn => $finfo) {
            if (isset($restore_area_data[$fn])) {
                file_put_contents("/home/fpp/media/tmp/here.$fn", json_encode($restore_area_data));
                if (file_put_contents($finfo['location'], base64_decode($restore_area_data[$fn][0])) === false) {
                    $save_result = false;
                } else {
                    $save_result = true;
                }
            }
        }

        $settings_restored[$restore_area_key]['SUCCESS'] = $save_result;

        //Set FPPD reboot flag
        WriteSettingToFile('rebootFlag', 1);
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

    //finally if there was no data for the restore area remove it's key for tracking attempts, this is fine here as the separate restore area tests above will avoid restore areas with no data
    if ($restore_data_is_empty) {
        unset($settings_restored[$restore_area_key]);
    }

    //wrote message out
    if (!$restore_data_is_empty &&
        (array_key_exists($restore_area_key, $settings_restored)
            && array_key_exists('ATTEMPT', $settings_restored[$restore_area_key])
            && $settings_restored[$restore_area_key]['ATTEMPT']
            && !$save_result
        )
    ) {
        error_log("RESTORE: Failed to restore " . $restore_area . " - " . json_encode($settings_restored));
    }

    //Return save result
    return $settings_restored;
}

/**
 * Process restore data that exists an array of multiple files (e.g channelOutputs is made yp of multiple files)
 *
 * @param $restore_area_key String The parent restore area e.g channelOutputs
 * @param $restore_area_sub_key String The child area e.g pixel_strings (under channelOutputs
 * @param $restore_areas Array The parent restore area and its child areas's/files
 * @param $restore_area_data Array The uploaded data for this parent restore area
 * @param $backup_version Float Backup version used to determine any backward compatibility tweaks
 * @return void
 */
function processRestoreDataArray($restore_area_key, $restore_area_sub_key, $restore_areas, $restore_area_data,$backup_version)
{
	global $SUDO, $settings, $mediaDirectory, $scheduleFile, $system_config_areas, $keepMasterSlaveSettings, $keepNetworkSettings, $uploadData_IsProtected, $settings_restored,
		   $network_settings_restored, $network_settings_restored_post_apply, $network_settings_restored_applied_ips,
		   $known_ini_config_files, $known_json_config_files;
	global $args;

	//RESTORE TWEAKS FOR SPECIFIC VERSIONS (all changes done by the logic below
	//Version 2 backups need to restore the schedule file to the old locations (auto converted on FPPD restart)
	//Version 3 backups need to restore the schedule to it's new json location
	//Version 4 backups - nothing changed
	//Version 5 backups - FPD/Falcon Pixlenet at the root of ['channelOutputs']['falcon_pixelnet_DMX'] is FPDv1 data
	//Version 6 backups - FPD/Falcon Pixlenet DMX data is keyed by the file it came from to more easily support a future version
	//                  - ['channelOutputs']['falcon_pixelnet_DMX'] will contain an additional key for each FPD file read
	//                  - eg Falcon.FPDV1 and in the future Falcon.F16V2
	//Version >7.2 backups - Panel layout is generated from the actual panel config (using col & row data) if the layout doesn't exist anywhere
	//                    - Single Panel size is generated from the same config and also written to the system settings

	//search through the files that should of been backed up for the specified area, eg. channelOutputs has multiple files
	//and then loop over the restore data and match up the data and restore it if anything exists.
	foreach ($restore_areas as $restore_areas_idx => $restore_areas_data) {
		$restore_location = $restore_areas_data['location'];
		$restore_type = $restore_areas_data['type'];
		$final_file_restore_data = "";

		//If $restore_area_sub_key is empty then no sub-area has been chosen -- restore as normal and restore all sub areas, which should be everything under e.g $system_config_areas['channelOutputs']
		//Or if $restore_area_sub_key is equal to the $show_setup_area_index we're on, then restore just this area because it matches users selection e.g user may just want the show schedule
		//and break the loop
		if ($restore_area_sub_key == "" || ($restore_area_sub_key == $restore_areas_idx)) {
			//if the restore key and the $system_config_areas key match then restore data to whatever location it is
			//eg. if we are on events, then look for events in the restore data, when found restore data to the events location (eg look at the location)
			foreach ($restore_area_data as $restore_area_data_index => $restore_area_data_payload) {
				$save_result = false;

				//$restore_area_data_data is an array representing the file contents
				//$restore_area_data_index represents the filename (used to key the array)
				if ($restore_areas_idx == $restore_area_data_index) {
					//data is an array then we can go
					if (is_array($restore_area_data_payload)) {
						//loop over all the files and their data and restore each
						//e.g $restore_area_data_data array will look like
						//array ('event'=> array('01_01.fevt' => array(data), '21_10.fevt' => array(data)))
						foreach ($restore_area_data_payload as $filename_to_restore => $file_data) {
							$save_result = false;

							$restore_location = $restore_areas_data['location']; //reset
							$final_file_restore_data = ""; //store restore data in variable

							//Work out what method we need to use to get the data back into an array
							if ($restore_type == "dir" || $restore_type == "file") {
								if (in_array($restore_area_data_index, $known_json_config_files)) {
									//JSON
									$final_file_restore_data = $file_data;
								} else {
									//everything else
									//line separate the lines
									$final_file_restore_data = implode("\n", $file_data);
								}
							} else if ($restore_type == "function") {
								//get the data as in without any modification so we can pass it into the restore function
								$final_file_restore_data = $file_data;
							}

							//If backup/restore type of a sub-area is folder, then build the full path to where the file will be restored
							if ($restore_type == "dir") {
								$restore_location .= "/" . $filename_to_restore;
							}

							//if restore sub-area is scripts, capture the file names so we can pass those along through RestoreScripts which will perform any InstallActions
							if (strtolower($restore_areas_idx) == "scripts") {
								$script_filenames[] = $filename_to_restore;
							}

							//if restore sub-area is LED panels, we need write the matrix size / layout setting to the settings file in case it's different to the backup
							if (strtolower($restore_areas_idx) == "led_panels") {
								$panel_layout = null;

								//Generate the single LED Panel size from info in the LED Panel layout e.g 32x16 1/2 Scan
								if (is_array($final_file_restore_data['channelOutputs'][0]) &&
									(
										array_key_exists('panelHeight', $final_file_restore_data['channelOutputs'][0])
										&& array_key_exists('panelWidth', $final_file_restore_data['channelOutputs'][0])
										&& array_key_exists('panelScan', $final_file_restore_data['channelOutputs'][0])
									)
								) {
									$singlePanelSize = $final_file_restore_data['channelOutputs'][0]['panelWidth'] . 'x' . $final_file_restore_data['channelOutputs'][0]['panelHeight'] . 'x' . $final_file_restore_data['channelOutputs'][0]['panelScan'];
									WriteSettingToFile('LEDPanelsSize', $singlePanelSize);
								}

								//Write the panel layout, e.g 4x4 into the system settings
								//This setting can exist in a few places (by default it's in the system settings)
								if (is_array($final_file_restore_data['channelOutputs'][0]) &&
									array_key_exists('ledPanelsLayout', $final_file_restore_data['channelOutputs'][0])
								) {
									WriteSettingToFile('LEDPanelsLayout', $final_file_restore_data['channelOutputs'][0]['ledPanelsLayout']);
								} elseif (!empty($restore_area_system_settings)) {
									// If it's a full backup we can get the panel settings from the system settings
									$panel_layout = $restore_area_system_settings[0]['LEDPanelsLayout'];

									if ($panel_layout != null) {
										//LEDPanelsLayout = "4x4"
										WriteSettingToFile('LEDPanelsLayout', $panel_layout);
									}
								} else {
									// ledPanelsLayout && LEDPanelsLayout don't exist so we can't determine the matrix size
									//As a last resort we can calculate the matrix size from the led panel layout
									$maxCol = $maxRow = null;
									$panel_layout_data = $final_file_restore_data['channelOutputs'][0]['panels'];

									foreach ($panel_layout_data as $idx => $panel) {
										if ($panel['col'] > $maxCol) {
											$maxCol = $panel['col'];
										}
										if ($panel['row'] > $maxRow) {
											$maxRow = $panel['row'];
										}
									}

									if ($maxCol != null && $maxRow != null) {
										//Adjust max values due to the array being 0 base
										$maxCol = $maxCol + 1;
										$maxRow = $maxRow + 1;

										//LEDPanelsLayout = "4x4"
										WriteSettingToFile('LEDPanelsLayout', $maxCol . "x" . $maxRow);
									}

								}
							}

							//if restore sub-area is the schedule, determine how to restore it based on the $backup_version
							if (strtolower($restore_areas_idx) == "schedule") {
								if ($backup_version == 2) {
									//Override the restore location so we write to the old schedule file, FPPD will convert this to the new json file
									$restore_location = $scheduleFile;
								}
//                                    else if ($backup_version == 3){
								//                                    //restore it to the new json file - don't adjust the path it'll go to configured path
								//                                    }
							}

							//if restore sub-area is falcon_pixelnet_DMX (under channelOutputs), determine how to restore it based on the $backup_version
							if (strtolower($restore_areas_idx) == "falcon_pixelnet_dmx") {
								//For FPD, backups from version 5 and under contain Falcon.FPDV1 data at it's root, extract and re-key the data, so it can be used correctly by the restore function
								//This change was also made part way through the usage of version 6, we can check the version and presence of the Falcon.FPDV1 key and do the same thing
								if ($backup_version <= 5 || ($backup_version == 6 and !array_key_exists('Falcon.FPDV1', $final_file_restore_data))) {
									$FPD_temp = array('Falcon.FPDV1' => $final_file_restore_data);
									$final_file_restore_data = $FPD_temp;
								}
							}

							///////////////////////////////////////////////////
							//////////////////////////////////////////////////
							//If we have data then write to where it needs to go
							if (!empty($final_file_restore_data)) {
								//Set we have valid ata
								$settings_restored[$restore_area_key][$restore_area_data_index]['VALID_DATA'] = true;

								//Decide what action to take based on the restore type
								if (($restore_type == "dir" || $restore_type == "file")) {
									$settings_restored[$restore_area_key][$restore_area_data_index]['ATTEMPT'] = true;

									//Work out what method we need to use to get the data back out into the correct format
									if (in_array($restore_area_data_index, $known_json_config_files)) {
										//JSON
										$final_file_restore_data = prettyPrintJSON(json_encode($final_file_restore_data));
									}
									//Save out the file
									if (file_put_contents($restore_location, $final_file_restore_data) === false) {
										$save_result = false;
									} else {
										$save_result = true;
									}
								} //If restore type is function - call the function
								else if ($restore_type == "function") {
									$settings_restored[$restore_area_key][$restore_area_data_index]['ATTEMPT'] = true;

									$restore_function = $restore_areas_data['location']['restore'];
									//if we have valid data and the function exists, call it
									if (function_exists($restore_function)) {
										$save_result = $restore_function($final_file_restore_data);
									}
								} else {
									//Unlikely - but no match on restore type
									$save_result = false;
								}
							} else {
								//Restore data is still empty after massaging
								$settings_restored[$restore_area_key][$restore_area_data_index]['VALID_DATA'] = false;
							}
						}
					} else {
						//Not an array, we need an array here, so data is not valid
						$settings_restored[$restore_area_key][$restore_area_data_index]['VALID_DATA'] = false;
					}
					$settings_restored[$restore_area_key][$restore_area_data_index]['SUCCESS'] = $save_result;

					//Cheat and remove the success key if no attempt was even made (no attempt key) and there was no valid data
					//meaning that we didn't even try, this check is done after the key and value is set because $restore_area_data_index might not exist at all if the ATTEMPT or VALID_DATA key was set
					if (!array_key_exists('ATTEMPT', $settings_restored[$restore_area_key][$restore_area_data_index])
						&&
						!array_key_exists('VALID_DATA', $settings_restored[$restore_area_key][$restore_area_data_index])) {
						unset($settings_restored[$restore_area_key][$restore_area_data_index]);
					}

					break; //break after data is restored for this section
				}
			}
			//Don't break if area key is empty, because we want to process all the sub-areas
			if ($restore_area_sub_key != "") {
				//we found the sub-area, stop looking
				break;
			}
		}
	}
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
 * Sets the Audio Output device
 * @param $card String soundcard
 * @return  string
 */
function SetAudioOutput($card)
{
    global $args, $SUDO, $debug;

    global $args, $SUDO, $debug, $settings;

    if ($card != 0 && file_exists("/proc/asound/card$card")) {
        exec($SUDO . " sed -i 's/card [0-9]/card " . $card . "/' /root/.asoundrc", $output, $return_val);
        unset($output);
        if ($return_val) {
            error_log("Failed to set audio to card $card!");
            return;
        }
        if ($debug) {
            error_log("Setting to audio output $card");
        }
    } else if ($card == 0) {
        exec($SUDO . " sed -i 's/card [0-9]/card " . $card . "/' /root/.asoundrc", $output, $return_val);
        unset($output);
        if ($return_val) {
            error_log("Failed to set audio back to default!");
            return;
        }
        if ($debug) {
            error_log("Setting default audio");
        }

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
function ReadTimeZone()
{
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
    unset($system_config_areas_local['misc_configs']); //to avoid something weird happening
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
//                                echo "Excluding file from Extra Config backup" . $path_parts_filename;
                            }
                        } //end loop
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
//                            echo "Excluding file from Extra Config backup" . $path_parts_filename;
                        }
                    } //end else
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
//                        echo "Excluding file from Extra Config backup" . $path_parts_filename;
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
            if (file_put_contents($misc_data_rest_filepath, $misc_data_restore) === false) {
                $save_result = false;
            } else {
                $save_result = true;
            }
        }
    }

    return $save_result;
}

/**
 * This function acts as a wrapper to collect the Falcon.FPDV1 and Falcon.FPDV2 files
 * @return array
 */
function LoadPixelnetDMXFiles()
{
    global $settings;

    $FPD_V1_data = array();
    $FPD_V2_data = array();
    $return_data = array();

    //check if the FPDV1 file exists
    if (file_exists($settings['configDirectory'] . "/Falcon.FPDV1")) {
        $FPD_V1_data = LoadPixelnetDMXFile_FPDv1();
        //If data is valid put into return array to be saved into the JSON file
        if (is_array($FPD_V1_data) && !empty($FPD_V1_data)) {
            $return_data['Falcon.FPDV1'] = $FPD_V1_data;
        }
    }

//    if (file_exists($settings['configDirectory'] . "/Falcon.F16V2-alpha") || file_exists($settings['configDirectory'] . "/Falcon.F16V2")) {
//        $FPD_V2_data = LoadPixelnetDMXFile_FPDV2();
//
//        //If data is valid put into return array to be saved into the JSON file
//        if (is_array($FPD_V1_data) && !empty($FPD_V1_data)) {
//            $return_data['Falcon.F16V2'] = $FPD_V2_data;
//        }
//    }
    return $return_data;
}

/**
 * This function acts as a wrapper to restore the Falcon.FPDV1 and Falcon.FPDV2 files
 * depending on what restore data is available, we'll call the relevant function to restore data data
 * @param $restore_data array FPD data to restore
 * @return array
 */
function SavePixelnetDMXFiles($restore_data)
{
    $FPD_V1_Restore_data = false;
    $FPD_V2_Restore_data = false;
    $write_status_arr = array();

    //check if the FPDV1 file exists
    if (array_key_exists('Falcon.FPDV1', $restore_data) && !empty($restore_data['Falcon.FPDV1'])) {
        $FPD_V1_Restore_data = SavePixelnetDMXFile_FPDv1($restore_data['Falcon.FPDV1']);
        $write_status_arr['Falcon.FPDV1'] = $FPD_V1_Restore_data;
    }

//    if (array_key_exists('Falcon.F16V2-alpha', $restore_data) && !empty($restore_data['Falcon.F16V2-alpha'])) {
//        $FPD_V2_Restore_data = SavePixelnetDMXFile_F16v2Alpha($restore_data['Falcon.F16V2-alpha']);
//
//        $write_status_arr['Falcon.F16V2-alpha'] = $FPD_V2_Restore_data;
//    } else if (array_key_exists('Falcon.F16V2', $restore_data) && !empty($restore_data['Falcon.F16V2'])) {
//        $FPD_V2_Restore_data = SavePixelnetDMXFile_F16v2Alpha($restore_data['Falcon.F16V2']);
//
//        $write_status_arr['Falcon.F16V2'] = $FPD_V2_Restore_data;
//    }

    return $write_status_arr;
}

/**
 * Reads the Falcon.FPDV1 - Falcon Pixelnet/DMX file
 * extracted & modified from channel_get_pixelnetDMX() in /api/controllers/channel.php
 *
 * @return array|void
 */
function LoadPixelnetDMXFile_FPDv1()
{
    global $settings;
    //Pull in the class
    require_once 'pixelnetdmxentry.php';
    //Store data in an array instead of session
    $return_data = array();

//    if (@filesize($settings['configDirectory'] . "/Falcon.FPDV1") < 1024) {
    //        return $return_data;
    //    }

    $f = fopen($settings['configDirectory'] . "/Falcon.FPDV1", "rb");
    if ($f == false) {
        fclose($f);
        //No file exists add one and save to new file.
        $address = 1;
        for ($i = 0; $i < 12; $i++) {
            $return_data[] = new PixelnetDMXentry(1, 0, $address);
            $address += 4096;
        }
    } else {
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
            $return_data[] = new PixelnetDMXentry($active, $type, $startAddress);
        }
    }

    return $return_data;
}

/**
 * TODO Reads the Falcon.FPDV2 - Falcon Pixelnet/DMX file
 * extracted & modified from channel_get_pixelnetDMX() in /api/controllers/channel.php
 *
 * @return array|void
 */
function LoadPixelnetDMXFile_FPDV2()
{
    //TODO (doesn't appear to be any code to reference for reading these files in /api/controllers/channel.php),, channel_get_pixelnetDMX() in the API is currently only reading the FPDv1 file
    return null;
}

/**
 * Restores the FPDv1 channel data from the supplied array
 * extracted & modified from SaveFPDv1() in /api/controllers/channel.php
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

        for ($o = 0; $o < $outputCount; $o++) {

            // Active
            $active = 0;
            $carr[$i++] = 0;
            if (isset($restore_data[$o]['active']) && ($restore_data[$o]['active'] || $restore_data[$o]['active'] == '1' || intval($restore_data[$o]['active']) == 1)) {
                $active = 1;
                $carr[$i - 1] = 1;
            }

            // Start Address
            $startAddress = intval($restore_data[$o]['startAddress']);
            $carr[$i++] = $startAddress % 256;
            $carr[$i++] = $startAddress / 256;

            // Type
            $type = intval($restore_data[$o]['type']);
            $carr[$i++] = $type;
        }
        $f = fopen($settings['configDirectory'] . "/Falcon.FPDV1", "wb");

        if (fwrite($f, implode(array_map("chr", $carr)), 1024) === false) {
            $write_status = false;
        } else {
            $write_status = true;
        }

        fclose($f);

    }
    return $write_status;
}

/**
 * Restores the FPDv2 channel data from the supplied array
 * extracted & modified from SaveFPDv1() in /api/controllers/channel.php
 *
 * @param $restore_data array FPDv2 data to restore
 * @return bool Success state file write
 */
//function SavePixelnetDMXFile_F16v2Alpha($restore_data)
//{
//    global $settings;
//    $outputCount = 16;
//    $write_status = false;
//
//    if (!empty($restore_data) && is_array($restore_data)) {
//        $carr = array();
//        for ($i = 0; $i < 1024; $i++) {
//            $carr[$i] = 0x0;
//        }
//
//        $i = 0;
//
//        // Header
//        $carr[$i++] = 0x55;
//        $carr[$i++] = 0x55;
//        $carr[$i++] = 0x55;
//        $carr[$i++] = 0x55;
//        $carr[$i++] = 0x55;
//        $carr[$i++] = 0xCD;
//
//        // Some byte
//        $carr[$i++] = 0x01;
//
//        for ($o = 0; $o < $outputCount; $o++) {
//            $cur = $restore_data[$o];
//            $nodeCount = $cur['nodeCount'];
//            $carr[$i++] = intval($nodeCount % 256);
//            $carr[$i++] = intval($nodeCount / 256);
//
//            $startChannel = $cur['startChannel'] - 1; // 0-based values in config file
//            $carr[$i++] = intval($startChannel % 256);
//            $carr[$i++] = intval($startChannel / 256);
//
//            // Node Type is set on groups of 4 ports
//            $carr[$i++] = intval($cur['nodeType']);
//
//            $carr[$i++] = intval($cur['rgbOrder']);
//            $carr[$i++] = intval($cur['direction']);
//            $carr[$i++] = intval($cur['groupCount']);
//            $carr[$i++] = intval($cur['nullNodes']);
//        }
//
//        $f = fopen($settings['configDirectory'] . "/Falcon.F16V2-alpha", "wb");
//
//        if (fwrite($f, implode(array_map("chr", $carr)), 1024) === false) {
//            $write_status = false;
//        } else {
//            $write_status = true;
//        }
//
//        fclose($f);
//    }
//
//    return $write_status;
//}

/**
 * Performs all the work to gather files to be backed up for the specified backup area
 * optionally $allowDownload can be set to false in order to prevent the backup being sent to the users browser and just have the file backed up to the config/backups folder
 *
 * This function can be called directly to perform a backup of settings, $allowDownload should be false in this instance to avoid header issues
 *
 * @param string $area Backup area to be backed up, refer $system_config_areas for the currently defined list of area
 * @param bool $allowDownload Toggle to allow or disallow the backup file being sent to the users browser
 * @param string $backupComment Optionally add a comment to the backup file that may be useful to describe what the backup is for
 * @param string $backupTriggerSource Optionally add where/what triggered the backup
 */
function performBackup($area = "all", $allowDownload = true, $backupComment = "User Initiated Manual Backup", $backupTriggerSource = null)
{
    global $fpp_backup_format_version, $system_config_areas, $protectSensitiveData, $known_json_config_files, $known_ini_config_files;

    //Toggle the flag to disallow the backup file to be downloaded by the browser
    if ($allowDownload === false) {
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
                            if (is_array($sfd['location'])) {
                                //loop over the locations to read them all in
                                foreach ($sfd['location'] as $location_filename => $location_path) {
                                    //Check if the file to be backed up is one of a known type
                                    if (in_array($sfi, $known_ini_config_files)) {
                                        //INI
                                        //parse ini properly
                                        $backup_file_data = parse_ini_string(@file_get_contents($location_path));
                                    } else if (in_array($sfi, $known_json_config_files)) {
                                        //JSON
                                        //channelOutputsJSON is a formatted (prettyPrint) JSON file, decode it into an assoc. array
                                        $backup_file_data = json_decode(@file_get_contents($location_path), true);
                                    } else if (isset($config_data['binary']) && $config_data['binary']) {
                                        $backup_file_data = base64_encode(@file_get_contents($location_path));
                                    } else {
                                        //all other files are std flat files, process them into an array by splitting at line breaks
                                        $backup_file_data = explode("\n", @file_get_contents($location_path));
                                    }

                                    //Collect the backup data
                                    $file_data[$location_filename] = array($backup_file_data);
                                } //end loop
                            } else {
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
                                } else if (isset($config_data['binary']) && $config_data['binary']) {
                                    $backup_file_data = base64_encode(@file_get_contents($sfd['location']));
                                } else {
                                    //all other files are std flat files, process them into an array by splitting at line breaks
                                    $backup_file_data = explode("\n", @file_get_contents($sfd['location']));
                                }

                                //Collect the backup data
                                $file_data = array($backup_file_data);
                            } //end else

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
                        if (!isset($config_data['binary']) || !$config_data['binary']) {
                            $tmp_settings_data[$config_key][$sfi] = remove_sensitive_data($file_data);
                        } else {
                            $tmp_settings_data[$config_key][$sfi] = $file_data;
                        }
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
                        } else if (isset($config_data['binary']) && $config_data['binary']) {
                            $file_data = base64_encode(file_get_contents($setting_file_to_backup));
                        } else {
                            //all other files are std flat files, process them into an array by splitting at line breaks
                            $file_data = explode("\n", file_get_contents($setting_file_to_backup));
                        }
                        //Remove sensitive data
                        if (!isset($config_data['binary']) || !$config_data['binary']) {
                            $tmp_settings_data[$config_key] = remove_sensitive_data($file_data);
                        } else {
                            $tmp_settings_data[$config_key] = $file_data;
                        }
                    }
                }
                //End for loop processing each individual "area" in order to get all areas
            }
            //End processing "ALL" backup areas
        } else {
            //AREA - Individual Backup section selections will arrive here
            //All other backup areas for individual selections
            $setting_file_to_backup = $tmp_config_areas[$area]['file'];

            //if setting file value is an array then there are one or more setting files related to this backup
            if (is_array($setting_file_to_backup)) {
                //loop over the array
                foreach ($setting_file_to_backup as $sfi => $sfd) {
                    $file_data = array();
                    if ($sfd['type'] == "dir") {
                        $file_data = read_directory_files($sfd['location']);
                    } else if ($sfd['type'] == "file") {

                        //Check if the file location(s) is an array, if so there will likely be more than 1 file location to read
                        //All other times location should just be a simple string for the file location
                        if (is_array($sfd['location'])) {
                            //loop over the locations to read them all in
                            foreach ($sfd['location'] as $location_filename => $location_path) {
                                //Check if the file to be backed up is one of a known type
                                if (in_array($sfi, $known_ini_config_files)) {
                                    //INI
                                    //parse ini properly
                                    $backup_file_data = parse_ini_string(file_get_contents($location_path));
                                } else if (in_array($sfi, $known_json_config_files)) {
                                    //JSON
                                    //channelOutputsJSON is a formatted (prettyPrint) JSON file, decode it into an assoc. array
                                    $backup_file_data = json_decode(file_get_contents($location_path), true);
                                } else if (isset($tmp_config_areas[$area]['binary']) && $tmp_config_areas[$area]['binary']) {
                                    $backup_file_data = base64_encode(file_get_contents($location_path));
                                } else {
                                    //all other files are std flat files, process them into an array by splitting at line breaks
                                    $backup_file_data = explode("\n", file_get_contents($location_path));
                                }

                                //Collect the backup data
                                $file_data[$location_filename] = array($backup_file_data);
                            } //end loop
                        } else {
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
                            } else if (isset($tmp_config_areas[$area]['binary']) && $tmp_config_areas[$area]['binary']) {
                                $backup_file_data = base64_encode(file_get_contents($sfd['location']));
                            } else {
                                //all other files are std flat files, process them into an array by splitting at line breaks
                                $backup_file_data = explode("\n", file_get_contents($sfd['location']));
                            }

                            //Collect the backup data
                            $file_data = array($backup_file_data);
                        } //end else

                    } else if ($sfd['type'] == "function") {
                        $backup_function = $sfd['location']['backup'];
                        $backup_file_data = array();
                        if (function_exists($backup_function)) {
                            $backup_file_data = $backup_function();
                        }
                        $file_data = array($backup_file_data);
                    }
                    //Remove Sensitive data
                    if (!isset($tmp_config_areas[$area]['binary']) || !$tmp_config_areas[$area]['binary']) {
                        $tmp_settings_data[$area][$sfi] = remove_sensitive_data($file_data);
                    } else {
                        $tmp_settings_data[$area][$sfi] = $file_data;
                    }
                }
            } else {
                if ($setting_file_to_backup !== false && file_exists($setting_file_to_backup)) {
                    if (in_array($area, $known_ini_config_files)) {
                        $file_data = parse_ini_string(file_get_contents($setting_file_to_backup));
                    } else if (in_array($area, $known_json_config_files)) {
                        $file_data = json_decode(file_get_contents($setting_file_to_backup), true);
                    } else if (isset($tmp_config_areas[$area]['binary']) && $tmp_config_areas[$area]['binary']) {
                        $file_data = base64_encode(file_get_contents($setting_file_to_backup));
                    } else {
                        $file_data = explode("\n", file_get_contents($setting_file_to_backup));
                    }
                    //Remove sensitive data
                    if (!isset($tmp_config_areas[$area]['binary']) || !$tmp_config_areas[$area]['binary']) {
                        $tmp_settings_data[$area] = remove_sensitive_data($file_data);
                    } else {
                        $tmp_settings_data[$area] = $file_data;
                    }
                }
            }
            //End individual / specific backup area processing
        }

        //Add the backup comment into settings data that has been gathered
        $tmp_settings_data['backup_comment'] = $backupComment;
        //Add the trigger source also, this may be used internally when clearing/manging number of backup files
        $tmp_settings_data['backup_trigger_source'] = $backupTriggerSource;
        //Lastly insert the backup system version (moved from doBackupDownload to here)
        $tmp_settings_data['fpp_backup_version'] = $fpp_backup_format_version;
        //Add the current UNIX epoc time, representing the time the backup as taken, may make it easier in the future to calculate when the backup was taken
        $tmp_settings_data['backup_taken'] = time();

        //DO IT!
        if (!empty($tmp_settings_data)) {
            $backup_result = doBackupDownload($tmp_settings_data, $area);
            return $backup_result;
        } else {
            $backup_error_string = "SETTINGS BACKUP: Something went wrong while generating backup file for " . ucwords(str_replace("_", " ", $area)) . ", no data was found. Have these settings been configured?";
            $backup_errors[] = $backup_error_string;
            return false;
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
    global $settings, $protectSensitiveData, $fpp_major_version, $fpp_backup_prompt_download, $fpp_backup_location, $backup_errors;

    if (!empty($settings_data)) {
        //is sensitive data removed (selectively used on restore to skip some processes)
        $settings_data['protected'] = $protectSensitiveData;
        //platform identifier
        $settings_data['platform'] = $settings['Platform'];

        //Once we have all the settings, process the array and dump it back to the user
        //filename
        $backup_fname = $settings['HostName'] . "_" . $area . "-backup_" . "v" . $fpp_major_version . "_";
        //change filename if sensitive data is not protected
        if ($protectSensitiveData == false) {
            $backup_fname .= "unprotected_";
        }
        $backup_fname .= date("YmdHis") . ".json";

        //check to see fi the backup directory exists
        if (!file_exists($fpp_backup_location)) {
            if (mkdir($fpp_backup_location) == false) {
                $backup_error_string = "SETTINGS BACKUP: Something went wrong creating the backup file directory '" . $fpp_backup_location . "' , backup file can't be created (permissions error?) and backup download will fail as a result.";
                $backup_errors[] = $backup_error_string;
                error_log($backup_error_string);
            }
        } else {
            //Backup location exists, test if writable
            if (is_writable($fpp_backup_location) == false) {
                $backup_error_string = "SETTINGS BACKUP: Something went wrong, '" . $fpp_backup_location . "'  is not writable.";
                $backup_errors[] = $backup_error_string;
                error_log($backup_error_string);
            }
        }

        //Write a copy locally into "config/backups"
        $backup_local_fpath = $fpp_backup_location . '/' . $backup_fname;
        $json = json_encode($settings_data);

        //Write data into backup file
        if (file_put_contents($backup_local_fpath, $json) !== false) {
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
            //If any errors encountered along the way we should log them
            if (!empty($backup_error_string)) {
                error_log($backup_error_string);
            }

            return array('success' => true, 'backup_file_path' => $backup_local_fpath);
        } else {
            $backup_error_string = "SETTINGS BACKUP: Something went wrong while writing the backup file to '" . $backup_local_fpath . "', JSON backup file unable to be downloaded.";
            $backup_errors[] = $backup_error_string;
            error_log($backup_error_string);

            return array('success' => false, 'backup_file_path' => '');
        }
    } else {
        //no data supplied
        $backup_error_string = "SETTINGS BACKUP: Something went wrong while generating backup file for " . ucwords(str_replace("_", " ", $area)) . ", no data was supplied. Have these settings been configured?";
        $backup_errors[] = $backup_error_string;
        error_log($backup_error_string);

        return array('success' => false, 'backup_file_path' => '');
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
        //If the filename contains 'interface.' or 'leases.', then we want to keep it, discard anything else
        if ((stripos(strtolower($filename), "interface") === false) &&
            (stripos(strtolower($filename), "leases") === false)) {
            unset($network_interfaces[$filename]);
        }
        //else true so we keep the file since it's a interface
    }

    //Remove eth0 and wlan0, since the backup system will already include them by default
    //anything remaining is a extra interface
    if (is_array($network_interfaces) && array_key_exists('interface.eth0', $network_interfaces)) {
        unset($network_interfaces['interface.eth0']);
    }
    if (is_array($network_interfaces) && array_key_exists('interface.wlan0', $network_interfaces)) {
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
            if (stripos(strtolower($fileInfo), "text") !== false) {
                //split the string to get just the plugin name
                $plugin_name = explode(".", $fname);
                $plugin_name = $plugin_name[1];

                //Store config file locations here
                $locations = array();
                //Normal Plugin config file location
                $locations[$fname] = ($settings['configDirectory'] . "/" . $fname);
//                //Check the extra backup locations where the plugin name matches this plugin and extra the extra path
                //                if (isset($extra_backup_locations['plugins'][$plugin_name]) && !empty($extra_backup_locations['plugins'][$plugin_name]))
                //                {
                //                    //Loop over the extra locations and add them to the list in case there are more than 1 extra location
                //                    foreach ($extra_backup_locations['plugins'][$plugin_name] as $p_extra_filename => $p_extra_file_location){
                //                        $locations[$p_extra_filename] = $p_extra_file_location;
                //                    }
                //                }

                //Add the expected & extra locations into the final array to be returned
                $plugin_names[$plugin_name] = array('type' => 'file', 'location' => $locations);

//                $plugin_names[$plugin_name] = array('type' => 'file', 'location' => $settings['configDirectory'] . "/" . $fname);

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
function pruneOrRemoveAgedBackupFiles()
{
    global $fpp_backup_max_age, $fpp_backup_min_number_kept, $fpp_backup_max_plugin_backups_key, $fpp_backup_location, $backups_verbose_logging;

    //gets all the files in the configs/backup directory via the API now since it will look in multiple areas
    $config_dir_files = file_get_contents('http://localhost/api/backups/configuration/list');
    $config_dir_files = json_decode($config_dir_files, true);
    $config_dir_files_tmp = $config_dir_files;
    $backups_to_delete = array();
    $num_backups_deleted = 0;

    //If the number of backup files that exist IS LESS than what the minimum we want to keep, return and stop processing
    if (count($config_dir_files) < $fpp_backup_min_number_kept) {
        $aged_backup_removal_message = "SETTINGS BACKUP: NOT removing JSON Settings backup files. Since there are (" . count($config_dir_files) . ") backups available and this is less than the minimum backups we want to keep ($fpp_backup_min_number_kept) \r\n";
        error_log($aged_backup_removal_message);
        return;
    }

    //Reverse the results so we get oldest to newest, the API by default returns results with newest first
//    $config_dir_files = array_reverse($config_dir_files);

    //Select the backups that lie outside what we want to keep, because the array is a list of backups with the newest first then we'll be selecting older/dated backups
    $plugin_backups_to_delete_tmp = array();

    //Look through all the backups, select ones with a trigger source, because the backups are newest first, we'll be selecting the file at the position of our max limit through to the end of the list
    $backups_by_trigger_source = array();
    foreach ($config_dir_files as $backups_by_trigger_source_index => $backups_by_trigger_source_meta_data) {
        $backup_filename = $backups_by_trigger_source_meta_data['backup_filename'];

        if ((stripos(strtolower($backup_filename), "-backup_") !== false) && (stripos(strtolower($backup_filename), ".json") !== false)) {
            //
            if (array_key_exists('backup_trigger_source', $backups_by_trigger_source_meta_data) && $backups_by_trigger_source_meta_data['backup_trigger_source'] !== null) {
                //Get the trigger source name & add it and the backup meta data to a temp array, this will group like sources together
                //then we'll limit those lists to enforce our backup limit
                $backup_trigger_source = $backups_by_trigger_source_meta_data['backup_trigger_source'];
                $backups_by_trigger_source[$backup_trigger_source][] = $backups_by_trigger_source_meta_data;

                //Check the number of items for this trigger source, if it's > than our allowed limit then add the backup file to the $backups_to_delete list
                if (count($backups_by_trigger_source[$backup_trigger_source]) > $fpp_backup_max_plugin_backups_key) {
                    $plugin_backups_to_delete_tmp[] = $backups_by_trigger_source_meta_data;
                    //Remove the entry from the list of backup files, because it will be deleted (we want this list to reflect the list after enforcing plugin backup limits)
                    //so that we can enforce the global limit after
                    unset($config_dir_files_tmp[$backups_by_trigger_source_index]);
                }
            }
        }
    }

    //Fix indexes
    $config_dir_files_tmp = array_values($config_dir_files_tmp);

    if (count($config_dir_files_tmp) > $fpp_backup_min_number_kept) {
        $backups_to_delete = array_slice($config_dir_files_tmp, $fpp_backup_min_number_kept);
    }

    //tack on the plugin backups to be deleted
    foreach ($plugin_backups_to_delete_tmp as $plugin_backups_to_delete_tmp_data) {
        $backups_to_delete[] = $plugin_backups_to_delete_tmp_data;
    }

    //loop over the backup files we've found that are beyond our set limit, all these to be deleted
    foreach ($backups_to_delete as $backup_file_index => $backup_file_meta_data) {
        $backup_filename = $backup_file_meta_data['backup_filename'];

        if ((stripos(strtolower($backup_filename), "-backup_") !== false) && (stripos(strtolower($backup_filename), ".json") !== false)) {
            //File is one of our backup files, this check is just in case there is other json files in this directory placed there by the user which we want to avoid touching.

            //reconstruct the path
            $backup_file_location = $backup_file_meta_data['backup_filedirectory'];
            $backup_file_path = $backup_file_location . $backup_filename;
            //Backup Directory
            $backup_directory = !$backup_file_meta_data['backup_alternative_location'] ? 'JsonBackups' : 'JsonBackupsAlternate';
            //Collect the backup file
            $backup_time = $backup_file_meta_data['backup_time_unix'];
            //backup max_age time in seconds, since we are dealing with UNIX epoc time
            $backup_max_age_seconds = ($fpp_backup_max_age * 86400);

            //Remove the file via API instead
            //Build the URL
            $url = 'http://localhost/api/backups/configuration/' . $backup_directory . '/' . $backup_filename;
            //options for the request
            $options = array(
                'http' => array(
                    'header' => "Content-type: text/plain",
                    'method' => 'DELETE',
                ),
            );
            $context = stream_context_create($options);

            //Make the call to the API to delete the backup file
            $backup_deleted_call = file_get_contents($url, false, $context);
            $backup_deleted_call_decoded = json_decode($backup_deleted_call, true);

            if ($backup_deleted_call !== false && $backup_deleted_call_decoded['Status'] == 'OK') {
                //Track how many were deleted
                $num_backups_deleted++;
            } else {
                $aged_backup_removal_message = "SETTINGS BACKUP: Something went wrong pruning old JSON settings backup file ($backup_file_path). Error: " . $backup_deleted_call;
                error_log($aged_backup_removal_message . PHP_EOL);
            }
        }
    }

    //Note how many were deleted
    if ($num_backups_deleted > 0) {
        //Note what happened in the logs also
        $aged_backup_removal_message = "SETTINGS BACKUP: Removed ($num_backups_deleted) old JSON settings backup files, because we only want to keep the $fpp_backup_min_number_kept most recent backups";
        error_log($aged_backup_removal_message . PHP_EOL);
    }
}

/**
 * Prevent or stop the users browser from showing a download prompt for the backup file
 * Useful for if we just want to trigger a backup and just have it saved locally instead of both saving locally and sending it to the users browser
 */
function preventPromptUserBrowserDownloadBackup()
{
    global $fpp_backup_prompt_download;
    //
    $fpp_backup_prompt_download = false;
}

/**
 * Extracts the FPP Major version
 * @return array|mixed|string|string[]
 */
function collectFppMajorVersion()
{
    //First try get the FPP version and break it down so we can get the major version
    $fpp_version = explode(".", getFPPVersion());
    $fpp_major_version = 0;

    //If explode worked correctly, the major version will be at indec 0
    if (array_key_exists(0, $fpp_version)) {
        $fpp_major_version = $fpp_version[0];
    } else {
        //Else something went wrong, try get the FPP version from the GIT Branch
        //replace the v in e.g v6.3 so we just have the digit
        $fpp_git_branch = str_replace("v", "", explode(".", getFPPBranch()));
        $fpp_major_version = $fpp_git_branch[0];
    }

    return $fpp_major_version;
}

/**
 * Set a new defined max backup age for the existing JSON settings backup files
 * @param $new_max_age
 */
function setBackupMaxAge($new_max_age)
{
    global $fpp_backup_max_age;
    //
    if (!empty($new_max_age) && ($new_max_age != 0)) {
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
    if (!empty($new_backup_count) && ($new_backup_count != 0)) {
        $fpp_backup_min_number_kept = $new_backup_count;
    }
}

function is_array_empty($InputVariable)
{
    $Result = true;

    if (is_array($InputVariable) && count($InputVariable) > 0) {
        foreach ($InputVariable as $Value) {
            $Result = $Result && is_array_empty($Value);
        }
    } else {
        $Result = empty($InputVariable);
    }

    return $Result;
}

//Move backup files
moveBackupFiles_ToBackupDirectory();

?>
<?php
//See if we should skip spitting out the HTML code below
if ($skipHTMLCodeOutput === false) {
    ?>

<!DOCTYPE html>
<html lang="en">
<head>
    <?php require_once 'common/menuHead.inc';?>
    <title><?=$pageTitle?></title>
    <!--    <script>var helpPage = "help/backup.php";</script>-->
    <meta http-equiv="Content-Type" content="text/html; charset=utf-8" />

	<?php
$backupHosts = getKnownFPPSystems();
    ?>
    <script type="text/javascript">
        var settings = new Array();
        var list_of_existing_backups;
        var debug_mode = <?echo $backups_verbose_logging; ?>
		<?php
////Override restartFlag setting not reflecting actual value after restoring, just read what's in the settings file
    $settings['restartFlag'] = ReadSettingFromFile('restartFlag');
    $settings['rebootFlag'] = ReadSettingFromFile('rebootFlag');

    foreach ($settings as $key => $value) {
        if (!is_array($value)) {
            if (preg_match('/\n/', $value)) {
                continue;
            }

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
    if (document.getElementById("backup.EEPROM").checked) {
        flags += " EEPROM";
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
    var path = document.getElementById("backup.Path").value.replaceAll('\\', '/');
    var pathSelect = document.getElementById("backup.PathSelect").value;
    var remoteStorage = document.getElementById("backup.RemoteStorage").value;
    var host = document.getElementById("backup.Host").value;
    var direction = document.getElementById("backup.Direction").value;
    var flags = GetCopyFlags();

    var url = "copystorage.php?wrapped=1&direction=" + direction;

    // Substite back in case we changed \ to /
    document.getElementById("backup.Path").value = path;

    if ((direction == 'TOUSB') ||
        (direction == 'FROMUSB')) {
        storageLocation = document.getElementById("backup.USBDevice").value;
    } else if ((direction == 'TOREMOTE') ||
               (direction == 'FROMREMOTE')) {
        if (host == '') {
            DialogError('Copy Failed', 'No host specified');
            return;
        }

        //Add the remote storage value is restoring to or from remote hosts
        if (typeof (remoteStorage) !== 'undefined' || remoteStorage !== '') {
            url += '&remoteStorage=' + remoteStorage;
        }

        storageLocation = host;
    } else {
        storageLocation = settings['mediaDirectory'] + "/backups";
    }

    //Add in the specified backup path
    if (direction.substring(0,4) == 'FROM') {
        if (pathSelect == '') {
            DialogError('Copy Failed', 'No path specified');
            return;
        }

        url += '&path=' + pathSelect;
    } else {
        if (path == '') {
            document.getElementById("backup.Path").value = '/';
            path = '/';
            SetSetting('backup.Path', '/', 0, 0, false);
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

    if (document.getElementById("backup.sendCompressed").checked) {
        url += '&compress=yes';
    } else {
        url += '&compress=no';
    }

    if (direction.substring(0,4) == 'FROM')
    {
        if (!confirm(warningMsg)) {
            $.jGrowl("Restore canceled.",{themeState:'success'});
            return;
        }
    }

    var title_txt = (direction.substring(0,4) == 'FROM') ? "FPP File Copy Restore" : "FPP File Copy Backup";

    DoModalDialog({
        id: "copyPopup_Modal",
        title: title_txt,
        height: 600,
        width: 900,
        autoResize: true,
        closeOnEscape: false,
        backdrop: true,
        body:  $('#copyPopup').html(),
        class: "no-close",
        buttons: {

        }
    });

    // $('#copyPopup').fppDialog({ height: 600, width: 900, title: title, dialogClass: 'no-close' });
    // $('#copyPopup').fppDialog( "moveToTop" );
    $('#copyText').val('');

    StreamURL(url, 'copyText', 'CopyDone', 'CopyTimeoutError');

}

function CloseCopyDialog() {
    var cd_remoteHost = settings['backup.Host'];
    var cd_remoteStorage = settings['backup.RemoteStorage'];

    if (typeof (cd_remoteHost) !== "undefined" && typeof (cd_remoteStorage) !== "undefined"){
        //Run a Unmount against the remote storage to make sure it's unmounted, this helps when there is a issue with the copy_settings_to_storeage.sh script and it doesn't unmount the specified device at the remote host
        //We don't do anything with the returned output
        $.post("http://" + cd_remoteHost + "/api/backups/devices/unmount/" + cd_remoteStorage + "/remote_filecopy");

    }

    CloseModalDialog("copyPopup_Modal");
}

function CopyDone() {
    $('#closeDialogButton').show();
}

function CopyTimeoutError() {
    var fpp_backup_filecopy_log_url = "api/file/Logs/fpp_backup_filecopy.log";
    var timeoutErrorMessage = "!!! Attempting to track file copy process via it's fallback log file... \n" +
        " The file copy is still running in the background and will complete in due course. Progress updates will appear periodically. !!! \n\n ";
    var noNewDataErrorMessage = "";
    var iterations = 0;
    var iterationsWithNoDataWarningsIssued = 1;
    var noNewDataIterationCount = 600; // The interval is every second so 600 iterations = 10minutes
    // var noNewDataIterationHardLimit = 900; // 15 minutes - Consider the process failed if no new log data received in 15 minutes
    var last_response_len = 0;

    //cache the reference to the element
    var outputArea = $('#copyText');
    outputArea.val('')
    outputArea.val(timeoutErrorMessage);

    //Every second read the alternative fpp_backup_filecopy.log
    var tailLogInterval = setInterval(function () {

            $.get(fpp_backup_filecopy_log_url, function (text) {
                //This is also a nasty workaround, but we reply on logs api will returning a file not found error to signify the copy process ending
                //as the log file is deleted at the end of that process
                if (text === "File does not exist.") {
                    //The file copy script has competed and removed the logfile
                    //If the log file cannot be found anymore consider the process complete
                    clearInterval(tailLogInterval);
                    CopyDone();
                } else {
                    //Track the number of interactions the response remained unchanged
                    if (last_response_len === 0) {
                        last_response_len = text.length;
                    }

                    if (last_response_len === text.length) {
                        iterations += 1;
                    } else {
                        noNewDataErrorMessage = "";
                        //Reset
                        iterations = 0;
                        //Store the new data length
                        last_response_len = text.length;
                    }

                    //Check if it's been more than 10 minutes that the data has remained unchanged
                    if (iterations === (noNewDataIterationCount * iterationsWithNoDataWarningsIssued)) {
                        iterationsWithNoDataWarningsIssued += 1;
                        //Some error occurred
                        noNewDataErrorMessage = "!!! WARNING: No new log entries has been received in over " + Math.floor(iterations / 60) + " minutes, still waiting.... !!! \n\n";
                    }
                    // if (iterations >= noNewDataIterationHardLimit) {
                    //     //Some error occurred
                    //     noNewDataErrorMessage = "!!! ERROR: No new log entries has been received in over " + noNewDataIterationHardLimit + " seconds - File Copy Backup process has likely failed !!! \n\n";
                    // }

                    //This is a bit ugly, but put our error message first (just to inform the user), then the contents of the log file every time
                    outputArea.val(timeoutErrorMessage + text + noNewDataErrorMessage);

                    outputArea.scrollTop(outputArea.prop('scrollHeight'));
                    outputArea.parent().scrollTop(outputArea.parent().prop('scrollHeight'));

                    //Check for device unmounted text - if this exists then the process has completed... the file should be removed at the end
                    //but this is just a failsafe in case it wasn't
                    //exit if we hit the hard limit
                    if (text.includes("unmounted from") || text.length === 0) {
                        clearInterval(tailLogInterval);
                        CopyDone();
                    }
                }
            });
        },
        1000);
}


function GetBackupDevices() {
    $('#backup\\.USBDevice').html('<option>Loading...</option>');
    //Add a loading spinner to show something is happening on the JSON Backup page dropdown list
    $('#jsonConfigbackup\\.USBDevice').parent().closest('div').addClass('fpp-backup-action-loading');
    //Also do the same for the file copy list these both use the same functions and deal with the same data
    //Add a loading spinner to show something is happening
    $('#backup\\.USBDevice').parent().closest('td').addClass('fpp-backup-action-loading');

    $.get("api/backups/devices"
        ).done(function(data) {
            var options = "";
            var default_none_selected_option = "<option value='none' selected>None</option>";

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
            $('#jsonConfigbackup\\.USBDevice').html(default_none_selected_option + options);

            //Remove the loading spinner
            $('#jsonConfigbackup\\.USBDevice').parent().closest('div').removeClass('fpp-backup-action-loading');
            //Also do the same for the file copy list these both use the same functions and deal with the same data
            //Add a loading spinner to show something is happening
            $('#backup\\.USBDevice').parent().closest('td').removeClass('fpp-backup-action-loading');

            if (options != "") {
                    if (document.getElementById("backup.Direction").value == 'FROMUSB')
                        GetBackupDeviceDirectories();
                    else if (document.getElementById("backup.Direction").value == 'TOUSB')
                        GetRestoreDeviceDirectories();

                    //Get the selected device setting and update the UI
                    GetJSONConfigBackupDevice();
                }
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

function GetRestoreDeviceDirectories() {
    var dev = document.getElementById("backup.USBDevice").value;

    if (dev == '') {
        $('#backup\\.PathSelect').html("<option value=''>No USB Device Selected</option>");
        return;
    }

    $('#usbDirectories').html('');
    $.get("api/backups/list/" + dev
        ).done(function(data) {
            var options = '';
            for (i = 0; i < data.length; i++) {
                if (data[i].substring(0,5) != 'ERROR')
                    options += "<option value='" + data[i] + "'>" + data[i] + "</option>";
            }
            $('#usbDirectories').html(options);
        });
}

function USBDeviceChanged(toFromRemote=false) {
    var direction = document.getElementById("backup.Direction").value;
    if (debug_mode == true) {
        alert('direction: ' + direction);
    }
    if (direction == 'FROMUSB')
        GetBackupDeviceDirectories();
    else if (direction == 'TOUSB')
        GetRestoreDeviceDirectories();
}

function JSONConfigBackupUSBDeviceChanged() {
    var storage_location = document.getElementById("jsonConfigbackup.USBDevice").value;

    //Add a loading spinner to indicate the setting is being saved
    $('#jsonConfigbackup\\.USBDevice').parent().closest('div').addClass('fpp-backup-action-loading');

    //Write setting to system
    $.ajax({
        url: 'api/settings/jsonConfigBackupUSBLocation',
        type: 'PUT',
        data: storage_location,
        success: function(data){
            $('#jsonConfigbackup\\.USBDevice').parent().closest('div').removeClass('fpp-backup-action-loading');

            if (storage_location !== "none"){
                $.jGrowl('JSON Configuration Backups will now be copied to: ' + storage_location, {themeState: 'success'});

                //Pop a dialog and let the user choose if they want to copy backups to USB
                //Only copy existing backup files to the selected storage if something  other than no device has been selected
                //so for example only do the initial  copy if going from none -> sda1
                DoModalDialog({
                    id: "dialog_copyToUsb_Modal",
                    title: "Copy Backups to USB",
                    width: 400,
                    autoResize: true,
                    closeOnEscape: false,
                    backdrop: true,
                    body: $('#dialog_copyToUsb').html(),
                    class: "modal-dialog-scrollable",
                    buttons: {
                        "Yes Copy To USB": {
                            id: "dialog_copyToUsb_DoCopy",
                            click: function () {
                                CopyBackupsToUSBHelper();
                                CloseModalDialog("dialog_copyToUsb_Modal");
                            }
                        },
                        "Not Right Now": {
                            id: "dialog_copyToUsb_NoCopy",
                            click: function () {
                                CloseModalDialog("dialog_copyToUsb_Modal");
                            }
                        }
                    }
                });
            }else{
                $.jGrowl('JSON Configuration Backups will no longer be copied to an additional storage device: ', {themeState: 'detract'});
            }

            //Reload the list of JSON backups on the device, it will now show backups on the selected device
            GetJSONConfigBackupList();
        },
        error: function(data) {
            $('#jsonConfigbackup\\.USBDevice').parent().closest('div').removeClass('fpp-backup-action-loading');

            DialogError('JSON Configuration Backup Storage Location', 'Failed to set additional storage location.');

            //Reload the list of JSON backups on the device
            GetJSONConfigBackupList();
        }
    });
}

function CopyBackupsToUSBHelper(){
    //Get the backup path from the File Copy Backup page
    var selected_jsonConfigBackupUSBLocation = $('#jsonConfigbackup\\.USBDevice').val()

    //Generate the URL
    var url = 'copystorage.php?wrapped=1&direction=TOUSB&path=' + 'Automatic_Backups' + '&storageLocation=' + selected_jsonConfigBackupUSBLocation + '&flags=JsonBackups&delete=no';

    $.ajax({
        url: url,
        type: 'GET',
        success: function(data){
            $.jGrowl('JSON Configuration Backups copied to: ' + selected_jsonConfigBackupUSBLocation, {themeState: 'success'});
        },
        error: function(data) {
            //do nothing
         }
    });
}

function GetJSONConfigBackupDevice() {
    //Add a the loading spinner to show something is happening
    $('#jsonConfigbackup\\.USBDevice').parent().closest('div').addClass('fpp-backup-action-loading');

    $.ajax({
        url: 'api/settings/jsonConfigBackupUSBLocation',
        type: 'GET',
        success: function(data){
            if (typeof (data.value) !== "" || typeof (data.value) !== "undefined"){
                //Change the JSON Config backup location to the one set by the user if a valid value is set
                $('#jsonConfigbackup\\.USBDevice option[value="'+data.value+'"]').attr('selected', true);
                //
                $('#jsonConfigbackup\\.USBDevice').parent().closest('div').removeClass('fpp-backup-action-loading');
            }
        },
        error: function(data) {
            //do nothing
            DialogError('JSON Configuration Backup Storage Location', 'Failed to read additional storage location.');

            $('#jsonConfigbackup\\.USBDevice').parent().closest('div').removeClass('fpp-backup-action-loading');
        }
    });
}

function GetJSONConfigBackupList() {
    $.ajax({
           url: 'api/backups/configuration/list',
           type: 'GET',
           success: function (data) {
               //Store the list of backups found so we can reuse this for the restore panel which is the exact same but diffrent actions
               list_of_existing_backups = data;

               //Add a placeholder saying we're loading data
               $('#table-download-existing-backups-content').html('<tr><td></td><td>Loading Existing Backups....</td><td></td></tr>');

               if (typeof (data) !== "undefined") {
                   //Clear the table again
                   $('#table-download-existing-backups-content').html('');
                   //Loop over the results and build the rows
                   $.each(data, function (backup_filename_id, backup_filename_meta) {
                       //
                       //Path and Directory
                       var backup_filename = backup_filename_meta.backup_filename;
                       var backup_filedirectory = backup_filename_meta.backup_filedirectory;
                       //Comment and time
                       var backup_comment = backup_filename_meta.backup_comment;
                       var backup_time = backup_filename_meta.backup_time;
                       var backup_is_in_location =  backup_filename_meta.backup_alternative_location;

                       //By default
                       var location_icon = '<span><i class="fas fa-sd-card"></i></span>';
                       //
                       //Build the onclick command for downloading the backuo
                       var onclick_download_command = "DownloadJsonBackupFile('JsonBackups', ['" + backup_filename + "']);";
                       //Build the onclick command for deleting the backup
                       var onclick_delete_command = "DeleteJsonBackupFile('JsonBackups', '." + backup_filename_id + "', ['" + backup_filename + "']);";

                       if (backup_is_in_location === true) {
                           location_icon = '<span><i class="fas fa-hdd"></i></span>';

                           onclick_download_command = "DownloadJsonBackupFile('JsonBackupsAlternate', ['" + backup_filename + "']);";
                           onclick_delete_command = "DeleteJsonBackupFile('JsonBackupsAlternate', '." + backup_filename_id + "', ['" + backup_filename + "']);";
                       }

                       var backup_file_data_row = '<tr id="backup-file-id" class="' + backup_filename_id + '" > ' +
                           '<td  id="backup-file-date">' + location_icon + ' ' + backup_filename_meta.backup_time + '</td>' +
                           '<td  id="backup-file-configuration-change">' + backup_filename_meta.backup_comment + '</td>' +
                           '<td id="backup-file-configuration-actions">' +

                            //Button to Download the Backup
                           '<button name="btnDownloadConfig_' + backup_filename_id + '" type="button" class="buttons downloadConfigButton" value="Download Backup" onClick="' + onclick_download_command + '"> ' +
                           '<i class="fas fa-fw fa-download"></i> ' +
                           '</button>' +

                           //Button to delete the backup
                           '<button name="btnDeleteConfig_' + backup_filename_id + '" type="button" class="buttons deleteConfigButton" value="Delete Backup" onclick="' + onclick_delete_command + '"> ' +
                           '<i class="fas fa-fw fa-trash"></i> ' +
                           '</button>' +

                           '</td>' +
                           '</tr>'

                       //Append each row to the table content
                       $('#table-download-existing-backups-content').append(backup_file_data_row);
                   });
                   //Once were done with the backup panel, populate the restore list
                   GetJSONConfigRestoreList();

                   $('table').floatThead('reflow');
               }
        },
        error: function (data) {
            var backup_file_data_row = '<tr id="backup-file-id" class="" > ' +
                '<td  id="backup-file-date"> </td>' +
                '<td  id="backup-file-configuration-change">Error Occured Retreiving Existing Backup List</td>' +
                '<td id="backup-file-configuration-actions"> </td>' +
                '</tr>'

            //Add a row saying there was a error
            $('#table-download-existing-backups-content').html('');
            $('#table-download-existing-backups-content').append(backup_file_data_row);
        }
    });
}

function  GetJSONConfigRestoreList(){
    var data = list_of_existing_backups;

    //Add a placeholder saying we're loading data
    $('#table-restore-existing-backups-content').html('<tr><td></td><td>Loading Existing Backups....</td><td></td></tr>');

    if (typeof (data) !== "undefined") {
        //Clear the table again
        $('#table-restore-existing-backups-content').html('');
        //Loop over the results and build the rows
        $.each(data, function (backup_filename_id, backup_filename_meta) {
            //Path and Directory
            var backup_filename = backup_filename_meta.backup_filename;
            var backup_filedirectory = backup_filename_meta.backup_filedirectory;
            //Comment and time
            var backup_comment = backup_filename_meta.backup_comment;
            var backup_time = backup_filename_meta.backup_time;
            var backup_is_in_location = backup_filename_meta.backup_alternative_location
            ;
            //By default
            var location_icon = '<span><i class="fas fa-sd-card"></i></span>';
            //
            var onclick_restore_command = "RestoreJsonBackup('JsonBackups', ['" + backup_filename + "'], '." + backup_filename_id + "');";

            //Build the onclick command for restoreing the backuo
            if (backup_is_in_location === true) {
                location_icon = '<span><i class="fas fa-hdd"></i></span>';
                onclick_restore_command = "RestoreJsonBackup('JsonBackupsAlternate', ['" + backup_filename + "'], '." + backup_filename_id + "');";
            }
            //Build the onclick command for deleting the backup
            // var onclick_delete_command = "DeleteFile('JsonBackups', '." + backup_filename_id + "', ['" + backup_filename + "']);"

            var backup_file_data_row = '<tr id="backup-file-id" class="' + backup_filename_id + '" > ' +
                '<td  id="backup-file-date">' + location_icon + ' ' + backup_filename_meta.backup_time + '</td>' +
                '<td  id="backup-file-configuration-change">' + backup_filename_meta.backup_comment + '</td>' +
                '<td id="backup-file-configuration-actions">' +

                //Button to restore the Backup
                '<button name="btnrestoreConfig_' + backup_filename_id + '" type="button" class="buttons restoreJsonConfigActionButton" value="Restore Backup" onClick="' + onclick_restore_command + '"> ' +
                '<i class="fas fa-fw fa-undo"></i> ' +
                '</button>' +

                // //Button to delete the backup
                // '<button name="btnDeleteConfig_' + backup_filename_id + '" type="button" class="buttons" value="Delete Backup" onclick="' + onclick_delete_command + '"> ' +
                // '<i class="fas fa-fw fa-trash"></i> ' +
                // '</button>' +

                '</td>' +
                '</tr>'

            //Append each row to the table content
            $('#table-restore-existing-backups-content').append(backup_file_data_row);
        });
    } else {
        var backup_file_data_row = '<tr id="backup-file-id" class="" > ' +
            '<td  id="backup-file-date"> </td>' +
            '<td  id="backup-file-configuration-change">Error Occured Retreiving Existing Backup List</td>' +
            '<td  id="backup-file-configuration-actions"> </td>' +
            '</tr>'

        //Add a row saying there was a error
        $('#table-restore-existing-backups-content').html('');
        $('#table-restore-existing-backups-content').append(backup_file_data_row);

    }
}

function DownloadJsonBackupFile(dir, files) {
    //Clone of GetFiles() but with a different URL and just dealing with a single file
    if (files.length == 1) {
        location.href = "api/backups/configuration/" + dir + "/" + encodeURIComponent(files[0].replace(/\//g, '_'));
    }
}

function DeleteJsonBackupFile(dir, row, file, silent = false) {
    //Clone of DeleteFiles with a different URL
    if (file.indexOf("/") > -1) {
        alert("You can not delete this file.");
        return;
    }

    //Add a loading  spinner to the delete button for the row we're deleting to add some feedback to the user something is happening
    $( row + ' .deleteConfigButton > i').addClass('fpp-backup-action-loading');

    $.ajax({
        url: "api/backups/configuration/" + dir + "/" + encodeURIComponent(file),
        type: 'DELETE'
    }).done(function (data) {
        if (data.Status == "OK") {
            $(row).remove();
        } else {
            if (!silent)
                DialogError("ERROR", "Error deleting file \"" + file + "\": " + data.Status);
            }
    }).fail(function () {
        if (!silent)
            DialogError("ERROR", "Error deleting file: " + file);
    });
}

function RestoreJsonBackup(directory, filename, row){
    //Get the selected backup area in case teh user has selected a specific area to restore
    var selected_restore_area = $('#restorearea').val();

    //validate directory and filename are not emptry
    if (typeof (selected_restore_area) && (typeof (directory) !== 'undefined' && typeof (filename) !== 'undefined')) {
        //Add a loading  spinner to the delete button for the row we're deleting to add some feedback to the user something is happening
        $( row + ' .restoreJsonConfigActionButton > i').addClass('fpp-backup-action-loading');

        //all the API backend to do the restore
        $.ajax({
            url: 'api/backups/configuration/restore/' + directory + '/' + filename,
            type: 'POST',
            data: selected_restore_area,
            processData: false,
            success: function (data) {
                //Remove the loading spinner
                $( row + ' .restoreJsonConfigActionButton > i').removeClass('fpp-backup-action-loading');

                if (data.Success === true) {
                    $.jGrowl('Successfully restored selected backup: ', {themeState: 'success'});
                } else {
                    $.jGrowl('Error occurred restoring selected backup: ', {themeState: 'danger'});
                }
            },
            error: function (data) {
                //Remove the loading spinner also if we fail
                $( row + ' .restoreJsonConfigActionButton > i').removeClass('fpp-backup-action-loading');

                DialogError('Error occurred attempting to restore data', data.Message);
            }
        });
    }


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

function GetBackupDirsViaAPI(host, remoteStorageSelected = '') {
    $('#backup\\.PathSelect').html('<option>Loading...</option>');

    //Build a different URL if a storage device has been specified
    var url = "http://" + host + "/api/backups/list";
    if (remoteStorageSelected !== '' || remoteStorageSelected !== false) {
        url = "http://" + host + "/api/backups/list/" + remoteStorageSelected;
    }

    //Add a loading spinner to show the user something is happening
    $('#backup\\.PathSelect').parent().closest('td').addClass('fpp-backup-action-loading');

    $.get(url
    ).done(function (data) {
        PopulateBackupDirs(data);
        $('#backup\\.PathSelect').parent().closest('td').removeClass('fpp-backup-action-loading');
    }).fail(function () {
        $('#backup\\.PathSelect').html('');
        $('#backup\\.PathSelect').parent().closest('td').removeClass('fpp-backup-action-loading');
    });
}

//Wrapper for GetBackupDirsViaAPI when quering folders on a remote hosts storage device
function GetBackupHostBackupDirs(remoteStorageSelected = false) {
    if (remoteStorageSelected !== false) {
        //Get the value of the selected storage
        var selectedStorage = remoteStorageSelected;
        //ignore if the default of none is still selected
        if (selectedStorage == 'none') {
            selectedStorage = '';
        }
    }

    //Get the backup directories on the specified storage device
    GetBackupDirsViaAPI($('#backup\\.Host').val(), selectedStorage);
}

function BackupDirectionChanged() {
    var direction = document.getElementById("backup.Direction").value;
    var host = document.getElementById("backup.Host").value;

    switch (direction) {
        case 'TOUSB':
            $('.copyUSB').show();
            $('.copyPath').show();
            $('.copyPathSelect').hide();
            $('.copyHost').hide();
            $('.copyHostDevice').hide();
            $('.copyBackups').show();
            GetRestoreDeviceDirectories();
            break;
        case 'FROMUSB':
            $('.copyUSB').show();
            $('.copyPath').hide();
            $('.copyPathSelect').show();
            $('.copyHost').hide();
            $('.copyHostDevice').hide();
            $('.copyBackups').hide();
            GetBackupDeviceDirectories();
            break;
        case 'TOLOCAL':
            $('.copyUSB').hide();
            $('.copyPath').show();
            $('.copyPathSelect').hide();
            $('.copyHost').hide();
            $('.copyHostDevice').hide();
            $('.copyBackups').hide();
            break;
        case 'FROMLOCAL':
            $('.copyUSB').hide();
            $('.copyPath').hide();
            $('.copyPathSelect').show();
            $('.copyHost').hide();
            $('.copyHostDevice').hide();
            $('.copyBackups').hide();
            GetBackupDirsViaAPI('<?php echo $_SERVER['HTTP_HOST'] ?>');
            break;
        case 'TOREMOTE':
            $('.copyUSB').hide();
            $('.copyPath').show();
            $('.copyPathSelect').hide();
            $('.copyHost').show();
            $('.copyHostDevice').show();
            $('.copyBackups').hide();
            $('.sendCompressed').show();
            //Check if remote has rsynd enabled
            CheckRemoteHasRsyncdEnabled(host);
            //USB Device on remote
            GetRemoteHostUSBStorage();
            break;
        case 'FROMREMOTE':
            $('.copyUSB').hide();
            $('.copyPath').hide();
            $('.copyPathSelect').show();
            $('.copyHost').show();
            $('.copyHostDevice').show();
            $('.copyBackups').hide();
            $('.sendCompressed').show();
            GetBackupHostBackupDirs();
            //Check if remote has rsynd enabled
            CheckRemoteHasRsyncdEnabled(host);
            //USB device on remote
            GetRemoteHostUSBStorage();
            break;
    }
}

 function CheckRemoteHasRsyncdEnabled(host) {
    //Read the the setting value for whether the Rsync Daemon is enabled on the host
    //if it isn't prompt the user that this needs to be enabled before copy TO or FROM the remote host can happen
    $.ajax({
        url: 'api/settings/Service_rsync',
        type: 'GET',
        success: function (data) {
            if (typeof (data) !== "undefined") {
                var rsyncd_setting_value = data['value'];
                if (rsyncd_setting_value === 0 || rsyncd_setting_value === '0') {
                    //remove the warning text in case it's hanging around from a previous host
                    $('.host_rsyncd_warning').remove();
                    //Generate a URL by IP address to the affected host so the user can navigate easier
                    var hostHref = "<a href='http://" + host + "/settings.php#settings-system' target='_blank'>" + host + "</a>";
                    //rsynd is disabled on host, add a warning next to the selected host
                    $('#backup\\.Host').after('<span class="host_rsyncd_warning"><b>WARNING!</b> Rsync server is disabled on remote host, please enable rsync under FPP Settings -> System -> OS Services on ' + hostHref + ' </span');
                    //<span><b>WARNING!</b> Rsync server is disabled on this host, please enable under FPP Settings -> System -> OS Services on *host*</span
                }else{
                    //remove the warning text in case it's hanging around from a previous host
                    $('.host_rsyncd_warning').remove();
                }
            }else{
                //something went wrong
                $.jGrowl('Error occurred reading the Service_rsync value from host - ' + host, {themeState: 'danger'});
            }
        },
        error: function (data) {
            //something went wrong
            $.jGrowl('Error occurred reading the Service_rsync value from host - ' + host, {themeState: 'danger'});
        }
    });
}

//Retrieves a list of available backup devices on the selected remote host
function GetRemoteHostUSBStorage() {
    //Very similar to GetBackupDevices() but adapted to collect storage info from a remote system
    var host = document.getElementById("backup.Host").value;
    var requestUrl = "http://" + host + "/" + "api/backups/devices";
    var default_none_selected_option = "<option value='none' selected>Default FPP Storage</option>";

    // $('#backup\\.USBDevice').html('<option>Loading...</option>');
    //Add a loading spinner to show something is happening
    $('#backup\\.RemoteStorage').parent().closest('td').addClass('fpp-backup-action-loading');

    $.get(requestUrl
    ).done(function (data) {
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
        //Populate the dropdown with the detected storage devices
        $('#backup\\.RemoteStorage').html(default_none_selected_option + options);

        //Remove the loading spinner
        $('#backup\\.RemoteStorage').parent().closest('td').removeClass('fpp-backup-action-loading');

        if (options !== "") {
            //After populating the list, Get the currently set remote host storage device
            GetBackupRemoteStorageDevice();
        }
    }).fail(function () {
        $('#backup\\.RemoteStorage').html(default_none_selected_option);
    });
}

//Save the selected remote backup storage device selection to settings
function backupRemoteStorageChanged() {
    //Get the value of the selected remove storage device
    var value = encodeURIComponent($('#backup\\.RemoteStorage').val());

    //Add the loading spinner
    $('#backup\\.RemoteStorage').parent().closest('td').addClass('fpp-backup-action-loading');

    //Write setting to system
    $.ajax({
        url: 'api/settings/backup.RemoteStorage/',
        type: 'PUT',
        data: value,
        success: function (data) {
            $('#backup\\.RemoteStorage').parent().closest('td').removeClass('fpp-backup-action-loading');

            $.jGrowl('Remote Backup Storage saved', {themeState: 'success'});
            settings['backup.RemoteStorage'] = value;

            //Get the directories on the selected storage
            GetBackupHostBackupDirs(encodeURIComponent(value));
        },
        error: function (data) {
            DialogError('Remote Backup Storage', 'Failed to save Backup Storage');
            //remove the spinner
            $('#backup\\.RemoteStorage').parent().closest('td').removeClass('fpp-backup-action-loading');
        }
    });
}

//Get the current setting for the remote hosts remote backup storage device (where the file copy will go to)
function GetBackupRemoteStorageDevice() {
    //Add a the loading spinner to show something is happening
    $('#backup\\.RemoteStorage').parent().closest('div').addClass('fpp-backup-action-loading');

    $.ajax({
        url: 'api/settings/backup.RemoteStorage',
        type: 'GET',
        success: function (data) {
            if (data.value !== "" || typeof (data.value) !== "undefined") {
                //Check if the chosen USB device/location exists in the dropdown list
                //The USB device dropdown list only lists devices which are available for use, so if the chosen device is not in the list
                //it's likely unavailable or still mounted (as such not available for use)
                var remote_storage_ddl_selector = $('#backup\\.RemoteStorage option[value="' + data.value + '"]');
                if (remote_storage_ddl_selector.length){
                    //Change the JSON Config backup location to the one set by the user if a valid value is set
                    remote_storage_ddl_selector.attr('selected', true);
                }else{
                    var host = document.getElementById("backup.Host").value;
                    $('#backup\\.RemoteStorage').parent().append("<span> <b>Warning:</b> " + data.value + " is not available. Check device is attached to host and that is not currently mounted. Check <a href='http://" + host + "/settings.php#settings-storage' target='_blank'>" + host + " - Mounted USB Devices</a></span>");
                }
                //
                $('#backup\\.RemoteStorage').parent().closest('div').removeClass('fpp-backup-action-loading');

                //Get the backup directories avaiable on the selected storage device if were restoring from remote,
                if (document.getElementById("backup.Direction").value == 'FROMREMOTE') {
                    //If no Remote Storage device is selected don't pass anything to this function telling it to look at another device
                    //so we just get what is on the default FPP storage device
                    var selectedStorage = encodeURIComponent($('#backup\\.RemoteStorage').val());

                    GetBackupHostBackupDirs(selectedStorage);
                }
            }
        },
        error: function (data) {
            //do nothing
            DialogError('Remote Host Backup Storage Location', 'Failed to read remote storage location.');

            $('#backup\\.RemoteStorage').parent().closest('div').removeClass('fpp-backup-action-loading');
        }
    });
}



function pageSpecific_PageLoad_DOM_Setup() {
    $('#backup\\.Path').attr('list', 'usbDirectories');
    GetBackupDevices();
    GetJSONConfigBackupList();
}

function pageSpecific_PageLoad_PostDOMLoad_ActionsSetup(){
    $('a[data-bs-toggle="pill"]').on('shown.bs.tab', function (e) {
        $('table').floatThead('reflow');
    });

        //float th thead on download existing backups table
        $('.table-download-existing-backups').floatThead({
                zIndex: 990,
                debug: true ,
                scrollContainer: function() {
                    return $('.table-download-existing-backups').closest(".table-download-existing-backups-container");
                }
        });


        //float th thead on restore existing backups table
        $('.table-restore-existing-backups').floatThead({
                zIndex: 990,
                debug: false ,
                scrollContainer: function() {
                    return $('.table-restore-existing-backups').closest(".table-restore-existing-backups-container");
                }
        });


}

        var activeTabNumber =
			<?php
if (isset($_GET['tab']) and is_numeric($_GET['tab'])) {
        print $_GET['tab'];
    } else {
        print "0";
    }

    ?>;
    </script>

    <style>
        .copyHost {
            display: none;
        }
        .copyPathSelect {
            display: none;
        }
        .copyHostDevice {
            display: none;
        }
        .sendCompressed{
            display: none;
        }
    </style>
</head>
<body>
<div id="bodyWrapper">
    <?php
$activeParentMenuItem = 'status';
    include 'menu.inc';?>
    <div class="mainContainer">
        <h1 class='title'>FPP Backups</h1>
        <div class="pageContent">
            <div class="fppTabs">
                <div id="fppBackups">
                    <ul id="fppBackupTabs" class="nav nav-pills pageContent-tabs" role="tablist">

                        <li class="nav-item">
                            <a class="nav-link active" id="backups-jsonBackup-tab" data-bs-toggle="tab" data-bs-target="#tab-jsonBackup" href="#tab-jsonBackup" role="tab" aria-controls="tab-jsonBackup" aria-selected="true">
                            JSON Configuration Backup
                            </a>
                        </li>
                        <li class="nav-item">
                            <a class="nav-link" id="backups-fileCopy-tab" data-bs-toggle="tab" data-bs-target="#tab-fileCopy" href="#tab-fileCopy" role="tab" aria-controls="tab-fileCopy" aria-selected="false">
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
                            <div id="rebootFlag" style="display: block;" class="callout callout-warning">Backup Restored, FPPD Restart or Reboot may be required.
                            </div>
                            <div id="restoreSuccessFlag" class="callout callout-primary">What was restored: <br>
                                <?php
foreach ($settings_restored as $area_restored => $success) {
            $success_str = "";
            if (is_array($success)) {
//                $success_area_data = false;
                $success_messages = "";

                //If the ATTEMPT and SUCCESS keys don't exist in the array, then try to process the internals which will be a sub areas and possibly have them.
                if (!array_key_exists('ATTEMPT', $success) && !array_key_exists('SUCCESS', $success) && !empty($success)) {
                    //process internal array for areas with sub areas
                    foreach ($success as $success_area_idx => $success_area_data) {
                        if (is_array($success_area_data) && array_key_exists('ATTEMPT', $success_area_data) && array_key_exists('SUCCESS', $success_area_data)) {
                            $success_area_attempt = $success_area_data['ATTEMPT'];
                            $success_area_success = $success_area_data['SUCCESS'];

                            if ($success_area_attempt == true && $success_area_success == true) {
                                $success_str = "Success";
                                $success_messages .= "<span class='callout callout-success' style='padding-top: 0.2em; padding-bottom: 0.2em;margin-top: 0.2em;margin-bottom: 0.2em;'>" . ucwords(str_replace("_", " ", $success_area_idx)) . " - " . "<b>" . $success_str . "</b>" . "</span><br/>";
                            } else {
                                $success_str = "Failed";
                                $success_messages .= "<span class='callout callout-danger' style='padding-top: 0.2em; padding-bottom: 0.2em;margin-top: 0.2em;margin-bottom: 0.2em;'>" . ucwords(str_replace("_", " ", $success_area_idx)) . " - " . "<b>" . $success_str . "</b>" . "</span><br/>";
                            }
                        }
                    }
                } //There is an ATTEMPT and SUCCESS key, check values of both
                else if (array_key_exists('ATTEMPT', $success) && array_key_exists('SUCCESS', $success)) {
                    $success_area_attempt = $success['ATTEMPT'];
                    $success_area_success = $success['SUCCESS'];

                    if ($success_area_attempt == true && $success_area_success == true) {
                        $success_str = "Success";
                        $success_messages .= "<span class='callout callout-success' style='padding-top: 0.2em; padding-bottom: 0.2em;margin-top: 0.2em;margin-bottom: 0.2em;'>" . ucwords(str_replace("_", " ", $area_restored)) . " - " . "<b>" . $success_str . "</b>" . "</span><br/>";
                    } else {
                        $success_str = "Failed";
                        $success_messages .= "<span class='callout callout-danger' style='padding-top: 0.2em; padding-bottom: 0.2em;margin-top: 0.2em;margin-bottom: 0.2em;'>" . ucwords(str_replace("_", " ", $area_restored)) . " - " . "<b>" . $success_str . "</b>" . "</span><br/>";
                    }

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
                    $success_messages = "<span class='callout callout-success' style='padding-top: 0.2em; padding-bottom: 0.2em;margin-top: 0.2em;margin-bottom: 0.2em;'>" . ucwords(str_replace("_", " ", $area_restored)) . " - " . "<b>" . $success_str . "</b>" . "</span><br/>";
                } else {
                    $success_str = "Failed";
                    $success_messages = "<span class='callout callout-danger' style='padding-top: 0.2em; padding-bottom: 0.2em;margin-top: 0.2em;margin-bottom: 0.2em;'>" . ucwords(str_replace("_", " ", $area_restored)) . " - " . "<b>" . $success_str . "</b>" . "</span><br/>";

                }
                echo $success_messages;
            }
        }
        //If network settings have been restored, print out the IP addresses that should come info effect
        if ($network_settings_restored) {
            //Print the IP addresses out
            foreach ($network_settings_restored_applied_ips as $idx => $network_ip_address) {
                if (!empty($network_ip_address)) {
                    echo ucwords(str_replace("_", " ", $idx)) . " - Network Settings" . "<br/>";
                    //If there is a SSID, print it also
                    if (is_array($network_ip_address) && array_key_exists('SSID', $network_ip_address)) {
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

            echo "<span class='callout callout-danger'>REBOOT REQUIRED: Please VERIFY the above settings, if they seem incorrect please adjust them in <a href='./networkconfig.php'>Network Settings</a> BEFORE rebooting. </span>";
        }
        ?>
                            </div>
                            <?php
}
    ?>
                            <div class="backdrop">
                                <div class="container-fluid">
                                    <div class="row">
                                        <div class="col-md-6 backup-config-start-panel">
                                            <div class="row">
                                                <h2>Backup Configuration</h2>
                                            </div>
                                            <div class="row">
                                                <div class="col-md-4">
                                                    <span>Protect sensitive data?</span>
                                                </div>
                                                <div class="col-md-8">
                                                    <input id="dataProtect" name="protectSensitive"
                                                           type="checkbox"
                                                           checked="true">
                                                </div>
                                            </div>

                                            <div class="row">
                                                <div class="col-md-4">
                                                    <span class='jsonConfigUSB'>Copy Backups To Additional Location:</span>
                                                </div>
                                                <div class="col-md-8">
                                                    <select name='jsonConfigbackup.USBDevice'
                                                            id='jsonConfigbackup.USBDevice'
                                                            onChange='JSONConfigBackupUSBDeviceChanged();'></select>
                                                    <input type='button' class='buttons refreshBackupDevicesList' onClick='GetBackupDevices();'
                                                           value='Refresh List'>
                                                    <img id="jsonConfigUSBUsage_img"
                                                         title="Specify an additional storage device where configuration backups will be copied to. Backups will first be saved to the config directory (<?php echo $settings['configDirectory'] . "/backups" ?>), and then copied to the alternative location."
                                                         src="images/redesign/help-icon.svg" class="icon-help">
                                                </div>
                                            </div>

                                            <div class="row">
                                                <div class="col-md-4"><span>Backup Area</span></div>
                                                <div class="col-md-8"><?php echo genSelectList('backuparea'); ?></div>
                                            </div>
                                            <div class="row">
                                                <div class="col-md-4"></div>
                                                <div class="col-md-8">
                                                    <button name="btnDownloadConfig" type="Submit" class="buttons"
                                                            value="Download Configuration"><i
                                                                class="fas fa-fw fa-nbsp fa-download"></i>Download
                                                        Configuration
                                                    </button>
                                                </div>
                                            </div>
                                        </div>

                                        <div class="col-md-6 backup-config-end-panel">
                                            <div class="row">
                                                <div class="col-md-12">
                                                    <h2>Download Existing Backups</h2>
                                                </div>
                                            </div>

                                            <div class="row">
                                                <div class="col-md-12 table-download-existing-backups-container table-responsive">
                                                    <table class="table table-download-existing-backups">
                                                        <thead>
                                                        <tr>
                                                            <th scope="col">Date</th>
                                                            <th scope="col">Configuration Change</th>
                                                            <th scope="col">Actions</th>
                                                        </tr>
                                                        </thead>
                                                        <tbody id="table-download-existing-backups-content">

                                                        <tr>
                                                            <td></td>
                                                            <td>Loading Existing Backups....</td>
                                                            <td></td>
                                                        </tr>

                                                        </tbody>
                                                    </table>
                                                </div>
                                            </div>

                                            <div class="row">
                                                <div class="col-md-6 text-center table-download-existing-backups-legend">
                                                    <span>
                                                        <i class="fas fa-sd-card"></i> - Located On FPP Storage Device
                                                    </span>
                                                </div>
                                                <div class="col-md-6 text-center table-download-existing-backups-legend">
                                                    <span><i class="fas fa-hdd">
                                                        </i> - Located On Alternate Backup Location
                                                    </span>
                                                </div>
                                            </div>
                                        </div>
                                    </div>
                                </div>
                            </div>

                            <br/>

                            <div class="backdrop">

                                <div class="container-fluid">

                                    <div class="row">
                                        <div class="col-md-6 restore-config-start-panel">
                                            <div class="row">
                                                <h2>Restore Configuration</h2>
                                            </div>
                                            <div class="row">
                                                <div class="col-md-4">Keep Existing Network Settings</div>
                                                <div class="col-md-8">
                                                    <input name="keepExitingNetwork"
                                                           type="checkbox"
                                                           checked="true">
                                                </div>
                                            </div>
                                            <div class="row">
                                                <div class="col-md-4">Keep Existing Player/Remote Settings</div>
                                                <div class="col-md-8">
                                                    <input name="keepMasterSlave"
                                                           type="checkbox"
                                                           checked="true">
                                                </div>
                                            </div>
                                            <div class="row">
                                                <div class="col-md-4">Restore Area</div>
                                                <div class="col-md-8">
													<?php echo genSelectList('restorearea'); ?></div>
                                            </div>
                                            <div class="row">
                                                <div class="col-md-4"></div>
                                                <div class="col-md-8">
                                                    <i class="fas fa-fw fa-nbsp fa-upload"></i>
                                                    <input id="btnUploadConfig" name="conffile" type="file"
                                                           accept=".json" id="conffile" autocomplete="off">
                                                    <script>
                                                        $('#btnUploadConfig').on("change", function (e) {
                                                            if (e.target.files[0].name.length > 4) {
                                                                $('#btnRestoreConfig').show();
                                                            }
                                                        });
                                                    </script>
                                                </div>
                                            </div>
                                            <div class='row'>
                                                <div class="col-md-4"></div>
                                                <div class="col-md-8">
                                                    <button id="btnRestoreConfig" name="btnRestoreConfig" type="Submit"
                                                            class="buttons hidden">
                                                        <i class="fas fa-fw fa-nbsp fa-file-import"></i>Restore
                                                        Configuration
                                                    </button>
                                                </div>
                                            </div>
                                        </div>

                                        <div class="col-md-6 restore-config-end-panel">
                                            <div class="row">
                                                <div class="col-md-12">
                                                    <h2>Restore Existing Backups</h2>
                                                </div>
                                            </div>

                                            <div class="row">
                                                <div class="col-md-12 table-restore-existing-backups-container">
                                                    <table class="table table-restore-existing-backups">
                                                        <thead>
                                                        <tr>
                                                            <th scope="col">Date</th>
                                                            <th scope="col">Configuration Change</th>
                                                            <th scope="col">Actions</th>
                                                        </tr>
                                                        </thead>
                                                        <tbody id="table-restore-existing-backups-content">

                                                        <tr>
                                                            <td></td>
                                                            <td>Loading Existing Backups....</td>
                                                            <td></td>
                                                        </tr>

                                                        </tbody>
                                                    </table>
                                                </div>
                                            </div>

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
        <tr class='copyHost'><td>Remote Host:</td><td><?php PrintSettingSelect('Backup Host', 'backup.Host', 0, 0, '', $backupHosts, '', 'GetBackupHostBackupDirs');?></td></tr>
        <tr class='copyHostDevice'><td>Remote Storage:</td><td><select id="backup.RemoteStorage" onchange="backupRemoteStorageChanged();"><option value="none" selected="">Default FPP Storage</option></select></td></tr>
        <tr class='copyPath'><td>Backup Path:</td><td><?php PrintSettingTextSaved('backup.Path', 0, 0, 128, 64, '', $settings["HostName"]);?></td></tr>
        <tr class='copyPathSelect'><td>Backup Path:</td><td><select name='backup.PathSelect' id='backup.PathSelect'></select></td></tr>
        <tr class='sendCompressed'>
            <td>Send Compressed Data: </td>
            <td>
				<?php PrintSettingCheckbox('Send Compressed Data:', 'backup.sendCompressed', 0, 0, 1, 0, "", "", 0, ' (Compress files during copy to speed up the copy process. NOTE: Newer xLights versions already used a compressed FSEQ format, so this option may only slow down the transfer as FPP tries to recompress already-compressed data. Some data like Music and Videos are not compressed.)');?>
                <br>
            </td>
        </tr>

       <tr><td>What to copy:</td><td>
        <table id="CopyFlagsTable">
        <tr><td>
				<?php PrintSettingCheckbox('Backup Configuration', 'backup.Configuration', 0, 0, 1, 0, "", "", 1, 'Configuration');?><br>
				<?php PrintSettingCheckbox('Backup Playlists', 'backup.Playlists', 0, 0, 1, 0, "", "", 1, 'Playlists');?><br>
            </td><td width='10px'></td><td>
				<?php PrintSettingCheckbox('Backup Plugins', 'backup.Plugins', 0, 0, 1, 0, "", "", 1, 'Plugins');?><br>
            </td><td width='10px'></td><td>
				<?php PrintSettingCheckbox('Backup Sequences', 'backup.Sequences', 0, 0, 1, 0, "", "", 1, 'Sequences');?><span style="color: #AA0000">*</span><br>
				<?php PrintSettingCheckbox('Backup Images', 'backup.Images', 0, 0, 1, 0, "", "", 1, 'Images');?><br>
            </td><td width='10px'></td><td>
				<?php PrintSettingCheckbox('Backup Scripts', 'backup.Scripts', 0, 0, 1, 0, "", "", 1, 'Scripts');?><br>
				<?php PrintSettingCheckbox('Backup Effects', 'backup.Effects', 0, 0, 1, 0, "", "", 1, 'Effects');?><br>
            </td><td width='10px'></td><td>
				<?php PrintSettingCheckbox('Backup Music', 'backup.Music', 0, 0, 1, 0, "", "", 1, 'Music');?><br>
				<?php PrintSettingCheckbox('Backup Videos', 'backup.Videos', 0, 0, 1, 0, "", "", 1, 'Videos');?><br>
            </td><td width='10px'></td><td valign='top'>
<?php
$eepromValue = file_exists('/home/fpp/media/config/cape-eeprom.bin') ? 1 : 0;
    PrintSettingCheckbox('Backup Virtual EEPROM', 'backup.EEPROM', 0, 0, 1, 0, "", "", $eepromValue, 'Virtual EEPROM');
    ?>
                <span class='copyBackups'><br><input type='checkbox' id='backup.Backups'>Backups <span style="color: #AA0000">*</span></span>
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
            <div id="dialogSensitiveDetails" title="Warning!" style="display:none">
                <p>Un-checking this box will disable protection (automatic removal) of sensitive data like passwords.
                    <br>
                    <b>ONLY</b> Un-check this if you want to be able make an exact clone of settings to another FPP.
                    <br>
                    <b>NOTE:</b> The backup will include passwords in plaintext, you assume full responsibility for this
                    file.
                    <br>
                </p>
            </div>

            <div id="dialog_copyToUsb" title="Do You To Copy Existing Backups to USB?" style="display:none">
                <p>Do you want to perform an initial copy of any existing backups on SD card to the chosen USB device?.
                    <br>
                    <b>If Yes</b>, any existing JSON Setting backups on the SD card will be copied to the selected USB storage device.
                    <br>
                    <b>If No</b>, backups will copied across on any future setting change.
                </p>
            </div>

        </div>
    </div>
    <script>
        $('#dataProtect').on("click", function () {
            var checked = $(this).is(':checked');
            if (!checked) {

                DoModalDialog({
                    id: "dialogSensitiveDetails_Modal",
                    title: "Sensitive Details Will Not Be Protected",
                    width: 400,
                    autoResize: true,
                    closeOnEscape: false,
                    backdrop: true,
                    body: $('#dialogSensitiveDetails').html(),
                    class: "",
                    buttons: {
                        "Ok": {
                            id: "dialog_copyToUsb_DoCopy",
                            click: function () {
                                CloseModalDialog("dialogSensitiveDetails_Modal");
                            }
                        }
                    }
                });
            }
        });

        // $("#tabs").tabs({cache: true, active: activeTabNumber, spinner: "", fx: { opacity: 'toggle', height: 'toggle' } });

    </script>
    <?php include 'common/footer.inc';?>
</div>
<div id='copyPopup' title='FPP Backup/Restore' style="display: none;">
    <textarea style='min-height: 600px; width: 100%' disabled id='copyText'></textarea>
    <input id='closeDialogButton' type='button' class='buttons' value='Close' onClick='CloseCopyDialog();' style='display: none;'>
</div>

<datalist id='usbDirectories'>
</datalist>
</body>
</html>
	<?php
}
?>
