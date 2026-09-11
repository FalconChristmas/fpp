<h3>Email</h3>
<p>Sends alerts from this FPP through your own SMTP account (your ISP’s server or a service like Gmail with an app password). Nothing is stored on a mail server by FPP; it just relays through the account you provide.</p>

<h4>Outbound Email Configuration</h4>
<ul>
    <li><b>SMTP Server Hostname</b> (text, pii) — Address of the server that sends mail, e.g. <code>smtp.gmail.com</code> or your provider’s SMTP host. When empty the group below is hidden; entering a host reveals all related fields.</li>
    <li><b>SMTP Server Port</b> (dropdown) — TCP port on the SMTP server. Options: <b>25</b> (unencrypted), <b>465</b> (implicit TLS), <b>587</b> (STARTTLS, most common for encrypted mail). Default 587.</li>
    <li><b>SMTP Server Login</b> (text, pii) — Username to log into the SMTP server. Often the same as the From address.</li>
    <li><b>SMTP Server Password</b> (password) — Password for the SMTP login. For Gmail, use an app-specific password, not your normal Google password.</li>
    <li><b>From Email Address</b> (text, pii) — The envelope-from address mail should appear to come from.</li>
    <li><b>From Name</b> (text, pii) — Display name in the From field, e.g. “FPP Porch”.</li>
    <li><b>Default TO Address</b> (text, pii) — Where alerts are delivered by default.</li>
    <li><b>Configure Email</b> (button) — Writes an <code>exim4</code> config and reloads mail services to apply the above. Call this after saving changes.</li>
    <li><b>Send Test Email</b> (button) — Sends a test message to the Default TO address. Success is a growl; failure shows an error dialog — fix hostname/port/login before retrying.</li>
</ul>
