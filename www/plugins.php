<!DOCTYPE html>
<html>
<head>
<?php include 'common/menuHead.inc'; ?>
<script type="text/javascript" src="/js/fpp.js"></script>
<title>Falcon PI Player - FPP</title>
</head>
<body>
<div id="bodyWrapper">
  <?php include 'menu.inc'; ?>
  <br/>

<?php

require_once("pluginData.inc.php");

?>

<div id=plugins" class="settings">
<fieldset>
<legend>Plugins</legend>


<table>

<?php foreach ($plugins as $plugin) { ?>
<tr>
  <td><span class="pluginTitle"><?php echo $plugin['name']; ?></span></td>
  <td align="right">
<?php

if (file_exists( $pluginDirectory."/".$plugin['shortName'] ))
{
	//Installed!

	//TODO: Check for updates somehow
	if ( false ) // ( $version != $version )
	{
		echo "<img src=\"images/update.png\" class=\"button\" title=\"Update Plugin\" onClick='updatePlugin(\"".$plugin['shortName']."\");'>";
	}
	else
	{
		echo "<img src=\"images/uninstall.png\" class=\"button\" title=\"Uninstall Plugin\" onClick='uninstallPlugin(\"".$plugin['shortName']."\");'>";
	}
}
else
{
	//Not installed
	echo "<img src=\"images/install.png\" class=\"button\" title=\"Install Plugin\" onClick='installPlugin(\"".$plugin['shortName']."\");'>";
}
?>
  </td>
<?php if ( $plugin['homeUrl'] ): ?>
<td><a href="<?php echo $plugin['homeUrl']; ?>" target="_blank"><img src="images/home.png" title="Home Page" class="button"></a></td>
<?php endif; if ( $plugin['sourceUrl'] ): ?>
<td><a href="<?php echo $plugin['sourceUrl']; ?>" target="_blank"><img src="images/source.png" title="Source" class="button"></a></td>
<?php endif; if ( $plugin['bugUrl'] ): ?>
<td><a href="<?php echo $plugin['bugUrl']; ?>" target="_blank"><img src="images/bug.png" title="Report Bug" class="button"></a></td>
<?php endif; ?>
</tr>
<tr>
<td colspan=2>
<?php echo $plugin['description']; ?>
</td>
</tr>

<?php } ?>
</table>

</fieldset>
</div>

</div>
<?php	include 'common/footer.inc'; ?>
</body>
</html>
