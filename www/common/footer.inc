<div id="dialog-popup"></div>
<div id="dialog-help" title="Help" style="display: none">
  <div id="helpText">
  </div>
</div>


<!-- Modal -->
<div class="modal fade" id="modalDialogBase" tabindex="-1" data-bs-backdrop="static" data-bs-keyboard="false"
  aria-labelledby="modalDialogLabel" aria-hidden="true">
  <div class="modal-dialog">
    <div class="modal-content">
      <div class="modal-header">
        <h3 class="modal-title fs-5" id="modalDialogLabel"></h3>
        <button id="modalCloseButton" type="button" class="btn-close" data-bs-dismiss="modal"
          aria-label="Close"></button>
      </div>
      <div class="modal-body"></div>
      <div class="modal-footer"></div>
    </div>
  </div>
</div>


<div id="footer">
  <br>
  <div id="rebootShutdown">
    <button type='button' class='buttons wideButton btn-outline-light' onClick='ShowRunCommandPopup();'><span
        class='hidden' id='runCommandJSON'></span><i class='fas fa-fw fa-terminal fa-nbsp'></i>Run FPP Command</button>
    <div class="dropup" id="fppModeDropdown">
      <button class="buttons btn-outline-light dropdown-toggle" type="button" id="fppModeDropdownButton"
        data-bs-toggle="dropdown" aria-haspopup="true" aria-expanded="false">
        FPP Mode: <span id="fppModeDropdownButtonModeText"></span>
      </button>
      <div class="dropdown-menu dropdown-menu-right" aria-labelledby="fppModeDropdownButton">
        <button class="dropdown-item" onClick="SetFPPDmode('player');" data-bs-toggle="tooltip"
          data-bs-placement="right"
          data-bs-title="This FPP will control itself and not be controlled by another FPP">Player</button>
        <button class="dropdown-item" onClick="SetFPPDmode('remote');" data-bs-toggle="tooltip"
          data-bs-placement="right" data-bs-title="This FPP will be controlled by a Player FPP Instance">Remote</button>
      </div>
    </div>
    <? if ($settings["Platform"] != "MacOS") { ?>
      <button name="btnReboot" onClick="Reboot();" type="button" class="buttons btn-outline-light"><i
          class='fas fa-fw fa-retweet fa-nbsp'></i>Reboot</button>
      <button name="btnShutdown" type="button" onClick="Shutdown();" class="buttons btn-outline-light"><i
          class='fas fa-fw fa-power-off fa-nbsp'></i>Shutdown</button>
    <? } ?>
    <button name="btnRestartFPPD" type="button" onClick="ClearRestartFlag(); RestartFPPD();"
      class="buttons btn-outline-light"><i class='fas fa-fw fa-sync fa-nbsp'></i>Restart FPPD</button>
    <button type="button" id="btnDaemonControl" value="" class="buttons btn-outline-light hidden"
      onClick="ControlFPPD();" aria-label="DaemonControl"></button>



  </div>
  <br>
  <div class="footerLink">
    <a href='<?= $themeInfo["FOOTER_URL"] ?>'><?= $themeInfo["FOOTER_URL_TEXT"] ?></a>
    <?
    if ($themeInfo["FOOTER_URL_TEXT"] != "www.falconplayer.com") {
      echo "<br>\n<p style='font-size: 70%'>Powered by FPP</p>\n";
    }
    ?>
  </div>

  <br>
  <br>
</div>

</div> <!-- div for fppOuterContainer opened in menu.inc -->