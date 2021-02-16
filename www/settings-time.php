<?
$skipJSsettings = 1;
require_once('common.php');
?>

<script>
function GetTimeZone() {
    $.get('https://ipapi.co/json/'
    ).done(function(data) {
        $('#TimeZone').val(data.timezone).change();
    }).fail(function() {
        DialogError("Time Zone Lookup", "Time Zone lookup failed.");
    });
}

</script>

<div class="settingsManagerTimeContent">
        <?
        $extraData = "<input type='button' class='buttons' value='Lookup Time Zone' onClick='GetTimeZone();'>";
        $prependData = "<div class='row'><div class='col-lg'><div>Current System Time</div></div><div id='currentTime' class='col-xl labelValue disabled'></div></div>";
        PrintSettingGroup('time', $extraData, $prependData);
        ?>
</div>

<script>
UpdateCurrentTime(true);
</script>
