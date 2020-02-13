<!DOCTYPE html>
<html>
<head>
<?php
require_once('config.php');
require_once('common.php');
include 'playlistEntryTypes.php';
include 'common/menuHead.inc';
?>
<script>
    PlayEntrySelected = 0;
    PlaySectionSelected = '';

    $(function() {
        $('.playlistEntriesBody').on('mousedown', 'tr', function(event,ui){
            $('#tblPlaylistDetails tbody tr').removeClass('playlistSelectedEntry');
            $(this).addClass('playlistSelectedEntry');
            PlayEntrySelected = parseInt($(this).attr('id').substr(11)) - 1;
            PlaySectionSelected = $(this).parent().attr('id').substr(11);
        });

//        $('#playlistDetailsContents').resizable({
//            "handles": "s"
//        });
    });

</script>

<title><? echo $pageTitle; ?></title>

	<script>
		$(function() {

			//Store frequently elements in variables
			var slider  = $('#slider');
			//Call the Slider
			slider.slider({
				//Config
				range: "min",
				min: 1,
				//value: 35,
			});


			$('#slider').slider({
				stop: function( event, ui ) {
					var value = slider.slider('value');

					SetSpeakerIndicator(value);
					$('#volume').html(value);
					SetVolume(value);
				}
			});

            $(document).tooltip({
                content: function() {
                    $('.ui-tooltip').hide();
                    var id = $(this).attr('id');
                    id = id.replace('_img', '_tip');
                    return $('#' + id).html();
                },
                hide: { delay: 1000 }
            });

		});

	function SetSpeakerIndicator(value) {
		var speaker = $('#speaker');

		if(value <= 5)
		{
			speaker.css('background-position', '0 0');
		}
		else if (value <= 25)
		{
			speaker.css('background-position', '0 -25px');
		}
		else if (value <= 75)
		{
			speaker.css('background-position', '0 -50px');
		}
		else
		{
			speaker.css('background-position', '0 -75px');
		};
	}

	function IncrementVolume()
	{
		var volume = parseInt($('#volume').html());
		volume += 1;
		if (volume > 100)
			volume = 100;
		$('#volume').html(volume);
		$('#slider').slider('value', volume);
		SetSpeakerIndicator(volume);
		SetVolume(volume);
	}

	function DecrementVolume()
	{
		var volume = parseInt($('#volume').html());
		volume -= 1;
		if (volume < 0)
			volume = 0;
		$('#volume').html(volume);
		$('#slider').slider('value', volume);
		SetSpeakerIndicator(volume);
		SetVolume(volume);
	}

    function PreviousPlaylistEntry()
    {
        var xmlhttp=new XMLHttpRequest();
        var url = "fppxml.php?command=playlistPrevEntry";
        xmlhttp.open("GET",url,true);
        xmlhttp.setRequestHeader('Content-Type', 'text/xml');
        xmlhttp.send();
    }
    function NextPlaylistEntry()
    {
        var xmlhttp=new XMLHttpRequest();
        var url = "fppxml.php?command=playlistNextEntry";
        xmlhttp.open("GET",url,true);
        xmlhttp.setRequestHeader('Content-Type', 'text/xml');
        xmlhttp.send();
    }

	</script>


</head>
<body onLoad="GetFPPDmode();PopulatePlaylists(true);GetFPPStatus();bindVisibilityListener();GetVolume();">
<div id="bodyWrapper">
<?php
	include 'menu.inc';
  ?>
<br/>
<?php
    if (isset($settings["LastBlock"]) && $settings["LastBlock"] > 1000000 && $settings["LastBlock"] < 7400000) {
    ?>
<div id='upgradeFlag' style='background-color:red'>SD card has unused space.  Go to <a href="advancedsettings.php">Advanced Settings</a> to expand the file system or create a new storage partition.</div>
<br>
<?php
    }
?>
<div id="programControl" class="settings">
    <fieldset>
        <legend>Program Control</legend>
        <!-- Main FPP Mode/status/time header w/ sensor info -->
        <div id='daemonControl' class='statusDiv'>
            <div class='statusBoxLeft'>
                <table class='statusTable'>
                    <tr>
                        <th>FPPD Mode:</th>
                        <td>
                            <select id="selFPPDmode" onChange="SetFPPDmode();">
                                <option id="optFPPDmode_Player" value="2">Player (Standalone)</option>
                                <option id="optFPPDmode_Master" value="6">Player (Master)</option>
                                <option id="optFPPDmode_Remote" value="8">Player (Remote)</option>
                                <option id="optFPPDmode_Bridge" value="1">Bridge</option>
                            </select>
                            <input type="button" id="btnDaemonControl" class ="buttons" value="" onClick="ControlFPPD();">
                            </td>
                    </tr>
                    <tr>
                        <th>FPPD Status:</th>
                        <td id = "daemonStatus"></td>
                    </tr>
                    <tr>
                        <th>FPP Time:</th>
                        <td id="fppTime"></td>
                    </tr>
                    <tr id="warningsRow"><td colspan="4" id="warningsTd"><div id="warningsDiv"></div></td>
                    </tr>
                </table>
            </div>
            <div class='statusBoxRight'>
                <div id="sensorData">
                </div>
            </div>
            <div class='clear'></div>
            <hr>
        </div>

        <!-- Bridge Mode stats -->
        <div id="bridgeModeInfo">
            <H3>E1.31/DDP/ArtNet Packets and Bytes Received</H3>
            <table style='width: 100%' class='statusTable'>
                <tr>
                    <td align='left'>
                        <input type='button' onClick='GetUniverseBytesReceived();' value='Update'>
                    </td>
                    <td align='right'>
