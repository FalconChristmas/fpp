<?php

$kernel_version = exec("uname -r");

$fpp_head_version = "v" . getFPPVersion();
$git_branch = getFPPBranch();

if (!preg_match("/^$git_branch(-.*)?$/", $fpp_head_version))
	$fpp_head_version .= " ($git_branch branch)";

?>

<?php require_once(dirname(__FILE__)."/config.php"); ?>
<?php
function list_plugin_entries($menu) {
	global $pluginDirectory;
    $menukey = $menu;
    if ($menu == "output") {
        $menukey = "input-output";
    }
	if ($menu && file_exists($pluginDirectory)) {
		$handle = opendir($pluginDirectory);
		if ( $handle ) {
			$first = 1;
			while (($plugin = readdir($handle)) !== false) {
				if (!in_array($plugin, array('.', '..'))) {
					// Old naming convention ${MENU}_menu.inc
					if ( file_exists($pluginDirectory."/".$plugin."/".$menu."_menu.inc") ) {
						if ($first) {
							$first = 0;
                            echo '<h6 class="dropdown-header">- - - - - - - Plugins - - - - - - -</h6>';
						}

                        ob_start();
                        -include_once($pluginDirectory."/".$plugin."/".$menu."_menu.inc");
                        $buffer = ob_get_clean();
                        $buffer = str_replace(array("<li>","</li>","</li\n>"),"",$buffer);
                        $buffer = str_replace("<a ","<a class=\"dropdown-item\" ",$buffer);
                        $buffer = str_replace("plugin.php?plugin", "plugin.php?_menu=" . $menukey . "&plugin", $buffer);
                        echo $buffer;
                    }

					// New single menu.inc file which has some logic in it to display relevant menus.
					// See the example in the fpp-plugin-Template plugin
					if ( file_exists($pluginDirectory."/".$plugin."/menu.inc") ) {
                        ob_start();
                        -include($pluginDirectory."/".$plugin."/menu.inc");
                        $buffer = ob_get_clean();
                        $buffer = str_replace(array("<li>","</li>"),"",$buffer);
                        $buffer = str_replace("<a ","<a class=\"dropdown-item\" ",$buffer);
                        $buffer = str_replace("plugin.php?plugin", "plugin.php?_menu=" . $menukey . "&plugin",  $buffer);
                        if ($first && !empty($buffer)) {
                            $first = 0;
                            echo '<h6 class="dropdown-header">- - - - - - - Plugins - - - - - - -</h6>';
                        }

                        echo $buffer;
					}
				}
			}
		}
	}
}
?>

   <div class="header">
    <div class="headerLeft">
          <div class="headerLeftCol fppBrand">
            <a href="/">
              <img src="images/redesign/falconlogo.svg" class="logo" title="Host: <? echo $settings['HostName']; ?>" />
              <img src="images/redesign/fpp-logo.svg" class="fpp-logo" />
            </a>
            <a href='about.php' class='nonULLink headerLink versionHead'><?php echo $fpp_head_version; ?></a>
          </div>

    </div>
    <div class="headerCenter">
      <div class="headerCenterRow headerHost">
<?
    if (!file_exists("/etc/fpp/desktop")) {
        echo("<span class=\"headerHostName headerBox\" id=\"header_host\"><span title=\"Host: ".gethostname()."\"><a href='networkconfig.php#tab-host-dns' class='nonULLink'>".$settings['HostName']."</a></span></span>");
    } else {
        echo("<span class=\"headerHostName headerBox\" id=\"header_host\"><span title=\"Host: ".gethostname()."\">".gethostname()."</span></span>");
    }

    $currentHeaderSensor = "1";
    if (isset($settings['currentHeaderSensor'])) {
       $currentHeaderSensor = $settings['currentHeaderSensor'];
    }

?>
    <span class="headerBox" id="header_player"></span>
    <span class="headerBox largeonly" id="header_sensors" data-defaultsensor="<? echo $currentHeaderSensor; ?>"></span>
    <span class="headerBox" id="header_IPs"></span>
    <span class="headerBox" id="header_Time"></span>
