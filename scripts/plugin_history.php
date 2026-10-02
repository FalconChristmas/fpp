<?php
/*
 * The Plugins group under Help > Troubleshooting Commands
 * (www/troubleshoot-commands.json).
 *
 *   (no option)  The plugin install history (www/common/pluginhistory.inc.php),
 *                oldest first in the player's time zone, then what is
 *                installed now and what was removed.
 *   --summary    The same without clone URLs, and only the last
 *                SUMMARY_MAX_EVENTS events: the form manual Diagnostic Reports
 *                get. (Installed plugins' URLs are in every crash report's
 *                plugins.json anyway; this keeps removed and unlisted ones out.)
 *   --leftovers  Settings and data of plugins that are no longer installed
 *                (config/plugin.*, plugindata/*), matched by name prefix, so a
 *                guide, not a verdict; and system packages still
 *                claimed in config/userpackages.json by plugins that are gone.
 */

if (PHP_SAPI !== 'cli') {
    die('This script is for the command line only.');
}

$mode = isset($argv[1]) ? $argv[1] : '';
if (!in_array($mode, array('', '--summary', '--leftovers'), true)) {
    fwrite(STDERR, "Usage: plugin_history.php [--summary | --leftovers]\n");
    exit(1);
}
$showURL = ($mode !== '--summary'); // clone URLs: not in the Diagnostic Report form
// The history is never trimmed, so the Diagnostic Report form lists only the
// latest events; the summary at the end is still built from every one.
define('SUMMARY_MAX_EVENTS', 100);

// FPP's own settings: the media, config and plugin directories (which can all
// be moved) and the player's TimeZone, which is what the history's times are
// written in. Same bootstrap as scripts/plugin_update_check.php.
$fppDir = dirname(__DIR__);
ob_start();
if (!isset($_SERVER['REQUEST_URI'])) {
    $_SERVER['REQUEST_URI'] = '/cli/plugin_history';
}
if (!isset($_SERVER['SCRIPT_NAME'])) {
    $_SERVER['SCRIPT_NAME'] = '/cli/plugin_history';
}
require_once($fppDir . '/www/config.php');
ob_end_clean();
require_once($fppDir . '/www/common/pluginhistory.inc.php');

$mediaDir = rtrim($settings['mediaDirectory'], '/');
$cfgDir = rtrim($settings['configDirectory'], '/');
$pluginDir = rtrim($settings['pluginDirectory'], '/');

// What git knows about an installed copy: its commit, that commit's date and
// where it was cloned from (credentials stripped). Empty strings when it is not
// a git checkout.
function gitInfo($dir)
{
    $git = 'git -c safe.directory=' . escapeshellarg($dir) . ' -C ' . escapeshellarg($dir);
    $out = trim((string) shell_exec($git . ' log -1 --format=%H%x09%ct 2>/dev/null'));
    $parts = explode("\t", $out);
    $url = trim((string) shell_exec($git . ' config --get remote.origin.url 2>/dev/null'));
    return array(
        'sha' => isset($parts[0]) ? substr($parts[0], 0, 8) : '',
        'date' => (isset($parts[1]) && ctype_digit($parts[1])) ? date('j M Y', (int) $parts[1]) : '',
        'url' => PluginHistoryURL($url),
    );
}

// Column widths in characters, not bytes, so a non-ASCII plugin name lines up;
// a column is never padded past COLUMN_MAX, so one very long name can't push
// every other row across the page (it keeps a two-space gap instead).
define('COLUMN_MAX', 40);
function width($s)
{
    $n = preg_match_all('/./us', $s);
    return $n === false ? strlen($s) : $n;
}
function pad($s, $w)
{
    return $s . str_repeat(' ', max(2, $w - width($s)));
}
function colWidth($strings)
{
    return min(COLUMN_MAX, max(array_map('width', $strings)));
}

function shownPath($f, $mediaDir)
{
    return strpos($f, $mediaDir . '/') === 0 ? substr($f, strlen($mediaDir) + 1) : $f;
}

