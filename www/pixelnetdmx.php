<?php
require_once('config.php');
require_once('universeentry.php');
$a = session_id();

if(empty($a))
{
	session_start();
}
$_SESSION['session_id'] = session_id();
//ini_set('display_errors', 'On');
error_reporting(E_ALL);

$hwModel = "FPDv1";
$hwFWVer = "1.10";

?>

<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
<?php	include 'common/menuHead.inc'; ?>
<script language="Javascript">
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
});
</script>

<script>
    $(function() {
		$('#tblOutputs').on('mousedown', 'tr', function(event,ui){
					$('#tblOutputs tr').removeClass('selectedEntry');
          $(this).addClass('selectedEntry');
					var items = $('#tblOutputs tr');
					PixelnetDMXoutputSelected  = items.index(this);

		});
	});
</script>

<script>
$(document).ready(function(){
	$('#frmPixelnetDMX').submit(function(event) {
			 event.preventDefault();
			 //var success = validateUniverseData();
			 //if(success == true)
			 //{
				 dataString = $("#frmPixelnetDMX").serializeArray();
				 $.ajax({
						type: "post",
						url: "fppxml.php",
						dataType:"text",
						data: dataString,
						success: function (response) {
								getPixelnetDMXoutputs('TRUE');
						}
				})
				return false;
			 //}
	});
});</script>

<title>Pixelnet/DMX Outputs</title>
<style>
.clear {
	clear: both;
}


.items {
	width: 40%;
	background: rgb#FFF;
	float: right;
	margin: 0, auto;
}
.selectedEntry {
	background: #888;
}
.pl_title {
	font-size: larger;
}
h4, h3 {
	padding: 0;
	margin: 0;
}

.tblheader{
	background-color:#CCC;
	text-align:center;
	}
tr.rowUniverseDetails
{
	border:thin solid;
	border-color:#CCC;
}

tr.rowUniverseDetails td
{
	padding:1px 5px;
}

#tblUniverses
{
	border:thin;
	border-color:#333;
	border-collapse: collapse;
}

a:active {
    color: none;
}
a:visited{
  color:blue;
}
</style>
</head>

<body onload="getPixelnetDMXoutputs('TRUE');">
<div id="bodyWrapper">
<?php	include 'menu.inc'; ?>

<div style="width:1100px;margin:0 auto;">
<br \>
  <fieldset style="padding: 10px; border: 2px solid #000;">
<legend>Pixelnet/DMX Outputs</legend>
  <div style="overflow: hidden; padding: 10px;">
    <div id = "playList" style="float: left;"> </div>
    <div width: 400px; padding: 10px; background: #f9f9f9;
; border: 1px solid #ccc;">
    </div>
		<br/>
    <form id="frmPixelnetDMX">
    <input name="command" type="hidden" value="SaveHardwareConfig" />
    <input name='model' type='hidden' value='<? echo $hwModel; ?>' />
		<input name='firmware' type='hidden' value='<? echo $hwFWVer; ?>' />

    <table>
    	<tr>
      	<td width = "70 px"><input id="btnSaveOutputs" class="buttons" type="submit" value = "Save" /></td>
      	<td width = "70 px"><input id="btnCloneOutputs" class="buttons" type="button" value = "Clone" onClick="ClonePixelnetDMXoutput();"/></td>
      	<td width = "40 px">&nbsp;</td>
      	<td width = "70 px"><input id="btnReload" class="buttons" type="button" value = "Reload" onClick="ReloadPixelnetDMX();"/></td>
      </tr>
    </table>
		<table id="tblOutputs">
    </table>
		</form>

  </fieldset>



</div>
  <?php	include 'common/footer.inc'; ?>
</div>
</body>
</html>
