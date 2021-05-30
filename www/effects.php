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
    new $.Zebra_Pin($('#divRunningEffects'), {
        contained: true,
        top_spacing: $('.header').outerHeight() + 10
    });
    $('#tblEffectLibraryBody').on('click', '.buttons', function(event,ui){
          $('#tblEffectLibraryBody tr').removeClass('effectSelectedEntry');
          var $selectedEntry = $(this).parent().parent();
          $selectedEntry.addClass('effectSelectedEntry');
          EffectSelectedName  = $selectedEntry.find('td:first').text();
          EffectSelectedType  = $selectedEntry.find('td:nth-child(2)').text();
          $('#divEffectStartModal').fppDialog({
            title:'Start Effect '+$selectedEntry.find('td:first').text(),
            buttons:{
              'Start Effect': {
                click:function(){
                  StartSelectedEffect();
                  $('#divEffectStartModal').fppDialog('close');
                },
                class:'btn-success'
              }
              
            }
          });
          //SetButtonState('#btnStartEffect','enable');
    });

    $('#tblRunningEffectsBody').on('click', '.buttons', function(event,ui){
          $('#tblRunningEffectsBody tr').removeClass('effectSelectedEntry');
          var $selectedEntry = $(this).parent().parent();
          $selectedEntry.addClass('effectSelectedEntry');
          RunningEffectSelectedId = $selectedEntry.find('td:first').text();
          RunningEffectSelectedName = $selectedEntry.find('td:nth-child(2)').text();
          StopEffect();
          console.log('stopping')
          //SetButtonState('#btnStopEffect','enable');
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
        startChannel = parseInt(startChannel);

    if ($('#loopEffect').is(':checked'))
        loop = 1;

    if ($('#backgroundEffect').is(':checked'))
        background = 1;

    var url = '';
    if (EffectSelectedType == 'eseq') {
        url = 'api/command/Effect Start/' + EffectSelectedName + '/' + startChannel + '/' + loop + '/' + background;
    } else {
        url = 'api/command/FSEQ Effect Start/' + EffectSelectedName + '/' + loop + '/' + background;
    }

    $.get(url
        ).done(function() {
            $.jGrowl('Effect Started',{themeState:'success'});
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
  $activeParentMenuItem = 'status';
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
        $files[$seqFile] = "eseq";
      }
    }

    global $sequenceDirectory;
    foreach(scandir($sequenceDirectory) as $seqFile)
    {
      if($seqFile != '.' && $seqFile != '..' && preg_match('/.fseq$/', $seqFile))
      {
        $seqFile = preg_replace('/.fseq$/', '', $seqFile);
        $files[$seqFile] = "fseq";
      }
    }

    ksort($files);

    foreach ($files as $f => $t) {
        echo "<tr id='effect_" . $f . "'><td><img src='images/redesign/icon-" . $t . ".svg' alt=" . $t . " class='icon-effect-type'/>" . $f . "</td><td>" . $t . "</td><td><button class='buttons btn-success'>Start</button></td></tr>\n";
    }
  }

  ?>

<div class="mainContainer">
<h1 class="title">Effects</h1>
  <div class="pageContent">

  
              <div class="row">
                <div class="col">
                <h2>Effects Library</h2>
                  <div id= "divEffectLibrary">
                  
                 
                      <div id="divEffectStartModal" class="hidden">
                        <div class="row">
                          <div class="col-auto">
                            <div class="form-inline">
                              <div class="form-group">
                                <div>Loop Effect:</div>
                                <div class="p-1"><input type='checkbox' id='loopEffect'></div>
                              </div>
                            </div>
                          </div>
                          <div class="col-auto">
                            <div class="form-inline">
                              <div class="form-group">
                                <div>Run in Background:</div>
                                <div class="p-1"><input type='checkbox' id='backgroundEffect'></div>
                              </div>
                            </div>
                          </div>
                        </div>
                        <div>
                          <div class="form-inline">
                            <div class="form-group">
                              <div>Start Channel Override:</div>
                              <div class="p-1"><input id="effectStartChannel" class="default-value" type="number" value="" min="1" max="<? echo FPPD_MAX_CHANNELS; ?>" /></div>
                            </div>
                          </div>
                        </div>
                      </div>
                 



                      <div class='fppTableWrapper'>
                          <div class='fppTableContents'>
                              <table id="tblEffectLibrary" width="100%" cellpadding=1 cellspacing=0 class="fppActionTable">
                                  <thead><tr><th>Effects Library</th><th>Type</th><th width='15%'></th></tr></thead>
                                  <tbody id='tblEffectLibraryBody'>
                      <? PrintEffectRows(); ?>
                                  </tbody>
                              </table>
                          </div>
                      </div>
                  </div>

                </div>
                <div class="col pin-parent">
                  <div id= "divRunningEffects" class="backdrop-disabled">
                    <h2>Running Effects</h2>
                    <!-- <input id="btnStopEffect" type="button" class="disableButtons" value="Stop Effect" onclick="StopEffect();" /><br> -->
                    <div class='fppTableWrapper'>
                      <div class='fppTableContents'>
                        <table id="tblRunningEffects" class="fppActionTable fppActionTable-success" width="100%" cellpadding=1 cellspacing=0>
                          <thead><tr><th>ID</th><th>Running Effects</th><th></th></tr></thead>
                          <tbody id='tblRunningEffectsBody'></tbody>
                        </table>
                        <div class="tblRunningEffectsPlaceholder">
                          There are currently no effects running
                        </div>
                      </div>
                    </div>
                            </div>
                </div>
              </div>
  

  </div>
</div>

<?php include 'common/footer.inc'; ?>
</div>
</body>
</html>
