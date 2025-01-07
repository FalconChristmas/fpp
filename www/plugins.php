<!DOCTYPE html>
<html lang="en">

<head>
    <?php
    include 'common/htmlMeta.inc';
    require_once('config.php');

    writeFPPVersionJavascriptFunctions();

    include 'common/menuHead.inc';
    ?>
    <script>
        var installedPlugins = [];
        var pluginInfos = [];
        var pluginInfoURLs = [];

        function PluginIsInstalled(plugin) {
            for (var i = 0; i < installedPlugins.length; i++) {
                if (installedPlugins[i] == plugin)
                    return 1;
            }

            return 0;
        }

        function PluginProgressDialogDone() {
            $('#pluginsProgressPopupCloseButton').prop("disabled", false);
            EnableModalDialogCloseButton("pluginsProgressPopup");
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

        function UpgradePlugin(plugin) {
            var url = 'api/plugin/' + plugin + '/upgrade?stream=true';
            DisplayProgressDialog("pluginsProgressPopup", "Upgrade Plugin");
            StreamURL(url, 'pluginsProgressPopupText', 'PluginProgressDialogDone', 'PluginProgressDialogDone');
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

            var postData = JSON.stringify(pluginInfo);
            DisplayProgressDialog("pluginsProgressPopup", "Install Plugin");
            StreamURL(url, 'pluginsProgressPopupText', 'PluginProgressDialogDone', 'PluginProgressDialogDone', 'POST', postData, 'application/json');
        }

        function UninstallPlugin(plugin) {
            var url = 'api/plugin/' + plugin + '?stream=true'; // Assuming your API supports streaming for uninstall
            DisplayProgressDialog("pluginsProgressPopup", "Uninstall Plugin");
            StreamURL(url, 'pluginsProgressPopupText', 'PluginProgressDialogDone', 'PluginProgressDialogDone', 'DELETE');
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

        function FindPluginInfo(plugin) {
            for (var i = 0; i < pluginInfos.length; i++) {
                if (pluginInfos[i].repoName == plugin)
                    return i;
            }

            return -1;
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
                    } else if (nv == getFPPMajorVersion()) {
                        maxFPPVersion = -1;
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
            var compatibleVersionClass = (compatibleVersion == -1) ? " has-previous-compatible-version" : '';
            html += '<div id="row-' + data.repoName + '" class="fppPluginEntry' + compatibleVersionClass + '"><div class="backdrop fppPluginEntryBackdrop"><div class="row">';
            html += '<div class="col-lg-3"><h3 class="pluginTitle">' + data.name + '</h3>';

            if (installed) {
                html += '<div class="text-success fppPluginEntryInstallStatus"><i class="far fa-check-circle"></i> <b>Installed</b></div>';
            }

            html += '</div>';
            html += '<div class="col-lg-2 text-muted"><div class="labelHeading">Author:</div>' + data.author + '</div>';
            html += '<div class="col-lg text-muted"><div class="labelHeading">Description:</div><div class="fppPluginEntryDescription">' + data.description + '</div>';

            html += '</div>';
            html += '<div class="col-lg-auto fppPluginEntryActions">';

            html += '<div align="right">';

            if (installed) {
                if (!data.hasOwnProperty('allowUpdates') || data.allowUpdates) {
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
                    html += "<button class='buttons btn-success' onClick=' InstallPlugin(\"" + data.repoName + "\", \"" + data.versions[idx].branch + "\", \"" + data.versions[idx].sha + "\");'><i class='far fa-arrow-alt-circle-down'></i> Install</button>";
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
                        html += ' &gt; v' + data.versions[i].minFPPVersion;
                    } else if (data.versions[i].maxFPPVersion > 0) {
                        html += ' &lt; v' + data.versions[i].maxFPPVersion;
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
                html += '</b></div></div>';
            }

            if (installed) {
                $('#installedPlugins').show();
                if (firstInstalled) {
                    firstInstalled = 0;
                }

                if (compatibleVersion == -1) {
                    html += '<div class="row"><div class="col" class="bad">WARNING: This plugin is already installed, but may be incompatible with this FPP version or platform.</div></div>';
                }

                InsertPluginTableItem('installedPlugins', data.name, html);
            } else if (data.repoName == 'fpp-plugin-Template') {
                if (settings["uiLevel"] > 2) {
                    $('#templatePlugin').show();
                    $('#templatePlugin').append(html);
                }
            } else if (untestedVersion >= 0) {
                if (firstUntested && settings["uiLevel"] > 0) {
                    $('#untestedPlugins').show();
                    firstUntested = 0;
                }

                InsertPluginTableItem('untestedPlugins', data.name, html);
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
                            $('#pluginInput').on('input', FilterPlugins);
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
                $.ajax({
                    url: url,
                    dataType: 'json',
                    success: function (data) {
                        $('html,body').css('cursor', 'auto');
                        pluginInfoURLs[data.repoName] = url;
                        LoadPlugin(data, true);
                        $('#pluginInput').val('');
                        FilterPlugins();
                    },
                    error: function (d) {
                        $('html,body').css('cursor', 'auto');
                        if (d.statusText !== undefined) {
                            d = d.statusText;
                        }
                        alert('Error, failed to fetch ' + pluginInfos[i] + " - " + d);
                    }
                });
            }
            else {
                alert('Invalid pluginInfo.json URL');
            }
        }
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
                            if ($(".pluginTitle", this).text().toLowerCase().indexOf(value) > -1) {
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

        }
        $(document).ready(function () {
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
                    <div class='plugindiv'>

                        <div id='installedPlugins' class="fppPluginSection">
                            <div class='pluginsHeader'>
                                <h2>Installed Plugins</h2>
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
