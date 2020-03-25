<!DOCTYPE html>
<html>
<?php
require_once('config.php');
require_once("common.php");
?>
<head>
<script type="text/javascript" src="js/fpp.js?ref=<?php echo filemtime('js/fpp.js'); ?>"></script>
<?php
include 'common/menuHead.inc';

$data = file_get_contents('http://127.0.0.1:32322/gpio');
$gpiojson = json_decode($data, true);
?>
<script language="Javascript">
allowMultisyncCommands = true;

var gpioPinNames = {
<?
$count = 0;
foreach($gpiojson as $gpio) {
    $pinName = $gpio['pin'];
    $gpioNum = $gpio['gpio'];
    $pinNameClean = str_replace("-", "_", $pinName);
    if ($count != 0) {
        echo ",\n";
    }
    $count = $count + 1;
    echo "    \"" . $pinNameClean . "\": \"" . $pinName . "\"";
}
echo "\n";
?>
};
    
function SaveGPIOInputs() {
    var gpios = Array();
    $.each( gpioPinNames, function( key, val ) {
           var gp = {};
           gp["pin"] = val;
           gp["enabled"] = $("#gpio_" + key + "_enabled").is(':checked');
           gp["mode"] = $("#gpio_" + key + "_PullUpDown").val();
           
           var rc = $('#gpio_' + key + '_RisingCommand').val();
           if (rc != "") {
                gp["rising"] = {};
                CommandToJSON("gpio_" + key + "_RisingCommand", "tableRisingGPIO" + key, gp["rising"]);
           }
           var fc = $('#gpio_' + key + '_FallingCommand').val();
           if ( fc != "") {
                gp["falling"] = {};
                CommandToJSON('gpio_' + key + '_FallingCommand', 'tableFallingGPIO' + key , gp["falling"]);
           }
           
           if (rc === undefined) {
            rc = "";
           }
           if (fc === undefined) {
            fc = "";
           }
           if (gp["enabled"] || rc != "" || fc != "") {
                gpios.push(gp);
           }
        });
    
    var postData = JSON.stringify(gpios, null, 4);
    $.ajax({
            url: "api/configfile/gpio.json",
            type: 'POST',
            contentType: 'application/json',
            data: postData,
            dataType: 'json',
            success: function(data) {
                    $('html,body').css('cursor','auto');
                    if (data.Status == 'OK') {
                        $.jGrowl("GPIO Inputs Saved.");
                        SetRestartFlag(2);
                    } else {
                        alert('ERROR: ' + data.Message);
                    }
            },
            error: function() {
                    $('html,body').css('cursor','auto');
                    alert('Error, Failed to save GPIO inputs');
            }
    });
    
}

/////////////////////////////////////////////////////////////////////////////
$(document).ready(function(){
});

extraCommands = [
{
    "name": "OLED Navigation",
    "args": [
        {
            "description": "Action",
            "name": "Action",
            "optional": false,
            "type": "string",
            "contents": [
                "Up", "Down", "Back", "Enter", "Test/Down"
            ]
        }
    ]
}
];

</script>


<title><? echo $pageTitle; ?></title>

</head>
<body>
	<div id="bodyWrapper">
		<?php include 'menu.inc'; ?>
		<br/>

<div id='channelOutputManager'>
				<div id='divGPIO'>
					<fieldset class="fs">
						<legend> GPIO Input Triggers </legend>
						<div id='divGPIOData'>

<!-- --------------------------------------------------------------------- -->

  <div style="overflow: hidden; padding: 10px;">
    <div>
        <input type="button" value="Save" class="buttons" onClick="SaveGPIOInputs();"></input>
    </div>
    <div class='fppTableWrapper'>
    <div class='fppTableContents fullWidth'>
    <table id='GPIOInputs' width="100%">
        <thead>
            <tr>
                <th rowspan='2'>En.</th>
                <th rowspan='2'>Hdr -<br>Pin</th>
                <th rowspan='2'>GPIO # -<br>GPIOD</th>
                <th rowspan='2' id='pullHeader' >Pull<br>Up/Down</th>
                <th colspan='2'>Commands</th>
            </tr>
            <tr>
                <th >Rising Edge</th>
                <th >Falling Edge</th>
            </tr>
        </thead>
        <tbody>
