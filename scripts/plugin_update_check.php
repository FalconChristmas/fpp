#!/usr/bin/php
<?php
/*
 * Refreshes the installed plugins' remote-tracking refs and records a verdict
 * for each in media/cache/plugin_updates.json, for the navbar icon and the
 * Updates tab. Spawned detached by PluginUpdateStartSweep(): from a page load
 * when the last answer is old, or with --force from Check for Updates;
 * sequential and niced. No play-state check: a page load means someone is at
 * the keyboard. By hand:
 *
 *   sudo -u fpp /opt/fpp/scripts/plugin_update_check.php --force
 */

if (php_sapi_name() !== 'cli') {
    die('This script is for the command line only.');
}

$fppDir = dirname(__DIR__);
// The web includes print markup of their own; this is a CLI tool.
ob_start();
// config.php reads these off the request; there is no request here.
if (!isset($_SERVER['REQUEST_URI'])) {
    $_SERVER['REQUEST_URI'] = '/cli/plugin_update_check';
}
if (!isset($_SERVER['SCRIPT_NAME'])) {
    $_SERVER['SCRIPT_NAME'] = '/cli/plugin_update_check';
}
require_once($fppDir . '/www/config.php');
require_once($fppDir . '/www/common.php');
require_once($fppDir . '/www/api/controllers/plugin.php');
ob_end_clean();

$force = in_array('--force', $argv);
$trigger = $force ? 'manual' : 'background';



function SweepLog($msg)
{
    PluginLog('update-check', 'all', $msg);
}

// The installed plugins that are due, oldest answer first (never checked
// first): a verdict older than $maxAge, or a failed check older than $retryAge.
// Oldest-first makes a pass that runs out of time resumable next time.
function PluginSweepDueList($maxAge, $retryAge)
{
    $state = PluginUpdateStateRead();
    $due = array();
    foreach (InstalledPluginNames() as $plugin) {
        $e = isset($state['plugins'][$plugin]) ? $state['plugins'][$plugin] : array();
        $at = isset($e['checkedAt']) ? (int) $e['checkedAt'] : 0;
        if ($at > time()) {
            $at = 0; // clock moved; treat as never checked
        }
        // A failed check is due again sooner than a real verdict, or the
        // retry sweep after an offline pass would find nothing to do.
        $unanswered = !array_key_exists('updates', $e) || $e['updates'] === null;
        if ((time() - $at) < ($unanswered ? $retryAge : $maxAge)) {
            continue;
        }
        $due[$plugin] = $at;
    }
    asort($due);
    return array_keys($due);
}

// Record a verdict, and log it only if it differs from what was recorded
// before: this log is also the install/upgrade history and goes in the
// support zip, so a plugin that fails for a lasting reason (a private
// repository) must not add a line every pass.
function PluginSweepRecord($plugin, $updates, $error, $since = 0)
{
    $state = PluginUpdateStateRead();
    $prev = isset($state['plugins'][$plugin]) ? $state['plugins'][$plugin] : array();
    $prevUpdates = array_key_exists('updates', $prev) ? $prev['updates'] : 'none';
    $prevError = isset($prev['error']) ? (string) $prev['error'] : '';
    PluginUpdateStateSet($plugin, $updates, $error, 'sweep', $since);
    if ($prevUpdates === $updates && $prevError === (string) $error) {
        return;
    }
    if ($updates === null) {
        PluginLog('update-check', $plugin, "could not check -- $error");
    } else if ($updates) {
        PluginLog('update-check', $plugin, 'update available');
    } else if ($prevUpdates === true) {
        PluginLog('update-check', $plugin, 'up to date');
    }
}

// Fetch one plugin and record its verdict, failures included (so it does not
// stay first in line). Returns 'skipped' (lock held elsewhere), 'fetch-failed',
// 'fetched', or 'no-fetch' (not a clone).
function PluginSweepOne($plugin)
{
    global $settings;
    $dir = $settings['pluginDirectory'] . '/' . $plugin;

    if (!is_dir($dir . '/.git')) {
        // Nothing to fetch, but still record an answer, or it stays oldest for ever.
        list($updates, $error) = PluginUpdateVerdict($plugin);
        PluginSweepRecord($plugin, $updates, $error);
        return 'no-fetch';
    }

    // Someone is checking it by hand. The caller must not pick it again this
    // pass, or it spins on it.
    $lock = PluginFetchLock($plugin, false);
    if ($lock === false) {
        return 'skipped';
    }
    // Same cap as a user's check: a large fetch that is still progressing
    // must not fail here and succeed from the button.
    $cmd = PluginGitFetchCmd($dir, '', true, array('background' => true));
    $since = time();
    unset($output);
    exec($cmd, $output, $rv);
    // Only the fetch needs the lock; the verdict can run the plugin's script
    // for minutes. $since lets PluginUpdateStateSet() drop this verdict if a user
    // action on the plugin lands in that gap.
    fclose($lock);
    if ($rv != 0) {
        PluginSweepRecord($plugin, null, PluginFetchFailureReason($output, $rv), $since);
        return 'fetch-failed';
    }
    list($updates, $error) = PluginUpdateVerdict($plugin);
    PluginSweepRecord($plugin, $updates, $error, $since);
    return 'fetched';
}

