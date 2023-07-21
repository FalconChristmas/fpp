<!DOCTYPE html>
<html>
<head>
<?php
require_once('config.php');
require_once('common.php');
include 'common/menuHead.inc';

$showOSSecurity = 0;
if (file_exists('/etc/fpp/platform') && !file_exists('/.dockerenv'))
    $showOSSecurity = 1;
?>
<script>
function finishSetup() {
    var passwordEnable = $('#passwordEnable').val();
    if (passwordEnable == '') {
        alert('You must choose to either Enable or Disable the UI password.');
        return;
    }

    if (passwordEnable == '1') {
        if (!confirm("You have chosen to use '" + settings['password'] + "' as a password for the FPP web User Interface.  This password will be required for all use of the FPP web User Interface as well as xLight's FPP Connect feature.  If you forget this password, you may be locked out of FPP and it may require a FPP reinstall to recover.  Make sure you have written down this password if it is not something you will easily remember."))
            return;
    }

<? if ($showOSSecurity) { ?>
    if ($('#osPasswordEnable').val() == '') {
        alert('You must choose to either use the default OS password or choose a custom password.');
        return;
    }
<? } ?>

    // Must match the -XX setting name in common/menuHead.inc
    Put('api/settings/initialSetup-02', false, '1');

    var redirectURL = '<?=$_GET['redirect'] ?>';
    location.href = (redirectURL == '') ? 'index.php' : redirectURL;
}

var hiddenChildren = {};
function UpdateChildSettingsVisibility() {
    hiddenChildren = {};
    $('.parentSetting').each(function() {
        var fn = 'Update' + $(this).attr('id') + 'Children';
        window[fn](2); // Hide if necessary
    });
    $('.parentSetting').each(function() {
        var fn = 'Update' + $(this).attr('id') + 'Children';
        window[fn](1); // Show if not hidden
    });
}

$( document ).ready(function() {
    var selected = '';
    if (!settings['passwordEnable'])
        selected = 'selected';
    $('#passwordEnable').prepend("<option value='' " + selected + ">-- Choose an Option --</option>");
    if ($('#passwordEnable').val() == '1') {
        $('.passwordEnableChild').show();
    }

<? if ($showOSSecurity) { ?>
    selected = '';
    if (!settings['osPasswordEnable'])
        selected = 'selected';
    $('#osPasswordEnable').prepend("<option value='' " + selected + ">-- Choose an Option --</option>");
    if ($('#osPasswordEnable').val() == '1') {
        $('.osPasswordEnableChild').show();
    }
<? } ?>

    UpdateChildSettingsVisibility();
});
</script>

<title><? echo $pageTitle; ?></title>
</head>
<body class="is-loading">
<div id="bodyWrapper">
<?php
include 'menu.inc'; ?>
  <div class="mainContainer">
    <h2 class="title d-none d-sm-block ">FPP Initial Setup</h2>
    <div class="pageContent">
        <div id="initialSetup" class="">

        <div id="warningsRow" class="alert alert-danger"><div id="warningsTd"><div id="warningsDiv"></div></div></div>
        <div class="row tablePageHeader">
            <div class="col-md">
                <h2>Initial Setup</h2>
            </div>
            <div class="col-md-auto ms-lg-auto">
                <div class="form-actions">
                    <input type='button' class='buttons btn-success' value='Finish Setup' onClick='finishSetup();'>
                </div>
            </div>
        </div>
        <hr>
        <div class='container-fluid'>
        <b>UI Password</b><br>
<?
PrintSetting('passwordEnable');
?>
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
}
?>
        <b>System Settings</b><br>
<?

$extraData = "<div class='form-actions'>" .
    "<input type='button' class='buttons' value='Preview Statistics' onClick='PreviewStatistics();'> ";
if ($settings["Platform"] != "MacOS") {
    $extraData .= "<input type='button' class='buttons' value='Lookup Time Zone' onClick='GetTimeZone();'> ";
}
$extraData .= "<input type='button' class='buttons' value='Lookup Location' onClick='GetGeoLocation();'> " .
    "<input type='button' class='buttons' value='Show On Map' onClick='ViewLatLon();'> " .
    "</div>";

PrintSettingGroup('initialSetup', $extraData, '', 1, '', '',false);
?>

        </div>
    </div>
</div>
<?php	include 'common/footer.inc'; ?>
</div>
</body>
</html>
