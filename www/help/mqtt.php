<?
require_once '../config.php';
?>
<script>
    helpPage = "help/mqtt.php";
</script>
<center><b>MQTT Integration</b></center>
<hr>

<h3>MQTT Topics Published by FPP</h3>
<p>When MQTT is enabled, FPP publishes status and events to various topics. All topics follow the format:</p>
<p><code><?echo GetSettingValue('MQTTPrefix', '', '', '/'); ?>falcon/player/<?echo GetSettingValue('HostName', 'FPP'); ?>/{sub_topic}</code></p>

<h4>System Status Topics</h4>
<div class='fppTableWrapper selectable'>
<div class='fppTableContents' role="region" aria-labelledby="SystemStatusTable" tabindex="0">
<table width="100%" border="1" id="SystemStatusTable" cellpadding="1" cellspacing="1">
<thead>
<tr><th width="30%">Topic</th><th>Description</th><th width="15%">Retained</th></tr>
</thead>
<tbody>
<tr><td>ready</td><td>1 when fppd starts, 0 when fppd stops. Has "last will" set to 0.</td><td>Yes</td></tr>
<tr><td>version</td><td>Current full version of FPP software</td><td>Yes</td></tr>
<tr><td>branch</td><td>Git branch currently in use</td><td>Yes</td></tr>
<tr><td>status</td><td>Current player status: <code>idle</code> or <code>playing</code></td><td>No</td></tr>
<tr><td>warnings</td><td>JSON array of warning messages</td><td>Yes</td></tr>
</tbody>
</table>
</div>
</div>

<h4>Playlist Topics</h4>
<div class='fppTableWrapper selectable'>
<div class='fppTableContents' role="region" aria-labelledby="PlaylistStatusTable" tabindex="0">
<table width="100%" border="1" id="PlaylistStatusTable" cellpadding="1" cellspacing="1">
<thead>
<tr><th width="30%">Topic</th><th>Description</th></tr>
</thead>
<tbody>
<tr><td>playlist/name/status</td><td>Name of current playlist (can be empty)</td></tr>
<tr><td>playlist/position/status</td><td>Current position in playlist (zero-based)</td></tr>
<tr><td>playlist/repeat/status</td><td>0 = not repeating, 1 = repeating current playlist</td></tr>
<tr><td>playlist/section/status</td><td>Current section: New, LeadIn, MainPlayList, LeadOut, or empty</td></tr>
<tr><td>playlist/sectionPosition/status</td><td>Position of item in current playlist section (zero-based)</td></tr>
<tr><td>playlist/sequence/status</td><td>Name of current sequence file playing (can be empty)</td></tr>
<tr><td>playlist/sequence/secondsTotal</td><td>Duration of current sequence in seconds (can be empty)</td></tr>
<tr><td>playlist/media/status</td><td>Filename of current audio/video file (can be empty)</td></tr>
<tr><td>playlist/media/title</td><td>Title metadata from audio/video file (can be empty)</td></tr>
<tr><td>playlist/media/artist</td><td>Artist metadata from audio/video file (can be empty)</td></tr>
</tbody>
</table>
</div>
</div>

<h4>GPIO Input Event Topics</h4>
<p><i>Note: GPIO inputs must be configured in Status/Control → GPIO Inputs</i></p>
<div class='fppTableWrapper selectable'>
<div class='fppTableContents' role="region" aria-labelledby="GPIOTable" tabindex="0">
<table width="100%" border="1" id="GPIOTable" cellpadding="1" cellspacing="1">
<thead>
<tr><th width="30%">Topic</th><th>Description</th></tr>
</thead>
<tbody>
<tr><td>gpio/{pin_name}/rising</td><td>Published when GPIO input goes high (rising edge). Value is always 1.</td></tr>
<tr><td>gpio/{pin_name}/falling</td><td>Published when GPIO input goes low (falling edge). Value is always 0.</td></tr>
</tbody>
</table>
</div>
</div>
<p><b>Example topics:</b> <code>gpio/GPIO17/rising</code>, <code>gpio/P8-07/falling</code></p>

