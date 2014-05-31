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
<title>Falcon PI Player - FPP</title>
</head>
<body>
<?php

function PopulateInterfaces()
{
  $interfaces = explode("\n",trim(shell_exec("/sbin/ifconfig | cut -f1 -d' ' | grep -v ^$ | grep -v lo | grep -v eth0:0")));
  $ifaceE131 = ReadSettingFromFile("E131interface");
  error_log("$ifaceE131:" . $ifaceE131);
  foreach ($interfaces as $iface)
  {
    $iface = preg_replace("/:$/", "", $iface);
    $ifaceChecked = $iface == $ifaceE131 ? " selected" : "";
    echo "<option value='" . $iface . "'" . $ifaceChecked . ">" . $iface . "</option>";
  }
}


?>
<script>

function WirelessSettingsVisible(visible)
{
  if(visible == true)
  {
    $("#WirelessSettings").show();
  }
  else
  {
    $("#WirelessSettings").hide();
  }
}

function checkStaticIP()
{
	if ($('#eth_ip').val().substr(0,7) == "192.168")
	{
		if ($('#eth_netmask').val() == "")
		{
			$('#eth_netmask').val("255.255.255.0");
		}
		if ($('#eth_gateway').val() == "")
		{
			var gateway = $('#eth_ip').val().replace(/\.\d+$/, ".1");
			$('#eth_gateway').val(gateway);
		}
	}
}

function validateNetworkFields()
{
	if($('#eth_static').is(':checked'))
	{
		if(validateIPaddress('eth_ip')== false)
		{
			$.jGrowl("Invalid IP Address");
			return false;
		}
		if(validateIPaddress('eth_netmask')== false)
		{
			$.jGrowl("Invalid Netmask");
			return false;
		}
		if(validateIPaddress('eth_gateway')== false)
		{
			$.jGrowl("Invalid Gateway");
			return false;
		}
	}

	return true;
}

function validateDNSFields()
{
	if(validateIPaddress('dns1') == false)
	{
		$.jGrowl("Invalid DNS Server #1");
		return false;
	}
	if(validateIPaddress('dns2') == false)
	{
		$.jGrowl("Invalid DNS Server #2");
		return false;
	}

	return true;
}

function ApplyDNSConfig()
{
	$.get("fppjson.php?command=applyDNSInfo", LoadDNSConfig);
}

function SaveDNSConfig()
{
	if (validateDNSFields() == false)
	{
		DialogError("Invalid DNS Config", "Save Failed");
		return;
	}

	var data = {};

	if ($('#dns_manual').is(':checked'))
	{
		data.DNS1 = $('#dns1').val();
		data.DNS2 = $('#dns2').val();
	}
	else
	{
		data.DNS1 = "";
		data.DNS2 = "";
	}

	var postData = "command=setDNSInfo&data=" + JSON.stringify(data);

	$.post("fppjson.php", postData
	).success(function(data) {
		LoadDNSConfig();
		$.jGrowl(" DNS configuration saved");
		$('#btnConfigDNS').show();
	}).fail(function() {
		DialogError("Save DNS Config", "Save Failed");
	});

}

function GetDNSInfo(data)
{
	$('#dns1').val(data.DNS1);
	$('#dns2').val(data.DNS2);

	if ((data.DNS1 != "") || (data.DNS2 != ""))
	{
			$('#dns_manual').prop('checked', true);
			$('#dns_dhcp').prop('checked', false);
			DisableDNSFields(false);
	}
	else
	{
			$('#dns_manual').prop('checked', false);
			$('#dns_dhcp').prop('checked', true);
			DisableDNSFields(true);
	}
}

function LoadDNSConfig()
{
	var url = "fppjson.php?command=getDNSInfo";

	$.get(url, GetDNSInfo);
}

function ApplyNetworkConfig()
{
	$('#dialog-confirm').dialog({
		resizeable: false,
		height: 300,
		width: 500,
		modal: true,
		buttons: {
			"Yes" : function() {
				$(this).dialog("close");
				$.get("fppjson.php?command=applyInterfaceInfo&interface=" + $('#selInterfaces').val());
				},
			"Cancel and apply at next reboot" : function() {
				$(this).dialog("close");
				}
			}
		});
}

function SaveNetworkConfig()
{
	if (validateNetworkFields() == false)
	{
		DialogError("Invalid Network Config", "Save Failed");
		return;
	}

	var iface = $('#selInterfaces').val();
	var url;
	var data = {};
	data.INTERFACE = iface;
	if ($('#eth_static').is(':checked')) {
		data.PROTO   = 'static';
		data.ADDRESS = $('#eth_ip').val();
		data.NETMASK = $('#eth_netmask').val();
		data.GATEWAY = $('#eth_gateway').val();
	} else {
		data.PROTO   = 'dhcp';
	}

	if (iface.substr(0,4) == "wlan")
	{
		data.SSID = $('#eth_ssid').val();
		data.PSK = $('#eth_psk').val();
	}

	var postData = "command=setInterfaceInfo&data=" + JSON.stringify(data);

	$.post("fppjson.php", postData
	).success(function(data) {
		LoadNetworkConfig();
		$.jGrowl(iface + " network interface configuration saved");
		$('#btnConfigNetwork').show();
	}).fail(function() {
		DialogError("Save Network Config", "Save Failed");
	});
}

function LoadNetworkConfig() {
	var iface = $('#selInterfaces').val();
	var url = "fppjson.php?command=getInterfaceInfo&interface=" + iface;
	var visible = iface.slice(0,4).toLowerCase() == "wlan"?true:false;

	WirelessSettingsVisible(visible);
	$.get(url,GetInterfaceInfo);
}

