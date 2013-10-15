<!DOCTYPE html>
<html>
<head>
<?php include 'common/menuHead.inc'; ?>
<script type="text/javascript" src="/js/fpp.js"></script>
<title>Falcon PI Player - FPP</title>
</head>
<body>
<?php
$readinterface = shell_exec('/home/pi/fpp/www/readInterface.awk /etc/network/interfaces device=eth0');
$parseethernet = explode(" ", $readinterface);
if (trim($parseethernet[0], "\"\n\r") == "dhcp" )
 {
	 $eth_mode = "dhcp";
 }
 else
 {
 	 $eth_mode = "static";
 }
unset ($readinterface);
unset ($parseethernet);

$ifconfig = shell_exec('/sbin/ifconfig eth0');
preg_match('/addr:([\d\.]+)/', $ifconfig, $eth_ip);
preg_match('/Mask:([\d\.]+)/', $ifconfig, $eth_netmask);
preg_match('/Bcast:([\d\.]+)/', $ifconfig, $eth_broadcast);
unset ($ifconfig);
$iproute = shell_exec('/sbin/ip route');
preg_match('/via ([\d\.]+)/', $iproute, $eth_gateway);
unset ($iproute);
$ipdns = shell_exec('/bin/cat /etc/resolv.conf | grep nameserver');
preg_match('/nameserver ([\d\.]+)/', $ipdns, $eth_dns);
unset ($ipdns);

$ifconfig = shell_exec('/sbin/ifconfig wlan0');
if(strpos($ifconfig, "Device not found") != true) {
	$wlan_enabled = true;
	preg_match('/addr:([\d\.]+)/', $ifconfig, $wlan_ip);
    preg_match('/Mask:([\d\.]+)/', $ifconfig, $wlan_netmask);
    preg_match('/Bcast:([\d\.]+)/', $ifconfig, $wlan_broadcast);
    unset ($ifconfig);
    $iproute = shell_exec('/sbin/ip route');
    preg_match('/via ([\d\.]+)/', $iproute, $wlan_gateway);
    preg_match('/nameserver ([\d\.]+)/', $ipdns, $wlan_dns);
    unset ($iproute);
    $ipdns = shell_exec('/bin/cat /etc/resolv.conf | grep nameserver');
    preg_match('/nameserver ([\d\.]+)/', $ipdns, $wlan_dns);
    unset ($ipdns);
	$readinterface = shell_exec('/home/pi/fpp/www/readInterface.awk /etc/network/interfaces device=wlan0');
	$parsewireless = explode(" ", $readinterface);
	if (trim($parsewireless[0], "\"\n\r") == "dhcp" )
	 {
		 $wlan_mode = "dhcp";
		 $wlan_ssid = trim($parsewireless[1], "\"\n\r");
		 $wlan_passphrase = trim($parsewireless[2], "\"\n\r");
	 }
	 else
	 {
	 	 $wlan_mode = "static";
		 $wlan_ssid = $parsewireless[4];
		 $wlan_passphrase = $parsewireless[5];
	 }
	unset ($readinterface);
	unset ($parsewireless);
}
?>
<script>
window.onload = function() {
document.getElementById('eth_static').onchange = disablefield_eth;
document.getElementById('eth_dhcp').onchange = disablefield_eth;
document.getElementById('wlan_static').onchange = disablefield_wlan;
document.getElementById('wlan_dhcp').onchange = disablefield_wlan;
}

function disablefield_eth()
{
if ( document.getElementById('eth_dhcp').checked == true ){
	document.getElementById('eth_ip').disabled = true;
	document.getElementById('eth_netmask').disabled = true;
	document.getElementById('eth_broadcast').disabled = true;
	document.getElementById('eth_gateway').disabled = true;
	document.getElementById('eth_dns').disabled = true}
else if (document.getElementById('eth_static').checked == true ){
	document.getElementById('eth_ip').disabled = false;
	document.getElementById('eth_netmask').disabled = false;
	document.getElementById('eth_broadcast').disabled = false;
	document.getElementById('eth_gateway').disabled = false;
	document.getElementById('eth_dns').disabled = false}
}

