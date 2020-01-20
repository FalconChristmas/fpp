<?
$cmd = "cd " . dirname(dirname(__FILE__)) . " && /usr/bin/git status";
$git_status = "Unknown";
exec($cmd, $output, $return_val);
if ( $return_val == 0 )
	$git_status = implode("\n", $output) . "\n";
unset($output);

$git_branch = exec("git --git-dir=".dirname(dirname(__FILE__))."/.git/ branch --list | grep '\\*' | awk '{print \$2}'", $output, $return_val);
if ( $return_val != 0 )
  $git_branch = "Unknown";
unset($output);

function filterBranch($branch) {
    if (preg_match("*v[01]\.[0-9x]*", $branch)   // very very old v0.x and v1.x branches
        || preg_match("*v2\.[0-5x]*", $branch)   // old v2.x branchs, that can no longer work (wrong lib versions)
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

$platforms = Array();
    $platforms['Default'] = '';
    $platforms['BeagleBone Black'] = 'BeagleBone Black';
    $platforms['CHIP'] = 'CHIP';
    $platforms['Debian'] = 'Debian';
    $platforms['FreeBSD'] = 'FreeBSD';
    $platforms['Linux'] = 'Linux';
    $platforms['ODROID'] = 'ODROID';
    $platforms['OrangePi'] = 'OrangePi';
    $platforms['Pine64'] = 'Pine64';
    $platforms['PogoPlug'] = 'PogoPlug';
    $platforms['qemu'] = 'qemu';
    $platforms['Raspberry Pi'] = 'Raspberry Pi';
    $platforms['Ubuntu'] = 'Ubuntu';

?>

<script language="Javascript">
function GitReset() {
	$.get("fppxml.php?command=resetGit"
		).done(function() {
			$('#gitStatusPre').load('fppxml.php?command=gitStatus');
		});
}

</script>

<table class='settingsTable'>
    <tr><td valign='top'>Git Branch:</td><td><select id='gitBranch' onChange="ChangeGitBranch($('#gitBranch').val());"><? PrintGitBranchOptions(); ?></select> <? stt('gitBranch'); ?>
            <br><b>Note: Changing branches may take a couple minutes to recompile and may not work if you have any modified<br>source files.</b>
            <br><font color='red'><b>WARNING: Switching branches will run a "git clean -df" which will remove any untracked files. If you are doing<br>development, you may want to backup the source directory before switching branches using this page.</b></font></td>
    </tr>
    <tr><td valign='top'>Git Status:</td>
        <td><input type='button' value='Reset Local Changes' onClick='GitReset();'> <b>WARNING:</b> This performs a "git reset --hard HEAD" to revert all local source code changes<br></td>
    </tr>
    <tr><td colspan='2'><pre id='gitStatusPre'><? echo $git_status; ?></pre></td>
<? if ($uiLevel >= 3) { ?>
    <tr><td>UI Platform Masq:</td>
        <td><? PrintSettingSelect("UI Platform Masquerade", "masqUIPlatform", 0, 0, '', $platforms, '', 'reloadSettingsPage'); stt('masqUIPlatform'); ?></td>
    </tr>
<? } ?>
</table>
