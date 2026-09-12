/*
 * Plugin privacy lights.
 *
 * The rules and wording here are published for plugin authors in the
 * fpp-plugin-Template repository: PLUGININFO_FORMAT.md (`privacy` section)
 * and PLUGIN_GUIDELINES.md §14.15 "How the install dialog is coloured".
 * "Guidelines" below refers to that section.
 *
 * Turns the `privacy` block of a pluginInfo.json into the six lights the
 * install dialog and the plugin detail modal show as chips, and the plugin
 * cards as dots: Sends data, Collects data, Camera & mic, Remote access,
 * System changes and Can it be checked?. Each chip is green, amber or red,
 * and its text states the finding ("Sends when enabled") rather than naming
 * the light, so a row of greens needs no legend; only an undeclared chip,
 * which has no finding, names its light ("Sends data: not declared").
 *
 * The colour is computed HERE, from the declaration, never chosen by the
 * author. The table that decides it is RULES below, one entry per cell of
 * the published table (guidelines), in the order they are tested: every red
 * rule for a light first, then every amber one; the first match wins and
 * green is what is left when nothing matched.
 *
 * Every sentence shown is FPP's own (guidelines). The author's text only ever
 * appears as a fragment inserted into a fixed template -- `to`, `what`,
 * `why`, `where`, `systemChanges[].what` -- plus the `summary` paragraph and
 * the `other` line, all escaped.
 *
 * Tolerance: a declaration is self-described and may come from a newer FPP
 * than this one. Anything missing is treated as empty, a wrongly typed value
 * is treated as empty or false, unknown keys are ignored, and nothing here
 * ever refuses a block. Strict validation lives in the fpp-data listing
 * check, not in the player.
 *
 * Plain script, no dependencies: plugins.php loads it before its own inline
 * code.
 */
