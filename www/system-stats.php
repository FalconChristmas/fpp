<?php
if (isset($_GET['cpu'])) {
    $skipJSsettings = true;
    require_once 'config.php';
    require_once 'common.php';
    header('Content-Type: application/json');
    echo json_encode(['cpu' => get_cpu_stats_raw()]);
    exit;
}
?>
<!DOCTYPE html>
<html>

<head>
    <?php
    include 'common/htmlMeta.inc';
    require_once 'config.php';
    require_once 'common.php';
    include 'common/htmlMeta.inc';

    include 'common/menuHead.inc';
    ?>
    <title><?php echo $pageTitle; ?></title>
    <link rel="stylesheet" href="css/fpp-system-design.css">
    <script>
        var uptimeInterval;
        var uptimeSeconds = 0;

        var healthCheckSource = null;

        // Track which checks have received results
        var receivedChecks = {};

        function HealthCheckDone() {
            $('#btnStartHealthCheck').prop('disabled', false);
            $('#btnStartHealthCheck i').removeClass('fa-spin');
            if (healthCheckSource) {
                healthCheckSource.close();
                healthCheckSource = null;
            }

            // Mark any static checks that didn't receive results as "Skipped"
            var allChecks = healthChecks.left.concat(healthChecks.right);
            allChecks.forEach(function (check) {
                if (check.static && !receivedChecks[check.id]) {
                    var statusEl = $('#hc-' + check.id);
                    if (statusEl.length) {
                        statusEl.html(
                            '<i class="fas fa-minus-circle fpp-health-check__status-icon fpp-status--loading"></i>' +
                            '<span class="fpp-health-check__status-text" title="Skipped">Skipped</span>'
                        );
                    }
                }
            });
        }

        function getStatusIcon(status) {
            switch (status) {
                case 'pass': return 'fa-check-circle fpp-status--pass';
                case 'warn': return 'fa-exclamation-circle fpp-status--warn';
                case 'fail': return 'fa-times-circle fpp-status--fail';
                default: return 'fa-spinner fa-spin fpp-status--loading';
            }
        }

        function updateHealthCheckItem(check) {
            var itemEl = $('[data-check="' + check.id + '"]');
            var statusEl = $('#hc-' + check.id);

            // Track that we received this check
            receivedChecks[check.id] = true;

            if (statusEl.length) {
                var statusHtml =
                    '<i class="fas ' + getStatusIcon(check.status) + ' fpp-health-check__status-icon"></i>' +
                    '<span class="fpp-health-check__status-text" title="' + check.message + '">' + check.message + '</span>';

                // Add expand icon if warning details are present
                if (check.details && check.details.length > 0) {
                    statusHtml += '<i class="fas fa-chevron-down fpp-health-check__expand-icon"></i>';
                }

                statusEl.html(statusHtml);

                // Expandable details (warnings)
                if (check.details && check.details.length > 0) {
                    itemEl.addClass('fpp-health-check__item--expandable');
                    itemEl.off('click').on('click', function () {
                        $(this).toggleClass('fpp-health-check__item--expanded');
                        $('#details-' + check.id).toggleClass('fpp-health-check__details--visible');
                    });

                    $('#details-' + check.id).remove();
                    var detailsHtml = '<div class="fpp-health-check__details" id="details-' + check.id + '"><ul>';
                    check.details.forEach(function (detail) {
                        var escaped = $('<span>').text(detail).html();
                        detailsHtml += '<li><i class="fas fa-circle"></i> ' + escaped + '</li>';
                    });
                    detailsHtml += '</ul></div>';
                    itemEl.after(detailsHtml);
                }

                // Show the item if it was hidden (conditional check)
                if (itemEl.length && conditionalChecks.includes(check.id)) {
                    itemEl.show();
                }
            }
        }

        function updateHealthSummary(summary) {
            $('#hcPassCount').text(summary.pass).removeClass().addClass('fpp-health-summary__count fpp-status--pass');
            $('#hcWarnCount').text(summary.warn).removeClass().addClass('fpp-health-summary__count fpp-status--warn');
            $('#hcFailCount').text(summary.fail).removeClass().addClass('fpp-health-summary__count fpp-status--fail');
        }

        // All checks in display order - static ones are always shown, conditional ones start hidden
        // Only scheduler and mediadisk are truly conditional (may not be emitted at all)
        var healthChecks = {
            left: [
                { id: 'fppd', label: 'FPPD Daemon', icon: 'fa-play-circle', static: true },
                { id: 'warnings', label: 'FPPD Warnings', icon: 'fa-exclamation-triangle', static: true },
                { id: 'hostname', label: 'Unique Hostname', icon: 'fa-server', static: true },
                { id: 'rootdisk', label: 'Root Filesystem', icon: 'fa-hdd', static: true },
                { id: 'ntp', label: 'Time Sync (NTP)', icon: 'fa-clock', static: true },
                { id: 'scheduler', label: 'Scheduler', icon: 'fa-calendar-alt', static: false }
            ],
            right: [
                { id: 'gateway', label: 'Default Gateway', icon: 'fa-network-wired', static: true },
                { id: 'gateway_ping', label: 'Gateway Reachable', icon: 'fa-exchange-alt', static: true },
                { id: 'internet', label: 'Internet Access', icon: 'fa-globe', static: true },
                { id: 'dns', label: 'DNS Resolution', icon: 'fa-search', static: true },
                { id: 'datetime', label: 'Browser Time Sync', icon: 'fa-clock', static: true },
                { id: 'mediadisk', label: 'Media Partition', icon: 'fa-folder', static: false }
            ]
        };

        // Only these two checks may not be emitted at all
        var conditionalChecks = ['scheduler', 'mediadisk'];

        function renderPlaceholder(check) {
            var hiddenStyle = check.static ? '' : ' style="display: none;"';
            var statusHtml = check.static
                ? '<i class="fas fa-spinner fa-spin fpp-health-check__status-icon fpp-status--loading"></i>' +
                '<span class="fpp-health-check__status-text">Checking...</span>'
                : '';
            return '<li class="fpp-health-check__item" data-check="' + check.id + '"' + hiddenStyle + '>' +
                '<span class="fpp-health-check__label">' +
                '<i class="fas me-1 ' + check.icon + '"></i> ' + check.label +
                '</span>' +
                '<span class="fpp-health-check__status" id="hc-' + check.id + '">' +
                statusHtml +
                '</span>' +
                '</li>';
        }

        function StartHealthCheck() {
            $('#btnStartHealthCheck').prop('disabled', true);
            $('#btnStartHealthCheck i').addClass('fa-spin');

            // Reset tracking for new health check run
            receivedChecks = {};

            // Build layout with pre-rendered placeholders
            var leftHtml = healthChecks.left.map(renderPlaceholder).join('');
            var rightHtml = healthChecks.right.map(renderPlaceholder).join('');

            $('#healthCheckOutput').html(
                '<div class="fpp-health-summary">' +
                '<div class="fpp-health-summary__item">' +
                '<span class="fpp-health-summary__count fpp-status--pass" id="hcPassCount">-</span>' +
                '<span class="fpp-health-summary__label">Passed</span>' +
                '</div>' +
                '<div class="fpp-health-summary__item">' +
                '<span class="fpp-health-summary__count fpp-status--warn" id="hcWarnCount">-</span>' +
                '<span class="fpp-health-summary__label">Warnings</span>' +
                '</div>' +
                '<div class="fpp-health-summary__item">' +
                '<span class="fpp-health-summary__count fpp-status--fail" id="hcFailCount">-</span>' +
                '<span class="fpp-health-summary__label">Issues</span>' +
                '</div>' +
                '</div>' +
                '<div class="row">' +
                '<div class="col-lg-6"><ul class="fpp-health-check" id="healthCheckLeft">' + leftHtml + '</ul></div>' +
                '<div class="col-lg-6"><ul class="fpp-health-check" id="healthCheckRight">' + rightHtml + '</ul></div>' +
                '</div>'
            );

            var timestamp = Math.floor(Date.now() / 1000);

            fetch('healthCheckSSE.php?timestamp=' + timestamp, { credentials: 'same-origin' })
                .then(function (response) {
                    if (!response.ok) throw new Error('HTTP ' + response.status);
                    var reader = response.body.getReader();
                    var decoder = new TextDecoder();
                    var buffer = '';

                    function processStream() {
                        reader.read().then(function (result) {
                            if (result.done) {
                                HealthCheckDone();
                                return;
                            }

                            buffer += decoder.decode(result.value, { stream: true });
                            var lines = buffer.split('\n');
                            buffer = lines.pop();

                            lines.forEach(function (line) {
                                if (line.startsWith('data: ')) {
                                    try {
                                        var check = JSON.parse(line.substring(6));
                                        if (check.id === 'done') {
                                            updateHealthSummary(check.summary);
                                        } else {
                                            updateHealthCheckItem(check);
                                        }
                                    } catch (e) { }
                                }
                            });

                            processStream();
                        }).catch(function () {
                            HealthCheckDone();
                        });
                    }

                    processStream();
                })
                .catch(function () {
                    HealthCheckDone();
                    $('#healthCheckOutput').html('<div class="alert alert-danger">Failed to run health check</div>');
                });
        }

        // Updates gauge with thresholds: { yellow: 60, red: 80, max: 100, unit: '%' }
        function updateGauge(gaugeId, value, thresholds) {
            var fillElement = document.getElementById(gaugeId + 'Fill');
            var valueElement = document.getElementById(gaugeId.replace('Gauge', 'Value'));
            if (!fillElement) return;

            var normalizedValue = thresholds.max
                ? Math.min((value / thresholds.max) * 100, 100)
                : value;

            // Calculate stroke-dasharray (circumference = 2 * π * 45 ≈ 282.7)
            var dashArray = (normalizedValue * 2.827).toFixed(1) + ' 282.7';
            fillElement.setAttribute('stroke-dasharray', dashArray);

            // Update color class based on thresholds
            fillElement.classList.remove('fpp-gauge__fill--success', 'fpp-gauge__fill--warning', 'fpp-gauge__fill--danger');
            if (value > thresholds.red) {
                fillElement.classList.add('fpp-gauge__fill--danger');
            } else if (value > thresholds.yellow) {
                fillElement.classList.add('fpp-gauge__fill--warning');
            } else {
                fillElement.classList.add('fpp-gauge__fill--success');
            }

            if (valueElement) {
                valueElement.textContent = value.toFixed(0) + thresholds.unit;
            }
        }

        function updateUptimeDisplay() {
            var days = Math.floor(uptimeSeconds / 86400);
            var hours = Math.floor((uptimeSeconds % 86400) / 3600);
            var minutes = Math.floor((uptimeSeconds % 3600) / 60);
            var seconds = uptimeSeconds % 60;

            $('#uptime-days').text(days.toString().padStart(2, '0'));
            $('#uptime-hours').text(hours.toString().padStart(2, '0'));
            $('#uptime-minutes').text(minutes.toString().padStart(2, '0'));
            $('#uptime-seconds').text(seconds.toString().padStart(2, '0'));
        }

        function formatBytes(bytes, decimals = 1) {
            if (bytes === 0) return '0 B';
            const k = 1024;
            const sizes = ['B', 'KB', 'MB', 'GB', 'TB'];
            const i = Math.floor(Math.log(bytes) / Math.log(k));
            return parseFloat((bytes / Math.pow(k, i)).toFixed(decimals)) + ' ' + sizes[i];
        }

        // CPU: EMA smoothing - reduce jitter
        var cpuPrev = null;
        var cpuEma = null;
        var cpuAlpha = 0.4;
        // 2 polls (~10 sec) to reach ~64% of a sustained change
        // 4 polls (~20 sec) to reach ~87%
        // 5 polls (~25 sec) to reach ~92%
        function updateCpuGauge() {
            $.getJSON('system-stats.php?cpu=1', function (data) {
                var raw;
                if (data.cpu.mac !== undefined) {
                    raw = data.cpu.mac;
                } else {
                    var cur = data.cpu.map(Number);
                    var idle, total = 0;
                    if (cpuPrev) {
                        idle = cur[3] - cpuPrev[3];
                        for (var i = 0; i < cur.length; i++) total += cur[i] - cpuPrev[i];
                    } else {
                        idle = cur[3];
                        for (var i = 0; i < cur.length; i++) total += cur[i];
                    }
                    raw = (total > 0) ? 100 - (idle * 100 / total) : 0;
                    cpuPrev = cur;
                }
                cpuEma = (cpuEma === null) ? raw : cpuAlpha * raw + (1 - cpuAlpha) * cpuEma;
                updateGauge('cpuGauge', cpuEma, { yellow: 60, red: 80, unit: '%' });
            });
        }

        function updateStats() {
            updateCpuGauge();
            $.get('api/system/status', function (data) {
                // Temperature
                if (data.sensors && data.sensors.length > 0) {
                    var temp = parseFloat(data.sensors[0].value);
                    <?php if (isset($settings['temperatureInF']) && $settings['temperatureInF'] == 1) { ?>
                        temp = (temp * 9 / 5) + 32;
                        // Fahrenheit thresholds: 140°F (60°C), 176°F (80°C), max 212°F (100°C)
                        updateGauge('tempGauge', temp, { yellow: 140, red: 176, max: 212, unit: '°F' });
                    <?php } else { ?>
                        // Celsius thresholds: 60°C, 80°C, max 100°C
                        updateGauge('tempGauge', temp, { yellow: 60, red: 80, max: 100, unit: '°C' });
                    <?php } ?>
                }

                // Uptime
                if (data.systemUptimeTotalSeconds) {
                    uptimeSeconds = parseInt(data.systemUptimeTotalSeconds);
                    updateUptimeDisplay();
                    if (!uptimeInterval) {
                        uptimeInterval = setInterval(function () { uptimeSeconds++; updateUptimeDisplay(); }, 1000);
                    }
                }

                // Load Average - get from PHP
                <?php
                $loadavg = sys_getloadavg();
                $cores = 4; // default
                if (file_exists('/proc/cpuinfo')) {
                    $cpuinfo = file_get_contents('/proc/cpuinfo');
                    preg_match_all('/^processor/m', $cpuinfo, $matches);
                    $cores = count($matches[0]);
                }
                ?>
                var loads = [<?= implode(',', $loadavg) ?>];
                var cores = <?= $cores ?>;

                if (loads.length >= 3) {
                    var load1 = parseFloat(loads[0]);
                    var pct1 = Math.min((load1 / cores) * 100, 100);
                    $('#load-1min-value').text(load1.toFixed(2));
                    $('#load-1min-bar').css('width', pct1 + '%').attr('aria-valuenow', pct1);
                    updateLoadColor($('#load-1min-bar'), pct1);
                    updateBusynessStatus(pct1);

                    var load5 = parseFloat(loads[1]);
                    var pct5 = Math.min((load5 / cores) * 100, 100);
                    $('#load-5min-value').text(load5.toFixed(2));
                    $('#load-5min-bar').css('width', pct5 + '%').attr('aria-valuenow', pct5);
                    updateLoadColor($('#load-5min-bar'), pct5);

                    var load15 = parseFloat(loads[2]);
                    var pct15 = Math.min((load15 / cores) * 100, 100);
                    $('#load-15min-value').text(load15.toFixed(2));
                    $('#load-15min-bar').css('width', pct15 + '%').attr('aria-valuenow', pct15);
                    updateLoadColor($('#load-15min-bar'), pct15);
                }

                $('#cpu-cores').text(cores);
            });


            // Update disk storage - using server-side data
            updateDiskStorage();

            // Player Statistics
            updatePlayerStats();
        }

        function updateDiskStorage() {
            <?php
            // Get root disk info
            $rootTotal = disk_total_space("/");
            $rootFree = disk_free_space("/");
            $rootUsed = $rootTotal - $rootFree;
            $rootPct = ($rootUsed / $rootTotal) * 100;

            // Get root device name
            $rootDevice = 'unknown';
            if (file_exists("/bin/findmnt")) {
                exec('findmnt -n -o SOURCE / | colrm 1 5', $output, $return_val);
                if ($return_val == 0 && !empty($output[0])) {
                    $rootDevice = trim($output[0]);
                }
            }

            // Check if media is on different device
            $mediaDirectory = $settings['mediaDirectory'] ?? '/home/fpp/media';
            $mediaTotal = disk_total_space($mediaDirectory);
            $mediaFree = disk_free_space($mediaDirectory);
            $mediaSeparate = false;

            if (file_exists("/bin/findmnt")) {
                exec('findmnt -n -o SOURCE ' . $mediaDirectory . ' | colrm 1 5', $output2, $return_val2);
                if ($return_val2 == 0 && !empty($output2[0])) {
                    $mediaDevice = trim($output2[0]);
                    $mediaSeparate = ($mediaDevice !== $rootDevice);
                }
            }
            ?>

            // Root partition
            var rootUsed = <?= $rootUsed ?>;
            var rootTotal = <?= $rootTotal ?>;
            var rootFree = <?= $rootFree ?>;
            var rootPct = <?= $rootPct ?>;

            $('#root-device').text('<?= addslashes($rootDevice) ?>');
            $('#root-info').text(formatBytes(rootUsed) + ' / ' + formatBytes(rootTotal) + ' (' + formatBytes(rootFree) + ' free)');
            $('#root-bar').css('width', rootPct + '%').attr('aria-valuenow', rootPct);
            updateDiskColor($('#root-bar'), rootPct);

            <?php if ($mediaSeparate) {
                $mediaUsed = $mediaTotal - $mediaFree;
                $mediaPct = ($mediaUsed / $mediaTotal) * 100;
                ?>
                // Media partition
                var mediaUsed = <?= $mediaUsed ?>;
                var mediaTotal = <?= $mediaTotal ?>;
                var mediaFree = <?= $mediaFree ?>;
                var mediaPct = <?= $mediaPct ?>;

                $('#media-row').show();
                $('#media-device').text('<?= addslashes($mediaDevice) ?>');
                $('#media-info').text(formatBytes(mediaUsed) + ' / ' + formatBytes(mediaTotal) + ' (' + formatBytes(mediaFree) + ' free)');
                $('#media-bar').css('width', mediaPct + '%').attr('aria-valuenow', mediaPct);
                updateDiskColor($('#media-bar'), mediaPct);

                // Show disk warning if media or root partition is over 85%
                if (mediaPct > 85 || rootPct > 85) {
                    $('#disk-warning').show();
                } else {
                    $('#disk-warning').hide();
                }
            <?php } else { ?>
                $('#media-row').hide();

                // Show disk warning if root partition is over 85%
                if (rootPct > 85) {
                    $('#disk-warning').show();
                } else {
                    $('#disk-warning').hide();
                }
            <?php } ?>
        }

        function updateDiskColor(element, percent) {
            element.removeClass('bg-success bg-warning bg-danger');
            if (percent > 85) {
                element.addClass('bg-danger');
            } else if (percent > 70) {
                element.addClass('bg-warning');
            } else {
                element.addClass('bg-success');
            }
        }

        function updateLoadColor(element, percent) {
            element.removeClass('bg-success bg-warning bg-danger');
            if (percent > 80) {
                element.addClass('bg-danger');
            } else if (percent > 60) {
                element.addClass('bg-warning');
            } else {
                element.addClass('bg-success');
            }
        }

        function updateBusynessStatus(pct) {
            var statusEl = document.getElementById('busyness-status');
            if (!statusEl) return;
            var text, icon, cls;
            if (pct > 90) {
                text = 'Overloaded';
                icon = 'fa-exclamation-triangle';
                cls = 'fpp-status--fail';
            } else if (pct > 70) {
                text = 'Busy';
                icon = 'fa-exclamation-circle';
                cls = 'fpp-status--warn';
            } else {
                text = 'Running smoothly';
                icon = 'fa-check-circle';
                cls = 'fpp-status--pass';
            }
            statusEl.innerHTML = '<i class="fas ' + icon + ' ' + cls + '"></i> ' + text;
            statusEl.className = 'busyness-status ' + cls;
        }

        function updatePlayerStats() {
            var stats = {
                schedules: 0,
                playlists: 0,
                sequences: 0,
                audio: 0,
                videos: 0,
                effects: 0,
                scripts: 0
            };

            $.get('api/playlists', function (data) {
                if (data && Array.isArray(data)) {
                    stats.playlists = data.length;
                }
                $('#stat-playlists').text(stats.playlists);
            });

            $.get('api/schedule', function (data) {
                if (data && Array.isArray(data)) {
                    stats.schedules = data.length;
                }
                $('#stat-schedules').text(stats.schedules);
            });

            $.get('api/files/sequences?nameOnly=1', function (data) {
                if (data && Array.isArray(data)) {
                    stats.sequences = data.length;
                }
                $('#stat-sequences').text(stats.sequences);
            });

            $.get('api/files/music?nameOnly=1', function (data) {
                if (data && Array.isArray(data)) {
                    stats.audio = data.length;
                }
                $('#stat-audio').text(stats.audio);
            });

            $.get('api/files/videos?nameOnly=1', function (data) {
                if (data && Array.isArray(data)) {
                    stats.videos = data.length;
                }
                $('#stat-videos').text(stats.videos);
            });

            $.get('api/files/effects?nameOnly=1', function (data) {
                if (data && Array.isArray(data)) {
                    stats.effects = data.length;
                }
                $('#stat-effects').text(stats.effects);
            });

            $.get('api/files/scripts?nameOnly=1', function (data) {
                if (data && Array.isArray(data)) {
                    stats.scripts = data.length;
                }
                $('#stat-scripts').text(stats.scripts);
            });
        }

        $(document).ready(function () {
            StartHealthCheck(); updateStats();
            setInterval(function () {
                updateStats();
            }, 5000);
        });
    </script>
