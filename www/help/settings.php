<h3>FPP General Settings</h3>

<p><b>Blank screen on startup</b> - Enable/Disable a screen blanker at FPP startup.  If you are using a projector with your Pi to display video then you will want to enable this setting so that the text console does not show on your projector when you are not playing videos</p>

<p><b>Force Analog Audio Output</b> - Force audio output out the 1/8-inch stereo jack even when using HDMI for video</p>

<p><b>Pi 2x16 LCD Enabled</b> - Enable use of a LCD display attache to the Pi for status and control</p>

<p><b>Always transmit channel data</b> - Force transmission of channel data out to controllers whenever FPP is running.  FPP will normally only transmit data when there is a sequence playing or the system is running in Bridge mode or a Pixel Overlay model is enabled.  Some controllers go into test mode when not receiving data.  This setting causes FPP to always send data so the controllers do not go into test mode.</p>

<p><b>Audio Output Device</b> - Allows controlling whether audio is sent out via the onboard soundcard or a USB attached soundcard or FM transmitter.</p>

<p><b>Log Level</b> - The E1.31 output can drive up to 128 universes (65,536 channels) over the Pi's ethernet network interface.</p>
<center>
<b>Log Levels</b><br>
<table border='1' cellpadding='4' cellspacing='1'>
<tr><th>Level</th><th>Description</th></tr>
<tr><td>Warn</td><td>Errors and warnings</td></tr>
<tr><td>Info</td><td>Informational logs about state of the system.</td></tr>
<tr><td>Debug</td><td>The Debug level generates verbose logs useful for debugging issues or configurations.  This should not be enabled during normal use unless you are trying to track down an issue.</td></tr>
<tr><td>Excessive</td><td>The Excessive level generates very detailed logs which are excessive enough to affect performance.  Turning on Excessive level logging may cause parts of FPP to not function properly due to the excessive amount of logging.  It is normally only used in specific cases with specific Log Masks configured.</td></tr>
</table>
</center>

<p><b>Log Mask</b> - The E1.31 output can drive up to 128 universes (65,536 channels) over the Pi's ethernet network interface.</p>
<center>
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
