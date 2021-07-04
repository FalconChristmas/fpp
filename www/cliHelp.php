<!DOCTYPE html>
<html>
<?php
require_once('common.php');
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
	  <div class="title">CLI Commands</div>
	  <div class="pageContent">
	  	  
	  	    <div style="margin:0 auto;"> <br />
	  	      <fieldset class="fieldSection">
	  	        <div style="overflow: hidden; padding: 10px;">
	  	      <div class="clear"></div>
	  	  		<h3>fpp -h</h3>
	  	  		<pre>
	  	  <?
	  	  	system($settings['fppBinDir'] . "/fpp -h");
	  	  ?>
	  	  		</pre>
	  	  
	  	  		<hr>
	  	  		<h3>fppmm -h</h3>
	  	  		<pre>
	  	  <?
	  	  	system($settings['fppBinDir'] . "/fppmm -h");
	  	  ?>
	  	  		</pre>
	  	  
	  	      </fieldset>
	  	    </div>
	    </div>
  </div>
  <?php include 'common/footer.inc'; ?>
</div>
</body>
</html>
