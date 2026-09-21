<script>
    var commandEditorTarget = '';
    var commandEditorData = {};
    var commandEditorCallback = '';
    var commandEditorCancelCallback = '';
    var commandEditorPresets = '';
    var commandEditorArgs = {};

    function CommandEditorSetup(target, data, callback, cancelCallback, args) {
        commandEditorTarget = target;
        commandEditorData = data;
        commandEditorCallback = callback;
        commandEditorCancelCallback = cancelCallback;
        commandEditorArgs = args;

        $('.presetSelect').hide();
        if (args.showPresetSelect) {
            $.get('api/configfile/commandPresets.json'
            ).done(function (data) {
                if (data.hasOwnProperty('commands') && data.commands.length > 0) {
                    commandEditorPresets = data;
                    var names = [];
                    for (var i = 0; i < data.commands.length; i++) {
                        names.push(data.commands[i].name);
                    }
                    names.sort();

                    var options = "<option value=''>-- Select a Command Preset --</option>";
                    for (var i = 0; i < names.length; i++) {
                        options += "<option value='" + names[i] + "'>" + names[i] + "</option>";
                    }
                    $('#presetNames').html(options);
                    $('.presetSelect').show();
                    if (args.presetInsertsReference) {
                        // Picking a preset here inserts a live reference to it
                        // (Trigger Command Preset), not an editable snapshot of
                        // its underlying command's fields - so "Save Preset"
                        // (which overwrites *this* preset's own definition)
                        // doesn't apply, and would risk saving a preset that
                        // triggers itself.
                        $('#btnSaveDirectEditorCommand').hide();
                    }
                }
            });
        }

        if (args.hasOwnProperty("validCommands")) {
            PopulateCommandListCache();
            var cmds = args["validCommands"];
            for (i = 0; i < cmds.length; i++) {
                $("#editorCommand").append("<option value='" + cmds[i] + "'>" + cmds[i] + "</option>");
            }
        } else {
            LoadCommandList($('#editorCommand'), data['command']);
        }

        $('#btnSaveEditorCommand').val(args.saveButton);
        $('#btnCancelCommandEditor').val(args.cancelButton);

        $('#editorCommand').val(data['command']);
        CommandSelectChanged('editorCommand', 'tblCommandEditor');

        PopulateExistingCommand(data, 'editorCommand', 'tblCommandEditor');
        ldEnhanceOverlayPickers('tblCommandEditor');
    }

    function CommandEditorPresetSelectChanged() {
        if (typeof commandEditorPresets != 'string') {
            var presetName = $('#presetNames').val();
            if (presetName == '') {
                // Deselecting the preset resets the fields below back to the
                // state the editor opened with.
                PopulateExistingCommand(commandEditorData, 'editorCommand', 'tblCommandEditor');
                $('#btnSaveDirectEditorCommand').prop('disabled', true);
                return;
            }
            for (var i = 0; i < commandEditorPresets.commands.length; i++) {
                if (commandEditorPresets.commands[i].name == presetName) {
                    if (commandEditorArgs.presetInsertsReference) {
                        // Callers that persist this row into a list (If's Then
                        // Run/Otherwise Run, a GPIO edge's command list) want a
                        // live reference to the preset, not a frozen copy of
                        // whatever the preset's underlying command/args happen
                        // to be right now - so editing the preset later is
                        // reflected everywhere it's used, instead of only in
                        // presets picked after the edit.
                        PopulateExistingCommand({ command: 'Trigger Command Preset', args: [presetName] }, 'editorCommand', 'tblCommandEditor');
                    } else {
                        PopulateExistingCommand(commandEditorPresets.commands[i], 'editorCommand', 'tblCommandEditor');
                    }
                    $('#btnSaveDirectEditorCommand').prop('disabled', false);
                }
            }
        }
    }

    function CommandEditorCommandChanged() {
        CommandSelectChanged('editorCommand', 'tblCommandEditor');
        ldEnhanceOverlayPickers('tblCommandEditor');
    }

    function CommandEditorRunNow() {
        var data = {};
        data = CommandToJSON('editorCommand', 'tblCommandEditor', data);

        RunCommand(data);
    }

    function CommandEditorSave() {
        var data = {};
        data = CommandToJSON('editorCommand', 'tblCommandEditor', data);

        $('#commandEditorPopup').fppDialog("close");

        if (commandEditorCallback != '') {
            window[commandEditorCallback](commandEditorTarget, data);
        }
    }

    function CommandEditorSaveDirect() {
        var cmd = {};
        name = $('#presetNames').val();
        cmd = CommandToJSON('editorCommand', 'tblCommandEditor', cmd);

        if (name == '') {
            $('#btnSaveDirectEditorCommand').prop('disabled', true);
            $.jGrowl("No preset selected", { themeState: 'success' });
            return;
        }

        $.get('api/configfile/commandPresets.json'
        ).done(function (data) {
            if (data.hasOwnProperty('commands')) {
                commandEditorPresets = data;
            } else {
                data = {};
                data.commands = [];
            }

            var found = 0;
            for (var i = 0; i < data.commands.length; i++) {
                if (data.commands[i].name == name) {
                    found = 1;
                    data.commands[i].command = cmd.command;
                    data.commands[i].multisyncCommand = cmd.multisyncCommand;
                    data.commands[i].args = cmd.args;
                }
            }

            if (!found) {
                $.jGrowl("Preset '" + name + "' not found", { themeState: 'success' });
                return;
            }

            var json = JSON.stringify(data, null, 2);
            var result = Post('api/configfile/commandPresets.json', false, json);

            if (!result.hasOwnProperty('Status') || (result['Status'] != 'OK')) {
                alert('Error saving commands!');
            } else {
                $.jGrowl('Commands saved.', { themeState: 'success' });
                SetRestartFlag(2);
            }
        });
    }

    function CommandEditorCancel() {
        $('#commandEditorPopup').fppDialog("close");

        if (commandEditorCancelCallback != '') {
            window[commandEditorCancelCallback](commandEditorTarget);
        }
    }

    // ── Overlay-model picker enhancement ──────────────────────────────────
    // The "Overlay Model Effect" (and sibling overlay) commands list their
    // targets from api/models, which by default returns only top-level models.
    // FPP now resolves xLights submodels and model groups by name too, so this
    // adds submodels/groups to those pickers and, because the combined list is
    // large, a Type selector (Models / Submodels / Model Groups / All) plus a
    // text filter to keep it manageable. Submodels stay lazily materialised in
    // the daemon — only the picker list is expanded here. Scoped to the command
    // editor modal so the generic command renderer is untouched.

    var _ldOverlayTypes = null;   // { sub:{normName:1}, grp:{normName:1} }, cached

    function ldNorm(s) { return String(s || '').toLowerCase().replace(/[^a-z0-9]/g, ''); }

    function ldLoadOverlayTypes(cb) {
        if (_ldOverlayTypes) { cb(_ldOverlayTypes); return; }
        var t = { sub: {}, grp: {}, polar: {} };
        // Settle BOTH requests (via .always + a counter) before classifying, so
        // a missing file (404) on either one doesn't short-circuit the other.
        // Absent submodel/group JSON simply leaves its set empty — no error.
        var pending = 2;
        var done = function () { if (--pending === 0) { _ldOverlayTypes = t; cb(t); } };
        // cache:false on both -- these files are rewritten whenever models are
        // re-uploaded from xLights or a polar buffer is built, and a browser
        // serving a stale copy silently misclassifies everything it describes
        // (a polar buffer filed under Model Groups, a new submodel as a model).
        $.ajax({ url: 'api/configfile/xlights-submodels.json', dataType: 'json', cache: false })
            .done(function (d) { ((d && d.submodels) || []).forEach(function (s) { if (s.Name) t.sub[ldNorm(s.Name)] = 1; }); })
            .always(done);
        $.ajax({ url: 'api/configfile/xlights-modelgroups.json', dataType: 'json', cache: false })
            .done(function (d) { ((d && d.modelgroups) || []).forEach(function (g) {
                if (!g.Name) return;
                // BufferStyle is core's field (PixelOverlayModel::getBufferStyle):
                // a polar buffer lives in the group file but is not a group, and
                // filing it under "Model Groups" hides it from anyone looking for
                // one. See the Polar Buffers section on pixeloverlaymodels.php.
                if (g.BufferStyle === 'polar') { t.polar[ldNorm(g.Name)] = 1; }
                else { t.grp[ldNorm(g.Name)] = 1; }
            }); })
            .always(done);
    }

    function ldOverlayType(name) {
        if (!_ldOverlayTypes) return 'model';
        var n = ldNorm(name);
        if (_ldOverlayTypes.polar[n]) return 'polar';
        if (_ldOverlayTypes.grp[n]) return 'group';
        if (_ldOverlayTypes.sub[n]) return 'submodel';
        return 'model';
    }

    function ldEsc(s) { return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/"/g, '&quot;'); }

    // Enhance every api/models picker in the command editor table (idempotent).
    function ldEnhanceOverlayPickers(tblId) {
        $('#' + tblId + ' select[data-contentlisturl]').each(function () {
            var $sel = $(this);
            var url = String($sel.data('contentlisturl') || '');
            if (!/api\/models\b/.test(url) || !/simple=true/.test(url)) return;
            if ($sel.data('ldEnhanced')) return;
            $sel.data('ldEnhanced', true);

            ldLoadOverlayTypes(function (t) {
                // No xLights submodels/groups on this system (files absent or
                // empty) → leave the picker as the stock models-only list, with
                // no URL rewrite and no Type filter bar.
                if (Object.keys(t.sub).length === 0 && Object.keys(t.grp).length === 0) {
                    return;
                }

                // Include submodels + groups in the option list, then re-load it.
                if (!/submodels=true/.test(String($sel.data('contentlisturl') || ''))) {
                    var newUrl = url + '&submodels=true';
                    $sel.attr('data-contentlisturl', newUrl).data('contentlisturl', newUrl);
                    try { ReloadContentList(window.location.hostname, $sel); } catch (e) {}
                }

                // Build the master list from the content-list URL directly
                // rather than from the <option> elements in the DOM.
                //
                // Reading the DOM couples this to exactly when fpp.js finished
                // populating the select, and the first filter pass then REPLACES
                // those options -- so a snapshot taken a moment too early is
                // permanently short, and every Type view but the default comes
                // up empty with only the already-selected value surviving
                // (ldApplyOverlayFilter always keeps those). Fetching the list
                // is the same request fpp.js just made, served from cache, and
                // it cannot race.
                $.ajax({
                    dataType: 'json',
                    url: String($sel.data('contentlisturl') || ''),
                    async: false,
                    success: function (data) {
                        var all = [];
                        if ($sel.data('allowblanks')) {
                            all.push({ value: '', label: '', special: true, type: 'model' });
                        }
                        $.each(data, function (k, v) {
                            var value = Array.isArray(data) ? v : k;
                            var label = Array.isArray(data) ? v : v;
                            var special = (value === '' || value === '--All Models--');
                            all.push({ value: value, label: label, special: special,
                                       type: special ? 'model' : ldOverlayType(value) });
                        });
                        $sel.data('ldAll', all);
                    }
                });
                if (!($sel.data('ldAll') || []).length) {
                    return;   // could not load the list; leave the stock picker alone
                }
                ldBuildFilterBar($sel);
                ldApplyOverlayFilter($sel);
            });
        });
    }

    function ldBuildFilterBar($sel) {
        if ($sel.data('ldBar')) return;
        // Default the Type view to the currently-selected item's type (so
        // editing a submodel/group command opens on the right list), else Models.
        var sel = $sel.val() || [];
        sel = Array.isArray(sel) ? sel : [sel];
        var defType = 'model';
        for (var i = 0; i < sel.length; i++) {
            var t = ldOverlayType(sel[i]);
            if (t !== 'model') { defType = t; break; }
        }
        var $bar = $(
            "<div class='ldOverlayFilter' style='margin-bottom:5px;display:flex;gap:6px;align-items:center;'>"
            + "<label style='font-size:0.85em;white-space:nowrap;'>Type:</label>"
            + "<select class='ldTypeFilter'>"
            + "<option value='model'>Models</option>"
            + "<option value='submodel'>Submodels</option>"
            + "<option value='group'>Model Groups</option>"
            + "<option value='polar'>Polar Buffers</option>"
            + "<option value='all'>All</option>"
            + "</select>"
            + "<input type='text' class='ldSearchFilter' placeholder='filter…' style='flex:1;min-width:80px;'>"
            + "</div>");
        $bar.find('.ldTypeFilter').val(defType);
        $bar.find('.ldTypeFilter, .ldSearchFilter').on('input change', function () {
            ldApplyOverlayFilter($sel);
        });
        $sel.before($bar);
        $sel.data('ldBar', $bar);
    }

    // Rebuild the <option> list from the snapshot, filtered by Type + text.
    // Currently-selected values are always kept (even if filtered out) so a
    // selection isn't silently lost when switching Type.
    function ldApplyOverlayFilter($sel) {
        var all = $sel.data('ldAll') || [];
        var $bar = $sel.data('ldBar');
        var type = $bar.find('.ldTypeFilter').val();
        var q = ($bar.find('.ldSearchFilter').val() || '').toLowerCase();

        var cur = $sel.val() || [];
        cur = Array.isArray(cur) ? cur : [cur];
        var selSet = {}; cur.forEach(function (v) { selSet[v] = 1; });

        var html = '';
        all.forEach(function (o) {
            var keep = o.special || selSet[o.value]
                || ((type === 'all' || o.type === type)
                    && (!q || o.label.toLowerCase().indexOf(q) >= 0
                           || o.value.toLowerCase().indexOf(q) >= 0));
            if (!keep) return;
            html += "<option value='" + ldEsc(o.value) + "'"
                 + (selSet[o.value] ? ' selected' : '') + '>' + ldEsc(o.label) + '</option>';
        });
        $sel.html(html);

        // A multistring picker is DISPLAYED as a checkbox list mirroring this
        // <select>, which is itself hidden (fpp.js SyncMultistringChecks, issue
        // #2733). Rewriting the options alone therefore changes nothing on
        // screen -- the Type and text filters appear to do nothing at all.
        // Rebuild the mirror whenever the option set changes.
        if ($sel.attr('data-multistring') === '1' && typeof SyncMultistringChecks === 'function') {
            SyncMultistringChecks($sel[0]);
        }
    }
</script>

<style>
    #tblCommandEditor td:first-child {
        padding-right: 8px;
        width: 25%;
    }
</style>

<table width="100%" class="tblCommandEditor settingsTable" id="tblCommandEditor">
    <tr class='presetSelect'>
        <td class="text-nowrap">Load Existing Command Preset:</td>
        <td><select id='presetNames' onChange='CommandEditorPresetSelectChanged();'
                title="Selecting a preset pre-populates the command and its options below"></select></td>
    </tr>
    <tr>
        <td>Command:</td>
        <td><select id="editorCommand" onChange="CommandEditorCommandChanged();"></select></td>
    </tr>
</table>
<hr class="mt-4">
<div class="text-center pt-2 pb-3">
    <input id="btnSaveEditorCommand" type="button" class="buttons wideButton" value="Accept Changes"
        onClick="CommandEditorSave();">
    <input id="btnRunEditorCommand" type="button" class="buttons wideButton" value="Run Now"
        onClick="CommandEditorRunNow();">
    <input id="btnSaveDirectEditorCommand" type="button" class="presetSelect buttons wideButton" value="Save Preset"
        onClick="CommandEditorSaveDirect();" disabled='disabled'>
    <input id="btnCancelCommandEditor" type="button" class="buttons wideButton" value="Cancel Edit"
        onClick="CommandEditorCancel();">
</div>
