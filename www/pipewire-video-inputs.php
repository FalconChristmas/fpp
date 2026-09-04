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

    <?php $modalMode = isset($_GET['modal']) && $_GET['modal'] == '1'; ?>

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
            background: var(--bs-success, #28a745);
        }

        .status-stopped {
            background: var(--bs-danger, #dc3545);
        }

        .status-unknown {
            background: var(--bs-warning, #ffc107);
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

<body<?php if ($modalMode)
    echo ' class="modal-mode m-0 p-3"'; ?>>
    <?php if (!$modalMode) { ?>
        <div id="bodyWrapper">
            <?php
            $activeParentMenuItem = 'status';
            include 'menu.inc';
            ?>
            <div class="mainContainer">
                <h1 class="title">PipeWire Video Input Sources</h1>
                <div class="pageContent">
                <?php } ?>

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
                        <i class="fas fa-exclamation-triangle fa-2x text-warning"></i>
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

                    <?php $ytdlpVersion = trim(shell_exec('yt-dlp --version 2>/dev/null')); ?>
                    <div class="alert alert-secondary d-flex flex-wrap align-items-center gap-3">
                        <div class="me-auto">
                            <i class="fas fa-cloud-download-alt"></i>
                            <strong>Web/HTTP URL</strong> sources resolve YouTube links with <code>yt-dlp</code>
                            <?php if ($ytdlpVersion != "") { ?>
                                &mdash; installed version <code><?= htmlspecialchars($ytdlpVersion) ?></code>.
                            <?php } else { ?>
                                &mdash; <span class="text-danger">not installed</span>.
                            <?php } ?>
                            <br>
                            <small class="text-body-secondary">YouTube reworks its player every few months, and a
                                yt-dlp older than that stops resolving links entirely &mdash; the source starts but
                                never produces a frame. FPP checks weekly for a newer one.</small>
                        </div>
                        <?php PrintSettingCheckbox('Keep yt-dlp updated', 'ytdlpAutoUpdate', 0, 0, '1', '0', '', '', 1); ?>
                    </div>

                    <div id="pipewireStatus" class="toolbar">
                        <div class="toolbar-left">
                            <span id="pwStatus"><span class="status-indicator status-unknown"></span> Checking PipeWire
                                status...</span>
                        </div>
                        <div class="toolbar-right">
                            <?php if (!$modalMode) { ?>
                                <a class="btn btn-sm btn-outline-secondary" href="settings.php#settings-av"
                                    title="Back to Pipewire Settings">
                                    <i class="fas fa-arrow-left"></i> Pipewire Settings
                                </a>
                            <?php } ?>
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
                            <i class="fas fa-video fs-1 text-secondary"></i>
                            <h4>No Video Input Sources Configured</h4>
                            <p>Add a source to create a persistent video signal in the PipeWire graph
                                (test patterns, cameras, etc.).</p>
                            <button class="buttons btn-outline-success" onclick="AddSource()">
                                <i class="fas fa-plus"></i> Add First Source
                            </button>
                        </div>
                    </div>

                    <div id="bottomToolbar" class="toolbar d-none mt-3">
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

                <?php if (!$modalMode) { ?>
                </div>
            </div>

            <?php include 'common/footer.inc'; ?>
        </div>
    <?php } else { ?>
        <div class="modal fade" id="modalDialogBase" tabindex="-1" data-bs-backdrop="static" data-bs-keyboard="false"
            aria-labelledby="modalDialogLabel" aria-hidden="true">
            <div class="modal-dialog">
                <div class="modal-content">
                    <div class="modal-header">
                        <h3 class="modal-title fs-5" id="modalDialogLabel"></h3>
                        <button id="modalCloseButton" type="button" class="btn-close" data-bs-dismiss="modal"
                            aria-label="Close"></button>
                    </div>
                    <div class="modal-body"></div>
                    <div class="modal-footer"></div>
                </div>
            </div>
        </div>
    <?php } ?>

    <script>
        // Available V4L2 devices
        var availableV4l2Devices = [];

        // Current config
        var videoInputSources = { videoInputSources: [] };
        var nextSourceId = 1;
        // Set when the config has been modified but not yet saved
        var hasUnsavedChanges = false;

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
            hasUnsavedChanges = false;
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

        // Clear the unsaved-changes state once the config has been written to disk.
        // The banner is removed in place rather than re-rendering the cards.
        function ClearUnsavedChanges() {
            hasUnsavedChanges = false;
            $('#unsavedChangesBanner').remove();
        }

        // Banner shown while added/deleted sources are not yet written to disk
        function UnsavedChangesBanner() {
            if (!hasUnsavedChanges) return '';
            return '<div class="alert alert-warning d-flex align-items-center gap-2 mb-3" id="unsavedChangesBanner">' +
                '<i class="fas fa-exclamation-triangle"></i>' +
                '<div>Sources have been added or deleted but <b>not saved yet</b>. ' +
                'Click <b>Save &amp; Apply</b> to make the change permanent.</div>' +
                '</div>';
        }

        // ------------------------------------------------------------------
        // Live preview
        //
        // The preview endpoint returns a single JPEG, so "live" here is a
        // polled still rather than a video stream -- no second decode
        // pipeline is held open for every config page someone leaves sitting
        // on a monitor.
        //
        // Two things decide how fluid that looks:
        //
        //  * Pacing.  This used to sleep a flat 500ms *after* each frame
        //    finished loading, so the real interval was 500ms + round-trip
        //    (measured 33-77ms) -- about 1.8fps, with the cadence visibly
        //    wandering as the round-trip moved.  Scheduling on a fixed
        //    period instead makes the spacing even, which reads as much
        //    smoother than the raw frame count suggests.
        //
        //  * Whether anyone is looking.  A frame costs roughly 2% of a core
        //    on a Pi (snapshot pipeline build, 1080p->320 scale, JPEG
        //    encode), so 10fps is around 20% of one core.  That is only
        //    affordable because a preview that is scrolled out of view or in
        //    a background tab stops asking for frames entirely.
        var previewTimers = {};

        // Per-preview pacing state, keyed by source id: { rtt, targetMs }.
        var previewPacing = {};

        var PREVIEW_TARGET_MS = 100;   // 10fps ceiling
        var PREVIEW_MAX_MS = 1000;     // floor of 1fps when the server is slow
        var PREVIEW_IDLE_MS = 1000;    // re-check rate while nothing is watching

        // Is this preview worth spending a frame on right now?
        function PreviewWorthDrawing(img) {
            if (document.visibilityState === 'hidden') return false;

            // The <img> carries d-none until the first frame lands, so it
            // measures 0x0 at exactly the moment we most need to fetch.
            // Measure its container in that case, and if nothing can be
            // measured at all, fail open -- a preview that stalls forever is
            // far worse than one that draws a frame nobody is looking at.
            var el = (img.getBoundingClientRect().height > 0) ? img : (img.parentElement || img);
            var r = el.getBoundingClientRect();
            if (r.width === 0 && r.height === 0) return true;

            var vh = window.innerHeight || document.documentElement.clientHeight;
            return r.bottom > 0 && r.top < vh;
        }

        // Warn when the configured size/rate isn't one the camera advertises.
        // FPP scales and re-times whatever the camera gives it, so a mismatch
        // is not fatal -- but a native mode costs less CPU and looks better.
        function BuildNativeModeHint(source) {
            var dev = null;
            for (var i = 0; i < availableV4l2Devices.length; i++) {
                if (availableV4l2Devices[i].device === source.device) {
                    dev = availableV4l2Devices[i];
                    break;
                }
            }
            if (!dev || !dev.modes || dev.modes.length === 0) return '';

            var exact = false;
            var sizes = {};
            for (var j = 0; j < dev.modes.length; j++) {
                var m = dev.modes[j];
                sizes[m.width + 'x' + m.height] = true;
                if (m.width === source.width && m.height === source.height) exact = true;
            }
            if (exact) return '';

            var list = Object.keys(sizes).slice(0, 8).join(', ');
            return '<div class="row mt-1"><div class="col-auto">' +
                '<small class="text-warning"><i class="fas fa-info-circle"></i> ' +
                EscapeAttr(source.width + 'x' + source.height) + ' is not a native mode for this camera ' +
                '(FPP will scale). Native sizes: ' + EscapeAttr(list) + '</small>' +
                '</div></div>';
        }

        // Device-level controls for a capture card / webcam.
        //
        // These are deliberately separate from the Resolution/FPS row: those
        // describe the stream FPP produces, while these are settings pushed
        // into the camera itself.  Flicker under mains lighting is the case
        // that forces the distinction -- it is decided at exposure time, so
        // nothing downstream of the capture can undo it.
        function BuildV4L2ControlsBlock(source, index) {
            var dev = null;
            for (var i = 0; i < availableV4l2Devices.length; i++) {
                if (availableV4l2Devices[i].device === source.device) {
                    dev = availableV4l2Devices[i];
                    break;
                }
            }

            var plf = (typeof source.powerLineFrequency === 'number') ? source.powerLineFrequency : -1;
            var expMode = source.exposureMode || 'camera';
            var dynFps = (typeof source.dynamicFramerate === 'number') ? source.dynamicFramerate : -1;

            function Options(opts, current) {
                var out = '';
                for (var i = 0; i < opts.length; i++) {
                    var sel = (opts[i].v === current) ? ' selected' : '';
                    out += '<option value="' + opts[i].v + '"' + sel + '>' + EscapeAttr(opts[i].label) + '</option>';
                }
                return out;
            }

            var html = '<div class="row align-items-center mt-2">';
            html += '<div class="col-auto"><label>Anti-flicker:</label></div>';
            html += '<div class="col-auto">';
            html += '<select class="form-select form-select-sm w-auto" onchange="UpdateSourceField(' + index + ',\'powerLineFrequency\',parseInt(this.value))">';
            html += Options([
                { v: -1, label: 'Camera default (leave unchanged)' },
                { v: 0, label: 'Off' },
                { v: 1, label: '50 Hz mains (UK, Europe, Asia, Africa, Australia)' },
                { v: 2, label: '60 Hz mains (Americas, Japan, Taiwan, South Korea)' }
            ], plf);
            html += '</select>';
            html += '</div>';

            // What the camera is set to *right now* -- the mismatch between
            // this and the room is the whole diagnosis, and it is invisible
            // otherwise.
            if (dev && dev.powerLineFrequency !== null && typeof dev.powerLineFrequency !== 'undefined') {
                var names = { 0: 'Off', 1: '50 Hz', 2: '60 Hz' };
                var cur = names[dev.powerLineFrequency] || 'unknown';
                html += '<div class="col-auto"><small class="text-muted">Camera currently reports <b>' + EscapeAttr(cur) + '</b></small></div>';
            }
            html += '</div>';

            html += '<div class="row"><div class="col-auto">';
            html += '<small class="text-muted">Locks the camera\'s exposure to whole cycles of the mains supply. ' +
                    'Rolling bands or a pulsing brightness under artificial lighting nearly always means this is set ' +
                    'for the wrong region &mdash; changing the FPS above will not fix it.</small>';
            html += '</div></div>';

            // Only offered when the camera actually implements the control;
            // otherwise the select would silently do nothing.
            if (!dev || dev.hasExposureControls) {
                html += '<div class="row align-items-center mt-2">';
                html += '<div class="col-auto"><label>Exposure:</label></div>';
                html += '<div class="col-auto">';
                html += '<select class="form-select form-select-sm w-auto" onchange="UpdateExposureMode(' + index + ',this.value)">';
                html += Options([
                    { v: 'camera', label: 'Camera default (leave unchanged)' },
                    { v: 'auto', label: 'Auto' },
                    { v: 'manual', label: 'Manual' }
                ], expMode);
                html += '</select>';
                html += '</div>';

                var shutterMs = (typeof source.exposureTime100us === 'number' && source.exposureTime100us > 0)
                    ? (source.exposureTime100us / 10) : 10;
                html += '<div class="col-auto' + (expMode === 'manual' ? '' : ' d-none') + '" id="shutterGroup_' + index + '">';
                html += '<label class="me-1">Shutter:</label>';
                html += '<input type="number" class="form-control form-control-sm d-inline-block w-auto" value="' + shutterMs + '" ' +
                        'onchange="UpdateShutterMs(' + index + ',this.value)" min="0.1" max="1000" step="0.1"> ms';
                html += '</div>';
                html += '</div>';

                html += '<div class="row' + (expMode === 'manual' ? '' : ' d-none') + '" id="shutterHint_' + index + '"><div class="col-auto">';
                html += '<small class="text-muted">Use 10 ms (or any multiple) under 50 Hz mains, 8.33 ms under 60 Hz. ' +
                        'A fixed shutter also stops the picture breathing as the lighting state changes mid-show.</small>';
                html += '</div></div>';

                html += '<div class="row align-items-center mt-2">';
                html += '<div class="col-auto"><label>Auto-exposure may lower FPS:</label></div>';
                html += '<div class="col-auto">';
                html += '<select class="form-select form-select-sm w-auto" onchange="UpdateSourceField(' + index + ',\'dynamicFramerate\',parseInt(this.value))">';
                html += Options([
                    { v: -1, label: 'Camera default (leave unchanged)' },
                    { v: 0, label: 'No \u2014 hold the frame rate' },
                    { v: 1, label: 'Yes \u2014 allow longer exposures in low light' }
                ], dynFps);
                html += '</select>';
                html += '</div>';
                html += '</div>';
            }

            return html;
        }

        function BuildPreviewBlock(source) {
            var id = parseInt(source.id, 10);

            // A v4l2 source can be previewed straight off the device even
            // while stopped, so the operator can confirm the camera before
            // committing.  Every other type is only reachable through fppd's
            // running pipeline, so say so rather than letting them press a
            // button that can only fail.
            var hint = '<small>Preview stopped.</small>';
            if (!source.enabled && source.type !== 'v4l2src') {
                hint = '<small>Enable this source and click <b>Save &amp; Apply</b> to preview it.</small>';
            }

            return '<div class="row align-items-start mt-2">' +
                '<div class="col-auto"><label>Preview:</label></div>' +
                '<div class="col-auto">' +
                '<div class="d-flex flex-column gap-1">' +
                '<img id="videoPreviewImg' + id + '" class="border rounded d-none" alt="Video input preview" width="320">' +
                '<div id="videoPreviewMsg' + id + '" class="text-muted">' + hint + '</div>' +
                '<div class="d-flex gap-2">' +
                '<button type="button" class="btn btn-sm btn-outline-primary" id="videoPreviewBtn' + id + '" onclick="TogglePreview(' + id + ')">' +
                '<i class="fas fa-play"></i> Start Preview</button>' +
                '</div></div></div></div>';
        }

        function TogglePreview(id) {
            if (previewTimers[id]) {
                StopPreview(id);
            } else {
                StartPreview(id);
            }
        }

        function StopPreview(id) {
            if (previewTimers[id]) {
                clearTimeout(previewTimers[id]);
                delete previewTimers[id];
            }
            delete previewPacing[id];
            $('#videoPreviewImg' + id).addClass('d-none').removeAttr('src');
            $('#videoPreviewMsg' + id).removeClass('text-danger').addClass('text-muted')
                .html('<small>Preview stopped.</small>');
            $('#videoPreviewBtn' + id).html('<i class="fas fa-play"></i> Start Preview');
        }

        function StartPreview(id) {
            $('#videoPreviewBtn' + id).html('<i class="fas fa-stop"></i> Stop Preview');
            $('#videoPreviewMsg' + id).removeClass('text-danger').addClass('text-muted')
                .html('<small>Connecting&hellip;</small>');
            previewTimers[id] = setTimeout(function () { PreviewTick(id); }, 0);
        }

        // Chained timeouts rather than setInterval: a slow or failing grab
        // must not stack up requests behind itself, and a device that has
        // gone away should back off instead of hammering gst-launch.
        function PreviewTick(id) {
            if (!previewTimers[id]) return;
            var img = document.getElementById('videoPreviewImg' + id);
            if (!img) {           // row was re-rendered out from under us
                StopPreview(id);
                return;
            }

            function schedule(delay) {
                previewTimers[id] = setTimeout(function () { PreviewTick(id); }, delay);
            }

            // Nothing is looking at it: skip the frame entirely rather than
            // paying for one nobody sees.  This is what pays for the higher
            // rate above.
            if (!PreviewWorthDrawing(img)) {
                schedule(PREVIEW_IDLE_MS);
                return;
            }

            var pace = previewPacing[id];
            if (!pace) {
                pace = previewPacing[id] = { rtt: 0, targetMs: PREVIEW_TARGET_MS };
            }

            var started = Date.now();
            var url = 'api/pipewire/video/input-sources/' + id + '/preview?width=320&_=' + started;
            var probe = new Image();

            probe.onload = function () {
                if (!previewTimers[id]) return;
                img.src = probe.src;
                $('#videoPreviewImg' + id).removeClass('d-none');
                $('#videoPreviewMsg' + id).addClass('d-none');

                // Never ask for frames faster than the server has actually
                // been delivering them.  Smoothing the round-trip rather than
                // reacting to the last one keeps a single slow frame from
                // lurching the rate, and the 1.1 margin leaves the daemon
                // some headroom instead of running it at exactly saturation.
                // On a slower Pi, or a 4K source, this settles at whatever
                // rate is sustainable instead of queueing up requests.
                var elapsed = Date.now() - started;
                pace.rtt = pace.rtt ? (pace.rtt * 0.7 + elapsed * 0.3) : elapsed;
                pace.targetMs = Math.min(PREVIEW_MAX_MS,
                                         Math.max(PREVIEW_TARGET_MS, Math.round(pace.rtt * 1.1)));

                // Fixed period, not a fixed gap: the time already spent
                // fetching comes out of the wait, so the spacing stays even.
                schedule(Math.max(0, pace.targetMs - elapsed));
            };

            probe.onerror = function () {
                if (!previewTimers[id]) return;
                $('#videoPreviewImg' + id).addClass('d-none');
                $('#videoPreviewMsg' + id).removeClass('d-none text-muted').addClass('text-danger')
                    .html('<small><i class="fas fa-exclamation-triangle"></i> No frames. ' +
                          'Check the device is connected, then Save &amp; Apply and retry.</small>');
                // Nothing learned about pacing from a failure -- start clean
                // when frames come back.
                pace.rtt = 0;
                pace.targetMs = PREVIEW_TARGET_MS;
                schedule(3000);
            };
            probe.src = url;
        }

        // Re-rendering the source list destroys the <img> elements the
        // running previews write into, so drop the timers with them.
        function StopAllPreviews() {
            for (var id in previewTimers) {
                if (previewTimers.hasOwnProperty(id)) {
                    clearTimeout(previewTimers[id]);
                }
            }
            previewTimers = {};
            previewPacing = {};
        }

        function RenderSources() {
            StopAllPreviews();
            var container = $('#sourcesContainer');
            container.empty();
            container.append(UnsavedChangesBanner());

            if (videoInputSources.videoInputSources.length === 0) {
                container.append(
                    '<div class="no-sources-msg" id="noSourcesMsg">' +
                    '<i class="fas fa-video fs-1 text-secondary"></i>' +
                    '<h4>No Video Input Sources Configured</h4>' +
                    '<p>Add a source to create a persistent video signal in the PipeWire graph.</p>' +
                    '<button class="buttons btn-outline-success" onclick="AddSource()">' +
                    '<i class="fas fa-plus"></i> Add First Source</button>' +
                    '</div>'
                );
                // Keep the toolbar (and its Save buttons) available when the last source
                // has just been deleted, otherwise the deletion can never be saved and
                // the source reappears on reload.
                $('#bottomToolbar').toggleClass('d-none', !hasUnsavedChanges);
                return;
            }

            $('#bottomToolbar').removeClass('d-none');

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
            html += '<div class="flex-grow-1"></div>';
            html += '<button class="buttons btn-outline-danger" onclick="DeleteSource(' + index + ')" title="Delete Source"><i class="fas fa-trash"></i></button>';
            html += '</div>';

            // Body
            html += '<div class="source-body">';

            // Source type selector
            html += '<div class="row align-items-center">';
            html += '<div class="col-auto"><label>Type:</label></div>';
            html += '<div class="col-auto">';
            html += '<select class="form-select form-select-sm w-auto" onchange="UpdateSourceType(' + index + ',this.value)">';
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
            html += '<select class="form-select form-select-sm w-auto d-inline-block" onchange="ApplyResolutionPreset(' + index + ',this.value)">';
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
            html += '<input type="number" class="form-control form-control-sm d-inline-block" id="resW_' + index + '" style="width:80px;" value="' + (source.width || 320) + '" onchange="UpdateResolution(' + index + ')" min="16" max="7680"> x ';
            html += '<input type="number" class="form-control form-control-sm d-inline-block" id="resH_' + index + '" style="width:80px;" value="' + (source.height || 240) + '" onchange="UpdateResolution(' + index + ')" min="16" max="4320">';
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
                    html += '<select class="form-select form-select-sm w-auto" onchange="UpdateSourceField(' + index + ',\'pattern\',this.value)">';
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
                        html += '<select class="form-select form-select-sm w-auto" onchange="UpdateSourceField(' + index + ',\'device\',this.value)">';
                        html += '<option value="">-- Select Device --</option>';
                        for (var i = 0; i < availableV4l2Devices.length; i++) {
                            var d = availableV4l2Devices[i];
                            var sel = (source.device === d.device) ? ' selected' : '';
                            html += '<option value="' + EscapeAttr(d.device) + '"' + sel + '>' + EscapeAttr(d.device + ' - ' + d.name) + '</option>';
                        }
                        html += '</select>';
                    } else {
                        html += '<input type="text" class="form-control form-control-sm w-auto" value="' + EscapeAttr(source.device || '/dev/video0') + '" onchange="UpdateSourceField(' + index + ',\'device\',this.value)" placeholder="/dev/video0">';
                        html += ' <span class="text-muted"><small>(no capture devices detected &mdash; check the camera is plugged in)</small></span>';
                    }
                    html += '</div>';
                    html += '</div>';
                    html += BuildNativeModeHint(source);
                    html += BuildV4L2ControlsBlock(source, index);
                    break;

                case 'rtspsrc':
                    html += '<div class="row align-items-center mt-2">';
                    html += '<div class="col-auto"><label>RTSP URL:</label></div>';
                    html += '<div class="col-auto">';
                    html += '<input type="text" class="form-control form-control-sm placeholder-muted" style="width:350px;" value="' + EscapeAttr(source.uri || '') + '" onchange="UpdateSourceField(' + index + ',\'uri\',this.value)" placeholder="rtsp://host:554/path">';
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
                    html += '<input type="text" class="form-control form-control-sm placeholder-muted" style="width:420px;" value="' + EscapeAttr(source.uri || '') + '" onchange="UpdateSourceField(' + index + ',\'uri\',this.value)" placeholder="https://www.youtube.com/watch?v=... or HLS URL">';
                    html += '</div>';
                    html += '</div>';
                    html += '<div class="row align-items-center mt-1">';
                    html += '<div class="col-auto"><label>Buffer (sec):</label></div>';
                    html += '<div class="col-auto">';
                    html += '<input type="number" class="form-control form-control-sm" style="width:80px;" value="' + (source.bufferSec != null ? source.bufferSec : 3) + '" onchange="UpdateSourceField(' + index + ',\'bufferSec\',parseFloat(this.value))" min="0" max="30" step="0.5">';
                    html += '</div>';
                    html += '<div class="col-auto text-muted"><small>YouTube URL, HTTP, HLS, or any GStreamer-supported URI</small></div>';
                    html += '</div>';
                    // Audio extraction (YouTube only)
                    html += '<div class="row align-items-center mt-2">';
                    html += '<div class="col-auto"><label>Audio:</label></div>';
                    html += '<div class="col-auto">';
                    html += '<input type="checkbox" class="form-check-input" id="audioEn_' + index + '"' + (source.audioEnabled ? ' checked' : '') + ' onchange="ToggleAudioEnabled(' + index + ', this.checked)"> ';
                    html += '<label class="form-check-label fw-normal" for="audioEn_' + index + '">Extract audio from stream</label>';
                    html += '</div>';
                    if (source.audioEnabled) {
                        var audioNode = source.audioPipeWireNodeName || ('fpp_audio_src_' + source.id + '_' + EscapeNodeName(source.name || 'source'));
                        html += '<span class="badge bg-info pipewire-badge ms-2" title="PipeWire audio source node">' + EscapeAttr(audioNode) + '</span>';
                    }
                    html += '</div>';
                    if (source.audioEnabled) {
                        html += '<div class="row mt-1"><div class="col text-muted" style="padding-left:5.5rem;"><small>Audio is extracted as a separate PipeWire source node. Add it to an Audio Input Group to route it to an Output Group.</small></div></div>';
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
                    html += '<select class="form-select form-select-sm w-auto" onchange="UpdateSourceField(' + index + ',\'encoding\',this.value)">';
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
                    html += '<input type="text" class="form-control form-control-sm placeholder-muted" style="width:180px;" value="' + EscapeAttr(source.multicastGroup || '') + '" onchange="UpdateSourceField(' + index + ',\'multicastGroup\',this.value)" placeholder="(optional, e.g. 239.1.1.1)">';
                    html += '</div>';
                    html += '</div>';
                    break;
            }

            // Every source type benefits from a preview, not just cameras:
            // it is the quickest way to tell a mis-typed RTSP URL or a dead
            // stream from a routing problem further downstream.
            html += BuildPreviewBlock(source);

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
            hasUnsavedChanges = true;
            RenderSources();
        }

        function DeleteSource(index) {
            var name = videoInputSources.videoInputSources[index].name || 'this source';
            if (confirm('Delete "' + name + '"?')) {
                videoInputSources.videoInputSources.splice(index, 1);
                hasUnsavedChanges = true;
                RenderSources();
                $.jGrowl('Source deleted — click "Save & Apply" to make the change permanent.', { themeState: 'warning' });
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
            delete src.powerLineFrequency;
            delete src.exposureMode;
            delete src.exposureTime100us;
            delete src.dynamicFramerate;
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

        function UpdateExposureMode(index, mode) {
            videoInputSources.videoInputSources[index].exposureMode = mode;
            // Toggled in place rather than via RenderSources(), which stops
            // every running preview.
            $('#shutterGroup_' + index).toggleClass('d-none', mode !== 'manual');
            $('#shutterHint_' + index).toggleClass('d-none', mode !== 'manual');
            if (mode === 'manual' && !videoInputSources.videoInputSources[index].exposureTime100us) {
                // 10ms: one full 50Hz half-cycle, and a sane starting point.
                videoInputSources.videoInputSources[index].exposureTime100us = 100;
            }
        }

        function UpdateShutterMs(index, val) {
            // Stored in the V4L2 control's own 100us units so the config maps
            // 1:1 onto exposure_time_absolute; milliseconds are just the UI.
            var ms = parseFloat(val);
            if (isNaN(ms) || ms <= 0) return;
            videoInputSources.videoInputSources[index].exposureTime100us = Math.round(ms * 10);
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
                    ClearUnsavedChanges();
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
                    ClearUnsavedChanges();
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