<!DOCTYPE html>
<html lang="en">

<head>
    <?php
    include 'common/htmlMeta.inc';
    require_once('config.php');
    require_once('common.php');

    writeFPPVersionJavascriptFunctions();

    include 'common/menuHead.inc';
    ?>
    <script>
        var installedPlugins = [];
        var pluginInfos = [];
        var pluginInfoURLs = [];
        var pluginInfoUseCredentials = {};

        function PluginIsInstalled(plugin) {
            for (var i = 0; i < installedPlugins.length; i++) {
                if (installedPlugins[i] == plugin)
                    return 1;
            }

            return 0;
        }

        function GetInstalledPlugins() {
            var url = 'api/plugin';
            $.ajax({
                url: url,
                dataType: 'json',
                success: function (data) {
                    installedPlugins = data;
                    LoadInstalledPlugins();
                    GetPluginList();
                },
                error: function () {
                    GetPluginList();
                    alert('Error, failed to get list of installed plugins.');
                }
            });
        }

        function GetPluginList() {
            var url = 'https://raw.githubusercontent.com/FalconChristmas/fpp-data/master/pluginList.json';
            $.ajax({
                url: url,
                dataType: 'json',
                success: function (data) {
                    LoadPlugins(data.pluginList);
                    // Deep-link from the post-FPPOS-upgrade warning's Fix button
                    // (plugins.php?action=reinstallAll): now that installedPlugins
                    // and pluginInfos are loaded, pop the Reinstall All confirm.
                    MaybeAutoOpenReinstallAll();
                },
                error: function () {
                    alert('Error, failed to get pluginList.json');
                }
            });
        }

        function CheckPluginForUpdates(plugin) {
            var url = 'api/plugin/' + plugin + '/updates';

            $('html,body').css('cursor', 'wait');
            $.ajax({
                url: url,
                type: 'POST',
                dataType: 'json',
                success: function (data) {
                    $('html,body').css('cursor', 'auto');
                    if (data.Status == 'OK') {
                        if (data.updatesAvailable)
                            $('#row-' + plugin).find('.updatesAvailable').show();
                        else
                            $.jGrowl('No updates available for ' + plugin, { themeState: 'detract' });
                    }
                    else
                        alert('ERROR: ' + data.Message);
                },
                error: function () {
                    $('html,body').css('cursor', 'auto');
                    alert('Error, API call failed when checking plugin for updates');
                }
            });
        }

        function CheckAllPluginsForUpdates() {
            if (installedPlugins.length === 0) {
                $.jGrowl('No plugins installed', { themeState: 'detract' });
                return;
            }

            $('html,body').css('cursor', 'wait');
            $('#checkAllUpdatesBtn').prop('disabled', true);

            var checked = 0;
            var total = installedPlugins.length;
            var updatesFound = 0;

            installedPlugins.forEach(function (plugin) {
                var url = 'api/plugin/' + plugin + '/updates';

                $.ajax({
                    url: url,
                    type: 'POST',
                    dataType: 'json',
                    success: function (data) {
                        checked++;
                        if (data.Status == 'OK' && data.updatesAvailable) {
                            $('#row-' + plugin).find('.updatesAvailable').show();
                            updatesFound++;
                        }

                        if (checked === total) {
                            $('html,body').css('cursor', 'auto');
                            $('#checkAllUpdatesBtn').prop('disabled', false);
                            if (updatesFound > 0) {
                                $.jGrowl('Found updates for ' + updatesFound + ' plugin(s)', { themeState: 'success' });
                            } else {
                                $.jGrowl('All plugins are up to date', { themeState: 'success' });
                            }
                        }
                    },
                    error: function () {
                        checked++;
                        if (checked === total) {
                            $('html,body').css('cursor', 'auto');
                            $('#checkAllUpdatesBtn').prop('disabled', false);
                            $.jGrowl('Completed checking plugins (some checks failed)', { themeState: 'warn' });
                        }
                    }
                });
            });
        }

        function UpgradePlugin(plugin) {
            var url = 'api/plugin/' + plugin + '/upgrade?stream=true';
            DisplayProgressDialog("pluginsProgressPopup", "Upgrade Plugin");
            StreamURL(url, 'pluginsProgressPopupText', 'ProgressDialogDone', 'ProgressDialogDone');
        }

        function InstallPlugin(plugin, branch, sha) {
            var url = 'api/plugin?stream=true';
            var i = FindPluginInfo(plugin);

            if (i < -1) {
                alert('Could not find plugin ' + plugin + ' in pluginInfo cache.');
                return;
            }

            var pluginInfo = pluginInfos[i];
            pluginInfo['branch'] = branch;
            pluginInfo['sha'] = sha;
            pluginInfo['infoURL'] = pluginInfoURLs[plugin];
            // Automatically use the configured GitHub credentials for plugins
            // whose pluginInfo.json is flagged as private, or which were
            // manually loaded via the credentialed proxy.
            pluginInfo['useCredentials'] = (pluginInfo.private || pluginInfoUseCredentials[plugin]) ? 1 : 0;

            var postData = JSON.stringify(pluginInfo);
            DisplayProgressDialog("pluginsProgressPopup", "Install Plugin");
            StreamURL(url, 'pluginsProgressPopupText', 'ProgressDialogDone', 'ProgressDialogDone', 'POST', postData, 'application/json');
        }

        function UninstallPlugin(plugin) {
            var url = 'api/plugin/' + plugin + '?stream=true'; // Assuming your API supports streaming for uninstall
            DisplayProgressDialog("pluginsProgressPopup", "Uninstall Plugin");
            StreamURL(url, 'pluginsProgressPopupText', 'ProgressDialogDone', 'ProgressDialogDone', 'DELETE');
        }

        function ShowUninstallPluginPopup(plugin, pluginName) {
            DoModalDialog({
                id: "uninstallPluginDialog",
                class: "modal-lg",
                title: "Warning: Uninstalling Plugin",
                body: "Please confirm you wish to uninstall the " + pluginName + " plugin",
                backdrop: true,
                keyboard: true,
                buttons: {
                    Uninstall: function () {
                        UninstallPlugin(plugin);
                    },
                    Abort: function () {
                        CloseModalDialog("uninstallPluginDialog");
                    }
                }
            });
        }

        var uninstallAllQueue = [];
        function UninstallAllPlugins() {
            uninstallAllQueue = installedPlugins.slice();
            if (uninstallAllQueue.length === 0) {
                $.jGrowl('No plugins installed', { themeState: 'detract' });
                return;
            }
            DisplayProgressDialog("pluginsProgressPopup", "Uninstall All Plugins");
            UninstallNextPlugin();
        }

        function UninstallNextPlugin() {
            if (uninstallAllQueue.length === 0) {
                ProgressDialogDone('pluginsProgressPopupText');
                return;
            }
            var plugin = uninstallAllQueue.shift();
            // The progress popup body is a <textarea>, which cannot render HTML, so
            // status must be plain text appended to .value (matching StreamURL).
            var outputArea = document.getElementById('pluginsProgressPopupText');
            if (outputArea) {
                outputArea.value += '\n===== Uninstalling ' + plugin + ' =====\n';
                outputArea.scrollTop = outputArea.scrollHeight;
            }
            var url = 'api/plugin/' + plugin + '?stream=true';
            // Chain to the next plugin whether this one succeeds or fails so a
            // single failure does not stop the rest of the batch.
            StreamURL(url, 'pluginsProgressPopupText', 'UninstallNextPlugin', 'UninstallNextPlugin', 'DELETE');
        }

        function ShowUninstallAllPluginsPopup() {
            if (installedPlugins.length === 0) {
                $.jGrowl('No plugins installed', { themeState: 'detract' });
                return;
            }
            DoModalDialog({
                id: "uninstallAllPluginsDialog",
                class: "modal-lg",
                title: "Warning: Uninstalling All Plugins",
                body: "Please confirm you wish to uninstall all " + installedPlugins.length + " installed plugin(s). This cannot be undone.",
                backdrop: true,
                keyboard: true,
                buttons: {
                    "Uninstall All": function () {
                        CloseModalDialog("uninstallAllPluginsDialog");
                        UninstallAllPlugins();
                    },
                    Abort: function () {
                        CloseModalDialog("uninstallAllPluginsDialog");
                    }
                }
            });
        }

        // Reinstall All: uninstall every installed plugin, then reinstall each one
        // by one. Runs entirely client-side against the per-plugin API endpoints,
        // mirroring the Uninstall All queue pattern above but in two phases.
        var reinstallUninstallQueue = [];
        var reinstallInstallQueue = [];
        var reinstallAttempted = [];   // repo names we intend to reinstall
        var reinstallSkipped = [];     // installed plugins with no cached info
        var reinstallTotal = 0;
        var reinstallUninstallDone = 0;
        var reinstallInstallDone = 0;
        // The progress popup body is a <textarea> (see DisplayProgressDialog), which
        // cannot render HTML, so all of our own status lines must be plain text
        // appended to .value -- matching how StreamURL writes the streamed command
        // output. Auto-scroll to keep the latest line visible.
        function ReinstallLog(text) {
            var outputArea = document.getElementById('pluginsProgressPopupText');
            if (!outputArea)
                return;
            outputArea.value += text;
            outputArea.scrollTop = outputArea.scrollHeight;
        }
        function ReinstallAllPlugins() {
            if (installedPlugins.length === 0) {
                $.jGrowl('No plugins installed', { themeState: 'detract' });
                return;
            }

            // Phase 0: capture the install POST body for every plugin BEFORE
            // removing anything, since uninstalling drops entries from
            // installedPlugins / the DOM. Only plugins we can rebuild an install
            // body for are queued; anything without cached info is left untouched
            // and reported as skipped rather than uninstalled-without-reinstall.
            reinstallInstallQueue = [];
            reinstallAttempted = [];
            reinstallSkipped = [];
            reinstallUninstallDone = 0;
            reinstallInstallDone = 0;
            installedPlugins.forEach(function (repo) {
                var i = FindPluginInfo(repo);
                if (i < 0) {
                    reinstallSkipped.push(repo); // no cached info -> can't rebuild the install body
                    return;
                }
                var info = JSON.parse(JSON.stringify(pluginInfos[i])); // copy, don't mutate cache
                var sel = SelectPluginVersionIndices(info);
                var idx = sel.compatible >= 0 ? sel.compatible : (sel.untested >= 0 ? sel.untested : 0);
                var v = info.versions[idx];
                info['branch'] = (v.branch && v.branch !== '') ? v.branch : 'master';
                info['sha'] = v.sha || '';
                if (pluginInfoURLs[repo])
                    info['infoURL'] = pluginInfoURLs[repo]; // else backend uses the repo's own pluginInfo.json
                info['useCredentials'] = (info.private || pluginInfoUseCredentials[repo]) ? 1 : 0;
                reinstallInstallQueue.push(info);
                reinstallAttempted.push(repo);
            });

            if (reinstallAttempted.length === 0) {
                $.jGrowl('No reinstallable plugins found (plugin info unavailable)', { themeState: 'detract' });
                return;
            }

            reinstallUninstallQueue = reinstallAttempted.slice();
            reinstallTotal = reinstallAttempted.length;
            DisplayProgressDialog("pluginsProgressPopup", "Reinstall All Plugins");
            if (reinstallSkipped.length) {
                ReinstallLog('\nSkipping ' + reinstallSkipped.length +
                    ' plugin(s) with no available plugin info (left installed): ' +
                    reinstallSkipped.join(', ') + '\n');
            }
            ReinstallUninstallNext();
        }

        function ReinstallUninstallNext() {
            if (reinstallUninstallQueue.length === 0) {
                ReinstallInstallNext(); // move on to the reinstall phase
                return;
            }
            var plugin = reinstallUninstallQueue.shift();
            reinstallUninstallDone++;
            SetProgressDialogStatus('pluginsProgressPopup',
                'Reinstall All — uninstalling ' + reinstallUninstallDone + ' of ' + reinstallTotal);
            ReinstallLog('\n===== Uninstalling ' + plugin + ' (' +
                reinstallUninstallDone + ' of ' + reinstallTotal + ') =====\n');
            var url = 'api/plugin/' + plugin + '?stream=true';
            // Continue on both success and failure so one failure does not strand
            // the batch.
            StreamURL(url, 'pluginsProgressPopupText', 'ReinstallUninstallNext', 'ReinstallUninstallNext', 'DELETE');
        }

        function ReinstallInstallNext() {
            if (reinstallInstallQueue.length === 0) {
                ReinstallFinish();
                return;
            }
            reinstallInstallDone++;
            var info = reinstallInstallQueue.shift();
            SetProgressDialogStatus('pluginsProgressPopup',
                'Reinstall All — installing ' + reinstallInstallDone + ' of ' + reinstallTotal);
            ReinstallLog('\n===== Installing ' + info.repoName + ' (' +
                reinstallInstallDone + ' of ' + reinstallTotal + ') =====\n');
            var postData = JSON.stringify(info);
            StreamURL('api/plugin?stream=true', 'pluginsProgressPopupText', 'ReinstallInstallNext', 'ReinstallInstallNext', 'POST', postData, 'application/json');
        }

        // After the reinstall phase, verify what actually ended up installed by
        // re-querying the authoritative list rather than trying to parse streamed
        // output (the install endpoint streams "Done" even on a logical failure).
        // Any plugin we attempted but that is now missing is reported as failed.
        function ReinstallFinish() {
            $.ajax({
                url: 'api/plugin',
                dataType: 'json',
                success: function (data) {
                    installedPlugins = data;
                    var failed = reinstallAttempted.filter(function (r) { return data.indexOf(r) < 0; });
                    var ok = reinstallAttempted.length - failed.length;
                    // A clean reinstall clears the post-FPPOS-upgrade flag; fppd
                    // is watching the settings file and will drop the associated
                    // "plugins must be reinstalled" warning. Leave it set if any
                    // plugin failed so the prompt persists for a retry.
                    if (failed.length === 0) {
                        SetSetting('pluginReinstallNeededAfterOS', '', 0, 0, true);
                    }
                    SetProgressDialogStatus('pluginsProgressPopup',
                        failed.length ? ('Reinstall All — ' + failed.length + ' failed, ' + ok + ' of ' + reinstallAttempted.length + ' ok')
                                      : ('Reinstall All — complete (' + ok + ' of ' + reinstallAttempted.length + ')'));
                    ReinstallLog('\n===== Reinstall complete: ' + ok + ' of ' +
                        reinstallAttempted.length + ' plugin(s) reinstalled successfully =====\n');
                    if (failed.length) {
                        ReinstallLog('Failed to reinstall ' + failed.length +
                            ' plugin(s): ' + failed.join(', ') + '\n');
                    }
                    if (reinstallSkipped.length) {
                        ReinstallLog('Skipped (no plugin info, left installed): ' +
                            reinstallSkipped.join(', ') + '\n');
                    }
                    ReinstallLog('Reload the page to refresh the plugin list.\n');
                    if (failed.length)
                        $.jGrowl(failed.length + ' plugin(s) failed to reinstall', { themeState: 'danger' });
                    else
                        $.jGrowl('All ' + ok + ' plugin(s) reinstalled successfully', { themeState: 'success' });
                    ProgressDialogDone('pluginsProgressPopupText');
                },
                error: function () {
                    ReinstallLog('\nReinstall finished, but could not verify the installed plugin list. Reload the page to check.\n');
                    ProgressDialogDone('pluginsProgressPopupText');
                }
            });
        }

        function ShowReinstallAllPluginsPopup() {
            if (installedPlugins.length === 0) {
                $.jGrowl('No plugins installed', { themeState: 'detract' });
                return;
            }
            DoModalDialog({
                id: "reinstallAllPluginsDialog",
                class: "modal-lg",
                title: "Warning: Reinstalling All Plugins",
                body: "This will uninstall and then reinstall all " + installedPlugins.length + " installed plugin(s), one at a time. This may take a while.",
                backdrop: true,
                keyboard: true,
                buttons: {
                    "Reinstall All": function () {
                        CloseModalDialog("reinstallAllPluginsDialog");
                        ReinstallAllPlugins();
                    },
                    Abort: function () {
                        CloseModalDialog("reinstallAllPluginsDialog");
                    }
                }
            });
        }

        // The post-FPPOS-upgrade warning's Fix button links here with
        // ?action=reinstallAll; pop the Reinstall All confirmation automatically.
        // Gated on the pluginReinstallNeededAfterOS setting (still set only while a
        // reinstall is actually needed): a successful Reinstall All clears it, so
        // the location.reload() the progress dialog performs on close will NOT
        // re-open the confirmation. Also guarded to at most once per page load.
        var autoReinstallHandled = false;
        function MaybeAutoOpenReinstallAll() {
            if (autoReinstallHandled)
                return;
            var params = new URLSearchParams(window.location.search);
            if (params.get('action') === 'reinstallAll' && settings['pluginReinstallNeededAfterOS']) {
                autoReinstallHandled = true;
                ShowReinstallAllPluginsPopup();
            }
        }

        function FindPluginInfo(plugin) {
            for (var i = 0; i < pluginInfos.length; i++) {
                if (pluginInfos[i].repoName == plugin)
                    return i;
            }

            return -1;
        }

        // Determine which version entry applies to this FPP version/platform.
        // Returns { compatible, untested } indices (or -1). Shared by LoadPlugin
        // (rendering) and ReinstallAllPlugins (picking a version to reinstall) so
        // the selection logic lives in one place. NOTE: this mutates a version's
        // maxFPPVersion to flag untested entries for the compatibility display,
        // matching the original inline behaviour in LoadPlugin.
        function SelectPluginVersionIndices(data) {
            var compatibleVersion = -1;
            var untestedVersion = -1;
            for (var i = 0; i < data.versions.length; i++) {
                if ((data.versions[i].maxFPPVersion == "0") || (data.versions[i].maxFPPVersion == "0.0") || (data.versions[i].maxFPPVersion == "")) {
                    // maxVersion is the .999 of the min version Major version due to symantic versioning
                    var nv = data.versions[i].minFPPVersion;
                    nv = nv.split('.')[0];
                    if (nv != getFPPMajorVersion()) {
                        nv = (getFPPMajorVersion() - 1) + ".999";
                        data.versions[i].maxFPPVersion = nv;
                        untestedVersion = i;
                    }
                }

                if ((CompareFPPVersions(data.versions[i].minFPPVersion, getFPPVersionTriplet()) <= 0) &&
                    ((data.versions[i].maxFPPVersion == "0") || (data.versions[i].maxFPPVersion == "0.0") ||
                        (CompareFPPVersions(data.versions[i].maxFPPVersion, getFPPVersionTriplet()) > 0)) &&
                    ((!data.versions[i].hasOwnProperty('platforms')) ||
                        (data.versions[i].platforms.includes(settings['Platform'])))) {
                    compatibleVersion = i;
                }
            }
            return { compatible: compatibleVersion, untested: untestedVersion };
        }

        function InsertPluginTableItem(tableName, key, html) {
            var i = 0;
            var strcmp = new Intl.Collator(undefined, { numeric: true, sensitivity: 'base' }).compare;
            $('#' + tableName).children('div').each(function (item) {
                if ((i > 0) && (i < 9999)) {
                    var title = $(this).find('.pluginTitle').html();
                    if (title && strcmp(title, key) >= 0) {
                        $(html).insertBefore(this);
                        i = 9999;
                    }
                }
                i++;
            });
            if (i < 9999) {
                $('#' + tableName).append(html);
            }
        }

        var firstInstalled = 1;
        var firstCompatible = 1;
        var firstUntested = 1;
        var firstIncompatible = 1;
        function LoadPlugin(data, insert = false) {
            var html = '';
            var infoURL = pluginInfoURLs[data.repoName];

            if ($('#row-' + data.repoName).length) {
                // Delete the original entry so we can re-add with the latest info
                $('#row-' + data.repoName).next('.row').remove();
                if ($('#row-' + data.repoName).next('.row').html() == '<div class="col"><hr></div>')
                    $('#row-' + data.repoName).next('.row').remove();
                else
                    $('#row-' + data.repoName).prev('.row').remove();

                $('#row-' + data.repoName).remove();
            } else {
                pluginInfos.push(data);
            }

            var installed = PluginIsInstalled(data.repoName);
            var versionSel = SelectPluginVersionIndices(data);
            var compatibleVersion = versionSel.compatible;
            var untestedVersion = versionSel.untested;
            var compatibleVersionClass = (compatibleVersion == -1) ? " has-previous-compatible-version" : '';
            html += '<div id="row-' + data.repoName + '" class="fppPluginEntry' + compatibleVersionClass + '"><div class="backdrop fppPluginEntryBackdrop"><div class="row">';
            var avatarHtml = '';
            if (data.icon && data.icon !== '') {
                avatarHtml = '<img class="fppPluginAvatar fppPluginAvatarImg" src="' + data.icon + '" alt="" loading="lazy" onerror="this.style.display=\'none\'">';
            } else {
                avatarHtml = '<div class="fppPluginAvatar fppPluginAvatarInitial" style="background-color: ' + PluginColor(data.repoName) + '">' + PluginInitial(data.name) + '</div>';
            }
            html += '<div class="col-lg-3"><div class="d-flex align-items-center">' + avatarHtml + '<h3 class="pluginTitle">' + data.name + '</h3></div>';

            if (installed) {
                html += '<div class="text-success fppPluginEntryInstallStatus"><i class="far fa-check-circle"></i> <b>Installed</b></div>';
            }

            if (data.private || pluginInfoUseCredentials[data.repoName]) {
                html += '<div class="text-warning fppPluginEntryPrivateStatus" title="This plugin is hosted in a private GitHub repository. The GitHub user name and Personal Access Token configured on the Developer settings page will be used to clone it."><i class="fas fa-lock"></i> <b>Private</b></div>';
            }

            html += '</div>';
            html += '<div class="col-lg-2"><div class="labelHeading text-secondary">Author:</div><div class="text-primary pluginAuthor">' + data.author + '</div></div>';
            html += '<div class="col-lg"><div class="labelHeading text-secondary">Description:</div><div class="text-primary pluginDescription">' + data.description + '</div>';

            html += '</div>';
            html += '<div class="col-lg-auto fppPluginEntryActions">';

            html += '<div align="right">';

            if (installed) {
                // Determine the effective allowUpdates flag. A matching version
                // entry's allowUpdates overrides the top-level value; when neither
                // is set, updates are allowed by default. This lets a plugin freeze
                // an old FPP major (pinned sha, allowUpdates: 0 on that entry) while
                // keeping updates enabled on the current-major entry.
                var allowUpdates = true;
                if (data.hasOwnProperty('allowUpdates'))
                    allowUpdates = data.allowUpdates ? true : false;
                if ((compatibleVersion >= 0) && data.versions[compatibleVersion].hasOwnProperty('allowUpdates'))
                    allowUpdates = data.versions[compatibleVersion].allowUpdates ? true : false;

                if (allowUpdates) {
                    html += "<div class='pendingSpan updatesAvailable'";
                    if (!data.updatesAvailable)
                        html += " style='display: none;'";

                    html += "><div class='updateTable text-success fppPluginEntryUpdateStatus'><i class='fas fa-exclamation-circle'></i> <b>Updates Available</b></div>";
                    html += "<button class='buttons btn-success' onClick='UpgradePlugin(\"" + data.repoName + "\");'><i class='far fa-arrow-alt-circle-down'></i> Update Now</button>";

                    html += '</div>';
                    html += '</div><div align="right">';

                    html += "<button class='buttons btn-outline-success' onClick='CheckPluginForUpdates(\"" + data.repoName + "\");'><i class='fas fa-sync-alt'></i> Check for Updates</button>";

                } else {
                    html += '</div><div align="right">';
                }

                html += '</div><div align="right">';
                html += "<button class='buttons btn-outline-danger'  onClick='ShowUninstallPluginPopup(\"" + data.repoName + "\",\"" + data.name + "\");'><i class='fas fa-trash-alt'></i> Uninstall</button>";
            } else {
                html += '</div><div align="right">';
                html += '</div><div align="right">';
                if (compatibleVersion >= 0 || untestedVersion >= 0) {
                    let idx = compatibleVersion < 0 ? untestedVersion : compatibleVersion;

                    let installText = "Install";
                    let btnClass = "btn-success";

                    if (compatibleVersion < 0 && untestedVersion >= 0) {
                        installText = "Install untested plugin at your own risk";
                        btnClass = "btn-warning";
                    }

                    html += "<button class='buttons " + btnClass + "' onClick=' InstallPlugin(\"" + data.repoName + "\", \"" + data.versions[idx].branch + "\", \"" + data.versions[idx].sha + "\");'><i class='far fa-arrow-alt-circle-down'></i> " + installText + "</button>";
                }
            }

            html += '</div>';

            html += '</div></div>';
            html += '<div class="row fppPluginEntryFooter"><div class="col-lg"><a href="' + data.homeURL + '" target="_blank" rel="noopener noreferrer"><i class="fas fa-home"></i> ' + data.homeURL + '</a></div>';
            html += '<div class="col-lg-auto"><a href="' + data.srcURL + '" target="_blank" rel="noopener noreferrer"><i class="fas fa-code"></i> View Source</a>';
            html += ' <a href="' + data.bugURL + '" target="_blank" rel="noopener noreferrer" class="ps-2"><i class="fas fa-bug"></i> Report a Bug</a>';
            html += '</div>';
            html += '</div>';
            html += '</div>';

            if (compatibleVersion == -1) {
                html += '<div class="text-muted fppPluginEntryCompatibilityStatus">';
                html += '<i class="fas fa-info-circle"></i> Plugin has compatible versions for FPP Versions: <b>';
                for (var i = 0; i < data.versions.length; i++) {
                    if (i > 0)
                        html += ',';

                    if ((data.versions[i].minFPPVersion > 0) &&
                        (data.versions[i].maxFPPVersion > 0)) {
                        html += ' v' + data.versions[i].minFPPVersion + ' - v' + data.versions[i].maxFPPVersion;
                    } else if (data.versions[i].minFPPVersion > 0) {
                        html += ' > v' + data.versions[i].minFPPVersion;
                    } else if (data.versions[i].maxFPPVersion > 0) {
                        html += ' < v' + data.versions[i].maxFPPVersion;
                    }

                    if (data.versions[i].hasOwnProperty('platforms')) {
                        var platforms = data.versions[i].platforms;
                        html += " ";
                        for (var p = 0; p < platforms.length; p++) {
                            if (p != 0)
                                html += "/";
                            if (platforms[p] == 'Raspberry Pi') {
                                html += "Pi";
                            } else if (platforms[p] == 'BeagleBone Black') {
                                html += "BBB";
                            } else if (platforms[p] == 'BeagleBone 64') {
                                html += "BB64";
                            } else {
                                html += platforms[p];
                            }
                        }
                    }
                }
                html += '</b></div>';
                if (installed) {
                    html += '<div class="row"><div class="col" class="bad">WARNING: This plugin is already installed, but may be incompatible with this FPP version or platform.</div></div>';
                }
                html += '</div>';
            }

            if (installed) {
                $('#installedPlugins').show();
                if (firstInstalled) {
                    firstInstalled = 0;
                }

                InsertPluginTableItem('installedPlugins', data.name, html);
            } else if (data.repoName == 'fpp-plugin-Template') {
                if (settings["uiLevel"] > 2) {
                    $('#templatePlugin').show();
                    $('#templatePlugin').append(html);
                }
            } else if (compatibleVersion != -1) {
                if (firstCompatible) {
                    firstCompatible = 0;
                }

                if (insert) {
                    $('#pluginTable').children(':first-child').after(html);
                    document.getElementById("pluginTable").scrollIntoView();
                } else {
                    InsertPluginTableItem('pluginTable', data.name, html);
                }
            } else if (untestedVersion >= 0) {
                if (firstUntested && settings["uiLevel"] > 0) {
                    $('#untestedPlugins').show();
                    firstUntested = 0;
                }

                InsertPluginTableItem('untestedPlugins', data.name, html);
            } else {
                if (firstIncompatible && settings["uiLevel"] > 2) {
                    $('#incompatiblePlugins').show();
                    firstIncompatible = 0;
                }

                InsertPluginTableItem('incompatiblePlugins', data.name, html);
            }
        }

        function LoadInstalledPlugins() {
            for (var i = 0; i < installedPlugins.length; i++) {
                var url = 'api/plugin/' + installedPlugins[i];
                let index = i;
                $.ajax({
                    url: url,
                    dataType: 'json',
                    success: function (data) {
                        LoadPlugin(data);
                        FilterPlugins();
                    },
                    error: function () {
                        alert('Error, failed to fetch ' + installedPlugins[index]);
                    }
                });
            }
        }

        function LoadPlugins(pluginList) {
            for (var i = 0; i < pluginList.length; i++) {
                if (!PluginIsInstalled(pluginList[i][0])) {
                    var url = pluginList[i][1];
                    let index = i;
                    pluginInfoURLs[pluginList[i][0]] = url;

                    $('html,body').css('cursor', 'wait');
                    $.ajax({
                        url: url,
                        dataType: 'json',
                        success: function (data) {
                            $('html,body').css('cursor', 'auto');
                            LoadPlugin(data);
                            $('#pluginInput').off('input.pluginFilter').on('input.pluginFilter', function () { clearTimeout(filterDebounceTimer); filterDebounceTimer = setTimeout(FilterPlugins, 150); });
                            FilterPlugins();

                        },
                        error: function (d) {
                            $('html,body').css('cursor', 'auto');
                            if (d.statusText !== undefined) {
                                d = d.statusText;
                            }
                            alert('Error, failed to fetch ' + pluginList[index] + " - " + d);
                        }
                    });
                }
            }
        }

        function ManualLoadInfo() {
            var url = $('#pluginInput').val();

            if (url.indexOf('://') > -1) {
                if (url.indexOf('https://github.com/') > -1) {
                    url = url.replace(/https:\/\/github.com\//, 'https://raw.githubusercontent.com/').replace(/\/blob\//, '/');
                }

                $('html,body').css('cursor', 'wait');

                var onSuccess = function (data, viaProxy) {
                    $('html,body').css('cursor', 'auto');
                    pluginInfoURLs[data.repoName] = url;
                    if (viaProxy) {
                        // Loaded via the credentialed proxy => treat as private
                        // for subsequent install/upgrade operations.
                        pluginInfoUseCredentials[data.repoName] = 1;
                    }
                    LoadPlugin(data, true);
                    $('#pluginInput').val('');
                    FilterPlugins();
                    $('#row-' + data.repoName)[0].scrollIntoView({ behavior: 'smooth', block: 'center' });
                };

                // First try a direct anonymous fetch. If that fails (404/401/403
                // are typical for private repos), retry through the server-side
                // proxy which injects the configured GitHub credentials.
                $.ajax({
                    url: url,
                    dataType: 'json',
                    success: function (data) { onSuccess(data, false); },
                    error: function () {
                        $.ajax({
                            url: 'api/plugin/fetchInfo',
                            type: 'POST',
                            contentType: 'application/json',
                            data: JSON.stringify({ url: url, useCredentials: 1 }),
                            dataType: 'json',
                            success: function (data) {
                                if (data && data.Status === 'Error') {
                                    $('html,body').css('cursor', 'auto');
                                    alert('Error fetching plugin info: ' + data.Message + '\n\nIf this is a private repository, configure the GitHub user name and Personal Access Token on the Developer settings page.');
                                    return;
                                }
                                onSuccess(data, true);
                            },
                            error: function (d) {
                                $('html,body').css('cursor', 'auto');
                                if (d.statusText !== undefined) {
                                    d = d.statusText;
                                }
                                alert('Error, failed to fetch ' + url + " - " + d);
                            }
                        });
                    }
                });
            }
            else {
                alert('Invalid pluginInfo.json URL');
            }
        }
        // Generate a consistent color from a plugin name for the avatar
        function PluginColor(name) {
            var hash = 0;
            for (var i = 0; i < name.length; i++) {
                hash = name.charCodeAt(i) + ((hash << 5) - hash);
            }
            var hue = Math.abs(hash) % 360;
            return 'hsl(' + hue + ', 50%, 48%)';
        }

        function PluginInitial(name) {
            return name.charAt(0);
        }

        function UpdatePluginSectionCounts() {
            $('.fppPluginSection').each(function () {
                var visible = $(this).find('.fppPluginEntry.pluginFilterVisible').length;
                var total = $(this).find('.fppPluginEntry').length;
                var badge = $(this).find('.pluginSectionCount');
                if (badge.length === 0 && total > 0) {
                    badge = $('<span class="pluginSectionCount"></span>');
                    $(this).find('.pluginsHeader h2').append(badge);
                }
                if (total > 0) {
                    badge.text(visible + '/' + total);
                }
            });
        }

        var filterDebounceTimer = null;
        function FilterPlugins() {
            if ($('#pluginInput').val().indexOf('://') > -1) {
                $('.fppPluginInput').addClass('is-url');
                $('.fppPluginEntry').addClass('pluginFilterVisible');
                $('.fppPluginSection').addClass('pluginFilterSectionVisible');
            } else {
                $('.fppPluginInput').removeClass('is-url');
                var value = $('#pluginInput').val().toLowerCase();
                if (value == '') {
                    $('.fppPluginEntry').addClass('pluginFilterVisible');
                    $('.fppPluginSection').addClass('pluginFilterSectionVisible');
                } else {
                    $('.fppPluginSection').each(function () {
                        var filterMatchesInSection = 0;
                        $(this).children('.fppPluginEntry').each(function (index) {
                            var title = $(".pluginTitle", this).text().toLowerCase();
                            var author = $(this).find('.pluginAuthor').text().toLowerCase();
                            var desc = $(this).find('.pluginDescription').text().toLowerCase();
                            if (title.indexOf(value) > -1 || author.indexOf(value) > -1 || desc.indexOf(value) > -1) {
                                $(this).addClass('pluginFilterVisible');
                                filterMatchesInSection++;
                            } else {
                                $(this).removeClass('pluginFilterVisible');
                            }
                        });
                        if (filterMatchesInSection > 0) {
                            $(this).addClass('pluginFilterSectionVisible');
                        } else {
                            $(this).removeClass('pluginFilterSectionVisible');
                        }
                    });
                }
            }
            UpdatePluginSectionCounts();
        }

        function SetPluginView(view) {
            if (view === 'grid') {
                $('.plugindiv').addClass('fppPluginGridView');
                $('#pluginViewList').removeClass('active');
                $('#pluginViewGrid').addClass('active');
            } else {
                $('.plugindiv').removeClass('fppPluginGridView');
                $('#pluginViewList').addClass('active');
                $('#pluginViewGrid').removeClass('active');
            }
            try { localStorage.setItem('fppPluginView', view); } catch (e) {}
        }

        $(document).on('click', '#pluginViewList', function () { SetPluginView('list'); });
        $(document).on('click', '#pluginViewGrid', function () { SetPluginView('grid'); });

        $(document).ready(function () {
            // Uninstall All and Reinstall All are bulk destructive actions, so only
            // expose them in Advanced UI mode or higher.
            if (settings["uiLevel"] > 0) {
                $('#uninstallAllBtn').removeClass('d-none');
                $('#reinstallAllBtn').removeClass('d-none');
            }
            // Restore last-used view
            try {
                var savedView = localStorage.getItem('fppPluginView');
                if (savedView === 'grid') SetPluginView('grid');
            } catch (e) {}
            GetInstalledPlugins();

        });
    </script>
    <title><? echo $pageTitle; ?></title>
</head>

<body>
    <div id="bodyWrapper">
        <?php
        $activeParentMenuItem = 'content';
        include 'menu.inc'; ?>
        <div class="mainContainer">
            <h1 class="title">Plugins</h1>
            <div class="pageContent">

                <div id="plugins" class="settings">

                    <div id='pluginTableHead'>

                        <div class="row fppPluginInput">
                            <div class="col">
                                <input type="text" id="pluginInput"
                                    class="form-control form-control-lg form-control-rounded has-shadow"
                                    placeholder="Find a Plugin or Enter a plugininfo.json URL" />
                            </div>
                            <div class="col-auto fppPluginInputActionCol">
                                <div class="buttons btn-lg btn-rounded btn-outline-success" onClick='ManualLoadInfo();'>
                                    <i class="fas fa-download"></i> Get Plugin Info
                                </div>
                            </div>
                        </div>

                    </div>
                    <div class="d-flex justify-content-end gap-1 mt-2 mb-1">
                        <span class="fppPluginViewLabel">View:</span>
                        <div class="btn-group btn-group-sm" role="group">
                            <button type="button" class="buttons btn-outline-secondary active" id="pluginViewList" title="List view"><i class="fas fa-list"></i></button>
                            <button type="button" class="buttons btn-outline-secondary" id="pluginViewGrid" title="Grid view"><i class="fas fa-th-large"></i></button>
                        </div>
                    </div>
                    <div class='plugindiv'>

                        <div id='installedPlugins' class="fppPluginSection">
                            <div class='pluginsHeader'>
                                <h2>Installed Plugins</h2>
                                <div class="d-flex gap-2 align-items-center">
                                    <button id="checkAllUpdatesBtn" class="buttons btn-outline-success"
                                        onClick='CheckAllPluginsForUpdates();'
                                        title="Check all installed plugins for updates">
                                        <i class='fas fa-sync-alt'></i> Check All for Updates
                                    </button>
                                    <button id="reinstallAllBtn" class="buttons btn-outline-warning d-none"
                                        onClick='ShowReinstallAllPluginsPopup();'
                                        title="Uninstall and reinstall all installed plugins">
                                        <i class='fas fa-redo-alt'></i> Reinstall All
                                    </button>
                                    <button id="uninstallAllBtn" class="buttons btn-outline-danger d-none"
                                        onClick='ShowUninstallAllPluginsPopup();'
                                        title="Uninstall all installed plugins">
                                        <i class='fas fa-trash-alt'></i> Uninstall All
                                    </button>
                                </div>
                            </div>
                        </div>
                        <div id='pluginTable' class="fppPluginSection">
                            <div class='pluginsHeader '>
                                <h2>Available Plugins</h2>
                            </div>
                        </div>
                        <div id='untestedPlugins' class="fppPluginSection" style="display: none">
                            <div class='pluginsHeader '>
                                <h2>Plugins not tested with this FPP version</h2>
                            </div>
                        </div>
                        <div id='templatePlugin' class="fppPluginSection" style="display: none">
                            <div class='pluginsHeader '>
                                <h2>Template Plugin</h2>
                            </div>
                        </div>
                        <div id='incompatiblePlugins' class="fppPluginSection" style="display: none">
                            <div class='pluginsHeader '>
                                <h2>Incompatible Plugins</h2>
                            </div>
                        </div>
                    </div>

                    <div id="overlay">
                    </div>

                </div>
            </div>
        </div>


        <?php include 'common/footer.inc'; ?>
    </div>

</body>

</html>