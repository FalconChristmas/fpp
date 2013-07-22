<?php
$ifconfig = shell_exec('/sbin/ifconfig eth0');
preg_match('/addr:([\d\.]+)/', $ifconfig, $ipaddress);
preg_match('/Mask:([\d\.]+)/', $ifconfig, $netmask);
$iproute = shell_exec('/sbin/ip route');
preg_match('/via ([\d\.]+)/', $iproute, $gateway);
$ipdns = shell_exec('/bin/cat /etc/resolv.conf | grep nameserver');
preg_match('/nameserver ([\d\.]+)/', $ipdns, $nameserver);
$handle = fopen("/etc/hostname", "r"); $hostname = trim(fgets($handle)); fclose($handle);




?>
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
  <br/>
  <div id = "networkconfig">
    <fieldset>
      <legend>Network Config</legend>
      <FORM NAME="netconfig" ACTION="" METHOD="POST">
         <div id= "divEthernet">
            <fieldset class="fs">
              <legend> Ethernet </legend>
              <div class='right'>
              
              </div>
            </fieldset>
          </div>
          <div id= "divWireless">
            <fieldset  class="fs">
              <legend>Wireless </legend>
              <div class='right'>
              </div>
            </fieldset>
          </div>
      <div class = "clear"></div>
	  <div id="divNetwork"><br /><input name="btnSetNetwork" type="submit" class = "Buttons"></div>
     </FORM>
    </fieldset>
  </div>
</div>
<?php include 'common/footer.inc'; ?>
</body>
</html>