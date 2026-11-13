<?

/**
 * Get all sequences
 *
 * Returns a list of all `*.fseq` sequence files.
 *
 * @route GET /api/sequence
 * @response 200 List of sequence names
 * ```json
 * ["GreatestShow", "StPatricksDay", "Valentine"]
 * ```
 */
function GetSequences()
{
    global $settings;
    $sequences = array();

    $dir = $settings['sequenceDirectory'];
    foreach (glob($dir . "/*.fseq") as $filename) {
        array_push($sequences, basename($filename, ".fseq"));
    }

    return json($sequences);
}

/**
 * Download sequence
 *
 * Downloads the `*.fseq` file for the named sequence.
 *
 * @route GET /api/sequence/{SequenceName}
 * @response 200 Raw FSEQ file download
 * ```bytes
 * [Content-Type: application/octet-stream]
 * ```
 * @response 404 Sequence not found
 * ```text
 * Not found: {SequenceName}
 * ```
 */
function GetSequence()
{
    global $settings;

    $sequence = params('SequenceName');
    if ((substr($sequence, -5) != ".fseq") && (substr($sequence, -5) != ".eseq")) {
        $sequence = $sequence . ".fseq";
    }
    $dir = FSeqOrEseqDirectory($sequence);
    $name = findFile($dir, $sequence);
    if ($name === false) {
        http_response_code(400);
        return json(array('Status' => 'Error', 'Message' => 'Invalid sequence name.'));
    }
    $file = $dir . '/' . $name;
    if (file_exists($file)) {
        if (ob_get_level()) {
            ob_end_clean();
        }
        header('Content-Description: File Transfer');
        header('Content-Type: application/octet-stream');
        header('Content-Disposition: attachment; filename="' . SanitizeDownloadFilename(basename($file)) . '"');
        header('Expires: 0');
        header('Cache-Control: must-revalidate');
        header('Pragma: public');
        //No Content-Length: Apache drops a FastCGI reply's (see GetFileImpl)
        readfile($file);
    } else {
        halt(404, "Not found: " . $sequence);
    }
}

/**
 * Get sequence metadata
 *
 * Returns `name`, `version`, `id`, `time`, and other details from the `*.fseq` file for the named sequence.
 *
 * @route GET /api/sequence/{SequenceName}/meta
 * @response 200 Sequence metadata
 * ```json
 * {
 *   "Name": "GreatestShow.fseq",
 *   "Version": "2.0",
 *   "ID": "1553194098754908",
 *   "StepTime": 25,
 *   "NumFrames": 10750,
 *   "MaxChannel": 84992,
 *   "ChannelCount": 84992
 * }
 * ```
 * @response 404 Sequence not found
 * ```text
 * Not found: {SequenceName}
 * ```
 */
function GetSequenceMetaData()
{
    global $settings, $fppDir;
    $sequence = params('SequenceName');
    if ((substr($sequence, -5) != ".fseq") && (substr($sequence, -5) != ".eseq")) {
        $sequence = $sequence . ".fseq";
    }
    $dir = FSeqOrEseqDirectory($sequence);
    $name = findFile($dir, $sequence);
    if ($name === false) {
        http_response_code(400);
        return json(array('Status' => 'Error', 'Message' => 'Invalid sequence name.'));
    }
    $file = $dir . '/' . $name;
    if (file_exists($file)) {
        $cmd = $fppDir . "/src/fsequtils -j " . escapeshellarg($file) . " 2> /dev/null";
        exec($cmd, $output);
        if (isset($output[0])) {
            $js = json_decode($output[0]);
            return json($js);
        } else {
            $data = array();
            $data['Name'] = $sequence;
            $data['Version'] = '?.?';
            $data['ID'] = '';
            $data['StepTime'] = -1;
            $data['NumFrames'] = -1;
            $data['MaxChannel'] = -1;
            $data['ChannelCount'] = -1;
            return json($data);
        }
    }
    halt(404, "Not found: " . $file);
}

