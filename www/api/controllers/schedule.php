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

        //Trigger a JSON Configuration Backup
        GenerateBackupViaAPI('Schedule was modified.');
        try {
            TriggerScheduleSaveHook();
        } catch (\Throwable $e) {
            error_log('[schedule] post-save hook threw: ' . $e->getMessage());
        }

        return GetSchedule();
    }

    halt(404, 'Unable to open schedule.json for writing.');
}

/////////////////////////////////////////////////////////////////////////////
// Optional post-save hook. Never blocks/fails schedule save.
function TriggerScheduleSaveHook() {
    global $settings;

    try {
        $hookUrl = '';
        if (isset($settings['ScheduleSaveHookUrl'])) {
            $hookUrl = trim((string)$settings['ScheduleSaveHookUrl']);
        }

        if ($hookUrl === '') {
            // Generic opt-in hook. No default target in core.
            return;
        }

        $parts = @parse_url($hookUrl);
        if (!is_array($parts)) {
            error_log('[schedule] post-save hook skipped: invalid URL');
            return;
        }

        $scheme = strtolower((string)($parts['scheme'] ?? ''));
        $host = strtolower((string)($parts['host'] ?? ''));
        $allowedHosts = array('127.0.0.1', 'localhost', '::1');

        if (
            !in_array($scheme, array('http', 'https'), true) ||
            !in_array($host, $allowedHosts, true)
        ) {
            error_log('[schedule] post-save hook skipped: URL must target localhost');
            return;
        }

        if (!function_exists('curl_init')) {
            error_log('[schedule] post-save hook skipped: curl extension unavailable');
            return;
        }

        $schedulePath = $settings['configDirectory'] . '/schedule.json';
        $payload = array(
            'event' => 'schedule.saved',
            'source' => 'api.schedule',
            'savedAtUtc' => gmdate('c'),
            'schedulePath' => $schedulePath,
        );
        $payloadJson = json_encode($payload);
        if (!is_string($payloadJson) || $payloadJson === '') {
            error_log('[schedule] post-save hook skipped: failed to encode payload');
            return;
        }

        $ch = curl_init();
        if ($ch === false) {
            error_log('[schedule] post-save hook failed: curl_init returned false');
            return;
        }

        curl_setopt_array($ch, array(
            CURLOPT_URL => $hookUrl,
            CURLOPT_RETURNTRANSFER => true,
            CURLOPT_NOBODY => false,
            CURLOPT_CUSTOMREQUEST => 'POST',
            CURLOPT_POST => true,
            CURLOPT_POSTFIELDS => $payloadJson,
            CURLOPT_CONNECTTIMEOUT_MS => 400,
            CURLOPT_TIMEOUT_MS => 1500,
            CURLOPT_FOLLOWLOCATION => false,
            CURLOPT_MAXREDIRS => 0,
            CURLOPT_USERAGENT => 'FPP-ScheduleHook/1.0',
            CURLOPT_HTTPHEADER => array(
                'Connection: close',
                'Content-Type: application/json'
            ),
        ));

        $resp = curl_exec($ch);
        $errno = curl_errno($ch);
        $error = curl_error($ch);
        $httpCode = (int)curl_getinfo($ch, CURLINFO_RESPONSE_CODE);
        curl_close($ch);

        if ($resp === false || $errno !== 0 || $httpCode < 200 || $httpCode >= 300) {
            $safeErr = $error !== '' ? $error : 'n/a';
            error_log(
                '[schedule] post-save hook failed url=' . $hookUrl .
                ' errno=' . (string)$errno .
                ' http=' . (string)$httpCode .
                ' err=' . $safeErr
            );
        }
    } catch (\Throwable $e) {
        error_log('[schedule] post-save hook failed unexpectedly: ' . $e->getMessage());
    }
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
