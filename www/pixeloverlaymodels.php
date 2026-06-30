<!DOCTYPE html>
<html lang="en">

<head>
    <?php
    include 'common/htmlMeta.inc';
    require_once "common.php";
    include 'common/menuHead.inc'; ?>

    <script language="Javascript">
        var FBDevices = new Array();
        var FBInfo = {};
<?
$showAddFBButton = 0;

exec($SUDO . " " . $settings["fppBinDir"] . "/fpp -FB", $output, $return_val);
$js = json_decode($output[0]);
foreach ($js as $fileName) {
    echo "FBDevices['$fileName'] = '$fileName';\n";
    echo "FBInfo['$fileName'] = {};\n";
    $showAddFBButton = 1;

    if ($settings["Platform"] == "MacOS" || !file_exists("/dev/$fileName")) {
        echo "FBInfo['$fileName'].Width = 150;\n";
        echo "FBInfo['$fileName'].Height = 100;\n";
    } else {
        $geometry = exec("fbset -i -fb /dev/$fileName | grep geometry", $output, $return_val);
        if ($return_val == 0) {
            $parts = preg_split("/\s+/", preg_replace('/^ *geometry */', '', $geometry));
            if (count($parts) > 3) {
                echo "FBInfo['$fileName'].Width = " . $parts[0] . ";\n";
                echo "FBInfo['$fileName'].Height = " . $parts[1] . ";\n";
            }
        }
    }
}

if (($settings['Platform'] == "Linux") && (file_exists('/usr/include/X11/Xlib.h'))) {
    ?>
            FBDevices['x11'] = 'x11';
            FBInfo['x11'] = {};
            FBInfo['x11'].Width = 640;
            FBInfo['x11'].Height = 480;
            <?
}

