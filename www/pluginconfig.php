<?
$pluginSettings = array();

$pluginName = htmlspecialchars($_GET['plugin'], ENT_QUOTES, 'UTF-8');

$pluginConfigFile = $settings['configDirectory'] . "/plugin." . $pluginName;
echo ($pluginConfigFile);
if (file_exists($pluginConfigFile))
    $pluginSettings = parse_ini_file($pluginConfigFile);

$pluginSettings['plugin'] = $pluginName;

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
