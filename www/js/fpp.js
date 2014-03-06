
STATUS_IDLE = "0";
STATUS_PLAYING = "1";
STATUS_STOPPING_GRACEFULLY = "2";


// Globals
gblCurrentPlaylistIndex = 0;
gblCurrentLoadedPlaylist  = '';
gblCurrentLoadedPlaylistCount = 0;


function PopulateLists()
{
	PopulatePlaylists("playList");
	var firstPlaylist = document.getElementById("playlist0").innerHTML;
	PopulatePlayListEntries(firstPlaylist,true);
}

function PopulatePlaylists(element)
	{
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


function GetPlayListSettings(playList)
{
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

function GetStatusPlayListSettings(playList)
{
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

function PopulatePlayListEntries(playList,reloadFile,selectedRow)
	{
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
								var videoFile = entries.childNodes[i].childNodes[5].textContent;
								var eventName = entries.childNodes[i].childNodes[6].textContent;
								var eventID = entries.childNodes[i].childNodes[7].textContent.replace('_', ' / ');
								if(type == 'b')
										innerHTML += GetPlaylistRowHTML((i+1).toString(), "Seq/Med", mediaFile, seqFile, i.toString());
								else if(type == 'm')
										innerHTML += GetPlaylistRowHTML((i+1).toString(), "Media", mediaFile, "---", i.toString());
								else if(type == 's')
										innerHTML += GetPlaylistRowHTML((i+1).toString(), "Seq.", "---", seqFile, i.toString());
								else if(type == 'p')
										innerHTML += GetPlaylistRowHTML((i+1).toString(), "Pause", "PAUSE - " + pause.toString(), "---", i.toString());
								else if(type == 'v')
								{
										innerHTML += GetPlaylistRowHTML(
											"<font color='red'><b>" + (i+1).toString() + "</b></font>",
											"<font color='red'><b>Video</b></font>",
											"<font color='red'><b>" + videoFile + "</b></font>",
											"<font color='red'><b>'Video' type deprecated, use 'Media' entries instead</b></font>",
											i.toString());
								}
								else if(type == 'e')
								{
										innerHTML += GetPlaylistRowHTML((i+1).toString(), "Event", eventID + " - " + eventName, "---", i.toString());
								}
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
		
		function PlaylistTypeChanged()
		{
			var type=document.getElementById("selType").selectedIndex;
			switch(type)
			{
				case 0:
					$("#musicOptions").css("display","block");
					$("#sequenceOptions").css("display","block");
					$("#videoOptions").css("display","none");
					$("#eventOptions").css("display","none");
					$("#pauseTime").css("display","none");
					$("#pauseText").css("display","none");
					$("#delayText").css("display","none");
					break;
				case 1:	
					$("#musicOptions").css("display","block");
					$("#sequenceOptions").css("display","none");
					$("#videoOptions").css("display","none");
					$("#eventOptions").css("display","none");
					$("#pauseTime").css("display","none");
					$("#pauseText").css("display","none");
					$("#delayText").css("display","none");
					break;
				case 2:	
					$("#musicOptions").css("display","none");
					$("#sequenceOptions").css("display","block");
					$("#videoOptions").css("display","none");
					$("#eventOptions").css("display","none");
					$("#pauseTime").css("display","none");
					$("#pauseText").css("display","block");
					$("#delayText").css("display","none");
					break;
				case 3:	
					$("#musicOptions").css("display","none");
					$("#sequenceOptions").css("display","none");
					$("#videoOptions").css("display","none");
					$("#eventOptions").css("display","none");
					$("#pauseTime").css("display","block");
					$("#pauseText").css("display","block");
					$("#delayText").css("display","none");
					break;
				case 4:
					$("#musicOptions").css("display","none");
					$("#sequenceOptions").css("display","none");
					$("#videoOptions").css("display","block");
					$("#eventOptions").css("display","none");
					$("#pauseTime").css("display","block");
					$("#pauseText").css("display","none");
					$("#delayText").css("display","block");
					break;
				case 5:
					$("#musicOptions").css("display","none");
					$("#sequenceOptions").css("display","none");
					$("#videoOptions").css("display","none");
					$("#eventOptions").css("display","block");
					$("#pauseTime").css("display","none");
					$("#pauseText").css("display","none");
					$("#delayText").css("display","none");
					break;
			}
			
		}

	
		function AddNewPlaylist()
		{
			var name=document.getElementById("txtNewPlaylistName");

    	var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=addPlayList&pl=" + name.value;
			xmlhttp.open("GET",url,false);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
	 
			xmlhttp.onreadystatechange = function () {
				if (xmlhttp.readyState == 4 && xmlhttp.status==200) 
				{
					var xmlDoc=xmlhttp.responseXML; 
					var productList = xmlDoc.getElementsByTagName('Music')[0];
					PopulatePlaylists('playList');
					PopulatePlayListEntries(name.value,true);
					$('#txtName').val(name.value);
					$('#txtName').focus()
					$('#txtName').select()
					
				}
			};
			
			xmlhttp.send();

		}
		
		function PlaylistEntryIndexChanged(newIndex,oldIndex)
		{
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

		function AddPlaylistEntry()
		{
    	var xmlhttp=new XMLHttpRequest();
			var name=document.getElementById("txtPlaylistName");
			var type = document.getElementById("selType").value;
			var	seqFile = document.getElementById("selSequence").value;
			var	mediaFile = document.getElementById("selMedia").value;
			//var	videoFile = document.getElementById("selVideo").value;
			//var	eventSel = document.getElementById("selEvent");
			//var	eventID = eventSel.value;
			//var	eventName = '';
      //if(eventSel.selectedIndex>=0)
      //{
      //  eventName = eventSel.options[eventSel.selectedIndex].innerHTML.replace(/.* - /, '');
      //}
			var	pause = document.getElementById("txtPause").value;
			var url = "fppxml.php?command=addPlaylistEntry&type=" + type + "&seqFile=" + 
			           encodeURIComponent(seqFile) + "&mediaFile=" + 
								 encodeURIComponent(mediaFile) + "&pause=" + pause 
								 //encodeURIComponent(videoFile) + "&eventID=" +
								 //encodeURIComponent(eventID) + "&eventName=" +
								 //encodeURIComponent(eventName);
								 ;
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
		
		function SavePlaylist()
		{
			var name=document.getElementById("txtPlaylistName");
    	var xmlhttp=new XMLHttpRequest();
			var first = $("#chkFirst").is(':checked')?'1':'0';
			var last = $("#chkLast").is(':checked')?'1':'0';
			var url = "fppxml.php?command=savePlaylist&name=" + name.value + "&first=" + first + "&last="+last;
			xmlhttp.open("GET",url,false);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
	 
			xmlhttp.onreadystatechange = function () {
				if (xmlhttp.readyState == 4 && xmlhttp.status==200) 
				{
					var xmlDoc=xmlhttp.responseXML; 
          PopulatePlaylists("playList");
          PopulatePlayListEntries(name.value,true);
				}
			};
			
			xmlhttp.send();

		}

		function DeletePlaylist()
		{
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
					var firstPlaylist = document.getElementById("playlist0").innerHTML;
					PopulatePlayListEntries(firstPlaylist,true);
				}
			};
			
			xmlhttp.send();

		}
		
		
		
		function RemovePlaylistEntry()
		{
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

		function ManualGitUpdate()
		{
			SetButtonState("#ManualUpdate", "disable");

			var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=manualGitUpdate";
			xmlhttp.open("GET",url,false);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
			xmlhttp.onreadystatechange = function () {
				if (xmlhttp.readyState == 4)
					location.reload(true);
			}
			xmlhttp.send();
		}

		function ChangeGitBranch(newBranch)
		{
			var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=changeGitBranch&branch=" + newBranch;
			xmlhttp.open("GET",url,false);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
			xmlhttp.onreadystatechange = function () {
				if (xmlhttp.readyState == 4)
					location.reload(true);
			}
			xmlhttp.send();
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

			xmlhttp.onreadystatechange = function () {
				if (xmlhttp.readyState == 4 && xmlhttp.status==200) 
				{
					var xmlDoc=xmlhttp.responseXML; 
					var entries = xmlDoc.getElementsByTagName('UniverseEntries')[0];
					if(entries.childNodes.length> 0)
					{
						innerHTML = "<tr class=\"tblheader\">" +  
    	                  "<td width=\"5%\">#</td>" +
                        "<td width=\"7%\">Act</td>" +
												"<td width=\"8%\">Universe</td>" +
												"<td width=\"8%\">Start</td>" +
												"<td width=\"8%\">Size</td>" +
                        "<td width=\"25%\">Type</td>" +
												"<td width=\"20%\">Unicast Address</td>" +
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
								var multicastChecked = type == 0 ? "selected" : "";
								var unicastChecked = type == 1   ? "selected": "";
								var staticIPdisabled = type == 0 ? "disabled": "";
								
								
								
								innerHTML += 	"<tr class=\"rowUniverseDetails\">" +
								              "<td>" + (i+1).toString() + "</td>" +
															"<td><input name=\"chkActive[" + i.toString() + "]\" id=\"chkActive[" + i.toString() + "]\" type=\"checkbox\" " + activeChecked +"/></td>" +
															"<td><input name=\"txtUniverse[" + i.toString() + "]\" id=\"txtUniverse[" + i.toString() + "]\" type=\"text\" size=\"10\" value=\"" + universe.toString() + "\"/></td>" +
															"<td><input name=\"txtStartAddress[" + i.toString() + "]\" id=\"txtStartAddress[" + i.toString() + "]\" type=\"text\" size=\"8\" value=\"" + startAddress.toString() + "\"/></td>" +
															"<td><input name=\"txtSize[" + i.toString() + "]\" id=\"txtSize[" + i.toString() + "]\" type=\"text\"  size=\"8\"/ value=\"" + size.toString() + "\"></td>" +
															
															"<td><select id=\"universeType[" + i.toString() + "]\" name=\"universeType[" + i.toString() + "]\" style=\"width:150px\">" +
															      "<option value=\"0\" " + multicastChecked + ">Multicast</option>" +
															      "<option value=\"1\" " + unicastChecked + ">Unicast</option></select></td>" + 
															"<td><input name=\"txtIP[" + i.toString() + "]\" id=\"txtIP[" + i.toString() + "]\" type=\"text\"/ value=\"" + unicastAddress + "\"></td>" +
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
    	                  "<td width=\"5%\">#</td>" +
                        "<td width=\"15%\">Act</td>" +
                        "<td width=\"40%\">Type</td>" +
												"<td width=\"40%\">Start</td>" +
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
															"<td><input name=\"chkActive[" + i.toString() + "]\" id=\"chkActive[" + i.toString() + "]\" type=\"checkbox\" " + activeChecked +"/></td>" +
															"<td><select id=\"pixelnetDMXtype[" + i.toString() + "]\" name=\"pixelnetDMXtype[" + i.toString() + "]\" style=\"width:150px\">" +
															      "<option value=\"0\" " + pixelnetChecked + ">Pixelnet</option>" +
															      "<option value=\"1\" " + dmxChecked + ">DMX</option></select></td>" + 
															"<td><input name=\"txtStartAddress[" + i.toString() + "]\" id=\"txtStartAddress[" + i.toString() + "]\" type=\"text\" size=\"8\" value=\"" + startAddress.toString() + "\"/></td>" +
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
						if(universeType == '1')
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
					var active=document.getElementById("chkActive[" + selectIndex + "]").value;
					var pixelnetDMXtype=document.getElementById("pixelnetDMXtype[" + selectIndex + "]").value;
					var size = pixelnetDMXtype == "0" ? 4096:512;
					var startAddress=Number(document.getElementById("txtStartAddress[" + selectIndex + "]").value)+ size;
					for(i=PixelnetDMXoutputSelected;i<PixelnetDMXoutputSelected+cloneNumber;i++)
					{
						document.getElementById("pixelnetDMXtype[" + i + "]").value	 = pixelnetDMXtype;
						document.getElementById("txtStartAddress[" + i + "]").value	 = startAddress.toString();
						document.getElementById("chkActive[" + i + "]").value = active;
						startAddress+=size;
					}
				}
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
				if(!validateNumber(txtStartAddress,1,65536))
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
				if(universeType == 1)
				{
					txtUnicastAddress=document.getElementById("txtIP[" + i + "]");	
					if(!validateIPaddress(txtUnicastAddress))
					{
						returnValue = false;
					}
				}
			}
			return returnValue;
		}
		
		function validateIPaddress(textbox)   
		{  
			 if (/^(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/.test(textbox.value))  		{  
				return (true)  
			}  
			textbox.style.border="red solid 1px";
			textbox.value = ""; 
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
    	                  "<td width=\"5%\">#</td>" +
                        "<td width=\"10%\">Enable</td>" +
                        "<td width=\"10%\">Playlist</td>" +
												"<td width=\"10%\">Day</td>" +
												"<td width=\"20%\">Start T	ime</td>" +
												"<td width=\"20%\">End Time</td>" +
                        "<td width=\"15%\">Repeat</td>" +
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

									playlistOptionsText = ""
									for(j=0;j<PlaylistCount;j++)
									{
										playListChecked = playListArray[j] == playlist ? "selected" : "";
										playlistOptionsText +=  "<option value=\"" + playListArray[j] + "\" " + playListChecked + ">" + playListArray[j] + "</option>";
									}
									var tableRow = 	"<tr class=\"rowScheduleDetails\">" +
								              "<td class='center'>" + (i+1).toString() + "</td>" +
															"<td class='center' ><input  name=\"chkEnable[" + i.toString() + "]\" id=\"chkEnable[" + i.toString() + "]\" type=\"checkbox\" " + enableChecked +"/></td>" +

															"<td><select id=\"selPlaylist[" + i.toString() + "]\" name=\"selPlaylist[" + i.toString() + "]\" style=\"width:150px\">" +
															playlistOptionsText + "</select></td>" + 
															"<td><select id=\"selDay[" + i.toString() + "]\" name=\"selDay[" + i.toString() + "]\" style=\"width:150px\">" +
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
															      "<option value=\"13\" " + dayChecked_13 + ">Fri/Sat</option></select>" +
															"<td><input class='time center'  name=\"txtStartTime[" + i.toString() + "]\" id=\"txtStartTime[" + i.toString() + "]\" type=\"text\" size=\"8\" value=\"" + startTime + "\"/></td>" +
															"<td><input class='time center' name=\"txtEndTime[" + i.toString() + "]\" id=\"txtEndTime[" + i.toString() + "]\" type=\"text\" size=\"8\" value=\"" + endTime + "\"/></td>" +
															"<td class='center' ><input name=\"chkRepeat[" + i.toString() + "]\" id=\"chkEnable[" + i.toString() + "]\" type=\"checkbox\" " + repeatChecked +"/></td>" +
															"</tr>";
															
									$('#tblSchedule').append(tableRow);
									$('.time').timeEntry({show24Hours: true, showSeconds: true,spinnerImage: ''});
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

		function DeleteSequence()
		{
    	var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=deleteSequence&name=" + SequenceNameSelected;
			xmlhttp.open("GET",url,false);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
	 
			xmlhttp.onreadystatechange = function () {
				if (xmlhttp.readyState == 4 && xmlhttp.status==200) 
				{
          GetFiles('Sequences');
				}
			};
			xmlhttp.send();
		}

		function DeleteMusic()
		{
    	var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=deleteMusic&name=" + SongNameSelected;
			xmlhttp.open("GET",url,false);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
	 
			xmlhttp.onreadystatechange = function () {
				if (xmlhttp.readyState == 4 && xmlhttp.status==200) 
				{
          GetFiles('Music');
				}
			};
			xmlhttp.send();
		}
		
		function DeleteVideo()
		{
			var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=deleteVideo&name=" + VideoNameSelected;
			xmlhttp.open("GET",url,false);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
	 
			xmlhttp.onreadystatechange = function () {
				if (xmlhttp.readyState == 4 && xmlhttp.status==200) 
				{
					GetFiles('Videos');
				}
			};
			xmlhttp.send();
		}

		function DeleteScript()
		{
			var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=deleteScript&name=" + ScriptNameSelected;
			xmlhttp.open("GET",url,false);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
	 
			xmlhttp.onreadystatechange = function () {
				if (xmlhttp.readyState == 4 && xmlhttp.status==200) 
				{
					GetFiles('Scripts');
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
        var name = files.childNodes[i].childNodes[0].textContent;
        var time = files.childNodes[i].childNodes[1].textContent;
        var tableRow = "<tr class ='fileDetails'><td class ='fileName'>" + name + "</td><td class ='fileTime'>" + time + "</td></tr>";
        $('#tbl' + dir).append(tableRow);
      }
    }
  }

	function moveFile(fileName)
	{
    	var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=moveFile&file=" + fileName;
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
							$("#btnDaemonControl").attr('value', 'Stop FPPD');
							$('#daemonStatus').html("FPPD is running.");
							//$("#playerStatus").css({ display: "block" });
						}
						else
						{
							$("#btnDaemonControl").attr('value', 'Start FPPD');
							$('#daemonStatus').html("FPPD is stopped.");
							//$("#playerStatus").css({ display: "none" });
						} 
					}
				}
			};
			xmlhttp.send();
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

						if (fppStatus == 0)
						{
							$("#btnDaemonControl").attr('value', 'Stop FPPD');
							$('#daemonStatus').html("FPPD is running.");
						}

						var fppMode = status.childNodes[0].textContent;
						if(fppMode == 0 )
						{
							$("#playerStatus").css("display","block");
							$("#nextPlaylist").css("display","block");
							$("#bytesTransferred").css("display","none");
							
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
								SetButtonState('#selStartPlaylist','enable');
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
								if(gblCurrentLoadedPlaylist != CurrentPlaylist)
								{
									PopulateStatusPlaylistEntries(false,CurrentPlaylist,true);
								}
								// Disable Play
								SetButtonState('#btnPlay','disable');
								SetButtonState('#btnStopNow','enable');
								SetButtonState('#btnStopGracefully','enable');
								SetButtonState('#selStartPlaylist','disable');
	
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
									$('#txtTimePlayed').html("Elasped: " + zeroPad(minsPlayed,2) + ":" + zeroPad(secsPlayed,2));								
									$('#txtTimeRemaining').html("Remaining: " + zeroPad(minsRemaining,2) + ":" + zeroPad(secsRemaining,2));						
									
								}
								else if(fppStatus == STATUS_STOPPING_GRACEFULLY)
								{
									$('#txtPlayerStatus').html("Playing <strong>'" + CurrentPlaylist + "'</strong> - Stopping Gracefully");
									$('#txtTimePlayed').html("Elasped: " + zeroPad(minsPlayed,2) + ":" + zeroPad(secsPlayed,2));								
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
							}
						}
						else if (fppMode == 1)
						{
							$("#playerStatus").css("display","none");
							$("#nextPlaylist").css("display","none");
							$("#bytesTransferred").css("display","block");
							
							GetUniverseBytesReceived();
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

	function StopFPPD()
	{
    	var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=stopFPPD";
			xmlhttp.open("GET",url,true);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
			xmlhttp.send();
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
	if (startChannel == undefined)
		startChannel = "1";

	var url = "fppxml.php?command=playEffect&effect=" + PlayEffectSelected + "&startChannel=" + startChannel;
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

function DeleteEffect()
{
	var url = "fppxml.php?command=deleteEffect&effect=" + PlayEffectSelected;
	var xmlhttp=new XMLHttpRequest();
	xmlhttp.open("GET",url,true);
	xmlhttp.setRequestHeader('Content-Type', 'text/xml');
	xmlhttp.onreadystatechange = function () {
		if (xmlhttp.readyState == 4)
			location.reload(true);
	}
	xmlhttp.send();
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
		if (confirm('REBOOT the Falcon Pi Player?')) 
		{
			var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=rebootPi";
			xmlhttp.open("GET",url,true);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
			xmlhttp.send();
		} 
	}

	function ShutdownPi()
	{
		if (confirm('SHUTDOWN the Falcon Pi Player?')) 
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
					
					gblCurrentLoadedPlaylist = playList;
					gblCurrentLoadedPlaylistCount = entries.childNodes.length;
					if(entries.childNodes.length> 0)	
					{
							for(i=0;i<entries.childNodes.length;i++)
							{
								type = entries.childNodes[i].childNodes[0].textContent;
  							seqFile = entries.childNodes[i].childNodes[1].textContent;
								mediaFile = entries.childNodes[i].childNodes[2].textContent;
								pause = entries.childNodes[i].childNodes[3].textContent;
								videoFile = entries.childNodes[i].childNodes[5].textContent;
								eventName = entries.childNodes[i].childNodes[6].textContent;
								eventID = entries.childNodes[i].childNodes[7].textContent.replace('_', ' / ');
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
								else if(type == 'v')
								{
										innerHTML +=  "<tr id=\"playlistRow" + (i+1).toString() + "\">";
										innerHTML +=  "<td id = \"colEntryNumber" + (i+1).toString() + "\" width=\"6%\" class = \"textRight\"><font color='red'><b>" + (i+1).toString() + ".</b></font></td>";
										innerHTML +=  "<td width=\"42%\" class=\"textLeft\"><font color='red'><b>" + videoFile + "</b></font></td>";
										innerHTML +=  "<td width=\"42%\" class=\"textLeft\"><font color='red'><b>'Video' type deprecated, use 'Media' entries instead</b></font></td>"
										innerHTML += "<td width=\"10%\" id=\"firstLast" + i.toString() + "\" class=\"textCenter\"><font color='red'><b></b></font></td>";
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
			var xmlhttp=new XMLHttpRequest();
			var mode = $('#selFPPDmode').val();	
			var url = "fppxml.php?command=setFPPDmode&mode=" + mode;
			xmlhttp.open("GET",url,true);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
			xmlhttp.send();

}

function SetE131interface()
{
			var xmlhttp=new XMLHttpRequest();
			var iface = $('#selE131interfaces').val();	
			var url = "fppxml.php?command=setE131interface&iface=" + iface;
			xmlhttp.open("GET",url,true);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
			xmlhttp.send();
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
					var Volume = xmlDoc.getElementsByTagName('Volume')[0].childNodes[0].textContent;
					$('#slider').slider('value', parseInt(Volume));
					$('#volume').html(Volume);
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
					var mode = xmlDoc.getElementsByTagName('mode')[0].childNodes[0].textContent;
					if(mode == '1')
					{
							$("#playerStatus").css("display","none");
							$("#nextPlaylist").css("display","none");
							$("#selFPPDmode").prop("selectedIndex",1);
							$("#textFPPDmode").text("Bridged Mode");
					}
			}
		};
		xmlhttp.send();
}

function GetGitOriginLog()
{
	$('#logText').html("Loading list of changes from github.");
	$('#logText').load("fppxml.php?command=getGitOriginLog");
	$('#logViewer').dialog({ height: 600, width: 800, title: "Git Changes" });
	$('#logViewer').dialog( "moveToTop" );
}

function ViewLog()
{
	$('#logText').load("fppxml.php?command=getLog&filename=" + LogFileSelected);
	$('#logViewer').dialog({ height: 600, width: 800, title: "Log Viewer: " + LogFileSelected });
	$('#logViewer').dialog( "moveToTop" );
}

function DownloadLog()
{
	location.href="fppxml.php?command=getLog&filename=" + LogFileSelected;
}

function DeleteLog()
{
	if (LogFileSelected.substring(0,1) == "/")
	{
		alert("You can not delete system logs");
		return;
	}

	var xmlhttp=new XMLHttpRequest();
	var url = "fppxml.php?command=deleteLog&name=" + LogFileSelected;
	xmlhttp.open("GET",url,false);
	xmlhttp.setRequestHeader('Content-Type', 'text/xml');
	 
	xmlhttp.onreadystatechange = function () {
		if (xmlhttp.readyState == 4 && xmlhttp.status==200)
		{
			GetFiles('Logs');
		}
	};
	xmlhttp.send();
}

function SaveUSBDongleSettings()
{
	var usbDonglePort = $('#USBDonglePort').val();
	var usbDongleType = $('#USBDongleType').val();

	var xmlhttp=new XMLHttpRequest();
	var url = "fppxml.php?command=saveUSBDongle&port=" + usbDonglePort +
				"&type=" + usbDongleType;
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

