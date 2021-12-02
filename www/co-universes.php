<?
require_once('fppdefines.php');
?>
<script>
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

	$('#txtUniverseCount').on('focus',function() {
		$(this).select();
	});

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


	$('#tblUniversesBody').on('mousedown', 'tr', function(event,ui){
		$('#tblUniversesBody tr').removeClass('selectedEntry');
		$(this).addClass('selectedEntry');
		var items = $('#tblUniversesBody tr');
		UniverseSelected  = items.index(this);
	});

	$('#frmUniverses').submit(function(event) {
			 event.preventDefault();
			 var success = validateUniverseData();
			 if(success == true)
			 {
                postUniverseJSON(false);
				return false;
			 }
			 else
			 {
			   DialogError("Save E1.31 Universes", "Validation Failed");
			 }
	});

	InitializeUniverses();
	getUniverses('TRUE', 0);
});

<?
function PopulateInterfaces()
{
	global $settings;

	$interfaces = explode("\n",trim(shell_exec("/sbin/ifconfig | cut -f1 -d' ' | grep -v ^$ | grep -v lo | grep -v usb | grep -v SoftAp | grep -v 'can.'")));
	$ifaceE131 = "";
    // legacy setting for the E131Interface
	if (isset($settings['E131interface'])) {
		$ifaceE131 = $settings['E131interface'];
	}
	$found = 0;
	if ($ifaceE131 == "") {
		$ifaceE131 = "eth0";
	}
	foreach ($interfaces as $iface)
	{
		$iface = preg_replace("/:$/", "", $iface);
		$ifaceChecked = "";
		if ($iface == $ifaceE131) {
			$ifaceChecked = " selected";
			$found = 1;
		}
		echo "<option value='" . $iface . "'" . $ifaceChecked . ">" . $iface . "</option>";
	}
	if (!$found && ($ifaceE131 != "")) {
		echo "<option value='" . $ifaceE131 . "' selected>" . $ifaceE131 . "</option>";
	}
}
?>

</script>

<div id='tab-e131'>
	<div id='divE131'>


			<div id='divE131Data'>

				<div>
		
						

					<form id="frmUniverses">



						<div class="row tablePageHeader">
							<div class="col-md"><h2>E1.31 / ArtNet / DDP / KiNet</h2></div>
							<div class="col-md-auto ml-lg-auto">
								<div class="form-actions">
										<input name="input" type="hidden" value="0" />
										<input id="btnDeleteUniverses" class="buttons btn-outline-danger" type="button" value = "Delete" onClick="DeleteUniverse(0);" />
										<input id="btnCloneUniverses" class="buttons" type="button" value = "Clone" onClick="CloneUniverse();" />
							
										
										<input id="btnSaveUniverses" class="buttons btn-success ml-1" type="submit" value = "Save" />

								</div>
							</div>
						</div>
						<div id='outputOffWarning' style='display: none;' class='callout callout-warning'>
							<b>WARNING </b>universe outputs are active, but the primary channel output is disabled.  No data will be sent. Check "Enable Output" below if if you wish to enable.
						</div>
						<div class="backdrop tableOptionsForm">
							<div class="row">
								<div class="col-md-auto">
									<div class="backdrop-dark form-inline enableCheckboxWrapper">

									<div><b>Enable Output:</b></div>
										<div> <input type="checkbox" id="E131Enabled"/></div>
										
									</div>
								</div>
								<div class="col-md-auto form-inline">
									<div><b>Source Interface:</b></div>
									<div ><select id="selE131interfaces"><? PopulateInterfaces(); ?></select></div>

								</div>
								<div class="col-md-auto form-inline" <? if ($uiLevel < 1) { ?> style="display:none;" <? } ?>>
									<div><i class="fas fa-fw fa-graduation-cap ui-level-1"></i><b> Multi-Threaded:</b></div>
									<div>
										<input id="E131ThreadedOutput" type="checkbox" checked />
									</div>
								</div>
								<div class="col-md-auto form-inline">
									<div><b>Outputs Count: </b></div>
									<div ><input id="txtUniverseCount" class="default-value" type="text" value="Enter Universe Count" size="3" maxlength="3" /></div>
									<div><input id="btnUniverseCount" onclick="SetUniverseCount(0);" type="button"  class="buttons" value="Set" /></div>
								</div>
							</div>
						</div>



                        <div class='fppTableWrapper'>
                            <div class='fppTableContents' class='universeTable fullWidth' role="region" aria-labelledby="tblUniverses" tabindex="0">
						<table id="tblUniverses" class="fppSelectableRowTable">
							<thead id='tblUniversesHead'>
                                <tr>
									<th class="tblScheduleHeadGrip"></th>
                                    <th aria-label='Output Number'>Output</th>
                                    <th aria-label='Output Enabled/Disabled status'>Active</th>
                                    <th aria-label='User Description'>Description</th>
                                    <th aria-label='Output Type'>Output <br>Type</th>
                                    <th aria-label='Unicast IP Address'>Unicast <br>Address</th>
   
                                    <th aria-label='FPP Start Channel'>FPP Start <br>Channel</th>
                                    <th aria-label='FPP End Channel'>FPP End <br>Channel</th>
                                    <th aria-label='Universe Number'>Universe #</th>
                                    <th aria-label='Universe Count for this controller'>Universe <br>Count</th>
                                    <th aria-label='Universe size'>Universe <br>Size</th>
                                    <th aria-label='Universe Priority'>Universe <br>Priority</th>
									<th aria-label='Monitor controller'>Monitor</th>
                                    <th aria-label='Suppress Duplicate network packets'>DeDup</th>
                                    <th aria-label='Test ping controller'>Ping</th>
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
</div>
