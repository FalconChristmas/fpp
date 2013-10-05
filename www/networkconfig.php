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

<?php
// God save me... I'm using a goto...
goto DEVELOPMENT;
?>

<div id="network" class="settings">
<fieldset>
<legend>Network Settings</legend>

<?php

$interfaces = explode("\n",trim(shell_exec("/sbin/ifconfig | cut -f1 -d' ' | grep -v ^$ | grep -v lo")));

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

  $interface["name"] = $iface;
  $interface["mode"] = "dhcp";//TODO

  if (trim($iface,"1234567890") == "eth")
  {
    $interface["pretty_name"] = "Wired Ethernet";
    $interface["wireless"] = false;
  }
  elseif (trim($iface,"1234567890") == "wlan")
  {
    $interface["pretty_name"] = "Wireless Ethernet";
    $interface["wireless"] = true;
    $interface["ssid"] = "";
    $interface["psk"] = "";
    $thisdir = dirname(__FILE__);
    preg_match_all('/"(?:\\\\.|[^\\\\"])*"|\S+/', shell_exec("$thisdir/readInterface.awk /etc/network/interfaces device=".$interface["name"]), $matches);
    if ( count($matches[0]) > 1 )
    {
      $interface["ssid"] = trim($matches[0][1],'"');
    }
    if ( count($matches[0]) > 2 )
    {
      $interface["psk"] = trim($matches[0][2],'"');
    }
  }
  preg_match('/addr:([\d\.]+)/', $ifconfig, $interface["ipaddr"]);
  preg_match('/Mask:([\d\.]+)/', $ifconfig, $interface["netmask"]);
  preg_match('/Bcast:([\d\.]+)/', $ifconfig, $interface["broadcast"]);
  preg_match('/via ([\d\.]+)/', shell_exec('/sbin/ip route'), $interface["gateway"]);
  preg_match('/nameserver ([\d\.]+)/', shell_exec('/bin/cat /etc/resolv.conf | grep nameserver'), $interface["dns"]);

?>
      <h4><?php echo $interface['pretty_name'] . " (" . $interface['name'] . ")"; ?></h4>

      <?php if ($interface["wireless"]) { ?>
			<div class="<?php echo $interface['name']; ?>_wifi_settings" id="<?php echo $interface['name']; ?>_wifi_settings">
				<table width= "100%" border="0" cellpadding="2" cellspacing="2">
				<tr>
					<td><label for="<?php echo $interface['name']; ?>_ssid">SSID:</label></td>
					<td><input type="text" name="<?php echo $interface['name']; ?>_ssid" id="<?php echo $interface['name']; ?>_ssid"
                                        value="<?php echo $interface["ssid"]; ?>"></td>
				</tr><tr>
					<td><label for="<?php echo $interface['name']; ?>_psk">Key:</label></td>
					<td><input type="password" name="<?php echo $interface['name']; ?>_psk" id="<?php echo $interface['name']; ?>_psk"
                                        value="<?php echo $interface["psk"]; ?>"></td>
				</tr>
        </table>
      </div>
      <?php } ?>

      <div id="<?php echo "${interface['name']}_settings"; ?>">
			<label for="<?php echo $interface['name']; ?>_dhcp">DHCP</label>
			<input type="radio" name="<?php echo $interface['name']; ?>_mode" id="<?php echo $interface['name']; ?>_dhcp" value="dhcp" <?php checked_if_matched($interface, "dhcp"); ?>>
			<label for="<?php echo $interface['name']; ?>_static">Static</label>
			<input type="radio" name="<?php echo $interface['name']; ?>_mode" id="<?php echo $interface['name']; ?>_static" value="static" <?php checked_if_matched($interface, "static"); ?>>

			<div class="<?php echo $interface['name']; ?>_net_settings" id="<?php echo $interface['name']; ?>_dhcp_settings" <?php hide_if_not_matched($interface, "dhcp"); ?>>
			</div>
			<div class="<?php echo $interface['name']; ?>_net_settings" id="<?php echo $interface['name']; ?>_static_settings" <?php hide_if_not_matched($interface, "static"); ?>>
				<table width= "100%" border="0" cellpadding="2" cellspacing="2">
				<tr>
					<td><label for="<?php echo $interface['name']; ?>_ip_addr">IP Address:</label></td>
					<td><input type="text" name="<?php echo $interface['name']; ?>_ip_addr" id="<?php echo $interface['name']; ?>_ip_addr"
                                        value="<?php if (count($interface["ipaddr"]) > 1) echo $interface["ipaddr"][1]; ?>"></td>
				</tr><tr>
					<td><label for="<?php echo $interface['name']; ?>_netmask">Netmask:</label></td>
					<td><input type="text" name="<?php echo $interface['name']; ?>_netmask" id="<?php echo $interface['name']; ?>_netmask"
                                        value="<?php if (count($interface["netmask"]) > 1) echo $interface["netmask"][1]; ?>"></td>
				</tr><tr>
					<td><label for="<?php echo $interface['name']; ?>_bcast">Broadcast:</label></td>
					<td><input type="text" name="<?php echo $interface['name']; ?>_bcast" id="<?php echo $interface['name']; ?>_bcast"
                                        value="<?php if (count($interface["broadcast"]) > 1) echo $interface["broadcast"][1]; ?>"></td>
				</tr><tr>
					<td><label for="<?php echo $interface['name']; ?>_gw">Gateway:</label></td>
					<td><input type="text" name="<?php echo $interface['name']; ?>_gw" id="<?php echo $interface['name']; ?>_gw"
                                        value="<?php if (count($interface["gateway"]) > 1) echo $interface["gateway"][1]; ?>"></td>
				</tr><tr>
					<td><label for="<?php echo $interface['name']; ?>_dns">DNS IP:</label></td>
					<td><input type="text" name="<?php echo $interface['name']; ?>_dns" id="<?php echo $interface['name']; ?>_dns"
                                        value="<?php if (count($interface["dns"]) > 1) echo $interface["dns"][1]; ?>"></td>
				</tr>
				</table>
			</div>
      </div>
<script>
$(document).ready(function(){
	$("input[name$='<?php echo $interface['name']; ?>_mode']").change(function() {
		var test = $(this).val();
		$(".<?php echo $interface['name']; ?>_net_settings").hide();
		$("#<?php echo $interface['name']."_"; ?>"+test+"_settings").show();
	});
});
</script>
<?php
}
?>
            <input id="submit" name="submit" type="submit" class="buttons" value="Submit">
<?php
DEVELOPMENT:
?>
<div class="settings">
<fieldset>
<legend>Network Settings - Under Construction</legend>
Stay tuned!
    </fieldset>
  </div>
</div>
<?php include 'common/footer.inc'; ?>
</body>
</html>
