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
        applyPasswordEnable();
    }
}

$( document ).ready(function() {
    if ($('#passwordEnable').val() == '1') {
        $('.passwordEnableChild').show();
    }

    // Replace the auto-generated onChange handlers for the UI password settings
    // so they stage the value instead of saving it immediately.
    $.each(['passwordEnable', 'password', 'passwordVerify'], function (i, name) {
        window[name + 'Changed'] = function () {
            settings[name] = $('#' + name).val();

            if (typeof window['Update' + name + 'Children'] === 'function') {
                window['Update' + name + 'Children'](0);
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
            <div class='row'>
                <div class="printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2"></div>
                <div class='printSettingFieldCol col-md'>
                    <input type='button' class='buttons btn-success' id='saveUIPasswordBtn'
                        value='Save UI Password' onClick='SaveUIPasswordSettings();'>
                    <span id='uiPasswordSaveStatus' class='ms-2 text-muted'></span>
                </div>
            </div>
<?
PrintSettingGroup('uiColors');
?>
