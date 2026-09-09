/*
 * Behaviour for the privacy disclosure table, shared by the setup wizard and the
 * Privacy settings page. Markup and data come from privacyTable.inc.
 *
 * The two pages differ in exactly one way: what happens when an answer changes.
 * The wizard holds it until Finish; Settings saves immediately. That is the
 * `commit` callback, and it is the only thing a caller has to supply.
 */
var fppPrivacyTable = {

	// Rows are disclosures, settings are settings, and they do not line up.
	//
	// ShareCrashData is one ordered value, so its three rows are a ladder:
	// allowing a row allows its parents, denying one denies and locks its
	// descendants. Drawn as independent pairs it could express states that do
	// not exist. The four reachable states are 0, 1, 2, 3.
	//
	// Denying the crash rows writes 0 ("keep locally"), never -1: a report is
	// written to the crashes folder either way, which is what the help text
	// promises, so -1 is deliberately unreachable here.
	ROWS: {
		'stats': { setting: 'statsPublish', allow: 'Enabled', deny: 'Disabled' },
		'crash': { setting: 'ShareCrashData', level: 1 },
		'crash-settings': { setting: 'ShareCrashData', level: 2, parent: 'crash' },
		'crash-config': { setting: 'ShareCrashData', level: 3, parent: 'crash-settings' },
		'logos': { setting: 'FetchVendorLogos', allow: '1', deny: '0' },
		'serial': { setting: 'SendVendorSerial', allow: '1', deny: '0', parent: 'logos' }
	},
	CRASH_ROWS: ['crash', 'crash-settings', 'crash-config'],

	cfg: null,
	commit: null,
	touched: {},

	/**
	 * cfg comes from PrivacyTableConfig(); commit(settingsMap) persists.
	 */
	init: function (cfg, commit) {
		this.cfg = cfg;
		this.commit = commit;
		this.touched = {};
		var self = this;

		// Namespaced and removed first: a page that includes this fragment more
		// than once would otherwise stack a second set of handlers, firing the
		// commit twice per change.
		$("input[name^='privacy_']").off('change.fppPrivacy').on('change.fppPrivacy', function () {
			var row = $(this).attr('name').replace(/^privacy_/, '');
			self.touched[row] = true;
			self.applyLadder(row);
			if (self.commit) { self.commit(self.collect()); }
		});
		$('#emailAddress').off('change.fppPrivacy').on('change.fppPrivacy', function () {
			if (self.commit) { self.commit(self.collect()); }
		});
		// One popover at a time, so the disclosures cannot stack on each other.
		$('.privacyHelp').off('click.fppPrivacy').on('click.fppPrivacy', function (e) {
			e.preventDefault();
			var d = self.cfg.help[$(this).data('row')];
			if (d) { DialogOK(d.title, d.body); }
		});

		this.fromSettings();
	},

	get: function (row) {
		return $("input[name='privacy_" + row + "']:checked").val() || '';
	},
	set: function (row, val) {
		if (val === '') {
			$("input[name='privacy_" + row + "']").prop('checked', false);
		} else {
			$("input[name='privacy_" + row + "'][value='" + val + "']").prop('checked', true);
		}
	},

	// A cape's proposed default is the vendor's suggestion, not the user's
	// answer, so answeredness is the settings FILE only.
	answeredInFile: function (key) {
		return this.cfg.answered.indexOf(key) !== -1;
	},

	/**
	 * Enforce the ladder both ways, so an impossible combination cannot be
	 * clicked into being rather than merely rejected afterwards.
	 */
	applyLadder: function (changedRow) {
		var self = this;
		$.each(this.ROWS, function (row, def) {
			if (!def.parent) { return; }
			if (changedRow === row && self.get(row) === 'allow') {
				var p = def.parent;
				while (p) { self.set(p, 'allow'); p = self.ROWS[p].parent; }
			}
		});
		$.each(this.ROWS, function (row, def) {
			if (!def.parent) { return; }
			var blocked = false, p = def.parent;
			while (p) {
				if (self.get(p) === 'deny') { blocked = true; break; }
				p = self.ROWS[p].parent;
			}
			$("input[name='privacy_" + row + "']").prop('disabled', blocked);
			$("tr[data-row='" + row + "']").toggleClass('privacyBlocked', blocked);
			if (blocked) { self.set(row, 'deny'); }
		});
		// Pointless until something is being sent.
		$('#emailRow').toggleClass('d-none', this.get('crash') !== 'allow');
	},

	/**
	 * Rows with no answer. One locked by a denied parent is answered by that
	 * denial and does not count as outstanding.
	 */
	unanswered: function () {
		var self = this, out = [];
		$.each(this.ROWS, function (row) {
			if ($("input[name='privacy_" + row + "']").prop('disabled')) { return; }
			if (self.get(row) === '') { out.push(row); }
		});
		return out;
	},

	/**
	 * Collapse the table back into settings values.
	 */
	collect: function () {
		var self = this, out = {};
		var stats = this.get('stats');
		if (stats !== '') { out['statsPublish'] = this.ROWS['stats'][stats]; }

		// Deepest allowed rung wins; nothing allowed means 0, keep locally.
		var level = 0;
		$.each(this.CRASH_ROWS, function (i, row) {
			if (self.get(row) === 'allow') { level = self.ROWS[row].level; }
		});
		out['ShareCrashData'] = '' + level;

		$.each(['logos', 'serial'], function (i, row) {
			var v = self.get(row);
			if (v !== '') {
				out[row === 'logos' ? 'FetchVendorLogos' : 'SendVendorSerial'] = self.ROWS[row][v];
			}
		});
		out['emailAddress'] = $('#emailAddress').val() || '';
		return out;
	},

	/**
	 * Fill the table in from what the box holds.
	 *
	 * Under a prior-opt-in regime nothing is pre-selected except "send crash
	 * reports", which may start allowed: a stack trace rests on legitimate
	 * interests (Art 6(1)(f)) rather than consent, and the tiers that do need
	 * consent are the two below it. A row already answered in the settings file
	 * keeps its answer either way.
	 */
	fromSettings: function () {
		var self = this;
		var crashLevel = parseInt(this.cfg.current['ShareCrashData'], 10);
		if (isNaN(crashLevel)) { crashLevel = 0; }
		var crashAnswered = this.answeredInFile('ShareCrashData');

		$.each(this.ROWS, function (row, def) {
			if (self.touched[row]) { return; }
			var answered = self.answeredInFile(def.setting);
			if (self.cfg.priorOptIn && !answered) {
				self.set(row, row === 'crash' ? 'allow' : '');
				return;
			}
			if (def.level !== undefined) {
				self.set(row, crashLevel >= def.level ? 'allow' : 'deny');
				return;
			}
			var cur = self.cfg.current[def.setting];
			if (cur === def.allow) {
				self.set(row, 'allow');
			} else if (cur === def.deny) {
				self.set(row, 'deny');
			} else {
				// Neither, which is what statsPublish's shipped "Banner" is: a
				// prompt to decide later rather than a decision. The table has no
				// third state and this is the permissive branch, so it resolves to
				// the shipped intent. Blank would be worse -- it would pass
				// validation with nothing selected and write no value at all.
				self.set(row, 'allow');
			}
		});
		$('#emailAddress').val(this.cfg.current['emailAddress'] || '');
		this.applyLadder(null);
	},

	setAll: function (val) {
		var self = this;
		$.each(this.ROWS, function (row) { self.set(row, val); self.touched[row] = true; });
		this.applyLadder(null);
		if (this.commit) { this.commit(this.collect()); }
	}
};
