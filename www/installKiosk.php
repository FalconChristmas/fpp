<?

$wrapped = 1;
$skipJSsettings = 1;
require_once("common.php");
DisableOutputBuffering();
// Finish the job if the browser goes away: a closed tab or dropped connection
// used to abort PHP part way through, leaving the command it ran half done.
ignore_user_abort(true);

echo "==========================================================================\n";

$command = "sudo /opt/fpp/SD/FPP_Kiosk.sh 2>&1";

echo "Command: $command\n";
echo "--------------------------------------------------------------------------\n";
system($command);
echo "\n";

echo "==========================================================================\n";

?>
