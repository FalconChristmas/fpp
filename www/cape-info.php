<?php
header("Access-Control-Allow-Origin: *");
?>
<!DOCTYPE html>
<html>
<?php
require_once 'common.php';
require_once 'config.php';

$APIhost = 'api.FalconPlayer.com';
if (isset($settings['SigningAPIHost'])) {
    $APIhost = $settings['SigningAPIHost'];
}

$channelOutputDriver = "";

// Set either of these to 1 for testing
$printSigningUI = 0;
$offlineMode = 0;

// attempt to find a physical eeprom
$eepromFile = "/sys/bus/i2c/devices/1-0050/eeprom";
if (!file_exists($eepromFile)) {
    $eepromFile = "/sys/bus/i2c/devices/2-0050/eeprom";
    if (!file_exists($eepromFile)) {
        $eepromFile = '';
    }
}

// Test to see if FPP can get to the signing API
$curl = curl_init("https://$APIhost/js/internetTest.js");
curl_setopt($curl, CURLOPT_HEADER, 0);
curl_setopt($curl, CURLOPT_CONNECTTIMEOUT, 5);
curl_setopt($curl, CURLOPT_TIMEOUT, 5);
curl_setopt($curl, CURLOPT_USERAGENT, getFPPVersion());
curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
$request_content = curl_exec($curl);
$rc = curl_getinfo($curl, CURLINFO_RESPONSE_CODE);
curl_close($curl);

if ($rc != 200) {
    $offlineMode = 1;
}

$capeHardwareType = "Cape/Hat";
$currentCapeInfo = array();
if (isset($settings["cape-info"])) {
    $currentCapeInfo = $settings["cape-info"];
    if (isset($currentCapeInfo["hardwareType"])) {
        $capeHardwareType = $currentCapeInfo["hardwareType"];
    }

    $channelOutputDriver = '';
    $file = '/home/fpp/media/config/co-bbbStrings.json';
    if (!file_exists($file)) {
        $file = '/home/fpp/media/config/co-pixelStrings.json';
    }
    if (file_exists($file)) {
        $json = file_get_contents($file);
        $data = json_decode($json, true);
        if (isset($data['channelOutputs'])) {
            foreach ($data['channelOutputs'] as $co) {
                if (isset($co['type'])) {
                    if (($co['type'] == 'BBB48String') || ($co['type'] == 'DPIPixels') || ($co['type'] == 'BBShiftString')) {
                        $channelOutputDriver = $co['type'];
                    }
                }
            }
        }
    } else {
        $path = $mediaDirectory . "/tmp/strings/";
        if (is_dir($path)) {
            $files = scandir($path);
            foreach ($files as $file) {
                if (substr($file, 0, 1) != '.') {
                    $json = file_get_contents($path . $file);
                    $data = json_decode($json, true);
                    if ((isset($data['driver'])) &&
                        (($data['driver'] == 'DPIPixels') ||
                            ($data['driver'] == 'BBB48String') ||
                            ($data['driver'] == 'BBShiftString'))) {
                        $channelOutputDriver = $data['driver'];
                        break;
                    }
                }
            }
        }
    }

    if (($channelOutputDriver == 'BBB48String') || ($channelOutputDriver == 'DPIPixels') || ($channelOutputDriver == 'BBShiftString')) {
        $channelOutputDriverStr = "  This cape uses the $channelOutputDriver Channel Output driver.";
        if ($currentCapeInfo['serialNumber'] == 'FPP-INTERNAL') {
            if (isset($currentCapeInfo['verifiedKeyId'])) {
                $signingStatus = sprintf("Cape is using a virtual EEPROM signed with the '%s' key and the %s Channel Output driver.", $currentCapeInfo['verifiedKeyId'], $channelOutputDriver);
                if ($currentCapeInfo['verifiedKeyId'] == 'fp') {
                    $printSigningUI = 1;
                    if ((isset($currentCapeInfo['signed']['licensePorts'])) && ($currentCapeInfo['signed']['licensePorts'] >= 9999)) {
                        $signingStatus .= "  This cape is already licensed for it's full set of outputs.  The signing form is shown below in case the cape EEPROM needs to be re-signed.";
                    }
                } else {
                    $signingStatus .= "  This cape is licensed for it's full set of outputs.";
                }
            } else {
                if (isset($currentCapeInfo['validEepromLocation']) && !$currentCapeInfo['validEepromLocation']) {
                    $signingStatus .= "<b>WARNING: The location field specified in this EEPROM is invalid so the EEPROM is being treated as unsigned.</b>";
                } else {
                    $signingStatus = sprintf("Cape is using an <b>Unsigned</b> virtual EEPROM and the $channelOutputDriver Channel Output driver.");
                    $printSigningUI = 1;
                }
            }
        } else if (isset($currentCapeInfo['verifiedKeyId'])) {
            $signingStatus = sprintf("Cape EEPROM is signed using the '%s' key and uses the %s Channel Output driver.", $currentCapeInfo['verifiedKeyId'], $channelOutputDriver);
            if ($currentCapeInfo['verifiedKeyId'] == 'fp') {
                $printSigningUI = 1;
                if ((isset($currentCapeInfo['signed']['licensePorts'])) && ($currentCapeInfo['signed']['licensePorts'] >= 9999)) {
                    $signingStatus .= "  This cape is already licensed for an unlimited number of outputs.  The signing inputs are shown below in case the cape needs to be re-signed.";
                }
            } else {
                $signingStatus .= "  This cape is licensed for it's full set of outputs.";
            }
        } else {
            $signingStatus = "Cape is using an <b>Unsigned</b> EEPROM and the $channelOutputDriver Channel Output driver.";
            $printSigningUI = 1;
        }
    } else {
        $signingStatus = "This cape is not using any Channel Output drivers which need a signed EEPROM.  If you believe it should be, go to the Pixel Strings config page and re-save the string configs and check back here.";
    }
}

?>

<head>
<?php
include 'common/menuHead.inc';
?>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
<script language='Javascript' src='https://<?=$APIhost?>/js/internetTest.js?ref=<?php echo time(); ?>'></script>
<script language="Javascript">

function CloseUpgradeDialog(reload = false) {
    $('#upgradePopup').fppDialog('close');
    if (reload)
        location.reload();
}



