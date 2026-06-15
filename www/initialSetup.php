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

    // The initial setup page is shown both on a true first boot and again after an
    // fppOS update that introduces a new required setup step. The redirect gate
    // (menuHead.inc) keys off the current completion flag (initialSetup-02); each
    // time a new step is required the flag name is bumped, so a box that completed
    // an EARLIER setup generation still carries a previous-generation flag set to 1
    // while the current one is absent. A true first boot has none of them set.
    //
    // When the box was already configured we take a configuration backup BEFORE
    // applying any changes, so the prior config can be restored if needed. A fresh
    // box has nothing worth backing up beforehand. Both cases get the single
    // after-setup backup (see finishSetup()).
    $previousSetupFlags = array('initialSetup', 'initialSetup-01');
    $alreadyConfigured = 0;
    foreach ($previousSetupFlags as $f) {
        if (isset($settings[$f]) && $settings[$f] == '1') {
            $alreadyConfigured = 1;
            break;
        }
    }
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

        function showSetupProgress(msg) {
            $('#setupProgress').removeClass('d-none').html('<i class="fas fa-spinner fa-spin"></i> ' + msg);
        }

        function hideSetupProgress() {
            $('#setupProgress').addClass('d-none').html('');
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

            // Enabling the UI password reloads apache with authentication required,
            // after which the browser's (remote) requests get 401s. So the UI
            // password settings MUST be applied LAST - after every other setting and
            // after the completion flag - otherwise those later saves would be
            // rejected and the user would be bounced back into initial setup. (The
            // server-side config backup still works after auth is enabled because it
            // runs over localhost, which the apache rule allows via 'Require local'.)
            var uiPasswordOrder = ['password', 'passwordEnable']; // write value, then enable
            // OS (system login) password: applying it runs chpasswd (yescrypt, ~3s).
            // When both the value and the enable toggle are being saved, write the
            // value WITHOUT applying it and let the single enable step run chpasswd
            // once - otherwise it runs twice (value + enable) for ~6s. Order matters:
            // the value must be written to the settings file before the enable step,
            // which reads it back to apply it.
            var osPasswordOrder = ['osPassword', 'osPasswordEnable'];
            var specialKeys = uiPasswordOrder.concat(osPasswordOrder);
            var otherKeys = [];
            var uiPasswordKeys = [];
            var osPasswordKeys = [];
            $.each(filteredSettings, function (key) {
                if (specialKeys.indexOf(key) === -1) {
                    otherKeys.push(key);
                }
            });
            $.each(uiPasswordOrder, function (i, key) {
                if (filteredSettings.hasOwnProperty(key)) {
                    uiPasswordKeys.push(key);
                }
            });
            $.each(osPasswordOrder, function (i, key) {
                if (filteredSettings.hasOwnProperty(key)) {
                    osPasswordKeys.push(key);
                }
            });

            var failedSettings = [];

            // Disable the buttons and show a spinner so the user gets immediate
            // feedback - on slower devices saving the settings can take a few seconds.
            var $finishBtn = $('#finishSetupBtn');
            $finishBtn.prop('disabled', true);
            $('#restoreSetupBtn').prop('disabled', true);

            function finishWithErrors() {
                hideSetupProgress();
                $finishBtn.prop('disabled', false);
                $('#restoreSetupBtn').prop('disabled', false);
                DialogError('Save Setting', 'Failed to save: ' + failedSettings.join(', '));
            }

            function redirectToApp() {
                var redirectURL = <?= json_encode($_GET['redirect'] ?? '') ?>;
                location.href = (redirectURL == '') ? 'index.php' : redirectURL;
            }

            // Save a single setting via PUT. skipBackup avoids the (expensive)
            // per-setting configuration backup - exactly one backup is generated by
            // the final save below so it captures the fully-configured state.
            // skipApply persists the value without running its apply side effects.
            function savePut(key, value, skipBackup, done, skipApply) {
                var params = [];
                if (skipBackup) { params.push('skipBackup=1'); }
                if (skipApply) { params.push('skipApply=1'); }
                $.ajax({
                    url: 'api/settings/' + key + (params.length ? ('?' + params.join('&')) : ''),
                    data: '' + value,
                    method: 'PUT',
                    success: function () { settings[key] = value; },
                    error: function () { failedSettings.push(key); },
                    complete: done
                });
            }

            // Save the OS (system login) password settings, then call done(). If both
            // the value and the enable toggle are present, the value is written with
            // skipApply so only the enable step runs chpasswd (once).
            function saveOsPasswordThen(done) {
                if (failedSettings.length > 0 || osPasswordKeys.length === 0) {
                    done();
                    return;
                }
                var dedupe = (osPasswordKeys.length > 1);
                function step(i) {
                    if (i >= osPasswordKeys.length) {
                        done();
                        return;
                    }
                    var key = osPasswordKeys[i];
                    var skipApply = dedupe && (key !== 'osPasswordEnable');
                    showSetupProgress('Saving OS password...');
                    savePut(key, filteredSettings[key], true, function () {
                        step(i + 1);
                    }, skipApply);
                }
                step(0);
            }

            // Save a list of keys (from filteredSettings) one at a time, async so the
            // browser can paint the progress indicator (a synchronous loop freezes the
            // UI). All use skipBackup.
            function saveSequential(list, i, label, done) {
                if (i >= list.length) {
                    done();
                    return;
                }
                var key = list[i];
                showSetupProgress(label + ' (' + (i + 1) + ' of ' + list.length + ')...');
                savePut(key, filteredSettings[key], true, function () {
                    saveSequential(list, i + 1, label, done);
                });
            }

            // After the non-password settings are saved: write the completion flag,
            // then apply the UI password LAST, then redirect.
            function saveCompletionAndPasswords() {
                if (failedSettings.length > 0) {
                    finishWithErrors();
                    return;
                }

                if (uiPasswordKeys.length === 0) {
                    // No UI password change: save the completion flag WITHOUT
                    // skipBackup so it generates the single configuration backup.
                    showSetupProgress('Creating configuration backup...');
                    savePut('initialSetup-02', '1', false, function () {
                        settings['initialSetup-02'] = '1';
                        if (failedSettings.length > 0) {
                            finishWithErrors();
                            return;
                        }
                        redirectToApp();
                    });
                    return;
                }

                // UI password is being applied. Save the completion flag first (with
                // skipBackup, while the browser is still un-authenticated); it is then
                // captured by the final backup. Then apply the UI password keys last,
                // the final one (passwordEnable) without skipBackup so it generates
                // the single backup AND enables auth as the very last action.
                showSetupProgress('Finishing setup...');
                savePut('initialSetup-02', '1', true, function () {
                    settings['initialSetup-02'] = '1';

                    function savePw(i) {
                        // Don't enable auth if an earlier (password value) save failed
                        // - that could lock the user out with an unknown password.
                        if (failedSettings.length > 0) {
                            finishWithErrors();
                            return;
                        }
                        var key = uiPasswordKeys[i];
                        var isLast = (i === uiPasswordKeys.length - 1);
                        showSetupProgress(isLast ? 'Applying UI password & creating backup...' : 'Saving UI password...');
                        savePut(key, filteredSettings[key], !isLast, function () {
                            if (isLast) {
                                // Auth is now enabled; the browser is locked out of
                                // further requests, so redirect - index.php will prompt
                                // for the new credentials.
                                redirectToApp();
                            } else {
                                savePw(i + 1);
                            }
                        });
                    }
                    savePw(0);
                });
            }

            function startSaving() {
                var afterOthers = function () {
                    saveOsPasswordThen(saveCompletionAndPasswords);
                };
                if (otherKeys.length > 0) {
                    saveSequential(otherKeys, 0, 'Saving settings', afterOthers);
                } else {
                    afterOthers();
                }
            }

            // On an already-configured box (initial setup re-run after an fppOS
            // update), back up the current configuration BEFORE applying any changes
            // so the prior config can be restored. A fresh first boot skips this.
            <? if ($alreadyConfigured) { ?>
                showSetupProgress('Backing up current configuration...');
                $.ajax({
                    url: 'api/backups/configuration',
                    type: 'POST',
                    data: 'Before FPP Setup (run after update)',
                    contentType: 'text/plain',
                    complete: function () {
                        startSaving();
                    }
                });
            <? } else { ?>
                startSaving();
            <? } ?>
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
                            <div class="d-flex gap-2 align-items-center">
                                <span id='setupProgress' class='d-none text-muted me-2'></span>
                                <input type='button' id='restoreSetupBtn' class='buttons' value='Restore from Backup'
                                    onClick='openRestoreDialog();'>
                                <input type='button' id='finishSetupBtn' class='buttons btn-success' value='Finish Setup'
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
