<?
$skipJSsettings = 1;
require_once('common.php');

$showOSSecurity = 0;
if (file_exists('/etc/fpp/platform') && !file_exists('/.dockerenv'))
    $showOSSecurity = 1;
?>

<script>
function SaveSSHKeys() {
    var keys = $('#sshKeys').val();
    var result = Post('api/configfile/authorized_keys', false, keys);
    if (result.Status == 'OK')
        $.jGrowl("Keys Saved", { themeState: 'success' });
}

function KioskInstallDone() {
    SetRebootFlag();
    $('#kioskCloseDialogButton').show();
}
function DisableKiosk() {
    $('#kioskCloseDialogButton').hide();
    $('#kioskPopup').fppDialog({ height: 600, width: 900, title: "Kiosk Frontend", dialogClass: 'no-close' });
    $('#kioskPopup').fppDialog( "moveToTop" );
    $('#kioskInstallText').html('');

    StreamURL('disableKiosk.php', 'kioskInstallText', 'KioskInstallDone');
}

function EnableKiosk() {
    if (confirm('Installing Kiosk components will take some time and consume around 400MB of space.')) {
        $('#kioskCloseDialogButton').hide();
        $('#kioskPopup').fppDialog({ height: 600, width: 900, title: "Kiosk Frontend", dialogClass: 'no-close' });
        $('#kioskPopup').fppDialog( "moveToTop" );
        $('#kioskInstallText').html('');

        StreamURL('installKiosk.php', 'kioskInstallText', 'KioskInstallDone');
    }
}

function CloseKioskDialog() {
    $('#kioskPopup').fppDialog('close');
    SetRebootFlag();
}

<? if ($showOSSecurity) { ?>
$( document ).ready(function() {
    if ($('#osPasswordEnable').val() == '1') {
        $('.osPasswordEnableChild').show();
    }
});
<? } ?>

</script>
<?
if (file_exists("/.dockerenv") || $settings["IsDesktop"]) {
    PrintSettingGroup('hostDesktop');
}

PrintSettingGroup('system');
PrintSettingGroup('BBBLeds');

$extraData = "<div class='form-actions'><input type='button' class='buttons' value='Lookup Location' onClick='GetGeoLocation();'> <input type='button' class='buttons' value='Show On Map' onClick='ViewLatLon();'></div>";
PrintSettingGroup('geolocation', $extraData);

if ($showOSSecurity) {
?>
    <b>OS Password</b><br>
<?
    PrintSetting('osPasswordEnable');
?>
    <div class='row osPasswordEnableChild' style='display: none;'>
        <div class="printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2">
            <div class='description'><i class="fas fa-fw fa-nbsp ui-level-0"></i>Username
            </div>
        </div>
        <div class='printSettingFieldCol col-md'><input disabled value='fpp' size='5'></div>
    </div>
<?
    PrintSetting('osPassword');
    PrintSetting('osPasswordVerify');

    if ($uiLevel >= 1) {
?>
    <br>
    <b>SSH Keys</b> (root and fpp users)
    <img id="ssh_img" title="Add optional SSH key(s) for passwordless SSH authentication." src="images/redesign/help-icon.svg" width=22 height=22>
    <span id="ssh_tip" class="tooltip" style="display: none">Add optional SSH key(s) for passwordless SSH authentication.</span><br>
    <textarea  id='sshKeys' style='width: 100%;' rows='10'><?  echo shell_exec('sudo cat /root/.ssh/authorized_keys'); ?></textarea>
    <input type='button' class='buttons' value='Save Keys' onClick='SaveSSHKeys();'><br><br>
<?
    }
}

if (($settings['uiLevel'] >= 1) && ($settings['Platform'] == "Raspberry Pi")) {
?>
    <b>Kiosk Frontend</b><br>
    The Kiosk frontend installs a bunch of extra packages and sets up Chrome running on the local HDMI port to
    allow FPP to be configured and monitored from a keyboard and mouse connected to the Pi's USB ports.  The
    additional packages take up about 400MB of space.
    <br>
<?
    if (file_exists("/etc/fpp/kiosk")) {
        echo "<input type='button' class='buttons' value='Disable Kiosk' onClick='DisableKiosk();'>";
    } else {
        echo "<input type='button' class='buttons' value='Enable Kiosk' onClick='EnableKiosk();'>";
    }
}
?>




<div id='kioskPopup' title='Kiosk Frontend' style="display: none">
    <textarea style='width: 99%; height: 94%;' disabled id='kioskInstallText'>
    </textarea>
    <input id='kioskCloseDialogButton' type='button' class='buttons' value='Close' onClick='CloseKioskDialog();' style='display: none;'>
</div>
