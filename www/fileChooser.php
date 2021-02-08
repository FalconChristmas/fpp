<script>
var fileChooserTarget = '';

function SetupFileChooser(dir, target)
{
    $('#fileChooserDir').html(dir);

    fileChooserTarget = target;

    var file = $(fileChooserTarget).val();

    $('#fileChooserFile').html(file);

    $.get('api/files/' + dir, function(data) {
        var options = "";
        for (var i = 0; i < data.length; i++) {
            options += "<option value='" + data[i].name + "'";

            if (file == data[i].name)
                options += " selected";

            options += ">" + data[i].name + "</option><br>";
        }
        $('#fileChooserSelect').html(options);
    });
}

function FileChooserSelectChanged()
{
    var file = $('#fileChooserSelect').val();
    $(fileChooserTarget).val(file);
    $('#fileChooserPopup').dialog("close");
}
</script>

<table border=0 cellpadding=2 cellspacing=3>
<tr><th class='left'>Directory:</th><td id='fileChooserDir'></td></tr>
<tr><th class='left'>Current File:</th><td id='fileChooserFile'></td></tr>
</table>

<select id='fileChooserSelect' onChange='FileChooserSelectChanged();' size='15' style='width: 100%;'></select>

<center><b>Click to select a file</b></center>