/**
 * Uploads sequence file
 *
 * Uploads a new `*.fseq` sequence file. It replaces a sequence of the same
 * name only once all of it has arrived; one that is playing carries on with
 * the old version until it is next opened.
 *
 * @route POST /api/sequence/{SequenceName}
 * @body "(Raw FSEQ file data)"
 * @response 200 Sequence uploaded
 * ```json
 * {"Status": "OK", "Message": ""}
 * ```
 * @response 400 The request was empty or cut off, or the name is invalid
 * ```json
 * {"Status": "Error", "Message": "The request held no sequence data."}
 * ```
 * @response 409 A directory of that name is in the way
 * ```json
 * {"Status": "Error", "Message": "A directory of that name is in the way."}
 * ```
 * @response 500 The sequence could not be written
 * ```json
 * {"Status": "Error", "Message": "Could not store the sequence."}
 * ```
 */
function PostSequence()
{
    global $settings;
    $sequence = params('SequenceName');
    if ((substr($sequence, -5) != ".fseq") && (substr($sequence, -5) != ".eseq")) {
        $sequence = $sequence . ".fseq";
    }
    $dir = FSeqOrEseqDirectory($sequence);
    $name = findFile($dir, $sequence);
    if ($name === false) {
        http_response_code(400);
        return json(array('Status' => 'Error', 'Message' => 'Invalid sequence name.'));
    }
    $file = $dir . '/' . $name;
    if (is_dir($file)) {
        return PostSequenceError(409, 'A directory of that name is in the way.');
    }

    // fppd and the chunked upload look for <name>.replace beside the name.
    $replacement = $file . '.replace';

    // A sequence linked in from elsewhere (USB media) is replaced where it lives.
    if (is_link($file) && ($real = realpath($file)) !== false) {
        $file = $real;
    }

    // Written beside the target and renamed over it, so a request that is
    // empty, cut off or can't be written leaves the existing file alone, and
    // fppd keeps reading the old file if it has it open.
    $tmp = dirname($file) . '/.' . basename($file) . '.upload.' . getmypid();
    register_shutdown_function(function () use ($tmp) {
        @unlink($tmp);
    });
    $in = fopen("php://input", "rb");
    $out = @fopen($tmp, "wb");
    if ($in === false || $out === false) {
        return PostSequenceError(500, 'Could not open the sequence for writing.');
    }
    $size = 0;
    $ok = true;
    while (!feof($in)) {
        $data = fread($in, 1024 * 64);
        if ($data === false || ($data !== '' && fwrite($out, $data) !== strlen($data))) {
            $ok = false;
            break;
        }
        $size += strlen($data);
    }
    fclose($in);
    if ($ok && function_exists('fsync')) {
        $ok = fsync($out);
    }
    if (!fclose($out) || !$ok) {
        @unlink($tmp);
        return PostSequenceError(500, 'Could not write the sequence: the disk may be full (an upload needs room for a second copy until it replaces the old one).');
    }
    $expected = isset($_SERVER['CONTENT_LENGTH']) && $_SERVER['CONTENT_LENGTH'] !== '' ? (int) $_SERVER['CONTENT_LENGTH'] : -1;
    if ($expected > 0 && $size != $expected) {
        @unlink($tmp);
        if ($size > 0) {
            return PostSequenceError(400, "The upload was cut off ($size of $expected bytes arrived).");
        }
        if (stripos($_SERVER['CONTENT_TYPE'] ?? '', 'multipart/') === 0) {
            return PostSequenceError(400, 'Send the sequence file itself as the request body, not as a form.');
        }
        return PostSequenceError(400, 'No sequence data arrived; it may be larger than the upload limit (' . ini_get('post_max_size') . ').');
    }
    if ($size == 0) {
        @unlink($tmp);
        return PostSequenceError(400, 'The request held no sequence data.');
    }

    if (file_exists($file)) {
        @chmod($tmp, fileperms($file) & 0777);
    }
    if (!rename($tmp, $file)) {
        @unlink($tmp);
        return PostSequenceError(500, 'Could not store the sequence.');
    }
    // fppd swaps a <name>.replace in when it next opens the sequence; one left
    // by an earlier upload is older than this one.
    @unlink($replacement);
    @unlink($file . '.replace');

    return json(array('Status' => 'OK', 'Message' => ''));
}

function PostSequenceError($code, $message)
{
    http_response_code($code);
    return json(array('Status' => 'Error', 'Message' => $message));
}

/**
 * Delete sequence file
 *
 * Deletes the named `*.fseq` sequence file.
 *
 * @route DELETE /api/sequence/{SequenceName}
 * @response 200 Sequence deleted
 * ```json
 * {"Status": "OK", "Message": ""}
 * ```
 */
