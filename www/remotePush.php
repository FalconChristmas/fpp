<!DOCTYPE html>
<html>
<?php

/*
 * Transfers a file from the specified directory on this machine 
 * to a remote FPP instance
 */

$skipJSsettings = 1;
require_once "common.php";

DisableOutputBuffering();

?>

<head>
<title>
Remote Push
</title>
</head>
<body>
<h2>Remote File Push</h2>
<pre>
==========================================================================


<?php

$lastUpdate = time();

if (!(isset($_GET['filename']) && isset($_GET['remoteHost']) && isset($_GET['dir']))) {
    echo ("<b>ERROR:</b> filename, remoteHost, and dir are required parameters.<br>");
} else {
    $is_valid = true;
    $filename = sanitizeFilename($_GET["filename"]);
    $remoteHost = $_GET["remoteHost"];
    $dir = $_GET["dir"];

    # Validate Input
    $dir = GetDirSetting($dir);
    $fullPath = "$dir/$filename";

    if ($dir == "") {
        $is_valid = false;
        echo ("<b>Error: Invalid Directory.<br>");
    }

    if (!(is_valid_domain_name($remoteHost) || filter_var($remoteHost, FILTER_VALIDATE_IP))) {
        $is_valid = false;
        echo ("<b>Error:</b>: Invalid Remote Host<br>");
    }

    if (!file_exists($fullPath)) {
        $is_valid = false;
        echo ("<b>Error:</b>: Supplied File not found. - $fullPath<br>");
    }

    if ($is_valid) {
        echo ("Uploading $fullPath to $remoteHost...<br>");

        $cfile = new CURLFile(realpath($fullPath));
        $post = array(
            'myfile' => $cfile,
        );
        $ch = curl_init();
        curl_setopt($ch, CURLOPT_URL, "$remoteHost/jqupload.php");
        curl_setopt($ch, CURLOPT_POST, 1);
        curl_setopt($ch, CURLOPT_CONNECTTIMEOUT, 10); // Connect in 10 seconds or less
        curl_setopt($ch, CURLOPT_TIMEOUT, 86400);     // 1 Day Timeout to transfer
        curl_setopt($ch, CURLOPT_USERAGENT, "Mozilla/4.0 (compatible;)");
        curl_setopt($ch, CURLOPT_HTTPHEADER, array('Content-Type: multipart/form-data'));
        curl_setopt($ch, CURLOPT_FRESH_CONNECT, 1);
        curl_setopt($ch, CURLOPT_FORBID_REUSE, 1);
        curl_setopt($ch, CURLOPT_POSTFIELDS, $post);
        curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
        curl_setopt($ch, CURLOPT_PROGRESSFUNCTION, 'progress');
        curl_setopt($ch, CURLOPT_NOPROGRESS, false); // needed to make progress function work
        $result = curl_exec($ch);

        if ($result === false) {
            echo "Error sending" . $fname . " " . curl_error($ch);
            curl_close($ch);
        } else {
            curl_close($ch);
            move($remoteHost, $filename);
            echo "Complete:" . $result;
        }
    }
}

function progress($resource, $download_size, $downloaded, $upload_size, $uploaded)
{
    global $lastUpdate;
    $now = time();
    if ($now <= $lastUpdate) {
        return;
    } else {
        $lastUpdate = $now + 1;
    }

    if ($upload_size > 0) {
        echo("Progress: ");
        echo intval($uploaded / $upload_size * 100);
        echo("%");
    } else {
        echo("Finding Remote Host");
    }
    echo("\n");

    flush();
}

function move($remoteHost, $file) {
    $ch = curl_init();
    curl_setopt($ch, CURLOPT_URL, "$remoteHost//api/file/move/$file");
    curl_setopt($ch, CURLOPT_CONNECTTIMEOUT, 10); // Connect in 10 seconds or less
    curl_setopt($ch, CURLOPT_TIMEOUT, 10);     // 1 Day Timeout to transfer
    curl_setopt($ch, CURLOPT_USERAGENT, "Mozilla/4.0 (compatible;)");
    curl_setopt($ch, CURLOPT_FRESH_CONNECT, 1);
    curl_setopt($ch, CURLOPT_FORBID_REUSE, 1);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($ch, CURLOPT_FAILONERROR, true);

    $result = curl_exec($ch);

    if ($result === false) {
        echo "Error calling move URL";
        curl_close($ch);
    } else {
        curl_close($ch);
        echo "Complete:" . $result;
    }

    echo("\n");

}

?>

Done!
</pre>
</body>
</html>
