// Persistent FPP update progress.
//
// The FPP software / OS update used to live only in the streaming modal that
// started it: refresh the page (or open FPP from another device) and the log
// was gone, leaving just a rebuild warning with no idea an update was
// running. This module makes the update visible everywhere:
//
// - a global banner (menu.inc #updateInProgressFlag) on every page with
//   "View progress" (reopens the live log) and "Dismiss" (hides the banner
//   for a stuck update; the update keeps running),
// - a spinning "Update in progress" warning row on the status page
//   (index.php #updateInProgressWarningRow) that clears itself when done,
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
	fppUpdateActivity = activity;
	var show = !!(activity.inProgress || activity.stale);
	var dismissed = show && activity.runId && FPPUpdate_GetDismissedRunId() === activity.runId;

	// Global banner (all pages). Dismiss hides the banner only; the update
	// keeps running and the status-page warning keeps its View link.
	var $flag = $('#updateInProgressFlag');
	if ($flag.length) {
		if (show && !dismissed) {
			$('#updateInProgressFlagText').text(FPPUpdate_Title(activity));
			$flag.show();
		} else {
			$flag.hide();
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

// Direct poll fallback: every 15s refresh from the dedicated endpoint so the
// banner also works where the status loop is slow or absent. Skipped while
// the re-attached modal runs its own faster poll.
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

function FPPUpdate_Init() {
	if (typeof OnSystemStatusChange === 'function') {
		OnSystemStatusChange(FPPUpdate_OnStatusChange);
	}
	// First paint: menuHead.inc pre-populates lastStatusJSON server-side
	// (now including updateActivity), so render immediately when present.
	FPPUpdate_OnStatusChange();
	if (typeof fppUpdatePollTimer === 'undefined' || fppUpdatePollTimer === null) {
		fppUpdatePollTimer = setInterval(FPPUpdate_FallbackPoll, 15000);
	}
	// Event delegation: the banner lives in menu.inc (body) while this file
	// loads in the head, so direct binding would miss it.
	$(document).on('click', '#updateInProgressViewBtn, #updateInProgressWarningViewBtn', function () {
		openUpdateProgress();
	});
	$(document).on('click', '#updateInProgressDismissBtn', function () {
		if (fppUpdateActivity && fppUpdateActivity.runId) {
			FPPUpdate_SetDismissedRunId(fppUpdateActivity.runId);
		}
		$('#updateInProgressFlag').hide();
	});
}

var fppUpdatePollTimer = null;
if (typeof $ !== 'undefined') {
	$(document).ready(FPPUpdate_Init);
}
