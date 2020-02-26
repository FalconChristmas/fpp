<?php

$kernel_version = exec("uname -r");

$fpp_head_version = "v" . exec("git --git-dir=".dirname(dirname(__FILE__))."/.git/ describe --tags", $output, $return_val);
if ( $return_val != 0 ) 
	$fpp_head_version = "Unknown";
unset($output);

$git_branch = exec("git --git-dir=".dirname(dirname(__FILE__))."/.git/ branch --list | grep '\\*' | awk '{print \$2}'", $output, $return_val);
if ( $return_val != 0 )
	$git_branch = "Unknown";
unset($output);

if (!preg_match("/^$git_branch(-.*)?$/", $fpp_head_version))
	$fpp_head_version .= " ($git_branch branch)";

$IPs = explode("\n",trim(shell_exec("/sbin/ifconfig -a | cut -f1 -d' ' | grep -v ^$ | grep -v lo | grep -v eth0:0 | grep -v usb | grep -v SoftAp | grep -v 'can.' | sed -e 's/://g' | while read iface ; do /sbin/ifconfig \$iface | grep 'inet ' | awk '{print \$2}'; done")));
$ipStr = '';
if (count($IPs)) {
    $ipStr = '(' . join(' ', $IPs) . ')';
}
?>

<?php require_once(dirname(__FILE__)."/config.php"); ?>
<?php
function list_plugin_entries($menu)
{
	global $pluginDirectory;
	if ( $menu && file_exists($pluginDirectory))
	{
		$handle = opendir($pluginDirectory);
		if ( $handle )
		{
			$first = 1;
			while (($plugin = readdir($handle)) !== false)
			{
				if (!in_array($plugin, array('.', '..')))
				{
					// Old naming convention ${MENU}_menu.inc
					if ( file_exists($pluginDirectory."/".$plugin."/".$menu."_menu.inc") )
					{
						if ($first)
						{
							$first = 0;
							echo "<li><b><font color='white' size=-1>- - - - - - - Plugins - - - - - - -</font></b></li>\n";
						}

						-include_once($pluginDirectory."/".$plugin."/".$menu."_menu.inc");
					}

					// New single menu.inc file which has some logic in it to display relevant menus.
					// See the example in the fpp-plugin-Template plugin
					if ( file_exists($pluginDirectory."/".$plugin."/menu.inc") )
					{
						if ($first)
						{
							$first = 0;
							echo "<li><b><font color='white' size=-1>- - - - - - - Plugins - - - - - - -</font></b></li>\n";
						}

						-include($pluginDirectory."/".$plugin."/menu.inc");
					}
				}
			}
		}
	}
}
?>

  <div style='float: right'><a href='#' class='nonULLink' onClick='ClearRestartFlag(); ClearRebootFlag();'><img src='images/1x1.png' width='5' height='5' style="vertical-align: top;"></a></div>
  <div class="header">
    <div class="headerLeft"><a href="http://www.falconchristmas.com"><img src="images/falconlogo.png" class="logo" /></a></div>
    <div class="headerCenter"><div class="siteName"><? echo $settings['Title']; ?></div>
      Version: <a href='about.php' class='nonULLink'><?php echo $fpp_head_version; ?></a>&nbsp;&nbsp;Host: <? echo "<a href='networkconfig.php' class='nonULLink'>" . $settings['HostName'] . "</a> " . $ipStr; ?>
<?
	if (file_exists("/etc/fpp/rfs_version") && exec('grep "^[^#].*/home/pi/media" /etc/fstab') )
	{
		$mounted = 0;
		$needToMount = 0;
		$mountPoints = file("/etc/fstab");
		foreach ($mountPoints as $mountPoint)
		{
			if (preg_match("/^[^#].*\/home\/[a-z]*\/media/", $mountPoint))
			{
				$needToMount = 1;
				$mounts = file("/proc/mounts");
				$mounted = 0;
				foreach ($mounts as $mount)
				{
					if (preg_match("/\/home\/[a-z]*\/media/", $mount))
					{
						$mounted = 1;
						break;
					}
				}

				break;
			}
		}


		if ($needToMount && !$mounted)
		{
?>
	<br><b><font color='#ff0000'>WARNING: <?= $fppHome; ?>/media is not mounted, is a USB flash drive inserted?</font></b>
<?
		}
	}
