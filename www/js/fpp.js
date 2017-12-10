
STATUS_IDLE = "0";
STATUS_PLAYING = "1";
STATUS_STOPPING_GRACEFULLY = "2";


// Globals
gblCurrentPlaylistIndex = 0;
gblCurrentLoadedPlaylist  = '';
gblCurrentLoadedPlaylistCount = 0;

var statusTimeout = null;
var lastStatus = '';

function PopulateLists() {
	PlaylistTypeChanged();
	PopulatePlaylists("playList");
	var firstPlaylist = $('#playlist0').html();
	if (firstPlaylist != undefined)
		PopulatePlayListEntries(firstPlaylist,true);
}

function PopulatePlaylists(element) {
	var xmlhttp=new XMLHttpRequest();
	var url = "fppxml.php?command=getPlayLists";
	
	xmlhttp.open("GET",url,false);
	xmlhttp.setRequestHeader('Content-Type', 'text/xml');
	var Filename;
 
	xmlhttp.onreadystatechange = function () {
		if (xmlhttp.readyState == 4 && xmlhttp.status==200) 
		{
			var xmlDoc=xmlhttp.responseXML; 
	
			var productList = xmlDoc.getElementsByTagName('Playlists')[0];
			var innerHTML = "<ol>";
			if(productList.childNodes.length> 0)
			{
				for(i=0;i<productList.childNodes.length;i++)
					{
						Filename = productList.childNodes[i].textContent;
						// Remove extension
						//Filename = Filename.substr(0, x.lastIndexOf('.'));	
						innerHTML += "<li><a href='#' id=playlist" + i.toString() + " onclick=\"PopulatePlayListEntries('" + Filename + "',true)\">" + Filename + "</a></li>";
					}
					innerHTML += "</ol>";
			}
			else
			{
				innerHTML = "No Results Found";	
			}
			var results = document.getElementById(element);
			results.innerHTML = innerHTML;	
		}
	};
	xmlhttp.send();
}


function GetPlayListSettings(playList) {
	var xmlhttp=new XMLHttpRequest();
		var url = "fppxml.php?command=getPlayListSettings&pl=" + playList;
		xmlhttp.open("GET",url,true);
		xmlhttp.setRequestHeader('Content-Type', 'text/xml');
 
		xmlhttp.onreadystatechange = function () {
		if (xmlhttp.readyState == 4 && xmlhttp.status==200) 
		{
				var xmlDoc=xmlhttp.responseXML; 
				var entries = xmlDoc.getElementsByTagName('playlist_settings')[0];
				var first = entries.childNodes[0].textContent;
				var last = entries.childNodes[1].textContent;
				if(first == "1")
				{
					$("#chkFirst").prop( "checked", true );
					$("#firstLast0").html("First");
				}
				else
				{
					$("#chkFirst").prop( "checked", false );
				}
				if(last == "1")
				{
					$("#chkLast").prop( "checked", true );
					$("#firstLast" + (gblCurrentLoadedPlaylistCount-1).toString()).html("Last");
				}
				else
				{
					$("#chkLast").prop( "checked", false );
				}
		}
	};
	xmlhttp.send();
}

function GetStatusPlayListSettings(playList) {
	var xmlhttp=new XMLHttpRequest();
		var url = "fppxml.php?command=getPlayListSettings&pl=" + playList;
		xmlhttp.open("GET",url,true);
		xmlhttp.setRequestHeader('Content-Type', 'text/xml');
 
		xmlhttp.onreadystatechange = function () {
		if (xmlhttp.readyState == 4 && xmlhttp.status==200) 
		{
				var xmlDoc=xmlhttp.responseXML; 
				var entries = xmlDoc.getElementsByTagName('playlist_settings')[0];
				var first = entries.childNodes[0].textContent;
				var last = entries.childNodes[1].textContent;
				if(first == "1")
				{
					$("#firstLast0").html("First");
				}
				else
				{
					$("#chkFirst").prop( "checked", false );
				}
				if(last == "1")
				{
					$("#firstLast" + (gblCurrentLoadedPlaylistCount-1).toString()).html("Last");
				}
				else
				{
					$("#chkLast").prop( "checked", false );
				}
		}
	};
	xmlhttp.send();
}

function SetPlayListFirst()
{
    var xmlhttp=new XMLHttpRequest();
	var first = $("#chkFirst").is(':checked')?'1':'0';
	var last = $("#chkLast").is(':checked')?'1':'0';
	var playlist = $("#txtPlaylistName").val();
	if(first == 1 && last ==1 && gblCurrentLoadedPlaylistCount < 3)
	{
		$("#chkFirst").prop( "checked", false );
		first =0;
		alert("A minimum of 3 entries are required for both 'First' and 'Last' options");
	}
	else if(first == 1 && last == 0 && gblCurrentLoadedPlaylistCount < 2)
	{
		$("#chkFirst").prop( "checked", false );
		first =0;
		alert("A minimum of 2 entries are required for 'First' option");
	}
	var url = "fppxml.php?command=setPlayListFirstLast&first=" + first + "&last="+last;
	xmlhttp.open("GET",url,true);
	xmlhttp.setRequestHeader('Content-Type', 'text/xml');
	xmlhttp.send();
	PopulatePlayListEntries(playlist,false);
}

function SetPlayListLast()
{
    	var xmlhttp=new XMLHttpRequest();
			var first = $("#chkFirst").is(':checked')?'1':'0';
			var last = $("#chkLast").is(':checked')?'1':'0';
			var playlist = $("#txtPlaylistName").val();
			if(first == 1 && last ==1 && gblCurrentLoadedPlaylistCount < 3)
			{
				$("#chkLast").prop( "checked", false );
				last =0;
				alert("A minimum of 3 entries are required for both 'First' and 'Last' options");
			}
			else if(last == 1 && first == 0 && gblCurrentLoadedPlaylistCount < 2)
			{
				$("#chkLast").prop( "checked", false );
				last =0;
				alert("A minimum of 2 entries are required for 'Last' option");
			}
			var url = "fppxml.php?command=setPlayListFirstLast&first=" + first + "&last="+last;
			xmlhttp.open("GET",url,true);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
			xmlhttp.send();
			PopulatePlayListEntries(playlist,false);
}

