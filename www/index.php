<!DOCTYPE html>
<html lang="en">

<head>
    <?php
    include 'common/htmlMeta.inc';
    require_once 'config.php';
    require_once 'common.php';

    include 'playlistEntryTypes.php';
    include 'common/menuHead.inc';
    ?>
    <script type="text/javascript" src="jquery/jquery.tablesorter/jquery.tablesorter.min.js"></script>
    <script type="text/javascript" src="jquery/jquery.tablesorter/jquery.tablesorter.widgets.min.js"></script>
    <script type="text/javascript" src="jquery/jquery.tablesorter/parsers/parser-network.min.js"></script>

    <link rel="stylesheet" href="jquery/jquery.tablesorter/css/theme.blue.css">

    <link rel="manifest" href="/manifest.json">

    <script>
        SetStatusRefreshSeconds(1);
        PlayEntrySelected = 1;


        function pageSpecific_PageLoad_DOM_Setup() {

        }

        function pageSpecific_PageLoad_PostDOMLoad_ActionsSetup() {
            //set mouse down actions on playlist:
            $('.playlistEntriesBody').on('mousedown', 'tr', function (event, ui) {
                $('#tblPlaylistDetails tr').removeClass('playlistSelectedEntry');
                $(this).addClass('playlistSelectedEntry');
                PlayEntrySelected = parseInt($(this).attr('id').substr(11));
            });

            //Setup Tablesorter on Sync Stats Table (shown on E1.31/DDP Input)
            $('#syncStatsTable').tablesorter({
                headers: {
                    1: {
                        sorter: 'ipAddress'
                    }
                },
                widthFixed: false,
                theme: 'fpp',
                widgets: ['zebra', 'filter', 'staticRow'],
                widgetOptions: {
                    filter_hideFilters: true
                }
            });

            // Pin Player Controls to top of index page
            //kill any existing instance in the browser
            if (typeof zp_playerControls !== 'undefined') {
                zp_playerControls.destroy();
            }
            zp_playerControls = new $.Zebra_Pin($('#controlsSection'), {
                top_spacing: $('.header').css('position') == 'fixed' ?
                    $('.header').outerHeight() : 0,
                contain: false,
                position: "relative"

            });

            //Reset the player controls when the visibility of the PlayerInfoMode section VolumeChangeInProgress
            respondToVisibility(document.getElementById("playerModeInfo"), visible => {
                if (visible) {
                    //visible
                    zp_playerControls.update();
                }
                else {
                    //not visible
                    zp_playerControls.update();
                }
            });

            //refresh player control positioning - tiny delay to help chromium based browsers
            setTimeout(function () { zp_playerControls.update(); }, 50);


        }

        function pageSpecific_ViewPortChange() {

            //Positioning Pinned divs working from top to bottom
            //Player Controls
            if (typeof zp_playerControls !== 'undefined') {
                switch (gblCurrentBootstrapViewPort) {
                    case 'xs':
                        //
                        zp_playerControls.settings.top_spacing = 0;
                        $('#playerModeInfo .controlsSectionToPin').css({
                            width: $('#programControl').width()
                        });
                        $('#playerModeInfo .controlsSectionToPin').css({
                            left: $('.mainContainer').position().left
                        });
                        break;
                    case 'sm':
                        //
                        zp_playerControls.settings.top_spacing = 0;
                        $('#playerModeInfo .controlsSectionToPin').css({
                            width: $('#programControl').width()
                        });
                        $('#playerModeInfo .controlsSectionToPin').css({
                            left: $('.mainContainer').position().left
                        });
                        break;
                    case 'md':
                        //
                        zp_playerControls.settings.top_spacing = 0;
                        $('#playerModeInfo .controlsSectionToPin').css({
                            width: $('#programControl').width()
                        });
                        $('#playerModeInfo .controlsSectionToPin').css({
                            left: $('.mainContainer').position().left
                        });
                        break;
                    case 'lg':
                        //has sticky page header
                        zp_playerControls.settings.top_spacing = $('.header').css('position') == 'fixed' ?
                            $('.header').outerHeight() : 0;
                        $('#playerModeInfo .controlsSectionToPin').css({
                            width: $('#programControl').width()
                        });
                        $('#playerModeInfo .controlsSectionToPin').css({
                            left: $('.mainContainer').position().left
                        });
                        break;
                    case 'xl':
                        //has sticky page header
                        zp_playerControls.settings.top_spacing = $('.header').css('position') == 'fixed' ?
                            $('.header').outerHeight() : 0;
                        $('#playerModeInfo .controlsSectionToPin').css({
                            width: $('#programControl').width()
                        });
                        $('#playerModeInfo .controlsSectionToPin').css({
                            left: $('.mainContainer').position().left
                        });
                        break;
                    case 'xxl':
                        //has sticky page header
                        zp_playerControls.settings.top_spacing = $('.header').css('position') == 'fixed' ?
                            $('.header').outerHeight() : 0;
                        $('#playerModeInfo .controlsSectionToPin').css({
                            width: $('#programControl').width()
                        });
                        $('#playerModeInfo .controlsSectionToPin').css({
                            left: $('.mainContainer').position().left
                        });
                        break;
                    default:
                }
                //refresh player control positioning
                zp_playerControls.update();
            }

        }

        //Define Volume change event steps for Player Mode
        function VolumeControlChange(value) {
            VolumeChangeInProgress = true;
            SetSpeakerIndicator(value);
            $('#volume').html(value);
            SetVolume(value);
        }

        //Define Volume change event steps for Remote Mode
        function RemoteVolumeControlChange(value) {
            VolumeChangeInProgress = true;
            SetSpeakerIndicator(value);
            $('#remoteVolume').html(value);
            SetVolume(value);
        }

        function PageSetup() {

            //Volume Controls
            //Store frequently elements in variables
            var slider = $('#slider');
            var rslider = $('#remoteVolumeSlider');

            //Triggered after the user slides a handle, if the value has changed
            slider.on('change', function (e) {
                VolumeChangeInProgress = true;
                //console.log("slider value changed");
                VolumeControlChange(this.value);
            });

            //Initialize the Remote Mode Slider
            //Triggered after the user slides a handle, if the value has changed
            rslider.on('change', function (e) {
                VolumeChangeInProgress = true;
                //console.log("slider value changed");
                RemoteVolumeControlChange(this.value);
            });

            SetupBanner();
        };

        function EnabledStats() {
            SetSetting("statsPublish", "Enabled", 2);
            $("#bannerRow").hide();
        }

        function SetupBanner() {
            let showIt = true;
            if (settings.hasOwnProperty('statsPublish') && settings.statsPublish != "Banner") {
                showIt = false;
            }

            if (showIt) {
                $("#bannerRow").html(
                    "Please consider enabling the collection of anonymous statistics " +
                    "on the hardware and features used to help us improve FPP in the " +
                    "future. You may preview the data or disable this banner on the " +
                    "<a href=\"settings.php#settings-privacy\">Privacy Settings Page</a>. " +
                    "<div style='margin-top:1em'><button class='buttons wideButton " +
                    "btn-outline-light' onClick='EnabledStats();'>Enable Stats</button></div>"
                ).show();
            }
        }

        function SetSpeakerIndicator(value) {
            var speaker = $('#speaker');
            var speaker_d_flex = $('#speaker_d_flex');
            var remoteSpeaker = $('#remoteSpeaker');

            if (value <= 5) {
                speaker.css('background-position', '0 0');
                speaker_d_flex.css('background-position', '0 0');
                remoteSpeaker.css('background-position', '0 0');
            } else if (value <= 25) {
                speaker.css('background-position', '0 -25px');
                speaker_d_flex.css('background-position', '0 -25px');
                remoteSpeaker.css('background-position', '0 -25px');
            } else if (value <= 75) {
                speaker.css('background-position', '0 -50px');
                speaker_d_flex.css('background-position', '0 -50px');
                remoteSpeaker.css('background-position', '0 -50px');
            } else {
                speaker.css('background-position', '0 -75px');
                speaker_d_flex.css('background-position', '0 -75px');
                remoteSpeaker.css('background-position', '0 -75px');
            };
        }

        function IncrementVolume() {
            VolumeChangeInProgress = true;
            var volume = parseInt($('#volume').html());
            volume += 1;
            if (volume > 100)
                volume = 100;

            updateVolumeUI(volume);
            SetVolume(volume);
        }

        function DecrementVolume() {
            VolumeChangeInProgress = true;
            var volume = parseInt($('#volume').html());
            volume -= 1;
            if (volume < 0)
                volume = 0;

            updateVolumeUI(volume);
            SetVolume(volume);
        }

        function PreviousPlaylistEntry() {
            var url = 'api/command/Prev Playlist Item';
            $.get(url)
                .done(function () { })
                .fail(function () { });
        }

        function NextPlaylistEntry() {
            var url = 'api/command/Next Playlist Item';
            $.get(url)
                .done(function () { })
                .fail(function () { });
        }
    </script>

    <title><?= $pageTitle ?></title>
