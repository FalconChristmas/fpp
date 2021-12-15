<!DOCTYPE html>
<html>
<?php
//ini_set('display_errors', 'On');
error_reporting(E_ALL);
?>

<head>
<?php
require_once('config.php');
require_once('common.php');

include 'common/menuHead.inc';

$commandOptions = "";
$commandsJSON = file_get_contents('http://localhost:32322/commands');
$data = json_decode($commandsJSON, true);
foreach ($data as $cmd) {
    $commandOptions .= "<option value='" . $cmd['name'] . "'>" . $cmd['name'] . "</option>";
}
?>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
<link rel="stylesheet" type="text/css" href="css/jquery.timepicker.css">
<script type="text/javascript" src="js/jquery.timepicker.js"></script>
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

    $('img[src="images/redesign/help-icon.svg"]').tooltip();
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
        data.startDate = '' + formatDate(new Date());
        data.endDate = '' + MAXYEAR + '-12-31';
        data.stopType = 0;
        data.endTimeOffset = 0;
        data.startTimeOffset = 0;
        data.type = 'playlist';
        data.command = '';
        data.args = [];
    }

    if (!data.hasOwnProperty('type') && (data.playlist != '')) {
        data.type = 'playlist';
    }

    var row = AddTableRowFromTemplate('tblScheduleBody');

    if (data.enabled == 1)
        row.find('.schEnable').prop('checked', true);

    row.find('.schType').val(data.type);

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

    if ((data.playlist != '') || (data.type == 'playlist')) {
        // Playlist or .fseq Sequence
        row.find('.schOptionsCommand').hide();

        if (data.hasOwnProperty('sequence') && data.sequence == 1) {
            row.find('.schOptionsPlaylist').hide();
            row.find('.schOptionsSequence').show();
            row.find('.schSequence').val(data.playlist);
            row.find('.schType').val('sequence');

            row.find('.schSequence').tooltip({
                content: function() {
                    return $(this).val().replace(/.fseq$/,'');
                }
            });
        } else {
            row.find('.schOptionsSequence').hide();
            row.find('.schOptionsPlaylist').show();
            row.find('.schPlaylist').val(data.playlist);
            row.find('.schType').val('playlist');

            row.find('.schPlaylist').tooltip({
                content: function() {
                    return $(this).val();
                }
            });
        }

    } else {
        // FPP Command
        row.find('.schOptionsPlaylist').hide();
        row.find('.schOptionsSequence').hide();
        row.find('.schOptionsCommand').show();
        row.find('.schType').val('command');
        row.find('.schRepeat').find('.immediate').hide();

        // Handle old FPP Command entries where repeat was set to 1
        if (data.repeat == 1)
            data.repeat = 0;

        FillInCommandTemplate(row, data);
    }

    if (data.startTime == '25:00:00')
        row.find('.schStartTime').val('SunRise');
    else if (data.startTime == '26:00:00')
        row.find('.schStartTime').val('SunSet');
    else
        row.find('.schStartTime').val(Convert24HToUIFormat(data.startTime));
    row.find('.schStartTime').trigger('change');

    if (data.endTime == '25:00:00')
        row.find('.schEndTime').val('SunRise');
    else if (data.endTime == '26:00:00')
        row.find('.schEndTime').val('SunSet');
    else
        row.find('.schEndTime').val(Convert24HToUIFormat(data.endTime));
    row.find('.schEndTime').trigger('change');
    
    if (data.endTimeOffset != null) row.find('.schEndTimeOffset').val(data.endTimeOffset);
    if (data.startTimeOffset != null) row.find('.schStartTimeOffset').val(data.startTimeOffset);

    row.find('.schRepeat').val(data.repeat);
    row.find('.schStopType').val(data.stopType);

    if (newEntry) {
        SetScheduleInputNames();
    }

    SetupDatePicker(row);
}
function formatDate(date) {
    var d = new Date(date),
        month = '' + (d.getMonth() + 1),
        day = '' + d.getDate(),
        year = d.getFullYear();

    if (month.length < 2) 
        month = '0' + month;
    if (day.length < 2) 
        day = '0' + day;

    return [year, month, day].join('-');
}
 
