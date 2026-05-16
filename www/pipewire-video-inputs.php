<!DOCTYPE html>
<html lang="en">

<head>
    <?php
    include 'common/htmlMeta.inc';
    require_once "common.php";
    require_once 'config.php';
    include 'common/menuHead.inc';
    ?>

    <title><? echo $pageTitle; ?> - PipeWire Video Input Sources</title>

    <style>
        .source-card {
            border: 1px solid var(--bs-border-color, #dee2e6);
            border-radius: 8px;
            margin-bottom: 1.5rem;
            background: var(--bs-body-bg, #fff);
        }

        .source-card.disabled-source {
            opacity: 0.6;
        }

        .source-header {
            display: flex;
            align-items: center;
            gap: 0.75rem;
            padding: 0.75rem 1rem;
            border-bottom: 1px solid var(--bs-border-color, #dee2e6);
            background: var(--bs-tertiary-bg, #f8f9fa);
            border-radius: 8px 8px 0 0;
            flex-wrap: wrap;
        }

        .source-header .source-name-input {
            font-size: 1.1rem;
            font-weight: 600;
            border: 1px solid var(--bs-border-color, #ccc);
            background: var(--bs-body-bg, #fff);
            color: var(--bs-body-color, #212529);
            padding: 0.25rem 0.5rem;
            border-radius: 3px;
            min-width: 250px;
        }

        .source-header .source-name-input:focus {
            border-color: var(--bs-primary, #007cba);
            background: var(--bs-body-bg, #fff);
            color: var(--bs-body-color, #212529);
            outline: none;
            box-shadow: 0 0 3px rgba(0, 124, 186, 0.3);
        }

        .source-body {
            padding: 1rem;
        }

        .source-body .row {
            margin-bottom: 0.5rem;
        }

        .source-body label {
            font-weight: 600;
            font-size: 0.85rem;
        }

        .no-sources-msg {
            text-align: center;
            margin: 2rem 0;
        }

        .toolbar {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 1rem;
            flex-wrap: wrap;
            gap: 0.5rem;
        }

        .toolbar-left {
            display: flex;
            align-items: center;
            gap: 0.75rem;
        }

        .toolbar-right {
            display: flex;
            align-items: center;
            gap: 0.5rem;
        }

        .status-indicator {
            display: inline-block;
            width: 10px;
            height: 10px;
            border-radius: 50%;
            margin-right: 0.4rem;
        }

        .status-running {
            background: #28a745;
        }

        .status-stopped {
            background: #dc3545;
        }

        .status-unknown {
            background: #ffc107;
        }

        .pipewire-badge {
            font-size: 0.75rem;
            font-family: monospace;
        }

        .type-badge {
            font-size: 0.8rem;
            padding: 0.2rem 0.6rem;
        }

        .alsa-warning {
            text-align: center;
            margin: 2rem 0;
        }

        .info-block {
            background: var(--bs-info-bg-subtle, #cff4fc);
            border: 1px solid var(--bs-info-border-subtle, #9eeaf9);
            border-radius: 6px;
            padding: 0.75rem 1rem;
            margin-bottom: 1rem;
            font-size: 0.9rem;
        }

        .info-block i {
            color: var(--bs-info, #0dcaf0);
            margin-right: 0.5rem;
        }

        #applyOverlay {
            position: fixed;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background: rgba(0, 0, 0, 0.5);
            display: flex;
            align-items: center;
            justify-content: center;
            z-index: 9999;
        }

        #applyOverlay .apply-box {
            background: var(--bs-body-bg, #fff);
            border-radius: 8px;
            padding: 2rem;
            text-align: center;
            min-width: 300px;
        }
    </style>
</head>

<body>
    <div id="bodyWrapper">
        <?php
        $activeParentMenuItem = 'status';
        include 'menu.inc';
        ?>
        <div class="mainContainer">
            <h1 class="title">PipeWire Video Input Sources</h1>
            <div class="pageContent">

                <?php
                $mediaBackend = isset($settings['MediaBackend']) ? $settings['MediaBackend'] : 'alsa';
                $mediaBackendLabel = array(
                    'alsa' => 'Hardware Direct',
                    'pipewire-simple' => 'Simple PipeWire',
                    'pipewire' => 'PipeWire (Advanced)'
                );
                $mbDisplay = isset($mediaBackendLabel[$mediaBackend]) ? $mediaBackendLabel[$mediaBackend] : ucfirst($mediaBackend);
                if ($mediaBackend !== 'pipewire') {
                    ?>
                    <div class="alsa-warning">
                        <i class="fas fa-exclamation-triangle fa-2x" style="color: var(--bs-warning, #ffc107);"></i>
                        <h4>Advanced PipeWire Required</h4>
                        <p>Video Input Sources require the Advanced PipeWire backend.<br>
                            Currently using: <strong><?= htmlspecialchars($mbDisplay) ?></strong></p>
                        <p>Change to PipeWire (Advanced) in <a href="settings.php?tab=Audio%2FVideo">FPP Settings &rarr;
                                Audio/Video</a>,
                            then return here to configure video input sources.</p>
                    </div>
                <?php } else { ?>

                    <div class="info-block">
                        <i class="fas fa-info-circle"></i>
                        A <strong>Video Input Source</strong> is a persistent video producer that
                        appears in the PipeWire graph as a <code>Video/Source</code> node.
                        Examples include test patterns, USB cameras, RTSP streams, web URLs, and RTP feeds.
                        Video Output Groups can target these sources to display them
                        on HDMI outputs, pixel overlays, or network streams.
                    </div>

                    <div id="pipewireStatus" class="toolbar">
                        <div class="toolbar-left">
                            <span id="pwStatus"><span class="status-indicator status-unknown"></span> Checking PipeWire
                                status...</span>
                        </div>
                        <div class="toolbar-right">
                            <a class="btn btn-sm btn-outline-secondary" href="settings.php#settings-av"
                                title="Back to Pipewire Settings">
                                <i class="fas fa-arrow-left"></i> Pipewire Settings
                            </a>
                            <button class="buttons btn-outline-success" onclick="AddSource()">
                                <i class="fas fa-plus"></i> Add Source
                            </button>
                            <button class="buttons" onclick="SaveSources()">
                                <i class="fas fa-save"></i> Save
                            </button>
                            <button class="buttons btn-outline-primary" onclick="ApplySources()">
                                <i class="fas fa-sync"></i> Save &amp; Apply
                            </button>
                        </div>
                    </div>

                    <div id="sourcesContainer">
                        <div class="no-sources-msg" id="noSourcesMsg">
                            <i class="fas fa-video" style="font-size:2rem; color:var(--bs-secondary-color,#6c757d);"></i>
                            <h4>No Video Input Sources Configured</h4>
                            <p>Add a source to create a persistent video signal in the PipeWire graph
                                (test patterns, cameras, etc.).</p>
                            <button class="buttons btn-outline-success" onclick="AddSource()">
                                <i class="fas fa-plus"></i> Add First Source
                            </button>
                        </div>
                    </div>

                    <div id="bottomToolbar" class="toolbar" style="display:none; margin-top:1rem;">
                        <div class="toolbar-left"></div>
                        <div class="toolbar-right">
                            <button class="buttons" onclick="SaveSources()">
                                <i class="fas fa-save"></i> Save
                            </button>
                            <button class="buttons btn-outline-primary" onclick="ApplySources()">
                                <i class="fas fa-sync"></i> Save &amp; Apply
                            </button>
                        </div>
                    </div>

                <?php } ?>

            </div>
        </div>

        <?php include 'common/footer.inc'; ?>
    </div>

    <script>
        // Available V4L2 devices
        var availableV4l2Devices = [];

        // Current config
        var videoInputSources = { videoInputSources: [] };
        var nextSourceId = 1;

        // videotestsrc pattern options
        var testPatterns = [
            { value: 'smpte', label: 'SMPTE Color Bars' },
            { value: 'snow', label: 'Snow (Random Noise)' },
            { value: 'black', label: 'Black' },
            { value: 'white', label: 'White' },
            { value: 'red', label: 'Red' },
            { value: 'green', label: 'Green' },
            { value: 'blue', label: 'Blue' },
            { value: 'checkers-1', label: 'Checkers (1px)' },
            { value: 'checkers-4', label: 'Checkers (4px)' },
            { value: 'circular', label: 'Circular' },
            { value: 'smpte75', label: 'SMPTE 75%' },
            { value: 'ball', label: 'Moving Ball' },
            { value: 'bar', label: 'Bar' },
            { value: 'pinwheel', label: 'Pinwheel' },
            { value: 'gradient', label: 'Gradient' },
        ];

        function EscapeAttr(s) {
            return String(s).replace(/&/g, '&amp;').replace(/"/g, '&quot;')
                .replace(/'/g, '&#39;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
        }

        function EscapeNodeName(name) {
            return name.replace(/[^a-zA-Z0-9_]/g, '_').toLowerCase();
        }

        $(document).ready(function () {
            CheckPipeWireStatus();
            LoadV4l2Devices().then(function () {
                LoadSources();
            });
        });

        function CheckPipeWireStatus() {
            $.getJSON('api/pipewire/audio/sinks')
                .done(function () {
                    $('#pwStatus').html(
                        '<span class="status-indicator status-running"></span>' +
                        'PipeWire running'
                    );
                })
                .fail(function () {
                    $('#pwStatus').html(
                        '<span class="status-indicator status-stopped"></span>' +
                        'PipeWire not responding'
                    );
                });
        }

        function LoadV4l2Devices() {
            return $.getJSON('api/pipewire/video/input-sources/v4l2-devices')
                .done(function (data) {
                    availableV4l2Devices = (data && data.devices) ? data.devices : [];
                })
                .fail(function () {
                    availableV4l2Devices = [];
                });
        }

        function LoadSources() {
            $.getJSON('api/pipewire/video/input-sources')
                .done(function (data) {
                    videoInputSources = data || { videoInputSources: [] };
                    if (!videoInputSources.videoInputSources)
                        videoInputSources.videoInputSources = [];
                    nextSourceId = 1;
                    for (var i = 0; i < videoInputSources.videoInputSources.length; i++) {
                        if (videoInputSources.videoInputSources[i].id >= nextSourceId) {
                            nextSourceId = videoInputSources.videoInputSources[i].id + 1;
                        }
                    }
                    RenderSources();
                })
                .fail(function () {
                    videoInputSources = { videoInputSources: [] };
                    RenderSources();
                });
        }

        function RenderSources() {
            var container = $('#sourcesContainer');
            container.empty();

            if (videoInputSources.videoInputSources.length === 0) {
                container.append(
                    '<div class="no-sources-msg" id="noSourcesMsg">' +
                    '<i class="fas fa-video" style="font-size:2rem; color:var(--bs-secondary-color,#6c757d);"></i>' +
                    '<h4>No Video Input Sources Configured</h4>' +
                    '<p>Add a source to create a persistent video signal in the PipeWire graph.</p>' +
                    '<button class="buttons btn-outline-success" onclick="AddSource()">' +
                    '<i class="fas fa-plus"></i> Add First Source</button>' +
                    '</div>'
                );
                $('#bottomToolbar').hide();
                return;
            }

            $('#bottomToolbar').show();

            for (var i = 0; i < videoInputSources.videoInputSources.length; i++) {
                container.append(RenderSourceCard(videoInputSources.videoInputSources[i], i));
            }
        }

        function RenderSourceCard(source, index) {
            var enabledClass = source.enabled ? '' : ' disabled-source';
            var enabledChecked = source.enabled ? ' checked' : '';
            var nodeName = source.pipeWireNodeName ||
                ('fpp_video_src_' + source.id + '_' + EscapeNodeName(source.name || 'source'));

            var html = '<div class="source-card' + enabledClass + '" id="source-' + source.id + '">';

            // Header
            html += '<div class="source-header">';
            html += '<input type="checkbox" class="form-check-input" onchange="ToggleSourceEnabled(' + index + ', this.checked)"' + enabledChecked + ' title="Enable/Disable source">';
            html += '<input type="text" class="source-name-input" value="' + EscapeAttr(source.name || '') + '" onchange="UpdateSourceName(' + index + ', this.value)" placeholder="Source Name">';
            html += '<span class="badge bg-success pipewire-badge" title="PipeWire node name">' + EscapeAttr(nodeName) + '</span>';
            html += '<div style="flex:1"></div>';
            html += '<button class="buttons btn-outline-danger" onclick="DeleteSource(' + index + ')" title="Delete Source" style="padding:0.25rem 0.5rem;"><i class="fas fa-trash"></i></button>';
            html += '</div>';

            // Body
            html += '<div class="source-body">';

            // Source type selector
            html += '<div class="row align-items-center">';
            html += '<div class="col-auto"><label>Type:</label></div>';
            html += '<div class="col-auto">';
            html += '<select class="form-select form-select-sm" style="width:auto;" onchange="UpdateSourceType(' + index + ',this.value)">';
            html += '<option value="videotestsrc"' + (source.type === 'videotestsrc' ? ' selected' : '') + '>Test Pattern</option>';
            html += '<option value="v4l2src"' + (source.type === 'v4l2src' ? ' selected' : '') + '>USB Camera (V4L2)</option>';
            html += '<option value="rtspsrc"' + (source.type === 'rtspsrc' ? ' selected' : '') + '>RTSP Network Stream</option>';
            html += '<option value="urisrc"' + (source.type === 'urisrc' ? ' selected' : '') + '>Web/HTTP URL</option>';
            html += '<option value="rtpsrc"' + (source.type === 'rtpsrc' ? ' selected' : '') + '>RTP Stream (UDP)</option>';
            html += '</select>';
            html += '</div>';
            html += '</div>';

            // Type-specific settings
            html += RenderSourceSettings(source, index);

            // Resolution / framerate
            html += '<div class="row align-items-center mt-2">';
            html += '<div class="col-auto"><label>Resolution:</label></div>';
            html += '<div class="col-auto">';
            var presetVal = (source.width || 320) + 'x' + (source.height || 240);
            html += '<select class="form-select form-select-sm" style="width:auto;display:inline-block;" onchange="ApplyResolutionPreset(' + index + ',this.value)">';
            var presets = [
                { label: 'Custom', w: 0, h: 0 },
                { label: '240p', w: 426, h: 240 },
                { label: '360p', w: 640, h: 360 },
                { label: '480p SD', w: 854, h: 480 },
                { label: '720p HD', w: 1280, h: 720 },
                { label: '1080p Full HD', w: 1920, h: 1080 },
                { label: '1440p 2K', w: 2560, h: 1440 },
                { label: '2160p 4K', w: 3840, h: 2160 },
                { label: '4320p 8K', w: 7680, h: 4320 }
            ];
            var matched = false;
            for (var p = 0; p < presets.length; p++) {
                var sel = '';
                if (presets[p].w > 0 && presets[p].w == (source.width || 0) && presets[p].h == (source.height || 0)) {
                    sel = ' selected'; matched = true;
                }
                html += '<option value="' + presets[p].w + 'x' + presets[p].h + '"' + sel + '>' + presets[p].label + (presets[p].w > 0 ? ' (' + presets[p].w + 'x' + presets[p].h + ')' : '') + '</option>';
            }
            if (!matched) {
                // Current values don't match any preset — select Custom
                html = html.replace('value="0x0"', 'value="0x0" selected');
            }
            html += '</select>';
            html += '</div>';
            html += '<div class="col-auto">';
            html += '<input type="number" class="form-control form-control-sm" id="resW_' + index + '" style="width:80px;display:inline-block;" value="' + (source.width || 320) + '" onchange="UpdateResolution(' + index + ')" min="16" max="7680"> x ';
            html += '<input type="number" class="form-control form-control-sm" id="resH_' + index + '" style="width:80px;display:inline-block;" value="' + (source.height || 240) + '" onchange="UpdateResolution(' + index + ')" min="16" max="4320">';
            html += '</div>';
            html += '<div class="col-auto"><label>FPS:</label></div>';
            html += '<div class="col-auto">';
            html += '<input type="number" class="form-control form-control-sm" style="width:70px;" value="' + (source.framerate || 10) + '" onchange="UpdateSourceField(' + index + ',\'framerate\',parseInt(this.value))" min="1" max="60">';
            html += '</div>';
            html += '</div>';

            html += '</div>'; // source-body
            html += '</div>'; // source-card

            return html;
        }

        function RenderSourceSettings(source, index) {
            var html = '';
            switch (source.type) {
                case 'videotestsrc':
                    html += '<div class="row align-items-center mt-2">';
                    html += '<div class="col-auto"><label>Pattern:</label></div>';
                    html += '<div class="col-auto">';
                    html += '<select class="form-select form-select-sm" style="width:auto;" onchange="UpdateSourceField(' + index + ',\'pattern\',this.value)">';
                    for (var i = 0; i < testPatterns.length; i++) {
                        var p = testPatterns[i];
                        var sel = ((source.pattern || 'smpte') === p.value) ? ' selected' : '';
                        html += '<option value="' + p.value + '"' + sel + '>' + EscapeAttr(p.label) + '</option>';
                    }
                    html += '</select>';
                    html += '</div>';
                    html += '</div>';
                    break;

                case 'v4l2src':
                    html += '<div class="row align-items-center mt-2">';
                    html += '<div class="col-auto"><label>Device:</label></div>';
                    html += '<div class="col-auto">';
                    if (availableV4l2Devices.length > 0) {
                        html += '<select class="form-select form-select-sm" style="width:auto;" onchange="UpdateSourceField(' + index + ',\'device\',this.value)">';
                        html += '<option value="">— Select Device —</option>';
                        for (var i = 0; i < availableV4l2Devices.length; i++) {
                            var d = availableV4l2Devices[i];
                            var sel = (source.device === d.device) ? ' selected' : '';
                            html += '<option value="' + EscapeAttr(d.device) + '"' + sel + '>' + EscapeAttr(d.device + ' - ' + d.name) + '</option>';
                        }
                        html += '</select>';
                    } else {
                        html += '<input type="text" class="form-control form-control-sm" style="width:200px;" value="' + EscapeAttr(source.device || '/dev/video0') + '" onchange="UpdateSourceField(' + index + ',\'device\',this.value)" placeholder="/dev/video0">';
                        html += ' <span class="text-muted" style="font-size:0.85rem;">(no V4L2 devices detected)</span>';
                    }
                    html += '</div>';
                    html += '</div>';
                    break;

                case 'rtspsrc':
                    html += '<div class="row align-items-center mt-2">';
                    html += '<div class="col-auto"><label>RTSP URL:</label></div>';
                    html += '<div class="col-auto">';
                    html += '<input type="text" class="form-control form-control-sm" style="width:350px;" value="' + EscapeAttr(source.uri || '') + '" onchange="UpdateSourceField(' + index + ',\'uri\',this.value)" placeholder="rtsp://host:554/path">';
                    html += '</div>';
                    html += '</div>';
                    html += '<div class="row align-items-center mt-1">';
                    html += '<div class="col-auto"><label>Latency (ms):</label></div>';
                    html += '<div class="col-auto">';
                    html += '<input type="number" class="form-control form-control-sm" style="width:90px;" value="' + (source.latency || 200) + '" onchange="UpdateSourceField(' + index + ',\'latency\',parseInt(this.value))" min="0" max="10000">';
                    html += '</div>';
                    html += '</div>';
                    break;

                case 'urisrc':
                    html += '<div class="row align-items-center mt-2">';
                    html += '<div class="col-auto"><label>URL:</label></div>';
                    html += '<div class="col-auto">';
                    html += '<input type="text" class="form-control form-control-sm" style="width:420px;" value="' + EscapeAttr(source.uri || '') + '" onchange="UpdateSourceField(' + index + ',\'uri\',this.value)" placeholder="https://www.youtube.com/watch?v=... or HLS URL">';
                    html += '</div>';
                    html += '</div>';
                    html += '<div class="row align-items-center mt-1">';
                    html += '<div class="col-auto"><label>Buffer (sec):</label></div>';
                    html += '<div class="col-auto">';
                    html += '<input type="number" class="form-control form-control-sm" style="width:80px;" value="' + (source.bufferSec != null ? source.bufferSec : 3) + '" onchange="UpdateSourceField(' + index + ',\'bufferSec\',parseFloat(this.value))" min="0" max="30" step="0.5">';
                    html += '</div>';
                    html += '<div class="col-auto text-muted" style="font-size:0.85rem;">YouTube URL, HTTP, HLS, or any GStreamer-supported URI</div>';
                    html += '</div>';
                    // Audio extraction (YouTube only)
                    html += '<div class="row align-items-center mt-2">';
                    html += '<div class="col-auto"><label>Audio:</label></div>';
                    html += '<div class="col-auto">';
                    html += '<input type="checkbox" class="form-check-input" id="audioEn_' + index + '"' + (source.audioEnabled ? ' checked' : '') + ' onchange="ToggleAudioEnabled(' + index + ', this.checked)"> ';
                    html += '<label class="form-check-label" for="audioEn_' + index + '" style="font-weight:normal;">Extract audio from stream</label>';
                    html += '</div>';
                    if (source.audioEnabled) {
                        var audioNode = source.audioPipeWireNodeName || ('fpp_audio_src_' + source.id + '_' + EscapeNodeName(source.name || 'source'));
                        html += '<span class="badge bg-info pipewire-badge" title="PipeWire audio source node" style="margin-left:0.5rem;">' + EscapeAttr(audioNode) + '</span>';
                    }
                    html += '</div>';
                    if (source.audioEnabled) {
                        html += '<div class="row mt-1"><div class="col text-muted" style="font-size:0.8rem; padding-left:5.5rem;">Audio is extracted as a separate PipeWire source node. Add it to an Audio Input Group to route it to an Output Group.</div></div>';
                    }
                    break;

                case 'rtpsrc':
                    html += '<div class="row align-items-center mt-2">';
                    html += '<div class="col-auto"><label>UDP Port:</label></div>';
                    html += '<div class="col-auto">';
                    html += '<input type="number" class="form-control form-control-sm" style="width:100px;" value="' + (source.port || 5004) + '" onchange="UpdateSourceField(' + index + ',\'port\',parseInt(this.value))" min="1024" max="65535">';
                    html += '</div>';
                    html += '</div>';
                    html += '<div class="row align-items-center mt-1">';
                    html += '<div class="col-auto"><label>Encoding:</label></div>';
                    html += '<div class="col-auto">';
                    html += '<select class="form-select form-select-sm" style="width:auto;" onchange="UpdateSourceField(' + index + ',\'encoding\',this.value)">';
                    var encodings = [{ v: 'H264', l: 'H.264' }, { v: 'H265', l: 'H.265 (HEVC)' }, { v: 'MP2T', l: 'MPEG-TS' }, { v: 'RAW', l: 'Raw Video' }, { v: 'JPEG', l: 'Motion JPEG' }];
                    for (var e = 0; e < encodings.length; e++) {
                        var sel = ((source.encoding || 'H264') === encodings[e].v) ? ' selected' : '';
                        html += '<option value="' + encodings[e].v + '"' + sel + '>' + encodings[e].l + '</option>';
                    }
                    html += '</select>';
                    html += '</div>';
                    html += '</div>';
                    html += '<div class="row align-items-center mt-1">';
                    html += '<div class="col-auto"><label>Multicast Group:</label></div>';
                    html += '<div class="col-auto">';
                    html += '<input type="text" class="form-control form-control-sm" style="width:180px;" value="' + EscapeAttr(source.multicastGroup || '') + '" onchange="UpdateSourceField(' + index + ',\'multicastGroup\',this.value)" placeholder="(optional, e.g. 239.1.1.1)">';
                    html += '</div>';
                    html += '</div>';
                    break;
            }
            return html;
        }

        function AddSource() {
            var source = {
                id: nextSourceId++,
                name: 'Video Source ' + (videoInputSources.videoInputSources.length + 1),
                enabled: true,
                type: 'videotestsrc',
                pattern: 'smpte',
                width: 320,
                height: 240,
                framerate: 10
            };
            videoInputSources.videoInputSources.push(source);
            RenderSources();
        }

        function DeleteSource(index) {
            var name = videoInputSources.videoInputSources[index].name || 'this source';
            if (confirm('Delete "' + name + '"?')) {
                videoInputSources.videoInputSources.splice(index, 1);
                RenderSources();
            }
        }

        function ToggleSourceEnabled(index, enabled) {
            videoInputSources.videoInputSources[index].enabled = enabled;
            RenderSources();
        }

        function UpdateSourceName(index, name) {
            videoInputSources.videoInputSources[index].name = name;
            var slug = EscapeNodeName(name);
            videoInputSources.videoInputSources[index].pipeWireNodeName =
                'fpp_video_src_' + videoInputSources.videoInputSources[index].id + '_' + slug;
            RenderSources();
        }

        function ToggleAudioEnabled(index, enabled) {
            videoInputSources.videoInputSources[index].audioEnabled = enabled;
            RenderSources();
        }

        function UpdateSourceType(index, type) {
            var src = videoInputSources.videoInputSources[index];
            src.type = type;
            // Set defaults for new type
            delete src.pattern;
            delete src.device;
            delete src.uri;
            delete src.latency;
            delete src.bufferSec;
            delete src.port;
            delete src.encoding;
            delete src.multicastGroup;
            delete src.audioEnabled;
            if (type === 'videotestsrc') {
                src.pattern = 'smpte';
            } else if (type === 'v4l2src') {
                src.device = availableV4l2Devices.length > 0 ? availableV4l2Devices[0].device : '/dev/video0';
            } else if (type === 'rtspsrc') {
                src.uri = '';
                src.latency = 200;
            } else if (type === 'urisrc') {
                src.uri = '';
                src.bufferSec = 3;
            } else if (type === 'rtpsrc') {
                src.port = 5004;
                src.encoding = 'H264';
                src.multicastGroup = '';
            }
            RenderSources();
        }

        function UpdateSourceField(index, field, value) {
            videoInputSources.videoInputSources[index][field] = value;
        }

        function ApplyResolutionPreset(index, val) {
            var parts = val.split('x');
            var w = parseInt(parts[0]);
            var h = parseInt(parts[1]);
            if (w > 0 && h > 0) {
                videoInputSources.videoInputSources[index].width = w;
                videoInputSources.videoInputSources[index].height = h;
                $('#resW_' + index).val(w);
                $('#resH_' + index).val(h);
            }
        }

        function UpdateResolution(index) {
            var w = parseInt($('#resW_' + index).val()) || 320;
            var h = parseInt($('#resH_' + index).val()) || 240;
            videoInputSources.videoInputSources[index].width = w;
            videoInputSources.videoInputSources[index].height = h;
            // Reset preset dropdown to Custom if values don't match
            RenderSources();
        }

        function SaveSources() {
            $.ajax({
                url: 'api/pipewire/video/input-sources',
                type: 'POST',
                contentType: 'application/json',
                data: JSON.stringify(videoInputSources),
                success: function () {
                    $.jGrowl('Video input sources saved', { theme: 'success' });
                },
                error: function (xhr) {
                    $.jGrowl('Failed to save: ' + (xhr.responseJSON ? xhr.responseJSON.message : xhr.statusText), { theme: 'danger' });
                }
            });
        }

        function ApplySources() {
            ShowApplyProgress('Saving configuration...');
            $.ajax({
                url: 'api/pipewire/video/input-sources',
                type: 'POST',
                contentType: 'application/json',
                data: JSON.stringify(videoInputSources),
                success: function () {
                    ShowApplyProgress('Applying video input sources...');
                    $.ajax({
                        url: 'api/pipewire/video/input-sources/apply',
                        type: 'POST',
                        contentType: 'application/json',
                        data: '{}',
                        success: function (result) {
                            HideApplyProgress();
                            $.jGrowl('Video input sources applied: ' + (result.message || 'OK'), { theme: 'success' });
                            LoadSources();
                        },
                        error: function (xhr) {
                            HideApplyProgress();
                            $.jGrowl('Failed to apply: ' + (xhr.responseJSON ? xhr.responseJSON.message : xhr.statusText), { theme: 'danger' });
                        }
                    });
                },
                error: function (xhr) {
                    HideApplyProgress();
                    $.jGrowl('Failed to save before apply: ' + (xhr.responseJSON ? xhr.responseJSON.message : xhr.statusText), { theme: 'danger' });
                }
            });
        }

        function ShowApplyProgress(msg) {
            if ($('#applyOverlay').length === 0) {
                $('body').append(
                    '<div id="applyOverlay"><div class="apply-box">' +
                    '<div class="spinner-border text-primary mb-3" role="status"></div>' +
                    '<div id="applyMsg"></div></div></div>'
                );
            }
            $('#applyMsg').text(msg || 'Applying...');
            $('#applyOverlay').show();
        }

        function HideApplyProgress() {
            $('#applyOverlay').hide();
        }
    </script>
</body>

</html>