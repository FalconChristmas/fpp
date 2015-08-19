<?php
require_once('config.php');
require_once('common.php');

putenv("PATH=/bin:/usr/bin:/sbin:/usr/sbin");

$commands = array(
	// Networking
	'Interfaces'         => 'ifconfig -a',
	'Wired'              => 'ethtool eth0',
	'Wireless'           => 'iwconfig',
	'Routing'            => 'netstat -rn',
	'Default Gateway'    => 'ping -c 1 $(netstat -rn | grep \'^0.0.0.0\' | awk \'{print $2}\')',
	'Internet Access'    => 'ping -c 1 github.com',

	// Disk
	'Partitions'         => $SUDO . ' fdisk -l',
	'Filesystems'        => 'df -k',
	'Mounts'             => 'mount | grep -v password',

	// Date/Time
	'Date'               => 'date',
	'NTP Peers'          => 'pgrep ntpd > /dev/null && ntpq -c peers',
	'RTC'                => $SUDO . ' hwclock -r',

	// Memory & Processes
	'Memory'             => 'free',
	'CPU Utilization'    => 'top -bn1 | head -20',
	'Processes'          => 'ps -edaf --forest',  // Keep this last since it is so long
	);

$results = array();

foreach ($commands as $title => $command)
{
	$results[$command] = "Unknown";
	exec($command . ' 2>&1 | fold -w 80 -s', $output, $return_val);
	if ( $return_val == 0 )
		$results[$command] = implode("\n", $output) . "\n";
	unset($output);
}

?>

<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<?php include 'common/menuHead.inc'; ?>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
<title><? echo $pageTitle; ?></title>
</head>

<body>
<div id="bodyWrapper">
  <?php include 'menu.inc'; ?>
  <div style="margin:0 auto;"> <br />
    <fieldset style="padding: 10px; border: 2px solid #000;">
      <legend>Troubleshooting Commands</legend>
      <div style="overflow: hidden; padding: 10px;">
    <div class="clear"></div>
<?
foreach ($commands as $title => $command)
{
?>
				<h3><? echo $title . ':&nbsp;&nbsp;&nbsp;&nbsp;' . $command; ?></h3><pre><? echo $results[$command]; ?></pre><hr>
<?
}
?>
    </fieldset>
  </div>
</div>
  <?php include 'common/footer.inc'; ?>
</body>
</html>
