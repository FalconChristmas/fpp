<!DOCTYPE html>
<html lang="en">

<head>
    <?php
    include 'common/htmlMeta.inc';
    require_once 'config.php';
    require_once 'common.php';
    include 'common/menuHead.inc';
    ?>
    <script>


        function OnEnableClicked(name) {
            $.get("api/command/Set Port Status/" + encodeURIComponent(name) + "/true");
        }
        function OnDisableClicked(name) {
            $.get("api/command/Set Port Status/" + encodeURIComponent(name) + "/false");
        }

        function CountPixels() {
            $.get("api/fppd/ports/pixelCount");
        }

        function FormatSmartReceiver(first, name, k, port, row, col, totals) {
            if (port == null) {
                return "";
            }
            var html = "";
            if (first) {
                html += "<b>" + name + "</b><br>";
            }
            html += "<b>" + k + ":</b><br>";
            html += "<div style='margin-left: 15px;'>";

            if (port["enabled"]) {
                html += "Enabled: <i class='fas fa-check-circle text-success' title='Port Enabled' onclick='OnDisableClicked(\"" + name + k + "\")'></i><br>";
            } else if (port["status"]) {
                html += "Enabled: <i class='fas fa-times-circle text-info' title='Port Disabled' onclick='OnEnableClicked(\"" + name + k + "\")'></i><br>";
            } else {
                html += "Enabled: <i class='fas fa-times-circle text-danger' title='eFuse Triggered' onclick='OnEnableClicked(\"" + name + k + "\")'></i><br>";
            }
            if (port["status"]) {
                html += "Status: <i class='fas fa-check-circle text-success' title='eFuse Normal'></i><br>";
            } else {
                html += "Status: <i class='fas fa-times-circle text-danger' title='eFuse Triggered'></i><br>";
            }
            if (typeof port.pixelCount !== 'undefined') {
                html += "Pixels: " + port["pixelCount"] + "<br>";
            }
            html += port["ma"] + " ma";

            html += "</div>";
            $("#fppPorts tr:nth-child(" + row + ") td:nth-child(" + col + ")").html(html);
            $("#fppPorts tr:nth-child(" + row + ")").show();


            var idx = name.substr(5);
            idx = (idx - 1) & 0xFFFC;
            var key = "Smart Receiver " + (idx + 1) + "-" + (idx + 4) + k;
            if (!totals.has(key)) {
                totals.set(key, port["ma"]);
            } else {
                totals.set(key, port["ma"] + totals.get(key));
            }
        }
        function StartMonitoring() {
            $.get('api/fppd/ports', function (data) {
                var totals = new Map();
                data.forEach(function (port) {
                    var count = $('#fppPorts tr').length;
                    var rn = port["row"] + 1;
                    if (port["smartReceivers"]) {
                        rn += 6;
                    }
                    while (rn >= count) {
                        $('#fppPorts').append("<tr style='display:none'><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td></tr>");
                        count++;
                    }
                    if (!port["smartReceivers"]) {
                        $("#fppPorts tr:nth-child(" + port["row"] + ")").show();
                        var html = "<b>" + port["name"] + "</b><br>";
                        if (port["enabled"]) {
                            html += "Enabled: <i class='fas fa-check-circle text-success' title='Port Enabled' onclick='OnDisableClicked(\"" + port["name"] + "\")'></i><br>";
                        } else if (port["status"]) {
                            html += "Enabled: <i class='fas fa-times-circle text-info' title='Port Disabled' onclick='OnEnableClicked(\"" + port["name"] + "\")'></i><br>";
                        } else {
                            html += "Enabled: <i class='fas fa-times-circle text-danger' title='eFuse Triggered' onclick='OnEnableClicked(\"" + port["name"] + "\")'></i><br>";
                        }
                        if (port["status"]) {
                            html += "Status: <i class='fas fa-check-circle text-success' title='eFuse Normal'></i><br>";
                        } else {
                            html += "Status: <i class='fas fa-times-circle text-danger' title='eFuse Triggered'></i><br>";
                        }
                        if (typeof port.pixelCount !== 'undefined') {
                            html += "Pixels: " + port["pixelCount"] + "<br>";
                        }
                        html += port["ma"] + " ma";
                        $("#fppPorts tr:nth-child(" + port["row"] + ") td:nth-child(" + port["col"] + ")").html(html);

                        var key;
                        if (!port["bank"]) {
                            var idx = port["name"].substr(5);
                            idx = Math.floor((idx - 1) / 8);
                            key = "Ports " + (idx * 8 + 1) + "-" + (idx * 8 + 8);
                        } else {
                            key = port["bank"];
                        }
                        if (!totals.has(key)) {
                            totals.set(key, port["ma"]);
                        } else {
                            totals.set(key, port["ma"] + totals.get(key));
                        }
                    } else {
                        rn = port["row"];
                        FormatSmartReceiver(true, port["name"], "A", port["A"], rn, port["col"], totals);
                        FormatSmartReceiver(false, port["name"], "B", port["B"], rn + 1, port["col"], totals);
                        FormatSmartReceiver(false, port["name"], "C", port["C"], rn + 2, port["col"], totals);
                        FormatSmartReceiver(false, port["name"], "D", port["D"], rn + 3, port["col"], totals);
                        FormatSmartReceiver(false, port["name"], "E", port["E"], rn + 4, port["col"], totals);
                        FormatSmartReceiver(false, port["name"], "F", port["F"], rn + 5, port["col"], totals);
                    }
                });
                var banks = "<b>Banks</b><br>";
                for (const [key, value] of totals.entries()) {
                    banks += key + ": " + value + " ma<br>";
                }
                var count = $('#fppPorts tr').length;
                $("#fppPorts tr:nth-child(" + (count - 1) + ") td:nth-child(1)").html(banks);
                $("#fppPorts tr:nth-child(" + (count - 1) + ") td:nth-child(2)").remove();
                $("#fppPorts tr:nth-child(" + (count - 1) + ") td:nth-child(3)").remove();
                $("#fppPorts tr:nth-child(" + (count - 1) + ") td:nth-child(4)").remove();
                $("#fppPorts tr:nth-child(" + (count - 1) + ") td:nth-child(1)").attr("colspan", 4);
                $("#fppPorts tr:nth-child(" + (count - 1) + ")").show();
            });
        }
        function StopPixelCount() {
            $.get("api/fppd/ports/stop");
        }

        function SetupPage() {
            setInterval(StartMonitoring, 1000);
            window.addEventListener('beforeunload', StopPixelCount, false);
        }
    </script>

    <title><? echo $pageTitle; ?></title>
    <style>
        .progress-bar-vertical {
            max-width: 20px;
            min-width: 10px;
            min-height: 100px;
            display: flex;
            align-items: flex-end;
            margin-right: 20px;
            float: left;
        }

        .progress-bar-vertical .progress-bar {
            width: 100%;
            height: 0;
            -webkit-transition: height 0.6s ease;
            -o-transition: height 0.6s ease;
            transition: height 0.6s ease;
        }
    </style>
    <script>
        $(function () {
            $("#slider-range").slider({
                range: true,
                min: 0,
                max: 500,
                values: [75, 300],
                slide: function (event, ui) {
                    $("#amount").val("$" + ui.values[0] + " - $" + ui.values[1]);
                }
            });
            $("#amount").val("$" + $("#slider-range").slider("values", 0) +
                " - $" + $("#slider-range").slider("values", 1));
        });
    </script>

