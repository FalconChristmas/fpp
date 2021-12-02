<!DOCTYPE html>
<html>
<head>
<?php
require_once("common.php");
require_once('universeentry.php');
require_once('fppdefines.php');
include 'common/menuHead.inc';
?>
<script language="Javascript">

var currentTabTitle = "E1.31/DDP Input";

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
		      $.jGrowl("Input Delay Saved",{themeState:'success'});
		      SetRestartFlag(2);
		      CheckRestartRebootFlags();
	      }
	   });

	});

    if (window.innerWidth > 600) {

    }
	var sortableOptions = {
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
	}
	if(hasTouch){
		$.extend(sortableOptions,{handle:'.rowGrip'});
	}
	$('#tblUniversesBody').sortable(sortableOptions).disableSelection();
	
	$('#tblUniverses').on('mousedown', 'tr', function(event,ui){
		$('#tblUniverses tr').removeClass('selectedEntry');
		$(this).addClass('selectedEntry');
		var items = $('#tblUniverses tbody tr');
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
		<?php 
		$activeParentMenuItem = 'input-output';
		include 'menu.inc'; ?>
  <div class="mainContainer">
	<h1 class="title">Channel Inputs</h1>
	<div class="pageContent">
		
		<div id='channelInputManager'>



		<ul class="nav nav-pills pageContent-tabs" id="channelInputTabs" role="tablist">
              <li class="nav-item">
                <a class="nav-link active" id="tab-e131-tab" tabType='UDP' data-toggle="pill" href="#tab-e131" role="tab" aria-controls="tab-e131" aria-selected="true">
				E1.31/ArtNet/DDP Inputs
				</a>
              </li>
		</ul>

		<!-- --------------------------------------------------------------------- -->
		

		
		<div class="tab-content" id="channelOutputTabsContent">
			<div class="tab-pane fade show active" id="tab-e131" role="tabpanel" aria-labelledby="tab-e131-tab">


						<div id='divE131'>


		    <form id="frmUniverses">
                <div class="row tablePageHeader">
                    <div class="col-md"><h2>E1.31 / ArtNet / DDP Inputs</h2></div>
                    <div class="col-md-auto ml-lg-auto">
                        <div class="form-actions">
                                <input name="input" type="hidden" value="0" />
                                <input id="btnDeleteUniverses" class="buttons btn-outline-danger" type="button" value = "Delete" onClick="DeleteUniverse(1);" />
                                <input id="btnCloneUniverses" class="buttons" type="button" value = "Clone" onClick="CloneUniverse();" />
                                <input id="btnSaveUniverses" class="buttons btn-success ml-1" type="submit" value = "Save" />
                        </div>
                    </div>
                </div>
                <div class="backdrop tableOptionsForm">
                    <div class="row">
                        <div class="col-md-auto">
                            <div class="backdrop-dark form-inline enableCheckboxWrapper">
                            <div><b>Enable Input:</b></div>
                                <div> <input type="checkbox" id="E131Enabled"/></div>
                            </div>
                        </div>
                            <div class="col-md-auto form-inline">
                                <div><b>Timeout:</b></div>
                                <div ><input id="bridgeTimeoutMS" type="number" min="0" max="9999" size="4" maxlength="4">
                                        <img id="timeout_img" title="Blank Timeout" src="images/redesign/help-icon.svg" width=22 height=22>
                                        <span id="timeout_tip" class="tooltip" style="display: none">Timeout for input channel data (in MS).  If no new data is received for this time, the input data is cleared.</span></div>
                            </div>
							<div class="col-md-auto form-inline">
								<div><b>Inputs Count: </b></div>
								<div ><input id="txtUniverseCount" class="default-value" type="text" value="Enter Universe Count" size="3" maxlength="3" /></div>
								<div><input id="btnUniverseCount" onclick="SetUniverseCount(1);" type="button"  class="buttons" value="Set" /></div>
							</div>
                    </div>
                </div>

		
		    <div class='fppTableWrapper'>
		        <div class='fppTableContents' role="region" aria-labelledby="tblUniverses" tabindex="0">
		            <table id="tblUniverses" class='universeTable fullWidth fppSelectableRowTable'>
		                <thead id='tblUniversesHead'>
								<th class="tblScheduleHeadGrip"></th>
				        		<th title='Input Number'>Input</th>
		                        <th title='Input Enabled/Disabled status'>Active</th>
		                        <th title='User Description'>Description</th>
		                        <th title='Input Type'>Input Type</th>
		                        <th title='FPP Start Channel'>FPP Channel Start</th>
		                        <th title='FPP End Channel'>FPP Channel End</th>
		                        <th title='Universe Number'>Universe #</th>
		                        <th title='Universe Count for this controller'>Universe Count</th>
		                        <th title='Universe size'>Universe Size</th>
		                    </tr>
		                </thead>
		                <tbody id='tblUniversesBody'>
		                </tbody>
		            </table>
		        </div>
		    </div>
				<small class="text-muted">(Drag entry to reposition) </small>
				</form>
			</div>
		
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
	</div>
</div>

	<?php	include 'common/footer.inc'; ?>
</div>
</body>
</html>
