<?
$skipJSsettings = 1;
require_once('common.php');

$git_branch = exec("$SUDO git --git-dir=" . dirname(dirname(__FILE__)) . "/.git/ branch --list | grep '\\*' | awk '{print \$2}'", $output, $return_val);
if ($return_val != 0)
    $git_branch = "Unknown";
unset($output);

$git_branch_remote = "";
if ($git_branch !== "Unknown" && $git_branch !== "") {
    $tmpRemote = exec("$SUDO git --git-dir=" . dirname(dirname(__FILE__)) . "/.git/ config --get branch." . escapeshellarg($git_branch) . ".remote 2>/dev/null", $out_tmp, $ret_tmp);
    if ($ret_tmp === 0 && trim($tmpRemote) !== "") {
        $git_branch_remote = trim($tmpRemote);
    } else {
        $upstream = exec("$SUDO git --git-dir=" . dirname(dirname(__FILE__)) . "/.git/ rev-parse --abbrev-ref --symbolic-full-name \\@{u} 2>/dev/null", $out3, $ret3);
        if ($ret3 === 0 && strpos($upstream, '/') !== false) {
            $git_branch_remote = substr($upstream, 0, strpos($upstream, '/'));
        }
    }
    unset($out_tmp); unset($out3); unset($tmpRemote); unset($upstream);
}

function filterBranch($branch)
{
    if (
        preg_match("*v[01]\.[0-9x]*", $branch)   // very very old v0.x and v1.x branches
        || preg_match("*v2\.[0-9x]*", $branch)   // old v2.x branchs, that can no longer work (wrong lib versions)
        || preg_match("*v3\.[0-9x]*", $branch)   // old v3.x branchs, that can no longer work (wrong libhttp versions)
        || preg_match("*v4\.[0-9x]*", $branch)   // old v4.x branchs, that can no longer work (wrong vlc versions)
        || preg_match("*cpinkham*", $branch)     // privatish branches used by developers, developers should know how to flip from command line
        || preg_match("*dkulp*", $branch)
        || $branch == "new-ui"                   // some irrelevant branches at this point
        || $branch == "stage"
    ) {
        return "";
    }

    return $branch;
}

function PrintGitBranchOptions()
{
    global $git_branch, $SUDO, $settings, $mediaDirectory;

    $branches = array();
    // Get remote from settings, default to 'origin' if not set
    $remote = 'origin'; // default
    if (isset($settings['gitRemote'])) {
        // Handle both old and new format - extract remote name from value
        if (strpos($settings['gitRemote'], 'newfeatures') !== false) {
            $remote = 'newfeatures';
        } else if (strpos($settings['gitRemote'], 'origin') !== false) {
            $remote = 'origin';
        }
    }

    // Serialize this fetch through the same lock the upgrade scripts use so it
    // can't race on FETCH_HEAD / ref locks with a running upgrade (which would
    // otherwise show up as "Cannot rebase onto multiple branches"). See
    // runGitLocked in scripts/functions.
    $gitLock = escapeshellarg($mediaDirectory . "/tmp/fpp-git-repo.lock");
    $fetchCmd = escapeshellarg("git fetch -p --all && git remote prune " . escapeshellarg($remote));
    if (is_executable("/usr/bin/flock")) {
        exec("$SUDO flock -w 300 -x $gitLock bash -c $fetchCmd");
    } else {
        exec("$SUDO bash -c $fetchCmd");
    }

    // Get all remote branches
    exec("$SUDO git --git-dir=" . dirname(dirname(__FILE__)) . "/.git/ branch -r", $all_branches);

    // Filter to only branches from the selected remote
    foreach ($all_branches as $branch_line) {
        $branch_line = trim($branch_line);
        // Check if this branch is from the selected remote
        if (strpos($branch_line, "$remote/") === 0 && strpos($branch_line, '->') === false) {
            // Remove the remote prefix
            $branch = substr($branch_line, strlen($remote) + 1);
            $branch = filterBranch($branch);
            if ($branch != "" && $branch !== false) {
                $branches[] = $branch;
            }
        }
    }

    // Sort and remove duplicates
    $branches = array_unique($branches);
    sort($branches);

    foreach ($branches as $branch) {
        if ($branch == $git_branch) {
            echo "<option value='$branch' selected>$branch</option>\n";
        } else {
            echo "<option value='$branch'>$branch</option>\n";
        }
    }
}

?>

