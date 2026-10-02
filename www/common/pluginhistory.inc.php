<?php
/*
 * The plugin install history, config/pluginHistory.jsonl: one JSON line per
 * install, upgrade, uninstall, 'verified' (ResolveUnverifiedPlugins() in
 * api/controllers/plugin.php), 'deleted' (removed by the "plugins" reset in
 * resetConfig.php, without its uninstall) and 'copied' (a Copy Settings
 * restore, copystorage.php). scripts/plugin_history.php shows it under
 * Troubleshooting Commands > Plugins.
 *
 * Kept after a plugin is gone, for support: uninstalling may not undo
 * everything an install changed. In config/ so logrotate and the "logs" reset
 * leave it alone, and named so no resetConfig.php area matches it; never
 * trimmed. Not in backups; Copy Settings "All" carries it, which the 'copied'
 * entry marks. Never copied into a crash report. Not tamper-proof.
 */

define('PLUGIN_HISTORY_FILE', 'pluginHistory.jsonl');

function PluginHistoryFile()
{
    global $settings;
    return $settings['configDirectory'] . '/' . PLUGIN_HISTORY_FILE;
}

// The plugins installed in $dir: directories with a pluginInfo.json. A
// plugin's linkName alias is a symlink to one of them, not another plugin
// (InstalledPluginNames() in api/controllers/plugin.php counts it).
function PluginDirsInstalled($dir)
{
    $names = array();
    foreach (glob($dir . '/*', GLOB_ONLYDIR) ?: array() as $d) {
        if (!is_link($d) && file_exists($d . '/pluginInfo.json')) {
            $names[] = basename($d);
        }
    }
    return $names;
}

// The commit a plugin checkout is on, or '' if it is not a git checkout.
// safe.directory: git still answers when another user (root, via a plugin
// script) owns it.
function PluginGitHead($dir)
{
    return trim((string) shell_exec('git -c safe.directory=' . escapeshellarg($dir) . ' -C ' . escapeshellarg($dir) . ' rev-parse HEAD 2>/dev/null'));
}

// A clone URL without embedded credentials: everything up to the last '@'
// before the path goes, so a password containing '@' does too.
function PluginHistoryURL($url)
{
    if (!is_string($url) || $url === '') {
        return '';
    }
    return preg_replace('#^([a-z][a-z0-9+.-]*://)[^/]*@#i', '$1', $url);
}

// Appends one entry ($fields on top of time, action, plugin and FPP version).
// A failure loses at most that entry: a partial last line left by a crash is
// ended first so it costs only itself, and readers skip it. Best effort: never
// fails the operation being recorded.
function AppendPluginHistory($plugin, $action, $fields = array())
{
    $line = json_encode(array_merge(array(
        'at' => date('c'), // the player's local time; the offset keeps it unambiguous
        'action' => $action,
        'plugin' => $plugin,
        'fppVersion' => getFPPVersion(),
    ), $fields), JSON_UNESCAPED_SLASHES | JSON_INVALID_UTF8_SUBSTITUTE);
    if ($line === false) {
        error_log("plugin history: could not encode the $action entry for $plugin");
        return;
    }

    $file = PluginHistoryFile();
    $new = !file_exists($file);
    $fh = @fopen($file, 'a+'); // a+: appends always go to the end, and the last byte can be read
    if (!$fh) {
        error_log("plugin history: could not open $file");
        return;
    }
    flock($fh, LOCK_EX);
    $stat = fstat($fh);
    if ($stat && $stat['size'] > 0 && fseek($fh, -1, SEEK_END) === 0 && fread($fh, 1) !== "\n") {
        $line = "\n" . $line;
    }
    if (fwrite($fh, $line . "\n") !== strlen($line) + 1) {
        error_log("plugin history: could not write the $action entry for $plugin to $file");
    }
    fflush($fh);
    flock($fh, LOCK_UN);
    fclose($fh);
    if ($new) {
        @chmod($file, 0664);
    }
}

// The kinds of system package change PluginPackageChanges() records (in
// api/controllers/plugin.php), with their labels: one list for the operation
// output and the Install History.
function PluginPackageChangeLabels()
{
    return array('removed' => 'removed', 'changed' => 'version changed', 'held' => 'held',
        'unheld' => 'hold released', 'added' => 'installed', 'aptFiles' => 'apt sources/keys/pins');
}
