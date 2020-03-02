<?php
include "playlistEntryTypes.php";

if (!isset($simplifiedPlaylist))
	$simplifiedPlaylist = 0;

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
	$('.playlistEntriesBody').sortable({
		start: function (event, ui) {
			start_pos = ui.item.index();

			start_parent = $(ui.item).parent().attr('id');
			start_id = start_parent.replace(/tblPlaylist/i, "");
		},
		update: function(event, ui) {
			if (this === ui.item.parent()[0]) {
                var parent = $(ui.item).parent().attr('id');
                var rowsInNew = $('#' + start_parent + ' >tr').length;
//                if (rowsInNew == 1)
                    $('#' + parent + 'PlaceHolder').remove();

                var rowsLeft = $('#' + start_parent + ' >tr').length;
                if (rowsLeft == 0)
                    $('#' + start_parent).html("<tr id='" + start_parent + "PlaceHolder' class='unselectable'><td colspan=4>&nbsp;</td></tr>");
console.log('parent: ' + parent + ', start_parent: ' + start_parent + ', rowsInNew: ' + rowsInNew + ', rowsLeft: ' + rowsLeft);

                RenumberPlaylistEditorEntries();
                UpdatePlaylistDurations();
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
		item: '> tr',
		connectWith: '.playlistEntriesBody',
		scroll: true,
		stop: function (event, ui) {
			//SAVE YOUR SORT ORDER                    
			}
		}).disableSelection();

	$('.playlistEntriesBody').on('mousedown', 'tr', function(event,ui){
		$('#tblPlaylistDetails tbody tr').removeClass('playlistSelectedEntry');
		$(this).addClass('playlistSelectedEntry');
        EnableButtonClass('playlistDetailsEditButton');
	});

	$('.playlistEntriesBody').on('dblclick','tr',function() {
        EditPlaylistEntry();
    });

	$('#txtNewPlaylistName').on('focus',function() {
		$(this).select();
	});

	$('.time').timepicker({'timeFormat': 'H:i:s', 'typeaheadHighlight': false});

	PlaylistTypeChanged();
    DisableButtonClass('playlistEditButton');
    DisableButtonClass('playlistDetailsEditButton');

//    $('#playlistDetailsContents').resizable({
//        "handles": "s"
//    });
}

function MediaChanged()
{
	if ($('#autoSelectMatches').is(':checked') == false)
		return;

	var value = $('.arg_mediaName').val();
    if (value == null)  {
        return;
    }
    value = value.replace(/\.ogg|\.mp3|\.mp4|\.mov|\.m4a/i, "");

	var seq = document.getElementsByClassName("arg_sequenceName")[0];
    if (seq) {
        for (var i = 0; i < seq.length; i++) {
            if (seq.options[i].value.replace(/\.fseq/i, "") == value)
                $('.arg_sequenceName').val(seq.options[i].value);
        }
    }
}

function SequenceChanged()
{
	if ($('#autoSelectMatches').is(':checked') == false)
		return;

    var val = $('.arg_sequenceName').val();
    if (val == null)  {
        return;
    }
    
    var value = val.replace(/\.fseq/i, "");
    var media = document.getElementsByClassName("arg_mediaName")[0];
    if (media) {
        for (var i = 0; i < media.length; i++) {
            if (media.options[i].value.replace(/\.ogg|\.mp3|\.mp4|\.mov|\.m4a/i, "") == value)
                $('.arg_mediaName').val(media.options[i].value);
        }
    }
    
    $.ajax({
           dataType: "json",
           url: "api/sequence/" + val + "/meta",
           async: false,
           success: function(data) {
                var media = document.getElementsByClassName("arg_mediaName")[0];
                if ((typeof data["variableHeaders"] === 'object') &&
                    (typeof data["variableHeaders"]["mf"] === 'string')) {
                    var mfHeader = data["variableHeaders"]["mf"];
                    var idx = mfHeader.lastIndexOf("/");
                    if (idx >= 0) {
                        mfHeader = mfHeader.substring(idx + 1);
                    }
                    idx = mfHeader.lastIndexOf("\\");
                    if (idx >= 0) {
                        mfHeader = mfHeader.substring(idx + 1);
                    }

                    if (media) {
                        for (var i = 0; i < media.length; i++) {
                            if (media.options[i].value == mfHeader) {
                                 $('.arg_mediaName').val(media.options[i].value);
                            }
                        }
                    }
                }
           }
        });
}

function BranchTypeChanged() {
    var type = $('.arg_trueNextBranchType').val();
    if (type == 'Offset') {
        $('.arg_trueNextItem').attr({"min": -99, "max": 99});
    } else if (type == 'Index') {
        $('.arg_trueNextItem').attr({"min": 1, "max": 99});
        if ($('.arg_trueNextItem').val() < 1)
            $('.arg_trueNextItem').val(1);
    }

    type = $('.arg_falseNextBranchType').val();
    if (type == 'Index') {
        $('.arg_falseNextItem').attr({"min": -99, "max": 99});
    } else if (type == 'Offset') {
        $('.arg_falseNextItem').attr({"min": 1, "max": 99});
        if ($('.arg_falseNextItem').val() < 1)
            $('.arg_falseNextItem').val(1);
    }
}

$(document).ready(function() {
    allowMultisyncCommands = true;
    SetupToolTips();
	playlistEditorDocReady();
    LoadCommandList($('#commandSelect'));
    CommandSelectChanged('commandSelect', 'tblCommandBody');
});

simplifiedPlaylist = <? echo $simplifiedPlaylist; ?>;
</script>
    <fieldset style="padding: 10px; border: 2px solid #000;">
        <legend>Playlist Details</legend>
        <div style="border-bottom:solid 1px #000; padding-bottom:10px;">
            <div style="float:left; margin-right: 50px;">
                <b>Playlist Name:</b><br>
                <input type="text" id="txtPlaylistName" class="pl_title" disabled />
                <br />
                <input type="button" value="Save" onclick="<? if (isset($saveCallback)) echo $saveCallback; else echo "SavePlaylist('', '');"; ?>" class="buttons playlistEditButton" />
                <input type="button" value="Delete" onclick="DeletePlaylist();"  class="buttons playlistEditButton playlistExistingButton" />
                <input type="button" value="Copy" onclick="CopyPlaylist();"  class="buttons playlistEditButton playlistExistingButton" />
                <input type="button" value="Rename" onclick="RenamePlaylist();"  class="buttons playlistEditButton playlistExistingButton" />
                <input type="button" value="Randomize" onclick="RandomizePlaylistEntries();"  class="buttons playlistEditButton" />
                <input type="button" value="Reset" onclick="EditPlaylist();"  class="buttons playlistEditButton playlistExistingButton" />
            </div>
            <div style="float:left;" class="playlistInfoText">
                <table>
                    <tr><th></th>
                        <th>Items</th>
                        <th>Duration</th>
                    </tr>
                    <tr><th align='left'>Lead In:</th>
                        <td class='playlistItemCountLeadIn'>0</td>
                        <td class='playlistDurationLeadIn'>00m:00s</td>
                    </tr>
                    <tr><th align='left'>Main Playlist:</th>
                        <td class='playlistItemCountMainPlaylist'>0</td>
                        <td class='playlistDurationMainPlaylist'>00m:00s</td>
                    </tr>
                    <tr><th align='left'>Lead Out:</th>
                        <td class='playlistItemCountLeadOut'>0</td>
                        <td class='playlistDurationLeadOut'>00m:00s</td>
                    </tr>
                </table>
                <span style='display: none;' id='playlistDuration'>0</span>
            </div>
            <div class="clear"></div>
        </div>
        <br />

    <div id="playlistEntryProperties" style='float: left'>
        <table border='0'>
            <colgroup>
                <col class='colPlaylistEditorLabel'></col>
                <col class='colPlaylistEditorData'></col>
            </colgroup>
            <tbody>
                <tr><td colspan='2'><b>Edit Playlist Entry</b></td></tr>
                <tr><td>Type:</td>
                    <td><select id="pe_type" size="1" onchange="PlaylistTypeChanged();">
<?
foreach ($playlistEntryTypes as $pet) {
    if ((!isset($pet['deprecated'])) || ($pet['deprecated'] == 0)) {
        echo "<option value='" . $pet['name'] . "'";
        if ($pet['name'] == 'both')
            echo " selected";
        echo ">" . (isset($pet['longDescription']) ? $pet['longDescription'] : $pet['description']) . "</option>\n";
    }
}
?>
                        </select>
                        <span id='autoSelectWrapper' class='playlistOptions'><input type='checkbox' id='autoSelectMatches' checked> Auto-Select Matching Media/Sequence</span>
                    </td>
                </tr>
            </tbody>
            <tbody id='playlistEntryOptions'>
            </tbody>
            <tbody id='playlistEntryCommandOptions'>
            </tbody>
        </table>
    </div>
    <div class="clear"></div>
    <div>
        <input width="200px"  onclick="AddPlaylistEntry(0);" class="buttons playlistEditButton" type="button" value="Add" />
        <input width="200px"  onclick="AddPlaylistEntry(2);" class="buttons playlistDetailsEditButton" type="button" value="Insert Before" />
        <input width="200px"  onclick="AddPlaylistEntry(3);" class="buttons playlistDetailsEditButton" type="button" value="Insert After" />
        <input width="200px"  onclick="EditPlaylistEntry();" class="buttons playlistDetailsEditButton" type="button" value="Edit" />
        <input width="200px"  onclick="AddPlaylistEntry(1);" class="buttons playlistDetailsEditButton" type="button" value="Replace" />
        <input width="200px"  onclick="RemovePlaylistEntry();" class="buttons playlistDetailsEditButton" type="button" value="Remove" />
        <br>
<? PrintSetting('verbosePlaylistItemDetails', 'VerbosePlaylistItemDetailsToggled'); ?>
    </div>
<?
include "playlistDetails.php";
?>
    <span style="font-size:12px; font-family:Arial; margin-left:15px;">(Drag entry to reposition) </span>
  </fieldset>

<div id="copyPlaylist_dialog" title="Copy Playlist" style="display: none">
    <span>Enter name for new playlist:</span>
    <input name="newPlaylistName" type="text" style="z-index:10000; width: 95%" class="newPlaylistName" value="New Playlist Name">
</div>

<div id="renamePlaylist_dialog" title="Rename Playlist" style="display: none">
    <span>Enter new name for playlist:</span>
    <input name="newPlaylistName" type="text" style="z-index:10000; width: 95%" class="newPlaylistName" value="New Playlist Name">
</div>

<span id='randomizeBuffer' style='display: none;'>
</span>
