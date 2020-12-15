<!DOCTYPE html>
<html>
<?php
require_once('config.php');

function startsWith($string, $startString)
{
    $len = strlen($startString);
    return (substr($string, 0, $len) === $startString);
}

//ini_set('display_errors', 'On');
error_reporting(E_ALL);

$fpp_version = "v" . getFPPVersion();

$logCmd = "git --git-dir=" . dirname(dirname(__FILE__)) . "/.git/ log --pretty=format:'%h - %<|(30)%an - %<|(46)%ar - %s' | cut -c1-140 | head -200";

$git_log = "";

$currentVersion = "";
if ($uiLevel >= 1) {
    $currentBranch = "";
    $cmd = "git --git-dir=".dirname(dirname(__FILE__))."/.git/ branch -a | grep '^*' | awk '{print \$2}' | sed -e 's/[^a-zA-Z0-9\.]*//'";
    exec($cmd, $output, $return_val);
    if ( $return_val == 0 ) {
        $currentBranch = $output[0];
    }
    unset($output);

    $output = "";
    if ($currentBranch != 'HEAD') {
        $cmd = $logCmd . " | tee -a /home/fpp/media/tmp/gitlog";
        exec($cmd, $output, $return_val);
    } else {
        $cmd = "cat /home/fpp/media/tmp/gitlog";
        exec($cmd, $output, $return_val);
    }

    $cmd = "git --git-dir=" . dirname(dirname(__FILE__)) . "/.git/ log | head -1 | awk '{print \$2}'";
    exec($cmd, $output2, $return_val);
    $currentVersion = $output2[0];

    if ( $return_val == 0 ) {
        foreach ($output as $line) {
            $thisVersion = preg_replace('/^([a-zA-Z0-9-]+).*/', '$1', $line);
            $line = preg_replace('/^([a-zA-Z0-9-]+)/', "<a href='#' onClick='GitCheckoutVersion(\"$1\"); return false;'>$1</a>", $line);

            if (startsWith($currentVersion, $thisVersion)) {
                $git_log .= "--> " . $line . '<br>';
            } else {
                $git_log .= "    " . $line . '<br>';
            }
        }
    }
    unset($output);
} else {
    exec($logCmd, $output, $return_val);
    $git_log = implode('<br>', $output);
}
unset($output);

?>

<head>
<?php include 'common/menuHead.inc'; ?>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
<title>FPP - ChangeLog</title>
<script>
function CloseUpgradeDialog() {
    $('#upgradePopup').dialog('close');
    location.reload();
}

function UpgradeDone() {
    $('#closeDialogButton').show();
}

function GitCheckoutVersion(version) {
    $('#upgradePopup').dialog({ height: 600, width: 900, title: "Switching to version: " + version, dialogClass: 'no-close' });
    $('#upgradePopup').dialog( "moveToTop" );
    $('#upgradeText').html('');

    StreamURL('gitCheckoutVersion.php?wrapped=1&version=' + version, 'upgradeText', 'UpgradeDone');

}
</script>
</head>

<body>
<div id="bodyWrapper">
  <?php include 'menu.inc'; ?>
  <div style="margin:0 auto;"> <br />
    <fieldset style="padding: 10px; border: 2px solid #000;">
      <legend>ChangeLog</legend>
<?
if ($uiLevel >= 1) {
    echo "<b>Click a SHA1 hash to jump to that previous version of code</b><br>";
}
?>
      <pre><?
if ($uiLevel >= 1) {
    echo "    <a href='#' onClick='GitCheckoutVersion(\"HEAD\"); return false;'>HEAD</a>     - (Pull in changes and switch to latest version in this branch)\n";
}
?><? echo $git_log; ?></pre>
    </fieldset>
  </div>
  <?php include 'common/footer.inc'; ?>
</div>
<div id='upgradePopup' title='Switch Version' style="display: none">
    <textarea style='width: 99%; height: 94%;' disabled id='upgradeText'>
    </textarea>
    <input id='closeDialogButton' type='button' class='buttons' value='Close' onClick='CloseUpgradeDialog();' style='display: none;'>
</div>
</body>
</html>