function CheckFirstLastOptions()
{
    	var xmlhttp=new XMLHttpRequest();
			var first = $("#chkFirst").is(':checked')?'1':'0';
			var last = $("#chkLast").is(':checked')?'1':'0';
			var playlist = $("#txtPlaylistName").val();
			if(first == 1 && last ==1 && gblCurrentLoadedPlaylistCount < 4)
			{
				$("#chkLast").prop( "checked", false );
				$("#chkFirst").prop( "checked", false );
				last =0;
				first =0;
				alert("A minimum of 3 entries are required for both 'First' and 'Last' options");
			}
			else if(last == 1 && first == 0 && gblCurrentLoadedPlaylistCount < 3)
			{
				$("#chkLast").prop( "checked", false );
				last =0;
				alert("A minimum of 2 entries are required for 'First' option");
			}
			else if(last == 0 && first == 1 && gblCurrentLoadedPlaylistCount < 3)
			{
				$("#chkLast").prop( "checked", false );
				first =0;
				alert("A minimum of 2 entries are required for 'Last' option");
			}
			var url = "fppxml.php?command=setPlayListFirstLast&first=" + first + "&last="+last;
			xmlhttp.open("GET",url,true);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
			xmlhttp.send();
			PopulatePlayListEntries(playlist,false);
}

function GetPlaylistRowHTML(ID, type, data1, data2, firstlast)
{
	var HTML = "";

	HTML += "<tr id=\"playlistRow" + ID + "\">";
	HTML += "<td class=\"colPlaylistNumber colPlaylistNumberDrag\" id = \"colEntryNumber" + ID + "\" >" + ID + ".</td>";
	HTML += "<td class=\"colPlaylistType\">" + type + "</td>";
	HTML += "<td class=\"colPlaylistData1\">" + data1 + "</td>";
	HTML += "<td class=\"colPlaylistData2\">" + data2 + "</td>"
	HTML += "<td class=\"colPlaylistFlags\" id=\"firstLast" + firstlast + "\">&nbsp;</td>";
	HTML += "</tr>";

	return HTML;
}

function PopulatePlayListEntries(playList,reloadFile,selectedRow) {
	if ( ! playList ) {
		$('#txtPlaylistName').val(playList);

		var innerHTML="";
		innerHTML +=  "<tr class=\"playlistPlayingEntry\">";
		innerHTML +=  "<td>No entries in playlist.</td>";
		innerHTML += "</tr>";
		$('#tblCreatePlaylistEntries_tbody').html(innerHTML);

		return false;
	}

	var type;
	var pl;
	var mediaFile;
	var seqFile;
	var pause;
var xmlhttp=new XMLHttpRequest();
	var innerHTML="";
	var url = "fppxml.php?command=getPlayListEntries&pl=" + playList + "&reload=" + reloadFile;
	xmlhttp.open("GET",url,true);
	xmlhttp.setRequestHeader('Content-Type', 'text/xml');

	xmlhttp.onreadystatechange = function () {
		if (xmlhttp.readyState == 4 && xmlhttp.status==200) 
		{
			var xmlDoc=xmlhttp.responseXML; 
			var entries = xmlDoc.getElementsByTagName('PlaylistEntries')[0];
			
			gblCurrentLoadedPlaylist = playList;
			gblCurrentLoadedPlaylistCount = entries.childNodes.length;
			$('#txtPlaylistName').val(playList);
			
			if(entries.childNodes.length> 0)	
			{
					for(i=0;i<entries.childNodes.length;i++)
					{
						var type = entries.childNodes[i].childNodes[0].textContent;
						var seqFile = entries.childNodes[i].childNodes[1].textContent;
						var mediaFile = entries.childNodes[i].childNodes[2].textContent;
						var pause = entries.childNodes[i].childNodes[3].textContent;
						var eventName = entries.childNodes[i].childNodes[5].textContent;
						var eventID = entries.childNodes[i].childNodes[6].textContent.replace('_', ' / ');
						var pluginData = entries.childNodes[i].childNodes[7].textContent;
						if(type == 'b')
								innerHTML += GetPlaylistRowHTML((i+1).toString(), "Seq/Med", mediaFile, seqFile, i.toString());
						else if(type == 'm')
								innerHTML += GetPlaylistRowHTML((i+1).toString(), "Media", mediaFile, "---", i.toString());
						else if(type == 's')
								innerHTML += GetPlaylistRowHTML((i+1).toString(), "Seq.", "---", seqFile, i.toString());
						else if(type == 'p')
								innerHTML += GetPlaylistRowHTML((i+1).toString(), "Pause", "PAUSE - " + pause.toString(), "---", i.toString());
						else if(type == 'e')
						{
								innerHTML += GetPlaylistRowHTML((i+1).toString(), "Event", eventID + " - " + eventName, "---", i.toString());
						}
						else if(type == 'P')
								innerHTML += GetPlaylistRowHTML((i+1).toString(), "Plugin", "---", pluginData.toString());
					}
			}
			else
			{
								innerHTML +=  "<tr class=\"playlistPlayingEntry\">";
								innerHTML +=  "<td>No entries in playlist.</td>";
							  innerHTML += "</tr>";
			}
			$('#tblCreatePlaylistEntries_tbody').html(innerHTML);
			GetPlayListSettings(playList);
		}
	}
	xmlhttp.send();
}
		
function PlaylistTypeChanged() {
	var type=document.getElementById("selType").selectedIndex;
	switch(type)
	{
		case 0: // Music and Sequence
			$("#musicOptions").show();
			$("#sequenceOptions").show();
			$("#autoSelectWrapper").show();
			$("#autoSelectMatches").prop('checked', true);
			$("#videoOptions").hide();
			$("#eventOptions").hide();
			$("#pauseTime").hide();
			$("#pauseText").hide();
			$("#delayText").hide();
			$("#pluginData").hide();
			break;
		case 1:	// Media Only
			$("#musicOptions").show();
			$("#sequenceOptions").hide();
			$("#autoSelectWrapper").hide();
			$("#videoOptions").hide();
			$("#eventOptions").hide();
			$("#pauseTime").hide();
			$("#pauseText").hide();
			$("#delayText").hide();
			$("#pluginData").hide();
			break;
		case 2:	// Sequence Only
			$("#musicOptions").hide();
			$("#sequenceOptions").show();
			$("#autoSelectWrapper").hide();
			$("#videoOptions").hide();
			$("#eventOptions").hide();
			$("#pauseTime").hide();
			$("#pauseText").show();
			$("#delayText").hide();
			$("#pluginData").hide();
			break;
		case 3:	// Pause
			$("#musicOptions").hide();
			$("#sequenceOptions").hide();
			$("#autoSelectWrapper").hide();
			$("#videoOptions").hide();
			$("#eventOptions").hide();
			$("#pauseTime").show();
			$("#pauseText").show();
			$("#delayText").hide();
			$("#pluginData").hide();
			break;
		case 4: // Event
			$("#musicOptions").hide();
			$("#sequenceOptions").hide();
			$("#autoSelectWrapper").hide();
			$("#videoOptions").hide();
			$("#eventOptions").show();
			$("#pauseTime").hide();
			$("#pauseText").hide();
			$("#delayText").hide();
			$("#pluginData").hide();
			break;
		case 5: // Plugin
			$("#musicOptions").hide();
			$("#sequenceOptions").hide();
			$("#autoSelectWrapper").hide();
			$("#videoOptions").hide();
			$("#eventOptions").hide();
			$("#pauseTime").hide();
			$("#pauseText").hide();
			$("#delayText").hide();
			$("#pluginData").show();
			break;
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
			PopulatePlaylists('playList');
			PopulatePlayListEntries(plName,true);
			$('#txtName').val(plName);
			$('#txtName').focus()
			$('#txtName').select()
			
		}
	};
	
	xmlhttp.send();

}
		
