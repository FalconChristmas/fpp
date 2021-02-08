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
    if (window.innerWidth > 600) {
        $('.playlistEntriesBody').sortable({
            start: function (event, ui) {
                start_pos = ui.item.index();

                start_parent = $(ui.item).parent().attr('id');
                start_id = start_parent.replace(/tblPlaylist/i, "");
            },
            update: function(event, ui) {
                if (this === ui.item.parent()[0]) {
                    var parent = $(ui.item).parent().attr('id');
                    $('#' + parent + 'PlaceHolder').remove();

                    var rowsLeft = $('#' + start_parent + ' >tr').length;
                    if (rowsLeft == 0)
                        $('#' + start_parent).html("<tr id='" + start_parent + "PlaceHolder' class='unselectable'><td colspan=4>&nbsp;</td></tr>");

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
            scroll: true
        }).disableSelection();
    }

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
    value = value.replace(/\.flac|\.ogg|\.mp3|\.mp4|\.mov|\.m4a/i, "");

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
    <div class="playlistEditContainer">
        <div class="playlistEditForm">
            <div class="playlistEdit">
                <div class="form-group">
                    <label for="txtPlaylistName">Playlist Name:</label>
                    <input type="text" id="txtPlaylistName" class="pl_title form-control" disabled />
                </div>
                <div class="form-group">
                    <label for="txtPlaylistDesc">Playlist Description:</label>
                    <input type="text" id="txtPlaylistDesc" class="pl_description form-control" />
                </div>
                <div class="form-group flow">
                    <label for="randomizePlaylist">Randomize:</label>
                    <select id='randomizePlaylist' class="form-control">
                        <option value='0'>Off</option>
                        <option value='1'>Once per load</option>
                        <option value='2'>Every iteration</option>
                    </select>
                </div>
                <div class="form-actions">
                    <button value="Save" onclick="<? if (isset($saveCallback)) echo $saveCallback; else echo "SavePlaylist('', '');"; ?>" class="buttons btn-success playlistEditButton">Save</button>
                    <div class="dropdown">
                        <button class="buttons dropdown-toggle playlistEditButton" type="button" id="playlistEditMoreButton" data-toggle="dropdown" aria-haspopup="true" aria-expanded="false">
                            More
                        </button>
                        <div class="dropdown-menu playlistEditMoreButtonMenu" aria-labelledby="playlistEditMoreButton">
                            
                            <a href="#" value="Copy" onclick="CopyPlaylist();"  class="dropdown-item">Copy</a>
                            <a href="#" value="Rename" onclick="RenamePlaylist();"  class="dropdown-item ">Rename</a>
                            <a href="#" value="Randomize" onclick="RandomizePlaylistEntries();"  class="dropdown-item ">Randomize</a>
                            <a href="#" value="Reset" onclick="EditPlaylist();"  class="dropdown-item ">Reset</a>
                            <a href="#" value="Delete" onclick="DeletePlaylist();"  class="dropdown-item ">Delete</a>
                        </div>
                    </div>
                </div>


            
            
            </div>

            <!-- <div style="float:left;" class="playlistInfoText">
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
            <div class="clear"></div> -->
        </div>


    <div id="playlistEntryProperties" class="backdrop-dark">
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
        <div class="form-actions">
            <button onclick="AddPlaylistEntry(0);" class="buttons playlistEditButton" value="Add">Add</button>
            <button onclick="AddPlaylistEntry(1);" class="buttons playlistDetailsEditButton" value="Replace">Replace</button>
            <button onclick="RemovePlaylistEntry();" class="buttons playlistDetailsEditButton" value="Remove">Remove</button>
            <button class="buttons dropdown-toggle playlistDetailsEditButton" type="button" id="playlistDetailsEditMoreButton" data-toggle="dropdown" aria-haspopup="true" aria-expanded="false">
                More
            </button>
            <div class="dropdown-menu playlistDetailsEditMoreButtonMenu" aria-labelledby="playlistDetailsEditMoreButton">
                <a href="#" onclick="AddPlaylistEntry(2);" class="dropdown-item" value="Insert Before">Insert Before</a>
                <a href="#" onclick="AddPlaylistEntry(3);" class="dropdown-item" value="Insert After">Insert After</a>
                <a href="#" onclick="EditPlaylistEntry();" class="dropdown-item" value="Edit">Edit</a>

            </div>
            
        </div>
        <div>
             <? PrintSetting('verbosePlaylistItemDetails', 'VerbosePlaylistItemDetailsToggled'); ?>
            </div>
    </div>
</div>

<?
include "playlistDetails.php";
?>



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
