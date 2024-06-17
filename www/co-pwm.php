<?

function outputTypeSelect($port, $idx)
{
    $type = "any";
    if (isset($port["type"])) {
        $type = $port['type'];
    }
    $ledSelected = $type == "LED" ? " selected" : "";
    $servoSelected = $type == "Servo" ? " selected" : "";
    $selectVis = "";
    if ($type == "any") {
        $servoSelected = " selected";
    } else {
        $selectVis = " hidden";
        echo "<b>" . $type . "</b>";
    }
    ?>
    <select class="vsPortType" onChange="pwmPortTypeChanged(<?= $idx ?>);" <?= $selectVis ?> id="PWM-PORT-TYPE-<?= $idx ?>">
        <option value="Servo" <?= $servoSelected ?>>Servo</option>
        <option value="LED" <?= $ledSelected ?>>LED</option>
    </select>
    <?
}
function outputPortRow($port, $idx, $channel)
{
    $supports16Bit = true;
    $supports16BitStyle = "checked='checked'";
    $description = "";
    $label = "Port " . $idx;
    if (isset($port["supports16bit"])) {
        $supports16BitStyle = $port["supports16bit"] ? "checked='checked'" : " hidden";
    }
    if (isset($port["description"])) {
        $description = $port["description"];
    }
    if (isset($port["label"])) {
        $label = $port["label"];
    }
    ?>
    <tr id='PWM-<?= $idx ?>'>
        <td nowrap><b><?= $label ?></b></td>
        <td><input type="text" class="vsDescription" size="25" maxlength="60" value="<?= $description ?>"
                id="pwmDescription<?= $idx ?>" />
        </td>
        <td><input type="number" class="vsStartChannel" data-bs-html="true" size="7" value="<?= $channel ?>" min="1"
                max="8388608" onkeypress="preventNonNumericalInput(event)" onchange="pwmStartChannelChanged(this);"
                onpaste="this.onchange();" oninput="this.onchange();" id="pwmStartChannel<?= $idx ?>"></td>
        <td><input type="checkbox" id="pwm16Bit<?= $idx ?>" <?= $supports16BitStyle ?>></input></td>
        <td><? outputTypeSelect($port, $idx); ?></td>
        <td width='80%' id='PWM-SETTINGS-<?= $idx ?>'></td>
    </tr>
    <?
    $channel += $supports16Bit ? 2 : 1;
    return $channel;
}

$pwmCape = array();
if (is_dir($mediaDirectory . "/tmp/pwm/")) {
    $files = scandir($mediaDirectory . "/tmp/pwm/");
    foreach ($files as $file) {
        $string = "";
        if (substr($file, 0, 1) == '.') {
            $string = "";
        } else {
            $string = file_get_contents($mediaDirectory . "/tmp/pwm/" . $file);
        }
        if ($string != "") {
            $pwmCape = json_decode($string, true);
            break;
        }
    }
}
?>

<style>
    #PWM thead th {
        font-weight: bold;
        background: #fff;
        position: sticky;
        top: 0;
        z-index: 20;
        padding: 8px 0 0 5px;
        border-bottom: 2px solid #d5d7da;
    }
</style>


