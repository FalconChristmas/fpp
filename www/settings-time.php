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
    <div id="playerTime" class="statusTable d-flex">
        <div>
            <div class="labelHeading">FPP Time:</div>
            <div id="currentTime" class="labelValue"></div>
        </div>
    </div>
    <div>
        <?
        $extraData = "<input type='button' class='buttons' value='Lookup Time Zone' onClick='GetTimeZone();'>";
        PrintSettingGroup('time', $extraData);
        ?>
    </div>


</div>

<script>
UpdateCurrentTime(true);
</script>
