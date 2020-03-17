<?
$skipJSsettings = 1;
require_once('common.php');
?>

<script>
function MaskChanged(cbox)
{
    var newValue = false;
    if ($(cbox).is(':checked', true))
        newValue = true;

    var id = $(cbox).attr('id');
    if (id == "mask_all")
    {
        $('#LogMask input').prop('checked', false);
        // Do this so we don't have to put class='mask_most,mask_all' on every 'most' items
        $('#LogMask input.mask_most').prop('checked', newValue);
        $('#LogMask input.mask_all').prop('checked', newValue);
        $('#mask_most').prop('checked', false);
    }
    else if (id == "mask_most")
    {
        $('#LogMask input').prop('checked', false);
        $('#LogMask input.mask_most').prop('checked', newValue);
    }
    else
    {
        $('#mask_most').prop('checked', false);
        $('#mask_all').prop('checked', false);
    }

    var newValue = "";
    $('#LogMask input').each(function() {
        if ($(this).is(':checked', true)) {
            if (newValue != "") {
                newValue += ",";
            }
            newValue += $(this).attr('id').replace(/(.*,)?mask_/,"");
        }
    });

    $.get("fppjson.php?command=setSetting&key=LogMask&value="
        + newValue).fail(function() { alert("Error saving new mask") });

    $.ajax({ url: 'fppjson.php?command=setSetting&key=LogMask&value=' + newValue, 
        async: false,
        dataType: 'json',
        success: function(data) {
            $.jGrowl("Log Mask Saved.");
        },
        failure: function(data) {
            DialogError("Save Log Mask", "Error Saving new Log Mask.");
        }
    });
}

</script>

<?
$logLevels = Array();
$logLevels['Warn'] = 'warn';
$logLevels['Info'] = 'info';
$logLevels['Debug'] = 'debug';
$logLevels['Excessive'] = 'excess';
?>

<table class='settingsTable'>
<?
PrintSetting('LogLevel');
?>
    <tr>
        <td valign='top'>Log Mask:</td>
        <td><table border=0 cellpadding=2 cellspacing=5 id='LogMask'>
                <tr><td valign=top>
                        <input type='checkbox' id='mask_all' class='mask_all' onChange='MaskChanged(this);'>ALL<br>
                        <br>
                        <input type='checkbox' id='mask_channeldata' class='mask_all' onChange='MaskChanged(this);'>Channel Data<br>
                        <input type='checkbox' id='mask_channelout' class='mask_most' onChange='MaskChanged(this);'>Channel Outputs<br>
                        <input type='checkbox' id='mask_command' class='mask_most' onChange='MaskChanged(this);'>Commands<br>
                        <input type='checkbox' id='mask_control' class='mask_most' onChange='MaskChanged(this);'>Control Interface<br>
                        <input type='checkbox' id='mask_e131bridge' class='mask_most' onChange='MaskChanged(this);'>E1.31 Bridge<br>
                        <input type='checkbox' id='mask_effect' class='mask_most' onChange='MaskChanged(this);'>Effects<br>
                        <input type='checkbox' id='mask_event' class='mask_most' onChange='MaskChanged(this);'>Events<br>
                        <input type='checkbox' id='mask_general' class='mask_most' onChange='MaskChanged(this);'>General<br>
                    </td>
                    <td width='10px'></td>
                    <td valign=top>
                        <input type='checkbox' id='mask_most' class='mask_most' onChange='MaskChanged(this);'>Most (default) <? PrintToolTip('LogMask'); ?><br>
                        <br>
                        <input type='checkbox' id='mask_gpio' class='mask_most' onChange='MaskChanged(this);'>GPIO<br>
                        <input type='checkbox' id='mask_mediaout' class='mask_most' onChange='MaskChanged(this);'>Media Outputs<br>
                        <input type='checkbox' id='mask_sync' class='mask_most' onChange='MaskChanged(this);'>MultiSync<br>
                        <input type='checkbox' id='mask_playlist' class='mask_most' onChange='MaskChanged(this);'>Playlists<br>
                        <input type='checkbox' id='mask_plugin' class='mask_most' onChange='MaskChanged(this);'>Plugins<br>
                        <input type='checkbox' id='mask_schedule' class='mask_most' onChange='MaskChanged(this);'>Scheduler<br>
                        <input type='checkbox' id='mask_sequence' class='mask_most' onChange='MaskChanged(this);'>Sequence Parser<br>
                        <input type='checkbox' id='mask_setting' class='mask_most' onChange='MaskChanged(this);'>Settings<br>
                    </td>
                </tr>
            </table>
        </td>
    </tr>
</table>

<script>
var logLevel = settings['LogLevel'];
if (typeof logLevel === 'undefined')
    logLevel = "info";

$('#LogLevel').val(logLevel);

var logMasks = Array('most');

if (typeof settings['LogMask'] === 'undefined') {
    logMasks = [ 'most' ];
    $('#LogMask input').prop('checked', false);
    $('#LogMask input.mask_most').prop('checked', true);
} else {
    logMasks = settings['LogMask'].split(",");

    for (var i = 0; i < logMasks.length; i++) {
        $('#mask_' + logMasks[i]).prop('checked', true);
    }
}

</script>
