<?

/**
 * Get available backups from subdir
 *
 * Returns a list of full system backup directories within the given path,
 * excluding directories that exist in the media root.
 *
 * @param string $backupDir Absolute path to the directory to scan.
 * @return array List of relative directory paths found under $backupDir.
 */
function getAvailableBackupsFromDir($backupDir)
{
	global $settings;

	$excludeList = array();
	$dirs = array();

	foreach (scandir($settings['mediaDirectory']) as $fileName) {
		if (($fileName != '.') &&
			($fileName != '..')) {
			array_push($excludeList, $fileName);
		}
	}

	array_push($dirs, '/');

	foreach (scandir($backupDir) as $fileName) {
		if (($fileName != '.') &&
			($fileName != '..') &&
			(!in_array($fileName, $excludeList)) &&
			(is_dir($backupDir . '/' . $fileName))) {
			array_push($dirs, $fileName);

			foreach (scandir($backupDir . '/' . $fileName) as $subfileName) {
				if (($subfileName != '.') &&
					($subfileName != '..') &&
					(!in_array($subfileName, $excludeList)) &&
					(is_dir($backupDir . '/' . $fileName . '/' . $subfileName))) {
					array_push($dirs, $fileName . '/' . $subfileName);
				}
			}
		}
	}

	return $dirs;
}

/**
 * Get available backups
 *
 * Returns a list of full system backup files stored in the local `backups/` directory.
 *
 * @route GET /api/backups/list
 * @response 200 List of backup directory names
 * ```json
 * ["/", "FPPDevP4", "FPPDevP4_2026_05_02"]
 * ```
 */
function GetAvailableBackups()
{
	global $settings;
	return json(getAvailableBackupsFromDir($settings['mediaDirectory'] . '/backups/'));
}

/**
 * Get devices available for backups
 *
 * Returns a list of devices (e.g. USB drives, SSDs) attached to the system that can be used for backups.
 *
 * @route GET /api/backups/devices
 * @response 200 List of available backup devices
 * ```json
 * [
 *   {
 *     "name": "sda1",
 *     "size": 7.5,
 *     "model": "Cruzer Blade",
 *     "vendor": "SanDisk"
 *   }
 * ]
 * ```
 */
function RetrieveAvailableBackupsDevices()
{
	$devices = GetAvailableBackupsDevices();

	return json($devices);
}

/**
 * Get list of backups on device
 *
 * Returns a list of full system backup files stored on the specified device (e.g. a USB drive).
 *
 * @route GET /api/backups/list/{DeviceName}
 * @response 200 List of backup directory names on the device
 * ```json
 * ["/", "FPPDevP4", "FPPDevP4_2026_05_02"]
 * ```
 */
function GetAvailableBackupsOnDevice()
{
	global $SUDO;
	$deviceName = params('DeviceName');
	$dirs = array();

	$dirs = driveMountHelper($deviceName, 'getAvailableBackupsFromDir', array('/mnt/tmp/'));

	return json($dirs);
}

/**
 * Mount device (with callback)
 *
 * Mounts the specified device and performs a task via the callback function (if specified).
 *
 * @param string $deviceName              The device to be mounted.
 * @param string $usercallback_function   The function to call once mounting is completed.
 * @param array  $functionArgs            Arguments passed to the callback; order and count MUST match the callback signature.
 * @param string $mountPath               Path where the device will be mounted. Default: /mnt/tmp.
 * @param bool   $unmountWhenDone         Whether to automatically unmount the device when done.
 * @param bool   $globalNameSpace         Whether to use nsenter to mount under the root mount namespace.
 * @param bool   $returnResultCodes       Whether to return output and result codes of the commands that were run.
 * @return mixed Callback return value, or an array with detailed command results when $returnResultCodes is true.
 */