<script type="text/javascript">
    var PWM_CAPE = <?= json_encode($pwmCape, JSON_PRETTY_PRINT); ?>;

    function servoMinMaxChanged(idx) {
        var min = $("#pwmMin" + idx).val();
        var max = $("#pwmMax" + idx).val();
        $("#pwmServoSlider-" + idx).slider({
            values: [min, max]
        });
    }

    function pwmPortTypeChanged(idx) {
        var type = $("#PWM-PORT-TYPE-" + idx).val();
        var html = type;
        if (type == 'LED') {
            html = "Brightness: ";
            html += "<select class='vsBrightness' id='pwmBrightness" + idx + "'>";
            var i = 100;
            for (i = 100; i >= 5; i -= 5) {
                html += "<option value='" + i + "'";
                html += ">" + i + "%</option>";
            }
            html += "</select>";
            html += "    Gamma: <input type='number' class='vsGamma'  id='pwmGamma" + idx + "' size='3' value='1.0' min='0.1' max='5.0' step='0.01'>";
            $("#PWM-SETTINGS-" + idx).html(html);
        } else if (type == "Servo") {
            html = "<div style=' white-space: nowrap; display:flex; overflow:auto; align-items:center;'>Range:&nbsp;";
            html += "<input type='number' min='500' max='2500' id='pwmMin" + idx + "' value='1000' onChange='servoMinMaxChanged(" + idx + ");'/>&nbsp;"
            html += "<div id='pwmServoSlider-" + idx + "' class='fppMinMaxSliderRange servo-range-slider' style='max-width: 350px;'></div>";
            html += "&nbsp;<input type='number' min='500' max='2500' id='pwmMax" + idx + "' value='2000' onChange='servoMinMaxChanged(" + idx + ");'/>";
            html += "&nbsp;Reverse:&nbsp;<input type='checkbox' id='pwmReverse" + idx + "'></div>";
            html += "<span style=' white-space: nowrap; display:inline; overflow:auto; align-items:center;'>";
            html += "Zero Behavior:&nbsp;<select id='pwmZero" + idx + "'>";
            html += "<option value='Hold' selected>Hold</optoin>";
            html += "<option value='Min'>Min</optoin>";
            html += "<option value='Max'>Max</optoin>";
            html += "<option value='Center'>Center</optoin>";
            html += "<option value='Stop PWM'>Stop PWM</optoin>";
            html += "</select>";
            html += "</span>";
            html += "<span style=' white-space: nowrap; display:inline; overflow:auto; align-items:center;'>";
            html += " &nbsp;Data Type:&nbsp;<select id='pwmDataType" + idx + "'>";
            html += "<option value='Scaled' selected>Scaled</optoin>";
            html += "<option value='Absolute'>Absolute</optoin>";
            html += "<option value='1/2 Absolute'>1/2 Absolute</optoin>";
            html += "<option value='2x Absolute'>2x Absolute</optoin>";
            html += "</select>";
            html += "</span>";

            $("#PWM-SETTINGS-" + idx).html(html);
            $("#pwmServoSlider-" + idx).slider({
                range: true,
                min: 500,
                max: 2500,
                values: [1000, 2000],
                slide: function (event, ui) {
                    if (ui.values[0] == ui.values[1]) {
                        return false;
                    }
                    $("#pwmMax" + idx).val(ui.values[1]);
                    $("#pwmMin" + idx).val(ui.values[0]);
                }
            });
        } else {
            $("#PWM-SETTINGS-" + idx).html(html);
        }
    }
    function pwmStartChannelChanged(field) {

    }
    function populatePWMOutputs(data) {
        $("#PWM_enable").prop('checked', data.enabled);
        var idx = 1;
        $("#PWMFrequency").val(data.frequency);

        $.each(data.outputs, function (index, output) {
            $("#pwmDescription" + idx).val(output.description);
            $('#pwm16Bit' + idx).prop('checked', output.is16bit);
            $("#pwmStartChannel" + idx).val(output.startChannel + 1);
            $("#PWM-PORT-TYPE-" + idx).val(output.type);
            pwmPortTypeChanged(idx);
            if (output.type == "LED") {
                $("#pwmBrightness" + idx).val(output.brightness);
                $("#pwmGamma" + idx).val(output.gamma);
            } else {
                $("#pwmMin" + idx).val(output.min);
                $("#pwmMax" + idx).val(output.max);
                servoMinMaxChanged(idx);
                $('#pwmReverse' + idx).prop('checked', output.reverse);
                $("#pwmZero" + idx).val(output.zero);
                $("#pwmDataType" + idx).val(output.dataType);
            }
            idx = idx + 1;
        });
    }
    function savePWMOutputs() {

        var postData = {};
        postData.channelOutputs = [];
        var channelOutputs = {};
        channelOutputs['type'] = PWM_CAPE['driver'];
        channelOutputs['subType'] = PWM_CAPE['name'];
        channelOutputs['enabled'] = $("#PWM_enable").is(':checked') ? 1 : 0;
        channelOutputs['frequency'] = $("#PWMFrequency").val();
        channelOutputs["startChannel"] = 0;
        channelOutputs["channelCount"] = -1;
        channelOutputs['outputs'] = [];
        var max = Object.keys(PWM_CAPE['outputs']).length;
        for (var x = 0; x < max; x++) {
            var idx = x + 1;
            var output = {};
            output.description = $("#pwmDescription" + idx).val();
            output.startChannel = parseInt($("#pwmStartChannel" + idx).val()) - 1;
            output.is16bit = $('#pwm16Bit' + idx).is(':checked') ? 1 : 0;
            output.type = $("#PWM-PORT-TYPE-" + idx).val();
            if (output.type == "LED") {
                output.brightness = parseInt($("#pwmBrightness" + idx).val());
                output.gamma = parseFloat($("#pwmGamma" + idx).val());
            } else if (output.type == "Servo") {
                output.min = parseInt($("#pwmMin" + idx).val());
                output.max = parseInt($("#pwmMax" + idx).val());
                output.reverse = $("#pwmReverse" + idx).is(':checked') ? 1 : 0;
                output.zero = $("#pwmZero" + idx).val();
                output.dataType = $("#pwmDataType" + idx).val();
            }
            channelOutputs['outputs'].push(output);
        }

        postData.channelOutputs.push(channelOutputs);
        $.post("api/channel/output/co-pwm", JSON.stringify(postData)).done(function (data) {
            $.jGrowl("PWM/Servo Output Configuration Saved", { themeState: 'success' });
            SetRestartFlag(1);
        }).fail(function () {
            DialogError("Save PWM/Servo Outputs", "Save Failed");
        });
    }

    function loadPWMOutputs() {
        $.getJSON("api/channel/output/co-pwm", function (data) {
            populatePWMOutputs(data.channelOutputs[0])
        });
    }


    $(document).ready(function () {
        <?
        $idx = 1;
        foreach ($pwmCape['outputs'] as $port) {
            echo "pwmPortTypeChanged(" . $idx . ");";
            $idx += 1;
        }
        ?>
        loadPWMOutputs();
    });
