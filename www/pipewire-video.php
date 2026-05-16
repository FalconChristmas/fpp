<!DOCTYPE html>
<html lang="en">

<head>
    <?php
    include 'common/htmlMeta.inc';
    require_once "common.php";
    require_once 'config.php';
    include 'common/menuHead.inc';
    ?>

    <title><? echo $pageTitle; ?> - PipeWire Video Output Groups</title>

    <?php $modalMode = isset($_GET['modal']) && $_GET['modal'] == '1'; ?>

    <style>
        .group-card {
            border: 1px solid var(--bs-border-color, #dee2e6);
            border-radius: 8px;
            margin-bottom: 1.5rem;
            background: var(--bs-body-bg, #fff);
        }

        .group-card.disabled-group {
            opacity: 0.6;
        }

        .group-header {
            display: flex;
            align-items: center;
            gap: 0.75rem;
            padding: 0.75rem 1rem;
            border-bottom: 1px solid var(--bs-border-color, #dee2e6);
            background: var(--bs-tertiary-bg, #f8f9fa);
            border-radius: 8px 8px 0 0;
            flex-wrap: wrap;
        }

        .group-header .group-name-input {
            font-size: 1.1rem;
            font-weight: 600;
            border: 1px solid var(--bs-border-color, #ccc);
            background: var(--bs-body-bg, #fff);
            color: var(--bs-body-color, #212529);
            padding: 0.25rem 0.5rem;
            border-radius: 3px;
            min-width: 250px;
        }

        .group-header .group-name-input:focus {
            border-color: var(--bs-primary, #007cba);
            background: var(--bs-body-bg, #fff);
            color: var(--bs-body-color, #212529);
            outline: none;
            box-shadow: 0 0 3px rgba(0, 124, 186, 0.3);
        }

        .group-body {
            padding: 1rem;
        }

        .member-table {
            width: 100%;
            margin-top: 0.5rem;
        }

        .member-table th {
            font-weight: 600;
            font-size: 0.85rem;
            padding: 0.4rem 0.5rem;
            border-bottom: 2px solid var(--bs-border-color, #dee2e6);
            white-space: nowrap;
        }

        .member-table td {
            padding: 0.4rem 0.5rem;
            vertical-align: middle;
            border-bottom: 1px solid var(--bs-border-color-translucent, #dee2e620);
        }

        .no-groups-msg {
            text-align: center;
            margin: 2rem 0;
        }

        .no-members-row td {
            text-align: center;
            color: var(--bs-secondary-color, #6c757d);
            padding: 1rem;
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

        .connector-status {
            font-size: 0.85rem;
        }

        .connector-status .connected {
            color: #28a745;
        }

        .connector-status .disconnected {
            color: #dc3545;
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

        .btn-group-action {
            font-size: 0.85rem;
            padding: 0.25rem 0.75rem;
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
    echo ' style="margin:0;padding:1rem;"'; ?>>
    <?php if (!$modalMode) { ?>
        <div id="bodyWrapper">
            <?php
            $activeParentMenuItem = 'status';
            include 'menu.inc';
            ?>
            <div class="mainContainer">
                <h1 class="title">PipeWire Video Output Groups</h1>
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
                        <p>Video Output Groups require the Advanced PipeWire backend.<br>
                            Currently using: <strong><?= htmlspecialchars($mbDisplay) ?></strong></p>
                        <p>Change to PipeWire (Advanced) in <a href="settings.php?tab=Audio%2FVideo">FPP Settings &rarr;
                                Audio/Video</a>,
                            then return here to configure video output groups.</p>
                    </div>
                <?php } else { ?>

                    <div class="info-block">
                        <i class="fas fa-info-circle"></i>
                        A <strong>Video Output Group</strong> fans out a single video stream to
                        multiple destinations — a second HDMI display, pixel overlay models, or
                        network streams. Each group member receives the same video signal.
                        The primary HDMI display is always driven directly by GStreamer
                        for zero-latency output; these groups handle additional outputs
                        routed through the PipeWire graph.
                    </div>

                    <div id="pipewireStatus" class="toolbar">
                        <div class="toolbar-left">
                            <span id="pwStatus"><span class="status-indicator status-unknown"></span> Checking PipeWire
                                status...</span>
                        </div>
                        <div class="toolbar-right">
                            <button class="buttons btn-outline-success" onclick="AddGroup()">
                                <i class="fas fa-plus"></i> Add Group
                            </button>
                            <button class="buttons" onclick="SaveGroups()">
                                <i class="fas fa-save"></i> Save
                            </button>
                            <button class="buttons btn-outline-primary" onclick="ApplyGroups()">
                                <i class="fas fa-sync"></i> Save &amp; Apply
                            </button>
                        </div>
                    </div>

                    <div id="groupsContainer">
                        <div class="no-groups-msg" id="noGroupsMsg">
                            <i class="fas fa-tv" style="font-size:2rem; color:var(--bs-secondary-color,#6c757d);"></i>
                            <h4>No Video Output Groups Configured</h4>
                            <p>Add a group to route video streams through PipeWire to additional displays,
                                pixel overlays, or network destinations.</p>
                            <button class="buttons btn-outline-success" onclick="AddGroup()">
                                <i class="fas fa-plus"></i> Add First Group
                            </button>
                        </div>
                    </div>

                    <div id="bottomToolbar" class="toolbar" style="display:none; margin-top:1rem;">
                        <div class="toolbar-left"></div>
                        <div class="toolbar-right">
                            <button class="buttons" onclick="SaveGroups()">
                                <i class="fas fa-save"></i> Save
                            </button>
                            <button class="buttons btn-outline-primary" onclick="ApplyGroups()">
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
        // Available hardware targets
        var availableConnectors = [];
        var availableOverlays = [];
        var availableVideoSources = [];
        // Current config
        var videoGroups = { videoOutputGroups: [] };
        var nextGroupId = 1;

        function EscapeAttr(s) {
            return String(s).replace(/&/g, '&amp;').replace(/"/g, '&quot;')
                .replace(/'/g, '&#39;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
        }

        function EscapeNodeName(name) {
            return name.replace(/[^a-zA-Z0-9_]/g, '_').toLowerCase();
        }

        $(document).ready(function () {
            CheckPipeWireStatus();
            $.when(LoadTargets(), LoadVideoSources()).then(function () {
                LoadGroups();
            });
        });

        /////////////////////////////////////////////////////////////////////////////
        // PipeWire status check
        function CheckPipeWireStatus() {
            $.getJSON('api/pipewire/audio/sinks')
                .done(function (data) {
                    var count = data ? data.length : 0;
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

        /////////////////////////////////////////////////////////////////////////////
        // Load available video targets (connectors + overlays)
        function LoadTargets() {
            return $.getJSON('api/pipewire/video/connectors')
                .done(function (data) {
                    availableConnectors = (data && data.connectors) ? data.connectors : [];
                    availableOverlays = (data && data.overlayModels) ? data.overlayModels : [];
                })
                .fail(function () {
                    availableConnectors = [];
                    availableOverlays = [];
                });
        }

        /////////////////////////////////////////////////////////////////////////////
        // Load available video input sources
        function LoadVideoSources() {
            return $.getJSON('api/pipewire/video/input-sources')
                .done(function (data) {
                    var sources = (data && data.videoInputSources) ? data.videoInputSources : [];
                    availableVideoSources = [];
                    for (var i = 0; i < sources.length; i++) {
                        if (sources[i].enabled && sources[i].pipeWireNodeName) {
                            availableVideoSources.push({
                                nodeName: sources[i].pipeWireNodeName,
                                name: sources[i].name || sources[i].pipeWireNodeName
                            });
                        }
                    }
                })
                .fail(function () {
                    availableVideoSources = [];
                });
        }

        /////////////////////////////////////////////////////////////////////////////
        // Load saved groups
        function LoadGroups() {
            $.getJSON('api/pipewire/video/groups')
                .done(function (data) {
                    videoGroups = data || { videoOutputGroups: [] };
                    if (!videoGroups.videoOutputGroups) videoGroups.videoOutputGroups = [];
                    nextGroupId = 1;
                    for (var i = 0; i < videoGroups.videoOutputGroups.length; i++) {
                        if (videoGroups.videoOutputGroups[i].id >= nextGroupId) {
                            nextGroupId = videoGroups.videoOutputGroups[i].id + 1;
                        }
                    }
                    RenderGroups();
                })
                .fail(function () {
                    videoGroups = { videoOutputGroups: [] };
                    RenderGroups();
                });
        }

        /////////////////////////////////////////////////////////////////////////////
        // Render all group cards
        function RenderGroups() {
            var container = $('#groupsContainer');
            container.empty();

            if (videoGroups.videoOutputGroups.length === 0) {
                container.append(
                    '<div class="no-groups-msg" id="noGroupsMsg">' +
                    '<i class="fas fa-tv" style="font-size:2rem; color:var(--bs-secondary-color,#6c757d);"></i>' +
                    '<h4>No Video Output Groups Configured</h4>' +
                    '<p>Add a group to route video streams through PipeWire to additional displays, ' +
                    'pixel overlays, or network destinations.</p>' +
                    '<button class="buttons btn-outline-success" onclick="AddGroup()">' +
                    '<i class="fas fa-plus"></i> Add First Group</button>' +
                    '</div>'
                );
                $('#bottomToolbar').hide();
                return;
            }

            $('#bottomToolbar').show();

            for (var i = 0; i < videoGroups.videoOutputGroups.length; i++) {
                container.append(RenderGroupCard(videoGroups.videoOutputGroups[i], i));
            }
        }

        /////////////////////////////////////////////////////////////////////////////
        // Render a single group card
        function RenderGroupCard(group, index) {
            var enabledClass = group.enabled ? '' : ' disabled-group';
            var enabledChecked = group.enabled ? ' checked' : '';
            var nodeName = group.pipeWireNodeName ||
                ('fpp_video_group_' + group.id + '_' + EscapeNodeName(group.name || 'group'));

            var html = '<div class="group-card' + enabledClass + '" id="group-' + group.id + '">';

            // Header
            html += '<div class="group-header">';
            html += '<input type="checkbox" class="form-check-input" onchange="ToggleGroupEnabled(' + index + ', this.checked)"' + enabledChecked + ' title="Enable/Disable group">';
            html += '<input type="text" class="group-name-input" value="' + EscapeAttr(group.name || '') + '" onchange="UpdateGroupName(' + index + ', this.value)" placeholder="Group Name">';
            html += '<span class="badge bg-info pipewire-badge" title="PipeWire node name">' + EscapeAttr(nodeName) + '</span>';
            html += '<div style="flex:1"></div>';
            html += '<button class="buttons btn-outline-danger" onclick="DeleteGroup(' + index + ')" title="Delete Group" style="padding:0.25rem 0.5rem;"><i class="fas fa-trash"></i></button>';
            html += '</div>';

            // Body
            html += '<div class="group-body">';

            // Video source selector
            html += '<div class="row align-items-center mb-2">';
            html += '<div class="col-auto"><label style="font-weight:600;font-size:0.85rem;">Video Source:</label></div>';
            html += '<div class="col-auto">';
            html += '<select class="form-select form-select-sm" style="width:auto;" onchange="UpdateGroupSource(' + index + ', this.value)">';
            html += '<option value=""' + (!group.videoSource ? ' selected' : '') + '>Media Playback (Default)</option>';
            for (var s = 0; s < availableVideoSources.length; s++) {
                var vs = availableVideoSources[s];
                var sel = (group.videoSource === vs.nodeName) ? ' selected' : '';
                html += '<option value="' + EscapeAttr(vs.nodeName) + '"' + sel + '>' + EscapeAttr(vs.name) + '</option>';
            }
            html += '</select>';
            if (group.videoSource) {
                html += ' <span class="badge bg-success" style="font-size:0.75rem;"><i class="fas fa-video"></i> Persistent source</span>';
            }
            html += '</div>';
            html += '</div>';

            // Stream slot selector (only for on-demand / media playback groups)
            if (!group.videoSource) {
                var slots = group.streamSlots || [];
                var allSlots = (slots.length === 0); // empty = all
                html += '<div class="row align-items-center mb-2">';
                html += '<div class="col-auto"><label style="font-weight:600;font-size:0.85rem;">Stream Slots:</label></div>';
                html += '<div class="col-auto d-flex flex-wrap gap-2 align-items-center">';
                html += '<div class="form-check form-check-inline">';
                html += '<input class="form-check-input" type="checkbox" id="slot_all_' + group.id + '" ' + (allSlots ? 'checked' : '') + ' onchange="ToggleAllSlots(' + index + ',this.checked)">';
                html += '<label class="form-check-label" for="slot_all_' + group.id + '">All</label>';
                html += '</div>';
                for (var sl = 1; sl <= 5; sl++) {
                    var slotChecked = allSlots || (slots.indexOf(sl) >= 0);
                    var slotDisabled = allSlots ? ' disabled' : '';
                    html += '<div class="form-check form-check-inline">';
                    html += '<input class="form-check-input slot-cb-' + group.id + '" type="checkbox" value="' + sl + '" id="slot_' + group.id + '_' + sl + '" ' + (slotChecked ? 'checked' : '') + slotDisabled + ' onchange="UpdateGroupSlots(' + index + ')">';
                    html += '<label class="form-check-label" for="slot_' + group.id + '_' + sl + '">' + sl + '</label>';
                    html += '</div>';
                }
                html += '</div>';
                html += '</div>';
            }

            // Member table
            html += '<table class="member-table">';
            html += '<thead><tr>';
            html += '<th>#</th>';
            html += '<th>Output Type</th>';
            html += '<th>Destination</th>';
            html += '<th>Options</th>';
            html += '<th></th>';
            html += '</tr></thead>';
            html += '<tbody id="members-' + group.id + '">';

            if (!group.members || group.members.length === 0) {
                html += '<tr class="no-members-row"><td colspan="5">No output members — add an HDMI display, pixel overlay, or network stream</td></tr>';
            } else {
                for (var m = 0; m < group.members.length; m++) {
                    html += RenderMemberRow(group, index, m);
                }
            }

            html += '</tbody></table>';

            // Add member button
            html += '<div style="margin-top:0.5rem;">';
            html += '<button class="buttons btn-outline-success btn-group-action" onclick="AddMember(' + index + ')">';
            html += '<i class="fas fa-plus"></i> Add Output</button>';
            html += '</div>';

            html += '</div>'; // group-body
            html += '</div>'; // group-card

            return html;
        }

        /////////////////////////////////////////////////////////////////////////////
        // Render a single member row
        function RenderMemberRow(group, groupIndex, memberIndex) {
            var member = group.members[memberIndex];
            var type = member.type || 'hdmi';

            var html = '<tr data-member="' + memberIndex + '">';
            html += '<td>' + (memberIndex + 1) + '</td>';

            // Type selector
            html += '<td>';
            html += '<select class="form-select form-select-sm" style="width:auto;" onchange="UpdateMemberType(' + groupIndex + ',' + memberIndex + ',this.value)">';
            html += '<option value="hdmi"' + (type === 'hdmi' ? ' selected' : '') + '>HDMI Display</option>';
            html += '<option value="overlay"' + (type === 'overlay' ? ' selected' : '') + '>Pixel Overlay</option>';
            html += '<option value="rtp"' + (type === 'rtp' ? ' selected' : '') + '>Network (RTP)</option>';
            html += '</select>';
            html += '</td>';

            // Destination (type-specific)
            html += '<td>' + RenderMemberDestination(member, groupIndex, memberIndex, type) + '</td>';

            // Options (type-specific)
            html += '<td>' + RenderMemberOptions(member, groupIndex, memberIndex, type) + '</td>';

            // Remove button
            html += '<td>';
            html += '<button class="buttons btn-outline-danger btn-group-action" onclick="RemoveMember(' + groupIndex + ',' + memberIndex + ')" title="Remove">';
            html += '<i class="fas fa-times"></i></button>';
            html += '</td>';

            html += '</tr>';
            return html;
        }

        /////////////////////////////////////////////////////////////////////////////
        // Render member destination field (varies by type)
        function RenderMemberDestination(member, gi, mi, type) {
            var html = '';
            switch (type) {
                case 'hdmi':
                    html += '<select class="form-select form-select-sm" style="width:auto;" onchange="UpdateMemberField(' + gi + ',' + mi + ',\'connector\',this.value)">';
                    html += '<option value="">— Select Connector —</option>';
                    for (var i = 0; i < availableConnectors.length; i++) {
                        var c = availableConnectors[i];
                        var sel = (member.connector === c.connector) ? ' selected' : '';
                        var statusTxt = c.connected ? ' (' + c.width + 'x' + c.height + ')' : ' (disconnected)';
                        html += '<option value="' + EscapeAttr(c.connector) + '"' + sel + '>' + EscapeAttr(c.connector) + statusTxt + '</option>';
                    }
                    html += '</select>';
                    if (member.connector) {
                        for (var i = 0; i < availableConnectors.length; i++) {
                            if (availableConnectors[i].connector === member.connector) {
                                var c = availableConnectors[i];
                                if (c.connected) {
                                    html += ' <span class="connector-status"><span class="connected"><i class="fas fa-check-circle"></i></span></span>';
                                } else {
                                    html += ' <span class="connector-status"><span class="disconnected"><i class="fas fa-times-circle"></i></span></span>';
                                }
                                break;
                            }
                        }
                    }
                    break;

                case 'overlay':
                    html += '<select class="form-select form-select-sm" style="width:auto;" onchange="UpdateMemberField(' + gi + ',' + mi + ',\'overlayModel\',this.value)">';
                    html += '<option value="">— Select Model —</option>';
                    for (var i = 0; i < availableOverlays.length; i++) {
                        var m = availableOverlays[i];
                        var sel = (member.overlayModel === m.name) ? ' selected' : '';
                        html += '<option value="' + EscapeAttr(m.name) + '"' + sel + '>' + EscapeAttr(m.name) + ' (' + m.width + 'x' + m.height + ')</option>';
                    }
                    html += '</select>';
                    break;

                case 'rtp':
                    html += '<input type="text" class="form-control form-control-sm" style="width:140px;display:inline-block;" value="' + EscapeAttr(member.address || '239.0.0.1') + '" onchange="UpdateMemberField(' + gi + ',' + mi + ',\'address\',this.value)" placeholder="Multicast address">';
                    html += ' : ';
                    html += '<input type="number" class="form-control form-control-sm" style="width:80px;display:inline-block;" value="' + (member.port || 5004) + '" onchange="UpdateMemberField(' + gi + ',' + mi + ',\'port\',parseInt(this.value))" min="1024" max="65535">';
                    html += ' &nbsp; ';
                    html += '<select class="form-select form-select-sm" style="width:auto;display:inline-block;" onchange="UpdateMemberField(' + gi + ',' + mi + ',\'encoding\',this.value)">';
                    var enc = member.encoding || 'h264';
                    html += '<option value="h264"' + (enc === 'h264' ? ' selected' : '') + '>H.264</option>';
                    html += '<option value="h265"' + (enc === 'h265' ? ' selected' : '') + '>H.265 (HEVC)</option>';
                    html += '<option value="mjpeg"' + (enc === 'mjpeg' ? ' selected' : '') + '>Motion JPEG</option>';
                    html += '<option value="raw"' + (enc === 'raw' ? ' selected' : '') + '>Raw (uncompressed)</option>';
                    html += '</select>';
                    break;
            }
            return html;
        }

        /////////////////////////////////////////////////////////////////////////////
        // Render member options field (varies by type)
        function RenderMemberOptions(member, gi, mi, type) {
            var html = '';
            switch (type) {
                case 'hdmi':
                    var scaling = member.scaling || 'fit';
                    html += '<select class="form-select form-select-sm" style="width:auto;" onchange="UpdateMemberField(' + gi + ',' + mi + ',\'scaling\',this.value)" title="Scaling mode">';
                    html += '<option value="fit"' + (scaling === 'fit' ? ' selected' : '') + '>Fit</option>';
                    html += '<option value="fill"' + (scaling === 'fill' ? ' selected' : '') + '>Fill</option>';
                    html += '<option value="stretch"' + (scaling === 'stretch' ? ' selected' : '') + '>Stretch</option>';
                    html += '</select>';
                    break;
                case 'overlay':
                    html += '<span class="text-muted" style="font-size:0.85rem;">Auto-scaled to model</span>';
                    break;
                case 'rtp':
                    var sdpName = member.pipeWireNodeName || '';
                    if (sdpName) {
                        html += '<a href="api/configfile/rtp_' + encodeURIComponent(sdpName) + '.sdp" class="btn btn-sm btn-outline-secondary" title="Download SDP file for VLC/ffplay" download>';
                        html += '<i class="fas fa-download"></i> SDP</a>';
                    } else {
                        html += '<span class="text-muted" style="font-size:0.85rem;">Save &amp; Apply to generate SDP file</span>';
                    }
                    break;
            }
            return html;
        }

        /////////////////////////////////////////////////////////////////////////////
        // Add a new group
        function AddGroup() {
            var group = {
                id: nextGroupId++,
                name: 'Video Group ' + (videoGroups.videoOutputGroups.length + 1),
                enabled: true,
                members: []
            };
            videoGroups.videoOutputGroups.push(group);
            RenderGroups();
        }

        /////////////////////////////////////////////////////////////////////////////
        // Delete a group
        function DeleteGroup(index) {
            var name = videoGroups.videoOutputGroups[index].name || 'this group';
            if (confirm('Delete "' + name + '"?')) {
                videoGroups.videoOutputGroups.splice(index, 1);
                RenderGroups();
            }
        }

        /////////////////////////////////////////////////////////////////////////////
        // Toggle group enabled
        function ToggleGroupEnabled(index, enabled) {
            videoGroups.videoOutputGroups[index].enabled = enabled;
            RenderGroups();
        }

        /////////////////////////////////////////////////////////////////////////////
        // Update group name
        function UpdateGroupName(index, name) {
            videoGroups.videoOutputGroups[index].name = name;
            var slug = EscapeNodeName(name);
            videoGroups.videoOutputGroups[index].pipeWireNodeName =
                'fpp_video_group_' + videoGroups.videoOutputGroups[index].id + '_' + slug;
            RenderGroups();
        }

        /////////////////////////////////////////////////////////////////////////////
        // Update group video source
        function UpdateGroupSource(index, source) {
            if (source) {
                videoGroups.videoOutputGroups[index].videoSource = source;
                // Persistent source — clear stream slots (not applicable)
                delete videoGroups.videoOutputGroups[index].streamSlots;
            } else {
                delete videoGroups.videoOutputGroups[index].videoSource;
            }
            RenderGroups();
        }

        /////////////////////////////////////////////////////////////////////////////
        // Toggle "All" stream slots for a group
        function ToggleAllSlots(index, allChecked) {
            if (allChecked) {
                delete videoGroups.videoOutputGroups[index].streamSlots;
            } else {
                // Default to all 5 when unchecking "All"
                videoGroups.videoOutputGroups[index].streamSlots = [1, 2, 3, 4, 5];
            }
            RenderGroups();
        }

        /////////////////////////////////////////////////////////////////////////////
        // Update stream slots from checkboxes
        function UpdateGroupSlots(index) {
            var groupId = videoGroups.videoOutputGroups[index].id;
            var slots = [];
            $('.slot-cb-' + groupId + ':checked:not(:disabled)').each(function () {
                slots.push(parseInt($(this).val()));
            });
            if (slots.length === 0 || slots.length === 5) {
                delete videoGroups.videoOutputGroups[index].streamSlots;
            } else {
                slots.sort();
                videoGroups.videoOutputGroups[index].streamSlots = slots;
            }
        }

        /////////////////////////////////////////////////////////////////////////////
        // Add a member to a group
        function AddMember(groupIndex) {
            var member = {
                type: 'hdmi',
                connector: '',
                scaling: 'fit'
            };

            // Default to first unused HDMI connector
            var usedConnectors = {};
            var grp = videoGroups.videoOutputGroups[groupIndex];
            for (var i = 0; i < grp.members.length; i++) {
                if (grp.members[i].type === 'hdmi' && grp.members[i].connector) {
                    usedConnectors[grp.members[i].connector] = true;
                }
            }
            for (var i = 0; i < availableConnectors.length; i++) {
                if (!usedConnectors[availableConnectors[i].connector]) {
                    member.connector = availableConnectors[i].connector;
                    break;
                }
            }

            grp.members.push(member);
            RenderGroups();
        }

        /////////////////////////////////////////////////////////////////////////////
        // Remove a member from a group
        function RemoveMember(groupIndex, memberIndex) {
            videoGroups.videoOutputGroups[groupIndex].members.splice(memberIndex, 1);
            RenderGroups();
        }

        /////////////////////////////////////////////////////////////////////////////
        // Update member type (clears type-specific fields)
        function UpdateMemberType(groupIndex, memberIndex, type) {
            var member = videoGroups.videoOutputGroups[groupIndex].members[memberIndex];
            member.type = type;
            delete member.connector;
            delete member.overlayModel;
            delete member.address;
            delete member.port;
            delete member.scaling;
            delete member.encoding;
            if (type === 'hdmi') {
                member.scaling = 'fit';
            } else if (type === 'rtp') {
                member.address = '239.0.0.1';
                member.port = 5004;
                member.encoding = 'h264';
            }
            RenderGroups();
        }

        /////////////////////////////////////////////////////////////////////////////
        // Update a single field on a member
        function UpdateMemberField(groupIndex, memberIndex, field, value) {
            videoGroups.videoOutputGroups[groupIndex].members[memberIndex][field] = value;
            RenderGroups();
        }

        /////////////////////////////////////////////////////////////////////////////
        // Save groups to server (without applying)
        function SaveGroups() {
            $.ajax({
                url: 'api/pipewire/video/groups',
                type: 'POST',
                contentType: 'application/json',
                data: JSON.stringify(videoGroups),
                success: function (result) {
                    $.jGrowl('Video output groups saved', { theme: 'success' });
                },
                error: function (xhr) {
                    $.jGrowl('Failed to save: ' + (xhr.responseJSON ? xhr.responseJSON.message : xhr.statusText), { theme: 'danger' });
                }
            });
        }

        /////////////////////////////////////////////////////////////////////////////
        // Save and apply
        function ApplyGroups() {
            ShowApplyProgress('Saving configuration...');
            $.ajax({
                url: 'api/pipewire/video/groups',
                type: 'POST',
                contentType: 'application/json',
                data: JSON.stringify(videoGroups),
                success: function () {
                    ShowApplyProgress('Applying video output groups...');
                    $.ajax({
                        url: 'api/pipewire/video/groups/apply',
                        type: 'POST',
                        contentType: 'application/json',
                        data: '{}',
                        success: function (result) {
                            HideApplyProgress();
                            $.jGrowl('Video output groups applied: ' + (result.message || 'OK'), { theme: 'success' });
                            LoadGroups();
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

        /////////////////////////////////////////////////////////////////////////////
        // Progress overlay
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