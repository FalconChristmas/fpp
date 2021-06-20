<?

// This file runs each command when called via the specified key
// and returns the raw results
   
$skipJSsettings = 1;
require_once('troubleshootingCommands.php');

$found = 0;
if (isset($_GET['key'])) {
    $key = $_GET['key'];
    if (isset($commands[$key])) {
        $found = 1;
        $command = $commands[$key];
        exec($command . ' 2>&1 | fold -w 80 -s', $output, $return_val);
        if ( $return_val == 0 )
            echo(implode("\n", $output) . "\n");
        unset($output);    
    }
}

if (! $found) {
    echo("Unknwon Command");
}

?>