if ($mode === '--leftovers') {
    $installed = PluginDirsInstalled($pluginDir);
    $found = array();
    $files = array_merge(glob($cfgDir . '/plugin.*') ?: array(), glob($mediaDir . '/plugindata/*') ?: array());
    foreach ($files as $f) {
        $name = preg_replace('/^plugin\./', '', basename($f));
        $owned = false;
        foreach ($installed as $p) {
            if ($name === $p || strpos($name, $p . '.') === 0 || strpos($name, $p . '-') === 0 || strpos($name, $p . '_') === 0) {
                $owned = true;
                break;
            }
        }
        if (!$owned) {
            $found[shownPath($f, $mediaDir)] = @filemtime($f);
        }
    }
    uksort($found, 'strnatcasecmp');
    if (empty($found)) {
        echo "No settings or data found for plugins that are not installed.\n";
    } else {
        echo "Settings and data on this player for plugins that are not installed:\n\n";
        // When each was last changed, to set against the history's dates.
        $w = max(array_map('width', array_keys($found))); // paths: no cap
        foreach ($found as $f => $mtime) {
            echo rtrim('  ' . pad($f, $w + 2) . ($mtime ? 'changed ' . date('j M Y', $mtime) : '')) . "\n";
        }
    }

    // System packages FPP installed for a plugin that is gone. A normal
    // uninstall releases its claims (ReleasePackageClaims() in
    // www/common/packages.inc.php) and removes what nothing else needs; a
    // plugin removed any other way leaves them claimed, and they are never
    // removed. Python packages are installed system-wide and not tracked, so
    // they cannot be listed.
    $orphans = array();   // plugin -> packages
    $data = json_decode((string) @file_get_contents($cfgDir . '/userpackages.json'), true);
    foreach (is_array($data) ? $data : array() as $entry) {
        if (!is_array($entry) || !isset($entry['package']) || !is_string($entry['package'])) {
            continue; // a bare string is a Package Manager install, not a plugin's
        }
        foreach ((isset($entry['requestedBy']) && is_array($entry['requestedBy'])) ? $entry['requestedBy'] : array() as $r) {
            if (is_string($r) && $r !== 'user' && !in_array($r, $installed, true)) {
                $orphans[$r][] = $entry['package'];
            }
        }
    }
    echo "\n";
    if (empty($orphans)) {
        echo "No system packages are still claimed by plugins that are not installed.\n";
        exit(0);
    }
    // Whether each is still installed, where dpkg exists.
    $status = array();
    $all = array_unique(array_merge(...array_values($orphans)));
    $haveDpkg = trim((string) shell_exec('command -v dpkg-query 2>/dev/null')) !== '';
    if ($haveDpkg) {
        $out = (string) shell_exec('dpkg-query -W -f=\'${Package} ${Architecture} ${db:Status-Status}\n\' ' .
            implode(' ', array_map('escapeshellarg', $all)) . ' 2>/dev/null');
        // Claims are arch-less (PackageBaseName()); one counts as installed if
        // any architecture of it is (name:arch handled for hand edits).
        foreach (explode("\n", $out) as $l) {
            $parts = preg_split('/\s+/', trim($l));
            if (count($parts) >= 3) {
                $inst = ($parts[2] === 'installed');
                $status[$parts[0]] = !empty($status[$parts[0]]) || $inst;
                $status[$parts[0] . ':' . $parts[1]] = $inst;
            }
        }
    }
    ksort($orphans, SORT_NATURAL | SORT_FLAG_CASE);
    echo "System packages FPP installed for plugins that are not installed (left claimed, so never removed):\n\n";
    $w = colWidth(array_keys($orphans));
    foreach ($orphans as $plugin => $pkgs) {
        $shown = array();
        foreach ($pkgs as $pkg) {
            // dpkg lists nothing at all for a purged package
            $shown[] = $pkg . ($haveDpkg && empty($status[$pkg]) ? ' (not installed)' : '');
        }
        echo '  ' . pad($plugin, $w + 2) . implode(', ', $shown) . "\n";
    }
    exit(0);
}

