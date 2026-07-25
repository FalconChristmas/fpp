<!DOCTYPE html>
<html lang="en">

<head>
    <?php
    include 'common/htmlMeta.inc';
    require_once('config.php');
    require_once('common.php');

    // Device capability for the plugin resource-hint check (D14). Computed
    // server-side so it is exact for this box: total RAM (MB) and CPU cores.
    // Plugins may declare optional per-version resource requirements in
    // pluginInfo.json; the UI compares them against these values.
    $__pluginDevMem = get_server_memory_info();
    $pluginDeviceMemMB = (int) round(($__pluginDevMem['total'] ?? 0) / 1048576);
    $pluginDeviceCores = (int) trim(@shell_exec('nproc 2>/dev/null'));
    if ($pluginDeviceCores < 1) {
        $pluginDeviceCores = 1;
    }

    writeFPPVersionJavascriptFunctions();

    include 'common/menuHead.inc';
    ?>
    <script>
        var installedPlugins = [];
        var pluginInfos = [];
        var pluginInfoURLs = [];
        var pluginInfoUseCredentials = {};
        var manuallyLoadedPlugins = {};
        var lastAutoLoadedUrl = '';
        var urlLoadedRepo = null;
        var pluginUrlError = '';
        // --- Plugin categories (Phase 1) ---
        var pluginCategoryList = [];      // [{name,longName,slug,icon}] from pluginCategories.json
        var pluginCategoryBySlug = {};
        var pluginCategoryByName = {};
        var pluginCategoryOf = {};        // lowercased pluginList name -> category name
        var activeCategorySlug = 'all';
        var activeTopTab = 'available';
        var updatesCheckedOnce = false;
        // Both feed UpdatePopularStripVisibility(): the strip is hidden during a search
        // (the results grid is the answer then) and whenever it has nothing to show.
        var popularStripHasCards = false;
        var pluginSearchActive = false;
        var OTHER_CATEGORY = { name: 'Other', slug: 'other', icon: 'fas fa-puzzle-piece' };
        // The plugin list and its category taxonomy both live in FalconChristmas/fpp-data
        // (raw.githubusercontent.com is already allow-listed in FPP's Apache CSP connect-src).
        // The 3rd element of each pluginList entry is the (short) category name; older FPP
        // clients read only [0]/[1] and ignore it, so it is backward compatible.
        var PLUGIN_LIST_URL = 'https://raw.githubusercontent.com/FalconChristmas/fpp-data/master/pluginList.json';
        var PLUGIN_CATEGORIES_URL = 'https://raw.githubusercontent.com/FalconChristmas/fpp-data/master/pluginCategories.json';
        // --- Plugin popularity (Phase 2) ---
        // Install counts keyed by repoName (== row id). D17: the device does NOT fetch the
        // personal stats host from the browser — that origin is not in FPP's Apache CSP
        // connect-src. Instead the SAME-ORIGIN backend endpoint api/plugin/popularity proxies
        // the stats feed server-side (CSP does not apply to PHP), requests gzip, slims it to
        // repoName->count, and disk-caches the result (shared per box, 7-day TTL). It fails soft:
        // on an upstream error it serves a stale cache, else an empty map — the UI then hides
        // the Popular strip and falls back to name sort. Same-origin, so no popularity fixture
        // and no CSP entry are needed.
        var pluginPopularity = {};        // repoName -> integer install count
        var popularityLoaded = false;
        var POPULARITY_URL = 'api/plugin/popularity';

        // Device capability for the resource-hint check (D14), injected server-side
        // (exact for this box). 0 means "unknown" -> the check degrades to no-op.
        var DEVICE_MEM_MB = <?php echo $pluginDeviceMemMB; ?>;
        var DEVICE_CORES  = <?php echo $pluginDeviceCores; ?>;

        // Evaluate a plugin's optional resource requirements against this device.
        // Fields (both optional, top-level on pluginInfo.json — plugin-wide, not
        // per-version): minMemoryMB / minCpuCores (precise, self-reported).
        // Returns { known, exceeds, badge, label, title }:
        //   exceeds  - a precise minimum is not met by this device (drives hide-on-Basic
        //              and an install confirmation on Advanced+), only when the device
        //              value is known.
        //   badge    - show a muted advisory tag (only when exceeds).
        function EvalPluginResources(data) {
            var r = { known: false, exceeds: false, badge: false, label: '', title: '' };
            if (!data) return r;
            var minMem = parseInt(data.minMemoryMB) || 0;
            var minCores = parseInt(data.minCpuCores) || 0;
            if (!minMem && !minCores) return r; // nothing declared
            r.known = true;

            var memShort = (minMem > 0 && DEVICE_MEM_MB > 0 && minMem > DEVICE_MEM_MB);
            var coresShort = (minCores > 0 && DEVICE_CORES > 0 && minCores > DEVICE_CORES);
            r.exceeds = memShort || coresShort;

            if (r.exceeds) {
                var parts = [];
                if (memShort) parts.push('needs ' + minMem + ' MB RAM (this device has ' + DEVICE_MEM_MB + ' MB)');
                if (coresShort) parts.push('needs ' + minCores + ' CPU cores (this device has ' + DEVICE_CORES + ')');
                r.badge = true;
                r.label = 'Not Enough RAM/CPU';
                r.title = 'This plugin ' + parts.join('; ') + '. It may run poorly or not at all on this device.';
            }
            return r;
        }

        // Resource verdict for a plugin. Plugin-wide (not per-version), so this is a
        // thin, stably-named wrapper over EvalPluginResources — kept so call sites
        // don't need to know the fields moved off the versions[] entry.
        function PluginResourceVerdict(data) {
            return EvalPluginResources(data);
        }

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
            // Fetch the category taxonomy first (non-fatal on failure), then the list.
            $.ajax({
                url: PLUGIN_CATEGORIES_URL,
                dataType: 'json',
                complete: function (xhr) {
                    var cats = (xhr && xhr.responseJSON && xhr.responseJSON.categories) || [];
                    LoadPluginCategories(cats);
                    GetPluginListData();
                }
            });
        }

        function GetPluginListData() {
            $.ajax({
                url: PLUGIN_LIST_URL,
                dataType: 'json',
                success: function (data) {
                    LoadPlugins(data.pluginList);
                    // Both of these need installedPlugins and pluginInfos loaded, so
                    // they run here rather than in document.ready.
                    RestoreTopTab();
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

        // Build category lookup maps + pills from pluginCategories.json (D16).
        function LoadPluginCategories(cats) {
            pluginCategoryList = [];
            pluginCategoryBySlug = {};
            pluginCategoryByName = {};
            for (var i = 0; i < cats.length; i++) {
                var c = cats[i];
                if (!c || !c.name) continue;
                if (!c.slug) c.slug = c.name.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-+|-+$/g, '');
                if (!c.icon) c.icon = 'fas fa-puzzle-piece';
                pluginCategoryList.push(c);
                pluginCategoryBySlug[c.slug] = c;
                // name is the PRIMARY key (what pluginList stores + matches on); longName
                // is a SECONDARY key kept for back-tolerance + the tooltip.
                pluginCategoryByName[c.name] = c;
                if (c.longName) pluginCategoryByName[c.longName] = c;
            }
            // Present categories alphabetically by their displayed label so the visible
            // pill order reads A-Z ("All" stays pinned first in BuildCategoryPills).
            pluginCategoryList.sort(function (a, b) { return a.name.localeCompare(b.name, undefined, { sensitivity: 'base' }); });
            BuildCategoryPills();
        }

        // "Official" = clone origin (srcURL) is a repo in the FalconChristmas GitHub org.
        // Parse the URL (host + first path segment) so a spoofed host/path can't earn it (D18).
        function IsOfficialPlugin(data) {
            var u = data && data.srcURL;
            if (!u) return false;
            try {
                var parsed = new URL(u);
                if (parsed.host.toLowerCase() !== 'github.com') return false;
                var seg = parsed.pathname.split('/').filter(function (x) { return x.length > 0; });
                return seg.length > 0 && seg[0].toLowerCase() === 'falconchristmas';
            } catch (e) {
                return false;
            }
        }

        // Attribution HTML derived from the clone origin (srcURL) rather than the
        // self-supplied `author` field, which nobody verifies (D18). Returns the repo
        // owner (the GitHub/host account or org) linked to their profile. The
        // self-reported `author` is deliberately not shown — only the verifiable
        // source owner. Returns '' when there is no usable source URL (caller omits
        // the attribution line entirely).
        function PluginAuthorHtml(data) {
            var u = data && data.srcURL;
            if (!u) return '';
            try {
                var parsed = new URL(u);
                var seg = parsed.pathname.split('/').filter(function (x) { return x.length > 0; });
                if (seg.length === 0) return '';
                var owner = seg[0];
                var profile = (parsed.host.toLowerCase() === 'github.com')
                    ? 'https://github.com/' + owner
                    : parsed.origin + '/' + owner;
                return '<a href="' + profile + '" target="_blank" rel="noopener noreferrer">' + owner + '</a>';
            } catch (e) {
                return '';
            }
        }

        // Compute a 1-2 character initial from a plugin name.
        // "FPP Brightness" -> "FB", "MatrixTools" -> "M", "a test plugin" -> "AT"
        function GetInitials(name) {
            if (!name) return '?';
            var words = name.replace(/[^a-zA-Z0-9\s-]/g, '').split(/[\s-]+/).filter(function (w) { return w.length > 0; });
            if (words.length === 0) return name.charAt(0).toUpperCase() || '?';
            if (words.length === 1) return words[0].charAt(0).toUpperCase();
            return (words[0].charAt(0) + words[1].charAt(0)).toUpperCase();
        }

        // Cache-busting nonce shared by all icon URLs loaded during this page
        // session, so a plugin update with a new icon is visible immediately
        // rather than showing a stale cached copy. The server sends 304 Not
        // Modified when the icon file is unchanged, so the overhead is minimal.
        var iconCacheNonce = Date.now();

        // Get the plugin icon URL. Always routes through the same-origin API to
        // avoid CSP restrictions on external image hosts (raw.githubusercontent.com
        // is only allow-listed in connect-src, not img-src).
        function GetIconUrl(data, installed) {
            if (installed) {
                if (data.hasOwnProperty('hasIcon') && !data.hasIcon) return null;
                return 'api/plugin/' + data.repoName + '/icon?_=' + iconCacheNonce;
            }
            if (data.iconURL) return 'api/plugin/fetchImage?url=' + encodeURIComponent(data.iconURL);
            return null;
        }

        function BuildCategoryPills() {
            var $pills = $('#pluginCategoryPills');
            if (!$pills.length) return;
            $pills.empty();
            var uiLevel = parseInt(settings["uiLevel"]) || 0;
            var pills = [];
            // "All" view shown at every UI level (D27) and is the default landing view.
            pills.push({ name: 'All', slug: 'all', icon: 'fas fa-border-all' });
            for (var i = 0; i < pluginCategoryList.length; i++) {
                // Drop any "Other" entry from the JSON so we only insert
                // our canonical OTHER_CATEGORY below with the correct icon.
                if (pluginCategoryList[i].name.localeCompare('Other', undefined, { sensitivity: 'base' }) === 0) continue;
                pills.push(pluginCategoryList[i]);
            }
            // Insert "Other" alphabetically among the known categories (skip index 0 which is "All")
            var insIdx = 1;
            while (insIdx < pills.length && pills[insIdx].name.localeCompare('Other', undefined, { sensitivity: 'base' }) < 0) insIdx++;
            pills.splice(insIdx, 0, OTHER_CATEGORY);
            activeCategorySlug = 'all';
            for (var j = 0; j < pills.length; j++) {
                var c = pills[j];
                var li = $('<li class="nav-item" role="presentation"></li>');
                var btn = $('<button type="button" role="tab" class="nav-link text-nowrap"></button>');
                if (c.slug === activeCategorySlug) btn.addClass('active');
                btn.attr('data-category-slug', c.slug);
                btn.attr('title', c.longName || c.name);
                btn.html('<i class="' + c.icon + '"></i> ' + c.name +
                    ' <span class="badge bg-secondary ms-1 fppCatCount" data-count-slug="' + c.slug + '">0</span>');
                btn.on('click', function () {
                    $('#pluginCategoryPills .nav-link').removeClass('active');
                    $(this).addClass('active');
                    activeCategorySlug = $(this).attr('data-category-slug');
                    this.scrollIntoView({ block: 'nearest', inline: 'center' });
                    BuildPopularStrip();   // strip follows the category being browsed
                    FilterPlugins();
                });
                li.append(btn);
                $pills.append(li);
            }
            FilterPlugins();
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
                        if (data.updatesAvailable) {
                            $('#row-' + plugin).addClass('fppHasUpdate').find('.updatesAvailable').show();
                        } else {
                            $('#row-' + plugin).removeClass('fppHasUpdate');
                            $.jGrowl('No updates available for ' + plugin, { themeState: 'detract' });
                        }
                        FilterPlugins();
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
                            $('#row-' + plugin).addClass('fppHasUpdate').find('.updatesAvailable').show();
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
                            FilterPlugins();
                        }
                    },
                    error: function () {
                        checked++;
                        if (checked === total) {
                            $('html,body').css('cursor', 'auto');
                            $('#checkAllUpdatesBtn').prop('disabled', false);
                            $.jGrowl('Completed checking plugins (some checks failed)', { themeState: 'warn' });
                            FilterPlugins();
                        }
                    }
                });
            });
        }

        // Update All: (re)check every installed plugin for updates, then upgrade
        // each one that has an update available, sequentially, in a single progress
        // dialog. Cheaper and safer than Reinstall All -- it only touches plugins
        // with a pending update and never uninstalls anything (no removal window),
        // so a shared dependency can't be dropped mid-flight. Mirrors the Reinstall
        // All queue + progress-dialog + verify-by-recheck pattern.
        var updateAllQueue = [];
        var updateAllAttempted = [];
        var updateAllTotal = 0;
        // The progress popup body is a <textarea> (see DisplayProgressDialog) and
        // cannot render HTML, so status lines are plain text appended to .value,
        // matching how StreamURL writes the streamed command output. Auto-scroll.
        function UpdateAllLog(text) {
            var outputArea = document.getElementById('pluginsProgressPopupText');
            if (!outputArea)
                return;
            outputArea.value += text;
            outputArea.scrollTop = outputArea.scrollHeight;
        }

        // Entry point (toolbar button). Runs a fresh update check across all
        // installed plugins first so the user does not have to click "Check All for
        // Updates" beforehand, then confirms and upgrades those with updates.
        function UpdateAllPlugins() {
            if (installedPlugins.length === 0) {
                $.jGrowl('No plugins installed', { themeState: 'detract' });
                return;
            }
            $('html,body').css('cursor', 'wait');
            $('#updateAllBtn').prop('disabled', true);
            $('#checkAllUpdatesBtn').prop('disabled', true);

            var checked = 0;
            var total = installedPlugins.length;
            var withUpdates = [];
            installedPlugins.forEach(function (plugin) {
                $.ajax({
                    url: 'api/plugin/' + plugin + '/updates',
                    type: 'POST',
                    dataType: 'json',
                    success: function (data) {
                        if (data.Status == 'OK' && data.updatesAvailable) {
                            $('#row-' + plugin).addClass('fppHasUpdate').find('.updatesAvailable').show();
                            withUpdates.push(plugin);
                        }
                    },
                    // complete runs on both success and error so one failed check
                    // does not strand the batch.
                    complete: function () {
                        checked++;
                        if (checked === total)
                            UpdateAllChecksDone(withUpdates);
                    }
                });
            });
        }

        function UpdateAllChecksDone(withUpdates) {
            $('html,body').css('cursor', 'auto');
            $('#updateAllBtn').prop('disabled', false);
            $('#checkAllUpdatesBtn').prop('disabled', false);
            FilterPlugins();
            if (withUpdates.length === 0) {
                $.jGrowl('All plugins are up to date', { themeState: 'success' });
                return;
            }
            DoModalDialog({
                id: "updateAllPluginsDialog",
                class: "modal-lg",
                title: "Update All Plugins",
                body: "Update the " + withUpdates.length + " plugin(s) with an available update, one at a time?" +
                    "<div class='small text-secondary mt-2'>" + withUpdates.join(', ') + "</div>",
                backdrop: true,
                keyboard: true,
                buttons: {
                    "Update All": function () {
                        CloseModalDialog("updateAllPluginsDialog");
                        RunUpdateAll(withUpdates);
                    },
                    Abort: function () {
                        CloseModalDialog("updateAllPluginsDialog");
                    }
                }
            });
        }

        function RunUpdateAll(withUpdates) {
            updateAllQueue = withUpdates.slice();
            updateAllAttempted = withUpdates.slice();
            updateAllTotal = withUpdates.length;
            DisplayProgressDialog("pluginsProgressPopup", "Update All Plugins");
            UpdateNextPlugin();
        }

        function UpdateNextPlugin() {
            if (updateAllQueue.length === 0) {
                UpdateAllFinish();
                return;
            }
            var plugin = updateAllQueue.shift();
            var done = updateAllTotal - updateAllQueue.length;
            SetProgressDialogStatus('pluginsProgressPopup',
                'Update All — updating ' + done + ' of ' + updateAllTotal);
            UpdateAllLog('\n===== Updating ' + plugin + ' (' + done + ' of ' + updateAllTotal + ') =====\n');
            var url = 'api/plugin/' + plugin + '/upgrade?stream=true';
            // Chain to the next plugin on both success and failure so a single
            // failed upgrade does not stop the rest of the batch.
            StreamURL(url, 'pluginsProgressPopupText', 'UpdateNextPlugin', 'UpdateNextPlugin');
        }

        // After all upgrades have streamed, verify by re-checking each attempted
        // plugin: the upgrade endpoint streams output even on a logical failure, so
        // any plugin that STILL reports an update available did not update. Mirrors
        // ReinstallFinish's re-query verification.
        function UpdateAllFinish() {
            var rechecked = 0;
            var total = updateAllAttempted.length;
            var stillStale = [];
            updateAllAttempted.forEach(function (plugin) {
                $.ajax({
                    url: 'api/plugin/' + plugin + '/updates',
                    type: 'POST',
                    dataType: 'json',
                    success: function (data) {
                        if (data.Status == 'OK' && data.updatesAvailable) {
                            stillStale.push(plugin);
                            $('#row-' + plugin).addClass('fppHasUpdate').find('.updatesAvailable').show();
                        } else {
                            $('#row-' + plugin).removeClass('fppHasUpdate').find('.updatesAvailable').hide();
                        }
                    },
                    complete: function () {
                        rechecked++;
                        if (rechecked === total) {
                            var ok = total - stillStale.length;
                            SetProgressDialogStatus('pluginsProgressPopup',
                                stillStale.length ? ('Update All — ' + stillStale.length + ' may have failed, ' + ok + ' of ' + total + ' ok')
                                                  : ('Update All — complete (' + ok + ' of ' + total + ')'));
                            UpdateAllLog('\n===== Update complete: ' + ok + ' of ' + total + ' plugin(s) updated successfully =====\n');
                            if (stillStale.length)
                                UpdateAllLog('Still reporting an available update (may have failed): ' + stillStale.join(', ') + '\n');
                            UpdateAllLog('Reload the page to refresh the plugin list.\n');
                            if (stillStale.length)
                                $.jGrowl(stillStale.length + ' plugin(s) may not have updated', { themeState: 'warn' });
                            else
                                $.jGrowl('All ' + ok + ' plugin(s) updated successfully', { themeState: 'success' });
                            FilterPlugins();
                            ProgressDialogDone('pluginsProgressPopupText');
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

        // Gate before InstallPlugin (D12): Official plugins (FalconChristmas org)
        // install directly; third-party/community plugins pop a confirmation first
        // so the user acknowledges they are installing code from outside the FPP
        // project, which runs with full access to their system. Applies at all UI
        // levels and to every install entry point (cards, popular strip, modal).
        function ConfirmAndInstall(plugin, branch, sha) {
            var i = FindPluginInfo(plugin);
            var data = (i >= 0) ? pluginInfos[i] : null;
            var res = data ? PluginResourceVerdict(data) : { exceeds: false };
            // Resource warning is orthogonal to trust: it applies to Official plugins too.
            var resWarn = '';
            if (res.exceeds)
                resWarn = '<div class="fpp-major-callout mb-2"><i class="fas fa-microchip"></i>' +
                    '<span><b>Not enough RAM/CPU.</b> ' + res.title +
                    ' Installing it anyway may degrade or disrupt your show.</span></div>';
            if (data && IsOfficialPlugin(data)) {
                // Official plugins install directly unless they exceed device resources.
                if (!resWarn) {
                    InstallPlugin(plugin, branch, sha);
                    return;
                }
                DoModalDialog({
                    id: "confirmInstallDialog",
                    class: "modal-lg",
                    title: "Install this plugin?",
                    body: resWarn,
                    backdrop: true,
                    keyboard: true,
                    buttons: {
                        "Install anyway": function () {
                            CloseModalDialog("confirmInstallDialog");
                            InstallPlugin(plugin, branch, sha);
                        },
                        Cancel: function () {
                            CloseModalDialog("confirmInstallDialog");
                        }
                    }
                });
                return;
            }
            var name = (data && data.name) ? data.name : plugin;
            var src = (data && data.srcURL) ? data.srcURL : '';
            var body = resWarn +
                '<div class="fpp-inline-warn mb-2"><i class="fas fa-exclamation-triangle"></i>' +
                '<span>Installing <b>' + name + '</b> runs ' +
                '<b>third-party, untrusted code</b> on your FPP. It has full access to this device <b>and to ' +
                'anything else on the network FPP is connected to</b>. This is inherently dangerous unless you ' +
                'trust the plugin\'s author. The FPP project <b>does not test, vet, or guarantee the quality or ' +
                'safety</b> of plugins &mdash; install at your own risk, and only from authors you trust. The ' +
                '<span class="badge text-bg-graceful"><i class="fas fa-certificate"></i> Official</span> badge marks ' +
                'plugins maintained by the FPP team (this plugin is not one of them).</span></div>';
            if (src) body += '<div class="small text-secondary"><i class="fas fa-code"></i> Source: ' +
                '<a href="' + src + '" target="_blank" rel="noopener noreferrer">' + src + '</a></div>';
            DoModalDialog({
                id: "confirmInstallDialog",
                class: "modal-lg",
                title: "Install third-party plugin?",
                body: body,
                backdrop: true,
                keyboard: true,
                buttons: {
                    Install: function () {
                        CloseModalDialog("confirmInstallDialog");
                        InstallPlugin(plugin, branch, sha);
                    },
                    Cancel: function () {
                        CloseModalDialog("confirmInstallDialog");
                    }
                }
            });
        }

        function UninstallPlugin(plugin) {
            var url = 'api/plugin/' + plugin + '?stream=true'; // Assuming your API supports streaming for uninstall
            DisplayProgressDialog("pluginsProgressPopup", "Uninstall Plugin");
            StreamURL(url, 'pluginsProgressPopupText', 'ProgressDialogDone', 'ProgressDialogDone', 'DELETE');
        }

        function ShowUninstallPluginPopup(plugin, pluginName) {
            if (!pluginName) {
                var pi = FindPluginInfo(plugin);
                pluginName = (pi >= 0 && pluginInfos[pi].name) ? pluginInfos[pi].name : plugin;
            }
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
        // (rendering), ShowPluginDetail (the modal) and ReinstallAllPlugins so the
        // selection logic lives in one place. MUST be idempotent: it is called
        // repeatedly on the same pluginInfo object (card render, then again when the
        // detail modal opens). It therefore does NOT mutate the version data — a
        // version with no declared upper bound that was built for an older FPP major
        // is capped only in a LOCAL variable for the compatibility test. (The earlier
        // version mutated maxFPPVersion, so the second call saw a real cap and lost
        // the "untested" flag — the card offered install but the modal didn't.)
        function SelectPluginVersionIndices(data) {
            var compatibleVersion = -1;
            var untestedVersion = -1;
            var curMajor = getFPPMajorVersion();
            var isUnset = function (m) { return m == "0" || m == "0.0" || m == "" || m == undefined; };
            for (var i = 0; i < data.versions.length; i++) {
                var v = data.versions[i];
                var effMax = v.maxFPPVersion; // effective upper bound used only for this test
                if (isUnset(effMax)) {
                    // No upper bound declared. If it was built for a different (older)
                    // FPP major, treat it as "not updated for this version": cap at the
                    // previous major's .999 so the compat test fails, and flag untested.
                    var minMajor = String(v.minFPPVersion).split('.')[0];
                    if (minMajor != curMajor) {
                        effMax = (curMajor - 1) + ".999";
                        untestedVersion = i;
                    }
                }

                var minOk = CompareFPPVersions(v.minFPPVersion, getFPPVersionTriplet()) <= 0;
                var maxOk = isUnset(effMax) || (CompareFPPVersions(effMax, getFPPVersionTriplet()) >= 0);
                var platOk = (!v.hasOwnProperty('platforms')) || (v.platforms.includes(settings['Platform']));
                if (minOk && maxOk && platOk)
                    compatibleVersion = i;
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
        // Sorted insert shared by both grids: group by rank first (lower sorts higher),
        // then A-Z within a rank. Available cards rank by how usable the plugin is on
        // this box (see PluginSortRank); the Installed grid passes 0 throughout, so it
        // stays purely alphabetical. Rank and name are both known at insert time, so
        // insertion order is final -- nothing ever re-sorts the grid.
        function InsertCardSorted(gridId, key, html, rank) {
            var strcmp = new Intl.Collator(undefined, { numeric: true, sensitivity: 'base' }).compare;
            var r = rank || 0;
            var placed = false;
            $('#' + gridId).children('.pluginCard').each(function () {
                if (placed) return;
                var tr = parseInt($(this).attr('data-sort-rank'), 10) || 0;
                var t = $(this).find('.pluginTitle').text();
                if (tr > r || (tr === r && t && strcmp(t, key) >= 0)) {
                    $(html).insertBefore(this); placed = true;
                }
            });
            if (!placed) $('#' + gridId).append(html);
        }

        // --- Popularity (Phase 2) ---

        // Install count for a repo (0 when unknown / feed unavailable).
        function PopularityOf(repo) {
            var n = pluginPopularity[repo];
            return (typeof n === 'number' && n > 0) ? n : 0;
        }

        // A quiet install-count tag. Count is an integer, so the interpolation below is
        // safe; untrusted plugin text is never placed here.
        function PopularityBadgeHtml(count) {
            if (!count) return '';
            var formatted = count.toLocaleString();
            return '<span class="fpp-tag gap-1 me-1 fppPopBadge" title="' + formatted +
                ' installs (past year)" aria-label="' + formatted + ' installs">' +
                '<i class="fas fa-download"></i> ' + formatted + '</span>';
        }

        // Fetch install counts via the backend proxy (api/plugin/popularity), which
        // caches server-side (shared per box, 7-day TTL) — no browser-side cache. Runs in
        // parallel with the installed/list loads; failure degrades gracefully.
        function GetPluginPopularity() {
            $.ajax({
                url: POPULARITY_URL,
                dataType: 'json',
                success: function (d) {
                    // Accept the slim snapshot ({counts:{…}} or a bare repoName->count
                    // map), and still tolerate the raw stats feed shape if ever pointed
                    // at it directly. Treat the payload as untrusted.
                    var counts = (d && d.counts) ? d.counts
                        : (d && d.topPlugins && d.topPlugins.data)
                            ? (d.topPlugins.data.last365Days || d.topPlugins.data.totalCount || {})
                            : (d || {});
                    var map = {};
                    for (var k in counts) {
                        if (!counts.hasOwnProperty(k)) continue;
                        var n = parseInt(counts[k], 10);
                        if (!isNaN(n) && n > 0) map[k] = n;
                    }
                    ApplyPopularity(map);
                }
                // No error handler: no badges, no Popular strip, alphabetical order.
            });
        }

        function ApplyPopularity(map) {
            pluginPopularity = map || {};
            popularityLoaded = true;
            PatchPopularityBadges();   // stamp already-rendered cards
            BuildPopularStrip();
            FilterPlugins();
        }

        // Inject/refresh the install-count tag on already-rendered cards. Only the tag:
        // the grids are sorted A-Z, so a late-arriving count never reorders anything.
        function PatchPopularityBadges() {
            $('#pluginGrid, #installedGrid').children('.pluginCard').each(function () {
                var repo = ($(this).attr('id') || '').replace(/^row-/, '');
                var count = PopularityOf(repo);
                var $holder = $(this).find('.pluginCardBadges').first();
                $holder.find('.fppPopBadge').remove();
                if (count) $holder.append(PopularityBadgeHtml(count));
            });
        }

        // Debounced "loads settled" strip rebuild + filter: popularity can arrive
        // mid-load, and the strip needs the full plugin list to rank against.
        var _settleTimer = null;
        function ScheduleSettle() {
            if (!popularityLoaded) return;
            clearTimeout(_settleTimer);
            _settleTimer = setTimeout(function () {
                BuildPopularStrip();
                FilterPlugins();
            }, 300);
        }

        // Top-10 Popular strip for the active category ("All" spans every category).
        // Excludes already-installed plugins and ones that can't be installed on this box
        // (no compatible or untested version) — the strip is a discovery surface for
        // plugins the user can actually add. Rebuilt as info/feed arrive and whenever the
        // category changes; the grid itself is plain A-Z, so this is the only place
        // popularity affects what you see first.
        function BuildPopularStrip() {
            var $strip = $('#popularStrip');
            if (!$strip.length) return;
            popularStripHasCards = false;
            if (!popularityLoaded) { UpdatePopularStripVisibility(); return; }
            var uiLevel = parseInt(settings["uiLevel"]) || 0;
            var ranked = [];
            for (var i = 0; i < pluginInfos.length; i++) {
                var d = pluginInfos[i];
                if (!d || !d.repoName) continue;
                if (PluginIsInstalled(d.repoName)) continue;   // exclude installed
                var sel = SelectPluginVersionIndices(d);
                if (sel.compatible < 0 && sel.untested < 0) continue;  // exclude uninstallable
                // Basic UI: don't recommend a plugin this device doesn't meet the
                // minimum memory/CPU for (matches the grid's hide-on-Basic rule).
                if (uiLevel < 1 && PluginResourceVerdict(d).exceeds) continue;
                if (activeCategorySlug !== 'all' &&
                    PluginCategoryInfo(d).obj.slug !== activeCategorySlug) continue;
                var c = PopularityOf(d.repoName);
                if (c > 0) ranked.push({ data: d, count: c });
            }
            ranked.sort(function (a, b) { return b.count - a.count; });
            ranked = ranked.slice(0, 10);
            $strip.empty();
            popularStripHasCards = ranked.length > 0;
            for (var j = 0; j < ranked.length; j++)
                $strip.append(PopularCardHtml(ranked[j].data, ranked[j].count));

            // Name the category being browsed so the strip explains its own contents.
            // .text() because category names come from the plugin feed.
            var $h = $('#popularStripHeading');
            if (activeCategorySlug === 'all') $h.text('Popular Plugins');
            else {
                var pill = pluginCategoryBySlug[activeCategorySlug];
                $h.text(pill ? 'Popular in ' + pill.name : 'Popular Plugins');
            }
            UpdatePopularStripVisibility();
        }

        // Sole owner of the strip's visibility: BuildPopularStrip and FilterPlugins both
        // have a say (cards vs. search), so deciding it in one place keeps them from
        // fighting each other and flickering the strip.
        function UpdatePopularStripVisibility() {
            var show = popularityLoaded && popularStripHasCards && !pluginSearchActive;
            $('#popularStripWrap').toggleClass('d-none', !show);
            // Only measurable once shown: while the wrap (or its pane) is d-none,
            // clientWidth is 0 and every strip looks like it does not overflow.
            if (show) UpdatePopularScrollState();
        }

        // Compact card for the Popular strip. Built with jQuery .text() for the
        // author-controlled name so the strip does not widen the existing XSS surface.
        function PopularCardHtml(data, count) {
            // Strip only holds installable, not-installed plugins (see BuildPopularStrip),
            // so a compatible/untested version index always exists here.
            var repo = data.repoName;
            var cat = PluginCategoryInfo(data);
            var $col = $('<div class="pluginPopularCard"></div>');
            // The whole card opens the detail modal; there is no direct install from the
            // strip so users always see the plugin's full detail (and third-party
            // warning) before installing.
            var $card = $('<div class="card h-100 pluginCardInner" role="button" tabindex="0"></div>')
                .on('click', function () { ShowPluginDetail(repo); });
            var $body = $('<div class="card-body d-flex flex-column p-2"></div>');
            // Category logo + name so the strip conveys what each popular plugin is for.
            // Wraps (no truncation) so long category names show in full on the narrow card.
            $body.append('<div class="small text-secondary mb-1" title="' + (cat.obj.longName || cat.name) +
                '"><i class="' + cat.obj.icon + '"></i> ' + cat.name + '</div>');
            // Icon + title row
            var $titleRow = $('<div class="d-flex align-items-center gap-2 mb-1"></div>');
            var iconUrl = GetIconUrl(data, false);
            var initials = GetInitials(data.name);
            if (iconUrl) {
                var $iconWrap = $('<div class="pluginIconWrap pluginIconWrapSm flex-shrink-0"></div>');
                var $img = $('<img class="pluginIcon pluginIconSm" src="' + iconUrl + '" alt="" loading="lazy">');
                var $fb = $('<div class="pluginIconFallback pluginIconFallbackSm" style="display:none;">' + initials + '</div>');
                $img.on('error', function () { $img.hide(); $fb.show(); });
                $iconWrap.append($img).append($fb);
                $titleRow.append($iconWrap);
            } else {
                $titleRow.append($('<div class="pluginIconWrap pluginIconWrapSm flex-shrink-0"><div class="pluginIconFallback pluginIconFallbackSm">' + initials + '</div></div>'));
            }
            $titleRow.append($('<div class="card-title fw-semibold small mb-0 pluginPopularTitle pluginTitle min-w-0"></div>').text(data.name));
            $body.append($titleRow);
            // Bottom line: install count over the past year. Shares PopularityBadgeHtml
            // with the grid cards so the two can't drift apart.
            var $act = $('<div class="mt-auto d-flex align-items-center gap-2"></div>');
            $act.append(PopularityBadgeHtml(count));
            $body.append($act);
            $card.append($body);
            $col.append($card);
            return $col;
        }

        // The edge fade and the arrows exist because overlay scrollbars (macOS/iOS/
        // Android) stay invisible until you are already scrolling, so without them
        // roughly half the strip is undiscoverable. Both are driven from here: measure
        // the overflow, then reflect it into state classes (read by .pluginPopularScroll
        // in fpp.css) and the arrow buttons. Must run only once #popularStripWrap is
        // visible -- clientWidth is 0 while it, or the pane, is d-none.
        function UpdatePopularScrollState() {
            var el = document.getElementById('popularStrip');
            if (!el) return;
            var max = el.scrollWidth - el.clientWidth;
            var overflows = max > 1;    // 1px of slack for sub-pixel layout
            var atStart = !overflows || el.scrollLeft <= 1;
            var atEnd = !overflows || el.scrollLeft >= max - 1;

            el.classList.toggle('is-scrollable-start', !atStart);
            el.classList.toggle('is-scrollable-end', !atEnd);
            $('#popularStripNav').toggleClass('d-none', !overflows);
            $('#popularStripPrev').prop('disabled', atStart);
            $('#popularStripNext').prop('disabled', atEnd);
        }

        // Scroll by just under a viewport so one card stays on screen as an anchor;
        // scroll-snap then settles the result to a card edge.
        function ScrollPopularStrip(dir) {
            var el = document.getElementById('popularStrip');
            if (!el) return;
            var reduce = window.matchMedia('(prefers-reduced-motion: reduce)').matches;
            el.scrollBy({ left: dir * el.clientWidth * 0.8, behavior: reduce ? 'auto' : 'smooth' });
        }

        // Bound once: BuildPopularStrip() only empties #popularStrip, it never replaces
        // the element, so these survive every rebuild.
        function BindPopularStripControls() {
            var el = document.getElementById('popularStrip');
            if (!el) return;
            var raf = null;
            el.addEventListener('scroll', function () {
                // Coalesce to one update per frame: the fade has to track the scroll
                // live, so a trailing debounce would lag and a leading one would miss
                // the final resting position.
                if (raf) return;
                raf = requestAnimationFrame(function () { raf = null; UpdatePopularScrollState(); });
            }, { passive: true });
            // Catches window resize, the tab/pane d-none -> visible transition, and font
            // reflow -- none of which fire a scroll event, and the last two of which
            // never fire window resize either.
            if (window.ResizeObserver) new ResizeObserver(UpdatePopularScrollState).observe(el);
            $('#popularStripPrev').on('click', function () { ScrollPopularStrip(-1); });
            $('#popularStripNext').on('click', function () { ScrollPopularStrip(1); });
        }

        function PluginVersionsText(data) {
            var html = '';
            for (var i = 0; i < data.versions.length; i++) {
                if (i > 0) html += ',';
                if ((data.versions[i].minFPPVersion > 0) && (data.versions[i].maxFPPVersion > 0))
                    html += ' v' + data.versions[i].minFPPVersion + ' - v' + data.versions[i].maxFPPVersion;
                else if (data.versions[i].minFPPVersion > 0)
                    html += ' > v' + data.versions[i].minFPPVersion;
                else if (data.versions[i].maxFPPVersion > 0)
                    html += ' < v' + data.versions[i].maxFPPVersion;
                if (data.versions[i].hasOwnProperty('platforms')) {
                    var platforms = data.versions[i].platforms;
                    html += ' ';
                    for (var p = 0; p < platforms.length; p++) {
                        if (p != 0) html += '/';
                        if (platforms[p] == 'Raspberry Pi') html += 'Pi';
                        else if (platforms[p] == 'BeagleBone Black') html += 'BBB';
                        else if (platforms[p] == 'BeagleBone 64') html += 'BB64';
                        else html += platforms[p];
                    }
                }
            }
            return html;
        }

        // Full-detail modal for a plugin card (reuses FPP's DoModalDialog).
        function ShowPluginDetail(repo) {
            var i = FindPluginInfo(repo);
            if (i < 0) return;
            var data = pluginInfos[i];
            var installed = PluginIsInstalled(repo);
            var sel = SelectPluginVersionIndices(data);
            var compatibleVersion = sel.compatible, untestedVersion = sel.untested;

            // Small icon to the left of the title
            var iconUrl = GetIconUrl(data, installed);
            var initials = GetInitials(data.name);
            var titleIcon = '';
            if (iconUrl) {
                titleIcon += '<div class="pluginIconWrap" style="width:2rem;height:2rem;border-radius:0.4rem;display:inline-flex;vertical-align:middle;margin-right:0.5rem;">';
                titleIcon += '<img class="pluginIcon" src="' + iconUrl + '" alt="" loading="lazy"';
                titleIcon += ' onerror="this.style.display=\'none\';this.nextElementSibling.style.display=\'flex\';">';
                titleIcon += '<div class="pluginIconFallback" style="display:none;font-size:0.65rem;">' + initials + '</div>';
                titleIcon += '</div>';
            } else {
                titleIcon += '<div class="pluginIconWrap" style="width:2rem;height:2rem;border-radius:0.4rem;display:inline-flex;vertical-align:middle;margin-right:0.5rem;"><div class="pluginIconFallback" style="font-size:0.65rem;">' + initials + '</div></div>';
            }

            var body = '';
            body += '<div class="mb-2">' + PluginBadgesHtml(data, true) + '</div>';
            var authorHtml = PluginAuthorHtml(data);
            if (authorHtml) body += '<div class="mb-2 text-secondary"><i class="fas fa-user"></i> ' + authorHtml + '</div>';
            body += '<p>' + data.description + '</p>';
            // Supported-versions detail is noise for Basic users; show it from Advanced up.
            if ((parseInt(settings["uiLevel"]) || 0) >= 1)
                body += '<div class="mb-2 text-muted small"><i class="fas fa-info-circle"></i> Compatible FPP versions: <b>' + PluginVersionsText(data) + '</b></div>';
            if (!installed && compatibleVersion == -1 && untestedVersion >= 0)
                body += '<div class="fpp-inline-warn mb-2"><i class="fas fa-exclamation-triangle"></i>' +
                    '<span>This plugin has not been updated to work with your version of FPP (' + getFPPMajorVersion() + '). You can still install it, but it may not work correctly.</span></div>';
            else if (!installed && compatibleVersion == -1)
                body += '<div class="fpp-major-callout mb-2"><i class="fas fa-exclamation-triangle"></i>' +
                    '<span>No version is compatible with your FPP version/platform.</span></div>';
            body += '<div class="d-flex flex-column gap-1 small">';
            if (data.homeURL) body += '<a href="' + data.homeURL + '" target="_blank" rel="noopener noreferrer" class="text-decoration-none"><i class="fas fa-home"></i> <span class="text-decoration-underline">' + data.homeURL + '</span></a>';
            // Omit "View Source" when srcURL just duplicates the home link (same repo),
            // ignoring a trailing slash or .git suffix so github.com/x/y(.git)(/) all match.
            var sameLink = function (a, b) {
                var n = function (u) { return (u || '').replace(/\.git$/i, '').replace(/\/+$/, '').toLowerCase(); };
                return a && b && n(a) === n(b);
            };
            if (data.srcURL && !sameLink(data.srcURL, data.homeURL)) body += '<a href="' + data.srcURL + '" target="_blank" rel="noopener noreferrer" class="text-decoration-none"><i class="fas fa-code"></i> <span class="text-decoration-underline">View Source</span></a>';
            if (data.bugURL) body += '<a href="' + data.bugURL + '" target="_blank" rel="noopener noreferrer" class="text-decoration-none"><i class="fas fa-bug"></i> <span class="text-decoration-underline">Report a Bug</span></a>';
            body += '</div>';

            var buttons = {};
            if (installed) {
                if (data.pageUrl) {
                    buttons['Open'] = function () { CloseModalDialog('pluginDetailDialog'); window.open(data.pageUrl, 'pluginContent'); };
                }
                buttons['Check for Updates'] = function () { CheckPluginForUpdates(repo); };
                buttons['Uninstall'] = function () { CloseModalDialog('pluginDetailDialog'); ShowUninstallPluginPopup(repo, data.name); };
            } else if (compatibleVersion >= 0 || untestedVersion >= 0) {
                var idx = compatibleVersion < 0 ? untestedVersion : compatibleVersion;
                // Match the card's wording: "Install anyway" when the only available
                // version has not been updated for this FPP release, OR it doesn't meet
                // this device's declared minimums.
                var installLabel = (compatibleVersion < 0 && untestedVersion >= 0) || PluginResourceVerdict(data).exceeds
                    ? 'Install anyway' : 'Install';
                var installClick = function () { CloseModalDialog('pluginDetailDialog'); ConfirmAndInstall(repo, data.versions[idx].branch, data.versions[idx].sha); };
                // Match the card: doesn't meet this device's minimums wins regardless
                // of compat/untested status.
                if (PluginResourceVerdict(data).exceeds) {
                    buttons[installLabel] = { click: installClick, class: 'btn-danger' };
                } else {
                    buttons[installLabel] = installClick;
                }
            }
            buttons['Close'] = function () { CloseModalDialog('pluginDetailDialog'); };

            DoModalDialog({ id: 'pluginDetailDialog', class: 'modal-lg', title: titleIcon + data.name, body: body, backdrop: true, keyboard: true, buttons: buttons });
        }

        // Category name/icon for a plugin, validated against the loaded taxonomy so
        // only known category names reach the DOM (unknown -> "Other").
        function PluginCategoryInfo(data) {
            var repo = data.repoName || '';
            var name = data.__category
                || pluginCategoryOf[repo.toLowerCase()]
                || pluginCategoryOf[(data.name || '').toLowerCase()]
                || 'Other';
            var known = pluginCategoryByName[name];
            return { name: known ? known.name : 'Other', obj: known || OTHER_CATEGORY };
        }

        // Single source of truth for a plugin's status badges, so cards and the
        // detail modal stay consistent. includeCategory adds the category chip.
        //
        // Color budget: amber "Not updated" and red "Incompatible" are the only problem
        // colors, and purple "Official" the only provenance one (matching the Dev badge
        // on about.php); everything else is a quiet .fpp-tag. So a card carries at most
        // one warning plus one trust mark rather than five competing alerts. Previously
        // Private and "Not updated" were both amber, which left amber meaning nothing in
        // particular. Every tag keeps an icon: with the color gone the icon is what tells
        // the grey pills apart, and it keeps the two problem states from being
        // distinguished by hue alone.
        function PluginBadgesHtml(data, includeCategory) {
            var repo = data.repoName;
            var installed = PluginIsInstalled(repo);
            var sel = SelectPluginVersionIndices(data);
            var official = IsOfficialPlugin(data);
            var isPrivate = (data.private || pluginInfoUseCredentials[repo]);
            var h = '';
            if (installed)
                h += '<span class="fpp-tag gap-1 me-1"><i class="far fa-check-circle"></i> Installed</span>';
            if (official)
                h += '<span class="badge text-bg-graceful me-1" title="Official FPP plugin (maintained in the FalconChristmas GitHub organization)"><i class="fas fa-certificate"></i> Official</span>';
            if (isPrivate)
                h += '<span class="fpp-tag gap-1 me-1" title="Hosted in a private GitHub repository"><i class="fas fa-lock"></i> Private</span>';
            if (!installed && sel.compatible == -1 && sel.untested >= 0)
                h += '<span class="badge text-bg-warning me-1" title="This plugin has not been updated to work with your version of FPP. It may still install and work, but has not been confirmed for this release."><i class="fas fa-exclamation-triangle"></i> Not updated for FPP ' + getFPPMajorVersion() + '</span>';
            else if (!installed && sel.compatible == -1)
                h += '<span class="badge text-bg-danger me-1" title="No version compatible with this FPP version/platform"><i class="fas fa-ban"></i> Incompatible</span>';
            if (!installed) {
                var res = PluginResourceVerdict(data);
                if (res.badge)
                    h += '<span class="fpp-tag fpp-tag--danger gap-1 me-1 pluginResourceBadge" title="' + res.title + '"><i class="fas fa-microchip"></i> ' + res.label + '</span>';
            }
            h += PopularityBadgeHtml(PopularityOf(repo));
            if (includeCategory) {
                var cat = PluginCategoryInfo(data);
                h += '<span class="fpp-tag gap-1 me-1 pluginCatChip" title="' + (cat.obj.longName || cat.name) + '"><i class="' + cat.obj.icon + '"></i> ' + cat.name + '</span>';
            }
            return h;
        }

        function LoadPlugin(data, insert = false) {
            // Re-render: drop any existing card for this repo and refresh the cache.
            if ($('#row-' + data.repoName).length) $('#row-' + data.repoName).remove();
            var pi = FindPluginInfo(data.repoName);
            if (pi >= 0) pluginInfos[pi] = data; else pluginInfos.push(data);

            var installed = PluginIsInstalled(data.repoName);
            var versionSel = SelectPluginVersionIndices(data);
            var compatibleVersion = versionSel.compatible;
            var untestedVersion = versionSel.untested;
            var isPrivate = (data.private || pluginInfoUseCredentials[data.repoName]);
            var official = IsOfficialPlugin(data);
            var pcatName = data.__category || pluginCategoryOf[(data.repoName || '').toLowerCase()] || pluginCategoryOf[(data.name || '').toLowerCase()] || 'Other';
            var pcatObj = pluginCategoryByName[pcatName] || OTHER_CATEGORY;
            // Available grid order: usable here first, then "install anyway", then
            // won't-install. Installed cards are all rank 0 so their grid stays A-Z.
            // The uiLevel gate below means ranks 1 and 2 are often not rendered at all.
            var sortRank = installed ? 0
                : (compatibleVersion >= 0 ? 0 : (untestedVersion >= 0 ? 1 : 2));

            // Category chip on Available cards only (shown in the All view / search — see
            // FilterPlugins); installed cards live in their own tab with no category browse.
            var badges = PluginBadgesHtml(data, !installed);

            var actions = '';
            if (installed) {
                var allowUpdates = true;
                if (data.hasOwnProperty('allowUpdates'))
                    allowUpdates = data.allowUpdates ? true : false;
                if ((compatibleVersion >= 0) && data.versions[compatibleVersion].hasOwnProperty('allowUpdates'))
                    allowUpdates = data.versions[compatibleVersion].allowUpdates ? true : false;
                if (allowUpdates) {
                    actions += "<span class='updatesAvailable' style='display: none;'>";
                    actions += "<button class='btn btn-sm btn-success' onclick='event.stopPropagation();UpgradePlugin(\"" + data.repoName + "\");'><i class='far fa-arrow-alt-circle-down'></i> Update</button>";
                    actions += "</span>";
                }
                if (data.pageUrl) {
                    actions += "<a class='btn btn-sm btn-outline-primary' href='" + data.pageUrl + "' target='pluginContent'><i class='fas fa-external-link-alt'></i> Open</a>";
                }
                actions += "<button class='btn btn-sm btn-outline-danger' onclick='event.stopPropagation();ShowUninstallPluginPopup(\"" + data.repoName + "\");'><i class='far fa-trash-alt'></i> Uninstall</button>";
            } else if (compatibleVersion >= 0 || untestedVersion >= 0) {
                var idx = compatibleVersion < 0 ? untestedVersion : compatibleVersion;
                var installText = 'Install';
                var btnClass = 'btn-success';
                if (compatibleVersion < 0 && untestedVersion >= 0) {
                    installText = 'Install anyway';
                    btnClass = 'btn-warning';
                }
                // Doesn't meet this device's declared minimums wins regardless of
                // compat/untested status -- a compatible-but-underpowered plugin still
                // needs the red signal (and matching "anyway" wording), not just the
                // untested+underpowered combo.
                if (PluginResourceVerdict(data).exceeds) {
                    installText = 'Install anyway';
                    btnClass = 'btn-danger';
                }
                actions += "<button class='btn btn-sm " + btnClass + "' onclick='event.stopPropagation();ConfirmAndInstall(\"" + data.repoName + "\", \"" + data.versions[idx].branch + "\", \"" + data.versions[idx].sha + "\");'><i class='far fa-arrow-alt-circle-down'></i> " + installText + "</button>";
            }

            // Plugin icon / initials avatar
            var iconUrl = GetIconUrl(data, installed);
            var initials = GetInitials(data.name);
            var iconHtml = '';
            if (iconUrl) {
                iconHtml += '<div class="pluginIconWrap">';
                iconHtml += '<img class="pluginIcon" src="' + iconUrl + '" alt="" loading="lazy"';
                iconHtml += ' onerror="this.style.display=\'none\';this.nextElementSibling.style.display=\'flex\';">';
                iconHtml += '<div class="pluginIconFallback" style="display:none;">' + initials + '</div>';
                iconHtml += '</div>';
            } else {
                iconHtml += '<div class="pluginIconWrap"><div class="pluginIconFallback">' + initials + '</div></div>';
            }

            var html = '';
            html += '<div id="row-' + data.repoName + '" class="col pluginCard" data-category-slug="' + pcatObj.slug + '" data-sort-rank="' + sortRank + '"';
            if (manuallyLoadedPlugins[data.repoName]) html += ' data-manual="1"';
            html += '>';
            html += '<div class="card h-100 pluginCardInner" role="button" tabindex="0" onclick="ShowPluginDetail(\'' + data.repoName + '\');">';
            html += '<div class="card-body d-flex flex-column">';
            html += '<div class="d-flex align-items-start gap-3 mb-1">';
            html += iconHtml;
            html += '<div class="min-w-0 flex-grow-1">';
            html += '<h5 class="card-title pluginTitle mb-1">' + data.name + '</h5>';
			html += '<div class="pluginCardBadges">' + badges;
            if (manuallyLoadedPlugins[data.repoName]) {
                html += '<span class="fpp-tag gap-1 pluginManualBadge"><i class="fas fa-link"></i> Manual URL</span>';
            }
            html += '</div>';
            html += '</div></div>';
            var cardAuthorHtml = PluginAuthorHtml(data);
            if (cardAuthorHtml) html += '<div class="text-secondary small mb-1 pluginAuthor"><i class="fas fa-user"></i> ' + cardAuthorHtml + '</div>';
            html += '<p class="card-text pluginCardDesc small flex-grow-1">' + data.description + '</p>';
            html += '<div class="pluginCardActions d-flex flex-wrap gap-2 mt-2" onclick="event.stopPropagation();">' + actions + '</div>';
            html += '</div></div></div>';

            if (installed) {
                InsertCardSorted('installedGrid', data.name, html, sortRank);
            } else {
                var uiLevel = parseInt(settings["uiLevel"]) || 0;
                // Basic hides plugins whose declared requirements exceed this device
                // (D14). Advanced/Developer still see them, with an advisory badge and
                // an install confirmation. Coarse "heavy" profiles never hide.
                if (uiLevel < 1 && PluginResourceVerdict(data).exceeds) return;
                if (data.repoName == 'fpp-plugin-Template') {
                    if (uiLevel < 3) return;
                } else if (compatibleVersion != -1) {
                    // compatible: shown at all UI levels
                } else if (untestedVersion >= 0) {
                    if (uiLevel < 1) return;
                } else {
                    if (uiLevel < 3) return;
                }
                InsertCardSorted('pluginGrid', data.name, html, sortRank);
            }
            if (insert) {
                var el = document.getElementById('row-' + data.repoName);
                if (el) el.scrollIntoView({ behavior: 'smooth', block: 'center' });
            }
            FilterPlugins();
            ScheduleSettle();
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
                // Record the category for every entry, installed or not. Installed plugins
                // are fetched from api/plugin, which carries no category, so this map is the
                // only way PluginCategoryInfo() can resolve one for them.
                if (pluginList[i].length > 2 && pluginList[i][2])
                    pluginCategoryOf[(pluginList[i][0] || '').toLowerCase()] = pluginList[i][2];

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
                            if (data && data.repoName && PluginIsInstalled(data.repoName)) {
                                return;
                            }
                            if (pluginList[index] && pluginList[index].length > 2 && pluginList[index][2])
                                data.__category = pluginList[index][2];
                            LoadPlugin(data);
                            $('#pluginInput').on('input', function () {
                                var val = $('#pluginInput').val() || '';
                                pluginUrlError = '';
                                if (val.length > 0) {
                                    $('#pluginInput').addClass('has-text');
                                    $('#pluginClearBtn').css('display', 'block');
                                } else {
                                    $('#pluginInput').removeClass('has-text');
                                    $('#pluginClearBtn').css('display', '');
                                }
                                 if (/plugininfo\.json$/i.test(val)) {
                                    if (val !== lastAutoLoadedUrl) {
                                        if (urlLoadedRepo) {
                                            $('#row-' + urlLoadedRepo).remove();
                                            delete manuallyLoadedPlugins[urlLoadedRepo];
                                            urlLoadedRepo = null;
                                        }
                                        lastAutoLoadedUrl = val;
                                        ManualLoadInfo(true);
                                    }
                                } else if (urlLoadedRepo) {
                                    $('#row-' + urlLoadedRepo).remove();
                                    delete manuallyLoadedPlugins[urlLoadedRepo];
                                    urlLoadedRepo = null;
                                    lastAutoLoadedUrl = '';
                                }
                                FilterPlugins();
                            });
                            FilterPlugins();

                        },
                        error: function (d) {
                            $('html,body').css('cursor', 'auto');
                            if (d.statusText !== undefined) {
                                d = d.statusText;
                            }
                            // A broken/unreachable pluginInfo.json is the plugin author's
                            // problem, not the user's — fail soft (drop this one card) rather
                            // than block every visitor with an alert(). The daily CI job in
                            // fpp-data-ci is what actually catches and reports this upstream.
                            console.warn('Skipping plugin ' + pluginList[index][0] + ' (' + url + '): ' + d);
                        }
                    });
                }
            }
        }

        function ClearPluginInput() {
            $('#pluginInput').val('').removeClass('has-text');
            $('#pluginClearBtn').css('display', '');
            pluginUrlError = '';
            if (urlLoadedRepo) {
                $('#row-' + urlLoadedRepo).remove();
                delete manuallyLoadedPlugins[urlLoadedRepo];
                urlLoadedRepo = null;
            }
            lastAutoLoadedUrl = '';
            $('#pluginInput').focus();
            FilterPlugins();
        }

        function ManualLoadInfo(auto) {
            var url = $('#pluginInput').val();

            if (url.indexOf('://') > -1) {
                if (url.indexOf('https://github.com/') > -1) {
                    url = url.replace(/https:\/\/github.com\//, 'https://raw.githubusercontent.com/').replace(/\/blob\//, '/');
                }

                $('html,body').css('cursor', 'wait');

                var onSuccess = function (data, viaProxy) {
                    $('html,body').css('cursor', 'auto');
                    pluginInfoURLs[data.repoName] = url;
                    manuallyLoadedPlugins[data.repoName] = 1;
                    if (viaProxy) {
                        // Loaded via the credentialed proxy => treat as private
                        // for subsequent install/upgrade operations.
                        pluginInfoUseCredentials[data.repoName] = 1;
                    }
                    urlLoadedRepo = data.repoName;
                    LoadPlugin(data, true);
                    ShowTopTab('available');
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
                                    pluginUrlError = url;
                                    FilterPlugins();
                                    return;
                                }
                                onSuccess(data, true);
                            },
                            error: function (d) {
                                $('html,body').css('cursor', 'auto');
                                pluginUrlError = url;
                                FilterPlugins();
                            }
                        });
                    }
                });
            }
            else if (!auto) {
                alert('Invalid plugininfo.json URL');
            }
        }
        // Remembered across page loads: the progress dialogs (install/upgrade/
        // uninstall) reload the page when closed, and landing back on Available
        // after upgrading from the Updates tab loses the user's place.
        function ShowTopTab(name) {
            activeTopTab = name;
            try { sessionStorage.setItem('pluginsTopTab', name); } catch (e) { }
            $('#pluginTopTabs .nav-link').removeClass('active');
            $('#pluginTopTabs .nav-link[data-top-tab="' + name + '"]').addClass('active');
            $('#pane-available').toggleClass('d-none', name !== 'available');
            $('#pane-manage').toggleClass('d-none', name === 'available');
            $('#manageHeading').html(name === 'updates' ? '<i class="fas fa-arrow-alt-circle-up text-secondary"></i> Updates Available' : '<i class="fas fa-check-circle text-secondary"></i> Installed Plugins');
            if (name === 'updates' && !updatesCheckedOnce && installedPlugins.length > 0) {
                updatesCheckedOnce = true;
                CheckAllPluginsForUpdates();
            }
            FilterPlugins();
        }

        // Re-select the tab the user was on before the last load. Called once the
        // plugin data is in so the Updates tab can run its update check.
        function RestoreTopTab() {
            var saved = '';
            try { saved = sessionStorage.getItem('pluginsTopTab') || ''; } catch (e) { }
            if (saved === 'installed' || saved === 'updates')
                ShowTopTab(saved);
        }

        function FilterPlugins() {
            var raw = $('#pluginInput').val() || '';
            var value = raw.toLowerCase();

            var isUrlInput = /plugininfo\.json$/i.test(raw);
            var urlLoadedMode = isUrlInput && urlLoadedRepo;
            var searching = (value !== '' && !isUrlInput);
            pluginSearchActive = searching;

            $('#pluginGrid .pluginCatChip').toggleClass('d-none', !(searching || activeCategorySlug === 'all'));

            var loadedCardEl = urlLoadedMode ? document.getElementById('row-' + urlLoadedRepo) : null;

            // Available cards
            var counts = {}, total = 0, availVisible = 0;
            $('#pluginGrid').children('.pluginCard').each(function () {
                var slug = $(this).attr('data-category-slug') || 'other';
                if (urlLoadedMode) {
                    var show = this === loadedCardEl;
                    $(this).toggleClass('d-none', !show);
                    if (show) { counts[slug] = (counts[slug] || 0) + 1; total++; availVisible++; }
                } else {
                    var searchText = $('.pluginTitle', this).text().toLowerCase();
                    var authorTxt = $('.pluginAuthor', this).text().toLowerCase();
                    if (authorTxt) searchText += ' ' + authorTxt;
                    var descTxt = $('.pluginCardDesc', this).text().toLowerCase();
                    if (descTxt) searchText += ' ' + descTxt;
                    var matchesSearch = value === '' || searchText.indexOf(value) > -1;
                    var matchesCat = searching || activeCategorySlug === 'all' || slug === activeCategorySlug;
                    var show = matchesSearch && matchesCat;
                    $(this).toggleClass('d-none', !show);
                    if (show) availVisible++;
                    if (matchesSearch) { counts[slug] = (counts[slug] || 0) + 1; total++; }
                }
            });

            // Installed cards
            var installedVisible = 0, updateVisible = 0;
            $('#installedGrid').children('.pluginCard').each(function () {
                if (urlLoadedMode) {
                    $(this).addClass('d-none');
                    return;
                }
                var searchText = $('.pluginTitle', this).text().toLowerCase();
                var authorTxt = $('.pluginAuthor', this).text().toLowerCase();
                if (authorTxt) searchText += ' ' + authorTxt;
                var descTxt = $('.pluginCardDesc', this).text().toLowerCase();
                if (descTxt) searchText += ' ' + descTxt;
                var matchesSearch = value === '' || searchText.indexOf(value) > -1;
                var hasUpdate = $(this).hasClass('fppHasUpdate');
                if (hasUpdate) updateVisible++;
                var matchesTab = (activeTopTab !== 'updates') || hasUpdate;
                var vis = matchesSearch && matchesTab;
                $(this).toggleClass('d-none', !vis);
                if (vis) installedVisible++;
            });
            if (activeTopTab === 'updates') $('#noUpdatesHint').toggleClass('d-none', installedVisible > 0);
            else $('#noUpdatesHint').addClass('d-none');

            $('#manageHeading').toggleClass('invisible', activeTopTab === 'updates' && updateVisible === 0);

            var installedTotal = $('#installedGrid').children('.pluginCard').length;
            var hasUrlScheme = /^https?:\/\//i.test(raw);
            var hasUrlSchemeError = isUrlInput && !hasUrlScheme;
            var hasUrlError = pluginUrlError && raw === pluginUrlError;
            if (hasUrlSchemeError) {
                $('#noAvailableResults').addClass('d-none');
                $('#noUrlResults').addClass('d-none');
                $('#noUrlSchemeResults').removeClass('d-none');
            } else if (hasUrlError) {
                $('#noAvailableResults').addClass('d-none');
                $('#noUrlResults').addClass('d-none');
                $('#noUrlSchemeResults').addClass('d-none');
            } else {
                var showAvailEmpty = searching && activeTopTab === 'available' && availVisible === 0;
                $('#noAvailableResults').toggleClass('d-none', !showAvailEmpty);
                $('#noUrlResults').addClass('d-none');
                $('#noUrlSchemeResults').addClass('d-none');
                if (showAvailEmpty && installedVisible > 0) {
                    $('#noAvailCrossRef').text('Found ' + installedVisible + ' plugin' + (installedVisible === 1 ? '' : 's') + ' that match on the Installed list. ');
                } else {
                    $('#noAvailCrossRef').text('');
                }
            }
            var showInstalledEmpty = searching && activeTopTab === 'installed' && installedVisible === 0 && installedTotal > 0;
            $('#noInstalledResults').toggleClass('d-none', !showInstalledEmpty);
            if (showInstalledEmpty && availVisible > 0) {
                $('#noInstalledCrossRef').text('Found ' + availVisible + ' plugin' + (availVisible === 1 ? '' : 's') + ' that match on the Available list.');
            } else {
                $('#noInstalledCrossRef').text('');
            }
            $('.fppNoResultsTerm').text(raw);
            $('.fppUrlErrorTerm').text(raw);
            $('.fppUrlSchemeErrorTerm').text(raw);

            $('#pluginCategoryPills .fppCatCount').each(function () {
                var s = $(this).attr('data-count-slug');
                var val = (s === 'all') ? total : (counts[s] || 0);
                $(this).text(val);
                var $li = $(this).closest('.nav-item');
                if (s !== 'all' && val === 0) $li.addClass('d-none'); else $li.removeClass('d-none');
            });

            if (isUrlInput) {
                $('#popularStripWrap').addClass('d-none');
            } else {
                UpdatePopularStripVisibility();
            }

            $('#topCountAvailable').text(availVisible);
            $('#topCountInstalled').text(installedVisible);
            $('#topCountUpdates').text(updateVisible);
        }
        $(document).ready(function () {
            // Firefox restores input values on reload regardless of autocomplete="off",
            // which would leave the list filtered by a term the user can't see a reason for.
            $('#pluginInput').val('');

            // Uninstall All and Reinstall All are bulk destructive actions, so only
            // expose them in Advanced UI mode or higher.
            if (settings["uiLevel"] > 0) {
                $('#updateAllBtn').removeClass('d-none');
                $('#uninstallAllBtn').removeClass('d-none');
                $('#reinstallAllBtn').removeClass('d-none');
            }
            $('#pluginTopTabs .nav-link').on('click', function () {
                ShowTopTab($(this).attr('data-top-tab'));
                this.scrollIntoView({ block: 'nearest', inline: 'center' });
            });
            $('#pluginClearBtn').on('click', ClearPluginInput);
            BindPopularStripControls();
            GetPluginPopularity();   // parallel with the list/installed loads (Phase 2)
            GetInstalledPlugins();

        });
    </script>
    <style>
        /* Round the top corners of the Available/Installed/Updates tabs to match
           the 12px border-radius used by the category pills (.nav-pills).  The
           category pills get 12px from --bs-nav-pills-border-radius; the top tabs
           use Bootstrap's default --bs-nav-tabs-border-radius (~0.375rem).  We
           override just the top corners here so both tab strips share the same
           visual radius. */
        #pluginTopTabs .nav-link {
            border-top-left-radius: 12px;
            border-top-right-radius: 12px;
        }

        /* Thicken the bottom border of the top tab bar to make the separator
           between the tabs and the category pills more visually distinct.
           Bootstrap's .nav-tabs default is 1px; bumping to 3px gives a
           visible line without overwhelming the layout. */
        #pluginTopTabs {
            border-bottom-width: 3px;
        }

        /* On mobile portrait (<576px), move the "Installed Plugins" heading
           below the action buttons instead of sitting beside them.
           flex-direction: column-reverse puts the heading (first child) at the
           bottom and the button group (second child) at the top. */
        @media (max-width: 575.98px) {
            .pluginsHeader {
                flex-direction: column-reverse;
                align-items: flex-start;
                gap: 0.5rem;
            }
        }
    </style>
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

                    <div class='plugindiv'>
                        <!-- Desktop: tabs on the left, find box on the right of the same row.
                             align-items-lg-center vertically centres the find box against the
                             (taller) tab row so the input sits level with the tab labels instead
                             of floating high. Mobile (<lg): stacks with the find box on top. -->
                        <div class="row align-items-lg-center g-2 mb-3">
                            <div id='pluginTableHead' class="col-12 col-lg-4 order-lg-2 d-lg-flex">
                                <div class="row fppPluginInput gx-2 flex-grow-1 align-items-center">
                                    <div class="col d-flex position-relative">
                                        <input type="text" id="pluginInput" autocomplete="off"
                                            class="form-control form-control-rounded has-shadow flex-grow-1"
                                            placeholder="Find a Plugin or Enter a pluginInfo.json URL" />
                                        <i id="pluginClearBtn" class="fas fa-times-circle pluginClearBtn"
                                            title="Clear search"></i>
                                    </div>
                                </div>
                            </div>
                            <div class="col-12 col-lg order-lg-1">
                                <!-- overflow-x-auto, not overflow-auto: .nav-tabs .nav-link carries
                                     margin-bottom:-1px to sit over the nav's border, which overflows
                                     the box vertically by exactly 1px. Against overflow-y:auto that
                                     is enough for Chrome to draw a full vertical scrollbar next to
                                     the tabs. Horizontal scroll is still needed -- the tabs do not
                                     fit on a phone. (The old overflow-md-visible here was a no-op:
                                     Bootstrap 5.3 ships no responsive overflow utilities.) -->
                                <ul class="nav nav-tabs flex-nowrap flex-md-wrap overflow-x-auto overflow-y-hidden" id="pluginTopTabs" role="tablist">
                                    <li class="nav-item" role="presentation">
                                        <button type="button" class="nav-link active text-nowrap" data-top-tab="available" role="tab">
                                            <i class="fas fa-store"></i> Available
                                            <span class="badge bg-secondary ms-1" id="topCountAvailable">0</span>
                                        </button>
                                    </li>
                                    <li class="nav-item" role="presentation">
                                        <button type="button" class="nav-link text-nowrap" data-top-tab="installed" role="tab">
                                            <i class="far fa-check-circle"></i> Installed
                                            <span class="badge bg-secondary ms-1" id="topCountInstalled">0</span>
                                        </button>
                                    </li>
                                    <li class="nav-item" role="presentation">
                                        <button type="button" class="nav-link text-nowrap" data-top-tab="updates" role="tab">
                                            <i class="far fa-arrow-alt-circle-up"></i> Updates
                                            <span class="badge bg-secondary ms-1" id="topCountUpdates">0</span>
                                        </button>
                                    </li>
                                </ul>
                            </div>
                        </div>

                        <div id="pane-available" class="pluginTopPane">
                            <div class="fppPluginAvailableHead">
                                <h2 class="h5 mb-2"><i class="fas fa-tags text-secondary"></i> Categories</h2>
                                <ul class="nav nav-pills mb-3 pageContent-tabs flex-nowrap flex-md-wrap overflow-x-auto gap-1 pb-1" id="pluginCategoryPills" role="tablist"></ul>
                            </div>

                            <!-- Lives inside pane-available, below the pills that drive it: the strip
                                 follows the active category, and the pane's own d-none keeps it off the
                                 Installed/Updates tabs where no category pills exist to explain it. -->
                            <div id="popularStripWrap" class="mb-3 d-none">
                                <div class="d-flex align-items-center justify-content-between mb-2">
                                    <h2 class="h5 mb-0"><i class="fas fa-fire text-secondary"></i> <span id="popularStripHeading">Popular Plugins</span></h2>
                                    <!-- Shown by UpdatePopularScrollState() only when the strip actually
                                         overflows; the edge fade alone can't be clicked or tabbed to. -->
                                    <div class="btn-group btn-group-sm d-none" id="popularStripNav" role="group"
                                        aria-label="Scroll popular plugins">
                                        <button type="button" class="btn btn-outline-secondary" id="popularStripPrev"
                                            aria-label="Scroll popular plugins left" aria-controls="popularStrip">
                                            <i class="fas fa-chevron-left" aria-hidden="true"></i>
                                        </button>
                                        <button type="button" class="btn btn-outline-secondary" id="popularStripNext"
                                            aria-label="Scroll popular plugins right" aria-controls="popularStrip">
                                            <i class="fas fa-chevron-right" aria-hidden="true"></i>
                                        </button>
                                    </div>
                                </div>
                                <div id="popularStrip" class="d-flex flex-nowrap overflow-auto gap-2 pb-2 pluginPopularScroll"
                                    tabindex="0" role="group" aria-label="Popular plugins"></div>
                            </div>

                            <div id='pluginTable'>
                                <h2 class="h5 mb-2"><i class="fas fa-box text-secondary"></i> Available Plugins</h2>
                                <div id='pluginGrid' class="row row-cols-1 row-cols-md-2 row-cols-xxl-3 g-3"></div>
                                <div id="noAvailableResults" class="alert alert-info d-none mt-2">
                                    <i class="fas fa-search"></i> No plugins match
                                    "<b class="fppNoResultsTerm"></b>". <span id="noAvailCrossRef"></span>Clear the search box to see all plugins.
                                </div>
                                <div id="noUrlResults" class="alert alert-info d-none mt-2">
                                    <i class="fas fa-exclamation-triangle"></i> No valid plugins found on JSON URL:
                                    "<b class="fppUrlErrorTerm"></b>". Clear the search box to see all plugins.
                                </div>
                                <div id="noUrlSchemeResults" class="alert alert-warning d-none mt-2">
                                    <i class="fas fa-exclamation-circle"></i> URL:
                                    "<b class="fppUrlSchemeErrorTerm"></b>" must contain http:// or https://.
                                    Clear the search box to see all plugins.
                                </div>
                            </div>
                        </div>

                        <div id="pane-manage" class="pluginTopPane d-none">
                            <div class='pluginsHeader'>
                                <h2 id="manageHeading"><i class="fas fa-check-circle text-secondary"></i> Installed Plugins</h2>
                                <div class="d-flex flex-wrap gap-2 align-items-center">
                                    <button id="checkAllUpdatesBtn" class="buttons btn-outline-success"
                                        onClick='CheckAllPluginsForUpdates();'
                                        title="Check all installed plugins for updates">
                                        <i class='fas fa-sync-alt'></i> Check All for Updates
                                    </button>
                                    <button id="updateAllBtn" class="buttons btn-outline-primary d-none"
                                        onClick='UpdateAllPlugins();'
                                        title="Check for and update all installed plugins that have an update available">
                                        <i class='far fa-arrow-alt-circle-down'></i> Update All
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
                            <div id='installedPlugins'>
                                <div id='installedGrid' class="row row-cols-1 row-cols-md-2 row-cols-xxl-3 g-3"></div>
                            </div>
                            <div id="noInstalledResults" class="alert alert-info d-none mt-2">
                                <i class="fas fa-search"></i> No installed plugins match
                                "<b class="fppNoResultsTerm"></b>". <span id="noInstalledCrossRef"></span>
                            </div>
                            <div id="noUpdatesHint" class="text-secondary d-none">No updates found. Use <b>Check All for Updates</b> to refresh.</div>
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