function driveMountHelper($deviceName, $usercallback_function, $functionArgs = array(), $mountPath = '/mnt/tmp', $unmountWhenDone = true, $globalNameSpace = false, $returnResultCodes = false)
{
	global $SUDO;
	$dirs = array();
	$nsEnter = '';
	$fsTypeOutput = $mountCmdOutput = $unmountCmdOutput = array();
	$unmountCmdResultCode = null;

	//Run commands so mount is available to entire system
	if ($globalNameSpace === true) {
		//Since Apache sandboxes all mount interactions and makes them private to the apache process(s)
		//https://manpages.ubuntu.com/manpages/bionic/man1/nsenter.1.html
		//Run commands under the root (process 1) mount namespace so the mounted device is available to the entire system
		$nsEnter = ' nsenter -m -t 1 ';
	}

	// unmount just in case
	exec($SUDO . $nsEnter . ' umount ' . $mountPath);

	$unusable = CheckIfDeviceIsUsable($deviceName);
	if ($unusable != '') {
		array_push($dirs, $unusable);
		return json($dirs);
	}

	exec($SUDO . $nsEnter . ' mkdir -p ' . $mountPath);

	$fsType = exec($SUDO . $nsEnter . ' file -sL /dev/' . $deviceName, $fsTypeOutput, $fsTypeResultCode);

	$mountCmd = '';
	// Same mount options used in scripts/copy_settings_to_storage.sh
	if (preg_match('/BTRFS/', $fsType)) {
		$mountCmd = "mount -t btrfs -o noatime,nodiratime,compress=zstd,nofail /dev/$deviceName $mountPath";
	} else if ((preg_match('/FAT/', $fsType)) ||
		(preg_match('/DOS/', $fsType))) {
		$mountCmd = "mount -t auto -o noatime,nodiratime,exec,nofail,uid=500,gid=500 /dev/$deviceName $mountPath";
	} else {
		// Default to ext4
		$mountCmd = "mount -t ext4 -o noatime,nodiratime,nofail /dev/$deviceName $mountPath";
	}

	exec($SUDO . $nsEnter . ' ' . $mountCmd, $mountCmdOutput, $mountCmdResultCode);

	if (isset($usercallback_function) && !empty($functionArgs)) {
		//Call the function that will do some work in the mounted directory
		$dirs = call_user_func_array($usercallback_function, $functionArgs);
	}

	//Unmount by default before we finish
	if ($unmountWhenDone === true) {
		$umountCmd = exec($SUDO . $nsEnter . ' umount -l ' . $mountPath, $unmountCmdOutput, $unmountCmdResultCode);
	}

	//Return more detail about the what has happened with each command
	if ($returnResultCodes === true) {
		return (array('dirs' => $dirs,
			'fsType' => array('fsTypeOutput' => $fsTypeOutput, 'fsTypeResultCode' => $fsTypeResultCode),
			'mountCmd' => array('mountCmdOutput' => $mountCmdOutput, 'mountCmdResultCode' => $mountCmdResultCode, 'mountCmdResultCodeText' => mountReturnCodeMap($mountCmdResultCode), 'actualMountCmd' => $mountCmd),
			'unmountCmd' => array('unmountCmdOutput' => $unmountCmdOutput, 'unmountCmdResultCode' => $unmountCmdResultCode),
			'args' => array('mountPath' => '/mnt/tmp', 'unmountWhenDone' => $unmountWhenDone, 'globalNameSpace' => $globalNameSpace)
		)
		);
	}

	return ($dirs);
}

/**
 * Mount device
 *
 * Mounts the specified device to `/mnt/{MountLocation}` (defaults to `/mnt/api_mount`).
 *
 * @route POST /api/backups/devices/mount/{DeviceName}/{MountLocation}
 * @response 200 Device mounted successfully
 * ```json
 * {
 *   "Status": "OK",
 *   "Message": "Device (sda1) mounted at (/mnt/api_mount)",
 *   "MountLocation": "/mnt/api_mount"
 * }
 * ```
 */
