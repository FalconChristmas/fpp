<!DOCTYPE html>
<html>
<head>
<?php require_once 'common.php';?>
<?php include 'common/menuHead.inc';?>

<title><?echo $pageTitle; ?></title>

<script>
var hiddenChildren = {};
function UpdateChildSettingsVisibility() {
    hiddenChildren = {};
    $('.parentSetting').each(function() {
        var fn = 'Update' + $(this).attr('id') + 'Children';
        window[fn](2); // Hide if necessary
    });
    $('.parentSetting').each(function() {
        var fn = 'Update' + $(this).attr('id') + 'Children';
        window[fn](1); // Show if not hidden
    });
}
$(document).ready(function() {
    UpdateChildSettingsVisibility();
});
</script>
</head>
<body>


<?php

function PopulateInterfaces()
{
    $first = 1;
    $interfaces = network_list_interfaces_array();
    foreach ($interfaces as $iface) {
        $iface = preg_replace("/:$/", "", $iface);
        $ifaceChecked = $first ? " selected" : "";
        echo "<option value='" . $iface . "'" . $ifaceChecked . ">" . $iface . "</option>";
        $first = 0;
    }
}

?>
<script>

function WirelessSettingsVisible(visible) {
    if(visible == true) {
        LoadSIDS($('#selInterfaces').val());
        $("#WirelessSettings").show();
    } else {
      $("#WirelessSettings").hide();
    }
}

function checkStaticIP() {
	var ip = $('#eth_ip').val();
    $("#ipWarning").html('');
	if (ip.startsWith( "192.168") || ip.startsWith("10.") ) {
		if ($('#eth_netmask').val() == "") {
			$('#eth_netmask').val("255.255.255.0");
		}
		if ($('#eth_gateway').val() == "") {
			var gateway = $('#eth_ip').val().replace(/\.\d+$/, ".1");
			$('#eth_gateway').val(gateway);
		}
	}
    if (ip.startsWith("192.168.6.") || ip.startsWith("192.168.7.") || ip.startsWith("192.168.8.")) {
        var text = "It is recommended to use subnets other than 192.168.6.x, 192.168.7.x, and 192.168.8.x to avoid issues with tethering."
		$("#ipWarning").html(text);
		$.jGrowl(text,{themeState:'danger'});
    }
}

function validateNetworkFields() {
    var eth_ip = $('#eth_ip').val();
    $("#ipWarning").html('');
	if($('#eth_static').is(':checked')) {
		if((validateIPaddress('eth_ip')== false) || (eth_ip == "")) {
		    $.jGrowl("Invalid IP Address. Expect format like 192.168.0.101",{themeState:'danger'});
			$("#ipWarning").html('Invalid IP Address. Expect format like 192.168.0.101');
			return false;
		}

		if((validateIPaddress('eth_netmask')== false) || ($('#eth_netmask').val() == "")) {
			$.jGrowl("Invalid Netmask. Expect format like 255.255.255.0",{themeState:'danger'});
			$("#ipWarning").html('Invalid Netmask. Expect format like 255.255.255.0');
			return false;
		}
		if(validateIPaddress('eth_gateway')== false) {
			$.jGrowl("Invalid Gateway. Expect format like 192.168.0.1",{themeState:'danger'});
			return false;
		}
	}
	return true;
}

function validateDNSFields()
{
    setDNSWarning("");
    if(validateIPaddress('dns1') == false) {
        $.jGrowl("Invalid DNS Server #1",{themeState:'danger'});
        setDNSWarning("Invalid DNS Server #1");
		return false;
	}
	if(validateIPaddress('dns2') == false) {
		$.jGrowl("Invalid DNS Server #2",{themeState:'danger'});
        setDNSWarning("Invalid DNS Server #2");
		return false;
	}
	return true;
}

