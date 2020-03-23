<!DOCTYPE html>
<html>
<?php
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

    $(document).tooltip();
});
</script>
<script>
function AddScheduleEntry(data = {}) {
    var newEntry = 0;
    if (!data.hasOwnProperty('enabled')) {
        newEntry = 1;
        data.enabled = 1;
        data.day = 7;
        data.playlist = '';
        data.startTime = '00:00:00';
        data.endTime = '24:00:00';
        data.repeat = 1;
        data.startDate = '' + MINYEAR + '-01-01';
        data.endDate = '' + MAXYEAR + '-12-31';
        data.stopType = 0;
    }

    var row = AddTableRowFromTemplate('tblScheduleBody');

    row.find('.rowID').val($('#tblScheduleBody > tr').length + 1);

    if (data.enabled == 1)
        row.find('.schEnable').prop('checked', true);

    row.find('.schStartDate').val(data.startDate);
    row.find('.schStartDate').after(HolidaySelect(data.startDate, "holStartDate"));
    row.find('.schEndDate').val(data.endDate);
    row.find('.schEndDate').after(HolidaySelect(data.endDate, "holEndDate"));

    if (data.day > 0x10000) {
        if (data.day & 0x04000)
            row.find('.maskSunday').prop('checked', true);
        if (data.day & 0x02000)
            row.find('.maskMonday').prop('checked', true);
        if (data.day & 0x01000)
            row.find('.maskTuesday').prop('checked', true);
        if (data.day & 0x00800)
            row.find('.maskWednesday').prop('checked', true);
        if (data.day & 0x00400)
            row.find('.maskThursday').prop('checked', true);
        if (data.day & 0x00200)
            row.find('.maskFriday').prop('checked', true);
        if (data.day & 0x00100)
            row.find('.maskSaturday').prop('checked', true);

        row.find('.schDay').val(65536).trigger('change');
    } else {
        row.find('.schDay').val(data.day).trigger('change');
    }

    if (data.playlist != '')
        row.find('.schPlaylist').val(data.playlist);

    if (data.startTime == '25:00:00')
        row.find('.schStartTime').val('SunRise');
    else if (data.startTime == '26:00:00')
        row.find('.schStartTime').val('SunSet');
    else
        row.find('.schStartTime').val(data.startTime);

    if (data.endTime == '25:00:00')
        row.find('.schEndTime').val('SunRise');
    else if (data.endTime == '26:00:00')
        row.find('.schEndTime').val('SunSet');
    else
        row.find('.schEndTime').val(data.endTime);

    if (data.repeat == 1)
        row.find('.schRepeat').prop('checked', true);

    row.find('.schStopType').val(data.stopType);

    if (newEntry) {
        SetScheduleInputNames();
    }
}

function getSchedule()
{
    $('#tblScheduleBody').empty();

    $.get('/api/schedule').success(function(data) {
        GetPlaylistArray(false);

        var options = "";
        for(j = 0; j < playListArray.length; j++)
        {
            options +=  "<option value=\"" + playListArray[j] + "\">" + playListArray[j] + "</option>";
        }
        $('.fppTableRowTemplate').find('.schPlaylist').html(options);

        for (var i = 0; i < data.length; i++) {
            AddScheduleEntry(data[i]);
        }

        SetScheduleInputNames();
    }).fail(function() {
    });
}

function ReloadSchedule()
{
    reloadPage();
}


function ScheduleDaysSelectChanged(item)
{
    if ($(item).find('option:selected').text() == 'Day Mask')
        $(item).parent().parent().find('.dayMask').show();
    else
        $(item).parent().parent().find('.dayMask').hide();
}

function SetScheduleRowInputNames(row, id) {
	row.find('span.rowID').html((id + 1).toString());

    var startDate = row.find('.schStartDate').val().replace(/[-0-9]/g,'');
    if (startDate != '') {
        row.find('.schStartDate').hide();
        row.find('.holStartDate').show();
    }
    var endDate = row.find('.schEndDate').val().replace(/[-0-9]/g,'');
    if (endDate != '') {
        row.find('.schEndDate').hide();
        row.find('.holEndDate').show();
    }
}

