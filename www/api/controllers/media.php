<?

// This path is relative to the api directory, not the controllers directory
require_once '../common/metadata.php';

/////////////////////////////////////////////////////////////////////////////
// GET /api/media
function GetMedia() {
    global $settings;
    $files = Array();
    
    $dir = $settings['musicDirectory'];
    foreach (glob($dir . "/*") as $filename) {
        array_push($files, basename($filename));
    }
    
    $dir = $settings['videoDirectory'];
    foreach (glob($dir . "/*") as $filename) {
        array_push($files, basename($filename));
    }

    sort($files);

    return json($files);
}

/////////////////////////////////////////////////////////////////////////////
// GET /api/media/:MediaName/duration
function GetMediaDuration() {
    global $settings;
    $resp = Array();

    $file = params('MediaName');

    $resp = getMediaDurationInfo($file, true);

    return json($resp);
}

/////////////////////////////////////////////////////////////////////////////
// GET /api/media/:MediaName/meta
function GetMediaMetaData() {
    global $settings;
    $resp = Array();

    $file = params('MediaName');

    $resp = GetMetaDataFromFFprobe($file);

    return json($resp);
}

?>
