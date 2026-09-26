<?php
/**
 * Error Report bundle — F8 modal.
 *
 * Packages vital info + selectable logs/config for manual download
 * before creating a GitHub issue. No PAT, no outbound GitHub call.
 * Redaction mirrors scripts/generate_crash_report (settings.json
 * type=password / pii, interface-settings.json gatherStats allowlist).
 */

// ---------------------------------------------------------------------------
// Helpers — mirror generate_crash_report redaction
// ---------------------------------------------------------------------------

function ErrorReportTempDir()
{
    $mediaDir = isset($GLOBALS['settings']['mediaDirectory']) ? $GLOBALS['settings']['mediaDirectory'] : '/home/fpp/media';
    $base = rtrim($mediaDir, '/') . '/tmp';
    if (!is_dir($base) && !@mkdir($base, 0775, true) && !is_dir($base)) {
        // Fallback to system tmp if media tmp not writable (e.g., dev machine)
        $base = sys_get_temp_dir();
    }
    $dir = @tempnam($base, 'error-report-');
    if ($dir !== false) {
        @unlink($dir);
        if (@mkdir($dir, 0775, true)) {
            return $dir;
        }
    }
    $dir = $base . '/error-report-' . uniqid('', true);
    if (@mkdir($dir, 0775, true)) {
        return $dir;
    }
    return false;
}

function ErrorReportBuildSecretKeys()
{
    static $cached = null;
    if ($cached !== null) return $cached;
    $fallback = "MQTTPassword|TetherPSK|emailpass|gitHubPAT|osPassword|osPasswordVerify|password|passwordVerify|Latitude|Longitude|emailAddress|emailuser|emailserver|MQTTUsername";
    $settingsJson = "/opt/fpp/www/settings.json";
    if (!is_readable($settingsJson)) {
        $settingsJson = __DIR__ . "/../../settings.json";
    }
    if (is_readable($settingsJson)) {
        $raw = @file_get_contents($settingsJson);
        $j = @json_decode($raw, true);
        if (isset($j['settings']) && is_array($j['settings'])) {
            $names = array();
            foreach ($j['settings'] as $k => $v) {
                if (is_array($v) && ((isset($v['type']) && $v['type'] === 'password') || !empty($v['pii']))) {
                    if (preg_match('/^[A-Za-z_][A-Za-z0-9_]*$/', $k)) {
                        $names[] = $k;
                    }
                }
            }
            if (!empty($names)) {
                usort($names, function ($a, $b) {
                    $d = strlen($b) - strlen($a);
                    if ($d !== 0) return $d;
                    return strcmp($a, $b);
                });
                $cached = implode("|", $names);
                return $cached;
            }
        }
    }
    $cached = $fallback;
    return $cached;
}

function ErrorReportInterfaceFields()
{
    static $cached = null;
    if ($cached !== null) return $cached;
    $fields = array();
    $ij = "/opt/fpp/www/interface-settings.json";
    if (!is_readable($ij)) {
        $ij = __DIR__ . "/../../interface-settings.json";
    }
    if (is_readable($ij)) {
        $raw = @file_get_contents($ij);
        $j = @json_decode($raw, true);
        if (isset($j['fields']) && is_array($j['fields'])) {
            foreach ($j['fields'] as $k => $v) {
                if (is_array($v) && !empty($v['gatherStats']) && preg_match('/^[A-Za-z_][A-Za-z0-9_]*$/', $k)) {
                    $fields[] = $k;
                }
            }
        }
    }
    sort($fields);
    $cached = $fields;
    return $cached;
}

