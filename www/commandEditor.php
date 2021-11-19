<script>
var commandEditorTarget = '';
var commandEditorData = {};
var commandEditorCallback = '';
var commandEditorCancelCallback = '';
var commandEditorPresets = '';

function CommandEditorSetup(target, data, callback, cancelCallback, args)
{
    commandEditorTarget = target;
    commandEditorData = data;
    commandEditorCallback = callback;
    commandEditorCancelCallback = cancelCallback;

    $('.presetSelect').hide();
    if (args.showPresetSelect) {
        $.get('api/configfile/commandPresets.json'
        ).done(function (data) {
            if (data.hasOwnProperty('commands')) {
                commandEditorPresets = data;
                var names = [];
                for (var i = 0; i < data.commands.length; i++) {
                    names.push(data.commands[i].name);
                }
                names.sort();

                var options = "<option value=''>---- Select a Command Preset ----</option>";
                for (var i = 0; i < names.length; i++) {
                    options += "<option value='" + names[i] + "'>" + names[i] + "</option>";
                }
                $('#presetNames').html(options);
                $('.presetSelect').show();
            }
        });
    }

    LoadCommandList($('#editorCommand'));

    $('#btnSaveEditorCommand').val(args.saveButton);
    $('#btnCancelCommandEditor').val(args.cancelButton);

    $('#editorCommand').val(data['command']);
    CommandSelectChanged('editorCommand', 'tblCommandEditor');

    PopulateExistingCommand(data, 'editorCommand', 'tblCommandEditor');
}

function CommandEditorPresetSelectChanged()
{
    if (typeof commandEditorPresets != 'string') {
        var presetName = $('#presetNames').val();
        for (var i = 0; i < commandEditorPresets.commands.length; i++) {
            if (commandEditorPresets.commands[i].name == presetName) {
                PopulateExistingCommand(commandEditorPresets.commands[i], 'editorCommand', 'tblCommandEditor');
                $('#btnSaveDirectEditorCommand').prop('disabled', false);
            }
        }
    }
}

function CommandEditorCommandChanged()
{
    CommandSelectChanged('editorCommand', 'tblCommandEditor');
}

function CommandEditorRunNow()
{
    var data = {};
    data = CommandToJSON('editorCommand', 'tblCommandEditor', data);

    RunCommand(data);
}

function CommandEditorSave()
{
    var data = {};
    data = CommandToJSON('editorCommand', 'tblCommandEditor', data);

    $('#commandEditorPopup').fppDialog("close");

    if (commandEditorCallback != '') {
        window[commandEditorCallback](commandEditorTarget, data);
    }
}

function CommandEditorSaveDirect()
{
    var cmd = {};
    name = $('#presetNames').val();
    cmd = CommandToJSON('editorCommand', 'tblCommandEditor', cmd);

    if (name == '') {
        $('#btnSaveDirectEditorCommand').prop('disabled', true);
        $.jGrowl("No preset selected", { themeState: 'success' });
        return;
    }

    $.get('api/configfile/commandPresets.json'
    ).done(function (data) {
        if (data.hasOwnProperty('commands')) {
            commandEditorPresets = data;
        } else {
            data = {};
            data.commands = [];
        }

        var found = 0;
        for (var i = 0; i < data.commands.length; i++) {
            if (data.commands[i].name == name) {
                found = 1;
                data.commands[i].command = cmd.command;
                data.commands[i].multisyncCommand = cmd.multisyncCommand;
                data.commands[i].args = cmd.args;
            }
        }

        if (!found) {
            $.jGrowl("Preset '" + name + "' not found", { themeState: 'success' });
            return;
        }

        var json = JSON.stringify(data, null, 2);
        var result = Post('api/configfile/commandPresets.json', false, json);

        if (!result.hasOwnProperty('Status') || (result['Status'] != 'OK')) {
            alert('Error saving commands!');
        } else {
            $.jGrowl('Commands saved.',{themeState:'success'});
            SetRestartFlag(2);
        }
    });
}

function CommandEditorCancel()
{
    $('#commandEditorPopup').fppDialog("close");

    if (commandEditorCancelCallback != '') {
        window[commandEditorCancelCallback](commandEditorTarget);
    }
}
</script>

<table width="100%"  class="tblCommandEditor" id="tblCommandEditor">
    <tr class='presetSelect'><td></td><td>Select an existing Command Preset or setup a new command</td></tr>
    <tr class='presetSelect'><td>Preset:</td><td><select id='presetNames' onChange='CommandEditorPresetSelectChanged();'></select></td></tr>
    <tr><td>Command:</td><td><select id="editorCommand" onChange="CommandEditorCommandChanged();"></select></td></tr>
</table>
<hr>
<center>
    <input id="btnSaveEditorCommand" type="button" class="buttons wideButton" value="Accept Changes" onClick="CommandEditorSave();">
    <input id="btnRunEditorCommand" type="button" class="buttons wideButton" value="Run Now" onClick="CommandEditorRunNow();">
    <input id="btnSaveDirectEditorCommand" type="button" class="presetSelect buttons wideButton" value="Save Preset" onClick="CommandEditorSaveDirect();" disabled='disabled'>
    <input id="btnCancelCommandEditor" type="button" class="buttons wideButton" value="Cancel Edit" onClick="CommandEditorCancel();">

</center>

