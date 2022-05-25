<?php
header("Access-Control-Allow-Origin: *");
?>
<!DOCTYPE html>
<html>
<?php
require_once 'common.php';
require_once 'config.php';

$APIhost = 'api.FalconPlayer.com';
if (isset($settings['SigningAPIHost']))
    $APIhost = $settings['SigningAPIHost'];

$channelOutputDriver = "";

// Set either of these to 1 for testing
$printSigningUI = 0;
$offlineMode = 0;

// Test to see if FPP can get to the signing API
$curl = curl_init("https://$APIhost/js/internetTest.js");
curl_setopt($curl, CURLOPT_HEADER, 0);
curl_setopt($curl, CURLOPT_CONNECTTIMEOUT, 2); // Connect in 10 seconds or less
curl_setopt($curl, CURLOPT_TIMEOUT, 5); // 1 Day Timeout to transfer
curl_setopt($curl, CURLOPT_USERAGENT, getFPPVersion());
curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
$request_content = curl_exec($curl);
$rc = curl_getinfo($curl, CURLINFO_RESPONSE_CODE);
curl_close($curl);

if ($rc != 200) {
    $offlineMode = 1;
}

$capeHardwareType = "Cape/Hat";
$currentCapeInfo = Array();
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
                    if (($co['type'] == 'BBB48String') || ($co['type'] == 'DPIPixels')) {
                        $channelOutputDriver = $co['type'];
                    }
                }
            }
        }
    } else {
        $path = $mediaDirectory . "/tmp/strings/";
        $files = scandir($path);
        foreach ($files as $file) {
            if (substr($file, 0, 1) != '.') {
                $json = file_get_contents($path . $file);
                $data = json_decode($json, true);
                if ((isset($data['driver'])) &&
                    (($data['driver'] == 'DPIPixels') ||
                     ($data['driver'] == 'BBB48String'))) {
                     $channelOutputDriver = $data['driver'];
                     break;
                }
            }
        }
    }

    if (($channelOutputDriver == 'BBB48String') || ($channelOutputDriver == 'DPIPixels')) {
        $channelOutputDriverStr = "  This cape uses the $channelOutputDriver Channel Output driver.";
        if ($currentCapeInfo['serialNumber'] == 'FPP-INTERNAL') {
            if (isset($currentCapeInfo['verifiedKeyId'])) {
                $signingStatus = sprintf( "Cape is using a virtual EEPROM signed with the '%s' key and the %s Channel Output driver.", $currentCapeInfo['verifiedKeyId'], $channelOutputDriver);
                if ($currentCapeInfo['verifiedKeyId'] == 'fp') {
                    $printSigningUI = 1;
                    if ((isset($currentCapeInfo['signed']['licensePorts'])) && ($currentCapeInfo['signed']['licensePorts'] >= 9999)) {
                        $signingStatus .= "  This cape is already licensed for an unlimited number of outputs.  The signing inputs are shown below in case the cape needs to be re-signed.";
                    }
                }
            } else {
                $signingStatus = sprintf( "Cape is using an <b>Unsigned</b> virtual EEPROM and the $channelOutputDriver Channel Output driver.");
                $printSigningUI = 1;
            }
        } else if (isset($currentCapeInfo['verifiedKeyId'])) {
            $signingStatus = sprintf( "Cape EEPROM is signed using the '%s' key and uses the %s Channel Output driver.", $currentCapeInfo['verifiedKeyId'], $channelOutputDriver);
            if ($currentCapeInfo['verifiedKeyId'] == 'fp') {
                $printSigningUI = 1;
                if ((isset($currentCapeInfo['signed']['licensePorts'])) && ($currentCapeInfo['signed']['licensePorts'] >= 9999)) {
                    $signingStatus .= "  This cape is already licensed for an unlimited number of outputs.  The signing inputs are shown below in case the cape needs to be re-signed.";
                }
            }
        } else {
            $signingStatus = sprintf( "Cape is using an <b>Unsigned</b> EEPROM and the $channelOutputDriver Channel Output driver.");
            $printSigningUI = 1;
        }
    }
}

?>

<head>
<?php
include 'common/menuHead.inc';
?>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
<script language='Javascript' src='https://<?=$APIhost ?>/js/internetTest.js?ref=<?php echo time(); ?>'></script>
<script language="Javascript">

function CloseUpgradeDialog(reload = false) {
    $('#upgradePopup').fppDialog('close');
    if (reload)
        location.reload();
}

function RestoreFirmwareDone() {
    var txt = $('#upgradeText').val();
    if (txt.includes("Cape does not match new firmware")) {
        var arrayOfLines = txt.match(/[^\r\n]+/g);
        var msg = "Are you sure you want to replace the firmware for cape:\n" + arrayOfLines[1] + "\n\nWith the firmware for: \n" + arrayOfLines[2] + "\n";
        if (confirm(msg)) {
            var filename = $('#backupFile').val();
            $('#upgradeText').html('');
            StreamURL('upgradeCapeFirmware.php?force=true&filename=' + filename, 'upgradeText', 'UpgradeDone', 'UpgradeDone', 'GET', null, false, false);
        }
    }
    $('#closeDialogButton').show();
}

