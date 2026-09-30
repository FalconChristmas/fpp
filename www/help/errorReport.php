<h3>Diagnostic Report</h3>
<p>
    Press <b>F8</b> on any page (or use the <b>Diagnostic Report</b> button on the Get Help or Troubleshooting pages)
    when something isn’t working and you’d like the developers to take a look.
    The tool builds a diagnostic report <b>on your FPP</b> via <code>POST /api/crashes/report</code>
    (<code>{"client":"Diagnostic Report"}</code>), then helps you send it. Nothing is sent until you review and confirm it.
</p>
<p class="text-muted small">See <a href="settings.php#settings-privacy">Settings › Privacy</a> for how reports are handled and who receives them.</p>
<h4>What it packages</h4>
<p class="small text-muted">The report is always built at the full “configuration and logs” level by fppd — there is nothing to select:</p>
<ul>
    <li><b>System information</b> — FPP version, platform, OS, plugin list and cape info.</li>
    <li><b>Settings and configuration</b></li>
    <li><b>Logs</b> (<code>fppd.log</code>, <code>apache2-error.log</code>, boot log, the end of the plugin install and FPP upgrade logs, and <code>git status</code>) and your playlists.</li>
    <li><b>System diagnostics</b> — the <a href="troubleshooting.php">Troubleshooting</a> page’s output and the health check, including hardware and USB serial numbers, mounted network shares and network settings (IP and MAC addresses). Nearby Wi-Fi networks are only counted per channel; Wi-Fi network names (yours and nearby), process command lines and git identity are left out.</li>
</ul>
<p class="small text-muted">Reports are named like <code>fpp-&lt;platform&gt;-&lt;version&gt;-&lt;uuid&gt;-&lt;timestamp&gt;-manual.zip</code> and kept in <code>media/crashes/</code> (two manual reports at most).</p>
<h4>Steps — a quick wizard</h4>
<p class="small text-muted">The three steps — <b>Build</b>, <b>Send</b> and <b>GitHub</b> — are buttons along the top of the window, with the step you are on highlighted and shown below them. A finished step shows a green check, and clicking it goes back to look; steps you have not reached yet cannot be opened. The green button at the bottom advances the flow: <b>Build Report</b> → <b>Next</b> → <b>Send Report</b> → <b>Open GitHub</b>.</p>
<ol>
    <li><b>Step 1 — Build Report:</b> fppd builds the zip on your FPP — or, if fppd is not running, FPP builds it without fppd and says so — (seconds on fast players, minutes on a Pi Zero/BeagleBone; one build at a time, and none for 10 seconds after one finishes). Errors shown: <i>busy</i>, <i>rate-limited</i>, <i>build-failed</i>, <i>unavailable</i>. A download link lets you inspect the file first, and the copy button copies its name. If you close the window while a build is running, the build carries on, and opening Diagnostic Report again picks up the finished report. Click <b>Next</b> when you are ready to review it.</li>
    <li><b>Step 2 — Review &amp; Send:</b> Check the system-information preview, then hit <b>Send Report</b> — you’ll see what is sent and confirm. If the player has no internet, your browser sends it instead. The file is deleted from the player once delivery is confirmed (if that delete fails you are told, and can delete it from File Manager › Crash Reports); cancelling sends nothing. If your browser sent it but no delivery confirmation came back, the report is kept on the player and you can send it again.</li>
    <li><b>Step 3 — GitHub Issue:</b> Use <b>Open GitHub</b> (the green button, next to <b>Done</b>), which opens <code>FalconChristmas/fpp</code> issues with the report filename, FPP version and platform already filled in. Choose <b>Bug report</b>, describe the problem, submit, and mention the sent filename — the copy button next to it helps if you add it to an existing issue instead.</li>
</ol>
<p class="small text-muted">Closing the window part-way keeps any report already built in File Manager › Crash Reports, so you can send it later.</p>
<h4>Keys</h4>
<ul>
    <li><b>F8</b> — open/close Diagnostic Report. Ignored when typing in an input/textarea/select.</li>
    <li><b>F1</b> — help (shows this page on top of the wizard). <b>ESC</b> — close the window, also while a report is building.</li>
</ul>
