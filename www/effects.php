<!DOCTYPE html>
<html>
<head>
<?php include 'common/menuHead.inc'; ?>
<script type="text/javascript" src="js/fpp.js"></script>
<script src="http://code.jquery.com/jquery-1.9.1.js"></script>
<script src="http://code.jquery.com/ui/1.10.3/jquery-ui.js"></script>
<script>
    $(function() {
    $('#tblEffectLibrary').on('mousedown', 'tr', function(event,ui){
          $('#tblEffectLibrary tr').removeClass('effectSelectedEntry');
          $(this).addClass('effectSelectedEntry');
          var items = $('#tblEffectLibrary tr');
          PlayEffectSelected  = $(this).find('td:first').text();
          SetButtonState('#btnPlayEffect','enable');
          SetButtonState('#btnDeleteEffect','enable');
    });

    $('#tblRunningEffects').on('mousedown', 'tr', function(event,ui){
          $('#tblRunningEffects tr').removeClass('effectSelectedEntry');
          $(this).addClass('effectSelectedEntry');
          var items = $('#tblRunningEffects tr');
          RunningEffectSelected  = $(this).find('td:first').text();
          SetButtonState('#btnStopEffect','enable');
    });
  });
</script>

<title>Falcon PI Player - Effects</title>
</head>
<body onLoad="GetFPPDmode();setInterval(updateFPPStatus,1000);GetRunningEffects();">
<div id="bodyWrapper">
<?php
  include 'menu.inc';

  function PrintEffectRows()
  {
    global $sequenceDirectory;
    foreach(scandir($sequenceDirectory) as $seqFile)
    {
      if($seqFile != '.' && $seqFile != '..' && preg_match('/.eseq$/', $seqFile))
      {
        $seqFile = preg_replace('/.eseq$/', '', $seqFile);
        
        echo "<tr id='effect_" . $seqFile . "'><td width='1000%'>" . $seqFile . "</td></tr>\n";
      }
    }
  }

  ?>
<br/>
<div id="top" class="settings">
  <fieldset>
    <legend>Program Control</legend>
    <div id="daemonControl">
      <table width= "100%">
        <tr>
          <td class='controlHeader'> FPPD Mode: </td>
          <td><div id='textFPPDmode'>Player Mode</div>
        </tr>
        <tr>
          <td class='controlHeader'> FPPD Status: </td>
          <td id="daemonStatus"></td>
        </tr>
        <tr>
          <td class='controlHeader'> FPP Time: </td>
          <td id="fppTime"></td>
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
      <table  width= "100%">
        <tr>
          <td class='controlHeader'>Player Status: </td>
          <td id="txtPlayerStatus"></td>
        </tr>
      </table>
      <table  width= "100%">
        <tr>
          <td id="txtTimePlayed"></td>
          <td id="txtTimeRemaining"></td>
        </tr>
      </table>
    </div>
    <div id="playerControls" style="margin-top:5px">
      <input id= "btnPlay" type="button"  class ="buttons"value="Start a Playlist" onClick="location.href = '/index.php';">
      <input id= "btnStopGracefully" type="button"  class ="buttons"value="Stop Gracefully" onClick="StopGracefully();">
      <input id= "btnStopNow" type="button" class ="buttons" value="Stop Now" onClick="StopNow();">
    </div>
  </fieldset>

  <br />

      <div id= "divEffectLibrary">
        <fieldset class="fs">
          <legend> Effects Library </legend>
          <div id="effectLibrary">
            <table id="tblEffectListHeader" width="100%">
              <tr class="effectListHeader">
                <td width="100%">Effect</td>
              </tr>
            </table>
            <div>
              <table id="tblEffectLibrary"   width="100%">
<? PrintEffectRows(); ?>
              </table>
            </div>
          </div>
          <div class='right'>
            <div>
              <table width="100%">
                <tr><td>Start Channel Override: <input id="effectStartChannel" class="default-value" type="text" value="1" size="5" maxlength="5" /></td></tr>
              </table>
            </div>
            <input id= "btnPlayEffect" type="button" class ="disableButtons" value="Play Effect" onClick="PlayEffect($('#effectStartChannel').val());">
<!--
            <input id="btnDeleteEffect" type="button" class="disableButtons" value="Delete Effect" onclick="DeleteEffect();" />
-->
          </div>
        </fieldset>
      <br />
      </div>
      <div id= "divRunningEffects">
        <fieldset  class="fs">
          <legend> Running Effects </legend>
          <div id="runningEffects">
            <table id="tblEffectListHeader" width="100%">
              <tr class="effectListHeader">
                <td width="5%">ID</td>
                <td width="95%">Effect</td>
              </tr>
            </table>
            <div>
              <table id="tblRunningEffects"   width="100%">
              </table>
            </div>
          </div>
          <div class='right'>
            <input id="btnStopEffect" type="button" class="disableButtons" value="Stop Effect" onclick="StopEffect();" />
          </div>
        </fieldset>
      </div>
</div>
<?php include 'common/footer.inc'; ?>
</body>
</html>
