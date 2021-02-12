<!DOCTYPE html>
<html>
<?php
require_once('config.php');

error_reporting(E_ALL);
$fpp_version = "v" . getFPPVersion();

?>

<head>
<?php include 'common/menuHead.inc'; ?>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
<title>Credits</title>

</head>

<body>
<div id="bodyWrapper">
  <?php include 'menu.inc'; ?><div class="container">
  <div class="title">Credits</div>
    <div class="pageContent">
      
      <div style="margin:0 auto;"> 

          <div>
        <div>
          <div id='credits'>
            <h2>FPP Developed by:</h2>
            <div class="backdrop">
            David Pitts<br />
            Tony Mace (MyKroFt)<br />
            Mathew Mrosko (Materdaddy)<br />
            Chris Pinkham (CaptainMurdoch)<br />
            Dan Kulp (dkulp)<br />
            Stephane Legargeant (ihbar)

            </div>
<br>
            <h2>Video Tutorials by:</h2>
            <div class="backdrop">
            Alan Dahl (bajadahl)
            </div>
            <br>

            <h2>3rd Party Libraries used by FPP for some Channel Outputs:</h2>
    		<div class="backdrop">
          <a href='https://github.com/jgarff/rpi_ws281x'>rpi_ws281x</a> by Jeremy Garff.  Used for driving WS281x pixels directly off the Pi's GPIO header.<br />
          <a href='https://github.com/hzeller/rpi-rgb-led-matrix'>rpi-rgb-led-matrix</a> by Henner Zeller.  Used for driving HUB75 panels directly off the Pi's GPIO header.<br />
          <a href='https://github.com/TMRh20/RF24'>RF24</a>. Used for driving nRF24L01 output for Komby.
        </div>
        <br>
            <small>Copyright &copy; 2013-2021</small>
          </div>
        </div>

      </div>
    </div>
  </div>
  <?php include 'common/footer.inc'; ?>
</div>
</body>
</html>
