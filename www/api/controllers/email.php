<?
require_once '../common.php';

/**
 * Set email options
 *
 * Configures outbound email using the existing settings.
 *
 * @route-v1 POST /email/configure
 * @route-v2 POST /email/configure
 * @response 200 Email configured
 * ```json
 * {"Status": "OK", "Message": ""}
 * ```
 */
function ConfigureEmail() {
    $result = Array();

    ApplyEmailConfig();

    $result['Status'] = 'OK';
    $result['Message'] = '';

    //Trigger a JSON Configuration Backup
    GenerateBackupViaAPI('Email Configuration was modified.');

    return json($result);
}

/**
 * Send test email
 *
 * Sends a test email using the existing settings.
 *
 * @route-v1 POST /email/test
 * @route-v2 POST /email/test
 * @response 200 Test email sent
 * ```json
 * {"Status": "OK", "Message": ""}
 * ```
 */
function SendTestEmail() {
    global $settings;
    $result = Array();
    $result_code = 0;
    $tmpfname = tempnam("/tmp", "sendmail-stderr.txt");

    system('echo "Email test from $(hostname)" | mail -s "Email test from $(hostname)" ' . $settings['emailtoemail'] . " 2> " . $tmpfname, $result_code); //capture stderr

    $result['Status'] = 'OK'; //maybe not; need to check ret code
    $result['Message'] = '';
    $result['result_code'] = $result_code; //DJ
    $result['stderr'] = file_get_contents($tmpfname);
    unlink($tmpfname);

    return json($result);
}

?>
