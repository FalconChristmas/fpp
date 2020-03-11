<?
$skipJSsettings = 1;
require_once('common.php');
?>

<script>
function ConfirmPasswordEnable()
{
    var password = $('#password').val();

    if (($('#passwordEnable').is(':checked')) &&
        ((password == '') ||
         (confirm('Click "OK" to reset the existing password to "falcon" before enabling, click "Cancel" to reuse the existing saved password.  Warning: If you do not know the existing password, enabling without resetting could lock you out of the system.  The default password is "falcon" if you have not previously set a UI password.')))) {
        $('#password').val('falcon');
        window["passwordChanged"]();
        $('#passwordVerify').val('falcon');
        window["passwordVerifyChanged"]();
    }

    window["passwordEnableChanged"]();

    if (password == '') {
        alert('UI Password enabled using default password of "falcon"');
    }
}

function UpdatePasswordFieldsType() {
    if ($('#passwordShow').is(':checked')) {
        $('#password').attr('type', 'text');
        $('#passwordVerify').attr('type', 'text');
    } else {
        $('#password').attr('type', 'password');
        $('#passwordVerify').attr('type', 'password');
    }
}

function ShowHidePassword() {
    UpdatePasswordFieldsType();

    window["passwordShowChanged"]();
}

function ValidatePassword(password)
{
    // Allow minimum of 6 so default 'falcon' password is valid
    if (password.length < 6) {
        DialogError('Password Length', 'Password Length should be 6 or more characters');
        return 0;
    }

    return 1;
}

function CheckPassword()
{
    var password = $('#password').val();
    var passwordVerify = $('#passwordVerify').val();

    if (password == passwordVerify) {
        if (ValidatePassword(password)) {
            window["passwordVerifyChanged"]();
            window["passwordChanged"]();
        }
    } else {
        $('#passwordVerify').val('');
        $('#passwordVerify').focus();
    }
}

function CheckPasswordVerify()
{
    var password = $('#password').val();
    var passwordVerify = $('#passwordVerify').val();

    if (password == passwordVerify) {
        if (ValidatePassword(password)) {
            window["passwordVerifyChanged"]();
            window["passwordChanged"]();
        }
    } else {
        $('#password').val('');
        $('#password').focus();
    }
}

</script>

<table class='settingsTableWrapper'>
    <tr><td>
<?
PrintSettingGroup('ui');
?>

        </td>
    </tr>
    <tr>
        <td>
            <b>UI Password</b><br>
            <table class='settingsTable settingsGroupTable'>
<?
PrintSetting('passwordEnable');
?>
                <tr class='passwordEnableChild'><th>Username:</th><td>admin</td></tr>
<?
PrintSetting('password');
PrintSetting('passwordVerify');
PrintSetting('passwordShow');
?>
            </table>
        </td>
    </tr>
</table>

<script>
UpdatePasswordFieldsType();
</script>
