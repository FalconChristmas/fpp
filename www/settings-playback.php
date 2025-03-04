<?
$skipJSsettings = 1;
require_once('common.php');
?>

<?
PrintSettingGroup('generalPlayback');
if ($uiLevel >= 1) {
    PrintSettingGroup('generalScheduler');
}
?>