function SaveDNSConfig() {
	if (validateDNSFields() == false) {
		DialogError("Invalid DNS Config", "Save Failed");
		return;
	}

	var data = {};

	if ($('#dns_manual').is(':checked')) {
		data.DNS1 = $('#dns1').val();
		data.DNS2 = $('#dns2').val();
	} else {
		data.DNS1 = "";
		data.DNS2 = "";
	}

	var postData = JSON.stringify(data);

	$.post("api/network/dns", postData
	).done(function(data) {
		LoadDNSConfig();
        SetRebootFlag();
		$.jGrowl(" DNS configuration saved",{themeState:'success'});
	}).fail(function() {
		DialogError("Save DNS Config", "Save Failed");
	});
}

function GetDNSInfo(data) {
	$('#dns1').val(data.DNS1);
	$('#dns2').val(data.DNS2);

	if ((typeof data.DNS1 !== "undefined" && data.DNS1 != "") ||
	    (typeof data.DNS2 !== "undefined" && data.DNS2 != "")) {
		$('#dns_manual').prop('checked', true);
		$('#dns_dhcp').prop('checked', false);
		DisableDNSFields(false);
	} else {
		$('#dns_manual').prop('checked', false);
		$('#dns_dhcp').prop('checked', true);
		DisableDNSFields(true);
	}

	CheckDNS();
}

function onlyUnique(value, index, self) {
    return self.indexOf(value) === index;
}

function LoadSIDS(interface) {
  $.get("api/network/wifi/scan/" + interface
  ).done(function(data){
    var ssids = [];
    data.networks.forEach(function(n){
        if (n.SSID != "") {
            ssids.push(n.SSID);
        }
    });
    ssids = ssids.filter(onlyUnique);
    ssids.sort();
    html=[];
    ssids.forEach(function(n){
        html.push("<option value='");
        html.push(n);
        html.push("'>");
    });
    $("#eth_ssids").html(html.join(''));
  }).fail(function(){
    DialogError("Scan Failed", "Failed to Scan for wifi networks");
  });
}

function LoadDNSConfig() {
	var url = "api/network/dns";
	$.get(url, GetDNSInfo);
}

function ApplyNetworkConfig()
{
	$('#dialog-confirm').fppDialog({
		resizeable: false,
		width: 500,
		modal: true,
		buttons: {
			"Yes" : {class:'btn-success',click:function() {
				$(this).fppDialog("close");
				$.post("api/network/interface/" + $('#selInterfaces').val() + "/apply");
				}},
			"Cancel and apply at next reboot" : {click:function() {
				$(this).fppDialog("close");
				}}
			}
		});
}

