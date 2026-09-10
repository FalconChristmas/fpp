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
        } else if (isset($settings['gitHubUser']) && strtolower(trim($settings['gitRemote'])) === strtolower(trim($settings['gitHubUser'])) ) {
            $remote = trim($settings['gitHubUser']);
        } else if (!empty($settings['gitRemote']) && preg_match('/^[a-zA-Z0-9_-]+$/', $settings['gitRemote'])) {
            // Custom fork remote (e.g. GitHub username) - use as-is after validation
            $remote = $settings['gitRemote'];
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

    function ReloadGitBranch() {
        var branch = $('#gitBranch').val();
        if (!branch) return;
        ChangeGitBranch(branch);
    }

    var currentGitBranch = "<?php echo addslashes($git_branch); ?>";
    var currentGitRemote = "<?php echo addslashes($git_branch_remote); ?>";

    function loadGitRemoteForkOption() {
        var user = $('#gitHubUser').val();
        var pat = $('#gitHubPAT').val();
        var $remoteSel = $('#gitRemote');
        // Remove any previous fork option to avoid duplicates / stale user
        $remoteSel.find('option[data-fork="1"]').remove();
        if (!user || !pat || $.trim(user) === '' || $.trim(pat) === '') {
            var curVal = $remoteSel.val();
            if (curVal && $remoteSel.find('option[value="' + curVal + '"]').length === 0) {
                // Fork was selected but credentials cleared - fall back to origin
                $remoteSel.val('origin');
                reloadGitBranches('origin');
            }
            return;
        }
        $.ajax({
            url: 'api/git/forkBranches',
            type: 'GET',
            success: function (data) {
                if (data && data.hasFork) {
                    var label = 'GitHub User Fork (' + data.user + '/fpp)';
                    var value = data.user;
                    if ($remoteSel.find('option[value="' + value + '"]').length === 0) {
                        $remoteSel.append($('<option>').attr('value', value).attr('data-fork', '1').text(label));
                    } else {
                        $remoteSel.find('option[value="' + value + '"]').attr('data-fork', '1').text(label);
                    }
                    var savedRemote = $remoteSel.val();
                    var shouldSelectFork = false;
                    if (savedRemote && savedRemote.toLowerCase() === data.user.toLowerCase()) {
                        shouldSelectFork = true;
                    } else if (currentGitRemote && currentGitRemote.toLowerCase() === data.user.toLowerCase()) {
                        shouldSelectFork = true;
                        $remoteSel.val(value);
                    }
                    if (shouldSelectFork) {
                        reloadGitBranches(value);
                    }
                } else {
                    $remoteSel.find('option[data-fork="1"]').remove();
                }
            },
            error: function () {
                $remoteSel.find('option[data-fork="1"]').remove();
            }
        });
    }

    $(document).ready(function () {
        reloadGitStatus();
        loadGitRemoteForkOption();

        // Listen for changes to the git remote setting - supports origin/newfeatures and fork user
        $('#gitRemote').on('change', function () {
            var $opt = $(this).find('option:selected');
            var remote;
            if ($opt.attr('data-fork') === '1') {
                remote = $opt.val();
            } else {
                remote = $(this).val();
                if (remote.indexOf('newfeatures') !== -1) {
                    remote = 'newfeatures';
                } else if (remote.indexOf('origin') !== -1) {
                    remote = 'origin';
                }
            }
            reloadGitBranches(remote);
        });

        // Reload fork option when GitHub credentials change (saved via SetSetting)
        var forkReloadTimer = null;
        function scheduleForkReload() {
            if (forkReloadTimer) clearTimeout(forkReloadTimer);
            forkReloadTimer = setTimeout(loadGitRemoteForkOption, 800);
        }
        $('#gitHubUser, #gitHubPAT').on('change blur', scheduleForkReload);
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
    <div class="row">
        <div class='printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2 align-top'>
            <div class="description"><i class='fas fa-fw fa-code fa-nbsp ui-level-3' title='Developer Level Setting'></i>Git Branch:</div>
        </div>
        <div class='printSettingFieldCol col-md'>
            <select id='gitBranch'
                onChange="ChangeGitBranch($('#gitBranch').val());"><? PrintGitBranchOptions(); ?></select>
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
