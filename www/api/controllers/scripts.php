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
function scripts_list()
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
function script_get()
{
    global $settings;

    $dir = $settings['scriptDirectory'];
    $script = params('scriptName');
    // Zero-risk traversal block: reject .., /, \, null — covers ../../etc/passwd without allow-list
    if (strpos($script, '..') !== false || strpos($script, '/') !== false || strpos($script, '\\') !== false || strpos($script, "\0") !== false) {
        http_response_code(400);
        return json(array("status" => "Invalid scriptName"));
    }
    $filename = $dir . '/' . $script;

    $header = "Content-type: text/plain";
    send_header($header);
    file_read($filename, false);
}

/**
 * Update script
 *
 * Writes the `POST` request body to the file specified by `{scriptName}`.
 * A body that is a JSON string (as the bundled script editor sends) is decoded
 * first; any other body is written as-is.
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
function script_save()
{
    global $settings;
    $scriptName = params("scriptName");
    if (strpos($scriptName, '..') !== false || strpos($scriptName, '/') !== false || strpos($scriptName, '\\') !== false || strpos($scriptName, "\0") !== false) {
        http_response_code(400);
        return json(array("status" => "Invalid scriptName"));
    }
    $body = file_get_contents('php://input');
    if (!is_string($body)) {
        $body = '';
    }
    // The bundled editor POSTs the script JSON-encoded (JSON.stringify), and
    // clients following the docblock POST it as the raw body.  Only unwrap a
    // body that decodes to a JSON string; anything else is the script itself.
    // Decoding a raw body yields null, which used to truncate the file to
    // zero bytes.
    $content = $body;
    $decoded = json_decode($body, true);
    if (is_string($decoded)) {
        $content = $decoded;
    }
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

    // scriptBody can hold arbitrary bytes from a raw request body; without the
    // substitute flag json_encode() would fail and the reply would be empty.
    return json($result, JSON_INVALID_UTF8_SUBSTITUTE);
}

/**
 * Run script
 *
 * Runs a locally installed script.
 *
 * @route POST /api/scripts/{scriptName}/run
 * @route GET /api/scripts/{scriptName}/run deprecated
 * @response 200 Script output
 * ```text
 * The output of the script as a String
 * ```
 */
function script_run()
{
    global $settings;

    $script = params('scriptName');
    if (strpos($script, '..') !== false || strpos($script, '/') !== false || strpos($script, '\\') !== false || strpos($script, "\0") !== false) {
        http_response_code(400);
        return json(array("status" => "Invalid scriptName"));
    }
    $request_content = FppdCommand('Run Script', array($script));
    return $request_content;
}

