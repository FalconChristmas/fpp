<!DOCTYPE html>
<html>
<head>
<?php
require_once('config.php');
require_once('fppdefines.php');
include 'common/menuHead.inc';
?>
<script>
    var EffectSelectedName = "";
    var EffectSelectedType = "";
    var RunningEffectSelectedId = -1;
    var RunningEffectSelectedName = "";

    $(function() {
    $('#tblEffectLibraryBody').on('mousedown', 'tr', function(event,ui){
          $('#tblEffectLibraryBody tr').removeClass('effectSelectedEntry');
          $(this).addClass('effectSelectedEntry');
          EffectSelectedName  = $(this).find('td:first').text();
          EffectSelectedType  = $(this).find('td:nth-child(2)').text();
          SetButtonState('#btnStartEffect','enable');
    });

    $('#tblRunningEffectsBody').on('mousedown', 'tr', function(event,ui){
          $('#tblRunningEffectsBody tr').removeClass('effectSelectedEntry');
          $(this).addClass('effectSelectedEntry');
          RunningEffectSelectedId = $(this).find('td:first').text();
          RunningEffectSelectedName = $(this).find('td:nth-child(2)').text();
          SetButtonState('#btnStopEffect','enable');
    });
  });

function StartSelectedEffect() {
    var row = $('#tblEffectLibraryBody tr').find('.effectSelectedEntry');
    var startChannel = $('#effectStartChannel').val();
    var loop = 0;
    var background = 0;

    if (startChannel == undefined || startChannel == '')
        startChannel = 0;
    else
        startChannel = parseInt(startChannel) - 1;

    if ($('#loopEffect').is(':checked'))
        loop = 1;

    if ($('#backgroundEffect').is(':checked'))
        background = 1;

    var url = '';
    if (EffectSelectedType == 'ESEQ') {
        url = '/api/command/Effect Start/' + EffectSelectedName + '/' + startChannel + '/' + loop + '/' + background;
    } else {
        url = '/api/command/FSEQ Effect Start/' + EffectSelectedName + '/' + loop + '/' + background;
    }

    $.get(url
        ).done(function() {
            $.jGrowl('Effect Started');
            GetRunningEffects();
        }).fail(function() {
            DialogError('Error Starting Effect', 'Error Starting ' + name + ' Effect');
        });
}

</script>

<title><? echo $pageTitle; ?></title>
</head>
<body onLoad="GetRunningEffects();">
<div id="bodyWrapper">
<?php
  include 'menu.inc';

  function PrintEffectRows()
  {
    $files = Array();

    global $effectDirectory;
    foreach(scandir($effectDirectory) as $seqFile)
    {
      if($seqFile != '.' && $seqFile != '..' && preg_match('/.eseq$/', $seqFile))
      {
        $seqFile = preg_replace('/.eseq$/', '', $seqFile);
        $files[$seqFile] = "ESEQ";
      }
    }

    global $sequenceDirectory;
    foreach(scandir($sequenceDirectory) as $seqFile)
    {
      if($seqFile != '.' && $seqFile != '..' && preg_match('/.fseq$/', $seqFile))
      {
        $seqFile = preg_replace('/.fseq$/', '', $seqFile);
        $files[$seqFile] = "FSEQ";
      }
    }

    ksort($files);

    foreach ($files as $f => $t) {
        echo "<tr id='effect_" . $f . "'><td>" . $f . "</td><td valign='top'>" . $t . "</td></tr>\n";
    }
  }

  ?>
<br/>
<div id="top" class="settings">
  <fieldset class="fs">
	  <legend> Effects </legend>
        <div id= "divEffectLibrary">
              <table>
                <tr><td>Loop Effect:</td><td><input type='checkbox' id='loopEffect'></td>
                    <td width='20px'></td>
                    <td>Run in Background:</td><td><input type='checkbox' id='backgroundEffect'></td>
                    </tr>
                <tr><td colspan='5'>Start Channel Override: <input id="effectStartChannel" class="default-value" type="number" value="" min="1" max="<? echo FPPD_MAX_CHANNELS; ?>" /></td></tr>
                <tr><td><input id= "btnStartEffect" type="button" class ="disableButtons" value="Start Effect" onClick="StartSelectedEffect();"></td>
                </tr>
              </table>
            <div class='fppTableWrapper'>
                <div class='fppTableContents'>
                    <table id="tblEffectLibrary" width="100%" cellpadding=1 cellspacing=0>
                        <thead><tr><th>Effects Library</th><th>Type</th></tr></thead>
                        <tbody id='tblEffectLibraryBody'>
<? PrintEffectRows(); ?>
                        </tbody>
                    </table>
                </div>
            </div>
      </div>
      <div id= "divRunningEffects">
          <input id="btnStopEffect" type="button" class="disableButtons" value="Stop Effect" onclick="StopEffect();" /><br>
          <div class='fppTableWrapper'>
            <div class='fppTableContents'>
              <table id="tblRunningEffects" width="100%" cellpadding=1 cellspacing=0>
                <thead><tr><th width='8%'>ID</th><th>Running Effects</th></tr></thead>
                <tbody id='tblRunningEffectsBody'></tbody>
              </table>
            </div>
          </div>
      </div>
   </fieldset>
</div>
<?php include 'common/footer.inc'; ?>
</div>
</body>
</html>
