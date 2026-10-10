// Persistent FPP update progress.
//
// The FPP software / OS update used to live only in the streaming modal that
// started it: refresh the page (or open FPP from another device) and the log
// was gone, leaving just a rebuild warning with no idea an update was
// running. This module makes the update visible everywhere:
//
// - a header spinner icon (menu.inc #navbarUpdateInProgress, next to the
//   update-available spot) on every page that opens the live log,
// - a global banner (menu.inc #updateInProgressFlag): the same banner in
//   the same spot on every page, including the status page, with
//   "View progress" (reopens the live log) and "Dismiss" (hides the banner
//   for a stuck update; the update keeps running). Once the run ends the
//   banner keeps showing its outcome until dismissed or superseded,
// - a completion toast (jGrowl, 15s) for runs seen live, persisted across
//   the starter's Close-reload via sessionStorage,
// - suppression of the "update available" prompts (menu.inc #upgradeFlag,
//   about.php availability banners) while a run is active, restored after,
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
// tracking picks up the real run).
//
// A stale idle echo of the previous run must not clear this instantly: the
// worker's START marker may not be in the log yet when the next poll lands.
// Idle payloads are held off until FPP_UPDATE_START_GRACE_MS after the click;
// an active payload for the real run always takes over immediately. If the
// start never materializes server-side, the first idle poll after the grace
// clears this on its own.
var fppUpdateMarkStartedAt = 0;
var FPP_UPDATE_START_GRACE_MS = 15000;
function FPPUpdate_MarkStarted(side) {
	fppUpdateMarkStartedAt = Date.now();
	FPPUpdate_Render({
		status: 'OK',
		inProgress: true,
		op: side === 'os' ? 'os-upgrade' : 'fpp-update',
		target: '',
		kind: side === 'os' ? 'FPP OS Upgrade' : 'FPP Update',
		runId: 'starting',
		stage: 'Starting…',
		logUpdatedAt: 0,
		logSize: 0,
		stale: false
	});
}

// Which side of the upgrade UI is busy, from the shared activity state:
// 'fpp' covers fpp-update/fpp-upgrade/branch-switch/version-checkout, 'os'
// is an OS upgrade, null when idle. Global so page starters (about.php,
// fpp.js) can refuse a conflicting update while one is already running.
// Instant UX-only feedback for the common case; every starter additionally
// takes the server-side flock (common/updateLock.inc.php), which atomically
// refuses racers with HTTP 409.
function UpdateActivityBusySide() {
	if (typeof fppUpdateActivity === 'undefined' || !fppUpdateActivity) {
		return null;
	}
	if (!fppUpdateActivity.inProgress) {
		return null;
	}
	return fppUpdateActivity.op === 'os-upgrade' ? 'os' : 'fpp';
}

// Ordering against stale echoes. Every server payload carries the log's
// version vector (logUpdatedAt = mtime, logSize = bytes); a payload older
// than the last applied one is a stale echo and is dropped. Equal vectors
// always apply so state transitions without log writes (active->stale)
// still land. Synthetic 'starting' placeholders and empty-runId idles
// (missing/rotated log) are exempt: they carry no vector.
var fppUpdateLastMtime = -1;
var fppUpdateLastSize = -1;
function FPPUpdate_IsStaleEcho(activity) {
	if (!activity || typeof activity !== 'object') {
		return true;
	}
	if (activity.runId === 'starting') {
		return false;
	}
	if (!activity.inProgress && !activity.runId) {
		return false;
	}
	var mtime = (typeof activity.logUpdatedAt === 'number') ? activity.logUpdatedAt : -1;
	var size = (typeof activity.logSize === 'number') ? activity.logSize : -1;
	if (mtime < 0 || fppUpdateLastMtime < 0) {
		return false;
	}
	if (mtime !== fppUpdateLastMtime) {
		return mtime < fppUpdateLastMtime;
	}
	if (size >= 0 && fppUpdateLastSize >= 0) {
		return size < fppUpdateLastSize;
	}
	return false;
}
function FPPUpdate_NoteAppliedVector(activity) {
	if (activity && typeof activity.logUpdatedAt === 'number') {
		fppUpdateLastMtime = activity.logUpdatedAt;
	}
	if (activity && typeof activity.logSize === 'number') {
		fppUpdateLastSize = activity.logSize;
	}
}

