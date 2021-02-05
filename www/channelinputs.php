<!DOCTYPE html>
<html>
<head>
<?php
require_once("common.php");
require_once('universeentry.php');
include 'common/menuHead.inc';
?>
<script language="Javascript">

var currentTabTitle = "E1.31 Bridge";

/////////////////////////////////////////////////////////////////////////////
// E1.31 support functions here
$(document).ready(function() {
	$('.default-value').each(function() {
		var default_value = this.value;
		$(this).focus(function() {
			if(this.value == default_value) {
				this.value = '';
				$(this).css('color', '#333');
			}
		});
		$(this).blur(function() {
			if(this.value == '') {
				$(this).css('color', '#999');
				this.value = default_value;
			}
		});
	});

	$.ajax({
	   url: "api/settings/BridgeInputDelayBeforeBlack", 
	   method: "GET",
	   dataType: "json",
	   success: function (data) {
		   let val = 0;
		   if ("value" in data) {
			   val = data.value;
		   }
		   $('#txtBridgeInputDelayBeforeBlack').val(val);
	   }
	});
	$(document).tooltip({
	   content: function() {
	     $('.ui-tooltip').hide();
	     var id=$(this).attr('id');
	     id = id.replace('_img', '_tip');
	     return $('#'+id).html();
	     },
	   hide: {delay: 100 }
	});


	$('#txtUniverseCount').on('focus',function() {
		$(this).select();
	});

	$('#txtBridgeInputDelayBeforeBlack').change(function() {
	   var newValue = $('#txtBridgeInputDelayBeforeBlack').val();
	   $.ajax({
	      url: "api/settings/BridgeInputDelayBeforeBlack",
		  data: newValue, 
	      method: "PUT",
	      dataType: "text",
	      success: function (data) {
		      $.jGrowl("Input Delay Saved");
		      SetRestartFlag(2);
		      CheckRestartRebootFlags();
	      }
	   });

	});

    if (window.innerWidth > 600) {
        $('#tblUniversesBody').sortable({
            start: function (event, ui) {
                start_pos = ui.item.index();
            },
            update: function(event, ui) {
                SetUniverseInputNames();
            },
            beforeStop: function (event, ui) {
                //undo the firefox fix.
                // Not sure what this is, but copied from playlists.php to here
                if (navigator.userAgent.toLowerCase().match(/firefox/) && ui.offset !== undefined) {
                    $(window).unbind('scroll.sortableplaylist');
                    ui.helper.css('margin-top', 0);
                }
            },
            helper: function (e, ui) {
                ui.children().each(function () {
                    $(this).width($(this).width());
                });
                return ui;
            },
            scroll: true
        }).disableSelection();
    }

	$('#tblUniverses').on('mousedown', 'tr', function(event,ui){
		$('#tblUniverses tr').removeClass('selectedEntry');
		$(this).addClass('selectedEntry');
		var items = $('#tblUniverses tr');
		UniverseSelected  = items.index(this);
	});

	$('#frmUniverses').submit(function(event) {
         event.preventDefault();
         var success = validateUniverseData();
         if(success == true) {
             postUniverseJSON(true);
             return false;
         } else {
             DialogError("Save E1.31 Universes", "Validation Failed");
         }
	});

    $(document).tooltip();
});

/////////////////////////////////////////////////////////////////////////////

function handleCIKeypress(e)
{
	if (e.keyCode == 113) {
//		if (currentTabTitle == "Pi Pixel Strings")
//			setPixelStringsStartChannelOnNextRow();
	}
}

$(document).ready(function(){
	$(document).on('keydown', handleCIKeypress);

	// E1.31 initialization
	InitializeUniverses();
	getUniverses('TRUE', 1);

	// Init tabs
	$tabs = $("#tabs").tabs({
		activate: function(e, ui) {
			currentTabTitle = $(ui.newTab).text();
		},
  		cache: true,
		spinner: "",
		fx: {
			opacity: 'toggle',
			height: 'toggle'
		}
	});

	var total = $tabs.find('.ui-tabs-nav li').length;
	if (total > 1)
	{
		var currentLoadingTab = 1;
		$tabs.bind('tabsload', function() {
			currentLoadingTab++;
			if (currentLoadingTab < total)
				$tabs.tabs('load',currentLoadingTab);
			else
				$tabs.unbind('tabsload');
		}).tabs('load',currentLoadingTab);
	}

});

