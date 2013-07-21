<?php
$page_num = isset($_GET['example']) ? $_GET['example']:'';
$pages = array(
	array('classic_theme.php','Classic Theme'),
	array('basic.php', 'Basic set up'),
	array('drag_drop.php', 'Upload by Drag & Drop'),
	array('file_ext.php', 'Restrict file extension'),
	array('auto.php', 'Auto start'),
	array('api_calls.php', 'API calls'),
	array('events.php', 'Events and Options'),
	array('form.php', 'Form Integration and File rename'),
	array('form_validation.php', 'Advanced Form Integration'),
	array('fallback.php', 'No JS fallback')
);

?>

<!DOCTYPE html>
<html>
<head>
<?php include 'common/menuHead.inc'; ?>
<script type="text/javascript" src="/js/fpp.js"></script>
<SCRIPT TYPE="text/javascript" SRC="/js/verifynotify.js"></SCRIPT>
<title>Falcon PI Player - FPP</title>
</head>
<body>
<div id="bodyWrapper">
  <?php include 'menu.inc'; ?>
  <br/>
  <div id = "uipassword">
    <fieldset>
      <legend>UI Password</legend>
          <FORM NAME="password_form" ACTION="" METHOD="POST">
      <table width= "100%" border="1">
        <tr>
          <td align="right">Password:</td><td colspan="2"><INPUT name="password1" type="password" onKeyUp="verify.check()" size="30" maxlength="30"></td></td>
        </tr>
        <tr>
          <td align="right">Re-enter Password:</td><td><INPUT NAME=password2 TYPE=password onKeyUp="verify.check()" size="30" maxlength="30"></td></td><td><DIV ID="password_result">&nbsp;</DIV></td>
        </tr>
        <tr>
          <td colspan="3" align="center">Button Here</td>
        </tr>
      </table>
      <SCRIPT TYPE="text/javascript">
<!--
verify = new verifynotify();
verify.field1 = document.password_form.password1;
verify.field2 = document.password_form.password2;
verify.result_id = "password_result";
verify.match_html = "<SPAN STYLE=\"color:blue\">Thank you, your passwords match!<\/SPAN>";
verify.nomatch_html = "<SPAN STYLE=\"color:red\">Please make sure your passwords match.<\/SPAN>";

// Update the result message
verify.check();

// -->
</SCRIPT> 
</FORM>
    </fieldset>
  </div>
</div>
<?php	include 'common/footer.inc'; ?>
</body>
</html>
