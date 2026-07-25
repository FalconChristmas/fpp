<!DOCTYPE html>
<html lang="en">

<head>
    <?php
    include 'common/htmlMeta.inc';
    require_once "config.php";
    require_once 'common.php';
    include 'common/menuHead.inc';
    ?>
    <title><? echo $pageTitle; ?></title>
    <style>
        /* Three independent <table>s below need their Name/Topic and Value
           columns to land at identical pixel x-offsets so the eye can track
           straight down the page - no Bootstrap utility does cross-table
           fixed-width alignment (its w-* classes are %-of-own-table, and
           these tables have different trailing column counts: the User
           table has extra Storage/Actions columns the other two don't).
           Fixed px widths on the two shared leading columns solves it: since
           every table is the same overall width (Bootstrap's .table is
           width:100%), and Name/Value get identical widths everywhere, the
           Last Updated column also starts at the same offset in all three,
           even though its own trailing width differs per table. */
        .variables-aligned-table { table-layout: fixed; width: 100%; }
        .variables-aligned-table .varcol-name { width: 380px; }
        .variables-aligned-table .varcol-copy { width: 40px; }
        .variables-aligned-table .varcol-value { width: 380px; }
        .variables-aligned-table .varcol-eye { width: 46px; }
        .variables-aligned-table .varcol-updated { width: 110px; }
        .variables-aligned-table .varcol-storage { width: 60px; }
        /* table-layout:fixed enforces column widths but doesn't clip
           overflowing cell content on its own (.text-nowrap only stops
           wrapping) - without this, a value cell's appended "(N bytes)" note
           can render past its column and visually collide with the next
           (eye icon) column. */
        .variables-aligned-table td.text-nowrap { overflow: hidden; }

        /* Mobile: the fixed 380px Name+Value columns alone are wider than
           the whole viewport. Fall back to natural sizing and hide the
           Value column outright - visibility:collapse is the correct tool
           for hiding a <col> (unlike display:none on a <td>, it removes the
           column from the table's layout instead of leaving a blank gap),
           so the row reflows to just Name/Topic + the eye icon (forced on
           for every row via JS at this breakpoint, see VariableBreakpoint())
           + Last Updated (+ Storage/Actions on the User table). */
        @media (max-width: 767.98px) {
            .variables-aligned-table { table-layout: auto; }
            .variables-aligned-table .varcol-name,
            .variables-aligned-table .varcol-copy,
            .variables-aligned-table .varcol-eye,
            .variables-aligned-table .varcol-updated,
            .variables-aligned-table .varcol-storage { width: auto; }
            .variables-aligned-table .varcol-value { visibility: collapse; }
        }

        /* Wide screens: don't clip content just because a narrower screen
           would have needed to - both the column width and the JS
           character-truncation caps (VariableBreakpoint() below) scale up
           together at this breakpoint. */
        @media (min-width: 1800px) {
            .variables-aligned-table .varcol-name { width: 700px; }
            .variables-aligned-table .varcol-value { width: 700px; }
        }
    </style>
    <script>

        // Mirrors the <style> media query breakpoints above - the JS
        // character-truncation caps and the "always show the eye icon" mobile
        // behavior need to move in lockstep with the column widths those
        // queries set, or truncation stops matching what the column can
        // actually fit. 'mobile' hides the Value column via CSS entirely
        // (visibility:collapse), so its valueChars doesn't matter, but
        // topicChars still does (Name/Topic stays visible) and forceEye
        // makes every row's eye icon show, since it's the only way left to
        // see a value at all.
        // topicChars leaves headroom below what the fixed-width column could
        // otherwise fit - the Name cell is overflow:hidden (see .text-nowrap
        // rule above), so text sized to fill the column exactly would clip.
        // The copy button itself lives in its own dedicated varcol-copy
        // column (RenderCopyButton), not appended inline after the name, so
        // it lands at the same x-offset on every row regardless of name
        // length.
        var VARIABLE_BREAKPOINTS = {
            mobile: { valueChars: 0, topicChars: 22, forceEye: true },
            normal: { valueChars: 44, topicChars: 42, forceEye: false },
            wide: { valueChars: 80, topicChars: 78, forceEye: false }
        };
        function VariableBreakpoint() {
            var w = window.innerWidth;
            if (w < 768) {
                return VARIABLE_BREAKPOINTS.mobile;
            }
            if (w >= 1800) {
                return VARIABLE_BREAKPOINTS.wide;
            }
            return VARIABLE_BREAKPOINTS.normal;
        }

        // Value cells stay one line regardless of how long the (already-short,
        // backend-capped) value is. A value large enough to be server-truncated
        // doesn't print any of its content inline at all - just a byte count -
        // hover it for a preview (title tooltip, up to the 200-byte server cap)
        // or click View (its own column, see RenderEyeCell) for the exact
        // current full value. Capped short enough to fit the Value column's
        // one line on its own - truncate the text first rather than let the
        // browser wrap it.
        function RenderValueCell(value, truncatedOnServer, size, maxChars) {
            if (maxChars <= 0) {
                return ''; // mobile - the whole column is CSS-hidden, eye icon is the only way to view
            }
            var esc = $('<div>').text(value).html();
            // Both branches below are "clipped" the same way visually - only
            // whether a byte count is appended differs, so a value doesn't
            // flip between "shows a text preview" and "shows nothing but a
            // count" depending on which side of the 200-byte server cutoff
            // it happens to fall on.
            if (value.length > maxChars) {
                var shownFull = $('<div>').text(value.slice(0, maxChars)).html();
                var titleSuffix = truncatedOnServer ? '&hellip;' : '';
                var sizeNote = truncatedOnServer ? " <span class='text-muted'>(" + size + " bytes)</span>" : '';
                return "<span title='" + esc + titleSuffix + "'>" + shownFull + "&hellip;</span>" + sizeNote;
            }
            return "<code>" + esc + "</code>";
        }

        // The eye/View button gets its own unlabeled column (rather than
        // being appended inline after the value text) so it always lands at
        // the same x-offset down the page - only shown when the cell's
        // display is actually clipped (server-truncated, or just longer than
        // the client-side display cap), or forceEye is set (mobile - the
        // Value column itself is hidden there, so the eye is the only way
        // left to see any value regardless of length).
        function RenderEyeCell(name, value, truncatedOnServer, maxChars, forceEye) {
            if (!forceEye && !truncatedOnServer && value.length <= maxChars) {
                return '';
            }
            var escName = name.replace(/"/g, '&quot;');
            return "<button type='button' class='buttons btn-sm' title='View' onclick='ViewVariable(\"" + escName + "\");'><i class='fas fa-eye'></i></button>";
        }

        // Brief inline feedback (swap the icon to a checkmark for a moment)
        // rather than a toast/alert - keeps it lightweight for a button that
        // can appear on many rows at once. Delegated (not inline onclick=""),
        // so the copied text comes from a data-* attribute the browser
        // decodes for us - safe for any name containing a quote/apostrophe,
        // unlike hand-escaping text into an inline onclick string.
        $(document).on('click', '.variableCopyBtn', function () {
            CopyTextToClipboard($(this).attr('data-copy'));
            var $icon = $(this).find('i');
            $icon.removeClass('fa-copy').addClass('fa-check text-success');
            setTimeout(function () {
                $icon.removeClass('fa-check text-success').addClass('fa-copy');
            }, 1000);
        });

        // Truncates from the START, not the end, when over maxChars - for
        // MQTT topics the meaningful/distinguishing part is usually the
        // tail (.../state, .../temperature), while the prefix repeats
        // across dozens of sibling rows and just adds noise when scanning.
        // Full name is still in the title tooltip, still searchable/
        // sortable underneath. The copy button lives in its own dedicated
        // column (RenderCopyButton/varcol-copy) rather than inline after
        // the name, so it lands at the same x-offset on every row
        // regardless of name length.
        function RenderNameCell(name, maxChars) {
            var esc = $('<div>').text(name).html();
            if (name.length <= maxChars) {
                return "<code>" + esc + "</code>";
            }
            var tail = $('<div>').text(name.slice(-maxChars)).html();
            return "<code title='" + esc + "'>&hellip;" + tail + "</code>";
        }

        // Every row gets a copy button, regardless of table - names get
        // pasted into %VAR:name%/%%name%%/If-condition fields elsewhere in
        // the app often enough that retyping-by-hand isn't a safe
        // assumption even for short names.
        function RenderCopyButton(name) {
            var escForAttr = $('<div>').text(name).html().replace(/"/g, '&quot;');
            return "<button type='button' class='buttons btn-sm variableCopyBtn' data-copy=\"" + escForAttr + "\" " +
                "title='Copy full name'><i class='fas fa-copy'></i></button>";
        }

        // Rows are tagged with data-name (the variable/topic name, lowercased)
        // as they're rendered; filtering just toggles .d-none rather than
        // re-fetching, so it stays instant and survives the 3s auto-refresh
        // (each Load*Table() re-applies it after rebuilding its rows).
        var VARIABLE_SEARCH_TABLES = [
            { tbody: '#variablesTableBody', colspan: 7 },
            { tbody: '#fppVariablesTableBody', colspan: 5 },
            { tbody: '#mqttVariablesTableBody', colspan: 5 }
        ];
        function ApplyVariableSearchFilter() {
            var term = $('#variableSearchBox').val().trim().toLowerCase();
            $.each(VARIABLE_SEARCH_TABLES, function (i, t) {
                var $tbody = $(t.tbody);
                var $rows = $tbody.find('tr[data-name]');
                var visibleCount = 0;
                $rows.each(function () {
                    var name = $(this).attr('data-name');
                    var match = term === '' || name.indexOf(term) !== -1;
                    $(this).toggleClass('d-none', !match);
                    if (match) {
                        visibleCount++;
                    }
                });
                $tbody.find('tr.variableSearchNoMatch').remove();
                if (term !== '' && $rows.length && !visibleCount) {
                    $("<tr class='variableSearchNoMatch'><td colspan='" + t.colspan + "' class='text-muted'>" +
                        "No matches for \"" + $('<div>').text(term).html() + "\".</td></tr>").appendTo($tbody);
                }
            });
        }

        // Per-table sort state, applied client-side to the already-fetched
        // data (no re-fetch needed) - clicking a sortable header re-renders
        // that table's rows in the new order and toggles asc/desc on a
        // second click of the same column.
        var VariableSortState = {
            user: { field: 'name', dir: 1 },
            fpp: { field: 'name', dir: 1 },
            mqtt: { field: 'name', dir: 1 }
        };

        function SetVariableSort(table, field) {
            var s = VariableSortState[table];
            if (s.field === field) {
                s.dir = -s.dir;
            } else {
                s.field = field;
                s.dir = 1;
            }
            RenderSortIndicators();
            if (table === 'user') {
                LoadVariablesTable();
            } else if (table === 'fpp') {
                LoadFppVariablesTable();
            } else if (table === 'mqtt') {
                LoadMqttVariablesTable();
            }
        }

        function RenderSortIndicators() {
            $('.sortableHeader').each(function () {
                var s = VariableSortState[$(this).data('table')];
                var isActive = s.field === $(this).data('field');
                $(this).find('.sortIndicator').text(isActive ? (s.dir === 1 ? ' ▲' : ' ▼') : '');
            });
        }

        // Numeric-aware: "2" sorts before "10" instead of after, but falls
        // back to a plain string compare for anything that isn't a bare number
        // (most Values/timestamps-as-text won't be).
        function CompareForSort(va, vb) {
            var na = parseFloat(va), nb = parseFloat(vb);
            var bothNumeric = va !== '' && vb !== '' && !isNaN(na) && !isNaN(nb) &&
                String(na) === va.trim() && String(nb) === vb.trim();
            if (bothNumeric) {
                return na - nb;
            }
            return va < vb ? -1 : (va > vb ? 1 : 0);
        }

        function SortNames(names, data, table) {
            var s = VariableSortState[table];
            var field = s.field, dir = s.dir;
            return names.slice().sort(function (a, b) {
                var va, vb;
                if (field === 'value') {
                    va = data[a].value || '';
                    vb = data[b].value || '';
                } else if (field === 'lastUpdated') {
                    va = data[a].lastUpdated || 0;
                    vb = data[b].lastUpdated || 0;
                    return dir * (va - vb);
                } else {
                    va = a.toLowerCase();
                    vb = b.toLowerCase();
                }
                return dir * CompareForSort(va, vb);
            });
        }

        // Single coarsest-fitting unit (not a combined breakdown), compact
        // form - "8s", "5m", "3h", "2d", never "5 minutes 12 seconds ago".
        // Recomputed for free every 3s since these tables already fully
        // re-render on that timer.
        function FormatVariableTimestamp(unixSeconds) {
            if (!unixSeconds) {
                return '<span class="text-muted">never</span>';
            }
            var diff = Math.max(0, Math.floor(Date.now() / 1000) - unixSeconds);
            var units = [
                ['d', 86400],
                ['h', 3600],
                ['m', 60],
                ['s', 1]
            ];
            for (var i = 0; i < units.length; i++) {
                var count = Math.floor(diff / units[i][1]);
                if (count >= 1 || units[i][0] === 's') {
                    return count + units[i][0];
                }
            }
        }

        function LoadVariablesTable() {
            $.ajax({
                dataType: 'json',
                url: 'api/variables',
                success: function (data) {
                    var $tbody = $('#variablesTableBody');
                    $tbody.empty();
                    var names = SortNames(Object.keys(data || {}), data, 'user');
                    if (!names.length) {
                        $tbody.append(
                            "<tr><td colspan='7' class='text-muted'>No variables defined yet. Use the " +
                            "<b>Set Variable</b> command from a preset, GPIO input, MQTT topic, the API, or " +
                            "a <a href='recurringtasks.php'>Recurring Task</a> to create one.</td></tr>"
                        );
                        return;
                    }
                    var bp = VariableBreakpoint();
                    $.each(names, function (i, name) {
                        var v = data[name];
                        var persistIcon = v.persist
                            ? "<i class='fas fa-save text-info' title='Persisted - survives an fppd restart'></i>"
                            : "<i class='fas fa-memory text-muted' title='In-memory only - lost on an fppd restart'></i>";
                        var escName = name.replace(/"/g, '&quot;');
                        var valueCell = RenderValueCell(v.value, v.truncated, v.size, bp.valueChars);
                        var eyeCell = RenderEyeCell(name, v.value, v.truncated, bp.valueChars, bp.forceEye);
                        var row =
                            "<tr>" +
                            "<td class='text-nowrap'>" + RenderNameCell(name, bp.topicChars) + "</td>" +
                            "<td class='text-center'>" + RenderCopyButton(name) + "</td>" +
                            "<td class='ps-4 text-nowrap'>" + valueCell + "</td>" +
                            "<td class='text-center'>" + eyeCell + "</td>" +
                            "<td>" + FormatVariableTimestamp(v.lastUpdated) + "</td>" +
                            "<td class='text-center'>" + persistIcon + "</td>" +
                            "<td class='ps-4'>" +
                            "<button type='button' class='buttons btn-sm' title='Clear (reset value)' onclick='ClearVariable(\"" + escName + "\");'><i class='fas fa-eraser'></i></button> " +
                            "<button type='button' class='buttons btn-sm' title='Delete (remove entirely)' onclick='DeleteVariable(\"" + escName + "\");'><i class='fas fa-trash text-danger'></i></button>" +
                            "</td>" +
                            "</tr>";
                        $(row).attr('data-name', name.toLowerCase()).appendTo($tbody);
                    });
                    ApplyVariableSearchFilter();
                },
                error: function () {
                    $('#variablesTableBody').html("<tr><td colspan='7' class='text-danger'>Error loading variables.</td></tr>");
                }
            });
        }

        function ClearVariable(name) {
            if (!confirm('Clear variable "' + name + '"? This resets its value (removing any persisted copy on ' +
                'disk) but keeps the row - use Delete instead to remove it entirely.')) {
                return;
            }
            $.ajax({
                type: 'POST',
                url: 'api/variables/' + encodeURIComponent(name) + '?persist=false',
                data: '',
                contentType: 'text/plain',
                complete: function () {
                    LoadVariablesTable();
                }
            });
        }

        function DeleteVariable(name) {
            if (!confirm('Delete variable "' + name + '"? This removes it entirely from the list (not just its ' +
                'value) - it can be recreated later via Set Variable if needed.')) {
                return;
            }
            $.ajax({
                type: 'DELETE',
                url: 'api/variables/' + encodeURIComponent(name),
                complete: function () {
                    LoadVariablesTable();
                }
            });
        }

        var viewVariableSeq = 0;
        function ViewVariable(name) {
            // Fetch fresh rather than reusing the table's cached value - the
            // table row can be stale by the time the user clicks (another
            // periodic task/GPIO/MQTT write may have landed since the last
            // LoadVariablesTable()).
            $.ajax({
                dataType: 'text',
                url: 'api/variables/' + encodeURIComponent(name),
                success: function (value) {
                    ShowVariableValuePopup(name, value);
                },
                error: function () {
                    ShowVariableValuePopup(name, '');
                }
            });
        }

        function ShowVariableValuePopup(name, value) {
            var domId = 'viewVariablePopup_' + (++viewVariableSeq);
            var $popup = $(
                "<div id='" + domId + "'><pre class='text-break m-0' style='white-space:pre-wrap;'></pre></div>"
            ).appendTo('body');
            $popup.find('pre').text(value);
            $popup.fppDialog({
                height: 'auto',
                maxHeight: 500,
                width: 500,
                title: name,
                modal: true,
                open: function () {
                    $popup.parent().find('.ui-dialog-titlebar-close').hide();
                },
                close: function () {
                    $popup.remove();
                },
                buttons: {
                    'Copy to Clipboard': function () {
                        CopyTextToClipboard(value);
                    },
                    'Close': function () {
                        $popup.fppDialog('close');
                    }
                }
            });
        }

        // Fixed meanings for the read-only "fpp_" status variables - unlike
        // user/MQTT variables (arbitrary, no canonical description), these
        // are a known, stable set computed by ComputeFppStatusVariables()
        // (Variables.cpp). Keep in sync with that function.
        var FPP_VARIABLE_DESCRIPTIONS = {
            fpp_status: 'Numeric player status code (0=idle, 1=playing, 2-4=stopping variants, 5=paused).',
            fpp_status_name: 'Player status as text: idle, playing, stopping gracefully, stopping gracefully after loop, stopping now, or paused.',
            fpp_mode_name: 'Current FPP mode, e.g. "player", "bridge", "master", "remote".',
            fpp_volume: 'Current audio output volume (0-100).',
            fpp_multisync: '1 if MultiSync is enabled, 0 otherwise.',
            fpp_uptime_seconds: 'Seconds since fppd started.',
            fpp_is_playing: '1 if a playlist is currently playing, 0 otherwise.',
            fpp_was_scheduled: '1 if the current/most recent playlist was started by the Scheduler rather than manually or via the API, 0 otherwise.',
            fpp_scheduler_enabled: '1 if the Scheduler is enabled, 0 if it has been disabled.',
            fpp_current_time: 'Current local time as HH:MM (24-hour) - same format as the "Time" If-condition Source, so they compare directly.',
            fpp_current_date: 'Current local date as YYYY-MM-DD.',
            fpp_day_of_week: 'Current local day name, e.g. "Monday".',
            fpp_current_month: 'Current local month name, e.g. "July".',
            fpp_warning_count: 'Number of active system warnings.',
            fpp_next_playlist: 'Name of the next Scheduler-triggered playlist, or empty if none is scheduled.',
            fpp_next_playlist_start: 'Start time of the next scheduled playlist.',
            fpp_current_playlist: 'Name of the currently loaded playlist.',
            fpp_current_playlist_count: 'Number of entries in the current playlist.',
            fpp_current_playlist_index: 'Index of the currently playing entry within the current playlist.',
            fpp_current_sequence: 'Filename of the currently playing sequence.',
            fpp_current_song: 'Filename of the currently playing media/song.',
            fpp_seconds_played: 'Seconds played so far in the current playlist entry.',
            fpp_seconds_remaining: 'Seconds remaining in the current playlist entry.',
            fpp_time_elapsed: 'Elapsed time of the current playlist entry, formatted as text.',
            fpp_time_remaining: 'Remaining time of the current playlist entry, formatted as text.',
            fpp_repeat_mode: 'Current playlist repeat mode.',
            fpp_random: 'Current playlist shuffle/random mode.'
        };

        function LoadFppVariablesTable() {
            $.ajax({
                dataType: 'json',
                url: 'api/variables?fpp=true',
                success: function (data) {
                    var $tbody = $('#fppVariablesTableBody');
                    // This table refreshes every 3s (LoadAllVariableTables'
                    // setInterval below) and SetupToolTips() converts each
                    // info icon's title into a Bootstrap tooltip, whose
                    // floating bubble lives outside the trigger element. If a
                    // refresh empties $tbody while one is showing, its
                    // trigger is gone but the bubble is never told to hide -
                    // it's orphaned on screen until the page reloads. Dispose
                    // any live instances on the old rows first so this can't
                    // happen.
                    $tbody.find('[data-bs-toggle="tooltip"]').each(function () {
                        var inst = bootstrap.Tooltip.getInstance(this);
                        if (inst) {
                            inst.dispose();
                        }
                    });
                    $tbody.empty();
                    var names = SortNames(Object.keys(data || {}), data, 'fpp');
                    if (!names.length) {
                        $tbody.append("<tr><td colspan='5' class='text-muted'>None available.</td></tr>");
                        return;
                    }
                    var bp = VariableBreakpoint();
                    $.each(names, function (i, name) {
                        var v = data[name];
                        var valueCell = RenderValueCell(v.value, v.truncated, v.size, bp.valueChars);
                        var eyeCell = RenderEyeCell(name, v.value, v.truncated, bp.valueChars, bp.forceEye);
                        var desc = FPP_VARIABLE_DESCRIPTIONS[name];
                        var descIcon = desc
                            ? "<i class='fas fa-info-circle text-muted me-1' title='" + $('<div>').text(desc).html().replace(/'/g, '&#39;') + "'></i>"
                            : '';
                        var row =
                            "<tr>" +
                            "<td class='text-nowrap'>" + descIcon + RenderNameCell(name, bp.topicChars) + "</td>" +
                            "<td class='text-center'>" + RenderCopyButton(name) + "</td>" +
                            "<td class='ps-4 text-nowrap'>" + valueCell + "</td>" +
                            "<td class='text-center'>" + eyeCell + "</td>" +
                            "<td>" + FormatVariableTimestamp(v.lastUpdated) + "</td>" +
                            "</tr>";
                        $(row).attr('data-name', name.toLowerCase()).appendTo($tbody);
                    });
                    ApplyVariableSearchFilter();
                    // These rows (and their info-icon title attributes) are
                    // added after the page's one-time SetupToolTips() pass
                    // (fpp.js, called on initial load only) already ran, so
                    // without re-running it here the icons' titles never get
                    // converted into (or shown as) Bootstrap tooltips - same
                    // reason co-ledPanels.php/schedulePreview.php/
                    // multisync.php/settings.php all re-call it after their
                    // own dynamic re-renders.
                    SetupToolTips();
                },
                error: function () {
                    $('#fppVariablesTableBody').html("<tr><td colspan='5' class='text-danger'>Error loading FPP variables.</td></tr>");
                }
            });
        }

        function LoadMqttVariablesTable() {
            $.ajax({
                dataType: 'json',
                url: 'api/variables?mqtt=true',
                success: function (data) {
                    var $tbody = $('#mqttVariablesTableBody');
                    $tbody.empty();
                    var names = SortNames(Object.keys(data || {}), data, 'mqtt');
                    if (!names.length) {
                        $tbody.append(
                            "<tr><td colspan='5' class='text-muted'>No MQTT messages cached yet. Go to " +
                            "<a href='settings-mqtt.php'>MQTT Settings</a> to connect to a broker and " +
                            "subscribe to a topic (or <code>#</code> for everything) - any topic this " +
                            "device receives a message on will appear here.</td></tr>"
                        );
                        return;
                    }
                    var bp = VariableBreakpoint();
                    $.each(names, function (i, name) {
                        var v = data[name];
                        var valueCell = RenderValueCell(v.value, v.truncated, v.size, bp.valueChars);
                        var eyeCell = RenderEyeCell(name, v.value, v.truncated, bp.valueChars, bp.forceEye);
                        var row =
                            "<tr>" +
                            "<td class='align-top text-nowrap'>" + RenderNameCell(name, bp.topicChars) + "</td>" +
                            "<td class='align-top text-center'>" + RenderCopyButton(name) + "</td>" +
                            "<td class='ps-4 align-top text-nowrap'>" + valueCell + "</td>" +
                            "<td class='align-top text-center'>" + eyeCell + "</td>" +
                            "<td class='align-top'>" + FormatVariableTimestamp(v.lastUpdated) + "</td>" +
                            "</tr>";
                        $(row).attr('data-name', name.toLowerCase()).appendTo($tbody);
                    });
                    ApplyVariableSearchFilter();
                },
                error: function () {
                    $('#mqttVariablesTableBody').html("<tr><td colspan='5' class='text-danger'>Error loading MQTT variables.</td></tr>");
                }
            });
        }

        function LoadAllVariableTables() {
            LoadVariablesTable();
            LoadFppVariablesTable();
            LoadMqttVariablesTable();
        }

        function pageSpecific_PageLoad_PostDOMLoad_ActionsSetup() {
            $('#variableSearchBox').on('input', ApplyVariableSearchFilter);
            $(document).on('click', '.sortableHeader', function () {
                SetVariableSort($(this).data('table'), $(this).data('field'));
            });
            RenderSortIndicators();
            LoadAllVariableTables();
            // The listing endpoints only ever return short values (or a
            // capped preview + a View button for large ones - see
            // kInlineValueMaxBytes in Variables.cpp), so it's safe to keep
            // these fresh automatically without a manual Refresh.
            setInterval(LoadAllVariableTables, 3000);

            // Re-render (not just re-fetch) on a real breakpoint crossing,
            // not every resize pixel - the truncation caps and forced-eye
            // behavior in VariableBreakpoint() only change at those points.
            var lastVariableBreakpoint = VariableBreakpoint();
            var variableResizeTimer = null;
            $(window).on('resize', function () {
                clearTimeout(variableResizeTimer);
                variableResizeTimer = setTimeout(function () {
                    var bp = VariableBreakpoint();
                    if (bp !== lastVariableBreakpoint) {
                        lastVariableBreakpoint = bp;
                        LoadAllVariableTables();
                    }
                }, 250);
            });
        }

    </script>
</head>

<body>
    <div id="bodyWrapper">
        <?php
        $activeParentMenuItem = 'content';
        include 'menu.inc'; ?>
        <div class="mainContainer">
            <h1 class="title">Variables</h1>
            <div class="pageContent">
                <input type="text" id="variableSearchBox" class="form-control mb-3" placeholder="Search variable/topic names...">
                <div class="text-muted mb-3">
                    <b>User Variables</b> are named values you set with the <b>Set Variable</b> command (from a
                    preset, GPIO input, MQTT topic, scheduler entry, the API, or
                    <a href="recurringtasks.php">Recurring Tasks</a>) and read back anywhere via
                    <code>%VAR:name%</code> or the <code>If</code> command.
                </div>
                <table class="table table-striped variables-aligned-table">
                    <colgroup>
                        <col class="varcol-name"><col class="varcol-copy"><col class="varcol-value"><col class="varcol-eye"><col class="varcol-updated"><col class="varcol-storage"><col>
                    </colgroup>
                    <thead>
                        <tr>
                            <th><a href="javascript:void(0)" class="text-decoration-none text-reset sortableHeader" data-table="user" data-field="name">Name<span class="sortIndicator"></span></a></th>
                            <th></th>
                            <th class="ps-4"><a href="javascript:void(0)" class="text-decoration-none text-reset sortableHeader" data-table="user" data-field="value">Value<span class="sortIndicator"></span></a></th>
                            <th></th>
                            <th><a href="javascript:void(0)" class="text-decoration-none text-reset sortableHeader" data-table="user" data-field="lastUpdated">Last Updated<span class="sortIndicator"></span></a></th>
                            <th class="text-center">Storage</th>
                            <th></th>
                        </tr>
                    </thead>
                    <tbody id="variablesTableBody">
                        <tr>
                            <td colspan="7" class="text-muted">Loading...</td>
                        </tr>
                    </tbody>
                </table>

                <div class="text-muted mb-2 mt-2">
                    <b>FPP Read-only Variables</b> - These can be used the same way as User Variables above.
                </div>
                <table class="table table-striped variables-aligned-table">
                    <colgroup>
                        <col class="varcol-name"><col class="varcol-copy"><col class="varcol-value"><col class="varcol-eye"><col>
                    </colgroup>
                    <thead>
                        <tr>
                            <th><a href="javascript:void(0)" class="text-decoration-none text-reset sortableHeader" data-table="fpp" data-field="name">Name<span class="sortIndicator"></span></a></th>
                            <th></th>
                            <th class="ps-4"><a href="javascript:void(0)" class="text-decoration-none text-reset sortableHeader" data-table="fpp" data-field="value">Value<span class="sortIndicator"></span></a></th>
                            <th></th>
                            <th><a href="javascript:void(0)" class="text-decoration-none text-reset sortableHeader" data-table="fpp" data-field="lastUpdated">Last Updated<span class="sortIndicator"></span></a></th>
                        </tr>
                    </thead>
                    <tbody id="fppVariablesTableBody">
                        <tr>
                            <td colspan="5" class="text-muted">Loading...</td>
                        </tr>
                    </tbody>
                </table>

                <div class="text-muted mb-2 mt-2">
                    <b>MQTT Read-only Variables</b> - These can be used the same way as User Variables
                    above (exposed as <code>mqtt-&lt;topic&gt;</code>), including inside an Expression field.
                </div>
                <div class="table-responsive">
                    <table class="table table-striped variables-aligned-table">
                        <colgroup>
                            <col class="varcol-name"><col class="varcol-copy"><col class="varcol-value"><col class="varcol-eye"><col>
                        </colgroup>
                        <thead>
                            <tr>
                                <th><a href="javascript:void(0)" class="text-decoration-none text-reset sortableHeader" data-table="mqtt" data-field="name">Name<span class="sortIndicator"></span></a></th>
                                <th></th>
                                <th class="ps-4"><a href="javascript:void(0)" class="text-decoration-none text-reset sortableHeader" data-table="mqtt" data-field="value">Value<span class="sortIndicator"></span></a></th>
                                <th></th>
                                <th><a href="javascript:void(0)" class="text-decoration-none text-reset sortableHeader" data-table="mqtt" data-field="lastUpdated">Last Updated<span class="sortIndicator"></span></a></th>
                            </tr>
                        </thead>
                        <tbody id="mqttVariablesTableBody">
                            <tr>
                                <td colspan="5" class="text-muted">Loading...</td>
                            </tr>
                        </tbody>
                    </table>
                </div>
            </div>
        </div>
        <?php include 'common/footer.inc'; ?>
    </div>
</body>

</html>
