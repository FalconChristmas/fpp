<h3>Plugins</h3>
<p>Plugins add extra features to FPP without modifying the core system. Each plugin is maintained by its own author
    and runs as <code>root</code>, so install only from authors you trust. This page lists what is available,
    what is installed, and what has updates.</p>
<p>Plugins are written and published by other people, and the FPP project does not test, vet or guarantee their
    quality or safety. <b>Every plugin runs as root: once installed it has full control of this player</b>. It can
    read and change every setting, including the privacy settings, reach anything on the network this player is
    connected to, and do anything at all, whether or not its privacy disclosure mentions it. This is inherently
    dangerous. Install a plugin at your own risk, and only if you trust the person who wrote it. Plugins marked
    <b>Official</b> are maintained by the FPP project, and still run as root with the same access.</p>

<h4>The six privacy lights</h4>
<p>Each plugin's author fills in a short privacy disclosure, and FPP turns it into six coloured lights. They are
    shown as small dots on the plugin's card, and in full when you click the card or press <b>Install</b>.</p>
<ul>
    <li><b>Sends data</b> — Does it send anything off this player, and to whom?</li>
    <li><b>Collects data</b> — Does it keep information about you, your household or your visitors?</li>
    <li><b>Camera &amp; mic</b> — Does it use a camera, microphone or similar sensor, and does it record?</li>
    <li><b>Remote access</b> — Can it be reached from outside this player, or from the internet?</li>
    <li><b>System changes</b> — Does it change the player itself, beyond its own files?</li>
    <li><b>Can it be checked?</b> — Is all of its code somewhere anyone can look at it?</li>
</ul>
<p><b>Green</b> means the author says there is nothing to report under that heading. <b>Amber</b> means there is
    something you should know; tap the chip to read it. <b>Red</b> means read the line before you install. A line
    above the lights sums up the worst finding, and the install button says what you are agreeing to, for example
    <i>Install anyway</i> or <i>Install, opens FPP to internet</i>.</p>
<p>Remember that the lights come from what the author wrote. FPP does not check the plugin's code against its
    disclosure, and a green light is the author's word, not a test result. The lights describe what the author says
    the plugin does; they do not limit what it can do.</p>

<h4>Plugins with no privacy disclosure</h4>
<p>If the author has not written a disclosure, every light reads <i>not disclosed</i> and the install button says
    <i>Install, no disclosure</i>. Nothing is known about what the plugin does with data; treat it as unknown
    rather than safe. Every plugin will be required to have a disclosure from 1 January 2027, and from that date a
    plugin without one is shown in red.</p>

<h4>Checking a plugin without installing</h4>
<p>Click a card to see its lights and full disclosure without installing anything. Use it to see what an installed
    plugin says about itself, or to compare plugins before choosing one. The dot row on the card is a summary; the
    detail modal shows each light's full text.</p>

<h4>Top tabs</h4>
<ul>
    <li><b>Available</b> — Browse the catalog from
        <code>https://raw.githubusercontent.com/FalconChristmas/fpp-data/master/pluginList.json</code> filtered by
        category and search. Cards you have already installed are marked installed and offer <b>Upgrade</b> or
        <b>Uninstall</b> instead of <b>Install</b>.</li>
    <li><b>Updates</b> — Shows only installed plugins that have a newer version. The badge count in the tab is
        updated quietly in the background (one serial check at a time via <code>api/plugin/{name}/updates</code>)
        so it reflects real fetch-based results, not stale data. Use this tab to focus on what needs attention.</li>
    <li><b>Installed</b> — Shows only what is on this device, regardless of catalog availability. Each card offers
        row actions below.</li>
</ul>

<h4>Search and categories (Available tab)</h4>
<ul>
    <li><b>Search box</b> — Type to filter by plugin name, description or author. While a search is active the
        Popular strip is hidden and results are shown directly in the grid.</li>
    <li><b>Category pills</b> — Built from <code>pluginCategories.json</code> (All, plus each category like
        Controllers, Output, etc., sorted A–Z with <b>Other</b> inserted alphabetically). <b>All</b> is the
        default. Clicking a pill filters the grid, rebuilds the Popular strip for that category, and updates counts
        shown as <code>badge bg-secondary fppCatCount</code> per category.</li>
    <li><b>Popular strip</b> — Horizontal strip of popular plugins for the current category/search, ordered by
        install counts fetched from <code>api/plugin/popularity</code> (proxied, gzipped, 7-day cached on the box).
        Left/right scroll buttons scroll the strip. It hides while searching or when it has no cards to show.</li>
</ul>