?>
    </div>
    <div class="headerRight">
		  <?
            if ($settings['LogoLink'] != "") echo "<a href='" . $settings['LogoLink'] . "' target='_blank'>";
		    if ($settings['Logo'] != "") echo "<img src='images/" . $settings['Logo'] . "' class='logo'>";
		    if ($settings['LogoLink'] != "") echo "</a>";
            if (isSet($settings['cape-info']) && isSet($settings['cape-info']['vendor']) && isSet($settings['cape-info']['vendor']['image'])) {
                $landing = $settings['cape-info']['vendor']['url'];
                if (isSet($settings['cape-info']['vendor']['landingPage'])) {
                    $landing = $settings['cape-info']['vendor']['landingPage'];
                }
                $iurl = $settings['cape-info']['vendor']['image'] . "?sn=" . $settings['cape-info']['serialNumber'] . "&id=" . $settings['cape-info']['id'];
                $landing = $landing . "?sn=" . $settings['cape-info']['serialNumber'] . "&id=" . $settings['cape-info']['id'];
                if (isset($settings['cape-info']['cs']) && $settings['cape-info']['cs'] != "") {
                    $iurl = $iurl . "&cs=" . $settings['cape-info']['cs'];
                    $landing = $landing . "&cs=" . $settings['cape-info']['cs'];
                }
                echo "<a href=\"" . $landing . "\"><img style='max-width:125px; max-height:60px;' src=\"" . $iurl . "\"></a>";
            }
          ?>

		</div>
    <div class="clear"></div>
  </div>

    <ul id="fppMenu" class="sm sm-clean">
      <li><a>Status/Control</a>
        <ul>
          <li><a href="index.php">Status Page</a></li>
          <li><a href="networkconfig.php">Network</a></li>
          <li><a href="multisync.php">MultiSync</a></li>
          <li><a href="settings.php">FPP Settings</a></li>
          <li><a href="backup.php">FPP Backup</a></li>
          <li><a href="proxies.php">Proxy Settings</a></li>
          <li><a href="events.php">Events</a></li>
          <li><a href="effects.php">Effects</a></li>
          <li><a href="testing.php">Display Testing</a></li>
          <?
          if (file_exists($settings['co-other'])) {
            $js = json_decode(file_get_contents($settings['co-other']), true);
            foreach($js["channelOutputs"] as $key => $val) {
                if (($val["enabled"] == 1) && ($val["type"] == "HTTPVirtualDisplay")) {
                echo "<li><a href='virtualdisplaywrapper.php'>Virtual Display</a></li>";
                }
            }
          
          }
          ?>
          
          <?php list_plugin_entries("status"); ?>
        </ul>
      </li>
      <li><a>Content Setup</a>
        <ul>
          <li><a href="uploadfile.php">File Manager</a></li>
          <li><a href="playlists.php">Playlists</a></li>
          <li><a href="scheduler.php">Scheduler</a></li>
          <li><a href="scriptbrowser.php">Script Repository Browser</a></li>
          <li><a href="plugins.php">Plugin Manager</a></li>
          <?php list_plugin_entries("content"); ?>
        </ul>
      </li>
      <li><a>Input/Output Setup</a>
        <ul>
          <li><a href="channelinputs.php">Channel Inputs</a></li>
          <li><a href="channeloutputs.php">Channel Outputs</a></li>
          <li><a href="outputprocessors.php">Output Processors</a></li>
          <li><a href="pixeloverlaymodels.php">Pixel Overlay Models</a></li>
          <li><a href="gpio.php">GPIO Inputs</a></li>
          <?php list_plugin_entries("output"); ?>
        </ul>
      </li>
      <li><a>Help</a>
        <ul>
          <li><a href="https://docs.google.com/viewer?url=https://falconchristmas.github.io/FPP_Manual.pdf" target="_blank">FPP Manual</a></li>
          <li><a href="http://falconchristmas.com/wiki/index.php/Falcon_FPP" target="_blank">Wiki/Help</a></li>
          <li><a href="help.php">Help Index</a></li>
<? if ($settings['uiLevel'] > 0) { ?>
          <li><a href="api/help">REST API Help</a></li>
<? } ?>
          <li><a href="cliHelp.php">fpp &amp; fppmm Usage</a></li>
          <li><a href="troubleshooting.php">Troubleshooting Commands</a></li>
          <li><a href="http://<?php echo $_SERVER['SERVER_ADDR'] ?>:4200" target="_new">SSH Shell</a></li>
          <li><a href="about.php">About</a></li>
          <li><a href="credits.php">Credits</a></li>
          <?php list_plugin_entries("help"); ?>
        </ul>
      </li>
    </ul>
<!--
  <div id='upgradeFlag'>FPP v1.6 is available for install - <input name="btnUpgrade" onClick="ViewReleaseNotes('1.6');" type="button" class = "buttons" value="Upgrade"></div>
-->
  <div id='restartFlag'>Settings have changed. FPPD Restart Required - <input name="btnRestartFPPD" type="button" onClick="RestartFPPD();" class = "buttons" value="Restart FPPD">
<?
  if (basename($_SERVER['PHP_SELF']) == "developer.php")
  {
?>
  <input type='button' class = "buttons" value='Clear' onClick='ClearRestartFlag();'>
<?
  }
?>
  </div>
  <div id='rebootFlag'>Base configuration has changed. Reboot Required - <input name="btnReboot" onClick="RebootPi();" type="button" class = "buttons" value="Reboot">
<?
  if (basename($_SERVER['PHP_SELF']) == "developer.php")
  {
?>
  <input type='button' class = "buttons" value='Clear' onClick='ClearRebootFlag();'>
<?
  }
?>
  </div>

      <div class='helpLinkDiv'><a href='javascript:void();' onClick='DisplayHelp();' class='nonULLink'>&nbsp;Press F1 for help&nbsp;&nbsp;</a></div>

<script language="Javascript">
function handleKeypress(e) {
	if(e.keyCode == 112) {
		e.preventDefault();
		DisplayHelp();
	}
};

$(document).ready(function() {
	$(document).on('keydown', handleKeypress);

	CheckRestartRebootFlags();
});

</script>

