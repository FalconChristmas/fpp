<?

require_once('../commandsocket.php');

/**
 * Get schedules
 *
 * Returns the current FPP schedule configuration from `schedule.json`.
 *
 * @route GET /api/schedule
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
 * @route POST /api/schedule
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

    // Stored as sent, but only if it is a schedule (a JSON array of entries):
    // the scheduler reads this file and anything else would leave the player
    // with no schedule.  Written atomically so fppd never reads half of it.
    $body = file_get_contents("php://input");
    $entries = json_decode($body, true);
    if (!is_array($entries) || !array_is_list($entries)) {
        halt(400, 'The schedule must be a JSON array of entries.');
    }

    if (WriteFileAtomic($fileName, $body))
    {
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
 * @route POST /api/schedule/reload
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