function SetScheduleInputNames() {
	var id = 0;
	$('#tblScheduleBody > tr').each(function() {
		SetScheduleRowInputNames($(this), id);
		id += 1;
	});

	$('.time').timepicker({
		'timeFormat': 'H:i:s',
		'typeaheadHighlight': false,
		'show2400': true,
		'noneOption': [
				{
					'label': 'SunRise',
					'value': 'SunRise'
				},
				{
					'label': 'SunSet',
					'value': 'SunSet'
				}
			]
		});

	$('.date').datepicker({
		'changeMonth': true,
		'changeYear': true,
		'dateFormat': 'yy-mm-dd',
		'minDate': new Date(MINYEAR-1, 1 - 1, 1),
		'maxDate': new Date(MAXYEAR, 12 - 1, 31),
		'showButtonPanel': true,
		'selectOtherMonths': true,
		'showOtherMonths': true,
		'yearRange': "" + MINYEAR +":" + MAXYEAR,
		'autoclose': true,
		'beforeShow': function( input ) {
            setTimeout(function() {
                var buttonPane = $( input )
                    .datepicker( "widget" )
                    .find( ".ui-datepicker-buttonpane" );

                $( "<button>", {
                    text: "Select from Holidays",
                    click: function() {
                        $('.ui-datepicker').hide();
                        $(input).hide();
                        $(input).val('Christmas');
                        $(input).parent().find('.holidays').val('Christmas');
                        $(input).parent().find('.holidays').show();
                    }
                }).appendTo( buttonPane ).addClass("ui-datepicker-clear ui-state-default ui-priority-primary ui-corner-all");
            }, 1 );
        }
    });
}

function HolidaySelected(item)
{
    if ($(item).val() == 'SpecifyDate') {
        $(item).hide();
        $(item).parent().find('.date').show();
    } else {
        $(item).parent().find('.date').val($(item).val());
        $(item).parent().find('.date').hide();
    }
}

function HolidaySelect(userKey, classToAdd)
{
    var result = "<select class='holidays " + classToAdd + "' onChange='HolidaySelected(this);' style='display: none;'>";
    result += "<option value='SpecifyDate'>Specify Date</option>";

    for (var i in settings['locale']['holidays']) {
        var holiday = settings['locale']['holidays'][i];

        result += "<option value='" + holiday['shortName'] + "'";

        if (holiday['shortName'] == userKey)
            result += " selected";

        result += ">" + holiday['name'] + "</option>";
    }

    result += "</select>";
    return result;
}

function SaveSchedule() {
    var data = [];

    $('#tblScheduleBody > tr').each(function() {
        e = {};
        e.enabled = $(this).find('.schEnable').is(':checked') ? 1 : 0;
        e.playlist = $(this).find('.schPlaylist').val();
        e.day = parseInt($(this).find('.schDay').val());
        e.startTime = $(this).find('.schStartTime').val();
        e.endTime = $(this).find('.schEndTime').val();
        e.repeat = $(this).find('.schRepeat').is(':checked') ? 1 : 0;
        e.startDate = $(this).find('.schStartDate').val();
        e.endDate = $(this).find('.schEndDate').val();
        e.stopType = parseInt($(this).find('.schStopType').val());

        if (e.day & 0x10000) {
            if ($(this).find('.maskSunday').is(':checked'))
                e.day |= 0x4000;
            if ($(this).find('.maskMonday').is(':checked'))
                e.day |= 0x2000;
            if ($(this).find('.maskTuesday').is(':checked'))
                e.day |= 0x1000;
            if ($(this).find('.maskWednesday').is(':checked'))
                e.day |= 0x0800;
            if ($(this).find('.maskThursday').is(':checked'))
                e.day |= 0x0400;
            if ($(this).find('.maskFriday').is(':checked'))
                e.day |= 0x0200;
            if ($(this).find('.maskSaturday').is(':checked'))
                e.day |= 0x0100;
        }

        if (e.startTime == 'SunRise')
            e.startTime = '25:00:00';
        else if (e.startTime == 'SunSet')
            e.startTime = '26:00:00';

        if (e.endTime == 'SunRise')
            e.endTime = '25:00:00';
        else if (e.endTime == 'SunSet')
            e.endTime = '26:00:00';

        data.push(e);
    });

    dataStr = JSON.stringify(data, null, 4);

    $.ajax({
        url: '/api/schedule',
        type: 'post',
        dataType: 'json',
        contentType: 'application/json',
        data: dataStr,
        success: function (response) {
            $.jGrowl('Schedule saved');
            $.ajax({
                url: '/api/schedule/reload',
                type: 'post',
                contentType: 'application/json',
                success: function(response) {
                    $.jGrowl('Schedule reloaded');
                }
            });
        },
        error: function() {
            DialogError('Error saving', 'Error saving schedule');
        }
    });
}

