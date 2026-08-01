<!DOCTYPE html>
<html lang="en">

<head>
    <?php
    require_once 'common/htmlMeta.inc';
    require_once 'config.php';
    require_once 'common/menuHead.inc';
    require_once 'common.php';
    ?>
    <title><? echo $pageTitle; ?></title>
    <script>

        function AddTask (task = {}) {
            var row = AddTableRowFromTemplate('tblTasksBody');
            FillInTaskRow(row, task);
        }

        // Fetched once (like fpp.js's commandList) and reused for every row's
        // .ptTmplPreset <select> - mirrors FillInCommandTemplate's handling
        // of .cmdTmplCommand (LoadCommandList) so a saved-but-now-missing
        // preset shows up as a disabled "(unavailable)" option instead of
        // silently losing the row's actual saved value.
        var presetNameList = null;
        function PopulatePresetSelect ($select, currentValue) {
            if (presetNameList === null) {
                $.ajax({
                    dataType: 'json',
                    url: 'api/commandPresets?names=true',
                    async: false,
                    success: function (data) {
                        presetNameList = data || [];
                    }
                });
            }
            $select.empty();
            $select.append("<option value=''></option>");
            $.each(presetNameList, function (i, name) {
                var esc = $('<div>').text(name).html().replace(/'/g, '&#39;');
                $select.append("<option value='" + esc + "'>" + esc + '</option>');
            });
            if (currentValue && presetNameList.indexOf(currentValue) === -1) {
                var escMissing = $('<div>').text(currentValue).html().replace(/'/g, '&#39;');
                $select.prepend(
                    "<option class='invalidPresetOption' value='" + escMissing +
                    "' disabled>" + escMissing + ' (unavailable)</option>'
                );
            }
        }

        function TaskTypeChanged (row) {
            var type = row.find('.ptTmplType').val();
            var isPreset = type === 'preset';
            row.find('.ptTmplPresetCell').toggleClass('d-none', !isPreset);
            row.find('.ptTmplCommandCell').toggleClass('d-none', isPreset);
            if (isPreset) {
                // A Command Preset can fire any number of commands at once
                // (CommandManager::TriggerPreset) - there's no single result
                // to test-run or view, so both are hidden for this type (see
                // RecurringTasks.cpp's TestRunTask, which returns a "Command
                // Preset tasks have no output to filter" note rather than a
                // real result for this type).
                row.find('.ptTmplPresetCell .ptTmplTestRun').addClass('d-none');
                row.find('.ptTmplViewVar').addClass('d-none');
            } else {
                row.find('.ptTmplPresetCell .ptTmplTestRun').removeClass('d-none');
                // Restored to whatever LoadTaskStatus's resultVariable check
                // last decided, not forced visible - a freshly-switched
                // "FPP Command" row with no Result Variable set still has
                // nothing to view.
                LoadTaskStatus();
            }
        }

        var MIN_TASK_INTERVAL_SEC = 30;

        // Shared by the initial FillInTaskRow() fill and the live 'input'
        // listener below - a loaded task with a bad/short saved value shows
        // the red highlight immediately, not just after the user touches
        // the field. See .intervalInvalid above for why this is a filled
        // class toggle rather than fpp.js's plain-border validateNumber()
        // convention - a bare border didn't read clearly in this table.
        function UpdateIntervalField (input) {
            var sec = parseInt(input.value, 10);
            input.classList.toggle('intervalInvalid', isNaN(sec) || sec < MIN_TASK_INTERVAL_SEC);
        }

        function FilterTypeChanged (row) {
            var type = row.find('.ptTmplFilterType').val();
            row.find('.ptTmplFilterJsonRow').toggleClass('d-none', type !== 'json');
            row.find('.ptTmplFilterBetweenRow').toggleClass('d-none', type !== 'between');
            row.find('.ptTmplFilterRegexRow').toggleClass('d-none', type !== 'regex');
        }

        function FillInTaskRow (row, data) {
            row.find('.ptTmplEnabled').prop('checked', data.enabled !== false);
            row.find('.ptTmplName').val(data.name || '');

            var sec = data.intervalMS ? Math.round(data.intervalMS / 1000) : 60;
            row.find('.ptTmplIntervalSec').val(sec);
            UpdateIntervalField(row.find('.ptTmplIntervalSec')[0]);

            row.find('.ptTmplType').val(data.type === 'command' ? 'command' : 'preset');
            var $presetSelect = row.find('.ptTmplPreset');
            if ($presetSelect.find('option').length === 0) {
                PopulatePresetSelect($presetSelect, data.preset || '');
            }
            $presetSelect.val(data.preset || '');
            row.find('.ptTmplResultVariable').val(data.resultVariable || '');
            row.find('.ptTmplPersist').prop('checked', !!data.persistResult);
            row.find('.ptTmplFilterType').val(data.filterType || 'none');
            row.find('.ptTmplFilterJsonPath').val(data.filterJsonPath || '');
            row.find('.ptTmplFilterBetweenStart').val(data.filterBetweenStart || '');
            row.find('.ptTmplFilterBetweenEnd').val(data.filterBetweenEnd || '');
            row.find('.ptTmplFilterRegex').val(data.filterRegex || '');

            // FillInCommandTemplate/GetCommandTemplateData are the same
            // helpers commandPresets.php and gpio.php use for their command
            // rows (www/js/fpp.js) - reused as-is here for the "FPP Command"
            // task type, including the Edit modal and args preview.
            FillInCommandTemplate(row, { command: data.command || '', args: data.args || [] });

            TaskTypeChanged(row);
            FilterTypeChanged(row);
            UpdateTaskAdvancedSummary(row);

            // One-time init, same as fpp.js's InitArgHelpTooltips - these are
            // the row's own real elements (AppendTaskAdvancedSectionToCommandEditor
            // relocates them into the shared command editor dialog and back,
            // never clones them), so binding once here covers both places
            // they ever appear.
            if (typeof bootstrap !== 'undefined' && bootstrap.Tooltip) {
                row.find('.argHelpIcon').each(function () {
                    new bootstrap.Tooltip(this);
                });
            }
        }

        // One-line stand-in for the Result Variable/Persist/Filter fields
        // (edited via the same popup as the command itself - see
        // EditTaskCommandAndSettings) so a task row stays a single compact
        // line instead of the ~5-line stack those fields used to always
        // show inline.
        function UpdateTaskAdvancedSummary (row) {
            var resultVar = row.find('.ptTmplResultVariable').val().trim();
            var filterType = row.find('.ptTmplFilterType').val();
            var parts = [];
            parts.push(resultVar
                ? ('&rarr; <code>' + $('<div>').text(resultVar).html() + '</code>')
                : 'no result variable');
            if (filterType && filterType !== 'none') {
                parts.push('filter: ' + filterType);
            }
            row.find('.ptTmplAdvancedSummary').html(parts.join(', '));
        }

        // Opens the shared FPP Command editor (ShowCommandEditor, the same
        // singleton dialog commandPresets.php/gpio.php use) via
        // EditCommandTemplate, and - once its own fields have loaded -
        // appends our Result Variable/Persist/Filter fields as a second
        // section in that SAME dialog rather than nesting a separate one.
        // Nesting a dialog inside this singleton is a documented past bug
        // (silently corrupts the outer dialog's state - see cerebrum
        // Do-Not-Repeat), so this only works because it's one dialog with
        // extra appended content, never two stacked dialogs.
        function EditTaskCommandAndSettings (row) {
            EditCommandTemplate(row, function () {
                AppendTaskAdvancedSectionToCommandEditor(row);
            });
        }

        // The fields aren't cloned/duplicated - they're the exact same real
        // <input>s GetTaskRowData()/SaveTasks() already read via
        // row.find(...), just temporarily moved into the command editor
        // dialog and moved back (to td.ptTmplCommandTd, their original home)
        // when it closes. Safe to move out of the row's DOM subtree because
        // a modal dialog blocks interaction with the rest of the page -
        // nothing else can call row.find('.ptTmplResultVariable') etc.
        // while they're relocated.
        function AppendTaskAdvancedSectionToCommandEditor (row) {
            var $fields = row.find('.ptTmplAdvancedFields');
            var $section = $(
                "<div class='ptTmplAdvancedSectionWrapper border-top mt-3 pt-3'>" +
                "<h5 class='mb-2'>Result Variable / Filter</h5></div>"
            );
            $section.append($fields.removeClass('d-none'));
            // commandEditor.php's own markup is: ...command fields table...
            // <hr> <button row>. Insert before that <hr> (found as the
            // button row's previous sibling), not before the button row
            // itself, so the existing <hr> stays doing its one job -
            // separating all the editable content above it from the
            // buttons - instead of ending up sandwiched between our
            // section and the buttons.
            $('#btnSaveEditorCommand').closest('div').prev('hr').before($section);

            // CommandEditorSave()/Cancel() both close #commandEditorPopup
            // regardless of which was clicked - one listener on the dialog's
            // own close event covers both paths, same as a Save/Cancel pair.
            $('#commandEditorPopup').one('hidden.bs.modal', function () {
                row.find('td.ptTmplCommandTd .ptTmplCommandCell').append($fields.addClass('d-none'));
                $section.remove();
                UpdateTaskAdvancedSummary(row);
            });
        }

        function GetTaskRowData (row) {
            var task = {};
            task.name = row.find('.ptTmplName').val().trim();
            task.enabled = row.find('.ptTmplEnabled').is(':checked');

            var sec = parseInt(row.find('.ptTmplIntervalSec').val(), 10);
            task.intervalMS = (isNaN(sec) || sec < 30) ? 60000 : sec * 1000;

            task.type = row.find('.ptTmplType').val();
            if (task.type === 'preset') {
                task.preset = row.find('.ptTmplPreset').val().trim();
                task.command = '';
                task.args = [];
                task.resultVariable = '';
                task.persistResult = false;
            } else {
                var cmd = GetCommandTemplateData(row);
                task.preset = '';
                task.command = cmd.command;
                task.args = cmd.args;
                task.resultVariable = row.find('.ptTmplResultVariable').val().trim();
                task.persistResult = row.find('.ptTmplPersist').is(':checked');
                var filterType = row.find('.ptTmplFilterType').val();
                task.filterType = filterType === 'none' ? '' : filterType;
                task.filterJsonPath = row.find('.ptTmplFilterJsonPath').val().trim();
                task.filterBetweenStart = row.find('.ptTmplFilterBetweenStart').val();
                task.filterBetweenEnd = row.find('.ptTmplFilterBetweenEnd').val();
                task.filterRegex = row.find('.ptTmplFilterRegex').val().trim();
            }
            return task;
        }

        function TestRunTask (row) {
            var task = GetTaskRowData(row);
            if (task.type === 'command' && task.command === '') {
                alert('Select an FPP Command first.');
                return;
            }
            if (task.type === 'preset' && task.preset === '') {
                alert('Select a Command Preset first.');
                return;
            }
            // Runs against the row's current (possibly unsaved) data, not a
            // saved task by name - see TestRunTask() in RecurringTasks.cpp.
            // Searches the whole row (not just .ptTmplCommandCell) since
            // Command Preset-type rows have their own Test Run button in
            // .ptTmplPresetCell instead.
            var $btn = row.find("input[value='Test Run']:visible");
            var origVal = $btn.val();
            $btn.val('Running...').prop('disabled', true);
            $.ajax({
                type: 'POST',
                url: 'api/fppd/recurringtasks',
                contentType: 'application/json',
                data: JSON.stringify({ command: 'test', task: task }),
                dataType: 'json',
                success: function (data) {
                    ShowTestRunPopup(task.name || '(unnamed task)', data);
                    LoadTaskStatus();
                },
                error: function (...args) {
                    DialogError('Test Run failed', 'api/fppd/recurringtasks call failed' + show_details(args));
                },
                complete: function () {
                    $btn.val(origVal).prop('disabled', false);
                }
            });
        }

        var testRunPopupSeq = 0;
        function ShowTestRunPopup (name, data) {
            var domId = 'testRunPopup_' + (++testRunPopupSeq);
            var body = '';
            if (data.note) {
                body += "<div class='text-muted mb-2'>" + $('<div>').text(data.note).html() + '</div>';
            }
            if (!data.ok) {
                body += "<div class='text-danger mb-2'><b>Error:</b> " + $('<div>').text(data.error || 'unknown error').html() + '</div>';
            } else {
                body += "<b>Raw result:</b><pre class='testRunRaw text-break border overflow-auto p-1' style='white-space:pre-wrap;max-height:150px;'></pre>";
                body += "<b>Filtered (&rarr; Variable):</b><pre class='testRunFiltered text-break border overflow-auto p-1' style='white-space:pre-wrap;max-height:150px;'></pre>";
            }
            var $popup = $("<div id='" + domId + "'>" + body + '</div>').appendTo('body');
            if (data.ok) {
                $popup.find('.testRunRaw').text(data.raw || '(empty)');
                $popup.find('.testRunFiltered').text(data.filtered || '(empty)');
            }
            $popup.fppDialog({
                height: 'auto',
                maxHeight: 500,
                width: 500,
                title: 'Test Run: ' + name,
                modal: true,
                open: function () {
                    $popup.parent().find('.ui-dialog-titlebar-close').hide();
                },
                close: function () {
                    $popup.remove();
                },
                buttons: {
                    'Close': function () {
                        $popup.fppDialog('close');
                    }
                }
            });
        }

        // Status only ever shows OK/Error + a timestamp - never what the
        // task actually wrote, so confirming the data pump produces what
        // you expect meant leaving this page for variables.php. The eye
        // icon next to Status (shown whenever the task has a resultVariable
        // configured) fetches from GET api/variables (the listing, for its
        // lastUpdated) rather than the plain-text api/variables/{name}
        // variables.php's own View button uses - see ViewTaskResultVariable.
        // Single-value FormatVariableTimestamp equivalent (variables.php's
        // version isn't reachable from here - separate page, separate
        // <script>). Kept in lockstep with that one intentionally: same
        // "8s"/"5m"/"3h"/"2d" compact single-unit format.
        function FormatTaskVarTimestamp (unixSeconds) {
            if (!unixSeconds) {
                return 'never';
            }
            var diff = Math.max(0, Math.floor(Date.now() / 1000) - unixSeconds);
            var units = [['d', 86400], ['h', 3600], ['m', 60], ['s', 1]];
            for (var i = 0; i < units.length; i++) {
                var count = Math.floor(diff / units[i][1]);
                if (count >= 1 || units[i][0] === 's') {
                    return count + units[i][0] + ' ago';
                }
            }
        }

        var viewTaskVarPopupSeq = 0;
        // Fetches the JSON listing (api/variables), not the plain-text
        // api/variables/{name} - the listing also carries lastUpdated, which
        // this popup needs to show staleness. Without it, a Test Run that
        // errors (writes nothing) followed by View looks identical to a
        // fresh, just-written value - see ShowTaskVariablePopup.
        function ViewTaskResultVariable (name) {
            $.ajax({
                dataType: 'json',
                url: 'api/variables',
                success: function (data) {
                    var v = (data && data[name]) || {};
                    ShowTaskVariablePopup(name, v.value || '', v.lastUpdated);
                },
                error: function () {
                    ShowTaskVariablePopup(name, '', 0);
                }
            });
        }

        function ShowTaskVariablePopup (name, value, lastUpdated) {
            var domId = 'taskVarPopup_' + (++viewTaskVarPopupSeq);
            var $popup = $(
                "<div id='" + domId + "'>" +
                "<div class='text-muted mb-2'>Last updated: " + FormatTaskVarTimestamp(lastUpdated) +
                " - this is the Variable's current value, which may predate the most recent Test Run " +
                "if that run errored or hasn't been saved yet.</div>" +
                "<pre class='text-break m-0' style='white-space:pre-wrap;'></pre></div>"
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
                        navigator.clipboard.writeText(value);
                    },
                    'Close': function () {
                        $popup.fppDialog('close');
                    }
                }
            });
        }

        function LoadTasks () {
            $.ajax({
                dataType: 'json',
                url: 'api/configfile/recurringtasks.json',
                success: function (data) {
                    $('#tblTasksBody').empty();
                    if (Array.isArray(data)) {
                        $.each(data, function (i, t) {
                            AddTask(t);
                        });
                    }
                    LoadTaskStatus();
                },
                error: function () {
                    // No recurringtasks.json saved yet - start with an empty table.
                    $('#tblTasksBody').empty();
                }
            });
        }

        function LoadTaskStatus () {
            $.ajax({
                dataType: 'json',
                url: 'api/fppd/recurringtasks',
                success: function (data) {
                    var tasks = (data && data.tasks) || [];
                    var byName = {};
                    $.each(tasks, function (i, t) {
                        byName[t.name] = t;
                    });
                    $('#tblTasksBody > tr').each(function () {
                        var name = $(this).find('.ptTmplName').val();
                        var $status = $(this).find('.ptTmplStatus');
                        var $eye = $(this).find('.ptTmplViewVar');
                        var t = byName[name];

                        // Shown whenever the task targets a Variable at all,
                        // regardless of hasRun - lets you check what's
                        // currently there even before/without ever running
                        // this task (e.g. another task/preset already wrote
                        // to the same Variable).
                        if (t && t.resultVariable) {
                            $eye.removeClass('d-none').attr('data-varname', t.resultVariable);
                        } else {
                            $eye.addClass('d-none');
                        }

                        if (!t || !t.hasRun) {
                            $status.removeClass('text-success text-danger').addClass('text-muted').text('never run').attr('title', '');
                            return;
                        }
                        var when = new Date(t.lastRunMS).toLocaleString();
                        if (t.lastOk) {
                            $status.removeClass('text-muted text-danger').addClass('text-success').text('OK @ ' + when).attr('title', '');
                        } else {
                            $status.removeClass('text-muted text-success').addClass('text-danger')
                                .text('Error @ ' + when)
                                .attr('title', t.lastError || '');
                        }
                    });
                }
            });
        }

        function SaveTasks () {
            var tasks = [];
            var errors = [];
            var names = {};

            $('#tblTasksBody > tr').each(function () {
                var task = GetTaskRowData($(this));
                if (task.name === '') {
                    errors.push('A task is missing a Name and will not be saved.');
                    return;
                }
                if (names.hasOwnProperty(task.name)) {
                    errors.push('Task name "' + task.name + '" is used more than once - names must be unique.');
                    return;
                }
                if (task.type === 'preset' && task.preset === '') {
                    errors.push('Task "' + task.name + '" has no Command Preset selected.');
                    return;
                }
                if (task.type === 'command' && task.command === '') {
                    errors.push('Task "' + task.name + '" has no FPP Command selected.');
                    return;
                }
                names[task.name] = true;
                tasks.push(task);
            });

            if (errors.length > 0) {
                alert(errors.join('\n'));
                return;
            }

            var json = JSON.stringify(tasks, null, 2);
            var result = Post('api/configfile/recurringtasks.json', false, json);

            if (!result.hasOwnProperty('Status') || (result['Status'] != 'OK')) {
                alert('Error saving recurring tasks!');
                return;
            }

            // Config-driven timers only get (re-)scheduled at fppd startup or
            // on an explicit reload - ask fppd to pick up the just-saved file
            // now so the change is live without a restart.
            $.ajax({
                type: 'POST',
                url: 'api/fppd/recurringtasks',
                contentType: 'application/json',
                data: JSON.stringify({ command: 'reload' }),
                complete: function () {
                    $.jGrowl('Recurring tasks saved and reloaded.', { themeState: 'success' });
                    LoadTaskStatus();
                }
            });
        }

        $(document).ready(function () {
            $('#tblTasksBody').on('mousedown', 'tr', function (event, ui) {
                HandleTableRowMouseClick(event, $(this));

                if ($('#tblTasksBody > tr.selectedEntry').length > 0) {
                    EnableButtonClass('deleteTaskButton');
                } else {
                    DisableButtonClass('deleteTaskButton');
                }
            });
            $('#tblTasksBody').on('click', '.ptTmplViewVar', function (event) {
                event.stopPropagation(); // don't also trigger the row's own mousedown-select handler
                ViewTaskResultVariable($(this).attr('data-varname'));
            });
            $('#tblTasksBody').on('input', '.ptTmplIntervalSec', function () {
                UpdateIntervalField(this);
            });
        });

        // Only real (writable) User Variables belong here - fpp-/mqtt-
        // names are read-only and would be rejected if actually used as a
        // Result Variable target, so suggesting them would be misleading.
        function LoadResultVariableNames () {
            $.ajax({
                url: 'api/variables',
                dataType: 'json',
                success: function (data) {
                    var names = Object.keys(data || {}).sort();
                    var $list = $('#ptResultVariableNames').empty();
                    $.each(names, function (i, name) {
                        $list.append("<option value='" + $('<div>').text(name).html() + "'>");
                    });
                }
            });
        }

        function pageSpecific_PageLoad_PostDOMLoad_ActionsSetup () {
            // Populates the shared commandListByName cache (www/js/fpp.js)
            // used by FillInCommandTemplate's "is this command still
            // available" check - same call commandPresets.php makes, just
            // against a throwaway select since this page doesn't otherwise
            // need one.
            LoadCommandList($('<select>'));
            LoadResultVariableNames();
            LoadTasks();
        }

    </script>
