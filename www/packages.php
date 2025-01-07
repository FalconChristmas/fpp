<!DOCTYPE html>
<html lang="en">

<head>
    <?php
    require_once('config.php');
    require_once('common.php');
    include('common/menuHead.inc');

    $userPackagesFile = $settings['configDirectory'] . '/userpackages.json';
    $userPackages = [];
    if (file_exists($userPackagesFile)) {
        $userPackages = json_decode(file_get_contents($userPackagesFile), true);
        if (!is_array($userPackages)) {
            $userPackages = [];
        }
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
                source: systemPackages,
                select: function (event, ui) {
                    const selectedPackage = ui.item.value;
                    $(this).val(selectedPackage);
                    return false;
                }
            });
        }

        function UpdateUserPackagesList() {
            if (!userInstalledPackages.length) {
                $('#userPackagesList').html('<p>No user-installed packages found.</p>');
                return;
            }

            let userPackagesListHTML = '';
            let pendingRequests = userInstalledPackages.length;

            userInstalledPackages.forEach(pkg => {
                $.ajax({
                    url: `/api/system/packages/info/${encodeURIComponent(pkg)}`,
                    type: 'GET',
                    dataType: 'json',
                    success: function (data) {
                        const isInstalled = data.Installed === 'Yes';
                        userPackagesListHTML += `<li>${pkg}
                            ${isInstalled ? `
                                <button class='btn btn-sm btn-outline-danger' onClick='UninstallPackage("${pkg}")'>
                                    Uninstall
                                </button>`
                            : `
                                <button class='btn btn-sm btn-outline-warning' onClick='InstallPackage("${pkg}")'>
                                    Reinstall Required
                                </button>
                                <button class='btn btn-sm btn-outline-danger ms-2' onClick='UninstallPackage("${pkg}")'>
                                    Remove
                                </button>`}
                        </li>`;
                    },
                    error: function () {
                        console.error(`Error checking installation status for package: ${pkg}`);
                        userPackagesListHTML += `<li>${pkg} 
                            <button class='btn btn-sm btn-outline-warning' onClick='InstallPackage("${pkg}")'>
                                Reinstall Required
                            </button>
                        </li>`;
                    },
                    complete: function () {
                        pendingRequests--;
                        if (pendingRequests === 0) {
                            $('#userPackagesList').html(userPackagesListHTML);
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
                               <strong>Will also install these packages:</strong> ${dependencies}<br>
                               <div class="buttons btn-lg btn-rounded btn-outline-success mt-2" onClick="InstallPackage('${selectedPackageName}');">
                                   <i class="fas fa-download"></i> Install Package
                               </div>`
                            : ""}
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
            $('#packageProgressPopupCloseButton').text('Please Wait').prop("disabled", true);
            DisplayProgressDialog("packageProgressPopup", `Installing Package: ${packageName}`);
            StreamURL(
                url,
                'packageProgressPopupText',
                'EnableCloseButtonAfterOperation',
                'EnableCloseButtonAfterOperation'
            );
        }

        function UninstallPackage(packageName) {
            const url = `packagesHelper.php?action=uninstall&package=${encodeURIComponent(packageName)}`;
            $('#packageProgressPopupCloseButton').text('Please Wait').prop("disabled", true);
            DisplayProgressDialog("packageProgressPopup", `Uninstalling Package: ${packageName}`);
            StreamURL(
                url,
                'packageProgressPopupText',
                'EnableCloseButtonAfterOperation',
                'EnableCloseButtonAfterOperation'
            );
        }

        function EnableCloseButtonAfterOperation(id) {
            $('#packageProgressPopupCloseButton')
                .text('Close')
                .prop("disabled", false)
                .on('click', function () {
                    $('#packageInput').val(''); // Clear the input field
                    location.reload(); // Refresh the page when the close button is clicked
                });
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
                    Installing additional packages can break your FPP installation requiring complete reinstallation of FPP.  Continue at your own risk.
                    <p>
                    <h2>Installed User Packages</h2>
                    <ul id="userPackagesList"></ul>

                    <div id="loadingIndicator" style="display: none; text-align: center;">
                        <p>Loading package list, please wait...</p>
                    </div>

                    <div id="packageInputContainer">
                        <div class="row">
                            <div class="col">
                                <input type="text" id="packageInput" class="form-control form-control-lg form-control-rounded has-shadow" placeholder="Enter package name" />
                            </div>
                            <div class="col-auto">
                                <div class="buttons btn-lg btn-rounded btn-outline-info" onClick='GetPackageInfo($("#packageInput").val().trim());'>
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

                <div id="packageProgressPopup" class="modal taller-modal" tabindex="-1">
                    <div class="modal-dialog modal-lg">
                        <div class="modal-content">
                            <div class="modal-header">
                                <h5 class="modal-title">Installing Package</h5>
                                <button type="button" class="btn-close" data-bs-dismiss="modal" aria-label="Close"></button>
                            </div>
                            <div class="modal-body">
                                <pre id="packageProgressPopupText" style="white-space: pre-wrap;"></pre>
                            </div>
                            <div class="modal-footer">
                                <button id="packageProgressPopupCloseButton" type="button" class="btn btn-secondary" data-bs-dismiss="modal" disabled>Close</button>
                            </div>
                        </div>
                    </div>
                </div>

            </div>
        </div>
        <?php include 'common/footer.inc'; ?>
    </div>
</body>

</html>

