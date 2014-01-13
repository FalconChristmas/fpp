<?php

// if ( isset($_POST['eth_mode']) && !empty($_POST['eth_mode']) )
  // {
  // if ($_POST['eth_mode'] == "dhcp") { 
      // $interface = "sudo changeInterface.awk /etc/network/interfaces device=eth0 mode=dhcp > /home/pi/media/interfaces";
      // error_log($interface);
	   // shell_exec($interface);
  // }
  // if ($_POST['eth_mode'] == "static") {
       // $interface = "sudo changeInterface.awk /etc/network/interfaces device=eth0 mode=static address=" . $_POST['eth_ip'] . " netmask=" . $_POST['eth_netmask'] . " broadcast=" . $_POST['eth_broadcast'] ." gateway=" . $_POST['eth_gateway'] . " > /home/pi/media/interfaces";
      // error_log($interface);
	   // shell_exec($interface);
  // }
  // if ($_POST['wlan_mode'] == "dhcp") {
	  // $interface = "sudo changeInterface.awk /etc/network/interfaces device=wlan0 mode=dhcp wpa-ssid=\"" . $_POST['wlan_ssid'] . "\" wpa-psk=\"" . $_POST['wlan_passphrase'] . "\""  . " > /home/pi/media/interfaces";
      // error_log($interface);
	   // shell_exec($interface);
  // }
  // if ($_POST['wlan_mode'] == "static") {
	  // $interface = "sudo changeInterface.awk /etc/network/interfaces device=wlan0 mode=static address=" . $_POST['wlan_ip'] . " netmask=" . $_POST['wlan_netmastk'] . " broadcast=" . $_POST['wlan_broadcast'] ." gateway=" . $_POST['wlan_gateway'] . "wpa-ssid=" . $_POST['wlan_ssid'] . " wpa-psk=" . $_POST['wlan_passphrase'] . " > /home/pi/media/interfaces";
      // error_log($interface);
	  // shell_exec($interface);
  // }
// }
?>
<!DOCTYPE html>
<html>
<head>
<?php require_once('common.php'); ?>
<?php include 'common/menuHead.inc'; ?>
<script type="text/javascript" src="/js/fpp.js"></script>
<title>Falcon PI Player - FPP</title>
</head>
<body>
<?php

function PopulateInterfaces()
{
  $interfaces = explode("\n",trim(shell_exec("/sbin/ifconfig | cut -f1 -d' ' | grep -v ^$ | grep -v lo")));
  $ifaceE131 = ReadSettingFromFile("E131interface");
  error_log("$ifaceE131:" . $ifaceE131);
  foreach ($interfaces as $iface)
  {
    $ifaceChecked = $iface == $ifaceE131 ? " selected" : "";
    echo "<option value='" . $iface . "'" . $ifaceChecked . ">" . $iface . "</option>";
  }
}


?>
<script>

window.onload = function() {
document.getElementById('eth_static').onchange = disablefield_eth;
document.getElementById('eth_dhcp').onchange = disablefield_eth;
document.getElementById('wlan_static').onchange = disablefield_wlan;
document.getElementById('wlan_dhcp').onchange = disablefield_wlan;
}

function WirelessSettingsVisible(visible)
{
  if(visible == true)
  {
    $("#WirlessSettings").show();
  }
  else
  {
    $("#WirlessSettings").hide();
  }
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
//  txtfield.style.border = "none";
//  txtfield.style.border = "1px inset #EBE9ED";
  txtfield.style.border = "black solid 1px";
}


$(document).ready(function(){
  $("#selInterfaces").change(function(){
    var iface = $('#selInterfaces').val();	
		var url = "fppxml.php?command=getInterfaceInfo&interface=" + iface;
    var visible = iface.slice(0,4).toLowerCase() == "wlan"?true:false;
    WirelessSettingsVisible(visible);
    $.get(url,GetInterfaceInfo);
  });

  $("#eth_static").click(function(){
    DisableNetworkFields(false);
    $('#eth_dhcp').prop('checked', false);
  });
  
  $("#eth_dhcp").click(function(){
    DisableNetworkFields(true);
    $('#eth_static').prop('checked', false);
  });

  
});

