FPP has the ability to both publish status via MQTT as well as receive  commands by subscribing to specific topics.

Topic Format
------------

{optional prefix}/falcon/player/{hostname}/{sub_topic}

The {optional prefix} is defined in the advanced settings
The {hostname} is defined in the Network Settings tab and should be different for each FPP device on the network.
The {sub_topic} is outlined below...

FPP Publish subtopics
---------------------

FPP will published the following sub_topics (using the full topic format) if MQTT is configured

*/ready -- 1 when fppd starts, 0 when fppd goes down.  Has "last will" set to 0.
*/version -- The current full version of the fpp software
*/branch -- The git branch currently in use
*/status -- Status of the player (idle, playing)
*/warnings -- A JSON string that can be parsed into an Array of Warning messages.

*/playlist/position/status
*/playlist/name/status - Name of current playlist. Can be {null}.
*/playlist/repeat/status - {0 = not repeating current playlist, 1 = repeating current playlist}
*/playlist/section/status - either “New”, “LeadIn”, “MainPlayList”, “LeadOut”, or {null} if nothing playing
*/playlist/sectionPosition/status - The numeric position of the item currently playing in the playlist section (zero based)
*/playlist/sequence/status - Name of current sequence file playing. Can be {null}.
*/playlist/sequence/secondsTotal - The duration of the current sequence file playing, in seconds.  Can be {null}.

*/playlist/media/title - Title of the audio / video media being displayed
*/playlist/media/status - filename of the current audio/movie
*/playlist/media/artist - The artist listing in the audio / video file being played
*/playlist_details - If the MQTT Playlist Publish Frequency option (Advanced settings) is > 0 then a JSON file is published based on the duration (seconds) specified.
*/fppd_status - If the MQTT Status Publish Frequency option (Advanced settings) is > 0 then a JSON file is published based on the duration (seconds) specified.
*/port_status - If the MQTT Port Status Publish Frequency option (Advanced settings) is > 0 then a JSON file is published based on the duration (seconds) specified.

Message Example for fppd_status
-------------------------------

```json
{
"MQTT" :                                                                                                                                                                                                                    [38/1995]
{
"configured" : true,
"connected" : true
},
"branch" : "master",
"bridging" : false,
"current_playlist" :
{
"count" : "14",
"description" : "Idle 30 seconds",
"index" : "8",
"playlist" : "Idle30",
"type" : "pause"
},
"current_sequence" : "",
"current_song" : "",
"dateStr" : "Fri Jan 26",
"fppd" : "running",
"mode" : 2,
"mode_name" : "player",
"multisync" : false,
"next_playlist" :
{
"playlist" : "Idle30",
"start_time" : "Sat Jan 27 @ 12:00 AM - (Everyday)"
},
"repeat_mode" : 1,
"scheduler" :
{
"currentPlaylist" :
{
"actualEndTime" : 1706331600,
"actualEndTimeStr" : "Sat Jan 27 @ 12:00 AM",
"actualStartTime" : 1706319270,
"actualStartTimeStr" : "Fri Jan 26 @ 08:34 PM",
"currentTime" : 1706320325,
"currentTimeStr" : "Wed Dec 31 @ 07:00 PM",
"playlistName" : "Idle30",
"scheduledEndTime" : 1706331600,
"scheduledEndTimeStr" : "Sat Jan 27 @ 12:00 AM",
"scheduledStartTime" : 1706245200,
"scheduledStartTimeStr" : "Fri Jan 26 @ 12:00 AM",
"secondsRemaining" : 11275,
"stopType" : 0,
"stopTypeStr" : "Graceful"
},
"enabled" : 1,
"nextPlaylist" :
{
"playlistName" : "Idle30",
"scheduledStartTime" : 1706331600,
"scheduledStartTimeStr" : "Sat Jan 27 @ 12:00 AM - (Everyday)"
},
"status" : "playing"
},
"seconds_elapsed" : "0",
"seconds_played" : "0",
"seconds_remaining" : "4",
"sensors" :
[
{
"formatted" : "26.0",
"label" : "Temp: ",
"postfix" : "",
"prefix" : "",
"value" : 25.971428571428731,
"valueType" : "Temperature"
},
{
"formatted" : "5.1V",
"label" : "VIN: ",
"postfix" : "V",
"prefix" : "",
"value" : 5.0794517007823536,
"valueType" : "Voltage"
}
],
"status" : 1,
"status_name" : "playing",
"time" : "Fri Jan 26 20:52:05 EST 2024",
"timeStr" : "08:52 PM",
"timeStrFull" : "08:52:05 PM",
"time_elapsed" : "00:00",
"time_remaining" : "00:04",
"uptime" : "17:35",
"uptimeDays" : 0.012210648148148148,
"uptimeHours" : 0.29305555555555557,
"uptimeMinutes" : 17.583333333333332,
"uptimeSeconds" : 35,
"uptimeStr" : "0 days, 0 hours, 17 minutes, 35 seconds",
"uptimeTotalSeconds" : 1055,
"uuid" : "M2-4c928e82-f67f-930d-56ee-7f883e62591b",
"version" : "8.x-master-25-g7d4021a0-dirty",
"volume" : 70
}
```

