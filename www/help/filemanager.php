<h3>File Manager</h3>
<p>File Manager is where you see, upload, preview and organize everything FPP plays or stores. Files are grouped into tabs across the top. Click a tab to show that kind of file. Tables can be sorted by clicking column headers and filtered when the <i>Enable Filter</i> option is on in <code>Settings → UI</code>. Counts and total size for the current tab appear at the top right of each section.</p>

<h4>Uploading</h4>
<ul>
    <li><b>Drag and drop</b> — Drop files anywhere on the File Manager page or onto the dedicated Uploads area. FilePond handles the transfer and shows progress.</li>
    <li><b>Browse</b> — Click the upload area to pick files from your computer.</li>
    <li><b>Where files go</b> — Sequences, audio, video, images, effects and scripts are placed in their matching tab based on extension. Unknown types land in <b>Uploads</b> for you to move.</li>
    <li><b>Storage full warning</b> — A red banner appears at the top of the page if the media device is over 95% full. Free space or remove old files before adding more.</li>
</ul>

<h4>Selecting files</h4>
<ul>
    <li><b>Click</b> a row to select one file.</li>
    <li><b>Ctrl+Click</b> (Cmd on Mac) selects multiple files.</li>
    <li><b>Shift+Click</b> selects a continuous range between two rows.</li>
    <li><b>Clear</b> deselects everything. Most action buttons are disabled until a selection exists; single-item actions like Play/View enable only when one file is selected.</li>
</ul>

<h4>Tabs and their controls</h4>

<h5>Sequences — .fseq files</h5>
<p>Lighting data exported from xLights or Vixen. Columns: File, Size, FPS, Date Modified.</p>
<ul>
    <li><b>Play</b> (Player mode, single file) — Starts the sequence immediately on this Player (and synced Remotes).</li>
    <li><b>Play Here</b> (Player mode, single file) — Plays the sequence locally for testing without broadcasting MultiSync to Remotes.</li>
    <li><b>Sequence Info</b> (single) — Shows header details such as channel count, frame count and timing.</li>
    <li><b>Add To Playlist</b> — Adds the selected sequence(s) to a playlist you pick.</li>
    <li><b>Download</b> — Saves copies to your computer. Works on multiple selections.</li>
    <li><b>Rename</b> (single) — Changes the filename on the device.</li>
    <li><b>Delete</b> — Permanently removes the selected files.</li>
</ul>

<h5>Audio — .mp3 / .ogg / .m4a / .flac / .aac / .wav / .m4p</h5>
<p>Columns: File, Duration, Date Modified.</p>
<ul>
    <li><b>Listen</b> (single) — Plays the audio in the browser for quick preview.</li>
    <li><b>MP3Gain</b> (when <code>mp3gain</code> is installed, single or multiple) — Normalizes volume across tracks.</li>
    <li><b>Add To Playlist, Download, Rename, Delete</b> — As above, with the same single/multi rules.</li>
</ul>

<h5>Video — .mp4 / .mkv / .avi / .mpg / .mov</h5>
<p>Columns: File, Duration, Date Modified.</p>
<ul>
    <li><b>View</b> (single) — Plays the video in the browser.</li>
    <li><b>Video Info</b> (single) — Shows codec, resolution and duration.</li>
    <li><b>Add To Playlist, Download, Rename, Delete</b> — As above.</li>
</ul>

<h5>Images</h5>
<p>Columns: File, Size, Date Modified, <b>Thumbnail</b> (size set in <code>Settings → UI → File Manager Thumbnail Size</code>, 0=Disabled, up to 200 px).</p>
<ul>
    <li><b>View</b> (single) — Opens the image in the browser.</li>
    <li><b>Download, Rename, Delete</b> — As above.</li>
</ul>

<h5>Effects — .eseq</h5>
<p>Columns: File, Duration, Date Modified.</p>
<ul>
    <li><b>Sequence Info</b> (single) — Header details for the effect sequence.</li>
    <li><b>Download, Rename, Delete</b> — As above.</li>
</ul>

<h5>Scripts — .sh / .pl / .pm / .php / .py</h5>
<p>Columns: File, Size, Date Modified. Scripts extend FPP with custom commands, listeners or setup tasks.</p>
<ul>
    <li><b>View</b> (single) — Displays the script text in a viewer.</li>
    <li><b>Run</b> (single) — Executes the script immediately. Use with care.</li>
    <li><b>Edit</b> (single) — Opens the script in the editor. Changes are saved on the device.</li>
    <li><b>Add To Playlist</b> — Inserts a <i>Run Script</i> entry for the selected scripts.</li>
    <li><b>Download, Copy</b> (single, creates “filename copy”), <b>Rename, Delete</b> — Standard file actions.</li>
</ul>

<h5>Logs</h5>
<p>Columns: File, Size, Date Modified.</p>
<ul>
    <li><b>Zip</b> — Bundles selected logs into a single zip for download.</li>
    <li><b>View</b> (single) — Opens the log text.</li>
    <li><b>Tail</b> (single) — Shows the end of the log file.</li>
    <li><b>Tail Follow</b> (single) — Live-updating tail, like watching the log while FPP runs.</li>
    <li><b>Download, Delete</b> — As above.</li>
</ul>

<h5>Uploads</h5>
<p>Holding area for files whose type was not recognized or that were uploaded before sorting. Columns: File, Size, Date Modified.</p>
<ul>
    <li><b>Download, Copy</b> (single), <b>Rename, Delete</b> — Move or clean up. Copy places a duplicate in the same folder; rename is how you correct a file that landed here by mistake.</li>
</ul>

<h5>Crash Reports</h5>
<p>Generated when <code>fppd</code> crashes, if crash reporting is enabled in <code>Settings → Privacy</code>.</p>
<ul>
    <li><b>Download</b> — Save reports to your computer for sharing.</li>
    <li><b>Upload and Delete</b> — Sends the selected reports to the FPP developers, then removes them from this player. Use this when you want to help diagnose a crash.</li>
    <li><b>Delete</b> — Discards reports without sending.</li>
</ul>

<h5>Backups</h5>
<p>JSON backups created from <code>Backup &amp; Restore</code> (also stored in <code>/config</code> on the media device). Columns: File, Size, Date Modified.</p>
<ul>
    <li><b>Download, Delete</b> — Save copies to your computer or remove old backups. Restore is done from <code>Status/Control → FPP Backup</code>, not here.</li>
</ul>

<h5>Config (Developer visibility only)</h5>
<p>Shown only when <code>Settings → UI → UI Level</code> is Developer. Raw config files with <b>maxdepth=1</b> listing. Use with caution — editing directly can break FPP.</p>

<h5>Plugin tabs</h5>
<p>Some plugins add their own tabs (declared via <code>pluginInfo.json → fileExtensions → tab</code>). Each plugin-added tab appears between Scripts and Logs and provides its own table and controls defined by that plugin. The folder for each is created automatically if it does not already exist in the media directory.</p>

<h4>Tips</h4>
<ul>
    <li>If a file you just uploaded does not appear where you expect, look in <b>Uploads</b> and check its extension.</li>
    <li>Keep file names short and without special characters so playlists and sequences can find them after a rename.</li>
    <li>Select a file and use <b>Sequence Info</b> / <b>Video Info</b> to verify timing and channel counts match what your playlist expects.</li>
</ul>