function ErrorReportRedactCopy($src, $dst)
{
    if (!is_readable($src)) return false;
    $secretKeys = ErrorReportBuildSecretKeys();
    $enableRe = '^[[:space:]]*"?[A-Za-z0-9_.-]*[Ee]nable"?[[:space:]]*[:=]';
    $loose = '[Pp]assword|PASSWORD|[Pp]asswd|PASSWD|[Pp]assphrase|PASSPHRASE|[Pp]asscode|PASSCODE|[Pp]wd|PWD|[Pp]sk|PSK|[Ss]ecret|SECRET|[Tt]oken|TOKEN|[Cc]redential|CREDENTIAL';
    $tight = '([Pp]ass|[Kk]ey|[Aa]uth|[Pp]at)([^a-z"\'[:space:]:=][A-Za-z0-9_.-]*)?|(PASS|KEY|AUTH|PAT)([^A-Za-z"\'[:space:]:=][A-Za-z0-9_.-]*)?';
    $backstopKey = "([A-Za-z0-9_.-]*((" . $loose . ")[A-Za-z0-9_.-]*|(" . $tight . "))\"?[[:space:]]*[:=][[:space:]]*)";
    $full = "cat " . escapeshellarg($src) . " | sed -E "
        . "-e " . escapeshellarg("/" . $enableRe . "/! s/^([[:space:]]*\"?(" . $secretKeys . ")\"?[[:space:]]*[:=][[:space:]]*).*$/\\1\"**REDACTED**\"/") . " "
        . "-e " . escapeshellarg("/" . $enableRe . "/! s/(\"(" . $secretKeys . ")\"[[:space:]]*:[[:space:]]*)\"[^\"]*\"/\\1\"**REDACTED**\"/g") . " "
        . "-e " . escapeshellarg("/" . $enableRe . "/! s/" . $backstopKey . "\"[^\"]*\"/\\1\"**REDACTED**\"/g") . " "
        . "-e " . escapeshellarg("/" . $enableRe . "/! s/" . $backstopKey . "'[^']*'/\\1'**REDACTED**'/g") . " "
        . "-e " . escapeshellarg("/" . $enableRe . "/! s/" . $backstopKey . "([^\"'[:space:]]+)/\\1**REDACTED**/g") . " "
        . "-e " . escapeshellarg("s#([a-z][a-z0-9+.-]*://)[^:/@[:space:]]+:[^@[:space:]]+@#\\1**REDACTED**@#gI")
        . " > " . escapeshellarg($dst) . " 2>/dev/null";
    $ret = 0;
    exec($full, $out, $ret);
    if ($ret !== 0 || !file_exists($dst) || filesize($dst) === false) {
        @unlink($dst);
        return false;
    }
    return true;
}

function ErrorReportRedactCopyOrStub($src, $dst)
{
    $head = @file_get_contents($src, false, null, 0, 8192);
    if ($head !== false && strpos($head, "\0") !== false) {
        $sz = @filesize($src);
        @file_put_contents($dst, "**OMITTED: binary file, " . ($sz !== false ? $sz : 0) . " bytes**\n");
        return true;
    }
    return ErrorReportRedactCopy($src, $dst);
}

function ErrorReportFilterInterface($src, $dst, $allowFields)
{
    if (!is_readable($src)) {
        @file_put_contents($dst, "**UNREADABLE INTERFACE CONFIG**\n");
        return;
    }
    if (empty($allowFields)) {
        @file_put_contents($dst, "**REDACTED INTERFACE CONFIG (no field allowlist available)**\n");
        return;
    }
    $ereg = '^[[:space:]]*(' . implode('|', array_map(function ($k) { return preg_quote($k, '/'); }, $allowFields)) . ')[[:space:]]*=';
    $cmd = "grep -E " . escapeshellarg($ereg) . " " . escapeshellarg($src) . " 2>/dev/null";
    $content = shell_exec($cmd);
    // shell_exec returns null on error, string otherwise; trim must handle null safely
    $trimmed = $content !== null ? trim($content) : "";
    if ($trimmed === "") {
        $out = "# Filtered: only fields on the shared allowlist are included.\n";
        if ($content !== null && $content !== "") $out .= $content;
    } else {
        $out = "# Filtered: only fields on the shared allowlist are included.\n" . $content;
    }
    @file_put_contents($dst, $out !== "" ? $out : "**REDACTED INTERFACE CONFIG**\n");
}