function getSchedule()
{
    $('#tblScheduleBody').empty();

    $.get('api/schedule', function(data) {
        GetPlaylistArray(false);

        var options = "";
        for(j = 0; j < playListArray.length; j++)
        {
            options +=  "<option value=\"" + playListArray[j].name + "\">" + playListArray[j].name + "</option>";
        }
        $('.fppTableRowTemplate').find('.schPlaylist').html(options);

        GetSequenceArray();
        options = "";
        for(j = 0; j < sequenceArray.length; j++)
        {
            options +=  "<option value=\"" + sequenceArray[j] + ".fseq\">" + sequenceArray[j] + "</option>";
        }
        $('.fppTableRowTemplate').find('.schSequence').html(options);

        var ScheduleSeconds = 0;
        var checkTimes = false;
        if ((!settings.hasOwnProperty('ScheduleSeconds')) ||
            ((settings['ScheduleSeconds'] != '1') &&
             (settings['ScheduleSeconds'] != '0'))) {
            checkTimes = true;
        }

        for (var i = 0; i < data.length; i++) {
            if (checkTimes) {
                if (((data[i].startTime.indexOf(':') != -1) &&
                     (!data[i].startTime.endsWith(':00'))) ||
                    ((data[i].endTime.indexOf(':') != -1) &&
                     (!data[i].endTime.endsWith(':00')))) {
                    ScheduleSeconds = 1;
                    settings['ScheduleSeconds'] = 1;
                }
            }

            AddScheduleEntry(data[i]);
        }

        if (checkTimes)
            SetSetting('ScheduleSeconds', ScheduleSeconds, 0, 0);

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

function SetRowDateTimeFieldVisibility(row) {
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
    
    var re = new RegExp(/[0-9]:[0-9]/);
    var startTime = row.find('.schStartTime').val();
    if (startTime.match(re)) {
        row.find('.startOffset').hide();
    } else {
        row.find('.startOffset').show();
    }
    var endTime = row.find('.schEndTime').val();
    if (endTime.match(re)) {
        row.find('.endOffset').hide();
    } else {
        row.find('.endOffset').show();
    }
}

function SetScheduleInputNames() {
	$('#tblScheduleBody > tr').each(function(i) {
		SetRowDateTimeFieldVisibility($(this));
	});

    var seconds = '';
    if ((settings.hasOwnProperty('ScheduleSeconds')) && (settings['ScheduleSeconds'] == 1))
        seconds = ':s';

    var timeFormat = 'H:i' + seconds;
    if (settings.hasOwnProperty('TimeFormat')) {
        var fmt = settings['TimeFormat'];
        if (fmt == '%I:%M %p') {
            timeFormat = 'h:i' + seconds + ' A';
        } else {
            timeFormat = 'H:i' + seconds;
        }
    }

	$('.time').timepicker({
		'timeFormat': timeFormat,
		'typeaheadHighlight': false,
		'show2400': true,
		'noneOption': [
				{
					'label': 'Dawn',
					'value': 'Dawn'
				},
				{
					'label': 'SunRise',
					'value': 'SunRise'
				},
				{
					'label': 'SunSet',
					'value': 'SunSet'
				},
				{
					'label': 'Dusk',
					'value': 'Dusk'
				}
			]
		});

    SetupDatePicker("#tblScheduleBody > tr > td > input.date");
}

function SetupDatePicker(item)
{
    if(hasTouch && window.innerWidth < 601){
        //use native date picker for small touchscreens (datepicker widget is bad experience on mobile)
        $(item).attr('type','date');
    }else{
        $(item).datepicker({
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
                        text: "Holidays",
                        click: function() {
                            $('.ui-datepicker').hide();
                            $(input).hide();
                            $(input).val('Christmas');
                            $(input).parent().find('.holidays').val('Christmas');
                            $(input).parent().find('.holidays').show();
                        }
                    }).appendTo( buttonPane ).addClass("ui-datepicker-clear ui-state-default ui-priority-primary ui-corner-all");

                    $( "<button>", {
                        text: "Today",
                        click: function() {
                            $(input).datepicker('setDate', new Date());
                            $('.ui-datepicker').hide();
                        }
                    }).appendTo( buttonPane ).addClass("ui-datepicker-clear ui-state-default ui-priority-primary ui-corner-all");
                }, 1 );
            }
        });
    }

}

