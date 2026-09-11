<h3>Services and Kiosk</h3>
<p>Enables background services that let other devices reach files on this FPP, and optionally a local HDMI kiosk.</p>

<h4>OS Services (4 checkboxes)</h4>
<ul>
    <li><b>Enable rsync</b> (checkbox) — Starts the rsync daemon so other FPP systems can copy sequences/media/OS files <i>to</i> this device via <i>Content Setup → MultiSync / File Copy</i>. Enable on the <b>destination</b> FPP whenever you plan to push files to it.</li>
    <li><b>Enable Samba/CIFS</b> (checkbox, Advanced) — Starts Samba (<code>smbd</code>/<code>nmbd</code>) for Windows file sharing to browse this device as a network share.</li>
    <li><b>Enable FTP</b> (checkbox, Advanced) — Starts vsftpd for FTP file access.</li>
    <li><b>Enable Local MQTT Broker</b> (checkbox, Advanced) — Starts a Mosquitto broker on localhost:1883 using the <code>fpp</code> user’s credentials. Use it when you want this FPP itself to act as the broker rather than an external host.</li>
</ul>

<h4>Kiosk Mode (Raspberry Pi, Advanced)</h4>
<ul>
    <li><b>Kiosk Start URL</b> (text, pii) — URL shown on the Pi’s HDMI output when kiosk is active. Default = main FPP page (<code>http://localhost/</code>). Example: Big Buttons plugin at <code>http://localhost/plugin.php?_menu=status&amp;plugin=fpp-BigButtons&amp;page=bigbuttons.php&amp;nopage=1</code>.</li>
    <li><b>Kiosk Screen DPMS Timeout</b> (number, 0–600 seconds) — Seconds until the HDMI display is powered off via DPMS. Any touch wakes it.</li>
    <li><b>Rotate Kiosk if using Raspberry Pi Touch Display 2 7-inch</b> (checkbox, Raspberry Pi) — Applies the rotation workaround for that panel.</li>
    <li><b>Enable Kiosk / Disable Kiosk</b> (button) — Installing enables Chrome + supporting packages (~400–470 MB) on the HDMI port so a keyboard/mouse on the Pi’s USB can configure FPP locally. Disabling restores normal Player mode. Both show a progress dialog and set the <i>Reboot Required</i> flag when done.</li>
</ul>