function disablefield_wlan()
{
if ( document.getElementById('wlan_dhcp').checked == true ){
	document.getElementById('wlan_ip').disabled = true;
	document.getElementById('wlan_netmask').disabled = true;
	document.getElementById('wlan_broadcast').disabled = true;
	document.getElementById('wlan_gateway').disabled = true;
	document.getElementById('wlan_dns').disabled = true}
else if (document.getElementById('wlan_static').checked == true ){
	document.getElementById('wlan_ip').disabled = false;
	document.getElementById('wlan_netmask').disabled = false;
	document.getElementById('wlan_broadcast').disabled = false;
	document.getElementById('wlan_gateway').disabled = false;
	document.getElementById('wlan_dns').disabled = false}
}

function validateNetworkFields()
{
  var success = true;
  if(document.getElementById("eth_static").checked)
  {
    if(validateIPaddress(document.getElementById("eth_ip"))== false)
    {
      success = false;
    }
    if(validateIPaddress(document.getElementById("eth_gateway"))== false)
    {
      success = false;
    }
    if(validateIPaddress(document.getElementById("eth_netmask"))== false)
    {
      success = false;
    }
    if(validateIPaddress(document.getElementById("eth_broadcast"))== false)
    {
      success = false;
    }
    if(validateIPaddress(document.getElementById("eth_dns"))== false)
    {
      success = false;
    }
  }

  if(document.getElementById("wlan_static").checked)
  {
    if(validateIPaddress(document.getElementById("wlan_ip"))== false)
    {
      success = false;
    }
    if(validateIPaddress(document.getElementById("wlan_gateway"))== false)
    {
      success = false;
    }
    if(validateIPaddress(document.getElementById("wlan_netmask"))== false)
    {
      success = false;
    }
    if(validateIPaddress(document.getElementById("wlan_dns"))== false)
    {
      success = false;
    }
    if(validateIPaddress(document.getElementById("wlan_broadcast"))== false)
    {
      success = false;
    }
  }
  return success;
}

function ClearError(txtfield)
{
  //var txtFld = document.getElementById(txtfieldName);
  txtfield.style.border = "none";
  txtfield.style.border = "2px inset #EBE9ED";
}

</script>
<div id="bodyWrapper">
  <?php include 'menu.inc'; ?>