var FPPPluginPrivacy = (function () {
	"use strict";

	// A plugin with no `privacy` block is grey ("Not declared") before this
	// date and red on or after it (guidelines). The same constant lives in
	// fpp-data's lint_plugin.py.
	var PRIVACY_DECLARATION_REQUIRED_FROM = "2027-01-01";

	// The six lights, in display order, with the chip text for each colour
	// (guidelines). Level 'n' (undeclared) has the one label below for every
	// light; chipText() prefixes the light's name to it.
	//
	// Attribution: nothing here is verified by FPP. A declaration AGAINST the
	// author's interest (amber, red) is stated as a finding; a FAVOURABLE one
	// (green) is attributed to the author ("No sending declared"), so a row of
	// greens reads as what the author says, not as a clean bill from FPP.
	var LIGHTS = [
		{
			id: "send",
			name: "Sends data",
			g: "No sending disclosed",
			a: "Sends when enabled",
			r: "Sends identifying data",
		},
		{
			id: "collect",
			name: "Collects data",
			g: "No collection disclosed",
			a: "Collects, with limits",
			r: "Collects visitor data",
		},
		{
			id: "camera",
			name: "Camera & mic",
			g: "No camera or mic disclosed",
			a: "Camera, not stored",
			r: "Records people",
		},
		{
			id: "remote",
			name: "Remote access",
			g: "No remote access disclosed",
			a: "Remote access, off by default",
			r: "Can be reached from the internet",
		},
		{
			id: "system",
			name: "System changes",
			g: "No system changes disclosed",
			a: "Changes this device",
			r: "Changes this device permanently",
		},
		{
			id: "code",
			name: "Can it be checked?",
			g: "Author says all its software can be checked",
			a: "Downloads extra software",
			r: "Includes software that can't be checked",
		},
	];
	var UNDECLARED_LABEL = "Not disclosed";

	// Guidelines: a `to` that contains a dotted domain is a hostname; anything
	// else is an operator-entered phrase ("your MQTT broker") or a broadcast.
	var HOST_RE = /\b[a-z0-9-]+(\.[a-z0-9-]+)*\.[a-z]{2,}\b/i;
	// Guidelines: a `what` that names a hardware identifier.
	var HW_ID_RE =
		/\b(serial( number)?|mac( address)?|uuid|hardware id|hwid|hw id)\b/i;

	var VISITOR_ABOUT = { visitors: 1, "passers-by": 1 };
	var ALWAYS_RED_ABOUT = { "third-parties": 1, performers: 1 };
	var TRACKING_TYPES = { "face-tracking": 1, "body-tracking": 1 };
	var REMOTE_AMBER = { lan: 1, "internet-authenticated": 1 };
	var REMOTE_RED = { "internet-open": 1, "exposes-fpp": 1, tunnel: 1 };
	var SYSTEM_RED_KINDS = {
		"package-source": 1,
		tunnel: 1,
		"reads-core-credentials": 1,
		privilege: 1,
	};

	// Guidelines: the fixed pieces of the detail lines.
	var ABOUT_LABEL = {
		operator: "you",
		household: "your household",
		visitors: "visitors",
		"passers-by": "passers-by",
		"third-parties": "third parties",
		performers: "performers",
	};
	var REMOTE_LINE = {
		none: "Author says no way in from outside this device.",
		lan: "Listens on your network, off until you enable it.",
		"internet-authenticated": "Can be reached from the internet with a login.",
		"internet-open": "Can be reached from the internet with no login.",
		"exposes-fpp": "Puts FPP's own pages on the internet.",
		tunnel: "Bundles a tunnel that lets an outside service reach this device.",
	};
	// Sensor types as a reader who has never heard of GPIO sees them.
	var SENSOR_LABEL = {
		camera: "Camera",
		microphone: "Microphone",
		"face-tracking": "Face tracking",
		"body-tracking": "Body tracking",
		presence: "Presence sensor",
		rfid: "RFID reader",
		"gpio-input": "Button or sensor wired to the player",
	};
	var KIND_LABEL = {
		service: "Service",
		network: "Network",
		"core-settings": "FPP settings",
		download: "Download",
		"package-source": "Package source",
		tunnel: "Tunnel",
		"reads-core-credentials": "Reads FPP credentials",
		privilege: "Privilege",
	};
	var GREEN_LINE = {
		send: "Author says it talks only to FPP on this device.",
		collect: "Author says it keeps only your own settings.",
		camera: "Author says no camera or microphone.",
		remote: REMOTE_LINE.none,
		system: "Author says nothing outside its own directory.",
		code: "Author says everything that runs is in the repository or comes from a public package source such as apt, pip or npm.",
	};
	var UNDECLARED_LINE = "The author has not disclosed this.";
	var UNDECLARED_EXPLAIN =
		"Every FPP plugin has been required to describe what it does with data since 1 January 2027. This one has not.";
	var BLACK_BOX_EXPLAIN =
		"Almost every FPP plugin is made entirely of code anyone can read. Part of this one is not, so nobody — not FPP, not you — " +
		"can check what that part does, and the disclosure below cannot be checked for it either.";

	function arr(v) {
		return Array.isArray(v) ? v : [];
	}
	function obj(v) {
		return v && typeof v === "object" && !Array.isArray(v) ? v : {};
	}
	function str(v) {
		return v == null || typeof v === "object" ? "" : String(v);
	}
	function lower(v) {
		return str(v).toLowerCase().trim();
	}
	function truthy(v) {
		return v === true || v === 1 || lower(v) === "true" || lower(v) === "yes";
	}
	function esc(s) {
		return str(s)
			.replace(/&/g, "&amp;")
			.replace(/</g, "&lt;")
			.replace(/>/g, "&gt;")
			.replace(/"/g, "&quot;");
	}
	function cap(s) {
		return s ? s.charAt(0).toUpperCase() + s.slice(1) : s;
	}
	// Lower-case a leading capital so an author's "To control the projector"
	// reads on after the dash like the template's "to ..."; a leading
	// initialism ("FPP ...") is left alone.
	function uncap(s) {
		return s && s.length > 1 && s.charAt(1) === s.charAt(1).toLowerCase() ?
				s.charAt(0).toLowerCase() + s.slice(1)
			:	s;
	}
	// An address the author prefixed with http:// (the "unencrypted" marker
	// in the schema) is shown without the scheme, which is not a word.
	function unhttp(to) {
		return /^http:\/\//i.test(to) ? to.slice(7) : to;
	}
	function isHostname(to) {
		return HOST_RE.test(to);
	}

	// Fill in every key so the rules below never test for presence. Wrong
	// types become empty; unknown keys are dropped.
	function normalise(privacy) {
		var src = obj(privacy);
		var p = {
			summary: str(src.summary).trim(),
			sends: [],
			collects: [],
			sensors: [],
			remoteAccess: lower(src.remoteAccess) || "none",
			systemChanges: [],
			closedCode: truthy(src.closedCode),
			other: str(src.other).trim(),
		};
		arr(src.sends).forEach(function (s) {
			s = obj(s);
			p.sends.push({
				to: str(s.to).trim(),
				what: str(s.what).trim(),
				why: str(s.why).trim(),
				alwaysOn: truthy(s.alwaysOn),
			});
		});
		arr(src.collects).forEach(function (c) {
			c = obj(c);
			var days =
				c.keptDays == null || c.keptDays === "" ? null : Number(c.keptDays);
			p.collects.push({
				what: str(c.what).trim(),
				about: lower(c.about),
				keptDays: days === null || isNaN(days) ? null : days,
				canDelete: truthy(c.canDelete),
				where: str(c.where).trim(),
			});
		});
		arr(src.sensors).forEach(function (s) {
			s = obj(s);
			p.sensors.push({ type: lower(s.type), stored: truthy(s.stored) });
		});
		arr(src.systemChanges).forEach(function (c) {
			c = obj(c);
			p.systemChanges.push({ kind: lower(c.kind), what: str(c.what).trim() });
		});
		return p;
	}

	// THE TABLE (guidelines). Red rules first, then amber; first match wins;
	// green when nothing matched. The comment on each rule is the published
	// wording; `test` is what it means in the block. A rule may carry its own
	// chip `label`; otherwise the light's label for that level is used.
	var RULES = {
		send: [
			{
				// a hardware identifier
				level: "r",
				test: function (p) {
					return p.sends.some(function (s) {
						return HW_ID_RE.test(s.what);
					});
				},
			},
			{
				// an always-on send to a hostname
				level: "r",
				label: "Sends to the internet on its own",
				test: function (p) {
					return p.sends.some(function (s) {
						return s.alwaysOn && isHostname(s.to);
					});
				},
			},
			{
				// over plain http:// to a hostname (unencrypted on a LAN address
				// is ordinary for projectors, brokers and players: amber below)
				level: "r",
				label: "Sends unencrypted to the internet",
				test: function (p) {
					return p.sends.some(function (s) {
						return /^http:\/\//i.test(s.to) && isHostname(s.to);
					});
				},
			},
			{
				// any send
				level: "a",
				test: function (p) {
					return p.sends.length > 0;
				},
			},
		],
		collect: [
			{
				// visitors or passers-by with no time limit
				level: "r",
				test: function (p) {
					return p.collects.some(function (c) {
						return VISITOR_ABOUT.hasOwnProperty(c.about) && c.keptDays === null;
					});
				},
			},
			{
				// third parties or performers
				level: "r",
				test: function (p) {
					return p.collects.some(function (c) {
						return ALWAYS_RED_ABOUT.hasOwnProperty(c.about);
					});
				},
			},
			{
				// any collection
				level: "a",
				test: function (p) {
					return p.collects.length > 0;
				},
			},
		],
		camera: [
			{
				// stored
				level: "r",
				test: function (p) {
					return p.sensors.some(function (s) {
						return s.stored;
					});
				},
			},
			{
				// face or body tracking
				level: "r",
				test: function (p) {
					return p.sensors.some(function (s) {
						return TRACKING_TYPES.hasOwnProperty(s.type);
					});
				},
			},
			{
				// a camera or microphone, not stored
				level: "a",
				test: function (p) {
					return p.sensors.some(function (s) {
						return s.type === "camera" || s.type === "microphone";
					});
				},
			},
			{
				// any other sensor (presence, RFID, GPIO input), not stored
				level: "a",
				label: "Uses a sensor, not stored",
				test: function (p) {
					return p.sensors.length > 0;
				},
			},
		],
		remote: [
			{
				// internet-open, exposes-fpp or tunnel
				level: "r",
				test: function (p) {
					return REMOTE_RED.hasOwnProperty(p.remoteAccess);
				},
			},
			{
				// lan or internet-authenticated
				level: "a",
				test: function (p) {
					return REMOTE_AMBER.hasOwnProperty(p.remoteAccess);
				},
			},
			// A value this FPP does not know is a declaration of something, so
			// it is never shown as "none".
			{
				// an unknown remoteAccess value
				level: "a",
				test: function (p) {
					return p.remoteAccess !== "none";
				},
			},
		],
		system: [
			{
				// package-source, tunnel, reads-core-credentials or privilege
				level: "r",
				test: function (p) {
					return p.systemChanges.some(function (c) {
						return SYSTEM_RED_KINDS.hasOwnProperty(c.kind);
					});
				},
			},
			{
				// any change
				level: "a",
				test: function (p) {
					return p.systemChanges.length > 0;
				},
			},
		],
		code: [
			{
				// closedCode
				level: "r",
				test: function (p) {
					return p.closedCode;
				},
			},
			{
				// a download kind
				level: "a",
				test: function (p) {
					return p.systemChanges.some(function (c) {
						return c.kind === "download";
					});
				},
			},
		],
	};

	function decide(id, p) {
		var rules = RULES[id];
		for (var i = 0; i < rules.length; i++) {
			if (rules[i].test(p))
				return { level: rules[i].level, label: rules[i].label };
		}
		return { level: "g" };
	}

	// One entry of a detail line: bold key fact + italic muted tag on the
	// first line, the rest on a second line. Author fragments arrive escaped.
	function item(head, tag, body) {
		var h = "<b>" + head + "</b>";
		if (tag) h += ' <i class="text-secondary">&middot; ' + esc(tag) + "</i>";
		if (body) h += "<br>" + body;
		return h;
	}

	// The default-visible detail under a chip (guidelines): FPP's template with
	// the author's fragments inserted, as a list of HTML entries. One entry
	// renders inline; several render as bullets (lineHtml), never run
	// together in one paragraph. A light with nothing to report is the
	// author's word and says so.
	function lineFor(id, p) {
		var out = [];
		switch (id) {
			case "send":
				p.sends.forEach(function (s) {
					var to = s.to || "an unnamed destination";
					var shown = unhttp(to);
					var tag = s.alwaysOn ? "always on" : "only when you use that feature";
					if (shown !== to) tag += ", unencrypted";
					out.push(
						item(
							esc(isHostname(shown) ? shown : cap(shown)),
							tag,
							esc(cap(s.what || "data")) +
								" &mdash; " +
								esc(uncap(s.why || "no purpose given")) +
								".",
						),
					);
				});
				break;
			case "collect":
				p.collects.forEach(function (c) {
					var about =
						ABOUT_LABEL.hasOwnProperty(c.about) ?
							ABOUT_LABEL[c.about]
						:	c.about || "you";
					var tag =
						c.keptDays !== null ?
							"kept " + c.keptDays + " days"
						:	"no time limit";
					if (!c.canDelete) tag += ", no delete control";
					out.push(
						item(
							esc(cap(c.what || "data")),
							tag,
							"About " +
								esc(about) +
								", in " +
								esc(c.where || "an unnamed place") +
								".",
						),
					);
				});
				break;
			case "camera":
				p.sensors.forEach(function (s) {
					out.push(
						item(
							esc(
								SENSOR_LABEL.hasOwnProperty(s.type) ?
									SENSOR_LABEL[s.type]
								:	cap(s.type.replace(/-/g, " ")) || "Sensor",
							),
							s.stored ? "recordings kept" : "nothing kept",
							"",
						),
					);
				});
				break;
			case "remote":
				if (REMOTE_LINE.hasOwnProperty(p.remoteAccess))
					out.push(esc(REMOTE_LINE[p.remoteAccess]));
				else
					out.push(
						"Remote access disclosed as &quot;" +
							esc(p.remoteAccess) +
							"&quot;.",
					);
				break;
			case "system":
				p.systemChanges.forEach(function (c) {
					var kind =
						KIND_LABEL.hasOwnProperty(c.kind) ?
							KIND_LABEL[c.kind]
						:	cap(c.kind) || "Change";
					out.push(
						item(esc(kind), "", esc(cap(c.what || "not described")) + "."),
					);
				});
				break;
			case "code":
				if (p.closedCode)
					out.push("Includes software whose source is not available.");
				p.systemChanges.forEach(function (c) {
					// The author's line already says what is downloaded ("downloads
					// and builds ..."); prefixing "Downloads" doubled the verb.
					if (c.kind === "download")
						out.push(esc(cap(c.what || "Downloads extra software")) + ".");
				});
				break;
		}
		if (!out.length) out.push(esc(GREEN_LINE[id]));
		return out;
	}

	// Entries -> HTML: inline when there is one, bullets when there are more.
	function lineHtml(entries) {
		if (entries.length === 1) return entries[0];
		return (
			'<ul class="mb-0 mt-1 ps-3">' +
			entries
				.map(function (e) {
					return '<li class="mb-1">' + e + "</li>";
				})
				.join("") +
			"</ul>"
		);
	}

	// Headline, by the published rule (guidelines), first match wins. Red
	// headlines state the finding; the amber and green ones are attributed
	// to the author. `byId` maps light id -> level, `labelById` -> chip text.
	function headlineFor(byId, labelById, p) {
		if (byId.code === "r")
			return {
				text: "Part of this plugin is a black box",
				level: "r",
				explain: BLACK_BOX_EXPLAIN,
			};
		if (byId.collect === "r" || byId.camera === "r")
			return { text: "Handles other people's data", level: "r" };
		if (byId.remote === "r")
			return { text: "Can be reached from the internet", level: "r" };
		if (byId.send === "r") return { text: labelById.send, level: "r" };
		if (byId.system === "r")
			return { text: "Changes this device permanently", level: "r" };
		if (p.sends.length) {
			var allLocal = p.sends.every(function (s) {
				return !isHostname(s.to);
			});
			return {
				text:
					allLocal ?
						"Author says it talks to devices on your network"
					:	"Author says it talks to online services",
				level: "a",
			};
		}
		return { text: "Author says it runs on this device only", level: "g" };
	}

	// YYYY-MM-DD of the player's current local date, comparable as a string.
	function todayString() {
		return new Date().toLocaleDateString("en-CA");
	}

	/**
	 * Evaluate a pluginInfo.json `privacy` block.
	 *
	 * opts.unreviewed: the plugin was loaded from a pasted URL rather than
	 * the listing, so the block was never checked against the code; the
	 * label line says so.
	 *
	 * Each light carries `entries` (HTML, one per declared item) beside
	 * `label` and `level`.
	 * Returns { declared, unreviewed, lights[], headline, red, summary,
	 *           other, raw, normalised }.
	 */
	function evaluate(privacy, opts) {
		opts = opts || {};
		var declared = !!(
			privacy &&
			typeof privacy === "object" &&
			!Array.isArray(privacy)
		);
		var overdue =
			!declared && todayString() >= PRIVACY_DECLARATION_REQUIRED_FROM;
		var p = normalise(privacy);
		var lights = [];
		var byId = {};
		var labelById = {};
		var red = overdue;
		LIGHTS.forEach(function (L) {
			var d = declared ? decide(L.id, p) : { level: overdue ? "r" : "n" };
			var level = d.level;
			var label = declared ? d.label || L[level] : UNDECLARED_LABEL;
			byId[L.id] = level;
			labelById[L.id] = label;
			if (declared && level === "r") red = true;
			lights.push({
				id: L.id,
				name: L.name,
				level: level,
				label: label,
				entries: declared ? lineFor(L.id, p) : [esc(UNDECLARED_LINE)],
				declared: declared,
			});
		});
		var headline;
		if (declared) headline = headlineFor(byId, labelById, p);
		else if (overdue)
			headline = {
				text: "No privacy disclosure",
				level: "r",
				explain: UNDECLARED_EXPLAIN,
			};
		else headline = { text: "No privacy disclosure", level: "n" };
		return {
			declared: declared,
			unreviewed: !!opts.unreviewed,
			lights: lights,
			headline: headline,
			red: red,
			summary: p.summary,
			other: p.other && lower(p.other) !== "none" ? p.other : "",
			raw: declared ? privacy : null,
			normalised: p,
		};
	}

	/**
	 * The install button for a result (guidelines): text and Bootstrap class
	 * from the worst finding, same priority as the headline. The caller
	 * still forces "Install anyway"/btn-warning when its own warnings fire.
	 */
	function installButtonFor(result) {
		var byId = {};
		var anyAmber = false;
		result.lights.forEach(function (l) {
			byId[l.id] = l.level;
			if (l.level === "a") anyAmber = true;
		});
		if (!result.declared)
			return { text: "Install, no disclosure", cls: "btn-danger" };
		if (byId.code === "r")
			return { text: "Install, black box included", cls: "btn-danger" };
		if (byId.collect === "r" || byId.camera === "r")
			return { text: "Install, handles others' data", cls: "btn-danger" };
		if (byId.remote === "r")
			return { text: "Install, opens FPP to internet", cls: "btn-danger" };
		if (byId.send === "r")
			return { text: "Install, sends data out", cls: "btn-danger" };
		if (byId.system === "r")
			return { text: "Install, permanent changes", cls: "btn-danger" };
		if (anyAmber || result.normalised.sends.length)
			return { text: "Install anyway", cls: "btn-warning" };
		return { text: "Install", cls: "btn-success" };
	}

	// ---- Rendering ---------------------------------------------------------
	//
	// Bootstrap 5.3 utilities only (see .claude/FRONTEND-GUIDELINES.md): every
	// colour is a theme token (text-*-emphasis on bg-*-subtle), so dark mode
	// needs nothing of its own, and no rule of its own in fpp.css.

	// Level -> Bootstrap contextual name. Grey ('n') is "nothing declared"
	// before the deadline.
	var LEVEL_THEME = { g: "success", a: "warning", r: "danger", n: "secondary" };

	// The coloured dot: a Font Awesome circle in currentColor. Green is
	// hollow (the author's word, not a tick), amber and red are filled, and
	// grey is a question mark so "nothing declared" does not look like a green.
	function dotHtml(theme, extra) {
		var cls = extra ? " " + extra : "";
		if (theme === "secondary")
			return (
				'<i class="far fa-circle-question fa-xs' +
				cls +
				'" aria-hidden="true"></i>'
			);
		var filled = theme === "warning" || theme === "danger";
		return (
			'<i class="' +
			(filled ? "fas" : "far") +
			" fa-circle fa-xs" +
			cls +
			'" aria-hidden="true"></i>'
		);
	}

	// A declared chip's text names the finding ("Sends when enabled") and so
	// implies the light. An undeclared one has no finding: "Not declared" six
	// times over says nothing, so it carries the light's name instead, with the
	// grey dot and the headline saying why.
	function chipText(l) {
		if (l.declared) return l.label;
		return /\?$/.test(l.name) ?
				l.name + " " + l.label
			:	l.name + ": " + lower(l.label);
	}

	/**
	 * The strip of six chips plus the lines under them. Red lines are always
	 * open; amber and green open on tap (aria-expanded drives it, see bind())
	 * and carry a caret that turns down while open. Undeclared chips, grey or
	 * red, have no line to open.
	 *
	 * opts.prefix: id prefix so several strips can share a page.
	 * opts.compact: dots with the state text as a title, for cards.
	 */
	function stripHtml(result, opts) {
		opts = opts || {};
		var prefix = opts.prefix || "pp";
		var h = "";
		if (opts.compact) {
			var tip =
				result.declared ?
					"Author's disclosure: " + result.headline.text
				:	"No disclosure";
			h +=
				'<span class="d-inline-flex align-items-center gap-1" title="' +
				esc(tip) +
				'">';
			result.lights.forEach(function (l) {
				var theme = LEVEL_THEME[l.level];
				var filled = theme === "warning" || theme === "danger";
				var title = ' title="' + esc(l.name + ": " + l.label) + '"';
				if (theme === "secondary")
					h +=
						'<i class="far fa-circle-question fa-2xs text-secondary"' +
						title +
						"></i>";
				else
					h +=
						'<i class="' +
						(filled ? "fas" : "far") +
						" fa-circle fa-2xs " +
						(filled ? "text-" + theme + "-emphasis" : "text-" + theme) +
						'"' +
						title +
						"></i>";
			});
			return h + "</span>";
		}
		h +=
			'<div class="d-flex flex-wrap gap-2 mt-2" role="group" aria-label="Privacy lights">';
		result.lights.forEach(function (l) {
			var theme = LEVEL_THEME[l.level];
			var open = l.level === "r" && l.declared;
			var id = prefix + "-" + l.id;
			h +=
				'<button type="button" class="pluginPrivacyChip badge rounded-pill border fw-semibold d-inline-flex align-items-center gap-1' +
				" text-" +
				theme +
				"-emphasis bg-" +
				theme +
				"-subtle border-" +
				theme +
				'-subtle"' +
				' data-light="' +
				l.id +
				'" data-level="' +
				l.level +
				'" data-declared="' +
				(l.declared ? "1" : "0") +
				'"' +
				' aria-expanded="' +
				(open ? "true" : "false") +
				'" aria-controls="' +
				id +
				'"' +
				' title="' +
				esc(l.name) +
				'">' +
				dotHtml(theme) +
				esc(chipText(l)) +
				(l.declared && l.level !== "r" ?
					'<i class="fas fa-caret-right fa-xs" aria-hidden="true"></i>'
				:	"") +
				"</button>";
		});
		h +=
			'</div><ul class="list-unstyled d-flex flex-column gap-1 small mt-2 mb-2">';
		result.lights.forEach(function (l) {
			var theme = LEVEL_THEME[l.level];
			var open = l.level === "r" && l.declared;
			// d-none, not the hidden attribute: d-flex would outrank [hidden].
			h +=
				'<li id="' +
				prefix +
				"-" +
				l.id +
				'" class="d-flex gap-2 align-items-start' +
				(open ? "" : " d-none") +
				(open ? " text-danger-emphasis" : "") +
				'">' +
				dotHtml(theme, "mt-1 text-" + theme + "-emphasis") +
				"<span><b>" +
				esc(l.name) +
				(/\?$/.test(l.name) ? "" : ":") +
				"</b> " +
				lineHtml(l.entries) +
				"</span></li>";
		});
		return h + "</ul>";
	}

	// The one-line headline over the strip, tinted by the worst finding, with
	// FPP's explainer under it when the rule has one.
	function headlineHtml(result) {
		var theme = LEVEL_THEME[result.headline.level];
		var h =
			'<div class="p-2 rounded bg-' +
			theme +
			"-subtle text-" +
			theme +
			'-emphasis">' +
			'<div class="d-flex gap-2 align-items-center fw-semibold">' +
			dotHtml(theme) +
			esc(result.headline.text) +
			"</div>";
		if (result.headline.explain)
			h += '<div class="small mt-1">' + esc(result.headline.explain) + "</div>";
		return h + "</div>";
	}

	// The author's summary, then "Full declaration": the free-text `other`
	// and the block as written, so nothing the author declared is hidden
	// even when this FPP does not render a key.
	function detailsHtml(result) {
		if (!result.declared) return "";
		var h = "";
		if (result.summary)
			h += '<p class="small mb-2">' + esc(result.summary) + "</p>";
		var json = JSON.stringify(result.raw, null, 2);
		h +=
			'<details class="small"><summary class="link-primary">Full disclosure</summary>';
		if (result.other)
			h += '<p class="mt-1 mb-1"><b>Other:</b> ' + esc(result.other) + "</p>";
		h +=
			'<pre class="mt-1 mb-0 overflow-auto"><code>' +
			esc(json) +
			"</code></pre></details>";
		return h;
	}

	// Chip tap toggles its line. Red lines stay open (the chip is still
	// focusable so screen readers read the same text). Hover is left to the
	// title attribute; a phone has no hover, so tap is the primary path.
	function bind(container) {
		var root =
			typeof container === "string" ?
				document.getElementById(container)
			:	container;
		if (!root) return;
		var chips = root.querySelectorAll(".pluginPrivacyChip[aria-controls]");
		for (var i = 0; i < chips.length; i++) {
			(function (chip) {
				if (chip.dataset.ppBound) return;
				chip.dataset.ppBound = "1";
				chip.addEventListener("click", function (e) {
					e.stopPropagation();
					var line = document.getElementById(
						chip.getAttribute("aria-controls"),
					);
					if (!line || chip.dataset.declared !== "1") return;
					if (chip.dataset.level === "r") {
						line.classList.remove("d-none");
						chip.setAttribute("aria-expanded", "true");
						return;
					}
					var open = chip.getAttribute("aria-expanded") === "true";
					chip.setAttribute("aria-expanded", open ? "false" : "true");
					line.classList.toggle("d-none", open);
					var caret = chip.querySelector(".fa-caret-right, .fa-caret-down");
					if (caret) {
						caret.classList.toggle("fa-caret-right", open);
						caret.classList.toggle("fa-caret-down", !open);
					}
				});
			})(chips[i]);
		}
	}

	return {
		evaluate: evaluate,
		installButtonFor: installButtonFor,
		stripHtml: stripHtml,
		headlineHtml: headlineHtml,
		detailsHtml: detailsHtml,
		bind: bind,
	};
})();
