<!DOCTYPE html>
<html>
<head>
<?php
require_once "config.php";
require_once "common.php";
include 'common/menuHead.inc';

$advancedView = true;
if ((isset($settings['MultiSyncAdvancedView'])) &&
    ($settings['MultiSyncAdvancedView'] == 1)) {
    $advancedView = true;
}

?>
<script type="text/javascript" src="jquery/jquery.tablesorter/jquery.tablesorter.js"></script>
<script type="text/javascript" src="jquery/jquery.tablesorter/jquery.tablesorter.widgets.js"></script>
<script type="text/javascript" src="jquery/jquery.tablesorter/parsers/parser-network.js"></script>
<script type="text/javascript" src="js/xlsx.full.min.js" async></script>
<script type="text/javascript" src="js/FileSaver.min.js" async></script>

<title><?echo $pageTitle; ?></title>
<style>


.actionOptions {
    display: none;
}
</style>

<script>
    var hostRows = new Object();
    var rowSpans = new Object();
    var advancedView = true;
    var systemStatusCache = {}; // Cache of api/system/status?ip[]=
    var localFpposFiles = [];
    var proxies = [];

    function getLocalFpposFiles() {
        $.get('api/files/uploads', function(data) {
            if (data.hasOwnProperty("files")) {
                data.files.forEach(function(f) {
                    if (f.hasOwnProperty("name")) {
                        if (f.name.endsWith(".fppos")) {
                            let type = "UNKNOWN";
                            if (f.name.startsWith("BBB-")) {
                                type = "BBB";
                            } else if (f.name.startsWith("Pi-")) {
                                type = "PI";
                            }

                            if (type != "UNKNOWN") {
                                localFpposFiles.push({
                                    type: type,
                                    name: f.name,
                                    sizeBytes: f.sizeBytes,
                                    sizeHuman: f.sizeHuman
                                });
                            }
                        } // check if .fppos
                    } // Verify has name
                }); // loop over files

                if (localFpposFiles.length > 0) {
                    let html = [];
                    localFpposFiles.forEach(function(f) {
                        html.push('<div class="row">');
                        html.push('<div class="col-2 col-sm-1"><input id="');
                        html.push(f.name);
                        html.push('" type="checkbox"></div>')
                        html.push('<div class="col-7 col-sm-5 col-md-4 col-lg-3 col-xl-2">');
                        html.push(f.name);
                        html.push('</div><div class="col-2 col-sm-1">');
                        html.push(f.type);
                        html.push('</div><div class="col-2" col-md-1>');
                        html.push(f.sizeHuman);
                        html.push('</div>');
                        html.push("</div>");
                    });
                    $("#copyOSOptionsDetails").html(html.join(''));
                    }
            } // hasOwnProperty("files");
        });
    }

    /*
     * Does a deep object flattening of "obj" using the specified base path
     * setting new properties on rc to rc[path + "." + key] == value
     */
    function flattenObject(obj, path, rc) {
        if( ((typeof obj != "object" && typeof obj != 'function')) || (obj == null) ) {
            console.log("WARNING: Not an object", obj);
            return;
        }

        for (const [key, value] of Object.entries(obj)) {
            let newPath = (path === "" ? key : path + "." + key )
            if (typeof value === "object") {
                flattenObject(value, newPath, rc);
            } else if (typeof value === "boolean" || typeof value === "string" || typeof value == "number") {
                rc[newPath] = value;
            } else {
                console.log("Unable to handle path ", newPath, "of type", typeof value);
            }
        }

    }

    /*
     * Returns Mode + value of "Send Multisync"
     */
    function getFullMode(data) {
        rc = "Unknown";
        if (data.hasOwnProperty('mode')) {
            rc = modeToString(data.mode);
        }
        if (data.hasOwnProperty('multisync') && data.multisync && data.mode == 2) {
            rc += " w/ Multisync"
        }

        return rc;
    }

    function exportMultisync() {
        if (systemStatusCache == null || systemStatusCache == "" || systemStatusCache == "null") {
            $.jGrowl("Please wait until the system statuses finish loading",{themeState:'danger'});
            return;
        }
        const allKeys = new Set();
        let finalData = {};
        // Flatten the data
        for (const [ip, data] of Object.entries(systemStatusCache)) {
            let rc={};
            flattenObject(data, "", rc);
            finalData[ip] = rc;

            for(const [key, junk] of Object.entries(rc)) {
                allKeys.add(key);
            }
        }

        // Create XLSX
        const sortedKeys = Array.from(allKeys).sort();
        let labels = ['ip'];
        labels = labels.concat(sortedKeys);

        let allRows = [];
        allRows.push(labels);

        for (const [ip, data] of Object.entries(finalData)) {
            let row = [];
            row.push(ip);
            let value = "";
            for (const key of sortedKeys) {
                if (key in data) {
                    value = data[key];
                }
                row.push(value);
            }
            allRows.push(row);
        }

        var wb = XLSX.utils.book_new();
        wb.SheetNames.push("Data");
        var ws = XLSX.utils.aoa_to_sheet(allRows);
        wb.Sheets["Data"] = ws;
        var wbout = XLSX.write(wb, {bookType:'xlsx',  type: 'binary'});
        saveAs(new Blob([s2ab(wbout)],{type:"application/octet-stream"}), 'export.xlsx');

    }

    function s2ab(s) {
        var buf = new ArrayBuffer(s.length); //convert s to arrayBuffer
        var view = new Uint8Array(buf);  //create uint8array as viewer
        for (var i=0; i<s.length; i++) view[i] = s.charCodeAt(i) & 0xFF; //convert to octet
        return buf;
    }

    function rowSpanSet(rowID) {
        var rowSpan = 1;

        if ($('#' + rowID + '_logs').is(':visible'))
            rowSpan++;

        if ($('#' + rowID + '_warnings').is(':visible'))
            rowSpan++;

        $('#' + rowID + ' > td:nth-child(1)').attr('rowspan', rowSpan);
    }

    // Updates the warning information for multi-sync
    // Also handles if warnings row should be displayed in general.
    function validateMultiSyncSettings() {
        var multicastChecked = $('#MultiSyncMulticast').is(":checked");
        var broadcastChecked = $('#MultiSyncBroadcast').is(":checked");
        // Remove any Multisync warnings
        $(document).find(".multisync-warning").remove();

        // check if Warnings window should be shown
        $(document).find('tr[id*="_warnings"').each(function(){
            let cnt = $(this).find('td[id*="_warningCell"').children().length;
            if (cnt ==0) {
                $(this).hide();
            } else {
                $(this).show();
            }
        });


        // If these are unchecked, than each remote can be set either way
        // no need to check anything
        if (! (multicastChecked || broadcastChecked)) {
            return;
        }

        $("input.syncCheckbox").each(function() {
			if ($(this).is(":checked")) {
                let name = $(this).attr('name');
                let msg = "";
                if (multicastChecked) {
                    msg = "Having Unicast checked when Multicast is enabled (view options below) is discouraged: " + name;
                } else {
                    msg = "Having Unicast checked when Broadcast is enabled (view options below) is discouraged: " + name;
                }
                msg = '<div class="warning-text multisync-warning">' + msg + '</div>';
                $(this).closest('.systemRow').next(".warning-row").each(function(){
                    $(this).find("td").append(msg);
                    $(this).show();
                })
            }
        });
    }

    function updateMultiSyncRemotes(verbose = false) {
		var remotes = "";

		$('input.syncCheckbox').each(function() {
			if ($(this).is(":checked")) {
				if (remotes != "") {
					remotes += ",";
				}
				remotes += $(this).attr("name");
			}
		});

        var multicastChecked = $('#MultiSyncMulticast').is(":checked");
        var broadcastChecked = $('#MultiSyncBroadcast').is(":checked");

        if ((remotes == '') &&
            (!$('#MultiSyncMulticast').is(":checked")) &&
            (!$('#MultiSyncBroadcast').is(":checked"))) {
            $('#MultiSyncMulticast').prop('checked', true).trigger('change');
            alert('FPP will use multicast if no other sync methods are chosen.');
        }

		$.put("api/settings/MultiSyncRemotes", remotes
		).done(function() {
			settings['MultiSyncRemotes'] = remotes;
            if (verbose) {
                if (remotes == "") {
                    $.jGrowl("Remote List Cleared.  You must restart fppd for the changes to take effect.",{themeState:'success'});
                } else {
                    $.jGrowl("Remote List set to: '" + remotes + "'.  You must restart fppd for the changes to take effect.",{themeState:'success'});
                }
            }

            //Mark FPPD as needing restart
            SetRestartFlag(2);
            settings['restartFlag'] = 2;
            //Get the resart banner showing
            CheckRestartRebootFlags();
            validateMultiSyncSettings();
        }).fail(function() {
			DialogError("Save Remotes", "Save Failed");
		});

	}

    function isFPP(typeId) {
        typeId = parseInt(typeId);

        if ((typeId >= 0x01) && (typeId < 0x80))
            return true;

        return false;
    }

    function isFPPPi(typeId) {
        typeId = parseInt(typeId);

        if ((typeId >= 0x02) && (typeId <= 0x3F))
            return true;

        return false;
    }

    function isFPPBeagleBone(typeId) {
        typeId = parseInt(typeId);

        if ((typeId >= 0x41) && (typeId <= 0x5F))
            return true;

        return false;
    }
    function isFPPMac(typeId) {
        typeId = parseInt(typeId);

        if (typeId == 0x70)
            return true;

        return false;
    }
    function isFPPArmbian(typeId) {
     typeId = parseInt(typeId);

        if (typeId == 0x60)
            return true;

        return false;
    }

    function isUnknownController(typeId) {
        typeId = parseInt(typeId);

        if (typeId == 0x00)
            return true;

        return false;
    }

    function isFalcon(typeId) {
        typeId = parseInt(typeId);

        if ((typeId >= 0x80) && (typeId <= 0x87))
            return true;

        return false;
    }

    function isFalconV4(typeId) {
        typeId = parseInt(typeId);

        if ((typeId >= 0x88) && (typeId <= 0x89))
            return true;

        return false;
    }

    function isESPixelStick(typeId) {
        typeId = parseInt(typeId);

        if (typeId == 0xC2 || typeId == 0xC3)
            return true;

        return false;
    }

    function isSanDevices(typeId) {
        typeId = parseInt(typeId);

        if (typeId == 0xFF)
            return true;

        return false;
    }

    function isGenius(typeId) {
        typeId = parseInt(typeId);

        if ((typeId >= 0xA0) && (typeId <= 0xAF))
            return true;

        return false;
    }


    function isWLED(typeId) {
        typeId = parseInt(typeId);

        if (typeId == 0xFB)
            return true;

        return false;
    }

    function isProxied(ip) {
        return proxies.includes(ip);
    }

    function getLocalVersionLink(ip, data) {
        var updatesAvailable = 0;
        if ((typeof (data.advancedView.RemoteGitVersion) !== 'undefined') &&
            (typeof (data.advancedView.LocalGitVersion) !== 'undefined') &&
            (data.advancedView.RemoteGitVersion !== "Unknown") &&
            (data.advancedView.RemoteGitVersion !== "") &&
            (data.advancedView.RemoteGitVersion !== data.advancedView.LocalGitVersion)) {
            updatesAvailable = 1;
        }

        var localVer = "<a target='host_" + ip + "' href='" + wrapUrlWithProxy(ip, '/about.php') + "' target='_blank' ip='" + ip + "'><b><font color='";
        if (updatesAvailable) {
            localVer += 'red';
        } else if ((typeof (data.advancedView.RemoteGitVersion) !== 'undefined') &&
                   (data.advancedView.RemoteGitVersion == data.advancedView.LocalGitVersion)) {
            localVer += 'darkgreen';
        } else {
            // Unknown or can't tell if up to date or not for some reason
            localVer += 'blue';
        }
        localVer += "'>" + data.advancedView.LocalGitVersion + "</font></b></a>";

        return localVer;
    }

    var ipRows = new Object();

    var refreshTimer = null;
    var geniusRefreshTimer = null;
    var wledRefreshTimer = null;
    var falconRefreshTimer = null;
    function clearRefreshTimers() {
        clearTimeout(refreshTimer);
        clearTimeout(geniusRefreshTimer);
        clearTimeout(wledRefreshTimer);
        clearTimeout(falconRefreshTimer);
        refreshTimer = null;
        geniusRefreshTimer = null;
        wledRefreshTimer = null;
        falconRefreshTimer = null;
    }
    var unavailables = [];

	function getFPPSystemStatus(ipAddresses, refreshing = false) {
	ips = "";
	if (Array.isArray(ipAddresses)) {
        if (refreshTimer != null) {
            clearTimeout(refreshTimer);
            delete refreshTimer;
            refreshTimer = null;
        }
        ipAddresses.forEach(function(entry) {
            ips += "&ip[]=" + entry;
	   });
	} else {
        ips = "&ip[]=" + ipAddresses;
    }
    if (ips == "") {
        return;
    }
    $.get("api/system/status?type=FPP" + ips)
    .done(function(alldata) {
        systemStatusCache = alldata;
        jQuery.each(alldata, function(ip, data) {
			var status = 'Idle';
			var statusInfo = "";
			var elapsed = "";
			var files = "";
            if (data == null || data == "" || data == "null") {
                return;
            }

			if (data.status_name == 'playing') {
				status = 'Playing';

				elapsed = data.time_elapsed;

				if (data.current_sequence != "") {
					files += data.current_sequence;
					if (data.current_song != "")
						files += "<br>" + data.current_song;
				} else {
					files += data.current_song;
				}

                if (files != "")
                    status += ":<br>" + files;
			} else if (data.status_name == 'updating') {
				status = 'Updating';
			} else if (data.status_name == 'stopped') {
				status = 'Stopped';
			} else if (data.status_name == 'stopping gracefully') {
				status = 'Stopping Gracefully';
			} else if (data.status_name == 'stopping gracefully after loop') {
				status = 'Stopping Gracefully After Loop';
			} else if (data.status_name == 'paused') {
				status = 'Paused';
			} else if (data.status_name == 'testing') {
				status = 'Testing';
            } else if (data.status_name == 'unreachable') {
                unavailables[ip]++;
                status = "unreachable";
            } else if (data.status_name == 'password') {
                status = '<font color="red">Protected</font>';
			} else if (data.status_name == 'unknown') {
                status = '-';
            } else if (data.status_name == 'idle') {
				if (data.mode_name == 'remote') {
					if ((data.sequence_filename != "") ||
						(data.media_filename != "")) {
						status = 'Syncing';

						elapsed += data.time_elapsed;

						if (data.sequence_filename != "") {
							files += data.sequence_filename;
							if (data.media_filename != "")
								files += "<br>" + data.media_filename;
						} else {
							files += data.media_filename;
						}

                        if (files != "")
                            status += ":<br>" + files;
					}
				}
            } else {
                status = data.status_name;
            }
            if (data.status_name != 'unreachable') {
                unavailables[ip] = 0;
            }

			var rowID = "fpp_" + ip.replace(/\./g, '_');
            var hostRowKey = ip.replace(/\./g, '_');

            rowID = hostRows[hostRowKey];

            var curStatus = $('#' + rowID + '_status').html();
            if ((curStatus.substr(0, 9) != "Last Seen") &&
                (curStatus != '') &&
                (!refreshing)) {
                // Don't replace an existing status via a different IP
                return;
            }
            if (status == 'unreachable') {
                if (unavailables[ip] < 4) {
                    return;
                }
                $('#' + rowID + '_mode').html("<span class=\"warning-text\">Unreachable</span>");
            } else if (status != "") {
                $('#' + rowID + '_status').html(status);
                $('#' + rowID + '_mode').html(getFullMode(data));
            } else {
                $('#' + rowID + '_mode').html(getFullMode(data));
            }
            if (data.hasOwnProperty('wifi')) {
                var wifi_html = [];
                data.wifi.forEach(function(w) {
                    wifi_html.push('<span title="');
                    if (w.pct) {
                        wifi_html.push(w.pct + '%');
                        if (w.unit == 'dBm') {
                            wifi_html.push(' ' + w.level + 'dBm');
                        }
                    } else {
                        wifi_html.push(w.level + w.unit);
                    }

                    wifi_html.push('" class="wifi-icon wifi-');
                    wifi_html.push(w.desc);
                    wifi_html.push('"></span>');
                });
                if (wifi_html.length > 0) {
                    $('#' + rowID + "_ip").find(".wifi-icon").remove();
                    $(wifi_html.join('')).appendTo('td[ip="' + ip + '"]');
                }
            }

            if ($('#' + rowID).attr('ip') != ip)
                $('#' + rowID).attr('ip', ip);

			$('#' + rowID + '_elapsed').html(elapsed);

            if (data.warnings != null && data.warnings.length > 0) {
               $('#' + rowID + '_warnings').removeAttr('style'); // Remove 'display: none' style

               // Handle tablesorter bug not assigning same color to child rows
               if ($('#' + rowID).hasClass('odd'))
                   $('#' + rowID + '_warnings').addClass('odd');

               var wHTML = "";
               for(var i = 0; i < data.warnings.length; i++) {
                wHTML += "<span class='warning-text'>" + data.warnings[i] + "</span><br>";
               }
               $('#' + rowID + '_warningCell').html(wHTML);
            }
            rowSpanSet(rowID);

	    //Expert View Rows
	    if( data.hasOwnProperty('advancedView') && data.status_name !== 'unknown' && data.status_name !== 'unreachable' && data.status_name !== 'password') {
		    if (data.advancedView.hasOwnProperty('Platform')) {
			    $('#' + rowID + '_platform').html(data.advancedView.Platform);
	        }
		    if (data.advancedView.hasOwnProperty('Variant') && (data.advancedView.Variant != '')) {
			    $('#' + rowID + '_variant').html(data.advancedView.Variant);
	        }

                var updatesAvailable = 0;
                if ((typeof (data.advancedView.RemoteGitVersion) !== 'undefined') &&
                    (typeof (data.advancedView.LocalGitVersion) !== 'undefined') &&
                    (data.advancedView.RemoteGitVersion !== "Unknown") &&
                    (data.advancedView.RemoteGitVersion !== "") &&
                    (data.advancedView.RemoteGitVersion !== data.advancedView.LocalGitVersion)) {
                    updatesAvailable = 1;
                }
                if (data.advancedView.hasOwnProperty("backgroundColor") && data.advancedView.backgroundColor != "") {
                    $('#' + rowID).css('background', "#" + data.advancedView.backgroundColor);
                    $('#' + rowID + "_warnings").css('background', "#" + data.advancedView.backgroundColor);
                    $('#' + rowID).css('color', "#FFF");
                    $('#' + rowID + " a").css('color', "#989898");
                    $('#' + rowID + "_warnings .warning-text").css('color', "#FF8080");
                }
                if (data.advancedView.hasOwnProperty("RemoteGitVersion")) {
                    var u = "<table class='multiSyncVerboseTable'>";
                    u += "<tr><td>Local:</td><td id='" + rowID + "_localgitvers'>";
                    u += getLocalVersionLink(ip, data);
                    u += "</td></tr>" +
                        "<tr><td>Remote:</td><td id='" + rowID + "_remotegitvers'>" + data.advancedView.RemoteGitVersion + "</td></tr>" +
                        "<tr><td>Branch:</td><td id='" + rowID + "_gitbranch'>" + data.advancedView.Branch + "</td></tr>";

                    if ((typeof(data.advancedView.UpgradeSource) !== 'undefined') &&
                        (data.advancedView.UpgradeSource != 'github.com')) {
                        u += "<tr><td>Origin:</td><td id='" + rowID + "_origin'>" + data.advancedView.UpgradeSource + "</td></tr>";
                    } else {
                        u += "<span style='display: none;' id='" + rowID + "_origin'></span>";
                    }
                    u += "</table>";
                    $('#advancedViewGitVersions_' + rowID).html(u);
                }


                if (data.advancedView.OSVersion !== "") {
                    $('#' + rowID + '_osversionRow').show();
                    $('#' + rowID + '_osversion').html(data.advancedView.OSVersion);
                }
                if (data.advancedView.HostDescription !== "") {
                    var origDesc = $('#' + rowID).find('.hostDescriptionSM').html();
                    if (origDesc == '')
                        $('#' + rowID).find('.hostDescriptionSM').html(data.advancedView.HostDescription);
                }

                var u = "<table class='multiSyncVerboseTable'>";
                if (typeof (data.advancedView.Utilization) !== 'undefined') {
                    let diskHtml = "";
                    try {
                        let row = data.advancedView.Utilization.Disk;
                        for (const [type, data] of Object.entries(row)) {
                            let used = bytesToHuman(data["Total"] - data["Free"]);
                            let total = bytesToHuman(data["Total"]);
                            if (diskHtml == "") {
                                diskHtml += "<b>Disk Usage:</b> "
                            } else {
                                diskHtml += ", "
                            }
                            diskHtml += type + ": " + used + "/" + total;
                        }
                    } catch(error) {
                        // This feature may not exists on older devices
                    }

                    if (diskHtml == "") {
                        diskHtml = "<b>Disk Usage:</b> unknown";
                    }
                    if (data.advancedView.Utilization.hasOwnProperty("CPU")) {
                        u += "<tr><td>CPU:</td><td>" + Math.round(data.advancedView.Utilization.CPU) + "%</td></tr>";
                    }
                    if (data.advancedView.Utilization.hasOwnProperty("Memory")) {
                        u += "<tr><td>Mem:</td><td>" + Math.round(data.advancedView.Utilization.Memory) + "%</td></tr>";
                    }
                    if (data.advancedView.Utilization.hasOwnProperty("MemoryFree")) {
                        var fr = data.advancedView.Utilization.MemoryFree;
                        fr /= 1024;
                        u += "<tr><td>Free Mem:&nbsp;</td><td>" + Math.round(fr) + "K</td></tr>";
                    }
                    if (data.advancedView.hasOwnProperty("rssi")) {
                        var rssi = +data.advancedView.rssi;
                        var quality = 2 * (rssi + 100);

                        if (rssi <= -100)
                            quality = 0;
                        else if (rssi >= -50)
                            quality = 100;
                        u += "<tr><td>RSSI:</td><td>" + rssi + "dBm / " + quality + "%</td></tr>";
                    }
                    if (data.advancedView.Utilization.hasOwnProperty("Uptime")) {
                       var ut = data.advancedView.Utilization.Uptime;
                       if (typeof ut === "string" || ut instanceof String) {
                         ut = ut.replace(/ /, '&nbsp;');
                       } else {
                         ut /= 1000;
                         ut = Math.round(ut);

                         var min = Math.round(ut / 60);
                         var hours = Math.round(min / 60);
                         min = Math.round(min % 60);
                         min = min.toString();
                        if (min.length < 2) {
                            min = "0" + min;
                        }
                         ut = hours.toString() + ":" + min.toString();

                       }
                       u += "<tr><td>Up:&nbsp;</td><td>" + ut
                       u += ' <span class="multisync-utilization-more" data-bs-html="true" title="<span class=\'tooltipSpan\'>' + diskHtml + '<br><b>Uptime:</b> ' + ut;
                       u += '</span>">...</td></tr>'
                    }
               }
               u += "</table>";

                $('#advancedViewUtilization_' + rowID).html(u);
                SetupToolTips();
            }
            });

            if ($('.logRow:visible').length == 0)
                $('#fppSystems').trigger('update', true);

		}).always(function() {
			if (Array.isArray(ipAddresses) && $('#MultiSyncRefreshStatus').is(":checked")) {
				refreshTimer = setTimeout(function() {getFPPSystemStatus(ipAddresses, true);}, 2000);
            }
		});

        validateMultiSyncSettings();
	} // end of "api/system/status?ip=" + ips

    function ipLink(ip) {
        return "<a target='host_" + ip + "' href='" + wrapUrlWithProxy(ip, "/") + "' ip='" + ip + "'>" + ip + "</a>";
    }

    function parseFPPSystems(data) {
	$('#fppSystems').empty();
	rowSpans = [];

        var uniqueHosts = new Object();

        var fppIpAddresses = [];
        var wledIpAddresses = [];
        var geniusIpAddresses = [];
        var falconV4Addresses = [];
        var falconV3Addresses = [];

		var remotes = [];
		if ((settings['MultiSyncEnabled'] == '1') &&
            (settings['fppMode'] == 'player')) {
            if (typeof settings['MultiSyncRemotes'] === 'string') {
                var tarr = settings['MultiSyncRemotes'].split(',');
                for (var i = 0; i < tarr.length; i++) {
                    remotes[tarr[i]] = 1;

                    if ((tarr[i] == "255.255.255.255") &&
                        (!$('#MultiSyncBroadcast').is(":checked")))
                        $('#MultiSyncBroadcast').prop('checked', true).trigger('change');

                    if ((tarr[i] == "239.70.80.80") &&
                        (!$('#MultiSyncMulticast').is(":checked")))
                        $('#MultiSyncMulticast').prop('checked', true).trigger('change');
                }
            }

			$('.masterOptions').show();
		}

		for (var i = 0; i < data.length; i++) {
			var star = "";
			var ip = data[i].address;
            var hostDescription = "";

            if (ip.indexOf('169.254') == 0)
                continue;

            if ((settings.hasOwnProperty('MultiSyncHide10')) &&
                (settings['MultiSyncHide10'] == '1') &&
                (ip.indexOf('10.') == 0)) {
                continue;
            }

            if ((settings.hasOwnProperty('MultiSyncHide172')) &&
                (settings['MultiSyncHide172'] == '1') &&
                (ip.indexOf('172.') == 0)) {
                var parts = ip.split('.');
                var second = parseInt(parts[1]);
                if ((second >= 16) && (second <= 31)) {
                    continue;
                }
            }

            if ((settings.hasOwnProperty('MultiSyncHide192')) &&
                (settings['MultiSyncHide192'] == '1') &&
                (ip.indexOf('192.168.') == 0)) {
                continue;
            }

			var rowID = "fpp_" + ip.replace(/\./g, '_');
            var newHost = 1;
            var hostRowKey = ip.replace(/\./g, '_');

            var hostname = data[i].hostname;
            if (hostname == "") {
                hostname = ip;
            } else {
                var cleanHost = hostname.replace(/[^a-zA-Z0-9]/, '_');
                rowID = rowID + '_' + cleanHost;
            }
            var hostKey = hostname + '_' + data[i].version + '_' + data[i].fppModeString + '_' + data[i].channelRanges;
            hostKey = hostKey.replace(/[^a-zA-Z0-9]/, '_');

            hostRows[hostRowKey] = rowID;

            var hnSpanStyle = "";
            if (data[i].local) {
                hnSpanStyle = " style='font-weight:bold;'";
            } else {
                if ((settings['MultiSyncEnabled'] == '1') &&
                    (settings['fppMode'] == 'player') &&
                        (data[i].fppModeString == "remote"))
                {
                    star = "<input type='checkbox' class='syncCheckbox' name='" + data[i].address + "'";
                    if (typeof remotes[data[i].address] !== 'undefined') {
                        star += " checked";
                        delete remotes[data[i].address];
                    }
                    star += " onClick='updateMultiSyncRemotes(true);'>";
                }
            }
            if (uniqueHosts.hasOwnProperty(hostKey)) {
                rowID = uniqueHosts[hostKey];
                hostRows[hostRowKey] = rowID;
                ipRows[data[i].address] = rowID;

                $('#' + rowID + '_ip').append('<br>' + ipLink(data[i].address));

                $('#' + rowID).attr('ipList', $('#' + rowID).attr('ipList') + ',' + data[i].address);

                if (data[i].fppModeString == 'remote') {
                    $('#' + rowID + '_ip').append(star);
                }

                if (isFPP(data[i].typeId)) {
                    fppIpAddresses.push(ip);
                }
            } else {
                uniqueHosts[hostKey] = rowID;
                ipRows[data[i].address] = rowID;

                var fppMode = 'Player';
                if (data[i].fppModeString == 'bridge') {
                    fppMode = 'Bridge';
                } else if (data[i].fppModeString == 'master') {
                    fppMode = 'Master';
                } else if (data[i].fppModeString == 'remote') {
                    fppMode = 'Remote';
                } else if (data[i].fppModeString == 'unknown') {
                    fppMode = 'Unknown';
                }

                rowSpans[rowID] = 1;

		var ipTxt = data[i].local ? data[i].address : ipLink(data[i].address);

                if ((data[i].fppModeString == 'remote') && (star != ""))
                    ipTxt = "<small class='hostDescriptionSM'>Select IPs for Unicast Sync</small><br>" + ipTxt + star;

		var hostTxt = "<a href='http://" + hostname + ".local'>" + hostname + "</a>";
                if(data[i].address == hostname){
                    hostTxt = hostname;
                }

                var newRow = "<tr id='" + rowID + "' ip='" + data[i].address + "' ipList='" + data[i].address + "' class='systemRow'>" +
                    "<td class='hostnameColumn'><span id='fpp_" + ip.replace(/\./g,'_') + "_hostname'" +  hnSpanStyle +">" + hostTxt + "</span><br><small class='hostDescriptionSM' id='fpp_" + ip.replace(/\./g,'_') + "_desc'>"+ hostDescription +"</small></td>" +
                    "<td id='" + rowID + "_ip' ip='" + data[i].address + "'>" + ipTxt + "</td>" +
                    "<td><span id='" + rowID + "_platform'>" + data[i].type + "</span><br><small id='" + rowID + "_variant'>" + data[i].model + "</small><span class='hidden typeId'>" + data[i].typeId + "</span>"
                        + "<span class='hidden version'>" + data[i].version + "</span></td>" +
                    "<td id='" + rowID + "_mode'>" + fppMode + "</td>" +
                    "<td id='" + rowID + "_status'>Last Seen:<br>" + data[i].lastSeenStr + "</td>" +
                    "<td id='" + rowID + "_elapsed'></td>";

                var versionParts = data[i].version.split('.');
                var majorVersion = 0;
                if (data[i].version != 'Unknown')
                    majorVersion = parseInt(versionParts[0]);

                if ((isFPP(data[i].typeId))) {
                    newRow += "<td><table class='multiSyncVerboseTable'><tr><td>FPP:</td><td id='" + rowID + "_version'>" + data[i].version.replace('.x-master', '.x').replace(/-g[A-Za-z0-9]*/, '').replace('-dirty', "<br><a href='settings.php#settings-developer'>Modified</a>") + "</td></tr><tr><td>OS:</td><td id='" + rowID + "_osversion'></td></tr></table></td>";
                } else {
                    newRow += "<td id='" + rowID + "_version'>" + data[i].version + "</td>";
                }


                    newRow +=
                        "<td id='advancedViewGitVersions_" + rowID + "'></td>" +
                        "<td id='advancedViewUtilization_" + rowID + "'></td>";

                    newRow += "<td class='centerCenter'>";
                    if ((isFPP(data[i].typeId)) &&
                        (majorVersion >= 4))
                        newRow += "<input type='checkbox' class='remoteCheckbox largeCheckbox multisyncRowCheckbox' name='" + data[i].address + "'>";

                    newRow += "</td>";


                newRow = newRow + "</tr>";
                $('#fppSystems').append(newRow);

                var colspan = (advancedView === true) ? 9 : 7;

                newRow = "<tr id='" + rowID + "_warnings' class='tablesorter-childRow warning-row'><td colspan='" + colspan + "' id='" + rowID + "_warningCell'></td></tr>";
                $('#fppSystems').append(newRow);

                newRow = "<tr id='" + rowID + "_logs' style='display:none' class='logRow tablesorter-childRow'><td colspan='" + colspan + "' id='" + rowID + "_logCell'><table class='multiSyncVerboseTable' width='100%'><tr><td>Log:</td><td width='100%'><textarea id='" + rowID + "_logText' style='width: 100%;' rows='8' disabled></textarea></td></tr><tr><td></td><td><div class='right' id='" + rowID + "_doneButtons' style='display: none;'><input type='button' class='buttons' value='Restart FPPD' onClick='restartSystem(\"" + rowID + "\");' style='float: left;'><input type='button' class='buttons' value='Reboot' onClick='rebootRemoteFPP(\"" + rowID + "\", \"" + ip + "\");' style='float: left;'><input type='button' class='buttons' value='Close Log' onClick='$(\"#" + rowID +"_logs\").hide(); rowSpanSet(\"" + rowID + "\");'></div></td></tr></table></td></tr>";
                $('#fppSystems').append(newRow);

                if (isFPP(data[i].typeId)) {
                    fppIpAddresses.push(ip);
                } else if (isESPixelStick(data[i].typeId)) {
                    if ((majorVersion == 3) || (majorVersion == 0)) {
                        getESPixelStickBridgeStatus(ip);
                    } else {
                        fppIpAddresses.push(ip);
                    }
                } else if (isFalconV4(data[i].typeId)) {
                    falconV4Addresses.push(ip);
                } else if (isFalcon(data[i].typeId)) {
                    falconV3Addresses.push(ip);
                } else if (isWLED(data[i].typeId)) {
                    wledIpAddresses.push(ip);
                } else if (isGenius(data[i].typeId)) {
                    geniusIpAddresses.push(ip);
                }
            }
        }
        getFPPSystemStatus(fppIpAddresses, false);
        getWLEDControllerStatus(wledIpAddresses, false);
        getGeniusControllerStatus(geniusIpAddresses, false);
        getFalconControllerStatus(falconV3Addresses, falconV4Addresses, false);

        var extraRemotes = [];
        var origExtra = "";
        if (typeof settings['MultiSyncExtraRemotes'] === 'string') {
            origExtra = settings['MultiSyncExtraRemotes'];
            extraRemotes = origExtra.split(',');
        }
        for (var x in remotes) {
            if (!extraRemotes.includes(x)) {
                extraRemotes.push(x);
            }
        }
        extraRemotes.sort();
        var extras = extraRemotes.join(',');
        settings['MultiSyncExtraRemotes'] = extras;

        if (extras != '' && origExtra != extras) {
            <?php
if ($uiLevel >= 1) {
    ?>
            var inp = document.getElementById("MultiSyncExtraRemotes");
            if (inp) {
                $('#MultiSyncExtraRemotes').val(extras);
            }
            <?
}
?>
            SetSetting("MultiSyncExtraRemotes", extras, 0, 0);
        }

        $('#fppSystems').trigger('update', true);
	}

    var systemsList = [];
	function getFPPSystems() {
		if (streamCount) {
			alert("FPP Systems are being updated, you will need to manually refresh once these updates are complete.");
			return;
		}

		$('.masterOptions').hide();
		$('#fppSystems').html("<tr><td colspan=8 align='center'>Loading system list from fppd.</td></tr>");

        $.get('api/fppd/multiSyncSystems', function(data) {
            systemsList = data.systems;
            parseFPPSystems(data.systems);
		});
	}

