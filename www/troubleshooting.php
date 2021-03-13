<!DOCTYPE html>
<html>
<?php
require_once('troubleshootingCommands.php');
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

    <h1 class="title">Troubleshooting</h1>
    <div class="pageContent">
      <h2>Troubleshooting Commands</h2>
        <div style="overflow: hidden; padding: 10px;">
      <div class="clear"></div>
  <?
  foreach ($commands as $title => $command)
  {
  ?>
          <h3><? echo $title . ':&nbsp;&nbsp;&nbsp;&nbsp;' . $command; ?></h3><pre><? echo $results[$command]; ?></pre><hr>
  <?
  }
  ?>

    </div>
    
  </div>
  <?php include 'common/footer.inc'; ?>
</div>
</body>
</html>
