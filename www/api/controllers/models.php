<?
/* This file will eventually (possibly) contain API's for interacting
 * with the memory mapped model definitions.   For now, it only
 * has a single POST to overwrite the current file
 */
/////////////////////////////////////////////////////////////////////////////

function post_models_raw()
{
    global $settings;
    
    $memmaps = file_get_contents("php://input");
    $filename = $settings['mediaDirectory'] . '/channelmemorymaps';
    
    $f = fopen($filename, "w");
    if ($f) {
        fwrite($f, $memmaps);
        fclose($f);
    }
    
    $stat['Status'] = 'OK';
    $stat['Message'] = 'File saved';
    return json($playlist);
}

?>