var ESPSockets = {};
function parseESPixelStickConfig(ip, data) {
    var s = JSON.parse(data);
    var ips = ip.replace(/\./g, '_');

    $('#fpp_' + ips + '_desc').html(s.device.id);
}

function parseESPixelStickStatus(ip, data) {
    var s = JSON.parse(data);
    var ips = ip.replace(/\./g, '_');

    if (s.hasOwnProperty("status")) {
        s = s.status;
    }

    if (s.hasOwnProperty('system')) {
        if (s['system'].hasOwnProperty('hostname'))
            $('#fpp_' + ips + '_hostname').html(s.system.hostname);
    }

    var rssi = +s.system.rssi;
    var quality = 2 * (rssi + 100);

    if (rssi <= -100)
        quality = 0;
    else if (rssi >= -50)
        quality = 100;

    var date = new Date(+s.system.uptime);
    var uptime = '';

    uptime += Math.floor(date.getTime()/86400000) + " days, ";
    uptime += ("0" + date.getUTCHours()).slice(-2) + ":";
    uptime += ("0" + date.getUTCMinutes()).slice(-2) + ":";
    uptime += ("0" + date.getUTCSeconds()).slice(-2);

    var u = "<table class='multiSyncVerboseTable'>";
    u += "<tr><td>RSSI:</td><td>" + rssi + "dBm / " + quality + "%</td></tr>";
    u += "<tr><td>Uptime:</td><td>" + uptime + "</td></tr>";
    u += "</table>";

    $('#advancedViewUtilization_fpp_' + ips).html(u);

    var mode = $('#fpp_' + ips + '_mode').html();

    if (mode == 'Bridge') {
        st = 'Bridging';
        if (s.hasOwnProperty('e131')) {
            st = "<table class='multiSyncVerboseTable'>";
            st += "<tr><td>Tot Pkts:</td><td>" + s.e131.num_packets + "</td></tr>";
            st += "<tr><td>Seq Errs:</td><td>" + s.e131.seq_errors + "</td></tr>";
            st += "<tr><td>Pkt Errs:</td><td>" + s.e131.packet_errors + "</td></tr>";
            st += "</table>";
        } else if (s.hasOwnProperty('input')) {
            for (var i = 0; i < s.input.length; i++) {
                if (s.input[i].hasOwnProperty('e131')) {
                    st = "<table class='multiSyncVerboseTable'>";
                    st += "<tr><td>Tot Pkts:</td><td>" + s.input[i].e131.num_packets + "</td></tr>";
                    st += "<tr><td>Pkt Errs:</td><td>" + s.input[i].e131.packet_errors + "</td></tr>";
                    st += "</table>";
                }
            }
        }

        $('#fpp_' + ips + '_status').html(st);
    }

    if ($('#MultiSyncRefreshStatus').is(":checked")) {
        setTimeout(function() {ESPSockets[ips].send("XJ");}, 1000);
    }
}

