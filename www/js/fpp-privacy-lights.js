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
 * System changes and Can it be checked?. Each chip is green, amber or red
 * (a coloured dot before the label; only a red chip is tinted as a whole),
 * and its text states the finding ("Sends when enabled") rather than naming
 * the light, so a row of greens needs no legend; only an undeclared chip,
 * which has no finding, names its light ("Sends data: not declared") and
 * carries a hollow dot.
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
 * than this one. Unknown keys are ignored, unknown values are shown as
 * written, and nothing here ever refuses a block. A key that is missing or
 * of the wrong type is not a statement, though: that light is "not
 * disclosed" (grey), never green, and a block with none of the six keys is
 * undeclared -- the same line the server draws (PluginPrivacyMaterial in
 * api/controllers/plugin.php). Strict validation lives in the fpp-data
 * listing check, not in the player.
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
	// A name whose last label is a reserved LAN suffix (projector.local,
	// nas.lan) or a file extension (remotes.json) is not on the internet.
	var HOST_RE =
		/\b[a-z0-9-]+(\.[a-z0-9-]+)*\.(?!(?:local|localhost|localdomain|lan|home|internal|arpa|json|txt|csv|log|conf|cfg|ini|php|py|sh|js|html|xml|yaml|yml|db)\b)[a-z]{2,}\b(?!\.[a-z0-9-])/i;
	// An IPv4 literal, and the ranges that are not the internet: loopback,
	// RFC1918, link-local, multicast and "this host".
	var IPV4_RE = /\b(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})\b/g;
	function isPrivateIp(a, b) {
		return (
			a === 0 || a === 10 || a === 127 || a >= 224 ||
			(a === 172 && b >= 16 && b <= 31) ||
			(a === 192 && b === 168) ||
			(a === 169 && b === 254)
		);
	}
	// Guidelines: a `what` that names a hardware identifier, in its qualified
	// form only -- "serial port" and "the playlist uuid" identify nobody.
	var HW_ID_RE =
		/\b(serial numbers?|cpu serial|mac addresse?s?|hardware ids?|hwid|hw id|device ids?|(?:device|player|device's|player's) uuids?)\b/i;

	var OTHERS_ABOUT = { visitors: 1, "passers-by": 1, "third-parties": 1, performers: 1 };
	var TRACKING_TYPES = { "face-tracking": 1, "body-tracking": 1 };
	var RECORDING_TYPES = { camera: 1, microphone: 1, "face-tracking": 1, "body-tracking": 1 };
	var REMOTE_AMBER = { lan: 1, "internet-authenticated": 1 };
	var REMOTE_RED = { "internet-open": 1, "exposes-fpp": 1, tunnel: 1 };
	var PERMANENT_KINDS = { "package-source": 1, tunnel: 1 };

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
	// An IPv6 literal, bare or in brackets (with or without a port), that is
	// not loopback, unspecified, link-local, unique-local, multicast or an
	// IPv4-mapped private address. A token is an IPv6 literal when it is hex
	// groups and colons with either "::" or all eight groups, so a clock
	// time ("10:30:00") is not one.
	function isPublicIp6(token) {
		var t = token.replace(/^https?:\/\//i, "");
		var m = /^\[([0-9a-f:.]+)\]/i.exec(t);
		t = (m ? m[1] : t).toLowerCase();
		if (!/^[0-9a-f:.]+$/.test(t) || (t.match(/:/g) || []).length < 2) return false;
		var halves = t.split("::");
		if (halves.length > 2) return false;
		var groups = t.split(/::?/).filter(function (g) { return g; });
		var v4 = /(\d{1,3})\.(\d{1,3})\.\d{1,3}\.\d{1,3}$/.exec(t);
		if (v4) groups.push("");   // the dotted tail counts as two groups
		if (halves.length === 2 ? groups.length > 7 : groups.length !== 8) return false;
		if (t === "::" || t === "::1") return false;
		if (/^fe[89ab]/.test(t) || /^f[cd]/.test(t) || /^ff/.test(t)) return false;
		if (v4 && /^::ffff:/.test(t)) return !isPrivateIp(Number(v4[1]), Number(v4[2]));
		return true;
	}
	// Guidelines: a `to` is on the internet when it names a hostname (above)
	// or a public IP literal. A LAN name, a private address or an
	// operator-entered phrase is not.
	function isInternet(to) {
		if (isHostname(to)) return true;
		var m;
		IPV4_RE.lastIndex = 0;
		while ((m = IPV4_RE.exec(to))) {
			if (!isPrivateIp(Number(m[1]), Number(m[2]))) return true;
		}
		return to.split(/[\s,;()"']+/).some(isPublicIp6);
	}
	// A `to` that names a device on the operator's own network: a name with a
	// reserved LAN suffix (projector.local, nas.lan), localhost, or an IP
	// literal that isInternet() did not accept (private IPv4, link-local or
	// unique-local IPv6). A phrase ("your MQTT broker", "anyone in FM range",
	// "the developer's own server") is not.
	var LAN_NAME_RE = /\b(?:[a-z0-9-]+\.)+(?:local|localhost|localdomain|lan|home|internal|arpa)\b|\blocalhost\b/i;
	var IP6ISH_RE = /^\[?[0-9a-f:.]*:[0-9a-f:.]*:[0-9a-f:.]*\]?(?::\d+)?$/i;
	function isLocalDevice(to) {
		if (isInternet(to)) return false;
		if (LAN_NAME_RE.test(to)) return true;
		IPV4_RE.lastIndex = 0;
		if (IPV4_RE.test(to)) return true;
		return to.split(/[\s,;()"']+/).some(function (t) {
			return IP6ISH_RE.test(t.replace(/^https?:\/\//i, ""));
		});
	}
	// A send whose `what` is the operator's browser's address: the page makes
	// the browser load a file (a CDN script, a font, a badge) from that host.
	// The Template tells authors to write exactly "your browser's address".
	var BROWSER_LOAD_RE = /\byour browser\b/i;
	function isBrowserLoad(s) {
		return BROWSER_LOAD_RE.test(s.what);
	}

	// A scalar key (remoteAccess, closedCode) is present when it holds a
	// value of some kind; null and a nested object are not values.
	function scalar(v) {
		return v != null && typeof v !== "object";
	}

	// Fill in every key so the rules below never test for presence, and
	// record in `has` (by light id) which keys the author actually wrote:
	// a missing or wrongly typed key is filled with empty so the rules run,
	// but it is not a statement, so decide() never turns it green. Unknown
	// keys are dropped.
	function normalise(privacy) {
		var src = obj(privacy);
		var p = {
			summary: str(src.summary).trim(),
			sends: [],
			collects: [],
			sensors: [],
			remoteAccess: scalar(src.remoteAccess) ? lower(src.remoteAccess) || "none" : "none",
			systemChanges: [],
			closedCode: truthy(src.closedCode),
			other: str(src.other).trim(),
			has: {
				send: Array.isArray(src.sends),
				collect: Array.isArray(src.collects),
				camera: Array.isArray(src.sensors),
				remote: scalar(src.remoteAccess),
				system: Array.isArray(src.systemChanges),
				code: scalar(src.closedCode),
			},
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
				// an always-on send to the internet (a hostname or a public
				// IP). A browser-side page asset (what: "your browser's
				// address") is not the plugin sending: it has its own amber
				// rule below and is tested out of the two internet rules
				level: "r",
				label: "Sends to the internet on its own",
				test: function (p) {
					return p.sends.some(function (s) {
						return s.alwaysOn && isInternet(s.to) && !isBrowserLoad(s);
					});
				},
			},
			{
				// over plain http:// to the internet (unencrypted on a LAN
				// address is ordinary for projectors, brokers and players:
				// amber below)
				level: "r",
				label: "Sends unencrypted to the internet",
				test: function (p) {
					return p.sends.some(function (s) {
						return /^http:\/\//i.test(s.to) && isInternet(s.to) && !isBrowserLoad(s);
					});
				},
			},
			{
				// the operator's browser loads a file from a host (a CDN
				// script, a font, a badge): the host sees the browser's
				// address, not data the plugin holds, so amber whatever
				// alwaysOn says, named after the first such host
				level: "a",
				label: function (p) {
					var s = p.sends.filter(isBrowserLoad)[0];
					return "Your browser loads files from " + (unhttp(s.to) || "a web host");
				},
				test: function (p) {
					return p.sends.some(isBrowserLoad);
				},
			},
			{
				// an always-on send to a LAN name or a private address: local
				// traffic is never red, but "Sends when enabled" would be
				// false, so it has its own text
				level: "a",
				label: "Sends on its own to a device on your network",
				test: function (p) {
					return p.sends.some(function (s) {
						return s.alwaysOn && isLocalDevice(s.to);
					});
				},
			},
			{
				// an always-on send to any other phrase (an address the
				// operator enters, a broadcast, a named server, the device
				// itself): the `to` fragment on the line says who
				level: "a",
				label: "Sends on its own",
				test: function (p) {
					return p.sends.some(function (s) {
						return s.alwaysOn;
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
				// about anyone but the operator or their household (visitors,
				// passers-by, third parties, performers) with no time limit --
				// retention is the one thing an author can add to leave red
				level: "r",
				test: function (p) {
					return p.collects.some(function (c) {
						return OTHERS_ABOUT.hasOwnProperty(c.about) && c.keptDays === null;
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
				// a camera, microphone or tracker, stored
				level: "r",
				test: function (p) {
					return p.sensors.some(function (s) {
						return s.stored && RECORDING_TYPES.hasOwnProperty(s.type);
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
				// any other sensor (presence, RFID, GPIO input), stored: a
				// button log is kept, but nobody is recorded
				level: "a",
				label: "Sensor readings kept",
				test: function (p) {
					return p.sensors.some(function (s) {
						return s.stored;
					});
				},
			},
			{
				// any other sensor, not stored
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
				// package-source or tunnel: what these leave behind outlives
				// the plugin
				level: "r",
				test: function (p) {
					return p.systemChanges.some(function (c) {
						return PERMANENT_KINDS.hasOwnProperty(c.kind);
					});
				},
			},
			{
				// reads-core-credentials
				level: "r",
				label: "Reads FPP's credentials",
				test: function (p) {
					return p.systemChanges.some(function (c) {
						return c.kind === "reads-core-credentials";
					});
				},
			},
			{
				// privilege: a sudoers rule or a group membership is red, but
				// not "permanent" -- some are undone by a restart
				level: "r",
				label: "Grants extra privileges",
				test: function (p) {
					return p.systemChanges.some(function (c) {
						return c.kind === "privilege";
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

	// A finding (amber, red) stands on whatever keys are present; green is
	// the author's statement that there is nothing, so it needs the light's
	// own key to have been written -- otherwise the light is undeclared.
	function decide(id, p) {
		var rules = RULES[id];
		for (var i = 0; i < rules.length; i++) {
			if (rules[i].test(p)) {
				var label = rules[i].label;
				return { level: rules[i].level, label: typeof label === "function" ? label(p) : label };
			}
		}
		return { level: p.has[id] ? "g" : "n" };
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

	// The lights the any-amber headline step reads, in display order; Sends
	// has its own step before it.
	var AMBER_HEADLINE_ORDER = ["collect", "camera", "remote", "system", "code"];

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
		if (byId.system === "r") return { text: labelById.system, level: "r" };
		// A light with no key: nothing the author says about the rest can
		// stand for the whole.
		if (
			LIGHTS.some(function (L) {
				return byId[L.id] === "n";
			})
		)
			return { text: "Not everything is disclosed", level: "n" };
		if (p.sends.length) {
			var allLocal = p.sends.every(function (s) {
				return !isInternet(s.to);
			});
			return {
				text:
					allLocal ?
						"Author says it talks to devices on your network"
					:	"Author says it talks to online services",
				level: "a",
			};
		}
		// Any other amber light: its own chip text, so a LAN listener or a
		// camera never reads "runs on this device only".
		for (var i = 0; i < AMBER_HEADLINE_ORDER.length; i++) {
			var id = AMBER_HEADLINE_ORDER[i];
			if (byId[id] === "a") return { text: labelById[id], level: "a" };
		}
		return { text: "Author says it runs on this device only", level: "g" };
	}

	// YYYY-MM-DD of the player's current local date, comparable as a string.
	// Built by hand: toLocaleDateString("en-CA") depends on the ICU data the
	// browser ships, and a WebView without that locale returns "1/1/2027".
	function todayString() {
		var d = new Date();
		function two(n) { return (n < 10 ? "0" : "") + n; }
		return d.getFullYear() + "-" + two(d.getMonth() + 1) + "-" + two(d.getDate());
	}

	/**
	 * Evaluate a pluginInfo.json `privacy` block.
	 *
	 * opts.unreviewed: the plugin was loaded from a pasted URL rather than
	 * the listing, so the block was never checked against the code; the
	 * label line says so.
	 *
	 * Each light carries `entries` (HTML, one per declared item) beside
	 * `label` and `level`. `declared` on the result is the block as a whole
	 * (an object with at least one of the six keys, as the server counts
	 * it); `declared` on a light is its own key -- a light whose key is
	 * missing is grey "not disclosed" inside a declared block, whatever the
	 * date.
	 * Returns { declared, unreviewed, lights[], headline, red, summary,
	 *           other, raw, normalised }.
	 */
	function evaluate(privacy, opts) {
		opts = opts || {};
		var p = normalise(privacy);
		var declared =
			!!privacy &&
			typeof privacy === "object" &&
			!Array.isArray(privacy) &&
			LIGHTS.some(function (L) {
				return p.has[L.id];
			});
		var overdue =
			!declared && todayString() >= PRIVACY_DECLARATION_REQUIRED_FROM;
		var lights = [];
		var byId = {};
		var labelById = {};
		var red = overdue;
		LIGHTS.forEach(function (L) {
			var d = declared ? decide(L.id, p) : { level: overdue ? "r" : "n" };
			var level = d.level;
			var lit = declared && level !== "n";
			var label = lit ? d.label || L[level] : UNDECLARED_LABEL;
			byId[L.id] = level;
			labelById[L.id] = label;
			if (declared && level === "r") red = true;
			lights.push({
				id: L.id,
				name: L.name,
				level: level,
				label: label,
				entries: lit ? lineFor(L.id, p) : [esc(UNDECLARED_LINE)],
				declared: lit,
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
			// A light with no key ranks with amber: not the red "no
			// disclosure" of a missing block, not the green of a full one.
			if (l.level === "a" || l.level === "n") anyAmber = true;
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

	// Bootstrap classes for a line or a change summary in a theme. Green is
	// drawn in the body's own muted colour, not Bootstrap's success green:
	// colour on the screen then means "look here", and a row of greens is
	// quiet.
	function themeClasses(theme) {
		if (theme === "success")
			return { text: "text-body-secondary", bg: "bg-body-tertiary", border: "border-secondary-subtle" };
		return { text: "text-" + theme + "-emphasis", bg: "bg-" + theme + "-subtle", border: "border-" + theme + "-subtle" };
	}

	// Classes for a chip: only a red chip keeps the tinted background,
	// coloured border and coloured text. Amber, green and grey chips are the
	// same neutral pill, and the dot before the label (stateDotHtml) carries
	// the state, so a strip reads as one row with the reds standing out.
	function chipClasses(theme) {
		if (theme === "danger") return themeClasses(theme);
		return { text: "text-body-secondary", bg: "bg-body-tertiary", border: "border-secondary-subtle" };
	}

	// The state dot before a chip's label and its line: an 8px Font Awesome
	// circle (fa-2xs on the chip's .8em text) in the state's emphasis colour,
	// filled for green, amber and red, hollow (border only) for grey --
	// "nothing declared" is an empty slot, not a colour.
	function stateDotHtml(theme, extra) {
		var cls = extra ? " " + extra : "";
		if (theme === "secondary")
			return '<i class="far fa-circle fa-2xs text-secondary' + cls + '" aria-hidden="true"></i>';
		return '<i class="fas fa-circle fa-2xs text-' + theme + '-emphasis' + cls + '" aria-hidden="true"></i>';
	}

	// The coloured dot of the compact card strip: a Font Awesome circle in
	// currentColor. Green is hollow (the author's word, not a tick), amber
	// and red are filled, and grey is a question mark so "nothing declared"
	// does not look like a green. (Chips and their lines use stateDotHtml.)
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
	 * opts.from: an earlier result (the block the operator accepted) --
	 *   "was / now" mode for the changed-disclosure dialog: a light whose
	 *   colour, chip text or lines differ is opened whatever its colour and
	 *   reads "Was: ... / Now: ...", with added lines marked + and removed
	 *   ones struck through; unchanged lights render as at install.
	 */
	function stripHtml(result, opts) {
		opts = opts || {};
		var prefix = opts.prefix || "pp";
		var changes = opts.from ? lightChanges(opts.from, result) : null;
		var h = "";
		if (opts.compact) {
			var tip =
				!result.declared ? "No disclosure"
				: result.headline.level === "n" ? result.headline.text
				: "Author's disclosure: " + result.headline.text;
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
						themeClasses(theme).text +
						'"' +
						title +
						"></i>";
			});
			return h + "</span>";
		}
		h +=
			'<div class="d-flex flex-wrap gap-2 mt-2" role="group" aria-label="Privacy lights">';
		result.lights.forEach(function (l, i) {
			var theme = LEVEL_THEME[l.level];
			var ch = changes ? changes[i] : null;
			var open = (l.level === "r" && l.declared) || !!ch;
			var id = prefix + "-" + l.id;
			h +=
				'<button type="button" class="pluginPrivacyChip badge rounded-pill border fw-semibold d-inline-flex align-items-center gap-1 ' +
				chipClasses(theme).text + " " + chipClasses(theme).bg + " " + chipClasses(theme).border + '"' +
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
				stateDotHtml(theme) +
				esc(chipText(l)) +
				(ch ?
					'<i class="fas fa-' +
						(ch.direction === "better" ? "arrow-down" : ch.direction === "worse" ? "arrow-up" : "pen") +
						' fa-xs" aria-hidden="true" title="' +
						esc(ch.direction === "better" ? "Improved" : ch.direction === "worse" ? "Worse than before" : "Changed") +
						'"></i>'
				: l.declared && l.level !== "r" ?
					'<i class="fas fa-caret-right fa-xs" aria-hidden="true"></i>'
				:	"") +
				"</button>";
		});
		h +=
			'</div><ul class="list-unstyled d-flex flex-column gap-1 small mt-2 mb-2">';
		result.lights.forEach(function (l, i) {
			var theme = LEVEL_THEME[l.level];
			var ch = changes ? changes[i] : null;
			var open = (l.level === "r" && l.declared) || !!ch;
			// d-none, not the hidden attribute: d-flex would outrank [hidden].
			h +=
				'<li id="' +
				prefix +
				"-" +
				l.id +
				'" class="d-flex gap-2 align-items-start' +
				(open ? "" : " d-none") +
				(l.level === "r" && l.declared ? " text-danger-emphasis" : "") +
				'">' +
				stateDotHtml(theme, "mt-1") +
				"<span><b>" +
				esc(l.name) +
				(/\?$/.test(l.name) ? "" : ":") +
				"</b> " +
				(ch ? changeLineHtml(ch) : lineHtml(l.entries)) +
				"</span></li>";
		});
		return h + "</ul>";
	}

	// Worst-first order of a light's colour, for "better" / "worse".
	var LEVEL_RANK = { n: 0, g: 1, a: 2, r: 3 };

	// Per light, what differs between an earlier result and this one, or
	// null when nothing does: the old and new chip text and colour, which
	// lines were added, removed or kept, and the direction of the change.
	function lightChanges(from, to) {
		return to.lights.map(function (l, i) {
			var o = from.lights[i];
			var same =
				o.level === l.level &&
				chipText(o) === chipText(l) &&
				o.entries.join("\n") === l.entries.join("\n");
			if (same) return null;
			var added = l.entries.filter(function (e) { return o.entries.indexOf(e) < 0; });
			var removed = o.entries.filter(function (e) { return l.entries.indexOf(e) < 0; });
			var kept = l.entries.filter(function (e) { return o.entries.indexOf(e) >= 0; });
			// Grey ranks lowest, but a light that went from declared to not
			// declared has not improved -- the author stopped saying. Either
			// side undeclared is a change, never better or worse.
			var direction =
				!o.declared || !l.declared ? "changed"
				: LEVEL_RANK[l.level] > LEVEL_RANK[o.level] ? "worse"
				: LEVEL_RANK[l.level] < LEVEL_RANK[o.level] ? "better"
				: "changed";
			return {
				id: l.id, name: l.name, direction: direction,
				was: { text: chipText(o), level: o.level, declared: o.declared },
				now: { text: chipText(l), level: l.level, declared: l.declared },
				added: added, removed: removed, kept: kept,
			};
		});
	}

	// The opened line of a changed light: the lines with + for added and a
	// strike-through for removed. Unchanged lines stay plain so the reader
	// sees what moved and what did not. Which way the light went is said
	// once, in changesSummaryHtml, and by the arrow on the chip -- not
	// repeated here.
	function changeLineHtml(ch) {
		var h = "";
		var items = [];
		// A light that is no longer declared has nothing added: its one
		// line is the plain "not disclosed" sentence, then the struck lines.
		if (!ch.now.declared) items.push(esc(UNDECLARED_LINE));
		else ch.added.forEach(function (e) { items.push('<span class="fw-semibold" title="Added">+</span> ' + e); });
		ch.removed.forEach(function (e) { items.push('<s class="text-secondary" title="No longer disclosed">' + e + "</s>"); });
		ch.kept.forEach(function (e) { items.push(e); });
		return h + lineHtml(items);
	}

	// One line under the headline in "was / now" mode naming the lights that
	// changed and which way, worst first: "Sends data: was Sends when enabled,
	// now Sends identifying data".
	function changesSummaryHtml(from, to) {
		// The whole block gone: one line, not six "now not disclosed".
		if (from.declared && !to.declared)
			return (
				'<ul class="small mb-2 mt-2 ps-3"><li class="mb-1"><b>Disclosure removed:</b> the author no longer says what this plugin does with data. ' +
				"What was disclosed before is struck through below.</li></ul>"
			);
		var changes = lightChanges(from, to).filter(function (c) { return c; });
		if (!changes.length) return "";
		var rank = { worse: 0, changed: 1, better: 2 };
		changes.sort(function (a, b) { return rank[a.direction] - rank[b.direction]; });
		var h = '<ul class="small mb-2 mt-2 ps-3">';
		changes.forEach(function (c) {
			h += '<li class="mb-1"><b>' + esc(c.name) + (/\?$/.test(c.name) ? "" : ":") + "</b> ";
			// Same finding, different detail under it: say that, not
			// "was X, now X".
			if (c.was.text === c.now.text)
				h += "still " + '<span class="' + themeClasses(LEVEL_THEME[c.now.level]).text + ' fw-semibold">' + esc(c.now.text) + "</span>, the details changed";
			else
				h +=
					"was " + '<span class="' + themeClasses(LEVEL_THEME[c.was.level]).text + '">' + esc(c.was.text) + "</span>, now " +
					'<span class="' + themeClasses(LEVEL_THEME[c.now.level]).text + ' fw-semibold">' + esc(c.now.text) + "</span>" +
					(c.direction === "better" ? ' <span class="text-success-emphasis">(improved)</span>' : "");
			h += "</li>";
		});
		return h + "</ul>";
	}

	// The one-line headline over the strip, a left rule in the colour of the
	// worst finding, with FPP's explainer under it when the rule has one. A
	// rule rather than a filled box: the dialogs stack this under other
	// callouts, and one more tinted block is one more thing to look at.
	function headlineHtml(result) {
		var theme = LEVEL_THEME[result.headline.level];
		var h =
			'<div class="ps-2 border-start border-3 border-' +
			theme +
			" text-" +
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

	// The author's summary on its own, above the headline: their plain
	// sentence about what the plugin does comes before FPP's verdict on it.
	function summaryHtml(result) {
		if (!result.declared || !result.summary) return "";
		return '<p class="mb-2">' + esc(result.summary) + "</p>";
	}

	// After the lines: the free-text `other`, so nothing the author declared
	// is hidden even when this FPP does not render a key, and -- only when
	// opts.raw -- "Full disclosure", the block as written, for people who
	// want to see the JSON (the page shows it in Developer UI mode).
	function detailsHtml(result, opts) {
		if (!result.declared) return "";
		var h = "";
		if (result.other)
			h += '<p class="small mb-2"><b>Other:</b> ' + esc(result.other) + "</p>";
		if (opts && opts.raw)
			h +=
				'<details class="small"><summary class="link-primary">Full disclosure</summary>' +
				'<pre class="mt-1 mb-0 overflow-auto"><code>' +
				esc(JSON.stringify(result.raw, null, 2)) +
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
		changesSummaryHtml: changesSummaryHtml,
		headlineHtml: headlineHtml,
		summaryHtml: summaryHtml,
		detailsHtml: detailsHtml,
		bind: bind,
	};
})();
