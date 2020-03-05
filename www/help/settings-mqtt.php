<?
require_once('../config.php');
?>
<center><b>MQTT Settings</b></center>
<hr>
<ul>
<li>MQTT events will be published to "<? echo GetSettingValue('MQTTPrefix', '', '', '/'); ?>falcon/player/<? echo GetSettingValue('HostName', 'FPP'); ?>/" with playlist events being in the "playlist" subtopic.
<li><b>CA file</b> is the full path to the signer certificate.  Only needed if using mqtts server that is self signed.
<li><b>Publish Frequence</b> should be zero (disabled) or the number of seconds between periodic mqtt publish events<br/>
<li>FPP will respond to certain events:
</ul>
<div class='fppTableWrapper selectable'>
<div class='fppTableContents'>
<table width = "100%" border="1" cellpadding="1" cellspacing="1">
<thead>
<tr><th>Topic</th><th>Action</th></tr>
</thead>
<tbody>
<tr>
<td><? echo GetSettingValue('MQTTPrefix', '', '', '/'); ?>falcon/player/<? echo GetSettingValue('HostName', 'FPP'); ?>/set/playlist/${PLAYLISTNAME}/start</td><td>Starts the playlist (optional payload can be index of item to start with)</td>
</tr>
<tr>
<td><? echo GetSettingValue('MQTTPrefix', '', '', '/'); ?>falcon/player/<? echo GetSettingValue('HostName', 'FPP'); ?>/set/playlist/${PLAYLISTNAME}/next</td><td>Forces playing of the next item in the playlist (payload ignored)</td>
</tr>
<tr>
<td><? echo GetSettingValue('MQTTPrefix', '', '', '/'); ?>falcon/player/<? echo GetSettingValue('HostName', 'FPP'); ?>/set/playlist/${PLAYLISTNAME}/prev</td><td>Forces playing of the previous item in the playlist (payload ignored)</td>
</tr>
<tr>
<td><? echo GetSettingValue('MQTTPrefix', '', '', '/'); ?>falcon/player/<? echo GetSettingValue('HostName', 'FPP'); ?>/set/playlist/${PLAYLISTNAME}/repeat</td><td>If payload is "1", will turn on repeat, otherwise it is turned off</td>
</tr>
<tr>
<td><? echo GetSettingValue('MQTTPrefix', '', '', '/'); ?>falcon/player/<? echo GetSettingValue('HostName', 'FPP'); ?>/set/playlist/${PLAYLISTNAME}/sectionPosition</td><td>Payload contains an integer for the position in the playlist (0 based)</td>
</tr>
<tr>
<td><? echo GetSettingValue('MQTTPrefix', '', '', '/'); ?>falcon/player/<? echo GetSettingValue('HostName', 'FPP'); ?>/set/playlist/${PLAYLISTNAME}/stop/now</td><td>Forces the playlist to stop immediately.  PLAYLISTNAME can be ALLPLAYLISTS.</td>
</tr>
<tr>
<td><? echo GetSettingValue('MQTTPrefix', '', '', '/'); ?>falcon/player/<? echo GetSettingValue('HostName', 'FPP'); ?>/set/playlist/${PLAYLISTNAME}/stop/graceful</td><td>Gracefully stop playlist.  PLAYLISTNAME can be ALLPLAYLISTS.</td>
</tr>
<tr>
<td><? echo GetSettingValue('MQTTPrefix', '', '', '/'); ?>falcon/player/<? echo GetSettingValue('HostName', 'FPP'); ?>/set/playlist/${PLAYLISTNAME}/stop/afterloop</td><td>Allow playlist to finish current loop then stop.  PLAYLISTNAME can be ALLPLAYLISTS.</td>
</tr>
<tr>
<td><? echo GetSettingValue('MQTTPrefix', '', '', '/'); ?>falcon/player/<? echo GetSettingValue('HostName', 'FPP'); ?>/event/</td><td>Starts the event identified by the payload.   The payload format is MAJ_MIN identifying the event.</td>
</tr>
<tr>
<td><? echo GetSettingValue('MQTTPrefix', '', '', '/'); ?>falcon/player/<? echo GetSettingValue('HostName', 'FPP'); ?>/effect/start</td><td>Starts the effect named in the payload</td>
</tr>
<tr>
<td><? echo GetSettingValue('MQTTPrefix', '', '', '/'); ?>falcon/player/<? echo GetSettingValue('HostName', 'FPP'); ?>/effect/stop</td><td>Stops the effect named in the payload or all effects if payload is empty</td>
</tr>
<tr>
<td><? echo GetSettingValue('MQTTPrefix', '', '', '/'); ?>falcon/player/<? echo GetSettingValue('HostName', 'FPP'); ?>/light/${MODELNAME}/cmd<br>
    <? echo GetSettingValue('MQTTPrefix', '', '', '/'); ?>falcon/player/<? echo GetSettingValue('HostName', 'FPP'); ?>/light/${MODELNAME}/state</td><td>Control a Pixel Overlay Model via Home Assistant's MQTT Light interface.  The Pixel Overlay Model is treated as a RGB light.</td>
</tr>
</table>
</div>
</div>
