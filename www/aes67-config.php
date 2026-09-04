<!DOCTYPE html>
<html lang="en">

<head>
    <?php
    include 'common/htmlMeta.inc';
    require_once "common.php";
    require_once 'config.php';
    include 'common/menuHead.inc';
    ?>

    <title><? echo $pageTitle; ?> - AES67 Audio-over-IP Instances</title>

    <?php $modalMode = isset($_GET['modal']) && $_GET['modal'] == '1'; ?>

    <style>
        .instance-card {
            border: 1px solid var(--bs-border-color, #dee2e6);
            border-radius: 8px;
            margin-bottom: 1.5rem;
            background: var(--bs-body-bg, #fff);
        }

        .instance-card.disabled-instance {
            opacity: 0.6;
        }

        .instance-header {
            display: flex;
            align-items: center;
            gap: 0.75rem;
            padding: 0.75rem 1rem;
            background: var(--bs-tertiary-bg, #f8f9fa);
            border-bottom: 1px solid var(--bs-border-color, #dee2e6);
            border-radius: 8px 8px 0 0;
        }

        .instance-name-input {
            border: 1px solid transparent;
            background: transparent;
            font-weight: 600;
            font-size: 1.1rem;
            padding: 0.15rem 0.5rem;
            border-radius: 4px;
            min-width: 200px;
            color: var(--bs-body-color, #212529);
        }

        .instance-name-input:focus {
            border-color: var(--bs-primary, #0d6efd);
            outline: none;
            background: var(--bs-body-bg, #fff);
        }

        .instance-body {
            padding: 1rem;
        }

        .instance-settings {
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(280px, 1fr));
            gap: 0.75rem 1.5rem;
        }

        .instance-settings label {
            font-weight: 500;
            font-size: 0.9rem;
            display: block;
            margin-bottom: 0.25rem;
        }

        .instance-settings .form-text {
            font-size: 0.78rem;
        }

        .aes67-badge {
            font-size: 0.75rem;
            font-weight: normal;
        }

        .no-instances-msg {
            text-align: center;
            padding: 3rem 1rem;
            color: var(--bs-secondary-color, #6c757d);
        }

        .no-instances-msg i {
            font-size: 3rem;
            margin-bottom: 1rem;
            display: block;
        }

        .btn-instance-action {
            padding: 0.25rem 0.5rem;
            font-size: 0.8rem;
            border-radius: 4px;
        }

        .status-indicator {
            display: inline-block;
            width: 10px;
            height: 10px;
            border-radius: 50%;
            margin-right: 6px;
            vertical-align: middle;
        }

        .status-running {
            background: #28a745;
        }

        .status-stopped {
            background: #dc3545;
        }

        .ptp-settings {
            border: 1px solid var(--bs-border-color, #dee2e6);
            border-radius: 8px;
            padding: 1rem;
            margin-bottom: 1.5rem;
            background: var(--bs-tertiary-bg, #f8f9fa);
        }

        .ptp-settings h5 {
            margin-bottom: 0.75rem;
            font-size: 1rem;
        }

        /* Help icon sizing */
        .instance-settings .icon-help {
            width: 20px;
            height: 20px;
            padding-left: 2px;
            vertical-align: middle;
            cursor: help;
        }

        .ptp-settings .icon-help {
            width: 20px;
            height: 20px;
            padding-left: 2px;
            vertical-align: middle;
            cursor: help;
        }

        /* Busy overlay: everything else about it is Bootstrap utilities, but
           there is no z-index utility that clears a Bootstrap modal, and this
           page raises its own modals to 99999 when embedded (below).  The
           overlay has to cover those too, or it is invisible in modal mode. */
        .aes67-busy-overlay {
            z-index: 100000;
        }

        <?php if ($modalMode) { ?>
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
    echo ' class="modal-mode"'; ?>>
    <div id="bodyWrapper">
        <?php
        // Only show full chrome when not in modal mode
        if (!$modalMode) {
            $activeParentMenuItem = 'status';
            include 'menu.inc';
            ?>
            <div class="mainContainer">
                <div class="title">PipeWire AES67 Audio-over-IP</div>

                <div class="pageContent">
                <?php } else { ?>
                    <div style="padding: 1rem;">
                    <?php } ?>

                    <!-- PipeWire status bar -->
                    <?php if (!isset($settings['MediaBackend']) || $settings['MediaBackend'] != 'pipewire') { ?>
                        <div class="callout callout-warning" style="padding:1rem;">
                            <h4>Advanced PipeWire Required</h4>
                            <p>AES67 audio-over-IP requires the Advanced PipeWire backend.<br>
                                Go to <b>Settings → Audio/Video</b> and set <b>Media Backend</b> to <b>PipeWire
                                    (Advanced)</b>,
                                then reboot.</p>
                        </div>
                    <?php } else { ?>

                        <div
                            style="display:flex; justify-content:space-between; align-items:center; margin-bottom:1rem; flex-wrap:wrap; gap:0.5rem;">
                            <div>
                                <span id="pwStatus">
                                    <span class="status-indicator status-stopped"></span>Checking PipeWire...
                                </span>
                                &nbsp;&nbsp;
                                <span id="ptpStatus"></span>
                            </div>
                            <div style="display:flex; gap:0.5rem;">
                                <button class="buttons btn-outline-success" onclick="AddInstance()">
                                    <i class="fas fa-plus"></i> Add AES67 Instance
                                </button>
                            </div>
                        </div>

                        <!-- Global PTP settings -->
                        <div class="ptp-settings" id="ptpSettings">
                            <h5><i class="fas fa-clock"></i> PTP Clock Synchronization
                                <img src="images/redesign/help-icon.svg" class="icon-help" data-bs-toggle="tooltip"
                                    data-bs-html="true" data-bs-placement="auto"
                                    title="IEEE 1588 Precision Time Protocol (PTP) provides sample-accurate clock synchronization between AES67 devices on the network. Enable this if you need tight sync between multiple FPP instances or professional AES67 gear.">
                            </h5>
                            <div class="d-flex gap-4 align-items-center flex-wrap">
                                <label>
                                    <input type="checkbox" class="form-check-input" id="ptpEnabledCheck"
                                        onchange="UpdatePTPEnabled(this.checked)"> Enable PTP
                                </label>
                                <div>
                                    <label for="ptpInterfaceSelect">Network Interface:</label>
                                    <select class="form-select form-select-sm d-inline-block w-auto"
                                        id="ptpInterfaceSelect" onchange="UpdatePTPInterface(this.value)">
                                        <option value="">(Default)</option>
                                    </select>
                                </div>
                                <div>
                                    <label for="ptpDomainInput">Domain:</label>
                                    <input type="number" min="0" max="127"
                                        class="form-control form-control-sm d-inline-block w-auto"
                                        id="ptpDomainInput" onchange="UpdatePTPDomain(this.value)">
                                    <img src="images/redesign/help-icon.svg" class="icon-help" data-bs-toggle="tooltip"
                                        data-bs-html="true" data-bs-placement="auto"
                                        title="PTP domain number (0-127).  All devices that must share a clock have to be on the same domain.  AES67 gear normally uses domain 0; leave this alone unless your console or DSP is configured otherwise.">
                                </div>
                                <div>
                                    <label for="ptpRoleSelect">Clock Role:</label>
                                    <select class="form-select form-select-sm d-inline-block w-auto"
                                        id="ptpRoleSelect" onchange="UpdatePTPRole(this.value)">
                                        <option value="auto">Auto</option>
                                        <option value="master">Master</option>
                                        <option value="follower">Slave</option>
                                    </select>
                                    <img src="images/redesign/help-icon.svg" class="icon-help" data-bs-toggle="tooltip"
                                        data-bs-html="true" data-bs-placement="auto"
                                        title="Who provides the PTP clock.  This is unrelated to FPP's Master/Remote player mode.<br><br><b>Auto</b> &mdash; join the BMCA election at a low priority: FPP still becomes the clock when it is the only device on the domain, but yields to a console or DSP that wants the role.<br><b>Master</b> &mdash; prefer to win the election.  Only use this if FPP is intended to be the clock for the network.<br><b>Slave</b> &mdash; always follow another grandmaster, never become the clock.">
                                </div>
                            </div>
                            <!-- Live PTP state, next to the control that sets it:
                                 commissioning needs role, lock state, grandmaster
                                 and graph rate visible in one place. -->
                            <dl id="ptpDetail" class="row mb-0 mt-3 small d-none">
                                <dt class="col-sm-3 fw-normal text-body-secondary">PTP State</dt>
                                <dd class="col-sm-9 mb-1" id="ptpDetailState">&mdash;</dd>
                                <dt class="col-sm-3 fw-normal text-body-secondary">Grandmaster</dt>
                                <dd class="col-sm-9 mb-1" id="ptpDetailGm">&mdash;</dd>
                                <dt class="col-sm-3 fw-normal text-body-secondary">Domain</dt>
                                <dd class="col-sm-9 mb-1" id="ptpDetailDomain">&mdash;</dd>
                                <dt class="col-sm-3 fw-normal text-body-secondary">Offset</dt>
                                <dd class="col-sm-9 mb-1" id="ptpDetailOffset">&mdash;</dd>
                                <dt class="col-sm-3 fw-normal text-body-secondary">Stream Source Rate</dt>
                                <dd class="col-sm-9 mb-0" id="ptpDetailRate">&mdash;</dd>
                            </dl>
                        </div>

                        <!-- Instances container -->
                        <div id="instancesContainer"></div>

                        <!-- Bottom toolbar -->
                        <div id="bottomToolbar"
                            style="display:none; position:sticky; bottom:0; background:var(--bs-body-bg,#fff); border-top:1px solid var(--bs-border-color,#dee2e6); padding:0.75rem 1rem; margin:0 -1rem; display:flex; justify-content:space-between; gap:0.5rem; z-index:10;">
                            <button class="buttons btn-outline-success" onclick="AddInstance()">
                                <i class="fas fa-plus"></i> Add Instance
                            </button>
                            <div style="display:flex; gap:0.5rem;">
                                <button class="buttons btn-success" onclick="SaveAndApply()">
                                    <i class="fas fa-save"></i> Save &amp; Apply
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
            var aes67Data = { instances: [], ptpEnabled: true, ptpInterface: '', ptpDomain: 0, ptpRole: 'auto' };
            // Raw SDP behind the export dialog.  Held here rather than read
            // back out of the textarea because a textarea's value normalises
            // CRLF to LF, and SDP lines are CRLF-terminated (RFC 4566 §5) --
            // copying through the DOM would hand out a subtly different file
            // from the one fppd announces.
            var currentSDP = { text: '', filename: '' };
            var availableInterfaces = [];
            var audioGroups = [];
            var nextInstanceId = 1;
            var hasUnsavedChanges = false;
            // Must track AES67::DEFAULT_PTIME_MS in AES67Manager.h: fppd
            // falls back to that for any instance with no stored ptime, so
            // a different default here would show a value fppd never used.
            var AES67_DEFAULT_PTIME = 1;

            // Help icon tooltip builder
            function HelpIcon(text) {
                return ' <img src="images/redesign/help-icon.svg" class="icon-help" data-bs-toggle="tooltip" data-bs-html="true" data-bs-placement="auto" title="' + EscapeAttr(text) + '">';
            }

            function InitTooltips() {
                $('[data-bs-toggle="tooltip"]').each(function () {
                    var existing = bootstrap.Tooltip.getInstance(this);
                    if (existing) existing.dispose();
                });
                $('[data-bs-toggle="tooltip"]').each(function () {
                    new bootstrap.Tooltip(this);
                });
            }

            $(document).ready(function () {
                CheckPipeWireStatus();
                setInterval(RefreshAES67Status, 10000);
                LoadInterfaces().then(function () {
                    LoadInstances();
                });
                LoadAudioGroups();
            });

            /////////////////////////////////////////////////////////////////////////////
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
                            'PipeWire not responding'
                        );
                    });

                RefreshAES67Status();
            }

            /////////////////////////////////////////////////////////////////////////////
            // PTP state moves for the first minute or so after fppd starts
            // (LISTENING -> MASTER/SLAVE as BMCA settles), so this is polled
            // rather than read once on page load.
            function RefreshAES67Status() {
                // Field names here must track AES67Manager::render_GET() —
                // pipelines[] and ptp{} — not the older PipeWire-module shape.
                $.getJSON('api/pipewire/aes67/status')
                    .done(function (data) {
                        var parts = [];
                        var pipelines = data.pipelines || [];
                        var running = 0;
                        for (var i = 0; i < pipelines.length; i++) {
                            if (pipelines[i].running)
                                running++;
                        }
                        if (pipelines.length > 0) {
                            parts.push('<span class="status-indicator ' +
                                (running === pipelines.length ? 'status-running' : 'status-stopped') +
                                '"></span>' + running + ' of ' + pipelines.length + ' stream' +
                                (pipelines.length !== 1 ? 's' : '') + ' running');
                        }

                        var ptp = data.ptp || {};
                        if (ptp.enabled === false) {
                            parts.push('PTP disabled');
                        } else if (ptp.synced) {
                            var label = ptp.isGrandmaster
                                ? 'PTP master (this device)'
                                : 'PTP synced to ' + EscapeHtml(ptp.grandmasterId || 'grandmaster');
                            if (!ptp.isGrandmaster && typeof ptp.offsetNs === 'number')
                                label += ' (' + FormatPTPOffset(ptp.offsetNs) + ')';
                            parts.push('<span class="status-indicator status-running"></span>' + label);
                        } else {
                            parts.push('<span class="status-indicator status-stopped"></span>PTP not synced' +
                                (ptp.portState ? ' (' + EscapeHtml(ptp.portState) + ')' : ''));
                        }

                        var discovered = data.discoveredStreams || [];
                        if (discovered.length > 0) {
                            parts.push(discovered.length + ' stream' +
                                (discovered.length !== 1 ? 's' : '') + ' discovered');
                        }

                        $('#ptpStatus').html(parts.join(' &nbsp;|&nbsp; '));
                        RenderPTPDetail(data);
                    })
                    .fail(function () {
                        $('#ptpStatus').html(
                            '<span class="status-indicator status-stopped"></span>AES67 status unavailable'
                        );
                        $('#ptpDetail').addClass('d-none');
                    });
            }

            /////////////////////////////////////////////////////////////////////////////
            // Commissioning needs role, lock state, grandmaster and the audio
            // graph rate together — chasing them across separate pages is what
            // makes a 44.1/48 kHz mismatch so easy to miss.
            function RenderPTPDetail(data) {
                var ptp = data.ptp || {};
                if (ptp.enabled === false) {
                    $('#ptpDetail').addClass('d-none');
                    return;
                }
                $('#ptpDetail').removeClass('d-none');

                var state = ptp.portState || 'unknown';
                if (ptp.synced)
                    state += ptp.isGrandmaster ? ' — this device is the clock' : ' — locked';
                $('#ptpDetailState').html('<span class="status-indicator ' +
                    (ptp.synced ? 'status-running' : 'status-stopped') + '"></span>' +
                    EscapeHtml(state));

                // The clock identity alone does not tell you which box on the
                // network it is -- it is an EUI-64 off some MAC, not
                // necessarily the one carrying PTP, so it cannot be looked up
                // in ARP.  The address is what gets typed into a browser when
                // the clock turns out to be the wrong device.
                var gm = ptp.grandmasterId
                    ? EscapeHtml(ptp.grandmasterId)
                    : '<span class="text-warning">none selected yet</span>';
                if (ptp.grandmasterId && ptp.grandmasterAddress) {
                    gm += ' <span class="text-body-secondary">' +
                        (ptp.grandmasterViaBoundary ? 'via boundary clock ' : 'at ') +
                        EscapeHtml(ptp.grandmasterAddress) + '</span>';
                }
                $('#ptpDetailGm').html(gm);
                $('#ptpDetailDomain').text(ptp.domain != null ? ptp.domain : '\u2014');
                $('#ptpDetailOffset').text(
                    ptp.synced && !ptp.isGrandmaster && typeof ptp.offsetNs === 'number'
                        ? FormatPTPOffset(ptp.offsetNs)
                        : (ptp.isGrandmaster ? 'n/a (we are the clock)' : '\u2014'));

                // AES67 is 48 kHz on the wire.  What matters is the rate each
                // send stream is actually fed — the graph clock alone does not
                // tell you that, because per-card and per-group rates sit in
                // between, so report what pipewiresrc negotiated.
                var sends = [];
                for (var j = 0; j < (data.pipelines || []).length; j++) {
                    var pl = data.pipelines[j];
                    if (pl.mode === 'send' && pl.sourceRate)
                        sends.push(pl);
                }
                var graph = data.graphSampleRate || 0;
                if (sends.length === 0) {
                    $('#ptpDetailRate').text(graph ? 'graph ' + graph + ' Hz' : '\u2014');
                } else {
                    var bad = [];
                    for (var k = 0; k < sends.length; k++) {
                        if (sends[k].sourceRate !== 48000)
                            bad.push(EscapeHtml(sends[k].name || ('#' + sends[k].instanceId)) +
                                ': ' + sends[k].sourceRate + ' Hz');
                    }
                    if (bad.length === 0) {
                        $('#ptpDetailRate').html('48000 Hz &mdash; fed directly, no resampling' +
                            (graph && graph !== 48000
                                ? ' <span class="text-body-secondary">(audio graph clock is ' + graph + ' Hz)</span>'
                                : ''));
                    } else {
                        $('#ptpDetailRate').html('<span class="text-warning">' + bad.join(', ') +
                            ' &mdash; resampled to 48000 Hz for AES67.</span> ' +
                            'Set the output group feeding this stream to 48000 Hz in ' +
                            '<b>PipeWire Audio Output Groups</b> to avoid the conversion.');
                    }
                }
            }

            /////////////////////////////////////////////////////////////////////////////
            // PTP offsets are reported in nanoseconds and swing over several
            // orders of magnitude while a clock settles, so scale the unit.
            function FormatPTPOffset(ns) {
                var abs = Math.abs(ns);
                if (abs < 1000)
                    return ns + ' ns';
                if (abs < 1000000)
                    return (ns / 1000).toFixed(1) + ' \u00b5s';
                return (ns / 1000000).toFixed(2) + ' ms';
            }

            /////////////////////////////////////////////////////////////////////////////
            function LoadInterfaces() {
                return $.getJSON('api/pipewire/aes67/interfaces')
                    .done(function (data) {
                        availableInterfaces = data || [];
                        // Populate PTP interface dropdown
                        var sel = $('#ptpInterfaceSelect');
                        sel.find('option:not(:first)').remove();
                        for (var i = 0; i < availableInterfaces.length; i++) {
                            sel.append('<option value="' + EscapeAttr(availableInterfaces[i]) + '">' + EscapeHtml(availableInterfaces[i]) + '</option>');
                        }
                    });
            }

            /////////////////////////////////////////////////////////////////////////////
            // A send instance is a PipeWire *sink* that something else has to
            // feed.  Its pipewiresrc is created with node.autoconnect=false, so
            // with no Audio Output Group member targeting it nothing ever links
            // in, the pipeline cannot preroll, and gst_element_set_state() sits
            // there for 30s before returning FAILURE -- which reaches the user
            // as the bare warning "AES67: audio send stream failed to start",
            // 30 seconds after an Apply that reported success.  Groups reference
            // the instance as cardId "aes67_<id>" (see GetPipeWireAudioCards),
            // so the page can see this coming and say so instead.
            function LoadAudioGroups() {
                return $.getJSON('api/pipewire/audio/groups')
                    .done(function (data) {
                        audioGroups = (data && data.groups) ? data.groups : [];
                        RenderInstances();
                    });
            }

            function InstanceHasAudioSource(inst) {
                var cardId = 'aes67_' + inst.id;
                for (var g = 0; g < audioGroups.length; g++) {
                    if (audioGroups[g].enabled === false) continue;
                    var members = audioGroups[g].members || [];
                    for (var m = 0; m < members.length; m++) {
                        if (members[m].cardId === cardId) return true;
                    }
                }
                return false;
            }

            /////////////////////////////////////////////////////////////////////////////
            function LoadInstances() {
                hasUnsavedChanges = false;
                $.getJSON('api/pipewire/aes67/instances')
                    .done(function (data) {
                        aes67Data = data || { instances: [], ptpEnabled: true, ptpInterface: '', ptpDomain: 0, ptpRole: 'auto' };
                        if (!aes67Data.instances) aes67Data.instances = [];

                        // Calculate next ID
                        nextInstanceId = 1;
                        for (var i = 0; i < aes67Data.instances.length; i++) {
                            if (aes67Data.instances[i].id >= nextInstanceId) {
                                nextInstanceId = aes67Data.instances[i].id + 1;
                            }
                        }

                        // Set PTP controls
                        $('#ptpEnabledCheck').prop('checked', aes67Data.ptpEnabled !== false);
                        $('#ptpInterfaceSelect').val(aes67Data.ptpInterface || '');
                        $('#ptpDomainInput').val(aes67Data.ptpDomain != null ? aes67Data.ptpDomain : 0);
                        $('#ptpRoleSelect').val(aes67Data.ptpRole || 'auto');

                        RenderInstances();
                    })
                    .fail(function () {
                        aes67Data = { instances: [], ptpEnabled: true, ptpInterface: '', ptpDomain: 0, ptpRole: 'auto' };
                        RenderInstances();
                    });
            }

            /////////////////////////////////////////////////////////////////////////////
            // Banner shown while added/deleted instances are not yet written to disk
            function UnsavedChangesBanner() {
                if (!hasUnsavedChanges) return '';
                return '<div class="alert alert-warning d-flex align-items-center gap-2 mb-3" id="unsavedChangesBanner">' +
                    '<i class="fas fa-exclamation-triangle"></i>' +
                    '<div>Instances have been added or deleted but <b>not saved yet</b>. ' +
                    'Click <b>Save &amp; Apply</b> to make the change permanent.</div>' +
                    '</div>';
            }

            /////////////////////////////////////////////////////////////////////////////
            function RenderInstances() {
                var container = $('#instancesContainer');
                container.empty();
                container.append(UnsavedChangesBanner());

                if (aes67Data.instances.length === 0) {
                    container.append(
                        '<div class="no-instances-msg" id="noInstancesMsg">' +
                        '<i class="fas fa-broadcast-tower"></i>' +
                        '<h4>No AES67 Instances Configured</h4>' +
                        '<p>Create AES67 instances to send and/or receive professional audio-over-IP streams.<br>' +
                        'Each instance gets its own multicast address and appears as a virtual sound card<br>' +
                        'that can be used standalone or added to an Audio Output Group.</p>' +
                        '<button class="buttons btn-outline-success" onclick="AddInstance()">' +
                        '<i class="fas fa-plus"></i> Create First Instance</button>' +
                        '</div>'
                    );
                    // Keep the toolbar (and its Save & Apply button) available when the
                    // last instance has just been deleted, otherwise the deletion can
                    // never be saved and the instance reappears on reload.
                    if (hasUnsavedChanges) {
                        $('#bottomToolbar').show();
                    } else {
                        $('#bottomToolbar').hide();
                    }
                    return;
                }

                $('#bottomToolbar').show();

                for (var i = 0; i < aes67Data.instances.length; i++) {
                    container.append(RenderInstanceCard(aes67Data.instances[i], i));
                }

                InitTooltips();
            }

            /////////////////////////////////////////////////////////////////////////////
            function RenderInstanceCard(inst, index) {
                var enabledClass = inst.enabled ? '' : ' disabled-instance';
                var enabledChecked = inst.enabled ? ' checked' : '';
                var sapChecked = inst.sapEnabled ? ' checked' : '';
                var nodeName = 'aes67_' + EscapeNodeName(inst.name);

                var html = '<div class="instance-card' + enabledClass + '" id="instance-' + inst.id + '" data-index="' + index + '">';

                // Header
                html += '<div class="instance-header">';
                html += '<input type="checkbox" class="form-check-input" onchange="ToggleInstanceEnabled(' + index + ', this.checked)"' + enabledChecked + ' title="Enable/Disable instance">';
                html += '<input type="text" class="instance-name-input" value="' + EscapeAttr(inst.name) + '" onchange="UpdateField(' + index + ', \'name\', this.value)" placeholder="Instance Name">';

                // Show node names based on mode
                var mode = inst.mode || 'send';
                if (mode === 'send' || mode === 'both') {
                    html += '<span class="badge bg-success aes67-badge"><i class="fas fa-arrow-up"></i> ' + nodeName + '_send</span>';
                }
                if (mode === 'receive' || mode === 'both') {
                    html += '<span class="badge bg-info aes67-badge"><i class="fas fa-arrow-down"></i> ' + nodeName + '_recv</span>';
                }

                html += '<div style="flex:1"></div>';
                // Only a sender has a session to describe -- a receive-only
                // instance is described by whoever transmits to it.
                if (mode === 'send' || mode === 'both') {
                    html += '<button class="buttons btn-outline-secondary btn-instance-action" onclick="ShowSDP(' + index + ')" title="Session description (SDP) for Stream Monitor, VLC and other external tools"><i class="fas fa-file-export"></i> SDP</button>';
                }
                html += '<button class="buttons btn-outline-danger btn-instance-action" onclick="DeleteInstance(' + index + ')" title="Delete Instance"><i class="fas fa-trash"></i></button>';
                html += '</div>';

                // Body
                html += '<div class="instance-body">';

                // Nothing feeds this sender -- see LoadAudioGroups().  Only
                // meaningful once the groups have actually loaded, and only for
                // an enabled sender: a disabled or receive-only instance has no
                // sink to feed.
                if (inst.enabled && (mode === 'send' || mode === 'both') &&
                    audioGroups.length > 0 && !InstanceHasAudioSource(inst)) {
                    html += '<div class="alert alert-warning d-flex align-items-start gap-2 mb-3">' +
                        '<i class="fas fa-exclamation-triangle mt-1"></i>' +
                        '<div>No audio is routed to this stream. It is not a member of any enabled ' +
                        '<a href="pipewire-audio.php">Audio Output Group</a>, so nothing feeds ' +
                        '<code>' + nodeName + '_send</code> and FPPD cannot start the stream ' +
                        '(&ldquo;audio send stream failed to start&rdquo;). Add it as a member of a group, ' +
                        'then apply the Audio Output Groups config.</div>' +
                        '</div>';
                }

                html += '<div class="instance-settings">';

                // Stream Mode
                html += '<div>';
                html += '<label>Stream Mode' + HelpIcon('Select whether this instance sends audio out over the network (Send), receives AES67 streams from other devices (Receive), or does both simultaneously. Send creates a virtual sink; Receive creates a virtual source.') + '</label>';
                html += '<select class="form-select form-select-sm" onchange="UpdateField(' + index + ', \'mode\', this.value)">';
                html += '<option value="send"' + (mode === 'send' ? ' selected' : '') + '>Send (Transmit)</option>';
                html += '<option value="receive"' + (mode === 'receive' ? ' selected' : '') + '>Receive</option>';
                html += '<option value="both"' + (mode === 'both' ? ' selected' : '') + '>Both (Send &amp; Receive)</option>';
                html += '</select>';
                html += '</div>';

                // Multicast IP
                html += '<div>';
                html += '<label>Multicast IP Address' + HelpIcon('The multicast group IP address for this AES67 stream. AES67 uses the 239.69.x.x range. Each instance should use a unique IP to avoid conflicts.') + '</label>';
                html += '<input type="text" class="form-control form-control-sm" value="' + EscapeAttr(inst.multicastIP || '239.69.0.1') + '" ';
                html += 'onchange="UpdateField(' + index + ', \'multicastIP\', this.value)" maxlength="15" placeholder="239.69.0.1">';
                html += '</div>';

                // Port
                html += '<div>';
                html += '<label>RTP Port' + HelpIcon('The UDP port for RTP traffic. Default AES67 port is 5004. Use different ports if multiple instances share the same multicast IP.') + '</label>';
                html += '<input type="number" class="form-control form-control-sm" min="1024" max="65535" step="1" value="' + (inst.port || 5004) + '" ';
                html += 'onchange="UpdateField(' + index + ', \'port\', parseInt(this.value))">';
                html += '</div>';

                // Channels
                html += '<div>';
                html += '<label>Audio Channels' + HelpIcon('AES67 allows up to 8 channels per stream, but FPP can only carry stereo to an AES67 stream today: the audio graph feeding it is fixed at 2 channels. Selecting more stops the stream starting at all, so the wider options are disabled until that is supported.') + '</label>';
                html += '<select class="form-select form-select-sm" onchange="UpdateChannels(' + index + ', parseInt(this.value))">';
                // Anything above stereo fails caps negotiation against the
                // stereo delay chain and the pipeline never produces a packet,
                // so these are shown-but-disabled rather than removed -- a
                // config saved with 8 still has to render as something.
                var chOpts = [
                    { v: 2, l: '2 (Stereo)' }, { v: 4, l: '4' },
                    { v: 6, l: '6 (5.1)' }, { v: 8, l: '8 (7.1)' }
                ];
                var chVal = inst.channels || 2;
                for (var c = 0; c < chOpts.length; c++) {
                    var wide = chOpts[c].v > 2;
                    html += '<option value="' + chOpts[c].v + '"' + (chVal === chOpts[c].v ? ' selected' : '') + (wide ? ' disabled' : '') + '>' + chOpts[c].l + (wide ? ' - not yet supported' : '') + '</option>';
                }
                html += '</select>';
                if (chVal > 2) {
                    html += '<div class="text-danger small mt-1">This stream is set to ' + chVal + ' channels, which FPP cannot carry - it will run as stereo. Select 2 (Stereo) to clear this.</div>';
                }
                html += '</div>';

                // Network Interface
                html += '<div>';
                html += '<label>Network Interface' + HelpIcon('The network interface to use for multicast traffic. Select the wired Ethernet interface for best results. Leave as Default to use the system primary route.') + '</label>';
                html += '<select class="form-select form-select-sm" onchange="UpdateField(' + index + ', \'interface\', this.value)">';
                html += '<option value="">(Default)</option>';
                for (var n = 0; n < availableInterfaces.length; n++) {
                    var iSel = (inst.interface === availableInterfaces[n]) ? ' selected' : '';
                    html += '<option value="' + EscapeAttr(availableInterfaces[n]) + '"' + iSel + '>' + EscapeHtml(availableInterfaces[n]) + '</option>';
                }
                html += '</select>';
                html += '</div>';

                // Packet Time (ptime)
                html += '<div>';
                html += '<label>Packet Time (ptime)' + HelpIcon('Audio packetization interval in milliseconds. 1ms is the default: it is mandatory for all AES67 devices, is the only packet time Dante will transmit, and measured tighter packet timing here than 4ms. 4ms is optional and uses less CPU. Must match between sender and receiver.') + '</label>';
                html += '<select class="form-select form-select-sm" onchange="UpdateField(' + index + ', \'ptime\', parseInt(this.value))">';
                // 4ms of L24 is 576 bytes per channel: fine in stereo (1152),
                // over the 1440-byte packet limit from 4 channels up (2304).
                // fppd clamps this anyway, so disable rather than mislead.
                var wideOk = (inst.channels || 2) <= 2;
                var ptimeVal = inst.ptime || AES67_DEFAULT_PTIME;
                if (!wideOk) { ptimeVal = 1; }
                html += '<option value="1"' + (ptimeVal === 1 ? ' selected' : '') + '>1 ms (default)</option>';
                html += '<option value="4"' + (ptimeVal === 4 ? ' selected' : '') + (wideOk ? '' : ' disabled') + '>4 ms' + (wideOk ? '' : ' (stereo only)') + '</option>';
                html += '</select>';
                html += '</div>';

                // Session Name
                html += '<div>';
                html += '<label>Session Name' + HelpIcon('A descriptive name for this stream visible to other AES67 devices on the network via SAP announcements.') + '</label>';
                html += '<input type="text" class="form-control form-control-sm" value="' + EscapeAttr(inst.sessionName || inst.name) + '" ';
                html += 'onchange="UpdateField(' + index + ', \'sessionName\', this.value)" maxlength="64">';
                html += '</div>';

                // Latency (only relevant for receive)
                html += '<div>';
                html += '<label>Network Latency' + HelpIcon('Target network latency in milliseconds for receive streams. Lower values reduce delay but may cause dropouts on congested networks. AES67 minimum is 1ms, typical values 1-20ms.') + '</label>';
                html += '<div class="input-group input-group-sm">';
                html += '<input type="number" class="form-control form-control-sm" min="1" max="100" step="1" value="' + (inst.latency || 10) + '" ';
                html += 'onchange="UpdateField(' + index + ', \'latency\', parseInt(this.value))">';
                html += '<span class="input-group-text">ms</span>';
                html += '</div>';
                html += '</div>';

                // SAP Discovery
                html += '<div>';
                html += '<label><input type="checkbox" class="form-check-input" onchange="UpdateField(' + index + ', \'sapEnabled\', this.checked)"' + sapChecked + '> SAP Discovery' + HelpIcon('Enable SAP (Session Announcement Protocol) for automatic discovery of AES67 streams. When enabled, sent streams are announced and incoming announcements auto-create receive streams.') + '</label>';
                html += '</div>';

                html += '</div>'; // instance-settings
                html += '</div>'; // instance-body
                html += '</div>'; // instance-card

                return html;
            }

            /////////////////////////////////////////////////////////////////////////////
            // SDP export
            //
            // SAP announcements only reach a receiver on the same subnet that
            // is listening for them.  Anything else -- Stream Monitor
            // (https://aes67.app) on a laptop, VLC, a scope on another VLAN --
            // needs the session description handed to it as text, which is
            // what this dialog is for.
            //
            // The text comes from fppd rather than from the fields on this
            // page: fppd generates it with the same builder that feeds the SAP
            // announcer, so it carries the live PTP grandmaster in ts-refclk
            // and cannot disagree with what is on the wire.  The cost is that
            // it describes the *applied* config, so edits that have not been
            // saved and applied are called out below rather than silently
            // exported.
            function ShowSDP(index) {
                var inst = aes67Data.instances[index];

                DoModalDialog({
                    id: 'aes67SDPDialog',
                    title: '<i class="fas fa-file-export"></i> Stream Description (SDP) &mdash; ' + EscapeHtml(inst.name),
                    class: 'modal-lg modal-dialog-scrollable',
                    keyboard: true,
                    backdrop: true,
                    body: '<div id="aes67SDPBody"><i class="fas fa-spinner fa-spin"></i> Loading session description&hellip;</div>',
                    // Reopening reuses the same dialog, so the previous
                    // instance's text must not survive into the new one.
                    open: function () {
                        currentSDP = { text: '', filename: '' };
                    },
                    buttons: {
                        Copy: {
                            text: '<i class="fas fa-copy"></i> Copy',
                            id: 'aes67SDPCopyBtn',
                            class: 'btn-outline-primary',
                            disabled: true,
                            click: function () {
                                CopyTextToClipboard(currentSDP.text);
                                $.jGrowl('SDP copied to clipboard', { themeState: 'success' });
                            }
                        },
                        Download: {
                            text: '<i class="fas fa-download"></i> Download .sdp',
                            id: 'aes67SDPDownloadBtn',
                            class: 'btn-outline-primary',
                            disabled: true,
                            click: function () {
                                DownloadSDP(currentSDP.filename, currentSDP.text);
                            }
                        },
                        Close: {
                            class: 'btn-success',
                            click: function () {
                                CloseModalDialog('aes67SDPDialog');
                            }
                        }
                    }
                });

                $.getJSON('api/pipewire/aes67/sdp')
                    .done(function (data) {
                        var streams = (data && data.streams) || [];
                        var stream = null;
                        for (var i = 0; i < streams.length; i++) {
                            if (streams[i].instanceId === inst.id) {
                                stream = streams[i];
                                break;
                            }
                        }
                        if (!stream) {
                            $('#aes67SDPBody').html(SDPNotice('warning',
                                'This instance has not been applied yet. Click <b>Save &amp; Apply</b>, ' +
                                'then reopen this dialog.'));
                            return;
                        }
                        RenderSDPBody(inst, stream);
                    })
                    .fail(function (xhr) {
                        var msg = (xhr.responseJSON && xhr.responseJSON.message)
                            ? xhr.responseJSON.message
                            : 'Could not read the session description from fppd.';
                        $('#aes67SDPBody').html(SDPNotice('danger', EscapeHtml(msg)));
                    });
            }

            function SDPNotice(kind, html) {
                return '<div class="alert alert-' + kind + ' mb-3">' + html + '</div>';
            }

            /////////////////////////////////////////////////////////////////////////////
            // The dialog reports the applied stream, so anything edited on the
            // card since the last Apply would be exported wrong without a
            // word.  Comparing the fields that actually appear in the SDP
            // catches that -- hasUnsavedChanges only tracks added and deleted
            // instances, not edits to an existing one.
            function SDPStaleFields(inst, stream) {
                // Same fallbacks the card itself renders with, so a config
                // that omits a field is compared against the value the user is
                // looking at rather than against undefined -- which would
                // report every such field as changed.
                var checks = [
                    ['multicastIP', 'Multicast IP', inst.multicastIP || '239.69.0.1'],
                    ['port', 'RTP Port', parseInt(inst.port, 10) || 5004],
                    ['channels', 'Audio Channels', parseInt(inst.channels, 10) || 2],
                    ['ptime', 'Packet Time', parseInt(inst.ptime, 10) || AES67_DEFAULT_PTIME],
                    ['sessionName', 'Session Name', inst.sessionName || inst.name]
                ];
                var stale = [];
                for (var i = 0; i < checks.length; i++) {
                    if (stream[checks[i][0]] != checks[i][2])
                        stale.push(checks[i][1]);
                }
                return stale;
            }

            function RenderSDPBody(inst, stream) {
                var html = '';

                var stale = SDPStaleFields(inst, stream);
                if (stale.length > 0) {
                    html += SDPNotice('warning',
                        '<b>' + EscapeHtml(stale.join(', ')) + '</b> ' +
                        (stale.length === 1 ? 'has' : 'have') +
                        ' been changed on this page but not applied. The description below is the ' +
                        'stream fppd is currently sending. Click <b>Save &amp; Apply</b> to make them match.');
                }
                if (!stream.enabled) {
                    html += SDPNotice('warning',
                        'This instance is <b>disabled</b>, so nothing is being transmitted. ' +
                        'The description is still accurate for the stream it would send once enabled.');
                }

                html += '<p>Paste this into <a href="https://aes67.app" target="_blank" rel="noopener">Stream Monitor</a>, ' +
                    'or download it as a <code>.sdp</code> file and open it with <b>VLC</b> ' +
                    '(Media &rarr; Open File).</p>';

                html += '<textarea id="aes67SDPText" class="form-control font-monospace mb-3" rows="12" readonly ' +
                    'spellcheck="false" onclick="this.select()">' +
                    EscapeHtml(stream.sdp) + '</textarea>';

                html += '<dl class="row mb-0 small">';
                html += '<dt class="col-sm-4 fw-normal text-body-secondary">Multicast Group</dt>' +
                    '<dd class="col-sm-8 mb-1">' + EscapeHtml(stream.multicastIP) + ':' + stream.port + '</dd>';
                html += '<dt class="col-sm-4 fw-normal text-body-secondary">Format</dt>' +
                    '<dd class="col-sm-8 mb-1">L24 / 48000 Hz / ' + stream.channels +
                    ' ch, ' + stream.ptime + ' ms packets</dd>';
                html += '<dt class="col-sm-4 fw-normal text-body-secondary">SAP Announcement</dt>' +
                    '<dd class="col-sm-8 mb-0">' + (stream.sapEnabled
                        ? 'On &mdash; tools on this subnet should find the stream on their own'
                        : '<span class="text-warning">Off</span> &mdash; the stream is not announced, so this file is the only way to subscribe') +
                    '</dd>';
                html += '</dl>';

                $('#aes67SDPBody').html(html);
                currentSDP.text = stream.sdp;
                currentSDP.filename = stream.filename || ('aes67_' + stream.instanceId + '.sdp');
                $('#aes67SDPCopyBtn').prop('disabled', false);
                $('#aes67SDPDownloadBtn').prop('disabled', false);
            }

            // Built from the text already in the dialog rather than fetched
            // again, so the file and what the user just read are the same
            // bytes.  A .sdp is a few hundred characters -- no need to stream it.
            function DownloadSDP(filename, text) {
                var blob = new Blob([text], { type: 'application/sdp' });
                var url = URL.createObjectURL(blob);
                var a = document.createElement('a');
                a.href = url;
                a.download = filename;
                document.body.appendChild(a);
                a.click();
                document.body.removeChild(a);
                URL.revokeObjectURL(url);
            }

            /////////////////////////////////////////////////////////////////////////////
            // Instance management
            function AddInstance() {
                var id = nextInstanceId++;
                aes67Data.instances.push({
                    id: id,
                    name: 'AES67 Stream ' + id,
                    enabled: true,
                    mode: 'send',
                    multicastIP: '239.69.0.' + id,
                    port: 5004,
                    channels: 2,
                    interface: '',
                    sessionName: 'AES67 Stream ' + id,
                    ptime: AES67_DEFAULT_PTIME,
                    latency: 10,
                    sapEnabled: true
                });
                hasUnsavedChanges = true;
                RenderInstances();
            }

            function DeleteInstance(index) {
                if (!confirm('Delete instance "' + aes67Data.instances[index].name + '"?')) return;
                aes67Data.instances.splice(index, 1);
                hasUnsavedChanges = true;
                RenderInstances();
                $.jGrowl('Instance deleted — click "Save & Apply" to make the change permanent.', { themeState: 'warning' });
            }

            function ToggleInstanceEnabled(index, enabled) {
                aes67Data.instances[index].enabled = enabled;
                var card = $('#instance-' + aes67Data.instances[index].id);
                if (enabled) {
                    card.removeClass('disabled-instance');
                } else {
                    card.addClass('disabled-instance');
                }
            }

            function UpdateField(index, field, value) {
                aes67Data.instances[index][field] = value;
                // Re-render if mode or name changed (affects badges)
                if (field === 'mode' || field === 'name') {
                    RenderInstances();
                }
            }

            // Channel count constrains ptime: 4ms only fits in a single
            // packet in stereo.  Drop to 1ms and re-render so the select
            // shows what is actually in effect.
            function UpdateChannels(index, value) {
                aes67Data.instances[index].channels = value;
                if (value > 2 && (aes67Data.instances[index].ptime || AES67_DEFAULT_PTIME) > 1) {
                    aes67Data.instances[index].ptime = 1;
                }
                RenderInstances();
            }

            function UpdatePTPEnabled(enabled) {
                aes67Data.ptpEnabled = enabled;
            }

            function UpdatePTPInterface(iface) {
                aes67Data.ptpInterface = iface;
            }

            function UpdatePTPDomain(domain) {
                var d = parseInt(domain, 10);
                if (isNaN(d) || d < 0 || d > 127) {
                    d = 0;
                    $('#ptpDomainInput').val(d);
                }
                aes67Data.ptpDomain = d;
            }

            function UpdatePTPRole(role) {
                aes67Data.ptpRole = role;
            }

            /////////////////////////////////////////////////////////////////////////////
            // Busy overlay
            //
            // Save & Apply is two round trips, and the second one can restart the
            // whole PipeWire stack and fppd with it (see RebuildAudioGraphForSenderChange
            // in api/controllers/pipewire.php), which takes tens of seconds.  Without
            // an overlay the page looks inert for that whole time and the button
            // invites a second click, so block input and say what is happening.
            function ShowBusyOverlay(message) {
                if ($('#aes67BusyOverlay').length === 0) {
                    $('body').append(
                        '<div id="aes67BusyOverlay" class="aes67-busy-overlay position-fixed top-0 start-0 ' +
                        'w-100 h-100 d-flex align-items-center justify-content-center bg-black bg-opacity-50">' +
                        '<div class="bg-body rounded-3 shadow p-4 mx-3 text-center">' +
                        '<div class="spinner-border text-primary mb-3" role="status">' +
                        '<span class="visually-hidden">Working&hellip;</span></div>' +
                        '<div id="aes67BusyMsg"></div>' +
                        '</div></div>'
                    );
                }
                UpdateBusyOverlay(message);
                $('#aes67BusyOverlay').removeClass('d-none');
            }

            function UpdateBusyOverlay(message) {
                $('#aes67BusyMsg').html(message);
            }

            function HideBusyOverlay() {
                $('#aes67BusyOverlay').addClass('d-none');
            }

            /////////////////////////////////////////////////////////////////////////////
            // Save & Apply
            function SaveAndApply() {
                ShowBusyOverlay('Saving AES67 configuration&hellip;');
                // Save first
                $.ajax({
                    url: 'api/pipewire/aes67/instances',
                    type: 'POST',
                    contentType: 'application/json',
                    data: JSON.stringify(aes67Data),
                    dataType: 'json'
                })
                    .done(function () {
                        // Config is now on disk — drop the unsaved-changes banner and,
                        // if the last instance was just deleted, the toolbar with it.
                        hasUnsavedChanges = false;
                        RenderInstances();
                        // Then apply
                        UpdateBusyOverlay('Applying configuration&hellip;<br>' +
                            '<small class="text-body-secondary">If the audio graph changed, PipeWire and FPPD ' +
                            'are restarted — this can take up to a minute.</small>');
                        $.post('api/pipewire/aes67/apply', '')
                            .done(function (applyData) {
                                HideBusyOverlay();
                                // The endpoint reports failures in the body with HTTP 200
                                // (e.g. fppd not running), so .done() alone is not success.
                                if (applyData && applyData.status === 'ERROR') {
                                    DialogError('Apply Failed', 'Error applying AES67 config: ' +
                                        (applyData.message || 'unknown error'));
                                } else if (applyData && applyData.restartRequired) {
                                    DialogOK('Configuration Applied',
                                        '<p>AES67 instances have been applied. The audio graph changed, so ' +
                                        'PipeWire and FPPD have both been restarted.</p>' +
                                        '<p>If you are using AES67 sinks as members of Audio Output Groups, ' +
                                        're-apply the Audio Groups config as well.</p>'
                                    );
                                } else {
                                    DialogOK('Saved', 'AES67 configuration applied successfully.');
                                }
                                CheckPipeWireStatus();
                            })
                            .fail(function (xhr) {
                                HideBusyOverlay();
                                DialogError('Apply Failed', 'Error applying AES67 config: ' + (xhr.responseJSON ? xhr.responseJSON.message : xhr.statusText));
                            });
                    })
                    .fail(function (xhr) {
                        HideBusyOverlay();
                        DialogError('Save Failed', 'Error saving AES67 config: ' + (xhr.responseJSON ? xhr.responseJSON.message : xhr.statusText));
                    });
            }

            /////////////////////////////////////////////////////////////////////////////
            // Utility functions
            function EscapeHtml(str) {
                if (!str) return '';
                return String(str).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
            }

            function EscapeAttr(str) {
                if (!str) return '';
                return String(str).replace(/&/g, '&amp;').replace(/"/g, '&quot;').replace(/'/g, '&#39;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
            }

            function EscapeNodeName(str) {
                if (!str) return '';
                return String(str).replace(/[^a-zA-Z0-9_]/g, '_').toLowerCase();
            }
        </script>
</body>

</html>