function MountDevice()
{
	$drive_mount_location = "/mnt/api_mount";
	//get the device name
	$deviceName = params('DeviceName');
	$mountLocation = params('MountLocation');
	//If a mount location was supplied, adjust the drive mount location to latch it
	if (isset($mountLocation) && !empty($mountLocation)) {
		$drive_mount_location = "/mnt/" . $mountLocation;
	}

	$mountResultCode = $mountResultCodeText = null;
	$result = array();

	//Make sure we have a valid device name supplied
	if (isset($deviceName) && ($deviceName !== "no" || $deviceName !== "")) {
		//Mount the device at the specified location
		$mountResult = driveMountHelper($deviceName, '', array(), $drive_mount_location, false, true, true);

		//Check the relevant result code keys exist in the returned data
		if (array_key_exists('mountCmd', $mountResult) && array_key_exists('mountCmdResultCode', $mountResult['mountCmd'])) {
			$mountResultCode = $mountResult['mountCmd']['mountCmdResultCode'];
			$mountResultCodeText = $mountResult['mountCmd']['mountCmdResultCodeText'];
		}

		//Success
		if ($mountResultCode == 0) {
			$result = array('Status' => 'OK', 'Message' => 'Device (' . $deviceName . ') mounted at (' . $drive_mount_location . ')', 'MountLocation' => $drive_mount_location);
			//TODO REMOVE

			error_log('MountDevice: ( ' . json($result) . ' )');
		} else {
			//Some kind of failure
			$result = array('Status' => 'Error', 'Message' => 'Unable to mount device (' . $deviceName . ') under (' . $drive_mount_location . ')', 'MountLocation' => $drive_mount_location, 'Error' => $mountResultCodeText);
			error_log('MountDevice: ( ' . json($result) . ' )');
		}
	} else {
		$result = array('Status' => 'Error', 'Message' => 'Invalid Device Name Supplied');
		error_log('MountDevice: ( ' . json($result) . ' )');
	}

	return json($result);
}

/**
 * Unmount device
 *
 * Unmounts the drive at `/mnt/{MountLocation}` (defaults to `/mnt/api_mount`).
 *
 * @route POST /api/backups/devices/unmount/{DeviceName}/{MountLocation}
 * @response 200 Device unmounted successfully
 * ```json
 * {
 *   "Status": "OK",
 *   "Message": "Device (sda1) unmounted from (/mnt/api_mount)",
 *   "MountLocation": "/mnt/api_mount"
 * }
 * ```
 */
function UnmountDevice()
{
	global $SUDO;
	$drive_mount_location = "/mnt/api_mount";
	$nsEnter = ' nsenter -m -t 1 ';

	//get the device name
	$deviceName = params('DeviceName');
	$mountLocation = params('MountLocation');
	//If a mount location was supplied, adjust the drive mount location to latch it
	if (isset($mountLocation) && !empty($mountLocation)) {
		$drive_mount_location = "/mnt/" . $mountLocation;
	}

	$unmountCmdOutput = array();
	$unmountCmdResultCode = null;

	//Make sure we have a valid device name supplied
	if (isset($deviceName) && ($deviceName !== "no" || $deviceName !== "")) {
		//Unmount device from the root namespace (it will be mounted there)
		$umountCmd = exec($SUDO . $nsEnter . ' umount -l ' . $drive_mount_location, $unmountCmdOutput, $unmountCmdResultCode);
		//This could be incorrect as this result codes are for the mount command, but will give us an idea
		$unmountCmdOutputText = mountReturnCodeMap($unmountCmdResultCode);

		//Remove the folder that is created for the device to be mounted under
		exec($SUDO . ' rmdir ' . $drive_mount_location);

		//Success
		if ($unmountCmdResultCode == 0) {
			$result = array('Status' => 'OK', 'Message' => 'Device (' . $deviceName . ') unmounted from (' . $drive_mount_location . ')', 'MountLocation' => $drive_mount_location);
			//TODO REMOVE
			error_log('UnmountDevice: ( ' . json($result) . ' )');
		} else {
			//Some kind of failure
			$result = array('Status' => 'Error', 'Message' => 'Unable to unmount device (' . $deviceName . ') from under (' . $drive_mount_location . ')', 'MountLocation' => $drive_mount_location, 'Error' => $unmountCmdOutputText);
			error_log('UnmountDevice: ( ' . json($result) . ' )');
		}
	} else {
		$result = array('Status' => 'Error', 'Message' => 'Invalid Device Name Supplied');
		error_log('UnmountDevice: ( ' . json($result) . ' )');
	}

	return json($result);
}

