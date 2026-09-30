<?php
/**
 * Diagnostic Report — the wizard's preview data.
 *
 * Step 2 of the wizard (Review & Send) shows vital system info from this endpoint.
 * Report building and sending go through the crash-report backend:
 * POST /api/crashes/report, then UploadAndDeleteCrashReports() in fpp.js.
 * (The old custom errorReport/create + download bundler was removed to avoid
 * a second, divergent redaction/bundling pipeline.)
 */

// ---------------------------------------------------------------------------
// API: Preview vital info for modal
// ---------------------------------------------------------------------------
/**
 * Get Diagnostic Report preview data
 *
 * Returns the vital system information shown in Step 2 of the Diagnostic Report wizard (FPP version, platform, OS, plugins, etc.). See Settings › Privacy for how reports are handled.
 *
 * @route GET /api/errorReport/preview
 * @response 200 Preview data
 * ```json
 * {"Status":"OK","advancedView":{"Version":"8.5","Platform":"Raspberry Pi","Variant":"Pi 4"},"fppInfo":{"Version":"8.5"},"plugins":["plugin1"],"cape":{},"version":"8.5-123-gabc","branch":"master"}
 * ```
 */
function GetErrorReportPreview()
{
    global $settings;
    $status = array();
    // Try local API status (same as menuHead.inc:140)
    $ctx = @stream_context_create(array('http' => array('timeout' => 2)));
    $raw = @file_get_contents('http://127.0.0.1/api/system/status', false, $ctx);
    if ($raw !== false) {
        $j = @json_decode($raw, true);
        if (isset($j['advancedView']) && is_array($j['advancedView'])) {
            $status = $j['advancedView'];
        } elseif (is_array($j)) {
            $status = $j;
        }
    }
    // Fallback to settings + fpp-info
    $mediaDir = isset($settings['mediaDirectory']) ? $settings['mediaDirectory'] : '/home/fpp/media';
    $info = array();
    $infoPath = $mediaDir . '/fpp-info.json';
    if (is_readable($infoPath)) {
        $ij = @json_decode(@file_get_contents($infoPath), true);
        if (is_array($ij)) {
            // Scrub hostname / addresses like generate_crash_report does
            unset($ij['addresses'], $ij['hostname'], $ij['HostDescription']);
            $info = $ij;
        }
    }
    // Plugins
    $plugins = array();
    $plugDir = $mediaDir . '/plugins';
    if (is_dir($plugDir)) {
        foreach (scandir($plugDir) as $d) {
            if ($d === '.' || $d === '..') continue;
            if (is_dir($plugDir . '/' . $d)) $plugins[] = $d;
        }
    }
    // Cape
    $cape = array();
    if (is_readable($mediaDir . '/tmp/cape-info.json')) {
        $cj = @json_decode(@file_get_contents($mediaDir . '/tmp/cape-info.json'), true);
        if (is_array($cj)) $cape = $cj;
    }
    return json(array(
        'Status' => 'OK',
        'advancedView' => $status,
        'fppInfo' => $info,
        'plugins' => $plugins,
        'cape' => $cape,
        'version' => function_exists('getFPPVersion') ? getFPPVersion() : 'Unknown',
        'branch' => function_exists('getFPPBranch') ? getFPPBranch() : '',
    ));
}