</div>
    </div>
    <div class="headerRight largeonly">
		  <?
            if ($settings['LogoLink'] != "") echo "<a href='" . $settings['LogoLink'] . "' target='_blank' class='platformLogoLink'>";
		    if ($settings['Logo'] != "") {
           echo "<img src='images/" . $settings['Logo'] . "' class='logo'>"; }
		    if ($settings['LogoLink'] != "") echo "</a>";
            if (isSet($settings['cape-info']) && isSet($settings['cape-info']['vendor']) && isSet($settings['cape-info']['vendor']['image']) && $settings['FetchVendorLogos'] == 1) {
                $landing = $settings['cape-info']['vendor']['url'];
                if (isSet($settings['cape-info']['vendor']['landingPage'])) {
                    $landing = $settings['cape-info']['vendor']['landingPage'];
                }
                if ($settings['SendVendorSerial'] == 1) {
                     $iurl = $settings['cape-info']['vendor']['image'] . "?sn=" . $settings['cape-info']['serialNumber'] . "&id=" . $settings['cape-info']['id'];
                } else {
                     $iurl = $settings['cape-info']['vendor']['image']; 
                }

                if ($settings['SendVendorSerial'] == 1) {
                    $landing = $landing . "?sn=" . $settings['cape-info']['serialNumber'] . "&id=" . $settings['cape-info']['id'];
                }
                if (isset($settings['cape-info']['cs']) && $settings['cape-info']['cs'] != "" && $settings['SendVendorSerial'] == 1) {
                    $iurl = $iurl . "&cs=" . $settings['cape-info']['cs'];
                }
                ?>
                      <div class="capeVendor">
                        <div class="capeVendorLogo">
                          <a href="<? echo $landing; ?>" target="capeVendor">
                           <img src="<? echo $iurl; ?>" class="logo" / >
                          </a>
                        </div>
                      </div>
                <?

            }
          ?>
		</div>

  </div>
  <div style='float: right'><a href='#' class='nonULLink' onClick='ClearRestartFlag(); ClearRebootFlag();'><img src='images/1x1.png' width='5' height='5' style="vertical-align: top;"></a></div>
  <div class='helpLinkDiv largeonly'><a href='javascript:void();' onClick='DisplayHelp();' class='nonULLink'>&nbsp;Press F1 for help&nbsp;&nbsp;</a></div>

  <div class="fppOuterContainer">
    <div class="fppMenuContainer">
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
  <div class="alert danger">
    WARNING: <?= $fppHome; ?>/media is not mounted, is a USB flash drive inserted?
  </div>
<?
		}
	}
?>
  <!--div id='upgradeFlag' class="alert detract"><span class='inlineBlock'>FPP v5.1 is available for install</span><span class='inlineBlock'><button name="btnUpgrade" type="button" onClick="ViewReleaseNotes('5.1');"  class = "buttons btn-outline-light" ><i class='fas fa-fw fa-sync fa-nbsp'></i>Upgrade FPP</button></div-->
  <div id='restartFlag' class="alert detract"><span class='inlineBlock'>Settings have changed. FPPD Restart Required</span><span class='inlineBlock'><button name="btnRestartFPPD" type="button" onClick="RestartFPPD();" class="buttons btn-outline-light"><i class='fas fa-fw fa-sync fa-nbsp'></i>Restart FPPD</button>

<?
  if (basename($_SERVER['PHP_SELF']) == "developer.php")
  {
?>
  <input type='button' class = "buttons btn-outline-light" value='Clear' onClick='ClearRestartFlag();'>
<?
  }
?>
    </span>
  </div>
