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

        // A backup made before the legal jurisdiction existed -- or taken from a
        // box that never answered it -- restores a configuration with no answer
        // in it. The restore is still complete and initialSetup-02 stands: the
        // only thing missing is the one question FPP cannot infer. Say so here
        // so the caller can keep the user on this page instead of bouncing them
        // to index.php and straight back again by way of the redirect in
        // menuHead.inc.
        //
        // Note the restore MERGES: a jurisdiction already recorded is not
        // cleared by a backup that lacks one, so this only fires where there
        // genuinely is no answer.
        $missing = MissingSetupSettings();
        $msg = 'Configuration restored successfully. A reboot is required for changes to take effect.';
        if (count($missing)) {
            $msg .= ' This backup does not answer everything FPP needs to know about privacy on this player'
                . ' (' . implode(', ', $missing) . '), so setup will ask before continuing.';
        }
        echo json_encode(['Status' => 'OK', 'Message' => $msg, 'NeedsSetup' => (count($missing) > 0)]);
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
        /* FPP's Bootstrap build ships no table rules at all, so these cells
           would otherwise render at the browser's 1px default and the rows run
           together.  Padding and the group rules below are the table's only
           spacing. */
        #privacyTable td, #privacyTable th { vertical-align: top; padding: 0.4rem 0.5rem; }
        #privacyTable thead th { border-bottom: 1px solid var(--fpp-border); }
        /* Each top-level disclosure opens a group; its indented "... and also"
           children hang off it with no rule of their own. */
        #privacyTable tbody tr[data-depth='0']:not(:first-child) td { border-top: 1px solid var(--fpp-border); }
        /* .smallText is used in four places here and defined in no stylesheet,
           so the secondary text was rendering at full body size.  Everything
           read as equally important; this is what makes the label the label. */
        .smallText { font-size: 0.875rem; }
        /* Depth is otherwise carried by the indent alone: a rung of the ladder
           was exactly as heavy as the disclosure it hangs off. */
        #privacyTable tr[data-depth='1'] b { font-weight: 400; }
        /* The description ran ~90 characters, and pushed the radios ~840px away
           from the label they belong to.  Capping the text column pulls the
           choice columns back towards what they answer. */
        #privacyTable td:first-child { max-width: 34em; }
        #privacyTable .privacyChoice { text-align: center; width: 6em; white-space: nowrap; }
        /* Whole cell clickable, not just a 13px UA radio at the end of a long row. */
        #privacyTable .privacyChoice label { display: block; padding: 0.15rem 0; cursor: pointer; }
        #privacyTable .privacyChoice input[type='radio'] { width: 1.1rem; height: 1.1rem; cursor: pointer; }
        #privacyTable tfoot td { border-top: 1px solid var(--fpp-border); padding-top: 0.6em; }
        /* Rows still to answer, marked only once Next has been refused.  The
           subtle tokens are defined per theme by Bootstrap, so this reads in
           both light and dark without a theme-specific rule. */
        #privacyTable tr.privacyUnanswered > td { background-color: var(--bs-warning-bg-subtle); }
        #privacyTable tr.privacyUnanswered > td:first-child { box-shadow: inset 0.25rem 0 0 var(--bs-warning); }
        #privacyTable tfoot .buttons { width: auto; }
        #privacyTable .privacyGoesTo { width: 14em; }
        #privacyTable tr.privacyBlocked { opacity: 0.5; }
        #privacyTable .privacyHelp { cursor: pointer; opacity: 0.7; }
        #privacyTable .privacyHelp:hover { opacity: 1; }
        #fcFlagsTable td { vertical-align: top; white-space: nowrap; }
        /* Without the old "UI Password"/"OS Password" sub-headings there is
           nothing between the two groups, so they read as one list. */
        #osPasswordEnableRow { margin-top: 1rem; }
    </style>
    <?php
    include 'common/htmlMeta.inc';
    require_once('config.php');
    require_once('common.php');
    require_once('jurisdiction.inc');
    require_once('privacyDisclosures.inc');
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
                            // Straight back to this page when the restored
                            // configuration has no jurisdiction in it -- going to
                            // index.php would only be redirected here anyway, and
                            // the reload picks up the restored values.
                            location.href = response.NeedsSetup ? 'initialSetup.php' : 'index.php';
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
        // Every setting this page puts in front of the user, from the same groups
        // the markup is generated from so the two cannot drift. Privacy is not
        // here: privacyToPending() owns those, because its rows do not map one
        // to one onto settings.
        var WIZARD_SETTING_KEYS = <?= json_encode(array_values(array_merge(
            $settingGroups['initialSetup-location']['settings'],
            $settingGroups['initialSetup']['settings']
        ))) ?>;

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

        // What the password fields held when the page loaded, so an untouched
        // pair can be told from a mistyped one.
        //
        // Setup is re-run after an fppOS update, where a password already chosen
        // is rendered into its field while the "verify" box -- which is UI-only
        // and never stored -- comes up empty. Comparing the two unconditionally
        // then reported a mismatch on a screen the user had not touched, and
        // there was no way past it but to retype a password they had already
        // set. Only compare once something has actually been typed.
        var passwordInitial = {};

        function passwordPairChanged(key) {
            return $('#' + key).val() !== (passwordInitial[key] || '') ||
                $('#' + key + 'Verify').val() !== '';
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
                // Under a prior-opt-in regime every row starts unanswered and an
                // answer is required.  Elsewhere they are pre-filled and moving on
                // accepts what is shown -- which is a choice the user can see, not
                // a default nobody was shown.
                if (setupPriorOptIn && privacyUnanswered().length > 0) {
                    privacyShowUnanswered = true;
                    privacyMarkUnanswered();
                    $('#privacyRequiredNote').removeClass('d-none');
                    return false;
                }
                privacyShowUnanswered = false;
                privacyMarkUnanswered();
                $('#privacyRequiredNote').addClass('d-none');
                privacyToPending();
                return true;
            }
            if (n === 4) {
                var pwEnable = $('#passwordEnable').val();
                if (pwEnable === '') {
                    alert('You must choose to either Enable or Disable the UI password.');
                    return false;
                }
                if (pwEnable === '1' && passwordPairChanged('password') &&
                    $('#password').val() !== $('#passwordVerify').val()) {
                    alert('The UI password and its confirmation do not match.');
                    return false;
                }
                <? if ($showOSSecurity) { ?>
                    if ($('#osPasswordEnable').val() === '') {
                        alert('You must choose to either use the default OS password or choose a custom password.');
                        return false;
                    }
                    if ($('#osPasswordEnable').val() === '1' && passwordPairChanged('osPassword') &&
                        $('#osPassword').val() !== $('#osPasswordVerify').val()) {
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
                // A privacy setting has no control of its own any more -- the
                // table owns those, and privacyToPending() writes them.  Feed the
                // cape's value in as the current value so it shows as the
                // pre-filled answer where the jurisdiction permits one.
                if (privacyCurrent.hasOwnProperty(key)) {
                    privacyCurrent[key] = '' + value;
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
            array('statsPublish', 'ShareCrashData', 'emailAddress', 'FetchVendorLogos', 'SendVendorSerial',
                'passwordEnable', 'osPasswordEnable'),
            function ($k) { return ReadSettingFromFile($k) !== false; }))) ?>;

        // Same question for the jurisdiction, asked the same way and for the same
        // reason: PHP's $settings has defaults merged in, so isset() is useless.
        var jurisdictionAnswered = <?= json_encode(ReadSettingFromFile('LegalJurisdiction') !== false) ?>;

        function settingAnswered(key) {
            return pendingSettings.hasOwnProperty(key) || answeredSettings.indexOf(key) !== -1;
        }

        // The privacy table's rows, and how each maps onto a setting.
        //
        // Rows are DISCLOSURES, not settings, and the two do not line up one to
        // one.  ShareCrashData is a single ordered value, so its three rows are a
        // ladder: "include configuration and logs" necessarily includes "your
        // settings", which necessarily includes the stack.  Drawn as three
        // independent Allow/Deny pairs it could express states that do not exist,
        // so the ladder is enforced here in both directions.
        //
        // Denying the crash rows writes 0 ("keep locally"), not -1: a report is
        // written to the crashes folder either way, which is what the help text
        // promises.  -1 is deliberately not reachable from this page.
        var PRIVACY_ROWS = {
            'stats':          { setting: 'statsPublish',     allow: 'Enabled', deny: 'Disabled' },
            'crash':          { setting: 'ShareCrashData',   level: 1 },
            'crash-settings': { setting: 'ShareCrashData',   level: 2, parent: 'crash' },
            'crash-config':   { setting: 'ShareCrashData',   level: 3, parent: 'crash-settings' },
            'logos':          { setting: 'FetchVendorLogos', allow: '1', deny: '0' },
            'serial':         { setting: 'SendVendorSerial', allow: '1', deny: '0', parent: 'logos' }
        };
        var CRASH_ROWS = ['crash', 'crash-settings', 'crash-config'];

        // What the box currently holds for each of these.  PHP's $settings has
        // defaults merged in, so this is the effective value; answeredSettings
        // above is what says whether it was ever actually chosen.
        var privacyCurrent = <?= json_encode(array(
            'statsPublish' => (string) ($settings['statsPublish'] ?? ''),
            'ShareCrashData' => (string) ($settings['ShareCrashData'] ?? ''),
            'FetchVendorLogos' => (string) ($settings['FetchVendorLogos'] ?? ''),
            'SendVendorSerial' => (string) ($settings['SendVendorSerial'] ?? ''),
            'emailAddress' => (string) ($settings['emailAddress'] ?? ''),
        )) ?>;

        // Rows the user has actually touched here.  privacyFromSettings() runs
        // again every time step 1 is left, and must not discard an answer already
        // given on step 3 and then navigated away from.
        var privacyTouched = {};

        function privacyAnsweredInFile(key) {
            return answeredSettings.indexOf(key) !== -1;
        }

        var PRIVACY_HELP = <?= json_encode(privacyDisclosures()) ?>;

        function privacyGet(row) {
            return $("input[name='privacy_" + row + "']:checked").val() || '';
        }
        function privacySet(row, val) {
            if (val === '') {
                $("input[name='privacy_" + row + "']").prop('checked', false);
            } else {
                $("input[name='privacy_" + row + "'][value='" + val + "']").prop('checked', true);
            }
        }

        // Enforce the ladder in both directions, then reflect it in the UI.
        // Allowing a child allows its parents; denying a parent denies and
        // disables its children, so an impossible combination cannot be reached
        // by clicking rather than merely being rejected afterwards.
        function privacyApplyLadder(changedRow) {
            $.each(PRIVACY_ROWS, function (row, def) {
                if (!def.parent) { return; }
                if (changedRow === row && privacyGet(row) === 'allow') {
                    // Walk up: every ancestor must be allowed for this to mean anything.
                    var p = def.parent;
                    while (p) {
                        privacySet(p, 'allow');
                        p = PRIVACY_ROWS[p].parent;
                    }
                }
            });
            // Walk down: a denied parent forces its descendants denied and locks them.
            $.each(PRIVACY_ROWS, function (row, def) {
                if (!def.parent) { return; }
                var blocked = false;
                var p = def.parent;
                while (p) {
                    if (privacyGet(p) === 'deny') { blocked = true; break; }
                    p = PRIVACY_ROWS[p].parent;
                }
                var $inputs = $("input[name='privacy_" + row + "']");
                $inputs.prop('disabled', blocked);
                $("tr[data-row='" + row + "']").toggleClass('privacyBlocked', blocked);
                if (blocked) { privacySet(row, 'deny'); }
            });
            // The e-mail field is pointless until something is being sent.
            $('#emailRow').toggleClass('d-none', privacyGet('crash') !== 'allow');
        }

        // Which rows still have no answer.  A row locked by a denied parent is
        // answered by that denial, so it does not count as outstanding.
        // Once Next has been refused, show WHICH rows the note is talking
        // about, and let the marks clear one by one as they are answered - a
        // banner alone does not say where to look in a table of eight rows.
        var privacyShowUnanswered = false;

        function privacyMarkUnanswered() {
            $('#privacyTable tr.privacyRow').removeClass('privacyUnanswered');
            if (!privacyShowUnanswered) { return; }
            $.each(privacyUnanswered(), function (i, row) {
                $("#privacyTable tr[data-row='" + row + "']").addClass('privacyUnanswered');
            });
        }

        function privacyUnanswered() {
            var out = [];
            $.each(PRIVACY_ROWS, function (row) {
                if ($("input[name='privacy_" + row + "']").prop('disabled')) { return; }
                if (privacyGet(row) === '') { out.push(row); }
            });
            return out;
        }

        // Collapse the table back into settings values.
        function privacyToPending() {
            var stats = privacyGet('stats');
            if (stats !== '') {
                pendingSettings['statsPublish'] = PRIVACY_ROWS['stats'][stats];
            }
            // Deepest allowed rung wins; nothing allowed means 0, keep locally.
            var level = 0;
            $.each(CRASH_ROWS, function (i, row) {
                if (privacyGet(row) === 'allow') { level = PRIVACY_ROWS[row].level; }
            });
            pendingSettings['ShareCrashData'] = '' + level;
            $.each(['logos', 'serial'], function (i, row) {
                var v = privacyGet(row);
                if (v !== '') { pendingSettings[row === 'logos' ? 'FetchVendorLogos' : 'SendVendorSerial'] = PRIVACY_ROWS[row][v]; }
            });
            pendingSettings['emailAddress'] = $('#emailAddress').val() || '';
            $.each(pendingSettings, function (k, v) { settings[k] = v; });
        }

        // Fill the table in from what the box holds.
        //
        // Under a prior-opt-in regime nothing is pre-selected and the step will
        // not advance until each row is answered -- except "send crash reports",
        // which may start allowed: sending a stack trace rests on legitimate
        // interests (GDPR Art 6(1)(f)) rather than consent, and the tiers that do
        // need consent are the two below it.  A row already answered in the
        // settings file keeps its answer either way.
        function privacyFromSettings() {
            var crashLevel = parseInt(privacyCurrent['ShareCrashData'], 10);
            if (isNaN(crashLevel)) { crashLevel = 0; }
            var crashAnswered = privacyAnsweredInFile('ShareCrashData');

            $.each(PRIVACY_ROWS, function (row, def) {
                if (privacyTouched[row]) { return; }
                // Deliberately the settings FILE, not pendingSettings: a cape's
                // default is the vendor's suggestion, not the user's answer, so
                // under a prior-opt-in regime the row still has to be asked.
                var answered = privacyAnsweredInFile(def.setting);
                if (setupPriorOptIn && !answered) {
                    privacySet(row, row === 'crash' ? 'allow' : '');
                    return;
                }
                if (def.level !== undefined) {
                    if (!crashAnswered && !setupPriorOptIn) {
                        // Never chosen, permissive regime: take the shipped default.
                        privacySet(row, crashLevel >= def.level ? 'allow' : 'deny');
                    } else {
                        privacySet(row, crashLevel >= def.level ? 'allow' : 'deny');
                    }
                    return;
                }
                var cur = privacyCurrent[def.setting];
                if (cur === def.allow) {
                    privacySet(row, 'allow');
                } else if (cur === def.deny) {
                    privacySet(row, 'deny');
                } else {
                    // Neither, which is what statsPublish's shipped "Banner" is:
                    // a prompt to decide later rather than a decision.  The table
                    // has no third state and this branch is the permissive
                    // regime, so it resolves to the shipped intent -- allow.
                    // Leaving it blank would be worse than either answer: the
                    // step would pass validation with nothing selected and
                    // privacyToPending() would write no value at all, which is
                    // precisely the "Finish writes nothing" hole this replaces.
                    privacySet(row, 'allow');
                }
            });
            $('#emailAddress').val(privacyCurrent['emailAddress'] || '');
            privacyApplyLadder(null);
        }

        function PrivacySetAll(val) {
            $.each(PRIVACY_ROWS, function (row) { privacySet(row, val); });
            privacyApplyLadder(null);
        }

        function applyPrivacyPresentation() {
            privacyFromSettings();

            // Group by reason rather than listing keys against the first one:
            // several settings usually share a reason, and naming a reason that
            // does not apply to every key listed is worse than saying nothing.
            var byReason = {};
            $.each(capeRefusals, function (key, why) {
                if (!byReason[why]) { byReason[why] = []; }
                byReason[why].push(key);
            });
            var parts = [];
            $.each(byReason, function (why, keys) {
                var list = keys.length > 1
                    ? keys.slice(0, -1).join(', ') + ' and ' + keys[keys.length - 1]
                    : keys[0];
                parts.push('<b>' + list + '</b> ' + (keys.length > 1 ? 'were' : 'was') +
                    ' not applied because ' + why + '.');
            });
            if (parts.length > 0) {
                $('#capeRefusedNote').removeClass('d-none').html(
                    'Your cape suggested settings for this player.  ' + parts.join('  ') +
                    '  You can still choose them yourself below.');
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

        // Record every setting the wizard showed, not only the ones touched.
        //
        // Only pendingSettings is saved, so a control the user looked at and
        // accepted wrote nothing at all and the box carried on running the
        // shipped default with no record that anyone had been asked. That is the
        // same hole the privacy step had, and it bites hardest on TimeZone:
        // nothing else writes it, so leaving it unrecorded is what let cape
        // detection be unable to work out where a box was.
        //
        // "Shown it and left it" is an answer, and this is what records it.
        function recordPromptedSettings() {
            $.each(WIZARD_SETTING_KEYS, function (i, key) {
                var $e = $('#' + key.replace(/\./g, '\\.'));
                if ($e.length === 0) {
                    // Not rendered here -- platform-gated, or hidden by a
                    // checkFile like InstalledCape when a real cape is present.
                    return;
                }
                var value;
                if ($e.attr('type') === 'checkbox') {
                    value = $e.is(':checked') ? ($e.data('checked-value') || '1')
                                              : ($e.data('unchecked-value') || '0');
                } else {
                    value = $e.val();
                }
                if (value === null || value === undefined) {
                    return;
                }
                pendingSettings[key] = value;
                settings[key] = value;
            });
        }

        function finishSetup() {
            // Field-level validation is done by validateSetupStep(4) when leaving
            // the security step, so a typo is caught where it was made rather
            // than three steps later.  This re-runs it because Finish lives on
            // that step and is reachable without ever leaving it.
            if (!validateSetupStep(4)) {
                return;
            }

            recordPromptedSettings();

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

        // Offer "-- Choose an Option --" only where the choice has not been made.
        //
        // The old test was `!settings[key]`, which re-asked anyone who had
        // already answered: both of these settings spell their first option "0"
        // -- "No Password (Default)" and "falcon (Default)" -- and "0" is falsy
        // in JavaScript. Someone who had deliberately chosen no password was
        // shown an unanswered dropdown and made to choose again on every re-run
        // of setup. Same trap as ShareCrashData's "keep locally", and the same
        // fix: ask the settings file whether the key is there.
        function offerUnansweredChoice(key, childClass) {
            var $e = $('#' + key);
            if ($e.length === 0) {
                return;
            }
            if (!settingAnswered(key)) {
                $e.prepend("<option value='' selected>-- Choose an Option --</option>");
                $e.val('');
            }
            if ($e.val() == '1') {
                $('.' + childClass).show();
            }
        }

        $(document).ready(function () {
            $.each(['password', 'osPassword'], function (i, k) {
                passwordInitial[k] = $('#' + k).val() || '';
            });
            offerUnansweredChoice('passwordEnable', 'passwordEnableChild');
            <? if ($showOSSecurity) { ?>
                offerUnansweredChoice('osPasswordEnable', 'osPasswordEnableChild');
            <? } ?>

            UpdateChildSettingsVisibility();
            setupDropZone();

            // Override all auto-generated onChange handlers to track changes instead of saving immediately
            // List of all settings on this page
            var settingsToOverride = [
                'passwordEnable', 'password', 'passwordVerify',
                'osPasswordEnable', 'osPassword', 'osPasswordVerify',
                'fppMode', 'HostName', 'HostDescription', 'InstalledCape', 'uiLevel',
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

            // Ladder enforcement runs on every change, so an impossible
            // combination cannot be clicked into existence.
            $("input[name^='privacy_']").on('change', function () {
                var row = $(this).attr('name').replace(/^privacy_/, '');
                privacyTouched[row] = true;
                privacyApplyLadder(row);
                // Answering a row clears its mark; clearing the last one takes
                // the note with it rather than leaving a stale complaint.
                privacyMarkUnanswered();
                if (privacyShowUnanswered && privacyUnanswered().length === 0) {
                    privacyShowUnanswered = false;
                    $('#privacyRequiredNote').addClass('d-none');
                }
            });

            // One popover at a time, so the disclosures cannot stack up on top of
            // each other. The text is the same content the Settings privacy tab
            // will want, which is why it lives in privacyDisclosures.inc.
            $('.privacyHelp').on('click', function (e) {
                e.preventDefault();
                var row = $(this).data('row');
                var d = PRIVACY_HELP[row];
                if (d) { DialogOK(d.title, d.body); }
            });

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
                    <ul id="setupSteps" class="nav nav-pills pageContent-tabs">
                        <li class="nav-item"><span class="nav-link pe-none setupStepTab" data-step="1">1. Location</span></li>
                        <li class="nav-item"><span class="nav-link pe-none setupStepTab" data-step="2">2. Device</span></li>
                        <li class="nav-item"><span class="nav-link pe-none setupStepTab" data-step="3">3. Privacy</span></li>
                        <li class="nav-item"><span class="nav-link pe-none setupStepTab" data-step="4">4. Security</span></li>
                    </ul>

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
                            <!-- Raised only when the coordinates had to be derived from
                                 the time zone, which is accurate to the zone and not to
                                 the town.  These fields schedule shows at sunset, so an
                                 approximation that big is worth a standing warning
                                 rather than a dialog that gets dismissed. -->
                            <div class="alert alert-warning small d-none" id="approxCoordsNote">
                                Latitude and longitude were estimated from your time zone
                                because this player could not reach the internet.  They can
                                be out by enough to move sunset by up to an hour &mdash;
                                worth correcting if you schedule anything at sunrise or sunset.
                            </div>
                        </div>

                        <div class="setupStep d-none" data-step="2">
                            <p class="text-muted">What this player is and how it presents itself.</p>
                            <?
                            PrintSettingGroup('initialSetup', '', '', 1, '', '', false);
                            ?>
                        </div>

                        <div class="setupStep d-none" data-step="3">
                            <p>FPP is built by volunteers: statistics tell us how FPP is used and help us
                                focus development where it matters most.  Please enable them so we can help
                                you better.</p>

                            <div class="alert alert-warning small d-none" id="privacyRequiredNote">
                                Please answer each of these before continuing.
                            </div>

                            <!-- One row per disclosure, not one per setting: "send crash
                                 reports" is three rows against a single ShareCrashData
                                 value, because the tiers are cumulative rather than
                                 independent (see the ladder handling in JS). -->
                            <div class="table-responsive">
                            <table class="privacyTable" id="privacyTable">
                                <thead>
                                    <tr>
                                        <th></th>
                                        <th class="privacyGoesTo">Goes to</th>
                                        <th class="privacyChoice">Allow</th>
                                        <th class="privacyChoice">Deny</th>
                                    </tr>
                                </thead>
                                <tbody>
                                    <?php
                                    require_once 'privacyDisclosures.inc';
                                    $disclosures = privacyDisclosures();

                                    // label, goesTo, extra classes, sub-row depth
                                    $rows = array(
                                        array('stats', 'Share usage statistics',
                                            'FPP &amp; xLights developers',
                                            'Tells us which platforms and features to keep supporting, so yours is '
                                            . 'less likely to be dropped. '
                                            . "<a href='#' onClick='PreviewStatistics(); return false;'>Preview data</a>.", 0),
                                        array('logos', 'Load cape logos from vendor website', 'Cape vendor', '', 0),
                                        array('serial', '&hellip; and include your cape\'s serial number', '', '', 1),
                                        // Last, because the e-mail field hangs off the deepest
                                        // crash rung and looks orphaned mid-table.
                                        array('crash', 'Send crash reports',
                                            'FPP &amp; xLights developers,<br>AI service',
                                            'The only way most crashes are ever found.', 0),
                                        array('crash-settings', '&hellip; and include your settings', '', '', 1),
                                        array('crash-config', '&hellip; and include configuration and logs', '', '', 1),
                                    );

                                    foreach ($rows as $r) {
                                        list($id, $label, $goesTo, $sub, $depth) = $r;
                                        $help = isset($disclosures[$id])
                                            ? htmlspecialchars($disclosures[$id]['title'] . "\n\n" . strip_tags(str_replace(array('</p>', '<br>'), "\n", $disclosures[$id]['body'])), ENT_QUOTES)
                                            : '';
                                        $indent = $depth ? " style='padding-left:2.2em'" : '';
                                        echo "<tr class='privacyRow' data-row='$id' data-depth='$depth'>";
                                        echo "<td$indent><b>$label</b>";
                                        echo " <i class='fas fa-question-circle privacyHelp' data-row='$id' title='Explain what this sends'></i>";
                                        if ($sub != '') {
                                            echo "<div class='smallText text-muted'>$sub</div>";
                                        }
                                        echo "</td>";
                                        echo "<td class='privacyGoesTo smallText text-muted'>$goesTo</td>";
                                        echo "<td class='privacyChoice'><label><input type='radio' name='privacy_$id' value='allow'></label></td>";
                                        echo "<td class='privacyChoice'><label><input type='radio' name='privacy_$id' value='deny'></label></td>";
                                        echo "</tr>";

                                        // The e-mail field belongs to the crash rows and is
                                        // pointless until they are allowed, so it lives inside
                                        // the ladder rather than after the table.
                                        if ($id === 'crash-config') {
                                            echo "<tr class='privacyRow d-none' id='emailRow'>";
                                            echo "<td style='padding-left:2.2em'><b>E-mail address</b> <span class='text-muted'>(optional)</span>";
                                            echo " <i class='fas fa-question-circle privacyHelp' data-row='email' title='Explain what this sends'></i></td>";
                                            echo "<td colspan='3'><input type='text' id='emailAddress' size='40' maxlength='256' placeholder='you@example.com'></td>";
                                            echo "</tr>";
                                        }
                                    }
                                    ?>
                                </tbody>
                                <tfoot>
                                    <tr class="privacyAllRow">
                                        <td class="text-muted smallText">You can change this at any time under Status/Control &rarr; FPP Settings &rarr; Privacy.</td>
                                        <td></td>
                                        <td class="privacyChoice">
                                            <input type='button' class='buttons' value='Allow all' onClick='PrivacySetAll("allow");'>
                                        </td>
                                        <td class="privacyChoice">
                                            <input type='button' class='buttons' value='Deny all' onClick='PrivacySetAll("deny");'>
                                        </td>
                                    </tr>
                                </tfoot>
                            </table>
                            </div>

                            <div class="alert alert-secondary small d-none" id="capeRefusedNote"></div>
                        </div>

                        <div class="setupStep d-none" data-step="4">
                            <p class="text-muted">Who may use this player.</p>
                            <b>Passwords</b><br>
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
                            <div id="sshKeysBlock" class="d-none mt-4">
                                <i class="fas fa-fw fa-graduation-cap fa-nbsp ui-level-1" title="Advanced Level Setting"></i>
                                <b>SSH Keys</b> (root and fpp users)
                                <img id="setupssh_img" title="Add optional SSH key(s) for passwordless SSH authentication." src="images/redesign/help-icon.svg" width=22 height=22>
                                <span id="setupssh_tip" class="tooltip d-none">Add optional SSH key(s) for passwordless SSH authentication.</span><br>
                                <textarea id='setupSSHKeys' class='w-100' rows='8'><? echo htmlspecialchars(shell_exec('sudo cat /root/.ssh/authorized_keys 2>/dev/null')); ?></textarea>
                                <div class="smallText">Saved when setup is finished.</div>
                            </div>
                        </div>

                    <hr class="mt-4 mb-3">
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