////
//Functions for JSON Configuration Backup API
/**
 * Get available JSON backups
 *
 * Returns a list of JSON configuration backups stored locally, or — if `jsonConfigBackupUSBLocation`
 * is set — a combined list from local storage and the configured USB device.
 *
 * @route GET /api/backups/configuration/list
 * @response 200 List of available JSON configuration backups
 * ```json
 * [
 *   {
 *     "backup_alternative_location": false,
 *     "backup_filedirectory": "/home/fpp/media/config/backups",
 *     "backup_filename": "FPP_all-backup_v6_20230124212305.json",
 *     "backup_comment": "FPP Settings - Disable Scheduler setting was set to ( 0 ).",
 *     "backup_time": "Tue Jan 24 21:23:05 2023",
 *     "backup_time_unix": "1674559385"
 *   },
 *   {
 *     "backup_alternative_location": true,
 *     "backup_filedirectory": "/mnt/tmp/Automatic_Backups/config/backups",
 *     "backup_filename": "FPP_all-backup_v6_20230124210519.json",
 *     "backup_comment": "Schedule was modified.",
 *     "backup_time": "Tue Jan 24 21:05:19 2023",
 *     "backup_time_unix": "1674558319"
 *   }
 * ]
 * ```
 */
function GetAvailableJSONBackups(){
	global $settings;

	$json_config_backup_filenames_on_alternative = array();
	$json_config_backup_filenames_clean = array();

	//Get the full directory path for the type of directory type we're processing
	$dir_jsonbackups = GetDirSetting('JsonBackups');

	//Grabs only the array keys which contain the JSON filenames
	$json_config_backup_filenames = (read_directory_files($dir_jsonbackups, false, true, 'asc'));
	//Process the backup files to extra some info about them
	$json_config_backup_filenames = processJsonBackupFileDataHelper($json_config_backup_filenames, $dir_jsonbackups);

	//See what backups are stored on the selected storage device if it's value is set
	if (isset($settings['jsonConfigBackupUSBLocation']) && !empty($settings['jsonConfigBackupUSBLocation'])) {
		$dir_jsonbackupsalternate = GetDirSetting('JsonBackupsAlternate');

		//$settings['jsonConfigBackupUSBLocation'] is the selected alternative drive to stop backups to
		$json_config_backup_filenames_on_alternative = driveMountHelper($settings['jsonConfigBackupUSBLocation'], 'read_directory_files', array($dir_jsonbackupsalternate, false, true, 'asc'));
		//Process the backup files to extra some info about them
		$json_config_backup_filenames_on_alternative =  driveMountHelper($settings['jsonConfigBackupUSBLocation'], 'processJsonBackupFileDataHelper', array($json_config_backup_filenames_on_alternative, $dir_jsonbackupsalternate));
	}
	//Merge the results together, if the same backup name exists in the alternative backup location it will overwrite the record from the local cnfig directory
	$json_config_backup_filenames_clean = array_merge($json_config_backup_filenames, $json_config_backup_filenames_on_alternative);

	//Once merged - do another sort on the entries but sort on the backup_time_unix value
	usort($json_config_backup_filenames_clean, "sortBackupTimeAsc");

	return json($json_config_backup_filenames_clean);
}

/**
 * Extracts metadata (comment, trigger source, timestamp) from a list of backup filenames.
 *
 * @param array  $json_config_backup_Data Associative array of backup filenames to raw file data.
 * @param string $source_directory        Absolute path to the directory containing the backup files.
 * @return array Processed array keyed by clean backup name with metadata fields.
 */
