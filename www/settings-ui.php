<?
$skipJSsettings = 1;
require_once 'common.php';
?>

<script>
// The UI password settings are deliberately NOT saved one field at a time like
// every other setting on this page.  Applying 'passwordEnable' reloads apache
// with basic authentication turned on, so every request the browser makes after
// that point needs credentials.  Saving on each change meant the enable was
// applied before the password the user typed had been written, so the browser
// was challenged for the interim default ('falcon') and rejected the password
// the user had just chosen (issue #2829).  It also meant the enable was pushed
// through SetSetting()'s 1 second timeout, which the apache reload plus the
// configuration backup routinely exceed - the save actually succeeded but the
// UI reported "Failed to save passwordEnable setting."
//
// Instead the fields are staged as they change and applied together, in order
// (password first, enable last), by SaveUIPasswordSettings() with no timeout.
//
// Turning the password OFF has neither problem - there is no password to write
// first, and it goes through the same no-timeout path - so switching the
// dropdown back to 'No Password' applies at once rather than waiting for the
// Save button, which is hidden along with the password fields (issue #2985).
function MarkUIPasswordDirty() {
    $('#uiPasswordSaveStatus')
        .removeClass('text-muted')
        .addClass('text-warning')
        .html('Not applied yet &ndash; click "Save UI Password".');
}

function SetUIPasswordStatus(html, cssClass) {
    $('#uiPasswordSaveStatus')
        .removeClass('text-muted text-warning')
        .addClass(cssClass || 'text-muted')
        .html(html);
}

function SaveUIPasswordSettings() {
    var enable = $('#passwordEnable').val();
    var password = $('#password').val();
    var passwordVerify = $('#passwordVerify').val();

    if (enable == '1') {
        if (password != passwordVerify) {
            DialogError('UI Password', 'The password and the verification do not match.');
            return;
        }

        if (!ValidatePassword(password)) {
            return;
        }

        if (!confirm("The FPP web User Interface will now require the username 'admin' and the password '" + password + "'.  This password is also needed by other applications, such as xLights' FPP Connect.  If you forget it you may be locked out of FPP.  Click OK to continue."))
            return;
    }

    var $btn = $('#saveUIPasswordBtn');
    $btn.prop('disabled', true);
    SetUIPasswordStatus('<i class="fas fa-spinner fa-spin"></i> Saving...');

    function saveFailed(key) {
        $btn.prop('disabled', false);
        SetUIPasswordStatus('');
        if (enable == '0') {
            // Put the dropdown back to what is still live so choosing
            // 'No Password' again retries the disable.
            $('#passwordEnable').val(livePasswordEnable);
            settings['passwordEnable'] = livePasswordEnable;
            window['UpdatepasswordEnableChildren'](0);
            $('.passwordEnableChild').show();
        }
        DialogError('Save Setting', 'Failed to save ' + key + ' setting.');
    }

    // Applied last: this turns authentication on (or off) and reloads apache.
    function applyPasswordEnable() {
        SetUIPasswordStatus('<i class="fas fa-spinner fa-spin"></i> Applying...');
        $.ajax({
            url: 'api/settings/passwordEnable',
            data: '' + enable,
            method: 'PUT',
            success: function () {
                settings['passwordEnable'] = enable;
                // Authentication has just been enabled or disabled, so reload -
                // the browser is challenged (or released) using the credentials
                // that are now live rather than the ones it cached.
                location.reload();
            },
            error: function () { saveFailed('passwordEnable'); }
        });
    }

    if (enable == '1') {
        // Write the password FIRST so the htpasswd file already holds the chosen
        // password by the time authentication is switched on.  skipBackup avoids
        // generating a configuration backup here; the passwordEnable save below
        // generates the single backup covering both values.
        $.ajax({
            url: 'api/settings/password?skipBackup=1',
            data: '' + password,
            method: 'PUT',
            success: function () {
                settings['password'] = password;
                applyPasswordEnable();
            },
            error: function () { saveFailed('password'); }
        });
    } else {
        $.jGrowl('Disabling the UI password...', { themeState: 'success' });
        applyPasswordEnable();
    }
}

// The enable flag as loaded, i.e. what is live.
var livePasswordEnable = '0';