<?  if (!file_exists("/opt/fpp/src/fppd")) { ?>
<div id='rebuildPopup' title='FPP Upgrade' style="display: none">
    <textarea style='width: 99%; height: 97%;' disabled id='rebuildText'>
    </textarea>
</div>
<script>
function RebuildFPPD() {
    $('#rebuildPopup').fppDialog({ height: 600, width: 900, title: "Rebuild FPP" });
    $('#rebuildPopup').fppDialog( "moveToTop" );
    $('#rebuildText').html('');
    StreamURL('manualUpdate.php?wrapped=1', 'rebuildText');
}
</script>
  <div id="compileFPPDBanner" class="alert alert-danger"><span class='inlineBlock'>FPPD not found. Rebuild required</span><span class='inlineBlock'><input name="btnRebuild" onClick="RebuildFPPD();" type="button" class = "buttons btn-outline-light" value="Rebuild"></span></div>
<? } ?>
  <div id='unsupportedBrowser' class="alert alert-danger">FPP will not work with this browser.  Chrome, Firefox, or Edge is strongly recommended.</div>
  <div id='rebootFlag' class="alert detract"><span class='inlineBlock'>Base configuration has changed. Reboot Required</span><span class='inlineBlock'><input name="btnReboot" onClick="RebootPi();" type="button" class = "buttons btn-outline-light" value="Reboot">
<?
  if (basename($_SERVER['PHP_SELF']) == "developer.php")
  {
?>
  <input type='button' class = "buttons btn-outline-light" value='Clear' onClick='ClearRebootFlag();'>
<?
  }
?>
    </span>
  </div>


  <nav id="fppMenu" class="navbar navbar-expand-lg navbar-dark">
  <div class="navbar-translate">
  <button class="navbar-toggler" type="button" data-toggle="collapse" data-target="#navbarNavDropdown" aria-controls="navbarNavDropdown" aria-expanded="false" aria-label="Toggle navigation">
    <span class="navbar-toggler-icon"></span>
  </button>
          </div>
  <div class="collapse navbar-collapse" id="navbarNavDropdown">
    <ul id="fppMenuList" class="navbar-nav">
    <li class="nav-item dropdown <? echo $activeParentMenuItem=='status' ? 'active' :''; ?>">
        <a class="nav-link dropdown-toggle" href="#" id="navbarDropdownMenuLink" role="button" data-toggle="dropdown" aria-haspopup="true" aria-expanded="false">
        Status/Control
        </a>
        <div class="dropdown-menu dropdown-with-icons" aria-labelledby="navbarDropdownMenuLink">
        <a class="dropdown-item" href="index.php"><i class="fas fa-info"></i> Status Page</a>
