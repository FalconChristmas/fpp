<h3>System</h3>
<p>Identity, hardware and OS-level security for this FPP. Some fields appear only on certain platforms or with Advanced UI level.</p>

<h4>Identity / Host (top of page; varies by container/desktop vs bare metal)</h4>
<ul>
    <li><b>Host Name</b> / <b>Host Description</b> — System-wide identity also shown near the header. Host Name is the device’s mDNS/hostname; Host Description is free-text shown in discovery. The desktop/container variant shows only Host Description.</li>
</ul>

<h4>System Settings</h4>
<ul>
    <li><b>GPIO 14 Fan Control</b> (checkbox, Raspberry Pi, Advanced) — Uses GPIO 14 PWM to control a cooling fan. Takes effect on reboot; the fan’s temperatures are then set under <b>Fan Temperatures</b> below. A cape that declares its own fan in its device tree does not use this setting and removes it.</li>
    <li><b>Fan Temperatures</b> (dynamic section, shown only when a fan thermal zone is present) — Every thermal zone with a fan cooling device bound to it — from the setting above, a cape’s own device tree, or the Pi 5 active cooler — is rendered by <code>PrintFanThermalSettings()</code> as <code>FanTrip_&lt;zone sanitized&gt;_&lt;index&gt;</code> inputs, per zone (e.g. CPU, K16-Pro) with trip points sorted low→high (“Fan On Temperature”, “Fan Speed 2/3…”). Changes are written to sysfs immediately and re-applied at every boot by <code>FPPINIT_Config</code>, since trip points revert to their device tree values on boot. <b>Reset to Defaults</b> POSTs <code>api/settings/fanThermal/reset</code> to restore the hardware values. Stored in °C, displayed in °F when <i>Temperature display units = Fahrenheit</i>.</li>
    <li><b>Prevent HDMI CEC at Boot</b> (checkbox, Raspberry Pi except Pi 5) — Stops the Pi firmware sending the CEC “Active Source” broadcast at boot. That broadcast is what brings a TV or projector out of standby and switches it to the Pi’s input, so enabling this is how you keep a display asleep when the player boots — after a power cut, for instance. <b>It also means the display will no longer come on by itself:</b> FPP never sends CEC of its own, so a set left in standby stays dark until someone uses its remote. A plain monitor is unaffected either way, since it wakes on signal rather than on CEC. Be aware too that a display asleep at boot may drop hot plug detect, in which case its HDMI audio device is unusable and FPP switches the audio output to another card. When enabled, <code>fppinit</code> writes <code>hdmi_ignore_cec_init=1</code> into a managed block in <code>/boot/firmware/config.txt</code>; unlike <code>hdmi_ignore_cec=1</code> that only affects the boot broadcast, leaving CEC itself available. Hidden on the Pi 5 (KMS/RP1, no firmware HDMI path), where the option does nothing and any block left behind is removed. <i>Reboot required.</i></li>
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
