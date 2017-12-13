<?php
require_once('config.php');

//ini_set('display_errors', 'On');
error_reporting(E_ALL);

$fpp_version = "v" . exec("git --git-dir=".dirname(dirname(__FILE__))."/.git/ describe --tags", $output, $return_val);
if ( $return_val != 0 )
	$fpp_version = "Unknown";
unset($output);

$git_log = "";
$cmd = "git --git-dir=".dirname(dirname(__FILE__))."/.git/ log --pretty=format:'%h - %an, %ar : %s' | cut -c1-100";
exec($cmd, $output, $return_val);
if ( $return_val == 0 )
  $git_log = implode('<br>', $output);
unset($output);

?>

<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<?php include 'common/menuHead.inc'; ?>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
<title>FPP - ChangeLog</title>
</head>

<body>
<div id="bodyWrapper">
  <?php include 'menu.inc'; ?>
  <div style="margin:0 auto;"> <br />
    <fieldset style="padding: 10px; border: 2px solid #000;">
      <legend>ChangeLog</legend>
      <pre><? echo $git_log; ?></pre>
    </fieldset>
  </div>
  <?php include 'common/footer.inc'; ?>
</div>
</body>
</html>
