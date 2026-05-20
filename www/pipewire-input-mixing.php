<!DOCTYPE html>
<html lang="en">

<head>
    <?php
    include 'common/htmlMeta.inc';
    require_once "common.php";
    require_once 'config.php';
    include 'common/menuHead.inc';
    ?>

    <title><? echo $pageTitle; ?> - Input Mixing</title>

    <?php $modalMode = isset($_GET['modal']) && $_GET['modal'] == '1'; ?>

    <style>
        .ig-card {
            border: 1px solid var(--bs-border-color, #dee2e6);
            border-radius: 8px;
            margin-bottom: 1.5rem;
            background: var(--bs-body-bg, #fff);
        }

        .ig-card.disabled-group {
            opacity: 0.6;
        }

        .ig-header {
            display: flex;
            align-items: center;
            gap: 0.75rem;
            padding: 0.75rem 1rem;
            border-bottom: 1px solid var(--bs-border-color, #dee2e6);
            background: var(--bs-tertiary-bg, #f8f9fa);
            border-radius: 8px 8px 0 0;
            flex-wrap: wrap;
        }

        .ig-header .ig-name-input {
            font-size: 1.1rem;
            font-weight: 600;
            border: 1px solid var(--bs-border-color, #ccc);
            background: var(--bs-body-bg, #fff);
            color: var(--bs-body-color, #212529);
            padding: 0.25rem 0.5rem;
            border-radius: 3px;
            min-width: 250px;
        }

        .ig-header .ig-name-input:focus {
            border-color: var(--bs-primary, #007cba);
            background: var(--bs-body-bg, #fff);
            color: var(--bs-body-color, #212529);
            outline: none;
            box-shadow: 0 0 3px rgba(0, 124, 186, 0.3);
        }

        .ig-body {
            padding: 1rem;
        }

        .ig-settings {
            display: flex;
            gap: 1rem;
            flex-wrap: wrap;
            margin-bottom: 1rem;
            align-items: center;
        }

        .ig-settings label {
            font-weight: 500;
            margin-right: 0.25rem;
        }

        .member-table {
            width: 100%;
            border-collapse: collapse;
        }

        .member-table th {
            background: var(--bs-tertiary-bg, #f8f9fa);
            padding: 0.5rem 0.75rem;
            text-align: left;
            font-weight: 600;
            font-size: 0.9rem;
        }

        .member-table td {
            padding: 0.5rem 0.75rem;
            vertical-align: middle;
            border-bottom: 1px solid var(--bs-border-color-translucent, rgba(0, 0, 0, 0.1));
        }

        .member-table tr:last-child td {
            border-bottom: none;
        }

        .btn-remove-member {
            color: #dc3545;
            background: none;
            border: none;
            cursor: pointer;
            font-size: 1.1rem;
            padding: 0.25rem;
        }

        .btn-remove-member:hover {
            color: #a71d2a;
        }

        .toolbar {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 1rem;
            flex-wrap: wrap;
            gap: 0.5rem;
        }

        .toolbar-left,
        .toolbar-right {
            display: flex;
            gap: 0.5rem;
            align-items: center;
            flex-wrap: wrap;
        }

        .no-groups-msg {
            text-align: center;
            padding: 3rem;
            color: var(--bs-secondary-color, #6c757d);
        }

        .no-groups-msg i {
            font-size: 3rem;
            margin-bottom: 1rem;
            opacity: 0.5;
        }

        .status-indicator {
            display: inline-block;
            width: 10px;
            height: 10px;
            border-radius: 50%;
            margin-right: 0.25rem;
        }

        .status-ok {
            background: #28a745;
        }

        .status-error {
            background: #dc3545;
        }

        .status-unknown {
            background: #6c757d;
        }

        .volume-slider {
            width: 80px;
            display: inline-block;
            vertical-align: middle;
        }

        .volume-value {
            display: inline-block;
            width: 35px;
            text-align: right;
            font-size: 0.85rem;
        }

        .mute-btn {
            border: none;
            background: none;
            cursor: pointer;
            font-size: 1rem;
            padding: 0.25rem 0.5rem;
            border-radius: 3px;
        }

        .mute-btn.muted {
            color: #dc3545;
        }

        .mute-btn:not(.muted) {
            color: #28a745;
        }

        .channel-mapping {
            display: inline-flex;
            align-items: center;
            gap: 0.35rem;
            margin-top: 0.35rem;
            font-size: 0.82rem;
        }

        .channel-mapping label {
            font-weight: 500;
            font-size: 0.82rem;
            color: var(--bs-secondary-color, #6c757d);
            margin: 0;
        }

        .source-state-badge {
            display: inline-block;
            font-size: 0.7rem;
            padding: 0.1rem 0.4rem;
            border-radius: 3px;
            margin-left: 0.35rem;
            vertical-align: middle;
        }

        .source-state-badge.state-running {
            background: #d4edda;
            color: #155724;
        }

        .source-state-badge.state-idle {
            background: #fff3cd;
            color: #856404;
        }

        .source-state-badge.state-error,
        .source-state-badge.state-disconnected {
            background: #f8d7da;
            color: #721c24;
        }

        .source-info {
            font-size: 0.78rem;
            color: var(--bs-secondary-color, #6c757d);
            margin-top: 0.15rem;
        }

        .output-routing {
            margin-top: 0.75rem;
            padding: 0.75rem;
            border: 1px solid var(--bs-border-color-translucent, rgba(0, 0, 0, 0.1));
            border-radius: 6px;
            background: var(--bs-tertiary-bg, #f8f9fa);
        }

        .output-routing label {
            font-weight: 500;
            margin-bottom: 0.5rem;
            display: block;
        }

        .output-routing .form-check {
            margin-right: 1rem;
            display: inline-block;
        }

        .alsa-warning {
            text-align: center;
            padding: 3rem;
        }

        .btn-group-action {
            font-weight: 500;
        }

        <?php if ($modalMode) { ?>
            body {
                overflow-y: auto !important;
            }

            .modal {
                z-index: 99999 !important;
            }

            .modal-backdrop {
                z-index: 99998 !important;
            }

        <?php } ?>
    </style>
</head>

<body<?php if ($modalMode)
    echo ' style="margin:0;padding:1rem;"'; ?>>
    <?php if (!$modalMode) { ?>
        <div id="bodyWrapper">
            <?php
            $activeParentMenuItem = 'status';
            include 'menu.inc';
            ?>
            <div class="mainContainer">
                <h1 class="title">Input Mixing</h1>
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
                        <i class="fas fa-exclamation-triangle fa-2x" style="color: var(--bs-warning, #ffc107);"></i>
                        <h4>Advanced PipeWire Required</h4>
                        <p>Input Mixing requires the Advanced PipeWire backend.<br>
                            Currently using: <strong><?= htmlspecialchars($mbDisplay) ?></strong></p>
                        <p>Change to PipeWire (Advanced) in <a href="settings.php?tab=Audio%2FVideo">FPP Settings &rarr;
                                Audio/Video</a>,
                            then return here to configure input mixing.</p>
                    </div>
                <?php } else { ?>

                    <div id="pipewireStatus" class="toolbar">
                        <div class="toolbar-left">
                            <span id="pwStatus"><span class="status-indicator status-unknown"></span> Checking PipeWire
                                status...</span>
                        </div>
                        <div class="toolbar-right">
                            <button class="buttons btn-outline-secondary btn-sm" onclick="RefreshSources()"
                                title="Refresh capture device list">
                                <i class="fas fa-redo"></i> Refresh Sources
                            </button>
                            <button class="buttons btn-outline-success btn-group-action" onclick="AddInputGroup()">
                                <i class="fas fa-plus"></i> Add Input Group
                            </button>
                            <button class="buttons" onclick="SaveInputGroups()">
                                <i class="fas fa-save"></i> Save
                            </button>
                            <button class="buttons btn-outline-primary" onclick="ApplyInputGroups()">
                                <i class="fas fa-sync"></i> Save &amp; Apply
                            </button>
                            <a class="buttons btn-outline-secondary" href="#" title="Open Routing Matrix"
                                onclick="OpenRoutingMatrix(); return false;">
                                <i class="fas fa-th"></i> Routing Matrix
                            </a>
                        </div>
                    </div>

                    <div id="inputGroupsContainer">
                        <div class="no-groups-msg" id="noGroupsMsg">
                            <i class="fas fa-sliders-h"></i>
                            <h4>No Input Groups Configured</h4>
                            <p>Create input groups (mix buses) to combine multiple audio sources.<br>
                                Route fppd streams, ALSA line-in, and AES67 receives into mix buses,<br>
                                then send them to your Audio Output Groups.</p>
                            <button class="buttons btn-outline-success" onclick="AddInputGroup()">
                                <i class="fas fa-plus"></i> Create First Input Group
                            </button>
                        </div>
                    </div>

                    <div id="bottomToolbar" class="toolbar" style="display:none; margin-top:1rem;">
                        <div class="toolbar-left"></div>
                        <div class="toolbar-right">
                            <button class="buttons" onclick="SaveInputGroups()">
                                <i class="fas fa-save"></i> Save
                            </button>
                            <button class="buttons btn-outline-primary" onclick="ApplyInputGroups()">
                                <i class="fas fa-sync"></i> Save &amp; Apply
                            </button>
                        </div>
                    </div>

                <?php } ?>

                <?php if (!$modalMode) { ?>
                </div>
            </div>
        </div>

        <?php include 'common/footer.inc'; ?>
    <?php } ?>

    <script>
        // ─── State ─────────────────────────────────────────────────────
        var inputGroups = { inputGroups: [] };
        var availableSources = [];
        var availableOutputGroups = [];
        var availableCards = [];
        var availableAES67Instances = [];
        var availableOpusRTPInstances = [];
        var availablePWSources = [];  // Audio-enabled video input sources

        // ─── Init ──────────────────────────────────────────────────────
        $(document).ready(function () {
            CheckPipeWireStatus();
            LoadAll();

            // Auto-refresh source states every 10s for hot-plug detection
            setInterval(function () {
                RefreshSources(true);
            }, 10000);
        });

        function CheckPipeWireStatus() {
            $.get('/api/pipewire/audio/sinks', function (data) {
                var hasSinks = Array.isArray(data) && data.length > 0;
                $('#pwStatus').html(
                    '<span class="status-indicator ' + (hasSinks ? 'status-ok' : 'status-error') + '"></span> ' +
                    (hasSinks ? 'PipeWire running (' + data.length + ' sinks)' : 'PipeWire not detected')
                );
            }).fail(function () {
                $('#pwStatus').html(
                    '<span class="status-indicator status-error"></span> Cannot reach PipeWire API'
                );
            });
        }

        function LoadAll() {
            // Load all data in parallel
            var p1 = $.get('/api/pipewire/audio/input-groups');
            var p2 = $.get('/api/pipewire/audio/sources');
            var p3 = $.get('/api/pipewire/audio/groups');
            var p4 = $.get('/api/pipewire/aes67/instances');
            var p5 = $.get('/api/pipewire/video/input-sources');
            var p6 = $.get('/api/pipewire/opusrtp/instances');

            $.when(p1, p2, p3, p4, p5, p6).done(function (r1, r2, r3, r4, r5, r6) {
                inputGroups = r1[0];
                if (!inputGroups || !inputGroups.inputGroups) {
                    inputGroups = { inputGroups: [] };
                }
                availableSources = Array.isArray(r2[0]) ? r2[0] : [];
                var ogData = r3[0];
                availableOutputGroups = (ogData && ogData.groups) ? ogData.groups : [];
                var aesData = r4[0];
                availableAES67Instances = (aesData && aesData.instances) ? aesData.instances : [];
                var opusData = r6[0];
                availableOpusRTPInstances = (opusData && opusData.instances) ? opusData.instances : [];
                // Extract audio-enabled video input sources
                var viData = r5[0];
                availablePWSources = [];
                if (viData && viData.videoInputSources) {
                    viData.videoInputSources.forEach(function (src) {
                        if (src.audioEnabled && src.audioPipeWireNodeName) {
                            availablePWSources.push({
                                nodeName: src.audioPipeWireNodeName,
                                name: src.name || 'Source ' + src.id,
                            });
                        }
                    });
                }

                RenderAll();
            }).fail(function () {
                // Load what we can
                $.get('/api/pipewire/audio/input-groups', function (data) {
                    inputGroups = data || { inputGroups: [] };
                    RenderAll();
                });
            });
        }

        // Refresh capture device states (for hot-plug detection)
        function RefreshSources(silent) {
            $.get('/api/pipewire/audio/sources', function (data) {
                var newSources = Array.isArray(data) ? data : [];
                var changed = JSON.stringify(newSources) !== JSON.stringify(availableSources);
                availableSources = newSources;
                if (changed) {
                    RenderAll();
                    if (!silent) {
                        $.jGrowl('Source list updated.', { theme: 'success' });
                    }
                } else if (!silent) {
                    $.jGrowl('Sources up to date.', { theme: 'info' });
                }
            }).fail(function () {
                if (!silent) {
                    $.jGrowl('Failed to refresh sources.', { theme: 'danger' });
                }
            });
        }

        // ─── Render ────────────────────────────────────────────────────
        function RenderAll() {
            var container = $('#inputGroupsContainer');
            container.empty();

            if (!inputGroups.inputGroups || inputGroups.inputGroups.length === 0) {
                container.html(
                    '<div class="no-groups-msg" id="noGroupsMsg">' +
                    '<i class="fas fa-sliders-h"></i>' +
                    '<h4>No Input Groups Configured</h4>' +
                    '<p>Create input groups (mix buses) to combine multiple audio sources.<br>' +
                    'Route fppd streams, ALSA line-in, and AES67 receives into mix buses,<br>' +
                    'then send them to your Audio Output Groups.</p>' +
                    '<button class="buttons btn-outline-success" onclick="AddInputGroup()">' +
                    '<i class="fas fa-plus"></i> Create First Input Group</button>' +
                    '</div>'
                );
                $('#bottomToolbar').hide();
                return;
            }

            inputGroups.inputGroups.forEach(function (ig, idx) {
                container.append(RenderInputGroup(ig, idx));
            });

            $('#bottomToolbar').show();
        }

        function RenderInputGroup(ig, idx) {
            var enabled = ig.enabled !== false;
            var html = '<div class="ig-card' + (!enabled ? ' disabled-group' : '') + '" data-idx="' + idx + '">';

            // Header
            html += '<div class="ig-header">';
            html += '<input type="text" class="ig-name-input" value="' + EscapeAttr(ig.name || 'Input Group') + '" ' +
                'onchange="UpdateGroupField(' + idx + ', \'name\', this.value)">';
            html += '<label style="margin:0;cursor:pointer;"><input type="checkbox" ' +
                (enabled ? 'checked' : '') + ' onchange="UpdateGroupField(' + idx + ', \'enabled\', this.checked)"> Enabled</label>';
            html += '<select class="form-select form-select-sm" style="width:auto;" onchange="UpdateGroupField(' + idx + ', \'channels\', parseInt(this.value))">';
            [2, 1, 4, 6, 8].forEach(function (ch) {
                html += '<option value="' + ch + '"' + (ig.channels == ch ? ' selected' : '') + '>' + ch + ' ch</option>';
            });
            html += '</select>';
            html += '<div style="margin-left:auto;display:flex;gap:0.5rem;">';
            html += '<button class="buttons btn-outline-success btn-sm" onclick="AddMember(' + idx + ')">' +
                '<i class="fas fa-plus"></i> Add Source</button>';
            html += '<button class="buttons btn-outline-danger btn-sm" onclick="RemoveGroup(' + idx + ')">' +
                '<i class="fas fa-trash"></i></button>';
            html += '</div>';
            html += '</div>';

            // Body
            html += '<div class="ig-body">';

            // Members table
            var members = ig.members || [];
            if (members.length > 0) {
                html += '<table class="member-table">';
                html += '<thead><tr>';
                html += '<th>Type</th><th>Source</th><th>Name</th><th>Volume</th><th>Mute</th><th></th>';
                html += '</tr></thead><tbody>';

                members.forEach(function (mbr, mi) {
                    html += RenderMember(idx, mi, mbr);
                });

                html += '</tbody></table>';
            } else {
                html += '<div style="text-align:center;padding:1rem;color:#6c757d;">' +
                    '<i class="fas fa-info-circle"></i> No sources added. Click "Add Source" to add audio inputs.' +
                    '</div>';
            }

            // Output routing
            html += '<div class="output-routing">';
            html += '<label><i class="fas fa-arrow-right"></i> Route to Output Groups: ';
            html += '<a href="#" onclick="OpenRoutingMatrix(); return false;" style="font-size:0.8rem;font-weight:normal;" title="Open Routing Matrix for per-path volume control">';
            html += '<i class="fas fa-th"></i> Matrix View</a></label>';
            html += '<div>';
            var outputs = ig.outputs || [];
            var routing = ig.routing || {};
            if (availableOutputGroups.length > 0) {
                availableOutputGroups.forEach(function (og) {
                    if (!og.enabled) return;
                    var checked = outputs.indexOf(og.id) !== -1;
                    var pathVol = routing[String(og.id)] ? (routing[String(og.id)].volume || 100) : 100;
                    var pathMute = routing[String(og.id)] ? (routing[String(og.id)].mute || false) : false;
                    html += '<div class="form-check" style="display:inline-flex;align-items:center;gap:4px;">';
                    html += '<input class="form-check-input" type="checkbox" ' +
                        (checked ? 'checked' : '') +
                        ' onchange="ToggleOutput(' + idx + ', ' + og.id + ', this.checked)">';
                    html += '<label class="form-check-label">' + EscapeHtml(og.name || 'Group ' + og.id) + '</label>';
                    if (checked && pathVol < 100) {
                        html += ' <small class="text-muted">(' + pathVol + '%)</small>';
                    }
                    if (pathMute) {
                        html += ' <small class="text-danger"><i class="fas fa-volume-mute"></i></small>';
                    }
                    html += '</div>';
                });
            } else {
                html += '<span style="color:#6c757d;">No output groups configured. ' +
                    '<a href="pipewire-audio.php">Create output groups first.</a></span>';
            }
            html += '</div></div>';

            html += '</div>'; // ig-body
            html += '</div>'; // ig-card

            return html;
        }

        function RenderMember(groupIdx, memberIdx, mbr) {
            var type = mbr.type || 'fppd_stream';
            // Look up the group's channel count for channel mapping
            var groupChannels = inputGroups.inputGroups[groupIdx].channels || 2;
            var html = '<tr>';

            // Type selector
            html += '<td><select class="form-select form-select-sm" style="width:auto;" ' +
                'onchange="UpdateMemberField(' + groupIdx + ',' + memberIdx + ',\'type\',this.value); RenderAll();">';
            html += '<option value="fppd_stream"' + (type === 'fppd_stream' ? ' selected' : '') + '>fppd Stream</option>';
            html += '<option value="capture"' + (type === 'capture' ? ' selected' : '') + '>ALSA Capture</option>';
            html += '<option value="pw_source"' + (type === 'pw_source' ? ' selected' : '') + '>PipeWire Source</option>';
            html += '<option value="aes67_receive"' + (type === 'aes67_receive' ? ' selected' : '') + '>AES67 Receive</option>';
            html += '<option value="opus_rtp_receive"' + (type === 'opus_rtp_receive' ? ' selected' : '') + '>Opus RTP Receive</option>';
            html += '</select></td>';

            // Source selector
            html += '<td>';
            if (type === 'fppd_stream') {
                var curSrc = mbr.sourceId || 'fppd_stream_1';
                html += '<select class="form-select form-select-sm" style="width:auto;" ' +
                    'onchange="UpdateMemberField(' + groupIdx + ',' + memberIdx + ',\'sourceId\',this.value)">';
                for (var si = 1; si <= 5; si++) {
                    var val = 'fppd_stream_' + si;
                    var lbl = 'FPP Media Stream ' + si + (si === 1 ? ' (default)' : '');
                    html += '<option value="' + val + '"' + (curSrc === val ? ' selected' : '') + '>' + lbl + '</option>';
                }
                html += '</select>';
            } else if (type === 'capture') {
                html += '<select class="form-select form-select-sm" style="width:auto;" ' +
                    'onchange="SelectCaptureDevice(' + groupIdx + ',' + memberIdx + ',this.value); RenderAll();">';
                html += '<option value="">-- Select capture device --</option>';
                availableSources.forEach(function (src) {
                    var sel = (mbr.cardId === src.cardId) ? ' selected' : '';
                    var stateClass = src.state === 'running' ? 'state-running' :
                        (src.state === 'idle' ? 'state-idle' : 'state-disconnected');
                    html += '<option value="' + EscapeAttr(src.cardId) + '"' + sel + '>' +
                        EscapeHtml(src.description || src.name) +
                        ' (' + src.channels + 'ch)</option>';
                });
                html += '</select>';

                // Device state badge
                if (mbr.cardId) {
                    var matchedSrc = availableSources.find(function (s) { return s.cardId === mbr.cardId; });
                    if (matchedSrc) {
                        var stateLabel = matchedSrc.state || 'unknown';
                        var stateClass = stateLabel === 'running' ? 'state-running' :
                            (stateLabel === 'idle' ? 'state-idle' : 'state-disconnected');
                        html += '<span class="source-state-badge ' + stateClass + '">' + stateLabel + '</span>';
                        html += '<div class="source-info">' + matchedSrc.channels + 'ch, ' +
                            matchedSrc.sampleRate + ' Hz</div>';
                    } else {
                        html += '<span class="source-state-badge state-disconnected">disconnected</span>';
                    }

                    // Channel mapping selector (when source channels != group channels)
                    var srcChannels = matchedSrc ? matchedSrc.channels : 0;
                    if (srcChannels > 0 && srcChannels !== groupChannels) {
                        html += RenderChannelMapping(groupIdx, memberIdx, mbr, srcChannels, groupChannels);
                    }
                }
            } else if (type === 'pw_source') {
                // PipeWire Audio/Source nodes from video input streams
                if (availablePWSources.length > 0) {
                    html += '<select class="form-select form-select-sm" style="width:auto;" ' +
                        'onchange="SelectPWSource(' + groupIdx + ',' + memberIdx + ',this.value); RenderAll();">';
                    html += '<option value="">-- Select PipeWire source --</option>';
                    availablePWSources.forEach(function (src) {
                        var sel = (mbr.nodeName === src.nodeName) ? ' selected' : '';
                        html += '<option value="' + EscapeAttr(src.nodeName) + '"' + sel + '>' +
                            EscapeHtml(src.name) + ' (' + EscapeHtml(src.nodeName) + ')</option>';
                    });
                    html += '</select>';
                } else {
                    html += '<span style="color:#6c757d;font-size:0.85rem;">' +
                        'No audio-enabled video sources. <a href="pipewire-video-inputs.php">Configure Video Inputs</a></span>';
                }
            } else if (type === 'aes67_receive') {
                // Dropdown populated from AES67 receive instances
                var recvInstances = availableAES67Instances.filter(function (inst) {
                    return inst.mode === 'receive' && inst.enabled;
                });
                if (recvInstances.length > 0) {
                    html += '<select class="form-select form-select-sm" style="width:auto;" ' +
                        'onchange="UpdateMemberField(' + groupIdx + ',' + memberIdx + ',\'instanceId\',this.value)">';
                    html += '<option value="">-- Select AES67 receive --</option>';
                    recvInstances.forEach(function (inst) {
                        var sel = (mbr.instanceId == inst.id) ? ' selected' : '';
                        html += '<option value="' + inst.id + '"' + sel + '>' +
                            EscapeHtml(inst.name || 'AES67 Receive ' + inst.id) +
                            ' (' + inst.channels + 'ch)</option>';
                    });
                    html += '</select>';
                } else {
                    html += '<span style="color:#6c757d;font-size:0.85rem;">' +
                        'No AES67 receive instances. <a href="aes67-config.php">Configure AES67</a></span>';
                }
            } else if (type === 'opus_rtp_receive') {
                var opusRecvInstances = availableOpusRTPInstances.filter(function (inst) {
                    return inst.mode === 'receive' && inst.enabled;
                });
                if (opusRecvInstances.length > 0) {
                    html += '<select class="form-select form-select-sm" style="width:auto;" ' +
                        'onchange="UpdateMemberField(' + groupIdx + ',' + memberIdx + ',\'instanceId\',this.value)">';
                    html += '<option value="">-- Select Opus RTP receive --</option>';
                    opusRecvInstances.forEach(function (inst) {
                        var sel = (mbr.instanceId == inst.id) ? ' selected' : '';
                        html += '<option value="' + inst.id + '"' + sel + '>' +
                            EscapeHtml(inst.name || 'Opus RTP Receive ' + inst.id) +
                            ' (' + inst.channels + 'ch)</option>';
                    });
                    html += '</select>';
                } else {
                    html += '<span style="color:#6c757d;font-size:0.85rem;">' +
                        'No Opus RTP receive instances. <a href="opus-rtp-config.php">Configure Opus RTP</a></span>';
                }
            }
            html += '</td>';

            // Name
            html += '<td><input type="text" class="form-control form-control-sm" style="width:150px;" ' +
                'value="' + EscapeAttr(mbr.name || '') + '" placeholder="Label" ' +
                'onchange="UpdateMemberField(' + groupIdx + ',' + memberIdx + ',\'name\',this.value)"></td>';

            // Volume
            var vol = mbr.volume !== undefined ? mbr.volume : 100;
            html += '<td>';
            html += '<input type="range" class="volume-slider" min="0" max="100" value="' + vol + '" ' +
                'oninput="UpdateMemberVolume(' + groupIdx + ',' + memberIdx + ',parseInt(this.value));' +
                'this.nextElementSibling.textContent=this.value+\'%\'">';
            html += '<span class="volume-value">' + vol + '%</span>';
            html += '</td>';

            // Mute
            var muted = mbr.mute === true;
            html += '<td>';
            html += '<button class="mute-btn ' + (muted ? 'muted' : '') + '" ' +
                'onclick="ToggleMute(' + groupIdx + ',' + memberIdx + ')" title="' + (muted ? 'Unmute' : 'Mute') + '">';
            html += '<i class="fas ' + (muted ? 'fa-volume-mute' : 'fa-volume-up') + '"></i>';
            html += '</button>';
            html += '</td>';

            // Remove
            html += '<td><button class="btn-remove-member" onclick="RemoveMember(' + groupIdx + ',' + memberIdx + ')" title="Remove source">';
            html += '<i class="fas fa-times"></i></button></td>';

            html += '</tr>';
            return html;
        }

        // ─── Actions ───────────────────────────────────────────────────
        function AddInputGroup() {
            var newId = 1;
            inputGroups.inputGroups.forEach(function (g) {
                if (g.id >= newId) newId = g.id + 1;
            });

            inputGroups.inputGroups.push({
                id: newId,
                name: "Input Group " + newId,
                enabled: true,
                channels: 2,
                volume: 100,
                members: [
                    {
                        type: "fppd_stream",
                        sourceId: "fppd_stream_1",
                        name: "Media Playback",
                        volume: 100,
                        mute: false
                    }
                ],
                outputs: []
            });

            RenderAll();
        }

        function RemoveGroup(idx) {
            if (!confirm('Remove this input group?')) return;
            inputGroups.inputGroups.splice(idx, 1);
            RenderAll();
        }

        function UpdateGroupField(idx, field, value) {
            inputGroups.inputGroups[idx][field] = value;
            if (field === 'enabled') {
                RenderAll();
            }
        }

        function AddMember(groupIdx) {
            if (!inputGroups.inputGroups[groupIdx].members) {
                inputGroups.inputGroups[groupIdx].members = [];
            }
            inputGroups.inputGroups[groupIdx].members.push({
                type: "capture",
                cardId: "",
                name: "",
                volume: 100,
                mute: false
            });
            RenderAll();
        }

        function RemoveMember(groupIdx, memberIdx) {
            inputGroups.inputGroups[groupIdx].members.splice(memberIdx, 1);
            RenderAll();
        }

        function UpdateMemberField(groupIdx, memberIdx, field, value) {
            inputGroups.inputGroups[groupIdx].members[memberIdx][field] = value;
        }

        // Debounced real-time volume control
        var _volumeTimers = {};
        function UpdateMemberVolume(groupIdx, memberIdx, volume) {
            var mbr = inputGroups.inputGroups[groupIdx].members[memberIdx];
            mbr.volume = volume;

            // Don't send real-time API for fppd_stream (no loopback node)
            if (mbr.type === 'fppd_stream') return;

            // If muted, save the new volume but don't send it to PipeWire
            // (PipeWire node stays at 0 until unmuted)
            if (mbr.mute) return;

            // Debounce: only send after 150ms of no further changes
            var key = groupIdx + '_' + memberIdx;
            if (_volumeTimers[key]) clearTimeout(_volumeTimers[key]);
            _volumeTimers[key] = setTimeout(function () {
                var groupId = inputGroups.inputGroups[groupIdx].id;
                $.ajax({
                    url: '/api/pipewire/audio/input-groups/volume',
                    type: 'POST',
                    contentType: 'application/json',
                    data: JSON.stringify({
                        groupId: groupId,
                        memberIndex: memberIdx,
                        volume: volume
                    }),
                    error: function () {
                        // Silent fail — volume will be applied on next Save & Apply
                    }
                });
            }, 150);
        }

        function ToggleMute(groupIdx, memberIdx) {
            var mbr = inputGroups.inputGroups[groupIdx].members[memberIdx];
            mbr.mute = !mbr.mute;
            RenderAll();

            // Send real-time mute/unmute to PipeWire via the volume API.
            // Mute = volume 0, unmute = restore saved volume.
            // fppd_stream members don't have loopback nodes.
            if (mbr.type === 'fppd_stream') return;

            var effectiveVol = mbr.mute ? 0 : (mbr.volume !== undefined ? mbr.volume : 100);
            var groupId = inputGroups.inputGroups[groupIdx].id;
            $.ajax({
                url: '/api/pipewire/audio/input-groups/volume',
                type: 'POST',
                contentType: 'application/json',
                data: JSON.stringify({
                    groupId: groupId,
                    memberIndex: memberIdx,
                    volume: effectiveVol,
                    mute: mbr.mute
                }),
                error: function () {
                    // Silent fail — mute will be applied on next Save & Apply
                }
            });
        }

        function ToggleOutput(groupIdx, outputGroupId, checked) {
            var ig = inputGroups.inputGroups[groupIdx];
            if (!ig.outputs) ig.outputs = [];

            if (checked) {
                if (ig.outputs.indexOf(outputGroupId) === -1) {
                    ig.outputs.push(outputGroupId);
                }
            } else {
                ig.outputs = ig.outputs.filter(function (id) { return id !== outputGroupId; });
            }
        }

        // ─── Capture Device Selection ──────────────────────────────────
        function SelectCaptureDevice(groupIdx, memberIdx, cardId) {
            var mbr = inputGroups.inputGroups[groupIdx].members[memberIdx];
            mbr.cardId = cardId;

            // Store exact PipeWire node name for reliable targeting
            var src = availableSources.find(function (s) { return s.cardId === cardId; });
            if (src) {
                mbr.nodeName = src.name;  // exact PipeWire node name

                // Auto-populate name from device description
                if (!mbr.name) {
                    mbr.name = (src.nick || src.description || cardId).substring(0, 30);
                }
            } else {
                delete mbr.nodeName;
            }

            // Clear stale channel mapping when device changes
            delete mbr.channelMapping;
        }

        function SelectPWSource(groupIdx, memberIdx, nodeName) {
            var mbr = inputGroups.inputGroups[groupIdx].members[memberIdx];
            mbr.nodeName = nodeName;

            // Auto-populate name from source
            var src = availablePWSources.find(function (s) { return s.nodeName === nodeName; });
            if (src && !mbr.name) {
                mbr.name = src.name.substring(0, 30);
            }
        }

        // ─── Channel Mapping ───────────────────────────────────────────
        var CHANNEL_POSITIONS = {
            1: ['MONO'],
            2: ['FL', 'FR'],
            4: ['FL', 'FR', 'RL', 'RR'],
            6: ['FL', 'FR', 'FC', 'LFE', 'RL', 'RR'],
            8: ['FL', 'FR', 'FC', 'LFE', 'RL', 'RR', 'SL', 'SR']
        };

        function GetChannelMappingPresets(srcCh, grpCh) {
            // Common mapping presets for different source→group combinations
            var presets = [];
            presets.push({ label: 'Default (auto)', value: '' });

            if (srcCh === 1 && grpCh >= 2) {
                presets.push({
                    label: 'Mono → Center',
                    value: JSON.stringify({ sourceChannels: ['MONO'], groupChannels: ['FC'] })
                });
                presets.push({
                    label: 'Mono → Left+Right',
                    value: JSON.stringify({ sourceChannels: ['MONO'], groupChannels: ['FL', 'FR'] })
                });
                presets.push({
                    label: 'Mono → Left only',
                    value: JSON.stringify({ sourceChannels: ['MONO'], groupChannels: ['FL'] })
                });
                presets.push({
                    label: 'Mono → Right only',
                    value: JSON.stringify({ sourceChannels: ['MONO'], groupChannels: ['FR'] })
                });
            } else if (srcCh === 2 && grpCh === 1) {
                presets.push({
                    label: 'Left only → Mono',
                    value: JSON.stringify({ sourceChannels: ['FL'], groupChannels: ['MONO'] })
                });
                presets.push({
                    label: 'Right only → Mono',
                    value: JSON.stringify({ sourceChannels: ['FR'], groupChannels: ['MONO'] })
                });
            } else if (srcCh === 2 && grpCh > 2) {
                presets.push({
                    label: 'Stereo → Front L+R',
                    value: JSON.stringify({ sourceChannels: ['FL', 'FR'], groupChannels: ['FL', 'FR'] })
                });
                presets.push({
                    label: 'Stereo → Rear L+R',
                    value: JSON.stringify({ sourceChannels: ['FL', 'FR'], groupChannels: ['RL', 'RR'] })
                });
            } else if (srcCh > grpCh) {
                // Generic downmix
                var srcPos = CHANNEL_POSITIONS[srcCh] || [];
                var grpPos = CHANNEL_POSITIONS[grpCh] || [];
                if (srcPos.length && grpPos.length) {
                    presets.push({
                        label: 'First ' + grpCh + ' channels',
                        value: JSON.stringify({
                            sourceChannels: srcPos.slice(0, grpCh),
                            groupChannels: grpPos
                        })
                    });
                }
            }
            return presets;
        }

        function RenderChannelMapping(groupIdx, memberIdx, mbr, srcChannels, groupChannels) {
            var html = '<div class="channel-mapping">';
            html += '<label><i class="fas fa-exchange-alt"></i> Map:</label>';

            var presets = GetChannelMappingPresets(srcChannels, groupChannels);
            var currentVal = mbr.channelMapping ? JSON.stringify(mbr.channelMapping) : '';

            html += '<select class="form-select form-select-sm" style="width:auto;font-size:0.82rem;" ' +
                'onchange="UpdateChannelMapping(' + groupIdx + ',' + memberIdx + ',this.value)">';
            presets.forEach(function (p) {
                var sel = (p.value === currentVal) ? ' selected' : '';
                html += '<option value=\'' + EscapeAttr(p.value) + '\'' + sel + '>' + EscapeHtml(p.label) + '</option>';
            });
            html += '</select>';
            html += '</div>';
            return html;
        }

        function UpdateChannelMapping(groupIdx, memberIdx, jsonValue) {
            var mbr = inputGroups.inputGroups[groupIdx].members[memberIdx];
            if (jsonValue) {
                try {
                    mbr.channelMapping = JSON.parse(jsonValue);
                } catch (e) {
                    delete mbr.channelMapping;
                }
            } else {
                delete mbr.channelMapping;
            }
        }

        // ─── Save & Apply ──────────────────────────────────────────────
        function SaveInputGroups() {
            $.ajax({
                url: '/api/pipewire/audio/input-groups',
                type: 'POST',
                contentType: 'application/json',
                data: JSON.stringify(inputGroups),
                success: function (result) {
                    if (result.data) {
                        inputGroups = result.data;
                    }
                    $.jGrowl('Input groups saved.', { theme: 'success' });
                },
                error: function (xhr) {
                    var msg = 'Save failed';
                    try { msg = JSON.parse(xhr.responseText).message || msg; } catch (e) { }
                    $.jGrowl(msg, { theme: 'danger' });
                }
            });
        }

        function ApplyInputGroups() {
            // Save first, then apply
            $.ajax({
                url: '/api/pipewire/audio/input-groups',
                type: 'POST',
                contentType: 'application/json',
                data: JSON.stringify(inputGroups),
                success: function (saveResult) {
                    if (saveResult.data) {
                        inputGroups = saveResult.data;
                    }
                    $.jGrowl('Saving input groups...', { theme: 'info' });

                    // Now apply
                    $.ajax({
                        url: '/api/pipewire/audio/input-groups/apply',
                        type: 'POST',
                        contentType: 'application/json',
                        data: '{}',
                        success: function (applyResult) {
                            $.jGrowl(applyResult.message || 'Input groups applied.', { theme: 'success' });
                            CheckPipeWireStatus();
                        },
                        error: function (xhr) {
                            var msg = 'Apply failed';
                            try { msg = JSON.parse(xhr.responseText).message || msg; } catch (e) { }
                            $.jGrowl(msg, { theme: 'danger' });
                        }
                    });
                },
                error: function (xhr) {
                    var msg = 'Save failed';
                    try { msg = JSON.parse(xhr.responseText).message || msg; } catch (e) { }
                    $.jGrowl(msg, { theme: 'danger' });
                }
            });
        }

        // ─── Helpers ───────────────────────────────────────────────────
        function EscapeHtml(str) {
            if (!str) return '';
            return str.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
                .replace(/"/g, '&quot;').replace(/'/g, '&#039;');
        }

        function EscapeAttr(str) {
            if (!str) return '';
            return str.replace(/&/g, '&amp;').replace(/"/g, '&quot;').replace(/'/g, '&#039;');
        }
        // ─── Routing Matrix Modal ──────────────────────────────────────
        function OpenRoutingMatrix() {
            $('#routingMatrixFrame').attr('src', 'pipewire-routing-matrix.php?modal=1');
            $('#routingMatrixModal').modal('show');
        }
    </script>

    <!-- Routing Matrix Modal -->
    <div class="modal fade" id="routingMatrixModal" tabindex="-1" aria-labelledby="routingMatrixModalLabel"
        aria-hidden="true">
        <div class="modal-dialog modal-xl">
            <div class="modal-content">
                <div class="modal-header py-2">
                    <h5 class="modal-title" id="routingMatrixModalLabel"><i class="fas fa-th"></i> Routing Matrix</h5>
                    <button type="button" class="btn-close" data-bs-dismiss="modal" aria-label="Close"></button>
                </div>
                <div class="modal-body p-0">
                    <iframe id="routingMatrixFrame" src="" style="width:100%;border:none;display:block;height:78vh;"
                        scrolling="yes"></iframe>
                </div>
            </div>
        </div>
    </div>
    <script>
        $('#routingMatrixModal').on('hidden.bs.modal', function () {
            $('#routingMatrixFrame').attr('src', '');
        });
    </script>
    </body>

</html>