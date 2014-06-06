<?php

$kernel_version = exec("uname -r");

$fpp_version = "v" . exec("git --git-dir=".dirname(dirname(__FILE__))."/.git/ describe --tags", $output, $return_val);
if ( $return_val != 0 ) 
	$fpp_version = "Unknown";
unset($output);

?>

<?php require_once(dirname(__FILE__)."/config.php"); ?>
  <div class="header">
    <div class="headerLeft"><a href="http://www.falconchristmas.com"><img src="images/falconlogo.png" class="logo" /></a></div>
    <div class="headerCenter"><div class="siteName">Falcon Pi Player (FPP)</div>
      Version: <a href='about.php' class='nonULLink'><?php echo $fpp_version; ?></a>
<?
	if (file_exists("/etc/fpp/rfs_version"))
	{
		$mounts = file("/proc/mounts");
		$mounted = 0;
		foreach ($mounts as $mount)
		{
			if (preg_match("/\/home\/pi\/media/", $mount))
			{
				$mounted = 1;
				break;
			}
		}

		if (!$mounted)
		{
?>
	<br><b><font color='#ff0000'>WARNING: /home/pi/media is not mounted, is a USB flash drive inserted?</font></b>
<?
		}
	}
?>
    </div>
    <div class="headerRight"><a href="http://raspberrypi.org"><img src="images/large_Raspberry_Pi_Logo_4.png" class="logo" /></a></div>
    <div class="clear"></div>
  </div>

    <div id="fppMenu" class="ddsmoothmenu">
    <ul>
      <li><a href="index.php">Status/Control</a>
        <ul>
          <li><a href="/">Status Page</a></li>
          <div class="desktopItem">
            <li><a href="uipassword.php">Set UI Password</a></li>
            <li><a href="timeconfig.php">Config/Set Time</a></li>
            <li><a href="networkconfig.php">Network</a></li>
<?php
$handle = opendir($pluginDirectory);
if ( $handle )
{
	while (($file = readdir($handle)) !== false)
	{
		if (!in_array($file, array('.', '..')))
		{
			if ( file_exists($pluginDirectory."/".$file."/status_menu.inc") )
			{
				-include_once($pluginDirectory."/".$file."/status_menu.inc");
			}
		}
	}
}
?>
            <li><a href="settings.php">FPP Settings</a></li>
          </div>
          <li><a href="events.php">Events</a></li>
          <li><a href="effects.php">Effects</a></li>
        </ul>
      </li>
      <li><a href="#">Content Setup</a>
        <ul>
          <div class="desktopItem">
            <li><a href="uploadfile.php">File Manager</a></li>
          </div>
          <li><a href="playlists.php">Playlists</a></li>
          <li><a href="scheduler.php">Scheduler</a></li>
          <!-- <li><a href="video.php">Videos</a></li> -->
		  <li><a href="plugins.php">Plugins</a></li>
<?php
$handle = opendir($pluginDirectory);
if ( $handle )
{
	while (($file = readdir($handle)) !== false)
	{
		if (!in_array($file, array('.', '..')))
		{
			if ( file_exists($pluginDirectory."/".$file."/content_menu.inc") )
			{
				-include_once($pluginDirectory."/".$file."/content_menu.inc");
			}
		}
	}
}
?>
        </ul>
      </li>
      <li id='outputMenu'><a href="#">Output Setup</a>
        <ul>
          <li><a href="channeloutputs.php">Channel Outputs</a></li>
          <li><a href="channelremaps.php">Remap Channels</a></li>
          <li><a href="channelmemorymaps.php">Memory Mapped Channel Blocks</a></li>
<?php
$handle = opendir($pluginDirectory);
if ( $handle )
{
	while (($file = readdir($handle)) !== false)
	{
		if (!in_array($file, array('.', '..')))
		{
			if ( file_exists($pluginDirectory."/".$file."/output_menu.inc") )
			{
				-include_once($pluginDirectory."/".$file."/output_menu.inc");
			}
		}
	}
}
?>
        </ul>
      </li>
      <li><a href="#">Help</a>
        <ul>
          <li><a href="http://falconchristmas.com/wiki/index.php/Falcon_FPP" target="_blank">Wiki/Help</a></li>
<!--
          <li><a href="docs.php">Docs</a></li>
-->
          <li><a href="about.php">About</a></li>
          <li><a href="credits.php">Credits</a></li>
<?php
$handle = opendir($pluginDirectory);
if ( $handle )
{
	while (($file = readdir($handle)) !== false)
	{
		if (!in_array($file, array('.', '..')))
		{
			if ( file_exists($pluginDirectory."/".$file."/help_menu.inc") )
			{
				-include_once($pluginDirectory."/".$file."/help_menu.inc");
			}
		}
	}
}
?>
        </ul>
      </li>
    </ul>
    <br style="clear: left" />
    </div>
