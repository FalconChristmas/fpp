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
function CrashReportPath($file, &$error, &$code = null)
{
    global $settings;

    $error = '';
    $code = null;
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
        // Callers tell this apart by code: e.g. pruned by newer builds
        $code = 'not-found';
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
 * **For FPP's own web UI only** (`ShowCrashUploadDialog()` in www/js/fpp.js,
 * which asks the user before every send). Other apps and plugins should open
 * FPP's Diagnostic Report instead.
 *
 * A -manual.zip report (built by `POST /api/crashes/report`) needs a JSON body
 * of `{"consent": true}`: the user's answer in that dialog, never a value a
 * caller sets on their behalf. If the player cannot reach the server
 * (`CanRetryFromBrowser`), the same dialog has the browser post the file itself
 * to the `url` and `field` that `GET /api/crashes/uploadTarget` returns.
 *
 * Error `Code`: consent-required (a -manual.zip report without
 * `{"consent":true}`), not-found (no such report on this player, e.g. pruned
 * by newer builds).
 *
 * @route POST /api/crashes/upload/{file}
 * @badge "FPP UI ONLY" warning
 * @badge "USER CONSENT REQUIRED" warning
 * @body {"consent":true}
 * @response 200 {"Status":"OK"|"Error", ...}
 */
function PostCrashUpload()
{
    $path = CrashReportPath(params('file'), $error, $code);
    if ($path === false) {
        if ($code !== null) {
            return CrashReportError($code, $error);
        }
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
 * fppd builds a Diagnostic Report (settings, configuration and logs, level 3,
 * plus the Troubleshooting page's output and the health check) named like a
 * crash report with a -manual suffix, and keeps it in the crashes folder.
 * When fppd is not running the API builds it instead and the reply adds
 * `"Fppd":"not-running"`
 * (two manual reports at most). Nothing is sent. `client` names the calling
 * app in fppd.log.
 *
 * Any API client may build a report, but what happens to it next is the
 * user's: they download it in a browser from File Manager > Crash Reports to see
 * what it holds, or send it from FPP's own UI (see
 * `POST /api/crashes/upload/{file}`), which asks for their consent. Scripts
 * and other API clients must not download the report themselves (e.g. with
 * `GET /api/file/Crashes/<File>`): use `File` to point the user at it. The
 * server cannot tell callers apart, so this is a rule for clients, not
 * something it enforces.
 *
 * A build takes seconds on a fast player and can take a few minutes on a Pi
 * Zero or BeagleBone. One build at a time, and none for 10s after one finishes.
 *
 * Error `Code`s: bad-request, busy, rate-limited, build-failed, unavailable
 * (fppd is running but did not answer).
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
    $withoutFppd = false;
    if ($reply === false && !FppdListening()) {
        // fppd is not running (stopped, or failing to start): build it here.
        // A hung fppd still accepts the connection, so it never lands here.
        $error = '';
        $file = BuildManualCrashReportWithoutFppd($error);
        $built = $file !== '' ? array('File' => $file) : array('Code' => $error);
        $withoutFppd = true;
    }
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
    $how = $withoutFppd ? ' without fppd (not running)' : '';
    FppdLogLine('crashes.php', 'General', "crash report on request $file (level 3) built$how, kept in crashes/$by");
    $result = array('Status' => 'OK', 'File' => $file,
        'Size' => (int)@filesize($settings['mediaDirectory'] . '/crashes/' . $file));
    if ($withoutFppd) {
        $result['Fppd'] = 'not-running';
    }
    return json($result);
}

// True when something accepts connections on fppd's port
function FppdListening()
{
    $sock = @fsockopen('127.0.0.1', 32322, $errno, $errstr, 1);
    if ($sock === false) {
        return false;
    }
    fclose($sock);
    return true;
}

// ManualCrashReportCallback (fppd.cpp) for when fppd is not running: same
// name (keep in step with CrashReportName), one build at a time, none for 10s
// after one, two manual reports kept.  Runs as the web user, which can read
// everything the report needs.  Returns the file name, or '' with $error set.
function BuildManualCrashReportWithoutFppd(&$error)
{
    global $settings;

    $media = $settings['mediaDirectory'];
    $cdir = $media . '/crashes';
    // Not fppd's tmp/manual-crash: fppd empties that at the start of a build
    $tmpDir = $media . '/tmp/manual-crash-php';
    @mkdir($media . '/tmp', 0775, true);
    $lock = @fopen($media . '/tmp/manual-crash-php.lock', 'c');
    if ($lock === false || !flock($lock, LOCK_EX | LOCK_NB)) {
        $error = 'busy';
        return '';
    }
    $manual = glob($cdir . '/*-manual.zip') ?: array();
    usort($manual, function ($a, $b) {
        return filemtime($b) - filemtime($a);
    });
    if (count($manual) > 0 && time() - filemtime($manual[0]) < 10) {
        $error = 'rate-limited';
        return '';
    }

    // fppd's compile-time platform names
    $platforms = array('Raspberry Pi' => 'Pi', 'BeagleBone Black' => 'BBB', 'BeagleBone 64' => 'BB64',
        'Armbian' => 'Armbian', 'Debian' => 'Debian', 'Docker' => 'Docker', 'MacOS' => 'MacOS');
    $platform = isset($platforms[$settings['Platform']]) ? $platforms[$settings['Platform']] : 'Unknown';
    $uuid = substr(preg_replace('/[^A-Za-z0-9._-]/', '', getSystemUUID()), 0, 63);
    // The system's local time, as fppd uses
    $stamp = trim((string)shell_exec('date +%Y-%m-%d_%H-%M-%S'));
    if ($stamp === '') {
        $stamp = date('Y-m-d_H-i-s');
    }
    $base = 'fpp-' . $platform . '-' . getFPPVersion() . '-' . $uuid . '-' . $stamp . '-manual.zip';
    if (!preg_match('/^fpp-[A-Za-z0-9._-]+-manual\.zip$/', $base)) {
        $error = 'build-failed';
        return '';
    }

    exec('rm -rf ' . escapeshellarg($tmpDir));
    @mkdir($tmpDir, 0775, true);
    $tmpPath = $tmpDir . '/' . $base;
    $cmd = escapeshellarg($settings['fppDir'] . '/scripts/generate_crash_report') . ' 3 ' .
        escapeshellarg($tmpPath) . ' manual';
    if (file_exists('/usr/bin/timeout')) {
        $cmd = '/usr/bin/timeout -k 10 240 ' . $cmd;
    }
    exec($cmd . ' > /dev/null 2>&1');
    clearstatcache();
    if (!is_file($tmpPath) || filesize($tmpPath) == 0) {
        @unlink($tmpPath);
        $error = 'build-failed';
        return '';
    }

    @mkdir($cdir, 0775, true);
    $path = $cdir . '/' . $base;
    // media/tmp may be on another filesystem
    if (!@rename($tmpPath, $path)) {
        $copied = @copy($tmpPath, $path);
        @unlink($tmpPath);
        if (!$copied) {
            @unlink($path);
            $error = 'build-failed';
            return '';
        }
    }
    @chmod($path, 0664);
    // Keep at most two manual reports, counting the new one
    foreach (array_slice(array_values(array_diff($manual, array($path))), 1) as $old) {
        @unlink($old);
    }
    flock($lock, LOCK_UN);
    fclose($lock);
    return $base;
}
