<?php $skipJSsettings = 1; //because config.php outputs JS which it shouldn't be doing anyway ?>
<?php require_once('common.php'); ?>
<?php
//TODO Backup/Restore of events
//TODO Backup/Restore of scripts
//TODO fppMode in general settings file handling

/**
 * Define entries map for backup/restore area select lists
 */
$system_config_aras = array(
    'all' => array('friendly_name' => 'All', 'file' => false),
    'channelOutputsJSON' => array('friendly_name' => 'Channel Outputs (RGBMatrix)', 'file' => $settings['channelOutputsJSON']),
    'channeloutputs' => array('friendly_name' => 'Channel Outputs (Other eg, LOR, Renard, Tricks-C)', 'file' => $settings['channelOutputsFile']),
    'channelmemorymaps' => array('friendly_name' => 'Channel Memory Maps', 'file' => $settings['channelMemoryMapsFile']),
    'email' => array('friendly_name' => 'Email', 'file' => false),
    'schedule' => array('friendly_name' => 'Schedule', 'file' => $scheduleFile),
    'settings' => array('friendly_name' => 'Settings', 'file' => $settingsFile),
    'timezone' => array('friendly_name' => 'Timezone', 'file' => $timezoneFile),
//    'pixelnetDMX' => array('friendly_name' => 'FPP Pixlenet/DMX', 'file' => $pixelnetFile),
    'universes' => array('friendly_name' => 'Universes', 'file' => $universeFile),
//    'wlan' => array('friendly_name' => 'WLAN', 'file' => $settings['configDirectory']. "/interface.wlan0")

);

$keepMasterSlaveSettings = null;
$keepNetworkSettings = null;

//list of settings restored
$settings_restored = array();
$restore_done = false;

/**
 * Handle POST for download or restore
 * Check which submit button was pressed
 */
