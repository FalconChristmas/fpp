<h3>Error Report (F8)</h3>
<p>
    Press <b>F8</b> on any page (or open <b>Help → Error Report</b>) when something isn’t working and you’d like help.
    The tool builds a full crash-style report <b>on your FPP</b> via <code>POST /api/crashes/report</code>
    (<code>{"client":"F8 Error Report"}</code>), then helps you send it with consent. Everything is scrubbed
    <b>on your FPP</b> — passwords and Wi-Fi keys become <code>**REDACTED**</code>. Nothing is sent until you confirm.
</p>
<p class="text-muted small"><i class="fas fa-shield-halved me-1"></i> Same redaction rules as Crash Reports; hostname and network addresses are stripped from the basic info. Use this whenever a developer asks for “logs and system info.”</p>
<h4>What it packages</h4>
<p class="small text-muted">The report is always built at the full “configuration and logs” level by fppd (level 3) — there is nothing to select:</p>
<ul>
    <li><b>Vital info</b> — FPP version/hardware from <code>api/system/status → advancedView</code>,
        scrubbed <code>fpp-info.json</code> (hostname/addresses removed), <code>cape-info.json</code> if present, and
        <code>plugins.json</code>.</li>
    <li><b>Settings</b> (redacted), <b>configuration</b> (interface files allowlisted, binaries stubbed),
        <b>logs</b> (<code>fppd.log</code>, <code>apache2-error.log</code>, boot log, crash ring).</li>
</ul>
<p class="small text-muted">Reports are named like <code>fpp-&lt;platform&gt;-&lt;version&gt;-&lt;uuid&gt;-&lt;timestamp&gt;-manual.zip</code> and kept in <code>media/crashes/</code> (two manual reports at most).</p>
<h4>Steps — a quick wizard</h4>
<p class="small text-muted">Click <b>Next</b> to move through the steps. The green button at the bottom-right advances the flow: <b>Next</b> → <b>Build Report</b> → <b>Send Report</b> → <b>Done</b>. <b>Open GitHub</b> stays at the bottom-left the whole time.</p>
<ol>
    <li><b>Step 1 — Basic Info:</b> A friendly preview of your system — FPP version, branch, platform, mode, OS, kernel and plugin list. No action needed — just check it looks right and hit <b>Next</b>.</li>
    <li><b>Step 2 — Review:</b> Read what a full level-3 report contains and where it goes (<a href="settings.php#settings-privacy">Settings › Privacy</a>). Hit <b>Next</b> to continue to building.</li>
    <li><b>Step 3 — Build, Send &amp; GitHub:</b> Hit <b>Build Report</b> — fppd builds the zip (seconds on fast players, minutes on a Pi Zero/BeagleBone; one build per minute; shows <i>busy</i>/<i>rate-limited</i>/<i>build-failed</i>/<i>unavailable</i> errors). Optionally download a copy from the <code>api/file/Crashes/&lt;File&gt;</code> link to inspect it. Then hit <b>Send Report</b> — you’ll see the privacy disclosures and confirm; the player sends it (or your browser does if the player has no internet) and the file is deleted once delivery is confirmed. Cancelling sends nothing. Finally use <b>Open GitHub</b>, which prefills the filename, FPP version and platform into a Bug Report.</li>
</ol>
<div class="alert alert-light border small">
    <div class="fw-semibold mb-1"><i class="fab fa-github me-1"></i> How to post on GitHub (about a minute)</div>
    <ol class="mb-0 ps-3" style="line-height:1.5;">
        <li>Click <b>Send Report</b> and confirm — note the <code>*-manual.zip</code> filename.</li>
        <li>Click <b>Open GitHub</b> (bottom-left) — opens <code>FalconChristmas/fpp</code> issues with filename, version and platform prefilled.</li>
        <li>Click <b>New issue</b> → choose <b>Bug report</b>, describe what happened and what you expected.</li>
        <li>Submit and mention the sent report filename so developers can find it.</li>
    </ol>
</div>
<h4>Keys</h4>
<ul>
    <li><b>F8</b> — open/close Error Report. Ignored when typing in an input/textarea/select.</li>
    <li><b>F1</b> — help (unchanged). <b>ESC</b> — close modal.</li>
</ul>
