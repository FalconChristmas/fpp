// Persistent FPP update progress.
//
// The FPP software / OS update used to live only in the streaming modal that
// started it: refresh the page (or open FPP from another device) and the log
// was gone, leaving just a rebuild warning with no idea an update was
// running. This module makes the update visible everywhere:
//
// - a header spinner icon (menu.inc #navbarUpdateInProgress, next to the
//   update-available spot) on every page that opens the live log,
// - a global banner (menu.inc #updateInProgressFlag) on every page with
//   "View progress" (reopens the live log) and "Dismiss" (hides the banner
//   for a stuck update; the update keeps running). The status page is the
//   exception: it shows the warning row below instead of the banner, so the
//   update appears exactly once there (plus the header icon),
// - a spinning "Update in progress" warning row on the status page
//   (index.php #updateInProgressWarningRow) that clears itself when done,
// - suppression of the "FPPD not found. Rebuild required" banner
//   (menu.inc #compileFPPDBanner) while an update is actively running,
//   since the binary is absent *because* of the running rebuild,
// - a re-openable live modal fed from logs/fpp_system_upgrades.log, so any
//   browser can attach, detach (Hide), refresh, and reattach.
//
// State source is GET api/system/updateActivity (also embedded in
// api/system/status as updateActivity). Dismiss is per-browser per-run:
// localStorage remembers the dismissed runId, so a *new* update re-shows
// the banner while a dismissed stuck one stays hidden on that browser.

var FPP_UPDATE_DISMISS_KEY = 'fppUpdateDismissedRunId';
var fppUpdateActivity = null;
var fppUpdateModalOpen = false;
var fppUpdateModalTimer = null;
var fppUpdateModalSawCompletion = false;
var fppUpdateModalFirstLoad = true;

// Called the moment this browser starts an update, so the header icon,
// banner, and about.php sections react instantly instead of waiting for the
// next server poll (which can lag several seconds behind the click, and stall
// entirely under compile load). The payload is deliberately shaped exactly
// like the server's: the first real poll transparently takes over (its runId
// differs, so a dismiss of this placeholder never sticks, and completion
// tracking picks up the real run). If the start never materializes
// server-side, the next idle poll clears this on its own.
function FPPUpdate_MarkStarted(side) {
	FPPUpdate_Render({
		status: 'OK',
		inProgress: true,
		op: side === 'os' ? 'os-upgrade' : 'fpp-update',
		target: '',
		kind: side === 'os' ? 'FPP OS Upgrade' : 'FPP Update',
		runId: 'starting',
		stage: 'Starting…',
		logUpdatedAt: 0,
		stale: false
	});
}

// Which side of the upgrade UI is busy, from the shared activity state:
// 'fpp' covers fpp-update/fpp-upgrade/branch-switch/version-checkout, 'os'
// is an OS upgrade, null when idle. Global so page starters (about.php,
// fpp.js) can refuse a conflicting update while one is already running.
function UpdateActivityBusySide() {
	if (typeof fppUpdateActivity === 'undefined' || !fppUpdateActivity) {
		return null;
	}
	if (!fppUpdateActivity.inProgress) {
		return null;
	}
	return fppUpdateActivity.op === 'os-upgrade' ? 'os' : 'fpp';
}

// Activity subscribers (e.g. about.php's per-section busy states). Invoked
// with the activity payload at the end of every FPPUpdate_Render.
var fppUpdateBadgesHidden = false;
var fppUpdateActivityListeners = [];
function FPPUpdate_OnActivity(fn) {
	if (typeof fn === 'function') {
		fppUpdateActivityListeners.push(fn);
	}
}