function SaveNetworkConfig() {
	if (validateNetworkFields() == false) {
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
<?if ($settings['uiLevel'] >= 1) {?>
    data.ROUTEMETRIC = $('#routeMetric').val();
    data.DHCPSERVER = $('#dhcpServer').is(':checked');
    data.DHCPOFFSET = $('#dhcpOffset').val();
    data.DHCPPOOLSIZE = $('#dhcpPoolSize').val();
    data.IPFORWARDING = $('#ipForwarding').val();
<?}?>

	if (iface.substr(0,2) == "wl") {
		data.SSID = $('#eth_ssid').val();
		data.PSK = $('#eth_psk').val();
        data.HIDDEN = $('#eth_hidden').is(':checked');
	}

	var postData = JSON.stringify(data);

	$.post("api/network/interface/" + iface, postData
	).done(function(rc) {
        if (rc.status == "OK") {
            LoadNetworkConfig();
            $.jGrowl(iface + " network interface configuration saved",{themeState:'success'});
            $('#btnConfigNetwork').show();

            if (data.PROTO == 'static' && $('#dns1').val() == "" && $('#dns2').val() == "") {
                DialogOK("Check DNS", "Don't forget to set a DNS IP address. You may use 8.8.8.8 or 1.1.1.1 if you are not sure.")
            }
        } else {
            DialogError("Save Network Config", "Save Failed: " + rc.status);
        }
	}).fail(function() {
		DialogError("Save Network Config", "Save Failed");
	});
}

function CreatePersistentNames() {
    $('#dialog-create-persistent').fppDialog({
        resizeable: false,
        height: 300,
        width: 500,
        modal: true,
        buttons: {
            "Yes" : function() {
                $(this).fppDialog("close");
                SetRebootFlag();
                $.post("api/network/presisentNames", "", function() {location.reload(true);});
            },
            "No" : function() {
            $(this).fppDialog("close");
            }
        }
    });
}
function ClearPersistentNames() {
    $('#dialog-clear-persistent').fppDialog({
        resizeable: false,
        height: 300,
        width: 500,
        modal: true,
        buttons: {
            "Yes" : function() {
                $(this).fppDialog("close");
                SetRebootFlag();
                $.ajax( {
                  type: "DELETE",
                  url: "api/network/presisentNames",
                  data: "",
                  success: function() {location.reload(true);},
                });
            },
            "No" : function() {
            $(this).fppDialog("close");
            }
        }
    });
}

function LoadNetworkConfig() {
	var iface = $('#selInterfaces').val();
	var url = "api/network/interface/" + iface;
	var visible = iface.slice(0,2).toLowerCase() == "wl"?true:false;

	$.get(url,GetInterfaceInfo);
    WirelessSettingsVisible(visible);
}


function CheckDNSCallback(data) {
    var iface = $('#selInterfaces').val();
	if (iface.startsWith('e')) {
		// FIXME, check the size of #selInterfaces here to be > 1
		iface = 'wl';
	} else if (iface.startsWith('wl')) {
		iface = 'e';
	}

    data.forEach(function(i) {
        if (i.ifname.startsWith(iface)) {
            if (i.hasOwnProperty("config")) {
                if (i.config.PROTO == "static") {
	                if (($('#eth_static').is(':checked')) &&
		                ($('#dns_dhcp').is(':checked'))) {
                        setDNSWarning("Warning: You must manually configure your DNS Server(s) if all network interfaces use static IPs.");
	                } else {
                        setDNSWarning("");
	                }
	            } else if ($('#eth_static').is(':checked') && ($('#eth_gateway').val() == '')) {
                    setDNSWarning("Warning: if any interface is using DHCP while another interface is using a static IP address, you WILL need to enter a valid Gateway address.");
	            } else {
                    setDNSWarning("");
	            }
            }
        }
    });
}

function CheckDNS() {
	var url = "api/network/interface";

	$.get(url,CheckDNSCallback);
}

function setDNSWarning(msg) {
  if (msg == "") {
    $("#dns_warning").hide();
    $("#dnsWarning").hide();
  } else {
    $("#dns_warning").html(msg).show();
    $("#dnsWarning").html(msg).show();
  }
}

$(document).ready(function(){

    LoadNetworkConfig();
    LoadDNSConfig();

    $("#dns1").change(validateDNSFields);
    $("#dns2").change(validateDNSFields);

    $("#eth_static").click(function() {
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
<?if ($settings['uiLevel'] >= 1) {?>
        $('#dhcpServer').prop('checked', false);
        UpdateChildSettingsVisibility();
<?}?>
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

function GetInterfaceInfo(data,status) {
	if(data.PROTO == "static") {
		$('#eth_static').prop('checked', true);
		$('#eth_dhcp').prop('checked', false);
		DisableNetworkFields(false);
	} else {
		$('#eth_dhcp').prop('checked', true);
		$('#eth_static').prop('checked', false);
		DisableNetworkFields(true);
	}

	$('#eth_ip').val(data.ADDRESS);
	$('#eth_netmask').val(data.NETMASK);
	$('#eth_gateway').val(data.GATEWAY);

	if (data.INTERFACE && data.INTERFACE.substr(0,2) == "wl") {
		$('#eth_ssid').val(data.SSID);
		$('#eth_psk').val(data.PSK);
        if (data.HIDDEN == "1") {
            $('#eth_hidden').prop('checked', true);
        } else {
            $('#eth_hidden').prop('checked', false);
        }
	}
<?if ($settings['uiLevel'] >= 1) {?>
    if (data.ROUTEMETRIC) {
        $('#routeMetric').val(data.ROUTEMETRIC);
    } else {
        $('#routeMetric').val(0);
    }
    if (data.DHCPSERVER) {
        if (data.DHCPSERVER == "1") {
            $('#dhcpServer').prop('checked', true);
        } else {
            $('#dhcpServer').prop('checked', false);
        }
    } else {
        $('#dhcpServer').prop('checked', false);
    }
    if (data.DHCPOFFSET) {
        $('#dhcpOffset').val(data.DHCPOFFSET);
    } else {
        $('#dhcpOffset').val(100);
    }
    if (data.DHCPPOOLSIZE) {
        $('#dhcpPoolSize').val(data.DHCPPOOLSIZE);
    } else {
        $('#dhcpPoolSize').val(50);
    }
    if (data.IPFORWARDING) {
        $('#ipForwarding').val(data.IPFORWARDING);
    } else {
        $('#ipForwarding').val(0);
    }
    UpdateChildSettingsVisibility();
<?}?>
	CheckDNS();
}

function DisableNetworkFields(disabled) {
    $('#eth_ip').prop( "disabled", disabled );
    $('#eth_netmask').prop( "disabled", disabled );
    $('#eth_gateway').prop( "disabled", disabled );
    <?if ($settings['uiLevel'] >= 1) {?>
    $('#dhcpServer').prop('disabled', disabled);
    <?}?>
}

function DisableDNSFields(disabled) {
    $('#dns1').prop( "disabled", disabled );
    $('#dns2').prop( "disabled", disabled );
}

function setHostName() {
	var newHostname = $('#hostName').val();

	var regExpHostname = new RegExp(/^([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\-]*[a-zA-Z0-9])$/);
	var regResultHostname = regExpHostname.exec(newHostname);

	if (regResultHostname === null) {
		alert("Invalid hostname.  Hostname may contain only letters, numbers, and hyphens and may not begin or end with a hyphen.");
		return;
	}

	$.put("api/settings/HostName", $('#hostName').val()
	).done(function() {
		$.jGrowl("HostName Saved",{themeState:'success'});
        SetRebootFlag();
	}).fail(function() {
		DialogError("Save HostName", "Save Failed");
	});
}

function setHostDescription() {
    $.put("api/settings/HostDescription", $('#hostDescription').val()
    ).done(function() {
        $.jGrowl("HostDescription Saved",{themeState:'success'});
        SetRestartFlag(2);
    }).fail(function() {
        DialogError("Save HostDescription", "Save Failed");
    });
}

</script>
<div id="bodyWrapper">
<?php
$activeParentMenuItem = 'status';
include 'menu.inc';?>
  <div class="mainContainer">
  <h1 class="title">Network Configuration</h1>
  <div class="pageContent">
  <ul class="nav nav-pills pageContent-tabs" role="tablist">
    <li class="nav-item">
      <a class="nav-link active" id="interface-settings-tab" data-toggle="tab" href="#tab-interface-settings" role="tab" aria-controls="interface-settings" aria-selected="true">
        Interface Settings
      </a>
    </li>
    <li class="nav-item">
      <a class="nav-link" id="tab-host-dns-tab" data-toggle="tab" href="#tab-host-dns" role="tab" aria-controls="tab-host-dns" aria-selected="false">
      Host & DNS Settings
      </a>
    </li>
    <li class="nav-item">
      <a class="nav-link" id="tab-tethering-tab" data-toggle="tab" href="#tab-tethering" role="tab" aria-controls="tab-tethering" aria-selected="false">
        Tethering
      </a>
    </li>
<?if (file_exists("/usr/bin/connmanctl")) {?>
    <li class="nav-item">
      <a class="nav-link" id="tab-interface-routing-tab" data-toggle="tab" href="#tab-interface-routing" role="tab" aria-controls="tab-interface-routing" aria-selected="false">
        Interface Routing
      </a>
    </li>
<?}?>
  </ul>
  <div class="tab-content">
    <div class="tab-pane fade show active" id="tab-interface-settings" role="tabpanel" aria-labelledby="interface-settings-tab">
    <div id="InterfaceSettings">

<?php
if (file_exists("/etc/modprobe.d/wifi-disable-power-management.conf")) {
    PrintSettingGroup("wifiDriversGroup");
}
?>
        <h2> Interface Settings</h2>
        <div class="container-fluid settingsTable settingsGroupTable">
            <div class="row"><div class="printSettingLabelCol col-md-8">Select an interface name to configure the network information for that interface.</div></div>
            <div class="row" id="selInterfacesRow">
                <div class="printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2"><div class="description">Interface Name</div></div>
                <div class="printSettingFieldCol col-md"><select id ="selInterfaces"  class='multiSelect' size='3' style="width:10em;"  onChange='LoadNetworkConfig();'><?php PopulateInterfaces();?></select></div>
            </div>
            <div class="row" id="interfaceConfigTypeRow">
                <div class="printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2"><div class="description">Interface Mode</div></div>
                <div class="printSettingFieldCol col-md"><label><input type="radio" id ="eth_static" value="static">
                        Static</label>
                      <label><input type="radio" id ="eth_dhcp" value="dhcp">
                        DHCP</label></div>
            </div>
            <div class="row" id="ipAddressRow">
                <div class="printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2"><div class="description">IP Address</div></div>
                <div class="printSettingFieldCol col-md"><input type="text" name="eth_ip" id="eth_ip" size=15 maxlength=15 onChange="checkStaticIP();">
    								<input type="button" class="buttons" onClick='PingIP($("#eth_ip").val(), 3);' value='Ping'></div>
            </div>
            <div class="row" id="netmaskRow">
                <div class="printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2"><div class="description">Netmask</div></div>
                <div class="printSettingFieldCol col-md"><input type="text" name="eth_netmask" id="eth_netmask" size="15" maxlength="15"></div>
            </div>
            <div class="row" id="gatewayRow">
                <div class="printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2"><div class="description">Netmask</div></div>
                <div class="printSettingFieldCol col-md"><input type="text" name="eth_gateway" id="eth_gateway" size="15" maxlength="15">
    								<input type="button" class="buttons" onClick='PingIP($("#eth_gateway").val(), 3);' value='Ping'></div>
            </div>
            <div class="row">
                <div class="col-md-6"> <b><font color='#ff0000'><span id='ipWarning'></span></font></b>
                    <b><font color='#ff0000'><span id='dnsWarning'></span></font></b>
                </div>
            </div>
        </div>
        <div id="WirelessSettings">
    		<h2>Wireless Settings</h2>
            <div class="container-fluid settingsTable settingsGroupTable">
                <div class="row" id="essidRow">
                    <div class="printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2"><div class="description">WPA SSID</div></div>
                    <div class="printSettingFieldCol col-md"><input list="eth_ssids" name="eth_ssid" id="eth_ssid" size="32" maxlength="32"><datalist id='eth_ssids'></datalist><input type="checkbox" name="eth_hidden" id="eth_hidden" value="Hidden">Hidden</div>
                </div>
                <div class="row" id="pskRow">
                    <div class="printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2"><div class="description">WPA Pre Shared key (PSK)</div></div>
                    <div class="printSettingFieldCol col-md"><input type="password" name="eth_psk" id="eth_psk" size="32" maxlength="64">&nbsp;<i class='fas fa-eye' id='eth_pskHideShow' onClick='TogglePasswordHideShow("eth_psk");'></i></div>
                </div>
            </div>
        </div>
<script>
function NoSaveSettingCallback() {
}
function dhcpServerEnabledCallback() {
    UpdatedhcpServerChildren(0);
}
</script>
<?php
PrintSettingGroup("advNetworkSettingsGroup", "", "", 1, "", "", true);
?>

              <br>
              <input name="btnSetInterface" type="button" style="" class = "buttons btn-success" value="Update Interface" onClick="SaveNetworkConfig();">
              <input id="btnConfigNetwork" type="button" style="display: none;" class = "buttons" value="Restart Network" onClick="ApplyNetworkConfig();">

            &nbsp; &nbsp; &nbsp;<input id="btnConfigNetworkPersist" type="button"  class = "buttons" value="Create Persistent Names" onClick="CreatePersistentNames();">
            &nbsp;<input id="btnConfigNetworkPersistClear" type="button" class = "buttons" value="Clear Persistent Names" onClick="ClearPersistentNames();">

            </div>
    </div>
    <div class="tab-pane fade" id="tab-host-dns" role="tabpanel" aria-labelledby="tab-host-dns-tab">


<?
PrintSettingGroup('host');
?>
            <h2>DNS Settings</h2>
            <div class="warning-text" id="dns_warning"></div>

            <div class="container-fluid settingsTable settingsGroupTable">
                <div class="row" id="dnsServerRow">
                    <div class="printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2"><div class="description">DNS Server Mode</div></div>
                    <div class="printSettingFieldCol col-md"><label><input type="radio" id ="dns_manual" value="manual">
                        Manual</label>
                      <label><input type="radio" id ="dns_dhcp" value="dhcp" checked>
                        DHCP</label></div>
                </div>
                <div class="row" id="dns1Row">
                    <div class="printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2"><div class="description">DNS Server 1</div></div>
                    <div class="printSettingFieldCol col-md"><input type="text" name="dns1" id="dns1"><input type="button" class="buttons" onClick='PingIP($("#dns1").val(), 3);' value='Ping'></div>
                </div>
                <div class="row" id="dns2Row">
                    <div class="printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2"><div class="description">DNS Server 2</div></div>
                    <div class="printSettingFieldCol col-md"><input type="text" name="dns2" id="dns2"><input type="button" class="buttons" onClick='PingIP($("#dns2").val(), 3);' value='Ping'></div>
                </div>
            </div>
              <br>
              <input name="btnSetDNS" type=""  class = "buttons btn-success" value="Update DNS" onClick="SaveDNSConfig();">



    </div>
    <div class="tab-pane fade" id="tab-tethering" role="tabpanel" aria-labelledby="tab-tethering-tab">

<?
PrintSettingGroup('tethering');
?>
                      <br>
                      <div class="callout callout-warning">
                      <h4>Warning:</h4> Turning on tethering may make FPP unavailable.   The WIFI adapter will be used for
              tethering and will thus not be usable for normal network operations.   The WIFI tether IP address will be
      192.168.8.1 for Hostapd tethering, but unpredictable for ConnMan (although likely 192.168.0.1).
                      </p>

      <p>
      <?if ($settings['Platform'] == "BeagleBone Black") {?>
          On BeagleBones, USB tethering is available.  The IP address for USB tethering would be 192.168.6.2
              (OSX/Linux) or 192.168.7.2 (Windows).
      <?}?>
      <?if ($settings['Platform'] == "Raspberry Pi") {?>
          On the Pi Zero and Pi Zero W devices, USB tethering is available if using an appropriate USB cable plugged into the USB port, not the power-only port.  Don't plug anything into the power port for this.  The IP address for USB tethering would be 192.168.7.2.
      <?}?>



                      </div>
                      <p>



    </div>
    <div class="tab-pane fade" id="tab-interface-routing" role="tabpanel" aria-labelledby="tab-interface-routing-tab">

      <h2>Interface Routing</h2>
          <?PrintSettingCheckbox("Enable Routing", "EnableRouting", 0, 0, "1", "0");?> Enable Routing between
                  network interfaces
    </div>
  </div>



    <div id="dialog-confirm" style="display: none">
    	<p><span class="ui-icon ui-icon-alert" style="flat:left; margin: 0 7px 20px 0;"></span>Reconfiguring the network will cause you to lose your connection and have to reconnect if you have changed the IP address.  Do you wish to proceed?</p>
    </div>
    <div id="dialog-clear-persistent" style="display: none">
    <p><span class="ui-icon ui-icon-alert" style="flat:left; margin: 0 7px 20px 0;"></span>Clearing out persistent device names can cause interfaces to use different configuration and become unavailable.  Do you wish to proceed?</p>
    </div>
    <div id="dialog-create-persistent" style="display: none">
        <p><span class="ui-icon ui-icon-alert" style="flat:left; margin: 0 7px 20px 0;"></span>Creating persisten device names can make it harder to add new network devices or replace existing devices in the future.  Do you wish to proceed?</p>
    </div>

</div>
<?php include 'common/footer.inc';?>
</div>
</div>
</body>
</html>
