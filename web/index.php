<?php
$page_num = isset($_GET['example']) ? $_GET['example']:'';
$pages = array(
	array('classic_theme.php','Classic Theme'),
	array('basic.php', 'Basic set up'),
	array('drag_drop.php', 'Upload by Drag & Drop'),
	array('file_ext.php', 'Restrict file extension'),
	array('auto.php', 'Auto start'),
	array('api_calls.php', 'API calls'),
	array('events.php', 'Events and Options'),
	array('form.php', 'Form Integration and File rename'),
	array('form_validation.php', 'Advanced Form Integration'),
	array('fallback.php', 'No JS fallback')
);

?>

<!DOCTYPE html>
<html>
<head>
<?php	include 'common/menuHead.inc'; ?>
<script type="text/javascript" src="js/fpp.js"></script>
<title>Falcon PI Player - FPP</title>
</head>
<body onLoad="GetFPPstatus();StatusPopulatePlaylists();setInterval(updateFPPStatus,1000);">
<div id="bodyWrapper">
  <?php
	include 'menu.inc';
  ?>
  <br/>
  <div id = "programControl">
  <fieldset>
    <legend>[ Program Control ]</legend>
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
          <td>&nbsp;</td>
          <td>&nbsp;</td>
        </tr>
      </table>
    </div>
    <br/>
    <div id="playerStatus">
      <table  width= "100%">
        <tr>
          <td width = "20%">Player Status: </td>
          <td id = "txtPlayerStatus" width = "80%"></td>
        </tr>
       </table>
        <div id="currentPlaylistDetails">
       <table  width= "100%">
          <tr>
            <td width="20%">Playlist: </td>
            <td width="25%" id = "currentPlaylist"></td>
            <td width="55%" id = "playingInfo"></td>
          </tr>
          <tr>
            <td>Song File: </td>
            <td id = "songFile"></td>
            <td>&nbsp;</td>
            <td>&nbsp;</td>
          </tr>
          <tr>
            <td>Sequence File: </td>
            <td id = "seqFile"></td>
            <td>&nbsp;</td>
            <td>&nbsp;</td>
          </tr>
          <tr>
            <td>Time: </td>
            <td id = "SecondsPlayed"></td>
            <td id = "timeRemaining"></td>
            <td>&nbsp;</td>
          </tr>
      </table>
        </div>
      <div id = "startPlaylistControls">
        <table width="100%">
          <tr>
            <td width="20%">Start this playlist</td>
            <td  width="25%"><select id="selStartPlaylist" name="selStartPlaylist" size="1"></select></td>
            <td  width="15%"><input type="checkbox">Repeat</input></td>
            <td  width="40%"><input type="button"  class ="buttons" value="Start Now" onClick="StartPlaylistNow();"></td>
          </tr>
        </table>
      </div>
      <div id="playerControls">
        <table width="100%">
          <tr>
            <td width="20%">Control:	</td>
            <td  width="25%"><input type="button"  class ="buttons"value="Stop Gracefully" onClick="StopGracefully();"></td>
            <td  width="55%"><input type="button" class ="buttons" value="Stop Now" onClick="StopNow();"></td>
              
          </tr>
        </table>
      </div>
    </div>
    <br/>
    <div id="nextPlaylist">
      <table  width="100%">
        <tr>
          <td width="20%"> Next Playlist: </td>
          <td id = "txtNextPlaylist" width = "80%"></td>
        </tr>
        <tr>
          <td> Time: </td>
          <td id = "nextPlaylistTime"></td>
        </tr>
      </table>
    </div>

    <div id="rebootShutdown">
      <table  width="100%">
        <tr>
          <td width="20%"><input name="btnReboot" onClick="RebootPi();" type="button" class = "buttons" value="Reboot Pi"></td>
          <td id = "txtNextPlaylist" width = "80%"><input name="btnShutdown" type="button" onClick="ShutdownPi();" class = "buttons" value="Shutdown Pi"></td>
        </tr>
      </table>
    </div>

  </fieldset>
  <?php	include 'common/footer.inc'; ?>
</div>
</body>
</html>
