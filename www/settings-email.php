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
$extraData = "<input type='button' value='Configure Email' onClick='ConfigureEmail();'> <input type='button' value='Send Test Email' onClick='SendTestEmail();'>";
PrintSettingGroup('email', $extraData);
?>
