<?
$pluginSettings = array();

$pluginConfigFile = $settings['configDirectory'] . "/plugin." . $_GET['plugin'];
if (file_exists($pluginConfigFile))
    $pluginSettings = parse_ini_file($pluginConfigFile);

$pluginSettings['plugin'] = $_GET['plugin'];

if (!isset($skipJSsettings))
{
?>
<script type="text/javascript">
var pluginSettings = new Array();
<?
foreach ($pluginSettings as $key => $value) {
    printf("    pluginSettings['%s'] = \"%s\";\n", $key, $value);
}
?>
</script>
<?
}
?>
