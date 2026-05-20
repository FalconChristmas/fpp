<!DOCTYPE html>
<html lang="en">

<head>
    <?php
    include 'common/htmlMeta.inc';
    require_once "common.php";
    require_once 'config.php';
    include 'common/menuHead.inc';
    ?>

    <title><? echo $pageTitle; ?> - Opus RTP Audio Streaming</title>

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

        .opus-badge {
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

        .instance-settings .icon-help {
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
        if (!$modalMode) {
            $activeParentMenuItem = 'status';
            include 'menu.inc';
            ?>
            <div class="mainContainer">
                <div class="title">PipeWire Opus RTP Audio Streaming</div>

                <div class="pageContent">
                <?php } else { ?>
                    <div style="padding: 1rem;">
                    <?php } ?>

                    <?php if (!isset($settings['MediaBackend']) || $settings['MediaBackend'] != 'pipewire') { ?>
                        <div class="callout callout-warning" style="padding:1rem;">
                            <h4>Advanced PipeWire Required</h4>
                            <p>Opus RTP audio streaming requires the Advanced PipeWire backend.<br>
                                Go to <b>Settings &rarr; Audio/Video</b> and set <b>Media Backend</b> to <b>PipeWire
                                    (Advanced)</b>,
                                then reboot.</p>
                        </div>
                    <?php } else { ?>

                        <div class="callout callout-info" style="padding:0.75rem 1rem; margin-bottom:1rem;">
                            <b>Opus RTP</b> provides compressed audio streaming ideal for WiFi networks.
                            Unlike AES67 (uncompressed, wired-only), Opus is loss-tolerant and bandwidth-efficient.
                            <ul style="margin:0.5rem 0 0 0; padding-left:1.2rem;">
                                <li><b>WiFi:</b> Use <b>unicast</b> (the receiver's IP address) for reliable streaming.
                                    WiFi multicast is unreliable &mdash; most access points send it without retransmission at the lowest data rate.</li>
                                <li><b>Wired:</b> Both unicast and multicast (239.x.x.x) work well. Multicast allows one sender to reach multiple receivers.</li>
                                <li><b>Receiver:</b> Set the Destination IP to the <em>receiver's own IP</em> for unicast, or a multicast group address (e.g. 239.69.1.x) that both sender and receiver share.</li>
                            </ul>
                        </div>

                        <div
                            style="display:flex; justify-content:space-between; align-items:center; margin-bottom:1rem; flex-wrap:wrap; gap:0.5rem;">
                            <div>
                                <span id="pwStatus">
                                    <span class="status-indicator status-stopped"></span>Checking PipeWire...
                                </span>
                            </div>
                            <div style="display:flex; gap:0.5rem;">
                                <button class="buttons btn-outline-success" onclick="AddInstance()">
                                    <i class="fas fa-plus"></i> Add Opus RTP Instance
                                </button>
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
            var opusData = { instances: [] };
            var availableInterfaces = [];
            var nextInstanceId = 1;

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
            }

            /////////////////////////////////////////////////////////////////////////////
            function LoadInterfaces() {
                return $.getJSON('api/pipewire/opusrtp/interfaces')
                    .done(function (data) {
                        availableInterfaces = data || [];
                    });
            }

            /////////////////////////////////////////////////////////////////////////////
            function LoadInstances() {
                $.getJSON('api/pipewire/opusrtp/instances')
                    .done(function (data) {
                        opusData = data || { instances: [] };
                        if (!opusData.instances) opusData.instances = [];

                        nextInstanceId = 1;
                        for (var i = 0; i < opusData.instances.length; i++) {
                            if (opusData.instances[i].id >= nextInstanceId) {
                                nextInstanceId = opusData.instances[i].id + 1;
                            }
                        }

                        RenderInstances();
                    })
                    .fail(function () {
                        opusData = { instances: [] };
                        RenderInstances();
                    });
            }

            /////////////////////////////////////////////////////////////////////////////
            function RenderInstances() {
                var container = $('#instancesContainer');
                container.empty();

                if (opusData.instances.length === 0) {
                    container.append(
                        '<div class="no-instances-msg" id="noInstancesMsg">' +
                        '<i class="fas fa-wifi"></i>' +
                        '<h4>No Opus RTP Instances Configured</h4>' +
                        '<p>Create Opus RTP instances to send and/or receive compressed audio streams over the network.<br>' +
                        'Ideal for WiFi connections where AES67 (uncompressed) would be unreliable.<br>' +
                        'Each instance appears as a virtual sound card that can be used in Audio Output Groups.</p>' +
                        '<button class="buttons btn-outline-success" onclick="AddInstance()">' +
                        '<i class="fas fa-plus"></i> Create First Instance</button>' +
                        '</div>'
                    );
                    $('#bottomToolbar').hide();
                    return;
                }

                $('#bottomToolbar').show();

                for (var i = 0; i < opusData.instances.length; i++) {
                    container.append(RenderInstanceCard(opusData.instances[i], i));
                }

                InitTooltips();
                UpdateMulticastWarnings();
            }

            /////////////////////////////////////////////////////////////////////////////
            function RenderInstanceCard(inst, index) {
                var enabledClass = inst.enabled ? '' : ' disabled-instance';
                var enabledChecked = inst.enabled ? ' checked' : '';
                var dtxChecked = inst.dtx ? ' checked' : '';
                var fecChecked = (inst.fec !== false) ? ' checked' : '';
                var nodeName = 'opusrtp_' + EscapeNodeName(inst.name);

                var html = '<div class="instance-card' + enabledClass + '" id="instance-' + inst.id + '" data-index="' + index + '">';

                // Header
                html += '<div class="instance-header">';
                html += '<input type="checkbox" class="form-check-input" onchange="ToggleInstanceEnabled(' + index + ', this.checked)"' + enabledChecked + ' title="Enable/Disable instance">';
                html += '<input type="text" class="instance-name-input" value="' + EscapeAttr(inst.name) + '" onchange="UpdateField(' + index + ', \'name\', this.value)" placeholder="Instance Name">';

                var mode = inst.mode || 'send';
                if (mode === 'send' || mode === 'both') {
                    html += '<span class="badge bg-success opus-badge"><i class="fas fa-arrow-up"></i> ' + nodeName + '_send</span>';
                }
                if (mode === 'receive' || mode === 'both') {
                    html += '<span class="badge bg-info opus-badge"><i class="fas fa-arrow-down"></i> ' + nodeName + '_recv</span>';
                }

                html += '<div style="flex:1"></div>';
                html += '<button class="buttons btn-outline-danger btn-instance-action" onclick="DeleteInstance(' + index + ')" title="Delete Instance"><i class="fas fa-trash"></i></button>';
                html += '</div>';

                // Body
                html += '<div class="instance-body">';
                html += '<div class="instance-settings">';

                // Stream Mode
                html += '<div>';
                html += '<label>Stream Mode' + HelpIcon('Select whether this instance sends audio out over the network (Send), receives Opus RTP streams from other devices (Receive), or does both simultaneously.') + '</label>';
                html += '<select class="form-select form-select-sm" onchange="UpdateField(' + index + ', \'mode\', this.value)">';
                html += '<option value="send"' + (mode === 'send' ? ' selected' : '') + '>Send (Transmit)</option>';
                html += '<option value="receive"' + (mode === 'receive' ? ' selected' : '') + '>Receive</option>';
                html += '<option value="both"' + (mode === 'both' ? ' selected' : '') + '>Both (Send &amp; Receive)</option>';
                html += '</select>';
                html += '</div>';

                // Destination IP
                html += '<div>';
                html += '<label>Destination IP' + HelpIcon('<b>Unicast</b> (recommended for WiFi): enter the receiver\'s IP address for reliable point-to-point streaming with full WiFi retransmission.<br><br><b>Multicast</b> (wired only): use a 239.x.x.x address to send to multiple receivers simultaneously. Avoid multicast over WiFi &mdash; most access points deliver it unreliably at the lowest data rate with no retransmission.') + '</label>';
                html += '<input type="text" class="form-control form-control-sm" value="' + EscapeAttr(inst.destIP || '239.69.1.1') + '" ';
                html += 'onchange="UpdateField(' + index + ', \'destIP\', this.value)" maxlength="45" placeholder="Receiver IP (e.g. 192.168.1.x)">';
                html += '<div id="destip-warn-' + inst.id + '" class="form-text text-warning" style="display:none;"><i class="fas fa-exclamation-triangle"></i> Multicast over WiFi is unreliable. Use the receiver\'s IP address (unicast) for WiFi streaming.</div>';
                html += '</div>';

                // Port
                html += '<div>';
                html += '<label>RTP Port' + HelpIcon('The UDP port for RTP traffic. Default is 5005. Use different ports if running multiple instances to the same destination.') + '</label>';
                html += '<input type="number" class="form-control form-control-sm" min="1024" max="65535" step="1" value="' + (inst.port || 5005) + '" ';
                html += 'onchange="UpdateField(' + index + ', \'port\', parseInt(this.value))">';
                html += '</div>';

                // Channels
                html += '<div>';
                html += '<label>Audio Channels' + HelpIcon('Number of audio channels. Opus supports mono and stereo natively. Higher channel counts use multiple Opus streams.') + '</label>';
                html += '<select class="form-select form-select-sm" onchange="UpdateField(' + index + ', \'channels\', parseInt(this.value))">';
                var chOpts = [
                    { v: 1, l: '1 (Mono)' }, { v: 2, l: '2 (Stereo)' }
                ];
                for (var c = 0; c < chOpts.length; c++) {
                    html += '<option value="' + chOpts[c].v + '"' + ((inst.channels || 2) === chOpts[c].v ? ' selected' : '') + '>' + chOpts[c].l + '</option>';
                }
                html += '</select>';
                html += '</div>';

                // Network Interface
                html += '<div>';
                html += '<label>Network Interface' + HelpIcon('The network interface to send or receive on. Select the interface matching your network:<br><br><b>WiFi (e.g. wlan0):</b> Use with unicast for best results.<br><b>Wired (e.g. eth0):</b> Works with both unicast and multicast.<br><b>Default:</b> Uses the system\'s primary route.') + '</label>';
                html += '<select class="form-select form-select-sm" onchange="UpdateField(' + index + ', \'interface\', this.value)">';
                html += '<option value="">(Default)</option>';
                for (var n = 0; n < availableInterfaces.length; n++) {
                    var iSel = (inst.interface === availableInterfaces[n]) ? ' selected' : '';
                    html += '<option value="' + EscapeAttr(availableInterfaces[n]) + '"' + iSel + '>' + EscapeHtml(availableInterfaces[n]) + '</option>';
                }
                html += '</select>';
                html += '</div>';

                // Bitrate
                html += '<div>';
                html += '<label>Bitrate' + HelpIcon('Opus encoder bitrate. Higher values improve quality but use more bandwidth. 128 kbps is excellent for stereo music over WiFi. Lower values (64-96 kbps) work well when bandwidth is limited.') + '</label>';
                html += '<select class="form-select form-select-sm" onchange="UpdateField(' + index + ', \'bitrate\', parseInt(this.value))">';
                var brOpts = [
                    { v: 32000, l: '32 kbps (Low)' },
                    { v: 64000, l: '64 kbps' },
                    { v: 96000, l: '96 kbps' },
                    { v: 128000, l: '128 kbps (Default)' },
                    { v: 192000, l: '192 kbps' },
                    { v: 256000, l: '256 kbps (High)' },
                    { v: 320000, l: '320 kbps' }
                ];
                var curBitrate = inst.bitrate || 128000;
                for (var b = 0; b < brOpts.length; b++) {
                    html += '<option value="' + brOpts[b].v + '"' + (curBitrate === brOpts[b].v ? ' selected' : '') + '>' + brOpts[b].l + '</option>';
                }
                html += '</select>';
                html += '</div>';

                // Latency
                html += '<div>';
                html += '<label>Jitter Buffer' + HelpIcon('Jitter buffer size in milliseconds for receive streams. This buffers incoming packets to smooth out network timing variations.<br><br><b>Wired unicast:</b> 50ms is usually sufficient.<br><b>WiFi unicast:</b> 100&ndash;150ms recommended.<br><b>WiFi multicast:</b> not recommended &mdash; if you must, try 200&ndash;500ms.<br><br>Higher values add audio delay but improve reliability.') + '</label>';
                html += '<div class="input-group input-group-sm">';
                html += '<input type="number" class="form-control form-control-sm" min="10" max="500" step="10" value="' + (inst.latency || 50) + '" ';
                html += 'onchange="UpdateField(' + index + ', \'latency\', parseInt(this.value))">';
                html += '<span class="input-group-text">ms</span>';
                html += '</div>';
                html += '</div>';

                // FEC
                html += '<div>';
                html += '<label><input type="checkbox" class="form-check-input" onchange="UpdateField(' + index + ', \'fec\', this.checked)"' + fecChecked + '> Forward Error Correction' + HelpIcon('Opus in-band FEC adds redundancy to help recover from packet loss. Recommended for WiFi where some packet loss is expected.') + '</label>';
                html += '</div>';

                // DTX
                html += '<div>';
                html += '<label><input type="checkbox" class="form-check-input" onchange="UpdateField(' + index + ', \'dtx\', this.checked)"' + dtxChecked + '> Discontinuous Transmission (DTX)' + HelpIcon('DTX reduces bandwidth during silence by sending fewer packets. Saves bandwidth but may cause slight artifacts when audio resumes.') + '</label>';
                html += '</div>';

                // Packet Loss
                html += '<div>';
                html += '<label>Expected Packet Loss' + HelpIcon('The expected packet loss percentage on the network. This tunes the FEC encoder to optimize for the given loss rate. 5% is typical for WiFi; 0-1% for wired.') + '</label>';
                html += '<div class="input-group input-group-sm">';
                html += '<input type="number" class="form-control form-control-sm" min="0" max="50" step="1" value="' + (inst.packetLoss != null ? inst.packetLoss : 5) + '" ';
                html += 'onchange="UpdateField(' + index + ', \'packetLoss\', parseInt(this.value))">';
                html += '<span class="input-group-text">%</span>';
                html += '</div>';
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
                opusData.instances.push({
                    id: id,
                    name: 'Opus RTP Stream ' + id,
                    enabled: true,
                    mode: 'send',
                    destIP: '',
                    port: 5005,
                    channels: 2,
                    interface: '',
                    bitrate: 128000,
                    latency: 100,
                    fec: true,
                    dtx: false,
                    packetLoss: 5
                });
                RenderInstances();
            }

            function DeleteInstance(index) {
                if (!confirm('Delete instance "' + opusData.instances[index].name + '"?')) return;
                opusData.instances.splice(index, 1);
                RenderInstances();
            }

            function ToggleInstanceEnabled(index, enabled) {
                opusData.instances[index].enabled = enabled;
                var card = $('#instance-' + opusData.instances[index].id);
                if (enabled) {
                    card.removeClass('disabled-instance');
                } else {
                    card.addClass('disabled-instance');
                }
            }

            function UpdateField(index, field, value) {
                opusData.instances[index][field] = value;
                if (field === 'mode' || field === 'name') {
                    RenderInstances();
                }
                if (field === 'destIP' || field === 'interface') {
                    UpdateMulticastWarnings();
                }
            }

            function IsMulticastIP(ip) {
                if (!ip) return false;
                var first = parseInt(ip.split('.')[0], 10);
                return first >= 224 && first <= 239;
            }

            function IsWifiInterface(iface) {
                return iface && (iface.indexOf('wlan') === 0 || iface.indexOf('wlp') === 0);
            }

            function UpdateMulticastWarnings() {
                for (var i = 0; i < opusData.instances.length; i++) {
                    var inst = opusData.instances[i];
                    var warn = $('#destip-warn-' + inst.id);
                    if (warn.length && IsMulticastIP(inst.destIP) && IsWifiInterface(inst.interface)) {
                        warn.show();
                    } else if (warn.length) {
                        warn.hide();
                    }
                }
            }

            /////////////////////////////////////////////////////////////////////////////
            // Save & Apply
            function SaveAndApply() {
                $.ajax({
                    url: 'api/pipewire/opusrtp/instances',
                    type: 'POST',
                    contentType: 'application/json',
                    data: JSON.stringify(opusData),
                    dataType: 'json'
                })
                    .done(function () {
                        $.post('api/pipewire/opusrtp/apply', '')
                            .done(function (applyData) {
                                if (applyData && applyData.restartRequired) {
                                    DialogOK('Configuration Applied',
                                        '<p>Opus RTP instances have been applied.</p>' +
                                        '<p>If you are using Opus RTP sinks as members of Audio Output Groups, ' +
                                        're-apply the Audio Groups config as well.</p>' +
                                        '<p><b>FPPD must be restarted</b> for audio routing changes to take effect.</p>' +
                                        '<button class="btn btn-warning mt-2" onclick="RestartFPPD()"><i class="fas fa-sync"></i> Restart FPPD Now</button>'
                                    );
                                } else {
                                    DialogOK('Saved', 'Opus RTP configuration applied successfully.');
                                }
                                CheckPipeWireStatus();
                            })
                            .fail(function (xhr) {
                                DialogError('Apply Failed', 'Error applying Opus RTP config: ' + (xhr.responseJSON ? xhr.responseJSON.message : xhr.statusText));
                            });
                    })
                    .fail(function (xhr) {
                        DialogError('Save Failed', 'Error saving Opus RTP config: ' + (xhr.responseJSON ? xhr.responseJSON.message : xhr.statusText));
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
