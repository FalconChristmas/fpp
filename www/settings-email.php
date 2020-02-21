<script>
function ConfigureEmail() {
    $.ajax({
        url: '/api/email/configure',
        type: 'POST',
        async: true,
        success: function(data) {
            $.jGrowl("Email configured.");
        },
        fail: function() {
            DialogError('Error configuring email', 'Error configuring email');
        }
    });
}

function SendTestEmail() {
    $.ajax({
        url: '/api/email/test',
        type: 'POST',
        async: true,
        success: function(data) {
            $.jGrowl("Test Email sent.");
        },
        fail: function() {
            DialogError('Error sending email', 'Error sending email');
        }
    });
}

$(document).ready(function(){
    if ($('#emailenable').is(':checked')) {
        $('.emailenableChild').show();
    } else {
        $('.emailenableChild').hide();
    }
});
</script>

<?
$extraData = "<span class='emailenableChild'>
<input type='button' value='Configure Email' onClick='ConfigureEmail();'>
<input type='button' value='Send Test Email' onClick='SendTestEmail();'>
</span>";
PrintSettingGroup('email', $extraData);
?>
