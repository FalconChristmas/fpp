<!DOCTYPE html>
<html>
<head>
<?php
require_once("config.php");
require_once("common.php");
include 'common/menuHead.inc';

$advancedView = false;
if ((isset($settings['MultiSyncAdvancedView'])) &&
    ($settings['MultiSyncAdvancedView'] == 1)) {
	$advancedView = true;
}
?>
<script type="text/javascript" src="/jquery/jquery.tablesorter/jquery.tablesorter.js"></script>
<script type="text/javascript" src="/jquery/jquery.tablesorter/jquery.tablesorter.widgets.js"></script>
<script type="text/javascript" src="/jquery/jquery.tablesorter/parsers/parser-network.js"></script>

<link rel="stylesheet" href="/jquery/jquery.tablesorter/css/theme.blue.css">
<title><? echo $pageTitle; ?></title>
<style>
input.largeCheckbox {
    -ms-transform: scale(2); /* IE */
    -moz-transform: scale(2); /* FF */
    -webkit-transform: scale(2); /* Safari and Chrome */
    -o-transform: scale(2); /* Opera */
    transform: scale(2);
    padding: 10px;
}

.actionOptions {
    display: none;
}
</style>
<script>
    var hostRows = new Object();
    var rowSpans = new Object();
    var advancedView = <? echo $advancedView == true ? 'true' : 'false'; ?>;

    function rowSpanSet(rowID) {
        var rowSpan = 1;

        if ($('#' + rowID + '_logs').is(':visible'))
            rowSpan++;

        if ($('#' + rowID + '_warnings').is(':visible'))
            rowSpan++;

        $('#' + rowID + ' > td:nth-child(1)').attr('rowspan', rowSpan);
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
        
		$.get("fppjson.php?command=setSetting&key=MultiSyncRemotes&value=" + remotes
		).done(function() {
			settings['MultiSyncRemotes'] = remotes;
            if (verbose) {
                if (remotes == "") {
                    $.jGrowl("Remote List Cleared.  You must restart fppd for the changes to take effect.");
                } else {
                    $.jGrowl("Remote List set to: '" + remotes + "'.  You must restart fppd for the changes to take effect.");
                }
            }

            //Mark FPPD as needing restart
            $.get('fppjson.php?command=setSetting&key=restartFlag&value=2');
            settings['restartFlag'] = 2;
            //Get the resart banner showing
            CheckRestartRebootFlags();
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

        if ((typeId >= 0x01) && (typeId <= 0x3F))
            return true;

        return false;
    }

    function isFPPBeagleBone(typeId) {
        typeId = parseInt(typeId);

        if ((typeId >= 0x41) && (typeId <= 0x7F))
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

        if ((typeId >= 0x80) && (typeId <= 0x8F))
            return true;

        return false;
    }

    function isESPixelStick(typeId) {
        typeId = parseInt(typeId);

        if (typeId == 0xC2)
            return true;

        return false;
    }

    function isSanDevices(typeId) {
        typeId = parseInt(typeId);

        if (typeId == 0xFF)
            return true;

        return false;
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

        var localVer = "<a href='http://" + ip + "/about.php' target='_blank'><b><font color='";
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

	function getFPPSystemInfo(ip) {
		$.get("http://" + ip + "/fppjson.php?command=getHostNameInfo", function(data) {
			$('#fpp_' + ip.replace(/\./g,'_') + '_desc').html(data.HostDescription);
		});
	}

    var refreshTimers = new Object();
    function clearRefreshTimers() {
        for (var refreshKey in refreshTimers) {
            clearTimeout(refreshTimers[refreshKey]);
        }

        refreshTimers = new Object();
    }

	function getFPPSystemStatus(ip, refreshing = false) {
        var refreshKey = ip.replace(/\./g, '_');
        if (refreshTimers.hasOwnProperty(refreshKey)) {
            clearTimeout(refreshTimers[refreshKey]);
            delete refreshTimers[refreshKey];
        }

		$.get("fppjson.php?command=getFPPstatus&ip=" + ip + (advancedView == true ? '&advancedView=true' : '')
		).done(function(data) {
			var status = 'Idle';
			var statusInfo = "";
			var elapsed = "";
			var files = "";

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
                status = '<font color="red">Unreachable</font>';
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

            if ($('#' + rowID).attr('ip') != ip)
                $('#' + rowID).attr('ip', ip);

			$('#' + rowID + '_status').html(status);
			$('#' + rowID + '_elapsed').html(elapsed);
               
            if (data.warnings != null && data.warnings.length > 0) {
               var result_style = document.getElementById(rowID + '_warnings').style;
               result_style.display = 'table-row';

               // Handle tablesorter bug not assigning same color to child rows
               if ($('#' + rowID).hasClass('odd'))
                   $('#' + rowID + '_warnings').addClass('odd');

               var wHTML = "";
               for(var i = 0; i < data.warnings.length; i++) {
                wHTML += "<font color='red'>" + data.warnings[i] + "</font><br>";
               }
               $('#' + rowID + '_warningCell').html(wHTML);
            } else {
               var result_style = document.getElementById(rowID + '_warnings').style;
               result_style.display = 'none';
            }
            rowSpanSet(rowID);
               
			//Expert View Rows
            if(advancedView === true && data.status_name !== 'unknown' && data.status_name !== 'password') {
                $('#' + rowID + '_platform').html(data.advancedView.Platform);

                var updatesAvailable = 0;
                if ((typeof (data.advancedView.RemoteGitVersion) !== 'undefined') &&
                    (typeof (data.advancedView.LocalGitVersion) !== 'undefined') &&
                    (data.advancedView.RemoteGitVersion !== "Unknown") &&
                    (data.advancedView.RemoteGitVersion !== "") &&
                    (data.advancedView.RemoteGitVersion !== data.advancedView.LocalGitVersion)) {
                    updatesAvailable = 1;
                }

                var u = "<table class='multiSyncVerboseTable'>" +
                    "<tr><td>Local:</td><td id='" + rowID + "_localgitvers'>";
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

                if (data.advancedView.OSVersion !== "") {
                    $('#' + rowID + '_osversionRow').show();
                    $('#' + rowID + '_osversion').html(data.advancedView.OSVersion);
                }

                var u = "<table class='multiSyncVerboseTable'>" +
                    "<tr><td>CPU:</td><td>" + (typeof (data.advancedView.Utilization) !== 'undefined' ? Math.round(data.advancedView.Utilization.CPU) : 'Unk.') + "%</td></tr>" +
                    "<tr><td>Mem:</td><td>" + (typeof (data.advancedView.Utilization) !== 'undefined' ? Math.round(data.advancedView.Utilization.Memory) : 'Unk.') + "%</td></tr>" +
                    "<tr><td>Uptime:&nbsp;</td><td>" + (typeof (data.advancedView.Utilization) !== 'undefined' ? data.advancedView.Utilization.Uptime.replace(/ /, '&nbsp;') : 'Unk.') + "</td></tr>" +
                    "</table>";

                $('#advancedViewUtilization_' + rowID).html(u);
            }
		}).always(function() {
			if ($('#MultiSyncRefreshStatus').is(":checked")) {
				refreshTimers[refreshKey] = setTimeout(function() {getFPPSystemStatus(ip, true);}, <? if ($advancedView) echo '5000'; else echo '1000'; ?>);
            }
		});
	}

    function ipLink(ip) {
        return "<a href='http://" + ip + "/'>" + ip + "</a>";
    }

	function parseFPPSystems(data) {
		$('#fppSystems').empty();
        rowSpans = [];

        var uniqueHosts = new Object();

		var remotes = [];
		if (settings['fppMode'] == 'master') {
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
			var ip = data[i].IP;
            var hostDescription = data[i].HostDescription;

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

            var hostKey = data[i].HostName + '_' + data[i].version + '_' + data[i].fppModeString + '_' + data[i].lastSeenStr + '_' + data[i].channelRanges;
            hostKey = hostKey.replace(/[^a-zA-Z0-9]/, '_');

            hostRows[hostRowKey] = rowID;

            if (uniqueHosts.hasOwnProperty(hostKey)) {
                rowID = uniqueHosts[hostKey];
                hostRows[hostRowKey] = rowID;

                $('#' + rowID + '_ip').append('<br>' + ipLink(data[i].IP));

                if (isFPP(data[i].typeId)) {
                    getFPPSystemStatus(ip, false);
                    getFPPSystemInfo(ip);
                }
            } else {
                uniqueHosts[hostKey] = rowID;

                var hostname = data[i].HostName;
                if (data[i].Local) {
                    hostname = "<b>" + hostname + "</b>";
                } else {
                    if ((settings['fppMode'] == 'master') &&
                            (data[i].fppMode == "remote"))
                    {
                        star = "<input type='checkbox' class='syncCheckbox' name='" + data[i].IP + "'";
                        if (typeof remotes[data[i].IP] !== 'undefined') {
                            star += " checked";
                            delete remotes[data[i].IP];
                        }
                        star += " onClick='updateMultiSyncRemotes(true);'>";
                    }
                }

                var fppMode = 'Player';
                if (data[i].fppMode == 'bridge') {
                    fppMode = 'Bridge';
                } else if (data[i].fppMode == 'master') {
                    fppMode = 'Master';
                } else if (data[i].fppMode == 'remote') {
                    fppMode = 'Remote';

                    if (settings['fppMode'] == 'master')
                        fppMode += "<span class='syncCheckboxSpan'>:<br>Enable Sync: " + star + "</span>";
                } else if (data[i].fppMode == 'unknown') {
                    fppMode = 'Unknown';
                }

                rowSpans[rowID] = 1;

                var ipTxt = data[i].Local ? data[i].IP : ipLink(data[i].IP);
                var newRow = "<tr id='" + rowID + "' ip='" + data[i].IP + "' class='systemRow'>" +
                    "<td class='hostnameColumn'>" + hostname + "<br><small class='hostDescriptionSM' id='fpp_" + ip.replace(/\./g,'_') + "_desc'>"+ hostDescription +"</small></td>" +
                    "<td id='" + rowID + "_ip'>" + ipTxt + "</td>" +
                    "<td><span id='" + rowID + "_platform'>" + data[i].Platform + "</span><br><small class='hostDescriptionSM' id='" + rowID + "_variant'>" + data[i].model + "</small><span class='hidden typeId'>" + data[i].typeId + "</span></td>" +
                    "<td id='" + rowID + "_mode'>" + fppMode + "</td>" +
                    "<td id='" + rowID + "_status'>Last Seen:<br>" + data[i].lastSeenStr + "</td>" +
                    "<td id='" + rowID + "_elapsed'></td>";

                var versionParts = data[i].version.split('.');
                var majorVersion = parseInt(versionParts[0]);

                if ((advancedView === true) &&
                    (isFPP(data[i].typeId))) {
                    newRow += "<td><table class='multiSyncVerboseTable'><tr><td>FPP:</td><td id='" + rowID + "_version'>" + data[i].version + "</td></tr><tr><td>OS:</td><td id='" + rowID + "_osversion'></td></tr></table></td>";
                } else {
                    newRow += "<td id='" + rowID + "_version'>" + data[i].version + "</td>";
                }

                if (advancedView === true) {
                    newRow +=
                        "<td id='advancedViewGitVersions_" + rowID + "'></td>" +
                        "<td id='advancedViewUtilization_" + rowID + "'></td>";

                    newRow += "<td class='centerCenter'>";
                    if ((isFPP(data[i].typeId)) &&
                        (majorVersion >= 4))
                        newRow += "<input type='checkbox' class='remoteCheckbox largeCheckbox' name='" + data[i].IP + "'>";

                    newRow += "</td>";
                }

                newRow = newRow + "</tr>";
                $('#fppSystems').append(newRow);

                var colspan = (advancedView === true) ? 9 : 7;

                newRow = "<tr id='" + rowID + "_warnings' style='display:none' class='tablesorter-childRow'><td colspan='" + colspan + "' id='" + rowID + "_warningCell'></td></tr>";
                $('#fppSystems').append(newRow);

                newRow = "<tr id='" + rowID + "_logs' style='display:none' class='tablesorter-childRow'><td colspan='" + colspan + "' id='" + rowID + "_logCell'><table class='multiSyncVerboseTable' width='100%'><tr><td>Log:</td><td width='100%'><textarea id='" + rowID + "_logText' style='width: 100%;' rows='8' disabled></textarea></td></tr><tr><td></td><td><div class='right' id='" + rowID + "_doneButtons' style='display: none;'><input type='button' class='buttons' value='Restart FPPD' onClick='restartSystem(\"" + rowID + "\");' style='float: left;'><input type='button' class='buttons' value='Reboot' onClick='rebootRemoteFPP(\"" + rowID + "\", \"" + ip + "\");' style='float: left;'><input type='button' class='buttons' value='Close Log' onClick='$(\"#" + rowID +"_logs\").hide(); rowSpanSet(\"" + rowID + "\");'></div></td></tr></table></td></tr>";
                $('#fppSystems').append(newRow);

                if (isFPP(data[i].typeId)) {
                    getFPPSystemStatus(ip, false);
                    getFPPSystemInfo(ip);
                } else if ((isESPixelStick(data[i].typeId)) &&
                    (data[i].fppMode == 'bridge') &&
                    (majorVersion >= 3)) {
                    getESPixelStickBridgeStatus(ip);
                } else if (isFalcon(data[i].typeId)) {
                    getFalconControllerStatus(ip);
                }
            }
        }

        var extras = "";
        for (var x in remotes) {
            if (extras != "") {
                extras += ",";
            }
            extras += x;
        }
<?php
if ($uiLevel >= 1) {
?>
        var inp = document.getElementById("MultiSyncExtraRemotes");
        if (extras != '' && inp && inp.value != extras) {
			$('#MultiSyncExtraRemotes').val(extras).trigger('change');
        }
<?
}
?>

        $('#fppSystems').trigger('update', true);
	}

	function getFPPSystems() {
		if (streamCount) {
			alert("FPP Systems are being udpated, you will need to manually refresh once these updates are complete.");
			return;
		}

		$('.masterOptions').hide();
		$('#fppSystems').html("<tr><td colspan=8 align='center'>Loading system list from fppd.</td></tr>");

		$.get("fppjson.php?command=getFPPSystems", function(data) {
            parseFPPSystems(data);
//        $.get('/api/fppd/multiSyncSystems', function(data) {
//            parseFPPSystems(data.systems);
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
        var st = "<table class='multiSyncVerboseTable'>";
        st += "<tr><td>Tot Pkts:</td><td>" + s.e131.num_packets + "</td></tr>";
        st += "<tr><td>Seq Errs:</td><td>" + s.e131.seq_errors + "</td></tr>";
        st += "<tr><td>Pkt Errs:</td><td>" + s.e131.packet_errors + "</td></tr>";
        st += "</table>";

        $('#fpp_' + ips + '_status').html(st);
    }

    if ($('#MultiSyncRefreshStatus').is(":checked")) {
        setTimeout(function() {ESPSockets[ips].send("XJ");}, 1000);
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
            ws.send("XJ");
        };

        ws.onmessage = function(e) {
            if ("string" == typeof e.data) {
                var t = e.data.substr(0, 2)
                  , n = e.data.substr(2);
                switch (t) {
                    case "G1":
                        parseESPixelStickConfig(ip, n);
                        break;
                    case "XJ":
                        parseESPixelStickStatus(ip, n);
                        break;
                }
            }
        };

        ws.onclose = function() {
            delete ESPSockets[ips];
        };
    }
}

function getFalconControllerStatus(ip) {
    // Need to update this once Falcon controllers support
    // FPP MultiSync discovery
    $.ajax({
        url: 'api/proxy/' + ip + '/status.xml',
        dataType: 'xml',
        success: function(data) {
            var ips = ip.replace(/\./g, '_');
            var u = "<table class='multiSyncVerboseTable'>";
            u += "<tr><td>Uptime:</td><td>" + $(data).find('u').text() + "</td></tr>";
            u += "</table>";

            $('#advancedViewUtilization_fpp_' + ips).html(u);

            var mode = $('#fpp_' + ips + '_mode').html();

            if (mode == 'Bridge') {
                $('#fpp_' + ips + '_status').html('Bridging');
            } else {
                $('#fpp_' + ips + '_status').html('');
            }
        }
    });
}

function RefreshStats() {
    var keys = Object.keys(hostRows);
    for (var i = 0; i < keys.length; i++) {
        var rowID = hostRows[keys[i]];
        var ip = ipFromRowID(rowID);
        var mode = $('#' + rowID + '_mode').html();

        var typeId = $('#' + rowID).find('.typeId').html();
        if (isFPP(typeId)) {
            getFPPSystemStatus(ip, true);
            getFPPSystemInfo(ip);
        } else if (isESPixelStick(typeId) &&
            (mode == 'Bridge')) {
            getESPixelStickBridgeStatus(ip);
        } else if (isFalcon(typeId)) {
            getFalconControllerStatus(ip);
        }
    }
}

function autoRefreshToggled() {
	if ($('#MultiSyncRefreshStatus').is(":checked")) {
        RefreshStats();
	}
}

function reloadMultiSyncPage() {
	if (streamCount) {
		alert("FPP Systems are being udpated, you will need to manually refresh once these updates are complete.");
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
}

function rebootRemoteFPP(rowID, ip) {
    $('#' + rowID + '_logText').val($('#' + rowID + '_logText').val() + '\n==================================\n');

    StreamURL('rebootRemoteFPP.php?ip=' + ip, rowID + '_logText');
}

function ipFromRowID(id) {
    ip = $('#' + id).attr('ip');

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
    setTimeout(function() { getFPPSystemStatus(ip, true); }, 1500);

    if (origins.hasOwnProperty(ip)) {
        for (var i = 0; i < origins[ip].length; i++) {
			var rowID = "fpp_" + origins[ip][i].replace(/\./g, '_');
            upgradeSystem(rowID);
        }
    }

    EnableDisableStreamButtons();
}

function upgradeFailed(id) {
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

function upgradeSystem(rowID) {
    $('#' + rowID).find('input.remoteCheckbox').prop('checked', false);

    streamCount++;
    EnableDisableStreamButtons();

    showLogsRow(rowID);
    addLogsDivider(rowID);

    var ip = ipFromRowID(rowID);
    StreamURL('http://' + ip + '/manualUpdate.php?wrapped=1', rowID + '_logText', 'upgradeDone', 'upgradeFailed');
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

function restartDone(id) {
    id = id.replace('_logText', '');
    $('#' + id + '_doneButtons').show();
    streamCount--;

    var ip = ipFromRowID(id);

    setTimeout(function() { getFPPSystemStatus(ip, true); }, 1500);

    EnableDisableStreamButtons();
}

function restartFailed(id) {
    var ip = ipFromRowID(id);

    alert('Restart failed out for FPP system at ' + ip);

    streamCount--;

    EnableDisableStreamButtons();
}

function restartSystem(rowID) {
    streamCount++;
    EnableDisableStreamButtons();

    showLogsRow(rowID);
    addLogsDivider(rowID);

    var ip = ipFromRowID(rowID);
    StreamURL('restartRemoteFPPD.php?ip=' + ip, rowID + '_logText', 'restartDone', 'restartFailed');
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

function copyFilesToSystem(rowID) {
    streamCount++;
    EnableDisableStreamButtons();

    showLogsRow(rowID);
    addLogsDivider(rowID);

    var ip = ipFromRowID(rowID);
    StreamURL('copyFilesToRemote.php?ip=' + ip, rowID + '_logText', 'copyDone', 'copyFailed');
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

function copyDone(id) {
    id = id.replace('_logText', '');
    $('#' + id + '_doneButtons').show();
    streamCount--;

    EnableDisableStreamButtons();
}

function copyFailed(id) {
    var ip = ipFromRowID(id);

    alert('File Copy failed for FPP system at ' + ip);
    streamCount--;

    EnableDisableStreamButtons();

    if (!$('#fppSystemsTableWrapper').hasClass('fppTableWrapperErrored'))
        $('#fppSystemsTableWrapper').addClass('fppTableWrapperErrored');
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
        case 'upgradeFPP':     upgradeSelectedSystems();      break;
        case 'restartFPPD':    restartSelectedSystems();      break;
        case 'copyFiles':      copyFilesToSelectedSystems();  break;
        default:               alert('You must select an action first.'); break;
    }

    $('#selectAllCheckbox').prop('checked', false);
}

function multiActionChanged() {
    var action = $('#multiAction').val();

    $('.actionOptions').hide();
    
    switch (action) {
        case 'copyFiles':      $('#copyOptions').show();      break;
    }
}

</script>
</head>
<body>
<div id="bodyWrapper">
	<?php include 'menu.inc'; ?>
	<br/>
	<div id="uifppsystems" class="settings">
		<fieldset>
			<legend>FPP MultiSync</legend>
            <div id='fppSystemsTableWrapper' class='fppTableWrapper<? if ($advancedView != true) { echo " fppTableWrapperAsTable"; }?>'>
                <div class='fppTableContents'>
			<table id='fppSystemsTable' cellpadding='3'>
				<thead>
					<tr>
						<th class="hostnameColumn">Hostname</th>
						<th>IP Address</th>
						<th>Platform</th>
						<th>Mode</th>
						<th>Status</th>
						<th data-sorter='false' data-filter='false'>Elapsed</th>
						<th>Version</th>
						<?php
                        //Only show expert view is requested
						if ($advancedView == true) {
							?>
                            <th data-sorter='false' data-filter='false'>Git Versions</th>
                            <th data-sorter='false' data-filter='false'>Utilization</th>
                            <th data-sorter='false' data-filter='false'><input id='selectAllCheckbox' type='checkbox' class='largeCheckbox' onChange='selectAllChanged();' /></th>
                        <?php
                    }
                    ?>
                </tr>
            </thead>
            <tbody id='fppSystems'>
                <tr><td colspan=8 align='center'>Loading system list from fppd.</td></tr>
            </tbody>
        </table>
    </div>
</div>
<?
if ($advancedView) {
?>
<div style='text-align: right;'>
    <div style='float: left;'>
        <input id='refreshStatsButton' type='button' class='buttons' value='Refresh Stats' onClick='clearRefreshTimers(); RefreshStats();'>
    </div>
    <div>
        <b>Action for selected systems:</b>
        <select id='multiAction' onChange='multiActionChanged();'>
            <option value='noop'>---- Select an Action ----</option>
            <option value='upgradeFPP'>Upgrade FPP</option>
            <option value='restartFPPD'>Restart FPPD</option>
            <option value='copyFiles'>Copy Files</option>
        </select>
        <input id='performActionButton' type='button' class='buttons' value='Run' onClick='performMultiAction();'>
        <input type='button' class='buttons' value='Clear List' onClick='clearSelected();'>
    </div>
</div>
<div style='text-align: left;'>
    <span class='actionOptions' id='copyOptions'>
        <br>
<?php
PrintSettingGroupTable('multiSyncCopyFiles', '', '', 0);
?>
    </span>
</div>
<div style='width: 100%; text-align: center;'>
    <span id='exitWarning' class='warning' style='display: none;'>WARNING: Other FPP Systems are being updated from this interface. DO NOT reload or exit this page until these updates are complete.</b><br></span>
</div>
<hr>
<? } ?>
            <table class='settingsTable'>
<?
PrintSetting('MultiSyncMulticast', 'syncModeUpdated');
PrintSetting('MultiSyncBroadcast', 'syncModeUpdated');
PrintSetting('MultiSyncExtraRemotes');
PrintSetting('MultiSyncHTTPSubnets');
PrintSetting('MultiSyncHide10', 'getFPPSystems');
PrintSetting('MultiSyncHide172', 'getFPPSystems');
PrintSetting('MultiSyncHide192', 'getFPPSystems');
PrintSetting('MultiSyncRefreshStatus', 'autoRefreshToggled');
PrintSetting('MultiSyncAdvancedView', 'reloadMultiSyncPage');
?>
            </table>
		</fieldset>
<?
if ($uiLevel > 0) {
    echo "<b>* - Advanced Level Setting</b>\n";
}
?>
	</div>
	<?php include 'common/footer.inc'; ?>
</div>

<script>

$(document).ready(function() {
    SetupToolTips();
	getFPPSystems();

    var $table = $('#fppSystemsTable');

    $table
<? if (0) { ?>
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
<? } ?>
    .tablesorter({
        widthFixed: false,
        theme: 'blue',
        cssInfoBlock: 'tablesorter-no-sort',
        widgets: ['zebra', 'filter', 'staticRow'],
        headers: {
            2: { sorter: 'ipAddress' }
        },
        widgetOptions: {
            filter_functions: {
                2: {
                    "FPP (All)": function(e,n,f,i,$r,c,data) {
                                return isFPP($r.find('span.typeId').html());
                            },
                    "FPP (Pi)": function(e,n,f,i,$r,c,data) {
                                return isFPPPi($r.find('span.typeId').html());
                            },
                    "FPP (BeagleBone)": function(e,n,f,i,$r,c,data) {
                                return isFPPBeagleBone($r.find('span.typeId').html());
                            },
                    "Falcon": function(e,n,f,i,$r,c,data) {
                                return isFalcon($r.find('span.typeId').html());
                            },
                    "ESPixelStick": function(e,n,f,i,$r,c,data) {
                                return isESPixelStick($r.find('span.typeId').html());
                            },
                    "SanDevices": function(e,n,f,i,$r,c,data) {
                                return isSanDevices($r.find('span.typeId').html());
                            },
                    "Unknown": function(e,n,f,i,$r,c,data) {
                                return isUnknownController($r.find('span.typeId').html());
                            }
                },
                3: {
                    "Master": function(e,n,f,i,$r,c,data) { return e === "Master"; },
                    "Player": function(e,n,f,i,$r,c,data) { return e === "Player"; },
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
