<h3>Status Page</h3>
<p>The Status page is the dashboard of FPP. It shows what the player is doing right now, the health of the system, and the controls you use most often. The content changes with <b>FPP Mode</b>: <b>Player</b> shows scheduler and playlist controls, <b>Remote</b> shows sync status from its Player. A warning banner at the top and the header icons show storage, update availability and FPPD restart/reboot needs.</p>

<h4>Header warnings and banners</h4>
<ul>
    <li><b>Unpartitioned Space</b> (danger, top) — When the SD card has unused space, links to <b>Settings → Storage</b> to expand the filesystem or create a new storage partition.</li>
    <li><b>Warnings row</b> (<code>#warningsRow</code>, danger, hidden until needed) — Lists current FPP warnings from <code>api/system/status</code> (e.g. missing media, channel output errors). Each warning links to its help via the small icons handled in <code>fpp.js</code>’s warning helpers.</li>
    <li><b>Hostname banner</b> (<code>#bannerHostnameRow</code>) — Shown only when Host Name is still <code>FPP</code> and you have not confirmed it. Explains that each FPP on your network should have a unique Host Name and links to <b>Network Settings → Host/DNS</b> to change it, plus <b>Dismiss Warning</b>.</li>
    <li><b>Statistics banner</b> (<code>#bannerRow</code>) — Shown when <code>statsPublish == Banner</code> (default). Explains anonymous statistics collection and links to <b>Privacy Settings</b> with an <b>Enable Stats</b> button.</li>
    <li><b>Status title</b> — Shows <b>Status – Player Mode</b> or <b>– Remote Mode</b> plus the Host Name on the right.</li>
</ul>

<h4>Scheduler Info (Player mode only, top status bar)</h4>
<ul>
    <li><b>Scheduler Status</b> — Current scheduler state (idle, playing, etc.).</li>
    <li><b>Extend Current Playlist</b> — <b>Extend</b> opens a dialog to extend the end time by a chosen amount; <b>+5 min</b> extends 5 minutes instantly. Only appears while a scheduled playlist is active.</li>
    <li><b>Playlist Started at / Stop at</b> — Actual start time and calculated stop time (with stop type: Graceful / After Loop / Immediate) for the current scheduled entry.</li>
    <li><b>Next Playlist</b> — Name of the next scheduled playlist and a <b>Preview</b> dropdown: <b>Nested Table View</b> (<code>PreviewSchedule()</code>) and <b>Calendar View</b> (<code>ScheduleCalendar()</code>), plus <b>Start Next</b> (<code>StartNextScheduledItemNow()</code>) to jump to the next scheduled item immediately.</li>
</ul>

<h4>Remote Mode panel (<code>#remoteModeInfo</code>, visible only when Remote)</h4>
<ul>
    <li><b>Remote Status</b> — Current remote state.</li>
    <li><b>Player IP</b> (<code>#syncMaster</code>) — IP of the Player this Remote is synced to.</li>
    <li><b>Sequence / Media Filename</b> — The sequence and media the Player is currently driving, as seen by this Remote.</li>
    <li><b>Volume (Remote)</b> — <b>Volume</b> number (0–100) plus the same slider controls as Player (see below) but labeled <b>remoteVolume</b>. Hidden when <code>disableAudioVolumeSlider == 1</code>.</li>
    <li><b>Volume controls</b> (shared) — <b>–</b> (DecrementVolume) / slider (<code>#remoteVolumeSlider</code> / <code>#slider</code>) / <b>+</b> (IncrementVolume) plus optional <b>Mixer</b> button (sliders icon) that opens the PipeWire mixer via <code>OpenPipeWireMixer()</code> when advanced PipeWire mode is active. The speaker icon reflects level bands 0–5, 6–25, 26–75, 76–100.</li>
    <li><b>MultiSync Packet Counts</b> — Table <code>#syncStatsTable</code> (Bootstrap Table) showing per-Host <b>Last Rcvd, Sequence Sync</b> (Open/Start/Stop/Sync), <b>Media Sync</b> (Open/Start/Stop/Sync), <b>Blank Data, Ping, Plugin, FPP Cmd, Errors</b>. Buttons <b>Update</b> (<code>GetMultiSyncStats()</code>) and <b>Reset</b> (<code>ResetMultiSyncStats()</code>) plus checkbox <b>MultiSync Stats Live Update</b> (<code>PrintSettingCheckbox</code>).</li>
</ul>

<h4>Player Mode panel (<code>#playerModeInfo</code>, visible only when Player)</h4>
<ul>
    <li><b>Player Status</b> (<code>#txtPlayerStatus</code>) — idle / playing, with the current playlist entry highlighted.</li>
    <li><b>Playlist selector</b> (<code>#playlistSelect</code>) — Dropdown of all playlists via <code>PopulatePlaylists(true)</code>. Populated on load and drives the details below. <b>Repeat</b> checkbox (<code>#chkRepeat</code>) and badges <b>Randomised: On</b> and <b>Global Pause: Configured</b> (shown when the selected playlist has those features).</li>
    <li><b>Player Controls</b> (<code>#controlsSection</code>, pinned to top via <code>Zebra_Pin</code> when scrolled) — Large buttons:
        <ul>
            <li><b>Play</b> (green, <code>StartPlaylistNow()</code>) — Starts the selected playlist from the beginning.</li>
            <li><b>Previous</b> (<code>PreviousPlaylistEntry()</code> via <code>api/command/Prev Playlist Item</code>) — Jumps to previous entry.</li>
            <li><b>Next</b> (<code>NextPlaylistEntry()</code> via <code>Next Playlist Item</code>) — Jumps to next entry.</li>
            <li><b>Stop Gracefully</b> (<code>StopGracefully()</code>) — Lets the current sequence finish, then stops.</li>
            <li><b>Stop After Loop</b> (<code>StopGracefullyAfterLoop()</code>) — Finishes the current Main Playlist loop, then stops (honors Lead Out).</li>
            <li><b>Stop Now</b> (red, <code>StopNow()</code>) — Stops immediately.</li>
        </ul>
    </li>
    <li><b>Time and Progress</b> — <b>Elapsed</b> (<code>#txtTimePlayed</code>) and <b>Remaining</b> (<code>#txtTimeRemaining</code>) with a linear progress bar (<code>#progressBar</code>) and percentage. Updates each status poll.</li>
    <li><b>Volume (Player)</b> — <b>Volume</b> number (<code>#volume</code>) with the same slider / mixer controls as Remote. The slider’s title reminds you the scale is perceptual (cubic): 70 ≈ −9 dB and 50 ≈ −18 dB; per-output trim lives in PipeWire Audio Groups.</li>
    <li><b>Playlist Details</b> (<code>playlistDetails.php</code> included) — Expandable table of the selected playlist’s entries: sequence/media names, durations, and status (playing, next, etc.). Clicking a row selects it (<code>playlistSelectedEntry</code>). Rows marked with <b>*</b> are deprecated and will be auto-upgraded next time you edit (note in <code>#deprecationWarning</code>).</li>
    <li><b>Verbose Playlist Item Details</b> / <b>Playlist Auto Scroll</b> (checkboxes via <code>PrintSetting</code>) — Verbose shows more per-entry detail; Auto Scroll keeps the playing entry visible.</li>
</ul>

<h4>Bridge / E1.31 stats (<code>#bridgeModeInfo</code>, shown only when a Channel Input is configured and enabled)</h4>
<ul>
    <li><b>E1.31/DDP/ArtNet Packets and Bytes Received</b> — Three columns of universe/DMX input counters (<code>#bridgeStatistics1/2/3</code>). <b>Update</b> (<code>GetUniverseBytesReceived()</code>), <b>Reset</b> (<code>ResetUniverseBytesReceived()</code>), and checkbox <b>E1.31 Live Update</b> (<code>PrintSettingCheckbox</code>). Useful to verify incoming live data is arriving.</li>
</ul>

<h4>Tips</h4>
<ul>
    <li>Pinning of controls keeps Play/Stop reachable while scrolling a long playlist — the header height is accounted for on large viewports.</li>
    <li>Use <b>Play Here</b> vs <b>Play</b> in File Manager to test locally vs broadcast; on the Status page <b>Play</b> always broadcasts to Remotes when Player.</li>
    <li>If the E1.31 section is empty, no Channel Input is enabled — configure it under <b>Input/Output Setup → Channel Inputs</b> first.</li>
</ul>
