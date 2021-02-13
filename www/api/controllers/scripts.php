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

// GET /api/scripts/viewRemote/:category/:filename
function scripts_view_remote()
{
	$category = params('category');
	$filename = params('filename');

	$script = file_get_contents("https://raw.githubusercontent.com/FalconChristmas/fpp-scripts/master/" . $category . "/" . $filename);

	echo $script;
}

function scripts_install_remote()
{
	global $fppDir, $SUDO;
	global $scriptDirectory;

    $category = params('category');
	$filename = params('filename');

	exec("$SUDO $fppDir/scripts/installScript \"$category\" \"$filename\"");

    return json(array("status" => "OK"));
}


?>
