<center>
<b>Log Levels</b><br>
<table border='1' cellpadding='4' cellspacing='1'>
<tr><th>Level</th><th>Description</th></tr>
<tr><td>Warn</td><td>Errors and warnings</td></tr>
<tr><td>Info</td><td>Informational logs about state of the system.</td></tr>
<tr><td>Debug</td><td>The Debug level generates verbose logs useful for debugging issues or configurations.  This should not be enabled during normal use unless you are trying to track down an issue.</td></tr>
<tr><td>Excessive</td><td>The Excessive level generates very detailed logs which are excessive enough to affect performance.  Turning on Excessive level logging may cause parts of FPP to not function properly due to the excessive amount of logging.  It is normally only used in specific cases with specific Log Masks configured.</td></tr>
</table>
<br>
<b>Log Masks</b><br>
<table border='1' cellpadding='4' cellspacing='1'>
<tr><th>Mask</th><th>Description</th></tr>
<tr><td>ALL</td><td>The ALL meta-value enables all debug logs at the selected level.  This can be very verbose and is normally only recommended at the Warn or Info levels.</td></tr>
<tr><td>Most</td><td>The Most meta-value enables all debug logs except for Channel Data.</td></tr>
<tr><td>Channel Data</td><td>Log every time that channel data is sent out to controllers</td></tr>
<tr><td>Channel Outputs</td><td>Log info about the Channel Outputs themselves</td></tr>
<tr><td>Commands</td><td>Log received commands and their replies</td></tr>
<tr><td>Control Interface</td><td>Log info from the remote control interface</td></tr>
<tr><td>E1.31 Bridge</td><td>Log info about E1.31 bridge mode</td></tr>
<tr><td>Effects</td><td>Log info about Effects sequences</td></tr>
<tr><td>Events</td><td>Log info about Events triggered</td></tr>
<tr><td>General</td><td>General log info for miscellaneous areas of FPP</td></tr>
<tr><td>GPIO</td><td>Log GPIO Input events</td></tr>
<tr><td>Media Outputs</td><td>Log info about the media players that FPP uses.</td></tr>
<tr><td>MultiSync</td><td>Log info when syncing playback on multiple Pi's</td></tr>
<tr><td>Playlists</td><td>Log info when parsing playlists</td></tr>
<tr><td>Plugins</td><td>Plugin log info</td></tr>
<tr><td>Scheduler</td><td>Log info when scheduling playlists</td></tr>
<tr><td>Sequence Parser</td><td>Log info when playing sequences</td></tr>
<tr><td>Settings</td><td>Log info when parsing or applying settings</td></tr>
</table>
</center>