$plugins = InstalledPluginNames();
if (empty($plugins)) {
    PluginUpdateStateRecordSweep('ok', 'no plugins installed', $trigger);
    exit(0);
}

// A hand-run --force means "check now", so nothing is too fresh to re-check.
$maxAge = $force ? 0 : PLUGIN_UPDATE_SWEEP_INTERVAL;
$retryAge = $force ? 0 : PLUGIN_UPDATE_SWEEP_RETRY;
$prevSweep = PluginUpdateStateRead()['sweep'];

PluginUpdateStateRecordSweep('in-progress', '', $trigger, true);

$deadline = time() + PLUGIN_UPDATE_SWEEP_MAX_RUNTIME;
$done = 0;
$checkedNow = array();
$fetchTried = 0;
$fetchFailed = 0;
$outOfTime = false;
$due = PluginSweepDueList($maxAge, $retryAge);
$total = count($due);
PluginUpdateStateRecordSweepProgress(0, $total);
foreach ($due as $i => $plugin) {
    if (time() > $deadline) {
        $outOfTime = true;
        break;
    }
    $outcome = PluginSweepOne($plugin);
    if ($outcome !== 'skipped') {
        $done++;
        $checkedNow[] = $plugin;
    }
    if ($outcome === 'fetched' || $outcome === 'fetch-failed') {
        $fetchTried++;
        if ($outcome === 'fetch-failed') {
            $fetchFailed++;
        }
    }
    PluginUpdateStateRecordSweepProgress($i + 1, $total);
}

// Count only what this pass established.
$state = PluginUpdateStateRead();
$failed = 0;
foreach ($checkedNow as $p) {
    if (isset($state['plugins'][$p]) && array_key_exists('updates', $state['plugins'][$p])
        && $state['plugins'][$p]['updates'] === null) {
        $failed++;
    }
}
// The last FINISHED pass. Read from lastResult, not result: the endpoint
// wrote in-progress before this script started, so result is never it.
$prevResult = isset($prevSweep['lastResult']) ? (string) $prevSweep['lastResult'] : '';
$prevRetryAfter = isset($prevSweep['retryAfter']) ? (int) $prevSweep['retryAfter'] : 0;
if (empty($checkedNow)) {
    // Nothing was due (or every due plugin was being checked by hand): this
    // pass learned nothing, so the previous result stands rather than "ok".
    $result = ($prevResult !== '') ? $prevResult : 'ok';
    $message = isset($prevSweep['lastMessage']) ? (string) $prevSweep['lastMessage'] : '';
} else if ($fetchFailed > 0 && $fetchFailed === $fetchTried) {
    // Every fetch failed: most likely the network. Archive installs don't count.
    $result = 'offline';
    $message = 'nothing could be reached';
} else {
    $result = ($failed > 0) ? 'partial' : 'ok';
    $message = ($failed > 0) ? ($failed . ' of ' . count($checkedNow) . ' could not be checked') : '';
}
if ($outOfTime) {
    $result = ($result === 'offline') ? $result : 'partial';
    $message = trim($message . ($message !== '' ? '; ' : '') . 'ran out of time, the rest are checked next time');
}
// When the next sweep may start. Out of time: the retry clock, there is more
// to do. Offline: the retry clock the first time, then doubling towards the
// full interval, so a show network with no route out is not fetched every
// 15 minutes for as long as someone is in the UI.
if ($outOfTime) {
    $retryAfter = PLUGIN_UPDATE_SWEEP_RETRY;
} else if (empty($checkedNow)) {
    // Learned nothing, so no reason to change the clock either.
    $retryAfter = ($prevRetryAfter > 0) ? $prevRetryAfter : PLUGIN_UPDATE_SWEEP_INTERVAL;
} else if ($result === 'offline') {
    $retryAfter = ($prevResult === 'offline' && $prevRetryAfter > 0)
        ? min($prevRetryAfter * 2, PLUGIN_UPDATE_SWEEP_INTERVAL)
        : PLUGIN_UPDATE_SWEEP_RETRY;
} else {
    $retryAfter = PLUGIN_UPDATE_SWEEP_INTERVAL;
}
PluginUpdateStateRecordSweep($result, $message, $trigger, false, $retryAfter);
// One line per pass that did something; a pass with nothing due is silent.
if ($done > 0) {
    SweepLog('checked ' . $done . ' plugin' . ($done == 1 ? '' : 's')
        . ($failed ? (', ' . $failed . ' could not be checked') : '')
        . ($result === 'offline' ? ' (offline)' : '') . ($outOfTime ? ' (ran out of time)' : ''));
}
exit(0);
