<!DOCTYPE html>
<html>
<head>
<?php
include 'common.php';
include 'common/menuHead.inc';
include 'fppdefines.php';

$commandOptions = "";
$commandsJSON = file_get_contents('http://localhost:32322/commands');
$data = json_decode($commandsJSON, true);
foreach ($data as $cmd) {
    $commandOptions .= "<option value='" . $cmd['name'] . "'>" . $cmd['name'] . "</option>";
}

?>
<script>
allowMultisyncCommands = true;

var TriggerEventSelected = "";
var TriggerEventID = "";
$(function() {
    $('#tblEventEntries').on('mousedown', 'tr', function(event,ui) {
        $('#tblEventEntries tr').removeClass('eventSelectedEntry');
        $(this).addClass('eventSelectedEntry');
        var items = $('#tblEventEntries tr');
        TriggerEventSelected = $(this).find('td:eq(0)').text().replace(' \/ ', '_');
        TriggerEventID = $(this).attr('id');
        SetButtonState('#btnTriggerEvent','enable');
        SetButtonState('#btnEditEvent','enable');
        SetButtonState('#btnDeleteEvent','enable');
    });
});

function AddCommand(cmd = {})
{
    var row = AddTableRowFromTemplate('tblCommandsBody');

    if (!cmd.hasOwnProperty('command')) {
        cmd.command = '';
        cmd.args = [];
        cmd.multisyncCommand = false;
        cmd.multisyncHosts = '';
    }

    FillInCommandTemplate(row, cmd);
}

function GetNamedCommandTemplateData(row)
{
    var cmd = GetCommandTemplateData(row);
    cmd.name = row.find('.cmdTmplName').val();
    cmd.presetSlot = parseInt(row.find('.cmdTmplPresetSlot').val());

    return cmd;
}

function CloneCommand() {
    $('#tblCommandsBody > tr.selectedEntry').each(function() {
        AddCommand(GetNamedCommandTemplateData($(this)));
    });
}

function LoadCommands()
{
    $.get('api/configfile/commandPresets.json', function(data) {
        if (!data.hasOwnProperty('commands'))
            return;

        for (var i = 0; i < data.commands.length; i++) {
            AddCommand(data.commands[i]);
        }
    });
}

function SaveCommands()
{
    var data = {};
    var commands = [];
    $('#tblCommandsBody > tr').each(function() {
        var cmd = GetNamedCommandTemplateData($(this));

        if ((cmd.name != '') && (cmd.command != ''))
            commands.push(cmd);
    });

    data['commands'] = commands;
    var json = JSON.stringify(data, null, 2);

    var result = Post('api/configfile/commandPresets.json', false, json);

    if (!result.hasOwnProperty('Status') || (result['Status'] != 'OK')) {
        alert('Error saving commands!');
    } else {
        $.jGrowl('Commands saved.',{themeState:'success'});
        SetRestartFlag(2);
    }
}