<script language="Javascript">

    function reloadGitStatus() {
        $.ajax({
            url: 'api/git/status',
            type: 'GET',
            success: function (data) {
                if ("log" in data) {
                    $('#gitStatusPre').html(data.log);
                }
            },
            error: function (data) {
                alert('Call to api/git/status failed');
            }
        });
    }

    function GitReset() {
        $.get("api/git/reset"
        ).done(function () {
            reloadGitStatus();
        });
    }

    function reloadGitBranches(remote) {
        $.ajax({
            url: 'api/git/branches?remote=' + remote,
            type: 'GET',
            success: function (branches) {
                $('#gitBranch').empty();
                // Add a placeholder option to force user selection
                $('#gitBranch').append('<option value="" selected>-- Select Branch --</option>');
                branches.forEach(function (branch) {
                    $('#gitBranch').append('<option value="' + branch + '">' + branch + '</option>');
                });
            },
            error: function (data) {
                alert('Call to api/git/branches failed');
            }
        });
    }

    function ChangeGitHubForkBranch(branch) {
        if (!branch) {
            return;
        }
        var user = $('#gitHubUser').val() || '';
        user = user.trim();
        if (!user) {
            alert('GitHub user not set');
            return;
        }
        // Reuse the same confirmation as ChangeGitBranch but indicate fork remote
        if (confirm("Are you really sure you want to switch to the '" + branch + "' branch from your fork (" + user + "/fpp)?  This may take some time and it may not be fully compatible with this FPP OS version.  Click 'OK' to continue.")) {
            location.href = 'changebranch.php?branch=' + encodeURIComponent(branch) + '&remote=' + encodeURIComponent(user);
        } else {
            $('#gitHubForkBranch').val('');
        }
    }

    function ReloadGitBranch() {
        var branch = $('#gitBranch').val();
        if (!branch) return;
        ChangeGitBranch(branch);
    }

    function ReloadGitHubForkBranch() {
        var branch = $('#gitHubForkBranch').val();
        if (!branch) return; // Use Official Branches selected -> do nothing
        ChangeGitHubForkBranch(branch);
    }

    var currentGitBranch = "<?php echo addslashes($git_branch); ?>";
    var currentGitRemote = "<?php echo addslashes($git_branch_remote); ?>";

    function loadGitHubForkBranches() {
        var user = $('#gitHubUser').val();
        var pat = $('#gitHubPAT').val();
        if (!user || !pat || $.trim(user) === '' || $.trim(pat) === '') {
            $('#gitHubForkBranchRow').hide();
            return;
        }
        $.ajax({
            url: 'api/git/forkBranches',
            type: 'GET',
            success: function (data) {
                if (data && data.hasFork && data.branches && data.branches.length > 0) {
                    var $sel = $('#gitHubForkBranch');
                    $sel.empty();
                    $sel.append($('<option>').attr('value', '').prop('selected', true).text('Use Official Branches'));
                    data.branches.forEach(function (branch) {
                        $sel.append($('<option>').attr('value', branch).text(branch));
                    });
                    $('#gitHubForkRepoLabel').text(data.user + '/fpp');
                    var tipText = "Switch to a branch from your fork (" + data.user + ") or use official branches.";
                    $('#gitHubForkBranch_tip').attr('data-bs-title', tipText);
                    var tipEl = document.getElementById('gitHubForkBranch_tip');
                    if (tipEl && window.bootstrap && bootstrap.Tooltip) {
                        var inst = bootstrap.Tooltip.getInstance(tipEl);
                        if (inst) inst.dispose();
                        new bootstrap.Tooltip(tipEl);
                    }
                    // Stay on the branch that was switched to instead of jumping back to Use Official Branches
                    if (currentGitBranch && currentGitRemote && data.user && currentGitRemote.toLowerCase() === data.user.toLowerCase() && data.branches.indexOf(currentGitBranch) !== -1) {
                        $sel.val(currentGitBranch);
                    }
                    $('#gitHubForkBranchRow').show();
                } else if (data && data.hasFork) {
                    // Fork exists but no branches (or all filtered) - still show dropdown with default only
                    var $sel = $('#gitHubForkBranch');
                    $sel.empty();
                    $sel.append($('<option>').attr('value', '').prop('selected', true).text('Use Official Branches'));
                    $('#gitHubForkRepoLabel').text(data.user + '/fpp');
                    var tipText2 = "Switch to a branch from your fork (" + data.user + ") or use official branches.";
                    $('#gitHubForkBranch_tip').attr('data-bs-title', tipText2);
                    var tipEl2 = document.getElementById('gitHubForkBranch_tip');
                    if (tipEl2 && window.bootstrap && bootstrap.Tooltip) {
                        var inst2 = bootstrap.Tooltip.getInstance(tipEl2);
                        if (inst2) inst2.dispose();
                        new bootstrap.Tooltip(tipEl2);
                    }
                    if (currentGitBranch && currentGitRemote && data.user && currentGitRemote.toLowerCase() === data.user.toLowerCase()) {
                        // Current branch is on this fork but has no listed branches (filtered) - keep default
                    }
                    $('#gitHubForkBranchRow').show();
                } else {
                    $('#gitHubForkBranchRow').hide();
                }
            },
            error: function () {
                $('#gitHubForkBranchRow').hide();
            }
        });
    }

    $(document).ready(function () {
        reloadGitStatus();
        loadGitHubForkBranches();

        // Listen for changes to the git remote setting
        $('#gitRemote').on('change', function () {
            var remote = $(this).val();
            // Extract the remote name from the value (in case it contains description text)
            if (remote.indexOf('newfeatures') !== -1) {
                remote = 'newfeatures';
            } else if (remote.indexOf('origin') !== -1) {
                remote = 'origin';
            }
            reloadGitBranches(remote);
        });

        // Reload fork branches when GitHub credentials change (saved via SetSetting)
        // The inputs use onChange to call SetSetting; poll for a short time after change
        var forkReloadTimer = null;
        function scheduleForkReload() {
            if (forkReloadTimer) clearTimeout(forkReloadTimer);
            forkReloadTimer = setTimeout(loadGitHubForkBranches, 800);
        }
        $('#gitHubUser, #gitHubPAT').on('change blur', scheduleForkReload);
        // Also observe after SetSetting completes - hook into the global SetSetting if available
        var origSetSetting = window.SetSetting;
        if (typeof origSetSetting === 'function') {
            window.SetSetting = function () {
                var args = Array.prototype.slice.call(arguments);
                var setting = args[0];
                var origCb = args[6];
                args[6] = function () {
                    if (typeof origCb === 'function') origCb.apply(this, arguments);
                    if (setting === 'gitHubUser' || setting === 'gitHubPAT') {
                        scheduleForkReload();
                    }
                };
                return origSetSetting.apply(this, args);
            };
        }
    });

