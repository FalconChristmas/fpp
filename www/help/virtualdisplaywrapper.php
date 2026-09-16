<h3>Virtual Display</h3>
<p>The Virtual Display renders a preview of your show in the browser without lighting real props. It is fed by the same channel data that drives real outputs — including E1.31/DDP, pixel strings and Pixel Overlay Models — but drawn on a canvas. Use it to verify sequencing, channel mapping and effects before going to the yard. Up to three modes are registered depending on installed plugins: this <b>Virtual Display</b> wrapper, <b>2D Virtual Display</b> and <b>3D Virtual Display</b> (shown in the Status/Control menu only when <code>co-other.json</code> contains an enabled <code>HTTPVirtualDisplay</code> or <code>HTTPVirtualDisplay3D</code> output).</p>

<h4>Page header</h4>
<ul>
    <li><b>Virtual Display</b> title — Indicates which wrapper you are in (the inner canvas is the same component in all wrappers; the URL determines which set of outputs are visualized). The 2D and 3D wrappers are separate entries that appear only when the corresponding virtual output is enabled.</li>
    <li><b>Canvas / Stage</b> — The drawable area. Its size and aspect are derived from the output’s configured width/height (e.g. panel matrix dimensions or virtual string layout). The canvas is rendered via HTML5 canvas / WebGL depending on the output type.</li>
</ul>

<h4>Controls and overlays</h4>
<ul>
    <li><b>Source selection</b> — The display follows the live channel data. When a playlist or Display Testing pattern is active, the preview updates in real time. When nothing is playing it shows the current idle channel state.</li>
    <li><b>Model highlighting</b> — Pixel Overlay Models and channel output groups are shown in their configured positions and colors. Hovering a model (where supported) shows its name and channel range.</li>
    <li><b>Zoom and pan</b> (3D variant) — Drag to orbit, scroll to zoom, right-drag to pan. The 2D variant is a fixed top-down view with no camera controls.</li>
</ul>

<h4>Tips</h4>
<ul>
    <li>If the preview stays blank, check that the virtual output is <b>Enabled</b> in <b>Input/Output Setup → Channel Outputs</b> and that its channel range matches the sequences you are playing.</li>
    <li>The virtual display adds no load to real pixels — you can keep it open on a laptop while testing on the live rig, but closing it saves a small amount of browser CPU.</li>
    <li>Use it together with <b>Display Testing</b> to isolate a channel range and see exactly how the preview maps that range onto the virtual layout.</li>
</ul>
