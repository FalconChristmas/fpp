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
<script>
    var rowSpans = [];
    var advancedView = <? echo $advancedView == true ? 'true' : 'false'; ?>;

    function rowSpanSet(rowID) {
        $('#' + rowID + ' > td:nth-child(1)').attr('rowspan', rowSpans[rowID]);
    }

    function rowSpanUp(rowID) {
        if (!rowSpans.includes(rowID))
            rowSpans[rowID] = 1;

        rowSpans[rowID] += 1;
        rowSpanSet(rowID);
    }

    function rowSpanDown(rowID) {
        if (!rowSpans.includes(rowID))
            rowSpans[rowID] = 1;

        if (rowSpans[rowID] <= 1)
            return;

        rowSpans[rowID] -= 1;
        rowSpanSet(rowID);
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

    function platformIsFPP(platform) {
        if (platform && !platform.includes("FPP") &&
            (platform.toLowerCase().includes("unknown")
             || platform == "xSchedule"
             || platform == "xLights"
             || platform.includes("Falcon ")
             || platform == "ESPixelStick")) {
            return false;
        }

        return true;
    }

    function updateFPPVersionInfo(ip) {
		$.get("fppjson.php?command=getFPPstatus&ip=" + ip + '&advancedView=true').done(function(data) {
            var rowID = "fpp_" + ip.replace(/\./g, '_');
            $('#' + rowID + '_version').html(data.advancedView.Version);
            $('#' + rowID + '_osversion').html(data.advancedView.OSVersion);
            $('#' + rowID + '_localgitvers').html("<a href='http://" + ip + "/about.php' target='_blank'><b><font color='red'>" + data.advancedView.LocalGitVersion + "</a>");
            $('#' + rowID + '_remotegitvers').html(data.advancedView.RemoteGitVersion);
		});
    }

	function getFPPSystemInfo(ip, platform) {
        //eventually figure out what to do
        if (!platformIsFPP(platform))
            return;

		$.get("http://" + ip + "/fppjson.php?command=getHostNameInfo", function(data) {
			$('#fpp_' + ip.replace(/\./g,'_') + '_desc').html(data.HostDescription);
		});
	}

	function getFPPSystemStatus(ip, platform) {
        //eventually figure out what to do
        if (!platformIsFPP(platform))
            return;

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
               rowSpanUp(rowID);
            } else {
               var result_style = document.getElementById(rowID + '_warnings').style;
               result_style.display = 'none';
               rowSpanDown(rowID);
            }
               
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
                if (updatesAvailable) {
                    u += "<a href='http://" + ip + "/about.php' target='_blank'><b><font color='red'>" +
                        data.advancedView.LocalGitVersion + "</font></b></a>";
                } else if (data.advancedView.RemoteGitVersion !== "") {
                    u += "<font color='darkgreen'><b>" + data.advancedView.LocalGitVersion + "</b></font>";
                } else {
                    u += data.advancedView.LocalGitVersion;
                }
                u += "</td></tr>" +
                    "<tr><td>Remote:</td><td id='" + rowID + "_remotegitvers'>" + data.advancedView.RemoteGitVersion + "</td></tr>" +
                    "</table>";

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
			if ($('#MultiSyncRefreshStatus').is(":checked"))
				setTimeout(function() {getFPPSystemStatus(ip);}, 1000);
		});
	}

	function parseFPPSystems(data) {
		$('#fppSystems').empty();
        rowSpans = [];

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
			var link = "";
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

			if (data[i].Local)
			{
				link = data[i].HostName + ' <b>*</b>';
				star = "*";
			} else {
				link = "<a href='http://" + data[i].IP + "/'>" + data[i].HostName + "</a>";
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
            }

			var rowID = "fpp_" + ip.replace(/\./g, '_');
            rowSpans[rowID] = 1;

			var newRow = "<tr id='" + rowID + "'>" +
				"<td class='hostnameColumn'>" + link + "<br><small class='hostDescriptionSM' id='fpp_" + ip.replace(/\./g,'_') + "_desc'>"+ hostDescription +"</small></td>" +
				"<td>" + data[i].IP + "</td>" +
                "<td><span id='" + rowID + "_platform'>" + data[i].Platform + "</span><br><small class='hostDescriptionSM' id='" + rowID + "_variant'>" + data[i].model + "</small></td>" +
				"<td>" + fppMode + "</td>" +
				"<td id='" + rowID + "_status'></td>" +
				"<td id='" + rowID + "_elapsed'></td>";

            if ((advancedView === true) &&
                (platformIsFPP(data[i].Platform))) {
				newRow += "<td><table class='multiSyncVerboseTable'><tr><td>FPP:</td><td id='" + rowID + "_version'>" + data[i].version + "</td></tr><tr><td>OS:</td><td id='" + rowID + "_osversion'></td></tr></table></td>";
            } else {
				newRow += "<td id='" + rowID + "_version'>" + data[i].version + "</td>";
            }

            if (advancedView === true) {
                newRow +=
                    "<td id='advancedViewGitVersions_" + rowID + "'></td>" +
                    "<td id='advancedViewUtilization_" + rowID + "'></td>";

                newRow += "<td align='center'>";
                if (platformIsFPP(data[i].Platform))
                    newRow += "<input type='checkbox' class='remoteCheckbox' name='" + data[i].IP + "'>";

                newRow += "</td>";
            }

            newRow = newRow + "</tr>";
			$('#fppSystems').append(newRow);

            var colspan = (advancedView === true) ? 9 : 7;

            newRow = "<tr id='" + rowID + "_warnings' style='display:none' class='tablesorter-childRow'><td colspan='" + colspan + "' id='" + rowID + "_warningCell'></td></tr>";
            $('#fppSystems').append(newRow);

            newRow = "<tr id='" + rowID + "_logs' style='display:none' class='tablesorter-childRow'><td colspan='" + colspan + "' id='" + rowID + "_logCell'><table class='multiSyncVerboseTable' width='100%'><tr><td>Log:</td><td width='100%'><textarea id='" + rowID + "_logText' style='width: 100%;' rows='8' disabled></textarea></td></tr><tr id='" + rowID + "_doneButtons' style='display: none;'><td></td><td><input type='button' class='buttons' value='Reboot' onClick='rebootRemoteFPP(\"" + rowID + "\", \"" + ip + "\");'></td></tr></table></td></tr>";
            $('#fppSystems').append(newRow);

			getFPPSystemStatus(ip, data[i].Platform);
			getFPPSystemInfo(ip, data[i].Platform);
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
        if (inp && inp.value == '') {
            inp.value = extras;
        }
<?
}
?>

        $('#fppSystems').trigger('update', true);
	}

	function getFPPSystems() {
		$('.masterOptions').hide();
		$('#fppSystems').html("<tr><td colspan=8 align='center'>Loading system list from fppd.</td></tr>");

		$.get("fppjson.php?command=getFPPSystems", function(data) {
            parseFPPSystems(data);
            showHideSyncCheckboxes();
//        $.get('/api/fppd/multiSyncSystems', function(data) {
//            parseFPPSystems(data.systems);
//            showHideSyncCheckboxes();
		});
	}

