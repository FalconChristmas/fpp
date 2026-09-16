<h3>Privacy</h3>
<p>This page and the privacy step in the setup wizard show the <i>same</i> table and save the same way, with instant apply. All rows start from what this device actually has in its settings file, not defaults, and each save also stamps a consent record so changing your mind is recorded too.</p>

<h4>The table (6 rows — order matters)</h4>
<ul>
    <li><b>Share usage statistics</b> → <i>Goes to FPP &amp; xLights developers</i> — Anonymous stats about platforms and features so developers keep supporting what people actually use. <i>Click “Preview data” on the page to see the exact JSON before anything is sent. Corresponds to <code>statsPublish</code>.</i></li>
    <li><b>Load cape logos from vendor website</b> → <i>Cape vendor</i> — When you open the UI with a cape installed, FPP fetches the vendor’s logo from their site, which discloses your IP to that vendor. <i>Controls <code>FetchVendorLogos</code>.</i></li>
    <li><b>…and include your cape’s serial number</b> (indented child) → Vendor — Appends <code>?sn=&amp;id=&amp;cs=</code> to the logo URL above so the vendor can identify you. Technically a sub-choice of the logo row — denying the logo automatically denies the serial. <i>Controls <code>SendVendorSerial</code>; only meaningful when the row above is allowed.</i></li>
    <li><b>Send crash reports</b> → <i>FPP &amp; xLights developers, AI service</i> — If fppd crashes, build a report. This is how most crashes are found. <i>First level of <code>ShareCrashData</code> ladder; see the setting below for the levels.</i></li>
    <li><b>…and include your settings</b> (indented, ladder) — Include a scrubbed settings file so developers can reproduce the configuration that crashed.</li>
    <li><b>…and include configuration and logs</b> (indented, ladder) — Include channel configs plus recent logs for fuller reproduction. Passwords, Wi-Fi keys, addresses and location are redacted; network interface config is never included.</li>
    <li><b>E-mail address</b> (optional, text, pii, <i>crash-report purpose</i>) — Contact for crash follow-up only. Shown only when crash reporting is at least “stack traces only”; sent only at that level and above. Leave blank to stay anonymous.</li>
</ul>

<h4>Controls</h4>
<ul>
    <li><b>Allow / Deny</b> (radio per row) — Each row is a binary choice. Indented rows inherit and extend the parent level — deeper “allow” implies the parent “allow”.</li>
    <li><b>Allow all / Deny all</b> (footer buttons) — Sets every row to Allow or Deny at once. You can then flip individual rows back.</li>
    <li><b>Preview data</b> (link in the usage-statistics sub-caption) — Renders exactly what would be sent for stats. Nothing leaves this device until you allow it.</li>
    <li><b>Autosave</b> — No Apply button; each change writes via <code>api/settings/{key}</code> and stamps <code>api/privacy/consent</code> with <code>via:"settings"</code> and a timestamp for only the keys that actually changed. A withdrawal is recorded like a grant — denial is also an act.</li>
</ul>

<h4>Behind the scenes</h4>
<ul>
    <li><b>ShareCrashData</b> (select, 5 options, ladder): <b>Disabled — do not create a report</b> (-1), <b>Keep locally, do not send</b> (0, still writes to crashes folder), <b>Send stack traces only</b> (1, includes stack, registers, playlist position, plugin list, plus contact email if given — no device name/addresses/settings), <b>Send, include settings</b> (2), <b>Send, include settings and configurations</b> (3). Default 3.</li>
    <li><b>Legal Jurisdiction</b> (set in <i>Setup → Location / Time &amp; Location</i>) determines whether privacy rows start pre-filled or blank for users in prior-opt-in regions. Once answered, your answer wins over any inference from time zone.</li>
</ul>
