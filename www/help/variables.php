<h3>Variables</h3>
<p>Variables are named values that stick around so different parts of FPP can share data — a recurring task can fetch a weather feed into a variable, and an <code>If</code> command or playlist entry can read it instantly without waiting for a network fetch.</p>
<p>Use them two ways: drop <code>%VAR:name%</code> into any command’s text field and it is swapped for the current value when that command runs (for example a <code>URL</code> command with <code>%VAR:apiKey%</code>), or pick a Variable directly inside an <code>If</code> command’s condition editor — no <code>%VAR:</code> needed there.</p>

<h4>Search and sorting</h4>
<ul>
    <li><b>Search variables</b> (text, top) — Instant, case-insensitive substring filter on <b>Name</b> across all three tables. Typed text highlights matches; when nothing matches a “No matches for …” row appears. The filter is reapplied after each 3-second auto-refresh so it survives updates.</li>
    <li><b>Sortable headers</b> — Click <b>Name</b>, <b>Value</b> or <b>Last Updated</b> in any table to sort that table. Click again to toggle ascending ↔ descending. The arrow (▲/▼) shows the active column and direction. Sorting is numeric-aware (“2” before “10”) and otherwise alphabetical.</li>
    <li><b>Responsive layout</b> — On narrow screens (&lt;768 px) the <b>Value</b> column is hidden and an eye icon is forced on every row — tap the eye to view the full value. On very wide screens (≥1800 px) Name and Value columns expand to 700 px. Truncation limits adapt to the breakpoint: mobile shows 22 topic chars, normal shows 44 value / 42 name chars, wide shows 80/78.</li>
</ul>

<h4>User Variables</h4>
<p>Values you create yourself via the <b>Set Variable</b> command (triggered directly, from a GPIO input, scheduler entry, the <code>api/variables</code> API, or a <a href="recurringtasks.php">Recurring Task</a>). Table columns, left to right:</p>
<ul>
    <li><b>Name</b> (fixed 380/700 px, <code>text-nowrap</code>, overflow hidden) — Rendered as <code>code</code>; long names are tail-truncated with a leading <code>…</code> keeping the meaningful tail (e.g. <code>…/temperature</code>) and the full name in a hover tooltip. The most distinguishing part is the tail, so truncation keeps it.</li>
    <li><b>Copy button</b> (40 px, dedicated column) — Copies the full name to the clipboard; briefly swaps the copy icon to a green check. Every row has one.</li>
    <li><b>Value</b> (fixed 380/700 px) — One-line preview, already capped by the backend (<code>kInlineValueMaxBytes ≈ 200</code>). If the value exceeds the display cap it is tail-truncated with <code>…</code> plus a muted <code>(N bytes)</code> note; hover shows the truncated preview. Server-truncated values never show inline text beyond the byte count — use <b>View</b> for the full value. Mobile hides this column entirely.</li>
    <li><b>View (eye)</b> (46 px) — Shows only when the value is clipped (server-truncated or longer than the display cap) or on mobile where the Value column is hidden. Clicking fetches the current exact value via <code>GET api/variables/{name}</code> (fresh, not the cached table preview) and opens a dialog with the full text, wrap-enabled. The dialog offers <b>Copy to Clipboard</b> and <b>Close</b>. The row’s cached preview may be stale by the time you click, which is why View re-fetches.</li>
    <li><b>Last Updated</b> (110 px) — Compact single-unit form: <code>8s</code>, <code>5m</code>, <code>3h</code>, <code>2d</code>, or <code>never</code> when never set. Recomputed every 3 seconds.</li>
    <li><b>Storage</b> (100 px, User table only) — Icon plus two buttons: save icon <b>Persisted</b> (blue, survives <code>fppd</code> restart, stored on disk) vs memory icon <b>In-memory only</b> (gray, lost on restart). Then <b>Clear</b> (eraser) — resets the value (removes persisted copy) but keeps the row — and <b>Delete</b> (trash, red) — removes the variable entirely (recreatable via Set Variable). Both confirm before acting and use <code>POST/DELETE api/variables/{name}</code>.</li>
    <li><b>Empty state</b> — “No variables defined yet.” when none exist; “Error loading variables.” on fetch failure.</li>
