<?
$skipJSsettings = 1;
require_once('common.php');

/////////////////////////////////////////////////////////////////////////////
// Set a sane audio device if not set already
if (
    (!isset($settings['AudioOutput'])) ||
    ($settings['AudioOutput'] == '')
) {

    exec($SUDO . " grep card /root/.asoundrc | head -n 1 | awk '{print $2}'", $output, $return_val);
    if ($return_val) {
        error_log("Error getting currently selected alsa card used!");
    } else {
        if (isset($output[0]))
            $settings['AudioOutput'] = $output[0];
        else
            $settings['AudioOutput'] = "0";
    }
    unset($output);
}

/////////////////////////////////////////////////////////////////////////////
// Set a sane audio mixer device if not set already
if (!isset($settings['AudioMixerDevice'])) {
    if ($settings['BeaglePlatform']) {
        $settings['AudioMixerDevice'] = exec($SUDO . " amixer -c " . $settings['AudioOutput'] . " scontrols | head -1 | cut -f2 -d\"'\"", $output, $return_val);
        if ($return_val) {
            $settings['AudioMixerDevice'] = "PCM";
        }
    } else {
        $settings['AudioMixerDevice'] = "PCM";
    }
}

/////////////////////////////////////////////////////////////////////////////

?>

<?
PrintSettingGroup('avModes');

PrintSettingGroup('generalAudio');

// PipeWire-advanced section — always rendered, visibility controlled dynamically by MediaBackend
{
    $mediaBackend = isset($settings['MediaBackend']) ? $settings['MediaBackend'] : 'alsa';
    $isPipeWireAdv = ($mediaBackend === 'pipewire');
    $isPipeWireSimple = ($mediaBackend === 'pipewire-simple');
    $isAlsa = (!$isPipeWireAdv && !$isPipeWireSimple);
    ?>
    <div id="pipeWireSection" <?= $isPipeWireAdv ? '' : ' style="display:none;"' ?>>
        <h2>PipeWire Routing</h2>
        <?
        PrintSettingGroup('pipeWireGeneral', '', '', 1, '', '', false);
        ?>

        <h2>PipeWire Audio</h2>

        <?
        PrintSettingGroup('pipeWireAudio', '', '', 1, '', '', false);
        ?>

        <h2>PipeWire Network Streams</h2>

        <?
        PrintSettingGroup('pipeWireStreams', '', '', 1, '', '', false);
        ?>


    </div>

    <div id="alsaHardwareAudioSection" <?= $isPipeWireAdv ? ' style="display:none;"' : '' ?>>
        <h2 id="alsaHardwareAudioHeader"><?= $isPipeWireSimple ? 'Simple PipeWire Audio' : 'ALSA Hardware Audio' ?></h2>
        <? PrintSettingGroup('alsaHardwareAudio', '', '', 1, '', '', false); ?>
    </div>
    <script>
        (function () {
            function updateAvSections(val) {
                if (val === 'pipewire') {
                    $('#pipeWireSection').show();
                    $('#pipeWireVideoSection').show();
                    $('#alsaHardwareAudioSection').hide();
                    $('#hardwareDirectVideoSection').hide();
                } else if (val === 'pipewire-simple') {
                    $('#pipeWireSection').hide();
                    $('#pipeWireVideoSection').hide();
                    $('#alsaHardwareAudioSection').show();
                    $('#hardwareDirectVideoSection').show();
                    $('#alsaHardwareAudioHeader').text('Simple PipeWire Audio');
                    $('#hardwareDirectVideoHeader').text('Simple PipeWire Video');
                    // AudioOutput / VideoOutput live in the 'alsa' children
                    // list, so the children logic hides them here.  Re-show
                    // them — they are the primary controls in Simple mode.
                    $('#AudioOutputRow').show();
                    $('#VideoOutputRow').show();
                } else {
                    $('#pipeWireSection').hide();
                    $('#pipeWireVideoSection').hide();
                    $('#alsaHardwareAudioSection').show();
                    $('#hardwareDirectVideoSection').show();
                    $('#alsaHardwareAudioHeader').text('ALSA Hardware Audio');
                    $('#hardwareDirectVideoHeader').text('Hardware Direct Video');
                }
            }
            var origChildFn = window.UpdateMediaBackendChildren;
            if (typeof origChildFn === 'function') {
                window.UpdateMediaBackendChildren = function (mode) {
                    origChildFn(mode);
                    updateAvSections($('#MediaBackend').val());
                };
            }
            // Apply initial visibility immediately so the correct state is set
            // before UpdateChildSettingsVisibility() runs after tab injection.
            updateAvSections($('#MediaBackend').val());
        })();
    </script>
    <?
}
?>

<div id="pipeWireVideoSection" <?= ($mediaBackend === 'pipewire') ? '' : ' style="display:none;"' ?>>
    <?
    PrintSettingGroup('pipeWireVideo');
    ?>
</div>

<div id="hardwareDirectVideoSection" <?= ($mediaBackend === 'pipewire') ? ' style="display:none;"' : '' ?>>
    <h2 id="hardwareDirectVideoHeader">
        <?= ($mediaBackend === 'pipewire-simple') ? 'Simple PipeWire Video' : 'Hardware Direct Video' ?>
    </h2>
    <? PrintSettingGroup('generalVideo', '', '', 1, '', '', false); ?>
</div>