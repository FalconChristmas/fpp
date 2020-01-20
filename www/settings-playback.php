<?

/////////////////////////////////////////////////////////////////////////////
// Audio support code
exec($SUDO . " grep card /root/.asoundrc | head -n 1 | awk '{print $2}'", $output, $return_val);
if ( $return_val )
{
    error_log("Error getting currently selected alsa card used!");
}
else
{
    if (isset($output[0]))
        $CurrentCard = $output[0];
    else
        $CurrentCard = "0";
}
unset($output);

$AlsaCards = Array();
exec($SUDO . " aplay -l | grep '^card' | sed -e 's/^card //' -e 's/:[^\[]*\[/:/' -e 's/\].*\[.*\].*//' | uniq", $output, $return_val);
if ( $return_val )
{
    error_log("Error getting alsa cards for output!");
}
else
{
    $foundOurCard = 0;
    foreach($output as $card)
    {
        $values = explode(':', $card);

        if ($values[0] == $CurrentCard)
            $foundOurCard = 1;

        if ($values[1] == "bcm2835 ALSA")
            $AlsaCards[$values[1] . " (Pi Onboard Audio)"] = $values[0];
        else if ($values[1] == "CD002")
            $AlsaCards[$values[1] . " (FM Transmitter)"] = $values[0];
        else
            $AlsaCards[$values[1]] = $values[0];
    }

    if (!$foundOurCard)
        $AlsaCards['-- Select an Audio Device --'] = $CurrentCard;
}
unset($output);

$AudioMixerDevice = "PCM";
if (isset($settings['AudioMixerDevice']))
{
    $AudioMixerDevice = $settings['AudioMixerDevice'];
}
else if ($settings['Platform'] == "BeagleBone Black")
{
    $AudioMixerDevice = exec($SUDO . " amixer -c $CurrentCard scontrols | head -1 | cut -f2 -d\"'\"", $output, $return_val);
    if ( $return_val )
    {
        $AudioMixerDevice = "PCM";
    }
}

$MixerDevices = Array();
exec($SUDO . " amixer -c $CurrentCard scontrols | cut -f2 -d\"'\"", $output, $return_val);
if ( $return_val || strpos($output[0], "Usage:") === 0) {
    error_log("Error getting mixer devices!");
    $AudioMixerDevice = "PCM";
} else {
    foreach($output as $device)
    {
        $MixerDevices[$device] = $device;
    }
}
unset($output);

$AudioFormats = Array();
    $AudioFormats['Default'] = 0;
    $AudioFormats['44100/S16'] = 1;
    $AudioFormats['44100/S32'] = 2;
    $AudioFormats['44100/FLT'] = 3;
    $AudioFormats['48000/S16'] = 4;
    $AudioFormats['48000/S32'] = 5;
    $AudioFormats['48000/FLT'] = 6;
    $AudioFormats['96000/S16'] = 7;
    $AudioFormats['96000/S32'] = 8;
    $AudioFormats['96000/FLT'] = 9;


/////////////////////////////////////////////////////////////////////////////
// Video support code
$VideoOutputModels = Array();
if ($settings['Platform'] != "BeagleBone Black") {
    $VideoOutputModels['HDMI'] = "--HDMI--";
}
$VideoOutputModels['Disabled'] = "--Disabled--";
if (file_exists($settings['model-overlays'])) {
    $json = json_decode(file_get_contents($settings['model-overlays']));
    foreach ($json->models as $value) {
        $VideoOutputModels[$value->Name] = $value->Name;
    }
}
/////////////////////////////////////////////////////////////////////////////

?>

<script>
</script>

<table class='settingsTableWrapper'>
    <tr><td valign='top'><b>General Audio/Video</b>
            <table class='settingsTable'>
                <tr><td>Audio Output Device:</td>
                    <td><? PrintSettingSelect("Audio Output Device", "AudioOutput", 2, 0, "$CurrentCard", $AlsaCards, ""); stt('AudioOutput'); ?></td>
                </tr>
                <tr><td>Audio Output Mixer Device:</td>
                    <td><? PrintSettingSelect("Audio Mixer Device", "AudioMixerDevice", 2, 0, $AudioMixerDevice, $MixerDevices, ""); stt('AudioMixerDevice'); ?></td>
                </tr>
