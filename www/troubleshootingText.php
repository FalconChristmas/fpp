<?php

$skipJSsettings = 1;

require_once('troubleshootingCommands.php');

echo "Troubleshooting Commands:\n";

foreach ($commands as $title => $command)
{
	echo "===================================================================\n";
	echo "Title  : $title\n";
	echo "Command: $command\n";
	echo "-----------------------------------\n";
	echo $results[$command];
	echo "\n";
}

echo "===================================================================\n";

?>
