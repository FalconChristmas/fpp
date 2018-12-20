<?php
require_once('playlistentry.php');

if (!isset($simplifiedPlaylist))
	$simplifiedPlaylist = 0;
if (!isset($allowDelete))
	$allowDelete = 1;

$playlistNames = Array();

if ($handle = opendir($settings['playlistDirectory']))
{
	while (($file = readdir($handle)) !== false)
	{
		if (preg_match('/\.json$/', $file))
		{
			$file = preg_replace('/\.json$/', '', $file);
			array_push($playlistNames, $file);
		}
	}
	closedir($handle);
}

sort($playlistNames);

?>

<link rel="stylesheet" type="text/css" href="css/jquery.timepicker.css">
<script type="text/javascript" src="js/jquery.timepicker.min.js"></script>

<script language="Javascript">
var simplifiedPlaylist = 0;

function playlistEditorDocReady() {
	$('.default-value').each(function() {
		var default_value = this.value;
		$(this).focus(function() {
			if(this.value == default_value) {
				this.value = '';
				$(this).css('color', '#333');
			}
		});
		$(this).blur(function() {
			if(this.value == '') {
				this.value = default_value;
				$(this).css('color', '#999');
			}
		});
	});

	//make table rows sortable
	$('.tblCreatePlaylistEntries_tbody').sortable({
		start: function (event, ui) {
			start_pos = ui.item.index();

			start_parent = $(ui.item).parent().attr('id');
			start_id = start_parent.replace(/tblPlaylist/i, "");
		},
		update: function(event, ui) {
			if (this === ui.item.parent()[0]) {
				var end_pos = ui.item.index();
				var parent = $(ui.item).parent().attr('id');
				var end_id = parent.replace(/tblPlaylist/i, "");

				var rowsInNew = $('#' + start_parent + ' >tr').length;
				if (rowsInNew > 1)
					$('#' + parent + 'PlaceHolder').remove();

				var rowsLeft = $('#' + start_parent + ' >tr').length;
				if (rowsLeft == 0)
					$('#' + start_parent).html('<tr id="' + parent + 'PlaceHolder"><td colspan=4>&nbsp;</td></tr>');

				if ((end_id != start_id) || (end_pos != start_pos)) {
					PlaylistEntryPositionChanged(end_id,end_pos,start_id,start_pos);
					var pl = document.getElementById("txtPlaylistName").value;
					PopulatePlayListEntries(pl,false,end_pos);
				}
			}
		},
		beforeStop: function (event, ui) {
			//undo the firefox fix.
			if (navigator.userAgent.toLowerCase().match(/firefox/) && ui.offset !== undefined) {
				$(window).unbind('scroll.sortableplaylist');
				ui.helper.css('margin-top', 0);
			}
		},
		helper: function (e, ui) {
			ui.children().each(function () {
				$(this).width($(this).width());
			});
			return ui;
		},
		item: 'tr',
		connectWith: '.tblCreatePlaylistEntries_tbody',
		scroll: true,
		stop: function (event, ui) {
			//SAVE YOUR SORT ORDER                    
			}
		}).disableSelection();

	$('#tblCreatePlaylist tbody').on('mousedown', 'tr', function(event,ui){
		$('#tblCreatePlaylist tbody tr').removeClass('selectedEntry');
		$(this).addClass('selectedEntry');
		var items = $('#tblCreatePlaylist tbody tr');
		lastPlaylistEntry = parseInt($(this).attr('id').substr(11));
		lastPlaylistSection = $(this).parent().attr('id').substr(11);
	});

	$('#txtNewPlaylistName').on('focus',function() {
		$(this).select();
	});

	$('.time').timepicker({'timeFormat': 'H:i:s', 'typeaheadHighlight': false});

	PlaylistTypeChanged();
}

function MediaChanged()
{
	if ($('#autoSelectMatches').is(':checked') == false)
		return;

	var value = $('#selMedia').val().replace(/\.ogg|\.mp3|\.mp4|\.m4a/i, "");

	var seq = document.getElementById("selSequence")
	for (var i = 0; i < seq.length; i++) {
		if (seq.options[i].value.replace(/\.fseq/i, "") == value)
			$('#selSequence').val(seq.options[i].value);
	}
}