function RestoreFirmware() {
    var filename = $('#backupFile').val();

    $('.dialogCloseButton').hide();
    $('#upgradePopup').fppDialog({ height: 600, width: 900, title: "Restore Cape Firmware", dialogClass: 'no-close' });
    $('#upgradePopup').fppDialog( "moveToTop" );
    $('#upgradeText').html('');
    StreamURL('upgradeCapeFirmware.php?filename=' + filename, 'upgradeText', 'RestoreFirmwareDone', 'RestoreFirmwareDone', 'GET', null, false, false);
}

function UpgradeDone() {
    $('#closeDialogButton').show();
}

function UpgradeFirmwareDone() {
    var txt = $('#upgradeText').val();
    if (txt.includes("Cape does not match new firmware")) {
        var arrayOfLines = txt.match(/[^\r\n]+/g);
        var msg = "Are you sure you want to replace the firmware for cape:\n" + arrayOfLines[1] + "\n\nWith the firmware for: \n" + arrayOfLines[2] + "\n";
        if (confirm(msg)) {
            let formData = new FormData();
            let firmware = document.getElementById("firmware").files[0];
            formData.append("firmware", firmware);

            $('#upgradeText').html('');
            StreamURL('upgradeCapeFirmware.php?force=true', 'upgradeText', 'UpgradeDone', 'UpgradeDone', 'POST', formData, false, false);
        }
    }
    $('#closeDialogButton').show();
}
function UpgradeFirmware() {
    let firmware = document.getElementById("firmware").files[0];
    if (firmware == "" || firmware == null) {
        return;
    }
    let formData = new FormData();
    formData.append("firmware", firmware);

    $('.dialogCloseButton').hide();
    $('#upgradePopup').fppDialog({ height: 600, width: 900, title: "Upgrade Cape Firmware", dialogClass: 'no-close' });
    $('#upgradePopup').fppDialog( "moveToTop" );
    $('#upgradeText').html('');
    StreamURL('upgradeCapeFirmware.php', 'upgradeText', 'UpgradeFirmwareDone', 'UpgradeFirmwareDone', 'POST', formData, false, false);
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
                    url: 'https://<?=$APIhost ?>/api/fpp/eeprom/sign',
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
    var url = 'api/cape/eeprom/sign/' + key + '/' + order;

    $.ajax({
        url: url,
        type: 'POST',
        data: '',
        async: true,
        dataType: 'json',
        success: function (data) {
            if (data.Status == 'OK') {
                reloadPage();
            } else {
                alert('ERROR signing EEPROM: ' + data.Message);
            }
        },
        error: function () {
            $.jGrowl('Error calling signing API', { themeState: 'danger' });
        }
    });
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
        <div class="title"><?=$capeHardwareType ?> Info</div>
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
                                    <a class="nav-link active" id="eeprom-about-tab" data-toggle="pill" href="#eeprom-about" role="tab" aria-controls="eeprom-about" aria-selected="true">
                                        About
                                    </a>
                                </li>
                                <li class='nav-item'>
                                    <a class="nav-link" id="eeprom-signature-tab" data-toggle="pill" href="#eeprom-signature" role="tab" aria-controls="eeprom-signature">
                                        EEPROM Signature
                                    </a>
                                </li>
<?php
    if ($printSigningUI && $offlineMode) {
?>
                                <li class='nav-item'>
                                    <a class="nav-link" id="eeprom-offline-tab" data-toggle="pill" href="#eeprom-offline" role="tab" aria-controls="eeprom-offline">
                                        Offline Signing
                                    </a>
                                </li>
<?php
    }
?>
                                <li class='nav-item'>
                                    <a class="nav-link" id="eeprom-upgrade-tab" data-toggle="pill" href="#eeprom-upgrade" role="tab" aria-controls="eeprom-upgrade">
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
    if (isset($currentCapeInfo['verifiedKeyId'])) {
        echo "<tr><td><b>Key ID:</b></td><td>" . $currentCapeInfo['verifiedKeyId'] . "</td></tr>";
    }
    if ($channelOutputDriver != '') {
        echo "<tr><td><b>Channel&nbsp;Output:</b></td><td>" . $channelOutputDriver . "</td></tr>";
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
            echo "<tr><td><b>Vendor&nbsp;URL:</b></td><td><a href=\"" . $landing . "\">" . $url . "</a></td></tr>";
        }
        if (isset($currentCapeInfo['vendor']['phone'])) {
            echo "<tr><td><b>Phone&nbsp;Number:</b></td><td>" . $currentCapeInfo['vendor']['phone'] . "</td></tr>";
        }
        if (isset($currentCapeInfo['vendor']['email'])) {
            echo "<tr><td><b>E-mail:</b></td><td><a href=\"mailto:" . $currentCapeInfo['vendor']['email'] . "\">" . $currentCapeInfo['vendor']['email'] . "</td></tr>";
        }
        if (isset($currentCapeInfo['vendor']['forum'])) {
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
            if ($outputs == 9999)
                $outputs = "Unlimited";
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
            if (isset($currentCapeInfo['signed']['capeSerial']))
                echo "<tr><td><b>Licensed Serial:</b></td><td>" . $currentCapeInfo['signed']['capeSerial'] . "</td></tr>";
            if (isset($currentCapeInfo['signed']['deviceSerial']))
                echo "<tr><td><b>Licensed Device:</b></td><td>" . $currentCapeInfo['signed']['deviceSerial'] . "</td></tr>";
        }

        if (isset($currentCapeInfo['signed']['signTime'])) {
            echo "<tr><td><b>EEPROM Signed at:</b></td><td>" . date('Y-m-d h:i:s A T', $currentCapeInfo['signed']['signTime']) . "</td></tr>";
        }

        $printBreak = 1;
    }