function processJsonBackupFileDataHelper($json_config_backup_Data, $source_directory)
{
	global $settings;

	$json_config_backup_filenames_clean = array();

	//process each of the backups and read out the backup comment, and work out the date it was created
	foreach ($json_config_backup_Data as $backup_filename => $backup_data) {
		$backup_data_comment = '';
		$backup_data_trigger_source = null;
		$backup_alternative = false;
		$backup_filepath = $source_directory;
		//Check to see if the source direct is the same as the default or not, if it is't then the the directory is the alternative backup directory (USB or something)
		if ($source_directory !== GetDirSetting('JsonBackups')) {
			$backup_alternative = true;
		}

		//cleanup the filename so it can be used as as a ID
		$backup_filename_clean = trim(str_replace('.json', '', $backup_filename));

		//Read the backup file so we can extract some metadata
		$decoded_backup_data = json_decode(file_get_contents($backup_filepath . '/' . $backup_filename), true);
		if (is_null($decoded_backup_data)) {
			$decode_error_result = array('Status' => 'Error', 'Message' => 'Unable to decode JSON backup file (' . $backup_filepath . '/' . $backup_filename, 'IsReadable' => is_readable($backup_filepath . '/' . $backup_filename), 'FileContent' => file_get_contents($backup_filepath . '/' . $backup_filename));
			error_log('processJsonBackupFileDataHelper: ( ' . json_encode($decode_error_result) . ' )');
		}

		if (is_array($decoded_backup_data) && array_key_exists('backup_comment', $decoded_backup_data)) {
			$backup_data_comment = $decoded_backup_data['backup_comment'];
		}
		if (is_array($decoded_backup_data) && array_key_exists('backup_trigger_source', $decoded_backup_data)) {
			$backup_data_trigger_source = $decoded_backup_data['backup_trigger_source'];
		}

		//Locate the last underscore, this appears before the date/time in the filename
		$backup_date_time_pos = strrpos($backup_filename_clean, '_');
		//Extract everything between this occurrence and the end of the string, this will be the date time in full
		$backup_date_time_str = substr($backup_filename_clean, $backup_date_time_pos + 1);
		//Date time created in this format date("YmdHis"), output it in this format date('D M d H:i:s T Y') so it's more human-readable
		$backup_date_time = DateTime::createFromFormat("YmdHis", $backup_date_time_str)->format('D M d H:i:s Y');
		$backup_date_time_unix = DateTime::createFromFormat("YmdHis", $backup_date_time_str)->format('U');

		$json_config_backup_filenames_clean[$backup_filename_clean] = array('backup_alternative_location' => $backup_alternative,
			'backup_filedirectory' => $backup_filepath,
			'backup_filename' => $backup_filename,
			'backup_comment' => $backup_data_comment,
			'backup_trigger_source' => $backup_data_trigger_source,
			'backup_time' => $backup_date_time,
			'backup_time_unix' => $backup_date_time_unix
		);

		unset($decoded_backup_data);
	}

	return $json_config_backup_filenames_clean;
}

/**
 * Create JSON backup
 *
 * Generates a new JSON settings backup for all settings areas. If an alternate backup location has been
 * set, the backup is also copied to that location.
 *
 * @route POST /api/backups/configuration
 * @body "The describing comment to be added to the backup"
 * @response 200 Backup created successfully
 * ```json
 * {
 *   "success": true,
 *   "backup_file_path": "/home/fpp/media/config/backups/FPP_all-backup_v6_20230124212305.json",
 *   "copied_to_usb": true
 * }
 * ```
 */
function MakeJSONBackup()
{
	global $settings, $skipJSsettings,
		   $mediaDirectory, $playlistDirectory, $scriptDirectory, $settingsFile,
		   $skipHTMLCodeOutput,
		   $system_config_areas, $known_json_config_files, $known_ini_config_files,
		   $backup_errors, $backup_error_string, $backups_verbose_logging,
		   $sensitive_data, $protectSensitiveData,
		   $fpp_backup_format_version, $fpp_major_version, $fpp_backup_prompt_download,
		   $fpp_backup_max_age, $fpp_backup_min_number_kept,
		   $fpp_backup_location, $fpp_backup_location_alternate_drive;

	$trigger_source = null;

	//Get data out of the post data
	$input_data = file_get_contents('php://input');

	//Try JSON decode the input in case it's an array with extra data
	$input_data_decoded = json_decode($input_data, true);

	if (json_last_error() === JSON_ERROR_NONE) {
		// JSON is valid, get comment and trigger source
		$backup_comment = $input_data_decoded['backup_comment'];
		$trigger_source = $input_data_decoded['trigger_source'];
	} else {
		//It's just a string so it'll be just a comment
		//Get the backup comment out of the post data
		$backup_comment = $input_data;
	}

	$toUSBResult = false;

	//Include the FPP backup script
	require_once "../backup.php";

	//Create the backup and return the status
	$backup_creation_status = performBackup('all', false, $backup_comment, $trigger_source);

	if (array_key_exists('success', $backup_creation_status) && $backup_creation_status['success'] == true) {
		$toUSBResult = DoJsonBackupToUSB();
		if ($toUSBResult){
			$backup_creation_status['copied_to_usb'] = true;
		}else{
			$backup_creation_status['copied_to_usb'] = false;
		}
	} else {
		/* Handle error */
		error_log('MakeJSONBackup: Something went wrong trying to call backups API to make a settings backup. (' . json_encode(['result' => $backup_creation_status]));
		$backup_creation_status['copied_to_usb'] = false;
	}

	//Return the result which contains the success of the backup and the path which it was written to;
	return json($backup_creation_status);
}

