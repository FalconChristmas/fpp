<?
require_once '../common.php';

/**
 * Set email options
 *
 * Configures outbound email using the existing settings.
 *
 * @route POST /api/email/configure
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
 * @route POST /api/email/test
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

    $emailTo = isset($settings['emailtoemail']) ? trim($settings['emailtoemail']) : '';
    // escapeshellarg() stops shell injection but not a value that starts with
    // '-', which mail would parse as an option (mailutils has --exec and
    // --config-file).  The same list check the setting is stored with.
    if ($emailTo === '' || !IsValidEmailList($emailTo)) {
        unlink($tmpfname);
        return json(array('Status' => 'ERROR', 'Message' => 'The default TO address is not a valid email address.'));
    }
    $toArg = escapeshellarg($emailTo);
    $tmpArg = escapeshellarg($tmpfname);
    system('echo "Email test from $(hostname)" | mail -s "Email test from $(hostname)" -- ' . $toArg . " 2> " . $tmpArg, $result_code); //capture stderr

    $result['Status'] = 'OK'; //maybe not; need to check ret code
    $result['Message'] = '';
    $result['result_code'] = $result_code; //DJ
    $result['stderr'] = file_get_contents($tmpfname);
    unlink($tmpfname);

    return json($result);
}

?>