// Completion tracking: remembers the run this browser session saw active,
// so the notification below fires exactly once, only for a run that was
// observed live (opening a page after the fact stays silent), and only when
// the run actually finished (not when it went stale).
var fppUpdateLastActive = null;
function FPPUpdate_TrackCompletion(activity) {
	if (activity.inProgress && activity.runId) {
		fppUpdateLastActive = {
			runId: activity.runId,
			kind: activity.kind || 'Update',
			stage: activity.stage || ''
		};
	} else if (
		fppUpdateLastActive &&
		!activity.inProgress &&
		!activity.stale &&
		activity.runId === fppUpdateLastActive.runId
	) {
		var done = fppUpdateLastActive;
		fppUpdateLastActive = null;
		FPPUpdate_NotifyCompletion(done.kind, activity.stage || done.stage);
	} else if (fppUpdateLastActive && activity.runId !== fppUpdateLastActive.runId) {
		fppUpdateLastActive = null;
	}
}

// Built-in completion notification (jGrowl popover, same mechanism the rest
// of FPP uses). Success vs failure comes from the run's closing stage.
function FPPUpdate_NotifyCompletion(kind, stage) {
	if (typeof $.jGrowl !== 'function') {
		return;
	}
	var failed = /fail/i.test(stage || '');
	var label = kind || 'Update';
	$.jGrowl(label + (failed ? ' failed.' : ' complete.'), {
		themeState: failed ? 'danger' : 'success'
	});
}

// Dismissed run id for this browser ("" when nothing dismissed). Wrapped:
// private-mode localStorage throws on access in some browsers, and that must
// never break the banner render.
function FPPUpdate_GetDismissedRunId() {
	try {
		return window.localStorage ? window.localStorage.getItem(FPP_UPDATE_DISMISS_KEY) || '' : '';
	} catch (e) {
		return '';
	}
}

function FPPUpdate_SetDismissedRunId(runId) {
	try {
		if (window.localStorage) {
			window.localStorage.setItem(FPP_UPDATE_DISMISS_KEY, runId || '');
		}
	} catch (e) {
		// Best-effort only.
	}
}

// Title for banner/warning/modal from an activity payload.
function FPPUpdate_Title(activity) {
	if (!activity) {
		return 'FPP Update';
	}
	var kind = activity.kind || 'FPP Update';
	if (activity.inProgress && activity.stage) {
		return kind + ' — ' + activity.stage;
	}
	if (!activity.inProgress && !activity.stale) {
		return kind;
	}
	if (activity.stale) {
		return 'Last update did not finish';
	}
	return kind + ' in progress';
}

