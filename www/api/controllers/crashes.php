<?php
/*
 * Submitting a crash report that was kept locally.
 *
 * fppd always writes the report to <media>/crashes/ and only uploads it when
 * ShareCrashData >= 1 (src/fppd.cpp).  At "Keep locally, do not send" it still
 * writes one -- at level 3, the fullest bundle -- precisely so the user can look
 * at it and submit it themselves, which is what the setting's own tip promises.
 * These endpoints are that promise made into one click instead of "download the
 * zip and email it to a developer".
 *
 * Two ways out, because the two machines involved are not always the same one:
 *
 *   POST /api/crashes/upload/<file>  -- the player uploads it itself.
 *   GET  /api/crashes/uploadTarget   -- tells the browser where to post it, for
 *                                       a player that has no route to the
 *                                       internet but whose operator's laptop
 *                                       does.
 *
 * And a third, further down, for when there is no crash to send but the user
 * wants help: POST /api/crashes/report builds a new report on request, which is
 * then sent the same way.
 *
 * The destination lives here, once, and the browser asks for it rather than
 * hardcoding a second copy that can drift from fppd's.
 */

require_once __DIR__ . '/../../common/oplog.inc.php';

// Same endpoint and form field fppd posts to from its crash handler.
define('CRASH_UPLOAD_URL', 'https://crashes.falconplayer.com/crashUpload/index.php');
define('CRASH_UPLOAD_FIELD', 'userfile');
define('CRASH_REPORT_CONSENT_MESSAGE', 'The user must agree to send this report first; then pass "consent": true');

// JSON only: a cross-site page can post text/plain without a CORS preflight,
// but application/json needs one, and that fails because Limonade answers
// OPTIONS with 501.  Apache already sends Access-Control-Allow-Origin: *, so a
// preflight that succeeded would let any site build and send reports.
function CrashReportJsonBody()
{
    $ctype = isset($_SERVER['CONTENT_TYPE']) ? $_SERVER['CONTENT_TYPE'] : '';
    if (!preg_match('/^application\/json\s*(;|$)/i', $ctype)) {
        return null;
    }
    $raw = file_get_contents('php://input');
    $body = ($raw === '' || $raw === false) ? array() : json_decode($raw, true);
    return is_array($body) ? $body : null;
}

/**
 * Resolve a caller-supplied name to a real crash report.
 *
 * The name comes off the wire, so it is reduced to a basename and required to
 * look like one of our own reports before it is used as a path.  Without that,
 * this is an "upload any file on the box to the internet" endpoint.
 */
function CrashReportPath($file, &$error)
{
    global $settings;

    $error = '';
    $name = basename(str_replace('\\', '/', $file));

    if ($name === '' || $name === '.' || $name === '..') {
        $error = 'Invalid crash report name';
        return false;
    }
    // fppd names these fpp-<platform>-<version>-<uuid>-<timestamp>.zip.  Accept
    // that shape only; anything else is not a crash report we wrote.
    if (!preg_match('/^fpp-[A-Za-z0-9._-]+\.zip$/', $name)) {
        $error = 'Not a crash report file name';
        return false;
    }

    $dir = $settings['mediaDirectory'] . '/crashes';
    $path = $dir . '/' . $name;
    if (!file_exists($path) || !is_file($path)) {
        $error = 'Crash report not found';
        return false;
    }
    // Belt and suspenders after the basename: the file must really sit in the
    // crashes directory and not be a symlink pointing out of it.
    $real = realpath($path);
    $realDir = realpath($dir);
    if ($real === false || $realDir === false || strpos($real, $realDir . '/') !== 0) {
        $error = 'Crash report is outside the crashes directory';
        return false;
    }

    return $real;
}

/**
 * Where a browser should post a report it fetched from this player.
 *
 * @route GET /api/crashes/uploadTarget
 */
function GetCrashUploadTarget()
{
    return json(array(
        'Status' => 'OK',
        'url' => CRASH_UPLOAD_URL,
        'field' => CRASH_UPLOAD_FIELD,
    ));
}

