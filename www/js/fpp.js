STATUS_IDLE = '0';
STATUS_PLAYING = '1';
STATUS_STOPPING_GRACEFULLY = '2';
STATUS_STOPPING_GRACEFULLY_AFTER_LOOP = '3';
STATUS_PAUSED = '5';

// Globals
gblCurrentPlaylistModified = false;
gblCurrentPlaylistIndex = 0;
gblCurrentPlaylistEntryType = '';
gblCurrentPlaylistEntrySeq = '';
gblCurrentPlaylistEntrySong = '';
gblCurrentLoadedPlaylist = '';
gblCurrentLoadedPlaylistCount = 0;
gblNavbarMenuVisible = 0;
gblStatusRefreshSeconds = 5;
gblCurrentBootstrapViewPort = '';

var max_retries = 60;
var retry_poll_interval_arr = [];

var minimalUI = 0;
var hasTouch = false;
var statusTimeout = null;
var lastStatus = '';
var lastStatusJSON = null;
var statusChangeFuncs = [];

// fppd status WebSocket (/fppdws).  When connected, fppd pushes its /fppd/status
// payload here instead of every page polling api/system/status for it.  The PHP
// poll then runs slowly (gblSystemAugRefreshSeconds) and only for the host-side
// augmentation (advancedView, wifi, interfaces, flags, plugin indicators, crash
// warning) via ?systemonly=1.  If the socket never connects or drops -- an older
// system without the proxy_wstunnel apache module, an odd reverse proxy, or fppd
// itself being down -- everything falls back to the full poll at gblStatusRefreshSeconds,
// which is also the only path that can report "fppd Not Running".

// The path prefix this FPP's document root is being served under, with a
// trailing slash ('/' when it is served at the root).
//
// It cannot be derived from window.location.pathname.  That is the path of the
// *page*, which says nothing about how much of it belongs to a reverse proxy:
// /api/commandhelp.php and /proxy/1.2.3.4/index.php are indistinguishable by
// shape, and matching one known proxy layout only ever works for that layout.
//
// This script is always at <root>/js/fpp.js, so its own resolved src is the one
// thing that reliably names the prefix -- whatever it is, and whichever
// subdirectory the page loading it lives in, since the browser has already
// resolved '../js/fpp.js' and 'js/fpp.js' to the same absolute URL.
//
// currentScript is only valid while this file is first evaluated, so it is read
// here at the top rather than at point of use.
var gblFPPRoot = (function () {
	var el = document.currentScript;
	if (!el) {
		var all = document.getElementsByTagName('script');
		for (var i = 0; i < all.length; i++) {
			if (/\/js\/fpp\.js(\?|$)/.test(all[i].src)) {
				el = all[i];
				break;
			}
		}
	}
	if (!el || !el.src) {
		return '/';
	}
	try {
		var path = new URL(el.src, document.baseURI).pathname;
		var root = path.replace(/js\/fpp\.js.*$/, '');
		return root.charAt(root.length - 1) === '/' ? root : root + '/';
	} catch (e) {
		return '/';
	}
})();

var fppdWS = null;
var fppdWSConnected = false;
var fppdWSReconnectDelay = 1000; // ms, backoff up to 30s
var fppdWSReconnectTimer = null;
var fppdWSLastMsgTime = 0;
var fppdWSWatchdog = null;
// Latest snapshot received while the page was hidden.  A hidden tab keeps the
// socket open (so it is instantly up to date when the user comes back) but skips
// the statusChangeFuncs/DOM work for every push; this holds the last one so the
// page can catch up in a single pass on the way back to visible.
var fppdWSPendingStatus = null;
var gblSystemAugRefreshSeconds = 30; // WS-mode PHP poll interval for the augmentation
// Warnings have two independent sources that must be unioned for display: fppd's
// live warnings (over the WS) and PHP's warnings (crash report / "fppd Not
// Running") which survive an fppd crash.  Kept separate so a cleared fppd warning
// disappears immediately instead of lingering in a stale PHP copy.
var _wsWarnings = [];
var _wsWarningInfo = [];
var _systemWarnings = [];
var _systemWarningInfo = [];

// Bumped every time this page sets restartFlag/rebootFlag locally.  A status
// response carries the value PHP read out of /media/settings when the request
// started, so one that was already in flight when the page changed a flag is
// older than what we know and has to be discarded -- otherwise saving a setting
// while a poll is outstanding hides the banner it just raised.  See
// applyServerRestartRebootFlags().
var gblRestartRebootFlagEpoch = 0;

// Track fppd running state across status callbacks to detect a restart
// transition (not-running → running), at which point we re-fetch the
// command list and refresh the page if it has changed (e.g. plugin
// commands added/removed).
var _fppdWasRunning = null;
var _savedCommandListJSON = '';
var zebraPinSubContentTop = 0;
var VolumeChangeInProgress = false;
var VolumeChangeAPIInProgress = false;
var currentWarnings = [];
var warningDefinitions = [];

// Global FPP update state - used by navbar icon, menu banner, and upgrade page
var FPP_UPDATE_STATE = {
	branchUpgradeAvailable: false,
	branchUpgradeTarget: '',
	branchUpgradeVersion: '',
	isMajorVersionUpgrade: false,
	commitUpdateAvailable: false,
	remoteCommit: '',
	currentBranch: '',
	localCommit: '',
	versionUnknown: false,
	isEndOfLife: false,
	latestMajorVersion: 0,
	checked: false
};

// Build "http://host" + path. IPv6 literals (contain ':') must be bracketed;
// IPv4 and hostnames never contain ':' so they pass through unchanged.
// No zone-id ("%eth0") handling on purpose: a link-local address can't be
// reached from the browser regardless of how it's encoded, so callers filter
// those out instead of building a URL that can never work.
function buildHttpURL(ip, path) {
	path = path || '';
	var host = ip.indexOf(':') !== -1 ? '[' + ip + ']' : ip;
	return 'http://' + host + path;
}

// navigator.clipboard only exists in a secure context (HTTPS or localhost) -
// FPP's web UI is normally reached over plain HTTP on a LAN IP, where it's
// simply undefined and calling .writeText() on it throws. Falls back to the
// classic hidden-<textarea> + execCommand('copy') trick there.
function CopyTextToClipboard (text) {
	if (navigator.clipboard && navigator.clipboard.writeText) {
		return navigator.clipboard.writeText(text);
	}
	var $ta = $('<textarea readonly>').val(text).css({ position: 'fixed', top: '-1000px', left: '-1000px' }).appendTo('body');
	$ta[0].select();
	try {
		document.execCommand('copy');
	} catch (e) {
		// nothing more we can do
	}
	$ta.remove();
	return Promise.resolve();
}

/* jQuery Colpick activation */
var fppCommandColorPicker_fppDialogIntervalTimer = null;
var fppCommandColorPicker_fppDialogIsOpen = false;
var fppCommandColorPicker_loopMaxRetries = 10;
var fppCommandColorPicker_loopCount = 0;
var fppCommandColorPicker_intervalMs = 150;

if (
	'ontouchstart' in window ||
	navigator.maxTouchPoints > 0 ||
	navigator.msMaxTouchPoints > 0
) {
	hasTouch = true;
}

/* Load warnings definitions */
$.getJSON('warnings-definitions.json', function (json) {
	//console.log(json); // this will show the info it in firebug console
	warningDefinitions = json;
});

/* On Page Ready Function Handler
There is a common set of content loading, action setting and viewport change functions in this file.

As well as the common behaviors a set of page specific page ready functions will be called if
declared on each individual php page

All Pages should use the following functions declared in page if required:

pageSpecific_PageLoad_DOM_Setup() - loads any page specific DOM manipulations
pageSpecific_PageLoad_PostDOMLoad_ActionsSetup() - loads any page specific actions onto DOM elements
once in place pageSpecific_ViewPortChange() - loads any actions unique to the page which need to
occur on a viewport size change

*/
// Loading skeletons only earn their keep on a slow load. Against a local FPP
// the status APIs answer in tens of milliseconds, so painting the placeholders
// immediately just flashes a big grey block where the playlist controls are
// about to appear. Hold them back and reveal them only if the page is still
// loading once the delay is up; a fast refresh then never shows them at all.
var SKELETON_REVEAL_DELAY_MS = 400;

$(function () {
	setTimeout(function () {
		if (document.body && document.body.classList.contains('is-loading')) {
			document.body.classList.add('show-loading-skeleton');
		}
	}, SKELETON_REVEAL_DELAY_MS);

	// do any page DOM manipulation required
	common_PageLoad_DOM_Setup();
	if (typeof pageSpecific_PageLoad_DOM_Setup === 'function') {
		pageSpecific_PageLoad_DOM_Setup();
	}

	// Activate UI components / actions on fully loaded DOM
	if (document.readyState === 'loading') {
		document.addEventListener('DOMContentLoaded', loadPageReadyActions);
	} else {
		loadPageReadyActions();
	}
});

function loadPageReadyActions () {
	// call common setup actions
	common_PageLoad_PostDOMLoad_ActionsSetup();
	// call page specific setup actions
	if (typeof pageSpecific_PageLoad_PostDOMLoad_ActionsSetup === 'function') {
		pageSpecific_PageLoad_PostDOMLoad_ActionsSetup();
	}
	// setup control of how to handle a change in bootstrap viewport size
	setViewPortControl();
}

var UI_LEVEL_OVERRIDE_MINUTES = 15;

function ShowAdvancedTemporarily () {
	var expires = Math.floor(Date.now() / 1000) + UI_LEVEL_OVERRIDE_MINUTES * 60;
	document.cookie =
		'fppUiLevelOverrideExp=' +
		expires +
		'; path=/; max-age=' +
		UI_LEVEL_OVERRIDE_MINUTES * 60;
	location.reload();
}

function ExitUiLevelOverride () {
	document.cookie = 'fppUiLevelOverrideExp=; path=/; max-age=0';
	location.reload();
}

// Status-change callback that detects an fppd restart (transition from
// not-running to running).  When that happens, re-fetch the command list
// and reload the page if the list has changed.
function OnFPPDRestart () {
	var isRunning = lastStatusJSON &&
		'fppd' in lastStatusJSON &&
		lastStatusJSON.fppd === 'running';
	if (_fppdWasRunning === false && isRunning) {
		checkCommandListChanged();
	}
	_fppdWasRunning = isRunning;
}

function common_PageLoad_DOM_Setup () {
	OnSystemStatusChange(RefreshHeaderBar);
	OnSystemStatusChange(IsFPPDrunning);
	OnSystemStatusChange(OnFPPDRestart);
	captureCommandListJSON();
	// If status was pre-populated server-side, fire callbacks immediately so
	// the header/footer render without waiting for the first AJAX round-trip.
	if (lastStatusJSON) {
		triggerStatusChangeFunctions();
	}
	bindVisibilityListener();

	$('a.link-to-fpp-manual').attr('href', getManualLink());

	$.jGrowl.defaults.closerTemplate = '<div>Close Notifications</div>';
	SetupToolTips();
	LoadSystemStatus();
	startFppdWS();

	CheckBrowser();
	CheckRestartRebootFlags();
}

function common_PageLoad_PostDOMLoad_ActionsSetup () {
	$(document).on('click', '.navbar-toggler', ToggleMenu);
	$(document).on('keydown', handleKeypress);

	// Handling touch
	if (hasTouch == true) {
		$('body').addClass('has-touch');
		if ($('.header').length > 0) {
			var swipeHandler = new SwipeHandler($('.header').get(0));
			swipeHandler.onLeft(function () {
				$('.header').toggleClass('swiped');
			});
			swipeHandler.onRight(function () {
				$('.header').toggleClass('swiped');
			});
			swipeHandler.run();
		}
	} else {
		$('body').addClass('no-touch');
	}

	// button click functionality
	$('[data-bs-toggle=pill], [data-bs-toggle=tab]').on('click', function () {
		if (history.pushState) {
			history.pushState(null, null, $(this).attr('href'));
		}
	});

	// window scrolling events
	window.onscroll = function () {
		checkScrollTopButton();
	};

	//show first visible tab (if no tab specified in url)
	if (!location.hash) {
		const triggerFirstTabEl = $('[role="tablist"] li:visible a').first()[0];
		if (triggerFirstTabEl) {
			bootstrap.Tab.getOrCreateInstance(triggerFirstTabEl).show();
			//setup sticky on first page load
			setTimeout(function () {
				SetTablePageHeader_ZebraPin();
				float_fppStickyThead();
				scrollToTop();
			}, 50);
		}
	}
	var listOfTabs = [];
	$('[role="tab"]').each(function () {
		listOfTabs.push($(this).attr('data-bs-target'));
	});

	// showing tab directly if referenced in url
	if (location.hash && listOfTabs.includes(location.hash)) {
		bootstrap.Tab.getOrCreateInstance(
			document.querySelector('[data-bs-target="' + location.hash + '"]')
		).show();
		setTimeout(function () {
			SetTablePageHeader_ZebraPin();
			float_fppStickyThead();
			scrollToTop();
		}, 50);
	}

	//Setup Tab actions for <a> based tabs
	const triggerTabList_a = document.querySelectorAll('[role="tablist"] a');
	triggerTabList_a.forEach(triggerEl => {
		const tabTrigger = new bootstrap.Tab(triggerEl);

		triggerEl.addEventListener('shown.bs.tab', event => {
			// when the tab is selected update the url with the hash
			window.location.hash = event.target.dataset.bsTarget;
			SetTablePageHeader_ZebraPin();
			float_fppStickyThead();
			scrollToTop();
		});
	});

	//Setup Tab actions for button based tabs
	const triggerTabList_btn = document.querySelectorAll(
		'[role="tablist"] li button'
	);
	triggerTabList_btn.forEach(triggerEl => {
		const tabTrigger = new bootstrap.Tab(triggerEl);

		triggerEl.addEventListener('shown.bs.tab', event => {
			// when the tab is selected update the url with the hash
			window.location.hash = event.target.dataset.bsTarget;
			SetTablePageHeader_ZebraPin();
			float_fppStickyThead();
			scrollToTop();
		});
	});
}

function common_ViewPortChange () {
	// Events to run on window viewport resizing
	/* 	console.log(
		'View port changed to: ' + gblCurrentBootstrapViewPort + ' - fixing layout'
	); */
	SetTablePageHeader_ZebraPin();
	float_fppStickyThead();
}

function SetTablePageHeader_ZebraPin () {
	//console.log('running zebra pin');
	//if Zebra pin already exists destroy it
	if (window.hasOwnProperty('zp_tablePageHeader')) {
		zp_tablePageHeader.destroy(); //needs Zebra Pin v3 to work
		zp_tablePageHeader = null;
	}

	//check if a normal or tabbed page layout
	if ($('.nav-pills').children().length > 0) {
		hasTabs = true;
	} else {
		hasTabs = false;
	}

	if (hasTabs) {
		// Pin Visible Table Tab Page Headers
		if ($('.tableTabPageHeader:visible')) {
			zp_tablePageHeader = new $.Zebra_Pin($('.tableTabPageHeader:visible'), {
				contained: true,
				top_spacing:
					$('.header').css('position') == 'fixed'
						? $('.header').outerHeight(true)
						: 0
			});
		}
		// Calc position of bottom of pinned tableTabPageHeader
		zebraPinSubContentTop =
			($('.header').css('position') == 'fixed'
				? $('.header').outerHeight(true)
				: 0) + $('.tableTabPageHeader:visible').outerHeight(true);
	} else {
		// Pin Table Page Headers
		if ($('.tablePageHeader')) {
			zp_tablePageHeader = new $.Zebra_Pin($('.tablePageHeader'), {
				contained: true,
				top_spacing:
					$('.header').css('position') == 'fixed'
						? $('.header').outerHeight(true)
						: 0
			});
		}

		// Calc position of bottom of pinned tablePageHeader
		zebraPinSubContentTop =
			($('.header').css('position') == 'fixed'
				? $('.header').outerHeight(true)
				: 0) + $('.tablePageHeader').outerHeight(true);
	}
}

function getViewport () {
	// return Boostrap viewport size of current window size (useful for changing pinning behaviour and
	// other layout logic based around size of current window)
	const width = Math.max(
		document.documentElement.clientWidth,
		window.innerWidth || 0
	);
	if (width <= 576) return 'xs';
	if (width <= 768) return 'sm';
	if (width <= 992) return 'md';
	if (width <= 1200) return 'lg';
	if (width <= 1400) return 'xl';
	return 'xxl';
}

function setViewPortControl () {
	let viewport = getViewport();
	let debounce;
	$(window).resize(() => {
		debounce = setTimeout(() => {
			const currentViewport = getViewport();
			if (currentViewport !== viewport) {
				viewport = currentViewport;
				$(window).trigger('newViewport', viewport);
			}
		}, 500);
	});
	$(window).on('newViewport', (event, viewport) => {
		// logic to carryout on viewport change
		// set a global variable with current viewport to reference in processing logic
		gblCurrentBootstrapViewPort = viewport;
		// common actions
		common_ViewPortChange();
		// page specific
		if (typeof pageSpecific_ViewPortChange === 'function') {
			pageSpecific_ViewPortChange();
		}
	});
	// run when page loads
	$(window).trigger('newViewport', viewport);
}

function float_fppModalStickyThead () {
	// Fix Table Headers on scroll
	var $previewTable = $('table.schedulePreviewTable');

	$previewTable.floatThead({
		top:
			($('.header').css('position') == 'fixed'
				? $('.header').outerHeight(true)
				: 0) +
			$('#schedulePreview .modal-content .modal-header').outerHeight(true),
		zIndex: 99999,
		debug: false,
		responsiveContainer: function ($previewTable) {
			return $previewTable.closest('.modal-body');
		}
	});
	//TO-DO - currently only working for the one named table, need to change to class based selection for future alternative table use
}

function float_fppStickyThead () {
	//Destroy any existing floatTheads
	for (element in window) {
		if (element.substring(0, 3) == 'ft_') {
			tablename = element.substring(3);
			//console.log('destroying ' + window[element]);
			var reinit = window[element].floatThead('destroy');
			//console.log(reinit);
			delete window[element];
			//remove created thead elements
			$.each($('#' + tablename + ' thead'), function () {
				if ($(this).find('tr.size-row').length > 0) {
					$(this).remove();
				}
			});
		}
	}

	// check if there is at least 1 stickyThead table to process
	if ($('.fppStickyTheadTable').length > 0) {
		if ($('.tab-pane.active.show table.fppStickyTheadTable thead').length > 0) {
			// tables in a tab
			var tablesToProcess = $(
				'.tab-pane.active.show table.fppStickyTheadTable'
			).not('.floatThead-table');
		} else {
			// tables not in tab
			var tablesToProcess = $('.fppStickyTheadTable').not('.floatThead-table');
		}
		// float th thead on all found tables
		$(tablesToProcess).each(function (index, element) {
			var $table = $(element);

			//console.log('ft_' + $table[0].id);

			//only add float if doesn't already have float
			if (
				!window.hasOwnProperty('ft_' + $table[0].id) ||
				$table.floatThead('getRowGroups').length < 2
			) {
				window['ft_' + $table[0].id] = $table.floatThead({
					top: zebraPinSubContentTop,
					position: 'fixed',
					zIndex: 990,
					debug: false,
					autoReflow: true,
					copyTableClass: true,
					responsiveContainer: function ($table) {
						return $table.closest('.fppFThScrollContainer');
					}
				});
			}
			$table.trigger('reflow');
		});
	}
}

function findPrefixedVariable (prefix, context, enumerableOnly) {
	var i = prefix.length;
	context = context || window;
	if (enumerableOnly)
		return Object.keys(context).filter(function (e) {
			return e.slice(0, i) === prefix;
		});
	else
		return Object.getOwnPropertyNames(context).filter(function (e) {
			return e.slice(0, i) === prefix;
		});
}

function sortHTMLSelectByText (selector, skip_first, sortAscending) {
	var options = skip_first
		? $(selector + ' option:not(:first)')
		: $(selector + ' option');
	var arr = options
		.map(function (_, o) {
			return { t: $(o).text(), v: o.value, s: $(o).prop('selected') };
		})
		.get();
	if (sortAscending) {
		arr.sort(function (o1, o2) {
			var t1 = o1.t.toLowerCase(),
				t2 = o2.t.toLowerCase();
			return t1 > t2 ? 1 : t1 < t2 ? -1 : 0;
		});
	} else {
		arr.sort(function (o1, o2) {
			var t1 = o1.t.toLowerCase(),
				t2 = o2.t.toLowerCase();
			return t2 > t1 ? 1 : t2 < t1 ? -1 : 0;
		});
	}
	options.each(function (i, o) {
		o.value = arr[i].v;
		$(o).text(arr[i].t);
		if (arr[i].s) {
			$(o).attr('selected', 'selected').prop('selected', true);
		} else {
			$(o).removeAttr('selected');
			$(o).prop('selected', false);
		}
	});
}

function getManualLink () {
	return 'https://manual.falconplayer.com';
}

function CloseModalDialog (id) {
	const myModal = bootstrap.Modal.getInstance(document.getElementById(id));
	myModal.hide();
}
function EnableModalDialogCloseButton (id) {
	$('#' + id)
		.find('#modalCloseButton')
		.prop('disabled', false);
	// Flip the footer progress button from the disabled "Please Wait" state
	// back to an enabled "Close". No-op for modals without this button.
	$('#' + id + 'CloseButton')
		.prop('disabled', false)
		.text('Close');
}
// Generic StreamURL done/error callback for progress dialogs opened via
// DisplayProgressDialog. StreamURL invokes the callback with the output
// element id, which is always "<modalId>Text"; strip the trailing "Text"
// to recover the modal id and enable its Close button. Pass this instead of
// writing a per-page wrapper whose only job is to enable the Close button.
function ProgressDialogDone (textId) {
	EnableModalDialogCloseButton(('' + textId).replace(/Text$/, ''));
}
// Update the header/title of a progress dialog (the `id` passed to
// DisplayProgressDialog) with a concise, always-visible status line while the
// dialog's textarea keeps scrolling the verbose log. Safe to call before the
// dialog exists (no-op). Useful for long-running operations -- reinstall all
// plugins, FPP/OS upgrades, package installs, etc.
function SetProgressDialogStatus (id, status) {
	$('#' + id + ' .modal-title').text(status);
}

// Extract the most recent progress "stage" declared by a streamed operation via
// logStage() (scripts/common) -- section-header lines of the form
// "===== <message> =====". Returns the latest stage message, or '' if none seen
// yet. This lets any streaming dialog show "what's happening now" without
// pattern-matching incidental log wording: scripts declare their stages, and any
// script that calls logStage participates automatically. Pair with StreamURL's
// dataCallback. The required interior spaces mean a plain "=====...=====" rule
// (no spaces) is NOT matched, only genuine logStage headers.
function ParseLastStageMarker (text) {
	var re = /^===== (.+?) =====\s*$/gm;
	var m,
		last = '';
	while ((m = re.exec(text)) !== null) {
		last = m[1];
	}
	return last;
}
function DoModalDialog (options) {
	var dlg = $('#' + options.id);
	var isNewDialog = dlg.length == 0;
	if (isNewDialog) {
		dlg = $('#modalDialogBase').clone();
		dlg.attr('id', options.id);
		if (options.hasOwnProperty('class')) {
			dlg.addClass(options.class);
		}
		$('#modalDialogBase').parent().append(dlg);

		if (options.open && typeof options.open === 'function') {
			dlg.on('show.bs.modal', function () {
				options.open.call(self);
			});
		}
		if (options.close && typeof options.close === 'function') {
			dlg.on('hide.bs.modal', function () {
				options.close.call(self);
			});
		}

		if (typeof options.body !== 'string') {
			dlg.find('.modal-body').append(options.body);
			options.body.removeClass('hidden');
		}
		if (typeof options.title !== 'string') {
			dlg.find('.modal-title').append(options.title);
		}
	}

	// (Re)build the footer whenever the caller supplies buttons/footer. This runs
	// on first creation AND on reuse of a same-id dialog: previously the footer
	// was wired only inside the creation block, so reopening a reused dialog kept
	// the FIRST invocation's button labels and click handlers (title/body did
	// refresh, the footer did not) -- e.g. a shared confirm dialog would fire the
	// first item's action. Rebuilding here fixes that app-wide. Body-only updaters
	// (progress dialogs pass neither buttons nor footer) leave the footer untouched.
	if (options.hasOwnProperty('footer') || options.hasOwnProperty('buttons')) {
		var $footer = dlg.find('.modal-footer');
		if ($footer.length == 0) {
			$footer = $('<div class="modal-footer"></div>').appendTo(dlg.find('.modal-content'));
		}
		$footer.html(options.footer || '');
		$.each(options.buttons, function (buttonKey, buttonProps) {
			var buttonId = '';
			var buttonText = buttonKey;
			var handleClick = buttonProps;
			var buttonClass = 'buttons';
			var buttonStyle = '';
			var buttonEnabled = '';
			if (typeof buttonProps === 'object') {
				if (buttonProps.click) {
					handleClick = buttonProps.click;
				}
				if (buttonProps.id) {
					buttonId = ' id="' + buttonProps.id + '"';
				}
				if (buttonProps.text) {
					buttonText = buttonProps.text;
				}
				if (buttonProps.class) {
					buttonClass += ' ' + buttonProps.class;
				}
				if (buttonProps.disabled) {
					buttonEnabled = ' disabled';
				}
				if (buttonProps.style) {
					buttonStyle = ' style="' + buttonProps.style + '"';
				}
			}
			var $newButton = $(
				'<button ' +
					buttonId +
					buttonEnabled +
					buttonStyle +
					' class="' +
					buttonClass +
					'">' +
					buttonText +
					'</button>'
			);
			$newButton.on('click', function () {
				handleClick.call(self);
			});
			$footer.append($newButton);
		});
	} else if (isNewDialog) {
		dlg.find('.modal-footer').remove();
	}
	if (options.noClose) {
		dlg.find('#modalCloseButton').prop('disabled', true);
	}

	var focus = options.focus;
	delete options.focus;

	if (typeof options.title === 'string') {
		dlg.find('.modal-title').html(options.title);
	}
	if (typeof options.body === 'string') {
		dlg.find('.modal-body').html(options.body);
	}
	new bootstrap.Modal('#' + options.id, options).show();

	// Namespaced + rebound each call so a reused dialog does not accumulate a fresh
	// shown handler every time it is reopened (which would re-run this N times).
	$('#' + options.id).off('shown.bs.modal.fppDoModal').on('shown.bs.modal.fppDoModal', function () {
		float_fppModalStickyThead();

		// Now that the bootstrap is shown, focus the element if specified
		if (typeof focus === 'function') {
			focus = focus.call(self); // call the function to get the element to focus
		}
		if (typeof focus === 'string') {
			$('#' + focus).focus();
		}
	});
}
function DisplayProgressDialog (id, title) {
	DoModalDialog({
		id: id,
		title: title,
		noClose: true,
		backdrop: 'static',
		keyboard: false,
		body:
			" <textarea style='max-width:100%; max-height:100%; width: 100%; height:100%;' disabled id='" +
			id +
			"Text'></textarea>",
		class: 'modal-dialog-scrollable',
		buttons: {
			Close: {
				text: 'Please Wait',
				disabled: true,
				id: id + 'CloseButton',
				click: function () {
					CloseModalDialog(id);
					location.reload();
				}
			}
		}
	});
}
function DisplayConfirmationDialog (id, title, body, yesFunction) {
	DoModalDialog({
		id: id,
		class: 'modal-m',
		backdrop: true,
		keyboard: true,
		body: body,
		title: title,
		buttons: {
			Yes: function () {
				CloseModalDialog(id);
				yesFunction();
			},
			No: function () {
				CloseModalDialog(id);
			}
		}
	});
}

(function ($) {
	/*  A custom jQuery plugin that uses jQueryUI.Dialog API
    to create equivalent bootstrap modals. */

	$.fn.fppDialog = function (options) {
		if (options == 'close') {
			this.each(function () {
				$(this).modal('hide');
			});
			return this;
		}
		if (options == 'open') {
			this.each(function () {
				$(this).modal('show');
			});
			return this;
		}
		if (options == 'enableClose') {
			this.each(function () {
				$(this).removeClass('no-close');
			});
			return this;
		}
		if (options == 'moveToTop') {
			this.each(function () {
				$(this).modal('show');
			});
			return this;
		}
		if (options == 'option') {
			return this;
		}
		var settings = $.extend(
			{
				title: '',
				dialogClass: '',
				width: null,
				content: null,
				footer: null,
				closeText:
					'<button type="button" class="close" data-bs-dismiss="modal" aria-label="Close"><span aria-hidden="true">&times;</span></button>'
			},
			options
		);

		this.each(function () {
			var $buttons = $(this).find('.modal-footer').html('');
			var $title = '';
			var self = this;
			var modalOptions = {};
			if (settings.dialogClass.split(' ').includes('no-close')) {
				$.extend(modalOptions, { backdrop: 'static' });
			}
			$(this).addClass(settings.dialogClass);
			if (!$(this).hasClass('has-title')) {
				$(this).addClass('has-title');
				var title = settings.title;
				if (title !== '') {
					title = '<h3 class="modal-title">' + settings.title + '</h3>';
				}
				$title = $('<div class="modal-header">' + title + '</div>');
			} else {
				$(this).find('.modal-title').html(settings.title);
			}

			if (settings.buttons) {
				if (!$(this).hasClass('has-buttons')) {
					$(this).addClass('has-buttons');
					if ($(this).hasClass('has-footer')) {
						$(this).removeClass('has-footer');
					} else {
						$buttons = $('<div class="modal-footer"/>');
					}
				}
				$.each(settings.buttons, function (buttonKey, buttonProps) {
					var buttonText = buttonKey;
					var handleClick = buttonProps;
					var buttonClass = 'buttons';
					if (typeof buttonProps === 'object') {
						if (buttonProps.click) {
							handleClick = buttonProps.click;
						}
						if (buttonProps.text) {
							buttonText = buttonProps.text;
						}
						if (buttonProps.class) {
							buttonClass += ' ' + buttonProps.class;
						}
					}
					$newButton = $(
						'<button class="' + buttonClass + '">' + buttonText + '</button>'
					);
					$newButton.on('click', function () {
						handleClick.call(self);
					});
					$buttons.append($newButton);
				});
			} else if (settings.footer) {
				if (!$(this).hasClass('has-footer')) {
					$(this).addClass('has-footer');
					if ($(this).hasClass('has-buttons')) {
						$(this).removeClass('has-buttons');
					} else {
						$buttons = $('<div class="modal-footer"/>');
					}
				}
				$buttons.append(settings.footer);
			}

			if (!$(this).hasClass('modal')) {
				var $dialogBody = $('<div class="modal-body"/>');
				var modalDialogSizeClass = '';
				if (settings.width) {
					if (settings.width < 400) {
						modalDialogSizeClass = 'modal-sm';
					}
					if (settings.width > 500) {
						modalDialogSizeClass = 'modal-lg';
					}
					if (settings.width > 799) {
						modalDialogSizeClass = 'modal-xl';
					}
					if (settings.width > 1099) {
						modalDialogSizeClass = 'modal-xxl';
					}
				}

				var modalDialogClass = 'modal-dialog ' + modalDialogSizeClass;
				var $dialogInner = $('<div class="' + modalDialogClass + '"/>');
				if (settings.height) {
					if (settings.height == '100%') {
						$dialogBody.css({ height: 'calc(100vh - 150px)' });
						$dialogInner.css({ 'margin-top': '10px' });
					} else {
						$dialogBody.height(settings.height);
					}
				}

				$(this).wrapInner($dialogBody);
				$(this).addClass('modal fade');
				$(this).prepend($title);
				$(this).append($buttons);
				if (settings.closeText) {
					if (!$(this).hasClass('has-closeText')) {
						$(this).addClass('has-closeText');
						$title.append(settings.closeText);
					}
				}

				var $dialogContent = $('<div class="modal-content"/>');
				$(this).wrapInner($dialogInner.wrapInner($dialogContent));
				if (settings.content) {
					$dialogBody.html(settings.content);
				}
			}

			if (settings.open && typeof settings.open === 'function') {
				$(this).on('show.bs.modal', function () {
					settings.open.call(self);
				});
			}

			if (settings.close && typeof settings.close === 'function') {
				$(this).on('hide.bs.modal', function () {
					settings.close.call(self);
				});
			}
			$(this).modal(modalOptions).addClass('fppDialog');
		});
		$(this).modal('show');
		return this;
	};
})(jQuery);

function handleKeypress (e) {
	if (e.keyCode == 112) {
		e.preventDefault();
		DisplayHelp();
	}
}
class SwipeHandler {
	constructor (element) {
		this.xDown = null;
		this.yDown = null;
		this.element =
			typeof element === 'string' ? document.querySelector(element) : element;

		this.element.addEventListener(
			'touchstart',
			function (evt) {
				this.xDown = evt.touches[0].clientX;
				this.yDown = evt.touches[0].clientY;
			}.bind(this),
			false
		);
	}
	onLeft (callback) {
		this.onLeft = callback;
		return this;
	}

	onRight (callback) {
		this.onRight = callback;
		return this;
	}

	onUp (callback) {
		this.onUp = callback;
		return this;
	}

	onDown (callback) {
		this.onDown = callback;
		return this;
	}
	handleTouchMove (evt) {
		if (!this.xDown || !this.yDown) {
			return;
		}
		var xUp = evt.touches[0].clientX;
		var yUp = evt.touches[0].clientY;
		this.xDiff = this.xDown - xUp;
		this.yDiff = this.yDown - yUp;

		if (Math.abs(this.xDiff) > Math.abs(this.yDiff)) {
			// Most significant.
			if (this.xDiff > 0) {
				this.onLeft();
			} else {
				this.onRight();
			}
		} else {
			if (this.yDiff > 0) {
				this.onUp();
			} else {
				this.onDown();
			}
		}
		// Reset values.
		this.xDown = null;
		this.yDown = null;
	}

	run () {
		this.element.addEventListener(
			'touchmove',
			function (evt) {
				this.handleTouchMove(evt).bind(this);
			}.bind(this),
			false
		);
	}
}
function CheckBrowser () {
	var ua = window.navigator.userAgent;
	var msie = ua.indexOf('MSIE '); // IE<11
	var trident = ua.indexOf('Trident/'); // IE11
	if (msie > 0 || trident > 0) {
		// IE 10 or older => return version number
		$('#unsupportedBrowser').show();
	} else {
		$('#unsupportedBrowser').hide();
	}
	if (navigator.userAgent.indexOf('Mac') > 0) {
		$('body').addClass('mac-os');
	}
}

/* jQuery helper method to allow for PUT (similar to standard GET/POST)*/
$.put = function (url, data, callback, type) {
	if (typeof data === 'function') {
		(type = type || callback), (callback = data), (data = {});
	} else if (data != undefined && typeof data != 'object') {
		data = JSON.stringify(data);
	}

	return $.ajax({
		url: url,
		type: 'PUT',
		success: callback,
		data: data,
		contentType: type
	});
};

/* jQuery helper method to allow for DELETE (similar to standard GET/POST)*/
$.delete = function (url, data, callback, type) {
	if (typeof data === 'function') {
		(type = type || callback), (callback = data), (data = {});
	} else if (data != undefined && typeof data != 'object') {
		data = JSON.stringify(data);
	}

	return $.ajax({
		url: url,
		type: 'DELETE',
		success: callback,
		data: data,
		contentType: type
	});
};

function PadLeft (string, pad, length) {
	return (new Array(length + 1).join(pad) + string).slice(-length);
}

function SecondsToHuman (seconds, addIdentifiers = false) {
	var m = parseInt(seconds / 60);
	var s = parseInt(seconds % 60);
	var h = parseInt(seconds / 3600);
	if (h > 0) {
		m = m % 60;
		return (
			PadLeft(h, '0', 2) +
			(addIdentifiers ? 'h' : '') +
			':' +
			PadLeft(m, '0', 2) +
			(addIdentifiers ? 'm' : '') +
			':' +
			PadLeft(s, '0', 2) +
			(addIdentifiers ? 's' : '')
		);
	}
	return (
		PadLeft(m, '0', 2) +
		(addIdentifiers ? 'm' : '') +
		':' +
		PadLeft(s, '0', 2) +
		(addIdentifiers ? 's' : '')
	);
}

function versionToNumber (version) {
	// convert a version string like 2.7.1-2-dirty to "20701"
	if (version.charAt(0) == 'v') {
		version = version.substr(1);
	}
	if (version.indexOf('-') != -1) {
		version = version.substr(0, version.indexOf('-'));
	}
	var parts = version.split('.');

	while (parts.length < 3) {
		parts.push('0');
	}
	var number = 0;
	for (var x = 0; x < 3; x++) {
		var val = parseInt(parts[x]);
		if (val >= 9990) {
			return number * 10000 + 9999;
		} else if (val > 99) {
			val = 99;
		}
		number = number * 100 + val;
	}
	return number;
}

function TogglePasswordHideShow (setting) {
	if (setting.indexOf('Verify') > 0) setting = setting.replace(/Verify$/, '');

	if ($('#' + setting).attr('type') == 'text') {
		$('#' + setting).attr('type', 'password');
		$('#' + setting + 'Verify').attr('type', 'password');
		$('#' + setting + 'HideShow').removeClass('fa-eye-slash');
		$('#' + setting + 'VerifyHideShow').removeClass('fa-eye-slash');
		$('#' + setting + 'HideShow').addClass('fa-eye');
		$('#' + setting + 'VerifyHideShow').addClass('fa-eye');
	} else {
		$('#' + setting).attr('type', 'text');
		$('#' + setting + 'Verify').attr('type', 'text');
		$('#' + setting + 'HideShow').removeClass('fa-eye');
		$('#' + setting + 'VerifyHideShow').removeClass('fa-eye');
		$('#' + setting + 'HideShow').addClass('fa-eye-slash');
		$('#' + setting + 'VerifyHideShow').addClass('fa-eye-slash');
	}
}

function ConfirmPasswordEnable () {
	var password = $('#password').val();
	var value = $('#passwordEnable').val();

	// Selecting "Enter a Password" only reveals/seeds the password fields, it must
	// never overwrite an already-saved password with the 'falcon' default: doing
	// that installed a password the user had not chosen and left them having to
	// authenticate with 'falcon' before their own password would be accepted
	// (issue #2829). Seed the fields only when no password has been set yet.
	if (value == '1') {
		if (password == '') {
			password = 'falcon';
			$('#password').val(password);
		}
		$('#passwordVerify').val(password);
		window['passwordChanged']();
		window['passwordVerifyChanged']();
	}

	window['passwordEnableChanged']();

	if (value == '0') {
		$('.passwordEnableChild').hide();
	} else if (value == '1') {
		$('.passwordEnableChild').show();
	}
}

function ValidatePassword (password) {
	// Allow minimum of 6 so default 'falcon' password is valid
	if (password.length < 6) {
		DialogError(
			'Password Length',
			'Password Length should be 6 or more characters'
		);
		return 0;
	}

	return 1;
}

function CheckPassword () {
	var password = $('#password').val();
	var passwordVerify = $('#passwordVerify').val();

	if (password == passwordVerify) {
		if (ValidatePassword(password)) {
			window['passwordVerifyChanged']();
			window['passwordChanged']();
		}
	} else {
		$('#passwordVerify').val('');
		$('#passwordVerify').focus();
	}
}

function CheckPasswordVerify () {
	var password = $('#password').val();
	var passwordVerify = $('#passwordVerify').val();

	if (password == passwordVerify) {
		if (ValidatePassword(password)) {
			window['passwordVerifyChanged']();
			window['passwordChanged']();
		}
	} else {
		$('#password').val('');
		$('#password').focus();
	}
}

function ConfirmOSPasswordEnable () {
	var password = $('#osPassword').val();
	var value = $('#osPasswordEnable').val();

	if ((value == '1' && password == '') || value == '0') {
		$('#osPassword').val('falcon');
		window['osPasswordChanged']();
		$('#osPasswordVerify').val('falcon');
		window['osPasswordVerifyChanged']();
		password = 'falcon';
	}

	window['osPasswordEnableChanged']();

	if (value == '0') {
		$('.osPasswordEnableChild').hide();
	} else if (value == '1') {
		$('.osPasswordEnableChild').show();
	}
}

function CheckOSPassword () {
	var password = $('#osPassword').val();
	var passwordVerify = $('#osPasswordVerify').val();

	if (password == passwordVerify) {
		if (ValidatePassword(password)) {
			window['osPasswordVerifyChanged']();
			window['osPasswordChanged']();
		}
	} else {
		$('#osPasswordVerify').val('');
		$('#osPasswordVerify').focus();
	}
}

function CheckOSPasswordVerify () {
	var password = $('#osPassword').val();
	var passwordVerify = $('#osPasswordVerify').val();

	if (password == passwordVerify) {
		if (ValidatePassword(password)) {
			window['osPasswordVerifyChanged']();
			window['osPasswordChanged']();
		}
	} else {
		$('#osPassword').val('');
		$('#osPassword').focus();
	}
}

function RegexCheckData (regexStr, value, desc, hideValue = false) {
	var regex = new RegExp(regexStr);

	if (regex.test(value)) {
		return true;
	}

	if (hideValue)
		DialogError(
			'Data Format Error',
			'ERROR: The new value does not match proper format: ' + desc
		);
	else
		DialogError(
			'Data Format Error',
			"ERROR: '" + value + "' does not match proper format: " + desc
		);
	return false;
}

// Compare two version numbers
function CompareFPPVersions (a, b) {
	// Turn any non-string version numbers into a string
	a = '' + a;
	b = '' + b;
	a = versionToNumber(a);
	b = versionToNumber(b);

	if (a > b) {
		return 1;
	} else if (a < b) {
		return -1;
	}

	return 0;
}

function Convert24HToUIFormat (tm) {
	var newTime = tm;

	if (tm.indexOf(':') == -1)
		// Sunrise/Sunset/Dusk/Dawn
		return tm;

	var fmt = '%I:%M %p'; // default format in settings.json

	var showingSeconds = false;
	if (
		settings.hasOwnProperty('ScheduleSeconds') &&
		settings['ScheduleSeconds'] == 1
	)
		showingSeconds = true;

	if (settings.hasOwnProperty('TimeFormat')) fmt = settings['TimeFormat'];

	if (fmt == '%H:%M') {
		// if set to use 24H time then just exit
		if (showingSeconds) return tm;
		else return tm.substr(0, 5);
	}

	var parts = tm.split(':');
	var h = parseInt(parts[0]);
	var m = parseInt(parts[1]);
	var s = parseInt(parts[2]);

	var ampm = 'AM';
	var h12 = h;
	if (h == 24) {
		ampm = 'Mid';
		h12 -= 12;
	} else if (h >= 12) {
		ampm = 'PM';
		h12 -= 12;
	}

	if (fmt == '%I:%M %p') {
		newTime = PadLeft(h12, '0', 2) + ':' + PadLeft(m, '0', 2);

		if (showingSeconds) newTime += ':' + PadLeft(s, '0', 2);

		newTime += ' ' + ampm;
	}

	return newTime;
}

function Convert24HFromUIFormat (tm) {
	var newTime = tm;

	if (tm.indexOf(':') == -1)
		// Sunrise/Sunset/Dusk/Dawn
		return tm;

	var fmt = '%I:%M %p'; // default format in settings.json

	var showingSeconds = false;
	if (
		settings.hasOwnProperty('ScheduleSeconds') &&
		settings['ScheduleSeconds'] == 1
	)
		showingSeconds = true;

	if (settings.hasOwnProperty('TimeFormat')) fmt = settings['TimeFormat'];

	if (fmt == '%H:%M') {
		// if set to use 24H time then just exit
		if (showingSeconds) return tm;
		else return tm + ':00';
	}

	var h = 0;
	var m = 0;
	var s = 0;

	if (fmt == '%I:%M %p') {
		var tmp = tm.split(/ /);
		var parts = tmp[0].split(':');
		h = parseInt(parts[0]);
		m = parseInt(parts[1]);

		if (showingSeconds) {
			s = parseInt(parts[2]);
		}
		if (h == 12) {
			h -= 12;
		}
		if (tmp[1] == 'PM' || tmp[1] == 'pm') {
			h += 12;
		} else if (tmp[1] == 'Mid') {
			h += 24;
		}
	}

	newTime =
		PadLeft(h, '0', 2) + ':' + PadLeft(m, '0', 2) + ':' + PadLeft(s, '0', 2);

	return newTime;
}

function InitializeTimeInputs (format = 'H:i:s') {
	$('.time').timepicker({
		timeFormat: format,
		typeaheadHighlight: false
	});
}

function InitializeDateInputs (format = 'yy-mm-dd') {
	$('.date').datepicker({
		changeMonth: true,
		changeYear: true,
		dateFormat: format,
		minDate: new Date(MINYEAR - 1, 1 - 1, 1),
		maxDate: new Date(MAXYEAR, 12 - 1, 31),
		showButtonPanel: true,
		selectOtherMonths: true,
		showOtherMonths: true,
		yearRange: '' + MINYEAR + ':' + MAXYEAR,
		autoclose: true
	});
}

function DeleteSelectedEntries (item) {
	$('#' + item)
		.find('.selectedEntry')
		.remove();
}

function AddTableRowFromTemplate (table) {
	$('#' + table).append(
		$('#' + table)
			.parent()
			.parent()
			.find('.fppTableRowTemplate')
			.find('tr')
			.parent()
			.html()
	);

	return $('#' + table + ' > tr').last();
}

function HandleTableRowMouseClick (event, row) {
	if (
		event.target.nodeName == 'INPUT' ||
		event.target.nodeName == 'TEXTAREA' ||
		event.target.nodeName == 'SELECT' ||
		row.hasClass('unselectableRow')
	)
		return;

	event.preventDefault(); // prevent mouse move from highlighting text

	if (row.hasClass('selectedEntry')) {
		row.removeClass('selectedEntry');
	} else {
		if (event.shiftKey) {
			var na = row.nextAll().length;
			var nl = row.nextUntil('.selectedEntry').length;
			var pa = row.prevAll().length;
			var pl = row.prevUntil('.selectedEntry').length;

			if (pa == pl) pl = -1;

			if (na == nl) nl = -1;

			if (pl >= 0 && nl >= 0) {
				if (nl > pl) {
					row.prevUntil('.selectedEntry').addClass('selectedEntry');
				} else {
					row.nextUntil('.selectedEntry').addClass('selectedEntry');
				}
			} else if (pl >= 0) {
				row.prevUntil('.selectedEntry').addClass('selectedEntry');
			} else if (nl >= 0) {
				row.nextUntil('.selectedEntry').addClass('selectedEntry');
			}
		} else {
			if (!event.ctrlKey) {
				row.parent().find('tr').removeClass('selectedEntry');
			}
		}

		row.addClass('selectedEntry');
	}
}

var StreamScriptStart = "<script class='streamScript'>";
var StreamScriptEnd = '</script>';
var StreamScript = '';
function ProcessStreamedScript (str, allowEmpty = false) {
	if (str.length == 0) return;

	StreamScript += str;
	if (StreamScript != '' || allowEmpty == true) {
		var is = StreamScript.indexOf(StreamScriptStart);
		var ie = StreamScript.indexOf(StreamScriptEnd);
		if (is >= 0 && ie >= 0 && is < ie) {
			// this packet contains the script
			var script = StreamScript.substring(is + StreamScriptStart.length, ie);
			eval(script);
			script = StreamScript.substring(ie + StreamScriptEnd.length);
			StreamScript = '';
			ProcessStreamedScript(script, true);
		}
	}
}

function StreamURL (
	url,
	id,
	doneCallback = '',
	errorCallback = '',
	reqType = 'GET',
	postData = null,
	postContentType = null,
	postProcessData = true,
	raw = false,
	dataCallback = ''
) {
	var last_response_len = false;
	var outputArea = document.getElementById(id);
	var reAddLF = false;

	$.ajax(url, {
		type: reqType,
		contentType: postContentType,
		data: postData,
		processData: postProcessData,
		// Every terminal-output popup in FPP streams through here. Without this,
		// a re-run whose URL happens to match a prior run byte-for-byte (same
		// remote IP, same firmware filename, same version, or a no-param action
		// like kiosk install/disable) can be served straight from the browser's
		// HTTP cache -- the popup opens already "full" of the previous run's
		// output instead of streaming the current one.
		cache: false,
		xhrFields: {
			onprogress: function (e) {
				var this_response,
					response = e.currentTarget.response;
				if (last_response_len === false) {
					this_response = response;
					last_response_len = response.length;
				} else {
					this_response = response.substring(last_response_len);
					last_response_len = response.length;
				}

				if (reAddLF) {
					this_response = '\n' + this_response;
					reAddLF = false;
				}

				if (this_response.endsWith('\n')) {
					this_response = this_response.replace(/\n$/, '');
					reAddLF = true;
				}

				var orig_response = this_response;

				if (
					outputArea.nodeName == 'DIV' ||
					outputArea.nodeName == 'TD' ||
					outputArea.nodeName == 'PRE' ||
					outputArea.nodeName == 'SPAN'
				) {
					if (outputArea.nodeName != 'PRE' && raw == false) {
						this_response = this_response.replace(/(?:\r\n|\r|\n)/g, '<br>');
					}

					outputArea.innerHTML += this_response;
				} else {
					outputArea.value += this_response;
				}

				if (orig_response.includes('<script')) {
					ProcessStreamedScript(orig_response);
				}

				outputArea.scrollTop = outputArea.scrollHeight;
				outputArea.parentElement.scrollTop =
					outputArea.parentElement.scrollHeight;

				// Optional progress hook: hand the caller the full accumulated
				// response so it can scan for phase markers (robust to a marker
				// being split across chunks) and update a status line.
				if (dataCallback != '') {
					window[dataCallback](response);
				}
			}
		}
	})
		.done(function (data) {
			// Because xhrFields.onprogress is not guaranteed to fire on the last chunk
			// any scripts at the end may be missed.  This will execute those, but has
			// the side effecting of running all other streamScripts again.
			$('script.streamScript').each(function () {
				eval($(this).html());
			});
			if (doneCallback != '') {
				window[doneCallback](id);
			}
		})
		.fail(function (data) {
			if (errorCallback != '') {
				window[errorCallback](id);
			}
		});
}

function PostPutHelper (url, async, data, silent, type) {
	var result = {};

	$.ajax({
		url: url,
		type: type,
		contentType: 'application/json',
		data: data,
		async: async,
		dataType: 'json',
		success: function (data) {
			result = data;
		},
		error: function () {
			if (!silent) {
				$.jGrowl('Error with ' + type + ' to ' + url, { themeState: 'danger' });
			}
		}
	});

	return result;
}

function Post (url, async, data, silent = false) {
	return PostPutHelper(url, async, data, silent, 'POST');
}

function Put (url, async, data, silent = false) {
	return PostPutHelper(url, async, data, silent, 'PUT');
}

function Delete (url, async, data, silent = false) {
	return PostPutHelper(url, async, data, silent, 'DELETE');
}

function Get (url, async, silent = false) {
	var result = {};

	$.ajax({
		url: url,
		type: 'GET',
		async: async,
		dataType: 'json',
		success: function (data) {
			result = data;
		},
		error: function () {
			if (!silent)
				$.jGrowl('Error: Unable to get ' + url, { themeState: 'danger' });
		}
	});

	return result;
}

function GetSync (url) {
	return Get(url, false);
}

function GetAsync (url) {
	return Get(url, true);
}

function SetElementValue (elem, val) {
	if ($(elem)[0].tagName == 'INPUT' || $(elem)[0].tagName == 'SELECT') {
		$(elem).val(val);
	} else {
		$(elem).html(val);
	}
}

function GetItemCount (url, id, key = '') {
	$.ajax({
		url: url,
		type: 'GET',
		dataType: 'json',
		success: function (data) {
			if (key != '') SetElementValue($('#' + id), data[key].length);
			else SetElementValue($('#' + id), data.length);
		},
		error: function () {
			SetElementValue($('#' + id), '0');
		}
	});
}

function SetupToolTips (delay = 100) {
	var titles = document.querySelectorAll('[title]');

	titles.forEach(value => {
		var title = value.title;
		value.setAttribute('data-bs-tooltip-title', title);
		value.setAttribute('data-bs-toggle', 'tooltip');
		value.setAttribute('data-bs-placement', 'auto');
		delete value.title;
	});

	const tooltipTriggerList = document.querySelectorAll(
		'[data-bs-toggle="tooltip"]'
	);
	const tooltipList = [...tooltipTriggerList].map(tooltipTriggerEl => {
		let tooltipInstance;
		try {
			// Bootstrap treats an empty data-bs-title attribute as null and
			// throws while type-checking the config. A single bad tooltip
			// must not abort the loop (and with it the rest of page setup,
			// such as binding the mobile nav menu), so guard each one.
			tooltipInstance = new bootstrap.Tooltip(tooltipTriggerEl);
		} catch (e) {
			console.warn('Skipping invalid tooltip', tooltipTriggerEl, e);
			return null;
		}

		// Auto-hide tooltip after 3 seconds if mouse is not hovering
		tooltipTriggerEl.addEventListener('mouseenter', () => {
			clearTimeout(tooltipTriggerEl.tooltipTimeout);
		});

		tooltipTriggerEl.addEventListener('mouseleave', () => {
			tooltipTriggerEl.tooltipTimeout = setTimeout(() => {
				if (!tooltipTriggerEl.matches(':hover')) {
					tooltipInstance.hide();
				}
			}, 3000);
		});

		return tooltipInstance;
	});
}
function SetHomepageStatusRowWidthForMobile () {
	if ($('.statusDivTopRow').length > 0) {
		if ($(window).width() < 481) {
			var statusWidth = 0;
			$('.statusDivTopCol').each(function () {
				statusWidth += $(this).outerWidth(true);
			});
			$('.statusDivTopRow').css('width', statusWidth + 40);
		} else {
			$('.statusDivTopRow').css('width', '');
		}
	}
}
function ShowTableWrapper (tableName) {
	if (
		$('#' + tableName)
			.parent()
			.parent()
			.hasClass('fppTableWrapperAsTable')
	)
		$('#' + tableName)
			.parent()
			.parent()
			.attr('style', 'display: table');
	else
		$('#' + tableName)
			.parent()
			.parent()
			.show();
}

function HideTableWrapper (tableName) {
	$('#' + tableName)
		.parent()
		.parent()
		.hide();
}

function ShowPlaylistDetails () {
	$('#playlistDetailsWrapper').show();
	$('#btnShowPlaylistDetails').hide();
	$('#btnHidePlaylistDetails').show();
}

function HidePlaylistDetails () {
	$('#playlistDetailsWrapper').hide();
	$('#btnShowPlaylistDetails').show();
	$('#btnHidePlaylistDetails').hide();
}

function PopulateLists (options) {
	var onPlaylistArrayLoaded = function () {};
	if (options && typeof options.onPlaylistArrayLoaded === 'function') {
		onPlaylistArrayLoaded = options.onPlaylistArrayLoaded;
	}
	DisableButtonClass('playlistEditButton');
	//PlaylistTypeChanged();
	PopulatePlaylists(false, { onPlaylistArrayLoaded: onPlaylistArrayLoaded });
}

function PlaylistEntryTypeToString (type) {
	switch (type) {
		case 'both':
			return 'Seq+Med';
		case 'branch':
			return 'Branch';
		case 'command':
			return 'Command';
		case 'dynamic':
			return 'Dynamic';
		case 'event':
			return 'Event';
		case 'image':
			return 'Image';
		case 'media':
			return 'Media';
		case 'mqtt':
			return 'MQTT';
		case 'pause':
			return 'Pause';
		case 'playlist':
			return 'Playlist';
		case 'plugin':
			return 'Plugin';
		case 'remap':
			return 'Remap';
		case 'script':
			return 'Script';
		case 'sequence':
			return 'Sequence';
		case 'url':
			return 'URL';
		case 'volume':
			return 'Volume';
	}
}

function psiDetailsBegin () {
	return "<div class='psiDetailsWrapper'><div class='psiDetails'>";
}

function psiDetailsArgBegin () {
	return "<div class='psiDetailsArg'>";
}

function psiDetailsHeader (text) {
	return "<div class='psiDetailsHeader'>" + text + ':</div>';
}

function psiDetailsData (name, value, units = '', hide = false) {
	var str = '';
	var style = '';

	if (hide) style = " style='display: none;'";

	if (typeof value === 'string' || value instanceof String) {
		value = value.replace(/&/g, '&amp;').replace(/</g, '&lt;');
	}
	if (units == '') {
		return (
			"<div class='psiDetailsData field_" +
			name +
			"'" +
			style +
			'>' +
			value +
			'</div>'
		);
	}

	return (
		"<div class='psiDetailsData'><span class='field_" +
		name +
		"'" +
		style +
		'>' +
		value +
		'</span> ' +
		units +
		'</div>'
	);
}

function psiDetailsArgEnd () {
	return '</div>';
}

function psiDetailsLF () {
	return "<div class='psiDetailsLF'></div>";
}

function psiDetailsEnd () {
	return '</div></div>';
}

function psiDetailsForEntrySimpleBranch (entry, editMode) {
	var result = '';

	switch (entry.branchTest) {
		case 'Time':
			result += 'Time: ' + entry.startTime + ' < X < ' + entry.endTime;
			break;
		case 'Loop':
			result +=
				'Loop: Every ' +
				entry.iterationCount +
				' iterations starting at ' +
				entry.iterationStart;
			break;
		case 'MQTT':
			result +=
				'MQTT: Topic: "' +
				entry.mqttTopic +
				'", Message: "' +
				entry.mqttMessage;
			break;
	}

	result += psiDetailsBranchDestination(entry);

	return result;
}

function psiDetailsForEntrySimple (entry, editMode) {
	var pet = playlistEntryTypes[entry.type];
	var result = '';
	var keys = Object.keys(pet.args);
	for (var i = 0; i < keys.length; i++) {
		var a = pet.args[keys[i]];

		if ((!a.hasOwnProperty('simpleUI') || !a.simpleUI) && a.name != 'args') {
			continue;
		}

		if (editMode && a.hasOwnProperty('statusOnly') && a.statusOnly == true) {
			continue;
		}

		if (!a.optional || (entry.hasOwnProperty(a.name) && entry[a.name] != '')) {
			var partialResult = '';

			if (a.type == 'args') {
				if (entry[a.name].length == 1 && !isNaN(parseFloat(entry[a.name][0]))) {
					partialResult += entry[a.name][0];
				} else {
					for (var x = 0; x < entry[a.name].length; x++) {
						if (partialResult != '') partialResult += ' ';

						partialResult += '"' + entry[a.name][x] + '"';
					}
				}
			} else if (a.type == 'array') {
				var akeys = Object.keys(entry[a.name]);
				if (akeys.length == 1 && !isNaN(parseFloat(entry[a.name][akeys[0]]))) {
					partialResult += entry[a.name][akeys[0]];
				} else {
					for (var x = 0; x < akeys.length; x++) {
						if (partialResult != '') partialResult += ' ';

						partialResult += '"' + entry[a.name][akeys[x]] + '"';
					}
				}
			} else {
				if (a.hasOwnProperty('contents')) {
					var ckeys = Object.keys(a.contents);
					for (var x = 0; x < ckeys.length; x++) {
						if (a.contents[ckeys[x]] == entry[a.name]) {
							partialResult += ckeys[x];
						}
					}
				} else if (
					typeof entry[a.name] === 'string' ||
					entry[a.name] instanceof String
				) {
					partialResult += entry[a.name]
						.replace(/&/g, '&amp;')
						.replace(/</g, '&lt;');
				} else {
					partialResult += entry[a.name];
				}

				if (a.hasOwnProperty('unit')) {
					partialResult += ' ' + a.unit;
				}
			}

			if (partialResult != '') {
				if (result != '') result += ' <b>|</b> ';

				result += partialResult;
			}
		}
	}

	result += '<br>';

	return result;
}

function psiDetailsForEntry (entry, editMode) {
	var pet = playlistEntryTypes[entry.type];
	var result = '';

	result += psiDetailsBegin();

	var children = [];
	var childrenToShow = [];
	var divs = 0;
	var keys = Object.keys(pet.args);
	for (var i = 0; i < keys.length; i++) {
		var a = pet.args[keys[i]];

		if (editMode && a.hasOwnProperty('statusOnly') && a.statusOnly == true) {
			continue;
		}

		if (children.includes(a.name) && !childrenToShow.includes(a.name)) {
			continue;
		}

		if (!a.optional && !entry.hasOwnProperty(a.name)) {
			if (a.hasOwnProperty('default')) {
				entry[a.name] = a.default;
			} else {
				if (a.type == 'int') entry[a.name] = 0;
				else if (a.type == 'bool') entry[a.name] = false;
				else entry[a.name] = '';
			}
		}

		if (!a.optional || (entry.hasOwnProperty(a.name) && entry[a.name] != '')) {
			if (typeof a['children'] === 'object') {
				var val = entry[a.name];
				var ckeys = Object.keys(a.children);
				for (var c = 0; c < ckeys.length; c++) {
					for (var x = 0; x < a.children[ckeys[c]].length; x++) {
						if (!children.includes(a.children[ckeys[c]][x]))
							children.push(a.children[ckeys[c]][x]);

						if (val == ckeys[c]) {
							childrenToShow.push(a.children[ckeys[c]][x]);
						}
					}
				}
			}

			if (i > 0) result += psiDetailsLF();

			if (a.type == 'args') {
				for (var x = 0; x < entry[a.name].length; x++) {
					if (x > 0) result += psiDetailsLF();

					result += psiDetailsArgBegin();
					result += psiDetailsHeader('Arg #' + (x + 1));
					result += psiDetailsData(a.name + '_' + (x + 1), entry[a.name][x]);
					result += psiDetailsArgEnd();
				}
			} else if (a.type == 'array') {
				var keys = Object.keys(entry[a.name]);
				for (var x = 0; x < keys.length; x++) {
					if (x > 0) result += psiDetailsLF();

					result += psiDetailsArgBegin();
					result += psiDetailsHeader('Extra Data #' + (x + 1));
					result += psiDetailsData(a.name + '_' + (x + 1), entry[a.name][x]);
					result += psiDetailsArgEnd();
				}
			} else {
				var units = '';
				if (a.hasOwnProperty('unit')) {
					units = a.unit;
				}

				result += psiDetailsArgBegin();
				result += psiDetailsHeader(a.description);

				if (a.hasOwnProperty('contents')) {
					result += psiDetailsData(a.name, entry[a.name], '', true);

					var ckeys = Object.keys(a.contents);
					for (var x = 0; x < ckeys.length; x++) {
						if (a.contents[ckeys[x]] == entry[a.name]) {
							result += ckeys[x] + ' ' + units;
						}
					}
				} else {
					result += psiDetailsData(a.name, entry[a.name], units);
				}

				result += psiDetailsArgEnd();
			}
		}
	}

	result += psiDetailsEnd();

	return result;
}

function psiDetailsBranchDestination (entry) {
	var result = '';

	switch (entry.trueNextBranchType) {
		case 'Index':
			result += ', True: Index: ';
			if (entry.trueNextSection != '') {
				result += entry.trueNextSection + '/';
			}
			result += entry.trueNextItem;
			break;
		case 'Offset':
			result += ', True: Offset: ' + entry.trueNextItem;
			break;
		case 'Playlist':
			result += ', True: Call Playlist: "' + entry.trueBranchPlaylist + '"';
			break;
	}

	switch (entry.falseNextBranchType) {
		case 'Index':
			result += ', False: Index: ';
			if (entry.falseNextSection != '') {
				result += entry.falseNextSection + '/';
			}
			result += entry.falseNextItem;
			break;
		case 'Offset':
			result += ', False: Offset: ' + entry.falseNextItem;
			break;
		case 'Playlist':
			result += ', False: Call Playlist: "' + entry.falseBranchPlaylist + '"';
			break;
	}

	return result;
}

function psiDetailsForEntryBranch (entry, editMode) {
	var result = '';

	result += psiDetailsBegin();

	var branchStr = '';
	if (entry.branchTest == 'Time') {
		branchStr = entry.startTime + ' < X < ' + entry.endTime;
		branchStr += psiDetailsBranchDestination(entry);
	} else if (entry.branchTest == 'Loop') {
		if (entry.loopTest == 'iteration') {
			branchStr =
				'Every ' +
				entry.iterationCount +
				' iterations starting at ' +
				entry.iterationStart;
			branchStr += psiDetailsBranchDestination(entry);
		}
	} else if (entry.branchTest == 'MQTT') {
		branchStr =
			'MQTT: Topic: "' + entry.mqttTopic + '", Message: "' + entry.mqttMessage;
		branchStr += psiDetailsBranchDestination(entry);
	} else {
		branchStr = 'Invalid Config: ' + JSON.stringify(entry);
	}

	result += psiDetailsHeader('Test');
	result += psiDetailsData('test', branchStr);
	result += psiDetailsEnd();

	var keys = Object.keys(entry);
	for (var i = 0; i < keys.length; i++) {
		var a = entry[keys[i]];
		if (keys[i] == 'compInfo') {
			var akeys = Object.keys(a);
			for (var x = 0; x < akeys.length; x++) {
				var aa = entry[keys[i]][akeys[x]];
				result +=
					"<span style='display:none;' class='field_compInfo_" +
					akeys[x] +
					"'>" +
					aa +
					'</span>';
			}
		} else {
			result +=
				"<span style='display:none;' class='field_" +
				keys[i] +
				"'>" +
				a +
				'</span>';
		}
	}

	return result;
}

function VerbosePlaylistItemDetailsToggled () {
	if ($('#verbosePlaylistItemDetails').is(':checked')) {
		$('.psiData').show();
		$('.psiDataSimple').hide();
	} else {
		$('.psiDataSimple').show();
		$('.psiData').hide();
	}

	// The Randomised / Global Pause indicators are tied to this setting too.
	if (typeof window.updateMainPageGlobalPauseIndicator === 'function') {
		window.updateMainPageGlobalPauseIndicator();
	}
	// And the Randomize / Global Pause rows in the playlist details header.
	UpdatePlaylistHeaderDetailVisibility();
}

function GetPlaylistDurationDiv (entry) {
	var h = '';
	var s = 0;

	if (entry.hasOwnProperty('duration') && entry.duration > 0) {
		h = '<b>Length:</b> ' + SecondsToHuman(entry.duration);
		s = entry.duration;
	}

	return (
		"<div class='psiDuration'><span class='humanDuration'>" +
		h +
		"</span><span class='psiDurationSeconds'>" +
		s +
		'</span></div>'
	);
}

// A media / sequence+media entry can carry a companion file that plays on its
// own stream slot at the same time as the entry's main media (see
// PlaylistEntryMedia's ExtraMedia). Nothing else in a playlist row says so, so
// one row would silently be playing two different files. Flags it inline, with
// the file and slot in the badge (and the full detail on hover).
function psiExtraMediaBadge (entry) {
	var extras = [];
	if (typeof entry.extraMediaName == 'string' && entry.extraMediaName != '') {
		extras.push({
			name: entry.extraMediaName,
			slot: entry.extraMediaSlot ? entry.extraMediaSlot : 2
		});
	}
	// Hand-authored playlists can list several companions in an array instead;
	// the editor only ever writes the single flat form above.
	if (Array.isArray(entry.extraMedia)) {
		for (var i = 0; i < entry.extraMedia.length; i++) {
			var e = entry.extraMedia[i];
			if (e && typeof e.mediaName == 'string' && e.mediaName != '') {
				extras.push({ name: e.mediaName, slot: e.slot ? e.slot : 2 });
			}
		}
	}
	if (!extras.length) {
		return '';
	}

	var labels = [];
	var titles = [];
	for (var x = 0; x < extras.length; x++) {
		labels.push(extras[x].name + ' (slot ' + extras[x].slot + ')');
		titles.push(
			'Also plays ' + extras[x].name + ' on stream slot ' + extras[x].slot
		);
	}

	// The pill carries the label only - .badge is uppercased site-wide, which
	// would mangle a filename - so the file itself sits next to it in its own
	// case.
	return (
		"<span class='psiExtraMedia ms-2' title='" +
		titles.join(' | ').replace(/'/g, '&#39;') +
		"'><span class='badge rounded-pill bg-info-subtle text-info-emphasis border border-info-subtle'>" +
		"<i class='fas fa-layer-group me-1'></i>Extra Media</span> <span class='small'>" +
		labels.join(', ').replace(/&/g, '&amp;').replace(/</g, '&lt;') +
		'</span></span>'
	);
}

function GetPlaylistRowHTML (ID, entry, editMode, invalidNames = {}) {
	var HTML = '';
	var rowNum = ID + 1;

	var warningClass = '';
	var warningTitle = '';
	if (entry.type == 'sequence' && entry.sequenceName && invalidNames[entry.sequenceName]) {
		warningClass = ' playlistRowWarning';
		warningTitle = ' title="Missing sequence: ' + entry.sequenceName.replace(/'/g, '&#39;') + '"';
	} else if (entry.type == 'both') {
		if (entry.sequenceName && invalidNames[entry.sequenceName]) {
			warningClass = ' playlistRowWarning';
			warningTitle = ' title="Missing sequence: ' + entry.sequenceName.replace(/'/g, '&#39;') + '"';
		} else if (entry.mediaName && invalidNames[entry.mediaName]) {
			warningClass = ' playlistRowWarning';
			warningTitle = ' title="Missing media: ' + entry.mediaName.replace(/'/g, '&#39;') + '"';
		}
	} else if (entry.type == 'media' && entry.mediaName && invalidNames[entry.mediaName]) {
		warningClass = ' playlistRowWarning';
		warningTitle = ' title="Missing media: ' + entry.mediaName.replace(/'/g, '&#39;') + '"';
	} else if (entry.type == 'playlist' && entry.name && invalidNames[entry.name]) {
		warningClass = ' playlistRowWarning';
		warningTitle = ' title="Missing playlist: ' + entry.name.replace(/'/g, '&#39;') + '"';
	} else if (entry.type == 'image' && entry.imagePath && invalidNames[entry.imagePath]) {
		warningClass = ' playlistRowWarning';
		warningTitle = ' title="Missing image: ' + entry.imagePath.replace(/'/g, '&#39;') + '"';
	}

	if (editMode) {
		HTML += "<tr class='playlistRow" + warningClass + "'" + warningTitle + ">";
		HTML +=
			"<td class='playlistRowCheckCell'><input type='checkbox' class='playlistEntryCheckbox' onchange='UpdatePlaylistSelectCount()' /></td>";
		HTML +=
			"<td class='center' valign='middle'> <div class='rowGrip'><i class='rowGripIcon fpp-icon-grip'></i></div></td>";
	} else {
		HTML += "<tr id='playlistRow" + rowNum + "' class='playlistRow" + warningClass + "'" + warningTitle + ">";
	}

	HTML += "<td class='colPlaylistNumber";

	if (editMode) HTML += ' colPlaylistNumberDrag';

	if (editMode) HTML += " playlistRowNumber'>" + rowNum + '.</td>';
	else
		HTML +=
			" playlistRowNumber' id='colEntryNumber" +
			rowNum +
			"'>" +
			rowNum +
			'.</td>';

	var pet = playlistEntryTypes[entry.type];
	var deprecated = '';

	if (typeof pet.deprecated === 'number' && pet.deprecated == 1) {
		deprecated = "<font color='red'><b>*</b></font>";
		$('#deprecationWarning').show();
	}

	HTML +=
		"<td><div class='psi'><div class='psiHeader' >" +
		PlaylistEntryTypeToString(entry.type) +
		':' +
		deprecated +
		(warningClass ? "<span class='playlistEntryWarningIcon'>&#x26a0;</span>" : "") +
		"<span style='display: none;' class='entryType'>" +
		entry.type +
		"</span></div><div class='psiData'>";

	if (entry.type == 'dynamic') {
		HTML += psiDetailsForEntry(entry, editMode);

		if (entry.hasOwnProperty('dynamic'))
			HTML += psiDetailsForEntry(entry.dynamic, editMode);
	} else if (entry.type == 'branch') {
		HTML += psiDetailsForEntryBranch(entry, editMode);
	} else {
		HTML += psiDetailsForEntry(entry, editMode);
	}

	HTML += '</div>';

	HTML += "<div class='psiDataSimple'";
	if (editMode && typeof entry.note == 'string' && entry.note != '')
		HTML += " title='" + entry.note + "'";
	HTML += '>';

	// Determine display mode (default to argsOnly for backward compatibility)
	var displayMode = entry.displayMode || 'argsOnly';
	var noteText =
		typeof entry.note == 'string' && entry.note != '' ? entry.note : '';

	if (displayMode === 'justNote' && noteText) {
		// Display only the note
		HTML += "<span class='psiNote'>Note: " + noteText + '</span>';
	} else if (displayMode === 'argsAndNote' && noteText) {
		// Display args first, then note
		if (entry.type == 'dynamic') {
			HTML += psiDetailsForEntrySimple(entry, editMode);
			if (entry.hasOwnProperty('dynamic'))
				HTML += psiDetailsForEntrySimple(entry.dynamic, editMode);
		} else if (entry.type == 'branch') {
			HTML += psiDetailsForEntrySimpleBranch(entry, editMode);
		} else {
			HTML += psiDetailsForEntrySimple(entry, editMode);
		}
		HTML += " <span class='psiNote'>Note: " + noteText + '</span>';
	} else {
		// Default: argsOnly - display just the args (current behavior)
		if (entry.type == 'dynamic') {
			HTML += psiDetailsForEntrySimple(entry, editMode);
			if (entry.hasOwnProperty('dynamic'))
				HTML += psiDetailsForEntrySimple(entry.dynamic, editMode);
		} else if (entry.type == 'branch') {
			HTML += psiDetailsForEntrySimpleBranch(entry, editMode);
		} else {
			HTML += psiDetailsForEntrySimple(entry, editMode);
		}
	}
	// Outside the display-mode branches: a companion file is playing whatever
	// the row chooses to show, including in Just Note mode.
	HTML += psiExtraMediaBadge(entry);
	HTML += '</div>';

	HTML += GetPlaylistDurationDiv(entry);
	HTML += '</div></td>';
	if (editMode) {
		HTML += '<td class="playlistRowEditActionCell">';
		HTML +=
			'<button class="circularButton circularEditButton playlistRowEditButton">Edit</button>';
		HTML +=
			'<button class="circularButton circularDeleteButton playlistRowDeleteButton ml-2">Delete</button>';
		HTML += '</td>';
	}
	HTML += '</tr>';

	return HTML;
}

function BranchItemToString (branchType, nextSection, nextIndex) {
	if (typeof branchType == 'undefined') {
		branchType = 'Index';
	}
	if (branchType == 'None') {
		return 'None';
	} else if (branchType == '' || branchType == 'Index') {
		var r = 'Index: ';
		if (nextSection != '') {
			r = r + nextSection + '/';
		}
		r = r + nextIndex;
		return r;
	} else if (branchType == 'Offset') {
		return 'Offset: ' + nextIndex;
	}
}

var oldPlaylistEntryType = '';
function PlaylistTypeChanged () {
	var type = $('#pe_type').val();

	$('.playlistOptions').hide();
	$('#pbody_' + type).show();

	var oldSequence = '';
	if (oldPlaylistEntryType == 'sequence' || oldPlaylistEntryType == 'both') {
		oldSequence = $('.arg_sequenceName').val();
	}

	var oldMedia = '';
	if (oldPlaylistEntryType == 'media' || oldPlaylistEntryType == 'both') {
		oldMedia = $('.arg_mediaName').val();
	}

	DisposeArgHelpTooltips($('#playlistEntryOptions, #playlistEntryCommandOptions, #playlistEntryPropertiesOptions'));
	$('#playlistEntryOptions').html('');
	$('#playlistEntryCommandOptions').html('');
	$('#playlistEntryPropertiesOptions').html('');
	PrintArgInputs('playlistEntryOptions', true, playlistEntryTypes[type].args);

	if (oldPlaylistEntryType == '') {
		// First load on page defaults to 'both'
		if ($('.arg_sequenceName option').length == 0) {
			if ($('.arg_mediaName option').length >= 0) {
				oldPlaylistEntryType = 'both';
				$('#pe_type').val('media');
				PlaylistTypeChanged();
				return;
			}
		} else {
			if ($('.arg_mediaName option').length == 0) {
				oldPlaylistEntryType = 'both';
				$('#pe_type').val('sequence');
				PlaylistTypeChanged();
				return;
			}
		}
	}

	if (type == 'both') {
		$('#autoSelectWrapper').show();
		$('#autoSelectMatches').prop('checked', true);
	}

	if (type == 'both' || type == 'sequence') {
		$('#filterSequencesWrapper').show();
	}

	if (oldSequence != '') {
		$('.arg_sequenceName').val(oldSequence);
	}

	// If no sequence is selected (e.g., old sequence was filtered out), select the first one
	if ($('.arg_sequenceName').length && !$('.arg_sequenceName').val()) {
		$('.arg_sequenceName').prop('selectedIndex', 0);
	}

	if (oldMedia != '') {
		$('.arg_mediaName').val(oldMedia);
	}

	oldPlaylistEntryType = type;
	UpdateChildVisibility();
	MoveEntryPropertiesToBottom();
}

function PlaylistNameOK (name) {
	var tmpName = name.replace(/[^-a-zA-Z0-9_ ]/g, '');
	if (name != tmpName) {
		DialogError(
			'Invalid Playlist Name',
			'You may use only letters, numbers, spaces, hyphens, and underscores in playlist names.'
		);
		return 0;
	}

	return 1;
}

function LoadPlaylistDetails (name) {
	$.get('api/playlist/' + name)
		.done(function (data) {
			var invalidNames = {};
			for (var i = 0; i < playListArray.length; i++) {
				if (playListArray[i].name == name) {
					var msgs = playListArray[i].messages;
					for (var m = 0; m < msgs.length; m++) {
						var msg = msgs[m];
						var match;
						if ((match = msg.match(/^Invalid Sequence (.+)$/))) {
							invalidNames[match[1]] = true;
						} else if ((match = msg.match(/^Invalid mediaName (.+)$/))) {
							invalidNames[match[1]] = true;
						} else if ((match = msg.match(/^Invalid Playlist (.+)$/))) {
							invalidNames[match[1]] = true;
						} else if ((match = msg.match(/^Invalid Image (.+)$/))) {
							invalidNames[match[1]] = true;
						}
					}
					break;
				}
			}
			setTimeout(function () {
				PopulatePlaylistDetails(data, 1, name, invalidNames);
			}, 0);
		})
		.fail(function () {
			DialogError('Error loading playlist', 'Error loading playlist details!');
		});
}

function CreateNewPlaylist () {
	var name = $('#txtNewPlaylistName').val();

	if (!PlaylistNameOK(name)) return;

	for (var i = 0; i < playListArray.length; i++) {
		if (playListArray[i].name == name) {
			DialogError(
				'Playlist name conflict',
				"Found existing playlist named '" +
					name +
					"'.  Loading existing playlist."
			);
			$('#playlistSelect option[value="' + name + '"]').prop('selected', true);
			LoadPlaylistDetails(name);
			return;
		}
	}

	SetPlaylistName(name);
	$('#tblPlaylistLeadIn').html(
		"<tr id='tblPlaylistLeadInPlaceHolder' class='unselectable'><td>&nbsp;</td></tr>"
	);
	$('#tblPlaylistLeadIn').show();
	$('#tblPlaylistLeadInHeader').show();

	$('#tblPlaylistMainPlaylist').html(
		"<tr id='tblPlaylistMainPlaylistPlaceHolder' class='unselectable'><td>&nbsp;</td></tr>"
	);
	$('#tblPlaylistMainPlaylist').show();
	$('#tblPlaylistMainPlaylistHeader').show();

	$('#tblPlaylistLeadOut').html(
		"<tr id='tblPlaylistLeadOutPlaceHolder' class='unselectable'><td>&nbsp;</td></tr>"
	);
	$('#tblPlaylistLeadOut').show();
	$('#tblPlaylistLeadOutHeader').show();

	EnableButtonClass('playlistEditButton');
	DisableButtonClass('playlistExistingButton');
	DisableButtonClass('playlistDetailsEditButton');
}

function EditPlaylist () {
	var name = $('#playlistSelect').val();
	EnableButtonClass('playlistEditButton');
	DisableButtonClass('playlistDetailsEditButton');

	if (typeof ExitPlaylistSelectMode === 'function') {
		ExitPlaylistSelectMode();
	}

	LoadPlaylistDetails(name);
	$('#playlistEditor').addClass('hasPlaylistDetailsLoaded');

	// Push history state so browser back button works
	if (window.location.pathname.includes('playlists.php')) {
		history.pushState(
			{ view: 'editor', playlist: name },
			'',
			window.location.href
		);
	}
}
function SetButtonState (button, state) {
	// Enable Button
	if (state == 'enable') {
		$(button).addClass('buttons').addClass($(button).data('btn-enabled-class'));
		$(button).removeClass('disableButtons');
		$(button).removeClass('disabled');
		$(button).prop('disabled', false);
	} else {
		$(button)
			.removeClass('buttons')
			.removeClass($(button).data('btn-enabled-class'));
		$(button).addClass('disableButtons');
		$(button).attr('disabled', 'disabled');
	}
}

function SetCheckBoxState (checkbox, state) {
	// Enable Checkbox
	if (state == 'enable') {
		$(checkbox).prop('disabled', false);
	} else {
		$(checkbox).prop('disabled', true);
	}
}

function EnableButtonClass (c) {
	$('.' + c).each(function () {
		SetButtonState(this, 'enable');
	});
}

function DisableButtonClass (c) {
	$('.' + c).each(function () {
		SetButtonState(this, 'disable');
	});
}

function RenumberPlaylistEditorEntries () {
	var id = 1;
	var sections = ['LeadIn', 'MainPlaylist', 'LeadOut'];
	for (var s = 0; s < sections.length; s++) {
		var $sectionTable = $('#tblPlaylist' + sections[s]);
		if (!$sectionTable.is(':empty')) {
			$('#tblPlaylist' + sections[s] + ' tr.playlistRow').each(function () {
				$(this)
					.find('.playlistRowNumber')
					.html('' + id + '.');
				id++;
			});
		} else {
			$sectionTable.append(
				"<tr id='tblPlaylist" +
					sections[s] +
					"PlaceHolder' class='unselectable'><td>&nbsp;</td></tr>"
			);
		}
	}
}

function UpdatePlaylistDurations () {
	var sections = ['LeadIn', 'MainPlaylist', 'LeadOut'];
	for (var s = 0; s < sections.length; s++) {
		var duration = 0;
		var totalItemCount = 0;
		var pauseableItemCount = 0; // Items that will have pauses between them

		$('#tblPlaylist' + sections[s] + ' tr.playlistRow').each(function () {
			if ($(this).find('.psiDurationSeconds').length) {
				let current = parseFloat($(this).find('.psiDurationSeconds').html());
				if (isNaN(current)) {
					current = 0.0;
				}
				duration += current;
				totalItemCount++;

				// Count items that aren't already pause entries for global pause calculation
				var entryType = $(this).find('.entryType').html();
				if (entryType && entryType !== 'pause') {
					pauseableItemCount++;
				}
			}
		});

		// Add global pause time for MainPlaylist section
		if (sections[s] == 'MainPlaylist' && pauseableItemCount > 1) {
			// Try to get global pause value from input field, or from global variable if available
			var globalPauseMS =
				parseInt($('#globalPauseBetweenSequences').val()) || 0;
			if (
				globalPauseMS === 0 &&
				typeof window.currentPlaylistGlobalPause !== 'undefined'
			) {
				globalPauseMS = window.currentPlaylistGlobalPause;
			}

			if (globalPauseMS > 0) {
				// Add pause time between pauseable items (pauseableItemCount - 1 pauses between pauseableItemCount items)
				var totalPauseSeconds =
					(globalPauseMS * (pauseableItemCount - 1)) / 1000;
				duration += totalPauseSeconds;
			}
		}

		var items = $('#tblPlaylist' + sections[s] + ' tr.playlistRow').length;
		$('.playlistItemCount' + sections[s]).html(items);
		if (items == 1) items = items.toString() + ' item';
		else items = items.toString() + ' items';

		$('.playlistItemCountWithLabel' + sections[s]).html(items);

		$('.playlistDuration' + sections[s]).html(SecondsToHuman(duration));

		// Store raw duration values for v4 playlist format
		if (sections[s] == 'MainPlaylist') $('#playlistDuration').html(duration);
		if (sections[s] == 'LeadIn') $('#playlistDurationLeadIn').html(duration);
		if (sections[s] == 'LeadOut') $('#playlistDurationLeadOut').html(duration);
	}
}

function GetSequenceDuration (sequence, updateUI, row) {
	var durationInSeconds = 0;
	var file = sequence.replace(/.fseq$/, '');
	$.ajax({
		url: 'api/sequence/' + encodeURIComponent(file) + '/meta',
		type: 'GET',
		async: updateUI,
		dataType: 'json',
		success: function (data) {
			if (data.NumFrames <= 0) {
				row.find('.psiDurationSeconds').html(0);
				row.find('.humanDuration').html('<b>Length: </b>??:??');
				return;
			}

			durationInSeconds = (1.0 * data.NumFrames) / (1000 / data.StepTime);
			if (updateUI) {
				var humanDuration = SecondsToHuman(durationInSeconds);

				row.find('.psiDurationSeconds').html(durationInSeconds);
				row.find('.humanDuration').html('<b>Length: </b>' + humanDuration);

				UpdatePlaylistDurations();
			}
		},
		error: function () {
			durationInSeconds = -1;
			row.find('.humanDuration').html('');
			row
				.find('.psiDataSimple')
				.append(
					'<span style="color: #FF0000; font-weight: bold;">ERROR: Sequence "' +
						sequence +
						'" Not Found</span><br>'
				);
			row
				.find('.psiData')
				.append(
					'<div style="color: #FF0000; font-weight: bold;">ERROR: Sequence "' +
						sequence +
						'" Not Found</div>'
				);
		}
	});

	return durationInSeconds;
}

function SetPlaylistItemMetaData (row) {
	var type = row.find('.entryType').html();
	var file = row.find('.field_mediaName').html();

	if ((type == 'both' || type == 'media') && typeof file != 'undefined') {
		file = $('<div/>').html(file).text(); // handle any & or other chars that got converted
		$.get(
			'api/media/' + encodeURIComponent(file) + '/duration',
			function (mdata) {
				var duration = -1;

				if (
					mdata.hasOwnProperty(file) &&
					mdata[file].hasOwnProperty('duration')
				) {
					duration = mdata[file].duration;
				}

				if (type == 'both') {
					var seq = row.find('.field_sequenceName').text();
					var sDuration = GetSequenceDuration(seq, false, row);

					// Playlist/PlaylistEntryBoth.cpp ends whenever shortest item ends
					if (duration > sDuration || duration < 0) duration = sDuration;
				}

				if (duration > 0) {
					var humanDuration = SecondsToHuman(duration);

					row.find('.psiDurationSeconds').html(duration);
					row.find('.humanDuration').html('<b>Length: </b>' + humanDuration);

					UpdatePlaylistDurations();
				} else {
					row.find('.humanDuration').html('');
				}
			}
		).fail(function () {
			row.find('.humanDuration').html('');
			row
				.find('.psiDataSimple')
				.append(
					'<span style="color: #FF0000; font-weight: bold;">ERROR: Media File "' +
						file +
						'" Not Found</span><br>'
				);
			row
				.find('.psiData')
				.append(
					'<div style="color: #FF0000; font-weight: bold;">ERROR: Media File "' +
						file +
						'" Not Found</div>'
				);

			if (type == 'both')
				GetSequenceDuration(row.find('.field_sequenceName').text(), false, row);
		});
	} else if (type == 'sequence') {
		GetSequenceDuration(row.find('.field_sequenceName').text(), true, row);
	} else if (type == 'image') {
		let file = row.find('.field_imagePath').html();
		if (file.startsWith('/') || file.endsWith('/')) return;
		$.ajax({
			url: 'api/files/images?nameOnly=1',
			type: 'GET',
			async: false,
			dataType: 'json',
			success: function (data) {
				if (!Array.isArray(data) || !data.includes(file)) {
					row
						.find('.psiDataSimple')
						.append(
							'<span style="color: #FF0000; font-weight: bold;">ERROR: Image File "' +
								file +
								'" Not Found</span><br>'
						);
					row
						.find('.psiData')
						.append(
							'<div style="color: #FF0000; font-weight: bold;">ERROR: Image File "' +
								file +
								'" Not Found</div>'
						);
				}
			},
			error: function (...args) {
				DialogError(
					'Failed to Query Image',
					'Error: Unable to query list of images' + show_details(args)
				);
			}
		});
	} else if (type == 'playlist') {
		let playlistName = row.find('.field_name').text();
		$.ajax({
			url: 'api/playlist/' + playlistName,
			type: 'GET',
			async: false,
			dataType: 'json',
			success: function (data) {
				if (!data.hasOwnProperty('name')) {
					row
						.find('.psiDataSimple')
						.append(
							'<span style="color: #FF0000; font-weight: bold;">ERROR: Playlist "' +
								playlistName +
								'" Not Found</span><br>'
						);
				}
				if (data.hasOwnProperty('playlistInfo')) {
					var duration = data.playlistInfo.total_duration;
					var humanDuration = SecondsToHuman(duration);

					row.find('.psiDurationSeconds').html(duration);
					row.find('.humanDuration').html('<b>Length: </b>' + humanDuration);

					UpdatePlaylistDurations();
				}
			},
			error: function () {
				row
					.find('.psiDataSimple')
					.append(
						'<span style="color: #FF0000; font-weight: bold;">ERROR: Loading Playlist "' +
							playlistName +
							'" </span><br>'
					);
			}
		});
	}
}

function PopulatePlaylistItemDuration (row, editMode) {
	var type = row.find('.entryType').html();

	if (!editMode) {
		var duration = row.find('.psiDurationSeconds').html();
		if (duration != '0') return;
	}

	SetPlaylistItemMetaData(row);

	if (type == 'pause') {
		var duration = parseFloat(row.find('.field_duration').html());
		row.find('.psiDurationSeconds').html(duration);
		row
			.find('.humanDuration')
			.html('<b>Length: </b>' + SecondsToHuman(duration));
		UpdatePlaylistDurations();
	}
}

function AddPlaylistEntry (mode) {
	if (mode && !$('#tblPlaylistDetails').find('.playlistSelectedEntry').length) {
		DialogError(
			'No playlist item selected',
			'Error: No playlist item selected.'
		);
		return;
	}

	$('#tblPlaylistMainPlaylistPlaceHolder').remove();

	markCurrentPlaylistModified();

	var type = $('#pe_type').val();
	var pet = playlistEntryTypes[type];

	var pe = {};
	pe.type = type;
	pe.enabled = 1; // no way to disable currently, so force this
	pe.playOnce = 0; // Not currently used by player

	var keys = Object.keys(pet.args);
	for (var i = 0; i < keys.length; i++) {
		var a = pet.args[keys[i]];

		var style = PlaylistEntryArgScope()
			.find('.arg_' + a.name)
			.parent()
			.parent()
			.attr('style');
		if (typeof style != 'undefined' && style.includes('display: none;')) {
			continue;
		}

		if (a.type == 'int') {
			pe[a.name] = parseInt(
				PlaylistEntryArgScope()
					.find('.arg_' + a.name)
					.val()
			);
		} else if (a.type == 'float') {
			pe[a.name] = parseFloat(
				PlaylistEntryArgScope()
					.find('.arg_' + a.name)
					.val()
			);
		} else if (a.type == 'bool') {
			pe[a.name] = PlaylistEntryArgScope()
				.find('.arg_' + a.name)
				.is(':checked')
				? 'true'
				: 'false';
		} else if (a.type == 'time' || a.type == 'date') {
			pe[a.name] = PlaylistEntryArgScope()
				.find('.arg_' + a.name)
				.val();
		} else if (a.type == 'array') {
			var f = {};
			for (x = 0; x < a.keys; x++) {
				f[a.keys[x]] = PlaylistEntryArgScope()
					.find('.arg_' + a.name + '_' + a.keys[x])
					.val();
			}
			pe[a.name] = f;
		} else if (a.type == 'args') {
			var arr = [];
			if (type == 'command') {
				for (var c = 0; c < commandList.length; c++) {
					if (
						commandList[c]['name'] == $('#playlistEntryOptions_arg_1').val()
					) {
						var json = {};
						CommandToJSON(
							'playlistEntryOptions_arg_1',
							'playlistEntryCommandOptions',
							json
						);
						arr = json['args'];
						pe['multisyncCommand'] = json['multisyncCommand'];
						pe['multisyncHosts'] = json['multisyncHosts'];
					}
				}
			} else {
				for (x = 1; x <= 20; x++) {
					if ($('#playlistEntryCommandOptions_arg_' + x).length) {
						arr.push($('#playlistEntryCommandOptions_arg_' + x).val());
					}
				}
			}
			pe[a.name] = arr;
		} else if (a.type == 'string' || a.type == 'file') {
			var inp = PlaylistEntryArgScope().find('.arg_' + a.name);
			var val = inp.val();
			if (val !== undefined) {
				pe[a.name] = val;
			}
		} else {
			pe[a.name] = PlaylistEntryArgScope()
				.find('.arg_' + a.name)
				.html();
		}
	}

	var newRow;
	var html = GetPlaylistRowHTML(0, pe, 1);
	if (mode == 1) {
		// replace
		var row = $('#tblPlaylistDetails').find('.playlistSelectedEntry');
		$(row).after(html);
		$(row).removeClass('playlistSelectedEntry');
		newRow = $(row).next();
		newRow.addClass('playlistSelectedEntry');
		$(row).remove();
	} else if (mode == 2) {
		// insert before
		var row = $('#tblPlaylistDetails').find('.playlistSelectedEntry');
		$(row).before(html);
		$(row).removeClass('playlistSelectedEntry');
		newRow = $(row).prev();
		newRow.addClass('playlistSelectedEntry');
	} else if (mode == 3) {
		// insert after
		var row = $('#tblPlaylistDetails').find('.playlistSelectedEntry');
		$(row).after(html);
		$(row).removeClass('playlistSelectedEntry');
		newRow = $(row).next();
		newRow.addClass('playlistSelectedEntry');
	} else {
		$('#tblPlaylistMainPlaylist').append(html);

		$('#tblPlaylistDetails tr').removeClass('playlistSelectedEntry');

		newRow = $('#tblPlaylistMainPlaylist > tr').last();
		$(newRow).addClass('playlistSelectedEntry');
	}

	RenumberPlaylistEditorEntries();

	PopulatePlaylistItemDuration($(newRow), 1);

	if (type == 'pause') UpdatePlaylistDurations();

	VerbosePlaylistItemDetailsToggled();
}

function GetPlaylistEntry (row) {
	var e = {};
	e.type = $(row).find('.entryType').html();
	e.enabled = 1; // no way to disable currently, so force this
	e.playOnce = 0; // Not currently used by player

	var pet = playlistEntryTypes[e.type];
	var haveDuration = 0;

	var keys = Object.keys(pet.args);
	for (var i = 0; i < keys.length; i++) {
		var a = pet.args[keys[i]];

		if (a.type != 'args' && !$(row).find('.field_' + a.name).length) {
			// handle new fields by using default for fields we can't find
			if (typeof a.default != 'undefined') e[a.name] = a.default;
			continue;
		}

		if (a.type == 'int') {
			e[a.name] = parseInt(
				$(row)
					.find('.field_' + a.name)
					.html()
			);

			if (a.name == 'duration') haveDuration = 1;
		} else if (a.type == 'float') {
			e[a.name] = parseFloat(
				$(row)
					.find('.field_' + a.name)
					.html()
			);

			if (a.name == 'duration') haveDuration = 1;
		} else if (a.type == 'bool') {
			e[a.name] =
				$(row)
					.find('.field_' + a.name)
					.html() == 'true'
					? true
					: false;
		} else if (a.type == 'array') {
			var f = {};
			for (var x = 0; x < a.keys.length; x++) {
				f[a.keys[x]] = parseInt(
					$(row)
						.find('.field_' + a.name + '_' + a.keys[x])
						.html()
				);
			}
			e[a.name] = f;
		} else if (a.type == 'args') {
			var arr = [];
			for (x = 1; x <= 20; x++) {
				if ($(row).find('.field_args_' + x).length) {
					arr.push(
						$(row)
							.find('.field_args_' + x)
							.text()
					);
				}
			}
			e[a.name] = arr;
		} else if (a.type == 'string') {
			var v = $(row)
				.find('.field_' + a.name)
				.text();
			if (parseInt(v) == v) {
				e[a.name] = parseInt(v);
			} else {
				e[a.name] = v;
			}
		} else {
			e[a.name] = $(row)
				.find('.field_' + a.name)
				.text();
		}
	}

	if (!haveDuration && $(row).find('.psiDurationSeconds').html() != '0')
		e['duration'] = parseFloat($(row).find('.psiDurationSeconds').html());

	return e;
}
function AddPlaylist (filter, callback) {
	var name = $('#txtAddPlaylistName').val();
	if (name == '') {
		DialogError('Playlist name cannot be empty');
		return;
	}

	return SavePlaylistAs(name, filter, callback);
}
function SavePlaylist (filter, callback) {
	var name = $('#txtPlaylistName').val();
	if (name == '') {
		DialogError('Playlist name cannot be empty');
		return;
	}

	return SavePlaylistAs(name, filter, callback);
}

function SetPlaylistName (name) {
	if (name) {
		$('#txtPlaylistName').val(name);
		$('#txtPlaylistName').prop('size', name.length);
	}
}

function isCurrentPlaylistModified () {
	return gblCurrentPlaylistModified;
}

function markCurrentPlaylistModified (modified = true) {
	gblCurrentPlaylistModified = modified;
	if (modified) {
		$('.savePlaylistBtnHasChange').show();
	} else {
		$('.savePlaylistBtnHasChange').hide();
	}
}

function updateGlobalPauseIndicator () {
	// Only update the global pause value if the input field exists (on edit pages)
	if ($('#globalPauseBetweenSequences').length) {
		var pauseValue = parseInt($('#globalPauseBetweenSequences').val()) || 0;
		window.currentPlaylistGlobalPause = pauseValue; // Store for duration calculations
	} else {
		// On index page, use existing value from window.currentPlaylistGlobalPause
		var pauseValue = window.currentPlaylistGlobalPause || 0;
	}

	if (pauseValue > 0) {
		$('#globalPauseConfigIndicator').show();
	} else {
		$('#globalPauseConfigIndicator').hide();
	}

	// Update playlist durations to include global pause time
	UpdatePlaylistDurations();
}

function updateAddGlobalPauseIndicator () {
	var pauseValue = parseInt($('#globalPauseAddPlaylist').val()) || 0;
	console.log('Updating add global pause indicator, value:', pauseValue);
	if (pauseValue > 0) {
		$('#globalPauseAddIndicator').show();
		console.log('Showing add global pause indicator');
	} else {
		$('#globalPauseAddIndicator').hide();
		console.log('Hiding add global pause indicator');
	}
}

// Function to manually check and update the main page global pause indicator
window.updateMainPageGlobalPauseIndicator = function () {
	// Only show the Randomised / Global Pause indicators when the user has
	// enabled Verbose Playlist Item Details.
	if (!$('#verbosePlaylistItemDetails').is(':checked')) {
		$('#globalPauseIndicator').hide();
		$('#randomizeIndicator').hide();
		return;
	}

	// Check if a playlist is currently selected (not sequence or media)
	var selectedValue = $('#playlistSelect').val();
	var isPlaylistSelected = false;

	if (selectedValue && playListArray) {
		// Check if the selected value is in the playlist array
		for (var i = 0; i < playListArray.length; i++) {
			if (playListArray[i].name === selectedValue) {
				isPlaylistSelected = true;
				break;
			}
		}
	}

	// Only proceed if a playlist is selected
	if (!isPlaylistSelected) {
		$('#globalPauseIndicator').hide();
		$('#randomizeIndicator').hide();
		return;
	}

	// Helper to update randomize indicator from playlist data
	function updateRandomizeFromPlaylist(playlistData) {
		if (playlistData && playlistData.random && playlistData.random > 0) {
			$('#randomizeIndicator').show();
			if (playlistData.random == 1) {
				$('#randomizeStatus').text('Once at load time');
			} else if (playlistData.random == 2) {
				$('#randomizeStatus').text('Once per iteration');
			}
		} else {
			$('#randomizeIndicator').hide();
		}
	}

	// First try to get the current player status to see if we're actively playing
	$.get('api/player/status', function (statusData) {
		if (
			statusData &&
			statusData.global_pause &&
			statusData.global_pause.configured
		) {
			// We're playing and have global pause info from the player
			$('#globalPauseIndicator').show();
			if (statusData.global_pause.active) {
				$('#globalPauseStatus')
					.removeClass('btn-info btn-warning')
					.addClass('btn-danger')
					.text('Active');
			} else {
				$('#globalPauseStatus')
					.removeClass('btn-danger btn-warning')
					.addClass('btn-info')
					.text('Configured');
			}
		} else {
			// Not playing or no global pause from player, check the selected playlist file directly
			$.get(
				'api/playlist/' + encodeURIComponent(selectedValue),
				function (playlistData) {
					if (
						playlistData &&
						playlistData.globalPauseBetweenSequencesMS &&
						playlistData.globalPauseBetweenSequencesMS > 0
					) {
						$('#globalPauseIndicator').show();
						$('#globalPauseStatus')
							.removeClass('btn-danger btn-warning')
							.addClass('btn-info')
							.text('Configured');
					} else {
						$('#globalPauseIndicator').hide();
					}
					updateRandomizeFromPlaylist(playlistData);
				}
			).fail(function () {
				// If we can't load the playlist, hide the indicators
				$('#globalPauseIndicator').hide();
				$('#randomizeIndicator').hide();
			});
			return;
		}

		// Update randomize from player status
		if (statusData && statusData.random && statusData.random > 0) {
			$('#randomizeIndicator').show();
			if (statusData.random == 1) {
				$('#randomizeStatus').text('Once at load time');
			} else if (statusData.random == 2) {
				$('#randomizeStatus').text('Once per iteration');
			}
		} else {
			// Fall back to playlist data for randomize
			$.get(
				'api/playlist/' + encodeURIComponent(selectedValue),
				function (playlistData) {
					updateRandomizeFromPlaylist(playlistData);
				}
			).fail(function () {
				$('#randomizeIndicator').hide();
			});
		}
	}).fail(function () {
		// If player status fails, try to load the playlist directly
		$.get(
			'api/playlist/' + encodeURIComponent(selectedValue),
			function (playlistData) {
				if (
					playlistData &&
					playlistData.globalPauseBetweenSequencesMS &&
					playlistData.globalPauseBetweenSequencesMS > 0
				) {
					$('#globalPauseIndicator').show();
					$('#globalPauseStatus')
						.removeClass('btn-danger btn-warning')
						.addClass('btn-info')
						.text('Configured');
				} else {
					$('#globalPauseIndicator').hide();
				}
				updateRandomizeFromPlaylist(playlistData);
			}
		).fail(function () {
			console.log('Failed to get playlist data for indicators');
			$('#globalPauseIndicator').hide();
			$('#randomizeIndicator').hide();
		});
	});
};

function SavePlaylistAs (name, options, callback) {
	if (!PlaylistNameOK(name)) return 0;

	var itemCount = 0;
	var pl = {};
	pl.name = name;
	pl.version = 4; // v1 == CSV, v2 == JSON, v3 == deprecated some things, v4 == per-section stats in playlistInfo
	pl.repeat = 0; // currently unused by player
	pl.loopCount = 0; // currently unused by player
	pl.desc = $('#txtPlaylistDesc').val();
	pl.random = parseInt($('#randomizePlaylist').prop('value'));
	pl.globalPauseBetweenSequencesMS =
		parseInt($('#globalPauseBetweenSequences').val()) || 0;
	if (typeof options === 'object') {
		$.extend(pl, options);
	}

	var leadIn = [];
	var mainPlaylist = [];
	var leadOut = [];
	var playlistInfo = {};

	// Collect all playlist entries
	$('#tblPlaylistLeadIn > tr:not(.unselectable)').each(function () {
		leadIn.push(GetPlaylistEntry(this));
	});

	$('#tblPlaylistMainPlaylist > tr:not(.unselectable)').each(function () {
		mainPlaylist.push(GetPlaylistEntry(this));
	});

	$('#tblPlaylistLeadOut > tr:not(.unselectable)').each(function () {
		leadOut.push(GetPlaylistEntry(this));
	});

	// Determine if playlist is empty based on actual content
	pl.empty =
		leadIn.length === 0 && mainPlaylist.length === 0 && leadOut.length === 0;

	if (pl.empty) {
		// v4 format: per-section stats
		playlistInfo.total_duration = parseFloat(0);
		playlistInfo.total_items = 0;
		playlistInfo.leadIn_duration = parseFloat(0);
		playlistInfo.leadIn_items = 0;
		playlistInfo.mainPlaylist_duration = parseFloat(0);
		playlistInfo.mainPlaylist_items = 0;
		playlistInfo.leadOut_duration = parseFloat(0);
		playlistInfo.leadOut_items = 0;
	} else {
		// Calculate durations from collected entries
		var leadInDuration = 0;
		for (var i = 0; i < leadIn.length; i++) {
			if (leadIn[i].hasOwnProperty('duration') && leadIn[i].duration > 0) {
				leadInDuration += leadIn[i].duration;
			}
		}

		var mainDuration = 0;
		for (var i = 0; i < mainPlaylist.length; i++) {
			if (
				mainPlaylist[i].hasOwnProperty('duration') &&
				mainPlaylist[i].duration > 0
			) {
				mainDuration += mainPlaylist[i].duration;
			}
		}

		var leadOutDuration = 0;
		for (var i = 0; i < leadOut.length; i++) {
			if (leadOut[i].hasOwnProperty('duration') && leadOut[i].duration > 0) {
				leadOutDuration += leadOut[i].duration;
			}
		}

		// v4 format: per-section stats
		playlistInfo.leadIn_duration = leadInDuration;
		playlistInfo.leadIn_items = leadIn.length;
		playlistInfo.mainPlaylist_duration = mainDuration;
		playlistInfo.mainPlaylist_items = mainPlaylist.length;
		playlistInfo.leadOut_duration = leadOutDuration;
		playlistInfo.leadOut_items = leadOut.length;
		playlistInfo.total_duration =
			leadInDuration + mainDuration + leadOutDuration;
		playlistInfo.total_items =
			leadIn.length + mainPlaylist.length + leadOut.length;
	}
	pl.leadIn = leadIn;
	pl.mainPlaylist = mainPlaylist;
	pl.leadOut = leadOut;
	pl.playlistInfo = playlistInfo;

	var str = JSON.stringify(pl, true);
	$.ajax({
		url: 'api/playlist/' + name,
		type: 'POST',
		contentType: 'application/json',
		data: str,
		async: false,
		dataType: 'json',
		success: function (data) {
			var rowSelected = $('#tblPlaylistDetails').find(
				'.playlistSelectedEntry'
			).length;

			PopulateLists();
			EnableButtonClass('playlistEditButton');

			if (rowSelected) {
				EnableButtonClass('playlistDetailsEditButton');
			} else {
				DisableButtonClass('playlistDetailsEditButton');
			}

			SetPlaylistName(name);

			if ($('#tblPlaylistDetails').find('.playlistSelectedEntry').length)
				EditPlaylistEntry();

			$.jGrowl('Playlist Saved', { themeState: 'success' });
			markCurrentPlaylistModified(false);
			if (typeof callback === 'function') {
				callback();
			}
		},
		error: function (...args) {
			DialogError(
				'Unable to save playlist',
				'Error: Unable to save playlist.' + show_details(args)
			);
		}
	});

	return 1;
}

function RandomizePlaylistEntries () {
	$('#randomizeBuffer').html($('#tblPlaylistMainPlaylist').html());
	$('#tblPlaylistMainPlaylist').empty();

	var itemsLeft = $('#randomizeBuffer > tr').length;
	while (itemsLeft > 0) {
		var x = Math.floor(Math.random() * Math.floor(itemsLeft)) + 1;
		var item = $('#randomizeBuffer > tr:nth-child(' + x + ')').clone();
		$('#randomizeBuffer > tr:nth-child(' + x + ')').remove();

		$('#tblPlaylistMainPlaylist').append(item);

		itemsLeft = $('#randomizeBuffer > tr').length;
	}

	RenumberPlaylistEditorEntries();

	//    $('.playlistEntriesBody').sortable('refresh').sortable('refreshPositions');
}

function GetTimeZone () {
	// Server-side proxy (api/geoip) - ipapi.co doesn't send
	// Access-Control-Allow-Origin, so the browser can't call it directly.
	$.get('api/geoip')
		.done(function (data) {
			$('#TimeZone').val(data.timezone).change();
		})
		.fail(function () {
			DialogError('Time Zone Lookup', 'Time Zone lookup failed.');
		});
}

function GetGeoLocation () {
	$.get('api/geoip')
		.done(function (data) {
			$('#Latitude').val(data.latitude).change();
			$('#Longitude').val(data.longitude).change();
		})
		.fail(function () {
			DialogError('GeoLocation Lookup', 'GeoLocation lookup failed.');
		});
}

function ViewLatLon () {
	var lat = $('#Latitude').val();
	var lon = $('#Longitude').val();

	var url = 'https://www.google.com/maps/@' + lat + ',' + lon + ',15z';
	window.open(url, '_blank');
}

/**
 * Removes any of the following characters from the supplied name, can be used to cleanse playlist
 * names, event names etc Current needed for example it the case of the scheduler since it is still
 * CSV and commas in a playlist name cause issues Everything is currently replaced with a hyphen ( -
 * )
 *
 * Currently unused in the front-end
 */
function RemoveIllegalChars (name) {
	// , (comma)
	// < (less than)
	// > (greater than)
	// : (colon)
	// " (double quote)
	// / (forward slash)
	// \ (backslash)
	// | (vertical bar or pipe)
	// ? (question mark)
	// * (asterisk)

	var illegalChars = [',', '<', '>', ':', '"', '/', '\\', '|', '?', '*'];

	for (ill_index = 0; ill_index < illegalChars.length; ++ill_index) {
		name = name.toString().replace(illegalChars[ill_index], ' - ');
	}

	return name;
}

function AssignPlaylistEditorFPPCommandArgsFromList (row, c) {
	for (var x = 0; x < commandList[c]['args'].length; x++) {
		var a = commandList[c]['args'][x];
		if (a.type == 'bool') {
			if (
				$(row)
					.find('.field_args_' + (x + 1))
					.html() == 'true'
			)
				$('.arg_' + a.name)
					.prop('checked', true)
					.trigger('change');
			else
				$('.arg_' + a.name)
					.prop('checked', false)
					.trigger('change');
		} else if (a.type == 'int') {
			$('.arg_' + a.name)
				.val(
					parseInt(
						$(row)
							.find('.field_args_' + (x + 1))
							.html()
					)
				)
				.trigger('change');
		} else {
			$('.arg_' + a.name)
				.val(
					$(row)
						.find('.field_args_' + (x + 1))
						.html()
				)
				.trigger('change');
		}
	}
}

function EditPlaylistEntry () {
	if (!$('#tblPlaylistDetails').find('.playlistSelectedEntry').length) {
		DialogError(
			'No playlist item selected',
			'Error: No playlist item selected.'
		);
		return;
	}

	//$("#playlistEntryProperties").get(0).scrollIntoView();

	var row = $('#tblPlaylistDetails').find('.playlistSelectedEntry');
	var type = $(row).find('.entryType').html();
	var pet = playlistEntryTypes[type];

	$('#pe_type').val(type);
	PlaylistTypeChanged();
	EnableButtonClass('playlistEditButton');

	var keys = Object.keys(pet.args);
	for (var i = 0; i < keys.length; i++) {
		var a = pet.args[keys[i]];

		if (a.hidden == true) {
			continue;
		}

		if (a.type == 'bool') {
			if (
				$(row)
					.find('.field_' + a.name)
					.text() == 'true'
			)
				$('.arg_' + a.name)
					.prop('checked', true)
					.trigger('change');
			else
				$('.arg_' + a.name)
					.prop('checked', false)
					.trigger('change');
		} else if (a.type == 'args') {
			if (type == 'command') {
				var pe = GetPlaylistEntry(row);
				PopulateExistingCommand(
					pe,
					'playlistEntryOptions_arg_1',
					'playlistEntryCommandOptions'
				);
			} else {
				for (x = 1; x <= 20; x++) {
					if ($(row).find('.field_args_' + x).length) {
						$('#playlistEntryCommandOptions_arg_' + x).val(
							$(row)
								.find('.field_args_' + x)
								.text()
						);
					}
				}
			}
		} else {
			if ($(row).find('.field_' + a.name).length) {
				var savedVal = $(row)
					.find('.field_' + a.name)
					.text();
				$('.arg_' + a.name).val(savedVal).trigger('change');
				// .arg_command's options are filtered by LoadCommandList to the
				// current UI level; if the saved command is Advanced/Developer
				// tier and we're viewing in a lower UI level, the option won't
				// exist yet and .val() silently leaves nothing selected. Add it
				// back so the saved selection is preserved instead of dropping.
				if (a.name == 'command' && savedVal !== '' && $('.arg_command').val() !== savedVal) {
					$('.arg_command').append(
						"<option value='" + savedVal + "'>" + savedVal + '</option>'
					);
					$('.arg_command').val(savedVal).trigger('change');
				}
			}
		}
	}

	UpdateChildVisibility();
	RevealAdvancedArgsWithValues(pet);
}

// Advanced args are hidden below the advanced UI level.  That is right for an
// entry that doesn't use them, but an entry that already carries a value would
// otherwise hide it with no way to see it, change it, or clear it -- and the
// value is still saved, so it keeps taking effect invisibly.  Reveal the ones
// that are actually set.
function RevealAdvancedArgsWithValues (pet) {
	var keys = Object.keys(pet.args);
	for (var i = 0; i < keys.length; i++) {
		var a = pet.args[keys[i]];
		if (!a.advanced) continue;

		var inp = PlaylistEntryArgScope().find('.arg_' + a.name);
		if (!inp.length) continue;

		var set;
		if (a.type == 'bool') {
			set = inp.is(':checked');
		} else {
			var v = inp.val();
			set = v !== undefined && v !== null && v !== '' && v != '--Default--';
		}

		if (set) inp.closest('tr').removeAttr('style');
	}
}

function RemovePlaylistEntry () {
	if (!$('#tblPlaylistDetails').find('.playlistSelectedEntry').length) {
		DialogError(
			'No playlist item selected',
			'Error: No playlist item selected.'
		);
		return;
	}

	DisableButtonClass('playlistDetailsEditButton');
	$('#tblPlaylistDetails').find('.playlistSelectedEntry').remove();
	RenumberPlaylistEditorEntries();
	UpdatePlaylistDurations();
	markCurrentPlaylistModified();
}

function reloadPage () {
	location.reload(true);
}

function PingIP (ip, count) {
	if (ip == '') return;

	var opts = {
		id: 'pingDialog',
		title: 'Ping ' + ip,
		body:
			"<div id='pingText'>Pinging " +
			ip +
			'<br><br>This will take a few seconds to load</div>',
		backdrop: 'static',
		keyboard: false,
		focus: true
	};

	DoModalDialog(opts);

	$.get('ping.php?ip=' + ip + '&count=' + count)
		.done(function (data) {
			$('#pingText').html(data);
		})
		.fail(function () {
			$('#pingText').html('Error pinging ' + ip);
		});
}

function PingE131IP (id) {
	var ip = $("[name='txtIP[" + id + "]']").val();

	PingIP(ip, 3);
}

function ViewReleaseNotes (version) {
	var opts = {
		id: 'releaseNotesDialog',
		title: 'Release Notes for FPP v' + version,
		body: "<div id='releaseNotesText'>Retrieving Release Notes...</div>",
		class: 'modal-dialog-scrollable',
		backdrop: 'static',
		keyboard: false,
		focus: true
	};

	DoModalDialog(opts);

	$.get('api/system/releaseNotes/' + version)
		.done(function (data) {
			// version is without 'v' prefix (for GitHub API), but UpgradeFPPVersion needs 'v' prefix (for git)
			var gitVersion = version.startsWith('v') ? version : 'v' + version;
			$('#releaseNotesText').html(
				'<center><input onClick=\'UpgradeFPPVersion("' +
					gitVersion +
					"\");' type='button' class='buttons' value='Upgrade'></center>" +
					"<pre style='white-space: pre-wrap; word-wrap: break-word;'>" +
					data.body +
					'</pre>'
			);
		})
		.fail(function () {
			$('#releaseNotesText').html('Error loading release notes.');
		});
}

function VersionUpgradeDone (id) {
	$('#fppUpgradeCloseDialogButton').prop('disabled', false);
}
function UpgradeFPPVersion (newVersion) {
	if (
		confirm(
			'Do you wish to upgrade the Falcon Player?\n\nClick "OK" to continue.\n\nThe system will automatically reboot to complete the upgrade.\nThis can take a long time,  20-30 minutes on slower devices.'
		)
	) {
		CloseModalDialog('releaseNotesDialog');

		var opts = {
			id: 'upgradeFPPDialog',
			title: 'Upgrading to FPP v' + newVersion,
			body: "<textarea class='w-100' style='height: 55vh; min-height: 200px;' disabled id='upgradeFPPDialogText'>Starting upgrade....</textarea>",
			class: 'modal-dialog-scrollable',
			backdrop: 'static',
			keyboard: false,
			noClose: true,
			focus: true,
			footer: ''
		};
		if (settings['Platform'] == 'MacOS') {
			opts['buttons'] = {
				Close: {
					id: 'fppUpgradeCloseDialogButton',
					click: function () {
						CloseModalDialog('upgradeFPPDialog');
					},
					disabled: true,
					class: 'btn-success'
				}
			};
		} else {
			opts['buttons'] = {
				Reboot: {
					id: 'fppUpgradeCloseDialogButton',
					click: function () {
						Reboot();
					},
					disabled: true,
					class: 'btn-success'
				}
			};
		}

		DoModalDialog(opts);
		StreamURL(
			'upgradefpp.php?version=v' + newVersion,
			'upgradeFPPDialogText',
			'VersionUpgradeDone'
		);
	}
}

function ChangeGitBranch (newBranch) {
	if (
		confirm(
			"Are you really sure you want to switch to the '" +
				newBranch +
				"' branch?  This may take some time and it may not be fully compatible with this FPP OS version.  Click 'OK' to continue."
		)
	) {
		var remote = $('#gitRemote').val() || 'origin';
		location.href =
			'changebranch.php?branch=' + newBranch + '&remote=' + remote;
	} else {
		location.reload(true);
	}
}

function RebuildFPPSource () {
	location.href = 'rebuildfpp.php';
}

// Single source of truth for the per-row fields in the universe output/input
// table (co-universes.php). Used by SetUniverseRowInputNames to assign the
// name/id attributes for each control. When adding, removing, or reordering
// columns here, also update:
//   - the <th> list in www/co-universes.php (and channelinputs.php for inputs)
//   - the <td> generator in populateUniverseData()
// The order here does not affect visual column order, only the order fields
// are visited when assigning ids; keeping it aligned with the HTML row order
// makes it easier to scan for consistency.
var UNIVERSE_ROW_FIELDS = [
	'rowGrip',
	'chkActive',
	'txtDesc',
	'universeType',
	'txtIP',
	'txtStartAddress',
	'txtUniverse',
	'numUniverseCount',
	'txtSize',
	'txtPriority',
	'txtSyncUniverse',
	'pacingRate',
	'txtMonitor',
	'txtDeDuplicate'
];

// Per-controller pacing override options for the advanced universe table.
// -1 = "Default" (use the global Pacing setting); 0 = Disabled (line rate);
// any positive value is a Mbps cap.  Kept aligned with the global #E131PacingRate
// options, plus a 30Mbps step for ESP32-class controllers.
var PACING_RATE_OPTIONS = [
	{ value: -1, label: 'Default' },
	{ value: 0, label: 'Disabled' },
	{ value: 30, label: '30 Mbps' },
	{ value: 45, label: '45 Mbps' },
	{ value: 90, label: '90 Mbps' },
	{ value: 200, label: '200 Mbps' },
	{ value: 450, label: '450 Mbps' },
	{ value: 900, label: '900 Mbps' }
];

function PacingRateOptionsHTML (selectedVal) {
	var html = '';
	for (var i = 0; i < PACING_RATE_OPTIONS.length; i++) {
		var o = PACING_RATE_OPTIONS[i];
		html +=
			"<option value='" +
			o.value +
			"'" +
			(o.value == selectedVal ? ' selected' : '') +
			'>' +
			o.label +
			'</option>';
	}
	return html;
}

function SetUniverseCount (input) {
	var txtCount = document.getElementById('txtUniverseCount');
	var count = Number(txtCount.value);
	if (isNaN(count)) {
		count = 8;
	}

	if (count < UniverseCount) {
		while (count < UniverseCount) {
			UniverseSelected = UniverseCount - 1;
			DeleteUniverse(input);
		}
	} else {
		if (UniverseCount == 0) {
			var data = {};
			var channelData = {};
			channelData.enabled = 0;
			channelData.type = 'universes';
			channelData.universes = [];
			var universe = {};
			universe.active = 1;
			universe.description = '';
			universe.id = 1;
			universe.startChannel = 1;
			universe.channelCount = 512;
			universe.type = 1;
			universe.address = '';
			universe.priority = 0;
			universe.monitor = 1;
			universe.deDuplicate = 0;
			channelData.universes.push(universe);
			if (input) {
				data.channelInputs = [];
				data.channelInputs.push(channelData);
			} else {
				data.channelOutputs = [];
				data.channelOutputs.push(channelData);
			}
			populateUniverseData(data, false, input);
		}
		var selectIndex = UniverseCount - 1;
		var universe = Number(
			document.getElementById('txtUniverse[' + selectIndex + ']').value
		);
		var universeType = document.getElementById(
			'universeType[' + selectIndex + ']'
		).value;
		var unicastAddress = document.getElementById(
			'txtIP[' + selectIndex + ']'
		).value;
		var size = Number(
			document.getElementById('txtSize[' + selectIndex + ']').value
		);
		var ucount = Number(
			document.getElementById('numUniverseCount[' + selectIndex + ']').value
		);
		var startAddress = Number(
			document.getElementById('txtStartAddress[' + selectIndex + ']').value
		);
		var active = document.getElementById(
			'chkActive[' + selectIndex + ']'
		).value;
		var priority = Number(
			document.getElementById('txtPriority[' + selectIndex + ']').value
		);
		var monitor = document.getElementById('txtMonitor[' + selectIndex + ']')
			.checked
			? 1
			: 0;
		var deDuplicate = document.getElementById(
			'txtDeDuplicate[' + selectIndex + ']'
		).checked
			? 1
			: 0;

		var tbody = document.getElementById('tblUniversesBody'); // get the table
		var origRow = tbody.rows[selectIndex];
		var origUniverseCount = UniverseCount;
		while (UniverseCount < count) {
			var row = origRow.cloneNode(true);
			tbody.appendChild(row);
			UniverseCount++;
		}
		UniverseCount = origUniverseCount;
		SetUniverseInputNames();
		while (UniverseCount < count) {
			if (universe != 0) {
				universe += ucount;
				document.getElementById('txtUniverse[' + UniverseCount + ']').value =
					universe;
			}
			startAddress += size * ucount;
			document.getElementById('txtStartAddress[' + UniverseCount + ']').value =
				startAddress;

			if (!input) {
				var pingBtn = document
					.getElementById('tblUniversesBody')
					.rows[UniverseCount].querySelector('input.pingButton');
				if (pingBtn) {
					pingBtn.setAttribute('onClick', 'PingE131IP(' + UniverseCount + ');');
				}
			}
			updateUniverseEndChannel(
				document.getElementById('tblUniversesBody').rows[UniverseCount]
			);
			UniverseCount++;
		}
		document.getElementById('txtUniverseCount').value = UniverseCount;
	}
}

// DDP (4 and 5) and Twinkly (8) address a flat channel range rather than a run
// of universes: fppd reads channelCount alone for these and never looks at the
// universe number or the universe count, which is why the UI hides both inputs
// for them.  Named rather than repeated so the three places that have to agree
// on the list cannot drift apart.
function isFlatUniverseType (type) {
	return type == 4 || type == 5 || type == 8;
}

function IPOutputTypeChanged (item, input) {
	var type = $(item).val();
	if (isFlatUniverseType(type)) {
		// DDP, Twinkly
		var univ = $(item).parent().parent().find('input.txtUniverse');
		univ.prop('hidden', true);
		var univc = $(item).parent().parent().find('input.numUniverseCount');
		univc.prop('hidden', true);
		var sz = $(item).parent().parent().find('input.txtSize');
		sz.prop('max', FPPD_MAX_CHANNELS);

		var monitor = $(item).parent().parent().find('input.txtMonitor');
		monitor.prop('hidden', false);

		var universe = $(item).parent().parent().find('input.txtUniverse');
		universe.prop('min', 1);

		$(item).parent().parent().find('input.txtIP').prop('hidden', false);

		if (!input) {
			$(item).parent().parent().find('input.pingButton').prop('hidden', false);
		}
	} else {
		// 0,1 = E1.31, 2,3,9 = Artnet, 6,7 = KiNet
		var univ = $(item).parent().parent().find('input.txtUniverse');
		univ.prop('hidden', false);
		if (type <= 1 && parseInt(univ.val()) < 1) {
			univ.val(1);
		}
		var univc = $(item).parent().parent().find('input.numUniverseCount');
		univc.prop('hidden', false);
		if (parseInt(univc.val()) < 1) {
			univc.val(1);
		}
		var sz = $(item).parent().parent().find('input.txtSize');
		var val = parseInt(sz.val());
		if (val > 512) {
			sz.val(512);
		}
		sz.prop('max', 512);

		if (!input) {
			if (type == 0 || type == 2) {
				$(item).parent().parent().find('input.txtIP').val('');
				$(item).parent().parent().find('input.txtIP').prop('hidden', true);
				$(item).parent().parent().find('input.pingButton').prop('hidden', true);
			} else {
				$(item).parent().parent().find('input.txtIP').prop('hidden', false);
				$(item)
					.parent()
					.parent()
					.find('input.pingButton')
					.prop('hidden', false);
			}

			var monitor = $(item).parent().parent().find('input.txtMonitor');
			if (type == 0 || type == 2) {
				monitor.prop('hidden', true);
				$('#sourceInterfaceDiv').show();
			} else {
				monitor.prop('hidden', false);
				//$('#sourceInterfaceDiv').hide();
			}

			var universe = $(item).parent().parent().find('input.txtUniverse');
			if (type == 2 || type == 3 || type == 9) {
				universe.prop('min', 0);
			} else {
				universe.prop('min', 1);
			}
		}
	}
	var priority = $(item).parent().parent().find('input.txtPriority');
	priority.prop('hidden', type > 1);

	// Sync universe is only supported for E1.31 (types 0 and 1)
	var syncUniv = $(item).parent().parent().find('input.txtSyncUniverse');
	if (type > 1) {
		syncUniv.val(0);
		syncUniv.prop('hidden', true);
	} else {
		syncUniv.prop('hidden', false);
	}

	// Pacing only applies to unicast destinations; multicast/broadcast share a
	// socket carrying the aggregate of all their controllers and aren't paced.
	var pacing = $(item).parent().parent().find('select.pacingRate');
	pacing.prop('disabled', type == 0 || type == 2);
}

function updateUniverseEndChannel (row) {
	var startChannel = parseInt($(row).find('input.txtStartAddress').val());
	var count = parseInt($(row).find('input.numUniverseCount').val());
	var size = parseInt($(row).find('input.txtSize').val());
	var end = startChannel + count * size - 1;

	$(row).find('span.numEndChannel').html(end);
}

function populateUniverseData (data, reload, input) {
	var bodyHTML = '';
	UniverseCount = 0;
	var inputStyle = '';
	var inputStr = 'Output';
	var anyEnabled = 0;

	// Incase none found
	var channelData = { universes: [] };

	if (input && 'channelInputs' in data) {
		channelData = data.channelInputs[0];
	} else if ('channelOutputs' in data) {
		channelData = data.channelOutputs[0];
	}

	// Ensure universes array exists
	if (!channelData || !channelData.universes) {
		channelData = { universes: [] };
	}

	if (input) {
		inputStr = 'Input';
		inputStyle = "style='display: none;'";
	} else {
		if (channelData.hasOwnProperty('interface')) {
			$('#selE131interfaces').val(channelData.interface).prop('selected', true);
		}
		if (channelData.hasOwnProperty('threaded')) {
			$('#E131ThreadedOutput').val(channelData.threaded).prop('selected', true);
		}
		if (channelData.hasOwnProperty('pacingRate')) {
			$('#E131PacingRate').val(channelData.pacingRate).prop('selected', true);
		}
		UpdateSendingModeOptions();
	}
	UniverseCount = channelData.universes.length;
	var hasMCBC = false;
	for (var i = 0; i < channelData.universes.length; i++) {
		var universe = channelData.universes[i];
		var active = universe.active;
		var desc = universe.description;
		var uid = universe.id;
		var ucount = universe.universeCount;
		if (!ucount) {
			ucount = 1;
		}
		var startAddress = universe.startChannel;
		var size = universe.channelCount;
		var type = universe.type;
		var unicastAddress = universe.address;
		var priority = universe.priority;
		var syncUniverse = universe.syncUniverse ? universe.syncUniverse : 0;
		unicastAddress = unicastAddress.trim();
		var endChannel = universe.startChannel + ucount * size - 1;

		var activeChecked = active == 1 ? 'checked="checked"' : '';
		var typeMulticastE131 = type == 0 ? 'selected' : '';
		var typeUnicastE131 = type == 1 ? 'selected' : '';
		var typeBroadcastArtNet = type == 2 ? 'selected' : '';
		var typeUnicastArtNet = type == 3 ? 'selected' : '';
		var typeDDPR = type == 4 ? 'selected' : '';
		var typeDDP1 = type == 5 ? 'selected' : '';
		var typeKiNet1 = type == 6 ? 'selected' : '';
		var typeKiNet2 = type == 7 ? 'selected' : '';
		var typeTwinkly = type == 8 ? 'selected' : '';
		var typeUniqueArtNet = type == 9 ? 'selected' : '';
		var monitor = 1;
		if (universe.monitor != null) {
			monitor = universe.monitor;
		}
		var deDuplicate = 0;
		if (universe.deDuplicate != null) {
			deDuplicate = universe.deDuplicate;
		}
		// per-controller pacing override; absent means "Default" (global rate)
		var pacingRateVal = universe.pacingRate != null ? universe.pacingRate : -1;

		var universeSize = 512;
		var universeCountDisable = '';
		var universeNumberDisable = '';
		var monitorDisabled = '';
		var ipDisabled = '';
		if (isFlatUniverseType(type)) {
			universeSize = FPPD_MAX_CHANNELS;
			universeCountDisable = ' disabled';
			universeNumberDisable = ' disabled';
		}
		if (type == 0 || type == 2) {
			monitorDisabled = ' disabled';
			hasMCBC = true;
		}
		var minNum = 1;
		if (type == 2 || (type == 3) | (type == 9)) {
			minNum = 0;
		}
		if (type == 0 || type == 2) {
			ipDisabled = ' disabled';
			unicastAddress = '';
		}

		anyEnabled |= active == 1;

		bodyHTML +=
			'<tr>' +
			'<td valign="middle">  <div class="rowGrip"> <i class="rowGripIcon fpp-icon-grip"></i> </div> </td>' +
			"<td><span class='rowID' id='rowID'>" +
			(i + 1).toString() +
			'</span></td>' +
			"<td><input class='chkActive' type='checkbox' " +
			activeChecked +
			'/></td>' +
			"<td><input class='txtDesc' type='text' size='24' maxlength='64' value='" +
			desc +
			"'/></td>";
		bodyHTML += "<td><select class='form-select universeType'";

		if (input) {
			bodyHTML +=
				'>' +
				"<option value='0' " +
				typeMulticastE131 +
				'>E1.31 - Multicast</option>' +
				"<option value='1' " +
				typeUnicastE131 +
				'>E1.31 - Unicast</option>' +
				"<option value='2' " +
				typeBroadcastArtNet +
				'>ArtNet</option>';
		} else {
			bodyHTML +=
				" onChange='IPOutputTypeChanged(this, " +
				input +
				");'>" +
				"<option value='0' " +
				typeMulticastE131 +
				'>E1.31 - Multicast</option>' +
				"<option value='1' " +
				typeUnicastE131 +
				'>E1.31 - Unicast</option>' +
				"<option value='2' " +
				typeBroadcastArtNet +
				'>ArtNet - Broadcast</option>' +
				"<option value='3' " +
				typeUnicastArtNet +
				'>ArtNet - Unicast/ArtNet Port</option>' +
				"<option value='9' " +
				typeUniqueArtNet +
				'>ArtNet - Unicast</option>' +
				"<option value='4' " +
				typeDDPR +
				'>DDP - Raw Channel Numbers</option>' +
				"<option value='5' " +
				typeDDP1 +
				'>DDP - One Based</option>' +
				"<option value='6' " +
				typeKiNet1 +
				'>KiNet v1</option>' +
				"<option value='7' " +
				typeKiNet2 +
				'>KiNet v2</option>' +
				"<option value='8' " +
				typeTwinkly +
				'>Twinkly</option>';
		}

		bodyHTML += '</select></td>';
		bodyHTML +=
			'<td ' +
			inputStyle +
			"><input class='txtIP' type='text' value='" +
			unicastAddress +
			"' size='16' maxlength='32' " +
			ipDisabled +
			'></td>';
		bodyHTML +=
			"<td><input class='txtStartAddress singleDigitInput' type='number' min='1' max='8388608' value='" +
			startAddress.toString() +
			"' onChange='updateUniverseEndChannel($(this).parent().parent());' onkeypress='this.onchange();' onpaste='this.onchange();' oninput='this.onchange();'/></td><td><span class='numEndChannel'>" +
			endChannel.toString() +
			'</span></td>';

		bodyHTML +=
			"<td><input class='txtUniverse singleDigitInput' type='number' min='" +
			minNum +
			"' max='63999' value='" +
			uid.toString() +
			"'" +
			universeNumberDisable +
			'/></td>';

		bodyHTML +=
			"<td><input class='numUniverseCount singleDigitInput' type='number' min='1' max='999' value='" +
			ucount.toString() +
			"'" +
			universeCountDisable +
			" onChange='updateUniverseEndChannel($(this).parent().parent());' onkeypress='this.onchange();' onpaste='this.onchange();' oninput='this.onchange();'/></td>";

		bodyHTML +=
			"<td><input class='txtSize' type='number'  min='1'  max='" +
			universeSize +
			"' value='" +
			size.toString() +
			"' onChange='updateUniverseEndChannel($(this).parent().parent());' onkeypress='this.onchange();' onpaste='this.onchange();' oninput='this.onchange();'></td>";
		var priorityStyle =
			settings['uiLevel'] < 1 ? "style='display: none;'" : inputStyle;
		bodyHTML +=
			'<td ' +
			priorityStyle +
			"><input class='txtPriority' type='number' min='0' max='9999' value='" +
			priority.toString() +
			"'";
		if (type > 1) {
			// DDP/ArtNet/KiNet/Twinkly don't support priority
			bodyHTML += ' disabled';
		}
		bodyHTML += '/></td>';
		var syncStyle =
			input || settings['uiLevel'] < 1 ? "style='display: none;'" : inputStyle;
		bodyHTML +=
			'<td ' +
			syncStyle +
			"><input class='txtSyncUniverse singleDigitInput' type='number' min='0' max='63999' value='" +
			syncUniverse.toString() +
			"'";
		if (type > 1) {
			// only E1.31 supports the sync universe field
			bodyHTML += ' disabled';
		}
		bodyHTML += '/></td>';
		// Per-controller pacing override (advanced, output-only). Disabled for
		// multicast/broadcast types since only unicast destinations are paced.
		var pacingStyle =
			input || settings['uiLevel'] < 1
				? "style='display: none;'"
				: inputStyle;
		bodyHTML +=
			'<td ' +
			pacingStyle +
			"><select class='form-select pacingRate'" +
			ipDisabled +
			'>' +
			PacingRateOptionsHTML(pacingRateVal) +
			'</select></td>';
		bodyHTML +=
			'<td ' +
			inputStyle +
			"><input class='txtMonitor' id='txtMonitor' type='checkbox' size='4' maxlength='4' " +
			(monitor == 1 ? 'checked' : '') +
			monitorDisabled +
			'/></td>' +
			'<td ' +
			inputStyle +
			"><input class='txtDeDuplicate' id='txtDeDuplicate' type='checkbox' size='4' maxlength='4' " +
			(deDuplicate == 1 ? 'checked' : '') +
			'/></td>' +
			'<td ' +
			inputStyle +
			"><input type=button class='pingButton buttons' onClick='PingE131IP(" +
			i.toString() +
			");' value='Ping' " +
			ipDisabled +
			'></td>' +
			'</tr>';
	}

	var ecb = $('#E131Enabled');
	if (channelData.enabled == 1) {
		ecb.prop('checked', true);
		$('#tab-e131-LI').show();
		if (!input) {
			$('#outputOffWarning').hide();
		}
	} else {
		ecb.prop('checked', false);
		if (!input && anyEnabled) $('#outputOffWarning').show();
	}
	if (input) {
		if (channelData.timeout != null) {
			$('#bridgeTimeoutMS').val(channelData.timeout);
		} else {
			$('#bridgeTimeoutMS').val(1000);
		}
	}
	$('#tblUniversesBody').html(bodyHTML);

	$('#txtUniverseCount').val(UniverseCount);
	if (hasMCBC) {
		$('#sourceInterfaceDiv').show();
	} else {
		$('#sourceInterfaceDiv').hide();
	}

	SetUniverseInputNames(); // in co-universes.php
	if (!input) {
		CalculatePacingMaxFPS();
	}
}

function SetUniverseInputShownFields () {
	// Show/Hide the fields based on the type of universe
	$('#tblUniversesBody tr').each(function () {
		var UniTypeSelector = $(this).find('select.universeType');
		IPOutputTypeChanged(UniTypeSelector, false);
	});
}

function getUniverses (reload, input) {
	var url = 'api/channel/output/universeOutputs';
	if (input) {
		url = 'api/channel/output/universeInputs';
	}
	$.getJSON(url, function (data) {
		populateUniverseData(data, reload, input);
		SetUniverseInputShownFields(); // hide fields based on output type
	}).fail(function () {
		// no config saved yet; still need the Sending options set up to
		// match the default pacing selection
		UniverseCount = 0;
		$('#txtUniverseCount').val(UniverseCount);
		if (!input) {
			UpdateSendingModeOptions();
		}
	});
}

function SetUniverseRowInputNames (row, id) {
	row.find('span.rowID').html((id + 1).toString());

	for (var i = 0; i < UNIVERSE_ROW_FIELDS.length; i++) {
		var f = UNIVERSE_ROW_FIELDS[i];
		row.find('input.' + f).attr('name', f + '[' + id + ']');
		row.find('input.' + f).attr('id', f + '[' + id + ']');
		row.find('select.' + f).attr('name', f + '[' + id + ']');
		row.find('select.' + f).attr('id', f + '[' + id + ']');
	}
}
function SetUniverseInputNames () {
	var id = 0;
	$('#tblUniversesBody tr').each(function () {
		SetUniverseRowInputNames($(this), id);
		id += 1;
	});
}

function InitializeUniverses () {
	UniverseSelected = -1;
	UniverseCount = 0;
}

// Copy one field from row srcIdx to row destIdx. Handles checkboxes vs other
// inputs/selects and silently skips non-input fields (like 'rowGrip').
function copyUniverseRowField (fieldName, srcIdx, destIdx) {
	var src = document.getElementById(fieldName + '[' + srcIdx + ']');
	var dest = document.getElementById(fieldName + '[' + destIdx + ']');
	if (!src || !dest) return;
	if (src.type === 'checkbox') {
		dest.checked = src.checked;
	} else {
		dest.value = src.value;
	}
}

function DeleteUniverse (input) {
	if (UniverseSelected >= 0) {
		var selectedIndex = UniverseSelected;
		for (i = UniverseSelected + 1; i < UniverseCount; i++, selectedIndex++) {
			for (var j = 0; j < UNIVERSE_ROW_FIELDS.length; j++) {
				copyUniverseRowField(UNIVERSE_ROW_FIELDS[j], i, selectedIndex);
			}
			var universeType = document.getElementById(
				'universeType[' + selectedIndex + ']'
			).value;
			document.getElementById('txtIP[' + selectedIndex + ']').disabled = !(
				universeType == '1' || universeType == '3'
			);
			updateUniverseEndChannel(
				document.getElementById('tblUniversesBody').rows[selectedIndex]
			);
		}
		document.getElementById('tblUniversesBody').deleteRow(UniverseCount - 1);
		UniverseCount--;
		document.getElementById('txtUniverseCount').value = UniverseCount;
		UniverseSelected = -1;
	}
}

function CloneUniverses (cloneNumber) {
	var selectIndex = UniverseSelected;
	if (isNaN(cloneNumber)) {
		DialogError('Clone Universe', 'Error, invalid number');
		return;
	}
	if (UniverseSelected + cloneNumber - 1 >= UniverseCount) {
		return;
	}

	var universeType = document.getElementById(
		'universeType[' + selectIndex + ']'
	).value;
	var size = Number(
		document.getElementById('txtSize[' + selectIndex + ']').value
	);
	var uCount = Number(
		document.getElementById('numUniverseCount[' + selectIndex + ']').value
	);
	var universe =
		Number(document.getElementById('txtUniverse[' + selectIndex + ']').value) +
		uCount;
	var startAddress =
		Number(
			document.getElementById('txtStartAddress[' + selectIndex + ']').value
		) +
		size * uCount;

	for (var z = 0; z < cloneNumber; z++, universe += uCount) {
		var i = z + UniverseSelected + 1;
		for (var j = 0; j < UNIVERSE_ROW_FIELDS.length; j++) {
			copyUniverseRowField(UNIVERSE_ROW_FIELDS[j], selectIndex, i);
		}
		// Per-clone overrides: each successive clone bumps the universe number
		// and start channel by the source row's universe span.
		document.getElementById('txtUniverse[' + i + ']').value =
			universe.toString();
		document.getElementById('txtStartAddress[' + i + ']').value =
			startAddress.toString();

		document.getElementById('txtIP[' + i + ']').disabled = !(
			universeType == '1' || universeType == '3'
		);
		updateUniverseEndChannel(
			document.getElementById('tblUniversesBody').rows[i]
		);
		startAddress += size * uCount;
	}
}
function CloneUniverse () {
	var answer = prompt(
		'How many universes to clone from selected universe?',
		'1'
	);
	var cloneNumber = Number(answer);
	CloneUniverses(cloneNumber);
}

function UpdateSendingModeOptions () {
	var sel = $('#E131ThreadedOutput');
	if (sel.length == 0 || $('#E131PacingRate').length == 0) {
		return;
	}
	var pacing = parseInt($('#E131PacingRate').val());
	var threaded = parseInt(sel.val());
	sel.empty();
	if (pacing > 0) {
		// paced destinations always use the batched non-blocking send path,
		// so only the thread-isolation choice remains
		sel.append(new Option('Single-Threaded', '2'));
		sel.append(new Option('Multi-Threaded', '3'));
		sel.val(threaded == 0 || threaded == 2 ? '2' : '3');
	} else {
		sel.append(new Option('Single-Threaded Blocking', '0'));
		sel.append(new Option('Multi-Threaded Blocking', '1'));
		sel.append(new Option('Single-Threaded Non-Blocking', '2'));
		sel.append(new Option('Multi-Threaded Non-Blocking', '3'));
		sel.val('' + threaded);
	}
}

function CalculatePacingMaxFPS () {
	var el = $('#pacingMaxFPS');
	if (el.length == 0 || $('#E131PacingRate').length == 0) {
		return;
	}
	var globalPacing = parseInt($('#E131PacingRate').val());
	if (UniverseCount == 0) {
		el.html('');
		return;
	}
	// Pacing is applied per unicast destination.  Sum the wire bytes per frame
	// for each controller and track its effective rate (per-controller override,
	// else the global rate), then report the destination with the lowest fps.
	// 42 = IP+UDP+ethernet framing per packet; the rest is each protocol header.
	var perDest = {}; // addr -> bytes/frame
	var perDestRate = {}; // addr -> effective Mbps (0 = unpaced)
	for (var i = 0; i < UniverseCount; i++) {
		var activeEl = document.getElementById('chkActive[' + i + ']');
		var typeEl = document.getElementById('universeType[' + i + ']');
		var ipEl = document.getElementById('txtIP[' + i + ']');
		var sizeEl = document.getElementById('txtSize[' + i + ']');
		var ucountEl = document.getElementById('numUniverseCount[' + i + ']');
		if (!activeEl || !activeEl.checked || !typeEl || !ipEl || !sizeEl) {
			continue;
		}
		var type = parseInt(typeEl.value);
		var addr = ipEl.value.trim();
		var chans = parseInt(sizeEl.value) || 0;
		var ucount = (ucountEl ? parseInt(ucountEl.value) : 1) || 1;
		var bytes = 0;
		if (type == 1) {
			// E1.31 unicast
			bytes = ucount * (chans + 126 + 42);
		} else if (type == 3 || type == 9) {
			// ArtNet unicast
			bytes = ucount * (chans + 18 + 42);
		} else if (type == 4 || type == 5) {
			// DDP
			bytes = chans + Math.ceil(chans / 1440) * (10 + 42);
		} else if (type == 6 || type == 7) {
			// KiNet
			bytes = ucount * (chans + 24 + 42);
		} else if (type == 8) {
			// Twinkly
			bytes = chans + Math.ceil(chans / 900) * (12 + 42);
		} else {
			// multicast/broadcast types are not paced
			continue;
		}
		if (addr == '' || bytes <= 0) {
			continue;
		}
		perDest[addr] = (perDest[addr] || 0) + bytes;
		var rateEl = document.getElementById('pacingRate[' + i + ']');
		var rowRate = rateEl ? parseInt(rateEl.value) : -1;
		var effRate = rowRate >= 0 ? rowRate : globalPacing;
		// most conservative rate wins for a shared destination; 0 = unpaced,
		// treated as unlimited so any positive rate on the same IP takes over
		if (!(addr in perDestRate)) {
			perDestRate[addr] = effRate;
		} else {
			var curInf = perDestRate[addr] == 0 ? Infinity : perDestRate[addr];
			var newInf = effRate == 0 ? Infinity : effRate;
			perDestRate[addr] = newInf < curInf ? effRate : perDestRate[addr];
		}
	}
	var worstAddr = '';
	var worstFps = Infinity;
	for (var a in perDest) {
		var rate = perDestRate[a];
		if (!(rate > 0)) {
			// disabled/unpaced destination isn't limited by pacing
			continue;
		}
		var fps = Math.floor((rate * 1000000) / (perDest[a] * 8));
		if (fps < worstFps) {
			worstFps = fps;
			worstAddr = a;
		}
	}
	if (worstAddr == '') {
		el.html('');
		return;
	}
	if (worstFps > 999) {
		el.html('Supports &gt;999 fps');
	} else {
		el.html(
			'Supports &asymp;' + worstFps + ' fps max (limited by ' + worstAddr + ')'
		);
	}
}

function PacingRateChanged () {
	UpdateSendingModeOptions();
	CalculatePacingMaxFPS();
}

function postUniverseJSON (input) {
	var postData = {};
	var anyEnabled = 0;

	var output = {};
	output.type = 'universes';
	output.enabled = document.getElementById('E131Enabled').checked ? 1 : 0;
	if (!input) {
		// output only properties
		output.interface = document.getElementById('selE131interfaces').value;
		output.threaded = parseInt(
			document.getElementById('E131ThreadedOutput').value
		);
		output.pacingRate = parseInt(
			document.getElementById('E131PacingRate').value
		);
	} else {
		// input only properties
		output.timeout = parseInt(document.getElementById('bridgeTimeoutMS').value);
	}
	output.startChannel = 1;
	output.channelCount = -1;
	output.universes = [];

	var i;
	for (i = 0; i < UniverseCount; i++) {
		var universe = {};
		universe.active = document.getElementById('chkActive[' + i + ']').checked
			? 1
			: 0;
		anyEnabled |= universe.active;
		universe.description = document.getElementById('txtDesc[' + i + ']').value;
		universe.id = parseInt(
			document.getElementById('txtUniverse[' + i + ']').value
		);
		universe.startChannel = parseInt(
			document.getElementById('txtStartAddress[' + i + ']').value
		);
		universe.channelCount = parseInt(
			document.getElementById('txtSize[' + i + ']').value
		);
		universe.type = parseInt(
			document.getElementById('universeType[' + i + ']').value
		);
		// The universe-count input is hidden for the flat types, but its value
		// was still read and saved -- so a DDP or Twinkly row kept whatever was
		// last left in that field.  fppd ignores it, but nothing reading the
		// config back can tell a leftover from a real count, and multiplying by
		// it gives a channel span that is wrong differently on every install.
		// Write the one value that means "not a run of universes" instead.
		universe.universeCount = isFlatUniverseType(universe.type)
			? 1
			: parseInt(
					document.getElementById('numUniverseCount[' + i + ']').value
			  );
		universe.address = document.getElementById('txtIP[' + i + ']').value;
		universe.priority = parseInt(
			document.getElementById('txtPriority[' + i + ']').value
		);
		if (!input) {
			var syncEl = document.getElementById('txtSyncUniverse[' + i + ']');
			universe.syncUniverse = syncEl ? parseInt(syncEl.value) || 0 : 0;
			universe.monitor = document.getElementById('txtMonitor[' + i + ']')
				.checked
				? 1
				: 0;
			universe.deDuplicate = document.getElementById(
				'txtDeDuplicate[' + i + ']'
			).checked
				? 1
				: 0;
			// only persist a pacing override when it differs from Default (-1),
			// so unchanged configs stay clean and fall back to the global rate
			var pacingEl = document.getElementById('pacingRate[' + i + ']');
			var pacingVal = pacingEl ? parseInt(pacingEl.value) : -1;
			if (pacingVal >= 0) {
				universe.pacingRate = pacingVal;
			}
		}
		output.universes.push(universe);
	}
	if (input) {
		postData.channelInputs = [];
		postData.channelInputs.push(output);
	} else {
		postData.channelOutputs = [];
		postData.channelOutputs.push(output);
	}
	var fileName = input ? 'universeInputs' : 'universeOutputs';
	var postDataString = JSON.stringify(postData);

	if (anyEnabled && !output.enabled) $('#outputOffWarning').show();
	else $('#outputOffWarning').hide();

	$.post('api/channel/output/' + fileName, postDataString)
		.done(function (data) {
			$.jGrowl('E1.31 Universes Saved', { themeState: 'success' });

			// Auto-disable testing mode when output configs change
			if (typeof disableTestModeIfActive === 'function') {
				disableTestModeIfActive();
			}
		})
		.fail(function () {
			DialogError('Save Universes', 'Error: Unable to save E1.31 Universes.');
		});
}

/**
 * Disables testing mode if it is currently enabled.
 * This function is called automatically when channel output configurations are changed.
 */
function disableTestModeIfActive () {
	$.get('api/testmode', function (data) {
		if (data && data.enabled) {
			// Testing mode is active, disable it
			var disableData = {
				enabled: 0,
				mode: data.mode || 'RGBChase',
				subMode: data.subMode || 'RGBChase-RGB',
				cycleMS: data.cycleMS || 1000,
				channelSet: data.channelSet || '1-1024',
				channelSetType: data.channelSetType || 'channelRange',
				colorPattern: data.colorPattern || 'FF000000FF000000FF'
			};

			$.ajax({
				url: 'api/testmode',
				type: 'POST',
				contentType: 'application/json',
				data: JSON.stringify(disableData),
				success: function () {
					console.log(
						'Testing mode automatically disabled due to output configuration change'
					);
				},
				error: function () {
					console.log('Failed to disable testing mode');
				}
			});
		}
	});
}

function validateEmail (email) {
	return email.match(
		/^(([^<>()[\]\\.,;:\s@\"]+(\.[^<>()[\]\\.,;:\s@\"]+)*)|(\".+\"))@((\[[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\])|(([a-zA-Z\-0-9]+\.)+[a-zA-Z]{2,}))$/
	);
}

function validateUniverseData () {
	var i;
	var txtUniverse;
	var txtStartAddress;
	var txtSize;
	var universeType;
	var txtPriority;
	var result;
	var returnValue = true;
	for (i = 0; i < UniverseCount; i++) {
		// unicast address
		universeType = document.getElementById('universeType[' + i + ']').value;
		if (
			universeType == 1 ||
			universeType == 3 ||
			universeType == 4 ||
			universeType == 5
		) {
			if (!validateIPaddress('txtIP[' + i + ']', true)) {
				returnValue = false;
			}
		}
		// universe
		if (universeType >= 0 && universeType <= 3) {
			txtUniverse = document.getElementById('txtUniverse[' + i + ']');
			var minNum = 1;
			if (universeType >= 2 && universeType <= 3) minNum = 0;

			if (!validateNumber(txtUniverse, minNum, 63999)) {
				returnValue = false;
			}
		}
		// start address
		txtStartAddress = document.getElementById('txtStartAddress[' + i + ']');
		if (!validateNumber(txtStartAddress, 1, FPPD_MAX_CHANNELS)) {
			returnValue = false;
		}
		// size
		txtSize = document.getElementById('txtSize[' + i + ']');
		var max = 512;
		if (isFlatUniverseType(universeType)) {
			max = FPPD_MAX_CHANNELS;
		}
		if (!validateNumber(txtSize, 1, max)) {
			returnValue = false;
		}

		// priority
		txtPriority = document.getElementById('txtPriority[' + i + ']');
		if (!validateNumber(txtPriority, 0, 9999)) {
			returnValue = false;
		}

		// sync universe (E1.31 only, 0 = disabled)
		if (universeType == 0 || universeType == 1) {
			var txtSyncUniverse = document.getElementById(
				'txtSyncUniverse[' + i + ']'
			);
			if (txtSyncUniverse && !validateNumber(txtSyncUniverse, 0, 63999)) {
				returnValue = false;
			}
		}
	}
	return returnValue;
}

/*
 * checks if IP Address looks like an valid IP,
 */
function validateIPaddress (id, allowHostnames = false) {
	var ipb = document.getElementById(id);
	var ip = ipb.value;

	var isIpRegex =
		/^(?:(25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9]?[0-9])\.){3}(?:25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9]?[0-9])$/;
	// hostnames must begin with a letter, contain only letters/numbers/hyphens, and end with a letter
	// or number
	var isHostnameRegex =
		/^(([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\-]*[a-zA-Z0-9])\.)*([A-Za-z0-9]|[A-Za-z0-9][A-Za-z0-9\-]*[A-Za-z0-9])$/;
	var rc = false;
	if (
		ip == '' ||
		(allowHostnames && isHostnameRegex.test(ip)) ||
		isIpRegex.test(ip)
	) {
		ipb.style.border = '#D2D2D2 1px solid';
		return true;
	}

	ipb.style.border = '#F00 2px solid';
	return false;
}

function validateNumber (textbox, minimum, maximum) {
	result = true;
	value = Number(textbox.value);
	if (isNaN(value)) {
		textbox.style.border = 'red solid 1px';
		textbox.value = '';
		result = false;
	}
	if (value >= minimum && value <= maximum) {
		return true;
	} else {
		textbox.style.border = 'red solid 1px';
		textbox.value = '';
		result = false;
		alert(textbox.value + ' is not between ' + minimum + ' and ' + maximum);
	}
}

function StartNextScheduledItemNow () {
	var url = 'api/command/Start Next Scheduled Item';
	$.get(url)
		.done(function (data) {
			$.jGrowl(data, { themeState: 'success' });
		})
		.fail(function () {
			$.jGrowl('Failed to start next scheduled item.', {
				themeState: 'danger'
			});
		});
}

function ExtendSchedule (minutes) {
	var seconds = minutes * 60;
	var url = 'api/command/Extend Schedule/' + seconds;
	$.get(url)
		.done(function (data) {
			$.jGrowl(data, { themeState: 'success' });
		})
		.fail(function () {
			$.jGrowl('Failed to extend schedule.', { themeState: 'danger' });
		});
}

function ExtendSchedulePopup () {
	var minutes = prompt(
		'Extend running scheduled playlist by how many minutes?',
		'1'
	);
	if (minutes === null) {
		$.jGrowl('Extend cancelled', { themeState: 'danger' });
		return;
	}

	minutes = Number(minutes);

	var minimum = -3 * 60;
	var maximum = 12 * 60;

	if (minutes > maximum || minutes < minimum) {
		DialogError(
			'Extend Schedule',
			'Error: Minutes is not between the minimum ' +
				minimum +
				' and maximum ' +
				maximum
		);
	} else {
		ExtendSchedule(minutes);
	}
}

var playListArray = [];
function GetPlaylistArray (callback) {
	$.ajax({
		dataType: 'json',
		url: 'api/playlists/validate',
		async: false,
		success: function (data) {
			playListArray = data;
			if (typeof callback === 'function') {
				callback();
			}
		},
		error: function (...args) {
			DialogError(
				'Load Playlists',
				'Error loading list of playlists' + show_details(args)
			);
		}
	});
}

var sequenceArray = [];
function GetSequenceArray () {
	$.ajax({
		dataType: 'json',
		url: 'api/sequence',
		async: false,
		success: function (data) {
			sequenceArray = data;
		},
		error: function (...args) {
			DialogError(
				'Load Sequences',
				'Error loading list of sequences' + show_details(args)
			);
		}
	});
}

var mediaArray = [];
function GetMediaArray () {
	$.ajax({
		dataType: 'json',
		url: 'api/media',
		async: false,
		success: function (data) {
			mediaArray = data;
		},
		error: function () {
			DialogError('Load Media', 'Error loading list of media');
		}
	});
}

function GetFiles (dir) {
	$.ajax({
		dataType: 'json',
		url: 'api/files/' + dir,
		success: function (data) {
			let i = 0;

			if (data.files.length > 0) {
				$('#tbl' + dir)
					.find('tbody')
					.html('');
			} else {
				$('#tbl' + dir)
					.find('tbody')
					.html(
						"<tr class='unselectableRow'><td colspan=8 align='center'>No files found.</td></tr>"
					);
			}
			data.files.forEach(function (f) {
				var detail = f.sizeHuman;
				if ('playtimeSeconds' in f) {
					detail = f.playtimeSeconds;
				}

				var thumbSize = 0;
				if (
					settings.hasOwnProperty('fileManagerThumbnailSize') &&
					settings['fileManagerThumbnailSize'] > 0
				)
					thumbSize = settings['fileManagerThumbnailSize'];

				var detailField = 'playtimeSeconds' in f ? 'duration' : 'size';

				var tableRow = '';
				if (dir == 'Images' && thumbSize > 0) {
					if (parseInt(f.sizeBytes) > 0) {
						tableRow =
							"<tr class='fileDetails' id='fileDetail_" +
							i +
							"'><td data-field='filename' class ='filenameColumn fileName'>" +
							f.name.replace(/&/g, '&amp;').replace(/</g, '&lt;') +
							"</td><td data-field='" +
							detailField +
							"' class='fileExtraInfo'>" +
							detail +
							"</td><td data-field='dateModified' class ='fileTime'>" +
							f.mtime +
							"</td><td data-field='thumbnail'><img style='display: block; max-width: " +
							thumbSize +
							'px; max-height: ' +
							thumbSize +
							"px; width: auto; height: auto;' src='api/file/" +
							dir +
							'/' +
							f.name +
							"' onClick=\"ViewImage('" +
							f.name +
							'\');" /></td></tr>';
					} else {
						tableRow =
							"<tr class='fileDetails fileIsDirectory' id='fileDetail_" +
							i +
							"'><td data-field='filename' class ='filenameColumn fileName'>" +
							f.name.replace(/&/g, '&amp;').replace(/</g, '&lt;') +
							"</td><td data-field='" +
							detailField +
							"' class='fileExtraInfo'>" +
							detail +
							"</td><td data-field='dateModified' class ='fileTime'>" +
							f.mtime +
							"</td><td data-field='thumbnail'>Subdir</td></tr>";
					}
				} else {
					tableRow =
						"<tr class='fileDetails' id='fileDetail_" +
						i +
						"'><td data-field='filename' class ='filenameColumn fileName'>" +
						f.name.replace(/&/g, '&amp;').replace(/</g, '&lt;') +
						"</td><td data-field='" +
						detailField +
						"' class='fileExtraInfo'>" +
						detail +
						"</td><td data-field='dateModified' class ='fileTime'>" +
						f.mtime +
						'</td></tr>';
				}

				$('#tbl' + dir)
					.find('tbody')
					.append(tableRow);
				++i;
			});
		},
		error: function (x, t, e) {
			DialogError(
				'Load Files',
				'Error loading list of files in ' +
					dir +
					' directory' +
					show_details([x, t, e])
			);
		},
		complete: function () {
			SetupTableSorter('tbl' + dir);
		}
	});
}

// show error details:
const WANT_DETAILS = true; // false; //TODO: maybe use config setting?
function show_details (args) {
	if (!WANT_DETAILS || !args || !args.length) return '';
	if (typeof args[0] == 'object' && args[0].responseText) {
		return args[0].responseText;
	} // show most useful part
	const retval = [''];
	args.forEach(function (arg) {
		var js = JSON.stringify(arg);
		if (js.length > 200) {
			js = js.substr(0, 200) + ' ...';
		}
		retval.push(typeof arg + ': ' + js);
	});
	console.log(arg);
	return retval.join('<br/>');
}

function moveFile (file, callback = null) {
	$.get('api/file/move/' + encodeURIComponent(file))
		.done(function (data) {
			if ('OK' != data.status) {
				DialogError('File Move Error', data.status);
			}
			if (callback != null) {
				callback();
			}
		})
		.fail(function (data) {
			DialogError('File Move Error', 'Unexpected error while to move file');
			if (callback != null) {
				callback();
			}
		});
}

function updateFPPStatus () {
	var status = GetFPPStatus();
}

function IsFPPDrunning () {
	var ret = 'false';
	if (lastStatusJSON) {
		if ('fppd' in lastStatusJSON && lastStatusJSON.fppd == 'running') {
			ret = 'true';
		}
		if (
			'status_name' in lastStatusJSON &&
			lastStatusJSON.status_name == 'updating'
		) {
			ret = 'updating';
		}
	}

	if (ret == 'true') {
		SetButtonState('#btnDaemonControl', 'enable');
		$('#btnDaemonControl')
			.html("<i class='fas fa-fw fa-stop fa-nbsp'></i>Stop FPPD")
			.attr('value', 'Stop FPPD');
		$('#daemonStatus').html('FPPD is running.');
	} else if (ret == 'updating') {
		SetButtonState('#btnDaemonControl', 'disable');
		$('#btnDaemonControl')
			.html("<i class='fas fa-fw fa-play fa-nbsp'></i>Start FPPD")
			.attr('value', 'Start FPPD');
		$('#daemonStatus').html('FPP is currently updating.');
	} else {
		SetButtonState('#btnDaemonControl', 'enable');
		$('#btnDaemonControl')
			.html("<i class='fas fa-fw fa-play fa-nbsp'></i>Start FPPD")
			.attr('value', 'Start FPPD');
		$('#daemonStatus').html('FPPD is stopped.');
		$('.schedulerStartTime').hide();
		$('.schedulerEndTime').hide();
	}
}

function SetupUIForMode (fppMode) {
	if (fppMode == 8) {
		// Remote Mode
		if ($('h1.title.statusTitle')[0].innerHTML != 'Status - Remote Mode') {
			$('#playerModeInfo').hide();
			$('#remoteModeInfo').show();
			$('h1.title.statusTitle')[0].innerHTML = 'Status - Remote Mode';
		}
	} else {
		// Player Mode
		if ($('h1.title.statusTitle')[0].innerHTML != 'Status - Player Mode') {
			if ($('#bridgeModeInfo').is(':hidden')) {
				$('#playerModeInfo').show();
			}
			$('#remoteModeInfo').hide();
			$('h1.title.statusTitle')[0].innerHTML = 'Status - Player Mode';
		}
	}
	if ($('body').hasClass('is-loading')) {
		$('body').removeClass('is-loading show-loading-skeleton');
	}
}

// When the device is bridging, #bridgeModeInfo is shown and SetupUIForMode
// leaves #playerModeInfo alone, so its visibility is decided here: the player
// controls are only relevant when there is content to play (a playlist or a
// sequence) and we are not in remote mode. This is re-run after the playlist
// and sequence arrays finish loading so a slow load can't leave the section
// hidden until a manual refresh. When not bridging, visibility is owned by
// SetupUIForMode, so do nothing.
function UpdatePlayerModeInfoVisibility () {
	if ($('#bridgeModeInfo').length == 0 || $('#bridgeModeInfo').is(':hidden')) {
		return;
	}
	if (
		(playListArray.length == 0 && sequenceArray.length == 0) ||
		GetFPPDmodeLocal() == 8
	) {
		$('#playerModeInfo').hide();
	} else {
		$('#playerModeInfo').show();
	}
}

// Initialize temperature unit variable
var temperatureUnit = false;
if (
	typeof settings !== 'undefined' &&
	settings.hasOwnProperty('temperatureInF')
) {
	temperatureUnit = settings['temperatureInF'] == 1;
}

function changeTemperatureUnit () {
	if (temperatureUnit) {
		SetSetting('temperatureInF', '0', 0, 0);
		temperatureUnit = false;
	} else {
		SetSetting('temperatureInF', '1', 0, 0);
		temperatureUnit = true;
	}
	triggerStatusChangeFunctions();
}
function GetFPPStatus () {
	/* $.ajax({
      url: 'api/system/status',
      dataType: 'json',
      success: function(response, reqStatus, xhr) {
           */
	var response = lastStatusJSON;
	if (response && typeof response === 'object') {
		$('#btnDaemonControl').show();

		if (response.status_name == 'stopped') {
			if (!('warnings' in response)) {
				response.warnings = [];
			}
			if (!('warningInfo' in response)) {
				response.warningInfo = [];
			}
			// Check if boot delay is active
			if (response.bootDelayActive == 1) {
				var message =
					'Boot Delay in Progress - FPPD will start when delay completes';

				// Calculate remaining time if we have timing info
				if (response.bootDelayStart && response.bootDelayDuration) {
					var currentTime = Math.floor(Date.now() / 1000);
					var elapsed = currentTime - response.bootDelayStart;

					if (response.bootDelayDuration === 'auto') {
						message =
							'Boot Delay in Progress - Waiting for valid system time (max 5 minutes)';
						var maxDuration = 300; // 5 minutes
						var remaining = Math.max(0, maxDuration - elapsed);
						if (remaining > 0) {
							var mins = Math.floor(remaining / 60);
							var secs = remaining % 60;
							message +=
								' - ' + (mins > 0 ? mins + 'm ' : '') + secs + 's remaining';
						}
					} else {
						var duration = parseInt(response.bootDelayDuration);
						var remaining = Math.max(0, duration - elapsed);
						if (remaining > 0) {
							var mins = Math.floor(remaining / 60);
							var secs = remaining % 60;
							message =
								'Boot Delay in Progress - ' +
								(mins > 0 ? mins + 'm ' : '') +
								secs +
								's remaining';
						}
					}
				}

				// Add Boot Now button to the message
				message +=
					' <button class="btn btn-success btn-xs ml-2" onclick="SkipBootDelay()"><i class="fas fa-play"></i> Boot Now</button>';

				response.warnings.push(message);
				// Use id 0 to avoid the click-through handler which mangles HTML in the message
				response.warningInfo.push({
					message: message,
					id: 0
				});
			} else {
				response.warnings.push('FPPD Daemon is not running');
				response.warningInfo.push({
					message: 'FPPD Daemon is not running',
					id: 1
				});
			}
			$.get('api/system/volume')
				.done(function (data) {
					updateVolumeUI(parseInt(data.volume));
				})
				.fail(function () {
					DialogError(
						'Volume Query Failed',
						'Failed to query Volume when FPPD stopped'
					);
				});
			$('#fppTime').html('');
			SetButtonState('#btnDaemonControl', 'enable');
			$('#btnDaemonControl').html(
				"<i class='fas fa-fw fa-play fa-nbsp'></i>Start FPPD"
			);
			$('#daemonStatus').html('FPPD is stopped.');
			$('#txtPlayerStatus').html(status);
			$('#playerTime').hide();
			$('#txtSeqFilename').html('');
			$('#txtMediaFilename').html('');
			$('#schedulerStatus').html('');
			$('.schedulerStartTime').hide();
			$('.schedulerEndTime').hide();
			$('#mqttRow').hide();
			updateWarnings(response);
		} else if (response.status_name == 'updating') {
			$('#fppTime').html('');
			SetButtonState('#btnDaemonControl', 'disable');
			$('#btnDaemonControl').html(
				"<i class='fas fa-fw fa-play fa-nbsp'></i>Start FPPD"
			);
			$('#daemonStatus').html('FPP is currently updating.');
			$('#txtPlayerStatus').html(status);
			$('#playerTime').hide();
			$('#txtSeqFilename').html('');
			$('#txtMediaFilename').html('');
			$('#schedulerStatus').html('');
			$('.schedulerStartTime').hide();
			$('.schedulerEndTime').hide();
			$('#mqttRow').hide();
		} else {
			SetButtonState('#btnDaemonControl', 'enable');
			$('#btnDaemonControl').attr(
				"<i class='fas fa-fw fa-stop fa-nbsp'></i>Stop FPPD"
			);
			$('#daemonStatus').html('FPPD is running.');
			parseStatus(response);
		}

		lastStatus = response.status;
	}
	/*
              },
              complete: function() {
                  clearTimeout(statusTimeout);
                  statusTimeout = setTimeout(GetFPPStatus, 1000);
              }
          })
   */
}

function updateWarnings (jsonStatus) {
	if (jsonStatus.hasOwnProperty('warnings')) {
		var txt =
			'<b>Abnormal Conditions - May cause poor performance or other issues (Click link icon for help)</b><br/><ul style="list-style-type: none;">';

		if (jsonStatus.hasOwnProperty('warningInfo')) {
			currentWarnings = jsonStatus['warningInfo'];
		} else {
			$.ajax({
				url: 'warnings_full.json',
				async: false,
				dataType: 'json',
				cache: false,
				success: function (response) {
					currentWarnings = response;
				}
			});
		}

		// Add blocked schedule warning if present
		if (jsonStatus.scheduler && jsonStatus.scheduler.blockedSchedule) {
			var blockedWarning = {
				id: 0,
				message:
					"Scheduled playlist '" +
					jsonStatus.scheduler.blockedSchedule.playlistName +
					"' was blocked from starting due to schedule protection on manually started playlist.",
				icon: 'fas fa-exclamation-triangle'
			};
			// Add at the beginning of the warnings array
			currentWarnings = [blockedWarning].concat(currentWarnings);
		}

		// The status payload now always carries a warnings array (empty when there
		// is nothing wrong), so the presence of the key no longer implies there is
		// anything to report.  Render only once a warning has actually survived,
		// otherwise the panel shows an empty "Abnormal Conditions" heading.
		if (!currentWarnings || currentWarnings.length == 0) {
			$('#warningsRow').hide();
			return;
		}

		var txt =
			'<b>Abnormal Conditions - May cause poor performance or other issues';
		var hasID = false;
		for (var i = 0; i < currentWarnings.length; i++) {
			var warningID = currentWarnings[i]['id'];
			if (warningID != 0) {
				hasID = true;
			}
		}
		if (hasID) {
			txt += ' (Click link icon for help)';
		}
		txt += '</b><br/><ul style="list-style-type: none;">';

		for (var i = 0; i < currentWarnings.length; i++) {
			var warningID = currentWarnings[i]['id'];
			var warningMessage = currentWarnings[i]['message'];
			// Optional one-click "Fix" button: a warning may carry a fixUrl (a
			// same-origin URL to navigate to) and fixText (the button label) in
			// its data. Rendered next to the warning so the user can act on it
			// directly (e.g. "Reinstall All Plugins" after an FPPOS upgrade).
			var fixButton = '';
			var warningData = currentWarnings[i]['data'];
			if (warningData && warningData['fixUrl'] && warningData['fixText']) {
				fixButton =
					' <a class="btn btn-sm btn-outline-primary warning-fix-btn" href="' +
					encodeURI(warningData['fixUrl']) +
					'"><i class="fas fa-wrench"></i> ' +
					warningData['fixText'] +
					'</a>';
			}
			if (warningID == 0) {
				//handle old style warnings with no id with legacy behavior
				txt +=
					'<li><i class="fas fa-solid fa-circle fa-2xs"></i>  ' +
					currentWarnings[i]['message'] +
					fixButton +
					'</li>';
			} else if (warningDefinitions && warningDefinitions['Warnings']) {
				//find extra warning info from definitions
				for (var z = 0; z < warningDefinitions['Warnings'].length; z++) {
					if (warningDefinitions['Warnings'][z]['id'] == warningID) {
						currentWarnings[i]['HelpPageType'] =
							warningDefinitions['Warnings'][z]['HelpPageType'];
						currentWarnings[i]['Title'] =
							warningDefinitions['Warnings'][z]['Title'];
						currentWarnings[i]['HelpTxt'] =
							warningDefinitions['Warnings'][z]['HelpTxt'];
						var warningGrp = warningDefinitions['Warnings'][z]['WarningGroup'];
						currentWarnings[i]['icon'] =
							warningDefinitions['WarningGroups'][warningGrp]['fa-icon'];
					}
				}

				//create output link string for each warning with a valid definition
				if (
					currentWarnings[i]['HelpPageType'] !== (null || '') ||
					currentWarnings[i]['HelpTxt'] !== (null || '')
				) {
					// Click-through behavior is wired up after the HTML is inserted
					// into the DOM (see the addEventListener loop below), not via an
					// inline onclick= built from string concatenation - the warning
					// message/help text are free-form and may contain quote
					// characters (a warning naming a specific command in quotes, for
					// instance), which broke the attribute/JS-string quoting when
					// embedded directly. data-warning-idx is always just a plain
					// integer, so it needs no escaping.
					txt +=
						'<li><span class="warning-link"><a href="javascript:void(0)" data-warning-idx="' +
						i +
						'"><i class="fas fa-' +
						currentWarnings[i]['icon'] +
						'"></i> ' +
						currentWarnings[i]['message'] +
						' (<i class="fas fa-link"></i> Warning ID: ' +
						warningID +
						')</a></span>' +
						fixButton +
						'</li>';
				} else {
					txt +=
						'<li><i class="fas fa-' +
						currentWarnings[i]['icon'] +
						'"></i> ' +
						currentWarnings[i]['message'] +
						fixButton +
						'</li>';
				}
			}
		}

		txt += '</ul>';

		document.getElementById('warningsDiv').innerHTML = txt;

		// Wire up click-through behavior here instead of via an inline onclick=
		// built from string concatenation (see the data-warning-idx comment
		// above) - message/HelpTxt are passed as real JS values through this
		// closure, never serialized into markup, so there's nothing to escape.
		document
			.querySelectorAll('#warningsDiv a[data-warning-idx]')
			.forEach(function (a) {
				var w = currentWarnings[parseInt(a.getAttribute('data-warning-idx'), 10)];
				a.addEventListener('click', function () {
					switch (w['HelpPageType']) {
						case 'php':
							doWarningPHPModal(w['id'], w['message']);
							break;
						case 'md':
							doWarningMDModal(w['id'], w['message']);
							break;
						default:
							doWarningBasicModal(w['id'], w['message'], w['HelpTxt']);
					}
				});
			});

		$('#warningsRow').show();
	} else {
		$('#warningsRow').hide();
	}
}

function doWarningPHPModal (id, message) {
	var options = {
		id: 'warningHelpDialog',
		title: 'Warning ID: ' + id + ' - ' + message,
		class: 'modal-dialog-scrollable',
		keyboard: true,
		backdrop: true
	};
	$.ajax({
		url: 'warningHelper.php?id=' + id,
		type: 'GET',
		error: function () {
			//file not exists
		},
		success: function (response) {
			options.body = response;
			DoModalDialog(options);
		}
	});
}

function doWarningMDModal (id, message) {
	var mdFile = 'help/warning-helpers/warning-' + id + '.md';
	var options = {
		id: 'warningHelpDialog',
		title: 'Warning ID: ' + id + ' - ' + message,
		body: '<zero-md src="' + mdFile + '"></zero-md>',
		class: 'modal-dialog-scrollable',
		keyboard: true,
		backdrop: true
	};
	$.ajax({
		url: mdFile,
		type: 'HEAD',
		error: function () {
			//file not exists
		},
		success: function () {
			DoModalDialog(options);
		}
	});
}

function doWarningBasicModal (id, message, helptxt) {
	var options = {
		id: 'warningHelpDialog',
		title: 'Warning ID: ' + id + ' - ' + message,
		body: helptxt,
		class: 'modal-dialog-scrollable',
		keyboard: true,
		backdrop: true
	};

	DoModalDialog(options);
}

function modeToString (mode) {
	switch (mode) {
		case 1:
			return 'Bridge';
		case 2:
			return 'Player';
		case 6:
			return 'Master';
		case 8:
			return 'Remote';
	}
	return 'Unknown Mode';
}

function updateVolumeUI (Volume) {
	//only update UI if no current change in progress
	if (VolumeChangeInProgress !== true) {
		if ($('#volume').html() !== Volume) {
			// Update the volume display only if changed
			$('#volume').html(Volume);
			$('#remoteVolume').html(Volume);
			$('#slider').val(Volume);
			$('#remoteVolumeSlider').val(Volume);
			SetSpeakerIndicator(Volume);
		}
	}
}

var firstStatusLoad = 1;

//
// UPdates the Sensor table when status is refreshed
// Only displayed on About page currently
//

function updateSensorStatus () {
	jsonStatus = lastStatusJSON;
	if (jsonStatus.hasOwnProperty('sensors')) {
		var nonFanSensors = jsonStatus.sensors.filter(function(s) {
			return s.valueType !== 'FanSpeed';
		});
		var sensorText = "<table id='sensorTable'>";
		var outPos = 0;
		var sensorType = '';
		if (nonFanSensors.length > 0) {
			sensorType = nonFanSensors[0].valueType;
		}
		for (var i = 0; i < nonFanSensors.length; i++) {
			if (
				nonFanSensors[i].valueType != sensorType &&
				nonFanSensors.length > 3 &&
				outPos % 2 == 1
			) {
				sensorText += '</tr>';
				outPos++;
			}
			sensorType = nonFanSensors[i].valueType;
			if (nonFanSensors.length < 4 || outPos % 2 == 0) {
				sensorText += '<tr>';
			}
			sensorText += '<td>';
			sensorText += nonFanSensors[i].label;
			sensorText += '</td><td style="padding-right: 15px;"';
			if (nonFanSensors[i].valueType == 'Temperature') {
				sensorText += " onclick='changeTemperatureUnit()'>";
				var val = nonFanSensors[i].value;
				if (temperatureUnit) {
					val *= 1.8;
					val += 32;
					sensorText += val.toFixed(1);
					sensorText += 'F';
				} else {
					sensorText += val.toFixed(1);
					sensorText += 'C';
				}
			} else {
				sensorText += '>';
				sensorText += nonFanSensors[i].formatted;
			}
			sensorText += '</td>';

			if (nonFanSensors.length > 4 && outPos % 2 == 1) {
				sensorText += '<tr>';
			}
			outPos++;
		}
		sensorText += '</table>';
		var sensorData = document.getElementById('sensorData');
		if (typeof sensorData != 'undefined' && sensorData != null) {
			sensorData.innerHTML = sensorText;
		}
		$('#sensorData').show();
	} else {
		$('#sensorData').hide();
	}
}

function parseStatus (jsonStatus) {
	var fppStatus = jsonStatus.status;
	var fppMode = jsonStatus.mode;
	var status = 'Idle';
	if (jsonStatus.status_name == 'testing') {
		status = 'Testing';
	}
	if (
		fppStatus == STATUS_IDLE ||
		fppStatus == STATUS_PLAYING ||
		fppStatus == STATUS_PAUSED ||
		fppStatus == STATUS_STOPPING_GRACEFULLY ||
		fppStatus == STATUS_STOPPING_GRACEFULLY_AFTER_LOOP
	) {
		$('#btnDaemonControl').show();
		$('#btnDaemonControl').html(
			"<i class='fas fa-fw fa-stop fa-nbsp'></i>Stop FPPD"
		);
		$('#daemonStatus').html('FPPD is running.');
	}

	updateVolumeUI(parseInt(jsonStatus.volume));

	AdjustFPPDModeFromStatus(fppMode);
	if (jsonStatus.hasOwnProperty('MQTT')) {
		if (jsonStatus.MQTT.configured) {
			$('#mqttRow').show();
			var mqttConnected = jsonStatus.MQTT.connected
				? 'Connected'
				: 'Disconnected';
			$('#mqttStatus').html(mqttConnected);
		} else {
			$('#mqttRow').hide();
		}
	} else {
		$('#mqttRow').hide();
	}

	updateWarnings(jsonStatus);
	if (jsonStatus['bridging']) {
		// Bridging
		$('#fppTime').html(jsonStatus.time);
		$('#bridgeModeInfo').show();
		if (firstStatusLoad || $('#e131statsLiveUpdate').is(':checked'))
			GetUniverseBytesReceived();
	} else {
		$('#bridgeModeInfo').hide();
	}
	if (fppMode == 8) {
		// Remote Mode
		$('#fppTime').html(jsonStatus.time);

		if (
			(jsonStatus.time_elapsed != '00:00' && jsonStatus.time_elapsed != '') ||
			(jsonStatus.time_remaining != '00:00' && jsonStatus.time_remaining != '')
		) {
			status =
				'Syncing to Player: Elapsed: ' +
				jsonStatus.time_elapsed +
				'&nbsp;&nbsp;&nbsp;&nbsp;Remaining: ' +
				jsonStatus.time_remaining;
		} else {
			status = 'Waiting for MultiSync commands';
		}

		$('#txtRemoteStatus').html(status);
		$('#txtRemoteSeqFilename').html(jsonStatus.sequence_filename);
		$('#txtRemoteMediaFilename').html(jsonStatus.media_filename);

		if (firstStatusLoad || $('#syncStatsLiveUpdate').is(':checked'))
			GetMultiSyncStats();
	} else {
		// Player Mode
		var nextPlaylist = jsonStatus.next_playlist;
		var nextPlaylistStartTime = jsonStatus.next_playlist_start_time;
		var currentPlaylist = jsonStatus.current_playlist;

		if (fppStatus == STATUS_IDLE) {
			// Not Playing Anything
			gblCurrentPlaylistIndex = 0;
			gblCurrentPlaylistEntryType = '';
			gblCurrentPlaylistEntrySeq = '';
			gblCurrentPlaylistEntrySong = '';
			$('#txtPlayerStatus').html(status);
			$('#playerTime').hide();
			$('#txtSeqFilename').html('');
			$('#txtMediaFilename').html('');
			$('#schedulerStatus').html('Idle');
			$('.schedulerStartTime').hide();
			$('.schedulerEndTime').hide();
			$('body').removeClass('schedulderStatusPlaying');

			// Update global pause and randomize indicators based on selected playlist
			if (typeof window.updateMainPageGlobalPauseIndicator === 'function') {
				window.updateMainPageGlobalPauseIndicator();
			}

			// Enable Play
			SetButtonState('#btnPlay', 'enable');
			SetButtonState('#btnStopNow', 'disable');
			SetButtonState('#btnPrev', 'disable');
			SetButtonState('#btnNext', 'disable');
			SetButtonState('#btnStopGracefully', 'disable');
			SetButtonState('#btnStopGracefullyAfterLoop', 'disable');
			SetCheckBoxState('#chkRepeat', 'enable');
			$('#playlistSelect').removeAttr('disabled');
			UpdateCurrentEntryPlaying(0);
		} else if (currentPlaylist.playlist != '') {
			// Playing a playlist
			var playerStatusText = 'Playing ';
			if (fppStatus == STATUS_PAUSED) {
				playerStatusText = 'Paused ';
			}
			if (jsonStatus.current_song != '') {
				playerStatusText +=
					" - <strong>'" + jsonStatus.current_song + "'</strong>";
				if (jsonStatus.current_sequence != '') {
					playerStatusText += '/';
				}
			}
			if (jsonStatus.current_sequence != '') {
				if (jsonStatus.current_song == '') {
					playerStatusText += ' - ';
				}
				playerStatusText +=
					"<strong>'" + jsonStatus.current_sequence + "'</strong>";
			}
			var repeatMode = jsonStatus.repeat_mode;
			if (
				gblCurrentLoadedPlaylist != currentPlaylist.playlist ||
				gblCurrentPlaylistIndex != currentPlaylist.index ||
				gblCurrentPlaylistEntryType != currentPlaylist.type ||
				gblCurrentPlaylistEntrySeq != jsonStatus.current_sequence ||
				gblCurrentPlaylistEntrySong != jsonStatus.current_song
			) {
				$('#playlistSelect').val(currentPlaylist.playlist);
				PopulatePlaylistDetailsEntries(false, currentPlaylist.playlist);

				gblCurrentPlaylistEntryType = currentPlaylist.type;
				gblCurrentPlaylistEntrySeq = jsonStatus.current_sequence;
				gblCurrentPlaylistEntrySong = jsonStatus.current_song;
			}

			SetButtonState('#btnPlay', 'enable');
			SetButtonState('#btnStopNow', 'enable');
			SetButtonState('#btnPrev', 'enable');
			SetButtonState('#btnNext', 'enable');
			SetButtonState('#btnStopGracefully', 'enable');
			SetButtonState('#btnStopGracefullyAfterLoop', 'enable');
			SetCheckBoxState('#chkRepeat', 'disable');

			$('#playlistSelect').attr('disabled');

			if (fppStatus == STATUS_STOPPING_GRACEFULLY) {
				playerStatusText += ' - Stopping Gracefully';
			} else if (fppStatus == STATUS_STOPPING_GRACEFULLY_AFTER_LOOP) {
				playerStatusText += ' - Stopping Gracefully After Loop';
			}
			txtPlayerStatusLabel = 'Player Status';
			if (
				Array.isArray(jsonStatus['breadcrumbs']) &&
				jsonStatus.breadcrumbs.length > 0
			) {
				txtPlayerStatusLabel += ' (';
				jsonStatus.breadcrumbs.forEach(function (r) {
					txtPlayerStatusLabel += r + ' -> ';
				});
				txtPlayerStatusLabel += jsonStatus.current_playlist.playlist;
				txtPlayerStatusLabel += ')';
			}
			txtPlayerStatusLabel += ':';
			$('#txtPlayerStatusLabel').html(txtPlayerStatusLabel);
			$('#txtPlayerStatus').html(playerStatusText);
			$('#playerTime').show();
			$('#txtTimePlayed').html(jsonStatus.time_elapsed);
			$('#txtTimeRemaining').html(jsonStatus.time_remaining);
			jsonStatus.percentage_played = (
				(parseInt(jsonStatus.seconds_elapsed) /
					(parseInt(jsonStatus.seconds_elapsed) +
						parseInt(jsonStatus.seconds_remaining))) *
				100
			).toFixed(0);
			$('#playerTime .progress')[0].setAttribute(
				'aria-valuenow',
				jsonStatus.percentage_played
			);
			$('#playerTime .progress > .progress-bar')[0].setAttribute(
				'style',
				'width: ' + jsonStatus.percentage_played + '%'
			);
			$('#txtPercentageComplete p').html(jsonStatus.percentage_played + '%');
			$('#txtSeqFilename').html(jsonStatus.current_sequence);
			$('#txtMediaFilename').html(jsonStatus.current_song);

			//				if(currentPlaylist.index != gblCurrentPlaylistIndex &&
			//					currentPlaylist.index <=
			// gblCurrentLoadedPlaylistCount) {
			// FIXME, somehow this doesn't refresh on the first page load, so refresh
			// every time for now
			if (1) {
				UpdateCurrentEntryPlaying(currentPlaylist.index);
				gblCurrentPlaylistIndex = currentPlaylist.index;
			}

			if (repeatMode) {
				$('#chkRepeat').prop('checked', true);
			} else {
				$('#chkRepeat').prop('checked', false);
			}

			// Randomised / Global Pause indicators are only shown when the user
			// has enabled Verbose Playlist Item Details.
			var showPlaylistIndicators = $('#verbosePlaylistItemDetails').is(
				':checked'
			);

			// Update randomize indicator - only show when enabled
			if (showPlaylistIndicators && jsonStatus.random && jsonStatus.random > 0) {
				$('#randomizeIndicator').show();
				if (jsonStatus.random == 1) {
					$('#randomizeStatus').text('Once at load time');
				} else if (jsonStatus.random == 2) {
					$('#randomizeStatus').text('Once per iteration');
				}
			} else {
				$('#randomizeIndicator').hide();
			}

			// Update global pause indicator - only show for playlists
			if (
				showPlaylistIndicators &&
				jsonStatus.global_pause &&
				jsonStatus.global_pause.configured
			) {
				$('#globalPauseIndicator').show();
				if (jsonStatus.global_pause.active) {
					$('#globalPauseStatus')
						.removeClass('btn-info btn-warning')
						.addClass('btn-danger')
						.text('Active');
					// Removed countdown display - main elapsed/remaining time shows pause progress
				} else {
					$('#globalPauseStatus')
						.removeClass('btn-danger btn-warning')
						.addClass('btn-info')
						.text('Configured');
				}
			} else {
				$('#globalPauseIndicator').hide();
			}

			if (jsonStatus.scheduler != '') {
				if (jsonStatus.scheduler.status == 'playing') {
					var pl = jsonStatus.scheduler.currentPlaylist;
					$('#schedulerStatus').html(
						"Playing <b>'" + pl.playlistName + "'</b>"
					);
					$('body').addClass('schedulderStatusPlaying');
					$('.schedulerStartTime').show();
					$('#schedulerStartTime').html(
						pl.actualStartTimeStr.replace(' @ ', '<br>')
					);
					$('.schedulerEndTime').show();
					$('#schedulerEndTime').html(
						pl.actualEndTimeStr.replace(' @ ', '<br>')
					);
					$('#schedulerStopType').html(pl.stopTypeStr);

					if (
						fppStatus == STATUS_STOPPING_GRACEFULLY ||
						fppStatus == STATUS_STOPPING_GRACEFULLY_AFTER_LOOP
					) {
						$('.schedulerExtend').hide();
					} else {
						$('.schedulerExtend').show();
					}
				} else if (jsonStatus.scheduler.status == 'manual') {
					var pl = jsonStatus.scheduler.currentPlaylist;
					$('#schedulerStatus').html(
						"Playing <b>'" + pl.playlistName + "'</b> (manually started)"
					);
					$('body').addClass('schedulderStatusPlaying');
					$('.schedulerStartTime').hide();
					$('.schedulerEndTime').hide();
				} else {
					$('#schedulerStatus').html('Idle');
					$('body').removeClass('schedulderStatusPlaying');
					$('.schedulerStartTime').hide();
					$('.schedulerEndTime').hide();
				}
			} else {
				$('#schedulerStatus').html('Idle');
				$('body').removeClass('schedulderStatusPlaying');
				$('#schedulerStartTime').html('N/A');
				$('#schedulerEndTime').html('N/A');
			}
		} else if (jsonStatus.current_sequence != '') {
			//  Playing a sequence via test mode
			var playerStatusText = 'Playing ';
			if (fppStatus == STATUS_PAUSED) {
				playerStatusText = 'Paused ';
			}
			playerStatusText =
				"<strong>'" + jsonStatus.current_sequence + "'</strong>";
			SetButtonState('#btnPlay', 'disable');
			SetButtonState('#btnPrev', 'enable');
			SetButtonState('#btnNext', 'enable');
			SetButtonState('#btnStopNow', 'enable');
			SetButtonState('#btnStopGracefully', 'enable');
			SetButtonState('#btnStopGracefullyAfterLoop', 'enable');

			$('#txtPlayerStatus').html(playerStatusText);
			$('#playerTime').show();
			$('#txtTimePlayed').html(jsonStatus.time_elapsed);
			$('#txtTimeRemaining').html(jsonStatus.time_remaining);
			$('#txtSeqFilename').html(jsonStatus.current_sequence);
			$('#txtMediaFilename').html(jsonStatus.current_song);
		}
		setTimeout(function () {
			SetHomepageStatusRowWidthForMobile();
		}, 100);

		$('#fppTime').html(jsonStatus.time);

		var npl = jsonStatus.scheduler.nextPlaylist;
		if (npl.scheduledStartTimeStr != '')
			$('#nextPlaylist').html(
				"'" + npl.playlistName + "' on " + npl.scheduledStartTimeStr
			);
		else $('#nextPlaylist').html('No playlist scheduled.');
	}
	const pph = document.querySelector('#powerPlaceHolder');
	if (jsonStatus['powerBad']) {
		pph.innerHTML = "<i class='fas fa-2xl fa-bolt' style='color:yellow;''></i>";
	} else {
		pph.textContent = '';
	}

	updateSensorStatus(jsonStatus);
	firstStatusLoad = 0;
}

function niceDuration (ms) {
	var t = ms;
	if (t <= 0) {
		return '&lt; 1 min';
	}

	if (t < 1000) {
		return '' + t + ' ms';
	}
	t = t / 1000; // in sec now

	if (t < 60) {
		return '' + Math.round(t) + ' sec';
	}

	t = t / 60; // in min now
	if (t < 60) {
		return '' + Math.round(t) + ' min';
	}

	t = t / 60;
	return '' + Math.round(t) + ' hours';
}

function ShowMultiSyncStats (data) {
	var master =
		"<a href='" + buildHttpURL(data.masterIP, '') + "'>" + data.masterIP + '</a>';
	if (data.masterHostname != '') master += ' (' + data.masterHostname + ')';

	$('#syncMaster').html(master);

	var now = new Date().getTime();
	var rows = [];

	// Sort the syncing player (master) first, then the rest.
	var systems = data.systems.slice();
	systems.sort(function (a, b) {
		if (data.masterIP) {
			var aIsMaster = a.sourceIP === data.masterIP;
			var bIsMaster = b.sourceIP === data.masterIP;
			if (aIsMaster !== bIsMaster) return aIsMaster ? -1 : 1;
		}
		return 0;
	});

	for (var i = 0; i < systems.length; i++) {
		var s = systems[i];
		// Hide the local host's own loopback stats row.
		if (s.sourceIP === '127.0.0.1') continue;
		var hostText = s.sourceIP;
		if (s.hostname != '') hostText += '&nbsp;(' + s.hostname + ')';
		if (s.sourceIP === data.masterIP) hostText += ' <b>(Player)</b>';

		var ms = now - new Date(s.lastReceiveTime).getTime();
		rows.push({
			host: hostText,
			lastrcvd:
				'<span title="' +
				s.lastReceiveTime +
				'">' +
				niceDuration(ms) +
				'</span>',
			seqopen: s.pktSyncSeqOpen,
			seqstart: s.pktSyncSeqStart,
			seqstop: s.pktSyncSeqStop,
			seqsync: s.pktSyncSeqSync,
			medopen: s.pktSyncMedOpen,
			medstart: s.pktSyncMedStart,
			medstop: s.pktSyncMedStop,
			medsync: s.pktSyncMedSync,
			blank: s.pktBlank,
			ping: s.pktPing,
			plugin: s.pktPlugin,
			fppcmd: s.pktFPPCommand,
			errors: s.pktError
		});
	}

	var $tbl = $('#syncStatsTable');
	if ($tbl.closest('.bootstrap-table').length) {
		$tbl.bootstrapTable('load', rows);
	}
}

function ResetMultiSyncStats () {
	$.get('api/fppd/multiSyncStats?reset=1', function (data) {
		ShowMultiSyncStats(data);
	});
}

function GetMultiSyncStats () {
	$.get('api/fppd/multiSyncStats', function (data) {
		ShowMultiSyncStats(data);
	});
}

function ResetUniverseBytesReceived () {
	$.ajax({ url: 'api/channel/input/stats', type: 'DELETE' })
		.done(function (data) {
			$.jGrowl('Stat Reset Complete', { themeState: 'success' });
		})
		.fail(function () {
			DialogError(
				'Stat Reset Failed',
				'Stat Reset failed. Check if FPPD is running.'
			);
		});
}

function GetUniverseBytesReceived () {
	var html = [];
	var html1 = '';
	var html2 = '';
	$.get('api/channel/input/stats')
		.done(function (data) {
			if (data.status == 'OK') {
				var maxRows = data.universes.length / 3;
				if (maxRows < 32) {
					maxRows = 32;
				}
				// Only show the Suppressed column when the priority feature has
				// actually suppressed something; it is "-" or 0 otherwise.
				var showSuppressed = data.universes.some(function (u) {
					return parseInt(u.suppressedPackets) > 0;
				});
				var tableHeader =
					'<thead><tr id="rowReceivedBytesHeader"><th>Universe</th><th>Start Address</th><th>Packets</th><th>Bytes</th><th>Errors</th>' +
					(showSuppressed ? '<th>Suppressed</th>' : '') +
					'</tr></thead><tbody>';
				var nextRows = maxRows;
				if (data.universes.length > 0) {
					html.push(
						'<table class="fppBridgeStatsTable fppSelectableRowTable">'
					);
					html.push(tableHeader);
				}
				for (i = 0; i < data.universes.length; i++) {
					if (i == nextRows) {
						nextRows += maxRows;
						html.push('</table>');
						if (html1 == '') {
							html1 = html.join('');
						} else {
							html2 = html.join('');
						}
						html = [];
						html.push(
							'<table class="fppBridgeStatsTable fppSelectableRowTable">'
						);
						html.push(tableHeader);
					}
					html.push('<tr><td>');
					html.push(data.universes[i].id);
					html.push('</td><td>');
					html.push(data.universes[i].startChannel);
					html.push('</td><td>');
					html.push(data.universes[i].packetsReceived);
					html.push('</td><td>');
					html.push(data.universes[i].bytesReceived);
					html.push('</td><td>');
					html.push(data.universes[i].errors);
					if (showSuppressed) {
						html.push('</td><td>');
						html.push(
							data.universes[i].suppressedPackets === undefined ?
								'-'
							:	data.universes[i].suppressedPackets
						);
					}
					html.push('</td></tr>');
				}
				html.push('</tbody></table>');
				if (html1 != '') {
					$('#bridgeStatistics1').html(html1);
					if (html2 != '') {
						$('#bridgeStatistics2').html(html2);
						$('#bridgeStatistics3').html(html.join(''));
					} else {
						$('#bridgeStatistics2').html(html.join(''));
						$('#bridgeStatistics3').html('');
					}
				} else {
					$('#bridgeStatistics1').html(html.join(''));
					$('#bridgeStatistics2').html('');
					$('#bridgeStatistics3').html('');
				}

				UpdatePlayerModeInfoVisibility();
			} else {
				// data.status != OK
				$('#bridgeStatistics1').html(
					'Bridge Data not avaiable -- ' + data.status
				);
				$('#bridgeStatistics2').html('');
				$('#bridgeStatistics3').html('');
			}
		})
		.fail(function () {
			$('#bridgeStatistics1').html(
				'Failed to refresh Bridge Stats - Unknown Error'
			);
			$('#bridgeStatistics2').html('');
			$('#bridgeStatistics3').html('');
		});
}

function UpdateCurrentEntryPlaying (index, lastIndex) {
	$('#tblPlaylistDetails tr').removeClass('PlaylistRowPlaying');
	$('#tblPlaylistDetails td').removeClass('PlaylistPlayingIcon');

	if (index >= 0 && $('#playlistRow' + index).length) {
		if (!$('#playlistRow' + index).hasClass('PlaylistRowPlaying')) {
			if (settings['playlistAutoScroll'] > 0) {
				var topPos = document.getElementById('playlistRow' + index).offsetTop;
				document.getElementById('playlistOuterScroll').scrollTop = topPos + 100;
			}
		}
		$('#colEntryNumber' + index).addClass('PlaylistPlayingIcon');
		$('#playlistRow' + index).addClass('PlaylistRowPlaying');
	}
}

function SetIconForCurrentPlayingEntry (index) {}

function StopGracefully () {
	var url = 'api/playlists/stopgracefully';
	$.get(url)
		.done(function () {})
		.fail(function () {});
}

function StopGracefullyAfterLoop () {
	var url = 'api/playlists/stopgracefullyafterloop';
	$.get(url)
		.done(function () {})
		.fail(function () {});
}

function StopNow () {
	var url = 'api/playlists/stop';
	$.get(url)
		.done(function () {})
		.fail(function () {});
}

function ToggleSequencePause () {
	var url = 'api/sequence/current/togglePause';

	$.get(url)
		.done(function () {
			$.jGrowl('Pause/Resume', { themeState: 'success' });
		})
		.fail(function () {
			DialogError('Failed to Pause / Resume', 'Start failed');
		});
}

function SingleStepSequence () {
	var url = 'api/sequence/current/step';

	$.get(url)
		.done(function () {
			$.jGrowl('Sequence Step', { themeState: 'success' });
		})
		.fail(function () {
			DialogError('Failed Step Current Sequence', 'Step failed');
		});
}

// ---------------------------------------------------------------------------
// Saving a setting is not instant.  The API writes the value and then APPLIES
// it before it replies, and some of those applies are slow: restarting a
// service, rewriting /etc/hostname, hashing a password with yescrypt, reloading
// apache.  Several seconds with no feedback looks like the UI ignored the
// change, so the setting's row gets a spinner for as long as its save is in
// flight (the '.row.settingSaving' rule in css/fpp.css).
//
// The row is '<setting>Row', emitted by PrintSetting() in common.php, so every
// setting type - checkbox, select, text, number, colour - gets the indicator
// from here rather than each generated onChange handler rolling its own.
function SettingRow (key) {
	// Setting names may contain '.' (e.g. 'backup.Path'), which is a class
	// separator in a jQuery selector unless it is escaped.
	return $('#' + String(key).replace(/\./g, '\\.') + 'Row');
}

function SettingSaveStarted (key) {
	SettingRow(key).addClass('settingSaving');
}

function SettingSaveFinished (key) {
	SettingRow(key).removeClass('settingSaving');
}

// Setting saves used to be synchronous XHRs.  That serialized them, but it also
// froze the browser for the whole request, so nothing the change handler drew
// beforehand - a spinner included - could ever reach the screen.  They are async
// now; queue them so exactly one is in flight at a time and the ordering the
// synchronous calls used to guarantee is kept.  Rows waiting their turn show
// their spinner straight away.
var settingSaveQueue = [];
var settingSaveInFlight = false;

function QueueSettingSave (fn) {
	settingSaveQueue.push(fn);
	if (!settingSaveInFlight) {
		RunNextSettingSave();
	}
}

function RunNextSettingSave () {
	var fn = settingSaveQueue.shift();
	if (!fn) {
		settingSaveInFlight = false;
		return;
	}
	settingSaveInFlight = true;
	fn();
}

function SetSettingReboot (key, value) {
	SetSetting(key, value, 0, 1);
}

function SetSetting (
	key,
	value,
	restart,
	reboot,
	hideChange = false,
	isBool = null,
	callback = '',
	failCallback = ''
) {
	// console.log("api/settings/", key);

	// The restart/reboot flags are written from inside another save's success
	// handler, whose callback may reload the page - the whole point of setting
	// them there (see the comment further down).  Keep those two synchronous and
	// out of the queue so the flag reaches disk before a reload can cancel the
	// request.  They have no apply step, so the block is just the round trip.
	if (key == 'restartFlag' || key == 'rebootFlag') {
		$.ajax({
			url: 'api/settings/' + key,
			data: '' + value,
			method: 'PUT',
			async: false,
			success: function () {
				settings[key] = value;
			}
		}).fail(function () {
			DialogError('Save Setting', 'Failed to save ' + key + ' setting.');
			if (typeof failCallback === 'function') {
				failCallback();
			}
		});
		return;
	}

	// Silent background writes (hideChange) are not something the user asked
	// for, so they get no spinner; everything else marks its row before the
	// request goes out.
	if (!hideChange) {
		SettingSaveStarted(key);
	}
	QueueSettingSave(function () {
		$.ajax({
			url: 'api/settings/' + key,
			data: '' + value,
			method: 'PUT',
			// The reply does not come back until the value has been applied, and
			// an apply can legitimately run for several seconds.  The timeout is
			// only here so a connection that dies mid-save cannot wedge the queue
			// behind a request that will never complete.
			timeout: 120000,
			complete: function () {
				SettingSaveFinished(key);
				RunNextSettingSave();
			},
			success: function () {
				settings[key] = value;

				// Set restart/reboot flags BEFORE callback to ensure they're saved
				// even if callback reloads the page
				if (restart > 0 && restart != settings['restartFlag']) {
					SetRestartFlag(restart);
				}
				if (reboot > 0 && reboot != settings['rebootFlag']) {
					SetRebootFlag(reboot);
				}
				CheckRestartRebootFlags();

				if (!hideChange) {
					if (isBool === null) {
						$.jGrowl(key + ' setting saved.', { themeState: 'success' });
					} else if (isBool) {
						$.jGrowl(key + ' Enabled.', { themeState: 'success' });
					} else {
						$.jGrowl(key + ' Disabled.', { themeState: 'detract' });
					}
				}
				if (typeof callback === 'function') {
					callback();
				}
			}
		}).fail(function () {
			if (isBool === null) {
				DialogError('Save Setting', 'Failed to save ' + key + ' setting.');
			} else if (isBool) {
				DialogError('Save Setting', 'Failed to Enable ' + key + '.');
			} else {
				DialogError('Save Setting', 'Failed to Disable ' + key + '.');
			}
			if (typeof failCallback === 'function') {
				failCallback();
			}
			CheckRestartRebootFlags();
		});
	});
}

function SetPluginSetting (
	plugin,
	key,
	value,
	restart,
	reboot,
	isBool = false,
	callback = '',
	failCallback = ''
) {
	SettingSaveStarted(key);
	QueueSettingSave(function () {
		$.ajax({
			url: 'api/plugin/' + plugin + '/settings/' + key,
			data: '' + value,
			method: 'PUT',
			// See SetSetting() above.
			timeout: 120000,
			complete: function () {
				SettingSaveFinished(key);
				RunNextSettingSave();
			},
			success: function () {
				if (key != 'restartFlag' && key != 'rebootFlag') {
					// Set restart/reboot flags BEFORE callback to ensure they're saved
					// even if callback reloads the page
					if (restart > 0 && restart != settings['restartFlag']) {
						SetRestartFlag(restart);
					}
					if (reboot > 0 && reboot != settings['rebootFlag']) {
						SetRebootFlag(reboot);
					}
					CheckRestartRebootFlags();

					if (isBool === null) {
						$.jGrowl(key + ' setting saved.', { themeState: 'success' });
					} else if (isBool) {
						$.jGrowl(key + ' Enabled.', { themeState: 'success' });
					} else {
						$.jGrowl(key + ' Disabled.', { themeState: 'detract' });
					}
					if (typeof callback === 'function') {
						callback();
					}
				}
			}
		}).fail(function () {
			if (isBool === null) {
				DialogError('Save Setting', 'Failed to save ' + key + ' setting.');
			} else if (isBool) {
				DialogError('Save Setting', 'Failed to Enable ' + key + '.');
			} else {
				DialogError('Save Setting', 'Failed to Disable ' + key + '.');
			}
			CheckRestartRebootFlags();
			if (typeof failCallback === 'function') {
				failCallback();
			}
		});
	});
}

function ReloadSettingOptions (settingName) {
	$.get('settings.json', function (sdata) {
		$.get(sdata.settings[settingName].optionsURL, function (data) {
			var options = '';
			if (typeof data != 'object') {
				for (var i = 0; i < data.length; i++) {
					options += "<option value='" + data[i] + "'";

					if (
						settings.hasOwnProperty(settingName) &&
						settings[settingName] == data[i]
					)
						options += ' selected';

					options += '>' + data[i] + '</option>';
				}
			} else {
				var keys = Object.keys(data);
				for (var i = 0; i < keys.length; i++) {
					options += "<option value='" + data[keys[i]] + "'";

					if (
						settings.hasOwnProperty(settingName) &&
						settings[settingName] == data[keys[i]]
					)
						options += ' selected';

					options += '>' + keys[i] + '</option>';
				}
			}

			$('#' + settingName).html(options);
			// if the current setting doesn't match any value in the new list,
			// then invoke the change so the currently displayed value is
			// the actual correct value
			if ($('#' + settingName).val() != settings[settingName]) {
				$('#' + settingName).trigger('change');
				settings[settingName] = $('#' + settingName).val();
			}
		});
	});
}

function setTopScrollText (text = 'Top') {
	if (settings['restartFlag'] != 0 || settings['rebootFlag'] != 0) {
		text = 'See Alert';
	}

	if ($('#scrollTopButton').html() != text) {
		$('#scrollTopButton').html(text);
	}
}

function ClearRestartFlag () {
	gblRestartRebootFlagEpoch++;
	settings['restartFlag'] = 0;
	SetSetting('restartFlag', 0, 0, 0);
}

function SetRestartFlag (newValue) {
	// 0 - no restart needed
	// 1 - full restart is needed
	// 2 - quick restart is OK
	if (newValue == 2 && settings['restartFlag'] == 1) return;

	gblRestartRebootFlagEpoch++;
	settings['restartFlag'] = newValue;
	SetSetting('restartFlag', newValue, newValue, 0);
}

function ClearRebootFlag () {
	gblRestartRebootFlagEpoch++;
	settings['rebootFlag'] = 0;
	SetSetting('rebootFlag', 0, 0, 0);
}

function SetRebootFlag () {
	if (settings['Platform'] == 'MacOS') {
		// no reboot on MacOS, just restart
		SetRestartFlag(2);
	} else {
		gblRestartRebootFlagEpoch++;
		settings['rebootFlag'] = 1;
		SetSettingReboot('rebootFlag', 1);
	}
}

/*
 * Copy the restart/reboot flags out of a status response into settings[] and
 * refresh the banners.
 *
 * Only api/system/status produces these two fields -- fppd's own status (and so
 * the WebSocket push) has never carried them.  They therefore refresh at the
 * PHP poll's cadence, which in WebSocket mode is gblSystemAugRefreshSeconds
 * (30s), and must only be applied when a response actually delivered them.
 * Applying whatever happens to be sitting in the merged lastStatusJSON cache on
 * every WebSocket tick instead re-asserts a value up to 30s old once a second,
 * which showed up as the restart banner flashing and vanishing right after a
 * setting was saved.
 *
 * `epoch` is gblRestartRebootFlagEpoch as of when the request was issued; a
 * mismatch means the page changed a flag while this response was in flight, so
 * the response predates the change and is dropped.
 */
function applyServerRestartRebootFlags (response, epoch) {
	if (!response || typeof settings === 'undefined') return;
	if (epoch !== gblRestartRebootFlagEpoch) return;

	var applied = false;
	if (response.rebootFlag != undefined) {
		settings['rebootFlag'] = response.rebootFlag;
		applied = true;
	}
	if (response.restartFlag != undefined) {
		settings['restartFlag'] = response.restartFlag;
		applied = true;
	}
	if (applied) {
		CheckRestartRebootFlags();
	}
}

function showRestartAlert () {
	if ($('#restartFlag').is(':hidden')) {
		$('#restartFlag').show();
		common_ViewPortChange();
	}
}

function hideRestartAlert () {
	if ($('#restartFlag').is(':visible')) {
		$('#restartFlag').hide();
		common_ViewPortChange();
	}
}

function showRebootAlert () {
	if ($('#rebootFlag').is(':hidden')) {
		$('#rebootFlag').show();
		common_ViewPortChange();
	}
}

function hideRebootAlert () {
	if ($('#rebootFlag').is(':visible')) {
		$('#rebootFlag').hide();
		common_ViewPortChange();
	}
}

function CheckRestartRebootFlags () {
	if (typeof settings !== 'undefined') {
		// Suppress restart/reboot banners while the initial setup wizard hasn't
		// completed yet (e.g. a reboot flagged by the automatic first-boot rootfs
		// expansion) - it reads as caused by the user before they've done anything.
		// Once initialSetup-02 flips to 1 (wizard finished or short-circuited by a
		// restore), any still-pending flag is shown normally.
		var setupIncomplete = settings['initialSetup-02'] != 1 && settings['initialSetup-02'] != '1';
		if (settings['disableUIWarnings'] == 1 || setupIncomplete) {
			setTopScrollText('Top');
			hideRestartAlert();
			hideRebootAlert();
			return;
		}

		if (settings['restartFlag'] >= 1) {
			showRestartAlert();
		} else {
			hideRestartAlert();
		}

		if (settings['rebootFlag'] == 1) {
			hideRestartAlert();
			showRebootAlert();
		} else {
			hideRebootAlert();
		}

		// Adjust the scroll up text to match state.
		setTopScrollText();
	}
}

function SkipBootDelay () {
	$.post('api/system/fppd/skipBootDelay')
		.done(function (data) {
			$.jGrowl('Boot delay skip requested - FPPD will start shortly', {
				themeState: 'success'
			});
		})
		.fail(function () {
			DialogError('Skip Boot Delay', 'Failed to skip boot delay');
		});
}

function RestartFPPD () {
	var args = '';

	// Perform a quick restart if requested or if no restart is required
	if (settings['restartFlag'] == 2 || settings['restartFlag'] == 0)
		args = '?quick=1';

	clearTimeout(statusTimeout);
	$('html,body').css('cursor', 'wait');
	$.get('api/system/fppd/restart' + args)
		.done(function () {
			$('html,body').css('cursor', 'auto');
			$.jGrowl('FPPD Restarted', { themeState: 'success' });
			ClearRestartFlag();
			LoadSystemStatus();
		})
		.fail(function () {
			$('html,body').css('cursor', 'auto');
			LoadSystemStatus();

			// If fail, the FPP may have rebooted (eg.FPPD triggering a reboot due to disabling
			// soundcard or activating Pi Pixel output start polling and see if we can wait for the FPP
			// to come back up restartFlag will already be cleared, but to keep things simple, just call
			// the original code
			retries = 0;
			retries_max = max_retries; // avg timeout is 10-20seconds, polling resolves after 6-10 polls
			// attempt to poll every second, AJAX block for the default browser timeout if host is
			// unavail
			retry_poll_interval_arr['restartFPPD'] = setInterval(function () {
				poll_result = false;
				if (retries < retries_max) {
					// console.log("Polling @ " + retries);
					$.ajax({
						url: 'api/system/status',
						timeout: 1000,
						async: true,
						success: function (data) {
							if ('fppd' in data && data.fppd == 'running') {
								poll_result = true;
								// FPP is up then
								clearInterval(retry_poll_interval_arr['restartFPPD']);
								// run original code for success
								$.jGrowl('FPPD Restarted', { themeState: 'success' });
								ClearRestartFlag();
							} else {
								retries++;
							}
						}
					}).fail(function () {
						poll_result = false;
						retries++;
						// If on first try throw up a FPP is rebooting notification
						if (retries === 1) {
							// Show FPP is rebooting notification for 10 seconds
							$.jGrowl('FPP is rebooting..', {
								life: 10000,
								themeState: 'detract'
							});
						}
					});

					// console.log("Polling Result " + poll_result);
				} else {
					// run original code
					clearInterval(retry_poll_interval_arr['restartFPPD']);
					DialogError('Restart FPPD', 'Error restarting FPPD');
				}
			}, 2000);
		});
}

function zeroPad (num, places) {
	var zero = places - num.toString().length + 1;
	return Array(+(zero > 0 && zero)).join('0') + num;
}

function ControlFPPD () {
	var url = 'api/system/fppd/';
	var btnVal = $('#btnDaemonControl').attr('value');

	if (btnVal == 'Stop FPPD') {
		url = url + 'stop';
	} else {
		url = url + 'start';
	}

	$.get({
		url: url,
		data: ''
	})
		.done(function (data) {
			$.jGrowl('Completed ' + btnVal, { themeState: 'success' });
			IsFPPDrunning();
		})
		.fail(function () {
			DialogError('ERROR', 'Error Settng fppMode to ' + modeText);
		});
}

function PopulatePlaylists (sequencesAlso, options) {
	let onPlaylistArrayLoaded = function () {};
	if (options && typeof options.onPlaylistArrayLoaded === 'function') {
		onPlaylistArrayLoaded = options.onPlaylistArrayLoaded;
	}

	// Show loading placeholder
	$('#playlistSelect').html('<option>Loading...</option>');

	// Start all AJAX calls in parallel
	let playlistPromise = $.getJSON('api/playlists/validate');
	let sequencePromise = sequencesAlso
		? $.getJSON('api/sequence')
		: $.Deferred().resolve([]).promise();
	let mediaPromise = sequencesAlso
		? $.getJSON('api/media')
		: $.Deferred().resolve([]).promise();

	$.when(playlistPromise, sequencePromise, mediaPromise)
		.done(function (playlists, sequences, media) {
			// playlists, sequences, media are arrays like [data, status, xhr]
			let playlistOptionsText = '';
			playListArray = playlists[0];
			sequenceArray = sequences[0];
			mediaArray = media[0];

			if (sequencesAlso) {
				// Default to a placeholder rather than auto-selecting the first
				// playlist (whose details aren't loaded on page load anyway).
				playlistOptionsText +=
					'<option value="" selected>-- Select Playlist or Sequence --</option>';
				playlistOptionsText += "<optgroup label='Playlists'>";
			}
			for (let j = 0; j < playListArray.length; j++) {
				playlistOptionsText +=
					'<option value="' +
					playListArray[j].name +
					'">' +
					playListArray[j].name +
					'</option>';
			}
			if (sequencesAlso) {
				playlistOptionsText += "</optgroup><optgroup label='Sequences'>";
				for (let j = 0; j < sequenceArray.length; j++) {
					playlistOptionsText +=
						'<option value="' +
						sequenceArray[j] +
						'.fseq">' +
						sequenceArray[j] +
						'.fseq</option>';
				}
				playlistOptionsText += '</optgroup><optgroup label="Media">';
				for (let j = 0; j < mediaArray.length; j++) {
					playlistOptionsText +=
						'<option value="' +
						mediaArray[j] +
						'">' +
						mediaArray[j] +
						'</option>';
				}
				playlistOptionsText += '</optgroup>';
			}
			$('#playlistSelect').html(playlistOptionsText);

			// Call callback if provided
			onPlaylistArrayLoaded();

			// Now that the playlist/sequence arrays are populated, re-evaluate
			// whether the player controls should be shown. This corrects the
			// race where a bridging device's first status update hides
			// #playerModeInfo because these arrays had not loaded yet.
			UpdatePlayerModeInfoVisibility();
		})
		.fail(function () {
			$('#playlistSelect').html('<option>Error loading playlists</option>');
		});
}

/* LEGACY FUNCTION COMMENTED OUT UNTIL NEW APPROACH CONFIRMED STABLE

function PopulatePlaylists (sequencesAlso, options) {
	var playlistOptionsText = '';
	var onPlaylistArrayLoaded = function () {};
	if (options) {
		if (typeof options.onPlaylistArrayLoaded === 'function') {
			onPlaylistArrayLoaded = options.onPlaylistArrayLoaded;
		}
	}
	GetPlaylistArray(onPlaylistArrayLoaded);

	if (sequencesAlso) playlistOptionsText += "<optgroup label='Playlists'>";

	for (j = 0; j < playListArray.length; j++) {
		playlistOptionsText +=
			'<option value="' +
			playListArray[j].name +
			'">' +
			playListArray[j].name +
			'</option>';
	}

	if (sequencesAlso) {
		playlistOptionsText += "</optgroup><optgroup label='Sequences'>";
		GetSequenceArray();

		for (j = 0; j < sequenceArray.length; j++) {
			playlistOptionsText +=
				'<option value="' +
				sequenceArray[j] +
				'.fseq">' +
				sequenceArray[j] +
				'.fseq</option>';
		}

		playlistOptionsText += '</optgroup>';

		playlistOptionsText += "</optgroup><optgroup label='Media'>";
		GetMediaArray();

		for (j = 0; j < mediaArray.length; j++) {
			playlistOptionsText +=
				'<option value="' + mediaArray[j] + '">' + mediaArray[j] + '</option>';
		}

		playlistOptionsText += '</optgroup>';
	}

	$('#playlistSelect').html(playlistOptionsText);
} */

function PlayPlaylist (Playlist, goToStatus = 0) {
	// Check if UI-started playlists should be protected from schedule override
	var scheduleProtected =
		settings.hasOwnProperty('UIStartedPlaylistsProtected') &&
		settings['UIStartedPlaylistsProtected'] == '1';

	var url =
		'api/command/Start Playlist/' +
		Playlist +
		'/0/false/' +
		(scheduleProtected ? 'true' : 'false');
	$.get(url, function () {
		if (goToStatus) location.href = 'index.php';
		else $.jGrowl('Playlist Started', { themeState: 'success' });
	});
}

function StartPlaylistNow () {
	var Playlist = $('#playlistSelect').val();
	var repeat = $('#chkRepeat').is(':checked') ? true : false;
	// Check if UI-started playlists should be protected from schedule override
	var scheduleProtected =
		settings.hasOwnProperty('UIStartedPlaylistsProtected') &&
		settings['UIStartedPlaylistsProtected'] == '1';

	var obj = {
		command: 'Start Playlist At Item',
		args: [Playlist, PlayEntrySelected, repeat, false, scheduleProtected]
	};
	$.post('api/command', JSON.stringify(obj))
		.done(function () {
			$.jGrowl('Playlist Started', { themeState: 'success' });
		})
		.fail(function () {
			DialogError('Command failed', 'Unable to start Playlist');
		});
}

function StopEffect () {
	if (RunningEffectSelectedId < 0) return;

	var msg = { command: 'Effect Stop', args: [RunningEffectSelectedName] };

	$.post({ url: 'api/command', data: JSON.stringify(msg) })
		.done(function (data) {
			RunningEffectSelectedId = -1;
			RunningEffectSelectedName = '';
			GetRunningEffects();
		})
		.fail(function () {
			DialogError('Command failed', 'Call to Stop Effect Failed');
			GetRunningEffects();
		});
}

var lastRunningEffectsData = null;

function GetRunningEffects () {
	$.get('api/fppd/effects')
		.done(function (data) {
			if ('runningEffects' in data) {
				var isFreshData =
					!lastRunningEffectsData ||
					JSON.stringify(lastRunningEffectsData) !=
						JSON.stringify(data.runningEffects);
				if (data.runningEffects.length > 0) {
					if (isFreshData) {
						$('#tblRunningEffectsBody').html('');
						data.runningEffects.forEach(function (e) {
							if (e.name == RunningEffectSelectedName) {
								$('#tblRunningEffectsBody').append(
									'<tr class="effectSelectedEntry"><td width="5%">' +
										e.id +
										'</td><td width="80%">' +
										e.name +
										'</td><td width="15%"><button class="buttons btn-danger">Stop</button></td></tr>'
								);
							} else {
								$('#tblRunningEffectsBody').append(
									'<tr><td width="5%">' +
										e.id +
										'</td><td width="80%">' +
										e.name +
										'</td><td width="15%"><button class="buttons btn-danger">Stop</button></td></tr>'
								);
							}
							$('#divRunningEffects')
								.removeClass('divRunningEffectsDisabled backdrop-disabled')
								.addClass('divRunningEffectsRunning backdrop-success');
						});
						lastRunningEffectsData = data.runningEffects;
					}
				} else {
					lastRunningEffectsData = null;
					$('#divRunningEffects')
						.addClass('divRunningEffectsDisabled backdrop-disabled')
						.removeClass('divRunningEffectsRunning backdrop-success');
				}
			} else {
				lastRunningEffectsData = null;
				$('#divRunningEffects')
					.addClass('divRunningEffectsDisabled backdrop-disabled')
					.removeClass('divRunningEffectsRunning backdrop-success');
			}
			setTimeout(GetRunningEffects, 1000);
		})
		.fail(function () {
			DialogError(
				'Query Failed',
				'Failed to refresh running effects, reload page to try again.'
			);
		});
}

let SelectedOverlayModel = null;

function GetRunningOverlayEffects () {
	$.getJSON('api/overlays/running')
		.done(function (data) {
			const $tbody = $('#tblOverlayEffectsBody');
			const $table = $('#tblOverlayEffects');
			const $title = $('#overlayEffectsTitle');
			const $container = $('#divOverlayEffects');

			$tbody.empty(); // Clear old rows

			// --- group effect names per model ---
			const models = {}; // { model: [effect, …] }
			$.each(data || [], (_, item) => {
				const model = item?.Name;
				const effect = item?.effect?.name;
				if (!model) return;
				if (!models[model]) models[model] = [];
				if (effect && !models[model].includes(effect))
					models[model].push(effect);
			});

			const anyRunning = Object.keys(models).length > 0;
			$table.toggleClass('fppActionTable-success', anyRunning);

			if (anyRunning) {
				$.each(Object.keys(models).sort(), (_, model) => {
					const effectsText = models[model].join(', ');
					const $tr = $('<tr>');
					if (model === SelectedOverlayModel) {
						$tr.addClass('effectSelectedEntry');
					}

					$('<td width="5%">').text(model).appendTo($tr);
					$('<td width="80%">').text(effectsText).appendTo($tr);
					$('<td width="15%">')
						.append(
							$('<button>', {
								class: 'buttons btn-danger stop-overlay-effects',
								'data-model': model,
								text: 'Stop'
							})
						)
						.appendTo($tr);

					$tbody.append($tr);
				});

				$container
					.removeClass('divOverlayEffectsDisabled backdrop-disabled')
					.addClass('divOverlayEffectsRunning backdrop-success');
			} else {
				// Nothing running
				$container
					.addClass('divOverlayEffectsDisabled backdrop-disabled')
					.removeClass('divOverlayEffectsRunning backdrop-success');
			}

			setTimeout(GetRunningOverlayEffects, 1000);
		})

		.fail(function () {
			$('#tblOverlayEffectsBody').empty();
			$('#tblOverlayEffects').removeClass('fppActionTable-success');
			$('#overlayEffectsTitle').removeClass('text-success');

			$('#divOverlayEffects')
				.removeClass('divOverlayEffectsRunning backdrop-success')
				.addClass('divOverlayEffectsDisabled backdrop-disabled');

			setTimeout(GetRunningOverlayEffects, 1000);
		});
}

function Reboot () {
	DoModalDialog({
		id: 'RebootModal',
		title: 'Reboot FPP Device?',
		noClose: false,
		backdrop: 'static',
		keyboard: false,
		body: 'Are you sure you wish to reboot the device?',
		class: 'modal-sm',
		buttons: {
			Reboot: {
				disabled: false,
				id: 'RebootButton',
				class: 'btn-danger',
				click: function () {
					//CloseModalDialog('RebootModal');
					//location.reload();
					RebootFPP();
					CloseModalDialog('RebootModal');
				}
			},
			Abort: {
				disabled: false,
				id: 'AbortButton',
				click: function () {
					CloseModalDialog('RebootModal');
					location.reload();
				}
			}
		}
	});
}

function RebootFPP () {
	ClearRestartFlag();
	ClearRebootFlag();

	// Delay reboot for 1 second to allow flags to be cleared
	setTimeout(function () {
		$.get({
			url: 'api/system/reboot',
			data: '',
			success: function (data) {
				// Show FPP is rebooting notification for 60 seconds then reload the page
				$.jGrowl('FPP is rebooting..', {
					life: 60000,
					themeState: 'detract'
				});
				setTimeout(function () {
					location.href = 'index.php';
				}, 60000);
			},
			error: function (...args) {
				DialogError(
					'Command failed',
					'Reboot Command failed' + show_details(args)
				);
			}
		});
	}, 1000);
}

function Shutdown () {
	DoModalDialog({
		id: 'ShutdownModal',
		title: 'Shutdown FPP Device?',
		noClose: false,
		backdrop: 'static',
		keyboard: false,
		body: 'Are you sure you wish to shutdown the device?',
		class: 'modal-sm',
		buttons: {
			Shutdown: {
				disabled: false,
				id: 'ShutdownButton',
				class: 'btn-danger',
				click: function () {
					//CloseModalDialog('RebootModal');
					//location.reload();
					ShutdownFPP();
					CloseModalDialog('ShutdownModal');
				}
			},
			Abort: {
				disabled: false,
				id: 'AbortButton',
				click: function () {
					CloseModalDialog('ShutdownModal');
					location.reload();
				}
			}
		}
	});
}

function ShutdownFPP () {
	$.get({
		url: 'api/system/shutdown',
		data: '',
		success: function (data) {
			// Show FPP is rebooting notification for 60 seconds then reload the page
			$.jGrowl('FPP is shutting down..', {
				life: 60000,
				themeState: 'detract'
			});
		},
		error: function (...args) {
			DialogError(
				'Command failed',
				'Shutdown Command failed' + show_details(args)
			);
		}
	});
}

function UpgradePlaylist (data, editMode) {
	var sections = ['leadIn', 'mainPlaylist', 'leadOut'];
	var error = '';

	for (var s = 0; s < sections.length; s++) {
		if (typeof data[sections[s]] != 'object') {
			continue;
		}

		for (i = 0; i < data[sections[s]].length; i++) {
			var type = data[sections[s]][i]['type'];
			var o = data[sections[s]][i];
			var n = {};

			n.enabled = o.enabled;
			n.playOnce = o.playOnce;

			// Changes for both Status UI and Edit Mode.  These are needed in the status UI
			// when new fields replace old fields and where the PlaylistEntry* classes also
			// handle these conversions.
			if (type == 'branch') {
				if (
					(typeof o.startTime === 'undefined' ||
						typeof o.endTime === 'undefined') &&
					typeof o.compInfo != 'undefined'
				) {
					n = o;
					n.startTime =
						PadLeft(o.compInfo.startHour, '0', 2) +
						':' +
						PadLeft(o.compInfo.startMinute, '0', 2) +
						':' +
						PadLeft(o.compInfo.startSecond, '0', 2);

					n.endTime =
						PadLeft(o.compInfo.endHour, '0', 2) +
						':' +
						PadLeft(o.compInfo.endMinute, '0', 2) +
						':' +
						PadLeft(o.compInfo.endSecond, '0', 2);

					delete n.compInfo;
					data[sections[s]][i] = n;
				}

				if (typeof o.branchType != 'undefined') {
					n = o;
					n.branchTest = n.branchType;
					delete n.branchType;
					data[sections[s]][i] = n;
				}
			} else if (type == 'dynamic') {
				if (
					o.subType == 'file' &&
					typeof o.dataFile != 'string' &&
					typeof o.data == 'string'
				) {
					n = o;
					n.dataFile = n.data;
					data[sections[s]][i] = n;
				} else if (
					o.subType == 'plugin' &&
					typeof o.pluginName != 'string' &&
					typeof o.data == 'string'
				) {
					n = o;
					n.pluginName = n.data;
					data[sections[s]][i] = n;
				} else if (
					o.subType == 'url' &&
					typeof o.url != 'string' &&
					typeof o.data == 'string'
				) {
					n = o;
					n.url = n.data;
					data[sections[s]][i] = n;
				}
			}

			// Changes needed only during edit mode when we are upgrading a playlist
			if (editMode) {
				if (type == 'mqtt') {
					n.type = 'command';
					n.command = 'MQTT';

					var args = [];
					args.push(o.topic);
					args.push(o.message);

					n.args = args;

					data[sections[s]][i] = n;
					// 'Run Script' command does not support blocking yet. If this
					// is done, then PlaylistEntryScript.cpp can be deprecated.
					//                } else if (type == 'script') {
					//                    n.type = 'command';
					//                    n.command = 'Run Script';
					//
					//                    var args = [];
					//                    args.push(o.scriptName);
					//                    args.push(o.scriptArgs);
					//
					//                    n.args = args;
					//
					//                    data[sections[s]][i] = n;
				} else if (type == 'volume') {
					n.type = 'command';
					n.command = 'Volume Adjust';
					n.args = [o.volume];

					data[sections[s]][i] = n;
				} else if (type == 'command' && o.command == 'Overlay Model Text') {
					n = o;
					n.command = 'Overlay Model Effect';
					n.multisyncCommand = false;

					var args = [];
					args.push(o.args[0]);
					args.push('Enabled');
					args.push('Text');
					args.push(o.args[1]);
					args.push(o.args[2]);
					args.push(o.args[3]);
					args.push(o.args[4]);
					args.push(o.args[5]);
					args.push(o.args[6]);
					args.push('0');
					args.push(o.args[8]);

					n.args = args;

					data[sections[s]][i] = n;
				}
			}
		}
	}

	return data;
}

// Tracks whether the playlist details were last rendered for the editor (1) or
// the read-only status page (0), so the shared verbose toggle handler knows the
// current context.
var gblPlaylistDetailsEditMode = 0;

// Show/hide the Randomize & Global Pause rows in the Main Playlist header.
// In the playlist editor they are always shown when configured. On the status
// page they only appear when Verbose Playlist Item Details is enabled.
function UpdatePlaylistHeaderDetailVisibility () {
	var allowed =
		gblPlaylistDetailsEditMode == 1 ||
		$('#verbosePlaylistItemDetails').length == 0 ||
		$('#verbosePlaylistItemDetails').is(':checked');
	$('#playlistRandomizeDetails').toggle(
		allowed && $('#playlistRandomizeDetails').data('hasValue') === true
	);
	$('#playlistMainGlobalPauseDetails').toggle(
		allowed && $('#playlistMainGlobalPauseDetails').data('hasValue') === true
	);

	// Update rounded corner on last visible detail field
	$('.tblPlaylistHeaderDetails').each(function () {
		$(this).children('div').removeClass('lastVisibleDetail');
		$(this).children('div:visible:last').addClass('lastVisibleDetail');
	});
}

function PopulatePlaylistDetails (data, editMode, name = '', invalidNames = {}) {
	var innerHTML = '';
	var entries = 0;
	gblPlaylistDetailsEditMode = editMode ? 1 : 0;
	data = UpgradePlaylist(data, editMode);

	if (!editMode) $('#deprecationWarning').hide(); // will re-show if we find any

	var sections = ['leadIn', 'mainPlaylist', 'leadOut'];

	// Build all HTML first (fast, synchronous)
	for (let s = 0; s < sections.length; s++) {
		let idPart = sections[s].charAt(0).toUpperCase() + sections[s].slice(1);

		if (data.hasOwnProperty(sections[s]) && data[sections[s]].length > 0) {
			let sectionData = data[sections[s]];
			innerHTML = '';
			for (var i = 0; i < sectionData.length; i++) {
				innerHTML += GetPlaylistRowHTML(entries, sectionData[i], editMode, invalidNames);
				entries++;
			}
			$('#tblPlaylist' + idPart).html(innerHTML);
			$('#tblPlaylist' + idPart + 'Header')
				.show()
				.parent()
				.addClass('tblPlaylistActive');

			if (!data[sections[s]].length)
				$('#tblPlaylist' + idPart).html(
					"<tr id='tblPlaylist" +
						idPart +
						"PlaceHolder' class='unselectable'><td>&nbsp;</td></tr>"
				);
		} else {
			$('#tblPlaylist' + idPart).html('');
			if (editMode) {
				$('#tblPlaylist' + idPart + 'Header')
					.show()
					.parent()
					.addClass('tblPlaylistActive');
				$('#tblPlaylist' + idPart).html(
					"<tr id='tblPlaylist" +
						idPart +
						"PlaceHolder' class='unselectable'><td>&nbsp;</td></tr>"
				);
			} else {
				$('#tblPlaylist' + idPart + 'Header')
					.hide()
					.parent()
					.removeClass('tblPlaylistActive');
			}
		}
	}

	RenumberPlaylistEditorEntries();
	UpdatePlaylistDurations();
	VerbosePlaylistItemDetailsToggled();

	// Don't fetch durations on load - they're already in the JSON data
	// Only fetch when explicitly needed (like when adding new items)

	if (!editMode) {
		gblCurrentLoadedPlaylist = data.name;
		gblCurrentLoadedPlaylistCount = entries;
		// Don't call UpdatePlaylistDurations() here - moved after global pause value is loaded
	}
	var desc = '';
	if (data.hasOwnProperty('desc')) {
		desc = data.desc;
	}
	$('#txtPlaylistDesc').val(desc);
	if (name == '') {
		SetPlaylistName(data.name);
	} else {
		SetPlaylistName(name);
	}

	if (typeof data.random === 'undefined') {
		$('#randomizePlaylist').val(0);
	} else {
		$('#randomizePlaylist').val(data.random);
	}
	if (data.random == 1) {
		$('#txtRandomize').html('Once at load time');
		$('#playlistRandomizeDetails').data('hasValue', true);
	} else if (data.random == 2) {
		$('#txtRandomize').html('Once per iteration');
		$('#playlistRandomizeDetails').data('hasValue', true);
	} else {
		$('#txtRandomize').html('Off');
		$('#playlistRandomizeDetails').data('hasValue', false);
	}

	// Update main playlist view global pause indicator
	if (
		typeof data.globalPauseBetweenSequencesMS !== 'undefined' &&
		data.globalPauseBetweenSequencesMS > 0
	) {
		$('#txtGlobalPause').html(
			'<span class="btn btn-sm btn-info" style="padding: 2px 6px; font-size: 10px;">' +
				data.globalPauseBetweenSequencesMS +
				'ms</span>'
		);
		$('#playlistMainGlobalPauseDetails').data('hasValue', true);
	} else {
		$('#txtGlobalPause').html(
			'<span class="btn btn-sm btn-secondary" style="padding: 2px 6px; font-size: 10px;">Disabled</span>'
		);
		$('#playlistMainGlobalPauseDetails').data('hasValue', false);
	}

	// Show/hide the Randomize & Global Pause rows (respects verbose setting)
	UpdatePlaylistHeaderDetailVisibility();

	// Load global pause between sequences setting
	if (typeof data.globalPauseBetweenSequencesMS === 'undefined') {
		$('#globalPauseBetweenSequences').val(0);
		window.currentPlaylistGlobalPause = 0;
	} else {
		$('#globalPauseBetweenSequences').val(data.globalPauseBetweenSequencesMS);
		window.currentPlaylistGlobalPause = data.globalPauseBetweenSequencesMS;
	}

	// Update global pause indicator
	updateGlobalPauseIndicator();

	// Update playlist durations now that global pause value is loaded
	if (!editMode) {
		UpdatePlaylistDurations();
	}

	// Also update the main page indicator if this is a playlist edit scenario
	if (typeof window.updateMainPageGlobalPauseIndicator === 'function') {
		window.updateMainPageGlobalPauseIndicator();
	}
}

function PopulatePlaylistDetailsEntries (playselected, playList) {
	var pl;
	var fromMemory = '';
	var url = '';

	if (playselected == true) {
		// Nothing selected (the placeholder option) - nothing to load.
		if (!$('#playlistSelect').val()) {
			return;
		}
		pl = $('#playlistSelect :selected').text();
		url = 'api/playlist/' + pl + '?mergeSubs=1';
	} else {
		pl = playList;
		url = 'api/fppd/playlist/config/'; // In Memory URL
	}

	PlayEntrySelected = 1;

	$.ajax({
		url: url,
		dataType: 'json',
		success: function (data, reqStatus, xhr) {
			PopulatePlaylistDetails(data, 0, pl);
			VerbosePlaylistItemDetailsToggled();
		}
	});
}

function SetVolume (value) {
	var obj = { volume: value };
	if (VolumeChangeAPIInProgress == false) {
		VolumeChangeAPIInProgress = true;
		$.post({ url: 'api/system/volume', data: JSON.stringify(obj) })
			.done(function (data) {
				// Unblock volume UI updates
				settings['volume'] = String(value);
				VolumeChangeInProgress = false;
				VolumeChangeAPIInProgress = false;
			})
			.fail(function () {
				DialogError('ERROR', 'Failed to set volume to ' + value);
				VolumeChangeInProgress = false;
				VolumeChangeAPIInProgress = false;
			});
	}
}

respondToVisibility = function (element, callback) {
	var options = {
		root: document.documentElement
	};

	var observer = new IntersectionObserver((entries, observer) => {
		entries.forEach(entry => {
			callback(entry.intersectionRatio > 0);
		});
	}, options);

	observer.observe(element);
};

function SetFPPDmode (modeText) {
	// var mode = $('#selFPPDmode').val();
	//  var modeText = "unknown"; // 0
	//  if (mode == 1) {
	//      modeText = "bridge";
	//  } else if (mode == 2) {
	//      modeText = "player";
	//  } else if (mode ==6) {
	//      modeText = "master";
	//  } else if (mode == 8) {
	//      modeText = "remote";
	//  }

	$.ajax({ url: 'api/settings/fppMode', type: 'PUT', data: modeText })
		.done(function (data) {
			$.jGrowl('fppMode Saved', { themeState: 'success' });
			RestartFPPD();
		})
		.fail(function () {
			DialogError('ERROR', 'Error Settng fppMode to ' + modeText);
		});
}

function AdjustFPPDModeFromStatus (mode) {
	SetupUIForMode(mode);
	if (mode == 8) {
		// Remote Mode
		$('#selFPPDmode').prop('selectedIndex', 1);
		$('#textFPPDmode').text('Player (Remote)');
	} else {
		// Player
		$('#selFPPDmode').prop('selectedIndex', 0);
		$('#textFPPDmode').text('Player (Player)');
	}
}

// Returns the FPP Model from Local Status
function GetFPPDmodeLocal () {
	if (!lastStatusJSON) {
		return 0;
	}
	if (!('mode' in lastStatusJSON)) {
		return 0;
	}
	return lastStatusJSON.mode;
}

// Calls ajax to get the mode
function GetFPPDmode () {
	$.get('api/settings/fppMode')
		.done(function (data) {
			if ('value' in data) {
				var mode = 0;
				if (data.value == 'player') {
					mode = 2;
				} else if (data.value == 'master') {
					mode = 4;
				} else if (data.value == 'remote') {
					mode = 8;
				}
				SetupUIForMode(mode);
				if (mode == 8) {
					// Remote Mode
					$('#selFPPDmode').prop('selectedIndex', 2);
					$('#textFPPDmode').text('Player (Remote)');
				} else {
					// Player
					$('#selFPPDmode').prop('selectedIndex', 0);
					$('#textFPPDmode').text('Player (Player)');
				}
			} else {
				DialogError('Invalid Mode', 'Mode API returned unexpected value');
			}
		})
		.fail(function (data) {
			DialogError('Failed to query Settings', 'Could not load mode');
		});
}

var helpOpen = 0;
function HelpClosed () {
	helpOpen = 0;
}

function DisplayHelp () {
	if (helpOpen) {
		CloseModalDialog('helpDialog');
		helpOpen = 0;
		return;
	}

	var tmpHelpPage = helpPage;
	var tabs = $('#settingsManagerTabs li .active');

	if (helpPage == 'help/settings.php' && tabs.length == 1) {
		var id = tabs.first().attr('id');
		const re = /settings-(.*)-tab/;
		var tab = '';
		var findings = id.match(re);
		if (findings) {
			tab = findings[1];
		}
		if (tab != '') {
			tmpHelpPage = 'help/settings-' + tab + '.php';
		}
	}
	var options = {
		id: 'helpDialog',
		title: 'Help - Hit F1 or ESC to close',
		body:
			"<div id='helpDialogText'>No help file exists for this page yet.  Check the <a class='link-to-fpp-manual' href='" +
			getManualLink() +
			"' target='_blank'>FPP Manual</a> for more info.</div>",
		close: HelpClosed,
		class: 'modal-dialog-scrollable',
		keyboard: true,
		backdrop: true
	};
	DoModalDialog(options);

	$('#helpDialogText').load(tmpHelpPage);
	helpOpen = 1;
}

function GetGitOriginLog () {
	DoModalDialog({
		id: 'GitOriginLogView',
		title: 'Git Changes',
		backdrop: true,
		keyboard: true,
		body: "<div id='GitOriginLogViewText'>Loading........</div>",
		class: 'modal-dialog-scrollable',
		buttons: {
			Close: {
				id: 'GitOriginLogViewCloseButton',
				click: function () {
					CloseModalDialog('GitOriginLogView');
				}
			}
		}
	});

	$.get({
		url: 'api/git/originLog',
		data: '',
		success: function (data) {
			if ('rows' in data) {
				html = [];
				html.push('<table>');
				data.rows.forEach(function (r) {
					html.push(
						'<tr><td><a href="https://github.com/FalconChristmas/fpp/commit/'
					);
					html.push(r.hash);
					html.push('">');
					html.push(r.hash.substring(0, 8));
					html.push('</a></td><td> - </td><td>');
					html.push(r.author);
					html.push('</td><td> - </td><td>');
					html.push(r.msg);
					html.push('</td></tr>');
				});
				html.push('</table>');
				$('#GitOriginLogViewText').html(html.join(''));
			}
		}
	});
}

function PlayFileInBrowser (dir, file) {
	window.open('api/file/' + dir + '/' + encodeURIComponent(file) + '?play=1');
}

function CopyFile (dir, file) {
	var newFile = prompt('New Filename:', file);
	if (newFile != null) {
		var url = 'api/file/' + dir + '/copy/' + file + '/' + newFile;

		$.post(url, '')
			.done(function (data) {
				if (data.status == 'success') GetFiles(dir);
				else DialogError('File Copy Failed', 'Error: File Copy failed.');
			})
			.fail(function () {
				DialogError('File Copy Failed', 'Error: File Copy failed.');
			});
	}
}

function RenameFile (dir, file) {
	var newFile = prompt('New Filename:', file);
	if (newFile != null) {
		var url = 'api/file/' + dir + '/rename/' + file + '/' + newFile;

		$.post(url, '')
			.done(function (data) {
				if (data.status == 'success') GetFiles(dir);
				else DialogError('File Rename Failed', 'Error: File Rename failed.');
			})
			.fail(function () {
				DialogError('File Rename Failed', 'Error: File Rename failed.');
			});
	}
}

function DownloadFile (dir, file) {
	location.href =
		'api/file/' + dir + '/' + encodeURIComponent(file).replaceAll('%2F', '/');
}

function DownloadFiles (dir, files) {
	if (files.length == 1) {
		DownloadFile(dir, files[0]);
	} else {
		for (var i = 0; i < files.length; i++) {
			window.open(
				'api/file/' +
					dir +
					'/' +
					encodeURIComponent(files[i]).replaceAll('%2F', '/')
			);
		}
	}
}

/**
 * Downloads a directory as a zip. Pass the triggering button to keep it in a
 * busy state until the file actually arrives.
 *
 * The server builds the whole archive before sending a single byte, and for the
 * Logs zip (the support bundle) that is ~11s of shell commands -- troubleshooting
 * Text.php accounts for almost all of it. A location.href navigation gives the
 * browser nothing to show for that wait and no event to tell us it ended, so the
 * click looks like it did nothing and a second click starts the work over again.
 * Fetching the zip instead lets us hold the busy state until the bytes are here,
 * then hand the blob to the browser as a normal download.
 *
 * Buffering the archive in memory is fine for what both callers ask for (Logs,
 * ~1MB). Without a button it falls back to the original streaming navigation,
 * which is also what a much larger directory would want.
 */
function DownloadZip (dir, btn, busyText) {
	if (!btn) {
		location.href = 'api/files/zip/' + dir;
		return;
	}
	// The file manager's Zip control is an <input type="button">, whose label is
	// its value attribute -- it has no innerHTML to set (and so no room for a
	// spinner element); the support bundle is a <button>.
	var isInput = btn.tagName == 'INPUT';
	var original = isInput ? btn.value : btn.innerHTML;
	btn.disabled = true;
	btn.setAttribute('aria-busy', 'true');
	if (isInput) {
		btn.value = busyText;
	} else {
		btn.innerHTML =
			"<span class='spinner-border spinner-border-sm me-1' role='status' aria-hidden='true'></span>" +
			busyText;
	}

	var restore = function () {
		btn.disabled = false;
		btn.removeAttribute('aria-busy');
		if (isInput) {
			btn.value = original;
		} else {
			btn.innerHTML = original;
		}
	};

	// no-store: the archive is generated fresh per request (troubleshooting
	// output, health check, current logs), so a cached copy is a stale bundle
	// reporting a state the box is no longer in. It also has to be uncached for
	// the busy state to mean anything -- a cache hit returns instantly and the
	// user never sees the ~11s of work they are actually waiting on.
	fetch('api/files/zip/' + dir, { cache: 'no-store' })
		.then(function (response) {
			if (!response.ok) {
				throw new Error('HTTP ' + response.status);
			}
			// Prefer the name the server chose (it carries the host name and
			// timestamp); fall back only if the header is somehow absent.
			var name = 'FPP_' + dir + '.zip';
			var cd = response.headers.get('Content-Disposition');
			if (cd) {
				var m = /filename="?([^";]+)"?/.exec(cd);
				if (m) {
					name = m[1];
				}
			}
			return response.blob().then(function (blob) {
				return { blob: blob, name: name };
			});
		})
		.then(function (file) {
			var url = URL.createObjectURL(file.blob);
			var a = document.createElement('a');
			a.href = url;
			a.download = file.name;
			document.body.appendChild(a);
			a.click();
			document.body.removeChild(a);
			URL.revokeObjectURL(url);
			restore();
		})
		.catch(function (err) {
			restore();
			$.jGrowl('Could not download ' + dir + ' zip: ' + err.message, {
				themeState: 'danger'
			});
		});
}

function ViewImage (file) {
	var url =
		'api/file/Images/' + encodeURIComponent(file).replaceAll('%2F', '/');
	ViewFileImpl(
		url,
		file,
		"<center><a href='" +
			url +
			"' target='_blank'><img src='" +
			url +
			"' style='display: block; max-width: 700px; max-height: 500px; width: auto; height: auto;'></a><br>Click image to display full size.</center>"
	);
}

function ViewFile (dir, file) {
	var url =
		'api/file/' + dir + '/' + encodeURIComponent(file).replaceAll('%2F', '/');
	// Logs read newest-last, so open at the end -- the top of a rotated log can
	// be weeks old. Scripts and Config (the other tabs with a View button) are
	// read from the top, so they keep the default.
	ViewFileImpl(url, file, '', { scrollToBottom: dir == 'Logs' });
}
function TailFile (dir, file, lines) {
	var url =
		'api/file/' +
		dir +
		'/' +
		encodeURIComponent(file).replaceAll('%2F', '/') +
		'?tail=' +
		lines;
	// console.log(url);
	ViewFileImpl(url, file, '', {
		title: 'Tail (last ' + lines + ' lines)',
		scrollToBottom: true
	});
}

var tailFollowEventSource = null;
var TAIL_FOLLOW_MAX_LINES = 1000;

function TailFollowFile (dir, file, lines = 50) {
	var url =
		'api/file/' +
		dir +
		'/tailfollow/' +
		encodeURIComponent(file).replaceAll('%2F', '/') +
		'?lines=' +
		lines;

	var options = {
		id: 'tailFollowDialog',
		title: 'Tail Follow: ' + file,
		// No max-height/overflow here: the scrollable modal body is the single
		// scroller. Giving the pre its own 65vh box made it a second one, and
		// pushed the dialog past short viewports so the modal root scrolled too.
		body: "<pre id='tailFollowText' class='fileText' style='margin: 0; padding: 10px; background: #000; color: #0f0; overflow-anchor: none; font-family: monospace; white-space: pre-wrap; word-wrap: break-word;'></pre>",
		class: 'modal-xl modal-dialog-scrollable',
		keyboard: false,
		backdrop: 'static',
		buttons: {
			Stop: {
				id: 'tailFollowStopButton',
				click: function () {
					if (tailFollowEventSource) {
						tailFollowEventSource.close();
						tailFollowEventSource = null;
						$('#tailFollowStopButton')
							.text('Start')
							.removeClass('btn-danger')
							.addClass('btn-success');
						var pre = document.getElementById('tailFollowText');
						if (pre) {
							pre.textContent += '\n--- Streaming stopped ---\n';
						}
					} else {
						// Restart
						$('#tailFollowStopButton')
							.text('Stop')
							.removeClass('btn-success')
							.addClass('btn-danger');
						var pre = document.getElementById('tailFollowText');
						if (pre) {
							pre.textContent += '\n--- Restarting stream ---\n';
						}
						startTailFollowStream(url);
					}
				},
				class: 'btn-danger'
			},
			Close: {
				id: 'tailFollowCloseButton',
				click: function () {
					if (tailFollowEventSource) {
						tailFollowEventSource.close();
						tailFollowEventSource = null;
					}
					CloseModalDialog('tailFollowDialog');
				},
				class: 'btn-secondary'
			}
		}
	};

	DoModalDialog(options);

	// Reset button state
	$('#tailFollowStopButton')
		.text('Stop')
		.removeClass('btn-success')
		.addClass('btn-danger')
		.prop('disabled', false);

	// Clean up on modal close
	$('#tailFollowDialog')
		.off('hidden.bs.modal.tailfollow')
		.on('hidden.bs.modal.tailfollow', function () {
			if (tailFollowEventSource) {
				tailFollowEventSource.close();
				tailFollowEventSource = null;
			}
		});
	$('#tailFollowDialog').one('shown.bs.modal', function () {
		startTailFollowStream(url);
	});
}

function startTailFollowStream (url) {
	if (tailFollowEventSource) {
		tailFollowEventSource.close();
	}

	tailFollowEventSource = new EventSource(url);
	var outputArea = document.getElementById('tailFollowText');
	var lineCount = 0;

	tailFollowEventSource.onmessage = function (event) {
		if (outputArea && event.data) {
			outputArea.textContent += event.data + '\n';
			lineCount++;

			// Trim if exceeds max lines (keep most recent)
			if (lineCount > TAIL_FOLLOW_MAX_LINES) {
				var lines = outputArea.textContent.split('\n');
				var trimmed = lines.slice(-TAIL_FOLLOW_MAX_LINES);
				outputArea.textContent =
					'... (older content trimmed) ...\n' + trimmed.join('\n');
				lineCount = TAIL_FOLLOW_MAX_LINES;
			}

			// Auto-scroll to bottom (deferred for cross-platform reliability).
			// The scrollable modal body is the scroller, not the pre.
			requestAnimationFrame(function () {
				var scroller = outputArea.closest('.modal-body') || outputArea;
				scroller.scrollTop = scroller.scrollHeight;
			});
		}
	};

	tailFollowEventSource.onerror = function (error) {
		if (outputArea) {
			outputArea.textContent += '\n--- Connection error or stream ended ---\n';
		}
		if (tailFollowEventSource) {
			tailFollowEventSource.close();
			tailFollowEventSource = null;
		}
		$('#tailFollowStopButton')
			.text('Start')
			.removeClass('btn-danger')
			.addClass('btn-success');
	};
}

// opts.title overrides the dialog heading; opts.scrollToBottom starts the view
// at the newest content, which is what a tail wants (its last line is the most
// recent one) while a whole-file view still starts at the top.
function ViewFileImpl (url, file, html = '', opts = {}) {
	var options = {
		id: 'fileViewerDialog',
		title: (opts.title || 'File Viewer') + ': ' + file,
		body: "<div id='fileViewerText' class='fileText'>Loading...</div>",
		class: 'modal-dialog-scrollable',
		keyboard: true,
		backdrop: true,
		buttons: {
			Close: {
				id: 'fileViewerCloseButton',
				click: function () {
					CloseModalDialog('fileViewerDialog');
				},
				class: 'btn-success'
			}
		}
	};
	DoModalDialog(options);
	// The modal body is the scroller, not the pre inside it. Scrolling only
	// sticks once the dialog is visible AND the text is laid out -- until then
	// scrollHeight equals clientHeight and scrollTop silently clamps to 0. The
	// fetch can finish either side of the show animation, so run it on both and
	// let whichever lands last win.
	var scrollToNewest = function () {
		if (!opts.scrollToBottom) return;
		var body = document.querySelector('#fileViewerDialog .modal-body');
		if (body) body.scrollTop = body.scrollHeight;
	};
	if (opts.scrollToBottom) {
		$('#fileViewerDialog').off('shown.bs.modal.viewer').on('shown.bs.modal.viewer', scrollToNewest);
	}
	if (html == '') {
		$.get(url, function (text) {
			var ext = file.split('.').pop();
			if (ext != 'html') {
				$('#fileViewerText').html(
					'<pre>' + text.replace(/</g, '&lt;').replace(/>/g, '&gt;') + '</pre>'
				);
				// Double rAF: the first yields to style, the second runs after
				// layout, when scrollHeight reflects the inserted text.
				requestAnimationFrame(function () {
					requestAnimationFrame(scrollToNewest);
				});
			}
		});
	} else {
		$('#fileViewerText').html(html);
	}
}

function DeleteFile (dir, row, file, silent = false) {
	$.ajax({
		url:
			'api/file/' + dir + '/' + encodeURIComponent(file).replaceAll('%2F', '/'),
		type: 'DELETE'
	})
		.done(function (data) {
			if (data.status == 'OK') {
				$(row).remove();
				UpdateFileCount(dir);
			} else {
				if (!silent)
					DialogError(
						'ERROR',
						'Error deleting file "' + file + '": ' + data.status
					);
			}
		})
		.fail(function () {
			if (!silent) DialogError('ERROR', 'Error deleting file: ' + file);
		});
}

function SetupSelectableTableRow (info) {
	$('#' + info.tableName + ' > tbody').on(
		'mousedown',
		'tr',
		function (event, ui) {
			var enabledButtonState;
			var disabledButtonState;

			if ($(this).hasClass('fppTableSelectedEntry')) {
				$(this).removeClass('fppTableSelectedEntry');

				info.selected = -1;
				enabledButtonState = 'disable';
				disabledButtonState = 'enable';
			} else {
				$('#' + info.tableName + ' > tbody > tr').removeClass(
					'fppTableSelectedEntry'
				);
				$(this).addClass('fppTableSelectedEntry');

				var items = $('#' + info.tableName + ' > tbody > tr');
				info.selected = items.index(this);
				enabledButtonState = 'enable';
				disabledButtonState = 'disable';
			}

			for (var i = 0; i < info.enableButtons.length; i++) {
				SetButtonState('#' + info.enableButtons[i], enabledButtonState);
			}
			for (var i = 0; i < info.disableButtons.length; i++) {
				SetButtonState('#' + info.disableButtons[i], disabledButtonState);
			}
		}
	);

	if (info.hasOwnProperty('sortable') && info.sortable) {
		sortableOptions = {
			update: function (event, ui) {
				if (
					info.hasOwnProperty('sortableCallback') &&
					info.sortableCallback != ''
				) {
					window[info.sortableCallback]();
				}
			},
			scroll: true
		};
		if (hasTouch) {
			$.extend(sortableOptions, { handle: '.rowGrip' });
		}
		$('#' + info.tableName + ' > tbody')
			.sortable(sortableOptions)
			.disableSelection();
	}
}

function DialogOK (title, message) {
	DoModalDialog({
		id: 'dialogOKPopup',
		title: title,
		body: message,
		class: 'modal-sm',
		keyboard: true,
		backdrop: true,
		buttons: {
			Close: {
				click: function () {
					CloseModalDialog('dialogOKPopup');
				},
				class: 'btn-success'
			}
		},
		open: function () {
			$('#dialogOKPopup').css('z-index', 1060);
			$('.modal-backdrop').last().css('z-index', 1059);
		},
		close: function () {
			$('.modal-backdrop').remove(); // Remove lingering backdrop
			$('body').removeClass('modal-open').css('padding-right', ''); // Reset body state
		}
	});
}

// Simple wrapper for now, but we may highlight this somehow later
function DialogError (title, message) {
	DialogOK(title, message);
}

// page visibility prefixing
function getHiddenProp () {
	var prefixes = ['webkit', 'moz', 'ms', 'o'];

	// if 'hidden' is natively supported just return it
	if ('hidden' in document) return 'hidden';

	// otherwise loop over all the known prefixes until we find one
	for (var i = 0; i < prefixes.length; i++) {
		if (prefixes[i] + 'Hidden' in document) return prefixes[i] + 'Hidden';
	}

	// otherwise it's not supported
	return null;
}

// return page visibility
function isHidden () {
	var prop = getHiddenProp();
	if (!prop) return false;

	return document[prop];
}

function bindVisibilityListener () {
	var visProp = getHiddenProp();
	if (visProp) {
		var evtname = visProp.replace(/[H|h]idden/, '') + 'visibilitychange';
		document.addEventListener(evtname, handleVisibilityChange);
	}
}

function handleVisibilityChange () {
	if (isHidden()) {
		if (statusTimeout != null) {
			clearTimeout(statusTimeout);
			statusTimeout = null;
		}
		return;
	}
	// Visible again.  Apply the most recent snapshot the WebSocket delivered
	// while we were hidden (its DOM work was deliberately skipped) so the page is
	// current before the poll round-trip, then restart the poll loop.
	// LoadSystemStatus() picks its own URL and cadence from fppdWSConnected, so
	// this resumes the cheap ?systemonly=1 augmentation poll when the socket is
	// up and the full status poll when it is not.
	if (fppdWSPendingStatus) {
		var pending = fppdWSPendingStatus;
		fppdWSPendingStatus = null;
		applyWsStatus(pending);
	}
	LoadSystemStatus();
	// GetFPPStatus();
}

// syntaxHighlight() from
// https://stackoverflow.com/questions/4810841/pretty-print-json-using-javascript
function syntaxHighlight (json) {
	json = json
		.replace(/&/g, '&amp;')
		.replace(/</g, '&lt;')
		.replace(/>/g, '&gt;');
	return json.replace(
		/("(\\\\u[a-zA-Z0-9]{4}|\\\\[^u]|[^\\\\"])*"(\s*:)?|\b(true|false|null)\b|-?\d+(?:\.\d*)?(?:[eE][+\-]?\d+)?)/g,
		function (match) {
			var cls = 'jsNumber';
			if (/^"/.test(match)) {
				if (/:$/.test(match)) {
					cls = 'jsKey';
				} else {
					cls = 'jsString';
				}
			} else if (/true|false/.test(match)) {
				cls = 'jsBoolean';
			} else if (/null/.test(match)) {
				cls = 'jsNull';
			}
			return '<span class="' + cls + '">' + match + '</span>';
		}
	);
}

function CommandToJSON (commandSelect, tblCommand, json, addArgTypes = false) {
	var args = new Array();
	var argTypes = new Array();
	var commandVal = $('#' + commandSelect).val();
	json['command'] = commandVal;
	if (commandVal != '' && !(typeof commandVal == 'undefined')) {
		json['multisyncCommand'] = $('#' + tblCommand + '_multisync').is(
			':checked'
		);
		json['multisyncHosts'] = ($('#' + tblCommand + '_multisyncHosts').val() || '')
			.split(',')
			.map(function (h) {
				return h.trim();
			})
			.filter(function (h) {
				return h != '';
			})
			.join(',');
		for (var x = 1; x < 20; x++) {
			var inp = $('#' + tblCommand + '_arg_' + x);
			var val = inp.val();
			if (inp.attr('type') == 'checkbox') {
				if (inp.is(':checked')) {
					args.push('true');
				} else {
					args.push('false');
				}
				if (addArgTypes) {
					argTypes.push(inp.data('arg-type'));
				}
			} else if (inp.attr('type') == 'number' || inp.attr('type') == 'text') {
				args.push(val);
				var adj = $('#' + tblCommand + '_arg_' + x + '_adjustable');
				if (adj.attr('type') == 'checkbox') {
					if (adj.is(':checked')) {
						if (typeof json['adjustable'] == 'undefined') {
							json['adjustable'] = {};
						}
						json['adjustable'][x] = inp.attr('type');
					} else {
						if (typeof json['adjustable'] != 'undefined') {
							delete json['adjustable'][x];
							if (jQuery.isEmptyObject(json['adjustable'])) {
								delete json['adjustable'];
							}
						}
					}
				}
				if (addArgTypes) {
					argTypes.push(inp.data('arg-type'));
				}
			} else if (Array.isArray(val)) {
				args.push(val.toString());
				if (addArgTypes) {
					argTypes.push(inp.data('arg-type'));
				}
			} else if (typeof val != 'undefined') {
				args.push(val);
				if (addArgTypes) {
					argTypes.push(inp.data('arg-type'));
				}
			}
		}
	}
	json['args'] = args;
	if (addArgTypes) {
		json['argTypes'] = argTypes;
	}
	return json;
}

function LoadCommandArg () {
	LoadCommandList($('.arg_command'));
}

var commandList = '';
var commandListByName = {};
var extraCommands = '';
function PopulateCommandListCache () {
	if (typeof commandList != 'string') return;

	$.ajax({
		dataType: 'json',
		url: 'api/commands',
		async: false,
		success: function (data) {
			commandList = data;
			if (extraCommands != '') {
				$.each(extraCommands, function (key, val) {
					commandList.push(val);
				});
			}

			$.each(commandList, function (key, val) {
				commandListByName[val['name']] = val;
			});
		}
	});
}

// Eagerly capture the command list fingerprint once on page load so we can
// detect changes after an fppd restart.
function captureCommandListJSON () {
	$.ajax({
		dataType: 'json',
		url: 'api/commands',
		async: true,
		success: function (data) {
			_savedCommandListJSON = JSON.stringify(data);
		}
	});
}

// Called after an fppd restart transition.  Re-fetches the command list and
// hard-reloads the page if it differs from the pre-restart snapshot.
function checkCommandListChanged () {
	if (!_savedCommandListJSON) return;
	$.ajax({
		dataType: 'json',
		url: 'api/commands',
		async: true,
		success: function (data) {
			var hash = JSON.stringify(data);
			if (hash !== _savedCommandListJSON) {
				location.reload();
			}
		}
	});
}

function LoadCommandList (commandSelect, currentValue) {
	if (typeof commandSelect === 'string') {
		commandSelect = $('#' + commandSelect);
	}
	if (commandList == '') {
		PopulateCommandListCache();
	}

	var uiLevel = (typeof settings !== 'undefined' && settings['uiLevel']) ? parseInt(settings['uiLevel']) : 0;
	var groups = {};
	$.each(commandList, function (key, val) {
		var level = val['level'] || 0;
		// Always show the currently-selected command even if its level is above
		// the viewer's UI level, so switching to a lower UI level never hides or
		// corrupts an already-saved selection - it just stops offering it as a
		// choice for anything new. Mirrors common.php's optionsLevel handling.
		if (level > uiLevel && val['name'] !== currentValue) {
			return;
		}
		var cat = val['category'] || 'Other';
		if (!groups[cat]) {
			groups[cat] = [];
		}
		groups[cat].push(val['name']);
	});

	var cats = Object.keys(groups).sort();

	$.each(cats, function (i, cat) {
		var og = $("<optgroup label='" + cat + "'></optgroup>");
		$.each(groups[cat], function (i, name) {
			og.append("<option value='" + name + "'>" + name + '</option>');
		});
		commandSelect.append(og);
	});
}

function UpdateChildVisibility () {
	if (typeof playlistEntryTypes === 'undefined') {
		return;
	}
	var pet = playlistEntryTypes[$('#pe_type').val()];
	var keys = Object.keys(pet.args);
	var shown = [];
	var hidden = [];
	for (var i = 0; i < keys.length; i++) {
		var a = pet.args[keys[i]];
		if (typeof a['children'] === 'object') {
			var val = $('.arg_' + a.name).val();
			var ckeys = Object.keys(a.children);
			for (var c = 0; c < ckeys.length; c++) {
				for (var x = 0; x < a.children[ckeys[c]].length; x++) {
					if (val == ckeys[c]) {
						if (!hidden.includes(a.name)) {
							$('.arg_row_' + a.children[ckeys[c]][x]).show();
							shown.push(a.children[ckeys[c]][x]);
						} else {
							$('.arg_row_' + a.children[ckeys[c]][x]).hide();
							hidden.push(a.children[ckeys[c]][x]);
						}
					} else {
						if (!shown.includes(a.children[ckeys[c]][x])) {
							$('.arg_row_' + a.children[ckeys[c]][x]).hide();
							hidden.push(a.children[ckeys[c]][x]);
						}
					}
				}
			}
		} else if (!hidden.includes(a.name)) {
			$('.arg_row_' + a.name).show();
		}
	}

	// Try activte the colpick colour picker
	fppCommandColorPicker();
}

// Same show/hide-by-parent-value behavior as UpdateChildVisibility(), but for
// generic Command-args tables (commandEditor.php, GPIO's command picker,
// etc.) that render straight from a Command's getDescription() JSON instead
// of the global playlistEntryTypes structure - so the children map travels on
// the element itself (data-children) rather than being looked up globally.
function UpdateGenericArgChildVisibility (el) {
	var $el = $(el);
	var childrenMap;
	try {
		childrenMap = JSON.parse($el.attr('data-children'));
	} catch (e) {
		return;
	}
	var val = $el.val();
	var shownNames = childrenMap.hasOwnProperty(val) ? childrenMap[val] : [];
	var allNames = [];
	$.each(childrenMap, function (k, names) {
		allNames = allNames.concat(names);
	});
	$.each(allNames, function (i, name) {
		if (shownNames.includes(name)) {
			// Rows can start hidden via the server-rendered inline
			// "visibility:collapse" (see initiallyHiddenChildren above) -
			// jQuery's show() only ever touches `display`, never
			// `visibility`, so a row hidden that way would otherwise never
			// reappear. Clear it explicitly before show().
			$('.arg_row_' + name).css('visibility', '').show();
		} else {
			$('.arg_row_' + name).hide();
		}
	});
}

// ---------------------------------------------------------------------
// "commandlist" arg type - a repeatable list of {command, args} rows,
// backing the If command's Then/Else Run fields. Ported from gpio.php's
// GPIO-edge command list (renderModalList/cmdSummary/editModalCmd/
// moveModalCmd/removeModalCmd, gpio.php ~line 531-780) rather than
// invented fresh - same proven pattern: an in-memory array per list,
// one-line summaries, Edit opens the existing ShowCommandEditor modal
// (so a row's own args get the full generic command-args UI for free,
// including any subcommand/commandlist/expression fields it has),
// Up/Down swap array entries, Delete splices. See PrintArgInputs'
// val['type'] == 'commandlist' branch for the markup this drives.
// ---------------------------------------------------------------------

var commandListState = {}; // id -> array of {command, args}

function CommandListSummary (cmd) {
	if (!cmd || !cmd.command) {
		return '(none)';
	}
	var s = cmd.command;
	if (cmd.args && cmd.args.length) {
		s +=
			': ' +
			cmd.args
				.map(function (a) {
					return a !== '' ? a : '—';
				})
				.join(', ');
	}
	return s;
}

// Parses each not-yet-initialized commandListRoot hidden field's saved JSON
// value into commandListState and renders it - called once per render via
// initFuncs, same as PopulateExpressionVarHelpers.
function InitCommandLists () {
	$('.commandListRoot')
		.not('[data-initialized]')
		.each(function () {
			$(this).attr('data-initialized', '1');
			LoadCommandListFromHiddenField(this.id);
		});
}

function LoadCommandListFromHiddenField (id) {
	var raw = $('#' + id).val();
	try {
		commandListState[id] = JSON.parse(raw || '[]');
	} catch (e) {
		commandListState[id] = [];
	}
	RenderCommandListRows(id);
}

// PopulateExistingCommand() (used when reopening a command editor prefilled
// with previously-saved values, e.g. editing an existing "If" or reopening
// the outer editor after a nested commandlist pick) sets this hidden
// field's real value *after* the form has already rendered and
// InitCommandLists has already run once against the schema default -
// via plain inp.val(v).change() (fpp.js PopulateExistingCommand). Nothing
// was listening for that change, so the real saved value never made it
// into commandListState; catch it here instead of only at initial render.
$(document).on('change', '.commandListRoot', function () {
	LoadCommandListFromHiddenField(this.id);
});

function RenderCommandListRows (id) {
	var list = commandListState[id] || [];
	var $c = $('#' + id + '_container').empty();
	if (!list.length) {
		$c.append("<div class='text-muted small mb-2'>No commands — click Add Command to configure.</div>");
	}
	$.each(list, function (i, cmd) {
		var row = $("<div class='cmdListRow d-flex align-items-center gap-1 mb-1'></div>");
		var label = $("<span class='cmdListLabel flex-fill'></span>").text(CommandListSummary(cmd));
		var edit = $(
			"<button type='button' class='buttons btn-sm' title='Edit'><i class='fas fa-pencil-alt'></i></button>"
		).on('click', function () {
			EditCommandListRow(id, i);
		});
		var up = $(
			"<button type='button' class='buttons btn-sm' title='Move up'" +
				(i === 0 ? ' disabled' : '') +
				"><i class='fas fa-arrow-up'></i></button>"
		).on('click', function () {
			MoveCommandListRow(id, i, -1);
		});
		var dn = $(
			"<button type='button' class='buttons btn-sm' title='Move down'" +
				(i === list.length - 1 ? ' disabled' : '') +
				"><i class='fas fa-arrow-down'></i></button>"
		).on('click', function () {
			MoveCommandListRow(id, i, 1);
		});
		var del = $(
			"<button type='button' class='buttons btn-sm' title='Remove'><i class='fas fa-trash'></i></button>"
		).on('click', function () {
			RemoveCommandListRow(id, i);
		});
		row.append(label, edit, up, dn, del);
		$c.append(row);
	});
	$('#' + id).val(JSON.stringify(list));

	// A toggleStyle arg (e.g. the If command's Sequential/Parallel "Order:")
	// can register itself as pairing with this commandlist via
	// data-pairs-with (see PrintArgInputs) - only meaningful once there are
	// 2+ commands to actually order, same rationale as the condition
	// editor's own Match ALL/ANY toggle only showing for 2+ conditions.
	$("tr[data-pairs-with='" + id + "']").toggle(list.length > 1);
}

// ShowCommandEditor's dialog (#commandEditorPopup/#commandEditorDiv) is a
// singleton, not stackable - opening it again while an outer command editor
// (e.g. the If command's own "FPP Command Editor" dialog) is already open
// doesn't layer a second dialog on top, it *replaces* the outer one's DOM
// content in place, silently discarding whatever the user had entered in
// the outer form. commandEditor.php's own ids (#editorCommand etc.) aren't
// parameterized for multiple simultaneous instances, so true nesting isn't
// available without changing that shared file - not worth the risk to a
// file GPIO's own editor also depends on.
//
// Instead: capture the outer editor's current in-progress state (via the
// same CommandToJSON() the outer Save button itself uses, plus the global
// commandEditorTarget/Callback/CancelCallback commandEditor.php's own
// script already tracks) right before opening the nested picker, then
// restore it - by literally reopening the same outer editor with that
// captured state as its data - once the nested pick is saved OR cancelled.
var commandListPendingOuterReopen = null;

function CaptureOuterEditorAndOpenNested (nestedTarget, nestedData, title, saveButton) {
	commandListPendingOuterReopen = {
		target: commandEditorTarget,
		data: CommandToJSON('editorCommand', 'tblCommandEditor', {}),
		callback: commandEditorCallback,
		cancelCallback: commandEditorCancelCallback
	};
	ShowCommandEditor(nestedTarget, nestedData, 'CommandListRowSavedThenReopen', 'CommandListRowCancelledThenReopen', {
		title: title,
		saveButton: saveButton,
		cancelButton: 'Cancel',
		showPresetSelect: true,
		presetInsertsReference: true
	});
}

function ReopenOuterCommandEditorAfterCommandListPick () {
	var outer = commandListPendingOuterReopen;
	commandListPendingOuterReopen = null;
	if (!outer) {
		return;
	}
	function doReopen () {
		// Re-render the outer form exactly as it was, including whatever
		// commandlist state was already accumulated in commandListState -
		// InitCommandLists reads the (unchanged) hidden field value again,
		// so rows added/edited before this nested pick are still there.
		ShowCommandEditor(outer.target, outer.data, outer.callback, outer.cancelCallback, {
			title: 'FPP Command Editor',
			saveButton: 'Accept Changes',
			cancelButton: 'Cancel Edit',
			showPresetSelect: false
		});
	}
	// fppDialog (fpp.js ~line 705) is a Bootstrap-modal wrapper, not jQuery
	// UI - "close" maps to .modal('hide'), an async CSS-transition teardown.
	// The nested picker's own Save/Cancel handler (commandEditor.php) calls
	// that hide immediately before invoking our callback, so the hide is
	// still in flight here. Reopening synchronously (or after a guessed
	// timeout) races that teardown: the new content loads correctly but the
	// modal chrome itself doesn't reliably become visible. Wait for
	// Bootstrap's own "fully hidden" event instead of guessing a duration.
	$('#commandEditorPopup').one('hidden.bs.modal', doReopen);
}

function AddCommandListRow (id) {
	CaptureOuterEditorAndOpenNested(id + '||-1', {}, 'Add Command', 'Add Command');
}

function EditCommandListRow (id, idx) {
	CaptureOuterEditorAndOpenNested(id + '||' + idx, (commandListState[id] || [])[idx] || {}, 'Edit Command', 'Save Command');
}

// ShowCommandEditor's "target" param is free-form and comes back verbatim
// to the save callback, so it carries both which commandlist and which row
// index this edit was for - no extra global state needed for a widget that
// may have several independent instances on one page (e.g. If's
// thenCommands and elseCommands at once).
function CommandListRowSavedThenReopen (target, data) {
	if (data && data.command) {
		var parts = target.split('||');
		var id = parts[0];
		var idx = parseInt(parts[1], 10);
		if (!commandListState[id]) {
			commandListState[id] = [];
		}
		if (idx === -1) {
			commandListState[id].push(data);
		} else {
			commandListState[id][idx] = data;
		}
		// commandListPendingOuterReopen.data was captured via CommandToJSON
		// *before* this nested pick happened, so it still holds the old
		// pre-edit commandlist JSON in this arg's slot. Patch that slot with
		// the freshly updated array now, or the reopened outer form (which
		// rebuilds entirely from this captured data) would show the stale
		// list instead of what was just added/edited. ID encodes its own
		// position as "..._arg_N" (see PrintArgInputs), which is exactly the
		// index CommandToJSON's positional args array uses.
		if (commandListPendingOuterReopen) {
			var m = id.match(/_arg_(\d+)$/);
			if (m) {
				var argIndex = parseInt(m[1], 10) - 1;
				commandListPendingOuterReopen.data.args = commandListPendingOuterReopen.data.args || [];
				commandListPendingOuterReopen.data.args[argIndex] = JSON.stringify(commandListState[id]);
			}
		}
	}
	ReopenOuterCommandEditorAfterCommandListPick();
}

function CommandListRowCancelledThenReopen () {
	ReopenOuterCommandEditorAfterCommandListPick();
}

function RemoveCommandListRow (id, idx) {
	(commandListState[id] || []).splice(idx, 1);
	RenderCommandListRows(id);
}

function MoveCommandListRow (id, idx, dir) {
	var list = commandListState[id] || [];
	var t2 = idx + dir;
	if (t2 < 0 || t2 >= list.length) {
		return;
	}
	var tmp = list[idx];
	list[idx] = list[t2];
	list[t2] = tmp;
	RenderCommandListRows(id);
}

// ---------------------------------------------------------------------
// "conditionlist" arg type - the If command's Check field, and its only
// way to specify a condition. A tree of conditions and groups, each
// sharing one AND/OR: a single plain condition renders inline (no modal -
// matches the old single-condition case exactly); once there's more than
// one item at a level, or a Group is added, every item becomes a one-line
// summary with Edit/Up/Down/Delete, and Edit opens a small dedicated
// modal - the same "summary + modal" shape already proven for Then/Else
// Run (~8440), just via a lightweight modal of our own (fppDialog
// directly) instead of the Command-specific ShowCommandEditor. A Group's
// own modal renders this same tree logic recursively, so nesting - e.g.
// (A AND B) OR C - falls out of the architecture instead of needing
// bespoke recursive inline rendering.
//
// Must stay in sync with Condition.cpp's LeafNode::compare()/readSource()
// comparator and source vocabulary - there's no shared source of truth
// for these on the frontend.
// ---------------------------------------------------------------------

var conditionListState = {};
// "Variable" was removed entirely (not kept as a choice) - merged into
// "Expression", which now auto-detects: an exact match to a real variable
// name is a plain lookup (old "Variable" behavior), anything that looks like
// a formula is computed, and anything else is literal text - see
// Condition.cpp's EvaluateNameOrExpression for the exact rules. Any existing
// saved condition using "source":"Variable" now reads as not-found.
// Expression pinned first (the general-purpose default/fallback - anything
// not covered by its own dedicated Source is still reachable through it),
// everything else alphabetical - now that the list has grown past a small
// handful, alphabetical is easier to scan than an ad-hoc grouping.
var CONDITION_SOURCES = [
	'Expression', 'Current Playlist', 'Current Song', 'Day', 'GPIO Pin',
	'Is Playing', 'Month', 'Player Status', 'Sun', 'Time'
];
// Sources with exactly one live current value to read, same as Time - no
// "which thing to read" choice (unlike Sun's Sunrise/Sunset, or GPIO Pin's
// pin name), so there's nothing meaningful to put in a Name field at all.
// Matches Condition.cpp's ReadConditionSourceValue() one-for-one: every
// entry here just returns Variables::INSTANCE.getVariable("fpp_...")
// directly, ignoring `name` entirely.
var CONDITION_SOURCES_NO_NAME_FIELD = [
	'Time', 'Day', 'Month', 'Player Status', 'Is Playing',
	'Current Playlist', 'Current Song'
];
// Name-field label per source, matching Condition.cpp's readSource() use of
// `name` - Time (and the rest of CONDITION_SOURCES_NO_NAME_FIELD) ignores
// `name` entirely, so those get no Name field at all rather than a
// placeholder that would just be confusing.
var CONDITION_NAME_LABELS = {
	'GPIO Pin': 'GPIO Pin Name',
	Sun: 'Sunrise or Sunset'
};
// Small fixed-choice Value suggestions (via <datalist>, same mechanism as
// the Name field's own suggested-values list) for the new no-Name sources
// whose Value really only has a handful of valid answers - Day/Month/Player
// Status/Is Playing/GPIO Pin. Current Playlist/Current Song have no such
// fixed set (playlist/media names are arbitrary), so they get no entry.
// An entry is either a plain string (shown as-is, and is the Value it sets)
// or a {value, label} object for when the underlying Value needs to stay a
// literal ("0"/"1") but the label should read as something more meaningful -
// GPIO Pin deliberately says "Low"/"High" rather than "Not Triggered"/
// "Triggered": whether 1 means triggered depends on that specific pin's own
// wiring/pull config (pull-up vs pull-down, NO vs NC), which FPP has no way
// to know, so a "Triggered" label could be flat-out backwards for a given
// pin. Is Playing has no such ambiguity - 1 always means "yes, playing" -
// so it gets the more readable label without caveat.
var CONDITION_VALUE_SUGGESTIONS = {
	Day: ['Sunday', 'Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday'],
	Month: ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December'],
	'Player Status': ['idle', 'playing', 'stopping gracefully', 'stopping gracefully after loop', 'stopping now', 'paused'],
	'Is Playing': [{ value: '0', label: '0 - Not Playing' }, { value: '1', label: '1 - Playing' }],
	'GPIO Pin': [{ value: '0', label: '0 - Low' }, { value: '1', label: '1 - High' }]
};

// Always-visible explanation of the selected Source, shown at the bottom of
// the Edit Condition dialog (see RenderConditionFields) - what "Name" means
// varies a lot per Source (a lookup key vs. a tinyexpr expression vs. an
// ignored field), so a quick example beats making the user guess or read
// Condition.cpp themselves. Keep in sync with ReadConditionSourceValue()
// there (~line 125), which is the actual source of truth for this behavior.
var CONDITION_SOURCE_HELP = {
	// Merged with the old, separate "Variable" Source (removed entirely) -
	// also covers what a dedicated "MQTT" source used to do (a cached
	// mqtt-<topic> message is just a read-only Variable, per
	// Variables::getVariable()'s own IsMqttVariableName() branch). Auto-
	// detects intent (Condition.cpp's EvaluateNameOrExpression): type just a
	// real variable's exact name for a plain lookup, or a formula to compute
	// one - no need to type "=" or wrap anything in "%%...%%" by hand.
	// HTML, not plain text (see the html-vs-EscapeHtml branch in
	// RenderConditionFields below) - broken into bullets since this is the
	// one help entry dense enough (3 examples + a tip) that a single
	// run-on sentence was hard to scan.
	Expression: 'Type a Variable\'s exact name for a plain lookup, or a formula to compute one.' +
		'<ul class="mb-1 ps-3">' +
		'<li>Plain lookup: Name = "myCounter", Is = equal to, Value = "5"</li>' +
		'<li>Formula: Name = "myCounter * 2", Is = greater than, Value = "10"</li>' +
		'<li>MQTT variable: Name = "mqtt-homeassistant/sensor/outside_temperature/state", Is = equal to, Value = "20"</li>' +
		'</ul>' +
		'Tip: every dedicated Source below (Time, Day, Player Status, etc.) is really just a shortcut for one of the read-only fpp_ variables on the Variables page - anything not listed as its own Source (fpp_volume, fpp_warning_count, and more) is still reachable this way, by name.',
	Time: 'Compares the current wall-clock time (HH:MM). Example: Is = greater than, Value = "18:00" (after 6pm).',
	Day: 'Compares today’s day of the week. Example: Is = equal to, Value = "Saturday".',
	Month: 'Compares the current month. Example: Is = equal to, Value = "December".',
	'GPIO Pin': 'Reads a GPIO pin’s current value.' +
		'<ul class="mb-1 ps-3">' +
		'<li>Uses the pin’s last commanded output value, falling back to its live input reading if it hasn’t been commanded</li>' +
		'<li>Suggested names below are only pins currently enabled as GPIO Inputs - a pin can still be typed manually if it was disabled after this condition was set up</li>' +
		'<li>Example: Name = "P1-3", Is = equal to, Value = "1"</li>' +
		'</ul>',
	Sun: 'Compares today’s sunrise or sunset time (HH:MM), based on FPP’s configured location - pick Sunrise or Sunset below. Example: Sunset, Is = greater than, Value = "20:30".',
	'Player Status': 'Compares the player’s current status (as text). Example: Is = equal to, Value = "playing".',
	'Is Playing': 'Compares whether a playlist is currently playing ("1") or not ("0"). Example: Is = equal to, Value = "1".',
	'Current Playlist': 'Compares the name of the currently loaded playlist (empty if none). Example: Is = equal to, Value = "Halloween Show".',
	'Current Song': 'Compares the filename of the currently playing media (empty if none). Example: Is = contains, Value = "Jingle".'
};

// Suggests real, currently-known names for the Name field via a <datalist>,
// so the user isn't guessing exact spelling/case against Condition.cpp's
// exact-match readSource() lookups. Fetched once per source per page load
// and cached - these lists (variable names, setting names, GPIO pin names,
// sensor labels) don't change often enough to justify refetching on every
// row render, and RenderConditionFields() re-renders on every source-dropdown
// change.
var CONDITION_NAME_OPTIONS_CACHE = {};
function FetchConditionNameOptions (source, callback) {
	if (CONDITION_NAME_OPTIONS_CACHE[source]) {
		callback(CONDITION_NAME_OPTIONS_CACHE[source]);
		return;
	}
	function done (names) {
		names.sort();
		CONDITION_NAME_OPTIONS_CACHE[source] = names;
		callback(names);
	}
	if (source === 'Variable') {
		// 'Variable' here is just an internal cache key for "the full list of
		// real variable names" - used by the insert-variable-arrow datalist
		// (RenderSharedExpressionValueHelper/PopulateExpressionVarHelpers),
		// not a Source dropdown choice (that separate "Variable" Source was
		// removed entirely, merged into "Expression" - see CONDITION_SOURCES).
		// Three sources of names, all read through the same
		// Variables::getVariable() call (Condition.cpp): User Variables
		// (api/variables), the read-only fpp_ status variables
		// (api/variables?fpp=true), and the read-only mqtt-<topic> variables
		// (api/variables?mqtt=true). Grouped rather than one flat
		// alphabetical sort (bypasses the shared done() helper below, which
		// would otherwise re-sort everything together) - User Variables are
		// the ones actually worth typing by hand and there are usually only
		// a few of them, while mqtt-<topic> alone can run into the
		// thousands on a busy broker and would otherwise bury them
		// alphabetically. Browsers show <datalist> suggestions in source
		// order when filtering by what's typed so far (verified in Chrome/
		// Firefox), so this grouping survives into what the user sees.
		$.when(
			$.ajax({ url: 'api/variables', dataType: 'json' }),
			$.ajax({ url: 'api/variables?fpp=true', dataType: 'json' }),
			$.ajax({ url: 'api/variables?mqtt=true', dataType: 'json' })
		)
			.done(function (r1, r2, r3) {
				var names = Object.keys(r1[0] || {}).sort()
					.concat(Object.keys(r2[0] || {}).sort())
					.concat(Object.keys(r3[0] || {}).sort());
				CONDITION_NAME_OPTIONS_CACHE[source] = names;
				callback(names);
			})
			.fail(function () {
				done([]);
			});
		return;
	}
	if (source === 'GPIO Pin') {
		// Unlike the "GPIO" Command's own Pin arg (api/options/GPIOLIST, every
		// physical pin on the board), a condition can only ever see a value
		// for pins actually configured as GPIO Inputs (Condition.cpp falls
		// back to GPIOManager::GetInputPinValue, which only knows about pins
		// SetupGPIOInput registered from config/gpio.json's enabled entries -
		// see gpio.cpp). So list only those, read the same way gpio.php reads
		// its own trigger list, and apply the same "enabled" default (missing
		// enabled = active) used there.
		$.ajax({
			url: 'api/configfile/gpio.json',
			dataType: 'json',
			success: function (data) {
				var names = (Array.isArray(data) ? data : [])
					.filter(function (t) { return t.enabled !== false && t.pin; })
					.map(function (t) { return t.pin; });
				done(names);
			},
			error: function () {
				done([]);
			}
		});
		return;
	}
	done([]);
}

var CONDITION_COMPARATORS = [
	'equal to',
	'not equal to',
	'greater than',
	'greater or equal',
	'less than',
	'less or equal',
	'contains',
	'between'
];

function BuildInlineSelect (options, selected, onChange) {
	var $sel = $("<select class='form-select form-select-sm d-inline-block w-auto me-1'></select>");
	$.each(options, function (i, o) {
		$sel.append("<option value='" + o + "'" + (o === selected ? ' selected' : '') + '>' + o + '</option>');
	});
	$sel.on('change', function () {
		onChange($(this).val());
	});
	return $sel;
}

function NewCondition () {
	return { type: 'condition', source: 'Expression', name: '', comparator: 'equal to', value: '' };
}
function NewGroup () {
	return { type: 'group', op: 'AND', items: [], label: '' };
}

function InitConditionLists () {
	$('.conditionListRoot')
		.not('[data-initialized]')
		.each(function () {
			$(this).attr('data-initialized', '1');
			LoadConditionListFromHiddenField(this.id);
		});
}

// Backend JSON shape (ConditionNode::FromJSON, Condition.cpp): a group is
// {"op":"AND"|"OR","conditions":[...]}, a leaf is
// {"source":...,"name":...,"comparator":...,"value":...} - already
// naturally recursive (a "conditions" entry can itself be a group), so no
// backend changes were needed to support nesting, only a frontend that can
// build/read that shape as a tree instead of a flat list. "label" (below) is
// a frontend-only optional display name for a group - FromJSON reads only
// the keys it knows about and silently ignores unrecognized ones, so this
// is a safe additive field; old saved conditions simply have no label.
function BackendJSONToTree (json) {
	if (!json || typeof json !== 'object') {
		return NewGroup();
	}
	if (json.op) {
		var group = { type: 'group', op: json.op === 'OR' ? 'OR' : 'AND', items: [], label: json.label || '' };
		if (Array.isArray(json.conditions)) {
			group.items = json.conditions.map(BackendJSONToTree);
		}
		return group;
	}
	return {
		type: 'condition',
		source: json.source || 'Expression',
		name: json.name || '',
		comparator: json.comparator || 'equal to',
		value: json.value || ''
	};
}

function TreeToBackendJSON (item) {
	if (item.type === 'group') {
		var out = { op: item.op, conditions: item.items.map(TreeToBackendJSON) };
		// Only emit "label" when set, so an unnamed group round-trips to the
		// exact same minimal shape as before this field existed.
		if (item.label) {
			out.label = item.label;
		}
		return out;
	}
	return { source: item.source, name: item.name || '', comparator: item.comparator, value: item.value || '' };
}

function LoadConditionListFromHiddenField (id) {
	var raw = $('#' + id).val();
	var tree;
	if (raw) {
		try {
			tree = BackendJSONToTree(JSON.parse(raw));
		} catch (e) {
			tree = NewGroup();
		}
	} else {
		// Brand-new/unconfigured If - default to one condition, same as if
		// the user had clicked "+ Add Condition" themselves. Safe to default
		// to non-empty now that a single condition renders collapsed (a
		// summary row + Edit button, same as the 2+ case - see
		// RenderConditionTree) rather than an already-expanded
		// Source/Name/Comparator/Value box, so this doesn't reintroduce an
		// unconfigured If that looks "ready" without the user choosing
		// anything - they still have to click Edit to fill it in.
		tree = { type: 'group', op: 'AND', items: [NewCondition()], label: '' };
	}
	conditionListState[id] = tree;
	RenderConditionTree($('#' + id + '_container'), tree, function () {
		OnConditionTreeChanged(id);
	});
	OnConditionTreeChanged(id);
}

// Same rationale as the .commandListRoot change listener above -
// PopulateExistingCommand() sets this hidden field's real saved value via
// inp.val(v).change() *after* the initial render/InitConditionLists pass.
$(document).on('change', '.conditionListRoot', function () {
	LoadConditionListFromHiddenField(this.id);
});

// A toggleStyle 2-option arg (see PrintArgInputs) keeps its real value in a
// hidden mirror input, id-addressed the same way CommandToJSON/
// PopulateExistingCommand read/write every other arg. This listener is what
// keeps the visible btn-check radio pair showing the right selection
// whenever that hidden value changes from either direction - a user
// clicking a radio (below), or a saved value being loaded via
// PopulateExistingCommand's inp.val(v).change().
$(document).on('change', '.toggleStyleRoot', function () {
	var v = $(this).val();
	// .find() (not .nextAll()) since the toggle sits inside its own label
	// wrapper div, not as a direct sibling of the hidden field - both live
	// in the same <td> cell regardless.
	$(this).closest('td').find('.toggleStyleGroup input[type=radio]').each(function () {
		$(this).prop('checked', $(this).val() === v);
	});
});

// Clicking a toggleStyle radio just updates its hidden mirror field (which
// re-fires the listener above to reflect the click back into the radios -
// harmless, since setting the same checked state again doesn't re-trigger
// anything). Delegated since these radios are created dynamically per arg.
// Scoped to .toggleStyleGroup specifically (not the generic .btn-group the
// condition-tree's own Match ALL/ANY toggle also uses) so the two don't
// cross-talk.
$(document).on('change', '.toggleStyleGroup input[type=radio]', function () {
	var $hidden = $(this).closest('td').find('.toggleStyleRoot');
	if ($hidden.length) {
		$hidden.val($(this).val()).trigger('change');
	}
});

function SerializeConditionList (id) {
	var tree = conditionListState[id];
	// No items yet -> write an empty string, not an empty AND-group. An
	// empty AND-group would evaluate to true unconditionally (the loop
	// never runs), silently matching before the user configures anything.
	if (!tree || !tree.items.length) {
		$('#' + id).val('');
		return;
	}
	$('#' + id).val(JSON.stringify(TreeToBackendJSON(tree)));
}

// Wraps SerializeConditionList so the live formula preview (below) refreshes
// alongside serialization - every mutation in the tree already funnels
// through one onChange closure passed down from LoadConditionListFromHiddenField,
// so wrapping it here is the only place this needs to be threaded.
function OnConditionTreeChanged (id) {
	SerializeConditionList(id);
	RefreshConditionFormula(id);
}

function RefreshConditionFormula (id) {
	var tree = conditionListState[id];
	var $out = $('#' + id + '_formula');
	if (!tree || !tree.items.length) {
		$out.text('No conditions configured yet.');
		return;
	}
	$out.html(BuildConditionFormula(tree));
}

// $('<div>').text(s).html() is the standard jQuery-only-toolkit idiom for
// HTML-escaping arbitrary text - was previously repeated inline at every
// datalist-option call site rather than named; consolidated here since
// BuildConditionFormula also needs it, at several points, to stay safe
// against Source/Name/Value text that itself contains "<", "&", etc. Not the
// same as EscapeMultisyncHost (below) - that one's a narrower, incomplete
// (only &/'/< ) regex escape purpose-built for a single-quoted HTML
// attribute context, kept separate rather than merged in here.
function EscapeHtml (s) {
	return $('<div>').text(String(s == null ? '' : s)).html();
}

// Recursively builds a color-coded HTML one-line summary of the whole tree,
// shown above the Check section and reused for the outer preset/task row
// list's summary (FillInCommandTemplate) so neither place shows raw JSON.
// Color is the main readability aid once a condition mixes Expression
// sources/values with plain ones - AND/OR (structure) vs. what's being
// compared vs. the value it's compared against otherwise all read as the
// same undifferentiated wall of text. A group joins its children with its
// own AND/OR, wraps in parens once there's more than one child (so nesting
// precedence reads unambiguously), and prefixes its own label if the user
// named it.
function BuildConditionFormula (item) {
	if (item.type !== 'group') {
		return SummarizeConditionItemHtml(item);
	}
	if (!item.items.length) {
		return (item.label ? '<span class="text-muted">' + EscapeHtml(item.label) + ':</span> ' : '') + '(empty)';
	}
	var opHtml = ' <span class="text-warning fw-semibold">' + item.op + '</span> ';
	var joined = item.items.map(BuildConditionFormula).join(opHtml);
	if (item.items.length > 1) {
		joined = '<span class="text-muted">(</span>' + joined + '<span class="text-muted">)</span>';
	}
	return item.label ? '<span class="text-muted">' + EscapeHtml(item.label) + ':</span> ' + joined : joined;
}

// Same leaf text as SummarizeConditionItem (plain-text, used for the
// collapsed per-row label in the tree editor itself - that one stays plain
// since it's set via .text(), not .html()) but as color-coded, escaped HTML
// for the formula preview/outer summary above. Source == "Expression" is
// shown as "Expression(...)" - function-call notation - rather than
// "Expression <text>", since parens make it visually obvious the contents
// are evaluated as one unit instead of reading like a literal Name (this
// matters more now that Value is unconditionally expression-capable too,
// see RenderConditionFields).
function SummarizeConditionItemHtml (item) {
	if (item.type === 'group') {
		var n = item.items.length;
		var base = 'Group (' + item.op + ', ' + n + (n === 1 ? ' condition' : ' conditions') + ')';
		return item.label ? '<span class="text-muted">' + EscapeHtml(item.label) + ':</span> ' + base : base;
	}
	var lhs =
		item.source === 'Expression'
			? 'Expression(' + EscapeHtml(item.name || '') + ')'
			: EscapeHtml(item.source || 'Expression') + (item.name ? ' ' + EscapeHtml(item.name) : '');
	var s = '<span class="text-info">' + lhs + '</span>';
	s += ' <span class="text-muted">' + EscapeHtml(item.comparator || 'equal to') + '</span>';
	if (item.value !== '') {
		// Value is unconditionally run through the same expression evaluator
		// as an Expression Source (Condition.cpp's LeafNode::evaluate() calls
		// EvaluateConditionExpression() on it regardless of Source) - shown as
		// "Expression(...)" here too, not quoted like a literal, so the
		// summary doesn't imply Value is somehow just fixed text while the
		// left side is "the real expression". Comparator stays outside both -
		// lhs and rhs are evaluated independently and then compared in C++,
		// not combined into one bigger expression the way merging them into a
		// single Expression(...) would wrongly suggest. The closing paren
		// delimits it unambiguously against surrounding " AND "/" OR " text
		// without needing quotes for that anymore.
		s += ' <span class="text-success">Expression(' + EscapeHtml(item.value) + ')</span>';
	}
	return s;
}

// Renders one group's contents ($container is emptied and rebuilt): the
// ALL/ANY match toggle (only for 2+ items, meaningless for exactly one),
// each item - a leaf condition as a collapsed summary+Edit row (unchanged
// from before), or a nested group rendered fully inline/recursively as its
// own bordered, indented card (never behind a modal - this is what makes the
// whole tree's shape visible in one view instead of requiring a chain of
// modals to see nested groups) - and the Add Condition/Add Group buttons.
// onChange() runs after every mutation so the caller can reserialize and
// refresh the formula preview.
function RenderConditionTree ($container, tree, onChange, depth) {
	depth = depth || 0;
	$container.empty();

	if (tree.items.length > 1) {
		$container.append(RenderMatchToggle(tree, onChange));
	}

	$.each(tree.items, function (i, item) {
		var row = (item.type === 'group')
			? RenderConditionGroupRow($container, tree, item, i, onChange, depth + 1)
			: RenderConditionLeafRow($container, tree, item, i, onChange);
		$container.append(row);
	});

	$container.append(RenderAddButtons(tree, $container, onChange, depth));
}

// ALL/ANY toggle - a Bootstrap btn-check radio pair rather than a bare
// AND/OR <select>, so the plain-English word and the technical term (still
// what fppd.log's debug trace prints) are both visible at once. Needs a
// unique radio "name" per call since - unlike the old modal-per-group flow -
// multiple groups can now be visible/interactive on the page simultaneously;
// reusing one name across groups would let clicking one group's toggle
// visually affect a same-named radio in a sibling group.
var conditionMatchToggleSeq = 0;
function RenderMatchToggle (tree, onChange) {
	var seq = ++conditionMatchToggleSeq;
	var name = 'condOp_' + seq;
	var allId = 'condOpAll_' + seq;
	var anyId = 'condOpAny_' + seq;
	var $wrap = $(
		"<div class='mb-2 d-flex align-items-center gap-2'>" +
			"<span class='text-muted'>Match:</span>" +
			"<div class='btn-group btn-group-sm' role='group'>" +
			"<input type='radio' class='btn-check' name='" + name + "' id='" + allId + "' autocomplete='off'" +
			(tree.op !== 'OR' ? ' checked' : '') + ">" +
			"<label class='btn btn-outline-secondary' for='" + allId + "'>Match ALL (AND)</label>" +
			"<input type='radio' class='btn-check' name='" + name + "' id='" + anyId + "' autocomplete='off'" +
			(tree.op === 'OR' ? ' checked' : '') + ">" +
			"<label class='btn btn-outline-secondary' for='" + anyId + "'>Match ANY (OR)</label>" +
			'</div>' +
			'</div>'
	);
	$wrap.find('#' + allId).on('change', function () {
		tree.op = 'AND';
		onChange();
	});
	$wrap.find('#' + anyId).on('change', function () {
		tree.op = 'OR';
		onChange();
	});
	return $wrap;
}

// A nested group's row: a bordered, indented "card" rendered directly
// inline (never behind a modal, unlike a leaf's Edit button) so its entire
// contents - down to any further-nested groups - are visible without
// clicking anything. Groups have no Edit button; Move up/down/Remove act on
// $parentContainer/parentTree directly, same as a leaf row's equivalents.
function RenderConditionGroupRow ($parentContainer, parentTree, group, i, onChange, depth) {
	// Cycle 3 depth-shades regardless of how deep the tree goes (see
	// fpp-system-design.css - --fpp-border-medium is dark-theme-only, so
	// only 3 tokens are safe to reuse across both themes).
	var shade = ((depth - 1) % 3) + 1;
	var indent = Math.min(depth, 3);
	var $card = $(
		"<div class='condGroupCard rounded p-2 mb-2 ms-" + indent + " d-flex flex-column gap-1' data-depth='" + shade + "'></div>"
	);
	var $header = $("<div class='d-flex align-items-center gap-1'></div>");
	var $toggle = $(
		"<button type='button' class='buttons btn-sm' title='Collapse/expand'><i class='fas fa-chevron-down'></i></button>"
	);
	var $body = $("<div class='condGroupBody'></div>");
	$toggle.on('click', function () {
		$body.toggle();
		$toggle.find('i').toggleClass('fa-chevron-down fa-chevron-right');
	});
	var $name = $(
		"<input type='text' class='form-control form-control-sm w-auto flex-fill condGroupLabel' placeholder='Group name (optional)'>"
	).val(group.label || '');
	// input (not just change) for live formula-preview feedback while
	// typing, without re-rendering the tree (which would steal focus).
	$name.on('input', function () {
		group.label = $(this).val();
		onChange();
	});
	var up = $(
		"<button type='button' class='buttons btn-sm' title='Move up'" +
			(i === 0 ? ' disabled' : '') +
			"><i class='fas fa-arrow-up'></i></button>"
	).on('click', function () {
		if (i === 0) {
			return;
		}
		var tmp = parentTree.items[i - 1];
		parentTree.items[i - 1] = parentTree.items[i];
		parentTree.items[i] = tmp;
		RenderConditionTree($parentContainer, parentTree, onChange, depth - 1);
		onChange();
	});
	var down = $(
		"<button type='button' class='buttons btn-sm' title='Move down'" +
			(i === parentTree.items.length - 1 ? ' disabled' : '') +
			"><i class='fas fa-arrow-down'></i></button>"
	).on('click', function () {
		if (i === parentTree.items.length - 1) {
			return;
		}
		var tmp = parentTree.items[i + 1];
		parentTree.items[i + 1] = parentTree.items[i];
		parentTree.items[i] = tmp;
		RenderConditionTree($parentContainer, parentTree, onChange, depth - 1);
		onChange();
	});
	var del = $(
		"<button type='button' class='buttons btn-sm' title='Remove group'><i class='fas fa-trash'></i></button>"
	).on('click', function () {
		parentTree.items.splice(i, 1);
		RenderConditionTree($parentContainer, parentTree, onChange, depth - 1);
		onChange();
	});
	$header.append($toggle, $name, up, down, del);
	$card.append($header, $body);
	RenderConditionTree($body, group, onChange, depth);
	return $card;
}

// A leaf condition's row - unchanged from before this redesign: a collapsed
// one-line summary + Edit/Up/Down/Delete, Edit opening the existing small
// ShowConditionSubEditor modal with RenderConditionFields inside.
function RenderConditionLeafRow ($container, tree, item, i, onChange) {
	var row = $("<div class='condListRow border rounded p-2 mb-2 d-flex align-items-center gap-1'></div>");
	var label = $("<span class='flex-fill'></span>").text(SummarizeConditionItem(item));
	var edit = $(
		"<button type='button' class='buttons btn-sm' title='Edit'><i class='fas fa-pencil-alt'></i></button>"
	).on('click', function () {
		ShowConditionSubEditor('Edit Condition', function ($body) {
			RenderConditionFields($body, item, function () {
				label.text(SummarizeConditionItem(item));
				onChange();
			});
		});
	});
	var up = $(
		"<button type='button' class='buttons btn-sm' title='Move up'" +
			(i === 0 ? ' disabled' : '') +
			"><i class='fas fa-arrow-up'></i></button>"
	).on('click', function () {
		if (i === 0) {
			return;
		}
		var tmp = tree.items[i - 1];
		tree.items[i - 1] = tree.items[i];
		tree.items[i] = tmp;
		RenderConditionTree($container, tree, onChange);
		onChange();
	});
	var down = $(
		"<button type='button' class='buttons btn-sm' title='Move down'" +
			(i === tree.items.length - 1 ? ' disabled' : '') +
			"><i class='fas fa-arrow-down'></i></button>"
	).on('click', function () {
		if (i === tree.items.length - 1) {
			return;
		}
		var tmp = tree.items[i + 1];
		tree.items[i + 1] = tree.items[i];
		tree.items[i] = tmp;
		RenderConditionTree($container, tree, onChange);
		onChange();
	});
	var del = $(
		"<button type='button' class='buttons btn-sm' title='Remove'><i class='fas fa-trash'></i></button>"
	).on('click', function () {
		tree.items.splice(i, 1);
		RenderConditionTree($container, tree, onChange);
		onChange();
	});
	row.append(label, edit, up, down, del);
	return row;
}

// Deeply nested AND/OR trees are hard for a human to reason about
// regardless of how clean the UI is - the depth-shading CSS itself only
// cycles 3 distinct colors and indentation caps at 3 levels (see
// RenderConditionGroupRow), so nesting further stops being visually
// distinguishable anyway. "+ Add Group" is hidden (not just disabled) once
// a group is already 3 levels deep, since a 4th level would be invisible in
// the UI as well as hard to reason about. "+ Add Condition" has no such
// limit - a flat list of conditions inside one group doesn't have this
// problem, only nesting groups within groups does.
var CONDITION_MAX_GROUP_DEPTH = 3;
function RenderAddButtons (tree, $container, onChange, depth) {
	var addCond = $('<input type="button" class="buttons btn-sm me-1" value="+ Add Condition">').on('click', function () {
		tree.items.push(NewCondition());
		RenderConditionTree($container, tree, onChange, depth);
		onChange();
	});
	if (depth >= CONDITION_MAX_GROUP_DEPTH) {
		return [addCond];
	}
	var addGroup = $('<input type="button" class="buttons btn-sm" value="+ Add Group">').on('click', function () {
		tree.items.push(NewGroup());
		RenderConditionTree($container, tree, onChange, depth);
		onChange();
	});
	return [addCond, addGroup];
}

function SummarizeConditionItem (item) {
	if (item.type === 'group') {
		var n = item.items.length;
		var base = 'Group (' + item.op + ', ' + n + (n === 1 ? ' condition' : ' conditions') + ')';
		return item.label ? item.label + ': ' + base : base;
	}
	var s = (item.source || 'Expression') + (item.name ? ' ' + item.name : '') + ' ' + (item.comparator || 'equal to');
	if (item.value !== '') {
		// Quoted (double quotes, since values can themselves contain an
		// apostrophe) so a value is never ambiguous with surrounding text -
		// this matters most once BuildConditionFormula joins several
		// conditions with literal " AND "/" OR ", where an unquoted value
		// containing those words could be misread as tree structure rather
		// than data.
		s += ' "' + item.value + '"';
	}
	return s;
}

// Renders the Source/Name/Comparator/Value fields for one condition into
// $container - used both for the sole-inline-condition case and inside a
// per-condition modal. When Source is "Expression", Name gets the same
// live-validation + variable-insert-helper treatment as the dedicated
// "expression" CommandArg type (Set Variable's own Expression mode) -
// reusing DebounceValidateExpression/PopulateExpressionVarHelpers as-is;
// they only need an input element plus a couple of target ids/classes,
// nothing tying them specifically to the generic command-args renderer.
var conditionFieldIdSeq = 0;
function RenderConditionFields ($container, cond, onChange) {
	$container.empty();

	// Live, color-coded preview of this condition exactly as it'll appear in
	// the formula preview/summary elsewhere (reuses SummarizeConditionItemHtml,
	// the same renderer BuildConditionFormula uses for a leaf) - shown above
	// the fields for every Source, not just Expression, so the user can see
	// what they're building without having to save/close first. Wrapping
	// onChange (rather than adding a call at every individual field's own
	// input/change handler below) means every existing onChange() call in
	// this function already refreshes it for free, including the full
	// re-render on Source change.
	var $preview = $("<div class='mb-2'></div>");
	function RefreshConditionPreview () {
		$preview.html(SummarizeConditionItemHtml(cond));
	}
	RefreshConditionPreview();
	$container.append($preview);

	// Catches the one remaining way to get a silently-useless condition now
	// that Condition.cpp's EvaluateNameOrExpression auto-detects a bare
	// variable name (exact match -> plain lookup) and a bare formula
	// referencing one (LooksLikeFormula -> auto math) with no wrapping
	// needed - see that function's own comment for the full rules. This is
	// a JS mirror of the SAME rules, not a separate/looser heuristic: it
	// used to just flag any bare "mqtt-"/"fpp-" text, which made it fire on
	// cases the backend already handles correctly (an exact variable name
	// alone, or "name+1" as a formula) - false positives that told the user
	// to add wrapping they didn't need, and for the formula case suggested
	// wrapping that would have actively broken it (%%name+1%% looks up a
	// variable literally named "name+1", which doesn't exist). Only warns
	// for what's still genuinely broken: a real variable name embedded in
	// otherwise-literal, non-formula text (e.g. "Status: fpp_mode_name"),
	// where nothing auto-substitutes it.
	function FindUnwrappedVariableReference (text) {
		if (!text) {
			return null;
		}
		var trimmed = text.trim();
		var knownNames = CONDITION_NAME_OPTIONS_CACHE['Variable'] || [];
		if (knownNames.indexOf(trimmed) !== -1) {
			return null; // exact match - EvaluateNameOrExpression does a plain lookup, no wrapping needed
		}
		if (text.indexOf('=') === 0 || text.indexOf('%%') !== -1 || text.indexOf('==') !== -1) {
			return null; // already using explicit syntax
		}
		// Mirrors Condition.cpp's LooksLikeFormula(): strip every known
		// variable name out as a substring (longest-first), then check
		// what's left for a math/comparison operator character.
		var remaining = text;
		var sortedNames = knownNames.slice().sort(function (a, b) {
			return b.length - a.length;
		});
		for (var i = 0; i < sortedNames.length; i++) {
			var n = sortedNames[i];
			if (!n) {
				continue;
			}
			var idx;
			while ((idx = remaining.indexOf(n)) !== -1) {
				remaining = remaining.slice(0, idx) + remaining.slice(idx + n.length);
			}
		}
		if (/[+\-*/()<>=!^]/.test(remaining)) {
			return null; // looks like a formula - auto-prepended "=" handles it, no wrapping needed
		}
		var m = text.match(/\bmqtt-\S+|\bfpp_\S+/);
		return m ? m[0] : null;
	}
	var $warning = $("<div class='text-warning small mb-2' style='display:none'></div>");
	function RefreshConditionWarning () {
		// Name is only expression-evaluated when Source == "Expression" - for
		// every other Source (Variable, GPIO Pin, Sun) it's a plain exact-match
		// lookup key (Variables::getVariable(name, "") etc.), so a bare
		// "fpp-..."/"mqtt-..." there is completely correct, not a mistake.
		// Value is unconditionally expression-evaluated regardless of Source,
		// so it's always worth checking.
		var hit = (cond.source === 'Expression' ? FindUnwrappedVariableReference(cond.name) : null) || FindUnwrappedVariableReference(cond.value);
		if (!hit) {
			$warning.hide();
			return;
		}
		$warning
			.html(
				"<i class='fas fa-triangle-exclamation me-1'></i>Contains \"" +
					EscapeHtml(hit) +
					'" which looks like a Variable name, but isn\'t wrapped in <code>%%...%%</code> - it will be compared as literal ' +
					'text, not that Variable\'s actual value. Wrap the variable name itself, e.g. <code>%%' +
					EscapeHtml(hit) +
					'%%</code>.'
			)
			.show();
	}
	RefreshConditionWarning();
	$container.append($warning);

	var innerOnChange = onChange;
	onChange = function () {
		RefreshConditionPreview();
		RefreshConditionWarning();
		innerOnChange();
	};

	// $row holds Source/Name/is/Comparator/Value/eye on one line via flex, with
	// align-items-start so a two-line $nameWrap (Expression source, see below)
	// doesn't stretch/recenter the single-line controls beside it. For
	// Expression specifically, $nameWrap itself becomes a flex-column - the
	// expression input on its own line, with the insert-variable helper
	// directly beneath it (same left edge, same width) rather than spanning
	// the whole row's width as an unrelated-looking sibling underneath
	// everything.
	var $row = $("<div class='d-flex align-items-start flex-wrap gap-1'></div>");

	var sourceSel = BuildInlineSelect(CONDITION_SOURCES, cond.source, function (v) {
		cond.source = v;
		// Name/Comparator/Value typed against the old source (e.g. a GPIO
		// pin name where a Variable name now belongs, or a "1"/"0" that was
		// only meaningful for a GPIO/Sensor-style reading) are generally
		// meaningless once Source changes, so don't leave them looking like
		// they still apply - reset the whole row back to defaults rather
		// than just Value. Each Source's own branch below still fills in
		// its own default where one makes sense (e.g. Sun defaults Name to
		// "Sunrise").
		cond.name = '';
		cond.comparator = 'equal to';
		cond.value = '';
		RenderConditionFields($container, cond, onChange); // re-render so Name's widget can swap for Expression
		onChange();
	});

	var $nameWrap = $("<span class='d-inline-block'></span>");
	if (CONDITION_SOURCES_NO_NAME_FIELD.indexOf(cond.source) !== -1) {
		// These all ignore `name` entirely in Condition.cpp - nothing to fill
		// in, so don't show a field that would look meaningful but do
		// nothing.
		cond.name = '';
	} else if (cond.source === 'Expression') {
		var fieldId = 'condExpr_' + ++conditionFieldIdSeq;
		// textarea, not a single-line input: a formula long enough to need
		// the horizontal scrolling a single-line box would force is common
		// enough here (and for Value below) that seeing more of it at once,
		// wrapped, is worth the extra vertical space - resize:vertical (the
		// browser default for a textarea) still lets it be dragged taller
		// still for anything even longer.
		var nameInp = $(
			"<textarea id='" +
				fieldId +
				"' rows='2' class='form-control form-control-sm flex-grow-1' placeholder='Expression'></textarea>"
		)
			.val(cond.name)
			.on('input', function () {
				cond.name = $(this).val();
				DebounceValidateExpression(this, fieldId + '_kindBadge', true);
				onChange();
			});
		// No trailing valid/invalid icon here anymore - see the matching
		// comment on Value below; the badge lives next to the field's own
		// label instead (LabeledField's badgeId param).
		// Fixed width (rather than d-inline-block/w-auto) so the insert-variable
		// helper (appended below, once $row's full layout - including whether
		// Value is also an Expression - is known) lines up under the SAME
		// left/right edges as the expression input, instead of two
		// independently-sized boxes that only coincidentally share a left edge.
		$nameWrap = $("<div class='d-flex flex-column' style='width:260px'></div>");
		var $inputLine = $("<div class='d-flex align-items-center'></div>").append(nameInp);
		$nameWrap.append($inputLine);
		if (cond.name) {
			DebounceValidateExpression(nameInp[0], fieldId + '_kindBadge', true);
		}
	} else if (cond.source === 'Sun') {
		// Only two valid values (Condition.cpp's sunTimeHHMM: anything that
		// isn't literally "Sunrise" falls through to sunset) - a free-text
		// field here is pure typo risk for zero benefit, so use a real
		// dropdown instead of a suggested-values datalist.
		var sunSel = BuildInlineSelect(['Sunrise', 'Sunset'], cond.name || 'Sunrise', function (v) {
			cond.name = v;
			onChange();
		});
		if (!cond.name) {
			cond.name = 'Sunrise';
		}
		$nameWrap.append(sunSel);
	} else {
		var nameFieldId = 'condName_' + ++conditionFieldIdSeq;
		var plainNameInp = $(
			"<input type='text' id='" +
				nameFieldId +
				"' list='" +
				nameFieldId +
				"_list' class='form-control form-control-sm d-inline-block w-auto' placeholder='" +
				(CONDITION_NAME_LABELS[cond.source] || 'Name') +
				"'>"
		)
			.val(cond.name)
			.on('input', function () {
				cond.name = $(this).val();
				onChange();
			});
		var nameDatalist = $("<datalist id='" + nameFieldId + "_list'></datalist>");
		$nameWrap.append(plainNameInp, nameDatalist);
		FetchConditionNameOptions(cond.source, function (names) {
			$.each(names, function (i, n) {
				nameDatalist.append("<option value='" + EscapeHtml(n) + "'>");
			});
		});
	}

	var compSel = BuildInlineSelect(CONDITION_COMPARATORS, cond.comparator, function (v) {
		cond.comparator = v;
		onChange();
	});

	// Value is always run through the same tinyexpr engine as Source ==
	// "Expression" (Condition.cpp's LeafNode::evaluate() now calls
	// EvaluateConditionExpression() unconditionally) - a plain literal like
	// "1" or "ON" compiles to itself unchanged, so this is a strict superset
	// of the old fixed-Value behavior, not a separate mode needing its own
	// toggle.
	var valueFieldId = 'condValue_' + ++conditionFieldIdSeq;
	// textarea for the same reason as Name/Expression above - this field is
	// unconditionally expression-capable for every Source, not just
	// Expression, so it's just as likely to hold something long.
	var valInp = $(
		"<textarea id='" +
			valueFieldId +
			"' rows='2' class='form-control form-control-sm flex-grow-1' placeholder='Value or Expression'></textarea>"
	)
		.val(cond.value)
		.on('input', function () {
			cond.value = $(this).val();
			DebounceValidateExpression(this, valueFieldId + '_kindBadge', true);
			onChange();
		});
	// No trailing valid/invalid icon here anymore - what kind of thing this
	// text currently resolves to (Variable/Formula/Text/Invalid) is shown as
	// a badge next to the "Value" label itself instead (see LabeledField's
	// badgeId param below), which also means the textarea's own right edge
	// now actually lines up with the insert-variable arrow underneath it.
	var $valueWrap = $("<div class='d-flex flex-column' style='width:260px'></div>");
	$valueWrap.append($("<div class='d-flex align-items-center'></div>").append(valInp));
	if (cond.value) {
		DebounceValidateExpression(valInp[0], valueFieldId + '_kindBadge', true);
	}
	// Day/Month/Player Status only have a handful of valid answers - a
	// <datalist> would be the usual way to suggest them (see the Name
	// field's own datalist a bit above), but datalist/list= only works on
	// <input>, not the <textarea> Value now is. A small "quick pick" select
	// that just fills Value in on choice gets the same one-click result.
	var valueSuggestions = CONDITION_VALUE_SUGGESTIONS[cond.source];
	if (valueSuggestions) {
		var $quickPick = $("<select class='form-select form-select-sm mt-1'><option value=''>-- Quick pick --</option></select>");
		$.each(valueSuggestions, function (i, v) {
			var optValue = typeof v === 'object' ? v.value : v;
			var optLabel = typeof v === 'object' ? v.label : v;
			$quickPick.append("<option value='" + EscapeHtml(optValue) + "'>" + EscapeHtml(optLabel) + '</option>');
		});
		$quickPick.on('change', function () {
			var v = $(this).val();
			if (v) {
				cond.value = v;
				valInp.val(v);
				onChange();
			}
			$(this).val('');
		});
		$valueWrap.append($quickPick);
	}

	// One consolidated eye button (was two, one per side) - opens a modal
	// with three views of the WHOLE leaf at once: the raw formula, the same
	// formula with each side's current value substituted in, and the actual
	// LHS/Comparator/RHS/result evaluation - see ShowConditionEvaluationPopup.
	var showEvalBtn = $(
		"<button type='button' class='buttons reallySmallButton ms-1' title='Show how this condition currently evaluates'><i class='fas fa-eye'></i></button>"
	).on('click', function () {
		ShowConditionEvaluationPopup(cond);
	});

	// Small muted label above each control - reuses CONDITION_NAME_LABELS
	// (already the placeholder text) as the Name column's label too, so
	// "what goes here" is contextual (e.g. "GPIO Pin Name") rather than a
	// generic "Name" that doesn't say what kind of name. Time gets no Name
	// label since it has no Name field at all to label.
	// badgeId, when given, adds an empty pill right after the label text -
	// DebounceValidateExpression (conditionExpr=true mode) fills it in with
	// what the field's current text resolves to (Variable/Formula/Text/
	// Invalid). Replaces the old per-field check/X/amber icon: a label like
	// "Value [Invalid]" reads on its own, no hovering for a tooltip needed,
	// and it stops competing with the insert-variable arrow for space right
	// next to the textarea.
	function LabeledField (label, $control, badgeId) {
		if (!label) {
			return $control;
		}
		var $label = $("<span class='text-muted text-center' style='font-size:0.8rem'></span>").text(label);
		if (badgeId) {
			$label.append(' ', $("<span id='" + badgeId + "'></span>"));
		}
		return $("<div class='d-flex flex-column'></div>").append($label, $control);
	}
	// The eye buttons sit beside labeled columns (each a label span stacked
	// above its control), so a bare button would render shorter and land a
	// row higher than the controls beside it. Give it the same invisible
	// label spacer (via a non-breaking space, so the muted span still takes
	// up a line instead of collapsing to zero height) so its own control
	// row lines up with the inputs/selects next to it.
	function EyeButtonField ($btn) {
		return $("<div class='d-flex flex-column'></div>").append($("<span style='font-size:0.8rem'> </span>"), $btn);
	}
	var nameLabelText = CONDITION_SOURCES_NO_NAME_FIELD.indexOf(cond.source) !== -1 ? null : CONDITION_NAME_LABELS[cond.source] || 'Name';
	$row.append(
		LabeledField('Source', sourceSel),
		// Only the Expression source's Name field runs through
		// DebounceValidateExpression at all (every other source's Name is
		// either absent or a fixed picker/lookup key, not expression text) -
		// fieldId only exists in that branch above, so the badge is only
		// wired up there.
		LabeledField(nameLabelText, $nameWrap, cond.source === 'Expression' ? fieldId + '_kindBadge' : null),
		// "Comparator" doubles as the "is" connector now (e.g. "Variable
		// myVar" / "Comparator: equal to" reads the same as "myVar is equal
		// to" did before) - a separate literal " is " text plus a
		// "Comparator" label above the dropdown was saying the same thing
		// twice.
		LabeledField('Is', compSel),
		LabeledField('Value', $valueWrap, valueFieldId + '_kindBadge'),
		EyeButtonField(showEvalBtn)
	);
	$container.append($row);

	// Value is always expression-capable, and Name is too whenever Source is
	// "Expression" (which now also covers what the old, separate "Variable"
	// Source did - merged/removed, see CONDITION_SOURCES) - share a single
	// insert box between the two fields with an up-arrow on either side,
	// rather than stacking two separate "insert variable" boxes. Every other
	// Source's Name field is either absent (Time) or a fixed picker (Sun)/
	// lookup key that isn't meaningfully "insert a variable into" (GPIO
	// Pin), so those still just get Value's own single-target helper, same
	// as before. Inserting always wraps in "%%...%%" (see
	// RenderSharedExpressionValueHelper) - correct regardless of whether the
	// field ends up being just that one variable's name (an exact-match
	// %%wrapped%% name still resolves via template-mode substitution, same
	// result as an unwrapped exact match) or part of a larger formula.
	if (cond.source === 'Expression') {
		RenderSharedExpressionValueHelper($container, nameInp, valInp, fieldId, valueFieldId, 'Expression');
	} else {
		var $valueOnlyHelper = $("<div class='expressionVarHelper text-muted small mt-1' data-target='" + valueFieldId + "'></div>");
		$valueWrap.append($valueOnlyHelper);
		PopulateExpressionVarHelpers();
	}

	// Always-visible (not hover-only) explanation of the selected Source,
	// appended last so it lands at the bottom of the dialog body - between
	// the fields above and the Close button, which ShowConditionSubEditor
	// renders into a separate sibling "_footer" element.
	// Entries are static, developer-authored strings (never user input), so
	// they're trusted HTML - not run through EscapeHtml - which lets the
	// Expression entry above use <ul>/<li> bullets instead of one run-on
	// sentence.
	$container.append(
		$("<div class='text-muted small mt-2'></div>").html(
			"<i class='fas fa-circle-info me-1'></i>" + (CONDITION_SOURCE_HELP[cond.source] || '')
		)
	);
}

// The two-field ("Expression" Source + Value, both expression-capable)
// insert-variable helper: a single box shared between them with an up-arrow
// on each side, positioned so each arrow sits directly under the actual
// horizontal center of the field it targets - not just at the left/right
// edges of the row, which pointed at nothing whenever the row's other
// controls (Source select, "is", Comparator) made the two fields
// asymmetric. Measures real DOM positions via getBoundingClientRect() rather
// than guessing from flex order, so it stays correct regardless of
// Comparator/label width. Falls back to a plain non-pointing row when the
// two fields have wrapped onto separate lines (narrow modal/mobile) - there
// no single horizontal position can point at both at once.
function RenderSharedExpressionValueHelper ($container, nameInp, valInp, nameFieldId, valueFieldId, leftLabel) {
	leftLabel = leftLabel || 'Expression';
	var $helperRow = $("<div class='mt-1' style='position:relative;height:34px'></div>");
	var $leftArrow = $(
		"<button type='button' class='buttons reallySmallButton varInsertArrow' title='Insert into the " + leftLabel + " field above'><i class='fas fa-arrow-up'></i></button>"
	);
	var $rightArrow = $(
		"<button type='button' class='buttons reallySmallButton varInsertArrow' title='Insert into the Value field above'><i class='fas fa-arrow-up'></i></button>"
	);
	// A field .focus()ed programmatically keeps whatever selection range it
	// already had - 0,0 for one that's never been clicked into, since
	// selectionStart defaults to 0, not null (InsertAtCursor's null check
	// only catches inputs that don't support selection at all). Without
	// explicitly moving the caret to the end first, every insert via an
	// arrow lands back at position 0 - each one *prepending*, silently
	// mashing values together with no separator in reverse order instead of
	// appending like a user typing normally would expect.
	function focusAtEnd (el) {
		el.focus();
		var len = el.value.length;
		el.setSelectionRange(len, len);
	}
	var activeTarget = nameFieldId;
	var $input = $(
		"<input type='text' list='" +
			VAR_INSERT_DATALIST_ID +
			"' class='form-control form-control-sm placeholder-center position-absolute' placeholder='-- Select Variable to Insert --'>"
	);
	function insertPendingValue (targetId) {
		var name = $input.val();
		if (!name) {
			return;
		}
		// Clear BEFORE calling InsertAtCursor, not after: InsertAtCursor calls
		// .focus() on the target field, which synchronously blurs $input right
		// here mid-call - re-firing $input's own 'change' handler below before
		// this call returns. If $input still held "name" at that point, the
		// reentrant call would insert the exact same text a second time
		// (this was a real bug: clicking an arrow after picking a variable
		// inserted it twice). Clearing first makes the reentrant call's
		// $input.val() empty, so it's a no-op.
		$input.val('');
		// Inserted bare, NOT wrapped in "%%...%%": Condition.cpp's
		// EvaluateNameOrExpression() already auto-detects a bare real
		// variable name (exact match -> plain lookup) and a bare formula
		// referencing one (LooksLikeFormula -> auto math, via
		// ExpressionProcessor's own aliasExpr substring substitution) with
		// no wrapping needed. Wrapping used to be applied here defensively,
		// but that's actively WRONG for combining two inserted variables
		// with a math operator (e.g. "%%a%%+%%b%%") - the "%%" forces
		// template mode, which does STRING CONCATENATION ("3"+"4" ->
		// "3+4"), not arithmetic. Bare "a+b" correctly triggers pure math
		// mode instead. "%%...%%" is still valid syntax for embedding a
		// variable's value inside other literal text (not a formula) - just
		// type it by hand for that case, this helper no longer does it
		// automatically.
		InsertAtCursor(document.getElementById(targetId), name);
	}
	// mousedown, not click, with preventDefault - a plain click handler here
	// runs too late: clicking a button blurs whatever input was previously
	// focused FIRST (firing $input's own 'change' below with the stale
	// activeTarget from before this click), and only THEN does the button's
	// click handler fire. That race is exactly what sent a just-picked
	// variable into the wrong field after switching targets. preventDefault()
	// on mousedown stops the browser's default focus-shift entirely, so
	// $input never blurs/changes here - this handler alone decides where the
	// currently-typed/selected value goes.
	$leftArrow.on('mousedown', function (e) {
		e.preventDefault();
		activeTarget = nameFieldId;
		insertPendingValue(nameFieldId);
		focusAtEnd(nameInp[0]);
	});
	$rightArrow.on('mousedown', function (e) {
		e.preventDefault();
		activeTarget = valueFieldId;
		insertPendingValue(valueFieldId);
		focusAtEnd(valInp[0]);
	});
	// Still needed for the case where the user picks/types a value and just
	// tabs away or presses Enter without touching an arrow at all - inserts
	// into whichever target is currently active.
	$input.on('change', function () {
		insertPendingValue(activeTarget);
	});
	$helperRow.append($leftArrow, $input, $rightArrow);
	$container.append($helperRow);

	if (!$('#' + VAR_INSERT_DATALIST_ID).length) {
		var $datalist = $("<datalist id='" + VAR_INSERT_DATALIST_ID + "'></datalist>").appendTo('body');
		FetchConditionNameOptions('Variable', function (names) {
			$.each(names, function (i, n) {
				$datalist.append("<option value='" + EscapeHtml(n) + "'>");
			});
		});
	}

	function position () {
		if (!document.body.contains(nameInp[0]) || !document.body.contains(valInp[0])) {
			$(window).off('.' + resizeNS);
			return;
		}
		var containerLeft = $container[0].getBoundingClientRect().left;
		var nameRect = nameInp[0].getBoundingClientRect();
		var valueRect = valInp[0].getBoundingClientRect();
		var sameRow = Math.abs(nameRect.top - valueRect.top) < 5;
		if (!sameRow) {
			// Stacked (narrow layout) - nothing to point at sideways, just show
			// a plain row. justify-content-center (not just align-items-center,
			// which only centers cross-axis/vertically) actually centers the
			// left-arrow/input/right-arrow trio horizontally within the full
			// row width, instead of it defaulting to flex's normal
			// left-aligned packing.
			$helperRow.css({ height: 'auto', position: 'static', width: '100%' });
			$leftArrow.css({ position: 'static', left: '' });
			$rightArrow.css({ position: 'static', left: '' });
			$input.css({ position: 'static', left: '', width: '200px' });
			$helperRow.removeClass().addClass('mt-1 d-flex align-items-center justify-content-center gap-1');
			return;
		}
		$helperRow.removeClass().addClass('mt-1');
		$helperRow.css({ position: 'relative', height: '34px' });
		var nameCenter = nameRect.left + nameRect.width / 2 - containerLeft;
		var valueCenter = valueRect.left + valueRect.width / 2 - containerLeft;
		$leftArrow.css({ position: 'absolute', left: nameCenter + 'px', transform: 'translateX(-50%)' });
		$rightArrow.css({ position: 'absolute', left: valueCenter + 'px', transform: 'translateX(-50%)' });
		// Gap past each arrow's own half-width (~21px, a reallySmallButton
		// with an icon) plus visible breathing room, so the box reads as
		// clearly separate from the arrows on either side rather than
		// crowding them.
		var inputLeft = nameCenter + 50;
		var inputRight = valueCenter - 50;
		$input.css({ position: 'absolute', left: inputLeft + 'px', width: Math.max(60, inputRight - inputLeft) + 'px' });
	}
	// Calling position() synchronously here can measure a zero-sized
	// container: this helper is built while ShowConditionSubEditor's nested
	// dialog is still being opened, and jQuery UI's dialog widget doesn't
	// finish laying out/positioning itself before this render call returns -
	// a getBoundingClientRect() taken this early comes back all zeros (seen
	// live: editing a condition nested inside a group, one dialog stacked on
	// another - a plain double requestAnimationFrame still measured zero
	// there, so the dialog settles later than "next paint"). Poll via rAF
	// until $container actually has real width instead of guessing a fixed
	// frame count, bounded so a container that's genuinely still zero-width
	// (shouldn't happen, but don't hang forever) still gets one final
	// best-effort position() call rather than none.
	(function waitForLayout (attemptsLeft) {
		if ($container[0].getBoundingClientRect().width > 0 || attemptsLeft <= 0) {
			position();
			return;
		}
		requestAnimationFrame(function () {
			waitForLayout(attemptsLeft - 1);
		});
	})(30);

	// Re-measure on window resize (e.g. browser resized while the dialog is
	// open) - self-unsubscribes the first time it finds these fields gone
	// (RenderConditionFields re-rendered this row), so re-editing the same
	// condition repeatedly doesn't pile up dead listeners on window.
	var resizeNS = 'conditionExprHelper_' + nameFieldId;
	$(window).on('resize.' + resizeNS, position);
}

// Consolidated eye-preview modal (replaces the old two-button "show LHS" /
// "show RHS" pair) - one request to api/fppd/condition/preview in its
// full-leaf mode (comparator+value params supplied, see GetConditionPreview
// in httpAPI.cpp) gets everything needed for three views of the SAME leaf at
// once: the raw formula, that formula with each side's current value
// substituted in, and the actual LHS/Comparator/RHS/result evaluation.
// Reuses ConditionNode::PreviewLeafResult() server-side, the exact same
// evaluation path a real saved leaf's evaluate() takes, so "result" here can
// never drift from what actually happens at runtime.
var conditionValuePreviewSeq = 0;
function ShowConditionEvaluationPopup (cond) {
	var domId = 'condValuePreview_' + (++conditionValuePreviewSeq);
	var $popup = $("<div id='" + domId + "'><div class='condValuePreviewBody'>Loading...</div></div>").appendTo('body');
	$popup.fppDialog({
		height: 'auto',
		width: 500,
		title: 'How this condition evaluates',
		modal: true,
		open: function () {
			$popup.parent().find('.ui-dialog-titlebar-close').hide();
		},
		close: function () {
			$popup.remove();
		},
		buttons: {
			Close: function () {
				$popup.fppDialog('close');
			}
		}
	});
	var mainPreview = $.ajax({
		url: 'api/fppd/condition/preview',
		data: { source: cond.source, name: cond.name || '', comparator: cond.comparator || 'equal to', value: cond.value || '', not: cond.negate ? 'true' : 'false' },
		dataType: 'json'
	});
	// Resolves with {varName: value} for every real variable name referenced
	// in Name/Value, so "Values substituted" can replace each NAME with its
	// own VALUE directly inside the "Expression(...)" text (e.g.
	// "Expression(fpp_mode_name*3)" -> "Expression("player"*3)"), not just
	// report the final post-computation LHS/RHS number, which doesn't say
	// what fed into it.
	var varValues = $.Deferred();
	FetchConditionNameOptions('Variable', function (knownNames) {
		// Classified per field, not concatenated together - an exact-match
		// check (see FindReferencedVariableNames) needs to see each field's
		// own trimmed text alone, not Name+Value glued together with a
		// space, which would never equal any real variable name and would
		// always fall through to the (less precise) substring scan instead.
		var nameRefs = FindReferencedVariableNames(cond.name || '', knownNames);
		var valueRefs = FindReferencedVariableNames(cond.value || '', knownNames);
		var refs = nameRefs.concat(
			valueRefs.filter(function (r) {
				return nameRefs.indexOf(r) === -1;
			})
		);
		if (!refs.length) {
			varValues.resolve({});
			return;
		}
		$.when
			.apply(
				$,
				refs.map(function (varName) {
					return $.ajax({ url: 'api/fppd/condition/preview', data: { source: 'Expression', name: varName }, dataType: 'json' });
				})
			)
			.done(function () {
				var results = refs.length === 1 ? [arguments[0]] : Array.prototype.slice.call(arguments).map(function (r) {
					return r[0];
				});
				var map = {};
				refs.forEach(function (varName, i) {
					var r = results[i];
					map[varName] = r && r.found ? r.value : null;
				});
				varValues.resolve(map);
			});
	});
	// Longest-name-first substring replace of each known variable name with
	// its own quoted value (or an explicit "(not found)" marker) - same
	// longest-first rationale as ExpressionProcessor's own aliasExpr(), so a
	// name that's a substring of a longer one can't get partially replaced
	// first and corrupt the longer match.
	function SubstituteVariableValues (text, valueMap) {
		var names = Object.keys(valueMap).sort(function (a, b) {
			return b.length - a.length;
		});
		var result = text;
		names.forEach(function (n) {
			var replacement = valueMap[n] == null ? '(not found)' : '"' + valueMap[n] + '"';
			result = result.split(n).join(replacement);
		});
		return result;
	}
	$.when(mainPreview, varValues).done(function (previewResult, valueMap) {
		var data = previewResult[0];
		var $body = $popup.find('.condValuePreviewBody');
		$body.empty();

		$body.append($("<div class='mb-3'></div>").append($("<div class='text-muted small mb-1'>Formula</div>"), $("<div></div>").html(SummarizeConditionItemHtml(cond))));

		if (!data) {
			$body.append("<div class='text-danger'>Failed to fetch condition evaluation.</div>");
			return;
		}

		// LHS not found no longer stops the popup cold - the API now always
		// returns rhsValue/result too (a not-found LHS still yields a
		// definite runtime result of `negate`), so every item below still
		// gets evaluated and shown; only the LHS side falls back to a
		// "(not found)" placeholder instead of its actual value.
		var lhsFound = !!data.found;
		var lhsSubstituted = cond.source === 'Expression' ? SubstituteVariableValues(cond.name || '', valueMap) : cond.name || '';
		var rhsSubstituted = SubstituteVariableValues(cond.value || '', valueMap);
		var lhsDisplay = cond.source === 'Expression' ? 'Expression(' + EscapeHtml(lhsSubstituted) + ')' : EscapeHtml(cond.source) + (cond.name ? ' ' + EscapeHtml(lhsSubstituted) : '');

		if (!lhsFound) {
			var missingLabel = cond.source === 'Expression'
				? 'Expression(' + EscapeHtml(cond.name || '') + ')'
				: EscapeHtml(cond.source) + (cond.name ? ' ' + EscapeHtml(cond.name) : '');
			$body.append("<div class='text-muted mb-3'>" + missingLabel + " has no current value - not set/seen yet, or the Name doesn't match anything.</div>");
		}

		var substituted =
			'<span class="text-info">' + lhsDisplay + '</span>' +
			' <span class="text-muted">' + EscapeHtml(cond.comparator || 'equal to') + '</span>' +
			' <span class="text-success">Expression(' + EscapeHtml(rhsSubstituted) + ')</span>';
		$body.append($("<div class='mb-3'></div>").append($("<div class='text-muted small mb-1'>Values substituted</div>"), $("<div></div>").html(substituted)));

		var lhsValueDisplay = lhsFound ? '"' + EscapeHtml(data.value) + '"' : '(not found)';
		var resultLine =
			'<code>' + lhsValueDisplay + '</code> <span class="text-muted">' + EscapeHtml(cond.comparator || 'equal to') +
			'</span> <code>"' + EscapeHtml(data.rhsValue || '') + '"</code> &rarr; <span class="' + (data.result ? 'text-success' : 'text-danger') + ' fw-semibold">' +
			(data.result ? 'true' : 'false') + (lhsFound ? '' : ' <span class="text-muted fw-normal">(no current value, so nothing was actually compared)</span>') + '</span>';
		$body.append($("<div></div>").append($("<div class='text-muted small mb-1'>Result</div>"), $("<div></div>").html(resultLine)));
	});
	$.when(mainPreview).fail(function () {
		$popup.find('.condValuePreviewBody').html("<span class='text-danger'>Failed to fetch condition evaluation.</span>");
	});
}

// Finds which real, currently-known variable names appear as substrings in
// `text` (longest-first, so e.g. a name that's a substring of a longer one
// doesn't get double-reported) - used by ShowConditionEvaluationPopup to
// show each referenced variable's own value, not just the final computed
// LHS/RHS.
function FindReferencedVariableNames (text, knownNames) {
	// An exact whole-field match takes priority over any substring scan -
	// mirrors Condition.cpp's EvaluateNameOrExpression, which returns that
	// variable's raw value for an exact trimmed match and never even runs
	// the formula-reference heuristic in that case. Skipping this check let
	// a short real variable name (e.g. "state") that happens to appear as a
	// substring inside a longer, unrelated field (a typo'd variable name
	// that isn't actually an exact match to anything) steal a false
	// substitution below - two unrelated variables' values getting spliced
	// together with no separator in the "Values substituted" line.
	var trimmed = (text || '').trim();
	if (trimmed && knownNames.indexOf(trimmed) !== -1) {
		return [trimmed];
	}
	var found = [];
	var sorted = knownNames.slice().sort(function (a, b) {
		return b.length - a.length;
	});
	var remaining = text;
	for (var i = 0; i < sorted.length; i++) {
		var n = sorted[i];
		if (!n || remaining.indexOf(n) === -1) {
			continue;
		}
		found.push(n);
		var idx;
		while ((idx = remaining.indexOf(n)) !== -1) {
			remaining = remaining.slice(0, idx) + remaining.slice(idx + n.length);
		}
	}
	return found;
}

// Lightweight dedicated modal for editing one condition or group - not
// ShowCommandEditor, which is specific to Commands (a fixed arg schema
// from getDescription()); conditions/groups are plain in-memory objects
// mutated live as the user edits, with no separate Save/Cancel state to
// reconcile, so a single Close button is enough. A fresh, uniquely-id'd
// dialog element per call - not a shared singleton like
// commandEditorPopup - so editing a condition *inside* an already-open
// group modal genuinely stacks (Bootstrap handles multiple independent
// modal instances stacking correctly) instead of replacing the parent,
// the same class of bug already fixed once for the nested commandlist
// editor - avoided here by construction instead of by capture/restore.
var conditionSubEditorSeq = 0;
function ShowConditionSubEditor (title, renderBodyFn) {
	var domId = 'conditionSubEditor_' + ++conditionSubEditorSeq;
	// Body and footer are separate containers: renderBodyFn (RenderConditionTree)
	// re-renders "_body" on every Add Group/Add Condition, so the Close button
	// lives in "_footer" instead - otherwise it gets wiped out along with the
	// tree on the very next edit (reported: "close button disappears").
	var $popup = $(
		"<div id='" + domId + "'><div id='" + domId + "_body'></div>" +
			"<div id='" + domId + "_footer' class='mt-2'></div></div>"
	).appendTo('body');
	$popup.fppDialog({
		height: 'auto',
		width: 500,
		title: title,
		modal: true,
		// Match ShowCommandEditor's own convention (fpp.js ~10660): FPP's editor
		// dialogs don't use a title-bar close X at all, only a bottom Cancel/Close
		// button - hide the titlebar close here too instead of trying to make it
		// visible, so this dialog looks and behaves like the rest of the app.
		open: function () {
			$popup.parent().find('.ui-dialog-titlebar-close').hide();
		},
		close: function () {
			$popup.remove();
		}
	});
	renderBodyFn($('#' + domId + '_body'));
	$('<input type="button" class="buttons wideButton" value="Close">')
		.on('click', function () {
			$popup.fppDialog('close');
		})
		.appendTo('#' + domId + '_footer');
}

// Live valid/invalid indicator for an "expression" arg field, debounced so
// it validates against api/variables?validateExpression=... only after the
// user pauses typing, not on every keystroke.
var expressionValidateTimers = {};
var expressionValidateSeq = {};
// conditionExpr=true (only passed by the If Check editor's Name/Value
// fields - see the DebounceValidateExpression calls in
// RenderConditionFields) asks the server to classify the text the same way
// it's actually evaluated at runtime (ConditionNode::ClassifyNameOrExpression)
// instead of a bare compile check, and renders the result as a small pill
// badge next to the field's own label (id passed as targetId - see
// LabeledField's badgeId param) rather than a check/X icon next to the
// field itself: "Value [Invalid]" reads on its own with no hovering for a
// tooltip needed, distinguishes a real variable match / working formula /
// inert unmatched literal text from each other (a bare compile check alone
// can't - unwrapped, unmatched plain text always "compiles" as a literal
// passthrough, so a typo'd variable name used to look identical to a real
// match), and stops competing with the insert-variable arrow for space
// right next to the textarea. The plain icon-in-targetId path below is only
// still used by the generic "expression" CommandArg widget (e.g. Set
// Variable's Expression field), which validates via a bare
// ExpressionProcessor::compile() call with no such classification.
function DebounceValidateExpression (inputEl, targetId, conditionExpr) {
	var value = inputEl.value;
	clearTimeout(expressionValidateTimers[targetId]);
	if (!value) {
		$('#' + targetId).empty();
		return;
	}
	if (conditionExpr) {
		$('#' + targetId).text('…').removeClass().addClass('badge rounded-pill text-bg-secondary');
	} else {
		$('#' + targetId).html('<i class="fas fa-ellipsis-h text-muted" title="Validating..."></i>');
	}
	expressionValidateTimers[targetId] = setTimeout(function () {
		// Guard against an earlier, slower request resolving after a later
		// one and clobbering the badge/icon with a stale result.
		var seq = (expressionValidateSeq[targetId] || 0) + 1;
		expressionValidateSeq[targetId] = seq;
		$.ajax({
			dataType: 'json',
			url: 'api/variables?validateExpression=' + encodeURIComponent(value) + (conditionExpr ? '&conditionExpr=true' : ''),
			success: function (data) {
				if (expressionValidateSeq[targetId] !== seq) {
					return;
				}
				if (conditionExpr) {
					RenderExpressionKindBadge(targetId, data);
					return;
				}
				if (!data || !data.valid) {
					$('#' + targetId).html('<i class="fas fa-times text-danger" title="Invalid expression"></i>');
				} else if (data.kind === 'literal') {
					// compile() always "succeeds" on plain text with no '='
					// and no embedded ==...==/%%...%% markers - it's stored
					// verbatim, not evaluated, so a bare variable name here
					// (e.g. "outsideTemp" instead of "=outsideTemp") silently
					// becomes dead literal text. A plain green check would lie
					// about that, so warn instead of confirming.
					$('#' + targetId).html(
						'<i class="fas fa-triangle-exclamation text-warning" title="Stored as literal text, not evaluated - start with &#39;=&#39; to compute a value or reference a variable (e.g. =outsideTemp)"></i>'
					);
				} else {
					$('#' + targetId).html('<i class="fas fa-check text-success" title="Valid expression"></i>');
				}
			},
			error: function () {
				if (expressionValidateSeq[targetId] !== seq) {
					return;
				}
				if (conditionExpr) {
					$('#' + targetId).text('?').removeClass().addClass('badge rounded-pill text-bg-secondary').attr('title', 'Could not validate');
					return;
				}
				$('#' + targetId).html('<i class="fas fa-question text-muted" title="Could not validate"></i>');
			}
		});
	}, 400);
}

// Fills in one "Value [Formula]" / "Value [Invalid]" style label badge -
// see the DebounceValidateExpression comment above for why this exists.
// data.kind is only present when the request was ?conditionExpr=true.
function RenderExpressionKindBadge (badgeId, data) {
	var $badge = $('#' + badgeId);
	var text, danger, title;
	if (!data) {
		text = '?';
		title = 'Could not validate';
	} else if (data.kind === 'variable') {
		text = 'Variable';
		title = 'Matches a real, currently-known variable name';
	} else if (data.kind === 'formula') {
		text = data.valid ? 'Formula' : 'Invalid';
		title = data.valid ? 'A formula that currently compiles' : "Looks like a formula, but doesn't compile (e.g. references an unknown variable)";
	} else if (data.kind === 'literal') {
		text = 'Text';
		title = "Doesn't match any known variable and isn't a formula - will be compared as literal text";
	} else {
		text = data.valid ? '' : 'Invalid';
		title = data.valid ? '' : 'Invalid expression';
	}
	danger = text === 'Invalid' || text === '?';
	$badge.text(text)
		.attr('title', title)
		.removeClass()
		.addClass('badge rounded-pill ' + (danger ? 'text-bg-danger' : 'text-bg-secondary'));
}

// Populates every not-yet-populated .expressionVarHelper div with a
// datalist-backed "insert variable" input - reused across the If Check
// editor's fields and the generic "expression" CommandArg type (e.g. Set
// Variable's Expression mode), rather than custom-built per caller: it
// already fetches and groups all three Variable namespaces (User, read-only
// fpp_ status, read-only mqtt- cache) via FetchConditionNameOptions('Variable',
// ...) and, being a native <datalist>, the browser itself handles matching/
// scrolling even at the thousands of mqtt- entries a busy broker can produce
// - no manual filter box, size cap, or "+N more" bookkeeping required.
var VAR_INSERT_DATALIST_ID = 'variableInsertHelperDatalist';
function PopulateExpressionVarHelpers () {
	var $helpers = $('.expressionVarHelper').not('[data-populated]');
	if (!$helpers.length) {
		return;
	}
	$helpers.attr('data-populated', '1');
	var $datalist = $('#' + VAR_INSERT_DATALIST_ID);
	if (!$datalist.length) {
		$datalist = $("<datalist id='" + VAR_INSERT_DATALIST_ID + "'></datalist>").appendTo('body');
		FetchConditionNameOptions('Variable', function (names) {
			$.each(names, function (i, n) {
				$datalist.append("<option value='" + EscapeHtml(n) + "'>");
			});
		});
	}
	$helpers.each(function () {
		RenderVarInsertHelper($(this), $(this).data('target'), $(this).attr('data-wrap') === 'true');
	});
}

// No separate "Insert" button - typing/picking a name and tabbing or
// pressing Enter (both fire native 'change') inserts it immediately, same
// as the Name field's own datalist input a few hundred lines up never
// needed a "Set" button either. The placeholder text alone explains the
// action, so there's no accompanying label to keep visually tied to the
// right expression field once this sits directly under it. The two-field
// case (both Source == "Expression" and Value are expression-capable at
// once) is handled separately by RenderSharedExpressionValueHelper, which
// needs real DOM measurements to point its arrows at the right fields -
// not a fit for this generic single-target helper.
// shouldWrap distinguishes the two contexts this generic helper is shared
// between:
//  - If Check editor fields (shouldWrap=false, the default): Condition.cpp's
//    EvaluateNameOrExpression() auto-detects a bare real variable name
//    (exact match) or a bare formula referencing one, no wrapping needed -
//    and wrapping here would actively break combining two inserted
//    variables with a math operator (forces string concatenation instead of
//    arithmetic - see RenderSharedExpressionValueHelper's own
//    insertPendingValue for the full explanation).
//  - The generic "expression" CommandArg type, e.g. Set Variable's
//    Expression mode (shouldWrap=true, see data-wrap='true' at this div's
//    creation site): evaluates via a bare ExpressionProcessor::compile()
//    call with no auto-detection at all, so an inserted bare name would
//    just land as literal text unless wrapped.
function RenderVarInsertHelper ($helper, targetId, shouldWrap) {
	$helper.empty();
	var $input = $(
		"<input type='text' list='" +
			VAR_INSERT_DATALIST_ID +
			"' class='form-control form-control-sm w-100 placeholder-center' placeholder='-- Select Variable to Insert --'>"
	).on('change', function () {
		var name = $(this).val();
		if (name) {
			InsertAtCursor(document.getElementById(targetId), shouldWrap ? '%%' + name + '%%' : name);
		}
		$(this).val('');
	});
	// Same visual language as RenderSharedExpressionValueHelper's dual arrows
	// (the Expression Source's Name+Value case) - without it, this plain
	// text box gave no hint that picking a name here does anything, let
	// alone that it lands in the single field directly above. Only one
	// target here (unlike the dual-arrow case), so it's a simple trailing
	// icon, not an absolutely-positioned pointer aimed at a measured field
	// center - but still a real, focusable button rather than decoration
	// only, so it's an equally obvious click target. Trailing (not leading)
	// by request, so the input box itself sits flush left.
	//
	// Clicking the arrow with a name already typed/picked actually inserts
	// it now - previously it only re-focused $input (a no-op if $input was
	// already focused, which it always is right after typing), so clicking
	// the arrow right after typing a name silently did nothing; only Tab/
	// Enter/picking a datalist option (which fire native 'change') actually
	// inserted. Matches RenderSharedExpressionValueHelper's arrows, which
	// have always inserted on click - same focusAtEnd-before-insert fix for
	// the same reason (a never-focused target field's selectionStart is 0,
	// not null, so skipping this would insert at position 0/prepend instead
	// of appending), and same clear-before-insert ordering to avoid
	// double-inserting via the reentrant blur this triggers on $input.
	var $arrow = $(
		"<button type='button' class='buttons reallySmallButton varInsertArrow ms-1' title='Insert into the field above'><i class='fas fa-arrow-up'></i></button>"
	).on('mousedown', function (e) {
		e.preventDefault();
		var name = $input.val();
		if (!name) {
			$input.trigger('focus');
			return;
		}
		$input.val('');
		var targetEl = document.getElementById(targetId);
		var len = targetEl.value.length;
		targetEl.focus();
		targetEl.setSelectionRange(len, len);
		InsertAtCursor(targetEl, shouldWrap ? '%%' + name + '%%' : name);
	});
	$helper.append($("<div class='d-flex align-items-center'></div>").append($input, $arrow));
}

// Inserts text at the current cursor position of a text input (replacing any
// current selection), leaving the cursor positioned right after the insert.
function InsertAtCursor (input, text) {
	if (!input) {
		return;
	}
	var start = input.selectionStart != null ? input.selectionStart : input.value.length;
	var end = input.selectionEnd != null ? input.selectionEnd : input.value.length;
	input.value = input.value.slice(0, start) + text + input.value.slice(end);
	var newPos = start + text.length;
	input.focus();
	input.setSelectionRange(newPos, newPos);
	// Setting .value directly doesn't fire a native input event, so kick the
	// expression validator by hand if this input has one wired up.
	$(input).trigger('input');
}

function CommandArgChanged () {
	$('#playlistEntryCommandOptions').html('');
	CommandSelectChanged(
		'playlistEntryOptions_arg_1',
		'playlistEntryCommandOptions'
	);
}

var allowMultisyncCommands = false;
function OnMultisyncChanged (mscheck, tblCommand) {
	var b = $(mscheck).is(':checked');
	if (b) {
		$('#' + tblCommand + '_multisyncHosts_row').show();
	} else {
		$('#' + tblCommand + '_multisyncHosts_row').hide();
		// Clear the host list so a hidden, stale value isn't saved with the command.
		SetMultisyncHosts(tblCommand, '');
	}
}
// Repopulate one command argument's content list from the remote host(s).
// "datalist" args (e.g. the Preset Name field on "Trigger Command Preset")
// keep their contentListUrl on the <datalist> element (id _arg_x_list) rather
// than on the input itself, so handle both shapes.
function ReloadArgContentList (tblCommand, x, baseUrl) {
	var inp = $('#' + tblCommand + '_arg_' + x);
	if (inp.data('contentlisturl') != null) {
		ReloadContentList(baseUrl, inp);
	} else {
		var list = $('#' + tblCommand + '_arg_' + x + '_list');
		if (list.data('contentlisturl') != null) {
			ReloadContentList(baseUrl, list);
		}
	}
}
function OnMultisyncHostsChanged (hosts, tblCommand) {
	// ReloadContentList() accepts the full comma-separated host list: it queries
	// every selected remote and annotates each value with how many hosts expose
	// it, so forward all checked hosts rather than only the first.
	var baseURL = String($(hosts).val() || '').trim();
	if (baseURL != '') {
		for (var x = 1; x < 20; x++) {
			ReloadArgContentList(tblCommand, x, baseURL);
		}
	}
}

// The Multisync "Hosts" field is a scrollable list of checkboxes (one per
// discovered host) plus a small text box for adding arbitrary hosts. Checkboxes
// are single-tap on touch/"Big Button" kiosk displays and scroll cleanly for
// large device lists, with no jQuery-UI dependency. The saved value is kept as a
// comma-separated string in the hidden #<tblCommand>_multisyncHosts input so the
// save/load paths and the backend are unchanged. See issue #2733.

// Escape a value for safe use inside a single-quoted HTML attribute.
function EscapeMultisyncHost (v) {
	return String(v).replace(/&/g, '&amp;').replace(/'/g, '&#39;').replace(/</g, '&lt;');
}

// Find the checkbox for a host by comparing decoded values (robust to any
// characters, unlike an attribute selector built from escaped HTML).
function FindMultisyncHostCheckbox (container, host) {
	return container.find('input.multisyncHostCheckbox').filter(function () {
		return $(this).val() === host;
	});
}

// Build one checkbox row for the host list.
function MultisyncHostCheckboxHtml (tblCommand, value, label, checked) {
	var v = EscapeMultisyncHost(value);
	return (
		"<label class='d-block text-nowrap fw-normal mb-0 py-1'>" +
		"<input type='checkbox' class='multisyncHostCheckbox me-2'" +
		" value='" + v + "'" + (checked ? ' checked' : '') +
		" onChange='SyncMultisyncHostsFromSelect(\"" + tblCommand + "\");'>" +
		EscapeMultisyncHost(label) + '</label>'
	);
}

// Collect checked hosts, push them into the hidden input (comma string) and
// refresh any arg content lists that populate from the chosen remote (see #2016).
function SyncMultisyncHostsFromSelect (tblCommand) {
	var hosts = [];
	$('#' + tblCommand + '_multisyncHostsSelect input.multisyncHostCheckbox:checked').each(function () {
		hosts.push($(this).val());
	});
	var hidden = $('#' + tblCommand + '_multisyncHosts');
	hidden.val(hosts.join(','));
	OnMultisyncHostsChanged(hidden[0], tblCommand);
}

// Add a manually typed host (e.g. an offline device not in the discovered list)
// as a new checked row, then re-sync.
function AddCustomMultisyncHost (tblCommand) {
	var box = $('#' + tblCommand + '_multisyncHostsCustom');
	var host = (box.val() || '').trim();
	if (host == '') {
		return;
	}
	var container = $('#' + tblCommand + '_multisyncHostsSelect');
	var existing = FindMultisyncHostCheckbox(container, host);
	if (existing.length == 0) {
		container.append(MultisyncHostCheckboxHtml(tblCommand, host, host, true));
	} else {
		existing.prop('checked', true);
	}
	box.val('');
	SyncMultisyncHostsFromSelect(tblCommand);
}

// Reflect a saved comma-separated host string into the checkbox list: check
// matching hosts and append any saved-but-unknown host as a new checked row.
function SetMultisyncHosts (tblCommand, csv) {
	var hidden = $('#' + tblCommand + '_multisyncHosts');
	hidden.val(csv || '');
	var container = $('#' + tblCommand + '_multisyncHostsSelect');
	if (container.length == 0) {
		return;
	}
	var hosts = String(csv || '')
		.split(',')
		.map(function (h) {
			return h.trim();
		})
		.filter(function (h) {
			return h != '';
		});
	container.find('input.multisyncHostCheckbox').prop('checked', false);
	$.each(hosts, function (i, host) {
		var cb = FindMultisyncHostCheckbox(container, host);
		if (cb.length == 0) {
			container.append(MultisyncHostCheckboxHtml(tblCommand, host, host, true));
		} else {
			cb.prop('checked', true);
		}
	});
}

var remoteIpList = null;
function GetRemotes () {
	if (remoteIpList == null) {
		$.ajax({
			dataType: 'json',
			async: false,
			url: 'api/remotes',
			success: function (data) {
				remoteIpList = data;
			}
		});
	}
	return remoteIpList;
}
// Bootstrap tooltips append their popup <div> to <body> and track their
// instance outside jQuery's data/event bookkeeping - a plain jQuery .remove()
// on an .argHelpIcon's row leaks both the instance and (if the tooltip
// happened to be showing) an orphaned popup that nothing will ever hide
// again, since its trigger element is gone. Must dispose() before removing.
function DisposeArgHelpTooltips (root) {
	if (typeof bootstrap === 'undefined' || !bootstrap.Tooltip) {
		return;
	}
	root.find('.argHelpIcon').each(function () {
		var existing = bootstrap.Tooltip.getInstance(this);
		if (existing) {
			existing.dispose();
		}
	});
}

function CommandSelectChanged (
	commandSelect,
	tblCommand,
	configAdjustable = false,
	argPrintFunc = PrintArgInputs
) {
	DisposeArgHelpTooltips($('#' + tblCommand));
	for (var x = 1; x < 25; x++) {
		$('#' + tblCommand + '_arg_' + x + '_row').remove();
	}
	$('#' + tblCommand + '_multisync_row').remove();
	$('#' + tblCommand + '_multisyncHosts_row').remove();
	$('#' + tblCommand + '_description_row').remove();
	// Section headings (e.g. If's "When Check is True"/"When Check is False")
	// have no numbered _arg_N_row id, so the removal loop above never catches
	// them - a stale one from a previous render (e.g. the command picker's
	// initial default before the real command is applied) would otherwise be
	// left orphaned above the freshly re-rendered rows.
	$('#' + tblCommand + ' tr.argSectionHeaderRow').remove();
	var command = $('#' + commandSelect).val();
	if (typeof command == 'undefined' || command == null) {
		return;
	}
	var co = commandList.find(function (element) {
		return element['name'] == command;
	});
	if (typeof co == 'undefined' || co == null) {
		$.ajax({
			dataType: 'json',
			async: false,
			url: 'api/commands/' + command,
			success: function (data) {
				co = data;
			}
		});
	}
	if (co.hasOwnProperty('description')) {
		var line =
			"<tr id='" +
			tblCommand +
			"_description_row' ><td></td><td>" +
			co['description'] +
			'</td></tr>';
		$('#' + tblCommand).append(line);
	}

	var line = "<tr id='" + tblCommand + "_multisync_row' ";
	if (!allowMultisyncCommands || command == '' || co['disallowMultisync']) {
		line += "style='display:none'";
	}
	line +=
		'><td>Multisync:' +
		ArgHelpIcon('Send this command to multiple FPP instances') +
		"</td><td><input type='checkbox' id='" +
		tblCommand +
		"_multisync' class='arg_multisync' onChange='OnMultisyncChanged(this, \"" +
		tblCommand +
		'");\'></input></td></tr>';
	$('#' + tblCommand).append(line);
	InitArgHelpTooltips();
	// Hosts row: a scrollable checkbox list (single-tap, touch/kiosk friendly,
	// no jQuery-UI needed) plus a text box to add an arbitrary host. A hidden
	// input keeps the comma-separated value that the save/load paths and the
	// backend expect. See issue #2733.
	line =
		"<tr id='" +
		tblCommand +
		"_multisyncHosts_row' style='display:none'><td>Hosts:</td><td>" +
		"<input type='hidden' id='" +
		tblCommand +
		"_multisyncHosts' class='arg_multisyncHosts'>";
	line +=
		"<div class='checkboxSelectList border rounded overflow-auto px-2 py-1' id='" +
		tblCommand +
		"_multisyncHostsSelect'>";
	remotes = GetRemotes();
	$.each(remotes, function (k, v) {
		line += MultisyncHostCheckboxHtml(tblCommand, k, v, false);
	});
	line += '</div>';
	line +=
		"<div class='d-flex gap-1 mt-1'><input type='text' class='flex-fill' id='" +
		tblCommand +
		"_multisyncHostsCustom' placeholder='Add host…'" +
		" onkeydown='if(event.keyCode==13){event.preventDefault();AddCustomMultisyncHost(\"" +
		tblCommand +
		"\");}'>";
	line +=
		"<input type='button' class='buttons' value='Add' onclick='AddCustomMultisyncHost(\"" +
		tblCommand +
		"\");'></div>";
	line += '</td></tr>';

	$('#' + tblCommand).append(line);

	argPrintFunc(tblCommand, configAdjustable, co['args']);

	// The command's own args have just been appended below the entry's rows -
	// put the Entry Properties block back at the bottom of the form.
	if (tblCommand == 'playlistEntryCommandOptions') {
		MoveEntryPropertiesToBottom();
	}
}

// The playlist entry editor renders an entry's own args (#playlistEntryOptions)
// and, for an "FPP Command" entry, the selected command's args
// (#playlistEntryCommandOptions) into two separate <tbody>s of one table, so
// the command's args always land BELOW the entry's own rows. The Entry
// Properties block (Note / Display Mode / Time Code) belongs at the bottom of
// the form for every entry type, so move it into a third, trailing tbody. It
// gets its own rather than being parked in the command one because
// CommandArgChanged() empties that tbody every time the command changes.
// Reading the form back stays correct because PlaylistEntryArgScope() (used by
// GetPlaylistEntry) covers both entry tbodies.
function MoveEntryPropertiesToBottom () {
	var $tail = $('#playlistEntryPropertiesOptions');
	if (!$tail.length) {
		return;
	}
	// Both the heading row and its arg rows carry data-arg-section, so the
	// whole block is found wherever it currently sits.
	var $rows = PlaylistEntryArgScope().find(
		"tr[data-arg-section='Entry Properties']"
	);
	if (!$rows.length) {
		return;
	}
	$tail.append($rows);
}

// Where an entry's own arg inputs live: the main tbody plus the trailing one
// holding the relocated Entry Properties block. Deliberately excludes
// #playlistEntryCommandOptions - a selected command's args are read separately
// (CommandToJSON) and could otherwise shadow an entry arg of the same name.
function PlaylistEntryArgScope () {
	return $('#playlistEntryOptions, #playlistEntryPropertiesOptions');
}

function SubCommandChanged (
	subCommandV,
	configAdjustable = false,
	argPrintFunc = PrintArgInputs
) {
	var subCommand = $(subCommandV);
	if (typeof subCommandV === 'string') {
		subCommand = $('#' + subCommandV);
	}
	var val = subCommand.val();
	var url = subCommand.data('url');
	if (url == null) {
		url = subCommand.data('contentlisturl');
	}
	var count = subCommand.data('count');
	var tblCommand = subCommand.data('tblcommand');

	for (var x = count + 1; x < 25; x++) {
		DisposeArgHelpTooltips($('#' + tblCommand + '_arg_' + x + '_row'));
		$('#' + tblCommand + '_arg_' + x + '_row').remove();
	}
	$.ajax({
		dataType: 'json',
		async: false,
		url: url + val,
		success: function (data) {
			argPrintFunc(tblCommand, false, data['args'], count + 1);
		}
	});
}

// Overlay-model command pickers (api/models?simple=true...) list only top-level
// models by default. FPP resolves xLights submodels and model groups by name
// too, so append submodels=true to have the endpoint include them in the
// suggestion list. Idempotent; leaves non-model content lists untouched.
function OverlayModelContentListUrl (url) {
	var u = String(url || '');
	if (/api\/models\b/.test(u) && /simple=true/.test(u) && !/submodels=true/.test(u)) {
		return u + '&submodels=true';
	}
	return u;
}

// A row's description repeated across 2+ CONSECUTIVE args (e.g. the If
// command's "Then Run" on both thenCommands and thenMode) marks them as one
// logical group - only THOSE boundaries get a divider. Naively drawing one
// on every "description changed from the previous row" would fire on
// virtually every row of any ordinary multi-arg command, since two adjacent
// args almost always have two different, unique labels (e.g. GPIO's "Pin"
// then "Action") without being a "group" at all. Pass the full args array in
// registration order (before any hidden/statusOnly filtering - this filters
// the same way internally) to get, for each arg, whether drawing a divider
// immediately before it is warranted.
function ComputeArgGroupBoundaries (args) {
	// args is an array for generic Command args (commandEditor.php, GPIO's
	// command picker, etc.) but a plain object keyed by arg name for
	// playlistEntryTypes.json's per-entry-type args (see PlaylistTypeChanged) -
	// PrintArgInputs' own render loop uses $.each() so it tolerates both, but
	// .filter()/.map() below need a real array.
	// A command with zero registered args (e.g. "All Lights Off") has no
	// "args" key at all in api/commands' JSON (Commands.cpp only creates
	// cmd["args"] once it appends at least one entry), so args can be
	// undefined here - treat that the same as an empty arg list.
	var argList = Array.isArray(args) ? args : Object.keys(args || {}).map(function (k) {
		return args[k];
	});
	var included = argList.filter(function (val) {
		if (val['type'] == 'args') return false;
		if (val.hasOwnProperty('statusOnly') && val.statusOnly == true) return false;
		if (val.hasOwnProperty('hidden') && val.hidden == true) return false;
		return true;
	});
	var isGrouped = included.map(function (val, i) {
		var prevDesc = i > 0 ? included[i - 1]['description'] : null;
		var nextDesc = i < included.length - 1 ? included[i + 1]['description'] : null;
		return val['description'] === prevDesc || val['description'] === nextDesc;
	});
	var boundaries = included.map(function (val, i) {
		if (i === 0) return false;
		var descChanged = val['description'] !== included[i - 1]['description'];
		return descChanged && (isGrouped[i] || isGrouped[i - 1]);
	});
	return { included: included, boundaries: boundaries };
}

function PrintArgsInputsForEditable (
	tblCommand,
	configAdjustable,
	args,
	startCount = 1
) {
	var count = startCount;
	var initFuncs = [];
	var haveTime = 0;
	var haveDate = 0;
	var children = [];
	var argGroups = ComputeArgGroupBoundaries(args);
	var groupIndex = 0;

	//    $.each( args,
	var valFunc = function (key, val) {
		if (val['type'] == 'args') {
			return;
		}

		if (val.hasOwnProperty('statusOnly') && val.statusOnly == true) {
			return;
		}
		if (val.hasOwnProperty('hidden') && val.hidden == true) {
			return;
		}
		var ID = tblCommand + '_arg_' + count;
		var isNewGroup = argGroups.boundaries[groupIndex];
		groupIndex++;
		var line =
			"<tr id='" + ID + "_row' class='arg_row_" + val['name'] + (isNewGroup ? ' argRowGroupStart' : '') + "'><td>";
		var subCommandInitFunc = null;
		if (children.includes(val['name']))
			line += '&nbsp;&nbsp;&nbsp;&nbsp;&bull;&nbsp;';

		var typeName = val['type'];
		if (typeName == 'datalist') {
			typeName = 'string';
		}

		var dv = '';
		if (typeof val['default'] != 'undefined') {
			dv = val['default'];
		}
		var contentListPostfix = '';
		if (val['type'] == 'subcommand') {
			line += val['description'] + ':</td><td>';
			line +=
				"<select class='playlistDetailsSelect arg_" +
				val['name'] +
				"' name='parent_" +
				val['name'] +
				"' id='" +
				ID +
				"'";
			line +=
				" onChange='SubCommandChanged(this, " +
				configAdjustable +
				", PrintArgsInputsForEditable)'";
			line += " data-url='" + val['contentListUrl'] + "'";
			line += " data-count='" + count + "'";
			line += " data-tblcommand='" + tblCommand + "'";
			line += " data-arg-type='subcommand'";
			line += '>';
			subCommandInitFunc = function () {
				SubCommandChanged(ID, configAdjustable, PrintArgsInputsForEditable);
			};
			$.each(val['contents'], function (key, v) {
				line += '<option value="' + v + '"';
				if (v == dv) {
					line += ' selected';
				}

				if (Array.isArray(val['contents'])) line += '>' + v + '</option>';
				else line += '>' + key + '</option>';
			});
			line += '</select>';
		} else {
			line += val['description'] + ' (' + typeName + '):</td><td>';
			line +=
				"<input class='arg_" +
				val['name'] +
				"' id='" +
				ID +
				"' type='text' size='40' maxlength='200' data-arg-type='" +
				typeName +
				"' ";
			if (
				val['type'] == 'datalist' ||
				typeof val['contentListUrl'] != 'undefined' ||
				typeof val['contents'] != 'undefined'
			) {
				line += " list='" + ID + "_list' value='" + dv + "'";
			} else if (val['type'] == 'bool') {
				if (dv == 'true' || dv == '1') {
					line += " value='true'";
				} else {
					line += " value='false'";
				}
			} else if (val['type'] == 'time') {
				line += " value='00:00:00'";
			} else if (val['type'] == 'date') {
				line += " value='2020-12-25'";
			} else if (val['type'] == 'int' || val['type'] == 'float') {
				if (dv != '') {
					line += " value='" + dv + "'";
				} else if (typeof val['min'] != 'undefined') {
					line += " value='" + val['min'] + "'";
				}
			} else if (dv != '') {
				line += " value='" + dv + "'";
			}
			line += '>';
			if (val['type'] == 'int' || val['type'] == 'float') {
				if (typeof val['unit'] === 'string') {
					line += ' ' + val['unit'];
				}
			}
			line += '</input>';
			if (
				val['type'] == 'datalist' ||
				typeof val['contentListUrl'] != 'undefined' ||
				typeof val['contents'] != 'undefined'
			) {
				line += "<datalist id='" + ID + "_list'>";
				$.each(val['contents'], function (key, v) {
					line += '<option value="' + v + '"';
					line += '>' + v + '</option>';
				});
				line += '</datalist>';
				contentListPostfix = '_list';
			}
		}

		line += '</td></tr>';
		$('#' + tblCommand).append(line);
		if (typeof val['contentListUrl'] != 'undefined') {
			var selId = '#' + tblCommand + '_arg_' + count + contentListPostfix;
			$.ajax({
				dataType: 'json',
				url: OverlayModelContentListUrl(val['contentListUrl']),
				async: false,
				success: function (data) {
					if (Array.isArray(data)) {
						$.each(data, function (key, v) {
							var line = '<option value="' + v + '"';
							if (v == dv) {
								line += ' selected';
							}
							line += '>' + v + '</option>';
							$(selId).append(line);
						});
					} else {
						$.each(data, function (key, v) {
							var line = '<option value="' + key + '"';
							if (key == dv) {
								line += ' selected';
							}
							line += '>' + v + '</option>';
							$(selId).append(line);
						});
					}
				}
			});
		}
		if (subCommandInitFunc != null) {
			subCommandInitFunc();
		}
		count = count + 1;
	};
	$.each(args, valFunc);
}

// The standard FPP help affordance, as emitted by PrintToolTip() in
// common.php for settings pages (networkconfig.php et al): the help-icon.svg
// glyph wrapped in a Bootstrap tooltip trigger. Used by the generic per-arg
// renderers so a Command/playlist-entry arg's "help" text looks and behaves
// exactly like a setting's tooltip rather than being its own one-off style.
// Args tables are built as HTML strings and injected long after page load, so
// the markup carries data-bs-title (not title - SetupToolTips' page-load sweep
// never sees these elements) and InitArgHelpTooltips() does the wiring.
function ArgHelpIcon (text) {
	if (typeof text !== 'string' || text === '') {
		return '';
	}
	var t = text
		.replace(/&/g, '&amp;')
		.replace(/</g, '&lt;')
		.replace(/'/g, '&apos;');
	return (
		" <span class='argHelpIcon' data-bs-toggle='tooltip' data-bs-html='true' data-bs-placement='auto' data-bs-title='" +
		t +
		"'><img src='images/redesign/help-icon.svg' class='icon-help' alt='Help icon'></span>"
	);
}

// Activates Bootstrap tooltips for any CommandArg's "help" text. Disposes any
// existing instance first (same pattern as networkconfig.php's
// initializeTooltips) since the args table is re-rendered in place whenever
// the command / playlist entry type changes.
function InitArgHelpTooltips () {
	if (typeof bootstrap === 'undefined' || !bootstrap.Tooltip) {
		return;
	}
	$('.argHelpIcon').each(function () {
		var existing = bootstrap.Tooltip.getInstance(this);
		if (existing) {
			existing.dispose();
		}
		try {
			new bootstrap.Tooltip(this);
		} catch (e) {
			console.warn('Skipping invalid arg help tooltip', this, e);
		}
	});
}

function PrintArgInputs (tblCommand, configAdjustable, args, startCount = 1) {
	var count = startCount;
	var initFuncs = [];
	var haveTime = 0;
	var haveDate = 0;
	var children = [];
	var timeOptions = new Map();
	var argGroups = ComputeArgGroupBoundaries(args);
	var groupIndex = 0;
	var currentSection = null;
	var sectionRowCounts = {};
	// Args listed as a "children" value of some OTHER arg start hidden unless
	// that parent arg's default value is the one that reveals them - keeps
	// initial render consistent with UpdateGenericArgChildVisibility(), which
	// takes over once the user changes the parent dropdown. A child name can
	// legitimately appear under several sibling values (e.g. If's "name" is
	// listed for every Check source except Time) - it must only be hidden
	// when it's absent from the CURRENT default's own list, not merely
	// because some other, non-matching value's list happens to include it.
	var initiallyHiddenChildren = {};
	$.each(args, function (key, val) {
		if (typeof val['children'] !== 'object') return;
		var parentDefault = typeof val['default'] != 'undefined' ? val['default'] : '';
		var shownForDefault = val['children'][parentDefault] || [];
		var allChildNames = [];
		$.each(val['children'], function (forValue, names) {
			$.each(names, function (i, name) {
				if (allChildNames.indexOf(name) === -1) {
					allChildNames.push(name);
				}
			});
		});
		$.each(allChildNames, function (i, name) {
			if (shownForDefault.indexOf(name) === -1) {
				initiallyHiddenChildren[name] = true;
			}
		});
	});

	$.each(args, function (key, val) {
		if (val['type'] == 'args') return;

		if (val.hasOwnProperty('statusOnly') && val.statusOnly == true) {
			return;
		}
		if (val.hasOwnProperty('hidden') && val.hidden == true) {
			return;
		}

		var advancedHidden =
			val.hasOwnProperty('advanced') &&
			val.advanced == true &&
			settings['uiLevel'] < 1;
		var rowStyle = '';
		if (advancedHidden || initiallyHiddenChildren[val['name']]) {
			rowStyle = " style='display:hidden; visibility:collapse'";
		}

		// An arg's optional "section" groups consecutive args under a named
		// heading (e.g. the playlist entry editor's Primary Media / Extra
		// Media / Entry Properties) instead of one flat run of rows. Purely
		// presentational - the heading is a row of its own, so nothing about
		// how args are read back (by .arg_<name> class / _arg_N id) changes.
		var section = typeof val['section'] === 'string' ? val['section'] : '';
		var sectionHeader = '';
		if (section !== '' && section !== currentSection) {
			currentSection = section;
			var esc = section
				.replace(/&/g, '&amp;')
				.replace(/</g, '&lt;')
				.replace(/'/g, '&apos;');
			sectionHeader =
				"<tr class='argSectionHeaderRow' data-arg-section='" +
				esc +
				"'><td colspan='2' class='pt-3 pb-1'><div class='fw-bold text-uppercase small text-secondary border-bottom pb-1'>" +
				esc +
				'</div></td></tr>';
		}
		if (section !== '') {
			// A section that ends up with no visible rows at all (e.g. Extra
			// Media, whose args are all advanced:true, at UI level Basic)
			// must not leave a dangling heading behind - see the post-render
			// cleanup after this loop.
			if (!sectionRowCounts.hasOwnProperty(section)) {
				sectionRowCounts[section] = 0;
			}
			if (!advancedHidden) {
				// A row hidden only because its parent arg's current value
				// doesn't reveal it still counts - UpdateChildVisibility can
				// show it again without a re-render.
				sectionRowCounts[section]++;
			}
		}

		var ID = tblCommand + '_arg_' + count;
		// A heading already separates this row from what came before, so the
		// generic group divider on the same row would just double it up.
		var isNewGroup = argGroups.boundaries[groupIndex] && sectionHeader === '';
		groupIndex++;
		var line =
			"<tr id='" +
			ID +
			"_row' class='arg_row_" +
			val['name'] +
			(isNewGroup ? ' argRowGroupStart' : '') +
			"'" +
			// Tags the row with the section it belongs to so a whole section can
			// be found (and, for Entry Properties, repositioned) later without
			// relying on where it currently sits in the table.
			(section !== ''
				? " data-arg-section='" +
					section.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/'/g, '&apos;') +
					"'"
				: '') +
			rowStyle +
			'>' +
			// commandlist's content column stacks a taller block (rows/placeholder
			// plus the Add Command button) below the label's single line of text -
			// default middle valign centers the label against that whole stack
			// instead of its top line, so it visibly drifts away from "Then Run:"/
			// "Otherwise Run:". Pin it to the top so it lines up with the first line.
			(val['type'] == 'commandlist' ? "<td class='align-top'>" : '<td>');
		var subCommandInitFunc = null;

		if (children.includes(val['name']))
			line += '&nbsp;&nbsp;&nbsp;&nbsp;&bull;&nbsp;';

		// A toggleStyle arg (see below) shows its own inline "Order:" label
		// next to the toggle itself (matching the condition-tree's own
		// "Match:" toggle), not a separate left-column description - it sits
		// directly above the row it modifies (e.g. Then Run's command list),
		// so a second copy of that row's own label here would just repeat it.
		if (!val['toggleStyle']) {
			line += val['description'] + ':';
			var helpIcon = ArgHelpIcon(val['help']);
			if (helpIcon !== '') {
				line += helpIcon;
				initFuncs.push('InitArgHelpTooltips');
			}
		}
		line += '</td><td>';

		var dv = '';
		if (typeof val['default'] != 'undefined') {
			dv = val['default'];
		}
		var contentListPostfix = '';
		if (
			val['type'] == 'string' ||
			val['type'] == 'file' ||
			val['type'] == 'multistring' ||
			val['type'] == 'expression'
		) {
			if (typeof val['init'] === 'string') {
				initFuncs.push(val['init']);
			}

			if (typeof val['contents'] !== 'undefined' && val['toggleStyle'] && Array.isArray(val['contents']) && val['contents'].length === 2) {
				// Opt-in 2-option toggle (e.g. the If command's Sequential/
				// Parallel mode, matching its own Match ALL/ANY condition
				// toggle) instead of a <select>. CommandToJSON/
				// PopulateExistingCommand both read/write a single element
				// addressed by id="ID" via .val() - so the toggle's real
				// value lives in a hidden mirror input with that id (same
				// "hidden field is the source of truth" pattern already used
				// for conditionListRoot/commandListRoot), and the visible
				// btn-check pair only drives that hidden field. A delegated
				// change listener (see toggleStyleRoot below) keeps a
				// radio's checked state in sync whenever the hidden field's
				// value changes from either direction (a user's click, or
				// PopulateExistingCommand's inp.val(v).change() on load).
				var opt0 = val['contents'][0];
				var opt1 = val['contents'][1];
				var toggleLabel = (typeof val['toggleLabel'] === 'string' && val['toggleLabel'] !== '') ? val['toggleLabel'] : val['description'];
				var toggleHelp = ArgHelpIcon(val['help']);
				if (toggleHelp !== '') {
					initFuncs.push('InitArgHelpTooltips');
				}
				line +=
					"<input type='hidden' id='" + ID + "' class='arg_" + val['name'] + " toggleStyleRoot' value='" + dv + "'>" +
					"<div class='d-flex align-items-center gap-2'>" +
					"<span class='text-muted'>" + toggleLabel + ':' + toggleHelp + '</span>' +
					"<div class='btn-group btn-group-sm toggleStyleGroup' role='group'>" +
					"<input type='radio' class='btn-check' name='" + ID + "_toggle' id='" + ID + "_opt0' autocomplete='off'" +
					(dv === opt0 ? ' checked' : '') + " value='" + opt0 + "'>" +
					"<label class='btn btn-outline-secondary' for='" + ID + "_opt0'>" + opt0 + '</label>' +
					"<input type='radio' class='btn-check' name='" + ID + "_toggle' id='" + ID + "_opt1' autocomplete='off'" +
					(dv === opt1 ? ' checked' : '') + " value='" + opt1 + "'>" +
					"<label class='btn btn-outline-secondary' for='" + ID + "_opt1'>" + opt1 + '</label>' +
					'</div>' +
					'</div>';
			} else if (typeof val['contents'] !== 'undefined') {
				line +=
					"<select class='playlistDetailsSelect arg_" +
					val['name'] +
					"' name='parent_" +
					val['name'] +
					"' id='" +
					ID +
					"'";
				if (typeof val['contentListUrl'] != 'undefined') {
					line += " data-contentlisturl='" + OverlayModelContentListUrl(val['contentListUrl']) + "'";
				}
				if (val['type'] == 'multistring') {
					line += " multiple data-multistring='1'";
				}

				if (typeof val['children'] === 'object') {
					if (
						tblCommand == 'playlistEntryCommandOptions' ||
						tblCommand == 'playlistEntryOptions'
					) {
						line += " onChange='UpdateChildVisibility();";
					} else {
						// Generic Command-args tables (commandEditor.php, GPIO's
						// command picker, etc.) - not driven by the global
						// playlistEntryTypes structure UpdateChildVisibility()
						// reads, so ship the children map on the element itself.
						line +=
							" data-children='" +
							JSON.stringify(val['children']).replace(/'/g, '&apos;') +
							"' onChange='UpdateGenericArgChildVisibility(this);";
					}
					if (typeof val['onChange'] === 'string') {
						line += ' ' + val['onChange'] + '();';
						initFuncs.push(val['onChange']);
					}

					line += "'";

					var ckeys = Object.keys(val['children']);
					for (var c = 0; c < ckeys.length; c++) {
						for (var x = 0; x < val['children'][ckeys[c]].length; x++) {
							children.push(val['children'][ckeys[c]][x]);
						}
					}
				} else {
					if (typeof val['onChange'] === 'string') {
						line += " onChange='" + val['onChange'] + "();'";
						initFuncs.push(val['onChange']);
					}
				}

				line += '>';
				$.each(val['contents'], function (key, v) {
					line += '<option value="' + v + '"';
					if (v == dv) {
						line += ' selected';
					}

					if (Array.isArray(val['contents'])) line += '>' + v + '</option>';
					else line += '>' + key + '</option>';
				});
				line += '</select>';
			} else if (
				typeof val['contentListUrl'] == 'undefined' &&
				typeof val['init'] == 'undefined'
			) {
				line +=
					"<input class='arg_" +
					val['name'] +
					"' id='" +
					ID +
					"' type='text' size='40' maxlength='200' value='" +
					dv +
					"'";

				if (typeof val['placeholder'] === 'string') {
					line += " placeholder='" + val['placeholder'] + "'";
				}

				if (val['type'] == 'expression') {
					line +=
						" oninput='DebounceValidateExpression(this, \"" +
						ID +
						"_validIcon\");'";
				}

				line += '></input>';
				if (val['type'] == 'expression') {
					// Live valid/invalid indicator, debounced so it doesn't
					// fire an API call on every keystroke.
					line += " <span id='" + ID + "_validIcon' class='expressionValidIcon'></span>";
					// Click-to-insert variable name helper - simpler than true
					// inline autocomplete (no existing FPP pattern for that),
					// populated once after render by PopulateExpressionVarHelpers().
					// data-wrap='true': this generic "expression" CommandArg
					// type (e.g. Set Variable's Expression mode) evaluates via
					// a bare ExpressionProcessor::compile() call with no
					// "=" prefix typed by default - unlike the If Check
					// editor's fields, it does NOT auto-detect a bare
					// variable name/formula (that's Condition.cpp's
					// EvaluateNameOrExpression, specific to If), so an
					// inserted name here needs "%%...%%" wrapping to actually
					// take effect instead of landing as literal text.
					line +=
						"<div class='expressionVarHelper text-muted small' data-target='" +
						ID +
						"' data-wrap='true'></div>";
					initFuncs.push('PopulateExpressionVarHelpers');
				}
				if (configAdjustable && val['adjustable']) {
					line +=
						"&nbsp;<input type='checkbox' id='" +
						ID +
						"_adjustable' class='arg_" +
						val['name'] +
						"'>Adjustable</input>";
				}

				if (val['type'] == 'file') {
					line +=
						"&nbsp;<input type='button' value='Choose File' onclick='FileChooser(" +
						'"' +
						val['directory'] +
						'",".arg_' +
						val['name'] +
						'"' +
						");' class='buttons'>";
				}
			} else {
				// Has a contentListUrl OR a init script
				line +=
					"<select class='playlistDetailsSelect arg_" +
					val['name'] +
					"' id='" +
					ID +
					"'";
				if (val['type'] == 'multistring') {
					line += " multiple data-multistring='1'";
				}
				if (typeof val['contentListUrl'] != 'undefined') {
					line += " data-contentlisturl='" + OverlayModelContentListUrl(val['contentListUrl']) + "'";
				}
				if (val['allowBlanks']) {
					line += " data-allowblanks='true'";
					// An unlabelled blank option reads as an empty row in the
					// list; blankLabel names what picking it means (e.g.
					// "-- None --" for an optional file). Carried on the element
					// so ReloadContentList can rebuild the same placeholder.
					if (typeof val['blankLabel'] === 'string' && val['blankLabel'] !== '') {
						line +=
							" data-blanklabel='" +
							val['blankLabel'].replace(/'/g, '&apos;') +
							"'";
					}
				}

				if (typeof val['children'] === 'object') {
					if (tblCommand == 'playlistEntryCommandOptions')
						line += " onChange='UpdateChildVisibility();";
					if (typeof val['onChange'] === 'string') {
						line += ' ' + val['onChange'] + '();';
						initFuncs.push(val['onChange']);
					}

					line += "'";

					var ckeys = Object.keys(val['children']);
					for (var c = 0; c < ckeys.length; c++) {
						for (var x = 0; x < val['children'][ckeys[c]].length; x++) {
							children.push(val['children'][ckeys[c]][x]);
						}
					}
				} else {
					if (typeof val['onChange'] === 'string') {
						line += " onChange='" + val['onChange'] + "();'";
						initFuncs.push(val['onChange']);
					}
				}

				line += '>';
				if (val['allowBlanks']) {
					line +=
						"<option value=''>" +
						(typeof val['blankLabel'] === 'string'
							? val['blankLabel'].replace(/&/g, '&amp;').replace(/</g, '&lt;')
							: '') +
						'</option>';
				}
				line += '</select>';
			}
		} else if (val['type'] == 'commandlist') {
			// A repeatable, user-growable list of {command, args} entries -
			// ported from gpio.php's GPIO-edge command list (renderModalList
			// et al): one-line summaries, Edit opens the existing
			// ShowCommandEditor modal, Up/Down/Delete manage an in-memory
			// array (commandListState), synced into this hidden field.
			line +=
				"<input type='hidden' id='" +
				ID +
				"' class='arg_" +
				val['name'] +
				" commandListRoot' value='" +
				(dv ? dv.replace(/'/g, '&apos;') : '[]') +
				"'>";
			line += "<div id='" + ID + "_container' class='commandListContainer'></div>";
			line +=
				"<input type='button' class='buttons btn-sm' value='+ Add Command' onclick='AddCommandListRow(\"" +
				ID +
				"\");'>";
			initFuncs.push('InitCommandLists');
		} else if (val['type'] == 'conditionlist') {
			// The If command's compound Rules field - an AND/OR tree of
			// conditions and nested groups, rendered fully inline/recursively
			// (see RenderConditionTree) rather than behind a chain of modals,
			// plus a live plain-English formula preview above it.
			line +=
				"<div id='" + ID + "_formula' class='conditionFormulaPreview small text-muted mb-2 fst-italic'></div>";
			line +=
				"<input type='hidden' id='" +
				ID +
				"' class='arg_" +
				val['name'] +
				" conditionListRoot' value='" +
				(dv ? dv.replace(/'/g, '&apos;') : '') +
				"'>";
			line += "<div id='" + ID + "_container' class='conditionListContainer'></div>";
			initFuncs.push('InitConditionLists');
		} else if (val['type'] == 'datalist') {
			line +=
				"<input class='arg_" +
				val['name'] +
				"' id='" +
				ID +
				"' type='text' size='40' maxlength='200' value='" +
				dv +
				"' list='" +
				ID +
				"_list'></input>";
			line += "<datalist id='" + ID + "_list'";
			if (typeof val['contentListUrl'] != 'undefined') {
				line += " data-contentlisturl='" + OverlayModelContentListUrl(val['contentListUrl']) + "'";
			}
			line += '>';
			$.each(val['contents'], function (key, v) {
				line += '<option value="' + v + '"';
				line += '>' + v + '</option>';
			});
			line += '</datalist>';
			contentListPostfix = '_list';
		} else if (val['type'] == 'bool') {
			line +=
				"<input type='checkbox' class='arg_" +
				val['name'] +
				"' id='" +
				ID +
				"' value='true'";
			if (dv == 'true' || dv == '1') {
				line += ' checked';
			}
			line += '></input>';
		} else if (val['type'] == 'color') {
			line +=
				"<input type='color' class='color-box fppCommandColor arg_" +
				val['name'] +
				"' id='" +
				ID +
				"' value='" +
				dv +
				"' style='background-color: " +
				dv +
				";'></input>";
		} else if (val['type'] == 'time') {
			haveTime = 1;
			line +=
				"<input class='time center arg_" +
				val['name'] +
				"' id='" +
				ID +
				"' type='text' size='8' value='";
			if (val.hasOwnProperty('default')) {
				line += val['default'];
			} else {
				line += '00:00:00';
			}
			line += "'/>";
			if (val.hasOwnProperty('extraOptions')) {
				timeOptions.set(ID, val['extraOptions']);
			}
		} else if (val['type'] == 'date') {
			haveDate = 1;
			line +=
				"<input class='date center arg_" +
				val['name'] +
				"' id='" +
				ID +
				"' type='text' size='10' value='2020-01-01'/>";
		} else if (val['type'] == 'range') {
			line += '<script>';
			line += 'function ' + ID + 'RangeChanged(val) {';
			line += "    $('#" + ID + "CurrentValue').html(val);";
			line += '}';
			line += '</script>';
			line +=
				'<span>' +
				val['min'] +
				"<input type='range' class='arg_" +
				val['name'] +
				" cmdArgSlider' id='" +
				ID +
				"' min='" +
				val['min'] +
				"' max='" +
				val['max'] +
				"'";
			var vl = "&nbsp;(<span id='" + ID + "CurrentValue'>";
			if (dv != '') {
				line += " value='" + dv + "'";
				vl += dv;
			} else if (typeof val['min'] != 'undefined') {
				line += " value='" + val['min'] + "'";
				vl += val['min'];
			}
			line += " oninput='" + ID + "RangeChanged(this.value)'";
			line += " onchange='" + ID + "RangeChanged(this.value)'";
			vl += '</span>)';
			line += '></input>' + val['max'] + vl + '</span>';
		} else if (val['type'] == 'int' || val['type'] == 'float') {
			line +=
				"<input type='number' class='arg_" +
				val['name'] +
				"' id='" +
				ID +
				"' min='" +
				val['min'] +
				"' max='" +
				val['max'] +
				"'";
			if (dv != '') {
				line += " value='" + dv + "'";
			} else if (typeof val['min'] != 'undefined') {
				line += " value='" + val['min'] + "'";
			}
			if (typeof val['step'] === 'number') {
				line += " step='" + val['step'] + "'";
			}
			line += '></input>';

			if (typeof val['unit'] === 'string') {
				line += ' ' + val['unit'];
			}
			if (configAdjustable && val['adjustable']) {
				line +=
					"&nbsp;<input type='checkbox' id='" +
					ID +
					"_adjustable' class='arg_" +
					val['name'] +
					"'>Adjustable</input>";
			}
		} else if (val['type'] == 'subcommand') {
			line +=
				"<select class='playlistDetailsSelect arg_" +
				val['name'] +
				"' name='parent_" +
				val['name'] +
				"' id='" +
				ID +
				"'";
			line += " onChange='SubCommandChanged(this, " + configAdjustable + ")'";
			line += " data-url='" + val['contentListUrl'] + "'";
			line += " data-count='" + count + "'";
			line += " data-tblcommand='" + tblCommand + "'";
			line += '>';
			subCommandInitFunc = function () {
				SubCommandChanged(ID, configAdjustable);
			};
			$.each(val['contents'], function (key, v) {
				line += '<option value="' + v + '"';
				if (v == dv) {
					line += ' selected';
				}

				if (Array.isArray(val['contents'])) line += '>' + v + '</option>';
				else line += '>' + key + '</option>';
			});
			line += '</select>';
		}

		// A multistring is a native <select multiple>; overlay a scrollable
		// checkbox list (single-tap, touch/kiosk friendly) that mirrors it. The
		// <select> stays the source of truth for save/load/content-reload. #2733
		if (val['type'] == 'multistring') {
			line +=
				"<div id='" +
				ID +
				"_checks' class='checkboxSelectList border rounded overflow-auto px-2 py-1'></div>";
		}

		line += '</td></tr>';
		// A toggleStyle arg renders directly above the row it modifies (e.g.
		// Then Run's command-list row, registered immediately before this
		// one) rather than appended at the end - purely a DOM position
		// change, the underlying args[] positional order (what actually gets
		// saved/loaded) is untouched, since count/CommandToJSON never look
		// at DOM order, only at #tblCommand_arg_N ids.
		var $precedingRow = val['toggleStyle'] ? $('#' + tblCommand + '_arg_' + (count - 1) + '_row') : null;
		if ($precedingRow && $precedingRow.length) {
			var $toggleRow = $(line);
			// Lets RenderCommandListRows(id) (fpp.js) find this toggle row
			// and show/hide it based on how many commands are in the list it
			// pairs with - Sequential/Parallel is meaningless with 0 or 1
			// commands, same rationale as the Match ALL/ANY toggle only
			// showing for 2+ conditions.
			$toggleRow.attr('data-pairs-with', $precedingRow.attr('id').replace(/_row$/, ''));
			// The divider (argRowGroupStart, drawn as a border-top - see
			// PrintArgInputs' isNewGroup logic above) was computed against
			// registration order, so it landed on $precedingRow (e.g. Then
			// Run's command-list row). This toggle row is being moved to sit
			// above it - and is itself hidden until there are 2+ commands
			// (see RenderCommandListRows), so it can't be the ONLY row
			// carrying the divider (a hidden row renders no border at all).
			// Put it on both rows instead of moving it - only one of the two
			// is ever visible at a time, so whichever one is showing renders
			// the divider correctly.
			if ($precedingRow.hasClass('argRowGroupStart')) {
				$toggleRow.addClass('argRowGroupStart');
			}
			if (sectionHeader !== '') {
				$(sectionHeader).insertBefore($precedingRow);
			}
			$toggleRow.insertBefore($precedingRow);
		} else {
			if (sectionHeader !== '') {
				$('#' + tblCommand).append(sectionHeader);
			}
			$('#' + tblCommand).append(line);
		}
		if (typeof val['contentListUrl'] != 'undefined') {
			var selId = '#' + tblCommand + '_arg_' + count + contentListPostfix;

			// Check if we should filter used sequences - reuse existing GetPlaylistEntry()
			var filterSequences = [];
			if (
				val['contentListUrl'].includes('sequences') &&
				$('#filterUsedSequences').length &&
				$('#filterUsedSequences').is(':checked')
			) {
				$(
					'#tblPlaylistLeadIn > tr:not(.unselectable), #tblPlaylistMainPlaylist > tr:not(.unselectable), #tblPlaylistLeadOut > tr:not(.unselectable)'
				).each(function () {
					var entry = GetPlaylistEntry(this);
					if (entry.sequenceName) {
						filterSequences.push(entry.sequenceName);
					}
				});
			}

			$.ajax({
				dataType: 'json',
				url: val['contentListUrl'],
				async: false,
				success: function (data) {
					if (Array.isArray(data)) {
						$.each(data, function (key, v) {
							// Skip if filtering and sequence is already used
							if (filterSequences.length > 0 && filterSequences.includes(v)) {
								return true; // continue to next iteration
							}
							var line = '<option value="' + v.replace('"', '&quot;') + '"';
							if (v == dv) {
								line += ' selected';
							}
							line +=
								'>' +
								v.replace(/&/g, '&amp;').replace(/</g, '&lt;') +
								'</option>';
							$(selId).append(line);
						});
					} else {
						$.each(data, function (key, v) {
							// Skip if filtering and sequence is already used
							if (filterSequences.length > 0 && filterSequences.includes(key)) {
								return true; // continue to next iteration
							}
							var line = '<option value="' + key.replace('"', '&quot;') + '"';
							if (key == dv) {
								line += ' selected';
							}
							line +=
								'>' +
								v.replace(/&/g, '&amp;').replace(/</g, '&lt;') +
								'</option>';
							$(selId).append(line);
						});
					}
				}
			});
		}
		if (subCommandInitFunc != null) {
			subCommandInitFunc();
		}
		count = count + 1;
	});

	// A little breathing room above each divider (argRowGroupStart) - table
	// cells don't respect margin, so this adds bottom padding to the row(s)
	// immediately before it instead. Tags a small bounded window (not just
	// the single nearest sibling) since the row right before a divider can
	// itself be conditionally hidden (Multisync's Hosts row when Multisync
	// is off; a toggleStyle Order row when its command list has 0/1 items) -
	// padding-bottom on a hidden row has no visual effect, so whichever one
	// in the window actually ends up visible is what shows the gap. Done as
	// its own pass over the finished table (not computed inline during the
	// loop above) since a toggleStyle row can get moved via insertBefore
	// after the fact - only the real, final DOM order reliably answers
	// "what rows are actually right before this divider".
	$('#' + tblCommand + ' tr.argRowGroupStart').each(function () {
		var $r = $(this);
		for (var k = 0; k < 3; k++) {
			$r = $r.prev('tr');
			if (!$r.length) {
				break;
			}
			$r.addClass('argRowGroupEnd');
		}
	});

	// Drop the heading of any section whose rows are all hidden at the current
	// UI level (see sectionRowCounts above) so it can't render as a label with
	// nothing under it.
	$('#' + tblCommand + ' tr.argSectionHeaderRow').each(function () {
		var name = $(this).attr('data-arg-section');
		// Only headings this call rendered are ours to remove: the playlist
		// entry editor parks the Entry Properties block in the command-args
		// tbody (see MoveEntryPropertiesToBottom), so a heading found here may
		// belong to the entry, not to the command being rendered.
		if (sectionRowCounts.hasOwnProperty(name) && !sectionRowCounts[name]) {
			$(this).remove();
		}
	});

	if (haveTime) {
		InitializeTimeInputs();
		for (const [key, value] of timeOptions) {
			$('#' + key).timepicker(value);
		}
	}

	if (haveDate) {
		InitializeDateInputs();
	}

	if (tblCommand == 'playlistEntryCommandOptions') UpdateChildVisibility();

	// Build the checkbox overlay for every multistring select now that their
	// options (including any synchronously fetched content lists) are in place.
	$('#' + tblCommand + " select[data-multistring='1']").each(function () {
		SyncMultistringChecks(this);
	});

	for (var i = 0; i < initFuncs.length; i++) {
		if (typeof window[initFuncs[i]] == 'function') {
			window[initFuncs[i]]();
		}
	}
}

// Mirror a multistring <select multiple> (the source of truth for save/load and
// content reloads) into a scrollable, touch-friendly checkbox list, and hide the
// native select. Rebuilt whenever the option list or selection changes. #2733
function SyncMultistringChecks (sel) {
	var $sel = $(sel);
	var id = $sel.attr('id');
	if (!id) {
		return;
	}
	var $container = $('#' + id + '_checks');
	if ($container.length === 0) {
		return;
	}
	$sel.addClass('d-none'); // the checkbox list replaces the native multi-select
	$container.empty();
	$sel.find('option').each(function () {
		var opt = this;
		if (opt.value === '') {
			return; // skip an allowBlanks placeholder
		}
		var $label = $("<label class='d-block text-nowrap fw-normal mb-0 py-1'></label>");
		var $cb = $("<input type='checkbox' class='me-2'>");
		$cb.prop('checked', opt.selected);
		if (opt.title) {
			$label.attr('title', opt.title);
		}
		$cb.on('change', function () {
			opt.selected = this.checked;
			$sel.trigger('change'); // fire onChange/child-visibility handlers
		});
		$label.append($cb).append(document.createTextNode(opt.text));
		$container.append($label);
	});
}

function ReloadContentList (baseUrl, inp) {
	var arg = $(inp);
	if (typeof inp === 'string') {
		arg = $('#' + inp);
	}
	var url = arg.data('contentlisturl');
	var allowblank = arg.data('allowblanks');

	// Remember the current selection so we can restore it after repopulating.
	var selectedValue = arg.val();

	// Get current browser IP (assumes http://IP/...)
	var currentHost = window.location.hostname;

	var hosts = baseUrl
		.split(',')
		.map(function (h) {
			return h.trim();
		})
		.filter(function (h) {
			return h != '';
		});
	var numHosts = hosts.length;

	// Tally how many of the selected hosts have each value. Use a Map to keep
	// first-seen order. An unreachable host simply contributes nothing, so its
	// items end up with a lower count.
	var items = new Map();
	hosts.forEach(function (burl) {
		let requestUrl;
		if (burl !== currentHost) {
			// Rewrite to proxy path
			requestUrl = buildHttpURL(currentHost, '/proxy/' + burl + '/' + url);
		} else {
			requestUrl = buildHttpURL(burl, '/' + url);
		}
		$.ajax({
			dataType: 'json',
			async: false,
			timeout: 2000,
			// An unreachable host, or an endpoint the host doesn't support
			// (e.g. a 404 on older firmware), just contributes nothing.
			error: function () {},
			url: requestUrl,
			success: function (data) {
				var addItem = function (value, label) {
					var existing = items.get(value);
					if (existing) {
						existing.count++;
					} else {
						items.set(value, { label: label, count: 1 });
					}
				};
				if (Array.isArray(data)) {
					$.each(data, function (key, v) {
						addItem(v, v);
					});
				} else {
					$.each(data, function (key, v) {
						addItem(key, v);
					});
				}
			}
		});
	});

	arg.empty();
	if (allowblank) {
		var blankLabel = arg.data('blanklabel');
		arg.append(
			"<option value=''>" +
				(typeof blankLabel === 'string'
					? String(blankLabel).replace(/&/g, '&amp;').replace(/</g, '&lt;')
					: '') +
				'</option>'
		);
	}
	// Match the HTML escaping the initial (local) population uses so names
	// containing &, <, or " do not break the option markup.
	var escapeAttr = function (s) {
		return String(s)
			.replace(/&/g, '&amp;')
			.replace(/</g, '&lt;')
			.replace(/"/g, '&quot;');
	};
	var escapeText = function (s) {
		return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;');
	};
	items.forEach(function (info, value) {
		var label = escapeText(info.label);
		var title = '';
		// When more than one host is selected, annotate items that are not present
		// on every host with a "(present/total)" count rather than hiding them.
		if (numHosts > 1) {
			label += ' (exists on ' + info.count + '/' + numHosts + ' Hosts)';
			title = ' title="Exists on ' + info.count + ' of ' + numHosts + ' selected hosts"';
		}
		arg.append(
			'<option value="' +
				escapeAttr(value) +
				'"' +
				title +
				'>' +
				label +
				'</option>'
		);
	});

	// Restore the previous selection if it is still available. Compare option
	// values directly rather than via a selector to stay safe with quotes.
	if (selectedValue != null) {
		var stillAvailable = false;
		arg.find('option').each(function () {
			if (this.value === selectedValue) {
				stillAvailable = true;
			}
		});
		if (stillAvailable) {
			arg.val(selectedValue);
		}
	}

	// Rebuild the checkbox overlay after the option list has been repopulated.
	if (arg.attr('data-multistring')) {
		SyncMultistringChecks(arg[0]);
	}
}

function PopulateExistingCommand (
	json,
	commandSelect,
	tblCommand,
	configAdjustable = false,
	argPrintFunc = PrintArgInputs
) {
	if (typeof json != 'undefined') {
		$('#' + commandSelect).val(json['command']);
		CommandSelectChanged(
			commandSelect,
			tblCommand,
			configAdjustable,
			argPrintFunc
		);
		var baseUrl = '';
		if (allowMultisyncCommands) {
			var to = typeof json['multisyncCommand'];

			if (typeof json['multisyncCommand'] != 'undefined') {
				var val = json['multisyncCommand'];
				$('#' + tblCommand + '_multisync').prop('checked', val);
				if (val) {
					val = json['multisyncHosts'];
					if (val !== undefined) {
						baseUrl = val;
					}
					$('#' + tblCommand + '_multisyncHosts_row').show();
					SetMultisyncHosts(tblCommand, val);
				}
			}
		} else {
			$('#' + tblCommand + '_multisync_row').hide();
			$('#' + tblCommand + '_multisyncHosts_row').hide();
		}

		if (typeof json['args'] != 'undefined') {
			var count = 1;
			$.each(json['args'], function (key, v) {
				var inp = $('#' + tblCommand + '_arg_' + count);
				if (baseUrl != '') {
					ReloadArgContentList(tblCommand, count, baseUrl);
				}

				var multattr = inp.attr('multiple');
				if (inp.attr('type') == 'checkbox') {
					var checked = false;
					if (v == 'true' || v == '1') {
						checked = true;
					}
					inp.prop('checked', checked);
				} else if (typeof multattr !== typeof undefined && multattr !== false) {
					var split = v.split(',');
					// console.log(inp.attr('type') + "  " + inp.attr('multiple') + "  " + v + "  " + split +
					// " " + split.length + "\n");

					$('#' + tblCommand + '_arg_' + count + ' option').prop(
						'selected',
						function () {
							return ~$.inArray(this.text, split);
						}
					);
					// Reflect the loaded selection into the checkbox overlay.
					SyncMultistringChecks(inp[0]);
				} else {
					// Update the colour fields differently
					if (inp.attr('type') === 'color') {
						// v is already the hex colour value from the playlist entry
						inp.attr('value', v);
						inp.css('background-color', v);
					} else {
						inp.val(v).change();
					}
				}

				if (inp.data('url') != null) {
					SubCommandChanged(
						tblCommand + '_arg_' + count,
						configAdjustable,
						argPrintFunc
					);
				}

				if (
					typeof json['adjustable'] != 'undefined' &&
					typeof json['adjustable'][count] != 'undefined'
				) {
					$('#' + tblCommand + '_arg_' + count + '_adjustable').prop(
						'checked',
						true
					);
				}
				count = count + 1;
			});
		}
	}
}

/**
 * Tries to activate the jQuery Colpicker for any colour input fields on the Command Editor form
 * because the modal is not immediately visible, the colpciker doesn't seem to work, so we create a
 * small loop that waits for the modal to show and then we active the colpicker
 */
function fppCommandColorPicker () {
	if (
		typeof fppCommandColorPicker_fppDialogIntervalTimer === 'undefined' ||
		fppCommandColorPicker_fppDialogIntervalTimer === null
	) {
		// Use a interval timer to keep waiting for a open modal to then apply the colpicker
		fppCommandColorPicker_fppDialogIntervalTimer = setInterval(function () {
			// Detect if the modal is visible now
			if ($('.modal-body').is(':visible') === true) {
				fppCommandColorPicker_fppDialogIsOpen = true;

				// Destroy existing colour pickers
				$('div[id*="collorpicker_"]').remove();

				// try to calculate margins around the modal dialog so we can try correct the color pickers
				// location the colour picker is using the viewport dimensions but the page we're on is in a
				// modal with a top and left pixel offset
				var modalDialog_topOffset = Math.round(
					$('.modal-dialog').css('margin-top').replace('px', '')
				);
				var modalDialog_LeftOffset = Math.round(
					$('.modal-dialog').css('margin-left').replace('px', '')
				);
				var modalDialog_headerHeight = Math.round(
					$('.modal-header').innerHeight()
				);
				var modalDialog_bodyPaddingHeight = Math.round(
					$('.modal-body').css('padding-top').replace('px', '')
				);
				var colpickNewTopMargin =
					-Math.abs(modalDialog_topOffset + modalDialog_headerHeight) +
					modalDialog_bodyPaddingHeight;

				// Add the colour picker to any color elements if we don't have as many as there are colour
				// inputs
				if (
					$('div[id*="collorpicker_"]').length !== $('.fppCommandColor').length
				) {
					// Ideally we want to append to the modals footer element, but this doesn't exist on the
					// commandPresets page
					var appendToElement = '.modal-footer';
					// If the footer doesn't exist, append to the header (sounds weird but it works and allows
					// the colour picker to float over the footer and not get obscured begin it)
					if ($('.modal-footer').length === 0) {
						appendToElement = '.modal-header';
					}
				}
			} else {
				// Not found yet so keep looping
				fppCommandColorPicker_fppDialogIsOpen = false;
			}

			fppCommandColorPicker_loopCount++;
			if (
				fppCommandColorPicker_loopCount ===
					fppCommandColorPicker_loopMaxRetries ||
				fppCommandColorPicker_fppDialogIsOpen === true
			) {
				clearInterval(fppCommandColorPicker_fppDialogIntervalTimer);
				// Reset the interval reference so it can started again
				fppCommandColorPicker_fppDialogIntervalTimer = null;
				// reset the loop count so we're ready again
				fppCommandColorPicker_loopCount = 0;
			}
		}, fppCommandColorPicker_intervalMs);
	} else {
		// interval loop is defined and most probably running as something might be in the process of
		// changing reset the loop count to give it more time
		fppCommandColorPicker_loopCount = 0;
	}
}

function FileChooser (dir, target) {
	if ($('#fileChooser').length == 0) {
		var dialogHTML =
			"<div id='fileChooserPopup'><div id='fileChooserDiv'></div></div>";
		$(dialogHTML).appendTo('body');
	}

	$('#fileChooserPopup').fppDialog({
		height: 440,
		width: 600,
		title: 'File Chooser',
		modal: true
	});
	$('#fileChooserPopup').fppDialog('moveToTop');
	$('#fileChooserDiv').load('fileChooser.php', function () {
		SetupFileChooser(dir, target);
	});
}

function EditCommandTemplateCanceled (row) {
	var json = $(row).find('.cmdTmplJSON').text();
	var data = JSON.parse(json);
	$(row).find('.cmdTmplCommand').val(data.command);
}

function EditCommandTemplateSaved (row, data) {
	FillInCommandTemplate(row, data);
}

function EditCommandTemplate (row, onReady = null) {
	var command = $(row).find('.cmdTmplCommand').val();
	var json = $(row).find('.cmdTmplJSON').text();

	var cmd = {};
	if (json == '') {
		cmd.command = command;
		cmd.args = [];
		cmd.multisyncCommand = false;
		cmd.multisyncHosts = '';
	} else {
		cmd = JSON.parse(json);
		if (cmd.command != command) {
			cmd.command = command;
			cmd.args = [];
			cmd.multisyncCommand = false;
			cmd.multisyncHosts = '';
		}
	}

	ShowCommandEditor(
		row,
		cmd,
		'EditCommandTemplateSaved',
		'EditCommandTemplateCanceled',
		'',
		onReady
	);
}

function GetCommandTemplateData (row) {
	var json = $(row).find('.cmdTmplJSON').text();

	if (json != '') return JSON.parse(json);

	var data = {};
	data.command = '';
	data.args = [];
	data.multisyncCommand = false;
	data.multisyncHosts = '';

	return data;
}

function FillInCommandTemplate (row, data) {
	if (row.find('.cmdTmplName').val() == '' && data.hasOwnProperty('name')) {
		row.find('.cmdTmplName').val(data.name);
	}

	// The row template ships with an empty .cmdTmplCommand select; populate it
	// per-row here (rather than once, statically, in PHP) so the currently-set
	// command for THIS row can be passed through as the "always show" exception
	// to UI-level filtering.
	var $cmdSelect = row.find('.cmdTmplCommand');
	if ($cmdSelect.find('option').length === 0) {
		LoadCommandList($cmdSelect, data.command);
	}

	// Check if command exists in the command list
	var commandExists =
		data.command !== '' && commandListByName.hasOwnProperty(data.command);

	// If command doesn't exist in the dropdown, add it as a disabled option
	if (data.command !== '' && !commandExists) {
		var $select = row.find('.cmdTmplCommand');
		// Remove any previously added invalid option to avoid duplicates
		$select.find('option.invalidCommandOption').remove();
		// Add the invalid command as a disabled option
		$select.prepend(
			'<option class="invalidCommandOption" value="' +
				data.command +
				'" disabled>' +
				data.command +
				' (unavailable)</option>'
		);
	}

	row.find('.cmdTmplCommand').val(data.command);

	if (data.hasOwnProperty('presetSlot'))
		row.find('.cmdTmplPresetSlot').val(data.presetSlot);

	// Add visual indicator if command is missing
	if (data.command !== '' && !commandExists) {
		row.addClass('commandPresetInvalidCommand');
		row.find('.cmdTmplCommand').css('background-color', '#ffcccc');
	} else {
		row.removeClass('commandPresetInvalidCommand');
		row.find('.cmdTmplCommand').css('background-color', '');
	}

	if (data.args.length) {
		var args = '';
		if (data.command == 'Run Script') {
			if (data.args.length > 1) args = data.args[0] + ' | ' + data.args[1];
			else args = data.args[0];
		} else if (data.command == 'If') {
			// args are [conditions, thenCommands, thenMode, elseCommands,
			// elseMode] - showing that raw (the generic branch below) is
			// unreadable. Reuse the same formula-builder the condition-tree
			// editor uses for its own live preview for the check, plus a
			// short summary of what actually runs (CommandListSummary,
			// already used by the commandlist widget's own rows) - a
			// condition with no visible actions would otherwise look like
			// the preset does nothing at all.
			try {
				var condTree = BackendJSONToTree(JSON.parse(data.args[0] || '{}'));
				var formula = condTree.items.length ? BuildConditionFormula(condTree) : 'No conditions configured';
				var summarizeCmdList = function (jsonStr) {
					var cmds = JSON.parse(jsonStr || '[]');
					return cmds.length ? cmds.map(CommandListSummary).join(', ') : '(none)';
				};
				args =
					'If ' + formula +
					' → Then: ' + summarizeCmdList(data.args[1]) +
					' | Otherwise: ' + summarizeCmdList(data.args[3]);
			} catch (e) {
				args = data.args.join(' | ');
			}
		} else {
			args = data.args.join(' | ');
		}

		row.find('.cmdTmplArgs').html(args);
		row.find('.cmdTmplArgsTable').show();
	} else {
		row.find('.cmdTmplArgs').html('');
		row.find('.cmdTmplArgsTable').hide();
	}

	var command = {};
	command.command = data.command;
	command.args = data.args;
	var mInfo = '';
	if (data.hasOwnProperty('multisyncCommand')) {
		if (data.multisyncCommand) {
			mInfo = 'Yes';
		} else {
			mInfo = 'No';
		}

		command.multisyncCommand = data.multisyncCommand;
		if (data.multisyncCommand && data.hasOwnProperty('multisyncHosts')) {
			mInfo += '<br>' + data.multisyncHosts;
			command.multisyncHosts = data.multisyncHosts;
		}
	} else {
		mInfo = 'No';
	}

	row.find('.cmdTmplJSON').html(JSON.stringify(command));

	// Surface the multisync target hosts in the row preview (the
	// .cmdTmplMulticastInfo span was previously left empty). An empty host
	// list means the command is sent to all hosts.
	if (data.multisyncCommand) {
		var hostsText =
			data.hasOwnProperty('multisyncHosts') && data.multisyncHosts != ''
				? data.multisyncHosts
				: 'All hosts';
		row.find('.cmdTmplMulticastInfo').html('<b>Multisync:</b> ' + hostsText);
	} else {
		row.find('.cmdTmplMulticastInfo').html('');
	}

	var json = JSON.stringify(command);
	var tip = 'No command selected.';
	if (json != '') {
		var data = JSON.parse(json);
		if (data.command != '') {
			// Check if command exists before accessing its properties
			if (!commandExists) {
				tip =
					"<span class='tooltipSpan' style='display: block; text-align: left; color: red;'><b>WARNING: Command not available</b><br>" +
					'<b>Command: </b>' +
					data.command +
					'<br>' +
					'This command is not currently available. It may be from a disabled plugin or require additional configuration (e.g., MQTT).' +
					'</span>';
			} else {
				tip =
					"<span class='tooltipSpan' style='display: block; text-align: left;'><b>Command: </b>" +
					data.command +
					'<br>';

				if (data.hasOwnProperty('multisyncCommand')) {
					tip += '<b>Multisync: </b>';
					if (data.multisyncCommand) tip += 'Yes';
					else tip += 'No';

					tip += '<br>';

					if (data.hasOwnProperty('multisyncHosts')) {
						tip += '<b>Multisync Hosts: </b>' + data.multisyncHosts + '<br>';
					}
				}
				var args = commandListByName[data.command]['args'];
				if (data.args.length) {
					for (var j = 0; j < args.length; j++) {
						tip +=
							'<b>' + args[j]['description'] + ': </b>' + data.args[j] + '<br>';
					}
				}
				tip += '</span>';
			}
		}
	}

	row.find('.cmdTmplTooltipIcon').attr('data-bs-original-title', tip);
	row.find('.cmdTmplTooltipIcon').tooltip();
}

function RunCommandJSON (cmdJSON) {
	$.ajax({
		url: 'api/command',
		type: 'POST',
		contentType: 'application/json',
		data: cmdJSON,
		async: true,
		success: function (data) {
			$.jGrowl('Command ran', { themeState: 'success' });
		},
		error: function (...args) {
			DialogError(
				'Command failed',
				'api/command call failed' + show_details(args)
			);
		}
	});
}

function RunCommand (cmd) {
	RunCommandJSON(JSON.stringify(cmd));
}

function RunCommandSaved (item, data) {
	if (data.command == null) return;

	var json = JSON.stringify(data);
	$('#runCommandJSON').html(json);

	Post('api/configfile/instantCommand.json', false, json);

	RunCommand(data);
}

function ShowRunCommandPopup () {
	var item = $('#runCommandJSON');
	var cmd = {};
	var json = $(item).text();

	if (json != '') cmd = JSON.parse(json);
	else cmd = Get('api/configfile/instantCommand.json', false, true);

	allowMultisyncCommands = true;

	var args = {};
	args.title = 'Run FPP Command';
	args.saveButton = 'Run and Close';
	args.cancelButton = 'Cancel';
	args.showPresetSelect = true;

	ShowCommandEditor(item, cmd, 'RunCommandSaved', '', args);
}

function ShowCommandEditor (
	target,
	data,
	callback,
	cancelCallback = '',
	args = '',
	onReady = null
) {
	if (typeof args === 'string') {
		args = {};
		args.title = 'FPP Command Editor';
		args.saveButton = 'Accept Changes';
		args.cancelButton = 'Cancel Edit';
		args.showPresetSelect = false;
	}

	allowMultisyncCommands = true;

	if ($('#commandEditorPopup').length == 0) {
		var dialogHTML =
			"<div id='commandEditorPopup'><div id='commandEditorDiv'></div></div>";
		$(dialogHTML).appendTo('body');
	}

	$('#commandEditorPopup').fppDialog({
		height: 'auto',
		width: 600,
		title: args.title,
		modal: true,
		open: function (event, ui) {
			$('#commandEditorPopup')
				.parent()
				.find('.ui-dialog-titlebar-close')
				.hide();
		},
		closeOnEscape: false
	});

	$('#commandEditorDiv').load('commandEditor.php', function () {
		CommandEditorSetup(target, data, callback, cancelCallback, args);

		//
		// Add the colour picker to any color elements
		fppCommandColorPicker();

		// Lets a caller append extra content into the SAME singleton dialog
		// once the command editor's own fields have finished loading -
		// recurringtasks.php uses this to add a Result Variable/Filter
		// section below the command fields, in one dialog instead of
		// nesting a second one (nesting a dialog inside this singleton
		// corrupts it - see cerebrum Do-Not-Repeat). No other caller passes
		// this, so existing behavior is unaffected.
		if (typeof onReady === 'function') {
			onReady();
		}
	});
}

function PreviewSchedule () {
	var response = '';
	$.ajax({
		type: 'GET',
		url: 'schedulePreview.php',
		async: false,
		success: function (text) {
			response = text;
		}
	});

	var options = {
		id: 'schedulePreview',
		title: 'Schedule Preview: Nested Table View',
		body: "<div id='schedulePreviewDiv'> " + response + '</div>',
		class: 'modal-xl',
		keyboard: true,
		backdrop: true
	};

	DoModalDialog(options);
}

var gblScheduleCalendar = null;

/**
 * Calendar preview of the schedule - month, week, day and list views over any
 * date range, alongside the existing flat list preview.
 *
 * Events come from /api/fppd/schedule/range, which expands the schedule rules
 * across whatever range the user has paged to. That is deliberately a different
 * endpoint from the one the list preview uses: /api/fppd/schedule can only
 * report the rolling window fppd has actually queued up.
 */
function ScheduleCalendar () {
	// FullCalendar is a chunky bundle and most visits to the scheduler never
	// open it, so pull it in on first use. jQuery caches the fetch.
	if (typeof FullCalendar === 'undefined') {
		$.ajax({
			url: 'js/fullcalendar/index.global.min.js',
			dataType: 'script',
			cache: true
		})
			.done(ScheduleCalendar)
			.fail(function () {
				$.jGrowl('Unable to load the calendar library.', {
					themeState: 'danger'
				});
			});
		return;
	}

	$.get('scheduleCalendar.php', function (response) {
		DoModalDialog({
			id: 'scheduleCalendarModal',
			title: 'Schedule Preview: Calendar View',
			body: response,
			class: 'modal-xl',
			keyboard: true,
			backdrop: true,
			close: function () {
				// Drop the instance so a reopen rebuilds against the current
				// schedule rather than redisplaying a stale render.
				if (gblScheduleCalendar) {
					gblScheduleCalendar.destroy();
					gblScheduleCalendar = null;
				}
			}
		});

		// Build only once Bootstrap has laid the modal out; FullCalendar sizes
		// itself on init and would measure a zero-width hidden container.
		$('#scheduleCalendarModal')
			.off('shown.bs.modal.fppSchCal')
			.on('shown.bs.modal.fppSchCal', function () {
				InitScheduleCalendar();
			});
	});
}

function InitScheduleCalendar () {
	var el = document.getElementById('scheduleCalendar');
	if (!el || gblScheduleCalendar) return;

	var use12Hour =
		!settings.hasOwnProperty('TimeFormat') || settings['TimeFormat'] != '%H:%M';

	gblScheduleCalendar = new FullCalendar.Calendar(el, {
		initialView: 'dayGridMonth',
		headerToolbar: {
			left: 'prev,next today',
			center: 'title',
			right: 'dayGridMonth,timeGridWeek,timeGridDay,listWeek'
		},
		buttonText: {
			today: 'Today',
			month: 'Month',
			week: 'Week',
			day: 'Day',
			list: 'List'
		},
		// Sized against the viewport rather than the parent: the modal body has
		// no resolved height when the calendar initialises, so a percentage
		// height would collapse the grid to nothing.
		height: '68vh',
		nowIndicator: true,
		eventTimeFormat: {
			hour: 'numeric',
			minute: '2-digit',
			hour12: use12Hour
		},
		slotLabelFormat: {
			hour: 'numeric',
			minute: '2-digit',
			hour12: use12Hour
		},
		// Playlists that run past midnight are one occurrence, not two, so keep
		// them as a single bar rather than letting the month grid split them.
		displayEventEnd: true,
		// Day numbers become links through to the detail views, which is the
		// natural way out of the summarised month grid.
		navLinks: true,
		// Whether a request is summarised depends on the view, not just the
		// dates. Left to its own devices FullCalendar would reuse the month's
		// summarised events for a week inside that month, so make every date
		// change refetch. The requests are small and quick.
		lazyFetching: false,
		// A busy show can put several entries on one day. Let FullCalendar fit
		// as many as the row height allows and collapse the rest behind a
		// "+N more" popover - a fixed cap would overflow the grid instead.
		dayMaxEvents: true,
		// Likewise in the time grid: a schedule that repeats a command every few
		// minutes would otherwise squeeze each of a week's seven columns too
		// narrow to read. Cap how many sit side by side and link to the rest.
		eventMaxStack: 3,
		views: {
			// A single day has the whole width to itself, so it can afford to
			// show everything side by side without becoming unreadable.
			timeGridDay: { eventMaxStack: -1 }
		},
		eventOrder: 'schedRank,start,title',
		events: FetchScheduleCalendarEvents,
		eventDidMount: function (info) {
			$(info.el).attr({
				'data-bs-toggle': 'tooltip',
				'data-bs-html': 'true',
				'data-bs-placement': 'auto',
				'data-bs-title': ScheduleCalendarItemInfo(info.event.extendedProps)
			});
			new bootstrap.Tooltip(info.el);
		}
	});

	gblScheduleCalendar.render();

	$('#schCalShowDisabled, #schCalHideOverridden').on('change', function () {
		gblScheduleCalendar.refetchEvents();
	});
}

/**
 * Only the detail views can meaningfully show individual occurrences. A month of
 * a schedule that repeats a command every 20 minutes is thousands of items, none
 * of which fit in a day cell, so wide spans ask the backend to collapse to one
 * entry per day and just report what is active.
 *
 * This keys off how much time is on screen rather than the view name. The events
 * function runs mid-transition, when the calendar still reports the view being
 * navigated away from, so the requested span is the only reliable signal - and
 * it is the one that actually matters.
 */
function ScheduleCalendarWantsSummary (startDate, endDate) {
	var days = (endDate.getTime() - startDate.getTime()) / 86400000;

	// Week and day views span at most a week; a month grid always spans four
	// weeks or more once its leading and trailing days are included.
	return days > 14;
}

function FetchScheduleCalendarEvents (fetchInfo, successCallback, failureCallback) {
	// fetchInfo covers exactly the span the active view is showing, so a request
	// never pulls back more of the schedule than is on screen.
	var summary = ScheduleCalendarWantsSummary(fetchInfo.start, fetchInfo.end);

	var params = {
		start: Math.floor(fetchInfo.start.getTime() / 1000),
		end: Math.floor(fetchInfo.end.getTime() / 1000)
	};

	if (summary) params.summary = 1;
	if ($('#schCalShowDisabled').is(':checked')) params.includeDisabled = 1;

	$.get('api/fppd/schedule/range', params)
		.done(function (data) {
			var items =
				data && data.schedule && data.schedule.items
					? data.schedule.items
					: [];

			// Overridden occurrences are the ones a higher priority playlist
			// will stop from running. Hidden by default so the calendar shows
			// what will actually play; turn the switch off to see them and
			// find where the schedule conflicts.
			if ($('#schCalHideOverridden').is(':checked')) {
				items = items.filter(function (item) {
					return !item.overridden;
				});
			}

			successCallback(
				items.map(function (item) {
					return {
						title: ScheduleCalendarItemTitle(item),
						start: new Date(item.startTime * 1000),
						end: new Date(item.endTime * 1000),
						// A summarised item stands for everything that entry does
						// that day, so show it as a plain all-day chip rather than
						// implying it runs only for the span it happens to cover.
						allDay: summary,
						// What is playing matters more than the housekeeping
						// commands around it, so when a day has more entries than
						// fit, the commands are the ones that collapse away.
						schedRank: ScheduleCalendarItemRank(item),
						classNames: ScheduleCalendarItemClasses(item),
						extendedProps: item
					};
				})
			);
		})
		.fail(function () {
			$.jGrowl('Unable to load the schedule for this range.', {
				themeState: 'danger'
			});
			failureCallback(new Error('schedule range request failed'));
		});
}

function ScheduleCalendarItemTitle (item) {
	var title;

	if (item.command == 'Start Playlist') {
		title = item.playlist;
	} else if (item.args && item.args.length) {
		// Commands carry their arguments; show the first one for context since
		// it is usually the thing being acted on.
		title = item.command + ': ' + item.args[0];
	} else {
		title = item.command;
	}

	// In summary mode one item stands for a whole day of occurrences - say how
	// many, otherwise a 20-minute repeat looks identical to a one-off.
	if (item.summary && item.count > 1) title += ' ×' + item.count;

	return title;
}

function ScheduleCalendarItemRank (item) {
	if (item.type == 'playlist') return 0;
	if (item.type == 'sequence') return 1;
	return 2;
}

function ScheduleCalendarItemClasses (item) {
	var classes = ['sch-cal-event'];

	if (item.type == 'command') classes.push('sch-cal-event-command');
	else if (item.type == 'sequence') classes.push('sch-cal-event-sequence');

	if (item.overridden) classes.push('sch-cal-event-overridden');
	if (!item.enabled) classes.push('sch-cal-event-disabled');

	return classes;
}

function ScheduleCalendarItemInfo (item) {
	var info = '';

	if (item.summary && item.count > 1) {
		info +=
			'<b>' +
			item.count +
			' occurrences</b> this day, ' +
			item.startTimeStr +
			' to ' +
			item.endTimeStr +
			'.<br>Open the day or week view for each one.<br>';
	} else {
		info += '<b>Start:</b> ' + item.startTimeStr + '<br>';
		info += '<b>End:</b> ' + item.endTimeStr + '<br>';
	}

	info += '<b>Action:</b> ' + item.command + '<br>';

	if (item.args && item.args.length && item.command != 'Start Playlist')
		info += '<b>Args:</b> ' + item.args.join(' | ') + '<br>';

	if (item.command == 'Start Playlist') {
		info += '<b>Stop:</b> ' + item.stopTypeStr + '<br>';
		info += '<b>Repeat:</b> ' + (item.repeat ? 'Immediate' : 'None') + '<br>';
	}

	if (item.repeatInterval)
		info += '<b>Every:</b> ' + item.repeatInterval / 60 + ' minutes<br>';

	if (item.multisyncCommand)
		info +=
			'<b>Multisync:</b> ' + (item.multisyncHosts || 'all remotes') + '<br>';

	if (item.overridden)
		info +=
			'<span class="text-danger">Skipped - a higher priority playlist is already running.</span><br>';

	if (!item.enabled)
		info += '<span class="text-muted">Entry is disabled.</span><br>';

	return info;
}

function ToggleMenu () {
	if (gblNavbarMenuVisible == 1) {
		$('html').removeClass('nav-open');
		gblNavbarMenuVisible = 0;
		$('#bodyClick').fadeOut('slow', function () {
			$('#bodyClick').remove();
		});
	} else {
		div = '<div id="bodyClick"></div>';
		$(div)
			.appendTo('body')
			.on('click', function () {
				$('.navbar-toggler').trigger('click');
				$('html').removeClass('nav-open');
				gblNavbarMenuVisible = 0;
			});
		$('html').addClass('nav-open');
		gblNavbarMenuVisible = 1;
	}
}

/*
 * Simply Loads the current system status into a JSON variable.
 * Other functions can either call the variable as a on-off or subscribe to changes.
 */
function LoadSystemStatus () {
	// When the WebSocket is delivering fppd's status, this poll drops to a slow
	// cadence and fetches only the host-side augmentation (?systemonly=1), so it
	// doesn't overwrite the fresh pushed status with a stale copy.  Otherwise it
	// fetches the full status exactly as before.
	var wsActive = fppdWSConnected;
	// This request is the only source of restartFlag/rebootFlag; snapshot the
	// epoch so a flag the page sets while it is in flight wins over the older
	// value this response will carry.
	var flagEpoch = gblRestartRebootFlagEpoch;
	$.ajax({
		url: wsActive ? 'api/system/status?systemonly=1' : 'api/system/status',
		dataType: 'json',
		success: function (response, reqStatus, xhr) {
			if (!response || typeof response !== 'object') return;
			if (wsActive && fppdWSConnected) {
				// Augmentation only; merge onto the WebSocket-supplied base.
				applySystemAugmentation(response);
				applyServerRestartRebootFlags(response, flagEpoch);
			} else if (!wsActive && !fppdWSConnected) {
				// Full status (no WebSocket) -- original behavior.  Both the
				// captured and the live flag have to be clear: if the socket
				// connected and pushed while this request was in flight, this
				// response is the older copy and applying it would replace the
				// fresher pushed status and wipe _wsWarnings (a warning would
				// blink out until the next slow poll).
				_wsWarnings = response.warnings || [];
				_wsWarningInfo = response.warningInfo || [];
				_systemWarnings = [];
				_systemWarningInfo = [];
				lastStatusJSON = response;
				lastStatus = response.status;
				triggerStatusChangeFunctions();
				applyServerRestartRebootFlags(response, flagEpoch);
			}
			// else the socket changed state while the request was in flight, and
			// either way the response is dropped: it dropped (this systemonly
			// response lacks the fppd base, and the close handler already
			// triggered a full reload), or it connected (this full response is
			// older than the status already pushed, and the transition's own
			// immediate systemonly poll re-fetches the augmentation).
		},
		complete: function () {
			clearTimeout(statusTimeout);
			var secs = fppdWSConnected
				? gblSystemAugRefreshSeconds
				: gblStatusRefreshSeconds;
			statusTimeout = setTimeout(LoadSystemStatus, secs * 1000);
		}
	});
}

// Recompute the displayed warning arrays from the two independent sources.
// updateWarnings() and everything else read lastStatusJSON.warnings/warningInfo
// unchanged; only how those arrays are assembled changes.
function mergeWarningsInto (obj) {
	if (!obj) return;
	obj.warnings = _wsWarnings.concat(_systemWarnings);
	obj.warningInfo = _wsWarningInfo.concat(_systemWarningInfo);
}

// The keys api/system/status adds on top of fppd's own /fppd/status payload --
// see finalizeStatusJson() in www/api/controllers/system.php.  fppd's WebSocket
// snapshot carries none of them, so these are exactly the keys that have to
// survive a snapshot; everything else in lastStatusJSON is fppd-owned.
// warnings/warningInfo are deliberately NOT listed: both sources produce them
// and mergeWarningsInto() reassembles them from _wsWarnings/_systemWarnings.
var STATUS_AUGMENTATION_KEYS = [
	'advancedView',
	'wifi',
	'interfaces',
	'rebootFlag',
	'restartFlag',
	'bootDelayActive',
	'bootDelayStart',
	'bootDelayDuration',
	'pluginHeaderIndicators'
];

// A status snapshot pushed by fppd over the WebSocket.
function applyWsStatus (status) {
	if (!status || typeof status !== 'object') return;
	// Rebuild rather than merge.  fppd emits a number of keys only conditionally
	// -- breadcrumbs only while a nested playlist runs, the media/sequence and
	// milliseconds_elapsed fields only while playing, others per mode -- so
	// merging each snapshot into the previous object leaves the last value of
	// each key in place forever.  That rendered a stale "(Main -> ...)"
	// breadcrumb label in the player status heading long after the nested
	// playlist ended.  The snapshot therefore replaces the whole fppd-owned
	// object and only the PHP augmentation above is carried across.
	var rebuilt = Object.assign({}, status);
	if (lastStatusJSON) {
		STATUS_AUGMENTATION_KEYS.forEach(function (key) {
			if (key in lastStatusJSON) rebuilt[key] = lastStatusJSON[key];
		});
	}
	_wsWarnings = status.warnings || [];
	_wsWarningInfo = status.warningInfo || [];
	lastStatusJSON = rebuilt;
	mergeWarningsInto(lastStatusJSON);
	lastStatus = status.status;
	triggerStatusChangeFunctions();
}

// The host-side augmentation from api/system/status?systemonly=1.
function applySystemAugmentation (response) {
	if (!lastStatusJSON) lastStatusJSON = {};
	_systemWarnings = response.warnings || [];
	_systemWarningInfo = response.warningInfo || [];
	Object.assign(lastStatusJSON, response);
	mergeWarningsInto(lastStatusJSON);
	triggerStatusChangeFunctions();
}

// Open (or reopen) the status WebSocket.  Safe to call repeatedly.
function startFppdWS () {
	if (typeof WebSocket === 'undefined') return; // very old browser: stay on polling
	if (fppdWS && (fppdWS.readyState === 0 || fppdWS.readyState === 1)) return;
	if (fppdWSReconnectTimer) {
		clearTimeout(fppdWSReconnectTimer);
		fppdWSReconnectTimer = null;
	}
	var proto = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
	// gblFPPRoot, not the page's own path: this URL is only ever built at
	// runtime in the browser, so mod_proxy_html cannot rewrite it the way it
	// rewrites static markup, and it has to name the same prefix the rest of
	// the site is served under. Getting it from the page path instead meant a
	// subdirectory page - /api/ (the API docs) or /wled/ - asked for
	// ws://<host>/api/fppdws, which nothing listens on: fppd registers the
	// socket at the root (WS_PATH_ADD("/fppdws") in src/StatusWebSocket.cpp).
	// That was refused and retried forever, filling the console with
	// NS_ERROR_WEBSOCKET_CONNECTION_REFUSED.
	var url = proto + '//' + window.location.host + gblFPPRoot + 'fppdws';
	// Detach the socket being replaced.  Belt and suspenders for the generation
	// check below: an abandoned socket is unreachable once nothing points at its
	// handlers, but the check is what makes a late event harmless either way.
	if (fppdWS) {
		fppdWS.onopen = null;
		fppdWS.onmessage = null;
		fppdWS.onclose = null;
		fppdWS.onerror = null;
	}
	var sock;
	try {
		sock = new WebSocket(url);
	} catch (e) {
		scheduleFppdWSReconnect();
		return;
	}
	fppdWS = sock;
	// Every handler below is bound to the socket generation that installed it.
	// The watchdog abandons a wedged socket by calling close() and reconnecting,
	// but a closing handshake can take a TCP timeout to complete: the old
	// socket's onclose then fires long after the new one is live, and without
	// this check it would tear down the NEW connection's state and clear its
	// watchdog -- which only onopen restarts -- leaving the page showing frozen
	// status forever while still claiming to be WebSocket-fed.
	sock.onopen = function () {
		if (sock !== fppdWS) return;
		fppdWSReconnectDelay = 1000;
		fppdWSLastMsgTime = Date.now();
		// Connected marker is set on the first message (proves data actually
		// flows through the proxy, not just that the handshake completed).
		startFppdWSWatchdog();
	};
	sock.onmessage = function (evt) {
		if (sock !== fppdWS) return;
		// Any frame proves the transport is alive, so the watchdog clock resets
		// before the payload is even looked at.
		fppdWSLastMsgTime = Date.now();
		var msg;
		try {
			msg = JSON.parse(evt.data);
		} catch (e) {
			return;
		}
		if (!msg || msg.type !== 'snapshot' || !msg.data) return;
		// Audio meters ride the same socket on their own key, at a much higher
		// rate than status. Handed straight to whoever is drawing them and
		// deliberately kept out of the status path below, which must only run
		// for an actual status snapshot.
		if (msg.data.levels && typeof window.PWMixerOnLevels === 'function') {
			window.PWMixerOnLevels(msg.data.levels);
		}
		if (!msg.data.status) return;
		// Only a parsed status snapshot may flip the page into WebSocket-fed
		// mode.  Setting the flag on any received frame instead means a garbage
		// or future non-snapshot frame switches the poll to ?systemonly=1 -- and
		// skips the immediate poll below -- while the page has no fppd status
		// base at all.
		var wasConnected = fppdWSConnected;
		fppdWSConnected = true;
		if (isHidden()) {
			// Hidden tab: keep the socket (so the page is current the instant it
			// comes back) but skip the status callbacks -- dozens of jQuery
			// operations per second against a document nobody is looking at.
			// handleVisibilityChange() applies this on the way back to visible.
			fppdWSPendingStatus = msg.data.status;
		} else {
			applyWsStatus(msg.data.status);
		}
		if (!wasConnected && !isHidden()) {
			// Just transitioned to WS-fed.  Poll the augmentation once
			// immediately (now that fppdWSConnected is set, this fetches
			// ?systemonly=1) so advancedView and the PHP-sourced crash-report
			// warning populate right away instead of blinking out until the
			// first slow poll; subsequent polls settle to the slow cadence.
			// While hidden there is no poll timer to preserve and nothing to
			// render, so the augmentation waits for the visibility handler.
			clearTimeout(statusTimeout);
			LoadSystemStatus();
		}
	};
	sock.onclose = function () {
		if (sock !== fppdWS) return;
		handleFppdWSDown();
	};
	sock.onerror = function () {
		// onclose fires after onerror; let it do the teardown.
	};
}

// Server pushes at least once a second while connected (the clock ticks), so a
// multi-second gap means the socket is wedged even if it hasn't reported closed.
function startFppdWSWatchdog () {
	if (fppdWSWatchdog) clearInterval(fppdWSWatchdog);
	fppdWSWatchdog = setInterval(function () {
		if (!fppdWS) return;
		if (fppdWSConnected && Date.now() - fppdWSLastMsgTime > 8000) {
			try {
				fppdWS.close();
			} catch (e) {}
			handleFppdWSDown();
		}
	}, 4000);
}

function handleFppdWSDown () {
	var wasConnected = fppdWSConnected;
	fppdWSConnected = false;
	if (fppdWSWatchdog) {
		clearInterval(fppdWSWatchdog);
		fppdWSWatchdog = null;
	}
	// Drop the fppd-sourced warnings; the full poll below re-supplies everything.
	_wsWarnings = [];
	_wsWarningInfo = [];
	// Anything buffered for a hidden tab predates the drop, so the full poll is
	// the authority now.
	fppdWSPendingStatus = null;
	if (wasConnected && !isHidden()) {
		// Fall back to the full status poll immediately so the fppd half (and,
		// if fppd itself is down, the "Not Running" state) refreshes without
		// waiting out the slow cadence.  Not while hidden: polling is stopped
		// there, and starting it here would resurrect it permanently (the
		// response reschedules itself) in a tab nobody is looking at.
		// handleVisibilityChange() runs this poll when the page comes back.
		clearTimeout(statusTimeout);
		LoadSystemStatus();
	}
	scheduleFppdWSReconnect();
}

function scheduleFppdWSReconnect () {
	if (fppdWSReconnectTimer) return;
	fppdWSReconnectTimer = setTimeout(function () {
		fppdWSReconnectTimer = null;
		startFppdWS();
	}, fppdWSReconnectDelay);
	fppdWSReconnectDelay = Math.min(fppdWSReconnectDelay * 2, 30000);
}

function triggerStatusChangeFunctions () {
	statusChangeFuncs.forEach(func => {
		// One consumer must not be able to kill the chain.  These callbacks run
		// against whatever the current status source supplied, and the WebSocket
		// and PHP payloads do not carry the same key set -- a consumer that
		// dereferences a key only one of them has throws, and an uncaught throw
		// here skips every remaining callback for that tick and propagates back
		// into the WebSocket onmessage handler.
		try {
			func();
		} catch (e) {
			console.error('Status change callback failed:', e);
		}
	});
}

/*
 * How often should the page call api/system/status
 */
function SetStatusRefreshSeconds (seconds) {
	if (seconds == undefined) return;
	if (!Number.isInteger(seconds)) return;
	if (seconds < 1) return;
	gblStatusRefreshSeconds = seconds;
}

/*
 * How often to poll the host-side augmentation (advancedView cpu/mem, wifi, ...)
 * while the status WebSocket is delivering the fppd half.  Defaults to 30s so
 * incidental pages are cheap; a page that shows near-real-time system stats
 * (system-stats.php) lowers this while it is open.
 */
function SetSystemAugRefreshSeconds (seconds) {
	if (seconds == undefined) return;
	if (!Number.isInteger(seconds)) return;
	if (seconds < 1) return;
	gblSystemAugRefreshSeconds = seconds;
	// If the WebSocket is already feeding status, apply the new cadence now.
	if (fppdWSConnected) {
		clearTimeout(statusTimeout);
		statusTimeout = setTimeout(LoadSystemStatus, gblSystemAugRefreshSeconds * 1000);
	}
}

/*
 * Pass your function to this and it will be executed when the system status API is called
 */
function OnSystemStatusChange (funcToCall) {
	statusChangeFuncs.push(funcToCall);
}

/*
 * Keep an output test "Testing" dropdown in step with the test fppd is actually
 * running.  fppd reports the running test in the status payload as testMode
 * (same shape as api/testmode), so this follows a test started, changed or
 * stopped from any tab or device -- and restores the dropdown on page load,
 * since the first status snapshot arrives right after the WebSocket connects.
 *
 * selectId    -- id of the <select> whose option values are the test "type"s
 * outputTypes -- the "outputs" names this page's tests are started with.  Pixel,
 *                panel and PWM tests are all fppd mode "Outputs" with a numeric
 *                type, so this is what keeps a panel test from claiming the
 *                pixel dropdown (and vice versa).
 *
 * The value is set programmatically, which fires no change event, so the page
 * never re-issues a test it merely observed.  Only a *changed* testMode is
 * applied: a snapshot built between the user's Test Start and fppd acting on
 * it still carries the previous state, and applying that would flip the
 * dropdown back to Off for a second.
 */
function SyncTestModeSelect (selectId, outputTypes) {
	if (!Array.isArray(outputTypes)) outputTypes = [outputTypes];
	var lastApplied = null;
	OnSystemStatusChange(function () {
		if (!lastStatusJSON || !('status_name' in lastStatusJSON)) return; // not an fppd status yet
		var tm = lastStatusJSON.testMode;
		var key = tm ? JSON.stringify(tm) : '';
		if (key === lastApplied) return;
		lastApplied = key;

		var sel = $('#' + selectId);
		if (!sel.length) return;
		var val = '0';
		if (
			tm &&
			tm.enabled &&
			tm.mode == 'Outputs' &&
			tm.hasOwnProperty('type') &&
			outputTypes.indexOf(String(tm.outputs)) !== -1
		) {
			val = String(tm.type);
		}
		if (sel.find("option[value='" + val + "']").length && sel.val() != val) {
			sel.val(val);
		}
	});
}

/*
 * Helper function to format IP address with CIDR notation
 */
function formatIPWithCIDR (ip, prefixlen) {
	if (prefixlen !== undefined && prefixlen !== null) {
		return ip + '/' + prefixlen;
	}
	return ip;
}

// Plugin header indicators are built by third-party plugin code (each plugin's
// /api/plugin/<name>/headerIndicator endpoint, collected by
// GetPluginHeaderIndicators), so every field of one is untrusted input.  The
// three that land somewhere escaping alone can't make safe get a whitelist
// instead: a class list and a CSS animation name have to stay identifiers so
// they can't start a second declaration, a color has to be a color and not a
// url(), and a link has to be checked by scheme -- a perfectly escaped
// "javascript:..." still runs when clicked.  Everything else is set through the
// DOM (.text()/.attr()/.css()), never concatenated into markup.
var PLUGIN_INDICATOR_COLOR_RE = /^#[0-9a-fA-F]{3,8}$|^[a-zA-Z]+$/;
var PLUGIN_INDICATOR_CLASS_RE = /^[a-zA-Z][a-zA-Z0-9 _-]*$/;
var PLUGIN_INDICATOR_ANIMATION_RE = /^[a-zA-Z][a-zA-Z0-9_-]*$/;

// Same scheme check plugins.php applies to plugin-declared page URLs.  Returns
// the resolved absolute URL, or null when the link is unusable/unsafe -- in
// which case the indicator simply isn't clickable.
function PluginIndicatorSafeLink (link) {
	try {
		var parsed = new URL(link, window.location.href);
		if (parsed.protocol !== 'http:' && parsed.protocol !== 'https:') {
			return null;
		}
		return parsed.href;
	} catch (e) {
		return null;
	}
}

// Builds one header indicator element.  Rejected values fall back to the
// same defaults an absent value gets, so a malformed plugin still shows its
// (generic) indicator rather than disappearing.
function BuildPluginHeaderIndicator (indicator) {
	var pluginName = String(indicator.pluginName == null ? '' : indicator.pluginName);
	var icon = indicator.icon || 'fa-puzzle-piece';
	var color = indicator.color || '#999';
	var animate = indicator.animate || '';
	var link = PluginIndicatorSafeLink(indicator.link || '#');

	if (!PLUGIN_INDICATOR_CLASS_RE.test(icon)) {
		console.warn('Plugin ' + pluginName + ' header indicator: ignoring icon', icon);
		icon = 'fa-puzzle-piece';
	}
	if (!PLUGIN_INDICATOR_COLOR_RE.test(color)) {
		console.warn('Plugin ' + pluginName + ' header indicator: ignoring color', color);
		color = '#999';
	}
	if (animate && !PLUGIN_INDICATOR_ANIMATION_RE.test(animate)) {
		console.warn('Plugin ' + pluginName + ' header indicator: ignoring animation', animate);
		animate = '';
	}
	if (link === null) {
		console.warn('Plugin ' + pluginName + ' header indicator: ignoring link', indicator.link);
	}

	var $icon = $('<i>').addClass('fas').addClass(icon);
	if (animate) {
		$icon.css('animation', animate + ' 2s infinite');
	}

	// margin-left/transition are not set here: .pluginIndicator in fpp.css
	// already carries both.
	var $indicator = $('<span>')
		.addClass('pluginIndicator headerBox')
		.attr('data-plugin', pluginName)
		.attr('title', String(indicator.tooltip || 'Plugin Indicator'))
		.css('cursor', 'pointer')
		.css('color', color)
		.append($icon);

	if (link !== null) {
		$indicator.on('click', function () {
			window.location.href = link;
		});
	}

	return $indicator;
}

/*
 * Called each time the system status JSON is updated to refresh icons in the header bar.
 */
var headerCache = {}; // Used to cache what we've displayed on screen so we only update it if it has changed
function RefreshHeaderBar () {
	var data = lastStatusJSON;
	if (data == undefined || data == null) return;
	if (data.interfaces != undefined) {
		var rc = [];

		data.interfaces.forEach(function (e) {
			if (e.ifname === 'lo') {
				return 0;
			}
			if (e.ifname.startsWith('eth0:0')) {
				return 0;
			}
			if (e.ifname.startsWith('usb')) {
				return 0;
			}
			if (e.ifname.startsWith('veth')) {
				return 0;
			}
			if (e.ifname.startsWith('br')) {
				return 0;
			}
			if (e.ifname.startsWith('docker')) {
				return 0;
			}
			if (e.ifname.startsWith('can.')) {
				return 0;
			}

			if (!e.flags.some(flag => flag === 'NO-CARRIER')) {
				// Show up interfaces even if they do not have IP configured - eg Colorlight
				if (e.operstate === 'UP') {
					if (!(Array.isArray(e.addr_info) && e.addr_info.length > 0)) {
						var icon = 'text-success';
						var row =
							'<span ifname="' +
							e.ifname +
							'" class="ipTooltip" data-bs-toggle="tooltip" data-bs-html="true" data-bs-placement="bottom" data-bs-title="IP: ' +
							e.local +
							'" ><i class="fas fa-network-wired ' +
							icon +
							'"></i><small>' +
							e.ifname +
							'<div class="divIPAddress">: ' +
							e.local +
							'</div></small></span>';
						rc.push(row);
					}
				}

				e.addr_info.forEach(function (n) {
					if (
						n.family === 'inet' &&
						(n.local == '192.168.8.1' ||
							e.ifname.startsWith('SoftAp') ||
							e.ifname.startsWith('tether'))
					) {
						var ipWithCIDR = formatIPWithCIDR(n.local, n.prefixlen);
						var row =
							'<span ifname="' +
							e.ifname +
							'" class="ipTooltip" data-bs-toggle="tooltip" data-bs-html="true" data-bs-placement="bottom" data-bs-title="Tether IP: ' +
							ipWithCIDR +
							'"><i class="fas fa-broadcast-tower"></i><small>' +
							e.ifname +
							'<div class="divIPAddress">: ' +
							n.local +
							'</div></small></span>';
						rc.push(row);
					} else if (n.family === 'inet' && 'wifi' in e) {
						var ipWithCIDR = formatIPWithCIDR(n.local, n.prefixlen);
						var row =
							'<span ifname="' +
							e.ifname +
							'" class="ipTooltip" data-bs-toggle="tooltip" data-bs-html="true" data-bs-placement="bottom" data-bs-title="IP: ' +
							ipWithCIDR +
							'<br/>Strength: ' +
							e.wifi.level +
							e.wifi.unit +
							'">';
						row +=
							'<img title="WiFi Strength" src="images/redesign/wifi-' +
							e.wifi.desc +
							'.svg" height="14px"/>';
						row +=
							'<small>' +
							e.ifname +
							'<div class="divIPAddress">: ' +
							n.local +
							'</div></small></span>';
						rc.push(row);
					} else if (n.family === 'inet') {
						var icon = 'text-success';
						if (n.local.startsWith('169.254.') && e.flags.includes('DYNAMIC')) {
							icon = 'text-warning';
						} else if (e.flags.includes('STATIC') && e.operstate != 'UP') {
							icon = 'text-danger';
						}
						var ipWithCIDR = formatIPWithCIDR(n.local, n.prefixlen);
						var row =
							'<span ifname="' +
							e.ifname +
							'" class="ipTooltip" data-bs-toggle="tooltip" data-bs-html="true" data-bs-placement="bottom" data-bs-title="IP: ' +
							ipWithCIDR +
							'" ><i class="fas fa-network-wired ' +
							icon +
							'"></i><small>' +
							e.ifname +
							'<div class="divIPAddress">: ' +
							n.local +
							'</div></small></span>';
						rc.push(row);
					}
				});
			}

			// All rows start with '<span ifname="INTERFACENAME" ' so we can sort the full list in the UI.
			rc.sort();
		});
		if (headerCache.Interfaces != rc.join('')) {
			$('#header_IPs').html(rc.join(''));
			var titles = document.getElementsByClassName('ipTooltip');
			[].forEach.call(titles, function (value) {
				new bootstrap.Tooltip(value);
			});
			headerCache.Interfaces = rc.join('');
		}
	}

	if (data.sensors != undefined) {
		var nonFanSensors = data.sensors.filter(function (s) {
			return s.valueType !== 'FanSpeed';
		});
		var tooltip = '';
		// Tooltip shows all sensors (including fans)
		data.sensors.forEach(function (e) {
			var tv = e.formatted;
			if (
				e.valueType === 'Temperature' &&
				typeof temperatureUnit !== 'undefined' &&
				temperatureUnit
			) {
				tv = (parseFloat(e.value) * 1.8 + 32).toFixed(2) + '&deg;F';
			}
			tooltip += '<b>' + e.label + '</b>' + tv + '<br/>';
		});
		// The tooltip text changes on every refresh.  Bootstrap resolves a
		// function title at show time, so stash the current text here and the
		// tooltip objects never have to be recreated.  Recreating them is what
		// used to make an open tooltip blink away and back once a second.
		headerCache.SensorTooltip = tooltip;

		// Header rotating display excludes fan speed sensors
		var sensorIcons = [];
		var sensorLabels = [];
		var sensorValues = [];
		nonFanSensors.forEach(function (e) {
			var icon = 'bolt';
			var val = e.formatted;
			if (e.valueType == 'Temperature') {
				icon = 'thermometer-half';
				if (typeof temperatureUnit !== 'undefined' && temperatureUnit) {
					val = parseFloat(val * 1.8 + 32).toFixed(2);
					val += '&deg;F';
				} else {
					val += '&deg;C';
				}
			}
			sensorIcons.push(icon);
			sensorLabels.push(e.label);
			sensorValues.push(e.label + val);
		});

		// Only the readings change from one refresh to the next, so rebuild the
		// spans (and the tooltips attached to them) only when the set of
		// sensors itself changes.
		var structure = sensorIcons.join('|') + '||' + sensorLabels.join('|');
		var values = sensorValues.join('|');
		if (headerCache.Sensors != structure) {
			$('#header_sensors')
				.find('.sensorSpan')
				.each(function () {
					var tip = bootstrap.Tooltip.getInstance(this);
					if (tip) tip.dispose();
				});
			var spans = '';
			for (var i = 0; i < sensorValues.length; i++) {
				spans +=
					'<span class="sensorSpan hiddenSensor" onclick="RotateHeaderSensor(' +
					(i + 1) +
					')" data-sensorcount="' +
					i +
					'"><i class="fas fa-' +
					sensorIcons[i] +
					'"></i><small>' +
					sensorValues[i] +
					'</small></span>';
			}
			$('#header_sensors').html(spans);
			$('#header_sensors')
				.find('.sensorSpan')
				.each(function () {
					new bootstrap.Tooltip(this, {
						placement: 'bottom',
						html: true,
						title: function () {
							return headerCache.SensorTooltip;
						}
					});
				});
			headerCache.Sensors = structure;
			headerCache.SensorValues = values;
			if (sensorValues.length > 1)
				$('#header_sensors').css('cursor', 'pointer');
			if (
				$('#header_sensors').data('defaultsensor') != undefined &&
				Number.isInteger($('#header_sensors').data('defaultsensor'))
			) {
				RotateHeaderSensor($('#header_sensors').data('defaultsensor'));
			} else {
				RotateHeaderSensor(0);
			}
		} else if (headerCache.SensorValues != values) {
			// Same sensors, new readings.  Update the text in place so the
			// tooltips (and which sensor is currently rotated into view) are
			// left alone.
			$('#header_sensors')
				.find('.sensorSpan')
				.each(function (i) {
					if (i < sensorValues.length) {
						$(this).find('small').html(sensorValues[i]);
					}
				});
			headerCache.SensorValues = values;
		}

		// If a tooltip is open right now, refresh its text in place rather than
		// letting it close and reopen.
		$('#header_sensors')
			.find('.sensorSpan[aria-describedby]')
			.each(function () {
				var tip = document.getElementById(
					this.getAttribute('aria-describedby')
				);
				var inner = tip ? tip.querySelector('.tooltip-inner') : null;
				if (inner && inner.innerHTML != tooltip) {
					inner.innerHTML = tooltip;
				}
			});
	}

	if (data.timeStr != undefined && data.dateStr != undefined) {
		var row = '';
		if (
			window.location.href.indexOf('index.php') >= 0 ||
			window.location.href.endsWith('/')
		) {
			row +=
				'<span><small>' +
				data.dateStr +
				'</small><small>' +
				data.timeStrFull +
				'</small></span>';
		} else {
			row +=
				'<span><small>' +
				data.dateStr +
				'</small><small>' +
				data.timeStr +
				'</small></span>';
		}
		row += '<!-- ' + window.location.href + ' -->';

		if (headerCache.Time != row) {
			$('#header_Time').html(row);
			headerCache.Time = row;
		}
	} else {
		$('#header_Time').hide();
	}

	if (data.status_name != undefined) {
		var row = '';
		if (data.status_name == 'playing') {
			var title = 'Playing:\n';
			if (data.current_song != undefined && data.current_song != '') {
				title += data.current_song;
				if (data.current_sequence != undefined && data.current_sequence != '') {
					title += '\n';
				}
			}
			if (data.current_sequence != undefined && data.current_sequence != '') {
				if (data.current_song == undefined || data.current_song == '') {
					title += ': ';
				}
				title += data.current_sequence;
			}
			row =
				'<span title="' +
				title +
				'"><i class="fas fa-play text-success"></i><small>Playing</small></span>';
		} else if (data.status_name == 'testing') {
			row =
				'<span title="Display Testing Active"><i class="fas fa-heart-pulse text-info"></i><small>Testing</small></span>';
		} else if (data.status_name == 'playing media') {
			var mtitle = 'Playing media outside a playlist';
			if (data.current_song != undefined && data.current_song != '') {
				mtitle += ':\n' + data.current_song;
			}
			row =
				'<span title="' +
				mtitle +
				'"><i class="fas fa-music text-success"></i><small>Media</small></span>';
		} else if (data.status_name == 'playing background') {
			var btitle = 'Background audio playing';
			if (data.current_song != undefined && data.current_song != '') {
				btitle += ':\n' + data.current_song;
			}
			row =
				'<span title="' +
				btitle +
				'"><i class="fas fa-music text-info"></i><small>Background</small></span>';
		} else if (data.status_name == 'idle') {
			row =
				'<span title="Idle"><i class="fas fa-pause"></i><small>Idle</small></span>';
		} else if (data.status_name == 'stopped') {
			row =
				'<span title="FPPD Stopped"><i class="fas fa-stop text-danger"></i><small>FPPD Stopped</small></span>';
		}
		if (headerCache.Player != row) {
			$('#header_player').html(row);
			headerCache.Player = row;
		}
	}
	// Render plugin header indicators.  Every field here is plugin-supplied and
	// untrusted, so the elements are built through the DOM rather than
	// concatenated into markup -- see BuildPluginHeaderIndicator above.
	if (data.pluginHeaderIndicators != undefined) {
		var indicators = [];
		data.pluginHeaderIndicators.forEach(function (indicator) {
			if (indicator && indicator.visible) {
				indicators.push(indicator);
			}
		});
		// Keyed off the raw values rather than the rendered markup: the link
		// now lives only in the click closure, so a markup-derived key would
		// miss a link-only change and leave the stale handler attached.
		var indicatorsKey = JSON.stringify(
			indicators.map(function (indicator) {
				return [
					indicator.pluginName,
					indicator.icon,
					indicator.color,
					indicator.tooltip,
					indicator.link,
					indicator.animate
				];
			})
		);
		if (headerCache.PluginIndicators != indicatorsKey) {
			var $indicatorBox = $('#header_plugin_indicators').empty();
			indicators.forEach(function (indicator) {
				$indicatorBox.append(BuildPluginHeaderIndicator(indicator));
			});
			headerCache.PluginIndicators = indicatorsKey;
		}
	}

	if (data.mode_name != undefined) {
		$('#fppModeDropdownButtonModeText').html(
			data.mode_name == 'player' ? 'Player' : data.mode_name
		);
	}

	// advancedView comes only from the PHP status poll, never from the WebSocket
	// push, so it is legitimately absent on every snapshot that arrives before
	// the first poll lands.  Leave the tooltip at whatever the last poll set
	// rather than throwing (or clearing it once a second).
	if (data.advancedView && data.advancedView.HostDescription) {
		var hostDetails =
			'Host: ' +
			data.advancedView.HostName +
			'<br/>Desc: ' +
			data.advancedView.HostDescription;
		if (headerCache.HostDetails != hostDetails) {
			$('.headerHostName>span').attr('title', hostDetails);
			headerCache.HostDetails = hostDetails;
		}
	}

	// restartFlag/rebootFlag are deliberately NOT read from lastStatusJSON here.
	// This function runs on every status change, including each WebSocket push
	// (~1/s), but those two fields only ever arrive from the PHP status poll --
	// so reading them out of the merged cache re-applies a value up to
	// gblSystemAugRefreshSeconds old at 1Hz.  applyServerRestartRebootFlags()
	// applies them from the response that actually delivered them instead.
}

/*
 * Used to rotate through the sensors in the header bar
 */
function RotateHeaderSensor (goto) {
	var currentDefault = $('#header_sensors').data('defaultsensor');

	var current = $('#header_sensors').find(
		"[data-sensorcount='" + (goto - 1) + "']"
	);
	var next = $('#header_sensors').find("[data-sensorcount='" + goto + "']");
	if (next.length == 0)
		next = $('#header_sensors').find("[data-sensorcount='0']");
	current.addClass('hiddenSensor');
	next.removeClass('hiddenSensor');

	// Save setting
	if (currentDefault == goto) return;
	$.put('api/settings/currentHeaderSensor', goto);
	$('#header_sensors').data('defaultsensor', goto);
}

function PreviewStatistics () {
	if ($('#statsPreviewPopup').length == 0) {
		var dialogHTML =
			"<div id='statsPreviewPopup'><pre><div id='statsPreviewDiv'></div></pre></div>";
		$(dialogHTML).appendTo('body');
	}

	$('#statsPreviewDiv').html('Loading...');
	$('#statsPreviewPopup').fppDialog({
		width: 900,
		title: 'Statistics Preview',
		modal: true
	});
	$('#statsPreviewPopup').fppDialog('moveToTop');
	$('#statsPreviewDiv').load('api/statistics/usage');
}

function isValidIpAddress (ip) {
	if (ip == '') {
		return false;
	}
	return /^(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/.test(
		ip
	);
}

// Source: https://github.com/miguelmota/is-valid-hostname/blob/master/index.js
// License MIT: https://raw.githubusercontent.com/miguelmota/is-valid-hostname/master/LICENSE
function isValidHostname (value) {
	if (typeof value !== 'string') return false;

	const validHostnameChars = /^[a-zA-Z0-9-.]{1,253}\.?$/g;
	if (!validHostnameChars.test(value)) {
		return false;
	}

	if (value.endsWith('.')) {
		value = value.slice(0, value.length - 1);
	}

	if (value.length > 253) {
		return false;
	}

	const labels = value.split('.');

	const isValid = labels.every(function (label) {
		const validLabelChars = /^([a-zA-Z0-9-]+)$/g;

		const validLabel =
			validLabelChars.test(label) &&
			label.length < 64 &&
			!label.startsWith('-') &&
			!label.endsWith('-');

		return validLabel;
	});

	return isValid;
}

function bytesToHuman (bytes) {
	size = bytes;
	if (size < 1024) {
		return '' + Math.round(size) + 'B';
	}
	size = size / 1024;
	if (size < 1024) {
		return '' + Math.round(size) + 'KB';
	}
	size = size / 1024;
	if (size < 1024) {
		return '' + Math.round(size) + 'MB';
	}
	size = size / 1024;
	if (size < 1024) {
		return '' + Math.round(size) + 'GB';
	}
	size = size / 1024;
	return '' + Math.round(size) + 'TB';
}

function CreateSelect (
	optionArray = ['No Options'],
	currentValue,
	selectTitle,
	dropDownTitle,
	selectClass,
	onselect = ''
) {
	var result = selectTitle != '' ? selectTitle + ':&nbsp;' : '';

	result += "<select class='" + selectClass + "'";
	if (onselect != '') {
		result += " onchange='" + onselect + "'";
	}
	result += '>';

	if (currentValue === '' && !('' in optionArray))
		result += "<option value=''>" + dropDownTitle + '</option>';

	var found = 0;
	if (optionArray instanceof Map) {
		optionArray.forEach((key, value) => {
			result += "<option value='" + value + "'";

			if (currentValue == value) {
				result += ' selected';
				found = 1;
			}

			result += '>' + key + '</option>';
		});
	} else {
		for (var key in optionArray) {
			result += "<option value='" + key + "'";

			if (currentValue == key) {
				result += ' selected';
				found = 1;
			}

			result += '>' + optionArray[key] + '</option>';
		}
	}

	if (currentValue != '' && found == 0) {
		result +=
			"<option value='" +
			currentValue +
			"' selected>" +
			currentValue +
			'</option>';
	}
	result += '</select>';

	return result;
}

function DeviceSelect (
	deviceArray = ['No Devices'],
	currentValue,
	onselect = ''
) {
	return CreateSelect(
		deviceArray,
		currentValue,
		'Port',
		'-- Port --',
		'device',
		onselect
	);
}

function checkScrollTopButton () {
	var limit = 40;
	var btn = $('#scrollTopButton');

	if (
		document.body.scrollTop > limit ||
		document.documentElement.scrollTop > limit
	) {
		if (!btn.hasClass('scrollTopButtonShowing')) {
			btn.addClass('scrollTopButtonShowing');
			btn.removeClass('scrollTopButtonHidden');
		}
	} else {
		if (!btn.hasClass('scrollTopButtonHidden')) {
			btn.removeClass('scrollTopButtonShowing');
			btn.addClass('scrollTopButtonHidden');
		}
	}
}

function scrollToTop () {
	document.body.scrollTop = 0;
	document.documentElement.scrollTop = 0;
	document.scrollingElement.scrollTop = 0;
}

/**
 * Uses the unified update status API to check for updates.
 * Updates global FPP_UPDATE_STATE and fires 'fpp:updateStatusChanged' event.
 * Supports test mode via URL param: ?test=branch|commit|both|uptodate
 */
function checkForFppUpdate () {
	var testMode = new URLSearchParams(window.location.search).get('test');
	var apiUrl = 'api/system/updateStatus';
	if (testMode) {
		apiUrl += '?test=' + testMode;
		console.log('Update check using test mode: ' + testMode);
	}
	$.get(apiUrl)
		.done(function (data) {
			if (data.status !== 'OK') {
				console.log('Update status API returned error');
				return;
			}

			// Update global state
			FPP_UPDATE_STATE.branchUpgradeAvailable = data.branchUpgradeAvailable;
			FPP_UPDATE_STATE.branchUpgradeTarget = data.branchUpgradeTarget;
			FPP_UPDATE_STATE.branchUpgradeVersion = data.branchUpgradeVersion;
			FPP_UPDATE_STATE.isMajorVersionUpgrade =
				data.isMajorVersionUpgrade || false;
			FPP_UPDATE_STATE.commitUpdateAvailable = data.commitUpdateAvailable;
			FPP_UPDATE_STATE.remoteCommit = data.remoteCommit;
			FPP_UPDATE_STATE.currentBranch = data.currentBranch;
			FPP_UPDATE_STATE.localCommit = data.localCommit;
			FPP_UPDATE_STATE.versionUnknown = data.versionUnknown || false;
			FPP_UPDATE_STATE.isEndOfLife = data.isEndOfLife || false;
			FPP_UPDATE_STATE.latestMajorVersion = data.latestMajorVersion || 0;
			FPP_UPDATE_STATE.checked = true;

			// Update navbar indicator
			updateNavbarUpdateIndicator();

			// Fire event for other components (menu banner, upgrade page)
			$(document).trigger('fpp:updateStatusChanged', [FPP_UPDATE_STATE]);
		})
		.fail(function () {
			console.log('Failed to check for updates via API');

			// A cleaned/never-built tree reports an "Unknown" branch, which matches
			// no remote branch and makes every release branch look like an
			// available upgrade. There is nothing to compare, so don't guess.
			if (
				typeof FPP_BRANCH === 'undefined' ||
				!FPP_BRANCH ||
				FPP_BRANCH === 'Unknown'
			) {
				FPP_UPDATE_STATE.versionUnknown = true;
				return;
			}

			// Fallback to legacy fppstats check for navbar only
			const epochTimeMilliseconds = Date.now();
			$.get(
				'https://fppstats.falconchristmas.com/api/fpp_commits?v=' +
					epochTimeMilliseconds
			)
				.done(function (data) {
					let remote_commit = '';
					let latest_non_master = '';
					let latest_non_master_epoch = 0;

					data.branches.forEach(branch => {
						if (branch.name === FPP_BRANCH) {
							remote_commit = branch.commit.sha;
						}
						if (branch.name != 'master' && /^v\d/.test(branch.name)) {
							var bn = parseFloat(branch.name.substr(1));
							if (
								bn >= FPP_VERSION_FLOAT &&
								branch.commit.date_epoch > latest_non_master_epoch
							) {
								latest_non_master = branch.name;
								latest_non_master_epoch = branch.commit.date_epoch;
							}
						}
					});

					// Update global state from legacy data
					if (
						FPP_BRANCH != 'master' &&
						FPP_BRANCH != latest_non_master &&
						latest_non_master
					) {
						FPP_UPDATE_STATE.branchUpgradeAvailable = true;
						FPP_UPDATE_STATE.branchUpgradeTarget = latest_non_master;
						FPP_UPDATE_STATE.branchUpgradeVersion = latest_non_master.replace(
							/^v/,
							''
						);

						// Check if this is a major version upgrade
						var currentMatch = FPP_BRANCH.match(/^v?(\d+)/);
						var targetMatch = latest_non_master.match(/^v?(\d+)/);
						if (currentMatch && targetMatch) {
							FPP_UPDATE_STATE.isMajorVersionUpgrade =
								parseInt(targetMatch[1]) > parseInt(currentMatch[1]);
						}
					}
					if (remote_commit && !remote_commit.startsWith(FPP_LOCAL_COMMIT)) {
						FPP_UPDATE_STATE.commitUpdateAvailable = true;
						FPP_UPDATE_STATE.remoteCommit = remote_commit;
					}
					FPP_UPDATE_STATE.currentBranch = FPP_BRANCH;
					FPP_UPDATE_STATE.localCommit = FPP_LOCAL_COMMIT;
					FPP_UPDATE_STATE.checked = true;

					updateNavbarUpdateIndicator();
					$(document).trigger('fpp:updateStatusChanged', [FPP_UPDATE_STATE]);
				})
				.fail(function () {
					console.log(
						'Failed to check for updates. Assuming no internet access'
					);
				});
		});
}

/**
 * Update the navbar update indicator based on FPP_UPDATE_STATE
 */
function updateNavbarUpdateIndicator () {
	let msg = '';

	if (FPP_UPDATE_STATE.branchUpgradeAvailable) {
		// Branch upgrade takes priority
		msg = 'Upgrade to ' + FPP_UPDATE_STATE.branchUpgradeTarget + ' available';
	} else if (FPP_UPDATE_STATE.commitUpdateAvailable) {
		msg = 'Software update available';
	}

	if (msg !== '') {
		$('#navbarUpdateAvailIcon').attr('title', msg);
		$('#navbarUpdateAvail').show();
	} else {
		$('#navbarUpdateAvail').hide();
	}
}
