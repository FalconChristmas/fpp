<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<link rel="stylesheet" href="css/fpp.css" />
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<title>Untitled Document</title>

<?php 
	function IsDHCPmode()
	{
		$s = shell_exec("grep eth0 /etc/network/interfaces");
		if (strpos($s, 'dhcp') !== false)
		{
			 return true;
		}
		else
		{
			return false;	
		}
	}
	
	function PrintDHCPselected()
	{
		$s = isDHCPmode()==true ?"checked":"";
		echo $s;
	}
	function PrintStaticSelected()
	{
		$s = isDHCPmode()==true ?"":"checked";
		echo $s;
	}
	
	function PrintIPaddress()
	{
		$s = shell_exec("ip addr show scope global | grep inet | cut -d' ' -f6 | cut -d/ -f1");
		echo $s;
	}
	
	function PrintGateway()
	{
		$s = shell_exec("sudo route -n | grep 'UG[ \t]' | awk '{print $2}'");	
		//$s = shell_exec("ip addr show scope global | grep inet | cut -d' ' -f6 | cut -d/ -f1");	
		echo $s;
	}
	
	function PrintSubnet()
	{
//		$s = shell_exec("ifconfig|fgrep 'inet addr:'|fgrep -v '127'|cut -d: -f4|awk '{print $1}'|head -n1");	
		$s = shell_exec("sudo route -n | grep 'U[ \t]' | awk '{print $3}'");	
		echo $s;
	}

?>

</head>

<body>
<div id="settings-top">
<h2>FPP Settings</h2>
<fieldset>
<legend> Network Settings 
</legend>
<h5 id = "networkStatus">You must reboot for network settings to take effect.</h5>
<form>
<table>
<tr>
  <td>Address Type:</td>
	<td><input name="rbNetworkType"  type="radio" value="DHCP" <?php PrintDHCPselected();?>/>DHCP</td>
	<td><input name="rbNetworkType" type="radio" value="Static" <?php PrintStaticSelected();?>/>Static</td>
</tr>
<tr class="static">
  <td>IP Address:</td>
	<td colspan= '2'><input  name="txtIP"  type="text" value="<?php PrintIPaddress();?>"  /></td>
</tr>
<tr class="static">
  <td>Subnet Mask:</td>
	<td colspan= '2'><input  name="txtSubnetMask"  type="text" value = "<?php PrintSubnet();?>" /></td>
</tr>
<tr class="static">
  <td>Gateway:</td>
	<td colspan= '2'><input  name="txtGateway"  type="text" value = "<?php PrintGateway();?>"  /></td>
</tr>
<tr class="static">
  <td>DNS Server 1:</td>
	<td colspan= '2'><input  name="txtDNS1"  type="text"  /></td>
</tr>
<tr class="static">
  <td>DNS Server 2:</td>
	<td colspan= '2'><input  name="txtDNS2"  type="text"  /></td>
</tr>
<tr class="static">
  <td>&nbsp;</td>
  <td>&nbsp;</td>
  <td>&nbsp;</td>
</tr>
<tr class="static">
  <td>&nbsp;</td>
	<td><input class="buttons name="btnSaveNetwork" type="button" value="Save" /></td>
	<td><input class="buttons name="btnRebootNetwork" type="button" value="Reboot" /></td>
</tr>
</table>
</form>
</fieldset>
</div>
</body>
</html>