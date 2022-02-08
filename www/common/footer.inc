<div id="dialog-popup"></div>
<div id="dialog-help" title="Help" style="display: none">
  <div id="helpText">
  </div>
</div>

<div id="footer">
<br />
  <div id="rebootShutdown">
    <button type='button' class='buttons wideButton btn-outline-light' onClick='ShowRunCommandPopup();'><span class='hidden' id='runCommandJSON'></span><i class='fas fa-fw fa-terminal fa-nbsp'></i>Run FPP Command</button>
    <div class="dropup" id="fppModeDropdown">
      <button class="buttons btn-outline-light dropdown-toggle"  type="button" id="fppModeDropdownButton" data-toggle="dropdown" aria-haspopup="true" aria-expanded="false">
        FPP Mode: <span id="fppModeDropdownButtonModeText"></span>
      </button>
      <div class="dropdown-menu dropdown-menu-right" aria-labelledby="dropdownMenuButton"  >
        <button class="dropdown-item" onClick="SetFPPDmode('player');" data-tooltip-position-my="left+15 center" data-tooltip-touch="false" data-tooltip-position-at="right center" title="This FPP will control itself and not be controlled by another FPP">Player</button>
        <button class="dropdown-item" onClick="SetFPPDmode('remote');" data-tooltip-position-my="left+15 center" data-tooltip-touch="false" data-tooltip-position-at="right center" title="This FPP will be controlled by a Player FPP Instance">Remote</button>
      </div>
    </div>
    <button name="btnReboot" onClick="Reboot();" type="button" class="buttons btn-outline-light"><i class='fas fa-fw fa-retweet fa-nbsp'></i>Reboot</button>
    <button name="btnShutdown" type="button" onClick="Shutdown();" class = "buttons btn-outline-light"><i class='fas fa-fw fa-power-off fa-nbsp'></i>Shutdown</button>
    <button name="btnRestartFPPD" type="button" onClick="ClearRestartFlag(); RestartFPPD();" class="buttons btn-outline-light"><i class='fas fa-fw fa-sync fa-nbsp'></i>Restart FPPD</button>
    <button type="button" id="btnDaemonControl" value="" class="buttons btn-outline-light hidden" onClick="ControlFPPD();"></button>

 
 
  </div>
<br />
<div class="footerLink">
<a href="http://www.falconchristmas.com">www.falconchristmas.com</a>
</div>

<br />
<br />
</div>
