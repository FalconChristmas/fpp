<?
header("Access-Control-Allow-Origin: *");

$wrapped = 0;

if (isset($_GET['wrapped']))
    $wrapped = 1;

if (!$wrapped)
    echo "<html>\n";

$skipJSsettings = 1;
require_once("common.php");

DisableOutputBuffering();

if (!$wrapped) {
    ?>

    <head>
        <title>
            Rebuild FPP
        </title>
    </head>

    <body>
        <h2>Rebuild FPP</h2>
        <pre>
    <?
}

// Only output script tags if being viewed directly in a browser (not via curl/proxy)
$isDirectView = !empty($_SERVER['HTTP_USER_AGENT']) && strpos($_SERVER['HTTP_USER_AGENT'], 'curl') === false;
?>
Stopping fppd...
<?php
if (file_exists("/etc/fpp/container")) {
    system($SUDO . " " . $settings['fppDir'] . "/scripts/fppd_stop");
} else if ($settings["Platform"] == "MacOS") {
    exec("launchctl stop falconchristmas.fppd");
} else {
    exec($SUDO . " systemctl stop fppd");
}

touch("$mediaDirectory/tmp/fppd_restarted");
flush();
ob_flush();
if ($isDirectView) {
    echo '<script>window.scrollTo(0, document.body.scrollHeight);</script>';
    flush();
    ob_flush();
}
?>
==========================================================================
Rebuilding FPP...
<?
system($SUDO . " " . $settings['fppDir'] . "/scripts/fpp_build");
flush();
ob_flush();
if ($isDirectView) {
    echo '<script>window.scrollTo(0, document.body.scrollHeight);</script>';
    flush();
    ob_flush();
}
?>
==========================================================================
Restarting fppd...
<?
system($SUDO . " " . $settings['fppDir'] . "/scripts/fppd_start");
exec($SUDO . " rm -f /tmp/cache_*.cache");
flush();
ob_flush();
if ($isDirectView) {
    echo '<script>window.scrollTo(0, document.body.scrollHeight);</script>';
    flush();
    ob_flush();
}
?>
==========================================================================
Rebuild Complete.
<?
if (!$wrapped) {
    ?>
    <a href='index.php'>Go to FPP Main Status Page</a><br>
    <a href='about.php'>Go back to FPP About page</a><br>
    <script>
        // Ensure we scroll to bottom on page load
        window.scrollTo(0, document.body.scrollHeight);
    </script>

    </body>
    </html>
    <?
}
?>