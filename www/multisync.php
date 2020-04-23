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
    var advancedView = <? echo $advancedView == true ? 'true' : 'false'; ?>;

    function updateMultiSyncRemotes(verbose = false) {
		var remotes = "";

		$('input.remoteCheckbox').each(function() {
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
               var wHTML = "";
               for(var i = 0; i < data.warnings.length; i++) {
                wHTML += "<font color='red'>" + data.warnings[i] + "</font><br>";
               }
               $('#' + rowID + '_warningCell').html(wHTML);
            } else {
               var result_style = document.getElementById(rowID + '_warnings').style;
               result_style.display = 'none';
            }
               
			//Expert View Rows
            if(advancedView === true && data.status_name !== 'unknown' && data.status_name !== 'password') {
                $('#' + rowID + '_platform').html(data.advancedView.Platform + "<br><small class='hostDescriptionSM'>" + data.advancedView.Variant + "</small>");

                var updatesAvailable = 0;
                if ((typeof (data.advancedView.RemoteGitVersion) !== 'undefined') &&
                    (typeof (data.advancedView.LocalGitVersion) !== 'undefined') &&
                    (data.advancedView.RemoteGitVersion !== "Unknown") &&
                    (data.advancedView.RemoteGitVersion !== "") &&
                    (data.advancedView.RemoteGitVersion !== data.advancedView.LocalGitVersion)) {
                    updatesAvailable = 1;
                }

                var u = "<table class='multiSyncVerboseTable'>" +
                    "<tr><td>Local:</td><td>";
                if (updatesAvailable) {
                    u += "<a href='http://" + ip + "/about.php' target='_blank'><b><font color='red'>" +
                        data.advancedView.LocalGitVersion + "</font></b></a>";
                } else if (data.advancedView.RemoteGitVersion !== "") {
                    u += "<font color='darkgreen'><b>" + data.advancedView.LocalGitVersion + "</b></font>";
                } else {
                    u += data.advancedView.LocalGitVersion;
                }
                u += "</td></tr>" +
                    "<tr><td>Remote:</td><td>" + data.advancedView.RemoteGitVersion + "</td></tr>" +
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

		var remotes = [];
		if (typeof settings['MultiSyncRemotes'] === 'string') {
			var tarr = settings['MultiSyncRemotes'].split(',');
			for (var i = 0; i < tarr.length; i++) {
				remotes[tarr[i]] = 1;
			}
		}

		if (settings['fppMode'] == 'master') {
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
				link = data[i].HostName;
				star = "*";
			} else {
				link = "<a href='http://" + data[i].IP + "/'>" + data[i].HostName + "</a>";
				if ((settings['fppMode'] == 'master') &&
						(data[i].fppMode == "remote"))
				{
					star = "<input type='checkbox' class='remoteCheckbox' name='" + data[i].IP + "'";
                    if (typeof remotes[data[i].IP] !== 'undefined') {
						star += " checked";
                        delete remotes[data[i].IP];
                    }
					star += " onClick='updateMultiSyncRemotes(true);'>";
				}
			}

			var fppMode = 'Player';
			if (data[i].fppMode == 'bridge')
				fppMode = 'Bridge';
			else if (data[i].fppMode == 'master')
				fppMode = 'Master';
			else if (data[i].fppMode == 'remote')
				fppMode = 'Remote';

			var rowID = "fpp_" + ip.replace(/\./g, '_');

			var newRow = "<tr id='" + rowID + "'>" +
				"<td align='center'>" + star + "</td>" +
				"<td class='hostnameColumn'>" + link + "<br><small class='hostDescriptionSM' id='fpp_" + ip.replace(/\./g,'_') + "_desc'>"+ hostDescription +"</small></td>" +
				"<td>" + data[i].IP + "</td>" +
                "<td id='" + rowID + "_platform'>" + data[i].Platform + "<br><small class='hostDescriptionSM'>" + data[i].model + "</small></td>" +
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
            }

            newRow = newRow + "</tr>";
			$('#fppSystems').append(newRow);
            
            newRow = "<tr id='" + rowID + "_warnings' style='display:none' class='tablesorter-childRow'><td></td><td></td><td colspan='6' id='" + rowID + "_warningCell'></td></tr>";
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
            showHideRemoteCheckboxes();
//        $.get('/api/fppd/multiSyncSystems', function(data) {
//            parseFPPSystems(data.systems);
//            showHideRemoteCheckboxes();
		});
	}

function showHideRemoteCheckboxes() {
	if (($('#MultiSyncMulticast').is(":checked")) ||
        ($('#MultiSyncBroadcast').is(":checked"))) {
		$('input.remoteCheckbox').each(function() {
			$(this).hide();
			$(this).prop('checked', false);
        });
    } else {
		$('input.remoteCheckbox').show();
    }
    updateMultiSyncRemotes();
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
						<th data-sorter='false' data-filter='false'><? if ($settings['fppMode'] == 'master') echo "Sync"; ?></th>
						<th class="hostnameColumn">Hostname</th>
						<th>IP Address</th>
						<th>Platform</th>
						<th>Mode</th>
						<th>Status</th>
						<th data-sorter='false'>Elapsed</th>
						<th>Version</th>
						<?php
                        //Only show expert view is requested
						if ($advancedView == true) {
							?>
                            <th>Git Versions</th>
                            <th>Utilization</th>
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
    <font size=-1>
        <span id='legend'>
			<input type='button' class='buttons' value='Refresh' onClick='getFPPSystems();'><br>
            * - Local System
            <span class='masterOptions' style='display:none'><br>&#x2713; - Sync Remote FPP with this Master instance</span>
		</span>
    </font>
    <br>
    <hr>
            <table class='settingsTable'>
<?
PrintSetting('MultiSyncMulticast', 'showHideRemoteCheckboxes');
PrintSetting('MultiSyncBroadcast', 'showHideRemoteCheckboxes');
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
    showHideRemoteCheckboxes();

    $('#fppSystemsTable').tablesorter({
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
