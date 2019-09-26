
STATUS_IDLE = "0";
STATUS_PLAYING = "1";
STATUS_STOPPING_GRACEFULLY = "2";


// Globals
gblCurrentPlaylistIndex = 0;
gblCurrentPlaylistEntryType = '';
gblCurrentPlaylistEntrySeq = '';
gblCurrentPlaylistEntrySong = '';
gblCurrentLoadedPlaylist  = '';
gblCurrentLoadedPlaylistCount = 0;

lastPlaylistEntry = '';
lastPlaylistSection = '';

var max_retries = 60;
var retry_poll_interval_arr = [];

var minimalUI = 0;

var statusTimeout = null;
var lastStatus = '';


function versionToNumber(version)
{
    // convert a version string like 2.7.1-2-dirty to "20701"
    if (version.charAt(0) == 'v') {
        version = version.substr(1);
    }
    if (version.indexOf("-") != -1) {
        version = version.substr(0, version.indexOf("-"));
    }
    var parts = version.split('.');
    
    while (parts.length < 3) {
        parts.push("0");
    }
    var number = 0;
    for (var x = 0; x < 3; x++) {
        var val = parseInt(parts[x]);
        if (val >= 9990) {
            return number * 10000 + 9999;
        } else if (val > 99) {
            val = 99;
        }
        number = number * 100 + val;
    }
    return number;
}

// Compare two version numbers
function CompareFPPVersions(a, b) {
	// Turn any non-string version numbers into a string
	a = "" + a;
	b = "" + b;
    a = versionToNumber(a);
    b = versionToNumber(b);

	if (a > b) {
		return 1;
	} else if (a < b) {
		return -1;
	}

	return 0;
}

function ShowPlaylistDetails() {
	$('#statusPlaylistDetailsWrapper').show();
	$('#btnShowPlaylistDetails').hide();
	$('#btnHidePlaylistDetails').show();
}

function HidePlaylistDetails() {
	$('#statusPlaylistDetailsWrapper').hide();
	$('#btnShowPlaylistDetails').show();
	$('#btnHidePlaylistDetails').hide();
}

function PopulateLists() {
	PlaylistTypeChanged();
	PopulatePlaylists("playList", '', '');
	var firstPlaylist = $('#playlist0').html();
	if (firstPlaylist != undefined)
		PopulatePlayListEntries(firstPlaylist,true);
}

var currentPlaylists = [];
function PopulatePlaylists(element, filter, callback) {
	var xmlhttp=new XMLHttpRequest();
	var url = "fppxml.php?command=getPlayLists";

	if (filter != '')
		url += '&filter=' + filter;
	
	xmlhttp.open("GET",url,false);
	xmlhttp.setRequestHeader('Content-Type', 'text/xml');
	var Filename;
 
	xmlhttp.onreadystatechange = function () {
		if (xmlhttp.readyState == 4 && xmlhttp.status==200) {
			var xmlDoc=xmlhttp.responseXML; 
	
			var productList = xmlDoc.getElementsByTagName('Playlists')[0];
			var innerHTML = "";
			if(productList.childNodes.length> 0) {
				var colCount = 1;
				if (productList.childNodes.length > 30) {
					colCount = 4;
                } else if (productList.childNodes.length > 20) {
					colCount = 3;
                } else if (productList.childNodes.length > 10) {
                    colCount = 2;
                }
                
                currentPlaylists = [];

                innerHTML += "<ol style='list-style-position: inside; -webkit-column-count:" + colCount + "; -webkit-column-gap:20px; -moz-column-count:"
                    + colCount + "; -moz-column-gap:20px; column-count:" + colCount + "; column-gap:20px;'>";

				for (i=0; i < productList.childNodes.length; i++) {
                    Filename = productList.childNodes[i].textContent;
                    // Remove extension
                    //Filename = Filename.substr(0, x.lastIndexOf('.'));
                    currentPlaylists[i] = Filename;
                    innerHTML += "<li><a href='#editor' id=playlist" + i.toString() + " onclick=\"PopulatePlayListEntries('" + Filename + "',true);" + callback + "\">" + Filename + "</a></li>";
                }
                innerHTML += "</ol>";
			} else {
				innerHTML = "No Playlists Created";
			}
			var results = document.getElementById(element);
			results.innerHTML = innerHTML;	
		}
	};
	xmlhttp.send();
}

function GetPlaylistRowHTML(ID, type, data1, data2, data3, firstlast, editMode)
{
	var HTML = "";

	HTML += "<tr id=\"playlistRow" + ID + "\">";

	if (minimalUI)
	{
		HTML += "<td class=\"colPlaylistNumber\" id = \"colEntryNumber" + ID + "\" >" + ID + ".</td>";
		HTML += "<td class=\"colPlaylistData1\">" + data1;
		if (data2 && (data2 != '---'))
			HTML += "<br>" + data2;

		if (data3 && (data3 != '---'))
			HTML += "<br>" + data3;

		HTML += "</td>";
	}
	else
	{
		HTML += "<td class=\"colPlaylistNumber "

		if (editMode)
			HTML += "colPlaylistNumberDrag";

		HTML += "\" id = \"colEntryNumber" + ID + "\" >" + ID + ".</td>";

		if (editMode)
			HTML += "<td class=\"colPlaylistType\">" + type + "</td>";

		HTML += "<td class=\"colPlaylistData1\">" + data1 + "</td>";
        if (data2) {
            HTML += "<td class=\"colPlaylistData2\">" + data2 + "</td>"
        } else {
            HTML += "<td class=\"colPlaylistData2\"></td>"
        }
		if (data3) {
	        HTML += "<td class=\"colPlaylistData3\">" + data3 + "</td>"
		} else {
			HTML += "<td class=\"colPlaylistData3\"></td>"
		}
	}
	HTML += "</tr>";

	return HTML;
}

function PlaylistEntryToTR(i, entry, editMode)
{
	var HTML = "";

	if(entry.type == 'both')
		HTML += GetPlaylistRowHTML((i+1).toString(), "Seq/Med", entry.mediaName, entry.sequenceName, entry.videoOut, i.toString(), editMode);
	else if(entry.type == 'media')
		HTML += GetPlaylistRowHTML((i+1).toString(), "Media", entry.mediaName, "---", entry.videoOut, i.toString(), editMode);
	else if(entry.type == 'sequence')
		HTML += GetPlaylistRowHTML((i+1).toString(), "Sequence", "---", entry.sequenceName, "", i.toString(), editMode);
	else if(entry.type == 'pause')
		HTML += GetPlaylistRowHTML((i+1).toString(), "Pause", "PAUSE - " + entry.duration.toString(), "---", "", i.toString(), editMode);
	else if(entry.type == 'playlist')
		HTML += GetPlaylistRowHTML((i+1).toString(), "Playlist", "PLAYLIST - " + entry.name, "---", "", i.toString(), editMode);
	else if(entry.type == 'branch')
	{
		var branchStr = "Invalid Config";
		if (entry.trueNextItem < 999)
		{
			branchStr = "";
			if (entry.compInfo.startHour < 0)
				branchStr += "**:";
			else
				branchStr += ((entry.compInfo.startHour < 10) ? ("0" + entry.compInfo.startHour) : entry.compInfo.startHour) + ":";

			branchStr += ""
				+ ((entry.compInfo.startMinute < 10) ? ("0" + entry.compInfo.startMinute) : entry.compInfo.startMinute) + ":"
				+ ((entry.compInfo.startSecond < 10) ? ("0" + entry.compInfo.startSecond) : entry.compInfo.startSecond) + " < X < ";

			if (entry.compInfo.endHour < 0)
				branchStr += "**:";
			else
				branchStr += ((entry.compInfo.endHour < 10) ? ("0" + entry.compInfo.endHour) : entry.compInfo.endHour) + ":";

			branchStr += ""
				+ ((entry.compInfo.endMinute < 10) ? ("0" + entry.compInfo.endMinute) : entry.compInfo.endMinute) + ":"
				+ ((entry.compInfo.endSecond < 10) ? ("0" + entry.compInfo.endSecond) : entry.compInfo.endSecond);

			branchStr += ", True: ";
			if (entry.trueNextSection != "")
				branchStr += entry.trueNextSection + "/" + (entry.trueNextItem + 1);
			else
				branchStr += "+" + entry.trueNextItem;

			if (entry.falseNextItem < 999)
			{
				branchStr += ", False: ";
				if (entry.falseNextSection != "")
					branchStr += entry.falseNextSection + "/" + (entry.falseNextItem + 1);
				else
					branchStr += "+" + entry.falseNextItem;
			}

		}
		HTML += GetPlaylistRowHTML((i+1).toString(), "Branch", "BRANCH - " + branchStr, "---", "", i.toString(), editMode);
	}
	else if(entry.type == 'mqtt')
		HTML += GetPlaylistRowHTML((i+1).toString(), "MQTT", "MQTT - " + entry.topic, entry.message, "", i.toString(), editMode);
	else if(entry.type == 'dynamic')
	{
		if (entry.hasOwnProperty('dynamic'))
		{
			entry.dynamic.mediaName = "DYNAMIC >> " + entry.dynamic.mediaName;
			HTML += PlaylistEntryToTR(i, entry.dynamic, editMode);
		}
		else
			HTML += GetPlaylistRowHTML((i+1).toString(), "Dynamic", "DYNAMIC - " + entry.subType, entry.data, "", i.toString(), editMode);
	}
    else if(entry.type == 'volume')
        HTML += GetPlaylistRowHTML((i+1).toString(), "Volume", "VOLUME - " + entry.volume.toString(), "", "", i.toString(), editMode);
    else if(entry.type == 'url')
		HTML += GetPlaylistRowHTML((i+1).toString(), "URL", "URL - " + entry.method + ' - ' + entry.url, "", entry.data, i.toString(), editMode);
    else if(entry.type == 'image')
		HTML += GetPlaylistRowHTML((i+1).toString(), "Image", "Image - " + entry.imagePath, entry.imageFilename, entry.data, i.toString(), editMode);
	else if(entry.type == 'remap')
	{
		var desc = "Add ";
		if (entry.action == "remove")
			desc = "Remove ";

		desc += "remap for " + entry.count + " channels from " + entry.source + " to " + entry.destination + " " + entry.loops + " times";
		HTML += GetPlaylistRowHTML((i+1).toString(), "Remap", desc, "", "", i.toString(), editMode);
	}
	else if(entry.type == 'event')
	{
		majorID = parseInt(entry.majorID);
		if (entry.majorID < 10)
			majorID = "0" + majorID;

		minorID = parseInt(entry.minorID);
		if (entry.minorID < 10)
			minorID = "0" + minorID;

		id = majorID + '_' + minorID;

		HTML += GetPlaylistRowHTML((i+1).toString(), "Event", id + " - " + entry.desc, "---", "", i.toString(), editMode);
	}
	else if(entry.type == 'plugin')
		HTML += GetPlaylistRowHTML((i+1).toString(), "Plugin", "---", "", entry.data, editMode);
    else if(entry.type == 'script') {
        var blocking = "No Wait";
        if (entry.blocking) {
            blocking = "Wait";
        }
		HTML += GetPlaylistRowHTML((i+1).toString(), "Script", entry.scriptName, entry.scriptArgs, blocking, i.toString(), editMode);
    }

	return HTML;
}

function PopulatePlayListEntries(playList,reloadFile,selectedRow) {
	if ( ! playList ) {
		$('#txtPlaylistName').val(playList);

		var innerHTML="";
		innerHTML +=  "<tr class=\"playlistPlayingEntry\">";
		innerHTML +=  "<td>No playlist loaded.</td>";
		innerHTML += "</tr>";
		$('.tblCreatePlaylistEntries_tbody').html(innerHTML);

		return false;
	}

	lastPlaylistEntry = '';
	lastPlaylistSection = '';

	$.ajax({
		url: 'fppjson.php?command=getPlayListEntries&pl=' + playList + '&reload=' + reloadFile + '&mergeSubs=0',
		dataType: 'json',
		success: function(data, reqStatus, xhr) {	
			var innerHTML = "";

			if(data && typeof data === 'object') {
				var entries = 0;
				if (data.hasOwnProperty('leadIn') && data.leadIn.length > 0)
				{
					innerHTML = "";
					for (i = 0; i < data.leadIn.length; i++)
					{
						innerHTML += PlaylistEntryToTR(entries, data.leadIn[i], 1);
						entries++;
					}
					$('#tblPlaylistLeadIn').html(innerHTML);
				}
				else
					$('#tblPlaylistLeadIn').html("<tr id='tblPlaylistLeadInPlaceHolder'><td>&nbsp;</td></tr>");
					

				if (data.hasOwnProperty('mainPlaylist') && data.mainPlaylist.length > 0)
				{
					innerHTML = "";
					for (i = 0; i < data.mainPlaylist.length; i++)
					{
						innerHTML += PlaylistEntryToTR(entries, data.mainPlaylist[i], 1);
						entries++;
					}
					$('#tblPlaylistMainPlaylist').html(innerHTML);
				}
				else
					$('#tblPlaylistMainPlaylist').html("<tr id='tblPlaylistMainPlaylistPlaceHolder'><td>&nbsp;</td></tr>");

				if (data.hasOwnProperty('leadOut') && data.leadOut.length > 0)
				{
					innerHTML = "";
					for (i = 0; i < data.leadOut.length; i++)
					{
						innerHTML += PlaylistEntryToTR(entries, data.leadOut[i], 1);
						entries++;
					}
					$('#tblPlaylistLeadOut').html(innerHTML);
				}
				else
					$('#tblPlaylistLeadOut').html("<tr id='tblPlaylistLeadOutPlaceHolder'><td>&nbsp;</td></tr>");

                if (data.hasOwnProperty('playlistInfo')) {
                    $('#playlistDuration').text(data.playlistInfo.total_duration);
                    $('#playlistItems').text(data.playlistInfo.total_items);
                } else {
                    $('#playlistDuration').text("00m:00s");
                    $('#playlistItems').text("0");
                }

				if (entries == 0)
				{
					innerHTML  =  "<tr class=\"playlistPlayingEntry\">";
					innerHTML +=  "<td colspan='4'>No entries in playlist section.</td>";
					innerHTML += "</tr>";
					$('.tblCreatePlaylistEntries_tbody').html(innerHTML);
				}

				gblCurrentLoadedPlaylist = playList;
				gblCurrentLoadedPlaylistCount = entries;
				$('#txtPlaylistName').val(playList);
			}
			else
			{
				innerHTML  =  "<tr class=\"playlistPlayingEntry\">";
				innerHTML +=  "<td>No entries in playlist section.</td>";
				innerHTML += "</tr>";
				$('.tblCreatePlaylistEntries_tbody').html(innerHTML);

				gblCurrentLoadedPlaylist = playList;
				gblCurrentLoadedPlaylistCount = 0;
				$('#txtPlaylistName').val(playList);
			}
		}
	});
}


