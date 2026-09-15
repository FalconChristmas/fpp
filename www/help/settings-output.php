<h3>Output</h3>
<p>How FPP handles incoming data (bridge inputs) and outgoing light data.</p>

<h4>Input Control</h4>
<ul>
    <li><b>Disable Network Bridge Monitoring (E1.31/DDP/ArtNet)</b> (checkbox, Advanced) — FPP listens on bridge ports even when not in Bridge mode to warn about misconfigured lights. Check only if you run your own listener software on those ports.</li>
    <li><b>Bridge Input Source Priority</b> (checkbox, Advanced) — When multiple senders hit the same input universe, lock onto one and suppress others. For E1.31 the packet’s priority byte wins (higher priority always takes over). For ArtNet the first source wins. If the active source stops longer than the input timeout, any source can take over. DDP is unaffected.</li>
    <li><b>Bridge Data Priority</b> (dropdown) — What to do when live bridge data arrives while a sequence is already playing. Options: <b>Warn If Sequence Running</b> = keep the sequence and log a warning; <b>Prioritize Bridge</b> = live data wins; <b>Prioritize Sequence</b> = sequence wins.</li>
</ul>

<h4>Output Control</h4>
<ul>
    <li><b>Automatically turn on/off outputs</b> (checkbox, Advanced) — Lets FPP cut power to pixel ports with controllable power when there is nothing to send. Overridden by “Always transmit channel data” below.</li>
    <li><b>eFuse Retry Count</b> (number, 0–20, Advanced) — For controllers with eFuse, how many times FPP automatically resets and retries after a trip. 0 = disabled. Useful for intermittent startup trips.</li>
    <li><b>eFuse Retry Time Interval</b> (number, 100–5000 ms, Advanced) — Wait between eFuse retries. Default 100 ms. Only matters when Retry Count is non-zero.</li>
    <li><b>Always transmit channel data</b> (checkbox, Advanced) — Keep sending data even when no sequence, effect or overlay is active. Prevents some controllers from entering built-in test patterns when data stops.</li>
    <li><b>E1.31 Bridging Transmit Interval</b> (dropdown, Advanced) — Rate at which bridged E1.31 is re-transmitted. Options: 10, 25, 40, 50, 100 ms. Default 50 ms. Match this to the rate your Player is sending if you bridge between networks.</li>
</ul>
