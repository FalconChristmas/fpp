<script>
$(function() {
	$('#tblOutputs').on('mousedown', 'tr', function(event,ui){
		$('#tblOutputs tr').removeClass('selectedEntry');
		$(this).addClass('selectedEntry');
		var items = $('#tblOutputs tr');
		PixelnetDMXoutputSelected  = items.index(this);
	});
});

$(document).ready(function(){
	getPixelnetDMXoutputs('TRUE');

	$('#frmPixelnetDMX').submit(function(event) {
		 event.preventDefault();
		 dataString = $("#frmPixelnetDMX").serializeArray();
		 $.ajax({
			type: "post",
			url: "fppxml.php",
			dataType:"text",
			data: dataString
		}).done(function() {
			getPixelnetDMXoutputs('TRUE');
			$.jGrowl("FPD Config Saved");
			SetRestartFlag(2);
		}).fail(function() {
			DialogError("Save FPD Config", "Save Failed");
		});
		return false;
	});
});
</script>

<div id='tab-fpd'>
	<div id='divFPD'>
		<fieldset class="fs">
			<legend> Falcon Pixelnet/DMX (FPD) </legend>
			<div id='divFPDData'>
				<div style="overflow: hidden; padding: 10px;">
					<b>Enable FPD Output:</b> <? PrintSettingCheckbox("FPD Output", "FPDEnabled", 2, 0, "1", "0"); ?><br>
					<b>FPD Start Channel Offset:</b> <? PrintSettingTextSaved("FPDStartChannelOffset", 2, 0, 6, 6); ?> <font size=-1>(default is 0)</font><br>
					<br>
					<form id="frmPixelnetDMX">
						<input name="command" id="command"  type="hidden" value="saveHardwareConfig" />
						<input name='model' id="model" type='hidden' value='FPDv1' />
						<input name='firmware' id="firmware" type='hidden' value='1.10' />
						<table>
							<tr>
								<td width = "70 px"><input id="btnSaveOutputs" class="buttons" type="submit" value = "Save" /></td>
								<td width = "70 px"><input id="btnCloneOutputs" class="buttons" type="button" value = "Clone" onClick="ClonePixelnetDMXoutput();" /></td>
								<td width = "40 px">&nbsp;</td>
							</tr>
						</table>
                        <div class='fppTableWrapper'>
                            <div class='fppTableContents'>
                                <table id="tblOutputs">
                                </table>
                            </div>
                        </div>
					</form>
				</div>
			</div>
		</fieldset>
	</div>
</div>