</head>

<body class="is-loading" onLoad="PageSetup();GetFPPDmode();PopulatePlaylists(true);OnSystemStatusChange(GetFPPStatus);">
    <div id="bodyWrapper">
        <?php
        $activeParentMenuItem = 'status';
        include 'menu.inc';
        ?>


        <div class="mainContainer">
            <h1 class="title statusTitle">
                Status <? if ($settings['fppMode'] == 'remote') {
                    echo " - Remote Mode";
                } else {
                    echo " - Player Mode";
                }

                ?><span class="statusHostname"><?= $settings["HostName"] ?></span>
            </h1>

            <?php
            if (isset($settings["UnpartitionedSpace"]) && $settings["UnpartitionedSpace"] > 0):
                ?>
                <div id='spaceFlag' class="alert alert-danger" role="alert">
                    SD card has unused space. Go to <a href="settings.php#settings-storage">Storage Settings</a> to expand
                    the
                    file system or create a new storage partition.
                </div>
            <?php endif; ?>

            <div class="statusDivTopWrap">
                <div id="schedulerInfo" class="statusDiv statusDivTop" <? if ($settings['fppMode'] == 'remote') {
                    echo "style='display:none;'";
                } ?>>
                    <div class="statusTable statusDivTopRow">

                        <div class="statusDivTopCol">
                            <div class="labelHeading">Scheduler Status:</div>
                            <div class="labelValue"><span id='schedulerStatus'></span></div>
                        </div>
                        <div class="schedulerExtend schedulerEndTime statusDivTopCol ">
                            <div class="labelHeading">Extend Current Playlist:</div>
                            <div class='labelValue'>
                                <div class="btn-group">
                                    <button type='button' class="buttons btn-outline-light"
                                        onClick='ExtendSchedulePopup();'>
                                        <i class="fas fa-fw fa-calendar-plus"></i>Extend
                                    </button>
                                    <button type='button' class="buttons btn-outline-light"
                                        onClick='ExtendSchedule(5);'>
                                        <i class="fas fa-fw fa-clock"></i>+5min
                                    </button>
                                </div>
                            </div>
                        </div>
                        <div class="statusDivTopCol schedulerStartTime">
                            <div class='labelHeading'>Playlist Started at:</div>
                            <div class='labelValue' id='schedulerStartTime'></div>
                        </div>
                        <div class="statusDivTopCol schedulerEndTime">
                            <div class='labelHeading'>
                                <span id='schedulerStopType'></span> Stop at:
                            </div>
                            <div class='labelValue' id='schedulerEndTime'></div>
                        </div>
                        <div class="statusDivTopCol">
                            <div class="schedulerStatusCol">
                                <div>
                                    <div class="labelHeading">Next Playlist: </div>
                                    <div id='nextPlaylist' class="labelValue"></div>
                                </div>
                                <div>
                                    <div class="labelAction">
                                        <button class='buttons wideButton btn-outline-light'
                                            onClick='PreviewSchedule();'>
                                            <i class="fas fa-fw fa-calendar-alt"></i>Preview
                                        </button>
                                        <button class='buttons wideButton btn-outline-light'
                                            onClick='StartNextScheduledItemNow();'>
                                            <i class="fas fa-fw fa-running"></i>Start Next
                                        </button>
                                    </div>
                                </div>
                            </div>
                        </div>


                    </div>
                </div>
            </div>
            <div id="warningsRow" class="alert alert-danger">
                <div id="warningsTd">
                    <div id="warningsDiv"></div>
                </div>
            </div>
            <div id="bannerRow" class="alert alert-info">Banner Messages go here.</div>
            <div id="programControl" class="settings">

                <div class="statusPageLoading pageContent">
                    <div class="skeleton-loader">
                        <div></div>
                        <div class="sk-block sk-lg sk-rounded"></div>
                        <div></div>
                        <div></div>
                        <div></div>
                        <div></div>
                        <div></div>
                        <br><br>
                        <hr>

                        <div class="sk-btn"></div>
                        <div></div>
                    </div>
                </div>

                <!-- Remote Mode info -->
                <div id="remoteModeInfo" class='statusDiv pageContent'>

                    <div class='statusTable'>
                        <div class="row">
                            <div class="col-md-3">
                                <div class="playerStatusLabel">Remote Status:</div>
                                <div id="txtRemoteStatus" class="labelValue txtRemoteStatusLabelValue"></div>
                            </div>
                            <div class="col-md-3">
                                <div class="playerStatusLabel">Player IP:</div>
                                <div id="syncMaster" class="labelValue syncMasterLabelValue"></div>
                            </div>
                            <div class="col-md-3">
                                <div class="playerStatusLabel">Sequence Filename:</div>
                                <div id="txtRemoteSeqFilename" class="labelValue txtRemoteSeqFilenameLabelValue"></div>
                            </div>
                            <div class="col-md-3">
                                <div class="playerStatusLabel">Media Filename:</div>
                                <div id="txtRemoteMediaFilename" class="labelValue txtRemoteMediaFilenameLabelValue">
                                </div>
                            </div>
                        </div>
                    </div>

                    <div class="volumeControlsContainer d-flex">
                        <div class="ms-auto">
                            <div class="labelHeading">Volume</div>
                            <span id='remoteVolume' class='volume'></span>
                        </div>
                        <? if ($settings['disableAudioVolumeSlider'] != '1') {
                            ?>
                            <div class="volumeControls">
                                <button class='volumeButton buttons' onClick="DecrementVolume();"
                                    aria-label="DecrementVolume">
                                    <i class='fas fa-fw fa-volume-down'></i>
                                </button>
                                <input type="range" min="0" max="100" class="slider" id="remoteVolumeSlider"
                                    title="remoteVolumeSlider" oninput="RemoteVolumeControlChange(this.value)">
                                <button class='volumeButton buttons' onClick="IncrementVolume();"
                                    aria-label="IncrementVolume">
                                    <i class='fas fa-fw fa-volume-up'></i>
                                </button>
                                <span id='speaker_d_flex'></span> <!-- Volume -->
                            </div>
                            <?php
                        }
                        ?>
                    </div>
                    <hr>
                    <br>
                    <h2>MultiSync Packet Counts</h2>

                    <div class="row pb-1">
                        <div class="col-auto">
                            <input type='button' onClick='GetMultiSyncStats();' value='Update' class='buttons'>
                            <input type='button' onClick='ResetMultiSyncStats();' value='Reset' class='buttons'>
                        </div>
                        <div class="col-auto ms-auto">
                            <?php
                            PrintSettingCheckbox("MultiSync Stats Live Update", "syncStatsLiveUpdate", 0, 0, "1", "0");
                            ?>
                            Live Update Stats
                        </div>
                    </div>

                    <div class='fppTableWrapper backdrop'>
                        <div class='fppTableContents' role="region" aria-labelledby="syncStatsTable" tabindex="0">
                            <table id='syncStatsTable' class="tablesorter">
                                <thead>
                                    <tr>
                                        <th rowspan=2>Host</th>
                                        <th rowspan=2 data-filter='false'>Last<br>Rcvd</th>
                                        <th colspan=4 class="sorter-false">Sequence Sync</th>
                                        <th colspan=4 class="sorter-false">Media Sync</th>
                                        <th rowspan=2 data-filter='false'>Blank<br>Data</th>
                                        <th rowspan=2 data-filter='false'>Ping</th>
                                        <th rowspan=2 data-filter='false'>Plugin</th>
                                        <th rowspan=2 data-filter='false'>FPP<br>Cmd</th>
                                        <th rowspan=2 data-filter='false'>Errors</th>
                                    </tr>
                                    <tr>
                                        <th data-filter='false'>Open</th>
                                        <th data-filter='false'>Start</th>
                                        <th data-filter='false'>Stop</th>
                                        <th data-filter='false'>Sync</th>
                                        <th data-filter='false'>Open</th>
                                        <th data-filter='false'>Start</th>
                                        <th data-filter='false'>Stop</th>
                                        <th data-filter='false'>Sync</th>
                                    </tr>
                                </thead>
                                <tbody id='syncStats'>
                                </tbody>
                            </table>
                        </div>
                    </div>
                </div>
                <!-- Player/Master Mode Info -->
                <div id="playerModeInfo" class='statusDiv pageContent'>

                    <div id="playerStatusTop">
                        <div class='statusBoxLeft'>
                            <div class='statusTable container-fluid'>
                                <div class="row playerStatusRow">
                                    <div class="col-auto">
                                        <div id="txtPlayerStatusLabel" class="playerStatusLabel">Player Status:</div>
                                    </div>
                                    <div class="col-auto">
                                        <div id="txtPlayerStatus" class="labelValue playerStatusLabelValue"></div>
                                    </div>
                                </div>
                                <div class="row playlistSelectRow">
                                    <div class="col-auto playlistSelectCol">
                                        <select id="playlistSelect" title="playlistSelect" name="playlistSelect"
                                            class="form-control form-control-lg form-control-rounded has-shadow"
                                            size="1" onClick="PopulatePlaylistDetailsEntries(true,'');"
                                            onChange="PopulatePlaylistDetailsEntries(true,'');">
                                        </select>
                                    </div>
                                    <div class="col-auto playlistRepeatCol">
                                        <span class="settingLabelHeading">Repeat:</span>
                                        <input class="form-check-input" type="checkbox" value="" id="chkRepeat">
                                    </div>
                                </div>
                            </div>

                            <!-- Controls / Elapsed and Volume section --->

                            <div id="controlsSection" class="container-fluid bg-white">
                                <div class="row gx-0">
                                    <div class="col-xs-12 col-md-6 col-lg-8 col-xxl-7">
                                        <div id="playerControls"
                                            class="container text-center col-xs-12 col-md-8 col-lg-8 col-xxl-7">
                                            <button id="btnPlay" type="button"
                                                class="btn btn-rounded btn-success disableButtons"
                                                onClick="StartPlaylistNow();">
                                                <i class='fas fa-fw fa-play'></i>
                                                <span class="playerControlButton-text">Play</span>
                                            </button>
                                            <button id="btnPrev" type="button"
                                                class="btn btn-rounded btn-pleasant disableButtons"
                                                onClick="this.blur();PreviousPlaylistEntry();">
                                                <i class='fas fa-fw fa-step-backward'></i>
                                                <span class="playerControlButton-text">Previous</span>
                                            </button>
                                            <button id="btnNext" type="button"
                                                class="btn btn-rounded btn-pleasant disableButtons"
                                                onClick="this.blur();NextPlaylistEntry();">
                                                <i class='fas fa-fw fa-step-forward'></i>
                                                <span class="playerControlButton-text">Next</span>
                                            </button>
                                            <button id="btnStopGracefully" type="button"
                                                class="btn btn-rounded btn-graceful disableButtons"
                                                onClick="StopGracefully();">
                                                <i class='fas fa-fw fa-stop'></i>
                                                <span class="playerControlButton-text">
                                                    Stop
                                                    <span class="playerControlButton-text-important">Graceful</span>ly
                                                </span>
                                            </button>
                                            <button id="btnStopGracefullyAfterLoop" type="button"
                                                class="btn btn-rounded btn-detract disableButtons"
                                                onClick="StopGracefullyAfterLoop();">
                                                <i class='fas fa-fw fa-hourglass-half'></i>
                                                <span class="playerControlButton-text">
                                                    Stop
                                                    <span class="playerControlButton-text-important">After Loop</span>
                                                </span>
                                            </button>
                                            <button id="btnStopNow" type="button"
                                                class="btn btn-rounded btn-danger disableButtons" onClick="StopNow();">
                                                <i class='fas fa-fw fa-hand-paper'></i>
                                                <span class="playerControlButton-text">
                                                    Stop
                                                    <span class="playerControlButton-text-important">Now</span>
                                                </span>
                                            </button>
                                        </div>
                                    </div>

                                    <div class="col-md-6 col-lg-4 col-xxl-5">
                                        <div class="row gx-0">

                                            <div id='playerTime' class="col-xl-6 col-md-12">
                                                <div class='row w-100 d-flex justify-content-center'>

                                                    <div class="col-auto">
                                                        <p class="text-end"><span id="txtTimePlayed">02:00</span>
                                                        </p>
                                                        <p class="text-end">Elapsed</p>
                                                    </div>
                                                    <div id="progressBar"
                                                        class="col-4 d-flex flex-grow-1 align-items-center">
                                                        <div class="progress progress-linear w-100" role="progressbar"
                                                            aria-label="Playlist Item Progress" aria-valuenow="0"
                                                            aria-valuemin="0" aria-valuemax="100">
                                                            <div class="progress-bar bg-success" style="width: 0%">
                                                                <div id="txtPercentageComplete" class="">
                                                                    <p class="text-center"></p>
                                                                </div>
                                                            </div>
                                                        </div>
                                                    </div>

                                                    <div class="col-auto">
                                                        <p class="text-start"><span id="txtTimeRemaining">02:00</span>
                                                        <p class="text-start">Remaining</p>
                                                    </div>

                                                </div>
                                            </div>



                                            <div id='volumeControl' class="col-xl-6 col-md-12 flex-grow-1">
                                                <div class='row gx-0 d-flex justify-content-center'>
                                                    <div class="col-2">
                                                        <div class="labelHeading text-center">Volume</div>
                                                        <span id='volume' class='volume text-center'></span>
                                                    </div>
                                                    <? if ($settings['disableAudioVolumeSlider'] != '1') {
                                                        ?>
                                                    <div class="col-10 volumeControls">
                                                        <button class='volumeButton buttons'
                                                            onClick="DecrementVolume();" aria-label="DecrementVolume">
                                                            <i class='fas fa-fw fa-volume-down'></i>
                                                        </button>
                                                        <input type="range" min="0" max="100" class="slider" id="slider"
                                                            oninput="VolumeControlChange(this.value)">
                                                        <button class='volumeButton buttons'
                                                            onClick="IncrementVolume();" aria-label="IncrementVolume">
                                                            <i class='fas fa-fw fa-volume-up'></i>
                                                        </button>
                                                        <span id='speaker'></span> <!-- Volume -->
                                                        </div>
                                                        <?php
                                                    }
                                                    ?>
                                                </div>
                                            </div>

                                        </div>
                                    </div>
                                </div>

                            </div>
                            <!-- End of Controls, Elapsed and Volume section content -->
                        </div>

                        <hr>
                    </div>
                    <div id="playlistOuterScroll">

                        <div id="playerStatusBottom">
                            <?php include "playlistDetails.php"; ?>


                            <div id='deprecationWarning' class="hidden callout callout-danger">
                                <b>
                                    * - Playlist items marked with an asterisk have been deprecated
                                    and will be auto-upgraded the next time you edit the playlist.
                                </b>
                            </div>
                        </div>
                    </div>

                    <div class="verbosePlaylistItemSetting">
                        <?php PrintSetting('verbosePlaylistItemDetails', 'VerbosePlaylistItemDetailsToggled'); ?>
                        <?php PrintSetting('playlistAutoScroll'); ?>
                    </div>
                </div>
                <!-- Incoming E1.31/DDP/ArtNet stats (only displayed if "Channel Input" configured and enabled) -->
                <div id="bridgeModeInfo" class="pageContent" style="margin-top: 5px;">
                    <h3>E1.31/DDP/ArtNet Packets and Bytes Received</h3>
                    <table style='width: 100%' class='statusTable'>
                        <tr>
                            <td style="text-align: left;">
                                <input type='button' class="buttons" onClick='GetUniverseBytesReceived();'
                                    value='Update'>
                                <input type='button' class="buttons" onClick='ResetUniverseBytesReceived();'
                                    value='Reset'>
                            </td>
                            <td style="text-align: right;">
                                <?php PrintSettingCheckbox("E1.31 Live Update", "e131statsLiveUpdate", 0, 0, "1", "0"); ?>
                                Live Update Stats
                            </td>
                        </tr>
                    </table>
                    <hr>
                    <div class="container-fluid">
                        <div class="row">
                            <div class="col-auto" id="bridgeStatistics1"></div>
                            <div class="col-auto"></div>
                            <div class="col-auto" id="bridgeStatistics2"></div>
                            <div class="col-auto"></div>
                            <div class="col-auto" id="bridgeStatistics3"></div>
                        </div>
                    </div>
                </div>

            </div>
        </div>
        <?php include 'common/footer.inc' ?>
    </div>
    <div id='upgradePopup' title='FPP Upgrade' style="display: none">
        <textarea style='width: 99%; height: 97%;' disabled id='upgradeText'></textarea>
        <input id='rebootFPPAfterUpgradeButton' type='button' class='buttons' value='Reboot' onClick='Reboot();'
            style='display: none;'>
    </div>
</body>

</html>