</script>
<!-- FIXME, move this to CSS to standardize the UI -->
<style>
.tblheader{
    background-color:#CCC;
    text-align:center;
    }
.tblheader td {
    border: solid 2px #888888;
    text-align:center;
}
</style>

<title><? echo $pageTitle; ?></title>
</head>
<body>
	<div id="bodyWrapper">
		<?php include 'menu.inc'; ?>
		<br/>

<div id='channelInputManager'>
		<div class='title'>Channel Inputs</div>
		<div id="tabs">
			<ul>
				<li><a href="#tab-e131">E1.31/ArtNet Bridge</a></li>
			</ul>

<!-- --------------------------------------------------------------------- -->

			<div id='tab-e131'>
				<div id='divE131'>
					<fieldset class="fs">
						<legend> E1.31/ArtNet Bridge Mode Universes </legend>
						<div id='divE131Data'>

  <div style="overflow: hidden; padding: 10px;">
	<br>

    <div>
      <form>
        <span id="timeout_tip" class="tooltip" style="display: none">If in Bridge Mode and no DDP, E1.31, ArtNet input received within specified number of seconds, blank the output. (Zero to disable.)</span>
        <table width="100%">
        <tr>
        <td>
	   Inputs Count: <input id="txtUniverseCount" class="default-value" type="text" value="Enter Universe Count" size="3" maxlength="3" /><input id="btnUniverseCount" onclick="SetUniverseCount(1);" type="button"  class="buttons" value="Set" />
        </td><td>
	   Timeout: <input id="txtBridgeInputDelayBeforeBlack" class="default-value" type="number" min="0" max="999" size="3" maxlength="2">
           <img id="timeout_img" title="Blank Timeout" src="images/questionmark.png">
        </td>
        </tr>
        </table>
      </form>
    </div>
    <form id="frmUniverses">
    <input name="command" type="hidden" value="setUniverses" />
    <input name="input" type="hidden" value="1" />
    <table>
    	<tr>
      	<td width = "70 px"><input id="btnSaveUniverses" class="buttons" type="submit" value = "Save" /></td>
      	<td width = "70 px"><input id="btnCloneUniverses" class="buttons" type="button" value = "Clone" onClick="CloneUniverse();" /></td>
      	<td width = "40 px">&nbsp;</td>
      	<td width = "70 px"><input id="btnDeleteUniverses" class="buttons" type="button" value = "Delete" onClick="DeleteUniverse(1);" /></td>
      </tr>
    </table>

    <div class='fppTableWrapper'>
        <div class='fppTableContents'>
            <table id="tblUniverses" class='universeTable fullWidth'>
                <thead id='tblUniversesHead'>
                    <th rowspan=2 title='Input Number'>Input</th>
                        <th rowspan=2 title='Input Enabled/Disabled status'>Active</th>
                        <th rowspan=2 title='User Description'>Description</th>
                        <th rowspan=2 title='Input Type'>Input<br>Type</th>
                        <th colspan=2>FPP Channel</th>
                        <th colspan=3>Universe</th>
                    </tr>
                    <tr>
                        <th title='FPP Start Channel'>Start</th>
                        <th title='FPP End Channel'>End</th>
                        <th title='Universe Number'>#</th>
                        <th title='Universe Count for this controller'>Count</th>
                        <th title='Universe size'>Size</th>
                    </tr>
                </thead>
                <tbody id='tblUniversesBody'>
                </tbody>
            </table>
        </div>
    </div>
		<span style="font-size:12px; font-family:Arial; margin-left:15px;">(Drag entry to reposition) </span>
		</form>
	</div>

						</div>
					</fieldset>
				</div>
			</div>

<!-- --------------------------------------------------------------------- -->

</div>
</div>

<div id='debugOutput'>
</div>

<div id="dialog-panelLayout" title="panelLayout" style="display: none">
  <div id="layoutText">
  </div>
</div>

	<?php	include 'common/footer.inc'; ?>
</div>
</body>
</html>
