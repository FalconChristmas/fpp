<?
$skipJSsettings = 1;
require_once('common.php');

/////////////////////////////////////////////////////////////////////////////
// Set a sane audio device if not set already
if ((!isset($settings['AudioOutput'])) ||
    ($settings['AudioOutput'] == '')) {
    exec($SUDO . " grep card /root/.asoundrc | head -n 1 | awk '{print $2}'", $output, $return_val);
    if ( $return_val )
    {
        error_log("Error getting currently selected alsa card used!");
    }
    else
    {
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
    if ($settings['Platform'] == "BeagleBone Black")
    {
        $settings['AudioMixerDevice'] = exec($SUDO . " amixer -c " . $settings['AudioOutput'] . " scontrols | head -1 | cut -f2 -d\"'\"", $output, $return_val);
        if ( $return_val )
        {
            $settings['AudioMixerDevice'] = "PCM";
        }
    } else {
        $settings['AudioMixerDevice'] = "PCM";
    }
}

/////////////////////////////////////////////////////////////////////////////

?>

<script>
</script>

<table class='settingsTableWrapper'>
    <tr><td>
<?
PrintSettingGroup('generalAudioVideo');
PrintSettingGroup('omxplayer');
?>

        </td>
    </tr>
</table>
