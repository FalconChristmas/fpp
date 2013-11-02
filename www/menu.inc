<?php

$vFile = "/etc/fpp/rfs_version";
$fpp_version = "Unknown";
if (file_exists($vFile))
{
  $fpp_version = exec("cat $vFile", $output, $return_val);
  if ( $return_val != 0 )
    $fpp_version = "Unknown";
}

?>

<?php require_once(dirname(__FILE__)."/config.php"); ?>
  <div class="header">
    <div class="headerLeft"><a href="http://www.falconchristmas.com"><img src="images/falconlogo.png" class="logo" /></a></div>
    <div class="headerCenter"><div class="siteName">Falcon Pi Player (FPP)</div>
      Version: <a href='about.php' style="text-decoration: none; color: blue;"><?php echo $fpp_version; ?></a>
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
<?php if ( $rds_enabled == true ) { ?>
            <li><a href="rds.php">RDS</a></li>
<?php } ?>
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
        </ul>
      </li>
      <li id='outputMenu'><a href="#">Output Setup</a>
        <ul>
          <li><a href="universes.php">E1.31 Universes</a></li>
          <li><a href="pixelnetdmx.php">FPD - Pixelnet/DMX</a></li>
          <!-- <li><a href="rds.php">Radio RDS</a></li> -->
        </ul>
      </li>
      <li><a href="#">Help</a>
        <ul>
          <li><a href="http://falconchristmas.com/wiki/index.php/Falcon_FPP">Wiki/Help</a></li>
          <li><a href="about.php">About</a></li>
        </ul>
      </li>
    </ul>
    <br style="clear: left" />
    </div>
