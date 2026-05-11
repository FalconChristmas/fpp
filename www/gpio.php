<!DOCTYPE html>
<html lang="en">
<?php

// Check for cape-inputs.json file to determine which GPIO pins are already in use
$capeInputsFile = $mediaDirectory . '/tmp/cape-inputs.json';
$usedGpioPins = array();
if (file_exists($capeInputsFile)) {
    $capeInputsData = file_get_contents($capeInputsFile);
    $capeInputsJson = json_decode($capeInputsData, true);
    if (isset($capeInputsJson['inputs']) && is_array($capeInputsJson['inputs'])) {
        foreach ($capeInputsJson['inputs'] as $input) {
            if (isset($input['pin']) && isset($input['type'])) {
                $usedGpioPins[$input['pin']] = $input['type'];
            }
        }
    }
}

// Check for cape channel output configuration to block those GPIO pins as well
$stringsDir = $mediaDirectory . '/tmp/strings/';
if (is_dir($stringsDir)) {
    $stringFiles = scandir($stringsDir);
    foreach ($stringFiles as $file) {
        if (substr($file, 0, 1) != '.' && substr($file, -5) == '.json') {
            $stringFilePath = $stringsDir . $file;
            $stringData = file_get_contents($stringFilePath);
            $stringJson = json_decode($stringData, true);
            if (isset($stringJson['outputs']) && is_array($stringJson['outputs'])) {
                $portNumber = 1;
                foreach ($stringJson['outputs'] as $output) {
                    if (isset($output['pin'])) {
                        $pin = $output['pin'];
                        $driverType = isset($stringJson['driver']) ? $stringJson['driver'] : 'Channel Output';
                        $portInfo = $driverType . ' (Port ' . $portNumber . ')';
                        if (isset($output['description']) && !empty($output['description'])) {
                            $portInfo .= ' - ' . $output['description'];
                        }
                        if (!isset($usedGpioPins[$pin])) {
                            $usedGpioPins[$pin] = $portInfo;
                        }
                    }
                    $portNumber++;
                }
            }
        }
    }
}
?>

