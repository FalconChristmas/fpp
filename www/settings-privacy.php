<?
$skipJSsettings = 1;
require_once('common.php');

$extraData = "<input type='button' class='buttons' value='Preview Statistics' onClick='PreviewStatistics();'> ";
PrintSettingGroup('privacy',$extraData);
?>
<div>
	Clicking the Preview Statistics button will show you the information that is getting reported</br>
	to the developers so you can see what is being shared if you enable the Share Statistics option</br>
	It is recommended to enable this option to help the developers improve FPP and its performance.
</div>
