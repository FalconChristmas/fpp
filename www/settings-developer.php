<?
$skipJSsettings = 1;
require_once('common.php');

$git_branch = exec("$SUDO git --git-dir=" . dirname(dirname(__FILE__)) . "/.git/ branch --list | grep '\\*' | awk '{print \$2}'", $output, $return_val);
if ($return_val != 0)
    $git_branch = "Unknown";
unset($output);

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
    global $git_branch, $SUDO, $settings;

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

    exec("$SUDO git fetch -p --all && sudo git remote prune $remote");

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

    $(document).ready(function () {
        reloadGitStatus();

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
    });

</script>
<h2>Developer Settings</h2>
<div class="settingsTable container-fluid">
    <?
    PrintSetting('masqUIPlatform');
    PrintSetting('gitRemote');
    ?>
    <div class="row">
        <div class='printSettingLabelCol col-md-4 col-lg-3 col-xxxl-2 align-top'>
            <div class="description"><i class='fas fa-fw fa-nbsp ui-level-0'></i>Git Branch:</div>
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
            <div class="description"><i class='fas fa-fw fa-nbsp ui-level-0'></i>Git Status:</div>
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
            <div class="description"><i class='fas fa-fw fa-nbsp ui-level-0'></i>FPP Rebuild</div>
        </div>
        <div class='printSettingFieldCol col-md'>
            <input type='button' class="buttons btn-outline-danger" value='Rebuild FPP' onClick='RebuildFPPSource();'>

            <div class="callout callout-danger mt-1">
                <b>WARNING:</b> This recompiles the local source code
            </div>

        </div>
    </div>
</div>