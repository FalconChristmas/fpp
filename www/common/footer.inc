<div id="dialog-popup"></div>
<div id="dialog-help" title="Help" style="display: none">
  <div id="helpText">
  </div>
</div>

<div id="footer">
<br />
  <div id="rebootShutdown">
    <input type='button' class='buttons wideButton' value='Run FPP Command' onClick='RunCommand();'><span class='hidden' id='runCommandJSON'></span>
    <input name="btnReboot" onClick="RebootPi();" type="button" class = "buttons" value="Reboot">
    <input name="btnShutdown" type="button" onClick="ShutdownPi();" class = "buttons" value="Shutdown">
    <input name="btnRestartFPPD" type="button" onClick="ClearRestartFlag(); RestartFPPD();" class = "buttons" value="Restart FPPD">
  </div>
<br />
<a href="http://www.falconchristmas.com">www.falconchristmas.com</a>
<br />
<br />
</div>
