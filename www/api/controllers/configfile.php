<?

/**
 * Validate that a requested config path stays within the config directory.
 * Returns the contained absolute path on success, false otherwise.
 */
function ConfigFileWithinBase($baseDir, $requested) {
    $base = realpath($baseDir);
    if ($base === false) return false;
    $candidate = $base . '/' . $requested;
    $parent = dirname($candidate);
    $realParent = realpath($parent);
    if ($realParent === false) {
        $parts = explode('/', $requested);
        array_pop($parts);
        $cur = $base;
        foreach ($parts as $p) {
            if ($p === '' || $p === '.') continue;
            if (strpos($p, '..') !== false) return false;
            $cur .= '/' . $p;
            $rp = realpath($cur);
            if ($rp !== false && strpos($rp, $base) !== 0) return false;
        }
        $rpBase = $realParent !== false ? $realParent : $base;
        return strpos($rpBase, $base) === 0 ? $base . '/' . $requested : false;
    }
    $realCandidate = realpath($candidate);
    if ($realCandidate !== false) return strpos($realCandidate, $base) === 0 ? $realCandidate : false;
    return strpos($realParent, $base) === 0 ? $realParent . '/' . basename($candidate) : false;
}
function ConfigFileValidateOrFail($baseDir, $requested) {
    if ($requested === '' || strpos($requested, '\\') !== false || strpos($requested, "\0") !== false) { http_response_code(400); echo json_encode(["Status"=>"Error","Message"=>"Invalid path"]); exit; }
    $decoded = rawurldecode($requested);
    if (strpos($requested, '..') !== false || strpos($decoded, '..') !== false || (isset($decoded[0]) && $decoded[0] === '/') || preg_match('/%2e/i', $requested)) { http_response_code(403); echo json_encode(["Status"=>"Error","Message"=>"Invalid path"]); exit; }
    $san = ConfigFileWithinBase($baseDir, $requested);
    if ($san === false) { http_response_code(403); echo json_encode(["Status"=>"Error","Message"=>"Invalid path: outside config directory"]); exit; }
    return $san;
}

/**
 * Get files
 *
 * Recursively collects relative file paths within a directory.
 *
 * @param string $dir    Absolute base directory path.
 * @param string $subdir Current subdirectory relative to $dir (used during recursion).
 * @return array Flat array of relative file paths found under $dir.
 */
function GetFilesInDir($dir, $subdir = '')
{
	$result = array();

	if ($subdir != '')
		$subdir .= '/';

	foreach (scandir($dir . '/' . $subdir) as $file) {
		if ($file != '.' && $file != '..') {
			if (is_dir($dir . '/' . $subdir . $file))
				$result = array_merge($result, GetFilesInDir($dir, $subdir . $file));
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
	$base = $settings['configDirectory'];
	if ($dir == '')
		$dir = $base;
	else
		$dir = ConfigFileValidateOrFail($base, $dir);

	$result = array();

	$files = GetFilesInDir($dir);

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

	$raw = params(0);
	$base = $settings['configDirectory'];
	$probe = $base . '/' . $raw;
	$realProbe = realpath($probe);
	if ($realProbe !== false && is_dir($realProbe)) {
		ConfigFileValidateOrFail($base, $raw);
		return GetConfigFileList($raw);
	}
	$fileName = ConfigFileValidateOrFail($base, $raw);
	render_file($fileName);
}

/**
 * Config file already holds exactly this content
 *
 * @param string $path Absolute path to the existing config file.
 * @param string $data Content that was uploaded.
 * @return bool True if writing $data to $path would change nothing.
 */
function ConfigFileIsUnchanged($path, $data)
{
	if (!is_file($path)) {
		return false;
	}

	$cur = @file_get_contents($path);

	return ($cur !== false) && ($cur === $data);
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
	$base = $settings['configDirectory'];
	$fileName = ConfigFileValidateOrFail($base, $baseFile);
	$subDir = dirname($fileName);
	if ($subDir !== $base && !is_dir($subDir)) mkdir($subDir, 0755, true);

	// Read the full contents (multipart upload or raw POST body) and write them
	// atomically. The old code truncated the destination in place and streamed
	// into it, leaving a window where a reader (e.g. fppd reloading channel
	// outputs when co-universes.json changes) could see an empty/partial file and
	// fail to parse it. WriteFileAtomic() writes a temp file then rename()s it
	// over the destination so readers only ever see the complete old or new file.
	if (isset($_FILES['file']) && isset($_FILES['file']['tmp_name'])) {
		$data = @file_get_contents($_FILES['file']['tmp_name']);
	} else {
		$data = @file_get_contents("php://input");
	}

	// An identical upload is not written at all. WriteFileAtomic() finishes with a
	// rename() over the destination and fppd's FileMonitor watches the config
	// directory for exactly that, so rewriting the same bytes still tears down and
	// rebuilds every channel output loaded from the file -- PRU firmware reloads
	// included. xLights sends the whole controller config on each "output to
	// lights" when auto-upload is on, and most of those uploads change nothing.
	$unchanged = false;

	if ($data === false) {
		$result['Status'] = 'Error';
		$result['Message'] = 'Unable to read uploaded data';
	} else if (ConfigFileIsUnchanged($fileName, $data)) {
		$unchanged = true;
		$result['Status'] = 'OK';
		$result['Message'] = '';
	} else if (WriteFileAtomic($fileName, $data)) {
		$result['Status'] = 'OK';
		$result['Message'] = '';
	} else {
		$result['Status'] = 'Error';
		$result['Message'] = 'Unable to write file';
	}

	if ($result['Status'] == 'OK' && !$unchanged) {
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
	$base = $settings['configDirectory'];
	$fullFile = ConfigFileValidateOrFail($base, $fileName);

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