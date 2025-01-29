<!DOCTYPE html>
<html lang="en">

<head>
    <?php
    include 'common/htmlMeta.inc';
    require_once 'common.php';
    include 'common/menuHead.inc'; ?>

    <title><? echo $pageTitle; ?></title>

    <script>
        var hiddenChildren = {};
        function UpdateChildSettingsVisibility() {
            hiddenChildren = {};
            $('.parentSetting').each(function () {
                var fn = 'Update' + $(this).attr('id') + 'Children';
                window[fn](2); // Hide if necessary
            });
            $('.parentSetting').each(function () {
                var fn = 'Update' + $(this).attr('id') + 'Children';
                window[fn](1); // Show if not hidden
            });
        }
        $(document).ready(function () {
            UpdateChildSettingsVisibility();
        });
    </script>
</head>

<body>


    <?php

    function PopulateInterfaces($uiLevel, $configDirectory)
    {

        $first = 1;
        $interfaces = network_list_interfaces_array();
        $interfaces = array_map(function ($value) {
            return preg_replace("/:$/", "", $value);
        }, $interfaces);

        foreach ($interfaces as $iface) {
            $ifaceChecked = $first ? " selected" : "";
            echo "<option value='" . $iface . "'" . $ifaceChecked . ">" . $iface . "</option>";
            $first = 0;
        }

        if ($uiLevel >= 1) {
            foreach (scandir($configDirectory) as $interface_file) {
                if (preg_match("/^interface\..*/", $interface_file)) {
                    $interface_file = preg_replace("/^interface\./", "", $interface_file);
                    if (array_search($interface_file, $interfaces) === false) {
                        echo "<option value='" . $interface_file . "'>" . $interface_file . " (not detected)</option>";
                    }
                }
            }
        }
    }

    ?>
    <script>

        function WirelessSettingsVisible(visible) {
            if (visible == true) {
                LoadSIDS($('#selInterfaces').val());
                $("#WirelessSettings").show();
            } else {
                $("#WirelessSettings").hide();
            }
        }

        function checkStaticIP() {
            var ip = $('#eth_ip').val();
            $("#ipWarning").html('');
            if (ip.startsWith("192.168") || ip.startsWith("10.")) {
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
                $.jGrowl(text, { themeState: 'danger' });
            }
        }

        function validateNetworkFields(callback) {
            var eth_ip = $('#eth_ip').val();
            $("#ipWarning").html('');
            if ($('#eth_static').is(':checked')) {
                if ((validateIPaddress('eth_ip') == false) || (eth_ip == "")) {
                    $.jGrowl("Invalid IP Address. Expect format like 192.168.0.101", { themeState: 'danger' });
                    $("#ipWarning").html('Invalid IP Address. Expect format like 192.168.0.101');
                    return false;
                }

                if ((validateIPaddress('eth_netmask') == false) || ($('#eth_netmask').val() == "")) {
                    $.jGrowl("Invalid Netmask. Expect format like 255.255.255.0", { themeState: 'danger' });
                    $("#ipWarning").html('Invalid Netmask. Expect format like 255.255.255.0');
                    return false;
                }
                if (validateIPaddress('eth_gateway') == false) {
                    $.jGrowl("Invalid Gateway. Expect format like 192.168.0.1", { themeState: 'danger' });
                    return false;
                }

                // Check if another interface already has a gateway set
                $.get("api/network/interface", function (interfaces) {
                    var gatewayAlreadySet = false;
                    var dhcpOperStateUp = false;

                    interfaces.forEach(function (ifaceData) {
                        // Ensure the config and INTERFACE properties are defined
                        if (ifaceData.config && ifaceData.config.INTERFACE) {

                            var iface = $('#selInterfaces').val();

                            // Exclude the interface we are trying to save
                            if (ifaceData.config.INTERFACE == iface) {
                                return;
                            }

                            // Exclude WLAN interfaces without an SSID
                            if (ifaceData.config.INTERFACE.startsWith('wl') && !ifaceData.config.SSID) {
                                return;
                            }

                            if (ifaceData.config.INTERFACE !== $('#selInterfaces').val() &&
                                ifaceData.config.PROTO === "static" &&
                                ifaceData.config.GATEWAY) {
                                gatewayAlreadySet = true;
                            }

                            // Check if there's a DHCP interface with operstate UP
                            if (ifaceData.config.PROTO === "dhcp" && ifaceData.operstate === "UP") {
                                dhcpOperStateUp = true;
                            }
                        }
                    });

                    <? if ($settings['uiLevel'] < 3) { ?>
                        if ((gatewayAlreadySet || dhcpOperStateUp) && $('#eth_gateway').val()) {
                            $.jGrowl("A gateway is already set on another interface, or another interface is configured as DHCP. You cannot set multiple gateways.", { themeState: 'danger' });
                            return callback(false);
                        }
                    <? } ?>

                    <? if ($settings['uiLevel'] < 1) { ?>
                        // If no gateway is set anywhere and we're in static mode, ensure one is set
                        if (!gatewayAlreadySet && !$('#eth_gateway').val() && !dhcpOperStateUp) {
                            $.jGrowl("You must set a gateway when using static IPs.", { themeState: 'danger' });
                            return callback(false);
                        }
                    <? } ?>

                    // If all checks passed, validation succeeds
                    return callback(true);

                }).fail(function () {
                    $.jGrowl("Failed to validate network interfaces.", { themeState: 'danger' });
                    return callback(false);
                });

            } else {
                // If not in static mode, validation passes
                return callback(true);
            }
        }

        function validateDNSFields() {
            setDNSWarning("");
            if (validateIPaddress('dns1') == false) {
                $.jGrowl("Invalid DNS Server #1", { themeState: 'danger' });
                setDNSWarning("Invalid DNS Server #1");
                return false;
            }
            if (validateIPaddress('dns2') == false) {
                $.jGrowl("Invalid DNS Server #2", { themeState: 'danger' });
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
            ).done(function (data) {
                LoadDNSConfig();
                $.jGrowl(" DNS configuration saved", { themeState: 'success' });
            }).fail(function () {
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

        function escapeHtml(unsafe) {
            return unsafe
                .replace(/&/g, "&amp;")
                .replace(/</g, "&lt;")
                .replace(/>/g, "&gt;")
                .replace(/"/g, "&quot;")
                .replace(/'/g, "&#039;");
        }

        function LoadSIDS(interface) {
            $("#wifisearch").show();
            $.get("api/network/wifi/scan/" + interface,
                {
                    timeout: 30000
                }
            ).done(function (data) {
                var ssids = [];
                data.networks.forEach(function (n) {
                    if (n.SSID != "") {
                        ssids.push(n.SSID);
                    }
                });
                ssids = ssids.filter(onlyUnique);
                ssids.sort();
                html = [];
                ssids.forEach(function (n) {
                    if (typeof (n) != "undefined") {
                        html.push("<option value='");
                        html.push(escapeHtml(n));
                        html.push("'>");
                    }
                });
                $("#eth_ssids").html(html.join(''));
            }).fail(function () {
                DialogError("Scan Failed", "Failed to Scan for wifi networks");
            }).always(function () {
                $("#wifisearch").hide();
            });
        }

        function LoadDNSConfig() {
            var url = "api/network/dns";
            $.get(url, GetDNSInfo);
        }

        function ApplyNetworkConfig() {
            DoModalDialog({
                id: "applyNetworkConfirm",
                title: "Apply Network Changes",
                body: $("#dialog-confirm"),
                class: 'modal-lg',
                backdrop: true,
                keyboard: true,
                buttons: {
                    "Yes": {
                        class: 'btn-success', click: function () {
                            CloseModalDialog("applyNetworkConfirm");
                            $.post("api/network/interface/" + $('#selInterfaces').val() + "/apply");
                        }
                    },
                    "Cancel and apply at next reboot": {
                        click: function () {
                            CloseModalDialog("applyNetworkConfirm");
                        }
                    }
                }
            });
        }

        function SaveNetworkConfig() {
            validateNetworkFields(function (isValid) {
                if (!isValid) {
                    DialogError("Invalid Network Config", "Save Failed");
                    return;
                }

                var iface = $('#selInterfaces').val();
                var url;
                var data = {};
                data.INTERFACE = iface;
                if ($('#eth_static').is(':checked')) {
                    data.PROTO = 'static';
                    data.ADDRESS = $('#eth_ip').val();
                    data.NETMASK = $('#eth_netmask').val();
                    data.GATEWAY = $('#eth_gateway').val();
                } else {
                    data.PROTO = 'dhcp';
                }
                <? if ($settings['uiLevel'] >= 1) { ?>
                    data.ROUTEMETRIC = $('#routeMetric').val();
                    data.DHCPSERVER = $('#dhcpServer').is(':checked');
                    data.DHCPOFFSET = $('#dhcpOffset').val();
                    data.DHCPPOOLSIZE = $('#dhcpPoolSize').val();
                    data.IPFORWARDING = $('#ipForwarding').val();
                <? } ?>

                if (iface.substr(0, 2) == "wl") {
                    data.SSID = $('#eth_ssid').val();
                    data.PSK = $('#eth_psk').val();
                    data.HIDDEN = $('#eth_hidden').is(':checked');
                    data.WPA3 = $('#eth_wpa3').is(':checked');
                    data.BACKUPSSID = $('#backupeth_ssid').val();
                    data.BACKUPPSK = $('#backupeth_psk').val();
                    data.BACKUPHIDDEN = $('#backupeth_hidden').is(':checked');
                    data.BACKUPWPA3 = $('#backupeth_wpa3').is(':checked');
                }

                data.Leases = {};
                $('#staticLeasesTable > tbody > tr').each(function () {
                    var checkBox = $(this).find('#static_enabled');
                    if (checkBox.is(":checked")) {
                        var ip = $(this).find('#static_ip').val();
                        var mac = $(this).find('#static_mac').val();
                        data.Leases[ip] = mac;
                    }
                })

                var postData = JSON.stringify(data);

                $.post("api/network/interface/" + iface, postData
                ).done(function (rc) {
                    if (rc.status == "OK") {
                        LoadNetworkConfig();
                        $.jGrowl(iface + " network interface configuration saved", { themeState: 'success' });
                        $('#btnConfigNetwork').show();

                        if (data.PROTO == 'static' && $('#dns1').val() == "" && $('#dns2').val() == "") {
                            DialogError("Check DNS", "Don't forget to set a DNS IP address. You may use 8.8.8.8 or 1.1.1.1 if you are not sure.<br><br<span style='color: red;'>If you do not do this, your FPP will have no Internet Access</span>");
                            return;
                        }
                    } else {
                        DialogError("Save Network Config", "Save Failed: " + rc.status);
                    }
                }).fail(function () {
                    DialogError("Save Network Config", "Save Failed");
                });

                SaveDNSConfig();
            });
        }

        function AddInterface() {
            DoModalDialog({
                id: "addNewInterfaceDialog",
                title: "Add New Interface",
                body: $('#dialog-addinterface'),
                class: 'modal-m',
                backdrop: true,
                keyboard: true,
                buttons: {
                    "OK": {
                        class: 'btn-success', click: function () {
                            CloseModalDialog("addNewInterfaceDialog");
                            var newInterfaceName = $("#addNewInterfaceDialog").find(".newInterfaceName").val();
                            $.get("api/network/interface/add/" + newInterfaceName, "", function () { location.reload(true); });
                        }
                    },
                    "Cancel": {
                        click: function () {
                            CloseModalDialog("addNewInterfaceDialog");
                        }
                    }
                }
            });
        }

        function CreatePersistentNames() {
            DoModalDialog({
                id: "createPersistentDialog",
                title: "Create Persistent Names",
                body: $("#dialog-create-persistent"),
                class: 'modal-m',
                backdrop: true,
                keyboard: true,
                buttons: {
                    "Yes": {
                        class: 'btn-success', click: function () {
                            CloseModalDialog("createPersistentDialog");
                            SetRebootFlag();
                            $.post("api/network/persistentNames", "", function () { location.reload(true); });
                        }
                    },
                    "No": {
                        click: function () {
                            CloseModalDialog("createPersistentDialog");
                        }
                    }
                }
            });
        }
        function ClearPersistentNames() {
            DoModalDialog({
                id: "clearPersistentDialog",
                title: "Clear Persistent Names",
                body: $("#dialog-clear-persistent"),
                class: 'modal-m',
                backdrop: true,
                keyboard: true,
                buttons: {
                    "Yes": {
                        class: 'btn-success', click: function () {
                            CloseModalDialog("clearPersistentDialog");
                            SetRebootFlag();
                            $.ajax({
                                type: "DELETE",
                                url: "api/network/persistentNames",
                                data: "",
                                success: function () { location.reload(true); },
                            });
                        }
                    },
                    "No": {
                        click: function () {
                            CloseModalDialog("clearPersistentDialog");
                        }
                    }
                }
            });
        }

        function LoadNetworkConfig() {
            var iface = $('#selInterfaces').val();
            var url = "api/network/interface/" + iface;
            var visible = iface.slice(0, 2).toLowerCase() == "wl" ? true : false;

            $.get(url, GetInterfaceInfo);
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

            data.forEach(function (i) {
                if (i.ifname.startsWith(iface)) {
                    if (i.hasOwnProperty("config")) {
                        if (i.config.PROTO == "static") {
                            if (($('#eth_static').is(':checked')) &&
                                ($('#dns_dhcp').is(':checked'))) {
                                setDNSWarning("Warning: You must manually configure your DNS Server(s) if all network interfaces use static IPs.");
                            } else {
                                setDNSWarning("");
                            }
                        } else {
                            setDNSWarning("");
                        }
                    }
                }
            });
        }

        function CheckDNS() {
            var url = "api/network/interface";

            $.get(url, CheckDNSCallback);
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

        $(document).ready(function () {

            LoadNetworkConfig();
            LoadDNSConfig();

            $("#dns1").on("change", validateDNSFields);
            $("#dns2").on("change", validateDNSFields);

            $("#eth_static").on("click", function () {
                DisableNetworkFields(false);
                $('#eth_dhcp').prop('checked', false);

                $('#dns_dhcp').prop('checked', false);
                $('#dns_manual').prop('checked', true);
                DisableDNSFields(false);
            });

            $("#eth_dhcp").on("click", function () {
                DisableNetworkFields(true);
                $('#eth_static').prop('checked', false);
                $('#eth_ip').val("");
                $('#eth_netmask').val("");
                $('#eth_gateway').val("");
                <? if ($settings['uiLevel'] >= 1) { ?>
                    $('#dhcpServer').prop('checked', false);
                    UpdateChildSettingsVisibility();
                <? } ?>
            });

            $("#dns_manual").on("click", function () {
                DisableDNSFields(false);
                $('#dns_dhcp').prop('checked', false);
            });

            $("#dns_dhcp").on("click", function () {
                DisableDNSFields(true);
                $('#dns_manual').prop('checked', false);
                $('#dns1').val("");
                $('#dns2').val("");
            });
        });

        function GetInterfaceInfo(data, status) {
            if (data.PROTO == "static") {
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

            if (data.INTERFACE && data.INTERFACE.substr(0, 2) == "wl") {
                $('#eth_ssid').val(data.SSID);
                $('#eth_psk').val(data.PSK);
                if (data.HIDDEN == "1") {
                    $('#eth_hidden').prop('checked', true);
                } else {
                    $('#eth_hidden').prop('checked', false);
                }
                if (data.WPA3 == "1") {
                    $('#eth_wpa3').prop('checked', true);
                } else {
                    $('#eth_wpa3').prop('checked', false);
                }
                $('#backupeth_ssid').val(data.BACKUPSSID);
                $('#backupeth_psk').val(data.BACKUPPSK);
                if (data.BACKUPHIDDEN == "1") {
                    $('#backupeth_hidden').prop('checked', true);
                } else {
                    $('#backupeth_hidden').prop('checked', false);
                }
                if (data.BACKUPWPA3 == "1") {
                    $('#backupeth_wpa3').prop('checked', true);
                } else {
                    $('#backupeth_wpa3').prop('checked', false);
                }
            }
            <? if ($settings['uiLevel'] >= 1) { ?>
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

                $("#staticLeasesTable > tbody").empty();
                if (data.DHCPSERVER && (data.DHCPSERVER == "1")) {
                    var leases = data.StaticLeases;
                    for (ip in leases) {
                        var mac = leases[ip];
                        var tr = "<tr><td><input type='checkbox' id='static_enabled' checked></td>";
                        tr += "<td><input type='text' id='static_ip' value='" + ip + "' size='16'></td>";
                        tr += "<td><input type='hidden' id='static_mac' value='" + mac + "'/>" + mac + "</td></tr>";
                        $("#staticLeasesTable > tbody").append(tr);
                    }
                    var leases = data.CurrentLeases;
                    for (ip in leases) {
                        var mac = leases[ip];
                        var tr = "<tr><td><input type='checkbox' id='static_enabled'></td>";
                        tr += "<td><input type='text' id='static_ip' value='" + ip + "' size='16'></td>";
                        tr += "<td><input type='hidden' id='static_mac' value='" + mac + "'/>" + mac + "</td></tr>";
                        $("#staticLeasesTable > tbody").append(tr);
                    }
                    $("#staticLeases").show();
                } else {
                    $("#staticLeases").hide();
                }

                UpdateChildSettingsVisibility();
            <? } else { ?>
                $("#staticLeases").hide();
            <? } ?>
            CheckDNS();
        }

        function DisableNetworkFields(disabled) {
            $('#eth_ip').prop("disabled", disabled);
            $('#eth_netmask').prop("disabled", disabled);
            $('#eth_gateway').prop("disabled", disabled);
            <? if ($settings['uiLevel'] >= 1) { ?>
                $('#dhcpServer').prop('disabled', disabled);
            <? } ?>
        }

        function DisableDNSFields(disabled) {
            $('#dns1').prop("disabled", disabled);
            $('#dns2').prop("disabled", disabled);
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
            ).done(function () {
                $.jGrowl("HostName Saved", { themeState: 'success' });
                SetRebootFlag();
            }).fail(function () {
                DialogError("Save HostName", "Save Failed");
            });
        }

        function setHostDescription() {
            $.put("api/settings/HostDescription", $('#hostDescription').val()
            ).done(function () {
                $.jGrowl("HostDescription Saved", { themeState: 'success' });
                SetRestartFlag(2);
            }).fail(function () {
                DialogError("Save HostDescription", "Save Failed");
            });
        }

    </script>
    <div id="bodyWrapper">
        <?php
        $activeParentMenuItem = 'status';
        include 'menu.inc'; ?>
        <div class="mainContainer">
            <h1 class="title">Network Configuration</h1>
            <div class="pageContent">
                <ul class="nav nav-pills pageContent-tabs" role="tablist">
                    <li class="nav-item">
                        <a class="nav-link active" id="interface-settings-tab" data-bs-toggle="tab"
                            data-bs-target="#tab-interface-settings" href="#tab-interface-settings" role="tab"
                            aria-controls="interface-settings" aria-selected="true">
                            Interface Settings
                        </a>
                    </li>
                    <li class="nav-item">
                        <a class="nav-link" id="tab-host-dns-tab" data-bs-toggle="tab" data-bs-target="#tab-host-dns"
                            href="#tab-host-dns" role="tab" aria-controls="tab-host-dns" aria-selected="false">
                            Host Settings
                        </a>
                    </li>
                    <li class="nav-item">
                        <a class="nav-link" id="tab-tethering-tab" data-bs-toggle="tab" data-bs-target="#tab-tethering"
                            href="#tab-tethering" role="tab" aria-controls="tab-tethering" aria-selected="false">
                            Tethering
                        </a>
                    </li>
                </ul>
                <div class="tab-content">
                    <div class="tab-pane fade show active" id="tab-interface-settings" role="tabpanel"
                        aria-labelledby="interface-settings-tab">
                        <div id="InterfaceSettings">

                            <?php
                            if (file_exists("/etc/modprobe.d/wifi-disable-power-management.conf")) {
                                PrintSettingGroup("wifiDriversGroup");
                            } else {
                                PrintSettingGroup("wifiDomainGroup");
                            }
                            ?>
                            <h2> Interface Settings</h2>
                            <div class="container-fluid settingsTable settingsGroupTable">
                                <div class="row">
                                    <div class="printSettingLabelCol col-md-8">Select an interface name to configure the
                                        network information for that interface.</div>
                                    <div class="warning-text" style="display: none;" id=wifisearch>Performing Wifi
                                        Scan...</div>
                                </div>
                                <div class="row" id="selInterfacesRow">
                                    <div class="printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2">
                                        <div class="description">Interface Name</div>
                                    </div>
                                    <div class="printSettingFieldCol col-md"><select id="selInterfaces"
                                            class='multiSelect' size='3' style="width:13em;"
                                            onChange='LoadNetworkConfig();'><?php PopulateInterfaces($uiLevel, $configDirectory); ?></select>
                                    </div>
                                </div>
                                <div class="row" id="interfaceConfigTypeRow">
                                    <div class="printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2">
                                        <div class="description">Interface Mode</div>
                                    </div>
                                    <div class="printSettingFieldCol col-md"><label><input type="radio" id="eth_static"
                                                value="static">
                                            Static</label>
                                        <label><input type="radio" id="eth_dhcp" value="dhcp">
                                            DHCP</label>
                                    </div>
                                </div>
                                <div class="row" id="ipAddressRow">
                                    <div class="printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2">
                                        <div class="description">IP Address</div>
                                    </div>
                                    <div class="printSettingFieldCol col-md"><input type="text" name="eth_ip"
                                            id="eth_ip" size=15 maxlength=15 onChange="checkStaticIP();">
                                        <input type="button" class="buttons" onClick='PingIP($("#eth_ip").val(), 3);'
                                            value='Ping'>
                                    </div>
                                </div>
                                <div class="row" id="netmaskRow">
                                    <div class="printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2">
                                        <div class="description">Netmask</div>
                                    </div>
                                    <div class="printSettingFieldCol col-md"><input type="text" name="eth_netmask"
                                            id="eth_netmask" size="15" maxlength="15"></div>
                                </div>
                                <div class="row" id="gatewayRow">
                                    <div class="printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2">
                                        <div class="description">Gateway</div>
                                    </div>
                                    <div class="printSettingFieldCol col-md"><input type="text" name="eth_gateway"
                                            id="eth_gateway" size="15" maxlength="15">
                                        <input type="button" class="buttons"
                                            onClick='PingIP($("#eth_gateway").val(), 3);' value='Ping'>
                                    </div>
                                </div>
                                <div class="row">
                                    <div class="col-md-6"> <b>
                                            <font color='#ff0000'><span id='ipWarning'></span></font>
                                        </b>
                                        <b>
                                            <font color='#ff0000'><span id='dnsWarning'></span></font>
                                        </b>
                                    </div>
                                </div>
                            </div>
                            <h2>DNS Settings</h2>
                            <div class="warning-text" id="dns_warning"></div>

                            <div class="container-fluid settingsTable settingsGroupTable">
                                <div class="row" id="dnsServerRow">
                                    <div class="printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2">
                                        <div class="description">DNS Server Mode</div>
                                    </div>
                                    <div class="printSettingFieldCol col-md"><label><input type="radio" id="dns_manual"
                                                value="manual">
                                            Manual</label>
                                        <label><input type="radio" id="dns_dhcp" value="dhcp" checked>
                                            DHCP</label>
                                    </div>
                                </div>
                                <div class="row" id="dns1Row">
                                    <div class="printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2">
                                        <div class="description">DNS Server 1</div>
                                    </div>
                                    <div class="printSettingFieldCol col-md"><input type="text" name="dns1"
                                            id="dns1"><input type="button" class="buttons"
                                            onClick='PingIP($("#dns1").val(), 3);' value='Ping'></div>
                                </div>
                                <div class="row" id="dns2Row">
                                    <div class="printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2">
                                        <div class="description">DNS Server 2</div>
                                    </div>
                                    <div class="printSettingFieldCol col-md"><input type="text" name="dns2"
                                            id="dns2"><input type="button" class="buttons"
                                            onClick='PingIP($("#dns2").val(), 3);' value='Ping'></div>
                                </div>
                            </div>
                            <div id="WirelessSettings">
                                <h2>Wireless Settings</h2>
                                <div class="container-fluid settingsTable settingsGroupTable">
                                    <div class="row" id="essidRow">
                                        <div class="printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2">
                                            <div class="description">WPA SSID</div>
                                        </div>
                                        <div class="printSettingFieldCol col-md"><input list="eth_ssids" name="eth_ssid"
                                                id="eth_ssid" size="32" maxlength="32"><datalist
                                                id='eth_ssids'></datalist><input type="checkbox" name="eth_hidden"
                                                id="eth_hidden" value="Hidden">Hidden&nbsp;<input type="checkbox"
                                                name="eth_wpa3" id="eth_wpa3" value="WPA3">WPA3</div>
                                    </div>
                                    <div class="row" id="pskRow">
                                        <div class="printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2">
                                            <div class="description">WPA Pre Shared key (PSK)</div>
                                        </div>
                                        <div class="printSettingFieldCol col-md"><input type="password" name="eth_psk"
                                                id="eth_psk" size="32" maxlength="64">&nbsp;<i class='fas fa-eye'
                                                id='eth_pskHideShow' onClick='TogglePasswordHideShow("eth_psk");'></i>
                                        </div>
                                    </div>
                                    <? if ($settings['uiLevel'] >= 1) { ?>
                                        <div class="row" id="backupessidRow">
                                            <div class="printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2">
                                                <div class="description">Backup WPA SSID</div>
                                            </div>
                                            <div class="printSettingFieldCol col-md"><input list="eth_ssids"
                                                    name="backupeth_ssid" id="backupeth_ssid" size="32"
                                                    maxlength="32"><datalist id='eth_ssids'></datalist><input
                                                    type="checkbox" name="backupeth_hidden" id="backupeth_hidden"
                                                    value="Hidden">Hidden&nbsp;<input type="checkbox" name="backupeth_wpa3"
                                                    id="backupeth_wpa3" value="BACKUPWPA3">WPA3</div>
                                        </div>
                                        <div class="row" id="backuppskRow">
                                            <div class="printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2">
                                                <div class="description">Backup WPA Pre Shared key (PSK)</div>
                                            </div>
                                            <div class="printSettingFieldCol col-md"><input type="password"
                                                    name="backupeth_psk" id="backupeth_psk" size="32"
                                                    maxlength="64">&nbsp;<i class='fas fa-eye' id='backupeth_pskHideShow'
                                                    onClick='TogglePasswordHideShow("backupeth_psk");'></i></div>
                                        </div>
                                    <? } ?>
                                </div>
                            </div>
                            <script>
                                function NoSaveSettingCallback() {
                                }
                                function dhcpServerEnabledCallback() {
                                    UpdatedhcpServerChildren(0);
                                    if ($('#dhcpServer').is(':checked')) {
                                        $("#staticLeases").show();
                                    } else {
                                        $("#staticLeases").hide();
                                    }
                                }
                            </script>
                            <?php
                            PrintSettingGroup("advNetworkSettingsGroup", "", "", 1, "", "", true);
                            ?>
                            <div id="staticLeases">
                                <h3>Static Leases</h3>
                                <div class="fppTableWrapper">
                                    <div class='fppTableContents' role="region" aria-labelledby="staticLeasesTable"
                                        tabindex="0">
                                        <table id="staticLeasesTable" class="fppSelectableRowTable"
                                            style="width:500px;">
                                            <thead>
                                                <tr>
                                                    <th>Enable</td>
                                                    <th>IP</td>
                                                    <th>MAC Address</td>
                                                </tr>
                                            </thead>
                                            <tbody>
                                            </tbody>
                                        </table>
                                    </div>
                                </div>
                            </div>
                            <br>
                            <input name="btnSetInterface" type="button" style="" class="buttons btn-success"
                                value="Update" onClick="SaveNetworkConfig();">
                            <input id="btnConfigNetwork" type="button" style="display: none;" class="buttons"
                                value="Restart Network" onClick="ApplyNetworkConfig();">

                            &nbsp; &nbsp; &nbsp;
                            <? if ($settings['uiLevel'] >= 1) { ?>
                                <input name="btnAddInterface" type="button" style="" class="buttons"
                                    value="Add New Interface" onClick="AddInterface();">
                            <? } ?>
                            &nbsp;<input id="btnConfigNetworkPersist" type="button" class="buttons"
                                value="Create Persistent Names" onClick="CreatePersistentNames();">
                            &nbsp;<input id="btnConfigNetworkPersistClear" type="button" class="buttons"
                                value="Clear Persistent Names" onClick="ClearPersistentNames();">

                        </div>
                    </div>
                    <div class="tab-pane fade" id="tab-host-dns" role="tabpanel" aria-labelledby="tab-host-dns-tab">


                        <?
                        PrintSettingGroup('host');
                        ?>



                    </div>
                    <div class="tab-pane fade" id="tab-tethering" role="tabpanel" aria-labelledby="tab-tethering-tab">

                        <?
                        PrintSettingGroup('tethering');
                        ?>
                        <br>
                        <div class="callout callout-warning">
                            <h4>Warning:</h4> Turning on tethering may make FPP unavailable. The WIFI adapter will be
                            used for tethering and will thus not be usable for normal network operations. The WIFI
                            tether IP
                            address will be 192.168.8.1.
                            </p>

                            <p>
                                <? if ($settings['Platform'] == "BeagleBone Black" || $settings['Platform'] == "BeagleBone 64") { ?>
                                    On BeagleBones, USB tethering is available. The IP address for USB tethering would be
                                    192.168.6.2 (OSX/Linux) or 192.168.7.2 (Windows).
                                <? } ?>
                                <? if ($settings['Platform'] == "Raspberry Pi") { ?>
                                    On the Pi Zero and Pi Zero W devices, USB tethering is available if using an appropriate
                                    USB cable plugged into the USB port, not the power-only port. Don't plug anything into
                                    the power port for this. The IP address for USB tethering would be 192.168.7.2.
                                <? } ?>

                        </div>
                        <p>



                    </div>
                </div>



                <div id="dialog-confirm" class="hidden">
                    <p><span class="ui-icon ui-icon-alert" style="flat:left; margin: 0 7px 20px 0;"></span>Reconfiguring
                        the network will cause you to lose your connection and have to reconnect if you have changed the
                        IP address. Do you wish to proceed?</p>
                </div>
                <div id="dialog-clear-persistent" class="hidden">
                    <p><span class="ui-icon ui-icon-alert" style="flat:left; margin: 0 7px 20px 0;"></span>Clearing out
                        persistent device names can cause interfaces to use different configuration and become
                        unavailable. Do you wish to proceed?</p>
                </div>
                <div id="dialog-create-persistent" class="hidden">
                    <p><span class="ui-icon ui-icon-alert" style="flat:left; margin: 0 7px 20px 0;"></span>Creating
                        persistent device names can make it harder to add new network devices or replace existing
                        devices in the future. Do you wish to proceed?</p>
                </div>
                <div id="dialog-addinterface" title="Add New Interface" class="hidden">
                    <span>Enter name for the new interface (eg wlan0 or eth0 etc):</span>
                    <input name="newInterfaceName" type="text" style="z-index:10000; width: 95%"
                        class="newInterfaceName" value="">
                </div>

            </div>
            <?php include 'common/footer.inc'; ?>
        </div>
    </div>
</body>

</html>