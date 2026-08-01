<!DOCTYPE html>
<html lang="en">

<head>
    <?php
    include 'common/htmlMeta.inc';
    require_once "config.php";
    require_once 'common.php';
    include 'common/menuHead.inc';
    ?>
    <title><? echo $pageTitle; ?></title>
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
                    <div class="callout callout-warning">
                        <b>The FPP-sanctioned Script Repository has been deprecated.</b>

                        <p>&nbsp;</p>

                        <p>
                            Scripts themselves aren't going anywhere — you can still upload, edit, and run your own
                            scripts in FPP at any time via the Scripts tab in the
                            <a href="filemanager.php#tab-scripts">File Manager</a> screen. What's going away is only this
                            in-app browser for the FPP-sanctioned fpp-scripts
                            repository. Scripts were contributed there over the years but were often never revisited
                            or updated for later FPP versions — generally unloved — and most of what they offered has
                            since been replaced by FPP Commands, Overlay Models, and better-maintained plugins, so
                            keeping a separate script catalog and installer around no longer made sense.
                        </p>
                        <p>
                            The <a href="https://github.com/FalconChristmas/fpp-scripts" target="_blank">fpp-scripts
                                GitHub repository</a> is still available and its
                            <a href="https://github.com/FalconChristmas/fpp-scripts/blob/master/README.md"
                                target="_blank">README</a> documents which scripts are still relevant, which ones have
                            been replaced (and by what), and which have been deprecated. If you still need one of the
                            historical scripts, download it manually from the repository and install it via the
                            Scripts tab in the <a href="filemanager.php#tab-scripts">File Manager</a> screen.
                        </p>
                        <p><em>This page will be removed in FPP 11.</em></p>
                    </div>
                </div>
            </div>
        </div>
        <?php include 'common/footer.inc'; ?>
    </div>

</body>

</html>