function parseESPixelStickVersion(ip, data) {
    var s = JSON.parse(data);
    var ips = ip.replace(/\./g, '_');

    if (s.hasOwnProperty('version')) {
        $('#fpp_' + ips + '_version').html(s.version);
        $('#fpp_' + ips).find('.version').html(s.version);
        var versionParts = s['version'].split('.');
    }
}

function parseESPixelStickCommandResponse(ip, data) {
    var s = JSON.parse(data);
    var ips = ip.replace(/\./g, '_');

    if ((s.hasOwnProperty('get')) &&
        (s.get.hasOwnProperty('device')) &&
        (s.get.device.hasOwnProperty('id'))) {
        $('#fpp_' + ips + '_desc').html(s.get.device.id);
    }
}

function getESPixelStickBridgeStatus(ip) {
    var ips = ip.replace(/\./g, '_');

    if (ESPSockets.hasOwnProperty(ips)) {
        ESPSockets[ips].send("XJ");
    } else {
        var ws = new WebSocket("ws://" + ip + "/ws");
        ESPSockets[ips] = ws;

        ws.binaryType = "arraybuffer";
        ws.onopen = function() {
            ws.send("G1");
            ws.send("G2");
            ws.send("XA"); // ESPixelStick v4.x
            ws.send("XJ");
            ws.send('{"cmd":{"get":"device"}}'); // ESPixelStick v4.x
        };

        ws.onmessage = function(e) {
            if ("string" == typeof e.data) {
                var t = e.data.substr(0, 2)
                  , n = e.data.substr(2);
                switch (t) {
                    case "XA":
                    case "G2":
                        parseESPixelStickVersion(ip, n);
                        break;
                    case "G1":
                        parseESPixelStickConfig(ip, n);
                        break;
                    case "XJ":
                        parseESPixelStickStatus(ip, n);
                        break;
                    case '{"':
                        parseESPixelStickCommandResponse(ip, e.data);
                        break;
                }
            }
        };

        ws.onclose = function() {
            delete ESPSockets[ips];
        };
    }
}


