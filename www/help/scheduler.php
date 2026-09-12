<h3>Scheduler</h3>
<p>The Scheduler decides <em>when</em> playlists and commands run automatically. Each row in the table is one schedule entry. While its date, day and time window is active, FPP runs the chosen playlist or command. Only one scheduled playlist runs at a time; overlapping entries are resolved by the most recently started entry taking priority. Your device clock (Settings → Time) and location (Settings → System) must be correct for sunrise/sunset times to work.</p>

<h4>Top controls</h4>
<ul>
    <li><b>Preview</b> (dropdown, top left) — <b>Nested Table View</b> shows the upcoming schedule as a table grouped by playlist, with start and end times expanded. <b>Calendar View</b> shows the same as a monthly calendar. Both open in a dialog that updates from the current schedule; they are read-only previews.</li>
    <li><b>Edit Holidays</b> — Opens the holiday editor. User holidays are stored in <code>api/configfile/user-holidays.json</code> and merged with the locale holidays from Settings → Localization. Each holiday has <b>Display name</b> (e.g. “My Holiday”), <b>Config name</b> (alphanumeric short name used in dates, e.g. MyHoliday, auto-generated from the display name), <b>Month</b> and <b>Day</b>. Saving also rewrites any schedule entries that used a renamed holiday’s old Config name.</li>
    <li><b>Reload</b> — Re-reads <code>api/schedule</code> from disk and rebuilds the table, discarding unsaved edits.</li>
    <li><b>Clear Selection</b> — Deselects all highlighted rows. Enabled Delete and Clone buttons return to disabled.</li>
    <li><b>Delete</b> (top right, disabled until a row is selected by clicking it) — Removes the selected entries from the table. You must still press <b>Save</b> to make the deletion permanent.</li>
    <li><b>Clone</b> (disabled until selection) — Duplicates each selected row, appending the copies to the table. The copies can then be edited before saving. Handles the internal <code>day &gt; 0x10000</code> Day Mask encoding and synchronizes the seven individual day checkboxes.</li>
    <li><b>Add</b> (green +) — Appends a new default entry: Enabled, Day = Everyday, Type = Playlist (or Command on Remote mode), Playlist = empty, Start 00:00:00, End 24:00:00, Repeat = 1 (repeat), Start Date = today, End Date = Dec 31 MAXYEAR, Stop Type = 0 (Graceful).</li>
    <li><b>Save</b> (green) — Collects every row via <code>GetScheduleEntryRowData</code>, validates playlist/sequence/command fields (missing values get a yellow warning), warns if a non-Command entry is enabled on a Remote, then POSTs JSON to <code>api/schedule</code> and immediately POSTs to <code>api/schedule/reload</code>. Shows “Schedule saved” and “Schedule reloaded” growls.</li>
</ul>

