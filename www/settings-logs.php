<?
$skipJSsettings = 1;
require_once('common.php');
?>

<script>

function changeAll(newVal) {
   $("select[id^=LogLevel]:not(#LogLevel_ChannelData)").val(newVal).trigger("change");
}


</script>


<div class="callout callout-warning">
    <h4>Note:</h4>
   It is recommend that the Log Level be set to <b>Info</b> 
      on production systems
</div>
<?
PrintSettingGroup('log-levels');
?>

<div>
   <b>Bulk Change Log Level</b> (Changes all except for Channel Data)
   <div>
      <button class="buttons" onclick="changeAll('error')">Errors Only</button>
      <button class="buttons" onclick="changeAll('warn')">Warn</button>
      <button class="buttons" onclick="changeAll('info')">Info</button>
      <button class="buttons" onclick="changeAll('debug')">Debug</button>
      <button class="buttons" onclick="changeAll('excess')">Excessive</button>
   </div>
</div>
