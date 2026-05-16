<!DOCTYPE html>
<html lang="en">

<head>
    <?php
    include 'common/htmlMeta.inc';
    require_once "common.php";
    require_once 'config.php';
    include 'common/menuHead.inc';
    ?>

    <title><? echo $pageTitle; ?> - PipeWire Audio Output Groups</title>

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
            min-width: 300px;
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

        .group-settings {
            display: flex;
            gap: 1rem;
            flex-wrap: wrap;
            margin-bottom: 1rem;
            align-items: center;
        }

        .group-settings label {
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
            border-bottom: 2px solid var(--bs-border-color, #dee2e6);
            font-weight: 600;
            font-size: 0.9rem;
        }

        .member-table td {
            padding: 0.5rem 0.75rem;
            border-bottom: 1px solid var(--bs-border-color, #dee2e6);
            vertical-align: middle;
        }

        .member-table tr:last-child td {
            border-bottom: none;
        }

        .member-table tr:hover {
            background: var(--bs-tertiary-bg, rgba(0, 0, 0, 0.02));
        }

        .channel-map-grid {
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(120px, 1fr));
            gap: 0.25rem;
        }

        .channel-map-item {
            display: flex;
            align-items: center;
            gap: 0.25rem;
            font-size: 0.85rem;
        }

        .channel-map-item select {
            font-size: 0.85rem;
            padding: 0.15rem 0.3rem;
        }

        .volume-slider-container {
            display: flex;
            align-items: center;
            gap: 0.5rem;
            min-width: 200px;
        }

        .volume-slider-container input[type="range"] {
            flex: 1;
        }

        .volume-value {
            min-width: 40px;
            text-align: right;
            font-size: 0.9rem;
        }

        .btn-group-action {
            padding: 0.25rem 0.5rem;
            font-size: 0.85rem;
        }

        .no-groups-msg {
            text-align: center;
            padding: 3rem;
            color: var(--bs-secondary-color, #6c757d);
        }

        .no-groups-msg i {
            font-size: 3rem;
            margin-bottom: 1rem;
            display: block;
        }

        .pipewire-badge {
            font-size: 0.75rem;
            padding: 0.2rem 0.5rem;
            vertical-align: middle;
        }

        .status-indicator {
            display: inline-block;
            width: 10px;
            height: 10px;
            border-radius: 50%;
            margin-right: 0.5rem;
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

        .toolbar {
            display: flex;
            justify-content: space-between;
            align-items: center;
            flex-wrap: wrap;
            gap: 0.5rem;
            margin-bottom: 1rem;
        }

        .toolbar-left,
        .toolbar-right {
            display: flex;
            gap: 0.5rem;
            align-items: center;
        }

        .alsa-warning {
            background: var(--bs-warning-bg-subtle, #fff3cd);
            border: 1px solid var(--bs-warning-border-subtle, #ffecb5);
            border-radius: 8px;
            padding: 1.5rem;
            text-align: center;
            margin: 2rem 0;
        }

        /* EQ Controls */
        .eq-toggle-btn {
            font-size: 0.78rem;
            padding: 0.15rem 0.5rem;
        }

        .eq-panel {
            background: var(--bs-tertiary-bg, #f8f9fa);
            border: 1px solid var(--bs-border-color, #dee2e6);
            border-radius: 6px;
            padding: 0.75rem;
        }

        .eq-header {
            display: flex;
            align-items: center;
            justify-content: space-between;
            margin-bottom: 0.5rem;
        }

        .eq-header-label {
            font-weight: 600;
            font-size: 0.9rem;
        }

        .eq-band-header,
        .eq-band-row {
            display: flex;
            align-items: center;
            gap: 0.5rem;
            padding: 0.2rem 0;
        }

        .eq-band-header {
            font-weight: 600;
            font-size: 0.78rem;
            color: var(--bs-secondary-color, #6c757d);
            border-bottom: 1px solid var(--bs-border-color, #dee2e6);
            margin-bottom: 0.25rem;
        }

        .eq-col-num {
            min-width: 20px;
            text-align: center;
        }

        .eq-col-type {
            min-width: 100px;
        }

        .eq-col-freq {
            min-width: 80px;
        }

        .eq-col-gain {
            min-width: 160px;
        }

        .eq-col-q {
            min-width: 60px;
        }

        .eq-col-action {
            min-width: 30px;
        }

        .eq-gain-slider {
            display: flex;
            align-items: center;
            gap: 0.25rem;
        }

        .eq-gain-slider input[type="range"] {
            width: 100px;
        }

        .eq-gain-value {
            font-size: 0.8rem;
            min-width: 40px;
            text-align: right;
            font-family: monospace;
        }

        .eq-band-row select,
        .eq-band-row input[type="number"] {
            font-size: 0.8rem;
            padding: 0.15rem 0.3rem;
        }

        /* Help icon sizing inside compact areas */
        .eq-band-header .icon-help,
        .member-table th .icon-help {
            width: 18px;
            height: 18px;
            padding-left: 2px;
            vertical-align: middle;
            cursor: help;
        }

        .group-settings .icon-help {
            width: 22px;
            height: 22px;
            vertical-align: middle;
            cursor: help;
        }

        /* Sync calibration panel */
        .sync-cal-panel {
            background: var(--bs-warning-bg-subtle, #fff3cd);
            border: 1px solid var(--bs-warning-border-subtle, #ffecb5);
            border-radius: 6px;
            padding: 0.75rem 1rem;
            margin-top: 0.75rem;
        }

        .sync-cal-header {
            margin-bottom: 0.5rem;
        }

        .sync-cal-desc {
            display: block;
            font-size: 0.85rem;
            color: var(--bs-secondary-color, #6c757d);
            margin-top: 0.25rem;
        }

        .sync-cal-controls {
            display: flex;
            gap: 0.5rem;
            align-items: center;
        }

        /* Delay control */
        .delay-control {
            display: flex;
            align-items: center;
            gap: 0.25rem;
        }

        .delay-control input[type="number"] {
            font-size: 0.85rem;
        }

        <?php if ($modalMode) { ?>
            /* Ensure dialogs inside the iframe appear above the parent modal */
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
                <h1 class="title">PipeWire Audio Output Groups</h1>
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
                        <p>Audio Output Groups require the Advanced PipeWire backend.<br>
                            Currently using: <strong><?= htmlspecialchars($mbDisplay) ?></strong></p>
                        <p>Change to PipeWire (Advanced) in <a href="settings.php?tab=Audio%2FVideo">FPP Settings &rarr;
                                Audio/Video</a>,
                            then return here to configure audio groups.</p>
                    </div>
                <?php } else { ?>

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
                            <i class="fas fa-layer-group"></i>
                            <h4>No Audio Groups Configured</h4>
                            <p>Create audio output groups to combine multiple sound cards into virtual sinks.<br>
                                Each group becomes an independent audio output that can be targeted by different streams.
                            </p>
                            <button class="buttons btn-outline-success" onclick="AddGroup()">
                                <i class="fas fa-plus"></i> Create First Group
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
        <!-- Modal dialog base template needed by DialogOK/DialogError -->
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
        // Available ALSA cards cache
        var availableCards = [];
        // Current groups data
        var audioGroups = { groups: [] };
        // Next group ID counter
        var nextGroupId = 1;

        // Help icon tooltip builder
        function HelpIcon(text) {
            return ' <img src="images/redesign/help-icon.svg" class="icon-help" data-bs-toggle="tooltip" data-bs-html="true" data-bs-placement="auto" title="' + EscapeAttr(text) + '">';
        }

        function InitTooltips() {
            // Dispose old tooltips then re-init
            $('[data-bs-toggle="tooltip"]').each(function () {
                var existing = bootstrap.Tooltip.getInstance(this);
                if (existing) existing.dispose();
            });
            $('[data-bs-toggle="tooltip"]').each(function () {
                new bootstrap.Tooltip(this);
            });
        }

        // PipeWire channel positions
        var CHANNEL_POSITIONS = {
            1: ['MONO'],
            2: ['FL', 'FR'],
            4: ['FL', 'FR', 'RL', 'RR'],
            6: ['FL', 'FR', 'FC', 'LFE', 'RL', 'RR'],
            8: ['FL', 'FR', 'FC', 'LFE', 'RL', 'RR', 'SL', 'SR']
        };

        var ALL_POSITIONS = ['FL', 'FR', 'FC', 'LFE', 'RL', 'RR', 'SL', 'SR',
            'AUX0', 'AUX1', 'AUX2', 'AUX3', 'AUX4', 'AUX5', 'AUX6', 'AUX7',
            'MONO'];

        // EQ filter types available in PipeWire filter-chain biquads
        var EQ_BAND_TYPES = [
            { value: 'bq_peaking', label: 'Peaking' },
            { value: 'bq_lowshelf', label: 'Low Shelf' },
            { value: 'bq_highshelf', label: 'High Shelf' },
            { value: 'bq_lowpass', label: 'Low Pass' },
            { value: 'bq_highpass', label: 'High Pass' },
            { value: 'bq_notch', label: 'Notch' },
            { value: 'bq_bandpass', label: 'Band Pass' },
            { value: 'bq_allpass', label: 'All Pass' }
        ];

        function DefaultEQBands() {
            return [
                { type: 'bq_lowshelf', freq: 60, gain: 0, q: 0.7 },
                { type: 'bq_peaking', freq: 250, gain: 0, q: 1.0 },
                { type: 'bq_peaking', freq: 1000, gain: 0, q: 1.0 },
                { type: 'bq_peaking', freq: 4000, gain: 0, q: 1.0 },
                { type: 'bq_highshelf', freq: 12000, gain: 0, q: 0.7 }
            ];
        }

        $(document).ready(function () {
            CheckPipeWireStatus();
            // Cards must be loaded before groups so the dropdowns can render
            LoadAvailableCards().then(function () {
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
                        'PipeWire running — ' + count + ' sink' + (count !== 1 ? 's' : '') + ' available'
                    );
                })
                .fail(function () {
                    $('#pwStatus').html(
                        '<span class="status-indicator status-stopped"></span>' +
                        'PipeWire not responding — ensure the service is running'
                    );
                });
        }

        /////////////////////////////////////////////////////////////////////////////
        // Load available ALSA cards
        function LoadAvailableCards() {
            return $.getJSON('api/pipewire/audio/cards')
                .done(function (data) {
                    availableCards = data || [];
                })
                .fail(function () {
                    availableCards = [];
                    console.error('Failed to load ALSA cards');
                });
        }

        /////////////////////////////////////////////////////////////////////////////
        // Load saved groups
        function LoadGroups() {
            $.getJSON('api/pipewire/audio/groups')
                .done(function (data) {
                    audioGroups = data || { groups: [] };
                    // Calculate next ID
                    nextGroupId = 1;
                    for (var i = 0; i < audioGroups.groups.length; i++) {
                        if (audioGroups.groups[i].id >= nextGroupId) {
                            nextGroupId = audioGroups.groups[i].id + 1;
                        }
                    }
                    RenderGroups();
                })
                .fail(function () {
                    audioGroups = { groups: [] };
                    RenderGroups();
                });
        }

        /////////////////////////////////////////////////////////////////////////////
        // Render all groups
        function RenderGroups() {
            var container = $('#groupsContainer');
            container.empty();

            if (audioGroups.groups.length === 0) {
                container.append(
                    '<div class="no-groups-msg" id="noGroupsMsg">' +
                    '<i class="fas fa-layer-group"></i>' +
                    '<h4>No Audio Groups Configured</h4>' +
                    '<p>Create audio output groups to combine multiple sound cards into virtual sinks.<br>' +
                    'Each group becomes an independent audio output that can be targeted by different streams.</p>' +
                    '<button class="buttons btn-outline-success" onclick="AddGroup()">' +
                    '<i class="fas fa-plus"></i> Create First Group</button>' +
                    '</div>'
                );
                $('#bottomToolbar').hide();
                return;
            }

            $('#bottomToolbar').show();

            for (var i = 0; i < audioGroups.groups.length; i++) {
                container.append(RenderGroupCard(audioGroups.groups[i], i));
            }

            // Auto-expand EQ panels for members with EQ enabled
            for (var i = 0; i < audioGroups.groups.length; i++) {
                var g = audioGroups.groups[i];
                if (g.members) {
                    for (var m = 0; m < g.members.length; m++) {
                        if (g.members[m].eq && g.members[m].eq.enabled) {
                            $('#eq-panel-row-' + i + '-' + m).show();
                        }
                    }
                }
            }

            InitTooltips();
        }

        /////////////////////////////////////////////////////////////////////////////
        // Render a single group card
        function RenderGroupCard(group, index) {
            var enabledClass = group.enabled ? '' : ' disabled-group';
            var enabledChecked = group.enabled ? ' checked' : '';
            var latencyChecked = group.latencyCompensate ? ' checked' : '';

            var html = '<div class="group-card' + enabledClass + '" id="group-' + group.id + '" data-index="' + index + '">';

            // Header
            html += '<div class="group-header">';
            html += '<input type="checkbox" class="form-check-input" onchange="ToggleGroupEnabled(' + index + ', this.checked)"' + enabledChecked + ' title="Enable/Disable group">';
            html += '<input type="text" class="group-name-input" value="' + EscapeAttr(group.name) + '" onchange="UpdateGroupName(' + index + ', this.value)" placeholder="Group Name">';
            html += '<span class="badge bg-info pipewire-badge">Combine Sink: fpp_group_' + EscapeNodeName(group.name) + '</span>';
            html += '<div style="flex:1"></div>';
            html += '<button class="buttons btn-outline-danger btn-group-action" onclick="DeleteGroup(' + index + ')" title="Delete Group"><i class="fas fa-trash"></i></button>';
            html += '</div>';

            // Body
            html += '<div class="group-body">';

            // Group settings
            html += '<div class="group-settings">';
            html += '<div>';
            html += '<label># of Channels Group Accepts:' + HelpIcon('Total number of audio channels this group\'s virtual combine-sink will accept. Sources playing to this group must match this channel count. Common layouts: 2ch (Stereo), 6ch (5.1 Surround), 8ch (7.1 Surround).') + '</label>';
            html += '<select class="form-select form-select-sm" style="display:inline-block;width:auto;" onchange="UpdateGroupChannels(' + index + ', parseInt(this.value))">';
            var channelOptions = [2, 4, 6, 8];
            for (var c = 0; c < channelOptions.length; c++) {
                var sel = (group.channels === channelOptions[c]) ? ' selected' : '';
                html += '<option value="' + channelOptions[c] + '"' + sel + '>' + channelOptions[c] + 'ch (' + ChannelLayoutName(channelOptions[c]) + ')</option>';
            }
            html += '</select>';
            html += '</div>';

            html += '<div>';
            html += '<label><input type="checkbox" class="form-check-input" onchange="UpdateGroupLatency(' + index + ', this.checked)"' + latencyChecked + '> Latency Compensation' + HelpIcon('When enabled, PipeWire measures the latency of each member sound card and delays the faster ones so all outputs play in sync. Recommended when combining cards with different hardware (e.g. USB + HDMI) to avoid audible timing drift between outputs.') + '</label>';
            html += '</div>';

            // Volume control for the group sink
            html += '<div class="volume-slider-container">';
            html += '<label><i class="fas fa-volume-up"></i>' + HelpIcon('Master volume for the entire group combine-sink. This scales the mixed output sent to all member sound cards. Adjusts in real-time without needing to re-apply.') + '</label>';
            html += '<input type="range" class="form-range" min="0" max="100" value="' + (group.volume ?? 100) + '" oninput="UpdateGroupVolumeDisplay(this); ScheduleGroupVolume(' + index + ', this.value)">';
            html += '<span class="volume-value">' + (group.volume ?? 100) + '%</span>';
            html += '</div>';

            html += '</div>';

            // Members table
            html += '<table class="member-table">';
            html += '<thead><tr>';
            html += '<th style="width:30px">#</th>';
            html += '<th>Sound Card' + HelpIcon('The physical ALSA sound card to include in this group. Each card receives a copy of the group audio according to its channel mapping.') + '</th>';
            html += '<th>Card Channels' + HelpIcon('Number of channels the physical card will receive. Set this to match the card\'s actual capability (e.g. 2 for stereo, 8 for 7.1).') + '</th>';
            html += '<th>Channel Mapping' + HelpIcon('Maps each of this card\'s channels to a position in the group\'s channel layout. For example, you can route the group\'s Front-Left to this card\'s Front-Left, or remap surround channels to stereo outputs.') + '</th>';
            html += '<th>Volume' + HelpIcon('Per-card volume. Adjusts this member\'s output level independently from the group master volume. Useful for balancing levels between different sound cards. Adjusts in real-time.') + '</th>';
            html += '<th>Delay (ms)' + HelpIcon('Per-card delay compensation in milliseconds. Use this to synchronize outputs that have different inherent latencies (e.g. AES67 network audio vs local USB). Delay the faster outputs so they match the slowest one. Adjustable in real-time. Use the Sync Calibration tool to dial in the exact values.') + '</th>';
            html += '<th>Rate' + HelpIcon('Sample rate for this card\u2019s ALSA adapter. \u201cAuto\u201d lets PipeWire negotiate the best rate. Override only if a specific card needs a fixed rate.') + '</th>';
            html += '<th>Period' + HelpIcon('ALSA period size (DMA transfer size) for this card. \u201cAuto\u201d uses the default (1024). Larger values increase latency but improve stability for some USB devices.') + '</th>';
            html += '<th style="width:60px"></th>';
            html += '</tr></thead>';
            html += '<tbody id="members-' + group.id + '">';

            if (group.members && group.members.length > 0) {
                for (var m = 0; m < group.members.length; m++) {
                    html += RenderMemberRow(group, index, m);
                }
            } else {
                html += '<tr class="no-members-row"><td colspan="9" style="text-align:center;color:var(--bs-secondary-color,#6c757d);padding:1.5rem;">';
                html += '<i class="fas fa-info-circle"></i> No sound cards added to this group yet';
                html += '</td></tr>';
            }

            html += '</tbody>';
            html += '</table>';

            html += '<div style="margin-top:0.75rem; display:flex; gap:0.5rem; align-items:center;">';
            html += '<button class="buttons btn-outline-success btn-group-action" onclick="AddMember(' + index + ')">';
            html += '<i class="fas fa-plus"></i> Add Sound Card</button>';
            html += '<button class="buttons btn-outline-warning btn-group-action" id="sync-cal-btn-' + index + '" onclick="ToggleSyncCalibration(' + index + ')">';
            html += '<i class="fas fa-stopwatch"></i> Sync Calibration</button>';
            html += '</div>';

            // Sync calibration panel (hidden by default)
            html += '<div id="sync-cal-panel-' + index + '" class="sync-cal-panel" style="display:none;">';
            html += '<div class="sync-cal-header">';
            html += '<strong><i class="fas fa-stopwatch"></i> Sync Calibration Mode</strong>';
            html += '<span class="sync-cal-desc">A metronome click track plays through the group. Adjust each member\'s delay until all outputs sound simultaneous. ' +
                'Set the slowest output (usually AES67/network) to 0ms and add delay to the faster outputs.</span>';
            html += '</div>';
            html += '<div class="sync-cal-controls">';
            html += '<button class="buttons btn-outline-success btn-sm" id="sync-play-btn-' + index + '" onclick="StartCalibrationPlayback(' + index + ')">';
            html += '<i class="fas fa-play"></i> Play Click Track</button>';
            html += '<span style="margin: 0 10px; color: #999;">or</span>';
            html += '<select id="sync-media-select-' + index + '" class="form-control form-control-sm" style="display:inline-block; width:auto; max-width:300px; vertical-align:middle;">';
            html += '<option value="">— Select from library —</option>';
            html += '</select> ';
            html += '<button class="buttons btn-outline-info btn-sm" id="sync-media-btn-' + index + '" onclick="PlayCalibrationMedia(' + index + ')">';
            html += '<i class="fas fa-music"></i> Play</button>';
            html += '<button class="buttons btn-outline-danger btn-sm" onclick="StopCalibrationPlayback(' + index + ')">';
            html += '<i class="fas fa-stop"></i> Stop</button>';
            html += '</div>';
            html += '</div>';

            html += '</div>'; // group-body
            html += '</div>'; // group-card

            return html;
        }

        /////////////////////////////////////////////////////////////////////////////
        // Render a member row
        function RenderMemberRow(group, groupIndex, memberIndex) {
            var member = group.members[memberIndex];
            var cardSelect = BuildCardSelect(groupIndex, memberIndex, member.cardId, group.members);
            var channelMapping = BuildChannelMapping(group, groupIndex, memberIndex, member);
            var eqEnabled = member.eq && member.eq.enabled;

            var delayMs = member.delayMs || 0;

            var html = '<tr data-member="' + memberIndex + '">';
            html += '<td>' + (memberIndex + 1) + '</td>';
            html += '<td>' + cardSelect + '</td>';
            html += '<td>';
            html += '<select class="form-select form-select-sm" style="width:auto;display:inline-block;" onchange="UpdateMemberChannels(' + groupIndex + ',' + memberIndex + ', parseInt(this.value))">';
            var chOpts = [1, 2, 4, 6, 8];
            for (var c = 0; c < chOpts.length; c++) {
                var sel = (member.channels === chOpts[c]) ? ' selected' : '';
                html += '<option value="' + chOpts[c] + '"' + sel + '>' + chOpts[c] + '</option>';
            }
            html += '</select>';
            html += '</td>';
            html += '<td>' + channelMapping;
            // EQ toggle button
            html += '<div style="margin-top:0.35rem;">';
            html += '<button id="eq-btn-' + groupIndex + '-' + memberIndex + '" class="btn btn-sm ' + (eqEnabled ? 'btn-info' : 'btn-outline-secondary') + ' eq-toggle-btn" ';
            html += 'onclick="ToggleEQPanel(' + groupIndex + ',' + memberIndex + ')">';
            html += '<i class="fas fa-sliders-h"></i> EQ</button>';
            html += '</div>';
            html += '</td>';
            html += '<td>';
            html += '<div class="volume-slider-container">';
            html += '<input type="range" class="form-range" min="0" max="100" value="' + (member.volume ?? 100) + '" oninput="UpdateGroupVolumeDisplay(this); ScheduleMemberVolume(' + groupIndex + ',' + memberIndex + ', this.value)" style="min-width:80px;">';
            html += '<span class="volume-value">' + (member.volume ?? 100) + '%</span>';
            html += '</div>';
            html += '</td>';
            html += '<td>';
            html += '<div class="delay-control">';
            html += '<input type="number" class="form-control form-control-sm" min="0" max="5000" step="1" value="' + delayMs + '" ';
            html += 'style="width:75px;display:inline-block;" ';
            html += 'onchange="UpdateMemberDelay(' + groupIndex + ',' + memberIndex + ', parseFloat(this.value))" ';
            html += 'oninput="ScheduleDelayUpdate(' + groupIndex + ',' + memberIndex + ', parseFloat(this.value))">';
            html += '</div>';
            html += '</td>';
            // Sample rate
            html += '<td>';
            var curRate = member.sampleRate || 0;
            html += '<select class="form-select form-select-sm" style="width:auto;" onchange="UpdateMemberSampleRate(' + groupIndex + ',' + memberIndex + ', parseInt(this.value))">';
            var rateOpts = [{ v: 0, l: 'Auto' }, { v: 44100, l: '44100' }, { v: 48000, l: '48000' }, { v: 96000, l: '96000' }];
            for (var ri = 0; ri < rateOpts.length; ri++) {
                html += '<option value="' + rateOpts[ri].v + '"' + (curRate === rateOpts[ri].v ? ' selected' : '') + '>' + rateOpts[ri].l + '</option>';
            }
            html += '</select>';
            html += '</td>';
            // Period size
            html += '<td>';
            var curPeriod = member.periodSize || 0;
            html += '<select class="form-select form-select-sm" style="width:auto;" onchange="UpdateMemberPeriodSize(' + groupIndex + ',' + memberIndex + ', parseInt(this.value))">';
            var periodOpts = [{ v: 0, l: 'Auto' }, { v: 1024, l: '1024' }, { v: 1536, l: '1536' }, { v: 2048, l: '2048' }, { v: 2560, l: '2560' }, { v: 3072, l: '3072' }, { v: 4096, l: '4096' }, { v: 8192, l: '8192' }];
            for (var pi = 0; pi < periodOpts.length; pi++) {
                html += '<option value="' + periodOpts[pi].v + '"' + (curPeriod === periodOpts[pi].v ? ' selected' : '') + '>' + periodOpts[pi].l + '</option>';
            }
            html += '</select>';
            html += '</td>';
            html += '<td>';
            html += '<button class="buttons btn-outline-danger btn-group-action" onclick="RemoveMember(' + groupIndex + ',' + memberIndex + ')" title="Remove"><i class="fas fa-times"></i></button>';
            html += '</td>';
            html += '</tr>';

            // EQ panel row (expandable)
            html += '<tr id="eq-panel-row-' + groupIndex + '-' + memberIndex + '" style="display:none;">';
            html += '<td colspan="9">';
            html += '<div id="eq-panel-content-' + groupIndex + '-' + memberIndex + '">';
            html += BuildEQPanel(groupIndex, memberIndex, member);
            html += '</div>';
            html += '</td></tr>';

            return html;
        }

        /////////////////////////////////////////////////////////////////////////////
        // Build card select dropdown
        // members: array of group members — cards already used by other members are excluded
        function BuildCardSelect(groupIndex, memberIndex, selectedCardId, members) {
            // Collect cardIds already assigned to other members in this group
            var usedCards = {};
            if (members) {
                for (var m = 0; m < members.length; m++) {
                    if (m !== memberIndex && members[m].cardId) {
                        usedCards[members[m].cardId] = true;
                    }
                }
            }

            var html = '<select class="form-select form-select-sm" style="width:auto;display:inline-block;" ';
            html += 'onchange="UpdateMemberCard(' + groupIndex + ',' + memberIndex + ', this.value)">';
            html += '<option value="">-- Select Card --</option>';

            var hasAlsa = false, hasAES67 = false;
            for (var i = 0; i < availableCards.length; i++) {
                if (availableCards[i].cardId in usedCards) continue;
                if (availableCards[i].isAES67) hasAES67 = true;
                else hasAlsa = true;
            }

            if (hasAlsa && hasAES67) html += '<optgroup label="Physical Sound Cards">';
            for (var i = 0; i < availableCards.length; i++) {
                var card = availableCards[i];
                if (card.isAES67) continue;
                if (card.cardId in usedCards) continue;
                var sel = (card.cardId === selectedCardId) ? ' selected' : '';
                var label = EscapeHtml(card.cardName) + ' [' + EscapeHtml(card.cardId) + ']';
                if (card.byPath) label += ' (' + EscapeHtml(card.byPath) + ')';
                // Prepend user-defined alias (issue #2586) if set.
                if (card.alias) label = EscapeHtml(card.alias) + ' — ' + label;
                html += '<option value="' + EscapeAttr(card.cardId) + '"' + sel + '>' + label + '</option>';
            }
            if (hasAlsa && hasAES67) html += '</optgroup>';

            if (hasAES67) {
                html += '<optgroup label="AES67 Network Sinks">';
                for (var i = 0; i < availableCards.length; i++) {
                    var card = availableCards[i];
                    if (!card.isAES67) continue;
                    if (card.cardId in usedCards) continue;
                    var sel = (card.cardId === selectedCardId) ? ' selected' : '';
                    var label = '\u{1F310} ' + EscapeHtml(card.cardName);
                    if (card.multicastIP) label += ' (' + EscapeHtml(card.multicastIP) + ':' + card.port + ')';
                    html += '<option value="' + EscapeAttr(card.cardId) + '"' + sel + '>' + label + '</option>';
                }
                html += '</optgroup>';
            }

            html += '</select>';
            return html;
        }

        /////////////////////////////////////////////////////////////////////////////
        // Build channel mapping controls
        function BuildChannelMapping(group, groupIndex, memberIndex, member) {
            var memberCh = member.channels || 2;
            var groupCh = group.channels || 2;
            var groupPositions = CHANNEL_POSITIONS[groupCh] || CHANNEL_POSITIONS[2];
            var memberPositions = CHANNEL_POSITIONS[memberCh] || CHANNEL_POSITIONS[2];

            var mapping = member.channelMapping || null;

            var html = '<div class="channel-map-grid">';

            for (var c = 0; c < memberCh && c < memberPositions.length; c++) {
                var cardCh = memberPositions[c];
                // What group channel is this card channel mapped to?
                var mappedTo = cardCh; // Default: same position
                if (mapping && mapping.cardChannels && mapping.groupChannels) {
                    var idx = -1;
                    for (var j = 0; j < mapping.cardChannels.length; j++) {
                        if (mapping.cardChannels[j] === cardCh) {
                            idx = j;
                            break;
                        }
                    }
                    if (idx >= 0 && idx < mapping.groupChannels.length) {
                        mappedTo = mapping.groupChannels[idx];
                    }
                }

                html += '<div class="channel-map-item">';
                html += '<span title="Card channel">' + cardCh + '</span>';
                html += '<i class="fas fa-arrow-right" style="font-size:0.7rem;color:var(--bs-secondary-color,#999);"></i>';
                html += '<select class="form-select form-select-sm" style="width:auto;padding:0.1rem 1.5rem 0.1rem 0.3rem;" ';
                html += 'onchange="UpdateChannelMap(' + groupIndex + ',' + memberIndex + ',' + c + ', this.value)" ';
                html += 'title="Map card channel ' + cardCh + ' to group position">';

                for (var p = 0; p < groupPositions.length; p++) {
                    var sel = (groupPositions[p] === mappedTo) ? ' selected' : '';
                    html += '<option value="' + groupPositions[p] + '"' + sel + '>' + groupPositions[p] + '</option>';
                }
                // Also offer AUX positions for advanced setups
                html += '<option value="">-- None --</option>';
                html += '</select>';
                html += '</div>';
            }

            html += '</div>';
            return html;
        }

        /////////////////////////////////////////////////////////////////////////////
        // EQ Panel Builder & Management
        function BuildEQPanel(groupIndex, memberIndex, member) {
            var eq = member.eq || { enabled: false, bands: DefaultEQBands() };
            var enabledChecked = eq.enabled ? ' checked' : '';

            var html = '<div class="eq-panel">';
            html += '<div class="eq-header">';
            html += '<div class="eq-header-label">';
            html += '<label class="form-check-label">';
            html += '<input type="checkbox" class="form-check-input"' + enabledChecked;
            html += ' onchange="ToggleEQ(' + groupIndex + ',' + memberIndex + ',this.checked)">';
            html += ' Parametric EQ' + HelpIcon('Enables a per-card parametric equalizer using PipeWire filter-chain biquad filters. When enabled, audio passes through the EQ processing before reaching this sound card. Add bands to shape the frequency response. Changes apply in real-time once the group config has been applied at least once.') + '</label>';
            html += '</div>';
            html += '<div>';
            html += '<button class="btn btn-sm btn-outline-success" onclick="AddEQBand(' + groupIndex + ',' + memberIndex + ')" title="Add Band">';
            html += '<i class="fas fa-plus"></i> Band</button>';
            html += '</div>';
            html += '</div>';

            html += '<div class="eq-bands-container">';
            if (eq.bands && eq.bands.length > 0) {
                html += '<div class="eq-band-header">';
                html += '<span class="eq-col-num">#</span>';
                html += '<span class="eq-col-type">Type' + HelpIcon('Filter type for this EQ band:<br><b>Peaking</b> &ndash; boost/cut around a center frequency<br><b>Low Shelf</b> &ndash; boost/cut everything below a frequency<br><b>High Shelf</b> &ndash; boost/cut everything above<br><b>Low/High Pass</b> &ndash; remove frequencies beyond the cutoff<br><b>Notch</b> &ndash; narrow cut at a specific frequency<br><b>Bandpass</b> &ndash; pass only a narrow band<br><b>Allpass</b> &ndash; phase shift without gain change') + '</span>';
                html += '<span class="eq-col-freq">Freq (Hz)' + HelpIcon('Center or cutoff frequency for this band, in Hertz (20–20,000). For peaking filters this is the center of the boost/cut. For shelves it is the transition frequency.') + '</span>';
                html += '<span class="eq-col-gain">Gain (dB)' + HelpIcon('Amount of boost (positive) or cut (negative) in decibels. Range: -24 dB to +24 dB. Has no effect on Low Pass, High Pass, Notch, Bandpass, or Allpass filter types.') + '</span>';
                html += '<span class="eq-col-q">Q' + HelpIcon('Quality factor controlling the bandwidth of the filter. Higher Q = narrower bandwidth. Typical values: 0.5–2.0 for broad shaping, 5–30 for surgical cuts. For shelves, lower Q gives a gentler slope.') + '</span>';
                html += '<span class="eq-col-action"></span>';
                html += '</div>';
                for (var b = 0; b < eq.bands.length; b++) {
                    html += BuildEQBandRow(groupIndex, memberIndex, b, eq.bands[b]);
                }
            } else {
                html += '<div style="text-align:center;color:var(--bs-secondary-color,#6c757d);padding:0.5rem;">No EQ bands &mdash; click "+ Band" to add</div>';
            }
            html += '</div>';
            html += '</div>';
            return html;
        }

        function BuildEQBandRow(groupIndex, memberIndex, bandIndex, band) {
            var html = '<div class="eq-band-row">';
            html += '<span class="eq-col-num">' + (bandIndex + 1) + '</span>';

            // Type dropdown
            html += '<span class="eq-col-type"><select class="form-select form-select-sm" ';
            html += 'onchange="UpdateEQBand(' + groupIndex + ',' + memberIndex + ',' + bandIndex + ',\'type\',this.value)">';
            for (var t = 0; t < EQ_BAND_TYPES.length; t++) {
                var sel = (band.type === EQ_BAND_TYPES[t].value) ? ' selected' : '';
                html += '<option value="' + EQ_BAND_TYPES[t].value + '"' + sel + '>' + EQ_BAND_TYPES[t].label + '</option>';
            }
            html += '</select></span>';

            // Frequency
            html += '<span class="eq-col-freq"><input type="number" class="form-control form-control-sm" ';
            html += 'min="20" max="20000" step="1" value="' + (band.freq || 1000) + '" ';
            html += 'onchange="UpdateEQBand(' + groupIndex + ',' + memberIndex + ',' + bandIndex + ',\'freq\',parseFloat(this.value))" ';
            html += 'style="width:80px;"></span>';

            // Gain slider
            html += '<span class="eq-col-gain"><div class="eq-gain-slider">';
            html += '<input type="range" min="-24" max="24" step="0.5" value="' + (band.gain || 0) + '" ';
            html += 'oninput="UpdateEQGainDisplay(this); UpdateEQBand(' + groupIndex + ',' + memberIndex + ',' + bandIndex + ',\'gain\',parseFloat(this.value))">';
            html += '<span class="eq-gain-value">' + FormatGain(band.gain) + '</span>';
            html += '</div></span>';

            // Q factor
            html += '<span class="eq-col-q"><input type="number" class="form-control form-control-sm" ';
            html += 'min="0.1" max="30" step="0.1" value="' + (band.q || 1.0) + '" ';
            html += 'onchange="UpdateEQBand(' + groupIndex + ',' + memberIndex + ',' + bandIndex + ',\'q\',parseFloat(this.value))" ';
            html += 'style="width:65px;"></span>';

            // Remove button
            html += '<span class="eq-col-action"><button class="btn btn-sm btn-outline-danger" ';
            html += 'onclick="RemoveEQBand(' + groupIndex + ',' + memberIndex + ',' + bandIndex + ')" title="Remove Band">';
            html += '<i class="fas fa-times"></i></button></span>';

            html += '</div>';
            return html;
        }

        function ToggleEQPanel(groupIndex, memberIndex) {
            var row = $('#eq-panel-row-' + groupIndex + '-' + memberIndex);
            row.toggle();
        }

        function ToggleEQ(groupIndex, memberIndex, enabled) {
            var member = audioGroups.groups[groupIndex].members[memberIndex];
            if (!member.eq) {
                member.eq = { enabled: enabled, bands: DefaultEQBands() };
            } else {
                member.eq.enabled = enabled;
            }
            var btn = $('#eq-btn-' + groupIndex + '-' + memberIndex);
            if (enabled) {
                btn.removeClass('btn-outline-secondary').addClass('btn-info');
            } else {
                btn.removeClass('btn-info').addClass('btn-outline-secondary');
            }
        }

        function AddEQBand(groupIndex, memberIndex) {
            var member = audioGroups.groups[groupIndex].members[memberIndex];
            if (!member.eq) {
                member.eq = { enabled: false, bands: DefaultEQBands() };
            }
            member.eq.bands.push({ type: 'bq_peaking', freq: 1000, gain: 0, q: 1.0 });
            RefreshEQPanel(groupIndex, memberIndex);
        }

        function RemoveEQBand(groupIndex, memberIndex, bandIndex) {
            var member = audioGroups.groups[groupIndex].members[memberIndex];
            if (member.eq && member.eq.bands) {
                member.eq.bands.splice(bandIndex, 1);
            }
            RefreshEQPanel(groupIndex, memberIndex);
        }

        // Debounce timer for real-time EQ updates
        var eqUpdateTimers = {};

        function UpdateEQBand(groupIndex, memberIndex, bandIndex, field, value) {
            var member = audioGroups.groups[groupIndex].members[memberIndex];
            if (member.eq && member.eq.bands && member.eq.bands[bandIndex]) {
                member.eq.bands[bandIndex][field] = value;
            }
            // Debounced real-time push to running filter-chain
            ScheduleEQUpdate(groupIndex, memberIndex);
        }

        function UpdateEQGainDisplay(slider) {
            var val = parseFloat(slider.value);
            $(slider).siblings('.eq-gain-value').text(FormatGain(val));
        }

        // Push EQ params to the running PipeWire filter-chain in real time.
        // Debounced so rapid slider movements don't flood the API.
        function ScheduleEQUpdate(groupIndex, memberIndex) {
            var key = groupIndex + '_' + memberIndex;
            if (eqUpdateTimers[key]) clearTimeout(eqUpdateTimers[key]);
            eqUpdateTimers[key] = setTimeout(function () {
                SendEQUpdate(groupIndex, memberIndex);
            }, 80);
        }

        function SendEQUpdate(groupIndex, memberIndex) {
            var group = audioGroups.groups[groupIndex];
            var member = group.members[memberIndex];
            if (!member.eq || !member.eq.enabled || !member.eq.bands || !member.eq.bands.length) return;
            if (!member.cardId) return;

            $.ajax({
                url: 'api/pipewire/audio/eq/update',
                method: 'POST',
                contentType: 'application/json',
                data: JSON.stringify({
                    groupId: group.id,
                    cardId: member.cardId,
                    channels: member.channels || 2,
                    bands: member.eq.bands
                }),
                success: function (resp) {
                    if (resp && resp.status === 'NOT_RUNNING') {
                        // Filter not active yet — silent, user needs to Apply
                    }
                },
                error: function () {
                    // Silent — best-effort real-time preview
                }
            });
        }

        function FormatGain(gain) {
            var val = parseFloat(gain || 0);
            return (val >= 0 ? '+' : '') + val.toFixed(1);
        }

        function RefreshEQPanel(groupIndex, memberIndex) {
            var member = audioGroups.groups[groupIndex].members[memberIndex];
            var content = BuildEQPanel(groupIndex, memberIndex, member);
            $('#eq-panel-content-' + groupIndex + '-' + memberIndex).html(content);
            InitTooltips();
        }

        /////////////////////////////////////////////////////////////////////////////
        // Group management functions
        function AddGroup() {
            var group = {
                id: nextGroupId++,
                name: "Audio Group " + (audioGroups.groups.length + 1),
                enabled: true,
                channels: 2,
                latencyCompensate: false,
                volume: 100,
                members: []
            };
            audioGroups.groups.push(group);
            RenderGroups();
        }

        function DeleteGroup(index) {
            if (!confirm('Delete group "' + audioGroups.groups[index].name + '"?')) return;
            audioGroups.groups.splice(index, 1);
            RenderGroups();
        }

        function ToggleGroupEnabled(index, enabled) {
            audioGroups.groups[index].enabled = enabled;
            var card = $('#group-' + audioGroups.groups[index].id);
            if (enabled) {
                card.removeClass('disabled-group');
            } else {
                card.addClass('disabled-group');
            }
        }

        function UpdateGroupName(index, name) {
            audioGroups.groups[index].name = name;
            // Update the node name badge
            var card = $('#group-' + audioGroups.groups[index].id);
            card.find('.pipewire-badge').text('Combine Sink: fpp_group_' + EscapeNodeName(name));
        }

        function UpdateGroupChannels(index, channels) {
            audioGroups.groups[index].channels = channels;
            // Re-render to update channel mapping dropdowns
            RenderGroups();
        }

        function UpdateGroupLatency(index, enabled) {
            audioGroups.groups[index].latencyCompensate = enabled;
        }

        function UpdateGroupVolumeDisplay(slider) {
            $(slider).siblings('.volume-value').text(slider.value + '%');
        }

        /////////////////////////////////////////////////////////////////////////////
        // Member management functions
        function AddMember(groupIndex) {
            var member = {
                cardId: "",
                cardName: "",
                channels: 2,
                volume: 100,
                delayMs: 0,
                sampleRate: 0,
                periodSize: 0,
                channelMapping: null,
                eq: { enabled: false, bands: DefaultEQBands() }
            };
            audioGroups.groups[groupIndex].members.push(member);
            RenderGroups();
        }

        function RemoveMember(groupIndex, memberIndex) {
            audioGroups.groups[groupIndex].members.splice(memberIndex, 1);
            RenderGroups();
        }

        function UpdateMemberCard(groupIndex, memberIndex, cardId) {
            var member = audioGroups.groups[groupIndex].members[memberIndex];
            member.cardId = cardId;

            // Find card info and auto-set channels
            for (var i = 0; i < availableCards.length; i++) {
                if (availableCards[i].cardId === cardId) {
                    member.cardName = availableCards[i].cardName;
                    member.channels = Math.min(availableCards[i].channels, 2); // Default to stereo
                    break;
                }
            }

            // Reset channel mapping
            member.channelMapping = BuildDefaultChannelMapping(member.channels, audioGroups.groups[groupIndex].channels);
            RenderGroups();
        }

        function UpdateMemberChannels(groupIndex, memberIndex, channels) {
            var member = audioGroups.groups[groupIndex].members[memberIndex];
            member.channels = channels;
            member.channelMapping = BuildDefaultChannelMapping(channels, audioGroups.groups[groupIndex].channels);
            RenderGroups();
        }

        function UpdateChannelMap(groupIndex, memberIndex, channelIndex, groupPosition) {
            var member = audioGroups.groups[groupIndex].members[memberIndex];
            var memberCh = member.channels || 2;
            var memberPositions = CHANNEL_POSITIONS[memberCh] || CHANNEL_POSITIONS[2];

            if (!member.channelMapping) {
                member.channelMapping = BuildDefaultChannelMapping(memberCh, audioGroups.groups[groupIndex].channels);
            }

            if (channelIndex < member.channelMapping.groupChannels.length) {
                member.channelMapping.groupChannels[channelIndex] = groupPosition;
            }
        }

        function BuildDefaultChannelMapping(memberChannels, groupChannels) {
            var memberPositions = CHANNEL_POSITIONS[memberChannels] || CHANNEL_POSITIONS[2];
            var groupPositions = CHANNEL_POSITIONS[groupChannels] || CHANNEL_POSITIONS[2];

            var cardChannels = [];
            var grpChannels = [];

            for (var i = 0; i < memberPositions.length; i++) {
                cardChannels.push(memberPositions[i]);
                // Map to same position if available in group, otherwise first available
                if (groupPositions.indexOf(memberPositions[i]) >= 0) {
                    grpChannels.push(memberPositions[i]);
                } else {
                    grpChannels.push(groupPositions[i % groupPositions.length]);
                }
            }

            return {
                cardChannels: cardChannels,
                groupChannels: grpChannels
            };
        }

        /////////////////////////////////////////////////////////////////////////////
        // Volume control — debounced for real-time slider tracking
        var volumeTimers = {};

        function ScheduleGroupVolume(groupIndex, volume) {
            audioGroups.groups[groupIndex].volume = parseInt(volume);
            var key = 'g_' + groupIndex;
            if (volumeTimers[key]) clearTimeout(volumeTimers[key]);
            volumeTimers[key] = setTimeout(function () {
                SetGroupVolume(groupIndex, volume);
            }, 60);
        }

        function ScheduleMemberVolume(groupIndex, memberIndex, volume) {
            audioGroups.groups[groupIndex].members[memberIndex].volume = parseInt(volume);
            var key = 'm_' + groupIndex + '_' + memberIndex;
            if (volumeTimers[key]) clearTimeout(volumeTimers[key]);
            volumeTimers[key] = setTimeout(function () {
                SetMemberVolume(groupIndex, memberIndex, volume);
            }, 60);
        }

        function SetGroupVolume(groupIndex, volume) {
            audioGroups.groups[groupIndex].volume = parseInt(volume);
            var group = audioGroups.groups[groupIndex];
            var sinkName = 'fpp_group_' + EscapeNodeName(group.name);
            SendVolumeCommand(sinkName, volume);
        }

        function SetMemberVolume(groupIndex, memberIndex, volume) {
            audioGroups.groups[groupIndex].members[memberIndex].volume = parseInt(volume);
            var group = audioGroups.groups[groupIndex];
            var member = group.members[memberIndex];
            if (member.cardId) {
                // Target the filter-chain node (fpp_fx_g<groupId>_<cardId>) which sits
                // in the actual audio chain.  This avoids touching the WirePlumber
                // auto-detected ALSA sink whose volume propagates to the hardware
                // mixer and can zero it out.
                var fxNodeName = 'fpp_fx_g' + group.id + '_' + EscapeNodeName(member.cardId);
                SendVolumeCommand(fxNodeName, volume, function () {
                    // Filter-chain not running yet (pre-Apply) — fall back to
                    // the raw PipeWire sink (ALSA mode / legacy behaviour)
                    var sinkName = null;
                    for (var i = 0; i < availableCards.length; i++) {
                        if (availableCards[i].cardId === member.cardId) {
                            sinkName = availableCards[i].pwNodeName || null;
                            break;
                        }
                    }
                    if (sinkName) {
                        SendVolumeCommand(sinkName, volume);
                    }
                });
            }
        }

        function SendVolumeCommand(sinkName, volume, fallback) {
            $.ajax({
                url: 'api/pipewire/audio/group/volume',
                method: 'POST',
                contentType: 'application/json',
                data: JSON.stringify({ sink: sinkName, volume: parseInt(volume) }),
                error: function (xhr) {
                    console.warn('Volume command failed for ' + sinkName);
                    if (typeof fallback === 'function') fallback();
                }
            });
        }

        /////////////////////////////////////////////////////////////////////////////
        // Per-member delay compensation — debounced for real-time adjustment
        var delayTimers = {};

        function UpdateMemberSampleRate(groupIndex, memberIndex, rate) {
            audioGroups.groups[groupIndex].members[memberIndex].sampleRate = rate;
        }

        function UpdateMemberPeriodSize(groupIndex, memberIndex, periodSize) {
            audioGroups.groups[groupIndex].members[memberIndex].periodSize = periodSize;
        }

        function UpdateMemberDelay(groupIndex, memberIndex, delayMs) {
            delayMs = Math.max(0, Math.min(5000, parseInt(delayMs) || 0));
            audioGroups.groups[groupIndex].members[memberIndex].delayMs = delayMs;
        }

        function ScheduleDelayUpdate(groupIndex, memberIndex, delayMs) {
            delayMs = Math.max(0, Math.min(5000, parseInt(delayMs) || 0));
            audioGroups.groups[groupIndex].members[memberIndex].delayMs = delayMs;
            var key = 'd_' + groupIndex + '_' + memberIndex;
            if (delayTimers[key]) clearTimeout(delayTimers[key]);
            delayTimers[key] = setTimeout(function () {
                SendDelayUpdate(groupIndex, memberIndex, delayMs);
            }, 80);
        }

        function SendDelayUpdate(groupIndex, memberIndex, delayMs) {
            var group = audioGroups.groups[groupIndex];
            var member = group.members[memberIndex];
            if (!member.cardId) return;

            $.ajax({
                url: 'api/pipewire/audio/delay/update',
                method: 'POST',
                contentType: 'application/json',
                data: JSON.stringify({
                    groupId: group.id,
                    cardId: member.cardId,
                    delayMs: delayMs,
                    channels: member.channels || 2
                }),
                success: function (resp) {
                    if (resp && resp.status === 'NOT_RUNNING') {
                        // Filter not active yet — silent, user needs to Apply
                    }
                },
                error: function () {
                    // Silent — best-effort real-time preview
                }
            });
        }

        /////////////////////////////////////////////////////////////////////////////
        // Sync Calibration Mode
        var syncCalActive = {};

        function ToggleSyncCalibration(groupIndex) {
            var panel = $('#sync-cal-panel-' + groupIndex);
            var btn = $('#sync-cal-btn-' + groupIndex);
            if (panel.is(':visible')) {
                // Closing — stop playback
                StopCalibrationPlayback(groupIndex);
                panel.slideUp(200);
                btn.removeClass('btn-warning').addClass('btn-outline-warning');
            } else {
                // Populate media selector if not already done
                var sel = $('#sync-media-select-' + groupIndex);
                if (sel.find('option').length <= 1) {
                    $.getJSON('api/media', function (files) {
                        files.forEach(function (f) {
                            if (/\.(mp3|wav|ogg|flac|m4a|aac|wma)$/i.test(f)) {
                                sel.append($('<option>').val(f).text(f));
                            }
                        });
                    });
                }
                panel.slideDown(200);
                btn.removeClass('btn-outline-warning').addClass('btn-warning');
            }
        }

        function StartCalibrationPlayback(groupIndex) {
            var playBtn = $('#sync-play-btn-' + groupIndex);
            playBtn.prop('disabled', true).html('<i class="fas fa-spinner fa-spin"></i> Starting...');

            $.ajax({
                url: 'api/pipewire/audio/sync/start',
                method: 'POST',
                contentType: 'application/json',
                data: JSON.stringify({ groupIndex: groupIndex }),
                success: function (resp) {
                    syncCalActive[groupIndex] = true;
                    playBtn.prop('disabled', false).html('<i class="fas fa-play"></i> Playing...');
                    $.jGrowl('Click track playing — adjust delay sliders until all outputs are in sync', { themeState: 'highlight' });
                },
                error: function (xhr) {
                    playBtn.prop('disabled', false).html('<i class="fas fa-play"></i> Play Click Track');
                    DialogError('Calibration Error', 'Failed to start click track: ' + (xhr.responseText || 'Unknown error'));
                }
            });
        }

        function PlayCalibrationMedia(groupIndex) {
            var sel = $('#sync-media-select-' + groupIndex);
            var file = sel.val();
            if (!file) {
                $.jGrowl('Select a media file first', { themeState: 'highlight' });
                return;
            }
            // Stop anything currently playing
            StopCalibrationPlaybackSilent(groupIndex);

            var mediaBtn = $('#sync-media-btn-' + groupIndex);
            mediaBtn.prop('disabled', true).html('<i class="fas fa-spinner fa-spin"></i>');
            syncCalActive[groupIndex] = file;

            // Route through our group-aware backend so the audio actually
            // plays out of THIS group's combine sink (not fppd's default).
            $.ajax({
                url: 'api/pipewire/audio/sync/start',
                method: 'POST',
                contentType: 'application/json',
                data: JSON.stringify({ groupIndex: groupIndex, mediaFile: file }),
                success: function (resp) {
                    if (resp && resp.status === 'OK') {
                        mediaBtn.prop('disabled', false).html('<i class="fas fa-music"></i> Playing...');
                        $.jGrowl('Playing on this group: ' + file, { themeState: 'highlight' });
                    } else {
                        mediaBtn.prop('disabled', false).html('<i class="fas fa-music"></i> Play');
                        DialogError('Playback Error', (resp && resp.message) ? resp.message : 'Failed to play media');
                    }
                },
                error: function (xhr) {
                    mediaBtn.prop('disabled', false).html('<i class="fas fa-music"></i> Play');
                    DialogError('Playback Error', 'Failed to play: ' + file + ' — ' + (xhr.responseText || 'Unknown error'));
                }
            });
        }

        function StopCalibrationPlayback(groupIndex) {
            StopCalibrationPlaybackSilent(groupIndex);
            $.jGrowl('Calibration playback stopped', { themeState: 'highlight' });
        }

        function StopCalibrationPlaybackSilent(groupIndex) {
            // Stop click track / media playback via our group-aware API
            $.ajax({
                url: 'api/pipewire/audio/sync/stop',
                method: 'POST',
                contentType: 'application/json',
                data: JSON.stringify({})
            });
            syncCalActive[groupIndex] = false;
            $('#sync-play-btn-' + groupIndex).html('<i class="fas fa-play"></i> Play Click Track');
            $('#sync-media-btn-' + groupIndex).html('<i class="fas fa-music"></i> Play');
        }

        /////////////////////////////////////////////////////////////////////////////
        // Save / Apply
        function SaveGroups() {
            // Validate
            for (var i = 0; i < audioGroups.groups.length; i++) {
                var g = audioGroups.groups[i];
                if (!g.name || g.name.trim() === '') {
                    DialogError('Validation Error', 'Group ' + (i + 1) + ' must have a name.');
                    return;
                }
            }

            $.ajax({
                url: 'api/pipewire/audio/groups',
                method: 'POST',
                contentType: 'application/json',
                data: JSON.stringify(audioGroups),
                success: function (data) {
                    if (data && data.data) {
                        audioGroups = data.data;
                    }
                    $.jGrowl('Audio groups saved', { themeState: 'highlight' });
                },
                error: function (xhr) {
                    DialogError('Save Error', 'Failed to save audio groups: ' + (xhr.responseText || 'Unknown error'));
                }
            });
        }

        function ApplyGroups() {
            DoModalDialog({
                id: 'applyConfirmDialog',
                title: 'Save & Apply Audio Configuration',
                body: 'This will save the current configuration, restart PipeWire audio services, and restart FPPD.<br><br>' +
                    'Any currently playing audio will be interrupted briefly.',
                class: 'modal-sm',
                keyboard: true,
                backdrop: true,
                buttons: {
                    'Cancel': {
                        click: function () { CloseModalDialog('applyConfirmDialog'); },
                        class: 'btn-outline-secondary'
                    },
                    'Save & Apply': {
                        click: function () {
                            CloseModalDialog('applyConfirmDialog');
                            DoApplyGroups();
                        },
                        class: 'btn-success'
                    }
                }
            });
        }

        function DoApplyGroups() {
            // 1. Stop all calibration playback and close panels
            StopAllCalibration();

            // 2. Show progress overlay
            ShowApplyProgress('Saving configuration...');

            // 3. Save
            $.ajax({
                url: 'api/pipewire/audio/groups',
                method: 'POST',
                contentType: 'application/json',
                data: JSON.stringify(audioGroups),
                success: function (data) {
                    if (data && data.data) {
                        audioGroups = data.data;
                    }
                    ShowApplyProgress('Applying PipeWire configuration...');

                    // 4. Apply (generates PipeWire configs and restarts services)
                    $.ajax({
                        url: 'api/pipewire/audio/groups/apply',
                        method: 'POST',
                        success: function (applyData) {
                            ShowApplyProgress('Restarting FPPD...');

                            // 5. Auto-restart FPPD
                            $.ajax({
                                url: 'api/system/fppd/restart',
                                method: 'GET',
                                success: function () {
                                    ShowApplyProgress('Waiting for FPPD to come back online...');
                                    WaitForFPPD(function () {
                                        HideApplyProgress();
                                        $.jGrowl('Audio configuration applied and FPPD restarted successfully', { themeState: 'highlight' });
                                        setTimeout(function () { CheckPipeWireStatus(); }, 1000);
                                    });
                                },
                                error: function () {
                                    // Restart request may "fail" because fppd is shutting down
                                    ShowApplyProgress('Waiting for FPPD to come back online...');
                                    WaitForFPPD(function () {
                                        HideApplyProgress();
                                        $.jGrowl('Audio configuration applied and FPPD restarted', { themeState: 'highlight' });
                                        setTimeout(function () { CheckPipeWireStatus(); }, 1000);
                                    });
                                }
                            });
                        },
                        error: function (xhr) {
                            HideApplyProgress();
                            DialogError('Apply Error', 'Failed to apply PipeWire configuration: ' + (xhr.responseText || 'Unknown error'));
                        }
                    });
                },
                error: function (xhr) {
                    HideApplyProgress();
                    DialogError('Save Error', 'Failed to save audio groups: ' + (xhr.responseText || 'Unknown error'));
                }
            });
        }

        function StopAllCalibration() {
            // Stop any active calibration playback across all groups
            for (var i = 0; i < audioGroups.groups.length; i++) {
                if (syncCalActive[i]) {
                    StopCalibrationPlaybackSilent(i);
                }
                // Close calibration panels
                var panel = $('#sync-cal-panel-' + i);
                if (panel.is(':visible')) {
                    panel.hide();
                    $('#sync-cal-btn-' + i).removeClass('btn-warning').addClass('btn-outline-warning');
                }
            }
        }

        function ShowApplyProgress(message) {
            var overlay = $('#apply-progress-overlay');
            if (overlay.length === 0) {
                overlay = $('<div id="apply-progress-overlay" style="' +
                    'position:fixed;top:0;left:0;right:0;bottom:0;' +
                    'background:rgba(0,0,0,0.6);z-index:9999;' +
                    'display:flex;align-items:center;justify-content:center;">' +
                    '<div style="background:var(--bs-body-bg,#fff);border-radius:8px;padding:2rem 2.5rem;text-align:center;max-width:400px;">' +
                    '<div><i class="fas fa-spinner fa-spin fa-2x" style="color:var(--bs-primary,#0d6efd);"></i></div>' +
                    '<div id="apply-progress-msg" style="margin-top:1rem;font-size:1.1rem;color:var(--bs-body-color,#333);"></div>' +
                    '</div></div>');
                $('body').append(overlay);
            }
            $('#apply-progress-msg').text(message);
            overlay.show();
        }

        function HideApplyProgress() {
            $('#apply-progress-overlay').remove();
        }

        function WaitForFPPD(callback, attempts) {
            attempts = attempts || 0;
            if (attempts > 30) { // 30 seconds max wait
                HideApplyProgress();
                $.jGrowl('FPPD restart is taking longer than expected — check status manually', { themeState: 'highlight' });
                return;
            }
            $.ajax({
                url: 'api/fppd/status',
                method: 'GET',
                timeout: 2000,
                success: function (data) {
                    if (data && data.status_name && data.status_name !== '') {
                        // FPPD is responding
                        callback();
                    } else {
                        setTimeout(function () { WaitForFPPD(callback, attempts + 1); }, 1000);
                    }
                },
                error: function () {
                    setTimeout(function () { WaitForFPPD(callback, attempts + 1); }, 1000);
                }
            });
        }

        /////////////////////////////////////////////////////////////////////////////
        // Utility functions
        function ChannelLayoutName(ch) {
            switch (ch) {
                case 1: return 'Mono';
                case 2: return 'Stereo';
                case 4: return 'Quad';
                case 6: return '5.1';
                case 8: return '7.1';
                default: return ch + 'ch';
            }
        }

        function EscapeNodeName(name) {
            return name.toLowerCase().replace(/[^a-z0-9_]/g, '_');
        }

        function RestartFPPD() {
            $.jGrowl('Restarting FPPD...', { themeState: 'highlight' });
            $.ajax({
                url: 'api/system/fppd/restart',
                method: 'GET',
                success: function () {
                    $.jGrowl('FPPD restarting — audio will use the new output group', { themeState: 'highlight' });
                    setTimeout(function () { CheckPipeWireStatus(); }, 5000);
                },
                error: function () {
                    $.jGrowl('FPPD restart requested', { themeState: 'highlight' });
                }
            });
        }

        function EscapeHtml(str) {
            if (!str) return '';
            return str.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
        }

        function EscapeAttr(str) {
            if (!str) return '';
            return str.replace(/&/g, '&amp;').replace(/"/g, '&quot;').replace(/'/g, '&#39;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
        }
    </script>
    </body>

</html>