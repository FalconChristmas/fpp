<!DOCTYPE html>
<html>
<head>
<?php	include 'common/menuHead.inc'; ?>
<script type="text/javascript" src="js/fpp.js"></script>
 <script src="http://code.jquery.com/jquery-1.9.1.js"></script>
<script src="http://code.jquery.com/ui/1.10.3/jquery-ui.js"></script>
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
				value: 35,
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
	</script>
  <script>
	</script>


</head>
<body onLoad="GetFPPstatus();StatusPopulatePlaylists();setInterval(updateFPPStatus,1000);">
<div id="bodyWrapper">
<?php
	include 'menu.inc';
  ?>
<br/>
<div id = "programControl">
  <fieldset>
    <legend>Program Control</legend>
    <div id="daemonControl">
      <table width= "100%">
        <tr>
          <td width = "20%"> FPPD Status: </td>
          <td id = "daemonStatus" width = "25%"></td>
          <td width = "15%">&nbsp;</td>
          <td width = "40%"><input type="button" id="btnDaemonControl" class ="buttons" value="" onClick="ControlFPPD();"></td>
        </tr>
        <tr>
          <td> FPP Time: </td>
          <td id = "fppTime" colspan = "3"></td>
        </tr>
      </table>
    </div>
    <hr>
    <div id="playerStatus">
      <table  width= "100%">
        <tr>
          <td width = "20%">Player Status: </td>
          <td id = "txtPlayerStatus" width = "46%"></td>
          <td id = "txtTimePlayed" width = "17%"></td>
          <td id = "txtTimeRemaining" width = "17%"></td>
        </tr>
      </table>
      
      <div id = "startPlaylistControls">
        <table width="100%">
          <tr>
            <td width="20%">Load this playlist:</td>
            <td  width="50%"><select id="selStartPlaylist" name="selStartPlaylist" size="1" onClick="SelectStatusPlaylistEntryRow();PopulateStatusPlaylistEntries(true,'',true);" onChange="PopulateStatusPlaylistEntries(true,'',true);"></select></td>
            <td  width="15%"><input type="checkbox" id="chkRepeat">
              Repeat
              </input></td>
            <td  width="15%">     	
	         <div id="slider"></div> <!-- the Slider -->
           <span class="volume"></span> <!-- Volume -->

            </td>
          </tr>
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
          <td width="15%"> Next Playlist: </td>
          <td id = "txtNextPlaylist" width = "85%"></td>
        </tr>
        <tr>
          <td width="15%"> Time: </td>
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