function PlaylistTypeChanged() {
	var type = $('#selType').val();

	$('.playlistOptions').hide();

	if (type == 'both')
	{
		$("#musicOptions").show();
		$("#sequenceOptions").show();
		$("#autoSelectWrapper").show();
		$("#autoSelectMatches").prop('checked', true);
	}
	else if (type == 'media')
	{
		$("#musicOptions").show();
	}
	else if (type == 'sequence')
	{
		$("#sequenceOptions").show();
	}
	else if (type == 'pause')
	{
		$("#pauseTime").show();
		$("#pauseText").show();
	}
	else if (type == 'script')
	{
		$("#scriptOptions").show();
	}
	else if (type == 'event')
	{
		$("#eventOptions").show();
	}
	else if (type == 'plugin')
	{
		$("#pluginData").show();
	}
	else if (type == 'branch')
	{
		$('#branchOptions').show();
	}
	else if (type == 'mqtt')
	{
		$('#mqttOptions').show();
	}
	else if (type == 'remap')
	{
		$('#remapOptions').show();
	}
	else if (type == 'dynamic')
	{
		$('#dynamicOptions').show();
	}
	else if (type == 'playlist')
	{
		$('#subPlaylistOptions').show();
	}
    else if (type == 'volume')
    {
        $('#volumeOptions').show();
    }
	else if (type == 'url')
	{
		$('#urlOptions').show();
	}
	else if (type == 'image')
	{
		$('#imageOptions').show();
	}
}

function AddNewPlaylist() {
	var name=document.getElementById("txtNewPlaylistName");
	var plName = name.value.replace(/ /,'_');

	var xmlhttp=new XMLHttpRequest();
	var url = "fppxml.php?command=addPlayList&pl=" + plName;
	xmlhttp.open("GET",url,false);
	xmlhttp.setRequestHeader('Content-Type', 'text/xml');

	xmlhttp.onreadystatechange = function () {
		if (xmlhttp.readyState == 4 && xmlhttp.status==200) 
		{
			var xmlDoc=xmlhttp.responseXML; 
			var productList = xmlDoc.getElementsByTagName('Music')[0];
			PopulatePlaylists('playList', '', '');
			PopulatePlayListEntries(plName,true);
			$('#txtName').val(plName);
			$('#txtName').focus()
			$('#txtName').select()
			
		}
	};
	
	xmlhttp.send();

}
		
function PlaylistEntryPositionChanged(newSection,newIndex,oldSection,oldIndex) {
	var xmlhttp=new XMLHttpRequest();
	var url = "fppxml.php?command=playlistEntryPositionChanged&newSection=" + newSection + "&newIndex=" + newIndex + "&oldSection=" + oldSection + "&oldIndex=" + oldIndex;
	xmlhttp.open("GET",url,false);
	xmlhttp.setRequestHeader('Content-Type', 'text/xml');

	xmlhttp.onreadystatechange = function () {
		if (xmlhttp.readyState == 4 && xmlhttp.status==200) 
		{
//					var xmlDoc=xmlhttp.responseXML; 
//					var productList = xmlDoc.getElementsByTagName('Music')[0];
//					PopulatePlaylists('playList', '', '');
//					PopulatePlayListEntries(name.value);
//					$('#txtName').val(name.value);
//					$('#txtName').focus()
//					$('#txtName').select()
		}
	};
	
	xmlhttp.send();

}

function AddPlaylistEntry() {
    	var xmlhttp=new XMLHttpRequest();
			var	name=document.getElementById("txtPlaylistName");
			var	type = document.getElementById("selType").value;
			var	seqFile = document.getElementById("selSequence").value;
			var	mediaFile = document.getElementById("selMedia").value;
			var	eventSel = document.getElementById("selEvent");
			var	eventID = eventSel.value;
			var	eventName = '';
			var	pluginData = document.getElementById("txtData").value;

			if ((type == "both") &&
					((seqFile == "") || (mediaFile == "")))
			{
				var missingType = "";
				if (seqFile == "")
					missingType = "sequence";
				else if (mediaFile == "")
					missingType = "media";

				DialogError("Error adding playlist entry", "You must select a " +
										missingType + " file to add a 'Media and Sequence' " +
										"playlist entry");

				return;
			}

      if(eventSel.selectedIndex>=0)
      {
        eventName = eventSel.options[eventSel.selectedIndex].innerHTML.replace(/.. \/ .. - /, '');
      }

			var entry = new Object();

			entry.type = $('#selType').val();
			entry.enabled = 1; // FIXME
			entry.playOnce = 0; // FIXME

			if (entry.type == 'sequence')
			{
				entry.sequenceName = encodeURIComponent($('#selSequence').val());
			}
			else if (entry.type == 'media')
			{
				entry.mediaName = encodeURIComponent($('#selMedia').val());
                entry.videoOut = encodeURIComponent($('#videoOut').val());
			}
			else if (entry.type == 'both')
			{
				entry.sequenceName = encodeURIComponent($('#selSequence').val());
				entry.mediaName = encodeURIComponent($('#selMedia').val());
                entry.videoOut = encodeURIComponent($('#videoOut').val());
			}
			else if (entry.type == 'pause')
			{
				if ($('#txtPause').val() != '')
					entry.duration = parseFloat($('#txtPause').val());
				else
					entry.duration = 0;
			}
			else if (entry.type == 'event')
			{
				entry.majorID = $('#selEvent').val().substring(0,2);
				entry.minorID = $('#selEvent').val().substring(3,5);
				entry.desc =  eventName.replace(new RegExp("'", "g"),"").trim();
				entry.blocking = 0;
			}
			else if (entry.type == 'branch')
			{
				entry.branchType = $('#branchType').val();
				entry.compMode = 1; // FIXME
				entry.trueNextSection = $('#branchTrueSection').val();
				entry.trueNextItem = $('#branchTrueItem').val();
				entry.falseNextSection = $('#branchFalseSection').val();
				entry.falseNextItem = $('#branchFalseItem').val();
				entry.compInfo = new Object();
				entry.compInfo.startHour = parseInt($('#branchStartTime').val().substring(0,2));
				entry.compInfo.startMinute = parseInt($('#branchStartTime').val().substring(3,5));
				entry.compInfo.startSecond = parseInt($('#branchStartTime').val().substring(6,8));
				entry.compInfo.endHour = parseInt($('#branchEndTime').val().substring(0,2));
				entry.compInfo.endMinute = parseInt($('#branchEndTime').val().substring(3,5));
				entry.compInfo.endSecond = parseInt($('#branchEndTime').val().substring(6,8));
			}
			else if (entry.type == 'script')
			{
				entry.scriptName =  encodeURIComponent($('#selScript').val());
				entry.scriptArgs =  encodeURIComponent($('#selScript_args').val());
                entry.blocking = $('#selScript_blocking').prop('checked');;
			}
			else if (entry.type == 'mqtt')
			{
				entry.topic = $('#mqttTopic').val();
				entry.message = $('#mqttMessage').val();
			}
			else if (entry.type == 'remap')
			{
				entry.action = $('#remapAction').val();
				entry.source = parseInt($('#srcChannel').val());
				entry.destination = parseInt($('#dstChannel').val());
				entry.count = parseInt($('#channelCount').val());
				entry.loops = parseInt($('#remapLoops').val());
				entry.reverse = parseInt($('#remapReverse').val());
			}
			else if (entry.type == 'dynamic')
			{
				entry.subType = $('#dynamicSubType').val();
				entry.data = $('#dynamicData').val();
				entry.pluginHost = $('#dynamicDataHost').val();
				entry.dataArgs = $('#dynamicData_args').val();

				if ($('#drainQueue').is(':checked'))
					entry.drainQueue = 1;
				else
					entry.drainQueue = 0;
			}
			else if (entry.type == 'playlist')
			{
				entry.name = encodeURIComponent($('#selSubPlaylist').val());
			}
            else if (entry.type == 'volume')
            {
                entry.volume = $('#volume').val();
            }
			else if (entry.type == 'url')
			{
				entry.url = encodeURIComponent($('#url').val());
				entry.method = $('#urlMethod').val();
				entry.data = encodeURIComponent($('#urlData').val());
			}
			else if (entry.type == 'image')
			{
				image.imagePath = encodeURIComponent($('#imagePath').val());
				image.transitionType = $('#transitionType').val();
				image.outputDevice = encodeURIComponent($('#outputDevice').val());
			}
			else if (entry.type == 'plugin')
			{
				entry.data = $('#txtData').val();
			}

			var postData = 'command=addPlaylistEntry&data=' + JSON.stringify(entry);

			$.post("fppjson.php", postData).done(function(data) {
				PopulatePlayListEntries($('#txtPlaylistName').val(),false);
			}).fail(function() {
				$.jGrowl("Error: Unable to add new playlist entry.");
			});
		}

function ConvertPlaylistsToJSON() {
	$('#playlistConvertText').html("Converting");
	$('#playlistConverter').dialog({ height: 600, width: 800, title: "Playlist Converter" });
	$('#playlistConverter').dialog( "moveToTop" );

	$.ajax({
		url: 'fppjson.php?command=convertPlaylists',
		dataType: 'json',
		success: function(data, reqStatus, xhr) {
			if(data && typeof data === 'object') {
				for (i = 0; i < data.playlists.length; i++)
				{
					$('#playlistConverterText').append(data.playlists[i] + '<br>');
				}
				PopulatePlaylists('playList', '', '');
			}
		}
	});
}

function SavePlaylist(filter, callback)	{
	var name=document.getElementById("txtPlaylistName");
    if (name.value == "") {
        alert("Playlist name cannot be empty");
        return;
    }
    var xmlhttp=new XMLHttpRequest();
	var url = "fppjson.php?command=savePlaylist&name=" + name.value;
	xmlhttp.open("GET",url,false);
	xmlhttp.setRequestHeader('Content-Type', 'text/xml');

	xmlhttp.onreadystatechange = function () {
		if (xmlhttp.readyState == 4 && xmlhttp.status==200) {
			var xmlDoc=xmlhttp.responseXML; 
			PopulatePlaylists("playList", filter, callback);
  			PopulatePlayListEntries(name.value,true);
		}
	};
	
	xmlhttp.send();

}

function RandomizePlaylistEntries() {
    //Number of children / rows in the main playlist
    var mainPlaylistItems = $("#tblPlaylistMainPlaylist > tr").length;
    var mainPlaylistElements = [];

    //Build a list of child element id's
    $("#tblPlaylistMainPlaylist > tr").each(function (index) {
        // console.log( index + ": " + $( this ).text() )
        // console.log( index + ": " + $( this ).attr('id') );
        mainPlaylistElements.push($(this).attr('id'));
    });

    //Loop for the entire length of the main playlist, shifting items around
    var i;
    var pl = document.getElementById("txtPlaylistName").value;
    for (i = 0; i < mainPlaylistItems; i++) {
        random_pos = Math.floor(Math.random() * Math.floor(mainPlaylistItems));
        this_element = mainPlaylistElements[i];
        random_element = mainPlaylistElements[random_pos];

        //Move this item to the item determined by random pos;
        // $("#"+this_element).insertAfter( $("#"+random_element));

        //Change positions of items on the MainPlaylist
        PlaylistEntryPositionChanged('MainPlaylist', random_pos, 'MainPlaylist', i);
    }
    //Refresh playlist after all the moves
    PopulatePlayListEntries(pl, false, random_pos);
    //Refresh the sortable table
    $('.tblCreatePlaylistEntries_tbody').sortable('refresh').sortable('refreshPositions');
}

/**
 * Removes any of the following characters from the supplied name, can be used to cleanse playlist names, event names etc
 * Current needed for example it the case of the scheduler since it is still CSV and commas in a playlist name cause issues
 * Everything is currently replaced with a hyphen ( - )
 *
 * Currently unused in the front-end
 */
function RemoveIllegalChars(name) {

    // , (comma)
    // < (less than)
    // > (greater than)
    // : (colon)
    // " (double quote)
    // / (forward slash)
    // \ (backslash)
    // | (vertical bar or pipe)
    // ? (question mark)
    // * (asterisk)

    var illegalChars = [',', '<', '>', ':', '"', '/', '\\', '|', '?', '*'];

    for(ill_index = 0; ill_index < illegalChars.length; ++ill_index) {
        name = name.toString().replace(illegalChars[ill_index], " - ");
    }

    return name;
}

function CopyPlaylist()	{
    var name = document.getElementById("txtPlaylistName");
    //pop the dialog
    $("#copyPlaylist_dialog").dialog({
        buttons: {
            "Copy Playlist": function () {
                var new_playlist_name = $(this).find(".newPlaylistName").val();
                var copy_playlist_url = "fppjson.php?command=copyPlaylist&from=" + name.value + "&to=" + new_playlist_name;

                //Make the request
                var xmlhttp = new XMLHttpRequest();
                xmlhttp.open("GET", copy_playlist_url, false);
                xmlhttp.setRequestHeader('Content-Type', 'text/xml');

                xmlhttp.onreadystatechange = function () {
                    if (xmlhttp.readyState == 4 && xmlhttp.status == 200) {
                        var xmlDoc = xmlhttp.responseXML;
                        PopulatePlaylists("playList", '', '');
                        var firstPlaylist = document.getElementById("playlist0");
                        if (firstPlaylist)
                            PopulatePlayListEntries(firstPlaylist.innerHTML, true);
                        else
                            PopulatePlayListEntries();

                        //Close the dialog once done
                        $("#copyPlaylist_dialog").dialog("close");
                    }
                };

                xmlhttp.send();
            },
            Cancel: function () {
                $(this).dialog("close");
            }
        },
        open: function (event, ui) {
            //Generate a name for the new playlist
            $(this).find(".newPlaylistName").val(name.value + " - Copy");
        },
        close: function () {
            $(this).find(".newPlaylistName").val("New Playlist Name");
        }
    });
}

