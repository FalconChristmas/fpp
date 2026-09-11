<?php
/*
 * The privacy table: one row per DISCLOSURE, shared by the setup wizard and the
 * Privacy settings page.
 *
 * Both pages ask the same questions about the same settings, so they have to ask
 * them the same way -- same rows, same wording, same "goes to", same help text,
 * and the same ladder rules. Two copies would drift, and the copy people reach
 * later (Settings) is the one where they change their mind, so it is the worse
 * one to have go stale.
 *
 * Rows and settings do NOT line up one to one, which is the whole reason this is
 * a table of disclosures rather than a settings group:
 *
 *   - ShareCrashData is a single ordered value, so its three rows are a ladder.
 *     "include configuration and logs" necessarily includes "your settings",
 *     which necessarily includes the stack.
 *   - The cape serial rides on the logo request, so denying the logo denies it.
 *   - The e-mail field belongs to the crash rows and is pointless until they are
 *     allowed, so it lives inside the ladder rather than after the table.
 *
 * The behaviour lives in js/fpp-privacy-table.js. This file is the markup and the
 * data it needs; privacyDisclosures.inc is the text.
 */

require_once __DIR__ . '/privacyDisclosures.inc';

/**
 * Row definitions: id, label, who it goes to, sub-caption, indent depth.
 *
 * Order matters and is not alphabetical -- the crash rows sit last because the
 * e-mail field hangs off the deepest of them and looks orphaned mid-table.
 */
function privacyTableRows()
{
    return array(
        array('stats', 'Share usage statistics',
            'FPP &amp; xLights developers',
            'Tells us which platforms and features to keep supporting, so yours is '
            . 'less likely to be dropped. '
            . "<a href='#' onClick='PreviewStatistics(); return false;'>Preview data</a>.", 0),
        array('logos', 'Load cape logos from vendor website', 'Cape vendor', '', 0),
        array('serial', '&hellip; and include your cape\'s serial number', '', '', 1),
        array('crash', 'Send crash reports',
            'FPP &amp; xLights developers,<br>AI service',
            'The only way most crashes are ever found.', 0),
        array('crash-settings', '&hellip; and include your settings', '', '', 1),
        array('crash-config', '&hellip; and include configuration and logs', '', '', 1),
    );
}

/**
 * Emit the table.
 *
 * @param string $footNote Text for the footer's label cell. The two pages point
 *                         at each other, so neither can hardcode the other's
 *                         wording.
 */
function PrintPrivacyTable($footNote = '')
{
    $disclosures = privacyDisclosures();

    echo "<div class='table-responsive'>\n";
    echo "<table class='privacyTable' id='privacyTable'>\n";
    echo "<thead><tr><th></th><th class='privacyGoesTo'>Goes to</th>"
        . "<th class='privacyChoice'>Allow</th><th class='privacyChoice'>Deny</th></tr></thead>\n";
    echo "<tbody>\n";

    foreach (privacyTableRows() as $r) {
        list($id, $label, $goesTo, $sub, $depth) = $r;
        $indent = $depth ? " style='padding-left:2.2em'" : '';
        echo "<tr class='privacyRow' data-row='$id' data-depth='$depth'>";
        echo "<td$indent><b>$label</b>";
        echo " <i class='fas fa-question-circle privacyHelp' data-row='$id' title='Explain what this sends'></i>";
        if ($sub != '') {
            echo "<div class='smallText text-muted'>$sub</div>";
        }
        echo "</td>";
        echo "<td class='privacyGoesTo smallText text-muted'>$goesTo</td>";
        echo "<td class='privacyChoice'><label><input type='radio' name='privacy_$id' value='allow'></label></td>";
        echo "<td class='privacyChoice'><label><input type='radio' name='privacy_$id' value='deny'></label></td>";
        echo "</tr>";

        if ($id === 'crash-config') {
            echo "<tr class='privacyRow d-none' id='emailRow'>";
            echo "<td style='padding-left:2.2em'><b>E-mail address</b> <span class='text-muted'>(optional)</span>";
            echo " <i class='fas fa-question-circle privacyHelp' data-row='email' title='Explain what this sends'></i></td>";
            echo "<td colspan='3'><input type='text' id='emailAddress' size='40' maxlength='256' placeholder='you@example.com'></td>";
            echo "</tr>";
        }
    }

    echo "</tbody>\n<tfoot><tr class='privacyAllRow'>";
    echo "<td class='text-muted smallText'>$footNote</td><td></td>";
    echo "<td class='privacyChoice'><input type='button' class='buttons' value='Allow all' onClick='fppPrivacyTable.setAll(\"allow\");'></td>";
    echo "<td class='privacyChoice'><input type='button' class='buttons' value='Deny all' onClick='fppPrivacyTable.setAll(\"deny\");'></td>";
    echo "</tr></tfoot>\n</table>\n</div>\n";
}

/**
 * Everything the JS half needs, as one object.
 *
 * The current values come from $settings (effective values, defaults included);
 * `answered` comes from the settings FILE, because those are different questions
 * -- "what is this box doing" versus "did anybody ever choose it". Using
 * $settings for the second would make every setting look answered.
 */
function PrivacyTableConfig($priorOptIn)
{
    $keys = array('statsPublish', 'ShareCrashData', 'FetchVendorLogos', 'SendVendorSerial', 'emailAddress');

    $current = array();
    $answered = array();
    foreach ($keys as $k) {
        $current[$k] = (string) (isset($GLOBALS['settings'][$k]) ? $GLOBALS['settings'][$k] : '');
        if (ReadSettingFromFile($k) !== false) {
            $answered[] = $k;
        }
    }

    return array(
        'priorOptIn' => (bool) $priorOptIn,
        'current' => $current,
        'answered' => $answered,
        'help' => privacyDisclosures(),
    );
}