$(function() {
    $('#tblScheduleBody').on('mousedown', 'tr', function(event,ui) {
        HandleTableRowMouseClick(event, $(this));

        if ($('#tblScheduleBody > tr.selectedEntry').length > 0)
            EnableButtonClass('deleteSchButton');
        else
            DisableButtonClass('deleteSchButton');
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

<body onload="getSchedule();">
<div id="bodyWrapper">
  <?php	include 'menu.inc'; ?>
  <div style="width:100%;margin:0 auto;"> <br />
    <fieldset style="padding: 10px; border: 2px solid #000;">
      <legend>Schedule</legend>
      <div style="overflow: hidden; padding: 10px;">
        <table>
          <tr>
            <td width='70px'><input class="buttons" type='button' value="Save" onClick='SaveSchedule();' /></td>
            <td width = "70 px"><input class="buttons" type="button" value = "Add" onClick="AddScheduleEntry();"/></td>
            <td width = "40 px">&nbsp;</td>
            <td width = "70 px"><input class="buttons disableButtons deleteSchButton" type="button" value="Delete" onClick="DeleteSelectedEntries('tblScheduleBody'); DisableButtonClass('deleteSchButton');"/></td>
            <td width = "70 px"><input class="buttons" type="button" value="Clear Selection" onClick="$('#tblScheduleBody tr').removeClass('selectedEntry'); DisableButtonClass('deleteSchButton');"/></td>
            <td width = "40 px">&nbsp;</td>
            <td width = "70 px"><input class="buttons" type="button" value = "Reload" onClick="ReloadSchedule();"/></td>
          </tr>
        </table>
        <div class='fppTableWrapper'>
            <div class='fppTableContents'>
                <table class='fppTableRowTemplate template-tblScheduleBody'>
                    <tr class='rowScheduleDetails'>
                        <td class='center'><span class='rowID'></span></td>
                        <td class='center' ><input class='schEnable' type='checkbox' /></td>
                        <td><input class='date center schStartDate' type='text' size='10'  /></td><td>
                            <input class='date center schEndDate' type='text' size='10' /></td>
                        <td><select class='schDay' onChange='ScheduleDaysSelectChanged(this);'>
                                <option value='7'>Everyday</option>
                                <option value='0'>Sunday</option>
                                <option value='1'>Monday</option>
                                <option value='2'>Tuesday</option>
                                <option value='3'>Wednesday</option>
                                <option value='4'>Thursday</option>
                                <option value='5'>Friday</option>
                                <option value='6'>Saturday</option>
                                <option value='8'>Mon-Fri</option>
                                <option value='9'>Sat/Sun</option>
                                <option value='10'>Mon/Wed/Fri</option>
                                <option value='11'>Tues/Thurs</option>
                                <option value='12'>Sun-Thurs</option>
                                <option value='13'>Fri/Sat</option>
                                <option value='14'>Odd</option>
                                <option value='15'>Even</option>
                                <option value='65536'>Day Mask</option>
                                </select>
                            <br>
                            <span class='dayMask'>
                                <table border=0 cellpadding=0 cellspacing=0>
                                    <tr><th>S</th>
                                        <th>M</th>
                                        <th>T</th>
                                        <th>W</th>
                                        <th>T</th>
                                        <th>F</th>
                                        <th>S</th>
                                        </tr>
                                    <tr><td><input class='maskSunday' type='checkbox' /></td>
                                        <td><input class='maskMonday' type='checkbox' /></td>
                                        <td><input class='maskTuesday' type='checkbox' /></td>
                                        <td><input class='maskWednesday' type='checkbox' /></td>
                                        <td><input class='maskThursday' type='checkbox' /></td>
                                        <td><input class='maskFriday' type='checkbox' /></td>
                                        <td><input class='maskSaturday' type='checkbox' /></td>
                                    </tr>
                                </table>
                            </span>
                        </td>
                        <td><select class='schPlaylist'>
                            </select></td>
                        <td><input class='time center schStartTime' type='text' size='8' /></td>
                        <td><input class='time center schEndTime' type='text' size='8' /></td>
                        <td class='center' ><input class='schRepeat' type='checkbox' /></td>
                        <td class='center' >
                            <select class='schStopType'>
                                <option value='0'>Graceful</option>
                                <option value='2'>Graceful Loop</option>
                                <option value='1'>Hard Stop</option>
                            </select>
                        </td>
                    </tr>
                </table>
                <table id='tblSchedule'>
                    <thead id='tblScheduleHead'>
                        <tr>
                            <th rowspan='2' title='Schedule Entry Number'>Line<br>#</th>
                            <th rowspan='2' title='Schedule enabled/disabled'>Act<br>ive</th>
                            <th colspan='3'>Run Dates</th>
                            <th rowspan='2' title='Scheduled Playlist'>Playlist</th>
                            <th colspan='2'>Run Times</th>
                            <th rowspan='2' title='Repeat playlist'>Rpt</th>
                            <th rowspan='2' title='Playlist Stop Type'>Stop Type</th>
                        </tr>
                        <tr>
                            <th title='Start Date'>Start</th>
                            <th title='End Date'>End</th>
                            <th title='Day(s) of the week'>Day(s)</th>
                            <th title='Start Time'>Start</th>
                            <th title='End Time'>End</th>
                        </tr>
                    </thead>
                    <tbody id='tblScheduleBody'>
                    </tbody>
                </table>
            </div>
        </div>
        <font size=-1><b>CTRL+Click to select multiple items</b></font>
    </fieldset>
  </div>
  <?php	include 'common/footer.inc'; ?>
</div>
</body>
</html>