$(document).ready(function(){

  LoadNetworkConfig();
  LoadDNSConfig();

  $("#selInterfaces").change(function(){
    LoadNetworkConfig();
  });

  $("#eth_static").click(function(){
    DisableNetworkFields(false);
    $('#eth_dhcp').prop('checked', false);

    $('#dns_dhcp').prop('checked', false);
    $('#dns_manual').prop('checked', true);
    DisableDNSFields(false);
  });
  
  $("#eth_dhcp").click(function(){
    DisableNetworkFields(true);
    $('#eth_static').prop('checked', false);
    $('#eth_ip').val("");
    $('#eth_netmask').val("");
    $('#eth_gateway').val("");
  });

  $("#dns_manual").click(function(){
    DisableDNSFields(false);
    $('#dns_dhcp').prop('checked', false);
  });
  
  $("#dns_dhcp").click(function(){
    DisableDNSFields(true);
    $('#dns_manual').prop('checked', false);
    $('#dns1').val("");
    $('#dns2').val("");
  });


});

function GetInterfaceInfo(data,status) 
{
	if(data.PROTO == "static")
	{
		$('#eth_static').prop('checked', true);
		$('#eth_dhcp').prop('checked', false);
		DisableNetworkFields(false);
	}
	else
	{
		$('#eth_dhcp').prop('checked', true);
		$('#eth_static').prop('checked', false);
		DisableNetworkFields(true);
	}

	$('#eth_ip').val(data.ADDRESS);
	$('#eth_netmask').val(data.NETMASK);
	$('#eth_gateway').val(data.GATEWAY);

	if (data.INTERFACE.substr(0,4) == "wlan")
	{
		$('#eth_ssid').val(data.SSID);
		$('#eth_psk').val(data.PSK);
	}
}

function DisableNetworkFields(disabled)
{
  $('#eth_ip').prop( "disabled", disabled );
  $('#eth_netmask').prop( "disabled", disabled );
  $('#eth_gateway').prop( "disabled", disabled );
}

function DisableDNSFields(disabled)
{
  $('#dns1').prop( "disabled", disabled );
  $('#dns2').prop( "disabled", disabled );
}

</script>
<div id="bodyWrapper">
<?php include 'menu.inc'; ?>
<br/>
<div id="network" class="settings">
  <fieldset>
    <legend>Network Configuration</legend>
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
              <td><input type="text" name="eth_ip" id="eth_ip" size=15 maxlength=15 onChange="checkStaticIP();"></td>
            </tr>
            <tr>
              <td>Netmask:</td>
              <td><input type="text" name="eth_netmask" id="eth_netmask" size="15" maxlength="15"></td>
            </tr>
            <tr>
              <td>Gateway:</td>
              <td><input type="text" name="eth_gateway" id="eth_gateway" size="15" maxlength="15"></td>
            </tr>
          </table>
		  <br>
          <div id="WirelessSettings">
		  <b>Wireless Settings:</b>
          <table width = "100%" border="0" cellpadding="1" cellspacing="1">
            <tr>
              <td width = "25%">WPA SSID:</td>
              <td width = "75%"><input type="text" name="eth_ssid" id="eth_ssid" size="32" maxlength="32"></td>
            </tr>
            <tr>
              <td>WPA Pre Shared key (PSK):</td>
              <td><input type="text" name="eth_psk" id="eth_psk" size="32" maxlength="64"></td>
            </tr>
            </tr>
          </table>
          </div>
          <br>
          <input name="btnSetInterface" type="" style="margin-left:190px; width:135px;" class = "buttons" value="Update Interface" onClick="SaveNetworkConfig();">        
          <input id="btnConfigNetwork" type="" style="width:135px; display: none;" class = "buttons" value="Restart Network" onClick="ApplyNetworkConfig();">
        </fieldset>
        </div>
        <div id="DNS_Servers">
        <br>
        <fieldset class="fs2">
          <legend>DNS Servers</legend>
          <table width="100%" border="0" cellpadding="1" cellspacing="1">
            <tr>
              <td>DNS Server Mode:</td>
              <td><label><input type="radio" id ="dns_manual" value="manual">
                    Manual</label>
                  <label><input type="radio" id ="dns_dhcp" value="dhcp">
                    DHCP</label>
                  <br>
              </td>
            </tr>
            <tr>
              <td width = "25%">DNS Server 1:</td>
              <td width = "25%"><input type="text" name="dns1" id="dns1"></td>
              <td width = "50%">&nbsp;</td>
            </tr>
            <tr>
              <td>DNS Server 2:</td>
              <td><input type="text" name="dns2" id="dns2"></td>
            </tr>
          </table>
          <br>
          <input name="btnSetDNS" type="" style="margin-left:190px; width:135px;" class = "buttons" value="Update DNS" onClick="SaveDNSConfig();">        
          <input id="btnConfigDNS" type="" style="width:135px; display: none;" class = "buttons" value="Restart DNS" onClick="ApplyDNSConfig();">

        </fieldset>

        <br>
        </div>
        </fieldset>
  </fieldset>
</div>
<div id="dialog-confirm" style="display: none">
	<p><span class="ui-icon ui-icon-alert" style="flat:left; margin: 0 7px 20px 0;"></span>Reconfiguring the network will cause you to lose your connection and have to reconnect if you have changed the IP address.  Do you wish to proceed?</p>
</div>
<?php include 'common/footer.inc'; ?>
</body>
</html>
