<!DOCTYPE html>
<html>
<head>
<?php	include 'common.php'; ?>
<?php	include 'common/menuHead.inc'; ?>
<script>
        allowMultisyncCommands = true;

		var TriggerEventSelected = "";
		var TriggerEventID = "";
		$(function() {
		$('#tblEventEntries').on('mousedown', 'tr', function(event,ui){
					$('#tblEventEntries tr').removeClass('eventSelectedEntry');
					$(this).addClass('eventSelectedEntry');
					var items = $('#tblEventEntries tr');
					TriggerEventSelected = $(this).find('td:eq(0)').text().replace(' \/ ', '_');
					TriggerEventID = $(this).attr('id');
					SetButtonState('#btnTriggerEvent','enable');
					SetButtonState('#btnEditEvent','enable');
					SetButtonState('#btnDeleteEvent','enable');

					if ($('#newEvent').is(":visible"))
							EditEvent();
		});
	});

var controlMajor;
var controlMinor;
function SaveControlChannels()
{
	controlMajor = $('#controlMajor').val();
	controlMinor = $('#controlMinor').val();

	$.get('fppjson.php?command=setSetting&key=controlMajor&value=' + controlMajor)
		.done(function() {
			$.get('fppjson.php?command=setSetting&key=controlMinor&value=' + controlMinor)
				.done(function() {
					$.jGrowl('Event Control Channels Saved');
					settings['controlMajor'] = controlMajor;
					settings['controlMinor'] = controlMinor;
				}).fail(function() {
						DialogError('Event Control Channels', 'Failed to save Minor Event Control Channel');
				});
		}).fail(function() {
				DialogError('Event Control Channels', 'Failed to save Major Event Control Channel');
		});
}


	function TriggerEvent()
	{
		var url = "fppxml.php?command=triggerEvent&id=" + TriggerEventSelected;
		var xmlhttp=new XMLHttpRequest();
		xmlhttp.open("GET",url,true);
		xmlhttp.setRequestHeader('Content-Type', 'text/xml');
		xmlhttp.send();
	}

	function AddEvent()
	{
		$('#newEventID').val('');
		$('#newEventName').val('');
		$('#newEventCommand').val('');
        NewEventCommandChanged();
		$('#newEvent').show();
	}

	function EditEvent()
	{
		var IDt = $('#event_' + TriggerEventSelected).find('td:eq(0)').text();
		var ID = IDt.replace(' \/ ', '_');
		$('#newEventID').val(ID);
		if ($('#newEventID').val() != ID) {
			$('#newEventID').prepend("<option value='" + ID + "' selected>" + IDt + "</option>");
		}

		$('#newEventName').val($('#event_' + TriggerEventSelected).find('td:eq(1)').text());
		$('#newEventCommand').val($('#event_' + TriggerEventSelected).find('td:eq(2)').text());
        NewEventCommandChanged();
        
        if (ID.charAt(1) == '_') {
            ID = "0" + ID;
        }
        if (ID.length == 4) {
            ID = ID.substring(0, 3) + "0" + ID.substring(3);
        }
        
        
        $.getJSON("api/events/" + ID, function(data) {
                 var count = 1;
                 $.each( data['args'], function( key, v ) {
                        var inp =  $("#tblNewEvent_arg_" + count);
                        if (inp.attr('type') == 'checkbox') {
                            var checked = false;
                            if (v == "true" || v == "1") {
                                checked = true;
                            }
                            inp.prop( "checked", checked);
                        } else {
                            inp.val(v);
                        }
                        count = count + 1;
                 })
                 });
        

		$('#newEvent').show();
	}

	function NewEventCommandChanged()
	{
        CommandSelectChanged('newEventCommand', 'tblNewEvent');
	}

	function InsertNewEvent(name, id, json)
	{
		var idStr = id.replace('_', ' / ');
        var row = "<tr id='event_" + id + "'><td class='eventTblID'>" + idStr +
            "</td><td class='eventTblName'>" + name +
            "</td><td class='eventTblCommand'>" + json['command'] +
            "</td><td class='eventTblArgs'>";

        $.each( json['args'], function( key, v ) {
               row += "\"";
               row += v;
               row += "\" ";
        })
        row += "</td></tr>";
         
        $('#tblEventEntries').append(row);
	}

	function UpdateExistingEvent(name, id, json)
	{
		var idStr = id.replace('_', ' / ');
		var row = $('#tblEventEntries tr#event_' + id);
		row.find('td:eq(0)').text(idStr);
		row.find('td:eq(1)').text(name);
		row.find('td:eq(2)').text(json['command']);
        
        var av = "";
        $.each( json['args'], function( key, v ) {
               av += "\"";
               av += v;
               av += "\" ";
        })
		row.find('td:eq(3)').text(av);
	}

	function SaveEvent()
	{
        //If event name is empty, warn the user and skip saving
        if (typeof ($('#newEventName').val()) === 'undefined' || $('#newEventName').val() == "") {
            DialogError("Error Saving Event", "Event Name must be set!")
        } else {
            var json = {};
            json = CommandToJSON('newEventCommand', 'tblNewEvent', json);
            json['id'] = $('#newEventID').val();
            json['name'] = $('#newEventName').val();
            
            $.ajax({
                   'dataType': "json",
                   'data': JSON.stringify(json),
                   'url': "fppxml.php?command=saveEvent",
                   'async': false,
                   'processData': false,
                   'method' :'POST',
                   'contentType': 'application/json',
                   success: function(data) {
                   }});
            
            if ($('#tblEventEntries tr#event_' + $('#newEventID').val()).attr('id')) {
                UpdateExistingEvent($('#newEventName').val(), $('#newEventID').val(), json);
            } else {
                InsertNewEvent($('#newEventName').val(), $('#newEventID').val(), json);
            }
            SetButtonState('#btnTriggerEvent', 'disable');
            SetButtonState('#btnEditEvent', 'disable');
            SetButtonState('#btnDeleteEvent', 'disable');
        }

	}

	function CancelNewEvent()
	{
		$('#newEvent').hide();
	}

	function DeleteEvent()
	{
		if (TriggerEventSelected == "")
			return;

		var url = "fppxml.php?command=deleteEvent&id=" + TriggerEventSelected;
		var xmlhttp=new XMLHttpRequest();
		xmlhttp.open("GET",url,true);
		xmlhttp.setRequestHeader('Content-Type', 'text/xml');
		xmlhttp.onreadystatechange = function () {
			if (xmlhttp.readyState == 4 && xmlhttp.status==200)
			{
				$('#tblEventEntries tr#' + TriggerEventID).remove();
				SetButtonState('#btnTriggerEvent','disable');
				SetButtonState('#btnEditEvent','disable');
				SetButtonState('#btnDeleteEvent','disable');
			}
		}
		xmlhttp.send();
	}

