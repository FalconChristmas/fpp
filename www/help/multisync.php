<h3>MultiSync — Multi-System Synchronization</h3>
<p>MultiSync keeps multiple FPPs playing as one show. One system runs as <b>Player</b> and drives the timeline; other systems run as <b>Remote</b> and follow the Player’s sequence and media sync packets. This page lists every FPP seen on the local network, its role, health, versions and storage, and lets you copy files between them and group them by color.</p>

<h4>View controls (top of the table)</h4>
<ul>
    <li><b>Select Columns to Display</b> (columns icon) — Popover with checkboxes that hide/show table columns. Choices persist and are shared with filtering; the column count is the visual density you prefer.</li>
    <li><b>Sort Systems by Color</b> (palette icon) — Reorders the systems table by the dot color assigned to each row (green/yellow/red), grouping similar health together.</li>
    <li><b>Drag and drop systems to reorder them</b> (grip icon, tooltip: “Drag and drop systems to reorder them”) — Grab a row’s grip and drag; order is visual only until saved.</li>
    <li><b>Save the current display order</b> (save icon) — Persists the dragged order so the same arrangement is restored on next visit.</li>
    <li><b>Re-apply the saved display order</b> (restore icon) — Re-enables and re-applies the saved order after you have moved rows.</li>
    <li><b>Clear the saved display order</b> (clear icon) — Forgets the saved order and returns to the default (discovery) order.</li>
</ul>

<h4>Systems table — what each column means</h4>
<ul>
    <li><b>Host</b> — System hostname and, when collapsed, its color dot. The dot is also the color-picker control: click the dot to open the color palette and assign that host a color used for grouping and filtering.</li>
    <li><b>Last Rcvd</b> — Seconds since the last MultiSync heartbeat was received from that host.</li>
    <li><b>Sequence Sync</b> (4 sub-columns: Open / Start / Stop / Sync) — Counts of sequence sync packets seen; growing numbers indicate healthy sync, stuck zeros indicate the remote is not following sequence sync.</li>
    <li><b>Media Sync</b> (4 sub-columns: Open / Start / Stop / Sync) — Same for media (audio/video) sync.</li>
    <li><b>Blank Data / Ping / Plugin / FPP Cmd / Errors</b> — Remaining sync/utility/error counters per host.</li>
    <li><b>Channel Outputs / Channel Inputs</b> — Badge or icon indicating whether that host has E1.31/DDP outputs or inputs configured (hover tooltip “Channel Outputs/Inputs”).</li>
    <li><b>Versions</b> — FPP version and branch for that host; a yellow badge appears when an update is available.</li>
    <li><b>Uptime / Disk</b> — <code>UP: &lt;duration&gt; since {date}</code> and a hover tooltip with disk usage by partition (<code>diskHtml + Since:</code>). Disk warnings are surfaced here as well.</li>
    <li><b>WiFi signal</b> — <span class="wifi-icon">icon</span> with a tooltip showing quality % and RSSI dBm (e.g. “72% −42dBm”), plus the same Uptime row underneath.</li>
</ul>

<h4>File and OS copy panels (below the table)</h4>
<ul>
    <li><b>Copy OS Upgrade Files</b> — Shows pending <code>.fppos</code> upgrade files staged in the upload directory and their sources. Select a file and a destination host to push it to remotes before upgrading them.</li>
    <li><b>Upgrade OS</b> — Lists remote OS versions and which have an OS upgrade available (respecting UI level: Advanced shows beta/alpha, Developer shows nightlies per <code>populateOSUpgrade</code> logic). Use checkboxes to choose which remotes to upgrade together.</li>
    <li><b>Bulk file copy</b> — Select a local file group (Sequences, Media, Effects, etc.) and target remotes; the copy streams with progress and can be cancelled. The UI respects the donor’s “Uploaded files” staging and the receiver’s disk space.</li>
</ul>

<h4>Tips</h4>
<ul>
    <li>Use the color dots to group Players vs Remotes or by yard/zone — then <b>Sort by Color</b> to bring each group together for bulk actions.</li>
    <li>If a Remote’s sync counters stop advancing while the Player’s do, check network reachability (Ping/Errors) and that the Remote is actually in Remote mode (mode badge in the Host column).</li>
    <li>View options (columns shown, order) are per-browser; saving the order makes the arrangement stick across reloads and other browsers on the same account.</li>
</ul>
