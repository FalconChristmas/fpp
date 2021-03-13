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


<?
PrintSettingGroup('ui');
PrintSettingGroup('uiColors');
?>


            <b>UI Password</b><br>

<?
PrintSetting('passwordEnable');
?>
                <div class='tr passwordEnableChild'><div class="th">Username:</div><div class='td'>admin</div></div>
<?
PrintSetting('password');
PrintSetting('passwordVerify');
?>
