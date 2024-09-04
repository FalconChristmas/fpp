<!DOCTYPE html>
<html lang="en">

<head>
    <?php
    require_once 'config.php';
    require_once "common.php";
    include 'common/menuHead.inc';

    $tabDivs = array();
    $tabStrings = array();
    $fileExtensions = array();
    if (file_exists($pluginDirectory)) {
        $handle = opendir($pluginDirectory);
        while (($plugin = readdir($handle)) !== false) {
            if (!in_array($plugin, array('.', '..'))) {
                $pluginInfoFile = $pluginDirectory . "/" . $plugin . "/pluginInfo.json";
                if (file_exists($pluginInfoFile)) {
                    $pluginConfig = json_decode(file_get_contents($pluginInfoFile), true);
                    if (isset($pluginConfig["fileExtensions"])) {
                        foreach ($pluginConfig["fileExtensions"] as $key => $value) {
                            if (isset($value["tab"])) {
                                $tabStr = "<li class=\"nav-item\">";
                                $tabStr .= "<a class=\"nav-link \" id=\"tab-" . $key . "-tab\" data-bs-toggle=\"pill\" data-bs-target=\"#tab-" . $key . "\" href=\"#tab-" . $key . "\" role=\"tab\" aria-controls=\"tab-" . $key . "\">";
                                $tabStr .= $value["tab"];
                                $tabStr .= "</a></li>";
                                array_push($tabStrings, $tabStr);
                                array_push($fileExtensions, $key);
                                array_push($tabDivs, $pluginDirectory . "/" . $plugin . "/" . $key . "-tab.inc");
                            }
                            if (isset($value["folder"])) {
                                if (!is_dir($mediaDirectory . "/" . $value["folder"])) {
                                    mkdir($mediaDirectory . "/" . $value["folder"]);
                                }
                            }
                        }
                    }
                }
            }
        }
        closedir($handle);
    }
    ?>

    <script type="text/javascript" src="jquery/jquery.tablesorter/jquery.tablesorter.min.js"></script>
    <script type="text/javascript" src="jquery/jquery.tablesorter/jquery.tablesorter.widgets.min.js"></script>
    <script type="text/javascript" src="jquery/jquery.tablesorter/parsers/parser-metric.min.js"></script>
    <script type="text/javascript" src="jquery/jquery.tablesorter/widgets/widget-cssStickyHeaders.min.js"></script>
    <script type="text/javascript" src="jquery/jquery.tablesorter/extras/jquery.metadata.min.js"></script>
    <script type="text/javascript" src="js/fpp-filemanager.js"></script>

    <script>
        const pluginFileExtensions = [ <? echo implode(", ", array_map(fn($arg) => "'$arg'", $fileExtensions)); ?>];
        GetAllFiles();
    </script>

    <?php
    exec("df -k " . $mediaDirectory . "/upload |awk '/\/dev\//{printf(\"%d\\n\", $5);}'", $output, $return_val);
    $freespace = $output[0];
    unset($output);
    ?>
    <meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
    <title><? echo $pageTitle; ?></title>


    <script src="jquery/jQuery-Form-Plugin/js/jquery.form.js"></script>
    <script src="js/filepond.min.js"></script>

    <link rel="stylesheet" href="css/filepond.min.css" />
    <style>
        .fileponduploader {
            background: #fff;
            border: 2px dashed #ced4da;
            border-radius: 10px;
            transition: 0.3s all cubic-bezier(0.02, 0.72, 0.32, 0.99);
        }

        .filepond--root .filepond--drop-label {
            min-height: 100px;
            background: transparent;
        }

        .filepond--drop-label label {
            min-height: 100px;
        }

        .filepond--panel-root {
            background-color: transparent;
        }
    </style>


</head>

