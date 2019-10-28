<?

function events_list()
{
    global $settings;

    $events = array();
    $dir = $settings['eventDirectory'];
    if ($d = opendir($dir)) {
        while (($file = readdir($d)) !== false) {
            if (preg_match('/\.fevt$/', $file)) {
                $string = file_get_contents($dir . "/" . $file);
                $json_a = json_decode($string, false);
                
                $file = preg_replace('/\.fevt$/', '', $file);
                
                $events[$file] = $json_a;
            }
        }
        closedir($d);
    }
    return json($events);
}

function event_get()
{
    global $settings;

    $events = array();
    $dir = $settings['eventDirectory'];
    $event = params('eventId');
    if ($event == "ids") {
        $events = array();
        if ($d = opendir($dir)) {
            while (($file = readdir($d)) !== false) {
                if (preg_match('/\.fevt$/', $file)) {                    
                    $file = preg_replace('/\.fevt$/', '', $file);
                    
                    $events[] = $file;
                }
            }
            closedir($d);
        }
        return json($events);
    }
    $filename = $dir . '/' . $event . '.fevt';

    render_file($filename);
}

function event_trigger()
{
    global $settings;

    $event = params('eventId');
    $curl = curl_init('http://localhost:32322/commands/Trigger Event/' . $event);
    curl_setopt($curl, CURLOPT_FAILONERROR, true);
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 200);
    $request_content = curl_exec($curl);
    return $request_content;
}

?>