<head>
    <?php
    include 'common/htmlMeta.inc';
    require_once 'config.php';
    require_once "common.php";
    include 'common/menuHead.inc';

    // Generates an FPP-standard help icon with a Bootstrap tooltip.
    // $tip may contain HTML (e.g. <br>, <strong>) for formatted tips.
    function helpTip($tip) {
        $t = htmlspecialchars($tip, ENT_QUOTES | ENT_HTML5, 'UTF-8');
        return '<span data-bs-toggle="tooltip" data-bs-html="true" data-bs-placement="auto"'
             . ' data-bs-container="body" data-bs-title="' . $t . '">'
             . '<img src="images/redesign/help-icon.svg" class="icon-help" alt="help icon">'
             . '</span>';
    }

    $gpioData = file_get_contents('http://127.0.0.1:32322/gpio');
    $gpiojson = json_decode($gpioData, true);

    $includeFilters = array();
    $excludeFilters = array();
    if (!isset($settings["showAllOptions"]) || $settings["showAllOptions"] == 0) {
        if (isset($settings['cape-info']['show']) && isset($settings['cape-info']['show']['gpio'])) {
            $includeFilters = $settings['cape-info']['show']['gpio'];
        }
        if (isset($settings['cape-info']['hide']) && isset($settings['cape-info']['hide']['gpio'])) {
            $excludeFilters = $settings['cape-info']['hide']['gpio'];
        }
    }

    $availablePins = array();
    foreach ($gpiojson as $gpio) {
        $pinName = $gpio['pin'];
        if (isset($usedGpioPins[$pinName]))
            continue;
        if (count($includeFilters) > 0) {
            $matched = false;
            foreach ($includeFilters as $f) {
                if (preg_match("/" . $f . "/", $pinName)) {
                    $matched = true;
                    break;
                }
            }
            if (!$matched)
                continue;
        }
        $skip = false;
        foreach ($excludeFilters as $f) {
            if (preg_match("/" . $f . "/", $pinName)) {
                $skip = true;
                break;
            }
        }
        if (!$skip)
            $availablePins[] = $gpio;
    }
    ?>

    <style>
        /* ── GPIO trigger cards ─────────────────────────────────────────────────────── */
        .gpio-trigger-card {
            border: 1px solid var(--bs-border-color);
            border-radius: var(--fpp-radius-lg);
            margin-bottom: var(--fpp-sp-md);
            background: var(--fpp-bg-card);
            box-shadow: var(--fpp-shadow-sm);
            transition: box-shadow var(--fpp-transition-fast);
        }

        .gpio-trigger-card:hover {
            box-shadow: var(--fpp-shadow-md);
        }

        .gpio-trigger-card .card-header {
            background: var(--bs-secondary-bg);
            border-bottom: 1px solid var(--bs-border-color);
            border-radius: var(--fpp-radius-lg) var(--fpp-radius-lg) 0 0;
            padding: 0.6rem 1rem;
            display: flex;
            flex-wrap: wrap;
            align-items: center;
            gap: 0.5rem;
        }

        .gpio-trigger-card .card-body {
            padding: 0.75rem 1rem;
        }

        /* ── Command list ─────────────────────────────────────────────────────────── */
        .gpio-cmd-list {
            list-style: none;
            padding: 0;
            margin: 0;
        }

        .gpio-cmd-list li {
            display: flex;
            align-items: center;
            gap: 0.4rem;
            padding: 0.2rem 0;
            border-bottom: 1px solid var(--bs-border-color);
        }

        .gpio-cmd-list li:last-child {
            border-bottom: none;
        }

        .gpio-cmd-badge {
            flex: 1;
            font-family: var(--fpp-font-mono);
            font-size: var(--fpp-fs-sm);
            background: var(--bs-tertiary-bg);
            border-radius: var(--fpp-radius-sm);
            padding: 0.15rem 0.5rem;
            word-break: break-all;
            color: var(--bs-body-color);
        }

        .gpio-section-label {
            font-weight: var(--fpp-fw-semibold);
            font-size: var(--fpp-fs-xs);
            text-transform: uppercase;
            letter-spacing: 0.05em;
            color: var(--fpp-text-muted);
            margin-bottom: 0.35rem;
            margin-top: 0.6rem;
        }

        .gpio-section-label:first-child {
            margin-top: 0;
        }

        .gpio-empty-text {
            color: var(--fpp-text-muted);
            font-style: italic;
            font-size: var(--fpp-fs-sm);
        }

        /* ── Modal command rows ──────────────────────────────────────────────────── */
        .gpio-cmd-row {
            display: flex;
            align-items: center;
            gap: 0.4rem;
            padding: 0.4rem 0.6rem;
            background: var(--bs-body-bg);
            border: 1px solid var(--bs-border-color);
            border-radius: var(--fpp-radius-sm);
            margin-bottom: 0.4rem;
        }

        .gpio-cmd-row .cmd-label {
            flex: 1;
            font-family: var(--fpp-font-mono);
            font-size: var(--fpp-fs-sm);
            color: var(--bs-body-color);
            word-break: break-all;
        }

        /* ── Modal section grouping ─────────────────────────────────────────────── */
        .gpio-modal-section {
            background: var(--bs-secondary-bg);
            border: 1px solid var(--bs-border-color);
            border-radius: var(--fpp-radius-md);
            padding: 0.85rem 1rem;
            margin-bottom: 1rem;
        }

        .gpio-modal-section-title {
            font-weight: var(--fpp-fw-semibold);
            margin-bottom: 0.65rem;
            display: flex;
            align-items: center;
            gap: 0.4rem;
            color: var(--bs-body-color);
        }

        .gpio-modal-section-title .section-subtitle {
            font-weight: var(--fpp-fw-normal);
            font-size: var(--fpp-fs-sm);
            color: var(--fpp-text-muted);
        }

        /* ── Command group wrapper ───────────────────────────────────────────────── */
        .gpio-cmd-group {
            border: 2px solid var(--bs-primary);
            border-radius: var(--fpp-radius-lg);
            margin-bottom: 1rem;
            overflow: hidden;
        }

        .gpio-cmd-group-header {
            background: rgba(var(--bs-primary-rgb), 0.10);
            border-bottom: 2px solid var(--bs-primary);
            padding: 0.6rem 1rem;
            font-weight: var(--fpp-fw-semibold);
            color: var(--bs-primary);
            display: flex;
            align-items: center;
            gap: 0.5rem;
        }

        .gpio-cmd-group-header .section-subtitle {
            font-weight: var(--fpp-fw-normal);
            font-size: var(--fpp-fs-sm);
            color: var(--fpp-text-muted);
        }

        .gpio-cmd-group .gpio-modal-section {
            margin-bottom: 0;
            border-left: none;
            border-right: none;
            border-radius: 0;
        }

        .gpio-cmd-group .gpio-modal-section:last-child {
            border-bottom: none;
        }

        .gpio-cmd-group .gpio-modal-section+.gpio-modal-section {
            border-top: 1px solid var(--bs-border-color);
        }
    </style>

    <script>
        allowMultisyncCommands = true;

        var gpioPinData = <?php echo json_encode($gpiojson, JSON_PRETTY_PRINT); ?>;
        var usedGpioPins = <?php
        $usedOut = array();
        foreach ($usedGpioPins as $pin => $usage) {
            $usedOut[$pin] = $usage;
        }
        echo json_encode($usedOut);
        ?>;
        var availablePins = <?php echo json_encode(array_map(fn($g) => $g['pin'], $availablePins)); ?>;

        extraCommands = [{
            "name": "OLED Navigation",
            "args": [{
                "description": "Action", "name": "Action", "optional": false, "type": "string",
                "contents": ["Up", "Down", "Back", "Enter", "Test", "Test/Down", "Test Multisync"]
            }],
            "supportsMulticast": false
        }];

        // ─── State ────────────────────────────────────────────────────────────────────
        var gpioTriggers = [];
        var currentEditIdx = -1;

        var modalRising = [];
        var modalFalling = [];
        var modalHold = [];
        var cmdTarget = null;    // 'rising'|'falling'|'hold'
        var cmdListIdx = -1;      // -1 = add new

        // ─── Normalisation ────────────────────────────────────────────────────────────
        function normalizeActions(v) {
            if (!v) return [];
            if (Array.isArray(v)) return v.filter(function (c) { return c && c.command; });
            if (typeof v === 'object' && v.command) return [v];
            return [];
        }

        function normalizeTrigger(t) {
            return {
                pin: t.pin || '',
                enabled: t.enabled !== false,
                mode: t.mode || 'gpio',
                desc: t.desc || '',
                debounceTime: t.debounceTime || 100,
                debounceEdge: t.debounceEdge || 'both',
                rising: normalizeActions(t.rising),
                falling: normalizeActions(t.falling),
                holdTime: t.holdTime || 0,
                hold: normalizeActions(t.hold),
                // LED fields — backward compatible defaults
                ledPin: t.ledPin || '',
                ledActiveHigh: t.ledActiveHigh !== false,
                ledIdleMode: t.ledIdleMode || 'off',
                ledPulseRateMs: t.ledPulseRateMs || 500,
                ledTriggerMode: t.ledTriggerMode || 'follow_input',
                ledTriggerParam: t.ledTriggerParam || 0,
                // Re-enable fields — backward compatible defaults
                reEnableMode: t.reEnableMode || 'always',
                reEnableDelay: t.reEnableDelay || 0
            };
        }

        // ─── Load / Save ──────────────────────────────────────────────────────────────
        function loadGPIOConfig() {
            $.get('api/configfile/gpio.json', function (data) {
                gpioTriggers = (Array.isArray(data) ? data : []).map(normalizeTrigger);
                renderTriggerList();
            }).fail(function () { gpioTriggers = []; renderTriggerList(); });
        }

        function saveGPIOInputs() {
            $.ajax({
                url: 'api/configfile/gpio.json', type: 'POST',
                contentType: 'application/json',
                data: JSON.stringify(gpioTriggers, null, 4),
                dataType: 'json',
                success: function (data) {
                    if (data.Status === 'OK') {
                        $.jGrowl('GPIO Triggers Saved.', { themeState: 'success' });
                        SetRestartFlag(2);
                        common_ViewPortChange();
                    } else {
                        alert('ERROR: ' + data.Message);
                    }
                },
                error: function () { alert('Error: Failed to save GPIO triggers'); }
            });
        }

        // ─── Main list rendering ──────────────────────────────────────────────────────
        function renderTriggerList() {
            var c = $('#gpioTriggerList').empty();
            if (!gpioTriggers.length) {
                c.html('<div class="text-muted fst-italic p-3">No GPIO triggers configured. Click <strong>Add GPIO Trigger</strong> to get started.</div>');
                return;
            }
            $.each(gpioTriggers, function (i, t) { c.append(buildTriggerCard(i, t)); });
        }

        function buildTriggerCard(idx, t) {
            var pinInfo = getPinInfo(t.pin);
            var gpioLbl = pinInfo ? ' <span class="text-muted small">(' + esc(pinInfo.gpioChip) + '/' + esc(pinInfo.gpioLine) + ')</span>' : '';
            var enBadge = t.enabled
                ? '<span class="badge bg-success">Enabled</span>'
                : '<span class="badge bg-secondary">Disabled</span>';
            var holdBadge = (t.holdTime > 0 && t.hold.length)
                ? '<span class="badge bg-info text-dark" title="Long-press hold">Hold ' + t.holdTime + 'ms</span>' : '';
            var ledBadge = t.ledPin
                ? '<span class="badge bg-warning text-dark"><i class="fas fa-lightbulb"></i> ' + esc(t.ledPin) + '</span>' : '';
            var reEnableBadge = '';
            if (t.reEnableMode === 'timed') {
                reEnableBadge = '<span class="badge bg-secondary" title="Re-enable after delay"><i class="fas fa-clock"></i> ' + t.reEnableDelay + 'ms</span>';
            } else if (t.reEnableMode === 'player_idle') {
                reEnableBadge = '<span class="badge bg-secondary" title="Re-enable on player idle"><i class="fas fa-play-circle"></i> On idle</span>';
            }
            var modeLabel = { gpio: 'No pull', gpio_pu: 'Pull up', gpio_pd: 'Pull down' }[t.mode] || t.mode;

            var card = $('<div class="gpio-trigger-card"></div>');
            card.append(
                '<div class="card-header">' +
                enBadge +
                '<strong>' + esc(t.pin) + '</strong>' + gpioLbl +
                (t.desc ? '<span class="text-muted">' + esc(t.desc) + '</span>' : '') +
                holdBadge + ledBadge + reEnableBadge +
                '<span class="text-muted small ms-auto">' + esc(modeLabel) + ' · ' + t.debounceTime + 'ms debounce</span>' +
                '<button class="btn btn-sm btn-outline-primary ms-2" onclick="openEditModal(' + idx + ')"><i class="fas fa-edit"></i> Edit</button>' +
                '<button class="btn btn-sm btn-outline-danger ms-1" onclick="deleteTrigger(' + idx + ')"><i class="fas fa-trash"></i></button>' +
                '</div>'
            );

            var risingHtml = previewCmds(t.rising);
            var fallingHtml = previewCmds(t.falling);
            var holdHtml = '';
            if (t.holdTime > 0 && t.hold.length) {
                holdHtml = '<div class="mt-2 pt-2" style="border-top:1px solid var(--bs-border-color)">' +
                    '<div class="gpio-section-label"><i class="fas fa-hand-paper text-info"></i> Hold (' + t.holdTime + 'ms)</div>' +
                    previewCmds(t.hold) + '</div>';
            }

            card.append(
                '<div class="card-body">' +
                '<div class="row">' +
                '<div class="col-md-6">' +
                '<div class="gpio-section-label"><i class="fas fa-arrow-up text-success"></i> Rising</div>' +
                risingHtml +
                '</div>' +
                '<div class="col-md-6">' +
                '<div class="gpio-section-label"><i class="fas fa-arrow-down text-danger"></i> Falling</div>' +
                fallingHtml +
                '</div>' +
                '</div>' + holdHtml +
                '</div>'
            );
            return card;
        }

        function previewCmds(cmds) {
            if (!cmds || !cmds.length) return '<div class="gpio-empty-text">No commands</div>';
            var html = '<ul class="gpio-cmd-list">';
            $.each(cmds, function (i, c) {
                html += '<li><span class="gpio-cmd-badge">' + esc(cmdSummary(c)) + '</span></li>';
            });
            return html + '</ul>';
        }

        function cmdSummary(cmd) {
            if (!cmd || !cmd.command) return '(none)';
            var s = cmd.command;
            if (cmd.args && cmd.args.length) s += ': ' + cmd.args.map(function (a) { return a !== '' ? a : '—'; }).join(', ');
            return s;
        }

        function getPinInfo(n) {
            for (var i = 0; i < gpioPinData.length; i++) if (gpioPinData[i].pin === n) return gpioPinData[i];
            return null;
        }

        function esc(s) { return $('<div>').text(String(s)).html(); }

        // ─── Delete ───────────────────────────────────────────────────────────────────
        function deleteTrigger(idx) {
            if (!confirm('Remove trigger for ' + gpioTriggers[idx].pin + '?')) return;
            gpioTriggers.splice(idx, 1);
            renderTriggerList();
        }

        // ─── Open modal ───────────────────────────────────────────────────────────────
        function openAddModal() {
            currentEditIdx = -1;
            var configured = gpioTriggers.map(function (t) { return t.pin; });
            var free = '';
            for (var i = 0; i < availablePins.length; i++) {
                if (configured.indexOf(availablePins[i]) === -1) { free = availablePins[i]; break; }
            }
            openEditModalWith(normalizeTrigger({ pin: free }));
        }

        function openEditModal(idx) {
            currentEditIdx = idx;
            openEditModalWith(gpioTriggers[idx]);
        }

        function openEditModalWith(t) {
            modalRising = t.rising.map(function (c) { return $.extend(true, {}, c); });
            modalFalling = t.falling.map(function (c) { return $.extend(true, {}, c); });
            modalHold = t.hold.map(function (c) { return $.extend(true, {}, c); });

            // Pin selector
            var configured = gpioTriggers.map(function (x) { return x.pin; });
            var pinSel = $('#gpioModalPin').empty();
            $.each(availablePins, function (i, p) {
                if (configured.indexOf(p) === -1 || p === t.pin)
                    pinSel.append('<option value="' + esc(p) + '"' + (p === t.pin ? ' selected' : '') + '>' + esc(p) + '</option>');
            });

            $('#gpioModalEnabled').prop('checked', t.enabled);
            $('#gpioModalMode').val(t.mode);
            $('#gpioModalDesc').val(t.desc);
            $('#gpioModalDebounce').val(t.debounceTime);
            $('#gpioModalDebounceEdge').val(t.debounceEdge);
            $('#gpioModalHoldTime').val(t.holdTime);

            // LED fields
            var ledSel = $('#gpioModalLedPin').empty().append('<option value="">None</option>');
            $.each(availablePins, function (i, p) {
                ledSel.append('<option value="' + esc(p) + '"' + (p === t.ledPin ? ' selected' : '') + '>' + esc(p) + '</option>');
            });
            $('#gpioModalLedActiveHigh').val(t.ledActiveHigh ? 'true' : 'false');
            $('#gpioModalLedIdleMode').val(t.ledIdleMode);
            $('#gpioModalLedPulseRate').val(t.ledPulseRateMs);
            $('#gpioModalLedTriggerMode').val(t.ledTriggerMode);
            $('#gpioModalLedTriggerParam').val(t.ledTriggerParam);

            // Re-enable fields
            $('#gpioModalReEnableMode').val(t.reEnableMode);
            $('#gpioModalReEnableDelay').val(t.reEnableDelay || 5000);
            updateReEnableVisibility();

            updateLEDVisibility();
            updatePinModeOptions(t.pin);
            $('#gpioModalPin').off('change').on('change', function () { updatePinModeOptions($(this).val()); });

            renderModalList('rising');
            renderModalList('falling');
            renderModalList('hold');
            toggleHoldSection();

            $('#gpioEditModalLabel').text(currentEditIdx === -1 ? 'Add GPIO Trigger' : 'Edit GPIO Trigger — ' + t.pin);
            $('#gpioEditModal').modal('show');
        }

        function updatePinModeOptions(pinName) {
            var info = getPinInfo(pinName);
            var sel = $('#gpioModalMode'), cur = sel.val();
            sel.empty().append('<option value="gpio">None / External Pull</option>');
            if (info && info.supportsPullUp) sel.append('<option value="gpio_pu">Pull Up</option>');
            if (info && info.supportsPullDown) sel.append('<option value="gpio_pd">Pull Down</option>');
            sel.val(cur) || sel.val('gpio');
        }

        function toggleHoldSection() {
            var t = parseInt($('#gpioModalHoldTime').val()) || 0;
            $('#gpioModalHoldSection').toggle(t > 0);
        }

        function updateReEnableVisibility() {
            var mode = $('#gpioModalReEnableMode').val();
            $('#gpioReEnableDelayRow').toggle(mode === 'timed');
            $('#gpioReEnableInfo').toggle(mode !== 'always');
        }

        function updateLEDVisibility() {
            var hasPin = $('#gpioModalLedPin').val() !== '';
            var idleMode = $('#gpioModalLedIdleMode').val();
            var trigMode = $('#gpioModalLedTriggerMode').val();

            $('#gpioLEDSettingsRow').toggle(hasPin);
            $('#gpioLEDPulseRateRow').toggle(hasPin && idleMode === 'pulse');

            var showParam = hasPin && (trigMode === 'flash' || trigMode === 'timed_on');
            $('#gpioLEDTriggerParamRow').toggle(showParam);

            var labelText, helpText, tipText;
            if (trigMode === 'flash') {
                labelText = 'Number of flashes';
                helpText  = 'How many times the LED flashes before returning to idle.';
                tipText   = 'How many times the LED flashes after the button is pressed. Each flash is 150 ms on followed by 150 ms off.';
            } else {
                labelText = 'On duration (ms)';
                helpText  = 'How long (ms) the LED stays on before returning to idle.';
                tipText   = 'How long in milliseconds the LED stays on after the button is pressed before returning to idle mode.';
            }
            // Update label text node without touching the tooltip span child
            $('#gpioLEDTriggerParamLabel').contents().filter(function () {
                return this.nodeType === 3;
            }).first().replaceWith(labelText + ' ');
            $('#gpioLEDTriggerParamHelp').text(helpText);

            // Refresh the dynamic tooltip
            var tipEl = document.getElementById('gpioLEDTriggerParamTip');
            if (tipEl) {
                var existing = bootstrap.Tooltip.getInstance(tipEl);
                if (existing) existing.dispose();
                tipEl.setAttribute('data-bs-title', tipText);
                new bootstrap.Tooltip(tipEl);
            }
        }

        // ─── Save modal ───────────────────────────────────────────────────────────────
        function saveGPIOModal() {
            var pin = $('#gpioModalPin').val();
            if (!pin) { alert('Please select a GPIO pin.'); return; }
            if (currentEditIdx === -1) {
                for (var i = 0; i < gpioTriggers.length; i++) {
                    if (gpioTriggers[i].pin === pin) {
                        alert('A trigger for pin ' + pin + ' already exists. Edit that entry instead.');
                        return;
                    }
                }
            }
            var t = {
                pin: pin,
                enabled: $('#gpioModalEnabled').is(':checked'),
                mode: $('#gpioModalMode').val(),
                desc: $('#gpioModalDesc').val(),
                debounceTime: parseInt($('#gpioModalDebounce').val()) || 100,
                debounceEdge: $('#gpioModalDebounceEdge').val(),
                rising: modalRising.slice(),
                falling: modalFalling.slice(),
                holdTime: parseInt($('#gpioModalHoldTime').val()) || 0,
                hold: modalHold.slice(),
                ledPin: $('#gpioModalLedPin').val(),
                ledActiveHigh: $('#gpioModalLedActiveHigh').val() === 'true',
                ledIdleMode: $('#gpioModalLedIdleMode').val(),
                ledPulseRateMs: parseInt($('#gpioModalLedPulseRate').val()) || 500,
                ledTriggerMode: $('#gpioModalLedTriggerMode').val(),
                ledTriggerParam: parseInt($('#gpioModalLedTriggerParam').val()) || 0,
                reEnableMode: $('#gpioModalReEnableMode').val(),
                reEnableDelay: parseInt($('#gpioModalReEnableDelay').val()) || 0
            };
            if (currentEditIdx === -1) gpioTriggers.push(t);
            else gpioTriggers[currentEditIdx] = t;
            $('#gpioEditModal').modal('hide');
            renderTriggerList();
        }

        // ─── Command list inside modal ─────────────────────────────────────────────────
        function getModalList(name) {
            return name === 'rising' ? modalRising : name === 'falling' ? modalFalling : modalHold;
        }

        function renderModalList(name) {
            var list = getModalList(name);
            var c = $('#gpioModal_' + name + '_list').empty();
            if (!list.length) {
                c.append('<div class="gpio-empty-text mb-2">No commands — click Add to configure.</div>');
                return;
            }
            $.each(list, function (i, cmd) {
                var row = $('<div class="gpio-cmd-row"></div>');
                var label = $('<span class="cmd-label"></span>').text(cmdSummary(cmd));
                var edit = $('<button class="btn btn-sm btn-outline-secondary" title="Edit"><i class="fas fa-pencil-alt"></i></button>');
                var up = $('<button class="btn btn-sm btn-outline-secondary" title="Move up"' + (i === 0 ? ' disabled' : '') + '><i class="fas fa-arrow-up"></i></button>');
                var dn = $('<button class="btn btn-sm btn-outline-secondary" title="Move down"' + (i === list.length - 1 ? ' disabled' : '') + '><i class="fas fa-arrow-down"></i></button>');
                var del = $('<button class="btn btn-sm btn-outline-danger" title="Remove"><i class="fas fa-trash"></i></button>');
                (function (n, idx) {
                    edit.on('click', function () { editModalCmd(n, idx); });
                    up.on('click', function () { moveModalCmd(n, idx, -1); });
                    dn.on('click', function () { moveModalCmd(n, idx, +1); });
                    del.on('click', function () { removeModalCmd(n, idx); });
                })(name, i);
                row.append(label, edit, up, dn, del);
                c.append(row);
            });
        }

        function addModalCmd(name) {
            cmdTarget = name; cmdListIdx = -1;
            ShowCommandEditor('gpio_modal_' + name + '_new', {}, 'gpioCommandSaved', '', {
                title: 'Add Command — ' + name.charAt(0).toUpperCase() + name.slice(1) + ' Edge',
                saveButton: 'Add Command', cancelButton: 'Cancel', showPresetSelect: true
            });
        }

        function editModalCmd(name, idx) {
            cmdTarget = name; cmdListIdx = idx;
            ShowCommandEditor('gpio_modal_' + name + '_' + idx, getModalList(name)[idx], 'gpioCommandSaved', '', {
                title: 'Edit Command — ' + name.charAt(0).toUpperCase() + name.slice(1) + ' Edge',
                saveButton: 'Save Command', cancelButton: 'Cancel', showPresetSelect: true
            });
        }

        function gpioCommandSaved(target, data) {
            if (!data || !data.command) return;
            var list = getModalList(cmdTarget);
            if (cmdListIdx === -1) list.push(data);
            else list[cmdListIdx] = data;
            renderModalList(cmdTarget);
        }

        function removeModalCmd(name, idx) {
            getModalList(name).splice(idx, 1);
            renderModalList(name);
        }

        function moveModalCmd(name, idx, dir) {
            var list = getModalList(name), t2 = idx + dir;
            if (t2 < 0 || t2 >= list.length) return;
            var tmp = list[idx]; list[idx] = list[t2]; list[t2] = tmp;
            renderModalList(name);
        }

        $(document).ready(function () {
            PopulateCommandListCache();
            loadGPIOConfig();
        });
    </script>

    <title><?php echo $pageTitle; ?></title>
