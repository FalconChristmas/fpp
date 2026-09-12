<h3>Display Testing</h3>
<p>This page drives your lights directly without playing a sequence, so you can verify wiring, start channels, order and power before running a show. Test patterns are generated on the FPP and sent through the active Channel Outputs (including E1.31, DDP and pixel strings). Any test that is on overrides sequence data — turn testing off when finished.</p>

<h4>Mode and controls (top)</h4>
<ul>
    <li><b>Test Mode</b> (dropdown) — Which pattern to generate. The page remembers the per-mode settings as you switch, so you can hop between modes without losing your color or range choices.</li>
    <li><b>Start Channel</b> (number, 1-based) and <b>Channel Count</b> (number) — Which contiguous block of FPP channels to light. The preview span shows the calculated end channel. Use to isolate one prop or controller.</li>
    <li><b>Enable</b> (checkbox) — Master switch. When checked the chosen pattern is emitted continuously; when unchecked all test output stops. The page header also shows a live status (“Testing: On” vs “Testing: Off”).</li>
    <li><b>Repeat</b> (checkbox or per-pattern control where shown) — Whether the pattern loops or plays once and stops, depending on the mode.</li>
</ul>

<h4>Chase Patterns</h4>
<ul>
    <li><b>Colors</b> (color pickers, red default <code>#ff0000</code>) — One or more colors the chase cycles through. Each picker has a native color chooser. The chase steps through the list in order.</li>
    <li><b>Speed</b> (slider or number, low is slow, high is fast) — How fast the chase moves across the channel range.</li>
    <li><b>Direction</b> (Forward / Reverse) — Whether the chase runs from Start Channel upward or downward.</li>
    <li><b>Width</b> (number, pixels) — How many consecutive channels/pixels are lit at once (the “tail” length).</li>
    <li><b>How many channels one pixel occupies</b> (number, tooltip: “How many channels one pixel occupies. The hardware colour order (GRB, BGR, …) is set on the output, not here.”) — Default 3 (RGB). Set to 1 for single-channel props, 4 for RGBW, etc. This determines how the channel range is sliced into pixels.</li>
</ul>

<h4>Cycle Patterns</h4>
<ul>
    <li>Similar to Chase but the whole range cycles through the color list together rather than a moving chase. Same <b>Colors</b> pickers, <b>Speed</b>, and <b>Pixel Size</b> controls. The pattern fades or steps through colors depending on the chosen cycle type.</li>
</ul>

<h4>Fill Color</h4>
<ul>
    <li><b>Fill Color</b> (single color picker plus intensity 0–255) — Sets every channel in the Start→End block to the same value, useful for checking that every pixel can show the same color and for measuring power draw. The channel-per-pixel note above also applies here.</li>
</ul>

<h4>Channel Fader</h4>
<ul>
    <li><b>Channel Fader</b> (horizontal slider per channel range) — Drag to set an exact value 0–255 across the selected channels in real time. Useful for testing dimmer curves or a single channel without a full chase.</li>
</ul>

<h4>DMX / Fixture testing</h4>
<ul>
    <li>When a fixture is selected, the fixture name is shown as a tooltip on the label (via the <code>dmxFixtureName</code> handler). The same chase/cycle/fill controls apply, but channel numbers follow the fixture’s footprint.</li>
</ul>

<h4>Tips</h4>
<ul>
    <li>Limit <b>Channel Count</b> to one prop at first — if that prop looks correct, expand to the next.</li>
    <li>Set <b>How many channels one pixel occupies</b> to match your prop before judging color order; wrong slicing makes a correct color order look wrong.</li>
    <li>Remember to uncheck <b>Enable</b> when done — as long as testing is on, it wins over any scheduled playlist.</li>
</ul>