/**
 * The crash rows of Settings > Privacy, to show before sending a crash report
 *
 * The same labels, "goes to" and sub-captions as the privacy table, with each
 * row's text, plus the e-mail row when an address is set, so the two cannot
 * drift.  `item` is the label as a short "includes" phrase and `goesTo` the
 * recipients as one line, both derived from the table's own wording.  All text
 * fields are HTML: insert them as markup, not text.
 *
 * @route GET /api/crashes/disclosures
 * @response 200 Rows in table order
 * ```json
 * {"Status":"OK","goesTo":"FPP &amp; xLights developers and AI service","rows":[{"id":"crash","label":"Send crash reports","item":"crash reports","goesTo":"FPP &amp; xLights developers,<br>AI service","sub":"...","depth":0,"body":"<p>...</p>"}]}
 * ```
 */
function GetCrashDisclosures()
{
    global $settings;

    require_once __DIR__ . '/../../privacyTable.inc';
    $disclosures = privacyDisclosures();
    $rows = array();
    foreach (privacyTableRows() as $r) {
        list($id, $label, $goesTo, $sub, $depth) = $r;
        if (strpos($id, 'crash') === 0) {
            $rows[] = array('id' => $id, 'label' => $label, 'goesTo' => $goesTo,
                'sub' => $sub, 'depth' => $depth, 'body' => $disclosures[$id]['body']);
        }
    }
    // Only reports from a player with an e-mail address set carry one
    if (!empty($settings['emailAddress'])) {
        $rows[] = array('id' => 'email', 'label' => $disclosures['email']['title'], 'goesTo' => '',
            'sub' => '', 'depth' => 1, 'body' => $disclosures['email']['body']);
    }

    // "Send crash reports" -> "crash reports", "… and include your settings"
    // -> "your settings"
    $goesTo = '';
    foreach ($rows as &$row) {
        $row['item'] = lcfirst(preg_replace('/^(Send |&hellip; and include )/', '', $row['label']));
        if ($goesTo === '' && $row['goesTo'] !== '') {
            $goesTo = preg_replace('/,?\s*<br\s*\/?>\s*/i', ' and ', $row['goesTo']);
        }
    }
    unset($row);
    return json(array('Status' => 'OK', 'goesTo' => $goesTo, 'rows' => $rows));
}

/**
 * Upload one locally-kept crash report from the player itself.
 *
 * Deliberately does NOT delete on success.  The caller deletes, through the
 * existing file API, only after this reports the upload actually landed --
 * deleting the only copy of a crash report on an unverified "probably sent" is
 * how the evidence disappears.
 *
 * A -manual.zip report (built by `POST /api/crashes/report`) needs a JSON body
 * of `{"consent": true}`. Before sending it, show the user FPP's disclosures for
 * crash reports, settings, configuration and logs, and e-mail address (the
 * crash, crash-settings, crash-config and email rows in
 * www/privacyDisclosures.inc, shown under Settings > Privacy), and tell them
 * this report is at the "configuration and logs" level and is sent even if
 * their saved crash report setting is "Keep locally, do not send" or Disabled.
 * If the player cannot reach the server (`CanRetryFromBrowser`), the browser
 * can post the file itself to the `url` and `field` that
 * `GET /api/crashes/uploadTarget` returns.
 *
 * Error `Code`: consent-required (a -manual.zip report without
 * `{"consent":true}`).
 *
 * @route POST /api/crashes/upload/<file>
 * @badge "USER CONSENT REQUIRED" warning
 * @body {"consent":true}
 * @response 200 {"Status":"OK"|"Error", ...}
 */
