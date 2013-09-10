<?php

$thisdir = dirname(__FILE__);

// No other checking here, we're assuming that since they're able to POST apache has
// already taken care of validating a user.
if ( !empty($_POST) && $_POST["password"] == "disabled" )
{
  shell_exec("rm -f $thisdir/.htpasswd $thisdir/.htaccess");
}

if ( isset($_POST['password1']) && isset($_POST['password2']))
{
  if (($_POST['password1'] != "") && ($_POST['password1'] == $_POST['password2']))
  {
    // true - setup .htaccess & save it
    file_put_contents("$thisdir/.htaccess", "AuthUserFile $thisdir/.htpasswd\nAuthType Basic\nAuthName FPP-GUI\nRequire valid-user\n");
    $setpassword = shell_exec("htpasswd -cbd $thisdir/.htpasswd admin " . $_POST['password1']);
  }
}

function checked_if_equal($value1, $value2)
{
  if ( $value1 == $value2 )
  {
    echo "checked=\"checked\"";
  }
}

function hide_if_equal($value1, $value2)
{
  if ( $value1 == $value2 )
  {
    echo "style=\"display: none\"";
  }
}

$pw = file_exists("$thisdir/.htpasswd");

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
  <div id = "uipassword">
    <fieldset>
      <legend>UI Password</legend>
      <FORM NAME="password_form" ACTION="<?php echo $_SERVER['PHP_SELF'] ?>" METHOD="POST">
        <table width= "100%" border="0" cellpadding="2" cellspacing="2">
          <tr>
            <td colspan="3" align="center">Enter a matching passwors below to add/replace a password requirement to access the web FPP GUI</td>
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
            <td colspan="3" align="center"><input id="submit_button" name="submit_button" type="submit" disabled class="buttons" value="Submit"></td>
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
