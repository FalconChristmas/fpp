
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
	  	var browserName = navigator.userAgent;
  		var isIE = browserName.match(/MSIE/);
	 
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

function PopulatePlayListEntries(playList,reloadFile,selectedRow)
	{
			var type;
			var pl;
			var songFile;
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
								var songFile = entries.childNodes[i].childNodes[2].textContent;
								var pause = entries.childNodes[i].childNodes[3].textContent;
								if(type == 'b')
								{
										innerHTML +=  "<tr id=\"playlistRow" + (i+1).toString() + "\">";
										innerHTML +=  "<td id = \"colEntryNumber" + (i+1).toString() + "\" width=\"3%\" class = \"textRight\">" + (i+1).toString() + ".</td>";
										innerHTML +=  "<td width=\"42%\" class=\"textLeft\">" + songFile + "</td>";
										innerHTML +=  "<td width=\"42%\" class=\"textLeft\">" + seqFile + "</td>"
										innerHTML += "<td width=\"13%\" class=\"textLeft\"></td>";
									  innerHTML += "</tr>";
								}
								else if(type == 'm')
								{
										innerHTML +=  "<tr id=\"playlistRow" + (i+1).toString() + "\">";
										innerHTML +=  "<td id = \"colEntryNumber" + (i+1).toString() + "\" width=\"3%\" class = \"textRight\">" + (i+1).toString() + ".</td>";
										innerHTML +=  "<td width=\"42%\" class=\"textLeft\">" + songFile + "</td>";
										innerHTML +=  "<td width=\"42%\" class=\"textLeft\">---</td>"
										innerHTML += "<td width=\"13%\" class=\"textLeft\"></td>";
									  innerHTML += "</tr>";
								}
								else if(type == 's')
								{
										innerHTML +=  "<tr id=\"playlistRow" + (i+1).toString() + "\">";
										innerHTML +=  "<td id = \"colEntryNumber" + (i+1).toString() + "\" width=\"3%\" class = \"textRight\">" + (i+1).toString() + ".</td>";
										innerHTML +=  "<td width=\"42%\" class=\"textLeft\">---</td>";
										innerHTML +=  "<td width=\"42%\" class=\"textLeft\">" + seqFile + "</td>"
										innerHTML += "<td width=\"13%\" class=\"textLeft\"></td>";
									  innerHTML += "</tr>";
								}
								else if(type == 'p')
								{
										
										innerHTML +=  "<tr id=\"playlistRow" + (i+1).toString() + "\">";
										innerHTML +=  "<td id = \"colEntryNumber" + (i+1).toString() + "\" width=\"3%\" class = \"textRight\">" + (i+1).toString() + ".</td>";
										innerHTML +=  "<td width=\"42%\" class=\"textLeft\">PAUSE - " + pause.toString() + " seconds</td>";
										innerHTML +=  "<td width=\"42%\" class=\"textLeft\">---</td>"
										innerHTML += "<td width=\"13%\" class=\"textLeft\"></td>";
									  innerHTML += "</tr>";
								}
							}
					}
					else
					{
										innerHTML +=  "<tr class=\"playlistPlayingEntry\">";
										innerHTML +=  "<td>No entries in playlist.</td>";
									  innerHTML += "</tr>";
					}
					var results = document.getElementById("tblCreatePlaylistEntries_tbody");
					results.innerHTML = innerHTML;	
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
					$("#pauseTime").css("display","none");
					break;
				case 1:	
					$("#musicOptions").css("display","block");
					$("#sequenceOptions").css("display","none");
					$("#pauseTime").css("display","none");
					break;
				case 2:	
					$("#musicOptions").css("display","none");
					$("#sequenceOptions").css("display","block");
					$("#pauseTime").css("display","none");
					break;
				case 3:	
					$("#musicOptions").css("display","none");
					$("#sequenceOptions").css("display","none");
					$("#pauseTime").css("display","block");
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
			var	songFile = document.getElementById("selAudio").value;
			var	pause = document.getElementById("txtPause").value;
			var url = "fppxml.php?command=addPlaylistEntry&type=" + type + "&seqFile=" + seqFile + "&songFile=" + songFile + "&pause=" + pause  ;
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
			var url = "fppxml.php?command=save&name=" + name.value;
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
			var name=document.getElementById("txtName");
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

		}
		
		function SelectEntry(index)
		{
			$('#sortable li:nth-child(1n)').removeClass('selectedEntry');
			if(index > $("#sortable li").size())
			{	
				index = ("#sortable li").size()-1;
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
						innerHTML = "<tr class=tblheader\">" +  
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
						innerHTML = "<tr class=tblheader\">" +  
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
					if(!validateEmail(txtUnicastAddress))
					{
						returnValue = false;
					}
				}
			}
			return returnValue;
		}
		
		function validateEmail(textbox)   
		{  
			 if (/^(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/.test(textbox.value))  		{  
				return (true)  
			}  
			textbox.style.border="red solid 1px";
			textbox.value = ""; 
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
															      "<option value=\"6\" " + dayChecked_6 + ">Saturday</option></select>" +
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
			xmlhttp.send();

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
          GetSequenceFiles();
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
          GetMusicFiles();
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


	function moveFile(fileName)
	{
    	var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=moveFile&file=" + fileName;
			xmlhttp.open("GET",url,false);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
			xmlhttp.send();
   		GetSequenceFiles();
			GetMusicFiles();
	}

	function moveFile2(fileName)
	{
    	var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=moveFile&file=" + fileName;
			xmlhttp.open("GET",url,false);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
	 
			xmlhttp.onreadystatechange = function () {
				if (xmlhttp.readyState == 4 && xmlhttp.status==200) 
				{
					GetSequenceFiles();
					GetMusicFiles();
				}
			};
			
			xmlhttp.send();
	}
		
	function updateFPPStatus()
	{
		var status = IsFPPDrunning();
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
							status = GetFPPstatus();
							$("#playerStatus").css({ display: "block" });
						}
						else
						{
							$("#btnDaemonControl").attr('value', 'Start FPPD');
							$('#daemonStatus').html("FPPD is stopped.");
							$("#playerStatus").css({ display: "none" });
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
					if(status.childNodes.length> 0)
					{
						var fppStatus = status.childNodes[1].textContent;
						if(fppStatus == STATUS_IDLE)
						{
							gblCurrentPlaylistIndex =0;
							var NextPlaylist = status.childNodes[2].textContent;
							var NextPlaylistTime = status.childNodes[3].textContent;
							var fppTime = status.childNodes[4].textContent;

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
							var CurrentPlaylist = status.childNodes[2].textContent;
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
								$('#txtTimePlayed').html("Time Elasped: " + zeroPad(minsPlayed,2) + ":" + zeroPad(secsPlayed,2));								
								$('#txtTimeRemaining').html("Time Remaining: " + zeroPad(minsRemaining,2) + ":" + zeroPad(secsRemaining,2));								
								
							}
							else
							{
								$('#txtPlayerStatus').html("Playing <strong>'" + CurrentPlaylist + "'</strong>");
								$('#txtTimePlayed').html("Time Elasped: " + zeroPad(minsPlayed,2) + ":" + zeroPad(secsPlayed,2));								
								$('#txtTimeRemaining').html("Time Remaining: " + zeroPad(minsRemaining,2) + ":" + zeroPad(secsRemaining,2));								
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
		
				}
			};
			
			xmlhttp.send();
	}
	
	function UpdateCurrentEntryPlaying(index,lastIndex)
	{
		$('#tblStatusPlaylistEntries tr').removeClass('PlaylistRowPlaying');
		$('#tblStatusPlaylistEntries td').removeClass('PlaylistPlayingIcon');
		
		if(index != 0)
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
		var repeat = $('#chkRepeat').attr('checked');
		if (PlayEntrySelected >= gblCurrentLoadedPlaylistCount)
		{
				PlayEntrySelected = 0;
		}
		var url = "fppxml.php?command=startPlaylist&playList=" + Playlist + "&repeat=" + repeat + "&playEntry=" + PlayEntrySelected ;
		xmlhttp.open("GET",url,true);
		xmlhttp.setRequestHeader('Content-Type', 'text/xml');
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
		
function verifynotify(field1, field2, result_id, match_html, nomatch_html, button_disable) {
 this.field1 = field1;
 this.field2 = field2;
 this.result_id = result_id;
 this.match_html = match_html;
 this.nomatch_html = nomatch_html;
 this.button_disable = button_disable;

 this.check = function() {

   // Make sure we don't cause an error
   // for browsers that do not support getElementById
   if (!this.result_id) { return false; }
   if (!document.getElementById){ return false; }
   r = document.getElementById(this.result_id);
   if (!r){ return false; }

   if (this.field1.value != "" && this.field1.value == this.field2.value) {
     r.innerHTML = this.match_html;
	 var j = document.getElementById("submit_button");
     j.disabled = false; 
	$('#submit_button').removeClass('disableButtons');
	$('#submit_button').addClass('buttons');
   } else {
     r.innerHTML = this.nomatch_html;
	 //$("#submit_button").attr("disabled", "disabled");
	 var j = document.getElementById("submit_button");
     j.disabled = true; 
	$('#submit_button').removeClass('buttons');
	$('#submit_button').addClass('disableButtons');
   }
 }
}

function PopulateStatusPlaylistEntries(playselected,playList,reloadFile)
{
			var type;
			var pl;
			var songFile;
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
								songFile = entries.childNodes[i].childNodes[2].textContent;
								pause = entries.childNodes[i].childNodes[3].textContent;
								if(type == 'b')
								{
										innerHTML +=  "<tr id=\"playlistRow" + (i+1).toString() + "\">";
										innerHTML +=  "<td id = \"colEntryNumber" + (i+1).toString() + "\" width=\"6%\" class = \"textRight\">" + (i+1).toString() + ".</td>";
										innerHTML +=  "<td width=\"42%\" class=\"textLeft\">" + songFile + "</td>";
										innerHTML +=  "<td width=\"42%\" class=\"textLeft\">" + seqFile + "</td>"
										innerHTML += "<td width=\"10%\" class=\"textCenter\"></td>";
									  innerHTML += "</tr>";
								}
								else if(type == 'm')
								{
										innerHTML +=  "<tr id=\"playlistRow" + (i+1).toString() + "\">";
										innerHTML +=  "<td id = \"colEntryNumber" + (i+1).toString() + "\" width=\"6%\" class = \"textRight\">" + (i+1).toString() + ".</td>";
										innerHTML +=  "<td width=\"42%\" class=\"textLeft\">" + songFile + "</td>";
										innerHTML +=  "<td width=\"42%\" class=\"textLeft\">---</td>"
										innerHTML += "<td width=\"10%\" class=\"textCenter\"></td>";
									  innerHTML += "</tr>";
								}
								else if(type == 's')
								{
										innerHTML +=  "<tr id=\"playlistRow" + (i+1).toString() + "\">";
										innerHTML +=  "<td id = \"colEntryNumber" + (i+1).toString() + "\" width=\"6%\" class = \"textRight\">" + (i+1).toString() + ".</td>";
										innerHTML +=  "<td width=\"42%\" class=\"textLeft\">---</td>";
										innerHTML +=  "<td width=\"42%\" class=\"textLeft\">" + seqFile + "</td>"
										innerHTML += "<td width=\"10%\" class=\"textCenter\"></td>";
									  innerHTML += "</tr>";
								}
								else if(type == 'p')
								{
										
										innerHTML +=  "<tr id=\"playlistRow" + (i+1).toString() + "\">";
										innerHTML +=  "<td id = \"colEntryNumber" + (i+1).toString() + "\" width=\"6%\" class = \"textRight\">" + (i+1).toString() + ".</td>";
										innerHTML +=  "<td width=\"42%\" class=\"textLeft\">PAUSE - " + pause.toString() + " seconds</td>";
										innerHTML +=  "<td width=\"42%\" class=\"textLeft\">---</td>"
										innerHTML += "<td width=\"10%\" class=\"textCenter\"></td>";
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
					results.innerHTML = innerHTML;	
				}
			}
			xmlhttp.send();
}
	
function SetVolume(value)
{
			var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=setVolume&volume=" + value;
			xmlhttp.open("GET",url,true);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
			xmlhttp.send();

}

		
		
