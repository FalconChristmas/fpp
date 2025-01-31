<!DOCTYPE html>
<html lang="en">

<head>
    <?php
    include 'common/htmlMeta.inc';
    require_once "common.php";
    require_once 'universeentry.php';
    if (file_exists(__DIR__ . "/fppdefines.php")) {
        include_once __DIR__ . '/fppdefines.php';
    } else {
        include_once __DIR__ . '/fppdefines_unknown.php';
    }
    include 'common/menuHead.inc';
    ?>
    <script language="Javascript">

        var currentTabTitle = "E1.31/DDP Input";

        /////////////////////////////////////////////////////////////////////////////
        // E1.31 support functions here
        $(document).ready(function () {
            $('.default-value').each(function () {
                var default_value = this.value;
                $(this).on("focus", function () {
                    if (this.value == default_value) {
                        this.value = '';
                        $(this).css('color', '#333');
                    }
                });
                $(this).on("blur", function () {
                    if (this.value == '') {
                        $(this).css('color', '#999');
                        this.value = default_value;
                    }
                });
            });

            $.ajax({
                url: "api/settings/BridgeInputDelayBeforeBlack",
                method: "GET",
                dataType: "json",
                success: function (data) {
                    let val = 0;
                    if ("value" in data) {
                        val = data.value;
                    }
                    $('#txtBridgeInputDelayBeforeBlack').val(val);
                }
            });

            $('#txtUniverseCount').on('focus', function () {
                $(this).select();
            });

            $('#txtBridgeInputDelayBeforeBlack').on("change", function () {
                var newValue = $('#txtBridgeInputDelayBeforeBlack').val();
                $.ajax({
                    url: "api/settings/BridgeInputDelayBeforeBlack",
                    data: newValue,
                    method: "PUT",
                    dataType: "text",
                    success: function (data) {
                        $.jGrowl("Input Delay Saved", { themeState: 'success' });
                        SetRestartFlag(2);
                        CheckRestartRebootFlags();
                        common_ViewPortChange();
                    }
                });

            });

            if (window.innerWidth > 600) {

            }
            var sortableOptions = {
                start: function (event, ui) {
                    start_pos = ui.item.index();
                },
                update: function (event, ui) {
                    SetUniverseInputNames();
                },
                beforeStop: function (event, ui) {
                    //undo the firefox fix.
                    // Not sure what this is, but copied from playlists.php to here
                    if (navigator.userAgent.toLowerCase().match(/firefox/) && ui.offset !== undefined) {
                        $(window).unbind('scroll.sortableplaylist');
                        ui.helper.css('margin-top', 0);
                    }
                },
                helper: function (e, ui) {
                    ui.children().each(function () {
                        $(this).width($(this).width());
                    });
                    return ui;
                },
                scroll: true
            }
            if (hasTouch) {
                $.extend(sortableOptions, { handle: '.rowGrip' });
            }
            $('#tblUniversesBody').sortable(sortableOptions).disableSelection();

            $('#tblUniverses').on('mousedown', 'tr', function (event, ui) {
                $('#tblUniverses tr').removeClass('selectedEntry');
                $(this).addClass('selectedEntry');
                var items = $('#tblUniverses tbody tr');
                UniverseSelected = items.index(this);
            });

            $('#frmUniverses').on("submit", function (event) {
                event.preventDefault();
                var success = validateUniverseData();
                if (success == true) {
                    postUniverseJSON(true);
                    return false;
                } else {
                    DialogError("Save E1.31 Universes", "Validation Failed");
                }
            });
        });
        var SerialDevices = new Array();
        <?
        $dmxStyle = "hidden";
        if (isset($settings['cape-info']['tty-labels'])) {
            foreach ($settings['cape-info']['tty-labels'] as $label => $device) {
                echo "SerialDevices['$label'] = '$label';\n";
            }
        }
        foreach (scandir("/dev/") as $fileName) {
            if (
                (preg_match("/^ttySC?[0-9]+/", $fileName)) ||
                (preg_match("/^ttyACM[0-9]+/", $fileName)) ||
                (preg_match("/^ttyO[0-9]/", $fileName)) ||
                (preg_match("/^ttyAMA[0-9]+/", $fileName)) ||
                (preg_match("/^ttyUSB[0-9]+/", $fileName))
            ) {
                echo "SerialDevices['$fileName'] = '$fileName';\n";
                //echo "SerialDevices.push('$fileName');\n";
                $dmxStyle = "";
            }
        }
        ?>

        function SaveDMX() {
            var dmxs = [];
            $('#tblDMXs tbody tr').each(function () {
                var row = $(this);
                var device = row.attr('id');
                var enabled = row.find('input[type="checkbox"]').prop('checked');
                var startAddress = parseInt(row.find('input[type="number"]').eq(0).val());
                var numChannels = parseInt(row.find('input[type="number"]').eq(1).val());
                dmxs.push({
                    device: device,
                    enabled: enabled,
                    startAddress: startAddress,
                    channelCount: numChannels,
                    type: "dmx"
                });
            });
            var data = {
                channelInputs: dmxs
            };

            $.post('api/channel/output/dmxInputs', JSON.stringify(data))
                .done(function (data) {
                    $.jGrowl('DMX Inputs Saved', { themeState: 'success' });
                    SetRestartFlag(2);
                    CheckRestartRebootFlags();
                    common_ViewPortChange();
                })
                .fail(function () {
                    DialogError('Save Universes', 'Error: Unable to save DMX Inputs.');
                });

        }
        function LoadDMX() {
            $.get('api/channel/output/dmxInputs')
                .done(function (data) {
                    if (data.hasOwnProperty("channelInputs")) {
                        data.channelInputs.forEach(function (dmx) {
                            var row = $('#' + dmx.device);
                            if (row.length) {
                                row.find('input[type="checkbox"]').prop('checked', dmx.enabled);
                                row.find('input[type="number"]').eq(0).val(dmx.startAddress);
                                row.find('input[type="number"]').eq(1).val(dmx.channelCount);
                            }
                        });
                    }
                });
        }
        /////////////////////////////////////////////////////////////////////////////

        function handleCIKeypress(e) {
            if (e.keyCode == 113) {
                //		if (currentTabTitle == "Pi Pixel Strings")
                //			setPixelStringsStartChannelOnNextRow();
            }
        }

        $(document).ready(function () {
            $(document).on('keydown', handleCIKeypress);

            // E1.31 initialization
            InitializeUniverses();
            getUniverses('TRUE', 1);
            LoadDMX();

            // Init tabs
            $tabs = $("#tabs").tabs({
                activate: function (e, ui) {
                    currentTabTitle = $(ui.newTab).text();
                },
                cache: true,
                spinner: "",
                fx: {
                    opacity: 'toggle',
                    height: 'toggle'
                }
            });

            var total = $tabs.find('.ui-tabs-nav li').length;
            if (total > 1) {
                var currentLoadingTab = 1;
                $tabs.bind('tabsload', function () {
                    currentLoadingTab++;
                    if (currentLoadingTab < total)
                        $tabs.tabs('load', currentLoadingTab);
                    else
                        $tabs.unbind('tabsload');
                }).tabs('load', currentLoadingTab);
            }

        });

    </script>
    <title><? echo $pageTitle; ?></title>