<h4>Schedule table columns (each row)</h4>
<ul>
    <li><b>Grip handle</b> (left, on touch devices) — Drag to reorder rows. On non-touch, drag anywhere on the row. Order affects priority when entries overlap.</li>
    <li><b>Enable</b> (checkbox) — When unchecked the entry is ignored by the scheduler but stays in the table for later use.</li>
    <li><b>Type</b> (dropdown) — <b>Playlist</b> (run a playlist), <b>Sequence</b> (run a single .fseq file), or <b>FPP Command</b> (run any FPP command). Changing Type swaps the next column’s control (playlist dropdown vs sequence dropdown vs command template) and for Command hides the End Time column and disables the Immediate repeat option. On a Remote, non-Command types gain a yellow warning because Remotes only honor Commands.</li>
    <li><b>Playlist / Sequence / Command</b> (depending on Type) —
        <ul>
            <li><b>Playlist</b> — Dropdown of playlists from <code>api/playlists</code>. Highlighted with a tooltip showing the raw value.</li>
            <li><b>Sequence</b> — Dropdown of .fseq files from <code>GetSequenceArray()</code>. Same tooltip behavior (strips .fseq for display).</li>
            <li><b>Command</b> — Shared command template built by <code>FillInScheduleCommandTemplate</code> (which calls <code>FillInCommandTemplate</code> and then removes the “Start Playlist” option because scheduling a playlist directly is preferred). Shows <b>Command</b> dropdown, <b>Args</b> table that changes per command, and hidden multisync fields. The original command JSON is kept in a hidden <code>.cmdTmplJSON</code> element.</li>
        </ul>
    </li>
    <li><b>Start Date</b> / <b>End Date</b> (text + date picker, or holiday name) — Either a calendar date <code>YYYY-MM-DD</code> or a holiday short name (e.g. <code>Christmas</code>). A picker is shown for dates; when a holiday is chosen the text field hides and a <b>Holidays</b> dropdown shows instead, plus a <b>Specify Date</b> option to switch back. On small touch screens the native HTML date picker is used, except when a holiday name is already set. The picker is limited to <code>MINYEAR–MAXYEAR</code> and offers <b>Holidays</b> and <b>Today</b> buttons. Invalid non-date/non-holiday values are coerced to <code>MAXYEAR-12-31</code> via <code>DateChanged</code>.</li>
    <li><b>Day</b> (dropdown) — Which weekdays the entry is active:
        <b>Everyday</b> (7), <b>Sunday</b> (0) through <b>Saturday</b> (6), <b>Mon-Fri</b> (8), <b>Sat/Sun</b> (9), <b>Mon/Wed/Fri</b> (10), <b>Tues/Thurs</b> (11), <b>Sun-Thurs</b> (12), <b>Fri/Sat</b> (13), <b>Odd</b> (14), <b>Even</b> (15), or <b>Day Mask</b> (65536) which reveals seven individual <b>S M T W T F S</b> checkboxes underneath. Day Mask encodes Sunday as <code>0x4000</code> down to Saturday as <code>0x0100</code> on top of the base value.</li>
    <li><b>Start Time</b> / <b>End Time</b> (time picker) — Time of day the entry starts and stops. Also accepts the special values <b>Dawn</b>, <b>SunRise</b>, <b>SunSet</b>, <b>Dusk</b> (stored internally as <code>25:00:00</code> for sunrise and <code>26:00:00</code> for sunset). Format follows Settings → Localization → Time Format (12h or 24h) and includes seconds when <code>ScheduleSeconds = 1</code>. When either time is a dawn/sunset value, its adjacent <b>Offset</b> field appears; otherwise it hides. For Command entries with Repeat = 0, End Time is hidden and forced to Start Time. Times are converted through <code>Convert24HToUIFormat</code> / <code>Convert24HFromUIFormat</code>.</li>
    <li><b>Start Offset / End Offset</b> (number, shown only for dawn/sunset) — Minutes to shift the astronomical time earlier (negative) or later (positive).</li>
    <li><b>Repeat</b> (dropdown) — <b>Immediate</b> (loop without pause) and <b>Once</b> etc. For Command entries the Immediate option is removed and Repeat is forced to 0 (run once). Changing Repeat for a Command hides/shows the End Time column via <code>ScheduleEntryRepeatChanged</code>.</li>
    <li><b>Stop Type</b> (dropdown) — How the entry stops: <b>Graceful</b> (0, finish current sequence), <b>Graceful After Loop</b> etc., vs hard stop. The exact labels mirror the scheduler’s stop semantics.</li>
</ul>

<h4>Selection and keyboard</h4>
<ul>
    <li>Click a row to select it (adds <code>selectedEntry</code> class); Shift/Ctrl handling is via <code>HandleTableRowMouseClick</code>. Selected rows enable Delete and Clone.</li>
    <li>Drag-reorder updates the table order and calls <code>SetScheduleInputNames</code> to refresh time pickers and date pickers on the new order.</li>
</ul>

<h4>Tips</h4>
<ul>
    <li>Keep holidays in sync: after renaming a holiday, the editor automatically rewrites any schedule entries that used the old Config name so they continue to match.</li>
    <li>Use <b>Preview → Calendar View</b> to spot gaps or overlaps before the show, especially around holidays and Day Mask entries.</li>
    <li>On a Remote, schedule only Commands — playlist/sequence entries are ignored and flagged with a warning banner at the top of the table.</li>
</ul>
