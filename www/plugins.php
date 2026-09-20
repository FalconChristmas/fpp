<!DOCTYPE html>
<html lang="en">

<head>
    <?php
    include 'common/htmlMeta.inc';
    require_once('config.php');
    require_once('common.php');

    // Device capability for the plugin resource-hint check. Computed
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
    <!-- The privacy lights: colour rules and markup for the six chips, kept in
         their own file so the rule table can be read on its own. -->
    <script src="js/fpp-privacy-lights.js?ref=<?= filemtime('js/fpp-privacy-lights.js'); ?>"></script>
    <script>
        var installedPlugins = [];
        var pluginInfos = [];
        var pluginInfoURLs = [];
        var pluginInfoUseCredentials = {};
        var manuallyLoadedPlugins = {};
        // pluginInfo.json repoName -> the name that entry is listed under in
        // pluginList.json, when they differ. See LoadPlugins() and GetIconUrl().
        var pluginListKeyOf = {};
        var lastAutoLoadedUrl = '';
        var urlLoadedRepo = null;
        var pluginUrlError = '';
        // --- Plugin categories ---
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
        // --- Plugin popularity ---
        // Install counts keyed by repoName (== row id). The device does NOT fetch the
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

        // Device capability for the resource-hint check, injected server-side
        // (exact for this box). 0 means "unknown" -> the check degrades to no-op.
        var DEVICE_MEM_MB = <?php echo $pluginDeviceMemMB; ?>;
        var DEVICE_CORES  = <?php echo $pluginDeviceCores; ?>;

        // uiLevel is set server-side by config.php; injected once here (same pattern
        // as multisync.php) so the JS side never has to re-parse settings['uiLevel'].
        var uiLevel = <?php echo (int) $uiLevel; ?>;

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

        // --- Plugin privacy lights ---
        // The six chips (Sends data, Collects data, Camera & mic, Remote access,
        // System changes, Can it be checked?) come from the plugin's own `privacy`
        // block, coloured by FPP per the table in js/fpp-privacy-lights.js. Every
        // install dialog shows them, every card carries them as dots, and the
        // detail and privacy modals show them in full. A plugin loaded from a
        // pasted URL was never checked against its code for listing; its label
        // line says so.
        var pluginPrivacyChanged = {};   // repoName -> true once an update check said the disclosure changed (what Update would land)
        var pluginReinstallPrivacyChanged = {};   // same for what Reinstall would land (the versions[] branch/pin for this FPP)
        var pluginReinstallTarget = {};   // repoName -> {branch, sha} a Reinstall clones: the server's versions[] choice, posted back as is

        function PluginPrivacyResult(data) {
            return FPPPluginPrivacy.evaluate(data ? data.privacy : null,
                { unreviewed: !!(data && manuallyLoadedPlugins[data.repoName]) });
        }

        // Card action row: the six lights as dots, in the trailing ms-auto cluster
        // with the GitHub stats badge (same placement as GitHubStatsRowHtml).
        // Clicking the card opens the detail modal with the lights in full.
        function PrivacyRowHtml(data) {
            return '<span class="ms-auto d-inline-flex align-items-center small">' +
                FPPPluginPrivacy.stripHtml(PluginPrivacyResult(data), { compact: true }) + '</span>';
        }

        // Label line, the author's summary, FPP's headline, the strip and its
        // lines, then the author's `other` text; the block as written is
        // behind "Full disclosure" in Developer UI mode only. Shared by the
        // install dialog, the upgrade dialog and the detail modal. Wording per
        // fpp-plugin-Template PLUGIN_GUIDELINES.md §14.15.
        // from: the result of the block the operator accepted earlier, for the
        // changed-disclosure dialog -- lights that differ open as "Was / Now"
        // with their changed lines marked, and a list under the headline says
        // which lights moved and which way (fpp-privacy-lights.js).
        function PrivacyBlockHtml(r, prefix, from) {
            var h = r.declared
                ? '<div class="small text-secondary mb-1"><span class="fw-bold text-uppercase">Disclosed by the author</span> &middot; not verified by FPP</div>'
                : '<div class="small text-secondary mb-1"><span class="fw-bold text-uppercase">No disclosure</span> &middot; the author has not said what this plugin does with data</div>';
            if (r.unreviewed && r.declared)
                h += '<div class="small text-secondary mb-1">Loaded from a URL, not from the plugin list: this disclosure was not reviewed for the plugin list.</div>';
            h += FPPPluginPrivacy.summaryHtml(r);
            // The headline of a declared block is its worst chip's text
            // again, and the open red line under that says it a third time:
            // only an undeclared block needs it, to say why the row is grey.
            if (!r.declared) h += FPPPluginPrivacy.headlineHtml(r);
            if (from) {
                var changes = FPPPluginPrivacy.changesSummaryHtml(from, r);
                // `other` is material too (support access, payments, self-
                // update live there): a change to it alone re-prompts, so it
                // gets its own was / now line. result.other is already
                // trimmed with "none" read as empty, as the server compares it.
                if (r.declared && from.declared && (from.other || '') !== (r.other || '')) {
                    var otherLine = '<li class="mb-1"><b>Other:</b> ' +
                        (from.other ? 'was <s class="text-secondary">' + EscapeHtml(from.other) + '</s>, ' : 'was not disclosed, ') +
                        (r.other ? 'now <span class="fw-semibold">' + EscapeHtml(r.other) + '</span>' : 'now nothing') + '</li>';
                    changes = changes ? changes.replace(/<\/ul>\s*$/, otherLine + '</ul>') : '<ul class="small mb-2 mt-2 ps-3">' + otherLine + '</ul>';
                }
                if (changes) h += '<div class="small fw-semibold mt-2">What changed</div>' + changes;
            }
            h += FPPPluginPrivacy.stripHtml(r, { prefix: prefix, from: from || null });
            h += FPPPluginPrivacy.detailsHtml(r, { raw: uiLevel >= 3 });
            return h;
        }

        // The callout no declaration can change. Official (FalconChristmas org)
        // plugins get the same slot with a quieter message; everything else gets
        // the danger colour and the plugin's verifiable source owner by name.
        // Bootstrap subtle/emphasis tokens rather than .alert-danger: FPP's
        // Bootstrap build repaints .alert-danger solid red with white text
        // (fpp-bootstrap-5-3.css), which the author link cannot sit on.
        function PluginTrustHtml(data) {
            if (data && IsOfficialPlugin(data))
                return '<div class="d-flex gap-2 align-items-start p-2 mb-2 rounded border bg-success-subtle border-success-subtle text-success-emphasis"><i class="fas fa-circle-check mt-1"></i><div>' +
                    '<p class="fw-bold mb-1">Maintained by the FPP project &mdash; but it still runs as root.</p>' +
                    '<p class="mb-0">It can read and change any setting, including the privacy settings, and reach anything else on the network FPP is connected to.</p></div></div>';
            var author = PluginAuthorHtml(data) || EscapeHtml((data && data.author) ? data.author : 'the author');
            return '<div class="d-flex gap-2 align-items-start p-2 mb-2 rounded border bg-danger-subtle border-danger-subtle text-danger-emphasis"><i class="fas fa-triangle-exclamation mt-1"></i><div>' +
                '<p class="fw-bold mb-1">Warning: this plugin is untrusted third-party code that will run on your FPP as root.</p>' +
                '<p class="mb-0">It can read and change any setting, including the privacy settings, reach anything else on the ' +
                'network FPP is connected to, and do anything at all once installed &mdash; including things not listed below. ' +
                'This is inherently dangerous. The FPP project does not test, vet, or guarantee the quality or safety of plugins. ' +
                'Install at your own risk, and only if you trust <b>' + author + '</b>. Plugins marked ' +
                '<span class="badge text-bg-graceful"><i class="fas fa-certificate"></i> Official</span> are maintained by the FPP team; this one is not.</p>' +
                '</div></div>';
        }

        // The reminder for a plugin that is already installed: the operator
        // saw PluginTrustHtml when they installed it, so an update or reinstall
        // dialog says it in one line and leaves the room to the disclosure.
        function PluginTrustLineHtml(data) {
            return '<div class="small text-secondary mb-2"><i class="fas fa-triangle-exclamation"></i> ' +
                (data && IsOfficialPlugin(data) ? 'Maintained by the FPP project, but it still runs as root.' :
                    'Third-party code that runs as root: it can do anything on this player, including things not listed below.') + '</div>';
        }

        // The first screen of a Reinstall All / Update All that has more than
        // one disclosure to ask about: what is about to happen, the root
        // warning once, and the plugins in the order they will be asked. Each
        // plugin's own screen then carries only its disclosure.
        // opts: title, lead (what accept/decline do), plugins, button,
        // onStart, onCancel (omit for a review that must be walked).
        function ReviewIntroDialog(opts) {
            var id = 'privacyReviewIntroDialog';
            var body = '<p>' + opts.lead + '</p>';
            body += '<div class="d-flex gap-2 align-items-start p-2 mb-2 rounded border bg-danger-subtle border-danger-subtle text-danger-emphasis"><i class="fas fa-triangle-exclamation mt-1"></i><div>' +
                '<p class="fw-bold mb-1">Plugins are third-party code that runs on your FPP as root.</p>' +
                '<p class="mb-0">A plugin can read and change any setting, including the privacy settings, reach anything else on the network FPP is connected to, ' +
                'and do anything at all once installed &mdash; including things its disclosure does not list. The FPP project does not test, vet, or ' +
                'guarantee plugins; what follows is each author\'s own statement, not verified by FPP.</p></div></div>';
            body += '<div class="fw-semibold">You will be asked about, in this order:</div><ol class="mb-0">';
            opts.plugins.forEach(function (p) {
                var k = FindPluginInfo(p);
                body += '<li>' + EscapeHtml((k >= 0 && pluginInfos[k].name) ? pluginInfos[k].name : p) + '</li>';
            });
            body += '</ol>';
            var started = false;
            var buttons = {};
            buttons[opts.button] = { class: 'btn-primary', click: function () { started = true; CloseModalDialog(id); } };
            if (opts.onCancel) buttons['Cancel'] = function () { CloseModalDialog(id); };
            DoModalDialog({
                id: id,
                class: 'modal-lg',
                title: opts.title,
                body: body,
                backdrop: opts.onCancel ? true : 'static',
                keyboard: !!opts.onCancel,
                noClose: !opts.onCancel,
                buttons: buttons
            });
            $('#' + id).find('#modalCloseButton').prop('disabled', !opts.onCancel);
            $('#' + id).one('hidden.bs.modal', function () {
                if (started) opts.onStart();
                else if (opts.onCancel) opts.onCancel();
            });
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
                    // Everything is rendered and interactive; refresh the Updates
                    // tab count for real, quietly, in the background.
                    BackgroundCheckForUpdates();
                },
                error: function () {
                    alert('Error, failed to get pluginList.json');
                }
            });
        }

        // Build category lookup maps + pills from pluginCategories.json.
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
        // Parse the URL (host + first path segment) so a spoofed host/path can't earn it.
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
        // self-supplied `author` field, which nobody verifies. Returns the repo
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

        // Double-quote-attribute-safe escaping for untrusted values (plugin-supplied
        // URLs) going into href="...". EscapeHtml (shared, js/fpp.js) is only safe
        // for text content -- it does not escape '"', so an untrusted string placed
        // straight into a double-quoted attribute could close the attribute early
        // and inject a new one (e.g. onmouseover=...). pluginInfo.json fields are
        // third-party-supplied and not verified (see PluginAuthorHtml above).
        function EscapeAttr(s) {
            return EscapeHtml(s).replace(/"/g, '&quot;');
        }

        // Plugin-declared URLs (pageUrl, etc.) that get navigated to (href, window.open)
        // rather than just displayed need a scheme check, not just HTML/attribute
        // escaping -- EscapeAttr stops attribute breakout but a well-formed
        // "javascript:..." URL runs on click regardless of how cleanly it's escaped.
        function IsSafeHttpUrl(u) {
            if (!u) return false;
            try {
                var parsed = new URL(u, window.location.href);
                return parsed.protocol === 'http:' || parsed.protocol === 'https:';
            } catch (e) {
                return false;
            }
        }

        // Cache-busting nonce shared by all icon URLs loaded during this page
        // session, so a plugin update with a new icon is visible immediately
        // rather than showing a stale cached copy. The server sends 304 Not
        // Modified when the icon file is unchanged, so the overhead is minimal.
        var iconCacheNonce = Date.now();

        // Get the plugin icon URL. Always routes through the same-origin
        // api/plugin/:RepoName/icon, keyed purely by repo name -- both to avoid
        // CSP restrictions on external image hosts (raw.githubusercontent.com is
        // only allow-listed in connect-src, not img-src) and because the server
        // resolves the actual image source itself (local icon.png, or the
        // listed pluginInfo.json) rather than us handing it a URL to fetch.
        // The hasIcon/iconURL checks below are only hints that let us skip a
        // request we already know will 404.
        function GetIconUrl(data, installed) {
            var name = data.repoName;
            if (installed) {
                if (data.hasOwnProperty('hasIcon') && !data.hasIcon) return null;
            } else {
                if (!data.iconURL) return null;
                // Not installed, so the server has to find this one in the
                // plugin list -- ask under the name the list actually files it
                // under, which is not always the plugin's own repoName.
                name = pluginListKeyOf[name] || name;
            }
            return 'api/plugin/' + name + '/icon?_=' + iconCacheNonce;
        }

        function BuildCategoryPills() {
            var $pills = $('#pluginCategoryPills');
            if (!$pills.length) return;
            $pills.empty();
            var pills = [];
            // "All" view shown at every UI level and is the default landing view.
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

        // Busy state for the "Check for Update" button in the plugin detail
        // modal: disabled with a spinning wheel / "Checking for Updates" label
        // while the per-plugin check runs, mirroring SetCheckForUpdatesBusy on the
        // Updates tab. No-ops when the button doesn't exist (dialog closed).
        function SetDetailCheckBusy(plugin, busy) {
            var $btn = $('#pluginDetailCheckBtn');
            if (!$btn.length) return;
            if (busy) {
                $btn.prop('disabled', true);
                $btn.html('<i class="fas fa-spinner fa-spin me-1"></i>Checking for Updates');
            } else {
                $btn.prop('disabled', false);
                $btn.html('Check for Update');
            }
        }

        function CheckPluginForUpdates(plugin) {
            var url = 'api/plugin/' + plugin + '/updates';

            $('html,body').css('cursor', 'wait');
            SetDetailCheckBusy(plugin, true);
            $.ajax({
                url: url,
                type: 'POST',
                dataType: 'json',
                success: function (data) {
                    $('html,body').css('cursor', 'auto');
                    if (data.Status == 'OK') {
                        pluginPrivacyChanged[plugin] = !!data.privacyChanged;
                        pluginReinstallPrivacyChanged[plugin] = !!data.reinstallPrivacyChanged;
                        if (data.reinstallTarget) pluginReinstallTarget[plugin] = data.reinstallTarget;
                        if (data.updatesAvailable) {
                            RowEl(plugin).addClass('fppHasUpdate').find('.updatesAvailable').removeClass('d-none');
                            var $modal = $('#pluginDetailDialog');
                            if ($modal.length && $modal.is(':visible')) {
                                var $checkBtn = $modal.find('#pluginDetailCheckBtn');
                                if ($checkBtn.length) {
                                    // A real closure over `plugin` here, rather than an
                                    // onclick="Fn('...')" string built from it -- no HTML/JS
                                    // escaping needed at all since the value never round-trips
                                    // through markup.
                                    var $updateBtn = $('<button class="btn btn-success"><i class="far fa-arrow-alt-circle-down"></i> Update</button>');
                                    $updateBtn.on('click', function () {
                                        CloseModalDialog('pluginDetailDialog');
                                        UpgradePlugin(plugin);
                                    });
                                    $checkBtn.replaceWith($updateBtn);
                                }
                            }
                        } else {
                            RowEl(plugin).removeClass('fppHasUpdate');
                            SetDetailCheckBusy(plugin, false);
                            $.jGrowl('No updates available for ' + plugin, { themeState: 'detract' });
                        }
                        FilterPlugins();
                    }
                    else {
                        SetDetailCheckBusy(plugin, false);
                        alert('ERROR: ' + data.Message);
                    }
                },
                error: function () {
                    $('html,body').css('cursor', 'auto');
                    SetDetailCheckBusy(plugin, false);
                    alert('Error, API call failed when checking plugin for updates');
                }
            });
        }

        // Shared by CheckAllPluginsForUpdates, UpdateAllPlugins's pre-check, and
        // UpdateAllFinish's post-upgrade recheck: POSTs api/plugin/<name>/updates
        // for every plugin in pluginList in parallel, and calls onComplete once
        // every request has settled (success or error). onResult(plugin, hasUpdate)
        // fires per successful check (Status OK) so callers can drive their own row UI;
        // onComplete(withUpdates, anyError) fires once with the aggregate result.
        function CheckPluginsForUpdates(pluginList, onResult, onComplete) {
            var checked = 0;
            var total = pluginList.length;
            if (total === 0) {
                onComplete([], false);
                return;
            }
            var withUpdates = [];
            var anyError = false;
            pluginList.forEach(function (plugin) {
                // The listing's clone URL rides along: when the installed
                // clone's own origin can no longer be fetched (branch renamed
                // or deleted, repo moved) the server tries the reinstall
                // target there instead, so Reinstall can still re-clone it.
                var body = {};
                var pi = FindPluginInfo(plugin);
                if (pi >= 0 && pluginInfos[pi].srcURL) {
                    body.srcURL = pluginInfos[pi].srcURL;
                    body.useCredentials = (pluginInfos[pi].private || pluginInfoUseCredentials[plugin]) ? 1 : 0;
                }
                $.ajax({
                    url: 'api/plugin/' + plugin + '/updates',
                    type: 'POST',
                    contentType: 'application/json',
                    data: JSON.stringify(body),
                    dataType: 'json',
                    success: function (data) {
                        // A failed fetch comes back as Status:Error with HTTP 200;
                        // that is an error for onResult's purposes too (Reinstall
                        // relies on "no result" meaning "not checked").
                        if (data.Status != 'OK') {
                            anyError = true;
                            return;
                        }
                        if (data.originUnreachable) {
                            // Checked, but only the reinstall target answered:
                            // an upgrade is impossible, a Reinstall is the fix.
                            $.jGrowl(EscapeHtml(plugin) + ': the installed copy can no longer fetch from where it was cloned. Update cannot run; use Reinstall to clone it afresh.', { themeState: 'warn', sticky: true });
                        }
                        var hasUpdate = !!data.updatesAvailable;
                        pluginPrivacyChanged[plugin] = !!data.privacyChanged;
                        pluginReinstallPrivacyChanged[plugin] = !!data.reinstallPrivacyChanged;
                        if (data.reinstallTarget) pluginReinstallTarget[plugin] = data.reinstallTarget;
                        if (hasUpdate) withUpdates.push(plugin);
                        onResult(plugin, hasUpdate);
                    },
                    error: function () {
                        anyError = true;
                    },
                    complete: function () {
                        checked++;
                        if (checked === total) onComplete(withUpdates, anyError);
                    }
                });
            });
        }

        // Quiet background pass over the installed plugins so the Updates tab
        // count reflects a real (fetch-based) check without anyone pressing
        // "Check for Updates" -- the page-load render only knows about commits
        // that some earlier fetch already pulled down, so the tab's [0] was
        // asserting "no updates" from stale information.
        //
        // Deliberately different from CheckPluginsForUpdates: one request at a
        // time, not a parallel fan-out. Each check runs a git fetch server-side,
        // and N of those at once would occupy the PHP worker pool exactly when
        // the user might be clicking Install. No cursor, no button locking, no
        // growls -- rows and the tab count just correct themselves as answers
        // arrive. The explicit Check/Update All buttons cancel the sweep (they
        // are about to redo the same work in parallel anyway); a failed check
        // is skipped silently since offline boxes hit this on every page load.
        var bgUpdateSweep = null; // non-null while a background sweep is running

        function CancelBackgroundUpdateCheck() {
            if (bgUpdateSweep) {
                bgUpdateSweep.cancelled = true;
                bgUpdateSweep = null;
            }
        }

        function BackgroundCheckForUpdates() {
            if (bgUpdateSweep || installedPlugins.length === 0)
                return;
            var sweep = { cancelled: false };
            bgUpdateSweep = sweep;
            var queue = installedPlugins.slice();

            function finish() {
                if (bgUpdateSweep === sweep)
                    bgUpdateSweep = null;
            }
            function next() {
                if (sweep.cancelled || queue.length === 0) {
                    finish();
                    return;
                }
                var plugin = queue.shift();
                $.ajax({
                    url: 'api/plugin/' + plugin + '/updates',
                    type: 'POST',
                    dataType: 'json',
                    success: function (data) {
                        if (!sweep.cancelled && data.Status == 'OK') {
                            pluginPrivacyChanged[plugin] = !!data.privacyChanged;
                            pluginReinstallPrivacyChanged[plugin] = !!data.reinstallPrivacyChanged;
                            if (data.reinstallTarget) pluginReinstallTarget[plugin] = data.reinstallTarget;
                            if (data.updatesAvailable)
                                RowEl(plugin).addClass('fppHasUpdate').find('.updatesAvailable').removeClass('d-none');
                            else
                                RowEl(plugin).removeClass('fppHasUpdate');
                            FilterPlugins();
                        }
                        next();
                    },
                    error: function () {
                        next();
                    }
                });
            }
            next();
        }

        // Toggle the "Check for Updates" button between its idle and busy states.
        // Busy: disabled with a spinning wheel and "Checking for Updates" label.
        // Idle: enabled with the sync icon and "Check for Updates" label.
        function SetCheckForUpdatesBusy(busy) {
            var $btn = $('#checkAllUpdatesBtn');
            if (busy) {
                $btn.prop('disabled', true);
                $btn.html('<i class="fas fa-spinner fa-spin me-1"></i>Checking for Updates');
            } else {
                $btn.prop('disabled', false);
                $btn.html('<i class="fas fa-sync-alt"></i> Check for Updates');
            }
        }

        function CheckAllPluginsForUpdates() {
            if (installedPlugins.length === 0) {
                $.jGrowl('No plugins installed', { themeState: 'detract' });
                return;
            }
            CancelBackgroundUpdateCheck();

            $('html,body').css('cursor', 'wait');
            SetCheckForUpdatesBusy(true);

            CheckPluginsForUpdates(installedPlugins, function (plugin, hasUpdate) {
                if (hasUpdate) {
                    RowEl(plugin).addClass('fppHasUpdate').find('.updatesAvailable').removeClass('d-none');
                }
            }, function (withUpdates, anyError) {
                $('html,body').css('cursor', 'auto');
                SetCheckForUpdatesBusy(false);
                if (anyError) {
                    $.jGrowl('Completed checking plugins (some checks failed)', { themeState: 'warn' });
                } else if (withUpdates.length > 0) {
                    $.jGrowl('Found updates for ' + withUpdates.length + ' plugin(s)', { themeState: 'success' });
                } else {
                    $.jGrowl('All plugins are up to date', { themeState: 'success' });
                }
                FilterPlugins();
            });
        }

        // Update All: (re)check every installed plugin for updates, then upgrade
        // each one that has an update available, sequentially, in a single progress
        // dialog. Cheaper and safer than Reinstall All -- it only touches plugins
        // with a pending update and never uninstalls anything (no removal window),
        // so a shared dependency can't be dropped mid-flight. Mirrors the Reinstall
        // All queue + progress-dialog + verify-by-recheck pattern.
        var updateAllAttempted = [];

        // Entry point (toolbar button). Runs a fresh update check across all
        // installed plugins first so the user does not have to click "Check All for
        // Updates" beforehand, then confirms and upgrades those with updates.
        function UpdateAllPlugins() {
            if (installedPlugins.length === 0) {
                $.jGrowl('No plugins installed', { themeState: 'detract' });
                return;
            }
            CancelBackgroundUpdateCheck();
            $('html,body').css('cursor', 'wait');
            $('#updateAllBtn').prop('disabled', true);
            SetCheckForUpdatesBusy(true);

            CheckPluginsForUpdates(installedPlugins, function (plugin, hasUpdate) {
                if (hasUpdate) {
                    RowEl(plugin).addClass('fppHasUpdate').find('.updatesAvailable').removeClass('d-none');
                }
            }, function (withUpdates, anyError) {
                UpdateAllChecksDone(withUpdates, anyError);
            });
        }

        function UpdateAllChecksDone(withUpdates, anyError) {
            $('html,body').css('cursor', 'auto');
            $('#updateAllBtn').prop('disabled', false);
            SetCheckForUpdatesBusy(false);
            FilterPlugins();
            if (withUpdates.length === 0) {
                if (anyError)
                    $.jGrowl('Could not check every plugin for updates (is the player online?)', { themeState: 'warn' });
                else
                    $.jGrowl('All plugins are up to date', { themeState: 'success' });
                return;
            }
            // A plugin whose privacy disclosure changed gets its own dialog
            // before the batch runs (the server refuses a blind upgrade of it
            // anyway); the rest go straight through. Only plugins with a
            // question are asked.
            var needsReview = withUpdates.filter(function (p) { return pluginPrivacyChanged[p]; });
            var body = "Update the " + withUpdates.length + " plugin(s) with an available update, one at a time?" +
                "<div class='small text-secondary mt-2'>" + EscapeHtml(withUpdates.join(', ')) + "</div>";
            if (needsReview.length)
                body += "<div class='fpp-inline-warn mt-2'><i class='fas fa-shield-halved'></i><span>" +
                    needsReview.length + " of them changed " + (needsReview.length === 1 ? "its" : "their") +
                    " privacy disclosure, or " + (needsReview.length === 1 ? "has" : "have") + " not been reviewed yet; you will be asked about " + (needsReview.length === 1 ? "it" : "each one") +
                    " before the updates run: <b>" + EscapeHtml(needsReview.join(', ')) + "</b></span></div>";
            var buttons = {};
            buttons["Update All"] = function () {
                CloseModalDialog("updateAllPluginsDialog");
                ReviewThenUpdateAll(withUpdates, needsReview);
            };
            buttons["Abort"] = function () { CloseModalDialog("updateAllPluginsDialog"); };
            DoModalDialog({
                id: "updateAllPluginsDialog",
                class: "modal-lg",
                title: "Update All Plugins",
                body: body,
                backdrop: true,
                keyboard: true,
                buttons: buttons
            });
        }

        // --- Sequential batch queue runner ---
        // Shared by Update All / Uninstall All / Reinstall All: each shifts one
        // item off a queue, drives it through StreamURL against a per-batch
        // endpoint (streaming output into the progress popup), and re-enters
        // itself once StreamURL completes. StreamURL invokes its done/error
        // callbacks BY NAME (window[name](id)), not by reference, so the runner
        // needs one single, fixed global entry point (BatchQueueNext) rather than
        // a distinct named function per batch type -- the currently-running job's
        // parameters live in `batchJob` instead.
        var batchJob = null;
        // The progress popup body is a <textarea> (see DisplayProgressDialog) and
        // cannot render HTML, so status lines are plain text appended to .value,
        // matching how StreamURL writes the streamed command output. Auto-scroll.
        function BatchQueueLog(text) {
            var outputArea = document.getElementById('pluginsProgressPopupText');
            if (!outputArea)
                return;
            outputArea.value += text;
            outputArea.scrollTop = outputArea.scrollHeight;
        }
        // job: { label, verb, verbCap, showStatus, queue, total, done,
        //        itemName(item), urlFor(item), method, methodFor(item)?, dataFor(item)?, onDone() }
        function RunBatchQueue(job) {
            batchJob = job;
            BatchQueueNext();
        }
        function BatchQueueNext() {
            var job = batchJob;
            if (!job || job.queue.length === 0) {
                batchJob = null;
                if (job) job.onDone();
                return;
            }
            var item = job.queue.shift();
            job.done++;
            if (job.showStatus) {
                SetProgressDialogStatus('pluginsProgressPopup',
                    job.label + ' — ' + job.verb + ' ' + job.done + ' of ' + job.total);
            }
            BatchQueueLog('\n===== ' + job.verbCap + ' ' + job.itemName(item) +
                ' (' + job.done + ' of ' + job.total + ') =====\n');
            var data = job.dataFor ? job.dataFor(item) : null;
            // Chain to the next item on both success and failure so a single
            // failure does not strand the batch.
            StreamURL(job.urlFor(item), 'pluginsProgressPopupText', 'BatchQueueNext', 'BatchQueueNext',
                job.methodFor ? job.methodFor(item) : job.method, data, data ? 'application/json' : null);
        }

        // Ask about each plugin whose disclosure changed, one dialog at a
        // time, then run the batch. Accept includes the plugin with the new
        // block posted as privacyAccepted; Cancel skips it (it stays at its
        // current version, whose disclosure was accepted) and is named in
        // the log. Nothing is updated until every question is answered.
        var updateAllSkipped = [];
        function ReviewThenUpdateAll(withUpdates, needsReview) {
            var accepted = {};
            var skipped = [];
            var reviewNext = function (i) {
                if (i >= needsReview.length) {
                    var batch = withUpdates.filter(function (p) { return skipped.indexOf(p) < 0; });
                    RunUpdateAll(batch, accepted, skipped);
                    return;
                }
                var plugin = needsReview[i];
                var position = needsReview.length > 1 ? (' (' + (i + 1) + ' of ' + needsReview.length + ')') : '';
                $.ajax({
                    url: 'api/plugin/' + plugin + '/privacy',
                    dataType: 'json',
                    success: function (st) {
                        if (!st || st.Status !== 'OK' || !st.changed) {
                            // Not readable, or not changed after all: the
                            // server decides at upgrade time either way.
                            reviewNext(i + 1);
                            return;
                        }
                        ConfirmPrivacyChange(plugin, st, {
                            id: 'confirmUpgradeDialog', prefix: 'up',
                            verb: 'This update', button: 'Accept changes and update',
                            position: position, chained: chained,
                            note: chained ? null : 'Cancel skips this plugin and leaves it at its current version.',
                            onAccept: function () { accepted[plugin] = st.pending; reviewNext(i + 1); },
                            onCancel: function () { skipped.push(plugin); reviewNext(i + 1); }
                        });
                    },
                    error: function () { reviewNext(i + 1); }
                });
            };
            // More than one to ask about: say what is coming once, then ask.
            var chained = needsReview.length > 1;
            if (!chained) {
                reviewNext(0);
                return;
            }
            ReviewIntroDialog({
                title: 'Update All: review ' + needsReview.length + ' privacy disclosures',
                lead: needsReview.length + ' of the plugins with an update have changed what they disclose about privacy, or have a disclosure you have not reviewed yet. ' +
                    'You will see each one in turn: <b>Accept</b> updates that plugin, <b>Cancel</b> skips it and leaves it at its current version. ' +
                    'The other ' + (withUpdates.length - needsReview.length) + ' update without asking.',
                plugins: needsReview,
                button: 'Start',
                onStart: function () { reviewNext(0); },
                onCancel: function () { $.jGrowl('Update All cancelled. Nothing was changed.', { themeState: 'detract' }); }
            });
        }

        // accepted: repoName -> the block accepted in the dialog, posted with
        // that plugin's upgrade; skipped: named in the log, not attempted.
        function RunUpdateAll(withUpdates, accepted, skipped) {
            accepted = accepted || {};
            updateAllAttempted = withUpdates.slice();
            updateAllSkipped = (skipped || []).slice();
            DisplayProgressDialog("pluginsProgressPopup", "Update All Plugins");
            if (updateAllSkipped.length)
                BatchQueueLog('\nSkipped, left at the current version (privacy disclosure not accepted): ' + updateAllSkipped.join(', ') + '\n');
            if (!withUpdates.length) {
                BatchQueueLog('Nothing to update.\n');
                ProgressDialogDone('pluginsProgressPopupText');
                return;
            }
            RunBatchQueue({
                label: 'Update All',
                verb: 'updating',
                verbCap: 'Updating',
                showStatus: true,
                queue: withUpdates.slice(),
                total: withUpdates.length,
                done: 0,
                itemName: function (plugin) { return plugin; },
                urlFor: function (plugin) { return 'api/plugin/' + plugin + '/upgrade?stream=true'; },
                // An accepted disclosure travels in a POST body, as the single
                // Update button sends it; the rest are the plain GET.
                method: 'GET',
                methodFor: function (plugin) { return Object.prototype.hasOwnProperty.call(accepted, plugin) ? 'POST' : 'GET'; },
                dataFor: function (plugin) { return Object.prototype.hasOwnProperty.call(accepted, plugin) ? JSON.stringify({ privacyAccepted: accepted[plugin] }) : null; },
                onDone: UpdateAllFinish
            });
        }

        // Plugins whose upgrade_plugin run ended with rc=2 -- the code was
        // updated but the plugin's own fpp_upgrade.sh / fpp_install.sh failed
        // -- read from the streamed log, since the re-check below cannot see
        // it: the code is at the tip, so "update available" is gone. The
        // "===== upgrade FINISH: <plugin> (rc=N) =====" line is written by
        // startPluginLog's exit trap (scripts/common) on every run.
        function UpdateAllScriptFailures() {
            var outputArea = document.getElementById('pluginsProgressPopupText');
            var failed = [];
            if (!outputArea)
                return failed;
            var re = /===== upgrade FINISH: (\S+) \(rc=2\) =====/g;
            var m;
            while ((m = re.exec(outputArea.value)) !== null) {
                if (updateAllAttempted.indexOf(m[1]) >= 0 && failed.indexOf(m[1]) < 0)
                    failed.push(m[1]);
            }
            return failed;
        }

        // After all upgrades have streamed, verify by re-checking each attempted
        // plugin: the upgrade endpoint streams output even on a logical failure, so
        // any plugin that STILL reports an update available did not update. Mirrors
        // ReinstallFinish's re-query verification. A plugin whose code updated but
        // whose install script failed is not stale, so it is picked out of the
        // streamed log instead (UpdateAllScriptFailures) and reported separately.
        function UpdateAllFinish() {
            var total = updateAllAttempted.length;
            CheckPluginsForUpdates(updateAllAttempted, function (plugin, hasUpdate) {
                if (hasUpdate) {
                    RowEl(plugin).addClass('fppHasUpdate').find('.updatesAvailable').removeClass('d-none');
                } else {
                    RowEl(plugin).removeClass('fppHasUpdate').find('.updatesAvailable').addClass('d-none');
                }
            }, function (stillStale) {
                var scriptFailed = UpdateAllScriptFailures().filter(function (p) { return stillStale.indexOf(p) < 0; });
                var problems = stillStale.length + scriptFailed.length;
                var ok = total - problems;
                SetProgressDialogStatus('pluginsProgressPopup',
                    problems ? ('Update All — ' + problems + ' with problems, ' + ok + ' of ' + total + ' ok')
                             : ('Update All — complete (' + ok + ' of ' + total + ')'));
                BatchQueueLog('\n===== Update complete: ' + ok + ' of ' + total + ' plugin(s) updated successfully =====\n');
                if (stillStale.length)
                    BatchQueueLog('Still reporting an available update (may have failed): ' + stillStale.join(', ') + '\n');
                if (scriptFailed.length)
                    BatchQueueLog('Code updated, but the plugin\'s own install/upgrade script failed (see logs/fpp_plugin_manager.log): ' + scriptFailed.join(', ') + '\n');
                if (updateAllSkipped.length)
                    BatchQueueLog('Skipped (privacy disclosure not accepted, left at the current version): ' + updateAllSkipped.join(', ') + '\n');
                BatchQueueLog('Reload the page to refresh the plugin list.\n');
                if (stillStale.length)
                    $.jGrowl(stillStale.length + ' plugin(s) may not have updated', { themeState: 'warn' });
                else if (scriptFailed.length)
                    $.jGrowl(scriptFailed.length + ' plugin(s) updated but their install script failed', { themeState: 'warn' });
                else
                    $.jGrowl('All ' + ok + ' plugin(s) updated successfully', { themeState: 'success' });
                FilterPlugins();
                ProgressDialogDone('pluginsProgressPopupText');
            });
        }

        // Ask the server whether the incoming declaration differs from the one
        // accepted at install (sends, collects, sensors, remoteAccess,
        // systemChanges or closedCode); if so, the install dialog comes back with
        // the NEW block and the upgrade POSTs it as accepted. The server refuses an upgrade that
        // skips this, so a failed status query falls through to a plain attempt
        // rather than silently proceeding.
        function UpgradePlugin(plugin) {
            $.ajax({
                url: 'api/plugin/' + plugin + '/privacy',
                dataType: 'json',
                success: function (st) {
                    if (st && st.Status === 'OK' && st.changed) {
                        ConfirmPrivacyChange(plugin, st, {
                            id: 'confirmUpgradeDialog', prefix: 'up',
                            verb: 'This update', button: 'Accept changes and update',
                            onAccept: function () { RunUpgradePlugin(plugin, { privacyAccepted: st.pending }); }
                        });
                    } else {
                        RunUpgradePlugin(plugin, null);
                    }
                },
                // The server refuses a changed disclosure on its own, so a
                // failed status call costs nothing but the nicer dialog.
                error: function () { RunUpgradePlugin(plugin, null); }
            });
        }

        // ack: null for a plain upgrade, else { privacyAccepted: <block or null> }
        // -- an object, so that accepting a REMOVED disclosure (pending null)
        // still posts a body.
        function RunUpgradePlugin(plugin, ack) {
            var url = 'api/plugin/' + plugin + '/upgrade?stream=true';
            DisplayProgressDialog("pluginsProgressPopup", "Upgrade Plugin");
            if (ack !== null) {
                StreamURL(url, 'pluginsProgressPopupText', 'ProgressDialogDone', 'ProgressDialogDone',
                    'POST', JSON.stringify(ack), 'application/json');
            } else {
                StreamURL(url, 'pluginsProgressPopupText', 'ProgressDialogDone', 'ProgressDialogDone');
            }
        }

        // The "disclosure changed" dialog shared by Update and Reinstall. st is
        // the api/plugin/:RepoName/privacy result: st.pending is the block the
        // operation would land, st.recorded says whether the operator has ever
        // accepted one for this plugin -- a plugin installed before FPP kept
        // the record has not "changed" anything, it just has not been reviewed.
        // opts: id, prefix (chip id prefix), verb ("This update" /
        // "Reinstalling"), button + onAccept; optional declineLabel + onDecline
        // (a destructive choice such as Uninstall), noCancel (no Cancel button,
        // no Esc/backdrop/X), onCancel, note (a line under the intro).
        function ConfirmPrivacyChange(plugin, st, opts) {
            var i = FindPluginInfo(plugin);
            // A dependency the server refused is named as the plugin list
            // files it, which is not always its repoName.
            if (i < 0) i = FindListedPluginInfo(plugin);
            var data = (i >= 0) ? $.extend({}, pluginInfos[i]) : { repoName: plugin, name: plugin };
            data.privacy = st.pending;
            var r = PluginPrivacyResult(data);
            var name = '<b>' + EscapeHtml(data.name || plugin) + '</b>';
            var intro, title;
            var lands = (opts.verb === 'This reinstall' ? 'this reinstall installs.' : 'this update installs.');
            // A plugin that had no disclosure when it was accepted, and has
            // one now, is read as a first disclosure, not as a change from
            // "nothing declared": every light would show as changed and say
            // nothing useful.
            var firstDisclosure = !st.recorded || !st.accepted;
            // A first look needs no sentence: the title says what this is,
            // and the block is headed "Disclosed by the author".
            if (firstDisclosure) {
                intro = '';
                title = 'Review the privacy disclosure of ' + EscapeHtml(data.name || plugin);
            } else if (!st.pending) {
                // Had one, and the version being landed has none: say that
                // plainly rather than "the author's new disclosure".
                intro = 'The version ' + lands.replace(/\.$/, '') + ' has no privacy disclosure: the author no longer says what ' + name +
                    ' does with data. What you accepted before is shown struck through.';
                title = opts.verb + ' removes the privacy disclosure of ' + EscapeHtml(data.name || plugin);
            } else {
                intro = opts.verb + ' changes what ' + name + ' discloses about privacy. This is the author\'s new disclosure.';
                title = opts.verb + ' changes the privacy disclosure of ' + EscapeHtml(data.name || plugin);
            }
            // In a chained review the intro screen said what these are, so
            // each screen is titled by the plugin: "Projector Control (1 of 5)".
            if (opts.chained) title = EscapeHtml(data.name || plugin);
            if (opts.position) title += opts.position;
            // Plain text, not a callout: the title has said it already, and
            // the coloured things on this screen should all be findings.
            var body = intro ? '<p class="mb-2"><i class="fas fa-shield-halved text-secondary"></i> ' + intro + '</p>' : '';
            if (opts.note) body += '<div class="small text-secondary mb-2">' + EscapeHtml(opts.note) + '</div>';
            // The root warning was on the intro screen of a chained review,
            // and in full when the plugin was installed: one line here.
            if (!opts.chained) body += PluginTrustLineHtml(data);
            // With a record to compare against, show the change, not just the
            // new block: st.accepted is what the operator said yes to.
            var from = null;
            if (!firstDisclosure) {
                var prevData = $.extend({}, data);
                prevData.privacy = st.accepted;
                from = PluginPrivacyResult(prevData);
            }
            body += PrivacyBlockHtml(r, opts.prefix, from);
            if (IsSafeHttpUrl(data.srcURL)) body += '<div class="small text-secondary mt-2"><i class="fas fa-code"></i> Source: ' +
                '<a href="' + EscapeAttr(data.srcURL) + '" target="_blank" rel="noopener noreferrer">' + EscapeHtml(data.srcURL) + '</a></div>';
            var buttons = {};
            var outcome = 'cancel';
            buttons[opts.button] = function () { outcome = 'accept'; CloseModalDialog(opts.id); };
            if (opts.onDecline) {
                buttons[opts.declineLabel] = function () { outcome = 'decline'; CloseModalDialog(opts.id); };
            }
            if (!opts.noCancel) {
                buttons['Cancel'] = function () { CloseModalDialog(opts.id); };
            }
            DoModalDialog({
                id: opts.id,
                class: "modal-lg",
                title: title,
                body: body,
                backdrop: opts.noCancel ? 'static' : true,
                keyboard: !opts.noCancel,
                noClose: !!opts.noCancel,
                buttons: buttons
            });
            // DoModalDialog only ever disables the header X (noClose); the id
            // is reused, so re-enable it for a dialog that may be cancelled.
            $('#' + opts.id).find('#modalCloseButton').prop('disabled', !!opts.noCancel);
            // The outcome is delivered from `hidden`, not from the click: the
            // id is reused, and a batch that opened the next dialog before
            // this one had finished fading out would see THIS dialog's hide
            // event as the next one's.
            $('#' + opts.id).one('hidden.bs.modal', function () {
                if (outcome === 'accept') opts.onAccept();
                else if (outcome === 'decline') opts.onDecline();
                else if (opts.onCancel) opts.onCancel();
            });
            FPPPluginPrivacy.bind(opts.id);
        }

        // Severity order of installButtonFor's labels, worst last, so the
        // dialog's button can take the worst across a plugin and its
        // dependencies. Unknown text ranks lowest.
        var INSTALL_BUTTON_ORDER = ['Install', 'Install anyway', 'Install, permanent changes', 'Install, sends data out',
            'Install, opens FPP to internet', "Install, handles others' data", 'Install, black box included', 'Install, no disclosure'];
        function InstallButtonRank(btn) { return INSTALL_BUTTON_ORDER.indexOf(btn.text); }

        // The last install request, so a refusal can be answered with the
        // same request plus the block just reviewed (InstallStreamDone).
        var lastInstallRequest = null;

        // depAccepted: repoName -> the block shown for each dependency plugin
        // in the install dialog (null = no disclosure), or undefined.
        // accepted: the block shown for the plugin itself; undefined = the
        // cached pluginInfo's block, which is what the install dialog shows.
        function InstallPlugin(plugin, branch, sha, depAccepted, accepted) {
            var url = 'api/plugin?stream=true';
            var i = FindPluginInfo(plugin);

            if (i < 0) {
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
            // What the install dialog showed (null = "no disclosure"): the
            // server installs only if the cloned copy says the same, and
            // records it; otherwise it refuses and hands the cloned block
            // back for review (InstallStreamDone).
            pluginInfo['privacyAccepted'] = (accepted !== undefined) ? accepted
                : (pluginInfo.hasOwnProperty('privacy') ? pluginInfo.privacy : null);
            pluginInfo['dependencyPrivacyAccepted'] = depAccepted || {};
            lastInstallRequest = { plugin: plugin, branch: branch, sha: sha,
                depAccepted: pluginInfo['dependencyPrivacyAccepted'], accepted: pluginInfo['privacyAccepted'] };

            var postData = JSON.stringify(pluginInfo);
            // A refusal can come back before the progress dialog has finished
            // opening (a local clone is quick), and a modal still opening
            // cannot be closed: InstallStreamDone waits for `shown` then.
            installProgressShown = false;
            installProgressPending = null;
            DisplayProgressDialog("pluginsProgressPopup", "Install Plugin");
            $('#pluginsProgressPopup').one('shown.bs.modal', function () {
                installProgressShown = true;
                if (installProgressPending) { var f = installProgressPending; installProgressPending = null; f(); }
            });
            StreamURL(url, 'pluginsProgressPopupText', 'InstallStreamDone', 'InstallStreamDone', 'POST', postData, 'application/json');
        }
        var installProgressShown = false;
        var installProgressPending = null;

        // After an install has streamed. A refusal on privacy grounds -- the
        // cloned copy's block is not the one the dialog showed (a pinned
        // version, a listing behind the repository), or a dependency the
        // dialog did not show -- ends the stream with one marked line
        // carrying the block to review (PluginPrivacyRefuse in
        // api/controllers/plugin.php). Nothing was installed. Take the line
        // out of the log, close the progress dialog, show the block, and on
        // Accept post the same install again with that block as accepted.
        function InstallStreamDone(textId) {
            var outputArea = document.getElementById(textId);
            var req = lastInstallRequest;
            var refusal = null;
            var m = outputArea ? outputArea.value.match(/^@@PRIVACY-PENDING@@ (.*)$/m) : null;
            if (m) {
                try { refusal = JSON.parse(m[1]); } catch (e) { refusal = null; }
                outputArea.value = outputArea.value.replace(/^@@PRIVACY-PENDING@@ .*\n?/mg, '');
            }
            if (!refusal || !refusal.plugin || !req) {
                ProgressDialogDone(textId);
                return;
            }
            var repo = refusal.plugin;
            var ofParent = (repo === req.plugin);
            // The parent is named by repoName, a dependency as the plugin
            // list files it (the name the server resolved it by).
            var nameOf = function (r) { var k = ofParent ? FindPluginInfo(r) : FindListedPluginInfo(r); return (k >= 0 && pluginInfos[k].name) ? pluginInfos[k].name : r; };
            // What the server would answer for it: a block not yet reviewed.
            var st = { Status: 'OK', changed: true, recorded: false, accepted: null, pending: refusal.pending };
            var review = function () {
                ConfirmPrivacyChange(repo, st, {
                    id: 'confirmInstallChangedDialog', prefix: 'ic',
                    verb: 'This install', button: 'Accept and install',
                    note: ofParent
                        ? 'The version being installed has a different privacy disclosure from the one shown before (the listing may be behind the repository, or this version is pinned). Nothing has been installed yet.'
                        : nameOf(repo) + ' is needed by ' + nameOf(req.plugin) + ' and was not shown before. Nothing has been installed yet.',
                    onAccept: function () {
                        var dep = $.extend({}, req.depAccepted);
                        var accepted = req.accepted;
                        if (ofParent) accepted = refusal.pending; else dep[repo] = refusal.pending;
                        InstallPlugin(req.plugin, req.branch, req.sha, dep, accepted);
                    },
                    onCancel: function () { $.jGrowl('Install cancelled. Nothing was installed.', { themeState: 'detract', life: 4000 }); }
                });
            };
            var closeThenReview = function () {
                $('#pluginsProgressPopup').one('hidden.bs.modal', review);
                CloseModalDialog('pluginsProgressPopup');
            };
            if (installProgressShown) closeThenReview();
            else installProgressPending = closeThenReview;
        }

        // Gate before InstallPlugin, on every install entry point (cards, popular
        // strip, detail modal) at every UI level. EVERY install shows the dialog,
        // official plugins included: the trust callout at the top is the one fact
        // no declaration can change, and under it are the six privacy lights FPP
        // computes from the plugin's own declaration. The URL-paste developer
        // warning and the RAM/CPU warning are unchanged and sit above both.
        // Plugins that installing `plugin` would pull in as dependencies
        // (ResolvePluginDependencies on the server): the listed plugins named
        // in dependencies.plugins, top-level and on the versions[] entry this
        // FPP would select, recursively, that are not installed. Each is
        // returned once as {repo, via} -- via being the plugin that named it,
        // `plugin` itself for a direct dependency -- in the order the dialog
        // lists them: direct dependencies first, then what they need. `repo`
        // is the name as declared, which is how the server looks it up and
        // the key it accepts in dependencyPrivacyAccepted; `plugin` itself
        // is a repoName. A dependency is resolved as the server resolves it
        // (FindListedPluginInfo); a name the list does not know is skipped,
        // as the server skips it. Installed under the declared name or under
        // its own repoName counts as installed, as it does on the server.
        function DependencyPluginsToInstall(plugin) {
            var out = [];
            var seen = {};
            seen[plugin] = true;
            var walk = function (repo, depth) {
                if (depth > 8) return;
                var i = (depth === 0) ? FindPluginInfo(repo) : FindListedPluginInfo(repo);
                if (i < 0) return;
                var info = pluginInfos[i];
                var names = [];
                var add = function (deps) {
                    if (deps && Array.isArray(deps.plugins))
                        deps.plugins.forEach(function (n) { if (typeof n === 'string' && n !== '') names.push(n); });
                };
                add(info.dependencies);
                if (Array.isArray(info.versions) && info.versions.length) {
                    var sel = SelectPluginVersionIndices(info);
                    var vi = (sel.compatible >= 0) ? sel.compatible : (sel.untested >= 0 ? sel.untested : 0);
                    if (info.versions[vi]) add(info.versions[vi].dependencies);
                }
                names.forEach(function (n) {
                    if (seen[n]) return;
                    seen[n] = true;
                    if (installedPlugins.indexOf(n) >= 0) return;
                    var k = FindListedPluginInfo(n);
                    if (k < 0) return;
                    if (pluginInfos[k].repoName && installedPlugins.indexOf(pluginInfos[k].repoName) >= 0) return;
                    out.push({ repo: n, via: repo });
                    walk(n, depth + 1);
                });
            };
            walk(plugin, 0);
            return out;
        }

        // Installed plugins that depend on `repo`, directly or through another
        // plugin (dependencies.plugins, top-level and on the selected
        // versions[] entry, read from the cached pluginInfos). Uninstalling
        // `repo` leaves them without something they need.
        function InstalledDependantsOf(repo) {
            var out = [];
            var dependsOn = function (info, name) {
                var hit = false;
                // A dependant declares the dependency by its plugin-list
                // name, which can differ from the repoName it is installed
                // under (FPP-Plugin-TwilioControl vs TwilioControl).
                var listKey = pluginListKeyOf[name] || name;
                var check = function (deps) {
                    if (deps && Array.isArray(deps.plugins) && (deps.plugins.indexOf(name) >= 0 || deps.plugins.indexOf(listKey) >= 0)) hit = true;
                };
                check(info.dependencies);
                if (Array.isArray(info.versions) && info.versions.length) {
                    var sel = SelectPluginVersionIndices(info);
                    var vi = (sel.compatible >= 0) ? sel.compatible : (sel.untested >= 0 ? sel.untested : 0);
                    if (info.versions[vi]) check(info.versions[vi].dependencies);
                }
                return hit;
            };
            var walk = function (name) {
                installedPlugins.forEach(function (p) {
                    if (p === name || out.indexOf(p) >= 0) return;
                    var i = FindPluginInfo(p);
                    if (i >= 0 && dependsOn(pluginInfos[i], name)) {
                        out.push(p);
                        walk(p);
                    }
                });
            };
            walk(repo);
            return out;
        }

        // Everything an install asks the operator to accept, for the install
        // dialog and the detail modal alike: the root callout, the plugin's
        // block and source line, then one section per dependency plugin that
        // the install will bring in (DependencyPluginsToInstall), each with
        // its own callout, block and source. Returns
        //   html:        the markup;
        //   btn:         {text, cls} for the Install button, the worst finding
        //                across the plugin and its dependencies (guidelines
        //                §14.15: "Install", "Install anyway", "Install, black
        //                box included", ...). A pasted URL, a device that is
        //                too small, or a version not updated for this FPP
        //                forces at least "Install anyway" in warning colour
        //                when the finding-based label would be plainer;
        //   depAccepted: repoName -> the block shown for each dependency
        //                (null = no disclosure), posted with the install as
        //                dependencyPrivacyAccepted so the server can record
        //                each one. The plugin's own block is what
        //                InstallPlugin posts by default (the cached pluginInfo).
        // Dependency plugins are installed in the same operation with no
        // dialog of their own, which is why their disclosures are here.
        // prefix: id prefix for the strips (dependencies get prefix + 'd' + n).
        function InstallDisclosureHtml(plugin, data, prefix) {
            var html = '';
            // Resource warning is orthogonal to trust: it applies to Official plugins too.
            var res = data ? PluginResourceVerdict(data) : { exceeds: false };
            if (res.exceeds)
                html += '<div class="fpp-major-callout mb-2"><i class="fas fa-microchip"></i>' +
                    '<span><b>Not enough RAM/CPU.</b> ' + res.title +
                    ' Installing it anyway may degrade or disrupt your show.</span></div>';
            // Pasting a plugininfo.json URL is a developer workflow (testing a plugin
            // mid-development, often from a branch/fork that isn't in the plugin list at
            // all yet) -- not a general install path. Shown regardless of the
            // resolved plugin's official/third-party status.
            if (manuallyLoadedPlugins[plugin])
                html += '<div class="fpp-major-callout mb-2"><i class="fas fa-user-gear"></i>' +
                    '<span>Installing a plugin from a URL is intended for <b>plugin developers</b> ' +
                    'testing their own plugin while developing it. It runs <b>whatever code is found at that ' +
                    'URL</b> as <b>root</b>, with full access to this device &mdash; including every ' +
                    'setting, the privacy settings among them &mdash; <b>and to anything else on the ' +
                    'network FPP is connected to</b>. This is inherently dangerous. If you are not a developer, we recommend ' +
                    'you <b>do not</b> install this ' +
                    'plugin.</span></div>';
            html += PluginTrustHtml(data || { repoName: plugin, name: plugin });
            var r = PluginPrivacyResult(data || { repoName: plugin });
            html += PrivacyBlockHtml(r, prefix);
            var src = (data && data.srcURL) ? data.srcURL : '';
            if (IsSafeHttpUrl(src)) html += '<div class="small text-secondary mt-2"><i class="fas fa-code"></i> Source: ' +
                '<a href="' + EscapeAttr(src) + '" target="_blank" rel="noopener noreferrer">' + EscapeHtml(src) + '</a></div>';
            var btn = FPPPluginPrivacy.installButtonFor(r);
            var deps = data ? DependencyPluginsToInstall(plugin) : [];
            var depAccepted = {};
            deps.forEach(function (d, n) {
                var dep = d.repo;
                var di = FindListedPluginInfo(dep);
                var dinfo = pluginInfos[di];
                depAccepted[dep] = dinfo.hasOwnProperty('privacy') ? dinfo.privacy : null;
                var dr = PluginPrivacyResult(dinfo);
                // Name the plugin that needs it when that is not the one
                // being installed: a dependency of a dependency.
                var vi = (d.via === plugin) ? FindPluginInfo(d.via) : FindListedPluginInfo(d.via);
                var viaName = (d.via === plugin) ? 'this one' : ((vi >= 0 && pluginInfos[vi].name) ? pluginInfos[vi].name : d.via);
                html += '<hr><div class="fw-bold mb-2 pluginInstallDependency"><i class="fas fa-puzzle-piece"></i> Also installs <b>' + EscapeHtml(dinfo.name || dep) + '</b>' +
                    ' <span class="fw-normal text-secondary">(a plugin ' + EscapeHtml(viaName) + ' depends on)</span></div>';
                html += PluginTrustHtml(dinfo);
                html += PrivacyBlockHtml(dr, prefix + 'd' + n);
                if (IsSafeHttpUrl(dinfo.srcURL)) html += '<div class="small text-secondary mt-2"><i class="fas fa-code"></i> Source: ' +
                    '<a href="' + EscapeAttr(dinfo.srcURL) + '" target="_blank" rel="noopener noreferrer">' + EscapeHtml(dinfo.srcURL) + '</a></div>';
                var dbtn = FPPPluginPrivacy.installButtonFor(dr);
                if (InstallButtonRank(dbtn) > InstallButtonRank(btn)) btn = dbtn;
            });
            // The card says "Install anyway" for a version not updated for
            // this FPP release, and for a device under the plugin's declared
            // minimums; the dialogs say at least that too. A pasted URL is a
            // developer path and gets the same.
            var sel = (data && Array.isArray(data.versions) && data.versions.length) ? SelectPluginVersionIndices(data) : { compatible: 0, untested: -1 };
            var forceAnyway = !!manuallyLoadedPlugins[plugin] || (data && PluginResourceVerdict(data).exceeds) ||
                (sel.compatible < 0 && sel.untested >= 0);
            if (forceAnyway && btn.text === 'Install') btn = { text: 'Install anyway', cls: 'btn-warning' };
            return { html: html, btn: btn, depAccepted: depAccepted };
        }

        function ConfirmAndInstall(plugin, branch, sha) {
            var i = FindPluginInfo(plugin);
            var data = (i >= 0) ? pluginInfos[i] : null;
            var official = !!(data && IsOfficialPlugin(data));
            var d = InstallDisclosureHtml(plugin, data, 'ci');
            var body = d.html;
            var buttons = {};
            buttons[d.btn.text] = {
                class: d.btn.cls,
                click: function () {
                    CloseModalDialog("confirmInstallDialog");
                    InstallPlugin(plugin, branch, sha, d.depAccepted);
                }
            };
            buttons['Cancel'] = function () { CloseModalDialog("confirmInstallDialog"); };
            DoModalDialog({
                id: "confirmInstallDialog",
                class: "modal-lg",
                title: official ? "Install this plugin?" : "Install third-party plugin?",
                body: body,
                backdrop: true,
                keyboard: true,
                buttons: buttons
            });
            FPPPluginPrivacy.bind('confirmInstallDialog');
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
                body: "Please confirm you wish to uninstall the " + EscapeHtml(pluginName) + " plugin",
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

        function UninstallAllPlugins() {
            var queue = installedPlugins.slice();
            if (queue.length === 0) {
                $.jGrowl('No plugins installed', { themeState: 'detract' });
                return;
            }
            DisplayProgressDialog("pluginsProgressPopup", "Uninstall All Plugins");
            RunBatchQueue({
                verbCap: 'Uninstalling',
                showStatus: false,
                queue: queue,
                total: queue.length,
                done: 0,
                itemName: function (plugin) { return plugin; },
                urlFor: function (plugin) { return 'api/plugin/' + plugin + '?stream=true'; },
                method: 'DELETE',
                onDone: function () { ProgressDialogDone('pluginsProgressPopupText'); }
            });
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

        // Reinstall: uninstall the given plugin(s), then reinstall each one by
        // one. Runs entirely client-side against the per-plugin API endpoints,
        // mirroring the Uninstall All queue pattern above but in two phases.
        // Shared by both Reinstall All and a single plugin's Reinstall action --
        // repos.length is just 1 in the latter case, everything else (progress
        // dialog, batch queue, re-verify) is identical.
        var reinstallAttempted = [];   // repo names we intend to reinstall
        var reinstallSkipped = [];     // requested plugins with no cached info
        var reinstallHeldBack = [];    // could not be checked; left installed
        var reinstallDeclined = [];    // new disclosure not accepted in Reinstall All; uninstalled, not reinstalled

        // Build the install POST body for an installed plugin from its cached
        // pluginInfo.json, or null if we have no cached info to rebuild it from
        // (repo was never loaded this session -- can't safely reinstall it).
        // The branch and sha are the server's choice (reinstallTarget from the
        // update check every reinstall starts with, or from ?target=reinstall):
        // the block it diffed and the commit it clones are then the same one.
        // The page's own selection is only a fallback for a plugin never checked.
        function BuildReinstallInfo(repo) {
            var i = FindPluginInfo(repo);
            if (i < 0) return null;
            var info = JSON.parse(JSON.stringify(pluginInfos[i])); // copy, don't mutate cache
            var t = pluginReinstallTarget[repo];
            if (t && t.branch) {
                info['branch'] = t.branch;
                info['sha'] = t.sha || '';
            } else {
                if (!Array.isArray(info.versions) || !info.versions.length) return null;
                var sel = SelectPluginVersionIndices(info);
                var idx = sel.compatible >= 0 ? sel.compatible : (sel.untested >= 0 ? sel.untested : 0);
                var v = info.versions[idx];
                info['branch'] = (v.branch && v.branch !== '') ? v.branch : 'master';
                info['sha'] = v.sha || '';
            }
            if (pluginInfoURLs[repo])
                info['infoURL'] = pluginInfoURLs[repo]; // else backend uses the repo's own pluginInfo.json
            info['useCredentials'] = (info.private || pluginInfoUseCredentials[repo]) ? 1 : 0;
            return info;
        }

        // True when the post-FPPOS flag names this plugin (or is boot's "1",
        // before the server has turned it into names): it is dead as it is
        // until reinstalled, so a single-card Reinstall may not be cancelled.
        function PluginPendingAfterOS(plugin) {
            var v = (settings['pluginReinstallNeededAfterOS'] || '').trim();
            if (v === '') return false;
            if (v === '1') return true;
            return v.split(',').map(function (x) { return x.trim(); }).indexOf(plugin) >= 0;
        }

        function ReinstallAllPlugins() {
            if (installedPlugins.length === 0) {
                // Nothing to reinstall; the server cleared the post-FPPOS flag
                // on page load (PluginReinstallPendingSync) if it was set.
                $.jGrowl('No plugins installed', { themeState: 'detract' });
                return;
            }
            RunReinstall(installedPlugins.slice(), 'Reinstall All Plugins');
        }

        function ReinstallPlugin(repo) {
            RunReinstall([repo], 'Reinstall Plugin');
        }

        // label is also used as the progress dialog title and the RunBatchQueue
        // status-line prefix, so it reads e.g. "Reinstall Plugin — uninstalling 1
        // of 1" for a single plugin vs "Reinstall All Plugins — uninstalling 3 of 8".
        function RunReinstall(repos, label) {
            // A reinstall lands the versions[] branch tip, so it is an upgrade
            // by another route: check every plugin first, then ask about each
            // changed disclosure in turn -- Accept queues it with the new
            // block, "Uninstall instead" removes it, Cancel only on a single
            // card that is not waiting for a post-FPPOS reinstall -- and only
            // then uninstall anything. A plugin whose check FAILED is left as
            // it is: the uninstall is destructive and the clone needs the same
            // network the check did.
            if (!repos.length)
                return;
            // A single plugin with no cached pluginInfo cannot be rebuilt into
            // an install body (RunReinstallPhases would skip it), so say so now
            // rather than after a fetch and possibly a disclosure dialog.
            if (repos.length === 1 && !BuildReinstallInfo(repos[0])) {
                $.jGrowl('Cannot reinstall ' + EscapeHtml(repos[0]) + ': its plugin info is not available. Nothing was changed.', { themeState: 'warn', sticky: true });
                return;
            }
            CancelBackgroundUpdateCheck();
            $('html,body').css('cursor', 'wait');
            $.jGrowl('Checking ' + (repos.length === 1 ? EscapeHtml(repos[0]) : repos.length + ' plugins') + ' for changes...', { themeState: 'detract', life: 4000 });
            var checked = {};
            CheckPluginsForUpdates(repos, function (plugin) { checked[plugin] = true; }, function () {
                $('html,body').css('cursor', 'auto');
                var unchecked = repos.filter(function (r) { return !checked[r]; });
                var toReview = repos.filter(function (r) { return checked[r] && pluginReinstallPrivacyChanged[r]; });
                var ready = repos.filter(function (r) { return checked[r] && !pluginReinstallPrivacyChanged[r]; });
                var accepted = {};
                var declined = [];
                var single = (repos.length === 1);
                if (single && unchecked.length) {
                    // Nothing has been touched yet, so a stop here is a plain
                    // notice, not a progress log to close and reload from.
                    $.jGrowl('Could not check ' + EscapeHtml(repos[0]) + ' for changes (is the player online?). Nothing was changed.', { themeState: 'warn', sticky: true });
                    return;
                }
                var finish = function () {
                    RunReinstallPhases(ready, label, accepted, declined, unchecked);
                };
                // Ask about the plugins whose disclosure changed, in order.
                var reviewNext = function (i) {
                    if (i >= toReview.length) {
                        finish();
                        return;
                    }
                    var plugin = toReview[i];
                    var position = toReview.length > 1 ? (' (' + (i + 1) + ' of ' + toReview.length + ')') : '';
                    var last = (i + 1 >= toReview.length);
                    // Already taken down with a plugin it depends on: no question.
                    if (declined.indexOf(plugin) >= 0) {
                        reviewNext(i + 1);
                        return;
                    }
                    // Plugins in this batch that depend on this one go with it
                    // if it is uninstalled -- said before the choice, not after.
                    var dependants = InstalledDependantsOf(plugin).filter(function (d) { return (single || repos.indexOf(d) >= 0) && declined.indexOf(d) < 0; });
                    var depWarn = dependants.length ? (' Uninstalling it also uninstalls ' + dependants.map(function (d) { var k = FindPluginInfo(d); return (k >= 0 && pluginInfos[k].name) ? pluginInfos[k].name : d; }).join(', ') + ', which ' + (dependants.length === 1 ? 'depends' : 'depend') + ' on it.') : '';
                    $.ajax({
                        // target=reinstall: the block of the versions[] branch
                        // and pin a fresh clone lands, not origin/<current>
                        url: 'api/plugin/' + plugin + '/privacy?target=reinstall',
                        dataType: 'json',
                        success: function (st) {
                            if (!st || st.Status !== 'OK') {
                                unchecked.push(plugin);
                                reviewNext(i + 1);
                                return;
                            }
                            if (st.reinstallTarget) pluginReinstallTarget[plugin] = st.reinstallTarget;
                            if (!st.changed) {
                                ready.push(plugin);
                                reviewNext(i + 1);
                                return;
                            }
                            // Cancel (leave the plugin as it is) exists only on
                            // the single-card path and only when the plugin is
                            // not waiting for a post-FPPOS reinstall: after a
                            // reflash it is dead as it is, so the choice is
                            // accept or uninstall, as in Reinstall All.
                            var mustDecide = !single || PluginPendingAfterOS(plugin);
                            ConfirmPrivacyChange(plugin, st, {
                                id: 'confirmReinstallDialog', prefix: 'ri',
                                verb: 'This reinstall', button: 'Accept and reinstall' + (single ? '' : ' this plugin'),
                                declineLabel: 'Uninstall instead',
                                noCancel: mustDecide,
                                position: position, chained: chained,
                                // The chained screens were told on the intro
                                // screen what the buttons do; a single one
                                // is told here.
                                note: ((chained ? '' : !single ? 'Accept to reinstall, or uninstall instead \u2014 after an FPP OS upgrade a plugin that is not reinstalled no longer works as it is.'
                                    : (mustDecide ? 'After the FPP OS upgrade this plugin no longer works as it is: accept to reinstall it, or uninstall it instead.'
                                                  : 'Accept to reinstall, uninstall the plugin instead, or Cancel to leave it as it is.')) + depWarn).trim() || null,
                                onAccept: function () {
                                    accepted[plugin] = st.pending;
                                    ready.push(plugin);
                                    reviewNext(i + 1);
                                },
                                onDecline: function () {
                                    declined.push(plugin);
                                    dependants.forEach(function (d) {
                                        if (declined.indexOf(d) < 0) declined.push(d);
                                        ready = ready.filter(function (r) { return r !== d; });
                                        delete accepted[d];
                                    });
                                    reviewNext(i + 1);
                                },
                                onCancel: single ? function () { reviewNext(i + 1); } : null
                            });
                        },
                        error: function () {
                            unchecked.push(plugin);
                            reviewNext(i + 1);
                        }
                    });
                };
                if (single && !toReview.length) {
                    finish();
                    return;
                }
                // More than one to ask about: say what is coming once, then
                // ask. Nothing has been touched, but a Reinstall All after an
                // FPP OS upgrade has no way back, so no Cancel.
                var chained = toReview.length > 1;
                var start = function () {
                    if (!chained) {
                        reviewNext(0);
                        return;
                    }
                    ReviewIntroDialog({
                        title: 'Reinstall All: review ' + toReview.length + ' privacy disclosures',
                        lead: toReview.length + ' of your installed plugins have changed what they disclose about privacy, or have a disclosure you have not reviewed yet. ' +
                            'You will see each one in turn: <b>Accept</b> reinstalls that plugin, <b>Uninstall instead</b> removes it. After an FPP OS upgrade a plugin that is not reinstalled no longer works as it is.' +
                            (ready.length ? ' The other ' + ready.length + ' reinstall without asking.' : ''),
                        plugins: toReview,
                        button: 'Start',
                        onStart: function () { reviewNext(0); }
                    });
                };
                if (single) {
                    // The operator pressed Reinstall on one card: Uninstall
                    // instead is a plain uninstall; Cancel leaves it alone.
                    var origFinish = finish;
                    finish = function () {
                        if (unchecked.length) {
                            $.jGrowl('Could not read the privacy disclosure of ' + EscapeHtml(repos[0]) + '. Nothing was changed.', { themeState: 'warn', sticky: true });
                            return;
                        }
                        if (declined.length) {
                            // Also anything installed that depends on it; the
                            // dialog said so. Uninstall queue only, nothing reinstalled.
                            InstalledDependantsOf(repos[0]).forEach(function (d) { if (declined.indexOf(d) < 0) declined.push(d); });
                            RunReinstallPhases([], 'Uninstall Plugin', {}, declined, []);
                            return;
                        }
                        if (!ready.length) {
                            $.jGrowl('Reinstall cancelled. Nothing was changed.', { themeState: 'detract', life: 4000 });
                            return;
                        }
                        origFinish();
                    };
                }
                start();
            });
        }

        // acceptedPrivacy: repoName -> the block the operator just accepted in
        // the dialog, posted as privacyAccepted so the server records it once
        // the clone confirms it. declined: plugins the operator chose not to
        // accept a changed disclosure for -- uninstalled, not reinstalled.
        // unchecked: plugins whose update check failed -- left installed.
        function RunReinstallPhases(repos, label, acceptedPrivacy, declined, unchecked) {
            // Phase 0: capture the install POST body for every plugin BEFORE
            // removing anything, since uninstalling drops entries from
            // installedPlugins / the DOM. Only plugins we can rebuild an install
            // body for are queued; anything without cached info is left untouched
            // and reported as skipped rather than uninstalled-without-reinstall.
            var installQueue = [];
            reinstallAttempted = [];
            reinstallSkipped = [];
            reinstallHeldBack = unchecked;
            reinstallDeclined = declined;
            repos.forEach(function (repo) {
                var info = BuildReinstallInfo(repo);
                if (!info) {
                    reinstallSkipped.push(repo); // no cached info -> can't rebuild the install body
                    return;
                }
                // What the operator has seen for this plugin: the block just
                // accepted in the dialog, else the installed copy's (the check
                // above said it matches the accepted record).
                info.privacyAccepted = Object.prototype.hasOwnProperty.call(acceptedPrivacy, repo) ?
                    acceptedPrivacy[repo] : (info.hasOwnProperty('privacy') ? info.privacy : null);
                installQueue.push(info);
                reinstallAttempted.push(repo);
            });
            // A plugin in the batch that another one depends on is cloned as
            // that one's dependency when it comes up first: what was seen for
            // it travels with every install body in the batch, so the server
            // has it either way. A listed dependency that is not installed
            // (uninstalled since) goes in with the block the page has for it,
            // as it would have been shown at install. A dependency declared
            // only in the cloned copy was never shown: the server refuses it
            // (the parent is then reported as failed, with the reason in the
            // log, and can be installed again from its card).
            var batchShown = {};
            installQueue.forEach(function (info) {
                DependencyPluginsToInstall(info.repoName).forEach(function (d) {
                    var k = FindListedPluginInfo(d.repo);
                    if (k >= 0 && !Object.prototype.hasOwnProperty.call(batchShown, d.repo))
                        batchShown[d.repo] = pluginInfos[k].hasOwnProperty('privacy') ? pluginInfos[k].privacy : null;
                });
            });
            installQueue.forEach(function (info) { batchShown[info.repoName] = info.privacyAccepted; });
            installQueue.forEach(function (info) { info.dependencyPrivacyAccepted = batchShown; });

            DisplayProgressDialog("pluginsProgressPopup", label);
            if (declined.length) {
                BatchQueueLog('\nRemoving, not reinstalling (privacy disclosure not accepted): ' + declined.join(', ') + '\n');
            }
            if (unchecked.length) {
                BatchQueueLog('\nLeft installed as they are (could not be checked for changes \u2014 is the player online?): ' + unchecked.join(', ') + '\n');
            }
            if (reinstallAttempted.length === 0 && declined.length === 0) {
                if (unchecked.length)
                    $.jGrowl('Nothing reinstalled: ' + unchecked.length + ' plugin(s) could not be checked (is the player online?)', { themeState: 'warn', sticky: true });
                else
                    BatchQueueLog('No reinstallable plugins found (plugin info unavailable).\n');
                ProgressDialogDone('pluginsProgressPopupText');
                return;
            }
            if (reinstallSkipped.length) {
                BatchQueueLog('\nSkipping ' + reinstallSkipped.length +
                    ' plugin(s) with no available plugin info (left installed): ' +
                    reinstallSkipped.join(', ') + '\n');
            }
            RunBatchQueue({
                label: label,
                verb: 'uninstalling',
                verbCap: 'Uninstalling',
                showStatus: true,
                queue: reinstallAttempted.concat(declined),
                total: reinstallAttempted.length + declined.length,
                done: 0,
                itemName: function (plugin) { return plugin; },
                // A plugin about to be reinstalled keeps its apt packages across
                // the uninstall (the install half reconciles them); one whose new
                // disclosure was declined is really being uninstalled.
                urlFor: function (plugin) {
                    return 'api/plugin/' + plugin + '?stream=true'
                        + (reinstallAttempted.indexOf(plugin) !== -1 ? '&keepPackages=1' : '');
                },
                method: 'DELETE',
                onDone: function () { RunReinstallInstallPhase(installQueue, label); }
            });
        }

        function RunReinstallInstallPhase(installQueue, label) {
            RunBatchQueue({
                label: label,
                verb: 'installing',
                verbCap: 'Installing',
                showStatus: true,
                queue: installQueue,
                total: reinstallAttempted.length,
                done: 0,
                itemName: function (info) { return info.repoName; },
                urlFor: function () { return 'api/plugin?stream=true'; },
                method: 'POST',
                dataFor: function (info) { return JSON.stringify(info); },
                onDone: function () { ReinstallFinish(label); }
            });
        }

        // After the reinstall phase, verify what actually ended up installed by
        // re-querying the authoritative list rather than trying to parse streamed
        // output (the install endpoint streams "Done" even on a logical failure).
        // Any plugin we attempted but that is now missing is reported as failed.
        function ReinstallFinish(label) {
            // A privacy refusal in the batch (a dependency only the clone
            // declares, or a race) streamed its block on a marked line for
            // InstallStreamDone; here the readable line above it is enough.
            var ta = document.getElementById('pluginsProgressPopupText');
            if (ta) ta.value = ta.value.replace(/^@@PRIVACY-PENDING@@ .*\n?/mg, '');
            $.ajax({
                url: 'api/plugin',
                dataType: 'json',
                success: function (data) {
                    installedPlugins = data;
                    var failed = reinstallAttempted.filter(function (r) { return data.indexOf(r) < 0; });
                    var ok = reinstallAttempted.length - failed.length;
                    // The post-FPPOS-upgrade flag is the server's: each plugin
                    // drops off it as it is reinstalled or uninstalled
                    // (PluginReinstallPendingSync), and fppd drops its warning
                    // when the list is empty. An unchecked plugin is still on
                    // it, so the prompt persists for a retry.
                    var held = (reinstallHeldBack.length ? (', ' + reinstallHeldBack.length + ' not checked') : '') +
                               (reinstallDeclined.length ? (', ' + reinstallDeclined.length + ' removed') : '');
                    SetProgressDialogStatus('pluginsProgressPopup',
                        failed.length ? (label + ' — ' + failed.length + ' failed, ' + ok + ' of ' + reinstallAttempted.length + ' ok' + held)
                                      : (label + ' — complete (' + ok + ' of ' + reinstallAttempted.length + held + ')'));
                    BatchQueueLog('\n===== Reinstall complete: ' + ok + ' of ' +
                        reinstallAttempted.length + ' plugin(s) reinstalled successfully =====\n');
                    if (failed.length) {
                        BatchQueueLog('Failed to reinstall ' + failed.length +
                            ' plugin(s): ' + failed.join(', ') + '\n');
                    }
                    if (reinstallSkipped.length) {
                        BatchQueueLog('Skipped (no plugin info, left installed): ' +
                            reinstallSkipped.join(', ') + '\n');
                    }
                    if (reinstallHeldBack.length) {
                        BatchQueueLog('Left installed as they were (could not be checked): ' +
                            reinstallHeldBack.join(', ') + '\n');
                    }
                    if (reinstallDeclined.length) {
                        BatchQueueLog('Removed (privacy disclosure not accepted): ' +
                            reinstallDeclined.join(', ') + '\n');
                    }
                    BatchQueueLog('Reload the page to refresh the plugin list.\n');
                    if (failed.length)
                        $.jGrowl(failed.length + ' plugin(s) failed to reinstall', { themeState: 'danger' });
                    else if (reinstallHeldBack.length || reinstallDeclined.length)
                        $.jGrowl(ok + ' plugin(s) reinstalled' + held, { themeState: 'warn' });
                    else
                        $.jGrowl('All ' + ok + ' plugin(s) reinstalled successfully', { themeState: 'success' });
                    ProgressDialogDone('pluginsProgressPopupText');
                },
                error: function () {
                    BatchQueueLog('\nReinstall finished, but could not verify the installed plugin list. Reload the page to check.\n');
                    ProgressDialogDone('pluginsProgressPopupText');
                }
            });
        }

        function ShowReinstallAllPluginsPopup() {
            if (installedPlugins.length === 0) {
                // The server cleared the post-FPPOS flag on page load if it was set.
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

        function ShowReinstallPluginPopup(repo, pluginName) {
            if (!pluginName) {
                var pi = FindPluginInfo(repo);
                pluginName = (pi >= 0 && pluginInfos[pi].name) ? pluginInfos[pi].name : repo;
            }
            DoModalDialog({
                id: "reinstallPluginDialog",
                class: "modal-lg",
                title: "Reinstall Plugin",
                body: "This will uninstall and then reinstall the " + EscapeHtml(pluginName) + " plugin.",
                backdrop: true,
                keyboard: true,
                buttons: {
                    Reinstall: function () {
                        CloseModalDialog("reinstallPluginDialog");
                        ReinstallPlugin(repo);
                    },
                    Cancel: function () {
                        CloseModalDialog("reinstallPluginDialog");
                    }
                }
            });
        }

        // The post-FPPOS-upgrade warning's Fix button links here with
        // ?action=reinstallAll; pop the Reinstall All confirmation automatically.
        // Gated on the pluginReinstallNeededAfterOS setting (still set only while a
        // reinstall is actually needed) and guarded to at most once per page load.
        var autoReinstallHandled = false;
        function MaybeAutoOpenReinstallAll() {
            if (autoReinstallHandled)
                return;
            var params = new URLSearchParams(window.location.search);
            if (params.get('action') === 'reinstallAll' && settings['pluginReinstallNeededAfterOS']) {
                autoReinstallHandled = true;
                // Drop the parameter from the address: the progress dialog's
                // Close reloads the page, and when a plugin was left as it
                // was (its check failed) the flag stays set, so the confirm
                // would otherwise pop again and redo every plugin.
                params.delete('action');
                history.replaceState(null, '', window.location.pathname + (params.toString() ? '?' + params.toString() : '') + window.location.hash);
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

        // A dependency plugin, by the name another plugin declares it under:
        // resolved as the server does (ResolvePluginInfoByName), by the name
        // the plugin list files it under first, then by repoName. The two
        // differ for some plugins (FPP-Plugin-TwilioControl / TwilioControl).
        function FindListedPluginInfo(name) {
            for (var i = 0; i < pluginInfos.length; i++) {
                if (pluginInfos[i].repoName && pluginListKeyOf[pluginInfos[i].repoName] === name)
                    return i;
            }
            return FindPluginInfo(name);
        }

        // Selects a plugin's card by exact element id via getElementById rather than
        // a jQuery '#row-' + repo selector string -- repoName is plugin-declared free
        // text (see PluginAuthorHtml above), and building a CSS-selector string from
        // it can throw (special selector characters) instead of just finding nothing.
        function RowEl(repo) {
            return $(document.getElementById('row-' + repo));
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

        // --- Popularity ---

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

        // --- GitHub issue/PR counts (Developer UI) ---
        // In Developer UI mode each plugin card shows its repo's open issue and
        // open pull request counts in the bottom-right corner. Counts come from
        // the same-origin backend proxy (api/plugin/githubStats), which groups
        // repos into ONE GitHub issue-search query per chunk rather than one
        // request per plugin -- per-repo requests are what produce a flood of
        // 404s when a repo is gone or the device has no network. The backend
        // fails soft (stale disk cache, else empty), so when network access is
        // unavailable the response is empty and we simply never draw the corner.
        var pluginGitHubRepos = {};   // repoName -> "owner/repo" (GitHub srcURL only, lowercase)
        var pluginGitHubStats = {};   // "owner/repo" -> { openIssues, openPRs } -- ONLY resolved repos
        var githubStatsRequested = {}; // "owner/repo" -> true once asked, so unresolved repos are not re-requested
        var pluginDetailDialogRepo = null; // repo shown in the open detail modal, so its footer badge can be patched
        var githubStatsFetching = false;
        var githubStatsFailed = false; // latched after a network failure: never retry this session

        // GitHub owner/repo for a plugin's source repo ('' when not a GitHub repo).
        // Normalized to lowercase so keys match the backend, which lowercases
        // every repo in its response (GitHub repo names are case-insensitive).
        function GitHubRepoOf(data) {
            var u = data && data.srcURL;
            if (!u) return '';
            try {
                var parsed = new URL(u);
                if (parsed.host.toLowerCase() !== 'github.com') return '';
                var seg = parsed.pathname.split('/').filter(function (x) { return x.length > 0; });
                if (seg.length < 2) return '';
                // srcURL often ends in ".git" (e.g. .../fpp-brightness.git); the
                // GitHub repo name has no extension, and a ".git" repo in a
                // search query fails the whole query.
                return (seg[0] + '/' + seg[1].replace(/\.git$/i, '')).toLowerCase();
            } catch (e) {
                return '';
            }
        }

        // Corner badge HTML for a repo ('' when we have no data for it).
        function GitHubStatsBadgeHtml(repo) {
            var s = repo && pluginGitHubStats[repo];
            if (!s) return '';
            var issues = parseInt(s.openIssues, 10) || 0;
            var prs = parseInt(s.openPRs, 10) || 0;
            return '<span class="fpp-tag gap-1 pluginGitHubStats" title="' + issues +
                ' open issue' + (issues === 1 ? '' : 's') + ', ' + prs +
                ' open pull request' + (prs === 1 ? '' : 's') + '">' +
                '<i class="fas fa-exclamation-circle"></i> ' + issues +
                ' <i class="fas fa-code-branch ms-1"></i> ' + prs + '</span>';
        }

        // The badge, wrapped for inline placement at the right end of the action
        // button row ('' when we have no data for the repo). ms-auto pushes it to
        // the far right of the same flex line as the buttons; align-items-center on
        // the actions row keeps it vertically centered with them.
        function GitHubStatsRowHtml(repo) {
            var badge = GitHubStatsBadgeHtml(repo);
            if (!badge) return '';
            return '<div class="ms-auto pluginGitHubStatsRow">' + badge + '</div>';
        }

        // Stamp the counts onto already-rendered cards (called once counts
        // arrive; cards rendered after that get the corner inline in LoadPlugin).
        // Only touches cards whose repo we have counts for -- cards without data
        // keep no corner, which is what hides the feature when offline.
        function PatchPluginGitHubStats() {
            if (uiLevel < 3) return;
            $('#pluginGrid, #installedGrid, #incompatibleGrid').children('.pluginCard').each(function () {
                var repoName = ($(this).attr('id') || '').replace(/^row-/, '');
                var repo = pluginGitHubRepos[repoName];
                if (!repo) return;
                var badge = GitHubStatsBadgeHtml(repo);
                var $actions = $(this).find('.pluginCardActions').first();
                if (!$actions.length) return;
                var $badge = $actions.find('.pluginGitHubStatsRow').first();
                if (!$badge.length) {
                    if (!badge) return;  // nothing to show yet
                    $badge = $('<div class="ms-auto pluginGitHubStatsRow"></div>');
                    $actions.append($badge);
                }
                $badge.find('.pluginGitHubStats').remove();
                if (badge) $badge.append(badge);
                else $badge.remove();
            });

            // Keep an open detail dialog's footer badge in sync too (a card can be
            // opened before its counts arrive).
            if (pluginDetailDialogRepo && $('#pluginDetailDialog').length) {
                var dRepo = pluginGitHubRepos[pluginDetailDialogRepo];
                var dBadge = GitHubStatsBadgeHtml(dRepo);
                var $dBadge = $('#pluginDetailDialog .modal-footer .pluginDetailGitHubStats').first();
                if (dBadge) {
                    if (!$dBadge.length) {
                        $dBadge = $('<span class="pluginDetailGitHubStats me-auto"></span>');
                        $('#pluginDetailDialog .modal-footer').prepend($dBadge);
                    }
                    $dBadge.html(dBadge);
                } else if ($dBadge.length) {
                    $dBadge.remove();
                }
            }
        }

        var githubStatsDebounce = null;
        var githubStatsDebounceStart = 0;
        // Debounce (idle) vs max-wait: plugins trickle in as their pluginInfo.json
        // files resolve, but we must not wait for ALL of them -- the first GitHub
        // query should start while the rest are still loading (the backend cache
        // makes the follow-up for stragglers cheap). So the timer is never
        // restarted past GITHUB_STATS_MAX_WAIT from the first pending repo.
        var GITHUB_STATS_DEBOUNCE = 150;
        var GITHUB_STATS_MAX_WAIT = 600;
        function ScheduleGitHubStatsFetch() {
            if (uiLevel < 3 || githubStatsFailed || githubStatsFetching) return;
            // Nothing left to fetch (every known repo already has counts or was
            // already requested)? Stop scheduling; LoadPlugin re-arms this for
            // late-arriving plugins.
            var needs = false;
            for (var repoName in pluginGitHubRepos) {
                if (!pluginGitHubRepos.hasOwnProperty(repoName)) continue;
                var repo = pluginGitHubRepos[repoName];
                if (repo && !pluginGitHubStats[repo] && !githubStatsRequested[repo]) { needs = true; break; }
            }
            if (!needs) return;
            var now = Date.now();
            if (!githubStatsDebounce) githubStatsDebounceStart = now;
            var elapsed = now - githubStatsDebounceStart;
            var wait = GITHUB_STATS_DEBOUNCE;
            if (elapsed >= GITHUB_STATS_MAX_WAIT) {
                wait = 0;
            } else if (elapsed + GITHUB_STATS_DEBOUNCE > GITHUB_STATS_MAX_WAIT) {
                wait = GITHUB_STATS_MAX_WAIT - elapsed;
            }
            clearTimeout(githubStatsDebounce);
            githubStatsDebounce = setTimeout(FetchPluginGitHubStats, wait);
        }

        function FetchPluginGitHubStats() {
            githubStatsDebounce = null;
            githubStatsDebounceStart = 0;
            if (uiLevel < 3 || githubStatsFailed || githubStatsFetching) return;
            var missing = [];
            for (var repoName in pluginGitHubRepos) {
                if (!pluginGitHubRepos.hasOwnProperty(repoName)) continue;
                var repo = pluginGitHubRepos[repoName];
                if (repo && !pluginGitHubStats[repo] && !githubStatsRequested[repo] && missing.indexOf(repo) < 0)
                    missing.push(repo);
            }
            if (missing.length === 0) {
                PatchPluginGitHubStats();
                return;
            }
            // Remember we asked, so a repo the backend can't resolve (offline /
            // dead repo) is not re-requested this session -- its corner simply
            // stays hidden rather than being shown as a bogus 0/0.
            for (var m = 0; m < missing.length; m++)
                githubStatsRequested[missing[m]] = true;
            githubStatsFetching = true;
            $.ajax({
                url: 'api/plugin/githubStats?repos=' + encodeURIComponent(missing.join(',')),
                dataType: 'json',
                success: function (data) {
                    githubStatsFetching = false;
                    var repos = (data && data.repos) || {};
                    for (var k in repos) {
                        if (!repos.hasOwnProperty(k)) continue;
                        pluginGitHubStats[k] = {
                            openIssues: repos[k].openIssues,
                            openPRs: repos[k].openPRs
                        };
                    }
                    PatchPluginGitHubStats();
                    // A plugin that finished loading after this fetch started may
                    // have added a repo; the backend's per-box cache keeps the
                    // follow-up cheap.
                    ScheduleGitHubStatsFetch();
                },
                error: function () {
                    // Network unavailable: leave every corner hidden, latch so
                    // we do not keep firing requests for plugins still loading.
                    githubStatsFetching = false;
                    githubStatsFailed = true;
                }
            });
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
                var $img = $('<img class="pluginIcon pluginIconSm" src="' + EscapeAttr(iconUrl) + '" alt="" loading="lazy">');
                var $fb = $('<div class="pluginIconFallback pluginIconFallbackSm d-none">' + initials + '</div>');
                $img.on('error', function () { $img.addClass('d-none'); $fb.removeClass('d-none'); });
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
                    html += ' v' + EscapeHtml(data.versions[i].minFPPVersion) + ' - v' + EscapeHtml(data.versions[i].maxFPPVersion);
                else if (data.versions[i].minFPPVersion > 0)
                    html += ' > v' + EscapeHtml(data.versions[i].minFPPVersion);
                else if (data.versions[i].maxFPPVersion > 0)
                    html += ' < v' + EscapeHtml(data.versions[i].maxFPPVersion);
                if (data.versions[i].hasOwnProperty('platforms')) {
                    var platforms = data.versions[i].platforms;
                    html += ' ';
                    for (var p = 0; p < platforms.length; p++) {
                        if (p != 0) html += '/';
                        if (platforms[p] == 'Raspberry Pi') html += 'Pi';
                        else if (platforms[p] == 'BeagleBone Black') html += 'BBB';
                        else if (platforms[p] == 'BeagleBone 64') html += 'BB64';
                        else html += EscapeHtml(platforms[p]);
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
                titleIcon += '<div class="pluginIconWrap pluginIconWrapSm d-inline-flex align-middle me-2">';
                titleIcon += '<img class="pluginIcon" src="' + EscapeAttr(iconUrl) + '" alt="" loading="lazy"';
                titleIcon += ' onerror="this.classList.add(\'d-none\');this.nextElementSibling.classList.remove(\'d-none\');">';
                titleIcon += '<div class="pluginIconFallback pluginIconFallbackSm d-none">' + initials + '</div>';
                titleIcon += '</div>';
            } else {
                titleIcon += '<div class="pluginIconWrap pluginIconWrapSm d-inline-flex align-middle me-2"><div class="pluginIconFallback pluginIconFallbackSm">' + initials + '</div></div>';
            }

            var body = '';
            body += '<div class="mb-2">' + PluginBadgesHtml(data, true) + '</div>';
            var authorHtml = PluginAuthorHtml(data);
            if (authorHtml) body += '<div class="mb-2 text-secondary"><i class="fas fa-user"></i> ' + authorHtml + '</div>';
            body += '<p>' + EscapeHtml(data.description) + '</p>';
            // Supported-versions detail is noise for Basic users; show it from Advanced up.
            if (uiLevel >= 1)
                body += '<div class="mb-2 text-muted small"><i class="fas fa-info-circle"></i> Compatible FPP versions: <b>' + PluginVersionsText(data) + '</b></div>';
            if (!installed && compatibleVersion == -1 && untestedVersion >= 0)
                body += '<div class="fpp-inline-warn mb-2"><i class="fas fa-exclamation-triangle"></i>' +
                    '<span>This plugin has not been updated to work with your version of FPP (' + getFPPMajorVersion() + '). You can still install it, but it may not work correctly.</span></div>';
            else if (!installed && compatibleVersion == -1)
                body += '<div class="fpp-major-callout mb-2"><i class="fas fa-exclamation-triangle"></i>' +
                    '<span>No version is compatible with your FPP version/platform.</span></div>';
            body += '<div class="d-flex flex-column gap-1 small">';
            if (IsSafeHttpUrl(data.homeURL)) body += '<a href="' + EscapeAttr(data.homeURL) + '" target="_blank" rel="noopener noreferrer" class="text-decoration-none"><i class="fas fa-home"></i> <span class="text-decoration-underline">' + EscapeHtml(data.homeURL) + '</span></a>';
            // Omit "View Source" when srcURL just duplicates the home link (same repo),
            // ignoring a trailing slash or .git suffix so github.com/x/y(.git)(/) all match.
            var sameLink = function (a, b) {
                var n = function (u) { return (u || '').replace(/\.git$/i, '').replace(/\/+$/, '').toLowerCase(); };
                return a && b && n(a) === n(b);
            };
            if (IsSafeHttpUrl(data.srcURL) && !sameLink(data.srcURL, data.homeURL)) body += '<a href="' + EscapeAttr(data.srcURL) + '" target="_blank" rel="noopener noreferrer" class="text-decoration-none"><i class="fas fa-code"></i> <span class="text-decoration-underline">View Source</span></a>';
            if (IsSafeHttpUrl(data.bugURL)) body += '<a href="' + EscapeAttr(data.bugURL) + '" target="_blank" rel="noopener noreferrer" class="text-decoration-none"><i class="fas fa-bug"></i> <span class="text-decoration-underline">Report a Bug</span></a>';
            body += '</div>';
            // What a plugin declares is one tap away from its card at any
            // time, not only at install. Lines open on tap here too. For a
            // plugin that can be installed from here this is the whole of
            // what the install dialog would show -- the root callout, the
            // block, and a section per dependency plugin the install brings
            // in -- so its Install button installs without a second screen.
            var canInstall = !installed && (compatibleVersion >= 0 || untestedVersion >= 0);
            var disclosure = canInstall ? InstallDisclosureHtml(repo, data, 'dt') : null;
            body += '<div class="mt-2">' + (disclosure ? disclosure.html : PrivacyBlockHtml(PluginPrivacyResult(data), 'dt')) + '</div>';

            var buttons = {};
            if (installed) {
                if (IsSafeHttpUrl(data.pageUrl)) {
                    buttons['Open'] = { class: 'btn-outline-primary', click: function () { CloseModalDialog('pluginDetailDialog'); window.open(data.pageUrl, 'pluginContent'); } };
                }
                buttons['Check for Update'] = { id: 'pluginDetailCheckBtn', class: 'btn-outline-success', click: function () { CheckPluginForUpdates(repo); } };
                buttons['Reinstall'] = { class: 'btn-outline-warning', click: function () { CloseModalDialog('pluginDetailDialog'); ShowReinstallPluginPopup(repo, data.name); } };
                buttons['Uninstall'] = { class: 'btn-outline-danger', click: function () { CloseModalDialog('pluginDetailDialog'); ShowUninstallPluginPopup(repo, data.name); } };
            } else if (canInstall) {
                var idx = compatibleVersion < 0 ? untestedVersion : compatibleVersion;
                // The button the install dialog would have shown (same text
                // and colour, worst finding across plugin and dependencies),
                // and the same post: the block above is what is accepted, and
                // the server's check, refusal and record are as for the
                // dialog (InstallPlugin, InstallStreamDone).
                buttons[disclosure.btn.text] = {
                    class: disclosure.btn.cls,
                    click: function () { CloseModalDialog('pluginDetailDialog'); InstallPlugin(repo, data.versions[idx].branch, data.versions[idx].sha, disclosure.depAccepted); }
                };
            }
            buttons['Close'] = function () { CloseModalDialog('pluginDetailDialog'); };

            // Developer UI: GitHub issue/PR counts at the far left of the footer,
            // vertically level with the action buttons (me-auto pushes the buttons
            // to the right). Passed as the footer's leading content (buttons are
            // appended after it), so it only appears when we have data -- hidden
            // offline.
            var detailStats = '';
            if (uiLevel >= 3) {
                var dBadge = GitHubStatsBadgeHtml(pluginGitHubRepos[repo]);
                if (dBadge) detailStats = '<span class="pluginDetailGitHubStats me-auto">' + dBadge + '</span>';
            }
            pluginDetailDialogRepo = repo;

            DoModalDialog({ id: 'pluginDetailDialog', class: 'modal-lg', title: titleIcon + EscapeHtml(data.name), body: body, backdrop: true, keyboard: true, footer: detailStats, buttons: buttons });
            FPPPluginPrivacy.bind('pluginDetailDialog');
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
        // colors, and purple "Official" the only source one (matching the Dev badge
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
            if (RowEl(data.repoName).length) RowEl(data.repoName).remove();
            var pi = FindPluginInfo(data.repoName);
            if (pi >= 0) pluginInfos[pi] = data; else pluginInfos.push(data);

            var installed = PluginIsInstalled(data.repoName);
            var versionSel = SelectPluginVersionIndices(data);
            var compatibleVersion = versionSel.compatible;
            var untestedVersion = versionSel.untested;
            var isPrivate = (data.private || pluginInfoUseCredentials[data.repoName]);
            var official = IsOfficialPlugin(data);
            // Developer UI: record the plugin's GitHub repo so the issue/PR
            // corner can be filled in. Private repos are skipped (GitHub won't
            // search them, and a private repo would poison the aggregate query).
            if (uiLevel >= 3 && !isPrivate) {
                var ghRepo = GitHubRepoOf(data);
                if (ghRepo) {
                    pluginGitHubRepos[data.repoName] = ghRepo;
                    ScheduleGitHubStatsFetch();
                }
            }
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
                    actions += "<span class='updatesAvailable d-none'>";
                    actions += "<button class='btn btn-sm btn-success' data-plugin-action=\"update\" data-repo=\"" + EscapeAttr(data.repoName) + "\"><i class='far fa-arrow-alt-circle-down'></i> Update</button>";
                    actions += "</span>";
                }
                if (IsSafeHttpUrl(data.pageUrl)) {
                    actions += "<a class='btn btn-sm btn-outline-primary' href=\"" + EscapeAttr(data.pageUrl) + "\" target='pluginContent'><i class='fas fa-external-link-alt'></i> Open</a>";
                }
                actions += "<button class='btn btn-sm btn-outline-warning' data-plugin-action=\"reinstall\" data-repo=\"" + EscapeAttr(data.repoName) + "\"><i class='fas fa-redo-alt'></i> Reinstall</button>";
                actions += "<button class='btn btn-sm btn-outline-danger' data-plugin-action=\"uninstall\" data-repo=\"" + EscapeAttr(data.repoName) + "\"><i class='far fa-trash-alt'></i> Uninstall</button>";
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
                actions += "<button class='btn btn-sm " + btnClass + "' data-plugin-action=\"install\" data-repo=\"" + EscapeAttr(data.repoName) +
                    "\" data-branch=\"" + EscapeAttr(data.versions[idx].branch) + "\" data-sha=\"" + EscapeAttr(data.versions[idx].sha) +
                    "\"><i class='far fa-arrow-alt-circle-down'></i> " + installText + "</button>";
            }

            // Plugin icon / initials avatar
            var iconUrl = GetIconUrl(data, installed);
            var initials = GetInitials(data.name);
            var iconHtml = '';
            if (iconUrl) {
                iconHtml += '<div class="pluginIconWrap">';
                iconHtml += '<img class="pluginIcon" src="' + EscapeAttr(iconUrl) + '" alt="" loading="lazy"';
                iconHtml += ' onerror="this.classList.add(\'d-none\');this.nextElementSibling.classList.remove(\'d-none\');">';
                iconHtml += '<div class="pluginIconFallback d-none">' + initials + '</div>';
                iconHtml += '</div>';
            } else {
                iconHtml += '<div class="pluginIconWrap"><div class="pluginIconFallback">' + initials + '</div></div>';
            }

            var html = '';
            html += '<div id="row-' + EscapeAttr(data.repoName) + '" class="col pluginCard" data-category-slug="' + pcatObj.slug + '" data-sort-rank="' + sortRank + '"';
            if (manuallyLoadedPlugins[data.repoName]) html += ' data-manual="1"';
            html += '>';
            // Card open/action clicks are handled by one delegated listener (see
            // document.ready) reading data-plugin-action/data-repo/etc, rather than
            // inline onclick="Fn(\'...\')" -- pluginInfo.json fields are third-party
            // supplied (see PluginAuthorHtml above), and interpolating them into an
            // onclick attribute's nested JS-string literal is a much easier mistake
            // to get wrong than plain HTML-attribute escaping (EscapeAttr).
            html += '<div class="card h-100 pluginCardInner" role="button" tabindex="0" data-plugin-action="detail" data-repo="' + EscapeAttr(data.repoName) + '">';
            html += '<div class="card-body d-flex flex-column">';
            html += '<div class="d-flex align-items-start gap-3 mb-1">';
            html += iconHtml;
            html += '<div class="min-w-0 flex-grow-1">';
            html += '<h5 class="card-title pluginTitle mb-1">' + EscapeHtml(data.name) + '</h5>';
			html += '<div class="pluginCardBadges">' + badges;
            if (manuallyLoadedPlugins[data.repoName]) {
                html += '<span class="fpp-tag gap-1 pluginManualBadge"><i class="fas fa-link"></i> Manual URL</span>';
            }
            html += '</div>';
            html += '</div></div>';
            var cardAuthorHtml = PluginAuthorHtml(data);
            if (cardAuthorHtml) html += '<div class="text-secondary small mb-1 pluginAuthor"><i class="fas fa-user"></i> ' + cardAuthorHtml + '</div>';
            html += '<p class="card-text pluginCardDesc small flex-grow-1">' + EscapeHtml(data.description) + '</p>';
            // data-plugin-action="none": absorbs clicks on blank space within the
            // actions row so they don't bubble up to the card's own "detail" action
            // above -- the delegated handler finds this (nearer) match first via
            // closest() and stops there, same effect as the old inline
            // event.stopPropagation() without needing it on every button.
            html += '<div class="pluginCardActions d-flex flex-wrap gap-2 mt-2 align-items-center" data-plugin-action="none">' +
                actions + PrivacyRowHtml(data) + GitHubStatsRowHtml(pluginGitHubRepos[data.repoName]) + '</div>';
            html += '</div></div></div>';

            if (installed) {
                InsertCardSorted('installedGrid', data.name, html, sortRank);
            } else {
                // Basic hides plugins whose declared requirements exceed this device.
                // Advanced/Developer still see them, with an advisory badge and
                // an install confirmation. Coarse "heavy" profiles never hide.
                if (uiLevel < 1 && PluginResourceVerdict(data).exceeds) return;
                if (data.repoName == 'fpp-plugin-Template') {
                    if (uiLevel < 3) return;
                } else if (compatibleVersion != -1) {
                    // compatible: shown at all UI levels
                } else if (untestedVersion >= 0) {
                    if (uiLevel < 1) return;
                } else {
					if (uiLevel < 1) return;
                    InsertCardSorted('incompatibleGrid', data.name, html, sortRank);
                    var $wrap = $('#incompatiblePluginsWrap');
                    $('#incompatiblePluginsCount').text($wrap.find('.pluginCard').length);
                    $wrap.removeClass('d-none');
                    FilterPlugins();
                    ScheduleSettle();
                    return;
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
                            // A pluginInfo.json's own repoName is not always the
                            // name it is listed under in pluginList.json (e.g.
                            // "TwilioControl" vs "FPP-Plugin-TwilioControl"), and
                            // the plugin list name is the only one the server can look
                            // an icon up by, or resolve a dependency by. Record the
                            // pairing, for installed plugins too (a dependency
                            // declared by its listed name is then seen as installed).
                            if (data && data.repoName)
                                pluginListKeyOf[data.repoName] = pluginList[index][0];
                            if (data && data.repoName && PluginIsInstalled(data.repoName)) {
                                return;
                            }
                            if (pluginList[index] && pluginList[index].length > 2 && pluginList[index][2])
                                data.__category = pluginList[index][2];
                            LoadPlugin(data);
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

        // Bound once in document.ready -- previously this was (re)bound inside
        // LoadPlugins' per-plugin AJAX success callback, so with N listed
        // plugins the same handler ended up attached N times to the same input,
        // and every keystroke re-ran this whole body N times.
        function HandlePluginInputChange() {
            var val = $('#pluginInput').val() || '';
            pluginUrlError = '';
            if (val.length > 0) {
                $('#pluginInput').addClass('has-text');
                $('#pluginClearBtn').css('display', 'block');
            } else {
                $('#pluginInput').removeClass('has-text');
                $('#pluginClearBtn').css('display', '');
            }
            if (uiLevel >= 3 && /plugininfo\.json$/i.test(val)) {
                if (val !== lastAutoLoadedUrl) {
                    if (urlLoadedRepo) {
                        RowEl(urlLoadedRepo).remove();
                        delete manuallyLoadedPlugins[urlLoadedRepo];
                        urlLoadedRepo = null;
                    }
                    lastAutoLoadedUrl = val;
                    ManualLoadInfo(true);
                }
            } else if (urlLoadedRepo) {
                RowEl(urlLoadedRepo).remove();
                delete manuallyLoadedPlugins[urlLoadedRepo];
                urlLoadedRepo = null;
                lastAutoLoadedUrl = '';
            }
            FilterPlugins();
        }

        function ClearPluginInput() {
            $('#pluginInput').val('').removeClass('has-text');
            $('#pluginClearBtn').css('display', '');
            pluginUrlError = '';
            if (urlLoadedRepo) {
                RowEl(urlLoadedRepo).remove();
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
                    RowEl(data.repoName)[0].scrollIntoView({ behavior: 'smooth', block: 'center' });
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

            var isUrlInput = uiLevel >= 3 && /plugininfo\.json$/i.test(raw);
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
            // installedMatchingSearch is the search-filtered total for the Installed tab
            // badge (independent of which manage-tab is active); installedVisible is the
            // count actually visible in the current manage pane (updates-tab filters to
            // hasUpdate). updateVisible is the total pending-updates count (not search-
            // filtered) for the Updates badge. Separating installedMatchingSearch from
            // installedVisible prevents the Installed badge from dropping to 0 when the
            // Updates tab is selected.
            var installedVisible = 0, installedMatchingSearch = 0, updateVisible = 0;
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
                if (matchesSearch) installedMatchingSearch++;
                if (hasUpdate) updateVisible++;
                var matchesTab = (activeTopTab !== 'updates') || hasUpdate;
                var vis = matchesSearch && matchesTab;
                $(this).toggleClass('d-none', !vis);
                if (vis) installedVisible++;
            });
            if (activeTopTab === 'updates') $('#noUpdatesHint').toggleClass('d-none', installedVisible > 0);
            else $('#noUpdatesHint').addClass('d-none');

            // Incompatible cards -- same search matching as Available/Installed,
            // so the section doesn't sit there unfiltered (and misleadingly
            // prominent) once a search has hidden everything else.
            var incompatibleVisible = 0;
            $('#incompatibleGrid').children('.pluginCard').each(function () {
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
                $(this).toggleClass('d-none', !matchesSearch);
                if (matchesSearch) incompatibleVisible++;
            });
            var incompatibleTotal = $('#incompatibleGrid').children('.pluginCard').length;
            $('#incompatiblePluginsWrap').toggleClass('d-none', incompatibleTotal === 0 || incompatibleVisible === 0);
            $('#incompatiblePluginsCount').text(incompatibleVisible);

            // Update All is only useful once a check has actually found something
            // to update -- keep it out of the way otherwise, at every UI level.
            $('#updateAllBtn').toggleClass('d-none', updateVisible === 0);

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
                $('#noUrlResults').removeClass('d-none');
                $('#noUrlSchemeResults').addClass('d-none');
            } else {
                var showAvailEmpty = searching && activeTopTab === 'available' && availVisible === 0;
                $('#noAvailableResults').toggleClass('d-none', !showAvailEmpty);
                $('#noUrlResults').addClass('d-none');
                $('#noUrlSchemeResults').addClass('d-none');
                if (showAvailEmpty && installedMatchingSearch > 0) {
                    $('#noAvailCrossRef').text('Found ' + installedMatchingSearch + ' plugin' + (installedMatchingSearch === 1 ? '' : 's') + ' that match on the Installed list. ');
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
            $('#topCountInstalled').text(installedMatchingSearch);
            $('#topCountUpdates').text(updateVisible);
        }
        $(document).ready(function () {
            // Firefox restores input values on reload regardless of autocomplete="off",
            // which would leave the list filtered by a term the user can't see a reason for.
            $('#pluginInput').val('');
            // pluginInfo.json URL entry is a developer-only escape hatch; everyone
            // else just gets the plain search box.
            if (uiLevel < 3) {
                $('#pluginInput').attr('placeholder', 'Find a Plugin');
            }

            // Uninstall All and Reinstall All are visible at every UI level (each is
            // already gated by its own "this cannot be undone" confirmation dialog,
            // and Reinstall All is already reachable at any UI level via the
            // post-FPPOS-upgrade auto-open flow, MaybeAutoOpenReinstallAll). Update
            // All's own visibility is handled separately in FilterPlugins() -- shown
            // only once a check has actually found an update.
            $('#pluginTopTabs .nav-link').on('click', function () {
                ShowTopTab($(this).attr('data-top-tab'));
                this.scrollIntoView({ block: 'nearest', inline: 'center' });
            });
            $('#pluginClearBtn').on('click', ClearPluginInput);
            $('#pluginInput').on('input', HandlePluginInputChange);

            // Single delegated handler for every plugin-card click (open detail /
            // update / uninstall / install). Cards are built as HTML strings (see
            // LoadPlugin) carrying data-plugin-action/data-repo/etc rather than
            // per-button onclick="Fn('...')" attributes built from those same
            // pluginInfo.json-supplied values -- data-* attributes only ever need
            // plain HTML-attribute escaping (EscapeAttr) to be safe, whereas an
            // onclick string additionally has to be escaped for the JS-string
            // literal nested inside it, which is easy to get wrong and was
            // previously done for none of these sites. One listener here also means
            // newly-inserted/replaced cards need no rebinding.
            document.addEventListener('click', function (e) {
                var el = e.target.closest('[data-plugin-action]');
                if (!el) return;
                e.stopPropagation();
                var action = el.dataset.pluginAction;
                var repo = el.dataset.repo;
                if (action === 'detail') ShowPluginDetail(repo);
                else if (action === 'update') UpgradePlugin(repo);
                else if (action === 'uninstall') ShowUninstallPluginPopup(repo);
                else if (action === 'reinstall') ShowReinstallPluginPopup(repo);
                else if (action === 'install') ConfirmAndInstall(repo, el.dataset.branch, el.dataset.sha);
                // 'none' (blank space in a card's actions row): absorb the click,
                // do nothing -- same effect the old event.stopPropagation() had.
            });

            BindPopularStripControls();
            GetPluginPopularity();   // parallel with the list/installed loads
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

                    <!-- BP-09. The install dialogs below cover third-party and
                         URL-pasted plugins, but an Official plugin installs with no
                         dialog at all, so the one fact that is true of EVERY plugin
                         has to be stated somewhere that is always on screen. It is
                         not a warning about any particular plugin and deliberately
                         does not read like one. -->
                    <div class="alert alert-secondary small py-2 mb-3" id="pluginPrivilegeNote">
                        <i class="fas fa-circle-info"></i>
                        Every plugin, official or not, runs with <b>root privileges</b> on this
                        player &mdash; it can read and change any setting, including the privacy
                        settings, and reach anything else on the network FPP is connected to.
                    </div>

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

                            <div id="incompatiblePluginsWrap" class="mt-4 d-none">
                                <details>
                                    <summary class="text-secondary">
                                        <i class="fas fa-exclamation-triangle"></i> Incompatible Plugins (<span id="incompatiblePluginsCount">0</span>)
                                    </summary>
                                    <div class="callout mt-2">
                                        These plugins are not compatible with this version of FPP.
                                    </div>
                                    <div id='incompatibleGrid' class="row row-cols-1 row-cols-md-2 row-cols-xxl-3 g-3 mt-1"></div>
                                </details>
                            </div>
                        </div>

                        <div id="pane-manage" class="pluginTopPane d-none">
                            <div class='pluginsHeader'>
                                <h2 id="manageHeading"><i class="fas fa-check-circle text-secondary"></i> Installed Plugins</h2>
                                <div class="d-flex flex-wrap gap-2 align-items-center">
                                    <button id="checkAllUpdatesBtn" class="buttons btn-outline-success"
                                        onClick='CheckAllPluginsForUpdates();'
                                        title="Check all installed plugins for updates">
                                        <i class='fas fa-sync-alt'></i> Check for Updates
                                    </button>
                                    <button id="updateAllBtn" class="buttons btn-outline-primary d-none"
                                        onClick='UpdateAllPlugins();'
                                        title="Check for and update all installed plugins that have an update available">
                                        <i class='far fa-arrow-alt-circle-down'></i> Update All
                                    </button>
                                    <button id="reinstallAllBtn" class="buttons btn-outline-warning"
                                        onClick='ShowReinstallAllPluginsPopup();'
                                        title="Uninstall and reinstall all installed plugins">
                                        <i class='fas fa-redo-alt'></i> Reinstall All
                                    </button>
                                    <button id="uninstallAllBtn" class="buttons btn-outline-danger"
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
                            <div id="noUpdatesHint" class="text-secondary d-none">No updates found. Use <b>Check for Updates</b> to refresh.</div>
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