<h4>Command and Preset Event Topics</h4>
<div class='fppTableWrapper selectable'>
<div class='fppTableContents' role="region" aria-labelledby="CommandTable" tabindex="0">
<table width="100%" border="1" id="CommandTable" cellpadding="1" cellspacing="1">
<thead>
<tr><th width="30%">Topic</th><th>Description</th></tr>
</thead>
<tbody>
<tr><td>command/run</td><td>Published when any FPP command executes. Payload is JSON with command name and arguments array.</td></tr>
<tr><td>command/preset/triggered</td><td>Published when a command preset is triggered. Payload is JSON with preset name or slot number, and optional keyword replacements.</td></tr>
</tbody>
</table>
</div>
</div>

<h5>Example Command Payloads:</h5>
<pre style="background-color: #f5f5f5; padding: 10px; border-radius: 5px;">
<b>command/run:</b>
{
  "command": "Start Playlist",
  "args": ["MyPlaylist", "true"]
}

<b>command/preset/triggered (by name):</b>
{
  "preset": "MyPresetName",
  "keywords": {"PLAYLIST": "HolidayShow"}
}

<b>command/preset/triggered (by slot):</b>
{
  "slot": 5
}
</pre>

<h4>Detailed Status Topics (Configurable Frequency)</h4>
<p><i>Configure publish frequency in Advanced Settings (0 = disabled)</i></p>
<div class='fppTableWrapper selectable'>
<div class='fppTableContents' role="region" aria-labelledby="DetailedStatusTable" tabindex="0">
<table width="100%" border="1" id="DetailedStatusTable" cellpadding="1" cellspacing="1">
<thead>
<tr><th width="30%">Topic</th><th>Description</th></tr>
</thead>
<tbody>
<tr><td>playlist_details</td><td>Full playlist status as JSON. Set frequency with "MQTT Playlist Publish Frequency"</td></tr>
<tr><td>fppd_status</td><td>Complete FPPD status as JSON. Set frequency with "MQTT Status Publish Frequency"</td></tr>
<tr><td>port_status</td><td>Output port sensor data as JSON. Set frequency with "MQTT Port Status Publish Frequency"</td></tr>
</tbody>
</table>
</div>
</div>

<hr>

<h3>MQTT Topics FPP Subscribes To</h3>
<p>FPP listens to these topics to receive commands:</p>

<div class='fppTableWrapper selectable'>
<div class='fppTableContents' role="region" aria-labelledby="SubscribeTable" tabindex="0">
<table width="100%" border="1" id="SubscribeTable" cellpadding="1" cellspacing="1">
<thead>
<tr><th width="40%">Topic</th><th>Action</th></tr>
</thead>
<tbody>
<tr>
<td>/set/command/${command}</td>
<td>Runs an FPP Command. Payload should be the input given to the REST API for a command where ${command} would be something like "Volume Set"</td>
</tr>
<tr>
<td>/set/playlist/${PLAYLISTNAME}/start</td>
<td>Starts the playlist (optional payload can be index of item to start with)</td>
</tr>
<tr>
<td>/set/playlist/${PLAYLISTNAME}/next</td>
<td>Forces playing of the next item in the playlist (payload ignored)</td>
</tr>
<tr>
<td>/set/playlist/${PLAYLISTNAME}/prev</td>
<td>Forces playing of the previous item in the playlist (payload ignored)</td>
</tr>
<tr>
<td>/set/playlist/${PLAYLISTNAME}/repeat</td>
<td>If payload is "1", will turn on repeat, otherwise it is turned off</td>
</tr>
<tr>
<td>/set/playlist/${PLAYLISTNAME}/sectionPosition</td>
<td>Payload contains an integer for the position in the playlist (0 based)</td>
</tr>
<tr>
<td>/set/playlist/${PLAYLISTNAME}/stop/now</td>
<td>Forces the playlist to stop immediately. PLAYLISTNAME can be ALLPLAYLISTS.</td>
</tr>
<tr>
<td>/set/playlist/${PLAYLISTNAME}/stop/graceful</td>
<td>Gracefully stop playlist. PLAYLISTNAME can be ALLPLAYLISTS.</td>
</tr>
<tr>
<td>/set/playlist/${PLAYLISTNAME}/stop/afterloop</td>
<td>Allow playlist to finish current loop then stop. PLAYLISTNAME can be ALLPLAYLISTS.</td>
</tr>
<tr>
<td>/event/</td>
<td>Starts the event identified by the payload. The payload format is MAJ_MIN identifying the event.</td>
</tr>
<tr>
<td>/effect/start</td>
<td>Starts the effect named in the payload</td>
</tr>
<tr>
<td>/effect/stop</td>
<td>Stops the effect named in the payload or all effects if payload is empty</td>
</tr>
<tr>
<td>/light/${MODELNAME}/cmd<br>/light/${MODELNAME}/state</td>
<td>Control a Pixel Overlay Model via Home Assistant's MQTT Light interface. The Pixel Overlay Model is treated as an RGB light.</td>
</tr>
</tbody>
</table>
</div>
</div>

