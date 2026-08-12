<?
header("Access-Control-Allow-Origin: *");

$lastUpdate = time();
$applyUpdate = true;
$wrapped = 0;
$baseFile = "unknown";
$keepOptFPP = 0;

if (isset($_GET['os'])) {
    $baseFile = basename($_GET['os']); // strip path components
    // Only allow safe filename characters (alphanumeric, hyphens, underscores, dots)
    if (!preg_match('/^[a-zA-Z0-9._-]+$/', $baseFile)) {
        echo "Invalid OS filename\n";
        exit(1);
    }
}

if (isset($_GET['downloadOnly'])) {
    $applyUpdate = false;
}

if (isset($_GET['wrapped'])) {
    $wrapped = 1;
}

if (isset($_GET['keepOptFPP'])) {
    $keepOptFPP = 1;
}

if (!$wrapped) {
    echo "<html>\n";
    echo "<head>\n";
}

$skipJSsettings = 1;
require_once "common.php";
// Shared operation logging -> logs/fpp_system_upgrades.log. The OS upgrade ends in a
// reboot that replaces the whole root filesystem, so the streamed dialog is
// otherwise the ONLY record of it: close the window (or lose the tab to the
// reboot) and a failed flash leaves nothing behind to diagnose. logs/ lives on
// the media tree, which upgradeOS-part2.sh's rsync never touches (it copies only
// bin etc lib opt root sbin usr var), so this log survives the image swap.
require_once "common/oplog.inc.php";

DisableOutputBuffering();

// Emit a "===== <message> =====" stage header, matching the shell helper
// logStage() in scripts/common (and the same helper in manualUpdate.php), so the
// streaming FPP OS Upgrade dialog can drive its status line from PHP-side phases
// too. The upgradeOS-part1/part2 scripts emit their own stages during the long
// filesystem copy. Reads as a section header in the log and doubles as the
// machine-parsed progress marker (see ParseLastStageMarker() in www/js/fpp.js).
function logStage($msg)
{
    global $baseFile;
    echo "===== $msg =====\n";
    UpgradeLog('os-upgrade', $baseFile, "===== $msg =====");
}

function downloadImage($localFile): bool
{
    echo "==========================================================================\n";
    echo "Downloading OS Image:\n";
    $ch = curl_init();
    curl_setopt($ch, CURLOPT_FILE, $localFile);
    curl_setopt($ch, CURLOPT_URL, $_GET['os']);
    curl_setopt($ch, CURLOPT_HEADER, 0);
    curl_setopt($ch, CURLOPT_CONNECTTIMEOUT, 10); // Connect in 10 seconds or less
    curl_setopt($ch, CURLOPT_TIMEOUT, 86400); // 1 Day Timeout to transfer
    curl_setopt($ch, CURLOPT_FOLLOWLOCATION, true);
    curl_setopt($ch, CURLOPT_PROGRESSFUNCTION, 'progress');
    curl_setopt($ch, CURLOPT_NOPROGRESS, false); // needed to make progress function work
    curl_setopt($ch, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);

    $result = curl_exec($ch);

    if ($result) {
        echo ("Download complete...\n");
        return true;
    } else {
        echo ("Download aborted!\n");
        $applyUpdate = false;
        $msg = curl_error($ch);
        echo ("Error Message: $msg");
    }
    return false;
}

if (!$wrapped) {
    ?>

    <title>
        FPP OS Upgrade
    </title>
    </head>

    <body>
        <h2>FPP OS Upgrade</h2>
        Image: <? echo strip_tags($_GET['os']); ?><br>
        <pre><?
} else {
    echo "\nFPP OS Upgrade\n";
    echo "Image: " . strip_tags($_GET['os']) . "\n";
}

