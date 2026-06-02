<?php
// Handle configuration restore from uploaded backup file
if ($_SERVER['REQUEST_METHOD'] === 'POST' && isset($_FILES['restoreFile'])) {
    // Suppress errors and buffer any accidental output so we always return clean JSON
    error_reporting(0);
    ini_set('display_errors', 0);
    ob_start();

    $skipJSsettings = 1;
    require_once __DIR__ . '/config.php';
    require_once __DIR__ . '/common.php';

    // Ensure we have a valid config directory
    $configDir = isset($settings['configDirectory']) ? $settings['configDirectory'] : ($settings['mediaDirectory'] ?? '/home/fpp/media') . '/config';

    header('Content-Type: application/json');

    $file = $_FILES['restoreFile'];
    if ($file['error'] !== UPLOAD_ERR_OK) {
        ob_end_clean();
        echo json_encode(['Status' => 'Error', 'Message' => 'File upload failed (error code: ' . $file['error'] . ')']);
        exit;
    }

    $backupDir = $configDir . '/backups';
    if (!is_dir($backupDir)) {
        @mkdir($backupDir, 0755, true);
    }

    $filename = sanitizeFilename($file['name']);
    $destPath = $backupDir . '/' . $filename;

    if (!move_uploaded_file($file['tmp_name'], $destPath)) {
        ob_end_clean();
        echo json_encode(['Status' => 'Error', 'Message' => 'Failed to save uploaded file']);
        exit;
    }

    require_once __DIR__ . '/backup.php';

    $content = file_get_contents($destPath);
    if ($content === false) {
        ob_end_clean();
        echo json_encode(['Status' => 'Error', 'Message' => 'Failed to read uploaded file']);
        exit;
    }

    $data = json_decode($content, true);
    if ($data === null) {
        ob_end_clean();
        echo json_encode(['Status' => 'Error', 'Message' => 'Invalid JSON in backup file']);
        exit;
    }

    $result = doRestore('all', $data, $filename, true, true, 'api');

    ob_end_clean();

    if (isset($result['success']) && $result['success']) {
        @WriteSettingToFile('initialSetup-02', '1');
        @WriteSettingToFile('rebootFlag', '1');
        echo json_encode(['Status' => 'OK', 'Message' => 'Configuration restored successfully. A reboot is required for changes to take effect.']);
    } else {
        $errorMsg = is_array($result['message']) ? json_encode($result['message']) : $result['message'];
        echo json_encode(['Status' => 'Error', 'Message' => 'Restore failed: ' . $errorMsg]);
    }
    exit;
}
?>
<!DOCTYPE html>
<html lang="en">

