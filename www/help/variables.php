<h3>Variables</h3>
<p>Variables are named values that stick around so different parts of FPP can share data — a recurring task can fetch a weather feed into a variable, and an <code>If</code> command or playlist entry can read it instantly without waiting for a network fetch.</p>
<p>Use them two ways: drop <code>%VAR:name%</code> into any command’s text field and it is swapped for the current value when that command runs (for example a <code>URL</code> command with <code>%VAR:apiKey%</code>), or pick a Variable directly inside an <code>If</code> command’s condition editor — no <code>%VAR:</code> needed there.</p>

<h4>Search and sorting</h4>
<ul>
    <li><b>Search variables</b> — Type part of a name to filter all three tables as you type (case doesn’t matter). The filter stays applied while the tables refresh.</li>
    <li><b>Sortable headers</b> — Click <b>Name</b>, <b>Value</b> or <b>Last Updated</b> to sort that table; click again to reverse. The arrow shows the current column and direction. Numbers sort as numbers (“2” before “10”).</li>
    <li><b>Responsive layout</b> — Name and Value share the width the screen has to offer; text that doesn’t fit is shortened with <code>…</code> rather than wrapped, so each row stays one line. On phones the <b>Value</b> column is hidden and the eye icon appears on every row — tap it to view the value.</li>
</ul>

<h4>User Variables</h4>
<p>Values you set yourself with the <b>Set Variable</b> command — run directly, from a GPIO input, a scheduler entry, the API, or a <a href="recurringtasks.php">Recurring Task</a>. Columns, left to right:</p>
<ul>
    <li><b>Name</b> — Long names are shortened from the start with a leading <code>…</code>, keeping the meaningful tail (e.g. <code>…/temperature</code>); hover a shortened name for the full text. The most distinguishing part is the tail, so truncation keeps it.</li>
    <li><b>Copy</b> — Copies the full name to the clipboard, ready to paste into <code>%VAR:name%</code>. The icon briefly turns into a green check to confirm.</li>
    <li><b>Value</b> — A one-line preview. A value that doesn’t fit is shortened with <code>…</code>; hover it to see more. Large values show only their first part plus a <code>(N bytes)</code> size note — use <b>View</b> for the whole thing. Phones hide this column.</li>
    <li><b>View (eye)</b> — Appears only when the value on screen is shortened, and on every row on phones. Opens a dialog with the complete, up-to-the-moment value and a <b>Copy to Clipboard</b> button.</li>
    <li><b>Last Updated</b> — How long ago the value last changed: <code>8s</code>, <code>5m</code>, <code>3h</code>, <code>2d</code>, or <code>never</code>.</li>
    <li><b>Storage</b> — A blue save icon means the value is <b>persisted</b> (kept across an FPP restart); a gray memory icon means <b>in-memory only</b> (gone after a restart). Whether a variable persists is chosen in the Set Variable command.</li>
    <li><b>Clear</b> (eraser) — Empties the value but keeps the variable in the list. <b>Delete</b> (red trash) — Removes it entirely; Set Variable can recreate it later. Both ask for confirmation.</li>
</ul>

<h4>FPP Read-only Variables</h4>
<p>Live values FPP maintains about itself — you can read them anywhere a User Variable can be read, but not change them. Same columns as above without Storage. Hover the info icon beside a name for its meaning:</p>
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
<p>The most recent message received on each MQTT topic this player is subscribed to, named by topic. Read-only, like the FPP variables. If the table is empty, go to <a href='settings-mqtt.php'>MQTT Settings</a> to connect to a broker and subscribe to a topic (or <code>#</code> for everything). Because these are read-only, a Recurring Task’s <b>Result Variable</b> can only target a User Variable.</p>

<h4>Tips</h4>
<ul>
    <li>Keep names short and without spaces so <code>%VAR:name%</code> stays readable when pasted into other pages.</li>
    <li>On a phone, tap the eye icon to see a value — the Value column is hidden to make room for names.</li>
    <li>Tables refresh themselves every few seconds; there is no Reload button to press.</li>
</ul>
