<?
$skipJSsettings = 1;
require_once('common.php');
?>

<div class="settingsManagerTimeContent">
        <?
        $extraData = "<input type='button' class='buttons' value='Lookup Time Zone' onClick='GetTimeZone();'>";
        $prependData = "<div class='row'><div class='col-md-4 col-lg-3 col-xxxl-2 align-top'><div class='description'><i class='fas fa-fw ui-level-0'></i>&nbsp;Current System Time</div></div><div id='currentTime' class='col-md labelValue disabled'></div></div>";
        PrintSettingGroup('time', $extraData, $prependData);
        ?>
</div>

<script>
$( document ).ready(function() {
    var statusTimeout = null;
    function UpdateCurrentTime() {
        if ($('#settings-time-tab').hasClass('active')) {
            $.get('api/time', function(data) {
                $('#currentTime').html(data.time);
            });
        }
    }
    statusTimeout = setInterval(UpdateCurrentTime, 1000);
});
</script>