// Whether the authoritative direct poll has answered at least once. The
// status hook (fppd WebSocket snapshots carrying up-to-30s-old augmentation)
// renders only until then: afterwards the 5s direct poll is the single source
// of truth, so a stale "run X active" echo can never revive a finished run
// (re-firing the completion toast) behind the poll's back.
var fppUpdateDirectPollApplied = false;

// Activity subscribers (e.g. about.php's per-section busy states). Invoked
// with the activity payload at the end of every FPPUpdate_Render.
var fppUpdateBadgesHidden = false;
var fppUpdateAvailHidden = false;
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
function FPPUpdate_FailedFromActivity(activity, fallbackStage) {
	// Prefer the backend's explicit outcome (FINISH rc / "Upgrade Failed");
	// fall back to the stage-text heuristic for older payloads.
	if (activity && typeof activity.failed !== 'undefined') {
		return !!activity.failed;
	}
	return /fail/i.test(fallbackStage || '');
}
function FPPUpdate_IsSyntheticRunId(runId) {
	return runId === 'starting' || runId === 'workers-active';
}
function FPPUpdate_TrackCompletion(activity) {
	if (activity.inProgress && activity.runId) {
		// A placeholder never replaces a real run being tracked: the real
		// run's idle payload must still match when it arrives, or its
		// completion (toast, outcome banner, restart grace) is lost.
		if (FPPUpdate_IsSyntheticRunId(activity.runId) && fppUpdateLastActive
			&& !FPPUpdate_IsSyntheticRunId(fppUpdateLastActive.runId)) {
			return;
		}
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
		FPPUpdate_OnCompleted(done.kind, FPPUpdate_FailedFromActivity(activity, activity.stage || done.stage), done.runId, !!activity.fppdBinaryExists);
	} else if (fppUpdateLastActive && activity.runId !== fppUpdateLastActive.runId) {
		fppUpdateLastActive = null;
	}
}

// Just-finished run shown in the banner until dismissed or superseded.
// Survives a reload via sessionStorage (the starter's Close button reloads
// the page, which would otherwise erase the outcome).
var fppUpdateCompleted = null;
var FPP_UPDATE_COMPLETED_KEY = 'fppUpdateCompleted';

function FPPUpdate_ClearCompleted() {
	fppUpdateCompleted = null;
	try {
		if (window.sessionStorage) {
			window.sessionStorage.removeItem(FPP_UPDATE_COMPLETED_KEY);
		}
	} catch (e) {
		// Best-effort only.
	}
}

// Client-observed completion timestamp, for the restart grace below.
var fppUpdateCompletedAt = 0;
// Post-update restart window: fppd is expected to be briefly down while it
// restarts after an update completes. Callers skip the "FPPD Daemon is not
// running" warning while this holds; a daemon that never comes back still
// warns once the window passes.
var FPP_UPDATE_QUIET_NOTRUNNING_MS = 10000;
function FPPUpdate_QuietNotRunning() {
	return fppUpdateCompletedAt > 0 && (Date.now() - fppUpdateCompletedAt) < FPP_UPDATE_QUIET_NOTRUNNING_MS;
}