</script>

<div id='tab-PWM'>
    <div id='divPWM'>
        <div class="row tableTabPageHeader capeTypeRow">
            <div class="col-md">
                <h2><?= $pwmCape["longName"] ?></h2>
            </div>
            <div class="col-md-auto ml-lg-auto">
                <div class="form-actions">
                    <input type='button' class='buttons' onClick='loadPWMOutputs();' value='Revert' />
                    <input type='button' class='buttons btn-success ml-1' onClick='savePWMOutputs();' value='Save' />
                </div>
            </div>
        </div>


        <div class="backdrop tableOptionsForm capeTypeRow">
            <div class="row">
                <div class="col-md-auto">
                    <div class="backdrop-dark form-inline enableCheckboxWrapper">
                        <div><b>Enable <span class='capeNamePWM'><?= $pwmCape["longName"] ?></span>:</b></div>
                        <div><input id='PWM_enable' type='checkbox'></div>
                    </div>
                </div>
                <div class="col-md-auto form-inline">
                    <span style=' white-space: nowrap; display:inline; overflow:auto; align-items:center;'>&nbsp;<b>Frequency:</b>&nbsp;<select id='PWMFrequency'>
                        <option value='50hz'>50hz</optoin>
                        <option value='80hz'>80hz</optoin>
                        <option value='100hz'>100hz</optoin>
                        <option value='120hz'>120hz</optoin>
                        <option value='150hz'>150hz</optoin>
                        <option value='200hz'>200hz</optoin>
                        <option value='240hz'>240hz</optoin>
                        <option value='250hz'>250hz</optoin>
                        </select>
                    </span>
                </div>
            </div>
        </div>
        <div id='pixelOutputs' class="fppFThScrollContainer">
            <div class='fppTableWrapper'>
                <div class='fppTableContents' role='region' aria-labelledby='PWM' tabindex='0'>
                    <table id='PWM' class='fppSelectableRowTable fppStickyTheadTable'>
                        <thead>
                            <tr>
                                <th>Port</th>
                                <th>Descrption</th>
                                <th>Channel</th>
                                <th>16bit</th>
                                <th style="text-align: center;">Type</th>
                                <th>Settings</th>
                            </tr>
                        </thead>
                        <tbody>
                            <?
                            $idx = 1;
                            $channel = 1;
                            foreach ($pwmCape['outputs'] as $port) {
                                $channel = outputPortRow($port, $idx, $channel);
                                $idx += 1;
                            }
                            ?>
                        </tbody>
                    </table>
                </div>
            </div>
        </div>
    </div>
</div>