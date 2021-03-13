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
  <?php 
  $activeParentMenuItem = 'help'; 
  include 'menu.inc'; ?><div class="container">
  <div class="title">Credits</div>
    <div class="pageContent">
      
      <div style="margin:0 auto;"> 

          <div>
        <div>

<div id='credits'>
    <h2>FPP Developers:</h2>
    <div class="backdrop">
        Chris Pinkham (CaptainMurdoch)<br/>
        Dan Kulp (dkulp)<br/>
        Greg Hormann (ghormann)<br/>
        Andrew Ayton (drew8n) - UI<br/>
        Adam Coulombe (wzadz1) - UI<br/>
        Justin J. Novack (jnovack) - UI<br/>
        Pat Delaney (patdelaney) - QA<br/>
        Rick Harris (Poporacer) - Manual/QA<br/>
        Mark Amber (pixelpuppy) - Manual/QA<br/>
        <br/>
        For a full list of code contributors, visit the <a href='https://github.com/FalconChristmas/fpp/graphs/contributors' target='_blank'>contributors page</a> on github.<br/>
    </div>
    <br>

    <h2>FPP Created by:</h2>
    <div class="backdrop">
        David Pitts <br/>
        Tony Mace (MyKroFt)<br/>
        Mathew Mrosko (Materdaddy)<br/>
        Chris Pinkham (CaptainMurdoch)<br/>
    </div>
    <br>

    <h2>3rd Party Libraries used by FPP for some Channel Outputs:</h2>
    <div class="backdrop">
        <a href='https://github.com/jgarff/rpi_ws281x'>rpi_ws281x</a> by Jeremy Garff<br/>
        <a href='https://github.com/hzeller/rpi-rgb-led-matrix'>rpi-rgb-led-matrix</a> by Henner Zeller<br/>
        <a href='https://github.com/hzeller/spixels'>spixels</a> by Henner Zeller<br/>
        <a href='https://github.com/TMRh20/RF24'>RF24</a> (nRF24L01)<br/>
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