<? if ($uiLevel >= 1) { ?>
                <tr><td>Audio Output Format:</td>
                    <td><? PrintSettingSelect("Audio Output Format", "AudioFormat", 2, 0, isset($settings['AudioFormat']) ? $settings['AudioFormat'] : "0", $AudioFormats); stt('AudioFormat'); ?></td>
                </tr>
<? } ?>

                <tr><td>Default Video Output Device:</td>
                    <td><? PrintSettingSelect("Video Output Device", "VideoOutput", 0, 0, isset($settings['videoOutput']) ? $settings['videoOutput'] : array_values($VideoOutputModels)[0], $VideoOutputModels); stt('VideoOutput'); ?></td>
                </tr>

<? if (($uiLevel >= 1) && ($settings['fppMode'] != 'remote')) { ?>
                <tr><td>Media/Sequence Offset:</td>
                    <td><? PrintSettingTextSaved("mediaOffset", 2, 0, 9999, -9999, "", "0", "", "", "number"); ?> ms <? stt('mediaOffset'); ?></td>
                </tr>
<? } ?>

<? if ($settings['Platform'] == "Raspberry Pi") { ?>
                <tr><td>Disable IP announcement:</td>
                    <td><? PrintSettingCheckbox("Disable IP announcement during boot", "disableIPAnnouncement", 0, 0, "1", "0"); stt('disableIPAnnouncement'); ?></td>
                </tr>
<? } ?>
            </table>
            <br>

<? if ($settings['Platform'] == "Raspberry Pi") { ?>
            <b>OMXPlayer (mp4 playback)</b>
            <table class='settingsTable'>
                <tr><td>Audio Output:</td>
                    <td><? PrintSettingSelect("OMXPlayer Audio Device", "OMXPlayerAudioOutput", 0, 0, isset($settings['OMXPlayerAudioOutput']) ? $settings['OMXPlayerAudioOutput'] : "0", Array("ALSA" => "alsa", "HDMI" => "hdmi", "Local" => "local", "Both" => "both", "Disabled" => "disabled")); stt('OMXPlayerAudioOutput'); ?></td>
                </tr>
<?     if (($uiLevel >= 1) && ($settings['fppMode'] == 'remote')) { ?>
                <tr><td>Ignore Media Sync Packets:</td>
                    <td><? PrintSettingCheckbox("Ignore media sync packets", "remoteIgnoreSync", 2, 0, "1", "0"); stt('remoteIgnoreSync'); ?></td>
                </tr>
<?     } ?>
<? } ?>

            </table>
            </td>
        <td width='70px'></td>
        <td valign='top'><b>General Playback</b>
            <table class='settingsTable'>
                <tr><td>Blank between sequences:</td>
                    <td><? PrintSettingCheckbox("Blank Between Sequences", "blankBetweenSequences", 2, 0, "1", "0"); stt('blankBetweenSequences'); ?></td>
                </tr>
                <tr><td>Pause Background Effect Sequence when playing a FSEQ file:</td>
                    <td><? PrintSettingCheckbox("Pause Background Effects", "pauseBackgroundEffects", 2, 0, "1", "0"); stt('pauseBackgroundEffects'); ?></td>
                </tr>

<? if (($uiLevel >= 1) && ($settings['fppMode'] == 'master')) { ?>
                <tr><td>Open/Start Delay:</td>
                    <td><? PrintSettingTextSaved("openStartDelay", 2, 0, 9999, 0, "", "0", "", "", "number"); stt('openStartDelay'); ?> ms</td>
                </tr>
<? } ?>

<? if (($uiLevel >= 1) && ($settings['fppMode'] == 'remote')) { ?>
                <tr><td>Remote Media/Sequence Offset:</td>
                    <td><? PrintSettingTextSaved("remoteOffset", 2, 0, 9999, -9999, "", "0", "", "", "number"); stt('remoteOffset'); ?> ms</td>
                </tr>
<? } ?>
            </table>
            </td>
    </tr>
</table>