?>

        var PixelOverlayModels = new Array();
        <?
        $json = file_get_contents('http://localhost/api/models?simple=true');
        $models = json_decode($json, true);
        foreach ($models as $model) {
            echo "PixelOverlayModels['$model'] = '$model';";
        }

        ?>

        var modelPreviewData = {};
        var modelPreviewDataNormalized = {};
        <?
        $mapFile = $settings['configDirectory'] . '/virtualdisplaymap';
        if (file_exists($mapFile)) {
            $f = fopen($mapFile, 'r');
            if ($f) {
                $currentModel = null;
                $previewPixels = [];
                while (!feof($f)) {
                    $line = fgets($f);
                    $line = trim($line);
                    if ($line == '')
                        continue;
                    if (preg_match("/^#\s*Model:\s*'([^']+)'/", $line, $matches)) {
                        if ($currentModel !== null) {
                            echo "modelPreviewData[" . json_encode($currentModel) . "] = " . json_encode($previewPixels) . ";\n";
                        }
                        $currentModel = $matches[1];
                        $previewPixels = [];
                        continue;
                    }
                    if (preg_match('/^#/', $line))
                        continue;
                    $parts = explode(',', $line);
                    if (count($parts) >= 3 && $currentModel !== null) {
                        $previewPixels[] = [(int) $parts[0], (int) $parts[1], isset($parts[2]) ? (int) $parts[2] : 0];
                    }
                }
                if ($currentModel !== null) {
                    echo "modelPreviewData[" . json_encode($currentModel) . "] = " . json_encode($previewPixels) . ";\n";
                }
                fclose($f);
            }
        }
        ?>

        var xlightsSubModels = {};
        <?
        // Lightweight per-parent submodel list (names/dims only, no grids) from
        // the digested xlights-submodels.json.  Full grid data is fetched on
        // demand via the API so the page stays light.  See
        // docs/PixelOverlaySubModels.md.
        $subFile = $settings['configDirectory'] . '/xlights-submodels.json';
        if (file_exists($subFile)) {
            $subData = json_decode(file_get_contents($subFile), true);
            if (isset($subData['submodels']) && is_array($subData['submodels'])) {
                $byParent = [];
                foreach ($subData['submodels'] as $sm) {
                    if (!isset($sm['Parent'])) {
                        continue;
                    }
                    $byParent[$sm['Parent']][] = [
                        'Name' => $sm['Name'],
                        'DisplayName' => isset($sm['DisplayName']) ? $sm['DisplayName'] : $sm['Name'],
                        'Width' => isset($sm['Width']) ? (int) $sm['Width'] : 0,
                        'Height' => isset($sm['Height']) ? (int) $sm['Height'] : 0,
                    ];
                }
                foreach ($byParent as $parent => $list) {
                    echo "xlightsSubModels[" . json_encode($parent) . "] = " . json_encode($list) . ";\n";
                }
            }
        }
        ?>

        function normalizeModelName(name) {
            return String(name || '').toLowerCase().replace(/[^a-z0-9]/g, '');
        }

        function getSubModels(modelName) {
            if (xlightsSubModels.hasOwnProperty(modelName)) {
                return xlightsSubModels[modelName];
            }
            return null;
        }

        var xlightsModelGroups = [];
        <?
        // Lightweight model-group list from the digested xlights-modelgroups.json
        // (no grids inline). See docs/PixelOverlaySubModels.md.
        $grpFile = $settings['configDirectory'] . '/xlights-modelgroups.json';
        if (file_exists($grpFile)) {
            $grpData = json_decode(file_get_contents($grpFile), true);
            if (isset($grpData['modelgroups']) && is_array($grpData['modelgroups'])) {
                $glist = [];
                foreach ($grpData['modelgroups'] as $g) {
                    $glist[] = [
                        'Name' => $g['Name'],
                        'DisplayName' => isset($g['DisplayName']) ? $g['DisplayName'] : $g['Name'],
                        'MemberCount' => isset($g['MemberCount']) ? (int) $g['MemberCount'] : 0,
                        'PixelCount' => isset($g['PixelCount']) ? (int) $g['PixelCount'] : 0,
                        'Width' => isset($g['Width']) ? (int) $g['Width'] : 0,
                        'Height' => isset($g['Height']) ? (int) $g['Height'] : 0,
                    ];
                }
                echo "xlightsModelGroups = " . json_encode($glist) . ";\n";
            }
        }
        ?>

        function PopulateModelGroups() {
            if (!xlightsModelGroups || xlightsModelGroups.length == 0) {
                return;
            }
            var $tbody = $('#modelGroupsTable tbody');
            var html = "";
            for (var i = 0; i < xlightsModelGroups.length; i++) {
                var g = xlightsModelGroups[i];
                html += "<tr>"
                    + "<td style='text-align:left;'>" + g.DisplayName + "</td>"
                    + "<td style='text-align:center;'>" + g.MemberCount + "</td>"
                    + "<td style='text-align:center;'>" + g.PixelCount + "</td>"
                    + "<td style='text-align:center;'>" + g.Width + " x " + g.Height + "</td>"
                    + "<td><code style='font-size:0.85em;'>" + g.Name + "</code></td>"
                    + "</tr>";
            }
            $tbody.html(html);
            $('#modelGroupsCount').text("(" + xlightsModelGroups.length + ")");
            $('#modelGroupsSection').show();
        }

        function getModelPreviewPixels(modelName) {
            if (modelPreviewData.hasOwnProperty(modelName)) {
                return modelPreviewData[modelName];
            }
            return modelPreviewDataNormalized[normalizeModelName(modelName)] || null;
        }

        Object.keys(modelPreviewData).forEach(function (key) {
            modelPreviewDataNormalized[normalizeModelName(key)] = modelPreviewData[key];
        });

        function GetOrientationInput(currentValue, attr) {

            var options = {
                horizontal: "Horizontal",
                vertical: "Vertical"
            };
            var str = "";
            const is_custom = (currentValue || "").toLowerCase().startsWith("custom");
            if (is_custom) {
                str = "<b>" + currentValue + "</b>"; //allow descriptive text following
            }
            str += "<select class='form-select orientation'";
            str += attr;
            if (is_custom) {
                str += " style='visibility: hidden;'><option value='custom' selected>Custom</option";
            }
            str += ">";
            for (var key in options) {
                str += "<option value='" + key + "'";
                if (currentValue == key)
                    str += "selected";

                str += ">" + options[key] + "</option>";
            }

            str += "</select>";

            return str;
        }

        function GetStartingCornerInput(currentValue, attr) {
            var options = {
                TL: "Top Left",
                TR: "Top Right",
                BL: "Bottom Left",
                BR: "Bottom Right"
            };
            var str = "<select class='form-select corner'" + attr + ">";

            for (var key in options) {
                str += "<option value='" + key + "'";
                if (currentValue == key)
                    str += "selected";

                str += ">" + options[key] + "</option>";
            }

            str += "</select>";

            return str;
        }
        function PopulatePixelOverlaySettings(data) {
            if (data != null) {
                $("#AutoCreatePixelOverlays").prop('checked', data["autoCreate"]);
            }
        }

        function DeviceChanged(item) {
            var type = $(item).parent().parent().find('span.type').html();

            if (type == 'FB') {
                var device = $(item).val();

                if (FBInfo.hasOwnProperty(device)) {
                    $(item).parent().parent().find('input.width').val(FBInfo[device].Width);
                    $(item).parent().parent().find('input.height').val(FBInfo[device].Height);
                }

                if (device == 'x11')
                    $(item).parent().parent().find('.X11input').show();
                else
                    $(item).parent().parent().find('.X11input').hide();
            }
        }

        function WidthOrHeightModified(item) {
            var width = parseInt($(item).parent().parent().find('input.width').val());
            var height = parseInt($(item).parent().parent().find('input.height').val());
            var channels = width * height * 3;

            if ($(item).parent().parent().find('input.pixelSize').length) {
                var pixelSize = parseInt($(item).parent().parent().find('input.pixelSize').val());
                if (!pixelSize || pixelSize < 1) {
                    pixelSize = 1;
                }

                var pWidth = parseInt(width / pixelSize);
                var pHeight = parseInt(height / pixelSize);
                channels = pWidth * pHeight * 3;
            }

            $(item).parent().parent().find('td.channels').html(channels);
        }

        function PopulateChannelMemMapTable(data) {
            $('#channelMemMaps tbody').html("");
            $('#channelMemMapsAutoCreate tbody').html("");
            if (data == null) {
                return;
            }

            for (var i = 0; i < data.length; i++) {
                var model = data[i];
                var ChannelCountPerNode = model.ChannelCountPerNode;
                if (ChannelCountPerNode == undefined) {
                    ChannelCountPerNode = 3;
                }
                postr = "<tr id='row'" + i + " class='fppTableRow'>" +
                    "<td class='center' valign='middle'>";

                var attr = " ";
                if (model.autoCreated) {
                    postr += "";
                    attr = " disabled";
                } else {
                    postr += "<div class='rowGrip'><i class='rowGripIcon fpp-icon-grip'></i></div>";
                }

                postr += "</td><td><input class='blk' type='text' size='31' maxlength='31' value='" + model.Name + "'" + attr + "></td>";

                switch (model.Type) {
                    case "Channel":
                        postr += "<td><span class='hidden type'>" + model.Type + "</span>" + model.Type + "</td>" +
                            "<td><input class='start' type='text' size='6' maxlength='6' value='" + model.StartChannel + "'" + attr + "></td>" +
                            "<td><input class='cnt' type='text' size='6' maxlength='6' value='" + model.ChannelCount + "'" + attr + "></td>" +
                            "<td><input class='cpn' type='number' min='1' max='4' value='" + ChannelCountPerNode + "'" + attr + "></td>" +
                            "<td style=\"white-space: nowrap;\">" + GetOrientationInput(model.Orientation + orientationDetails(model), attr) + "</td>";
                        if (model.Orientation != "custom") {
                            postr += "<td>" + GetStartingCornerInput(model.StartCorner, attr) + "</td>" +
                                "<td><input class='strcnt' type='text' size='3' maxlength='3' value='" + model.StringCount + "'" + attr + "></td>" +
                                "<td><input class='strands' type='text' size='2' maxlength='2' value='" + model.StrandsPerString + "'" + attr + "></td>";
                        } else {
                            postr += "<td><input class='corner' type='hidden' value='" + model.StartCorner + "'><input class='data' type='hidden' value='" + model.data + "'></td>" +
                                "<td><input class='strcnt' type='hidden' value='" + model.StringCount + "'></td>" +
                                "<td><input class='strands' type='hidden' value='" + model.StrandsPerString + "'></td>";
                        }
                        var xlchecked = "";
                        if (model.xLights) {
                            xlchecked = " checked";
                        }
                        postr += "<td><input class='xlights' type='checkbox'" + xlchecked + " disabled>";
                        var previewPixels = getModelPreviewPixels(model.Name);
                        if (model.xLights && previewPixels && previewPixels.length > 0) {
                            postr += " <i class='fas fa-eye' style='cursor:pointer; color:#5bc0de;' title='Preview model layout' onclick='showModelPreview(" + JSON.stringify(model.Name) + ")'></i>";
                        }
                        var subModels = getSubModels(model.Name);
                        if (subModels && subModels.length > 0) {
                            postr += " <i class='fas fa-sitemap' style='cursor:pointer; color:#9b59b6;' title='" + subModels.length + " submodel(s)' onclick='toggleSubModels(this," + JSON.stringify(model.Name) + ")'></i>"
                                + " <span style='color:#9b59b6;font-size:0.85em;'>" + subModels.length + "</span>";
                        }
                        postr += "</td><td>"
                        break;
                    case "FB":
                        var pixelSize = parseInt(model.PixelSize);
                        if (!pixelSize || pixelSize < 1) {
                            // Auto-created FB models (e.g. Virtual Matrix) don't
                            // specify a PixelSize, which defaults to 1.
                            pixelSize = 1;
                        }
                        var pWidth = parseInt(model.Width / pixelSize);
                        var pHeight = parseInt(model.Height / pixelSize);
                        var channels = pWidth * pHeight * 3;
                        var select = CreateSelect(FBDevices, model.Device, '', '-- Port --', 'device', 'DeviceChanged(this);');

                        if (model.autoCreated) {
                            select = select.replace('<select ', '<select disabled ');
                        }

                        postr += "<td><span class='hidden type'>FB</span>FrameBuffer</td>" +
                            "<td>" + select + "</td>" +
                            "<td class='channels'>" + channels + "</td>" +
                            "<td colspan='3'>Size: <input class='width' type='number' min='8' max='4096' step='8' value='" + model.Width + "'" + attr + " onChange='WidthOrHeightModified(this);'> <b>X</b>" +
                            "<input class='height' type='number' min='8' max='2160' step='8' value='" + model.Height + "'" + attr + " onChange='WidthOrHeightModified(this);'>";

                        if (model.Device == 'x11') {
                            postr += "<span class='X11input'>&nbsp;@ <input class='xOffset X11input' type='number' min='8' max='4096' step='8' value='" + model.X + "'" + attr + ">"
                                + ",<input class='yOffset X11input' type='number' min='8' max='2160' step='8' value='" + model.Y + "'" + attr + "></span>";
                        }

                        postr += "<td colspan='2'>Pixel Size: <input class='pixelSize' type='number' min='1' max='64' value='" + pixelSize + "'" + attr + " onChange='WidthOrHeightModified(this);'></td>";
                        postr += "</td>" +
                            "<td></td>" +
                            "<td>";
                        break;
                    case "Sub":
                        var select = CreateSelect(PixelOverlayModels, model.Parent, '', '-- Parent --', 'parent', '');
                        if (model.autoCreated) {
                            select = select.replace('<select ', '<select disabled ');
                        }

                        postr += "<td><span class='hidden type'>Sub</span>Sub-Model</td>" +
                            "<td>" + select + "</td>" +
                            "<td class='channels'>" + (model.Width * model.Height * 3) + "</td><td>3</td>" +
                            "<td>xOffset: <input class='xOffset' type='number' min='0' max='4096' value='" + model.XOffset + "'" + attr + "></td>" +
                            "<td>yOffset: <input class='yOffset' type='number' min='0' max='2160' value='" + model.YOffset + "'" + attr + "></td>" +
                            "<td colspan='2'><input class='width' type='number' min='8' max='4096' value='" + model.Width + "'" + attr + " onChange='WidthOrHeightModified(this);'> <b>X</b>" +
                            "<input class='height' type='number' min='8' max='2160' value='" + model.Height + "'" + attr + " onChange='WidthOrHeightModified(this);'></td>" +
                            "<td></td>" +
                            "<td>";
                        break;
                }

                if (model.effectRunning) {
                    postr += model.effectName;
                }
                if (model.autoCreated) {
                    $('#channelMemMapsAutoCreate tbody').append(postr + "</td></tr>");
                } else {
                    $('#channelMemMaps tbody').append(postr + "</td></tr>");
                }
            }
        }

        // Toggle the collapsible detail row listing a model's xLights submodels.
        // The detail row (which can hold hundreds of submodels) is built lazily
        // on first expand so the main table stays fast to render. Submodels are
        // first-class overlay models: they can be targeted by name through the
        // standard overlay commands (e.g. "Overlay Model Effect").
        function toggleSubModels(iconEl, modelName) {
            var rowId = "subrow_" + normalizeModelName(modelName);
            var $existing = $('#' + rowId);
            if ($existing.length) {
                $existing.toggle();
                return;
            }
            var subModels = getSubModels(modelName);
            if (!subModels || subModels.length == 0) {
                return;
            }
            var html = "<tr id='" + rowId + "' class='submodelDetail'>"
                + "<td colspan='12' style='background:#1c1c1c;padding:8px 16px;'>"
                + "<b style='color:#9b59b6;'>Submodels of " + modelName + "</b>"
                + "<table style='width:100%;margin-top:6px;' class='submodelList'>"
                + "<thead><tr><th style='text-align:left;'>Name</th><th>Size</th><th>Overlay Model Name</th><th>Preview</th></tr></thead><tbody>";
            for (var i = 0; i < subModels.length; i++) {
                var sm = subModels[i];
                html += "<tr>"
                    + "<td>" + sm.DisplayName + "</td>"
                    + "<td style='text-align:center;'>" + sm.Width + " x " + sm.Height + "</td>"
                    + "<td><code style='font-size:0.85em;'>" + sm.Name + "</code></td>"
                    + "<td style='text-align:center;'><i class='fas fa-eye' style='cursor:pointer;color:#5bc0de;' title='Preview submodel' onclick='showSubModelPreview(" + JSON.stringify(modelName) + "," + JSON.stringify(sm.Name) + ")'></i></td>"
                    + "</tr>";
            }
            html += "</tbody></table></td></tr>";
            $(iconEl).closest('tr').after(html);
        }

        // Preview a submodel by highlighting its nodes over the parent's preview
        // pixels.  The submodel's grid (parent node numbers) is fetched on demand
        // from the API so the page stays light; the parent's per-pixel coords
        // come from the already-loaded virtualdisplaymap.
        function showSubModelPreview(parentName, subModelName) {
            var parentPixels = getModelPreviewPixels(parentName);
            if (!parentPixels || parentPixels.length == 0) {
                alert('No preview data available for parent model: ' + parentName);
                return;
            }
            $.get("api/overlays/model/" + encodeURIComponent(subModelName), function (sm) {
                if (!sm || !sm.Grid) {
                    alert('No grid data returned for submodel "' + subModelName + '".\n\n'
                        + 'This usually means fppd has not been restarted since submodel '
                        + 'support was added. Restart fppd and try again.');
                    return;
                }
                var nodes = parseGridNodes(sm.Grid);
                var highlight = {};
                for (var i = 0; i < nodes.length; i++) {
                    highlight[nodes[i]] = true; // 1-based parent node index
                }
                showModelPreview(parentName, highlight, subModelName);
            }).fail(function () {
                alert('Could not load submodel: ' + subModelName);
            });
        }

        // Parse a serialized submodel grid string ("n,n;n,n;..." with ""=hole)
        // into a flat list of 1-based parent node numbers.
        function parseGridNodes(grid) {
            var out = [];
            if (!grid) {
                return out;
            }
            var rows = String(grid).split(';');
            for (var r = 0; r < rows.length; r++) {
                var cells = rows[r].split(',');
                for (var c = 0; c < cells.length; c++) {
                    var v = parseInt(cells[c]);
                    if (!isNaN(v) && v > 0) {
                        out.push(v);
                    }
                }
            }
            return out;
        }

        function GetChannelMemMaps() {
            $.get("api/overlays/models", function (data) {
                PopulateChannelMemMapTable(data);
            }).fail(function () {
                DialogError("Load Pixel Overlay Models", "Load Failed, is fppd running?");
            });
            $.get("api/overlays/settings", function (data) {
                PopulatePixelOverlaySettings(data);
            }).fail(function () {
            });
        }

        //give a little more detail about custom models:
        function orientationDetails(data) {
            if (data.Orientation != "custom" || !data.hasOwnProperty("data")) return "";
            //for now, just show grid size
            const rows = data.data.split(";"), cols = rows[0].split(",");
            return "&nbsp;" + cols.length + "&nbsp;x&nbsp;" + rows.length;
        }

        function SetChannelMemMaps() {
            var postData = "";
            var dataError = 0;
            var data = {};
            var models = [];

            $('#channelMemMaps tbody tr').each(function () {
                $this = $(this);

                var model = {
                    Name: $this.find("input.blk").val(),
                    Type: $this.find("span.type").html(),
                };

                if ((model.Name != "") && (model.Type != "")) {
                    switch (model.Type) {
                        case "Channel":
                            model.StartChannel = parseInt($this.find("input.start").val());
                            model.ChannelCount = parseInt($this.find("input.cnt").val());
                            model.Orientation = $this.find("select.orientation").val();
                            model.StartCorner = $this.find("select.corner").val();
                            model.StringCount = parseInt($this.find("input.strcnt").val());
                            model.StrandsPerString = parseInt($this.find("input.strands").val());
                            model.ChannelCountPerNode = parseInt($this.find("input.cpn").val());
                            model.xLights = $this.find("input.xlights").is(':checked');

                            if ((model.StartChannel > 0) &&
                                (model.ChannelCount > 0)) {
                                if (model.Orientation == "custom") {
                                    model.data = $this.find("input.data").val();
                                    model.StartCorner = $this.find("input.corner").val();
                                    models.push(model);
                                } else if ((model.StringCount > 0) &&
                                    (model.StrandsPerString > 0)) {
                                    models.push(model);
                                } else {
                                    dataError = 1;
                                    // FIXME, put in some more info here, highlight bad field, etc.
                                    alert("MemMap '" + model.BlockName + "' starting at channel '" + model.StartChannel + "' containing '" + model.ChannelCount + "' channel(s) is not valid.");
                                    return;
                                }
                            }
                            break;
                        case "FB":
                            model.Width = parseInt($this.find("input.width").val());
                            model.Height = parseInt($this.find("input.height").val());
                            model.Device = $this.find("select.device").val();
                            model.PixelSize = parseInt($this.find("input.pixelSize").val());

                            if (model.Device == 'x11') {
                                model.X = parseInt($this.find("input.xOffset").val());
                                model.Y = parseInt($this.find("input.yOffset").val());
                            }

                            models.push(model);
                            break;
                        case "Sub":
                            model.Width = parseInt($this.find("input.width").val());
                            model.Height = parseInt($this.find("input.height").val());
                            model.XOffset = parseInt($this.find("input.xOffset").val());
                            model.YOffset = parseInt($this.find("input.yOffset").val());
                            model.Parent = $this.find("select.parent").val();

                            models.push(model);
                            break;
                    }
                } else {
                    dataError = 1;
                    // FIXME, put in some more info here, highlight bad field, etc.
                    alert("MemMap '" + model.BlockName + "' starting at channel '" + model.StartChannel + "' containing '" + model.ChannelCount + "' channel(s) is not valid.");
                    return;
                }
            });

            if (dataError != 0)
                return;

            data.models = models;
            data.autoCreate = $("#AutoCreatePixelOverlays").is(':checked');
            postData = JSON.stringify(data, null, 2);

            $.post("api/models", postData).done(function (data) {
                $.jGrowl("Pixel Overlay Models saved.", { themeState: 'success' });
                SetRestartFlag(2);
                common_ViewPortChange();
            }).fail(function () {
                DialogError("Save Pixel Overlay Models", "Save Failed, is fppd running?");
            });
        }

        function AddNewChannelModel() {
            var currentRows = $("#channelMemMaps > tbody > tr").length;
            $('#channelMemMaps tbody').append(
                "<tr id='row'" + currentRows + " class='fppTableRow'>" +
                "<td class='center' valign='middle'><div class='rowGrip'><i class='rowGripIcon fpp-icon-grip'></i></div></td>" +
                "<td><input class='blk' type='text' size='31' maxlength='31' value=''></td>" +
                "<td><span class='hidden type'>Channel</span>Channel</td>" +
                "<td><input class='start' type='text' size='7' maxlength='7' value=''></td>" +
                "<td><input class='cnt' type='text' size='6' maxlength='6' value=''></td>" +
                "<td><input class='cpn' type='number' min='1' max='4' value='3'></td>" +
                "<td>" + GetOrientationInput('') + "</td>" +
                "<td>" + GetStartingCornerInput('') + "</td>" +
                "<td><input class='strcnt' type='text' size='3' maxlength='3' value='1'></td>" +
                "<td><input class='strands' type='text' size='2' maxlength='2' value='1'></td>" +
                "<td><input class='xlights' type='checkbox' disabled></td><td>" +
                "<td></td>" +
                "</tr>");
        }

        function AddNewFBModel() {
            var currentRows = $("#channelMemMaps > tbody > tr").length;
            var device = Object.keys(FBDevices)[0];
            $('#channelMemMaps tbody').append(
                "<tr id='row'" + currentRows + " class='fppTableRow'>" +
                "<td class='center' valign='middle'><div class='rowGrip'><i class='rowGripIcon fpp-icon-grip'></i></div></td>" +
                "<td><input class='blk' type='text' size='31' maxlength='31' value='" + device + "'></td>" +
                "<td><span class='hidden type'>FB</span>FrameBuffer</td>" +
                "<td>" + CreateSelect(FBDevices, device, '', '-- Port --', 'device', 'DeviceChanged(this);') + "</td>" +
                "<td class='channels'>" + (FBInfo[device].Width * FBInfo[device].Height * 3) + "</td>" +
                "<td colspan='3'>Size: <input class='width' type='number' min='0' max='4096' step='8' value='" + FBInfo[device].Width + "' onChange='WidthOrHeightModified(this);'> <b>X</b>" +
                "<input class='height' type='number' min='0' max='2160' step='8' value='" + FBInfo[device].Height + "' onChange='WidthOrHeightModified(this);'></td>" +
                "<span class='X11input'>&nbsp;@ <input class='xOffset X11input' type='number' min='8' max='4096' step='8' value='0' style='display: none;'>" +
                ",<input class='yOffset X11input' type='number' min='8' max='2160' step='8' value='0' style='display: none;'></span>" +
                "</td>" +
                "<td colspan='2'>Pixel Size: <input class='pixelSize' type='number' min='1' max='64' value='1' onChange='WidthOrHeightModified(this);'></td>" +
                "<td></td>" +
                "<td></td>" +
                "</tr>");
        }

        function AddNewSubModel() {
            var currentRows = $("#channelMemMaps > tbody > tr").length;
            $('#channelMemMaps tbody').append(
                "<tr id='row'" + currentRows + " class='fppTableRow'>" +
                "<td class='center' valign='middle'><div class='rowGrip'><i class='rowGripIcon fpp-icon-grip'></i></div></td>" +
                "<td><input class='blk' type='text' size='31' maxlength='31' value=''></td>" +
                "<td><span class='hidden type'>Sub</span>Sub-Model</td>" +
                "<td>" + CreateSelect(PixelOverlayModels, '', '', '-- Parent --', 'parent', '') + "</td>" +
                "<td class='channels'>" + (64 * 32 * 3) + "</td><td>3</td>" +
                "<td>xOffset: <input class='xOffset' type='number' min='0' max='4096' value='0'></td>" +
                "<td>yOffset: <input class='yOffset' type='number' min='0' max='2160' value='0'></td>" +
                "<td colspan='2'><input class='width' type='number' min='8' max='4096' value='64' onChange='WidthOrHeightModified(this);'> <b>X</b>" +
                "<input class='height' type='number' min='8' max='2160' value='32' onChange='WidthOrHeightModified(this);'></td>" +
                "<td></td>" +
                "<td></td>" +
                "</tr>");
        }

        function AddNewModel() {
            DoModalDialog({
                id: "AddPixelOverlayModel",
                body: "Choose the type of model to add.",
                title: "Add Pixel Overlay Model",
                backdrop: true,
                keyboard: true,
                class: "modal-sm",
                buttons: {
                    Channel: function () {
                        AddNewChannelModel();
                        common_ViewPortChange();
                        CloseModalDialog("AddPixelOverlayModel");
                    },
                    <? if ($showAddFBButton) { ?>
                        FrameBuffer: function () {
                            AddNewFBModel();
                            common_ViewPortChange();
                            CloseModalDialog("AddPixelOverlayModel");
                        },
                    <? } ?>
                    SubModel: function () {
                        AddNewSubModel();
                        common_ViewPortChange();
                        CloseModalDialog("AddPixelOverlayModel");
                    }
                }
            });
        }

        var tableInfo = {
            tableName: "channelMemMaps",
            selected: -1,
            enableButtons: ["btnDelete"],
            disableButtons: [],
            sortable: 1
        };


        function DeleteSelectedMemMap() {
            if (tableInfo.selected >= 0) {
                //$('#channelMemMaps tbody tr:nth-child(' + (tableInfo.selected+1) + ')').remove();
                $('#channelMemMaps .selectedEntry').remove();
                tableInfo.selected = -1;
                SetButtonState("#btnDelete", "disable");
                common_ViewPortChange();
            }
        }

        function DeleteXlightsModels() {
            $('#channelMemMaps tbody tr').each(function () {
                $this = $(this);
                if ($this.find("input.xlights").is(':checked')) {
                    $this.remove();
                }
            });
            common_ViewPortChange();
        }

        // highlightNodes (optional): map of 1-based parent node index -> true.
        // When provided, those pixels (by node order in the preview list) are
        // drawn highlighted and the rest dimmed, so a submodel can be shown in
        // the context of its parent.  subModelName (optional) adjusts the title.
        function showModelPreview(modelName, highlightNodes, subModelName) {
            $('#modelPreviewModal').remove();

            var pixels = getModelPreviewPixels(modelName);
            if (!pixels || pixels.length === 0) {
                alert('No preview data available for model: ' + modelName);
                return;
            }
            var hasHighlight = highlightNodes && typeof highlightNodes === 'object';

            var minX = Infinity, maxX = -Infinity;
            var minY = Infinity, maxY = -Infinity;
            pixels.forEach(function (p) {
                minX = Math.min(minX, p[0]); maxX = Math.max(maxX, p[0]);
                minY = Math.min(minY, p[1]); maxY = Math.max(maxY, p[1]);
            });

            var rangeX = maxX - minX || 1;
            var rangeY = maxY - minY || 1;

            var canvasW = 560;
            var canvasH = Math.round(canvasW * rangeY / rangeX);
            if (canvasH > 420) {
                canvasH = 420;
                canvasW = Math.round(canvasH * rangeX / rangeY);
            }
            if (canvasH < 80) canvasH = 80;
            if (canvasW < 80) canvasW = 80;

            var $canvas = $('<canvas></canvas>').attr({ width: canvasW, height: canvasH }).css({ background: '#111', display: 'block', margin: '0 auto' });
            var infoText = pixels.length + ' pixels  |  W: ' + rangeX + '  H: ' + rangeY;
            if (hasHighlight) {
                var hcount = 0;
                for (var k in highlightNodes) { if (highlightNodes.hasOwnProperty(k)) hcount++; }
                infoText += '  |  submodel nodes: ' + hcount;
            }
            var $info = $('<p>').css({ 'text-align': 'center', 'margin-bottom': '8px' }).text(infoText);
            var $body = $('<div>').append($info).append($canvas);

            DoModalDialog({
                id: 'modelPreviewModal',
                title: hasHighlight ? ('Submodel Preview: ' + (subModelName || '')) : ('Model Preview: ' + modelName),
                body: $body,
                class: 'modal-lg',
                backdrop: true,
                keyboard: true,
                open: function () {
                    var canvas = $canvas[0];
                    var ctx = canvas.getContext('2d');
                    var margin = 8;
                    var scaleX = (canvasW - 2 * margin) / rangeX;
                    var scaleY = (canvasH - 2 * margin) / rangeY;
                    var dotSize = Math.max(1, Math.min(6, Math.floor(Math.min(scaleX, scaleY) * 0.8)));
                    pixels.forEach(function (p, idx) {
                        var cx = margin + (p[0] - minX) * scaleX;
                        var cy = canvasH - margin - (p[1] - minY) * scaleY;
                        if (hasHighlight) {
                            // preview pixels are in node order; node index is 1-based
                            ctx.fillStyle = highlightNodes[idx + 1] ? '#ff3b6b' : '#2a3a44';
                        } else {
                            ctx.fillStyle = '#00aaff';
                        }
                        ctx.fillRect(cx - dotSize / 2, cy - dotSize / 2, dotSize, dotSize);
                    });
                },
                buttons: {
                    Close: function () { CloseModalDialog('modelPreviewModal'); }
                }
            });
        }

        function pageSpecific_PageLoad_DOM_Setup() {
            SetupSelectableTableRow(tableInfo);
            GetChannelMemMaps();
            PopulateModelGroups();
        }

        function pageSpecific_PageLoad_PostDOMLoad_ActionsSetup() {
            //Mouse click on table rows
            $('#channelMemMaps').on('mousedown', 'tr', function (event, ui) {
                HandleTableRowMouseClick(event, $(this));

                //console.log(event.target.nodeName);

                if ($('#channelMemMaps > tr.selectedEntry').length > 0) {
                    EnableButtonClass('btnDelete');
                } else {
                    DisableButtonClass('btnDelete');
                }
            });
        }

    </script>

    <title><? echo $pageTitle; ?></title>
