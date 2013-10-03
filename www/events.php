<!DOCTYPE html>
<html>
<head>
<?php	include 'common/menuHead.inc'; ?>
<script type="text/javascript" src="js/fpp.js"></script>
<script src="http://code.jquery.com/jquery-1.9.1.js"></script>
<script src="http://code.jquery.com/ui/1.10.3/jquery-ui.js"></script>
<script>
		$(function() {
		$('#tblEventEntries').on('mousedown', 'tr', function(event,ui){
					$('#tblEventEntries tr').removeClass('eventSelectedEntry');
					$(this).addClass('eventSelectedEntry');
					var items = $('#tblEventEntries tr');
					TriggerEventSelected = $(this).find('td:eq(0)').text().replace(' \/ ', '_');
					SetButtonState('#btnTriggerEvent','enable');
					SetButtonState('#btnEditEvent','enable');
					SetButtonState('#btnDeleteEvent','enable');

					if ($('#newEvent').is(":visible"))
							EditEvent();
		});
	});
</script>

<title>Falcon PI Player - Events</title>
</head>
<body onLoad="GetFPPDmode();StatusPopulatePlaylists();setInterval(updateFPPStatus,1000);GetVolume();">
<div id="bodyWrapper">
<?php
	include 'menu.inc';

	$eventIDoptions = "";

	function PrintEventRows()
	{
		global $eventDirectory;
		global $eventIDoptions;

		$usedIDs = array();
		for ($i = 1; $i < 25; $i++)
		{
			$usedIDs[$i] = array();
			for ($j = 1; $j < 25; $j++)
			{
				$usedIDs[$i][$j] = 0;
			}
		}

		foreach (scandir($eventDirectory) as $eventFile)
		{
			if($eventFile != '.' && $eventFile != '..' && preg_match('/.fevt$/', $eventFile))
			{
				$info = parse_ini_file($eventDirectory . "/" . $eventFile);

		# probably can clean this up a bit
				$eventFile = preg_replace('/^0/', '', $eventFile);
				$eventFile = preg_replace('/_0/', '_', $eventFile);
				$eventFile = preg_replace('/.fevt$/', '', $eventFile);

				$info['effect'] = preg_replace('/.eseq$/', '', $info['effect']);

				echo "<tr id='event_" . $eventFile . "'><td width='5%'>" .
						$info['majorID'] . ' / ' . $info['minorID'] .
						"</td><td width='20%'>" . $info['name'] .
						"</td><td width='31%'>" . $info['effect'] .
						"</td><td width='7%'>" . $info['startChannel'] .
						"</td><td width='31%'>" . $info['script'] . "</td></tr>\n";

				$usedIDs[$info['majorID']][$info['minorID']] = 1;
			}
		}

		$eventIDoptions = "";
		for ($i = 1; $i < 25; $i++)
		{
			for ($j = 1; $j < 25; $j++)
			{
				if ($usedIDs["$i"]["$j"] == 0)
				{
					$eventIDoptions .= sprintf("<option value='" .
						sprintf( "%d_%d", $i, $j) .
						"'>%d / %d</option>\n", $i, $j);
				}
			}
		}
	}

	function PrintEventIDoptions()
	{
		global $eventIDoptions;

		echo $eventIDoptions;
	}

	function PrintEffectOptions()
	{
		global $sequenceDirectory;
		foreach(scandir($sequenceDirectory) as $seqFile)
		{
			if($seqFile != '.' && $seqFile != '..' && preg_match('/.eseq$/', $seqFile))
			{
				$seqFile = preg_replace('/.eseq$/', '', $seqFile);
				
				echo "<option value='" . $seqFile . "'>" . $seqFile . "</option>\n";
			}
		}
	}

	?>
