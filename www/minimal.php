<!DOCTYPE html>
<html>
<head>
<?php
require_once('config.php');
require_once('common.php');

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

?>
<script type="text/javascript" src="js/jquery-3.4.1.min.js"></script>
<script type="text/javascript" src="js/jquery-migrate-3.1.0.min.js"></script>
<script type="text/javascript" src="js/jquery-ui.min.js"></script>
<script type="text/javascript" src="js/jquery.ui.touch-punch.js"></script>
<script type="text/javascript" src="js/jquery.jgrowl.min.js"></script>
<link rel="stylesheet" href="css/jquery-ui.css" />
<link rel="stylesheet" href="css/jquery.jgrowl.min.css" />
<link rel="stylesheet" href="css/classicTheme/style.css" media="all" />
<link rel="stylesheet" href="css/minimal.css?ref=<?php echo filemtime('css/minimal.css'); ?>" />
<script type="text/javascript" src="js/fpp.js?ref=<?php echo filemtime('js/fpp.js'); ?>"></script>

<script>
	minimalUI = 1;
    PlayEntrySelected = 0;
    PlaySectionSelected = '';

    $(function() {
		$('#tblStatusPlaylist tbody').on('mousedown', 'tr', function(event,ui){
					$('#tblStatusPlaylist tbody tr').removeClass('playlistSelectedEntry');
// FIXME, may need to check each tbody individually
          $(this).addClass('playlistSelectedEntry');
					var items = $('#tblStatusPlaylistEntries tbody tr');
					PlayEntrySelected = parseInt($(this).attr('id').substr(11)) - 1;
					PlaySectionSelected = $(this).parent().attr('id').substr(11);
		});
	});
</script>

<title><? echo $pageTitle; ?></title>
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, minimal-ui">
<meta name="apple-mobile-web-app-capable" content="yes" />

	<script>
		$(function() {

			//Store frequently elements in variables
			var slider  = $('#slider');
			//Call the Slider
			slider.slider({
				//Config
				range: "min",
				min: 1,
				//value: 35,
			});


			$('#slider').slider({
				stop: function( event, ui ) {
					var value = slider.slider('value');

					SetSpeakerIndicator(value);
					$('#volume').html(value);
					SetVolume(value);
				}
			});
		});

	function SetSpeakerIndicator(value) {
		var speaker = $('#speaker');

		if(value <= 5)
		{
			speaker.css('background-position', '0 0');
		}
		else if (value <= 25)
		{
			speaker.css('background-position', '0 -25px');
		}
		else if (value <= 75)
		{
			speaker.css('background-position', '0 -50px');
		}
		else
		{
			speaker.css('background-position', '0 -75px');
		};
	}

	function IncrementVolume()
	{
		var volume = parseInt($('#volume').html());
		volume += 1;
		if (volume > 100)
			volume = 100;
		$('#volume').html(volume);
		$('#slider').slider('value', volume);
		SetSpeakerIndicator(volume);
		SetVolume(volume);
	}

	function DecrementVolume()
	{
		var volume = parseInt($('#volume').html());
		volume -= 1;
		if (volume < 0)
			volume = 0;
		$('#volume').html(volume);
		$('#slider').slider('value', volume);
		SetSpeakerIndicator(volume);
		SetVolume(volume);
	}

	</script>


</head>
<body onLoad="GetFPPDmode();PopulatePlaylists(true);GetFPPStatus();bindVisibilityListener();GetVolume();">
<div id="bodyWrapper">
  <div class="header">
    <div class="headerCenter"><div class="siteName"><? echo $settings['Title']; ?></div>
      Version: <a href='about.php' class='nonULLink'><?php echo $fpp_head_version; ?></a><br>Host: <? echo "<a href='networkconfig.php' class='nonULLink'>" . $settings['HostName'] . "</a> (" . join(' ', $IPs) . ")"; ?>
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
	}
?>
    </div>
    <div class="clear"></div>
  </div>
  <hr>
<div id="programControl" class="settings">
    <div id="daemonControl">
      <table width= "100%">
        <tr>
          <td class='controlHeader'> FPP Mode: </td>
          <td class='controlValue'>
<?
    if (isset($settings['fppMode']))
	{
		if ($settings['fppMode'] == 'master')
			echo "Master";
		if ($settings['fppMode'] == 'player')
			echo "Player (Standalone)";
		if ($settings['fppMode'] == 'remote')
			echo "Remote <b>(Not fully supported in minimal UI)</b>";
		if ($settings['fppMode'] == 'bridge')
			echo "Bridge <b>(Not fully supported in minimal UI)</b>";
	}
