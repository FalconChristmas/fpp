<?php
require_once('config.php');

$a = session_id();

if(empty($a))
{
  session_start();
}
$_SESSION['session_id'] = session_id();
//ini_set('display_errors', 'On');
error_reporting(E_ALL);

$fpp_version = "v" . exec("git --git-dir=".dirname(dirname(__FILE__))."/.git/ describe --tags", $output, $return_val);
if ( $return_val != 0 )
	$fpp_version = "Unknown";
unset($output);

if (!file_exists("/etc/fpp/config_version") && file_exists("/etc/fpp/rfs_version"))
{
	exec(SUDO . " $fppDir/scripts/upgrade_config");
}

$os_build = "Unknown";
if (file_exists("/etc/fpp/rfs_version"))
{
	$os_build = exec("cat /etc/fpp/rfs_version", $output, $return_val);
	if ( $return_val != 0 )
		$os_build = "Unknown";
	unset($output);
}

$os_version = "Unknown";
if (file_exists("/etc/os-release"))
{
	$info = parse_ini_file("/etc/os-release");
	if (isset($info["PRETTY_NAME"]))
		$os_version = $info["PRETTY_NAME"];
	unset($output);
}

$kernel_version = exec("/bin/uname -r", $output, $return_val);
if ( $return_val != 0 )
	$kernel_version = "Unknown";
unset($output);

$git_version = exec("git --git-dir=".dirname(dirname(__FILE__))."/.git/ rev-parse --short HEAD", $output, $return_val);
if ( $return_val != 0 )
  $git_version = "Unknown";
unset($output);

$git_branch = exec("git --git-dir=".dirname(dirname(__FILE__))."/.git/ branch --list | grep '\\*' | awk '{print \$2}'", $output, $return_val);
if ( $return_val != 0 )
  $git_branch = "Unknown";
unset($output);

$git_remote_version = "Unknown";
$git_remote_version = exec("git ls-remote --heads https://github.com/FalconChristmas/fpp | grep $git_branch | awk '$1 > 0 { print substr($1,1,7)}'", $output, $return_val);
if ( $return_val != 0 )
  $git_remote_version = "Unknown";
unset($output);

function getSymbolByQuantity($bytes) {
  $symbols = array('B', 'KiB', 'MiB', 'GiB', 'TiB', 'PiB', 'EiB', 'ZiB', 'YiB');
  $exp = floor(log($bytes)/log(1024));

  return sprintf('%.2f '. $symbols[$exp], ($bytes/pow(1024, floor($exp))));
}

function getFileCount($dir)
{
  $i = 0;
  if ($handle = opendir($dir))
  {
    while (($file = readdir($handle)) !== false)
    {
      if (!in_array($file, array('.', '..')) && !is_dir($dir . $file))
        $i++;
    }
  }

  return $i;
}

function getRemappedChannelCount()
{
	global $mediaDirectory;

	$file = $mediaDirectory . "/channelremap";

	$f = fopen($file, "r");
	if($f == FALSE)
	{
		return 0;
	}

	$i = 0;
    while (!feof($f))
	{
		$line = fgets($f);
		if (!feof($f))
		{
			$entry = explode(",", $line, 3);
			if (($entry[0] > 0) && ($entry[1] > 0) && ($entry[2] > 0))
			{
				$i += $entry[2];
			}
		}
	}

	fclose($f);

	return $i;
}

function PrintGitBranchOptions()
{
  $branches = Array();
  exec("git --git-dir=".dirname(dirname(__FILE__))."/.git/ branch --list", $branches);
  foreach($branches as $branch)
  {
    if (preg_match('/^\\*/', $branch))
    {
       $branch = preg_replace('/^\\* */', '', $branch);
       echo "<option value='$branch' selected>$branch</option>";
    }
    else
    {
       $branch = preg_replace('/^ */', '', $branch);
       echo "<option value='$branch'>$branch</option>";
    }
  }
}

?>

<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<?php include 'common/menuHead.inc'; ?>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
<script language="Javascript">
$(document).ready(function() {
$('.default-value').each(function() {
var default_value = this.value;
$(this).focus(function() {
if(this.value == default_value) {
this.value = '';
$(this).css('color', '#333');
}
});
$(this).blur(function() {
if(this.value == '') {
$(this).css('color', '#999');
this.value = default_value;
}
});
});
});

function ToggleAutoUpdate() {
	if ($('#autoUpdateDisabled').is(':checked')) {
		SetAutoUpdate(0);
	} else {
		SetAutoUpdate(1);
	}
}

function ToggleDeveloperMode() {
	if ($('#developerMode').is(':checked')) {
		SetDeveloperMode(1);
	} else {
		SetDeveloperMode(0);
	}
}

</script>
<title>About FPP</title>
<style>
.clear {
  clear: both;
}
.items {
  width: 40%;
  background: rgb#FFF;
  float: right;
  margin: 0, auto;
}
.selectedEntry {
  background: #CCC;
}
.pl_title {
  font-size: larger;
}
h4, h3 {
  padding: 0;
  margin: 0;
}
.tblheader {
  background-color: #CCC;
  text-align: center;
}
tr.rowScheduleDetails {
  border: thin solid;
  border-color: #CCC;
}
tr.rowScheduleDetails td {
  padding: 1px 5px;
}
#tblSchedule {
  border: thin;
  border-color: #333;
  border-collapse: collapse;
}
a:active {
  color: none;
}
a:visited {
  color: blue;
}
.time {
  width: 100%;
}
.center {
  text-align: center;
}
</style>
</head>

