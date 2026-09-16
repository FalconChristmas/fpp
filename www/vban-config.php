<!DOCTYPE html>
<html lang="en">

<head>
    <?php
    include 'common/htmlMeta.inc';
    require_once "common.php";
    require_once 'config.php';
    include 'common/menuHead.inc';
    ?>

    <title><? echo $pageTitle; ?> - VBAN Audio Streaming</title>

    <?php $modalMode = isset($_GET['modal']) && $_GET['modal'] == '1'; ?>

    <style>
        /* Help glyph sizing -- no Bootstrap utility covers an inline SVG icon
           that has to sit on the text baseline of a form label. */
        .icon-help {
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

<body<?php if ($modalMode)
    echo ' class="modal-mode"'; ?>>
    <div id="bodyWrapper">
        <?php
        if (!$modalMode) {
            $activeParentMenuItem = 'status';
            include 'menu.inc';
            ?>
            <div class="mainContainer">
                <div class="title">PipeWire VBAN Audio Streaming</div>

                <div class="pageContent">
                <?php } else { ?>
                    <div class="p-3">
                    <?php } ?>

                    <?php if (!isset($settings['MediaBackend']) || $settings['MediaBackend'] != 'pipewire') { ?>
                        <div class="callout callout-warning p-3">
                            <h4>Advanced PipeWire Required</h4>
                            <p>VBAN audio streaming requires the Advanced PipeWire backend.<br>
                                Go to <b>Settings &rarr; Audio/Video</b> and set <b>Media Backend</b> to <b>PipeWire
                                    (Advanced)</b>, then reboot.</p>
                        </div>
                    <?php } else { ?>

                        <div class="callout callout-info p-3 mb-3">
                            <b>VBAN</b> (VB-Audio Network) carries uncompressed PCM audio over UDP on a local network.
                            It is the protocol Voicemeeter, VBAN Talkie and the VB-Audio tools speak, which makes it the
                            simplest way to get audio from a Windows or macOS desktop into FPP.
                            <ul class="mt-2 mb-0 ps-4">
                                <li><b>Receive:</b> FPP listens on a UDP port and publishes each incoming stream as a
                                    PipeWire source. Use it as an <b>Input Mixing</b> member to route it to any output,
                                    and as the <b>WLED Sound Reactive Source</b> in Settings &rarr; Audio/Video.</li>
                                <li><b>Send:</b> FPP publishes a sink that an <b>Audio Output Group</b> can target,
                                    transmitting whatever that group plays to the destination.</li>
                                <li><b>Stream name:</b> VBAN multiplexes several streams onto one port and tells them
                                    apart by a name in each packet (Voicemeeter's default is <code>Stream1</code>).
                                    Several receivers may share a port as long as each has a different name.</li>
                                <li><b>Port:</b> VBAN's default is <b>6980</b> for both directions.</li>
                            </ul>
                        </div>

                        <div class="callout callout-warning p-3 mb-3">
                            <i class="fas fa-exclamation-triangle"></i>
                            PipeWire reads its module list only when it starts, so <b>Save &amp; Apply restarts the
                                PipeWire stack and fppd</b>. Audio stops for a few seconds. Apply VBAN changes when
                            nothing is playing.
                        </div>

                        <div class="d-flex justify-content-between align-items-center mb-3 flex-wrap gap-2">
                            <div>
                                <span id="pwStatus">
                                    <i class="fas fa-circle text-danger"></i> Checking PipeWire...
                                </span>
                                &nbsp;&nbsp;
                                <span id="vbanStatus"></span>
                            </div>
                            <button class="buttons btn-outline-success" onclick="AddInstance()">
                                <i class="fas fa-plus"></i> Add VBAN Stream
                            </button>
                        </div>

                        <div id="instancesContainer"></div>

                        <div id="bottomToolbar" class="d-none sticky-bottom bg-body border-top py-3 px-3 mx-n3 d-flex justify-content-between gap-2">
                            <button class="buttons btn-outline-success" onclick="AddInstance()">
                                <i class="fas fa-plus"></i> Add Stream
                            </button>
                            <button class="buttons btn-success" onclick="SaveAndApply()">
                                <i class="fas fa-save"></i> Save &amp; Apply
                            </button>
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
            var vbanData = { instances: [] };
            var availableInterfaces = [];
            // Per-direction node presence from the status poll, keyed
            // "<instanceId>:<recv|send>".  A receive node only exists once a
            // matching stream has actually arrived, so absence is information
            // worth showing rather than an error.
            var nodeState = {};
            var nextInstanceId = 1;
            var hasUnsavedChanges = false;

            $(document).ready(function () {
                CheckPipeWireStatus();
                setInterval(RefreshVBANStatus, 10000);
                LoadInterfaces().then(function () {
                    LoadInstances();
                });
            });

            function HelpIcon(text) {
                return ' <img src="images/redesign/help-icon.svg" class="icon-help" data-bs-toggle="tooltip" data-bs-html="true" data-bs-placement="auto" title="' + EscapeAttr(text) + '">';
            }

            function InitTooltips() {
                $('[data-bs-toggle="tooltip"]').each(function () {
                    var existing = bootstrap.Tooltip.getInstance(this);
                    if (existing) existing.dispose();
                    new bootstrap.Tooltip(this);
                });
            }

            /////////////////////////////////////////////////////////////////////////////
            function CheckPipeWireStatus() {
                $.getJSON('api/pipewire/audio/sinks')
                    .done(function (data) {
                        var count = data ? data.length : 0;
                        $('#pwStatus').html('<i class="fas fa-circle text-success"></i> PipeWire running &mdash; ' +
                            count + ' sink' + (count !== 1 ? 's' : '') + ' available');
                    })
                    .fail(function () {
                        $('#pwStatus').html('<i class="fas fa-circle text-danger"></i> PipeWire not responding');
                    });
                RefreshVBANStatus();
            }

            /////////////////////////////////////////////////////////////////////////////
            // Whether a stream is actually flowing is only knowable from the live
            // graph, so the badges come from the status endpoint rather than from
            // whatever is currently typed into the form.
            function RefreshVBANStatus() {
                $.getJSON('api/pipewire/vban/status')
                    .done(function (data) {
                        nodeState = {};
                        var running = 0, waiting = 0;
                        if (data && data.instances) {
                            for (var i = 0; i < data.instances.length; i++) {
                                var inst = data.instances[i];
                                for (var d = 0; d < inst.directions.length; d++) {
                                    var dir = inst.directions[d];
                                    nodeState[inst.id + ':' + dir.direction] = dir;
                                    if (!inst.enabled) continue;
                                    if (dir.present) running++; else waiting++;
                                }
                            }
                        }
                        var html = '';
                        if (running > 0) {
                            html += '<i class="fas fa-circle text-success"></i> ' + running + ' VBAN node' +
                                (running !== 1 ? 's' : '') + ' live';
                        }
                        if (waiting > 0) {
                            html += (html ? ' &nbsp; ' : '') + '<i class="fas fa-circle text-warning"></i> ' +
                                waiting + ' waiting';
                        }
                        $('#vbanStatus').html(html);
                        UpdateStatusBadges();
                    })
                    .fail(function () {
                        $('#vbanStatus').html('');
                    });
            }

            function UpdateStatusBadges() {
                for (var i = 0; i < vbanData.instances.length; i++) {
                    var inst = vbanData.instances[i];
                    var dirs = DirectionsFor(inst.mode);
                    for (var d = 0; d < dirs.length; d++) {
                        var el = $('#status-' + inst.id + '-' + dirs[d]);
                        if (!el.length) continue;
                        el.html(StatusBadgeHtml(inst, dirs[d]));
                    }
                }
                InitTooltips();
            }

            function DirectionsFor(mode) {
                if (mode === 'both') return ['recv', 'send'];
                if (mode === 'send') return ['send'];
                return ['recv'];
            }

            function StatusBadgeHtml(inst, dir) {
                var label = (dir === 'recv') ? 'Receive' : 'Send';
                if (!inst.enabled) {
                    return '<span class="badge bg-secondary">' + label + ': disabled</span>';
                }
                var st = nodeState[inst.id + ':' + dir];
                if (!st) {
                    return '<span class="badge bg-secondary">' + label + ': not applied</span>';
                }
                if (st.present) {
                    return '<span class="badge bg-success" data-bs-toggle="tooltip" title="' +
                        EscapeAttr(st.nodeName + ' — state: ' + st.state) + '">' + label + ': live</span>';
                }
                return '<span class="badge bg-warning text-dark" data-bs-toggle="tooltip" title="' +
                    EscapeAttr(st.detail) + '">' + label + ': waiting</span>';
            }

            /////////////////////////////////////////////////////////////////////////////
            function LoadInterfaces() {
                return $.getJSON('api/pipewire/vban/interfaces')
                    .done(function (data) {
                        availableInterfaces = data || [];
                    })
                    .fail(function () {
                        availableInterfaces = [];
                    });
            }

            function LoadInstances() {
                hasUnsavedChanges = false;
                $.getJSON('api/pipewire/vban/instances')
                    .done(function (data) {
                        vbanData = data || { instances: [] };
                        if (!vbanData.instances) vbanData.instances = [];
                        nextInstanceId = 1;
                        for (var i = 0; i < vbanData.instances.length; i++) {
                            if (vbanData.instances[i].id >= nextInstanceId) {
                                nextInstanceId = vbanData.instances[i].id + 1;
                            }
                        }
                        RenderInstances();
                    })
                    .fail(function () {
                        vbanData = { instances: [] };
                        RenderInstances();
                    });
            }

            /////////////////////////////////////////////////////////////////////////////
            function UnsavedChangesBanner() {
                if (!hasUnsavedChanges) return '';
                return '<div class="alert alert-warning d-flex align-items-center gap-2 mb-3">' +
                    '<i class="fas fa-exclamation-triangle"></i>' +
                    '<div>Streams have been added or deleted but <b>not saved yet</b>. ' +
                    'Click <b>Save &amp; Apply</b> to make the change permanent.</div>' +
                    '</div>';
            }

            function RenderInstances() {
                var container = $('#instancesContainer');
                container.empty();
                container.append(UnsavedChangesBanner());

                if (vbanData.instances.length === 0) {
                    container.append(
                        '<div class="text-center text-body-secondary py-5">' +
                        '<i class="fas fa-network-wired fs-1 d-block mb-3"></i>' +
                        '<p class="mb-1">No VBAN streams configured.</p>' +
                        '<p class="mb-0"><small>Add one to receive audio from Voicemeeter, or to send FPP audio to a desktop.</small></p>' +
                        '</div>'
                    );
                    $('#bottomToolbar').addClass('d-none');
                    return;
                }

                $('#bottomToolbar').removeClass('d-none');

                for (var i = 0; i < vbanData.instances.length; i++) {
                    container.append(RenderInstance(vbanData.instances[i], i));
                }
                UpdateStatusBadges();
                InitTooltips();
            }

            function RenderInstance(inst, index) {
                var isRecv = (inst.mode === 'receive' || inst.mode === 'both');
                var isSend = (inst.mode === 'send' || inst.mode === 'both');
                var dirs = DirectionsFor(inst.mode);

                var h = '<div class="card mb-3' + (inst.enabled ? '' : ' opacity-50') + '" id="instance-' + inst.id + '">';

                // Header
                h += '<div class="card-header d-flex align-items-center gap-3 flex-wrap">';
                h += '<div class="form-check form-switch mb-0">' +
                    '<input class="form-check-input" type="checkbox" id="enabled-' + inst.id + '"' +
                    (inst.enabled ? ' checked' : '') +
                    ' onchange="UpdateField(' + index + ', \'enabled\', this.checked)">' +
                    '</div>';
                h += '<input type="text" class="form-control form-control-sm w-auto fw-semibold" value="' +
                    EscapeAttr(inst.name) + '" onchange="UpdateField(' + index + ', \'name\', this.value)">';
                for (var d = 0; d < dirs.length; d++) {
                    h += '<span id="status-' + inst.id + '-' + dirs[d] + '">' + StatusBadgeHtml(inst, dirs[d]) + '</span>';
                }
                h += '<button class="btn btn-sm btn-outline-danger ms-auto" onclick="DeleteInstance(' + index + ')">' +
                    '<i class="fas fa-trash"></i></button>';
                h += '</div>';

                // Body
                h += '<div class="card-body"><div class="row g-3">';

                h += Field('Mode' + HelpIcon('<b>Receive</b> publishes an Audio/Source you can route or use for WLED sound reactive.<br><b>Send</b> publishes an Audio/Sink an Audio Output Group can target.<br><b>Both</b> does each, on the same port.'),
                    '<select class="form-select form-select-sm" onchange="UpdateField(' + index + ', \'mode\', this.value)">' +
                    Opt('receive', 'Receive (from network)', inst.mode) +
                    Opt('send', 'Send (to network)', inst.mode) +
                    Opt('both', 'Both', inst.mode) +
                    '</select>');

                h += Field('VBAN Stream Name' + HelpIcon('The name carried in each VBAN packet, max 16 characters. Voicemeeter\'s default is <code>Stream1</code>.<br>On a receiver, leave blank to accept whatever arrives &mdash; but two receivers on one port must have different names.'),
                    '<input type="text" class="form-control form-control-sm" maxlength="16" value="' +
                    EscapeAttr(inst.streamName || '') + '" placeholder="' + (isRecv ? 'Stream1 (blank = any)' : 'FPP' + inst.id) +
                    '" onchange="UpdateField(' + index + ', \'streamName\', this.value)">');

                if (isRecv) {
                    h += Field('Listen Address' + HelpIcon('Address to bind the receive socket to. <code>0.0.0.0</code> accepts from any sender on any interface, which is almost always what you want.'),
                        '<input type="text" class="form-control form-control-sm" value="' +
                        EscapeAttr(inst.sourceIP || '0.0.0.0') + '" placeholder="0.0.0.0"' +
                        ' onchange="UpdateField(' + index + ', \'sourceIP\', this.value)">');
                }

                if (isSend) {
                    h += Field('Destination IP' + HelpIcon('The IP address of the machine running the VBAN receiver (for example the PC running Voicemeeter).'),
                        '<input type="text" class="form-control form-control-sm" value="' +
                        EscapeAttr(inst.destIP || '') + '" placeholder="192.168.1.2"' +
                        ' onchange="UpdateField(' + index + ', \'destIP\', this.value)">');
                }

                h += Field('UDP Port' + HelpIcon('VBAN\'s default is 6980. Receivers may share a port when their stream names differ; two senders must not share a destination IP and port.'),
                    '<input type="number" class="form-control form-control-sm" min="1" max="65535" value="' +
                    parseInt(inst.port || 6980, 10) + '" onchange="UpdateField(' + index + ', \'port\', parseInt(this.value,10))">');

                h += Field('Channels',
                    '<select class="form-select form-select-sm" onchange="UpdateField(' + index + ', \'channels\', parseInt(this.value,10))">' +
                    Opt(1, '1 (Mono)', inst.channels) + Opt(2, '2 (Stereo)', inst.channels) +
                    Opt(4, '4', inst.channels) + Opt(6, '6 (5.1)', inst.channels) +
                    Opt(8, '8 (7.1)', inst.channels) +
                    '</select>');

                if (isSend) {
                    h += Field('Sample Rate' + HelpIcon('Must match what the receiving application expects. 48000 is the usual choice.'),
                        '<select class="form-select form-select-sm" onchange="UpdateField(' + index + ', \'sampleRate\', parseInt(this.value,10))">' +
                        Opt(44100, '44100 Hz', inst.sampleRate) + Opt(48000, '48000 Hz', inst.sampleRate) +
                        Opt(96000, '96000 Hz', inst.sampleRate) +
                        '</select>');

                    h += Field('Sample Format' + HelpIcon('VBAN\'s most widely supported format is 16-bit PCM (S16LE).'),
                        '<select class="form-select form-select-sm" onchange="UpdateField(' + index + ', \'format\', this.value)">' +
                        Opt('S16LE', '16-bit PCM', inst.format) +
                        Opt('S24LE', '24-bit PCM', inst.format) +
                        Opt('S32LE', '32-bit PCM', inst.format) +
                        Opt('F32LE', '32-bit float', inst.format) +
                        '</select>');
                }

                if (isRecv) {
                    h += Field('Jitter Buffer (ms)' + HelpIcon('How much network jitter to absorb. Too low and bursty senders underrun it; too high and each out-of-order packet costs more, because this receiver resyncs on reordering and a resync discards the whole buffer. 100 suits most links &mdash; drop toward 60 if you get regular dropouts exactly one buffer long, raise it if arrival is erratic.<br>When several receivers share a port, the largest value on that port applies to all of them.'),
                        '<input type="number" class="form-control form-control-sm" min="5" max="2000" value="' +
                        parseInt(inst.latency || 200, 10) + '" onchange="UpdateField(' + index + ', \'latency\', parseInt(this.value,10))">');
                }

                h += Field('Network Interface' + HelpIcon('Leave as Default unless this box is multi-homed and the stream must use a specific interface.'),
                    '<select class="form-select form-select-sm" onchange="UpdateField(' + index + ', \'interface\', this.value)">' +
                    Opt('', '(Default)', inst.interface || '') +
                    availableInterfaces.map(function (n) { return Opt(n, n, inst.interface || ''); }).join('') +
                    '</select>');

                h += '</div>';

                // Where this node shows up once applied.
                var recvNode = 'vban_' + EscapeNodeName(inst.name) + '_recv';
                var sendNode = 'vban_' + EscapeNodeName(inst.name) + '_send';
                h += '<hr class="my-3">';
                h += '<div class="small text-body-secondary">';
                if (isRecv) {
                    h += '<div><i class="fas fa-arrow-right"></i> Receives as PipeWire source <code>' + EscapeHtml(recvNode) + '</code>' +
                        ' &mdash; available in <b>Input Mixing</b> and as the <b>WLED Sound Reactive Source</b>.</div>';
                }
                if (isSend) {
                    h += '<div><i class="fas fa-arrow-left"></i> Sends from PipeWire sink <code>' + EscapeHtml(sendNode) + '</code>' +
                        ' &mdash; add it to an <b>Audio Output Group</b> to feed it.</div>';
                }
                h += '</div>';

                h += '</div></div>';
                return h;
            }

            function Field(label, control) {
                return '<div class="col-12 col-md-6 col-xl-4">' +
                    '<label class="form-label fw-medium mb-1">' + label + '</label>' +
                    control + '</div>';
            }

            function Opt(value, label, current) {
                return '<option value="' + EscapeAttr(value) + '"' +
                    (String(current) === String(value) ? ' selected' : '') + '>' + EscapeHtml(label) + '</option>';
            }

            /////////////////////////////////////////////////////////////////////////////
            function AddInstance() {
                var id = nextInstanceId++;
                vbanData.instances.push({
                    id: id,
                    name: 'VBAN Stream ' + id,
                    enabled: true,
                    mode: 'receive',
                    streamName: 'Stream1',
                    sourceIP: '0.0.0.0',
                    destIP: '',
                    port: 6980,
                    channels: 2,
                    sampleRate: 48000,
                    format: 'S16LE',
                    interface: '',
                    latency: 100,
                    ttl: 1,
                    dscp: 34
                });
                hasUnsavedChanges = true;
                RenderInstances();
            }

            function DeleteInstance(index) {
                if (!confirm('Delete VBAN stream "' + vbanData.instances[index].name + '"?')) return;
                vbanData.instances.splice(index, 1);
                hasUnsavedChanges = true;
                RenderInstances();
                $.jGrowl('Stream deleted — click "Save & Apply" to make the change permanent.', { themeState: 'warning' });
            }

            function UpdateField(index, field, value) {
                vbanData.instances[index][field] = value;
                // The mode decides which fields exist, the name decides the node
                // names shown, and enabled greys the card -- all need a redraw.
                if (field === 'mode' || field === 'name' || field === 'enabled') {
                    RenderInstances();
                }
            }

            /////////////////////////////////////////////////////////////////////////////
            function SaveAndApply() {
                $.ajax({
                    url: 'api/pipewire/vban/instances',
                    type: 'POST',
                    contentType: 'application/json',
                    data: JSON.stringify(vbanData),
                    dataType: 'json'
                })
                    .done(function () {
                        hasUnsavedChanges = false;
                        RenderInstances();
                        $.jGrowl('Applying VBAN config — PipeWire is restarting...', { themeState: 'notice' });
                        $.post('api/pipewire/vban/apply', '')
                            .done(function (applyData) {
                                DialogOK('Saved', (applyData && applyData.message)
                                    ? applyData.message
                                    : 'VBAN configuration applied.');
                                setTimeout(CheckPipeWireStatus, 3000);
                            })
                            .fail(function (xhr) {
                                DialogError('Apply Failed', 'Error applying VBAN config: ' +
                                    (xhr.responseJSON ? xhr.responseJSON.message : xhr.statusText));
                            });
                    })
                    .fail(function (xhr) {
                        DialogError('Save Failed', 'Error saving VBAN config: ' +
                            (xhr.responseJSON ? xhr.responseJSON.message : xhr.statusText));
                    });
            }

            /////////////////////////////////////////////////////////////////////////////
            function EscapeHtml(str) {
                if (str === null || str === undefined) return '';
                return String(str).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
            }

            function EscapeAttr(str) {
                if (str === null || str === undefined) return '';
                return String(str).replace(/&/g, '&amp;').replace(/"/g, '&quot;').replace(/'/g, '&#39;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
            }

            function EscapeNodeName(str) {
                if (!str) return 'instance';
                var s = String(str).replace(/[^a-zA-Z0-9_]/g, '_').toLowerCase();
                return s === '' ? 'instance' : s;
            }
        </script>
    </div>
</body>

</html>
