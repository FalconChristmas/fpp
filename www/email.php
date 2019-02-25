<!DOCTYPE html>
<html>
<head>
<?php
require_once("config.php");
require_once("common.php");
require_once('common/menuHead.inc');

if (isset($_POST['action']))
{
	if ($_POST['action'] == 'configureemail')
	{
		SaveEmailConfig($settings['emailguser'], $settings['emailgpass'], $settings['emailfromtext'], $settings['emailtoemail']);
	}
	else if ($_POST['action'] == 'testemail')
	{
		system('echo "Email test from $(hostname)" | mail -s "Email test from $(hostname)" root@localhost');
	}
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
      Gmail account.  Gmail accounts are free by going to <a href="https://gmail.com" target="new">Gmail.com</a>.  By routing thru Gmail we go
      around when ISPs block outbound port 25 making you route email thru them.<br /><br />
      To send email from a script:<br />
      1.  Create your message to a temp file of your chosing<br>
      2.  Execute the mail utility:  mail -s "This is the subject line" root@localhost < tempfile.txt<br>
      3.  If you need to attach a file also you can use the -a attachment_file param<br>
	  All outbound email via scripts needs to be sent to <b>root@localhost</b>.
      <hr>
        <table width="75%" border="0" align="center" cellspacing='2' id='email'>
          <tr>
            <td width="50%" align="right">Enable Email System:</td>
            <td><?php PrintSettingCheckbox("Email System: ", "emailenable", 0, 1, "1", "0"); ?></td>
          </tr>
          <tr>
            <td width="50%" align="right">Gmail Username:</td>
            <td><? PrintSettingTextSaved("emailguser", 0, 0, 50, 30); ?></td>
          </tr>
          <tr>
            <td align="right">Gmail Password:</td>
            <td><? PrintSettingPasswordSaved("emailgpass", 0, 0, 50, 30); ?></td>
          </tr>
          <tr>
            <td align="right">Destination From Text:</td>
            <td><? PrintSettingTextSaved("emailfromtext", 0, 0, 50, 30); ?></td>
          </tr>
          <tr>
            <td align="right">Destination To Email:</td>
            <td><? PrintSettingTextSaved("emailtoemail", 0, 0, 50, 30); ?></td>
          </tr>
          <tr>
            <td colspan="2" align="center"><div id="submit">
              <form name="email_form" action="<?php echo $_SERVER['PHP_SELF'] ?>" method="POST">
                <input type='hidden' name='action' value='configureemail'>
                <input id="submit_button" name="submit_button" type="submit" class="buttons" value="Configure mail">
              </form>
              &nbsp;&nbsp;
              <form name="email_form" action="<?php echo $_SERVER['PHP_SELF'] ?>" method="POST">
                <input type='hidden' name='action' value='testemail'>
                <input id="submit_button" name="submit_button" type="submit" class="buttons" value="Test mail">
              </form>
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
