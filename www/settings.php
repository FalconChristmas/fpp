<?php
if ( isset($_POST['password1']) && isset($_POST['password2']))
{
if (($_POST['password1'] != "") && ($_POST['password1'] == $_POST['password2']))
  {
    // true - setup .htaccess & save it
	file_put_contents("/home/pi/fpp/www/.htaccess", "AuthUserFile /home/pi/fpp/www/.htpasswd\nAuthType Basic\nAuthName FPP-GUI\nRequire valid-user\n");
    $setpassword = shell_exec('htpasswd -cbd /home/pi/fpp/www/.htpasswd admin ' . $_POST['password1']);
  }
}

if ( isset($_POST['submit']) )
{
	echo "<html><body>We don't do anything yet, sorry!</body></html>";
	exit(0);
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
<FORM NAME="password_form" ACTION="<?php echo $_SERVER['PHP_SELF'] ?>" METHOD="POST">
  <div id = "uipassword" class="settings">
    <fieldset>
      <legend>FPP Password</legend>
        <table width= "100%" border="0" cellpadding="2" cellspacing="2">
          <tr>
            <td colspan="3" align="center">Enter a password if you wish to enable password protection of FPP web GUI</td>
          </tr>
          <tr>
            <td colspan="3" align="center"><input name="login" type="hidden" value="admin"></td>
          </tr>
          <tr>
            <td width="18%" align="right">Password:</td>
            <td colspan="2"><INPUT name="password1" type="password" onKeyUp="verify.check()" size="40" maxlength="40"></td>
          </tr>
          <tr>
            <td align="right">Confirm Password:</td>
            <td width="43%"><INPUT NAME="password2" TYPE="password" onKeyUp="verify.check()" size="40" maxlength="40"></td>
            <td width="39%"><DIV ID="password_result">&nbsp;</DIV></td>
          </tr>
          <tr>
            <td colspan="3" align="center">&nbsp;</td>
          </tr>
        </table>
    </fieldset>
  </div>

<br />

<div id="time" class="settings">
<fieldset>
<legend>Time Settings</legend>
RTC
<br />
Manual (disable NTP)
<br />
Time Zone (dpkg-reconfigure trick from here: http://serverfault.com/questions/84521/automate-dpkg-reconfigure-tzdata/
listing of zones: /usr/share/zoneinfo
</fieldset>
</div>

<br />

<div id="usb" class="settings">
<fieldset>
<legend>USB Devices</legend>
<?php
$devices=explode("\n",trim(shell_exec("ls -w 1 /dev/ttyUSB*")));
foreach ($devices as $device)
{
	echo "WE FOUND DEVICE $device... what is it used for (renard, dmx, rds?\n";
	echo "<br />\n";
}
?>
</fieldset>
</div>

<br />

<div id="global" class="settings">
<fieldset>
<legend>FPP Global Settings</legend>
pixelnet?
e1.31?
</fieldset>
</div>

<br />

<div id="network" class="settings">
<fieldset>
<legend>Network Settings</legend>

<?php

$interfaces = explode("\n",trim(shell_exec("/sbin/ifconfig | cut -f1 -d' ' | grep -v ^$ | grep -v lo")));
$ifacename["eth0"] = "Wired Ethernet";
$ifacename["wlan0"] = "Wireless Ethernet";

function hide_if_not_matched($interface, $mode)
{
	if ( $interface["mode"] != $mode )
		echo "style=\"display: none\"";
}
function checked_if_matched($interface, $mode)
{
	if ( $interface["mode"] == $mode )
		echo "checked=\"checked\"";
}

foreach ($interfaces as $iface)
{
$ifconfig = shell_exec("/sbin/ifconfig $iface");
//preg_match('/addr:([\d\.]+)/', $ifconfig, $ethipaddress);
//preg_match('/Mask:([\d\.]+)/', $ifconfig, $ethnetmask);
//preg_match('/Bcast:([\d\.]+)/', $ifconfig, $ethbroadcast);
//$iproute = shell_exec('/sbin/ip route');
//preg_match('/via ([\d\.]+)/', $iproute, $ethgateway);
//$ipdns = shell_exec('/bin/cat /etc/resolv.conf | grep nameserver');
//preg_match('/nameserver ([\d\.]+)/', $ipdns, $ethnameserver);

//$ifconfig = shell_exec('/sbin/ifconfig wlan0');
//preg_match('/addr:([\d\.]+)/', $ifconfig, $wlanipaddress);
//preg_match('/Mask:([\d\.]+)/', $ifconfig, $wlannetmask);
//preg_match('/Bcast:([\d\.]+)/', $ifconfig, $wlanbroadcast);
//$iproute = shell_exec('/sbin/ip route');
//preg_match('/via ([\d\.]+)/', $iproute, $wlangateway);

//$interface = file_get_contents('/etc/network/interfaces');


$interface["name"] = $iface;
$interface["mode"] = "dhcp";
$interface["pretty_name"] = $ifacename[$iface];
//$interface["ipaddr"] = ;
//$interface["gateway"] =;
//$interface["netmask"] =;
//$interface["broadcast"] =;
//$interface["dns"] =;

?>
              <h4><?php echo $interface['pretty_name'] . " (" . $interface['name'] . ")"; ?></h4>
              <div id="<?php echo "${interface['name']}_settings"; ?>">
			<input type="radio" name="<?php echo $interface['name']; ?>_mode" value="<?php echo $interface['name']; ?>_dhcp" <?php checked_if_matched($interface, "dhcp"); ?>>
			<label for="<?php echo $interface['name']; ?>_mode">DHCP</label>
			<input type="radio" name="<?php echo $interface['name']; ?>_mode" value="<?php echo $interface['name']; ?>_static" <?php checked_if_matched($interface, "static"); ?>>
			<label for="<?php echo $interface['name']; ?>_mode">Static</label>

			<div class="<?php echo $interface['name']; ?>_net_settings" id="<?php echo $interface['name']; ?>_dhcp_settings" <?php hide_if_not_matched($interface, "dhcp"); ?>>
			</div>
			<div class="<?php echo $interface['name']; ?>_net_settings" id="<?php echo $interface['name']; ?>_static_settings" <?php hide_if_not_matched($interface, "static"); ?>>
				<table width= "100%" border="0" cellpadding="2" cellspacing="2">
				<tr>
					<td><label for="IpAddress">IP Address:</label></td>
					<td><input type="text" name="EthIpAddress" id="EthIpAddress" value=""></td>
				</tr><tr>
					<td><label for="EthNetmask">Netmask:</label></td>
					<td><input type="text" name="EthNetmask" id="EthNetmask" value=""></td>
				</tr><tr>
					<td><label for="EthBroadcast">Broadcast:</label></td>
					<td><input type="text" name="EthBroadcast" id="EthBroadcast" value=""></td>
				</tr><tr>
					<td><label for="EthGateway">Gateway:</label></td>
					<td><input type="text" name="EthGateway" id="EthGateway" value=""></td>
				</tr><tr>
					<td><label for="EthDNS">DNS IP:</label></td>
					<td><input type="text" name="EthDNS" id="EthDNS" value=""></td>
				</tr>
				</table>
			</div>
              </div>
<script>
$(document).ready(function(){
	$("input[name$='<?php echo $interface['name']; ?>_mode']").change(function() {
		var test = $(this).val();
		$(".<?php echo $interface['name']; ?>_net_settings").hide();
		$("#"+test+"_settings").show();
	});
});
</script>
<?php
}
?>


    </fieldset>
            <input id="submit" name="submit" type="submit" class="buttons" value="Submit">
  </div>

</div>
     </FORM>
</div>
<?php	include 'common/footer.inc'; ?>
</body>
</html>
