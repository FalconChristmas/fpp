<!DOCTYPE html>
<html>
<head>
<?php
require_once 'config.php';
require_once 'common.php';

include 'playlistEntryTypes.php';
include 'common/menuHead.inc';
?>
<script type="text/javascript" src="jquery/jquery.tablesorter/jquery.tablesorter.js"></script>
<script type="text/javascript" src="jquery/jquery.tablesorter/jquery.tablesorter.widgets.js"></script>
<script type="text/javascript" src="jquery/jquery.tablesorter/parsers/parser-network.js"></script>

<link rel="stylesheet" href="jquery/jquery.tablesorter/css/theme.blue.css">

<link rel="manifest" href="/manifest.json" />

<script>
    SetStatusRefreshSeconds(1);
    PlayEntrySelected = 1;

    $(function() {
        $('.playlistEntriesBody').on('mousedown', 'tr', function(event,ui){
            $('#tblPlaylistDetails tr').removeClass('playlistSelectedEntry');
            $(this).addClass('playlistSelectedEntry');
            PlayEntrySelected = parseInt($(this).attr('id').substr(11));
        });

//        $('#playlistDetailsContents').resizable({
//            "handles": "s"
//        });

        $('#syncStatsTable').tablesorter({
            headers: {
                1: { sorter: 'ipAddress' }
            },
            widthFixed: false,
            theme: 'fpp',
            widgets: ['zebra', 'filter', 'staticRow'],
            widgetOptions: {
                filter_hideFilters : true
            }
        });
    });

</script>

<title><?=$pageTitle?></title>

    <script>
        function PageSetup() {
            //Store frequently elements in variables
            var slider  = $('#slider');
            var rslider  = $('#remoteVolumeSlider');
            //Call the Slider
            // slider.slider({
            //  //Config
            //  range: "min",
            //  min: 1,
            //  //value: 35,
            // });
            rslider.slider({
                //Config
                range: "min",
                min: 1,
                //value: 35,
            });


            // slider.slider({
            //  stop: function( event, ui ) {
            //      var value = slider.slider('value');

            //      SetSpeakerIndicator(value);
            //      $('#volume').html(value);
            //         $('#remoteVolume').html(value);
            //      SetVolume(value);
            //  }
            // });
            slider.on('change',function(e) {
                var value = slider.val();
                SetSpeakerIndicator(value);
                $('#volume').html(value);
                $('#remoteVolume').html(value);
                SetVolume(value);
            });
            rslider.on('change', function(e) {
                var value = rslider.val();
                SetSpeakerIndicator(value);
                $('#volume').html(value);
                $('#remoteVolume').html(value);
                SetVolume(value);
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
                    "Please consider enabling the collection of anonymous statistics "
                    + "on the hardware and features used to help us improve FPP in the "
                    + "future. You may preview the data or disable this banner on the "
                    + "<a href=\"settings.php#settings-privacy\">Privacy Settings Page</a>. "
                    + "<div style='margin-top:1em'><button class='buttons wideButton "
                    + "btn-outline-light' onClick='EnabledStats();'>Enable Stats</button></div>"
                ).show();
            }
        }

        function SetSpeakerIndicator(value) {
            var speaker = $('#speaker');
            var remoteSpeaker = $('#remoteSpeaker');

            if(value <= 5)
            {
                speaker.css('background-position', '0 0');
                remoteSpeaker.css('background-position', '0 0');
            }
            else if (value <= 25)
            {
                speaker.css('background-position', '0 -25px');
                remoteSpeaker.css('background-position', '0 -25px');
            }
            else if (value <= 75)
            {
                speaker.css('background-position', '0 -50px');
                remoteSpeaker.css('background-position', '0 -50px');
            }
            else
            {
                speaker.css('background-position', '0 -75px');
                remoteSpeaker.css('background-position', '0 -75px');
            };
        }

        function IncrementVolume()
        {
            var volume = parseInt($('#volume').html());
            volume += 1;
            if (volume > 100)
                volume = 100;

            updateVolumeUI(volume);
            SetVolume(volume);
        }

        function DecrementVolume()
        {
            var volume = parseInt($('#volume').html());
            volume -= 1;
            if (volume < 0)
                volume = 0;

            updateVolumeUI(volume);
            SetVolume(volume);
        }

        function PreviousPlaylistEntry()
        {
            var url = 'api/command/Prev Playlist Item';
            $.get(url)
                .done(function() {})
                .fail(function() {});
        }
        function NextPlaylistEntry()
        {
            var url = 'api/command/Next Playlist Item';
            $.get(url)
                .done(function() {})
                .fail(function() {});
        }

    </script>