function DeleteSequences()
{
    global $settings;
    $sequence = params('SequenceName');
    if ((substr($sequence, -5) != ".fseq") && (substr($sequence, -5) != ".eseq")) {
        $sequence = $sequence . ".fseq";
    }
    $dir = FSeqOrEseqDirectory($sequence);
    $name = findFile($dir, $sequence);
    if ($name === false) {
        http_response_code(400);
        return json(array('Status' => 'Error', 'Message' => 'Invalid sequence name.'));
    }
    $file = $dir . '/' . $name;
    if (file_exists($file)) {
        unlink($file);
    }

    $resp = array();
    $resp['Status'] = 'OK';
    $resp['Message'] = '';

    return json($resp);
}

/**
 * Start sequence at time
 *
 * Starts the given sequence at the specified time frame. Only intended for testing. In most
 * situations, use the "Start Playlist" command from the command API and pass the sequence
 * name as the playlist name.
 *
 * @badge "FPP REQUIRED" critical
 * @badge "DEVELOPER ONLY" info
 *
 * @route POST /api/sequence/{SequenceName}/start/{startSecond}
 * @route GET /api/sequence/{SequenceName}/start/{startSecond} deprecated
 * @response 200 Sequence started
 * ```json
 * {
 *   "status": "OK",
 *   "SequenceName": "single_line.fseq",
 *   "startSecond": "9"
 * }
 * ```
 */
function GetSequenceStart()
{
    global $settings;

    $sequence = params('SequenceName');

    if (substr($sequence, -5) == ".eseq") {
        $rc = array("status" => "ERROR: This API call does only supports Sequences, not Effects");
        return json($rc);
    }

    if (substr($sequence, -5) != ".fseq") {
        $sequence = $sequence . ".fseq";
    }
    $dir = $settings['sequenceDirectory'];
    $sequence = findFile($dir, $sequence);
    if ($sequence === false) {
        return json(array("status" => "ERROR: Invalid sequence name"));
    }

    $startSecond = params('startSecond');

    SendCommand(sprintf("StartSequence,%s,%d", $sequence, $startSecond));

    $rc = array("status" => "OK", "SequenceName" => $sequence, "startSecond" => $startSecond);
    return json($rc);

}

/**
 * Step a paused sequence
 *
 * If the sequence was paused via `sequence/current/togglePause`, steps the sequence forward one frame.
 *
 * @badge "FPP REQUIRED" critical
 * @route GET /api/sequence/current/step
 * @response 200 Sequence stepped
 * ```json
 * {"status": "OK"}
 * ```
 */
function GetSequenceStep()
{
    SendCommand("SingleStepSequence");

    $rc = array("status" => "OK");
    return json($rc);

}

/**
 * Toggle play/pause on sequence
 *
 * Pauses or resumes the currently playing sequence. Only valid if the sequence was started via
 * `/api/sequence/{SequenceName}/start/{startSecond}`.
 *
 * @badge "FPP REQUIRED" critical
 * @badge "DEVELOPER ONLY" info
 * @route POST /api/sequence/current/togglePause
 * @route GET /api/sequence/current/togglePause deprecated
 * @response 200 Sequence play/pause toggled
 * ```json
 * {"status": "OK"}
 * ```
 */
function GetSequenceTogglePause()
{
    SendCommand("ToggleSequencePause");
    $rc = array("status" => "OK");
    return json($rc);
}

/**
 * Stop sequence
 *
 * Stops the currently playing sequence. Only valid if the sequence was started via
 * `/api/sequence/{SequenceName}/start/{startSecond}`.
 *
 * @badge "FPP REQUIRED" critical
 * @badge "DEVELOPER ONLY" info
 * @route POST /api/sequence/current/stop
 * @route GET /api/sequence/current/stop deprecated
 * @response 200 Sequence stopped
 * ```json
 * {"status": "OK"}
 * ```
 */
function GetSequenceStop()
{
    SendCommand("StopSequence");
    $rc = array("status" => "OK");
    return json($rc);

}

/**
 * Returns the sequence directory for a given sequence filename based on its extension.
 *
 * @param string $seq Sequence filename including extension (`.fseq` or `.eseq`).
 * @return string Absolute path to the directory containing the sequence file.
 */
function FSeqOrEseqDirectory($seq)
{
    global $settings;
    if (substr($seq, -5) == ".fseq") {
       return $settings['sequenceDirectory'];
    } else if (substr($seq, -5) == ".eseq") {
       return $settings['effectDirectory'];
    }
}
