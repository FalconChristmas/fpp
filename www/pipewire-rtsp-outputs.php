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
                        Serves video, with its audio, as an RTSP stream a player opens by URL &mdash; point VLC,
                        ffplay, OBS or a video wall at it and it negotiates the rest. Unlike the RTP outputs, no SDP
                        file has to be copied to the receiver.
                    </p>

                    <p class="text-muted">
                        This page is the server: whether it runs, and on which port. The streams themselves are
                        <strong>Video Output Group members</strong> &mdash; add a <em>Network (RTSP)</em> member to a
                        group and it serves whatever that group is showing, so one source can go to an HDMI display
                        and out over RTSP at the same time without being configured twice.
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
                        <div class="col-auto">
                            <button class="btn btn-success btn-sm" id="saveBtn" onclick="SaveAndApply()">
                                <i class="fas fa-save"></i> Save &amp; Apply</button>
                        </div>
                    </div>

                    <div class="alert alert-secondary py-2">
                        <small><i class="fas fa-info-circle"></i>
                            The server listens on every interface and does not ask for a password, the same as FPP's
                            other network media outputs. Anyone who can reach this player on the network can watch
                            these streams.</small>
                    </div>

                    <div id="saveResult" class="mb-3"></div>

                    <h5>Streams</h5>
                    <div id="statusNotes"></div>
                    <div id="mountList"></div>

                    <p class="mt-3">
                        <a href="pipewire-video.php" class="btn btn-outline-primary btn-sm">
                            <i class="fas fa-tv"></i> Configure Video Output Groups</a>
                    </p>

                <?php } ?>

                <?php if (!$modalMode) { ?>
                </div>
            </div>
            <?php include 'common/footer.inc'; ?>
        </div>
    <?php } ?>

    <script>
        var rtspConfig = { enabled: false, port: 8554 };

        function EscapeAttr(s) {
            return String(s).replace(/&/g, '&amp;').replace(/"/g, '&quot;')
                .replace(/'/g, '&#39;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
        }

        $(document).ready(function () {
            LoadConfig().always(RefreshStatus);
        });

        function LoadConfig() {
            return $.getJSON('api/pipewire/rtspoutputs').done(function (data) {
                rtspConfig = data || { enabled: false, port: 8554 };
                $('#rtspEnabled').prop('checked', !!rtspConfig.enabled);
                $('#rtspPort').val(rtspConfig.port || 8554);
            });
        }

        // fppd is the only thing that knows what is actually being served: the
        // mounts come from the group config, not from this page's settings.
        function RefreshStatus() {
            $.getJSON('api/pipewire/rtspoutputs/status').done(function (st) {
                var html;
                if (st.unavailable && st.reason === 'unsupported') {
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
                RenderMounts(st);
            }).fail(function () {
                $('#serverStatus').html('<span class="badge bg-secondary">Status unavailable</span>');
                RenderMounts(null);
            });
        }

        function RenderMounts(st) {
            var mounts = (st && st.mounts) ? st.mounts : [];
            if (mounts.length === 0) {
                $('#statusNotes').html('');
                $('#mountList').html('<div class="alert alert-light border mb-0">' +
                    'No RTSP streams yet. Add a <strong>Network (RTSP)</strong> member to a Video Output Group.' +
                    '</div>');
                return;
            }

            var host = window.location.hostname;
            var port = (st && st.port) ? st.port : ($('#rtspPort').val() || 8554);
            var notes = '';
            var html = '<div class="table-responsive"><table class="table table-sm table-hover align-middle">';
            html += '<thead><tr><th>Stream</th><th>URL</th><th>Audio</th></tr></thead><tbody>';

            mounts.forEach(function (m) {
                var url = 'rtsp://' + host + ':' + port + (m.mountPoint || '');
                html += '<tr>';
                html += '<td>' + EscapeAttr(m.name || '') + '</td>';
                html += '<td><code>' + EscapeAttr(url) + '</code></td>';
                if (!m.audioEnabled) {
                    html += '<td><span class="text-muted">Video only</span></td>';
                } else if (m.audioWaitingForSource) {
                    html += '<td><span class="badge bg-warning text-dark">Waiting for source</span></td>';
                } else {
                    html += '<td><span class="badge bg-success">Included</span></td>';
                }
                html += '</tr>';

                if (m.note) {
                    notes += '<div class="alert alert-info py-2 mb-2"><small><i class="fas fa-info-circle"></i> ' +
                        EscapeAttr(m.name) + ': ' + EscapeAttr(m.note) + '</small></div>';
                }
            });
            html += '</tbody></table></div>';
            $('#statusNotes').html(notes);
            $('#mountList').html(html);
        }

        function SetSaveResult(kind, html) {
            $('#saveResult').html('<div class="alert alert-' + kind + ' py-2 mb-0">' + html + '</div>');
        }

        function SaveAndApply() {
            rtspConfig.enabled = $('#rtspEnabled').is(':checked');
            rtspConfig.port = parseInt($('#rtspPort').val(), 10) || 8554;

            var $btn = $('#saveBtn');
            $btn.prop('disabled', true);
            SetSaveResult('info', '<i class="fas fa-spinner fa-spin"></i> Saving and applying...');

            $.ajax({
                url: 'api/pipewire/rtspoutputs',
                type: 'POST',
                contentType: 'application/json',
                data: JSON.stringify(rtspConfig)
            }).done(function () {
                $.post('api/pipewire/rtspoutputs/apply').done(function () {
                    LoadConfig().always(function () {
                        RefreshStatus();
                        SetSaveResult('success', '<i class="fas fa-check"></i> Saved and applied.');
                    });
                }).fail(function () {
                    // The config is on disk either way, so say so: the server
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
    </script>
</body>

</html>