</head>

<!--work in progress  - an alternative layout for the currentmonitor page using responsive bootstrao css rather than table layout -->

<body class="is-loading" onLoad="SetupPage();">
    <div id="bodyWrapper">
        <?php
        $activeParentMenuItem = 'status';
        include 'menu.inc'; ?>
        <div class="mainContainer">
            <h2 class="title d-none d-sm-block ">FPP Current Monitor</h2>
            <div class="pageContent">
                <input id="btnCountPixels" class="buttons" type="button" value="Count Pixels" onClick="CountPixels();">

                <div id="slider-range" class="fppMinMaxSliderRange"></div>
                <input type="text" id="amount" readonly="" style="border:0; color:#f6931f; font-weight:bold;">



                <!-- Example Code -->

                <div class="container-flex">
                    <div id="bank1">
                        <div class="row w-100">
                            <div class="bankinfo col-2 card">
                                <div class="card-body">
                                    <h5 class="card-title">Bank 1</h5>
                                    <div class="container">
                                        <div class="progress progress-bar-vertical">
                                            <div class="progress-bar bg-success progress-bar-striped progress-bar-animated"
                                                role="progressbar" aria-valuenow="60" aria-valuemin="0"
                                                aria-valuemax="100" style="height: 60%;">
                                            </div>
                                        </div>
                                    </div>
                                    <p class="card-text">Total Current Draw:</p>
                                </div>
                            </div>
                            <div class="bankports col-10 bg-success">
                                <div class="row w-100 justify-content-evenly">

                                    <div class="card col-3">
                                        <h5 class="card-title">Port 1</h5>
                                        <div class="row">
                                            <div class="col-2">
                                                <div class="container">
                                                    <div class="progress progress-bar-vertical">
                                                        <div class="progress-bar bg-success progress-bar-striped progress-bar-animated"
                                                            role="progressbar" aria-valuenow="60" aria-valuemin="0"
                                                            aria-valuemax="100" style="height: 60%;">
                                                        </div>
                                                    </div>
                                                </div>
                                            </div>
                                            <div class="col-10">
                                                <img src="images/3P-phoenix-cad.svg" style="--color_fill: red;"
                                                    alt="Port-img-1" width="70%" />
                                            </div>
                                        </div>
                                        <div class="card-body">
                                            <p class="card-text">Pixels: <span id="port1_pix_count">10</span></p>
                                            <p class="card-text">Current: <span id="port1_current">452</span>mA</p>
                                            <a href="#" class="btn btn-primary">Test String</a>
                                        </div>
                                    </div>

                                    <div class="card col-3">
                                        <h5 class="card-title">Port 2</h5>
                                        <div class="row">
                                            <div class="col-2">
                                                <div class="container">
                                                    <div class="progress progress-bar-vertical">
                                                        <div class="progress-bar bg-warning progress-bar-striped progress-bar-animated"
                                                            role="progressbar" aria-valuenow="80" aria-valuemin="0"
                                                            aria-valuemax="100" style="height: 80%;">
                                                        </div>
                                                    </div>
                                                </div>
                                            </div>
                                            <div class="col-10">
                                                <img src="images/3P-phoenix-cad.svg" style="--color_fill: red;"
                                                    alt="Port-img-1" width="70%" />
                                            </div>
                                        </div>
                                        <div class="card-body">
                                            <p class="card-text">Pixels: <span id="port1_pix_count">10</span></p>
                                            <p class="card-text">Current: <span id="port1_current">452</span>mA</p>
                                            <a href="#" class="btn btn-primary">Test String</a>
                                        </div>
                                    </div>

                                    <div class="card col-3">
                                        <h5 class="card-title">Port 3</h5>
                                        <div class="row">
                                            <div class="col-2">
                                                <div class="container">
                                                    <div class="progress progress-bar-vertical">
                                                        <div class="progress-bar bg-success progress-bar-striped progress-bar-animated"
                                                            role="progressbar" aria-valuenow="30" aria-valuemin="0"
                                                            aria-valuemax="100" style="height: 30%;">
                                                        </div>
                                                    </div>
                                                </div>
                                            </div>
                                            <div class="col-10">
                                                <img src="images/3P-phoenix-cad.svg" style="--color_fill: red;"
                                                    alt="Port-img-1" width="70%" />
                                            </div>
                                        </div>
                                        <div class="card-body">
                                            <p class="card-text">Pixels: <span id="port1_pix_count">10</span></p>
                                            <p class="card-text">Current: <span id="port1_current">452</span>mA</p>
                                            <a href="#" class="btn btn-primary">Test String</a>
                                        </div>
                                    </div>

                                    <div class="card col-3">
                                        <h5 class="card-title">Port 4</h5>
                                        <div class="row">
                                            <div class="col-2">
                                                <div class="container">
                                                    <div class="progress progress-bar-vertical">
                                                        <div class="progress-bar bg-danger progress-bar-striped progress-bar-animated"
                                                            role="progressbar" aria-valuenow="95" aria-valuemin="0"
                                                            aria-valuemax="100" style="height: 95%;">
                                                        </div>
                                                    </div>
                                                </div>
                                            </div>
                                            <div class="col-10">
                                                <img src="images/3P-phoenix-cad.svg" style="--color_fill: red;"
                                                    alt="Port-img-1" width="70%" />
                                            </div>
                                        </div>
                                        <div class="card-body">
                                            <p class="card-text">Pixels: <span id="port1_pix_count">10</span></p>
                                            <p class="card-text">Current: <span id="port1_current">452</span>mA</p>
                                            <a href="#" class="btn btn-primary">Test String</a>
                                        </div>
                                    </div>


                                    <div class="card col-3">
                                        <h5 class="card-title">Port 5</h5>
                                        <div class="row">
                                            <div class="col-2">
                                                <div class="container">
                                                    <div class="progress progress-bar-vertical">
                                                        <div class="progress-bar bg-success progress-bar-striped progress-bar-animated"
                                                            role="progressbar" aria-valuenow="60" aria-valuemin="0"
                                                            aria-valuemax="100" style="height: 60%;">
                                                        </div>
                                                    </div>
                                                </div>
                                            </div>
                                            <div class="col-10">
                                                <img src="images/3P-phoenix-cad.svg" style="--color_fill: red;"
                                                    alt="Port-img-1" width="70%" />
                                            </div>
                                        </div>
                                        <div class="card-body">
                                            <p class="card-text">Pixels: <span id="port1_pix_count">10</span></p>
                                            <p class="card-text">Current: <span id="port1_current">452</span>mA</p>
                                            <a href="#" class="btn btn-primary">Test String</a>
                                        </div>
                                    </div>

                                    <div class="card col-3">
                                        <h5 class="card-title">Port 6</h5>
                                        <div class="row">
                                            <div class="col-2">
                                                <div class="container">
                                                    <div class="progress progress-bar-vertical">
                                                        <div class="progress-bar bg-success progress-bar-striped progress-bar-animated"
                                                            role="progressbar" aria-valuenow="60" aria-valuemin="0"
                                                            aria-valuemax="100" style="height: 60%;">
                                                        </div>
                                                    </div>
                                                </div>
                                            </div>
                                            <div class="col-10">
                                                <img src="images/3P-phoenix-cad.svg" style="--color_fill: red;"
                                                    alt="Port-img-1" width="70%" />
                                            </div>
                                        </div>
                                        <div class="card-body">
                                            <p class="card-text">Pixels: <span id="port1_pix_count">10</span></p>
                                            <p class="card-text">Current: <span id="port1_current">452</span>mA</p>
                                            <a href="#" class="btn btn-primary">Test String</a>
                                        </div>
                                    </div>

                                    <div class="card col-3">
                                        <h5 class="card-title">Port 7</h5>
                                        <div class="row">
                                            <div class="col-2">
                                                <div class="container">
                                                    <div class="progress progress-bar-vertical">
                                                        <div class="progress-bar bg-success progress-bar-striped progress-bar-animated"
                                                            role="progressbar" aria-valuenow="60" aria-valuemin="0"
                                                            aria-valuemax="100" style="height: 60%;">
                                                        </div>
                                                    </div>
                                                </div>
                                            </div>
                                            <div class="col-10">
                                                <img src="images/3P-phoenix-cad.svg" style="--color_fill: red;"
                                                    alt="Port-img-1" width="70%" />
                                            </div>
                                        </div>
                                        <div class="card-body">
                                            <p class="card-text">Pixels: <span id="port1_pix_count">10</span></p>
                                            <p class="card-text">Current: <span id="port1_current">452</span>mA</p>
                                            <a href="#" class="btn btn-primary">Test String</a>
                                        </div>
                                    </div>

                                    <div class="card col-3">
                                        <h5 class="card-title">Port 8</h5>
                                        <div class="row">
                                            <div class="col-2">
                                                <div class="container">
                                                    <div class="progress progress-bar-vertical">
                                                        <div class="progress-bar bg-success progress-bar-striped progress-bar-animated"
                                                            role="progressbar" aria-valuenow="60" aria-valuemin="0"
                                                            aria-valuemax="100" style="height: 60%;">
                                                        </div>
                                                    </div>
                                                </div>
                                            </div>
                                            <div class="col-10">
                                                <img src="images/3P-phoenix-cad.svg" style="--color_fill: red;"
                                                    alt="Port-img-1" width="70%" />
                                            </div>
                                        </div>
                                        <div class="card-body">
                                            <p class="card-text">Pixels: <span id="port1_pix_count">10</span></p>
                                            <p class="card-text">Current: <span id="port1_current">452</span>mA</p>
                                            <a href="#" class="btn btn-primary">Test String</a>
                                        </div>
                                    </div>

                                </div>
                            </div>
                        </div>


                    </div>
                    <br>

                    <div id="bank2">
                        <div class="row w-100">
                            <div class="bankinfo col-2 card">
                                <div class="card-body">
                                    <h5 class="card-title">Bank 2</h5>
                                    <div class="container">
                                        <div class="progress progress-bar-vertical">
                                            <div class="progress-bar bg-warning progress-bar-striped progress-bar-animated"
                                                role="progressbar" aria-valuenow="75" aria-valuemin="0"
                                                aria-valuemax="100" style="height: 750%;">
                                            </div>
                                        </div>
                                    </div>
                                    <p class="card-text">Total Current Draw:</p>
                                </div>
                            </div>
                            <div class="bankports col-10 bg-success">
                                <div class="row w-100 justify-content-evenly">

                                    <div class="card col-3">
                                        <h5 class="card-title">Port 9</h5>
                                        <div class="row">
                                            <div class="col-2">
                                                <div class="container">
                                                    <div class="progress progress-bar-vertical">
                                                        <div class="progress-bar bg-success progress-bar-striped progress-bar-animated"
                                                            role="progressbar" aria-valuenow="60" aria-valuemin="0"
                                                            aria-valuemax="100" style="height: 60%;">
                                                        </div>
                                                    </div>
                                                </div>
                                            </div>
                                            <div class="col-10">
                                                <img src="images/3P-phoenix-cad.svg" style="--color_fill: red;"
                                                    alt="Port-img-1" width="70%" />
                                            </div>
                                        </div>
                                        <div class="card-body">
                                            <p class="card-text">Pixels:</p>
                                            <p class="card-text">Current: </p>
                                            <a href="#" class="btn btn-primary">Test String</a>
                                        </div>
                                    </div>

                                    <div class="card col-3">
                                        <h5 class="card-title">Port 10</h5>
                                        <div class="row">
                                            <div class="col-2">
                                                <div class="container">
                                                    <div class="progress progress-bar-vertical">
                                                        <div class="progress-bar bg-warning progress-bar-striped progress-bar-animated"
                                                            role="progressbar" aria-valuenow="80" aria-valuemin="0"
                                                            aria-valuemax="100" style="height: 80%;">
                                                        </div>
                                                    </div>
                                                </div>
                                            </div>
                                            <div class="col-10">
                                                <img src="images/3P-phoenix-cad.svg" style="--color_fill: red;"
                                                    alt="Port-img-1" width="70%" />
                                            </div>
                                        </div>
                                        <div class="card-body">
                                            <p class="card-text">Pixels:</p>
                                            <p class="card-text">Current: </p>
                                            <a href="#" class="btn btn-primary">Test String</a>
                                        </div>
                                    </div>

                                    <div class="card col-3">
                                        <h5 class="card-title">Port 11</h5>
                                        <div class="row">
                                            <div class="col-2">
                                                <div class="container">
                                                    <div class="progress progress-bar-vertical">
                                                        <div class="progress-bar bg-success progress-bar-striped progress-bar-animated"
                                                            role="progressbar" aria-valuenow="30" aria-valuemin="0"
                                                            aria-valuemax="100" style="height: 30%;">
                                                        </div>
                                                    </div>
                                                </div>
                                            </div>
                                            <div class="col-10">
                                                <img src="images/3P-phoenix-cad.svg" style="--color_fill: red;"
                                                    alt="Port-img-1" width="70%" />
                                            </div>
                                        </div>
                                        <div class="card-body">
                                            <p class="card-text">Pixels:</p>
                                            <p class="card-text">Current: </p>
                                            <a href="#" class="btn btn-primary">Test String</a>
                                        </div>
                                    </div>

                                    <div class="card col-3">
                                        <h5 class="card-title">Port 12</h5>
                                        <div class="row">
                                            <div class="col-2">
                                                <div class="container">
                                                    <div class="progress progress-bar-vertical">
                                                        <div class="progress-bar bg-danger progress-bar-striped progress-bar-animated"
                                                            role="progressbar" aria-valuenow="95" aria-valuemin="0"
                                                            aria-valuemax="100" style="height: 95%;">
                                                        </div>
                                                    </div>
                                                </div>
                                            </div>
                                            <div class="col-10">
                                                <img src="images/3P-phoenix-cad.svg" style="--color_fill: red;"
                                                    alt="Port-img-1" width="70%" />
                                            </div>
                                        </div>
                                        <div class="card-body">
                                            <p class="card-text">Pixels:</p>
                                            <p class="card-text">Current: </p>
                                            <a href="#" class="btn btn-primary">Test String</a>
                                        </div>
                                    </div>


                                    <div class="card col-3">
                                        <h5 class="card-title">Port 13</h5>
                                        <div class="row">
                                            <div class="col-2">
                                                <div class="container">
                                                    <div class="progress progress-bar-vertical">
                                                        <div class="progress-bar bg-success progress-bar-striped progress-bar-animated"
                                                            role="progressbar" aria-valuenow="60" aria-valuemin="0"
                                                            aria-valuemax="100" style="height: 60%;">
                                                        </div>
                                                    </div>
                                                </div>
                                            </div>
                                            <div class="col-10">
                                                <img src="images/3P-phoenix-cad.svg" style="--color_fill: red;"
                                                    alt="Port-img-1" width="70%" />
                                            </div>
                                        </div>
                                        <div class="card-body">
                                            <p class="card-text">Pixels:</p>
                                            <p class="card-text">Current: </p>
                                            <a href="#" class="btn btn-primary">Test String</a>
                                        </div>
                                    </div>

                                    <div class="card col-3">
                                        <h5 class="card-title">Port 14</h5>
                                        <div class="row">
                                            <div class="col-2">
                                                <div class="container">
                                                    <div class="progress progress-bar-vertical">
                                                        <div class="progress-bar bg-success progress-bar-striped progress-bar-animated"
                                                            role="progressbar" aria-valuenow="60" aria-valuemin="0"
                                                            aria-valuemax="100" style="height: 60%;">
                                                        </div>
                                                    </div>
                                                </div>
                                            </div>
                                            <div class="col-10">
                                                <img src="images/3P-phoenix-cad.svg" style="--color_fill: red;"
                                                    alt="Port-img-1" width="70%" />
                                            </div>
                                        </div>
                                        <div class="card-body">
                                            <p class="card-text">Pixels:</p>
                                            <p class="card-text">Current: </p>
                                            <a href="#" class="btn btn-primary">Test String</a>
                                        </div>
                                    </div>

                                    <div class="card col-3">
                                        <h5 class="card-title">Port 15</h5>
                                        <div class="row">
                                            <div class="col-2">
                                                <div class="container">
                                                    <div class="progress progress-bar-vertical">
                                                        <div class="progress-bar bg-success progress-bar-striped progress-bar-animated"
                                                            role="progressbar" aria-valuenow="60" aria-valuemin="0"
                                                            aria-valuemax="100" style="height: 60%;">
                                                        </div>
                                                    </div>
                                                </div>
                                            </div>
                                            <div class="col-10">
                                                <img src="images/3P-phoenix-cad.svg" style="--color_fill: red;"
                                                    alt="Port-img-1" width="70%" />
                                            </div>
                                        </div>
                                        <div class="card-body">
                                            <p class="card-text">Pixels:</p>
                                            <p class="card-text">Current: </p>
                                            <a href="#" class="btn btn-primary">Test String</a>
                                        </div>
                                    </div>

                                    <div class="card col-3">
                                        <h5 class="card-title">Port 16</h5>
                                        <div class="row">
                                            <div class="col-2">
                                                <div class="container">
                                                    <div class="progress progress-bar-vertical">
                                                        <div class="progress-bar bg-success progress-bar-striped progress-bar-animated"
                                                            role="progressbar" aria-valuenow="60" aria-valuemin="0"
                                                            aria-valuemax="100" style="height: 60%;">
                                                        </div>
                                                    </div>
                                                </div>
                                            </div>
                                            <div class="col-10">
                                                <img src="images/3P-phoenix-cad.svg" style="--color_fill: red;"
                                                    alt="Port-img-1" width="70%" />
                                            </div>
                                        </div>
                                        <div class="card-body">
                                            <p class="card-text">Pixels:</p>
                                            <p class="card-text">Current: </p>
                                            <a href="#" class="btn btn-primary">Test String</a>
                                        </div>
                                    </div>

                                </div>
                            </div>
                        </div>


                    </div>

                </div>
                <!-- End Example Code -->

            </div>
        </div>
        <?php include 'common/footer.inc'; ?>
    </div>
</body>

</html>