function wrapUrlWithProxy(ip, path) {
    <?if (!$settings['hideExternalURLs']) {?>
    if (isProxied(ip)) {
        return 'proxy/' + ip + path;
    }
    return 'http://' + ip + path;
    <?} else {?>
    return "";
    <?}?>
}

function getFalconControllerStatus(fv3ips, fv4ips, refreshing = false) {
    if (falconRefreshTimer != null) {
    clearTimeout(falconRefreshTimer);
        delete falconRefreshTimer;
        falconRefreshTimer = null;
    }

    ips3 = "";
	if (Array.isArray(fv3ips)) {
        fv3ips.forEach(function(entry) {
            ips3 += "&ip[]=" + entry;
	    });
	} else {
        ips3 = "&ip[]=" + fv3ips;
    }

    ips4 = "";
	if (Array.isArray(fv4ips)) {
        fv4ips.forEach(function(entry) {
            ips4 += "&ip[]=" + entry;
	    });
	} else {
        ips4 = "&ip[]=" + fv4ips;
    }

    if (ips3 != "") {
        $.get("api/system/status?type=FV3" + ips3)
        .done(function(alldata) {
            jQuery.each(alldata, function(ip, data) {
                var ips = ip.replace(/\./g, '_');
                var u = "<table class='multiSyncVerboseTable'>";
                u += "<tr><td>Uptime:</td><td>" + data['u'] + "</td></tr>";
                u += "<tr><td>V1 Voltage:</td><td> " + data['v1'] + "</td></tr>";
                u += "<tr><td>V2 Voltage:</td><td> " + data['v2'] + "</td></tr>";

                u += "</table>";

                $('#advancedViewUtilization_fpp_' + ips).html(u);

                var mode = $('#fpp_' + ips + '_mode').html();

                if (mode == 'Bridge') {
                    $('#fpp_' + ips + '_status').html('Bridging');
                } else {
                    $('#fpp_' + ips + '_status').html('');
                }
            });
            if ($('#MultiSyncRefreshStatus').is(":checked")) {
			    falconRefreshTimer = setTimeout(function() {getFalconControllerStatus(fv3ips, fv4ips, true);}, 2000);
            }

        });
    }
    if (ips4 != "") {
        $.get("api/system/status?type=FV4" + ips4)
        .done(function(alldata) {
            jQuery.each(alldata, function(ip, s) {
                var ips = ip.replace(/\./g, '_');

                var tempthreshold = s.P.BS;
                var t1temp = s.P.T1 / 10;
                var t2temp = s.P.T2 / 10;

                var v1voltage = s.P.V1 / 10;
                var v2voltage = s.P.V2 / 10;

                var testmode = new Boolean(s.P.TS);
                var overtemp = new Boolean(Math.max(t1temp,t2temp) > tempthreshold);

                var t=parseInt(s.P.U);
                var days=Math.floor(t/86400);
                var hours=Math.floor((t-86400*days)/3600);
                var mins=Math.floor((t-86400*days-3600*hours)/60);

                var uptime = '';

                uptime += (days + " days, ");
                uptime += ("0" + hours).slice(-2) + ":";
                uptime += ("0" + mins ).slice(-2);

                var u = "<table class='multiSyncVerboseTable'>";
                u += "<tr><td>Uptime:</td><td>" + uptime + "</td></tr>";
                u += "<tr><td>V1 Voltage:</td><td> " + v1voltage + "v</td></tr>";
                if (s.P.BR != 48) {
                    u += "<tr><td>V2 Voltage:</td><td> " + v2voltage + "v</td></tr>";
                }
                u += "<tr><td>Temp:</td><td> " + t1temp + "C</td></tr>";
                u += "</table>";

                $('#advancedViewUtilization_fpp_' + ips).html(u);
                $('#fpp_' + ips + '_status').html(s.status_name);

                if (testmode == true || overtemp == true) {
                    $('#fpp_' + ips + '_warnings').removeAttr('style'); // Remove 'display: none' style
                    // Handle tablesorter bug not assigning same color to child rows
                    if ($('#fpp_' + ips).hasClass('odd'))
                        $('#fpp_' + ips + '_warnings').addClass('odd');

                        var wHTML = "";
                        if (testmode == true) wHTML += "<span class='warning-text'>Controller Test mode is active</span><br>";
                        if (overtemp == true) wHTML += "<span class='warning-text'>Pixel brightness reduced due to high temperatures</span><br>";

                       $('#fpp_' + ips + '_warningCell').html(wHTML);
                }
            });
            if ($('#MultiSyncRefreshStatus').is(":checked")) {
			    falconRefreshTimer = setTimeout(function() {getFalconControllerStatus(fv3ips, fv4ips, true);}, 2000);
            }
        });
    }
}

