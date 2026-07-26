<!DOCTYPE html>
<html lang="en">

<head>
    <?php
    include 'common/htmlMeta.inc';
    require_once "config.php";
    require_once 'common.php';
    include 'common/menuHead.inc';

    function normalize_version($version)
    {
        $version = preg_replace('/[v]/', '', $version);
        $version = preg_replace('/-.*/', '', $version);
        $parts = explode('.', $version);
        while (count($parts) < 3) {
            array_push($parts, "0");
        }
        $number = 0;
        foreach ($parts as $part) {
            $val = intval($part);
            if ($val > 99) {
                $val = 99;
            }
            $number = $number * 100 + $val;
        }
        return $number;
    }

    $rfs_ver = normalize_version(getFPPVersionTriplet());

    $remoteScripts = [];
    $nonMatchingScripts = [];
    $categorySet = [];

    $indexCSV = @file_get_contents("https://raw.githubusercontent.com/FalconChristmas/fpp-scripts/master/index.csv");
    if ($indexCSV !== false) {
        $lines = explode("\n", $indexCSV);
        foreach ($lines as $line) {
            if (preg_match("/^#/", $line)) {
                continue;
            }
            $parts = explode(',', $line);
            if (count($parts) < 4) {
                continue;
            }
            $script = [
                'category' => trim($parts[0]),
                'filename' => trim($parts[1]),
                'description' => trim($parts[2]),
                'minVersion' => trim($parts[3]),
                'maxVersion' => isset($parts[4]) ? trim($parts[4]) : ''
            ];
            $compatible = true;
            if (normalize_version($script['minVersion']) > $rfs_ver) {
                $compatible = false;
            }
            if (strlen($script['maxVersion']) > 0 && normalize_version($script['maxVersion']) <= $rfs_ver) {
                $compatible = false;
            }
            if ($compatible) {
                $remoteScripts[] = $script;
                $categorySet[$script['category']] = true;
            } else {
                $nonMatchingScripts[] = $script;
            }
        }
    }

    $scriptCategories = [];
    $catKeys = array_keys($categorySet);
    sort($catKeys);
    foreach ($catKeys as $cat) {
        $scriptCategories[] = ['name' => $cat, 'slug' => strtolower(preg_replace('/[^a-zA-Z0-9]+/', '-', $cat))];
    }
    ?>
    <title><? echo $pageTitle; ?></title>
    <script>
        var remoteScripts = <?= json_encode($remoteScripts); ?>;
        var nonMatchingScripts = <?= json_encode($nonMatchingScripts); ?>;
        var scriptCategories = <?= json_encode($scriptCategories); ?>;
        var installedScripts = [];
        var activeCategorySlug = 'all';
        var activeTopTab = 'available';

        function ViewRemoteScript(category, filename) {
            DoModalDialog({
                id: "ScriptViewerDialog",
                title: "Script Viewer",
                body: "<div id='scriptViewerText' class='fileText'>Loading...</div>",
                class: "modal-dialog-scrollable",
                backdrop: true,
                keyboard: true,
                buttons: {
                    "Close": function () { CloseModalDialog("ScriptViewerDialog"); }
                }
            });

            $.get("api/scripts/viewRemote/" + encodeURIComponent(category) + "/" + encodeURIComponent(filename)
            ).done(function (data) {
                $('#scriptViewerText').html("<pre>" + data.replace(/</g, '&lt;').replace(/>/g, '&gt;') + "</pre>");
            }).fail(function () {
                $('#scriptViewerText').html("Error loading script contents from repository.");
            });
        }

        function InstallRemoteScript(category, filename) {
            $.get("api/scripts/installRemote/" + encodeURIComponent(category) + "/" + encodeURIComponent(filename)
            ).done(function () {
                $.jGrowl("Script installed.", { themeState: 'success' });
                var idx = installedScripts.indexOf(filename);
                if (idx < 0) {
                    installedScripts.push(filename);
                }
                RenderInstalledScripts();
                RenderAvailableScripts();
                FilterScripts();
            }).fail(function () {
                DialogError("Install Script", "Install Failed");
            });
        }

        function ViewLocalScript(filename) {
            DoModalDialog({
                id: "ScriptViewerDialog",
                title: "Script Viewer",
                body: "<div id='scriptViewerText' class='fileText'>Loading...</div>",
                class: "modal-dialog-scrollable",
                backdrop: true,
                keyboard: true,
                buttons: {
                    "Close": function () { CloseModalDialog("ScriptViewerDialog"); }
                }
            });

            $.get("api/scripts/" + encodeURIComponent(filename)
            ).done(function (data) {
                $('#scriptViewerText').html("<pre>" + data.replace(/</g, '&lt;').replace(/>/g, '&gt;') + "</pre>");
            }).fail(function () {
                $('#scriptViewerText').html("Error loading script contents.");
            });
        }

        function GetInstalledScripts() {
            $.ajax({
                url: 'api/scripts',
                dataType: 'json',
                success: function (data) {
                    installedScripts = data || [];
                    RenderInstalledScripts();
                    RenderAvailableScripts();
                    RestoreTopTab();
                    FilterScripts();
                },
                error: function () {
                    installedScripts = [];
                    RenderInstalledScripts();
                    RenderAvailableScripts();
                    RestoreTopTab();
                    FilterScripts();
                }
            });
        }

        function RenderAvailableScripts() {
            var $grid = $('#scriptGrid');
            $grid.empty();
            for (var i = 0; i < remoteScripts.length; i++) {
                var s = remoteScripts[i];
                var catSlug = s.category.toLowerCase().replace(/[^a-z0-9]+/g, '-');
                var isInstalled = installedScripts.indexOf(s.filename) >= 0;
                var $col = $('<div class="col scriptCard" data-category-slug="' + catSlug + '"></div>');
                var $card = $('<div class="card h-100 scriptCardInner"></div>');
                var $body = $('<div class="card-body d-flex flex-column p-3"></div>');

                var $header = $('<div class="mb-1"></div>');
                var $title = $('<h6 class="card-title fw-semibold mb-0 font-monospace scriptCardTitle min-w-0"></h6>').text(s.filename);
                var $catTag = $('<span class="fpp-tag mt-1 d-inline-block"></span>').text(s.category);
                $header.append($title).append($catTag);

                var $desc = $('<p class="card-text small text-secondary flex-grow-1 mb-2"></p>').text(s.description);

                var $actions = $('<div class="d-flex flex-wrap gap-2"></div>');
                $actions.append(
                    $('<button class="btn btn-sm btn-outline-primary"><i class="fas fa-eye"></i> View</button>')
                        .on('click', function (cat, fn) {
                            return function () { ViewRemoteScript(cat, fn); };
                        }(s.category, s.filename))
                );
                if (isInstalled) {
                    $actions.append(
                        $('<button class="btn btn-sm btn-secondary" disabled><i class="fas fa-check"></i> Installed</button>')
                    );
                } else {
                    $actions.append(
                        $('<button class="btn btn-sm btn-success"><i class="fas fa-download"></i> Install</button>')
                            .on('click', function (cat, fn) {
                                return function () { InstallRemoteScript(cat, fn); };
                            }(s.category, s.filename))
                    );
                }

                $body.append($header).append($desc).append($actions);
                $card.append($body);
                $col.append($card);
                $grid.append($col);
            }
        }

        function RenderInstalledScripts() {
            var $grid = $('#installedScriptGrid');
            $grid.empty();
            if (!installedScripts || installedScripts.length === 0) {
                $('#noInstalledScripts').removeClass('d-none');
                return;
            }
            $('#noInstalledScripts').addClass('d-none');
            for (var i = 0; i < installedScripts.length; i++) {
                var filename = installedScripts[i];
                var meta = null;
                for (var j = 0; j < remoteScripts.length; j++) {
                    if (remoteScripts[j].filename === filename) {
                        meta = remoteScripts[j];
                        break;
                    }
                }
                var $col = $('<div class="col scriptCard"></div>');
                var $card = $('<div class="card h-100 scriptCardInner"></div>');
                var $body = $('<div class="card-body d-flex flex-column p-3"></div>');

                var $title = $('<h6 class="card-title fw-semibold mb-1 font-monospace"></h6>').text(filename);
                $body.append($title);

                if (meta) {
                    var $desc = $('<p class="card-text small text-secondary flex-grow-1 mb-2"></p>').text(meta.description);
                    $body.append($desc);
                } else {
                    $body.append($('<p class="card-text small text-secondary flex-grow-1 mb-2"><i>Local script - not from the repository</i></p>'));
                }

                var $actions = $('<div class="d-flex flex-wrap gap-2"></div>');
                $actions.append(
                    $('<button class="btn btn-sm btn-outline-primary"><i class="fas fa-eye"></i> View</button>')
                        .on('click', function (fn) {
                            return function () { ViewLocalScript(fn); };
                        }(filename))
                );
                if (meta) {
                    $actions.append(
                        $('<button class="btn btn-sm btn-success"><i class="fas fa-redo-alt"></i> Reinstall</button>')
                            .on('click', function (cat, fn) {
                                return function () { InstallRemoteScript(cat, fn); };
                            }(meta.category, filename))
                    );
                }
                $body.append($actions);
                $card.append($body);
                $col.append($card);
                $grid.append($col);
            }
        }

        function BuildCategoryPills() {
            var $pills = $('#scriptCategoryPills');
            if (!$pills.length) return;
            $pills.empty();
            var allPill = {
                name: 'All',
                slug: 'all',
                icon: 'fas fa-border-all'
            };
            var pillList = [allPill];
            for (var i = 0; i < scriptCategories.length; i++) {
                pillList.push({
                    name: scriptCategories[i].name,
                    slug: scriptCategories[i].slug,
                    icon: 'fas fa-tag'
                });
            }
            activeCategorySlug = 'all';
            for (var j = 0; j < pillList.length; j++) {
                var c = pillList[j];
                var li = $('<li class="nav-item" role="presentation"></li>');
                var btn = $('<button type="button" role="tab" class="nav-link text-nowrap"></button>');
                if (c.slug === activeCategorySlug) btn.addClass('active');
                btn.attr('data-category-slug', c.slug);
                btn.html('<i class="' + c.icon + '"></i> ' + c.name +
                    ' <span class="badge bg-secondary ms-1 fppCatCount" data-count-slug="' + c.slug + '">0</span>');
                btn.on('click', function () {
                    $('#scriptCategoryPills .nav-link').removeClass('active');
                    $(this).addClass('active');
                    activeCategorySlug = $(this).attr('data-category-slug');
                    this.scrollIntoView({ block: 'nearest', inline: 'center' });
                    FilterScripts();
                });
                li.append(btn);
                $pills.append(li);
            }
            FilterScripts();
        }

        function ShowTopTab(name) {
            activeTopTab = name;
            try { sessionStorage.setItem('scriptTopTab', name); } catch (e) { }
            $('#scriptTopTabs .nav-link').removeClass('active');
            $('#scriptTopTabs .nav-link[data-top-tab="' + name + '"]').addClass('active');
            $('#pane-available').toggleClass('d-none', name !== 'available');
            $('#pane-installed').toggleClass('d-none', name !== 'installed');
            FilterScripts();
        }

        function RestoreTopTab() {
            var saved = '';
            try { saved = sessionStorage.getItem('scriptTopTab') || ''; } catch (e) { }
            if (saved === 'installed')
                ShowTopTab(saved);
        }

        function FilterScripts() {
            var value = ($('#ScriptSearchInput').val() || '').toLowerCase();
            var searching = value !== '';

            var counts = {}, total = 0, availVisible = 0;
            $('#scriptGrid').children('.scriptCard').each(function () {
                var slug = $(this).attr('data-category-slug') || 'other';
                var titleText = $(this).find('.scriptCardTitle').text().toLowerCase();
                var descText = $(this).find('.card-text').text().toLowerCase();
                var haystack = titleText + ' ' + descText;
                var matchesSearch = !searching || haystack.indexOf(value) > -1;
                var matchesCat = searching || activeCategorySlug === 'all' || slug === activeCategorySlug;
                var show = matchesSearch && matchesCat;
                $(this).toggleClass('d-none', !show);
                if (show) availVisible++;
                if (matchesSearch) {
                    counts[slug] = (counts[slug] || 0) + 1;
                    total++;
                }
            });

            var installedVisible = 0;
            $('#installedScriptGrid').children('.scriptCard').each(function () {
                var titleText = $(this).find('.card-title').text().toLowerCase();
                var haystack = titleText;
                var descText = $(this).find('.card-text').text().toLowerCase();
                if (descText) haystack += ' ' + descText;
                var matchesSearch = !searching || haystack.indexOf(value) > -1;
                var vis = matchesSearch;
                $(this).toggleClass('d-none', !vis);
                if (vis) installedVisible++;
            });

            var showNoAvail = searching && availVisible === 0 && remoteScripts.length > 0;
            $('#noAvailableResults').toggleClass('d-none', !showNoAvail);
            var showNoInstalled = searching && installedVisible === 0 && installedScripts.length > 0;
            $('#noInstalledResults').toggleClass('d-none', !showNoInstalled);
            if (showNoAvail) {
                if (installedVisible > 0) {
                    $('#noAvailCrossRef').text('Found ' + installedVisible + ' script' + (installedVisible === 1 ? '' : 's') + ' that match on the Installed tab. ');
                } else {
                    $('#noAvailCrossRef').text('');
                }
            }
            if (showNoInstalled) {
                if (availVisible > 0) {
                    $('#noInstalledCrossRef').text('Found ' + availVisible + ' script' + (availVisible === 1 ? '' : 's') + ' that match on the Available tab.');
                } else {
                    $('#noInstalledCrossRef').text('');
                }
            }
            $('.fppNoResultsTerm').text(value);

            $('#scriptCategoryPills .fppCatCount').each(function () {
                var s = $(this).attr('data-count-slug');
                var val = (s === 'all') ? total : (counts[s] || 0);
                $(this).text(val);
                var $li = $(this).closest('.nav-item');
                if (s !== 'all' && val === 0) $li.addClass('d-none'); else $li.removeClass('d-none');
            });

            $('#topCountAvailable').text(availVisible);
            $('#topCountInstalled').text(installedVisible);
        }

        function pageSpecific_PageLoad_PostDOMLoad_ActionsSetup() {
            BuildCategoryPills();
            RenderAvailableScripts();
            GetInstalledScripts();
            $("#ScriptSearchInput").on("keyup", function () {
                FilterScripts();
            });
            $('#scriptTopTabs .nav-link').on('click', function () {
                ShowTopTab($(this).attr('data-top-tab'));
                this.scrollIntoView({ block: 'nearest', inline: 'center' });
            });
        }
    </script>
    <style>
        #scriptTopTabs .nav-link {
            border-top-left-radius: 12px;
            border-top-right-radius: 12px;
        }
        #scriptTopTabs {
            border-bottom-width: 3px;
        }
        .scriptCardInner {
            cursor: default;
        }
        .scriptCardInner .btn {
            cursor: pointer;
        }
        @media (max-width: 575.98px) {
            .scriptsHeader {
                flex-direction: column-reverse;
                align-items: flex-start;
                gap: 0.5rem;
            }
        }
    </style>
