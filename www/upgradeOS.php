<?
header("Access-Control-Allow-Origin: *");

$lastUpdate = time();
$applyUpdate = true;
$wrapped = 0;
$baseFile = "unknown";

if (isset($_GET['os'])) {
    $baseFile = escapeshellcmd($_GET['os']);
}

if (isset($_GET['wrapped'])) {
    $wrapped = 1;
}

if (!$wrapped) {
    echo "<html>\n";
}

$skipJSsettings = 1;
require_once "common.php";

DisableOutputBuffering();

if (!$wrapped) {
    ?>
<head>
<title>
FPP OS Uprade
</title>
</head>
<body>
<h2>FPP OS Upgrade</h2>
Image: <?echo strip_tags($_GET['os']); ?><br>
<pre>
<?
} else {
    echo "FPP OS Upgrade\n";
    echo "Image: " . strip_tags($_GET['os']) . "\n";
}

if (preg_match('/^https?:/', $_GET['os'])) {
    echo "==========================================================================\n";
    $baseFile = escapeshellcmd(preg_replace('/.*\/([^\/]*)$/', '$1', $_GET['os']));
    $localFile = fopen("/home/fpp/media/upload/$baseFile", "wb");
    echo "Downloading OS Image:\n";
    $ch = curl_init();
    curl_setopt($ch, CURLOPT_FILE, $localFile);
    curl_setopt($ch, CURLOPT_URL, $_GET['os']);
    curl_setopt($ch, CURLOPT_HEADER, 0);
    curl_setopt($ch, CURLOPT_CONNECTTIMEOUT, 10); // Connect in 10 seconds or less
    curl_setopt($ch, CURLOPT_TIMEOUT, 86400); // 1 Day Timeout to transfer
    curl_setopt($ch, CURLOPT_USERAGENT, "Mozilla/4.0 (compatible;)");
    curl_setopt($ch, CURLOPT_FOLLOWLOCATION, true);
    curl_setopt($ch, CURLOPT_PROGRESSFUNCTION, 'progress');
    curl_setopt($ch, CURLOPT_NOPROGRESS, false); // needed to make progress function work

    $result = curl_exec($ch);

    if ($result) {
        echo ("Download complete...\n");
    } else {
        echo ("Download aborted!\n");
        $applyUpdate = false;
    }

}
?>
==========================================================================
Upgrading OS:
<?
if ($applyUpdate) {
    $TMP_FILE = "/home/fpp/media/tmp/upgradeOS-part1.sh";
    echo ("Checking for previous $TMP_FILE\n");
    if (file_exists($TMP_FILE)) {
        echo ("Cleaning up from previous upgradeOS\n");
        system($SUDO . " rm $TMP_FILE");
    }
    copy("$fppDir/SD/upgradeOS-part1.sh", $TMP_FILE);
    chmod($TMP_FILE, 0775);
    #system($SUDO . " stdbuf --output=L --error=L $TMP_FILE /home/fpp/media/upload/$baseFile");
    system($SUDO . " $TMP_FILE /home/fpp/media/upload/$baseFile");
} else {
    echo ("Skipping update\n");
}
?>
==========================================================================
<?
if (!$wrapped) {
    ?>
</pre>
<b>Rebooting.....Close this window and refresh the screen. It might take a minute or so for FPP to reboot</b>
<a href='index.php'>Go to FPP Main Status Page</a><br>
<a href='about.php'>Go back to FPP About page</a><br>
</body>
</html>
<?
} else {
    echo "Rebooting.....Close this window and refresh the screen. It might take a minute or so for FPP to reboot\n";
}
while (@ob_end_flush());
flush();
session_write_close();

if ($applyUpdate) {
    sleep(3);
    system($SUDO . " shutdown -r now");
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
