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

			<h2> E1.31 / ArtNet / DDP / KiNet</h2>
			<div id='divE131Data'>

				<div>
		
						<b>Enable E1.31 / ArtNet / DDP / KiNet Output:</b> <input type="checkbox" id="E131Enabled"/>
						<span id='outputOffWarning' style='display: none;' class='warning'>
						- WARNING, universe outputs are active, but the primary channel output is disabled.  No data will be sent.
						</span>

<br>
					Source Interface: <select id="selE131interfaces"><? PopulateInterfaces(); ?></select>
    <span
    <? if ($uiLevel < 1) { ?>
        style="display:none;"
    <? } ?>
    ><br>Multi-Threaded:&nbsp;<input id="E131ThreadedOutput" type="checkbox" checked /><b>*</b></span>
					<br><br>

					<div>
						<form>
							Outputs Count: <input id="txtUniverseCount" class="default-value" type="text" value="Enter Universe Count" size="3" maxlength="3" /><input id="btnUniverseCount" onclick="SetUniverseCount(0);" type="button"  class="buttons" value="Set" />
						</form>
					</div>
					<form id="frmUniverses">
						<input name="command" type="hidden" value="setUniverses" />
						<input name="input" type="hidden" value="0" />
						<table>
							<tr>
								<td width = "70 px"><input id="btnSaveUniverses" class="buttons btn-success" type="submit" value = "Save" /></td>
								<td width = "70 px"><input id="btnCloneUniverses" class="buttons" type="button" value = "Clone" onClick="CloneUniverse();" /></td>
								<td width = "40 px">&nbsp;</td>
								<td width = "70 px"><input id="btnDeleteUniverses" class="buttons" type="button" value = "Delete" onClick="DeleteUniverse(0);" /></td>
							</tr>
						</table>

                        <div class='fppTableWrapper'>
                            <div class='fppTableContents'>
						<table id="tblUniverses" class='universeTable fullWidth fppSelectableRowTable'>
							<thead id='tblUniversesHead'>
                                <tr>
                                    <th title='Output Number'>Output</th>
                                    <th title='Output Enabled/Disabled status'>Active</th>
                                    <th title='User Description'>Description</th>
                                    <th title='Output Type'>Output Type</th>
                                    <th title='Unicast IP Address'>Unicast Address</th>
                                    <th title='Monitor controller'>Monitor</th>
                                    <th title='Suppress Duplicate network packets'>DeDup</th>
                                    <th title='Test ping controller'>Ping</th>
                                    <th title='FPP Start Channel'>FPP Start Channel</th>
                                    <th title='FPP End Channel'>FPP End Channel</th>
                                    <th title='Universe Number'>Universe #</th>
                                    <th title='Universe Count for this controller'>Universe Count</th>
                                    <th title='Universe size'>Universe Size</th>
                                    <th title='Universe Priority'>Universe Priority</th>
                                </tr>
							</thead>
							<tbody id='tblUniversesBody'>
							</tbody>
						</table>
                            </div>
                        </div>
						<span>(Drag entry to reposition) </span>
					</form>
				</div>
			</div>

	</div>
</div>
