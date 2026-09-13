<h3>3D Virtual Display</h3>
<p>The 3D Virtual Display renders your show in true 3D space in the browser — including height (Z) — using the pixel map uploaded from xLights via FPP Connect (saved as <code>virtualdisplaymap</code> in FPP’s config directory). It is fed by the same channel data as your real outputs, so you can verify sequencing and model placement without lighting props. This 3D wrapper appears in the <b>Status/Control</b> menu only when an enabled <code>HTTPVirtualDisplay3D</code> output exists on the <b>Other</b> tab of <b>Input/Output → Channel Outputs</b>.</p>

<h4>How the page is built</h4>
<ul>
    <li><b>Canvas / Stage</b> — The drawable area. By default <code>?width</code> yields a 4:3 canvas (<code>width × 0.75</code>); in standalone the canvas fills the window (1920×1080 overridden to <code>window.innerWidth/innerHeight</code> via JavaScript). The page prevents caching (<code>no-cache, no-store, must-revalidate</code>) to avoid SSE connection issues.</li>
    <li><b>Three.js</b> — The import map (<code>three → /js/logic/three/three.module.js</code>, <code>three/addons/ → /js/logic/three/</code>) must be first, then modules <code>THREE + OBJLoader + MTLLoader + OrbitControls</code> are imported and exposed as <code>window.THREE</code>/<code>OBJLoader</code>/<code>MTLLoader</code>/<code>OrbitControls</code> with event <code>threejs-loaded</code> fired.</li>
    <li><b>Body</b> — Loads <code>virtualdisplaybody3d.php</code> (the WebGL scene). In normal mode it is wrapped in <code>#bodyWrapper + menu.inc</code> with title <b>3D Virtual Display</b>; in standalone (<code>?standalone=true</code>) only the canvas is rendered.</li>
</ul>

<h4>Mouse and keyboard controls</h4>
<ul>
    <li><b>Left-drag</b> — Orbit / rotate the view around the target. Implemented via <code>OrbitControls</code>.</li>
    <li><b>Middle-drag (scroll-wheel press + drag)</b> — Pan / move the view laterally.</li>
    <li><b>Scroll wheel</b> — Zoom in/out (changes distance to target, also reflected by the <code>zoom</code> URL param).</li>
    <li><b>Fullscreen button</b> — Enters immersive full-screen (browser fullscreen API); press <b>ESC</b> to exit. Can be auto-entered with <code>?fullscreen=true</code> (requires a user click if the browser blocks it).</li>
    <li><b>📋 Copy View URL</b> — Saves the current camera position, angles, zoom, FOV, brightness, etc. as a shareable URL (builds the same query string documented below).</li>
</ul>

<h4>What you see</h4>
<ul>
    <li><b>Pixels in 3D space</b> — Each pixel is drawn at its X/Y/Z from the virtualdisplaymap, so trees and matrices appear with true height. The grid and axes helpers are on by default; toggle via URL (see below).</li>
    <li><b>Pixel rendering</b> — Point size defaults to <code>1</code> (<code>pixelSize</code>), brightness multiplier defaults to <code>2.0</code> (<code>brightness</code>), ambient light defaults to <code>1.0</code> (<code>ambientLight</code>, 0.0 = dark).</li>
</ul>

