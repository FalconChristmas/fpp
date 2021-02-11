<?
$skipJSsettings = 1;
require_once('common.php');
?>

<script>
function ConfigureEmail() {
    $.ajax({
        url: 'api/email/configure',
        type: 'POST',
        async: true,
        success: function(data) {
            $.jGrowl("Email configured.",{themeState:'success'});
        },
        error: function() {
            DialogError('Error configuring email', 'Error configuring email');
        }
    });
}

function SendTestEmail() {
    $.ajax({
        url: 'api/email/test',
        type: 'POST',
        async: true,
        success: function(data) {
            $.jGrowl("Test Email sent.",{themeState:'success'});
        },
        error: function() {
            DialogError('Error sending email', 'Error sending email');
        }
    });
}

</script>

<?
$extraData = "<div class='form-actions'><input type='button' class='buttons' value='Configure Email' onClick='ConfigureEmail();'> <input type='button' class='buttons' value='Send Test Email' onClick='SendTestEmail();'></div>";
PrintSettingGroup('email', $extraData);
?>
