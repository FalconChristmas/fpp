<?
header("Access-Control-Allow-Origin: *");

$wrapped = 0;

if (isset($_GET['wrapped']))
  $wrapped = 1;

if (!$wrapped)
  echo "<html>\n";

$skipJSsettings = 1;
require_once("common.php");

DisableOutputBuffering();

if (!$wrapped) {
  ?>

  <head>
    <title>
      FPP Manual Update
    </title>
  </head>

  <body>
    <h2>FPP Manual Update</h2>
    <pre>
      <?
}
?>
Pulling in updates...
<?
$startTime = microtime(true);
system("$fppDir/scripts/git_pull");
$endTime = microtime(true);
$diffTime = round($endTime - $startTime);

$h = floor($diffTime / 3600);
$m = floor($diffTime % 3600 / 60);
$s = floor($diffTime % 60);

printf("----------------------\nElapsed Time: %02d:%02d:%02d\n", $h, $m, $s);
?>
==========================================================================
Restarting fppd...
<?

// Compare and copy apache config if needed
$srcConf = "/opt/fpp/etc/apache2.site";
$dstConf = "/etc/apache2/sites-enabled/000-default.conf";
$needCopy = true;

if (file_exists($srcConf) && file_exists($dstConf)) {
  // Compare file hashes
  if (md5_file($srcConf) === md5_file($dstConf)) {
    $needCopy = false;
  }
}

if ($needCopy) {
  echo "Updating Apache config...\n";
  // Use sudo if needed for permissions
  system("$SUDO cp $srcConf $dstConf");
} else {
  echo "Apache config is already up to date.\n";
}

touch("$mediaDirectory/tmp/fppd_restarted");

system($SUDO . " $fppDir/scripts/fppd_restart");

system($SUDO . " $fppDir/scripts/ManageApacheContentPolicy regenerate");

if (file_exists($settings['statsFile'])) {
  unlink($settings['statsFile']);
}

exec($SUDO . " rm -f /tmp/cache_*.cache");
if (file_exists($fppDir . "/src/fppd")) {
  print ("==========================================================================\n");
  print ("Upgrade Complete.\n");
} else {
  print ("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
  print ("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
  print ("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
  print ("Upgrade FAILED.\n");
  print ("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
  print ("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
  print ("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
}

if (!$wrapped) {
  ?>
      <a href='index.php'>Go to FPP Main Status Page</a><br>
      <a href='about.php'>Go back to FPP About page</a><br>

      </body>
      </html>
      <?
}
?>