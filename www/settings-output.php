<table class='settingsTable'>
    <tr><td>Always transmit channel data:</td>
        <td><? PrintSettingCheckbox("Always Transmit", "alwaysTransmit", 2, 0, "1", "0"); stt('alwaysTransmit'); ?></td>
    </tr>

<? if ($uiLevel >= 1) { ?>
    <tr><td>E1.31 Bridge Mode Transmit Interval</td>
        <td><? PrintSettingSelect("E1.31 Bridging Transmit Interval", "E131BridgingInterval", 2, 0, "50", Array('10ms' => '10', '25ms' => '25', '40ms' => '40', '50ms' => '50', '100ms' => '100')); stt('E131BridgingInterval'); ?></td>
    </tr>

<? } ?>
</table>
