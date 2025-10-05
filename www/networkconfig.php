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
                if (typeof window[fn] === 'function') {
                    window[fn](2); // Hide if necessary
                }
            });
            $('.parentSetting').each(function () {
                var fn = 'Update' + $(this).attr('id') + 'Children';
                if (typeof window[fn] === 'function') {
                    window[fn](1); // Show if not hidden
                }
            });
        }
        $(document).ready(function () {
            UpdateChildSettingsVisibility();

            // Initialize tooltips when page loads
            setTimeout(function () {
                initializeTooltips();
            }, 500);

            // Initialize tooltips when main tabs are shown
            $('a[data-bs-toggle="tab"]').on('shown.bs.tab', function (e) {
                var targetTab = $(e.target.getAttribute('data-bs-target'));
                initializeTooltips(targetTab);
            });
        });

        function initializeTooltips(container) {
            // If no container specified, initialize for entire document
            var selector = container ? container.find('[data-bs-toggle="tooltip"]') : $('[data-bs-toggle="tooltip"]');
            
            selector.each(function() {
                var element = this;
                // Dispose existing tooltip if any
                var existingTooltip = bootstrap.Tooltip.getInstance(element);
                if (existingTooltip) {
                    existingTooltip.dispose();
                }
                // Create new tooltip
                new bootstrap.Tooltip(element);
            });
        }
    </script>

    <style>
        #ipAddressRow,
        #netmaskRow {
            transition: opacity 0.3s ease-in-out;
        }
        
        /* WiFi Signal Strength Styling */
        .wifi-signal.excellent { color: #28a745; }
        .wifi-signal.good { color: #fd7e14; }
        .wifi-signal.fair { color: #dc3545; }
        .wifi-signal.poor { color: #6c757d; }
        
        /* Security Badge Styling */
        .security-badge {
            display: inline-block;
            padding: 2px 6px;
            border-radius: 3px;
            font-size: 11px;
            font-weight: bold;
            text-transform: uppercase;
        }
        
        .security-none {
            background-color: #dc3545;
            color: white;
        }
        
        .security-wep {
            background-color: #fd7e14;
            color: white;
        }
        
        .security-wpa {
            background-color: #198754;
            color: white;
        }
        
        .security-wpa2 {
            background-color: #0d6efd;
            color: white;
        }
        
        .security-wpa3 {
            background-color: #6f42c1;
            color: white;
        }
        
        /* WiFi Network Row Hover Effects */
        .wifi-network-row:hover {
            background-color: #f5f5f5;
        }
        
        .wifi-network-row:active {
            background-color: #e9ecef;
        }
    </style>
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
                $("#WirelessSubsection").show();
                $("#WirelessSettings").show();
                $("#wifiNetworksRow").show();
            } else {
                $("#WirelessSubsection").hide();
                $("#WirelessSettings").hide();
                $("#wifiNetworksRow").hide();
            }
        }

        function checkStaticIP() {
            var ip = $('#eth_ip').val();
            $("#ipWarning").html('');
            if (ip.startsWith("192.168") || ip.startsWith("10.")) {
                if ($('#eth_netmask').val() == "") {
                    $('#eth_netmask').val("255.255.255.0");
                }
                // Note: Gateway is now handled globally, not per-interface
            }
            if (ip.startsWith("192.168.6.") || ip.startsWith("192.168.7.") || ip.startsWith("192.168.8.")) {
                var text = "It is recommended to use subnets other than 192.168.6.x, 192.168.7.x, and 192.168.8.x to avoid issues with tethering."
                $("#ipWarning").html(text);
                $.jGrowl(text, { themeState: 'danger' });
            }
        }

        function validateGlobalGateway(callback) {
            var gateway = $('#global_gateway').val();

            // Empty gateway is acceptable - it means no manual gateway is set
            if (gateway === "" || gateway === null || gateway === undefined) {
                return callback(true);
            }

            // If gateway is provided, validate it's a proper IP
            if (gateway !== "" && validateIPaddress('global_gateway') == false) {
                $.jGrowl("Invalid Gateway. Expect format like 192.168.0.1", { themeState: 'danger' });
                return callback(false);
            }

            // Check if any interface has DHCP enabled and is UP
            $.get("api/network/interface", function (interfaces) {
                var dhcpOperStateUp = false;

                interfaces.forEach(function (ifaceData) {
                    if (ifaceData.config && ifaceData.config.INTERFACE) {
                        // Check if there's a DHCP interface with operstate UP
                        if (ifaceData.config.PROTO === "dhcp" && ifaceData.operstate === "UP") {
                            dhcpOperStateUp = true;
                        }
                    }
                });

                // If DHCP is providing a route and user tries to set manual gateway, warn them
                if (dhcpOperStateUp && gateway !== "") {
                    $.jGrowl("Warning: A DHCP interface is active and may provide its own default route. Manual gateway may conflict.", { themeState: 'warning' });
                }

                return callback(true);

            }).fail(function () {
                // If the API call fails, still allow the gateway to be saved
                return callback(true);
            });
        }

        function validateNetworkFields(callback) {
            // Use current interface for interface-specific validation
            var iface = currentInterface;
            if (!iface) {
                callback(false);
                return;
            }
            var safeName = iface.replace(/[^a-zA-Z0-9]/g, '_');
            
            var eth_ip = $('#eth_ip_' + safeName).val();
            $("#ipWarning").html('');
            if ($('#eth_static_' + safeName).is(':checked')) {
                if ((validateIPaddress('eth_ip_' + safeName) == false) || (eth_ip == "")) {
                    $.jGrowl("Invalid IP Address. Expect format like 192.168.0.101", { themeState: 'danger' });
                    $("#ipWarning").html('Invalid IP Address. Expect format like 192.168.0.101');
                    return false;
                }

                if ((validateIPaddress('eth_netmask_' + safeName) == false) || ($('#eth_netmask_' + safeName).val() == "")) {
                    $.jGrowl("Invalid Netmask. Expect format like 255.255.255.0", { themeState: 'danger' });
                    $("#ipWarning").html('Invalid Netmask. Expect format like 255.255.255.0');
                    return false;
                }

                // Gateway validation is now handled globally, not per interface
                return callback(true);
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

        function SaveGlobalGateway() {
            // Don't save if the gateway field is disabled
            if ($('#global_gateway').prop('disabled')) {
                $.jGrowl("Gateway is disabled because all interfaces use DHCP", { themeState: 'info' });
                return;
            }

            var gateway = $('#global_gateway').val();

            // If gateway is not empty, validate it first
            if (gateway !== "" && !validateIPaddress('global_gateway')) {
                $.jGrowl("Invalid Gateway. Expect format like 192.168.0.1", { themeState: 'danger' });
                return;
            }

            // Check if gateway value has actually changed
            $.get("api/network/gateway", function (currentData) {
                var currentGateway = currentData.GATEWAY || "";
                var newGateway = gateway || "";

                // Only save and show notification if value changed
                if (currentGateway !== newGateway) {
                    var data = {};
                    data.GATEWAY = newGateway;

                    var postData = JSON.stringify(data);

                    $.post("api/network/gateway", postData
                    ).done(function (data) {
                        LoadGlobalGateway();
                        $.jGrowl("Global gateway configuration saved", { themeState: 'success' });
                    }).fail(function (xhr, status, error) {
                        DialogError("Save Global Gateway", "Save Failed: " + error);
                    });
                } else {
                    // No change, just reload to ensure UI is in sync
                    LoadGlobalGateway();
                }
            }).fail(function (xhr, status, error) {
                DialogError("Save Global Gateway", "Save Failed: " + error);
            });
        }

        function LoadGlobalGateway() {
            var url = "api/network/gateway";
            $.get(url, function (data) {
                $('#global_gateway').val(data.GATEWAY || '');
                checkGatewayAvailability(); // Check if gateway should be enabled
            }).fail(function () {
                // If the API fails, just set empty and check availability
                $('#global_gateway').val('');
                checkGatewayAvailability(); // Check if gateway should be enabled
            });
        }

        function checkGatewayAvailability() {
            // First check the current UI state for the selected interface
            var currentIface = currentInterface;
            if (!currentIface) return;
            
            var safeName = currentIface.replace(/[^a-zA-Z0-9]/g, '_');
            var currentIsStatic = $('#eth_static_' + safeName).is(':checked');

            // Then check the saved state of other interfaces
            $.get("api/network/interface", function (interfaces) {
                var hasStaticInterface = false;
                var hasDHCPInterface = false;

                interfaces.forEach(function (ifaceData) {
                    if (ifaceData.config && ifaceData.config.INTERFACE) {
                        // Exclude WLAN interfaces without an SSID
                        if (ifaceData.config.INTERFACE.startsWith('wl') && !ifaceData.config.SSID) {
                            return;
                        }

                        // For the currently selected interface, use UI state instead of saved state
                        if (ifaceData.config.INTERFACE === currentIface) {
                            if (currentIsStatic) {
                                hasStaticInterface = true;
                            } else {
                                hasDHCPInterface = true;
                            }
                        } else {
                            // For other interfaces, use saved state
                            if (ifaceData.config.PROTO === "static") {
                                hasStaticInterface = true;
                            } else if (ifaceData.config.PROTO === "dhcp") {
                                hasDHCPInterface = true;
                            }
                        }
                    }
                });

                // If all interfaces are DHCP and none are static, disable gateway
                if (hasDHCPInterface && !hasStaticInterface) {
                    $('#global_gateway').prop('disabled', true);
                    $('#global_gateway').val('');
                    $('#global_gateway').attr('placeholder', 'Disabled - DHCP interfaces provide routing');
                    $('#global_gateway').next('.buttons').prop('disabled', true); // Disable ping button
                    $('#global_gateway').siblings('.btn-success').prop('disabled', true); // Disable update button
                    $('#globalGatewayRow .description-text').html(
                        '<span style="color: #666;">Gateway disabled because all interfaces use DHCP. DHCP will provide default routing.</span>'
                    );
                } else {
                    $('#global_gateway').prop('disabled', false);
                    $('#global_gateway').attr('placeholder', '');
                    $('#global_gateway').next('.buttons').prop('disabled', false); // Enable ping button
                    $('#global_gateway').siblings('.btn-success').prop('disabled', false); // Enable update button
                    $('#globalGatewayRow .description-text').html(
                        'Global gateway for all interfaces. Leave blank if using DHCP on any interface for routing.'
                    );
                }
            }).fail(function () {
                // If we can't check interfaces, enable the gateway field
                $('#global_gateway').prop('disabled', false);
            });
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

            // Check if DNS values have actually changed
            $.get("api/network/dns", function (currentData) {
                var currentDNS1 = currentData.DNS1 || "";
                var currentDNS2 = currentData.DNS2 || "";
                var newDNS1 = data.DNS1 || "";
                var newDNS2 = data.DNS2 || "";

                // Only save and show notification if values changed
                if (currentDNS1 !== newDNS1 || currentDNS2 !== newDNS2) {
                    var postData = JSON.stringify(data);

                    $.post("api/network/dns", postData
                    ).done(function (data) {
                        LoadDNSConfig();
                        $.jGrowl(" DNS configuration saved", { themeState: 'success' });
                    }).fail(function () {
                        DialogError("Save DNS Config", "Save Failed");
                    });
                } else {
                    // No change, just reload to ensure UI is in sync
                    LoadDNSConfig();
                }
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
            $("#wifiNetworksList").empty();

            $.get("api/network/wifi/scan/" + interface,
                {
                    timeout: 30000
                }
            ).done(function (data) {
                var ssids = [];
                var networksHtml = [];
                var currentSSID = $('#eth_ssid').val();
                var uniqueNetworks = {};

                // Process networks and keep only the strongest signal for each SSID
                data.networks.forEach(function (n) {
                    if (n.SSID && n.SSID.trim() !== "") {
                        var ssid = n.SSID.trim();
                        var signal = parseInt(n.signal) || -100;

                        // Only keep this network if it's the first occurrence or has better signal
                        if (!uniqueNetworks[ssid] || signal > uniqueNetworks[ssid].signal) {
                            uniqueNetworks[ssid] = {
                                SSID: ssid,
                                signal: signal,
                                encrypted: n.encrypted || false,
                                security: n.security || '',
                                cipher: n.cipher || '',
                                auth: n.auth || ''
                            };
                        }
                    }
                });

                // Convert to array and sort by signal strength (strongest first)
                var networksList = Object.values(uniqueNetworks).sort(function (a, b) {
                    return b.signal - a.signal;
                });

                // Build table and SSID list
                networksList.forEach(function (n) {
                    ssids.push(n.SSID);

                    // Determine signal strength display
                    var signalStrength = n.signal;
                    var signalBars = '';
                    var signalClass = '';

                    if (signalStrength >= -50) {
                        signalBars = '▮▮▮▮';
                        signalClass = 'text-success';
                    } else if (signalStrength >= -60) {
                        signalBars = '▮▮▮▯';
                        signalClass = 'text-success';
                    } else if (signalStrength >= -70) {
                        signalBars = '▮▮▯▯';
                        signalClass = 'text-warning';
                    } else if (signalStrength >= -80) {
                        signalBars = '▮▯▯▯';
                        signalClass = 'text-warning';
                    } else {
                        signalBars = '▯▯▯▯';
                        signalClass = 'text-danger';
                    }

                    // Determine security type using enhanced API data
                    var security = 'Open';
                    var securityDetails = '';

                    if (n.encrypted === true) {
                        // Network is encrypted, determine the type
                        if (n.security) {
                            security = n.security;
                            securityDetails = n.security;

                            // Add cipher and auth details if available
                            if (n.cipher) {
                                securityDetails += ' (' + n.cipher;
                                if (n.auth) {
                                    securityDetails += ', ' + n.auth;
                                }
                                securityDetails += ')';
                            }
                        } else {
                            // Encrypted but no specific security type detected
                            security = 'Secured';
                            securityDetails = 'Encrypted';
                        }
                    }

                    // Check if this is the currently configured network
                    var isSelected = (n.SSID === currentSSID);
                    var rowClass = isSelected ? 'table-primary' : '';
                    var selectedIcon = isSelected ? '<i class="fas fa-check text-success" title="Currently configured"></i> ' : '';

                    networksHtml.push(`
                        <tr class="wifi-network-row ${rowClass}" style="cursor: pointer;" onclick="selectWifiNetwork('${escapeHtml(n.SSID)}', '${security}')">
                            <td style="padding: 8px;">${selectedIcon}${escapeHtml(n.SSID)}</td>
                            <td style="padding: 8px; white-space: nowrap;" class="${signalClass}">${signalBars} ${signalStrength}dBm</td>
                            <td style="padding: 8px;"><span class="badge badge-secondary" title="Raw data: ${escapeHtml(securityDetails)}">${security}</span></td>
                        </tr>
                    `);
                });

                // Update datalist for backward compatibility
                var html = [];
                ssids.forEach(function (n) {
                    if (typeof (n) != "undefined") {
                        html.push("<option value='");
                        html.push(escapeHtml(n));
                        html.push("'>");
                    }
                });
                $("#eth_ssids").html(html.join(''));

                // Update networks table
                if (networksHtml.length > 0) {
                    $("#wifiNetworksList").html(networksHtml.join(''));
                    $("#wifiNetworksRow").show();
                } else {
                    $("#wifiNetworksList").html('<tr><td colspan="3" style="padding: 8px; text-align: center; color: #666;">No WiFi networks found</td></tr>');
                    $("#wifiNetworksRow").show();
                }

            }).fail(function () {
                DialogError("Scan Failed", "Failed to Scan for wifi networks");
                $("#wifiNetworksList").html('<tr><td colspan="3" style="padding: 8px; text-align: center; color: #666;">Scan failed</td></tr>');
                $("#wifiNetworksRow").show();
            }).always(function () {
                $("#wifisearch").hide();
            });
        }

        function selectWifiNetwork(ssid, security) {
            // Update SSID field
            $('#eth_ssid').val(ssid);

            // Auto-set WPA3 checkbox if network supports it
            $('#eth_wpa3').prop('checked', security === 'WPA3');

            // Show/hide appropriate password fields based on security type
            if (security === 'Open') {
                $('#pskRow').hide();
                $('#keyRow').hide();
            } else if (security === 'WEP') {
                $('#pskRow').hide();
                $('#keyRow').show();
                $('#eth_key').focus();
            } else {
                // WPA, WPA2, WPA3
                $('#keyRow').hide();
                $('#pskRow').show();
                $('#eth_psk').focus();
            }

            // Highlight selected row
            $('.wifi-network-row').removeClass('table-primary');
            $('.wifi-network-row').find('i.fa-check').remove();

            // Add selection to clicked row
            event.target.closest('tr').classList.add('table-primary');
            var firstCell = event.target.closest('tr').querySelector('td:first-child');
            firstCell.innerHTML = '<i class="fas fa-check text-success" title="Currently configured"></i> ' + firstCell.innerHTML.replace(/^<i[^>]*><\/i>\s*/, '');
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

                var iface = currentInterface; // Use the global currentInterface variable
                if (!iface) {
                    DialogError("No Interface Selected", "Please select an interface first");
                    return;
                }
                var safeName = iface.replace(/[^a-zA-Z0-9]/g, '_');
                var url;
                var data = {};
                data.INTERFACE = iface;
                if ($('#eth_static_' + safeName).is(':checked')) {
                    data.PROTO = 'static';
                    data.ADDRESS = $('#eth_ip_' + safeName).val();
                    data.NETMASK = $('#eth_netmask_' + safeName).val();
                    // Gateway is now handled globally, not per interface
                } else {
                    data.PROTO = 'dhcp';
                }
                <? if ($settings['uiLevel'] >= 1) { ?>
                    data.ROUTEMETRIC = $('#eth_route_metric_' + safeName).val();
                    data.DHCPSERVER = $('#dhcpServer_' + safeName).is(':checked');
                    data.DHCPOFFSET = $('#dhcpOffset_' + safeName).val();
                    data.DHCPPOOLSIZE = $('#dhcpPoolSize_' + safeName).val();
                    data.IPFORWARDING = $('#ipForwarding_' + safeName).val();
                <? } ?>

                if (iface.substr(0, 2) == "wl") {
                    data.SSID = $('#eth_ssid_' + safeName).val();
                    data.PSK = $('#eth_psk_' + safeName).val();
                    data.KEY = $('#eth_key_' + safeName).val();
                    data.HIDDEN = $('#eth_hidden_' + safeName).is(':checked');
                    data.WPA3 = $('#eth_wpa3_' + safeName).is(':checked');
                    data.BACKUPSSID = $('#backupeth_ssid_' + safeName).val();
                    data.BACKUPPSK = $('#backupeth_psk_' + safeName).val();
                    data.BACKUPHIDDEN = $('#backupeth_hidden_' + safeName).is(':checked');
                    data.BACKUPWPA3 = $('#backupeth_wpa3_' + safeName).is(':checked');
                }

                data.Leases = {};
                $('#staticLeasesTable_' + safeName + ' > tbody > tr').each(function () {
                    var checkBox = $(this).find('#static_enabled');
                    if (checkBox.is(":checked")) {
                        var ip = $(this).find('#static_ip').val();
                        var mac = $(this).find('#static_mac').val();
                        data.Leases[ip] = mac;
                    }
                })

                var postData = JSON.stringify(data);

                // Check if interface configuration has actually changed
                $.get("api/network/interface/" + iface, function (currentData) {
                    var hasChanged = false;

                    // Compare all relevant fields
                    if (currentData.PROTO !== data.PROTO ||
                        currentData.ADDRESS !== data.ADDRESS ||
                        currentData.NETMASK !== data.NETMASK ||
                        currentData.ROUTEMETRIC !== data.ROUTEMETRIC ||
                        currentData.DHCPSERVER !== data.DHCPSERVER ||
                        currentData.DHCPOFFSET !== data.DHCPOFFSET ||
                        currentData.DHCPPOOLSIZE !== data.DHCPPOOLSIZE ||
                        currentData.IPFORWARDING !== data.IPFORWARDING ||
                        currentData.SSID !== data.SSID ||
                        currentData.PSK !== data.PSK ||
                        currentData.HIDDEN !== data.HIDDEN ||
                        currentData.WPA3 !== data.WPA3 ||
                        currentData.BACKUPSSID !== data.BACKUPSSID ||
                        currentData.BACKUPPSK !== data.BACKUPPSK ||
                        currentData.BACKUPHIDDEN !== data.BACKUPHIDDEN ||
                        currentData.BACKUPWPA3 !== data.BACKUPWPA3) {
                        hasChanged = true;
                    }

                    // Compare static leases
                    var currentLeases = currentData.Leases || {};
                    var newLeases = data.Leases || {};
                    if (JSON.stringify(currentLeases) !== JSON.stringify(newLeases)) {
                        hasChanged = true;
                    }

                    if (hasChanged) {
                        // Configuration changed, save it
                        $.post("api/network/interface/" + iface, postData
                        ).done(function (rc) {
                            if (rc.status == "OK") {
                                // Reload the specific interface configuration instead of old LoadNetworkConfig
                                if (currentInterface) {
                                    loadInterfaceConfiguration(currentInterface);
                                }
                                $.jGrowl(iface + " network interface configuration saved", { themeState: 'success' });
                                $('#btnConfigNetwork').show();

                                // Check gateway availability after saving interface config
                                setTimeout(checkGatewayAvailability, 200);

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
                    } else {
                        // No changes, just reload UI to ensure sync
                        if (currentInterface) {
                            loadInterfaceConfiguration(currentInterface);
                        }
                        $('#btnConfigNetwork').show();
                        setTimeout(checkGatewayAvailability, 200);
                    }
                }).fail(function () {
                    // If we can't get current config, proceed with save
                    $.post("api/network/interface/" + iface, postData
                    ).done(function (rc) {
                        if (rc.status == "OK") {
                            // Reload the specific interface configuration instead of old LoadNetworkConfig
                            if (currentInterface) {
                                loadInterfaceConfiguration(currentInterface);
                            }
                            $.jGrowl(iface + " network interface configuration saved", { themeState: 'success' });
                            $('#btnConfigNetwork').show();

                            // Check gateway availability after saving interface config
                            setTimeout(checkGatewayAvailability, 200);

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
                });

                SaveDNSConfig();
                SaveGlobalGateway(); // Save global gateway along with other settings
                SetRebootFlag();
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
            var iface = currentInterface;
            if (!iface) return;
            
            var safeName = iface.replace(/[^a-zA-Z0-9]/g, '_');
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
                            if (($('#eth_static_' + safeName).is(':checked')) &&
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

        // Interface tab management functions
        var currentInterface = '';
        var interfaceList = [];

        function populateInterfaceTabs() {
            // Get list of interfaces from hidden select element
            interfaceList = [];
            $('#selInterfaces option').each(function() {
                interfaceList.push($(this).val());
            });

            var tabsHtml = '';
            var contentHtml = '';
            var first = true;

            interfaceList.forEach(function(iface, index) {
                var activeClass = first ? ' active' : '';
                var showClass = first ? ' show active' : '';
                var safeName = iface.replace(/[^a-zA-Z0-9]/g, '_');
                
                tabsHtml += '<li class="nav-item">' +
                    '<a class="nav-link' + activeClass + '" id="' + safeName + '-tab" ' +
                    'data-bs-toggle="tab" data-bs-target="#' + safeName + '-content" ' +
                    'href="#' + safeName + '-content" role="tab" ' +
                    'aria-controls="' + safeName + '-content" aria-selected="' + (first ? 'true' : 'false') + '" ' +
                    'onclick="switchToInterface(\'' + iface + '\')">' +
                    iface + '</a></li>';

                contentHtml += '<div class="tab-pane fade' + showClass + '" id="' + safeName + '-content" ' +
                    'role="tabpanel" aria-labelledby="' + safeName + '-tab">' +
                    '<div class="interface-config-content" data-interface="' + iface + '">' +
                    '<div class="loading-placeholder">Loading ' + iface + ' configuration...</div>' +
                    '</div></div>';

                if (first) {
                    currentInterface = iface;
                    first = false;
                }
            });

            $('#interfaceTabs').html(tabsHtml);
            $('#interfaceTabContent').html(contentHtml);

            // Add event handlers for interface sub-tabs
            $('#interfaceTabs a[data-bs-toggle="tab"]').on('shown.bs.tab', function (e) {
                var iface = e.target.textContent;
                var safeName = iface.replace(/[^a-zA-Z0-9]/g, '_');
                var targetContent = $('#' + safeName + '-content');
                setTimeout(function() {
                    initializeTooltips(targetContent);
                }, 100);
            });

            // Load configuration for the first interface
            if (currentInterface) {
                loadInterfaceConfiguration(currentInterface);
            }
        }

        function switchToInterface(iface) {
            if (currentInterface !== iface) {
                currentInterface = iface;
                loadInterfaceConfiguration(iface);
            }
        }

        function loadInterfaceConfiguration(iface) {
            var safeName = iface.replace(/[^a-zA-Z0-9]/g, '_');
            var contentDiv = $('#' + safeName + '-content .interface-config-content');
            
            // Show loading state
            contentDiv.html('<div class="loading-placeholder">Loading ' + iface + ' configuration...</div>');
            
            // Update the hidden select value for compatibility with existing functions
            $('#selInterfaces').val(iface);
            
            // Load interface configuration
            var url = "api/network/interface/" + iface;
            var visible = iface.slice(0, 2).toLowerCase() == "wl" ? true : false;

            $.get(url, function(data) {
                // Copy the configuration content to this tab
                var configContent = $('#interfaceConfigTemplate').html();
                contentDiv.html(configContent);
                
                // Update form IDs to be unique for this interface
                contentDiv.find('[id]').each(function() {
                    var oldId = $(this).attr('id');
                    var newId = oldId + '_' + safeName;
                    $(this).attr('id', newId);
                    
                    // Update any labels or onclick references
                    contentDiv.find('[for="' + oldId + '"]').attr('for', newId);
                });
                
                // Update wireless visibility for this interface
                if (visible) {
                    contentDiv.find('#WirelessSubsection_' + safeName).show();
                    contentDiv.find('#WirelessSettings_' + safeName).show();
                    contentDiv.find('#wifiNetworksRow_' + safeName).show();
                } else {
                    contentDiv.find('#WirelessSubsection_' + safeName).hide();
                    contentDiv.find('#WirelessSettings_' + safeName).hide();
                    contentDiv.find('#wifiNetworksRow_' + safeName).hide();
                }
                
                // Now populate the form with the interface data
                populateInterfaceForm(data, safeName, contentDiv, iface);
                
                // Load WiFi networks if it's a wireless interface
                if (visible) {
                    LoadSIDSForInterface(iface, safeName);
                }
                
                // Initialize tooltips for the new content
                initializeTooltips(contentDiv);
            });
        }
        
        function populateInterfaceForm(data, safeName, contentDiv, interfaceName) {
            // Store the interface name in the content div for later use
            contentDiv.data('interface', interfaceName);
            
            // Debug: Log the data to see what we're getting
            // Populate the form fields with the interface data
            if (data.ADDRESS) {
                var ipElement = contentDiv.find('#eth_ip_' + safeName);
                ipElement.val(data.ADDRESS);
            }
            if (data.NETMASK) {
                contentDiv.find('#eth_netmask_' + safeName).val(data.NETMASK);
            }
            
            // Set interface mode (static/dhcp)
            if (data.PROTO == "static") {
                contentDiv.find('#eth_static_' + safeName).prop('checked', true);
                contentDiv.find('#eth_dhcp_' + safeName).prop('checked', false);
                contentDiv.find('#ipAddressRow_' + safeName).show();
                contentDiv.find('#netmaskRow_' + safeName).show();
            } else {
                contentDiv.find('#eth_dhcp_' + safeName).prop('checked', true);
                contentDiv.find('#eth_static_' + safeName).prop('checked', false);
                contentDiv.find('#ipAddressRow_' + safeName).hide();
                contentDiv.find('#netmaskRow_' + safeName).hide();
            }
            
            // Populate wireless settings if available
            if (data.SSID) {
                var ssidElement = contentDiv.find('#eth_ssid_' + safeName);
                ssidElement.val(data.SSID);
                // Show the appropriate password field based on the network type
                if (data.PSK) {
                    contentDiv.find('#pskRow_' + safeName).show();
                    contentDiv.find('#keyRow_' + safeName).hide();
                } else if (data.KEY) {
                    contentDiv.find('#keyRow_' + safeName).show();
                    contentDiv.find('#pskRow_' + safeName).hide();
                }
            }
            if (data.PSK) {
                contentDiv.find('#eth_psk_' + safeName).val(data.PSK);
            }
            if (data.KEY) {
                contentDiv.find('#eth_key_' + safeName).val(data.KEY);
            }
            if (data.HIDDEN) {
                contentDiv.find('#eth_hidden_' + safeName).prop('checked', data.HIDDEN == "1");
            }
            if (data.WPA3) {
                contentDiv.find('#eth_wpa3_' + safeName).prop('checked', data.WPA3 == "1");
            }
            
            // Populate backup wireless settings if available
            if (data.BACKUPSSID) {
                contentDiv.find('#backupeth_ssid_' + safeName).val(data.BACKUPSSID);
            }
            if (data.BACKUPPSK) {
                contentDiv.find('#backupeth_psk_' + safeName).val(data.BACKUPPSK);
            }
            if (data.BACKUPHIDDEN) {
                contentDiv.find('#backupeth_hidden_' + safeName).prop('checked', data.BACKUPHIDDEN == "1");
            }
            if (data.BACKUPWPA3) {
                contentDiv.find('#backupeth_wpa3_' + safeName).prop('checked', data.BACKUPWPA3 == "1");
            }
            
            if (data.ROUTEMETRIC) {
                contentDiv.find('#eth_route_metric_' + safeName).val(data.ROUTEMETRIC);
            }
            
            // Set up event handlers for this interface
            setupInterfaceEvents(safeName, contentDiv);
        }
        
        function updateInterfaceChildSettings(safeName, contentDiv) {
            // Handle DHCP Server settings visibility for this interface
            var dhcpServerChecked = contentDiv.find('#dhcpServer_' + safeName).is(':checked');
            
            if (dhcpServerChecked) {
                // Show DHCP Pool fields and Static Leases
                contentDiv.find('#dhcpOffset_' + safeName).closest('.row').show();
                contentDiv.find('#dhcpPoolSize_' + safeName).closest('.row').show();
                contentDiv.find('#staticLeases_' + safeName).show();
            } else {
                // Hide DHCP Pool fields and Static Leases
                contentDiv.find('#dhcpOffset_' + safeName).closest('.row').hide();
                contentDiv.find('#dhcpPoolSize_' + safeName).closest('.row').hide();
                contentDiv.find('#staticLeases_' + safeName).hide();
            }
        }
        
        function setupInterfaceEvents(safeName, contentDiv) {
            var interfaceName = contentDiv.data('interface');
            
            // Store original static values when interface is first loaded
            var originalIP = contentDiv.find('#eth_ip_' + safeName).val();
            var originalNetmask = contentDiv.find('#eth_netmask_' + safeName).val();
            
            // Setup static/dhcp radio button handlers
            contentDiv.find('#eth_static_' + safeName).on('click', function() {
                if ($(this).is(':checked')) {
                    // Switch to static mode
                    contentDiv.find('#eth_dhcp_' + safeName).prop('checked', false);
                    contentDiv.find('#ipAddressRow_' + safeName).show();
                    contentDiv.find('#netmaskRow_' + safeName).show();
                    
                    // Restore original values if they exist and fields are currently empty
                    var currentIP = contentDiv.find('#eth_ip_' + safeName).val();
                    var currentNetmask = contentDiv.find('#eth_netmask_' + safeName).val();
                    
                    if (!currentIP && originalIP) {
                        contentDiv.find('#eth_ip_' + safeName).val(originalIP);
                    }
                    if (!currentNetmask && originalNetmask) {
                        contentDiv.find('#eth_netmask_' + safeName).val(originalNetmask);
                    }
                    
                    // Update DNS settings (global)
                    $('#dns_dhcp').prop('checked', false);
                    $('#dns_manual').prop('checked', true);
                    
                    // Re-check gateway availability
                    checkGatewayAvailability();
                }
            });
            
            contentDiv.find('#eth_dhcp_' + safeName).on('click', function() {
                if ($(this).is(':checked')) {
                    // Switch to DHCP mode
                    contentDiv.find('#eth_static_' + safeName).prop('checked', false);
                    contentDiv.find('#ipAddressRow_' + safeName).hide();
                    contentDiv.find('#netmaskRow_' + safeName).hide();
                    
                    // Store current values before clearing (in case user entered new ones)
                    var currentIP = contentDiv.find('#eth_ip_' + safeName).val();
                    var currentNetmask = contentDiv.find('#eth_netmask_' + safeName).val();
                    
                    if (currentIP) originalIP = currentIP;
                    if (currentNetmask) originalNetmask = currentNetmask;
                    
                    // Clear IP fields for DHCP mode
                    contentDiv.find('#eth_ip_' + safeName).val("");
                    contentDiv.find('#eth_netmask_' + safeName).val("");
                    
                    // Re-check gateway availability
                    checkGatewayAvailability();
                }
            });
            
            // Setup refresh networks button
            contentDiv.find('.refresh-networks-btn').on('click', function() {
                LoadSIDSForInterface(interfaceName, safeName);
            });
            
            // Setup ping IP button
            contentDiv.find('.ping-ip-btn').on('click', function() {
                var ipValue = contentDiv.find('#eth_ip_' + safeName).val();
                if (ipValue) {
                    PingIP(ipValue, 3);
                } else {
                    $.jGrowl("Please enter an IP address to ping", { themeState: 'danger' });
                }
            });
            
            // Setup DHCP Server checkbox event handler
            contentDiv.find('#dhcpServer_' + safeName).on('change', function() {
                updateInterfaceChildSettings(safeName, contentDiv);
            });
            
            // Set initial visibility based on current DHCP Server state
            setTimeout(function() {
                updateInterfaceChildSettings(safeName, contentDiv);
            }, 100);
        }
        
        function LoadSIDSForInterface(iface, safeName) {
            // Load WiFi networks for this specific interface
            var wifiTableBody = $('#wifiNetworksList_' + safeName);
            wifiTableBody.html('<tr><td colspan="3">Scanning for networks...</td></tr>');
            
            // Get the current SSID for this interface
            var currentSSID = $('#eth_ssid_' + safeName).val();
            
            $.get("api/network/wifi/scan/" + iface, {
                timeout: 30000
            }).done(function (data) {
                var html = '';
                var uniqueNetworks = {};

                if (data && data.networks && data.networks.length > 0) {
                    // Process networks and keep only the strongest signal for each SSID
                    data.networks.forEach(function (n) {
                        if (n.SSID && n.SSID.trim() !== "") {
                            var ssid = n.SSID.trim();
                            var signal = parseInt(n.signal) || -100;

                            // Only keep this network if it's the first occurrence or has better signal
                            if (!uniqueNetworks[ssid] || signal > uniqueNetworks[ssid].signal) {
                                uniqueNetworks[ssid] = {
                                    SSID: ssid,
                                    signal: signal,
                                    encrypted: n.encrypted || false,
                                    security: n.security || '',
                                    cipher: n.cipher || '',
                                    auth: n.auth || ''
                                };
                            }
                        }
                    });

                    // Convert to array and sort by signal strength (strongest first)
                    var networksList = Object.values(uniqueNetworks).sort(function (a, b) {
                        return b.signal - a.signal;
                    });

                    // Build table rows
                    networksList.forEach(function (n) {
                        // Determine signal strength display
                        var signalStrength = n.signal;
                        var signalBars = '';
                        var signalClass = '';

                        if (signalStrength >= -50) {
                            signalBars = '▮▮▮▮';
                            signalClass = 'excellent';
                        } else if (signalStrength >= -60) {
                            signalBars = '▮▮▮▯';
                            signalClass = 'good';
                        } else if (signalStrength >= -70) {
                            signalBars = '▮▮▯▯';
                            signalClass = 'fair';
                        } else if (signalStrength >= -80) {
                            signalBars = '▮▯▯▯';
                            signalClass = 'poor';
                        } else {
                            signalBars = '▯▯▯▯';
                            signalClass = 'poor';
                        }

                        // Determine security type
                        var security = 'Open';
                        var securityBadge = '<span class="security-badge security-none">Open</span>';

                        if (n.encrypted === true) {
                            if (n.security) {
                                security = n.security;
                                if (security.includes('WPA3')) {
                                    securityBadge = '<span class="security-badge security-wpa3">WPA3</span>';
                                } else if (security.includes('WPA2')) {
                                    securityBadge = '<span class="security-badge security-wpa2">WPA2</span>';
                                } else if (security.includes('WPA')) {
                                    securityBadge = '<span class="security-badge security-wpa">WPA</span>';
                                } else if (security.includes('WEP')) {
                                    securityBadge = '<span class="security-badge security-wep">WEP</span>';
                                } else {
                                    securityBadge = '<span class="security-badge security-wpa">Secured</span>';
                                }
                            } else {
                                securityBadge = '<span class="security-badge security-wpa">Secured</span>';
                            }
                        }

                        // Check if this is the currently configured network
                        var isSelected = (n.SSID === currentSSID);
                        var rowClass = isSelected ? 'table-primary' : '';
                        var selectedIcon = isSelected ? '<i class="fas fa-check text-success" title="Currently configured"></i> ' : '';

                        html += '<tr class="wifi-network-row ' + rowClass + '" onclick="selectWifiNetworkForInterface(\'' + n.SSID.replace(/'/g, "\\'") + '\', \'' + safeName + '\', \'' + security + '\')" style="cursor: pointer;">' +
                            '<td style="padding: 6px 8px;">' + selectedIcon + n.SSID + '</td>' +
                            '<td class="wifi-signal ' + signalClass + '" style="padding: 6px 8px; text-align: center; font-weight: bold; white-space: nowrap;">' + signalBars + ' ' + signalStrength + 'dBm</td>' +
                            '<td style="padding: 6px 8px;">' + securityBadge + '</td>' +
                            '</tr>';
                    });
                } else {
                    html = '<tr><td colspan="3">No networks found</td></tr>';
                }
                wifiTableBody.html(html);
            }).fail(function() {
                wifiTableBody.html('<tr><td colspan="3" style="padding: 8px; text-align: center; color: #666;">Scan failed</td></tr>');
            });
        }
        
        function selectWifiNetworkForInterface(ssid, safeName, security) {
            // Remove previous selection highlighting for this interface
            $('#wifiNetworksList_' + safeName + ' .wifi-network-row').removeClass('table-primary');
            
            // Highlight the selected row
            event.target.closest('tr').classList.add('table-primary');
            
            // Set the SSID value for this interface
            $('#eth_ssid_' + safeName).val(ssid);
            
            // Auto-set WPA3 checkbox if network supports it
            $('#eth_wpa3_' + safeName).prop('checked', security === 'WPA3');

            // Show/hide appropriate password fields based on security type
            if (security === 'Open') {
                $('#pskRow_' + safeName).hide();
                $('#keyRow_' + safeName).hide();
            } else if (security === 'WEP') {
                $('#pskRow_' + safeName).hide();
                $('#keyRow_' + safeName).show();
                $('#eth_key_' + safeName).focus();
            } else {
                // WPA, WPA2, WPA3
                $('#keyRow_' + safeName).hide();
                $('#pskRow_' + safeName).show();
                $('#eth_psk_' + safeName).focus();
            }
        }

        $(document).ready(function () {

            // Initialize interface tabs instead of loading single config
            populateInterfaceTabs();
            LoadDNSConfig();
            LoadGlobalGateway(); // Load global gateway settings

            $("#dns1").on("change", validateDNSFields);
            $("#dns2").on("change", validateDNSFields);
            $("#global_gateway").on("change", function () {
                // Simple validation on change - just check if it's a valid IP when not empty
                var gateway = $(this).val();
                if (gateway !== "" && !validateIPaddress('global_gateway')) {
                    $(this).css('border', '#F00 2px solid');
                } else {
                    $(this).css('border', '#D2D2D2 1px solid');
                }
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
            // Gateway is now handled globally, not per interface

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
                    contentDiv.find('#routeMetric_' + safeName).val(data.ROUTEMETRIC);
                } else {
                    contentDiv.find('#routeMetric_' + safeName).val(0);
                }
                if (data.DHCPSERVER) {
                    if (data.DHCPSERVER == "1") {
                        contentDiv.find('#dhcpServer_' + safeName).prop('checked', true);
                    } else {
                        contentDiv.find('#dhcpServer_' + safeName).prop('checked', false);
                    }
                } else {
                    contentDiv.find('#dhcpServer_' + safeName).prop('checked', false);
                }
                if (data.DHCPOFFSET) {
                    contentDiv.find('#dhcpOffset_' + safeName).val(data.DHCPOFFSET);
                } else {
                    contentDiv.find('#dhcpOffset_' + safeName).val(100);
                }
                if (data.DHCPPOOLSIZE) {
                    contentDiv.find('#dhcpPoolSize_' + safeName).val(data.DHCPPOOLSIZE);
                } else {
                    contentDiv.find('#dhcpPoolSize_' + safeName).val(50);
                }
                if (data.IPFORWARDING) {
                    contentDiv.find('#ipForwarding_' + safeName).val(data.IPFORWARDING);
                } else {
                    contentDiv.find('#ipForwarding_' + safeName).val(0);
                }

                contentDiv.find("#staticLeasesTable_" + safeName + " > tbody").empty();
                if (data.DHCPSERVER && (data.DHCPSERVER == "1")) {
                    var leases = data.StaticLeases;
                    for (ip in leases) {
                        var mac = leases[ip];
                        var tr = "<tr><td><input type='checkbox' id='static_enabled' checked></td>";
                        tr += "<td><input type='text' id='static_ip' value='" + ip + "' size='16'></td>";
                        tr += "<td><input type='hidden' id='static_mac' value='" + mac + "'/>" + mac + "</td></tr>";
                        contentDiv.find("#staticLeasesTable_" + safeName + " > tbody").append(tr);
                    }
                    var leases = data.CurrentLeases;
                    for (ip in leases) {
                        var mac = leases[ip];
                        var tr = "<tr><td><input type='checkbox' id='static_enabled'></td>";
                        tr += "<td><input type='text' id='static_ip' value='" + ip + "' size='16'></td>";
                        tr += "<td><input type='hidden' id='static_mac' value='" + mac + "'/>" + mac + "</td></tr>";
                        contentDiv.find("#staticLeasesTable_" + safeName + " > tbody").append(tr);
                    }
                    contentDiv.find("#staticLeases_" + safeName).show();
                } else {
                    contentDiv.find("#staticLeases_" + safeName).hide();
                }

                // Note: updateInterfaceChildSettings will be called after setupInterfaceEvents
            <? } else { ?>
                contentDiv.find("#staticLeases_" + safeName).hide();
            <? } ?>
            CheckDNS();

            // Check gateway availability after interface info is loaded
            checkGatewayAvailability();

            // Initialize tooltips for newly loaded content  
            setTimeout(function () {
                initializeTooltips();
            }, 200);
        }

        function DisableNetworkFields(disabled) {
            $('#eth_ip').prop("disabled", disabled);
            $('#eth_netmask').prop("disabled", disabled);

            // Hide/show IP Address and Netmask rows based on DHCP/Static selection
            if (disabled) {
                // DHCP mode - hide IP Address and Netmask rows
                $('#ipAddressRow').fadeOut(200);
                $('#netmaskRow').fadeOut(200);
            } else {
                // Static mode - show IP Address and Netmask rows
                $('#ipAddressRow').fadeIn(200);
                $('#netmaskRow').fadeIn(200);
            }

            // Gateway is now global, not per interface
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
                alert("Invalid Host Name.  Host Name may contain only letters, numbers, and hyphens and may not begin or end with a hyphen.");
                return;
            }

            // Check if hostname has actually changed
            $.get("api/settings/HostName", function (currentHostname) {
                if (currentHostname !== newHostname) {
                    $.put("api/settings/HostName", newHostname
                    ).done(function () {
                        $.jGrowl("Host Name Saved", { themeState: 'success' });
                        SetRebootFlag();
                    }).fail(function () {
                        DialogError("Save Host Name", "Save Failed");
                    });
                }
                // If no change, do nothing (no notification)
            }).fail(function () {
                // If we can't get current hostname, proceed with save
                $.put("api/settings/HostName", newHostname
                ).done(function () {
                    $.jGrowl("Host Name Saved", { themeState: 'success' });
                    SetRebootFlag();
                }).fail(function () {
                    DialogError("Save Host Name", "Save Failed");
                });
            });
        }

        function setHostDescription() {
            var newDescription = $('#hostDescription').val();

            // Check if host description has actually changed
            $.get("api/settings/HostDescription", function (currentDescription) {
                if (currentDescription !== newDescription) {
                    $.put("api/settings/HostDescription", newDescription
                    ).done(function () {
                        $.jGrowl("HostDescription Saved", { themeState: 'success' });
                        SetRestartFlag(2);
                    }).fail(function () {
                        DialogError("Save HostDescription", "Save Failed");
                    });
                }
                // If no change, do nothing (no notification)
            }).fail(function () {
                // If we can't get current description, proceed with save
                $.put("api/settings/HostDescription", newDescription
                ).done(function () {
                    $.jGrowl("HostDescription Saved", { themeState: 'success' });
                    SetRestartFlag(2);
                }).fail(function () {
                    DialogError("Save HostDescription", "Save Failed");
                });
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
                        <a class="nav-link" id="tab-global-network-tab" data-bs-toggle="tab" data-bs-target="#tab-global-network"
                            href="#tab-global-network" role="tab" aria-controls="tab-global-network" aria-selected="false">
                            Global Network Settings
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

                            <div class="interface-config-section">
                                <h2>Interface Settings</h2>

                                <!-- Interface Sub-tabs -->
                                <div class="interface-tabs-container" style="margin-bottom: 20px;">
                                    <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 15px;">
                                        <ul class="nav nav-tabs interface-tabs" id="interfaceTabs" role="tablist">
                                            <!-- Interface tabs will be populated by JavaScript -->
                                        </ul>
                                        <div class="interface-actions">
                                            <? if ($settings['uiLevel'] >= 1) { ?>
                                                <input name="btnAddInterface" type="button" class="buttons"
                                                    style="font-size: 12px; padding: 4px 8px; white-space: nowrap;"
                                                    value="Add New Interface" onClick="AddInterface();">
                                            <? } ?>
                                            <input name="btnSetInterface" type="button" class="buttons btn-success"
                                                value="Update Interface" onClick="SaveNetworkConfig();"
                                                style="margin-left: 10px;">
                                            <input id="btnConfigNetwork" type="button" style="display: none;"
                                                class="buttons" value="Restart Network" onClick="ApplyNetworkConfig();">
                                        </div>
                                    </div>
                                    
                                    <div class="tab-content interface-tab-content" id="interfaceTabContent">
                                        <!-- Interface configuration content will be populated by JavaScript -->
                                    </div>
                                </div>

                                <!-- Hidden select for backward compatibility -->
                                <select id="selInterfaces" style="display: none;">
                                    <?php PopulateInterfaces($uiLevel, $configDirectory); ?>
                                </select>

                                <!-- Hidden interface configuration template -->
                                <div id="interfaceConfigTemplate" style="display: none;">
                                    
                                    <!-- Wireless Configuration Subsection -->
                                    <h3 class="interface-subsection" id="WirelessSubsection" style="display: none;">
                                        <i class="fas fa-wifi"></i> Wireless Configuration
                                    </h3>
                                    <div class="container-fluid settingsTable settingsGroupTable" id="WirelessSettings"
                                        style="display: none;">

                                        <!-- WiFi Networks Table -->
                                        <div class="row" id="wifiNetworksRow" style="display: none;">
                                            <div class="printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2">
                                                <div class="description">Available Networks</div>
                                            </div>
                                            <div class="printSettingFieldCol col-md">
                                                <div class="warning-text" style="display: none;" id="wifisearch">Scanning
                                                    for WiFi networks...</div>
                                                <div id="wifiNetworksTable"
                                                    style="max-height: 200px; overflow-y: auto; border: 1px solid #ddd; border-radius: 4px; margin-bottom: 10px;">
                                                    <table class="table table-sm table-hover" style="margin: 0;">
                                                        <thead style="background-color: #f8f9fa;">
                                                            <tr>
                                                                <th style="padding: 8px;">Network Name (SSID)</th>
                                                                <th style="padding: 8px; width: 100px;">Signal</th>
                                                                <th style="padding: 8px; width: 80px;">Security</th>
                                                            </tr>
                                                        </thead>
                                                        <tbody id="wifiNetworksList">
                                                            <!-- Networks will be populated here -->
                                                        </tbody>
                                                    </table>
                                                </div>
                                                <button type="button" class="btn btn-sm btn-secondary" class="refresh-networks-btn">
                                                    <i class="fas fa-sync"></i> Refresh Networks
                                                </button>
                                            </div>
                                        </div>

                                        <div class="row" id="essidRow">
                                            <div class="printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2">
                                                <div class="description">WPA SSID</div>
                                            </div>
                                            <div class="printSettingFieldCol col-md"><input list="eth_ssids" name="eth_ssid"
                                                    id="eth_ssid" size="32" maxlength="32"><datalist
                                                    id='eth_ssids'></datalist><input type="checkbox" name="eth_hidden"
                                                    id="eth_hidden" value="Hidden">Hidden&nbsp;<span id="eth_hidden_help"
                                                    data-bs-title="Enable this for networks that don't broadcast their SSID (hidden networks)"
                                                    data-bs-toggle="tooltip" data-bs-placement="auto" data-bs-html="true">
                                                    <img id="eth_hidden_help_icon" src="images/redesign/help-icon.svg"
                                                        class="icon-help"
                                                        alt="Help icon for Hidden network setting"></span>&nbsp;<input
                                                    type="checkbox" name="eth_wpa3" id="eth_wpa3"
                                                    value="WPA3">WPA3&nbsp;<span id="eth_wpa3_help"
                                                    data-bs-title="Enable this for networks that support WPA3 (newer, more secure than WPA2)"
                                                    data-bs-toggle="tooltip" data-bs-placement="auto" data-bs-html="true">
                                                    <img id="eth_wpa3_help_icon" src="images/redesign/help-icon.svg"
                                                        class="icon-help" alt="Help icon for WPA3 setting"></span></div>
                                        </div>
                                        <div class="row" id="pskRow" style="display: none;">
                                            <div class="printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2">
                                                <div class="description">WPA Pre Shared key (PSK)</div>
                                            </div>
                                            <div class="printSettingFieldCol col-md"><input type="password" name="eth_psk"
                                                    id="eth_psk" size="32" maxlength="64">&nbsp;<i class='fas fa-eye'
                                                    id='eth_pskHideShow' onClick='TogglePasswordHideShow("eth_psk");'></i>
                                            </div>
                                        </div>
                                        <div class="row" id="keyRow" style="display: none;">
                                            <div class="printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2">
                                                <div class="description">WEP Key</div>
                                            </div>
                                            <div class="printSettingFieldCol col-md"><input type="password" name="eth_key"
                                                    id="eth_key" size="26">&nbsp;<i class='fas fa-eye' id='eth_keyHideShow'
                                                    onClick='TogglePasswordHideShow("eth_key");'></i></div>
                                        </div>
                                        <? if ($settings['uiLevel'] >= 2) { ?>
                                            <div class="row" id="backupessidRow">
                                                <div class="printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2">
                                                    <div class="description">
                                                        <i class="fas fa-fw fa-graduation-cap fa-nbsp ui-level-1"
                                                            data-bs-tooltip-title="Advanced Level Setting"
                                                            data-bs-toggle="tooltip" data-bs-placement="auto"
                                                            aria-label="Advanced Level Setting"
                                                            data-bs-original-title="Advanced Level Setting"></i>
                                                        Backup WPA SSID
                                                    </div>
                                                </div>
                                                <div class="printSettingFieldCol col-md"><input list="backupeth_ssids"
                                                        name="backupeth_ssid" id="backupeth_ssid" size="32"
                                                        maxlength="32"><datalist id='backupeth_ssids'></datalist><input
                                                        type="checkbox" name="backupeth_hidden" id="backupeth_hidden"
                                                        value="BACKUPHIDDEN">Hidden&nbsp;<span id="backupeth_hidden_help"
                                                        data-bs-title="Enable this for networks that don't broadcast their SSID (hidden networks)"
                                                        data-bs-toggle="tooltip" data-bs-placement="auto" data-bs-html="true">
                                                        <img id="backupeth_hidden_help_icon" src="images/redesign/help-icon.svg"
                                                            class="icon-help"
                                                            alt="Help icon for backup Hidden network setting"></span>&nbsp;<input
                                                        type="checkbox" name="backupeth_wpa3" id="backupeth_wpa3"
                                                        value="BACKUPWPA3">WPA3&nbsp;<span id="backupeth_wpa3_help"
                                                        data-bs-title="Enable this for networks that support WPA3 (newer, more secure than WPA2)"
                                                        data-bs-toggle="tooltip" data-bs-placement="auto" data-bs-html="true">
                                                        <img id="backupeth_wpa3_help_icon" src="images/redesign/help-icon.svg"
                                                            class="icon-help" alt="Help icon for backup WPA3 setting"></span>
                                                </div>
                                            </div>
                                            <div class="row" id="backuppskRow">
                                                <div class="printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2">
                                                    <div class="description">
                                                        <i class="fas fa-fw fa-graduation-cap fa-nbsp ui-level-1"
                                                            data-bs-tooltip-title="Advanced Level Setting"
                                                            data-bs-toggle="tooltip" data-bs-placement="auto"
                                                            aria-label="Advanced Level Setting"
                                                            data-bs-original-title="Advanced Level Setting"></i>
                                                        Backup WPA Pre Shared key (PSK)
                                                    </div>
                                                </div>
                                                <div class="printSettingFieldCol col-md"><input type="password"
                                                        name="backupeth_psk" id="backupeth_psk" size="32"
                                                        maxlength="64">&nbsp;<i class='fas fa-eye' id='backupeth_pskHideShow'
                                                        onClick='TogglePasswordHideShow("backupeth_psk");'></i></div>
                                            </div>
                                        <? } ?>
                                    </div>

                                    <!-- Basic Configuration -->
                                <h3 class="interface-subsection">
                                    <i class="fas fa-network-wired"></i> Basic Configuration
                                </h3>
                                <div class="container-fluid settingsTable settingsGroupTable">
                                    <div class="row" id="interfaceConfigTypeRow">
                                        <div class="printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2">
                                            <div class="description">Interface Mode</div>
                                        </div>
                                        <div class="printSettingFieldCol col-md"><label><input type="radio"
                                                    id="eth_static" value="static">
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
                                            <input type="button" class="buttons ping-ip-btn" value='Ping'>
                                        </div>
                                    </div>
                                    <div class="row" id="netmaskRow">
                                        <div class="printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2">
                                            <div class="description">Netmask</div>
                                        </div>
                                        <div class="printSettingFieldCol col-md"><input type="text" name="eth_netmask"
                                                id="eth_netmask" size="15" maxlength="15"></div>
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








                                <!-- Advanced Settings Subsection -->
                                <? if ($settings['uiLevel'] >= 1) { ?>
                                    <h3 class="interface-subsection">
                                        <i class="fas fa-cogs"></i> Advanced Configuration
                                    </h3>
                                    <div class="container-fluid settingsTable settingsGroupTable">
                                        <?php PrintSettingGroup("advNetworkSettingsGroup", "", "", 1, "", "", true); ?>
                                        
                                        <div class="row">
                                            <div class="printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2">
                                            </div>
                                            <div class="printSettingFieldCol col-md">
                                                <div id="staticLeases">
                                                    <h4>Static Leases</h4>
                                                    <div class="fppTableWrapper">
                                                        <div class='fppTableContents' role="region"
                                                            aria-labelledby="staticLeasesTable" tabindex="0">
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
                                            </div>
                                        </div>
                                    </div>
                                <? } ?>
                                </div>
                            </div>

                            <!-- Network Utilities -->
                            <div class="network-utilities-section">
                                <h2>Network Utilities</h2>

                                <!-- Interface Management Subsection -->
                                <h3 class="utilities-subsection">
                                    <i class="fas fa-tools"></i> Interface Management
                                </h3>
                                <div class="container-fluid settingsTable settingsGroupTable">
                                    <div class="row">
                                        <div class="printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2">
                                            <div class="description">Interface Names</div>
                                        </div>
                                        <div class="printSettingFieldCol col-md">
                                            <input id="btnConfigNetworkPersist" type="button" class="buttons"
                                                value="Create Persistent Names" onClick="CreatePersistentNames();">
                                            <input id="btnConfigNetworkPersistClear" type="button" class="buttons"
                                                value="Clear Persistent Names" onClick="ClearPersistentNames();">
                                            <div class="description-text"
                                                style="font-size: 0.9em; color: #666; margin-top: 5px;">
                                                Create or clear persistent network interface names to ensure consistent
                                                interface naming across reboots.
                                            </div>
                                        </div>
                                    </div>
                                </div>
                            </div>

                        </div>
                    </div>
                    <div class="tab-pane fade" id="tab-global-network" role="tabpanel" aria-labelledby="tab-global-network-tab">
                        
                        <!-- Global Network Settings -->
                        <div class="global-network-section">
                            <h2>Global Network Settings</h2>

                            <!-- Host Configuration Subsection -->
                            <h3 class="network-subsection">
                                <i class="fas fa-desktop"></i> Host Configuration
                            </h3>
                            <div class="warning-text" style="margin-bottom: 15px;">
                                <b>Changing the Host Name from FPP will cause http://fpp.local/ to change and you will need to
                                use the new Host Name eg http://&lt;Host Name&gt;.local/</b>
                            </div>
                            <div class="container-fluid settingsTable settingsGroupTable">
                                <?
                                PrintSettingGroup('host', '', '', '', '', '', false);
                                ?>
                            </div>

                            <!-- Gateway Subsection -->
                            <h3 class="network-subsection">
                                <i class="fas fa-route"></i> Gateway Configuration
                            </h3>
                            <div class="container-fluid settingsTable settingsGroupTable">
                                <div class="row" id="globalGatewayRow">
                                    <div class="printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2">
                                        <div class="description">Default Gateway</div>
                                    </div>
                                    <div class="printSettingFieldCol col-md">
                                        <input type="text" name="global_gateway" id="global_gateway" size="35"
                                            maxlength="15">
                                        <input type="button" class="buttons"
                                            onClick='PingIP($("#global_gateway").val(), 3);' value='Ping'>
                                        <input type="button" class="buttons btn-success"
                                            onClick='SaveGlobalGateway();' value='Update Gateway'>
                                        <div class="description-text"
                                            style="font-size: 0.9em; color: #666; margin-top: 5px;">
                                            Global gateway for all interfaces. Leave blank if using DHCP on any
                                            interface for routing.
                                        </div>
                                    </div>
                                </div>
                            </div>

                            <!-- DNS Subsection -->
                            <h3 class="network-subsection">
                                <i class="fas fa-server"></i> DNS Configuration
                            </h3>
                            <div class="warning-text" id="dns_warning"></div>
                            <div class="container-fluid settingsTable settingsGroupTable">
                                <div class="row" id="dnsServerRow">
                                    <div class="printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2">
                                        <div class="description">DNS Server Mode</div>
                                    </div>
                                    <div class="printSettingFieldCol col-md"><label><input type="radio"
                                                id="dns_manual" value="manual">
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
                                <div class="row">
                                    <div class="printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2">
                                    </div>
                                    <div class="printSettingFieldCol col-md">
                                        <input type="button" class="buttons btn-success" onClick='SaveDNSConfig();'
                                            value='Update DNS'>
                                        <div class="description-text"
                                            style="font-size: 0.9em; color: #666; margin-top: 5px;">
                                            Global DNS configuration for all interfaces.
                                        </div>
                                    </div>
                                </div>
                            </div>

                            <!-- WiFi Configuration Subsection -->
                            <h3 class="network-subsection">
                                <i class="fas fa-wifi"></i> WiFi Configuration
                            </h3>
                            <div class="container-fluid settingsTable settingsGroupTable">
                                <?php
                                if (file_exists("/etc/modprobe.d/wifi-disable-power-management.conf")) {
                                    PrintSettingGroup("wifiDriversGroup");
                                } else {
                                    PrintSettingGroup("wifiDomainGroup");
                                }
                                ?>
                            </div>
                        </div>
                        <script>
                            function NoSaveSettingCallback() {
                            }
                            
                            function dhcpServerEnabledCallback() {
                                // This function is called by PHP-generated DHCP Server checkboxes
                                // We need to find which interface tab is currently active and update its settings
                                var activeTab = $('.tab-pane.active .interface-config-content[data-interface]');
                                if (activeTab.length > 0) {
                                    var interfaceName = activeTab.data('interface');
                                    var safeName = interfaceName.replace(/[^a-zA-Z0-9]/g, '_');
                                    updateInterfaceChildSettings(safeName, activeTab);
                                }
                            }
                        </script>

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