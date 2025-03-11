<!DOCTYPE html>
<html lang="en">
<head>
    <?php
    include 'common/htmlMeta.inc';
    require_once('config.php');
    require_once('common.php');

    $helpPages = array(
        'help/pixeloverlaymodels.php' => 'Real-Time Pixel Overlay Models'
    );

    include 'common/menuHead.inc'; ?>
    <meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
    <title><?php echo $pageTitle; ?></title>
    <style>
        /* Add minimal CSS for better spacing and separation */
        .help-section {
            padding: 15px;
            margin-bottom: 20px;
            background-color: #f8f9fa; /* Light background similar to the first image */
            border-radius: 5px;
        }
        .help-section ul {
            padding-left: 0;
        }
        .help-section li {
            margin-bottom: 10px;
        }
        .help-section h3 {
            margin-bottom: 15px;
        }
    </style>
</head>
<body>
    <div id="bodyWrapper">
        <?php
        $activeParentMenuItem = 'help';
        include 'menu.inc'; ?>
        <div class="mainContainer">
            <h1 class="title">Help</h1>
            <div class="pageContent">
                <h2>Get Help</h2>
                <p>Need assistance with FPP? Explore the resources below.</p>

                <div class="row">
                    <div class="col-sm-12 col-md-6 col-xl-4">
                        <div class="help-section">
                            <h3>Places to Get Help</h3>
                            <ul class="no-bullets">
                                <li><a class="<?php echo $externalLinkClass; ?> link-to-fpp-manual"
                                       href="https://added-by-javascript.net" target="_blank" rel="noopener noreferrer">
                                       <i class="fas fa-book"></i> FPP has an extensive manual - FPP Manual
                                       <i class="fas fa-external-link-alt external-link"></i></a></li>
                                <li><a class="<?php echo $externalLinkClass; ?>"
                                       href="https://www.facebook.com/groups/1554782254796752" target="_blank" rel="noopener noreferrer">
                                       <i class="fas fa-comments"></i> Got a question? Join the FPP Facebook group
                                       <i class="fas fa-external-link-alt external-link"></i></a></li>
                                <li><a class="<?php echo $externalLinkClass; ?>" href="https://falconchristmas.com/forum" target="_blank" rel="noopener noreferrer">
                                       <i class="fas fa-comments"></i> or the FPP Forums
                                       <i class="fas fa-external-link-alt external-link"></i></a></li>
                                <li><a class="<?php echo $externalLinkClass; ?>" href="https://xlights.org/zoom/" target="_blank" rel="noopener noreferrer">
                                       <i class="fas fa-headphones"></i> Prefer to talk? Join the xLights Zoom room
                                       <i class="fas fa-external-link-alt external-link"></i></a></li>
                                <li><a class="<?php echo $externalLinkClass; ?>" href="https://github.com/FalconChristmas/fpp/issues" target="_blank" rel="noopener noreferrer">
                                       <i class="fas fa-bug"></i> Need to log a bug? - Issue Tracker
                                       <i class="fas fa-external-link-alt external-link"></i></a></li>
                            </ul>
                        </div>
                    </div>

                    <div class="col-sm-12 col-md-6 col-xl-4">
                        <div class="help-section">
                            <h3>FPP API</h3>
                            <ul class="no-bullets">
                                <li><a class="dropdown-item" href="apihelp.php">
                                       <i class="fas fa-plug"></i> REST API Help</a></li>
                                <li><a class="dropdown-item" href="commandhelp.php">
                                       <i class="fas fa-cubes"></i> FPP Commands Help</a></li>
                            </ul>
                        </div>
                    </div>

                    <div class="col-sm-12 col-md-6 col-xl-4">
                        <div class="help-section">
                            <h3>Specific Topics</h3>
                            <ul class="no-bullets"> <!-- Changed to no-bullets for consistency -->
                                <li><a href="javascript:void(0);" onclick="helpPage='help/backup.php'; DisplayHelp();">
                                       <i class="fas fa-archive"></i> Backup & Restore</a></li>
                                <li><a href="javascript:void(0);" onclick="helpPage='help/channeloutputs.php'; DisplayHelp();">
                                       <i class="fas fa-broadcast-tower"></i> Channel Outputs</a></li>
                                <li><a href="javascript:void(0);" onclick="helpPage='help/gpio.php'; DisplayHelp();">
                                       <i class="fas fa-microchip"></i> GPIO Input Triggers</a></li>
                                <li><a href="javascript:void(0);" onclick="helpPage='help/networkconfig.php'; DisplayHelp();">
                                       <i class="fas fa-network-wired"></i> Network Config</a></li>
                                <li><a href="javascript:void(0);" onclick="helpPage='help/outputprocessors.php'; DisplayHelp();">
                                       <i class="fas fa-cogs"></i> Output Processors</a></li>
                                <li><a href="javascript:void(0);" onclick="helpPage='help/pixeloverlaymodels.php'; DisplayHelp();">
                                       <i class="fas fa-layer-group"></i> Real-Time Pixel Overlay Models</a></li>
                                <li><a href="javascript:void(0);" onclick="helpPage='help/scriptbrowser.php'; DisplayHelp();">
                                       <i class="fas fa-code"></i> Script Repository Browser</a></li>
                            </ul>
                        </div>
                    </div>
                </div>
            </div>
        </div>
        <?php include 'common/footer.inc'; ?>
    </div>
</body>
</html>