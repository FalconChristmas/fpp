<!DOCTYPE html>
<html lang="en">

<head>
    <?php
    include 'common/htmlMeta.inc';
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
    function getAllFiles($dir, $prefix = '') {
        $files = array();
        if (is_dir($dir)) {
            $items = scandir($dir);
            foreach ($items as $item) {
                if ($item !== '.' && $item !== '..') {
                    $fullPath = $dir . '/' . $item;
                    if (is_dir($fullPath)) {
                        $files = array_merge($files, getAllFiles($fullPath, $prefix . $item . '/'));
                    } else {
                        $files[] = array(
                            'name' => $prefix . $item,
                            'fullPath' => $fullPath
                        );
                    }
                }
            }
        }
        return $files;
    }
    ?>

    <script type="text/javascript" src="jquery/jquery.tablesorter/jquery.tablesorter.min.js"></script>
    <script type="text/javascript" src="jquery/jquery.tablesorter/jquery.tablesorter.widgets.min.js"></script>
    <script type="text/javascript" src="js/sugar/sugar.min.js"></script>
    <script type="text/javascript" src="jquery/jquery.tablesorter/parsers/parser-metric.min.js"></script>
    <script type="text/javascript" src="jquery/jquery.tablesorter/parsers/parser-date.min.js"></script>
    <script type="text/javascript" src="jquery/jquery.tablesorter/parsers/parser-date-two-digit-year.min.js"></script>
    <script type="text/javascript" src="jquery/jquery.tablesorter/widgets/widget-cssStickyHeaders.min.js"></script>
    <script type="text/javascript" src="jquery/jquery.tablesorter/extras/jquery.metadata.min.js"></script>
    <script type="text/javascript" src="js/fpp-filemanager.js?ref=<?= filemtime('js/fpp-filemanager.js'); ?>"></script>

    <script>
        const pluginFileExtensions = [ <? echo implode(", ", array_map(fn($arg) => "'$arg'", $fileExtensions)); ?>];
        GetAllFiles();
        var tablesorterOptions_USB = {
            theme: 'fpp',
            widthFixed: true,
            headerTemplate: '{content} {icon}',
            widgets: ['zebra', 'filter'],
            widgetOptions: {
                zebra: ['even', 'odd'],
                filter_reset: '.reset'
            },
            sortList: [[0,0]],
            headers: {
                0: { sorter: 'text' },
                1: { sorter: 'metric' },
                2: { sorter: 'sugar' }
            }
        };
        $(document).ready(function() {
            // Initialize tablesorter for USB tab when shown
            $('a[data-bs-target="#tab-usb"]').on('shown.bs.tab', function(e) {
                if (!$.tablesorter.hasWidget($('#tblUSB'), 'zebra')) { // Prevent re-init
                    $('#tblUSB').tablesorter(tablesorterOptions_USB);
                }
                $('#fileCount_USB').text($('#tblUSB tbody tr').length);
            });
            // Handle selection for USB table with visual highlight (using stock FPP selected style if available, or fallback)
            $('#tblUSB tbody').on('click', 'tr', function(e) {
                if (e.ctrlKey || e.metaKey) {
                    $(this).toggleClass('selected');
                } else if (e.shiftKey) {
                    var last = $('#tblUSB tbody tr.lastSelected');
                    var first = $(this).index();
                    var lastIndex = last.index();
                    if (first < lastIndex) {
                        $('#tblUSB tbody tr').slice(first, lastIndex + 1).addClass('selected');
                    } else {
                        $('#tblUSB tbody tr').slice(lastIndex, first + 1).addClass('selected');
                    }
                } else {
                    $('#tblUSB tbody tr.selected').removeClass('selected');
                    $(this).addClass('selected');
                }
                $('#tblUSB tbody tr.lastSelected').removeClass('lastSelected');
                $(this).addClass('lastSelected');
                UpdateUSBButtons();
            });
        });
        function UpdateUSBButtons() {
            var selected = $('#tblUSB tbody tr.selected');
            var count = selected.length;
            $('#divUSB .form-actions input:not([value="Clear"])').addClass('disableButtons');
            if (count > 0) {
                $('#divUSB .form-actions input[value="Download"]').removeClass('disableButtons');
                $('#divUSB .form-actions input[value="Delete"]').removeClass('disableButtons');
                if (count === 1) {
                    $('#divUSB .form-actions input[value="View"]').removeClass('disableButtons');
                    $('#divUSB .form-actions input[value="Copy"]').removeClass('disableButtons');
                    $('#divUSB .form-actions input[value="Rename"]').removeClass('disableButtons');
                    $('#divUSB .singleUSBButton').removeClass('disableButtons');
                } else {
                    $('#divUSB .multiUSBButton').removeClass('disableButtons');
                }
            }
        }
        function ClearSelections(type) {
            if (type === 'USB') {
                $('#tblUSB tbody tr.selected').removeClass('selected');
                UpdateUSBButtons();
            }
        }
        // Extend the original ButtonHandler to handle USB without overriding other types
        var originalButtonHandler = ButtonHandler; // Preserve original
        ButtonHandler = function(type, action) {
            if (type === 'USB') {
                var selected = $('#tblUSB tbody tr.selected');
                var files = [];
                selected.each(function() {
                    files.push($(this).data('file'));
                });
                var dir = selected.first().data('dir');
                if (action === 'download') {
                    files.forEach(function(file) {
                        window.location = 'api/file/' + dir + '/' + file + '?attach=1';
                    });
                } else if (action === 'delete') {
                    if (confirm('Delete selected files?')) {
                        files.forEach(function(file) {
                            $.ajax({
                                url: 'api/files/' + dir + '/' + file,
                                type: 'DELETE',
                                success: function() {
                                    location.reload();
                                }
                            });
                        });
                    }
                } else if (action === 'copyFile') {
                    if (files.length === 1) {
                        var file = files[0];
                        var newName = prompt('New file name:', file);
                        if (newName != null && newName != '') {
                            $.get('api/files/' + dir + '/copy/' + file + '/' + newName)
                                .done(function(data) {
                                    location.reload();
                                }).fail(function() {
                                    DialogError('Copy File', "Failed to copy file");
                                });
                        }
                    }
                } else if (action === 'rename') {
                    if (files.length === 1) {
                        var file = files[0];
                        var newName = prompt('New file name:', file);
                        if (newName != null && newName != '') {
                            $.get('api/files/' + dir + '/rename/' + file + '/' + newName)
                                .done(function(data) {
                                    location.reload();
                                }).fail(function() {
                                    DialogError('Rename File', "Failed to rename file");
                                });
                        }
                    }
                } else if (action === 'viewFile') {
                    if (files.length === 1) {
                        var file = files[0];
                        var ext = file.split('.').pop().toLowerCase();
                        var fullPath = dir + '/' + file;
                        if (['jpg', 'jpeg', 'png', 'gif'].includes(ext)) {
                            DoModalDialog({
                                id: 'usbImageViewer',
                                title: 'Image Viewer - ' + file,
                                body: '<img src="api/file/' + fullPath + '" style="max-width:100%; max-height:80vh;" onclick="ViewFullImage(\'api/file/' + fullPath + '\');">',
                                class: 'modal-xl modal-dialog-centered',
                                keyboard: true,
                                backdrop: true,
                                buttons: {
                                    Close: function() { CloseModalDialog('usbImageViewer'); }
                                }
                            });
                        } else if (['mp4', 'mkv', 'avi', 'mpg', 'mov'].includes(ext)) {
                            DoModalDialog({
                                id: 'usbVideoViewer',
                                title: 'Video Viewer - ' + file,
                                body: '<video controls style="max-width:100%; max-height:80vh;"><source src="api/file/' + fullPath + '" type="video/' + ext + '"></video>',
                                class: 'modal-xl modal-dialog-centered',
                                keyboard: true,
                                backdrop: true,
                                buttons: {
                                    Close: function() { CloseModalDialog('usbVideoViewer'); }
                                }
                            });
                        } else {
                            $.get('api/file/' + fullPath).done(function(data) {
                                var body = '<pre>' + $('<div/>').text(data).html() + '</pre>';
                                DoModalDialog({
                                    id: 'usbFileViewer',
                                    title: 'File Viewer - ' + file,
                                    body: body,
                                    class: 'modal-lg modal-dialog-scrollable',
                                    keyboard: true,
                                    backdrop: true,
                                    buttons: {
                                        Close: function() { CloseModalDialog('usbFileViewer'); }
                                    }
                                });
                            }).fail(function(jqXHR, textStatus, errorThrown) {
                                console.error('View File Error:', textStatus, errorThrown);
                                DialogError("View File", "Load Failed: " + textStatus);
                            });
                        }
                    }
                }
                return; // Stop here for USB
            }
            // Call original for other types
            originalButtonHandler(type, action);
        };
    </script>

    <?php
    exec("df -k " . $mediaDirectory . "/upload |awk '/\/dev\//{printf(\"%d\\n\", $5);}'", $output, $return_val);
    $freespace = (int) $output[0];
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
        /* Temporary style for selected rows, stock CSS is not applying */
        #tblUSB tr.selected {
            background-color: #d3d3d3 !important; /* Light gray highlight
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
                        <?php
                        $mountPoint = '/home/fpp/media/mnt';
                        $mountedSubdirs = glob($mountPoint . '/*', GLOB_ONLYDIR);
                        $isMounted = !empty($mountedSubdirs);
                        $usbSubdir = $isMounted ? basename($mountedSubdirs[0]) : '';
                        if ($isMounted) {
                        ?>
                        <li class="nav-item">
                            <a class="nav-link" id="tab-usb-tab" data-bs-toggle="pill" data-bs-target="#tab-usb" href="#tab-usb" role="tab" aria-controls="tab-usb">
                                USB
                            </a>
                        </li>
                        <?php } ?>
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



                    <script>
                        //set filter state
                        $('#fileManagerTableFilter').prop('checked', (settings.fileManagerTableFilter == "1" ? true : false));
                    </script>

                    <div class="tab-content" id="fileUploadTabsContent">

                        <div class="tab-pane fade show active" id="tab-sequence" role="tabpanel"
                            aria-labelledby="tab-sequence-tab">

                            <div id="divSequences">
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
                                                    <th class="sorter-sugar">Date Modified</th>
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
                                                class="disableButtons noDirButton singleSequencesButton" type="button"
                                                value="Play" />
                                            <input onclick="ButtonHandler('Sequences', 'playHere');"
                                                class="disableButtons noDirButton singleSequencesButton" type="button"
                                                value="Play Here" />
                                        <?php } ?>
                                        <input onclick="ButtonHandler('Sequences', 'sequenceInfo');"
                                            class="disableButtons noDirButton singleSequencesButton" type="button"
                                            value="Sequence Info" />
                                        <input onclick="ButtonHandler('Sequences', 'addToPlaylist');"
                                            class="disableButtons noDirButton singleSequencesButton multiSequencesButton"
                                            type="button" value="Add To Playlist" />
                                        <input onclick="ButtonHandler('Sequences', 'download');"
                                            class="disableButtons noDirButton singleSequencesButton multiSequencesButton"
                                            type="button" value="Download" />
                                        <input onclick="ButtonHandler('Sequences', 'rename');"
                                            class="disableButtons noDirButton singleSequencesButton" type="button"
                                            value="Rename" />
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
                                                    <th class="sorter-sugar">Date Modified</th>
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
                                            id="btnPlayMusicInBrowser"
                                            class="disableButtons noDirButton singleMusicButton" type="button"
                                            value="Listen" />
                                        <? if (file_exists("/bin/mp3gain") || file_exists("/usr/bin/mp3gain") || file_exists("/opt/homebrew/bin/mp3gain") || file_exists("/usr/local/bin/mp3gain")) { ?>
                                            <input onclick="ButtonHandler('Music', 'mp3gain');" id="btnPlayMusicInBrowser"
                                                class="disableButtons noDirButton singleMusicButton multiMusicButton"
                                                type="button" value="MP3Gain" />
                                        <? } ?>

                                        <input onclick="ButtonHandler('Music', 'addToPlaylist');"
                                            class="disableButtons noDirButton singleMusicButton multiMusicButton"
                                            type="button" value="Add To Playlist" />
                                        <input onclick="ButtonHandler('Music', 'download');" id="btnDownloadMusic"
                                            class="disableButtons noDirButton singleMusicButton multiMusicButton"
                                            type="button" value="Download" />
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
                            <div id="divVideos">

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
                                                    <th>File</th>
                                                    <th>Duration</th>
                                                    <th class="sorter-sugar">Date Modified</th>
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
                                            class="disableButtons noDirButton singleVideosButton"
                                            type="button" value="Watch" />
                                        <input onclick="ButtonHandler('Videos', 'addToPlaylist');"
                                            class="disableButtons noDirButton singleVideosButton multiVideosButton"
                                            type="button" value="Add To Playlist" />
                                        <input onclick="ButtonHandler('Videos', 'download');"
                                            class="disableButtons noDirButton singleVideosButton multiVideosButton"
                                            type="button" value="Download" />
                                        <input onclick="ButtonHandler('Videos', 'rename');"
                                            class="disableButtons singleVideosButton" type="button"
                                            value="Rename" />
                                        <input onclick="ButtonHandler('Videos', 'delete');"
                                            class="disableButtons singleVideosButton multiVideosButton"
                                            type="button" value="Delete" />
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
                                            <h2>Image Files (.jpg/.jpeg/.png/.gif)</h2>
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
                                                    <th class="sorter-sugar">Date Modified</th>
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
                                            class="disableButtons noDirButton singleImagesButton" type="button"
                                            value="View" />
                                        <input onclick="ButtonHandler('Images', 'addToPlaylist');"
                                            class="disableButtons noDirButton singleImagesButton multiImagesButton"
                                            type="button" value="Add To Playlist" />
                                        <input onclick="ButtonHandler('Images', 'download');"
                                            class="disableButtons noDirButton singleImagesButton multiImagesButton"
                                            type="button" value="Download" />
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
                                                    <th class="sorter-sugar">Date Modified</th>
                                                </tr>
                                            </thead>
                                            <tbody>
                                            </tbody>
                                        </table>
                                    </div>

                                    <div class='form-actions'>
                                        <input onclick="ClearSelections('Effects');" class="buttons" type="button"
                                            value="Clear" />
                                        <input onclick="ButtonHandler('Effects', 'addToPlaylist');"
                                            class="disableButtons noDirButton singleEffectsButton multiEffectsButton"
                                            type="button" value="Add To Playlist" />
                                        <input onclick="ButtonHandler('Effects', 'download');"
                                            class="disableButtons noDirButton singleEffectsButton multiEffectsButton"
                                            type="button" value="Download" />
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
                                            <h2>Scripts (.sh/.pl/.pm/.php/.py/.js)</h2>
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
                                                    <th class="sorter-sugar">Date Modified</th>
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
                                            class="disableButtons noDirButton singleScriptsButton" type="button"
                                            value="View" />
                                        <input onclick="ButtonHandler('Scripts', 'runScript');"
                                            class="disableButtons noDirButton singleScriptsButton" type="button"
                                            value="Run" />
                                        <input onclick="ButtonHandler('Scripts', 'editScript');"
                                            class="disableButtons noDirButton singleScriptsButton" type="button"
                                            value="Edit" />
                                        <input onclick="ButtonHandler('Scripts', 'addToPlaylist');"
                                            class="disableButtons noDirButton singleScriptsButton multiScriptsButton"
                                            type="button" value="Add To Playlist" />
                                        <input onclick="ButtonHandler('Scripts', 'download');"
                                            class="disableButtons noDirButton singleScriptsButton multiScriptsButton"
                                            type="button" value="Download" />
                                        <input onclick="ButtonHandler('Scripts', 'copyFile');"
                                            class="disableButtons noDirButton singleScriptsButton" type="button"
                                            value="Copy" />
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
                        <?php if ($isMounted) { ?>
                        <div class="tab-pane fade" id="tab-usb" role="tabpanel" aria-labelledby="tab-usb-tab">
                            <div id="divUSB">
                                <div class="backdrop">
                                    <div class="row justify-content-between fileDetailsHeader">
                                        <div class="col-auto">
                                            <h2>USB Files (<?php echo $usbSubdir; ?>)</h2>
                                        </div>
                                        <div class="col-auto fileCountDetails">
                                            <div class="row">
                                                <div class="col-auto fileCountlabelHeading">Items</div>
                                                <div class="col-auto fileCountlabelValue">
                                                    <span id="fileCount_USB" class='badge text-bg-secondary'>
                                                    <?php
                                                    $usbPath = $mountPoint . '/' . $usbSubdir;
                                                    $allFiles = getAllFiles($usbPath);
                                                    $fileCount = count($allFiles);
                                                    echo $fileCount;
                                                    ?>
                                                    </span>
                                                </div>
                                            </div>
                                        </div>
                                    </div>
                                    <div id="divUSBData" class="fileManagerDivData">
                                        <table id="tblUSB" class="tablesorter">
                                            <thead>
                                                <tr>
                                                    <th>File</th>
                                                    <th class="sorter-metric" data-metric-name-full="byte|Byte|BYTE"
                                                        data-metric-name-abbr="b|B">Size</th>
                                                    <th class="sorter-sugar">Date Modified</th>
                                                </tr>
                                            </thead>
                                            <tbody>
                                                <?php
                                                if (!empty($allFiles)) {
                                                    foreach ($allFiles as $file) {
                                                        echo "<tr class='fppSelectableRow' data-file='" . htmlspecialchars($file['name']) . "' data-dir='mnt/" . htmlspecialchars($usbSubdir) . "'>";
                                                        echo "<td>" . htmlspecialchars($file['name']) . "</td>";
                                                        echo "<td>" . humanFileSize(filesize($file['fullPath'])) . "</td>";
                                                        echo "<td>" . date('Y-m-d H:i:s', filemtime($file['fullPath'])) . "</td>";
                                                        echo "</tr>";
                                                    }
                                                } else {
                                                    echo "<tr class='unselectableRow'><td colspan='3' align='center'>No files found</td></tr>";
                                                }
                                                ?>
                                            </tbody>
                                        </table>
                                    </div>
                                    <div class='form-actions'>
                                        <input onclick="ClearSelections('USB');" class="buttons" type="button" value="Clear" />
                                        <input onclick="ButtonHandler('USB', 'viewFile');"
                                            class="buttons disableButtons noDirButton singleUSBButton" type="button"
                                            value="View" />
                                        <input onclick="ButtonHandler('USB', 'download');"
                                            class="buttons disableButtons noDirButton singleUSBButton multiUSBButton"
                                            type="button" value="Download" />
                                        <input onclick="ButtonHandler('USB', 'copyFile');"
                                            class="buttons disableButtons noDirButton singleUSBButton" type="button"
                                            value="Copy" />
                                        <input onclick="ButtonHandler('USB', 'rename');"
                                            class="buttons disableButtons singleUSBButton" type="button" value="Rename" />
                                        <input onclick="ButtonHandler('USB', 'delete');"
                                            class="buttons disableButtons singleUSBButton multiUSBButton" type="button"
                                            value="Delete" />
                                    </div>
                                    <div class="note"><strong>CTRL+Click to select multiple items</strong></div>
                                </div>
                            </div>
                        </div>
                        <?php } ?>
                        <li class="nav-item">
                            <a class="nav-link " id="tab-logs-tab" data-bs-toggle="pill" data-bs-target="#tab-logs"
                                href="#tab-logs" role="tab" aria-controls="tab-logs">
                                Logs
                            </a>
                        </li>
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
                                                    <th class="sorter-sugar">Date Modified</th>
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
                                            class="disableButtons noDirButton singleLogsButton" type="button"
                                            value="View" />
                                        <input onclick="ButtonHandler('Logs', 'tailFile');"
                                            class="disableButtons noDirButton singleLogsButton" type="button"
                                            value="Tail" />
                                        <input onclick="ButtonHandler('Logs', 'tailFollow');"
                                            class="disableButtons noDirButton singleLogsButton" type="button"
                                            value="Tail Follow" />
                                        <input onclick="ButtonHandler('Logs', 'download');"
                                            class="disableButtons noDirButton singleLogsButton multiLogsButton"
                                            type="button" value="Download" />
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
                                                    <th class="sorter-sugar">Date Modified</th>
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
                                            class="disableButtons noDirButton singleUploadsButton multiUploadsButton"
                                            type="button" value="Download" />
                                        <input onclick="ButtonHandler('Uploads', 'copyFile');"
                                            class="disableButtons noDirButton singleUploadsButton" type="button"
                                            value="Copy" />
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
                                                <tr>
                                                    <th>File</th>
                                                    <th class="sorter-metric" data-metric-name-full="byte|Byte|BYTE"
                                                        data-metric-name-abbr="b|B">Size</th>
                                                    <th class="sorter-sugar">Date Modified</th>
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
                                            class="disableButtons noDirButton singleCrashesButton multiCrashesButton"
                                            type="button" value="Download" />
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
                                                <tr>
                                                    <th>File</th>
                                                    <th class="sorter-metric" data-metric-name-full="byte|Byte|BYTE"
                                                        data-metric-name-abbr="b|B">Size</th>
                                                    <th class="sorter-sugar">Date
                                                        Modified</th>
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
                                            class="disableButtons noDirButton singleBackupsButton multiBackupsButton"
                                            type="button" value="Download" />
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
                    <div id="tablefilterChk">
                        <?php PrintSetting('fileManagerTableFilter', 'FileManagerFilterToggled'); ?>
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