</script>

<title><? echo $pageTitle; ?></title>
</head>
<body onLoad="LoadCommandList($('#newEventCommand'));">
<div id="bodyWrapper">
<?php
	include 'menu.inc';

	$eventIDoptions = "";

	function PrintEventRows() {
		global $eventDirectory;
		global $eventIDoptions;

		$usedIDs = array();
		for ($i = 1; $i < 25; $i++) {
			$usedIDs[$i] = array();
            for ($j = 1; $j < 25; $j++) {
				$usedIDs[$i][$j] = 0;
			}
		}

		foreach (scandir($eventDirectory) as $eventFile) {
			if ($eventFile[0] != '.' && preg_match('/.fevt$/', $eventFile)) {
                $info = file_get_contents($eventDirectory . "/" . $eventFile);
				$info = json_decode($info, true);
                
                # probably can clean this up a bit
				$eventFile = preg_replace('/^0/', '', $eventFile);
				$eventFile = preg_replace('/_0/', '_', $eventFile);
				$eventFile = preg_replace('/.fevt$/', '', $eventFile);

				echo "<tr id='event_" . $eventFile . "'><td class='eventTblID'>" .
						$info['majorId'] . ' / ' . $info['minorId'] .
						"</td><td class='eventTblName'>" . $info['name'] .
						"</td><td class='eventTblCommand'>" . $info['command'] .
                        "</td><td class='eventTblArgs'>";
                
                foreach ($info['args'] as $value) {
                    echo "\"" . $value . "\"   ";
                }
                echo "</td></tr>\n";

				$usedIDs[$info['majorId']][$info['minorId']] = 1;
			}
		}

		$eventIDoptions = "";
		for ($i = 1; $i < 25; $i++) {
			for ($j = 1; $j < 25; $j++) {
				if ($usedIDs["$i"]["$j"] == 0) {
					$eventIDoptions .= sprintf("<option value='" .
						sprintf( "%d_%d", $i, $j) . "'>%d / %d</option>\n", $i, $j);
				}
			}
		}
	}

	function PrintEventIDoptions() {
		global $eventIDoptions;
		echo $eventIDoptions;
	}
	?>
<br/>
<div id="programControl" class="settings">
	<fieldset>
		<legend>Events</legend>
		<table>
			<tr><td colspan='5'>Event Control Channels: </td>
					<td width='30'></td>
					<td>Major:</td><td><? PrintSettingText("controlMajor", 1, 0, 6, 6); ?></td>
					<td width='20'></td>
					<td>Minor:</td><td><? PrintSettingText("controlMinor", 1, 0, 6, 6); ?></td>
					<td width='20'></td>
					<td>Use legacy 10x multiplier for Event ID's in Control Channels: <? PrintSettingCheckbox("10x Event IDs", "RawEventIDs", 2, 0, "0", "1", "", ""); ?></td>
					</tr>
		</table>
		<input type='Submit' value='Save' onClick='SaveControlChannels();'>
		<br>
		<br>
		<div>
            <div class='fppTableWrapper'>
                <div class='fppTableContents fullWidth'>
                    <table>
                        <thead>
                            <tr>
                                <th class='eventTblID'>ID</th>
                                <th class='eventTblName'>Name</th>
                                <th class='eventTblCommand'>Command</th>
                                <th class='eventTblArgs'>Arguments</th>
                            </tr>
                        </thead>
                        <tbody id="tblEventEntries" width="100%">
<? PrintEventRows(); ?>
                        </tbody>
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
				<table width="100%"  class="tblNewEvent" id="tblNewEvent">
					<tr><td width="20%">Event ID (Major/Minor):</td><td width="80%"><select id='newEventID'><? PrintEventIDoptions(); ?></select></td></tr>
					<tr><td width="20%">Event Name:</td><td width="80%"><input id="newEventName" class="default-value" type="text" value="" size="30" maxlength="60" /></td></tr>
					<tr><td width="20%">Effect Command:</td><td width="80%"><select id="newEventCommand" onChange="NewEventCommandChanged();"></select></td></tr>
				</table>
				<input id= "btnSaveNewEvent" type="button" class ="buttons" value="Save Event" onClick="SaveEvent();">
				<input id= "btnCancelNewEvent" type="button" class ="buttons" value="Cancel Edit" onClick="CancelNewEvent();">
			</div>
		</div>
	</fieldset>
  </div>
  <?php	include 'common/footer.inc'; ?>
</div>
</body>
</html>