$( document ).ready(function() {
    livePasswordEnable = $('#passwordEnable').val();

    if ($('#passwordEnable').val() == '1') {
        $('.passwordEnableChild').show();
    } else {
        $('.passwordEnableChild').hide();
    }

    // Replace the auto-generated onChange handlers for the UI password settings
    // so they stage the value instead of saving it immediately.
    $.each(['passwordEnable', 'password', 'passwordVerify'], function (i, name) {
        window[name + 'Changed'] = function () {
            settings[name] = $('#' + name).val();

            if (typeof window['Update' + name + 'Children'] === 'function') {
                window['Update' + name + 'Children'](0);
            }

            if (name == 'passwordEnable') {
                if ($('#passwordEnable').val() == '1') {
                    $('.passwordEnableChild').show();
                } else {
                    $('.passwordEnableChild').hide();

                    if (livePasswordEnable == '1') {
                        SaveUIPasswordSettings();
                    } else {
                        // Backed out of enabling before saving; nothing to apply.
                        SetUIPasswordStatus('');
                    }
                    return;
                }
            }

            MarkUIPasswordDirty();
        };
    });
});
</script>


<?
$uiLevelTogglePrepend = "";
if ($uiLevelOverrideActive || intval($settings['uiLevel']) < 1) {
    $uiLevelTogglePrepend = "<div class='row'><div class='printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2'><div class='description'>Temporary User Interface Level</div></div><div class='printSettingFieldCol col-md'>";
    if ($uiLevelOverrideActive) {
        $uiLevelTogglePrepend .= "<span>Advanced (~" . $uiLevelOverrideMinsLeft . " min remaining) &nbsp;</span>"
            . "<input type='button' class='buttons' value='Exit Advanced Mode' onClick='ExitUiLevelOverride();'>";
    } else {
        $uiLevelTogglePrepend .= "<input type='button' class='buttons' value='Change to Advanced UI for " . UI_LEVEL_OVERRIDE_MINUTES . " Minutes' onClick='ShowAdvancedTemporarily();'>";
    }
    $uiLevelTogglePrepend .= "</div></div>";
}
PrintSettingGroup('ui', "", $uiLevelTogglePrepend);
?>


            <h2>UI Password</h2>

<?
PrintSetting('passwordEnable');
?>
<br>
            <div class='row passwordEnableChild' style='display: none;'>
                <div class="printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2">
                    <div class='description'><i class="fas fa-fw fa-nbsp ui-level-0"></i>Username
                    </div>
                </div>
                <div class='printSettingFieldCol col-md'><input disabled value='admin' size='5'></div>
            </div>
<?
PrintSetting('password');
PrintSetting('passwordVerify');
?>
            <div class='row passwordEnableChild' style='display: none;'>
                <div class="printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2"></div>
                <div class='printSettingFieldCol col-md'>
                    <input type='button' class='buttons btn-success' id='saveUIPasswordBtn'
                        value='Save UI Password' onClick='SaveUIPasswordSettings();'>
                    <span id='uiPasswordSaveStatus' class='ms-2 text-muted'></span>
                </div>
            </div>
<? if ($uiLevel >= 1) { ?>
<h2>Keyboard Shortcuts</h2>

<div class='row' id='keyBindingsRow'>
    <div class='printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2'>
        <div class='description'><i class='fas fa-fw fa-graduation-cap fa-nbsp ui-level-1' title='Advanced Level Setting'></i>Keyboard Shortcuts</div>
    </div>
    <div class='printSettingFieldCol col-md'>
        <button type='button' class='buttons' id='keyBindingsOpenBtn' onClick='OpenKeyBindingsDialog();'><i class='fas fa-keyboard'></i> Configure Key Bindings</button>
        <span id='keyBindingsTip' class='ms-2' data-bs-toggle='tooltip' data-bs-html='true' data-bs-placement='auto' data-bs-title='<b>No custom shortcuts.</b><br>Keyboard shortcuts trigger a command preset, run an FPP command, or open a page. F1 opens help. Unless you assign them, F2 opens Settings and F8 opens Error Reporting.'><img id='keyBindings_img' src='images/redesign/help-icon.svg' class='icon-help' alt='keyBindings help icon'></span>
    </div>
</div>
<br>
<? } ?>
<?
PrintSettingGroup('uiColors');
?>