$(document).ready(function() {
    $('#tblCommandsBody').on('mousedown', 'tr', function(event,ui) {
        HandleTableRowMouseClick(event, $(this));

        if ($('#tblCommandsBody > tr.selectedEntry').length > 0) {
            EnableButtonClass('deleteCmdButton');
            EnableButtonClass('cloneCmdButton');
        } else {
            DisableButtonClass('deleteCmdButton');
            DisableButtonClass('cloneCmdButton');
        }
    });

    if (window.innerWidth > 600) {
        $('#tblCommandsBody').sortable({
            start: function (event, ui) {
                start_pos = ui.item.index();
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


    $(document).tooltip();
});
</script>

<title><? echo $pageTitle; ?></title>
</head>
<body onLoad="LoadCommandList($('#newEventCommand')); LoadCommands();">
<div id="bodyWrapper">
<?php include 'menu.inc'; ?>
<div class="container">
	  <h2 class="title">Command Presets</h2>
	  <div class="pageContent">
<div id="commandPresets" class="settings">

		<div>
            <table>
                <tr>
                    <td width='70px'><input class="buttons btn-success" type='button' value="Save" onClick='SaveCommands();' /></td>
                    <td width="70px"><input class="buttons" type="button" value = "Add" onClick="AddCommand();"/></td>
                    <td width="40px">&nbsp;</td>
                    <td width="70px"><input class="buttons disableButtons cloneCmdButton" type="button" value="Clone" onClick="CloneCommand();"/></td>
                    <td width="70px"><input class="buttons disableButtons deleteCmdButton" type="button" value="Delete" onClick="DeleteSelectedEntries('tblCommandsBody'); DisableButtonClass('deleteCmdButton');"/></td>
                    <td width="70px"><input class="buttons" type="button" value="Clear Selection" onClick="$('#tblCommandsBody tr').removeClass('selectedEntry'); DisableButtonClass('deleteCmdButton'); DisableButtonClass('cloneCmdButton');"/></td><td>&nbsp;&nbsp;</td>
                </tr>
            </table>
            <div class='fppTableWrapper'>
                <div class='fppTableContents'>
                    <table class='fppTableRowTemplate template-tblCommandsBody'>
                        <tr>
                            <td><input type='text' size='32' maxlength='64' class='cmdTmplName' list='PresetTriggerNames'></td>
                            <td><select class='cmdTmplCommand' onChange='EditCommandTemplate($(this).parent().parent());'><? echo $commandOptions; ?></select>
                                <input type='button' class='buttons reallySmallButton' value='Edit' onClick='EditCommandTemplate($(this).parent().parent());'>
                                <input type='button' class='buttons smallButton' value='Run Now' onClick='RunCommandJSON($(this).parent().find(".cmdTmplJSON").text());'>
                                <img class='cmdTmplTooltipIcon' title='' src='images/redesign/help-icon.svg' width=22 height=22>
                                <span class='cmdTmplMulticastInfo'></span>
                                <table class='cmdTmplArgsTable'><tr><th class='left'>Args:</th><td><span class='cmdTmplArgs'></span></td></tr></table>
                                <span class='cmdTmplJSON' style='display: none;'></span>
                            </td>
                            <td><input type='number' min='0' max='255' class='cmdTmplPresetSlot'></td>
                        </tr>
                    </table>

                    <table class="fppSelectableRowTable">
                        <thead>
                            <tr>
                                <th>Preset Name</th>
                                <th>FPP Command</th>
                                <th>Preset<br>Slot # <img id='presetSlot_img' title="The Preset Slot number is used along with the Command Preset Control Channel to allow FPP Command Presets to be triggered by sequence or incoming network data.  Set to '0' to disable."  width=22 height=22 src='images/redesign/help-icon.svg'></th>
                            </tr>
                        </thead>
                        <tbody id="tblCommandsBody" width="100%">
                        </tbody>
                    </table>
                </div>
            </div>
		</div>
  
        <div class="backdrop">
            <b>Notes:</b>
            <ul>
                <li>The Command Preset Control Channel is defined on the 'Input/Output' tab of the settings page.</li>
                <li>Predefined Preset Names are automatically triggered within FPP when the action described occurs.</li>
            </ul>

        </div>


  </div>
  </div>
  </div>
  <?php	include 'common/footer.inc'; ?>
</div>

<datalist id='PresetTriggerNames'>
    <option value='FPPD_STARTED'>FPPD Started</option>
    <option value='FPPD_STOPPED'>FPPD Stopped</option>
    <option value='PLAYLIST_STARTED'>Playlist Started</option>
    <option value='PLAYLIST_STOPPED'>Playlist Stopped</option>
    <option value='PLAYLIST_START_TMINUS_001'>Playlist will start in 1 second</option>
    <option value='PLAYLIST_START_TMINUS_002'>Playlist will start in 2 seconds</option>
    <option value='PLAYLIST_START_TMINUS_003'>Playlist will start in 3 seconds</option>
    <option value='PLAYLIST_START_TMINUS_004'>Playlist will start in 4 seconds</option>
    <option value='PLAYLIST_START_TMINUS_005'>Playlist will start in 5 seconds</option>
    <option value='PLAYLIST_START_TMINUS_010'>Playlist will start in 10 seconds</option>
    <option value='PLAYLIST_START_TMINUS_020'>Playlist will start in 20 seconds</option>
    <option value='PLAYLIST_START_TMINUS_030'>Playlist will start in 30 seconds</option>
    <option value='PLAYLIST_START_TMINUS_060'>Playlist will start in 60 seconds</option>
    <option value='PLAYLIST_START_TMINUS_120'>Playlist will start in 2 minutes</option>
    <option value='PLAYLIST_START_TMINUS_300'>Playlist will start in 5 minutes</option>
    <option value='PLAYLIST_START_TMINUS_600'>Playlist will start in 10 minutes</option>
    <option value='SEQUENCE_STARTED'>Sequence Started</option>
    <option value='SEQUENCE_STOPPED'>Sequence Stopped</option>
    <option value='MEDIA_STARTED'>Media Started</option>
    <option value='MEDIA_STOPPED'>Media Stopped</option>
</datalist>

</body>
</html>
