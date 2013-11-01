<!DOCTYPE html>
<html>
<head>
<?php	include 'common/menuHead.inc'; ?>
<script>
    $(function() {
		$('#tblStatusPlaylistEntries').on('mousedown', 'tr', function(event,ui){
					$('#tblStatusPlaylistEntries tr').removeClass('playlistSelectedEntry');
          $(this).addClass('playlistSelectedEntry');
					var items = $('#tblStatusPlaylistEntries tr');
					PlayEntrySelected  = items.index(this);

		});
	});
</script>

<title>Falcon PI Player - FPP</title>

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

						var value  = slider.slider('value'),
							volume = $('.volume');

						if(value <= 5)
						{
							volume.css('background-position', '0 0');
						}
						else if (value <= 25)
						{
							volume.css('background-position', '0 -25px');
						}
						else if (value <= 75)
						{
							volume.css('background-position', '0 -50px');
						}
						else
						{
							volume.css('background-position', '0 -75px');
						};

						SetVolume(value);
		}
		});

					$('#slider').slider({
		stop: function( event, ui ) {

						var value  = slider.slider('value'),
							volume = $('.volume');

						if(value <= 5)
						{
							volume.css('background-position', '0 0');
						}
						else if (value <= 25)
						{
							volume.css('background-position', '0 -25px');
						}
						else if (value <= 75)
						{
							volume.css('background-position', '0 -50px');
						}
						else
						{
							volume.css('background-position', '0 -75px');
						};

						SetVolume(value);
		}
		});


		});

	function IncrementVolume()
	{
		var volume = parseInt($('#volume').html());
		volume += 3;
		if (volume > 100)
			volume = 100;
		$('#volume').html(volume.toString());

		SetVolume(volume);
	}

	function DecrementVolume()
	{
		var volume = parseInt($('#volume').html());
		volume -= 3;
		if (volume < 0)
			volume = 0;
		$('#volume').html(volume.toString());

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
          <td class='controlValue'><select id="selFPPDmode"  onChange="SetFPPDmode();">
          										<option id="optFPPDmode_Player" value="0">

          										Player Mode</option>
          										<option id="optFPPDmode_Bridge" value="1">
          										Bridge Mode</option>
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
    <hr>
    <div id="bytesTransferred"><H3>Bytes Transferred</H3>
    <div id="bridgeStatistics1"></div>
    <div id="bridgeStatistics2"></div>
    <div class="clear"></div>
    </div>
    <div id="playerStatus">
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
            <td class='controlHeader'><div class='desktopItem'>Load this playlist:</div><div class='mobileItem'>Load playlist:</div></td>
            <td  width="50%"><select id="selStartPlaylist" name="selStartPlaylist" size="1" onClick="SelectStatusPlaylistEntryRow();PopulateStatusPlaylistEntries(true,'',true);" onChange="PopulateStatusPlaylistEntries(true,'',true);"></select></td>
            <td  width="15%"><input type="checkbox" id="chkRepeat">
              Repeat
              </input></td>
            <td  width="15%">
              <div class='desktopItem'>
                <div id="slider"></div> <!-- the Slider -->
                <span class="volume"></span> <!-- Volume -->
              </div>
            </td>
          </tr>
          <tr><td><div class='mobileItem'>Volume:</div></td>
            <td colspan="3"><div class='mobileItem' id='volume' style='float: left'></div><div class='mobileItem' style='float: left'>&nbsp;
              <input type="button" value="+" onClick="IncrementVolume();">
              &nbsp;
              <input type="button" value="-" onClick="DecrementVolume();">
              </div>
            </div>
          </td></tr>
      </table>
      </div>
      <div id="statusPlaylist"  class="unselectable">
        <table id="tblStatusPlaylistHeader" width="100%">
          <tr class="playlistHeader">
            <td width="6%">#</td>
            <td  width="42%">Songname / Pause</td>
            <td  width="42%">Sequence</td>
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
  </fieldset>
    <div id="rebootShutdown">
      <table  width="100%">
        <tr>
          <td width="20%"><input name="btnReboot" onClick="RebootPi();" type="button" class = "buttons" value="Reboot Pi"></td>
          <td id = "txtNextPlaylist" width = "80%"><input name="btnShutdown" type="button" onClick="ShutdownPi();" class = "buttons" value="Shutdown Pi"></td>
        </tr>
      </table>
    </div>
</div>
<?php	include 'common/footer.inc'; ?>
</body>
</html>
