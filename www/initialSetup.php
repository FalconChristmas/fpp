<!DOCTYPE html>
<html lang="en">

<head>
    <?php
    include 'common/htmlMeta.inc';
    require_once('config.php');
    require_once('common.php');
    include 'common/menuHead.inc';

    $showOSSecurity = 0;
    if (file_exists('/etc/fpp/platform') && !file_exists('/.dockerenv'))
        $showOSSecurity = 1;
    ?>
    <script>
        // Store all pending setting changes
        var pendingSettings = {};

        // Generic function to track setting changes without saving
        function trackSettingChange(settingName) {
            var value = $('#' + settingName.replace(/\./g, '\\.')).val();
            pendingSettings[settingName] = value;

            // Update the settings object locally for UI consistency
            settings[settingName] = value;
        }

        // Track checkbox changes
        function trackCheckboxChange(settingName, checkedValue, uncheckedValue) {
            var value = uncheckedValue;
            if ($('#' + settingName.replace(/\./g, '\\.')).is(':checked')) {
                value = checkedValue;
            }
            pendingSettings[settingName] = value;
            settings[settingName] = value;
        }

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

            // Save all pending settings
            var settingsToSave = Object.keys(pendingSettings).length;
            var settingsSaved = 0;

            if (settingsToSave > 0) {
                $.each(pendingSettings, function (key, value) {
                    SetSetting(key, value, 0, 0, false, null, function () {
                        settingsSaved++;
                        if (settingsSaved == settingsToSave) {
                            // All settings saved, now set completion flag
                            Put('api/settings/initialSetup-02', false, '1');
                            var redirectURL = '<?= $_GET['redirect'] ?>';
                            location.href = (redirectURL == '') ? 'index.php' : redirectURL;
                        }
                    });
                });
            } else {
                // No settings changed, just set completion flag
                Put('api/settings/initialSetup-02', false, '1');
                var redirectURL = '<?= $_GET['redirect'] ?>';
                location.href = (redirectURL == '') ? 'index.php' : redirectURL;
            }
        }

        var hiddenChildren = {};
        function UpdateChildSettingsVisibility() {
            hiddenChildren = {};
            $('.parentSetting').each(function () {
                var fn = 'Update' + $(this).attr('id') + 'Children';
                window[fn](2); // Hide if necessary
            });
            $('.parentSetting').each(function () {
                var fn = 'Update' + $(this).attr('id') + 'Children';
                window[fn](1); // Show if not hidden
            });
        }

        $(document).ready(function () {
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

            // Override all auto-generated onChange handlers to track changes instead of saving immediately
            // List of all settings on this page
            var settingsToOverride = [
                'passwordEnable', 'password', 'passwordVerify',
                'osPasswordEnable', 'osPassword', 'osPasswordVerify',
                'fppMode', 'HostName', 'InstalledCape',
                'ShareCrashData', 'emailAddress', 'FetchVendorLogos', 'SendVendorSerial', 'statsPublish',
                'Locale', 'TimeZone', 'Latitude', 'Longitude'
            ];

            // Replace onChange handlers for each setting
            $.each(settingsToOverride, function (index, settingName) {
                var $elem = $('#' + settingName.replace(/\./g, '\\.'));
                if ($elem.length > 0) {
                    // Store the original onChange function if we need child visibility updates
                    var originalOnChange = window[settingName + 'Changed'];

                    // Create new onChange that tracks but doesn't save
                    window[settingName + 'Changed'] = function () {
                        var $input = $('#' + settingName.replace(/\./g, '\\.'));

                        if ($input.attr('type') === 'checkbox') {
                            var checkedValue = $input.data('checked-value') || '1';
                            var uncheckedValue = $input.data('unchecked-value') || '0';
                            var value = $input.is(':checked') ? checkedValue : uncheckedValue;
                            pendingSettings[settingName] = value;
                            settings[settingName] = value;
                        } else {
                            var value = $input.val();
                            pendingSettings[settingName] = value;
                            settings[settingName] = value;
                        }

                        // Handle child visibility updates if the setting has children
                        if (typeof window['Update' + settingName + 'Children'] === 'function') {
                            window['Update' + settingName + 'Children'](0);
                        }
                    };
                }
            });
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

                    <div id="warningsRow" class="alert alert-danger">
                        <div id="warningsTd">
                            <div id="warningsDiv"></div>
                        </div>
                    </div>
                    <div class="row tablePageHeader">
                        <div class="col-md">
                            <h2>Initial Setup</h2>
                        </div>
                        <div class="col-md-auto ms-lg-auto">
                            <div class="form-actions">
                                <input type='button' class='buttons btn-success' value='Finish Setup'
                                    onClick='finishSetup();'>
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
                        PrintSettingGroup('initialSetup', '', '', 1, '', '', false);
                        ?>
                        <b>Privacy Settings</b><br>
                        <?
                        $extraData = "<div class='form-actions'>" .
                            "<input type='button' class='buttons' value='Preview Statistics' onClick='PreviewStatistics();'> </div>";
                        PrintSettingGroup('initialSetup-privacy', $extraData, '', '', '', '', false);
                        ?>
                        <b>Location Settings</b><br>
                        <?
                        $extraData = "<div class='form-actions'>";
                        if ($settings["Platform"] != "MacOS") {
                            $extraData .= "<input type='button' class='buttons' value='Lookup Time Zone' onClick='GetTimeZone();'> ";
                        }
                        $extraData .= "<input type='button' class='buttons' value='Lookup Location' onClick='GetGeoLocation();'> " .
                            "<input type='button' class='buttons' value='Show On Map' onClick='ViewLatLon();'> " .
                            "</div>";
                        PrintSettingGroup('initialSetup-location', '', $extraData, '', '', '', false);
                        ?>

                    </div>
                </div>
            </div>
            <?php include 'common/footer.inc'; ?>
        </div>
</body>

</html>