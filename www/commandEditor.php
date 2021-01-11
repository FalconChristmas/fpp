<script>
var commandEditorTarget = '';
var commandEditorData = {};
var commandEditorCallback = '';
var commandEditorCancelCallback = '';

function CommandEditorSetup(target, data, callback, cancelCallback = '')
{
    commandEditorTarget = target;
    commandEditorData = data;
    commandEditorCallback = callback;
    commandEditorCancelCallback = cancelCallback;

    LoadCommandList($('#editorCommand'));

    $('#editorCommand').val(data['command']);
    CommandSelectChanged('editorCommand', 'tblCommandEditor');

    for (var i = 0; i < data['args'].length; i++) {
        var inp =  $("#tblCommandEditor_arg_" + (i+1));
        if (inp.attr('type') == 'checkbox') {
            var checked = false;
            if (data['args'][i] == "true" || data['args'][i] == "1") {
                checked = true;
            }
            inp.prop( "checked", checked);
        } else {
            inp.val(data['args'][i]).trigger('change');
        }
    }
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
    <input id= "btnSaveEditorCommand" type="button" class ="buttons wideButton" value="Accept Changes" onClick="CommandEditorSave();">
    <input id= "btnCancelCommandEditor" type="button" class ="buttons wideButton" value="Cancel Edit" onClick="CommandEditorCancel();">

</center>

