<!DOCTYPE html>
<html>
<head>
<?php
require_once('config.php');
include 'common/menuHead.inc';
?>
<script>
    PlayEntrySelected = 0;
    $(function() {
		$('#tblStatusPlaylistEntries').on('mousedown', 'tr', function(event,ui){
					$('#tblStatusPlaylistEntries tr').removeClass('playlistSelectedEntry');
          $(this).addClass('playlistSelectedEntry');
					var items = $('#tblStatusPlaylistEntries tr');
					PlayEntrySelected  = items.index(this);

		});
	});
</script>

<title><? echo $pageTitle; ?></title>

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
<body onLoad="GetFPPDmode();StatusPopulatePlaylists();setInterval(updateFPPStatus,1000);GetVolume();">
<div id="bodyWrapper">
<?php
	include 'menu.inc';
  ?>
<br/>
<div id="programControl" class="settings">
  <fieldset>
    <legend>Program Control</legend>
    <div id="daemonControl">
      <table width= "100%">
        <tr>
          <td class='controlHeader'> FPPD Mode: </td>
          <td class='controlValue'>
				  	<select id="selFPPDmode" onChange="SetFPPDmode();">
							<option id="optFPPDmode_Player" value="2">Player (Standalone)</option>
							<option id="optFPPDmode_Master" value="6">Player (Master)</option>
							<option id="optFPPDmode_Remote" value="8">Player (Remote)</option>
							<option id="optFPPDmode_Bridge" value="1">Bridge</option>
						</select></td>
          <td class='controlButton'>&nbsp;</td>
        </tr>
        <tr>
          <td class='controlHeader'> FPPD Status: </td>
          <td class='controlValue' id = "daemonStatus"></td>
          <td class='controlButton'><input type="button" id="btnDaemonControl" class ="buttons" value="" onClick="ControlFPPD();"></td>
        </tr>
        <tr>
          <td class='controlHeader'> FPP Time: </td>
          <td id = "fppTime" colspan = "3"></td>
        </tr>
      </table>
    </div>
    <div id="bytesTransferred"><H3>Bytes Transferred</H3>
      <hr>
      <div id="bridgeStatistics1"></div>
      <div id="bridgeStatistics2"></div>
      <div class="clear"></div>
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
      <div>
        <div class='playerStatusLeft'>
          <table  width= "100%">
            <tr>
              <td class='playerStatusHeader'>Player Status: </td>
              <td id="txtPlayerStatus"></td>
            </tr>
          </table>
        </div>
        <div class='playerStatusRight'>
          <table  width= "100%">
            <tr>
              <td class='playerStatusHeader' id="txtTimePlayed"></td>
              <td id="txtTimeRemaining"></td>
            </tr>
          </table>
        </div>
        <div class="clear"></div>
      </div>

      <div id = "startPlaylistControls">
        <table width="100%">
          <tr>
            <td class='controlHeader'>Playlist:</td>
            <td><select id="selStartPlaylist" name="selStartPlaylist" size="1" onClick="SelectStatusPlaylistEntryRow();PopulateStatusPlaylistEntries(true,'',true);" onChange="PopulateStatusPlaylistEntries(true,'',true);"></select></td>
            <td><input type="checkbox" id="chkRepeat">Repeat</input></td>
          </tr>
      </table>
      </div>
		</div>

    <div id="volumeControls">
			<table width="100%">
          <tr>
            <td class='controlHeader'>Volume [<span id='volume' class='volume'></span>]:</td>
            <td>
				<input type="button" class='volumeButton' value="-" onClick="DecrementVolume();">
                <span id="slider"></span> <!-- the Slider -->
			    <input type="button" class='volumeButton' value="+" onClick="IncrementVolume();">
                <span id='speaker'></span> <!-- Volume -->
            </td>
		  </tr>
			</table>
		</div>

    	<div id="playerStatusBottom">
      <div id="statusPlaylist"  class="unselectable">
        <table id="tblStatusPlaylistHeader" width="100%">
          <tr class="playlistHeader">
            <td width="6%">#</td>
            <td  width="42%">Media File / Event / Pause </td>
            <td  width="42%">Sequence / Delay</td>
            <td  width="10%">First/Last</td>
          </tr>
        </table>
        <div id= "statusPlaylistContents">
          <table id="tblStatusPlaylistEntries"   width="100%">
          </table>
        </div>
      </div>

      <div id="playerControls" style="margin-top:5px">
        <input id= "btnPlay" type="button"  class ="buttons"value="Play" onClick="StartPlaylistNow();">
        <input id= "btnStopGracefully" type="button"  class ="buttons"value="Stop Gracefully" onClick="StopGracefully();">
        <input id= "btnStopNow" type="button" class ="buttons" value="Stop Now" onClick="StopNow();">
       </div>
    </div>
    <div id= "nextPlaylist">
      <table  width="100%">
        <tr>
          <td class='controlHeader'> Next Playlist: </td>
          <td id = "txtNextPlaylist" width = "85%"></td>
        </tr>
        <tr>
          <td class='controlHeader'> Time: </td>
          <td width="85%" id = "nextPlaylistTime"></td>
        </tr>
      </table>
    </div>
		</div>
  </fieldset>
</div>
<?php	include 'common/footer.inc'; ?>
</body>
</html>
