<?
$skipJSsettings = 1;
require_once 'common.php';

$showOSSecurity = 0;
if (file_exists('/etc/fpp/platform') && !file_exists('/etc/fpp/container')) {
    $showOSSecurity = 1;
}

?>

<script>
function SaveSSHKeys() {
    var keys = $('#sshKeys').val();
    var result = Post('api/configfile/authorized_keys', false, keys);
    if (result.Status == 'OK')
        $.jGrowl("Keys Saved", { themeState: 'success' });
}

function UploadAuthorizedKeys() {
	let authorized_keys = document.getElementById("authorized_keys").files[0];

    if (authorized_keys != '') {
        let formData = new FormData();
        formData.append('file', authorized_keys);

        $.ajax({
            url: 'api/configfile/authorized_keys',
            type: 'POST',
            data: formData,
            contentType: false,
            dataType: 'json',
            processData: false
        }).done(function (data) {
            $.get('api/configfile/authorized_keys'
            ).done(function (data) {
                $('#sshKeys').val(data);
            });
        }).fail(function (data) {
            $.jGrowl("Failed to reload updated keys.", { themeState: 'danger' });
        });
    }
}

var resetAreas = ['config', 'network', 'media', 'sequences', 'effects', 'playlists',
    'channeloutputs', 'eeprom', 'schedule', 'settings', 'uploads', 'logs', 'plugins',
    'pluginConfigs', 'user', 'caches', 'scripts', 'backups', 'audiobackend'];
function AllButtonClicked() {
    for (var i = 0; i < resetAreas.length; i++) {
        if (resetAreas[i] != 'network')
            $('#rc_' + resetAreas[i]).prop('checked', true);
    }
}
function ClearButtonClicked() {
    for (var i = 0; i < resetAreas.length; i++) {
        $('#rc_' + resetAreas[i]).prop('checked', false);
    }
}
function CommonButtonClicked() {
    $('#rc_sequences').prop('checked', true);
    $('#rc_effects').prop('checked', true);
    $('#rc_media').prop('checked', true);
    $('#rc_playlists').prop('checked', true);
}

function ShowResetConfigPopup() {
    DoModalDialog({
        id: "resetFPPDialog",
        class: "modal-xl",
        title: "Reset FPP Config",
        body: $("#resetConfigMenu"),
        backdrop: true,
        keyboard: true,
        buttons: {
            Reset: function() {
                ResetConfig();
            },
            Close: function() {
                CloseModalDialog("resetFPPDialog");
            }
        }
    });
}

function ResetConfig() {
    if (confirm('Are you sure you want to reset the speficied FPP config areas?')) {
        CloseModalDialog("resetFPPDialog");
        DoModalDialog({
            id: "doResetFPPConfigDialog",
            class: "modal-lg modal-dialog-scrollable",
            title: "Reset FPP Config",
            body: "<textarea class='w-100' style='height: 55vh; min-height: 200px;' disabled id='resetConfigText'></textarea>",
            backdrop: "static",
            keyboard: false,
            noClose: true,
            buttons: {
                Close: {
                    text: 'Please Wait',
                    click: function() {
                        CloseModalDialog("doResetFPPConfigDialog");
                    },
                    disabled: true,
                    id: "doResetFPPConfigDialogCloseButton"
                }
            }
        });
        // Reset the button on reopen (DoModalDialog reuses the modal without rebuilding its footer).
        $('#doResetFPPConfigDialogCloseButton').prop('disabled', true).text('Please Wait');
        var args = '';
        for (var i = 0; i < resetAreas.length; i++) {
            if ($('#rc_' + resetAreas[i]).is(':checked'))
                args += resetAreas[i] + ',';
        }

        if (args != '')
            args = '?areas=' + args + 'dummy';

        StreamURL('resetConfig.php' + args, 'resetConfigText', 'ResetConfigDone');
    }
}

function ResetConfigDone() {
    SetRebootFlag();
    EnableModalDialogCloseButton("doResetFPPConfigDialog");
}


<?if ($showOSSecurity) {?>
$( document ).ready(function() {
    if ($('#osPasswordEnable').val() == '1') {
        $('.osPasswordEnableChild').show();
    }
});
<?}?>

</script>
<?
if (file_exists("/etc/fpp/container") || $settings["IsDesktop"]) {
    PrintSettingGroup('hostDesktop');
}

PrintSettingGroup('system');
PrintFanThermalSettings();
PrintSettingGroup('BBBLeds');

if ($showOSSecurity) {
    ?>
    <b>OS Password</b><br>
<?
    PrintSetting('osPasswordEnable');
    ?>
    <div class='row osPasswordEnableChild d-none'>
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
    <span id="ssh_tip" class="tooltip d-none">Add optional SSH key(s) for passwordless SSH authentication.</span><br>
    <textarea  id='sshKeys' style='width: 100%;' rows='10'><?echo shell_exec('sudo cat /root/.ssh/authorized_keys'); ?></textarea>
    <input type='button' class='buttons' value='Save Keys' onClick='SaveSSHKeys();'>&nbsp;&nbsp;<b>OR</b>&nbsp;&nbsp;
    <input id='UploadAuthorizedKeys' type='button' class='buttons' value='Upload authorized_keys' onClick='UploadAuthorizedKeys();' disabled> <input type="file" id="authorized_keys" style='padding-left: 0px;' onChange="$('#UploadAuthorizedKeys').prop('disabled', false);"><br>
    <br>
<?
    }
}

