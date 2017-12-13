<html>
<head>
<?php
require_once("config.php");
include 'common/menuHead.inc';
?>
<title><? echo $pageTitle; ?></title>
<script>
</script>
</head>
<body>
<?php
class Crontab {
    
    // In this class, array instead of string would be the standard input / output format.
    
    // Legacy way to add a job:
    // $output = shell_exec('(crontab -l; echo "'.$job.'") | crontab -');
    
    static private function stringToArray($jobs = '') {
        $array = explode("\r\n", trim($jobs)); // trim() gets rid of the last \r\n
        foreach ($array as $key => $item) {
            if ($item == '') {
                unset($array[$key]);
            }
        }
        return $array;
    }
    
    static private function arrayToString($jobs = array()) {
        $string = implode("\r\n", $jobs);
        return $string;
    }
    
    static public function getJobs() {
        $output = shell_exec('crontab -l');
        return self::stringToArray($output);
    }
    
    static public function saveJobs($jobs = array()) {
        $output = shell_exec('echo "'.self::arrayToString($jobs).'" | crontab -');
        return $output;	
    }
    
    static public function doesJobExist($job = '') {
        $jobs = self::getJobs();
        if (in_array($job, $jobs)) {
            return true;
        } else {
            return false;
        }
    }
    
    static public function addJob($job = '') {
        if (self::doesJobExist($job)) {
            return false;
        } else {
            $jobs = self::getJobs();
            $jobs[] = $job;
            return self::saveJobs($jobs);
        }
    }
    
    static public function removeJob($job = '') {
        if (self::doesJobExist($job)) {
            $jobs = self::getJobs();
            unset($jobs[array_search($job, $jobs)]);
            return self::saveJobs($jobs);
        } else {
            return false;
        }
    }
    
}
?>
<div id="bodyWrapper">
<?php include 'menu.inc'; ?>
<br/>
<div id="uiscripts" class="settings">
<fieldset>
<legend>Cron Editor</legend>
<hr>
Cron jobs allow you to automate certain commands or scripts on your Pi to complete repetitive tasks
      automatically. A cron job allows you to run a certain command at times set by the job.</br>
<table id='temp' cellspacing='5' >
  <tbody>
    <tr>
      <td><img src="images/note.gif" width="31" height="37" border='0'></td>
      <td><b>NOTE:</b></br>
        Warning: You need to have a good knowledge of Linux commands before you can use cron jobs
        effectively. If you are not sure post your script on the Falcon Christmas forums for
        advise from others before adding a cron job.  In incorrect cron job can crash the OS.</td>
    </tr>
  </tbody>
</table>
<hr>
<table id='cronjobs' cellspacing='5'>
<tbody>
<table width="100%" border="0" cellpadding="2" cellspacing="2" bgcolor="#000000">
  <tr bgcolor="#000000">
    <td colspan="7"><font color="#FFFFFF" face="Verdana, Geneva, sans-serif"><b>Now:</b></font></td>
  </tr>
  <tr align="center" bgcolor="#FFFFFF"><font color="#000000" face="Verdana, Geneva, sans-serif">
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>0</td>
    <td width="100%" colspan="2">Live Time</td>
    </font></tr>
  <tr bgcolor="#FFFFFF">
    <td colspan="7"></td>
  </tr>
  <tr align="center" valign="middle" bgcolor="#CCCCCC">
    <td>Min</td>
    <td>Hour</td>
    <td>Day</td>
    <td>Month</td>
    <td>D/Wk</td>
    <td width="100%">Command</td>
    <td></td>
  </tr>
  <tr bgcolor="#FFFFFF">
    <td>*</td>
    <td>*</td>
    <td>*</td>
    <td>*</td>
    <td>*</td>
    <td>&nbsp;</td>
    <td>DEL</td>
  </tr>
  <tr bgcolor="#CCCCCC">
    <td colspan="7"></td>
  </tr>
    <tr bgcolor="#FFFFFF">
    <td>*</td>
    <td>*</td>
    <td>*</td>
    <td>*</td>
    <td>*</td>
    <td>&nbsp;</td>
    <td>ADD</td>
  </tr>
  </tbody>
  
</table>
</fieldset>
</div>
<?php include 'common/footer.inc'; ?>
</div>
</body>
</html>