Message Example for playlist_details
------------------------------------

Note: Currently FPP only allows one active Playlist and for each Playlist, one active "item".  However,
this will be increased in the future.  Thus activePlaylist and currentItems return list of entries.
The sample currentItems below shows 3 records (one for the 3 most common types) but until concurrent
entries in a Playlist are allowed, only one will be displayed in real life.

```{
"status" : "playing",    // playing or idle
"activePlaylist" : [
{
"name" : "Name of current playlist",
"desc" : "Description of the PlayList",
"repeat" : 1,     // 0 if playist is not repating, 1 if repeating
"currentItems" : [
{
"type": "both",
"sequenceName" : "Name of current sequence file playing.",
"mediaTitle" : "Title of the audio / video media being displayed",
"mediaName" : "filename of the current audio/movie",
"mediaArtist" : "The artist listing in the audio / video file being played",
"secondsRemaining" : 33,
"secondsTotal" : 54,
"secondsElapsed" : 21
},
{
"type": "sequence",
"sequenceName" : "Name of current sequence file playing.",
"secondsRemaining" : 33,
"secondsTotal" : 54,
"secondsElapsed" : 21
},
{
"type": "media",
"mediaTitle" : "Title of the audio / video media being displayed",
"mediaName" : "filename of the current audio/movie",
"mediaArtist" : "The artist listing in the audio / video file being played",
"secondsRemaining" : 33,
"secondsTotal" : 54,
"secondsElapsed" : 21
}
]
}
]
}
```

Message Example for port_status
-------------------------------

When output sensors are present, this will report the status of each port and how many milliamps were
being pulled when the port was sampled.

```[
{
"col": 1,
"enabled": true,
"ma": 435,
"name": "Port #1",
"row": 1,
"status": true
},
{
"col": 2,
"enabled": false,
"ma": 0,
"name": "Port #2",
"row": 1,
"status": true
},
{
"col": 3,
"enabled": false,
"ma": 0,
"name": "Port #3",
"row": 1,
"status": true
},
{
"col": 4,
"enabled": false,
"ma": 0,
"name": "Port #4",
"row": 1,
"status": true
}
]
```

FPP Subscribed subtopics
------------------------

FPP will listen for these topics if MQTT is configured.

* playlist/ALLPLAYLISTS/stop/now - Immediate stop all running playlist
* playlist/ALLPLAYLISTS/stop/graceful - gracefully stop all running playlist
* playlist/ALLPLAYLISTS/stop/afterloop.- Allow playlist to finish current loop then stop.

There are also Playlist specific topics. Currently, ${PLAYLIST} is ignored for all but /start and will affect the current running playlist. In the future, when multiple concurrent playlist are running it will affect only the specified playlist.

* playlist/${PLAYLISTNAME}/start - Starts the playlist with the given name. (payload ignored)
* playlist/${PLAYLISTNAME}/next - Forces playing of the the next item in the playlist. (payload ignored)
* playlist/${PLAYLISTNAME}/prev - Forces playing of the previous item in the playlist (payload ignored)
* playlist/${PLAYLISTNAME}/repeat - Sets if the playlist will be repeated or terminate when done. (payload should be 0 or 1)
* playlist/${PLAYLISTNAME}/startPosition - Sets the item in the playlist (zero based) to play next. Does not immediately stop current item .
* playlist/${PLAYLISTNAME}/stop/now - Forces the playlist to stop immediately.
* playlist/${PLAYLISTNAME}/stop/graceful - gracefully stop playlist
* playlist/${PLAYLISTNAME}/stop/afterloop - Allow playlist to finish current loop then stop.

These are system commands for specific

* system/fppd/stop - Stops the fppd process (Will also kill the MQTT listener!)
* system/fppd/restart - Executes a Fast restart on the fppd process. (Reloads all settings.)
* system/shutdown - Shuts down the OS
* system/restart - Reboots the OS

It is possible to send command

* /set/command/${command} - Would run the specify command (any playload will be passed as if it was a REST API call)