/**
 * Get list of JSON backups on device
 *
 * Returns a list of JSON configuration files on a specified alternate storage device.
 * Available devices can be obtained from `/backups/devices`, or the currently configured device
 * is stored in the `jsonConfigBackupUSBLocation` setting.
 *
 * @route GET /api/backups/configuration/list/{DeviceName}
 * @response 200 List of JSON backup filenames on the device
 * ```json
 * [
 *   "FPP_all-backup_v6_20230114025351.json",
 *   "FPP_all-backup_v6_20230114025354.json",
 *   "FPP_all-backup_v6_20230114214459.json",
 *   "FPP_all-backup_v6_20230114215622.json"
 * ]
 * ```
 */
function GetAvailableJSONBackupsOnDevice(){
	global $SUDO, $settings;
	$deviceName = params('DeviceName');

	//Get the full directory path for the type of directory type we're processing
	$dir_jsonbackupsalternate = GetDirSetting('JsonBackupsAlternate');

	$json_config_backup_filenames = driveMountHelper($deviceName, 'read_directory_files', array($dir_jsonbackupsalternate, false, true));

	//do some additional massaging of the data
	$json_config_backup_filenames = array_keys($json_config_backup_filenames);

	return json($json_config_backup_filenames);
}

/**
 * Restores JSON backup
 *
 * Restores the specified JSON backup. `Directory` is either `JsonBackups` (local) or `JsonBackupsAlternate`
 * (configured alternate device). `GET /api/backups/configuration/list` can be used to obtain valid
 * directory and filename combinations.
 *
 * @route POST /api/backups/configuration/restore/{Directory}/{BackupFilename}
 * @body "all"
 * @response 200 Restore result
 * ```json
 * {
 *   "Success": true,
 *   "Message": {
 *     "success": true,
 *     "message": {
 *       "channelInputs": {
 *         "VALID_DATA": true,
 *         "ATTEMPT": true,
 *         "SUCCESS": true
 *       }
 *     }
 *   }
 * }
 * ```
 */
function RestoreJsonBackup(){
	global $SUDO, $settings, $skipJSsettings,
		   $mediaDirectory, $playlistDirectory, $scriptDirectory, $settingsFile, $scheduleFile, $fppDir,
		   $skipHTMLCodeOutput,
		   $system_config_areas, $known_json_config_files, $known_ini_config_files,
		   $backup_errors,$backup_error_string, $backups_verbose_logging,
		   $keepMasterSlaveSettings, $keepNetworkSettings, $uploadData_IsProtected, $settings_restored,
		   $network_settings_restored, $network_settings_restored_post_apply, $network_settings_restored_applied_ips,
		   $sensitive_data, $protectSensitiveData,
		   $fpp_backup_format_version, $fpp_major_version, $fpp_backup_prompt_download,
		   $fpp_backup_max_age, $fpp_backup_min_number_kept,
		   $fpp_backup_location, $fpp_backup_location_alternate_drive,
		   $args;

	//Get the backup comment out of the post data
	$area_to_restore = file_get_contents('php://input');
	//Directory is either JsonBackups (matching $settings['configDirectory'] . "/backups/")
	// or JsonBackupsAlternate (using the device set in jsonConfigBackupUSBLocation, it's mounted to /mnt/tmp/, then backups are sourced from /mnt/tmp/config/backups)
	$restore_from_directory = params('Directory');
	//Filename of the backup to restore
	$restore_from_filename = params('BackupFilename');

	//Get the full directory path for the type of directory type we're processing
	$dir = GetDirSetting($restore_from_directory);
	$fullPath = "$dir/$restore_from_filename";

	$file_contents_decoded = null;
	$restore_status = array('Success' => 'Failed', 'Message' => '');

	//check that the area supplied is not empty, if so then assume we're restoring all araeas
	if (empty($area_to_restore)) {
		$area_to_restore = "all";
	}

	//Make sure the directory and filename are supplied
	if (isset($restore_from_directory) && isset($restore_from_filename)) {
		//Include the FPP backup script
		require_once "../backup.php";

		//Restore the backup and return the status
		//Read in the backup file and json_decode the contents
		if (strtolower($restore_from_directory) === 'jsonbackups') {
			//get load the file from the config directory
			$file_contents = file_get_contents($fullPath);

			if ($file_contents !== FALSE) {
				//decode back into an array
				$file_contents_decoded = json_decode($file_contents, true);
			} else {
				//file_get_contents will return false if it couldn't read the file so
				$restore_status['Success'] = "Ok";
				$restore_status['Message'] = 'Backup File ' . $fullPath . ' could not be read.';
			}
		} else if ((strtolower($restore_from_directory) === 'jsonbackupsalternate')) {
			if (isset($settings['jsonConfigBackupUSBLocation']) && !empty($settings['jsonConfigBackupUSBLocation'])) {
				//Mount and read the json backup from the jsonConfigBackupUSBLocation location
				$file_contents = driveMountHelper($settings['jsonConfigBackupUSBLocation'], 'file_get_contents', array($fullPath));

				//If the file was read ok, $file_contents will be false if there was issue reading the file
				if ($file_contents !== FALSE) {
					//decode back into an array
					$file_contents_decoded = json_decode($file_contents, true);
				} else {
					//file_get_contents will return false if it couldn't read the file so
					$restore_status['Success'] = "Ok";
					$restore_status['Message'] = 'Backup File ' . $fullPath . ' could not be read.';
				}
			}
		}

		//Perform the restore
		$restore_data = doRestore($area_to_restore, $file_contents_decoded, $restore_from_filename, true, true, 'api');

		$restore_status['Success'] = $restore_data['success'];
		$restore_status['Message'] = $restore_data['message'];
	} else {
		$restore_status['Success'] = "Error";
		$restore_status['Message'] = 'Supplied Directory or Filename is invalid';
	}

	//Return the result which contains the success of the backup and the path which it was written to;
	return json($restore_status);
}

