<!DOCTYPE html>
<html>
<head>
<?php
require_once('config.php');
include 'common/menuHead.inc';
?>
<script>
    $(function() {
    $('#tblEffectLibrary').on('mousedown', 'tr', function(event,ui){
          $('#tblEffectLibrary tr').removeClass('effectSelectedEntry');
          $(this).addClass('effectSelectedEntry');
          var items = $('#tblEffectLibrary tr');
          EffectNameSelected  = $(this).find('td:first').text();
          SetButtonState('#btnPlayEffect','enable');
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

<title><? echo $pageTitle; ?></title>
</head>
<body onLoad="GetFPPDmode();setInterval(updateFPPStatus,1000);GetRunningEffects();">
<div id="bodyWrapper">
<?php
  include 'menu.inc';

  function PrintEffectRows()
  {
    global $effectDirectory;
    foreach(scandir($effectDirectory) as $seqFile)
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
    <legend>Player Status</legend>
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
  </fieldset>

  <br />
  <fieldset class="fs">
	  <legend> Effects </legend>
      <div id= "divEffectLibrary">
          <div id="effectLibrary">
            <table id="tblEffectListHeader" width="100%">
              <tr class="effectListHeader">
                <td width="100%" class='fppTableHeader'>Effects Library</td>
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
		<tr>
                    <td>Start Channel Override: <input id="effectStartChannel" class="default-value" type="text" value="" size="5" maxlength="5" /></td>
                    <td>Keep last effect state: <input id="keepLastEffectState" class="default-value" type="checkbox"  /></td></tr>
               </tr>
              </table>
            </div>
            <input id= "btnPlayEffect" type="button" class ="disableButtons" value="Play Effect" onClick="PlayEffect($('#effectStartChannel').val(), $('#keepLastEffectState').prop('checked') ? '1' : '0');">
          </div>
      </div>
      <div id= "divRunningEffects">
          <div id="runningEffects">
            <table id="tblEffectListHeader" width="100%">
              <tr class="effectListHeader">
                <td width="5%" class="fppTableHeader">ID</td>
                <td width="95%" class="fppTableHeader">Running Effects</td>
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
      </div>
   </fieldset>
</div>
<?php include 'common/footer.inc'; ?>
</div>
</body>
</html>
