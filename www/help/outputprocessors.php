<h3>Output Processors</h3>
<p>Output Processors let you adjust channel data <b>just before it is sent</b> to controllers, without editing your sequences. Each processor is one row in the table and is applied top-to-bottom in the order shown. Add a row, pick its <b>Type</b>, fill in the channel range and type-specific fields, give it a <b>Description</b> for your own notes, and press <b>Save</b>. Drag rows by the grip to re-order; selected rows are highlighted and enable <b>Delete</b>.</p>

<h4>Top controls and table columns</h4>
<ul>
    <li><b>Add</b> (green plus) — Appends a blank row. A new row starts as <b>Active</b> checked, Type = Channel Remaps, and an empty Description.</li>
    <li><b>Delete</b> (red outline, disabled until one or more rows are selected by clicking them) — Removes the selected rows. You must still Save.</li>
    <li><b>Save</b> (green) — Collects every row, builds <code>{outputProcessors: [...]}</code> and saves to the processors JSON via the API. Failures show “Save Failed”.</li>
    <li><b>Active</b> (checkbox) — When unchecked the processor stays in the config but is skipped.</li>
    <li><b>#</b> — Row number, updated automatically after reorder or delete.</li>
    <li><b>Type</b> (dropdown) — Which processing to apply (see below). Changing Type swaps the “Output Type Details” fields inline and briefly highlights them.</li>
    <li><b>Description</b> (text, size 32, max 64) — Free-form label for the row shown in the table.</li>
    <li><b>Output Type Details</b> — Dynamic fields that change with Type (see per-type below). All channel fields accept 1-based FPP channel numbers (stored as 0-based internally).</li>
</ul>

<h4>Processor types — retained reference with details</h4>

<h5>Channel Remaps</h5>
<p>Allows output channels to be remapped onto other channels before being sent out to controllers.</p>
<ul>
    <li>Copies one or many channels to work around a bad controller port or a failed string.</li>
    <li>Copies a whole controller’s channels onto another controller without re-addressing the replacement.</li>
    <li>Duplicates data across two or more identical props (two matrices showing the same content).</li>
</ul>
<p>Fields: <b>Model</b> (dropdown, when a pixel overlay model is referenced), <b>Source</b> (first channel copied from), <b>Destination</b> (first channel copied to), <b>Count</b> (how many consecutive channels), <b>Loops</b> (how many times the block is repeated), <b>Reverse</b> (dropdown: No / Yes — reverses order within the block).</p>

<h5>Brightness</h5>
<p>Allows adjusting the brightness and gamma of a range of channels.</p>
<p>Fields: <b>Start Channel</b>, <b>Channel Count</b>, <b>Brightness</b> (number 0–100), <b>Gamma</b> (number 0.1–5.0, step 0.1). Lower Brightness dims; Gamma corrects the curve so mid-tones look natural.</p>

<h5>Hold Value</h5>
<p>Holds the channel value at the last non-zero value when a zero is received. Prevents a prop from snapping to black during a brief zero in the data. Fields: <b>Start Channel</b>, <b>Channel Count</b>.</p>

<h5>Set Value</h5>
<p>Sets the exact value on a range of channels. Useful if some channels are not working properly and need to always be set off (dead pixel forced to 0) or on (tune-to sign always on).</p>
<p>Fields: <b>Start Channel</b>, <b>Channel Count</b>, <b>Value</b> (0–255).</p>

<h5>Override Zero</h5>
<p>Sets the exact value on a range of channels, <b>but only when the source value is zero</b>. Non-zero data passes through untouched.</p>
<p>Fields: <b>Start Channel</b>, <b>Channel Count</b>, <b>Max Value</b> (0–255) — the value written when the input is zero.</p>

<h5>Reorder Colors</h5>
<p>Change the order of colors for a range of nodes, e.g. RGB → GRB when a string was wired differently.</p>
<p>Fields: <b>Start Channel</b>, <b>Nodes</b> (how many RGB nodes), <b>Color Order</b> (dropdown: RGB, RBG, GRB, GBR, BRG, BGR).</p>

<h5>Three to Four (RGB → RGBW)</h5>
<p>Expands RGB colors to four channel RGBW. There are three algorithms:</p>
<ul>
    <li><b>No White</b> — White channel stays off.</li>
    <li><b>R=G=B</b> — White is used only when all three channels are equal (pure grays).</li>
    <li><b>Advanced</b> — Uses more complex logic to decide when white should fill in some of the color space.</li>
</ul>
<p>Fields: <b>Start Channel</b>, <b>Nodes</b>, <b>Color Order</b> (as above), <b>Algorithm</b> (dropdown: No White / R=G=B / Advanced).</p>

<h4>Additional variants shown in code</h4>
<ul>
    <li><b>Scale</b> (Start/Count/Scale Factor 0–10) and other model-aware remaps appear for certain builds — they follow the same Source/Destination/Count pattern and are documented in the row’s inline help icon.</li>
</ul>

<h4>Tips</h4>
<ul>
    <li>Keep each processor’s range narrow and its Description specific — “Porch Hold Value 101-150” is easier to debug than a generic name.</li>
    <li>Order matters: a Brightness dim followed by a Hold Value behaves differently than the reverse when zeros are involved.</li>
    <li>Test with <b>Display Testing</b> to see the processor effect live without playing a sequence.</li>
</ul>