function getWLEDControllerStatus(ipAddresses, refreshing = false) {
	ips = "";
	if (Array.isArray(ipAddresses)) {
        if (wledRefreshTimer != null) {
            clearTimeout(wledRefreshTimer);
            delete wledRefreshTimer;
            wledRefreshTimer = null;
        }
        ipAddresses.forEach(function(entry) {
            ips += "&ip[]=" + entry;
	    });
	} else {
        ips = "&ip[]=" + ipAddresses;
    }
    if (ips == "") {
        return;
    }
    $.get("api/system/status?type=WLED" + ips)
    .done(function(alldata) {
        jQuery.each(alldata, function(ip, data) {
            if (data == null || data == "" || data == "null") {
                return;
            }
            var ips = ip.replace(/\./g, '_');
            var rssi = data.wifi.rssi;
            var quality = data.wifi.signal;

            var t=parseInt(data.uptime);
            var days=Math.floor(t/86400);
            var hours=Math.floor((t-86400*days)/3600);
            var mins=Math.floor((t-86400*days-3600*hours)/60);

            var uptime = '';

            uptime += (days + " days, ");
            uptime += ("0" + hours).slice(-2) + ":";
            uptime += ("0" + mins ).slice(-2);

            var u = "<table class='multiSyncVerboseTable'>";
            u += "<tr><td>RSSI:</td><td>" + rssi + "dBm / " + quality + "%</td></tr>";
            u += "<tr><td>Uptime:</td><td>" + uptime + "</td></tr>";
            u += "</table>";

            $('#advancedViewUtilization_fpp_' + ips).html(u);
            $('#fpp_' + ips + '_status').html(data.status_name);
        });

        if (Array.isArray(ipAddresses) && $('#MultiSyncRefreshStatus').is(":checked")) {
			wledRefreshTimer = setTimeout(function() {getWLEDControllerStatus(ipAddresses, true);}, 2000);
        }
    });
}

function getGeniusControllerStatus(ipAddresses, refreshing = false) {
	ips = "";
	if (Array.isArray(ipAddresses)) {
        if (geniusRefreshTimer != null) {
            clearTimeout(geniusRefreshTimer);
            delete geniusRefreshTimer;
            geniusRefreshTimer = null;
        }
        ipAddresses.forEach(function(entry) {
            ips += "&ip[]=" + entry;
	    });
	} else {
        ips = "&ip[]=" + ipAddresses;
    }
    if (ips == "") {
        return;
    }
    $.get("api/system/status?type=Genius" + ips)
    .done(function(alldata) {
        jQuery.each(alldata, function(ip, data) {
            if (data == null || data == "" || data == "null") {
                return;
            }
            var ips = ip.replace(/\./g, '_');
            var t = data.system.uptime_seconds;
            var days = Math.floor(t/86400);
            var hours = Math.floor((t-86400*days)/3600);
            var mins = Math.floor((t-86400*days-3600*hours)/60);

            var uptime = '';
            uptime += (days + " days, ");
            uptime += ("0" + hours).slice(-2) + ":";
            uptime += ("0" + mins ).slice(-2);

            var u = "<table class='multiSyncVerboseTable'>";
           //u += "<tr><td>RSSI:</td><td>" + rssi + "dBm / " + quality + "%</td></tr>";
            u += "<tr><td>Uptime:</td><td>" + uptime + "</td></tr>";
            u += "</table>";
            $('#advancedViewUtilization_fpp_' + ips).html(u);

            var origDesc = $('#fpp_' + ips + '_desc').html();
            if (origDesc == '') {
                $('#fpp_' + ips + '_desc').html(data.system.friendly_name);
            }
            $('#fpp_' + ips + '_status').html(data.status_name);
        });

        if (Array.isArray(ipAddresses) && $('#MultiSyncRefreshStatus').is(":checked")) {
			geniusRefreshTimer = setTimeout(function() {getGeniusControllerStatus(ipAddresses, true);}, 2000);
        }
    });
}

function RefreshStats() {
    var keys = Object.keys(hostRows);
    var ips = [];
    var gips = [];
    var wips = [];
    var fv3ips = [];
    var fv4ips = [];

    for (var i = 0; i < keys.length; i++) {
        var rowID = hostRows[keys[i]];
        var ip = ipFromRowID(rowID);
        var mode = $('#' + rowID + '_mode').html();

        var typeId = $('#' + rowID).find('.typeId').html();
        var version = $('#' + rowID).find('.version').html();
        if (isFPP(typeId)) {
            ips.push(ip);
        } else if (isESPixelStick(typeId)) {
            var versionParts = version.split('.');
            var majorVersion = parseInt(versionParts[0]);
            if (majorVersion == 3) {
                getESPixelStickBridgeStatus(ip);
            } else {
                ips.push(ip);
            }
        } else if (isFalcon(typeId)) {
            fv3ips.push(ip);
        } else if (isFalconV4(typeId)) {
            fv4ips.push(ip);
        } else if (isWLED(typeId)) {
            wips.push(ip);
        } else if (isGenius(typeId)) {
            gips.push(ip);
        }
    }
    getFPPSystemStatus(ips, true);
    getGeniusControllerStatus(gips, true);
    getWLEDControllerStatus(wips, true);
    getFalconControllerStatus(fv3ips, fv4ips, true);
}

