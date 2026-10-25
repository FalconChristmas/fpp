<!DOCTYPE html>
<html lang="en">

<head>
    <?php
    include 'common/htmlMeta.inc';
    require_once "common.php";
    require_once 'config.php';
    include 'common/menuHead.inc';
    ?>

    <title><? echo $pageTitle; ?> - PipeWire Pipeline</title>

    <script type="text/javascript" src="js/d3.v7.min.js?ref=<?= filemtime('js/d3.v7.min.js'); ?>"></script>

    <style>
        /* ── Layout ───────────────────────────────────────────── */
        #pw-graph-container {
            position: relative;
            width: 100%;
            height: calc(100vh - 160px);
            min-height: 500px;
            border: 1px solid var(--bs-border-color, #dee2e6);
            border-radius: 8px;
            background: var(--bs-body-bg, #fff);
            overflow: hidden;
        }

        #pw-graph-container svg {
            width: 100%;
            height: 100%;
            cursor: grab;
        }

        #pw-graph-container svg:active {
            cursor: grabbing;
        }

        /* ── Toolbar ──────────────────────────────────────────── */
        .pw-toolbar {
            display: flex;
            gap: 0.5rem;
            align-items: center;
            margin-bottom: 0.75rem;
            flex-wrap: wrap;
        }

        .pw-toolbar .btn {
            font-size: 0.85rem;
        }

        .pw-legend {
            display: flex;
            gap: 1rem;
            margin-left: auto;
            font-size: 0.8rem;
            align-items: center;
            flex-wrap: wrap;
        }

        .pw-legend-item {
            display: flex;
            align-items: center;
            gap: 4px;
        }

        .pw-legend-swatch {
            width: 14px;
            height: 14px;
            border-radius: 3px;
            border: 1px solid rgba(0, 0, 0, 0.15);
        }

        /* ── Node drawing ─────────────────────────────────────── */
        .pw-node rect {
            rx: 6;
            ry: 6;
            stroke-width: 2;
            cursor: pointer;
            transition: filter 0.15s;
        }

        .pw-node:hover rect {
            filter: brightness(1.12);
        }

        .pw-node.selected rect {
            stroke-width: 3;
            filter: drop-shadow(0 0 6px rgba(13, 110, 253, 0.5));
        }

        .pw-node-label {
            font-size: 12px;
            font-weight: 600;
            pointer-events: none;
            fill: #fff;
        }

        .pw-node-sublabel {
            font-size: 10px;
            pointer-events: none;
            fill: rgba(255, 255, 255, 0.8);
        }

        .pw-node-meta {
            font-size: 9px;
            pointer-events: none;
            fill: rgba(255, 255, 255, 0.65);
            font-style: italic;
        }

        /* ── Ports ────────────────────────────────────────────── */
        .pw-port circle {
            r: 5;
            stroke: #fff;
            stroke-width: 1.5;
            cursor: crosshair;
        }

        .pw-port-label {
            font-size: 9px;
            fill: var(--bs-body-color, #333);
            pointer-events: none;
        }

        /* ── Links ────────────────────────────────────────────── */
        .pw-link {
            fill: none;
            stroke-width: 2;
            opacity: 0.7;
        }

        .pw-link.active {
            stroke: #198754;
            opacity: 0.9;
        }

        .pw-link.paused {
            stroke: #6c757d;
            stroke-dasharray: 6 3;
        }

        .pw-link.error {
            stroke: #dc3545;
        }

        .pw-link.not-running {
            stroke: #6c757d;
            stroke-dasharray: 4 4;
            opacity: 0.4;
        }

        /* ── Detail panel ─────────────────────────────────────── */
        #pw-detail-panel {
            position: absolute;
            top: 10px;
            right: 10px;
            width: 320px;
            max-height: calc(100% - 20px);
            background: var(--bs-body-bg, #fff);
            border: 1px solid var(--bs-border-color, #dee2e6);
            border-radius: 8px;
            box-shadow: 0 4px 16px rgba(0, 0, 0, 0.12);
            overflow-y: auto;
            display: none;
            z-index: 10;
            font-size: 0.85rem;
        }

        #pw-detail-panel.show {
            display: block;
        }

        #pw-detail-header {
            position: sticky;
            top: 0;
            padding: 0.6rem 0.75rem;
            background: var(--bs-tertiary-bg, #f8f9fa);
            border-bottom: 1px solid var(--bs-border-color, #dee2e6);
            border-radius: 8px 8px 0 0;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }

        #pw-detail-header h6 {
            margin: 0;
        }

        #pw-detail-body {
            padding: 0.6rem 0.75rem;
        }

        #pw-detail-body table {
            width: 100%;
        }

        #pw-detail-body td {
            padding: 2px 4px;
            vertical-align: top;
            word-break: break-all;
        }

        #pw-detail-body td:first-child {
            white-space: nowrap;
            font-weight: 600;
            color: var(--bs-secondary-color, #6c757d);
            width: 40%;
        }

        /* ── State badge ──────────────────────────────────────── */
        .state-badge {
            display: inline-block;
            font-size: 0.7rem;
            padding: 1px 6px;
            border-radius: 3px;
            font-weight: 600;
            text-transform: uppercase;
        }

        .state-running {
            background: #198754;
            color: #fff;
        }

        .state-idle {
            background: #ffc107;
            color: #333;
        }

        .state-suspended {
            background: #6c757d;
            color: #fff;
        }

        .state-error {
            background: #dc3545;
            color: #fff;
        }
    </style>
</head>

