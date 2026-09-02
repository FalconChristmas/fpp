<!DOCTYPE html>
<html lang="en">

<head>
    <?
    include 'common/htmlMeta.inc';
    require_once('config.php');
    require_once('common.php');
    include('common/menuHead.inc');

    $storageUILevel = 1;
    if (isset($settings["UnpartitionedSpace"]) && $settings['UnpartitionedSpace'] > 0) {
        $storageUILevel = 0;
    }
    if ($storageUILevel > 0 && $settings['Platform'] != "MacOS" && $settings['Platform'] != "Docker") {
        exec('findmnt -n -o SOURCE / | colrm 1 5', $rootDevOutput, $return_val);
        $rootDev = isset($rootDevOutput[0]) ? trim($rootDevOutput[0]) : "";
        unset($rootDevOutput);
        $flashTargets = array();
        if (preg_match('/^mmcblk0p/', $rootDev)) {
            $flashTargets = array("mmcblk1", "nvme0n1", "sda");
        } else if (preg_match('/^mmcblk1p/', $rootDev) && strpos($settings['SubPlatform'], 'PocketBeagle2') !== false) {
            $flashTargets = array("mmcblk0");
        }
        foreach ($flashTargets as $target) {
            if (file_exists("/dev/" . $target)) {
                exec("lsblk -b -d -n -o SIZE /dev/" . $target, $sizeOutput, $return_val);
                $sizeGB = (isset($sizeOutput[0]) ? intval($sizeOutput[0]) : 0) / 1024 / 1024 / 1024;
                unset($sizeOutput);
                if ($sizeGB > 12) {
                    $storageUILevel = 0;
                    break;
                }
            }
        }
    }
    $pages = array(
        array("name" => "playback", "title" => "Playback", "ui" => 0),
        array("name" => "av", "title" => "Audio/Video", "ui" => 0),
        array("name" => "time", "title" => "Time", "ui" => 0),
        array("name" => "ui", "title" => "UI", "ui" => 0),
        array("name" => "email", "title" => "Email", "ui" => 0),
        array("name" => "mqtt", "title" => "MQTT", "ui" => 0),
        array("name" => "output", "title" => "Input/Output", "ui" => 1),
        array("name" => "logs", "title" => "Logging", "ui" => 1),
        array("name" => "storage", "title" => "Storage", "ui" => $storageUILevel),
        array("name" => "system", "title" => "System", "ui" => 0),
        array("name" => "developer", "title" => "Developer", "ui" => 1)
    );
    ?>
    <link rel="stylesheet" type="text/css" href="css/jquery.timepicker.css?ref=<?= filemtime('css/jquery.timepicker.css'); ?>">
    <link rel="stylesheet" type="text/css" href="jquery/colpick/css/colpick.css?ref=<?= filemtime('jquery/colpick/css/colpick.css'); ?>">
    <link rel="stylesheet" type="text/css" href="css/jquery.colpick.css?ref=<?= filemtime('css/jquery.colpick.css'); ?>">
    <script type="text/javascript" src="js/jquery.timepicker.js?ref=<?= filemtime('js/jquery.timepicker.js'); ?>"></script>
    <script type="text/javascript" src="jquery/colpick/js/colpick.js?ref=<?= filemtime('jquery/colpick/js/colpick.js'); ?>"></script>
    <meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
    <title><? echo $pageTitle; ?></title>
</head>

<body>
    <div id="bodyWrapper">
        <?php include 'menu.inc'; ?>
        <div class="mainContainer">
            <h1 class="title">FPP Settings</h1>
            <div class="pageContent">

                <div class='fppTabs'>
                    <div id="settingsManager">
                        <nav>
                            <div class="nav nav-pills" id="nav-tab" role="tablist">
                                <?php
                                foreach ($pages as $page) {
                                    if ($page["ui"] <= $uiLevel) {
                                        ?>
                                        <a class="nav-item nav-link" id="settings-<?php echo $page["name"]; ?>-tab"
                                            data-bs-toggle="tab" href="#settings-<?php echo $page["name"]; ?>"
                                            data-option="<?php echo $page["name"]; ?>" role="tab"
                                            aria-controls="settings-<?php echo $page["name"]; ?>"
                                            aria-selected="true"><?php echo $page["title"]; ?></a>
                                    <? } ?>
                                <? } ?>
                            </div>
                        </nav>

                        <div class="tab-content" id="nav-tabContent">
                            <?php
                            foreach ($pages as $page) {
                                ?>
                                <div class="tab-pane fade show <?php echo ($page['name'] == 'playback') ? 'active' : ''; ?>"
                                    id="settings-<?php echo $page['name']; ?>" role="tabpanel"
                                    aria-labelledby="settings-<?php echo $page['name']; ?>-tab">
                                    <?php require_once("settings-" . $page['name'] . ".php"); ?>
                                </div>
                            <? } ?>
                        </div>

                        <table>
                            <? if ($uiLevel >= 1) { ?>
                                <tr>
                                    <th align='right'><i class='fas fa-fw fa-graduation-cap ui-level-1'></i></th>
                                    <th align='left'>- Advanced Level Setting</th>
                                </tr>
                            <? } ?>
                            <? if ($uiLevel >= 2) { ?>
                                <tr>
                                    <th align='right'><i class='fas fa-fw fa-flask ui-level-2'></i></th>
                                    <th align='left'>- Experimental Level Setting</th>
                                </tr>
                            <? } ?>
                            <? if ($uiLevel >= 3) { ?>
                                <tr>
                                    <th align='right'><i class='fas fa-fw fa-code ui-level-3'></i></th>
                                    <th align='left'>- Developer Level Setting</th>
                                </tr>
                            <? } ?>
                        </table>
                    </div>
                </div>


            </div>

            <?php include 'common/footer.inc'; ?>

        </div>
        <script>
            // Enable link to tab (e.g. settings.php#settings-mqtt )
            var hash = location.hash.replace(/^#/, '');
            if (hash) {
                $('.nav a[href="#' + hash + '"]').tab('show');
            }
            // Changes hash in url bar for easy copy/paste
            $('.nav a.nav-item').on('shown.bs.tab', function (e) {
                history.pushState(null, null, e.target.hash);
            });
        </script>
</body>

</html>