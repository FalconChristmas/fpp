<?

require_once('../commandsocket.php');

/////////////////////////////////////////////////////////////////////////////
// GET /api/schedule
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

/////////////////////////////////////////////////////////////////////////////
// POST /api/schedule
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

        return GetSchedule();
    }

    halt(404, 'Unable to open schedule.json for writing.');
}

/////////////////////////////////////////////////////////////////////////////
// POST /api/schedule/reload
function ReloadSchedule() {
    SendCommand('R');

    $result = Array();
    $result['Status'] = 'OK';
    $result['Message'] = '';

    return json($result);
}

/////////////////////////////////////////////////////////////////////////////

?>
