<h3>Recurring Tasks</h3>
<p>Fetch data <em>ahead of time</em> and park it in a <a href="variables.php">User Variable</a> so a time-critical playlist or GPIO event can read it instantly. Running a slow URL fetch or script right when it matters would add a delay; a recurring task does it on a fixed interval in the background instead.</p>
<p>Each task runs either a <b>Command Preset</b> (which can fire many commands at once) or any single <b>FPP Command</b>. An <b>FPP Command</b> task can optionally save its (optionally filtered) result into a Variable for an <code>If</code> command to read with zero fetch delay — for example a <code>URL</code> command polling a weather API or a sensor’s web endpoint every few minutes.</p>

<h4>Header and buttons</h4>
<ul>
    <li><b>Delete</b> (top right, disabled until a row is selected) — Removes the selected task rows from the table. Requires <b>Save</b> to make permanent. Selection is via the row-click handler (<code>HandleTableRowMouseClick</code>) which toggles <code>selectedEntry</code> and enables/disables this button.</li>
    <li><b>Add</b> (green +) — Appends a default row: Enabled, Name empty, Interval 60 sec, Type Command Preset, no preset/command, no result variable, Filter None, no multisync. The new row is created via <code>AddTask({})</code> → <code>AddTableRowFromTemplate('tblTasksBody')</code> → <code>FillInTaskRow</code>.</li>
    <li><b>Save</b> (green) — Validates every row (missing Name, duplicate names, empty preset/command), then writes <code>api/configfile/recurringtasks.json</code> via <code>Post</code> and immediately POSTs <code>api/fppd/recurringtasks</code> with <code>{command:'reload'}</code> so the timer schedule is rebuilt without restarting <code>fppd</code>. Shows “Recurring tasks saved and reloaded.” and refreshes status via <code>LoadTaskStatus</code>.</li>
</ul>

<h4>Table columns (each row)</h4>
<ul>
    <li><b>Enabled</b> (checkbox, checked by default) — Unchecked tasks are ignored by the scheduler daemon until re-enabled.</li>
    <li><b>Name</b> (text, size 14, max 64) — Unique identifier. Used as the task’s key in <code>api/fppd/recurringtasks</code> status and for de-duplication on save; duplicate names are rejected with an alert.</li>
    <li><b>Interval (sec)</b> (number, min 30, step 1) — How often the task runs, in seconds. Minimum enforced is 30 s; values below are rejected and the field fills with a subtle danger background (<code>intervalInvalid</code> using <code>var(--bs-danger-bg-subtle)</code>) rather than a plain border so it reads clearly. Internally stored as <code>intervalMS = sec*1000</code> (default 60000 ms). The red highlight appears both immediately on load (for bad saved values) and live as you type via the <code>input</code> listener calling <code>UpdateIntervalField</code>.</li>
    <li><b>Type</b> (dropdown) — <b>Command Preset</b> vs <b>FPP Command</b>. Switching toggles which cell is visible (<code>ptTmplPresetCell</code> vs <code>ptTmplCommandCell</code>). Command Preset tasks hide Test Run and View icons because a preset can run many commands with no single result to show; FPP Command tasks restore them.</li>
    <li><b>Preset / Command</b> —
        <ul>
            <li><b>Command Preset</b> — Dropdown of preset names fetched once from <code>api/commandPresets?names=true</code> and cached in <code>presetNameList</code> (mirrors <code>commandPresets.php</code>). If the saved preset no longer exists it is prepended as a disabled <code>(unavailable)</code> option so the row’s actual saved value is not silently lost. Below the select is a <b>Test Run</b> button (hidden for this type per above).</li>
            <li><b>FPP Command</b> — A shared command picker built by <code>FillInCommandTemplate</code> (same helper as Schedule and Command Presets), plus an <b>Edit</b> button that opens the singleton command editor via <code>EditTaskCommandAndSettings</code>, a <b>Test Run</b> button, an info tooltip icon, an <b>Args</b> preview table, and a hidden <code>cmdTmplJSON</code> holding the full command + args. The Edit flow appends the Result Variable/Filter section (see below) into the same command-editor dialog rather than nesting a second dialog, to avoid corrupting the outer editor — the real inputs are temporarily relocated into the dialog and moved back on close, and the one-line summary is refreshed via <code>UpdateTaskAdvancedSummary</code>.</li>
        </ul>
    </li>
    <li><b>Status + View</b> — Status text plus an eye button:
        <ul>
            <li><code>never run</code> (muted) when the task has not yet run.</li>
            <li><code>OK @ {local time}</code> (green) or <code>Error @ {local time}</code> (red, hover shows <code>lastError</code>) after at least one run, sourced from <code>api/fppd/recurringtasks</code> mapped by task name.</li>
            <li><b>View eye</b> (hidden until the row targets a Result Variable) — Opens the current value of that Variable with staleness context. It reads from <code>GET api/variables</code> (the listing with <code>lastUpdated</code>) rather than the plain-text value, so the popup can show <b>Last updated: 8s ago — this is the Variable’s current value, which may predate the most recent Test Run if that run errored</b> plus the value itself with Copy to Clipboard. The eye is shown whenever the task has a <code>resultVariable</code>, even before the first run, so you can see what another writer has left there.</li>
        </ul>
    </li>
