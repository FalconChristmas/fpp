<html>
<head>
<?php
require_once("config.php");
require_once("common.php");
require_once('common/menuHead.inc');

function print_if_match($one, $two, $print)
{
  if ( $one == $two )
    return $print;
}
if ( isset($_POST['emailguser']) && !empty($_POST['emailguser']) )
{
  $emailguser = $_POST['emailguser'];
  WriteSettingToFile("emailguser",$emailguser);
}
if ( isset($_POST['emailgpass']) && !empty($_POST['emailgpass']) )
{
  $emailgpass = $_POST['emailgpass'];
  WriteSettingToFile("emailgpass",$emailgpass);
}
if ( isset($_POST['emailfromtext']) && !empty($_POST['emailfromtext']) )
{
  $emailfromtext = $_POST['emailfromtext'];
  WriteSettingToFile("emailfromtext",$emailfromtext);
}
if ( isset($_POST['emailtoemail']) && !empty($_POST['emailtoemail']) )
{
  $emailtoemail = $_POST['emailtoemail'];
  WriteSettingToFile("emailtoemail",$emailtoemail);
}
if ( ( isset($_POST['emailguser']) && !empty($_POST['emailguser']) ) &&
   ( isset($_POST['emailgpass']) && !empty($_POST['emailgpass']) ) &&
   ( isset($_POST['emailfromtext']) && !empty($_POST['emailfromtext']) ) &&
   ( isset($_POST['emailtoemail']) && !empty($_POST['emailtoemail']) ))
{
    SaveEmailConfig($emailguser, $emailgpass, $emailfromtext, $emailtoemail);
}

?>
<title><? echo $pageTitle; ?></title>
<script>
</script>
</head>
<body>
<div id="bodyWrapper">
  <?php include 'menu.inc'; ?>
  <br/>
  <div id="uiemail" class="settings">
    <fieldset>
    <legend>Email Setup</legend>
      Outbound email requires a Gmail account to relay mail thru.  Nothing is stored in/on your
      Gmail account.  Gmail accounts are free by going to <a href="gmail.com" target="new">Gmail.com</a>.  By routing thru Gmail we go
      around when ISPs block outbound port 25 making you route email thru them.<br /><br />
      To send email from a script:<br />
      1.  Create your message to a temp file of your chosing<br>
      2.  Execute the mail utility:  mail -s "This is the subject line" root@localhost < tempfile.txt<br>
      3.  If you need to attach a file also you can use the -a attachment_file param<br>
	  All outbound email via scripts needs to be sent to <b>root@localhost</b>.
      <hr>
      <form name="email_form" action="<?php echo $_SERVER['PHP_SELF'] ?>" method="POST">
        <table width="75%" border="0" align="center" cellspacing='2' id='email'>
          <tr>
            <td width="50%" align="right">Enable Email System:</td>
            <td>
              <?php PrintSettingCheckbox("Email System: ", "emailenable", 0, 1, "1", "0"); ?>
          </tr>
          <tr>
            <td width="50%" align="right">Gmail Username:</td>
            <td><input name="emailguser" type="text" size="50" maxlength="30" value="<?php echo $emailguser; ?>"></td>
          </tr>
          <tr>
            <td align="right">Gmail Password:</td>
            <td><input name="emailgpass" type="password" size="50" maxlength="30" value="<?php echo $emailgpass; ?>"></td>
          </tr>
          <tr>
            <td align="right">Destination From Text:</td>
            <td><input name="emailfromtext" type="text" size="50" maxlength="30" value="<?php echo $emailfromtext; ?>"></td>
          </tr>
          <tr>
            <td align="right">Destination To Email:</td>
            <td><input name="emailtoemail" type="text" size="50" maxlength="30" value="<?php echo $emailtoemail; ?>"></td>
          </tr>
          <tr>
            <td colspan="2" align="center"><div id="submit">
                <input id="submit_button" name="submit_button" type="submit" class="buttons" value="Save">
              </div></td>
          </tr>
        </table>
      </form>
    </fieldset>
  </div>
  <?php require_once 'common/footer.inc'; ?>
</div>
</body>
</html>