</head>

<body>
    <div id="bodyWrapper">
        <?php
        $activeParentMenuItem = 'input-output';
        include 'menu.inc'; ?>
        <div class="mainContainer">
            <h1 class="title">Pixel Overlay Models</h1>
            <div class="pageContent">


                <div class="row tablePageHeader">
                    <div class="col-md">
                        <h2>Pixel Overlay Models</h2>
                    </div>
                    <div class="col-md-auto ms-lg-auto">
                        <div class="form-actions">

                            <input type=button value='Delete' onClick='DeleteSelectedMemMap();'
                                data-btn-enabled-class="btn-outline-danger" id='btnDelete' class='disableButtons'>
                            <input type=button value='Delete xLights Generated' onClick='DeleteXlightsModels();'
                                id='btnDeleteXlights' class='buttons btn-outline-danger'>
                            <button type=button value='Add' onClick='AddNewModel();'
                                class='buttons btn-outline-success'><i class="fas fa-plus"></i> Add</button>
                            <input type=button value='Save' onClick='SetChannelMemMaps();'
                                class='buttons btn-success ml-1'>

                        </div>
                    </div>
                </div>
                <div>
                    Create Overlays Automatically From Outputs: <input id="AutoCreatePixelOverlays" type="checkbox"
                        checked />
                </div>
                <hr>
                <div class='fppTableWrapper fppTableWrapperAsTable'>
                    <div class='fppTableContents fppFThScrollContainer' role="region" aria-labelledby="channelMemMaps"
                        tabindex="0">
                        <table id="channelMemMaps" class="fppSelectableRowTable fppStickyTheadTable">
                            <thead>
                                <tr>
                                    <th class="tblChannelMemMapsHeadGrip"></th>
                                    <th><span title='Name of Model'>Model Name</span></th>
                                    <th><span title='Type'>Type</span></th>
                                    <th><span title='Start Channel'>Start Ch.</span></th>
                                    <th><span title='Channel Count'>Ch. Count</span></th>
                                    <th><span title='Chan Per Node'>Ch./Node</span></th>
                                    <th><span title='String Orientation'>Orientation</span></th>
                                    <th><span title='Starting Corner'>Start Corner</span></th>
                                    <th><span title='Number of Strings or Width of FB/X11/Sub-Model'>Strings</span></th>
                                    <th><span
                                            title='Number of Strands Per String or Height of FB/X11/Sub-Model'>Strands</span>
                                    </th>
                                    <th><span title='Model was Generated and Uploaded from xLights'>xLights
                                            Generated</span></th>
                                    <th><span title='Running Effect'>Running Effect</span></th>
                                </tr>
                            </thead>
                            <tbody>
                            </tbody>
                        </table>
                    </div>
                </div>

                <div class="row tableHeader">
                    <div class="col-md">
                        <h2>Auto Created Pixel Overlay Models</h2>
                    </div>
                </div>
                <div class='fppTableWrapper fppTableWrapperAsTable'>
                    <div class='fppTableContents fppFThScrollContainer' role="region"
                        aria-labelledby="channelMemMapsAutoCreate" tabindex="0">
                        <table id="channelMemMapsAutoCreate" class="fppSelectableRowTable fppStickyTheadTable">
                            <thead>
                                <tr>
                                    <th class="tblChannelMemMapsHeadGrip"></th>
                                    <th><span title='Name of Model'>Model Name</span></th>
                                    <th><span title='Type'>Type</span></th>
                                    <th><span title='Start Channel'>Start Ch.</span></th>
                                    <th><span title='Channel Count'>Ch. Count</span></th>
                                    <th><span title='Chan Per Node'>Ch./Node</span></th>
                                    <th><span title='String Orientation'>Orientation</span></th>
                                    <th><span title='Starting Corner'>Start Corner</span></th>
                                    <th><span title='Number of Strings or Width of FB/X11/Sub-Model'>Strings</span></th>
                                    <th><span
                                            title='Number of Strands Per String or Height of FB/X11/Sub-Model'>Strands</span>
                                    </th>
                                    <th><span title='Running Effect'>Running Effect</span></th>
                                </tr>
                            </thead>
                            <tbody>
                            </tbody>
                        </table>
                    </div>
                </div>

                <div id="modelGroupsSection" style="display:none;">
                    <div class="row tableHeader">
                        <div class="col-md">
                            <h2>xLights Model Groups <span id="modelGroupsCount"
                                    style="font-size:0.6em;color:#9b59b6;"></span></h2>
                            <p style="font-size:0.85em;color:#aaa;margin:0;">Groups are first-class overlay models
                                (addressable by name through the standard overlay commands, e.g. "Overlay Model
                                Effect"). An effect runs across the group's combined world-position buffer.</p>
                        </div>
                    </div>
                    <div class='fppTableWrapper fppTableWrapperAsTable'>
                        <div class='fppTableContents fppFThScrollContainer' tabindex="0">
                            <table id="modelGroupsTable" class="fppSelectableRowTable fppStickyTheadTable">
                                <thead>
                                    <tr>
                                        <th style="text-align:left;">Group Name</th>
                                        <th>Members</th>
                                        <th>Pixels</th>
                                        <th>Buffer</th>
                                        <th>Overlay Model Name</th>
                                    </tr>
                                </thead>
                                <tbody></tbody>
                            </table>
                        </div>
                    </div>
                </div>
            </div>
        </div>

        <?php include 'common/footer.inc'; ?>
    </div>


</body>

</html>