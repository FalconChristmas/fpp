<?

function effects_list()
{
    global $settings;

    $events = array();
    if ($d = opendir($settings['effectDirectory'])) {
        while (($file = readdir($d)) !== false) {
            if (preg_match('/\.eseq$/', $file)) {
                $file = preg_replace('/\.eseq$/', '', $file);
                array_push($events, $file);
            }
        }
        closedir($d);
    }
    return json($events);
}

?>
