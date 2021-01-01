<?
$skipJSsettings = 1;
require_once('common.php');
?>

<script>
function updateSettings() {
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
PrintSetting('LogLevel_General');
PrintSetting('LogLevel_ChannelOut');
PrintSetting('LogLevel_ChannelData');
PrintSetting('LogLevel_Control');
PrintSetting('LogLevel_E131Bridge');
PrintSetting('LogLevel_Effect');
PrintSetting('LogLevel_Event');
PrintSetting('LogLevel_GPIO');
PrintSetting('LogLevel_HTTP');
PrintSetting('LogLevel_MediaOut');
PrintSetting('LogLevel_Playlist');
PrintSetting('LogLevel_Plugin');
PrintSetting('LogLevel_Schedule');
PrintSetting('LogLevel_Settings');
PrintSetting('LogLevel_Sequence');
PrintSetting('LogLevel_Sync');
?>

<script>
var logLevel = settings['LogLevel'];
if (typeof logLevel === 'undefined')
    logLevel = "info";

$('#LogLevel').val(logLevel);

</script>