// Render banner + status-page warning from an activity payload. Idempotent:
// every poll (status hook or fallback) funnels through here.
function FPPUpdate_Render(activity) {
	if (!activity || typeof activity !== 'object') {
		return;
	}
	// Stale-echo guard. While fppd is up, its WebSocket pushes snapshots
	// about every second and each rebuild carries the last-seen PHP
	// augmentation forward, so the status hook can hand us an updateActivity
	// up to a poll interval older than the direct endpoint. Rendering that
	// blindly let a stale idle echo clobber live active state (the whole UI
	// flickered out ~1s after showing, until a refresh re-seeded it). An
	// idle payload for a DIFFERENT run than the one shown active is such an
	// echo, never an ending — ignore it. A same-run idle, a stale flag, or
	// any active payload always applies, so genuine completion still lands.
	if (fppUpdateActivity && fppUpdateActivity.inProgress && !activity.inProgress
		&& (activity.runId || '') !== (fppUpdateActivity.runId || '')) {
		return;
	}
	fppUpdateActivity = activity;
	var show = !!(activity.inProgress || activity.stale);
	var dismissed = show && activity.runId && FPPUpdate_GetDismissedRunId() === activity.runId;

	// Global banner (all pages). Dismiss hides the banner only; the update
	// keeps running and the status-page warning keeps its View link.
	// The status page has its own dedicated warning row (below) — showing
	// the banner there too renders the same update twice, so the banner
	// stays hidden wherever that row exists.
	var hasWarnRow = $('#updateInProgressWarningRow').length > 0;
	var $flag = $('#updateInProgressFlag');
	if ($flag.length) {
		if (show && !dismissed && !hasWarnRow) {
			$('#updateInProgressFlagText').text(FPPUpdate_Title(activity));
			$flag.show();
		} else {
			$flag.hide();
		}
	}

	// Header icon (every page): a spinner next to the update-available spot
	// that opens the live log. Never dismissible — it is the always-visible
	// indicator, and it clears itself when the update ends.
	var $navIcon = $('#navbarUpdateInProgress');
	if ($navIcon.length) {
		if (activity.inProgress) {
			$('#navbarUpdateInProgressLink').attr('title', FPPUpdate_Title(activity) + ' — view progress');
			$navIcon.show();
		} else {
			$navIcon.hide();
		}
	}

	// While an update runs, only its spinner shows in the header: the
	// "update available" badges describe the pre-update box and would
	// contradict it. Re-applied on every render (their own checks can
	// re-show them at any time) and restored from their own state once the
	// run ends.
	var $avail = $('#navbarUpdateAvail');
	var $pluginAvail = $('#navbarPluginUpdateAvail');
	if ($avail.length || $pluginAvail.length) {
		if (activity.inProgress) {
			$avail.hide();
			$pluginAvail.hide();
			fppUpdateBadgesHidden = true;
		} else if (fppUpdateBadgesHidden) {
			fppUpdateBadgesHidden = false;
			if (typeof updateNavbarUpdateIndicator === 'function') {
				try {
					updateNavbarUpdateIndicator();
				} catch (e) {
					// Restore is best-effort; the badges' own checks re-render them anyway.
				}
			}
			if (typeof updateNavbarPluginUpdateIndicator === 'function') {
				try {
					updateNavbarPluginUpdateIndicator();
				} catch (e) {
					// Same as above.
				}
			}
		}
	}

	// Status-page warning row. Never dismissible here: it is the way back to
	// the progress for a dismissed banner, and it clears itself on completion.
	var $warn = $('#updateInProgressWarningRow');
	if ($warn.length) {
		if (show) {
			$('#updateInProgressWarningText').text(FPPUpdate_Title(activity));
			$warn.show();
		} else {
			$warn.hide();
		}
	}

	// While an update is actively running, the "FPPD not found. Rebuild
	// required" banner (menu.inc #compileFPPDBanner, rendered when src/fppd
	// is missing) is wrong: the binary is absent *because* the update
	// cleaned it before rebuilding, and its Rebuild button would start a
	// second update on top of the running one. Hide it so the update banner
	// is the single call to action; it comes back on its own when the
	// update ends. A merely stale (never finished, nothing running) update
	// leaves the rebuild banner alone: fppd may genuinely need rebuilding.
	var $rebuild = $('#compileFPPDBanner');
	if ($rebuild.length) {
		if (activity.inProgress) {
			$rebuild.hide();
		} else {
			$rebuild.show();
		}
	}

	FPPUpdate_TrackCompletion(activity);
	FPPUpdate_DebugLog(activity);
	for (var i = 0; i < fppUpdateActivityListeners.length; i++) {
		try {
			fppUpdateActivityListeners[i](activity);
		} catch (e) {
			// One listener must not break render or the other listeners.
		}
	}
}

// Status-system hook: api/system/status already carries updateActivity
// (finalizeStatusJson), so on pages running the status loop this is free.
function FPPUpdate_OnStatusChange() {
	try {
		if (typeof lastStatusJSON !== 'undefined' && lastStatusJSON && lastStatusJSON.updateActivity) {
			FPPUpdate_Render(lastStatusJSON.updateActivity);
		}
	} catch (e) {
		// Render must never break the status chain (triggerStatusChangeFunctions
		// already isolates callbacks; this is belt and braces).
	}
}

