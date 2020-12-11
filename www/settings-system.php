<?
$skipJSsettings = 1;
require_once('common.php');
?>

<script>
function GetGeoLocation() {
    $.get('https://ipapi.co/json/'
    ).done(function(data) {
        $('#Latitude').val(data.latitude).change();
        $('#Longitude').val(data.longitude).change();
    }).fail(function() {
        DialogError("GeoLocation Lookup", "GeoLocation lookup failed.");
    });
}

function ViewLatLon()
{
    var lat = $('#Latitude').val();
    var lon = $('#Longitude').val();

    var url = 'https://www.google.com/maps/@' + lat + ',' + lon + ',15z';
    window.open(url, '_blank');
}

function KioskInstallDone() {
    SetRebootFlag();
    $('#kioskCloseDialogButton').show();
}
function DisableKiosk() {
    $('#kioskCloseDialogButton').hide();
    $('#kioskPopup').dialog({ height: 600, width: 900, title: "Kiosk Frontend", dialogClass: 'no-close' });
    $('#kioskPopup').dialog( "moveToTop" );
    $('#kioskInstallText').html('');

    StreamURL('disableKiosk.php', 'kioskInstallText', 'KioskInstallDone');
}

function EnableKiosk() {
    if (confirm('Installing Kiosk components will take some time and consume around 400MB os space.')) {
        $('#kioskCloseDialogButton').hide();
        $('#kioskPopup').dialog({ height: 600, width: 900, title: "Kiosk Frontend", dialogClass: 'no-close' });
        $('#kioskPopup').dialog( "moveToTop" );
        $('#kioskInstallText').html('');

        StreamURL('installKiosk.php', 'kioskInstallText', 'KioskInstallDone');
    }
}

function CloseKioskDialog() {
    $('#kioskPopup').dialog('close');
    SetRebootFlag();
}

</script>
<?
PrintSettingGroup('system');
PrintSettingGroup('BBBLeds');

$extraData = "<input type='button' value='Lookup Location' onClick='GetGeoLocation();'> <input type='button' value='Show On Map' onClick='ViewLatLon();'>";
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
        echo "<input type='button' value='Disable Kiosk' onClick='DisableKiosk();'>";
    } else {
        echo "<input type='button' value='Enable Kiosk' onClick='EnableKiosk();'>";
    }
}
?>




<div id='kioskPopup' title='Kiosk Frontend' style="display: none">
    <textarea style='width: 99%; height: 94%;' disabled id='kioskInstallText'>
    </textarea>
    <input id='kioskCloseDialogButton' type='button' class='buttons' value='Close' onClick='CloseKioskDialog();' style='display: none;'>
</div>