function autoRefreshToggled() {
	if ($('#MultiSyncRefreshStatus').is(":checked")) {
        RefreshStats();
	}
}

function reloadMultiSyncPage() {
	if (streamCount) {
		alert("FPP Systems are being updated, you will need to manually refresh once these updates are complete.");
		return;
	}

	reloadPage();
}

function syncModeUpdated(setting = '') {
    var multicastChecked = $('#MultiSyncMulticast').is(":checked");
    var broadcastChecked = $('#MultiSyncBroadcast').is(":checked");

    if (setting == 'MultiSyncMulticast') {
        if (multicastChecked && broadcastChecked)
            $('#MultiSyncBroadcast').prop('checked', false).trigger('change');
    } else if (setting == 'MultiSyncBroadcast') {
        if (multicastChecked && broadcastChecked)
            $('#MultiSyncMulticast').prop('checked', false).trigger('change');
    }

    var anyUnicast = 0;
    $('input.syncCheckbox').each(function() {
        if ($(this).is(":checked")) {
            anyUnicast = 1;
        }
    });

    if (!anyUnicast && !multicastChecked && !broadcastChecked) {
        $('#MultiSyncMulticast').prop('checked', true).trigger('change');
        alert('FPP will use multicast if no other sync methods are chosen.');
    }
}

function IPsCanTalk(ip1, ip2, octets) {
    var p1 = ip1.split('.');
    var p2 = ip2.split('.');

    switch (octets) {
        case 3: if ((p1[0] == p2[0]) && (p1[1] == p2[1]) && (p1[2] == p2[2]))
                    return true;
                break;
        case 2: if ((p1[0] == p2[0]) && (p1[1] == p2[1]))
                    return true;
                break;
        case 1: if (p1[0] == p2[0])
                    return true;
                break;
    }

    return false;
}

function getReachableIPFromRowID(id) {
    var ip = ipFromRowID(id);
    var ipListStr = $('#' + id).attr('ipList');

    if (ip == ipListStr)
        return ip;

    var ipList = ipListStr.split(',');

    for (var o = 3; o > 0; o--) {
        for (var i = 0; i < systemsList.length; i++) {
            if (systemsList[i].local == 1) {
                for (var j = 0; j < ipList.length; j++) {
                    if (IPsCanTalk(systemsList[i].address, ipList[j], o))
                        return ipList[j];
                }
            }
        }
    }

    return '';
}

function ipFromRowID(id) {
    ip = $('#' + id).attr('ip');

    return ip;
}
function ipOrHostnameFromRowID(id) {
<?if ($_SERVER['SERVER_NAME'] != $_SERVER['SERVER_ADDR']) {?>
    // Hitting the FPP instance via Hostname, not Ip address.  Thus, we need to use
    // hostnames for the remotes as well or CORS will trigger
    var ip = $('#' + id + "_hostname").html();
    if (ip == "") {
        ip = $('#' + id).attr('ip');
    }
<?} else {?>
    var ip = $('#' + id).attr('ip');
<?}?>
    return ip;
}

var streamCount = 0;
function EnableDisableStreamButtons() {
    if (streamCount) {
        $('#performActionButton').prop("disabled", true);
        $('#refreshButton').prop("disabled", true);

        if (!$('#fppSystemsTableWrapper').hasClass('fppTableWrapperHighlighted')) {
            $('#fppSystemsTableWrapper').addClass('fppTableWrapperHighlighted');
            $('#exitWarning').show();
        }
    } else {
        $('#performActionButton').prop("disabled", false);
        $('#refreshButton').prop("disabled", false);
        $('#fppSystemsTableWrapper').removeClass('fppTableWrapperHighlighted');
        $('#exitWarning').hide();
    }
}

function upgradeDone(id) {
    id = id.replace('_logText', '');
    $('#' + id + '_doneButtons').show();
    streamCount--;

    var ip = ipFromRowID(id);
    setTimeout(function() { getFPPSystemStatus(ip, true); }, 3000);

    if (origins.hasOwnProperty(ip)) {
        for (var i = 0; i < origins[ip].length; i++) {
			var rowID = "fpp_" + origins[ip][i].replace(/\./g, '_');
            upgradeSystem(rowID);
        }
    }

    EnableDisableStreamButtons();
}

function upgradeFailed(id) {
    id = id.replace('_logText', '');
    var ip = ipFromRowID(id);

    alert('Update failed for FPP system at ' + ip);
    streamCount--;

    EnableDisableStreamButtons();

    if (!$('#fppSystemsTableWrapper').hasClass('fppTableWrapperErrored'))
        $('#fppSystemsTableWrapper').addClass('fppTableWrapperErrored');
}

function showLogsRow(rowID) {
    // Handle tablesorter bug not assigning same color to child rows
    if ($('#' + rowID).hasClass('odd'))
        $('#' + rowID + '_logs').addClass('odd');

    $('#' + rowID + '_logs').show();
    rowSpanSet(rowID);
}

function addLogsDivider(rowID) {
    if ($('#' + rowID + '_logText').val() != '')
        $('#' + rowID + '_logText').val($('#' + rowID + '_logText').val() + '\n==================================\n');
}

function upgradeSystemByHostname(id) {
    id = id.replace('_logText', '');
    var ip = ipOrHostnameFromRowID(id);
    StreamURL(wrapUrlWithProxy(ip, '/manualUpdate.php?wrapped=1'), id + '_logText', 'upgradeDone', 'upgradeFailed');
}

function upgradeSystem(rowID) {
    $('#' + rowID).find('input.remoteCheckbox').prop('checked', false);

    streamCount++;
    EnableDisableStreamButtons();

    showLogsRow(rowID);
    addLogsDivider(rowID);

    var ip = ipFromRowID(rowID);
    StreamURL(wrapUrlWithProxy(ip, '/manualUpdate.php?wrapped=1'), rowID + '_logText', 'upgradeDone', 'upgradeSystemByHostname');
}

function showWaitingOnOriginUpdate(rowID, origin) {
    showLogsRow(rowID);
    addLogsDivider(rowID);

    $('#' + rowID + '_logText').append("Waiting for origin (" + origin + ") to finish updating...");
}

var origins = {};
function upgradeSelectedSystems() {
    $origins = {};
	$('input.remoteCheckbox').each(function() {
		if ($(this).is(":checked")) {
            var rowID = $(this).closest('tr').attr('id');
            if ($('#' + rowID).hasClass('filtered')) {
                return true;
            }
            var origin = $('#' + rowID + '_origin').html();
            if (typeof origin === 'undefined') {
                var msg =  "Invalid Origin for " + ipFromRowID(rowID) + ". Skipping";
                $.jGrowl(msg, { themeState: 'danger' });
                return;
            }
            var originRowID = "fpp_" + origin.replace(/\./g, '_');
            if ((origin != '') &&
                (origin != 'github.com') &&
                ($('#' + originRowID).find('input.remoteCheckbox').is(':checked'))) {
                if (!origins.hasOwnProperty(origin)) {
                    origins[origin] = [];
                }
                origins[origin].push(ipFromRowID(rowID));
            }
        }
    });

    var originsUpdating = {};
	$('input.remoteCheckbox').each(function() {
		if ($(this).is(":checked")) {
            var rowID = $(this).closest('tr').attr('id');
            if ($('#' + rowID).hasClass('filtered')) {
                return true;
            }
            var ip = ipFromRowID(rowID);

            if (origins.hasOwnProperty(ip)) {
                originsUpdating[ip] = 1;
            }
        }
    });

	$('input.remoteCheckbox').each(function() {
		if ($(this).is(":checked")) {
            var rowID = $(this).closest('tr').attr('id');
            if ($('#' + rowID).hasClass('filtered')) {
                return true;
            }

            var origin = $('#' + rowID + '_origin').html();
            if (typeof origin === 'undefined') {
                var msg  = "Invalid Origin for " + ipFromRowID(rowID) + ". Skipping";
                $.jGrowl(msg, { themeState: 'danger' });
                return;
            }
            var originRowID = "fpp_" + origin.replace(/\./g, '_');
            if ((origin == '') ||
                (origin == 'github.com') ||
                (!originsUpdating.hasOwnProperty(origin))) {
                upgradeSystem(rowID);
            } else {
                showWaitingOnOriginUpdate(rowID, origin);
            }
        }
    });
}

function actionDone(id) {
    id = id.replace('_logText', '');
    $('#' + id + '_doneButtons').show();
    streamCount--;

    var ip = ipFromRowID(id);

    setTimeout(function() { getFPPSystemStatus(ip, true); }, 1500);

    EnableDisableStreamButtons();
}

function actionFailed(id) {
    id = id.replace('_logText', '');
    var ip = ipFromRowID(id);

    alert('Action failed for FPP system at ' + ip);

    streamCount--;

    EnableDisableStreamButtons();
}

function restartSystem(rowID) {
    streamCount++;
    EnableDisableStreamButtons();

    showLogsRow(rowID);
    addLogsDivider(rowID);

    var ip = ipFromRowID(rowID);
    StreamURL('restartRemoteFPPD.php?ip=' + ip, rowID + '_logText', 'actionDone', 'actionFailed');
}
function setSystemMode(rowID, mode) {
    streamCount++;
    EnableDisableStreamButtons();

    showLogsRow(rowID);
    addLogsDivider(rowID);

    var ip = ipFromRowID(rowID);
    StreamURL('restartRemoteFPPD.php?ip=' + ip + '&mode=' + mode, rowID + '_logText', 'actionDone', 'actionFailed');
}

