<h3>Error Report (F8)</h3>
<p>
    Press <b>F8</b> (<b>Fn+F8</b> on a Mac) on any page (or use the <b>Error Report</b> button on the Get Help or Troubleshooting pages)
    when something isn’t working and you’d like the developers to take a look.
    The tool builds a diagnostic report <b>on your FPP</b> via <code>POST /api/crashes/report</code>
    (<code>{"client":"F8 Error Report"}</code>), then helps you send it. Nothing is sent until you review and confirm it.
</p>
<p class="text-muted small">See <a href="settings.php#settings-privacy">Settings › Privacy</a> for what reports contain and who receives them.</p>
<h4>What it packages</h4>
<p class="small text-muted">The report is always built at the full “configuration and logs” level by fppd — there is nothing to select:</p>
<ul>
    <li><b>System information</b> — FPP version, platform, OS, plugin list and cape info.</li>
    <li><b>Settings and configuration</b></li>
    <li><b>Logs</b> (<code>fppd.log</code>, <code>apache2-error.log</code>, boot log, crash ring).</li>
</ul>
<p class="small text-muted">Reports are named like <code>fpp-&lt;platform&gt;-&lt;version&gt;-&lt;uuid&gt;-&lt;timestamp&gt;-manual.zip</code> and kept in <code>media/crashes/</code> (two manual reports at most).</p>
<h4>Steps — a quick wizard</h4>
<p class="small text-muted">All three steps are listed in the window. The step you are on is open and highlighted; the others stay collapsed. A finished step shows a green check and a short summary, and clicking its header reopens it to look back. Steps you have not reached yet cannot be opened. The green button at the bottom-right does the next thing: <b>Build Report</b> → <b>Send Report</b> → <b>Open GitHub</b>.</p>
<ol>
    <li><b>Step 1 — Build Report:</b> fppd builds the zip on your FPP (seconds on fast players, minutes on a Pi Zero/BeagleBone; one build per minute). Errors shown: <i>busy</i>, <i>rate-limited</i>, <i>build-failed</i>, <i>unavailable</i>. When the build finishes the wizard moves straight on to Step 2.</li>
    <li><b>Step 2 — Review &amp; Send:</b> Shows the report’s filename (click it to download and look inside; the copy button copies the name), the system-information preview, and exactly what the report includes and who receives it — hover or tap each <i class="fas fa-question-circle"></i> for the Settings › Privacy explanation. Clicking <b>Send Report</b> is your confirmation; there is no second prompt. If the player has no internet, your browser sends it instead. The file is deleted from the player only once delivery is confirmed; if it is not, the report stays and you can try again.</li>
    <li><b>Step 3 — GitHub Issue:</b> <b>Open GitHub</b> (the green button, between <b>Done</b> and <b>Close</b>) opens <code>FalconChristmas/fpp</code> issues with the report filename, FPP version and platform already filled in. Choose <b>Bug report</b>, describe the problem and submit, then click <b>Done</b>. The copy button next to the filename helps when you add it to an existing issue instead.</li>
</ol>
<h4>Keys</h4>
<ul>
    <li><b>F8</b> — open/close Error Report. On a Mac hold <b>Fn</b> and press <b>F8</b>: F8 on its own is the Play/Pause media key and never reaches the browser (unless “Use F1, F2, etc. keys as standard function keys” is turned on in macOS Keyboard settings). Ignored when typing in an input/textarea/select.</li>
    <li><b>F1</b> — help (shows this page on top of the wizard). <b>ESC</b> — close modal.</li>
</ul>
<h4>Closing part-way through</h4>
<p>Before a report has been built, closing the window (the X, <b>Close</b>, <b>ESC</b>, <b>F8</b> or a click outside it) just closes it. Once a report is being built, or has been built but not sent, closing asks first: <b>Delete Report</b> deletes that report from the player and closes the wizard (a build still running is deleted as soon as it finishes); <b>Keep Working</b> returns you to where you were. While a report is being sent the window stays open until the send finishes. After it has been sent, the report is already gone from the player and the window closes without asking.</p>
