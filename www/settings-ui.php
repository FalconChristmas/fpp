<?
$uiLevels = Array();
    $uiLevels['Basic'] = 0;
    $uiLevels['Advanced'] = 1;
    $uiLevels['Experimental'] = 2;
    $uiLevels['Developer'] = 3;

$backgroundColors = Array();
    $backgroundColors['No Border'] = '';
    $backgroundColors['Red']       = "ff0000";
    $backgroundColors['Green']     = "008000";
    $backgroundColors['Blue']      = "0000ff";
    $backgroundColors['Aqua']      = "00ffff";
    $backgroundColors['Black']     = "000000";
    $backgroundColors['Gray']      = "808080";
    $backgroundColors['Lime']      = "00FF00";
    $backgroundColors['Navy']      = "000080";
    $backgroundColors['Olive']     = "808000";
    $backgroundColors['Purple']    = "800080";
    $backgroundColors['Silver']    = "C0C0C0";
    $backgroundColors['Teal']      = "008080";
    $backgroundColors['White']     = "FFFFFF";

?>

<table class='settingsTable'>
    <tr><td>UI Border Color:</td>
        <td><? PrintSettingSelect("UI Background Color", "backgroundColor", 0, 0, isset($settings['backgroundColor']) ? $settings['backgroundColor'] : "", $backgroundColors, "", "reloadSettingsPage"); stt('backgroundColor'); ?></td>
    </tr>
    <tr><td>User Interface Level:</td>
        <td><? PrintSettingSelect('UI Level', 'uiLevel', 0, 0, '0', $uiLevels, '', 'reloadSettingsPage', ''); stt('uiLevel'); ?></td>
    </tr>
<? if ($uiLevel >= 1) { ?>
    <tr><td>Display all options/settings</td>
        <td><? PrintSettingCheckbox("Show All Options", "showAllOptions", 0, 0, "1", "0"); stt('showAllOptions'); ?></td>
    </tr>
<? } ?>
<? if ($uiLevel >= 2) { ?>
    <tr><td>Disable restart/reboot UI Warnings:</td>
        <td><? PrintSettingCheckbox("Disable restart/reboot UI Warnings", "disableUIWarnings", 0, 0, "1", "0"); stt('disableUIWarnings'); ?></td>
    </tr>
<? } ?>

</table>
