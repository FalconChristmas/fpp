<?php
/*
 * Recording consent.
 *
 * The choices themselves are ordinary settings and are saved the ordinary way,
 * through PUT /api/settings/<key>. This endpoint records the ACT: which
 * disclosures were on the screen, when, and where the user was standing when
 * they answered.
 *
 * It is a separate call rather than a side effect of writing those settings,
 * because the settings API is also how a script or the command line sets them,
 * and a value set that way has no affirmative action behind it. Only a page that
 * actually showed someone the disclosure table gets to say so.
 *
 * Everything except the date is stamped here rather than taken from the caller:
 * the version, the text hash and the device UUID are facts about this player and
 * this build, and a record whose contents the browser could choose would prove
 * nothing.
 */

require_once __DIR__ . '/../../privacyConsent.inc';

/**
 * @route POST /api/privacy/consent
 *
 * Body: {"via": "wizard"|"settings",
 *        "date": "<ISO 8601, the browser's clock>",
 *        "settings": {"statsPublish": "Enabled", ...}}
 */
function RecordConsent()
{
    $body = json_decode(file_get_contents('php://input'), true);
    if (!is_array($body) || !isset($body['settings']) || !is_array($body['settings'])) {
        return json(array('Status' => 'Error', 'Message' => 'expected a settings object'));
    }

    // The only two surfaces that show the disclosures. Anything else claiming to
    // be one is exactly what this endpoint exists to keep out of the record.
    $via = isset($body['via']) ? $body['via'] : '';
    if ($via !== 'wizard' && $via !== 'settings') {
        return json(array('Status' => 'Error', 'Message' => 'unknown consent origin'));
    }

    $date = isset($body['date']) ? $body['date'] : '';
    if (!RecordPrivacyConsent($body['settings'], $via, $date)) {
        return json(array('Status' => 'Error', 'Message' => 'could not write the consent record'));
    }

    return json(array('Status' => 'OK', 'consent' => ReadPrivacyConsent()));
}

/**
 * @route GET /api/privacy/consent
 *
 * What this player would be able to show if asked. Also reports any setting
 * whose live value no longer matches what was consented to.
 */
function GetConsent()
{
    $rec = ReadPrivacyConsent();
    return json(array(
        'consent' => $rec,
        'currentVersion' => PRIVACY_CONSENT_VERSION,
        'currentTextHash' => privacyConsentTextHash(),
        'shortfall' => PrivacyConsentShortfall(),
        'drift' => PrivacyConsentDrift(),
    ));
}