// One JSON object per line. A line that does not parse (an interrupted write,
// a hand edit) is skipped rather than ending the report; the NULs a power cut
// can leave before the next line are dropped first.
$events = array();
$lines = @file(PluginHistoryFile(), FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
foreach (is_array($lines) ? $lines : array() as $l) {
    $e = json_decode(ltrim($l, "\0"), true);
    if (is_array($e) && isset($e['plugin']) && is_string($e['plugin'])
        && ($e['plugin'] !== '' || ($e['action'] ?? '') === 'copied')) {
        $events[] = $e;
    }
}

$sources = array(
    'official' => 'official',
    'listed' => 'plugin list',
    'unknown' => 'not on plugin list',
    'unverified' => 'unverified, list unreachable',
);

function str($v)
{
    return is_scalar($v) ? (string) $v : '';
}

function when($at)
{
    // Only a real timestamp: strtotime() reads odd strings ("x") as times too.
    $t = preg_match('/^\d{4}-\d\d-\d\d/', str($at)) ? strtotime(str($at)) : false;
    return $t === false ? str($at) : date('j M Y H:i', $t);
}

function shortSha($s)
{
    return substr(str($s), 0, 8);
}

// The player's own architecture and "all" are left off package names ("gpgv",
// not "gpgv:arm64"); a foreign one is kept.
$nativeArch = trim((string) @shell_exec('dpkg --print-architecture 2>/dev/null'));

$rows = array();
foreach ($events as $e) {
    $action = str($e['action'] ?? '?');
    // A Copy Settings restore (copystorage.php): one line across the table.
    if ($action === 'copied') {
        $modes = is_array($e['modes'] ?? null) ? array_map('str', $e['modes']) : array();
        $from = array('FROMUSB' => 'USB storage', 'FROMREMOTE' => 'another player', 'FROMLOCAL' => 'local storage')[str($e['direction'] ?? '')] ?? 'storage';
        $rows[] = array(
            'fpp' => str($e['fppVersion'] ?? '?'),
            'day' => substr(str($e['at'] ?? ''), 0, 10),
            'line' => when($e['at'] ?? '') . '  Copy Settings restored "' . implode('", "', $modes) . '" from ' . $from . ': ' .
                (in_array('All', $modes, true)
                    ? 'the entries above came with it and may describe another player'
                    : (in_array('Plugins', $modes, true)
                        ? 'plugins may have changed outside the Plugin Manager'
                        : 'FPP settings were replaced (not plugins or their settings)')),
            'cols' => array(),
            'url' => '',
            'pkgRows' => array(),
        );
        continue;
    }
    $verb = array('install' => 'Installed', 'upgrade' => 'Updated', 'uninstall' => 'Removed', 'deleted' => 'Deleted', 'verified' => 'Verified')[$action] ?? ucfirst($action);
    $commit = shortSha($e['sha'] ?? '');
    $notes = array();
    if (!empty($e['fromSha']) && $e['fromSha'] !== ($e['sha'] ?? '')) {
        $notes[] = 'from ' . shortSha($e['fromSha']);
    }
    $source = str($e['source'] ?? '');
    if ($source !== '') {
        $notes[] = $sources[$source] ?? $source;
    }
    if (!empty($e['reinstall'])) {
        $notes[] = 'part of a reinstall';
    }
    if ($action === 'deleted') {
        $notes[] = 'by a settings reset (its uninstall script did not run)';
    }
    if (!empty($e['dependency'])) {
        $notes[] = 'as a dependency';
    }
    if (!empty($e['dependencyFailed'])) {
        $notes[] = 'its dependencies could not be installed, so its script did not run';
    }
    if (!empty($e['scriptFailed'])) {
        $notes[] = 'its script FAILED';
    }
    // System packages changed while its script ran (PluginPackageChanges()): a
    // note here, then one row per package, as array(kind, name, detail).
    $pkgRows = array();
    if (!empty($e['packages']) && is_array($e['packages'])) {
        foreach (PluginPackageChangeLabels() as $kind => $label) {
            if (empty($e['packages'][$kind]) || !is_array($e['packages'][$kind])) {
                continue;
            }
            $first = true;
            foreach ($e['packages'][$kind] as $item) {
                $parts = explode(' ', str($item), 2);
                $name = preg_replace('/:(all|' . preg_quote($nativeArch, '/') . ')$/', '', $parts[0]);
                $pkgRows[] = array($first ? $label : '', $name, $parts[1] ?? '');
                $first = false;
            }
            $more = (int) ($e['packages'][$kind . 'Count'] ?? 0) - count($e['packages'][$kind]);
            if ($more > 0) {
                $pkgRows[] = array('', "... and $more more", '');
            }
        }
        if (!empty($pkgRows)) {
            $notes[] = 'packages changed:';
        }
    }
    $rows[] = array(
        'fpp' => str($e['fppVersion'] ?? '?'),
        'day' => substr(str($e['at'] ?? ''), 0, 10),
        'cols' => array(when($e['at'] ?? ''), $verb, $e['plugin'], $commit, implode(', ', $notes)),
        'url' => ($showURL && !empty($e['srcURL'])) ? str($e['srcURL']) : '',
        'pkgRows' => $pkgRows,
    );
}

$hiddenRows = 0;
if ($mode === '--summary' && count($rows) > SUMMARY_MAX_EVENTS) {
    $hiddenRows = count($rows) - SUMMARY_MAX_EVENTS;
    $rows = array_slice($rows, -SUMMARY_MAX_EVENTS);
}

$widths = array();
foreach ($rows as $r) {
    foreach ($r['cols'] as $i => $c) {
        $widths[$i] = min(COLUMN_MAX, max($widths[$i] ?? 0, width($c)));
    }
}

if (empty($events)) {
    echo "No plugin install history yet. It starts with the first plugin installed, updated or removed on this version of FPP.\n";
} else {
    if ($hiddenRows > 0) {
        echo 'Plugin install history: the last ' . count($rows) . ' events only, of ' . ($hiddenRows + count($rows)) .
            ' in all, oldest first. Times are ' . date_default_timezone_get() . ".\n";
        echo "The full history is under Help > Troubleshooting Commands > Plugins > Install History.\n";
    } else {
        echo 'Plugin install history, oldest first. Times are ' . date_default_timezone_get() . ".\n";
    }
}
$fpp = null;
$day = null;
foreach ($rows as $r) {
    if ($r['fpp'] !== $fpp) {
        $fpp = $r['fpp'];
        echo "\nOn FPP $fpp:\n";
    } else if ($r['day'] !== $day) {
        echo "\n"; // a blank line between days
    }
    $day = $r['day'];
    if (isset($r['line'])) {
        echo '  ' . $r['line'] . "\n";
        continue;
    }
    $line = '  ';
    foreach ($r['cols'] as $i => $c) {
        $line .= pad($c, $widths[$i] + 2);
    }
    echo rtrim($line) . "\n";
    if ($r['url'] !== '') {
        echo str_repeat(' ', 4 + $widths[0]) . 'from ' . $r['url'] . "\n";
    }
    if (!empty($r['pkgRows'])) {
        $kw = max(array_map(function ($p) { return width($p[0]); }, $r['pkgRows']));
        $nw = colWidth(array_map(function ($p) { return $p[1]; }, $r['pkgRows']));
        foreach ($r['pkgRows'] as $p) {
            echo rtrim(str_repeat(' ', 4 + $widths[0]) . pad($p[0], $kw + 2) . pad($p[1], $nw + 2) . $p[2]) . "\n";
        }
    }
}

// What became of every plugin the history mentions. A plugin whose last
// entry is an uninstall or a reset's delete was removed then; one that is
// gone although its last entry is neither (an rm over SSH, a restore) was
// removed some other way, and its last entry is only when it was last seen.
$installed = PluginDirsInstalled($pluginDir);
$last = array();
foreach ($events as $e) {
    if (!in_array(str($e['action'] ?? ''), array('verified', 'copied'), true)) {
        $last[$e['plugin']] = $e;
    }
}
// Installed, but the history does not account for it: never recorded
// (installed before the history began, copied in by Copy Settings, cloned by
// hand) or last recorded as removed and since put back outside the Plugin
// Manager. One whose last entry is a failed uninstall is listed on its own.
$unrecorded = array();
$stuck = array(); // name => when the uninstall failed
foreach ($installed as $name) {
    $act = isset($last[$name]) ? str($last[$name]['action'] ?? '') : '';
    if ($act === 'uninstall' && !empty($last[$name]['scriptFailed'])) {
        $stuck[$name] = when($last[$name]['at'] ?? '');
    } else if ($act !== 'install' && $act !== 'upgrade') {
        $unrecorded[] = $name;
    }
}
$removed = array(); // name => array(when, note)
foreach ($last as $name => $e) {
    $name = (string) $name; // an all-digit name is an integer key
    if (in_array($name, $installed, true)) {
        continue;
    }
    $removed[$name] = in_array(str($e['action'] ?? ''), array('uninstall', 'deleted'), true)
        ? array(when($e['at'] ?? ''), '')
        : array(when($e['at'] ?? ''), 'removed without a record; last seen then');
}
sort($installed, SORT_NATURAL | SORT_FLAG_CASE);
echo "\nInstalled now:" . (empty($installed) ? " none\n" : "\n");
foreach ($installed as $name) {
    echo "  $name\n";
}
echo "\n";
if (!empty($unrecorded)) {
    sort($unrecorded, SORT_NATURAL | SORT_FLAG_CASE);
    echo "Installed with no record in this history (installed before it began, copied in, or installed by hand):\n";
    $w = colWidth($unrecorded);
    foreach ($unrecorded as $name) {
        $g = gitInfo($pluginDir . '/' . $name);
        $line = '  ' . pad($name, $w + 2) . str_pad($g['sha'] !== '' ? $g['sha'] : '(not a git checkout)', 10);
        if ($g['date'] !== '') {
            $line .= 'commit of ' . $g['date'];
        }
        if ($showURL && $g['url'] !== '') {
            $line .= '  from ' . $g['url'];
        }
        echo rtrim($line) . "\n";
    }
    echo "\n";
}
if (!empty($stuck)) {
    uksort($stuck, 'strnatcasecmp');
    echo "Still installed after an uninstall that failed:\n";
    $w = colWidth(array_map('strval', array_keys($stuck)));
    foreach ($stuck as $name => $at) {
        echo '  ' . pad((string) $name, $w + 2) . $at . "\n";
    }
    echo "\n";
}
echo 'Removed since the history began:' . (empty($removed) ? " none\n" : "\n");
if (!empty($removed)) {
    uksort($removed, 'strnatcasecmp');
    $w = colWidth(array_map('strval', array_keys($removed)));
    $ww = colWidth(array_map(function ($r) { return $r[0]; }, $removed));
    foreach ($removed as $name => $r) {
        echo rtrim('  ' . pad((string) $name, $w + 2) . pad($r[0], $ww + 2) . $r[1]) . "\n";
    }
}
