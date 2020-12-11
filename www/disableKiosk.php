<?
header( "Access-Control-Allow-Origin: *");

$wrapped = 1;
$skipJSsettings = 1;
require_once("common.php");
DisableOutputBuffering();

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

