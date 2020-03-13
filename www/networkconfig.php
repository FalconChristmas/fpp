<?php require_once('common.php'); ?>
<?php include 'common/menuHead.inc'; ?>
<!DOCTYPE html>
<html>
<head>
<title><? echo $pageTitle; ?></title>
</head>
<body>
<?php
    
$wifiDrivers = Array();
$wifiDrivers['External'] = "External";
$wifiDrivers['Linux Kernel'] = "Kernel";
    
$defaultWifiDrivers = "External";
if (isset($settings['wifiDrivers']) && $settings['wifiDrivers'] == "Realtek") {
    $settings['wifiDrivers'] == "External";
}
    

function PopulateInterfaces()
{
  $first = 1;
  $interfaces = explode("\n",trim(shell_exec("/sbin/ifconfig -a | cut -f1 -d' ' | grep -v ^$ | grep -v lo | grep -v eth0:0 | grep -v usb | grep -v SoftAp | grep -v 'can.' | grep -v tether ")));
  $ifaceE131 = ReadSettingFromFile("E131interface");
  foreach ($interfaces as $iface)
  {
    $iface = preg_replace("/:$/", "", $iface);
    $ifaceChecked = $first ? " selected" : "";
    echo "<option value='" . $iface . "'" . $ifaceChecked . ">" . $iface . "</option>";
    $first = 0;
  }
}

function printTetheringSelect() {
    $tetherValues = array();
    $tetherValues["If no connection"] = 0;
    $tetherValues["Enabled"] = 1;
    $tetherValues["Disabled"] = 2;
    PrintSettingSelect("Enable Tethering", "EnableTethering", 0, 1, "0", $tetherValues);
}
function printTetheringTechnology() {
    $tetherValues = array();
    $tetherValues["Hostapd"] = 0;
    $tetherValues["ConnMan"] = 1;
    PrintSettingSelect("Tether Technology", "TetherTechnology", 0, 1, "0", $tetherValues);
}
function printTetheringInterfaces() {
    $tinterfacesRaw = explode("\n",trim(shell_exec("/sbin/ifconfig -a | cut -f1 -d' ' | grep -v ^$ | grep wlan | colrm 6")));
    $tingerfaces = Array();
    foreach ($tinterfacesRaw as $iface) {
        $tinterfaces[$iface] = $iface;
        echo "<!-- $iface -->\n";
    }
    $tiface = ReadSettingFromFile("TetherInterface");
    if (!isset($tiface) || $tiface == "") {
        $tiface = "wlan0";
    }
    PrintSettingSelect("Tether Interface", "TetherInterface", 0, 1, $tiface, $tinterfaces);
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
	).done(function(data) {
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

	if ((typeof data.DNS1 !== "undefined" && data.DNS1 != "") ||
	    (typeof data.DNS2 !== "undefined" && data.DNS2 != ""))
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

	CheckDNS();
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
		data.SSID = encodeURIComponent($('#eth_ssid').val());
		data.PSK = encodeURIComponent($('#eth_psk').val());
        data.Hidden = $('#eth_hidden').is(':checked');
	}

	var postData = "command=setInterfaceInfo&data=" + JSON.stringify(data);

	$.post("fppjson.php", postData
	).done(function(data) {
		LoadNetworkConfig();
		$.jGrowl(iface + " network interface configuration saved");
		$('#btnConfigNetwork').show();
	}).fail(function() {
		DialogError("Save Network Config", "Save Failed");
	});
}

function CreatePersistentNames() {
    $('#dialog-create-persistent').dialog({
        resizeable: false,
        height: 300,
        width: 500,
        modal: true,
        buttons: {
            "Yes" : function() {
                $(this).dialog("close");
                SetRebootFlag();
                $.get("fppjson.php?command=createPersistentNetNames", "", function() {location.reload(true);});
            },
            "No" : function() {
            $(this).dialog("close");
            }
        }
    });
}
function ClearPersistentNames() {
    $('#dialog-clear-persistent').dialog({
        resizeable: false,
        height: 300,
        width: 500,
        modal: true,
        buttons: {
            "Yes" : function() {
                $(this).dialog("close");
                SetRebootFlag();
                $.get("fppjson.php?command=clearPersistentNetNames", "", function() {location.reload(true);});
            },
            "No" : function() {
            $(this).dialog("close");
            }
        }
    });
}

function LoadNetworkConfig() {
	var iface = $('#selInterfaces').val();
	var url = "fppjson.php?command=getInterfaceInfo&interface=" + iface;
	var visible = iface.slice(0,4).toLowerCase() == "wlan"?true:false;

	WirelessSettingsVisible(visible);
	$.get(url,GetInterfaceInfo);
}

function CheckDNSCallback(data) {
	if (data.PROTO == "static") {
	   if (($('#eth_static').is(':checked')) &&
		($('#dns_dhcp').is(':checked'))) {
		$('#dnsWarning').html("Warning: You must manually configure your DNS Server(s) if all network interfaces use static IPs.");
	   } else {
                $('#dnsWarning').html("");
	   }
	} else if ($('#eth_static').is(':checked') && ($('#eth_gateway').val() == '')) {
	   $('#dnsWarning').html("Warning: if any interface is using DHCP while another interface is using a static IP address, you WILL need to enter a valid Gateway address.");
	} else {
	$('#dnsWarning').html("");
	}

}