<h4>Plugin cards</h4>
<ul>
    <li><b>Icon</b> — Served via same-origin <code>api/plugin/{RepoName}/icon</code> (bypasses CSP limits on
        external hosts). A cache-busting <code>?_=Date.now()</code> ensures an updated icon appears immediately.
        Initials are shown when no icon is available (first letters of words, e.g. "FB" for "FPP Brightness").</li>
    <li><b>Name, version, author</b> — Author is derived from the verified <code>srcURL</code> host/owner
        (e.g. <code>github.com/FalconChristmas</code>), not the self-reported author field. The badge
        <b>Official</b> (FalconChristmas org) marks FPP-team maintained plugins.</li>
    <li><b>Privacy dots</b> — The six lights summarized as coloured dots on the card. Green/amber/red as above.
        Click the card to read the full disclosure.</li>
    <li><b>Resource hint badge</b> — If the plugin declares <code>minMemoryMB</code> or
        <code>minCpuCores</code> and this device has less, a muted <b>Not Enough RAM/CPU</b> badge appears with a
        tooltip explaining the shortfall. On <b>Basic</b> UI level such plugins are hidden; on Advanced+ an install
        warns with a <b>Not enough RAM/CPU</b> callout.</li>
    <li><b>Install</b> (Available, not yet installed) — For <b>Official</b> plugins installs immediately unless a
        resource or privacy warning applies; for third-party it first shows a confirmation dialog explaining that
        third-party code runs as root and can change any setting and reach the network, with a link to the source
        (<code>srcURL</code>) when it is an <code>http/https</code> URL. The button label itself reflects the
        privacy disclosure (e.g. <i>Install anyway</i>, <i>Install, opens FPP to internet</i>,
        <i>Install, no disclosure</i>). Pasting a raw <code>pluginInfo.json</code> URL (developer workflow) always
        confirms, even for Official, with a separate developer warning that the URL's code will run as root.</li>
    <li><b>Upgrade / Check for Update</b> (installed, on card or detail modal) — <b>Check for Update</b> POSTs to
        <code>api/plugin/{name}/updates</code> (spinner + "Checking for Updates" state). If an update is found the
        button becomes green <b>Update</b> which streams <code>api/plugin/{name}/upgrade?stream=true</code> into
        the progress dialog. If the update changes the privacy disclosure, FPP shows the new disclosure before
        updating and asks you to accept it.</li>
    <li><b>Uninstall</b> (installed) — Streams <code>DELETE api/plugin/{name}?stream=true</code> into the progress
        dialog after confirmation. Anything the plugin leaves behind on the player is listed under
        <b>System changes</b> in its privacy disclosure.</li>
    <li><b>Reinstall</b> (single, detail view) — Removes and reinstalls that one plugin via the uninstall-then-install
        queue. Try this after an FPP upgrade, or when a plugin has stopped working.</li>
    <li><b>Open</b> — Opens the plugin's own page, if it has one (<code>pageUrl</code> in its
        <code>pluginInfo.json</code>).</li>
</ul>

<h4>Toolbar buttons (Installed / Updates views)</h4>
<ul>
    <li><b>Check for Updates</b> — Parallel checks of every installed plugin via
        <code>api/plugin/{name}/updates</code>. While busy the button shows a spinner; rows gain
        <code>fppHasUpdate</code> and an <b>updatesAvailable</b> badge as answers arrive. Ends with a growl:
        "All up to date", "Found updates for N", or "Completed checking (some checks failed)".</li>
    <li><b>Update All</b> — First re-checks all for updates, then confirms "Update N plugin(s) …?" and streams
        upgrades one by one via a batch queue (<code>RunBatchQueue</code>) into <b>Update All Plugins</b> progress
        dialog, then re-verifies each still has an update (since the upgrade endpoint streams even on logical
        failure) and reports how many succeeded. When a pending update changes its disclosure, <b>Update All</b>
        leaves that plugin alone and tells you which ones to update from their own <b>Update</b> button, so you see
        the change first. A change only to the author's summary or notes, with the rest of the disclosure the same,
        is updated without asking.</li>
    <li><b>Reinstall All</b> / <b>Uninstall All</b> — Reinstall runs an uninstall queue followed by an install
        queue (building install bodies from cached <code>pluginInfos</code> before removal so missing info is
        skipped and reported). Uninstall All streams deletes for all installed. Both use the same progress dialog
        and batch queue.</li>
</ul>

<h4>Progress dialog</h4>
<p>The large <b>pluginsProgressPopup</b> dialog shows streaming command output (plain text appended to a textarea,
    auto-scrolled). The status line reads <code>{label} — {verb} X of Y</code> when <code>showStatus</code> is
    true. Close is disabled until the batch finishes. Reload the page when the dialog completes to refresh the
    plugin list.</p>

<h4>Credential handling</h4>
<ul>
    <li><b>GitHub credentials</b> — Stored via <code>Settings → Developer → GitHub User Name / PAT</code>.
        Automatically used (<code>useCredentials=1</code>) for plugins whose <code>pluginInfo.json</code> is marked
        <code>private</code> or that were loaded via the credentialed proxy URL. Pasted
        <code>pluginInfo.json</code> URLs can also be fetched with credentials when the checkbox in the
        manual-URL dialog is ticked.</li>
    <li><b>Private plugins</b> — Not listed in the public catalog; load via the <b>Manual URL</b> field (paste a
        <code>pluginInfo.json</code> URL) or by uploading.</li>
</ul>

<h4>Tips</h4>
<ul>
    <li>Use <b>Updates</b> to focus on what actually needs attention after the quiet background check finishes.</li>
    <li>Read the six lights before installing — green is the author's word, not a test. Treat <i>not disclosed</i>
        as unknown, not safe.</li>
    <li>On a low-memory single-core device, heed the <b>Not Enough RAM/CPU</b> warning — the plugin may run poorly
        or not at all.</li>
    <li>After a FPP OS upgrade that warns "plugins must be reinstalled", use <b>Reinstall All</b> from
        <code>plugins.php?action=reinstallAll</code> handling or the toolbar, then reload the page when the
        progress dialog completes.</li>
</ul>
