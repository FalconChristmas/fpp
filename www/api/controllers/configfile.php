<?

/**
 * Get files
 *
 * Recursively collects relative file paths within a directory.
 *
 * @param string $dir    Absolute base directory path.
 * @param string $subdir Current subdirectory relative to $dir (used during recursion).
 * @return array Flat array of relative file paths found under $dir.
 */
function getFilesInDir($dir, $subdir = '')
{
	$result = array();

	if ($subdir != '')
		$subdir .= '/';

	foreach (scandir($dir . '/' . $subdir) as $file) {
		if ($file != '.' && $file != '..') {
			if (is_dir($dir . '/' . $subdir . $file))
				$result = array_merge($result, getFilesInDir($dir, $subdir . $file));
			else
				array_push($result, $subdir . $file);
		}
	}

	return $result;
}

/**
 * Get directory list
 *
 * Returns a list of config files in `/home/fpp/media/config` or an optional subdirectory.
 *
 * @route GET /api/configfile
 * @response 200 Directory listing
 * ```json
 * {
 *   "Path": "",
 *   "ConfigFiles": ["File1", "File2", "File3"]
 * }
 * ```
 */
function GetConfigFileList($dir = '')
{
	global $settings;

	$origDir = $dir;
	if ($dir == '')
		$dir = $settings['configDirectory'];
	else
		$dir = $settings['configDirectory'] . '/' . $dir;

	$result = array();

	$files = getFilesInDir($dir);

	$result['Path'] = $origDir;
	$result['ConfigFiles'] = $files;

	return json($result);
}

/**
 * Get file or directory list
 *
 * Returns the contents of a specific config file, or a directory listing if
 * the path resolves to a directory.
 *
 * @route GET /api/configfile/**
 * @response 200 Raw config file contents
 * ```text
 * (Raw config file contents)
 * ```
 */
function DownloadConfigFile()
{
	global $settings;

	$fileName = params(0);

	if (is_dir($settings['configDirectory'] . '/' . $fileName))
		return GetConfigFileList($fileName);

	$fileName = $settings['configDirectory'] . '/' . $fileName;

	render_file($fileName);
}

/**
 * Upload configuration file
 *
 * Uploads or overwrites a config file in `/home/fpp/media/config`, creating any
 * necessary subdirectories. Accepts a multipart file upload or raw `POST` body.
 *
 * @route POST /api/configfile/**
 * @body "(Raw config file contents)"
 * @response 200 File uploaded
 * ```json
 * {"Status": "OK", "Message": ""}
 * ```
 */
function UploadConfigFile()
{
	global $settings;

	$result = array();

	$baseFile = params(0);
	if (preg_match('/\//', $baseFile)) {
		// baseFile contains a subdir, so create if needed
		$subDir = $settings['configDirectory'] . '/' . $baseFile;
		$subDir = preg_replace('/\/[^\/]*$/', '', $subDir);

		mkdir($subDir, 0755, true);
	}

	$fileName = $settings['configDirectory'] . '/' . $baseFile;

	if (isset($_FILES['file']) && isset($_FILES['file']['tmp_name'])) {
		if (rename($_FILES["file"]["tmp_name"], $fileName)) {
			$result['Status'] = 'OK';
			$result['Message'] = '';
		} else {
			$result['Status'] = 'Error';
			$result['Message'] = 'Unable to rename uploaded file';
		}
	} else if ($f = fopen($fileName, "c")) {
		flock($f, LOCK_EX); // Lock the file for writing
		ftruncate($f, 0); // Clear the file before writing
		$postdata = fopen("php://input", "r");
		while ($data = fread($postdata, 1024 * 16)) {
			fwrite($f, $data);
		}
		fclose($postdata);
		fclose($f);

		$result['Status'] = 'OK';
		$result['Message'] = '';
	} else {
		$result['Status'] = 'Error';
		$result['Message'] = 'Unable to open file for writing';
	}

	if ($result['Status'] == 'OK') {
		//Trigger a JSON Configuration Backup
		GenerateBackupViaAPI('Config File ' . $baseFile . ' was uploaded/modified.', 'config_file/' . $baseFile);
	}

	if ($baseFile == 'authorized_keys') {
		system("sudo /opt/fpp/scripts/installSSHKeys");
	}

	// Clear locale cache when user-holidays.json is updated
	if ($baseFile == 'user-holidays.json') {
		SendCommand('Clear Locale Cache,true');
	}

	return json($result);
}

/**
 * Delete configuration file
 *
 * Deletes a config file from `/home/fpp/media/config`.
 *
 * @route DELETE /api/configfile/**
 * @response 200 File deleted
 * ```json
 * {"Status": "OK", "Message": ""}
 * ```
 */
function DeleteConfigFile()
{
	global $settings;
	$result = array();

	$fileName = params(0);

	$fullFile = $settings['configDirectory'] . '/' . $fileName;

	if (is_file($fullFile)) {
		unlink($fullFile);
		if (is_file($fullFile)) {
			$result['Status'] = 'Error';
			$result['Message'] = 'Unable to delete ' . $fileName;
		} else {
			$result['Status'] = 'OK';
			$result['Message'] = '';
		}
	} else {
		$result['Status'] = 'Error';
		$result['Message'] = $fileName . ' is not a regular file';
	}

	return json($result);
}

?>