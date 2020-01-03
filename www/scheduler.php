<!DOCTYPE html>
<html>
<?php
$a = session_id();

if(empty($a))
{
	session_start();
}
$_SESSION['session_id'] = session_id();
//ini_set('display_errors', 'On');
error_reporting(E_ALL);
?>

<head>
<?php
require_once('config.php');
include 'common/menuHead.inc';
?>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
<link rel="stylesheet" type="text/css" href="css/jquery.timepicker.css">
<script type="text/javascript" src="js/jquery.timepicker.min.js"></script>
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
		$('#tblScheduleBody').on('mousedown', 'tr', function(event,ui){
			$('#tblScheduleBody tr').removeClass('selectedEntry');
			$(this).addClass('selectedEntry');
			var items = $('#tblScheduleBody tr');
			ScheduleEntrySelected  = items.index(this);
		});
	});
</script>
<script>
$(document).ready(function(){
	$('#tblScheduleBody').sortable({
		start: function (event, ui) {
			start_pos = ui.item.index();
		},
		update: function(event, ui) {
			SetScheduleInputNames();
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

	$('#frmSchedule').submit(function(event) {
			 event.preventDefault();
			 var success =true;
			 if(success == true)
			 {
				 dataString = $("#frmSchedule").serializeArray();
				 $.ajax({
						type: "post",
						url: "fppxml.php",
						dataType:"text",
						data: dataString,
						success: function (response) {
								getSchedule();
						}
				})
				return false;
			 }
	});
});</script>
<title><? echo $pageTitle; ?></title>
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
	background: #CCC;
}
.pl_title {
	font-size: larger;
}
h4, h3 {
	padding: 0;
	margin: 0;
}
.tblheader {
	background-color: #CCC;
	text-align: center;
}
tr.rowScheduleDetails {
	border: thin solid;
	border-color: #CCC;
}
tr.rowScheduleDetails td {
	padding: 1px 5px;
}
#tblSchedule {
	width: 100%;
	border: thin;
	border-color: #333;
	border-collapse: collapse;
}
.dayMaskTable {
	padding: 2px 0px 0px 0px;
}
tr.rowScheduleDetails td.dayMaskTD {
    padding: 2px 2px;
}
tr.rowScheduleDetails input.dayMaskCheckbox {
    margin: 0px 0px 0px 0px;
}
tr.rowScheduleDetails select.selPlaylist {
    width: 250px;
}
tr.rowScheduleDetails select.selPlaylist option {
    overflow: hidden;
    white-space: no-wrap;
    text-overflow: ellipsis;
}
a:active {
	color: none;
}
a:visited {
	color: blue;
}
.center {
	text-align: center;
}
</style>
</head>

<body onload="getSchedule('TRUE');">
<div id="bodyWrapper">
  <?php	include 'menu.inc'; ?>
  <div style="width:1100px;margin:0 auto;"> <br />
    <fieldset style="padding: 10px; border: 2px solid #000;">
      <legend>Schedule</legend>
      <div style="overflow: hidden; padding: 10px;">
      <form id="frmSchedule">
        <input name="command" type="hidden" value="saveSchedule" />
        <table>
          <tr>
            <td width = "70 px"><input id="btnSaveSchedule" class="buttons" type="submit" value = "Save" /></td>
            <td width = "70 px"><input id="btnAddScheduleEntry" class="buttons" type="button" value = "Add" onClick="AddScheduleEntry();"/></td>
            <td width = "40 px">&nbsp;</td>
            <td width = "70 px"><input id="btnDeleteScheduleEntry" class="buttons" type="button" value = "Delete" onClick="DeleteScheduleEntry();"/></td>
            <td width = "70 px"><input id="btnReload" class="buttons" type="button" value = "Reload" onClick="ReloadSchedule();"/></td>
          </tr>
        </table>
        <table id="tblSchedule">
            <thead id='tblScheduleHead'>
            </thead>
            <tbody id='tblScheduleBody'>
            </tbody>
        </table>
      </form>
    </fieldset>
  </div>
  <?php	include 'common/footer.inc'; ?>
</div>
</body>
</html>
