<h3>Playlists</h3>
<p>A playlist is an ordered list of what to play — sequences, audio, video, pauses and the actions in between. The Scheduler decides <em>when</em> a playlist runs; this page decides <em>what</em> is inside it.</p>

<h4>Your Playlists (first view)</h4>
<ul>
    <li><b>Playlist cards</b> — Each card shows the playlist <b>Name</b>, optional <b>Description</b>, <b>Total Duration</b> (sum of all entries) and <b>Total Items</b> with a breakdown when using the sections below: <code>Lead In: X, Main: Y, Lead Out: Z</code>. Cards with a warning highlight indicate a missing file.</li>
    <li><b>New Playlist</b> (button, top right) — Opens <b>Add a New Playlist</b> dialog: <b>Playlist Name</b> (required, must be unique), <b>Description</b> (optional), <b>Randomize</b> dropdown (see below), and <b>Global Pause Between Sequences</b> (number, ms, 0–999999). Creates an empty playlist and opens it in the editor.</li>
    <li><b>Click a card</b> — Selects that playlist and opens the editor. The hidden <code>#playlistSelect</code> dropdown tracks the current choice and drives the header title.</li>
</ul>

<h4>Playlist Editor Header</h4>
<ul>
    <li><b>Back button</b> (chevron + grid icon, left) — Returns to the card grid. If you have unsaved changes you will be asked to confirm. Tables are cleared so the next playlist does not show stale rows.</li>
    <li><b>Title + Edit (pencil)</b> — The large title mirrors the current playlist name. <b>Edit</b> opens <b>Edit Playlist Details</b> (same fields as Add: Name, Description, Randomize, Global Pause) — saving reloads the details and updates the duration.</li>
    <li><b>Gear (Edit Playlist Details)</b> — Same dialog as above for the open playlist.</li>
    <li><b>Check-square (Select Mode)</b> — Toggles multi-select mode. Shows <b>0 Selected</b> dropdown with: <b>Select All</b>, <b>Deselect All</b>, <b>Remove Selected</b>, <b>Duplicate Selected</b> (clones each checked row right after itself), and <b>Copy to…</b> (pick a different target playlist + choose <b>Lead In / Main / Lead Out</b> section, then appends the entries via <code>api/playlist/{target}</code>).</li>
    <li><b>Playlist Actions</b> (dropdown, right) — <b>Copy Playlist</b> (prompts for new name, saves as copy), <b>Rename Playlist</b> (saves under new name and deletes the old via <code>DELETE api/playlist/{name}</code>), <b>Randomize Playlist</b> (shuffles entries), <b>Export Playlist</b> (dialog with <b>JSON / Text (TXT) / Excel (CSV)</b> — TXT lists entries by section with durations and total time, CSV is <code>#,Section,Type,Name,Duration</code>), <b>Reset Playlist</b> (reloads from disk), <b>Delete Playlist</b> (confirmation modal, then <code>DELETE</code> and reload).</li>
    <li><b>Save Playlist</b> (green, right) — Writes the current editor state to <code>api/playlist/{name}</code> and refreshes durations. The exclamation-triangle badge appears when the editor is dirty.</li>
</ul>

<h4>Three Sections</h4>
<p>Every playlist is split into three ordered tables you can drag between:</p>
<ul>
    <li><b>Lead In</b> — Plays once at the very start.</li>
    <li><b>Main Playlist</b> — The repeating body. When <b>Randomize</b> is on or the playlist is set to repeat on the Status page, this section loops.</li>
    <li><b>Lead Out</b> — Plays once at the very end. Drag rows between sections or reorder within a section by dragging the grip handle (touch devices drag via the grip). Double-click a row or click its pencil to edit.</li>
</ul>
<p>Each row shows <b>#</b>, <b>Type</b>, <b>Name/Details</b>, <b>Duration</b>, and row actions <b>Edit</b> / <b>Delete</b>. Durations are summed at the bottom of each section and as a grand total.</p>

<h4>Add a Sequence / Entry</h4>
<p>The large <b>Add a Sequence/Entry</b> button opens <b>New Entry</b> (800 px dialog). Footer offers <b>Add</b> (append), <b>Insert → Before Selection</b> and <b>Insert → After Selection</b>. Inside:</p>
<ul>
    <li><b>Type</b> (dropdown) — The kind of entry. Choices come from <code>playlistEntryTypes.json</code> (deprecated types hidden):
        <b>Sequence and Media</b> (<code>both</code> — default), <b>Sequence Only</b>, <b>Media Only</b>, <b>Pause</b>, <b>Playlist</b> (nest another playlist), <b>FPP Command</b>, <b>Script</b>, <b>Branch</b> (conditional jump), <b>Dynamic</b> (entries from a file/plugin/URL at play time), <b>Image</b> (pixel overlay), <b>URL</b> (GET/POST), and <b>Remap</b> (add/remove channel remaps). The form below changes to match the chosen type.</li>
    <li><b>Auto-Select Matching Media/Sequence</b> (checkbox, checked) — When you pick a sequence, the matching audio/video with the same base name is auto-selected, and vice versa. It also tries the <code>mf</code> header inside the .fseq via <code>api/sequence/{val}/meta</code>.</li>
    <li><b>Hide sequences already in playlist</b> (checkbox) — Filters the sequence dropdown to only files not yet used in this playlist.</li>
</ul>

<h5>Per-type fields (what you will see when that Type is chosen)</h5>
<ul>
    <li><b>Sequence and Media</b> — <b>Sequence</b> (dropdown from <code>api/files/sequences</code>), <b>Media</b> (from <code>api/media</code>), <b>Video Out</b> (from <code>api/options/PlaylistVideoOutput</code>, <code>--Default--</code> uses Settings → Playback), <b>Stream Slot</b> 1–5 (different slots can overlap; same slot interrupts), <b>Extra Media</b> + <b>Extra Media Video Out</b> + <b>Extra Media Slot</b> 2–5 (second simultaneous media, must use a different slot), plus <b>Note</b>, <b>Display Mode</b> (Args Only / Args and Note / Just Note), <b>Time Code</b> (HH:MM:SS or Default).</li>
    <li><b>Sequence Only</b> — <b>Sequence</b> plus Note/Display Mode/Time Code.</li>
    <li><b>Media Only</b> — <b>File Mode</b> (Single File / Random Video / Random Audio), <b>Media</b> or <b>Play files starting with this prefix only</b> when random, <b>Video Out</b>, <b>Stream Slot</b> 1–5, <b>Extra Media</b> trio, Note/Display Mode/Time Code.</li>
    <li><b>Pause</b> — <b>Duration</b> (float seconds, 0–9999).</li>
    <li><b>Playlist</b> — <b>Playlist</b> (from <code>api/playlists</code>) to nest.</li>
    <li><b>FPP Command</b> — <b>Command</b> (from <code>LoadCommandList</code>), then command-specific <b>Args</b> (multisync options are hidden in this view). Each command’s arguments show their own help icons.</li>
    <li><b>Script</b> — <b>Script</b> (from <code>api/scripts</code>), <b>Args</b> (free-form command line), <b>Blocking</b> checkbox (wait for script to finish if checked).</li>
    <li><b>Branch</b> — <b>Test Condition</b> (Time / Loop Number / MQTT Topic Message), then <b>Start/End Time</b> or <b>Loop Test</b> (<code>iteration</code> → <b>Starting iteration</b> 1–99 + <b>Every</b> N loops) or <b>MQTT Topic</b> + <b>MQTT Message</b>; plus <b>If True</b> / <b>If False</b> branches each offering <b>Do Nothing / Jump to Index / Jump to Offset / Call Playlist</b> with Section and Item fields (Index needs Section + Item number; Offset is -99…99).</li>
    <li><b>Dynamic</b> — <b>Source Type</b> (File / Plugin / URL) then <b>File</b> or <b>Plugin</b> + <b>Plugin Host</b> (from <code>api/remotes</code>) + <b>Drain Queue</b> checkbox or <b>URL</b>.</li>
    <li><b>Image</b> — <b>Image</b> file (from <code>images</code> directory), <b>Pixel Overlay Model</b> (from <code>api/models?simple=true</code>).</li>
    <li><b>URL</b> — <b>Method</b> (GET / POST) then <b>URL</b> and, for POST, <b>POST Data</b>.</li>
    <li><b>Remap</b> — <b>Action</b> (Add / Remove), <b>Source</b> channel, <b>Destination</b> channel, <b>Count</b> channels, <b>Loops</b> repeats, <b>Reverse</b> checkbox.</li>
    <li><b>Every entry</b> also carries <b>Entry Properties</b> at the bottom: <b>Note</b> (free-form, only shown per Display Mode) and <b>Display Mode</b> (Args Only / Args and Note / Just Note).</li>
</ul>

<h4>Tips</h4>
<ul>
    <li>Use <b>Lead In/Lead Out</b> for intros/outros you want once, and keep the looping body in <b>Main</b>.</li>
    <li>Keep file names stable — renaming a file in File Manager breaks any playlist entry that referenced the old name.</li>
    <li>Test a new playlist from <code>Status → Play</code> with <b>Play Here</b> on Sequences first to avoid broadcasting to Remotes while you iterate.</li>
</ul>