function SequenceChanged()
{
	if ($('#autoSelectMatches').is(':checked') == false)
		return;

	var value = $('#selSequence').val().replace(/\.fseq/i, "");

	var media = document.getElementById("selMedia")
	for (var i = 0; i < media.length; i++) {
		if (media.options[i].value.replace(/\.ogg|\.mp3|\.mp4|\.m4a/i, "") == value)
			$('#selMedia').val(media.options[i].value);
	}
}

function DynamicSubTypeChanged()
{
	var subType = $('#dynamicSubType').val();
	var inputStr = "Invalid Dynamic Sub Type: " + subType;

	if (subType == 'file')
	{
		inputStr = "<input id='dynamicData' type='text' size='60' maxlength='255' placeholder='Full File Name of JSON file'>";
	}
	else if (subType == 'plugin')
	{
		inputStr = '<? PrintDynamicJSONPluginOptions(); ?>';
	}
	else if (subType == 'url')
	{
		inputStr = "<input id='dynamicData' type='text' size='60' maxlength='255' placeholder='URL to JSON data'>";
	}
	else if (subType == 'command')
	{
		inputStr = '<? PrintScriptOptions("dynamicData"); ?>';
	}

	$('#dynamicDataWrapper').html(inputStr);
}

$(document).ready(function() {
	playlistEditorDocReady();
});

simplifiedPlaylist = <? echo $simplifiedPlaylist; ?>;
</script>

<?php 
function PrintMediaOptions()
{
	global $settings;
	global $musicDirectory;
	global $videoDirectory;
	echo "<select id=\"selMedia\" size=\"1\" onChange='MediaChanged();'>";

	$mediaEntries = array_merge(scandir($musicDirectory),scandir($videoDirectory));
	sort($mediaEntries);
	foreach($mediaEntries as $mediaFile) 
	{
		if($mediaFile != '.' && $mediaFile != '..')
		{
			echo "<option value=\"" . $mediaFile . "\">" . $mediaFile . "</option>";
		}
	}
	echo "</select>";

	echo "   Video Output: <select id=\"videoOut\" size=\"1\">";
	echo "  <option value='--Default--'>Default</option>";
	echo "  <option value='--Disabled--'>Disabled</option>";

	$f = fopen($settings['channelMemoryMapsFile'], "r");
	if ($f == FALSE) {
		fclose($f);
	} else {
		while (!feof($f)) {
			$line = fgets($f);
			if ($line == "")
				continue;
			$entry = explode(",", $line, 7);
			printf( "<option value='%s'>%s</option>\n", $entry[0], $entry[0]);
		}
		fclose($f);
	}
	echo "</select>";

}			
 
function PrintSequenceOptions()
{
	global $sequenceDirectory;
	echo "<select id=\"selSequence\" size=\"1\" onChange='SequenceChanged();'>";
	foreach(scandir($sequenceDirectory) as $seqFile) 
	{
		if($seqFile != '.' && $seqFile != '..' && !preg_match('/.eseq$/', $seqFile))
		{
			echo "<option value=\"" . $seqFile . "\">" . $seqFile . "</option>";
		}
	}
	echo "</select>";
}

function PrintVideoOptions()
{
	global $videoDirectory;
	echo "<select id=\"selVideo\" size=\"1\">";
	foreach(scandir($videoDirectory) as $videoFile)
	{
		if($videoFile != '.' && $videoFile != '..')
		{
			echo "<option value=\"" . $videoFile . "\">" . $videoFile . "</option>";
		}
	}
	echo "</select>";
}

function PrintDynamicJSONPluginOptions()
{
	global $settings;
	
	echo "<select id=\"dynamicData\" size=\"1\">";

	$dir = $settings['pluginDirectory'];
	if ($dh = opendir($dir))
	{
		while (($file = readdir($dh)) !== false)
		{
			if ((!in_array($file, array('.', '..'))) &&
				(is_dir($dir . '/' . $file)) &&
				(file_exists($dir . '/' . $file . '/playlistCallback.php')))
			{
				echo "<option value=\"" . $file . "\">" . $file . "</option>";
			}

		}
		closedir($dh);
	}
	echo "</select>";
}

function PrintScriptOptions($id)
{
	global $scriptDirectory;
	echo "<select id=\"" . $id . "\" size=\"1\">";

	foreach(scandir($scriptDirectory) as $scriptFile)
	{
		if($scriptFile != '.' && $scriptFile != '..')
		{
			echo "<option value=\"" . $scriptFile . "\">" . $scriptFile . "</option>";
		}
	}
	echo "</select>";

	echo "<br>Arguments: <input id=\"" . $id . "_args\" type=\"text\" size=\"60\" maxlength=\"255\">";
}

