<h3>Error Report (F8)</h3>
<p>
    Press <b>F8</b> on any page (or open <b>Help → Error Report</b>) when something isn’t working and you’d like help.
    The tool builds a tidy, privacy-safe <b>.zip</b> you can attach to a GitHub Bug Report
    (<code>.github/ISSUE_TEMPLATE/bug_report.md</code>). Everything is scrubbed <b>on your FPP</b> — passwords and Wi-Fi keys become <code>**REDACTED**</code> — and nothing is uploaded automatically. You stay in control of what you share.
</p>
<p class="text-muted small"><i class="fas fa-shield-halved me-1"></i> Same redaction rules as Crash Reports; hostname and network addresses are stripped from the basic info. Use this whenever a developer asks for “logs and system info.”</p>
<h4>What it packages</h4>
<ul>
    <li><b>Vital info (always included)</b> — FPP version/hardware from <code>api/system/status → advancedView</code>,
        scrubbed <code>fpp-info.json</code> (hostname/addresses removed), <code>cape-info.json</code> if present, and
        <code>plugins.json</code> (installed plugin names).</li>
    <li><b>Settings (redacted)</b> — <code>settings</code> / <code>settings/*</code> via the same redactor as
        <code>scripts/generate_crash_report</code> (keys with <code>type=password</code>, <code>pii</code>, or
        <code>piiPurpose</code> in <code>www/settings.json</code>, plus backstop for password-shaped names and URLs;
        <code>Enable</code> flags are kept).</li>
    <li><b>fppd.log (last 5000 lines)</b> — <code>logs/fppd.log</code> tail, redacted. Default on because bug reports
        without it are usually unactionable.</li>
    <li><b>System logs</b> — <code>logs/apache2-error.log</code>, <code>/tmp/fppd_crash_log_ring.log</code>,
        and <code>logs/boot.log</code> (<code>dmesg</code> + <code>journalctl -b -u fppinit -u fppoled -u fpp_postnetwork</code>).</li>
    <li><b>Network (allowlisted)</b> — <code>config/interface.*</code> filtered to <code>gatherStats:true</code> fields in
        <code>www/interface-settings.json</code> (SSID/PSK/addresses never ship; fails closed).</li>
    <li><b>Config</b> — <code>config/*</code> redacted/stubbed (binaries stubbed).</li>
    <li><b>Playlists</b> — <code>playlists/*</code> redacted.</li>
</ul>
<p><b>Defaults:</b> Vital (locked) + Settings (redacted) + fppd.log on; rest off.</p>
<h4>Steps — a quick wizard</h4>
<p class="small text-muted">Click <b>Next</b> to move through the steps. The green <b>Next</b> button is at the bottom-right; on Step 3 it becomes <b>Download Error Report</b>. <b>Open GitHub</b> stays at the bottom-left the whole time.</p>
<ol>
    <li><b>Step 1 — Basic Info (always included, expanded):</b> A friendly preview of your system — FPP version, branch, platform, mode, OS, kernel and plugin list. This is the context every bug report needs. Cape info is added when available. No action needed — just check it looks right and hit <b>Next</b>.</li>
    <li><b>Step 2 — Choose Files:</b> Pick what else to add. We default to <b>Settings (redacted)</b> and <b>Recent log (last 5,000 lines)</b> — that’s what fixes most issues. Leave them on unless a developer asks for more. Extra options (system logs, network safe fields, config, playlists) are there if needed and are also redacted.</li>
    <li><b>Step 3 — Download Report:</b> Hit the green <b>Download Error Report</b> button — FPP builds <code>/home/fpp/media/tmp/error-report-&lt;timestamp&gt;.zip</code> on your device and downloads it to your computer/phone. Re-toggling in Step 2 and re-downloading makes a fresh zip; old zips are pruned after 24h. After the download, follow the “How to post on GitHub” box in the dialog — it walks you through dragging the zip onto the issue. Or keep it handy and share it however a helper asks.</li>
</ol>
<div class="alert alert-light border small">
    <div class="fw-semibold mb-1"><i class="fab fa-github me-1"></i> How to post on GitHub (about a minute)</div>
    <ol class="mb-0 ps-3" style="line-height:1.5;">
        <li>Click <b>Open GitHub</b> (bottom-left) — opens <code>FalconChristmas/fpp</code> issues.</li>
        <li>Click <b>New issue</b> → choose <b>Bug report</b>, describe what happened and what you expected.</li>
        <li>Drag <code>error-report-*.zip</code> from your Downloads onto the issue — or click <i>attach files</i>. Up to 25 MB.</li>
        <li>Submit. You can also open the zip first to double-check — it’s a normal zip.</li>
    </ol>
</div>
<h4>Keys</h4>
<ul>
    <li><b>F8</b> — open/close Error Report. Ignored when typing in an input/textarea/select.</li>
    <li><b>F1</b> — help (unchanged). <b>ESC</b> — close modal.</li>
</ul>