if (preg_match('/^https?:/', $_GET['os'])) {
    $baseFile = basename($_GET['os']);
    if (!preg_match('/^[a-zA-Z0-9._-]+$/', $baseFile)) { echo 'Invalid filename'; exit(1); }


    // For now, we'll fork wget to get the file.   There is an issue with OpenSSL combined with the cURL built into PHP
    // on certain versions of debian (which includes what was shipped with FPP 8.5) which is causing very
    // slow transfers if using the above curl_easy stuff.

    // If an "Upgrade Source" other than github.com is configured (another FPP
    // instance on the LAN), pull the .fppos from that mirror's uploads dir
    // instead of re-downloading from GitHub. This matches the source setting
    // already honored by the FPP-software update path (scripts/git_pull). The
    // mirror is only a candidate; if it fails we fall back to the GitHub URL.
    global $settings;
    $upgradeSource = isset($settings['UpgradeSource']) ? $settings['UpgradeSource'] : 'github.com';
    $urls = array($_GET['os']);
    if ($upgradeSource != '' && $upgradeSource != 'github.com'
        && preg_match('#^https?://github\.com/#i', $_GET['os'])) {
        // $baseFile is the whitelist-validated basename() from above, safe to interpolate.
        array_unshift($urls, "http://" . fppUrlHost($upgradeSource) . "/api/file/uploads/" . $baseFile);
    }

    logStage("Downloading OS image");
    $rc = 1;
    foreach ($urls as $idx => $url) {
        if (count($urls) > 1) {
            if ($idx == 0) {
                UpgradeEchoLog('os-upgrade', $baseFile, "Downloading from local FPP mirror ${upgradeSource}...\n");
            } else {
                UpgradeEchoLog('os-upgrade', $baseFile, "Mirror download failed, falling back to GitHub...\n");
            }
        }
        // --progress=dot:giga (not bar:force): the output of this command is
        // passthru'd straight into the browser's streamed <pre>, not a terminal.
        // The bar style redraws one line via \r, which has nowhere to "redraw" in
        // a streamed HTML response -- passthru() just forwards every \r-terminated
        // update as more bytes appended to the page, so the dialog fills up with a
        // new line per update. dot:giga instead emits real \n-terminated summary
        // lines at large (1GB-per-row) intervals, which suits both the streamed
        // dialog and an OS-image-sized download.
        $command = "sudo wget -c --quiet --show-progress --progress=dot:giga " . $url . " -O /home/fpp/media/upload/$baseFile 2>&1";
        $retryCount = 0;
        while ($retryCount < 20 && $rc != 0) {
            echo "Running command: $command\n";
            // The URL (not the \r-heavy progress bar that passthru streams to the
            // browser) is what matters on disk: which mirror was used, and how many
            // attempts it took. A 20-deep retry loop silently burning through
            // attempts was previously indistinguishable from a first-try success.
            UpgradeLog('os-upgrade', $baseFile, "Downloading " . $url . " (attempt " . ($retryCount + 1) . " of 20)");
            passthru($command, $rc);
            $retryCount++;
        }
        UpgradeLog('os-upgrade', $baseFile, "wget rc=" . $rc . " after " . $retryCount . " attempt(s) from " . $url);
        if ($rc == 0) {
            break;
        }
    }
    if ($rc != 0) {
        UpgradeEchoLog('os-upgrade', $baseFile, "Download aborted!\n");
        $applyUpdate = false;
    } else {
        UpgradeEchoLog('os-upgrade', $baseFile, "Download complete...\n");
    }

    /*
    $localFile = fopen("/home/fpp/media/upload/$baseFile", "wb");

    if (!downloadImage($localFile)) {
        echo "Failed to download image, retrying.\n";
        fclose($localFile);
        unlink("/home/fpp/media/upload/$baseFile");
        $localFile = fopen("/home/fpp/media/upload/$baseFile", "wb");
        downloadImage($localFile);
    }
    fclose($localFile);
    */
}

$full_fppos_path = "/home/fpp/media/upload/$baseFile";

if (!file_exists($full_fppos_path)) {
    UpgradeEchoLog('os-upgrade', $baseFile, "File does not exist, aborting: $full_fppos_path\n");
    $applyUpdate = false;
} else if (filesize($full_fppos_path) == 0) {
    UpgradeEchoLog('os-upgrade', $baseFile, "File is empty, aborting: $full_fppos_path\n");
    $applyUpdate = false;
} else {
    UpgradeLog('os-upgrade', $baseFile, "Image ready: $full_fppos_path (" . filesize($full_fppos_path) . " bytes)");
}

// The fppos upgrade re-flashes the running root filesystem in place
// (upgradeOS-part2.sh rsync's bin etc lib opt root sbin usr var over the live
// root, with the running OS still on disk). If free space is low, that copy can
// die partway through and a half-written root cannot boot. Refuse to start
// unless the root filesystem has room for the image plus the new OS being
// written; the UI-side 200MB guard in about.php is not enforced server-side and
// is too small for the actual copy.
if ($applyUpdate) {
    $imageSize = filesize($full_fppos_path);
    $rootFree = disk_free_space("/");
    $requiredFree = 2 * $imageSize;
    if ($rootFree !== false && $rootFree < $requiredFree) {
        UpgradeEchoLog('os-upgrade', $baseFile, sprintf(
            "Not enough free space on the root filesystem to apply %s: %s bytes free, %s bytes required (2x the image size).\n",
            $baseFile,
            number_format($rootFree),
            number_format($requiredFree)
        ));
        UpgradeLog('os-upgrade', $baseFile, "Aborting - insufficient free space on root filesystem");
        $applyUpdate = false;
    }
}

