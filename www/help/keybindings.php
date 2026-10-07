<h3>Keyboard Shortcuts</h3>
<p>Custom shortcuts work on every page. They trigger a command preset, run an FPP command, or open a page, and are stored in the <code>keyBindings</code> setting. Configure them from Settings &rarr; UI &rarr; Keyboard Shortcuts &rarr; <b>Configure Key Bindings</b>.</p>

<h4>System shortcuts</h4>
<p>These work on every page. <b>F1</b> and <b>Esc</b> cannot be changed; <b>F2</b> and <b>F8</b> do their default action only until you assign a custom shortcut to that key.</p>
<ul>
    <li><b>F1</b> &mdash; Help &ndash; opens help for the current page. Press again or <kbd>Esc</kbd> to close. While this dialog is open, help shows this page instead of the settings help.</li>
    <li><b>F2</b> &mdash; Settings &ndash; opens the FPP Settings page, unless assigned to a custom shortcut. On the Channel Outputs <b>Pixel Strings</b> tab, F2 instead sets the start channel of the next string (and a custom F2 shortcut does not run there).</li>
    <li><b>F8</b> &mdash; Error Reporting &ndash; opens or closes the diagnostic report dialog, unless assigned to a custom shortcut.</li>
    <li><b>Esc</b> &mdash; Close &ndash; closes the Help or Error Reporting dialog.</li>
</ul>

<h4>Custom shortcuts</h4>
<ul>
    <li><b>Keys</b> &mdash; Click the field, then press the combination. Combinations must use <b>Ctrl</b> or <b>Alt</b> with another key, or be a function key (<b>F2</b>&ndash;<b>F4</b>, <b>F6</b>&ndash;<b>F10</b>). Assigning <b>F2</b> or <b>F8</b> replaces its default action. <b>F1</b> is reserved for Help, and browser and text-editing shortcuts (copy, paste, undo, find, reload, new tab, zoom, word-by-word cursor movement, ...) are rejected, since taking them over would break them on every page. <b>Esc</b> cancels recording, <b>Backspace</b> clears the field.</li>
    <li><b>Action type</b> &mdash; <b>Command preset</b> triggers one of your presets by name; <b>FPP command</b> runs any command with the arguments you enter; <b>Page</b> opens an FPP page. For a complex command, save it as a preset on the Command Presets page first and bind the preset, so the full argument editors are available.</li>
    <li><b>Test (play button)</b> &mdash; Runs that row's action immediately without pressing the keys.</li>
    <li><b>Save shortcuts</b> (button) &mdash; Validates for empty keys, reserved or browser shortcuts, and duplicates, then saves. Shortcuts apply on every page once saved; there is nothing to restart.</li>
</ul>
