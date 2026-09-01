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
    $userPackages = [];
    foreach (LoadUserPackages() as $pkgName => $requesters) {
        $userPackages[] = ['name' => $pkgName, 'requesters' => array_values($requesters)];
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
    </style>
    <script>
        var systemPackages = [];
        var userInstalledPackages = <?php echo json_encode($userPackages); ?>;
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
            const labels = requesters.map(r => r === 'user' ? 'you' : r);
            return ` <span class="text-muted" style="font-size: 0.85em;">(required by: ${labels.join(', ')})</span>`;
        }

        function UpdateUserPackagesList() {
            if (!userInstalledPackages.length) {
                $('#userPackagesList').html('<p>No managed packages found.</p>');
                return;
            }

            let rows = new Array(userInstalledPackages.length);
            let pendingRequests = userInstalledPackages.length;

            userInstalledPackages.forEach((entry, idx) => {
                const pkg = entry.name;
                const requesters = entry.requesters || [];
                const byUser = requesters.indexOf('user') !== -1;
                const note = RequestersNote(requesters);

                // A user can only remove packages they requested. Plugin-owned
                // packages are managed by uninstalling the owning plugin.
                const removeBtn = byUser
                    ? `<button class='btn btn-sm btn-outline-danger ms-2' onClick='UninstallPackage("${pkg}")'>Uninstall</button>`
                    : '';
                const reinstallBtn = `<button class='btn btn-sm btn-outline-warning ms-2' onClick='ReinstallPackage("${pkg}")'><i class="fas fa-sync-alt"></i> Reinstall Package</button>`;

                $.ajax({
                    url: `/api/system/packages/info/${encodeURIComponent(pkg)}`,
                    type: 'GET',
                    dataType: 'json',
                    success: function (data) {
                        const isInstalled = data.Installed === 'Yes';
                        rows[idx] = `<li>${pkg}${note}
                            ${isInstalled ? reinstallBtn + removeBtn
                                : `<button class='btn btn-sm btn-outline-warning' onClick='InstallPackage("${pkg}")'>Reinstall Required</button>${removeBtn}`}
                        </li>`;
                    },
                    error: function () {
                        console.error(`Error checking installation status for package: ${pkg}`);
                        rows[idx] = `<li>${pkg}${note}
                            <button class='btn btn-sm btn-outline-warning' onClick='InstallPackage("${pkg}")'>Reinstall Required</button>${removeBtn}
                        </li>`;
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
                    <ul id="userPackagesList"></ul>

                    <div id="loadingIndicator" style="display: none; text-align: center;">
                        <p>Loading package list, please wait...</p>
                    </div>

                    <div id="packageInputContainer">
                        <div class="row">
                            <div class="col">
                                <input type="text" id="packageInput"
                                    class="form-control form-control-lg form-control-rounded has-shadow"
                                    placeholder="Enter package name" />
                            </div>
                            <div class="col-auto">
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