<h3>Troubleshooting</h3>
<p>This page runs a suite of diagnostic commands grouped into tabs. Results stream live from the FPP: each command’s raw text is shown exactly as emitted.</p>

<h4>Header and actions</h4>
<ul>
    <li><b>Diagnostic Report</b> (top right) — Opens the Diagnostic Report wizard, which builds a diagnostic report on this FPP via <code>POST /api/crashes/report</code>, helps you send it with consent, and prefills a GitHub issue with the filename, version and platform. A logs/config zip of this page’s command output remains available from File Manager › Logs › Zip.</li>
    <li><b>Back to top</b> (red pill, fixed bottom-right) — Scrolls to the top; fades in after 100 px of scroll, fades out near the top.</li>
</ul>

<h4>Tabs and platform filtering</h4>
<ul>
    <li><b>Group tabs</b> — Built from <code>troubleshoot-commands.json</code> via <code>LoadTroubleShootingCommands()</code>. Each group has an ID, a display title (<code>grpDisplayTitle</code>) and a description (<code>grpDescription</code>). Only groups whose <code>platforms</code> intersect with <code>['all', current Platform]</code> are rendered.</li>
    <li><b>First tab auto-active</b> — The first matching group is shown with <code>active</code>/<code>show</code>. Switching tabs via Bootstrap pills triggers that group’s <code>dispTroubleTab{ID}()</code> which fires one <code>GET troubleshootingHelper.php?key={commandKey}</code> per command in the group that matches the platform. Results stream in as they arrive; the hash-based tab anchor is handled to jump to the correct pane.</li>
    <li><b>Hot links</b> (inside each group’s backdrop, before the results) — A wrapping flex row of links to every command’s anchor in that group: <code>&lt;a href="#header_{commandKey}"&gt;Title&lt;/a&gt;</code> paired with a compact status badge (see below). Rendered into <code>#troubleshooting-hot-links-{group}</code>.</li>
</ul>

<h4>Per-command blocks</h4>
<ul>
    <li><b>Title + badges</b> — <code>&lt;h3&gt;Title &lt;span id="status_{key}"&gt;</code> plus a matching <code>hotlinkstatus_{key}</code> badge. Badges are filled after output is rendered.</li>
    <li><b>Command Description / Command</b> — <b>Command Description:</b> plain text from the JSON, then <b>Command:</b> the exact shell command string shown for reference (what will appear in the bundle as well).</li>
    <li><b>Output</b> — A <code>&lt;pre id="command_{key}"&gt;</code> initially showing a spinner and <i>Loading…</i>. Once fetched, the raw text replaces it. The text is built from <b>Text nodes</b> (never <code>innerHTML</code>) so angle brackets like <code>&lt;unavailable&gt;</code> or <code>&lt;node&gt;</code> in tool output are not parsed as HTML and do not vanish.</li>
    <li><b>In a Diagnostic Report</b> — The report that <b>Diagnostic Report</b> builds includes this page’s output. A command whose output identifies you or your network (Wi-Fi network names, process command lines, git identity, web server clients) runs a cut-down version with those parts removed, or is left out. The report’s <b>Command:</b> line shows which one ran, or which was left out. Reports written after a crash do not include this page.</li>
</ul>

<h4>Verdict highlighting</h4>
<p>The scripts mark verdicts with a leading <code>[PASS]</code> / <code>[WARN]</code> / <code>[FAIL]</code> / <code>[INFO]</code> / <code>[SKIP]</code>. Plain-state commands carry no markers and render unchanged. Only on-screen decoration is added here; a Diagnostic Report gets the text exactly as the commands printed it.</p>
<ul>
    <li><b>Marker lines</b> — <code>[FAIL]</code> → red subtle background + bold, <code>[WARN]</code> → amber subtle + bold, <code>[PASS]</code> → green, <code>[INFO]/[SKIP]</code> → muted. Indented lines directly under a <code>FAIL</code>/<code>WARN</code> are tinted the same color but without the band, so the “what to do” stays visually attached. Section headers like <code>=== Section ===</code> / <code>--- Section ---</code> are bold, and <code>Result: …</code> is bold and colored by the overall worst verdict for that command.</li>
    <li><b>Counts and badges</b> — Each output is scanned for marker counts. Badges are rendered by <code>troubleshootBadges(counts, compact)</code>: one compact numeric badge per kind beside the hot link and on the tab (<code>3</code>), and a full label beside the heading (<code>3 failures</code> / <code>3 warnings</code> / <code>3 passed</code>). Only when there are no failures or warnings does a green “N passed” badge appear, and when a command reported nothing at all but skips — a section that has nothing applying to this device, such as Network Audio on a simple-mode player with no AES67, Opus or RTSP configured — a grey “nothing to check” badge appears in its place, so a section that ran is never left looking as though it had not.</li>
    <li><b>Tab badges</b> — Each group’s pill accumulates totals from all its commands’ <code>data-troubleshoot-fail/warn/pass/skip</code> datasets via <code>updateTroubleshootTabBadge()</code>, so looking at a tab shows its aggregate health without opening it.</li>
</ul>

<h4>Hash and navigation</h4>
<ul>
    <li>Hashes of the form <code>#header_{commandKey}</code> (hot-link anchors) and <code>#pills-{group}</code> (tab anchors) are both supported. On load, if the hash names a hot-link anchor, the code finds the enclosing <code>.tab-pane</code>, switches to that tab, then scrolls the anchor into view and pins the header. Direct <code>#pills-{group}</code> hashes also switch tabs.</li>
    <li>Anchors are served via <code>&lt;a name="header_{key}"&gt;</code> with a hidden <code>troubleshoot-anchor</code> class for reliable scrolling after AJAX insertion.</li>
</ul>

<h4>Tips</h4>
<ul>
    <li>Open each tab you care about — tabs run their commands only when first viewed, so an unseen tab shows no badge until visited.</li>
    <li>Use the hot-link row at the top of a group to jump to a failing command without reading every output.</li>
    <li>Use the <b>Diagnostic Report</b> button when you need to share diagnostics with the developers.</li>
</ul>