function restartSelectedSystems() {
	$('input.remoteCheckbox').each(function() {
		if ($(this).is(":checked")) {
            var rowID = $(this).closest('tr').attr('id');
            if ($('#' + rowID).hasClass('filtered')) {
                return true;
            }

            $(this).prop('checked', false);
            restartSystem(rowID);
        }
    });
}


function setSelectedSystemsMode(mode) {
    $('input.remoteCheckbox').each(function() {
        if ($(this).is(":checked")) {
            var rowID = $(this).closest('tr').attr('id');
            if ($('#' + rowID).hasClass('filtered')) {
                return true;
            }

            $(this).prop('checked', false);
            setSystemMode(rowID, mode);
        }
    });
}

function rebootSystem(rowID) {
    streamCount++;
    EnableDisableStreamButtons();

    showLogsRow(rowID);
    addLogsDivider(rowID);

    var ip = ipFromRowID(rowID);
    StreamURL('rebootRemoteFPP.php?ip=' + ip, rowID + '_logText', 'actionDone', 'actionFailed');
}

function rebootSelectedSystems() {
    $('input.remoteCheckbox').each(function() {
        if ($(this).is(":checked")) {
            var rowID = $(this).closest('tr').attr('id');
            if ($('#' + rowID).hasClass('filtered')) {
                return true;
            }

            $(this).prop('checked', false);
            rebootSystem(rowID);
        }
    });
}
function shutdownSystem(rowID) {
    streamCount++;
    EnableDisableStreamButtons();

    showLogsRow(rowID);
    addLogsDivider(rowID);

    var ip = ipFromRowID(rowID);
    StreamURL('shutdownRemoteFPP.php?ip=' + ip, rowID + '_logText', 'actionDone', 'actionFailed');
}
function shutdownSelectedSystems() {
    $('input.remoteCheckbox').each(function() {
        if ($(this).is(":checked")) {
            var rowID = $(this).closest('tr').attr('id');
            if ($('#' + rowID).hasClass('filtered')) {
                return true;
            }

            $(this).prop('checked', false);
            shutdownSystem(rowID);
        }
    });
}

function changeBranch(rowID) {
    streamCount++;
    EnableDisableStreamButtons();

    showLogsRow(rowID);
    addLogsDivider(rowID);

    var ip = ipFromRowID(rowID);
    var branch = $("#branchSelect").val();
    StreamURL('changeRemoteBranch.php?branch=' + branch + '&ip=' + ip, rowID + '_logText', 'actionDone', 'actionFailed');
}
function changeBranchSelectedSystems() {
	$('input.remoteCheckbox').each(function() {
		if ($(this).is(":checked")) {
            var rowID = $(this).closest('tr').attr('id');
            if ($('#' + rowID).hasClass('filtered')) {
                return true;
            }

            $(this).prop('checked', false);
            changeBranch(rowID);
        }
    });
}

function copyFilesToSystem(rowID) {
    streamCount++;
    EnableDisableStreamButtons();

    showLogsRow(rowID);
    addLogsDivider(rowID);

    var ip = getReachableIPFromRowID(rowID);
    if (ip == "") {
        ip = ipFromRowID(rowID);
        $('#' + rowID + '_logText').append('No IPs appear reachable for ' + ip);
    } else {
        StreamURL('copyFilesToRemote.php?ip=' + ip, rowID + '_logText', 'copyDone', 'copyFailed');
    }
}

function copyFileToSystem(file, rowID) {
    EnableDisableStreamButtons();
    streamCount++;

    showLogsRow(rowID);
    addLogsDivider(rowID);

    var ip = getReachableIPFromRowID(rowID);
    if (ip == "") {
        ip = ipFromRowID(rowID);
        $('#' + rowID + '_logText').append('No IPs appear reachable for ' + ip);
    } else {
        StreamURL('remotePush.php?dir=uploads&raw=1&filename=' + file.name + '&remoteHost=' + ip, rowID + '_logText', 'copyDone', 'copyFailed');
    }
}

function copyFilesToSelectedSystems() {
	$('input.remoteCheckbox').each(function() {
		if ($(this).is(":checked")) {
            var rowID = $(this).closest('tr').attr('id');
            if ($('#' + rowID).hasClass('filtered')) {
                return true;
            }

            $(this).prop('checked', false);
            copyFilesToSystem(rowID);
        }
    });
}

function copyOSFilesToSelectedSystems() {
    let files=[];
    let targets=[];
    $("#copyOSOptionsDetails input").each(function() {
		if ($(this).is(":checked")) {
            let name = $(this).attr('id');
            localFpposFiles.forEach(function(f){
                if (name === f.name) {
                    files.push(f);
                }
            });
        }
    });

    $('input.remoteCheckbox').each(function() {
		if ($(this).is(":checked")) {
            var rowID = $(this).closest('tr').attr('id');
            if ($('#' + rowID).hasClass('filtered')) {
                return true;
            }

            $(this).prop('checked', false);
            targets.push(rowID);
        }
    });

    if (files.length == 0 ) {
        $.jGrowl("No OS Upgrade files were selected",{themeState:'danger'});
    } else if (targets.length == 0) {
        $.jGrowl("No remote FPP systems were selected",{themeState:'danger'});
    } else {
        targets.forEach(function(rowID) {
            files.forEach(function(f) {
                ip = ipFromRowID(rowID);
                osType = "UNKNOWN";
                freeSpace = 0;
                if (systemStatusCache.hasOwnProperty(ip)) {
                    rec = systemStatusCache[ip];
                    try {
                        let platform = rec["advancedView"]["Platform"];
                        if (platform == "BeagleBone Black") {
                            osType = "BBB";
                        } else if (platform == "Raspberry Pi") {
                            osType = "PI";
                        }
                    } catch(error) {
                        // Eat it
                    }

                    try {
                        freeSpace = rec.advancedView.Utilization.Disk.Media.Free
                    } catch(error) {
                        // Eat it
                    }

                    // 0.95 is to not completely fill it.
                    if (freeSpace != 0 && ( (f.sizeBytes) > (freeSpace *0.95) )) {
                        $.jGrowl("Insufficient free disk space on remote " + ip + ". Skipping",{themeState:'danger'});
                    } else if (osType != f.type) {
                        let msg = f.name + " isn't compatible with " + ip + ". Skipping";
                        $.jGrowl(msg, {themeState:'danger'});
                    } else {
                        copyFileToSystem(f, rowID);
                    }

                } else {
                    $.jGrowl("Unable to locate data for " + ip + ". Skipping",{themeState:'danger'});
                }

            });
        });
    }
}

function copyDone(id) {
    id = id.replace('_logText', '');
    $('#' + id + '_doneButtons').show();
    streamCount--;

    EnableDisableStreamButtons();
}

function copyFailed(id) {
    id = id.replace('_logText', '');
    var ip = ipFromRowID(id);

    alert('File Copy failed for FPP system at ' + ip);
    streamCount--;

    EnableDisableStreamButtons();

    if (!$('#fppSystemsTableWrapper').hasClass('fppTableWrapperErrored'))
        $('#fppSystemsTableWrapper').addClass('fppTableWrapperErrored');
}

function addProxyForIP(rowID) {
    var ip = ipFromRowID(rowID);
    		$.post("api/proxies/" + ip,  "AddProxy").done(function(data) {
		}).fail(function(data) {
			DialogError("Failed to set Proxy", "Post failed");
		});
}

function proxySelectedIPs() {
    $('input.remoteCheckbox').each(function() {
        if ($(this).is(":checked")) {
            var rowID = $(this).closest('tr').attr('id');
            if ($('#' + rowID).hasClass('filtered')) {
                return true;
            }

            $(this).prop('checked', false);
            addProxyForIP(rowID);
        }
    });
}

function clearSelected() {
    // clear all entries, even if filtered
    $('input.remoteCheckbox').prop('checked', false);
}

function selectAll() {
    // select all entries that are not filtered out of view
    $('input.remoteCheckbox').each(function() {
        var rowID = $(this).closest('tr').attr('id');
        if (!$('#' + rowID).hasClass('filtered')) {
            $(this).prop('checked', true);
        }
    });
}

function selectAllChanged() {
    if ($('#selectAllCheckbox').is(":checked")) {
        selectAll();
    } else {
        clearSelected();
    }
}

function closeAllLogs() {
    $('.systemRow').each(function() {
        var rowID = $(this).attr('id');
        $('#' + rowID + '_logs').hide();
        rowSpanSet(rowID);
    });
}

function performMultiAction() {
    var action = $('#multiAction').val();

    switch (action) {
        case 'upgradeFPP':     upgradeSelectedSystems();        break;
        case 'restartFPPD':    restartSelectedSystems();        break;
        case 'copyFiles':      copyFilesToSelectedSystems();    break;
        case 'copyOSFiles':    copyOSFilesToSelectedSystems();  break;
        case 'reboot':         rebootSelectedSystems();         break;
        case 'shutdown':       shutdownSelectedSystems();       break;
        case 'remoteMode':     setSelectedSystemsMode('remote');     break;
        case 'playerMode':     setSelectedSystemsMode('player');     break;
        case 'addProxy':       proxySelectedIPs();                    break;
        case 'changeBranch':   changeBranchSelectedSystems();   break;
        default:               alert('You must select an action first.'); break;
    }

    $('#selectAllCheckbox').prop('checked', false);
}

function multiActionChanged() {
    var action = $('#multiAction').val();

    $('.actionOptions').hide();

    switch (action) {
        case 'copyFiles':      $('#copyOptions').show();      break;
        case 'copyOSFiles' :   $('#copyOSOptions').show();    break;
        case 'changeBranch' :   $('#changeBranchOptions').show();    break;
    }
}

</script>
</head>
<body>
<div id="bodyWrapper">
	<?php
