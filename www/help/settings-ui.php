<h3>User Interface</h3>
<p>Changes appearance and which advanced controls you see. No reboot is needed for most; UI-level changes reload the UI.</p>

<h4>User Interface</h4>
<ul>
    <li><b>Temporary User Interface Level</b> (button, when in Basic) — <b>Change to Advanced UI for 15 Minutes</b> unlocks Advanced fields for 15 minutes without permanently changing your level. While active you see <b>Advanced (~n min remaining)</b> and an <b>Exit Advanced Mode</b> button. Your saved preference stays Basic.</li>
    <li><b>User Interface Level</b> (dropdown) — How many settings the web UI shows. <b>Basic</b> = common settings. <b>Advanced</b> = most features. <b>Experimental</b> = rarely-needed/expert features that can affect performance. <b>Developer</b> = dev tools. Defaults to Basic. Reloads the UI on change.</li>
    <li><b>Theme</b> (dropdown) — <b>System Default</b> follows your browser/OS dark-mode preference; <b>Light</b> or <b>Dark</b> forces that theme regardless of the OS.</li>
    <li><b>Display all hardware options/settings</b> (checkbox, Advanced) — When FPP detects a connected cape/hat, it normally hides incompatible or rarely-used options. Check to force all options visible, even for missing hardware. Reloads UI.</li>
    <li><b>Disable restart/reboot UI Warnings</b> (checkbox, Developer) — Hides the yellow “restart required” / red “reboot required” banners at the top after settings change. Leave it off unless you know what you’re doing.</li>
    <li><b>Disable UI Popover Event Alerts</b> (checkbox, Advanced) — Hides popover alerts in the top-right. These give feedback on events; disable only in special cases. Reloads UI.</li>
    <li><b>File Manager Thumbnail Size</b> (dropdown, Advanced) — Max size of image previews in File Manager. Options: Disabled, 25, 50, 75, 100, 125, 150, 175, 200 pixels. Keeps aspect ratio.</li>
    <li><b>File Manager Enable Filter</b> (checkbox, Advanced) — Adds a filter/search box to the File Manager tables.</li>
    <li><b>File Manager Hide Empty Tabs</b> (checkbox, Advanced) — Hides File Manager tabs that have no files in them. A tab reappears once a file of that type is uploaded; the tab you are on is never hidden.</li>
    <li><b>Hide Cape Controlled GPIO Pins</b> (checkbox, Advanced) — Hides pins the current cape uses from the GPIO page so you do not accidentally reassign them.</li>
</ul>

<h4>UI Password</h4>
<ul>
    <li><b>UI password</b> (dropdown) — <b>No Password</b> (default) = open. <b>Enter a Password</b> = enables HTTP basic auth. The username is always <code>admin</code>. Choosing “Enter a Password” reveals the next three fields and a dedicated <b>Save UI Password</b> button. Switching a protected FPP back to “No Password” removes the password immediately and reloads the page — no Save click needed.</li>
    <li><b>Username</b> (display) — Fixed to <code>admin</code> when password protection is enabled.</li>
    <li><b>Password</b> (password) — The UI password. Must be retyped below. Used by the browser and by xLights FPP Connect.</li>
    <li><b>Verify Password</b> (password) — Retype the same password to confirm.</li>
    <li><b>Save UI Password</b> (button) — Saves password first, then the enable flag, then reloads the page so the browser re-authenticates. Status text shows “Not applied yet”, “Saving…”, “Applying…”, or errors. You will be prompted for <code>admin</code> + the new password after saving. <b>Caution:</b> forgetting this password locks you out until reset via console.</li>
</ul>

<h4>UI Colors</h4>
<ul>
    <li><b>Header Background Color</b> (dropdown) — Pick a header color to tell multiple FPPs apart at a glance.</li>
    <li><b>Color Pair 1-A / 1-B</b> (color pickers) — Alternating row colors for main tables. Default #B0EFBC / #7BE38F.</li>
    <li><b>Color Pair 2-A / 2-B</b> (color pickers) — Alternating row colors for nested tables. Default #CAE7FB / #A6D7F8.</li>
    <li><b>Color Pair 3-A / 3-B</b> (color pickers) — Another nested-table palette. Default #E9BEED / #DA92E1.</li>
    <li><b>Color Pair 4-A / 4-B</b> (color pickers) — Yet another nested-table palette. Default #FCEFBB / #F9E077.</li>
</ul>

<h4>Keyboard Shortcuts</h4>
<p>System shortcuts work on every page and cannot be changed. Custom shortcuts work on every page too, and are stored in the <code>keyBindings</code> setting.</p>
<ul>
    <li><b>Configure Key Bindings</b> (button) — Opens a dialog listing the system shortcuts (<b>F1</b> help, <b>F2</b> Settings, <b>F8</b> Error Reporting, <b>Esc</b> close) and your custom shortcuts.</li>
    <li><b>Keys</b> — Click the field, then press the combination. Combinations must use <b>Ctrl</b> or <b>Alt</b> with another key, or be an unused function key (<b>F3</b>–<b>F7</b>, <b>F9</b>–<b>F12</b>); <b>F1</b>/<b>F2</b>/<b>F8</b> are reserved system shortcuts and rejected on save. <b>Esc</b> cancels recording, <b>Backspace</b> clears the field.</li>
    <li><b>Action type</b> — <b>Command preset</b> triggers one of your presets by name; <b>FPP command</b> runs any command with the arguments you enter; <b>Page</b> opens an FPP page. For a complex command, save it as a preset on the Command Presets page first and bind the preset, so the full argument editors are available.</li>
    <li><b>Test (play button)</b> — Runs that row's action immediately without pressing the keys.</li>
    <li><b>Save shortcuts</b> (button, in the dialog) — Validates for empty keys, reserved keys and duplicates, then saves. Shortcuts apply on every page once saved; there is nothing to restart.</li>
</ul>