function RestoreDone() {
    EnableModalDialogCloseButton("RestoreEEPROM");
    $("#RestoreEEPROMCloseButton").prop("disabled", false);
}

function RestoreFirmwareDone() {
    var txt = $('#RestoreEEPROMText').val();
    if (txt.includes("Cape does not match new firmware")) {
        var arrayOfLines = txt.match(/[^\r\n]+/g);
        var msg = "Are you sure you want to replace the firmware for cape:\n" + arrayOfLines[1] + "\n\nWith the firmware for: \n" + arrayOfLines[2] + "\n";
        if (confirm(msg)) {
            var filename = $('#backupFile').val();
            $('#RestoreEEPROMText').html('');
            StreamURL('upgradeCapeFirmware.php?force=true&filename=' + filename, 'RestoreEEPROMText', 'RestoreDone', 'RestoreDone', 'GET', null, false, false);
        }
    }
    RestoreDone();
}

function RestoreFirmware() {
    var filename = $('#backupFile').val();

    DisplayProgressDialog("RestoreEEPROM", "Restore Cape Firmware");
    StreamURL('upgradeCapeFirmware.php?filename=' + filename, 'RestoreEEPROMText', 'RestoreFirmwareDone', 'RestoreFirmwareDone', 'GET', null, false, false);
}

function UpgradeDone() {
    EnableModalDialogCloseButton("UpgradeEEPROM");
    $("#UpgradeEEPROMCloseButton").prop("disabled", false);
}

function UpgradeFirmwareDone() {
    var txt = $('#UpgradeEEPROMText').val();
    if (txt.includes("Cape does not match new firmware")) {
        var arrayOfLines = txt.match(/[^\r\n]+/g);
        var msg = "Are you sure you want to replace the firmware for cape:\n" + arrayOfLines[2] + "\n\nWith the firmware for: \n" + arrayOfLines[3] + "\n";
        if (confirm(msg)) {
            var eepromFile = $('#eepromVendorCapeVersions').val();
            let firmware = document.getElementById("firmware").files[0];

            let formData = new FormData();

            if (eepromFile != '') {
                formData.append("filename", eepromFile);
            } else {
                formData.append("firmware", firmware);
            }

            $('#UpgradeEEPROMText').html('');
            StreamURL('upgradeCapeFirmware.php?force=true', 'UpgradeEEPROMText', 'UpgradeDone', 'UpgradeDone', 'POST', formData, false, false);
        }
    }
    UpgradeDone();
}
function UpgradeFirmware(force = false) {
    var eepromFile = $('#eepromVendorCapeVersions').val();
    let firmware = document.getElementById("firmware").files[0];
    if ((eepromFile == '') && (firmware == "" || firmware == null)) {
        alert('You must choose a downloadable file from the list or pick a local file to upload.');
        return;
    }

    let formData = new FormData();

    if (eepromFile != '') {
        formData.append("filename", eepromFile);
    } else {
        formData.append("firmware", firmware);
    }

    var forceOpt = '';
    if (force)
        forceOpt = '?force=true';

    DisplayProgressDialog("UpgradeEEPROM", "Upgrade Cape Firmware");
    StreamURL('upgradeCapeFirmware.php' + forceOpt, 'UpgradeEEPROMText', 'UpgradeFirmwareDone', 'UpgradeFirmwareDone', 'POST', formData, false, false);
}

var eepromList = [];
var hasPhysicalEEPROM = <?echo $eepromFile == '' ? "false" : "true" ?>;

function GetDownloadableEEPROMList() {
    $.get('https://raw.githubusercontent.com/FalconChristmas/fpp-data/master/eepromVendors.json', function(eepromVendors) {
        if (typeof eepromVendors === 'string') {
            eepromVendors = JSON.parse(eepromVendors);
        }
        for (var vendor in eepromVendors["vendors"]) {
            if (eepromVendors["vendors"][vendor].url != "") {
                $.get(eepromVendors["vendors"][vendor].url, function(eepromjson) {
                    if (typeof eepromjson === 'string') {
                        eepromjson = JSON.parse(eepromjson);
                    }
                    var vendName = eepromjson.name;
                    if ("url" in eepromjson) {
                        vendName += " (" + eepromjson.url + ")";
                    }
                    eepromList[vendName] = eepromjson["capes"];
                    var options = "<option value='" + vendName+ "'>" + vendName + '</option>';
                    $('#eepromVendorList').append(options);
                    $('#eepromVendorList').removeAttr('disabled');
                    $('#eepromVendorList').show();
                });
            }
        }
    });
}

function eepromVendorListChanged() {
    var vendor = $('#eepromVendorList').val();
    if (vendor in  eepromList) {
        var eepromVendorCape = $('#eepromVendorCapes');
        eepromVendorCape.empty();
        var capes = eepromList[vendor];
        for (cape in capes) {
            var capeObj = capes[cape];
            var valid = true;
            if ("location" in capeObj) {
                if (hasPhysicalEEPROM && capeObj.location == "virtual") {
                    valid = false;
                }
                if (!hasPhysicalEEPROM && capeObj.location == "eeprom") {
                    valid = false;
                }
            }
            if ("platforms" in capeObj) {
                var plat = capeObj.platforms;
                if (settings["Platform"] == "BeagleBone Black") {
                    if (!plat.includes(settings["Variant"])) {
                        valid = false;
                    }
                } else {
                    if (!plat.includes(settings["Platform"])
                       && !plat.includes(settings["Variant"])) {
                        valid = false;
                    }
                }
            }
            if (valid) {
                var option = "<option value='" + cape + "'>" + cape + "</option>";
                eepromVendorCape.append(option);
                eepromVendorCape.removeAttr('disabled');
            }
        }
    } else {
        $('#eepromVendorCapes').empty();
        $('#eepromVendorCapes').append("<option>-- Select Vendor --</option>");
        $('#eepromVendorCapes').prop('disabled', true);
    }
    eepromVendorCapeChanged();
}
function eepromVendorCapeChanged() {
    var vendor = $('#eepromVendorList').val();
    if (vendor in  eepromList) {
        var capeName = $('#eepromVendorCapes').val();
        var eepromVendorCapeVersions = $('#eepromVendorCapeVersions');
        eepromVendorCapeVersions.empty();
        if (capeName in eepromList[vendor]) {
            for (v in eepromList[vendor][capeName]["versions"]) {
                var option = "<option value='" + eepromList[vendor][capeName]["versions"][v].url + "'>" + v + "</option>";
                eepromVendorCapeVersions.append(option);
                eepromVendorCapeVersions.removeAttr('disabled');
            }
        } else {
            $('#eepromVendorCapeVersions').append("<option value=''>-- Select Vendor/Cape --</option>");
            $('#eepromVendorCapeVersions').prop('disabled', true);
        }
    } else {
        $('#eepromVendorCapeVersions').empty();
        $('#eepromVendorCapeVersions').append("<option value=''>-- Select Vendor/Cape --</option>");
        $('#eepromVendorCapeVersions').prop('disabled', true);
    }
    eepromVendorCapeVersionChanged();
}
function eepromVendorCapeVersionChanged() {
    var url = $('#eepromVendorCapeVersions').val();
    if (url == '') {
        $('#UpdateFirmware').removeClass('btn-success');
        $('#UpdateFirmware').attr('disabled', 'disabled');
    } else {
        $('#firmware').val('');
        $('#UpdateFirmware').addClass('btn-success');
        $('#UpdateFirmware').removeAttr('disabled');
    }
}

