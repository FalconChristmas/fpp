<?php
if (($_POST[password1] != "") && ($_POST[password1] == $_POST[password2]))
  {
    // true - save it
    $setpassword = shell_exec('hpasswd -cbd /home/pi/www/.htpasswd $_POST[login] $_POST[password1]');
    $pwchanged = "1";
  }
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
        <table width= "100%" border="0" cellpadding="2" cellspacing="2">
          <tr>
            <td colspan="3" align="center">Enter a password below to add/replace a password requirement to access the web GUI</td>
          </tr>
          <tr>
            <td colspan="3" align="center"><input name="login" type="hidden" value="admin"></td>
          </tr>
          <tr>
            <td width="18%" align="right">Password:</td>
            <td colspan="2"><INPUT name="password1" type="password" onKeyUp="verify.check()" size="40" maxlength="40"></td>
          </tr>
          <tr>
            <td align="right">Confirm Password:</td>
            <td width="43%"><INPUT NAME="password2" TYPE="password" onKeyUp="verify.check()" size="40" maxlength="40"></td>
            <td width="39%"><DIV ID="password_result">&nbsp;</DIV></td>
          </tr>
          <tr>
            <td colspan="3" align="center">&nbsp;</td>
          </tr>
          <tr>
            <td colspan="3" align="center"><input name="submit_button" type="submit" class="buttons" value="Submit"></td>
          </tr>
        </table>
        <SCRIPT TYPE="text/javascript">
        <!--
        verify = new verifynotify();
        verify.field1 = document.password_form.password1;
        verify.field2 = document.password_form.password2;
        verify.result_id = "password_result";
        verify.match_html = "<SPAN STYLE=\"color:blue\">Thank you, your passwords match!<\/SPAN>";
        verify.nomatch_html = "<SPAN STYLE=\"color:red\">Enter matching passwords to set/change.<\/SPAN>";
        
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