<?
$count = 0;
$pCount = 0;
foreach($gpiojson as $gpio) {
    $pinName = $gpio['pin'];
    $gpioNum = $gpio['gpio'];
    $pinNameClean = str_replace("-", "_", $pinName);
    $style = " evenRow";
    if ($count % 2 == 0) {
        $style = " oddRow";
    }
    $count = $count + 1;

    if ($gpio['supportsPullUp'] || $gpio['supportsPullDown'])
        $pCount++;
?>
    <tr class='fppTableRow <?= $style ?>' id='row_<?=$pinNameClean?>'>
        <td><input type="checkbox" id="gpio_<?= $pinNameClean ?>_enabled"></td>
        <td><?= $pinName ?></td>
    <td><?= $gpioNum ?>&nbsp;-&nbsp;<?= $gpio['gpioChip'] ?>/<?= $gpio['gpioLine'] ?></td>
<?
if ($gpio['supportsPullUp'] || $gpio['supportsPullDown']) {
?>
            <td>
            <select id='gpio_<?= $pinNameClean ?>_PullUpDown' <? if (!$gpio['supportsPullUp'] && !$gpio['supportsPullDown']) echo "style='display:none;'"; ?> >
            <option value='gpio'>None/External</option>
            <? if ($gpio['supportsPullUp']) echo "<option value='gpio_pu'>Pull Up</option>\n"; ?>
            <? if ($gpio['supportsPullDown']) echo "<option value='gpio_pd'>Pull Down</option>\n"; ?>
        </select>
    </td>
<?
} else if ($pCount > 0) {
    echo "<td></td>";
}
?>
        <td>
            <table border=0 class='fppTable' id='tableRisingGPIO<?= $pinNameClean ?>'>
            <tr>
<td>Command:</td><td><select id='gpio_<?= $pinNameClean ?>_RisingCommand' onChange='CommandSelectChanged("gpio_<?= $pinNameClean ?>_RisingCommand", "tableRisingGPIO<?= $pinNameClean ?>", false);'><option value=""></option></select></td>
            </tr>
            </table>
        </td>
        <td>
                <table border=0 class='fppTable' id='tableFallingGPIO<?= $pinNameClean ?>'>
                <tr>
<td>Command:</td><td><select id='gpio_<?= $pinNameClean ?>_FallingCommand' onChange='CommandSelectChanged("gpio_<?= $pinNameClean ?>_FallingCommand", "tableFallingGPIO<?= $pinNameClean ?>", false);'><option value=""></option></select></td>
                </tr>
                </table>
        </td>
        <script>
        LoadCommandList($('#gpio_<?= $pinNameClean ?>_RisingCommand'));
        LoadCommandList($('#gpio_<?= $pinNameClean ?>_FallingCommand'));
        </script>
    </tr>
<?
}
?>
</tbody>
</table>
</div>
</div>
<script>
<?
if ($pCount == 0) {
    echo "$('#pullHeader').hide();";
}

if (file_exists('/home/fpp/media/config/gpio.json')) {
    $data = file_get_contents('/home/fpp/media/config/gpio.json');
    $gpioInputJson = json_decode($data, true);
    echo "var gpioConfig = " . json_encode($gpioInputJson, JSON_PRETTY_PRINT) . ";\n";

    $x = 0;
    foreach($gpioInputJson as $gpio) {
        $pinName = $gpio['pin'];
        $pinNameClean = str_replace("-", "_", $pinName);
        
        
        if ($gpio['enabled'] == true) {
            echo "$('#gpio_" . $pinNameClean . "_enabled').prop('checked', true);\n";
        }

        if (isset($gpio["mode"]))
            echo "$('#gpio_" . $pinNameClean . "_PullUpDown').val(\"" . $gpio["mode"] . "\");\n";

        echo "PopulateExistingCommand(gpioConfig[" . $x . "][\"falling\"], 'gpio_" . $pinNameClean . "_FallingCommand', 'tableFallingGPIO" . $pinNameClean . "', false);\n";
        echo "PopulateExistingCommand(gpioConfig[" . $x . "][\"rising\"], 'gpio_" . $pinNameClean . "_RisingCommand', 'tableRisingGPIO" . $pinNameClean . "', false);\n";
        $x = $x + 1;
    }
}
?>
</script>

	</div>

<!-- --------------------------------------------------------------------- -->

						</div>
					</fieldset>
				</div>
			</div>


	<?php	include 'common/footer.inc'; ?>
</div>
</body>
</html>
