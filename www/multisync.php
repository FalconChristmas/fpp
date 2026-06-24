<!DOCTYPE html>
<html lang="en">

<head>
    <?php
    include 'common/htmlMeta.inc';
    require_once "config.php";
    require_once "common.php";
    include 'common/menuHead.inc';
    ?>
    <script type="text/javascript" src="bootstrap-table/js/bootstrap-table.min.js"></script>
    <script type="text/javascript" src="bootstrap-table/extensions/bootstrap-table-filter-control.min.js"></script>
    <link rel="stylesheet" href="bootstrap-table/css/bootstrap-table.min.css" />
    <link rel="stylesheet" href="bootstrap-table/extensions/bootstrap-table-filter-control.min.css" />

    <script type="text/javascript" src="js/xlsx.full.min.js" async></script>
    <script type="text/javascript" src="js/FileSaver.min.js" async></script>

    <title><? echo $pageTitle; ?></title>
    <!-- TODO: extract to www/css/multisync.css when ready to split into external files -->
    <style>
        /* === Action options === */
        .actionOptions {
            display: none;
        }

        /* === Channel I/O popover === */
        .channel-io-icons {
            display: inline-block;
            margin-left: 4px;
            font-size: 0.95em;
        }

        .channel-io-icons i {
            margin: 0 1px;
        }

        .channel-io-icon-input {
            color: var(--bs-info);
        }

        .channel-io-icon-output {
            color: var(--bs-success);
        }

        .channel-io-icon-output[data-ip],
        .channel-io-icon-input[data-ip] {
            cursor: pointer;
        }

        .channel-io-tooltip-table {
            font-size: 0.95em;
            border-collapse: collapse;
            white-space: nowrap;
            margin: 0;
        }

        .channel-io-tooltip-table th {
            padding: 2px 6px;
            border-bottom: 1px solid #555;
            text-align: left;
            font-weight: bold;
        }

        .channel-io-tooltip-table td {
            padding: 2px 6px;
        }

        .channel-io-tooltip-table tr:nth-child(even) {
            background-color: rgba(255, 255, 255, 0.05);
        }

        .channel-io-popover {
            max-width: 500px !important;
        }

        .channel-io-popover .popover-body {
            padding: 6px 8px;
            max-height: 300px;
            overflow-y: auto;
        }

        /* === Bootstrap popover (column selector target) === */
        #popover-target label {
            margin: 0 5px;
            display: block;
        }

        #popover-target input {
            margin-right: 5px;
        }

        #popover-target .disabled {
            color: var(--fpp-text-light);
        }

        /* === Table sort icons === */
        /* structural/theming counterparts live in fpp.css / fpp-dark.css */
        #fppSystemsTable thead th .both {
            background-image: url('data:image/svg+xml;utf8,<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 320 512"><path fill="%23888" d="m103.05877,41.4c9.37707,-12.5 24.60541,-12.5 33.98248,0l96.02113,128c6.90152,9.2 8.92696,22.9 5.17614,34.9s-12.45274,19.8 -22.20489,19.8l-192.04225,-0.1c-9.67713,0 -18.45406,-7.8 -22.20489,-19.8s-1.65036,-25.7 5.17614,-34.9l96.02113,-128l0.07501,0.1z"/><path fill="%23888" d="m103.05877,470.7l-96.02113,-128c-6.90152,-9.2 -8.92696,-22.9 -5.17614,-34.9s12.45274,-19.8 22.20489,-19.8l192.04225,0c9.67713,0 18.45406,7.8 22.20489,19.8s1.65036,25.7 -5.17614,34.9l-96.02113,128c-9.37707,12.5 -24.60541,12.5 -33.98248,0l-0.07501,0z"/></svg>');
        }

        /* Active sort: show both arrows, active one dark blue, inactive one light grey */
        #fppSystemsTable thead th .asc {
            background-image: url('data:image/svg+xml;utf8,<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 320 512"><path fill="%230d47a1" d="m103.05877,41.4c9.37707,-12.5 24.60541,-12.5 33.98248,0l96.02113,128c6.90152,9.2 8.92696,22.9 5.17614,34.9s-12.45274,19.8 -22.20489,19.8l-192.04225,-0.1c-9.67713,0 -18.45406,-7.8 -22.20489,-19.8s-1.65036,-25.7 5.17614,-34.9l96.02113,-128l0.07501,0.1z"/><path fill="%23ccc" d="m103.05877,470.7l-96.02113,-128c-6.90152,-9.2 -8.92696,-22.9 -5.17614,-34.9s12.45274,-19.8 22.20489,-19.8l192.04225,0c9.67713,0 18.45406,7.8 22.20489,19.8s1.65036,25.7 -5.17614,34.9l-96.02113,128c-9.37707,12.5 -24.60541,12.5 -33.98248,0l-0.07501,0z"/></svg>');
        }

        #fppSystemsTable thead th .desc {
            background-image: url('data:image/svg+xml;utf8,<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 320 512"><path fill="%23ccc" d="m103.05877,41.4c9.37707,-12.5 24.60541,-12.5 33.98248,0l96.02113,128c6.90152,9.2 8.92696,22.9 5.17614,34.9s-12.45274,19.8 -22.20489,19.8l-192.04225,-0.1c-9.67713,0 -18.45406,-7.8 -22.20489,-19.8s-1.65036,-25.7 5.17614,-34.9l96.02113,-128l0.07501,0.1z"/><path fill="%230d47a1" d="m103.05877,470.7l-96.02113,-128c-6.90152,-9.2 -8.92696,-22.9 -5.17614,-34.9s12.45274,-19.8 22.20489,-19.8l192.04225,0c9.67713,0 18.45406,7.8 22.20489,19.8s1.65036,25.7 -5.17614,34.9l-96.02113,128c-9.37707,12.5 -24.60541,12.5 -33.98248,0l-0.07501,0z"/></svg>');
        }

        /* === Column selector === */
        #columnSelector input[type="checkbox"] {
            margin-right: 8px;
            margin-bottom: 2px;
        }

        #columnSelector label:first-of-type {
            border-bottom: 2px dotted var(--bs-body-color);
            /* Adjust thickness and color as needed */
            padding-bottom: 1px;
            /* Adds spacing between the label and the border */
            display: block;
            /* Ensures the border spans the full width */

        }

        #columnSelector label:nth-of-type(2) {
            margin-top: -8px;
        }

        /* === Drag-and-drop reorder mode === */
        .reorder-grip {
            display: none;
            cursor: grab;
            padding-right: 6px;
            color: #adb5bd;
            float: left;
        }

        .reorder-mode .reorder-grip {
            display: inline-block;
        }

        .reorder-mode .systemRow:hover {
            background-color: rgba(0, 123, 255, 0.08) !important;
        }

        .reorder-mode .ui-sortable-helper {
            background-color: #e3f2fd !important;
            box-shadow: 0 2px 8px rgba(0, 0, 0, 0.15);
        }

        .reorder-mode .ui-sortable-placeholder {
            background-color: var(--bs-tertiary-bg);
            border: 2px dashed var(--bs-gray-700);
            visibility: visible !important;
            height: 2.5em;
        }
    </style>

    <script>
        // Migrate some inline php code to javascript to enhance readability
        window.fppConfig = {
            hideExternalURLs: <?= json_encode((bool)$settings['hideExternalURLs']) ?>,
            uiLevel: <?= json_encode((int)$uiLevel) ?>,
            serverName: <?= json_encode($_SERVER['SERVER_NAME'] ?? '') ?>,
            serverAddr: <?= json_encode($_SERVER['SERVER_ADDR'] ?? '') ?>
        };
    </script>

    <!-- TODO: when ready to add a build step, extract each section to its listed future file -->
    <script>
        // ============================================================
        // SECTION: Config / globals
        // ============================================================

        var hostRows = new Object();
        var rowSpans = new Object();
        var systemStatusCache = {}; // Cache of api/system/status?ip[]=
        var localFpposFiles = [];
        var proxies = [];
        var savedDisplayOrder = <?php
        $savedDisplayOrderStr = GetSettingValue('MultiSyncDisplayOrder', '');
        if (!empty($savedDisplayOrderStr)) {
            // Check for legacy JSON object format and convert
            $decoded = json_decode($savedDisplayOrderStr, true);
            if ($decoded !== null && isset($decoded['order'])) {
                // Legacy JSON format - convert and re-save as pipe-delimited
                $parts = $decoded['order'];
                WriteSettingToFile('MultiSyncDisplayOrder', implode('|', $parts));
            } else {
                // Pipe-delimited format: "uuid:abc|host:xyz|..."
                $parts = explode('|', $savedDisplayOrderStr);
            }
            echo json_encode(array_values(array_filter($parts, function ($v) {
                return $v !== '';
            })));
        } else {
            echo '[]';
        }
        ?>;

        class AutoRefreshController {
            constructor(intervalMs = 2000) {
                this._timer = null;
                this._intervalMs = intervalMs;
            }
            schedule(fn) {
                this.cancel();
                this._timer = setTimeout(() => {
                    this._timer = null;
                    fn();
                }, this._intervalMs);
            }
            cancel() {
                if (this._timer !== null) {
                    clearTimeout(this._timer);
                    this._timer = null;
                }
            }
        }

        /**
         * Returns true when the user is actively working with the table in a way
         * that should suppress scheduling the NEXT auto-refresh tick.
         *
         * WHY: safeInitBody() already preserves focus/order/checks on the current
         * in-flight render.  But if a filter dropdown is open, the safest UX is to
         * also skip scheduling the *next* tick — this gives a full 2 s grace period
         * after the last completed tick before the next one fires.  Reorder mode is
         * included because a drag-in-progress must never be interrupted by a re-render
         * that resets bt.data order back to pre-drag.
         */
        function isUserInteracting() {
            if (reorderModeActive) return true;
            var active = document.activeElement;
            return !!(active && $(active).closest('.filter-control').length);
        }

        const fppPoller     = new AutoRefreshController();
        const falconPoller  = new AutoRefreshController();
        const wledPoller    = new AutoRefreshController();
        const geniusPoller  = new AutoRefreshController();
        const baldrickPoller = new AutoRefreshController();

        var unavailables = [];
        var reorderModeActive = false;
        // Captured synchronously in the jQuery UI sortable 'stop' callback so
        // safeInitBody() has a reliable snapshot of the post-drag row order.
        // Reading the live DOM from a setTimeout (poll tick) turns out to be
        // unreliable — the DOM may appear reverted by the time the tick fires.
        var _reorderCommittedOrder = null;

        var channelOutputConfigCache = {};
        var channelInputConfigCache = {};

        // Universe type ID to display name mapping
        const universeTypeNames = {
            0: 'E1.31 Multicast',
            1: 'E1.31 Unicast',
            2: 'ArtNet Broadcast',
            3: 'ArtNet Unicast',
            4: 'DDP Raw',
            5: 'DDP',
            6: 'KiNet v1',
            7: 'KiNet v2',
            8: 'Twinkly',
            9: 'ArtNet Unicast'
        };

        var streamCount = 0;

        // ============================================================
        // SECTION: Device type classifiers
        // ============================================================

        // Lookup sets for each device category.
        // Range-based categories are generated programmatically to keep
        // this readable while remaining identical in behavior to the
        // original switch/if-chain implementations.
        const DEVICE_TYPE_SETS = {
            // 0x01–0x7F  (typeId >= 1 && typeId < 128)
            FPP:           new Set(Array.from({ length: 127 }, function (_, i) { return i + 1; })),
            // 0x02–0x3F  (typeId >= 2 && typeId <= 63)
            FPP_PI:        new Set(Array.from({ length: 62 },  function (_, i) { return i + 2; })),
            // 0x41–0x5F  (typeId >= 65 && typeId <= 95)
            FPP_BEAGLEBONE: new Set(Array.from({ length: 31 }, function (_, i) { return i + 65; })),
            // 0x70 = 112
            FPP_MAC:       new Set([0x70]),
            // 0x60 = 96
            FPP_ARMBIAN:   new Set([0x60]),
            // 0x00 = 0
            UNKNOWN:       new Set([0x00]),
            // 0x80–0x87  (128–135)
            FALCON:        new Set(Array.from({ length: 8 },   function (_, i) { return i + 0x80; })),
            // 0x88–0x8F  (136–143)
            FALCON_V4:     new Set(Array.from({ length: 8 },   function (_, i) { return i + 0x88; })),
            // 0xC2, 0xC3  (194, 195)
            ESPIXELSTICK:  new Set([0xC2, 0xC3]),
            // 0xFF = 255
            SANDEVICES:    new Set([0xFF]),
            // 0xA0–0xAF  (160–175)
            GENIUS:        new Set(Array.from({ length: 16 },  function (_, i) { return i + 0xA0; })),
            // 0xFB = 251
            WLED:          new Set([0xFB]),
            // 0xC4 = 196
            BALDRICK:      new Set([0xC4]),
        };

        function isDeviceType(typeId, category) {
            return DEVICE_TYPE_SETS[category]?.has(parseInt(typeId)) ?? false;
        }

        function isFPP(typeId)            { return isDeviceType(typeId, 'FPP'); }
        function isFPPPi(typeId)          { return isDeviceType(typeId, 'FPP_PI'); }
        function isFPPBeagleBone(typeId)  { return isDeviceType(typeId, 'FPP_BEAGLEBONE'); }
        function isFPPMac(typeId)         { return isDeviceType(typeId, 'FPP_MAC'); }
        function isFPPArmbian(typeId)     { return isDeviceType(typeId, 'FPP_ARMBIAN'); }
        function isUnknownController(typeId) { return isDeviceType(typeId, 'UNKNOWN'); }
        function isFalcon(typeId)         { return isDeviceType(typeId, 'FALCON'); }
        function isFalconV4(typeId)       { return isDeviceType(typeId, 'FALCON_V4'); }
        function isESPixelStick(typeId)   { return isDeviceType(typeId, 'ESPIXELSTICK'); }
        function isSanDevices(typeId)     { return isDeviceType(typeId, 'SANDEVICES'); }
        function isGenius(typeId)         { return isDeviceType(typeId, 'GENIUS'); }
        function isWLED(typeId)           { return isDeviceType(typeId, 'WLED'); }
        function isBaldrick(typeId)       { return isDeviceType(typeId, 'BALDRICK'); }

        // ============================================================
        // SECTION: Utilities
        // ============================================================

        /*
         * Does a deep object flattening of "obj" using the specified base path
         * setting new properties on rc to rc[path + "." + key] == value
         */
        function flattenObject(obj, path, rc) {
            if (((typeof obj != "object" && typeof obj != 'function')) || (obj == null)) {
                console.log("WARNING: Not an object", obj);
                return;
            }

            for (const [key, value] of Object.entries(obj)) {
                let newPath = (path === "" ? key : path + "." + key)
                if (typeof value === "object") {
                    flattenObject(value, newPath, rc);
                } else if (typeof value === "boolean" || typeof value === "string" || typeof value == "number") {
                    rc[newPath] = value;
                } else {
                    console.log("Unable to handle path ", newPath, "of type", typeof value);
                }
            }

        }

        function s2ab(s) {
            var buf = new ArrayBuffer(s.length); //convert s to arrayBuffer
            var view = new Uint8Array(buf);  //create uint8array as viewer
            for (var i = 0; i < s.length; i++) view[i] = s.charCodeAt(i) & 0xFF; //convert to octet
            return buf;
        }

        function rowSpanSet(rowID) {
            var rowSpan = 1;

            if ($('#' + rowID + '_logs').is(':visible'))
                rowSpan++;

            if ($('#' + rowID + '_warnings').is(':visible'))
                rowSpan++;

            $('#' + rowID + ' > td:nth-child(1)').attr('rowspan', rowSpan);
        }

        function proxyURLsInString(str, ip) {
            if (!isProxied(ip))
                return str;

            var re = /(href=['"])([^:]*)\//;
            if (re.test(str))
                str = str.replace(re, "$1proxy/" + ip + "/$2/");

            return str;
        }

        function isProxied(ip) {
            return proxies.includes(ip);
        }

        function wrapUrlWithProxy(ip, path) {
            if (fppConfig.hideExternalURLs) {
                return "";
            }
            if (isProxied(ip)) {
                return 'proxy/' + ip + path;
            }
            return 'http://' + ip + path;
        }

        function ipLink(ip) {
            if (fppConfig.hideExternalURLs) {
                return ip;
            }
            return "<a target='host_" + ip + "' href='" + wrapUrlWithProxy(ip, "/") + "' data-ip='" + ip + "'>" + ip + "</a>";
        }

        /**
         * safeInitBody — wrapper around Bootstrap Table's initBody() that preserves
         * ephemeral UI state across every poll-driven re-render.
         *
         * WHY THIS EXISTS
         * ---------------
         * Status polling calls bt.initBody() to push updated data (mode, status,
         * utilization…) from the item objects into the DOM.  initBody() rebuilds
         * the entire <tbody>, which silently destroys three pieces of state:
         *
         *   1. FOCUS  — the focused filter-control <select>/<input> is re-created as
         *               a brand-new DOM node.  The browser drops focus to <body>,
         *               so a user mid-selection of a filter dropdown loses it.
         *
         *   2. ORDER  — jQuery UI drag-and-drop only reorders DOM rows; bt.data keeps
         *               the original insertion order.  initBody() re-renders from
         *               bt.data, discarding any drag the user just performed.
         *
         *   3. CHECKS — row checkboxes (.multisyncRowCheckbox / .syncCheckbox) are
         *               generated as static HTML strings without a "checked" attr.
         *               initBody() rebuilds them all unchecked.
         *
         * HOW IT WORKS
         * ------------
         * Before calling origInitBody():
         *   a. Snapshot every checked checkbox (keyed by name=IP).
         *   b. If reorder mode is active, sync bt.options.data to the current DOM
         *      row order so the re-render preserves what the user dragged.
         *   c. Record the focused element's id and value (only if it is inside the
         *      filter-control row; other focus contexts are intentionally left alone).
         *
         * After calling origInitBody():
         *   d. Restore checked state on the freshly rendered checkboxes.
         *   e. Re-focus the saved element and restore its value.
         *
         * WHEN TO USE
         * -----------
         * Call safeInitBody() instead of the raw bt.initBody() pattern:
         *
         *   var bt = $tbl.data('bootstrap.table');
         *   if (bt) bt.initBody();         // ← old: unsafe for poll-cycle callers
         *
         *   safeInitBody($tbl);            // ← new: safe
         *
         * The monkey-patched initBody() inside parseFPPSystems() (which preserves
         * child rows across full system-list re-renders) still calls origInitBody()
         * directly — that is intentional and correct.  safeInitBody() is only for
         * the poll-cycle callers outside parseFPPSystems().
         */
        function safeInitBody($tbl) {
            var bt = $tbl.data('bootstrap.table');
            if (!bt) return;

            // (a) Snapshot checked action-checkboxes by IP/name so we can restore
            //     them after initBody() rebuilds the rows from static HTML.
            var checkedNames = {};
            $tbl.find('input.multisyncRowCheckbox, input.syncCheckbox').each(function () {
                if (this.checked) checkedNames[this.name] = true;
            });

            // (b) In reorder mode, use the order captured synchronously in the jQuery
            //     UI sortable 'stop' callback (_reorderCommittedOrder).  This is more
            //     reliable than reading the live DOM here, because by the time this
            //     setTimeout-based poll tick fires the browser's DOM can appear to have
            //     reverted to the pre-drag order (investigation found no MutationObserver
            //     events, suggesting it is an internal browser/jQuery UI state issue).
            //
            //     _reorderCommittedOrder is null if no drag has occurred since entering
            //     reorder mode, in which case we skip the reorder step.
            //
            //     We also clear the active sort options so initBody()/initSort() does
            //     not re-sort the data and undo the order we're about to restore.
            var savedDomOrder = null;
            if (reorderModeActive && _reorderCommittedOrder && _reorderCommittedOrder.length) {
                savedDomOrder = _reorderCommittedOrder;
                bt.options.sortName = undefined;
                bt.options.sortOrder = undefined;
            }

            // (c) Record the focused element's id and value if it lives inside a
            //     filter-control cell.  Other focus contexts (buttons, inputs in
            //     modals, etc.) are intentionally ignored.
            var focusedId = null;
            var focusedValue = null;
            var active = document.activeElement;
            if (active && active.id && $(active).closest('.filter-control').length) {
                focusedId = active.id;
                focusedValue = active.value;
            }

            bt.initBody();

            // (d) Re-apply the user's drag order if we are in reorder mode.
            //     initBody() may have re-sorted rows; we move them back to the
            //     position the user dragged them to.  Each system row is moved with
            //     its immediately-following child rows (warnings / logs) so that they
            //     stay attached to their parent after the reorder.
            if (savedDomOrder && savedDomOrder.length) {
                var $tbody = $tbl.find('>tbody');
                savedDomOrder.forEach(function (rowId) {
                    var $row = $tbody.find('#' + rowId);
                    if (!$row.length) return;
                    var toMove = [$row[0]];
                    var $next = $row.next();
                    while ($next.length && $next.hasClass('child-row') &&
                           $next.attr('id') && $next.attr('id').startsWith(rowId + '_')) {
                        toMove.push($next[0]);
                        $next = $next.next();
                    }
                    toMove.forEach(function (el) { $tbody.append(el); });
                });
            }

            // (f) Restore checked state on freshly rebuilt checkbox nodes.
            if (Object.keys(checkedNames).length) {
                $tbl.find('input.multisyncRowCheckbox, input.syncCheckbox').each(function () {
                    if (checkedNames[this.name]) this.checked = true;
                });
            }

            // (g) Re-focus the filter element and restore its value.
            if (focusedId) {
                var el = document.getElementById(focusedId);
                if (el) {
                    if (focusedValue !== null) el.value = focusedValue;
                    el.focus();
                }
            }
        }

        // ============================================================
        // SECTION: Uptime formatting
        // ============================================================

        /**
         * Format uptime seconds into a short 2-field string and a "since" timestamp.
         *   uptimeShort("12d 3h")  or  "3h 42m"  — no zero-padding
         * @param {number} seconds  — uptime in seconds
         * @returns {{ short: string, since: string }}
         */
        function formatUptime(seconds) {
            seconds = Math.round(+seconds || 0);
            var days  = Math.floor(seconds / 86400);
            var hours = Math.floor((seconds % 86400) / 3600);
            var mins  = Math.floor((seconds % 3600) / 60);

            var short = days > 0
                ? days + 'd ' + hours + 'h'
                : hours + 'h ' + mins + 'm';

            var since = new Date(Date.now() - seconds * 1000);
            var pad = function(n) { return String(n).padStart(2, '0'); };
            var sinceStr = since.getFullYear() + '-' + pad(since.getMonth() + 1) + '-' + pad(since.getDate()) +
                ' ' + pad(since.getHours()) + ':' + pad(since.getMinutes()) + ':' + pad(since.getSeconds());

            return { short: short, since: sinceStr };
        }

        // ============================================================
        // SECTION: Network helpers
        // ============================================================

        function IPsCanTalk(ip1, ip2, octets) {
            var p1 = ip1.split('.');
            var p2 = ip2.split('.');

            switch (octets) {
                case 3: if ((p1[0] == p2[0]) && (p1[1] == p2[1]) && (p1[2] == p2[2]))
                    return true;
                    break;
                case 2: if ((p1[0] == p2[0]) && (p1[1] == p2[1]))
                    return true;
                    break;
                case 1: if (p1[0] == p2[0])
                    return true;
                    break;
            }

            return false;
        }

        function getReachableIPFromRowID(id) {
            var ip = ipFromRowID(id);
            var ipListStr = $('#' + id).attr('data-iplist');

            if (ip == ipListStr)
                return ip;

            var ipList = ipListStr.split(',');

            for (var o = 3; o > 0; o--) {
                for (var i = 0; i < systemsList.length; i++) {
                    if (systemsList[i].local == 1) {
                        for (var j = 0; j < ipList.length; j++) {
                            if (IPsCanTalk(systemsList[i].address, ipList[j], o))
                                return ipList[j];
                        }
                    }
                }
            }

            return '';
        }

        function ipFromRowID(id) {
            ip = $('#' + id).attr('data-ip');

            return ip;
        }
        function ipOrHostnameFromRowID(id) {
            var ip;
            if (fppConfig.serverName !== fppConfig.serverAddr) {
                // Hitting the FPP instance via hostname, not IP; use hostnames
                // for remotes as well or CORS will trigger.
                ip = $('#' + id + "_hostname").html();
                if (ip == "") {
                    ip = $('#' + id).attr('data-ip');
                }
            } else {
                ip = $('#' + id).attr('data-ip');
            }
            return ip;
        }

        // ============================================================
        // SECTION: Channel I/O
        // ============================================================

        function getUniverseTypeName(typeId) {
            return universeTypeNames[typeId] || ('Type ' + typeId);
        }

        /**
         * Fetches channel output config from a remote FPP system.
         * Returns a promise resolving to the parsed config data.
         */
        async function fetchChannelOutputConfig(ip) {
            if (channelOutputConfigCache[ip]) {
                return channelOutputConfigCache[ip];
            }
            try {
                const r = await fetch('api/channel/output/universeOutputs?ip=' + encodeURIComponent(ip));
                const data = await r.json();
                channelOutputConfigCache[ip] = data;
                return data;
            } catch {
                channelOutputConfigCache[ip] = null;
                return null;
            }
        }

        /**
         * Fetches channel input config from a remote FPP system.
         */
        async function fetchChannelInputConfig(ip) {
            if (channelInputConfigCache[ip]) {
                return channelInputConfigCache[ip];
            }
            try {
                const r = await fetch('api/channel/output/universeInputs?ip=' + encodeURIComponent(ip));
                const data = await r.json();
                channelInputConfigCache[ip] = data;
                return data;
            } catch {
                channelInputConfigCache[ip] = null;
                return null;
            }
        }

        /**
         * Builds tooltip HTML table from channel output config data.
         */
        function buildOutputTooltipHtml(data) {
            if (!data || !data.channelOutputs || !data.channelOutputs.length) {
                return '<em>No Output configuration found</em>';
            }
            var co = data.channelOutputs[0];
            if (!co.universes || !co.universes.length) {
                return '<em>No universes configured</em>';
            }
            var activeUniverses = co.universes.filter(function (u) { return u.active; });
            if (!activeUniverses.length) {
                return '<em>No active outputs</em>';
            }
            var html = '<table class="channel-io-tooltip-table">';
            html += '<tr><th>Description</th><th>Type</th><th>IP/Address</th><th>Channels</th></tr>';
            activeUniverses.forEach(function (u) {
                var desc = u.description || '-';
                var typeName = getUniverseTypeName(u.type);
                var addr = u.address || '-';
                var chRange = u.startChannel + '-' + (u.startChannel + (u.channelCount * (u.universeCount || 1)) - 1);
                html += '<tr>';
                html += '<td>' + desc + '</td>';
                html += '<td>' + typeName + '</td>';
                html += '<td>' + addr + '</td>';
                html += '<td>' + chRange + '</td>';
                html += '</tr>';
            });
            html += '</table>';
            return html;
        }

        /**
         * Builds tooltip HTML table from channel input config data.
         */
        function buildInputTooltipHtml(data) {
            if (!data || !data.channelInputs || !data.channelInputs.length) {
                return '<em>No Input configuration found</em>';
            }
            var ci = data.channelInputs[0];
            if (!ci.universes || !ci.universes.length) {
                return '<em>No input universes configured</em>';
            }
            var activeInputs = ci.universes.filter(function (u) { return u.active; });
            if (!activeInputs.length) {
                return '<em>No active inputs</em>';
            }
            var html = '<table class="channel-io-tooltip-table">';
            html += '<tr><th>Type</th><th>Start Ch</th><th>Channels</th><th>Universes</th></tr>';
            activeInputs.forEach(function (u) {
                var typeName = getUniverseTypeName(u.type);
                var chCount = u.channelCount * (u.universeCount || 1);
                html += '<tr>';
                html += '<td>' + typeName + '</td>';
                html += '<td>' + u.startChannel + '</td>';
                html += '<td>' + chCount + '</td>';
                html += '<td>' + (u.universeCount || 1) + '</td>';
                html += '</tr>';
            });
            html += '</table>';
            return html;
        }

        /**
         * Shows a Bootstrap popover with channel output/input details on an icon.
         */
        async function showChannelIOPopover(iconEl, isOutput) {
            var $icon = $(iconEl);
            var ip = $icon.data('ip');
            if (!ip) return;

            // If popover already exists, don't recreate
            if (bootstrap.Popover.getInstance(iconEl)) return;

            // Show loading popover
            var popover = new bootstrap.Popover(iconEl, {
                html: true,
                trigger: 'manual',
                placement: 'bottom',
                customClass: 'channel-io-popover',
                title: isOutput ? 'Channel Outputs' : 'Channel Inputs',
                content: '<em>Loading...</em>'
            });
            popover.show();

            // Fetch and update content
            var fetchFn = isOutput ? fetchChannelOutputConfig : fetchChannelInputConfig;
            var buildFn = isOutput ? buildOutputTooltipHtml : buildInputTooltipHtml;
            const data = await fetchFn(ip);
            var html = buildFn(data);
            // Update the popover content
            var tip = popover.tip;
            if (tip) {
                $(tip).find('.popover-body').html(html);
                popover.update();
            }
        }

        // Track which IPs we've already probed for channel I/O on older systems
        var channelIOCheckedIPs = {};

        /**
         * For older FPP systems that don't report channelOutputsEnabled/channelInputsEnabled,
         * fetch the remote config and add icons if active universes are found.
         */
        async function checkRemoteChannelIO(ip, rowID, data) {
            if (!ip) return;
            // If the data already has the properties, getChannelIOIcons handled it
            var hasOutputProp = data && data.hasOwnProperty('channelOutputsEnabled');
            var hasInputProp = data && data.hasOwnProperty('channelInputsEnabled');
            if (hasOutputProp && hasInputProp) return;
            // Only check each IP once
            if (channelIOCheckedIPs[ip]) {
                // Re-apply cached icons if mode cell was refreshed
                applyChannelIOIcons(ip, rowID);
                return;
            }
            channelIOCheckedIPs[ip] = { output: false, input: false };

            var checks = [];
            if (!hasOutputProp) {
                checks.push(fetchChannelOutputConfig(ip).then(function (configData) {
                    if (configData && configData.channelOutputs && configData.channelOutputs.length) {
                        var co = configData.channelOutputs[0];
                        if (co.enabled && co.universes && co.universes.length) {
                            if (co.universes.some(function (u) { return u.active; })) {
                                channelIOCheckedIPs[ip].output = true;
                            }
                        }
                    }
                }));
            }
            if (!hasInputProp) {
                checks.push(fetchChannelInputConfig(ip).then(function (configData) {
                    if (configData && configData.channelInputs && configData.channelInputs.length) {
                        var ci = configData.channelInputs[0];
                        if (ci.enabled && ci.universes && ci.universes.length) {
                            if (ci.universes.some(function (u) { return u.active; })) {
                                channelIOCheckedIPs[ip].input = true;
                            }
                        }
                    }
                }));
            }
            await Promise.allSettled(checks);
            applyChannelIOIcons(ip, rowID);
        }

        /**
         * Apply cached channel I/O icons to a mode cell if not already present.
         */
        function applyChannelIOIcons(ip, rowID) {
            var info = channelIOCheckedIPs[ip];
            if (!info || (!info.output && !info.input)) return;
            var item = $('#fppSystemsTable').bootstrapTable('getRowByUniqueId', rowID);
            if (!item) return;
            if (item.mode && item.mode.indexOf('channel-io-icons') >= 0) return;
            var icons = '<span class="channel-io-icons">';
            if (info.input) {
                icons += '<i class="fas fa-regular fa-circle-down channel-io-icon-input" data-ip="' + ip + '" aria-label="Channel Inputs Enabled (hover for details)"></i>';
            }
            if (info.output) {
                icons += '<i class="fas fa-regular fa-circle-up channel-io-icon-output" data-ip="' + ip + '" aria-label="Channel Outputs Enabled (hover for details)"></i>';
            }
            icons += '</span>';
            item.mode = (item.mode || '') + icons;
        }

        // ============================================================
        // SECTION: Display order
        // ============================================================

        /**
         * Returns a stable identifier for a system.
         * Uses UUID for FPP systems (most stable across IP/hostname changes),
         * falls back to hostname for non-FPP or systems without UUID.
         */
        function getSystemIdentifier(system) {
            if (system.uuid && system.uuid !== '' && system.uuid !== 'Unknown') {
                return 'uuid:' + system.uuid;
            }
            return 'host:' + (system.hostname || system.address);
        }

        /**
         * Sorts a systems data array based on the saved display order.
         * Systems matching saved identifiers appear first in saved order,
         * unknown systems are appended at the end sorted by hostname.
         */
        function applySavedDisplayOrder(data, order) {
            if (!order || order.length === 0) return data;

            var orderMap = {};
            for (var i = 0; i < order.length; i++) {
                orderMap[order[i]] = i;
            }

            var sorted = data.slice(); // clone to avoid mutating original
            sorted.sort(function (a, b) {
                var idA = getSystemIdentifier(a);
                var idB = getSystemIdentifier(b);
                var posA = orderMap.hasOwnProperty(idA) ? orderMap[idA] : 99999;
                var posB = orderMap.hasOwnProperty(idB) ? orderMap[idB] : 99999;
                if (posA !== posB) return posA - posB;
                // For systems not in saved order, sort by hostname
                return (a.hostname || a.address).localeCompare(b.hostname || b.address);
            });
            return sorted;
        }

        /**
         * Saves the current visible table row order as the display order.
         * Captures system identifiers from the current DOM order (which reflects
         * any column sort the user has applied).
         */
        function saveDisplayOrder() {
            var order = [];
            var seen = {};
            $('#fppSystems tr.systemRow').each(function () {
                var ipList = $(this).attr('data-iplist');
                if (!ipList) return;
                var primaryIp = ipList.split(',')[0];
                for (var i = 0; i < systemsList.length; i++) {
                    if (systemsList[i].address === primaryIp) {
                        var id = getSystemIdentifier(systemsList[i]);
                        if (!seen[id]) {
                            order.push(id);
                            seen[id] = true;
                        }
                        break;
                    }
                }
            });

            if (order.length === 0) {
                $.jGrowl('No systems to save order for.', { themeState: 'detract' });
                return;
            }

            savedDisplayOrder = order;
            // Store as pipe-delimited string to avoid JSON-in-INI quoting issues
            SetSetting('MultiSyncDisplayOrder', order.join('|'), 0, 0);

            // Clear sort so our custom order takes effect on next load
            $('#fppSystemsTable').bootstrapTable('sortReset');

            updateDisplayOrderButtons();

            // Exit reorder mode if active
            if (reorderModeActive) {
                toggleReorderMode();
            }

            $.jGrowl('Display order saved.', { themeState: 'success' });
        }

        /**
         * Clears the saved display order and refreshes the system list.
         */
        function clearDisplayOrder() {
            savedDisplayOrder = [];
            SetSetting('MultiSyncDisplayOrder', '', 0, 0);
            updateDisplayOrderButtons();

            // Exit reorder mode if active
            if (reorderModeActive) {
                toggleReorderMode();
            }

            $.jGrowl('Display order cleared.', { themeState: 'success' });
            getFPPSystems();
        }

        /**
         * Re-applies the saved display order by re-parsing the systems list.
         * Useful when user has sorted by a column and wants to return to saved order.
         */
        function sortSystemsByColor() {
            var bt = $('#fppSystemsTable').data('bootstrap.table');
            if (!bt) return;
            bt.options.data.sort(function (a, b) {
                var ac = (a.fppcolor !== '' && a.fppcolor !== null && a.fppcolor !== undefined) ? parseInt(a.fppcolor, 10) : -1;
                var bc = (b.fppcolor !== '' && b.fppcolor !== null && b.fppcolor !== undefined) ? parseInt(b.fppcolor, 10) : -1;
                if (ac < 0 && bc < 0) return 0;
                if (ac < 0) return 1;
                if (bc < 0) return -1;
                return ac - bc;
            });
            bt.data = bt.options.data.slice();
            bt.initBody();
        }

        function sortBySavedOrder() {
            if (!savedDisplayOrder || savedDisplayOrder.length === 0) {
                $.jGrowl('No saved display order.', { themeState: 'detract' });
                return;
            }
            parseFPPSystems(systemsList);
            // Clear sort to preserve our DOM order
            $('#fppSystemsTable').bootstrapTable('sortReset');
        }

        /**
         * Updates visibility and text of display order buttons based on current state.
         */
        function updateDisplayOrderButtons() {
            if (savedDisplayOrder && savedDisplayOrder.length > 0) {
                $('#clearDisplayOrderBtn').show();
                $('#sortBySavedOrderBtn').show();
                $('#saveDisplayOrderBtn').html('<i class="fas fa-save"></i> Update Display Order');
            } else {
                $('#clearDisplayOrderBtn').hide();
                $('#sortBySavedOrderBtn').hide();
                $('#saveDisplayOrderBtn').html('<i class="fas fa-save"></i> Save Display Order');
            }
        }

        /**
         * Toggles reorder mode on/off.
         * When enabled: adds drag handles, enables jQuery UI sortable on tbody,
         * disables column sorting.
         * When disabled: removes sortable, re-enables sorting.
         */
        function toggleReorderMode() {
            reorderModeActive = !reorderModeActive;
            var $table = $('#fppSystemsTable');
            var $tbody = $('#fppSystems');

            if (reorderModeActive) {
                _reorderCommittedOrder = null; // cleared on entry; first drag will populate it
                $table.addClass('reorder-mode');
                $('#reorderModeBtn').html('<i class="fas fa-arrows-alt"></i> Exit Reorder Mode').removeClass('btn-secondary').addClass('btn-warning');
                $('#saveDisplayOrderBtn').show();

                // Disable header click sorting
                $table.find('th').css('pointer-events', 'none');
                // Disable filter inputs
                $table.find('.filter-control input, .filter-control select').prop('disabled', true);

                // Enable jQuery UI sortable on tbody
                $tbody.sortable({
                    items: '> tr.systemRow',
                    handle: '.reorder-grip',
                    axis: 'y',
                    tolerance: 'pointer',
                    helper: function (e, tr) {
                        // Preserve column widths while dragging
                        var $originals = tr.children();
                        var $helper = tr.clone();
                        $helper.children().each(function (index) {
                            $(this).width($originals.eq(index).outerWidth());
                        });
                        return $helper;
                    },
                    start: function (event, ui) {
                        // Hide child rows (warnings, logs) during drag
                        var rowID = ui.item.attr('id');
                        $('#' + rowID + '_warnings, #' + rowID + '_logs').hide();
                    },
                    stop: function (event, ui) {
                        var rowID = ui.item.attr('id');
                        rowSpanSet(rowID);
                        // Capture the post-drag row order synchronously here, while
                        // jQuery UI has just committed the new DOM position.
                        // safeInitBody() uses this snapshot rather than reading the
                        // live DOM from a setTimeout tick, which may see stale order.
                        _reorderCommittedOrder = [];
                        $('#fppSystems > tr.systemRow').each(function () {
                            _reorderCommittedOrder.push(this.id);
                        });
                    }
                });
            } else {
                $table.removeClass('reorder-mode');
                $('#reorderModeBtn').html('<i class="fas fa-arrows-alt"></i> Reorder Systems').removeClass('btn-warning').addClass('btn-secondary');

                // Destroy sortable
                if ($tbody.sortable('instance')) {
                    $tbody.sortable('destroy');
                }

                // Re-enable header click sorting
                $table.find('th').css('pointer-events', '');
                // Re-enable filter inputs
                $table.find('.filter-control input, .filter-control select').prop('disabled', false);
            }
        }

        // ============================================================
        // SECTION: HTML renderers
        // ============================================================

        /*
         * Returns Mode + value of "Send Multisync"
         */
        function getFullMode(data, ip) {
            rc = "Unknown";
            if (data.hasOwnProperty('mode')) {
                rc = modeToString(data.mode);
            }
            if (data.hasOwnProperty('multisync') && data.multisync && data.mode == 2) {
                rc += " w/ Multisync"
            }

            rc += getChannelIOIcons(data, ip);

            return rc;
        }

        function getChannelIOIcons(data, ip) {
            var icons = '';
            var hasInput = data.hasOwnProperty('channelInputsEnabled') && data.channelInputsEnabled;
            var hasOutput = data.hasOwnProperty('channelOutputsEnabled') && data.channelOutputsEnabled;
            if (hasInput || hasOutput) {
                var ipAttr = ip ? " data-ip='" + ip + "'" : '';
                icons += '<span class="channel-io-icons">';
                if (hasInput) {
                    icons += '<i class="fas fa-regular fa-circle-down channel-io-icon-input"' + ipAttr + ' aria-label="Channel Inputs Enabled (hover for details)"></i>';
                }
                if (hasOutput) {
                    icons += '<i class="fas fa-regular fa-circle-up channel-io-icon-output"' + ipAttr + ' aria-label="Channel Outputs Enabled (hover for details)"></i>';
                }
                icons += '</span>';
            }
            return icons;
        }

        function getLocalVersionLink(ip, data) {
            var updatesAvailable = 0;
            if ((typeof (data.advancedView.RemoteGitVersion) !== 'undefined') &&
                (typeof (data.advancedView.LocalGitVersion) !== 'undefined') &&
                (data.advancedView.RemoteGitVersion !== "Unknown") &&
                (data.advancedView.RemoteGitVersion !== "") &&
                (data.advancedView.RemoteGitVersion !== data.advancedView.LocalGitVersion)) {
                updatesAvailable = 1;
            }
            var localVer = fppConfig.hideExternalURLs ? ""
                : "<a target='host_" + ip + "' href='" + wrapUrlWithProxy(ip, '/about.php') + "' target='_blank' data-ip='" + ip + "'>";
            var colorClass = updatesAvailable ? 'text-danger'
                : ((typeof (data.advancedView.RemoteGitVersion) !== 'undefined') &&
                    (data.advancedView.RemoteGitVersion == data.advancedView.LocalGitVersion))
                    ? 'text-success'
                    : 'text-muted';
            localVer += "<span class='fw-bold " + colorClass + "'>" + data.advancedView.LocalGitVersion + "</span>";
            if (!fppConfig.hideExternalURLs) {
                localVer += "</a>";
            }

            return localVer;
        }

        // ============================================================
        // SECTION: FPP system polling
        // ============================================================

        async function getFPPSystemStatus(ipAddresses, refreshing = false) {
            ips = "";
            if (Array.isArray(ipAddresses)) {
                fppPoller.cancel();
                ipAddresses.forEach(function (entry) {
                    ips += "&ip[]=" + entry;
                });
            } else {
                ips = "&ip[]=" + ipAddresses;
            }
            if (ips == "") {
                return;
            }
            try {
                const r = await fetch("api/system/status?type=FPP" + ips);
                const alldata = await r.json();
                systemStatusCache = alldata;
                Object.entries(alldata).forEach(function (entry) {
                        var ip = entry[0], data = entry[1];
                        var status = 'Idle';
                        var statusInfo = "";
                        var elapsed = "";
                        var files = "";
                        if (data == null || data == "" || data == "null") {
                            return;
                        }

                        if (data.status_name == 'playing') {
                            status = 'Playing';

                            elapsed = data.time_elapsed;

                            if (data.current_sequence != "") {
                                files += data.current_sequence;
                                if (data.current_song != "")
                                    files += "<br>" + data.current_song;
                            } else {
                                files += data.current_song;
                            }

                            if (files != "")
                                status += ":<br>" + files;
                        } else if (data.status_name == 'updating') {
                            status = 'Updating';
                        } else if (data.status_name == 'stopped') {
                            status = 'Stopped';
                        } else if (data.status_name == 'stopping gracefully') {
                            status = 'Stopping Gracefully';
                        } else if (data.status_name == 'stopping gracefully after loop') {
                            status = 'Stopping Gracefully After Loop';
                        } else if (data.status_name == 'paused') {
                            status = 'Paused';
                        } else if (data.status_name == 'testing') {
                            status = 'Testing';
                        } else if (data.status_name == 'unreachable') {
                            unavailables[ip]++;
                            status = "unreachable";
                        } else if (data.status_name == 'password') {
                            status = '<span class="text-danger">Protected</span>';
                        } else if (data.status_name == 'unknown') {
                            status = '-';
                        } else if (data.status_name == 'idle') {
                            if (data.mode_name == 'remote') {
                                if ((data.sequence_filename != "") ||
                                    (data.media_filename != "")) {
                                    status = 'Syncing';

                                    elapsed += data.time_elapsed;

                                    if (data.sequence_filename != "") {
                                        files += data.sequence_filename;
                                        if (data.media_filename != "")
                                            files += "<br>" + data.media_filename;
                                    } else {
                                        files += data.media_filename;
                                    }

                                    if (files != "")
                                        status += ":<br>" + files;
                                }
                            }
                        } else {
                            status = data.status_name;
                        }
                        if (data.status_name != 'unreachable') {
                            unavailables[ip] = 0;
                        }

                        if (data.rebootFlag == 1) {
                            status += "<br><i class=\"fas fa-exclamation-triangle text-danger\"></i><span class='warning-text'>Device Reboot Required</span>";
                        }

                        if (data.restartFlag == 1) {
                            status += "<br><i class=\"fas fa-exclamation-triangle text-warning\"></i><span class='warning-text'>FPPD Restart Required</span>";
                        }

                        var hostRowKey = ip.replace(/\./g, '_');
                        var rowID = hostRows[hostRowKey];

                        var item = $('#fppSystemsTable').bootstrapTable('getRowByUniqueId', rowID);
                        if (!item) return;

                        var curStatus = item.status || '';
                        if (curStatus !== '' && curStatus.indexOf('Last Seen') === -1 && !refreshing) {
                            // Don't replace an existing status via a different IP
                            return;
                        }
                        if (status == 'unreachable') {
                            if (unavailables[ip] < 4) {
                                return;
                            }
                            item.mode = "<span class=\"warning-text\">Unreachable</span>";
                        } else if (status != "") {
                            item.status = status;
                            item.mode = getFullMode(data, ip);
                            checkRemoteChannelIO(ip, rowID, data);
                        } else {
                            item.mode = getFullMode(data, ip);
                            checkRemoteChannelIO(ip, rowID, data);
                        }

                        // Wifi path 1: interfaces-based (newer systems).
                        // Show icon next to whichever IP in this row is on an active wifi
                        // interface (link > 0). Covers both cases:
                        //   - polled IP is the wifi IP (common case)
                        //   - polled IP is eth0 but device's wlan0 IP is also shown in this row
                        var wifiIconHtml = '';
                        var wifiIconIp = null;
                        if (data.hasOwnProperty('wifi') && data.hasOwnProperty('interfaces')) {
                            var ipIface = null;
                            var anyWifiIface = null;
                            var anyWifiIp = null;
                            var rowIpHtml = (item._baseIpHtml || '') + (item._extraIpHtml || '');
                            for (var i = 0; i < data.interfaces.length; i++) {
                                var iface = data.interfaces[i];
                                if (iface.addr_info) {
                                    for (var j = 0; j < iface.addr_info.length; j++) {
                                        var addr = iface.addr_info[j].local;
                                        if (addr === ip) {
                                            ipIface = iface;
                                        }
                                        // Track any wifi interface whose IP is displayed in this row
                                        if (!anyWifiIface && iface.hasOwnProperty('wifi') &&
                                                (iface.wifi.link || 0) > 0 &&
                                                rowIpHtml.indexOf(addr) !== -1) {
                                            anyWifiIface = iface;
                                            anyWifiIp = addr;
                                        }
                                    }
                                }
                            }
                            // Prefer the polled IP's own wifi; fall back to any displayed wifi IP
                            var wifiSource = null;
                            if (ipIface && ipIface.hasOwnProperty('wifi') &&
                                    (ipIface.wifi.link || 0) > 0) {
                                wifiSource = ipIface;
                                wifiIconIp = ip;
                            } else if (anyWifiIface) {
                                wifiSource = anyWifiIface;
                                wifiIconIp = anyWifiIp;
                            }
                            if (wifiSource) {
                                var w = wifiSource.wifi;
                                var wifi_html = [];
                                wifi_html.push('<span title="');
                                if (w.pct) {
                                    wifi_html.push(w.pct + '%');
                                    if (w.unit == 'dBm') {
                                        wifi_html.push(' ' + w.level + 'dBm');
                                    }
                                } else {
                                    wifi_html.push(w.level + w.unit);
                                }
                                wifi_html.push('" class="wifi-icon wifi-');
                                wifi_html.push(w.desc);
                                wifi_html.push('"></span>');
                                wifiIconHtml = wifi_html.join('');
                            }
                        }
                        // Insert wifi icon immediately after the correct IP's link.
                        var base = item._baseIpHtml || '';
                        var extra = item._extraIpHtml || '';
                        if (wifiIconHtml && wifiIconIp) {
                            var endTag = wifiIconIp + '</a>';
                            if (base.indexOf(endTag) !== -1) {
                                item.ipaddress = base.replace(endTag, endTag + wifiIconHtml) + extra;
                            } else if (extra.indexOf(endTag) !== -1) {
                                item.ipaddress = base + extra.replace(endTag, endTag + wifiIconHtml);
                            } else {
                                item.ipaddress = base.replace(wifiIconIp, wifiIconIp + wifiIconHtml) + extra;
                            }
                        } else {
                            item.ipaddress = base + extra;
                        }

                        if (item._dataIp !== ip) item._dataIp = ip;

                        item.elapsed = elapsed;

                        if (data.warnings != null && data.warnings.length > 0) {
                            $('#' + rowID + '_warnings').removeAttr('style');

                            // Ensure child rows match parent striping color
                            if ($('#' + rowID).hasClass('odd'))
                                $('#' + rowID + '_warnings').addClass('odd');

                            var wHTML = "";
                            for (var i = 0; i < data.warnings.length; i++) {
                                if (isProxied(ip))
                                    data.warnings[i] = proxyURLsInString(data.warnings[i], ip);

                                let wstr = data.warnings[i];
                                let idx = wstr.indexOf("href=");
                                if (idx > 0) {
                                    wstr = wstr.substr(0, idx + 6) + "http://" + ip + "/" + wstr.substr(idx + 6);
                                }
                                wHTML += "<span class='warning-text'>" + wstr + "</span><br>";
                            }
                            $('#' + rowID + '_warningCell').html(wHTML);
                        }
                        rowSpanSet(rowID);

                        //Expert View Rows
                        if (data.hasOwnProperty('advancedView') && data.status_name !== 'unknown' && data.status_name !== 'unreachable' && data.status_name !== 'password') {
                            // Rebuild platform cell from advancedView data
                            // Prefer SubPlatform (verbose hardware model string, e.g.
                            // "Raspberry Pi Zero 2 W Rev 1.0") over Variant (short label,
                            // e.g. "PiZero 2") so the platform column keeps the detail
                            // shown by the initial multiSyncSystems render. See issue #2614.
                            var platformTxt = item._platformInit || '';
                            if (data.advancedView.hasOwnProperty('Platform')) {
                                platformTxt = data.advancedView.Platform;
                            }
                            var variantTxt = item._variantInit || '';
                            if (data.advancedView.hasOwnProperty('SubPlatform') && (data.advancedView.SubPlatform != '')) {
                                variantTxt = data.advancedView.SubPlatform;
                            } else if (data.advancedView.hasOwnProperty('Variant') && (data.advancedView.Variant != '')) {
                                variantTxt = data.advancedView.Variant;
                            }
                            item.platform = "<span id='" + rowID + "_platform'>" + platformTxt + "</span>" +
                                "<br><small id='" + rowID + "_variant'>" + variantTxt + "</small>" +
                                "<span class='hidden typeId'> " + item._typeIdHex + " </span>" +
                                "<span class='hidden version'>" + item._versionStr + "</span>";

                            if (data.advancedView.hasOwnProperty("backgroundColor") && data.advancedView.backgroundColor != "") {
                                var colorInt = parseInt(data.advancedView.backgroundColor, 16);
                                item.fppcolor = isNaN(colorInt) ? '' : colorInt;
                                item._style = isNaN(colorInt) ? '' : 'background: #' + data.advancedView.backgroundColor + '; color: #FFF;';
                            } else {
                                item.fppcolor = '';
                                item._style = '';
                            }
                            if (data.advancedView.hasOwnProperty("RemoteGitVersion")) {
                                var u = "<table class='multiSyncVerboseTable'>";
                                u += "<tr><td><small class='text-muted'>COMMIT:</small></td><td id='" + rowID + "_localgitvers'>";
                                u += getLocalVersionLink(ip, data);
                                u += "</td></tr>" +
                                    "<tr><td><small class='text-muted'>BRANCH:</small></td><td id='" + rowID + "_gitbranch'>" + data.advancedView.Branch + "</td></tr>";

                                if ((typeof (data.advancedView.UpgradeSource) !== 'undefined') &&
                                    (data.advancedView.UpgradeSource != 'github.com')) {
                                    u += "<tr><td><small class='text-muted'>ORIGIN:</small></td><td id='" + rowID + "_origin'>" + data.advancedView.UpgradeSource + "</td></tr>";
                                } else {
                                    u += "<span style='display: none;' id='" + rowID + "_origin'></span>";
                                }
                                u += "</table>";
                                item.gitversions = u;
                            }

                            if (data.advancedView.OSVersion !== "") {
                                item.version = "<table class='multiSyncVerboseTable'>" +
                                    "<tr><td><small class='text-muted'>FPP:</small></td><td>" + item._versionStr + "</td></tr>" +
                                    "<tr><td><small class='text-muted'>OS:</small></td><td>" + data.advancedView.OSVersion + "</td></tr>" +
                                    "</table>";
                            }

                            if (data.advancedView.HostDescription !== "") {
                                if (item.hostname.indexOf("class='hostDescriptionSM'></small>") >= 0) {
                                    item.hostname = item.hostname.replace(
                                        "class='hostDescriptionSM'></small>",
                                        "class='hostDescriptionSM'>" + data.advancedView.HostDescription + "</small>"
                                    );
                                }
                            }

                            var u = "<table class='multiSyncVerboseTable'>";
                            if (typeof (data.advancedView.Utilization) !== 'undefined') {
                                let diskHtml = "";
                                try {
                                    let row = data.advancedView.Utilization.Disk;
                                    for (const [type, data] of Object.entries(row)) {
                                        let used = bytesToHuman(data["Total"] - data["Free"]);
                                        let total = bytesToHuman(data["Total"]);
                                        if (diskHtml == "") {
                                            diskHtml += "<b>Disk Usage:</b> "
                                        } else {
                                            diskHtml += ", "
                                        }
                                        diskHtml += type + ": " + used + "/" + total;
                                    }
                                } catch (error) {
                                    // This feature may not exist on older devices
                                }

                                if (diskHtml == "") {
                                    diskHtml = "<b>Disk Usage:</b> unknown";
                                }
                                if (data.advancedView.Utilization.hasOwnProperty("CPU")) {
                                    u += "<tr><td><small class='text-muted'>CPU:</small></td><td>" + Math.round(data.advancedView.Utilization.CPU) + "%</td></tr>";
                                }
                                if (data.advancedView.Utilization.hasOwnProperty("Memory")) {
                                    u += "<tr><td><small class='text-muted'>MEM:</small></td><td>" + Math.round(data.advancedView.Utilization.Memory) + "%</td></tr>";
                                }
                                if (data.advancedView.Utilization.hasOwnProperty("MemoryFree")) {
                                    var fr = data.advancedView.Utilization.MemoryFree;
                                    fr /= 1024;
                                    u += "<tr><td>Mem:&nbsp;</td><td>" + Math.round(fr) + "K Free</td></tr>";
                                }
                                if (data.advancedView.hasOwnProperty("rssi")) {
                                    // Wifi path 2: legacy rssi field (older systems without interfaces data)
                                    var isWifiIp = true;
                                    if (data.hasOwnProperty('interfaces') && data.interfaces.length > 0) {
                                        isWifiIp = false;
                                        outer: for (var i = 0; i < data.interfaces.length; i++) {
                                            var iface = data.interfaces[i];
                                            if (iface.addr_info) {
                                                for (var j = 0; j < iface.addr_info.length; j++) {
                                                    if (iface.addr_info[j].local === ip) {
                                                        isWifiIp = iface.hasOwnProperty('wifi');
                                                        break outer;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    if (isWifiIp && wifiIconHtml === '') {
                                        var rssi = +data.advancedView.rssi;
                                        var quality = 2 * (rssi + 100);

                                        if (rssi <= -100)
                                            quality = 0;
                                        else if (rssi >= -50)
                                            quality = 100;

                                        var wifi_html = [];

                                        wifi_html.push('<span title="');
                                        wifi_html.push(quality + '%');
                                        wifi_html.push(' ' + rssi + 'dBm');
                                        wifi_html.push('" class="wifi-icon wifi-');

                                        if (quality < 25) { var desc = "weak"; }
                                        else if (quality < 50) { var desc = "fair"; }
                                        else if (quality < 75) { var desc = "good"; }
                                        else { var desc = "excellent"; }

                                        wifi_html.push(desc);
                                        wifi_html.push('"></span>');

                                        wifiIconHtml = wifi_html.join('');
                                        var base2 = item._baseIpHtml || '';
                                        var extra2 = item._extraIpHtml || '';
                                        var endTag2 = ip + '</a>';
                                        if (base2.indexOf(endTag2) !== -1) {
                                            item.ipaddress = base2.replace(endTag2, endTag2 + wifiIconHtml) + extra2;
                                        } else {
                                            item.ipaddress = base2.replace(ip, ip + wifiIconHtml) + extra2;
                                        }
                                    }
                                }

                                if (data.advancedView.Utilization.hasOwnProperty("Uptime")) {
                                    var rawUt = data.advancedView.Utilization.Uptime;
                                    var uptimeSec = null;
                                    if (typeof rawUt !== "string" && !(rawUt instanceof String)) {
                                        uptimeSec = rawUt / 1000;
                                    } else {
                                        // "D days H:M" or "H:M"
                                        var dm = String(rawUt).match(/(\d+)\s+days?\s+(\d+):(\d+)/);
                                        var hm = String(rawUt).match(/^(\d+):(\d+)$/);
                                        if (dm) uptimeSec = +dm[1]*86400 + +dm[2]*3600 + +dm[3]*60;
                                        else if (hm) uptimeSec = +hm[1]*3600 + +hm[2]*60;
                                    }
                                    if (uptimeSec !== null) {
                                        var uf = formatUptime(uptimeSec);
                                        u += "<tr><td><small class='text-muted'>UP:</small></td><td>";
                                        u += '<span title="since ' + uf.since + '">' + uf.short + '</span>';
                                        u += ' <span data-bs-html="true" title="<span class=\'tooltipSpan\'>' + diskHtml + '<br><b>Since:</b> ' + uf.since + '</span>">...</span>';
                                        u += "</td></tr>";
                                    }
                                }
                            }
                            u += "</table>";
                            item.utilization = u;

                            // Add any IPs the device reports in advancedView but that
                            // multiSyncSystems didn't include (e.g. a secondary subnet NIC).
                            // Extra IPs go into _extraIpHtml so the wifi icon stays between
                            // the primary IPs and the secondary ones.
                            if (data.advancedView.hasOwnProperty('IPs') &&
                                    Array.isArray(data.advancedView.IPs)) {
                                var changed = false;
                                var extra = item._extraIpHtml || '';
                                data.advancedView.IPs.forEach(function(avIp) {
                                    // Skip link-local (169.254.x.x) — APIPA addresses have no DHCP lease
                                    if (avIp.indexOf('169.254.') === 0) return;
                                    if ((item._baseIpHtml || '').indexOf(avIp) === -1 &&
                                            extra.indexOf(avIp) === -1) {
                                        extra += '<br>' + ipLink(avIp);
                                        changed = true;
                                    }
                                });
                                if (changed) {
                                    item._extraIpHtml = extra;
                                    item.ipaddress = (item._baseIpHtml || '') + wifiIconHtml + extra;
                                }
                            }
                        }
                    });
            safeInitBody($('#fppSystemsTable'));
            SetupToolTips();
            } finally {
                if (Array.isArray(ipAddresses) && $('#MultiSyncRefreshStatus').is(":checked") && !isUserInteracting()) {
                    fppPoller.schedule(() => getFPPSystemStatus(ipAddresses, true));
                }
            }
            validateMultiSyncSettings();
        } // end of "api/system/status?ip=" + ips

        function parseFPPSystems(data) {
            // Apply saved display order if one exists
            if (savedDisplayOrder && savedDisplayOrder.length > 0) {
                data = applySavedDisplayOrder(data, savedDisplayOrder);
            }

            // Destroy Bootstrap Table before manipulating DOM so destroy
            // doesn't restore a stale snapshot over our new rows
            var $tbl = $('#fppSystemsTable');
            if ($tbl.closest('.bootstrap-table').length) {
                $tbl.bootstrapTable('destroy');
            }

            $('#fppSystems').empty();
            rowSpans = [];
            var systemsData = [];

            // UUID → rowID. Each unique UUID = one physical device = one row.
            var seenUuids = {};

            var fppIpAddresses = [];
            var wledIpAddresses = [];
            var geniusIpAddresses = [];
            var baldrickIpAddresses = [];
            var falconV4Addresses = [];
            var falconV3Addresses = [];

            var remotes = [];
            if ((settings['MultiSyncEnabled'] == '1') &&
                (settings['fppMode'] == 'player')) {
                if (typeof settings['MultiSyncRemotes'] === 'string') {
                    var tarr = settings['MultiSyncRemotes'].split(',');
                    for (var i = 0; i < tarr.length; i++) {
                        remotes[tarr[i]] = 1;

                        if ((tarr[i] == "255.255.255.255") &&
                            (!$('#MultiSyncBroadcast').is(":checked")))
                            $('#MultiSyncBroadcast').prop('checked', true).trigger('change');

                        if ((tarr[i] == "239.70.80.80") &&
                            (!$('#MultiSyncMulticast').is(":checked")))
                            $('#MultiSyncMulticast').prop('checked', true).trigger('change');
                    }
                }

                $('.masterOptions').show();
            }

            for (var i = 0; i < data.length; i++) {
                var star = "";
                var ip = data[i].address;

                // Hide addresses that aren't useful here: loopback (127.x / ::1)
                // and link-local — IPv4 APIPA (169.254/16) and IPv6 (fe80::/10).
                // These aren't routable from the browser (the IPv6 zone id is
                // specific to the FPP host), and loopback is just the local box
                // discovering itself, so skip the entry entirely.
                var ipl = ip.toLowerCase();
                if (ip.indexOf('169.254') == 0 || ipl.indexOf('fe80') == 0 ||
                    ip.indexOf('127.') == 0 || ipl == '::1')
                    continue;

                if ((settings.hasOwnProperty('MultiSyncHide10')) &&
                    (settings['MultiSyncHide10'] == '1') &&
                    (ip.indexOf('10.') == 0)) {
                    continue;
                }

                if ((settings.hasOwnProperty('MultiSyncHide172')) &&
                    (settings['MultiSyncHide172'] == '1') &&
                    (ip.indexOf('172.') == 0)) {
                    var parts = ip.split('.');
                    var second = parseInt(parts[1]);
                    if ((second >= 16) && (second <= 31)) {
                        continue;
                    }
                }

                if ((settings.hasOwnProperty('MultiSyncHide192')) &&
                    (settings['MultiSyncHide192'] == '1') &&
                    (ip.indexOf('192.168.') == 0)) {
                    continue;
                }

                var hostname = data[i].hostname || ip;

                // Stable row ID: UUID when present, then hostname+mode, then IP.
                // UUID guarantees one physical device = one row. For older devices
                // without UUID, hostname+mode deduplicates multi-IP entries without
                // the version-string mismatch that broke the original hostKey approach.
                var rawHostname = data[i].hostname || '';
                var uuid = (data[i].uuid && data[i].uuid !== '')
                    ? data[i].uuid
                    : (rawHostname !== '' ? 'host:' + rawHostname + ':' + (data[i].fppModeString || '') : 'ip:' + ip);
                var rowID = uuid.replace(/[^a-zA-Z0-9]/g, '_');
                var hostRowKey = ip.replace(/\./g, '_');

                hostRows[hostRowKey] = rowID;

                var hnSpanStyle = "";
                if (data[i].local) {
                    hnSpanStyle = " class='fw-bold'";
                } else {
                    if ((settings['MultiSyncEnabled'] == '1') &&
                        (settings['fppMode'] == 'player') &&
                        (data[i].fppModeString == "remote")) {
                        star = " <input type='checkbox' class='syncCheckbox' name='" + data[i].address + "'";
                        if (typeof remotes[data[i].address] !== 'undefined') {
                            star += " checked";
                            delete remotes[data[i].address];
                        }
                        star += " onClick='updateMultiSyncRemotes(true);'>";
                    }
                }

                if (seenUuids.hasOwnProperty(uuid)) {
                    // Same physical device, additional IP — merge into existing row.
                    // Do NOT add to poll list; the primary IP already covers this device.
                    var mergeExtra = '<br>' + ipLink(data[i].address);
                    if (data[i].fppModeString == 'remote') mergeExtra += star;
                    var mergeItem = seenUuids[uuid]._item;
                    mergeItem.ipaddress    += mergeExtra;
                    mergeItem._baseIpHtml  += mergeExtra;
                    mergeItem._dataIplist  += ',' + data[i].address;
                    continue;
                }

                // First time we see this UUID — create the row.
                var fppMode = 'Player';
                if (data[i].fppModeString == 'bridge') {
                    fppMode = 'Bridge';
                } else if (data[i].fppModeString == 'master') {
                    fppMode = 'Master';
                } else if (data[i].fppModeString == 'remote') {
                    fppMode = 'Remote';
                } else if (data[i].fppModeString == 'unknown') {
                    fppMode = 'Unknown';
                }

                fppMode += getChannelIOIcons(data[i], data[i].address);

                rowSpans[rowID] = 1;

                var ipTxt = data[i].local ? data[i].address : ipLink(data[i].address);

                if ((data[i].fppModeString == 'remote') && (star != ""))
                    ipTxt = "<small>Select IPs for Unicast Sync</small><br>" + ipTxt + star;

                var hostTxt = (fppConfig.hideExternalURLs || data[i].local || data[i].address == hostname)
                    ? hostname
                    : "<a target='host_" + data[i].address + "' href='" + wrapUrlWithProxy(data[i].address, "/") + "'>" + hostname + "</a>";

                var versionParts = data[i].version.split('.');
                var majorVersion = 0;
                if (data[i].version != 'Unknown')
                    majorVersion = parseInt(versionParts[0]);

                var versionStr = data[i].version;
                var versionHtml;
                if (isFPP(data[i].typeId)) {
                    versionStr = data[i].version.replace('.x-master', '.x').replace(/-g[A-Za-z0-9]*/, '');
                    if (versionStr.endsWith('-dirty')) {
                        versionStr = versionStr.replace('-dirty', '');
                        var dirtyLink = "<br><a ";
                        if (data[i].local) {
                            dirtyLink += "href='settings.php#settings-developer'";
                        } else {
                            dirtyLink += "target='host_" + data[i].address + "' href='" + wrapUrlWithProxy(data[i].address, '/settings.php#settings-developer') + "'";
                        }
                        dirtyLink += ">Modified</a>";
                        versionStr += dirtyLink;
                    }
                    versionHtml = "<table class='multiSyncVerboseTable'>" +
                        "<tr><td>FPP:</td><td>" + versionStr + "</td></tr>" +
                        "<tr><td>OS:</td><td></td></tr>" +
                        "</table>";
                } else {
                    versionHtml = data[i].version;
                }

                var selectboxHtml = '';
                if (isFPP(data[i].typeId) && majorVersion >= 4) {
                    selectboxHtml = "<input type='checkbox' class='remoteCheckbox largeCheckbox multisyncRowCheckbox' name='" + data[i].address + "'>";
                }

                var ipDash = ip.replace(/\./g, '_');
                var typeIdHex = '0x' + parseInt(data[i].typeId).toString(16);

                var newItem = {
                    _id:           rowID,
                    _dataIp:       data[i].address,
                    _dataIplist:   data[i].address,
                    _isFPP:        isFPP(data[i].typeId),
                    _typeIdHex:    typeIdHex,
                    _platformInit: data[i].type,
                    _variantInit:  data[i].model,
                    _versionStr:   versionStr,
                    _baseIpHtml:   ipTxt,
                    hostname:     "<span class='reorder-grip'><i class='rowGripIcon fpp-icon-grip'></i></span>" +
                                  "<span id='fpp_" + ipDash + "_hostname'" + hnSpanStyle + ">" + hostTxt + "</span>" +
                                  "<br><small class='hostDescriptionSM'></small>",
                    ipaddress:    ipTxt,
                    platform:     "<span id='" + rowID + "_platform'>" + data[i].type + "</span>" +
                                  "<br><small id='" + rowID + "_variant'>" + data[i].model + "</small>" +
                                  "<span class='hidden typeId'> " + typeIdHex + " </span>" +
                                  "<span class='hidden version'>" + data[i].version + "</span>",
                    mode:         fppMode,
                    status:       'Last Seen:<br>' + data[i].lastSeenStr,
                    elapsed:      '',
                    version:      versionHtml,
                    gitversions:  '',
                    utilization:  '',
                    fppcolor:     '',
                    selectbox:    selectboxHtml
                };
                systemsData.push(newItem);
                // Register so subsequent entries with the same UUID can find this item.
                seenUuids[uuid] = { rowID: rowID, _item: newItem };

                // For older FPP systems without channelOutputsEnabled, check remote config
                if (isFPP(data[i].typeId)) {
                    checkRemoteChannelIO(data[i].address, rowID, data[i]);
                }

                $('#fppSystems').append(
                    "<tr id='" + rowID + "_warnings' class='child-row warning-row'>" +
                    "<td colspan='10' id='" + rowID + "_warningCell'></td></tr>"
                );
                $('#fppSystems').append(
                    "<tr id='" + rowID + "_logs' style='display:none' class='logRow child-row'>" +
                    "<td colspan='10' id='" + rowID + "_logCell'>" +
                    "<table class='multiSyncVerboseTable' width='100%'>" +
                    "<tr><td>Log:</td><td width='100%'><textarea id='" + rowID + "_logText' style='width: 100%;' rows='8' disabled></textarea></td></tr>" +
                    "<tr><td></td><td><div class='right' id='" + rowID + "_doneButtons' style='display: none;'>" +
                    "<input type='button' class='buttons' value='Restart FPPD' onClick='restartSystem(\"" + rowID + "\");' style='float: left;'>" +
                    "<input type='button' class='buttons' value='Reboot' onClick='rebootRemoteFPP(\"" + rowID + "\", \"" + ip + "\");' style='float: left;'>" +
                    "<input type='button' class='buttons' value='Close Log' onClick='$(\"#" + rowID + "_logs\").hide(); rowSpanSet(\"" + rowID + "\");'>" +
                    "</div></td></tr></table></td></tr>"
                );

                if (isFPP(data[i].typeId)) {
                    fppIpAddresses.push(ip);
                } else if (isESPixelStick(data[i].typeId)) {
                    if ((majorVersion == 4) || (majorVersion == 0)) {
                        getESPixelStickBridgeStatus(ip);
                    } else {
                        fppIpAddresses.push(ip);
                    }
                } else if (isFalconV4(data[i].typeId)) {
                    falconV4Addresses.push(ip);
                } else if (isFalcon(data[i].typeId)) {
                    falconV3Addresses.push(ip);
                } else if (isWLED(data[i].typeId)) {
                    wledIpAddresses.push(ip);
                } else if (isGenius(data[i].typeId)) {
                    geniusIpAddresses.push(ip);
                } else if (isBaldrick(data[i].typeId)) {
                    baldrickIpAddresses.push(ip);
                }
            }
            getFPPSystemStatus(fppIpAddresses, false);
            getWLEDControllerStatus(wledIpAddresses, false);
            getGeniusControllerStatus(geniusIpAddresses, false);
            getBaldrickControllerStatus(baldrickIpAddresses, false);
            getFalconControllerStatus(falconV3Addresses, falconV4Addresses, false);

            var extraRemotes = [];
            var origExtra = "";
            if (typeof settings['MultiSyncExtraRemotes'] === 'string') {
                origExtra = settings['MultiSyncExtraRemotes'];
                extraRemotes = origExtra.split(',');
            }
            for (var x in remotes) {
                if (!extraRemotes.includes(x)) {
                    extraRemotes.push(x);
                }
            }
            extraRemotes.sort();
            var extras = extraRemotes.join(',');
            settings['MultiSyncExtraRemotes'] = extras;

            if (extras != '' && origExtra != extras) {
                if (fppConfig.uiLevel >= 1) {
                    var inp = document.getElementById("MultiSyncExtraRemotes");
                    if (inp) {
                        $('#MultiSyncExtraRemotes').val(extras);
                    }
                }
                SetSetting("MultiSyncExtraRemotes", extras, 0, 0);
            }

            // Initialize Bootstrap Table with empty data so its first
            // initBody is a no-op, then load through the monkey-patch.
            var $tbl = $('#fppSystemsTable');

            // Detach child rows before BT init — they aren't BT data rows.
            var $detachedChildren = $('#fppSystems > tr.child-row').detach();

            $tbl.bootstrapTable({
                idField: '_id',
                uniqueId: '_id',
                data: [],
                filterControl: true,
                filterControlVisible: true,
                sortName: (savedDisplayOrder && savedDisplayOrder.length > 0) ? undefined : 'hostname',
                sortOrder: 'asc',
                showColumns: false,
                striped: true,
                undefinedText: '',
                rowAttributes: function (row) {
                    var attrs = { id: row._id, class: 'systemRow' };
                    if (row._dataIp) attrs['data-ip'] = row._dataIp;
                    if (row._dataIplist) attrs['data-iplist'] = row._dataIplist;
                    return attrs;
                },
                rowStyle: function (row) {
                    if (row && row.fppcolor !== '' && row.fppcolor !== null && row.fppcolor !== undefined) {
                        var colorNum = parseInt(row.fppcolor, 10);
                        if (!isNaN(colorNum)) {
                            var hex = colorNum.toString(16);
                            while (hex.length < 6) {
                                hex = '0' + hex;
                            }
                            return {
                                css: {
                                    background: '#' + hex,
                                    color: '#FFF'
                                }
                            };
                        }
                    }
                    return {};
                }
            });

            // Monkey-patch initBody to preserve child rows across re-renders.
            var bt = $tbl.data('bootstrap.table');
            if (bt) {
                var origInitBody = bt.initBody;
                bt.initBody = function (fixedScroll, updatedUid) {
                    var $body = this.$el.find('>tbody');
                    // Capture log <textarea> scroll positions BEFORE detaching the
                    // child rows.  Detaching an element from the DOM destroys its
                    // layout, so a detached textarea reports zero metrics and loses
                    // its scrollTop.  Without this, every status-poll re-render (every
                    // ~2 s) during a mass update reset the streaming upgrade logs back
                    // to the top, so the windows stopped following the latest command.
                    var $liveChildRows = $body.length ? $body.find('>tr.child-row') : $();
                    var scrollState = {};
                    $liveChildRows.find('textarea').each(function () {
                        if (!this.id) return;
                        var atBottom = (this.scrollHeight - this.scrollTop - this.clientHeight) <= 2;
                        scrollState[this.id] = { top: this.scrollTop, atBottom: atBottom };
                    });
                    var $childRows = $liveChildRows.detach();
                    origInitBody.call(this, fixedScroll, updatedUid);
                    if ($childRows.length) {
                        reattachChildRows($tbl, $childRows);
                        // Restore scroll position once the rows are back in the layout.
                        // Textareas pinned to the bottom stay pinned so the latest
                        // streamed line remains in view.
                        $childRows.find('textarea').each(function () {
                            var s = scrollState[this.id];
                            if (!s) return;
                            this.scrollTop = s.atBottom ? this.scrollHeight : s.top;
                        });
                    }
                };
            }

            // Load data through the monkey-patch so rowStyle and rowAttributes apply.
            $tbl.bootstrapTable('load', systemsData);

            // Re-attach child rows after their parent system rows.
            reattachChildRows($tbl, $detachedChildren);

            if (typeof buildColumnSelector === 'function') buildColumnSelector();
        }

        /**
         * Insert child rows (warnings / logs) immediately after their
         * parent system row inside the table body.
         */
        function reattachChildRows($tbl, $children) {
            if (!$children || !$children.length) return;
            var $tbody = $tbl.find('>tbody');
            if (!$tbody.length) $tbody = $tbl.find('tbody');
            $children.each(function () {
                var id = this.id || '';
                // Derive parent row id: strip _warnings or _logs suffix
                var parentId = id.replace(/_warnings$|_logs$/, '');
                var $parent = $tbody.find('#' + parentId);
                if ($parent.length) {
                    // Insert after parent row, but after any sibling
                    // child rows that already follow it.
                    var $after = $parent;
                    $parent.nextAll('.child-row').each(function () {
                        if (this.id && this.id.startsWith(parentId + '_')) {
                            $after = $(this);
                        } else {
                            return false; // stop at first non-sibling
                        }
                    });
                    $after.after(this);
                }
            });
        }

        var systemsList = [];
        async function getFPPSystems() {
            if (streamCount) {
                alert("FPP Systems are being updated, you will need to manually refresh once these updates are complete.");
                return;
            }

            $('.masterOptions').hide();
            $('#fppSystems').html("<tr><td colspan=8 align='center'>Loading system list from fppd.</td></tr>");

            const r = await fetch('api/fppd/multiSyncSystems');
            const data = await r.json();
            systemsList = data.systems;
            parseFPPSystems(data.systems);
        }

        // ============================================================
        // SECTION: ESPixelStick polling
        // ============================================================

        var ESPSockets = {};
        function espUpdateDescription(item, desc) {
            if (item.hostname.indexOf("class='hostDescriptionSM'></small>") >= 0) {
                item.hostname = item.hostname.replace(
                    "class='hostDescriptionSM'></small>",
                    "class='hostDescriptionSM'>" + desc + "</small>"
                );
            }
        }

        function parseESPixelStickConfig(ip, data) {
            var s = JSON.parse(data);
            var $tbl = $('#fppSystemsTable');
            var rowId = hostRows[ip.replace(/\./g, '_')];
            var item = $tbl.bootstrapTable('getRowByUniqueId', rowId);
            if (!item) return;
            espUpdateDescription(item, s.device.id);
            safeInitBody($tbl);
        }

        function parseESPixelStickStatus(ip, data) {
            var s = JSON.parse(data);
            var ips = ip.replace(/\./g, '_');

            if (s.hasOwnProperty("status")) {
                s = s.status;
            }

            var rssi = +s.system.rssi;
            var quality = 2 * (rssi + 100);

            if (rssi <= -100)
                quality = 0;
            else if (rssi >= -50)
                quality = 100;

            var wifiDesc = quality < 25 ? 'weak' : quality < 50 ? 'fair' : quality < 75 ? 'good' : 'excellent';
            var wifiIcon = '<span title="' + quality + '% ' + rssi + 'dBm" class="wifi-icon wifi-' + wifiDesc + '"></span>';

            var uf = formatUptime(+s.system.uptime / 1000);

            var u = "<table class='multiSyncVerboseTable'>";
            u += '<tr><td>UP:</td><td><span title="since ' + uf.since + '">' + uf.short + '</span></td></tr>';
            u += "</table>";

            var $tbl = $('#fppSystemsTable');
            var rowId = hostRows[ips];
            var item = $tbl.bootstrapTable('getRowByUniqueId', rowId);
            if (!item) return;

            item.utilization = u;

            var endTag = ip + '</a>';
            var base = item._baseIpHtml || '';
            var extra = item._extraIpHtml || '';
            if (base.indexOf(endTag) !== -1) {
                item.ipaddress = base.replace(endTag, endTag + wifiIcon) + extra;
            } else {
                item.ipaddress = base + wifiIcon + extra;
            }

            if (item.mode === 'Bridge') {
                var st = 'Bridging';
                if (s.hasOwnProperty('e131')) {
                    st = "<table class='multiSyncVerboseTable'>";
                    st += "<tr><td>Tot Pkts:</td><td>" + s.e131.num_packets + "</td></tr>";
                    st += "<tr><td>Seq Errs:</td><td>" + s.e131.seq_errors + "</td></tr>";
                    st += "<tr><td>Pkt Errs:</td><td>" + s.e131.packet_errors + "</td></tr>";
                    st += "</table>";
                } else if (s.hasOwnProperty('input')) {
                    for (var i = 0; i < s.input.length; i++) {
                        if (s.input[i].hasOwnProperty('e131')) {
                            st = "<table class='multiSyncVerboseTable'>";
                            st += "<tr><td>Tot Pkts:</td><td>" + s.input[i].e131.num_packets + "</td></tr>";
                            st += "<tr><td>Pkt Errs:</td><td>" + s.input[i].e131.packet_errors + "</td></tr>";
                            st += "</table>";
                        }
                    }
                }
                item.status = st;
            }

            safeInitBody($tbl);

            if ($('#MultiSyncRefreshStatus').is(":checked")) {
                setTimeout(function () { ESPSockets[ips].send("XJ"); }, 1000);
            }
        }

        function parseESPixelStickVersion(ip, data) {
            var s = JSON.parse(data);
            if (!s.hasOwnProperty('version')) return;
            var $tbl = $('#fppSystemsTable');
            var rowId = hostRows[ip.replace(/\./g, '_')];
            var item = $tbl.bootstrapTable('getRowByUniqueId', rowId);
            if (!item) return;
            item.version = s.version;
            item._versionStr = s.version;
            safeInitBody($tbl);
        }

        function parseESPixelStickCommandResponse(ip, data) {
            var s = JSON.parse(data);
            if (!s.hasOwnProperty('get') || !s.get.hasOwnProperty('device') || !s.get.device.hasOwnProperty('id')) return;
            var $tbl = $('#fppSystemsTable');
            var rowId = hostRows[ip.replace(/\./g, '_')];
            var item = $tbl.bootstrapTable('getRowByUniqueId', rowId);
            if (!item) return;
            espUpdateDescription(item, s.get.device.id);
            safeInitBody($tbl);
        }

        function getESPixelStickBridgeStatus(ip) {
            var ips = ip.replace(/\./g, '_');

            if (ESPSockets.hasOwnProperty(ips)) {
                ESPSockets[ips].send("XJ");
            } else {
                var ws = new WebSocket("ws://" + ip + "/ws");
                ESPSockets[ips] = ws;

                ws.binaryType = "arraybuffer";
                ws.onopen = function () {
                    ws.send("G1");
                    ws.send("G2");
                    ws.send("XA"); // ESPixelStick v4.x
                    ws.send("XJ");
                    ws.send('{"cmd":{"get":"device"}}'); // ESPixelStick v4.x
                };

                ws.onmessage = function (e) {
                    if ("string" == typeof e.data) {
                        var t = e.data.substr(0, 2)
                            , n = e.data.substr(2);
                        switch (t) {
                            case "XA":
                            case "G2":
                                parseESPixelStickVersion(ip, n);
                                break;
                            case "G1":
                                parseESPixelStickConfig(ip, n);
                                break;
                            case "XJ":
                                parseESPixelStickStatus(ip, n);
                                break;
                            case '{"':
                                parseESPixelStickCommandResponse(ip, e.data);
                                break;
                        }
                    }
                };

                ws.onclose = function () {
                    delete ESPSockets[ips];
                };
            }
        }


        // ============================================================
        // SECTION: Falcon polling
        // ============================================================

        async function getFalconControllerStatus(fv3ips, fv4ips, refreshing = false) {
            falconPoller.cancel();

            ips3 = "";
            if (Array.isArray(fv3ips)) {
                fv3ips.forEach(function (entry) {
                    ips3 += "&ip[]=" + entry;
                });
            } else {
                ips3 = "&ip[]=" + fv3ips;
            }

            ips4 = "";
            if (Array.isArray(fv4ips)) {
                fv4ips.forEach(function (entry) {
                    ips4 += "&ip[]=" + entry;
                });
            } else {
                ips4 = "&ip[]=" + fv4ips;
            }

            const promises = [];
            if (ips3 != "") {
                promises.push((async () => {
                    const r = await fetch("api/system/status?type=FV3" + ips3);
                    const alldata = await r.json();
                    var $tbl = $('#fppSystemsTable');
                    Object.entries(alldata).forEach(function (entry) {
                        var ip = entry[0], data = entry[1];
                        var rowId = hostRows[ip.replace(/\./g, '_')];
                        var item = $tbl.bootstrapTable('getRowByUniqueId', rowId);
                        if (!item) return;

                        var u = "<table class='multiSyncVerboseTable'>";
                        u += "<tr><td>Uptime:</td><td>" + data['u'] + "</td></tr>";
                        u += "<tr><td>V1 Voltage:</td><td> " + data['v1'] + "</td></tr>";
                        u += "<tr><td>V2 Voltage:</td><td> " + data['v2'] + "</td></tr>";
                        u += "</table>";

                        item.utilization = u;
                        item.status = (item.mode === 'Bridge') ? 'Bridging' : '';
                    });
                    safeInitBody($tbl);
                })());
            }
            if (ips4 != "") {
                promises.push((async () => {
                    const r = await fetch("api/system/status?type=FV4" + ips4);
                    const alldata = await r.json();
                    var $tbl = $('#fppSystemsTable');
                    Object.entries(alldata).forEach(function (entry) {
                        var ip = entry[0], s = entry[1];

                        var tempthreshold = s.P.BS;
                        var t1temp = s.P.T1 / 10;
                        var t2temp = s.P.T2 / 10;
                        var t1tempLabel = t1temp + "C";

                        if (settings['temperatureInF'] == 1) {
                            t1temp = Math.round((t1temp * 9 / 5) + 32);
                            t2temp = Math.round((t2temp * 9 / 5) + 32);
                            tempthreshold = Math.round((tempthreshold * 9 / 5) + 32);
                            t1tempLabel = t1temp + "F";
                        }

                        var v1voltage = s.P.V1 / 10;
                        var v2voltage = s.P.V2 / 10;

                        var testmode = new Boolean(s.P.TS);
                        var overtemp = new Boolean(Math.max(t1temp, t2temp) > tempthreshold);

                        var uf = formatUptime(parseInt(s.P.U));

                        var u = "<table class='multiSyncVerboseTable'>";
                        u += '<tr><td><small class="text-muted">UP:</small></td><td><span title="since ' + uf.since + '">' + uf.short + '</span></td></tr>';
                        u += "<tr><td><small class='text-muted'>V1:</small></td><td> " + v1voltage + "v</td></tr>";
                        if (s.P.BR != 48) {
                            u += "<tr><td><small class='text-muted'>V2:</small></td><td> " + v2voltage + "v</td></tr>";
                        }
                        u += "<tr><td><small class='text-muted'>TEMP:</small></td><td> " + t1tempLabel + "</td></tr>";
                        u += "</table>";

                        var rowId = hostRows[ip.replace(/\./g, '_')];
                        var item = $tbl.bootstrapTable('getRowByUniqueId', rowId);
                        if (!item) return;

                        item.utilization = u;
                        item.status = s.status_name;

                        if (testmode == true || overtemp == true) {
                            $('#' + rowId + '_warnings').removeAttr('style');
                            if ($('#' + rowId).hasClass('odd'))
                                $('#' + rowId + '_warnings').addClass('odd');

                            var wHTML = "";
                            if (testmode == true) wHTML += "<span class='warning-text'>Controller Test mode is active</span><br>";
                            if (overtemp == true) wHTML += "<span class='warning-text'>Pixel brightness reduced due to high temperatures</span><br>";

                            $('#' + rowId + '_warningCell').html(wHTML);
                        }
                    });
                    safeInitBody($tbl);
                })());
            }
            await Promise.allSettled(promises);
            if ($('#MultiSyncRefreshStatus').is(":checked") && !isUserInteracting()) {
                falconPoller.schedule(() => getFalconControllerStatus(fv3ips, fv4ips, true));
            }
        }

        // ============================================================
        // SECTION: WLED polling
        // ============================================================

        async function getWLEDControllerStatus(ipAddresses, refreshing = false) {
            ips = "";
            if (Array.isArray(ipAddresses)) {
                wledPoller.cancel();
                ipAddresses.forEach(function (entry) {
                    ips += "&ip[]=" + entry;
                });
            } else {
                ips = "&ip[]=" + ipAddresses;
            }
            if (ips == "") {
                return;
            }
            try {
                const r = await fetch("api/system/status?type=WLED" + ips);
                const alldata = await r.json();
                var $tbl = $('#fppSystemsTable');
                Object.entries(alldata).forEach(function (entry) {
                    var ip = entry[0], data = entry[1];
                    if (data == null || data == "" || data == "null") {
                        return;
                    }
                    var rssi = data.wifi.rssi;
                    var quality = data.wifi.signal;
                    var wifiDesc = quality < 25 ? 'weak' : quality < 50 ? 'fair' : quality < 75 ? 'good' : 'excellent';
                    var wifiIcon = '<span title="' + quality + '% ' + rssi + 'dBm" class="wifi-icon wifi-' + wifiDesc + '"></span>';

                    var uf = formatUptime(parseInt(data.uptime));

                    var u = "<table class='multiSyncVerboseTable'>";
                    u += '<tr><td><small class="text-muted">UP:</small></td><td><span title="since ' + uf.since + '">' + uf.short + '</span></td></tr>';
                    u += "</table>";

                    var rowId = hostRows[ip.replace(/\./g, '_')];
                    var item = $tbl.bootstrapTable('getRowByUniqueId', rowId);
                    if (!item) return;
                    item.utilization = u;
                    item.status = data.status_name;

                    var endTag = ip + '</a>';
                    var base = item._baseIpHtml || '';
                    var extra = item._extraIpHtml || '';
                    if (base.indexOf(endTag) !== -1) {
                        item.ipaddress = base.replace(endTag, endTag + wifiIcon) + extra;
                    } else {
                        item.ipaddress = base + wifiIcon + extra;
                    }
                });
                safeInitBody($tbl);
            } finally {
                if (Array.isArray(ipAddresses) && $('#MultiSyncRefreshStatus').is(":checked") && !isUserInteracting()) {
                    wledPoller.schedule(() => getWLEDControllerStatus(ipAddresses, true));
                }
            }
        }

        // ============================================================
        // SECTION: Genius polling
        // ============================================================

        async function getGeniusControllerStatus(ipAddresses, refreshing = false) {
            ips = "";
            if (Array.isArray(ipAddresses)) {
                geniusPoller.cancel();
                ipAddresses.forEach(function (entry) {
                    ips += "&ip[]=" + entry;
                });
            } else {
                ips = "&ip[]=" + ipAddresses;
            }
            if (ips == "") {
                return;
            }
            try {
                const r = await fetch("api/system/status?type=Genius" + ips);
                const alldata = await r.json();
                var $tbl = $('#fppSystemsTable');
                Object.entries(alldata).forEach(function (entry) {
                    var ip = entry[0], data = entry[1];
                    if (data == null || data == "" || data == "null") {
                        return;
                    }
                    var uf = formatUptime(data.system.uptime_seconds);

                    var u = "<table class='multiSyncVerboseTable'>";
                    u += '<tr><td><small class="text-muted">UP:</small></td><td><span title="since ' + uf.since + '">' + uf.short + '</span></td></tr>';
                    u += "</table>";

                    var rowId = hostRows[ip.replace(/\./g, '_')];
                    var item = $tbl.bootstrapTable('getRowByUniqueId', rowId);
                    if (!item) return;
                    item.utilization = u;
                    item.status = data.status_name;
                    if (item.hostname.indexOf("class='hostDescriptionSM'></small>") >= 0) {
                        item.hostname = item.hostname.replace(
                            "class='hostDescriptionSM'></small>",
                            "class='hostDescriptionSM'>" + data.system.friendly_name + "</small>"
                        );
                    }
                });
                safeInitBody($tbl);
            } finally {
                if (Array.isArray(ipAddresses) && $('#MultiSyncRefreshStatus').is(":checked") && !isUserInteracting()) {
                    geniusPoller.schedule(() => getGeniusControllerStatus(ipAddresses, true));
                }
            }
        }

        // ============================================================
        // SECTION: Baldrick polling
        // ============================================================

        async function getBaldrickControllerStatus(ipAddresses, refreshing = false) {
            ips = "";
            if (Array.isArray(ipAddresses)) {
                baldrickPoller.cancel();
                ipAddresses.forEach(function (entry) {
                    ips += "&ip[]=" + entry;
                });
            } else {
                ips = "&ip[]=" + ipAddresses;
            }
            if (ips == "") {
                return;
            }
            try {
                const r = await fetch("api/system/status?type=Baldrick" + ips);
                const alldata = await r.json();
                var $tbl = $('#fppSystemsTable');
                Object.entries(alldata).forEach(function (entry) {
                    var ip = entry[0], data = entry[1];
                    if (data == null || data == "" || data == "null") {
                        return;
                    }
                    var uf = formatUptime(data.uptime);

                    var u = "<table class='multiSyncVerboseTable'>";
                    u += '<tr><td><small class="text-muted">UP:</small></td><td><span title="since ' + uf.since + '">' + uf.short + '</span></td></tr>';
                    u += "</table>";

                    var rowId = hostRows[ip.replace(/\./g, '_')];
                    var item = $tbl.bootstrapTable('getRowByUniqueId', rowId);
                    if (!item) return;

                    item.utilization = u;
                    if (item.hostname.indexOf("class='hostDescriptionSM'></small>") >= 0) {
                        item.hostname = item.hostname.replace(
                            "class='hostDescriptionSM'></small>",
                            "class='hostDescriptionSM'>" + (data.board_model || data.hostname) + "</small>"
                        );
                    }

                    var status = 'Idle';
                    if (data.test_mode_active) {
                        status = 'Testing';
                    } else if (data.frame_rate && data.frame_rate > 0) {
                        status = 'Playing';
                    }
                    item.status = status;

                    if (data.ota && data.ota.current_firmware_version && data.ota.available_firmware_version) {
                        var current = data.ota.current_firmware_version;
                        var available = data.ota.available_firmware_version;
                        if (current !== available) {
                            item.version = '<span class="text-warning" title="Update available: ' + available + '">' +
                                current + ' <i class="fas fa-exclamation-triangle"></i></span>';
                        } else {
                            item.version = current;
                        }
                    }
                });
                safeInitBody($tbl);
            } finally {
                if (Array.isArray(ipAddresses) && $('#MultiSyncRefreshStatus').is(":checked") && !isUserInteracting()) {
                    baldrickPoller.schedule(() => getBaldrickControllerStatus(ipAddresses, true));
                }
            }
        }

        // ============================================================
        // SECTION: Refresh orchestration
        // ============================================================

        function clearRefreshTimers() {
            fppPoller.cancel();
            falconPoller.cancel();
            wledPoller.cancel();
            geniusPoller.cancel();
            baldrickPoller.cancel();
        }

        function RefreshStats() {
            var $tbl = $('#fppSystemsTable');
            var ips = [], gips = [], wips = [], bips = [], fv3ips = [], fv4ips = [];
            var seenRows = {};

            Object.keys(hostRows).forEach(function (key) {
                var rowID = hostRows[key];
                if (seenRows[rowID]) return;
                seenRows[rowID] = true;

                var item = $tbl.bootstrapTable('getRowByUniqueId', rowID);
                if (!item) return;

                var ip = item._dataIp;
                var typeId = item._typeIdHex;

                if (item._isFPP) {
                    ips.push(ip);
                } else if (isESPixelStick(typeId)) {
                    var majorVersion = parseInt((item._versionStr || '0').split('.')[0]);
                    if (majorVersion >= 4) {
                        getESPixelStickBridgeStatus(ip);
                    } else {
                        ips.push(ip);
                    }
                } else if (isFalconV4(typeId)) {
                    fv4ips.push(ip);
                } else if (isFalcon(typeId)) {
                    fv3ips.push(ip);
                } else if (isWLED(typeId)) {
                    wips.push(ip);
                } else if (isGenius(typeId)) {
                    gips.push(ip);
                } else if (isBaldrick(typeId)) {
                    bips.push(ip);
                }
            });

            getFPPSystemStatus(ips, true);
            getGeniusControllerStatus(gips, true);
            getWLEDControllerStatus(wips, true);
            getBaldrickControllerStatus(bips, true);
            getFalconControllerStatus(fv3ips, fv4ips, true);
        }

        function MultiSyncEnableToggled() {
            SetRestartFlag();
        }


        function autoRefreshToggled() {
            if ($('#MultiSyncRefreshStatus').is(":checked")) {
                RefreshStats();
            }
        }

        function syncModeUpdated(setting = '') {
            var multicastChecked = $('#MultiSyncMulticast').is(":checked");
            var broadcastChecked = $('#MultiSyncBroadcast').is(":checked");

            if (setting == 'MultiSyncMulticast') {
                if (multicastChecked && broadcastChecked)
                    $('#MultiSyncBroadcast').prop('checked', false).trigger('change');
            } else if (setting == 'MultiSyncBroadcast') {
                if (multicastChecked && broadcastChecked)
                    $('#MultiSyncMulticast').prop('checked', false).trigger('change');
            }

            var anyUnicast = 0;
            $('input.syncCheckbox').each(function () {
                if ($(this).is(":checked")) {
                    anyUnicast = 1;
                }
            });

            if (!anyUnicast && !multicastChecked && !broadcastChecked) {
                $('#MultiSyncMulticast').prop('checked', true).trigger('change');
                alert('FPP will use multicast if no other sync methods are chosen.');
            }
        }

        // ============================================================
        // SECTION: OS upgrade helpers
        // ============================================================

        async function getLocalFpposFiles() {
            const r = await fetch('api/files/uploads');
            const data = await r.json();
            if (data.hasOwnProperty("files")) {
                    data.files.forEach(function (f) {
                        if (f.hasOwnProperty("name")) {
                            if (f.name.endsWith(".fppos")) {
                                let type = "UNKNOWN";
                                if (f.name.startsWith("BBB-")) {
                                    type = "BBB";
                                } else if (f.name.startsWith("BB64-")) {
                                    type = "BB64";
                                } else if (f.name.startsWith("Pi-")) {
                                    type = "PI";
                                }

                                if (type != "UNKNOWN") {
                                    localFpposFiles.push({
                                        type: type,
                                        name: f.name,
                                        sizeBytes: f.sizeBytes,
                                        sizeHuman: f.sizeHuman
                                    });
                                }
                            } // check if .fppos
                        } // Verify has name
                    }); // loop over files

                    if (localFpposFiles.length > 0) {
                        let html = [];
                        localFpposFiles.forEach(function (f) {
                            html.push('<div class="row">');
                            html.push('<div class="col-2 col-sm-1"><input id="');
                            html.push(f.name);
                            html.push('" type="checkbox"></div>')
                            html.push('<div class="col-7 col-sm-5 col-md-4 col-lg-3 col-xl-2">');
                            html.push(f.name);
                            html.push('</div><div class="col-2 col-sm-1">');
                            html.push(f.type);
                            html.push('</div><div class="col-2" col-md-1>');
                            html.push(f.sizeHuman);
                            html.push('</div>');
                            html.push("</div>");
                        });
                        $("#copyOSOptionsDetails").html(html.join(''));
                    }
            } // hasOwnProperty("files");
        }

        /**
         * Extracts unique platform types from all selected (checked) remote systems.
         * Skips systems that are currently filtered out in the UI.
         *
         * @returns {Array} Array of unique platform names (e.g., ['BBB', 'Pi'])
         */
        function getUniquePlatformsFromSelectedCheckboxes() {
            var platforms = new Set();

            $('input.remoteCheckbox').each(function () {
                if ($(this).is(":checked")) {
                    var rowID = $(this).closest('tr').attr('id');
                    if ($('#' + rowID).hasClass('filtered')) {
                        return true;
                    }
                    var platform = $('#' + rowID + '_platform').text();
                    if (platform) {
                        platforms.add(platform);
                    }
                }
            });

            return Array.from(platforms);
        }

        /**
         * Extracts unique IP addresses from all selected (checked) remote systems.
         * Skips systems that are currently filtered out in the UI.
         *
         * @returns {Array} Array of unique IP addresses from selected systems
         */
        function getUniqueIpFromSelectedCheckboxes() {
            var ips = new Set();

            $('input.remoteCheckbox').each(function () {
                if ($(this).is(":checked")) {
                    var rowID = $(this).closest('tr').attr('id');
                    if ($('#' + rowID).hasClass('filtered')) {
                        return true;
                    }
                    var ip = ipFromRowID(rowID);
                    if (ip) {
                        ips.add(ip);
                    }
                }
            });

            return Array.from(ips);
        }

        /**
         * Validates the prerequisites for performing an OS upgrade on selected systems.
         * Checks that:
         * - At least one system is selected
         * - All selected systems have the same platform
         *
         * As a side effect, updates the list of available OS files via updateOSFileList()
         *
         * Displays appropriate warning messages if validation fails, or clears warnings
         * and populates the OS file dropdown if validation succeeds.
         */
        function validateOSUpgrade() {
            var uniquePlatforms = getUniquePlatformsFromSelectedCheckboxes();
            var warningDiv = $('#osUpgradeWarning');
            $('#osUpgradeActionDiv').hide();

            if (uniquePlatforms.length === 0) {
                warningDiv.html('You must select at least one remote system.');
            } else if (uniquePlatforms.length > 1) {
                warningDiv.html('All selected systems must have the same platform. Currently selected: ' + uniquePlatforms.join(', '));
            } else {
                warningDiv.html('');
            }

            if (!$('#osUpgradeOptions').is(':visible') || warningDiv.html() != '') {
                // No point in doing Rest calls if the section isn't shown or there are warnings.
                return;
            }

            var ips = getUniqueIpFromSelectedCheckboxes();
            if (ips.length == 0) {
                warningDiv.html('Unable to find IP address for selected systems. Please ensure at least one system is selected and not filtered out.');
            } else {
                var foundFiles = false;
                var checkNextIp = async function (index) {
                    if (foundFiles || index >= ips.length) return;
                    var ip = ips[index];
                    warningDiv.html('Please Wait... Checking ' + ip + ' for OS Upgrade files...');
                    try {
                        const r = await fetch('api/remoteAction?ip=' + ip + '&action=listUpgrades');
                        const data = await r.json();
                        if (data && Array.isArray(data.files) && data.files.length > 0) {
                            foundFiles = true;
                            warningDiv.html('');
                            updateOSFileList(data.files);
                        } else {
                            await checkNextIp(index + 1);
                        }
                    } catch (error) {
                        console.error('Error querying ' + ip + ':', error);
                        await checkNextIp(index + 1);
                    }
                };
                checkNextIp(0);
            }
        }

        /**
         * Updates the OS file dropdown list with available OS upgrade files.
         * Clears previous options and populates with new files, applying a minimum
         * asset_id threshold to filter out deprecated versions.
         * Makes the upgrade action div visible after populating the list.
         *
         * @param {Array} files - Array of file objects containing 'asset_id' and 'filename' properties
         */
        function updateOSFileList(files) {
            // Cleanup previous load values
            $('#OSSelect option').filter(function () { return parseInt(this.value) > 0; }).remove();


            for (const file of files) {
                let id = file["asset_id"];
                if (id < 211762298) {
                    // This is a safety check to prevent some really old files from showing up in the list.
                    // The asset_id may need to be manually updated in the future.
                    continue;
                }
                $('#OSSelect').append($('<option>', {
                    value: id,
                    text: file["filename"]
                }));
            }

            $('#osUpgradeActionDiv').show();
        }

        // ============================================================
        // SECTION: Stream / upgrade actions
        // ============================================================

        function EnableDisableStreamButtons() {
            if (streamCount) {
                $('#performActionButton').prop("disabled", true);
                $('#refreshButton').prop("disabled", true);

                if (!$('#fppSystemsTableWrapper').hasClass('fppTableWrapperHighlighted')) {
                    $('#fppSystemsTableWrapper').addClass('fppTableWrapperHighlighted');
                    $('#exitWarning').show();
                }
            } else {
                $('#performActionButton').prop("disabled", false);
                $('#refreshButton').prop("disabled", false);
                $('#fppSystemsTableWrapper').removeClass('fppTableWrapperHighlighted');
                $('#exitWarning').hide();
            }
        }

        function upgradeDone(id) {
            id = id.replace('_logText', '');
            $('#' + id + '_doneButtons').show();
            streamCount--;

            var ip = ipFromRowID(id);
            setTimeout(function () { getFPPSystemStatus(ip, true); }, 3000);

            if (origins.hasOwnProperty(ip)) {
                for (var i = 0; i < origins[ip].length; i++) {
                    var rowID = "fpp_" + origins[ip][i].replace(/\./g, '_');
                    upgradeSystem(rowID);
                }
            }

            EnableDisableStreamButtons();
        }

        function upgradeFailed(id) {
            id = id.replace('_logText', '');
            var ip = ipFromRowID(id);

            alert('Update failed for FPP system at ' + ip);
            streamCount--;

            EnableDisableStreamButtons();

            if (!$('#fppSystemsTableWrapper').hasClass('fppTableWrapperErrored'))
                $('#fppSystemsTableWrapper').addClass('fppTableWrapperErrored');
        }

        function showLogsRow(rowID) {
            // Ensure child rows match parent striping color
            if ($('#' + rowID).hasClass('odd'))
                $('#' + rowID + '_logs').addClass('odd');

            $('#' + rowID + '_logs').show();
            rowSpanSet(rowID);
        }

        function addLogsDivider(rowID) {
            if ($('#' + rowID + '_logText').val() != '')
                $('#' + rowID + '_logText').val($('#' + rowID + '_logText').val() + '\n==================================\n');
        }

        function upgradeSystemByHostname(id) {
            id = id.replace('_logText', '');
            var ip = ipOrHostnameFromRowID(id);
            StreamURL('streamRemote.php?action=upgrade&ip=' + ip, id + '_logText', 'upgradeDone', 'upgradeFailed');
        }

        function upgradeSystem(rowID) {
            streamUpgrade(rowID, 'upgrade', 'upgradeSystemByHostname');
        }

        /**
         * Initiates an OS upgrade for a single remote system.
         * Delegates to streamUpgrade with the 'upgradeOS' action.
         *
         * @param {string} rowID - The table row ID of the target system
         * @param {string} os - The OS upgrade filename/version to apply
         */
        function upgradeOSSystem(rowID, os) {
            streamUpgrade(rowID, 'upgradeOS', 'upgradeFailed', '&os=' + os);
        }

        /**
         * Generic upgrade streaming handler that manages the upgrade process for a single system.
         * Handles the UI state (hiding checkboxes, showing logs), manages stream count,
         * and invokes the appropriate remote action via StreamURL.
         *
         * @param {string} rowID - The table row ID of the target system
         * @param {string} action - The type of upgrade action ('upgrade', 'upgradeOS', etc.)
         * @param {string} failedCallback - Name of the callback function to invoke on failure
         * @param {string} [additionalParams=''] - Additional URL parameters to pass to the remote action (e.g., '&os=filename')
         */
        function streamUpgrade(rowID, action, failedCallback, additionalParams = '') {
            $('#' + rowID).find('input.remoteCheckbox').prop('checked', false);

            streamCount++;
            EnableDisableStreamButtons();

            showLogsRow(rowID);
            addLogsDivider(rowID);

            var ip = ipFromRowID(rowID);
            StreamURL('streamRemote.php?action=' + action + '&ip=' + ip + additionalParams, rowID + '_logText', 'upgradeDone', failedCallback);
        }

        function showWaitingOnOriginUpdate(rowID, origin) {
            showLogsRow(rowID);
            addLogsDivider(rowID);

            $('#' + rowID + '_logText').append("Waiting for origin (" + origin + ") to finish updating...");
        }

        var origins = {};

        function upgradeOSSelectedSystems() {
            // Apply fppos upgrades to selected systems.
            var os = $('#OSSelect option:selected').text();
            if (os == '' || !os.includes('.fppos')) {
                alert('Please select a valid OS upgrade file to upgrade to.');
                return;
            }
            $('input.remoteCheckbox').each(function () {
                if ($(this).is(":checked")) {
                    var rowID = $(this).closest('tr').attr('id');
                    ip = ipFromRowID(rowID);
                    if ($('#' + rowID).hasClass('filtered')) {
                        return true;
                    }
                    upgradeOSSystem(rowID, os);
                }
            });
        }
        function upgradeSelectedSystems() {
            $origins = {};
            $('input.remoteCheckbox').each(function () {
                if ($(this).is(":checked")) {
                    var rowID = $(this).closest('tr').attr('id');
                    if ($('#' + rowID).hasClass('filtered')) {
                        return true;
                    }
                    var origin = $('#' + rowID + '_origin').html();
                    if (typeof origin === 'undefined') {
                        var msg = "Invalid Origin for " + ipFromRowID(rowID) + ". Skipping";
                        $.jGrowl(msg, { themeState: 'danger' });
                        return;
                    }
                    var originRowID = "fpp_" + origin.replace(/\./g, '_');
                    if ((origin != '') &&
                        (origin != 'github.com') &&
                        ($('#' + originRowID).find('input.remoteCheckbox').is(':checked'))) {
                        if (!origins.hasOwnProperty(origin)) {
                            origins[origin] = [];
                        }
                        origins[origin].push(ipFromRowID(rowID));
                    }
                }
            });

            var originsUpdating = {};
            $('input.remoteCheckbox').each(function () {
                if ($(this).is(":checked")) {
                    var rowID = $(this).closest('tr').attr('id');
                    if ($('#' + rowID).hasClass('filtered')) {
                        return true;
                    }
                    var ip = ipFromRowID(rowID);

                    if (origins.hasOwnProperty(ip)) {
                        originsUpdating[ip] = 1;
                    }
                }
            });

            $('input.remoteCheckbox').each(function () {
                if ($(this).is(":checked")) {
                    var rowID = $(this).closest('tr').attr('id');
                    if ($('#' + rowID).hasClass('filtered')) {
                        return true;
                    }

                    var origin = $('#' + rowID + '_origin').html();
                    if (typeof origin === 'undefined') {
                        var msg = "Invalid Origin for " + ipFromRowID(rowID) + ". Skipping";
                        $.jGrowl(msg, { themeState: 'danger' });
                        return;
                    }
                    var originRowID = "fpp_" + origin.replace(/\./g, '_');
                    if ((origin == '') ||
                        (origin == 'github.com') ||
                        (!originsUpdating.hasOwnProperty(origin))) {
                        upgradeSystem(rowID);
                    } else {
                        showWaitingOnOriginUpdate(rowID, origin);
                    }
                }
            });
        }

        // ============================================================
        // SECTION: System actions
        // ============================================================

        function actionDone(id) {
            id = id.replace('_logText', '');
            $('#' + id + '_doneButtons').show();
            streamCount--;

            var ip = ipFromRowID(id);

            setTimeout(function () { getFPPSystemStatus(ip, true); }, 1500);

            EnableDisableStreamButtons();
        }

        function actionFailed(id) {
            id = id.replace('_logText', '');
            var ip = ipFromRowID(id);

            alert('Action failed for FPP system at ' + ip);

            streamCount--;

            EnableDisableStreamButtons();
        }

        function restartSystem(rowID) {
            streamCount++;
            EnableDisableStreamButtons();

            showLogsRow(rowID);
            addLogsDivider(rowID);

            var ip = ipFromRowID(rowID);
            StreamURL('restartRemoteFPPD.php?ip=' + ip, rowID + '_logText', 'actionDone', 'actionFailed');
        }
        function setSystemMode(rowID, mode) {
            streamCount++;
            EnableDisableStreamButtons();

            showLogsRow(rowID);
            addLogsDivider(rowID);

            var ip = ipFromRowID(rowID);
            StreamURL('restartRemoteFPPD.php?ip=' + ip + '&mode=' + mode, rowID + '_logText', 'actionDone', 'actionFailed');
        }

        function restartSelectedSystems() {
            $('input.remoteCheckbox').each(function () {
                if ($(this).is(":checked")) {
                    var rowID = $(this).closest('tr').attr('id');
                    if ($('#' + rowID).hasClass('filtered')) {
                        return true;
                    }

                    $(this).prop('checked', false);
                    restartSystem(rowID);
                }
            });
        }


        function setSelectedSystemsMode(mode) {
            $('input.remoteCheckbox').each(function () {
                if ($(this).is(":checked")) {
                    var rowID = $(this).closest('tr').attr('id');
                    if ($('#' + rowID).hasClass('filtered')) {
                        return true;
                    }

                    $(this).prop('checked', false);
                    setSystemMode(rowID, mode);
                }
            });
        }

        function rebootSystem(rowID) {
            streamCount++;
            EnableDisableStreamButtons();

            showLogsRow(rowID);
            addLogsDivider(rowID);

            var ip = ipFromRowID(rowID);
            StreamURL('rebootRemoteFPP.php?ip=' + ip, rowID + '_logText', 'actionDone', 'actionFailed');
        }

        function rebootSelectedSystems() {
            $('input.remoteCheckbox').each(function () {
                if ($(this).is(":checked")) {
                    var rowID = $(this).closest('tr').attr('id');
                    if ($('#' + rowID).hasClass('filtered')) {
                        return true;
                    }

                    $(this).prop('checked', false);
                    rebootSystem(rowID);
                }
            });
        }
        function shutdownSystem(rowID) {
            streamCount++;
            EnableDisableStreamButtons();

            showLogsRow(rowID);
            addLogsDivider(rowID);

            var ip = ipFromRowID(rowID);
            StreamURL('shutdownRemoteFPP.php?ip=' + ip, rowID + '_logText', 'actionDone', 'actionFailed');
        }
        function shutdownSelectedSystems() {
            $('input.remoteCheckbox').each(function () {
                if ($(this).is(":checked")) {
                    var rowID = $(this).closest('tr').attr('id');
                    if ($('#' + rowID).hasClass('filtered')) {
                        return true;
                    }

                    $(this).prop('checked', false);
                    shutdownSystem(rowID);
                }
            });
        }

        // ============================================================
        // SECTION: Git / branch actions
        // ============================================================

        function changeBranch(rowID) {
            streamCount++;
            EnableDisableStreamButtons();

            showLogsRow(rowID);
            addLogsDivider(rowID);

            var ip = ipFromRowID(rowID);
            var branch = $("#branchSelect").val();
            var remote = $("#branchRemoteSelect").length ? $("#branchRemoteSelect").val() : 'origin';
            StreamURL('changeRemoteBranch.php?branch=' + encodeURIComponent(branch) + '&remote=' + encodeURIComponent(remote) + '&ip=' + ip, rowID + '_logText', 'actionDone', 'actionFailed');
        }
        async function reloadBranchSelect() {
            var remote = $("#branchRemoteSelect").length ? $("#branchRemoteSelect").val() : 'origin';
            const r = await fetch("api/git/branches?remote=" + encodeURIComponent(remote));
            const data = await r.json();
            $('#branchSelect').empty();
            data.forEach(function (item) {
                $('#branchSelect').append($('<option>', { value: item, text: item }));
            });
        }
        function changeBranchSelectedSystems() {
            $('input.remoteCheckbox').each(function () {
                if ($(this).is(":checked")) {
                    var rowID = $(this).closest('tr').attr('id');
                    if ($('#' + rowID).hasClass('filtered')) {
                        return true;
                    }

                    $(this).prop('checked', false);
                    changeBranch(rowID);
                }
            });
        }

        // ============================================================
        // SECTION: File copy actions
        // ============================================================

        function copyFilesToSystem(rowID) {
            streamCount++;
            EnableDisableStreamButtons();

            showLogsRow(rowID);
            addLogsDivider(rowID);

            var ip = getReachableIPFromRowID(rowID);
            if (ip == "") {
                ip = ipFromRowID(rowID);
                $('#' + rowID + '_logText').append('No IPs appear reachable for ' + ip);
            } else {
                StreamURL('copyFilesToRemote.php?ip=' + ip, rowID + '_logText', 'copyDone', 'copyFailed');
            }
        }

        function copyFileToSystem(file, rowID) {
            EnableDisableStreamButtons();
            streamCount++;

            showLogsRow(rowID);
            addLogsDivider(rowID);

            var ip = getReachableIPFromRowID(rowID);
            if (ip == "") {
                ip = ipFromRowID(rowID);
                $('#' + rowID + '_logText').append('No IPs appear reachable for ' + ip);
            } else {
                StreamURL('remotePush.php?dir=uploads&raw=1&filename=' + file.name + '&remoteHost=' + ip, rowID + '_logText', 'copyDone', 'copyFailed');
            }
        }

        function copyFilesToSelectedSystems() {
            $('input.remoteCheckbox').each(function () {
                if ($(this).is(":checked")) {
                    var rowID = $(this).closest('tr').attr('id');
                    if ($('#' + rowID).hasClass('filtered')) {
                        return true;
                    }

                    $(this).prop('checked', false);
                    copyFilesToSystem(rowID);
                }
            });
        }

        function copyOSFilesToSelectedSystems() {
            let files = [];
            let targets = [];
            $("#copyOSOptionsDetails input").each(function () {
                if ($(this).is(":checked")) {
                    let name = $(this).attr('id');
                    localFpposFiles.forEach(function (f) {
                        if (name === f.name) {
                            files.push(f);
                        }
                    });
                }
            });

            $('input.remoteCheckbox').each(function () {
                if ($(this).is(":checked")) {
                    var rowID = $(this).closest('tr').attr('id');
                    if ($('#' + rowID).hasClass('filtered')) {
                        return true;
                    }

                    $(this).prop('checked', false);
                    targets.push(rowID);
                }
            });

            if (files.length == 0) {
                $.jGrowl("No OS Upgrade files were selected", { themeState: 'danger' });
            } else if (targets.length == 0) {
                $.jGrowl("No remote FPP systems were selected", { themeState: 'danger' });
            } else {
                targets.forEach(function (rowID) {
                    files.forEach(function (f) {
                        ip = ipFromRowID(rowID);
                        osType = "UNKNOWN";
                        freeSpace = 0;
                        if (systemStatusCache.hasOwnProperty(ip)) {
                            rec = systemStatusCache[ip];
                            try {
                                let platform = rec["advancedView"]["Platform"];
                                if (platform == "BeagleBone Black") {
                                    osType = "BBB";
                                } else if (platform == "BeagleBone 64") {
                                    osType = "BB64";
                                } else if (platform == "Raspberry Pi") {
                                    osType = "PI";
                                }
                            } catch (error) {
                                // Eat it
                            }

                            try {
                                freeSpace = rec.advancedView.Utilization.Disk.Media.Free
                            } catch (error) {
                                // Eat it
                            }

                            // 0.95 is to not completely fill it.
                            if (freeSpace != 0 && ((f.sizeBytes) > (freeSpace * 0.95))) {
                                $.jGrowl("Insufficient free disk space on remote " + ip + ". Skipping", { themeState: 'danger' });
                            } else if (osType != f.type) {
                                let msg = f.name + " isn't compatible with " + ip + ". Skipping";
                                $.jGrowl(msg, { themeState: 'danger' });
                            } else {
                                copyFileToSystem(f, rowID);
                            }

                        } else {
                            $.jGrowl("Unable to locate data for " + ip + ". Skipping", { themeState: 'danger' });
                        }

                    });
                });
            }
        }

        function copyDone(id) {
            id = id.replace('_logText', '');
            $('#' + id + '_doneButtons').show();
            streamCount--;

            EnableDisableStreamButtons();
        }

        function copyFailed(id) {
            id = id.replace('_logText', '');
            var ip = ipFromRowID(id);

            alert('File Copy failed for FPP system at ' + ip);
            streamCount--;

            EnableDisableStreamButtons();

            if (!$('#fppSystemsTableWrapper').hasClass('fppTableWrapperErrored'))
                $('#fppSystemsTableWrapper').addClass('fppTableWrapperErrored');
        }

        // ============================================================
        // SECTION: Proxy actions
        // ============================================================

        async function addProxyForIP(rowID) {
            var ip = ipFromRowID(rowID);
            try {
                const r = await fetch("api/proxies/" + ip, { method: 'POST', body: "AddProxy" });
                if (!r.ok) throw new Error(r.status);
            } catch {
                DialogError("Failed to set Proxy", "Post failed");
            }
        }

        function proxySelectedIPs() {
            $('input.remoteCheckbox').each(function () {
                if ($(this).is(":checked")) {
                    var rowID = $(this).closest('tr').attr('id');
                    if ($('#' + rowID).hasClass('filtered')) {
                        return true;
                    }

                    $(this).prop('checked', false);
                    addProxyForIP(rowID);
                }
            });
        }

        // ============================================================
        // SECTION: Selection helpers
        // ============================================================

        function clearSelected() {
            // clear all entries, even if filtered
            $('input.remoteCheckbox').prop('checked', false).first().trigger('change');
        }

        function selectAll() {
            // select all entries that are not filtered out of view
            $('input.remoteCheckbox').each(function () {
                var rowID = $(this).closest('tr').attr('id');
                if (!$('#' + rowID).hasClass('filtered')) {
                    $(this).prop('checked', true);
                }
            });
            // Trigger change event to update platform validation
            $('input.remoteCheckbox:first').trigger('change');
        }

        function selectAllChanged() {
            if ($('#selectAllCheckbox').is(":checked")) {
                selectAll();
            } else {
                clearSelected();
            }
        }

        // ============================================================
        // SECTION: MultiSync settings
        // ============================================================

        // Updates the warning information for multi-sync
        // Also handles if warnings row should be displayed in general.
        function validateMultiSyncSettings() {
            var multicastChecked = $('#MultiSyncMulticast').is(":checked");
            var broadcastChecked = $('#MultiSyncBroadcast').is(":checked");
            // Remove any Multisync warnings
            $(document).find(".multisync-warning").remove();

            // check if Warnings window should be shown
            $(document).find('tr[id*="_warnings"').each(function () {
                let cnt = $(this).find('td[id*="_warningCell"').children().length;
                if (cnt == 0) { //no warnings
                    $(this).hide();
                } else { //has warning messages
                    if ($(this).hasClass('filtered')) {
                        $(this).hide();
                    } else {
                        $(this).show();
                    }
                }
            });


            // If these are unchecked, than each remote can be set either way
            // no need to check anything
            if (!(multicastChecked || broadcastChecked)) {
                return;
            }

            $("input.syncCheckbox").each(function () {
                if ($(this).is(":checked")) {
                    let name = $(this).attr('name');
                    let msg = "";
                    if (multicastChecked) {
                        msg = "Having Unicast checked when Multicast is enabled (view options below) is discouraged: " + name;
                    } else {
                        msg = "Having Unicast checked when Broadcast is enabled (view options below) is discouraged: " + name;
                    }
                    msg = '<div class="warning-text multisync-warning">' + msg + '</div>';
                    $(this).closest('.systemRow').next(".warning-row").each(function () {
                        $(this).find("td").append(msg);
                        $(this).show();
                    })
                }
            });
        }

        async function updateMultiSyncRemotes(verbose = false) {
            var remotes = "";

            $('input.syncCheckbox').each(function () {
                if ($(this).is(":checked")) {
                    if (remotes != "") {
                        remotes += ",";
                    }
                    remotes += $(this).attr("name");
                }
            });

            var multicastChecked = $('#MultiSyncMulticast').is(":checked");
            var broadcastChecked = $('#MultiSyncBroadcast').is(":checked");

            if ((remotes == '') &&
                (!$('#MultiSyncMulticast').is(":checked")) &&
                (!$('#MultiSyncBroadcast').is(":checked"))) {
                $('#MultiSyncMulticast').prop('checked', true).trigger('change');
                alert('FPP will use multicast if no other sync methods are chosen.');
            }

            try {
                const r = await fetch("api/settings/MultiSyncRemotes", {
                    method: 'PUT',
                    body: JSON.stringify(remotes),
                    headers: { 'Content-Type': 'application/json' }
                });
                if (!r.ok) throw new Error(r.status);
                settings['MultiSyncRemotes'] = remotes;
                if (verbose) {
                    if (remotes == "") {
                        $.jGrowl("Remote List Cleared.  You must restart fppd for the changes to take effect.", { themeState: 'success' });
                    } else {
                        $.jGrowl("Remote List set to: '" + remotes + "'.  You must restart fppd for the changes to take effect.", { themeState: 'success' });
                    }
                }
                //Mark FPPD as needing restart
                SetRestartFlag(2);
                settings['restartFlag'] = 2;
                //Get the resart banner showing
                CheckRestartRebootFlags();
                validateMultiSyncSettings();
            } catch {
                DialogError("Save Remotes", "Save Failed");
            }
        }

        function exportMultisync() {
            if (systemStatusCache == null || systemStatusCache == "" || systemStatusCache == "null") {
                $.jGrowl("Please wait until the system statuses finish loading", { themeState: 'danger' });
                return;
            }
            const allKeys = new Set();
            let finalData = {};
            // Flatten the data
            for (const [ip, data] of Object.entries(systemStatusCache)) {
                let rc = {};
                flattenObject(data, "", rc);
                finalData[ip] = rc;

                for (const [key, junk] of Object.entries(rc)) {
                    allKeys.add(key);
                }
            }

            // Create XLSX
            const sortedKeys = Array.from(allKeys).sort();
            let labels = ['ip'];
            labels = labels.concat(sortedKeys);

            let allRows = [];
            allRows.push(labels);

            for (const [ip, data] of Object.entries(finalData)) {
                let row = [];
                row.push(ip);
                let value = "";
                for (const key of sortedKeys) {
                    if (key in data) {
                        value = data[key];
                    }
                    row.push(value);
                }
                allRows.push(row);
            }

            var wb = XLSX.utils.book_new();
            wb.SheetNames.push("Data");
            var ws = XLSX.utils.aoa_to_sheet(allRows);
            wb.Sheets["Data"] = ws;
            var wbout = XLSX.write(wb, { bookType: 'xlsx', type: 'binary' });
            saveAs(new Blob([s2ab(wbout)], { type: "application/octet-stream" }), 'export.xlsx');

        }

        // ============================================================
        // SECTION: Multi-action dispatch
        // ============================================================

        async function triggerCSPBashScript() {
            try {
                const response = await fetch('CSP_regeneration_script.php');
                const data = await response.text();
                //console.log(data);
                // Continue with the next JavaScript command
                console.log('CSP Regenerated.');
            } catch (error) {
                console.error('Error:', error);
            }
        }

        function performMultiAction() {
            //regenerate the CSP file to pick up all MultiSync IPs
            triggerCSPBashScript();

            var action = $('#multiAction').val();

            switch (action) {
                case 'upgradeFPP': upgradeSelectedSystems(); break;
                case 'upgradeOS': upgradeOSSelectedSystems(); break;
                case 'restartFPPD': restartSelectedSystems(); break;
                case 'copyFiles': copyFilesToSelectedSystems(); break;
                case 'copyOSFiles': copyOSFilesToSelectedSystems(); break;
                case 'reboot': rebootSelectedSystems(); break;
                case 'shutdown': shutdownSelectedSystems(); break;
                case 'remoteMode': setSelectedSystemsMode('remote'); break;
                case 'playerMode': setSelectedSystemsMode('player'); break;
                case 'addProxy': proxySelectedIPs(); break;
                case 'changeBranch': changeBranchSelectedSystems(); break;
                default: alert('You must select an action first.'); break;
            }

            $('#selectAllCheckbox').prop('checked', false);
        }

        function multiActionChanged() {
            var action = $('#multiAction').val();

            $('.actionOptions').hide();

            switch (action) {
                case 'copyFiles': $('#copyOptions').show(); break;
                case 'copyOSFiles': $('#copyOSOptions').show(); break;
                case 'changeBranch': $('#changeBranchOptions').show(); break;
                case 'upgradeOS': $('#osUpgradeOptions').show(); validateOSUpgrade(); break;
            }
        }

    </script>
</head>

<body>
    <div id="bodyWrapper">
        <?php
        $activeParentMenuItem = 'status';
        include 'menu.inc'; ?>
        <div class="mainContainer">
            <h1 class="title">FPP MultiSync</h1>
            <div class="pageContent">
                <div id="uifppsystems" class="settings">


                    <!-- Bootstrap popover button for column selection  -->
                    <button id="columnSelectorBtn" type="button" class="buttons btn btn-secondary"
                        data-bs-toggle="popover" data-bs-placement="bottom" data-bs-title="Select Columns to Display">
                        Select Columns to Display
                    </button>

                    <!-- Hidden Popover Content with Checkboxes -->
                    <div id="popoverContent" class="hidden">
                        <div id="columnSelector" class="columnSelector">
                            <!-- this div is where the column selector is added -->
                        </div>
                    </div>
                    <!-- Sort by Color Btn  -->
                    <button id="sortbyColorBtn" type="button" class="buttons btn btn-secondary"
                        data-bs-placement="bottom" data-bs-title="Sort Systems by Color"
                        onclick="sortSystemsByColor();">
                        Sort Systems by Color
                    </button>

                    <!-- Display Order Buttons -->
                    <button id="reorderModeBtn" type="button" class="buttons btn btn-secondary"
                        data-bs-placement="bottom" data-bs-title="Drag and drop systems to reorder them"
                        onclick="toggleReorderMode();">
                        <i class="fas fa-arrows-alt"></i> Reorder Systems
                    </button>
                    <button id="saveDisplayOrderBtn" type="button" class="buttons btn btn-secondary"
                        data-bs-placement="bottom" data-bs-title="Save the current display order of systems"
                        onclick="saveDisplayOrder();">
                        <i class="fas fa-save"></i> Save Display Order
                    </button>
                    <button id="sortBySavedOrderBtn" type="button" class="buttons btn btn-secondary"
                        data-bs-placement="bottom" data-bs-title="Re-apply the saved display order"
                        onclick="sortBySavedOrder();" style="display:none;">
                        <i class="fas fa-sort-amount-down"></i> Saved Order
                    </button>
                    <button id="clearDisplayOrderBtn" type="button" class="buttons btn btn-secondary"
                        data-bs-placement="bottom" data-bs-title="Clear the saved display order"
                        onclick="clearDisplayOrder();" style="display:none;">
                        <i class="fas fa-times"></i> Clear Display Order
                    </button>


                    <div id='fppSystemsTableWrapper' class='fppTableWrapper fppTableWrapperAsTable backdrop'>
                        <div class='fppTableContents' role="region" aria-labelledby="fppSystemsTable" tabindex="0">
                            <table id='fppSystemsTable' class="bootstrap-popup" cellpadding='3'>
                                <thead>
                                    <tr>
                                        <th data-field="hostname" data-sortable="true" data-filter-control="input">
                                            Hostname</th>
                                        <th data-field="ipaddress" data-sortable="true" data-filter-control="input"
                                            data-sorter="ipSorter">IP Address</th>
                                        <th data-field="platform" data-sortable="true" data-filter-control="select"
                                            data-filter-data="var:platformFilterOptions"
                                            data-filter-custom-search="platformFilterSearch">Platform</th>
                                        <th data-field="mode" data-sortable="true" data-filter-control="select"
                                            data-filter-data="var:modeFilterOptions"
                                            data-filter-custom-search="modeFilterSearch">Mode</th>
                                        <th data-field="status" data-sortable="true" data-filter-control="input">
                                            Status</th>
                                        <th data-field="elapsed" data-sortable="false">Elapsed</th>
                                        <th data-field="version" data-sortable="true" data-filter-control="input">
                                            Version</th>
                                        <th data-field="gitversions" data-sortable="false">Git Versions</th>
                                        <th data-field="utilization" data-sortable="false">Utilization</th>
                                        <th data-field="selectbox" data-sortable="false" data-filter-control="false"
                                            data-switchable="false">
                                            <input id='selectAllCheckbox' type='checkbox'
                                                class='largeCheckbox multisyncRowCheckbox'
                                                onChange='selectAllChanged();' />
                                        </th>

                                    </tr>
                                </thead>
                                <tbody id='fppSystems'>
                                    <tr>
                                        <td colspan=8 align='center'>Loading system list from fppd.</td>
                                    </tr>
                                </tbody>
                            </table>
                        </div>
                    </div>
                    <div class='container-fluid settingsTable settingsGroupTable'>
                        <? PrintSetting('MultiSyncEnabled', 'MultiSyncEnableToggled'); ?>
                        <? PrintSetting('MultiSyncRefreshStatus', 'autoRefreshToggled'); ?>
                    </div>

                    <div class="multisyncAdvancedFormActions row">
                        <div class="form-actions col-md">
                            <button class="fppSystemsUiSettingsToggle buttons dropdown-toggle" type="button"
                                data-bs-toggle="collapse" data-bs-target="#fppSystemsUiSettingsDrawer"
                                aria-expanded="false" aria-controls="fppSystemsUiSettingsDrawer">
                                <i class="fas fa-cog"></i> More Settings</button>
                            <button id='exportStatsButton' type='button' class='buttons' value='Export'
                                onClick='exportMultisync();'><i class="fas fa-scroll"></i> Export </button>
                            <button id='refreshStatsButton' type='button' class='buttons' value='Refresh Stats'
                                onClick='clearRefreshTimers(); RefreshStats();'><i class="fas fa-redo"></i> Refresh
                                Stats</button>
                            <div class="ms-2">
                            </div>
                        </div>
                        <div class="col-md-auto">
                            <div class="form-actions multisyncBulkActions  ">
                                <b>Action for selected systems:</b>
                                <select id='multiAction' onChange='multiActionChanged();'>
                                    <option value='noop'>---- Select an Action ----</option>
                                    <option value='upgradeFPP'>Upgrade FPP</option>
                                    <option value='upgradeOS'>Upgrade OS</option>
                                    <option value='restartFPPD'>Restart FPPD</option>
                                    <option value='reboot'>Reboot</option>
                                    <option value='shutdown'>Shutdown</option>
                                    <option value='copyFiles'>Copy Show Files</option>
                                    <option value='copyOSFiles'>Copy OS Upgrade Files</option>
                                    <option value='playerMode'>Set to Player</option>
                                    <option value='remoteMode'>Set to Remote</option>
                                    <option value='addProxy'>Add as Proxy</option>
                                    <? if ($uiLevel > 0) { ?>
                                            <option value='changeBranch'>Change Branch</option>
                                    <? } ?>
                                </select>
                                <button id='performActionButton' type='button' class='buttons btn-success' value='Run'
                                    onClick='performMultiAction();'><i class="fas fa-chevron-right"></i> Run</button>
                                <input type='button' class='buttons' value='Clear List' onClick='clearSelected();'>
                            </div>
                        </div>

                    </div>

                    <div style='text-align: left;'>
                        <div id='copyOSOptions' class='actionOptions'>
                            <h2>Copy OS Upgrade Files</h2>
                            <div class="container-fluid" id='copyOSOptionsDetails'>
                                <span class="warning-text">No .fppos files found on this system.</span>
                            </div>
                            <b>The rsync daemon must be enabled on the remote system(s) to use this functionality. rsync
                                can be enabled by going to the System tab on the Settings page of the remote system.</b>
                        </div>
                        <div id='changeBranchOptions' class='actionOptions'>
                            <h2>
                                <? if ($uiLevel > 0) {
                                    $defaultRemote = isset($settings['gitRemote']) ? $settings['gitRemote'] : 'origin';
                                    if (!preg_match('/^[a-zA-Z0-9_-]+$/', $defaultRemote)) {
                                        $defaultRemote = 'origin';
                                    }
                                    ?>
                                        Remote:
                                        <select id="branchRemoteSelect" onChange="reloadBranchSelect();">
                                            <option value="origin" <?= $defaultRemote === 'origin' ? ' selected' : '' ?>>FPP
                                                Releases (origin)</option>
                                            <option value="newfeatures" <?= $defaultRemote === 'newfeatures' ? ' selected' : '' ?>>
                                                FPP New Feature Testing (newfeatures)</option>
                                        </select>
                                        &nbsp;
                                <? } ?>
                                Change to branch:
                                <select id="branchSelect">
                                </select>
                            </h2>
                        </div>
                        <div id='osUpgradeOptions' class='actionOptions'>
                            <h2>Upgrade OS</h2>
                            <p>This will apply the FPPOS upgrade file to each selected remote system, triggering the
                                upgrade process.
                                This process can take a significant amount of time. As this executes a fppos update,
                                please use with great care.
                                <br>
                                <b>NOTES:</b>
                            <ul>
                                <li>This will fail if the FPPOS file is not already on the system. You can use the "Copy
                                    OS Upgrade Files" action
                                    to copy it to the system before applying it.</li>
                                <li>You should manually verify there is at least 200MB of free space on the remote
                                    system before applying the upgrade
                                    as it can fail if there isn't enough space to apply the update.</li>
                            </ul>
                            </p>
                            <div id='osUpgradeWarning' class='warning-text'>
                                Warning message goes here
                            </div>
                            <div id='osUpgradeActionDiv'>
                                <b>Select File:</b>
                                <select class='OSSelect' id='OSSelect'>
                                    <option value=''>-- Choose an OS Version --</option>
                                </select>
                            </div>
                        </div>
                        <span class='actionOptions' id='copyOptions'>
                            <br>
                            <?php
                            PrintSettingGroupTable('multiSyncCopyFiles', '', '', 0);
                            ?>
                            <b>The rsync daemon must be enabled on the remote system(s) to use this functionality. rsync
                                can be enabled by going to the System tab on the Settings page of the remote system.</b>
                        </span>
                    </div>

                    <div id='exitWarning' class='alert alert danger' style='display: none;'>WARNING: Other FPP Systems
                        are being updated from this interface. DO NOT reload or exit this page until these updates are
                        complete.</b></div>




                    <div id="fppSystemsUiSettingsDrawer" class="collapse">
                        <div id="multisyncViewOptions" class="fppSystemsUiSettings card ">
                            <div class="container-fluid settingsTable">

                                <?
                                PrintSetting('MultiSyncExternalIPAddress');
                                PrintSetting('MultiSyncMulticast', 'syncModeUpdated');
                                PrintSetting('MultiSyncBroadcast', 'syncModeUpdated');
                                PrintSetting('MultiSyncExtraRemotes');
                                PrintSetting('MultiSyncHTTPSubnets');
                                PrintSetting('MultiSyncHide10', 'getFPPSystems');
                                PrintSetting('MultiSyncHide172', 'getFPPSystems');
                                PrintSetting('MultiSyncHide192', 'getFPPSystems');
                                ?>
                            </div>

                            <?
                            if ($uiLevel > 0) {
                                echo "<b><i class='fas fa-fw fa-graduation-cap fa-nbsp ui-level-1' title='Advanced Level Setting'></i> - Advanced Level Setting</b>\n";
                            }
                            ?>

                        </div>


                    </div>
                </div>



            </div>
        </div>
    </div>
    <?php include 'common/footer.inc'; ?>
    </div>

    <script>

        $(document).ready(function () {

            // Channel I/O icon tooltip popovers
            var activePopover = null;
            $(document).on('mouseenter', '.channel-io-icon-output, .channel-io-icon-input', function () {
                var $icon = $(this);
                var isOutput = $icon.hasClass('channel-io-icon-output');
                // Dispose any existing popover
                if (activePopover) {
                    bootstrap.Popover.getInstance(activePopover)?.dispose();
                    activePopover = null;
                }
                activePopover = this;
                showChannelIOPopover(this, isOutput);
            });
            $(document).on('mouseleave', '.channel-io-icon-output, .channel-io-icon-input', function () {
                var self = this;
                // Small delay so user can move mouse into the popover
                setTimeout(function () {
                    if (!$('.channel-io-popover:hover').length) {
                        bootstrap.Popover.getInstance(self)?.dispose();
                        if (activePopover === self) activePopover = null;
                    }
                }, 200);
            });
            $(document).on('mouseleave', '.channel-io-popover', function () {
                // When mouse leaves the popover body, dispose it
                if (activePopover) {
                    bootstrap.Popover.getInstance(activePopover)?.dispose();
                    activePopover = null;
                }
            });

            fetch("api/proxies")
                .then(function (r) { return r.json(); })
                .then(function (data) {
                    // Extract just the host IPs from the proxy objects
                    proxies = data.map(function (proxy) { return proxy.host; });

                    // Update any existing links now that proxies are loaded
                    $("a[data-ip]").each(function () {
                        let ip = $(this).attr('data-ip');
                        $(this).attr('href', wrapUrlWithProxy(ip, "/"));
                    });

                    getFPPSystems();
                    getLocalFpposFiles();
                });
            fetch("api/git/branches")
                .then(function (r) { return r.json(); })
                .then(function (data) {
                    data.forEach(function (item) {
                        $('#branchSelect').append($('<option>', { value: item, text: item }));
                    });
                });

            // Reload branch list when remote selection changes (dev UI mode)
            $('#branchRemoteSelect').on('change', function () {
                reloadBranchSelect();
            });

            var $table = $('#fppSystemsTable');

            $("#MultiSyncBroadcast").on("change", validateMultiSyncSettings);
            $("#MultiSyncMulticast").on("change", validateMultiSyncSettings);

            // Custom IP sorter for Bootstrap Table - numeric IP comparison
            window.ipSorter = function (a, b) {
                // Extract first IP address from cell HTML (may contain links/tags)
                var ipRe = /(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})/;
                var mA = String(a).match(ipRe);
                var mB = String(b).match(ipRe);
                var ipA = mA ? mA[1] : '0.0.0.0';
                var ipB = mB ? mB[1] : '0.0.0.0';
                var partsA = ipA.split('.').map(Number);
                var partsB = ipB.split('.').map(Number);
                for (var i = 0; i < 4; i++) {
                    if ((partsA[i] || 0) !== (partsB[i] || 0)) {
                        return (partsA[i] || 0) - (partsB[i] || 0);
                    }
                }
                return 0;
            };

            window.fppColorSorter = function (a, b) {
                var colorA = parseInt(a, 10);
                var colorB = parseInt(b, 10);
                var hasA = !isNaN(colorA) && colorA >= 0;
                var hasB = !isNaN(colorB) && colorB >= 0;
                // Rows with a color sort before rows without
                if (hasA && !hasB) return -1;
                if (!hasA && hasB) return 1;
                if (!hasA && !hasB) return 0;
                return colorA - colorB;
            };

            // Custom Platform filter dropdown options and search function
            window.platformFilterOptions = {
                'FPP (All)': 'FPP (All)',
                'FPP (Raspberry Pi)': 'FPP (Raspberry Pi)',
                'FPP (BeagleBone)': 'FPP (BeagleBone)',
                'FPP (Armbian)': 'FPP (Armbian)',
                'FPP (MacOS)': 'FPP (MacOS)',
                'Falcon': 'Falcon',
                'FalconV4': 'FalconV4',
                'ESPixelStick': 'ESPixelStick',
                'Genius': 'Genius',
                'SanDevices': 'SanDevices',
                'WLED': 'WLED',
                'Unknown': 'Unknown'
            };
            window.platformFilterSearch = function (text, value, field, data) {
                if (!text || text === '') return true;
                // value is HTML-stripped lowercased cell text which includes hidden typeId hex
                // Extract the hex typeId (e.g. "0x02") from the concatenated text
                var match = value.match(/0x[0-9a-f]+/i);
                var typeId = match ? match[0] : '0x00';
                // filter-control lowercases the search text, so normalize
                switch (text.toLowerCase()) {
                    case 'fpp (all)': return isFPP(typeId);
                    case 'fpp (raspberry pi)': return isFPPPi(typeId);
                    case 'fpp (beaglebone)': return isFPPBeagleBone(typeId);
                    case 'fpp (armbian)': return isFPPArmbian(typeId);
                    case 'fpp (macos)': return isFPPMac(typeId);
                    case 'falcon': return isFalcon(typeId);
                    case 'falconv4': return isFalconV4(typeId);
                    case 'espixelstick': return isESPixelStick(typeId);
                    case 'genius': return isGenius(typeId);
                    case 'sandevices': return isSanDevices(typeId);
                    case 'wled': return isWLED(typeId);
                    case 'unknown': return isUnknownController(typeId);
                    default: return true;
                }
            };

            // Custom Mode filter dropdown options and search function
            window.modeFilterOptions = {
                'Master': 'Master',
                'Player': 'Player',
                'Player w/ Multisync': 'Multisync',
                'Bridge': 'Bridge',
                'Remote': 'Remote'
            };
            window.modeFilterSearch = function (text, value, field, data) {
                if (!text || text === '') return true;
                // value is HTML-stripped lowercased cell text (icons stripped away)
                var v = value.trim();
                var t = text.toLowerCase();
                if (t === 'player w/ multisync') return v.indexOf('player w/ multisync') === 0;
                if (t === 'player') return v === 'player' || (v.indexOf('player') === 0 && v.indexOf('player w/ multisync') !== 0);
                return v.indexOf(t) === 0;
            };

            // Build column selector checkboxes for Bootstrap Table
            // Responsive breakpoints matching old tablesorter columnSelector config
            var columnResponsivePriority = {
                'ipaddress': 6,   // hide at <= 70em
                'platform': 5,    // hide at <= 60em
                'mode': 4,        // hide at <= 50em
                'status': 3,      // hide at <= 40em
                'elapsed': 2,     // hide at <= 30em
                'version': 1,     // hide at <= 20em
                'gitversions': 1,
                'utilization': 2
            };
            var columnBreakpoints = ['20em', '30em', '40em', '50em', '60em', '70em'];
            var autoResponsive = true;

            function applyResponsiveColumns() {
                if (!autoResponsive) return;
                var $tbl = $('#fppSystemsTable');
                if (!$tbl.closest('.bootstrap-table').length) return;
                var viewWidth = window.innerWidth;
                Object.keys(columnResponsivePriority).forEach(function (field) {
                    var priority = columnResponsivePriority[field];
                    var bpStr = columnBreakpoints[priority - 1];
                    // Convert em to px (assume 16px base)
                    var bpPx = parseFloat(bpStr) * 16;
                    if (viewWidth <= bpPx) {
                        $tbl.bootstrapTable('hideColumn', field);
                    } else {
                        $tbl.bootstrapTable('showColumn', field);
                    }
                });
                // Update checkboxes to reflect current state
                $('#columnSelector input[type="checkbox"]').each(function () {
                    var field = $(this).data('field');
                    if (field) {
                        var isVisible = $tbl.bootstrapTable('getVisibleColumns').some(function (c) { return c.field === field; });
                        $(this).prop('checked', isVisible);
                    }
                });
            }

            window.buildColumnSelector = function () {
                var $tbl = $('#fppSystemsTable');
                var $container = $('#columnSelector');
                $container.empty();

                // Auto responsive toggle (first label gets dotted border via CSS)
                var autoLabel = $('<label></label>');
                var autoCheckbox = $('<input type="checkbox">').prop('checked', autoResponsive);
                autoCheckbox.on('change', function () {
                    autoResponsive = $(this).prop('checked');
                    if (autoResponsive) {
                        applyResponsiveColumns();
                        // Disable manual checkboxes in auto mode
                        $container.find('input[type="checkbox"]').not(this).prop('disabled', true);
                    } else {
                        $container.find('input[type="checkbox"]').not(this).prop('disabled', false);
                    }
                });
                autoLabel.append(autoCheckbox).append(' Auto');
                $container.append(autoLabel).append('<br>');

                // Individual column checkboxes
                var columns = $tbl.bootstrapTable('getVisibleColumns').concat($tbl.bootstrapTable('getHiddenColumns'));
                columns.forEach(function (col) {
                    if (col.field === 'hostname' || col.field === 'selectbox') return;
                    var isVisible = $tbl.bootstrapTable('getVisibleColumns').some(function (c) { return c.field === col.field; });
                    var label = $('<label></label>');
                    var checkbox = $('<input type="checkbox">').prop('checked', isVisible).data('field', col.field);
                    if (autoResponsive) checkbox.prop('disabled', true);
                    checkbox.on('change', function () {
                        var field = $(this).data('field');
                        if ($(this).prop('checked')) {
                            $tbl.bootstrapTable('showColumn', field);
                        } else {
                            $tbl.bootstrapTable('hideColumn', field);
                        }
                    });
                    label.append(checkbox).append(' ' + col.title);
                    $container.append(label).append('<br>');
                });

                if (autoResponsive) {
                    applyResponsiveColumns();
                }
            };

            $(window).on('resize', function () {
                if (autoResponsive) applyResponsiveColumns();
            });

            // Don't init BT here - parseFPPSystems will init after rows are populated

            // Update display order button visibility
            updateDisplayOrderButtons();

            // Initialize Bootstrap Popover with Column Selector inside
            var popover = new bootstrap.Popover(document.getElementById("columnSelectorBtn"), {
                html: true,
                content: $('#columnSelector')
            });

            // Prevent popover from closing when clicking inside
            document.getElementById("columnSelector").addEventListener("click", function (event) {
                event.stopPropagation(); // Stops click from propagating to document
            });

            // Close popover when clicking outside
            document.addEventListener("click", function (event) {
                var popoverEl = document.getElementById("columnSelectorBtn");
                if (!popoverEl.contains(event.target) && !document.querySelector(".popover")?.contains(event.target)) {
                    popover.hide();
                }
            });


            let mouseX = 0;
            let mouseY = 0;

            // Update mouse position on movement
            document.addEventListener("mousemove", function (event) {
                mouseX = event.pageX;
                mouseY = event.pageY;
            });

            // Run tooltip cleanup every 2 seconds
            setInterval(() => {
                $(".tooltip").each(function () {
                    const tooltip = $(this);
                    const offset = tooltip.offset();
                    const width = tooltip.outerWidth();
                    const height = tooltip.outerHeight();

                    // Expand boundaries
                    const leftBoundary = offset.left - (width * 0.2);  // Allow 20% leeway on left side
                    const rightBoundary = offset.left + width + (width * 0.15); // Allow 15% leeway on right side

                    // Check if mouse is inside tooltip bounds
                    if (
                        mouseX < leftBoundary || // Adjusted left boundary
                        mouseX > rightBoundary || // Adjusted right boundary

                        mouseY < offset.top ||
                        mouseY > offset.top + height

                    ) {
                        tooltip.remove(); // Remove tooltip if mouse is outside
                    }
                });
            }, 3000);

            // Event handler for remoteCheckbox changes to validate platform selection
            $(document).on('change', 'input.remoteCheckbox', function () {
                validateOSUpgrade();
            });

            autoRefreshToggled();

        });




    </script>
</body>

</html>