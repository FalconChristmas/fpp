<h3>Error Report (F8)</h3>
<p>
    Press <b>F8</b> on any page (or use the <b>Error Report</b> button on the Get Help or Troubleshooting pages)
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
<p class="small text-muted">The green button at the bottom-right advances the flow: <b>Build Report</b> → <b>Send Report</b> → <b>Done</b>. <b>Open GitHub</b> appears at the bottom-left once a report has been built.</p>
<ol>
    <li><b>Step 1 — Build Report:</b> fppd builds the zip on your FPP (seconds on fast players, minutes on a Pi Zero/BeagleBone; one build per minute). Errors shown: <i>busy</i>, <i>rate-limited</i>, <i>build-failed</i>, <i>unavailable</i>. A download link lets you inspect the file first.</li>
    <li><b>Step 2 — Review &amp; Send:</b> Check the system-information preview and contents list, then hit <b>Send Report</b> — you’ll see what is sent and confirm. If the player has no internet, your browser sends it instead. The file is deleted from the player once delivery is confirmed; cancelling sends nothing.</li>
    <li><b>Step 3 — GitHub Issue:</b> Use <b>Open GitHub</b>, which opens <code>FalconChristmas/fpp</code> issues with the report filename, FPP version and platform already filled in. Choose <b>Bug report</b>, describe the problem, submit, and mention the sent filename.</li>
</ol>
<h4>Keys</h4>
<ul>
    <li><b>F8</b> — open/close Error Report. Ignored when typing in an input/textarea/select.</li>
    <li><b>F1</b> — help (shows this page on top of the wizard). <b>ESC</b> — close modal.</li>
</ul>
