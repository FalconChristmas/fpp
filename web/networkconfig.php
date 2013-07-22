<?php
$ifconfig = shell_exec('/sbin/ifconfig eth0');
preg_match('/addr:([\d\.]+)/', $ifconfig, $ipaddress);
preg_match('/Mask:([\d\.]+)/', $ifconfig, $netmask);
$iproute = shell_exec('/sbin/ip route');
preg_match('/via ([\d\.]+)/', $iproute, $gateway);
$ipdns = shell_exec('/bin/cat /etc/resolv.conf | grep nameserver');
preg_match('/nameserver ([\d\.]+)/', $ipdns, $nameserver);
$handle = fopen("/etc/hostname", "r"); $hostname = trim(fgets($handle)); fclose($handle);




?>
<!DOCTYPE html>
<html>
<head>
<link href="/SpryAssets/SpryValidationTextField.css" rel="stylesheet" type="text/css">
<?php include 'common/menuHead.inc'; ?>
<script type="text/javascript" src="/js/fpp.js"></script>
<script src="/SpryAssets/SpryValidationTextField.js" type="text/javascript"></script>
<title>Falcon PI Player - FPP</title>
</head>
<body>
<div id="bodyWrapper">
  <?php include 'menu.inc'; ?>
  <br/>
  <div id = "networkconfig">
    <fieldset>
      <legend>Network Config</legend>
      <FORM NAME="netconfig" ACTION="" METHOD="POST">
         <div id= "divEthernet">
            <fieldset class="fs">
              <legend> Ethernet </legend>
              <div class='right'>
              <table border="0" cellpadding="2" cellspacing="2">
              <tr>
                <td width="15%" rowspan="5" align="center" valign="top"><table width="80">
                  <tr>
                    <td><p>
                      <label>
                        <input type="radio" name="EthMode" value="static" id="EthMode_0">
                        Static</label>
                      <br>
                      <label>
                        <input type="radio" name="EthMode" value="dhcp" id="EthMode_1">
                        DHCP</label>
                      <br>
                    </p></td>
                  </tr>
                </table></td>
                <td width="20%"><div align="right">IP Address:</div></td>
                <td width="13%"><span id="sprytextfield1">
                  <label for="IpAddress"></label>
                  <input type="text" name="EthIpAddress" id="EthIpAddress">
                  <span class="textfieldRequiredMsg">A value is required.</span><span class="textfieldInvalidFormatMsg">Invalid Format!</span></span></td>
                </tr>
              <tr>
                <td><div align="right">Netmask:</div></td>
                <td><span id="sprytextfield2">
                <label for="EthNetmask"></label>
                <input type="text" name="EthNetmask" id="EthNetmask">
                <span class="textfieldRequiredMsg">A value is required.</span><span class="textfieldInvalidFormatMsg">Invalid Format!</span></span></td>
                </tr>
              <tr>
                <td><div align="right">Broadcast:</div></td>
                <td><span id="sprytextfield3">
                <label for="EthBroadcast"></label>
                <input type="text" name="EthBroadcast" id="EthBroadcast">
                <span class="textfieldRequiredMsg">A value is required.</span><span class="textfieldInvalidFormatMsg">Invalid Format!</span></span></td>
                </tr>
              <tr>
                <td><div align="right">Gateway:</div></td>
                <td><span id="sprytextfield4">
                <label for="EthGateway"></label>
                <input type="text" name="EthGateway" id="EthGateway">
                <span class="textfieldRequiredMsg">A value is required.</span><span class="textfieldInvalidFormatMsg">Invalid Format!</span></span></td>
                </tr>
              <tr>
                <td><div align="right">DNS IP:</div></td>
                <td><span id="sprytextfield5">
                <label for="EthDNS"></label>
                <input type="text" name="EthDNS" id="EthDNS">
                <span class="textfieldRequiredMsg">A value is required.</span><span class="textfieldInvalidFormatMsg">Invalid Format!</span></span></td>
                </tr>
              </table>
              </div>
            </fieldset>
          </div>
          <div id= "divWireless">
            <fieldset  class="fs">
              <legend>Wireless</legend>
              <div class='right'>
              <table width="100%">
              <tr><td>
                    <table border="0" cellpadding="2" cellspacing="2">
              <tr>
                <td width="15%" rowspan="5" align="center" valign="top"><table width="80">
                  <tr>
                    <td><p>
                      <label>
                        <input type="radio" name="WirelessMode" value="static" id="WirelessMode_0">
                        Static</label>
                      <br>
                      <label>
                        <input type="radio" name="WirelessMode" value="dhcp" id="WirelessMode_1">
                        DHCP</label>
                      <br>
                    </p></td>
                  </tr>
                </table></td>
                <td width="20%"><div align="right">IP Address:</div></td>
                <td width="13%"><span id="sprytextfield6">
                <label for="WlanIpAddress"></label>
                <input type="text" name="WlanIpAddress" id="WlanIpAddress">
                <span class="textfieldRequiredMsg">A value is required.</span><span class="textfieldInvalidFormatMsg">Invalid format.</span></span></td>
                </tr>
              <tr>
                <td><div align="right">Netmask:</div></td>
                <td><span id="sprytextfield7">
                <label for="WlanNetmask"></label>
                <input type="text" name="WlanNetmask" id="WlanNetmask">
                <span class="textfieldRequiredMsg">A value is required.</span><span class="textfieldInvalidFormatMsg">Invalid format.</span></span></td>
                </tr>
              <tr>
                <td><div align="right">Broadcast:</div></td>
                <td><span id="sprytextfield8">
                <label for="WlanBroadcast"></label>
                <input type="text" name="WlanBroadcast" id="WlanBroadcast">
                <span class="textfieldRequiredMsg">A value is required.</span><span class="textfieldInvalidFormatMsg">Invalid format.</span></span></td>
                </tr>
              <tr>
                <td><div align="right">Gateway:</div></td>
                <td><span id="sprytextfield9">
                <label for="WlanGateway"></label>
                <input type="text" name="WlanGateway" id="WlanGateway">
                <span class="textfieldRequiredMsg">A value is required.</span><span class="textfieldInvalidFormatMsg">Invalid format.</span></span></td>
                </tr>
              </table>
              </td></tr>
              </table>
              </div>
            </fieldset>
          </div>
      <div class = "clear"></div>
	  <div id="divNetwork"><br /><input name="btnSetNetwork" type="submit" class = "Buttons"></div>
     </FORM>
    </fieldset>
  </div>