function PostCrashUpload()
{
    $path = CrashReportPath(params('file'), $error);
    if ($path === false) {
        return json(array('Status' => 'Error', 'Message' => $error));
    }

    if (preg_match('/-manual\.zip$/', $path)) {
        $body = CrashReportJsonBody();
        if (!is_array($body) || !isset($body['consent']) || $body['consent'] !== true) {
            return CrashReportError('consent-required', CRASH_REPORT_CONSENT_MESSAGE);
        }
        FppdLogLine('crashes.php', 'General', 'crash report ' . basename($path) . ' sending on request, user consent given');
    }

    if (!function_exists('curl_init')) {
        return json(array(
            'Status' => 'Error',
            'Message' => 'curl is not available on this player',
            'CanRetryFromBrowser' => true,
        ));
    }

    $ch = curl_init(CRASH_UPLOAD_URL);
    curl_setopt($ch, CURLOPT_POST, true);
    curl_setopt($ch, CURLOPT_POSTFIELDS, array(
        CRASH_UPLOAD_FIELD => new CURLFile($path, 'application/zip', basename($path)),
    ));
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
    // A player with no internet should fail fast and hand the job to the
    // browser, not sit on the request until the page gives up.
    curl_setopt($ch, CURLOPT_CONNECTTIMEOUT, 10);
    curl_setopt($ch, CURLOPT_TIMEOUT, 120);
    curl_setopt($ch, CURLOPT_FOLLOWLOCATION, true);

    $body = curl_exec($ch);
    $curlErr = curl_error($ch);
    $httpCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
    curl_close($ch);

    if ($body === false || $httpCode < 200 || $httpCode >= 300) {
        return json(array(
            'Status' => 'Error',
            'Message' => $curlErr !== '' ? $curlErr : ('Upload failed (HTTP ' . $httpCode . ')'),
            'HTTPCode' => $httpCode,
            // The distinguishing case for the caller: the player could not
            // reach the server, so a browser that can is worth trying.
            'CanRetryFromBrowser' => ($httpCode == 0),
        ));
    }

    return json(array(
        'Status' => 'OK',
        'File' => basename($path),
        'HTTPCode' => $httpCode,
        'Response' => is_string($body) ? substr($body, 0, 512) : '',
    ));
}

function CrashReportError($code, $message)
{
    return json(array('Status' => 'Error', 'Code' => $code, 'Message' => $message));
}

/**
 * Build a crash report now, for the user to look at and then send
 *
 * fppd builds a full report (settings, configuration and logs, level 3) named
 * like a crash report with a -manual suffix, and keeps it in the crashes folder
 * (two manual reports at most). Nothing is sent: the user can download it from
 * `GET /api/file/Crashes/<File>` to see what it holds, then send it with
 * `POST /api/crashes/upload/<File>`, which needs their consent. `client` names
 * the calling app in fppd.log.
 *
 * A build takes seconds on a fast player and can take a few minutes on a Pi
 * Zero or BeagleBone. One build a minute, counted from the start of the last
 * one.
 *
 * Error `Code`s: bad-request, busy, rate-limited, build-failed, unavailable.
 *
 * @route POST /api/crashes/report
 * @body {"client":"My App"}
 * @response 200 Report built
 * ```json
 * {"Status":"OK","File":"fpp-Pi-10.0-M1-abc-2026-09-25_10-00-00-manual.zip","Size":123456}
 * ```
 */
function PostCrashReport()
{
    global $settings;

    // fppd builds the report (ManualCrashReportCallback)
    $body = CrashReportJsonBody();
    if ($body === null) {
        return CrashReportError('bad-request', 'Send a JSON object as application/json');
    }
    $client = isset($body['client']) && is_string($body['client'])
        ? substr(preg_replace('/[^\x20-\x7e]/', '', $body['client']), 0, 64) : '';
    $by = $client !== '' ? " (client '$client')" : '';

    set_time_limit(0);
    ignore_user_abort(true);
    // fppd gives up on the build after 250s; Apache's own limit is 300s
    $ctx = stream_context_create(array('http' => array('method' => 'POST', 'header' => "Content-Length: 0\r\n",
        'timeout' => 270, 'ignore_errors' => true)));
    error_clear_last();
    $reply = @file_get_contents('http://127.0.0.1:32322/internal/crashReport', false, $ctx);
    $built = $reply === false ? null : json_decode($reply, true);
    if (!is_array($built) || !isset($built['File'])) {
        if (is_array($built) && isset($built['Code'])) {
            $code = $built['Code'];
            $message = 'The crash report could not be built: ' . $code;
        } else {
            $err = error_get_last();
            $code = 'unavailable';
            $message = isset($http_response_header[0]) ? 'fppd returned: ' . $http_response_header[0]
                : 'fppd did not respond' . ($err ? ': ' . $err['message'] : '');
        }
        FppdLogLine('crashes.php', 'General', "crash report on request not built ($code)$by");
        return CrashReportError($code, $message);
    }

    $file = basename($built['File']);
    clearstatcache();
    FppdLogLine('crashes.php', 'General', "crash report on request $file (level 3) built, kept in crashes/$by");
    return json(array('Status' => 'OK', 'File' => $file,
        'Size' => (int)@filesize($settings['mediaDirectory'] . '/crashes/' . $file)));
}
