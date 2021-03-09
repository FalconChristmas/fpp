<!DOCTYPE html>
<html>
<?php
require_once('config.php');
require_once('common.php');

$helpPages = array(
	'help/pixeloverlaymodels.php' => 'Real-Time Pixel Overlay Models'
	);
?>

<head>
<?php include 'common/menuHead.inc'; ?>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
<title><? echo $pageTitle; ?></title>
</head>

<body>
<div id="bodyWrapper">
  <?php 
  $activeParentMenuItem = 'help'; 
  include 'menu.inc'; ?>
  <div class="mainContainer">
	  <h1 class="title">Help Index</h1>
	  <div class="pageContent">
	  	  
	  	    <div style="margin:0 auto;"> <br />
	  	      <fieldset class="fieldSection">
	  	        <legend>Help Index</legend>
	  	        <div style="overflow: hidden; padding: 10px;">
	  	        <div>
	  	  				<ul>
	  	  					<li><a href='javascript:void(0);' onClick="helpPage='help/backup.php'; DisplayHelp();">Backup & Restore</a></li>
	  	  					<li><a href='javascript:void(0);' onClick="helpPage='help/channeloutputs.php'; DisplayHelp();">Channel Outputs</a></li>
	  	  					<li><a href='javascript:void(0);' onClick="helpPage='help/gpio.php'; DisplayHelp();">GPIO Input Triggers</a></li>
	  	  					<li><a href='javascript:void(0);' onClick="helpPage='help/networkconfig.php'; DisplayHelp();">Network Config</a></li>
	  	  					<li><a href='javascript:void(0);' onClick="helpPage='help/outputprocessors.php'; DisplayHelp();">Output Processors</a></li>
	  	  					<li><a href='javascript:void(0);' onClick="helpPage='help/scheduler.php'; DisplayHelp();">Scheduler</a></li>
	  	  					<li><a href='javascript:void(0);' onClick="helpPage='help/scriptbrowser.php'; DisplayHelp();">Script Repository Browser</a></li>
	  	  				</ul>
	  	        </div>
	  	      </fieldset>
	  	    </div>
	    </div>
  </div>
  <?php include 'common/footer.inc'; ?>
</div>
</body>
</html>
