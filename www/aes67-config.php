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

<body>
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
                            <div style="display:flex; gap:2rem; align-items:center; flex-wrap:wrap;">
                                <label>
                                    <input type="checkbox" class="form-check-input" id="ptpEnabledCheck"
                                        onchange="UpdatePTPEnabled(this.checked)"> Enable PTP
                                </label>
                                <div>
                                    <label for="ptpInterfaceSelect">Network Interface:</label>
                                    <select class="form-select form-select-sm" id="ptpInterfaceSelect"
                                        style="display:inline-block;width:auto;" onchange="UpdatePTPInterface(this.value)">
                                        <option value="">(Default)</option>
                                    </select>
                                </div>
                            </div>
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
            var aes67Data = { instances: [], ptpEnabled: true, ptpInterface: '' };
            var availableInterfaces = [];
            var nextInstanceId = 1;

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
                LoadInterfaces().then(function () {
                    LoadInstances();
                });
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

                $.getJSON('api/pipewire/aes67/status')
                    .done(function (data) {
                        var parts = [];
                        if (data.sinks && data.sinks.length > 0)
                            parts.push(data.sinks.length + ' AES67 sink' + (data.sinks.length !== 1 ? 's' : ''));
                        if (data.sources && data.sources.length > 0)
                            parts.push(data.sources.length + ' AES67 source' + (data.sources.length !== 1 ? 's' : ''));
                        if (data.ptpRunning)
                            parts.push('<span class="status-indicator status-running"></span>PTP synced');

                        if (parts.length > 0) {
                            $('#ptpStatus').html(parts.join(' &nbsp;|&nbsp; '));
                        }
                    });
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
            function LoadInstances() {
                $.getJSON('api/pipewire/aes67/instances')
                    .done(function (data) {
                        aes67Data = data || { instances: [], ptpEnabled: true, ptpInterface: '' };
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

                        RenderInstances();
                    })
                    .fail(function () {
                        aes67Data = { instances: [], ptpEnabled: true, ptpInterface: '' };
                        RenderInstances();
                    });
            }

            /////////////////////////////////////////////////////////////////////////////
            function RenderInstances() {
                var container = $('#instancesContainer');
                container.empty();

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
                    $('#bottomToolbar').hide();
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
                html += '<button class="buttons btn-outline-danger btn-instance-action" onclick="DeleteInstance(' + index + ')" title="Delete Instance"><i class="fas fa-trash"></i></button>';
                html += '</div>';

                // Body
                html += '<div class="instance-body">';
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
                html += '<label>Audio Channels' + HelpIcon('Number of audio channels in this AES67 stream. Standard AES67 supports up to 8 channels per stream. Most use cases need 2 (stereo).') + '</label>';
                html += '<select class="form-select form-select-sm" onchange="UpdateField(' + index + ', \'channels\', parseInt(this.value))">';
                var chOpts = [
                    { v: 2, l: '2 (Stereo)' }, { v: 4, l: '4' },
                    { v: 6, l: '6 (5.1)' }, { v: 8, l: '8 (7.1)' }
                ];
                for (var c = 0; c < chOpts.length; c++) {
                    html += '<option value="' + chOpts[c].v + '"' + ((inst.channels || 2) === chOpts[c].v ? ' selected' : '') + '>' + chOpts[c].l + '</option>';
                }
                html += '</select>';
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
                html += '<label>Packet Time (ptime)' + HelpIcon('Audio packetization interval in milliseconds. AES67 supports 1ms (low latency, higher CPU) or 4ms (common, more compatible). Must match between sender and receiver. Default is 4ms.') + '</label>';
                html += '<select class="form-select form-select-sm" onchange="UpdateField(' + index + ', \'ptime\', parseInt(this.value))">';
                var ptimeVal = inst.ptime || 4;
                html += '<option value="1"' + (ptimeVal === 1 ? ' selected' : '') + '>1 ms (low latency)</option>';
                html += '<option value="4"' + (ptimeVal === 4 ? ' selected' : '') + '>4 ms (default)</option>';
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
                    ptime: 4,
                    latency: 10,
                    sapEnabled: true
                });
                RenderInstances();
            }

            function DeleteInstance(index) {
                if (!confirm('Delete instance "' + aes67Data.instances[index].name + '"?')) return;
                aes67Data.instances.splice(index, 1);
                RenderInstances();
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

            function UpdatePTPEnabled(enabled) {
                aes67Data.ptpEnabled = enabled;
            }

            function UpdatePTPInterface(iface) {
                aes67Data.ptpInterface = iface;
            }

            /////////////////////////////////////////////////////////////////////////////
            // Save & Apply
            function SaveAndApply() {
                // Save first
                $.ajax({
                    url: 'api/pipewire/aes67/instances',
                    type: 'POST',
                    contentType: 'application/json',
                    data: JSON.stringify(aes67Data),
                    dataType: 'json'
                })
                    .done(function () {
                        // Then apply
                        $.post('api/pipewire/aes67/apply', '')
                            .done(function (applyData) {
                                if (applyData && applyData.restartRequired) {
                                    DialogOK('Configuration Applied',
                                        '<p>AES67 instances have been applied and PipeWire has been restarted.</p>' +
                                        '<p>If you are using AES67 sinks as members of Audio Output Groups, ' +
                                        're-apply the Audio Groups config as well.</p>' +
                                        '<p><b>FPPD must be restarted</b> for audio routing changes to take effect.</p>' +
                                        '<button class="btn btn-warning mt-2" onclick="RestartFPPD()"><i class="fas fa-sync"></i> Restart FPPD Now</button>'
                                    );
                                } else {
                                    DialogOK('Saved', 'AES67 configuration applied successfully.');
                                }
                                CheckPipeWireStatus();
                            })
                            .fail(function (xhr) {
                                DialogError('Apply Failed', 'Error applying AES67 config: ' + (xhr.responseJSON ? xhr.responseJSON.message : xhr.statusText));
                            });
                    })
                    .fail(function (xhr) {
                        DialogError('Save Failed', 'Error saving AES67 config: ' + (xhr.responseJSON ? xhr.responseJSON.message : xhr.statusText));
                    });
            }

            function RestartFPPD() {
                $.get('api/system/fppd/restart')
                    .done(function () {
                        $.jGrowl('FPPD restart requested', { themeState: 'success' });
                    })
                    .fail(function () {
                        $.jGrowl('FPPD restart failed', { themeState: 'danger' });
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