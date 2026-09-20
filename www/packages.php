<!DOCTYPE html>
<html lang="en">

<head>
    <?php
    include 'common/htmlMeta.inc';
    require_once('config.php');
    require_once('common.php');
    include('common/menuHead.inc');
    require_once('common/packages.inc.php');

    // Build the list of managed packages with their requesters. LoadUserPackages
    // normalizes both the legacy string[] schema and the new object schema into
    // package => [requesters].
    // 'via' lists the package(s) whose install pulled this one in as a
    // dependency; the list below folds those under their parent unless the
    // UI level is Developer, where the whole chain is shown.
    $userPackages = [];
    foreach (LoadUserPackages() as $pkgName => $requesters) {
        $userPackages[] = ['name' => $pkgName, 'requesters' => array_values($requesters), 'via' => PackageVia($pkgName)];
    }

    writeFPPVersionJavascriptFunctions();
    ?>
    <style>
        .taller-modal .modal-dialog {
            max-height: 90%;
            height: 90%;
            overflow-y: auto;
        }

        .taller-modal .modal-body {
            max-height: calc(100% - 120px);
            overflow-y: auto;
        }

        /* Limit autocomplete dropdown height to prevent browser lockup when
           filtering 100k+ packages — short terms like "li" or "py" can match
           thousands; rendering all at once freezes the DOM. */
        .ui-autocomplete {
            max-height: 250px;
            overflow-y: auto;
            overflow-x: hidden;
        }

        /* FPP's Bootstrap build leaves out the tables component, so .table /
           .table-sm / .table-responsive do nothing here; the managed-packages
           table carries its own rules, themed through Bootstrap's variables. */
        #userPackagesTable {
            width: 100%;
            border-collapse: collapse;
            margin-bottom: 1.5rem;
        }
        #userPackagesTable th,
        #userPackagesTable td {
            padding: 0.45rem 0.75rem;
            border-bottom: 1px solid var(--bs-border-color);
            vertical-align: middle;
            text-align: left;
        }
        #userPackagesTable th {
            font-weight: 600;
            color: var(--bs-secondary-color);
            border-bottom-width: 2px;
            white-space: nowrap;
        }
        #userPackagesTable tbody tr:hover {
            background-color: var(--bs-tertiary-bg);
        }
        #userPackagesTable td.pkg-name {
            font-family: var(--bs-font-monospace);
            font-size: 0.95em;
            white-space: nowrap;
        }
        #userPackagesTable tr.pkg-dep td.pkg-name {
            padding-left: 2.25rem;
        }
        #userPackagesTable .pkg-note {
            font-family: var(--bs-font-sans-serif);
            font-size: 0.85em;
            color: var(--bs-secondary-color);
        }
        #userPackagesTable .pkg-status,
        #userPackagesTable .pkg-actions {
            width: 1%;
            white-space: nowrap;
        }
        #userPackagesTable td.pkg-actions {
            text-align: right;
        }
        #userPackagesTable td.pkg-actions .btn {
            min-width: 7rem;
        }
        #userPackagesTable td.pkg-actions > * + * {
            margin-left: 0.5rem;
        }
        /* Phone: stack each row as a block. The name line keeps its status
           badge; the requester and the buttons get their own lines, so the
           actions are never scrolled off to the right. */
        @media (max-width: 767.98px) {
            #userPackagesTable thead {
                display: none;
            }
            #userPackagesTable tr {
                display: block;
                padding: 0.5rem 0;
                border-bottom: 1px solid var(--bs-border-color);
            }
            #userPackagesTable td {
                display: block;
                border: 0;
                padding: 0.15rem 0.5rem;
            }
            #userPackagesTable tr.pkg-dep td.pkg-name {
                padding-left: 1.75rem;
            }
            #userPackagesTable td.pkg-status {
                display: inline-block;
                padding-left: 0.5rem;
            }
            #userPackagesTable td.pkg-name {
                display: inline-block;
                white-space: normal;
            }
            #userPackagesTable .pkg-dep-note {
                display: block;
                padding-left: 1.4rem;
            }
            #userPackagesTable td.pkg-req::before {
                content: "Required by: ";
                color: var(--bs-secondary-color);
                font-size: 0.85em;
            }
            #userPackagesTable td.pkg-status,
            #userPackagesTable td.pkg-actions {
                width: auto;
            }
            #userPackagesTable td.pkg-actions {
                text-align: left;
                white-space: nowrap;
                padding-top: 0.4rem;
            }
            #userPackagesTable td.pkg-actions .btn {
                min-width: 0;
            }
        }
    </style>
    <script>
        var systemPackages = [];
        var userInstalledPackages = <?php echo json_encode($userPackages); ?>;
        // 3 = Developer: show dependency rows under their parent. Below that
        // they are folded into a "(+N dependencies)" note on the parent.
        var showDependencyChain = <?php echo ((int) $uiLevel >= 3) ? 'true' : 'false'; ?>;
        var selectedPackageName = "";

        function ShowLoadingIndicator() {
            $('#loadingIndicator').show();
            $('#packageInputContainer').hide();
        }

        function HideLoadingIndicator() {
            $('#loadingIndicator').hide();
            $('#packageInputContainer').show();
        }

        function GetSystemPackages() {
            ShowLoadingIndicator();
            $.ajax({
                url: '/api/system/packages',
                type: 'GET',
                dataType: 'json',
                success: function (data) {
                    if (!data || !Array.isArray(data)) {
                        console.error('Invalid data received from server.', data);
                        alert('Error: Unable to retrieve package list.');
                        return;
                    }
                    systemPackages = data;
                    InitializeAutocomplete();
                    HideLoadingIndicator();
                },
                error: function () {
                    alert('Error, failed to get system packages list.');
                    HideLoadingIndicator();
                }
            });
        }

        function InitializeAutocomplete() {
            if (!systemPackages.length) {
                console.warn('System packages list is empty.');
                return;
            }

            $("#packageInput").autocomplete({
                minLength: 2,
                delay: 300,
                source: function (request, response) {
                    var term = (request.term || '').trim();
                    if (term.length < 2) {
                        response([]);
                        return;
                    }
                    // $.ui.autocomplete.filter is the same filter jQuery UI uses
                    // internally — case-insensitive substring match. Slicing to 50
                    // avoids rendering thousands of <li>s for 2-letter terms
                    // (e.g. "li", "py" match 5k+ of the 105k packages on trixie).
                    var results = $.ui.autocomplete.filter(systemPackages, term);
                    if (results.length > 50) {
                        results = results.slice(0, 50);
                    }
                    response(results);
                },
                select: function (event, ui) {
                    const selectedPackage = ui.item.value;
                    $(this).val(selectedPackage);
                    return false;
                }
            });
        }

        // Renders the "required by" note for a package. "user" means it was
        // installed from this page; a plugin repoName means a plugin depends on
        // it (in which case removing it here only drops the user's claim - it
        // stays installed as long as a plugin still needs it).
        function RequestersNote(requesters) {
            if (!requesters || !requesters.length)
                return '';
            return requesters.map(r => r === 'user' ? 'you' : r).join(', ');
        }

        function UpdateUserPackagesList() {
            if (!userInstalledPackages.length) {
                $('#userPackagesTable').hide().parent().after('<p id="noManagedPackages">No managed packages found.</p>');
                return;
            }

            // Dependencies (rows with 'via') hang off their parent: shown
            // indented under it at Developer level, otherwise counted on the
            // parent's row and hidden. A dependency whose parent is no longer
            // in the list becomes a top-level row; one that was also requested
            // directly stays under its parent (uninstalling the parent
            // releases it either way).
            const names = userInstalledPackages.map(e => e.name);
            const childrenOf = {};
            userInstalledPackages.forEach(e => {
                (e.via || []).forEach(parent => {
                    if (names.indexOf(parent) !== -1) {
                        (childrenOf[parent] = childrenOf[parent] || []).push(e.name);
                    }
                });
            });
            const parentOf = e => (e.via || []).find(parent => names.indexOf(parent) !== -1);

            // Render order: each top-level package followed by its dependencies.
            const byName = {};
            userInstalledPackages.forEach(e => { byName[e.name] = e; });
            const ordered = [];
            const placed = {};
            userInstalledPackages.forEach(e => {
                if (parentOf(e)) return;
                ordered.push(e);
                (childrenOf[e.name] || []).forEach(d => {
                    if (!placed[d]) {               // two parents: it belongs to the first
                        placed[d] = true;
                        if (showDependencyChain) {
                            ordered.push(byName[d]);
                        }
                    }
                });
            });
            // A dependency whose parents are all dependencies of each other
            // (hand-edited manifest) would otherwise vanish: list it top-level.
            userInstalledPackages.forEach(e => {
                if (parentOf(e) && !placed[e.name]) {
                    ordered.push(e);
                }
            });

            const rows = new Array(ordered.length);
            let pendingRequests = ordered.length;

            ordered.forEach((entry, idx) => {
                const pkg = entry.name;
                const requesters = entry.requesters || [];
                const byUser = requesters.indexOf('user') !== -1;
                const parent = parentOf(entry);
                const deps = childrenOf[pkg] || [];

                let nameCell = pkg;
                if (parent) {
                    nameCell = `<span class="pkg-note me-2">&#8627;</span>${pkg}`
                        + ` <span class="pkg-note pkg-dep-note">dependency of ${(entry.via || []).join(', ')}</span>`;
                } else if (deps.length && !showDependencyChain) {
                    nameCell += ` <span class="pkg-note">+${deps.length} dependenc${deps.length === 1 ? 'y' : 'ies'}</span>`;
                }

                // A user can only remove packages they requested; a plugin-owned
                // package goes when its plugin does, and a dependency goes with
                // the package that pulled it in. Those still get an Uninstall
                // button, disabled, with the reason on hover, so the column
                // lines up and the rule is discoverable. (Wrapped in a span:
                // a disabled button doesn't raise hover events for its title.)
                const plugins = requesters.filter(r => r !== 'user');
                let cannotRemove = '';
                if (!byUser) {
                    cannotRemove = `Uninstall the plugin${plugins.length === 1 ? '' : 's'} that depend${plugins.length === 1 ? 's' : ''} on it first: ${plugins.join(', ')}`;
                } else if (parent) {
                    cannotRemove = `Uninstall ${parent}, which depends on it, first`;
                }
                const removeBtn = cannotRemove
                    ? `<span class="d-inline-block" tabindex="0" title="${cannotRemove}"><button class='btn btn-sm btn-outline-secondary' disabled><i class="fas fa-trash-alt"></i> Uninstall</button></span>`
                    : `<button class='btn btn-sm btn-outline-danger' onClick='UninstallPackage("${pkg}")' title="Remove ${pkg}${deps.length ? ' and the ' + deps.length + ' package(s) it pulled in' : ''}"><i class="fas fa-trash-alt"></i> Uninstall</button>`;
                const reinstallBtn = `<button class='btn btn-sm btn-outline-warning' onClick='ReinstallPackage("${pkg}")' title="Reinstall ${pkg} with apt"><i class="fas fa-sync-alt"></i> Reinstall</button>`;
                const row = (status, buttons) => `<tr class="${parent ? 'pkg-dep' : ''}">
                        <td class="pkg-name">${nameCell}</td>
                        <td class="pkg-status">${status}</td>
                        <td class="pkg-req">${RequestersNote(requesters)}</td>
                        <td class="pkg-actions">${buttons}</td>
                    </tr>`;

                $.ajax({
                    url: `/api/system/packages/info/${encodeURIComponent(pkg)}`,
                    type: 'GET',
                    dataType: 'json',
                    success: function (data) {
                        const isInstalled = data.Installed === 'Yes';
                        rows[idx] = isInstalled
                            ? row('<span class="badge text-bg-success">Installed</span>', removeBtn + reinstallBtn)
                            : row('<span class="badge text-bg-danger">Missing</span>',
                                removeBtn + `<button class='btn btn-sm btn-outline-warning' onClick='ReinstallPackage("${pkg}")' title="Reinstall ${pkg} with apt"><i class="fas fa-sync-alt"></i> Reinstall</button>`);
                    },
                    error: function () {
                        console.error(`Error checking installation status for package: ${pkg}`);
                        rows[idx] = row('<span class="badge text-bg-secondary">Unknown</span>',
                            removeBtn + `<button class='btn btn-sm btn-outline-warning' onClick='ReinstallPackage("${pkg}")' title="Reinstall ${pkg} with apt"><i class="fas fa-sync-alt"></i> Reinstall</button>`);
                    },
                    complete: function () {
                        pendingRequests--;
                        if (pendingRequests === 0) {
                            $('#userPackagesList').html(rows.join(''));
                        }
                    }
                });
            });
        }

        function GetPackageInfo(packageName) {
            if (!packageName.trim()) {
                alert("Please enter a valid package name.");
                return;
            }

            selectedPackageName = packageName.trim();
            $.ajax({
                url: `/api/system/packages/info/${encodeURIComponent(selectedPackageName)}`,
                type: 'GET',
                dataType: 'json',
                success: function (data) {
                    if (data.error) {
                        $('#packageInfo').html(`<strong>Error:</strong> ${data.error}`);
                        return;
                    }

                    const description = data.Description || 'No description available.';
                    const dependencies = data.Depends
                        ? data.Depends.replace(/\([^)]*\)/g, '').trim()
                        : 'No dependencies.';
                    const installed = data.Installed === "Yes" ? "(Already Installed)" : "";

                    $('#packageInfo').html(`
                        <strong>Selected Package:</strong> ${selectedPackageName} ${installed}<br>
                        ${data.Installed !== "Yes"
                            ? `<strong>Description:</strong> ${description}<br>
                               <strong>Will also install these packages (if not already installed):</strong> ${dependencies}<br>
                                <div class="buttons btn-lg btn-rounded btn-outline-success mt-2" onClick="InstallPackage('${selectedPackageName}');">
                                    <i class="fas fa-download"></i> Install Package
                                </div>`
                            : `<strong>Description:</strong> ${description}<br>
                               <strong>Dependencies:</strong> ${dependencies}<br>
                                <div class="buttons btn-lg btn-rounded btn-outline-warning mt-2" onClick="ReinstallPackage('${selectedPackageName}');">
                                    <i class="fas fa-sync-alt"></i> Reinstall Package
                                </div>`}
                    `);
                },
                error: function () {
                    alert('Error, failed to fetch package information.');
                }
            });
        }

        function InstallPackage(packageName) {
            if (!packageName) {
                alert('Invalid package name.');
                return;
            }

            const url = `packagesHelper.php?action=install&package=${encodeURIComponent(packageName)}`;
            DisplayProgressDialog("packageProgressPopup", `Installing Package: ${packageName}`);
            StreamURL(
                url,
                'packageProgressPopupText',
                'ProgressDialogDone',
                'ProgressDialogDone'
            );
        }

        function ReinstallPackage(packageName) {
            if (!packageName) {
                alert('Invalid package name.');
                return;
            }

            const url = `packagesHelper.php?action=reinstall&package=${encodeURIComponent(packageName)}`;
            DisplayProgressDialog("packageProgressPopup", `Reinstalling Package: ${packageName}`);
            StreamURL(
                url,
                'packageProgressPopupText',
                'ProgressDialogDone',
                'ProgressDialogDone'
            );
        }

        function UninstallPackage(packageName) {
            const url = `packagesHelper.php?action=uninstall&package=${encodeURIComponent(packageName)}`;
            DisplayProgressDialog("packageProgressPopup", `Uninstalling Package: ${packageName}`);
            StreamURL(
                url,
                'packageProgressPopupText',
                'ProgressDialogDone',
                'ProgressDialogDone'
            );
        }

        $(document).ready(function () {
            GetSystemPackages();
            UpdateUserPackagesList();
        });
    </script>
    <title>Package Manager</title>
