<?php
require_once('universeentry.php');
$a = session_id();

if(empty($a))
{
	session_start();
}
$_SESSION['session_id'] = session_id();
//ini_set('display_errors', 'On');
error_reporting(E_ALL);

function PrintUniverses()
{
	echo "<tr><td><input ></td></tr>";	
}
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
<script type="text/javascript">
        $(function() {
            $('#txtUniverseCount').on('focus',function() {
                $(this).select();
            });
        });
    </script>
    
<script>
    $(function() {
		$('#tblUniverses').on('mousedown', 'tr', function(event,ui){
					$('#tblUniverses tr').removeClass('selectedEntry');
          $(this).addClass('selectedEntry');
					var items = $('#tblUniverses tr');
					UniverseSelected  = items.index(this);
					
		});
	});
</script>

<script>
$(document).ready(function(){
	$('#frmUniverses').submit(function(event) {
			 event.preventDefault();
			 var success = validateUniverseData();
			 if(success == true)
			 {
				 dataString = $("#frmUniverses").serializeArray();
				 $.ajax({
						type: "post",
						url: "fppxml.php",
						dataType:"text",
						data: dataString,
						success: function (response) {
								getUniverses();
						}
				})
				return false;
			 }
	});
});</script>
    
<title>Universe Manager</title>
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

<body onload="InitializeUniverses();getUniverses('TRUE');">
<div id="bodyWrapper">
<?php	include 'menu.inc'; ?>

<div style="width:1100px;margin:0 auto;">
 <br />
  <fieldset style="padding: 10px; border: 2px solid #000;">
<legend>Universes</legend>
  <div style="overflow: hidden; padding: 10px;">
    <div id = "playList" style="float: left;"> </div>
    <div width: 400px; padding: 10px; background: #f9f9f9;
; border: 1px solid #ccc;">
      <form>
        Universe Count: <input id="txtUniverseCount" class="default-value" type="text" value="Enter Universe Count" size="30" maxlength="32" /><input id="btnUniverseCount" onclick="SetUniverseCount();" type="button"  class="buttons" value="Set" />
      </form>
    </div>
		<br/>
    <form id="frmUniverses">
    <input name="command" type="hidden" value="saveUniverses" />
    <table>
    	<tr>
      	<td width = "70 px"><input id="btnSaveUniverses" class="buttons" type="submit" value = "Save" /></td>
      	<td width = "70 px"><input id="btnCloneUniverses" class="buttons" type="button" value = "Clone" onClick="CloneUniverse();"/></td>
      	<td width = "40 px">&nbsp;</td>
      	<td width = "70 px"><input id="btnDeleteUniverses" class="buttons" type="button" value = "Delete" onClick="DeleteUniverse();"/></td>
      	<td width = "70 px"><input id="btnReload" class="buttons" type="button" value = "Reload" onClick="ReloadUniverses();"/></td>
      </tr>
    </table>
    
		<table id="tblUniverses">
    </table>
		</form>

  </fieldset>

 
  
</div>
  <?php	include 'common/footer.inc'; ?>
</div>
</body>
</html>
