<?


/////////////////////////////////////////////////////////////////////////////
// GET /api/files/:DirName
function GetFiles() {
    global $settings;
    $files = Array();
    $subDir = params('DirName');
    
    $dir = $settings['mediaDirectory'] . '/' . $subDir;
    foreach (glob($dir . "/*") as $filename) {
        array_push($files, basename($filename));
    }

    sort($files);

    return json($files);
}

?>