function showHideSyncCheckboxes() {
	if (($('#MultiSyncMulticast').is(":checked")) ||
        ($('#MultiSyncBroadcast').is(":checked"))) {
		$('input.syncCheckbox').each(function() {
			$(this).prop('checked', false).trigger('change');
        });
        $('span.syncCheckboxSpan').hide();
    } else {
        $('span.syncCheckboxSpan').show();
    }
}

function rebootRemoteFPP(rowID, ip) {
    $('#' + rowID + '_logText').val($('#' + rowID + '_logText').val() + '\n==================================\n');

    StreamURL('rebootRemoteFPP.php?ip=' + ip, rowID + '_logText');
}

function ipFromRowID(id) {
    ip = id.replace('fpp_', '').replace(/_/g, '.').replace(/[^\.0-9]/g, '').replace(/\.$/, '');

    return ip;
}

var streamCount = 0;
function EnableDisableStreamButtons() {
    if (streamCount) {
        $('#updateButton').attr("disabled", "disabled");
        $('#restartButton').attr("disabled", "disabled");
    } else {
        $('#updateButton').removeAttr("disabled");
        $('#restartButton').removeAttr("disabled");
    }
}

function updateDone(id) {
    id = id.replace('_logText', '');
    $('#' + id + '_doneButtons').show();
    streamCount--;

    var ip = ipFromRowID(id);
    updateFPPVersionInfo(ip);

    EnableDisableStreamButtons();
}

function updateFailed(id) {
    var ip = ipFromRowID(id);

    alert('Update failed for FPP system at ' + ip);
    streamCount--;

    EnableDisableStreamButtons();
}

