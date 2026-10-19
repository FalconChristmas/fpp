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
 * Events became Command Presets in FPP 5: fppd turned each event file into a
 * preset named after the event. This triggers the preset named in the
 * event file `{eventId}.fevt`, as the `Trigger Command Preset` command does.
 *
 * @badge "FPP REQUIRED" critical
 * @route POST /api/events/{eventId}/trigger
 * @route GET /api/events/{eventId}/trigger deprecated
 * @response 200 Event triggered
 * ```json
 * {"status": "OK"}
 * ```
 * @response 404 No such event, or no Command Preset by its name
 * ```json
 * {"status": "ERROR", "message": "No Command Preset named \"My Event\" (from event 01_01)"}
 * ```
 */
function event_trigger()
{
    global $settings;

    $event = params('eventId');
    // An event id is an event file's base name (e.g. 01_02).  It is appended
    // to an fppd URL, and limonade has already decoded it, so '?', '#' or an
    // encoded '/' would otherwise reach fppd as more URL than an id.
    if (!is_string($event) || !preg_match('/^[A-Za-z0-9_-]+$/', $event)) {
        http_response_code(400);
        return json(array("status" => "ERROR", "message" => "Invalid event id"));
    }
    $file = $settings['eventDirectory'] . '/' . $event . '.fevt';
    $info = is_file($file) ? json_decode(file_get_contents($file), true) : null;
    $name = is_array($info) && isset($info['name']) && is_string($info['name']) ? $info['name'] : '';
    if ($name === '') {
        http_response_code(404);
        return json(array("status" => "ERROR", "message" => "No event $event"));
    }

    // The command fppd had for this, "Trigger Event", went with the events.
    $body = FppdCommand('Trigger Command Preset', array($name), 200, 10, false, $code);
    if ($code == 200) {
        return json(array("status" => "OK"));
    }
    if ($code == 500 && trim((string) $body) == 'Not found') {
        http_response_code(404);
        return json(array("status" => "ERROR", "message" => "No Command Preset named \"$name\" (from event $event)"));
    }
    http_response_code(500);
    return json(array("status" => "ERROR", "message" => $body === false ? "Unable to contact fppd" : trim($body)));
}

?>
