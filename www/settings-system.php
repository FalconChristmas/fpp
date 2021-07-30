<?
$skipJSsettings = 1;
require_once('common.php');
?>

<script>
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

</script>
<?
if (file_exists("/.dockerenv")) {
    PrintSettingGroup('hostDocker');
}

PrintSettingGroup('system');
PrintSettingGroup('BBBLeds');

$extraData = "<div class='form-actions'><input type='button' class='buttons' value='Lookup Location' onClick='GetGeoLocation();'> <input type='button' class='buttons' value='Show On Map' onClick='ViewLatLon();'></div>";
PrintSettingGroup('geolocation', $extraData);

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