// ---------------------------------------------------------------------------
// Path validation for download
// ---------------------------------------------------------------------------
function ErrorReportPath($file, &$error)
{
    global $settings;
    $error = '';
    $name = basename(str_replace('\\', '/', $file));
    if ($name === '' || $name === '.' || $name === '..') {
        $error = 'Invalid file name';
        return false;
    }
    if (!preg_match('/^error-report-[A-Za-z0-9._-]+\.zip$/', $name)) {
        $error = 'Not an error report file';
        return false;
    }
    $dir = $settings['mediaDirectory'] . '/tmp';
    $path = $dir . '/' . $name;
    if (!file_exists($path) || !is_file($path)) {
        $error = 'Report not found';
        return false;
    }
    $real = realpath($path);
    $realDir = realpath($dir);
    if ($real === false || $realDir === false || strpos($real, $realDir . '/') !== 0) {
        $error = 'File is outside tmp directory';
        return false;
    }
    return $real;
}

// ---------------------------------------------------------------------------
// API: Preview vital info for modal
// ---------------------------------------------------------------------------
/**
 * Get Error Report preview data
 *
 * Returns the vital system information shown in Step 1 of the Error Report wizard (FPP version, platform, OS, plugins, etc.). Hostname and network addresses are already stripped.
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
    $mediaDir = $settings['mediaDirectory'];
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

// ---------------------------------------------------------------------------
// API: Create bundle
// ---------------------------------------------------------------------------
/**
 * Create an Error Report bundle
 *
 * Builds a redacted diagnostic zip on the device and returns its download URL. Mirrors the redaction rules of `scripts/generate_crash_report` (secrets become `**REDACTED**`). Use the flags to control what is included; Step 1 vital info is always included.
 *
 * @route POST /api/errorReport/create
 * @body {"includeSettings":true,"includeFppdLog":true,"includeSysLogs":false,"includeNet":false,"includeConfig":false,"includePlaylists":false}
 * @response 200 Bundle created
 * ```json
 * {"Status":"OK","file":"error-report-20240101-120000-abc123.zip","zipUrl":"api/errorReport/download/error-report-20240101-120000-abc123.zip","manifest":["fpp-info.json","plugins.json","settings"],"size":12345}
 * ```
 * @response 500 Failed to create bundle
 * ```json
 * {"Status":"Error","Message":"Failed to create bundle"}
 * ```
 */
