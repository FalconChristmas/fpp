<!DOCTYPE html>
<html lang="en">

<head>
    <?php
    include 'common/htmlMeta.inc';
    require_once "common.php";
    require_once 'config.php';
    include 'common/menuHead.inc';
    ?>

    <title><? echo $pageTitle; ?> - RTSP Video Outputs</title>

    <?php $modalMode = isset($_GET['modal']) && $_GET['modal'] == '1'; ?>
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
                <h1 class="title">RTSP Video Outputs</h1>
                <div class="pageContent">
                <?php } ?>

                <?php
                $mediaBackend = isset($settings['MediaBackend']) ? $settings['MediaBackend'] : 'alsa';
                if ($mediaBackend !== 'pipewire') {
                    ?>
                    <div class="alert alert-warning">
                        <h4 class="alert-heading"><i class="fas fa-exclamation-triangle"></i> Advanced PipeWire Required</h4>
                        <p class="mb-0">RTSP outputs require the Advanced PipeWire backend. Change it in
                            <a href="settings.php?tab=Audio%2FVideo">FPP Settings &rarr; Audio/Video</a>, then return here.</p>
                    </div>
                <?php } else { ?>

                    <p class="text-muted">
                        Serves a video output, with its audio, as an RTSP stream that a player opens by URL &mdash;
                        point VLC, ffplay, OBS or a video wall at the address below and it negotiates the rest.
                        Unlike the RTP network outputs, no SDP file has to be copied to the receiver.
                    </p>

                    <div class="row align-items-center g-2 mb-3">
                        <div class="col-auto">
                            <input type="checkbox" class="form-check-input" id="rtspEnabled">
                            <label class="form-check-label fw-semibold ms-1" for="rtspEnabled">Enable RTSP server</label>
                        </div>
                        <div class="col-auto"><label class="form-label mb-0" for="rtspPort">Port:</label></div>
                        <div class="col-auto">
                            <input type="number" class="form-control form-control-sm" id="rtspPort" min="1024" max="65535" value="8554">
                        </div>
                        <div class="col-auto" id="serverStatus"></div>
                    </div>

                    <div class="alert alert-secondary py-2">
                        <small><i class="fas fa-info-circle"></i>
                            The server listens on every interface and does not ask for a password, the same as FPP's
                            other network media outputs. Anyone who can reach this player on the network can watch
                            these streams.</small>
                    </div>

                    <div id="statusNotes"></div>

                    <div id="mountList"></div>

                    <div class="d-flex gap-2 mt-3">
                        <button class="btn btn-outline-primary btn-sm" onclick="AddMount()">
                            <i class="fas fa-plus"></i> Add RTSP Output</button>
                        <button class="btn btn-success btn-sm" id="saveBtn" onclick="SaveAndApply()">
                            <i class="fas fa-save"></i> Save &amp; Apply</button>
                    </div>

                    <div id="saveResult" class="mt-3"></div>

                <?php } ?>

                <?php if (!$modalMode) { ?>
                </div>
            </div>
            <?php include 'common/footer.inc'; ?>
        </div>
    <?php } ?>

    <script>
        var rtspConfig = { enabled: false, port: 8554, mounts: [] };
        var videoSources = [];
        var nextMountId = 1;

        function EscapeAttr(s) {
            return String(s).replace(/&/g, '&amp;').replace(/"/g, '&quot;')
                .replace(/'/g, '&#39;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
        }

        $(document).ready(function () {
            LoadVideoSources().always(function () {
                LoadConfig().always(RefreshStatus);
            });
        });

        function LoadVideoSources() {
            return $.getJSON('api/pipewire/video/input-sources').done(function (data) {
                var list = (data && data.videoInputSources) ? data.videoInputSources : [];
                videoSources = list.map(function (s) {
                    return { node: s.pipeWireNodeName, name: s.name };
                });
            });
        }

        function LoadConfig() {
            return $.getJSON('api/pipewire/rtspoutputs').done(function (data) {
                rtspConfig = data || { enabled: false, port: 8554, mounts: [] };
                if (!rtspConfig.mounts) rtspConfig.mounts = [];
                rtspConfig.mounts.forEach(function (m) {
                    if (m.id >= nextMountId) nextMountId = m.id + 1;
                });
                $('#rtspEnabled').prop('checked', !!rtspConfig.enabled);
                $('#rtspPort').val(rtspConfig.port || 8554);
                RenderMounts();
            });
        }

        // Live state from fppd, which is the only thing that knows whether the
        // port actually bound. The saved config only says what was asked for.
        function RefreshStatus() {
            $.getJSON('api/pipewire/rtspoutputs/status').done(function (st) {
                var html;
                if (st.unavailable && st.reason === 'unsupported') {
                    // fppd answered but has no RTSP endpoint: built without
                    // gst-rtsp-server, or it predates this feature.
                    html = '<span class="badge bg-warning text-dark" title="fppd is running but has no RTSP ' +
                        'endpoint - it was built without gst-rtsp-server, or predates this feature">' +
                        'RTSP not available in fppd</span>';
                } else if (st.unavailable) {
                    html = '<span class="badge bg-secondary">fppd not running</span>';
                } else if (st.active) {
                    html = '<span class="badge bg-success">Server running on port ' + (st.port || '') + '</span>';
                } else {
                    html = '<span class="badge bg-secondary">Server stopped</span>';
                }
                $('#serverStatus').html(html);

                // fppd is the only thing that knows an audio branch is still
                // waiting for a source, so show what it says per mount.
                var notes = '';
                (st.mounts || []).forEach(function (m) {
                    if (m.note) {
                        notes += '<div class="alert alert-info py-2 mb-2"><small><i class="fas fa-info-circle"></i> ' +
                            EscapeAttr(m.name) + ': ' + EscapeAttr(m.note) + '</small></div>';
                    }
                });
                $('#statusNotes').html(notes);
            }).fail(function () {
                $('#serverStatus').html('<span class="badge bg-secondary">Status unavailable</span>');
            });
        }

        function SourceOptions(selected) {
            var html = '<option value="">-- select a video source --</option>';
            var found = false;
            videoSources.forEach(function (s) {
                if (s.node === selected) found = true;
                html += '<option value="' + EscapeAttr(s.node) + '"' +
                    (s.node === selected ? ' selected' : '') + '>' +
                    EscapeAttr(s.name + ' (' + s.node + ')') + '</option>';
            });
            // A source configured elsewhere (an fppd stream slot, say) is still
            // valid even though it is not in the input-sources list.
            if (selected && !found) {
                html += '<option value="' + EscapeAttr(selected) + '" selected>' +
                    EscapeAttr(selected) + '</option>';
            }
            return html;
        }

        function RenderMounts() {
            var host = window.location.hostname;
            var port = parseInt($('#rtspPort').val(), 10) || 8554;
            var html = '';

            if (rtspConfig.mounts.length === 0) {
                html = '<div class="alert alert-light border">No RTSP outputs configured yet.</div>';
                $('#mountList').html(html);
                return;
            }

            rtspConfig.mounts.forEach(function (m, i) {
                var url = 'rtsp://' + host + ':' + port + (m.mountPoint || '/stream' + m.id);
                html += '<div class="card mb-3' + (m.enabled ? '' : ' opacity-50') + '">';
                html += '<div class="card-header d-flex align-items-center gap-2 flex-wrap">';
                html += '<input type="checkbox" class="form-check-input" ' + (m.enabled ? 'checked' : '') +
                    ' onchange="UpdateMount(' + i + ',\'enabled\',this.checked)" title="Enable this output">';
                html += '<input type="text" class="form-control form-control-sm w-auto" value="' +
                    EscapeAttr(m.name || '') + '" onchange="UpdateMount(' + i + ',\'name\',this.value)">';
                html += '<code class="ms-auto">' + EscapeAttr(url) + '</code>';
                html += '<button class="btn btn-outline-danger btn-sm" onclick="RemoveMount(' + i + ')">' +
                    '<i class="fas fa-trash"></i></button>';
                html += '</div><div class="card-body">';

                html += '<div class="row align-items-center g-2 mb-2">';
                html += '<div class="col-auto"><label class="form-label mb-0">Video source:</label></div>';
                html += '<div class="col-auto"><select class="form-select form-select-sm w-auto" onchange="UpdateMount(' + i +
                    ',\'sourceNode\',this.value)">' + SourceOptions(m.sourceNode) + '</select></div>';
                html += '<div class="col-auto"><label class="form-label mb-0">Mount path:</label></div>';
                html += '<div class="col-auto"><input type="text" class="form-control form-control-sm" ' +
                    'value="' + EscapeAttr(m.mountPoint || '') + '" onchange="UpdateMount(' + i +
                    ',\'mountPoint\',this.value)" placeholder="/live"></div>';
                html += '</div>';

                html += '<div class="row align-items-center g-2 mb-2">';
                html += '<div class="col-auto"><label class="form-label mb-0">Size:</label></div>';
                html += '<div class="col-auto"><input type="number" class="form-control form-control-sm" style="width:90px" value="' +
                    (m.width || 1280) + '" onchange="UpdateMount(' + i + ',\'width\',parseInt(this.value))"></div>';
                html += '<div class="col-auto">&times;</div>';
                html += '<div class="col-auto"><input type="number" class="form-control form-control-sm" style="width:90px" value="' +
                    (m.height || 720) + '" onchange="UpdateMount(' + i + ',\'height\',parseInt(this.value))"></div>';
                html += '<div class="col-auto"><label class="form-label mb-0">FPS:</label></div>';
                html += '<div class="col-auto"><input type="number" class="form-control form-control-sm" style="width:75px" value="' +
                    (m.framerate || 30) + '" onchange="UpdateMount(' + i + ',\'framerate\',parseInt(this.value))"></div>';
                html += '</div>';

                html += '<div class="row align-items-center g-2 mb-2">';
                html += '<div class="col-auto"><label class="form-label mb-0">Encoding:</label></div>';
                html += '<div class="col-auto"><select class="form-select form-select-sm w-auto" onchange="UpdateMount(' + i +
                    ',\'videoEncoding\',this.value)">';
                [['h264', 'H.264'], ['h265', 'H.265 (HEVC)'], ['mjpeg', 'Motion JPEG']].forEach(function (e) {
                    html += '<option value="' + e[0] + '"' + ((m.videoEncoding || 'h264') === e[0] ? ' selected' : '') +
                        '>' + e[1] + '</option>';
                });
                html += '</select></div>';
                html += '<div class="col-auto"><label class="form-label mb-0">Bitrate (kbps):</label></div>';
                html += '<div class="col-auto"><input type="number" class="form-control form-control-sm" style="width:100px" value="' +
                    (m.videoBitrate || 4000) + '" onchange="UpdateMount(' + i + ',\'videoBitrate\',parseInt(this.value))"></div>';
                html += '</div>';

                html += '<div class="row align-items-center g-2">';
                html += '<div class="col-auto"><label class="form-label mb-0">Audio:</label></div>';
                html += '<div class="col-auto">';
                html += '<input type="checkbox" class="form-check-input" id="rtspAudio_' + i + '" ' +
                    (m.audioEnabled ? 'checked' : '') + ' onchange="UpdateMount(' + i + ',\'audioEnabled\',this.checked)"> ';
                html += '<label class="form-check-label fw-normal" for="rtspAudio_' + i + '">Include audio in the stream</label>';
                html += '</div>';
                if (m.audioEnabled) {
                    html += '<div class="col-auto"><label class="form-label mb-0">Bitrate (bps):</label></div>';
                    html += '<div class="col-auto"><input type="number" class="form-control form-control-sm" style="width:110px" value="' +
                        (m.audioBitrate || 128000) + '" onchange="UpdateMount(' + i + ',\'audioBitrate\',parseInt(this.value))"></div>';
                    html += '<div class="col-auto"><span class="badge bg-info" title="PipeWire audio node">' +
                        EscapeAttr(m.audioNodeName || ('fpp_rtsp_audio_' + m.id)) + '</span></div>';
                }
                html += '</div>';
                if (m.audioEnabled) {
                    html += '<div class="row mt-1"><div class="col text-muted"><small>' +
                        'Route audio to this stream by adding the node above to an Audio Output Group. ' +
                        'Until something feeds it, the stream serves video only.</small></div></div>';
                }

                html += '</div></div>';
            });
            $('#mountList').html(html);
        }

        function SetSaveResult(kind, html) {
            $('#saveResult').html('<div class="alert alert-' + kind + ' py-2 mb-0">' + html + '</div>');
        }

        function UpdateMount(i, field, value) {
            rtspConfig.mounts[i][field] = value;
            RenderMounts();
        }

        function AddMount() {
            var id = nextMountId++;
            rtspConfig.mounts.push({
                id: id, name: 'RTSP Output ' + id, enabled: true,
                mountPoint: '/stream' + id, sourceNode: '',
                width: 1280, height: 720, framerate: 30,
                videoEncoding: 'h264', videoBitrate: 4000,
                audioEnabled: false, audioBitrate: 128000,
                audioNodeName: 'fpp_rtsp_audio_' + id
            });
            RenderMounts();
        }

        function RemoveMount(i) {
            rtspConfig.mounts.splice(i, 1);
            RenderMounts();
        }

        function SaveAndApply() {
            rtspConfig.enabled = $('#rtspEnabled').is(':checked');
            rtspConfig.port = parseInt($('#rtspPort').val(), 10) || 8554;

            var missing = rtspConfig.mounts.filter(function (m) {
                return m.enabled && !m.sourceNode;
            });
            if (missing.length > 0) {
                SetSaveResult('danger', '<i class="fas fa-times"></i> ' +
                    'Each enabled RTSP output needs a video source.');
                return;
            }

            var $btn = $('#saveBtn');
            $btn.prop('disabled', true);
            SetSaveResult('info', '<i class="fas fa-spinner fa-spin"></i> Saving and applying...');

            $.ajax({
                url: 'api/pipewire/rtspoutputs',
                type: 'POST',
                contentType: 'application/json',
                data: JSON.stringify(rtspConfig)
            }).done(function () {
                // fppd re-reads the config file on this, the same way the
                // video input and output pages apply their own changes.
                $.post('api/pipewire/rtspoutputs/apply').done(function () {
                    LoadConfig().always(function () {
                        RefreshStatus();
                        SetSaveResult('success', '<i class="fas fa-check"></i> Saved and applied.');
                    });
                }).fail(function () {
                    // The config is on disk either way, so say so: the streams
                    // will come up on the next fppd start even if this failed.
                    LoadConfig().always(RefreshStatus);
                    SetSaveResult('warning', '<i class="fas fa-exclamation-triangle"></i> ' +
                        'Saved, but fppd did not accept the reload. It will pick the ' +
                        'configuration up when it next starts.');
                });
            }).fail(function (xhr) {
                SetSaveResult('danger', '<i class="fas fa-times"></i> Failed to save: ' +
                    EscapeAttr((xhr && xhr.responseJSON && xhr.responseJSON.message) || 'unknown error'));
            }).always(function () {
                $btn.prop('disabled', false);
            });
        }

        $(document).on('change', '#rtspPort', RenderMounts);
    </script>
</body>

</html>