</head>

<body>
    <div id="bodyWrapper">
        <?php
        $activeParentMenuItem = 'input-output';
        include 'menu.inc';
        ?>

        <div class="container-fluid">
            <h1 class="title">GPIO Input Triggers</h1>
            <div class="pageContent">

                <div class="row tablePageHeader mb-3">
                    <div class="col-md">
                        <h2>GPIO Input Triggers</h2>
                    </div>
                    <div class="col-md-auto ms-lg-auto d-flex gap-2 align-items-center flex-wrap">
                        <button class="btn btn-outline-primary" onclick="openAddModal()">
                            <i class="fas fa-plus"></i> Add GPIO Trigger
                        </button>
                        <input type="button" value="Save" class="buttons btn-success" onclick="saveGPIOInputs()">
                    </div>
                </div>

                <?php if (!empty($usedGpioPins)): ?>
                    <div class="alert alert-info py-2 mb-3">
                        <i class="fas fa-info-circle"></i>
                        <strong><?php echo count($usedGpioPins); ?> pin(s)</strong> reserved by cape:
                        <span class="text-muted"><?php echo implode(', ', array_keys($usedGpioPins)); ?></span>
                    </div>
                <?php endif; ?>

                <div id="gpioTriggerList">
                    <div class="text-muted fst-italic p-3">Loading GPIO configuration…</div>
                </div>

            </div>
        </div>

        <!-- ═══ GPIO Edit Modal ═══════════════════════════════════════════════ -->
        <div class="modal fade" id="gpioEditModal" tabindex="-1" aria-labelledby="gpioEditModalLabel"
            aria-hidden="true">
            <div class="modal-dialog modal-xl modal-dialog-scrollable">
                <div class="modal-content">

                    <div class="modal-header">
                        <h5 class="modal-title" id="gpioEditModalLabel">Configure GPIO Trigger</h5>
                        <button type="button" class="btn-close" data-bs-dismiss="modal"></button>
                    </div>

                    <div class="modal-body">

                        <!-- Pin & General ──────────────────────────────────── -->
                        <div class="gpio-modal-section">
                            <div class="gpio-modal-section-title">
                                <i class="fas fa-microchip"></i> Pin &amp; General Settings
                            </div>
                            <div class="row g-3">
                                <div class="col-md-3">
                                    <label class="form-label fw-semibold">GPIO Pin <?= helpTip('The GPIO input pin to monitor for signal changes. Only pins not reserved by capes or channel outputs are listed.') ?></label>
                                    <select class="form-select" id="gpioModalPin"></select>
                                </div>
                                <div class="col-md-1 d-flex align-items-end pb-1">
                                    <div class="form-check">
                                        <input class="form-check-input" type="checkbox" id="gpioModalEnabled">
                                        <label class="form-check-label" for="gpioModalEnabled">Enabled <?= helpTip('When unchecked this trigger is loaded but never fires, letting you disable it temporarily without losing its configuration.') ?></label>
                                    </div>
                                </div>
                                <div class="col-md-4">
                                    <label class="form-label fw-semibold">Description <?= helpTip('Optional label shown on the trigger card. Useful for identifying buttons by function, e.g. <em>Start show</em> or <em>Emergency stop</em>.') ?></label>
                                    <input class="form-control" type="text" id="gpioModalDesc" maxlength="128"
                                        placeholder="e.g. Start button">
                                </div>
                                <div class="col-md-4">
                                    <label class="form-label fw-semibold">Pull Up / Down <?= helpTip('Configures the internal resistor on the pin.<br><strong>None</strong> – relies on an external pull resistor.<br><strong>Pull Up</strong> – pin reads HIGH at rest; pressing a button to GND produces a falling edge.<br><strong>Pull Down</strong> – pin reads LOW at rest; pressing a button to 3.3 V produces a rising edge.') ?></label>
                                    <select class="form-select" id="gpioModalMode">
                                        <option value="gpio">None / External Pull</option>
                                        <option value="gpio_pu">Pull Up</option>
                                        <option value="gpio_pd">Pull Down</option>
                                    </select>
                                </div>
                            </div>
                            <div class="row g-3 mt-1">
                                <div class="col-md-3">
                                    <label class="form-label fw-semibold">Debounce Time (ms) <?= helpTip('Mechanical switches briefly bounce between open and closed. FPP ignores transitions shorter than this window to prevent false triggers. 50–200 ms is typical for push buttons.') ?></label>
                                    <input class="form-control" type="number" id="gpioModalDebounce" min="10"
                                        max="60000" value="100">
                                </div>
                                <div class="col-md-3">
                                    <label class="form-label fw-semibold">Debounce On <?= helpTip('Which edges the debounce window applies to.<br><strong>Both</strong> – debounce rising and falling edges.<br><strong>Rising only</strong> – debounce the press; release fires immediately.<br><strong>Falling only</strong> – debounce the release; press fires immediately.') ?></label>
                                    <select class="form-select" id="gpioModalDebounceEdge">
                                        <option value="both">Both edges</option>
                                        <option value="rising">Rising only</option>
                                        <option value="falling">Falling only</option>
                                    </select>
                                </div>
                            </div>
                        </div>

                        <!-- ── Trigger Commands group ───────────────────── -->
                        <div class="gpio-cmd-group">
                            <div class="gpio-cmd-group-header">
                                <i class="fas fa-terminal"></i> Trigger Commands
                                <span class="section-subtitle">Commands to execute when the GPIO input fires</span>
                            </div>

                            <!-- Rising commands -->
                            <div class="gpio-modal-section">
                                <div class="gpio-modal-section-title">
                                    <i class="fas fa-arrow-up text-success"></i> Rising Edge Commands
                                    <span class="section-subtitle">(button pressed / signal HIGH)</span>
                                    <?= helpTip('Commands run when the GPIO signal transitions LOW → HIGH. With a pull-up circuit this is button release; with a pull-down circuit this is button press.') ?>
                                </div>
                                <div id="gpioModal_rising_list" class="mb-2"></div>
                                <button class="btn btn-sm btn-outline-success" onclick="addModalCmd('rising')">
                                    <i class="fas fa-plus"></i> Add Command
                                </button>
                            </div>

                            <!-- Falling commands -->
                            <div class="gpio-modal-section">
                                <div class="gpio-modal-section-title">
                                    <i class="fas fa-arrow-down text-danger"></i> Falling Edge Commands
                                    <span class="section-subtitle">(button released / signal LOW)</span>
                                    <?= helpTip('Commands run when the GPIO signal transitions HIGH → LOW. With a pull-up circuit this is button press; with a pull-down circuit this is button release.<br>Not fired if a Hold command triggered during that press.') ?>
                                </div>
                                <p class="small text-muted mb-2">
                                    <i class="fas fa-info-circle"></i>
                                    Falling commands are suppressed when a Hold command fires for that press.
                                </p>
                                <div id="gpioModal_falling_list" class="mb-2"></div>
                                <button class="btn btn-sm btn-outline-danger" onclick="addModalCmd('falling')">
                                    <i class="fas fa-plus"></i> Add Command
                                </button>
                            </div>

                            <!-- Hold / long-press -->
                            <div class="gpio-modal-section">
                                <div class="gpio-modal-section-title">
                                    <i class="fas fa-hand-paper text-info"></i> Long-Press / Hold Commands
                                    <span class="section-subtitle">(optional)</span>
                                    <?= helpTip('Commands that fire only when the button is held down for the configured duration. When hold fires, the falling-edge commands for that press are suppressed to prevent double-actions.') ?>
                                </div>
                                <div class="row g-3 mb-3">
                                    <div class="col-md-4">
                                        <label class="form-label fw-semibold">Hold Time (ms) <?= helpTip('How long in milliseconds the button must be continuously held before the hold commands fire. Set to 0 to disable hold detection entirely.') ?></label>
                                        <input class="form-control" type="number" id="gpioModalHoldTime" min="0"
                                            max="30000" value="0" placeholder="0 = disabled"
                                            oninput="toggleHoldSection()">
                                        <div class="form-text">0 = disabled. Falling commands suppressed when hold
                                            fires.</div>
                                    </div>
                                </div>
                                <div id="gpioModalHoldSection" style="display:none">
                                    <div id="gpioModal_hold_list" class="mb-2"></div>
                                    <button class="btn btn-sm btn-outline-info" onclick="addModalCmd('hold')">
                                        <i class="fas fa-plus"></i> Add Hold Command
                                    </button>
                                </div>
                            </div>

                        </div><!-- /.gpio-cmd-group -->

                        <!-- Re-enable Behavior ──────────────────────────────── -->
                        <div class="gpio-modal-section">
                            <div class="gpio-modal-section-title">
                                <i class="fas fa-lock-open text-secondary"></i> Re-enable GPIO After Trigger
                                <span class="section-subtitle">(optional — prevents rapid re-triggering)</span>
                            </div>
                            <div class="row g-3">
                                <div class="col-md-5">
                                    <label class="form-label fw-semibold">Re-enable Mode <?= helpTip('Controls when the input accepts another press after triggering.<br><strong>Always enabled</strong> – no delay; immediate re-triggering is allowed.<br><strong>Fixed delay</strong> – input is locked for the specified number of milliseconds after each trigger.<br><strong>Player idle</strong> – input stays locked until FPP finishes playback. If no playback starts within 2 seconds, the input re-enables automatically.') ?></label>
                                    <select class="form-select" id="gpioModalReEnableMode"
                                        onchange="updateReEnableVisibility()">
                                        <option value="always">Always enabled (no suppression)</option>
                                        <option value="timed">Re-enable after a fixed delay</option>
                                        <option value="player_idle">Re-enable when player becomes idle</option>
                                    </select>
                                </div>
                                <div class="col-md-3" id="gpioReEnableDelayRow" style="display:none">
                                    <label class="form-label fw-semibold">Delay (ms) <?= helpTip('How many milliseconds the input is suppressed after triggering before it will accept another press. Minimum 100 ms.') ?></label>
                                    <input class="form-control" type="number" id="gpioModalReEnableDelay" min="100"
                                        max="3600000" value="5000">
                                    <div class="form-text">Duration before the input is re-enabled.</div>
                                </div>
                            </div>
                            <div class="alert alert-info py-2 mt-2 mb-0" id="gpioReEnableInfo" style="display:none">
                                <i class="fas fa-info-circle"></i>
                                While suppressed, additional button presses are ignored.
                                <strong>Player idle</strong> mode re-enables once playback finishes;
                                if no playback starts within 2 seconds the input re-enables automatically.
                            </div>
                        </div>

                        <!-- LED / illuminated button ────────────────────────── -->
                        <div class="gpio-modal-section">
                            <div class="gpio-modal-section-title">
                                <i class="fas fa-lightbulb text-warning"></i> Illuminated Button LED
                                <span class="section-subtitle">(optional — drives a GPIO output for the button's
                                    LED)</span>
                            </div>

                            <div class="row g-3">
                                <div class="col-md-4">
                                    <label class="form-label fw-semibold">LED Output Pin <?= helpTip('GPIO output pin wired to the button\'s built-in LED. The pin is driven as a digital output — ensure it is not used for any other purpose such as pixel strings or cape outputs.') ?></label>
                                    <select class="form-select" id="gpioModalLedPin" onchange="updateLEDVisibility()">
                                        <option value="">None</option>
                                    </select>
                                    <div class="form-text">GPIO output connected to the button's LED.</div>
                                </div>
                            </div>

                            <div id="gpioLEDSettingsRow" style="display:none" class="mt-3">
                                <div class="row g-3">
                                    <div class="col-md-4">
                                        <label class="form-label fw-semibold">LED Logic <?= helpTip('Which output level illuminates the LED.<br><strong>Active-high</strong> – writing HIGH turns the LED on. This is the most common wiring when the LED is connected directly between the GPIO pin and GND.<br><strong>Active-low</strong> – writing LOW turns the LED on. Typical when a transistor or open-collector driver is used.') ?></label>
                                        <select class="form-select" id="gpioModalLedActiveHigh">
                                            <option value="true">Active-high (HIGH = LED on)</option>
                                            <option value="false">Active-low (LOW = LED on)</option>
                                        </select>
                                    </div>
                                </div>

                                <hr class="my-3">
                                <h6 class="fw-semibold mb-2"><i class="fas fa-moon text-secondary"></i> Idle / Standby
                                    State</h6>
                                <p class="small text-muted mb-2">What the LED does when no trigger is active.</p>
                                <div class="row g-3">
                                    <div class="col-md-4">
                                        <label class="form-label fw-semibold">Idle LED Mode <?= helpTip('What the LED does when no trigger is active — the standby / ready state.<br><strong>Off</strong> – LED stays dark at rest.<br><strong>On</strong> – LED stays lit at rest, indicating the input is ready to accept a press.<br><strong>Pulsing</strong> – LED blinks continuously at the configured rate. While the input is suppressed after a trigger, the LED is held off regardless of this setting.') ?></label>
                                        <select class="form-select" id="gpioModalLedIdleMode"
                                            onchange="updateLEDVisibility()">
                                            <option value="off">Off (dark at rest)</option>
                                            <option value="on">On (lit at rest)</option>
                                            <option value="pulse">Pulsing / blinking</option>
                                        </select>
                                    </div>
                                    <div class="col-md-4" id="gpioLEDPulseRateRow" style="display:none">
                                        <label class="form-label fw-semibold">Pulse half-period (ms) <?= helpTip('Time in milliseconds between each LED on↔off toggle while pulsing. 500 ms gives a 1 Hz blink (on for 500 ms, off for 500 ms). Minimum 50 ms.') ?></label>
                                        <input class="form-control" type="number" id="gpioModalLedPulseRate" min="50"
                                            max="5000" value="500">
                                        <div class="form-text">Time between LED on↔off toggles (e.g. 500 = 1 Hz blink).
                                        </div>
                                    </div>
                                </div>

                                <hr class="my-3">
                                <h6 class="fw-semibold mb-2"><i class="fas fa-bolt text-warning"></i> Trigger Effect
                                </h6>
                                <p class="small text-muted mb-2">What the LED does when the button fires (rising edge).
                                </p>
                                <div class="row g-3">
                                    <div class="col-md-5">
                                        <label class="form-label fw-semibold">Trigger Mode <?= helpTip('What the LED does immediately when the button fires (rising edge).<br><strong>None</strong> – idle mode controls the LED; triggers have no extra effect.<br><strong>Follow input</strong> – LED turns on while the button is held and off when released.<br><strong>Flash N times</strong> – LED flashes the set number of times (each flash: 150 ms on, 150 ms off) then returns to idle.<br><strong>Stay on for N ms</strong> – LED lights up for the specified duration then returns to idle.') ?></label>
                                        <select class="form-select" id="gpioModalLedTriggerMode"
                                            onchange="updateLEDVisibility()">
                                            <option value="none">None (idle mode only, ignore trigger)</option>
                                            <option value="follow_input">Follow input (on while pressed)</option>
                                            <option value="flash">Flash N times then return to idle</option>
                                            <option value="timed_on">Stay on for N ms then return to idle</option>
                                        </select>
                                    </div>
                                    <div class="col-md-3" id="gpioLEDTriggerParamRow" style="display:none">
                                        <label class="form-label fw-semibold" id="gpioLEDTriggerParamLabel">Number of
                                            flashes <span id="gpioLEDTriggerParamTip" data-bs-toggle="tooltip" data-bs-html="true" data-bs-placement="auto" data-bs-container="body" data-bs-title=""><img src="images/redesign/help-icon.svg" class="icon-help" alt="help icon"></span></label>
                                        <input class="form-control" type="number" id="gpioModalLedTriggerParam" min="1"
                                            max="20" value="3">
                                        <div class="form-text" id="gpioLEDTriggerParamHelp"></div>
                                    </div>
                                </div>
                            </div>
                        </div>

                    </div><!-- .modal-body -->

                    <div class="modal-footer">
                        <button type="button" class="btn btn-secondary" data-bs-dismiss="modal">Cancel</button>
                        <button type="button" class="btn btn-primary" onclick="saveGPIOModal()">
                            <i class="fas fa-check"></i> Apply
                        </button>
                    </div>

                </div>
            </div>
        </div>

        <?php include 'common/footer.inc'; ?>
    </div>
</body>

</html>