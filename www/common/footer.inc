<div id="dialog-popup"></div>
<div id="dialog-help" title="Help" style="display: none">
  <div id="helpText">
  </div>
</div>

<div id="footer">
<br />
  <div id="rebootShutdown">
    <input type='button' class='buttons wideButton btn-outline-light' value='Run FPP Command' onClick='ShowRunCommandPopup();'><span class='hidden' id='runCommandJSON'></span>
    <input name="btnReboot" onClick="RebootPi();" type="button" class = "buttons btn-outline-light" value="Reboot">
    <input name="btnShutdown" type="button" onClick="ShutdownPi();" class = "buttons btn-outline-light" value="Shutdown">
    <input name="btnRestartFPPD" type="button" onClick="ClearRestartFlag(); RestartFPPD();" class = "buttons btn-outline-light" value="Restart FPPD">
  </div>
<br />
<div class="footerLink">
<a href="http://www.falconchristmas.com">www.falconchristmas.com</a>
</div>

<br />
<br />
</div>
