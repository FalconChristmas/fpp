<?
header("Access-Control-Allow-Origin: *");

$wrapped = 0;

if (isset($_GET['wrapped']))
  $wrapped = 1;

if (!$wrapped)
  echo "<html>\n";

$skipJSsettings = 1;
require_once("common.php");
// Shared operation logging -> logs/fpp_system_upgrades.log, the same file the scripts
// this page drives (git_pull, and upgrade_config beneath it) append to.
require_once("common/oplog.inc.php");
// Atomic server-side update lock: refuse (409) when another update starter
// already holds it, before any mutating work. Fail-open when flock is
// unavailable. Held for the whole request (auto-released on kill).
require_once("common/updateLock.inc.php");

DisableOutputBuffering();

// Finish once started, even if the browser goes away: a closed tab must not stop
// PHP at the next echo.  git_pull survives behind its tee, but the fppd restart
// and the Apache/CSP updates after it would be skipped.
ignore_user_abort(true);

// Serialize concurrent starters (two browsers / direct calls can both pass
// the client-side busy guard before either poll observes the other).
UpdateLockAcquireOrConflict('fpp-update', 'manual');

// Label this page's log lines with the branch, matching how scripts/git_pull
// tags the run it is about to start, so the PHP-side phases and the script-side
// output read as one "[fpp-update <branch>]" run. Best-effort: an unreadable or
// detached checkout logs as "unknown" rather than holding up the update.
$upgradeTarget = trim(@exec("git --git-dir=" . escapeshellarg($fppDir . "/.git") . " rev-parse --abbrev-ref HEAD 2>/dev/null"));
if ($upgradeTarget === '') {
  $upgradeTarget = 'unknown';
}

// Emit a "===== <message> =====" stage header, matching the shell helper
// logStage() in scripts/common, so the streaming FPP Upgrade dialog can drive
// its status line from PHP-side phases too (git_pull emits its own). Reads as a
// section header in the log and doubles as the machine-parsed progress marker.
// Also recorded in fpp_system_upgrades.log: these phases run entirely in PHP, after
// git_pull has exited and its tee is gone, so without this the tail of an
// upgrade -- including whether it actually succeeded -- left no trace on disk.
function logStage($msg)
{
  global $upgradeTarget;
  echo "===== $msg =====\n";
  UpgradeLog('fpp-update', $upgradeTarget, "===== $msg =====");
}

if (!$wrapped) {
  ?>

  <head>
    <title>
      FPP Manual Update
    </title>
  </head>

  <body>
    <h2>FPP Manual Update</h2>
    <pre>
        <?
}
?>
Pulling in updates...
<?
// git_pull installs its own tee onto fpp_system_upgrades.log, so its output is logged
// by the script side; only the elapsed time and exit status are added here.
$startTime = microtime(true);
system("$fppDir/scripts/git_pull", $return_val);
$endTime = microtime(true);
$diffTime = round($endTime - $startTime);

$h = floor($diffTime / 3600);
$m = floor($diffTime % 3600 / 60);
$s = floor($diffTime % 60);

$elapsed = sprintf("Elapsed Time: %02d:%02d:%02d", $h, $m, $s);
printf("----------------------\n%s\n", $elapsed);
UpgradeLog('fpp-update', $upgradeTarget, $elapsed . " (git_pull rc=" . $return_val . ")");
?>
<?

// scripts/git_pull exits 3 when the pull succeeded but the build failed: the
// tree is already on the new code, so fppd is still restarted and the Apache
// config / CSP refreshed for it below, and the run is then reported failed.
// Any other nonzero status is a failed pull that left the old tree in place.
$buildFailed = ($return_val === 3);

if ($return_val === 0 || $buildFailed) {
  logStage("Restarting FPP");

  // Compare and copy apache config if needed
  $srcConf = "/opt/fpp/etc/apache2.site";
  $dstConf = "/etc/apache2/sites-enabled/000-default.conf";
  $needCopy = true;

  if (file_exists($srcConf) && file_exists($dstConf)) {
    // Compare file hashes
    if (md5_file($srcConf) === md5_file($dstConf)) {
      $needCopy = false;
    }
  }

  if ($needCopy) {
    UpgradeEchoLog('fpp-update', $upgradeTarget, "Updating Apache config...\n");
    // Use sudo if needed for permissions
    system("$SUDO cp $srcConf $dstConf");
  } else {
    UpgradeEchoLog('fpp-update', $upgradeTarget, "Apache config is already up to date.\n");
  }

  touch("$mediaDirectory/tmp/fppd_restarted");

  system($SUDO . " $fppDir/scripts/fppd_restart");

  system($SUDO . " $fppDir/scripts/ManageApacheContentPolicy regenerate");

  if (file_exists($settings['statsFile'])) {
    unlink($settings['statsFile']);
  }

  exec($SUDO . " rm -f /tmp/cache_*.cache");
  if (!$buildFailed && file_exists($fppDir . "/src/fppd")) {
    logStage("Upgrade Complete");
  } else {
    // The build failed, or git_pull returned 0 but produced no fppd binary.
    // Record the reason: the banner below only shouts "FAILED" at the
    // browser, and this is the one line that says why when it turns up in a
    // Support Zip later.
    if ($buildFailed) {
      UpgradeLog('fpp-update', $upgradeTarget, "ERROR: updates were pulled but the build failed (git_pull rc=3).");
    } else {
      UpgradeLog('fpp-update', $upgradeTarget, "ERROR: git_pull succeeded but " . $fppDir . "/src/fppd is missing -- build failed.");
    }
    logStage("Upgrade Failed");
    print ("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    print ("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    print ("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    print ("Upgrade FAILED.\n");
    print ("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    print ("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    print ("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
  }
} else {
  UpgradeLog('fpp-update', $upgradeTarget, "ERROR: git_pull exited rc=" . $return_val . "; not restarting FPP.");
  logStage("Upgrade Failed");
  print ("Upgrade FAILED. See above for details.\n");
}

if (!$wrapped) {
  ?>
        <a href='index.php'>Go to FPP Main Status Page</a><br>
        <a href='about.php'>Go back to FPP Upgrade page</a><br>

        </body>
        </html>
        <?
}
?>