// Direct poll: refresh from the dedicated endpoint every 5s so the banner,
// header icon, and about.php section states track a running update live.
// This deliberately does not wait on the status loop: while fppd is up the
// host-side augmentation (which carries updateActivity) only arrives every
// 30s, and the server-side first-paint snapshot can time out under compile
// load — both of which left the UI stale until a manual refresh. The
// endpoint is light (one ps + a bounded log tail), and rendering is
// idempotent, so overlapping with the status hook is harmless. Skipped while
// the re-attached modal runs its own 3s poll.
function FPPUpdate_FallbackPoll() {
	if (fppUpdateModalOpen) {
		return;
	}
	$.ajax({
		url: 'api/system/updateActivity',
		dataType: 'json',
		cache: false,
		success: function (data) {
			if (data && typeof data === 'object') {
				FPPUpdate_Render(data);
			}
		},
		error: function () {
			// Transient (update restarts the web server mid-run); keep the
			// last state and retry on the next tick.
		}
	});
}

// Fetch the run's log tail for the modal.
function FPPUpdate_FetchModalState(done) {
	$.ajax({
		url: 'api/system/updateActivity?logTail=1&lines=300',
		dataType: 'json',
		cache: false,
		success: function (data) {
			if (data && typeof data === 'object') {
				FPPUpdate_Render(data);
				done(data);
			}
		},
		error: function () {
			// Keep the modal open on transient errors; the next tick retries.
		}
	});
}

function FPPUpdate_UpdateModal(data) {
	var area = document.getElementById('fppUpdateProgressText');
	if (area && typeof data.logTail === 'string' && data.logTail !== '') {
		// Stick to the bottom only when the user was already there, so
		// scrolled-up reading is not yanked away by an update.
		var nearBottom = area.scrollHeight - area.scrollTop - area.clientHeight < 60;
		// Wholesale replace avoids overlap-diff bugs across polls; the tail
		// endpoint always returns the run's last lines in order.
		area.value = data.logTail;
		if (nearBottom || fppUpdateModalFirstLoad) {
			area.scrollTop = area.scrollHeight;
		}
		fppUpdateModalFirstLoad = false;
	}
	var title = (data.kind || 'FPP Update') + ' — ' + (data.stage || (data.inProgress ? 'Running…' : 'Done'));
	if (typeof SetProgressDialogStatus === 'function') {
		SetProgressDialogStatus('fppUpdateProgress', title);
	}
	if (!data.inProgress && !fppUpdateModalSawCompletion) {
		// Run ended (or was already over / stale): show the outcome and let
		// the user out. One final fetch is unnecessary -- this payload
		// already carries the closing tail.
		fppUpdateModalSawCompletion = true;
		if (fppUpdateModalTimer) {
			clearInterval(fppUpdateModalTimer);
			fppUpdateModalTimer = null;
		}
		if (typeof EnableModalDialogCloseButton === 'function') {
			EnableModalDialogCloseButton('fppUpdateProgress');
		}
	}
}

// Open (or reopen) the live update log from any page. Safe to call when no
// update is running: it shows the last run's tail with Close enabled.
function openUpdateProgress() {
	if (typeof DisplayProgressDialog !== 'function') {
		return;
	}
	fppUpdateModalOpen = true;
	fppUpdateModalSawCompletion = false;
	fppUpdateModalFirstLoad = true;
	var activity = fppUpdateActivity || {};
	DisplayProgressDialog('fppUpdateProgress', (activity.kind || 'FPP Update') + ' — Loading…');
	// A Hide button next to Close: closes the modal WITHOUT reloading so the
	// user can keep using FPP mid-update and come back via the banner or the
	// status-page warning. Added once per modal lifetime.
	var $footer = $('#fppUpdateProgress .modal-footer');
	if ($footer.length && $('#fppUpdateProgressHideButton').length === 0) {
		var $hide = $('<button id="fppUpdateProgressHideButton" class="buttons">Hide (view later)</button>');
		$hide.on('click', function () {
			FPPUpdate_HideModal();
		});
		$footer.prepend($hide);
	}
	if (fppUpdateModalTimer) {
		clearInterval(fppUpdateModalTimer);
	}
	var tick = function () {
		FPPUpdate_FetchModalState(FPPUpdate_UpdateModal);
	};
	tick();
	fppUpdateModalTimer = setInterval(tick, 3000);
	$('#fppUpdateProgress').off('hidden.bs.modal.fppUpdate').on('hidden.bs.modal.fppUpdate', function () {
		FPPUpdate_HideModal();
	});
}

