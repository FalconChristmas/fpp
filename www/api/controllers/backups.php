<?
/////////////////////////////////////////////////////////////////////////////
/**
 * Returns a list of backups in the specified location
 * GET /api/backups/list
 * @param $backupDir
 * @return array
 */
function GetAvailableBackupsFromDir($backupDir)
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
 * Returns a list of local FPP file backup directories
 * GET api/backups/list
 * @return string
 */
function GetAvailableBackups()
{
	global $settings;
	return json(GetAvailableBackupsFromDir($settings['mediaDirectory'] . '/backups/'));
}

/**
 * Returns a list of devices like USB's or SSD's attached to the system
 * GET api/backups/devices
 * @return string
 */
function RetrieveAvailableBackupsDevices()
{
	$devices = GetAvailableBackupsDevices();

	return json($devices);
}

/**
 * Returns a list of FPP file backups on a specified device like USB or SSD attached to the system
 * GET api/backups/list/:DeviceName
 * @return string
 */
function GetAvailableBackupsOnDevice()
{
	global $SUDO;
	$deviceName = params('DeviceName');
	$dirs = array();

//	// unmount just in case
//	exec($SUDO . ' umount /mnt/tmp');
//
//	$unusable = CheckIfDeviceIsUsable($deviceName);
//	if ($unusable != '') {
//		array_push($dirs, $unusable);
//		return json($dirs);
//	}
//
//	exec($SUDO . ' mkdir -p /mnt/tmp');
//
//	$fsType = exec($SUDO . ' file -sL /dev/' . $deviceName, $output);
//
//	$mountCmd = '';
//	// Same mount options used in scripts/copy_settings_to_storage.sh
//	if (preg_match('/BTRFS/', $fsType)) {
//		$mountCmd = "mount -t btrfs -o noatime,nodiratime,compress=zstd,nofail /dev/$deviceName /mnt/tmp";
//	} else if ((preg_match('/FAT/', $fsType)) ||
//		(preg_match('/DOS/', $fsType))) {
//		$mountCmd = "mount -t auto -o noatime,nodiratime,exec,nofail,uid=500,gid=500 /dev/$deviceName /mnt/tmp";
//	} else {
//		// Default to ext4
//		$mountCmd = "mount -t ext4 -o noatime,nodiratime,nofail /dev/$deviceName /mnt/tmp";
//	}
//
//	exec($SUDO . ' ' . $mountCmd);
//
//	$dirs = GetAvailableBackupsFromDir('/mnt/tmp/');
//
//	exec($SUDO . ' umount /mnt/tmp');

	$dirs = DriveMountHelper($deviceName, 'GetAvailableBackupsFromDir', array('/mnt/tmp/'));

	return json($dirs);
}

/**
 * Handles mounting the specified device and performing
 *
 * @param $deviceName string The device to be mounted
 * @param $usercallback_function string The function we should call once mounting is completed so we can do something in the directory
 * @param $functionArgs array Order AND Number arguments MUST!! match the arguments required by the supplied user function
 * @param $mountPath string Path/Folder where the device will be mounted, DEFAULT: /mnt/tmp
 * @param $unmountWhenDone string Whether to automatically unmount the device when done
 * @param $globalNameSpace string Whether to use 'nsenter' to mount the device under the root mount namespace
 * @param $returnResultCodes string Whether to return output and result codes of the commands that were run
 * @return mixed|string
 */
function DriveMountHelper($deviceName, $usercallback_function, $functionArgs = array(), $mountPath = '/mnt/tmp', $unmountWhenDone = true, $globalNameSpace = false, $returnResultCodes = false)
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
			'mountCmd' => array('mountCmdOutput' => $mountCmdOutput, 'mountCmdResultCode' => $mountCmdResultCode, 'mountCmdResultCodeText' => MountReturnCodeMap($mountCmdResultCode), 'actualMountCmd' => $mountCmd),
			'unmountCmd' => array('unmountCmdOutput' => $unmountCmdOutput, 'unmountCmdResultCode' => $unmountCmdResultCode),
			'args' => array('mountPath' => '/mnt/tmp', 'unmountWhenDone' => $unmountWhenDone, 'globalNameSpace' => $globalNameSpace)
		)
		);
	}

	return ($dirs);
}