</head>
<body class="is-loading" onLoad="PageSetup();GetFPPDmode();PopulatePlaylists(true);OnSystemStatusChange(GetFPPStatus);">
<div id="bodyWrapper">
<?php
$activeParentMenuItem = 'status';
include 'menu.inc';
?>


<div class="mainContainer">
<h1 class="title statusTitle">Status <span class="statusHostname"><?=$settings["HostName"]?></span></h1>

<?php
if (isset($settings["LastBlock"]) && $settings["LastBlock"] > 1000000 && $settings["LastBlock"] < 8000000):
?>
<div id='spaceFlag' class="alert alert-danger" role="alert">
     SD card has unused space.  Go to <a href="settings.php?tab=Storage">Storage Settings</a> to expand the
     file system or create a new storage partition.
     <input type='button' class='buttons btn-success' style='float: right' value='Leave Unused' onClick='SetSetting("LastBlock", "0", 0, 0); $("#spaceFlag").hide();'>
</div>
<?php endif;?>

<div class="statusDivTopWrap">
    <div id="schedulerInfo" class="statusDiv statusDivTop">
        <div class="statusTable statusDivTopRow">

            <div class="statusDivTopCol">
                <div class="labelHeading">Scheduler Status:</div>
                <div class="labelValue"><span id='schedulerStatus'></span></div>
            </div>
            <div class="schedulerExtend schedulerEndTime statusDivTopCol ">
                <div class="labelHeading">Extend Current Playlist:</div>
                <div class='labelValue' colspan='2'>
                    <div class="btn-group">
                        <button type='button' class="buttons btn-outline-light" onClick='ExtendSchedulePopup();'>
                            <i class="fas fa-fw fa-calendar-plus"></i>Extend
                        </button>
                        <button type='button' class="buttons btn-outline-light" onClick='ExtendSchedule(5);'>
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
                            <button class='buttons wideButton btn-outline-light' onClick='PreviewSchedule();'>
                                <i class="fas fa-fw fa-calendar-alt"></i>Preview
                            </button>
                            <button class='buttons wideButton btn-outline-light' onClick='StartNextScheduledItemNow();'>
                                <i class="fas fa-fw fa-running"></i>Start Next
                            </button>
                        </div>
                    </div>
                </div>
            </div>


        </div>
    </div>