<head>
    <style>
        #dropZone {
            cursor: pointer;
            border-style: dashed !important;
            min-height: 72px;
            display: flex;
            align-items: center;
            justify-content: center;
            transition: background-color 0.2s;
        }
        #dropZone.drag-over {
            background-color: var(--fpp-bg-card-hover, rgba(0,0,0,0.05));
        }
        #restoreSection .btn, #restoreSection small {
            font-size: 0.875rem;
        }
    </style>
    <?php
    include 'common/htmlMeta.inc';
    require_once('config.php');
    require_once('common.php');
    include 'common/menuHead.inc';

    $showOSSecurity = 0;
    if (file_exists('/etc/fpp/platform') && !file_exists('/etc/fpp/container'))
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

        // Restore from backup functions
        function handleRestoreFileSelect(input) {
            var file = input.files[0];
            if (file) {
                $('#restoreFileName').text(file.name);
                $('#restoreFileInfo').removeClass('d-none');
                $('#uploadRestoreBtn').prop('disabled', false);
                $('#restoreStatus').addClass('d-none');
            }
        }

        function clearRestoreFile() {
            $('#restoreFileInput').val('');
            $('#restoreFileInfo').addClass('d-none');
            $('#uploadRestoreBtn').prop('disabled', true);
            $('#restoreStatus').addClass('d-none');
        }

        function setupDropZone() {
            var dropZone = document.getElementById('dropZone');
            if (!dropZone) return;

            dropZone.addEventListener('dragover', function (e) {
                e.preventDefault();
                e.stopPropagation();
                $(this).addClass('drag-over');
            });

            dropZone.addEventListener('dragleave', function (e) {
                e.preventDefault();
                e.stopPropagation();
                $(this).removeClass('drag-over');
            });

            dropZone.addEventListener('drop', function (e) {
                e.preventDefault();
                e.stopPropagation();
                $(this).removeClass('drag-over');

                var files = e.dataTransfer.files;
                if (files.length > 0) {
                    var input = document.getElementById('restoreFileInput');
                    input.files = files;
                    handleRestoreFileSelect(input);
                }
            });

            $('#restoreFileInput').on('change', function () {
                handleRestoreFileSelect(this);
            });
        }

        function uploadRestoreFile() {
            var file = $('#restoreFileInput')[0].files[0];
            if (!file) return;

            var formData = new FormData();
            formData.append('restoreFile', file);

            $('#uploadRestoreBtn').prop('disabled', true).html('<i class="fas fa-spinner fa-spin"></i> Restoring...');
            $('#restoreStatus').removeClass('d-none alert-success alert-danger alert-info').addClass('alert alert-info').html('Uploading and restoring configuration...');

            $.ajax({
                url: 'initialSetup.php',
                type: 'POST',
                data: formData,
                processData: false,
                contentType: false,
                dataType: 'json',
                success: function (response) {
                    if (response.Status === 'OK') {
                        $('#restoreDialog').fppDialog('close');
                        Put('api/settings/initialSetup-02', false, '1');
                        setTimeout(function () {
                            location.href = 'index.php';
                        }, 500);
                    } else {
                        $('#uploadRestoreBtn').prop('disabled', false).html('<i class="fas fa-upload"></i> Upload & Restore');
                        $('#restoreStatus').removeClass('alert-info').addClass('alert-danger').html(
                            '<i class="fas fa-exclamation-triangle"></i> ' + response.Message
                        );
                    }
                },
                error: function (xhr, status, error) {
                    $('#uploadRestoreBtn').prop('disabled', false).html('<i class="fas fa-upload"></i> Upload & Restore');
                    var msg = 'Upload failed: ' + error;
                    if (xhr.responseText && xhr.responseText.indexOf('<!') === 0) {
                        msg = 'Server returned the page HTML instead of JSON. The PHP handler may not have been triggered. Check web server error logs.';
                    } else if (xhr.responseText) {
                        msg = 'Server response: ' + xhr.responseText.substring(0, 200);
                    }
                    $('#restoreStatus').removeClass('alert-info').addClass('alert-danger').html(
                        '<i class="fas fa-exclamation-triangle"></i> ' + msg
                    );
                }
            });
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

            var osPasswordEnable = $('#osPasswordEnable').val();

            // Password verify fields are UI-only confirmation values.
            // If UI/OS password auth is disabled, ignore password field writes even if
            // browser autofill or prior UI events populated pendingSettings.
            var filteredSettings = {};
            $.each(pendingSettings, function (key, value) {
                if (key !== 'passwordVerify' && key !== 'osPasswordVerify') {
                    // During initial setup, disabled password auth should not try to
                    // persist password-related fields at all.
                    if (key === 'passwordEnable' && passwordEnable !== '1') {
                        return;
                    }
                    if (key === 'osPasswordEnable' && osPasswordEnable !== '1') {
                        return;
                    }
                    filteredSettings[key] = value;
                }
            });

            // Save all pending settings
            var settingsToSave = Object.keys(filteredSettings).length;
            var settingsSaved = 0;
            var failedSettings = [];

            function completeSetup() {
                if (failedSettings.length > 0) {
                    DialogError('Save Setting', 'Failed to save: ' + failedSettings.join(', '));
                    return;
                }

                Put('api/settings/initialSetup-02', false, '1');
                var redirectURL = <?= json_encode($_GET['redirect'] ?? '') ?>;
                location.href = (redirectURL == '') ? 'index.php' : redirectURL;
            }

            if (settingsToSave > 0) {
                $.each(filteredSettings, function (key, value) {
                    SetSetting(key, value, 0, 0, false, null, function () {
                        settingsSaved++;
                        if (settingsSaved == settingsToSave) {
                            completeSetup();
                        }
                    }, function () {
                        failedSettings.push(key);
                        settingsSaved++;
                        if (settingsSaved == settingsToSave) {
                            completeSetup();
                        }
                    });
                });
            } else {
                // No settings changed, just set completion flag
                completeSetup();
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
            setupDropZone();

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
                            <div class="d-flex gap-2">
                                <input type='button' class='buttons' value='Restore from Backup'
                                    onClick='openRestoreDialog();'>
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

        <div id="restoreDialog" class="modal fade" tabindex="-1" role="dialog">
            <div class="modal-dialog" role="document">
                <div class="modal-content">
                    <div class="modal-header">
                        <h5 class="modal-title"><i class="fas fa-upload"></i> Restore from Backup</h5>
                        <button type="button" class="btn-close" data-bs-dismiss="modal" aria-label="Close"></button>
                    </div>
                    <div class="modal-body">
                        <p class="text-muted small mb-2">Upload a JSON backup file to restore all settings. All current settings will be overwritten.</p>
                        <div id="dropZone" class="border border-primary rounded text-center py-3 px-2"
                            onclick="document.getElementById('restoreFileInput').click()">
                            <div>
                                <div><i class="fas fa-cloud-upload-alt fa-lg text-primary"></i></div>
                                <p class="mb-0 small fw-bold">Drag file here or click to browse</p>
                            </div>
                        </div>
                        <input type="file" id="restoreFileInput" accept=".json" class="d-none">
                        <div id="restoreFileInfo" class="d-none mt-1">
                            <span class="text-success small"><i class="fas fa-check-circle"></i> <span id="restoreFileName" class="fw-bold"></span></span>
                            <button type="button" class="btn btn-sm btn-outline-secondary py-0 px-1 ms-1" onclick="clearRestoreFile()"><i class="fas fa-times"></i></button>
                        </div>
                        <div id="restoreStatus" class="mt-1 d-none"></div>
                    </div>
                    <div class="modal-footer">
                        <button type="button" class="buttons" data-bs-dismiss="modal">Cancel</button>
                        <button type="button" id="uploadRestoreBtn" class="buttons btn-success" disabled onclick="uploadRestoreFile()">
                            <i class="fas fa-upload"></i> Upload &amp; Restore
                        </button>
                    </div>
                </div>
            </div>
        </div>

        <script>
        function openRestoreDialog() {
            clearRestoreFile();
            $('#restoreDialog').fppDialog('open');
            setupDropZone();
        }
        </script>
</body>

</html>
