<?php require_once('common.php'); ?>

<!DOCTYPE html>
<html>
<head>
<?php include 'common/menuHead.inc'; ?>
<script type="text/javascript" src="/js/fpp.js"></script>
<title>Falcon PI Player - FPP</title>
</head>
<body>
<div id="bodyWrapper">
<?php include 'menu.inc'; ?>
<?php

function PopulateInterfaces()
{
  $interfaces = explode("\n",trim(shell_exec("/sbin/ifconfig | cut -f1 -d' ' | grep -v ^$ | grep -v lo")));
  $ifaceE131 =  ReadSettingFromFile("E131interface");
  error_log("$ifaceE131:" . $ifaceE131);
  foreach ($interfaces as $iface)
  {
    $ifaceChecked = $iface == $ifaceE131 ? " selected" : "";
    echo "<option value='" . $iface . "'" . $ifaceChecked . ">" . $iface . "</option>"; 
  }
}
?>
<br>
<div class="settings2">
  <fieldset>
    <legend>Network Settings </legend>
    <table>
      <tr>
        <td>E131 Interface:</td>
        <td><select id="selInterfaces" onChange="SetE131interface();">
            <?php PopulateInterfaces(); ?>
          </select></td>
      </tr>
    </table>
  </fieldset>
</div>
</div>
<?php include 'common/footer.inc'; ?>
</body>
</html>
