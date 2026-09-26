<h3>Packages</h3>
<p>The Package Manager lets you add, reinstall or remove extra software packages on the FPP operating system (the underlying Debian/Raspberry Pi OS or BeagleBone image). Most shows do not need to touch this page — only use it when a plugin’s instructions or a specific need tells you to install a system package such as a font, codec, library or utility.</p>

<div class="callout callout-warning"><b>Installing or reinstalling packages can break your FPP installation requiring complete reinstallation of FPP. Continue at your own risk.</b> Packages are managed by the OS package manager; a bad package, wrong architecture or interrupted install can leave the system in a state that requires reflashing.</div>

<h4>Installed User Packages</h4>
<ul>
    <li><b>List</b> (<code>#userPackagesList</code>, loaded from <code>LoadUserPackages()</code> via <code>api/system/packages</code> handling) — Shows every package that has been requested through this page or by a plugin, normalized to <code>package → [requesters]</code> whether the file was legacy <code>string[]</code> or the newer object form. An empty list shows <i>No managed packages found.</i></li>
    <li><b>Per-row details</b> — For each package the page calls <code>GET /api/system/packages/info/{name}</code> to learn if it is currently <b>Installed: Yes/No</b>. Each row shows the package, its status (<b>Installed</b> / <b>Missing</b>), who requires it (<code>user</code> displays as “you”, any other repo name is the plugin that depends on it) and its actions. Packages that an install pulled in as dependencies are counted on their parent’s row as “+N dependencies”; at the <b>Developer</b> UI level they are listed indented under it instead, each marked “dependency of …”. Uninstalling the parent releases them too. A package that was already installed when it was first requested (part of the FPP image, or installed by hand) is never listed: FPP did not install it, so FPP never removes it.</li>
    <li><b>Reinstall</b> (always shown, yellow outline, sync icon, to the right of Uninstall) — Streams <code>packagesHelper.php?action=reinstall&package={name}</code> into the progress dialog. Use when a package is reported as installed but is broken or missing files.</li>
    <li><b>Uninstall</b> (red outline, enabled only when <code>user</code> is among the requesters) — Streams <code>packagesHelper.php?action=uninstall&package={name}</code>, releasing the package and the dependencies its install pulled in. Plugin-owned packages, and dependencies whose parent is still installed, show the button greyed out; hover it for the reason (uninstall the owning plugin, or the parent package, first). FPP also refuses any removal that would take other packages with it (<code>apt-get remove</code> drops every reverse dependency), and leaves the row in place so it can be retried. Packages recorded by an older FPP release before the “already installed” rule existed can still be removed when their plugin is uninstalled if nothing else depends on them; use <b>Install a Package</b> below to put one back.</li>
    <li><b>Missing</b> / <b>Unknown</b> status — a package that is not currently installed (or whose status could not be read) shows a red <b>Missing</b> (grey <b>Unknown</b>) badge; Reinstall puts it back, and Uninstall, when enabled, just drops the row.</li>
    <li><b>Loading order</b> — Rows are rendered only after all per-package info requests complete (counter <code>pendingRequests</code>), so the list appears atomically rather than flickering in piece by piece. Errors for a single package log to console and show an <b>Unknown</b> status instead of breaking the whole list.</li>
</ul>

<h4>Install a Package</h4>
<ul>
    <li><b>Package name field</b> (<code>#packageInput</code>, <code>form-control-lg form-control-rounded has-shadow</code>, placeholder “Enter package name”) — Start typing a Debian package name (e.g. <code>fonts-dejavu</code>).</li>
    <li><b>Autocomplete</b> — After <b>2 characters</b> and a 300 ms delay, filters the full system package list (105k+ on trixie) via <code>$.ui.autocomplete.filter</code> (case-insensitive substring match). Results are <b>capped to 50</b> to avoid flooding the DOM for short terms like “li” or “py” which match thousands. The dropdown itself is limited to 250 px height with scroll (<code>.ui-autocomplete max-height:250px</code>). Selecting an item fills the field and closes the menu.</li>
    <li><b>Get Info</b> (blue outline, info icon) — Calls <code>GET /api/system/packages/info/{trimmed name}</code>. Empty input is rejected with an alert. On success shows:
        <ul>
            <li><b>Selected Package:</b> {name} plus <b>(Already Installed)</b> when <code>Installed==Yes</code></li>
            <li><b>Description</b> (or “No description available”)</li>
            <li><b>Will also install these packages (if not already installed):</b> when not yet installed — the <code>Depends</code> field with version constraints (<code>(…)</code>) stripped</li>
            <li><b>Dependencies:</b> when already installed — same Depends list but labelled as current dependencies</li>
            <li><b>Install Package</b> (green, large, download icon) when not installed, or <b>Reinstall Package</b> (yellow, sync icon) when already installed — both stream via <code>packagesHelper.php</code> into the progress dialog.</li>
        </ul>
        Errors show an inline <b>Error:</b> line; fetch failures alert “failed to fetch package information.”</li>
</ul>

<h4>Progress dialog</h4>
<p>Install, Reinstall and Uninstall all use <code>DisplayProgressDialog("packageProgressPopup", "… Package: {name}")</code> and <code>StreamURL(..., 'packageProgressPopupText', 'ProgressDialogDone', 'ProgressDialogDone')</code>. The dialog streams the underlying <code>apt</code> output line by line; “Done” re-enables the close button. The taller-modal CSS (<code>90% height, 250px autocomplete limit</code>) prevents the popup from overflowing on large dependency trees.</p>

<h4>Tips</h4>
<ul>
    <li>Prefer <b>Get Info</b> before installing — check the “Will also install” list so you know what else will be pulled in.</li>
    <li>If a plugin depends on a package, do not try to force-remove it here; uninstall the plugin instead.</li>
    <li>After a failed or interrupted package operation, use <b>Reinstall Package</b> on that package before retrying other actions, and keep the device on reliable power and network during the operation.</li>
</ul>
