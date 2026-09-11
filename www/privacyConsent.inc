<?php
/*
 * The consent record: what we can show if someone asks us to prove a choice was
 * made.
 *
 * GDPR Article 7(1) puts the burden on the controller to DEMONSTRATE consent,
 * and the ICO's reading of it is a list: who, when, what they consented to, how
 * they did it, and what they were shown at the time. A bare "statsPublish =
 * Enabled" in the settings file answers none of those -- it is a value, not an
 * act. This file is the act.
 *
 * Article 7 only binds where consent is actually the lawful basis, which here
 * means the prior-opt-in jurisdictions (see jurisdiction.inc). Elsewhere level 1
 * runs on legitimate interests and the duty is disclosure plus an easy
 * objection, not a record. We keep the record everywhere anyway: it costs one
 * settings key, and a player can move between jurisdictions.
 *
 * Shape, stored as JSON in the 'privacyConsent' setting (WriteSettingToFile
 * writes a JSON object unquoted and reads it back byte-identical -- 'locale' and
 * 'cape-info' already rely on that):
 *
 *   {
 *     "version": 1,
 *     "textHash": "9f86d081884c7d65",
 *     "uuid": "6ba176e7-da7f-49f4-8b27-edb5bd9ff616",
 *     "settings": {
 *       "statsPublish": { "value": "Enabled", "date": "...", "via": "wizard" },
 *       ...
 *     }
 *   }
 *
 * Each of the four choices carries its OWN date and origin, which is the part
 * that is easy to get wrong. A single global timestamp updated on any change
 * would say, of a user who allowed statistics in March and changed their crash
 * level in September, that they consented to statistics in September. That is
 * not a weaker record than none, it is a false one -- and it is the timestamp we
 * would then be uploading with the statistics themselves.
 *
 * Recording the VALUE alongside the stamp is what makes the record self
 * contained: "shown version N (hash H), on DATE, via VIA, they chose VALUE".
 * It also makes drift visible. Settings are writable over the API and from the
 * command line, so a live value that no longer matches the recorded one is a
 * value no affirmative act stands behind -- see PrivacyConsentDrift().
 */

require_once __DIR__ . '/privacyDisclosures.inc';
require_once __DIR__ . '/privacyTable.inc';

// The settings that are consent choices. Purpose ids ARE the setting names:
// they map one to one, and a translation table between them would be one more
// thing to drift.
function privacyConsentSettings()
{
    return array('statsPublish', 'FetchVendorLogos', 'SendVendorSerial', 'ShareCrashData');
}

/**
 * Fingerprint of the disclosure text as it stands right now.
 *
 * PRIVACY_CONSENT_VERSION deliberately is not bumped for rewording, so the
 * version alone cannot prove which words a user actually read. The hash can:
 * same text, same hash. It is not a substitute for the version -- the version
 * decides who gets re-prompted, the hash records what was on the screen.
 */
function privacyConsentTextHash()
{
    // Everything the user actually reads before answering, which is BOTH files:
    // privacyDisclosures() is the detail behind each question mark, and
    // privacyTableRows() is the row labels, the "goes to" column and the
    // sub-captions -- the words most people will read and the only words some of
    // them will. Hashing only the bodies would have let the visible half of the
    // table be rewritten without the record noticing.
    $text = privacyDisclosures();
    ksort($text);
    return substr(hash('sha256', json_encode(array($text, privacyTableRows()))), 0, 16);
}

/**
 * The stored record, or null if there isn't one.
 */
function ReadPrivacyConsent()
{
    $raw = ReadSettingFromFile('privacyConsent');
    if ($raw === false || $raw === '') {
        return null;
    }
    $rec = json_decode($raw, true);
    return is_array($rec) ? $rec : null;
}

/**
 * Stamp consent for the settings named in $values.
 *
 * Only the settings passed in are stamped; the rest of the record is left alone,
 * because a user changing one row on the Privacy page has not re-affirmed the
 * others.
 *
 * @param array  $values setting name => the value consented to
 * @param string $via    where the affirmative action happened: 'wizard' or
 *                       'settings'. This is the "how", and it is the field that
 *                       distinguishes a choice from a value that arrived over
 *                       the API with nobody looking at a disclosure.
 * @param string $date   ISO 8601. The browser's clock, passed in by the caller:
 *                       on a first boot the player may have no NTP yet and can
 *                       be years out, while the machine being used to set it up
 *                       is almost always right. Falls back to ours.
 */