function PlaylistEntryIndexChanged(newIndex,oldIndex) {
	var xmlhttp=new XMLHttpRequest();
	var url = "fppxml.php?command=sort&newIndex=" + newIndex + "&oldIndex=" + oldIndex;
	xmlhttp.open("GET",url,false);
	xmlhttp.setRequestHeader('Content-Type', 'text/xml');

	xmlhttp.onreadystatechange = function () {
		if (xmlhttp.readyState == 4 && xmlhttp.status==200) 
		{
//					var xmlDoc=xmlhttp.responseXML; 
//					var productList = xmlDoc.getElementsByTagName('Music')[0];
//					PopulatePlaylists('playList');
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

			if ((type == "b") &&
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
			var	pause = document.getElementById("txtPause").value;
			var url = "fppxml.php?command=addPlaylistEntry&type=" + type +
								"&seqFile=" + encodeURIComponent(seqFile) +
								"&mediaFile=" + encodeURIComponent(mediaFile) +
								"&pause=" + pause +
								"&eventID=" + encodeURIComponent(eventID) + 
								"&eventName=" + encodeURIComponent(eventName) +
								"&pluginData=" + encodeURIComponent(pluginData);
			xmlhttp.open("GET",url,false);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
	 
			xmlhttp.onreadystatechange = function () {
				if (xmlhttp.readyState == 4 && xmlhttp.status==200) 
				{
					var xmlDoc=xmlhttp.responseXML; 
          PopulatePlayListEntries(name.value,false);
				}
			};
			
			xmlhttp.send();

		}
		
function SavePlaylist()	{
	var name=document.getElementById("txtPlaylistName");
    var xmlhttp=new XMLHttpRequest();
	var first = $("#chkFirst").is(':checked')?'1':'0';
	var last = $("#chkLast").is(':checked')?'1':'0';
	var url = "fppxml.php?command=savePlaylist&name=" + name.value + "&first=" + first + "&last="+last;
	xmlhttp.open("GET",url,false);
	xmlhttp.setRequestHeader('Content-Type', 'text/xml');

	xmlhttp.onreadystatechange = function () {
		if (xmlhttp.readyState == 4 && xmlhttp.status==200) {
			var xmlDoc=xmlhttp.responseXML; 
  			PopulatePlaylists("playList");
  			PopulatePlayListEntries(name.value,true);
		}
	};
	
	xmlhttp.send();

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
          PopulatePlaylists("playList");
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
			var name=document.getElementById("txtPlaylistName");
    	var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=deleteEntry&index=" + lastPlaylistEntry;
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
			CheckFirstLastOptions();
		}

		function updatePlugin(pluginName)
		{
			var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=updatePlugin&plugin=" + pluginName;
			xmlhttp.open("GET",url,false);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
			xmlhttp.onreadystatechange = function () {
				if (xmlhttp.readyState == 4)
					location.reload(true);
			}
			xmlhttp.send();
		}

		function uninstallPlugin(pluginName)
		{
			var opts = {
				lines: 9, // The number of lines to draw
				length: 25, // The length of each line
				width: 10, // The line thickness
				radius: 25, // The radius of the inner circle
				corners: 1, // Corner roundness (0..1)
				rotate: 0, // The rotation offset
				direction: 1, // 1: clockwise, -1: counterclockwise
				color: '#fff', // #rgb or #rrggbb or array of colors
				speed: 1, // Rounds per second
				trail: 60, // Afterglow percentage
				shadow: false, // Whether to render a shadow
			};

			var target = document.getElementById('overlay');
			var spinner = new Spinner(opts).spin(target);

			target.style.display = 'block';
			document.body.style.cursor = "wait";

			$.get("fppxml.php?command=uninstallPlugin&plugin=" + pluginName
			).success(function() {
				document.body.style.cursor = "pointer";
				location.reload(true);
			}).fail(function() {
				document.body.style.cursor = "pointer";
				DialogError("Failed to install plugin", "Install failed");
			});
		}

		function installPlugin(pluginName)
		{
			var opts = {
				lines: 9, // The number of lines to draw
				length: 25, // The length of each line
				width: 10, // The line thickness
				radius: 25, // The radius of the inner circle
				corners: 1, // Corner roundness (0..1)
				rotate: 0, // The rotation offset
				direction: 1, // 1: clockwise, -1: counterclockwise
				color: '#fff', // #rgb or #rrggbb or array of colors
				speed: 1, // Rounds per second
				trail: 60, // Afterglow percentage
				shadow: false, // Whether to render a shadow
			};

			var target = document.getElementById('overlay');
			var spinner = new Spinner(opts).spin(target);

			target.style.display = 'block';
			document.body.style.cursor = "wait";

			$.get("fppxml.php?command=installPlugin&plugin=" + pluginName
			).success(function() {
				document.body.style.cursor = "pointer";
				location.reload(true);
			}).fail(function() {
				document.body.style.cursor = "pointer";
				DialogError("Failed to install plugin", "Install failed");
			});
		}

		function ManualGitUpdate()
		{
			SetButtonState("#ManualUpdate", "disable");
			document.body.style.cursor = "wait";

			$.get("fppxml.php?command=manualGitUpdate"
			).success(function() {
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
				).success(function(data) {
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
				).success(function(data) {
						$('#helpText').html(
						"<center><input onClick='UpgradeFPPVersion(\"" + version + "\");' type='button' class='buttons' value='Upgrade'></center>" +
						"<pre>" + data + "</pre>"
						);
				}).fail(function() {
						$('#helpText').html("Error loading release notes.");
				});
		}

		function UpgradeFPPVersion(newVersion)
		{
			if (confirm('Do you wish to upgrade the Falcon Player?\n\nClick "OK" to continue.\n\nThe system will automatically reboot to complete the upgrade.'))
			{
				document.body.style.cursor = "wait";
				$.get("fppxml.php?command=upgradeFPPVersion&version=v" + newVersion
				).success(function() {
					document.body.style.cursor = "pointer";
					location.reload(true);
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
			).success(function() {
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
		
		function SetUniverseCount()
		{
			var txtCount=document.getElementById("txtUniverseCount");
			var count = Number(txtCount.value);
			if(isNaN(count))
			{
				count = 8;
			}
			UniverseCount = count;
			
    	var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=setUniverseCount&count=" + count;
			xmlhttp.open("GET",url,false);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
	 
			xmlhttp.onreadystatechange = function () {
				if (xmlhttp.readyState == 4 && xmlhttp.status==200) 
				{
					var xmlDoc=xmlhttp.responseXML; 
          getUniverses("FALSE");
				}
			};
			
			xmlhttp.send();
		}
		
		function getUniverses(reload)
		{
    	var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=getUniverses&reload=" + reload;
			xmlhttp.open("GET",url,false);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
 			var innerHTML="";
			UniverseCount = 0;

			xmlhttp.onreadystatechange = function () {
				if (xmlhttp.readyState == 4 && xmlhttp.status==200) 
				{
					var xmlDoc=xmlhttp.responseXML; 
					var entries = xmlDoc.getElementsByTagName('UniverseEntries')[0];
					if(entries.childNodes.length> 0)
					{
						innerHTML = "<tr class=\"tblheader\">" +  
												"<td width=\"5%\" align='left'>Line<br>#</td>" +
												"<td width=\"10%\" align='left'>Universe Active</td>" +
												"<td width=\"10%\" align='left'>FPP Start<br>Channel</td>" +
												"<td width=\"10%\" align='left'>Universe<br>#</td>" +
												"<td width=\"10%\" align='left'>Universe<br>Size</td>" +
                        "<td width=\"20%\" align='left'>Universe<br>Type</td>" +
												"<td width=\"20%\" align='left'>Unicast Address</td>" +
												"<td width=\"5%\" align='left'>Ping</td>" +
												"</tr>";
												
							UniverseCount = entries.childNodes.length;
							document.getElementById("txtUniverseCount").value = UniverseCount.toString();
							for(i=0;i<UniverseCount;i++)
							{
								var active = entries.childNodes[i].childNodes[0].textContent;
								var universe = entries.childNodes[i].childNodes[1].textContent;
								var startAddress = entries.childNodes[i].childNodes[2].textContent;
								var size = entries.childNodes[i].childNodes[3].textContent;
								var type = entries.childNodes[i].childNodes[4].textContent;
								var unicastAddress =  entries.childNodes[i].childNodes[5].textContent;
								unicastAddress = unicastAddress.trim();

								var activeChecked = active == 1  ? "checked=\"checked\"" : "";
								var typeMulticastE131 = type == 0 ? "selected" : "";
								var typeUnicastE131 = type == 1 ? "selected": "";
								var typeBroadcastArtNet = type == 2 ? "selected" : "";
								var typeUnicastArtNet = type == 3 ? "selected": "";
								
								innerHTML += 	"<tr class=\"rowUniverseDetails\">" +
								              "<td>" + (i+1).toString() + "</td>" +
															"<td><input name=\"chkActive[" + i.toString() + "]\" id=\"chkActive[" + i.toString() + "]\" type=\"checkbox\" " + activeChecked +"/></td>" +
															"<td><input name=\"txtStartAddress[" + i.toString() + "]\" id=\"txtStartAddress[" + i.toString() + "]\" type=\"text\" size=\"6\" maxlength=\"6\" value=\"" + startAddress.toString() + "\"/></td>" +
															"<td><input name=\"txtUniverse[" + i.toString() + "]\" id=\"txtUniverse[" + i.toString() + "]\" type=\"text\" size=\"5\" maxlength=\"5\" value=\"" + universe.toString() + "\"/></td>" +
															"<td><input name=\"txtSize[" + i.toString() + "]\" id=\"txtSize[" + i.toString() + "]\" type=\"text\"  size=\"3\"/  maxlength=\"3\"value=\"" + size.toString() + "\"></td>" +
															
															"<td><select id=\"universeType[" + i.toString() + "]\" name=\"universeType[" + i.toString() + "]\" style=\"width:150px\">" +
															      "<option value=\"0\" " + typeMulticastE131 + ">E1.31 - Multicast</option>" +
															      "<option value=\"1\" " + typeUnicastE131 + ">E1.31 - Unicast</option>" +
															      "<option value=\"2\" " + typeBroadcastArtNet + ">ArtNet - Broadcast</option>" +
															      "<option value=\"3\" " + typeUnicastArtNet + ">ArtNet - Unicast</option>" +
																  "</select></td>" +
															"<td><input name=\"txtIP[" + i.toString() + "]\" id=\"txtIP[" + i.toString() + "]\" type=\"text\"/ value=\"" + unicastAddress + "\" size=\"15\" maxlength=\"32\"></td>" +
															"<td><input type=button onClick='PingE131IP(" + i.toString() + ");' value='Ping'></td>" +
															"</tr>";

							}
					}
					else
					{
						innerHTML = "No Results Found";	
					}
					var results = document.getElementById("tblUniverses");
					results.innerHTML = innerHTML;	
				}
			};
			
			xmlhttp.send();			
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
		
		function InitializeUniverses()
		{
			UniverseSelected =0;
			UniverseCount=0;	
		}
		
		function DeleteUniverse()
		{
    	var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=deleteUniverse&index=" + (UniverseSelected-1).toString();
			xmlhttp.open("GET",url,false);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
	 
			xmlhttp.onreadystatechange = function () {
				if (xmlhttp.readyState == 4 && xmlhttp.status==200) 
				{
					var xmlDoc=xmlhttp.responseXML; 
          getUniverses("FALSE");
				}
			};
			
			xmlhttp.send();
		}
		
		function CloneUniverse()
		{
			var answer = prompt ("How many universes to clone from selected universe?","1");
			var cloneNumber = Number(answer);
			var selectIndex = (UniverseSelected-1).toString();
			if(!isNaN(cloneNumber))
			{
				if((UniverseSelected + cloneNumber -1) < UniverseCount)
				{
					var universe=Number(document.getElementById("txtUniverse[" + selectIndex + "]").value)+1;
					var universeType=document.getElementById("universeType[" + selectIndex + "]").value;
					var unicastAddress=document.getElementById("txtIP[" + selectIndex + "]").value;
					var size=Number(document.getElementById("txtSize[" + selectIndex + "]").value);
					var startAddress=Number(document.getElementById("txtStartAddress[" + selectIndex + "]").value)+ size;
					var active=document.getElementById("chkActive[" + selectIndex + "]").value;

					for(i=UniverseSelected;i<UniverseSelected+cloneNumber;i++,universe++)
					{
						document.getElementById("txtUniverse[" + i + "]").value	 = universe.toString();
						document.getElementById("universeType[" + i + "]").value	 = universeType;
						document.getElementById("txtStartAddress[" + i + "]").value	 = startAddress.toString();
						document.getElementById("chkActive[" + i + "]").value = active;
						document.getElementById("txtSize[" + i + "]").value = size.toString();
						document.getElementById("txtIP[" + i + "]").value = unicastAddress;
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
					$.jGrowl("" + cloneNumber + " Universes Cloned");
				}
			} else {
				DialogError("Clone Universe", "Error, invalid number");
			}
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

		
		function validateUniverseData()
		{
			var i;
			var txtUniverse;
			var txtStartAddress;
			var txtSize;
			var universeType;
			var result;
			var returnValue=true;
			for(i=0;i<UniverseCount;i++)
			{

				// universe
				txtUniverse=document.getElementById("txtUniverse[" + i + "]");				
				if(!validateNumber(txtUniverse,1,63999))
				{
					returnValue = false;
				}
				// start address
				txtStartAddress=document.getElementById("txtStartAddress[" + i + "]");				
				if(!validateNumber(txtStartAddress,1,524288))
				{
					returnValue = false;
				}
				// size
				txtSize=document.getElementById("txtSize[" + i + "]");				
				if(!validateNumber(txtSize,1,512))
				{
					returnValue = false;
				}
				// unicast address
				universeType=document.getElementById("universeType[" + i + "]").value;
				if(universeType == 1 || universeType == 3)
				{
					if(!validateIPaddress("txtIP[" + i + "]"))
					{
						returnValue = false;
					}
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
			}
		}
		
		function ReloadUniverses()
		{
			getUniverses("TRUE");	
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
                        "<td width=\"3%\">#</td>" +
                        "<td width=\"7%\">Enable</td>" +
                        "<td width=\"10%\">First &amp; Last<br>Run Dates</td>" +
												"<td width=\"42%\">Playlist</td>" +
												"<td width=\"15%\">Day(s)</td>" +
												"<td width=\"15%\">Start &amp; End<br>Times</td>" +
                        "<td width=\"8%\">Repeat</td>" +
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
															"<td><input class='date center'  name=\"txtStartDate[" + i.toString() + "]\" id=\"txtStartDate[" + i.toString() + "]\" type=\"text\" size=\"10\" value=\"" + startDate + "\"/><br/>" +
																"<input class='date center'  name=\"txtEndDate[" + i.toString() + "]\" id=\"txtEndDate[" + i.toString() + "]\" type=\"text\" size=\"10\" value=\"" + endDate + "\"/></td>" +

															"<td><select id=\"selPlaylist[" + i.toString() + "]\" name=\"selPlaylist[" + i.toString() + "]\">" +
															playlistOptionsText + "</select><br>" +
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
															      "<option value=\"65536\" " + dayChecked_0x10000 + ">Day Mask</option></select><br>" +
																  "</td>" +
															"<td><input class='time center'  name=\"txtStartTime[" + i.toString() + "]\" id=\"txtStartTime[" + i.toString() + "]\" type=\"text\" size=\"8\" value=\"" + startTime + "\"/><br/>" +
															"<input class='time center' name=\"txtEndTime[" + i.toString() + "]\" id=\"txtEndTime[" + i.toString() + "]\" type=\"text\" size=\"8\" value=\"" + endTime + "\"/></td>" +
															"<td class='center' ><input name=\"chkRepeat[" + i.toString() + "]\" id=\"chkEnable[" + i.toString() + "]\" type=\"checkbox\" " + repeatChecked +"/></td>" +
															"</tr>";
															
									$('#tblSchedule').append(tableRow);
									$('.time').timepicker({'timeFormat': 'H:i:s', 'typeaheadHighlight': false});
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
        var tableRow = "<tr class ='fileDetails'><td class ='fileName'>" + name + "</td><td class ='fileTime'>" + time + "</td></tr>";
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
		var status = GetFPPstatus();
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

	function parseStatus(jsonStatus) {
		var fppStatus = jsonStatus.status;
		var fppMode = jsonStatus.mode;
		var status = "Idle";

		if (fppStatus == STATUS_IDLE ||
			fppStatus == STATUS_PLAYING ||
			fppStatus == STATUS_STOPPING_GRACEFULLY ) {
		
			$("#btnDaemonControl").attr('value', 'Stop FPPD');
			$('#daemonStatus').html("FPPD is running.");
		}

		SetupUIForMode(fppMode);

		if (fppMode == 1) {
			// Bridge Mode
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

			if(fppStatus == STATUS_IDLE) {
			
				gblCurrentPlaylistIndex =0;
				$('#txtPlayerStatus').html("Idle");
				$('#txtTimePlayed').html("");								
				$('#txtTimeRemaining').html("");	
								
				// Enable Play
				SetButtonState('#btnPlay','enable');
				SetButtonState('#btnStopNow','disable');
				SetButtonState('#btnStopGracefully','disable');
				$('#selStartPlaylist').removeAttr("disabled");
				UpdateCurrentEntryPlaying(0);
			
			} else if (currentPlaylist.playlist != "") {
				var playerStatusText = "Playing <strong>'" + currentPlaylist.playlist + "'</strong>";
                var repeatMode = jsonStatus.repeat_mode;
				if(gblCurrentLoadedPlaylist != currentPlaylist.playlist)	{
					$('#selStartPlaylist').val(currentPlaylist.playlist);
					PopulateStatusPlaylistEntries(false,currentPlaylist.playlist,true);
				}

				SetButtonState('#btnPlay','disable');
				SetButtonState('#btnStopNow','enable');
				SetButtonState('#btnStopGracefully','enable');
				$('#selStartPlaylist').attr("disabled");

				if(fppStatus == STATUS_STOPPING_GRACEFULLY) {
					playerStatusText += " - Stopping Gracefully";
				}

				$('#txtPlayerStatus').html(playerStatusText);
				$('#txtTimePlayed').html("Elapsed: " + jsonStatus.time_elapsed );				
				$('#txtTimeRemaining').html("Remaining: " + jsonStatus.time_remaining );	

				if(currentPlaylist.index != gblCurrentPlaylistIndex && 
					currentPlaylist.index <= gblCurrentLoadedPlaylistCount) {
							
							UpdateCurrentEntryPlaying(currentPlaylist.index);
							gblCurrentPlaylistIndex = currentPlaylist.index;
							
							if(currentPlaylist.index != 1) {
								var j=0;	
							}
					}

				if (repeatMode) {
					$("#chkRepeat").prop( "checked", true );
				} else{
					$("#chkRepeat").prop( "checked", false );
				}
			
			}

			$('#txtNextPlaylist').html(nextPlaylist.playlist);
			$('#nextPlaylistTime').html(nextPlaylist.start_time);
			$('#fppTime').html(jsonStatus.time);

		}
	
	}

	function GetFPPstatus()
	{
    	var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=getFPPstatus";
			xmlhttp.open("GET",url,true);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
	 
			xmlhttp.onreadystatechange = function () {
				if (xmlhttp.readyState == 4 && xmlhttp.status==200) 
				{
					var xmlDoc=xmlhttp.responseXML; 
					var status = xmlDoc.getElementsByTagName('Status')[0];
					if(status.childNodes.length> 1)
					{
						var fppStatus = status.childNodes[1].textContent;

						if (fppStatus == STATUS_IDLE ||
							fppStatus == STATUS_PLAYING ||
							fppStatus == STATUS_STOPPING_GRACEFULLY )
						{
							$("#btnDaemonControl").attr('value', 'Stop FPPD');
							$('#daemonStatus').html("FPPD is running.");
						}

						var fppMode = parseInt(status.childNodes[0].textContent);
						SetupUIForMode(fppMode);
						if (fppMode == 1)
						{
							// Bridge Mode
							GetUniverseBytesReceived();
						}
						else if (fppMode == 8)
						{
							// Remote Mode
							var Volume = status.childNodes[2].textContent;
							var seqFilename = status.childNodes[3].textContent;
							var mediaFilename = status.childNodes[4].textContent;
							var secsElapsed = parseInt(status.childNodes[5].textContent);
							var secsRemaining = parseInt(status.childNodes[6].textContent);
							var fppTime = status.childNodes[7].textContent;

							$('#fppTime').html(fppTime);

							var status = "Idle";
							if (secsElapsed)
							{
								var minsPlayed = Math.floor(secsElapsed/60);
								minsPlayed = isNaN(minsPlayed)?0:minsPlayed;
								var secsPlayed = secsElapsed%60;
								secsPlayed = isNaN(secsPlayed)?0:secsPlayed;

								var minsRemaining = Math.floor(secsRemaining/60);
								minsRemaining = isNaN(minsRemaining)?0:minsRemaining;
								var secsRemaining = secsRemaining%60;
								secsRemaining = isNaN(secsRemaining)?0:secsRemaining;
								status = "Syncing to Master: Elapsed: " + zeroPad(minsPlayed,2) + ":" + zeroPad(secsPlayed,2) + "&nbsp;&nbsp;&nbsp;&nbsp;Remaining: " + zeroPad(minsRemaining,2) + ":" + zeroPad(secsRemaining,2);
							}
							$('#txtRemoteStatus').html(status);
							$('#txtRemoteSeqFilename').html(seqFilename);
							$('#txtRemoteMediaFilename').html(mediaFilename);
						}
						else
						{
							// Player or Standalone Mode
							if(fppStatus == STATUS_IDLE)
							{
								gblCurrentPlaylistIndex =0;
								var Volume = status.childNodes[2].textContent;
								var NextPlaylist = status.childNodes[3].textContent;
								var NextPlaylistTime = status.childNodes[4].textContent;
								var fppTime = status.childNodes[5].textContent;
	
								$('#txtPlayerStatus').html("Idle");
								$('#txtTimePlayed').html("");								
								$('#txtTimeRemaining').html("");	
								$('#txtNextPlaylist').html(NextPlaylist);
								$('#nextPlaylistTime').html(NextPlaylistTime);
								$('#fppTime').html(fppTime);
								
								// Enable Play
								SetButtonState('#btnPlay','enable');
								SetButtonState('#btnStopNow','disable');
								SetButtonState('#btnStopGracefully','disable');
								$('#selStartPlaylist').removeAttr("disabled");
								UpdateCurrentEntryPlaying(0);
							}
							else
							{
								var Volume = status.childNodes[2].textContent;
								var CurrentPlaylist = status.childNodes[3].textContent;
								var CurrentPlaylistType = status.childNodes[4].textContent;
								var CurrentSeqName = status.childNodes[5].textContent;
								var CurrentSongName = status.childNodes[6].textContent;
								var CurrentPlaylistIndex = status.childNodes[7].textContent;
								var CurrentPlaylistCount = status.childNodes[8].textContent;
								var SecondsPlayed = parseInt(status.childNodes[9].textContent,10);
								var SecondsRemaining = parseInt(status.childNodes[10].textContent,10);
								var NextPlaylist = status.childNodes[11].textContent;
								var NextPlaylistTime = status.childNodes[12].textContent;
								var fppTime = status.childNodes[13].textContent;
								var repeatMode = parseInt(status.childNodes[14].textContent,10);
								if(gblCurrentLoadedPlaylist != CurrentPlaylist)
								{
									$('#selStartPlaylist').val(CurrentPlaylist);
									PopulateStatusPlaylistEntries(false,CurrentPlaylist,true);
								}
								// Disable Play
								SetButtonState('#btnPlay','disable');
								SetButtonState('#btnStopNow','enable');
								SetButtonState('#btnStopGracefully','enable');
								$('#selStartPlaylist').attr("disabled");
	
								$('#txtNextPlaylist').html(NextPlaylist);
								$('#nextPlaylistTime').html(NextPlaylistTime);
								$('#fppTime').html(fppTime);
								
								var minsPlayed = Math.floor(SecondsPlayed/60);
								minsPlayed = isNaN(minsPlayed)?0:minsPlayed;
								var secsPlayed = SecondsPlayed%60;
								secsPlayed = isNaN(secsPlayed)?0:secsPlayed;
								
								var minsRemaining = Math.floor(SecondsRemaining/60);
								minsRemaining = isNaN(minsRemaining)?0:minsRemaining;
								var secsRemaining = SecondsRemaining%60;
								secsRemaining = isNaN(secsRemaining)?0:secsRemaining;
								if(fppStatus == STATUS_PLAYING)
								{
									$('#txtPlayerStatus').html("Playing <strong>'" + CurrentPlaylist + "'</strong>");
									$('#txtTimePlayed').html("Elapsed: " + zeroPad(minsPlayed,2) + ":" + zeroPad(secsPlayed,2));								
									$('#txtTimeRemaining').html("Remaining: " + zeroPad(minsRemaining,2) + ":" + zeroPad(secsRemaining,2));						
									
								}
								else if(fppStatus == STATUS_STOPPING_GRACEFULLY)
								{
									$('#txtPlayerStatus').html("Playing <strong>'" + CurrentPlaylist + "'</strong> - Stopping Gracefully");
									$('#txtTimePlayed').html("Elapsed: " + zeroPad(minsPlayed,2) + ":" + zeroPad(secsPlayed,2));								
									$('#txtTimeRemaining').html("Remaining: " + zeroPad(minsRemaining,2) + ":" + zeroPad(secsRemaining,2));								
								}
	
								if(CurrentPlaylistIndex != gblCurrentPlaylistIndex && CurrentPlaylistIndex <= gblCurrentLoadedPlaylistCount)
								{
										UpdateCurrentEntryPlaying(CurrentPlaylistIndex);
										gblCurrentPlaylistIndex = CurrentPlaylistIndex;
										if(CurrentPlaylistIndex != 1)
										{
											var j=0;	
										}
								}

								if (repeatMode)
									$("#chkRepeat").prop( "checked", true );
								else
									$("#chkRepeat").prop( "checked", false );
							}
						}
					}
					else
					{
						$('#fppTime').html('');
						IsFPPDrunning();
					}
				}
			};
			
			xmlhttp.send();
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
					if(receivedBytes.childNodes.length> 0)
					{
						html =  "<table>";
						html += "<tr id=\"rowReceivedBytesHeader\"><td>Universe</td><td>Start Address</td><td>Bytes Transferred</td>";

						var i;	
						for(i=0;i<receivedBytes.childNodes.length;i++)
						{
								if(i==32)
								{
									html += "</table>";
									html1 = html;
									html =  "<table>";
									html += "<tr id=\"rowReceivedBytesHeader\"><td>Universe</td><td>Start Address</td><td>Bytes Transferred</td>";
								}
								var universe = receivedBytes.childNodes[i].childNodes[0].textContent;
								var startChannel = receivedBytes.childNodes[i].childNodes[1].textContent;
								var bytes = receivedBytes.childNodes[i].childNodes[2].textContent;
								html += "<tr><td>" + universe + "</td>";
								html += "<td>" + startChannel + "</td><td>" + bytes + "</td></tr>";
						}
						html += "</table>";
					}
					if(receivedBytes.childNodes.length>32)
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
		$('#tblStatusPlaylistEntries tr').removeClass('PlaylistRowPlaying');
		$('#tblStatusPlaylistEntries td').removeClass('PlaylistPlayingIcon');
		
		if(index >= 0)
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
				$(button).attr("disabled");
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
			.success(function() {
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
		).success(function() {
			$('html,body').css('cursor','auto');
			$.jGrowl('FPPD Restarted');
			ClearRestartFlag();
		}).fail(function() {
			$('html,body').css('cursor','auto');
			DialogError("Restart FPPD", "Error restarting FPPD");
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
		if (PlayEntrySelected >= gblCurrentLoadedPlaylistCount)
		{
				PlayEntrySelected = 0;
		}
		var url = "fppxml.php?command=startPlaylist&playList=" + Playlist + "&repeat=" + repeat + "&playEntry=" + PlayEntrySelected ;
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
		$('#newEventEffect').val($('#event_' + TriggerEventSelected).find('td:eq(3)').text());
		$('#newEventStartChannel').val($('#event_' + TriggerEventSelected).find('td:eq(4)').text());
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

	function InsertNewEvent(name, id, effect, startChannel, script)
	{
		var idStr = id.replace('_', ' / ');
		$('#tblEventEntries').append(
		"<tr id='event_" + id + "'><td class='eventTblID'>" + idStr +
            "</td><td class='eventTblName'>" + name +
            "</td><td class='eventTblScript'>" + script +
            "</td><td class='eventTblEffect'>" + effect +
            "</td><td class='eventTblStartCh'>" + startChannel +
            "</td></tr>"
		);
	}

	function UpdateExistingEvent(name, id, effect, startChannel, script)
	{
		var idStr = id.replace('_', ' / ');
		var row = $('#tblEventEntries tr#event_' + id);
		row.find('td:eq(0)').text(idStr);
		row.find('td:eq(1)').text(name);
		row.find('td:eq(2)').text(script);
		row.find('td:eq(3)').text(effect);
		row.find('td:eq(4)').text(startChannel);
	}

	function SaveEvent()
	{
		var url = "fppxml.php?command=saveEvent&event=" + $('#newEventName').val() +
			"&id=" + $('#newEventID').val() +
			"&effect=" + $('#newEventEffect').val() +
			"&startChannel=" + $('#newEventStartChannel').val() +
			"&script=" + $('#newEventScript').val();
		var xmlhttp=new XMLHttpRequest();
		xmlhttp.open("GET",url,true);
		xmlhttp.setRequestHeader('Content-Type', 'text/xml');
		xmlhttp.onreadystatechange = function () {
			if (xmlhttp.readyState == 4 && xmlhttp.status==200)
			{
				if ($('#tblEventEntries tr#event_' + $('#newEventID').val()).attr('id'))
				{
					UpdateExistingEvent($('#newEventName').val(), $('#newEventID').val(),
						$('#newEventEffect').val(), $('#newEventStartChannel').val(),
						$('#newEventScript').val());
				}
				else
				{
					InsertNewEvent($('#newEventName').val(), $('#newEventID').val(),
						$('#newEventEffect').val(), $('#newEventStartChannel').val(),
						$('#newEventScript').val());
				}
				SetButtonState('#btnTriggerEvent','disable');
				SetButtonState('#btnEditEvent','disable');
				SetButtonState('#btnDeleteEvent','disable');
			}
		}
		xmlhttp.send();
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
			var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=rebootPi";
			xmlhttp.open("GET",url,true);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
			xmlhttp.send();
			ClearRebootFlag();
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
			var mediaFile;
			var seqFile;
			var pause;
			var pluginData;
    	var xmlhttp=new XMLHttpRequest();
			var innerHTML="";
			if(playselected==true)
			{
				pl = $('#selStartPlaylist :selected').text(); 
			}
			else
			{	
				pl = playList;
			}
			var url = "fppxml.php?command=getPlayListEntries&pl=" + pl + "&reload=" + reloadFile;
			xmlhttp.open("GET",url,true);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
	 
			xmlhttp.onreadystatechange = function () {
				if (xmlhttp.readyState == 4 && xmlhttp.status==200) 
				{
					var xmlDoc=xmlhttp.responseXML; 
					var entries = xmlDoc.getElementsByTagName('PlaylistEntries')[0];
					
					gblCurrentLoadedPlaylist = pl;
					gblCurrentLoadedPlaylistCount = entries.childNodes.length;
					if(entries.childNodes.length> 0)	
					{
							for(i=0;i<entries.childNodes.length;i++)
							{
								type = entries.childNodes[i].childNodes[0].textContent;
  							seqFile = entries.childNodes[i].childNodes[1].textContent;
								mediaFile = entries.childNodes[i].childNodes[2].textContent;
								pause = entries.childNodes[i].childNodes[3].textContent;
								eventName = entries.childNodes[i].childNodes[5].textContent;
								eventID = entries.childNodes[i].childNodes[6].textContent.replace('_', ' / ');
								pluginData = entries.childNodes[i].childNodes[7].textContent;
								if(type == 'b')
								{
										innerHTML +=  "<tr id=\"playlistRow" + (i+1).toString() + "\">";
										innerHTML +=  "<td id = \"colEntryNumber" + (i+1).toString() + "\" width=\"6%\" class = \"textRight\">" + (i+1).toString() + ".</td>";
										innerHTML +=  "<td width=\"42%\" class=\"textLeft\">" + mediaFile + "</td>";
										innerHTML +=  "<td width=\"42%\" class=\"textLeft\">" + seqFile + "</td>"
										innerHTML += "<td width=\"10%\" id=\"firstLast" + i.toString() + "\" class=\"textCenter\"></td>";
									  innerHTML += "</tr>";
								}
								else if(type == 'm')
								{
										innerHTML +=  "<tr id=\"playlistRow" + (i+1).toString() + "\">";
										innerHTML +=  "<td id = \"colEntryNumber" + (i+1).toString() + "\" width=\"6%\" class = \"textRight\">" + (i+1).toString() + ".</td>";
										innerHTML +=  "<td width=\"42%\" class=\"textLeft\">" + mediaFile + "</td>";
										innerHTML +=  "<td width=\"42%\" class=\"textLeft\">---</td>"
										innerHTML += "<td width=\"10%\" id=\"firstLast" + i.toString() + "\" class=\"textCenter\"></td>";
									  innerHTML += "</tr>";
								}
								else if(type == 's')
								{
										innerHTML +=  "<tr id=\"playlistRow" + (i+1).toString() + "\">";
										innerHTML +=  "<td id = \"colEntryNumber" + (i+1).toString() + "\" width=\"6%\" class = \"textRight\">" + (i+1).toString() + ".</td>";
										innerHTML +=  "<td width=\"42%\" class=\"textLeft\">---</td>";
										innerHTML +=  "<td width=\"42%\" class=\"textLeft\">" + seqFile + "</td>"
										innerHTML += "<td width=\"10%\" id=\"firstLast" + i.toString() + "\" class=\"textCenter\"></td>";
									  innerHTML += "</tr>";
								}
								else if(type == 'p')
								{
										innerHTML +=  "<tr id=\"playlistRow" + (i+1).toString() + "\">";
										innerHTML +=  "<td id = \"colEntryNumber" + (i+1).toString() + "\" width=\"6%\" class = \"textRight\">" + (i+1).toString() + ".</td>";
										innerHTML +=  "<td width=\"42%\" class=\"textLeft\">PAUSE - " + pause.toString() + " seconds</td>";
										innerHTML +=  "<td width=\"42%\" class=\"textLeft\">---</td>"
										innerHTML += "<td width=\"10%\" id=\"firstLast" + i.toString() + "\" class=\"textCenter\"></td>";
									  innerHTML += "</tr>";
								}
								else if(type == 'e')
								{
										innerHTML +=  "<tr id=\"playlistRow" + (i+1).toString() + "\">";
										innerHTML +=  "<td id = \"colEntryNumber" + (i+1).toString() + "\" width=\"6%\" class = \"textRight\">" + (i+1).toString() + ".</td>";
										innerHTML +=  "<td width=\"42%\" class=\"textLeft\">" + eventID + " - " + eventName + "</td>";
										innerHTML +=  "<td width=\"42%\" class=\"textLeft\">---</td>";
										innerHTML += "<td width=\"10%\" id=\"firstLast" + i.toString() + "\" class=\"textCenter\"></td>";
										innerHTML += "</tr>";
								}
								else if(type == 'P')
								{
										innerHTML +=  "<tr id=\"playlistRow" + (i+1).toString() + "\">";
										innerHTML +=  "<td id = \"colEntryNumber" + (i+1).toString() + "\" width=\"6%\" class = \"textRight\">" + (i+1).toString() + ".</td>";
										innerHTML +=  "<td width=\"42%\" class=\"textLeft\">plugin - " + pluginData.toString() + "</td>";
										innerHTML +=  "<td width=\"42%\" class=\"textLeft\">---</td>"
										innerHTML += "<td width=\"10%\" id=\"firstLast" + i.toString() + "\" class=\"textCenter\"></td>";
									  innerHTML += "</tr>";
								}
							}
					}
					else
					{
										innerHTML +=  "<tr class=\"playlistPlayingEntry\">";
										innerHTML +=  "<td>Playlist not loaded</td>";
									  innerHTML += "</tr>";
					}
					var results = document.getElementById("tblStatusPlaylistEntries");
					GetStatusPlayListSettings(playList);
					results.innerHTML = innerHTML;	
				}
			}
			xmlhttp.send();
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
	).success(function() {
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

function DownloadFile(dir, file)
{
	location.href="fppxml.php?command=getFile&dir=" + dir + "&filename=" + file;
}

function DownloadZip(dir)
{
	location.href="fppxml.php?command=getZip&dir=" + dir;
}

function ViewFile(dir, file)
{
	$('#fileText').html("Loading...");
	$('#fileText').load("fppxml.php?command=getFile&dir=" + dir + "&filename=" + file, function() {
		var ext = file.split('.').pop();
		if (ext != "html")
			$('#fileText').html("<pre>" + $('#fileText').html() + "</pre>");
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

function ConvertFileDialog(file)
{
	$( "#dialog-confirm" ).dialog({
		resizable: false,
		height: 240,
		modal: true,
		buttons: {
			"Sequence": function() {
				$( this ).dialog( "close" );
				ConvertFile(file, "sequence");
			},
			"Effect": function() {
				$( this ).dialog( "close" );
				ConvertFile(file, "effect");
			}
		}
	});
}

function ConvertFile(file, convertTo)
{
	var opts = {
		lines: 9, // The number of lines to draw
		length: 25, // The length of each line
		width: 10, // The line thickness
		radius: 25, // The radius of the inner circle
		corners: 1, // Corner roundness (0..1)
		rotate: 0, // The rotation offset
		direction: 1, // 1: clockwise, -1: counterclockwise
		color: '#fff', // #rgb or #rrggbb or array of colors
		speed: 1, // Rounds per second
		trail: 60, // Afterglow percentage
		shadow: false, // Whether to render a shadow
	};

	var target = document.getElementById('overlay');
	var spinner = new Spinner(opts).spin(target);

	target.style.display = 'block';
	document.body.style.cursor = "wait";

	$.get("fppxml.php?command=convertFile&convertTo=" +
		convertTo + "&filename=" + file).success(function(data) {
	
			target.style.display = 'none';
			document.body.style.cursor = "default";

			var result = $(data).find( "Status" ).text();

			if ( result == "Success" )
			{
				GetFiles('Uploads');
				if ( convertTo == "sequence" )
				{
					GetFiles('Sequences');
					var index = $('#tabs a[href="#tab-sequence"]').parent().index();
					$('#tabs').tabs( "option", "active", index );
				}
				else if ( convertTo == "effect" )
				{
					GetFiles('Effects');
					var index = $('#tabs a[href="#tab-effects"]').parent().index();
					$('#tabs').tabs( "option", "active", index );
				}
				$.jGrowl("Sequence Converted Successfully!");
			}
			else // if ( result == "Failure" )
			{
				DialogError("Failed to convert sequence!", $(data).find( "Error" ).text());
			}
		}).fail(function(data) {
			target.style.display = 'none';
			document.body.style.cursor = "default";

			DialogError("Failed to initiate conversion!", $(data).find( "Error" ).text());
		});
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

