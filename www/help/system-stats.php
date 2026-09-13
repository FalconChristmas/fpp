<h3>System Health and Status</h3>
<p>This page gives a live dashboard of how the FPP is running. Data is gathered from the system itself (<code>api/system/status</code>, <code>system-stats.php?cpu=1</code>, <code>findmnt</code> and <code>/proc/cpuinfo</code>) and refreshed automatically: health checks run on demand, gauges and disk/player stats poll every 5 seconds, and uptime ticks every second. Sections below mirror the cards you see top-to-bottom.</p>

<h4>Unpartitioned space / Disk full banners</h4>
<ul>
    <li><b>SD card has unused space</b> (top, danger alert, when <code>settings[UnpartitionedSpace] &gt; 0</code>) — Link to <b>Settings → Storage</b> to expand the filesystem or create a new storage partition.</li>
    <li><b>High Disk Usage</b> (<code>#disk-warning</code>) — Shown when root or media partition exceeds 85 % used. Suggests cleaning up old files in <a href="filemanager.php">File Manager</a>.</li>
</ul>

<h4>System Health card</h4>
<ul>
    <li><b>Re-run</b> (top right, sync icon) — Re-runs the full health sweep via Server-Sent Events (<code>healthCheckSSE.php?timestamp=...</code>) and resets all status counters. While running the button spins.</li>
    <li><b>Summary counts</b> — Three numbers at the top: <b>Passed</b> (green), <b>Warnings</b> (amber), <b>Issues</b> (red) from the streamed <code>check.summary</code> object.</li>
    <li><b>Health items</b> — Two columns of checks. Each row shows a label with its icon and a status on the right. Status icons: <code>fa-check-circle</code> green (pass), <code>fa-check-circle</code> green-yellow (caution), <code>fa-exclamation-circle</code> amber (warn), <code>fa-times-circle</code> red (fail), spinner gray (loading). Clicking a row with warning details expands a bulleted list of footnotes; the chevron indicates expandability.</li>
    <li><b>Static checks</b> (always shown, keep declared order):
        <ul>
            <li>Left: <b>FPPD Daemon</b>, <b>FPPD Warnings</b>, <b>Unique Hostname</b>, <b>Root Filesystem</b>, <b>Time Sync (NTP)</b></li>
            <li>Right: <b>Default Gateway</b>, <b>Gateway Reachable</b>, <b>Internet Access</b>, <b>DNS Resolution</b>, <b>Browser Time Sync</b></li>
        </ul>
        Static rows that never receive a result show <b>Skipped</b> when the stream ends.
    </li>
    <li><b>Conditional checks</b> (hidden until a result arrives, rendered at the tail of their column):
        Left tail: <b>PipeWire Audio</b>, <b>Scheduler</b>, <b>Unknown Plugins</b>; Right tail: <b>GStreamer</b>, <b>Media Partition</b>. They appear only when the backend emits them, so an empty column does not create a gap in the always-shown block.</li>
    <li><b>Recovery actions</b> (<code>#healthRecoveryActions</code>, below the lists) — Appear only when <b>PipeWire</b> or <b>GStreamer</b> is degraded:
        <ul>
            <li>If <b>PipeWire</b> is <code>warn</code> or <code>fail</code>: yellow callout <i>“The PipeWire audio stack needs attention…”</i> with <b>Restart Audio Services</b> → <code>POST api/pipewire/audio/services/restart</code> (which bounces PipeWire and then restarts FPPD). Interrupts playback; auto re-checks after 5 s.</li>
            <li>Else if <b>GStreamer</b> alone is <code>warn</code>/<code>fail</code>: callout <i>“GStreamer reported a problem…”</i> with <b>Restart FPPD</b> → <code>GET api/system/fppd/restart</code>, also re-checking after 5 s.</li>
        </ul>
    </li>
    <li><b>Badges on warnings</b> — Inline <code>[FAIL]</code>/<code>[WARN]</code> markers in command output are highlighted (red bold / amber bold) and counted into badges beside the command heading, its hot link and the group tab (compact “3” vs full “3 failures”). Only indented explanation lines under a failing verdict are tinted.</li>
</ul>

<h4>CPU / Memory / Temperature gauges</h4>
<ul>
    <li><b>CPU Usage</b> (left) — Circular gauge 0–100 % with thresholds green &lt;60, amber 60–80, red &gt;80. On Linux it is derived from <code>/proc/stat</code> deltas with EMA smoothing (α=0.4, ~10 s to 64 %, ~20 s to 87 %); on Mac it uses the <code>?cpu=1</code> endpoint’s <code>cpu.mac</code>. Stroke is <code>normalized*2.827 @ 282.7</code>.</li>
    <li><b>Memory Usage</b> (center) — Stacked gauge showing <b>Used</b> (green/amber/red same thresholds), <b>Buffer/Cache</b> (info blue) and <b>Free</b> (muted). Text below shows <code>used (+cache) / free</code> and total via <code>formatMemBytes</code>. Hover segments show exact bytes. The help popover (?) explains Used vs Buffer/Cache vs Free and notes that high cache is normal.</li>
    <li><b>Temperature</b> (right) — CPU temperature from the <code>Temperature</code> sensor in <code>api/system/status</code>. Thresholds 60 °C (amber), 80 °C (red), max 100 °C; in Fahrenheit mode (via <code>temperatureInF</code>) they become 140 °F / 176 °F / max 212 °F and the label shows °F.</li>
</ul>

<h4>Fan Monitoring</h4>
<ul>
    <li>Hidden when no <code>valueType === 'FanSpeed'</code> sensors exist. Otherwise shows each fan’s label and RPM (e.g. “CPU Fan — 2450 RPM”) from the same sensor list, updated together with the other gauges.</li>
</ul>

<h4>Disk Utilization</h4>
<ul>
    <li><b>Root Partition</b> — Device from <code>findmnt -n -o SOURCE /</code>, plus <code>used / total (free free)</code> and a horizontal bar (green &lt;70, amber 70–85, red &gt;85). Values come from <code>disk_total_space / disk_free_space</code> on <code>/</code>.</li>
    <li><b>Media Partition</b> — Shown only when <code>findmnt</code> reports a different source for the media directory vs root. Same bars and colors, with its own warning at 85 % that also triggers the top disk-warning banner.</li>
</ul>

<h4>System Uptime</h4>
<ul>
    <li><b>Counters</b> — Days / Hours / Minutes / Seconds, zero-padded to two digits, seeded from <code>systemUptimeTotalSeconds</code> and ticked locally every second. Below the counters, <b>System started: {uptime -s}</b> (omitted on Mac) shows the wall-clock boot time from <code>uptime -s</code>.</li>
</ul>

<h4>System Busyness (Load Average)</h4>
<ul>
    <li><b>Three bars</b> — 1-min, 5-min and 15-min averages from <code>sys_getloadavg()</code> normalized to core count (<code>cores</code> from <code>/proc/cpuinfo</code>). Each bar’s percent is <code>min(load/cores*100,100)</code> with colors green &lt;60, amber 60–80, red &gt;80, and a numeric <code>load → 0.00</code> label. The help popover (?) explains thresholds as <b>Running smoothly</b> (&lt; 0.70 × cores), <b>Busy</b> (0.70–0.90 × cores), <b>Overloaded</b> (&gt; 0.90 × cores) using a highway-lanes analogy.</li>
    <li><b>Status line</b> — Below the bars, <code>Running smoothly / Busy / Overloaded</code> with matching icon and color, driven by the 1-min bar’s percent (90+ overloaded, 70+ busy).</li>
</ul>

<h4>Player Statistics (bottom cards)</h4>
<ul>
    <li>Counts fetched from: <code>api/playlists</code> → <b>Playlists</b>, <code>api/schedule</code> → <b>Schedules</b>, <code>api/files/sequences?nameOnly=1</code> → <b>Sequences</b>, <code>api/files/music</code> → <b>Audio</b>, <code>api/files/videos</code> → <b>Videos</b>, <code>api/files/effects</code> → <b>Effects</b>, <code>api/files/scripts</code> → <b>Scripts</b>. Each updates independently as its API returns.</li>
</ul>

<h4>Tips</h4>
<ul>
    <li>Run <b>Re-run</b> after fixing a network or time issue — the static rows will flip from Skipped/Loading to pass/fail as results stream in.</li>
    <li>High <b>Buffer/Cache</b> in Memory is healthy; free memory low alone is not a problem unless Used is also high.</li>
    <li>If <b>Busyness</b> stays Busy/Overloaded, reduce concurrent effects or check for a tight script loop; the 15-min bar shows longer-term pressure.</li>
</ul>