</head>

<body>
    <div id="bodyWrapper">
        <?php
        $activeParentMenuItem = 'content';
        include 'menu.inc'; ?>
        <div class="mainContainer">
            <h1 class="title">Package Manager</h1>
            <div class="pageContent">
                <div id="packages" class="settings">


                    <h2>Please Note:</h2>
                    Installing or reinstalling packages can break your FPP installation requiring complete reinstallation of
                    FPP. Continue at your own risk.
                    <p>
                    <h2>Installed User Packages</h2>
                    <div style="overflow-x: auto;">
                        <table id="userPackagesTable">
                            <thead>
                                <tr>
                                    <th>Package</th>
                                    <th>Status</th>
                                    <th>Required by</th>
                                    <th class="pkg-actions"></th>
                                </tr>
                            </thead>
                            <tbody id="userPackagesList"></tbody>
                        </table>
                    </div>

                    <div id="loadingIndicator" style="display: none; text-align: center;">
                        <p>Loading package list, please wait...</p>
                    </div>

                    <h2>Install a Package</h2>
                    <div id="packageInputContainer">
                        <div class="row g-2 align-items-center">
                            <div class="col-12 col-sm">
                                <input type="text" id="packageInput"
                                    class="form-control form-control-lg form-control-rounded has-shadow"
                                    placeholder="Enter package name" />
                            </div>
                            <div class="col-12 col-sm-auto">
                                <div class="buttons btn-lg btn-rounded btn-outline-info"
                                    onClick='GetPackageInfo($("#packageInput").val().trim());'>
                                    <i class="fas fa-info-circle"></i> Get Info
                                </div>
                            </div>
                        </div>
                    </div>

                    <div class='packageDiv'>
                        <div id="packageInfo" class="mt-3 text-muted"></div>
                        <div id="overlay"></div>
                    </div>
                </div>

            </div>
        </div>
        <?php include 'common/footer.inc'; ?>
    </div>
</body>

</html>