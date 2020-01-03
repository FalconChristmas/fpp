<?

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
    return json($scripts);
}
function script_get()
{
    global $settings;

    $dir = $settings['scriptDirectory'];
    $script = params('scriptName');
    $filename = $dir . '/' . $script;

    $header = "Content-type: text/plain";
    send_header($header);
    file_read($filename, false);
}

function script_run()
{
    global $settings;

    $script = params('scriptName');
    $curl = curl_init('http://localhost:32322/command/Run Script/' . $script);
    curl_setopt($curl, CURLOPT_FAILONERROR, true);
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 200);
    $request_content = curl_exec($curl);
    return $request_content;
}

?>