</div>
<?php include 'common/footer.inc'; ?>
<script type="text/javascript">
var sprytextfield1 = new Spry.Widget.ValidationTextField("sprytextfield1", "ip", {useCharacterMasking:true, validateOn:["blur", "change"]});
var sprytextfield2 = new Spry.Widget.ValidationTextField("sprytextfield2", "ip", {validateOn:["blur", "change"], useCharacterMasking:true});
var sprytextfield3 = new Spry.Widget.ValidationTextField("sprytextfield3", "ip", {validateOn:["blur", "change"], useCharacterMasking:true});
var sprytextfield4 = new Spry.Widget.ValidationTextField("sprytextfield4", "ip", {validateOn:["blur", "change"], useCharacterMasking:true});
var sprytextfield5 = new Spry.Widget.ValidationTextField("sprytextfield5", "ip", {validateOn:["blur", "change"], useCharacterMasking:true});
var sprytextfield6 = new Spry.Widget.ValidationTextField("sprytextfield6", "ip", {validateOn:["blur", "change"], useCharacterMasking:true});
var sprytextfield7 = new Spry.Widget.ValidationTextField("sprytextfield7", "ip", {validateOn:["blur", "change"], useCharacterMasking:true});
var sprytextfield8 = new Spry.Widget.ValidationTextField("sprytextfield8", "ip", {validateOn:["blur", "change"], useCharacterMasking:true});
var sprytextfield9 = new Spry.Widget.ValidationTextField("sprytextfield9", "ip", {validateOn:["blur", "change"], useCharacterMasking:true});
</script>
</body>
</html>