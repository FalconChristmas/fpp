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
<link rel="stylesheet" href="css/fpp.css" />
<title>Credits</title>
<style>
.pl_title {
  font-size: larger;
}
h4, h3 {
  padding: 0;
  margin: 0;
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
      <legend>Credits</legend>
      <div style="overflow: hidden; padding: 10px;">
    <div>
      <div id='credits'>
        <b>FPP Developed By:</b><br />
		<br />
        David Pitts (dpitts)<br />
        Tony Mace (MyKroFt)<br />
        Mathew Mrosko (Materdaddy)<br />
        Chris Pinkham (CaptainMurdoch)<br />
        Stephane Legargeant (ihbar)<br />
		<br />
        <hr width=300 />
		<br />
        <b>Video Tutorials by:</b><br />
		<br />
        Alan Dahl (bajadahl)<br />
		<br />
        <hr width=180 />
		<br />
        Copyright &copy; 2013-2014
      </div>
    </div>
    </fieldset>
  </div>
</div>
  <?php include 'common/footer.inc'; ?>
</body>
</html>