function PostErrorReportCreate()
{
    global $settings;
    $mediaDir = isset($settings['mediaDirectory']) ? $settings['mediaDirectory'] : '/home/fpp/media';
    if (!is_dir($mediaDir)) {
        return json(array('Status' => 'Error', 'Message' => 'Media directory not found'));
    }
    // Parse JSON body — limit to 64KB to avoid OOM
    $raw = file_get_contents('php://input');
    if ($raw !== false && strlen($raw) > 65536) {
        return json(array('Status' => 'Error', 'Message' => 'Request too large'));
    }
    $opts = @json_decode($raw, true);
    if (!is_array($opts)) $opts = array();
    // Query params also allowed (strict check)
    if (isset($_REQUEST['includeSettings'])) {
        $v = $_REQUEST['includeSettings'];
        $opts['includeSettings'] = ($v === '1' || $v === 'true' || $v === 1 || $v === true);
    }
    if (isset($_REQUEST['includeFppdLog'])) {
        $v = $_REQUEST['includeFppdLog'];
        $opts['includeFppdLog'] = ($v === '1' || $v === 'true' || $v === 1 || $v === true);
    }
    // Defaults: Settings ON, fppd.log ON, rest OFF
    $includeSettings = isset($opts['includeSettings']) ? (bool)$opts['includeSettings'] : true;
    $includeFppdLog = isset($opts['includeFppdLog']) ? (bool)$opts['includeFppdLog'] : true;
    $includeSysLogs = !empty($opts['includeSysLogs']);
    $includeNet = !empty($opts['includeNet']);
    $includeConfig = !empty($opts['includeConfig']);
    $includePlaylists = !empty($opts['includePlaylists']);

    $tmpRoot = ErrorReportTempDir();
    if ($tmpRoot === false || !is_dir($tmpRoot)) {
        return json(array('Status' => 'Error', 'Message' => 'Failed to create temp dir — check disk space/permissions'));
    }
    $bundleDir = $tmpRoot . '/bundle';
    if (!@mkdir($bundleDir, 0775, true) && !is_dir($bundleDir)) {
        @exec("rm -rf " . escapeshellarg($tmpRoot));
        return json(array('Status' => 'Error', 'Message' => 'Failed to create bundle dir'));
    }

    $manifest = array();

    // Vital: fpp-info.json (scrubbed) — always
    $infoSrc = $mediaDir . '/fpp-info.json';
    if (is_readable($infoSrc)) {
        $dst = $bundleDir . '/fpp-info.json';
        // Scrub via python if available, else just copy (still scrubbed preview style)
        if (function_exists('json_decode')) {
            $j = @json_decode(@file_get_contents($infoSrc), true);
            if (is_array($j)) {
                unset($j['addresses'], $j['hostname'], $j['HostDescription']);
                @file_put_contents($dst, json_encode($j, JSON_PRETTY_PRINT));
                $manifest[] = 'fpp-info.json';
            } else {
                ErrorReportRedactCopy($infoSrc, $dst);
                $manifest[] = 'fpp-info.json';
            }
        }
    }
    // plugins.json
    $plugDst = $bundleDir . '/plugins.json';
    $plugins = array();
    if (is_dir($mediaDir . '/plugins')) {
        foreach (scandir($mediaDir . '/plugins') as $d) {
            if ($d === '.' || $d === '..') continue;
            if (is_dir($mediaDir . '/plugins/' . $d)) $plugins[] = $d;
        }
    }
    @file_put_contents($plugDst, json_encode(array('plugins' => $plugins), JSON_PRETTY_PRINT));
    $manifest[] = 'plugins.json';

    // cape-info
    if (is_readable($mediaDir . '/tmp/cape-info.json')) {
        $cDst = $bundleDir . '/cape-info.json';
        @copy($mediaDir . '/tmp/cape-info.json', $cDst);
        $manifest[] = 'cape-info.json';
    }

    // Settings (redacted)
    if ($includeSettings) {
        if (is_file($mediaDir . '/settings')) {
            ErrorReportRedactCopy($mediaDir . '/settings', $bundleDir . '/settings');
            $manifest[] = 'settings';
        }
        if (is_dir($mediaDir . '/settings')) {
            @mkdir($bundleDir . '/settings', 0775, true);
            foreach (scandir($mediaDir . '/settings') as $f) {
                if ($f === '.' || $f === '..') continue;
                $src = $mediaDir . '/settings/' . $f;
                if (is_file($src)) {
                    ErrorReportRedactCopy($src, $bundleDir . '/settings/' . $f);
                }
            }
            $manifest[] = 'settings/*';
        }
    }

    // fppd.log — last 5000 lines, redacted
    if ($includeFppdLog) {
        $logSrc = $mediaDir . '/logs/fppd.log';
        if (is_readable($logSrc)) {
            @mkdir($bundleDir . '/logs', 0775, true);
            $tmpLog = $tmpRoot . '/fppd_tail.log';
            // tail last 5000 lines
            @exec("tail -n 5000 " . escapeshellarg($logSrc) . " > " . escapeshellarg($tmpLog) . " 2>/dev/null");
            if (!is_readable($tmpLog) || filesize($tmpLog) == 0) {
                // fallback to full file via tail failed
                @copy($logSrc, $tmpLog);
            }
            ErrorReportRedactCopy($tmpLog, $bundleDir . '/logs/fppd.log');
            @unlink($tmpLog);
            $manifest[] = 'logs/fppd.log (last 5000 lines)';
        }
    }

    // System logs
    if ($includeSysLogs) {
        @mkdir($bundleDir . '/logs', 0775, true);
        if (is_readable($mediaDir . '/logs/apache2-error.log')) {
            ErrorReportRedactCopy($mediaDir . '/logs/apache2-error.log', $bundleDir . '/logs/apache2-error.log');
            $manifest[] = 'logs/apache2-error.log';
        }
        if (is_readable('/tmp/fppd_crash_log_ring.log')) {
            ErrorReportRedactCopy('/tmp/fppd_crash_log_ring.log', $bundleDir . '/logs/fppd_crash_log_ring.log');
            $manifest[] = 'logs/fppd_crash_log_ring.log';
        }
        // boot.log from dmesg+journalctl
        $bootDst = $bundleDir . '/logs/boot.log';
        $bootContent = "";
        $dmesg = @shell_exec("dmesg -T 2>/dev/null | head -n 2000");
        if ($dmesg) $bootContent .= $dmesg . "\n";
        $jctl = @shell_exec("journalctl -b -u fppinit -u fppoled -u fpp_postnetwork 2>/dev/null | head -n 2000");
        if ($jctl) $bootContent .= $jctl;
        if ($bootContent !== "") {
            @file_put_contents($bootDst, $bootContent);
            // redact boot log too
            $tmpBoot = $bootDst . '.tmp';
            @rename($bootDst, $tmpBoot);
            ErrorReportRedactCopy($tmpBoot, $bootDst);
            @unlink($tmpBoot);
            $manifest[] = 'logs/boot.log';
        }
    }

    // Config
    if ($includeConfig) {
        $cfgSrc = $mediaDir . '/config';
        if (is_dir($cfgSrc)) {
            @mkdir($bundleDir . '/config', 0775, true);
            $allowFields = ErrorReportInterfaceFields();
            foreach (scandir($cfgSrc) as $f) {
                if ($f === '.' || $f === '..') continue;
                $src = $cfgSrc . '/' . $f;
                if (!is_file($src)) continue;
                $dst = $bundleDir . '/config/' . $f;
                if (preg_match('/^interface\./', $f)) {
                    if ($includeNet) {
                        ErrorReportFilterInterface($src, $dst, $allowFields);
                    } else {
                        // skip interface files unless net explicitly included
                        continue;
                    }
                } elseif (preg_match('/authorized_keys|id_rsa|id_dsa|\.pem$|\.key$/', $f)) {
                    @file_put_contents($dst, "**REDACTED SSH/KEY FILE**\n");
                } else {
                    ErrorReportRedactCopyOrStub($src, $dst);
                }
            }
            $manifest[] = 'config/*' . ($includeNet ? ' (interface allowlisted)' : ' (without interface)');
        }
    } elseif ($includeNet) {
        // Net checked but config not — still emit filtered interface files
        $cfgSrc = $mediaDir . '/config';
        if (is_dir($cfgSrc)) {
            @mkdir($bundleDir . '/config', 0775, true);
            $allowFields = ErrorReportInterfaceFields();
            foreach (scandir($cfgSrc) as $f) {
                if ($f === '.' || $f === '..') continue;
                if (!preg_match('/^interface\./', $f)) continue;
                $src = $cfgSrc . '/' . $f;
                if (!is_file($src)) continue;
                ErrorReportFilterInterface($src, $bundleDir . '/config/' . $f, $allowFields);
            }
            $manifest[] = 'config/interface.* (allowlisted)';
        }
    }

    // Playlists
    if ($includePlaylists) {
        $plSrc = $mediaDir . '/playlists';
        if (is_dir($plSrc)) {
            @mkdir($bundleDir . '/playlists', 0775, true);
            foreach (scandir($plSrc) as $f) {
                if ($f === '.' || $f === '..') continue;
                $src = $plSrc . '/' . $f;
                if (!is_file($src)) continue;
                ErrorReportRedactCopy($src, $bundleDir . '/playlists/' . $f);
            }
            $manifest[] = 'playlists/*';
        }
    }

    // Create zip — add uniqid to avoid collisions within same second
    $ts = date('Ymd-His');
    $zipName = 'error-report-' . $ts . '-' . substr(md5(uniqid('', true)), 0, 6) . '.zip';
    $tmpDir = rtrim($mediaDir, '/') . '/tmp';
    $zipPath = $tmpDir . '/' . $zipName;
    if (!is_dir($tmpDir) && !@mkdir($tmpDir, 0775, true) && !is_dir($tmpDir)) {
        @exec("rm -rf " . escapeshellarg($tmpRoot));
        return json(array('Status' => 'Error', 'Message' => 'Temp directory not writable'));
    }
    // Use zip -r from bundle dir
    $cmd = "cd " . escapeshellarg($bundleDir) . " && zip -r " . escapeshellarg($zipPath) . " . >/dev/null 2>&1";
    exec($cmd, $out, $ret);
    // Fallback to PHP ZipArchive if shell zip missing
    if ($ret !== 0 || !file_exists($zipPath)) {
        if (class_exists('ZipArchive')) {
            $zip = new ZipArchive();
            if ($zip->open($zipPath, ZipArchive::CREATE) === true) {
                $it = new RecursiveIteratorIterator(new RecursiveDirectoryIterator($bundleDir, RecursiveDirectoryIterator::SKIP_DOTS));
                foreach ($it as $file) {
                    $rel = substr($file->getPathname(), strlen($bundleDir) + 1);
                    $zip->addFile($file->getPathname(), $rel);
                }
                $zip->close();
            }
        }
    }

    // Cleanup temp bundle dir
    exec("rm -rf " . escapeshellarg($tmpRoot));

    // Prune old reports (>24h) — best effort
    foreach (glob($mediaDir . '/tmp/error-report-*.zip') as $old) {
        if (file_exists($old) && (time() - filemtime($old) > 86400)) {
            @unlink($old);
        }
    }

    if (!file_exists($zipPath)) {
        return json(array('Status' => 'Error', 'Message' => 'Failed to create bundle'));
    }

    return json(array(
        'Status' => 'OK',
        'file' => $zipName,
        'zipUrl' => 'api/errorReport/download/' . $zipName,
        'manifest' => $manifest,
        'size' => filesize($zipPath),
        'includes' => array(
            'settings' => $includeSettings,
            'fppdLog' => $includeFppdLog,
            'sysLogs' => $includeSysLogs,
            'net' => $includeNet,
            'config' => $includeConfig,
            'playlists' => $includePlaylists,
        ),
    ));
}

// ---------------------------------------------------------------------------
// API: Download bundle
// ---------------------------------------------------------------------------
/**
 * Download an Error Report bundle
 *
 * Streams the previously created zip as `application/zip`. The file name must match `^error-report-[A-Za-z0-9._-]+\.zip$` and must reside in the media tmp directory.
 *
 * @route GET /api/errorReport/download/:file
 * @response 200 Zip download
 * ```bytes
 * [Content-Type: application/zip]
 * ```
 * @response 404 Report not found
 * ```json
 * {"Status":"Error","Message":"Report not found"}
 * ```
 */
function GetErrorReportDownload()
{
    $file = params('file');
    $path = ErrorReportPath($file, $error);
    if ($path === false) {
        http_response_code(404);
        return json(array('Status' => 'Error', 'Message' => $error));
    }
    header('Content-Type: application/zip');
    header('Content-Disposition: attachment; filename="' . basename($path) . '"');
    header('Content-Length: ' . filesize($path));
    header('Cache-Control: no-cache');
    readfile($path);
    exit;
}
