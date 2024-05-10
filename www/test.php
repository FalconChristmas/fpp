<!DOCTYPE html>
<html lang="en">

<head>
    <script type="text/javascript"
        src="js/bootstrap.bundle.min.js?ref=<?= filemtime('js/bootstrap.bundle.min.js'); ?>"></script>

    <!-- finally FPP stuff, here so our CSS is last so we can override anything above -->
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <link rel="stylesheet"
        href="css/fpp-bootstrap/dist-new/fpp-bootstrap-5-3.css?ref=<?php echo filemtime('css/fpp-bootstrap/dist-new/fpp-bootstrap-5-3.css'); ?>">
    <link rel="stylesheet" href="css/fpp.css?ref=<?php echo filemtime('css/fpp.css'); ?>">

    <title>Test Page</title>
</head>

<body>
    <div class="container">
        <div class="row">
            <div class="col-md-7 col-xl-7 bg-primary-subtle">player controls
                <div id="playerControls" class="container col-xs-12 col-md-8 col-lg-8 col-xxl-7">
                    <button id="btnPlay" class="buttons btn-rounded btn-success disableButtons"
                        onClick="StartPlaylistNow();">

                        <i class='fas fa-fw fa-play'></i>
                        <span class="playerControlButton-text">Play</span>
                    </button>
                    <button id="btnPrev" class="buttons btn-rounded btn-pleasant disableButtons"
                        onClick="PreviousPlaylistEntry();">

                        <i class='fas fa-fw fa-step-backward'></i>
                        <span class="playerControlButton-text">Previous</span>
                    </button>
                    <button id="btnNext" class="buttons btn-rounded btn-pleasant disableButtons"
                        onClick="NextPlaylistEntry();">

                        <i class='fas fa-fw fa-step-forward'></i>
                        <span class="playerControlButton-text">Next</span>
                    </button>
                    <button id="btnStopGracefully" class="buttons btn-rounded btn-graceful disableButtons"
                        onClick="StopGracefully();">

                        <i class='fas fa-fw fa-stop'></i>
                        <span class="playerControlButton-text">
                            Stop
                            <span class="playerControlButton-text-important">Graceful</span>ly
                        </span>
                    </button>
                    <button id="btnStopGracefullyAfterLoop" class="buttons btn-rounded btn-detract disableButtons"
                        onClick="StopGracefullyAfterLoop();">

                        <i class='fas fa-fw fa-hourglass-half'></i>
                        <span class="playerControlButton-text">
                            Stop
                            <span class="playerControlButton-text-important">After Loop</span>
                        </span>
                    </button>
                    <button id="btnStopNow" class="buttons btn-rounded btn-danger disableButtons" onClick="StopNow();">

                        <i class='fas fa-fw fa-hand-paper'></i>
                        <span class="playerControlButton-text">
                            Stop
                            <span class="playerControlButton-text-important">Now</span>
                        </span>
                    </button>
                </div>
            </div>
            <div class="col-md-5 col-xl-3 bg-success ml-auto">elapsed time
                <!-- elapsed time -->

                <div id='playerTime' class='col-xs-12 col-md-4 col-lg-4 col-xxl-2 container'>
                    <div class="d-flex justify-content-center">
                        <div class="labelHeading col-4">
                            <p class="text-end"><span id="txtTimePlayed">02:00</span><br>
                                Elapsed</p>
                        </div>
                        <div class="progress progress-linear col-5" role="progressbar"
                            aria-label="Playlist Item Progress" aria-valuenow="0" aria-valuemin="0" aria-valuemax="100">
                            <div class="progress-bar bg-success" style="width: 0%">
                                <div id="txtPercentageComplete" class=""></div>
                            </div>
                        </div>
                        <div class="">
                            <div class="labelHeading col-3 text-start"><span
                                    id="txtTimeRemaining">02:00</span><br>Remaining</div>
                        </div>
                    </div>
                </div>
            </div>
            <div class="col-md-12 col-xl-2 bg-danger">volume controls
                <!-- Volume Controls -->
                <div class="volumeControlsContainer col-xs-12 col-md-12 col-lg-12 col-xxl-3 container">
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
    </div>
</body>
</html?