function updateSelectedSystems() {
	$('input.remoteCheckbox').each(function() {
		if ($(this).is(":checked")) {
            streamCount++;
            EnableDisableStreamButtons();

            var rowID = $(this).closest('tr').attr('id');

            // Handle tablesorter bug not assigning same color to child rows
            if ($('#' + rowID).hasClass('odd'))
                $('#' + rowID + '_logs').addClass('odd');

            $('#' + rowID + '_logs').show();
            rowSpanUp(rowID);

            var ip = ipFromRowID(rowID);

            if ($('#' + rowID + '_logText').val() != '')
                $('#' + rowID + '_logText').val($('#' + rowID + '_logText').val() + '\n==================================\n');

            StreamURL('http://' + ip + '/manualUpdate.php?wrapped=1', rowID + '_logText', 'updateDone', 'updateFailed');
        }
    });
}

function restartDone(id) {
    streamCount--;

    var ip = ipFromRowID(id);
    updateFPPVersionInfo(ip);

    EnableDisableStreamButtons();
}

function restartFailed(id) {
    var ip = ipFromRowID(id);

    alert('Restart failed out for FPP system at ' + ip);

    streamCount--;

    EnableDisableStreamButtons();
}

function restartSelectedSystems() {
	$('input.remoteCheckbox').each(function() {
		if ($(this).is(":checked")) {
            streamCount++;
            EnableDisableStreamButtons();

            var rowID = $(this).closest('tr').attr('id');

            // Handle tablesorter bug not assigning same color to child rows
            if ($('#' + rowID).hasClass('odd'))
                $('#' + rowID + '_logs').addClass('odd');

            $('#' + rowID + '_logs').show();
            rowSpanUp(rowID);

            var ip = ipFromRowID(rowID);

            if ($('#' + rowID + '_logText').val() != '')
                $('#' + rowID + '_logText').val($('#' + rowID + '_logText').val() + '\n==================================\n');

            StreamURL('restartRemoteFPPD.php?ip=' + ip, rowID + '_logText', 'restartDone', 'restartFailed');
        }
    });
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
            <div class='fppTableWrapper<? if ($advancedView != true) { echo " fppTableWrapperAsTable"; }?>'>
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
                            <th data-sorter='false' data-filter='false'>Select</th>
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
<div style='text-align: right;'>
    <input type='button' class='buttons' value='Refresh List' onClick='getFPPSystems();' style='float: left;'>
<?
if ($advancedView) {
?>
    <div>
        Selected Systems:<br>
    <input id='updateButton' type='button' class='buttons' value='Upgrade FPP' onClick='updateSelectedSystems();'>
    <input id='restartButton' type='button' class='buttons' value='Restart FPPD' onClick='restartSelectedSystems();'>
    </div>
<? } ?>
</div>
    <br>
    <hr>
            <table class='settingsTable'>
<?
PrintSetting('MultiSyncMulticast', 'showHideSyncCheckboxes');
PrintSetting('MultiSyncBroadcast', 'showHideSyncCheckboxes');
PrintSetting('MultiSyncExtraRemotes', 'updateMultiSyncRemotes');
PrintSetting('MultiSyncHide10', 'getFPPSystems');
PrintSetting('MultiSyncHide172', 'getFPPSystems');
PrintSetting('MultiSyncHide192', 'getFPPSystems');
PrintSetting('MultiSyncRefreshStatus', 'getFPPSystems');
PrintSetting('MultiSyncAdvancedView', 'reloadPage');
?>
            </table>
            <?php
                if ($advancedView ==true) {
					?>
                    <b style="color: #FF0000; font-size: 0.9em;">** Advanced View Active - Auto Refresh is not recommended as it may cause slowdowns</b>
					<?php
				}
            ?>
<?php
if ($settings['fppMode'] == 'master')
{
    echo "<hr><br>\n";
    PrintSettingGroupTable('multiSyncCopyFiles', '', '', 0);
?>
			<input type='button' class='buttons' value='Copy Files' onClick='location.href="syncRemotes.php";'>
<?php
}
?>
		</fieldset>
	</div>
	<?php include 'common/footer.inc'; ?>
</div>

<script>

$(document).ready(function() {
    SetupToolTips();
	getFPPSystems();
    showHideSyncCheckboxes();

    var $table = $('#fppSystemsTable');

    $table
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
    .tablesorter({
        widthFixed: false,
        theme: 'blue',
        cssInfoBlock: 'tablesorter-no-sort',
        widgets: ['zebra', 'filter', 'staticRow'],
        headers: {
            2: { sorter: 'ipAddress' }
        }
    });

});

</script>
</body>
</html>
