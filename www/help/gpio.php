<h3>GPIO Input Triggers</h3>
<p>GPIO Inputs let a physical button, switch or sensor trigger FPP actions without using the web interface. Each trigger watches one GPIO pin on the Pi’s header or on an attached I/O board. When the electrical level changes, the trigger can run any FPP Commands you assign to its rising, falling or hold edges. Pins that are already reserved by a cape or by pixel-string outputs are removed from the pin list so they cannot be double-used.</p>

<h4>Page layout</h4>
<ul>
    <li><b>Add GPIO Trigger</b> (top right, plus icon) — Opens the trigger editor for a new pin.</li>
    <li><b>Save</b> (green, top right) — Writes the whole trigger list to <code>config/GPIO.json</code> via <code>saveGPIOInputs()</code>. A growl confirms when saved.</li>
    <li><b>Trigger cards</b> — Each configured trigger appears as a card showing its pin, description and enable state. Cards are empty until you add one: “No GPIO triggers configured. Click Add GPIO Trigger to get started.” Each card has <b>Edit</b> (pencil) and <b>Delete</b> (trash) buttons, plus up/down arrows to reorder. The order does not affect firing — it is only for your organization.</li>
</ul>

<h4>Trigger editor — General</h4>
<ul>
    <li><b>Enabled</b> (checkbox, checked by default) — When unchecked the trigger stays in the config but never fires. Good for temporarily disabling a button without deleting its commands.</li>
    <li><b>GPIO Pin</b> (dropdown) — The input pin to watch. The list is built server-side from lines reported by <code>gpiod</code>, annotated with the gpiochip/line it maps to on the Pi P1 header when stable pin numbers are available. Only pins not reserved by capes or channel outputs are shown.</li>
    <li><b>Pull Up / Down</b> (dropdown: <b>None</b>, <b>Pull Up</b>, <b>Pull Down</b>)
        <ul>
            <li><b>None</b> — No internal resistor; you must provide an external pull resistor or the pin will float.</li>
            <li><b>Pull Up</b> (most common) — Pin reads HIGH at rest; wiring a normally-open button to GND pulls it LOW when pressed — that press is a <b>falling</b> edge, release is <b>rising</b>.</li>
            <li><b>Pull Down</b> — Pin reads LOW at rest; wiring a button to 3.3 V pulls HIGH when pressed — press is <b>rising</b>, release is <b>falling</b>.</li>
        </ul>
    </li>
    <li><b>Description</b> (text, max 128, placeholder “e.g. Start button”) — Optional label shown on the card to identify the trigger by function.</li>
</ul>

<h4>Commands to execute when the input fires</h4>
<p>Each edge has its own command list built with the same FPP Command picker used in Command Presets and the Scheduler. Click <b>Add Command</b>, pick a command and edit its arguments; the preview shows the chosen args.</p>
<ul>
    <li><b>Rising (button pressed / signal HIGH)</b> — Commands run on LOW → HIGH. With a pull-up + normally-open button this is <em>release</em>; with pull-down it is <em>press</em>. Help text: “Commands run when the GPIO signal transitions LOW → HIGH.”</li>
    <li><b>Falling (button released / signal LOW)</b> — Commands run on HIGH → LOW. With a pull-up + normally-open button this is <em>press</em>; with pull-down it is <em>release</em>. Note: if a Hold action fired during the press, the falling-edge commands for that press are suppressed to avoid double-actions.</li>
    <li><b>Hold (optional)</b> — Commands that fire only when the button stays held for <b>Hold Time (ms)</b> (number, 0 disables). Examples: long-press to trigger a different playlist. When Hold Time is 0 the hold section is disabled. Hold also suppresses the falling-edge actions for that same press.</li>
</ul>

<h4>Debounce, re-trigger and button LED</h4>
<ul>
    <li><b>Debounce Time (ms)</b> (number, min 10, default 50–200 for push buttons) — Mechanical switches bounce briefly between open/closed. FPP ignores transitions shorter than this window to prevent false triggers.</li>
    <li><b>Debounce Edge</b> (dropdown) — Which edges the debounce window applies to (both / rising / falling). Lets you debouce only the press while leaving release snappy, or vice versa.</li>
    <li><b>Re-enable GPIO After Trigger</b> — Controls whether the same trigger can fire again soon after. Settings are <b>Mode</b> (immediate, after delay, etc.) and <b>Re-enable Delay (ms)</b> (min 100). While suppressed, additional presses are ignored.</li>
    <li><b>LED Output Pin</b> (dropdown, optional) — GPIO output wired to the button’s built-in LED. The pin is driven as a digital output — ensure it is not used for pixels or cape outputs. Help text reminds you to pick an otherwise-unused pin.</li>
    <li><b>LED Logic</b> (dropdown: Active-high / Active-low) — <b>Active-high</b> writes HIGH to light the LED (direct GPIO→LED→GND). <b>Active-low</b> writes LOW (for transistor/open-collector drivers).</li>
    <li><b>LED Idle Mode</b> (dropdown) — What the LED does when idle (off, on, blink, etc.).</li>
    <li><b>Trigger Mode</b> (dropdown: None / Follow input / Flash N times / Stay on for N ms) — What the LED does on the rising edge. <b>Flash N times</b> flashes with 150 ms on / 150 ms off per flash; <b>Stay on for N ms</b> lights for the configured duration then returns to idle. The numeric <b>Trigger Param</b> field’s meaning follows the mode (number of flashes vs milliseconds).</li>
</ul>

<h4>Event types — when each list fires</h4>
<ul>
    <li><b>Rising</b> events fire when the pin goes HIGH.</li>
    <li><b>Falling</b> events fire when the pin goes LOW.</li>
</ul>

<h4>Connection types — how to wire a switch</h4>
<ul>
    <li><b>Normally Open (NO)</b> — Closes only when activated (most push buttons). With the pin pulled high, the pin sits HIGH at rest and is pulled LOW when you press. Configure the <b>Falling</b> command to run on press; optionally configure <b>Rising</b> for release. Below is the sample circuit FPP expects:</li>
</ul>
<center><b>Sample GPIO Normally Open Button Configuration</b><br>
<img src='help/GPIO.png' alt='GPIO normally open button wiring'></center>
<ul>
    <li><b>Normally Closed (NC)</b> — Closed at rest, open when activated (e.g. motion sensors). Use <b>Rising</b> to run when the sensor triggers and <b>Falling</b> for when it resets.</li>
</ul>

<h4>Tips</h4>
<ul>
    <li>Test with the <b>Enabled</b> toggle off: save, watch that nothing fires, then re-enable.</li>
    <li>If a press fires twice, increase <b>Debounce Time</b> to 100–200 ms before adjusting wiring.</li>
    <li>Keep trigger <b>Descriptions</b> short but meaningful — “Porch button” is easier than “GPIO17” when you have several.</li>
</ul>