function ScheduleEntryTypeChanged(item)
{
    var row = $(item).parent().parent();

    if ($(item).val() == 'playlist') {
        // Playlist
        row.find('.schOptionsCommand').hide();
        row.find('.schOptionsSequence').hide();
        row.find('.schOptionsPlaylist').show();
        row.find('.schRepeat').find('.immediate').show();
    } else if ($(item).val() == 'sequence') {
        // Sequence
        row.find('.schOptionsCommand').hide();
        row.find('.schOptionsPlaylist').hide();
        row.find('.schOptionsSequence').show();
        row.find('.schRepeat').find('.immediate').show();
    } else {
        // FPP Command
        row.find('.schOptionsPlaylist').hide();
        row.find('.schOptionsSequence').hide();
        row.find('.schOptionsCommand').show();
        row.find('.schRepeat').find('.immediate').hide();
        row.find('.schRepeat').val(0);
        row.find('.schEndTime').val(row.find('.schStartTime').val());

        if (row.find('.cmdTmplArgs').val() == '')
            row.find('.cmdTmplArgsTable').hide();

        var json = row.find('.cmdTmplJSON').text();
        if (json == '') {
            var data = {};
            data.command = '';
            data.args = [];
            data.multisyncCommand = false;
            data.multisyncHosts = '';
            FillInCommandTemplate(row, data);
        }
    }
}

function TimeChanged(item)
{
    var val = $(item).val();
    if (val.includes(":")) {
        $(item).parent().find('.offset').hide();
    } else {
        $(item).parent().find('.offset').show();
    }
}

function CloneSelectedEntry() {
    $('#tblScheduleBody > tr.selectedEntry').each(function() {
        AddScheduleEntry(GetScheduleEntryRowData($(this)));
    });
    SetScheduleInputNames();
}

