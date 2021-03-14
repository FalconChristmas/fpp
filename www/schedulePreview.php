<?
require_once('config.php');

$endTimes = array();
$depth = 0;

$colors = array(
    array(GetSettingValue('tableColorPair1A', '00FFFF'),
          GetSettingValue('tableColorPair1B', 'ADD8E6')),
    array(GetSettingValue('tableColorPair2A', '008000'),
          GetSettingValue('tableColorPair2B', '808000')),
    array(GetSettingValue('tableColorPair3A', '808080'),
          GetSettingValue('tableColorPair3B', 'C0C0C0')),
    array(GetSettingValue('tableColorPair4A', 'A52A2A'),
          GetSettingValue('tableColorPair4B', 'FFFF00'))
);
$colorIndexes = array( 0, 0, 0, 0 );
$depthsInUse = array();
$maxDepth = 0;

$json = file_get_contents('http://localhost:32322/fppd/schedule');
$data = json_decode($json, true);

function showPlaylistEnds($currTime = 0) {
    global $endTimes;
    global $depth;
    global $colors;
    global $colorIndexes;
    global $maxDepth;
    global $data;
    global $depthsInUse;

    $deleteEnds = array();

    foreach ($endTimes as $eTime => $endItem) {
        if (($currTime == 0) || ($eTime <= $currTime)) {
            printf("<tr style='background-color: #%s;'>", $colors[$endItem["previewDepth"]][$endItem["colorIndex"]]);
            for ($i = 0; $i < $endItem["previewDepth"]; $i++) {
                if (isset($depthsInUse[$i])) {
                    printf("<td width='20px' class='borderLeft' style='background-color: #%s;'>&nbsp;</td>", $colors[$i][$colorIndexes[$i]]);
                } else {
                    printf("<td width='20px' style='background-color: #000000;'>&nbsp;</td>");
                }
            }

            printf("<td width='20px' class='borderLeft borderBottom'>&nbsp;</td>");

            for ($j = $endItem["previewDepth"]; $j < $maxDepth; $j++) {
                if (isset($depthsInUse[$j+1]))
                    printf("<td width='20px' class='borderBottom' style='color: #%s; font-weight: bold; text-align: center;'>|</td>",
                        $colors[$j+1][$colorIndexes[$j+1]]);
                else
                    printf("<td width='20px' class='borderBottom'>&nbsp;</td>");
            }

            printf("<td>%s</td><th>&nbsp;-&nbsp;</th><td>End Playing</td><th>&nbsp;-&nbsp;</th><td>%s (%s Stop)</td></tr>\n<tr>",
                $endItem["endTimeStr"],
                $endItem["args"][0],
                $data["schedule"]["entries"][$endItem["id"]]["stopTypeStr"]
                );
            array_push($deleteEnds, $eTime);
            unset($depthsInUse[$endItem["previewDepth"]]);
        }
    }

    // Delete any handled ends
    foreach ($deleteEnds as $eTime) {
        unset($endTimes[$eTime]);
    }
}

// determine max depth
foreach ($data["schedule"]["items"] as $item) {
    $deleteEnds = array();

    foreach ($endTimes as $eTime => $endItem) {
        if ($eTime < $item["startTime"]) {
            $depth--;
            array_push($deleteEnds, $eTime);
        }
    }
    foreach ($deleteEnds as $eTime) {
        unset($endTimes[$eTime]);
    }

    if ($item["command"] == "Start Playlist") {
        $endTimes[$item["endTime"]] = $item;
        ksort($endTimes);

        if ($depth > $maxDepth)
            $maxDepth = $depth;

        $depth++;
    } else {
        if ($depth > $maxDepth)
            $maxDepth = $depth;
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

echo "<table class='fppSelectableRowTable schedulePreviewTable' border=0 cellpadding=0 cellspacing=0 style='color: #000000;'>\n";
echo "<thead><tr>";
for ($j = -1; $j < $maxDepth; $j++) {
    echo "<td>&nbsp;</td>";
}
echo "<th>Time</th><th></th><th>Action</th><th></th><th>Args</th></tr>";
echo "</thead><tbody>";


foreach ($data["schedule"]["items"] as $item) {
    showPlaylistEnds($item["startTime"]);

    $depth = 0;
    while (isset($depthsInUse[$depth]))
        $depth++;

    $colorIndex = $colorIndexes[$depth];
    $colorIndex++;
    if ($colorIndex > 1)
        $colorIndex = 0;
    $colorIndexes[$depth] = $colorIndex;

    printf("<tr style='background-color: #%s;'>", $colors[$depth][$colorIndex]);

    for ($i = 0; $i < $depth; $i++)
        printf("<td width='20px' class='borderLeft' style='background-color: #%s;'>&nbsp;</td>", $colors[$i][$colorIndexes[$i]]);

    if ($item["command"] == "Start Playlist") {
        $item["colorIndex"] = $colorIndex;
        $item["previewDepth"] = $depth;
        $depthsInUse[$depth] = 1;
        $endTimes[$item["endTime"]] = $item;
        ksort($endTimes);
        printf("<td width='20px' class='borderTop borderLeft'>&nbsp;</td>");

        for ($j = $depth; $j < $maxDepth; $j++) {
            printf("<td width='20px' class='borderTop'>&nbsp;</td>");
        }

        printf("<td>%s</td><th>&nbsp;-&nbsp;</th><td>Start Playing</td><th>&nbsp;-&nbsp;</th><td>%s (%s w/ %s Stop)</td>",
            $item["startTimeStr"],
            $item["args"][0],
            $data["schedule"]["entries"][$item["id"]]["repeat"] == 1 ?
                "Repeating" : "Non-Repeating",
            $data["schedule"]["entries"][$endItem["id"]]["stopTypeStr"]
            );
    } else {
        printf("<td width='20px' class='borderTop borderLeft borderBottom'>&nbsp;</td>");

        for ($j = $depth; $j < $maxDepth; $j++) {
//            printf("<td width='20px' class='borderTop borderBottom'>&nbsp;</td>");
            if (isset($depthsInUse[$j+1]))
                printf("<td width='20px' class='borderTop borderBottom' style='color: #%s; font-weight: bold; text-align: center;'>|</td>",
                    $colors[$j+1][$colorIndexes[$j+1]]);
            else
                printf("<td width='20px' class='borderTop borderBottom'>&nbsp;</td>");
        }

        printf("<td>%s</td><th>&nbsp;-&nbsp;</th><td>%s</td><th>&nbsp;-&nbsp;</th><td>%s</td>",
            $item["startTimeStr"],
            $item["command"],
            join(' | ', $item["args"]));
    }

    echo "</tr>\n";
}
showPlaylistEnds();
?>
    </tbody>
</table>

