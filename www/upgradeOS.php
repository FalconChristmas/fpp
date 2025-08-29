<?
header("Access-Control-Allow-Origin: *");

$lastUpdate = time();
$applyUpdate = true;
$wrapped = 0;
$baseFile = "unknown";
$keepOptFPP = 0;

if (isset($_GET['os'])) {
    $baseFile = escapeshellcmd($_GET['os']);
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

DisableOutputBuffering();

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
    $baseFile = escapeshellcmd(preg_replace('/.*\/([^\/]*)$/', '$1', $_GET['os']));


    // For now, we'll fork wget to get the file.   There is an issue with OpenSSL combined with the cURL built into PHP
    // on certain versions of debian (which includes what was shipped with FPP 8.5) which is causing very 
    // slow transfers if using the above curl_easy stuff.   

    $retryCount = 0;
    $command = "sudo wget -c --quiet --show-progress --progress=bar:force:noscroll " . $_GET['os'] . " -O /home/fpp/media/upload/$baseFile 2>&1";
    //$command = "sudo curl --progress-bar -L " . $_GET['os'] . " -o /home/fpp/media/upload/$baseFile 2>&1";
    $rc = 1;
    while ($retryCount < 20 && $rc != 0) {
        echo "Running command: $command\n";
        passthru($command, $rc);
        $retryCount++;
    }
    if ($rc != 0) {
        echo ("Download aborted!\n");
        $applyUpdate = false;
    } else {
        echo ("Download complete...\n");
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

if ($applyUpdate) {
    echo "==========================================================================\n";
    echo "Upgrading OS:\n";

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
    system($SUDO . " $TMP_FILE /home/fpp/media/upload/$baseFile", $return_code);
} else {
    echo ("Skipping update\n");
}

if (!$wrapped) {
    ?></pre>
        ==========================================================================
        <b>Rebooting.....Close this window and refresh the screen. It might take a minute or so for FPP to reboot</b>
        <a href='index.php'>Go to FPP Main Status Page</a><br>
        <a href='about.php'>Go back to FPP About page</a><br>
    </body>

    </html>
    <?
} else if ($applyUpdate && ($return_code == 0)) {
    echo "==========================================================================\n";
    echo "Rebooting.....Close this window and refresh the screen. It might take a minute or so for FPP to reboot\n";
} else if (!$applyUpdate) {
    echo "==========================================================================\n";
} else {
    echo "==========================================================================\n";
    echo "FPP UPGRADE FAILED\n";
    echo "==========================================================================\n";
}
while (@ob_end_flush())
    ;
flush();
session_write_close();

if ($applyUpdate && ($return_code == 0)) {
    sleep(3);

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