if ($applyUpdate) {
    logStage("Applying OS image");

    # Ensure /proc/sysrq-trigger is writable by fpp for reboot later.  Do it now whilst libraries are all good
    system($SUDO . " chmod a+w /proc/sysrq-trigger");

    $TMP_FILE = "/home/fpp/media/tmp/upgradeOS-part1.sh";
    echo ("Checking for previous $TMP_FILE\n");
    if (file_exists($TMP_FILE)) {
        echo ("Cleaning up from previous upgradeOS\n");
        system($SUDO . " rm $TMP_FILE");
    }
    copy("$fppDir/SD/upgradeOS-part1.sh", $TMP_FILE);
    if ($keepOptFPP) {
        system($SUDO . " touch /home/fpp/media/tmp/keepOptFPP");
    } else {
        system($SUDO . " rm /home/fpp/media/tmp/keepOptFPP");
    }
    chmod($TMP_FILE, 0775);
    #system($SUDO . " stdbuf --output=L --error=L $TMP_FILE /home/fpp/media/upload/$baseFile");
    $return_code = 0;
    // keepOptFPP decides whether the running /opt/fpp survives the image copy --
    // the difference between "upgraded the OS" and "upgraded the OS and replaced
    // FPP", which is the first thing worth knowing when a box comes back wrong.
    UpgradeLog('os-upgrade', $baseFile, "Running upgradeOS-part1.sh (keepOptFPP=" . ($keepOptFPP ? "1" : "0") . ")");
    // part1 installs its own tee onto fpp_system_upgrades.log and captures part2's
    // chroot'd output too, so its lines are logged by the script side.
    system($SUDO . " $TMP_FILE /home/fpp/media/upload/$baseFile", $return_code);
    UpgradeLog('os-upgrade', $baseFile, "upgradeOS-part1.sh rc=" . $return_code);
} else {
    UpgradeEchoLog('os-upgrade', $baseFile, "Skipping update\n");
}

if (!$wrapped) {
    // Non-streaming mode has no status line to drive, so log the outcome without
    // echoing a stage marker into the HTML. Logged rather than inferred from the
    // banner below, which announces "Rebooting" unconditionally.
    UpgradeLog('os-upgrade', $baseFile, ($applyUpdate && ($return_code == 0)) ? "===== Rebooting =====" : "===== Upgrade Failed =====");
    ?></pre>
        ==========================================================================
        <b>Rebooting.....Close this window and refresh the screen. It might take a minute or so for FPP to reboot</b>
        <a href='index.php'>Go to FPP Main Status Page</a><br>
        <a href='about.php'>Go back to FPP Upgrade page</a><br>
    </body>

    </html>
    <?
} else if ($applyUpdate && ($return_code == 0)) {
    logStage("Rebooting");
    echo "Rebooting.....Close this window and refresh the screen. It might take a minute or so for FPP to reboot\n";
} else if (!$applyUpdate) {
    echo "==========================================================================\n";
} else {
    logStage("Upgrade Failed");
    echo "FPP UPGRADE FAILED\n";
    echo "==========================================================================\n";
}
while (@ob_end_flush())
    ;
flush();
session_write_close();

if ($applyUpdate && ($return_code == 0)) {
    sleep(3);

    # Flush the log (and anything else still dirty) before the sysrq reboot
    # below: "echo b" is an IMMEDIATE reset that does not sync, so without this
    # the tail of fpp_system_upgrades.log -- exactly the part describing the reboot we
    # are about to do -- can be lost from the page cache. part1 syncs before it
    # returns, but the PHP-side lines are written after that.
    #
    # sysrq "s" (kernel-level sync), NOT system("sync"): part2 has just rsynced a
    # new root filesystem over the running one, so /bin/sync and the libraries it
    # needs may no longer be loadable. That is the whole reason 593cda7dd replaced
    # `shutdown -r now` with the sysrq write below, and pre-chmods the trigger at
    # the top of this block "whilst libraries are all good". "echo" is a shell
    # builtin, so this needs no binary the reboot below does not already need, and
    # it goes through the same trigger: sync is sysrq bit 16 and reboot is bit 128,
    # both set in Debian's default kernel.sysrq of 438 -- if one is unavailable the
    # reboot itself would not work either. Do NOT "simplify" this back to sync(1).
    system("echo s > /proc/sysrq-trigger");

    # Force reboot the system, try a variety of methods
    # to see if one will properly trigger
    system("echo b > /proc/sysrq-trigger");

    sleep(1);
    system("echo b | sudo tee /proc/sysrq-trigger");

    sleep(1);
    system("shutdown -r now");
}

function progress($resource, $download_size, $downloaded, $upload_size, $uploaded)
{
    global $lastUpdate;
    global $filename;
    $now = time();
    if ($now <= $lastUpdate) {
        return;
    } else {
        $delta = 1;
        if ($download_size > 0) {
            $delta = 5;
        }
        $lastUpdate = $now + $delta;

    }

    if ($download_size > 0) {
        echo ("Progress: ");
        echo intval($downloaded / $download_size * 100);
        echo ("%");
    } else {
        echo ("Finding Remote Host");
    }
    echo ("\n");

    flush();
}
?>