<? PrintSettingCheckbox("E1.31 Live Update", "e131statsLiveUpdate", 0, 0, "1", "0"); ?> Live Update Stats
                    </td>
                </tr>
            </table>
            <hr>
            <div id="bridgeStatistics1"></div>
            <div id="bridgeStatistics2"></div>
            <div class="clear"></div>
        </div>

        <!-- Remote Mode info -->
        <div id="remoteModeInfo" class='statusDiv'>
            <table class='statusTable'>
                <tr><th>Remote Status:</th>
                    <td id='txtRemoteStatus'></td></tr>
                <tr><th>Sequence Filename:</th>
                    <td id='txtRemoteSeqFilename'></td></tr>
                <tr><th>Media Filename:</th>
                    <td id='txtRemoteMediaFilename'></td></tr>
            </table>
        </div>

        <!-- Player/Master Mode Info -->
        <div id="playerModeInfo" class='statusDiv'>
            <div id='schedulerInfo'>
                <div class='statusBoxLeft'>
                    <table class='statusTable'>
                        <tr><th>Scheduler Status:</th>
                            <td id='schedulerStatus'></td>
                        </tr>
                        <tr>
                            <th>Next Playlist: </th>
                            <td id='nextPlaylist' colspan=9></td>
                        </tr>
<?
//PrintSetting('scheduling');
?>
                    </table>
                </div>
                <div class='statusBoxRight'>
                    <table class='statusTable'>
                        <tr>
                            <th class='schedulerStartTime'>Started at:</th>
                            <td class='schedulerStartTime' id='schedulerStartTime'></td>
                        </tr>
                        <tr>
                            <th class='schedulerEndTime'><span id='schedulerStopType'></span> Stop at:</th>
                            <td class='schedulerEndTime' id='schedulerEndTime'></td>
                        </tr>
                        <tr>
                            <td class='schedulerEndTime schedulerExtend' colspan='2'>
                                <input type='button' value='Extend' onClick='ExtendSchedulePopup();'>
                                <input type='button' value='+5m' onClick='ExtendSchedule(5);'>
                            </td>
                        </tr>
                    </table>
                </div>
                <div class="clear"></div>
                <hr>
            </div>

            <div id="playerStatusTop">
                <div class='statusBoxLeft'>
                    <table class='statusTable'>
                        <tr>
                            <th>Player Status:</th>
                            <td id="txtPlayerStatus" style="text-align:left; width=80%"></td>
                        </tr>
                        <tr>
                            <th>Playlist:</th>
                            <td><select id="playlistSelect" name="playlistSelect" size="1" onClick="SelectPlaylistDetailsEntryRow();PopulatePlaylistDetailsEntries(true,'');" onChange="PopulatePlaylistDetailsEntries(true,'');"></select>
                                &nbsp;&nbsp;&nbsp;
                                <b>Repeat:</b> <input type="checkbox" id="chkRepeat"></input></td>
                        </tr>
                        <tr>
                            <th>Volume [<span id='volume' class='volume'></span>]:</th>
                            <td>
                                <input type="button" class='volumeButton' value="-" onClick="DecrementVolume();">
                                <span id="slider"></span> <!-- the Slider -->
                                <input type="button" class='volumeButton' value="+" onClick="IncrementVolume();">
                                <span id='speaker'></span> <!-- Volume -->
                            </td>
                        </tr>
                        <tr>
                            <th>Playlist Details:</th>
                        </tr>
                    </table>
                    <table>
                        <tr>
                            <td><? PrintSetting('verbosePlaylistItemDetails', 'VerbosePlaylistItemDetailsToggled'); ?></td>
                        </tr>
                    </table>
                </div>
                <div class='statusBoxRight'>
                    <table id='playerTime' class='statusTable'>
                        <tr>
                            <th>Elapsed:</th>
                            <td id="txtTimePlayed"></td>
                        </tr>
                        <tr>
                            <th>Remaining:</th>
                            <td id="txtTimeRemaining"></td>
                        </tr>
                    </table>
                </div>
                <div class="clear"></div>
            </div>

            <div id="playerStatusBottom">
<?
include "playlistDetails.php";
?>

                <div id="playerControls" style="margin-top:5px">
                    <input id= "btnPlay" type="button"  class ="buttons"value="Play" onClick="StartPlaylistNow();">
                    <input id= "btnPrev" type="button"  class ="buttons"value="Previous" onClick="PreviousPlaylistEntry();">
                    <input id= "btnNext" type="button"  class ="buttons"value="Next" onClick="NextPlaylistEntry();">
                    <input id= "btnStopGracefully" type="button"  class ="buttons" value="Stop Gracefully" onClick="StopGracefully();">
                    <input id= "btnStopGracefullyAfterLoop" type="button"  class ="buttons" value="Stop After Loop" onClick="StopGracefullyAfterLoop();">
                    <input id= "btnStopNow" type="button" class ="buttons" value="Stop Now" onClick="StopNow();">
                </div>
                <div id='deprecationWarning' style='display:none'><font color='red'><b>* - Playlist items marked with an asterisk have been deprecated and will be auto-upgraded the next time you edit the playlist.</b></font></div>
            </div>
        </div>
    </fieldset>
</div>
<?php	include 'common/footer.inc'; ?>
</div>
</body>
</html>
