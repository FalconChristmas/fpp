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

</script>
<?
PrintSettingGroup('system');
PrintSettingGroup('BBBLeds');

$extraData = "<input type='button' value='Lookup Location' onClick='GetGeoLocation();'> <input type='button' value='Show On Map' onClick='ViewLatLon();'>";
PrintSettingGroup('geolocation', $extraData);
?>
