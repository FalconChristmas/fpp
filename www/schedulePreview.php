<?
require_once 'config.php';

$endTimes = array();
$depth = 0;

$colors = array(
    array(
        GetSettingValue('tableColorPair1A', '00FFFF'),
        GetSettingValue('tableColorPair1B', 'ADD8E6')
    ),
    array(
        GetSettingValue('tableColorPair2A', '008000'),
        GetSettingValue('tableColorPair2B', '808000')
    ),
    array(
        GetSettingValue('tableColorPair3A', '808080'),
        GetSettingValue('tableColorPair3B', 'C0C0C0')
    ),
    array(
        GetSettingValue('tableColorPair4A', 'A52A2A'),
        GetSettingValue('tableColorPair4B', 'FFFF00')
    ),
);
$colorIndexes = array(0, 0, 0, 0);
$depthsInUse = array();
$priorities = array();
$maxDepth = 0;

$json = file_get_contents('http://localhost:32322/fppd/schedule');
$data = json_decode($json, true);

function checkIfHoliday($item, $wrap = false)
{
    global $data;
    global $settings;

    $holiday = '';
    if (
        ($data['schedule']['entries'][$item['id']]['startDateInt'] == $item['startDateInt']) &&
        (!preg_match('/^[0-9]/', $data['schedule']['entries'][$item['id']]['startDate']))
    ) {
        $holiday = $data['schedule']['entries'][$item['id']]['startDate'];
    }
    if (
        ($data['schedule']['entries'][$item['id']]['endDateInt'] == $item['endDateInt']) &&
        (!preg_match('/^[0-9]/', $data['schedule']['entries'][$item['id']]['endDate']))
    ) {
        $holiday = $data['schedule']['entries'][$item['id']]['endDate'];
    }

    if (
        ($holiday != '') &&
        (isset($settings['locale'])) &&
        (isset($settings['locale']['holidays']))
    ) {
        for ($h = 0; $h < count($settings['locale']['holidays']); $h++) {
            if ($holiday == $settings['locale']['holidays'][$h]['shortName']) {
                if ($wrap) {
                    return ' (' . $settings['locale']['holidays'][$h]['name'] . ')';
                } else {
                    return $settings['locale']['holidays'][$h]['name'];
                }
            }

        }
    }

    return $holiday;
}

function getItemInfo($item)
{
    global $data;

    if (!isset($data['schedule']['entries'][$item['id']])) {
        return "ERROR: Unable to find schedule entry with id: " . $item['id'];
    }

    $sch = $data['schedule']['entries'][$item['id']];

    $info = "";

    $info .= '<b>Start Date:</b> ';
    if (preg_match('/^[0-9]/', $sch['startDate'])) {
        $info .= $sch['startDate'];
    } else {
        $info .= checkIfHoliday($item);
    }
    $info .= '<br>';

    $info .= '<b>End Date:</b> ';
    if (preg_match('/^[0-9]/', $sch['endDate'])) {
        $info .= $sch['endDate'];
    } else {
        $info .= checkIfHoliday($item);
    }
    $info .= '<br>';

    $info .= '<b>Start Time:</b> ' . $sch['startTimeStr'];
    if ($sch['startTimeOffset'] != 0) {
        $info .= (($sch['startTimeOffset'] > 0) ? ' +' : ' ') . $sch['startTimeOffset'] . ' ' . (($sch['startTimeOffset'] > 1) ? 'minutes' : 'minute');
    }

    $info .= '<br>';

    $info .= '<b>End Time:</b> ' . $sch['endTimeStr'];
    if ($sch['endTimeOffset'] != 0) {
        $info .= (($sch['endTimeOffset'] > 0) ? ' +' : ' ') . $sch['endTimeOffset'] . ' ' . (($sch['endTimeOffset'] > 1) ? 'minutes' : 'minute');
    }

    $info .= '<br>';

    $info .= '<b>Days:</b> ' . $sch['dayStr'] . '<br>';
    $info .= '<b>Stop:</b> ' . $sch['stopTypeStr'] . '<br>';

    $info .= '<b>Repeat:</b> ';
    if ($sch['repeat']) {
        $info .= 'Immediate';
    } else if ($sch['repeatInterval']) {
        $mins = $sch['repeatInterval'] / 60;
        $info .= "Every $mins minutes";
    } else {
        $info .= 'None';
    }
    $info .= '<br>';
    $info .= '<b>Priority:</b> ' . $item['priority'] . '<br>';
    return $info;
}