<h4>URL Parameters — customize without code</h4>
<p>Add any of these to the address bar; combine with <code>&amp;</code>. All are optional.</p>
<ul>
    <li><b>⭐ Standalone Mode:</b> <code>?standalone=true</code> — Full-window visualizer with <b>no UI elements</b> (perfect for a dedicated TV). The page injects full-viewport CSS hiding all controls/buttons/headers and a mock <code>$</code> to avoid errors; the canvas resizes with the window.</li>
    <li><b>Camera Position:</b> <code>?cameraX=500&amp;cameraY=300&amp;cameraZ=800</code> — Exact camera coordinates.</li>
    <li><b>Camera Angles:</b> <code>?cameraAngleH=45&amp;cameraAngleV=20</code> — Horizontal/vertical angles in degrees.</li>
    <li><b>Camera Target:</b> <code>?targetX=0&amp;targetY=100&amp;targetZ=0</code> — Look-at point.</li>
    <li><b>Zoom Level:</b> <code>?zoom=0.5</code> — Multiplier (0.5 closer, 2.0 farther).</li>
    <li><b>Field of View:</b> <code>?fov=90</code> — Camera FOV in degrees (default 75).</li>
    <li><b>Fullscreen:</b> <code>?fullscreen=true</code> — Auto-enter fullscreen.</li>
    <li><b>Brightness:</b> <code>?brightness=3.0</code> — Pixel brightness multiplier (default 2.0).</li>
    <li><b>Ambient Light:</b> <code>?ambientLight=0.5</code> — Scene lighting intensity (default 1.0, 0.0 dark).</li>
    <li><b>Pixel Size:</b> <code>?pixelSize=5</code> — Point size (default 1).</li>
    <li><b>Show Grid:</b> <code>?showGrid=false</code> — Hide grid helper.</li>
    <li><b>Show Axes:</b> <code>?showAxes=true</code> — Show axis helper.</li>
    <li><b>Holiday Mode:</b> <code>?holidayMode=true</code> — Enable holiday animations.</li>
    <li><b>Combine multiple:</b> <code>?standalone=true&amp;zoom=0.8&amp;brightness=2.5&amp;ambientLight=0.3&amp;holidayMode=true</code></li>
</ul>

<h4>Setup prerequisites — retained important notes</h4>
<ul>
    <li><b>If the 3D display is not working</b>, add the <b>HTTP Virtual Display</b> Channel Output on the <b>Other</b> tab of <b>Channel Outputs</b>. Upload the pixel map from xLights via FPP Connect by ticking <b>Models</b> for this device — it is saved as <code>virtualdisplaymap</code> in FPP’s configuration directory. <i>Retained verbatim in meaning.</i></li>
    <li><b>Tilted trees warning:</b> If tree models appear tilted or off-vertical, recreate those trees in the <b>current version of xLights</b>. Older xLights generated tree data with alignment inaccuracies; only a fresh generation and export will correct the geometry FPP receives. <i>Retained.</i></li>
</ul>

<h4>Upload 3D Object Files</h4>
<ul>
    <li><b>Section:</b> <i>Upload 3D Object Files</i> — Until xLights supports automatic upload of 3D objects, use this uploader. Drag &amp; Drop or <b>Select Files</b>. Supported extensions: <code>.obj, .mtl, .png, .jpg, .jpeg</code> (including <code>model/obj, model/mtl</code>). Files go to <code>/home/fpp/media/virtualdisplay_assets</code>.</li>
    <li><b>FilePond configuration</b> — Label “Drag &amp; Drop 3D Asset Files or Click to Select”, server <code>api/file/virtualdisplay_assets</code> (<code>POST</code>), chunked uploads enabled (<code>64 MB</code> chunks, <code>chunkForce</code> true, 3 parallel), type detection by extension fallback, success → <code>$.jGrowl('File uploaded successfully: …')</code>, error → toast with <code>error.main</code>.</li>
</ul>

<h4>Tips</h4>
<ul>
    <li>Use <b>📋 Copy View URL</b> after framing the show once, then bookmark the URL with your chosen <code>brightness/ambientLight/pixelSize</code> so every browser opens the same view.</li>
    <li>Keep <b>standalone</b> URLs for lobby TVs: pair with <code>?showGrid=false&amp;brightness=2.5</code> for a clean, bright wall display.</li>
    <li>If assets fail to appear, verify the uploaded <code>.obj</code> references a matching <code>.mtl</code> + textures by identical base name in the same assets folder.</li>
</ul>
