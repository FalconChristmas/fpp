<?

require_once('../commandsocket.php');

/**
 * Get schedules
 *
 * Returns the current FPP schedule configuration from `schedule.json`.
 *
 * @route-v1 GET /schedule
 * @route-v2 GET /schedule
 * @response 200 Current schedule entries
 * ```json
 * [
 *   {
 *     "day": 7,
 *     "enabled": 0,
 *     "endDate": "2099-12-31",
 *     "endTime": "23:00:00",
 *     "playlist": "Main Show",
 *     "repeat": 1,
 *     "startDate": "2014-01-01",
 *     "startTime": "17:00:00",
 *     "stopType": 0
 *   }
 * ]
 * ```
 */
function GetSchedule() {
    global $settings;

    $file = $settings['configDirectory'] . '/schedule.json';
    if (!file_exists($file)) {
        $schedule = [];
        return json($schedule);
    }

    $data = file_get_contents($file);

    return json(json_decode($data, true));
}

/**
 * Set schedule
 *
 * Saves the new schedule configuration to `schedule.json`.
 *
 * @route-v1 POST /schedule
 * @route-v2 POST /schedule
 * @body [{"day": 7, "enabled": 0, "endDate": "2099-12-31", "endTime": "23:00:00", "playlist": "Main Show", "repeat": 1, "startDate": "2014-01-01", "startTime": "17:00:00", "stopType": 0}]
 * @response 200 Saved schedule entries
 * ```json
 * [
 *   {
 *     "day": 7,
 *     "enabled": 0,
 *     "endDate": "2099-12-31",
 *     "endTime": "23:00:00",
 *     "playlist": "Main Show",
 *     "repeat": 1,
 *     "startDate": "2014-01-01",
 *     "startTime": "17:00:00",
 *     "stopType": 0
 *   }
 * ]
 * ```
 * @response 500 Failed to write schedule
 * ```text
 * Unable to open schedule.json for writing.
 * ```
 */
function SaveSchedule() {
    global $settings;
    $result = Array();

    $fileName = $settings['configDirectory'] . '/schedule.json';

    $f = fopen($fileName, "w");
    if ($f)
    {
        $postdata = fopen("php://input", "r");
        while ($data = fread($postdata, 1024*16)) {
            fwrite($f, $data);
        }
        fclose($postdata);
        fclose($f);

        //Trigger a JSON Configuration Backup
        GenerateBackupViaAPI('Schedule was modified.');

        return GetSchedule();
    }

    halt(500, 'Unable to open schedule.json for writing.');
}

/**
 * Reload schedules
 *
 * Sends a reload command to `fppd` to re-read the schedule configuration.
 *
 * @badge "FPP REQUIRED" critical
 * @route-v1 POST /schedule/reload
 * @route-v2 POST /schedule/reload
 * @response 200 Schedule reloaded
 * ```json
 * {"Status": "OK", "Message": ""}
 * ```
 */
function ReloadSchedule() {
    SendCommand('R');

    $result = Array();
    $result['Status'] = 'OK';
    $result['Message'] = '';

    return json($result);
}

?>
