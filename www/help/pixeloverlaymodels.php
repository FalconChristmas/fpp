<h3>Pixel Overlay Models</h3>
<p>The <b>Real-Time Pixel Overlay</b> feature lets you manipulate channel data in real time before it is sent out to controllers. This can be done while the rest of your display and channels are playing sequenced data from a <code>.fseq</code> sequence file. The Pixel Overlay Models can be turned on and off independently of each other and may be used when sequence data is playing or not playing. When a Pixel Overlay Model is enabled, the data input via the Pixel Overlay feature overrides channel data in the sequence file, allowing you to take control of a portion of your display in real time. This can be used for examples such as displaying real-time dynamic text on a matrix or pixel tree, displaying the current time or a Christmas countdown timer on a matrix, or turning on/off individual channels for items such as a Tune-To sign or inflatables without having to sequence the channels for these items in every one of your sequence files.</p>

<p>The <b>Matrix Tools</b> plugin available via the Plugin install page uses the Pixel Overlay feature to allow display and scrolling of dynamic text on a Pixel Overlay Model using a web interface. As an example of the power and flexibility of the Real-Time Pixel Overlay feature, you can also use the Matrix Tools plugin to draw in real-time on your matrix using your mouse and web browser. Future support includes displaying live video being captured from a webcam attached to the Falcon Player.</p>

<h4>Page controls</h4>
<ul>
    <li><b>Add</b> (plus, top right) — Opens <b>Add Pixel Overlay Model</b> dialog with three choices: <b>Channel Model</b> (channel range, see below), <b>Frame Buffer Model</b> (FB, for video/image overlays), and <b>Sub-Model</b> (derived from an xLights model). The dialog uses the same form as the table but creates a new row.</li>
    <li><b>Delete xLights Generated</b> — Removes models that were auto-imported from xLights. Use when you have re-exported models under new names.</li>
    <li><b>Save</b> (green) — Posts the whole table to <code>api/configfile/model-overlays.json</code> via <code>SetChannelMemMaps()</code>. A growl confirms when saved.</li>
    <li><b>Create Overlays Automatically From Outputs</b> (checkbox, below table) — When checked, FPP creates overlay models automatically from your Channel Outputs. Uncheck if you want only manually defined models.</li>
</ul>

<h4>Table columns (each row)</h4>
<ul>
    <li><b>Model Name</b> — Unique name shown in the UI and used by effects. Dropdown is populated from <code>PixelOverlayModels</code> (channel outputs) and from xLights imports.</li>
    <li><b>Start Ch.</b> — First FPP channel of the block.</li>
    <li><b>Channel Count / Width</b> — How many channels (or pixel width for FB models) the model covers.</li>
    <li><b>Type</b> — Shown as <b>Model</b>, <b>Submodel</b> or <b>Model Group</b> (when Name matches an xLights model group). Hidden <code>type</code> field drives this label.</li>
    <li><b>Parent</b> (for Sub-Model) — Which parent Model this sub-model derives its pixels from. Select via dropdown of existing models.</li>
    <li><b>Orientation / Start Corner / Strings / Strands</b> — Geometry of the matrix/tree. Orientation is the direction strings run; Start Corner is where pixel 1 sits; Strings is how many strings; Strands is strands per string. These determine how channel numbers map to physical pixels.</li>
    <li><b>Preview</b> (eye icon, blue) — Click to preview the model layout. Loads pixel grid via <code>api/overlays/model/{name}</code> and renders a preview dialog showing which channels light.</li>
    <li><b>Submodel indicator</b> (sitemap icon, purple, with count) — Shows how many submodels this parent has (from <code>xlightsSubModels</code> derived via <code>docs/PixelOverlaySubModels.md</code>). Click to expand an inline detail table of submodels with Name, Size, Overlay Model Name, Running Effect and Preview.</li>
    <li><b>Actions</b> — Move up/down arrows (disable at ends), Edit (pencil) and Delete (trash) per row. Reordering updates channel mapping display.</li>
</ul>

<h4>Model Groups (from xLights)</h4>
<ul>
    <li><b>Model Groups</b> — When xLights exports model groups, FPP shows them as grouped counts at the top and badges them as <code>Model Group</code> in the table. Version 4 playlists can show section counts per group.</li>
</ul>

<h4>Tips</h4>
<ul>
    <li>Keep overlay channel ranges from overlapping your main Channel Outputs unless you intend the overlay to take priority — enabled overlays override sequence data on those channels.</li>
    <li>Use <b>AutoCreate</b> only until you need custom geometry, then turn it off and define models manually to avoid duplicates.</li>
    <li>Test with the Matrix Tools plugin or Display Testing to confirm the model lights the right pixels in the right order before updating your show.</li>
</ul>