// A run this session saw live just finished: record it for the banner (and
// across a reload) and toast. Success vs failure comes from the closing stage.
function FPPUpdate_OnCompleted(kind, failed, runId, binaryExists) {
	fppUpdateCompleted = { kind: kind || 'Update', failed: !!failed, runId: runId || '' };
	fppUpdateCompletedAt = Date.now();
	// The rebuild banner was rendered server-side from a mid-update view of
	// the world: with the binary rebuilt it must go without a refresh, while
	// a failed build (binary still missing) must (re)show it.
	var $rebuild = $('#compileFPPDBanner');
	if ($rebuild.length) {
		if (binaryExists) {
			$rebuild.remove();
		} else {
			$rebuild.show();
		}
	}
	try {
		if (window.sessionStorage) {
			window.sessionStorage.setItem(FPP_UPDATE_COMPLETED_KEY, JSON.stringify({
				kind: fppUpdateCompleted.kind,
				failed: fppUpdateCompleted.failed,
				runId: fppUpdateCompleted.runId,
				at: Date.now()
			}));
		}
	} catch (e) {
		// Best-effort only.
	}
	FPPUpdate_ShowToast(fppUpdateCompleted.kind, fppUpdateCompleted.failed);
	FPPUpdate_RefreshHeaderVersion();
	// The availability state describes the pre-update box; re-check now that
	// the new version is installed so the navbar icon and banners settle on
	// the post-update truth (fpp.js coalesces concurrent checks per page).
	if (typeof checkForFppUpdate === 'function') {
		try {
			checkForFppUpdate();
		} catch (e) {
			// Best-effort; the next page load checks anyway.
		}
	}
}

// The header version is server-rendered per page load (menu.inc), so without
// this it names the old version until a refresh. Rebuild its label from the
// live values exactly like menu.inc does, and update the FPP_BRANCH global
// the update check compares against (a branch upgrade changes both).
function FPPUpdate_RefreshHeaderVersion() {
	$.ajax({
		url: 'api/system/info?simple=1',
		dataType: 'json',
		cache: false,
		success: function (data) {
			if (!data || typeof data !== 'object') {
				return;
			}
			var version = data.Version;
			var branch = data.Branch;
			if (!version || !branch) {
				return;
			}
			var label = 'v' + version;
			try {
				var escaped = String(branch).replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
				if (!(new RegExp('^' + escaped + '(-.*)?$').test(label))) {
					label += ' (' + branch + ' branch)';
				}
			} catch (e) {
				label += ' (' + branch + ' branch)';
			}
			var $head = $('.versionHead');
			if ($head.length) {
				$head.text(label);
			}
			if (typeof FPP_BRANCH !== 'undefined') {
				try {
					FPP_BRANCH = branch;
				} catch (e) {
					// Read-only context; the label above is what matters.
				}
			}
		},
		error: function () {
			// Keep the old label; the next page load renders it server-side.
		}
	});
}

// Built-in completion notification (jGrowl popover, same mechanism the rest
// of FPP uses). Generous lifetime: the default 3s toast expired unseen behind
// the still-open progress modal, which is why completions never appeared.
function FPPUpdate_ShowToast(kind, failed) {
	if (typeof $.jGrowl !== 'function') {
		return;
	}
	$.jGrowl((kind || 'Update') + (failed ? ' failed.' : ' complete.'), {
		themeState: failed ? 'danger' : 'success',
		life: 15000
	});
}

