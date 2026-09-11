<h3>Time and Location</h3>
<p>Keep the clock accurate and tell FPP where you are so sunrise/sunset schedules work.</p>

<h4>Time Config</h4>
<ul>
    <li><b>Current System Time</b> (read-only) — Live clock from the system. Updates every second when this tab is open. If wrong, set it below or fix NTP.</li>
    <li><b>Set Date</b> (date, YYYY-MM-DD) — Manually set the system date. Applied immediately. Prefer NTP (below) on internet-connected systems.</li>
    <li><b>Set Time</b> (time, HH:MM:SS 24-hour) — Manually set the system time in 24-hour format.</li>
    <li><b>Real Time Clock</b> (dropdown) — Hardware clock that keeps time while powered off. Auto-detected from options via <code>api/settings/RTC/options</code>. On Pi/BeagleBone with an RTC module or cape, pick it here so the system reads it at boot. <i>Requires reboot.</i></li>
    <li><b>Disable Pi 5 Built-in RTC</b> (checkbox, Pi 5 only) — The Pi 5 has its own RTC that claims <code>rtc0</code> even without a battery, hiding a cape’s RTC during early boot. Check this if no battery is on the Pi’s RTC header so only the cape’s clock is used. <i>Reboot required.</i></li>
    <li><b>Override default NTP Server</b> (text) — Leave blank to use <code>falconplayer.pool.ntp.org</code>. You can point to another FPP’s IP (every FPP runs its own NTP server) for an offline show network, or any NTP host. Blank uses pool; a fixed address is useful behind firewalls that block NTP.</li>
    <li><b>Use NTP Server from DHCP</b> (checkbox, Advanced) — When enabled, DHCP can override the fixed NTP server above. When disabled (default), only the setting you typed is used.</li>
    <li><b>Time Zone</b> (dropdown via <code>api/settings/TimeZone/options</code>) — Local time zone for this FPP. Sets the system time zone and determines when daily schedules fire. <i>Requires FPPD restart.</i></li>
    <li><b>Lookup Time Zone</b> (button) — Tries to guess the time zone from your location and fill the field for you. You can still edit it manually.</li>
</ul>

<h4>Regional Settings</h4>
<ul>
    <li><b>Holiday List</b> (dropdown via <code>api/settings/Locale/options</code>) — Which holidays the scheduler knows (e.g., Thanksgiving, Boxing Day) so you can schedule by name without picking the date each year. <b>Global</b> = no country-specific holidays. This does not change language, time zone, or number formats.</li>
    <li><b>Date Format</b> (dropdown) — How dates appear in the UI. Options: YYYY-MM-DD, MM/DD/YYYY, DD/MM/YYYY, Weekday Month Day, Month Day.</li>
    <li><b>Time Format</b> (dropdown) — How times appear. Options: 24-hour <code>23:40</code> vs 12-hour <code>11:40 PM</code>.</li>
    <li><b>Temperature display units</b> (dropdown) — <b>Celsius</b> or <b>Fahrenheit</b> for temperature displays (e.g., fan thresholds) across the UI.</li>
    <li><b>Latitude</b> (text, pii) — Your location’s latitude. Used with longitude to calculate sunrise/sunset for the scheduler. Default is Falcon, Colorado — change to your site. Tip: copy from a map provider and use decimal degrees.</li>
    <li><b>Longitude</b> (text, pii) — Your location’s longitude. Same purpose as latitude. Together they make “sunset - 30 min” style schedules accurate.</li>
    <li><b>Lookup Location / Show On Map</b> (buttons) — Lookup tries to fill latitude/longitude from your IP/controls; Show opens the current coordinates on a map.</li>
</ul>
