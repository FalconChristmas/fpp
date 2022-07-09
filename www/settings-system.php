<?
$skipJSsettings = 1;
require_once('common.php');

$showOSSecurity = 0;
if (file_exists('/etc/fpp/platform') && !file_exists('/.dockerenv'))
    $showOSSecurity = 1;
?>

<script>
function SaveSSHKeys() {
    var keys = $('#sshKeys').val();
    var result = Post('api/configfile/authorized_keys', false, keys);
    if (result.Status == 'OK')
        $.jGrowl("Keys Saved", { themeState: 'success' });
}

function KioskInstallDone() {
    SetRebootFlag();
    $('#kioskCloseDialogButton').show();
}
function DisableKiosk() {
    $('#kioskCloseDialogButton').hide();
    $('#kioskPopup').fppDialog({ height: 600, width: 900, title: "Kiosk Frontend", dialogClass: 'no-close' });
    $('#kioskPopup').fppDialog( "moveToTop" );
    $('#kioskInstallText').html('');

    StreamURL('disableKiosk.php', 'kioskInstallText', 'KioskInstallDone');
}

function EnableKiosk() {
    if (confirm('Installing Kiosk components will take some time and consume around 400MB of space.')) {
        $('#kioskCloseDialogButton').hide();
        $('#kioskPopup').fppDialog({ height: 600, width: 900, title: "Kiosk Frontend", dialogClass: 'no-close' });
        $('#kioskPopup').fppDialog( "moveToTop" );
        $('#kioskInstallText').html('');

        StreamURL('installKiosk.php', 'kioskInstallText', 'KioskInstallDone');
    }
}

function CloseKioskDialog() {
    $('#kioskPopup').fppDialog('close');
    SetRebootFlag();
}

var resetAreas = ['config', 'media', 'sequence', 'playlists', 'channeloutputs', 'schedule', 'settings', 'uploads', 'logs', 'plugins', 'pluginConfigs', 'user'];
function ResetAllChanged() {
    var checked = $('#rc_all').is(':checked');

    for (var i = 0; i < resetAreas.length; i++) {
        $('#rc_' + resetAreas[i]).prop('checked', checked);
    }
}

function ShowResetConfigPopup() {
    $('#resetConfigMenu').show();
    $('#resetCloseDialogButton').show();
    $('#resetPopup').fppDialog({ height: 600, width: 900, title: "Reset FPP Config", dialogClass: 'no-close' });
    $('#resetPopup').fppDialog( "moveToTop" );
    $('#resetConfigText').hide();
    $('#resetConfigText').val('');
}

function ResetConfig() {
    if (confirm('Are you sure you want to reset the speficied FPP config areas?')) {
        $('#resetConfigMenu').hide();
        $('#resetConfigText').show();
        $('#resetCloseDialogButton').hide();

        var args = '';

        if ($('#rc_all').is(':checked')) {
            args = '?areas=all';
        } else {
            for (var i = 0; i < resetAreas.length; i++) {
                if ($('#rc_' + resetAreas[i]).is(':checked'))
                    args += resetAreas[i] + ',';
            }

            if (args != '')
                args = '?areas=' + args + 'dummy';
        }


        StreamURL('resetConfig.php' + args, 'resetConfigText', 'ResetConfigDone');
    }
}

function ResetConfigDone() {
    SetRebootFlag();
    $('#resetCloseDialogButton').show();
}

function CloseResetDialog() {
    $('#resetPopup').fppDialog('close');
    SetRebootFlag();
}

<? if ($showOSSecurity) { ?>
$( document ).ready(function() {
    if ($('#osPasswordEnable').val() == '1') {
        $('.osPasswordEnableChild').show();
    }
});
<? } ?>

</script>
<?
if (file_exists("/.dockerenv") || $settings["IsDesktop"]) {
    PrintSettingGroup('hostDesktop');
}

PrintSettingGroup('system');
PrintSettingGroup('BBBLeds');

$extraData = "<div class='form-actions'><input type='button' class='buttons' value='Lookup Location' onClick='GetGeoLocation();'> <input type='button' class='buttons' value='Show On Map' onClick='ViewLatLon();'></div>";
PrintSettingGroup('geolocation', $extraData);