?>
    </table>
<?php
    if ($printBreak)
        echo "<br>\n";

    if ($printSigningUI) {
        if (!isset($currentCapeInfo['verifiedKeyId']))
            echo "<b>NOTE: This cape is using an unsigned EEPROM and the $channelOutputDriver Channel Output driver which operates in a limited manner until the EEPROM is signed.  Pixel outputs will be limited to 50 pixels per output and smart receivers will be disabled.</b><br><br>\n";
?>
    You may sign the EEPROM using a license key purchased from <a href='https://shop.FalconPlayer.com' target='_blank'>https://shop.FalconPlayer.com</a>.  Once you have purchased a license key, enter the Order Number and License Key below.
<?php
        if ($offlineMode)
            echo "If <b>neither</b> your FPP instance or your browser can reach the internet, you will need to use the 'Offline Signing' tab.  ";
?>
        See the <b>NOTES</b> section below for more information.<br>
    <br>

    <table <?php if ($offlineMode) echo "class='internetOnly' style='display: none;'"; ?>>
        <tr><td><b>Order Number:</b></td><td><input id='orderNumber' type='password' size=12 maxlength=10 value=''>
                <i class='fas fa-eye' id='orderNumberHideShow' onClick='TogglePasswordHideShow("orderNumber");'></i></td></tr>
        <tr><td><b>License Key:</b></td><td><input id='licenseKey' type='password' size=36 maxlength=34 placeholder='FPPAFK-XXXXXX-XXXXXX-XXXXXX-XXXXXX'>
                <i class='fas fa-eye' id='licenseKeyHideShow' onClick='TogglePasswordHideShow("licenseKey");'></i></td></tr>
        <tr><td></td><td><input type='button' class='buttons' value='Sign EEPROM' onClick='<?php if ($offlineMode) echo "SignEEPROMViaBrowser"; else echo "SignEEPROM"; ?>();'></td></tr>
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
if ($printSigningUI && $offlineMode) {
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
                                                Clicking the download button will prompt you to save a file called 'cape-signing-<?=$settings['HostName'] ?>.bin'.

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
}
?>

                                <div class="tab-pane fade show" id="eeprom-upgrade" role="tabpanel" aria-labelledby="eeprom-upgrade-tab">
                                    <h2>EEPROM Upgrade / Restore</h2>
                                    <div class="container-fluid">
                                        <div class="row">
                                            <div class="aboutLeft col-md">
                                                Select a local file to upgrade EEPROM firmware:<br>
                                                <input type="file" name="firmware" id="firmware"/ style='padding-left: 0px;'><br>
                                                <input type='button' class="buttons" value='Upgrade' onClick='UpgradeFirmware();' id='UpdateFirmware'>
                                            </div>
                                            <div class="aboutRight col-md">
                                                Select a backup EEPROM firmware to restore:<br>
                                                <select id='backupFile'>
<?php
$files = scandir('/home/fpp/media/upload/', SCANDIR_SORT_DESCENDING);
$restoreDisabled = 'disabled';

foreach ($files as $file) {
    if (preg_match('/^cape-eeprom-Backup-.*\.bin$/', $file)) {
        $restoreDisabled = '';
        printf( "<option value='%s'>%s</option>\n", $file, $file);
    }
}
closedir($dir);
?>
                                                </select><br>
                                                <input type='button' class="buttons" value='Restore' onClick='RestoreFirmware();' id='RestoreFirmware' <?php echo $restoreDisabled; ?>><br>
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
?>
    <h3>No Cape or Hat found.</h3>

    Does your cape have a physical EEPROM or did you install a virtual EEPROM on the <a href='initialSetup.php'>Initial Setup</a> page?
<?
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