</head>

<body>
    <div id="bodyWrapper">
        <?php
        $activeParentMenuItem = 'content';
        include 'menu.inc'; ?>
        <div class="mainContainer">
            <h1 class="title">Script Repository</h1>
            <div class="pageContent">
                <div id="scripts" class="settings">

                    <div class="row align-items-lg-center g-2 mb-3">
                        <div class="col-12 col-lg-4 order-lg-2 d-lg-flex">
                            <div class="row fppScriptSearch gx-2 flex-grow-1 align-items-center">
                                <div class="col d-flex position-relative">
                                    <input type="text" id="ScriptSearchInput" autocomplete="off"
                                        class="form-control form-control-rounded has-shadow flex-grow-1"
                                        placeholder="Search for a script..." />
                                </div>
                            </div>
                        </div>
                        <div class="col-12 col-lg order-lg-1">
                            <ul class="nav nav-tabs flex-nowrap flex-md-wrap overflow-x-auto overflow-y-hidden" id="scriptTopTabs" role="tablist">
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
                            </ul>
                        </div>
                    </div>

                    <div id="pane-available" class="scriptTopPane">
                        <div class="fppScriptAvailableHead">
                            <h2 class="h5 mb-2"><i class="fas fa-tags text-secondary"></i> Categories</h2>
                            <ul class="nav nav-pills mb-3 pageContent-tabs flex-nowrap flex-md-wrap overflow-x-auto gap-1 pb-1" id="scriptCategoryPills" role="tablist"></ul>
                        </div>

                        <div id='scriptTable'>
                            <h2 class="h5 mb-2"><i class="fas fa-box text-secondary"></i> Available Scripts</h2>
                            <div id='scriptGrid' class="row row-cols-1 row-cols-md-2 row-cols-xxl-3 g-3"></div>
                            <div id="noAvailableResults" class="alert alert-info d-none mt-2">
                                <i class="fas fa-search"></i> No scripts match
                                "<b class="fppNoResultsTerm"></b>". <span id="noAvailCrossRef"></span>Clear the search box to see all scripts.
                            </div>
                        </div>

                        <?php if ($settings["uiLevel"] >= 1 && !empty($nonMatchingScripts)): ?>
                        <div class="mt-4">
                            <details>
                                <summary class="text-secondary">
                                    <i class="fas fa-exclamation-triangle"></i> Incompatible Scripts (<?= count($nonMatchingScripts) ?>)
                                </summary>
                                <div class="callout mt-2">
                                    These scripts are from other versions of FPP and are not compatible with this version.
                                </div>
                                <div id='fppScriptsNonMatching' class="row row-cols-1 row-cols-md-2 row-cols-xxl-3 g-3 mt-1">
                                    <? foreach ($nonMatchingScripts as $ns): ?>
                                    <div class="col">
                                        <div class="card h-100 scriptCardInner bg-transparent border-muted">
                                            <div class="card-body d-flex flex-column p-3">
                                                <h6 class="card-title fw-semibold mb-1 font-monospace"><?= $ns['filename'] ?></h6>
                                                <p class="card-text small text-secondary flex-grow-1 mb-2"><?= $ns['description'] ?></p>
                                                <div>
                                                    <button class="btn btn-sm btn-outline-secondary" onclick='ViewRemoteScript("<?= $ns['category'] ?>", "<?= $ns['filename'] ?>");'>
                                                        <i class="fas fa-eye"></i> View
                                                    </button>
                                                </div>
                                            </div>
                                        </div>
                                    </div>
                                    <? endforeach; ?>
                                </div>
                            </details>
                        </div>
                        <?php endif; ?>

                        <div class="callout mt-3">
                            <b>NOTE:</b> Some scripts such as the Remote Control scripts may require editing to configure
                            variables to be functional. After installing a script from the Script Repository, you can view
                            and download the script from the Scripts tab in the <a href='filemanager.php'>File Manager</a>
                            screen.
                        </div>
                    </div>

                    <div id="pane-installed" class="scriptTopPane d-none">
                        <h2 class="h5 mb-2"><i class="far fa-check-circle text-secondary"></i> Installed Scripts</h2>
                        <div id='installedScriptGrid' class="row row-cols-1 row-cols-md-2 row-cols-xxl-3 g-3"></div>
                        <div id="noInstalledScripts" class="d-none">
                            <p class="text-secondary">No scripts are currently installed. Browse the <b>Available</b> tab to find and install scripts.</p>
                        </div>
                        <div id="noInstalledResults" class="alert alert-info d-none mt-2">
                            <i class="fas fa-search"></i> No installed scripts match
                            "<b class="fppNoResultsTerm"></b>". <span id="noInstalledCrossRef"></span>
                        </div>
                    </div>

                </div>
            </div>
        </div>
        <?php include 'common/footer.inc'; ?>
    </div>

</body>

</html>
