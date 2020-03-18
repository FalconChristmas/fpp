<?


/////////////////////////////////////////////////////////////////////////////
// GET /api/sequence
function GetSequences() {
    global $settings;
    $sequences = Array();
    
    $dir = $settings['sequenceDirectory'];
    foreach (glob($dir . "/*.fseq") as $filename) {
        array_push($sequences, basename($filename, ".fseq"));
    }
    
    
    return json($sequences);
}

/////////////////////////////////////////////////////////////////////////////
// GET /api/sequence/:SequenceName
function GetSequence() {
    global $settings;
    $sequence = params('SequenceName');
    $file = $settings['sequenceDirectory'] . "/" . $sequence;
    if (!file_exists($file)) {
        $file = $file . ".fseq";
    }
    if (file_exists($file)) {
        if (ob_get_level()) {
            ob_end_clean();
        }
        header('Content-Description: File Transfer');
        header('Content-Type: application/octet-stream');
        header('Content-Disposition: attachment; filename="'.basename($file).'"');
        header('Expires: 0');
        header('Cache-Control: must-revalidate');
        header('Pragma: public');
        header('Content-Length: ' . filesize($file));
        readfile($file);
    }
}
/////////////////////////////////////////////////////////////////////////////
// GET /api/sequence/:SequenceName/meta
function GetSequenceMetaData() {
    global $settings;
    $sequence = params('SequenceName');
    $file = $settings['sequenceDirectory'] . "/" . $sequence;
    if (substr( $file, -5 ) != ".fseq") {
        $file = $file . ".fseq";
    }
    if (file_exists($file)) {
        $cmd = "/opt/fpp/src/fsequtils -j \"$file\" 2>&1";
        exec( $cmd, $output);
        $js = json_decode($output[0]);
        return json($js);
    }
    halt(404, "Not found");
}

/////////////////////////////////////////////////////////////////////////////
// POST /api/sequence/:SequenceName
function PostSequence() {
    global $settings;
    $sequence = params('SequenceName');
    $file = $settings['sequenceDirectory'] . "/" . $sequence;
    if (substr( $file, -5 ) != ".fseq") {
        $file = $file . ".fseq";
    }
    
    $putdata = fopen("php://input", "r");
    $fp = fopen($file, "w");
    while ($data = fread($putdata, 1024*16)) {
        fwrite($fp, $data);
    }
    fclose($fp);
    fclose($putdata);

    $resp = Array();
    $resp['Status'] = 'OK';
    $resp['Message'] = '';

    return json($resp);
}

/////////////////////////////////////////////////////////////////////////////
// DELETE /api/sequence/:Sequence
function DeleteSequences() {
    global $settings;
    $sequence = params('SequenceName');
    $file = $settings['sequenceDirectory'] . "/" . $sequence;
    if (!file_exists($file)) {
        $file = $file . ".fseq";
    }
    if (file_exists($file)) {
        unlink($file);
    }

    $resp = Array();
    $resp['Status'] = 'OK';
    $resp['Message'] = '';

    return json($resp);
}


?>