function PrintVolumeOptions() {
	echo "<select id=\"volume\">";
	for ($x = 0; $x <= 100; $x += 5) {
		echo "<option value=\"" . $x . "\">" . $x . "</option>";
	}

	for ($x = 1; $x <= 4; $x += 1) {
		echo "<option value=\"%2B" . $x . "\">+" . $x . "</option>";
	}
	for ($x = 5; $x <= 50; $x += 5) {
		echo "<option value=\"%2B" . $x . "\">+" . $x . "</option>";
	}
	for ($x = 1; $x <= 4; $x += 1) {
		echo "<option value=\"-" . $x . "\">-" . $x . "</option>";
	}
	for ($x = 5; $x <= 50; $x += 5) {
		echo "<option value=\"-" . $x . "\">-" . $x . "</option>";
	}
	echo "</select>";
}

function PrintEventOptions()
{
	global $eventDirectory;
	echo "<select id=\"selEvent\" size=\"1\">";
	foreach(scandir($eventDirectory) as $eventFile)
	{
		if(preg_match('/\.fevt$/', $eventFile))
		{
			$f = fopen($eventDirectory . "/" . $eventFile, "r");
			if ($f == FALSE)
				die();

			$eventName = "";
			while (!feof($f))
			{
				$line = fgets($f);
				$entry = explode("=", $line, 2);
				if ($entry[0] == "name")
					$eventName = $entry[1];
			}
			fclose($f);

			$eventFile = preg_replace("/.fevt$/", "", $eventFile);
			$eventText = preg_replace("/_/", " / ", $eventFile);

			if ($eventName != "")
				$eventText .= " - " . $eventName;

			echo "<option value=\"" . $eventFile . "\">" . $eventText . "</option>";
		}
	}
	echo "</select>";
}

?>
      <fieldset style="padding: 10px; border: 2px solid #000;">
    <legend>Playlist Details</legend>
    <div style="border-bottom:solid 1px #000; padding-bottom:10px;">
      <div style="float:left">
            <input type="text" id="txtPlaylistName" size='40' class="pl_title" />
            <br />
            <span style="font-size:10px; padding-top:10px; font-family:Arial">(To rename edit name and click 'Save') </span> </div>
      <div style="float:left">
            <input name="" type="button" value="Save" onclick="<? if (isset($saveCallback)) echo $saveCallback; else echo "SavePlaylist('', '');"; ?>" class="buttons" style="margin-left:50px"/>
<?
if ($allowDelete)
{
?>
            <input name="" type="button" value="Delete" onclick="DeletePlaylist();"  class="buttons" />
<?
}
?>
            <br/>
            <input name="copyPlaylist" type="button" value="Copy Playlist" onclick="CopyPlaylist();"  class="buttons" style="margin-left:50px"/>
            <input name="randomizePlaylist" type="button" value="Randomize" onclick="RandomizePlaylistEntries();"  class="buttons" />
      </div>
        <div style="float:left; margin-left: 50px;" class="playlistInfoText">
            <span>Playlist Info:</span><br/>
            <span>Total Duration: <span id="playlistDuration">00m:00s</span></span><br/>
            <span>Total Items: <span id="playlistItems">0</span> (In Main)</span>
        </div>
          <div class="clear"></div>
    </div>
		    <br />
    <div id="playlistEntryProperties">
          <table border='0'>
				<tr><td colspan='2'><b>New Playlist Entry</b></td></tr>
        <tr><td>Type:</td>
            <td><select id="selType" size="1" onchange="PlaylistTypeChanged();">
            <option value = 'both'>Media and Sequence</option>
            <option value = 'media'>Media Only</option>
            <option value = 'sequence'>Sequence Only</option>
<? if (!$simplifiedPlaylist) echo "<option value = 'branch'>Branch</option>"; ?>
            <option value = 'remap'>Channel Remap</option>
<? if (!$simplifiedPlaylist) echo "<option value = 'dynamic'>Dynamic</option>"; ?>
            <option value = 'event'>Event</option>
            <option value = 'mqtt'>MQTT</option>
            <option value = 'pause'>Pause</option>
