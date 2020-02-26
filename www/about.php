<!DOCTYPE html>
<html>
<?php
require_once('common.php');
require_once('config.php');

//ini_set('display_errors', 'On');
error_reporting(E_ALL);

$fpp_version = "v" . exec("git --git-dir=".dirname(dirname(__FILE__))."/.git/ describe --tags", $output, $return_val);
if ( $return_val != 0 )
	$fpp_version = "Unknown";
unset($output);

$serialNumber = exec("sed -n 's/^Serial.*: //p' /proc/cpuinfo", $output, $return_val);
if ( $return_val != 0 )
    unset($serialNumber);
unset($output);

    
if (!file_exists("/etc/fpp/config_version") && file_exists("/etc/fpp/rfs_version"))
{
	exec($SUDO . " $fppDir/scripts/upgrade_config");
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

//Get local git version
$git_version = get_local_git_version();

//Get git branch
$git_branch = get_git_branch();

//Remote Git branch version
$git_remote_version = get_remote_git_version($git_branch);

//System uptime
$uptime = get_server_uptime();

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
    
function getFileList($dir, $ext)
{
  $i = array();
  if ($handle = opendir($dir)) {
      while (($file = readdir($handle)) !== false) {
          if (!in_array($file, array('.', '..')) && !is_dir($dir . $file) && strtolower(substr($file, strrpos($file, '.') + 1)) == $ext) {
              array_push($i, $file);
          }
      }
  }
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

function UpgradeOS() {
    var os = $('#OSSelect').val();
    if (confirm('Upgrade the OS using ' + os + '?\nThis can take a long time.')) {
        location.href="upgradeOS.php?os=" + os;
    }
}

</script>
<title><? echo $pageTitle; ?></title>
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
            <tr><td>Platform:</td><td><?
echo $settings['Platform'];
if (($settings['Variant'] != '') && ($settings['Variant'] != $settings['Platform']))
    echo " (" . $settings['Variant'] . ")";
?></td></tr>
            <tr><td>FPP OS Build:</td><td><? echo $os_build; ?></td></tr>
            <tr><td>OS Version:</td><td><? echo $os_version; ?></td></tr>
<? if (isset($serialNumber) && $serialNumber != "") { ?>
        <tr><td>Hardware Serial Number:</td><td><? echo $serialNumber; ?></td></tr>
<? } ?>
            <tr><td>Kernel Version:</td><td><? echo $kernel_version; ?></td></tr>
            <tr><td>Local Git Version:</td><td>
<?
  echo $git_version;
  if (($git_remote_version != "") &&
      ($git_remote_version != "Unknown") &&
      ($git_version != $git_remote_version))
    echo " <font color='#FF0000'>(Update is available)</font>";
	echo " <a href='changelog.php'>ChangeLog</a>";
?>
                </td></tr>
            <tr><td>Remote Git Version:</td><td>
<?
  echo $git_remote_version;
  if (($git_remote_version != "") &&
			($git_remote_version != "Unknown") &&
			($git_version != $git_remote_version))
    echo " <font color='#FF0000'><a href='javascript:void(0);' onClick='GetGitOriginLog();'>Preview Changes</a></font>";
?>
                </td></tr>
            <tr><td>Update FPP:</td><td><input type='button' value='Update FPP' onClick='location.href="manualUpdate.php";' class='buttons' id='ManualUpdate'></td></tr>
<?
            $osUpdateFiles = getFileList($uploadDirectory, "fppos");
            if (count($osUpdateFiles) > 0) {
                echo "<tr><td>Upgrade OS:</td><td><select class='OSSelect' id='OSSelect'>\n";
                foreach ($osUpdateFiles as $key => $value) {
                    echo "<option value='" . $value . "'>" . $value . "</option>\n";
                }
                echo "</select>&nbsp;<input type='button' value='Upgrade OS' onClick='UpgradeOS();' class='buttons' id='OSUpgrade'></td></tr>";
            }
?>
            <tr><td>&nbsp;</td><td>&nbsp;</td></tr>
            <tr><td><b>System Utilization</b></td><td>&nbsp;</td></tr>
            <tr><td>CPU Usage:</td><td><? printf( "%.2f", get_server_cpu_usage()); ?>%</td></tr>
            <tr><td>Memory Usage:</td><td><? printf( "%.2f", get_server_memory_usage()); ?>%</td></tr>

            <tr><td>&nbsp;</td><td>&nbsp;</td></tr>
            <tr><td><b>Uptime</b></td><td>&nbsp;</td></tr>
            <tr><td colspan='2'><? echo $uptime; ?></td></tr>

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
            <tr><td>Effects:</td><td><a href='uploadfile.php?tab=4' class='nonULLink'><? echo getFileCount($effectDirectory); ?></a></td></tr>
            <tr><td>Scripts:</td><td><a href='uploadfile.php?tab=5' class='nonULLink'><? echo getFileCount($scriptDirectory); ?></a></td></tr>

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
    <div class="clear"></div>
    </fieldset>
    <?
    if (isSet($settings["cape-info"]))  {
        $currentCapeInfo = $settings["cape-info"];
    ?>
        <br>
        <fieldset style="padding: 10px; border: 2px solid #000;">
        <legend>About Cape/Hat</legend>
        <div style="overflow: hidden; padding: 10px;">
        <div>
        <div class='<? if (isSet($currentCapeInfo['vendor'])) { echo "aboutLeft"; } else { echo "aboutAll";} ?> '>
        <table class='tblAbout'>
        <tr><td><b>Name:</b></td><td width="100%"><? echo $currentCapeInfo['name']  ?></td></tr>
        <?
        if (isSet($currentCapeInfo['version'])) {
            echo "<tr><td><b>Version:</b></td><td>" . $currentCapeInfo['version'] . "</td></tr>";
        }
        if (isSet($currentCapeInfo['serialNumber'])) {
            echo "<tr><td><b>Serial&nbsp;Number:</b></td><td>" . $currentCapeInfo['serialNumber'] . "</td></tr>";
        }
        if (isSet($currentCapeInfo['designer'])) {
            echo "<tr><td><b>Designer:</b></td><td>" . $currentCapeInfo['designer'] . "</td></tr>";
        }
        if (isSet($currentCapeInfo['description'])) {
            echo "<tr><td colspan=\"2\">";
            if (isSet($currentCapeInfo['vendor']) || $currentCapeInfo['name'] == "Unknown") {
                echo $currentCapeInfo['description'];
            } else {
                echo htmlspecialchars($currentCapeInfo['description']);
            }
            echo "</td></tr>";
        }
        ?>
        </table>
        </div>
        <? if (isSet($currentCapeInfo['vendor'])) { ?>
               <div class='aboutRight'>
               <table class='tblAbout'>
                    <tr><td><b>Vendor&nbsp;Name:</b></td><td><? echo $currentCapeInfo['vendor']['name']  ?></td></tr>
            <? if (isSet($currentCapeInfo['vendor']['url'])) {
                $url = $currentCapeInfo['vendor']['url'];
                $landing = $url;
                if (isSet($currentCapeInfo['vendor']['landingPage'])) {
                    $landing = $currentCapeInfo['vendor']['landingPage'];
                }
                $landing = $landing  . "?sn=" . $currentCapeInfo['serialNumber'] . "&id=" . $currentCapeInfo['id'];
                if (isset($currentCapeInfo['cs']) && $currentCapeInfo['cs'] != "") {
                    $landing = $landing . "&cs=" . $currentCapeInfo['cs'];
                }
                echo "<tr><td><b>Vendor&nbsp;URL:</b></td><td><a href=\"" . $landing . "\">" . $url . "</a></td></tr>";
            }
            if (isSet($currentCapeInfo['vendor']['phone'])) {
                 echo "<tr><td><b>Phone&nbsp;Number:</b></td><td>" . $currentCapeInfo['vendor']['phone'] . "</td></tr>";
            }
            if (isSet($currentCapeInfo['vendor']['email'])) {
                echo "<tr><td><b>E-mail:</b></td><td><a href=\"mailto:" . $currentCapeInfo['vendor']['email'] . "\">" . $currentCapeInfo['vendor']['email'] . "</td></tr>";
            }
            if (isSet($currentCapeInfo['vendor']['image'])) {
                $iurl = $currentCapeInfo['vendor']['image'] . "?sn=" . $currentCapeInfo['serialNumber'] . "&id=" . $currentCapeInfo['id'];
                if (isset($currentCapeInfo['cs']) && $currentCapeInfo['cs'] != "") {
                    $iurl = $iurl . "&cs=" . $currentCapeInfo['cs'];
                }
                echo "<tr><td colspan=\"2\"><a href=\"" . $landing . "\"><img style='max-height: 90px; max-width: 300px;' src=\"" . $iurl . "\" /></a></td></tr>";
            }?>
               </table>
               </div>
            <? } ?>
        </div>
        </div>
        </fieldset>
    <?
    }
    ?>
    <div id='logViewer' title='Log Viewer' style="display: none">
      <pre>
        <div id='logText'>
        </div>
      </pre>
    </div>
  </div>
  <?php include 'common/footer.inc'; ?>
</div>
</body>
</html>