?>
			</td>
        </tr>
        <tr>
          <td class='controlHeader'> FPP Status: </td>
          <td class='controlValue' id = "daemonStatus"></td>
        </tr>
        <tr>
          <td class='controlHeader'> FPP Time: </td>
          <td id = "fppTime" colspan = "2"></td>
        </tr>
      </table>
    </div>
    <div id="playerInfo">
      <hr>
      <div id="remoteStatus">
	  		<table>
	  			<tr><td>Remote Status:</td>
	  					<td id='txtRemoteStatus'></td></tr>
	  			<tr><td>Sequence Filename:</td>
	  					<td id='txtRemoteSeqFilename'></td>
	  					</tr>
	  			<tr><td>Media Filename:</td>
	  					<td id='txtRemoteMediaFilename'></td>
	  					</tr>
	  		</table>
      </div>
    	<div id="playerStatusTop">

      <div id = "startPlaylistControls">
        <table width="100%">
          <tr>
            <td class='controlHeader'>Player: </td>
            <td id="txtPlayerStatus"></td>
          </tr>
          <tr>
            <td class='controlHeader'>Playlist:</td>
            <td><select id="playlistSelect" name="playlistSelect" size="1" onClick="SelectStatusPlaylistEntryRow();PopulateStatusPlaylistEntries(true,'',true);" onChange="PopulateStatusPlaylistEntries(true,'',true);"></select>
            	&nbsp;Repeat: <input type="checkbox" id="chkRepeat"></input>
				</td>
          </tr>
          <tr>
            <td class='controlHeader'>Volume :</td>
            <td>[<span id='volume' class='volume'></span>]
            	&nbsp;&nbsp;&nbsp;
				<input type="button" class='volumeButton' value="-" onClick="DecrementVolume();">
            	&nbsp;&nbsp;&nbsp;
			    <input type="button" class='volumeButton' value="+" onClick="IncrementVolume();">
				</td>
		  </tr>
      </table>
      </div>
		</div>

      <div id="playerControls" style="margin-top: 10px">
        <input id= "btnPlay" type="button"  class ="buttons"value="Play" onClick="StartPlaylistNow();">
        <input id= "btnStopGracefully" type="button"  class ="buttons"value="Stop Gracefully" onClick="StopGracefully();">
        <input id= "btnStopGracefullyAfterLoop" type="button"  class ="buttons"value="Stop After Loop" onClick="StopGracefullyAfterLoop();">
        <input id= "btnStopNow" type="button" class ="buttons" value="Stop Now" onClick="StopNow();">
		<hr>
       	<input id="btnShowPlaylistDetails" type="button" class='buttons' value="Show Details" onClick="ShowPlaylistDetails();">
       	<input id="btnHidePlaylistDetails" type="button" class='buttons' value="Hide Details" onClick="HidePlaylistDetails();" style='display: none;'>
       </div>
    	<div id="playerStatusBottom" style='margins: 5px'>
		<div id='playlistDetailsWrapper' style='display: none;'>
        <table width="100%">
          <tr>
			<td class='controlHeader'>Position:</td>
            <td><span id="txtTimePlayed"></span>&nbsp;&nbsp;
            	<span id="txtTimeRemaining"></span>
				</td>
          </tr>
      </table>
      <div id="statusPlaylist"  class="unselectable">
        <div id= "statusPlaylistContents">
        <table id="tblStatusPlaylist" width="100%">
			<colgroup>
				<col class='colPlaylistNumber'></col>
				<col class='colPlaylistData1'></col>
			</colgroup>
          <thead>
          <tr class="playlistHeader">
		  	<th class='colPlaylistNumber'>#</th>
            <th class='colPlaylistData1'>Playlist Info</th>
          </tr>
						<tbody id='tblPlaylistLeadInHeader' style='display: none;'>
							<tr><th colspan=4>-- Lead In --</th></tr>
						</tbody>
            <tbody id="tblPlaylistLeadIn">
            </tbody>
						<tbody id='tblPlaylistMainPlaylistHeader' style='display: none;'>
							<tr><th colspan=4>-- Main Playlist --</th></tr>
						</tbody>
            <tbody id="tblPlaylistMainPlaylist">
            </tbody>
						<tbody id='tblPlaylistLeadOutHeader' style='display: none;'>
							<tr><th colspan=4>-- Lead Out --</th></tr>
						</tbody>
            <tbody id="tblPlaylistLeadOut">
            </tbody>
          </table>
        </div>
      </div>
	</div>

    </div>
    <div id= "nextPlaylist">
      <table  width="100%">
        <tr>
          <td class='controlHeader'> Next Playlist: </td>
          <td id = "txtNextPlaylist"></td>
        </tr>
        <tr>
          <td class='controlHeader'> Time: </td>
          <td id="nextPlaylistTime"></td>
        </tr>
      </table>
    </div>
		</div>
</div>
</div>
<center><a href='index.php'>Return to Full UI</a></center><br>
</body>
</html>