function DeletePlaylist() {
		var name=document.getElementById("txtPlaylistName");
    	var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=deletePlaylist&name=" + name.value;
			xmlhttp.open("GET",url,false);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
	 
			xmlhttp.onreadystatechange = function () {
				if (xmlhttp.readyState == 4 && xmlhttp.status==200) 
				{
					var xmlDoc=xmlhttp.responseXML;
                    status_xml = xmlDoc.getElementsByTagName("Status")[0].textContent;

                    if (status_xml === "Failure" || status_xml !== "Success"){
                        //fail
                        DialogError("Failed to delete Playlist","Failed to delete Playlist '" + name.value + "'.")
					}

                    PopulatePlaylists("playList", '', '');
					var firstPlaylist = document.getElementById("playlist0");
					if ( firstPlaylist )
						PopulatePlayListEntries(firstPlaylist.innerHTML,true);
					else
						PopulatePlayListEntries();
				}
			};
			
			xmlhttp.send();

}

		
		
function RemovePlaylistEntry()	{
		if (lastPlaylistEntry == '')
		{
			$.jGrowl("Error: No playlist item selected.");
			return;
		}

			var name=document.getElementById("txtPlaylistName");
    	var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=deleteEntry&index=" + (lastPlaylistEntry-1) + "&section=" + lastPlaylistSection;
			xmlhttp.open("GET",url,false);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
	 
			xmlhttp.onreadystatechange = function () {
				if (xmlhttp.readyState == 4 && xmlhttp.status==200)
				{
					var xmlDoc=xmlhttp.responseXML; 
          PopulatePlayListEntries(name.value,false);
					SelectEntry(lastPlaylistEntry);
				}
			};
			
			xmlhttp.send();
		}

		function reloadPage()
		{
			location.reload(true);
		}

		function reloadPage()
		{
			location.reload(true);
		}

		function ManualGitUpdate()
		{
			SetButtonState("#ManualUpdate", "disable");
			document.body.style.cursor = "wait";

			$.get("fppxml.php?command=manualGitUpdate"
			).done(function() {
				document.body.style.cursor = "pointer";
				location.reload(true);
			}).fail(function() {
				SetButtonState("#ManualUpdate", "enable");
				document.body.style.cursor = "pointer";
				DialogError("Manual Git Update", "Update failed");
			});
		}

		function PingIP(ip, count)
		{
			if (ip == "")
				return;

				$('#helpText').html("Pinging " + ip + "<br><br>This will take a few seconds to load");
				$('#dialog-help').dialog({ height: 600, width: 800, position: { my: 'center', at: 'top', of: window}, title: "Ping " + ip });
//				$('#dialog-help').dialog( "moveToTop" );

				$.get("ping.php?ip=" + ip + "&count=" + count
				).done(function(data) {
						$('#helpText').html(data);
				}).fail(function() {
						$('#helpText').html("Error pinging " + ip);
				});
		}

		function PingE131IP(id)
		{
			var ip = $("[name='txtIP\[" + id + "\]']").val();

			PingIP(ip, 3);
		}

		function ViewReleaseNotes(version) {
				$('#helpText').html("Retrieving Release Notes");
				$('#dialog-help').dialog({ height: 800, width: 800, title: "Release Notes for FPP v" + version });
				$('#dialog-help').dialog( "moveToTop" );

				$.get("fppxml.php?command=viewReleaseNotes&version=" + version
				).done(function(data) {
						$('#helpText').html(
						"<center><input onClick='UpgradeFPPVersion(\"" + version + "\");' type='button' class='buttons' value='Upgrade'></center>" +
						"<pre style='white-space: pre-wrap; word-wrap: break-word;'>" + data + "</pre>"
						);
				}).fail(function() {
						$('#helpText').html("Error loading release notes.");
				});
		}

		function UpgradeFPPVersion(newVersion)
		{
			if (confirm('Do you wish to upgrade the Falcon Player?\n\nClick "OK" to continue.\n\nThe system will automatically reboot to complete the upgrade.\nThis can take a long time,  20-30 minutes on slower devices.'))
			{
				document.body.style.cursor = "wait";
				$.get("fppxml.php?command=upgradeFPPVersion&version=v" + newVersion
				).done(function() {
					document.body.style.cursor = "pointer";
                       
                    ClearRestartFlag();
                    ClearRebootFlag();
                       
                    //Delay reboot for 1 second to allow flags to be cleared
                    setTimeout(function () {
                                  var xmlhttp = new XMLHttpRequest();
                                  var url = "fppxml.php?command=rebootPi";
                                  xmlhttp.open("GET", url, true);
                                  xmlhttp.setRequestHeader('Content-Type', 'text/xml');
                                  xmlhttp.send();
                                  
                                  //Show FPP is rebooting notification for 60 seconds then reload the page
                                  $.jGrowl('FPP is rebooting..', {life: 60000});
                                  setTimeout(function () {
                                          location.reload(true);
                                        }, 60000);
                         }, 1000);

					
				}).fail(function() {
					document.body.style.cursor = "pointer";
					DialogError("Upgrade FPP Version", "Upgrade failed");
				});
			}
		}

		function ChangeGitBranch(newBranch)
		{
			document.body.style.cursor = "wait";
			$.get("fppxml.php?command=changeGitBranch&branch=" + newBranch
			).done(function() {
				document.body.style.cursor = "pointer";
				location.reload(true);
			}).fail(function() {
				document.body.style.cursor = "pointer";
				DialogError("Switch Git Branch", "Switch failed");
			});
		}
	
		function SetAutoUpdate(enabled)
		{
			var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=setAutoUpdate&enabled=" + enabled;
			xmlhttp.open("GET",url,false);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
			xmlhttp.onreadystatechange = function () {
				if (xmlhttp.readyState == 4)
					location.reload(true);
			}
			xmlhttp.send();
		}
	
		function SetDeveloperMode(enabled)
		{
			var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=setDeveloperMode&enabled=" + enabled;
			xmlhttp.open("GET",url,false);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
			xmlhttp.onreadystatechange = function () {
				if (xmlhttp.readyState == 4)
					location.reload(true);
			}
			xmlhttp.send();
		}
	
		function SelectEntry(index)
		{
			$('#sortable li:nth-child(1n)').removeClass('selectedEntry');
			if(index > $("#sortable li").size())
			{	
				index = $("#sortable li").size()-1;
			}
			var j = $("#sortable li").get(index).addClass('selectedEntry');
			lastPlaylistEntry = index;
		}
		
		function SetUniverseCount(input)
		{
			var txtCount=document.getElementById("txtUniverseCount");
			var count = Number(txtCount.value);
			if(isNaN(count)) {
				count = 8;
			}
            
            if (count < UniverseCount) {
                while (count < UniverseCount) {
                    UniverseSelected = UniverseCount;
                    DeleteUniverse(input);
                }
            } else {
                if (UniverseCount == 0) {
                    var data = {};
                    var channelData = {};
                    channelData.enabled = 0;
                    channelData.type = "universes";
                    channelData.universes = [];
                    var universe = {};
                    universe.active = 1;
                    universe.description = "";
                    universe.id = 1;
                    universe.startChannel = 1;
                    universe.channelCount = 512;
                    universe.type = 0;
                    universe.address = "";
                    universe.priority = 0;
                    channelData.universes.push(universe);
                    if (input) {
                        data.channelInputs = [];
                        data.channelInputs.push(channelData);
                    } else {
                        data.channelOutputs = [];
                        data.channelOutputs.push(channelData);
                    }
                    populateUniverseData(data, false, input);
                }
                var selectIndex = UniverseCount - 1;
                var universe=Number(document.getElementById("txtUniverse[" + selectIndex + "]").value);
                var universeType=document.getElementById("universeType[" + selectIndex + "]").value;
                var unicastAddress=document.getElementById("txtIP[" + selectIndex + "]").value;
                var size=Number(document.getElementById("txtSize[" + selectIndex + "]").value);
                var ucount=Number(document.getElementById("numUniverseCount[" + selectIndex + "]").value);
                var startAddress=Number(document.getElementById("txtStartAddress[" + selectIndex + "]").value);
                var active=document.getElementById("chkActive[" + selectIndex + "]").value;
                var priority=Number(document.getElementById("txtPriority[" + selectIndex + "]").value);
                
                var tbody=document.getElementById("tblUniversesBody");  //get the table
                var origRow = tbody.rows[selectIndex];
                var origUniverseCount = UniverseCount;
                while (UniverseCount < count) {
                    var row = origRow.cloneNode(true);
                    tbody.appendChild(row);
                    UniverseCount++;
                }
                UniverseCount =  origUniverseCount;
                SetUniverseInputNames();
                while (UniverseCount < count) {
                    if (universe != 0) {
                        universe += ucount;
                        document.getElementById("txtUniverse[" + UniverseCount + "]").value = universe;
                    }
                    startAddress += size * ucount;
                    document.getElementById("txtStartAddress[" + UniverseCount + "]").value = startAddress;

                    if (!input) {
                        row.cells[10].innerHTML = "<input id='PingButton' type='button' value='Ping' onClick='PingE131IP(" + UniverseCount + ");'/>";
                    }
                    
                    UniverseCount++;
                }
                document.getElementById("txtUniverseCount").value = UniverseCount;
            }
		}

        function IPOutputTypeChanged(item) {
            var itemVal = $(item).val();
            if (itemVal == 4 || itemVal == 5) {
                var univ = $(item).parent().parent().find("input.txtUniverse");
                univ.prop('disabled', true);
                var univc = $(item).parent().parent().find("input.numUniverseCount");
                univc.prop('disabled', true);
                var sz = $(item).parent().parent().find("input.txtSize");
                sz.prop('max', 512000);
            } else {
                var univ = $(item).parent().parent().find("input.txtUniverse");
                univ.prop('disabled', false);
                if (parseInt(univ.val()) < 1) {
                    univ.val(1);
                }
                var univc = $(item).parent().parent().find("input.numUniverseCount");
                univc.prop('disabled', false);
                if (parseInt(univc.val()) < 1) {
                    univc.val(1);
                }
                var sz = $(item).parent().parent().find("input.txtSize");
                var val = parseInt(sz.val());
                if (val > 512) {
                    sz.val(512);
                }
                sz.prop('max', 512);
            }
        }
        function populateUniverseData(data, reload, input) {
			var headHTML="";
			var bodyHTML="";
			UniverseCount = 0;
            var inputStyle = "";
            if (input)
                inputStyle = "style='display: none;'";

            var channelData = input ? data.channelInputs[0] : data.channelOutputs[0];
            
            if (channelData.universes.length > 0) {
                headHTML = "<tr class=\"tblheader\">" +
                "<th width=\"5%\" align='left'>Line<br>#</th>" +
                "<th width=\"5%\" align='left'>Active</th>" +
                "<th width=\"30%\" align='left'>Description</th>" +
                "<th width=\"8%\" align='left'>FPP Start<br>Channel</th>" +
                "<th width=\"8%\" align='left'>Universe<br>#</th>" +
                "<th width=\"8%\" align='left'>Universe<br>Count</th>" +
                "<th width=\"8%\" align='left'>Universe<br>Size</th>" +
                "<th width=\"15%\" align='left'>Universe Type</th>" +
                "<th width=\"12%\" align='left' " + inputStyle + ">Unicast<br>Address</th>" +
                "<th width=\"8%\" align='left' " + inputStyle + ">Priority</th>" +
                "<th width=\"12%\" align='left' " + inputStyle + ">Ping</th>" +
                "</tr>";
            }
            UniverseCount = channelData.universes.length;
            for (var i = 0; i < channelData.universes.length; i++) {
                var universe = channelData.universes[i];
                var active = universe.active;
                var desc = universe.description;
                var uid = universe.id;
                var ucount = universe.universeCount;
                if (!ucount) {
                    ucount = 1;
                }
                var startAddress = universe.startChannel;
                var size = universe.channelCount;
                var type = universe.type;
                var unicastAddress =  universe.address;
                var priority =  universe.priority;
                unicastAddress = unicastAddress.trim();

                var activeChecked = active == 1  ? "checked=\"checked\"" : "";
                var typeMulticastE131 = type == 0 ? "selected" : "";
                var typeUnicastE131 = type == 1 ? "selected": "";
                var typeBroadcastArtNet = type == 2 ? "selected" : "";
                var typeUnicastArtNet = type == 3 ? "selected": "";
                var typeDDPR = type == 4 ? "selected": "";
                var typeDDP1 = type == 5 ? "selected": "";

                var universeSize = 512;
                var universeCountDisable = "";
                var universeNumberDisable = "";
                if (type == 4 || type == 5) {
                    universeSize = 512000;
                    universeCountDisable = " disabled";
                    universeNumberDisable = " disabled";
                }

                bodyHTML += "<tr class=\"rowUniverseDetails\">" +
                            "<td><span class='rowID' id='rowID'>" + (i+1).toString() + "</span></td>" +
                            "<td><input class='chkActive' type='checkbox' " + activeChecked +"/></td>" +
                            "<td><input class='txtDesc' type='text' size='24' maxlength='64' value='" + desc + "'/></td>" +
                            "<td><input class='txtStartAddress' type='number' min='1' max='1048576' value='" + startAddress.toString() + "'/></td>" +
                            "<td><input class='txtUniverse' type='number' min='1' max='63999' value='" + uid.toString() + "'" + universeNumberDisable + "/></td>";

                bodyHTML += "<td><input class='numUniverseCount' type='number' min='1' max='250' value='" + ucount.toString() + "'" + universeCountDisable + "/></td>";

                bodyHTML += "<td><input class='txtSize' type='number'  min='1'  max='" + universeSize + "' value='" + size.toString() + "'></td>" +
                            "<td><select class='universeType' style='width:150px'";

                if (input) {
                    bodyHTML += ">" +
                                "<option value='0' " + typeMulticastE131 + ">E1.31 - Multicast</option>" +
                                "<option value='1' " + typeUnicastE131 + ">E1.31 - Unicast</option>";
                } else {
                    bodyHTML += " onChange='IPOutputTypeChanged(this);'>" +
                                "<option value='0' " + typeMulticastE131 + ">E1.31 - Multicast</option>" +
                                "<option value='1' " + typeUnicastE131 + ">E1.31 - Unicast</option>" +
                                "<option value='2' " + typeBroadcastArtNet + ">ArtNet - Broadcast</option>" +
                                "<option value='3' " + typeUnicastArtNet + ">ArtNet - Unicast</option>" +
                                "<option value='4' " + typeDDPR + ">DDP Raw Channel Numbers</option>" +
                                "<option value='5' " + typeDDP1 + ">DDP One Based</option>";
                }

                bodyHTML += "</select></td>" +
                            "<td " + inputStyle + "><input class='txtIP' type='text' value='" + unicastAddress + "' size='15' maxlength='32'></td>" +
                            "<td " + inputStyle + "><input class='txtPriority' type='text' size='4' maxlength='4' value='" + priority.toString() + "'/></td>" +
                            "<td " + inputStyle + "><input id='PingButton' type=button onClick='PingE131IP(" + i.toString() + ");' value='Ping'></td>" +
                            "</tr>";
            }

            if (!input) {
                var ecb = $('#E131Enabled');
                if ( channelData.enabled == 1) {
                    ecb.prop('checked', true);
                } else {
                    ecb.prop('checked', false)
                }
            }
            $('#tblUniversesHead').html(headHTML);
            $('#tblUniversesBody').html(bodyHTML);

            $('#txtUniverseCount').val(UniverseCount);

            SetUniverseInputNames(); // in co-universes.php
		}
        function getUniverses(reload, input)
        {
            var url = "fppjson.php?command=getChannelOutputs&file=universeOutputs";
            if (input) {
                url = "fppjson.php?command=getChannelOutputs&file=universeInputs";
            }
            $.getJSON(url, function(data) {
                        populateUniverseData(data, reload, input)
                      }).fail(function() {
                              UniverseCount = 0;
                              $('#txtUniverseCount').val(UniverseCount);
                      })
        }

		function getPixelnetDMXoutputs(reload)
		{
    	var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=getPixelnetDMXoutputs&reload=" + reload;
			xmlhttp.open("GET",url,false);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
 			var innerHTML="";
			xmlhttp.onreadystatechange = function () {
				if (xmlhttp.readyState == 4 && xmlhttp.status==200) 
				{
					var xmlDoc=xmlhttp.responseXML; 
					var entries = xmlDoc.getElementsByTagName('PixelnetDMXentries')[0];
					if(entries.childNodes.length> 0)
					{
						innerHTML = "<tr class=\"tblheader\">" +  
    	                  "<td>#</td>" +
                        "<td>Act</td>" +
                        "<td>Type</td>" +
												"<td>Start</td>" +
												"</tr>";
												
							PixelnetDMXcount = entries.childNodes.length;
							for(i=0;i<PixelnetDMXcount;i++)
							{
								var active = entries.childNodes[i].childNodes[0].textContent;
								var type = entries.childNodes[i].childNodes[1].textContent;
								var startAddress = entries.childNodes[i].childNodes[2].textContent;

								var activeChecked = active == 1  ? "checked=\"checked\"" : "";
								var pixelnetChecked = type == 0 ? "selected" : "";
								var dmxChecked = type == 1 ? "selected" : "";
								
								innerHTML += 	"<tr class=\"rowUniverseDetails\">" +
								              "<td>" + (i+1).toString() + "</td>" +
															"<td><input name=\"FPDchkActive[" + i.toString() + "]\" id=\"FPDchkActive[" + i.toString() + "]\" type=\"checkbox\" " + activeChecked +"/></td>" +
															"<td><select id=\"pixelnetDMXtype[" + i.toString() + "]\" name=\"pixelnetDMXtype[" + i.toString() + "]\" style=\"width:150px\">" +
															      "<option value=\"0\" " + pixelnetChecked + ">Pixelnet</option>" +
															      "<option value=\"1\" " + dmxChecked + ">DMX</option></select></td>" + 
															"<td><input name=\"FPDtxtStartAddress[" + i.toString() + "]\" id=\"FPDtxtStartAddress[" + i.toString() + "]\" type=\"text\" size=\"8\" value=\"" + startAddress.toString() + "\"/></td>" +
															"</tr>";

							}
					}
					else
					{
						innerHTML = "No Results Found";	
					}
					var results = document.getElementById("tblOutputs");
					results.innerHTML = innerHTML;	
				}
			};
			
			xmlhttp.send();			
		}

        function SetUniverseRowInputNames(row, id) {
            var fields = Array('chkActive', 'txtDesc', 'txtStartAddress',
                               'txtUniverse', 'numUniverseCount', 'txtSize', 'universeType', 'txtIP',
                               'txtPriority');
            row.find('span.rowID').html((id + 1).toString());
            
            for (var i = 0; i < fields.length; i++)
            {
                row.find('input.' + fields[i]).attr('name', fields[i] + '[' + id + ']');
                row.find('input.' + fields[i]).attr('id', fields[i] + '[' + id + ']');
                row.find('select.' + fields[i]).attr('name', fields[i] + '[' + id + ']');
                row.find('select.' + fields[i]).attr('id', fields[i] + '[' + id + ']');
            }
        }
		function SetUniverseInputNames()
		{
            var id = 0;
			$('#tblUniversesBody tr').each(function() {
                SetUniverseRowInputNames($(this), id);
				id += 1;
			});
		}

		function InitializeUniverses()
		{
			UniverseSelected =0;
			UniverseCount=0;	
		}
		
		function DeleteUniverse(input)
		{
            if (UniverseSelected > 0) {
                var selectedIndex = (UniverseSelected-1);
                for(i = UniverseSelected; i < UniverseCount; i++, selectedIndex++) {
                    
                    document.getElementById("txtUniverse[" + selectedIndex + "]").value = document.getElementById("txtUniverse[" + i + "]").value
                    document.getElementById("txtDesc[" + selectedIndex + "]").value = document.getElementById("txtDesc[" + i + "]").value
                    document.getElementById("universeType[" + selectedIndex + "]").value = document.getElementById("universeType[" + i + "]").value;
                    var universeType = document.getElementById("universeType[" + selectedIndex + "]").value;
                    document.getElementById("txtStartAddress[" + selectedIndex + "]").value  = document.getElementById("txtStartAddress[" + i + "]").value;
                    document.getElementById("numUniverseCount[" + selectedIndex + "]").value  = document.getElementById("numUniverseCount[" + i + "]").value;

                    var checkb = document.getElementById("chkActive[" + i + "]");
                    document.getElementById("chkActive[" + selectedIndex + "]").checked = checkb.checked;
                    document.getElementById("txtSize[" + selectedIndex + "]").value = document.getElementById("txtSize[" + i + "]").value;
                    document.getElementById("txtIP[" + selectedIndex + "]").value = document.getElementById("txtIP[" + i + "]").value;
                    document.getElementById("txtPriority[" + selectedIndex + "]").value = document.getElementById("txtPriority[" + i + "]").value;
                    if ((universeType == '1') || (universeType == '3')) {
                        document.getElementById("txtIP[" + selectedIndex + "]").disabled = false;
                    } else {
                        document.getElementById("txtIP[" + selectedIndex + "]").disabled = true;
                    }
                }
                document.getElementById("tblUniversesBody").deleteRow(selectedIndex);
                UniverseCount--;
                document.getElementById("txtUniverseCount").value = UniverseCount;
                UniverseSelected = 0;
            }
        
        }
		
		function CloneUniverses(cloneNumber)
		{
			var selectIndex = (UniverseSelected-1).toString();
			if(!isNaN(cloneNumber))
			{
				if((UniverseSelected + cloneNumber -1) < UniverseCount)
				{
					var universe=Number(document.getElementById("txtUniverse[" + selectIndex + "]").value)+1;
                    var universeDesc=document.getElementById("txtDesc[" + selectIndex + "]").value;
					var universeType=document.getElementById("universeType[" + selectIndex + "]").value;
					var unicastAddress=document.getElementById("txtIP[" + selectIndex + "]").value;
					var size=Number(document.getElementById("txtSize[" + selectIndex + "]").value);
                    var uCount=Number(document.getElementById("numUniverseCount[" + selectIndex + "]").value);
					var startAddress=Number(document.getElementById("txtStartAddress[" + selectIndex + "]").value)+ size;
					var active=document.getElementById("chkActive[" + selectIndex + "]").value;
					var priority=Number(document.getElementById("txtPriority[" + selectIndex + "]").value);

					for(i=UniverseSelected;i<UniverseSelected+cloneNumber;i++,universe++)
					{
                        document.getElementById("txtDesc[" + i + "]").value = universeDesc;
						document.getElementById("txtUniverse[" + i + "]").value	= universe.toString();
						document.getElementById("universeType[" + i + "]").value = universeType;
						document.getElementById("txtStartAddress[" + i + "]").value	= startAddress.toString();
                        document.getElementById("numUniverseCount[" + i + "]").value = uCount.toString();
						document.getElementById("chkActive[" + i + "]").value = active;
						document.getElementById("txtSize[" + i + "]").value = size.toString();
						document.getElementById("txtIP[" + i + "]").value = unicastAddress;
						document.getElementById("txtPriority[" + i + "]").value = priority;
						if((universeType == '1') || (universeType == '3'))
						{
							document.getElementById("txtIP[" + i + "]").disabled = false;
						}
						else
						{
							document.getElementById("txtIP[" + i + "]").disabled = true;
						}
						
						startAddress+=size;
					}
				}
			} else {
				DialogError("Clone Universe", "Error, invalid number");
			}
		}
        function CloneUniverse()
        {
            var answer = prompt ("How many universes to clone from selected universe?","1");
            var cloneNumber = Number(answer);
            CloneUniverses(cloneNumber);
        }

		function ClonePixelnetDMXoutput()
		{
			var answer = prompt ("How many outputs to clone from selected output?","1");
			var cloneNumber = Number(answer);
			var selectIndex = (PixelnetDMXoutputSelected-1).toString();
			if(!isNaN(cloneNumber))
			{
				if((PixelnetDMXoutputSelected + cloneNumber -1) < 12)
				{
					var active=document.getElementById("FPDchkActive[" + selectIndex + "]").value;
					var pixelnetDMXtype=document.getElementById("pixelnetDMXtype[" + selectIndex + "]").value;
					var size = pixelnetDMXtype == "0" ? 4096:512;
					var startAddress=Number(document.getElementById("FPDtxtStartAddress[" + selectIndex + "]").value)+ size;
					for(i=PixelnetDMXoutputSelected;i<PixelnetDMXoutputSelected+cloneNumber;i++)
					{
						document.getElementById("pixelnetDMXtype[" + i + "]").value	 = pixelnetDMXtype;
						document.getElementById("FPDtxtStartAddress[" + i + "]").value	 = startAddress.toString();
						document.getElementById("FPDchkActive[" + i + "]").value = active;
						startAddress+=size;
					}
					$.jGrowl("" + cloneNumber + " Outputs Cloned");
				}
			} else {
				DialogError("Clone Output", "Error, invalid number");
			}
		}

        function postUniverseJSON(input) {
            var postData = {};
            
            var output = {};
            output.type = "universes";
            if (!input) {
                output.enabled = document.getElementById("E131Enabled").checked ? 1 : 0;
            } else {
                output.enabled = 1;
            }
            output.startChannel = 1;
            output.channelCount = -1;
            output.universes = [];
            
            var i;
            for(i = 0; i < UniverseCount; i++) {
                var universe = {};
                universe.active = document.getElementById("chkActive[" + i + "]").checked ? 1 : 0;
                universe.description = document.getElementById("txtDesc[" + i + "]").value;;
                universe.id = parseInt(document.getElementById("txtUniverse[" + i + "]").value);
                universe.startChannel = parseInt(document.getElementById("txtStartAddress[" + i + "]").value);
                universe.universeCount = parseInt(document.getElementById("numUniverseCount[" + i + "]").value);

                universe.channelCount = parseInt(document.getElementById("txtSize[" + i + "]").value);
                universe.type = parseInt(document.getElementById("universeType[" + i + "]").value);
                universe.address = document.getElementById("txtIP[" + i + "]").value;
                universe.priority = parseInt(document.getElementById("txtPriority[" + i + "]").value);
                output.universes.push(universe);
            }
            if (input) {
                postData.channelInputs = [];
                postData.channelInputs.push(output);
            } else {
                postData.channelOutputs = [];
                postData.channelOutputs.push(output);
            }
            var fileName = input ? 'universeInputs' : 'universeOutputs';
            var postDataString = 'command=setChannelOutputs&file='+ fileName +'&data={' + encodeURIComponent(JSON.stringify(postData)) + '}';
            
            $.post("fppjson.php", postDataString).done(function(data) {
                                                       $.jGrowl("E1.31 Universes Saved");
                                                       SetRestartFlag();
                                                       CheckRestartRebootFlags();
                                                 }).fail(function() {
                                                        $.jGrowl("Error: Unable to save E1.31 Universes.");
                                                  });
            
        }

		function validateUniverseData()
		{
			var i;
			var txtUniverse;
			var txtStartAddress;
			var txtSize;
			var universeType;
			var txtPriority;
			var result;
			var returnValue=true;
			for(i=0;i<UniverseCount;i++)
			{

                // unicast address
                universeType=document.getElementById("universeType[" + i + "]").value;
                if(universeType == 1 || universeType == 3 || universeType == 4 || universeType == 5)
                {
                    if(!validateIPaddress("txtIP[" + i + "]"))
                    {
                        returnValue = false;
                    }
                }
				// universe
                if (universeType >= 0 && universeType <= 3) {
                    txtUniverse=document.getElementById("txtUniverse[" + i + "]");
                    if(!validateNumber(txtUniverse,1,63999))
                    {
                        returnValue = false;
                    }
                }
				// start address
				txtStartAddress=document.getElementById("txtStartAddress[" + i + "]");				
				if(!validateNumber(txtStartAddress,1,1048576))
				{
					returnValue = false;
				}
                // size
                txtSize=document.getElementById("txtSize[" + i + "]");
                var max = 512;
                if (universeType == 4 || universeType == 5) {
                    max = 512000;
                }
                if(!validateNumber(txtSize,1,max))
                {
                    returnValue = false;
                }

				// priority
				txtPriority=document.getElementById("txtPriority[" + i + "]");
				if(!validateNumber(txtPriority,0,9999))
				{
					returnValue = false;
				}
			}
			return returnValue;
		}
		
		function validateIPaddress(id)
		{
			var ipb = document.getElementById(id);
			var ip = ipb.value;
			var isHostnameRegex = /[a-z]/i;
			var isHostname = ip.match(isHostnameRegex);
			if ((ip == "") || ((isHostname == null) || (isHostname.length > 0)) || (/^(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/.test(ip)))
			{
				ipb.style.border = "#000 0px none";
				return true;
			}

			ipb.style.border = "#F00 2px solid";
			return false;
		}  

		function validateNumber(textbox,minimum,maximum)   
		{  
			result = true;
			value = Number(textbox.value);
			if(isNaN(value))
			{
				textbox.style.border="red solid 1px";
				textbox.value = ""; 
				result = false;
			}
			if(value >= minimum && value <= maximum)
			{
				return true;	
			}
			else
			{
				textbox.style.border="red solid 1px";
				textbox.value = ""; 
				result = false;
			alert(textbox.value + ' is not between ' + minimum + ' and ' + maximum);
			}
		}
		
		function ReloadPixelnetDMX()
		{
			getPixelnetDMXoutputs("TRUE");
		} 

		function ReloadSchedule()
		{
			getSchedule("TRUE");	
		}  

		function ScheduleDaysSelectChanged(item)
		{
			var name = $(item).attr('name');
			var maskSpan = name.replace('selDay', 'dayMask');
			maskSpan = maskSpan.replace('[', '\\[');
			maskSpan = maskSpan.replace(']', '\\]');

			if ($(item).find('option:selected').text() == 'Day Mask')
				$('#' + maskSpan).show();
			else
				$('#' + maskSpan).hide();
		}

		function getSchedule(reload)
		{
    	var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=getSchedule&reload=" + reload;
			$(tblSchedule).empty();
			xmlhttp.open("GET",url,false);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
			xmlhttp.onreadystatechange = function () {
				if (xmlhttp.readyState == 4 && xmlhttp.status==200) 
				{
					var xmlDoc=xmlhttp.responseXML; 
					var entries = xmlDoc.getElementsByTagName('ScheduleEntries')[0];
					if(entries.childNodes.length> 0)
					{
						GetPlaylistArray();
						headerHTML = "<tr class=\"tblheader\">" +  
							"<td>#</td>" +
							"<td>Enable</td>" +
							"<td>Start Date</td>" +
							"<td>End Date</td>" +
							"<td>Playlist</td>" +
							"<td>Day(s)</td>" +
							"<td>Start Time</td>" +
							"<td>End Time</td>" +
							"<td>Repeat</td>" +
							"</tr>";


							
						$('#tblSchedule').append(headerHTML);					
							ScheduleCount = entries.childNodes.length;
							for(i=0;i<ScheduleCount;i++)
							{
									var enable = entries.childNodes[i].childNodes[0].textContent;
									var day = entries.childNodes[i].childNodes[1].textContent;
									var playlist = entries.childNodes[i].childNodes[2].textContent;
									var startTime = entries.childNodes[i].childNodes[3].textContent;
									var endTime = entries.childNodes[i].childNodes[4].textContent;
									var repeat = entries.childNodes[i].childNodes[5].textContent;
									var startDate = entries.childNodes[i].childNodes[6].textContent;
									var endDate = entries.childNodes[i].childNodes[7].textContent;

									var enableChecked = enable == 1  ? "checked=\"checked\"" : "";
									var repeatChecked = repeat == 1  ? "checked=\"checked\"" : "";
									var dayChecked_0 =  day == 0  ? "selected" : "";
									var dayChecked_1 =  day == 1  ? "selected" : "";
									var dayChecked_2 =  day == 2  ? "selected" : "";
									var dayChecked_3 =  day == 3  ? "selected" : "";
									var dayChecked_4 =  day == 4  ? "selected" : "";
									var dayChecked_5 =  day == 5  ? "selected" : "";
									var dayChecked_6 =  day == 6  ? "selected" : "";
									var dayChecked_7 =  day == 7  ? "selected" : "";
									var dayChecked_8 =  day == 8  ? "selected" : "";
									var dayChecked_9 =  day == 9  ? "selected" : "";
									var dayChecked_10 =  day == 10  ? "selected" : "";
									var dayChecked_11 =  day == 11  ? "selected" : "";
									var dayChecked_12 =  day == 12  ? "selected" : "";
									var dayChecked_13 =  day == 13  ? "selected" : "";
									var dayChecked_14 =  day == 14  ? "selected" : "";
									var dayChecked_15 =  day == 15  ? "selected" : "";
									var dayChecked_0x10000 = day >= 0x10000 ? "selected" : "";
									var dayMaskStyle = day >= 0x10000 ? "" : "display: none;";

									playlistOptionsText = ""
									for(j=0;j<PlaylistCount;j++)
									{
										playListChecked = playListArray[j] == playlist ? "selected" : "";
										playlistOptionsText +=  "<option value=\"" + playListArray[j] + "\" " + playListChecked + ">" + playListArray[j] + "</option>";
									}
									var tableRow = 	"<tr class=\"rowScheduleDetails\">" +
								              "<td class='center'>" + (i+1).toString() + "</td>" +
															"<td class='center' ><input  name=\"chkEnable[" + i.toString() + "]\" id=\"chkEnable[" + i.toString() + "]\" type=\"checkbox\" " + enableChecked +"/></td>" +
															"<td><input class='date center'  name=\"txtStartDate[" + i.toString() + "]\" id=\"txtStartDate[" + i.toString() + "]\" type=\"text\" size=\"10\" value=\"" + startDate + "\"/></td><td>" +
																"<input class='date center'  name=\"txtEndDate[" + i.toString() + "]\" id=\"txtEndDate[" + i.toString() + "]\" type=\"text\" size=\"10\" value=\"" + endDate + "\"/></td>" +

															"<td><select id=\"selPlaylist[" + i.toString() + "]\" name=\"selPlaylist[" + i.toString() + "]\">" +
															playlistOptionsText + "</select></td>" +
															"<td><select id=\"selDay[" + i.toString() + "]\" name=\"selDay[" + i.toString() + "]\" onChange='ScheduleDaysSelectChanged(this);'>" +
															      "<option value=\"7\" " + dayChecked_7 + ">Everyday</option>" +
															      "<option value=\"0\" " + dayChecked_0 + ">Sunday</option>" +
															      "<option value=\"1\" " + dayChecked_1 + ">Monday</option>" +
															      "<option value=\"2\" " + dayChecked_2 + ">Tuesday</option>" +
															      "<option value=\"3\" " + dayChecked_3 + ">Wednesday</option>" +
															      "<option value=\"4\" " + dayChecked_4 + ">Thursday</option>" +
															      "<option value=\"5\" " + dayChecked_5 + ">Friday</option>" +
															      "<option value=\"6\" " + dayChecked_6 + ">Saturday</option>" +
															      "<option value=\"8\" " + dayChecked_8 + ">Mon-Fri</option>" +
															      "<option value=\"9\" " + dayChecked_9 + ">Sat/Sun</option>" +
															      "<option value=\"10\" " + dayChecked_10 + ">Mon/Wed/Fri</option>" +
															      "<option value=\"11\" " + dayChecked_11 + ">Tues/Thurs</option>" +
															      "<option value=\"12\" " + dayChecked_12 + ">Sun-Thurs</option>" +
															      "<option value=\"13\" " + dayChecked_13 + ">Fri/Sat</option>" +
															      "<option value=\"14\" " + dayChecked_14 + ">Odd</option>" +
															      "<option value=\"15\" " + dayChecked_15 + ">Even</option>" +
															      "<option value=\"65536\" " + dayChecked_0x10000 + ">Day Mask</option></select><br>" +
																  "<span id='dayMask[" + i + "]' style='" + dayMaskStyle + "'>" +
																  "S:<input type='checkbox' name='maskSunday[" + i + "]'" +
																	((day & 0x04000) ? " checked" : "") + "> " +
																  "M:<input type='checkbox' name='maskMonday[" + i + "]'" +
																	((day & 0x02000) ? " checked" : "") + "> " +
																  "T:<input type='checkbox' name='maskTuesday[" + i + "]'" +
																	((day & 0x01000) ? " checked" : "") + "> " +
																  "W:<input type='checkbox' name='maskWednesday[" + i + "]'" +
																	((day & 0x00800) ? " checked" : "") + "> " +
																  "T:<input type='checkbox' name='maskThursday[" + i + "]'" +
																	((day & 0x00400) ? " checked" : "") + "> " +
																  "F:<input type='checkbox' name='maskFriday[" + i + "]'" +
																	((day & 0x00200) ? " checked" : "") + "> " +
																  "S:<input type='checkbox' name='maskSaturday[" + i + "]'" +
																	((day & 0x00100) ? " checked" : "") + "> " +
																  "</span></td>" +
															"<td><input class='time center'  name=\"txtStartTime[" + i.toString() + "]\" id=\"txtStartTime[" + i.toString() + "]\" type=\"text\" size=\"8\" value=\"" + startTime + "\"/></td><td>" +
															"<input class='time center' name=\"txtEndTime[" + i.toString() + "]\" id=\"txtEndTime[" + i.toString() + "]\" type=\"text\" size=\"8\" value=\"" + endTime + "\"/></td>" +
															"<td class='center' ><input name=\"chkRepeat[" + i.toString() + "]\" id=\"chkEnable[" + i.toString() + "]\" type=\"checkbox\" " + repeatChecked +"/></td>" +
															"</tr>";
															
									$('#tblSchedule').append(tableRow);
									$('.time').timepicker({
										'timeFormat': 'H:i:s',
										'typeaheadHighlight': false,
										'show2400': true,
										'noneOption': [
												{
													'label': 'SunRise',
													'value': 'SunRise'
												},
												{
													'label': 'SunSet',
													'value': 'SunSet'
												}
											]
										});

									$('.date').datepicker({
										'changeMonth': true,
										'changeYear': true,
										'dateFormat': 'yy-mm-dd',
										'minDate': new Date(2014, 1 - 1, 1),
										'maxDate': new Date(2099, 12 - 1, 31),
										'showButtonPanel': true,
										'selectOtherMonths': true,
										'showOtherMonths': true,
										'yearRange': "2014:2099"
										});

							}
					}
				}
			};
			
			xmlhttp.send();			
		}

	function GetPlaylistArray()
	{
    	var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=getPlayLists";
			var playListArr = new Array();
			xmlhttp.open("GET",url,false);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');

			xmlhttp.onreadystatechange = function() {
			var xmlDoc=xmlhttp.responseXML; 
			var playlist = xmlDoc.getElementsByTagName('Playlists')[0];
			PlaylistCount =playlist.childNodes.length;
			if(playlist.childNodes.length> 0)
			{
  			for(i=0;i<playlist.childNodes.length;i++)
				{
					playListArr[i] = playlist.childNodes[i].textContent;
				}
			}
			playListArray = playListArr;
			};

			xmlhttp.send();
		}

	function AddScheduleEntry()
	{
    	var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=addScheduleEntry";
			xmlhttp.open("GET",url,false);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
	 
			xmlhttp.onreadystatechange = function () {
				if (xmlhttp.readyState == 4 && xmlhttp.status==200) 
				{
					getSchedule("FALSE");					
				}
			};
			
			xmlhttp.send();
	}

		function DeleteScheduleEntry()
		{
    	var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=deleteScheduleEntry&index=" + (ScheduleEntrySelected-1).toString();
			xmlhttp.open("GET",url,false);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
	 
			xmlhttp.onreadystatechange = function () {
				if (xmlhttp.readyState == 4 && xmlhttp.status==200) 
				{
          getSchedule("FALSE");
				}
			};
			
			xmlhttp.send();
		}

	function GetSequenceFiles()
	{
    	var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=getSequences";
			var seqArr = new Array();
			$(tblSequences).empty();
			xmlhttp.open("GET",url,false);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
			xmlhttp.send();

			var xmlDoc=xmlhttp.responseXML; 
			var sequences = xmlDoc.getElementsByTagName('Sequences')[0];
			SequenceCount =sequences.childNodes.length;
			if(sequences.childNodes.length> 0)
			{
				var innerhtml = '';
  			for(i=0;i<sequences.childNodes.length;i++)
				{
					var name = sequences.childNodes[i].childNodes[0].textContent;
					var time = sequences.childNodes[i].childNodes[1].textContent;
					var tableRow = "<tr class ='seqDetails'><td class ='seqName'>" + name + "</td><td class ='seqTime'>" + time + "</td></tr>";
					$('#tblSequences').append(tableRow);
				}
			}
			sequenceArray = seqArr;
		}

	function GetMusicFiles()
	{
    	var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=getMusicFiles";
			var seqArr = new Array();
			$(tblMusic).empty();
			xmlhttp.open("GET",url,false);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
			xmlhttp.send();

			var xmlDoc=xmlhttp.responseXML; 
			var sequences = xmlDoc.getElementsByTagName('Songs')[0];
			SequenceCount =sequences.childNodes.length;
			if(sequences.childNodes.length> 0)
			{
				var innerhtml = '';
  			for(i=0;i<sequences.childNodes.length;i++)
				{
					var name = sequences.childNodes[i].childNodes[0].textContent;
					var time = sequences.childNodes[i].childNodes[1].textContent;
					var tableRow = "<tr class ='songDetails'><td class ='songName'>" + name + "</td><td class ='songTime'>" + time + "</td></tr>";
					$('#tblMusic').append(tableRow);
				}
			}
			sequenceArray = seqArr;
		}

  function GetFiles(dir)
  {
    var xmlhttp=new XMLHttpRequest();
    var url = "fppxml.php?command=getFiles&dir=" + dir;
    $('#tbl' + dir).empty();
    xmlhttp.open("GET",url,false);
    xmlhttp.setRequestHeader('Content-Type', 'text/xml');
    xmlhttp.send();

    var xmlDoc=xmlhttp.responseXML; 
    var files = xmlDoc.getElementsByTagName('Files')[0];
    if(files.childNodes.length> 0)
    {
      var innerhtml = '';
      for(i=0; i<files.childNodes.length; i++)
      {
        // Thanks: http://stackoverflow.com/questions/5396560/how-do-i-convert-special-utf-8-chars-to-their-iso-8859-1-equivalent-using-javasc
        var encodedstring = decodeURIComponent(escape(files.childNodes[i].childNodes[0].textContent));
        var name = "";
        try{
            // If the string is UTF-8, this will work and not throw an error.
            name=decodeURIComponent(escape(encodedstring));
        }catch(e){
            // If it isn't, an error will be thrown, and we can asume that we have an ISO string.
            name=encodedstring;
        }

        var time = files.childNodes[i].childNodes[1].textContent.replace(/ /g, '&nbsp;');
        var fileInfo = files.childNodes[i].childNodes[2].textContent;

          var tableRow = "<tr class='fileDetails' id='fileDetail_" + i + "'><td class ='fileName'>" + name + "</td><td class='fileExtraInfo'>" + fileInfo + "</td><td class ='fileTime'>" + time + "</td></tr>";
        $('#tbl' + dir).append(tableRow);
      }
    }
  }

	function moveFile(file)
	{
    	var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=moveFile&file=" + encodeURIComponent(file);
			xmlhttp.open("GET",url,false);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
			xmlhttp.send();
	}

	function updateFPPStatus()
	{
		var status = GetFPPStatus();
	}
	
	function IsFPPDrunning()
	{
    	var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=isFPPDrunning";
			xmlhttp.open("GET",url,true);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
			xmlhttp.onreadystatechange = function () 
			{
				if (xmlhttp.readyState == 4 && xmlhttp.status==200) 
				{
					var xmlDoc=xmlhttp.responseXML; 
					var status = xmlDoc.getElementsByTagName('Status')[0];
					var retValue='false';
					if(status.childNodes.length> 0)
					{
						ret = status.childNodes[0].textContent;
						if(ret == 'true')
						{
							SetButtonState('#btnDaemonControl','enable');
							$("#btnDaemonControl").attr('value', 'Stop FPPD');
							$('#daemonStatus').html("FPPD is running.");
						}
						else if (ret == 'updating')
						{
							SetButtonState('#btnDaemonControl','disable');
							$("#btnDaemonControl").attr('value', 'Start FPPD');
							$('#daemonStatus').html("FPP is currently updating.");
						} 
						else
						{
							SetButtonState('#btnDaemonControl','enable');
							$("#btnDaemonControl").attr('value', 'Start FPPD');
							$('#daemonStatus').html("FPPD is stopped.");
						} 
					}
				}
			};
			xmlhttp.send();
	}

	function SetupUIForMode(fppMode)
	{
		if (fppMode == 1) // Bridge Mode
		{
			$("#playerInfo").hide();
			$("#nextPlaylist").hide();
			$("#bytesTransferred").show();
			$("#remoteStatus").hide();
            $("#playerStatusBottom").hide();
		}
		else
		{
			if (fppMode == 8) // Remote Mode
			{
				$("#playerInfo").show();
				$("#playerStatusTop").hide();
				$("#playerStatusBottom").hide();
				$("#remoteStatus").show();
				$("#nextPlaylist").hide();
				$("#bytesTransferred").hide();
			}
			else // Player or Master Modes
			{
				$("#playerInfo").show();
				$("#playerStatusTop").show();
				$("#playerStatusBottom").show();
				$("#remoteStatus").hide();
				$("#nextPlaylist").show();
				$("#bytesTransferred").hide();
			}
		}
	}

    var temperatureUnit = false;
    function changeTemperatureUnit() {
        if (temperatureUnit) {
            temperatureUnit = false;
        } else {
            temperatureUnit = true;
        }
    }
	function GetFPPStatus()	{
		$.ajax({
			url: 'fppjson.php?command=getFPPstatus',
			dataType: 'json',
			success: function(response, reqStatus, xhr) {	
				
				if(response && typeof response === 'object') {

					if(response.status_name == 'stopped') {
						
						$('#fppTime').html('');
						SetButtonState('#btnDaemonControl','enable');
						$("#btnDaemonControl").attr('value', 'Start FPPD');
						$('#daemonStatus').html("FPPD is stopped.");
					
					} else if(response.status_name == 'updating') {

						$('#fppTime').html('');
						SetButtonState('#btnDaemonControl','disable');
						$("#btnDaemonControl").attr('value', 'Start FPPD');
						$('#daemonStatus').html("FPP is currently updating.");

					} else {

							SetButtonState('#btnDaemonControl','enable');
							$("#btnDaemonControl").attr('value', 'Stop FPPD');
							$('#daemonStatus').html("FPPD is running.");

						parseStatus(response);

					}

					lastStatus = response.status;
				}

			},
			complete: function() {
				clearTimeout(statusTimeout);
				statusTimeout = setTimeout(GetFPPStatus, 1000);
			}
		})
	}

	var firstStatusLoad = 1;
	function parseStatus(jsonStatus) {
		var fppStatus = jsonStatus.status;
		var fppMode = jsonStatus.mode;
        var status = "Idle";
        if (jsonStatus.status_name == "testing") {
            status = "Testing";
        }
		if (fppStatus == STATUS_IDLE ||
			fppStatus == STATUS_PLAYING ||
			fppStatus == STATUS_STOPPING_GRACEFULLY ) {
		
			$("#btnDaemonControl").attr('value', 'Stop FPPD');
			$('#daemonStatus').html("FPPD is running.");
		}

		var Volume = parseInt(jsonStatus.volume);
		$('#volume').html(Volume);
		$('#slider').slider('value', Volume);
		SetSpeakerIndicator(Volume);

		SetupUIForMode(fppMode);

		if (fppMode == 1) {
			// Bridge Mode
			$('#fppTime').html(jsonStatus.time);

			if (firstStatusLoad || $('#e131statsLiveUpdate').is(':checked'))
				GetUniverseBytesReceived();
		} else if (fppMode == 8) {
			$('#fppTime').html(jsonStatus.time);

			if(jsonStatus.time_elapsed) {
				status = "Syncing to Master: Elapsed: " + jsonStatus.time_elapsed + "&nbsp;&nbsp;&nbsp;&nbsp;Remaining: " + jsonStatus.time_remaining;
			}

			$('#txtRemoteStatus').html(status);
			$('#txtRemoteSeqFilename').html(jsonStatus.sequence_filename);
			$('#txtRemoteMediaFilename').html(jsonStatus.media_filename);
		
		} else {

			var nextPlaylist = jsonStatus.next_playlist;
			var nextPlaylistStartTime = jsonStatus.next_playlist_start_time;
			var currentPlaylist = jsonStatus.current_playlist;

			if (fppStatus == STATUS_IDLE) {
			
				gblCurrentPlaylistIndex =0;
				gblCurrentPlaylistEntryType = '';
				gblCurrentPlaylistEntrySeq = '';
				gblCurrentPlaylistEntrySong = '';
				$('#txtPlayerStatus').html(status);
				$('#txtTimePlayed').html("");								
				$('#txtTimeRemaining').html("");	
				$('#txtSeqFilename').html("");
				$('#txtMediaFilename').html("");
								
				// Enable Play
				SetButtonState('#btnPlay','enable');
                SetButtonState('#btnStopNow','disable');
                SetButtonState('#btnPrev','disable');
				SetButtonState('#btnNext','disable');
				SetButtonState('#btnStopGracefully','disable');
				$('#selStartPlaylist').removeAttr("disabled");
				UpdateCurrentEntryPlaying(0);
			
			} else if (currentPlaylist.playlist != "") {
				var playerStatusText = "Playing <strong>'" + currentPlaylist.playlist + "'</strong>";
                var repeatMode = jsonStatus.repeat_mode;
				if ((gblCurrentLoadedPlaylist != currentPlaylist.playlist) ||
					(gblCurrentPlaylistIndex != currentPlaylist.index) ||
					(gblCurrentPlaylistEntryType != currentPlaylist.type) ||
					(gblCurrentPlaylistEntrySeq != jsonStatus.current_sequence) ||
					(gblCurrentPlaylistEntrySong != jsonStatus.current_song)) {
					$('#selStartPlaylist').val(currentPlaylist.playlist);
					PopulateStatusPlaylistEntries(false,currentPlaylist.playlist,true);

					gblCurrentPlaylistEntryType = currentPlaylist.type;
					gblCurrentPlaylistEntrySeq = jsonStatus.current_sequence;
					gblCurrentPlaylistEntrySong = jsonStatus.current_song;
				}

				SetButtonState('#btnPlay','disable');
				SetButtonState('#btnStopNow','enable');
                SetButtonState('#btnPrev','enable');
                SetButtonState('#btnNext','enable');
				SetButtonState('#btnStopGracefully','enable');
				$('#selStartPlaylist').attr("disabled");

				if(fppStatus == STATUS_STOPPING_GRACEFULLY) {
					playerStatusText += " - Stopping Gracefully";
				}

				$('#txtPlayerStatus').html(playerStatusText);
				$('#txtTimePlayed').html("Elapsed: " + jsonStatus.time_elapsed );				
				$('#txtTimeRemaining').html("Remaining: " + jsonStatus.time_remaining );	
				$('#txtSeqFilename').html(jsonStatus.current_sequence);
				$('#txtMediaFilename').html(jsonStatus.current_song);

//				if(currentPlaylist.index != gblCurrentPlaylistIndex && 
//					currentPlaylist.index <= gblCurrentLoadedPlaylistCount) {
// FIXME, somehow this doesn't refresh on the first page load, so refresh
// every time for now
if (1) {
							
							UpdateCurrentEntryPlaying(currentPlaylist.index);
							gblCurrentPlaylistIndex = currentPlaylist.index;
					}

				if (repeatMode) {
					$("#chkRepeat").prop( "checked", true );
				} else{
					$("#chkRepeat").prop( "checked", false );
				}
			
            } else if (jsonStatus.current_sequence != "") {
                //only playing a sequence
                var playerStatusText = "Playing <strong>'" + jsonStatus.current_sequence + "'</strong>";
                SetButtonState('#btnPlay','disable');
                SetButtonState('#btnPrev','enable');
                SetButtonState('#btnNext','enable');
                SetButtonState('#btnStopNow','enable');
                SetButtonState('#btnStopGracefully','disable');
                
                $('#txtPlayerStatus').html(playerStatusText);
                $('#txtTimePlayed').html("Elapsed: " + jsonStatus.time_elapsed );
                $('#txtTimeRemaining').html("Remaining: " + jsonStatus.time_remaining );
				$('#txtSeqFilename').html(jsonStatus.current_sequence);
				$('#txtMediaFilename').html(jsonStatus.current_song);

			}

			$('#txtNextPlaylist').html(nextPlaylist.playlist);
			$('#nextPlaylistTime').html(nextPlaylist.start_time);
			$('#fppTime').html(jsonStatus.time);
            
		}

        if (jsonStatus.hasOwnProperty('sensors')) {
            var sensorText = "<table id='sensorTable'>";
            for (var i = 0; i < jsonStatus.sensors.length; i++) {
                if ((jsonStatus.sensors.length < 4) || ((i % 2) == 0)) {
                    sensorText += "<tr>";
                }
                sensorText += "<td>";
                sensorText += jsonStatus.sensors[i].label;
                sensorText += "</td><td";
                if (jsonStatus.sensors[i].valueType == "Temperature") {
                    sensorText += " onclick='changeTemperatureUnit()'>";
                    var val = jsonStatus.sensors[i].value;
                    if (temperatureUnit) {
                        val *= 1.8;
                        val += 32;
                        sensorText += val.toFixed(1);
                        sensorText += "F";
                    } else {
                        sensorText += val.toFixed(1);
                        sensorText += "C";
                    }
                } else {
                    sensorText += ">";
                    sensorText += jsonStatus.sensors[i].formatted;
                }
                sensorText += "</td>";
                
                if ((jsonStatus.sensors.length > 4) && ((i % 2) == 1)) {
                    sensorText += "<tr>";
                }
            }
            sensorText += "</table>";
            var sensorData = document.getElementById("sensorData");
            sensorData.innerHTML = sensorText;
        }

		firstStatusLoad = 0;
	}

	function GetUniverseBytesReceived()
	{	
		var html='';
		var html1='';
    var xmlhttp=new XMLHttpRequest();
		var url = "fppxml.php?command=getUniverseReceivedBytes";
		xmlhttp.open("GET",url,true);
		xmlhttp.setRequestHeader('Content-Type', 'text/xml');
		xmlhttp.onreadystatechange = function () {
			if (xmlhttp.readyState == 4 && xmlhttp.status==200) 
			{
					var xmlDoc=xmlhttp.responseXML; 
					var receivedBytes = xmlDoc.getElementsByTagName('receivedBytes')[0];
					if(receivedBytes && receivedBytes.childNodes.length> 0)
					{
						html =  "<table>";
						html += "<tr id=\"rowReceivedBytesHeader\"><td>Universe</td><td>Start Address</td><td>Packets</td><td>Bytes</td><td>Errors</td></tr>";

                        var i;
                        var maxRows = receivedBytes.childNodes.length / 2;
                        if (maxRows < 32) {
                            maxRows = 32;
                        }
						for(i=0;i<receivedBytes.childNodes.length;i++)
						{
								if(i==maxRows)
								{
									html += "</table>";
									html1 = html;
									html =  "<table>";
									html += "<tr id=\"rowReceivedBytesHeader\"><td>Universe</td><td>Start Address</td><td>Packets</td><td>Bytes</td><td>Errors</td></tr>";
								}
								var universe = receivedBytes.childNodes[i].childNodes[0].textContent;
								var startChannel = receivedBytes.childNodes[i].childNodes[1].textContent;
								var bytes = receivedBytes.childNodes[i].childNodes[2].textContent;
								var packets = receivedBytes.childNodes[i].childNodes[3].textContent;
                                var errors = receivedBytes.childNodes[i].childNodes[4].textContent;
								html += "<tr><td>" + universe + "</td>";
								html += "<td>" + startChannel + "</td><td>" + packets + "</td><td>" + bytes + "</td><td>" + errors + "</td></tr>";
						}
						html += "</table>";
					}
					if(receivedBytes && receivedBytes.childNodes.length>32)
					{
						$("#bridgeStatistics1").html(html1);
						$("#bridgeStatistics2").html(html);
					}
					else
					{
						$("#bridgeStatistics1").html(html);
						$("#bridgeStatistics2").html('');
					}					
			}
		};
		xmlhttp.send();
	}
	
	function UpdateCurrentEntryPlaying(index,lastIndex)
	{
		$('#tblStatusPlaylist tbody tr').removeClass('PlaylistRowPlaying');
		$('#tblStatusPlaylist tbody td').removeClass('PlaylistPlayingIcon');

		if((index >= 0) && ($('#playlistRow' + index).length))
		{
			$("#colEntryNumber" + index).addClass("PlaylistPlayingIcon");
			$("#playlistRow" + index).addClass("PlaylistRowPlaying");
		}
	}
	
	function SetIconForCurrentPlayingEntry(index)
	{
	}
	
	
	function SetButtonState(button,state)
	{
			// Enable Button
			if(state == 'enable')
			{
				$(button).addClass('buttons');
				$(button).removeClass('disableButtons');
				$(button).removeAttr("disabled");
			}
			else
			{
				$(button).removeClass('buttons');
				$(button).addClass('disableButtons');
				$(button).attr("disabled", "disabled");
			}
	}
	
	function StopGracefully()
	{
    	var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=stopGracefully";
			xmlhttp.open("GET",url,true);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
			xmlhttp.send();
	}

	function StopNow()
	{
    	var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=stopNow";
			xmlhttp.open("GET",url,true);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
			xmlhttp.send();
	}

	function ToggleSequencePause()
	{
		$.get("fppjson.php?command=toggleSequencePause");
	}

	function SingleStepSequence()
	{
		$.get("fppjson.php?command=singleStepSequence");
	}

	function SingleStepSequenceBack()
	{
		$.get("fppjson.php?command=singleStepSequenceBack");
	}

	function StopFPPD()
	{
    	var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=stopFPPD";
			xmlhttp.open("GET",url,true);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
			xmlhttp.send();
	}

	function SetSettingRestart(key, value) {
		var restartFPPD = 1;

		if ((key == 'LogLevel') ||
			(key == 'LogMask'))
		{
			restartFPPD = 0;
		}

		SetSetting(key, value, restartFPPD, 0);
	}

	function SetSettingReboot(key, value) {
		SetSetting(key, value, 0, 1);
	}

	function SetSetting(key, value, restart, reboot) {
		$.get("fppjson.php?command=setSetting&key=" + key + "&value=" + value)
			.done(function() {
				if ((key != 'restartFlag') && (key != 'rebootFlag'))
					$.jGrowl(key + " setting saved.");

				CheckRestartRebootFlags();
			}).fail(function() {
				DialogError("Failed to save " + key + " setting.");
				CheckRestartRebootFlags();
			});
	}

	function ClearRestartFlag() {
		settings['restartFlag'] = 0;
		SetSetting('restartFlag', 0, 0, 0);
	}

	function SetRestartFlag() {
		settings['restartFlag'] = 1;
		SetSettingRestart('restartFlag', 1);
	}

	function ClearRebootFlag() {
		settings['rebootFlag'] = 0;
		SetSetting('rebootFlag', 0, 0, 0);
	}

	function SetRebootFlag() {
		settings['rebootFlag'] = 1;
		SetSettingReboot('rebootFlag', 1);
	}

	function CheckRestartRebootFlags() {
		if (settings['disableUIWarnings'] == 1)
		{
			$('#restartFlag').hide();
			$('#rebootFlag').hide();
			return;
		}

		if (settings['restartFlag'] == 1)
			$('#restartFlag').show();
		else
			$('#restartFlag').hide();

		if (settings['rebootFlag'] == 1)
		{
			$('#restartFlag').hide();
			$('#rebootFlag').show();
		}
		else
		{
			$('#rebootFlag').hide();
		}
	}

function GetFPPDUptime()
	{
		$.get("fppxml.php?command=getFPPDUptime");
	}

function RestartFPPD() {
		$('html,body').css('cursor','wait');
		$.get("fppxml.php?command=restartFPPD"
		).done(function() {
			$('html,body').css('cursor','auto');
			$.jGrowl('FPPD Restarted');
			ClearRestartFlag();
		}).fail(function() {
			$('html,body').css('cursor','auto');

            //If fail, the FPP may have rebooted (eg.FPPD triggering a reboot due to disabling soundcard or activating Pi Pixel output
            //start polling and see if we can wait for the FPP to come back up
            //restartFlag will already be cleared, but to keep things simple, just call the original code
            retries = 0;
            retries_max = max_retries;//avg timeout is 10-20seconds, polling resolves after 6-10 polls
            //attempt to poll every second, AJAX block for the default browser timeout if host is unavail
            retry_poll_interval_arr['restartFPPD'] = setInterval(function () {
                poll_result = false;
                if (retries < retries_max) {
                    // console.log("Polling @ " + retries);
                    //poll for FPPDRunning as it's the simplest command to run and doesn't put any extra processing load on the backend
                    $.ajax({
                            url: "fppxml.php?command=isFPPDrunning",
                            timeout: 1000,
                            async: true
                        }
                    ).success(
                        function () {
                            poll_result = true;
                            //FPP is up then
                            clearInterval(retry_poll_interval_arr['restartFPPD']);
                            //run original code for success
                            $.jGrowl('FPPD Restarted');
                            ClearRestartFlag();
                        }
                    ).fail(
                        function () {
                            poll_result = false;
                            retries++;
                            //If on first try throw up a FPP is rebooting notification
                            if(retries === 1){
                                //Show FPP is rebooting notification for 10 seconds
                                $.jGrowl('FPP is rebooting..',{ life: 10000 });
                            }
                        }
                    );

                    // console.log("Polling Result " + poll_result);
                } else {
                    //run original code
                    clearInterval(retry_poll_interval_arr['restartFPPD']);
                    DialogError("Restart FPPD", "Error restarting FPPD");
                }
            }, 2000);
		});
	}

function zeroPad(num, places) {
	var zero = places - num.toString().length + 1;
	return Array(+(zero > 0 && zero)).join("0") + num;
}
	
function ControlFPPD()
	{
    var xmlhttp=new XMLHttpRequest();
		var btnVal = $("#btnDaemonControl").attr('value');
		if(btnVal == "Stop FPPD")
		{
			var url = "fppxml.php?command=stopFPPD";
		}
		else
		{
			var url = "fppxml.php?command=startFPPD";
		}
		xmlhttp.open("GET",url,true);
		xmlhttp.setRequestHeader('Content-Type', 'text/xml');
		xmlhttp.onreadystatechange = function () {
			if (xmlhttp.readyState == 4 && xmlhttp.status==200) 
			{
				var i = 19;
			}
		};
		xmlhttp.send();
	}
	
function StatusPopulatePlaylists()
	{
		var playlistOptionsText="";
		GetPlaylistArray();
		
		for(j=0;j<PlaylistCount;j++)
		{
			playlistOptionsText +=  "<option value=\"" + playListArray[j] + "\">" + playListArray[j] + "</option>";
		}	
		$('#selStartPlaylist').html(playlistOptionsText);
		

	}
	
function StartPlaylistNow()
	{
		var Playlist =  $("#selStartPlaylist").val();
    var xmlhttp=new XMLHttpRequest();
		var repeat = $("#chkRepeat").is(':checked')?'checked':'unchecked';
		if ((PlaySectionSelected != '') &&
			(PlayEntrySelected >= $('#tblPlaylist' + PlaySectionSelected + ' >tr').length))
		{
				PlayEntrySelected = 0;
				PlaySectionSelected = "";
		}
		var url = "fppxml.php?command=startPlaylist&playList=" + Playlist + "&repeat=" + repeat + "&playEntry=" + PlayEntrySelected + "&section=" + PlaySectionSelected ;
		xmlhttp.open("GET",url,true);
		xmlhttp.setRequestHeader('Content-Type', 'text/xml');
		xmlhttp.send();
	}

function PlayEffect(startChannel)
{
	// Start Channel 0 is a special case meaning use the channel # in the .eseq
	if ((startChannel == undefined) ||
		(startChannel == ""))
		startChannel = "0";

	var url = "fppxml.php?command=playEffect&effect=" + EffectNameSelected + "&startChannel=" + startChannel;
	var xmlhttp=new XMLHttpRequest();
	xmlhttp.open("GET",url,false);
	xmlhttp.setRequestHeader('Content-Type', 'text/xml');
	xmlhttp.send();

	GetRunningEffects();
}

function StopEffect()
{
	var url = "fppxml.php?command=stopEffect&id=" + RunningEffectSelected;
	var xmlhttp=new XMLHttpRequest();
	xmlhttp.open("GET",url,false);
	xmlhttp.setRequestHeader('Content-Type', 'text/xml');
	xmlhttp.send();

	GetRunningEffects();
}

var gblLastRunningEffectsXML = "";

function GetRunningEffects()
{
	var url = "fppxml.php?command=getRunningEffects";
	var xmlhttp=new XMLHttpRequest();
	xmlhttp.open("GET",url,true);
	xmlhttp.setRequestHeader('Content-Type', 'text/xml');

	xmlhttp.onreadystatechange = function () {
		if (xmlhttp.readyState == 4 && xmlhttp.status==200)
		{
			var xmlDoc=xmlhttp.responseXML;
			var xmlText = new XMLSerializer().serializeToString(xmlDoc);

			$('#tblRunningEffects').html('');
			if (xmlText != gblLastRunningEffectsXML)
			{
				xmlText = gblLastRunningEffectsXML;

				var entries = xmlDoc.getElementsByTagName('RunningEffects')[0];

				if(entries.childNodes.length> 0)
				{
					for(i=0;i<entries.childNodes.length;i++)
					{
						id = entries.childNodes[i].childNodes[0].textContent;
						name = entries.childNodes[i].childNodes[1].textContent;

						$('#tblRunningEffects').append('<tr><td width="5%">' + id + '</td><td width="95%">' + name + '</td></tr>');
					}

					setTimeout(GetRunningEffects, 1000);
				}
			}
		}
	}

	xmlhttp.send();
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
		$('#newEventEffect').val('');
		$('#newEventStartChannel').val('');
		$('#newEventScript').val('');
		$('#newEventScriptArgs').val('');
		$('#newEvent').show();
	}

	function EditEvent()
	{
		var IDt = $('#event_' + TriggerEventSelected).find('td:eq(0)').text();
		var ID = IDt.replace(' \/ ', '_');
		$('#newEventID').val(ID);
		if ($('#newEventID').val() != ID)
		{
			$('#newEventID').prepend("<option value='" + ID + "' selected>" + IDt + "</option>");
		}

		$('#newEventName').val($('#event_' + TriggerEventSelected).find('td:eq(1)').text());
		$('#newEventScript').val($('#event_' + TriggerEventSelected).find('td:eq(2)').text());
		$('#newEventScriptArgs').val($('#event_' + TriggerEventSelected).find('td:eq(3)').text());
		$('#newEventEffect').val($('#event_' + TriggerEventSelected).find('td:eq(4)').text());
		$('#newEventStartChannel').val($('#event_' + TriggerEventSelected).find('td:eq(5)').text());
		$('#newEvent').show();

		NewEventEffectChanged();
	}

	function NewEventEffectChanged()
	{
		if ($('#newEventEffect').val() != "")
			$('#newEventStartChannelWrapper').show();
		else
			$('#newEventStartChannelWrapper').hide();
	}

	function InsertNewEvent(name, id, effect, startChannel, script, scriptArgs)
	{
		var idStr = id.replace('_', ' / ');
		$('#tblEventEntries').append(
		"<tr id='event_" + id + "'><td class='eventTblID'>" + idStr +
            "</td><td class='eventTblName'>" + name +
            "</td><td class='eventTblScript'>" + script +
            "</td><td class='eventTblScriptArgs'>" + scriptArgs +
            "</td><td class='eventTblEffect'>" + effect +
            "</td><td class='eventTblStartCh'>" + startChannel +
            "</td></tr>"
		);
	}

	function UpdateExistingEvent(name, id, effect, startChannel, script, scriptArgs)
	{
		var idStr = id.replace('_', ' / ');
		var row = $('#tblEventEntries tr#event_' + id);
		row.find('td:eq(0)').text(idStr);
		row.find('td:eq(1)').text(name);
		row.find('td:eq(2)').text(script);
		row.find('td:eq(3)').text(scriptArgs);
		row.find('td:eq(4)').text(effect);
		row.find('td:eq(5)').text(startChannel);
	}

	function SaveEvent()
	{
		var url = "fppxml.php?command=saveEvent&event=" + $('#newEventName').val() +
			"&id=" + $('#newEventID').val() +
			"&effect=" + $('#newEventEffect').val() +
			"&startChannel=" + $('#newEventStartChannel').val() +
			"&script=" + $('#newEventScript').val() +
			"&scriptArgs=" + encodeURIComponent($('#newEventScriptArgs').val());

        //If event name is empty, warn the user and skip saving
        if (typeof ($('#newEventName').val()) === 'undefined' || $('#newEventName').val() == "") {
            DialogError("Error Saving Event", "Event Name must be set!")
        } else {
            var xmlhttp = new XMLHttpRequest();
            xmlhttp.open("GET", url, true);
            xmlhttp.setRequestHeader('Content-Type', 'text/xml');
            xmlhttp.onreadystatechange = function () {
                if (xmlhttp.readyState == 4 && xmlhttp.status == 200) {
                    if ($('#tblEventEntries tr#event_' + $('#newEventID').val()).attr('id')) {
                        UpdateExistingEvent($('#newEventName').val(), $('#newEventID').val(),
                            $('#newEventEffect').val(), $('#newEventStartChannel').val(),
                            $('#newEventScript').val(), $('#newEventScriptArgs').val());
                    }
                    else {
                        InsertNewEvent($('#newEventName').val(), $('#newEventID').val(),
                            $('#newEventEffect').val(), $('#newEventStartChannel').val(),
                            $('#newEventScript').val(), $('#newEventScriptArgs').val());
                    }
                    SetButtonState('#btnTriggerEvent', 'disable');
                    SetButtonState('#btnEditEvent', 'disable');
                    SetButtonState('#btnDeleteEvent', 'disable');
                }
            }
            xmlhttp.send();
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

	function RebootPi()
	{
		if (confirm('REBOOT the Falcon Player?'))
		{
			ClearRestartFlag();
			ClearRebootFlag();

			//Delay reboot for 1 second to allow flags to be cleared
            setTimeout(function () {
                var xmlhttp = new XMLHttpRequest();
                var url = "fppxml.php?command=rebootPi";
                xmlhttp.open("GET", url, true);
                xmlhttp.setRequestHeader('Content-Type', 'text/xml');
                xmlhttp.send();

                //Show FPP is rebooting notification for 10 seconds
                $.jGrowl('FPP is rebooting..', {life: 10000});
            }, 1000);
		} 
	}

	function ShutdownPi()
	{
		if (confirm('SHUTDOWN the Falcon Player?'))
		{
			var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=shutdownPi";
			xmlhttp.open("GET",url,true);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
			xmlhttp.send();
		} 
	}
		
function PopulateStatusPlaylistEntries(playselected,playList,reloadFile)
{
			var type;
			var pl;
    	var xmlhttp=new XMLHttpRequest();
			var innerHTML="";
			var fromMemory = "";
			if(playselected==true)
			{
				pl = $('#selStartPlaylist :selected').text(); 
			}
			else
			{	
				pl = playList;
				fromMemory = '&fromMemory=1';
			}

			PlayEntrySelected = 0;
			PlaySectionSelected = '';

	$.ajax({
		url: 'fppjson.php?command=getPlayListEntries&pl=' + pl + '&reload=' + reloadFile + '&mergeSubs=1' + fromMemory,
		dataType: 'json',
		success: function(data, reqStatus, xhr) {	
			var innerHTML = "";

			if(data && typeof data === 'object') {
				var entries = 0;
				if (data.hasOwnProperty('leadIn') && data.leadIn.length > 0)
				{
					innerHTML = "";
					for (i = 0; i < data.leadIn.length; i++)
					{
						innerHTML += PlaylistEntryToTR(entries, data.leadIn[i], 0);
						entries++;
					}
					$('#tblPlaylistLeadIn').html(innerHTML);
					$('#tblPlaylistLeadInHeader').show();
				}
				else
				{
					$('#tblPlaylistLeadIn').html("");
					$('#tblPlaylistLeadInHeader').hide();
				}

				if (data.hasOwnProperty('mainPlaylist') && data.mainPlaylist.length > 0)
				{
					innerHTML = "";
					for (i = 0; i < data.mainPlaylist.length; i++)
					{
						innerHTML += PlaylistEntryToTR(entries, data.mainPlaylist[i], 0);
						entries++;
					}
					$('#tblPlaylistMainPlaylist').html(innerHTML);
					$('#tblPlaylistMainPlaylistHeader').show();
				}
				else
				{
					$('#tblPlaylistMainPlaylist').html("");
					$('#tblPlaylistMainPlaylistHeader').hide();
				}

				if (data.hasOwnProperty('leadOut') && data.leadOut.length > 0)
				{
					innerHTML = "";
					for (i = 0; i < data.leadOut.length; i++)
					{
						innerHTML += PlaylistEntryToTR(entries, data.leadOut[i], 0);
						entries++;
					}
					$('#tblPlaylistLeadOut').html(innerHTML);
					$('#tblPlaylistLeadOutHeader').show();
				}
				else
				{
					$('#tblPlaylistLeadOut').html("");
					$('#tblPlaylistLeadOutHeader').hide();
				}

				if (entries == 0)
				{
					innerHTML  =  "<tr class=\"playlistPlayingEntry\">";
					innerHTML +=  "<td>No entries in playlist section.</td>";
					innerHTML += "</tr>";
					$('.tblCreatePlaylistEntries_tbody').html(innerHTML);
				}

				gblCurrentLoadedPlaylist = playList;
				gblCurrentLoadedPlaylistCount = entries;
				$('#txtPlaylistName').val(playList);
			}
			else
			{
				innerHTML  =  "<tr class=\"playlistPlayingEntry\">";
				innerHTML +=  "<td>No entries in playlist section.</td>";
				innerHTML += "</tr>";
				$('.tblCreatePlaylistEntries_tbody').html(innerHTML);
			}
		}
	});
}

function SelectStatusPlaylistEntryRow(index)
{
		PlayEntrySelected  = index;
}

function SetVolume(value)
{
			var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=setVolume&volume=" + value;
			xmlhttp.open("GET",url,true);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
			xmlhttp.send();
}

function SetFPPDmode()
{
	$.get("fppxml.php?command=setFPPDmode&mode=" + $('#selFPPDmode').val()
	).done(function() {
		$.jGrowl("fppMode Saved");
		RestartFPPD();
	}).fail(function() {
		DialogError("Save fppdMode", "Save Failed");
	});
}

function SetE131interface()
{
			var xmlhttp=new XMLHttpRequest();
			var iface = $('#selE131interfaces').val();	
			var url = "fppxml.php?command=setE131interface&iface=" + iface;
			xmlhttp.open("GET",url,true);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
			xmlhttp.send();
			$.jGrowl("E1.31 Interface Saved");
}

function GetVolume()
{
    var xmlhttp=new XMLHttpRequest();
		var url = "fppxml.php?command=getVolume";
		xmlhttp.open("GET",url,true);
		xmlhttp.setRequestHeader('Content-Type', 'text/xml');
		xmlhttp.onreadystatechange = function () {
			if (xmlhttp.readyState == 4 && xmlhttp.status==200) 
			{
					var xmlDoc=xmlhttp.responseXML; 
					var Volume = parseInt(xmlDoc.getElementsByTagName('Volume')[0].childNodes[0].textContent);
					if ((Volume < 0) || (Volume == "NaN"))
					{
						Volume = 75;
						SetVolume(Volume);
					}
					$('#volume').html(Volume);
					$('#slider').slider('value', Volume);
					SetSpeakerIndicator(Volume);
			}
		};
		xmlhttp.send();

}

function GetFPPDmode()
{
    var xmlhttp=new XMLHttpRequest();
		var url = "fppxml.php?command=getFPPDmode";
		xmlhttp.open("GET",url,true);
		xmlhttp.setRequestHeader('Content-Type', 'text/xml');
		xmlhttp.onreadystatechange = function () {
			if (xmlhttp.readyState == 4 && xmlhttp.status==200) 
			{
				var xmlDoc=xmlhttp.responseXML; 
				var mode = parseInt(xmlDoc.getElementsByTagName('mode')[0].childNodes[0].textContent);
				SetupUIForMode(mode);
				if(mode == 1) // Bridge Mode
				{
					$("#selFPPDmode").prop("selectedIndex",3);
					$("#textFPPDmode").text("Bridge");
				} else if (mode == 8) { // Remote Mode
					$("#selFPPDmode").prop("selectedIndex",2);
					$("#textFPPDmode").text("Player (Remote)");
				} else { // Player or Master modes
					if (mode == 2) { // Player
						$("#selFPPDmode").prop("selectedIndex",0);
						$("#textFPPDmode").text("Player (Standalone)");
					} else {
						$("#selFPPDmode").prop("selectedIndex",1);
						$("#textFPPDmode").text("Player (Master)");
					}
				}
			}
		};
		xmlhttp.send();
}

function DisplayHelp()
{
	$('#helpText').html("No help file exists for this page yet");
	$('#helpText').load(helpPage);
	$('#dialog-help').dialog({ height: 600, width: 800, title: "Help" });
	$('#dialog-help').dialog( "moveToTop" );
}

function GetGitOriginLog()
{
	$('#logText').html("Loading list of changes from github.");
	$('#logText').load("fppxml.php?command=getGitOriginLog");
	$('#logViewer').dialog({ height: 600, width: 800, title: "Git Changes" });
	$('#logViewer').dialog( "moveToTop" );
}

function GetVideoInfo(file)
{
	$('#fileText').html("Getting Video Info.");
	$('#fileText').load("fppxml.php?command=getVideoInfo&filename=" + file);
	$('#fileViewer').dialog({ height: 600, width: 800, title: "Video Info" });
	$('#fileViewer').dialog( "moveToTop" );
}

function PlayFileInBrowser(dir, file)
{
	location.href="fppxml.php?command=getFile&play=1&dir=" + dir + "&filename=" + file;
}

function CopyFile(dir, file)
{
	var newFile = prompt("New Filename:", file);

	var postData = "command=copyFile&dir=" + dir + "&filename=" + encodeURIComponent(file) + "&newfilename=" + encodeURIComponent(newFile);

	$.post("fppjson.php", postData).done(function(data) {
		if (data.status == 'success')
			GetFiles(dir);
		else
			$.jGrowl("Error: File Copy failed.");
	}).fail(function() {
		$.jGrowl("Error: File Copy failed.");
	});
}

function RenameFile(dir, file)
{
	var newFile = prompt("New Filename:", file);

	var postData = "command=renameFile&dir=" + dir + "&filename=" + encodeURIComponent(file) + "&newfilename=" + encodeURIComponent(newFile);

	$.post("fppjson.php", postData).done(function(data) {
		if (data.status == 'success')
			GetFiles(dir);
		else
			$.jGrowl("Error: File Rename failed.");
	}).fail(function() {
		$.jGrowl("Error: File Rename failed.");
	});
}

function DownloadFile(dir, file)
{
	location.href="fppxml.php?command=getFile&dir=" + dir + "&filename=" + file;
}

function DownloadZip(dir)
{
	location.href="fppxml.php?command=getZip&dir=" + dir;
}

function ViewImage(file)
{
	var url = "fppxml.php?command=getFile&dir=Images&filename=" + file + '&attach=0';
	window.open(url, '_blank');
}

function ViewFile(dir, file)
{
	$('#fileText').html("Loading...");
	$.get("fppxml.php?command=getFile&dir=" + dir + "&filename=" + file, function(text) {
		var ext = file.split('.').pop();
		if (ext != "html")
			$('#fileText').html("<pre>" + text + "</pre>");
	});

	$('#fileViewer').dialog({ height: 600, width: 800, title: "File Viewer: " + file });
	$('#fileViewer').dialog( "moveToTop" );
}

function DeleteFile(dir, file)
{
	if (file.indexOf("/") > -1)
	{
		alert("You can not delete this file.");
		return;
	}

	var xmlhttp=new XMLHttpRequest();
	var url = "fppxml.php?command=deleteFile&dir=" + dir + "&filename=" + encodeURIComponent(file);
	xmlhttp.open("GET",url,false);
	xmlhttp.setRequestHeader('Content-Type', 'text/xml');
	 
	xmlhttp.onreadystatechange = function () {
		if (xmlhttp.readyState == 4 && xmlhttp.status==200)
		{
			GetFiles(dir);
		}
	};
	xmlhttp.send();
}

function SaveUSBDongleSettings()
{
	var usbDonglePort = $('#USBDonglePort').val();
	var usbDongleType = $('#USBDongleType').val();
	var usbDongleBaud = $('#USBDongleBaud').val();

	var xmlhttp=new XMLHttpRequest();
	var url = "fppxml.php?command=saveUSBDongle&port=" + usbDonglePort +
				"&type=" + usbDongleType +
				"&baud=" + usbDongleBaud;
	xmlhttp.open("GET",url,false);
	xmlhttp.setRequestHeader('Content-Type', 'text/xml');

	xmlhttp.onreadystatechange = function () {
		if (xmlhttp.readyState == 4 && xmlhttp.status==200) 
		{
			alert("You must restart FPPD for changes to take effect.");
		}
	};
	xmlhttp.send();
}

function SetupSelectableTableRow(info)
{
	$('#' + info.tableName + ' > tbody').on('mousedown', 'tr', function(event,ui){
		$('#' + info.tableName + ' > tbody > tr').removeClass('fppTableSelectedEntry');
		$(this).addClass('fppTableSelectedEntry');
		var items = $('#' + info.tableName + ' > tbody > tr');
		info.selected  = items.index(this);

		for (var i = 0; i < info.enableButtons.length; i++) {
			SetButtonState('#' + info.enableButtons[i], "enable");
		}
		for (var i = 0; i < info.disableButtons.length; i++) {
			SetButtonState('#' + info.disableButtons[i], "disable");
		}
	});
}

function DialogOK(title, message)
{
	$('#dialog-popup').dialog({
		mocal: true,
		height: 'auto',
		width: 400,
		autoResize: true,
		closeOnEscape: false,
		buttons: {
			Ok: function() {
				$(this).dialog("close");
			}
		}
	});
	$('#dialog-popup').dialog('option', 'title', title);
	$('#dialog-popup').html("<center>" + message + "</center>");
	$('#dialog-popup').show();
	$('#dialog-popup').draggable();
}

// Simple wrapper for now, but we may highlight this somehow later
function DialogError(title, message)
{
	DialogOK(title, message);
}

// page visibility prefixing
function getHiddenProp(){
    var prefixes = ['webkit','moz','ms','o'];
    
    // if 'hidden' is natively supported just return it
    if ('hidden' in document) return 'hidden';
    
    // otherwise loop over all the known prefixes until we find one
    for (var i = 0; i < prefixes.length; i++){
        if ((prefixes[i] + 'Hidden') in document) 
            return prefixes[i] + 'Hidden';
    }

    // otherwise it's not supported
    return null;
}

// return page visibility
function isHidden() {
    var prop = getHiddenProp();
    if (!prop) return false;
    
    return document[prop];
}

function bindVisibilityListener() {
	var visProp = getHiddenProp();
	if (visProp) {
	  var evtname = visProp.replace(/[H|h]idden/,'') + 'visibilitychange';
	  document.addEventListener(evtname, handleVisibilityChange);
	}
}

function handleVisibilityChange() {

    if (isHidden() && statusTimeout != null) {
        clearTimeout(statusTimeout);
        statusTimeout = null;
    } else {
         GetFPPStatus();
    }
   
}

