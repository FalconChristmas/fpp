<!DOCTYPE html>
<html>
<?php
require_once('config.php');

//ini_set('display_errors', 'On');
error_reporting(E_ALL);

$fpp_version = "v" . getFPPVersion();

$git_log = "";
$cmd = "git --git-dir=".dirname(dirname(__FILE__))."/.git/ log --pretty=format:'%h - %an, %ar : %s' | cut -c1-100";
exec($cmd, $output, $return_val);
if ( $return_val == 0 )
  $git_log = implode('<br>', $output);
unset($output);

?>

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
