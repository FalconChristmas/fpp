<?
$skipJSsettings = 1;
require_once 'common.php';
?>

<script>
$( document ).ready(function() {
    if ($('#passwordEnable').val() == '1') {
        $('.passwordEnableChild').show();
    }
});
</script>


<?
PrintSettingGroup('ui');
?>


            <h2>UI Password</h2>

<?
PrintSetting('passwordEnable');
?>
<br>
            <div class='row passwordEnableChild' style='display: none;'>
                <div class="printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2">
                    <div class='description'><i class="fas fa-fw fa-nbsp ui-level-0"></i>Username
                    </div>
                </div>
                <div class='printSettingFieldCol col-md'><input disabled value='admin' size='5'></div>
            </div>
<?
PrintSetting('password');
PrintSetting('passwordVerify');
PrintSettingGroup('uiColors');
?>