<? if (!$simplifiedPlaylist) echo "<option value = 'playlist'>Playlist</option>"; ?>
<? if (!$simplifiedPlaylist) echo "<option value = 'plugin'>Plugin</option>"; ?>
            <option value = 'volume'>Volume</option>
            <option value = 'script'>Script</option>
            <option value = 'url'>URL</option>
          </select>
          <span id='autoSelectWrapper' class='playlistOptions'><input type='checkbox' id='autoSelectMatches' checked> Auto-Select Matching Media/Sequence</span>
				</td></tr>
        <tr id="musicOptions" class='playlistOptions'><td>Media:</td>
            <td><?php PrintMediaOptions();?></td></tr>
        <tr id="sequenceOptions" class='playlistOptions'><td>Sequence:</td>
            <td><?php PrintSequenceOptions();?></td></tr>
        <tr id="scriptOptions" style="display:none;" class='playlistOptions'><td>Script:</td>
            <td><?php PrintScriptOptions('selScript');?></td></tr>
        <tr id="eventOptions" style="display:none;" class='playlistOptions'><td>Event:</td>
            <td><?php PrintEventOptions();?></td></tr>
        <tr id="pauseTime" style="display:none;" class='playlistOptions'><td><div><div id='pauseText' class='playlistOptions'>Pause Time:</div><div id='delayText' class='playlistOptions'>Delayed By:</div></div></td>
            <td><input id="txtPause" name="txtPause" type="text" size="10" maxlength="10"/>
              (Seconds)</td></tr>
        <tr id="volumeOptions" style="display:none;" class='playlistOptions'><td>Volume:</td>
            <td><?php PrintVolumeOptions();?></td></tr>
		<tr id="mqttOptions" class='playlistOptions'><td>MQTT:</td>
			<td><table border=0 cellpadding=0 cellspacing=2>
				<tr><td>Topic:</td><td><input id='mqttTopic' type='text' size='60' maxlength='60'></td></tr>
				<tr><td>Message:</td><td><input id='mqttMessage' type='text' size='60' maxlength='255'></td></tr>
				</table>
			</td>
			</tr>
		<tr id="remapOptions" class='playlistOptions'><td valign='top'>Remap:</td>
			<td><table border=0 cellpadding=0 cellspacing=2>
				<tr><td>Action:</td><td><select id='remapAction'><option value='add'>Add</option><option value='remove'>Remove</option></select></td></tr>
				<tr><td>Source Channel:</td><td><input id='srcChannel' type='text' size='6' maxlength='6'></td></tr>
				<tr><td>Destination Channel:</td><td><input id='dstChannel' type='text' size='6' maxlength='6'></td></tr>
				<tr><td>Channel Count:</td><td><input id='channelCount' type='text' size='6' maxlength='6'></td></tr>
				<tr><td>Loops:</td><td><input id='remapLoops' type='text' size='6' maxlength='6'></td></tr>
				<tr><td>Reverse:</td><td><select id='remapReverse'><option value='0'>None</option><option value='1'>By Channel</option><option value='2'>RGB Pixels</option></select></td></tr>
				</table>
			</td>
			</tr>
		<tr id="urlOptions" class='playlistOptions'><td>URL:</td>
			<td><table border=0 cellpadding=0 cellspacing=2>
				<tr><td>URL:</td><td><input id='url' type='text' size='60' maxlength='255' placeholder='http://hostname/path'></td></tr>
				<tr><td>Method:</td><td><select id='urlMethod'><option value='GET'>GET</option><option value='POST'>POST</option></select></td></tr>
				<tr><td>Post Data:</td><td><input id='urlData' type='text' size='60' maxlength='1024' placeholder='POST Data'></td></tr>
				</table>
			</td>
			</tr>
		<tr id="branchOptions" class='playlistOptions'><td valign='top'>Branch:</td>
			<td><table border=0 cellpadding=0 cellspacing=2>
				<tr><td>True:</td><td>Section: <select id='branchTrueSection'>
						<option value=''>** Same **</option>
						<option value='leadIn'>Lead In</option>
						<option value='mainPlaylist'>Main Playlist</option>
						<option value='leadOut'>Lead Out</option>
					</select>
					<input id='branchTrueItem' type='text' size='3' maxlength='3' value='1'></td></tr>
				<tr><td>False:</td><td>Section: <select id='branchFalseSection'>
						<option value=''>** Same **</option>
						<option value='leadIn'>Lead In</option>
						<option value='mainPlaylist'>Main Playlist</option>
						<option value='leadOut'>Lead Out</option>
					</select>
					<input id='branchFalseItem' type='text' size='3' maxlength='3' value='2'></td></tr>
				<tr><td>Type:</td><td><select id='branchType'><option>Time</option></select></td></tr>
				<tr><td></td>
					<td><table border=0 cellpadding=0 cellspacing=2>
						<tr><td>Start Time:</td><td><input class='time center'  name='branchStartTime' id='branchStartTime' type='text' size='8' value='00:00:00'/></td></tr>
						<tr><td>End Time:</td><td><input class='time center'  name='branchEndTime' id='branchEndTime' type='text' size='8' value='00:00:00'/></td></tr></table>
					</td></tr>
				</table>
			</td>
			</tr>
		<tr id="dynamicOptions" class='playlistOptions'><td>Dynamic:</td>
			<td><table border=0 cellpadding=0 cellspacing=2>
				<tr><td>Source Type:</td><td><select id='dynamicSubType' onChange='DynamicSubTypeChanged();'>
						<option value='file'>File</option>
						<option value='plugin'>Plugin</option>
						<!--
						<option value='command'>Script</option>
						-->
						<option value='url'>URL</option>
					</select></td></tr>
				<tr><td>Source:</td><td id='dynamicDataWrapper'><input id='dynamicData' type='text' size='60' maxlength='255' placeholder='Full File Name of JSON file'></td></tr>
				</table>
			</td>
			</tr>
        <tr id="pluginData" style="display:none;" class='playlistOptions'><td><div><div id='pluginDataText'>Plugin Data:</div></div></td>
            <td><input id="txtData" name="txtData" type="text" size="80" maxlength="255"/></td></tr>
        <tr id="subPlaylistOptions" style="display:none;" class='playlistOptions'><td><div><div id='playlistDataText'>Plugin Data:</div></div></td>
            <td><select id="selSubPlaylist" name="selSubPlaylist">
