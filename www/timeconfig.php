<?php

$data = exec("ls -w 1 /etc/rc$(sudo runlevel | awk '{print $2}').d/ | grep ^S | grep -c ntp", $output, $return_val);
$ntp = ($output[0] == 1);
unset($output);
//TODO: check return

$current_tz = exec("cat /etc/timezone", $output, $return_val);
unset($output);

if ( isset($_POST['ntp']) && !empty($_POST['ntp']) && $_POST['ntp'] == "ntp_disabled" && $ntp )
{
  error_log("Disabling NTP because it's enabled and we were told to disable it.");
  exec("sudo service ntp stop", $output, $return_val);
  unset($output);
  //TODO: check return
  exec("sudo update-rc.d ntp remove", $output, $return_val);
  unset($output);
  //TODO: check return
}
elseif ( isset($_POST['ntp']) && !empty($_POST['ntp']) && $_POST['ntp'] == "ntp_enabled" && !$ntp )
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

$data = exec("ls -w 1 /etc/rc$(sudo runlevel | awk '{print $2}').d/ | grep ^S | grep -c ntp", $output, $return_val);
$ntp = ($output[0] == 1);
unset($output);
//TODO: check return

$current_tz = exec("cat /etc/timezone", $output, $return_val);
unset($output);

$zones = exec("find /usr/share/zoneinfo ! -type d | sed 's/\/usr\/share\/zoneinfo\///' | grep -v ^right | grep -v ^posix | grep -v ^\\/ | grep -v \\\\. | sort", $output, $return_val);
if ( $return_val != 0 )
{
  die("Couldn't get time zone listings");
}

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
<?php // TODO: RTC Configuration ?>
<h4>NTP</h4>

<!--

TODO: Make this a tool-tip:

<p>NTP stands for Network Time Protocol, and it is an Internet protocol used to synchronize the clocks of computers to some time reference. NTP is an Internet standard protocol originally developed by Professor David L. Mills at the University of Delaware.</p>

<p>For more information, please see: <a href="http://www.ntp.org/ntpfaq/NTP-s-def.htm" target="_blank">http://www.ntp.org/ntpfaq/NTP-s-def.htm</a></p>

<p>You will want to disable this if you're not planning on connecting your PI to the internet</p>
-->

			<input type="radio" name="ntp" value="ntp_enabled" <?php echo print_if_match($ntp, true, "checked=\"checked\""); ?>>
			<label for="ntp_enabled">Enabled</label>
			<input type="radio" name="ntp" value="ntp_disabled" <?php echo print_if_match($ntp, false, "checked=\"checked\""); ?>>
			<label for="ntp_disabled">Disabled</label>

<h4>Time Zone</h4>

<select name="timezone">

Time Zone (dpkg-reconfigure trick from here: http://serverfault.com/questions/84521/automate-dpkg-reconfigure-tzdata/
<?php

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
