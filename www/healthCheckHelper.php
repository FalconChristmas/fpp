<?
header("Access-Control-Allow-Origin: *");

$skipJSsettings = 1;

require_once "common.php";
DisableOutputBuffering();

$cmd = "/opt/fpp/scripts/healthCheck --php";

$descriptorspec = array(
    0 => array("pipe", "r"), // stdin is a pipe that the child will read from
    1 => array("pipe", "w"), // stdout is a pipe that the child will write to
    2 => array("pipe", "w"), // stderr is a pipe that the child will write to
);

ob_start();
$process = proc_open($cmd, $descriptorspec, $pipes, realpath('./'), array());

if (is_resource($process)) {
    while ($s = fgets($pipes[1])) {
        //if the user browser is Firefox, Internet Explorer or Safari
        // Google Chrome the str_repeat is required to display buffer correctly
        echo str_repeat(" ", 1024);
        print substr($s, 0, 10240);
        flush();

    }
}

ob_end_flush();