<br/>
<div id="programControl" class="settings">
	<fieldset>
		<legend>Events</legend>
		<div id="daemonControl">
			<table width= "100%">
				<tr>
					<td width = "20%"> FPPD Mode: </td>
					<td width = "25%"><div id='textFPPDmode'>Player Mode</div>
					<td width = "40%">&nbsp;</td>
					<td width = "15%">&nbsp;</td>
				</tr>
				<tr>
					<td width = "20%"> FPPD Status: </td>
					<td id = "daemonStatus" width = "25%"></td>
					<td width = "15%">&nbsp;</td>
					<td width = "40%">&nbsp;</td>
				</tr>
				<tr>
					<td> FPP Time: </td>
					<td id = "fppTime" colspan = "3"></td>
				</tr>
			</table>
		</div>
		<hr>
		<div id="bytesTransferred"><H3>Bytes Transferred</H3>
		<div id="bridgeStatistics1"></div>
		<div id="bridgeStatistics2"></div>
		<div class="clear"></div>
		</div>
		<div id="playerStatus">
			<table width= "100%">
				<tr>
					<td width = "20%">Player Status: </td>
					<td id = "txtPlayerStatus" width = "46%"></td>
					<td id = "txtTimePlayed" width = "17%"></td>
					<td id = "txtTimeRemaining" width = "17%"></td>
				</tr>
			</table>
		</div>
		<div id="playerControls" style="margin-top:5px">
			<input id= "btnPlay" type="button" class ="buttons"value="Start a Playlist" onClick="location.href = '/index.php';">
			<input id= "btnStopGracefully" type="button" class ="buttons"value="Stop Gracefully" onClick="StopGracefully();">
			<input id= "btnStopNow" type="button" class ="buttons" value="Stop Now" onClick="StopNow();">
		</div>

		<hr>

		<div>
			<div>
				<table width="100%">
					<tr>
						<td><b><center>Events</center></b></td>
					</tr>
				</table>
			</div>
			<div id="eventList" class="unselectable">
				<table id="tblEventListHeader" width="100%">
					<tr class="eventListHeader">
						<td width="5%">ID</td>
						<td width="20%">Name</td>
						<td width="31%">Effect</td>
						<td width="7%">Start Ch.</td>
						<td width="31%">Script</td>
					</tr>
				</table>
				<div id= "eventListContents">
				<table id="tblEventEntries" width="100%">
<? PrintEventRows(); ?>
				 </table>
			</div>
			</div>

			<div id="eventControls" style="margin-top:5px">
				<input id= "btnAddEvent" type="button" class ="buttons" value="Add Event" onClick="AddEvent();">
				<input id= "btnTriggerEvent" type="button" class ="disableButtons" value="Trigger Event" onClick="TriggerEvent();">
				<input id= "btnEditEvent" type="button" class ="disableButtons" value="Edit Event" onClick="EditEvent();">
				<input id= "btnDeleteEvent" type="button" class ="disableButtons" value="Delete Event" onClick="DeleteEvent();">
			 </div>
			<div id="newEvent" style="display: none;">
				<hr>
				<table width="100%">
					<tr>
						<td><b><center>Event Editor</center></b></td>
					</tr>
				</table>
				<table width="100%">
					<tr><td width="20%">Event ID:</td><td width="80%"><select id='newEventID'><? PrintEventIDoptions(); ?></select></td></tr>
					<tr><td width="20%">Event Name:</td><td width="80%"><input id="newEventName" class="default-value" type="text" value="" size="30" maxlength="60" /></td></tr>
					<tr><td width="20%">Effect Sequence:</td><td width="80%"><select id="newEventEffect">
							<option value=''>NONE</option>
<? PrintEffectOptions(); ?>
</select></td></tr>
					<tr><td width="20%">Effect Start Channel:</td><td width="80%"><input id="newEventStartChannel" class="default-value" type="text" value="" size="5" maxlength="5" /></td></tr>
					<tr><td width="20%">Event Script:</td><td width="80%"><input id="newEventScript" class="default-value" type="text" value="" size="60" maxlength="1024" /></td></tr>
				</table>
				<input id= "btnSaveNewEvent" type="button" class ="buttons" value="Save Event" onClick="SaveEvent();">
				<input id= "btnCancelNewEvent" type="button" class ="buttons" value="Cancel Edit" onClick="CancelNewEvent();">
			</div>
		</div>
	</fieldset>
</div>
<?php	include 'common/footer.inc'; ?>
</body>
</html>
