<script>
    var commandEditorTarget = '';
    var commandEditorData = {};
    var commandEditorCallback = '';
    var commandEditorCancelCallback = '';
    var commandEditorPresets = '';

    function CommandEditorSetup(target, data, callback, cancelCallback, args) {
        commandEditorTarget = target;
        commandEditorData = data;
        commandEditorCallback = callback;
        commandEditorCancelCallback = cancelCallback;

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

                    var options = "<option value=''>---- Select a Command Preset ----</option>";
                    for (var i = 0; i < names.length; i++) {
                        options += "<option value='" + names[i] + "'>" + names[i] + "</option>";
                    }
                    $('#presetNames').html(options);
                    $('.presetSelect').show();
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
            LoadCommandList($('#editorCommand'));
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
            for (var i = 0; i < commandEditorPresets.commands.length; i++) {
                if (commandEditorPresets.commands[i].name == presetName) {
                    PopulateExistingCommand(commandEditorPresets.commands[i], 'editorCommand', 'tblCommandEditor');
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
        var t = { sub: {}, grp: {} };
        // Settle BOTH requests (via .always + a counter) before classifying, so
        // a missing file (404) on either one doesn't short-circuit the other.
        // Absent submodel/group JSON simply leaves its set empty — no error.
        var pending = 2;
        var done = function () { if (--pending === 0) { _ldOverlayTypes = t; cb(t); } };
        $.get('api/configfile/xlights-submodels.json')
            .done(function (d) { ((d && d.submodels) || []).forEach(function (s) { if (s.Name) t.sub[ldNorm(s.Name)] = 1; }); })
            .always(done);
        $.get('api/configfile/xlights-modelgroups.json')
            .done(function (d) { ((d && d.modelgroups) || []).forEach(function (g) { if (g.Name) t.grp[ldNorm(g.Name)] = 1; }); })
            .always(done);
    }

    function ldOverlayType(name) {
        if (!_ldOverlayTypes) return 'model';
        var n = ldNorm(name);
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

                // Snapshot the full option set with its type once populated.
                var all = [];
                $sel.children('option').each(function () {
                    var v = $(this).attr('value');
                    var special = (v === '' || v === '--All Models--');
                    all.push({ value: v, label: $(this).text(), special: special,
                               type: special ? 'model' : ldOverlayType(v) });
                });
                $sel.data('ldAll', all);
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
    }
</script>

<style>
    #tblCommandEditor .commandEditorHint {
        font-style: italic;
        font-size: 0.875em;
        color: var(--fpp-text-secondary);
        padding-bottom: 8px;
    }

    #tblCommandEditor td:first-child {
        padding-right: 8px;
        width: 25%;
    }
</style>

<table width="100%" class="tblCommandEditor settingsTable" id="tblCommandEditor">
    <tr class='presetSelect'>
        <td colspan="2">
            <h2>Load from Preset</h2>
        </td>
    </tr>
    <tr class='presetSelect'>
        <td colspan="2" class="commandEditorHint">Selecting a preset pre-populates the command and its options below
        </td>
    </tr>
    <tr class='presetSelect'>
        <td>Preset:</td>
        <td><select id='presetNames' onChange='CommandEditorPresetSelectChanged();'></select></td>
    </tr>
    <tr>
        <td colspan="2">
            <h2>Command Settings</h2>
        </td>
    </tr>
    <tr>
        <td>Command:</td>
        <td><select id="editorCommand" onChange="CommandEditorCommandChanged();"></select></td>
    </tr>
</table>
<hr>
<center>
    <input id="btnSaveEditorCommand" type="button" class="buttons wideButton" value="Accept Changes"
        onClick="CommandEditorSave();">
    <input id="btnRunEditorCommand" type="button" class="buttons wideButton" value="Run Now"
        onClick="CommandEditorRunNow();">
    <input id="btnSaveDirectEditorCommand" type="button" class="presetSelect buttons wideButton" value="Save Preset"
        onClick="CommandEditorSaveDirect();" disabled='disabled'>
    <input id="btnCancelCommandEditor" type="button" class="buttons wideButton" value="Cancel Edit"
        onClick="CommandEditorCancel();">

</center>