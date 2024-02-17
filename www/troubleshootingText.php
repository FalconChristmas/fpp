<?php

$skipJSsettings = 1;

require_once 'troubleshootingCommands.php';

$results = array();

foreach ($commands as $title => $command) {
    $results[$command] = "Unknown";
    exec($command . ' 2>&1 | fold -w 160 -s', $output, $return_val);
    if ($return_val == 0) {
        $results[$command] = implode("\n", $output) . "\n";
    }

    unset($output);
}

echo "Troubleshooting Commands:\n";

foreach ($commands as $title => $command) {
    echo "===================================================================\n";
    echo "Title  : $title\n";
    echo "Command: $command\n";
    echo "-----------------------------------\n";
    echo $results[$command];
    echo "\n";
}

echo "===================================================================\n";