/**
 * Mounts the specified device to the specified mount location /mnt/<MountLocation>, Defaults to /mnt/api_mount
 * POST /backups/devices/mount/:DeviceName/:MountLocation
 * @return string
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
		$mountResult = DriveMountHelper($deviceName, '', array(), $drive_mount_location, false, true, true);

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
 * Unmounts the drive mounted to the supplied mount location e.g /mnt/<MountLocation>, Defaults to /mnt/api_mount
 * POST /backups/devices/unmount/:DeviceName/:MountLocation
 * @return string
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
		$unmountCmdOutputText = MountReturnCodeMap($unmountCmdResultCode);

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
 * Returns a list of JSON Configuration backups stored locally or if set the 'JSON Configuration Device'
 * GET /api/backups/configuration/list
 * @return string
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
	$json_config_backup_filenames = process_jsonbackup_file_data_helper($json_config_backup_filenames, $dir_jsonbackups);

	//See what backups are stored on the selected storage device if it's value is set
	if (isset($settings['jsonConfigBackupUSBLocation']) && !empty($settings['jsonConfigBackupUSBLocation'])) {
		$dir_jsonbackupsalternate = GetDirSetting('JsonBackupsAlternate');

		//$settings['jsonConfigBackupUSBLocation'] is the selected alternative drive to stop backups to
		$json_config_backup_filenames_on_alternative = DriveMountHelper($settings['jsonConfigBackupUSBLocation'], 'read_directory_files', array($dir_jsonbackupsalternate, false, true, 'asc'));
		//Process the backup files to extra some info about them
		$json_config_backup_filenames_on_alternative = process_jsonbackup_file_data_helper($json_config_backup_filenames_on_alternative, $dir_jsonbackupsalternate);
	}
	//Merge the results together, if the same backup name exists in the alternative backup location it will overwrite the record from the local cnfig directory
	$json_config_backup_filenames_clean = array_merge($json_config_backup_filenames, $json_config_backup_filenames_on_alternative);

	//Once merged - do another sort on the entries but sort on the backup_time_unix value
	usort($json_config_backup_filenames_clean, "sort_backup_time_asc");

	return json($json_config_backup_filenames_clean);
}

/**
 * Helper function to extact some metadata out of the backup files
 * @param $json_config_backup_Data
 * @param $source_directory
 * @return array
 */
function process_jsonbackup_file_data_helper($json_config_backup_Data, $source_directory)
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
		if (array_key_exists('backup_comment', $decoded_backup_data)) {
			$backup_data_comment = $decoded_backup_data['backup_comment'];
		}
		if (array_key_exists('backup_trigger_source', $decoded_backup_data)) {
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
 * Generates a JSON Configuration backup containing all config data
 * GET /api/backups/configuration/
 * @return string
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
 * Returns a list of JSON Configuration files on a specified device (i.e a alternate storage device)
 * Reuses some above functions to mount check and mount the device
 * Overrides the default behaviour of GetAvailableJSONBackups, so we can check specific devices if needed
 * GET /api/backups/configuration/list/:DeviceName
 * @return string
 */
function GetAvailableJSONBackupsOnDevice(){
	global $SUDO, $settings;
	$deviceName = params('DeviceName');

	//Get the full directory path for the type of directory type we're processing
	$dir_jsonbackupsalternate = GetDirSetting('JsonBackupsAlternate');

	$json_config_backup_filenames = DriveMountHelper($deviceName, 'read_directory_files', array($dir_jsonbackupsalternate, false, true));

	//do some additional massaging of the data
	$json_config_backup_filenames = array_keys($json_config_backup_filenames);

	return json($json_config_backup_filenames);
}

/**
 * Restored the specified JSON Backup
 * POST /api/backups/configuration/restore/:Directory/:BackupFilename
 * @return string
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
				$file_contents = DriveMountHelper($settings['jsonConfigBackupUSBLocation'], 'file_get_contents', array($fullPath));

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
 * Downloads a specific JSON Backup
 * Extracted from file.php GetFile() and modified to deal with mounting the selected USB drive to delete the JSON backup stored on it
 * @return string
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
		//Use our DriveMountHelper to mount the specified USB drive and check if the file exists
		$fileExists = DriveMountHelper($settings['jsonConfigBackupUSBLocation'], 'file_exists', array($fullPath));

		if ($fileExists) {
			//Content type will always be json so see the header
			header("Content-Type: application/json");
			header("Content-Disposition: attachment; filename=\"" . basename($fileName) . "\"");

			//Empty the output buffers
			ob_clean();
			flush();

			DriveMountHelper($settings['jsonConfigBackupUSBLocation'], 'readfile', array($fullPath));
		} else {
			$status = "File Not Found";
			return json(array("Status" => $status, "file" => $fileName, "dir" => $dirName));
		}
	}
}

/**
 * Deletes a specific JSON Backup
 * Extracted from file.php DeleteFile() and modified to deal with mounting the selected USB drive to delete the JSON backup stored on it
 * @return string
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
		//Use our DriveMountHelper to mount the specified USB drive and check if the file exists

		//Mount the drive and see if the file exists
		$fileExists = DriveMountHelper($settings['jsonConfigBackupUSBLocation'], 'file_exists', array($fullPath));
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
			//Use our DriveMountHelper to mount the specified USB drive and check if the file exists
			// Mount the drive and delete the file
			$fileDeleted = DriveMountHelper($settings['jsonConfigBackupUSBLocation'], 'unlink', array($fullPath));

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
 * Callback function to sort backups by their backup time
 * @param $a
 * @param $b
 * @return mixed
 */
function sort_backup_time_asc($a, $b)
{
	return $b['backup_time_unix'] - $a['backup_time_unix'];
}

/**
 * Maps the mount commands return codes to text
 * @param $returnCode
 * @return string
 */
function MountReturnCodeMap($returnCode)
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
