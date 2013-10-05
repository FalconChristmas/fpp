<?php
$a = session_id();

if(empty($a))
{
  session_start();
}
$_SESSION['session_id'] = session_id();
//ini_set('display_errors', 'On');
error_reporting(E_ALL);

$os_version = "Unknown";
if (file_exists("/etc/os-release"))
{
	$os_version = exec("grep PRETTY_NAME /etc/os-release | cut -f2 -d'\"'", $output, $return_val);
	if ( $return_val != 0 )
		$os_version = "Unknown";
}

$git_version = exec("git --git-dir=".dirname(dirname(__FILE__))."/.git/ rev-parse --short HEAD", $output, $return_val);
//TODO: Make this awesomer
if ( $return_val != 0 )
  $git_version = "Unknown";
unset($output);

$git_branch = exec("git --git-dir=".dirname(dirname(__FILE__))."/.git/ branch --list | grep '\\*' | awk '{print \$2}'", $output, $return_val);
//TODO: Make this awesomer
if ( $return_val != 0 )
  $git_branch = "Unknown";
unset($output);

$git_remote_version = "blah";
$git_remote_version = exec("git ls-remote --heads https://github.com/FalconChristmas/fpp | grep master | awk '$1 > 0 { print substr($1,1,7)}'", $output, $return_val);
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

?>

<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<?php include 'common/menuHead.inc'; ?>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
<link rel="stylesheet" href="css/jquery-ui.css" />
<script type="text/javascript" src="js/fpp.js"></script>
<script src="js/jquery-migrate-1.1.1.min.js"></script>
<script src="js/jquery-1.9.1.js"></script>
<script src="js/jquery-ui.js"></script>
<link rel="stylesheet" type="text/css" href="css/jquery.timeentry.css">
<script type="text/javascript" src="js/jquery.timeentry.min.js"></script>
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
            <tr><td>OS Version:</td><td><? echo $os_version; ?></td></tr>
<? if ($git_branch != "master") { ?>
            <tr><td>Git Branch:</td><td><? echo $git_branch; ?></td></tr>
<? } ?>
            <tr><td>Git Version:</td><td><? echo $git_version; ?></td></tr>
            <tr><td>Git Master Version:</td><td><? echo $git_remote_version; ?></td></tr>
            <tr><td>&nbsp;</td><td>&nbsp;</td></tr>
          </table>
        </div>
        <div class='aboutCenter'>
        </div>
        <div class='aboutRight'>
          <table class='tblAbout'>
            <tr><td><b>Player Stats</b></td><td>&nbsp;</td></tr>
            <tr><td>Playlists:</td><td><? echo getFileCount($playlistDirectory); ?></td></tr>
            <tr><td>Sequence Files:</td><td><? echo getFileCount($sequenceDirectory); ?></td></tr>
            <tr><td>Audio Files:</td><td><? echo getFileCount($musicDirectory); ?></td></tr>
            <tr><td>Events Defined:</td><td><? echo getFileCount($eventDirectory); ?></td></tr>
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
  $diskTotal = disk_total_space("/home/pi/media");
  $diskFree  = disk_free_space("/home/pi/media");
  printf( "%s (%2.0f%%)\n", getSymbolByQuantity($diskFree), $diskFree * 100 / $diskTotal);
?>
              </td></tr>

            <tr><td></td><td></td></tr>
          </table>
        </div>
      </div>
    <div class="clear"></div>
    <hr>
    <div>
      <div>
        <table width="100%">
          <tr><td><b><center>Credits</center></b></td></tr>
        </table>
      </div>
      <div id='credits'>
        FPP Designed By:<br />
        <br />
        David Pitts (dpitts)<br />
        Tony Mace (MyKroFt)<br />
        Mathew Mrosko (Materdaddy)<br />
        Chris Pinkham (CaptainMurdoch)<br />
        <br />
        Copyright 2013
      </div>
    </div>
    </fieldset>
  </div>
</div>
  <?php include 'common/footer.inc'; ?>
</body>
</html>