<?
	foreach ($playlistNames as $name)
		echo "<option value='$name'>$name</option>\n";
?>
				</select></td></tr>
        <tr><td colspan='2'>
            <input id='btnAddPlaylistEntry'  width="200px"  onclick="AddPlaylistEntry();" class="buttons" type="button" value="Add" />
            <input id='btnRemovePlaylistEntry'  width="200px"  onclick="RemovePlaylistEntry();" class="buttons" type="button" value="Remove" />
            </td></tr>
				</table>
          <div class="clear"></div>
        </div>
    <div id="createPlaylistItems">
          <table id="tblCreatePlaylist">
            <colgroup>
                <col class='colPlaylistNumber'>
                <col class='colPlaylistType'>
                <col class='colPlaylistData1'>
                <col class='colPlaylistData2'>
                <col class='colPlaylistData3'>
            </colgroup>
			<thead>
        <tr id="rowCreatePlaylistHeader">
              <th class="colPlaylistNumber">#</th>
              <th class="colPlaylistType">Type</th>
              <th class="colPlaylistData1">Media File / Script / Event / Pause</th>
              <th class="colPlaylistData2">Sequence / Delay / Data</th>
              <th class="colPlaylistData3"></th>
            </tr>
			</thead>
<?
if (!$simplifiedPlaylist)
{
?>
						<tbody>
							<tr><th colspan=5>-- Lead In --</th></tr>
						</tbody>
            <tbody id="tblPlaylistLeadIn" class='tblCreatePlaylistEntries_tbody'>
            </tbody>
<?
}
?>
						<tbody>
							<tr><th colspan=5>-- Main Playlist --</th></tr>
						</tbody>
            <tbody id="tblPlaylistMainPlaylist" class='tblCreatePlaylistEntries_tbody'>
            </tbody>
<?
if (!$simplifiedPlaylist)
{
?>
						<tbody>
							<tr><th colspan=5>-- Lead Out --</th></tr>
						</tbody>
            <tbody id="tblPlaylistLeadOut" class='tblCreatePlaylistEntries_tbody'>
            </tbody>
<?
}
?>
      </table>
        </div>
    <span style="font-size:12px; font-family:Arial; margin-left:15px;">(Drag entry to reposition) </span>
  </fieldset>
<div id="copyPlaylist_dialog" title="Copy Playlist" style="display: none">
    <form>
        <span>Enter new name for playlist</span>
        <input name="newPlaylistName" type="text" style="z-index:10000; width: 100%" class="newPlaylistName" value="New Playlist Name">
    </form>
</div>