// Restore a just-finished run after a reload (picks up where the starter's
// Close-reload left off). Stale entries are dropped silently.
function FPPUpdate_RestoreCompleted() {
	try {
		if (!window.sessionStorage) {
			return;
		}
		var raw = window.sessionStorage.getItem(FPP_UPDATE_COMPLETED_KEY);
		if (!raw) {
			return;
		}
		window.sessionStorage.removeItem(FPP_UPDATE_COMPLETED_KEY);
		var c = JSON.parse(raw);
		if (c && c.runId && (Date.now() - (c.at || 0)) < 15 * 60 * 1000) {
			fppUpdateCompleted = { kind: c.kind || 'Update', failed: !!c.failed, runId: c.runId };
			fppUpdateCompletedAt = c.at || Date.now();
			FPPUpdate_ShowToast(fppUpdateCompleted.kind, fppUpdateCompleted.failed);
		}
	} catch (e) {
		// Best-effort only.
	}
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

// Whether this run's UI (in-progress or completed banner) is dismissed on
// this browser. Shared so a mid-run dismiss also covers the completion.
function FPPUpdate_IsDismissed(runId) {
	return !!(runId && FPPUpdate_GetDismissedRunId() === runId);
}

// Swap the banner icon between the spinning progress state and the
// completion states. Unknown markup is left alone.
function FPPUpdate_SetBannerMode($flag, mode) {
	var $icon = $flag.find('i').first();
	if (!$icon.length) {
		return;
	}
	if (mode === 'active') {
		$icon.removeClass('fa-check-circle fa-exclamation-triangle text-success text-danger').addClass('fa-circle-notch fa-spin');
	} else if (mode === 'done-fail') {
		$icon.removeClass('fa-circle-notch fa-spin fa-check-circle text-success').addClass('fa-exclamation-triangle text-danger');
	} else {
		$icon.removeClass('fa-circle-notch fa-spin fa-exclamation-triangle text-danger').addClass('fa-check-circle text-success');
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

// Render the global banner (shown in the same spot on every page, including
// the status page) from an activity payload. Idempotent:
// every poll (status hook or fallback) funnels through here. Returns true
// when the payload was applied, false when ignored as a stale echo (see
// below) so callers (notably the modal fetch) can skip rejected payloads.
function FPPUpdate_Render(activity) {
	if (!activity || typeof activity !== 'object') {
		return false;
	}
	// Stale-echo guard. While fppd is up, its WebSocket pushes snapshots
	// about every second and each rebuild carries the last-seen PHP
	// augmentation forward, so the status hook can hand us an updateActivity
	// up to a poll interval older than the direct endpoint. Rendering that
	// blindly let a stale idle echo clobber live active state (the whole UI
	// flickered out ~1s after showing, until a refresh re-seeded it), and --
	// worse -- let a stale "run X active" echo revive a finished run behind
	// the poll's back, re-firing the completion toast every cycle. Three
	// layers, in order:
	// (1) Version ordering: any payload older than the last applied one
	//     (log mtime+size vector, see FPPUpdate_IsStaleEcho) is dropped.
	// (2) Placeholder grace: right after MarkStarted the worker's START may
	//     not be logged yet, so idle payloads are held off briefly; the real
	//     run's active payload always takes over immediately.
	// (3) Different-run idle: an idle payload for a DIFFERENT run than the
	//     one shown active is such an echo, never an ending -- ignore it. A
	//     same-run idle, a stale flag, or any active payload always applies,
	//     so genuine completion still lands. Synthetic placeholders
	//     ('starting' from MarkStarted, 'workers-active' from the markerless
	//     backend fallback) are explicitly allowed to go idle past their
	//     grace: their ending arrives with an empty/different runId, and
	//     holding them would trap the spinner/banner forever when a start
	//     never materializes or a worker exits with no markers.
	if (FPPUpdate_IsStaleEcho(activity)) {
		return false;
	}
	if (fppUpdateActivity && fppUpdateActivity.runId === 'starting' && !activity.inProgress
		&& (Date.now() - fppUpdateMarkStartedAt) < FPP_UPDATE_START_GRACE_MS) {
		return false;
	}
	if (fppUpdateActivity && fppUpdateActivity.inProgress && !activity.inProgress
		&& (activity.runId || '') !== (fppUpdateActivity.runId || '')) {
		var curRun = fppUpdateActivity.runId || '';
		if (curRun !== 'starting' && curRun !== 'workers-active') {
			return false;
		}
	}
	fppUpdateActivity = activity;
	// The synthetic 'starting' placeholder carries no vector; recording its
	// (0,0) would regress ordering and let older echoes apply afterwards.
	if (activity.runId !== 'starting') {
		FPPUpdate_NoteAppliedVector(activity);
	}
	var show = !!(activity.inProgress || activity.stale);
	var dismissed = show && FPPUpdate_IsDismissed(activity.runId);
	// Tracked here, ahead of the banner below, so the render that observes a
	// completion already shows its outcome (it used to land one render late).
	FPPUpdate_TrackCompletion(activity);

	// A live run supersedes any recorded completion.
	if (activity.inProgress) {
		FPPUpdate_ClearCompleted();
	}

	// Global banner: the same banner in the same spot on every page,
	// including the status page. While active it shows live progress with
	// View progress + Dismiss; once the run ends it keeps showing the
	// outcome ("FPP Update complete") until dismissed or superseded, so the
	// result cannot be missed the way the old transient toast was.
	var $flag = $('#updateInProgressFlag');
	if ($flag.length) {
		if (show && !dismissed) {
			FPPUpdate_SetBannerMode($flag, 'active');
			$('#updateInProgressFlagText').text(FPPUpdate_Title(activity));
			$flag.show();
		} else if (fppUpdateCompleted && !FPPUpdate_IsDismissed(fppUpdateCompleted.runId)) {
			FPPUpdate_SetBannerMode($flag, fppUpdateCompleted.failed ? 'done-fail' : 'done-ok');
			$('#updateInProgressFlagText').text(fppUpdateCompleted.kind + (fppUpdateCompleted.failed ? ' failed' : ' complete'));
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

	// While an update runs, hide the "update available" prompts ("FPP v10.2
	// is available for install", the about.php availability banners): they
	// describe the pre-update box, and acting on one would stack another
	// update on top of the running one. Re-applied on every render (their
	// own renders can re-show them at any time) and restored once via
	// republish when the run ends — but only if the update check already
	// answered, so an unanswered check is never forced into an "unknown"
	// verdict.
	var $availBanners = $('#upgradeFlag, #fppUpdateBanner, #osUpdateBanner, #upgradeRecommendationBanner');
	if ($availBanners.length) {
		if (activity.inProgress) {
			$availBanners.hide();
			fppUpdateAvailHidden = true;
		} else if (fppUpdateAvailHidden) {
			fppUpdateAvailHidden = false;
			if (typeof FPP_UPDATE_STATE !== 'undefined' && FPP_UPDATE_STATE && FPP_UPDATE_STATE.answered
				&& typeof publishFppUpdateState === 'function') {
				try {
					publishFppUpdateState();
				} catch (e) {
					// Restore is best-effort; the checks re-render on their own cycles anyway.
				}
			}
		}
	}

	// While an update is actively running, the "FPPD not found. Rebuild
	// required" banner (menu.inc #compileFPPDBanner, rendered when src/fppd
	// is missing) is wrong: the binary is absent *because* the update
	// cleaned it before rebuilding, and its Rebuild button would start a
	// second update on top of the running one. Hide it so the update banner
	// is the single call to action. Once idle it follows the binary's current
	// state: shown while it is still missing, removed once a rebuild has
	// recreated it -- even in a browser that did not observe the completion.
	var $rebuild = $('#compileFPPDBanner');
	if ($rebuild.length) {
		if (activity.inProgress) {
			$rebuild.hide();
		} else if (activity.fppdBinaryExists === true) {
			$rebuild.remove();
		} else {
			$rebuild.show();
		}
	}

	FPPUpdate_DebugLog(activity);
	for (var i = 0; i < fppUpdateActivityListeners.length; i++) {
		try {
			fppUpdateActivityListeners[i](activity);
		} catch (e) {
			// One listener must not break render or the other listeners.
		}
	}
	return true;
}

// Status-system hook: api/system/status already carries updateActivity
// (finalizeStatusJson), so on pages running the status loop this is free --
// but only until the first authoritative direct poll answers. After that the
// hook is ignored: the fppd snapshots replaying through it can be up to 30s
// older than the direct endpoint, and rendering them revived finished runs
// (repeated completion toasts, busy-side flapping, redundant version
// refreshes). First paint still renders immediately when present.
function FPPUpdate_OnStatusChange() {
	if (fppUpdateDirectPollApplied) {
		return;
	}
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
// load — both of which left the UI stale until a manual refresh. Once this
// poll has answered, it is the single source of truth and the status hook
// stops rendering (see FPPUpdate_OnStatusChange); the endpoint is light
// (cached when idle, one flock probe plus a bounded log tail otherwise),
// and rendering is idempotent. Skipped while the re-attached modal runs its
// own 3s poll, and while the tab is hidden.
function FPPUpdate_FallbackPoll() {
	if (fppUpdateModalOpen) {
		return;
	}
	// Background tabs do no polling: like LoadSystemStatus (fpp.js
	// handleVisibilityChange), skip while hidden. The status hook already
	// stops for hidden tabs; without this every background tab kept hitting
	// the endpoint (and the box) every 5s.
	try {
		if (typeof document !== 'undefined' && document.hidden) {
			return;
		}
	} catch (e) {
		// Absent/blocked visibility API: poll anyway.
	}
	$.ajax({
		url: 'api/system/updateActivity',
		dataType: 'json',
		cache: false,
		success: function (data) {
			if (data && typeof data === 'object') {
				fppUpdateDirectPollApplied = true;
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
				// Authoritative direct-endpoint response: take the status
				// hook out of the picture from here on (see above).
				fppUpdateDirectPollApplied = true;
				// Render may reject this as a stale idle echo for a different
				// run (see the guard above). The modal must not consume
				// rejected payloads: opening progress right after starting an
				// update can race the worker's START marker, and the echo of
				// the previous finished run would otherwise stop the poll and
				// display the old run instead of ever attaching to the new one.
				var accepted = FPPUpdate_Render(data);
				if (accepted) {
					done(data);
				}
			}
		},
		error: function () {
			// Keep the modal open on transient errors; the next tick retries.
		}
	});
}

// When the modal is opened while an update is starting, the first fetch can
// race the worker's START marker and return idle. Keep polling (up to a grace
// window) instead of treating that first idle as completion.
var fppUpdateModalWaitForActive = false;
var fppUpdateModalOpenTime = 0;
var FPP_UPDATE_MODAL_RACE_GRACE_MS = 15000;

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
	if (data.inProgress) {
		fppUpdateModalWaitForActive = false;
	}
	var doneLabel = (typeof data.failed !== 'undefined' && data.failed) ? 'Failed' : 'Done';
	var title = (data.kind || 'FPP Update') + ' — ' + (data.stage || (data.inProgress ? 'Running…' : doneLabel));
	if (typeof SetProgressDialogStatus === 'function') {
		SetProgressDialogStatus('fppUpdateProgress', title);
	}
	if (!data.inProgress && !fppUpdateModalSawCompletion) {
		// Opened mid-start and still within the race grace: the worker's START
		// may simply not be in the log yet. Stay open and keep polling rather
		// than showing the previous run as the outcome.
		if (fppUpdateModalWaitForActive && (Date.now() - fppUpdateModalOpenTime) < FPP_UPDATE_MODAL_RACE_GRACE_MS) {
			return;
		}
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
	fppUpdateModalWaitForActive = !!(activity && activity.inProgress);
	fppUpdateModalOpenTime = Date.now();
	DisplayProgressDialog('fppUpdateProgress', (activity.kind || 'FPP Update') + ' — Loading…');
	// A Hide button next to Close: closes the modal WITHOUT reloading so the
	// user can keep using FPP mid-update and come back via the banner.
	// Added once per modal lifetime.
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
		// A completion recorded just before a reload (the starter's Close
		// button reloads the page) is restored here, ahead of the first
		// paint, so the outcome still shows.
		FPPUpdate_RestoreCompleted();
		// First paint: menuHead.inc pre-populates lastStatusJSON server-side
		// (now including updateActivity), so render immediately when present —
		// then fetch directly at once rather than waiting for the first poll
		// tick, so a mid-update page load shows state within a second.
		FPPUpdate_OnStatusChange();
		FPPUpdate_FallbackPoll();
		// Event delegation: the banner lives in menu.inc (body) while this file
		// loads in the head, so direct binding would miss it.
	$(document).on('click', '#updateInProgressViewBtn, #navbarUpdateInProgressLink', function () {
		openUpdateProgress();
	});
		$(document).on('click', '#updateInProgressDismissBtn', function () {
			// Dismiss what is actually visible: the completed outcome when it
			// is showing (a later idle poll may have moved fppUpdateActivity
			// to a different/empty runId), else the active run.
			var visibleRunId = (fppUpdateCompleted && !FPPUpdate_IsDismissed(fppUpdateCompleted.runId))
				? fppUpdateCompleted.runId
				: (fppUpdateActivity && fppUpdateActivity.runId);
			if (visibleRunId) {
				FPPUpdate_SetDismissedRunId(visibleRunId);
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