</ul>

<h4>FPP Read-only Variables</h4>
<p>Computed by <code>ComputeFppStatusVariables()</code> in <code>Variables.cpp</code>; you cannot write them. Same Name/copy/Value/View/Last Updated columns as above, but no Storage column. Each Name is prefixed with an info icon whose tooltip shows its fixed meaning (keep in sync with that C++ function):</p>
<ul>
    <li><b>fpp_status</b> — Numeric status code: 0 idle, 1 playing, 2–4 stopping variants, 5 paused.</li>
    <li><b>fpp_status_name</b> — Text status: idle, playing, playing media, playing background, stopping gracefully / after loop / now, or paused. “playing media/background” means a stream slot is active outside a playlist (Play Media, PSA, background music) while the numeric status stays idle.</li>
    <li><b>fpp_mode_name</b> — Current mode, e.g. player, bridge, master, remote.</li>
    <li><b>fpp_volume</b> — Audio output volume 0–100.</li>
    <li><b>fpp_multisync</b> — 1 if MultiSync enabled, 0 otherwise.</li>
    <li><b>fpp_uptime_seconds</b> — Seconds since <code>fppd</code> started.</li>
    <li><b>fpp_is_playing</b> — 1 if a playlist is playing, 0 otherwise.</li>
    <li><b>fpp_was_scheduled</b> — 1 if the current/most recent playlist was started by the Scheduler.</li>
    <li><b>fpp_scheduler_enabled</b> — 1 if Scheduler is enabled.</li>
    <li><b>fpp_current_time</b> — Local time HH:MM (24h) — same format as the “Time” If-condition Source.</li>
    <li><b>fpp_current_date</b> — Local date YYYY-MM-DD.</li>
    <li><b>fpp_day_of_week</b> — Local day name, e.g. Monday.</li>
    <li><b>fpp_current_month</b> — Local month name, e.g. July.</li>
    <li><b>fpp_warning_count</b> — Number of active system warnings.</li>
    <li><b>fpp_next_playlist</b> — Next Scheduler playlist name, or empty.</li>
    <li><b>fpp_next_playlist_start</b> — Start time of that next playlist.</li>
    <li><b>fpp_current_playlist</b> — Currently loaded playlist name.</li>
    <li><b>fpp_current_playlist_count</b> — Entries in the current playlist.</li>
    <li><b>fpp_current_playlist_index</b> — Index of the currently playing entry.</li>
    <li><b>fpp_current_sequence</b> — Filename of the currently playing sequence.</li>
    <li><b>fpp_current_song</b> — Filename of the currently playing media/song.</li>
    <li><b>fpp_seconds_played</b> — Seconds played so far in the current entry.</li>
    <li><b>fpp_seconds_remaining</b> — Seconds remaining.</li>
    <li><b>fpp_time_elapsed / fpp_time_remaining</b> — Same as above formatted as text.</li>
    <li><b>fpp_repeat_mode / fpp_random</b> — Current repeat and shuffle modes.</li>
</ul>

<h4>MQTT Read-only Variables</h4>
<p>Mirrored from the MQTT broker cache (<code>GET api/variables?mqtt=true</code>). Same columns as FPP. When empty the table shows: “No MQTT messages cached yet. Go to <a href='settings-mqtt.php'>MQTT Settings</a> to connect to a broker and subscribe to a topic (or <code>#</code> for everything) — any topic this device receives a message on will appear here.” These are the same values the <code>api/fppd/mqtt/cache</code> endpoint and playlist branching can read. Only live, writable User Variables (not <code>fpp_</code>/MQTT) are suggested as a Recurring Task’s <b>Result Variable</b> target for the same reason.</p>

<h4>Tips</h4>
<ul>
    <li>Keep names short and without spaces so <code>%VAR:name%</code> stays readable when pasted into other pages.</li>
    <li>On mobile, rely on the eye icon — the Value column is intentionally hidden and the eye is the only way to see the value.</li>
    <li>Tables refresh automatically every 3 seconds and on breakpoint crossings; no manual Reload needed.</li>
</ul>