function showPlaylistEnds($currTime = 0)
{
    global $endTimes;
    global $depth;
    global $colors;
    global $colorIndexes;
    global $maxDepth;
    global $data;
    global $depthsInUse;
    global $priorities;

    $deleteEnds = array();

    foreach ($endTimes as $eTime => $endItems) {
        if (($currTime == 0) || ($eTime <= $currTime)) {
            foreach ($endItems as $endItem) {
                printf("<tr style='background-color: #%s;'>", $colors[$endItem["previewDepth"]][$endItem["colorIndex"]]);
                for ($i = 0; $i < $endItem["previewDepth"]; $i++) {
                    if (isset($depthsInUse[$i])) {
                        printf("<td width='20px' class='borderLeft' style='background-color: #%s;'>&nbsp;</td>", $colors[$i][$colorIndexes[$i]]);
                    } else {
                        printf("<td width='20px' style='background-color: #000000;'>&nbsp;</td>");
                    }
                }

                printf("<td width='20px' class='borderLeft'>&nbsp;</td>");

                for ($j = $endItem["previewDepth"]; $j < $maxDepth; $j++) {
                    if (isset($depthsInUse[$j + 1])) {
                        printf(
                            "<td width='20px' style='color: #%s; font-weight: bold; text-align: center;'>|</td>",
                            $colors[$j + 1][$colorIndexes[$j + 1]]
                        );
                    } else {
                        printf("<td width='20px'>&nbsp;</td>");
                    }

                }
                $style = "";
                if ($depthsInUse[$endItem["previewDepth"]] == 2) {
                    $style = " style='color: red;'";
                }
                printf(
                    "<td>%s%s</td><td>&nbsp;-&nbsp;</td><td%s>End Playing</td><td>&nbsp;-&nbsp;</td><td%s>%s (%s Stop)</td><td></td></tr>\n<tr>",
                    $endItem["endTimeStr"],
                    checkIfHoliday($endItem, true),
                    $style,
                    $style,
                    $endItem["args"][0],
                    $data["schedule"]["entries"][$endItem["id"]]["stopTypeStr"]
                );

                unset($depthsInUse[$endItem["previewDepth"]]);
                unset($priorities[$endItem["previewDepth"]]);
            }

            array_push($deleteEnds, $eTime);
        }
    }

    // Delete any handled ends
    foreach ($deleteEnds as $eTime) {
        unset($endTimes[$eTime]);
    }
}

/////////////////////////////////////////////////////////////////////////////

// determine max depth
foreach ($data["schedule"]["items"] as $item) {
    $deleteEnds = array();

    foreach ($endTimes as $eTime => $endItems) {
        if ($eTime < $item["startTime"]) {
            $depth--;
            array_push($deleteEnds, $eTime);
        }
    }
    foreach ($deleteEnds as $eTime) {
        unset($endTimes[$eTime]);
    }

    if ($item["command"] == "Start Playlist") {
        if (isset($endTimes[$item["endTime"]])) {
            array_push($endTimes[$item["endTime"]], $item);
        } else {
            $items = array();
            array_push($items, $item);
            $endTimes[$item["endTime"]] = $items;
        }

        ksort($endTimes);

        if ($depth > $maxDepth) {
            $maxDepth = $depth;
        }

        $depth++;
    } else {
        if ($depth > $maxDepth) {
            $maxDepth = $depth;
        }

    }
}

// Reset
$depth = 0;
$endTimes = array();

if ($data["schedule"]["enabled"] == 0) {
    echo "<center><font color='red'><b>Scheduler is currently disabled.</b></font></center>\n";
}

if (count($data["schedule"]["items"]) == 0) {
    echo "<center><font color='red'><b>Nothing Scheduled.</b></font></center>\n";
    exit;
}

