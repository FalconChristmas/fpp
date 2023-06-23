<?
require_once '../config.php';
require_once '../common/mqtthelp.php';
?>
<script>
    // Fixes a problem because config.php changes helpPage
    helpPage = "help/settings.php";
</script>
<center><b>MQTT Settings</b></center>
<hr>
<ul>
<li>MQTT events will be published to "<?echo GetSettingValue('MQTTPrefix', '', '', '/'); ?>falcon/player/<?echo GetSettingValue('HostName', 'FPP'); ?>/" with playlist events being in the "playlist" subtopic.
<li><b>CA file</b> is the full path to the signer certificate.  Only needed if using mqtts server that is self signed.
<li><b>Publish Frequence</b> should be zero (disabled) or the number of seconds between periodic mqtt publish events<br/>
<li>FPP will respond to certain events:
</ul>
<div class='fppTableWrapper selectable'>
<div class='fppTableContents' role="region" aria-labelledby="MQTTSettingsTable" tabindex="0">
<?showMqttHelpTable()?>
</div>
</div>