</head>

<body>
    <div id="bodyWrapper">
        <?php
        $activeParentMenuItem = 'input-output';
        include 'menu.inc'; ?>
        <div class="mainContainer">
            <h1 class="title">Channel Inputs</h1>
            <div class="pageContent">

                <div id='channelInputManager'>
                    <ul class="nav nav-pills pageContent-tabs" id="channelOutputTabs" role="tablist">
                        <li class="nav-item">
                            <a class="nav-link active" id="tab-e131-tab" tabType='UDP' data-bs-toggle="pill"
                                href="#tab-e131" role="tab" aria-controls="tab-e131" aria-selected="true">
                                E1.31/ArtNet/DDP Inputs
                            </a>
                        </li>
                        <li class="nav-item <?= $dmxStyle ?>">
                            <a class="nav-link" id="tab-dmx-tab" tabType='DMX' data-bs-toggle="pill" href="#tab-dmx"
                                role="tab" data-bs-target="#tab-dmx" aria-controls="tab-dmx" aria-selected="false">
                                DMX
                            </a>
                        </li>
                    </ul>

                    <!-- --------------------------------------------------------------------- -->



                    <div class="tab-content" id="channelOutputTabsContent">
                        <div class="tab-pane fade show active" id="tab-e131" role="tabpanel"
                            aria-labelledby="tab-e131-tab">


                            <div id='divE131'>


                                <form id="frmUniverses">
                                    <div class="row tableTabPageHeader">
                                        <div class="col-md">
                                            <h2>E1.31 / ArtNet / DDP Inputs</h2>
                                        </div>
                                        <div class="col-md-auto ms-lg-auto">
                                            <div class="form-actions">
                                                <input name="input" type="hidden" value="0" />
                                                <input id="btnDeleteUniverses" class="buttons btn-outline-danger"
                                                    type="button" value="Delete" onClick="DeleteUniverse(1);" />
                                                <input id="btnCloneUniverses" class="buttons" type="button"
                                                    value="Clone" onClick="CloneUniverse();" />
                                                <input id="btnSaveUniverses" class="buttons btn-success ms-1"
                                                    type="submit" value="Save" />
                                            </div>
                                        </div>
                                    </div>
                                    <div class="backdrop tableOptionsForm">
                                        <div class="row">
                                            <div class="col-md-auto">
                                                <div class="backdrop-dark form-inline enableCheckboxWrapper">
                                                    <div><b>Enable Input:</b></div>
                                                    <div> <input type="checkbox" id="E131Enabled" /></div>
                                                </div>
                                            </div>
                                            <div class="col-md-auto form-inline">
                                                <div><b>Timeout:</b></div>
                                                <div><input id="bridgeTimeoutMS" type="number" min="0" max="9999"
                                                        size="4" maxlength="4">
                                                    <img id="timeout_img"
                                                        title="Timeout for input channel data (in MS).  If no new data is received for this time, the input data is cleared."
                                                        src="images/redesign/help-icon.svg" width=22 height=22>
                                                </div>
                                            </div>
                                            <div class="col-md-auto form-inline">
                                                <div><b>Inputs Count: </b></div>
                                                <div><input id="txtUniverseCount" class="default-value" type="text"
                                                        value="Enter Universe Count" size="3" maxlength="3" /></div>
                                                <div><input id="btnUniverseCount" onclick="SetUniverseCount(1);"
                                                        type="button" class="buttons" value="Set" /></div>
                                            </div>
                                        </div>
                                    </div>
                                    <div class='fppTableWrapper'>
                                        <div class='fppTableContents fppFThScrollContainer' role="region"
                                            aria-labelledby="tblUniverses" tabindex="0">
                                            <table id="tblUniverses"
                                                class='universeTable fullWidth fppSelectableRowTable fppStickyTheadTable'>
                                                <thead id='tblUniversesHead'>
                                                    <tr>
                                                        <th class="tblScheduleHeadGrip"></th>
                                                        <th>Input</th>
                                                        <th>Active</th>
                                                        <th>Description</th>
                                                        <th>Input Type</th>
                                                        <th>FPP Channel Start</th>
                                                        <th>FPP Channel End</th>
                                                        <th>Universe #</th>
                                                        <th>Universe Count</th>
                                                        <th>Universe Size</th>
                                                    </tr>
                                                </thead>
                                                <tbody id='tblUniversesBody'>
                                                </tbody>
                                            </table>
                                        </div>
                                    </div>
                                    <small class="text-muted">(Drag entry to reposition) </small>
                                </form>
                            </div>
                        </div>
                    </div>
                    <div class="tab-content" id="channelOututTabsContent">
                        <div class="tab-pane fade" id="tab-dmx" role="tabpanel" aria-labelledby="tab-dmx-tab">
                            <div id='divDMX'>
                                <div class="row tableTabPageHeader">
                                    <div class="col-md">
                                        <h2>DMX</h2>
                                    </div>
                                    <div class="col-md-auto ms-lg-auto">
                                        <div class="form-actions">
                                            <input id="btnSaveUniverses" class="buttons btn-success ms-1" type="button"
                                                value="Save" onClick="SaveDMX()" />
                                        </div>
                                    </div>
                                </div>
                                <div class='fppTableWrapper'>
                                    <div class='fppTableContents fppFThScrollContainer' role="region"
                                        aria-labelledby="tblDMXs" tabindex="0">
                                        <table id="tblDMXs"
                                            class='dmxTable fullWidth fppSelectableRowTable fppStickyTheadTable'>
                                            <thead id='tblDMXHead'>
                                                <tr>
                                                    <th>Enable</th>
                                                    <th>Serial Port</th>
                                                    <th>Start Channel</th>
                                                    <th>Num Channels</th>
                                                </tr>
                                            </thead>
                                            <tbody id='tblUniversesBody'>
                                                <script>
                                                    Object.keys(SerialDevices).forEach(function (device) {
                                                        var row = "<tr id='" + device + "'>";
                                                        row += "<td><input type='checkbox'></td>";
                                                        row += "<td>" + device + "</td>";
                                                        row += "<td><input class='txtStartAddress singleDigitInput' type='number' min='1' max='8388608' value='1' id='txtStartAddress-" + device + "'></td>";
                                                        row += "<td><input type='number' min='1' max='512' value='512'></td>";
                                                        row += "</tr>";
                                                        $('#tblDMXs tbody').append(row);
                                                    });
                                                </script>
                                            </tbody>
                                        </table>
                                    </div>
                                </div>
                            </div>
                        </div>
                        <!-- --------------------------------------------------------------------- -->
                    </div>
                </div>

                <div id='debugOutput'>
                </div>

            </div>
        </div>
    </div>

    <?php include 'common/footer.inc'; ?>
</body>

</html>