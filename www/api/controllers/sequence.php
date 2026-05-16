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
    $dir = fSeqOrEseqDirectory($sequence);
    $file = $dir . '/' . findFile($dir, $sequence);
    if (file_exists($file)) {
        if (ob_get_level()) {
            ob_end_clean();
        }
        header('Content-Description: File Transfer');
        header('Content-Type: application/octet-stream');
        header('Content-Disposition: attachment; filename="' . basename($file) . '"');
        header('Expires: 0');
        header('Cache-Control: must-revalidate');
        header('Pragma: public');
        header('Content-Length: ' . real_filesize($file));
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
    $dir = fSeqOrEseqDirectory($sequence);
    $file = $dir . '/' . findFile($dir, $sequence);
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
 * Uploads a new `*.fseq` sequence file.
 *
 * @route POST /api/sequence/{SequenceName}
 * @body "(Raw FSEQ file data)"
 * @response 200 Sequence uploaded
 * ```json
 * {"Status": "OK", "Message": ""}
 * ```
 */
function PostSequence()
{
    global $settings;
    $sequence = params('SequenceName');
    if ((substr($sequence, -5) != ".fseq") && (substr($sequence, -5) != ".eseq")) {
        $sequence = $sequence . ".fseq";
    }
    $dir = fSeqOrEseqDirectory($sequence);
    $file = $dir . '/' . findFile($dir, $sequence);

    $putdata = fopen("php://input", "r");
    $fp = fopen($file, "w");
    while ($data = fread($putdata, 1024 * 16)) {
        fwrite($fp, $data);
    }
    fclose($fp);
    fclose($putdata);

    $resp = array();
    $resp['Status'] = 'OK';
    $resp['Message'] = '';

    return json($resp);
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
function DeleteSequence()
{
    global $settings;
    $sequence = params('SequenceName');
    if ((substr($sequence, -5) != ".fseq") && (substr($sequence, -5) != ".eseq")) {
        $sequence = $sequence . ".fseq";
    }
    $dir = fSeqOrEseqDirectory($sequence);
    $file = $dir . '/' . findFile($dir, $sequence);
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
 * @route POST /api/sequence/current/step
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
function fSeqOrEseqDirectory($seq)
{
    global $settings;
    if (substr($seq, -5) == ".fseq") {
       return $settings['sequenceDirectory'];
    } else if (substr($seq, -5) == ".eseq") {
       return $settings['effectDirectory'];
    }
}
