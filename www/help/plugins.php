<h3>Plugin Manager</h3>
<p>Plugins add extra features to FPP without modifying the core system. Each plugin is maintained by its own author and runs as <code>root</code>, so install only from authors you trust. This page lists what is available, what is installed, and what has updates.</p>

<h4>Top tabs</h4>
<ul>
    <li><b>Available</b> — Browse the catalog from <code>https://raw.githubusercontent.com/FalconChristmas/fpp-data/master/pluginList.json</code> filtered by category and search. Cards you have already installed are marked installed and offer <b>Upgrade</b> or <b>Uninstall</b> instead of <b>Install</b>.</li>
    <li><b>Updates</b> — Shows only installed plugins that have a newer version. The badge count in the tab is updated quietly in the background (one serial check at a time) so it reflects real fetch-based results, not stale data. Use this tab to focus on what needs attention.</li>
    <li><b>Installed</b> — Shows only what is on this device, regardless of catalog availability. Each card offers row actions below.</li>
</ul>

<h4>Search and categories (Available tab)</h4>
<ul>
    <li><b>Search box</b> — Type to filter by plugin name, description or author. While a search is active the Popular strip is hidden and results are shown directly in the grid.</li>
    <li><b>Category pills</b> — Built from <code>pluginCategories.json</code> (All, plus each category like Controllers, Output, etc., sorted A–Z with <b>Other</b> inserted alphabetically). <b>All</b> is the default. Clicking a pill filters the grid, rebuilds the Popular strip for that category, and updates counts shown as <code>badge bg-secondary fppCatCount</code> per category.</li>
    <li><b>Popular strip</b> — Horizontal strip of popular plugins for the current category/search, ordered by install counts fetched from <code>api/plugin/popularity</code> (proxied, gzipped, 7-day cached on the box). Left/right scroll buttons scroll the strip. It hides while searching or when it has no cards to show.</li>
</ul>

<h4>Plugin cards</h4>
<ul>
    <li><b>Icon</b> — Served via same-origin <code>api/plugin/{RepoName}/icon</code> (bypasses CSP limits on external hosts). A cache-busting <code>?_=Date.now()</code> ensures an updated icon appears immediately. Initials are shown when no icon is available (first letters of words, e.g. “FB” for “FPP Brightness”).</li>
    <li><b>Name, version, author</b> — Author is derived from the verified <code>srcURL</code> host/owner (e.g. <code>github.com/FalconChristmas</code>), not the self-reported author field. The badge <b>Official</b> (FalconChristmas org) marks FPP-team maintained plugins.</li>
    <li><b>Resource hint badge</b> — If the plugin declares <code>minMemoryMB</code> or <code>minCpuCores</code> and this device has less, a muted <b>Not Enough RAM/CPU</b> badge appears with a tooltip explaining the shortfall. On <b>Basic</b> UI level such plugins are hidden; on Advanced+ an install warns with a <b>Not enough RAM/CPU</b> callout.</li>
    <li><b>Install</b> (Available, not yet installed) — For <b>Official</b> plugins installs immediately; for third-party it first shows a confirmation dialog explaining that third-party code runs as root and can change any setting and reach the network, with a link to the source (<code>srcURL</code>) when it is an <code>http/https</code> URL. Pasting a raw <code>pluginInfo.json</code> URL (developer workflow) always confirms, even for Official, with a separate developer warning that the URL’s code will run as root.</li>
    <li><b>Upgrade / Check for Update</b> (installed, on card or detail modal) — <b>Check for Update</b> POSTs to <code>api/plugin/{name}/updates</code> (spinner + “Checking for Updates” state). If an update is found the button becomes green <b>Update</b> which streams <code>api/plugin/{name}/upgrade?stream=true</code> into the progress dialog.</li>
    <li><b>Uninstall</b> (installed) — Streams <code>DELETE api/plugin/{name}?stream=true</code> into the progress dialog after confirmation.</li>
    <li><b>Reinstall</b> (single, detail view) — Reinstalls that one plugin via the uninstall-then-install queue.</li>
</ul>

<h4>Toolbar buttons (Installed / Updates views)</h4>
<ul>
    <li><b>Check for Updates</b> — Parallel checks of every installed plugin via <code>api/plugin/{name}/updates</code>. While busy the button shows a spinner; rows gain <code>fppHasUpdate</code> and an <b>updatesAvailable</b> badge as answers arrive. Ends with a growl: “All up to date”, “Found updates for N”, or “Completed checking (some checks failed)”.</li>
    <li><b>Update All</b> — First re-checks all for updates, then confirms “Update N plugin(s) …?” and streams upgrades one by one via a batch queue (<code>RunBatchQueue</code>) into <b>Update All Plugins</b> progress dialog, then re-verifies each still has an update (since the upgrade endpoint streams even on logical failure) and reports how many succeeded.</li>
    <li><b>Reinstall All</b> / <b>Uninstall All</b> — Reinstall runs an uninstall queue followed by an install queue (building install bodies from cached <code>pluginInfos</code> before removal so missing info is skipped and reported). Uninstall All streams deletes for all installed. Both use the same progress dialog and batch queue.</li>
</ul>

<h4>Progress dialog</h4>
<p>The large <b>pluginsProgressPopup</b> dialog shows streaming command output (plain text appended to a textarea, auto-scrolled). The status line reads <code>{label} — {verb} X of Y</code> when <code>showStatus</code> is true. Close is disabled until the batch finishes.</p>

<h4>Credential handling</h4>
<ul>
    <li><b>GitHub credentials</b> — Stored via <code>Settings → Developer → GitHub User Name / PAT</code>. Automatically used (<code>useCredentials=1</code>) for plugins whose <code>pluginInfo.json</code> is marked <code>private</code> or that were loaded via the credentialed proxy URL. Pasted <code>pluginInfo.json</code> URLs can also be fetched with credentials when the checkbox in the manual-URL dialog is ticked.</li>
    <li><b>Private plugins</b> — Not listed in the public catalog; load via the <b>Manual URL</b> field (paste a <code>pluginInfo.json</code> URL) or by uploading.</li>
</ul>

<h4>Tips</h4>
<ul>
    <li>Use <b>Updates</b> to focus on what actually needs attention after the quiet background check finishes.</li>
    <li>On a low-memory single-core device, heed the <b>Not Enough RAM/CPU</b> warning — the plugin may run poorly or not at all.</li>
    <li>After a FPP OS upgrade that warns “plugins must be reinstalled”, use <b>Reinstall All</b> from <code>plugins.php?action=reinstallAll</code> handling or the toolbar, then reload the page when the progress dialog completes.</li>
</ul>
