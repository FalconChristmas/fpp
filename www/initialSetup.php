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

    //A backup copied off a box by hand can reference config stored out of line;
    //resolve against this box's blob store before decoding.  A backup downloaded
    //through the UI is already whole and passes straight through.
    $blob_error = '';
    $content = InlineBackupBlobs($content, GetBackupBlobDir(GetDirSetting('JsonBackups')), $blob_error);
    if ($content === false) {
        ob_end_clean();
        echo json_encode(['Status' => 'Error', 'Message' => 'Incomplete backup file: ' . $blob_error]);
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
        .fileCopyFields label { font-weight: 600; }
        #fcFlagsTable td { vertical-align: top; white-space: nowrap; }
    </style>
    <?php
    include 'common/htmlMeta.inc';
    require_once('config.php');
    require_once('common.php');
    require_once('jurisdiction.inc');
    include 'common/menuHead.inc';

    // Populates $settingInfos / $settingGroups. PrintSettingGroup() calls this
    // too, but not until the markup below - and the script block above it needs
    // to know which settings declare a reboot.
    LoadSettingInfos();
    global $settingInfos, $settingGroups;

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

    $backupHosts = getKnownFPPSystems();
    $hostOptions = '';
    if (!empty($backupHosts)) {
        foreach ($backupHosts as $desc => $addr) {
            $hostOptions .= '<option value="' . htmlspecialchars($addr) . '">' . htmlspecialchars($desc) . '</option>';
        }
    }
    ?>
    <script src="js/fpp-backup-filecopy.js?ref=<?= filemtime('js/fpp-backup-filecopy.js'); ?>"></script>
    <script>
        fppFileCopy.config = {
            direction: '#fileCopyDirection',
            usbDevice: '#fcUSBDevice',
            pathSelect: '#fcPathSelect',
            host: '#fcHost',
            remoteStorage: '#fcRemoteStorage',
            deleteExtra: '#fcDeleteExtra',
            copyText: '#fileCopyText',
            showUSB: '.fcUSB',
            showHost: '.fcHost',
            showHostDevice: '.fcHostDevice',
            showPathSelect: '.fcPathSelect',
            showPath: '.fcPath',
            showBackups: '.fcBackups',
            showCompressed: '.fcCompressed',
            popupModalId: 'fileCopyPopup_Modal',
            popupCloseBtnId: 'fileCopyPopup_ModalCloseButton',
            copyPopupBodyId: 'fileCopyPopup'
        };

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

        // File Copy Restore functions
        function fileCopyDirectionChanged() {
            fppFileCopy.directionChanged();
        }

        function GetBackupDevices() {
            fppFileCopy.getBackupDevices();
        }

        function GetBackupDeviceDirectories() {
            fppFileCopy.getBackupDeviceDirectories();
        }

        function USBDeviceChanged() {
            fppFileCopy.usbDeviceChanged();
        }

        function GetBackupDirsViaAPI(host, remoteStorageSelected, excludeRoot) {
            fppFileCopy.getBackupDirsViaAPI(host, remoteStorageSelected, excludeRoot);
        }

        function GetBackupHostBackupDirs(remoteStorageSelected) {
            fppFileCopy.getBackupHostBackupDirs(remoteStorageSelected);
        }

        function GetRemoteHostUSBStorage() {
            fppFileCopy.getRemoteHostUSBStorage();
        }

        function PopulateBackupDirs(data, excludeRoot) {
            fppFileCopy.populateBackupDirs(data, excludeRoot);
        }

        function GetFileCopyFlags() {
            var flags = '';
            $('.fcFlag:checked').each(function () {
                flags += ' ' + $(this).val();
            });
            return flags.trim();
        }

        function PerformFileCopyRestore() {
            var direction = $('#fileCopyDirection').val();
            var host = $('#fcHost').val();
            var remoteStorage = $('#fcRemoteStorage').val();
            var path = $('#fcPathSelect').val();
            var flags = GetFileCopyFlags();

            if (!path) {
                DialogError('Restore Failed', 'No backup path specified');
                return;
            }
            if (!flags) {
                DialogError('Restore Failed', 'No items selected to restore');
                return;
            }

            var storageLocation;
            var url = 'copystorage.php?wrapped=1&direction=' + direction;

            if (direction == 'FROMUSB') {
                storageLocation = $('#fcUSBDevice').val();
                if (!storageLocation || storageLocation == 'none') {
                    DialogError('Restore Failed', 'No USB device selected');
                    return;
                }
            } else if (direction == 'FROMREMOTE') {
                if (!host) {
                    DialogError('Restore Failed', 'No remote host specified');
                    return;
                }
                if (remoteStorage && remoteStorage !== 'none') {
                    url += '&remoteStorage=' + remoteStorage;
                }
                storageLocation = host;
            } else {
                storageLocation = settings['mediaDirectory'] + '/backups';
            }

            url += '&path=' + path;
            url += '&storageLocation=' + storageLocation;
            url += '&flags=' + flags;
            url += '&delete=' + ($('#fcDeleteExtra').is(':checked') ? 'yes' : 'no');
            url += '&compress=no';

            if (!confirm("Confirm File restore of '" + flags + "' from " + storageLocation + "?\n\nWARNING: This will overwrite any current files with the copies being restored")) {
                return;
            }

            $('#restoreDialog').fppDialog('close');

            var titleTxt = 'FPP File Copy Restore';
            DoModalDialog({
                id: 'fileCopyPopup_Modal',
                title: titleTxt,
                height: 600,
                width: 900,
                autoResize: true,
                closeOnEscape: false,
                backdrop: true,
                body: $('#fileCopyPopup').html(),
                class: 'no-close modal-dialog-scrollable',
                buttons: {
                    Close: {
                        text: 'Please Wait',
                        disabled: true,
                        id: 'fileCopyPopup_ModalCloseButton',
                        class: 'btn-secondary',
                        click: function () {
                            CloseFileCopyDialog();
                            window.location.href = '/';
                        }
                    }
                }
            });

            $('#fileCopyPopup_ModalCloseButton').prop('disabled', true).text('Please Wait');
            $('#fileCopyText').val('');
            StreamURL(url, 'fileCopyText', 'FileCopyDone', 'FileCopyTimeoutError');
        }

        function CloseFileCopyDialog() {
            fppFileCopy.closeCopyDialog();
        }

        function FileCopyDone() {
            fppFileCopy.copyDone();
        }

        function FileCopyTimeoutError() {
            fppFileCopy.copyTimeoutError();
        }

        function showSetupProgress(msg) {
            $('#setupProgress').removeClass('d-none').html('<i class="fas fa-spinner fa-spin"></i> ' + msg);
        }

        function hideSetupProgress() {
            $('#setupProgress').addClass('d-none').html('');
        }

        // ---------------------------------------------------------------
        // Wizard navigation
        //
        // Nothing here persists anything.  The page already routes every
        // tracked control's onChange into pendingSettings instead of saving
        // (see the settingsToOverride block below), so steps are pure
        // show/hide and Back costs nothing.  Only finishSetup() writes.
        // ---------------------------------------------------------------
        var setupCurrentStep = 1;
        var setupTotalSteps = 4;

        // The SSH key box is pre-filled with the keys already installed, so this
        // is what "unchanged" means at Finalize.
        var originalSSHKeys = '';

        // What the cape's defaultSettings would do under the declared
        // jurisdiction, from the last api/cape/defaults call.  Refusals are kept
        // so step 3 can say why a setting is not on offer rather than leaving an
        // unexplained gap.
        var capeRefusals = {};
        // Whether the declared jurisdiction requires consent before transmitting.
        // Decided server-side from etc/jurisdictions.json so this page and cape
        // detection cannot disagree; seeded here for the case where the user
        // never touches step 1.
        var setupPriorOptIn = <?= json_encode(jurisdictionRequiresPriorOptIn()) ?>;

        function showSetupStep(n) {
            setupCurrentStep = n;
            $('.setupStep').addClass('d-none');
            $('.setupStep[data-step="' + n + '"]').removeClass('d-none');
            $('.setupStepTab').removeClass('active');
            $('.setupStepTab[data-step="' + n + '"]').addClass('active');

            $('#setupBackBtn').prop('disabled', n === 1);
            var last = (n === setupTotalSteps);
            $('#setupNextBtn').toggleClass('d-none', last);
            $('#finishSetupBtn').toggleClass('d-none', !last);

            // The password fields are children of a parent select, and their
            // visibility is computed once at load - when their step was hidden.
            // Recompute on arrival or an enabled password shows no field.
            UpdateChildSettingsVisibility();
        }

        function setupStepBusy(msg) {
            if (msg) {
                $('#setupStepBusy').removeClass('d-none').html('<i class="fas fa-spinner fa-spin"></i> ' + msg);
            } else {
                $('#setupStepBusy').addClass('d-none').html('');
            }
            $('#setupNextBtn').prop('disabled', !!msg);
            $('#setupBackBtn').prop('disabled', !!msg || setupCurrentStep === 1);
        }

        // Validation belongs to the step that owns the field.  Validating at
        // Finish instead would send someone back three steps to fix a typo.
        function validateSetupStep(n) {
            if (n === 1) {
                var $lj = $('#LegalJurisdiction');
                if ($lj.length > 0 && $lj.val() === '') {
                    alert('Please select which privacy rules apply where this player is used.  ' +
                        'It only decides whether the privacy questions on step 3 start unanswered ' +
                        'or pre-filled, and you can change any of them either way.');
                    return false;
                }
                var lat = $('#Latitude').val();
                var lon = $('#Longitude').val();
                if (lat !== '' && isNaN(parseFloat(lat))) {
                    alert('Latitude must be a number.');
                    return false;
                }
                if (lon !== '' && isNaN(parseFloat(lon))) {
                    alert('Longitude must be a number.');
                    return false;
                }
                return true;
            }
            if (n === 2) {
                var host = $('#HostName').val();
                if (host === '' || !/^([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\-]*[a-zA-Z0-9])$/.test(host)) {
                    alert('Host names must contain only letters, numbers and hyphens, and must begin and end with a letter or number.');
                    return false;
                }
                return true;
            }
            if (n === 3) {
                // Under a prior-opt-in regime these start unanswered and an
                // answer is required.  Elsewhere they are pre-filled and moving
                // on accepts what is shown -- which is a choice the user can see,
                // not a default nobody was shown.
                if (setupPriorOptIn) {
                    var unanswered = [];
                    $.each(['statsPublish', 'ShareCrashData'], function (i, key) {
                        var $e = $('#' + key);
                        if ($e.length > 0 && $e.val() === '') {
                            unanswered.push(key);
                        }
                    });
                    if (unanswered.length > 0) {
                        $('#privacyRequiredNote').removeClass('d-none');
                        return false;
                    }
                }
                $('#privacyRequiredNote').addClass('d-none');
                // Record every privacy key, touched or not.  Only pendingSettings
                // is saved, so a step whose defaults were accepted wholesale would
                // otherwise write nothing at all and leave no record of the answer.
                $.each(['statsPublish', 'ShareCrashData', 'emailAddress', 'FetchVendorLogos', 'SendVendorSerial'], function (i, key) {
                    var $e = $('#' + key);
                    if ($e.length === 0) {
                        return;
                    }
                    if ($e.attr('type') === 'checkbox') {
                        pendingSettings[key] = $e.is(':checked') ? ($e.data('checked-value') || '1') : ($e.data('unchecked-value') || '0');
                    } else {
                        pendingSettings[key] = $e.val();
                    }
                    settings[key] = pendingSettings[key];
                });
                return true;
            }
            if (n === 4) {
                var pwEnable = $('#passwordEnable').val();
                if (pwEnable === '') {
                    alert('You must choose to either Enable or Disable the UI password.');
                    return false;
                }
                if (pwEnable === '1' && $('#password').val() !== $('#passwordVerify').val()) {
                    alert('The UI password and its confirmation do not match.');
                    return false;
                }
                <? if ($showOSSecurity) { ?>
                    if ($('#osPasswordEnable').val() === '') {
                        alert('You must choose to either use the default OS password or choose a custom password.');
                        return false;
                    }
                    if ($('#osPasswordEnable').val() === '1' && $('#osPassword').val() !== $('#osPasswordVerify').val()) {
                        alert('The OS password and its confirmation do not match.');
                        return false;
                    }
                <? } ?>
                return true;
            }
            return true;
        }

        // Ask what the cape's defaults would do under the declared jurisdiction.
        //
        // Cape detection ran from fppinit at boot, before anyone had been asked
        // where they are, so any telemetry default the cape wanted was held.  The
        // answer from step 1 may now permit it - but a real re-run would write to
        // the settings file mid-wizard, which is exactly what this page does not
        // do.  So it asks instead, and merges the answer into pendingSettings.
        function refreshCapeDefaults(done) {
            var regime = $('#LegalJurisdiction').val() || '';
            // Record it here rather than relying on the control's change handler
            // having fired: this is the value the rest of the wizard is about to
            // be shaped by, so it is the one that has to be saved.
            if (regime !== '') {
                pendingSettings['LegalJurisdiction'] = regime;
                settings['LegalJurisdiction'] = regime;
            }
            setupStepBusy('Checking cape defaults...');
            $.ajax({
                url: 'api/cape/defaults?regime=' + encodeURIComponent(regime),
                dataType: 'json',
                success: function (result) {
                    if (result && typeof result.priorOptIn === 'boolean') {
                        setupPriorOptIn = result.priorOptIn;
                    }
                    capeRefusals = (result && result.refused) ? result.refused : {};
                    mergeCapeDefaults(result && result.settings ? result.settings : {});
                },
                complete: function () {
                    setupStepBusy('');
                    done();
                }
            });
        }

        // Precedence, highest first: what the user chose in the wizard, then the
        // cape's proposal, then what the settings file already had.  So this
        // fills gaps and never overwrites a control the user has touched - even
        // if they touched it on an earlier step and then came back.
        function mergeCapeDefaults(proposed) {
            $.each(proposed, function (key, value) {
                if (pendingSettings.hasOwnProperty(key)) {
                    return;
                }
                pendingSettings[key] = value;
                settings[key] = value;
                var $e = $('#' + key.replace(/\./g, '\\.'));
                if ($e.length === 0) {
                    return;
                }
                if ($e.attr('type') === 'checkbox') {
                    $e.prop('checked', value === ($e.data('checked-value') || '1'));
                } else {
                    $e.val(value);
                }
            });
        }

        // Present step 3 according to the jurisdiction declared on step 1.
        //
        // Under a prior-opt-in regime the transmitting settings start with no
        // answer selected, so continuing requires making one.  Elsewhere they
        // keep the values the box already has.  FetchVendorLogos is left alone in
        // both: it fetches a remote image, it does not report anything.
        // Which privacy settings already have an answer IN THE SETTINGS FILE, as
        // opposed to merely showing their shipped default.
        //
        // Neither obvious test works, and both were tried:
        //
        //   - `!settings[key]` is wrong because ShareCrashData's "keep locally,
        //     do not send" is the string "0", which is falsy in JavaScript.  An
        //     explicit and deliberately restrictive answer read as "never
        //     answered" and got discarded on the strict path.
        //   - The browser's `settings` object cannot answer it either: it only
        //     carries settings marked exposedAsJSToPages, which ShareCrashData is
        //     not.  Nor can the rendered control, which shows the shipped default
        //     for a setting nobody has ever set.
        //   - PHP's $settings has defaults merged in, so isset() is true for
        //     everything.
        //
        // ReadSettingFromFile() is the same question cape detection asks with
        // settingIsSet(): is this key literally present in the settings file?
        var answeredSettings = <?= json_encode(array_values(array_filter(
            array('statsPublish', 'ShareCrashData', 'emailAddress', 'FetchVendorLogos', 'SendVendorSerial'),
            function ($k) { return ReadSettingFromFile($k) !== false; }))) ?>;

        // Same question for the jurisdiction, asked the same way and for the same
        // reason: PHP's $settings has defaults merged in, so isset() is useless.
        var jurisdictionAnswered = <?= json_encode(ReadSettingFromFile('LegalJurisdiction') !== false) ?>;

        function settingAnswered(key) {
            return pendingSettings.hasOwnProperty(key) || answeredSettings.indexOf(key) !== -1;
        }

        function applyPrivacyPresentation() {
            $.each(['statsPublish', 'ShareCrashData'], function (i, key) {
                var $e = $('#' + key);
                if ($e.length === 0) {
                    return;
                }
                var hasBlank = $e.find("option[value='']").length > 0;
                if (setupPriorOptIn) {
                    if (!hasBlank) {
                        $e.prepend("<option value=''>-- Choose an Option --</option>");
                    }
                    // Only unanswer it if it has no answer yet. A value already
                    // in the settings file is an answer given earlier and is not
                    // ours to discard.
                    if (!settingAnswered(key)) {
                        $e.val('');
                    }
                } else if (hasBlank && $e.val() !== '') {
                    $e.find("option[value='']").remove();
                }
            });

            if (setupPriorOptIn) {
                var $svs = $('#SendVendorSerial');
                if ($svs.length > 0 && !settingAnswered('SendVendorSerial')) {
                    $svs.prop('checked', false);
                }
            }

            var refusedKeys = [];
            $.each(capeRefusals, function (key) { refusedKeys.push(key); });
            if (refusedKeys.length > 0) {
                $('#capeRefusedNote').removeClass('d-none').html(
                    'Your cape suggested a value for ' + refusedKeys.join(', ') +
                    ', which was not applied: ' + capeRefusals[refusedKeys[0]] +
                    '.  You can still set it yourself.');
            } else {
                $('#capeRefusedNote').addClass('d-none').html('');
            }
        }

        function setupNext() {
            if (!validateSetupStep(setupCurrentStep)) {
                return;
            }
            var next = setupCurrentStep + 1;
            // Step 1 -> 2 is where the cape gets re-asked: the jurisdiction is
            // now known, so a default it was refused at boot may be permitted.
            if (setupCurrentStep === 1) {
                refreshCapeDefaults(function () {
                    applyPrivacyPresentation();
                    showSetupStep(next);
                });
                return;
            }
            if (next === 4) {
                updateSSHKeysVisibility();
            }
            showSetupStep(next);
        }

        function setupBack() {
            if (setupCurrentStep > 1) {
                showSetupStep(setupCurrentStep - 1);
            }
        }

        // An SSH public key box is noise for a basic user and the first thing an
        // advanced one goes looking for.  Keyed off the level chosen on step 2.
        function updateSSHKeysVisibility() {
            var level = parseInt($('#uiLevel').val() || settings['uiLevel'] || '0', 10);
            $('#sshKeysBlock').toggleClass('d-none', !(level >= 1));
        }

        // Which of the settings on this page declare that they need a reboot.
        //
        // Read from settings.json rather than listed here, so a setting that
        // gains or loses "reboot" is handled without anybody remembering to come
        // back and edit the wizard.
        var rebootSettings = <?= json_encode(array_values(array_filter(
            array_merge(
                $settingGroups['initialSetup']['settings'],
                $settingGroups['initialSetup-location']['settings'],
                $settingGroups['initialSetup-privacy']['settings']
            ),
            function ($k) use ($settingInfos) {
                return !empty($settingInfos[$k]['reboot']);
            }))) ?>;

        // Nothing in the wizard reboots: a box must not restart out from under
        // someone who is still setting it up, and savePut() deliberately does not
        // raise the flags the way fpp.js does.  But a change that needs a reboot
        // still needs one afterwards, so raise the flag at the very end and let
        // FPP's own banner ask, on the page the user lands on.  That is the same
        // mechanism restore-from-backup already uses, and it puts the prompt
        // where it can be acted on rather than three steps earlier where it
        // cannot.
        function needsReboot() {
            for (var i = 0; i < rebootSettings.length; i++) {
                if (pendingSettings.hasOwnProperty(rebootSettings[i])) {
                    return true;
                }
            }
            return false;
        }

        function finishSetup() {
            // Field-level validation is done by validateSetupStep(4) when leaving
            // the security step, so a typo is caught where it was made rather
            // than three steps later.  This re-runs it because Finish lives on
            // that step and is reachable without ever leaving it.
            if (!validateSetupStep(4)) {
                return;
            }

            var passwordEnable = $('#passwordEnable').val();
            if (passwordEnable == '1') {
                if (!confirm("You have chosen to use '" + settings['password'] + "' as a password for the FPP web User Interface.  This password will be required for all use of the FPP web User Interface as well as xLight's FPP Connect feature.  If you forget this password, you may be locked out of FPP and it may require a FPP reinstall to recover.  Make sure you have written down this password if it is not something you will easily remember."))
                    return;
            }

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
            // Applying HostName rewrites /etc/hostname and /etc/hosts and runs
            // hostname(1) immediately.  That does not drop the current HTTP
            // connection, so it is survivable - but it should not fire while
            // later settings are still being saved, so it goes near the end with
            // the passwords rather than in the middle of the bulk save.
            var hostNameOrder = ['HostName'];
            var specialKeys = uiPasswordOrder.concat(osPasswordOrder).concat(hostNameOrder);
            var otherKeys = [];
            var uiPasswordKeys = [];
            var osPasswordKeys = [];
            var hostNameKeys = [];
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
            $.each(hostNameOrder, function (i, key) {
                if (filteredSettings.hasOwnProperty(key)) {
                    hostNameKeys.push(key);
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

            // Authorized SSH keys are a config FILE, not a setting, so they do
            // not go through pendingSettings.  The box is pre-filled with the
            // keys already installed, so the test is whether the user CHANGED it
            // - writing an unchanged value would re-run installSSHKeys and take a
            // configuration backup for nothing, and an empty box means "remove
            // them", which is only a real instruction if there was something
            // there to remove.
            function saveSSHKeysThen(done) {
                var $keys = $('#setupSSHKeys');
                if (failedSettings.length > 0 || $keys.length === 0 ||
                    $('#sshKeysBlock').hasClass('d-none') ||
                    $keys.val() === originalSSHKeys) {
                    done();
                    return;
                }
                showSetupProgress('Saving SSH keys...');
                $.ajax({
                    url: 'api/configfile/authorized_keys',
                    type: 'POST',
                    data: $keys.val(),
                    contentType: 'text/plain',
                    error: function () { failedSettings.push('authorized_keys'); },
                    complete: done
                });
            }

            // Raise FPP's own reboot flag if anything saved declares it needs one.
            // Written before the completion flag, so it is captured by the single
            // configuration backup and is already set by the time the browser
            // lands on index.php and the standard banner is drawn.
            function saveRebootFlagThen(done) {
                if (failedSettings.length > 0 || !needsReboot()) {
                    done();
                    return;
                }
                savePut('rebootFlag', '1', true, done);
            }

            function saveHostNameThen(done) {
                if (failedSettings.length > 0 || hostNameKeys.length === 0) {
                    done();
                    return;
                }
                showSetupProgress('Setting host name...');
                savePut('HostName', filteredSettings['HostName'], true, done);
            }

            function startSaving() {
                var afterOthers = function () {
                    saveOsPasswordThen(function () {
                        saveHostNameThen(function () {
                            saveSSHKeysThen(function () {
                                saveRebootFlagThen(saveCompletionAndPasswords);
                            });
                        });
                    });
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
                'fppMode', 'HostName', 'HostDescription', 'InstalledCape', 'uiLevel',
                'ShareCrashData', 'emailAddress', 'FetchVendorLogos', 'SendVendorSerial', 'statsPublish',
                'Locale', 'TimeZone', 'Latitude', 'Longitude', 'WifiRegulatoryDomain', 'LegalJurisdiction'
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

            // The jurisdiction starts unanswered and step 1 will not advance
            // until it has an answer, the same way the password choices work.
            //
            // A seeded value was the obvious alternative and is weaker: once a
            // box is pre-selected there is no way to tell somebody who read it
            // and agreed from somebody who never saw it, and this is the one
            // field whose whole purpose is to record what the user actually
            // declared.  Making it explicit costs one click, and the Lookup
            // buttons above fill it in for anyone who would rather not think
            // about it.
            //
            // A jurisdiction already in the settings file is an answer given
            // earlier and stands; setup re-run after an update does not ask
            // again.  Note this changes nothing about the timezone inference in
            // CapeUtils -- that still decides for cape detection at boot, which
            // happens before anybody has been asked anything.
            var $lj = $('#LegalJurisdiction');
            if ($lj.length > 0 && !jurisdictionAnswered) {
                $lj.prepend("<option value='' selected>-- Choose an Option --</option>");
                $lj.val('');
            }

            $('#uiLevel').on('change', updateSSHKeysVisibility);
            updateSSHKeysVisibility();
            originalSSHKeys = $('#setupSSHKeys').val() || '';
            showSetupStep(1);
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
                            </div>
                        </div>
                    </div>

                    <!-- Step indicator.  Purely informational: it shows where the user
                         is and how much is left, and does not navigate.  Jumping
                         forward would skip the validation that gates Next, and the
                         privacy step genuinely depends on the locale step's answer. -->
                    <ul id="setupSteps" class="nav nav-pills mb-2">
                        <li class="nav-item"><span class="nav-link setupStepTab" data-step="1">1. Location</span></li>
                        <li class="nav-item"><span class="nav-link setupStepTab" data-step="2">2. Device</span></li>
                        <li class="nav-item"><span class="nav-link setupStepTab" data-step="3">3. Privacy</span></li>
                        <li class="nav-item"><span class="nav-link setupStepTab" data-step="4">4. Security</span></li>
                    </ul>
                    <hr>

                    <div class='container-fluid'>

                        <div class="setupStep" data-step="1">
                            <p class="text-muted">Where this player is used.  The time zone and
                                coordinates drive the scheduler's sunrise and sunset times, the holiday
                                list gives it the dates of named holidays, and the regulatory domain
                                decides which WiFi channels the adapter may use.</p>
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

                        <div class="setupStep d-none" data-step="2">
                            <p class="text-muted">What this player is and how it presents itself.</p>
                            <?
                            PrintSettingGroup('initialSetup', '', '', 1, '', '', false);
                            ?>
                        </div>

                        <div class="setupStep d-none" data-step="3">
                            <p class="text-muted">What, if anything, this player sends back.  Every one of
                                these is off unless you turn it on, and all of them can be changed later
                                under Content Setup &rarr; Privacy.</p>
                            <div class="alert alert-warning small d-none" id="privacyRequiredNote">
                                Please answer each of these before continuing.
                            </div>
                            <?
                            $extraData = "<div class='form-actions'>" .
                                "<input type='button' class='buttons' value='Preview Statistics' onClick='PreviewStatistics();'> </div>";
                            PrintSettingGroup('initialSetup-privacy', $extraData, '', '', '', '', false);
                            ?>
                            <div class="alert alert-secondary small d-none" id="capeRefusedNote"></div>
                        </div>

                        <div class="setupStep d-none" data-step="4">
                            <p class="text-muted">Who may use this player.</p>
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

                            <!-- Shown only when step 2 picked an advanced UI level: a
                                 basic user has no use for it, and an empty box that
                                 wants an SSH public key is a good way to make setup
                                 look harder than it is.

                                 Laid out like the same field on the System settings
                                 page, and pre-filled from the same place, so a box
                                 that already has keys shows them here rather than
                                 looking like it has none. -->
                            <div id="sshKeysBlock" class="d-none mt-3">
                                <i class="fas fa-fw fa-graduation-cap fa-nbsp ui-level-1" title="Advanced Level Setting"></i>
                                <b>SSH Keys</b> (root and fpp users)
                                <img id="setupssh_img" title="Add optional SSH key(s) for passwordless SSH authentication." src="images/redesign/help-icon.svg" width=22 height=22>
                                <span id="setupssh_tip" class="tooltip d-none">Add optional SSH key(s) for passwordless SSH authentication.</span><br>
                                <textarea id='setupSSHKeys' style='width: 100%;' rows='8'><? echo htmlspecialchars(shell_exec('sudo cat /root/.ssh/authorized_keys 2>/dev/null')); ?></textarea>
                                <div class="smallText">Saved when setup is finished.</div>
                            </div>
                        </div>

                    </div>

                    <hr>
                    <div class="d-flex gap-2 align-items-center">
                        <input type='button' id='setupBackBtn' class='buttons' value='Back' onClick='setupBack();'>
                        <input type='button' id='setupNextBtn' class='buttons btn-primary' value='Next' onClick='setupNext();'>
                        <input type='button' id='finishSetupBtn' class='buttons btn-success d-none' value='Finish Setup'
                            onClick='finishSetup();'>
                        <span id='setupStepBusy' class='d-none text-muted'></span>
                    </div>
                </div>
            </div>
            <?php include 'common/footer.inc'; ?>
        </div>

        <div id="restoreDialog" class="modal fade" tabindex="-1" role="dialog">
            <div class="modal-dialog modal-lg" role="document">
                <div class="modal-content">
                    <div class="modal-header">
                        <h5 class="modal-title"><i class="fas fa-upload"></i> Restore from Backup</h5>
                        <button type="button" class="btn-close" data-bs-dismiss="modal" aria-label="Close"></button>
                    </div>
                    <div class="modal-body">
                        <div class="mb-3">
                            <label class="fw-bold mb-2">Restore Type</label>
                            <select id="restoreType" class="form-select" onchange="restoreTypeChanged()">
                                <option value="json">JSON Configuration Restore</option>
                                <option value="filecopy">File Copy Restore</option>
                            </select>
                        </div>

                        <div id="jsonRestoreSection">
                            <p class="text-muted small mb-2">Upload a JSON backup file to restore configuration settings.</p>
                            <div class="alert alert-info small py-2 mb-2">
                                <i class="fas fa-info-circle"></i> The JSON Configuration backup file does not contain sequences, media, plugin files, or additional files. You will need to upload those files or perform an FPP Connect from xLights to restore your data.
                            </div>
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

                        <div id="fileCopySection" class="d-none">
                            <p class="text-muted small mb-2">Restore configuration, sequences, plugins, and other files from a backup directory.</p>
                            <table class="fileCopyFields">
                                <tr>
                                    <td class="pe-3 pb-2" style="white-space: nowrap;">Restore from:</td>
                                    <td class="pb-2">
                                        <select id="fileCopyDirection" class="form-select" onchange="fileCopyDirectionChanged()">
                                            <option value="FROMUSB">Restore from USB</option>
                                            <option value="FROMLOCAL">Restore from Local FPP Backups Directory</option>
                                            <option value="FROMREMOTE">Restore from Remote FPP Backups Directory</option>
                                        </select>
                                    </td>
                                </tr>
                                <tr class="fcUSB" style="display: none;">
                                    <td class="pe-3 pb-2">USB Device:</td>
                                    <td class="pb-2">
                                        <select id="fcUSBDevice" class="form-select d-inline w-auto" onchange="USBDeviceChanged()"></select>
                                        <input type='button' class='buttons btn-sm' onClick='GetBackupDevices();' value='Refresh List'>
                                    </td>
                                </tr>
                                <tr class="fcHost" style="display: none;">
                                    <td class="pe-3 pb-2">Remote Host:</td>
                                    <td class="pb-2">
                                        <select id="fcHost" class="form-select" onchange="GetRemoteHostUSBStorage(); GetBackupHostBackupDirs($('#fcRemoteStorage').val())">
                                            <option value="">-- Select Remote Host --</option>
                                            <?php echo $hostOptions; ?>
                                        </select>
                                    </td>
                                </tr>
                                <tr class="fcHostDevice" style="display: none;">
                                    <td class="pe-3 pb-2">Remote Storage:</td>
                                    <td class="pb-2">
                                        <select id="fcRemoteStorage" class="form-select" onchange="GetBackupHostBackupDirs($(this).val())">
                                            <option value="none">Default FPP Storage</option>
                                        </select>
                                    </td>
                                </tr>
                                <tr class="fcPathSelect" style="display: none;">
                                    <td class="pe-3 pb-2">Backup Path:</td>
                                    <td class="pb-2">
                                        <select id="fcPathSelect" class="form-select"></select>
                                    </td>
                                </tr>
                            </table>

                            <div class="mt-2">
                                <label class="fw-bold">What to restore:</label>
                                <table id="fcFlagsTable">
                                    <tr>
                                        <td>
                                            <input type='checkbox' class='fcFlag' value='Configuration' id='fcFlagConfig' checked>Configuration<br>
                                            <input type='checkbox' class='fcFlag' value='Playlists' id='fcFlagPlaylists' checked>Playlists<br>
                                        </td>
                                        <td width='10px'></td>
                                        <td>
                                            <input type='checkbox' class='fcFlag' value='Plugins' id='fcFlagPlugins' checked>Plugins<br>
                                        </td>
                                        <td width='10px'></td>
                                        <td>
                                            <input type='checkbox' class='fcFlag' value='Sequences' id='fcFlagSequences' checked>Sequences<br>
                                            <input type='checkbox' class='fcFlag' value='Images' id='fcFlagImages' checked>Images<br>
                                        </td>
                                        <td width='10px'></td>
                                        <td>
                                            <input type='checkbox' class='fcFlag' value='Scripts' id='fcFlagScripts' checked>Scripts<br>
                                            <input type='checkbox' class='fcFlag' value='Effects' id='fcFlagEffects' checked>Effects<br>
                                        </td>
                                        <td width='10px'></td>
                                        <td>
                                            <input type='checkbox' class='fcFlag' value='Music' id='fcFlagMusic' checked>Music<br>
                                            <input type='checkbox' class='fcFlag' value='Videos' id='fcFlagVideos' checked>Videos<br>
                                        </td>
                                        <td width='10px'></td>
                                        <td>
                                            <input type='checkbox' class='fcFlag' value='EEPROM' id='fcFlagEEPROM' checked>Virtual EEPROM<br>
                                        </td>
                                    </tr>
                                </table>
                                <div class="mt-1">
                                    Delete extras:
                                    <input type='checkbox' id='fcDeleteExtra'>
                                    (Delete extra files on destination that do not exist in the backup)
                                </div>
                            </div>
                        </div>
                    </div>
                    <div class="modal-footer">
                        <button type="button" class="buttons" data-bs-dismiss="modal">Cancel</button>
                        <button type="button" id="uploadRestoreBtn" class="buttons btn-success" disabled onclick="uploadRestoreFile()">
                            <i class="fas fa-upload"></i> Upload &amp; Restore
                        </button>
                        <button type="button" id="fileCopyRestoreBtn" class="buttons btn-success d-none" onclick="PerformFileCopyRestore()">
                            <i class="fas fa-undo"></i> Restore
                        </button>
                    </div>
                </div>
            </div>
        </div>

        <div id="fileCopyPopup" class="d-none">
            <textarea class="w-100" style="height: 55vh; min-height: 200px;" disabled id="fileCopyText"></textarea>
        </div>

        <script>
        function restoreTypeChanged() {
            var type = $('#restoreType').val();
            if (type == 'json') {
                $('#jsonRestoreSection').removeClass('d-none');
                $('#fileCopySection').addClass('d-none');
                $('#uploadRestoreBtn').removeClass('d-none');
                $('#fileCopyRestoreBtn').addClass('d-none');
            } else {
                $('#jsonRestoreSection').addClass('d-none');
                $('#fileCopySection').removeClass('d-none');
                $('#uploadRestoreBtn').addClass('d-none');
                $('#fileCopyRestoreBtn').removeClass('d-none');
                fileCopyDirectionChanged();
            }
        }

        function openRestoreDialog() {
            clearRestoreFile();
            $('#restoreType').val('json');
            restoreTypeChanged();
            $('#restoreDialog').fppDialog('open');
            setupDropZone();
            GetBackupDevices();
        }
        </script>
</body>

</html>