<h3>System</h3>
<p>Identity, hardware and OS-level security for this FPP. Some fields appear only on certain platforms or with Advanced UI level.</p>

<h4>Identity / Host (top of page; varies by container/desktop vs bare metal)</h4>
<ul>
    <li><b>Host Name</b> / <b>Host Description</b> — System-wide identity also shown near the header. Host Name is the device’s mDNS/hostname; Host Description is free-text shown in discovery. The desktop/container variant shows only Host Description.</li>
</ul>

<h4>System Settings</h4>
<ul>
    <li><b>GPIO 14 Fan Control</b> (checkbox, Raspberry Pi, Advanced) — Uses GPIO 14 PWM to control a cooling fan. When checked, the temperature field below appears.</li>
    <li><b>Fan On Temperature</b> (number, °C, 30–85, Advanced, Raspberry Pi) — Temperature above which the fan turns on. Child of the fan control above. Shown as a discrete setting for the single-trip case; multi-trip fans are rendered dynamically below as <code>FanTrip_&lt;zone sanitized&gt;_&lt;index&gt;</code> inputs via <code>PrintFanThermalSettings()</code> (e.g., “Fan On (Speed 1)”, “Fan Speed 2/3…” with a <b>Reset to Defaults</b> button that POSTs <code>api/settings/fanThermal/reset</code>). Values are stored in °C but displayed in °F when <i>Temperature display units = Fahrenheit</i>.</li>
    <li><b>Fan Temperatures</b> (dynamic section, when a fan thermal zone is present) — Per-zone rows (e.g., CPU, K16-Pro) with trip points sorted low→high. Changes apply immediately and at every boot via <code>FPPINIT_Config</code>. Use the Reset button to restore hardware defaults.</li>
    <li><b>Status Display</b> (dropdown via <code>/dev/i2c-*</code> check) — OLED/LCD on the Pi/BeagleBone I²C bus. Options: Disabled, 128×64 SSD1306 (normal/flipped/h-flipped), 128×64 2-color variants, 128×32 variants, SH1106/SH1107, SSD1327, PCF8574/PCF8574A 16×2/20×4 I²C LCDs, etc. <i>Reboot required.</i></li>
    <li><b>FPPD Boot Delay</b> (dropdown) — Wait after boot before starting <code>fppd</code> so slow switches/routers can come up and E1.31 multicast works. Options: 0–10s, 15/20/25/30s, 1/2/3/5 min, <b>Auto</b> (−1, wait until NTP/RTC time is valid or 5 min max). Not shown inside containers.</li>
    <li><b>Override UUID</b> (text, Advanced, pii) — Replacement UUID for FPP Connect dedup / stats / services instead of the CPU/serial-derived one. <i>Reboot required.</i></li>
</ul>

<h4>BeagleBone LEDs (BeagleBone Black / 64 only)</h4>
<ul>
    <li><b>USR0</b> (dropdown via <code>api/settings/BBBLeds/options</code>) — Function of the USR0 LED. Default <code>heartbeat</code>.</li>
    <li><b>USR1</b> (dropdown) — Function of the USR1 LED. Default <code>mmc0</code>.</li>
    <li><b>USR2</b> (dropdown) — Function of the USR2 LED. Default <code>cpu</code>.</li>
    <li><b>USR3</b> (dropdown) — Function of the USR3 LED. Default <code>mmc1</code>.</li>
    <li><b>Power</b> (dropdown, BeagleBone Black only) — Power indicator LED: <b>Enabled</b>/ <b>Disabled</b>.</li>
</ul>

<h4>OS Password (when not in container; SSH-related fields in Advanced)</h4>
<ul>
    <li><b>OS password</b> (dropdown) — <code>falcon (Default)</code> vs <code>Enter a Password</code>. Selecting custom reveals Username (fixed <code>fpp</code>), <b>OS Password</b> and <b>OS Verify Password</b> fields.</li>
    <li><b>OS Password / OS Verify Password</b> (passwords) — OS login for user <code>fpp</code> via SSH. Tip: you rarely need to change the default unless the device is on a public network; you can always reset it from the UI even if you forget it.</li>
    <li><b>SSH Keys (root and fpp users)</b> (textarea + buttons, Advanced) — Editor for <code>/root/.ssh/authorized_keys</code> with <b>Save Keys</b> (POST <code>api/configfile/authorized_keys</code>) and <b>Upload authorized_keys</b> file picker.</li>
</ul>

<h4>Reset FPP Config (Advanced)</h4>
<ul>
    <li><b>Reset FPP Config</b> (button) — Opens a checklist dialog streaming <code>resetConfig.php?areas=&lt;…&gt;</code>. Snack buttons: <b>Everything</b> (all except network), <b>Common</b> (sequences/effects/media/playlists), <b>Nothing</b>. Per-area checkboxes: <b>Configuration</b> (Configuration Files, Network Config Files, Channel Outputs, EEPROM/String Config, Settings, Schedule), <b>Content</b> (Sequences, Media, Effects, Playlists), <b>Plugins</b> (Installed Plugins, Plugin Config Files), <b>OS/System Files</b> (Logs, Backups, Uploads, Caches, Scripts, Root/FPP User Files incl. SSH, Audio Backend). Confirm then <i>Reboot Required</i>.</li>
</ul>