<hr>

<h3>Integration Examples</h3>

<h4>Subscribe to GPIO Events (Command Line)</h4>
<pre style="background-color: #f5f5f5; padding: 10px; border-radius: 5px;">
# All GPIO events
mosquitto_sub -h localhost -t "<?echo GetSettingValue('MQTTPrefix', '', '', '/'); ?>falcon/player/<?echo GetSettingValue('HostName', 'FPP'); ?>/gpio/#" -v

# Specific GPIO pin
mosquitto_sub -h localhost -t "<?echo GetSettingValue('MQTTPrefix', '', '', '/'); ?>falcon/player/<?echo GetSettingValue('HostName', 'FPP'); ?>/gpio/GPIO17/rising" -v
</pre>

<h4>Subscribe to Command Events (Command Line)</h4>
<pre style="background-color: #f5f5f5; padding: 10px; border-radius: 5px;">
# All command executions
mosquitto_sub -h localhost -t "<?echo GetSettingValue('MQTTPrefix', '', '', '/'); ?>falcon/player/<?echo GetSettingValue('HostName', 'FPP'); ?>/command/run" -v

# Command preset triggers
mosquitto_sub -h localhost -t "<?echo GetSettingValue('MQTTPrefix', '', '', '/'); ?>falcon/player/<?echo GetSettingValue('HostName', 'FPP'); ?>/command/preset/triggered" -v
</pre>

<h4>Home Assistant Integration Example</h4>
<pre style="background-color: #f5f5f5; padding: 10px; border-radius: 5px;">
# configuration.yaml
mqtt:
  binary_sensor:
    - name: "FPP GPIO17 Button"
      state_topic: "<?echo GetSettingValue('MQTTPrefix', '', '', '/'); ?>falcon/player/<?echo GetSettingValue('HostName', 'FPP'); ?>/gpio/GPIO17/rising"
      payload_on: "1"
      off_delay: 1
  
  sensor:
    - name: "FPP Status"
      state_topic: "<?echo GetSettingValue('MQTTPrefix', '', '', '/'); ?>falcon/player/<?echo GetSettingValue('HostName', 'FPP'); ?>/status"
    
    - name: "FPP Last Command"
      state_topic: "<?echo GetSettingValue('MQTTPrefix', '', '', '/'); ?>falcon/player/<?echo GetSettingValue('HostName', 'FPP'); ?>/command/run"
      value_template: "{{ value_json.command }}"

automation:
  - alias: "Log FPP Commands"
    trigger:
      - platform: mqtt
        topic: "<?echo GetSettingValue('MQTTPrefix', '', '', '/'); ?>falcon/player/<?echo GetSettingValue('HostName', 'FPP'); ?>/command/run"
    action:
      - service: logbook.log
        data:
          name: "FPP Command"
          message: "{{ trigger.payload_json.command }}"
</pre>

<hr>

<h3>Additional Resources</h3>
<ul>
<li>For complete documentation, see: <a href="https://github.com/FalconChristmas/fpp/blob/master/docs/MQTT.md" target="_blank">docs/MQTT.md</a></li>
<li>Configure MQTT settings: <a href="settings.php?tab=MQTT">Settings → MQTT</a></li>
<li>Configure GPIO inputs: <a href="gpio.php">Status/Control → GPIO Inputs</a></li>
<li>Manage Command Presets: <a href="commandPresets.php">Content Setup → Commands</a></li>
</ul>
