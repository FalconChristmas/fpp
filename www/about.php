<?php
$a = session_id();

if(empty($a))
{
	session_start();
}
$_SESSION['session_id'] = session_id();
//ini_set('display_errors', 'On');
error_reporting(E_ALL);


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

?>

<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<?php	include 'common/menuHead.inc'; ?>
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
  <?php	include 'menu.inc'; ?>
  <div style="width:800px;margin:0 auto;"> <br />
    <fieldset style="padding: 10px; border: 2px solid #000;">
      <legend>About FPP</legend>
      <div style="overflow: hidden; padding: 10px;">
      <div>
        <table id='tblAbout'>
          <tr><td>FPP Version:</td><td><? echo $fpp_version; ?></td></tr>
          <tr><td>OS Version:</td><td><? echo $rfs_version; ?></td></tr>
          <tr><td>Git Version:</td><td><? echo $git_version; ?></td></tr>
<? if ($git_branch != "master") { ?>
          <tr><td>Git Branch:</td><td><? echo $git_branch; ?></td></tr>
<? } ?>
          <tr><td>Git Master Version:</td><td><? echo $git_remote_version; ?></td></tr>
          <tr><td>&nbsp;</td><td>&nbsp;</td></tr>
<!--
Other Info/Stats:
- disk utilization (root & media)
- # of sequences available (.fseq and .eseq breakdown)
- # of audio files
-->
          <tr><td></td><td></td></tr>
          <tr><td></td><td></td></tr>
          <tr><td></td><td></td></tr>
          <tr><td></td><td></td></tr>
          <tr><td></td><td></td></tr>
        </table>
      </div>
    </fieldset>
  </div>
</div>
  <?php	include 'common/footer.inc'; ?>
</body>
</html>
