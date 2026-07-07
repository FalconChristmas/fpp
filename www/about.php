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
    <title><? echo $pageTitle; ?></title>
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
                    // Nightly builds are listed for dev users but should not
                    // trigger the "OS upgrade available" banner.
                    if (/nightly/i.test(this.text)) {
                        return;
                    }
                    var availableVersion = parseOSVersion(this.text);
                    if (isNewerOSVersion(availableVersion, currentVersion)) {
                        hasNewerOS = true;
                        return false;
                    }
                }
            });

            return hasNewerOS;
        }

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

        /**
         * Parse an FPPOS filename into sortable components.
         * Handles patterns like:
         *   Pi64-10.0-beta2_2025-07.fppos
         *   Pi-9.5.3_2025-07.fppos
         *   BBB-9.0_2025-07b.fppos
         *   Pi64-nightly-20250706.fppos
         * The optional " (download)" suffix is stripped before parsing.
         */
        function parseOSFilename(filename) {
            var name = filename.replace(/\s*\(download\)\s*$/i, '');
            var result = {
                platform: '',
                nightly: false,
                major: 0,
                minor: 0,
                patch: null,
                prereleaseType: null,
                prereleaseNum: null,
                rebuild: false,
                dateNum: 0,
                raw: name
            };

            var platMatch = name.match(/^(Pi64|Pi|BB64|BBB)-/i);
            if (!platMatch) {
                result.platform = name.split('-')[0] || 'zzz';
                return result;
            }
            result.platform = platMatch[1];

            var rest = name.substring(platMatch[0].length).replace(/\.fppos$/i, '');

            if (/nightly/i.test(rest)) {
                result.nightly = true;
                var ndm = rest.match(/(\d{4})-?(\d{2})(\d{2})?/);
                if (ndm) {
                    result.dateNum = parseInt(ndm[1] + (ndm[3] || ndm[2]));
                }
                return result;
            }

            var verMatch = rest.match(/^(\d+)\.(\d+)(?:\.(\d+))?/);
            if (!verMatch) return result;
            result.major = parseInt(verMatch[1]);
            result.minor = parseInt(verMatch[2]);
            if (verMatch[3] !== undefined) {
                result.patch = parseInt(verMatch[3]);
            }

            var afterVersion = rest.substring(verMatch[0].length);
            var preMatch = afterVersion.match(/^-(alpha|beta|rc)(\d*)/i);
            if (preMatch) {
                result.prereleaseType = preMatch[1].toLowerCase();
                result.prereleaseNum = preMatch[2] !== '' ? parseInt(preMatch[2]) : 0;
            }

            var dateMatch = rest.match(/_(\d{4})-?(\d{2})(\d{2})?/);
            if (dateMatch) {
                result.dateNum = parseInt(dateMatch[1] + (dateMatch[3] || dateMatch[2]));
            }

            result.rebuild = /b\.fppos$/i.test(name);

            return result;
        }

        /**
         * Compare two FPPOS filenames for sorting.
         * Order: nightly first, then by version (desc), final > prerelease,
         * rebuild > original, then Pi64 > Pi > BB64 > BBB, then date desc.
         */
        function compareOSFilenames(a, b) {
            var infoA = parseOSFilename(a);
            var infoB = parseOSFilename(b);

            if (infoA.nightly && !infoB.nightly) return -1;
            if (!infoA.nightly && infoB.nightly) return 1;
            if (infoA.nightly && infoB.nightly) {
                return infoB.dateNum - infoA.dateNum;
            }

            if (infoA.major !== infoB.major) return infoB.major - infoA.major;
            if (infoA.minor !== infoB.minor) return infoB.minor - infoA.minor;

            var patchA = infoA.patch !== null ? infoA.patch : -1;
            var patchB = infoB.patch !== null ? infoB.patch : -1;
            if (patchA !== patchB) return patchB - patchA;

            var prereleaseOrder = { 'alpha': 3, 'beta': 2, 'rc': 1 };
            var preA = infoA.prereleaseType ? (prereleaseOrder[infoA.prereleaseType] || 99) : -1;
            var preB = infoB.prereleaseType ? (prereleaseOrder[infoB.prereleaseType] || 99) : -1;
            if (preA !== preB) return preA - preB;

            if (infoA.prereleaseNum !== infoB.prereleaseNum) {
                return (infoB.prereleaseNum || 0) - (infoA.prereleaseNum || 0);
            }

            if (infoA.rebuild && !infoB.rebuild) return -1;
            if (!infoA.rebuild && infoB.rebuild) return 1;

            var platformOrder = { 'Pi64': 0, 'Pi': 1, 'BB64': 2, 'BBB': 3 };
            var platA = platformOrder[infoA.platform] !== undefined ? platformOrder[infoA.platform] : 99;
            var platB = platformOrder[infoB.platform] !== undefined ? platformOrder[infoB.platform] : 99;
            if (platA !== platB) return platA - platB;

            return infoB.dateNum - infoA.dateNum;
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

        // Set a quiet status dot (.fpp-upd-dot) on a version-info row.
        //   kind: 'ok' (up to date) | 'update' (available) | 'required' | 'unknown'
        function setVersionStatusDot(id, kind, text) {
            var cls = 'fpp-upd-dot';
            if (kind === 'ok' || kind === 'unknown') {
                cls += ' fpp-upd-dot--ok';
            } else if (kind === 'required') {
                cls += ' fpp-upd-dot--required';
            }
            // 'update' -> base (amber) dot
            $('#' + id).attr('class', cls).text(text);
        }

        // Keep the OS version status dot in sync with the detected OS upgrade
        // state so it doesn't contradict the banners/recommendation card. The
        // current-OS pill in the card header stays neutral (version text only).
        function updateOSVersionStatusBadge() {
            if (isMajorVersionUpgrade) {
                setVersionStatusDot('osVersionStatusBadge', 'required', 'Upgrade Required');
            } else if (osUpgradeAvailable) {
                setVersionStatusDot('osVersionStatusBadge', 'update', 'Upgrade Available');
            } else {
                setVersionStatusDot('osVersionStatusBadge', 'ok', 'Up to Date');
            }
        }

        // Single source of truth for which upgrade path is recommended, so the
        // status dot, subtitles, button styling, and card coordination can't
        // drift out of sync. Returns 'fpp' | 'os' | null.
        //   - Major version upgrades and any available OS upgrade -> 'os'
        //     (the OS image ships a fresh FPP, so an FPP update first is wasted).
        //   - FPP update available with OS current -> 'fpp'.
        function getRecommendedPath() {
            if (isMajorVersionUpgrade || osUpgradeAvailable) {
                return 'os';
            }
            if (fppUpdateAvailable) {
                return 'fpp';
            }
            return null;
        }

        // Coordinated semantic layout: highlight the recommended card in amber
        // end-to-end and fade the card that doesn't apply in the current state.
        //   recommended: 'fpp' | 'os' | null
        function setCoordinatedCards(recommended) {
            $('#fppCard')
                .toggleClass('is-recommended', recommended === 'fpp')
                .toggleClass('is-disabled', recommended !== 'fpp');
            $('#osCard')
                .toggleClass('is-recommended', recommended === 'os')
                .toggleClass('is-disabled', recommended !== 'os');
            // Re-derive the engaged (un-faded) state after (re)coordinating, so a
            // reset/repopulated dropdown can't leave the card un-faded with nothing
            // selected.
            syncOSCardEngaged();
        }

        // State-specific card subtitles, so each card describes what applies now
        // instead of a generic line (e.g. the OS card reads "OS is current" while
        // an FPP update is the recommended path).
        function updateCardSubtitles() {
            var $fpp = $('#fppCardSubtitle');
            if (isMajorVersionUpgrade) {
                $fpp.text('Cannot upgrade across major versions from here. Use the OS upgrade instead.');
            } else if (fppUpdateAvailable && osUpgradeAvailable) {
                $fpp.text('An FPP update is available, but the OS upgrade below already includes a fresh FPP build.');
            } else if (fppUpdateAvailable && branchUpgradeData && branchUpgradeData.branchUpgradeVersion) {
                $fpp.text('FPP ' + branchUpgradeData.branchUpgradeVersion + ' is available. Safe and quick (2-5 min).');
            } else if (fppUpdateAvailable) {
                $fpp.text('New commits are available on your current branch. Safe and quick.');
            } else if (isEndOfLife) {
                // Current major version is unsupported and no in-branch update is
                // offered -- don't claim "up to date"; the only path is an OS upgrade.
                $fpp.text('This version has reached End of Life. Upgrade the OS to a supported release.');
            } else {
                $fpp.text('FPP is up to date. No new commits or releases available.');
            }

            var $os = $('#osCardSubtitle');
            if (isMajorVersionUpgrade) {
                var majorTarget = (branchUpgradeData && branchUpgradeData.branchUpgradeVersion)
                    ? 'FPP ' + branchUpgradeData.branchUpgradeVersion : 'the new major version';
                $os.text('Required to install ' + majorTarget + '. Your settings and media are preserved.');
            } else if (fppUpdateAvailable && osUpgradeAvailable) {
                $os.text('A new OS image is available. Recommended -- it includes a fresh FPP build.');
            } else if (osUpgradeAvailable) {
                $os.text('A new OS image is available. Includes security patches and dependency updates.');
            } else if (isEndOfLife) {
                // EOL but no matching image is listed yet -- don't claim the OS is
                // "current" (that contradicts the End of Life call to upgrade).
                $os.text('An OS upgrade is required to reach a supported release, but no compatible image is listed for this board yet.');
            } else {
                $os.text('OS is current. No new image available.');
            }
        }

        // Keep the FPP/OS action buttons styled consistently with availability:
        //   updates available -> yellow (warning), no updates -> gray (secondary).
        function updateActionButtonStyles() {
            var $fpp = $('#fppUpdateButton');
            // Amber (recommended) only when the FPP update is the recommended path.
            // Major upgrades force the OS route, and when an OS upgrade is also
            // available we recommend that instead -- so the FPP button stays gray
            // to match its de-emphasized card in those cases.
            if (getRecommendedPath() === 'fpp') {
                $fpp.removeClass('fpp-btn--success fpp-btn--secondary').addClass('fpp-btn--warning');
            } else {
                $fpp.removeClass('fpp-btn--success fpp-btn--warning').addClass('fpp-btn--secondary');
            }

            var $os = $('#osUpgradeButton');
            if (osUpgradeAvailable) {
                $os.removeClass('fpp-btn--secondary fpp-btn--success').addClass('fpp-btn--warning');
            } else {
                $os.removeClass('fpp-btn--warning fpp-btn--success').addClass('fpp-btn--secondary');
            }
        }

        // Check upgrade scenarios, pick the recommended path, and coordinate the
        // banners + cards around it (amber flows from the recommendation banner
        // into the recommended card).
        function checkUpgradeRecommendation() {
            updateOSVersionStatusBadge();
            updateActionButtonStyles();

            var recommended = getRecommendedPath(); // 'fpp' | 'os' | null

            if (isMajorVersionUpgrade) {
                // Major version upgrades REQUIRE an OS upgrade. The OS banner is
                // already configured in UpdateVersionInfo(); recommend OS here.
                $('#upgradeRecommendationBanner').hide();
                $('#fppRecommendedBadge').hide();
                $('#osRecommendedBadge').show();
            } else if (fppUpdateAvailable && osUpgradeAvailable) {
                // Both available (non-major). The OS image ships with a fresh FPP,
                // so an FPP update first is wasted work -- recommend the OS upgrade.
                $('#upgradeRecommendationTitle').text('Recommended: Upgrade OS First');
                $('#upgradeRecommendationMessage').text(
                    'Both a software update and OS upgrade are available. We recommend the ' +
                    'OS upgrade -- it ships with a fresh FPP build, so a separate FPP update ' +
                    'beforehand is unnecessary. Always backup your configuration first!'
                );
                $('#osRecommendedBadge').show();
                $('#fppRecommendedBadge').hide();
                $('#upgradeRecommendationBanner').show();
                $('#osUpdateBanner').hide();
                $('#fppUpdateBanner').hide();
            } else if (osUpgradeAvailable) {
                // FPP is up to date, but an OS upgrade is available.
                $('#upgradeRecommendationBanner').hide();
                $('#fppRecommendedBadge').hide();
                $('#osRecommendedBadge').show();
                $('#osUpdateBanner')
                    .removeClass('fpp-banner--success')
                    .addClass('fpp-banner--warning')
                    .show();
                $('#osUpdateBanner .fpp-banner__title').text('Operating System Upgrade Available');
                $('#osUpdateBanner .fpp-banner__message').text(
                    'A newer OS version is available. OS upgrades include security patches, new hardware support, and system improvements. Always backup first!'
                );
                $('#osUpdateBanner .fpp-banner__icon i').removeClass('fa-arrow-circle-up').addClass('fa-exclamation-triangle');
            } else if (fppUpdateAvailable) {
                // FPP update available, OS current -- FPP update is the recommended path.
                $('#upgradeRecommendationBanner').hide();
                $('#osRecommendedBadge').hide();
                $('#fppRecommendedBadge').show();
                $('#osUpdateBanner').hide();
            } else {
                // Everything up to date.
                $('#upgradeRecommendationBanner').hide();
                $('#fppRecommendedBadge').hide();
                $('#osRecommendedBadge').hide();
                $('#osUpdateBanner').hide();
            }

            setCoordinatedCards(recommended);
            updateCardSubtitles();
            updateOSRebootWarning();
        }

        // Reboot warning: show it whenever an OS upgrade is the action at hand --
        // either an upgrade is detected, or the user has manually selected an image
        // (advanced "Show All Platforms"/"Show Legacy OS" flow enables the upgrade
        // button independently of osUpgradeAvailable).
        function updateOSRebootWarning() {
            var imageSelected = $('#osSelect').val() !== '' && $('#osSelect').val() != null;
            $('#osRebootWarning').toggle(!!osUpgradeAvailable || imageSelected);
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
            // Replace any still-shimmering async placeholders with a neutral "--"
            // so a failed/partial status response can't leave them animating forever.
            function clearVersionSkeletons() {
                $('#osVersionValue, #osReleaseValue, #kernelValue, #osCurrentVersionBadge').each(function () {
                    if ($(this).find('.fpp-skeleton').length) {
                        $(this).text('--');
                    }
                });
            }

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
                // These fields are populated only here; clear any that this
                // response didn't fill so they don't shimmer indefinitely.
                clearVersionSkeletons();
            }).fail(clearVersionSkeletons);

            // Fetch unified update status
            var updateStatusUrl = 'api/system/updateStatus';
            if (testMode) {
                updateStatusUrl += '?test=' + testMode;
            }
            $.get(updateStatusUrl, function (updateData) {
                if (updateData.status !== 'OK') return;

                // Test mode: force OS upgrade available regardless of which
                // FPP-update path we land in below.
                if (updateData.forceOsUpgradeAvailable) {
                    osUpgradeAvailable = true;
                    forceOsUpgradeTest = true;
                }

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
                // Reset the major-version callout; re-shown only on the major path below.
                $('#fppMajorCallout').hide();

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
                        $('#fppMajorCallout').show();
                        $('#fppUpdateBanner').hide();

                        // Show OS banner (amber = the recommended path)
                        $('#osUpdateBanner')
                            .removeClass('fpp-banner--success')
                            .addClass('fpp-banner--warning')
                            .show();
                        $('#osUpdateBanner .fpp-banner__title').text('FPP ' + updateData.branchUpgradeVersion + ' Available - OS Upgrade Required');
                        $('#osUpdateBanner .fpp-banner__message').html(
                            'A new major version of FPP is available! Major version upgrades require a fresh OS image. ' +
                            'Please <a href="backup.php">backup your configuration</a> first, then select the matching OS image below.'
                        );
                        $('#osUpdateBanner .fpp-banner__icon i').removeClass('fa-exclamation-triangle').addClass('fa-arrow-circle-up');

                        setVersionStatusDot('fppVersionStatusBadge', 'required', 'OS Upgrade Required');
                        // Signal that OS upgrade path should be used
                        osUpgradeAvailable = true;

                        // Standard view: show that OS upgrade is needed
                        $('#fppVersionStandardBranchUpgrade').show();
                        $('#fppTargetVersion').text('FPP ' + updateData.branchUpgradeVersion);
                        // Add visual indication that this requires OS upgrade
                        setVersionStatusDot('fppStandardBranchDot', 'required', 'Requires OS Upgrade');

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
                        $('#fppUpdateBanner').show();
                        setVersionStatusDot('fppVersionStatusBadge', 'update', 'Upgrade Available');

                        // Standard view: show branch upgrade
                        $('#fppVersionStandardBranchUpgrade').show();
                        $('#fppTargetVersion').text('FPP ' + updateData.branchUpgradeVersion);
                        // Reset the shared dot (major path sets it to "Requires OS Upgrade").
                        setVersionStatusDot('fppStandardBranchDot', 'update', 'Update available');

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
                    $('#fppUpdateBanner').show();
                    setVersionStatusDot('fppVersionStatusBadge', 'update', 'Update Available');

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

                    $('#fppUpdateBanner').hide();
                    // Don't hide OS banner here - let checkUpgradeRecommendation() handle it
                    // based on whether osUpgradeAvailable is set
                    setVersionStatusDot('fppVersionStatusBadge', 'ok', 'Up to Date');

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
                setVersionStatusDot('fppVersionStatusBadge', 'unknown', 'Unknown');
                // Don't leave placeholders shimmering forever if status can't be
                // fetched -- fall back to the neutral "--" for any unresolved field.
                clearVersionSkeletons();
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

                $.get(allPlatforms, function (data) {
                    var devMode = (settings['uiLevel'] && (parseInt(settings['uiLevel']) == 3));
                    var isAdvanced = (settings['uiLevel'] && (parseInt(settings['uiLevel']) >= 1));
                    var showLegacy = $('#LegacyOS').is(':checked');
                    // Regex to match versions below 9.0 (With N-1 - update this yearly)
                    var legacyVersionRegex = /[-_]v?[0-8]\./i;

                    if (showLegacy) {
                        $('#legacyOSWarning').show();
                    } else {
                        $('#legacyOSWarning').hide();
                    }

                    var options = [];

                    if ("files" in data) {
                        for (const file of data["files"]) {
                            osAssetMap[file["asset_id"]] = {
                                name: file["filename"],
                                url: file["url"]
                            };

                            var isLegacyVersion = legacyVersionRegex.test(file['filename']);
                            var isAllPlatforms = $('#allPlatforms').is(':checked');
                            if (isLegacyVersion && (isAllPlatforms || !showLegacy)) {
                                continue;
                            }

                            // Nightly builds are bleeding-edge -- Developer mode only.
                            // Other prereleases (alpha/beta) are fine in Advanced+.
                            // GitHub flags alpha/beta AND nightly as prerelease, so
                            // distinguish nightly by name rather than the flag alone.
                            var isNightlyBuild = /nightly/i.test(file["filename"]);
                            if (isNightlyBuild && !devMode) {
                                continue;
                            }
                            var isPrerelease = file["prerelease"] === true;
                            if (isPrerelease && !isNightlyBuild && !isAdvanced) {
                                continue;
                            }

                            var label = file["filename"];
                            if (!file["downloaded"]) {
                                label += " (download)";
                            }
                            options.push({
                                value: file["asset_id"],
                                text: label
                            });
                        }
                    }

                    //only show alpha and beta images in Advanced ui
                    if (!isAdvanced) {
                        options = options.filter(function (o) {
                            return !/beta/i.test(o.text) && !/alpha/i.test(o.text);
                        });
                    }

                    //insert files already downloaded if we haven't got them from the git api call
                    var osUpdateFiles = <?php echo json_encode($osUpdateFiles); ?>;
                    osUpdateFiles.forEach(element => {
                        var isDuplicate = options.some(function (o) {
                            return o.text.toLowerCase().indexOf(element.toLowerCase()) !== -1;
                        });
                        if (isDuplicate) return;
                        var isLegacyVersion = legacyVersionRegex.test(element);
                        var isAllPlatforms = $('#allPlatforms').is(':checked');
                        if (isLegacyVersion && (isAllPlatforms || !showLegacy)) {
                            return;
                        }
                        if (/nightly/i.test(element) && !devMode) {
                            return;
                        }
                        if (!matchesDeviceOSBuild(element)) {
                            return;
                        }
                        options.push({
                            value: element,
                            text: element
                        });
                    });

                    // Sort options using version-aware sorting
                    options.sort(function (a, b) {
                        return compareOSFilenames(a.text, b.text);
                    });

                    // Append sorted options
                    options.forEach(function (o) {
                        $('#osSelect').append($('<option>', o));
                    });

                    if (!forceOsUpgradeTest) {
                        osUpgradeAvailable = checkForNewerOS();
                    }
                    checkUpgradeRecommendation();
                    updateOSNoImages();
                }).fail(function () {
                    // API failed - still show any locally downloaded OS files
                    if (!forceOsUpgradeTest) {
                        osUpgradeAvailable = checkForNewerOS();
                    }
                    checkUpgradeRecommendation();
                    updateOSNoImages();
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
                var selectedText = $('#osSelect option:selected').text();
                var platformMatches = matchesDeviceOSBuild(selectedText);
                if (platformMatches) {
                    $('#osUpgradeButton').removeAttr('disabled');
                } else {
                    $('#osUpgradeButton').attr('disabled', 'disabled');
                }
                $('#osReleaseNotesButton').removeAttr('disabled');
                if (os in osAssetMap) {
                    $('#osDownloadButton').removeAttr('disabled');
                } else {
                    $('#osDownloadButton').attr('disabled', 'disabled');
                }
            }
            // Selecting an image enables the (rebooting) OS upgrade even when no
            // upgrade was auto-detected, so keep the reboot warning in sync.
            updateOSRebootWarning();

            syncOSCardEngaged();
        }

        // In the coordinated view the OS card may be faded back (is-disabled) when
        // it isn't the recommended path. Once the user actually picks an image, the
        // card is "engaged": it -- and its Upgrade button -- return to full opacity
        // and read as normal/clickable. Derived purely from the current selection
        // so a repopulated/reset dropdown can't leave the card falsely un-faded.
        function syncOSCardEngaged() {
            var os = $('#osSelect').val();
            $('#osCard').toggleClass('is-engaged', os !== '' && os != null);
        }

        // Show an explanatory note when the dropdown has no selectable image (only
        // the placeholder), so a blank select reads as "nothing available for this
        // platform yet" instead of looking broken.
        function updateOSNoImages() {
            var selectable = $('#osSelect option').filter(function () {
                return this.value !== '';
            }).length;
            $('#osNoImages').toggle(selectable === 0);
        }

        function initFaqAccordion() {
            var items = document.querySelectorAll('.fpp-faq__item');

            // Drive max-height from the answer's actual scrollHeight so open/close
            // animates to the real content height (no fixed-height clip).
            function syncFaqHeight(item) {
                var answer = item.querySelector('.fpp-faq__answer');
                if (!answer) return;
                answer.style.maxHeight = item.classList.contains('fpp-faq__item--open')
                    ? answer.scrollHeight + 'px'
                    : '0px';
            }

            items.forEach(function (item) {
                item.querySelector('.fpp-faq__question').addEventListener('click', function () {
                    var isOpen = item.classList.contains('fpp-faq__item--open');
                    items.forEach(function (i) { i.classList.remove('fpp-faq__item--open'); });
                    if (!isOpen) item.classList.add('fpp-faq__item--open');
                    items.forEach(syncFaqHeight);
                });
            });

            // Initialize (the first item is open by default in the markup).
            items.forEach(syncFaqHeight);

            // Re-measure open answers on resize: a narrower viewport reflows the
            // text taller, and a max-height frozen at the old scrollHeight would
            // clip it (overflow is hidden). Debounced to avoid thrashing.
            var faqResizeTimer = null;
            window.addEventListener('resize', function () {
                clearTimeout(faqResizeTimer);
                faqResizeTimer = setTimeout(function () {
                    items.forEach(syncFaqHeight);
                }, 100);
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
        $activeParentMenuItem = 'help';
        include 'menu.inc';
        ?>
        <div class="mainContainer">
            <h1 class="title">System Upgrade</h1>
            <div class="pageContent">

                <!-- Update Banners (conditionally shown) -->

                <!-- End of Life: slim severity strip so it does not compete with the
                     primary recommendation banner below. -->
                <div id="eolBanner" class="fpp-banner fpp-banner--danger fpp-banner--strip" style="display: none;">
                    <div class="fpp-banner__icon">
                        <i class="fas fa-exclamation-circle"></i>
                    </div>
                    <div class="fpp-banner__content">
                        <p class="fpp-banner__message">
                            <strong>End of Life</strong> &middot; FPP <span id="eolCurrentVersion"></span> no longer
                            receives bug fixes or security updates. Upgrade to FPP <span id="eolLatestVersion"></span>
                            via OS Upgrade to continue receiving support.
                        </p>
                    </div>
                </div>

                <!-- FPP software update available. Amber is the recommendation color in this
                     coordinated layout, matching the recommended card below. -->
                <div id="fppUpdateBanner" class="fpp-banner fpp-banner--warning" style="display: none;">
                    <div class="fpp-banner__icon">
                        <i class="fas fa-arrow-circle-up"></i>
                    </div>
                    <div class="fpp-banner__content">
                        <div class="fpp-banner__title">FPP Software Update Available</div>
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

                <!-- Upgrade Path Recommendation Banner (shown when both updates available).
                     Amber flows into the recommended card below. -->
                <div id="upgradeRecommendationBanner" class="fpp-banner fpp-banner--warning" style="display: none;">
                    <div class="fpp-banner__icon">
                        <i class="fas fa-lightbulb"></i>
                    </div>
                    <div class="fpp-banner__content">
                        <div class="fpp-banner__title" id="upgradeRecommendationTitle">Recommended: Upgrade OS First
                        </div>
                        <p class="fpp-banner__message" id="upgradeRecommendationMessage">
                            Both a software update and OS upgrade are available. We recommend the OS upgrade --
                            it ships with a fresh FPP build, so a separate FPP update beforehand is unnecessary.
                            Always backup your configuration first!
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
                                    <span id="fppVersionValue"><?= $fppVersion ?></span>
                                    <span id="fppVersionStatusBadge" class="fpp-upd-dot">Checking...</span>
                                </span>
                            </div>
                            <div class="fpp-row">
                                <span class="fpp-row__label">OS Version:</span>
                                <span class="fpp-row__value">
                                    <span id="osVersionValue"><span class="fpp-skeleton" aria-hidden="true"></span></span>
                                    <span id="osVersionStatusBadge" class="fpp-upd-dot fpp-upd-dot--ok"
                                        style="display: none;">Up to Date</span>
                                </span>
                            </div>

                        </div>

                        <div class="col-md-4 fpp-col-divider">
                            <div class="fpp-row">
                                <span class="fpp-row__label">OS Build:</span>
                                <span class="fpp-row__value" id="osReleaseValue"><span class="fpp-skeleton" aria-hidden="true"></span></span>
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
                                <span class="fpp-row__value" id="kernelValue"><span class="fpp-skeleton" aria-hidden="true"></span></span>
                            </div>
                        </div>
                    </div>
                </div>

                <!-- Upgrade Options -->
                <div class="row">
                    <!-- FPP Software Update -->
                    <div class="col-xl-6 fpp-col-flex">
                        <div id="fppCard" class="card fpp-card fpp-card--accent fpp-card--accent-neutral fpp-card--flex">
                            <div class="fpp-card__header">
                                <div class="fpp-card__icon fpp-card__icon--neutral">
                                    <i class="fas fa-sync-alt"></i>
                                </div>
                                <div>
                                    <h3 class="fpp-card__title">
                                        Update FPP Software
                                        <span id="fppRecommendedBadge" class="fpp-tag fpp-tag--recommended"
                                            style="display: none;">Recommended</span>
                                    </h3>
                                    <p class="fpp-card__subtitle" id="fppCardSubtitle">Get the latest bug fixes and
                                        features. This is safe and quick.</p>
                                </div>
                            </div>

                            <!-- Shown only for major-version upgrades, which cannot be applied here -->
                            <div id="fppMajorCallout" class="fpp-major-callout" style="display: none;">
                                <i class="fas fa-info-circle"></i>
                                <span>Major version upgrades cannot be installed via FPP update. Use the
                                    <strong>OS upgrade</strong> instead.</span>
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
                                    <span id="fppStandardBranchDot" class="fpp-upd-dot">Update available</span>
                                    <span
                                        class="fpp-version-indicator__label fpp-version-indicator__label--subtle">Click
                                        to see release notes</span>
                                </div>

                                <!-- Standard: Commit updates available (same version, new commits) -->
                                <div id="fppVersionStandardCommitUpdate" class="fpp-version-indicator"
                                    style="display: none;">
                                    <span class="fpp-version-indicator__current"><?= $fppVersionDisplay ?></span>
                                    <span class="fpp-upd-dot">Update available</span>
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
                                <button class="fpp-btn fpp-btn--secondary" id="fppUpdateButton"
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
                                        <span class="fpp-tag fpp-tag--adv">Adv</span>
                                        <span>Source:</span>
                                        <?php PrintSettingSelect("FPP Upgrade Source", "UpgradeSource", 0, 0, "github.com", $upgradeSources); ?>
                                    </div>
                                <?php } ?>
                            </div>
                        </div>
                    </div>

                    <!-- Operating System Upgrade -->
                    <div class="col-xl-6 fpp-col-flex">
                        <div id="osCard" class="card fpp-card fpp-card--accent fpp-card--accent-neutral fpp-card--flex">
                            <div class="fpp-card__header">
                                <div class="fpp-card__icon fpp-card__icon--neutral">
                                    <i class="fas fa-hdd"></i>
                                </div>
                                <div>
                                    <h3 class="fpp-card__title">
                                        Upgrade Operating System
                                        <span id="osRecommendedBadge" class="fpp-tag fpp-tag--recommended"
                                            style="display: none;">Recommended</span>
                                    </h3>
                                    <p class="fpp-card__subtitle" id="osCardSubtitle">Upgrade the entire FPP operating
                                        system with a new version.</p>
                                </div>
                                <div class="fpp-card__header-status">
                                    <span class="fpp-card__header-status-label">Current OS</span>
                                    <span class="fpp-tag" id="osCurrentVersionBadge"><span class="fpp-skeleton" aria-hidden="true"></span></span>
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
                                </div>
                                <div class="fpp-info-box fpp-info-box--info">
                                    <div class="fpp-info-box__title"><i class="fas fa-info-circle"></i> What it does
                                    </div>
                                    <p>Downloads a complete OS image and updates your current OS. Your media files are
                                        preserved, but backing up your configuration is strongly recommended.</p>
                                    <span class="fpp-note"><strong>Important:</strong> This takes
                                        15-30+ minutes and requires a reboot. <a href="backup.php">Backup
                                            first!</a></span>
                                    <span class="fpp-note"><strong>Architecture Note:</strong>
                                        Switching between 32-bit and 64-bit is not supported from this screen. To
                                        change architectures, flash a fresh image.</span>
                                </div>
                            </div>
                            <?php if ($advancedInfoCollapse) { ?></details><?php } ?>

                            <!-- Reboot warning: shown only when an OS upgrade is the relevant action -->
                            <div id="osRebootWarning" class="fpp-inline-warn" style="display: none;">
                                <i class="fas fa-exclamation-triangle"></i>
                                <span><strong>Warning:</strong> OS upgrade will reboot your system. Ensure no shows are
                                    running.</span>
                            </div>

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
                                <button class="fpp-btn fpp-btn--secondary" id="osUpgradeButton" onclick="UpgradeOS();"
                                    disabled>
                                    <i class="fas fa-arrow-up"></i> Upgrade OS
                                </button>
                                <a class="fpp-btn fpp-btn--outline" href="backup.php">
                                    <i class="fas fa-shield-alt"></i> Backup First
                                </a>
                                <button class="fpp-btn fpp-btn--secondary" id="osDownloadButton" onclick="DownloadOS();"
                                    disabled>
                                    <i class="fas fa-cloud-download-alt"></i> Download Only
                                </button>
                                <button class="fpp-btn fpp-btn--outline" id="osReleaseNotesButton"
                                    onclick="ViewOSReleaseNotes();" disabled>
                                    <i class="fas fa-file-alt"></i> Release Notes
                                </button>
                            </div>

                            <!-- Empty state: shown when no OS image matches this platform, so a
                                 blank dropdown reads as an explanation instead of a dead end. -->
                            <div id="osNoImages"
                                class="fpp-alert fpp-alert--info fpp-alert--compact fpp-alert--mb-md"
                                style="display: none;">
                                <i class="fas fa-info-circle"></i>
                                <span>No compatible OS image was found for this platform right now. New
                                    images appear here once a release is available for your board.</span>
                            </div>

                            <?php if (isset($settings['uiLevel']) && $settings['uiLevel'] >= 1) { ?>
                                <div class="fpp-checkbox-options">
                                    <label class="fpp-checkbox-option">
                                        <input type="checkbox" id="allPlatforms" onChange="PopulateOSSelect();">
                                        <span class="fpp-tag fpp-tag--adv">Adv</span>
                                        Show All Platforms
                                        <img title='Show both BBB & Pi downloads' src='images/redesign/help-icon.svg'
                                            class='icon-help'>
                                    </label>
                                    <label class="fpp-checkbox-option">
                                        <input type="checkbox" id="LegacyOS" onChange="PopulateOSSelect();">
                                        <span class="fpp-tag fpp-tag--adv">Adv</span>
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
                        <div class="fpp-card__icon fpp-card__icon--neutral">
                            <i class="fas fa-history"></i>
                        </div>
                        <div class="fpp-card__content">
                            <h3 class="fpp-card__title">
                                Revert to Previous Commit
                                <span class="fpp-tag">Advanced</span>
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
                            <div class="fpp-faq__item fpp-faq__item--open">
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