if (isset($_POST['btnDownloadConfig'])) {
    //Backup area value
    if (isset($_POST['backuparea']) && !empty($_POST['backuparea'])) {
        $area = $_POST['backuparea'];
        //this value *SHOULD* directly match a key in $system_config_aras

        //temp data
        $tmp_settings_data = array();

        if (array_key_exists($area, $system_config_aras)) {

            //Create a copy of the areas array to manipulate
            $tmp_config_areas = $system_config_aras;

            //Generate backup for selected area
            if (strtolower($area) == "all") {
                //combine all backup areas into a single file

                //remove the 'all' key and do some processing of areas array

                unset($tmp_config_areas['all']);
                //remove email as its in the general settings file
                unset($tmp_config_areas['email']);

                foreach ($tmp_config_areas as $key => $key_data) {
                    $setting_file = $key_data['file'];

                    if ($setting_file !== false && file_exists($setting_file)) {
                        if ($key == "settings") {
                            //parse ini
                            $file_data = parse_ini_string(file_get_contents($setting_file));
                        } else {
                            $file_data = file_get_contents($setting_file);
                        }

                        //TODO issue with new lines

                        $tmp_settings_data[$key] = $file_data;
                    }
                }
            } else if (strtolower($area) == "email") {
                //special treatment for email settings as they are inside the general settings file
                $email_settings = array(
                    'emailenable' => $settings['emailenable'],
                    'emailguser' => $settings['emailguser'],
                    'emailfromtext' => $settings['emailfromtext'],
                    'emailtoemail' => $settings['emailtoemail']
                );

                $tmp_settings_data['email'] = $email_settings;
            } else {
                //All other backup areas
                $setting_file = $tmp_config_areas[$area]['file'];

                if ($setting_file !== false && file_exists($setting_file)) {
                    if ($key == "settings") {
                        //parse ini properly
                        $file_data = parse_ini_string(file_get_contents($setting_file));
                    } else {

                        $file_data = file_get_contents($setting_file);
                    }

                    $tmp_settings_data[$key] = $file_data;
                }
            }

            //DO IT!
            if (!empty($tmp_settings_data)) {
                do_backup_download($tmp_settings_data, $area);
            }
        }
    }

} else if (isset($_POST['btnRestoreConfig'])) {

    //RESTORE
    if (isset($_POST['restorearea']) && !empty($_POST['restorearea'])) {
        $restore_area = $_POST['restorearea'];

        if (isset($_POST['keepExitingNetwork']) && !empty($_POST['keepExitingNetwork'])) {
            $keepNetworkSettings = $_POST['keepExitingNetwork'];
        }
        if (isset($_POST['keepMasterSlave']) && !empty($_POST['keepMasterSlave'])) {
            $keepMasterSlaveSettings = $_POST['keepMasterSlave'];
        }

        //this value *SHOULD* directly match a key in $system_config_aras
        if (array_key_exists($restore_area, $system_config_aras)) {
            //Do something with the uploaded file to restore it

            if (array_key_exists('conffile', $_FILES)) {

                //data is stored by area and keyed the same as $system_config_aras
                //read file, decode json
                //parse each area and write settings

                $rstfname = $_FILES['conffile']['name'];
                $rstftmp_name = $_FILES['conffile']['tmp_name'];
                //file contents
                $file_contents = file_get_contents($rstftmp_name);
                $file_contents_decoded = json_decode($file_contents, true);

                //successful decode
                if ($file_contents_decoded !== FALSE) {
                    if (strtolower($restore_area) == "all") {

                        //read each area and process it
                        foreach ($file_contents_decoded as $restore_area_key => $area_data) {
                            process_restore_data($restore_area_key, $area_data);
                        }
                    } else if (strtolower($restore_area) == "email") {

                        if (array_key_exists('email', $file_contents_decoded)) {
                            $emailguser = $file_contents_decoded['settings']['emailguser'];
                            $emailgpass = "";//Not available when doing anything except full backups
                            $emailfromtext = $file_contents_decoded['settings']['emailfromtext'];
                            $emailtoemail = $file_contents_decoded['settings']['emailtoemail'];

                            $settings_restored[] = $restore_area;

                            SaveEmailConfig($emailguser, $emailgpass, $emailfromtext, $emailtoemail);
                        } else if (array_key_exists('settings', $file_contents_decoded)) {
                            $emailguser = $file_contents_decoded['settings']['emailguser'];
                            $emailgpass = $file_contents_decoded['settings']['emailgpass'];
                            $emailfromtext = $file_contents_decoded['settings']['emailfromtext'];
                            $emailtoemail = $file_contents_decoded['settings']['emailtoemail'];

                            $settings_restored[] = $restore_area;

                            SaveEmailConfig($emailguser, $emailgpass, $emailfromtext, $emailtoemail);
                        }

                    } else {
                        //Process specific restore areas, this work almost like the 'all' area
                        //general settings, but only a matching area is cherry picked

                        //If the key exists in the decoded data
                        if (array_key_exists($restore_area, $file_contents_decoded)) {
                            $restore_area_key = $restore_area;
                            $area_data = $file_contents_decoded[$restore_area_key];
                            process_restore_data($restore_area, $area_data);
                        }
                    }

                    //All processed
                    $restore_done = true;
                } else {
                    error_log("The backup " . $rstfname . " could not be decoded properly.");
                }
            }
        }
    }
}


/**
 * Function to look after backup restoration
 */