<?
if (!file_exists("/etc/fpp/desktop")) {
          echo "<a class='dropdown-item' href='networkconfig.php'><i class='fas fa-network-wired'></i> Network</a>";
}
?>
          <a class="dropdown-item" href="multisync.php"><i class="fas fa-cogs"></i> MultiSync</a>
          <a class="dropdown-item" href="settings.php"><i class="fas fa-cog"></i> FPP Settings</a>
          <a class="dropdown-item" href="backup.php"><i class="fas fa-sim-card"></i> FPP Backup</a>
          <a class="dropdown-item" href="proxies.php"><i class="fas fa-globe"></i> Proxy Settings</a>
          <a class="dropdown-item" href="commandPresets.php"><i class="fas fa-calendar-alt"></i> Command Presets</a>
          <a class="dropdown-item" href="effects.php"><i class="fas fa-magic"></i> Effects</a>
          <a class="dropdown-item" href="testing.php"><i class="fas fa-clipboard-check"></i> Display Testing</a>
          <?
          if (file_exists($settings['co-other'])) {
            $js = json_decode(file_get_contents($settings['co-other']), true);
            if (isset($js["channelOutputs"])) {
              foreach($js["channelOutputs"] as $key => $val) {
                if (($val["enabled"] == 1) && ($val["type"] == "HTTPVirtualDisplay")) {
                echo "<a class='dropdown-item' href='virtualdisplaywrapper.php'><i class='fas fa-tv'></i> Virtual Display</a>";
                }
              }
            }
          }
          ?>

          <?php list_plugin_entries("status"); ?>
        </div>
      </li>

      <li class="nav-item dropdown <? echo $activeParentMenuItem=='content' ? 'active' :''; ?>">
        <a class="nav-link dropdown-toggle" href="#" id="navbarDropdownMenuLink" role="button" data-toggle="dropdown" aria-haspopup="true" aria-expanded="false">
        Content Setup
        </a>
        <div class="dropdown-menu dropdown-with-icons" aria-labelledby="navbarDropdownMenuLink">
        <a class="dropdown-item" href="uploadfile.php"><i class="fas fa-upload"></i> File Manager</a>
          <a class="dropdown-item" href="playlists.php"><i class="fas fa-list-ol"></i> Playlists</a>
          <a class="dropdown-item" href="scheduler.php"><i class="fas fa-clock"></i> Scheduler</a>
          <a class="dropdown-item" href="scriptbrowser.php"><i class="fas fa-scroll"></i> Script Repository Browser</a>
          <a class="dropdown-item" href="plugins.php"><i class="fas fa-puzzle-piece"></i> Plugin Manager</a>
          <?php list_plugin_entries("content"); ?>
        </div>
      </li>

      <li class="nav-item dropdown <? echo $activeParentMenuItem=='input-output' ? 'active' :''; ?>">
        <a class="nav-link dropdown-toggle" href="#" id="navbarDropdownMenuLink" role="button" data-toggle="dropdown" aria-haspopup="true" aria-expanded="false">
        Input/Output Setup
        </a>
        <div class="dropdown-menu dropdown-with-icons" aria-labelledby="navbarDropdownMenuLink">
          <a class="dropdown-item" href="channelinputs.php"><i class="fas fa-project-diagram" style="color:red;"></i> Channel Inputs</a>
          <a class="dropdown-item" href="channeloutputs.php"><i class="fas fa-project-diagram" style="color:green;"></i> Channel Outputs</a>
          <a class="dropdown-item" href="outputprocessors.php"><i class="fas fa-microchip"></i> Output Processors</a>
          <a class="dropdown-item" href="pixeloverlaymodels.php"><i class="fas fa-cubes"></i> Pixel Overlay Models</a>
          <a class="dropdown-item" href="gpio.php"><i class="fas fa-exchange-alt"></i> GPIO Inputs</a>
          <?php list_plugin_entries("output"); ?>
        </div>
      </li>

      <li class="nav-item dropdown <? echo $activeParentMenuItem=='help' ? 'active' :''; ?>">
        <a class="nav-link dropdown-toggle" href="#" id="navbarDropdownMenuLink" role="button" data-toggle="dropdown" aria-haspopup="true" aria-expanded="false">
        Help
        </a>
        <div class="dropdown-menu dropdown-with-icons dropdown-menu-right" aria-labelledby="navbarDropdownMenuLink">
          <a class="dropdown-item" href="about.php"><i class="fas fa-info-circle"></i> About</a>
          <a class="dropdown-item link-to-fpp-manual" href="https://added-by-javascript.net" target="_blank"><i class="fas fa-book"></i> FPP Manual <i class="fas fa-external-link-alt external-link"></i></a>
          <a class="dropdown-item" href="https://falconchristmas.com/forum" target="_blank"><i class="fas fa-comments"></i> Forums <i class="fas fa-external-link-alt external-link"></i></a>
          <!--
          <a class="dropdown-item" href="http://falconchristmas.com/wiki/index.php/Falcon_FPP" target="_blank"><i class="fas fa-external-link-square-alt"></i> Wiki/Help <i class="fas fa-external-link-alt external-link"></i></a>
          -->
          <a class="dropdown-item" href="help.php"><i class="fas fa-question-circle"></i> Help Index</a>
          <a class="dropdown-item" href="apihelp.php"><i class="fas fa-plug"></i> REST API Help</a>
          <a class="dropdown-item" href="cliHelp.php"><i class="fas fa-life-ring"></i> fpp &amp; fppmm Usage</a>
          <a class="dropdown-item" href="credits.php"><i class="fas fa-stream"></i> Credits</a>
          <hr class="dropdown-divider">
          <a class="dropdown-item" href="healthCheck.php"><i class="fas fa-thermometer-half"></i> System Health Check</a>
          <a class="dropdown-item" href="troubleshooting.php"><i class="fas fa-code"></i> Troubleshooting Commands</a>
          <a class="dropdown-item" href="http://<?php echo $_SERVER['SERVER_ADDR'] ?>:4200" target="_new"><i class="fas fa-terminal"></i> SSH Shell</a>
          <?php list_plugin_entries("help"); ?>
        </div>
      </li>
    </ul>
    </div>
</nav>


</div>
