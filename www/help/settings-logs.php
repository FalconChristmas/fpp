<h3>Logging</h3>
<p>Choose how much detail FPP writes to its logs. The defaults are fine for a normal show. Turn detail up only when diagnosing a problem, then turn it back down — excessive logging can slow playback and fill storage.</p>

<div class="callout callout-warning"><b>Recommended for production:</b> <b>Info</b> on every category except Channel Data.</div>

<h4>Log Levels (16 dropdowns → one per area)</h4>
<p>Each dropdown controls one subsystem. Choices for every dropdown: <b>Errors Only</b> (<code>error</code>), <b>Warn</b>, <b>Info</b>, <b>Debug</b>, <b>Excessive</b>. Defaults to <code>info</code>. Excessive is very noisy and can affect performance — only use on request.</p>
<ul>
    <li><b>General</b> — Core miscellaneous messages.</li>
    <li><b>ChannelOut</b> — Channel testing and overlay data flow.</li>
    <li><b>ChannelData</b> — Lowest-level serial/LOR data sends. Extremely chatty — bulk “make all Info” intentionally leaves this one alone.</li>
    <li><b>Command</b> — Commands and their replies (REST, scheduler, MQTT, GPIO-triggered).</li>
    <li><b>Control</b> — Control interface / MQTT input handling.</li>
    <li><b>E131Bridge</b> — E1.31/DDP bridge input.</li>
    <li><b>Effect</b> — Effects engine.</li>
    <li><b>GPIO</b> — GPIO input/output events.</li>
    <li><b>HTTP</b> — fppd REST API (HTTP) traffic.</li>
    <li><b>MediaOut</b> — Audio and video output.</li>
    <li><b>Playlist</b> — Playlist parsing and sequencing.</li>
    <li><b>Plugin</b> — Plugin management.</li>
    <li><b>Schedule</b> — Scheduler decisions.</li>
    <li><b>Settings</b> — Settings parsing/applying.</li>
    <li><b>Sequence</b> — Sequence file parsing.</li>
    <li><b>Sync</b> — MultiSync between Player and Remotes.</li>
</ul>

<h4>Bulk Change</h4>
<p>Buttons under the table set <i>all levels at once except Channel Data</i> to: <b>Errors Only</b>, <b>Warn</b>, <b>Info</b>, <b>Debug</b>, or <b>Excessive</b>. This does not save them — each button changes the dropdown values, which auto-save on change individually. Use it to quickly make the whole log “more talkative” while intentionally leaving channel data at Info.</p>
