<!DOCTYPE html>
<html>
<head>
<?php
require_once('config.php');
require_once('common.php');
include 'playlistEntryTypes.php';
include 'common/menuHead.inc';
?>
<script>
</script>

<title><? echo $pageTitle; ?></title>
</head>
<body>
<div id="bodyWrapper">
<?php
$activeParentMenuItem = 'help'; 
	include 'menu.inc'; ?>
  <div class="mainContainer">
    <h1 class="title">API Help</h1>
        <div class="pageContent">
<?
$apiDir = "api/";
include 'api/help.php';
?>
</div>
</div>

<?php	include 'common/footer.inc'; ?>
</div>
<div id='upgradePopup' title='FPP Upgrade' style="display: none">
    <textarea style='width: 99%; height: 97%;' disabled id='upgradeText'>
    </textarea>
</div>
</body>
</html>
