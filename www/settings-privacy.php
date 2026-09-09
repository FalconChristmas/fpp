<?
$skipJSsettings = 1;
require_once('common.php');
require_once('jurisdiction.inc');
require_once('privacyTable.inc');
require_once('privacyConsent.inc');
?>
<script src="js/fpp-privacy-table.js?ref=<?= filemtime('js/fpp-privacy-table.js'); ?>"></script>

<p>FPP is built by volunteers: statistics tell us how FPP is used and help us focus development
    where it matters most.  Each row says where it goes, and the
    <i class="fas fa-question-circle"></i> says exactly what is sent.</p>

<?php
// The same table the setup wizard uses -- same rows, same wording, same help,
// same ladder. This is the page people reach when they change their mind, so it
// is the worse one of the two to let go stale.
PrintPrivacyTable('These were chosen during setup and can be changed here at any time.');
?>

<div id='privacySaveStatus' class='smallText text-muted mt-1'></div>

<script>
    var PRIVACY_CONSENT_SETTINGS = <?= json_encode(privacyConsentSettings()) ?>;

    $(document).ready(function () {
        // Unlike the wizard, this page saves as you go: there is no Finish to
        // hold answers until.
        //
        // What counts as "changed" is measured against cfg.current -- the values
        // the box actually holds -- rather than against a baseline captured at
        // load. A baseline needs to be captured before the first change, and this
        // fragment can be initialised more than once, so that ordering was not
        // something to rely on: it silently saved nothing at all. Comparing to
        // the box's own values needs no ordering, and cfg.current is updated as
        // each write succeeds so it stays the truth.
        //
        // A setting that was never set reads as '' and so counts as changed the
        // first time a row is touched. That is correct: the user is choosing it
        // here, and recording the answer is the point.
        fppPrivacyTable.init(<?= json_encode(PrivacyTableConfig(jurisdictionRequiresPriorOptIn())) ?>, function (values) {
            var changed = [];
            $.each(values, function (k, v) {
                if ('' + (fppPrivacyTable.cfg.current[k] || '') !== '' + v) { changed.push(k); }
            });
            if (!changed.length) {
                return;
            }
            $('#privacySaveStatus').text('Saving...');
            var pending = changed.length;
            var failed = false;
            $.each(changed, function (i, k) {
                $.ajax({
                    url: 'api/settings/' + k,
                    data: '' + values[k],
                    method: 'PUT',
                    success: function () {
                        fppPrivacyTable.cfg.current[k] = '' + values[k];
                        settings[k] = values[k];
                    },
                    error: function () {
                        failed = true;
                        $('#privacySaveStatus').text('Could not save ' + k + '.');
                    },
                    complete: function () {
                        if (--pending === 0 && !failed) {
                            recordConsent(changed, values);
                        }
                    }
                });
            });
        });

        // Stamp the act, not just the value -- and stamp it here as well as in
        // the wizard, because this is the page where people change their mind.
        //
        // Only what actually changed is stamped. Someone who turns crash reports
        // off today has not re-affirmed anything about statistics, and dating
        // their statistics consent to today would be a false record rather than a
        // fresher one.
        //
        // A withdrawal is recorded exactly like a grant: Article 7(3) makes
        // withdrawing as much of an act as consenting, and "they turned it off,
        // on this date, here" is the half of the record people forget to keep.
        function recordConsent(changed, values) {
            var consented = {};
            $.each(changed, function (i, k) {
                if (PRIVACY_CONSENT_SETTINGS.indexOf(k) !== -1) {
                    consented[k] = '' + values[k];
                }
            });
            if ($.isEmptyObject(consented)) {
                $('#privacySaveStatus').text('Saved.');
                return;
            }
            $.ajax({
                url: 'api/privacy/consent',
                type: 'POST',
                contentType: 'application/json',
                data: JSON.stringify({
                    via: 'settings',
                    date: new Date().toISOString(),
                    settings: consented
                }),
                success: function () { $('#privacySaveStatus').text('Saved.'); },
                error: function () {
                    $('#privacySaveStatus').text('Saved, but the consent record could not be updated.');
                }
            });
        }
    });
</script>

<div class="mt-3 smallText text-muted">
    Clicking <b>Preview data</b> above shows exactly what this device would send.  The preview is
    generated here; nothing is sent.
</div>
