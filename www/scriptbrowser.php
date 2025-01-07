<!DOCTYPE html>
<html lang="en">

<head>
    <?php
    include 'common/htmlMeta.inc';
    require_once "config.php";
    include 'common/menuHead.inc';

    function normalize_version($version)
    {
        // convert a version string like 2.7.1-2-dirty to "20701"
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
    ?>
    <title><? echo $pageTitle; ?></title>
    <script>

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
            }).fail(function () {
                DialogError("Install Script", "Install Failed");
            });
        }

        function filterScripts(searchTxt) {
            var value = searchTxt.toLowerCase();
            $('#fppScripts .fppScriptEntry').filter(function () { $(this).toggle($(this).text().toLowerCase().indexOf(value) > -1) })
        }

        function pageSpecific_PageLoad_PostDOMLoad_ActionsSetup() {
            $("#ScriptSearchInput").on("keyup", function () {
                filterScripts($(this).val());
            });
        }

    </script>
</head>

<body>
    <div id="bodyWrapper">
        <?php
        $activeParentMenuItem = 'content';
        include 'menu.inc'; ?>
        <div class="mainContainer">
            <h1 class="title">Script Repository</h1>
            <div class="pageContent">
                <div id="uiscripts" class="settings">

                    <div class="row fppScriptSearch">
                        <div class="col">
                            <input type="text" id="ScriptSearchInput"
                                class="form-control form-control-lg form-control-rounded has-shadow"
                                placeholder="Search for a script..." />
                        </div>
                    </div>

                    <div id='fppScripts'>

                        <?
                        $indexCSV = file_get_contents("https://raw.githubusercontent.com/FalconChristmas/fpp-scripts/master/index.csv");
                        $lines = explode("\n", $indexCSV);

                        $nonMatchingScripts = [];

                        foreach ($lines as $line) {
                            if (preg_match("/^#/", $line)) {
                                continue;
                            }

                            $parts = explode(',', $line);

                            if (count($parts) < 4) {
                                continue;
                            }

                            if (normalize_version($parts[3]) > $rfs_ver) {
                                $nonMatchingScripts[] = $parts;
                                continue;
                            }

                            if (count($parts) > 4 && normalize_version($parts[4]) <= $rfs_ver) {
                                $nonMatchingScripts[] = $parts;
                                continue;
                            }

                            ?>

                            <div class="backdrop mb-2 fppScriptEntry">
                                <div class="row">


                                    <div class="col-lg-3 fppScriptEntryCol">
                                        <div class="labelHeading">Filename:</div>

                                        <h3><? echo $parts[1]; ?></h3>



                                    </div>
                                    <div class="col-lg-1  fppScriptEntryCol text-muted">
                                        <div class="labelHeading">Category:</div>
                                        <? echo $parts[0]; ?>
                                    </div>
                                    <div class="col-lg  fppScriptEntryCol text-muted">
                                        <div class="labelHeading">Description:</div>
                                        <? echo $parts[2]; ?>
                                    </div>
                                    <div class="col-lg-auto fppScriptEntryCol">
                                        <input type='button' class='buttons' value='View'
                                            onClick='ViewRemoteScript("<? echo $parts[0]; ?>", "<? echo $parts[1]; ?>");'>
                                        <input type='button' class='buttons btn-success' value='Install'
                                            onClick='InstallRemoteScript("<? echo $parts[0]; ?>", "<? echo $parts[1]; ?>");'>
                                    </div>
                                </div>

                            </div>
                            <?
                        }
                        ?>

                    </div>

                    <? if ($settings["uiLevel"] >= 1 && !empty($nonMatchingScripts)): ?>
                        <div class="callout">
                            <b>NOTE:</b> Some scripts such as the Remote Control scripts may require editing to configure
                            variables to be functional. After installing a script from the Script Repository, you can view
                            and download the script from the Scripts tab in the <a href='filemanager.php'>File Manager</a>
                            screen.
                        </div>

                        <h2>Incompatible Scripts</h2>
                        <div class="callout">
                            The scripts below are from other versions of FPP and are known not to function with this version
                            of FPP or have been replaced by plugins, FPP command functionality or other built-in functions.
                            They are listed here for your reference only.
                        </div>
                        <div id='fppScriptsNonMatching'>
                            <? foreach ($nonMatchingScripts as $parts): ?>
                                <div class="backdrop mb-2 fppScriptEntry">
                                    <div class="row">
                                        <div class="col-lg-3 fppScriptEntryCol">
                                            <div class="labelHeading">Filename:</div>
                                            <h3><?= $parts[1]; ?></h3>
                                        </div>
                                        <div class="col-lg-1 fppScriptEntryCol text-muted">
                                            <div class="labelHeading">Category:</div>
                                            <?= $parts[0]; ?>
                                        </div>
                                        <div class="col-lg fppScriptEntryCol text-muted">
                                            <div class="labelHeading">Description:</div>
                                            <?= $parts[2]; ?>
                                        </div>
                                        <div class="col-lg-auto fppScriptEntryCol">
                                            <input type='button' class='buttons' value='View'
                                                onClick='ViewRemoteScript("<?= $parts[0]; ?>", "<?= $parts[1]; ?>");'>
                                        </div>
                                    </div>
                                </div>
                            <? endforeach; ?>
                        </div>
                    <? endif; ?>
                </div>
            </div>
        </div>
        <?php include 'common/footer.inc'; ?>
    </div>

</body>

</html>