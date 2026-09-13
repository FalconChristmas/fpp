<h3>About / System Upgrade</h3>
<p>This page shows what is running on this FPP and helps you keep it up to date. At the top you see the device <b>Platform</b> and <b>Variant</b> (e.g. Raspberry Pi / Pi 4, BeagleBone Black), <b>Mode</b> (Player vs Remote), <b>Hostname</b> and <b>Host Description</b>, plus the installed <b>FPP Version</b> (e.g. <code>8.x-master-123-gabc</code>), <b>Local Git Version</b> hash, <b>OS Version</b> and <b>Kernel</b>. Those fields come from <code>api/system/status → advancedView</code> and are refreshed via <code>UpdateVersionInfo()</code>.</p>

<h4>FPP Software card</h4>
<ul>
    <li><b>Current version + status dot</b> — The dot beside the version shows <b>Up to Date</b> (green), <b>Upgrade Available</b> (amber), <b>OS Upgrade Required</b> / <b>Rebuild Required</b> (red), or <b>Check Failed/Unknown</b> (gray). The subtitle under the dot explains the current state: checking for updates, “FPP 8.2 is available (2–5 min)”, “Cannot upgrade across major versions from here”, “End of Life”, etc. The dot is set by <code>setVersionStatusDot()</code>.</li>
    <li><b>Version rows</b> — <b>Standard view</b> (Basic UI) shows one of: “Up to Date”, “Update available → Upgrade to X”, or “Requires OS Upgrade”. <b>Advanced view</b> shows the full <b>FPP Version</b> row with a clickable indicator that opens either the release notes or the pending Git log, plus <b>OS Version</b>, <b>OS Release</b>, <b>Kernel</b>, <b>Local Git</b> rows. When <code>versionUnknown</code> is true (no local build) the card switches to a single <b>Rebuild Required</b> row.</li>
    <li><b>Update FPP Now / Upgrade to … / Rebuild FPP</b> (right side) — <b>Rebuild</b> appears only when no local version exists. <b>Use OS Upgrade</b> (disabled) appears on major-version upgrades. Otherwise the button is <b>Upgrade to &lt;target&gt;</b> for a branch upgrade or <b>Update FPP Now</b> for a commit update. Clicking routes via <code>HandleFPPUpdate()</code>: rebuild streams <code>manualUpdate.php?wrapped=1</code>, branch upgrade opens release notes, commit update upgrades directly. The button is gray when not recommended and amber when this card is the recommended path (see below).</li>
    <li><b>Branch indicator</b> — Shows <b>Pending Git Changes</b> (“X changes behind” → opens <code>GetGitOriginLog()</code> table of commit/author/date/message) or <b>Click to see release notes</b> for branch upgrades.</li>
</ul>

<h4>Operating System Image card</h4>
<ul>
    <li><b>Current OS badge</b> — Shows the installed image’s version string (neutral pill). The status dot beside it mirrors whether an OS upgrade is available.</li>
    <li><b>OS Select dropdown</b> — Lists <code>.fppos</code> images filtered to this device. Filtering uses <code>matchesDeviceOSBuild()</code>: Pi vs Pi64 and BBB vs BB64 prefixes are enforced, and filenames containing <code>64</code>/<code>32</code> markers are excluded on the wrong architecture. Locally cached files from <code>/upload/*.fppos</code> are merged in. Options are sorted version-aware via <code>compareOSFilenames()</code>: nightlies first, then major → minor → patch descending, final over prerelease (alpha &lt; beta &lt; rc), rebuild over original, then platform order Pi64 &gt; Pi &gt; BB64 &gt; BBB, then date descending.</li>
    <li><b>Show All Platforms / Show Legacy OS</b> (Advanced) — When unchecked, images for other platforms and “legacy” images matching <code>[-_]v?[0-8]\.</code> are hidden. When checked they reappear with a <b>Legacy OS Warning</b> callout.</li>
    <li><b>Download vs Local</b> — Entries ending with <code> (download) from &lt;source&gt;</code> are not yet on disk; the map <code>osAssetMap[asset_id].downloaded</code> tracks this. The <code>(download)</code> suffix is cosmetic — the upgrade logic checks the map, not the label.</li>
    <li><b>Upgrade OS / Download OS</b> (right side) — Yellow when an OS upgrade is available, gray otherwise. The subtitle explains: “A new OS image is available…”, “Required to install FPP X. Your settings and media are preserved”, or “OS is current.”</li>
    <li><b>Reboot warning</b> — Shown whenever an OS upgrade is either available or a specific image is selected, reminding you the device will reboot.</li>
</ul>

<h4>Recommendation banners and coordination</h4>
<ul>
    <li><b>Upgrade Recommendation Banner</b> — When both FPP and OS upgrades exist (non-major), it reads <b>Recommended: Upgrade OS First</b> because the OS image already contains a fresh FPP. On major upgrades the banner is hidden and the OS card is directly recommended.</li>
    <li><b>Card highlighting</b> — The recommended card gets an amber border (<code>is-recommended</code>) and the other card is faded (<code>is-disabled</code>), driven by <code>getRecommendedPath()</code> → <code>setCoordinatedCards()</code>. Until the update check answers, fading is suppressed so the page does not falsely claim “Up to Date”.</li>
    <li><b>End-of-Life banner</b> — When <code>isEndOfLife</code> is true, both cards explain that the current major version is unsupported and an OS upgrade is the only path, even if no matching image is yet listed.</li>
</ul>

<h4>Progress dialogs and update check</h4>
<ul>
    <li><b>FPP Upgrade progress</b> — Streams <code>manualUpdate.php?wrapped=1</code> into a dialog. Stages are declared by the backend via <code>logStage()</code> and shown in the dialog title as “FPP Upgrade — Stage” via <code>FPPUpgradeProgress()</code> parsing the last marker.</li>
    <li><b>Shared update check</b> — The check is performed once per page load via <code>fpp.js → checkForFppUpdate()</code> and cached in <code>FPP_UPDATE_STATE</code>. This page subscribes to <code>fpp:updateStatusChanged</code> and re-renders from the same object as the navbar icon/menu banner, so they never disagree. A cold-cache “checked:false” response is treated as unknown, not “up to date”, and is retried after a few seconds.</li>
</ul>

<h4>Tips</h4>
<ul>
    <li>Always <b>Backup</b> first on a major upgrade — the banner’s “backup your configuration” link goes directly to <code>backup.php</code>.</li>
    <li>If an OS upgrade is offered, do it instead of a separate FPP update — you’ll get both at once.</li>
    <li>Leave <b>Show All Platforms</b> and <b>Show Legacy OS</b> off unless a developer explicitly asks you to pick a non-matching image.</li>
</ul>