</script>
<h2>Developer Settings</h2>
<div class="settingsTable container-fluid">
    <?
    PrintSetting('masqUIPlatform');
    PrintSetting('gitRemote');
    PrintSetting('gitHubUser');
    PrintSetting('gitHubPAT');
    PrintSetting('DistributedCompile');
    PrintSetting('DistccHosts');
    ?>
    <div class="row" id="gitHubForkBranchRow" style="display:none">
        <div class='printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2 align-top'>
            <div class="description"><i class='fas fa-fw fa-code fa-nbsp ui-level-3' title='Developer Level Setting'></i>GitHub Fork Branch:</div>
        </div>
        <div class='printSettingFieldCol col-md'>
            <select id='gitHubForkBranch' onChange="ChangeGitHubForkBranch($('#gitHubForkBranch').val());">
                <option value="" selected>Use Official Branches</option>
            </select>
            <a href="#" class="btn btn-sm btn-outline-secondary ms-2" title="Reload selected fork branch" onclick="ReloadGitHubForkBranch(); return false;"><i class="fas fa-sync-alt" aria-hidden="true"></i></a>
            <span id="gitHubForkBranch_tip" data-bs-toggle="tooltip" data-bs-html="true" data-bs-placement="auto" data-bs-title="Switch to a branch from your fork or use official branches."><img id="gitHubForkBranch_img" src="images/redesign/help-icon.svg" class="icon-help" alt="help icon"></span>
            <div class="callout callout-secondary mt-1">
                <b>Note:</b> Shows branches from <code id="gitHubForkRepoLabel">your fork</code> on GitHub.
            </div>
        </div>
    </div>
    <div class="row">
        <div class='printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2 align-top'>
            <div class="description"><i class='fas fa-fw fa-code fa-nbsp ui-level-3' title='Developer Level Setting'></i>Git Branch:</div>
        </div>
        <div class='printSettingFieldCol col-md'>
            <select id='gitBranch'
                onChange="ChangeGitBranch($('#gitBranch').val());"><? PrintGitBranchOptions(); ?></select>
            <a href="#" class="btn btn-sm btn-outline-secondary ms-2" title="Reload selected branch" onclick="ReloadGitBranch(); return false;"><i class="fas fa-sync-alt" aria-hidden="true"></i></a>
            <? PrintToolTip('gitBranch'); ?>
            <div class="callout callout-danger mt-1">
                <b>Note: </b>Changing branches may take a couple minutes to recompile and may not work if you have any
                modified source files.
                <br class="mt-1"><b class="txt-danger">WARNING: Switching branches will run a "git clean -df" which will
                    remove any untracked files. If you are doing development, you may want to backup the source
                    directory before switching branches using this page.</b>
            </div>
        </div>
    </div>
    <div class="row">
        <div class='printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2 align-top'>
            <div class="description"><i class='fas fa-fw fa-code fa-nbsp ui-level-3' title='Developer Level Setting'></i>Git Status:</div>
        </div>
        <div class='printSettingFieldCol col-md'>
            <input type='button' class="buttons btn-outline-danger" value='Reset Local Changes' onClick='GitReset();'>

            <div class="callout callout-danger mt-1">
                <b>WARNING:</b> Resetting local changes performs a "git reset --hard HEAD" to revert all local source
                code changes
            </div>
            <div class="backdrop">
                <pre id='gitStatusPre'>Loading</pre>
            </div>
        </div>
    </div>
    <div class="row mt-2">
        <div class='printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2 align-top'>
            <div class="description"><i class='fas fa-fw fa-code fa-nbsp ui-level-3' title='Developer Level Setting'></i>FPP Rebuild</div>
        </div>
        <div class='printSettingFieldCol col-md'>
            <input type='button' class="buttons btn-outline-danger" value='Rebuild FPP' onClick='RebuildFPPSource();'>

            <div class="callout callout-danger mt-1">
                <b>WARNING:</b> This recompiles the local source code
            </div>

        </div>
    </div>
</div>