</head>

<body>
    <div id="bodyWrapper">
        <?php
        $activeParentMenuItem = 'help';
        include 'menu.inc';
        ?>
        <div class="mainContainer">
            <h1 class="title">Health and Status</h1>
            <div class="pageContent">

                <?php
                if (isset($settings["UnpartitionedSpace"]) && $settings["UnpartitionedSpace"] > 0) {
                    ?>
                    <div id='upgradeFlag' class="fpp-alert fpp-alert--danger" role="alert">
                        SD card has unused space. Go to
                        <a href="settings.php#settings-storage">Storage Settings</a> to expand the
                        file system or create a new storage partition.
                    </div>
                    <?php
                }
                ?>

                <!-- Disk Space Warning Banner -->
                <div id="disk-warning" class="fpp-alert fpp-alert--danger" role="alert" style="display: none;">
                    <i class="fas fa-exclamation-triangle"></i>
                    <span>
                        <strong>High Disk Usage Detected!</strong>
                        Your disk is running low on space. Consider cleaning up old files in the
                        <a href="filemanager.php">File Manager</a>.
                    </span>
                </div>

                <!-- Health Check Section -->
                <div class="card compact-card health-check-card">
                    <button class="fpp-health-rerun-btn" id="btnStartHealthCheck" onclick="StartHealthCheck();">
                        <i class="fas fa-sync-alt"></i> Re-run
                    </button>
                    <div class="card-header">
                        <h3><i class="fa-solid fa-stethoscope"></i> System Health</h3>
                    </div>
                    <div class="card-body">
                        <div id='healthCheckOutput'></div>
                    </div>
                </div>

                <!-- System Monitoring Gauges -->
                <div class="row">
                    <div class="col-md-4">
                        <div class="card compact-card fpp-gauge">
                            <div class="card-header">
                                <h5><i class="fa-solid fa-microchip"></i> CPU Usage</h5>
                            </div>
                            <div class="card-body">
                                <div class="fpp-gauge__container">
                                    <div class="fpp-gauge__circle">
                                        <svg viewBox="0 0 100 100">
                                            <circle class="fpp-gauge__bg" cx="50" cy="50" r="45"></circle>
                                            <circle class="fpp-gauge__fill fpp-gauge__fill--success" id="cpuGaugeFill"
                                                cx="50" cy="50" r="45" stroke-dasharray="0 282.7"></circle>
                                        </svg>
                                        <div class="fpp-gauge__value" id="cpuValue">--%</div>
                                    </div>
                                </div>
                                <div class="fpp-gauge__label">Current CPU</div>
                            </div>
                        </div>
                    </div>
                    <div class="col-md-4">
                        <?php
                        $memInfo = get_server_memory_info();
                        $memTotal = $memInfo['total'];
                        $memFree = $memInfo['free'];
                        $memUsed = $memInfo['used'];
                        $memBufferCache = $memInfo['buffer_cache'];
                        $memUsage = $memInfo['usage_percent'];
                        $memBufferUsage = $memInfo['buffer_percent'];
                        $memFreeUsage = $memInfo['free_percent'];
                        $memTotalUsage = $memInfo['total_used_percent'];
                        $memClass = "fpp-gauge__fill--success";
                        if ($memUsage > 60)
                            $memClass = "fpp-gauge__fill--warning";
                        if ($memUsage > 80)
                            $memClass = "fpp-gauge__fill--danger";

                        function formatMemBytes($bytes, $decimals = 1)
                        {
                            if ($bytes == 0)
                                return '0 B';
                            $k = 1024;
                            $sizes = ['B', 'KB', 'MB', 'GB', 'TB'];
                            $i = floor(log($bytes) / log($k));
                            return round($bytes / pow($k, $i), $decimals) . ' ' . $sizes[$i];
                        }
                        ?>
                        <div class="card compact-card fpp-gauge">
                            <div class="card-header">
                                <h5>
                                    <span><i class="fa-solid fa-memory"></i> Memory Usage</span>
                                    <i class="fas fa-question-circle fpp-help-popover"
                                        data-help-content="memoryHelpContent" data-help-title="Memory Types"
                                        style="font-size: 0.8em; cursor: help; margin-left: auto;"></i>
                                </h5>
                            </div>
                            <div id="memoryHelpContent" style="display: none;">
                                <div class="fpp-help-content">
                                    <p><span class="fpp-help-swatch fpp-help-swatch--success"></span><strong>Used
                                            Memory</strong><br>
                                        Memory actively used by applications and the OS.</p>
                                    <p><span
                                            class="fpp-help-swatch fpp-help-swatch--info"></span><strong>Buffer/Cache</strong><br>
                                        Memory used for disk caching. Can be reclaimed if needed.</p>
                                    <p><span class="fpp-help-swatch fpp-help-swatch--muted"></span><strong>Free
                                            Memory</strong><br>
                                        Completely unused memory.</p>
                                    <hr>
                                    <p style="margin-bottom:0;"><em>High buffer/cache is normal — it means your system
                                            is efficiently using available RAM.</em></p>
                                </div>
                            </div>
                            <div class="card-body">
                                <div class="fpp-gauge__container">
                                    <div class="fpp-gauge__circle">
                                        <svg viewBox="0 0 100 100">
                                            <circle class="fpp-gauge__bg" cx="50" cy="50" r="45"></circle>
                                            <circle class="fpp-gauge__fill <?php echo $memClass; ?>" id="memGaugeFill"
                                                cx="50" cy="50" r="45"
                                                stroke-dasharray="<?php printf('%.1f', $memUsage * 2.827); ?> 282.7">
                                                <title>Used: <?php echo formatMemBytes($memUsed); ?>
                                                    (<?php printf('%.1f', $memUsage); ?>%)</title>
                                            </circle>
                                            <circle class="fpp-gauge__fill fpp-gauge__fill--buffer" cx="50" cy="50"
                                                r="45"
                                                stroke-dasharray="<?php printf('%.1f', $memBufferUsage * 2.827); ?> 282.7"
                                                style="transform: rotate(<?php printf('%.1f', $memUsage * 3.6); ?>deg); transform-origin: 50% 50%;">
                                                <title>Buffer/Cache: <?php echo formatMemBytes($memBufferCache); ?>
                                                    (<?php printf('%.1f', $memBufferUsage); ?>%)</title>
                                            </circle>
                                            <circle class="fpp-gauge__fill fpp-gauge__fill--free" cx="50" cy="50" r="45"
                                                stroke-dasharray="<?php printf('%.1f', $memFreeUsage * 2.827); ?> 282.7"
                                                style="transform: rotate(<?php printf('%.1f', $memTotalUsage * 3.6); ?>deg); transform-origin: 50% 50%;">
                                                <title>Free: <?php echo formatMemBytes($memFree); ?>
                                                    (<?php printf('%.1f', $memFreeUsage); ?>%)</title>
                                            </circle>
                                        </svg>
                                        <div class="fpp-gauge__value" id="memValue">
                                            <?php printf('%.0f', $memUsage); ?>%
                                            <span
                                                class="fpp-gauge__total"><?php echo formatMemBytes($memTotal); ?></span>
                                        </div>
                                    </div>
                                </div>
                                <div class="fpp-gauge__label">
                                    <?php echo formatMemBytes($memUsed); ?> used
                                    <span
                                        style="opacity: 0.7; margin-left: 4px;">(+<?php echo formatMemBytes($memBufferCache); ?>
                                        cache)</span>
                                    <br>
                                    <span style="opacity: 0.7;"><?php echo formatMemBytes($memFree); ?> free</span>
                                </div>
                            </div>
                        </div>
                    </div>
                    <div class="col-md-4">
                        <div class="card compact-card fpp-gauge">
                            <div class="card-header">
                                <h5><i class="fa-solid fa-temperature-half"></i> Temperature</h5>
                            </div>
                            <div class="card-body">
                                <div class="fpp-gauge__container">
                                    <div class="fpp-gauge__circle">
                                        <svg viewBox="0 0 100 100">
                                            <circle class="fpp-gauge__bg" cx="50" cy="50" r="45"></circle>
                                            <circle class="fpp-gauge__fill fpp-gauge__fill--success" id="tempGaugeFill"
                                                cx="50" cy="50" r="45" stroke-dasharray="0 282.7"></circle>
                                        </svg>
                                        <div class="fpp-gauge__value" id="tempValue">--°</div>
                                    </div>
                                </div>
                                <div class="fpp-gauge__label" id="tempLabel">CPU Temperature
                                    (<?php echo (isset($settings['temperatureInF']) && $settings['temperatureInF'] == 1) ? '°F' : '°C'; ?>)
                                </div>
                            </div>
                        </div>
                    </div>
                </div>

                <!-- Disk Utilization -->
                <div class="card compact-card">
                    <div class="card-header">
                        <h3><i class="fas fa-hdd"></i> Disk Utilization</h3>
                    </div>
                    <div class="card-body">
                        <div class="disk-row">
                            <div class="disk-label">
                                <span class="disk-device">Root Partition (<span id="root-device">--</span>)</span>
                                <span class="disk-info" id="root-info">-- / --</span>
                            </div>
                            <div class="progress">
                                <div id="root-bar" class="progress-bar bg-success" role="progressbar" style="width: 0%"
                                    aria-valuenow="0" aria-valuemin="0" aria-valuemax="100"></div>
                            </div>
                        </div>
                        <div class="disk-row" id="media-row" style="display: none;">
                            <div class="disk-label">
                                <span class="disk-device">Media Partition (<span id="media-device">--</span>)</span>
                                <span class="disk-info" id="media-info">-- / --</span>
                            </div>
                            <div class="progress">
                                <div id="media-bar" class="progress-bar bg-success" role="progressbar" style="width: 0%"
                                    aria-valuenow="0" aria-valuemin="0" aria-valuemax="100"></div>
                            </div>
                        </div>
                    </div>
                </div>

                <!-- Second Row: Uptime, Load Average, Player Stats -->
                <div class="row">
                    <!-- FPP Uptime -->
                    <div class="col-md-4">
                        <div class="card compact-card">
                            <div class="card-header">
                                <h3><i class="fas fa-clock"></i> System Uptime</h3>
                            </div>
                            <div class="card-body uptime-display">
                                <div class="uptime-counters">
                                    <div class="uptime-segment">
                                        <div class="uptime-number" id="uptime-days">00</div>
                                        <div class="uptime-label">Days</div>
                                    </div>
                                    <div class="uptime-separator">:</div>
                                    <div class="uptime-segment">
                                        <div class="uptime-number" id="uptime-hours">00</div>
                                        <div class="uptime-label">Hours</div>
                                    </div>
                                    <div class="uptime-separator">:</div>
                                    <div class="uptime-segment">
                                        <div class="uptime-number" id="uptime-minutes">00</div>
                                        <div class="uptime-label">Minutes</div>
                                    </div>
                                    <div class="uptime-separator">:</div>
                                    <div class="uptime-segment">
                                        <div class="uptime-number" id="uptime-seconds">00</div>
                                        <div class="uptime-label">Seconds</div>
                                    </div>
                                </div>
                                <?php
                                $lastBoot = "";
                                if ($settings["Platform"] != "MacOS") {
                                    $lastBoot = exec("uptime -s", $output, $return_val);
                                    if ($return_val != 0) {
                                        $lastBoot = "";
                                    }
                                    unset($output);
                                }
                                if ($lastBoot != "") {
                                    ?>
                                    <div class="uptime-started"><i class="fas fa-power-off"></i> System started:
                                        <?php echo $lastBoot; ?>
                                    </div>
                                <?php } ?>
                            </div>
                        </div>
                    </div>

                    <!-- Load Average -->
                    <div class="col-md-4">
                        <?php
                        $_la = sys_getloadavg();
                        $_nc = 4;
                        if (file_exists('/proc/cpuinfo')) {
                            preg_match_all('/^processor/m', file_get_contents('/proc/cpuinfo'), $_m);
                            $_nc = count($_m[0]) ?: 4;
                        }
                        $_lp = min(($_la[0] / $_nc) * 100, 100);
                        $_traffic = $_lp > 90 ? 'heavy' : ($_lp > 70 ? 'moderate' : 'light');
                        ?>
                        <div class="card compact-card">
                            <div class="card-header">
                                <h3 style="display: flex; align-items: center;">
                                    <span><i class="fas fa-tachometer-alt"></i> System Busyness</span>
                                    <i class="fas fa-question-circle fpp-help-popover"
                                        data-help-content="busynessHelpContent" data-help-title="System Busyness"
                                        style="font-size: 0.8em; cursor: help; margin-left: auto;"></i>
                                </h3>
                            </div>
                            <?php
                            $coresNum = (int) $cores;
                            $threshGreen = number_format($coresNum * 0.70, 2);
                            $threshYellow = number_format($coresNum * 0.90, 2);
                            ?>
                            <div id="busynessHelpContent" style="display: none;">
                                <div class="fpp-help-content">
                                    <p><span class="fpp-help-swatch fpp-help-swatch--success"></span><strong>Running
                                            smoothly</strong> (below <?= $threshGreen ?>)<br>
                                        Traffic is light — your system has plenty of capacity.</p>
                                    <p><span
                                            class="fpp-help-swatch fpp-help-swatch--warning"></span><strong>Busy</strong>
                                        (<?= $threshGreen ?> – <?= $threshYellow ?>)<br>
                                        Traffic is moderate — things are getting congested.</p>
                                    <p><span
                                            class="fpp-help-swatch fpp-help-swatch--danger"></span><strong>Overloaded</strong>
                                        (above <?= $threshYellow ?>)<br>
                                        Traffic is heavy — your system is in gridlock.</p>
                                    <hr>
                                    <p style="margin-bottom:0;"><em>Your system is like a highway with <?= $coresNum ?>
                                            lanes (CPU cores). The numbers show average load over 1, 5, and 15 minutes.
                                            Right now, traffic is <?= $_traffic ?>.</em></p>
                                </div>
                            </div>
                            <div class="card-body">
                                <div class="load-bar-container">
                                    <div class="load-label">
                                        <span>1 minute</span>
                                        <span id="load-1min-value">--</span>
                                    </div>
                                    <div class="progress">
                                        <div id="load-1min-bar" class="progress-bar bg-success" role="progressbar"
                                            style="width: 0%" aria-valuenow="0" aria-valuemin="0" aria-valuemax="100">
                                        </div>
                                    </div>
                                </div>
                                <div class="load-bar-container">
                                    <div class="load-label">
                                        <span>5 minutes</span>
                                        <span id="load-5min-value">--</span>
                                    </div>
                                    <div class="progress">
                                        <div id="load-5min-bar" class="progress-bar bg-success" role="progressbar"
                                            style="width: 0%" aria-valuenow="0" aria-valuemin="0" aria-valuemax="100">
                                        </div>
                                    </div>
                                </div>
                                <div class="load-bar-container">
                                    <div class="load-label">
                                        <span>15 minutes</span>
                                        <span id="load-15min-value">--</span>
                                    </div>
                                    <div class="progress">
                                        <div id="load-15min-bar" class="progress-bar bg-success" role="progressbar"
                                            style="width: 0%" aria-valuenow="0" aria-valuemin="0" aria-valuemax="100">
                                        </div>
                                    </div>
                                </div>
                                <div id="busyness-status" class="busyness-status"></div>
                                <div class="load-avg-info">
                                    <i class="fas fa-microchip"></i> <span id="cpu-cores">--</span> CPU cores available
                                </div>
                            </div>
                        </div>
                    </div>

                    <!-- Player Statistics -->
                    <div class="col-md-4">
                        <div class="card compact-card">
                            <div class="card-header">
                                <h3><i class="fas fa-chart-bar"></i> Player Statistics</h3>
                            </div>
                            <div class="card-body">
                                <a href="scheduler.php" class="stat-link">
                                    <span class="stat-name">Schedules</span>
                                    <span class="stat-count" id="stat-schedules">0</span>
                                </a>
                                <a href="playlists.php" class="stat-link">
                                    <span class="stat-name">Playlists</span>
                                    <span class="stat-count" id="stat-playlists">0</span>
                                </a>
                                <a href="filemanager.php" class="stat-link">
                                    <span class="stat-name">Sequences</span>
                                    <span class="stat-count" id="stat-sequences">0</span>
                                </a>
                                <a href="filemanager.php#tab-audio" class="stat-link">
                                    <span class="stat-name">Audio Files</span>
                                    <span class="stat-count" id="stat-audio">0</span>
                                </a>
                                <a href="filemanager.php#tab-video" class="stat-link">
                                    <span class="stat-name">Videos</span>
                                    <span class="stat-count" id="stat-videos">0</span>
                                </a>
                                <a href="filemanager.php#tab-effects" class="stat-link">
                                    <span class="stat-name">Effects</span>
                                    <span class="stat-count" id="stat-effects">0</span>
                                </a>
                                <a href="filemanager.php#tab-scripts" class="stat-link">
                                    <span class="stat-name">Scripts</span>
                                    <span class="stat-count" id="stat-scripts">0</span>
                                </a>
                            </div>
                        </div>
                    </div>
                </div>

            </div>
        </div>
    </div>
    <?php include 'common/footer.inc'; ?>
    <script>
        // Initialize popovers
        var popoverTriggerList = [].slice.call(document.querySelectorAll('[data-bs-toggle="popover"]'));
        var popoverList = popoverTriggerList.map(function (popoverTriggerEl) {
            return new bootstrap.Popover(popoverTriggerEl);
        });

        document.querySelectorAll('.fpp-help-popover').forEach(function (icon) {
            var contentEl = document.getElementById(icon.dataset.helpContent);
            if (contentEl) {
                new bootstrap.Popover(icon, {
                    title: icon.dataset.helpTitle || '',
                    content: contentEl.innerHTML,
                    html: true,
                    trigger: 'hover focus',
                    placement: 'bottom',
                    sanitize: false
                });
            }
        });
    </script>
</body>

</html>