/**
 * Download JSON backup
 *
 * Downloads a specific JSON backup. `Directory` is either `JsonBackups` (local) or `JsonBackupsAlternate`
 * (configured alternate device). `GET /api/backups/configuration/list` can be used to obtain valid
 * directories and filenames.
 *
 * @route GET /api/backups/configuration/{Directory}/{BackupFilename}
 * @response 200 Contents of the specified JSON Settings backup as a download.
 * ```json
 * {
 *   "key": "value"
 * }
 * ```
 *
 * @response 404 Returned when the requested file cannot be located on disk.
 * ```json
 * {
 *   "Status": "File Not Found",
 *   "file": "FPP_all-backup_v6_20230124210514.json",
 *   "dir": "JsonBackups"
 * }
 * ```
 */
function DownloadJsonBackup(){
	global $settings;

	$status = "File not found";
	$dirName = params("Directory");
	$fileName = params("BackupFilename");

	//Get the full directory path for the type of directory type we're processing
	$dir = GetDirSetting($dirName);
	$fullPath = "$dir/$fileName";

	if (strtolower($dirName) == "jsonbackups") {
		//Check if the file exists, we can use file_exists directly on the path for normal jsonhackups as it's going to be in the /home/fpp directory
		$fileExists = file_exists($fullPath);

		if ($fileExists) {
			//Content type will always be json so see the header
			header("Content-Type: application/json");
			header("Content-Disposition: attachment; filename=\"" . basename($fileName) . "\"");

			//Empty the output buffers
			ob_clean();
			flush();

			readfile($fullPath);
		} else {
			$status = "File Not Found";
			return json(array("Status" => $status, "file" => $fileName, "dir" => $dirName));
		}
	} elseif (strtolower($dirName) == "jsonbackupsalternate") {
		//Use our driveMountHelper to mount the specified USB drive and check if the file exists
		$fileExists = driveMountHelper($settings['jsonConfigBackupUSBLocation'], 'file_exists', array($fullPath));

		if ($fileExists) {
			//Content type will always be json so see the header
			header("Content-Type: application/json");
			header("Content-Disposition: attachment; filename=\"" . basename($fileName) . "\"");

			//Empty the output buffers
			ob_clean();
			flush();

			driveMountHelper($settings['jsonConfigBackupUSBLocation'], 'readfile', array($fullPath));
		} else {
			$status = "File Not Found";
			return json(array("Status" => $status, "file" => $fileName, "dir" => $dirName));
		}
	}
}

