<?

/**
 * Get all event files
 *
 * Returns a map of all event (`*.fevt`) files, keyed by event ID (filename without extension).
 *
 * @route GET /api/events
 * @response 200 Map of all event files keyed by event ID
 * ```json
 * {
 *   "1_1": {
 *     "name": "My Event",
 *     "effect": "rainbow",
 *     "startChannel": 1
 *   }
 * }
 * ```
 */
function EventsList()
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

/**
 * Get event file
 *
 * Returns the contents of a specific event file. If `{eventId}` is `ids`, returns a map
 * of event IDs to display names.
 *
 * @route GET /api/events/{eventId}
 * @response 200 Event file contents
 * ```json
 * {
 *   "name": "My Event",
 *   "effect": "rainbow",
 *   "startChannel": 1
 * }
 * ```
 */
function EventGet()
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
                    $e = file_get_contents($dir . "/" . $file);
                    $file = preg_replace('/\.fevt$/', '', $file);
                    $j = json_decode($e, true);
                    $name = $j["name"];
                    if ($name != "") {
                        $eventText = preg_replace("/_/", " / ", $file) . " - " . $name;
                        $events[$file] = $eventText;
                    } else{
                        $events[$file] = $file;
                    }
                }
            }
            closedir($d);
        }
        return json($events);
    }
    $filename = $dir . '/' . $event . '.fevt';

    render_file($filename);
}

/**
 * Trigger event
 *
 * Triggers the specified event by sending a `Trigger Event` command to `fppd`.
 *
 * @badges "FPP REQUIRED" critical
 * @route POST /api/events/{eventId}/trigger
 * @response 200 Event triggered
 * ```json
 * {"status": "OK"}
 * ```
 */
function EventTrigger()
{
    global $settings;

    $event = params('eventId');
    $curl = curl_init('http://localhost:32322/commands/Trigger%20Event/' . $event);
    curl_setopt($curl, CURLOPT_FAILONERROR, true);
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT_MS, 200);
    $request_content = curl_exec($curl);
    return $request_content;
}

?>
