<h3>Command Presets</h3>
<p>Command Presets are named shortcuts for any FPP Command plus its arguments. Save a command once (e.g. “Set Volume to 40” or “Start Playlist Holiday on Remotes”), then trigger the preset by name or by its numbered slot from playlists, the Scheduler, GPIO triggers, URL/MQTT, or the API — without re-entering the arguments each time.</p>

<h4>Header and buttons</h4>
<ul>
    <li><b>Add Preset</b> (green plus, top right) — Opens the preset editor for a new entry.</li>
    <li><b>Save</b> (green) — Writes the whole preset table to <code>api/commandPresets</code> and refreshes the list. Deletions and reorders are staged until Save.</li>
    <li><b>Delete</b> (red outline, disabled until a row is selected) — Removes the selected preset rows. Requires Save to make permanent.</li>
    <li><b>Reorder</b> — Drag the grip handle on a row to move it. In slot-triggered workflows the slot number follows the row’s position; reordering changes which slot triggers which preset, so update any triggers that reference slots.</li>
</ul>

<h4>Table columns (each row)</h4>
<ul>
    <li><b>Name</b> (text, required, unique) — Human-readable name shown in every picker that lists presets (Playlists, Scheduler, GPIO, Recurring Tasks). The name is the primary key; two rows may not share the same name and the row will highlight when duplicate or empty.</li>
    <li><b>Preset Slot</b> (number, with help icon) — The slot number used with <b>Trigger Command Preset Slot</b> to fire the preset via the API without naming it. Shown as a numeric input; Set to <code>0</code> to disable slot triggering. The help tooltip explains: “The Preset Slot number is used along with the ‘Trigger Command Preset Slot’ FPP Command to allow FPP Command Presets to be triggered via API. Set to ‘0’ to disable.”</li>
    <li><b>Command</b> — The underlying FPP Command for this preset, shown as a dropdown built by <code>LoadCommandList</code> (filtered by UI Level and platform). The command’s category and help icon are shared with every other command picker in FPP.</li>
    <li><b>Args</b> — Command-specific argument fields that appear to the right of the command dropdown. Their type and layout come from the command’s JSON definition: plain text inputs, number inputs, file pickers, playlist pickers, color pickers, time pickers, etc. Each arg has its own help icon where the command defines one. Multisync destinations (<code>multisyncCommand</code>/<code>multisyncHosts</code>) are hidden in this table and edited only when the preset is later triggered.</li>
    <li><b>Actions</b> — Per-row <b>Edit</b> (pencil) opens the preset editor modal with the full command form; <b>Delete</b> (trash) stages that row for deletion; drag grip reorders.</li>
</ul>

<h4>Preset editor modal</h4>
<ul>
    <li>Opened by <b>Add Preset</b> or per-row <b>Edit</b>. Shows <b>Name</b>, <b>Preset Slot</b> and the same <b>Command</b> + <b>Args</b> fields as the table row, plus a live preview of what will be saved. Validation (unique non-empty name) runs on Save in the modal; the row updates in the table without a separate Save of the whole page until you press the page-level <b>Save</b>.</li>
</ul>

<h4>How presets are used elsewhere</h4>
<ul>
    <li><b>Playlists</b> — Add a <b>FPP Command</b> entry and pick <b>Trigger Command Preset</b> by name.</li>
    <li><b>Scheduler</b> — Schedule a <b>FPP Command</b> that triggers a preset at a date/time.</li>
    <li><b>GPIO Inputs</b> — Rising/Falling/Hold edge actions can trigger a preset.</li>
    <li><b>API / MQTT / URL</b> — <code>POST api/commandPresets/{name or slot}/trigger</code> or <code>/set/command/Trigger Command Preset/{slot}</code> via MQTT. Slot 0 cannot be triggered.</li>
</ul>

<h4>Tips</h4>
<ul>
    <li>Prefer <b>Name</b> over <b>Slot</b> in new work — names survive reordering, slots do not.</li>
    <li>Keep one preset per logical action (e.g. “House Lights 50%”) so you can change the underlying command once and every trigger picks it up.</li>
    <li>Delete a preset only after searching for its name and slot in playlists and scheduler entries that may still reference it.</li>
</ul>
