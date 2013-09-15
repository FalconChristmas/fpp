<?php
$rfs_version = exec("cat /etc/fpp/rfs_version", $output, $return_val);
//TODO: Make this fancier
if ( $return_val != 0 )
$rfs_version = "Unknown";
unset($output);

$git_version = exec("git --git-dir=".dirname(dirname(__FILE__))."/.git/ rev-parse --short HEAD", $output, $return_val);
//TODO: Make this awesomer
if ( $return_val != 0 )
$git_version = "Unknown";
unset($output);

$git_remote_version = exec ("git ls-remote --heads https://github.com/FalconChristmas/fpp | grep master | awk '$1 > 0 { print substr($1,1,7)}'", $output, $return)val);
if ( $return_val != 0 )
$git_remote_version = "Unknown";
unset($output);
?>

	 <div class="header">
	 		<div class="headerLeft"><a href="http://www.falconchristmas.com"><img src="images/falconlogo.png"  height="60" /></a></div>
	 		<div class="headerCenter"><div class="siteName">Falcon Pi Player (FPP)</div>
		        OS Version: <?php echo $rfs_version; ?><br />
		        FPP Version: <?php echo $git_version; ?>, Master Version: <?php echo $git_remote_version; ?>
	 		</div>
			<div class="headerRight"<a href="http://raspberrypi.org"><img src="images/large_Raspberry_Pi_Logo_4.png" height="60" /></a></div>
			<div class="clear"></div>
	 </div>

    <div id="fppMenu" class="ddsmoothmenu">
    <ul>
      <li><a href="index.php">Status/Control</a></li>
      <!-- <li><a href="#">Basic Setup</a>
        <ul>
          <li><a href="uipassword.php">Set UI Password</a></li>
          <li><a href="timeconfig.php">Config/Set Time</a></li>
          <li><a href="networkconfig.php">Network</a></li>
          <li><a href="playerkconfig.php">Player Mode</a></li>
        </ul> -->
      </li>
      <li><a href="#">Content Setup</a>
        <ul>
          <li><a href="uploadfile.php">Source Files</a></li>
          <li><a href="playlists.php">Playlists</a></li>
          <li><a href="scheduler.php">Scheduler</a></li>
          <!-- <li><a href="video.php">Videos</a></li> -->
        </ul>
      </li>
      <li><a href="#">Output Setup</a>
        <ul>
          <li><a href="universes.php">Universe</a></li>
          <li><a href="pixelnetdmx.php">Pixelnet/DMX</a></li>
          <!-- <li><a href="rds.php">Radio RDS</a></li> -->
        </ul>
      </li>
      <li><a href="http://falconchristmas.com/wiki/index.php5/FPP">Wiki/Help</a></li>
    </ul>
    <br style="clear: left" />
    </div>