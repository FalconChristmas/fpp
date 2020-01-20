<?

$locales = Array();
foreach (scandir($fppDir . '/etc/locale') as $file)
{
    if (preg_match('/.json$/', $file))
    {
        $file = preg_replace('/.json$/', '', $file);
        $locales[$file] = $file;
    }
}
ksort($locales);


$BBBLeds = Array();
    $BBBLeds['Disabled'] = "none";
    $BBBLeds['Heartbeat'] = "heartbeat";
    $BBBLeds['SD Card Activity'] = "mmc0";
    $BBBLeds['eMMC Activity'] = "mmc1";
    $BBBLeds['CPU Activity'] = "cpu";

$BBBPowerLed = Array();
    $BBBPowerLed['Disabled'] = 0;
    $BBBPowerLed['Enabled'] = 1;

$ledTypes = Array();
    $ledTypes['Disabled'] = 0;
    $ledTypes['128x64 I2C (SSD1306)'] = 1;
    $ledTypes['128x64 Flipped I2C (SSD1306)'] = 2;
    $ledTypes['128x64 2 Color I2C (SSD1306)'] = 7;
    $ledTypes['128x64 2 Color Flipped I2C (SSD1306)'] = 8;
    $ledTypes['128x32 I2C (SSD1306)'] = 3;
    $ledTypes['128x32 Flipped I2C (SSD1306)'] = 4;
    $ledTypes['128x64 I2C (SH1106)'] = 5;
    $ledTypes['128x64 Flipped I2C (SH1106)'] = 6;
    $ledTypes['128x128 I2C (SSD1327)'] = 9;
    $ledTypes['128x128 Flipped I2C (SSD1327)'] = 10;

?>

<table class='settingsTable'>
    <tr><td>Locale:</td>
        <td><? PrintSettingSelect("Locale", "Locale", 2, 0, isset($settings['Locale']) ? $settings['Locale'] : "global", $locales); stt('Locale'); ?>
    </tr>

<? if ($settings['Platform'] == "BeagleBone Black") { ?>
    <tr><td valign='top'>BeagleBone LEDs:</td>
        <td>
            <table border=0 cellpadding=0 cellspacing=5 id='BBBLeds'>
                <tr><td valign=top>USR0:</td><td><? PrintSettingSelect("USR0 LED", "BBBLeds0", 0, 0, 'heartbeat', $BBBLeds); ?></td></tr>
                <tr><td valign=top>USR1:</td><td><? PrintSettingSelect("USR1 LED", "BBBLeds1", 0, 0, 'mmc0', $BBBLeds); ?></td></tr>
                <tr><td valign=top>USR2:</td><td><? PrintSettingSelect("USR2 LED", "BBBLeds2", 0, 0, 'cpu', $BBBLeds); ?></td></tr>
                <tr><td valign=top>USR3:</td><td><? PrintSettingSelect("USR3 LED", "BBBLeds3", 0, 0, 'mmc1', $BBBLeds); ?></td></tr>
                <tr><td valign=top>Power:</td><td><? PrintSettingSelect("Power LED", "BBBLedPWR", 0, 0, 1, $BBBPowerLed); ?></td></tr>
            </table>
        </td>
    </tr>
<? } ?>

<? if ($settings['Platform'] == "Raspberry Pi") { ?>
    <tr><td width = "45%">Blank screen on startup:</td>
        <td width = "55%"><? PrintSettingCheckbox("Screensaver", "screensaver", 0, 1, "1", "0"); stt('screensaver'); ?></td>
    </tr>
<?     if ($uiLevel >= 1) { ?>
    <tr><td>Force HDMI Display:</td>
        <td><? PrintSettingCheckbox("Force HDMI Display", "ForceHDMI", 0, 1, "1", "0"); stt('ForceHDMI'); ?></td>
    </tr>
<?     } ?>
<? } ?>

<? if ( file_exists("/dev/i2c-1") || file_exists("/dev/i2c-2") ) { ?>
    <tr><td>OLED Status Display:</td>
        <td><? PrintSettingSelect("OLED Status Display", "LEDDisplayType", 0, 1, isset($settings['LEDDisplayType']) ? $settings['LEDDisplayType'] : "", $ledTypes); stt('LEDDisplayType'); ?>
    </tr>
<? } ?>

<? if ($uiLevel >= 1) { ?>
    <tr><td>Boot Delay</td>
        <td><? PrintSettingSelect("Boot Delay", "bootDelay", 0, 0, "0", Array('0s' => '0', '1s' => '1', '2s' => '2', '3s' => '3', '4s' => '4', '5s' => '5', '6s' => '6', '7s' => '7', '8s' => '8', '9s' => '9', '10s' => '10', '15s' => '10', '20s' => '20', '25s' => '25', '30s' => '30')); stt('bootDelay'); ?></td>
    </tr>
<? } ?>

</table>