</head>

<body>
    <div id="bodyWrapper">
        <?php
        $activeParentMenuItem = 'content';
        include 'menu.inc'; ?>
        <div class="mainContainer">
            <h1 class="title">Recurring Tasks</h1>
            <div class="pageContent">
                <div class="callout">
                    <p>
                        Recurring Tasks fetch data <i>ahead of time</i> so it's already sitting in a
                        <a href="variables.php">User Variable</a> when something time-critical needs it -
                        a slow URL fetch or script run during a playlist or GPIO event would add a delay
                        right when it matters most.
                    </p>
                    <p>
                        Each task runs a Command Preset, or any single FPP Command, on a fixed interval,
                        and a <b>FPP Command</b> task can optionally land its result into a Variable. An
                        <code>If</code> command elsewhere then reads that Variable instantly, with no
                        fetch delay of its own.
                    </p>
                    <p class="mb-0">
                        Example: a <code>URL</code> command polling a weather API or a sensor's web
                        endpoint every few minutes.
                    </p>
                </div>
                <div id="recurringTasks" class="settings">
                    <div class="row tablePageHeader">
                        <div class="col-auto ms-auto">
                            <div class="form-actions form-actions-primary">
                                <div><button class="disableButtons deleteTaskButton"
                                        data-btn-enabled-class="btn-outline-danger" type="button" value="Delete"
                                        onClick="DeleteSelectedEntries('tblTasksBody'); DisableButtonClass('deleteTaskButton');">Delete</button>
                                </div>
                                <div><button class="buttons btn-outline-success form-actions-button-primary ms-1"
                                        type="button" onClick="AddTask();"><i class="fas fa-plus"></i>
                                        Add</button></div>
                                <div><button class="buttons btn-success form-actions-button-primary" type='button'
                                        value="Save" onClick='SaveTasks();'>Save</button></div>
                            </div>
                        </div>
                    </div>
                    <hr>
                    <style>
                        #tblTasksBody > tr > td {
                            vertical-align: top;
                        }
                        .ptTmplResultVariableRow {
                            display: block;
                            margin-top: 4px;
                        }
                        /* Edit/Test Run (and the FPP Command cell's tooltip
                           icon) always drop to their own line below the
                           select, rather than wrapping unpredictably next to
                           it at different table widths - keeps the
                           Preset/Command column's height (and where its
                           buttons land) consistent across screen sizes. */
                        .ptTmplButtonsRow {
                            display: block;
                            margin-top: 4px;
                        }
                        /* Matches #tblCommandEditor's own label-column rule
                           (commandEditor.php) so the Result Variable/Filter
                           section lines up the same way as the command
                           fields above it in the same popup. */
                        .ptTmplAdvancedTable td:first-child {
                            padding-right: 8px;
                            width: 25%;
                        }
                        /* A plain 2px red border (fpp.js's own
                           validateNumber()/IP-field convention) reads as just
                           another border in a table this dense. Filled
                           instead, using the same theme-aware Bootstrap
                           danger tokens .alert-danger already uses elsewhere
                           in FPP (correct in both light and dark mode,
                           unlike a hardcoded color) so it's unmistakable at
                           a glance without inventing a new visual language. */
                        .ptTmplIntervalSec.intervalInvalid {
                            background-color: var(--bs-danger-bg-subtle);
                            border-color: var(--bs-danger-border-subtle);
                            color: var(--bs-danger-text-emphasis);
                        }
                    </style>
                    <div class='fppTableWrapper fppTableWrapperAsTable'>
                        <div class='fppTableContents' role="region" aria-labelledby="tblTasksHead" tabindex="0">
                            <table class='fppTableRowTemplate template-tblTasksBody'>
                                <tr>
                                    <td class='center'><input type='checkbox' class='ptTmplEnabled' checked></td>
                                    <td><input type='text' size='14' maxlength='64' class='ptTmplName'></td>
                                    <td><input type='number' min='30' step='1' size='6' value='60'
                                            class='ptTmplIntervalSec'></td>
                                    <td>
                                        <select class='ptTmplType'
                                            onChange='TaskTypeChanged($(this).parent().parent());'>
                                            <option value='preset'>Command Preset</option>
                                            <option value='command'>FPP Command</option>
                                        </select>
                                    </td>
                                    <td class='ptTmplCommandTd'>
                                    <span class='ptTmplPresetCell'>
                                        <select class='ptTmplPreset'></select>
                                        <span class='ptTmplButtonsRow'>
                                            <input type='button' class='buttons smallButton ptTmplTestRun' value='Test Run'
                                                onClick='TestRunTask($(this).closest("tr"));'>
                                        </span>
                                    </span>
                                    <span class='ptTmplCommandCell d-none'>
                                        <select class='cmdTmplCommand'
                                            onChange='EditTaskCommandAndSettings($(this).closest("tr"));'></select>
                                        <span class='ptTmplButtonsRow'>
                                            <input type='button' class='buttons reallySmallButton' value='Edit'
                                                onClick='EditTaskCommandAndSettings($(this).closest("tr"));'>
                                            <input type='button' class='buttons smallButton' value='Test Run'
                                                onClick='TestRunTask($(this).closest("tr"));'>
                                            <img class='cmdTmplTooltipIcon' data-bs-html='true' data-bs-toggle='tooltip'
                                                title='' src='images/redesign/help-icon.svg' width=22 height=22>
                                        </span>
                                        <table class='cmdTmplArgsTable'>
                                            <tr>
                                                <th class='left'>Args:</th>
                                                <td><span class='cmdTmplArgs'></span></td>
                                            </tr>
                                        </table>
                                        <span class='cmdTmplJSON d-none'></span>
                                        <div class='ptTmplResultVariableRow'>
                                            <span class='ptTmplAdvancedSummary text-muted'></span>
                                        </div>
                                        <div class='ptTmplAdvancedFields d-none'>
                                            <table width='100%' class='ptTmplAdvancedTable settingsTable'>
                                                <tr>
                                                    <td>Result &rarr; Variable:
                                                        <span class='argHelpIcon' data-bs-toggle='tooltip' data-bs-html='true' data-bs-placement='auto'
                                                            data-bs-title="The Variable this task's result (optionally filtered below) is saved into. Leave blank to run the command without saving anything. Overwrites an existing Variable of the same name.">
                                                            <img src='images/redesign/help-icon.svg' class='icon-help' alt='Help icon'></span>
                                                    </td>
                                                    <td><input type='text' size='16' class='ptTmplResultVariable'
                                                            placeholder='(optional)' list='ptResultVariableNames'></td>
                                                </tr>
                                                <tr>
                                                    <td>Persist:
                                                        <span class='argHelpIcon' data-bs-toggle='tooltip' data-bs-html='true' data-bs-placement='auto'
                                                            data-bs-title='If checked, the Result Variable is saved to disk and reloaded automatically the next time FPP starts, so it keeps its value across a restart or reboot. If unchecked (the default), it only lives in memory and resets to unset every time FPP restarts.'>
                                                            <img src='images/redesign/help-icon.svg' class='icon-help' alt='Help icon'></span>
                                                    </td>
                                                    <td><input type='checkbox' class='ptTmplPersist'> persist across
                                                        restarts</td>
                                                </tr>
                                                <tr>
                                                    <td>Filter:
                                                        <span class='argHelpIcon' data-bs-toggle='tooltip' data-bs-html='true' data-bs-placement='auto'
                                                            data-bs-title="<b>None</b>: store the whole raw result as-is.<br><b>JSON Field</b>: pull one field out of a JSON response by dotted path (object keys only, no array indices).<br><b>Between Markers</b>: extract the text found between two literal marker strings - good for scraping plain text or HTML.<br><b>Regex (advanced)</b>: extract the first capture group matched by a regular expression, or the whole match if the pattern has no group.">
                                                            <img src='images/redesign/help-icon.svg' class='icon-help' alt='Help icon'></span>
                                                    </td>
                                                    <td><select class='ptTmplFilterType'
                                                            onChange='FilterTypeChanged($(this).closest(".ptTmplAdvancedFields"));'>
                                                            <option value='none'>None (use raw result)</option>
                                                            <option value='json'>JSON Field</option>
                                                            <option value='between'>Between Markers</option>
                                                            <option value='regex'>Regex (advanced)</option>
                                                        </select></td>
                                                </tr>
                                                <tr class='ptTmplFilterJsonRow d-none'>
                                                    <td>Field path:</td>
                                                    <td><input type='text' size='20' class='ptTmplFilterJsonPath'
                                                            placeholder='e.g. data.temperature'></td>
                                                </tr>
                                                <tr class='ptTmplFilterBetweenRow d-none'>
                                                    <td>After:
                                                        <span class='argHelpIcon' data-bs-toggle='tooltip' data-bs-html='true' data-bs-placement='auto'
                                                            data-bs-title='Text is kept starting right after this marker. Leave blank to start from the beginning of the raw result.'>
                                                            <img src='images/redesign/help-icon.svg' class='icon-help' alt='Help icon'></span>
                                                    </td>
                                                    <td><input type='text' size='14' class='ptTmplFilterBetweenStart'
                                                            placeholder='e.g. Temp: '></td>
                                                </tr>
                                                <tr class='ptTmplFilterBetweenRow d-none'>
                                                    <td>Before:
                                                        <span class='argHelpIcon' data-bs-toggle='tooltip' data-bs-html='true' data-bs-placement='auto'
                                                            data-bs-title='Text is kept up until this marker. Leave blank to keep everything to the end of the raw result.'>
                                                            <img src='images/redesign/help-icon.svg' class='icon-help' alt='Help icon'></span>
                                                    </td>
                                                    <td><input type='text' size='14' class='ptTmplFilterBetweenEnd'
                                                            placeholder='e.g. &deg;F'></td>
                                                </tr>
                                                <tr class='ptTmplFilterRegexRow d-none'>
                                                    <td>Pattern:
                                                        <span class='argHelpIcon' data-bs-toggle='tooltip' data-bs-html='true' data-bs-placement='auto'
                                                            data-bs-title='A regular expression matched against the raw result. The first capture group (in parentheses) is used, or the whole match if the pattern has no group.'>
                                                            <img src='images/redesign/help-icon.svg' class='icon-help' alt='Help icon'></span>
                                                    </td>
                                                    <td><input type='text' size='20' class='ptTmplFilterRegex'
                                                            placeholder='e.g. Temp: ([0-9.]+)'></td>
                                                </tr>
                                            </table>
                                        </div>
                                    </span>
                                    </td>
                                    <td><span class='ptTmplStatus text-muted'>never run</span>
                                        <button type='button' class='buttons btn-sm ptTmplViewVar d-none' title='View current value'><i class='fas fa-eye'></i></button>
                                    </td>
                                </tr>
                            </table>

                            <table id="taskTable" class="fppSelectableRowTable fppStickyTheadTable">
                                <thead>
                                    <tr>
                                        <th>Enabled</th>
                                        <th>Name</th>
                                        <th>Interval (sec)</th>
                                        <th>Type</th>
                                        <th>Preset / Command</th>
                                        <th>Status</th>
                                    </tr>
                                </thead>
                                <tbody id="tblTasksBody" width="100%">
                                </tbody>
                            </table>
                        </div>
                    </div>
                </div>
            </div>
        </div>
        <?php include 'common/footer.inc'; ?>
    </div>

    <!-- Populated in JS (LoadResultVariableNames) - Variables are created/
         renamed constantly at runtime (Set Variable, MQTT, recurring tasks
         themselves), so a page-load-time PHP snapshot would go stale
         immediately. Command Presets (.ptTmplPreset, PopulatePresetSelect)
         use the same JS-fetch approach now, for the same reason. -->
    <datalist id='ptResultVariableNames'></datalist>

</body>

</html>
