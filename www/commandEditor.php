<script>
var commandEditorTarget = '';
var commandEditorData = {};
var commandEditorCallback = '';
var commandEditorCancelCallback = '';

function CommandEditorSetup(target, data, callback, cancelCallback, args)
{
    commandEditorTarget = target;
    commandEditorData = data;
    commandEditorCallback = callback;
    commandEditorCancelCallback = cancelCallback;

    LoadCommandList($('#editorCommand'));

    $('#btnSaveEditorCommand').val(args.saveButton);
    $('#btnCancelCommandEditor').val(args.cancelButton);

    $('#editorCommand').val(data['command']);
    CommandSelectChanged('editorCommand', 'tblCommandEditor');

    PopulateExistingCommand(data, 'editorCommand', 'tblCommandEditor');
}

function CommandEditorCommandChanged()
{
    CommandSelectChanged('editorCommand', 'tblCommandEditor');
}

function CommandEditorSave()
{
    var data = {};
    data = CommandToJSON('editorCommand', 'tblCommandEditor', data);

    $('#commandEditorPopup').dialog("close");

    if (commandEditorCallback != '') {
        window[commandEditorCallback](commandEditorTarget, data);
    }
}

function CommandEditorCancel()
{
    $('#commandEditorPopup').dialog("close");

    if (commandEditorCancelCallback != '') {
        window[commandEditorCancelCallback](commandEditorTarget);
    }
}
</script>

<table width="100%"  class="tblCommandEditor" id="tblCommandEditor">
    <tr><td>Command:</td><td><select id="editorCommand" onChange="CommandEditorCommandChanged();"></select></td></tr>
</table>
<hr>
<center>
    <input id="btnSaveEditorCommand" type="button" class="buttons wideButton" value="Accept Changes" onClick="CommandEditorSave();">
    <input id="btnCancelCommandEditor" type="button" class="buttons wideButton" value="Cancel Edit" onClick="CommandEditorCancel();">

</center>