<body>
<div id="bodyWrapper">
  <?php include 'menu.inc'; ?>
  <div style="margin:0 auto;"> <br />
    <fieldset style="padding: 10px; border: 2px solid #000;">
      <legend>About FPP</legend>
      <div style="overflow: hidden; padding: 10px;">
      <div>
        <div class='aboutLeft'>
          <table class='tblAbout'>
            <tr><td><b>Version Info</b></td><td>&nbsp;</td></tr>
            <tr><td>FPP Version:</td><td><? echo $fpp_version; ?></td></tr>
            <tr><td>FPP OS Build:</td><td><? echo $os_build; ?></td></tr>
            <tr><td>OS Version:</td><td><? echo $os_version; ?></td></tr>
            <tr><td>Kernel Version:</td><td><? echo $kernel_version; ?></td></tr>
<? if (file_exists($mediaDirectory."/.developer_mode")) { ?>
            <tr><td>Git Branch:</td><td><select id='gitBranch' onChange="ChangeGitBranch($('#gitBranch').val());">
<? PrintGitBranchOptions(); ?>
                </select></td></tr>
<?
   } else {
?>
            <tr><td>Git Branch:</td><td><? echo $git_branch; ?></td></tr>
<?
   }
?>
            <tr><td>Local Git Version:</td><td>
<?
  echo $git_version;
  if (($git_remote_version != "") && ($git_version != $git_remote_version))
    echo " <font color='#FF0000'>(Update is available)</font>";
	echo " <a href='changelog.php'>ChangeLog</a>";
?>
                </td></tr>
            <tr><td>Remote Git Version:</td><td>
<?
  echo $git_remote_version;
  if (($git_remote_version != "") && ($git_version != $git_remote_version))
    echo " <font color='#FF0000'><a href='javascript:void(0);' onClick='GetGitOriginLog();'>Preview Changes</a></font>";
?>
                </td></tr>
            <tr><td>Disable Auto Update:</td><td><input type='checkbox' id='autoUpdateDisabled' onChange='ToggleAutoUpdate();'
<? if (file_exists($mediaDirectory."/.auto_update_disabled")) { ?>
            checked
<? } ?>
              >  <input type='button' value='Manual Update' onClick='ManualGitUpdate();' class='buttons' id='ManualUpdate'></td></tr>
<!--
            <tr><td>Developer Mode:</td><td><input type='checkbox' id='developerMode' onChange='ToggleDeveloperMode();'
<? if (file_exists($mediaDirectory."/.developer_mode")) { ?>
            checked
<? } ?>
              ></td></tr>
-->
            <tr><td>&nbsp;</td><td>&nbsp;</td></tr>
          </table>
        </div>
        <div class='aboutCenter'>
        </div>
        <div class='aboutRight'>
          <table class='tblAbout'>
            <tr><td><b>Player Stats</b></td><td>&nbsp;</td></tr>
            <tr><td>Playlists:</td><td><a href='playlists.php' class='nonULLink'><? echo getFileCount($playlistDirectory); ?></a></td></tr>
            <tr><td>Sequences:</td><td><a href='uploadfile.php?tab=0' class='nonULLink'><? echo getFileCount($sequenceDirectory); ?></a></td></tr>
            <tr><td>Audio Files:</td><td><a href='uploadfile.php?tab=1' class='nonULLink'><? echo getFileCount($musicDirectory); ?></a></td></tr>
            <tr><td>Videos:</td><td><a href='uploadfile.php?tab=2' class='nonULLink'><? echo getFileCount($videoDirectory); ?></a></td></tr>
            <tr><td>Events:</td><td><a href='events.php' class='nonULLink'><? echo getFileCount($eventDirectory); ?></a></td></tr>
            <tr><td>Effects:</td><td><a href='uploadfile.php?tab=3' class='nonULLink'><? echo getFileCount($effectDirectory); ?></a></td></tr>
            <tr><td>Scripts:</td><td><a href='uploadfile.php?tab=4' class='nonULLink'><? echo getFileCount($scriptDirectory); ?></a></td></tr>
			<tr><td>Remapped Channels:</td><td><a href='channelremaps.php' class='nonULLink'><? echo getRemappedChannelCount(); ?></a></td></tr>

            <tr><td>&nbsp;</td><td>&nbsp;</td></tr>

            <tr><td><b>Disk Utilization</b></td><td>&nbsp;</td></tr>
            <tr><td>Root Free Space:</td><td>
<?
  $diskTotal = disk_total_space("/");
  $diskFree  = disk_free_space("/");
  printf( "%s (%2.0f%%)\n", getSymbolByQuantity($diskFree), $diskFree * 100 / $diskTotal);
?>
              </td></tr>
            <tr><td>Media Free Space:</td><td>
<?
  $diskTotal = disk_total_space($mediaDirectory);
  $diskFree  = disk_free_space($mediaDirectory);
  printf( "%s (%2.0f%%)\n", getSymbolByQuantity($diskFree), $diskFree * 100 / $diskTotal);
?>
              </td></tr>

            <tr><td></td><td></td></tr>
          </table>
        </div>
      </div>
    <div class="clear"></div>
    </fieldset>
    <div id='logViewer' title='Log Viewer' style="display: none">
      <pre>
        <div id='logText'>
        </div>
      </pre>
    </div>
  </div>
</div>
  <?php include 'common/footer.inc'; ?>
</body>
</html>