<body>
    <div id="bodyWrapper">
        <?php
        $activeParentMenuItem = 'status';
        include 'menu.inc';
        ?>
        <div class="mainContainer">
            <h2 class="title d-pull-left">PipeWire Audio &amp; Video Pipeline</h2>
            <div class="pageContent">

                <div class="pw-toolbar">
                    <a class="btn btn-sm btn-outline-secondary" href="settings.php#settings-av"
                        title="Back to Pipewire Settings">
                        <i class="fas fa-arrow-left"></i> Pipewire Settings
                    </a>
                    <button class="btn btn-sm btn-outline-primary" id="btnRefresh" title="Refresh graph">
                        <i class="fas fa-sync-alt"></i> Refresh
                    </button>
                    <button class="btn btn-sm btn-outline-secondary" id="btnFitView" title="Fit graph to view">
                        <i class="fas fa-expand"></i> Fit
                    </button>
                    <button class="btn btn-sm btn-outline-secondary" id="btnResetLayout" title="Re-layout nodes">
                        <i class="fas fa-project-diagram"></i> Re-layout
                    </button>
                    <span id="graphStats" class="text-muted" style="font-size:0.8rem; margin-left:0.5rem;"></span>

                    <div class="pw-legend">
                        <span class="pw-legend-item"><span class="pw-legend-swatch" style="background:#20c997"></span>
                            HW Source</span>
                        <span class="pw-legend-item"><span class="pw-legend-swatch" style="background:#fd7e14"></span>
                            Media Stream</span>
                        <span class="pw-legend-item"><span class="pw-legend-swatch" style="background:#d63384"></span>
                            Input Tee</span>
                        <span class="pw-legend-item"><span class="pw-legend-swatch" style="background:#e35d6a"></span>
                            Input Group</span>
                        <span class="pw-legend-item"><span class="pw-legend-swatch" style="background:#0d6efd"></span>
                            Output Group</span>
                        <span class="pw-legend-item"><span class="pw-legend-swatch" style="background:#6f42c1"></span>
                            Effect</span>
                        <span class="pw-legend-item"><span class="pw-legend-swatch" style="background:#198754"></span>
                            HW Output (ALSA)</span>
                        <span class="pw-legend-item"><span class="pw-legend-swatch" style="background:#dc3545"></span>
                            AES67 / App</span>
                        <span class="pw-legend-item"><span class="pw-legend-swatch" style="background:#0dcaf0"></span>
                            Video Stream</span>
                        <span class="pw-legend-item"><span class="pw-legend-swatch" style="background:#e8651a"></span>
                            A+V Source</span>
                        <span class="pw-legend-item"><span class="pw-legend-swatch" style="background:#20b2aa"></span>
                            Video Output</span>
                    </div>
                </div>

                <div id="pw-graph-container">
                    <svg id="pw-svg"></svg>
                    <div id="pw-detail-panel">
                        <div id="pw-detail-header">
                            <h6 id="pw-detail-title">Node</h6>
                            <button type="button" class="btn-close btn-close-sm" id="btnCloseDetail"></button>
                        </div>
                        <div id="pw-detail-body"></div>
                    </div>
                </div>

            </div>
        </div>
    </div>

    <?php include 'common/footer.inc'; ?>

    <script>
        /* ═══════════════════════════════════════════════════════════════════
           PipeWire Pipeline Graph Visualizer
           Uses D3.js v7 — dagre-style left-to-right layout with manual
           node positioning and Bézier link routing.
           ═══════════════════════════════════════════════════════════════════ */
        (function () {
            'use strict';

            // ── Constants ─────────────────────────────────────────────────
            const NODE_W = 220, NODE_H_BASE = 52, PORT_R = 5, PORT_SPACING = 18;
            const PORT_Y_START = 44; // below the 3-line text area
            const PADDING = { top: 40, left: 60, colGap: 100, rowGap: 30 };

            // Colour mapping by node role
            function nodeColor(n) {
                const nm = n.name || '';
                const mc = n.mediaClass || '';
                const p = n.properties || {};
                // Combined A+V video input sources (merged audio+video node)
                if (p['fpp.combined.av'] && nm.startsWith('fpp_audio_src_')) return '#e8651a'; // combined A+V source
                // Video nodes
                if (nm.startsWith('fppd_video_stream_')) return '#0dcaf0'; // video producer
                if (nm.startsWith('fpp_hdmi_direct_')) return '#198754';   // HDMI direct output
                if (nm.startsWith('fpp_video_group_')) return '#20b2aa';   // video consumer
                if (mc === 'Stream/Output/Video') return '#0dcaf0';
                if (mc === 'Stream/Input/Video') return '#20b2aa';
                if (mc === 'Video/Source') return '#0dcaf0';
                if (mc === 'Video/Sink') return '#20b2aa';
                // Audio nodes
                if (nm.startsWith('fpp_input_')) return '#e35d6a'; // input group mix bus
                if (nm.startsWith('fpp_tee_fppd_stream_')) return '#d63384'; // input tee (fan-out)
                if (nm.startsWith('fpp_loopback_ig') || nm.startsWith('input.fpp_loopback_ig') || nm.startsWith('output.fpp_loopback_ig')) return '#e35d6a'; // input group routing
                if (nm.startsWith('fpp_route_ig_') || nm.startsWith('output.fpp_route_ig_')) return '#e35d6a'; // routing hub (post-EQ)
                if (nm.startsWith('fpp_group_')) return '#0d6efd'; // combine-stream group
                if (nm.startsWith('fpp_fx_') && !nm.endsWith('_out')) return '#6f42c1'; // delay filter sink
                if (nm.startsWith('output.fpp_group_') ||
                    nm.startsWith('fpp_fx_') && nm.endsWith('_out')) return '#fd7e14'; // internal stream
                if (mc === 'Audio/Sink' && nm.startsWith('alsa_')) return '#198754'; // ALSA hw output
                if (mc === 'Audio/Source') return '#20c997'; // source
                if (mc === 'Stream/Input/Audio') return '#dc3545'; // app capture (AES67)
                if (nm.startsWith('fppd_stream_')) {
                    const props = n.properties || {};
                    return props['fpp.combined.av'] ? '#e8651a' : '#fd7e14'; // combined A+V / audio only
                }
                if (mc === 'Stream/Output/Audio') return '#fd7e14'; // stream
                if (mc === 'Audio/Sink') return '#198754'; // other sink
                return '#6c757d';
            }

            // Build a short metadata string for the third line of each node
            function nodeMetaText(n) {
                const p = n.properties || {};
                const nm = n.name || '';
                const mc = n.mediaClass || '';

                // Delay / effect nodes
                if (nm.startsWith('fpp_fx_')) {
                    const parts = [];
                    if (p['fpp.delay.ms'] !== undefined) {
                        const ms = p['fpp.delay.ms'];
                        parts.push(ms > 0 ? 'delay ' + ms + ' ms' : 'no delay');
                    }
                    if (p['fpp.eq.enabled']) parts.push('EQ on');
                    return parts.join(' · ') || '';
                }

                // Audio group nodes
                if (nm.startsWith('fpp_group_')) {
                    const parts = [];
                    if (p['fpp.group.members']) parts.push(p['fpp.group.members'] + ' members');
                    if (p['fpp.group.latencyCompensate']) parts.push('latency comp');
                    return parts.join(' · ') || '';
                }

                // Input group nodes
                if (nm.startsWith('fpp_input_')) {
                    const parts = [];
                    if (p['fpp.inputGroup.members']) parts.push(p['fpp.inputGroup.members'] + ' sources');
                    if (p['fpp.inputGroup.outputs']) parts.push('→ ' + p['fpp.inputGroup.outputs'] + ' outputs');
                    return parts.join(' · ') || 'mix bus';
                }

                // ALSA sinks / sources
                if (mc.startsWith('Audio/') && nm.startsWith('alsa_')) {
                    const parts = [];
                    if (p['audio.format']) parts.push(p['audio.format']);
                    if (p['audio.rate']) parts.push((p['audio.rate'] / 1000).toFixed(1) + ' kHz');
                    if (p['audio.channels']) parts.push(p['audio.channels'] + ' ch');
                    if (p['api.alsa.headroom']) parts.push('headroom ' + p['api.alsa.headroom']);
                    return parts.join(' · ') || '';
                }

                // fppd media streams
                if (nm.startsWith('fppd_stream_')) {
                    const parts = [];
                    if (p['fpp.combined.av']) parts.push('audio + video');
                    if (p['fpp.stream.slot']) parts.push('slot ' + p['fpp.stream.slot']);
                    if (p['fpp.stream.targets'] && p['fpp.stream.targets'].length) {
                        const names = p['fpp.stream.targets'].map(t =>
                            t.replace(/^fpp_input_/, '').replace(/_/g, ' '));
                        parts.push('→ ' + names.join(', '));
                    } else if (p['fpp.stream.target']) {
                        const t = p['fpp.stream.target'].replace(/^fpp_input_/, '').replace(/_/g, ' ');
                        parts.push('→ ' + t);
                    }
                    if (p['fpp.stream.virtual']) parts.push('(inactive)');
                    return parts.join(' · ') || '';
                }

                // Video producer streams (legacy PipeWire-based)
                if (nm.startsWith('fppd_video_stream_')) {
                    const parts = [];
                    if (p['fpp.video.slot']) parts.push('slot ' + p['fpp.video.slot']);
                    parts.push('video producer');
                    return parts.join(' · ');
                }

                // Video input sources (GStreamer intervideo-based)
                if (nm.startsWith('fpp_video_src_') || nm.startsWith('fpp_audio_src_')) {
                    const parts = [];
                    if (p['fpp.combined.av']) parts.push('audio + video');
                    if (p['video.format']) parts.push(p['video.format']);
                    if (p['fpp.video.slot']) parts.push('slot ' + p['fpp.video.slot']);
                    if (!p['fpp.combined.av']) parts.push('video source');
                    return parts.join(' · ');
                }

                // Video output consumers
                if (nm.startsWith('fpp_video_group_')) {
                    const parts = [];
                    if (p['fpp.video.groupName']) parts.push(p['fpp.video.groupName']);
                    parts.push('video consumer');
                    return parts.join(' · ');
                }

                // HDMI direct output (kmssink in primary pipeline)
                if (nm.startsWith('fpp_hdmi_direct_')) {
                    return p['fpp.hdmi.connector'] || 'HDMI direct';
                }

                // Streams (AES67, other)
                if (mc.includes('Stream/')) {
                    const parts = [];
                    if (p['audio.channels']) parts.push(p['audio.channels'] + ' ch');
                    if (p['application.name']) parts.push(p['application.name']);
                    return parts.join(' · ') || '';
                }

                return '';
            }

            // ── State ─────────────────────────────────────────────────────
            let graphData = { nodes: [], ports: [], links: [] };
            let selectedNodeId = null;

            const svg = d3.select('#pw-svg');
            const container = d3.select('#pw-graph-container');

            // Root <g> for pan/zoom
            const gRoot = svg.append('g').attr('class', 'pw-root');
            const gBackground = gRoot.append('g').attr('class', 'pw-background-layer');
            const gLinks = gRoot.append('g').attr('class', 'pw-links-layer');
            const gNodes = gRoot.append('g').attr('class', 'pw-nodes-layer');

            // Zoom behaviour
            const zoom = d3.zoom()
                .scaleExtent([0.15, 3])
                .on('zoom', e => gRoot.attr('transform', e.transform));
            svg.call(zoom);

            // ── Data fetching ─────────────────────────────────────────────
            function fetchGraph() {
                return $.getJSON('/api/pipewire/graph').then(data => {
                    graphData = data;
                    mergeInternalNodes();
                    return graphData;
                });
            }

            // ── Merge paired PipeWire nodes ───────────────────────────────
            // PipeWire filter-chains create two nodes per filter: a sink
            // (fpp_fx_g1_s3) and a stream output (fpp_fx_g1_s3_out).
            // Combine-streams create a sink (fpp_group_*) plus one stream
            // output per member (output.fpp_group_*_member).
            // This merges them into single visual nodes so the graph shows
            // the logical audio flow without internal plumbing clutter.
            function mergeInternalNodes() {
                const nodesByName = {};
                const nodesById = {};
                graphData.nodes.forEach(n => {
                    nodesByName[n.name] = n;
                    nodesById[n.id] = n;
                });

                const absorbed = new Set(); // IDs of nodes merged into a parent
                const nodeIdRemap = {};     // absorbed-id → parent-id

                // 1) Filter-chain pairs: fpp_fx_*_out → fpp_fx_*
                graphData.nodes.forEach(n => {
                    if (n.name.startsWith('fpp_fx_') && n.name.endsWith('_out')) {
                        const parentName = n.name.slice(0, -4); // strip '_out'
                        const parent = nodesByName[parentName];
                        if (parent) {
                            absorbed.add(n.id);
                            nodeIdRemap[n.id] = parent.id;
                            // Move output ports from child to parent
                            graphData.ports.forEach(p => {
                                if (p.nodeId === n.id) p.nodeId = parent.id;
                            });
                            // Promote state: if child is running, parent should show running
                            if (n.state === 'running' && parent.state !== 'running') {
                                parent.state = n.state;
                            }
                        }
                    }
                });

                // 2) Combine-stream outputs: output.fpp_group_* → fpp_group_*
                graphData.nodes.forEach(n => {
                    if (n.name.startsWith('output.fpp_group_')) {
                        // Find the parent group node — name starts with fpp_group_
                        // output.fpp_group_XXX_memberName → fpp_group_XXX
                        // We find the longest matching fpp_group_* prefix
                        let bestParent = null;
                        for (const [nm, candidate] of Object.entries(nodesByName)) {
                            if (nm.startsWith('fpp_group_') && n.name.startsWith('output.' + nm)) {
                                if (!bestParent || nm.length > bestParent.name.length) {
                                    bestParent = candidate;
                                }
                            }
                        }
                        if (bestParent) {
                            absorbed.add(n.id);
                            nodeIdRemap[n.id] = bestParent.id;
                            // Move output ports from this stream node to the group node
                            graphData.ports.forEach(p => {
                                if (p.nodeId === n.id) p.nodeId = bestParent.id;
                            });
                            if (n.state === 'running' && bestParent.state !== 'running') {
                                bestParent.state = n.state;
                            }
                        }
                    }
                });

                // 3) Loopback sub-nodes: input.fpp_loopback_ig* / output.fpp_loopback_ig* → input group
                // PipeWire loopback modules create only input.* and output.* nodes (no bare parent).
                // Absorb both into the input group combine-stream node so the graph shows
                // ALSA source → Input Group directly without intermediate loopback nodes.
                // Uses fpp.inputGroup.id enriched by the graph API to match loopback → input group.
                //
                // Unlike other merges we do NOT remap loopback ports to the parent;
                // instead we build a portId remap so links are redirected to the
                // input group's own ports.  The orphaned loopback ports are cleaned
                // up by the orphan-port filter later.
                const portIdRemap = {};
                graphData.nodes.forEach(n => {
                    if (!(n.name.startsWith('input.fpp_loopback_ig') || n.name.startsWith('output.fpp_loopback_ig')))
                        return;
                    const loopbackGroupId = n.properties && n.properties['fpp.inputGroup.id'];
                    if (loopbackGroupId === undefined || loopbackGroupId === null) return;
                    // Find the input group combine-stream node with matching group ID
                    let igNode = null;
                    graphData.nodes.forEach(candidate => {
                        if (candidate.properties && candidate.properties['fpp.inputGroup'] &&
                            candidate.properties['fpp.inputGroup.id'] === loopbackGroupId) {
                            igNode = candidate;
                        }
                    });
                    if (igNode) {
                        absorbed.add(n.id);
                        nodeIdRemap[n.id] = igNode.id;
                        // Build port-to-port mapping by channel name so links
                        // get redirected to the input group's own ports.
                        const myPorts = graphData.ports.filter(p => p.nodeId === n.id);
                        const igPorts = graphData.ports.filter(p => p.nodeId === igNode.id);
                        myPorts.forEach((mp, idx) => {
                            // Strip common prefixes (playback_, capture_, input_, output_)
                            const ch = mp.name.replace(/^(playback|capture|input|output)_/, '');
                            let match = igPorts.find(ip => {
                                const igCh = ip.name.replace(/^(playback|capture|input|output)_/, '');
                                return igCh === ch && ip.direction === mp.direction;
                            });
                            // Fallback: match by index among same-direction ports
                            if (!match) {
                                const sameDirPorts = igPorts.filter(ip => ip.direction === mp.direction);
                                const myIdx = myPorts.filter(p => p.direction === mp.direction).indexOf(mp);
                                if (myIdx >= 0 && myIdx < sameDirPorts.length) {
                                    match = sameDirPorts[myIdx];
                                }
                            }
                            if (match) {
                                portIdRemap[mp.id] = match.id;
                            }
                        });
                        // Do NOT remap ports (p.nodeId stays as the loopback's id)
                        // — orphan-port filter will remove them.
                        if (n.state === 'running' && igNode.state !== 'running') {
                            igNode.state = n.state;
                        }
                    }
                });

                // 4) Combine-stream outputs for input groups: output.fpp_input_* → fpp_input_*
                graphData.nodes.forEach(n => {
                    if (n.name.startsWith('output.fpp_input_')) {
                        let bestParent = null;
                        for (const [nm, candidate] of Object.entries(nodesByName)) {
                            if (nm.startsWith('fpp_input_') && n.name.startsWith('output.' + nm)) {
                                if (!bestParent || nm.length > bestParent.name.length) {
                                    bestParent = candidate;
                                }
                            }
                        }
                        if (bestParent) {
                            absorbed.add(n.id);
                            nodeIdRemap[n.id] = bestParent.id;
                            graphData.ports.forEach(p => {
                                if (p.nodeId === n.id) p.nodeId = bestParent.id;
                            });
                            if (n.state === 'running' && bestParent.state !== 'running') {
                                bestParent.state = n.state;
                            }
                        }
                    }
                });

                // 5) Routing hub sub-nodes: output.fpp_route_ig_* → fpp_route_ig_*
                //    Created when EQ is enabled on an input group.
                graphData.nodes.forEach(n => {
                    if (n.name.startsWith('output.fpp_route_ig_')) {
                        let bestParent = null;
                        for (const [nm, candidate] of Object.entries(nodesByName)) {
                            if (nm.startsWith('fpp_route_ig_') && n.name.startsWith('output.' + nm)) {
                                if (!bestParent || nm.length > bestParent.name.length) {
                                    bestParent = candidate;
                                }
                            }
                        }
                        if (bestParent) {
                            absorbed.add(n.id);
                            nodeIdRemap[n.id] = bestParent.id;
                            graphData.ports.forEach(p => {
                                if (p.nodeId === n.id) p.nodeId = bestParent.id;
                            });
                            if (n.state === 'running' && bestParent.state !== 'running') {
                                bestParent.state = n.state;
                            }
                        }
                    }
                });

                // 6) Routing hub → EQ: fpp_route_ig_N → fpp_fx_ig_N
                //    Merge the post-EQ routing combine-stream into the EQ node so
                //    the graph cleanly shows:  Input Group → EQ → Output Groups
                graphData.nodes.forEach(n => {
                    if (!n.name.startsWith('fpp_route_ig_') || absorbed.has(n.id)) return;
                    // Extract group ID: fpp_route_ig_{id}
                    const m = n.name.match(/^fpp_route_ig_(\d+)$/);
                    if (!m) return;
                    const eqName = 'fpp_fx_ig_' + m[1];
                    const eqNode = nodesByName[eqName];
                    if (eqNode && !absorbed.has(eqNode.id)) {
                        absorbed.add(n.id);
                        nodeIdRemap[n.id] = eqNode.id;
                        // Only the hub's outputs move onto the EQ.  Its inputs
                        // are fed by the EQ alone (a self-link once merged), so
                        // they stay behind for the orphan-port filter; moved,
                        // they were a second FL/FR on the EQ's input side.
                        graphData.ports.forEach(p => {
                            if (p.nodeId === n.id && p.direction === 'output') p.nodeId = eqNode.id;
                        });
                        if (n.state === 'running' && eqNode.state !== 'running') {
                            eqNode.state = n.state;
                        }
                    }
                });

                // 7) Combined A+V: fppd_video_stream_N → fppd_stream_N
                //    Both nodes come from the same GStreamer pipeline playing
                //    a media file with audio + video.  Merge video into the
                //    audio node so they appear as a single process.
                graphData.nodes.forEach(n => {
                    const m = n.name.match(/^fppd_video_stream_(\d+)$/);
                    if (!m || absorbed.has(n.id)) return;
                    const audioName = 'fppd_stream_' + m[1];
                    const audioNode = nodesByName[audioName];
                    if (audioNode && !absorbed.has(audioNode.id)) {
                        absorbed.add(n.id);
                        nodeIdRemap[n.id] = audioNode.id;
                        // Move video output ports to the audio node
                        graphData.ports.forEach(p => {
                            if (p.nodeId === n.id) {
                                p.nodeId = audioNode.id;
                                // Relabel video port for clarity
                                p.channel = 'Video';
                                p.name = 'output_Video';
                            }
                        });
                        // Copy video properties and mark as combined
                        audioNode.properties = audioNode.properties || {};
                        audioNode.properties['fpp.combined.av'] = true;
                        const vp = n.properties || {};
                        if (vp['fpp.video.stream']) audioNode.properties['fpp.video.stream'] = true;
                        if (vp['fpp.video.slot']) audioNode.properties['fpp.video.slot'] = vp['fpp.video.slot'];
                        // Update description to reflect combined A+V
                        audioNode.description = (audioNode.description || audioNode.name)
                            .replace('Media Stream', 'Media Stream')
                            + ' (A+V)';
                        if (n.state === 'running' && audioNode.state !== 'running') {
                            audioNode.state = n.state;
                        }
                    }
                });

                // 8) Video input source + audio source pairs:
                //    fpp_video_src_N_<slug> → fpp_audio_src_N_<slug>
                //    Video uses GStreamer intervideo (not PipeWire) so the video
                //    node is synthetic.  Merge it into the real PipeWire audio node
                //    so they appear as a single combined A+V source node.
                graphData.nodes.forEach(n => {
                    const m = n.name.match(/^fpp_video_src_(\d+_.*$)/);
                    if (!m || absorbed.has(n.id)) return;
                    const audioName = 'fpp_audio_src_' + m[1];
                    const audioNode = nodesByName[audioName];
                    if (audioNode && !absorbed.has(audioNode.id)) {
                        absorbed.add(n.id);
                        nodeIdRemap[n.id] = audioNode.id;
                        // Move video output ports to the audio node
                        graphData.ports.forEach(p => {
                            if (p.nodeId === n.id) {
                                p.nodeId = audioNode.id;
                                p.channel = 'Video';
                                p.name = 'output_Video';
                            }
                        });
                        // Copy video properties and mark as combined
                        audioNode.properties = audioNode.properties || {};
                        audioNode.properties['fpp.combined.av'] = true;
                        const vp = n.properties || {};
                        if (vp['fpp.video.stream']) audioNode.properties['fpp.video.stream'] = true;
                        if (vp['fpp.video.slot']) audioNode.properties['fpp.video.slot'] = vp['fpp.video.slot'];
                        if (vp['video.format']) audioNode.properties['video.format'] = vp['video.format'];
                        // Use the video source's better description
                        audioNode.description = (n.description || audioNode.description) + ' (A+V)';
                        // Promote mediaClass to show it's a combined source
                        audioNode.mediaClass = 'Video/Source';
                        if (n.state === 'running' && audioNode.state !== 'running') {
                            audioNode.state = n.state;
                        }
                    }
                });

                // Remove absorbed nodes
                graphData.nodes = graphData.nodes.filter(n => !absorbed.has(n.id));

                // Update links to reference parent nodes (and remap loopback portIds)
                graphData.links.forEach(l => {
                    if (nodeIdRemap[l.outputNodeId]) l.outputNodeId = nodeIdRemap[l.outputNodeId];
                    if (nodeIdRemap[l.inputNodeId]) l.inputNodeId = nodeIdRemap[l.inputNodeId];
                    if (portIdRemap[l.outputPortId]) l.outputPortId = portIdRemap[l.outputPortId];
                    if (portIdRemap[l.inputPortId]) l.inputPortId = portIdRemap[l.inputPortId];
                });

                // Remove self-links that result from merging
                graphData.links = graphData.links.filter(l => l.outputNodeId !== l.inputNodeId);

                // De-duplicate links (same output-port → input-port)
                const seen = new Set();
                graphData.links = graphData.links.filter(l => {
                    const key = l.outputPortId + '>' + l.inputPortId;
                    if (seen.has(key)) return false;
                    seen.add(key);
                    return true;
                });

                // Remove orphan ports (belonging to absorbed nodes that weren't remapped)
                const liveNodeIds = new Set(graphData.nodes.map(n => n.id));
                graphData.ports = graphData.ports.filter(p => liveNodeIds.has(p.nodeId));

                // Remove monitor ports — they're internal PipeWire plumbing
                // (monitor_FL, monitor_FR etc.) and carry no real audio links.
                // Exception: keep monitor ports on tee nodes (fpp_tee_*) since
                // those are the output ports that feed into input group loopbacks.
                const teeNodeIds = new Set(graphData.nodes.filter(n => n.name.startsWith('fpp_tee_')).map(n => n.id));
                graphData.ports = graphData.ports.filter(p =>
                    !p.name.startsWith('monitor_') || teeNodeIds.has(p.nodeId)
                );

                // Collapse duplicate output ports on group/EQ nodes. After merging
                // combine-stream outputs, a node may have 3× FL + 3× FR output
                // ports. Keep one canonical port per channel and redirect links.
                graphData.nodes.forEach(n => {
                    if (!n.name.startsWith('fpp_group_') && !n.name.startsWith('fpp_fx_ig_')) return;
                    const myPorts = graphData.ports.filter(p => p.nodeId === n.id && p.direction === 'output');
                    const canonical = {};    // channel → portId to keep
                    const remapPort = {};    // duplicate portId → canonical portId
                    myPorts.forEach(p => {
                        const ch = p.channel || p.name;
                        if (!canonical[ch]) {
                            canonical[ch] = p.id;
                        } else {
                            remapPort[p.id] = canonical[ch];
                        }
                    });
                    // Redirect links from duplicate ports to canonical
                    if (Object.keys(remapPort).length) {
                        graphData.links.forEach(l => {
                            if (remapPort[l.outputPortId]) l.outputPortId = remapPort[l.outputPortId];
                            if (remapPort[l.inputPortId]) l.inputPortId = remapPort[l.inputPortId];
                        });
                        // Remove duplicate ports
                        const dupes = new Set(Object.keys(remapPort).map(Number));
                        graphData.ports = graphData.ports.filter(p => !dupes.has(p.id));
                    }
                });
            }

            // ── Layout algorithm ──────────────────────────────────────────
            // 7-column layout.  Video outputs sit to the LEFT of the media
            // sources so video flows ← while audio flows →.
            // Video port dots are placed on the OPPOSITE side from audio dots
            // so that video wires naturally flow right-to-left.
            //   0: Video Outputs — fpp_video_group_*, fpp_hdmi_direct_*, Video/Sink
            //   1: Media Sources — fppd_stream_* (A+V), Audio/Source, Video/Source
            //   2: Input Tees   — fpp_tee_* null-sink fan-out nodes
            //   3: Input Groups  — fpp_input_* combine-stream mix buses, loopbacks
            //   4: Output Groups — fpp_group_* combine-stream sinks (existing)
            //   5: Effects       — delay filters, EQ nodes (fpp_fx_*, fpp_eq_*)
            //   6: HW Outputs    — ALSA sinks, AES67 / app capture (Stream/Input/Audio)
            const NUM_COLS = 7;
            const COL_LABELS = ['Video Outputs', 'Media Sources', 'Input Tees', 'Input Groups', 'Output Groups', 'Effects', 'HW Outputs'];

            function classifyColumn(n) {
                const nm = n.name || '';
                const mc = n.mediaClass || '';

                // Video outputs (col 0 — left of sources)
                if (nm.startsWith('fpp_hdmi_direct_')) return 0;
                if (nm.startsWith('fpp_video_group_')) return 0;
                if (mc === 'Stream/Input/Video' || mc === 'Video/Sink') return 0;

                // Video sources go into Media Sources (col 1)
                if (nm.startsWith('fppd_video_stream_')) return 1;
                if (mc === 'Stream/Output/Video' || mc === 'Video/Source') return 1;

                // Input tees (fan-out null-sinks)
                if (nm.startsWith('fpp_tee_fppd_stream_')) return 2;

                // Input groups (mix buses)
                if (nm.startsWith('fpp_input_')) return 3;
                if (nm.startsWith('fpp_loopback_ig')) return 3;
                if (nm.startsWith('input.fpp_loopback_ig') || nm.startsWith('output.fpp_loopback_ig')) return 3;

                // Output group sinks
                if (nm.startsWith('fpp_group_')) return 4;

                // Effects — delay filters, EQ, any fpp_fx_ node
                if (nm.startsWith('fpp_fx_') || nm.startsWith('fpp_eq_')) return 5;

                // Routing hub (post-EQ fan-out) — normally merged into EQ node;
                // fallback classification if merge didn't absorb it
                if (nm.startsWith('fpp_route_ig_')) return 3;
                if (nm.startsWith('output.fpp_route_ig_')) return 3;

                // ALSA hardware outputs
                if (mc === 'Audio/Sink' && nm.startsWith('alsa_')) return 6;

                // AES67 / app capture sinks (Stream/Input/Audio)
                if (mc === 'Stream/Input/Audio') return 6;

                // Audio sources (mic inputs, ALSA capture)
                if (mc === 'Audio/Source') return 1;

                // Stream outputs (fppd playback streams)
                if (mc === 'Stream/Output/Audio') return 1;

                // Any other Audio/Sink not caught above (e.g. custom)
                if (mc === 'Audio/Sink') return 6;

                // Fallback
                return 1;
            }

            // ── Video port side-flipping ───────────────────────────────────
            // Video ports are placed on the OPPOSITE side of the node from
            // audio ports so that video wires flow right-to-left.
            function isVideoPort(port, node) {
                // Explicitly labelled video port (from merge)
                if (port.channel === 'Video' || port.name === 'output_Video') return true;
                if (port.channel === 'video' || port.name === 'input_video') return true;
                // Combined A+V nodes have both audio and video ports — only
                // ports explicitly labelled as video should be flipped.
                const props = (node && node.properties) || {};
                if (props['fpp.combined.av']) return false;
                // Pure video nodes: all ports are video
                const mc = (node && node.mediaClass) || '';
                if (mc.includes('Video')) return true;
                return false;
            }

            function portSide(port, node) {
                const video = isVideoPort(port, node);
                if (port.direction === 'output') return video ? 'left' : 'right';
                return video ? 'right' : 'left';
            }

            function layoutGraph() {
                // Build node lookup for port side classification
                const _nodesById = {};
                graphData.nodes.forEach(n => { _nodesById[n.id] = n; });

                // Compute port counts per node (left / right side)
                const portsPerNode = {};
                graphData.ports.forEach(p => {
                    if (!portsPerNode[p.nodeId]) portsPerNode[p.nodeId] = { left: 0, right: 0 };
                    const side = portSide(p, _nodesById[p.nodeId]);
                    if (side === 'left') portsPerNode[p.nodeId].left++;
                    else portsPerNode[p.nodeId].right++;
                });

                // Assign each node to a column
                const cols = {};
                for (let i = 0; i < NUM_COLS; i++) cols[i] = [];
                graphData.nodes.forEach(n => {
                    n._col = classifyColumn(n);
                    cols[n._col].push(n);
                });

                // ── Build node-level adjacency for crossing minimization ──
                const nodesById = {};
                graphData.nodes.forEach(n => { nodesById[n.id] = n; });

                // Collapse port-level links to node-level neighbour sets
                const neighbors = {};  // nodeId → Set of connected nodeIds
                graphData.links.forEach(l => {
                    const a = l.outputNodeId, b = l.inputNodeId;
                    if (a === b) return;
                    if (!neighbors[a]) neighbors[a] = new Set();
                    if (!neighbors[b]) neighbors[b] = new Set();
                    neighbors[a].add(b);
                    neighbors[b].add(a);
                });

                // ── Barycenter crossing minimization ──────────────────────
                // Helper: sub-group priority for Media Sources column (1).
                // Media streams first (0), video sources second (1), HW sources third (2).
                function inputSourceGroup(n) {
                    const mc = n.mediaClass || '';
                    if (mc === 'Stream/Output/Audio') return 0; // media streams at top
                    if (mc === 'Stream/Output/Video' || mc === 'Video/Source') return 1;
                    return 2; // HW sources (Audio/Source) below
                }

                // Assign initial order indices within each column (alphabetical seed)
                for (let c = 0; c < NUM_COLS; c++) {
                    if (c === 1) {
                        // Column 1 (Media Sources): group media streams above HW sources
                        cols[c].sort((a, b) => {
                            const ga = inputSourceGroup(a), gb = inputSourceGroup(b);
                            if (ga !== gb) return ga - gb;
                            return (a.description || a.name).localeCompare(b.description || b.name);
                        });
                    } else {
                        cols[c].sort((a, b) => (a.description || a.name).localeCompare(b.description || b.name));
                    }
                    cols[c].forEach((n, i) => { n._order = i; });
                }

                // Helper: compute node heights for Y positions
                function nodeHeight(n) {
                    const pc = portsPerNode[n.id] || { left: 0, right: 0 };
                    return Math.max(NODE_H_BASE, PORT_Y_START + Math.max(pc.left, pc.right, 1) * PORT_SPACING + 4);
                }

                // Helper: compute temporary Y centers based on current order
                function assignTempY(colNodes) {
                    let y = 0;
                    colNodes.forEach(n => {
                        const h = nodeHeight(n);
                        n._tempCY = y + h / 2;  // center Y
                        y += h + PADDING.rowGap;
                    });
                }

                // Barycenter sort: order nodes in targetCol by the average
                // center-Y of their neighbours in refCol
                function barySort(targetCol, refCol) {
                    const refSet = new Set(cols[refCol].map(n => n.id));
                    assignTempY(cols[refCol]);

                    // For each node in targetCol, compute barycenter of its
                    // neighbours that are in refCol
                    cols[targetCol].forEach(n => {
                        const nbrs = neighbors[n.id] || new Set();
                        let sum = 0, count = 0;
                        nbrs.forEach(nid => {
                            const nb = nodesById[nid];
                            if (nb && refSet.has(nid)) {
                                sum += nb._tempCY;
                                count++;
                            }
                        });
                        n._bary = count > 0 ? sum / count : n._order * 100;
                    });

                    cols[targetCol].sort((a, b) => a._bary - b._bary);
                    // Column 1: preserve media-stream / HW-source grouping
                    if (targetCol === 1) {
                        cols[1].sort((a, b) => {
                            const ga = inputSourceGroup(a), gb = inputSourceGroup(b);
                            if (ga !== gb) return ga - gb;
                            return a._bary - b._bary;
                        });
                    }
                    cols[targetCol].forEach((n, i) => { n._order = i; });
                }

                // Run several sweeps: left→right then right→left
                for (let pass = 0; pass < 4; pass++) {
                    // Forward sweep (use left neighbour col to sort right col)
                    for (let c = 1; c < NUM_COLS; c++) {
                        if (cols[c].length > 1) barySort(c, c - 1);
                    }
                    // Backward sweep
                    for (let c = NUM_COLS - 2; c >= 0; c--) {
                        if (cols[c].length > 1) barySort(c, c + 1);
                    }
                }

                // ── Assign final positions ────────────────────────────────
                for (let ci = 0; ci < NUM_COLS; ci++) {
                    let y = PADDING.top + 30; // room for column header
                    cols[ci].forEach(n => {
                        const h = nodeHeight(n);
                        if (n._x === undefined) {
                            n._x = PADDING.left + ci * (NODE_W + PADDING.colGap);
                            n._y = y;
                        }
                        n._w = NODE_W;
                        n._h = h;
                        const pc = portsPerNode[n.id] || { left: 0, right: 0 };
                        n._portsLeft = pc.left;
                        n._portsRight = pc.right;
                        y += h + PADDING.rowGap;
                    });
                }
            }

            // ── Rendering ─────────────────────────────────────────────────
            function render() {
                layoutGraph();

                // ── Background region boxes ────────────────────────
                gBackground.selectAll('*').remove();

                // Compute vertical extent of all nodes
                let contentMaxY = PADDING.top + 200;
                graphData.nodes.forEach(n => {
                    contentMaxY = Math.max(contentMaxY, n._y + n._h);
                });

                const boxPad = 20;
                const boxTop = PADDING.top - 20;
                const boxBottom = contentMaxY + boxPad;

                // Split at center of Media Sources column (col 1)
                const col1Center = PADDING.left + 1 * (NODE_W + PADDING.colGap) + NODE_W / 2;
                const videoLeft = PADDING.left - boxPad;
                const audioRight = PADDING.left + 6 * (NODE_W + PADDING.colGap) + NODE_W + boxPad;

                // Video Routing box (col 0 through center of col 1)
                gBackground.append('rect')
                    .attr('x', videoLeft).attr('y', boxTop)
                    .attr('width', col1Center - videoLeft)
                    .attr('height', boxBottom - boxTop)
                    .attr('rx', 12)
                    .attr('fill', 'rgba(13, 202, 240, 0.06)')
                    .attr('stroke', 'rgba(13, 202, 240, 0.25)')
                    .attr('stroke-width', 1.5)
                    .attr('stroke-dasharray', '6 3');
                gBackground.append('text')
                    .attr('x', (videoLeft + col1Center) / 2)
                    .attr('y', boxTop + 14)
                    .attr('text-anchor', 'middle')
                    .attr('font-size', '14px').attr('font-weight', '700')
                    .attr('fill', 'rgba(13, 40, 46, 0.5)')
                    .attr('letter-spacing', '1px')
                    .text('VIDEO ROUTING');

                // Audio Routing box (center of col 1 through col 6)
                gBackground.append('rect')
                    .attr('x', col1Center).attr('y', boxTop)
                    .attr('width', audioRight - col1Center)
                    .attr('height', boxBottom - boxTop)
                    .attr('rx', 12)
                    .attr('fill', 'rgba(253, 126, 20, 0.06)')
                    .attr('stroke', 'rgba(253, 126, 20, 0.25)')
                    .attr('stroke-width', 1.5)
                    .attr('stroke-dasharray', '6 3');
                gBackground.append('text')
                    .attr('x', (col1Center + audioRight) / 2)
                    .attr('y', boxTop + 14)
                    .attr('text-anchor', 'middle')
                    .attr('font-size', '14px').attr('font-weight', '700')
                    .attr('fill', 'rgba(253, 126, 20, 0.5)')
                    .attr('letter-spacing', '1px')
                    .text('AUDIO ROUTING');

                // Draw column headers
                gRoot.selectAll('.pw-col-header').remove();
                for (let ci = 0; ci < NUM_COLS; ci++) {
                    const x = PADDING.left + ci * (NODE_W + PADDING.colGap) + NODE_W / 2;
                    gRoot.append('text')
                        .attr('class', 'pw-col-header')
                        .attr('x', x)
                        .attr('y', PADDING.top + 10)
                        .attr('text-anchor', 'middle')
                        .attr('fill', 'var(--bs-secondary-color, #6c757d)')
                        .attr('font-size', '13px')
                        .attr('font-weight', '700')
                        .attr('letter-spacing', '0.5px')
                        .text(COL_LABELS[ci]);
                }

                const nodesById = {};
                graphData.nodes.forEach(n => nodesById[n.id] = n);

                // Build port position map — grouped by left/right side
                // (video ports are flipped: outputs→left, inputs→right)
                const portPositions = {};  // portId → {x, y}
                const nodePortsLeft = {};  // nodeId → [port, ...]
                const nodePortsRight = {};
                graphData.ports.forEach(p => {
                    const side = portSide(p, nodesById[p.nodeId]);
                    const bucket = side === 'left' ? nodePortsLeft : nodePortsRight;
                    if (!bucket[p.nodeId]) bucket[p.nodeId] = [];
                    bucket[p.nodeId].push(p);
                });

                // Assign port positions relative to their node
                graphData.nodes.forEach(n => {
                    const lefts = nodePortsLeft[n.id] || [];
                    const rights = nodePortsRight[n.id] || [];
                    lefts.forEach((p, i) => {
                        portPositions[p.id] = {
                            x: n._x,
                            y: n._y + PORT_Y_START + i * PORT_SPACING + PORT_SPACING / 2
                        };
                    });
                    rights.forEach((p, i) => {
                        portPositions[p.id] = {
                            x: n._x + n._w,
                            y: n._y + PORT_Y_START + i * PORT_SPACING + PORT_SPACING / 2
                        };
                    });
                });

                // ── Links ─────────────────────────────────
                gLinks.selectAll('.pw-link').remove();
                graphData.links.forEach(l => {
                    const p1 = portPositions[l.outputPortId];
                    const p2 = portPositions[l.inputPortId];
                    if (!p1 || !p2) return;

                    const dx = Math.abs(p2.x - p1.x) * 0.5;
                    const dir = p2.x >= p1.x ? 1 : -1;
                    const path = `M${p1.x},${p1.y} C${p1.x + dir * dx},${p1.y} ${p2.x - dir * dx},${p2.y} ${p2.x},${p2.y}`;
                    const stateClass = (l.state || '').toLowerCase();

                    gLinks.append('path')
                        .attr('class', 'pw-link ' + stateClass)
                        .attr('d', path);
                });

                // ── Nodes ─────────────────────────────────
                gNodes.selectAll('.pw-node').remove();

                const nodeGroups = gNodes.selectAll('.pw-node')
                    .data(graphData.nodes, d => d.id)
                    .enter()
                    .append('g')
                    .attr('class', d => 'pw-node' + (d.id === selectedNodeId ? ' selected' : ''))
                    .attr('transform', d => `translate(${d._x},${d._y})`)
                    .on('click', (e, d) => {
                        e.stopPropagation();
                        selectNode(d);
                    })
                    .call(d3.drag()
                        .on('start', dragStarted)
                        .on('drag', dragged)
                        .on('end', dragEnded));

                // Set opacity for virtual/inactive nodes (media sources stay opaque
                // so they render above the column background boxes)
                nodeGroups.style('opacity', d => {
                    const p = d.properties || {};
                    const col = d._col;
                    if (p['fpp.stream.virtual'] && col !== 1) return 0.45;
                    return 1.0;
                });

                // Background rect
                nodeGroups.append('rect')
                    .attr('width', d => d._w)
                    .attr('height', d => d._h)
                    .attr('fill', d => {
                        const base = nodeColor(d);
                        const p = d.properties || {};
                        if (p['fpp.stream.virtual'] && d._col === 1) {
                            const c = d3.color(base);
                            // Desaturate and lighten to a very pale version
                            return d3.interpolate(base, '#e8e8e8')(0.7);
                        }
                        return base;
                    })
                    .attr('stroke', d => {
                        const p = d.properties || {};
                        return p['fpp.stream.virtual']
                            ? '#6c757d'
                            : d3.color(nodeColor(d)).darker(0.6);
                    })
                    .attr('stroke-dasharray', d => {
                        const p = d.properties || {};
                        return p['fpp.stream.virtual'] ? '6 3' : null;
                    });

                // State indicator (small circle top-right)
                nodeGroups.append('circle')
                    .attr('cx', d => d._w - 10)
                    .attr('cy', 10)
                    .attr('r', 4)
                    .attr('fill', d => {
                        const s = (d.state || '').toLowerCase();
                        if (s === 'running') return '#00ff88';
                        if (s === 'idle') return '#ffc107';
                        return '#adb5bd';
                    })
                    .attr('stroke', 'rgba(0,0,0,0.2)')
                    .attr('stroke-width', 1);

                // Title – prefer ALSA card name for HW outputs (more human-readable)
                nodeGroups.append('text')
                    .attr('class', 'pw-node-label')
                    .attr('x', 10).attr('y', 16)
                    .style('fill', d => {
                        const p = d.properties || {};
                        return (p['fpp.stream.virtual'] && d._col === 1) ? '#555' : null;
                    })
                    .text(d => {
                        const p = d.properties || {};
                        if ((d.name || '').startsWith('alsa_') && p['api.alsa.card.name']) {
                            return truncate(p['api.alsa.card.name'], 28);
                        }
                        return truncate(d.description || d.name, 28);
                    });

                // Subtitle (media class)
                nodeGroups.append('text')
                    .attr('class', 'pw-node-sublabel')
                    .attr('x', 10).attr('y', 30)
                    .style('fill', d => {
                        const p = d.properties || {};
                        return (p['fpp.stream.virtual'] && d._col === 1) ? 'rgba(80,80,80,0.7)' : null;
                    })
                    .text(d => d.mediaClass || '');

                // Metadata line (delay, format, etc.)
                nodeGroups.append('text')
                    .attr('class', 'pw-node-meta')
                    .attr('x', 10).attr('y', 41)
                    .style('fill', d => {
                        const p = d.properties || {};
                        return (p['fpp.stream.virtual'] && d._col === 1) ? 'rgba(80,80,80,0.5)' : null;
                    })
                    .text(d => nodeMetaText(d));

                // Draw ports as circles on the node groups
                // Video ports are on the opposite side from audio ports
                nodeGroups.each(function (n) {
                    const g = d3.select(this);
                    const lefts = nodePortsLeft[n.id] || [];
                    const rights = nodePortsRight[n.id] || [];

                    lefts.forEach((p, i) => {
                        const py = PORT_Y_START + i * PORT_SPACING + PORT_SPACING / 2;
                        g.append('circle')
                            .attr('class', 'pw-port-dot')
                            .attr('cx', 0).attr('cy', py)
                            .attr('r', PORT_R)
                            .attr('fill', nodeColor(n))
                            .attr('stroke', '#fff')
                            .attr('stroke-width', 1.5);
                        g.append('text')
                            .attr('class', 'pw-port-label')
                            .attr('x', -8).attr('y', py + 3)
                            .attr('text-anchor', 'end')
                            .text(portLabel(p));
                    });

                    rights.forEach((p, i) => {
                        const py = PORT_Y_START + i * PORT_SPACING + PORT_SPACING / 2;
                        g.append('circle')
                            .attr('class', 'pw-port-dot')
                            .attr('cx', n._w).attr('cy', py)
                            .attr('r', PORT_R)
                            .attr('fill', nodeColor(n))
                            .attr('stroke', '#fff')
                            .attr('stroke-width', 1.5);
                        g.append('text')
                            .attr('class', 'pw-port-label')
                            .attr('x', n._w + 8).attr('y', py + 3)
                            .attr('text-anchor', 'start')
                            .text(portLabel(p));
                    });
                });

                // Stats
                $('#graphStats').text(
                    graphData.nodes.length + ' nodes, ' +
                    graphData.links.length + ' links'
                );
            }

            function portLabel(p) {
                // Show channel name if available (FL, FR), else port.name
                if (p.channel) return p.channel;
                const n = p.name || '';
                return n.replace('playback_', '').replace('output_', '').replace('input_', '');
            }

            function truncate(s, len) {
                return s.length > len ? s.substring(0, len - 1) + '…' : s;
            }

            // ── Drag behaviour ────────────────────────────────────────────
            function dragStarted(e, d) {
                d3.select(this).raise();
            }

            function dragged(e, d) {
                d._x += e.dx;
                d._y += e.dy;
                d3.select(this).attr('transform', `translate(${d._x},${d._y})`);
                rerouteLinks();
            }

            function dragEnded(e, d) {
                // positions stay where user dropped
            }

            function rerouteLinks() {
                // Rebuild port positions from node positions (left/right side)
                const portPositions = {};
                const nodesById = {};
                graphData.nodes.forEach(n => { nodesById[n.id] = n; });
                const nodePortsLeft = {};
                const nodePortsRight = {};
                graphData.ports.forEach(p => {
                    const side = portSide(p, nodesById[p.nodeId]);
                    const bucket = side === 'left' ? nodePortsLeft : nodePortsRight;
                    if (!bucket[p.nodeId]) bucket[p.nodeId] = [];
                    bucket[p.nodeId].push(p);
                });
                graphData.nodes.forEach(n => {
                    const lefts = nodePortsLeft[n.id] || [];
                    const rights = nodePortsRight[n.id] || [];
                    lefts.forEach((p, i) => {
                        portPositions[p.id] = {
                            x: n._x,
                            y: n._y + PORT_Y_START + i * PORT_SPACING + PORT_SPACING / 2
                        };
                    });
                    rights.forEach((p, i) => {
                        portPositions[p.id] = {
                            x: n._x + n._w,
                            y: n._y + PORT_Y_START + i * PORT_SPACING + PORT_SPACING / 2
                        };
                    });
                });

                gLinks.selectAll('.pw-link').remove();
                graphData.links.forEach(l => {
                    const p1 = portPositions[l.outputPortId];
                    const p2 = portPositions[l.inputPortId];
                    if (!p1 || !p2) return;
                    const dx = Math.abs(p2.x - p1.x) * 0.5;
                    const dir = p2.x >= p1.x ? 1 : -1;
                    const path = `M${p1.x},${p1.y} C${p1.x + dir * dx},${p1.y} ${p2.x - dir * dx},${p2.y} ${p2.x},${p2.y}`;
                    gLinks.append('path')
                        .attr('class', 'pw-link ' + (l.state || '').toLowerCase())
                        .attr('d', path);
                });
            }

            // ── Node detail panel ─────────────────────────────────────────
            function selectNode(d) {
                selectedNodeId = d.id;
                gNodes.selectAll('.pw-node').classed('selected', n => n.id === d.id);

                let html = '<table>';
                html += row('ID', d.id);
                html += row('Name', d.name);
                html += row('Description', d.description);
                html += row('Media Class', d.mediaClass);
                html += row('State', stateBadge(d.state));
                html += row('Factory', d.factory);

                if (d.properties && Object.keys(d.properties).length) {
                    html += '<tr><td colspan="2"><hr class="my-1"></td></tr>';
                    const propLabels = {
                        'fpp.delay.ms': 'Delay (ms)',
                        'fpp.eq.enabled': 'EQ Enabled',
                        'fpp.group.members': 'Members',
                        'fpp.group.latencyCompensate': 'Latency Compensation'
                    };
                    for (const [k, v] of Object.entries(d.properties)) {
                        const label = propLabels[k] || k;
                        let val = v;
                        if (typeof v === 'boolean') val = v ? 'Yes' : 'No';
                        html += row(label, val);
                    }
                }

                // Show connected ports
                const myPorts = graphData.ports.filter(p => p.nodeId === d.id);
                if (myPorts.length) {
                    html += '<tr><td colspan="2"><hr class="my-1"></td></tr>';
                    html += '<tr><td colspan="2"><strong>Ports (' + myPorts.length + ')</strong></td></tr>';
                    myPorts.forEach(p => {
                        const dir = p.direction === 'input' ? '← in' : '→ out';
                        html += row(dir, p.name + (p.channel ? ' [' + p.channel + ']' : ''));
                    });
                }

                html += '</table>';

                $('#pw-detail-title').text(d.description || d.name);
                $('#pw-detail-body').html(html);
                $('#pw-detail-panel').addClass('show');
            }

            function row(label, val) {
                return '<tr><td>' + label + '</td><td>' + (val != null ? val : '') + '</td></tr>';
            }

            function stateBadge(state) {
                const s = (state || 'unknown').toLowerCase();
                return '<span class="state-badge state-' + s + '">' + state + '</span>';
            }

            // ── Fit view ──────────────────────────────────────────────────
            function fitView() {
                if (!graphData.nodes.length) return;

                let minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
                graphData.nodes.forEach(n => {
                    minX = Math.min(minX, n._x - 60);
                    minY = Math.min(minY, n._y - 20);
                    maxX = Math.max(maxX, n._x + n._w + 60);
                    maxY = Math.max(maxY, n._y + n._h + 20);
                });

                const svgEl = document.getElementById('pw-svg');
                const W = svgEl.clientWidth || 900;
                const H = svgEl.clientHeight || 600;
                const gw = maxX - minX;
                const gh = maxY - minY;
                const scale = Math.min(W / gw, H / gh, 1.5) * 0.9;
                const tx = (W - gw * scale) / 2 - minX * scale;
                const ty = (H - gh * scale) / 2 - minY * scale;

                svg.transition().duration(500)
                    .call(zoom.transform, d3.zoomIdentity.translate(tx, ty).scale(scale));
            }

            // ── Event wiring ──────────────────────────────────────────────
            $('#btnRefresh').on('click', () => {
                fetchGraph().then(() => { render(); fitView(); });
            });
            $('#btnFitView').on('click', fitView);
            $('#btnResetLayout').on('click', () => {
                graphData.nodes.forEach(n => { delete n._x; delete n._y; });
                render(); fitView();
            });
            $('#btnCloseDetail').on('click', () => {
                selectedNodeId = null;
                gNodes.selectAll('.pw-node').classed('selected', false);
                $('#pw-detail-panel').removeClass('show');
            });
            svg.on('click', () => {
                selectedNodeId = null;
                gNodes.selectAll('.pw-node').classed('selected', false);
                $('#pw-detail-panel').removeClass('show');
            });

            // ── Initial load ──────────────────────────────────────────────
            fetchGraph().then(() => {
                render();
                // Wait a tick for SVG size to settle
                setTimeout(fitView, 100);
            });

            // Auto-refresh every 10 seconds (preserve positions)
            setInterval(() => {
                // Capture positions before fetch overwrites graphData
                const oldPos = {};
                graphData.nodes.forEach(n => {
                    if (n._x !== undefined) oldPos[n.id] = { x: n._x, y: n._y, w: n._w, h: n._h };
                });
                fetchGraph().then(() => {
                    // Restore dragged positions
                    graphData.nodes.forEach(n => {
                        if (oldPos[n.id]) {
                            n._x = oldPos[n.id].x;
                            n._y = oldPos[n.id].y;
                            n._w = oldPos[n.id].w;
                            n._h = oldPos[n.id].h;
                        }
                    });
                    render();
                });
            }, 10000);

        })();
    </script>

</body>

</html>