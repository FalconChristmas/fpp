<h3>Playback</h3>
<p>Controls how FPP plays sequences and media. Advanced scheduler options appear when the UI Level is set to Advanced.</p>

<h4>Playback</h4>
<ul>
    <li><b>Send MultiSync Packets</b> (checkbox, Player only) — When enabled, this Player sends sync packets so Remotes stay in time during a show. Turn it off only if this device should never lead Remotes. <i>Requires FPPD restart.</i></li>
    <li><b>Pause background effects during FSEQ playback</b> (checkbox) — Background effects run while a sequence plays. Check this to pause them while any sequence is running and automatically resume when the sequence stops.</li>
    <li><b>Blank between sequences</b> (checkbox) — Sends all channels to off between sequences in a playlist. Useful if your sequences do not end black and you want lights off between songs.</li>
    <li><b>Blank screen on startup</b> (checkbox, Raspberry Pi) — Blanks the HDMI/composite display after boot so boot text does not show on a projector. The screen stays blank until video plays or the timeout below expires.</li>
    <li><b>Inactivity timeout for screen blanking</b> (number, minutes, 0–60) — Shown when Blank screen on startup is enabled. How many minutes of no video before the display is blanked again. 0 means never re-blank after startup.</li>
    <li><b>Open/Start Delay</b> (number, ms, Player only, with MultiSync enabled) — Extra pause between the Player telling Remotes to open a file and actually starting it. Gives Remotes time to load large videos. Default 0. For heavy MP4 on Remotes, 650–800 ms is typical. <i>Requires FPP-3.2+ on both sides and a restart.</i></li>
    <li><b>Remote Media/Sequence Offset</b> (number, ms, Remote only) — Shifts timing on this Remote only. Positive moves the Remote ahead, negative moves it behind. Affects both sequence and media. <i>Requires restart on the Remote.</i></li>
</ul>

<h4>Scheduler</h4>
<ul>
    <li><b>Disable Scheduler</b> (checkbox, Advanced) — Turns off all scheduled playback without deleting entries. Useful for testing or off-season. <i>Requires restart.</i></li>
    <li><b>Protect UI-Started Playlists from Schedule Override</b> (checkbox, Advanced) — When enabled, playlists you start by hand from the Status page or UI are not stopped by the next scheduled entry. Playlists started via API/MQTT can set their own protection separately.</li>
    <li><b>Scheduler max timeframe to schedule out</b> (dropdown, Advanced) — How far ahead FPP pre-calculates the schedule. Options: 4 Weeks, 8 Weeks, 3 Months, 6 Months, 1 Year. Default 4 Weeks. Longer ranges consider holidays further out but need a restart to recompute. <i>Requires restart.</i></li>
    <li><b>Granular Scheduling</b> (checkbox, Advanced) — Adds seconds to schedule start/end times in the Scheduler UI. When off, entries are on minute boundaries. Turning it on reloads the UI.</li>
</ul>

<p><b>Tips:</b> Changes that say <i>Requires restart</i> show a “FPPD Restart Required” banner — click Restart FPPD or reboot to apply.</p>
