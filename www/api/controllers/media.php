<?

// This path is relative to the api directory, not the controllers directory
require_once '../common/metadata.php';

/////////////////////////////////////////////////////////////////////////////
// GET /api/media
function GetMedia()
{
    global $settings;
    $files = array();

    // Use scandir instead of glob for better performance
    $dirs = [$settings['musicDirectory'], $settings['videoDirectory']];
    foreach ($dirs as $dir) {
        if (is_dir($dir)) {
            foreach (scandir($dir) as $file) {
                if ($file !== '.' && $file !== '..' && is_file($dir . '/' . $file)) {
                    $files[] = $file;
                }
            }
        }
    }

    sort($files);

    return json($files);
}


/////////////////////////////////////////////////////////////////////////////
// GET /api/media/:MediaName/duration
function GetMediaDuration()
{
    global $settings;
    $resp = array();

    $file = params('MediaName');

    $resp = getMediaDurationInfo($file, true);

    if ($resp[$file]['duration'] < 0) {
        halt(404, "Not found: " . $file);
    } else {
        return json($resp);
    }

}

/////////////////////////////////////////////////////////////////////////////
// GET /api/media/:MediaName/meta
function GetMediaMetaData()
{
    global $settings;
    $resp = array();

    $file = params('MediaName');

    $resp = GetMetaDataFromFFProbe($file);

    return json($resp);
}
