<?php
//stop settings javascript output in results
$skipJSsettings =1;

require_once 'common.php';


//LoadCommands
$troubleshootingCommandsLoaded = 0;
LoadTroubleShootingCommands();
$target_platforms = array('all', $settings['Platform']);


echo "Troubleshooting Commands:\n";

////Display Command Contents
foreach ($troubleshootingCommandGroups as $commandGrpID => $commandGrp) {
    //Loop through groupings
    //Display group if relevant for current platform
    if (count(array_intersect($troubleshootingCommandGroups[$commandGrpID]["platforms"], $target_platforms)) > 0) {
        echo "===================================================================\n";
        echo "Command Group: $commandGrpID\n";

    }
    //Loop through commands in grp
    foreach ($commandGrp["commands"] as $commandKey => $commandID) {
        //Display command if relevant for current platform
        if (count(array_intersect($commandID["platforms"], $target_platforms)) > 0) {
            $commandTitle = $commandID["title"];
            $commandCmd = $commandID["cmd"];
            $commandDesc = $commandID["description"];

            $url = "http://localhost/troubleshootingHelper.php?key=" . urlencode($commandKey);
           $results = file_get_contents($url);
    
            

    echo "Title  : $commandTitle\n";
    echo "Command: $commandCmd\n";
    echo "Command Description: $commandDesc\n";
    echo "-----------------------------------\n";
    echo $results;
    echo "\n";
    $results="";

        }}


}

echo "===================================================================\n";