</div>
<div id="warningsRow" class="alert alert-danger"><div id="warningsTd"><div id="warningsDiv"></div></div></div>
<div id="bannerRow" class="alert alert-info">Banner Messages go here.</div>
    <div id="programControl" class="settings">

            <div class="statusPageLoading pageContent">
                <div class="skeleton-loader">
                    <div ></div>
                    <div class="sk-block sk-lg sk-rounded"></div>
                    <div></div>
                    <div></div>
                    <div></div>
                    <div></div>
                    <div></div>
                    <br><br>
                    <hr>

                    <div class="sk-btn" ></div>
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
                            <div id="txtRemoteMediaFilename" class="labelValue txtRemoteMediaFilenameLabelValue"></div>
                        </div>
                    </div>
                </div>

                <div class="volumeControlsContainer d-flex">
                    <div class="ms-auto">
                        <div class="labelHeading">Volume</div>
                        <span id='remoteVolume' class='volume'></span>
                    </div>
                    <div class="volumeControls">
                        <button class='volumeButton buttons' onClick="DecrementVolume();">
                            <i class='fas fa-fw fa-volume-down'></i>
                        </button>
                        <input type="range" min="0" max="100" class="slider" id="remoteVolumeSlider">
                        <button class='volumeButton buttons' onClick="IncrementVolume();">
                            <i class='fas fa-fw fa-volume-up'></i>
                        </button>
                        <span id='speaker'></span> <!-- Volume -->
                    </div>
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
                        <table id='syncStatsTable'>
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
                                    <select id="playlistSelect" name="playlistSelect"
                                        class="form-control form-control-lg form-control-rounded has-shadow"
                                        size="1" onClick="PopulatePlaylistDetailsEntries(true,'');"
                                        onChange="PopulatePlaylistDetailsEntries(true,'');">
                                    </select>
                                </div>
                                <div class="col-auto playlistRepeatCol">
                                    <span class="settingLabelHeading">Repeat:</span>
                                    <input type="checkbox" id="chkRepeat"></input>
                                </div>
                            </div>
                        </div>
                        <div class="row statusPageControlsRow">
                            <div id="playerControls" class="col-md" >
                                <button id="btnPlay" class="buttons btn-rounded btn-success disableButtons"
                                    onClick="StartPlaylistNow();">

                                    <i class='fas fa-fw fa-play'></i>
                                    <span class="playerControlButton-text">Play</span>
                                </button>
                                <button id="btnPrev" class ="buttons btn-rounded btn-pleasant disableButtons"
                                    onClick="PreviousPlaylistEntry();">

                                    <i class='fas fa-fw fa-step-backward'></i>
                                    <span class="playerControlButton-text">Previous</span>
                                </button>
                                <button id= "btnNext" class ="buttons btn-rounded btn-pleasant disableButtons"
                                    onClick="NextPlaylistEntry();">

                                    <i class='fas fa-fw fa-step-forward'></i>
                                    <span class="playerControlButton-text">Next</span>
                                </button>
                                <button id="btnStopGracefully" class ="buttons btn-rounded btn-graceful disableButtons"
                                    onClick="StopGracefully();">

                                    <i class='fas fa-fw fa-stop'></i>
                                    <span class="playerControlButton-text">
                                        Stop
                                        <span class="playerControlButton-text-important">Graceful</span>ly
                                    </span>
                                </button>
                                <button id="btnStopGracefullyAfterLoop"
                                    class="buttons btn-rounded btn-detract disableButtons"
                                    onClick="StopGracefullyAfterLoop();">

                                    <i class='fas fa-fw fa-hourglass-half'></i>
                                    <span class="playerControlButton-text">
                                        Stop
                                        <span class="playerControlButton-text-important">After Loop</span>
                                    </span>
                                </button>
                                <button id= "btnStopNow" class ="buttons btn-rounded btn-danger disableButtons"
                                    onClick="StopNow();">

                                    <i class='fas fa-fw fa-hand-paper'></i>
                                    <span class="playerControlButton-text">
                                        Stop
                                        <span class="playerControlButton-text-important">Now</span>
                                    </span>
                                </button>
                            </div>

                            <div class="volumeControlsContainer col-md-auto">
                                <div>
                                    <div class="labelHeading">Volume</div>
                                    <span id='volume' class='volume'></span>
                                </div>
                                <div class="volumeControls">
                                        <button class='volumeButton buttons' onClick="DecrementVolume();">
                                            <i class='fas fa-fw fa-volume-down'></i>
                                        </button>
                                        <input type="range" min="0" max="100" class="slider" id="slider">
                                        <button class='volumeButton buttons' onClick="IncrementVolume();">
                                            <i class='fas fa-fw fa-volume-up'></i>
                                        </button>
                                    <span id='speaker'></span> <!-- Volume -->
                                </div>
                            </div>

                        </div>
                    </div>

                    <hr>
                </div>
                <div id="playlistOuterScroll">
                    <div class='statusBoxRight'>
                        <div id='playerTime' class='statusTable'>
                            <div>
                                <div class="labelHeading">Elapsed:</div>
                                <div id="txtTimePlayed" class="labelValue"></div>
                            </div>
                            <div>
                                <div class="labelHeading">Remaining:</div>
                                <div id="txtTimeRemaining" class="labelValue"></div>
                            </div>
                            <div>
                                <div class="labelHeading">Randomize:</div>
                                <div id="txtRandomize" class="labelValue"></div>
                            </div>
                        </div>
                    </div>
                    <div id="playerStatusBottom">
                        <?php include "playlistDetails.php";?>


                        <div id='deprecationWarning' class="hidden callout callout-danger">
                            <b>
                                * - Playlist items marked with an asterisk have been deprecated
                                and will be auto-upgraded the next time you edit the playlist.
                            </b>
                        </div>
                    </div>
                </div>

                <div class="verbosePlaylistItemSetting">
                    <?php PrintSetting('verbosePlaylistItemDetails', 'VerbosePlaylistItemDetailsToggled');?>
                    <?php PrintSetting('playlistAutoScroll');?>
                </div>
            </div>
            <!-- Bridge Mode stats -->
            <div id="bridgeModeInfo" class="pageContent" style="margin-top: 5px;">
                <h3>E1.31/DDP/ArtNet Packets and Bytes Received</h3>
                <table style='width: 100%' class='statusTable'>
                    <tr>
                        <td style="text-align: left;">
                            <input type='button' class="buttons" onClick='GetUniverseBytesReceived();' value='Update'>
                            <input type='button' class="buttons" onClick='ResetUniverseBytesReceived();' value='Reset'>
                        </td>
                        <td style="text-align: right;">
                            <?php PrintSettingCheckbox("E1.31 Live Update", "e131statsLiveUpdate", 0, 0, "1", "0");?>
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
<?php include 'common/footer.inc'?>
</div>
<div id='upgradePopup' title='FPP Upgrade' style="display: none">
    <textarea style='width: 99%; height: 97%;' disabled id='upgradeText'></textarea>
    <input id='rebootFPPAfterUpgradeButton' type='button' class='buttons'
        value='Reboot' onClick='Reboot();' style='display: none;'>
</div>
</body>
</html>