function RecordPrivacyConsent($values, $via, $date = '')
{
    $rec = ReadPrivacyConsent();
    if ($rec === null) {
        $rec = array('settings' => array());
    }
    if (!isset($rec['settings']) || !is_array($rec['settings'])) {
        $rec['settings'] = array();
    }

    if ($date === '' || !preg_match('/^\d{4}-\d{2}-\d{2}T/', $date)) {
        $date = gmdate('Y-m-d\TH:i:s\Z');
    }

    $allowed = privacyConsentSettings();
    foreach ($values as $key => $value) {
        if (!in_array($key, $allowed, true)) {
            continue;
        }
        $rec['settings'][$key] = array(
            'value' => '' . $value,
            'date' => $date,
            'via' => $via,
        );
    }

    $rec['version'] = PRIVACY_CONSENT_VERSION;
    $rec['textHash'] = privacyConsentTextHash();
    // Binds the record to the device it was given on. Without this a settings
    // backup restored onto a second player carries the first player's consent,
    // and its owner is never asked anything -- see MissingSetupSettings().
    $rec['uuid'] = getSystemUUID();

    return WriteSettingToFile('privacyConsent', json_encode($rec));
}

/**
 * Why the record does not cover the current state, or '' if it does.
 *
 * Returned rather than a bool so the caller can say which of these it is: they
 * mean different things, and only some of them should re-prompt.
 */
function PrivacyConsentShortfall()
{
    $rec = ReadPrivacyConsent();
    if ($rec === null) {
        return 'absent';
    }
    if (!isset($rec['version']) || intval($rec['version']) < PRIVACY_CONSENT_VERSION) {
        // Consented to something FPP no longer says.
        return 'stale-version';
    }
    // A record made on another device, restored here in a backup. The choices in
    // it were somebody else's, made on a screen this device's owner never saw.
    if (isset($rec['uuid']) && $rec['uuid'] !== '' && $rec['uuid'] !== getSystemUUID()) {
        return 'other-device';
    }
    foreach (privacyConsentSettings() as $key) {
        if (!isset($rec['settings'][$key]['date'])) {
            return 'incomplete';
        }
    }
    return '';
}

/**
 * Settings whose live value no longer matches what was consented to.
 *
 * Not used to re-prompt -- changing a setting over the API is a legitimate thing
 * to do and this is a hobbyist appliance, not a managed fleet. It exists so the
 * mismatch is answerable rather than invisible if it is ever asked about.
 */
function PrivacyConsentDrift()
{
    $rec = ReadPrivacyConsent();
    if ($rec === null) {
        return privacyConsentSettings();
    }
    $drifted = array();
    foreach (privacyConsentSettings() as $key) {
        $live = ReadSettingFromFile($key);
        if ($live === false) {
            continue;
        }
        if (!isset($rec['settings'][$key]['value']) ||
            '' . $rec['settings'][$key]['value'] !== '' . $live) {
            $drifted[] = $key;
        }
    }
    return $drifted;
}

/**
 * The part of the record that belongs with a given transmission.
 *
 * Each thing we send carries the consent for ITS OWN purpose and nothing else.
 * The statistics upload gets the statistics stamp; sending the crash or vendor
 * stamps along with it would be disclosing choices that upload has no business
 * knowing. Returns null when there is nothing recorded for that setting.
 */
function PrivacyConsentFor($settingName)
{
    $rec = ReadPrivacyConsent();
    if ($rec === null || !isset($rec['settings'][$settingName])) {
        return null;
    }
    $stamp = $rec['settings'][$settingName];
    return array(
        'value' => isset($stamp['value']) ? $stamp['value'] : '',
        'date' => isset($stamp['date']) ? $stamp['date'] : '',
        'via' => isset($stamp['via']) ? $stamp['via'] : '',
        'version' => isset($rec['version']) ? intval($rec['version']) : 0,
        'textHash' => isset($rec['textHash']) ? $rec['textHash'] : '',
    );
}
