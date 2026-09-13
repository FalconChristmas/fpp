<h3>Channel Inputs</h3>
<p>Channel Inputs let another device drive your lights through FPP. Instead of playing a sequence stored on this FPP, the channel data arrives over the network (E1.31, ArtNet or DDP) or a serial port (DMX) and is mapped onto FPP’s channel range. This is how FPP works as a bridge or live receiver when xLights or another show computer is playing.</p>

<h4>Tabs</h4>
<ul>
    <li><b>E1.31 / ArtNet / DDP Inputs</b> — Network inputs that listen for E1.31 (sACN), ArtNet or DDP on the local network.</li>
    <li><b>DMX</b> — Serial DMX inputs that read DMX from a USB serial adapter. The tab is hidden when no serial devices (ttyACM, ttyUSB, ttyAMA, ttyO, ttySC) are detected.</li>
</ul>

<h4>E1.31 / ArtNet / DDP Inputs — Top controls</h4>
<ul>
    <li><b>Enable Input</b> (checkbox) — Master switch. When unchecked, all network inputs are ignored even if rows are configured.</li>
    <li><b>Timeout</b> (number, 10–9999 ms, default 1000) — How long to keep the last received data before clearing it if no new packets arrive. Help icon explains: if no new data arrives within this time, the input data is cleared to black. A shorter timeout stops stale looks faster; a longer timeout rides through brief network hiccups.</li>
    <li><b>Inputs Count</b> (text, default “Enter Universe Count”, size 3, max 3) — Type how many input universes you want and click <b>Set</b> to add or remove rows in the table to reach that count.</li>
    <li><b>Delete</b> (red outline) — Removes the currently selected table rows (click a row to select it; selected rows get the <code>selectedEntry</code> class). Not saved until you press <b>Save</b>.</li>
    <li><b>Clone</b> — Duplicates the selected rows, appending the copies. Use to quickly create multiple similar inputs.</li>
    <li><b>Save</b> (green, submit) — Validates the table and posts the JSON to the channel-inputs API. The form uses <code>frmUniverses</code> submit handling with <code>validateUniverseData()</code> before posting.</li>
</ul>

<h4>E1.31 / ArtNet / DDP Inputs — Table columns</h4>
<p>Rows are drag-reorderable (grip handle on touch, whole-row drag otherwise) via <code>#tblUniversesBody</code> sortable.</p>
<ul>
    <li><b>Grip</b> — Drag handle to reposition the row. Text note below the table reminds you: “Drag entry to reposition”.</li>
    <li><b>Input</b> — Row number / order.</li>
    <li><b>Active</b> (checkbox) — Per-row enable. Inactive rows are skipped even when the master Enable is on.</li>
    <li><b>Description</b> (text) — Free-form label for your own reference, e.g. “House front”.</li>
    <li><b>Input Type</b> (dropdown: E1.31, ArtNet, DDP) — Which protocol this row listens for. E1.31 uses Universe numbers; ArtNet uses Net/Subnet/Universe; DDP is channel-based and has no universe concept. The exact field that appears in the next columns adapts to the chosen type.</li>
    <li><b>FPP Channel Start</b> (number) — First FPP channel this input drives (1-based). Together with Universe Count × Size it determines how many channels are mapped.</li>
    <li><b>FPP Channel End</b> (read-only calculated) — Last channel covered, updated live as you edit Start/Count/Size.</li>
    <li><b>Universe #</b> (number, 1-based) — For E1.31/ArtNet, the first universe number to listen for. DDP rows ignore this concept.</li>
    <li><b>Universe Count</b> (number) — How many consecutive universes this row covers, starting at Universe #.</li>
    <li><b>Universe Size</b> (number, typically 512 for E1.31) — Channels per universe. 512 is standard; some ArtNet setups use smaller sizes.</li>
</ul>

<h4>DMX tab — Table columns</h4>
<p>One row per detected serial device, built at page load by scanning <code>/dev/tty*</code> and any cape-reported <code>tty-labels</code>. Each row shows:</p>
<ul>
    <li><b>Enable</b> (checkbox) — Whether DMX is read from this serial port.</li>
    <li><b>Serial Port</b> (device name) — e.g. <code>ttyUSB0</code>, <code>ttyACM0</code>, or a cape label.</li>
    <li><b>Start Channel</b> (number, 1–8388608, default 1) — First FPP channel driven by DMX starting at this port.</li>
    <li><b>Num Channels</b> (number, 1–512, default 512) — How many DMX channels to map from this port.</li>
    <li><b>Save</b> (green, top right) — Posts <code>{channelInputs: [{device, enabled, startAddress, channelCount, type:"dmx"}]}</code> to <code>api/channel/output/dmxInputs</code>. Data is loaded on entry via <code>GET</code> of the same endpoint and row fields are pre-filled.</li>
</ul>

<h4>Tips</h4>
<ul>
    <li>Start with the master <b>Enable Input</b> checked, add one universe row, set its FPP Channel Start to 1 and Universe # to 1, then verify with the incoming data indicator on the Status page or with Display Testing off.</li>
    <li>Use <b>Timeout</b> = 1000 ms for most shows. Shorten only if you need stale looks to clear faster when the sender stops.</li>
    <li>On the DMX tab, if no serial devices appear, check the USB adapter is plugged in and recognized before reloading the page.</li>
</ul>