// Detach from the modal without affecting the running update.
function FPPUpdate_HideModal() {
	fppUpdateModalOpen = false;
	if (fppUpdateModalTimer) {
		clearInterval(fppUpdateModalTimer);
		fppUpdateModalTimer = null;
	}
	if (typeof CloseModalDialog === 'function' && $('#fppUpdateProgress').length) {
		try {
			CloseModalDialog('fppUpdateProgress');
		} catch (e) {
			// Modal already gone.
		}
	}
}

// Add the same Hide button to the starter modals (FPP/OS/branch upgrade and
// the rebuild dialog) so the browser that kicked off the update can also get
// back to the UI mid-run and reattach later. No-op when already added or
// when the dialog does not exist (yet).
function FPPUpdate_AddHideButton(dialogId) {
	var $footer = $('#' + dialogId + ' .modal-footer');
	if (!$footer.length || $('#' + dialogId + 'HideButton').length !== 0) {
		return;
	}
	var $hide = $('<button id="' + dialogId + 'HideButton" class="buttons">Hide (view later)</button>');
	$hide.on('click', function () {
		if (typeof CloseModalDialog === 'function') {
			try {
				CloseModalDialog(dialogId);
			} catch (e) {
				// Modal already gone.
			}
		}
	});
	$footer.prepend($hide);
}

// Last rendered state key, for transition-only diagnostics (see below).
var fppUpdateLastLoggedKey = '';
function FPPUpdate_DebugLog(activity) {
	if (typeof console === 'undefined' || typeof console.debug !== 'function') {
		return;
	}
	var key = (activity.inProgress ? 'active' : (activity.stale ? 'stale' : 'idle')) +
		'|' + activity.op + '|' + (activity.stage || '');
	if (key !== fppUpdateLastLoggedKey) {
		fppUpdateLastLoggedKey = key;
		console.debug('[fpp-update] activity: ' + key);
	}
}

function FPPUpdate_Init() {
	// The interval is armed first, inside its own guard, so that a failure
	// anywhere in the first-paint work below can never leave this page with
	// no polling at all (which read as "nothing shows until refresh").
	if (typeof fppUpdatePollTimer === 'undefined' || fppUpdatePollTimer === null) {
		fppUpdatePollTimer = setInterval(FPPUpdate_FallbackPoll, 5000);
	}
	try {
		if (typeof OnSystemStatusChange === 'function') {
			OnSystemStatusChange(FPPUpdate_OnStatusChange);
		}
		// First paint: menuHead.inc pre-populates lastStatusJSON server-side
		// (now including updateActivity), so render immediately when present —
		// then fetch directly at once rather than waiting for the first poll
		// tick, so a mid-update page load shows state within a second.
		FPPUpdate_OnStatusChange();
		FPPUpdate_FallbackPoll();
		// Event delegation: the banner lives in menu.inc (body) while this file
		// loads in the head, so direct binding would miss it.
		$(document).on('click', '#updateInProgressViewBtn, #updateInProgressWarningViewBtn, #navbarUpdateInProgressLink', function () {
			openUpdateProgress();
		});
		$(document).on('click', '#updateInProgressDismissBtn', function () {
			if (fppUpdateActivity && fppUpdateActivity.runId) {
				FPPUpdate_SetDismissedRunId(fppUpdateActivity.runId);
			}
			$('#updateInProgressFlag').hide();
		});
	} catch (e) {
		// First paint is best-effort; the interval above retries continuously.
	}
}

var fppUpdatePollTimer = null;
if (typeof $ !== 'undefined') {
	$(document).ready(FPPUpdate_Init);
}
