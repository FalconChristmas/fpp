<?

$wrapped = 1;
$skipJSsettings = 1;
require_once("common.php");
DisableOutputBuffering();
// Finish the job if the browser goes away: a closed tab or dropped connection
// used to abort PHP part way through, leaving the command it ran half done.
ignore_user_abort(true);

echo "==========================================================================\n";

$command = "sudo sed -i -e '/ startx /d' /home/fpp/.bashrc 2>&1";
echo "Command: $command\n";
system($command);

$command = "sudo rm -f /etc/systemd/system/getty@tty1.service.d/autologin.conf 2>&1";
echo "Command: $command\n";
system($command);

$command = "sudo rm -f /etc/fpp/kiosk 2>&1";
echo "Command: $command\n";
system($command);

echo "==========================================================================\n";

?>