echo "<table class='fppSelectableRowTable schedulePreviewTable fppStickyModalTheadTable' border=0 cellpadding=0 cellspacing=0 style='color: #000000;'>\n";
echo "<thead><tr>";
for ($j = -1; $j < $maxDepth; $j++) {
    echo "<td>&nbsp;</td>";
}
echo "<th>Time</th><th></th><th>Action</th><th></th><th>Args</th><th>Sch Info</th></tr>";
echo "</thead><tbody>";

foreach ($data["schedule"]["items"] as $item) {

    showPlaylistEnds($item["startTime"]);
    $depth = 0;
    while (isset($depthsInUse[$depth])) {
        $depth++;
    }


    $colorIndex = $colorIndexes[$depth];
    $colorIndex++;
    if ($colorIndex > 1) {
        $colorIndex = 0;
    }

    $colorIndexes[$depth] = $colorIndex;

    printf("<tr style='background-color: #%s;'>", $colors[$depth][$colorIndex]);

    for ($i = 0; $i < $depth; $i++) {
        printf("<td width='20px' class='borderLeft' style='background-color: #%s;'>&nbsp;</td>", $colors[$i][$colorIndexes[$i]]);
    }

    if ($item["command"] == "Start Playlist") {
        $item["colorIndex"] = $colorIndex;
        $item["previewDepth"] = $depth;

        if (isset($endTimes[$item["endTime"]])) {
            array_unshift($endTimes[$item["endTime"]], $item);
        } else {
            $items = array($item);
            $endTimes[$item["endTime"]] = $items;
        }

        ksort($endTimes);
        $priorities[$depth] = $item['priority'];
        $style = "";
        if ($depth == 0 || $item['priority'] < $priorities[$depth - 1]) {
            $depthsInUse[$depth] = 1;
        } else {
            $depthsInUse[$depth] = 2;
            $style = " style='color: red;'";
        }
        printf("<td width='20px' class='borderLeft'>&nbsp;</td>");
        for ($j = $depth; $j < $maxDepth; $j++) {
            printf("<td width='20px'>&nbsp;</td>");
        }
        if ($depth == 0 || $item['priority'] < $priorities[$depth - 1]) {
            printf(
                "<td>%s%s</td><td>&nbsp;-&nbsp;</td><td>Start Playing</td><td>&nbsp;-&nbsp;</td><td>%s (%s w/ %s Stop)</td>",
                $item["startTimeStr"],
                checkIfHoliday($item, true),
                $item["args"][0],
                $data["schedule"]["entries"][$item["id"]]["repeat"] == 1 ? "Repeating" : "Non-Repeating",
                $data["schedule"]["entries"][$item["id"]]["stopTypeStr"]
            );
        } else {
            printf(
                "<td>%s%s</td><td>&nbsp;-&nbsp;</td><td style='color: red;'>Skip Start Playing</td><td>&nbsp;-&nbsp;</td><td style='color: red;'>%s - Lower Priority</td>",
                $item["startTimeStr"],
                checkIfHoliday($item, true),
                $item["args"][0]
            );
        }
    } else {
        printf("<td width='20px' class='borderLeft'>&nbsp;</td>");

        for ($j = $depth; $j < $maxDepth; $j++) {
            //            printf("<td width='20px'>&nbsp;</td>");
            if (isset($depthsInUse[$j + 1])) {
                printf(
                    "<td width='20px' style='color: #%s; font-weight: bold; text-align: center;'>|</td>",
                    $colors[$j + 1][$colorIndexes[$j + 1]]
                );
            } else {
                printf("<td width='20px'>&nbsp;</td>");
            }

        }

        printf(
            "<td>%s%s</td><td>&nbsp;-&nbsp;</th><td>%s</td><td>&nbsp;-&nbsp;</th><td>%s</td>",
            $item["startTimeStr"],
            checkIfHoliday($item, true),
            $item["command"],
            join(' | ', $item["args"])
        );
    }

    echo "<td><span data-bs-toggle='tooltip' data-bs-html='true' data-bs-placement='auto' data-bs-title=\"" . getItemInfo($item) . "\"><img src='images/redesign/help-icon.svg' class='icon-help'></span></td>";

    echo "</tr>\n";
}
showPlaylistEnds();
?>
</tbody>
</table>
<script>
    SetupToolTips();

</script>