function process_restore_data($restore_area, $area_data)
{
    global $system_config_aras, $keepMasterSlaveSettings, $keepNetworkSettings, $settings_restored;
    global $args;

    require_once('fppjson.php');
    require_once('fppxml.php');

    $restore_area_key = $restore_area;

    if ($restore_area_key == "channelOutputsJSON") {
        //Overwrite channel outputs JSON
        $channel_outputs_json_filepath = $system_config_aras['channelOutputsJSON']['file'];
        file_put_contents($channel_outputs_json_filepath, $area_data);
    }

    if ($restore_area_key == "channeloutputs") {
        //Overwrite channel outputs JSON
        $channel_outputs_filepath = $system_config_aras['channeloutputs']['file'];
        file_put_contents($channel_outputs_filepath, $area_data);
    }

    if ($restore_area_key == "channelmemorymaps") {
        //Overwrite channel outputs JSON
        $channelmemorymaps_filepath = $system_config_aras['channelmemorymaps']['file'];
        file_put_contents($channelmemorymaps_filepath, $area_data);
    }

    if ($restore_area_key == "schedule") {
        //Overwrite the schedule file and bump FPPD to reload it
        $schedule_filepath = $system_config_aras['universes']['file'];
        file_put_contents($schedule_filepath, $area_data);
        FPPDreloadSchedule();
    }

    if ($restore_area_key == "settings") {
        $ini_string = parse_ini_string($area_data);

        if (is_array($ini_string)) {


            foreach ($ini_string as $setting_name => $setting_value) {
                //check if we can change it (default value is checked - true)
                if ($setting_name == "fppMode") {
                    if ($keepMasterSlaveSettings == false) {
                        WriteSettingToFile($setting_name, $setting_value);
                    }
                } else {
                    WriteSettingToFile($setting_name, $setting_value);
                }

                //Do special things that require some sort of system change
                //eg. changing piRTC fire off a shell command to set it up
                if ($setting_name == 'piRTC') {
                    SetPiRTC($setting_value);
                } else if ($setting_name == "PI_LCD_Enabled") {
                    if ($setting_value == 1) {
                        $_GET['enabled'] = "true";
                    } else {
                        $_GET['enabled'] = "false";
                    }
                    SetPiLCDenabled();
                } else if ($setting_name == "AudioOutput") {
                    $args['value'] = $setting_value;
                    SetAudioOutput();
                } else if ($setting_name == "volume") {
                    $_GET['volume'] = trim($setting_value);
                    SetVolume();
                } else if ($setting_name == "NTP") {
                    SetNTPState($setting_value);
                }
            }
        } else {
            error_log("RESTORE: Cannot read Settings INI settings. Attempted to parse " . json_encode($area_data));
        }
    }

    if ($restore_area_key == "timezone") {
        SetTimezone($area_data);
    }

//  if ($restore_area_key == "pixelnetDMX") {
//  }

    if ($restore_area_key == "universes") {
        //Just overwrite the universes file
        $universe_filepath = $system_config_aras['universes']['file'];
        file_put_contents($universe_filepath, $area_data);
    }

    $settings_restored[] = $restore_area;
}


function SetTimezone($timezone_setting)
{
    global $mediaDirectory, $SUDO;
    //TODO: Check timezone for validity
    $timezone = urldecode($timezone_setting);
    error_log("RESTORE: Changing timezone to '" . $timezone . "'.");
    exec($SUDO . " bash -c \"echo $timezone > /etc/timezone\"", $output, $return_val);
    unset($output);
    //TODO: check return
    exec($SUDO . " dpkg-reconfigure -f noninteractive tzdata", $output, $return_val);
    unset($output);
    //TODO: check return
    exec(" bash -c \"echo $timezone > $mediaDirectory/timezone\"", $output, $return_val);
    unset($output);
    //TODO: check return
}

function SetPiRTC($pi_rtc_setting)
{
    global $SUDO, $fppDir;

    $piRTC = $pi_rtc_setting;
    WriteSettingToFile("piRTC", $piRTC);
    exec($SUDO . " $fppDir/scripts/piRTC set");
    error_log("RESTORE: Set RTC:" . $piRTC);
}

/**
 * Starts the download in the browser
 * @param $settings_data array of settings data
 * @param $area Area the download was for
 */