if ($settings['uiLevel'] >= 1) {
    ?>
&nbsp<i class="fas fa-fw fa-graduation-cap ui-level-1"></i>
&nbsp<input type='button' class='buttons' onClick='ShowResetConfigPopup();' value='Reset FPP Config'>
<img id="Reset_fpp_img" title="This will allow you to reset your controller to factory settings.
You can individually select what settings you want to reset." src="images/redesign/help-icon.svg" class="icon-help" exifid="912897540" oldsrc="http://192.168.1.200/images/redesign/help-icon.svg">
<?
}
?>

<div id='resetPopup' title='Reset FPP Config' class="d-none">
    <span id='resetConfigMenu'>
        <div class="mb-3">
            <b class="d-block mb-2">Choose areas to reset:</b>
            <div class="mb-1"><input id="allButton" class="buttons" value="Everything" onClick="AllButtonClicked()">&nbsp;Everything (includes everything below except network)</div>
            <div class="mb-1"><input id="commonButton" class="buttons" value="Common" onClick="CommonButtonClicked()">&nbsp;Sequences/Media/Playlist</div>
            <div><input id="noneButton" class="buttons" value="Nothing" onClick="ClearButtonClicked()">&nbsp;Clears all checkboxes</div>
        </div>

        <div class="container-fluid settingsTable settingsGroupTable">
            <div class="row">
                <div class="col-12 col-md-6">
                    <div class="mb-3">
                        <b class="d-block mb-2">Configuration</b>
                        <div class="d-flex align-items-center gap-2 mb-1"><input type='checkbox' id='rc_config'><label for="rc_config">Configuration Files</label></div>
                        <div class="d-flex align-items-center gap-2 mb-1"><input type='checkbox' id='rc_network'><label for="rc_network">Network Config Files</label></div>
                        <div class="d-flex align-items-center gap-2 mb-1"><input type='checkbox' id='rc_channeloutputs'><label for="rc_channeloutputs">Channel Outputs</label></div>
                        <div class="d-flex align-items-center gap-2 mb-1"><input type='checkbox' id='rc_eeprom'><label for="rc_eeprom">EEPROM / String Config</label></div>
                        <div class="d-flex align-items-center gap-2 mb-1"><input type='checkbox' id='rc_settings'><label for="rc_settings">Settings</label></div>
                        <div class="d-flex align-items-center gap-2 mb-1"><input type='checkbox' id='rc_schedule'><label for="rc_schedule">Schedule</label></div>
                    </div>
                    <div class="mb-3 mb-md-0">
                        <b class="d-block mb-2">Content</b>
                        <div class="d-flex align-items-center gap-2 mb-1"><input type='checkbox' id='rc_sequences'><label for="rc_sequences">Sequences</label></div>
                        <div class="d-flex align-items-center gap-2 mb-1"><input type='checkbox' id='rc_media'><label for="rc_media">Media</label></div>
                        <div class="d-flex align-items-center gap-2 mb-1"><input type='checkbox' id='rc_effects'><label for="rc_effects">Effects</label></div>
                        <div class="d-flex align-items-center gap-2 mb-1"><input type='checkbox' id='rc_playlists'><label for="rc_playlists">Playlists</label></div>
                    </div>
                </div>
                <div class="col-12 col-md-6">
                    <div class="mb-3">
                        <b class="d-block mb-2">Plugins</b>
                        <div class="d-flex align-items-center gap-2 mb-1"><input type='checkbox' id='rc_plugins'><label for="rc_plugins">Installed Plugins</label></div>
                        <div class="d-flex align-items-center gap-2 mb-1"><input type='checkbox' id='rc_pluginConfigs'><label for="rc_pluginConfigs">Plugin Config Files</label></div>
                    </div>
                    <div>
                        <b class="d-block mb-2">OS / System Files</b>
                        <div class="d-flex align-items-center gap-2 mb-1"><input type='checkbox' id='rc_logs'><label for="rc_logs">Logs</label></div>
                        <div class="d-flex align-items-center gap-2 mb-1"><input type='checkbox' id='rc_backups'><label for="rc_backups">Backups</label></div>
                        <div class="d-flex align-items-center gap-2 mb-1"><input type='checkbox' id='rc_uploads'><label for="rc_uploads">Uploads</label></div>
                        <div class="d-flex align-items-center gap-2 mb-1"><input type='checkbox' id='rc_caches'><label for="rc_caches">Caches</label></div>
                        <div class="d-flex align-items-center gap-2 mb-1"><input type='checkbox' id='rc_scripts'><label for="rc_scripts">Scripts</label></div>
                        <div class="d-flex align-items-center gap-2 mb-1"><input type='checkbox' id='rc_user'><label for="rc_user">Root/FPP User Files <span class="text-body-secondary">(ssh keys, history)</span></label></div>
                        <div class="d-flex align-items-center gap-2 mb-1"><input type='checkbox' id='rc_audiobackend'><label for="rc_audiobackend">Audio Backend (PipeWire)</label></div>
                    </div>
                </div>
            </div>
        </div>


    </span>
</div>

