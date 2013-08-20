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
<?php include 'common/footer.inc'; ?>
</body>
</html>
