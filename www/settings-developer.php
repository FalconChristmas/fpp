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

    // Pull Requests is synthetic – handled client-side via api/git/pullRequests
    if (isset($settings['gitRemote']) && $settings['gitRemote'] === 'pull-requests') {
        // Pre-fetch is done via GitHub API; server renders placeholder.
        // Detect if we are already on a pr-* local branch and select it.
        if (preg_match('/^pr-\d+$/', $git_branch)) {
            echo "<option value='$git_branch' selected>$git_branch</option>\n";
        }
        return;
    }

    $branches = array();
    // Get remote from settings, default to 'origin' if not set
    $remote = 'origin'; // default
    if (isset($settings['gitRemote'])) {
        // Handle both old and new format - extract remote name from value
        if ($settings['gitRemote'] === 'pull-requests') {
            $remote = 'origin';
        } else if (strpos($settings['gitRemote'], 'newfeatures') !== false) {
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
        if (remote === 'pull-requests') {
            reloadPullRequests();
            return;
        }
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
                $('#prMeta').hide();
            },
            error: function (data) {
                alert('Call to api/git/branches failed');
            }
        });
    }

    var prCache = [];
    function formatPRLabel(pr) {
        var label = '#' + pr.number + ' - ' + (pr.user || 'unknown');
        if (pr.draft) label = '[Draft] ' + label;
        if (label.length > 60) label = label.substring(0, 59) + '…';
        return label;
    }
    function reloadPullRequests() {
        $('#gitBranch').empty().append('<option value="" selected>Loading pull requests…</option>');
        $('#prMeta').hide();
        $.ajax({
            url: 'api/git/pullRequests',
            type: 'GET',
            success: function (prs) {
                prCache = Array.isArray(prs) ? prs : [];
                $('#gitBranch').empty();
                $('#gitBranch').append('<option value="" selected>-- Select Pull Request --</option>');
                if (prCache.length === 0) {
                    $('#gitBranch').append('<option value="" disabled>No open pull requests</option>');
                    $('#prMeta').html('No open pull requests on FalconChristmas/fpp or GitHub unreachable.').show();
                    return;
                }
                prCache.forEach(function (pr) {
                    var val = 'pr-' + pr.number;
                    var label = formatPRLabel(pr);
                    var titleEsc = $('<div>').text(pr.title || '').html();
                    $('#gitBranch').append('<option value="' + val + '" data-pr="' + pr.number + '" title="' + titleEsc + '">' + label + '</option>');
                });
            },
            error: function () {
                $('#gitBranch').empty().append('<option value="" selected>-- Select Pull Request --</option>');
                $('#gitBranch').append('<option value="" disabled>Failed to load PRs</option>');
                $('#prMeta').html('Failed to load pull requests from GitHub.').show();
            }
        });
    }
    function updatePRMeta() {
        var val = $('#gitBranch').val();
        var prNum = parseInt($('#gitBranch option:selected').attr('data-pr') || '0', 10);
        var pr = null;
        for (var i = 0; i < prCache.length; i++) { if (prCache[i].number === prNum) { pr = prCache[i]; break; } }
        if (!pr) { $('#prMeta').hide(); return; }
        var html = '<b>PR #' + pr.number + '</b> ' + (pr.draft ? '<span class="badge bg-secondary">Draft</span> ' : '') + $('<div>').text(pr.title || '').html() + ' by <b>' + $('<div>').text(pr.user || '').html() + '</b> – <code>' + $('<div>').text(pr.headRef || '').html() + '</code> <a href="' + pr.htmlUrl + '" target="_blank" rel="noopener">View on GitHub</a>';
        $('#prMeta').html(html).show();
    }

    function ReloadGitBranch() {
        var branch = $('#gitBranch').val();
        if (!branch) return;
        // pull-requests mode needs pr number
        if ($('#gitRemote').val() === 'pull-requests') {
            var prNum = $('#gitBranch option:selected').attr('data-pr');
            if (!prNum) return;
            ChangeGitBranch(branch, prNum);
            return;
        }
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
        // If saved remote is Pull Requests, populate PR list immediately
        if ($('#gitRemote').val() === 'pull-requests') {
            reloadPullRequests();
        }
        loadGitRemoteForkOption();

        // Wrap ChangeGitBranch to handle PR mode (pull-requests synthetic remote)
        var _origChangeGitBranch = window.ChangeGitBranch;
        window.ChangeGitBranch = function (branch, prNum) {
            var remote = $('#gitRemote').val() || 'origin';
            if (remote === 'pull-requests') {
                // branch is pr-<num>, extract number if not supplied
                var num = prNum || (branch && branch.indexOf('pr-') === 0 ? branch.substring(3) : $('#gitBranch option:selected').attr('data-pr'));
                if (!num) { alert('Select a pull request'); return; }
                branch = 'pr-' + String(num).replace(/[^0-9]/g,'');
                if (!branch || !confirm("Are you really sure you want to switch to PR #" + num + " ('" + branch + "') branch?  This may take some time and it may not be fully compatible with this FPP OS version.  Click 'OK' to continue.")) {
                    location.reload(true); return;
                }
                location.href = 'changebranch.php?branch=' + encodeURIComponent(branch) + '&remote=pull-requests&pr=' + encodeURIComponent(num);
                return;
            }
            if (typeof _origChangeGitBranch === 'function') return _origChangeGitBranch(branch);
            // fallback
            if (!branch || !confirm("Are you really sure you want to switch to the '" + branch + "' branch?  This may take some time and it may not be fully compatible with this FPP OS version.  Click 'OK' to continue.")) { location.reload(true); return; }
            location.href = 'changebranch.php?branch=' + encodeURIComponent(branch) + '&remote=' + encodeURIComponent(remote);
        };

        // Keep branch meta in sync for PR mode
        $('#gitBranch').on('change', function () {
            if ($('#gitRemote').val() === 'pull-requests') updatePRMeta();
            else $('#prMeta').hide();
        });

        // Listen for changes to the git remote setting - supports origin/newfeatures/fork/pull-requests
        $('#gitRemote').on('change', function () {
            var $opt = $(this).find('option:selected');
            var remote;
            if ($opt.attr('data-fork') === '1') {
                remote = $opt.val();
            } else {
                remote = $(this).val();
                if (remote === 'pull-requests') {
                    reloadPullRequests();
                    return;
                } else if (remote.indexOf('newfeatures') !== -1) {
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
            <div id="prMeta" class="callout callout-info mt-1" style="display:none"></div>
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