</ul>

<h4>Result Variable / Filter (per-task, for FPP Command type only)</h4>
<p>Collapsed to a one-line summary in the row — <code>→ code</code> or <code>no result variable, filter: …</code> — and fully editable only inside the command editor’s appended section (<code>Result Variable / Filter</code> heading). The summary updates on dialog close via <code>UpdateTaskAdvancedSummary</code>.</p>
<ul>
    <li><b>Result → Variable</b> (text, size 16, placeholder “(optional)”, datalist <code>#ptResultVariableNames</code>) — Variable this task’s (optionally filtered) result is saved into, overwriting any existing value of the same name. Leave blank to run without saving. Suggestions are loaded fresh from <code>GET api/variables</code> via <code>LoadResultVariableNames</code> so newly created/renamed variables appear immediately; only real writable User Variables are offered (not <code>fpp_</code>/MQTT read-only).</li>
    <li><b>Persist</b> (checkbox) — “persist across restarts”. When checked the Result Variable is saved to disk and reloaded at next <code>fppd</code> start; when unchecked it lives only in memory and clears on restart.</li>
    <li><b>Filter</b> (dropdown) — How to extract the value to store:
        <b>None (use raw result)</b> — store the whole raw output as-is;
        <b>JSON Field</b> — dotted path (e.g. <code>data.temperature</code>, object keys only, no array indices) pulls one field from a JSON response;
        <b>Between Markers</b> — keeps text between two literal markers (<b>After</b> + <b>Before</b> fields, each blank means from start/to end);
        <b>Regex (advanced)</b> — regular expression; the first capture group is used, or the whole match if no group. Each choice reveals its own row(s): Field path, After/Before, or Pattern (<code>e.g. Temp: ([0-9.]+)</code>), with inline help icons explaining the semantics.</li>
</ul>

<h4>Test Run</h4>
<ul>
    <li><b>Test Run</b> (per-row button, label swaps to “Running…” while in flight, disabled) — Runs the row’s current (possibly unsaved) task immediately via <code>POST api/fppd/recurringtasks {command:'test', task:{…}}</code>. For Command tasks the server validates a command is selected; for Preset tasks it validates a preset is selected. On success opens <b>Test Run: {name}</b> showing an optional <code>note</code>, plus <b>Raw result</b> and <b>Filtered (→ Variable)</b> panes (each pre-wrapped, 150 px max), and refreshes status. On error shows the server’s error. A Preset test has no filtered output to show by design (it can run many commands).</li>
</ul>

<h4>Tips</h4>
<ul>
    <li>Keep intervals at or above 30 s; shorter values are flagged red and coerced to 60 s on save. Polling every minute is a good default for weather/sensor APIs.</li>
    <li>Prefer a Recurring Task + Variable over fetching inside a playlist <code>If</code> — the playlist path is time-critical and should read a pre-fetched Variable instantly.</li>
    <li>Use <b>Test Run</b> to confirm both the raw fetch and the filtered value before saving, then check the eye icon’s <b>Last updated</b> to confirm the Variable actually changed.</li>
</ul>