/**
 * Delete JSON backup
 *
 * Deletes a specific JSON backup. `Directory` is either `JsonBackups` (local) or `JsonBackupsAlternate`
 * (configured alternate device). `GET /api/backups/configuration/list` can be used to obtain valid
 * directories and filenames.
 *
 * @route DELETE /api/backups/configuration/{Directory}/{BackupFilename}
 * @response 200 Backup deleted successfully
 * ```json
 * {
 *   "Status": "OK",
 *   "file": "FPP_all-backup_v6_20230124210514.json",
 *   "dir": "JsonBackupsAlternate"
 * }
 * ```
 */
function DeleteJsonBackup(){
	global $settings;

	$status = "File not found";
	$dirName = params("Directory");
	$fileName = params("BackupFilename");

	//Get the full directory path for the type of directory type we're processing
	$dir = GetDirSetting($dirName);
	$fullPath = "$dir/$fileName";

	$fileDeleted = false;
	$fileExists_alt = false;
	$dir_alt = $fullPath_alt = "";

	if (strtolower($dirName) == "jsonbackups") {
		//Check if the file exists, we can use file_exists directly on the path for normal jsonhackups as it's going to be in the /home/fpp/media directory
		$fileExists = file_exists($fullPath);

	} elseif (strtolower($dirName) == "jsonbackupsalternate") {
		//Use our driveMountHelper to mount the specified USB drive and check if the file exists

		//Mount the drive and see if the file exists
		$fileExists = driveMountHelper($settings['jsonConfigBackupUSBLocation'], 'file_exists', array($fullPath));
	}

	if ($dir == "") {
		$status = "Invalid Directory";
	} else if (!($fileExists)) {
		$status = "File Not Found";
	} else {

		if (strtolower($dirName) == "jsonbackups") {
			//Check if the file exists, we can use unlink directly on the path for normal jsonhackups as it's going to be in the /home/fpp directory
			$fileDeleted = unlink($fullPath);
		} elseif (strtolower($dirName) == "jsonbackupsalternate") {
			//Use our driveMountHelper to mount the specified USB drive and check if the file exists
			// Mount the drive and delete the file
			$fileDeleted = driveMountHelper($settings['jsonConfigBackupUSBLocation'], 'unlink', array($fullPath));

			//ALSO check if the file exists in the /home/fpp/media location, because the backup we're deleting could have been copied to USB from location
			//and we want to delete it in both
			$dir_alt = GetDirSetting('JsonBackups');
			$fullPath_alt = "$dir_alt/$fileName";
			$fileExists_alt = file_exists($fullPath_alt);
			//If the file exists in /home/media then delete it also as we want to delete copies of the file in both locations
			//it will exist in both locations if a USB device is selected to copy backups to
			if($fileExists_alt == true){
				//delete it
				unlink($fullPath_alt);
			}
		}

		if ($fileDeleted) {
			$status = "OK";
		} else {
			$status = "Unable to delete file";
		}
	}

	return json(array("Status" => $status, "file" => $fileName, "dir" => $dirName));
}

/**
 * Comparison function to sort backup entries by `backup_time_unix` in descending order.
 *
 * @param array $a First backup entry.
 * @param array $b Second backup entry.
 * @return int Negative, zero, or positive.
 */
function sortBackupTimeAsc($a, $b)
{
	return $b['backup_time_unix'] - $a['backup_time_unix'];
}

/**
 * Maps a `mount` command return code to a human-readable description.
 *
 * @param int $returnCode The numeric return code from the mount command.
 * @return string Human-readable status string.
 */
function mountReturnCodeMap($returnCode)
{
    if ($returnCode == 0) {
        return 'success';
    } else if ($returnCode == 1) {
        return 'incorrect invocation or permissions';
    } else if ($returnCode == 2) {
        return 'system error (out of memory, cannot fork, no more loop devices)';
    } else if ($returnCode == 4) {
        return 'internal mount bug';
    } else if ($returnCode == 8) {
        return 'user interrupt';
    } else if ($returnCode == 16) {
        return 'problems writing or locking /etc/mtab';
    } else if ($returnCode == 32) {
        return 'mount failure';
    } else if ($returnCode == 64) {
        return 'some mount succeeded';
    }
    return 'unkown';
}
?>