if ($showOSSecurity) {
?>
    <b>OS Password</b><br>
<?
    PrintSetting('osPasswordEnable');
?>
    <div class='row osPasswordEnableChild' style='display: none;'>
        <div class="printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2">
            <div class='description'><i class="fas fa-fw fa-nbsp ui-level-0"></i>Username
            </div>
        </div>
        <div class='printSettingFieldCol col-md'><input disabled value='fpp' size='5'></div>
    </div>
<?
    PrintSetting('osPassword');
    PrintSetting('osPasswordVerify');

    if ($uiLevel >= 1) {
?>
    <br>
    <i class="fas fa-fw fa-graduation-cap fa-nbsp ui-level-1" title="Advanced Level Setting"></i>
    <b>SSH Keys</b> (root and fpp users)
    <img id="ssh_img" title="Add optional SSH key(s) for passwordless SSH authentication." src="images/redesign/help-icon.svg" width=22 height=22>
    <span id="ssh_tip" class="tooltip" style="display: none">Add optional SSH key(s) for passwordless SSH authentication.</span><br>
    <textarea  id='sshKeys' style='width: 100%;' rows='10'><?  echo shell_exec('sudo cat /root/.ssh/authorized_keys'); ?></textarea>
    <input type='button' class='buttons' value='Save Keys' onClick='SaveSSHKeys();'><br><br>
<?
    }
}

if (($settings['uiLevel'] >= 1) && ($settings['Platform'] == "Raspberry Pi")) {
?>
    <i class="fas fa-fw fa-graduation-cap fa-nbsp ui-level-1" title="Advanced Level Setting"></i>
    <b>Kiosk Frontend</b><br>
    The Kiosk frontend installs a bunch of extra packages and sets up Chrome running on the local HDMI port to
    allow FPP to be configured and monitored from a keyboard and mouse connected to the Pi's USB ports.  The
    additional packages take up about 400MB of space.
    <br>
<?
    if (file_exists("/etc/fpp/kiosk")) {
        echo "<input type='button' class='buttons' value='Disable Kiosk' onClick='DisableKiosk();'>";
    } else {
        echo "<input type='button' class='buttons' value='Enable Kiosk' onClick='EnableKiosk();'>";
    }
}

echo "<br><br>\n";
PrintSettingGroup('services');

if ($settings['uiLevel'] >= 1) {
?>
<input type='button' class='buttons' onClick='ShowResetConfigPopup();' value='Reset FPP Config'>
<?
}
?>

<div id='kioskPopup' title='Kiosk Frontend' style="display: none">
    <textarea style='width: 99%; height: 94%;' disabled id='kioskInstallText'>
    </textarea>
    <input id='kioskCloseDialogButton' type='button' class='buttons' value='Close' onClick='CloseKioskDialog();' style='display: none;'>
</div>

<div id='resetPopup' title='Reset FPP Config' style="display: none">
    <span id='resetConfigMenu'>
        <b>Choose areas to reset:</b><br>
        <input type='checkbox' id='rc_all' onClick='ResetAllChanged();'> - Everything (includes everything below)<br>
        <table border=0 cellpadding=2>
            <tr><td valign='top'><b>FPP</b><br>
                    <input type='checkbox' id='rc_config'> - Configuration Files<br>
                    <input type='checkbox' id='rc_channeloutputs'> - Channel Outputs<br>
                    <input type='checkbox' id='rc_logs'> - Logs<br>
                    <input type='checkbox' id='rc_media'> - Media Files (audio, video, image)<br>
                    <input type='checkbox' id='rc_playlists'> - Playlists<br>
                    </td>
                <td width='10px'></td>
                <td valign='top'><br>
                    <input type='checkbox' id='rc_schedule'> - Schedule<br>
                    <input type='checkbox' id='rc_sequence'> - Sequence and Effects Files<br>
                    <input type='checkbox' id='rc_settings'> - Settings<br>
                    <input type='checkbox' id='rc_uploads'> - Uploads<br>
                    </td>
                <td width='10px'></td>
                <td valign='top'><b>Plugins</b><br>
                    <input type='checkbox' id='rc_plugins'> - Installed Plugins<br>
                    <input type='checkbox' id='rc_pluginConfigs'> - Plugin Config Files<br>
                    </td>
                <td width='10px'></td>
                <td valign='top'><b>OS</b><br>
                    <input type='checkbox' id='rc_user'> - root and fpp user files (ssh keys, bash history)<br>
                    </td>
            </tr>
        </table>
        <br>
        <input type='button' class='buttons' onClick='ResetConfig();' value='Reset'>
    </span>
    <textarea style='width: 99%; height: 92%;' disabled id='resetConfigText' style='display: none;'>
    </textarea>
    <input id='resetCloseDialogButton' type='button' class='buttons' value='Close' onClick='CloseResetDialog();' style='display: none;'>
</div>