<body>

    <div id="bodyWrapper">
        <?php
        $activeParentMenuItem = 'content';
        include 'menu.inc'; ?>
        <div class="mainContainer">
            <div class='title'>File Manager</div>
            <? if ($freespace > 95) { ?>
                <div class="alert alert-danger" role="alert">WARNING: storage device is almost full!</div>
            <? } ?>
            <div class="pageContent">

                <div id="fileManager">

                    <ul class="nav nav-pills pageContent-tabs" id="fileManagerTabs" role="tablist">
                        <li class="nav-item">
                            <a class="nav-link active" id="tab-sequence-tab" data-bs-toggle="pill"
                                data-bs-target="#tab-sequence" href="#tab-sequence" role="tab"
                                aria-controls="tab-sequence" aria-selected="true">
                                Sequences
                            </a>
                        </li>
                        <li class="nav-item">
                            <a class="nav-link " id="tab-audio-tab" data-bs-toggle="pill" data-bs-target="#tab-audio"
                                href="#tab-audio" role="tab" aria-controls="tab-audio">
                                Audio
                            </a>
                        </li>
                        <li class="nav-item">
                            <a class="nav-link " id="tab-video-tab" data-bs-toggle="pill" data-bs-target="#tab-video"
                                href="#tab-video" role="tab" aria-controls="tab-video">
                                Video
                            </a>
                        </li>
                        <li class="nav-item">
                            <a class="nav-link " id="tab-images-tab" data-bs-toggle="pill" data-bs-target="#tab-images"
                                href="#tab-images" role="tab" aria-controls="tab-images">
                                Images
                            </a>
                        </li>
                        <li class="nav-item">
                            <a class="nav-link " id="tab-effects-tab" data-bs-toggle="pill"
                                data-bs-target="#tab-effects" href="#tab-effects" role="tab"
                                aria-controls="tab-effects">
                                Effects
                            </a>
                        </li>
                        <li class="nav-item">
                            <a class="nav-link " id="tab-scripts-tab" data-bs-toggle="pill"
                                data-bs-target="#tab-scripts" href="#tab-scripts" role="tab"
                                aria-controls="tab-scripts">
                                Scripts
                            </a>
                        </li>
                        <?
                        foreach ($tabStrings as $ts) {
                            echo $ts;
                        }
                        ?>
                        <li class="nav-item">
                            <a class="nav-link " id="tab-logs-tab" data-bs-toggle="pill" data-bs-target="#tab-logs"
                                href="#tab-logs" role="tab" aria-controls="tab-logs">
                                Logs
                            </a>
                        </li>
                        <li class="nav-item">
                            <a class="nav-link " id="tab-uploads-tab" data-bs-toggle="pill"
                                data-bs-target="#tab-uploads" href="#tab-uploads" role="tab"
                                aria-controls="tab-uploads">
                                Uploads
                            </a>
                        </li>
                        <li class="nav-item">
                            <a class="nav-link " id="tab-crashes-tab" data-bs-toggle="pill"
                                data-bs-target="#tab-crashes" href="#tab-crashes" role="tab"
                                aria-controls="tab-crashes">
                                Crash Reports
                            </a>
                        </li>
                        <li class="nav-item">
                            <a class="nav-link " id="tab-backups-tab" data-bs-toggle="pill"
                                data-bs-target="#tab-backups" href="#tab-backups" role="tab"
                                aria-controls="tab-backups">
                                Backups
                            </a>
                        </li>
                    </ul>



                    <div id="tablefilterChk">
                        <?php PrintSetting('fileManagerTableFilter', 'FileManagerFilterToggled'); ?>
                    </div>
                    <script>
                        //set filter state
                        $('#fileManagerTableFilter').prop('checked', (settings.fileManagerTableFilter == "1" ? true : false));
                    </script>

                    <div class="tab-content" id="fileUploadTabsContent">

                        <div class="tab-pane fade show active" id="tab-sequence" role="tabpanel"
                            aria-labelledby="tab-sequence-tab">

                            <div id="divSeq">
                                <div class="backdrop">
                                    <div class="row justify-content-between fileDetailsHeader">
                                        <div class="col-auto">
                                            <h2>Sequence Files (.fseq)</h2>
                                        </div>
                                        <div class="col-auto fileCountDetails">
                                            <div class="row">
                                                <div class="col-auto fileCountlabelHeading">Items</div>
                                                <div class="col-auto fileCountlabelValue"><span id="fileCount_Sequences"
                                                        class='badge text-bg-secondary'>0</span></div>
                                            </div>
                                        </div>
                                    </div>

                                    <div id="divSeqData" class="fileManagerDivData">
                                        <table id="tblSequences" class="tablesorter">
                                            <thead>
                                                <tr>
                                                    <th>File</th>
                                                    <th class="sorter-metric" data-metric-name-full="byte|Byte|BYTE"
                                                        data-metric-name-abbr="b|B">Size</th>
                                                    <th>Date Modified</th>
                                                </tr>
                                            </thead>
                                            <tbody>
                                            </tbody>
                                        </table>
                                    </div>

                                    <div class='form-actions'>
                                        <input onclick="ClearSelections('Sequences');" class="buttons" type="button"
                                            value="Clear" />
                                        <?php if (isset($settings['fppMode']) && ($settings['fppMode'] == 'player')) { ?>
                                            <input onclick="ButtonHandler('Sequences', 'play');"
                                                class="disableButtons singleSequencesButton" type="button" value="Play" />
                                            <input onclick="ButtonHandler('Sequences', 'playHere');"
                                                class="disableButtons singleSequencesButton" type="button"
                                                value="Play Here" />
                                        <?php } ?>
                                        <input onclick="ButtonHandler('Sequences', 'sequenceInfo');"
                                            class="disableButtons singleSequencesButton" type="button"
                                            value="Sequence Info" />
                                        <input onclick="ButtonHandler('Sequences', 'addToPlaylist');"
                                            class="disableButtons singleSequencesButton multiSequencesButton"
                                            type="button" value="Add To Playlist" />
                                        <input onclick="ButtonHandler('Sequences', 'download');"
                                            class="disableButtons singleSequencesButton multiSequencesButton"
                                            type="button" value="Download" />
                                        <input onclick="ButtonHandler('Sequences', 'rename');"
                                            class="disableButtons singleSequencesButton" type="button" value="Rename" />
                                        <input onclick="ButtonHandler('Sequences', 'delete');"
                                            class="disableButtons singleSequencesButton multiSequencesButton"
                                            type="button" value="Delete" />
                                    </div>
                                    <div class="note"><strong>CTRL+Click to select multiple items. SHIFT+Click can be
                                            used to select a range of items.</strong></div>
                                </div>
                            </div>
                        </div>

                        <div class="tab-pane fade" id="tab-audio" role="tabpanel" aria-labelledby="tab-audio-tab">
                            <div id="divMusic">

                                <div class="backdrop">
                                    <div class="row justify-content-between fileDetailsHeader">
                                        <div class="col-auto">
                                            <h2>Music Files (.mp3/.ogg/.m4a/.flac/.aac/.wav/.m4p)</h2>
                                        </div>
                                        <div class="col-auto fileCountDetails">
                                            <div class="row">
                                                <div class="col-auto fileCountlabelHeading">Items</div>
                                                <div class="col-auto fileCountlabelValue"><span id="fileCount_Music"
                                                        class='badge text-bg-secondary'>0</span></div>
                                            </div>
                                        </div>

                                    </div>

                                    <div id="divMusicData" class="fileManagerDivData">
                                        <table id="tblMusic" class="tablesorter">
                                            <thead>
                                                <tr>
                                                    <th>File</th>
                                                    <th>Duration</th>
                                                    <th>Date Modified</th>
                                                </tr>
                                            </thead>
                                            <tbody>
                                            </tbody>
                                        </table>
                                    </div>

                                    <div class='form-actions'>
                                        <input onclick="ClearSelections('Music');" class="buttons" type="button"
                                            value="Clear" />
                                        <input onclick="ButtonHandler('Music', 'playInBrowser');"
                                            id="btnPlayMusicInBrowser" class="disableButtons singleMusicButton"
                                            type="button" value="Listen" />
                                        <? if (file_exists("/bin/mp3gain") || file_exists("/usr/bin/mp3gain") || file_exists("/opt/homebrew/bin/mp3gain") || file_exists("/usr/local/bin/mp3gain")) { ?>
                                            <input onclick="ButtonHandler('Music', 'mp3gain');" id="btnPlayMusicInBrowser"
                                                class="disableButtons singleMusicButton multiMusicButton" type="button"
                                                value="MP3Gain" />
                                        <? } ?>

                                        <input onclick="ButtonHandler('Music', 'addToPlaylist');"
                                            class="disableButtons singleMusicButton multiMusicButton" type="button"
                                            value="Add To Playlist" />
                                        <input onclick="ButtonHandler('Music', 'download');" id="btnDownloadMusic"
                                            class="disableButtons singleMusicButton multiMusicButton" type="button"
                                            value="Download" />
                                        <input onclick="ButtonHandler('Music', 'rename');" id="btnRenameMusic"
                                            class="disableButtons singleMusicButton" type="button" value="Rename" />
                                        <input onclick="ButtonHandler('Music', 'delete');" id="btnDeleteMusic"
                                            class="disableButtons singleMusicButton multiMusicButton" type="button"
                                            value="Delete" />
                                    </div>
                                    <div class="note"><strong>CTRL+Click to select multiple items</strong></div>
                                </div>

                            </div>
                        </div>

                        <div class="tab-pane fade" id="tab-video" role="tabpanel" aria-labelledby="tab-video-tab">
                            <div id="divVideo">

                                <div class="backdrop">
                                    <div class="row justify-content-between fileDetailsHeader">
                                        <div class="col-auto">
                                            <h2>Video Files (.mp4/.mkv/.avi/.mpg/.mov)</h2>
                                        </div>
                                        <div class="col-auto fileCountDetails">
                                            <div class="row">
                                                <div class="col-auto fileCountlabelHeading">Items</div>
                                                <div class="col-auto fileCountlabelValue"><span id="fileCount_Videos"
                                                        class='badge text-bg-secondary'>0</span></div>
                                            </div>
                                        </div>

                                    </div>

                                    <div id="divVideoData" class="fileManagerDivData">
                                        <table id="tblVideos" class="tablesorter">
                                            <thead>
                                                <tr>
                                                    <th class="tablesorter-header filenameColumn">File</th>
                                                    <th class="tablesorter-header">Duration</th>
                                                    <th class="tablesorter-header">Date Modified</th>
                                                </tr>
                                            </thead>
                                            <tbody>
                                            </tbody>
                                        </table>
                                    </div>

                                    <div class='form-actions'>
                                        <input onclick="ClearSelections('Videos');" class="buttons" type="button"
                                            value="Clear" />
                                        <input onclick="ButtonHandler('Videos', 'playInBrowser');"
                                            class="disableButtons singleVideosButton" type="button" value="View" />
                                        <input onclick="ButtonHandler('Videos', 'videoInfo');"
                                            class="disableButtons singleVideosButton" type="button"
                                            value="Video Info" />
                                        <input onclick="ButtonHandler('Videos', 'addToPlaylist');"
                                            class="disableButtons singleMusicButton multiMusicButton" type="button"
                                            value="Add To Playlist" />
                                        <input onclick="ButtonHandler('Videos', 'download');"
                                            class="disableButtons singleVideosButton multiVideosButton" type="button"
                                            value="Download" />
                                        <input onclick="ButtonHandler('Videos', 'rename');"
                                            class="disableButtons singleVideosButton" type="button" value="Rename" />
                                        <input onclick="ButtonHandler('Videos', 'delete');"
                                            class="disableButtons singleVideosButton multiVideosButton" type="button"
                                            value="Delete" />
                                    </div>
                                    <div class="note"><strong>CTRL+Click to select multiple items</strong></div>
                                </div>

                            </div>
                        </div>

                        <div class="tab-pane fade" id="tab-images" role="tabpanel" aria-labelledby="tab-images-tab">
                            <div id="divImages">

                                <div class="backdrop">
                                    <div class="row justify-content-between fileDetailsHeader">
                                        <div class="col-auto">
                                            <h2>Images</h2>
                                        </div>
                                        <div class="col-auto fileCountDetails">
                                            <div class="row">
                                                <div class="col-auto fileCountlabelHeading"><span class="">Items:<span>
                                                </div>
                                                <div class="col-auto fileCountlabelValue"><span id="fileCount_Images"
                                                        class='badge text-bg-secondary'>0</span></div>
                                            </div>
                                        </div>

                                    </div>

                                    <div id="divImagesData" class="fileManagerDivData">
                                        <table id="tblImages" class="tablesorter">
                                            <thead>
                                                <tr>
                                                    <th>File</th>
                                                    <th class="sorter-metric" data-metric-name-full="byte|Byte|BYTE"
                                                        data-metric-name-abbr="b|B">Size</th>
                                                    <th>Date Modified</th>
                                                    <th class="filter-false">Thumbnail</th>
                                                </tr>
                                            </thead>
                                            <tbody>
                                            </tbody>
                                        </table>
                                    </div>

                                    <div class='form-actions'>
                                        <input onclick="ClearSelections('Images');" class="buttons" type="button"
                                            value="Clear" />
                                        <input onclick="ButtonHandler('Images', 'viewImage');"
                                            class="disableButtons singleImagesButton" type="button" value="View" />
                                        <input onclick="ButtonHandler('Images', 'download');"
                                            class="disableButtons singleImagesButton multiImagesButton" type="button"
                                            value="Download" />
                                        <input onclick="ButtonHandler('Images', 'rename');"
                                            class="disableButtons singleImagesButton" type="button" value="Rename" />
                                        <input onclick="ButtonHandler('Images', 'delete');"
                                            class="disableButtons singleImagesButton multiImagesButton" type="button"
                                            value="Delete" />
                                    </div>
                                    <div class="note"><strong>CTRL+Click to select multiple items</strong></div>
                                </div>

                            </div>
                        </div>

                        <div class="tab-pane fade" id="tab-effects" role="tabpanel" aria-labelledby="tab-effects-tab">
                            <div id="divEffects">

                                <div class="backdrop">
                                    <div class="row justify-content-between fileDetailsHeader">
                                        <div class="col-auto">
                                            <h2>Effect Sequences (.eseq)</h2>
                                        </div>
                                        <div class="col-auto fileCountDetails">
                                            <div class="row">
                                                <div class="col-auto fileCountlabelHeading">Items</div>
                                                <div class="col-auto fileCountlabelValue"><span id="fileCount_Effects"
                                                        class='badge text-bg-secondary'>0</span></div>
                                            </div>
                                        </div>

                                    </div>

                                    <div id="divEffectsData" class="fileManagerDivData">
                                        <table id="tblEffects" class="tablesorter">
                                            <thead>
                                                <tr>
                                                    <th>File</th>
                                                    <th class="sorter-metric" data-metric-name-full="byte|Byte|BYTE"
                                                        data-metric-name-abbr="b|B">Size</th>
                                                    <th>Date Modified</th>
                                                </tr>
                                            </thead>
                                            <tbody>
                                            </tbody>
                                        </table>
                                    </div>

                                    <div class='form-actions'>
                                        <input onclick="ClearSelections('Effects');" class="buttons" type="button"
                                            value="Clear" />
                                        <input onclick="ButtonHandler('Effects', 'sequenceInfo');"
                                            class="disableButtons singleEffectsButton" type="button"
                                            value="Sequence Info" />
                                        <input onclick="ButtonHandler('Effects', 'download');"
                                            class="disableButtons singleEffectsButton multiEffectsButton" type="button"
                                            value="Download" />
                                        <input onclick="ButtonHandler('Effects', 'rename');"
                                            class="disableButtons singleEffectsButton" type="button" value="Rename" />
                                        <input onclick="ButtonHandler('Effects', 'delete');"
                                            class="disableButtons singleEffectsButton multiEffectsButton" type="button"
                                            value="Delete" />
                                    </div>
                                    <div class="note"><strong>CTRL+Click to select multiple items</strong></div>
                                </div>

                            </div>
                        </div>

                        <div class="tab-pane fade" id="tab-scripts" role="tabpanel" aria-labelledby="tab-scripts-tab">
                            <div id="divScripts">
                                <div class="backdrop">
                                    <div class="row justify-content-between fileDetailsHeader">
                                        <div class="col-auto">
                                            <h2>Scripts (.sh/.pl/.pm/.php/.py)</h2>
                                        </div>
                                        <div class="col-auto fileCountDetails">
                                            <div class="row">
                                                <div class="col-auto fileCountlabelHeading">Items</div>
                                                <div class="col-auto fileCountlabelValue"><span id="fileCount_Scripts"
                                                        class='badge text-bg-secondary'>0</span></div>
                                            </div>
                                        </div>

                                    </div>

                                    <div id="divScriptsData" class="fileManagerDivData">
                                        <table id="tblScripts" class="tablesorter">
                                            <thead>
                                                <tr>
                                                    <th>File</th>
                                                    <th class="sorter-metric" data-metric-name-full="byte|Byte|BYTE"
                                                        data-metric-name-abbr="b|B">Size</th>
                                                    <th>Date Modified</th>
                                                </tr>
                                            </thead>
                                            <tbody>
                                            </tbody>
                                        </table>
                                    </div>

                                    <div class='form-actions'>
                                        <input onclick="ClearSelections('Scripts');" class="buttons" type="button"
                                            value="Clear" />
                                        <input onclick="ButtonHandler('Scripts', 'viewFile');"
                                            class="disableButtons singleScriptsButton" type="button" value="View" />
                                        <input onclick="ButtonHandler('Scripts', 'runScript');"
                                            class="disableButtons singleScriptsButton" type="button" value="Run" />
                                        <input onclick="ButtonHandler('Scripts', 'editScript');"
                                            class="disableButtons singleScriptsButton" type="button" value="Edit" />
                                        <input onclick="ButtonHandler('Scripts', 'addToPlaylist');"
                                            class="disableButtons singleScriptsButton multiScriptsButton" type="button"
                                            value="Add To Playlist" />
                                        <input onclick="ButtonHandler('Scripts', 'download');"
                                            class="disableButtons singleScriptsButton multiScriptsButton" type="button"
                                            value="Download" />
                                        <input onclick="ButtonHandler('Scripts', 'copyFile');"
                                            class="disableButtons singleScriptsButton" type="button" value="Copy" />
                                        <input onclick="ButtonHandler('Scripts', 'rename');"
                                            class="disableButtons singleScriptsButton" type="button" value="Rename" />
                                        <input onclick="ButtonHandler('Scripts', 'delete');"
                                            class="disableButtons singleScriptsButton multiScriptsButton" type="button"
                                            value="Delete" />
                                    </div>
                                    <div class="note"><strong>CTRL+Click to select multiple items</strong></div>
                                </div>
                            </div>
                        </div>
                        <?php
                        foreach ($tabDivs as $td) {
                            include($td);
                        }
                        ?>
                        <div class="tab-pane fade" id="tab-logs" role="tabpanel" aria-labelledby="tab-logs-tab">
                            <div id="divLogs">
                                <div class="backdrop">
                                    <div class="row justify-content-between fileDetailsHeader">
                                        <div class="col-auto">
                                            <h2>Log Files</h2>
                                        </div>
                                        <div class="col-auto fileCountDetails">
                                            <div class="row">
                                                <div class="col-auto fileCountlabelHeading">Items</div>
                                                <div class="col-auto fileCountlabelValue"><span id="fileCount_Logs"
                                                        class='badge text-bg-secondary'>0</span></div>
                                            </div>
                                        </div>

                                    </div>
                                    <div id="divLogsData" class="fileManagerDivData">
                                        <table id="tblLogs" class="tablesorter">
                                            <thead>
                                                <tr>
                                                    <th>File</th>
                                                    <th class="sorter-metric" data-metric-name-full="byte|Byte|BYTE"
                                                        data-metric-name-abbr="b|B">Size</th>
                                                    <th>Date Modified</th>
                                                </tr>
                                            </thead>
                                            <tbody>
                                            </tbody>
                                        </table>
                                    </div>

                                    <div class='form-actions'>
                                        <input onclick="ClearSelections('Logs');" class="buttons" type="button"
                                            value="Clear" />
                                        <input onclick="DownloadZip('Logs');" class="buttons" type="button"
                                            value="Zip" />
                                        <input onclick="ButtonHandler('Logs', 'viewFile');"
                                            class="disableButtons singleLogsButton" type="button" value="View" />
                                        <input onclick="ButtonHandler('Logs', 'tailFile');"
                                            class="disableButtons singleLogsButton" type="button" value="Tail" />
                                        <input onclick="ButtonHandler('Logs', 'download');"
                                            class="disableButtons singleLogsButton multiLogsButton" type="button"
                                            value="Download" />
                                        <input onclick="ButtonHandler('Logs', 'delete');"
                                            class="disableButtons singleLogsButton multiLogsButton" type="button"
                                            value="Delete" />
                                    </div>
                                    <div class="note"><strong>CTRL+Click to select multiple items</strong></div>
                                </div>
                            </div>
                        </div>

                        <div class="tab-pane fade" id="tab-uploads" role="tabpanel" aria-labelledby="tab-uploads-tab">
                            <div id="divUploads">
                                <div class="backdrop">
                                    <div class="row justify-content-between fileDetailsHeader">
                                        <div class="col-auto">
                                            <h2>Uploaded Files</h2>
                                        </div>
                                        <div class="col-auto fileCountDetails">
                                            <div class="row">
                                                <div class="col-auto fileCountlabelHeading">Items</div>
                                                <div class="col-auto fileCountlabelValue"><span id="fileCount_Uploads"
                                                        class='badge text-bg-secondary'>0</span></div>
                                            </div>
                                        </div>

                                    </div>
                                    <div id="divUploadsData" class="fileManagerDivData">
                                        <table id="tblUploads" class="tablesorter">
                                            <thead>
                                                <tr>
                                                    <th>File</th>
                                                    <th class="sorter-metric" data-metric-name-full="byte|Byte|BYTE"
                                                        data-metric-name-abbr="b|B">Size</th>
                                                    <th>Date Modified</th>
                                                </tr>
                                            </thead>
                                            <tbody>
                                            </tbody>
                                        </table>
                                    </div>

                                    <div class='form-actions'>
                                        <input onclick="ClearSelections('Uploads');" class="buttons" type="button"
                                            value="Clear" />
                                        <input onclick="ButtonHandler('Uploads', 'download');"
                                            class="disableButtons singleUploadsButton multiUploadsButton" type="button"
                                            value="Download" />
                                        <input onclick="ButtonHandler('Uploads', 'copyFile');"
                                            class="disableButtons singleUploadsButton" type="button" value="Copy" />
                                        <input onclick="ButtonHandler('Uploads', 'rename');"
                                            class="disableButtons singleUploadsButton" type="button" value="Rename" />
                                        <input onclick="ButtonHandler('Uploads', 'delete');"
                                            class="disableButtons singleUploadsButton multiUploadsButton" type="button"
                                            value="Delete" />
                                    </div>
                                    <div class="note"><strong>CTRL+Click to select multiple items</strong></div>
                                </div>
                            </div>
                        </div>
                        <div class="tab-pane fade" id="tab-crashes" role="tabpanel" aria-labelledby="tab-crashes-tab">
                            <div id="divCrashes">
                                <div class="backdrop">
                                    <div class="row justify-content-between fileDetailsHeader">
                                        <div class="col-auto">
                                            <h2>Crash Reports</h2>
                                        </div>
                                        <div class="col-auto fileCountDetails">
                                            <div class="row">
                                                <div class="col-auto fileCountlabelHeading">Items</div>
                                                <div class="col-auto fileCountlabelValue"><span id="fileCount_Crashes"
                                                        class='badge text-bg-secondary'>0</span></div>
                                            </div>
                                        </div>

                                    </div>
                                    <div id="divCrashesData" class="fileManagerDivData">
                                        <table id="tblCrashes" class="tablesorter">
                                            <thead>
                                                <tr">
                                                    <th>File</th>
                                                    <th class="sorter-metric" data-metric-name-full="byte|Byte|BYTE"
                                                        data-metric-name-abbr="b|B">Size</th>
                                                    <th>Date Modified</th>
                                                    </tr>
                                            </thead>
                                            <tbody>
                                            </tbody>
                                        </table>
                                    </div>

                                    <div class='form-actions'>
                                        <input onclick="ClearSelections('Crashes');" class="buttons" type="button"
                                            value="Clear" />
                                        <input onclick="ButtonHandler('Crashes', 'download');"
                                            class="disableButtons singleCrashesButton multiCrashesButton" type="button"
                                            value="Download" />
                                        <input onclick="ButtonHandler('Crashes', 'delete');"
                                            class="disableButtons singleCrashesButton multiCrashesButton" type="button"
                                            value="Delete" />
                                    </div>
                                    <div class="note"><strong>CTRL+Click to select multiple items</strong></div>

                                </div>
                            </div>

                        </div>
                        <div class="tab-pane fade" id="tab-backups" role="tabpanel" aria-labelledby="tab-backups-tab">
                            <div id="divBackups">
                                <div class="backdrop">
                                    <div class="row justify-content-between fileDetailsHeader">
                                        <div class="col-auto">
                                            <h2>Backup Files</h2>
                                        </div>
                                        <div class="col-auto fileCountDetails">
                                            <div class="row">
                                                <div class="col-auto fileCountlabelHeading">Items</div>
                                                <div class="col-auto fileCountlabelValue"><span id="fileCount_Backups"
                                                        class='badge text-bg-secondary'>0</span></div>
                                            </div>
                                        </div>

                                    </div>
                                    <div id="divBackupsData" class="fileManagerDivData">
                                        <table id="tblBackups" class="tablesorter">
                                            <thead>
                                                <tr">
                                                    <th>File</th>
                                                    <th class="sorter-metric" data-metric-name-full="byte|Byte|BYTE"
                                                        data-metric-name-abbr="b|B">Size</th>
                                                    <th>Date Modified</th>
                                                    </tr>
                                            </thead>
                                            <tbody>
                                                <tr class='unselectableRow'>
                                                    <td colspan=8 align='center'>Loading Files...</td>
                                                </tr>
                                            </tbody>
                                        </table>
                                    </div>

                                    <div class='form-actions'>
                                        <input onclick="ClearSelections('Backups');" class="buttons" type="button"
                                            value="Clear" />
                                        <input onclick="ButtonHandler('Backups', 'download');"
                                            class="disableButtons singleBackupsButton multiBackupsButton" type="button"
                                            value="Download" />
                                        <input onclick="ButtonHandler('Backups', 'delete');"
                                            class="disableButtons singleBackupsButton multiBackupsButton" type="button"
                                            value="Delete" />
                                    </div>
                                    <div class="note"><strong>CTRL+Click to select multiple items</strong></div>

                                </div>
                            </div>

                        </div>
                        <div id='fileponduploader' class='fileponduploader ui-tabs-panel'>
                            <input type="file" class="filepond" id="filepondInput" multiple>
                        </div>
                    </div>
                </div>
                <div id="overlay">
                </div>
            </div>
        </div>



        <?php include 'common/footer.inc'; ?>

        <div id='bulkAddTemplate' style='display:none;'>
            <span id='bulkAddTypeTemplate' style='display:none;'></span>
            <table border=0 cellpadding=2 cellspacing=0>
                <tr>
                    <td><b>Playlist:</b></td>
                    <td><select id='bulkAddPlaylistTemplate'></select></td>
                </tr>
                <tr>
                    <td><b>Section:</b></td>
                    <td><select id='bulkAddPlaylistSectionTemplate'>
                            <option value='leadIn'>Lead In</option>
                            <option value='mainPlaylist' selected>Main Playlist</option>
                            <option value='leadOut'>Lead Out</option>
                        </select></td>
                </tr>
            </table>
            <br>
            <table class='fppSelectableRowTable'>
                <thead>
                    <th>File</th>
                    <th>Playlist Entry Type</th>
                    <th>Duration</th>
                </thead>
                <tbody id='bulkAddListTemplate'>
                </tbody>
            </table>
        </div>

</body>

</html>