<? if ($uiLevel >= 1) { ?>
<div id='keyBindingsEditorHolder' class='d-none'>
<div id='keyBindingsEditor'>
        <p class='text-muted mb-2'>Custom shortcuts must use
            <kbd>Ctrl</kbd> or <kbd>Alt</kbd>, or be a function key
            (<kbd>F2</kbd>&ndash;<kbd>F4</kbd>, <kbd>F6</kbd>&ndash;<kbd>F10</kbd>).
            Assigning <kbd>F2</kbd> or <kbd>F8</kbd> replaces its default action.
            Browser and text-editing shortcuts (copy, paste, undo, find, reload,
            new tab, zoom, ...) can't be used.</p>
        <h3 class='fs-6 fw-bold mt-2'>System shortcuts</h3>
        <table class='table table-sm w-auto' id='keyBindingsSystemTable'>
            <thead>
                <tr>
                    <th>Keys</th>
                    <th>Action</th>
                </tr>
            </thead>
            <tbody>
                <tr>
                    <td><kbd>F1</kbd></td>
                    <td>Help &ndash; opens help for the current page. Press again or <kbd>Esc</kbd> to close.</td>
                </tr>
                <tr>
                    <td><kbd>F2</kbd></td>
                    <td>Settings &ndash; opens the FPP Settings page, unless you assign <kbd>F2</kbd> to a custom shortcut.</td>
                </tr>
                <tr>
                    <td><kbd>F8</kbd></td>
                    <td>Error Reporting &ndash; opens or closes the diagnostic report dialog, unless you assign <kbd>F8</kbd> to a custom shortcut.</td>
                </tr>
                <tr>
                    <td><kbd>Esc</kbd></td>
                    <td>Close &ndash; closes the Help or Error Reporting dialog.</td>
                </tr>
            </tbody>
        </table>
        <h3 class='fs-6 fw-bold mt-3'>Custom shortcuts</h3>
        <div class='table-responsive'>
            <table class='table table-sm table-hover' id='keyBindingsTable'>
                <thead>
                    <tr>
                        <th>Keys</th>
                        <th>Action type</th>
                        <th>Action</th>
                        <th aria-label='Row actions'></th>
                    </tr>
                </thead>
                <tbody id='keyBindingsBody'>
                </tbody>
            </table>
        </div>
        <div class='d-flex gap-2 align-items-center flex-wrap mt-2'>
            <button type='button' class='buttons btn-outline-success' id='keyBindingsAddBtn'
                onClick='KeyBindingsAddRow();'><i class='fas fa-plus'></i> Add shortcut</button>
            <button type='button' class='buttons btn-success' id='keyBindingsSaveBtn'
                onClick='KeyBindingsSave();'>Save shortcuts</button>
            <span id='keyBindingsSaveStatus' class='ms-2 text-muted'></span>
        </div>
</div>
</div>

<script>
// Custom keyboard shortcuts editor. Bindings are stored as a JSON array in the
// 'keyBindings' setting, e.g.
//   [{key:'Ctrl+Shift+A', action:'preset', preset:'My Preset'},
//    {key:'Ctrl+Alt+P', action:'command', command:'Volume Set', args:['50']},
//    {key:'Alt+1', action:'page', page:'playlists.php'}]
// Key canonicalization, matching and dispatch live in fpp.js so every page
// shares them; this editor only builds the table rows and saves the setting.
// F1 is reserved for Help. F2/F8 default to Settings/Error Reporting in
// fpp.js until a shortcut here is assigned to them.
var keyBindingsPresetNames = null;
var keyBindingsCapturing = null;

var keyBindingsPages = [
    ['index.php', 'Status'],
    ['currentmonitor.php', 'Port Status'],
    ['system-stats.php', 'System Stats'],
    ['troubleshooting.php', 'Troubleshooting'],
    ['healthCheck.php', 'Health Check'],
    ['cape-info.php', 'Cape Info'],
    ['scheduler.php', 'Scheduler'],
    ['playlists.php', 'Playlists'],
    ['filemanager.php', 'File Manager'],
    ['effects.php', 'Effects'],
    ['commandPresets.php', 'Command Presets'],
    ['variables.php', 'User Variables'],
    ['recurringtasks.php', 'Recurring Tasks'],
    ['plugins.php', 'Plugins'],
    ['packages.php', 'Packages'],
    ['settings.php', 'Settings'],
    ['settings.php#settings-playback', 'Settings - Playback'],
    ['settings.php#settings-av', 'Settings - Audio/Video'],
    ['settings.php#settings-localization', 'Settings - Localization'],
    ['settings.php#settings-ui', 'Settings - UI'],
    ['settings.php#settings-email', 'Settings - Email'],
    ['settings.php#settings-mqtt', 'Settings - MQTT'],
    ['settings.php#settings-privacy', 'Settings - Privacy'],
    ['settings.php#settings-output', 'Settings - Input/Output'],
    ['settings.php#settings-logs', 'Settings - Logging'],
    ['settings.php#settings-services', 'Settings - Services'],
    ['settings.php#settings-storage', 'Settings - Storage'],
    ['settings.php#settings-system', 'Settings - System'],
    ['settings.php#settings-developer', 'Settings - Developer'],
    ['multisync.php', 'MultiSync'],
    ['networkconfig.php', 'Network'],
    ['proxies.php', 'Proxy Config'],
    ['backup.php', 'Backup'],
    ['about.php', 'About / Upgrade'],
    ['help.php', 'Help Index'],
    ['channelinputs.php', 'Channel Inputs'],
    ['channeloutputs.php', 'Channel Outputs'],
    ['outputprocessors.php', 'Output Processors'],
    ['pixeloverlaymodels.php', 'Pixel Overlay Models'],
    ['gpio.php', 'GPIO'],
    ['testing.php', 'Display Testing'],
    ['virtualdisplaywrapper.php', '2D Virtual Display'],
    ['virtualdisplaywrapper3d.php', '3D Virtual Display'],
    ['pipewire-graph.php', 'PipeWire Graph']
];

function KeyBindingsGetAll() {
    var raw = (typeof settings !== 'undefined' && settings['keyBindings']) ? settings['keyBindings'] : '[]';
    try {
        var bindings = JSON.parse(raw);
        return Array.isArray(bindings) ? bindings : [];
    } catch (e) {
        return [];
    }
}

function KeyBindingsSetStatus(html, cssClass) {
    $('#keyBindingsSaveStatus')
        .removeClass('text-muted text-warning text-danger text-success')
        .addClass(cssClass || 'text-muted')
        .html(html);
}

function KeyBindingsRenderArgs(detailCell, commandName, args) {
    detailCell.find('.keyBindingsArgs').remove();
    if (!commandName || !commandListByName.hasOwnProperty(commandName)) {
        return;
    }
    var defs = commandListByName[commandName]['args'] || [];
    if (!defs.length) {
        return;
    }
    var wrap = $("<span class='keyBindingsArgs d-block mt-1'></span>");
    defs.forEach(function (def, i) {
        var val = (args && i < args.length && args[i] !== undefined) ? args[i] : (def['default'] || '');
        var label = $('<label class="d-block mt-1"></label>');
        label.append($('<small class="text-muted d-block"></small>').text(def['description'] || def['name']));
        var input;
        if (def['type'] === 'bool' || def['type'] === 'boolean') {
            input = $('<input type="checkbox" class="form-check-input keyBindingsArg">');
            input.prop('checked', val === true || val === 'true' || val === '1');
        } else if (def.hasOwnProperty('contents') && Array.isArray(def['contents'])) {
            input = $('<select class="form-select form-select-sm keyBindingsArg"></select>');
            def['contents'].forEach(function (opt) {
                input.append($('<option></option>').attr('value', opt).text(opt));
            });
            input.val(val);
            if (input.val() === null) {
                input.val(def['contents'][0]);
            }
        } else if (def['type'] === 'int' || def['type'] === 'float' || def['type'] === 'number') {
            input = $('<input type="number" class="form-control form-control-sm keyBindingsArg">');
            if (def.hasOwnProperty('min')) input.attr('min', def['min']);
            if (def.hasOwnProperty('max')) input.attr('max', def['max']);
            input.val(val);
        } else {
            input = $('<input type="text" class="form-control form-control-sm keyBindingsArg">');
            input.val(val);
        }
        input.attr('data-arg-index', i);
        label.append(input);
        wrap.append(label);
    });
    detailCell.append(wrap);
}

function KeyBindingsRenderDetail(row, binding) {
    var detailCell = row.find('.keyBindingsDetail');
    detailCell.empty();
    var type = row.find('.keyBindingsType').val();
    if (type === 'preset') {
        var sel = $('<select class="form-select form-select-sm keyBindingsPreset"></select>');
        sel.append($('<option value=""></option>').text('-- Select a command preset --'));
        if (keyBindingsPresetNames) {
            keyBindingsPresetNames.forEach(function (name) {
                sel.append($('<option></option>').attr('value', name).text(name));
            });
        }
        sel.val(binding['preset'] || '');
        if (binding['preset'] && sel.val() === null) {
            sel.prepend($('<option selected></option>').attr('value', binding['preset']).text(binding['preset'] + ' (unavailable)'));
        }
        detailCell.append(sel);
    } else if (type === 'command') {
        if (typeof commandList === 'string') {
            PopulateCommandListCache();
        }
        var cmdSel = $('<select class="form-select form-select-sm keyBindingsCommand"></select>');
        cmdSel.append($('<option value=""></option>').text('-- Select a command --'));
        LoadCommandList(cmdSel, binding['command'] || '');
        cmdSel.val(binding['command'] || '');
        if (binding['command'] && cmdSel.val() === null) {
            cmdSel.prepend($('<option selected></option>').attr('value', binding['command']).text(binding['command'] + ' (unavailable)'));
        }
        cmdSel.on('change', function () {
            KeyBindingsRenderArgs(detailCell, cmdSel.val(), []);
        });
        detailCell.append(cmdSel);
        KeyBindingsRenderArgs(detailCell, binding['command'] || '', binding['args'] || []);
    } else {
        var pageSel = $('<select class="form-select form-select-sm keyBindingsPage"></select>');
        pageSel.append($('<option value=""></option>').text('-- Select a page --'));
        keyBindingsPages.forEach(function (p) {
            pageSel.append($('<option></option>').attr('value', p[0]).text(p[1] + ' (' + p[0] + ')'));
        });
        pageSel.val(binding['page'] || '');
        if (binding['page'] && pageSel.val() === null) {
            pageSel.prepend($('<option selected></option>').attr('value', binding['page']).text(binding['page']));
        }
        detailCell.append(pageSel);
    }
}

function KeyBindingsMakeRow(binding) {
    binding = binding || { key: '', action: 'preset' };
    var row = $('<tr></tr>');
    var keyCell = $('<td></td>');
    var keyInput = $('<input type="text" readonly class="form-control form-control-sm keyBindingsKey" placeholder="Click, then press keys\u2026">');
    keyInput.val(binding['key'] || '');
    keyInput.on('focus', function () {
        keyBindingsCapturing = keyInput;
        keyInput.select();
    });
    keyInput.on('blur', function () {
        if (keyBindingsCapturing === keyInput) {
            keyBindingsCapturing = null;
        }
    });
    keyInput.on('keydown', function (e) {
        // While recording, Tab/Enter leave the field, Escape cancels and
        // Backspace clears instead of saving.
        if (e.key === 'Tab' || e.key === 'Enter') {
            keyBindingsCapturing = null;
            keyInput.blur();
            return;
        }
        if (e.key === 'Escape') {
            e.preventDefault();
            e.stopPropagation();
            keyBindingsCapturing = null;
            keyInput.blur();
            return false;
        }
        if (e.key === 'Backspace' || e.key === 'Delete') {
            e.preventDefault();
            e.stopPropagation();
            keyInput.val('');
            return false;
        }
        // Ignore presses of modifiers alone; wait for the real key.
        if (e.key === 'Control' || e.key === 'Alt' || e.key === 'Shift' || e.key === 'Meta') {
            e.preventDefault();
            return false;
        }
        var combo = (typeof KeyBindingsEventToString === 'function') ? KeyBindingsEventToString(e) : null;
        if (combo) {
            e.preventDefault();
            e.stopPropagation();
            keyInput.val(combo);
            keyBindingsCapturing = null;
            keyInput.blur();
            var blocked = KeyBindingsBlockedReason(combo);
            if (KeyBindingsIsReservedKey(combo)) {
                KeyBindingsSetStatus('F1 is reserved for Help &ndash; pick another combination.', 'text-danger');
            } else if (blocked) {
                KeyBindingsSetStatus($('<span>').text(combo + " is reserved for " + blocked + " and can't be used.").html(), 'text-danger');
            } else {
                KeyBindingsSetStatus('Not saved yet &ndash; click "Save shortcuts".', 'text-warning');
            }
        }
        return false;
    });
    keyCell.append(keyInput);

    var typeCell = $('<td></td>');
    var typeSel = $('<select class="form-select form-select-sm keyBindingsType"></select>');
    typeSel.append($('<option value="preset">Command preset</option>'));
    typeSel.append($('<option value="command">FPP command</option>'));
    typeSel.append($('<option value="page">Page</option>'));
    typeSel.val(binding['action'] || 'preset');
    typeCell.append(typeSel);

    var detailCell = $('<td class="keyBindingsDetail"></td>');
    var actionCell = $('<td class="text-nowrap"></td>');
    var testBtn = $("<button type='button' class='buttons btn-sm' title='Run this shortcut action now'><i class='fas fa-play'></i></button>");
    testBtn.on('click', function () {
        var b = KeyBindingsReadRow(row);
        if (b) {
            KeyBindingsRunBinding(b);
        }
    });
    var delBtn = $("<button type='button' class='buttons btn-sm ms-1' title='Remove this shortcut'><i class='fas fa-trash'></i></button>");
    delBtn.on('click', function () {
        row.remove();
        KeyBindingsSetStatus('Not saved yet &ndash; click "Save shortcuts".', 'text-warning');
    });
    actionCell.append(testBtn).append(delBtn);

    row.append(keyCell).append(typeCell).append(detailCell).append(actionCell);
    typeSel.on('change', function () {
        KeyBindingsRenderDetail(row, {});
        KeyBindingsSetStatus('Not saved yet &ndash; click "Save shortcuts".', 'text-warning');
    });
    row.on('change', '.keyBindingsKey, .keyBindingsPreset, .keyBindingsCommand, .keyBindingsPage, .keyBindingsArg, .keyBindingsType', function () {
        KeyBindingsSetStatus('Not saved yet &ndash; click "Save shortcuts".', 'text-warning');
    });
    KeyBindingsRenderDetail(row, binding);
    return row;
}

function KeyBindingsAddRow() {
    $('#keyBindingsBody').append(KeyBindingsMakeRow({ key: '', action: 'preset' }));
    var last = $('#keyBindingsBody .keyBindingsKey').last();
    if (last.length) {
        last.focus();
    }
}

function KeyBindingsReadRow(row) {
    var key = row.find('.keyBindingsKey').val().trim();
    var type = row.find('.keyBindingsType').val();
    var binding = { key: key, action: type };
    if (type === 'preset') {
        binding['preset'] = row.find('.keyBindingsPreset').val() || '';
    } else if (type === 'command') {
        binding['command'] = row.find('.keyBindingsCommand').val() || '';
        var args = [];
        row.find('.keyBindingsArg').each(function () {
            if ($(this).attr('type') === 'checkbox') {
                args.push($(this).is(':checked') ? 'true' : 'false');
            } else {
                args.push($(this).val());
            }
        });
        binding['args'] = args;
    } else {
        binding['page'] = row.find('.keyBindingsPage').val() || '';
    }
    return binding;
}

function KeyBindingsCollect() {
    var bindings = [];
    $('#keyBindingsBody > tr').each(function () {
        bindings.push(KeyBindingsReadRow($(this)));
    });
    return bindings;
}

function KeyBindingsValidate(bindings) {
    var seen = {};
    for (var i = 0; i < bindings.length; i++) {
        var b = bindings[i];
        var n = i + 1;
        if (!b.key) {
            return 'Row ' + n + ': press the Keys field and enter a key combination (or delete the row).';
        }
        if (typeof KeyBindingsIsReservedKey === 'function' && KeyBindingsIsReservedKey(b.key)) {
            return 'Row ' + n + ': F1 is reserved for Help. Pick another combination.';
        }
        if (typeof KeyBindingsIsAllowedCombo === 'function' && !KeyBindingsIsAllowedCombo(b.key)) {
            return 'Row ' + n + ': use Ctrl or Alt with another key, or a function key (F2-F4, F6-F10). Plain letters would fire while typing.';
        }
        var blocked = KeyBindingsBlockedReason(b.key);
        if (blocked) {
            return 'Row ' + n + ': ' + b.key + ' is reserved for ' + blocked + ' and can\'t be used. Pick another combination.';
        }
        if (seen[b.key]) {
            return 'Row ' + n + ': ' + b.key + ' is already used by row ' + seen[b.key] + '.';
        }
        seen[b.key] = n;
        if (b.action === 'preset' && !b.preset) {
            return 'Row ' + n + ': choose a command preset.';
        }
        if (b.action === 'command' && !b.command) {
            return 'Row ' + n + ': choose an FPP command.';
        }
        if (b.action === 'page' && !b.page) {
            return 'Row ' + n + ': choose a page.';
        }
        if (b.action === 'page' && !KeyBindingsIsValidPage(b.page)) {
            return 'Row ' + n + ': ' + b.page + ' is not an FPP page. Choose a page from the list.';
        }
    }
    return null;
}

function KeyBindingsSave() {
    var bindings = KeyBindingsCollect();
    var err = KeyBindingsValidate(bindings);
    if (err) {
        KeyBindingsSetStatus('', 'text-muted');
        // The message can quote a stored key or page, which may not be plain text.
        DialogError('Keyboard Shortcuts', $('<span>').text(err).html());
        return;
    }
    KeyBindingsSetStatus('<i class="fas fa-spinner fa-spin"></i> Saving...', 'text-muted');
    $('#keyBindingsSaveBtn').prop('disabled', true);
    SetSetting('keyBindings', JSON.stringify(bindings), 0, 0, false, null, function () {
        $('#keyBindingsSaveBtn').prop('disabled', false);
        KeyBindingsSetStatus('Saved.', 'text-success');
        KeyBindingsUpdateTip();
    }, function () {
        $('#keyBindingsSaveBtn').prop('disabled', false);
        KeyBindingsSetStatus('Save failed.', 'text-danger');
    });
}

function KeyBindingsTipHtml() {
    var n = KeyBindingsGetAll().length;
    var count = n === 0 ? 'No custom shortcuts' : n + (n === 1 ? ' custom shortcut' : ' custom shortcuts');
    return '<b>' + count + '.</b><br>Keyboard shortcuts trigger a command preset, run an FPP command, or open a page. F1 opens help. Unless you assign them, F2 opens Settings and F8 opens Error Reporting.';
}

function KeyBindingsUpdateTip() {
    var html = KeyBindingsTipHtml();
    var tip = $('#keyBindingsTip');
    if (tip.length === 0) {
        return;
    }
    tip.attr('data-bs-title', html);
    tip.attr('data-bs-original-title', html);
    if (window.bootstrap && typeof bootstrap.Tooltip === 'function') {
        var inst = bootstrap.Tooltip.getInstance(tip[0]);
        if (inst && typeof inst.setContent === 'function') {
            inst.setContent({ '.tooltip-inner': html });
        }
    }
}

function OpenKeyBindingsDialog() {
    DoModalDialog({
        id: 'keyBindingsDlg',
        title: "<i class='fas fa-keyboard'></i> Configure Key Bindings",
        body: '<div id="keyBindingsDlgBody"></div>',
        open: function () {
            $('#keyBindingsDlg').find('.modal-dialog').addClass('modal-xl');
        },
        close: function () {
            $('#keyBindingsEditorHolder').append($('#keyBindingsEditor'));
            KeyBindingsUpdateTip();
        },
        buttons: {
            Close: function () {
                CloseModalDialog('keyBindingsDlg');
            }
        }
    });
    // Move the live editor (with its handlers and recorded rows) into the
    // dialog; the close callback above moves it back to its hidden holder.
    $('#keyBindingsDlgBody').append($('#keyBindingsEditor'));
}

function KeyBindingsInit() {
    if ($('#keyBindingsBody').length === 0) {
        return;
    }
    if ($('#keyBindingsBody').data('initialized')) {
        return;
    }
    $('#keyBindingsBody').data('initialized', true);
    KeyBindingsGetAll().forEach(function (b) {
        $('#keyBindingsBody').append(KeyBindingsMakeRow(b));
    });
    KeyBindingsUpdateTip();
    $.get('api/commandPresets?names=true', function (data) {
        keyBindingsPresetNames = Array.isArray(data) ? data : [];
        // Fill any preset dropdowns rendered before the names arrived.
        $('#keyBindingsBody > tr').each(function () {
            var row = $(this);
            if (row.find('.keyBindingsType').val() === 'preset') {
                var current = row.find('.keyBindingsPreset').val();
                KeyBindingsRenderDetail(row, { preset: current });
            }
        });
    });
}

$(document).ready(function () {
    KeyBindingsInit();
});
</script>
<? } ?>
