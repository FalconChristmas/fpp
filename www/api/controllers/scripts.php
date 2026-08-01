<?

/**
 * Get all scripts
 *
 * Returns a list of currently installed scripts.
 *
 * @route GET /api/scripts
 * @response 200 List of installed script filenames
 * ```json
 * ["script1.sh", "script2.sh"]
 * ```
 */
function ScriptsList()
{
    global $settings;

    $scripts = array();
    if ($d = opendir($settings['scriptDirectory'])) {
        while (($file = readdir($d)) !== false) {
            if (!is_dir($file)) {
                array_push($scripts, $file);
            }
        }
        closedir($d);
    }

    // Sort them
    natcasesort($scripts);
    $scripts = array_values($scripts);

    return json($scripts);
}

/**
 * Get a script
 *
 * Returns the source code of an installed script.
 *
 * @route GET /api/scripts/{scriptName}
 * @response 200 Script source code
 * ```text
 * The content of the script as a string
 * ```
 */
function ScriptGet()
{
    global $settings;

    $dir = $settings['scriptDirectory'];
    $script = params('scriptName');
    $filename = $dir . '/' . $script;

    $header = "Content-type: text/plain";
    send_header($header);
    file_read($filename, false);
}

/**
 * Update script
 *
 * Writes the `POST` request body to the file specified by `{scriptName}`.
 *
 * @route POST /api/scripts/{scriptName}
 * @response 200 Script saved
 * ```json
 * {
 *   "status": "OK",
 *   "scriptName": "test.py",
 *   "scriptBody": "#!/usr/bin/python\n\nprint(\"hi There Matt!\");\n"
 * }
 * ```
 */
function ScriptSave()
{
    global $settings;
    $scriptName = params("scriptName");
    $json = strval(file_get_contents('php://input'));
    $content = json_decode($json, true);
    $filename = $settings['scriptDirectory'] . '/' . $scriptName;
    $result = array();

    if (file_exists($filename)) {
        //error output is silenced by @, function returns false on failure, it doesn't return true
        $script_save_result = @file_put_contents($filename, $content);
        //check result is not a error
        if ($script_save_result !== false) {
            $result['status'] = "OK";
            $result['scriptName'] = $scriptName;
            $result['scriptBody'] = $content;

			//Trigger a JSON Configuration Backup
			GenerateBackupViaAPI('Script ' . $scriptName . ' was modified.');
        } else {
            $script_writable = is_writable($filename);
            $script_directory_writable = is_writable($settings['scriptDirectory']);

            $result['status'] = "Error updating file";
            error_log("SaveScript: Error updating file - " . $scriptName . " ($filename) ");
            error_log("SaveScript: Error updating file - " . $scriptName . " ($filename) " . " -> isWritable: " . $script_writable);
            error_log("SaveScript: Error updating file - " . $scriptName . " ($filename) " . " -> Scripts directory is writable: " . $script_directory_writable);
        }
    } else {
        $result['status'] = "Error, file does not exist";
        error_log("SaveScript: Error, file does not exist - " . $scriptName . " -> " . $filename);
    }

    return json($result);
}

/**
 * Run script
 *
 * Runs a locally installed script.
 *
 * @route POST /api/scripts/{scriptName}/run
 * @response 200 Script output
 * ```text
 * The output of the script as a String
 * ```
 */
function ScriptRun()
{
    global $settings;

    $script = params('scriptName');
    $curl = curl_init('http://localhost:32322/command/Run%20Script/' . $script);
    curl_setopt($curl, CURLOPT_FAILONERROR, true);
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 200);
    $request_content = curl_exec($curl);
    return $request_content;
}

/**
 * Get remote script
 *
 * Returns the source code of a remote script from the script repository.
 *
 * @route GET /api/scripts/viewRemote/{category}/{filename}
 * @response 200 Remote script source code
 * ```text
 * The content of the script as a string
 * ```
 */
function ScriptsViewRemote()
{
    $category = params('category');
    $filename = params('filename');

    $script = file_get_contents("https://raw.githubusercontent.com/FalconChristmas/fpp-scripts/master/" . $category . "/" . $filename);

    echo $script;
}

/**
 * Install remote script
 *
 * Installs a remote script from the script repository.
 *
 * @route POST /api/scripts/installRemote/{category}/{filename}
 * @response 200 Remote script installed
 * ```json
 * {"status": "OK"}
 * ```
 */
function ScriptsInstallRemote()
{
    global $fppDir, $SUDO;
    global $scriptDirectory;

    $category = params('category');
    $filename = params('filename');

    exec("$SUDO $fppDir/scripts/installScript \"$category\" \"$filename\"");

	//Trigger a JSON Configuration Backup
	GenerateBackupViaAPI($category . ' script ' . $filename . ' content was installed.');

    return json(array("status" => "OK"));
}