function GetInterfaceInfo(data,status) 
{
      var mode = $(data).find('mode').text();
      if(mode == "dhcp")
      {
        $('#eth_dhcp').prop('checked', true);
        $('#eth_static').prop('checked', false);
        DisableNetworkFields(true);
      }
      else
      {
        $('#eth_static').prop('checked', true);
        $('#eth_dhcp').prop('checked', false);
        DisableNetworkFields(false);
      }
      $('#eth_mode').val($(data).find('mode').text());
      $('#eth_ip').val($(data).find('address').text());
      $('#eth_netmask').val($(data).find('netmask').text());
      $('#eth_gateway').val($(data).find('gateway').text());
    
}

function DisableNetworkFields(disabled)
{
  $('#eth_ip').prop( "disabled", disabled );
  $('#eth_netmask').prop( "disabled", disabled );
  $('#eth_gateway').prop( "disabled", disabled );
}

</script>
<div id="bodyWrapper">
<?php include 'menu.inc'; ?>
<br/>
<div id="network" class="settings">
  <fieldset>
    <legend>Network Configuration</legend>
    <FORM NAME="netconfig" ACTION="" METHOD="POST" onsubmit="return validateNetworkFields()" >
      <div id="InterfaceSettings">
      <fieldset class="fs">
          <legend> Interface Settings</legend>
          <table width = "100%" border="0" cellpadding="1" cellspacing="1">
            <tr>
              <td width = "25%">Interface:</td>
              <td width = "25%"><select id ="selInterfaces" ><?php PopulateInterfaces();?></select></td>
              <td width = "50%">&nbsp;</td>
            </tr>
            <tr>
              <td>Interface Mode:</td>
              <td><label><input type="radio" id ="eth_static" value="static">
                    Static</label>
                  <label><input type="radio" id ="eth_dhcp" value="dhcp">
                    DHCP</label>
                  <br>
              </td>
            </tr>
            <tr>
              <td>IP Address:</td>
              <td><input type="text" name="eth_ip" id="eth_ip"></td>
            </tr>
            <tr>
              <td>Netmask:</td>
              <td><input type="text" name="eth_netmask" id="eth_netmask"></td>
            </tr>
            <tr>
              <td>Gateway:</td>
              <td><input type="text" name="eth_gateway" id="eth_gateway"></td>
            </tr>
          </table>
          <div id="WirlessSettings">
          <table width = "100%" border="0" cellpadding="1" cellspacing="1">
            <tr>
              <td width = "25%">WPA SSID:</td>
              <td width = "75%"><input type="text" name="eth_ssid" id="eth_ip"></td>
            </tr>
            <tr>
              <td>WPA Pre Shared key (PSK):</td>
              <td><input type="text" name="eth_psk" id="eth_netmask"></td>
            </tr>
            </tr>
          </table>
          </div>
          <br>
          <input name="btnSetInterface" type="" style="margin-left:190px; width:135px;" class = "buttons" value="Update Interface">        
        </fieldset>
        </div>
        <div id="DNS_Servers">
        <br>
        <fieldset class="fs2">
          <legend>DNS Servers</legend>
          <table width="100%" border="0" cellpadding="1" cellspacing="1">
            <tr>
              <td width = "25%">DNS Server 1:</td>
              <td width = "25%"><input type="text" name="eth_dns1" id="eth_dns1"></td>
              <td width = "50%">&nbsp;</td>
            </tr>
            <tr>
              <td>DNS Server 2:</td>
              <td><input type="text" name="eth_dns1" id="eth_dns2"></td>
            </tr>
          </table>
          <br>
          <input name="btnSetNetwork" type="" style="margin-left:190px; width:135px;" class = "buttons" value="Update DNS">        

        </fieldset>

        <br>
        </div>
        </fieldset>
    </FORM>
    <br>
    <div id="E131_Interface">
      <fieldset>
        <legend>E131 Output </legend>
        <table width = "100%" >
          <tr>
            <td width = "25%" >E131 Interface:</td>
            <td width = "75%" ><select id="selE131interfaces" onChange="SetE131interface();">
                <?php PopulateInterfaces(); ?>
              </select></td>
          </tr>
        </table>
      </fieldset>
    </div>
  </fieldset>
</div>
<?php include 'common/footer.inc'; ?>
</body>
</html>
