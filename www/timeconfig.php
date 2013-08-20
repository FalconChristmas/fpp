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


<div id="time" class="settings">
<fieldset>
<legend>Time Settings</legend>
RTC
<br />
Manual (disable NTP)
<br />
Time Zone (dpkg-reconfigure trick from here: http://serverfault.com/questions/84521/automate-dpkg-reconfigure-tzdata/
listing of zones: /usr/share/zoneinfo
</fieldset>
</div>


</div>
<?php	include 'common/footer.inc'; ?>
</body>
</html>