function do_backup_download($settings_data, $area)
{
    global $settings;

    //Once we have all the settings, process the array and dump it back to the user
    //filename
    $backup_fname = $settings['HostName'] . "_" . $area . "-settings_" . date("YmdHis") . ".json";
    $backup_local_fpath = $settings['configDirectory'] . "/" . $backup_fname;

    //Write data into backup file
    file_put_contents($backup_local_fpath, json_encode($settings_data));

    ///Generate the headers to prompt browser to start download
    header("Content-Disposition: attachment; filename=\"" . $backup_fname . "\"");
    header("Content-Type: application/json");
    header("Content-Length: " . filesize($backup_local_fpath));
    header("Connection: close");
    //Output the file
    readfile($backup_local_fpath);
    //die
    exit;
}

/**
 * Generate backup/restore area select list
 * @param string $area_name Either backuparea or restorearea for either section
 * @return string
 */
function backup_gen_select($area_name = "backuparea")
{
    global $system_config_aras;

    $select_html = "<select name=\"$area_name\" id=\"$area_name\">";
    foreach ($system_config_aras as $item => $item_options) {
        $select_html .= "<option value=" . $item . ">" . $item_options['friendly_name'] . "</option>";
    }
    $select_html .= "</select>";

    return $select_html;
}

?>

<!DOCTYPE html>
<html>
<head>
    <?php include 'common/menuHead.inc'; ?>

    <title><? echo $pageTitle; ?></title>
</head>
<body>
<div id="bodyWrapper">
    <?php include 'menu.inc'; ?>
    <br/>

    <form action="backup.php" method="post" name="frmBackup" enctype="multipart/form-data">

        <div id="global" class="settings">
            <fieldset>
                <legend>FPP Settings Backup</legend>
                <?php if ($restore_done == true) {
                    ?>
                    <div id="restoreSuccessFlag">Backup Restored. A Reboot May Be Required</div>
                    <div id="restoreSuccessFlag">What was
                        restored: <?php echo implode(", ", $settings_restored); ?></div>
                <?php
                }
                ?>
                <fieldset>
                    <legend>Backup Configuration</legend>
                    Click this button to download the system configuration in JSON format.
                    <br/>

                    <table width="100%">
                        <tr>
                            <td width="25%">Backup Area</td>
                            <td width="75%"><?php echo backup_gen_select('backuparea'); ?></td>
                        </tr>
                        <tr>
                            <td width="25%"></td>
                            <td width="75%">
                                <input name="btnDownloadConfig" type="Submit" style="width:30%" class="buttons"
                                       value="Download Configuration">
                            </td>
                        </tr>

                    </table>
                </fieldset>
                <br/>
                <fieldset>
                    <legend>Restore Configuration</legend>
                    Open a configuration JSON file and click the button below to restore the
                    configuration.
                    <br/>
                    <table width="100%">

                        <!--                        <tr>-->
                        <!--                            <td width="35%"><b>Keep Existing Network Settings</b></td>-->
                        <!--                            <td width="65%">-->
                        <!--                                <input name="keepExitingNetwork"-->
                        <!--                                       type="checkbox"-->
                        <!--                                       checked="true">-->
                        <!--                            </td>-->
                        <!--                        <tr/>-->

                        <tr>
                            <td width="35%"><b>Keep Existing Master/Slave Settings</b></td>
                            <td width="65%">
                                <input name="keepMasterSlave"
                                       type="checkbox"
                                       checked="true">
                            </td>
                        <tr/>

                        <tr>
                            <td width="25%">Restore Area</td>
                            <td width="75%">
                                <?php echo backup_gen_select('restorearea'); ?></td>
                        <tr/>

                        <tr>
                            <td width="25%"></td>
                            <td width="75%">
                                <input name="conffile" type="file" class="formbtn" id="conffile" size="50"
                                       autocomplete="off"></td>
                        <tr/>

                        <tr>
                            <td width="25%"></td>

                            <td width="75%">
                                <input name="btnRestoreConfig" type="Submit" style="width:30%" class="buttons"
                                       value="Restore  Configuration">
                            </td>
                        </tr>
                    </table>

                </fieldset>
            </fieldset>
        </div>
    </form>

    <?php include 'common/footer.inc'; ?>
</body>
</html>