$activeParentMenuItem = 'status';
include 'menu.inc';?>
    <div class="mainContainer">
    <h1 class="title">FPP MultiSync</h1>
        <div class="pageContent">
        	<div id="uifppsystems" class="settings">
                    <div id='fppSystemsTableWrapper' class='fppTableWrapper fppTableWrapperAsTable backdrop'>
                        <div class='fppTableContents' role="region" aria-labelledby="fppSystemsTable" tabindex="0">
        			<table id='fppSystemsTable' cellpadding='3'>
        				<thead>
        					<tr>
        						<th class="hostnameColumn" data-placeholder="Hostname">Hostname</th>
        						<th data-placeholder="IP Address">IP Address</th>
        						<th >Platform</th>
        						<th >Mode</th>
        						<th data-placeholder="Status">Status</th>
        						<th data-sorter='false' data-filter='false'>Elapsed</th>
        						<th data-placeholder="Version">Version</th>

                                <th data-sorter='false' data-filter='false'>Git Versions</th>
                                <th data-sorter='false' data-filter='false'>Utilization</th>
                                <th data-sorter='false' data-filter='false'><input id='selectAllCheckbox' type='checkbox' class='largeCheckbox multisyncRowCheckbox' onChange='selectAllChanged();' /></th>

                        </tr>
                    </thead>
                    <tbody id='fppSystems'>
                        <tr><td colspan=8 align='center'>Loading system list from fppd.</td></tr>
                    </tbody>
                </table>
            </div>
        </div>
	<div class='container-fluid settingsTable settingsGroupTable'>
        <?PrintSetting('MultiSyncEnabled');?>
        <?PrintSetting('MultiSyncRefreshStatus', 'autoRefreshToggled');?>
        </div>

        <div class="multisyncAdvancedFormActions row">
            <div class="form-actions col-md">
            <button class="fppSystemsUiSettingsToggle buttons dropdown-toggle"  type="button"data-bs-toggle="collapse" data-bs-target="#fppSystemsUiSettingsDrawer" aria-expanded="false" aria-controls="fppSystemsUiSettingsDrawer">
            <i class="fas fa-cog"></i> More Settings</button>
            <button id='exportStatsButton' type='button' class='buttons' value='Export' onClick='exportMultisync();'><i class="fas fa-scroll"></i> Export </button>
            <button id='refreshStatsButton' type='button' class='buttons' value='Refresh Stats' onClick='clearRefreshTimers(); RefreshStats();'><i class="fas fa-redo"></i> Refresh Stats</button>
            <div class="ms-2">
            </div>
            </div>
            <div class="col-md-auto">
                <div class="form-actions multisyncBulkActions  ">
                    <b>Action for selected systems:</b>
                    <select id='multiAction' onChange='multiActionChanged();'>
                        <option value='noop'>---- Select an Action ----</option>
                        <option value='upgradeFPP'>Upgrade FPP</option>
                        <option value='restartFPPD'>Restart FPPD</option>
                        <option value='reboot'>Reboot</option>
                        <option value='shutdown'>Shutdown</option>
                        <option value='copyFiles'>Copy Show Files</option>
                        <option value='copyOSFiles'>Copy OS Upgrade Files</option>
                        <option value='playerMode'>Set to Player</option>
                        <option value='remoteMode'>Set to Remote</option>
                        <option value='addProxy'>Add as Proxy</option>
                        <? if ($uiLevel > 0) { ?>
                        <option value='changeBranch'>Change Branch</option>
                        <?}?>
                    </select>
                    <button id='performActionButton' type='button' class='buttons btn-success' value='Run' onClick='performMultiAction();'><i class="fas fa-chevron-right"></i> Run</button>
                    <input type='button' class='buttons' value='Clear List' onClick='clearSelected();'>
                </div>
            </div>

        </div>

        <div style='text-align: left;'>
            <div id = 'copyOSOptions' class='actionOptions'>
                <h2>Copy OS Upgrade Files</h2>
                <div class="container-fluid" id = 'copyOSOptionsDetails'>
                    <span class="warning-text">No .fppos files found on this system.</span>
                </div>
                <b>The rsync daemon must be enabled on the remote system(s) to use this functionality.  rsync can be enabled by going to the System tab on the Settings page of the remote system.</b>
            </div>
            <div id = 'changeBranchOptions' class='actionOptions'>
                <h2>Change to branch:
                <select id="branchSelect">
                </select>
                </h2>
            </div>
            <span class='actionOptions' id='copyOptions'>
                <br>
        <?php
PrintSettingGroupTable('multiSyncCopyFiles', '', '', 0);
?>
                <b>The rsync daemon must be enabled on the remote system(s) to use this functionality.  rsync can be enabled by going to the System tab on the Settings page of the remote system.</b>
            </span>
        </div>

            <div id='exitWarning' class='alert alert danger' style='display: none;'>WARNING: Other FPP Systems are being updated from this interface. DO NOT reload or exit this page until these updates are complete.</b></div>




        <div id="fppSystemsUiSettingsDrawer" class="collapse">
            <div id="multisyncViewOptions" class="fppSystemsUiSettings card ">
                <div class="container-fluid settingsTable">

                                <?
PrintSetting('MultiSyncExternalIPAddress');
PrintSetting('MultiSyncMulticast', 'syncModeUpdated');
PrintSetting('MultiSyncBroadcast', 'syncModeUpdated');
PrintSetting('MultiSyncExtraRemotes');
PrintSetting('MultiSyncHTTPSubnets');
PrintSetting('MultiSyncHide10', 'getFPPSystems');
PrintSetting('MultiSyncHide172', 'getFPPSystems');
PrintSetting('MultiSyncHide192', 'getFPPSystems');
?>
                                            </div>

                                <?
if ($uiLevel > 0) {
    echo "<b><i class='fas fa-fw fa-graduation-cap fa-nbsp ui-level-1' title='Advanced Level Setting'></i> - Advanced Level Setting</b>\n";
}
?>

                </div>


            </div>
        </div>



        	</div>
        </div>
    </div>
	<?php include 'common/footer.inc';?>
</div>

<script>

$(document).ready(function() {

    $.get("api/proxies", function(data) {
        proxies = data;

        // Update any existing links now that proxies
        // are loaded
        $("a[ip]").each(function() {
            let ip = $(this).attr('ip');
            $(this).attr('href', wrapUrlWithProxy(ip));
        });

        getFPPSystems();
        getLocalFpposFiles();
    });
    $.get("api/git/branches", function(data) {

        $.each(data, function (i, item) {
            $('#branchSelect').append($('<option>', { 
                value: item,
                text : item 
            }));
        });
    });

    var $table = $('#fppSystemsTable');

    $("#MultiSyncBroadcast").change(validateMultiSyncSettings);
    $("#MultiSyncMulticast").change(validateMultiSyncSettings);

    $.tablesorter.addParser({
        id: 'FPPIPParser',
        is: function() {
            return false;
        },
        format: function(s, table, cell) {
                s = $(cell).attr('ip');
                return s;
        }
    });

    $table
<?if (0) {?>
    .bind('filterInit', function() {
        $table.find('.tablesorter-filter').hide().each(function(){
            var w, $t = $(this);
            w = $t.closest('td').innerWidth();
            $t
                .show()
                .css({
                    'min-width': w,
                    width: w // 'auto' makes it wide again
                });
        });
    })
<?}?>
    .tablesorter({
        widthFixed: false,
        theme: 'fpp',
        cssInfoBlock: 'tablesorter-no-sort',
        widgets: ['zebra', 'filter', 'staticRow', 'saveSort'],
        headers: {
            0: { sortInitialOrder: 'asc' },
            1: { extractor: 'FPPIPParser', sorter: 'ipAddress' }
        },
        widgetOptions: {
            filter_childRows     : true,
            filter_childByColumn : false,
            filter_childWithSibs : false,
            filter_saveFilters   : true,
            filter_functions: {
                2: {
                    "FPP (All)": function(e,n,f,i,$r,c,data) {
                                return isFPP($r.find('span.typeId').html());
                            },
                    "FPP (Raspberry Pi)": function(e,n,f,i,$r,c,data) {
                                return isFPPPi($r.find('span.typeId').html());
                            },
                    "FPP (BeagleBone)": function(e,n,f,i,$r,c,data) {
                                return isFPPBeagleBone($r.find('span.typeId').html());
                            },
                    "FPP (Armbian)": function(e,n,f,i,$r,c,data) {
                                return isFPPArmbian($r.find('span.typeId').html());
                            },
                    "FPP (MacOS)": function(e,n,f,i,$r,c,data) {
                                return isFPPMac($r.find('span.typeId').html());
                            },
                    "Falcon": function(e,n,f,i,$r,c,data) {
                                return isFalcon($r.find('span.typeId').html());
                            },
                    "FalconV4": function(e,n,f,i,$r,c,data) {
                                return isFalconV4($r.find('span.typeId').html());
                            },
                    "ESPixelStick": function(e,n,f,i,$r,c,data) {
                                return isESPixelStick($r.find('span.typeId').html());
                            },
                    "Genius": function(e,n,f,i,$r,c,data) {
                                return isGenius($r.find('span.typeId').html());
                            },
                    "SanDevices": function(e,n,f,i,$r,c,data) {
                                return isSanDevices($r.find('span.typeId').html());
                            },
                    "WLED": function(e,n,f,i,$r,c,data) {
                                return isWLED($r.find('span.typeId').html());
                            },
                    "Unknown": function(e,n,f,i,$r,c,data) {
                                return isUnknownController($r.find('span.typeId').html());
                            }
                },
                3: {
                    "Master": function(e,n,f,i,$r,c,data) { return e === "Master"; }, // Can this be removed now?
                    "Player": function(e,n,f,i,$r,c,data) { return e === "Player"; },
                    "Multisync": function(e,n,f,i,$r,c,data) { return e === "Player w/ Multisync"; },
                    "Bridge": function(e,n,f,i,$r,c,data) { return e === "Bridge"; },
                    "Remote": function(e,n,f,i,$r,c,data) { return e === "Remote"; }
                }
            }
        }
    });
});

</script>
</body>
</html>
