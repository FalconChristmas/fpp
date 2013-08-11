
<!DOCTYPE HTML>
<html>
<head>
<meta charset="utf-8">
<?php include('common/header.htm'); ?><TITLE>Request Information</TITLE></HEAD>
<BODY BGCOLOR=#F0F0F0 TEXT=#000000>
<div class="body-wrapper">
	<div class="header">
		<?php include('common/nav_main.htm'); ?>
	</div>
<div id="content">
    
<?php
  include("cgi-bin/common_db.inc");
  $query = "Hello";	
  function saveCustomer(){
   	$link_id = db_connect();
		$name = "'" . $_POST['customerName'] . "'";
		$company = "'" . $_POST['company'] . "'";
		$phone = "'" . $_POST['phone'] ."'";
		$fax = "'" . $_POST['fax'] . "'";
		$email = "'" . $_POST['email']  . "'";
		$address = "'" . $_POST['address'] . "'";
		$city = "'" . $_POST['city'] . "'";
		$state = "'" . $_POST['state']  . "'";
		$zip = "'" . $_POST['zip']  . "'";
		$country = "'" . $_POST['country'] . "'";
		$find = "'" . $_POST['name'] . "'";
		$interests = "'" . $_POST['interests'] . "'";
		$requestDate = "'" . Date("m-d-Y  h:i:s") . " EST'";
        $query = "INSERT INTO `Customers` (`Name`, `Company` , `Email` , `Phone` , `Fax` , `Address`,`City`, `State`, `Zipcode`, `Country`, `Source`, `RequestDate`, `Interests`,`RequestReason`) 
		         VALUES ($name,$company,$email,$phone,$fax,$address,$city,$state,$zip,$country,$find, $requestDate, $interests,'');";
        $result = mysql_query($query,$link_id);

		$result = mysql_query("SELECT LAST_INSERT_ID() AS CustomerId FROM Customers",$link_id);
		$query_sql = mysql_fetch_array($result);
		return $query_sql['CustomerId'];
	}

	if (!(preg_match("/href/i", strtolower($_POST['interests']))))
  {
	  if((!($_POST['email']=="") || !($_POST['address'] == "") || !($_POST['phone'] == "")) && !( $_POST['customerName'] == ""))
	  {
		  echo "<br><H3>Your request has been sent. Thank you for your interest in ESE products.<br><br><br></H3>";
		  echo "<table cols= 2 width=40%>";
			echo "<tr><td width=20% >Name:</td><td>" .  $_POST['customerName'] . "</td></tr>";
		  echo "<tr><td width=20%>Company:</td><td>" .  $_POST['company'] . "</td></tr>";
		  echo "<tr><td width=20%>Email:</td><td>" .  $_POST['email'] . "</td></tr>";
		  echo "<tr><td width=20%>Address:</td><td>" .  $_POST['address'] . "</td></tr>";
		  echo "<tr><td width=20%>City:</td><td>" .  $_POST['city'] . "</td></tr>";
		  echo "<tr><td width=20%>State:</td><td>" .  $_POST['state'] . "</td></tr>";
		  echo "<tr><td width=20%>Zipcode:</td><td>" .  $_POST['zip'] . "</td></tr></table>";
		
		  $customerId = saveCustomer();
		  $mailTo = "ese@ese-web.com";
		  $emailSubject = "ESE-WEB.COM Request #" . $customerId;
		  $emailBody = "Name: " .  $_POST['customerName'] . "\n";
		  $emailBody .= "Company: " .  $_POST['company'] . "\n";
		  $emailBody .= "Email: " .  $_POST['email'] . "\n";
		  $emailBody .= "Phone: " .  $_POST['phone'] . "\n";
		  $emailBody .= "Fax: " .  $_POST['fax'] . "\n";
		  $emailBody .= "Address: " .  $_POST['address'] . " \n";
		  $emailBody .= "City: " .  $_POST['city'] . "\n";
		  $emailBody .= "State: " .  $_POST['state'] . "\n";
		  $emailBody .= "Zipcode: " .  $_POST['zip'] . "\n";
		  $emailBody .= "Country: " .  $_POST['country'] . "\n\n";
		  $emailBody .= "Send by mail: " .  $_POST['sendlit'] . "\n\n";
		  $emailBody .= "How did you find us: " .  $_POST['find'] . "\n\n";
		  $emailBody .= "Interests: " .  $_POST['interests'] . "\n";
		  $emailBody .= "Request Time: " . Date("m-d-Y  h:i:s") . " EST";
		  if (!($name == ""))
		  {
			if (!($company == ""))
			{
				$from = $name . " - " . $_POST['company'];
			}
			else
			{
				$from =  $_POST['customerName']; 
			}
		  }		
		  else
		  {
			$from = "No Name Given";
		  }
		  $emailHeader = "From:" . $from . " \r\nReply-to:$email\r\n";
		  mail($mailTo,$emailSubject,$emailBody,$emailHeader);
	  }
	  else
	  {
		  echo "Please click back button and supply a method for us to respond to your request. (Email, Address, or Phone)";
	  } 
  }
  else
  {
	  echo "Sorry no spamming. Thank you.";
  }
?>
</div>
  <div class="footer"><?php include 'common/footer.htm'; ?></div>
</div>
<?php include('common/analytics.htm'); ?>
</body>
</HTML>
