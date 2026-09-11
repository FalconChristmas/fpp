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
 * The destination lives here, once, and the browser asks for it rather than
 * hardcoding a second copy that can drift from fppd's.
 */

// Same endpoint and form field fppd posts to from its crash handler.
define('CRASH_UPLOAD_URL', 'https://crashes.falconplayer.com/crashUpload/index.php');
define('CRASH_UPLOAD_FIELD', 'userfile');

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
 * Upload one locally-kept crash report from the player itself.
 *
 * Deliberately does NOT delete on success.  The caller deletes, through the
 * existing file API, only after this reports the upload actually landed --
 * deleting the only copy of a crash report on an unverified "probably sent" is
 * how the evidence disappears.
 *
 * @route POST /api/crashes/upload/<file>
 * @response 200 {"Status":"OK"|"Error", ...}
 */
function PostCrashUpload()
{
    $path = CrashReportPath(params('file'), $error);
    if ($path === false) {
        return json(array('Status' => 'Error', 'Message' => $error));
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