function firmwareChanged() {
    $('#eepromVendorList').val('').trigger("change");;

    if ($('#filename').val() == '') {
        $('#UpdateFirmware').removeClass('btn-success');
        $('#UpdateFirmware').attr('disabled', 'disabled');
    } else {
        $('#UpdateFirmware').addClass('btn-success');
        $('#UpdateFirmware').removeAttr('disabled');
    }
}

function backupChanged() {
    if ($('#backupFile').val() == '') {
        $('#RestoreFirmware').removeClass('btn-success');
        $('#RestoreFirmware').attr('disabled', 'disabled');
    } else {
        $('#RestoreFirmware').addClass('btn-success');
        $('#RestoreFirmware').removeAttr('disabled');
    }
}

function RedeemVoucher() {
    var voucherNumber = $('#voucherNumber').val().toUpperCase();
    var fname = $('#voucherFName').val();
    var lname = $('#voucherLName').val();
    var email = $('#voucherEmail').val();
    var password = $('#voucherPassword').val();
    var alphanumeric = /^[\p{L}\p{N}]+$/u;
    var passwordAllowed = /^[-\p{L}\p{N}_#@!\$%^&*()]+$/u;

    if (voucherNumber == 'SUDCAEPP') {
        alert("Thank you for reading the FPP Manual, but voucher ID 'SUDCAEPP' is for documentation use only and is not valid.  Please enter a valid voucher ID.");
        return;
    }

    if (!(/^[-A-Z0-9]+$/.test(voucherNumber))) {
        alert('Invalid Voucher Number.  The Voucher Number field should contain only the letters, numbers, and hypens.');
        return;
    }

    if (!fname.match(alphanumeric)) {
        alert('Invalid characters in first name.');
        return;
    }

    if (!lname.match(alphanumeric)) {
        alert('Invalid characters in last name.');
        return;
    }

    if (!validateEmail(email)) {
        alert('Invalid Email address.');
        return;
    }

    if ((!password.match(passwordAllowed)) || (password.length < 8)) {
        alert('Invalid characters in password or length too short.  Passwords should be at least 8 characters long and may contain only letters, numbers, and any of the following characters         -_#@!\$%^&*()');
        return;
    }

    $('#voucherNumber').val(voucherNumber); // Save the toUpperCase()-ed value
    var url = 'api/cape/eeprom/voucher';

    $('.dialogCloseButton').hide();
    $('#upgradePopup').fppDialog({ height: 600, width: 900, title: "Redeem Voucher", dialogClass: 'no-close' });
    $('#upgradePopup').fppDialog( "moveToTop" );
    $('#upgradeText').html('Voucher Redemption Status:\n');

    $('#upgradeText').append('- Contacting API to redeem voucher\n');

    data = {};
    data.voucher = voucherNumber;
    data.first_name = fname;
    data.last_name = lname;
    data.email = email;
    data.password = password;

    dataStr = JSON.stringify(data);

    $.ajax({
        url: url,
        type: 'POST',
        data: dataStr,
        contentType: 'application/json',
        async: true,
        dataType: 'json',
        success: function (data) {
            if (data.Status == 'OK') {
                $('#upgradeText').append('- Voucher Redeemed Successfully\n');
                $('#upgradeText').append('  - Order Number: ' + data.order + '\n');
                $('#upgradeText').append('  - License Key: ' + data.key + '\n');
                $('#upgradeText').append('\nProceeding to sign EEPROM with new order and key info.\n\n');

                SetSetting('voucherRedeemed', '1', true);

                SignEEPROMHelper(data.key, data.order, true);
            } else {
                $('#upgradeText').append('\nERROR redeeming voucher:\n\n' + data.Message);
                $('#errorDialogButton').show();
            }
        },
        error: function () {
            $('#upgradeText').append('\nERROR calling signing API\n');
            $('#errorDialogButton').show();
        }
    });
}

function DownloadOfflineSigningFile() {
    var order = $('#offlineOrderNumber').val();
    var orderRegex = /^[0-9]+$/;
    var key = $('#offlineLicenseKey').val().toUpperCase();
    var keyRegex = /^FPPAFK-[a-zA-Z0-9]{6}-[a-zA-Z0-9]{6}-[a-zA-Z0-9]{6}-[a-zA-Z0-9]{6}$/;

    if (!orderRegex.test(order)) {
        alert('Invalid Order Number.  The Order Number field should contain only the numbers 0-9.');
        return;
    }

    if (!keyRegex.test(key)) {
        alert("Invalid License Key Format.  The License Key should match the format 'FPPAFK-XXXXXX-XXXXXX-XXXXXX-XXXXXX' where the X's may be any letter or number.");
        return;
    }

    $('#offlineLicenseKey').val(key); // Save the toUpperCase()-ed value

    // Update values on the other tab
    $('#licenseKey').val(key);
    $('#orderNumber').val(order);

    location.href = 'api/cape/eeprom/signingFile/' + key + '/' + order;
}

function SignEEPROMViaBrowser() {
    var order = $('#orderNumber').val();
    var orderRegex = /^[0-9]+$/;
    var key = $('#licenseKey').val().toUpperCase();
    var keyRegex = /^FPPAFK-[a-zA-Z0-9]{6}-[a-zA-Z0-9]{6}-[a-zA-Z0-9]{6}-[a-zA-Z0-9]{6}$/;

    if (!orderRegex.test(order)) {
        alert('Invalid Order Number.  The Order Number field should contain only the numbers 0-9.');
        return;
    }

    if (!keyRegex.test(key)) {
        alert("Invalid License Key Format.  The License Key should match the format 'FPPAFK-XXXXXX-XXXXXX-XXXXXX-XXXXXX' where the X's may be any letter or number.");
        return;
    }

    $('#licenseKey').val(key); // Save the toUpperCase()-ed value
    var url = 'api/cape/eeprom/signingData/' + key + '/' + order;

    $('.dialogCloseButton').hide();
    $('#upgradePopup').fppDialog({ height: 600, width: 900, title: "Sign Cape EEPROM", dialogClass: 'no-close' });
    $('#upgradePopup').fppDialog( "moveToTop" );
    $('#upgradeText').html('Signing Status:\n');

    $('#upgradeText').append('- Downloading signing packet from FPP\n');

    $.ajax({
        url: url,
        type: 'GET',
        data: '',
        async: true,
        dataType: 'json',
        success: function (data) {
            if (data.hasOwnProperty('key')) {
                $('#upgradeText').append('- Uploading signing packet to signing API\n');
                $.ajax({
                    url: 'https://<?=$APIhost?>/api/fpp/eeprom/sign',
                    type: 'POST',
                    contentType: 'application/json',
                    data: JSON.stringify(data),
                    async: true,
                    dataType: 'json',
                    success: function (data) {
                        if (data.Status == 'OK') {
                            $('#upgradeText').append('- Received signed packet back from signing API\n');
                            $('#upgradeText').append('- Uploading signed packet to FPP\n');
                            $.ajax({
                                url: 'api/cape/eeprom/signingData',
                                type: 'POST',
                                contentType: 'application/json',
                                data: JSON.stringify(data),
                                async: true,
                                dataType: 'json',
                                success: function (data) {
                                    if (data.Status == 'OK') {
                                        SetRebootFlag();
                                        $('#upgradeText').append('- Signing Complete.  Please reboot.\n');
                                        $('#closeDialogButton').show();
                                    } else {
                                        $('#upgradeText').append('\nERROR processing signed packet: ' + data.Message + '\n');
                                        $('#errorDialogButton').show();
                                    }
                                },
                                error: function () {
                                    $('#upgradeText').append('\nERROR uploading signed packet\n');
                                    $('#errorDialogButton').show();
                                }
                            });
                        // END of second nested AJAX call
                        } else {
                            $('#upgradeText').append('\nERROR signing packet: ' + data.Message + '\n');
                            $('#errorDialogButton').show();
                        }
                    },
                    error: function () {
                        $('#upgradeText').append('\nERROR uploading signing packet\n');
                        $('#errorDialogButton').show();
                    }
                });
               // END of first nested AJAX call
            } else {
                $('#upgradeText').append('\nERROR retrieving signing packet: ' + data.Message + '\n');
                $('#errorDialogButton').show();
            }
        },
        error: function () {
            $('#upgradeText').append('\nERROR retrieving signing packet from FPP\n');
            $('#errorDialogButton').show();
        }
    });
}

function SignEEPROMHelper(key, order, redirect=false) {
    var url = 'api/cape/eeprom/sign/' + key + '/' + order;

    $('#upgradeText').append('- Contacting API to sign EEPROM\n');

    $.ajax({
        url: url,
        type: 'POST',
        data: '',
        async: true,
        dataType: 'json',
        success: function (data) {
            if (data.Status == 'OK') {
                SetRebootFlag();
                $('#upgradeText').append('- Signing Complete.  Please reboot.\n\nYou may also check the <b>EEPROM Signature</b> tab prior to reboot to confirm the EEPROM was signed.');
                $('#closeDialogButton').show();
            } else {
                $('#upgradeText').append('\nERROR signing EEPROM:\n\n' + data.Message);
                $('#errorDialogButton').show();
            }
        },
        error: function () {
            $('#upgradeText').append('\nERROR calling signing API\n');
            $('#errorDialogButton').show();
        }
    });
}

function SignEEPROM() {
    var order = $('#orderNumber').val();
    var orderRegex = /^[0-9]+$/;
    var key = $('#licenseKey').val().toUpperCase();
    var keyRegex = /^FPPAFK-[A-Z0-9]{6}-[A-Z0-9]{6}-[A-Z0-9]{6}-[A-Z0-9]{6}$/;

    if (!orderRegex.test(order)) {
        alert('Invalid Order Number.  The Order Number field should contain only the numbers 0-9.');
        return;
    }

    if (!keyRegex.test(key)) {
        alert("Invalid License Key Format.  The License Key should match the format 'FPPAFK-XXXXXX-XXXXXX-XXXXXX-XXXXXX' where the X's may be any letter or number.");
        return;
    }

    $('#licenseKey').val(key); // Save the toUpperCase()-ed value

    $('.dialogCloseButton').hide();
    $('#upgradePopup').fppDialog({ height: 600, width: 900, title: "Sign Cape EEPROM", dialogClass: 'no-close' });
    $('#upgradePopup').fppDialog( "moveToTop" );
    $('#upgradeText').html('Signing Status:\n');

    SignEEPROMHelper(key, order);
}

function UploadSignedPacket() {
    let signingPacket = document.getElementById("signingPacket").files[0];
    if (signingPacket == "" || signingPacket == null) {
        return;
    }
    let formData = new FormData();
    formData.append("signingPacket", signingPacket);

    $.ajax({
        url: 'api/cape/eeprom/signingData',
        method: 'POST',
        data: formData,
        async: true,
        contentType: false,
        processData: false,
        dataType: 'json',
        success: function (data) {
            if (data.Status == 'OK') {
                location.href='cape-info.php#eeprom-signature';
                reloadPage();
            } else {
                alert('ERROR getting EEPROM signing data: ' + data.Message);
            }
        },
        error: function () {
            $.jGrowl('Error getting EEPROM signing data', { themeState: 'danger' });
        }
    });
}

var keysVisible = 0;
function ToggleKeyVisibility() {
    if (keysVisible) {
        keysVisible = 0;
        $('#keyVisibilityIcon').removeClass('fa-eye-slash');
        $('#keyVisibilityIcon').addClass('fa-eye');
        $('.keyHidden').show();
        $('.keyShow').hide();
    } else {
        keysVisible = 1;
        $('#keyVisibilityIcon').addClass('fa-eye-slash');
        $('#keyVisibilityIcon').removeClass('fa-eye');
        $('.keyHidden').hide();
        $('.keyShow').show();
    }
}

function TestInternet() {
    // This variable is defined in https://api.FalconPlayer.com/js/internetTest.js included above
    if (typeof theInternetIsUp === 'undefined') {
        // Do nothing here
    } else {
        // Show the online version of the signing UI
        $('.internetOnly').show();
    }
}

$(document).ready(function() {
    GetDownloadableEEPROMList();
    setTimeout(function() { TestInternet(); }, 100);
});

</script>
<title><?echo $pageTitle; ?></title>
<style>
.no-close .ui-dialog-titlebar-close {display: none }
</style>
</head>
<body>
<div id="bodyWrapper">
<?php
$activeParentMenuItem = 'help';
include 'menu.inc';
?>
    <div class="mainContainer">
        <div class="title"><?=$capeHardwareType?> Info</div>
        <div class="pageContent">
<?
if (isset($settings["cape-info"])) {
    ?>
            <div>
                <div style="overflow: hidden; padding: 10px;">
                    <div class='eepromTabs'>
                        <div id='eepromManager'>
                            <ul id='eepromManagerTabs' class="nav nav-pills pageContent-tabs" role="tablist">
                                <li class='nav-item'>
                                    <a class="nav-link active" id="eeprom-about-tab" data-bs-toggle="pill" href="#eeprom-about" role="tab" aria-controls="eeprom-about" aria-selected="true">
                                        About
                                    </a>
                                </li>
                                <li class='nav-item'>
                                    <a class="nav-link" id="eeprom-signature-tab" data-bs-toggle="pill" href="#eeprom-signature" role="tab" aria-controls="eeprom-signature">
                                        EEPROM Signature
                                    </a>
                                </li>
<?php
if ($printSigningUI) {
        if ($offlineMode) {
            ?>
                                <li class='nav-item'>
                                    <a class="nav-link" id="eeprom-offline-tab" data-bs-toggle="pill" href="#eeprom-offline" role="tab" aria-controls="eeprom-offline">
                                        Offline Signing
                                    </a>
                                </li>
<?php
} else if (((!isset($settings['voucherRedeemed'])) || ($settings['voucherRedeemed'] != '1')) && ((!isset($currentCapeInfo['verifiedKeyId'])) || ($currentCapeInfo['verifiedKeyId'] != 'fp'))) {
            ?>
                                <li class='nav-item'>
                                    <a class="nav-link" id="eeprom-voucher-tab" data-bs-toggle="pill" href="#eeprom-voucher" role="tab" aria-controls="eeprom-voucher">
                                        Voucher Redemption
                                    </a>
                                </li>
<?php
}
    }
    ?>
                                <li class='nav-item'>
                                    <a class="nav-link" id="eeprom-upgrade-tab" data-bs-toggle="pill" href="#eeprom-upgrade" role="tab" aria-controls="eeprom-upgrade">
                                        EEPROM Upgrade
                                    </a>
                                </li>
                            </ul>

                            <div class="tab-content" id="eepromTabsContent">

                                <div class="tab-pane fade show active" id="eeprom-about" role="tabpanel" aria-labelledby="eeprom-about-tab">

            <h2>About <?=$capeHardwareType?> </h2>
            <div class="container-fluid">
                <div class="row">
                    <div class='<?if (isset($currentCapeInfo['vendor'])) {echo "aboutLeft col-md";} else {echo "aboutAll";}?> '>
                        <table class='tblAbout'>
                            <tr><td><b>Name:</b></td><td width="100%"><?echo $currentCapeInfo['name'] ?></td></tr>
<?php
if (isset($currentCapeInfo['version'])) {
        echo "<tr><td><b>Version:</b></td><td>" . $currentCapeInfo['version'] . "</td></tr>";
    }
    if (isset($currentCapeInfo['serialNumber'])) {
        echo "<tr><td><b>Serial&nbsp;Number:</b></td><td>" . $currentCapeInfo['serialNumber'] . "</td></tr>";
    }
    if (isset($currentCapeInfo['designer'])) {
        echo "<tr><td><b>Designer:</b></td><td>" . $currentCapeInfo['designer'] . "</td></tr>";
    }
    if ((isset($currentCapeInfo['verifiedKeyId'])) && (($channelOutputDriver == 'BBB48String') || ($channelOutputDriver == 'DPIPixels') || ($channelOutputDriver == 'BBShiftString'))) {
        $key = $currentCapeInfo['verifiedKeyId'];
        if ($key == 'fp') {
            if (isset($currentCapeInfo['signed']['licensePorts'])) {
                echo "<tr><td><b>Licensed&nbsp;Outputs:</b></td><td>" . $currentCapeInfo['signed']['licensePorts'] . " ('fp' key)</td></tr>";
            } else {
                echo "<tr><td><b>Licensed&nbsp;Outputs:</b></td><td>Unknown, licensePorts setting does not exist ('fp' key)</td></tr>";
            }
        } else {
            echo "<tr><td><b>Licensed&nbsp;Outputs:</b></td><td>Unlimited ('$key' key)</td></tr>";
        }
    } else if (($channelOutputDriver == 'BBB48String') || ($channelOutputDriver == 'DPIPixels') || ($channelOutputDriver == 'BBShiftString')) {
        echo "<tr><td><b>Licensed&nbsp;Outputs:</b></td><td>None, cape EEPROM is not signed.</td></tr>";
    }
    if ($channelOutputDriver != '') {
        echo "<tr><td><b>Output&nbsp;Driver:</b></td><td>" . $channelOutputDriver . "</td></tr>";
    }
    if (((!isset($currentCapeInfo['verifiedKeyId'])) || ($currentCapeInfo['verifiedKeyId'] == 'fp')) && isset($currentCapeInfo['eepromLocation'])) {
        $locationMsg = '';
        if (isset($currentCapeInfo['validEepromLocation']) && !$currentCapeInfo['validEepromLocation']) {
            $locationMsg = ' - <b>Warning:</b> Location flag specified in EEPROM<br>does not match actual EEPROM location.';
        }

        $location = 'Physical';
        if (preg_match('/cape-eeprom.bin/', $currentCapeInfo['eepromLocation'])) {
            $location = 'File';
        }
        echo "<tr><td valign='top'><b>EEPROM&nbsp;Location:</b></td><td>$location$locationMsg</td></tr>";
    }
    if (isset($currentCapeInfo['description'])) {
        echo "<tr><td colspan=\"2\">";
        if (isset($currentCapeInfo['vendor']) || $currentCapeInfo['name'] == "Unknown") {
            echo $currentCapeInfo['description'];
        } else {
            echo htmlspecialchars($currentCapeInfo['description']);
        }
        echo "</td></tr>";
    }
    ?>
                        </table>
                    </div>
<?php
if (isset($currentCapeInfo['vendor'])) {
        ?>
                   <div class='aboutRight col-md'>
                       <table class='tblAbout'>
                            <tr><td><b>Vendor&nbsp;Name:</b></td><td><?echo $currentCapeInfo['vendor']['name'] ?></td></tr>
<?php
if (isset($currentCapeInfo['vendor']['url'])) {
            $url = $currentCapeInfo['vendor']['url'];
            $landing = $url;
            if (isset($currentCapeInfo['vendor']['landingPage'])) {
                $landing = $currentCapeInfo['vendor']['landingPage'];
            }
            if ($settings['SendVendorSerial'] == 1) {
                $landing = $landing . "?sn=" . $currentCapeInfo['serialNumber'] . "&id=" . $currentCapeInfo['id'];
            }
            if (isset($currentCapeInfo['cs']) && $currentCapeInfo['cs'] != "" && $settings['SendVendorSerial'] == 1) {
                $landing = $landing . "&cs=" . $currentCapeInfo['cs'];
            }
            if ($settings['hideExternalURLs']) {
                $landing = "";
            }
            echo "<tr><td><b>Vendor&nbsp;URL:</b></td><td><a href=\"" . $landing . "\">" . $url . "</a></td></tr>";
        }
        if (isset($currentCapeInfo['vendor']['phone'])) {
            echo "<tr><td><b>Phone&nbsp;Number:</b></td><td>" . $currentCapeInfo['vendor']['phone'] . "</td></tr>";
        }
        if (isset($currentCapeInfo['vendor']['email']) && !$settings['hideExternalURLs']) {
            echo "<tr><td><b>E-mail:</b></td><td><a href=\"mailto:" . $currentCapeInfo['vendor']['email'] . "\">" . $currentCapeInfo['vendor']['email'] . "</td></tr>";
        }
        if (isset($currentCapeInfo['vendor']['forum']) && !$settings['hideExternalURLs']) {
            echo "<tr><td><b>Support Forum:</b></td><td><a href=\"" . $currentCapeInfo['vendor']['forum'] . "\">" . $currentCapeInfo['vendor']['forum'] . "</td></tr>";
        }
        if (isset($currentCapeInfo['vendor']['image']) && $settings['FetchVendorLogos']) {
            if ($settings['SendVendorSerial'] == 1) {
                $iurl = $currentCapeInfo['vendor']['image'] . "?sn=" . $currentCapeInfo['serialNumber'] . "&id=" . $currentCapeInfo['id'];
            }
            if (isset($currentCapeInfo['cs']) && $currentCapeInfo['cs'] != "" && $settings['SendVendorSerial'] == 1) {
                $iurl = $iurl . "&cs=" . $currentCapeInfo['cs'];
            }
            echo "<tr><td colspan=\"2\"><a href=\"" . $landing . "\"><img style='max-height: 90px; max-width: 300px;' src=\"" . $iurl . "\" /></a></td></tr>";
        }
        ?>
                       </table>
                   </div>
<?
    }
    ?>
               </div>
           </div>
                                </div>
                                <div class="tab-pane fade show" id="eeprom-signature" role="tabpanel" aria-labelledby="eeprom-signature-tab">
                                    <div class="container-fluid">
                                        <div class="row">
                                            <div class="aboutAll col-md">
<?php
if (isset($settings["cape-info"])) {
        echo $signingStatus . "<br>";
        ?>
    <br>
    <table>
<?php
$printBreak = 0;
        if (isset($currentCapeInfo['signed'])) {
            if (isset($currentCapeInfo['signed']['licensePorts'])) {
                $outputs = $currentCapeInfo['signed']['licensePorts'];
                if ($outputs == 9999) {
                    $outputs = "Unlimited";
                }

                echo "<tr><td><b>Licensed Outputs:</b></td><td>$outputs</td></tr>";
            }

            if (isset($currentCapeInfo['signed']['licenseKey'])) {
                if (gettype($currentCapeInfo['signed']['licenseKey']) == "string") {
                    echo "<tr><td><b>License Key: <i class='fas fa-eye' id='keyVisibilityIcon' onClick='ToggleKeyVisibility();'></i></b></td><td class='keyShow' style='display: none;'>" . $currentCapeInfo['signed']['licenseKey'] . "</td><td class='keyHidden'>******-******-******-******-******</td></tr>";
                } else {
                    $first = 1;
                    foreach ($currentCapeInfo['signed']['licenseKey'] as $key) {
                        if ($first) {
                            echo "<tr><td><b>License Keys: <i class='fas fa-eye' id='keyVisibilityIcon' onClick='ToggleKeyVisibility();'></i></b></td><td class='keyShow' style='display: none;'>$key</td><td class='keyHidden'>******-******-******-******-******</td></tr>\n";
                            $first = 0;
                        } else {
                            echo "<td></td><td class='keyShow' style='display: none;'>$key</td><td class='keyHidden'>******-******-******-******-******</td></tr>\n";
                        }
                    }
                }
            }

            if ($currentCapeInfo['serialNumber'] == 'FPP-INTERNAL') {
                if (isset($currentCapeInfo['signed']['capeSerial'])) {
                    echo "<tr><td><b>Licensed Serial:</b></td><td>" . $currentCapeInfo['signed']['capeSerial'] . "</td></tr>";
                }

                if (isset($currentCapeInfo['signed']['deviceSerial'])) {
                    echo "<tr><td><b>Licensed Device:</b></td><td>" . $currentCapeInfo['signed']['deviceSerial'] . "</td></tr>";
                }

            }

            if (isset($currentCapeInfo['signed']['signTime'])) {
                echo "<tr><td><b>EEPROM Signed at:</b></td><td>" . date('Y-m-d h:i:s A T', $currentCapeInfo['signed']['signTime']) . "</td></tr>";
            }

            $printBreak = 1;
        }
        ?>
    </table>
<?php
if ($printBreak) {
            echo "<br>\n";
        }

        if ($printSigningUI) {
            if (!isset($currentCapeInfo['verifiedKeyId'])) {
                echo "<b>NOTE: This cape is using an unsigned EEPROM and the $channelOutputDriver Channel Output driver which operates in a limited manner until the EEPROM is signed.  Pixel outputs will be limited to 50 pixels per output and smart receivers will be disabled.</b><br><br>\n";
            }

            ?>
    You may sign the EEPROM using a license key purchased from <a href='https://shop.FalconPlayer.com' target='_blank'>https://shop.FalconPlayer.com</a>.  Once you have purchased a license key, enter the Order Number and License Key below.
<?php
if ($offlineMode) {
                echo "If <b>neither</b> your FPP instance or your browser can reach the internet, you will need to use the 'Offline Signing' tab.  ";
            }

            ?>
    <br>

    <table <?php if ($offlineMode) {
                echo "class='internetOnly' style='display: none;'";
            }
            ?>>
        <tr><td><b>Order Number:</b></td><td><input id='orderNumber' type='password' size=12 maxlength=10 value=''>
                <i class='fas fa-eye' id='orderNumberHideShow' onClick='TogglePasswordHideShow("orderNumber");'></i></td></tr>
        <tr><td><b>License Key:</b></td><td><input id='licenseKey' type='password' size=36 maxlength=34 placeholder='FPPAFK-XXXXXX-XXXXXX-XXXXXX-XXXXXX'>
                <i class='fas fa-eye' id='licenseKeyHideShow' onClick='TogglePasswordHideShow("licenseKey");'></i></td></tr>
        <tr><td></td><td><input type='button' class='buttons' value='Sign EEPROM' onClick='<?php if ($offlineMode) {
                echo "SignEEPROMViaBrowser";
            } else {
                echo "SignEEPROM";
            }
            ?>();'></td></tr>
    </table>

    <br>
    <b>NOTES:</b>
    <ul>
        <li>For capes with a physical EEPROM chip, the signature will be written to the EEPROM chip.  For capes using virtual EEPROM files, the file will be signed.  This signed virtual EEPROM file is preserved across fppos upgrades, but if you reinstall FPP from scratch on the SD card, you will need to re-sign the virtual EEPROM file.</li>
        <li>If you receive errors activating a license key, email <a href='mailto: support@falconplayer.com'>support@falconplayer.com</a> with information regarding your order number and license key.</li>
    </ul>
<?php
}
    } else {
        echo "No EEPROM info found.";
    }
    ?>

                                            </div>
                                        </div>
                                    </div>
                                </div>
<?php
if ($printSigningUI) {
        if ($offlineMode) {
            ?>
                                <div class="tab-pane fade show" id="eeprom-offline" role="tabpanel" aria-labelledby="eeprom-offline-tab">
                                    <div class="container-fluid">
                                        <div class="row">
                                            <div class="aboutAll col-md">

                                                <h3>Step #1: Download Signing Packet</h3>
                                                <table>
                                                    <tr><td><b>Order Number:</b></td><td><input id='offlineOrderNumber' type='password' size=12 maxlength=10 value=''>
                                                            <i class='fas fa-eye' id='offlineOrderNumberHideShow' onClick='TogglePasswordHideShow("offlineOrderNumber");'></i></td></tr>
                                                    <tr><td><b>License Key:</b></td><td><input id='offlineLicenseKey' type='password' size=36 maxlength=34 placeholder='FPPAFK-XXXXXX-XXXXXX-XXXXXX-XXXXXX'>
                                                            <i class='fas fa-eye' id='offlineLicenseKeyHideShow' onClick='TogglePasswordHideShow("offlineLicenseKey");'></i></td></tr>
                                                    <tr><td colspan='2'><input type='button' class='buttons' value='Download Offline Signing Packet' onClick='DownloadOfflineSigningFile();'></td></tr>
                                                </table>
                                                Clicking the download button will prompt you to save a file called 'cape-signing-<?=$settings['HostName']?>.bin'.  Some browsers may be setup to automatically save downloaded files.  If you are not prompted to save the file, check your Download directory for the file.

                                                <hr>
                                                <h3>Step #2: Get the packet signed.</h3>
                                                Switch the laptop or other device you are currently using to a network which can access the internet and go to <a href='https://shop.falconplayer.com/offline-signing' target='_blank'>https://shop.falconplayer.com/offline-signing</a> and upload the signing packet.  Return to this page after you have downloaded the signed packet and then complete step #3.  <b>NOTE:</b> The above link should open in a new tab in your browser so you can return to this tab after signing the packet.<br>
                                                <hr>
                                                <h3>Step #3: Upload signed packet back to FPP</h3>
                                                <input type="file" name="signingPacket" id="signingPacket" style='padding-left: 0px;'/><br>
                                                <input class='buttons' type='button' value='Upload Signed Packet' onClick='UploadSignedPacket();'>
                                            </div>
                                        </div>
                                    </div>
                                </div>
<?php
} else if ((!isset($settings['voucherRedeemed'])) || ($settings['voucherRedeemed'] != '1')) {
            ?>
                                <div class="tab-pane fade show" id="eeprom-voucher" role="tabpanel" aria-labelledby="eeprom-voucher-tab">
                                    <div class="container-fluid">
                                        <div class="row">
                                            <div class="aboutAll col-md">
                                                If you have a License Key voucher, you may use this page to redeem the voucher for a License Key
                                                to enable full functionality of your cape.<br>
                                                <br>
                                                <b>NOTE: If you do not already have an account at
                                                <a href='https://shop.FalconPlayer.com'>https://shop.FalconPlayer.com</a>, one will be automatically
                                                created for you using the email address and password provided.  If you already have an account,
                                                you must supply the password for that account below.
                                                </b>

                                                <table>
                                                    <tr><td><b>Voucher Number:</b></td><td><input id='voucherNumber' type='password' size=18 maxlength=16 value=''>
                                                            <i class='fas fa-eye' id='voucherNumberHideShow' onClick='TogglePasswordHideShow("voucherNumber");'></i></td></tr>
                                                    <tr><td><b>First Name:</b></td><td><input id='voucherFName' type='text' size=36 maxlength=34></td></tr>
                                                    <tr><td><b>Last Name:</b></td><td><input id='voucherLName' type='text' size=36 maxlength=34></td></tr>
                                                    <tr><td><b>Email:</b></td><td><input id='voucherEmail' type='text' size=36 maxlength=34></td></tr>
                                                    <tr><td><b>Password:</b></td><td><input id='voucherPassword' type='password' size=36 maxlength=34 value=''>
                                                            <i class='fas fa-eye' id='voucherPasswordHideShow' onClick='TogglePasswordHideShow("voucherPassword");'></i></td></tr>
                                                    <tr><td colspan='2'><input type='button' class='buttons' value='Redeem Voucher' onClick='RedeemVoucher();'></td></tr>
                                                </table>
                                            </div>
                                        </div>
                                    </div>
                                </div>
<?php
}
    }
    ?>

                                <div class="tab-pane fade show" id="eeprom-upgrade" role="tabpanel" aria-labelledby="eeprom-upgrade-tab">
                                    <h2>EEPROM Upgrade / Restore</h2>
                                    <div class="container-fluid">
                                        <div class="row">
                                            <div class="aboutLeft col-8">
                                                Select a downloadable EEPROM image or a local file to upgrade EEPROM firmware:<br>
                                                <div><b>Vendor:</b>&nbsp;
                                                    <select id='eepromVendorList' disabled onChange='eepromVendorListChanged();'>
                                                        <option value=''>-- Select Vendor --</option>
                                                    </select><br>
                                                    <b>Cape/Hat:</b>&nbsp;
                                                    <select id='eepromVendorCapes' disabled onChange='eepromVendorCapeChanged();'>
                                                        <option value=''>-- Select Cape --</option>
                                                    </select><br>
                                                    <b>Version:</b>&nbsp;
                                                    <select id='eepromVendorCapeVersions' disabled onChange='eepromVendorCapeVersionChanged();'>
                                                        <option value=''>-- Select Version --</option>
                                                    </select><br>
                                                </div>

                                                <br>
                                                <input type="file" name="firmware" id="firmware" style='padding-left: 0px;' onChange='firmwareChanged();'><br>
                                                <input type='button' class="buttons" value='Upgrade' onClick='UpgradeFirmware();' id='UpdateFirmware' disabled>
                                            </div>
                                            <div class="aboutRight col-4">
                                                Select a backup EEPROM firmware to restore:<br>
                                                <select id='backupFile' onChange='backupChanged();'>
                                                    <option value=''>-- Choose a backup EEPROM to restore --</option>
<?php
$files = scandir('/home/fpp/media/upload/', SCANDIR_SORT_DESCENDING);
    foreach ($files as $file) {
        if (preg_match('/^cape-eeprom-Backup-.*\.bin$/', $file)) {
            $restoreDisabled = '';
            printf("<option value='%s'>%s</option>\n", $file, $file);
        }
    }
    closedir($dir);
    ?>
                                                </select><br>
                                                <input type='button' class="buttons" value='Restore' onClick='RestoreFirmware();' id='RestoreFirmware' disabled style='margin-top: 0.33rem !important;'><br>
                                            </div>
                                        </div>
                                        <br>
                                        Firmware backups are automatically created whenever an EEPROM is upgraded or signed.  You may delete old
                                        backups from the <a href='uploadfile.php#tab-uploads'>File Manager</a>.
                                    </div>
                                </div>
                            </div>
                        </div>
                    </div>

                </div>
            </div>
        </div>
<?
} else {
    if ($eepromFile != '') {
        ?>
                                <div class="tab-pane fade show" id="eeprom-upgrade" role="tabpanel" aria-labelledby="eeprom-upgrade-tab">
                                    <h2>EEPROM Install</h2>
                                    <div class="container-fluid">
                                        <div class="row">
                                            <div class="aboutLeft col-md">
                                                Your cape appears to have an unprogrammed physical EEPROM installed. Select a downloadable EEPROM image or a local file to program the EEPROM:<br>
                                                <div><b>Vendor:</b>&nbsp;
                                                    <select id='eepromVendorList' disabled onChange='eepromVendorListChanged();'>
                                                        <option value=''>-- Select Vendor --</option>
                                                    </select>&nbsp;&nbsp;
                                                    <b>Cape/Hat:</b>&nbsp;
                                                    <select id='eepromVendorCapes' disabled onChange='eepromVendorCapeChanged();'>
                                                        <option value=''>-- Select Cape --</option>
                                                    </select>&nbsp;&nbsp;
                                                    <b>Version:</b>&nbsp;
                                                    <select id='eepromVendorCapeVersions' disabled onChange='eepromVendorCapeVersionChanged();'>
                                                        <option value=''>-- Select Version --</option>
                                                    </select>&nbsp;&nbsp;
                                                </div>
                                                </br>
                                                <input type="file" name="firmware" id="firmware" style='padding-left: 0px;' onChange='firmwareChanged();'><br>
                                                <input type='button' class="buttons" value='Upgrade' onClick='UpgradeFirmware(true);' id='UpdateFirmware' disabled>
                                            </div>
                                        </div>
                                    </div>
                                </div>
<?
    } else {
        ?>
    <h3>No Cape or Hat EEPROM found.</h3>

    Unable to find a physical or virtual EEPROM.  If you have a cape without a physical EEPROM installed,
    you will need to select a virtual EEPROM from the Pixel Strings Channel Output configuration page.
<?
    }
}
?>
    </div>
<?php
include 'common/footer.inc';
?>
</div>
<div id='upgradePopup' title='FPP Upgrade' style="display: none">
    <textarea style='width: 99%; height: 500px;' disabled id='upgradeText'>
    </textarea>
    <input id='closeDialogButton' type='button' class='buttons dialogCloseButton' value='Close' onClick='CloseUpgradeDialog(true);' style='display: none;'>
    <input id='errorDialogButton' type='button' class='buttons dialogCloseButton' value='Close' onClick='CloseUpgradeDialog(false);' style='display: none;'>
</div>
</body>
</html>