<br/>
<div id="network" class="settings">
  <fieldset>
    <legend>Network Config</legend>
    <FORM NAME="netconfig" ACTION="" METHOD="POST" onsubmit="return validateNetworkFields()" >
      <table width="100%" border="0" cellspacing="1" cellpadding="1">
          <tr>
        
          <td valign='top' colspan='3' width='50%'>
        
        <fieldset class="fs">
          <legend> Ethernet </legend>
          <table width="100%" height="200" border="0" cellpadding="1" cellspacing="1">
            <tr>
              <td width='70' rowspan='7' valign='top'><p>
                  <label>
                    <input type="radio" name="eth_mode" value="static" ID="eth_static" <?php if ($eth_mode == "static") print "Checked"?>>
                    Static</label>
                  <br>
                  <label>
                    <input type="radio" name="eth_mode" value="dhcp" ID="eth_dhcp" <?php if ($eth_mode == "dhcp") print "Checked"?>>
                    DHCP</label>
                  <br>
                </p></td>
              <td align='right'>IP Address:</td>
              <td align='center'><input type="text" name="eth_ip" id="eth_ip" onKeyDown="ClearError(this);" value="<?php print $eth_ip[1] ?>" <?php if ($eth_mode == "dhcp") print "disabled"?>>
              </td>
            </tr>
            <tr>
              <td align='right'>Netmask:</td>
              <td align='center'><input type="text" name="eth_netmask" id="eth_netmask" onKeyDown="ClearError(this);" value="<?php print $eth_netmask[1] ?>" <?php if ($eth_mode == "dhcp") print "disabled"?>>
              </td>
            </tr>
            <tr>
              <td align='right'>Broadcast:</td>
              <td align='center'><input type="text" name="eth_broadcast" id="eth_broadcast" onKeyDown="ClearError(this);" value="<?php print $eth_broadcast[1] ?>" <?php if ($eth_mode == "dhcp") print "disabled"?>>
                </td>
            </tr>
            <tr>
              <td align='right'>Gateway:</td>
              <td align='center'><input type="text" name="eth_gateway" id="eth_gateway" onKeyDown="ClearError(this);" value="<?php print $eth_gateway[1] ?>" <?php if ($eth_mode == "dhcp") print "disabled"?>>
                </td>
            </tr>
            <tr>
              <td align='right'>DNS IP:</td>
              <td align='center'><input type="text" name="eth_dns" id="eth_dns" onKeyDown="ClearError(this);" value="<?php print $eth_dns[1] ?>" <?php if ($eth_mode == "dhcp") print "disabled"?>>
                </td>
            </tr>
            <tr>
              <td>&nbsp;</td>
              <td>&nbsp;</td>
            </tr>
            <tr>
              <td>&nbsp;</td>
              <td>&nbsp;</td>
            </tr>
          </table>
            </td>
          
        </fieldset>
          <td colspan='3' width='50%'>
        
        <fieldset class="fs">
          <legend> Wireless </legend>
          <table width="100%" height="200" border="0" cellspacing="1" cellpadding="1">
            <tr>
              <td width='70' rowspan='7' valign="top"><?php if ($wlan_enabled != true) print "<center>Dongle Not Found</center><br>\r\n"?>
                <p>
                  <label>
                    <input type="radio" name="wlan_mode" value="static" id="wlan_static" <?php if ($wlan_mode == "static") print "Checked"?> <?php if ($wlan_enabled != true) print "Disabled"?>>
                    Static</label>
                  <br>
                  <label>
                    <input type="radio" name="wlan_mode" value="dhcp" id="wlan_dhcp" <?php if ($wlan_mode == "dhcp") print "Checked"?> <?php if ($wlan_enabled != true) print "Disabled"?>>
                    DHCP</label>
                </p>
                <br>
                <center>
                  <?php if ($wlan_enabled == true) print "<a href=\"/netscan.php\" target=\"_blank\">Net Scan</a>"?>
                </center></td>
              <td align='right'>IP Address:</td>
              <td align='center'><input type="text" name="wlan_ip" id="wlan_ip" onKeyDown="ClearError(this);" value="<?php print $wlan_ip[1] ?>" <?php if (($wlan_enabled != true) or ($wlan_mode == "dhcp")) print "Disabled" ?>>
                </td>
            </tr>
            <tr>
              <td align='right'>Netmask:</td>
              <td align='center'><input type="text" name="wlan_netmask" id="wlan_netmask" onKeyDown="ClearError(this);" value="<?php print $wlan_netmask[1] ?>" <?php if (($wlan_enabled != true) or ($wlan_mode == "dhcp")) print "Disabled" ?>>
               </td>
            </tr>
            <tr>
              <td align='right'>Broadcast:</td>
              <td align='center'><input type="text" name="wlan_broadcast" id="wlan_broadcast" onKeyDown="ClearError(this);" value="<?php print $wlan_broadcast[1] ?>" <?php if (($wlan_enabled != true) or ($wlan_mode == "dhcp")) print "Disabled" ?>>
              </td>
            </tr>
            <tr>
              <td align='right'>Gateway:</td>
              <td align='center'><input type="text" name="wlan_gateway" id="wlan_gateway" onKeyDown="ClearError(this);" value="<?php print $wlan_gateway[1] ?>" <?php if (($wlan_enabled != true) or ($wlan_mode == "dhcp")) print "disabled" ?>>
              </td>
            </tr>
            <tr>
              <td align='right'>DNS IP:</td>
              <td align='center'><input type="text" name="wlan_dns" id="wlan_dns" onKeyDown="ClearError(this);" value="<?php print $wlan_dns[1] ?>" <?php if (($wlan_enabled != true) or ($wlan_mode == "dhcp")) print "disabled" ?>>
              </td>
            </tr>
            <tr>
              <td align='right'>SSID:</td>
              <td align='center'><input type="text" name="wlan_ssid" id="wlan_ssid" value="<?php print $wlan_ssid ?>" <?php if ($wlan_enabled != true) print "Disabled"?>><img src='/images/filler.png' width='15' height='12'></td>
            </tr>
            <tr>
              <td align='right'>Pass Phrase:</td>
              <td align='center'><input type="text" name="wlan_passphrase" id="wlan_passphrase" value="<?php print $wlan_passphrase ?>" <?php if ($wlan_enabled != true) print "Disabled"?>><img src='/images/filler.png' width='15' height='12'></td>
            </tr>
          </table>
            </td>
          
            </td>
          
            </tr>
          
          <tr>
        </fieldset>
        
          <td colspan='6'><div id="divNetwork">
              <input name="btnSetNetwork" type="submit" class = "Buttons">
            </div></td>
        </tr>
      </table>
    </FORM>
    </td>
    </tr>
    </table>
  </fieldset>
</div>
<?php include 'common/footer.inc'; ?>
</body>
</html>