function CheckDNS() {
	var iface = $('#selInterfaces').val();

	if (iface == 'eth0')
	{
		// FIXME, check the size of #selInterfaces here to be > 1
		iface = 'wlan0';
	}
	else if (iface == 'wlan0')
   	{
		iface = 'eth0';
	}

	var url = "fppjson.php?command=getInterfaceInfo&interface=" + iface;

	$.get(url,CheckDNSCallback);
}

$(document).ready(function(){

  LoadNetworkConfig();
  LoadDNSConfig();

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

	if (data.INTERFACE && data.INTERFACE.substr(0,4) == "wlan")
	{
		$('#eth_ssid').val(data.SSID);
		$('#eth_psk').val(data.PSK);
        if (data.HIDDEN == "1") {
            $('#eth_hidden').prop('checked', true);
        } else {
            $('#eth_hidden').prop('checked', false);
        }
	}

	CheckDNS();
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

function setHostName() {
	var newHostname = $('#hostName').val();

	var regExpHostname = new RegExp(/^([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\-]*[a-zA-Z0-9])$/);
	var regResultHostname = regExpHostname.exec(newHostname);

	if (regResultHostname === null)
	{
		alert("Invalid hostname.  Hostname may contain only letters, numbers, and hyphens and may not begin or end with a hyphen.");
		return;
	}

	$.get("fppjson.php?command=setSetting&key=HostName&value="
		+ $('#hostName').val()
	).done(function() {
		$.jGrowl("HostName Saved");
		refreshFPPSystems();
	}).fail(function() {
		DialogError("Save HostName", "Save Failed");
	});
}

function setHostDescription() {
    $.get("fppjson.php?command=setSetting&key=HostDescription&value="
        + $('#hostDescription').val()
    ).done(function() {
        $.jGrowl("HostDescription Saved");
    }).fail(function() {
        DialogError("Save HostDescription", "Save Failed");
    });
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
<?php
if (file_exists("/etc/modprobe.d/wifi-disable-power-management.conf")) {
?>
<table>
<tr>
<td width = "45%">WIFI Drivers:</td>
<td width = "55%">
<?php PrintSettingSelect("WIFI Drivers", "wifiDrivers", 0, 1, isset($settings['wifiDrivers']) ? $settings['wifiDrivers'] : $defaultWifiDrivers, $wifiDrivers, "", "reloadPage"); ?>
</td>
</tr>
</table>
<br>
<?
}
?>

          Select an interface name to configure the network information for that interface.<br><br>
          <table width = "100%" border="0" cellpadding="1" cellspacing="1">
            <tr>
              <td width = "25%" valign='top'>Interface Name:</td>
              <td width = "25%" valign='top'><select id ="selInterfaces" size='3' style="width:10em;"  onChange='LoadNetworkConfig();'><?php PopulateInterfaces();?></select></td>
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
              <td><input type="text" name="eth_ip" id="eth_ip" size=15 maxlength=15 onChange="checkStaticIP();">
								<input type="button" onClick='PingIP($("#eth_ip").val(), 3);' value='Ping'></td>
            </tr>
            <tr>
              <td>Netmask:</td>
              <td><input type="text" name="eth_netmask" id="eth_netmask" size="15" maxlength="15"></td>
            </tr>
            <tr>
              <td>Gateway:</td>
              <td><input type="text" name="eth_gateway" id="eth_gateway" size="15" maxlength="15">
								<input type="button" onClick='PingIP($("#eth_gateway").val(), 3);' value='Ping'></td>
            </tr>
            <tr>
              <td colspan='2'><b><font color='#ff0000'><span id='dnsWarning'></span></font></b></td>
            </tr>
          </table>
		  <br>
          <div id="WirelessSettings">
		  <b>Wireless Settings:</b>
          <table width = "100%" border="0" cellpadding="1" cellspacing="1">
            <tr>
              <td width = "25%">WPA SSID:</td>
              <td width = "75%"><input type="text" name="eth_ssid" id="eth_ssid" size="32" maxlength="32">&nbsp;<input type="checkbox" name="eth_hidden" id="eth_hidden" value="Hidden">Hidden</td>
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

        &nbsp; &nbsp; &nbsp;<input id="btnConfigNetworkPersist" type="" style="width:145px;" class = "buttons" value="Create Persistent Names" onClick="CreatePersistentNames();">
        &nbsp;<input id="btnConfigNetworkPersistClear" type="" style="width:145px; " class = "buttons" value="Clear Persistent Names" onClick="ClearPersistentNames();">
        </fieldset>
        </div>
        <div id="DNS_Servers">
        <br>
        <fieldset class="fs2">
          <legend>Host & DNS Settings</legend>
          <table width="100%" border="0" cellpadding="1" cellspacing="1">
            <tr>
              <td width = "25%">HostName:</td>
              <td colspan='2'><input id='hostName' value='<? if (isset($settings['HostName'])) echo $settings['HostName']; else echo 'FPP'; ?>' size='30' maxlength='30'> <input type='button' class='buttons' value='Save' onClick='setHostName();'></td>
            </tr>
              <tr>
                  <td width = "25%">Description:</td>
                  <td colspan='2'><input id='hostDescription' value='<? if (isset($settings['HostDescription'])) echo $settings['HostDescription']; else echo ('Falcon Player - ' .  $settings['Variant']); ?>' size='30' maxlength='48'> <input type='button' class='buttons' value='Save' onClick='setHostDescription();'></td>
              </tr>
            <tr>
              <td>&nbsp;</td>
            </tr>
            <tr>
              <td>DNS Server Mode:</td>
              <td><label><input type="radio" id ="dns_manual" value="manual">
                    Manual</label>
                  <label><input type="radio" id ="dns_dhcp" value="dhcp" checked>
                    DHCP</label>
                  <br>
              </td>
            </tr>
            <tr>
              <td width = "25%">DNS Server 1:</td>
              <td width = "25%"><input type="text" name="dns1" id="dns1"></td>
              <td width = "50%">
								<input type="button" onClick='PingIP($("#dns1").val(), 3);' value='Ping'></td>
            </tr>
            <tr>
              <td>DNS Server 2:</td>
              <td><input type="text" name="dns2" id="dns2"></td>
							<td>
								<input type="button" onClick='PingIP($("#dns2").val(), 3);' value='Ping'></td>
            </tr>
          </table>
          <br>
          <input name="btnSetDNS" type="" style="margin-left:190px; width:135px;" class = "buttons" value="Update DNS" onClick="SaveDNSConfig();">
          <input id="btnConfigDNS" type="" style="width:135px; display: none;" class = "buttons" value="Restart DNS" onClick="ApplyDNSConfig();">

        </fieldset>
        <br>
        <fieldset class="fs2">
        <legend>Tethering</legend>
            <table width = "100%" border="0" cellpadding="1" cellspacing="1">
            <tr>
                <td width = "25%">Tethering Mode:</td>
                <td width = "75%"><? printTetheringSelect(); ?></td>
            </tr>
            <tr>
                <td width = "25%">Tethering Interface:</td>
                <td width = "75%"><? printTetheringInterfaces(); ?></td>
            </tr>
            <tr>
                <td width = "25%">Tethering Technology:</td>
                <td width = "75%"><? printTetheringTechnology(); ?></td>
            </tr>
            <tr>
                <td width = "25%">Tethering SSID:</td>
                <td width = "75%"><? PrintSettingTextSaved("TetherSSID", 0, 1, 32, 32, "", "FPP"); ?></td>
            </tr>
            <tr>
                <td>Tethering Pre Shared key (PSK):</td>
                <td><? PrintSettingTextSaved("TetherPSK", 0, 1, 32, 32, "", "Christmas"); ?></td>
            </tr>
            </tr>
            </table>
                <br>
                <b>Warning:</b> Turning on tethering may make FPP unavailable.   The WIFI adapter will be used for
        tethering and will thus not be usable for normal network operations.   The WIFI tether IP address will be
192.168.8.1 for Hostapd tethering, but unprecitable for ConnMan (although likely 192.168.0.1).
<p>
<? if ($settings['Platform'] == "BeagleBone Black") { ?>
    On BeagleBones, USB tethering is available unless ConnMan tethering is enabled.  The IP address for USB tethering would be 192.168.6.2
        (OSX/Linux) or 192.168.7.2 (Windows).
<? } ?>
<? if ($settings['Platform'] == "Raspberry Pi") { ?>
    On the Pi Zero and Pi Zero W devices, USB tethering is available if using an appropriate USB cable plugged into the USB port, not the power-only port.  Don't plug anything into the power port for this.  The IP address for USB tethering would be 192.168.7.2.
<? } ?>

            </fieldset>
            <br>

            <fieldset class="fs2">
                <legend>Interface Routing</legend>
				<? PrintSettingCheckbox("Enable Routing", "EnableRouting", 0, 0, "1", "0"); ?> Enable Routing between
                network interfaces
                <br>
        </div>
  </fieldset>

<div id="dialog-confirm" style="display: none">
	<p><span class="ui-icon ui-icon-alert" style="flat:left; margin: 0 7px 20px 0;"></span>Reconfiguring the network will cause you to lose your connection and have to reconnect if you have changed the IP address.  Do you wish to proceed?</p>
</div>
<div id="dialog-clear-persistent" style="display: none">
<p><span class="ui-icon ui-icon-alert" style="flat:left; margin: 0 7px 20px 0;"></span>Clearing out persistent device names can cause interfaces to use different configuration and become unavailable.  Do you wish to proceed?</p>
</div>
<div id="dialog-create-persistent" style="display: none">
    <p><span class="ui-icon ui-icon-alert" style="flat:left; margin: 0 7px 20px 0;"></span>Creating persisten device names can make it harder to add new network devices or replace existing devices in the future.  Do you wish to proceed?</p>
</div>
<?php include 'common/footer.inc'; ?>
</div>
</div>
</body>
</html>
