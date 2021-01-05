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

function changeAll(newVal) {
   console.log("Changing all to ", newVal);
   $("select[id^=LogLevel]").val(newVal).trigger("change");
}


</script>


<div style="margin-bottom:1em;">
   <i>Note: It is recommend that the Log Level be set to <b>Info</b> 
      on production systems</i>
</div>
<?
PrintSettingGroup('log-levels');
?>

<div>
   <b>Bulk Change Log Level</b>
   <div>
      <button onclick="changeAll('error')">Errors Only</button>
      <button onclick="changeAll('warn')">Warn</button>
      <button onclick="changeAll('info')">Info</button>
      <button onclick="changeAll('debug')">Debug</button>
      <button onclick="changeAll('excess')">Excessive</button>
   </div>
</div>