function HolidaySelected(item)
{
    if ($(item).val() == 'SpecifyDate') {
        $(item).hide();
        $(item).parent().find('.date').show();
        $(item).parent().find('.date').datepicker('setDate', new Date());
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

function GetScheduleEntryRowData(item) {
    var schType = $(item).find('.schType').val();
    e = {};
    e.enabled = $(item).find('.schEnable').is(':checked') ? 1 : 0;
    e.sequence = 0;

    if (schType == 'playlist') {
        e.playlist = $(item).find('.schPlaylist').val();
    } else if (schType == 'sequence') {
        e.playlist = $(item).find('.schSequence').val();
        e.sequence = 1;
    }

    e.day = parseInt($(item).find('.schDay').val());
    e.startTime = Convert24HFromUIFormat($(item).find('.schStartTime').val());
    e.startTimeOffset = parseInt($(item).find('.schStartTimeOffset').val());
    e.endTime = Convert24HFromUIFormat($(item).find('.schEndTime').val());
    e.endTimeOffset = parseInt($(item).find('.schEndTimeOffset').val());

    e.repeat = parseInt($(item).find('.schRepeat').val());
    e.startDate = $(item).find('.schStartDate').val();
    e.endDate = $(item).find('.schEndDate').val();
    e.stopType = parseInt($(item).find('.schStopType').val());

    if (schType == 'command') {
        var json = $(item).find('.cmdTmplJSON').text();

        if (json == '') {
            var cmd = {};
            cmd.command = command;
            cmd.args = [];
            json = JSON.stringify(cmd);
            $(row).find('.cmdTmplJSON').html(json);
        }

        // Just in case, FPP Commands can't immediately repeat so disable
        if (e.repeat == 1)
            e.repeat = 0;

        var jdata = JSON.parse(json);
        e.playlist = '';
        e.command = $(item).find('.cmdTmplCommand').val();
        e.args = jdata.args;

        if (jdata.hasOwnProperty('multisyncCommand')) {
            e.multisyncCommand = jdata.multisyncCommand;
            if (e.multisyncCommand)
                e.multisyncHosts = jdata.multisyncHosts;
        }
    }

    if (e.day & 0x10000) {
        if ($(item).find('.maskSunday').is(':checked'))
            e.day |= 0x4000;
        if ($(item).find('.maskMonday').is(':checked'))
            e.day |= 0x2000;
        if ($(item).find('.maskTuesday').is(':checked'))
            e.day |= 0x1000;
        if ($(item).find('.maskWednesday').is(':checked'))
            e.day |= 0x0800;
        if ($(item).find('.maskThursday').is(':checked'))
            e.day |= 0x0400;
        if ($(item).find('.maskFriday').is(':checked'))
            e.day |= 0x0200;
        if ($(item).find('.maskSaturday').is(':checked'))
            e.day |= 0x0100;
    }

    return e;
}

function SaveSchedule() {
    var data = [];

    $('#tblScheduleBody > tr').each(function() {
        data.push(GetScheduleEntryRowData($(this)));
    });

    dataStr = JSON.stringify(data, null, 4);

    $.ajax({
        url: 'api/schedule',
        type: 'post',
        dataType: 'json',
        contentType: 'application/json',
        data: dataStr,
        success: function (response) {
            $.jGrowl('Schedule saved',{themeState:'success'});
            $.ajax({
                url: 'api/schedule/reload',
                type: 'post',
                contentType: 'application/json',
                success: function(response) {
                    $.jGrowl('Schedule reloaded',{themeState:'success'});
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

        if ($('#tblScheduleBody > tr.selectedEntry').length > 0) {
            EnableButtonClass('deleteSchButton');
            EnableButtonClass('cloneSchButton');
        } else {
            DisableButtonClass('deleteSchButton');
            DisableButtonClass('cloneSchButton');
        }
    });
});

</script>
<script>
$(document).ready(function(){
        var sortableOptions = {
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
        };
        if(hasTouch){
            $.extend(sortableOptions,{handle:'.rowGrip'});
        }
        $('#tblScheduleBody').sortable(sortableOptions).disableSelection();
});
</script>
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
.center {
	text-align: center;
}

button.ui-datepicker-current {
    display: none;
}
</style>
</head>

<body onload="PopulateCommandListCache(); getSchedule();">
<div id="bodyWrapper">
  <?php	
  $activeParentMenuItem = 'content';
  include 'menu.inc'; ?>
  <div class="mainContainer">
      <h1 class="title">Scheduler</h1>
      <div class="pageContent"> 
          <div>
            <div class="row tablePageHeader">
                <div class="col">
                    <div class="form-actions form-actions-secondary">

                        <div class='smallonly'>
                            <div class="dropdown">
                                <button class="btn btn-outline-primary" type="button" id="schedulerMobileActions" data-toggle="dropdown" aria-haspopup="true" aria-expanded="false">
                                    <i class="fas fa-ellipsis-h"></i>
                                </button>
                                <div class="dropdown-menu" aria-labelledby="schedulerMobileActions">
                                    <button type='button' class='buttons' onClick='PreviewSchedule();' value='View Schedule'><i class="fas fa-fw fa-calendar-alt"></i>Preview</button>
                                    <button class='buttons' type="button" value = "Reload" onClick="ReloadSchedule();"><i class="fas fa-redo"></i> Reload</button>
                                    <button class='buttons' type="button" value="Clear Selection" onClick="$('#tblScheduleBody tr').removeClass('selectedEntry'); DisableButtonClass('deleteSchButton'); DisableButtonClass('cloneSchButton');">Clear Selection</button>
                                    <button class="deleteSchButton disableButtons"  type="button" value="Delete" onClick="DeleteSelectedEntries('tblScheduleBody'); DisableButtonClass('deleteSchButton');">Delete</button>
                                    <button class="cloneSchButton disableButtons" type="button" value="Clone" onClick="CloneSelectedEntry();">Clone</button>
                                </div>
                            </div>
                        </div>
                        
                        <div class='largeonly'><button type='button' class='buttons wideButton' onClick='PreviewSchedule();' value='View Schedule'><i class="fas fa-fw fa-calendar-alt"></i>Preview</button></div>
                        <div class='largeonly'><button class="buttons" type="button" value = "Reload" onClick="ReloadSchedule();"><i class="fas fa-redo"></i> Reload</button></div>
                        <div class='largeonly'><input class="buttons" type="button" value="Clear Selection" onClick="$('#tblScheduleBody tr').removeClass('selectedEntry'); DisableButtonClass('deleteSchButton'); DisableButtonClass('cloneSchButton');"/></div>
                    </div>

                </div>
                <div class="col-auto ml-auto">
                    <div class="form-actions form-actions-primary">

                        <div class='largeonly'><input class="disableButtons deleteSchButton" data-btn-enabled-class="btn-outline-danger" type="button" value="Delete" onClick="DeleteSelectedEntries('tblScheduleBody'); DisableButtonClass('deleteSchButton');"/></div>
                        <div class='largeonly'><input class="disableButtons cloneSchButton" type="button" value="Clone" onClick="CloneSelectedEntry();"/></div>
                        <div><button class="buttons btn-outline-success form-actions-button-primary ml-1" type="button"  onClick="AddScheduleEntry();"><i class="fas fa-plus"></i> Add</button></div>
                        <div><input class="buttons btn-success form-actions-button-primary" type='button' value="Save" onClick='SaveSchedule();' /></div>
                    </div>
                </div>
            </div>
            <div class='fppTableWrapper'>
                <div class='fppTableContents'  role="region" aria-labelledby="tblSchedule" tabindex="0">
                    
                    <table class='fppTableRowTemplate template-tblScheduleBody'>
                        <tr class='rowScheduleDetails'>
                            <td class='center' valign="middle">
                                <div class="rowGrip">
                                    <i class="rowGripIcon fpp-icon-grip"></i>
                                </div>
                            </td>
                            <td class='center' ><input class='schEnable' type='checkbox' /></td>
                            <td><input class='date schStartDate' type='text' size='9'  /></td>
                            <td><input class='date schEndDate' type='text' size='9' /></td>
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
                                    <table  class='dayMaskTable' border=0 cellpadding=0 cellspacing=0>
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
                            <td><input class='time schStartTime' type='text' size='6' onChange='TimeChanged(this);' />
<span class='offset startOffset'><br>+<input class='schStartTimeOffset' type='number' size='4' value='0' min='-120' max='120'>min</span></td>
                            <td><select class='schType' onChange='ScheduleEntryTypeChanged(this);'>
                                <option value='playlist'>Playlist</option>
                                <option value='sequence'>Sequence</option>
                                <option value='command'>Command</option>
                                 </select>
                            </td>
                            <td class='schOptionsPlaylist'>
                                 <select class='schPlaylist' title=''>
                                </select>
                            </td>
                            <td class='schOptionsSequence'>
                                 <select class='schSequence' title=''>
                                </select>
                            </td>
                            <td class='schOptionsPlaylist schOptionsSequence' class='' >
                                <select class='schStopType'>
                                    <option value='0'>Graceful</option>
                                    <option value='2'>Graceful Loop</option>
                                    <option value='1'>Hard Stop</option>
                                </select>
                            </td>
                            <td class='schOptionsCommand' colspan='2'>
                                <select class='cmdTmplCommand' onChange='EditCommandTemplate($(this).parent().parent());'><? echo $commandOptions; ?></select>
                                <img class='cmdTmplTooltipIcon' title='' src='images/redesign/help-icon.svg' width=22 height=22>
                                <input type='button' class='buttons reallySmallButton' value='Edit' onClick='EditCommandTemplate($(this).parent().parent());'>
                                <input type='button' class='buttons smallButton' value='Run Now' onClick='RunCommandJSON($(this).parent().find(".cmdTmplJSON").text());'>
                                <table class='cmdTmplArgsTable'><tr><th class='left'>Args:</th><td><span class='cmdTmplArgs'></span></td></tr></table>
                                <span class='cmdTmplJSON' style='display: none;'></span>
                            </td>
                            <td>
                                <input class='time schEndTime' type='text' size='6' onChange='TimeChanged(this);' />
                                <span class='offset endOffset'><br>+<input class='schEndTimeOffset' type='number' size='4' value='0' min='-120' max='120'>min</span>
                            </td>
                            <td class='' >
                                <select class='schRepeat'>
                                    <option value='0'>None</option>
                                    <option value='1' class='immediate'>Immediate</option>
                                    <option value='500'>5 Min.</option>
                                    <option value='1000'>10 Min.</option>
                                    <option value='1500'>15 Min.</option>
                                    <option value='2000'>20 Min.</option>
                                    <option value='3000'>30 Min.</option>
                                    <option value='6000'>60 Min.</option>
                                </select>
                            </td>
                        </tr>
                    </table>
                    <table id='tblSchedule' class="fppSelectableRowTable">

                        <thead id='tblScheduleHead'>
                            <tr>
                                <th class="tblScheduleHeadGrip"></th>
                                <th class="tblScheduleHeadActive">Active</th>
                                <th class="tblScheduleHeadStartDate">Start Date</th>
                                <th class="tblScheduleHeadEndDate">End Date</th>
                                <th class="tblScheduleHeadDays">Day(s)</th>
                                <th class="tblScheduleHeadStartTime">Start<br>Time</th>
                                <th class="tblScheduleHeadSchType">Schedule<br>Type</th>
                                <th class="tblScheduleHeadPlaylist">Playlist /<br> Command Args</th>
                                <th class="tblScheduleHeadStopType">Stop Type</th>
                                <th class="tblScheduleHeadEndTime">End Time</th>
                                <th class="tblScheduleHeadRepeat">Repeat</th>
                            </tr>
                        </thead>
                        <tbody id='tblScheduleBody'>
                        </tbody>
                    </table>
                </div>
    	    </div>
    	<div>
        <div style="margin-bottom:10px;"><?php PrintSetting('DisableScheduler'); ?></div>
     

            <div class="backdrop">
                <b>Notes</b>:
                <ul style="margin-top:0px;">
                    <li>If playlist times overlap, items higher in the list have priority.</li>
                    <li>Drag/Drop to change order</li>
                    <li>CTRL+Click to select multiple items</li>
                    <li>Odd/Even for Days is used to alternate playlist over 2 days <img style="vertical-align:middle" width=22 height=22 src="images/redesign/help-icon.svg" title="This is not based on the day of the week or month or year. It is odd/even starting at July 15, 2013, the day of the first commit to the FPP code repository. This was done so that it did not have two odd days in a row on the 7th and first days of the week or on months that have 31 days going into the next month, etc."></li>
                </ul>
            </div>

 



            </div>

      </div>
      
  </div>
  <?php	include 'common/footer.inc'; ?>
</div>
</body>
</html>
