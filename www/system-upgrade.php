<!DOCTYPE html>
<html>

<head>
    <?php
    include 'common/htmlMeta.inc';
    require_once 'config.php';
    require_once 'common.php';

    include 'common/menuHead.inc';
    ?>
    <link rel="stylesheet" href="css/fpp-system-design.css?ref=<?php echo filemtime('css/fpp-system-design.css'); ?>">
    <?php
    $fppVersion = getFPPVersion();
    $localGitVersion = get_local_git_version();
    $fppVersionDisplay = getFPPVersionDisplay();

    $uploadDirectory = $mediaDirectory . "/upload";
    $freeSpace = disk_free_space($uploadDirectory);
    $osUpdateFiles = getFileList($uploadDirectory, "fppos");

    if (file_exists("/proc/cpuinfo")) {
        $serialNumber = exec("sed -n 's/^Serial.*: //p' /proc/cpuinfo", $output, $return_val);
        if ($return_val != 0) {
            unset($serialNumber);
        }
    }
    if ((!isset($serialNumber) || $serialNumber == "") && $settings['Variant'] == "PocketBeagle2") {
        $serialNumber = exec("dd if=/sys/bus/i2c/devices/0-0050/eeprom count=16 skip=40 bs=1 2>/dev/null", $output, $return_val);
        if ($return_val != 0) {
            unset($serialNumber);
        }
    }
    unset($output);
    ?>
    <script>
        var osAssetMap = {};
        var fppUpdateAvailable = false;
        var osUpgradeAvailable = false;
        var forceOsUpgradeTest = false; // Test mode flag - prevents PopulateOSSelect from overwriting
        var currentOSRelease = null;

        /**
         * Parse OS version from release string or filename
         * Extracts year-month pattern (e.g., "2025-11" from "v2025-11" or "Pi-9.3_2025-11.fppos")
         */
        function parseOSVersion(str) {
            if (!str) return null;
            var match = str.match(/(\d{4})-(\d{2})/);
            if (match) {
                return { year: parseInt(match[1]), month: parseInt(match[2]) };
            }
            return null;
        }

        /**
         * Check if available OS version is newer than current
         */
        function isNewerOSVersion(available, current) {
            if (!available || !current) return false;
            if (available.year > current.year) return true;
            if (available.year === current.year && available.month > current.month) return true;
            return false;
        }

        /**
         * Check if any available OS in the dropdown is newer than installed
         */
        function checkForNewerOS() {
            var currentVersion = parseOSVersion(currentOSRelease);
            if (!currentVersion) {
                // Can't determine current version, fall back to old behavior
                return $('#osSelect option').length > 1;
            }

            var hasNewerOS = false;
            $('#osSelect option').each(function () {
                if (this.value !== '') {
                    var availableVersion = parseOSVersion(this.text);
                    if (isNewerOSVersion(availableVersion, currentVersion)) {
                        hasNewerOS = true;
                        return false;
                    }
                }
            });

            return hasNewerOS;
        }

        // Cached git origin log data
        var gitOriginLogCache = null;

        /**
         * Fetch git origin log with caching
         */
        function fetchGitOriginLog(callback, forceRefresh) {
            if (!forceRefresh && gitOriginLogCache !== null) {
                callback(gitOriginLogCache);
                return;
            }

            $.get('api/git/originLog', function (data) {
                gitOriginLogCache = data;
                callback(data);
            }).fail(function () {
                callback(null);
            });
        }

        function buildGitLogTableHtml(data, options) {
            options = options || {};
            var showDate = options.showDate || false;
            var commitWidth = options.commitWidth || '80px';
            var authorWidth = options.authorWidth || '150px';
            var dateWidth = options.dateWidth || '150px';
            var colspan = showDate ? 4 : 3;
            var emptyHtml = options.emptyHtml || '<tr><td colspan="' + colspan + '" class="text-center">No entries found</td></tr>';

            var html = '<table class="table table-striped table-sm">';
            html += '<thead><tr>';
            html += '<th width="' + commitWidth + '">Commit</th>';
            html += '<th width="' + authorWidth + '">Author</th>';
            if (showDate) {
                html += '<th width="' + dateWidth + '">Date</th>';
            }
            html += '<th>Message</th></tr></thead>';
            html += '<tbody>';

            if (data && data.rows && data.rows.length > 0) {
                data.rows.forEach(function (row) {
                    html += '<tr>';
                    html += '<td><code>' + row.hash.substring(0, 8) + '</code></td>';
                    html += '<td>' + row.author + '</td>';
                    if (showDate) {
                        html += '<td class="fpp-text-date">' + (row.date || '') + '</td>';
                    }
                    html += '<td>' + row.msg + '</td>';
                    html += '</tr>';
                });
            } else {
                html += emptyHtml;
            }

            html += '</tbody></table>';
            return html;
        }

        /**
         * Get commit count from cached or fresh git log data
         */
        function getGitCommitCount(callback) {
            fetchGitOriginLog(function (data) {
                var count = (data && data.rows) ? data.rows.length : 0;
                callback(count);
            });
        }

        // Helper: Show a loading modal with spinner
        function ShowLoadingModal(id, title, modalClass) {
            DoModalDialog({
                id: id,
                title: title,
                body: '<div class="text-center"><i class="fas fa-spinner fa-spin"></i> Loading...</div>',
                class: modalClass || 'modal-lg',
                keyboard: true,
                backdrop: true
            });
        }

        // Check upgrade scenarios and show appropriate banners
        function checkUpgradeRecommendation() {
            // Major FPP version upgrades: OS banner already configured in UpdateVersionInfo(), no separate recommendation needed
            if (isMajorVersionUpgrade) {
                $('#upgradeRecommendationBanner').hide();
                $('#fppRecommendedBadge').hide();
                $('#osRecommendedBadge').hide();
                return;
            }

            if (fppUpdateAvailable && osUpgradeAvailable) {
                // Both FPP update and OS upgrade available (non-major version)
                $('#upgradeRecommendationTitle').text('Recommended: Update FPP Software First');
                $('#upgradeRecommendationMessage').text(
                    'Both a software update and OS upgrade are available. We recommend updating ' +
                    'FPP software first - it\'s quick (2-5 min). It resolves any bugs and provides a clean upgrade path. '
                );
                $('#fppRecommendedBadge').show();
                $('#osRecommendedBadge').hide();
                $('#upgradeRecommendationBanner').show();
                $('#osUpdateBanner').hide();
            } else if (!fppUpdateAvailable && osUpgradeAvailable) {
                // FPP is up to date, but OS upgrade available
                $('#upgradeRecommendationBanner').hide();
                $('#fppRecommendedBadge').hide();
                $('#osRecommendedBadge').hide();
                // Show OS banner
                $('#osUpdateBanner')
                    .removeClass('fpp-banner--success')
                    .addClass('fpp-banner--warning')
                    .show();
                $('#osUpdateBanner .fpp-banner__title').text('Operating System Upgrade Available');
                $('#osUpdateBanner .fpp-banner__message').text(
                    'A newer OS version is available. OS upgrades include security patches, new hardware support, and system improvements. Always backup first!'
                );
                $('#osUpdateBanner .fpp-banner__icon i').removeClass('fa-arrow-circle-up').addClass('fa-exclamation-triangle');
            } else {
                $('#upgradeRecommendationBanner').hide();
                $('#fppRecommendedBadge').hide();
                $('#osRecommendedBadge').hide();
                $('#osUpdateBanner').hide();
            }
        }

        function GetGitOriginLog() {
            ShowLoadingModal('gitOriginLogModal', 'Pending Git Changes');

            var emptyHtml = '<tr><td colspan="3" class="text-center">' +
                '<div class="alert alert-info mb-0">' +
                '<i class="fas fa-info-circle"></i> ' +
                'Unable to determine pending changes. This may occur when working with feature branches or forks. ' +
                'You can still update to get the latest changes using the "Update FPP Now" button.' +
                '</div></td></tr>';

            fetchGitOriginLog(function (data) {
                if (data === null) {
                    $('#gitOriginLogModal .modal-body').html('<div class="alert alert-danger">Failed to load git changes</div>');
                    return;
                }

                var html = buildGitLogTableHtml(data, {
                    showDate: false,
                    emptyHtml: emptyHtml
                });

                $('#gitOriginLogModal .modal-body').html(html);
            }, true); // force refresh
        }

        // Track what type of update is available
        var branchUpgradeData = null;
        var isMajorVersionUpgrade = false;
        var isEndOfLife = false;

        function UpdateVersionInfo(testMode) {
            // Fetch system status for version info
            $.get('api/system/status', function (data) {
                if (data.advancedView) {
                    if (data.advancedView.Version) {
                        $('#fppVersionValue').text(data.advancedView.Version);
                    }
                    if (data.advancedView.Platform) {
                        $('#platformValue').text(data.advancedView.Platform);
                    }
                    if (data.advancedView.Variant) {
                        $('#variantValue').text(data.advancedView.Variant);
                    }
                    if (data.advancedView.Mode) {
                        $('#modeValue').text(data.advancedView.Mode);
                    }
                    if (data.advancedView.HostName) {
                        $('#hostnameValue').text(data.advancedView.HostName);
                    }
                    if (data.advancedView.HostDescription) {
                        $('#hostDescValue').text(data.advancedView.HostDescription);
                    }
                    if (data.advancedView.OSVersion) {
                        $('#osVersionValue').text(data.advancedView.OSVersion);
                        $('#osCurrentVersionBadge').text(data.advancedView.OSVersion);
                        currentOSRelease = data.advancedView.OSVersion;
                        if ($('#osSelect option').length > 1) {
                            osUpgradeAvailable = checkForNewerOS();
                        }
                    }
                    if (data.advancedView.OSRelease) {
                        $('#osReleaseValue').text(data.advancedView.OSRelease);
                    }
                    if (data.advancedView.Kernel) {
                        $('#kernelValue').text(data.advancedView.Kernel);
                    }

                    if (data.advancedView.LocalGitVersion) {
                        $('#localGitValue').text(data.advancedView.LocalGitVersion);
                        $('#localGitShort').text(data.advancedView.LocalGitVersion);
                    }

                    $('#osVersionStatusBadge').show();
                }
            });

            // Fetch unified update status
            var updateStatusUrl = 'api/system/updateStatus';
            if (testMode) {
                updateStatusUrl += '?test=' + testMode;
            }
            $.get(updateStatusUrl, function (updateData) {
                if (updateData.status !== 'OK') return;

                var isAdvancedView = settings['uiLevel'] && (parseInt(settings['uiLevel']) >= 1);

                if (isAdvancedView) {
                    $('#fppVersionStandard').hide();
                    $('#fppVersionAdvanced').show();
                } else {
                    $('#fppVersionStandard').show();
                    $('#fppVersionAdvanced').hide();
                }

                // Hide all standard view states
                $('#fppVersionStandardBranchUpgrade, #fppVersionStandardCommitUpdate, #fppVersionStandardCurrent').hide();

                // Check for End of Life status
                isEndOfLife = updateData.isEndOfLife || false;
                if (isEndOfLife) {
                    $('#eolCurrentVersion').text('v<?= getFPPMajorVersion() ?>');
                    $('#eolLatestVersion').text('v' + updateData.latestMajorVersion);
                    $('#eolBanner').show();
                } else {
                    $('#eolBanner').hide();
                }

                if (updateData.branchUpgradeAvailable) {
                    // Branch upgrade available - takes priority
                    branchUpgradeData = updateData;
                    fppUpdateAvailable = true;

                    isMajorVersionUpgrade = updateData.isMajorVersionUpgrade || false;

                    if (isMajorVersionUpgrade) {
                        // Major version upgrades REQUIRE OS upgrade
                        $('#gitUpdateBadge').text('Requires OS Upgrade').show();
                        $('#fppUpdateBanner').hide();

                        // Show OS banner
                        $('#osUpdateBanner')
                            .removeClass('fpp-banner--warning')
                            .addClass('fpp-banner--success')
                            .show();
                        $('#osUpdateBanner .fpp-banner__title').text('FPP ' + updateData.branchUpgradeVersion + ' Available - OS Upgrade Required');
                        $('#osUpdateBanner .fpp-banner__message').html(
                            'A new major version of FPP is available! Major version upgrades require a fresh OS image. ' +
                            'Please <a href="backup.php">backup your configuration</a> first, then select the matching OS image below.'
                        );
                        $('#osUpdateBanner .fpp-banner__icon i').removeClass('fa-exclamation-triangle').addClass('fa-arrow-circle-up');

                        $('#fppVersionStatusBadge').removeClass('text-bg-secondary text-bg-success').addClass('text-bg-warning').text('OS Upgrade Required');
                        // Signal that OS upgrade path should be used
                        osUpgradeAvailable = true;

                        // Standard view: show that OS upgrade is needed
                        $('#fppVersionStandardBranchUpgrade').show();
                        $('#fppTargetVersion').text('FPP ' + updateData.branchUpgradeVersion);
                        // Add visual indication that this requires OS upgrade
                        $('#fppVersionStandardBranchUpgrade .badge').text('Requires OS Upgrade').removeClass('text-bg-warning').addClass('text-bg-primary');

                        // Advanced view
                        $('#fppVersionIndicator').show();
                        $('#fppVersionCurrent').hide();
                        $('#remoteGitShort').text(updateData.branchUpgradeTarget);
                        $('#commitCount').parent().hide();

                        // Disable FPP update button as users must use OS upgrade
                        $('#fppUpdateButton').prop('disabled', true);
                        $('#fppUpdateButtonText').text('Use OS Upgrade');
                    } else {
                        // Minor version branch upgrade
                        $('#gitUpdateBadge').text('Upgrade to ' + updateData.branchUpgradeTarget).show();
                        $('#fppUpdateBanner').show();
                        $('#fppVersionStatusBadge').removeClass('text-bg-secondary text-bg-success').addClass('text-bg-warning').text('Upgrade Available');

                        // Standard view: show branch upgrade
                        $('#fppVersionStandardBranchUpgrade').show();
                        $('#fppTargetVersion').text('FPP ' + updateData.branchUpgradeVersion);

                        $('#fppVersionIndicator')
                            .attr('onclick', 'HandleFPPUpdate();')
                            .attr('title', 'Click to see release notes')
                            .show();
                        $('#fppVersionIndicator .fpp-version-indicator__label')
                            .html('<i class="fas fa-file-alt"></i> Click to see release notes');
                        $('#fppVersionCurrent').hide();
                        $('#remoteGitShort').text(updateData.branchUpgradeTarget);

                        // Update button text for branch upgrade
                        $('#fppUpdateButton').prop('disabled', false);
                        $('#fppUpdateButtonText').text('Upgrade to ' + updateData.branchUpgradeTarget);
                    }

                } else if (updateData.commitUpdateAvailable) {
                    // Commit update available (same version, new commits)
                    branchUpgradeData = null;
                    fppUpdateAvailable = true;
                    isMajorVersionUpgrade = false;

                    $('#remoteGitShort').text(updateData.remoteCommit.substring(0, 9));
                    $('#gitUpdateBadge').text('Update Available').show();
                    $('#fppUpdateBanner').show();
                    $('#fppVersionStatusBadge').removeClass('text-bg-secondary text-bg-success').addClass('text-bg-warning').text('Update Available');

                    // Standard view: show commit update (no version arrow)
                    $('#fppVersionStandardCommitUpdate').show();

                    // Advanced view:  commit updates show git log
                    $('#fppVersionIndicator')
                        .attr('onclick', 'GetGitOriginLog();')
                        .attr('title', 'Click to preview changes')
                        .show();
                    $('#fppVersionCurrent').hide();

                    // Fetch commit count and update label
                    getGitCommitCount(function (count) {
                        if (count > 0) {
                            $('#commitCount').text(count);
                            $('#commitCountStandard').text(count);
                            $('#fppVersionIndicator .fpp-version-indicator__label')
                                .html('<i class="fas fa-search"></i> ' + count + ' changes behind');
                        }
                    });

                    // Button text for commit update
                    $('#fppUpdateButton').prop('disabled', false);
                    $('#fppUpdateButtonText').text('Update FPP Now');

                } else {
                    // Up to date
                    branchUpgradeData = null;
                    fppUpdateAvailable = false;
                    isMajorVersionUpgrade = false;

                    // Test mode: force OS upgrade available
                    if (updateData.forceOsUpgradeAvailable) {
                        osUpgradeAvailable = true;
                        forceOsUpgradeTest = true;
                    }

                    $('#gitUpdateBadge').hide();
                    $('#fppUpdateBanner').hide();
                    // Don't hide OS banner here - let checkUpgradeRecommendation() handle it
                    // based on whether osUpgradeAvailable is set
                    $('#fppVersionStatusBadge').removeClass('text-bg-secondary text-bg-warning').addClass('text-bg-success').text('Up to Date');

                    // When up to date: disable button for basic users, keep enabled for advanced
                    if (isAdvancedView) {
                        $('#fppUpdateButton').prop('disabled', false);
                        $('#fppUpdateButtonText').text('Update FPP Now');
                    } else {
                        $('#fppUpdateButton').prop('disabled', true);
                        $('#fppUpdateButtonText').text('Up to Date');
                    }

                    // Standard view
                    $('#fppVersionStandardCurrent').show();

                    // Advanced view
                    $('#fppVersionIndicator').hide();
                    $('#fppVersionCurrent').show();
                }

                checkUpgradeRecommendation();
            }).fail(function () {
                $('#fppVersionStatusBadge').removeClass('text-bg-secondary text-bg-success text-bg-warning').addClass('text-bg-secondary').text('Unknown');
            });
        }

        // Handle FPP update button click - route to appropriate action
        function HandleFPPUpdate() {
            if (branchUpgradeData && branchUpgradeData.branchUpgradeAvailable) {
                // Branch upgrade: show release notes modal with upgrade button
                ViewReleaseNotes(branchUpgradeData.branchUpgradeVersion);
            } else {
                // Commit update: direct update
                UpgradeFPP();
            }
        }

        function UpgradeFPP() {
            DisplayProgressDialog('fppUpgrade', 'FPP Upgrade');
            StreamURL('manualUpdate.php?wrapped=1', 'fppUpgradeText', 'FPPUpgradeDone');
        }

        function FPPUpgradeDone() {
            $('#fppUpgradeCloseButton').prop("disabled", false);
            EnableModalDialogCloseButton("fppUpgrade");
            UpdateVersionInfo();
        }

        function PopulateOSSelect() {
            <?php if ($freeSpace > 1000000000) { ?>

                var allPlatforms = '';
                if ($('#allPlatforms').is(':checked')) {
                    allPlatforms = 'api/git/releases/os/all';
                } else {
                    allPlatforms = 'api/git/releases/os';
                }

                //cleanup previous load values
                $('#osSelect option').filter(function () { return parseInt(this.value) > 0; }).remove();

                var osImagePrefix = '<?= $settings['OSImagePrefix'] ?>';
                var is64BitDevice = <?= !empty($settings['Is64Bit']) ? 'true' : 'false' ?>;

                function matchesDeviceOSBuild(filename) {
                    var name = filename || '';
                    var has64Marker = /(^|[-_])(64|64bit|aarch64|arm64)([-_.]|$)/i.test(name);
                    var has32Marker = /(^|[-_])(32|32bit|armv7|armv7l|armhf|arm32)([-_.]|$)/i.test(name);

                    if ((osImagePrefix === 'Pi' || osImagePrefix === 'Pi64') && !/^(Pi|Pi64)-/i.test(name)) {
                        return false;
                    }
                    if ((osImagePrefix === 'BBB' || osImagePrefix === 'BB64') && !/^(BBB|BB64)-/i.test(name)) {
                        return false;
                    }
                    if (osImagePrefix !== 'Pi' && osImagePrefix !== 'Pi64' && osImagePrefix !== 'BBB' && osImagePrefix !== 'BB64' && !name.startsWith(osImagePrefix + '-')) {
                        return false;
                    }

                    if (has64Marker && !is64BitDevice) {
                        return false;
                    }
                    if (has32Marker && is64BitDevice) {
                        return false;
                    }

                    if (!has64Marker && !has32Marker) {
                        if (is64BitDevice && (osImagePrefix === 'Pi64' || osImagePrefix === 'BB64')) {
                            return name.startsWith(osImagePrefix + '-');
                        }
                        if (!is64BitDevice && (osImagePrefix === 'Pi' || osImagePrefix === 'BBB')) {
                            return name.startsWith(osImagePrefix + '-');
                        }
                    }

                    return true;
                }

                $.get(allPlatforms, function (data) {
                    var devMode = (settings['uiLevel'] && (parseInt(settings['uiLevel']) == 3));
                    var showLegacy = $('#LegacyOS').is(':checked');
                    // Regex to match versions below 9.0 (With N-1 - update this yearly)
                    var legacyVersionRegex = /[-_]v?[0-8]\./i;

                    if (showLegacy) {
                        $('#legacyOSWarning').show();
                    } else {
                        $('#legacyOSWarning').hide();
                    }

                    if ("files" in data) {
                        for (const file of data["files"]) {
                            osAssetMap[file["asset_id"]] = {
                                name: file["filename"],
                                url: file["url"]
                            };

                            var isLegacyVersion = legacyVersionRegex.test(file['filename']);
                            if (isLegacyVersion && !showLegacy && !devMode) {
                                continue;
                            }

                            // Only show nightly OS builds in Dev mode
                            var isNightlyBuild = file["prerelease"] === true;
                            if (isNightlyBuild && !devMode) {
                                continue;
                            }

                            if (!file["downloaded"]) {
                                $('#osSelect').append($('<option>', {
                                    value: file["asset_id"],
                                    text: file["filename"] + " (download)"
                                }));
                            }
                            if (file["downloaded"]) {
                                $('#osSelect').append($('<option>', {
                                    value: file["asset_id"],
                                    text: file["filename"]
                                }));
                            }
                        }
                    }

                    //only show alpha and beta images in Advanced ui
                    if (settings['uiLevel'] && (parseInt(settings['uiLevel']) >= 1)) {
                        //leave all avail options in place
                    } else {
                        $('#osSelect option').filter(function () { return (/beta/i.test(this.text)); }).remove();
                        $('#osSelect option').filter(function () { return (/alpha/i.test(this.text)); }).remove();
                    }

                    //insert files already downloaded if we haven't got them from the git api call
                    var osUpdateFiles = <?php echo json_encode($osUpdateFiles); ?>;
                    var select = $('#osSelect');
                    osUpdateFiles.forEach(element => {
                        var isLegacyVersion = legacyVersionRegex.test(element);
                        if (isLegacyVersion && !showLegacy && !devMode) {
                            return;
                        }
                        if (/nightly/i.test(element) && !devMode) {
                            return;
                        }
                        if (!matchesDeviceOSBuild(element)) {
                            return;
                        }
                        if (select.has('option:contains("' + element + '")').length == 0) {
                            $('#osSelect').append($('<option>', {
                                value: element,
                                text: element
                            }));
                        }
                    });

                    if (!forceOsUpgradeTest) {
                        osUpgradeAvailable = checkForNewerOS();
                    }
                    checkUpgradeRecommendation();
                }).fail(function () {
                    // API failed - still show any locally downloaded OS files
                    if (!forceOsUpgradeTest) {
                        osUpgradeAvailable = checkForNewerOS();
                    }
                    checkUpgradeRecommendation();
                });

            <?php } ?>
        }

        function UpgradeOS() {
            var os = $('#osSelect').val();
            var osName = os;

            if (os == '') {
                DialogError('No OS Selected', 'Please select an OS version to upgrade to.');
                return;
            }
            if (os in osAssetMap) {
                osName = osAssetMap[os].name;
                os = osAssetMap[os].url;
            }

            //override file location from git to local if already downloaded
            if ($('#osSelect option:selected').text().toLowerCase().indexOf('(download)') === -1) {
                os = $('#osSelect option:selected').text();
                osName = $('#osSelect option:selected').text();
            }

            var keepOptFPP = '';
            if ($('#keepOptFPP').is(':checked')) {
                keepOptFPP = '&keepOptFPP=1';
            }

            if (confirm('Upgrade the OS using ' + osName +
                '?\nThis can take a long time. It is also strongly recommended to run FPP backup first.')) {

                DisplayProgressDialog('osUpgrade', 'FPP OS Upgrade');
                StreamURL('upgradeOS.php?wrapped=1&os=' + os + keepOptFPP, 'osUpgradeText', 'OSUpgradeDone', 'OSUpgradeDone');
            }
        }

        function OSUpgradeDone() {
            $("#osUpgradeCloseButton").prop("disabled", false);
            EnableModalDialogCloseButton("osUpgrade");
            UpdateVersionInfo();
        }

        function DownloadOS() {
            var os = $('#osSelect').val();
            var osName = os;

            if (os == '')
                return;

            if (os in osAssetMap) {
                osName = osAssetMap[os].name;
                os = osAssetMap[os].url;

                DisplayProgressDialog('osDownload', 'FPP Download OS Image');
                StreamURL('upgradeOS.php?wrapped=1&downloadOnly=1&os=' + os, 'osDownloadText', 'OSDownloadDone');
            } else {
                alert('This fppos image has already been downloaded.');
            }
        }

        function OSDownloadDone() {
            $("#osDownloadCloseButton").prop("disabled", false);
            EnableModalDialogCloseButton("osDownload");
            PopulateOSSelect();
        }

        function ViewOSReleaseNotes() {
            var osVersion = $('#osSelect option:selected').text();

            if (!osVersion || osVersion == '-- Choose an OS Version --') {
                DialogError('No OS Selected', 'Please select an OS version first to view its release notes.');
                return;
            }

            ShowLoadingModal('osReleaseNotesModal', 'OS Release Notes: ' + osVersion);

            // Nightly builds don't have published GitHub releases
            if (/nightly/i.test(osVersion)) {
                var html = '<div class="fpp-alert fpp-alert--warning" style="display: block;">';
                html += '<div><i class="fas fa-info-circle"></i> <strong>Nightly builds do not have published release notes.</strong></div>';
                html += '<div class="mt-3">Nightly builds track the latest development changes and are rebuilt automatically. To see what\'s changed, browse recent commits or releases on GitHub.</div>';
                html += '<div class="mt-3">';
                html += '<a href="https://github.com/FalconChristmas/fpp/commits/master" target="_blank" class="fpp-btn fpp-btn--outline">';
                html += '<i class="fas fa-external-link-alt"></i> View Recent Commits on GitHub';
                html += '</a>';
                html += '</div>';
                html += '</div>';
                $('#osReleaseNotesModal .modal-body').html(html);
                return;
            }

            // Extract version tag from OS filename (e.g., BBB-9.3_2025-11.fppos -> 9.3)
            var versionMatch = osVersion.match(/(v?\d+\.\d+(?:\.\d+)?(?:-[a-z]+\d*)?)/i);
            if (!versionMatch) {
                var html = '<div class="fpp-alert fpp-alert--warning" style="display: block;">';
                html += '<div><i class="fas fa-info-circle"></i> <strong>Could not determine version number from selected OS.</strong></div>';
                html += '<div class="mt-3">';
                html += '<a href="https://github.com/FalconChristmas/fpp/releases" target="_blank" class="fpp-btn fpp-btn--outline">';
                html += '<i class="fas fa-external-link-alt"></i> Browse All Releases on GitHub';
                html += '</a>';
                html += '</div>';
                html += '</div>';
                $('#osReleaseNotesModal .modal-body').html(html);
                return;
            }

            var version = versionMatch[1];
            // Remove 'v' prefix if present - GitHub tags use numeric version (e.g., 9.3, not v9.3)
            if (version.startsWith('v') || version.startsWith('V')) {
                version = version.substring(1);
            }

            // Fetch release notes from GitHub API
            $.ajax({
                url: 'https://api.github.com/repos/FalconChristmas/fpp/releases/tags/' + version,
                dataType: 'json',
                success: function (release) {
                    var html = '<div style=\"max-height: 70vh; overflow-y: auto;\">';

                    if (release.name) {
                        html += '<h4>' + release.name + '</h4>';
                    }

                    if (release.published_at) {
                        var date = new Date(release.published_at);
                        html += '<p class=\"text-muted\"><i class=\"fas fa-calendar\"></i> Published: ' + date.toLocaleDateString() + '</p>';
                    }

                    if (release.body) {
                        // Convert markdown to HTML
                        var body = release.body
                            // Escape HTML
                            .replace(/&/g, '&amp;')
                            .replace(/</g, '&lt;')
                            .replace(/>/g, '&gt;')
                            // Headers
                            .replace(/^### (.+)$/gm, '<h5>$1</h5>')
                            .replace(/^## (.+)$/gm, '<h4>$1</h4>')
                            .replace(/^# (.+)$/gm, '<h3>$1</h3>')
                            // Bold
                            .replace(/\*\*(.+?)\*\*/g, '<strong>$1</strong>')
                            .replace(/__(.+?)__/g, '<strong>$1</strong>')
                            // Italic
                            .replace(/\*(.+?)\*/g, '<em>$1</em>')
                            .replace(/_(.+?)_/g, '<em>$1</em>')
                            // Code blocks
                            .replace(/```(.+?)```/gs, '<pre><code>$1</code></pre>')
                            .replace(/`(.+?)`/g, '<code>$1</code>')
                            // Links
                            .replace(/\[(.+?)\]\((.+?)\)/g, '<a href="$2" target="_blank">$1</a>')
                            // Line breaks and paragraphs
                            .replace(/\n\n/g, '</p><p>')
                            .replace(/\n/g, '<br>');

                        // Wrap lists in ul tags
                        body = body.replace(/(<br>)?- (.+?)(<br>|<\/p>)/g, function (match, br1, content, br2) {
                            return '<li>' + content + '</li>';
                        });
                        body = body.replace(/(<li>.*?<\/li>)+/g, function (match) {
                            return '<ul>' + match + '</ul>';
                        });

                        html += '<div class=\"release-notes-body\"><p>' + body + '</p></div>';
                    } else {
                        html += '<p class=\"text-muted\">No release notes available for this version.</p>';
                    }

                    html += '<div class=\"mt-3\">';
                    html += '<a href=\"' + release.html_url + '\" target=\"_blank\" class=\"btn btn-outline-primary\">';
                    html += '<i class=\"fas fa-external-link-alt\"></i> View Full Release on GitHub';
                    html += '</a>';
                    html += '</div>';
                    html += '</div>';

                    $('#osReleaseNotesModal .modal-body').html(html);
                },
                error: function () {
                    var html = '<div class="fpp-alert fpp-alert--warning" style="display: block;">';
                    html += '<div><i class="fas fa-info-circle"></i> <strong>Release notes not found for version ' + version + '.</strong></div>';
                    html += '<div class="mt-3">This may be because:</div>';
                    html += '<ul style="margin: 0.5rem 0 1rem 1.5rem; padding: 0;">';
                    html += '<li>This is a minor version update. Refer to the major version for release notes.</li>';
                    html += '<li>The release notes are not available on GitHub</li></ul>';
                    html += '<a href="https://github.com/FalconChristmas/fpp/releases" target="_blank" class="fpp-btn fpp-btn--outline">';
                    html += '<i class="fas fa-external-link-alt"></i> Browse All Releases on GitHub';
                    html += '</a>';
                    html += '</div>';
                    $('#osReleaseNotesModal .modal-body').html(html);
                }
            });
        }

        function OSSelectChanged() {
            var os = $('#osSelect').val();
            <?php
            // we want at least a 200MB in order to be able to apply the fppos
            if ($freeSpace < 200000000) {
                echo "os = '';\n";
            } ?>
            if (os == '') {
                $('#osUpgradeButton').attr('disabled', 'disabled');
                $('#osDownloadButton').attr('disabled', 'disabled');
                $('#osReleaseNotesButton').attr('disabled', 'disabled');
            } else {
                $('#osUpgradeButton').removeAttr('disabled');
                $('#osReleaseNotesButton').removeAttr('disabled');
                if (os in osAssetMap) {
                    $('#osDownloadButton').removeAttr('disabled');
                } else {
                    $('#osDownloadButton').attr('disabled', 'disabled');
                }
            }
        }

        function initFaqAccordion() {
            document.querySelectorAll('.fpp-faq__item').forEach(item => {
                item.querySelector('.fpp-faq__question').addEventListener('click', function () {
                    const isOpen = item.classList.contains('fpp-faq__item--open');
                    document.querySelectorAll('.fpp-faq__item').forEach(i => i.classList.remove('fpp-faq__item--open'));
                    if (!isOpen) item.classList.add('fpp-faq__item--open');
                });
            });
        }

        // Test mode support: append ?test=branch (or commit, both, uptodate, major/eol, osonly)
        var upgradeTestMode = new URLSearchParams(window.location.search).get('test');

        $(document).ready(function () {
            UpdateVersionInfo(upgradeTestMode);
            PopulateOSSelect();
            initFaqAccordion();

            if (upgradeTestMode) {
                console.log('Upgrade test mode: ' + upgradeTestMode);
            }
        });
    </script>
</head>

<body>
    <div id="bodyWrapper">
        <?php
        $activeParentMenuItem = 'status';
        include 'menu.inc';
        ?>
        <div class="mainContainer">
            <h1 class="title">System Upgrade</h1>
            <div class="pageContent">

                <!-- Update Banners (conditionally shown) -->
                <div id="eolBanner" class="fpp-banner fpp-banner--danger" style="display: none;">
                    <div class="fpp-banner__icon">
                        <i class="fas fa-exclamation-circle"></i>
                    </div>
                    <div class="fpp-banner__content">
                        <div class="fpp-banner__title">End of Life Version</div>
                        <p class="fpp-banner__message">
                            You are running FPP <span id="eolCurrentVersion"></span>, which has reached End of Life.
                            This version no longer receives bug fixes or security updates.
                            Please upgrade to FPP <span id="eolLatestVersion"></span> via OS Upgrade to continue
                            receiving support.
                        </p>
                    </div>
                </div>

                <div id="fppUpdateBanner" class="fpp-banner fpp-banner--success" style="display: none;">
                    <div class="fpp-banner__icon">
                        <i class="fas fa-check-circle"></i>
                    </div>
                    <div class="fpp-banner__content">
                        <div class="fpp-banner__title">FPP Software Update Available!</div>
                        <p class="fpp-banner__message">A new version of the FPP software is ready to install. Updates
                            typically complete in under 5 minutes and keep all your settings.</p>
                    </div>
                </div>

                <div id="osUpdateBanner" class="fpp-banner fpp-banner--warning" style="display: none;">
                    <div class="fpp-banner__icon">
                        <i class="fas fa-exclamation-triangle"></i>
                    </div>
                    <div class="fpp-banner__content">
                        <div class="fpp-banner__title">Operating System Upgrade Available</div>
                        <p class="fpp-banner__message">A major OS version is available. OS upgrades include security
                            patches, new hardware support, and system improvements. Always backup first!</p>
                    </div>
                </div>

                <!-- Upgrade Path Recommendation Banner (shown when both updates available) -->
                <div id="upgradeRecommendationBanner" class="fpp-banner fpp-banner--info" style="display: none;">
                    <div class="fpp-banner__icon">
                        <i class="fas fa-lightbulb"></i>
                    </div>
                    <div class="fpp-banner__content">
                        <div class="fpp-banner__title" id="upgradeRecommendationTitle">Recommended: Update FPP Software
                            First</div>
                        <p class="fpp-banner__message" id="upgradeRecommendationMessage">
                            Both a software update and OS upgrade are available. We recommend updating
                            FPP software first - it's quick (2-5 min) and may resolve any issues.
                            Consider the OS upgrade after if needed.
                        </p>
                    </div>
                </div>

                <!-- Version Information -->
                <div class="card fpp-card fpp-card--accent fpp-card--accent-neutral">
                    <div class="fpp-card__header-simple">
                        <h3>
                            <i class="fas fa-info-circle"></i>
                            Version Information
                        </h3>
                    </div>
                    <div class="row">
                        <div class="col-md-4 fpp-col-divider">
                            <div class="fpp-row">
                                <span class="fpp-row__label">FPP Version:</span>
                                <span class="fpp-row__value">
                                    <span id="fppVersionStatusBadge"
                                        class="badge text-bg-secondary">Checking...</span>
                                    <span id="fppVersionValue"><?= $fppVersion ?></span>
                                </span>
                            </div>
                            <div class="fpp-row">
                                <span class="fpp-row__label">OS Version:</span>
                                <span class="fpp-row__value">
                                    <span id="osVersionStatusBadge" class="badge text-bg-success"
                                        style="display: none;">Up to Date</span>
                                    <span id="osVersionValue">--</span>
                                </span>
                            </div>

                        </div>

                        <div class="col-md-4 fpp-col-divider">
                            <div class="fpp-row">
                                <span class="fpp-row__label">OS Build:</span>
                                <span class="fpp-row__value" id="osReleaseValue">--</span>
                            </div>
                            <div class="fpp-row">
                                <span class="fpp-row__label">Platform:</span>
                                <span class="fpp-row__value" id="platformValue">
                                    <?php
                                    echo $settings['Platform'];
                                    if (($settings['Variant'] != '') && ($settings['Variant'] != $settings['Platform'])) {
                                        echo " (" . $settings['Variant'] . ")";
                                    }
                                    ?>
                                </span>
                            </div>
                        </div>

                        <div class="col-md-4">
                            <?php if (isset($serialNumber) && $serialNumber != "") { ?>
                                <div class="fpp-row">
                                    <span class="fpp-row__label">Serial Number:</span>
                                    <span class="fpp-row__value"><?= $serialNumber ?></span>
                                </div>
                            <?php } ?>
                            <div class="fpp-row">
                                <span class="fpp-row__label">Kernel:</span>
                                <span class="fpp-row__value" id="kernelValue">--</span>
                            </div>
                        </div>
                    </div>
                </div>

                <!-- Upgrade Options -->
                <div class="row">
                    <!-- FPP Software Update -->
                    <div class="col-xl-6 fpp-col-flex">
                        <div class="card fpp-card fpp-card--accent fpp-card--accent-success fpp-card--flex">
                            <div class="fpp-card__header">
                                <div class="fpp-card__icon fpp-card__icon--success">
                                    <i class="fas fa-sync-alt"></i>
                                </div>
                                <div>
                                    <h3 class="fpp-card__title">
                                        Update FPP Software
                                        <span id="gitUpdateBadge" class="badge text-bg-success text-bg-sm"
                                            style="display: none;">Update Available</span>
                                        <span id="fppRecommendedBadge" class="badge text-bg-primary text-bg-sm"
                                            style="display: none;">Recommended</span>
                                    </h3>
                                    <p class="fpp-card__subtitle">Get the latest bug fixes and features. This is safe
                                        and quick.</p>
                                </div>
                            </div>

                            <?php $advancedInfoCollapse = isset($settings['uiLevel']) && $settings['uiLevel'] >= 1; ?>
                            <?php if ($advancedInfoCollapse) { ?><details class="fpp-info-collapsible">
                                <summary class="fpp-info-collapsible__summary">
                                    <span><i class="fas fa-info-circle"></i> When to use &amp; What it does</span>
                                    <i class="fas fa-chevron-down fpp-info-collapsible__chevron"></i>
                                </summary><?php } ?>
                            <div class="fpp-info-grid">
                                <div class="fpp-info-box fpp-info-box--neutral">
                                    <div class="fpp-info-box__title"><i class="fas fa-question-circle"></i> When to use
                                    </div>
                                    <ul>
                                        <li>When "Update Available" badge shows</li>
                                        <li>For latest bug fixes &amp; features</li>
                                        <li>Regular maintenance updates</li>
                                    </ul>
                                </div>
                                <div class="fpp-info-box fpp-info-box--info">
                                    <div class="fpp-info-box__title"><i class="fas fa-info-circle"></i> What it does
                                    </div>
                                    <p>Downloads the latest code changes for your version and rebuilds FPP. Typically
                                        takes 2-5 minutes. Reboots are not usually required.</p>
                                </div>
                            </div>
                            <?php if ($advancedInfoCollapse) { ?></details><?php } ?>

                            <!-- Standard View Version Indicators (uiLevel 0 - Basic) -->
                            <div id="fppVersionStandard" class="fpp-version-standard-wrapper">
                                <!-- Standard: Branch upgrade available (e.g., v9.4 → v9.5) -->
                                <div id="fppVersionStandardBranchUpgrade"
                                    class="fpp-version-indicator fpp-version-indicator--clickable"
                                    style="display: none;" onclick="HandleFPPUpdate();"
                                    title="Click to see release notes">
                                    <span class="fpp-version-indicator__current"><?= $fppVersionDisplay ?></span>
                                    <i class="fas fa-arrow-right fpp-version-indicator__arrow"></i>
                                    <span class="fpp-version-indicator__to" id="fppTargetVersion">Latest</span>
                                    <span class="badge text-bg-warning text-bg-sm">Upgrade Available</span>
                                    <span
                                        class="fpp-version-indicator__label fpp-version-indicator__label--subtle">Click
                                        to see release notes</span>
                                </div>

                                <!-- Standard: Commit updates available (same version, new commits) -->
                                <div id="fppVersionStandardCommitUpdate" class="fpp-version-indicator"
                                    style="display: none;">
                                    <span class="fpp-version-indicator__current"><?= $fppVersionDisplay ?></span>
                                    <span class="badge text-bg-success text-bg-sm">Update Available</span>
                                    <span
                                        class="fpp-version-indicator__label fpp-version-indicator__label--subtle"><span
                                            id="commitCountStandard"></span> updates ready to install</span>
                                </div>

                                <!-- Standard: Up to date -->
                                <div id="fppVersionStandardCurrent"
                                    class="fpp-version-indicator fpp-version-indicator--current" style="display: none;">
                                    <i class="fas fa-check-circle"></i>
                                    <span class="fpp-version-indicator__current"><?= $fppVersionDisplay ?></span>
                                    <span class="fpp-version-indicator__label">You're up to date!</span>
                                </div>
                            </div>

                            <!-- Advanced View Version Indicators (uiLevel >= 1 - Advanced) -->
                            <div id="fppVersionAdvanced" class="fpp-version-advanced-wrapper" style="display: none;">
                                <!-- Advanced: Update available (git hashes) -->
                                <div id="fppVersionIndicator"
                                    class="fpp-version-indicator fpp-version-indicator--clickable"
                                    style="display: none;" onclick="GetGitOriginLog();"
                                    title="Click to preview changes">
                                    <span class="fpp-version-indicator__from"
                                        id="localGitShort"><?= $localGitVersion ?></span>
                                    <i class="fas fa-arrow-right fpp-version-indicator__arrow"></i>
                                    <span class="fpp-version-indicator__to" id="remoteGitShort"></span>
                                    <span class="fpp-version-indicator__label"><i class="fas fa-search"></i> <span
                                            id="commitCount">0</span> changes behind</span>
                                </div>

                                <!-- Advanced: Up to date -->
                                <div id="fppVersionCurrent" class="fpp-version-indicator fpp-version-indicator--current"
                                    style="display: none;">
                                    <i class="fas fa-check-circle"></i>
                                    <span class="fpp-version-indicator__current"
                                        id="localGitValue"><?= $localGitVersion ?></span>
                                    <span class="fpp-version-indicator__label">You're up to date!</span>
                                </div>
                            </div>

                            <div class="fpp-card__actions">
                                <button class="fpp-btn fpp-btn--success" id="fppUpdateButton"
                                    onclick="HandleFPPUpdate();">
                                    <i class="fas fa-download"></i> <span id="fppUpdateButtonText">Update FPP Now</span>
                                </button>
                                <?php
                                if ($settings['uiLevel'] > 0) {
                                    $upgradeSources = array();
                                    $remotes = getKnownFPPSystems();

                                    if ($settings["Platform"] != "MacOS") {
                                        $IPs = explode("\n", trim(shell_exec("/sbin/ifconfig -a | cut -f1 | cut -f1 -d' ' | grep -v ^$ | grep -v lo | grep -v eth0:0 | grep -v usb | grep -v SoftAp | grep -v 'can.' | sed -e 's/://g' | while read iface ; do /sbin/ifconfig \$iface | grep 'inet ' | awk '{print \$2}'; done")));
                                    } else {
                                        $IPs = explode("\n", trim(shell_exec("/sbin/ifconfig -a | grep 'inet ' | awk '{print \$2}'")));
                                    }
                                    $found = 0;
                                    foreach ($remotes as $desc => $host) {
                                        if ((!in_array($host, $IPs)) && (!preg_match('/^169\.254\./', $host))) {
                                            $upgradeSources[$desc] = $host;
                                            if (isset($settings['UpgradeSource']) && ($settings['UpgradeSource'] == $host)) {
                                                $found = 1;
                                            }
                                        }
                                    }
                                    if (!$found && isset($settings['UpgradeSource']) && ($settings['UpgradeSource'] != 'github.com')) {
                                        $upgradeSources = array($settings['UpgradeSource'] . ' (Unreachable)' => $settings['UpgradeSource'], 'github.com' => 'github.com') + $upgradeSources;
                                    } else {
                                        $upgradeSources = array("github.com" => "github.com") + $upgradeSources;
                                    }
                                    ?>
                                    <div class="fpp-advanced-options">
                                        <span class="badge text-bg-primary">Adv</span>
                                        <span>Source:</span>
                                        <?php PrintSettingSelect("FPP Upgrade Source", "UpgradeSource", 0, 0, "github.com", $upgradeSources); ?>
                                    </div>
                                <?php } ?>
                            </div>
                        </div>
                    </div>

                    <!-- Operating System Upgrade -->
                    <div class="col-xl-6 fpp-col-flex">
                        <div class="card fpp-card fpp-card--accent fpp-card--accent-warning fpp-card--flex">
                            <div class="fpp-card__header">
                                <div class="fpp-card__icon fpp-card__icon--warning">
                                    <i class="fas fa-hdd"></i>
                                </div>
                                <div>
                                    <h3 class="fpp-card__title">
                                        Upgrade Operating System
                                        <span id="osRecommendedBadge" class="badge text-bg-primary text-bg-sm"
                                            style="display: none;">Recommended</span>
                                    </h3>
                                    <p class="fpp-card__subtitle">Upgrade the entire FPP operating system with a new
                                        version</p>
                                </div>
                                <div class="fpp-card__header-status">
                                    <span class="fpp-card__header-status-label">Current OS</span>
                                    <span class="badge text-bg-secondary" id="osCurrentVersionBadge">--</span>
                                </div>
                            </div>

                            <?php if ($advancedInfoCollapse) { ?><details class="fpp-info-collapsible">
                                <summary class="fpp-info-collapsible__summary">
                                    <span><i class="fas fa-info-circle"></i> When to use &amp; What it does</span>
                                    <i class="fas fa-chevron-down fpp-info-collapsible__chevron"></i>
                                </summary><?php } ?>
                            <div class="fpp-info-grid">
                                <div class="fpp-info-box fpp-info-box--neutral">
                                    <div class="fpp-info-box__title"><i class="fas fa-question-circle"></i> When to use
                                    </div>
                                    <ul>
                                        <li>Moving to a new major version (e.g., v9 to v10)</li>
                                        <li>Release notes specifically recommend it</li>
                                        <li>Experiencing OS issues</li>
                                        <li>Applying latest OS security patches</li>
                                    </ul>
                                    <div class="fpp-alert fpp-alert--warning fpp-alert--compact"
                                        style="margin-top: var(--fpp-sp-md); margin-bottom: var(--fpp-sp-sm);">
                                        <i class="fas fa-exclamation-triangle"></i>
                                        <span><strong>Warning:</strong> OS upgrade will reboot your system. Ensure no
                                            shows are running.</span>
                                    </div>
                                </div>
                                <div class="fpp-info-box fpp-info-box--info">
                                    <div class="fpp-info-box__title"><i class="fas fa-info-circle"></i> What it does
                                    </div>
                                    <p>Downloads a complete OS image and updates your current OS. Your media files are
                                        preserved, but backing up your configuration is strongly recommended.</p>
                                    <span class="fpp-text-info-dark fpp-note"><strong>Important:</strong> This takes
                                        15-30+ minutes and requires a reboot. <a href="backup.php">Backup
                                            first!</a></span>
                                    <span class="fpp-text-info-dark fpp-note"><strong>Architecture Note:</strong>
                                        Switching between 32-bit and 64-bit is not supported from this screen. To
                                        change architectures, flash a fresh image.</span>
                                </div>
                            </div>
                            <?php if ($advancedInfoCollapse) { ?></details><?php } ?>

                            <!-- Legacy OS warning (shown when checkbox is checked) -->
                            <div id="legacyOSWarning"
                                class="fpp-alert fpp-alert--warning fpp-alert--compact fpp-alert--mb-md"
                                style="display: none;">
                                <i class="fas fa-history"></i>
                                <span>Installing a legacy OS is generally not recommended unless you're troubleshooting
                                    a specific issue.</span>
                            </div>

                            <div class="fpp-card__actions">
                                <select id="osSelect" class="form-select fpp-select" onChange="OSSelectChanged();">
                                    <option value="">-- Select OS Image --</option>
                                </select>
                                <button class="fpp-btn fpp-btn--warning" id="osUpgradeButton" onclick="UpgradeOS();"
                                    disabled>
                                    <i class="fas fa-arrow-up"></i> Upgrade OS
                                </button>
                                <button class="fpp-btn fpp-btn--secondary" id="osDownloadButton" onclick="DownloadOS();"
                                    disabled>
                                    <i class="fas fa-cloud-download-alt"></i> Download Only
                                </button>
                                <button class="fpp-btn fpp-btn--outline" id="osReleaseNotesButton"
                                    onclick="ViewOSReleaseNotes();" disabled>
                                    <i class="fas fa-file-alt"></i> Release Notes
                                </button>
                            </div>

                            <?php if (isset($settings['uiLevel']) && $settings['uiLevel'] >= 1) { ?>
                                <div class="fpp-checkbox-options">
                                    <label class="fpp-checkbox-option">
                                        <input type="checkbox" id="allPlatforms" onChange="PopulateOSSelect();">
                                        <span class="badge text-bg-primary">Adv</span>
                                        Show All Platforms
                                        <img title='Show both BBB & Pi downloads' src='images/redesign/help-icon.svg'
                                            class='icon-help'>
                                    </label>
                                    <label class="fpp-checkbox-option">
                                        <input type="checkbox" id="LegacyOS" onChange="PopulateOSSelect();">
                                        <span class="badge text-bg-primary">Adv</span>
                                        Show Legacy OS
                                        <img title='Include historic OS releases in listing'
                                            src='images/redesign/help-icon.svg' class='icon-help'>
                                    </label>
                                    <?php if (isset($settings['uiLevel']) && $settings['uiLevel'] >= 3) { ?>
                                        <label class="fpp-checkbox-option fpp-checkbox-option--dev">
                                            <input type="checkbox" id="keepOptFPP">
                                            <span class="badge text-bg-graceful">Dev</span>
                                            Keep /opt/fpp
                                            <img title='WARNING: This will upgrade the OS but will not upgrade the FPP version running in /opt/fpp. This is useful for developers who are developing the code in /opt/fpp and just want the underlying OS upgraded.'
                                                src='images/redesign/help-icon.svg' class='icon-help'>
                                        </label>
                                    <?php } ?>
                                </div>
                            <?php } ?>
                        </div>
                    </div>
                </div>

                <?php if (isset($settings['uiLevel']) && $settings['uiLevel'] >= 1) { ?>
                    <!-- Revert to Previous Commit Card -->
                    <div class="card fpp-card fpp-card--accent fpp-card--accent-neutral fpp-card--compact fpp-card--inline">
                        <div class="fpp-card__content">
                            <h3 class="fpp-card__title">
                                <i class="fas fa-history"></i>
                                Revert to Previous Commit
                                <span class="badge text-bg-secondary">Advanced</span>
                            </h3>
                            <p class="fpp-card__subtitle">Need to roll back changes? Use the changelog to revert to a
                                previous git commit while keeping your configuration.</p>
                        </div>
                        <button class="fpp-btn fpp-btn--secondary" onclick="window.location.href='changelog.php';">
                            <i class="fas fa-external-link-alt"></i> View Changelog
                        </button>
                    </div>

                <?php } ?>

                <?php if (!isset($settings['cape-info']) || !isset($settings['cape-info']['verifiedKeyId']) || ($settings['cape-info']['verifiedKeyId'] != 'fp')) { ?>
                    <div id="donateBanner" class="fpp-donate-banner">
                        <h3 class="fpp-donate-banner__title">
                            <i class="fas fa-heart"></i> Support FPP Development
                        </h3>
                        <p class="fpp-donate-banner__text">
                            Help support the continued development of the Falcon Player. Your donation
                            helps fund equipment, hosting, and countless hours of development.
                        </p>
                        <form action="https://www.paypal.com/donate" method="post" target="_top">
                            <input type="hidden" name="hosted_button_id" value="ASF9XYZ2V2F5G" />
                            <button type="submit" class="fpp-donate-btn" title="Donate to the Falcon Player">
                                <svg class="paypal-logo" viewBox="0 0 24 24" width="17" height="17" fill="currentColor">
                                    <path
                                        d="M7.076 21.337H2.47a.641.641 0 0 1-.633-.74L4.944 3.72a.77.77 0 0 1 .757-.629h6.578c2.182 0 3.91.558 5.143 1.66 1.233 1.1 1.677 2.65 1.321 4.612-.042.236-.09.473-.152.707a7.092 7.092 0 0 1-.906 2.326c-.402.627-.905 1.16-1.5 1.586-.596.426-1.297.756-2.09.986-.792.23-1.666.345-2.604.345h-1.58a.95.95 0 0 0-.938.803l-.692 4.39-.394 2.5a.641.641 0 0 1-.633.531h-.278zm11.461-14.02c-.014.084-.03.168-.048.254-.593 3.044-2.623 4.095-5.215 4.095h-1.32a.641.641 0 0 0-.633.543l-.676 4.282-.383 2.43a.336.336 0 0 0 .332.39h2.333a.564.564 0 0 0 .557-.476l.023-.12.441-2.8.028-.154a.564.564 0 0 1 .557-.476h.35c2.268 0 4.042-.921 4.561-3.585.217-1.113.105-2.042-.47-2.695a2.238 2.238 0 0 0-.637-.488z" />
                                </svg>
                                Donate with PayPal
                            </button>
                            <img alt="" src="https://www.paypal.com/en_US/i/scr/pixel.gif" width="1" height="1"
                                style="display:none;" />
                        </form>
                        <p class="fpp-donate-banner__footer">
                            <i class="fas fa-coffee"></i> It takes a lot of time, equipment, and coffee to power your shows!
                        </p>
                    </div>
                <?php } ?>

                <!-- Comparison & FAQ Section - Side by Side -->
                <div class="fpp-info-section">
                    <!-- Comparison Panel -->
                    <div class="fpp-info-panel">
                        <h4 class="fpp-info-panel__title"><i class="fas fa-balance-scale"></i> Quick Comparison</h4>
                        <table class="fpp-comparison-table">
                            <thead>
                                <tr>
                                    <th></th>
                                    <th>Update FPP</th>
                                    <th>Upgrade OS</th>
                                </tr>
                            </thead>
                            <tbody>
                                <tr>
                                    <td>Time</td>
                                    <td class="fpp-text-success">2-5 min</td>
                                    <td class="fpp-text-warning">15-30+ min</td>
                                </tr>
                                <tr>
                                    <td>Reboot</td>
                                    <td class="fpp-text-warning"><i class="fas fa-check"></i> Sometimes</td>
                                    <td class="fpp-text-danger"><i class="fas fa-times"></i> Yes</td>
                                </tr>
                                <tr>
                                    <td>Settings</td>
                                    <td class="fpp-text-success"><i class="fas fa-check"></i> Kept</td>
                                    <td class="fpp-text-warning"><i class="fas fa-exclamation"></i> Backup*</td>
                                </tr>
                                <tr>
                                    <td>Media Files</td>
                                    <td class="fpp-text-success"><i class="fas fa-check"></i> Kept</td>
                                    <td class="fpp-text-success"><i class="fas fa-check"></i> Kept</td>
                                </tr>
                                <tr>
                                    <td>Plugins</td>
                                    <td class="fpp-text-success"><i class="fas fa-check"></i> Kept</td>
                                    <td class="fpp-text-warning"><i class="fas fa-exclamation"></i> Reinstall*</td>
                                </tr>
                                <tr>
                                    <td>Risk Level</td>
                                    <td class="fpp-text-success">Low</td>
                                    <td class="fpp-text-warning">Medium</td>
                                </tr>
                                <tr>
                                    <td>OS Security Patches</td>
                                    <td class="fpp-text-warning"><i class="fas fa-times"></i> No</td>
                                    <td class="fpp-text-success"><i class="fas fa-check"></i> Yes</td>
                                </tr>
                                <tr>
                                    <td>Major Version Jump</td>
                                    <td class="fpp-text-warning"><i class="fas fa-times"></i> No</td>
                                    <td class="fpp-text-success"><i class="fas fa-check"></i> Yes</td>
                                </tr>
                                <tr>
                                    <td>New Hardware Support</td>
                                    <td class="fpp-text-warning"><i class="fas fa-times"></i> No</td>
                                    <td class="fpp-text-success"><i class="fas fa-check"></i> Yes</td>
                                </tr>
                            </tbody>
                        </table>
                        <p class="fpp-comparison-note">* Backup strongly recommended before OS upgrade</p>
                    </div>

                    <!-- FAQ Panel -->
                    <div class="fpp-info-panel">
                        <h4 class="fpp-info-panel__title"><i class="fas fa-question-circle"></i> Frequently Asked
                            Questions</h4>
                        <div class="fpp-faq" id="upgradeFaq">
                            <div class="fpp-faq__item">
                                <div class="fpp-faq__question">
                                    Which upgrade should I choose?
                                    <i class="fas fa-chevron-down"></i>
                                </div>
                                <div class="fpp-faq__answer">
                                    <div class="fpp-faq__answer-inner">
                                        For most users, <strong>"Update FPP Software"</strong> is all you need in
                                        season. It keeps your system current with bug fixes and new features. We
                                        recommend OS and major upgrades at least once a year.
                                    </div>
                                </div>
                            </div>
                            <div class="fpp-faq__item">
                                <div class="fpp-faq__question">
                                    When should I upgrade the OS?
                                    <i class="fas fa-chevron-down"></i>
                                </div>
                                <div class="fpp-faq__answer">
                                    <div class="fpp-faq__answer-inner">
                                        OS upgrades are typically only needed when moving to a new major FPP version,
                                        when release notes specifically recommend it, or if you're experiencing OS-level
                                        issues. It is recommend, at minimum, to do this once per year.
                                    </div>
                                </div>
                            </div>
                            <div class="fpp-faq__item">
                                <div class="fpp-faq__question">
                                    What's the difference between FPP and OS versions?
                                    <i class="fas fa-chevron-down"></i>
                                </div>
                                <div class="fpp-faq__answer">
                                    <div class="fpp-faq__answer-inner">
                                        <strong>FPP</strong> is the software that runs your display. <strong>OS</strong>
                                        is the underlying operating system (Debian Linux). They can be updated
                                        independently but major FPP versions usually require an OS Upgrade.
                                    </div>
                                </div>
                            </div>
                            <div class="fpp-faq__item">
                                <div class="fpp-faq__question">
                                    Can I roll back an upgrade?
                                    <i class="fas fa-chevron-down"></i>
                                </div>
                                <div class="fpp-faq__answer">
                                    <div class="fpp-faq__answer-inner">
                                        FPP updates can be rolled back via the changelog. OS upgrades are harder to
                                        reverse - always backup your configuration first!
                                    </div>
                                </div>
                            </div>
                            <div class="fpp-faq__item">
                                <div class="fpp-faq__question">
                                    Are my playlists and sequences safe?
                                    <i class="fas fa-chevron-down"></i>
                                </div>
                                <div class="fpp-faq__answer">
                                    <div class="fpp-faq__answer-inner">
                                        Yes! All upgrades preserve your media files. However, backing up before major
                                        upgrades is always a good practice.
                                    </div>
                                </div>
                            </div>
                            <div class="fpp-faq__item">
                                <div class="fpp-faq__question">
                                    Will my settings, playlists, schedules and sequences be preserved?
                                    <i class="fas fa-chevron-down"></i>
                                </div>
                                <div class="fpp-faq__answer">
                                    <div class="fpp-faq__answer-inner">
                                        Yes! Both update methods preserve your configuration files, sequences,
                                        playlists, and media files. Your settings are stored in a directory which is not
                                        affected by updates. However, it's always good practice to backup before major
                                        upgrades, especially OS upgrades, as they involve more significant changes to
                                        the underlying system.
                                    </div>
                                </div>
                            </div>
                        </div>
                    </div>
                </div>

                <!-- Resources -->
                <div class="fpp-info-panel fpp-info-panel--wide">
                    <h4 class="fpp-info-panel__title"><i class="fas fa-link"></i> Resources</h4>
                    <ul class="fpp-resources-list fpp-resources-list--spread">
                        <li><i class="fas fa-code-branch"></i> <a href="https://github.com/FalconChristmas/fpp"
                                target="_blank">GitHub Repository</a></li>
                        <li><i class="fas fa-book"></i> <a href="https://falconchristmas.github.io/FPP_Manual(9.x).pdf"
                                target="_blank">Documentation</a></li>
                        <li><i class="fas fa-microchip"></i> <a href="cape-info.php">Cape Info and EEPROM Signing</a>
                        </li>
                        <li><i class="fas fa-users"></i> <a href="https://www.facebook.com/groups/falconplayer"
                                target="_blank">Facebook Group</a></li>
                        <li><i class="fas fa-comments"></i> <a href="http://www.falconchristmas.com/forum"
                                target="_blank">Forums</a></li>
                        <li><i class="fas fa-bug"></i> <a href="https://github.com/FalconChristmas/fpp/issues"
                                target="_blank">Report Issues</a></li>
                        <li><i class="fas fa-heart"></i> <a href="system-stats.php">System Health</a></li>
                    </ul>
                </div>
            </div>
        </div>
    </div>
    <?php include 'common/footer.inc'; ?>
</body>

</html>