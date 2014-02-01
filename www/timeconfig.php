<?php
require_once('config.php');
require_once('common.php');

$current_tz = exec("cat /etc/timezone", $output, $return_val);
unset($output);

if ( isset($_POST['piRTC']) && !empty($_POST['piRTC']) )
{
  $piRTC = $_POST['piRTC'];
  WriteSettingToFile("piRTC",$piRTC);
  exec(SUDO . " $fppDir/scripts/piRTC set");
}

if ( isset($_POST['date']) && !empty($_POST['date']) )
{
//TODO: validate date format
error_log("Setting date to ".$_POST['date'].".");
//set the date
exec("sudo date +%Y/%m/%d -s \"".$_POST['date']."\"", $output, $return_val);
if (!(isset($_POST['time']) && !empty($_POST['time'])))
{
  exec("sudo hwclock -w", $output, $return_val);
}

unset($output);
//TODO: check return
}


if ( isset($_POST['time']) && !empty($_POST['time']) )
{
//TODO: validate time format
error_log("Setting time to ".$_POST['time'].".");
//set the time
exec("sudo date +%k:%M -s \"".$_POST['time']."\"", $output, $return_val);
exec("sudo hwclock -w", $output, $return_val);
unset($output);
//TODO: check return
}

exec("ls -w 1 /etc/rc$(sudo runlevel | awk '{print $2}').d/ | grep ^S | grep -c ntp | sed 's/1/true/;s/0/false/'", $output, $return_val);
$ntp = ( $output[0] == "true" );
unset($output);
//TODO: check return

if ( isset($_POST['ntp']) && !empty($_POST['ntp']) && $_POST['ntp'] == "disabled" && $ntp )
{
  error_log("Disabling NTP because it's enabled and we were told to disable it.");
  exec("sudo service ntp stop", $output, $return_val);
  unset($output);
  //TODO: check return
  exec("sudo update-rc.d ntp remove", $output, $return_val);
  unset($output);
  //TODO: check return
}
elseif ( isset($_POST['ntp']) && !empty($_POST['ntp']) && $_POST['ntp'] == "enabled" && !$ntp )
{
  error_log("Enabling NTP because it's disabled and we were told to enable it.");
  exec("sudo update-rc.d ntp defaults", $output, $return_val);
  unset($output);
  //TODO: check return
  exec("sudo service ntp start", $output, $return_val);
  unset($output);
  //TODO: check return
}

if ( isset($_POST['timezone']) && !empty($_POST['timezone']) && urldecode($_POST['timezone']) != $current_tz )
{
  //TODO: Check timezone for validity
  $timezone = urldecode($_POST['timezone']);
  error_log("Changing timezone to '".$timezone."'.");
  exec("sudo bash -c \"echo $timezone > /etc/timezone\"", $output, $return_val);
  unset($output);
  //TODO: check return
  exec("sudo dpkg-reconfigure -f noninteractive tzdata", $output, $return_val);
  unset($output);
  //TODO: check return
}

exec("ls -w 1 /etc/rc$(sudo runlevel | awk '{print $2}').d/ | grep ^S | grep -c ntp", $output, $return_val);
$ntp = ($output[0] == 1);
unset($output);
//TODO: check return

$current_tz = exec("cat /etc/timezone", $output, $return_val);
unset($output);

function print_if_match($one, $two, $print)
{
  if ( $one == $two )
    return $print;
}

?>
<!DOCTYPE html>
<html>
<head>
<?php include 'common/menuHead.inc'; ?>
<script type="text/javascript" src="/js/fpp.js"></script>
<title>Falcon PI Player - FPP</title>
</head>
<body>
<div id="bodyWrapper">
  <?php include 'menu.inc'; ?>
  <br/>


<div id="time" class="settings">
<fieldset>
<legend>Time Settings</legend>
      <form name="time_form" action="<?php echo $_SERVER['PHP_SELF'] ?>" method="POST">

<h4>Manually Set Date/Time</h4>

<p><label for="manual_date">Date:</label>
<input type="text" name="date" id="manual_date"></input>
(Expected format: YYYY/MM/DD)</p>

<p><label for="manual_time">Time:</label>
<input type="text" name="time" id="manual_time"></input>
(Expected format: HH:MM in 24 hour time)</p>


<?php // TODO: RTC Configuration ?>

<h4>Real Time Clock</h4>
<select name="piRTC">
  <option value = "0" <?php echo print_if_match("0",ReadSettingFromFile("piRTC"),"selected") ?> >None</option>
  <option value = "1" <?php echo print_if_match("1",ReadSettingFromFile("piRTC"),"selected") ?> >RasClock</option>
  <option value = "2" <?php echo print_if_match("2",ReadSettingFromFile("piRTC"),"selected") ?> >DS1305</option>
</select>


<h4>NTP</h4>

<!--

TODO: Make this a tool-tip:

<p>NTP stands for Network Time Protocol, and it is an Internet protocol used to synchronize the clocks of computers to some time reference. NTP is an Internet standard protocol originally developed by Professor David L. Mills at the University of Delaware.</p>

<p>For more information, please see: <a href="http://www.ntp.org/ntpfaq/NTP-s-def.htm" target="_blank">http://www.ntp.org/ntpfaq/NTP-s-def.htm</a></p>

<p>You will want to disable this if you're not planning on connecting your PI to the internet</p>
-->

			<label for="ntp_enabled">Enabled:</label>
			<input type="radio" name="ntp" id="ntp_enabled" value="enabled" <?php echo print_if_match($ntp, true, "checked=\"checked\""); ?>>
			<label for="ntp_disabled">Disabled:</label>
			<input type="radio" name="ntp" id="ntp_disabled" value="disabled" <?php echo print_if_match($ntp, false, "checked=\"checked\""); ?>>

<h4>Time Zone</h4>

<select name="timezone">

Time Zone (dpkg-reconfigure trick from here: http://serverfault.com/questions/84521/automate-dpkg-reconfigure-tzdata/
<?php

$zones = exec("find /usr/share/zoneinfo ! -type d | sed 's/\/usr\/share\/zoneinfo\///' | grep -v ^right | grep -v ^posix | grep -v ^\\/ | grep -v \\\\. | sort", $output, $return_val);
if ( $return_val != 0 )
{
  die("Couldn't get time zone listings");
}

foreach ($output as $zone)
{
  echo "<option value=\"".urlencode($zone)."\" ".print_if_match($zone, $current_tz, "selected").">$zone</option>\n";
}
unset($output);

?>
</select>

<div id="submit">
<input id="submit_button" name="submit_button" type="submit" class="buttons" value="Submit">
</div>
      </form>

</fieldset>
</div>


</div>
<?php	include 'common/footer.inc'; ?>
</body>
</html>
