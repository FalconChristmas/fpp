<?
$skipJSsettings = 1;
require_once 'common.php';

$showOSSecurity = 0;
if (file_exists('/etc/fpp/platform') && !file_exists('/etc/fpp/container')) {
    $showOSSecurity = 1;
}

?>

<script>
function KioskInstallDone() {
    SetRebootFlag();
    EnableModalDialogCloseButton("enableKioskPopup");
    $('#enableKioskPopupCloseButton').prop("disabled", false);
}
function DisableKiosk() {
    SetSetting("Kiosk", 0, 0, 1);
    DisplayProgressDialog("enableKioskPopup", "Kiosk Frontend");
    StreamURL('disableKiosk.php', 'enableKioskPopupText', 'KioskInstallDone');
}

function EnableKiosk() {
    if (confirm('Installing Kiosk components will take some time and consume around 470MB of space.')) {
        DisplayProgressDialog("enableKioskPopup", "Kiosk Frontend");
        StreamURL('installKiosk.php', 'enableKioskPopupText', 'KioskInstallDone');
    }
}

</script>
<?
PrintSettingGroup('services');
echo "<br><br>\n";

if (($settings['uiLevel'] >= 1) && ($settings['Platform'] == "Raspberry Pi")) {
    PrintSettingGroup('kiosk');
    ?>
    The Kiosk frontend installs a bunch of extra packages and sets up Chrome running on the local HDMI port to
    allow FPP to be configured and monitored from a keyboard and mouse connected to the Pi's USB ports.  The
    additional packages take up about 400MB of space.
    <br>
<?
    if ((isset($settings['Kiosk'])) && ($settings['Kiosk'] == 1)) {
        echo "<input type='button' class='buttons' value='Disable Kiosk' onClick='DisableKiosk();'>";
    } else {
        echo "<input type='button' class='buttons' value='Enable Kiosk' onClick='EnableKiosk();'>";
    }
}

