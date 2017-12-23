<?php
require_once('config.php');
require_once('common.php');

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

$cmd = "cd " . dirname(dirname(__FILE__)) . " && /usr/bin/git status";
$git_status = "Unknown";
exec($cmd, $output, $return_val);
if ( $return_val == 0 )
	$git_status = implode("\n", $output) . "\n";
unset($output);

$kernel_version = exec("/bin/uname -r", $output, $return_val);
if ( $return_val != 0 )
	$kernel_version = "Unknown";
unset($output);

$git_version = exec("git --git-dir=".dirname(dirname(__FILE__))."/.git/ rev-parse --short=7 HEAD", $output, $return_val);
if ( $return_val != 0 )
  $git_version = "Unknown";
unset($output);

$git_branch = exec("git --git-dir=".dirname(dirname(__FILE__))."/.git/ branch --list | grep '\\*' | awk '{print \$2}'", $output, $return_val);
if ( $return_val != 0 )
  $git_branch = "Unknown";
unset($output);

$git_remote_version = "Unknown";
$git_remote_version = exec("git --git-dir=".dirname(dirname(__FILE__))."/.git/ ls-remote --heads | grep 'refs/heads/$git_branch\$' | awk '$1 > 0 { print substr($1,1,7)}'", $output, $return_val);
if ( $return_val != 0 )
  $git_remote_version = "Unknown";
unset($output);

function PrintGitBranchOptions()
{
	global $git_branch;

  $branches = Array();
  exec("git --git-dir=".dirname(dirname(__FILE__))."/.git/ branch -a | grep -v -- '->' | sed -e 's/remotes\/origin\///' -e 's/\\* *//' -e 's/ *//' | sort -u", $branches);
  foreach($branches as $branch)
  {
    if ($branch == $git_branch)
    {
//       $branch = preg_replace('/^\\* */', '', $branch);
       echo "<option value='$branch' selected>$branch</option>";
    }
    else
    {
 //      $branch = preg_replace('/^ */', '', $branch);
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

function GitReset() {
	$.get("fppxml.php?command=resetGit"
		).success(function() {
			$('#gitStatusPre').load('fppxml.php?command=gitStatus');
		});
}

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
table.tblAbout td {
  vertical-align: text-top;
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
      <legend>Developer Info</legend>
      <div style="overflow: hidden; padding: 10px;">
      <div>
        <div>
          <table class='tblAbout'>
            <tr><td>Git Branch:</td><td><select id='gitBranch' onChange="ChangeGitBranch($('#gitBranch').val());">
<? PrintGitBranchOptions(); ?>
                </select>
				<br><b>Note: Changing branches may take a couple minutes to recompile<br>and may not work if you have any modified source files.</b>
				<br><font color='red'><b>WARNING: Switching branches will run a "git clean -df" which will remove any untracked files.  If you are doing development, you may want to backup the source directory before switching branches using the this page.</b></font>
				</td></tr>
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
    echo " <font color='#FF0000'><a href='javascript:void(0);' onClick='GetGitOriginLog();'>Preview Change Log</a></font>";
?>
                </td></tr>
            <tr><td>Git:</td><td><input type='button' value='Manual Update' onClick='ManualGitUpdate();' id='ManualUpdate'></td></tr>
            <tr><td>&nbsp;</td><td>&nbsp;</td></tr>
          </table>
        </div>
      </div>
    <div class="clear"></div>
      <div>
        <b>Git Status:</b><br>
        <pre id='gitStatusPre'><? echo $git_status; ?></pre>
      </div>
      <div>
        <b>Debug Actions:</b><br>
        <ul>
        <li><b>UI Warnings:</b><br>
        Disable restart/reboot UI Warnings: <? PrintSettingCheckbox("Disable restart/reboot UI Warnings", "disableUIWarnings", 0, 0, "1", "0"); ?><br>
        <input type='button' value='Clear Restart Warning' onClick='ClearRestartFlag();'>
        <input type='button' value='Clear Reboot Warning' onClick='ClearRebootFlag();'>
        </li><br>
		<br>
		<li><b>Git:</b><br>
			<input type='button' value='Reset Local Changes' onClick='GitReset();'> <b>WARNING:</b> This performs a "git reset --hard HEAD" to revert all local source code changes
			</li>
			</ul>
      </div>
    </fieldset>
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
