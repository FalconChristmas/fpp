<?
$skipJSsettings = 1;
require_once('common.php');

$git_branch = exec("git --git-dir=".dirname(dirname(__FILE__))."/.git/ branch --list | grep '\\*' | awk '{print \$2}'", $output, $return_val);
if ( $return_val != 0 )
  $git_branch = "Unknown";
unset($output);

function filterBranch($branch) {
    if (preg_match("*v[01]\.[0-9x]*", $branch)   // very very old v0.x and v1.x branches
        || preg_match("*v2\.[0-9x]*", $branch)   // old v2.x branchs, that can no longer work (wrong lib versions)
        || preg_match("*v3\.[0-9x]*", $branch)   // old v3.x branchs, that can no longer work (wrong libhttp versions)
        || preg_match("*cpinkham*", $branch)     // privatish branches used by developers, developers should know how to flip from command line
        || preg_match("*dkulp*", $branch)
        || $branch == "new-ui"                   // some irrelevant branches at this point
        || $branch == "stage") {
        return "";
    }
    
    return $branch;
}

function PrintGitBranchOptions()
{
	global $git_branch;

  $branches = Array();
  exec("git fetch -p --all && git remote prune origin");
  exec("git --git-dir=".dirname(dirname(__FILE__))."/.git/ branch -a | grep -v -- '->' | sed -e 's/remotes\/origin\///' -e 's/\\* *//' -e 's/ *//' | sort -u", $branches);
  foreach($branches as $branch)
  {
      $branch = filterBranch($branch);
      if ($branch != "") {
        if ($branch == $git_branch) {
    //       $branch = preg_replace('/^\\* */', '', $branch);
           echo "<option value='$branch' selected>$branch</option>\n";
        } else {
     //      $branch = preg_replace('/^ */', '', $branch);
           echo "<option value='$branch'>$branch</option>\n";
        }
      }
  }
}

?>

<script language="Javascript">

function reloadGitStatus() {
    $.ajax({
        url: 'api/git/status',
        type: 'GET',
        success: function(data){ 
            if ("log" in data) {
                $('#gitStatusPre').html(data.log);
            }
        },
        error: function(data) {
            alert('Call to api/git/status failed'); 
        }
    });
}

function GitReset() {
	$.get("api/git/reset"
		).done(function() {
			reloadGitStatus();
		});
}

$( document ).ready(function() {
    reloadGitStatus();
});

</script>

<table class='settingsTable'>
    <tr><th valign='top'>Git Branch:</th>
        <td><select id='gitBranch' onChange="ChangeGitBranch($('#gitBranch').val());"><? PrintGitBranchOptions(); ?></select> <? PrintToolTip('gitBranch'); ?>
            <br><b>Note: Changing branches may take a couple minutes to recompile and may not work if you have any modified source files.</b>
            <br><font color='red'><b>WARNING: Switching branches will run a "git clean -df" which will remove any untracked files. If you are doing development, you may want to backup the source directory before switching branches using this page.</b></font></td>
    </tr>
    <tr><th valign='top'>Git Status:</th>
        <td><input type='button' value='Reset Local Changes' onClick='GitReset();'> <b>WARNING:</b> This performs a "git reset --hard HEAD" to revert all local source code changes<br></td>
    </tr>
    <tr><th colspan='2'><pre id='gitStatusPre'>Loading</pre></th>
    <tr><th valign='top'>FPP Rebuild:</th>
        <td><input type='button' value='Rebuild FPP' onClick='RebuildFPPSource();'> <b>WARNING:</b> This recompiles the local source code<br></td>
    </tr>
<?
PrintSetting('masqUIPlatform');
?>
</table>
