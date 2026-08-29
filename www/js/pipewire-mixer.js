/*
 * PipeWire Mixer -- realtime multi-node volume control for the player page.
 *
 * The main page's volume slider drives one global master. In PipeWire Advanced
 * mode the graph has many independently controllable levels (media stream
 * slots, output groups and their per-card members, input-group members, and
 * routing-matrix paths); this dialog surfaces all of them at once.
 *
 * Reads and writes go to the same /api/pipewire/audio/* endpoints the Settings
 * pages use, so there is one implementation of the node resolution and the
 * changes persist the same way.
 */

var pwMixer = {
	pollTimer: null,
	pollMs: 2000,
	saveTimers: {},
	// Cached model, rebuilt each poll. Kept so a slider drag can update the
	// local value immediately without waiting for the next poll to confirm.
	data: { groups: [], inputGroups: [], routing: [], slots: [], slotVolumes: {}, nodeStates: {}, entities: {} },
	// Sliders the user is actively dragging, keyed by control id. A poll must
	// not yank the thumb out from under a finger mid-drag.
	dragging: {},
	openSections: null,
	previewNode: null,
	// Live meter for the node being previewed, driven from the audio the
	// browser is already receiving (see PWMixerStartMeter).
	meterCtx: null,
	meterRaf: null,
	// Values the user has just set, held briefly so a poll carrying the
	// pre-change state cannot snap a fader back under them. See PWMixerSlide.
	pending: {},
	// Per-node levels pushed by fppd over the status WebSocket, and the
	// keepalive that tells it which nodes to meter.
	levels: {},
	meterSubTimer: null,
};

function PWMixerSectionOpen(id) {
	if (pwMixer.openSections === null) {
		pwMixer.openSections = {};
		try {
			var saved = localStorage.getItem('pwMixerOpenSections');
			if (saved) {
				pwMixer.openSections = JSON.parse(saved);
			}
		} catch (e) {
			// Private mode / storage disabled -- fall through to defaults.
		}
	}
	if (typeof pwMixer.openSections[id] === 'undefined') {
		// Streams and output groups are what most people came for.
		return id === 'streams' || id === 'groups';
	}
	return !!pwMixer.openSections[id];
}

function PWMixerToggleSection(id) {
	// Toggle the state actually on screen, not the stored one: a section using
	// its default (nothing stored yet) held `undefined`, so !undefined set it
	// open when it already looked open and the first click appeared to do
	// nothing.
	var open = PWMixerSectionOpen(id);
	pwMixer.openSections[id] = !open;
	try {
		localStorage.setItem('pwMixerOpenSections', JSON.stringify(pwMixer.openSections));
	} catch (e) {
		// Not being able to remember the layout is not worth surfacing.
	}
	PWMixerRender(true);
}

function OpenPipeWireMixer() {
	DoModalDialog({
		id: 'pipewireMixerDialog',
		title: 'Audio Mixer',
		// Fullscreen on phones: this is a control surface people reach for
		// mid-show on a handset, not a desktop-only settings panel.
		class: 'modal-xl modal-dialog-scrollable modal-fullscreen-sm-down',
		// The preview host lives outside the polled region: re-rendering it
		// every 2s would tear down and restart the <audio> element mid-listen.
		body:
			"<div id='pwMixerBody'><div class='text-center p-4'><i class='fas fa-spinner fa-spin'></i> Loading mixer...</div></div>" +
			"<div id='pwMixerPreviewHost'></div>",
		buttons: {
			Close: function () {
				CloseModalDialog('pipewireMixerDialog');
			},
		},
	});

	$('#pipewireMixerDialog')
		.off('shown.bs.modal.pwmixer')
		.on('shown.bs.modal.pwmixer', function () {
			PWMixerStartPolling();
			PWMixerSubscribeMeters();
		});
	$('#pipewireMixerDialog')
		.off('hidden.bs.modal.pwmixer')
		.on('hidden.bs.modal.pwmixer', function () {
			PWMixerStopPolling();
			PWMixerStopPreview();
			PWMixerStopMeterSubscription();
		});
}

function PWMixerStartPolling() {
	PWMixerStopPolling();
	PWMixerPoll();
}

function PWMixerStopPolling() {
	if (pwMixer.pollTimer) {
		clearTimeout(pwMixer.pollTimer);
		pwMixer.pollTimer = null;
	}
}

function PWMixerSchedulePoll() {
	PWMixerStopPolling();
	if (!$('#pipewireMixerDialog').is(':visible')) {
		return;
	}
	// Don't burn a phone's battery/data polling a graph nobody is looking at.
	if (document.hidden) {
		pwMixer.pollTimer = setTimeout(PWMixerSchedulePoll, pwMixer.pollMs);
		return;
	}
	pwMixer.pollTimer = setTimeout(PWMixerPoll, pwMixer.pollMs);
}

function PWMixerPoll() {
	$.when(
		$.ajax({ url: 'api/pipewire/audio/groups', dataType: 'json' }),
		$.ajax({ url: 'api/pipewire/audio/input-groups', dataType: 'json' }),
		$.ajax({ url: 'api/pipewire/audio/routing', dataType: 'json' }),
		$.ajax({ url: 'api/pipewire/audio/stream/status', dataType: 'json' }),
		$.ajax({ url: 'api/pipewire/audio/stream/volumes', dataType: 'json' }),
		$.ajax({ url: 'api/pipewire/audio/node-states', dataType: 'json' })
	)
		.done(function (groups, inputGroups, routing, slots, slotVolumes, nodeStates) {
			pwMixer.data.groups = (groups[0] && groups[0].groups) || [];
			pwMixer.data.inputGroups = (inputGroups[0] && inputGroups[0].inputGroups) || [];
			pwMixer.data.routing = (routing[0] && routing[0].matrix) || [];
			pwMixer.data.slots = (slots[0] && slots[0].slots) || slots[0] || [];
			pwMixer.data.slotVolumes = (slotVolumes[0] && slotVolumes[0].slots) || {};
			pwMixer.data.nodeStates = (nodeStates[0] && nodeStates[0].nodes) || {};
			pwMixer.data.entities = (nodeStates[0] && nodeStates[0].entities) || {};
			PWMixerRender();
		})
		.fail(function () {
			$('#pwMixerBody').html(
				"<div class='alert alert-warning'>Could not read the PipeWire graph. Is the PipeWire backend running?</div>"
			);
		})
		.always(function () {
			PWMixerSchedulePoll();
		});
}

/////////////////////////////////////////////////////////////////////////////
// Rendering

// Activity comes from the server-resolved entity map where the strip has a
// target key (input-group members and routing paths ride on nodes whose names
// are not derivable from the group config), falling back to a direct node
// lookup for the strips that address a node by name.
function PWMixerNodeState(nodeName, stateKey) {
	if (stateKey && typeof pwMixer.data.entities[stateKey] !== 'undefined') {
		return pwMixer.data.entities[stateKey];
	}
	if (!nodeName) {
		return 'unknown';
	}
	return pwMixer.data.nodeStates[nodeName] || 'unknown';
}

// Activity LED. "running" means PipeWire is actually moving audio through the
// node right now; everything else is idle/absent.
function PWMixerLed(nodeName, stateKey, explicitState) {
	var state = explicitState || PWMixerNodeState(nodeName, stateKey);
	var cls = 'text-secondary';
	var title = 'Idle';
	if (state === 'running') {
		cls = 'text-success';
		title = 'Signal present';
	} else if (state === 'error') {
		cls = 'text-danger';
		title = 'Error';
	} else if (state === 'unknown') {
		cls = 'text-body-tertiary';
		title = 'Not present in the graph';
	}
	return "<i class='fas fa-circle fs-6 " + cls + "' title='" + title + "'></i>";
}

// One control: activity LED, label, slider, readout, mute, optional preview.
// Rendered as a row on phones and a column (channel strip) from md up.
function PWMixerStrip(opts) {
	var id = opts.id;
	var vol = parseInt(opts.volume, 10);
	if (isNaN(vol)) {
		vol = 100;
	}
	// A value the user set moments ago wins over whatever the poll reports,
	// until the server has had time to catch up. Slot 1 showed this most
	// clearly: it is the master, so its displayed value comes from the page's
	// own slider, which the status feed rewrites from fppd about once a second
	// -- landing before the change registered and snapping the fader back.
	var pend = pwMixer.pending[id];
	if (pend) {
		if (Date.now() < pend.until && pend.value !== vol) {
			vol = pend.value;
		} else if (Date.now() >= pend.until || pend.value === vol) {
			delete pwMixer.pending[id];
		}
	}
	var max = opts.max || 100;
	var muted = !!opts.mute;

	var h = "<div class='pw-mixer-strip border rounded p-2 d-flex align-items-center gap-2" + (muted ? ' opacity-50' : '') + "'>";

	h += "<div class='pw-mixer-strip-label d-flex align-items-center gap-2' title='" + PWMixerEscape(opts.title || opts.label) + "'>";
	h += PWMixerLed(opts.nodeName, opts.stateKey, opts.state);
	h += "<span class='text-truncate small fw-semibold'>" + PWMixerEscape(opts.label) + '</span>';
	h += '</div>';

	if (opts.sublabel) {
		h += "<div class='small text-body-secondary text-truncate d-none d-xl-block pw-mixer-sublabel'>" + PWMixerEscape(opts.sublabel) + '</div>';
	}

	h += "<input type='range' class='form-range pw-mixer-slider' min='0' max='" + max + "' value='" + vol + "'";
	h += " id='" + id + "' aria-label='" + PWMixerEscape(opts.label) + " volume'";
	h += ' oninput="PWMixerSlide(this,\'' + id + "')\"";
	h += ' onchange="PWMixerSlideEnd(\'' + id + "')\"";
	h += ' onpointerdown="PWMixerDragStart(\'' + id + "')\"";
	h += ' onpointerup="PWMixerDragEnd(\'' + id + "')\"";
	h += ' onpointercancel="PWMixerDragEnd(\'' + id + "')\"" + '>';

	h += "<span class='pw-mixer-value small text-body-secondary' id='" + id + "_val'>" + vol + '%</span>';

	if (opts.nodeName) {
		// The kind decides how fppd captures it: a sink is metered through its
		// monitor, a playback stream directly. Getting it wrong does not fail
		// loudly -- it silently meters the default sink instead.
		h += "<div class='pw-mixer-meter' data-meter-node='" + PWMixerEscape(opts.nodeName) + "'";
		h += " data-meter-sink='" + (opts.nodeIsStream ? '0' : '1') + "' title='Signal level'>";
		h += "<div class='pw-mixer-meter-fill'></div>";
		h += '</div>';
	}

	h += "<div class='d-flex gap-1'>";
	// Stream slots have no mute in the API (their level is the only control),
	// so don't offer a button that cannot do anything.
	if (!opts.noMute) {
		h += "<button type='button' class='btn btn-sm " + (muted ? 'btn-danger' : 'btn-outline-secondary') + "'";
		h += " onclick=\"PWMixerToggleMute('" + id + "')\" aria-label='Mute " + PWMixerEscape(opts.label) + "'>";
		h += "<i class='fas fa-fw fa-volume-" + (muted ? 'mute' : 'up') + "'></i></button>";
	}
	if (opts.nodeName) {
		var previewing = pwMixer.previewNode === opts.nodeName;
		h += "<button type='button' class='btn btn-sm " + (previewing ? 'btn-primary' : 'btn-outline-secondary') + "'";
		h += " onclick=\"PWMixerTogglePreview('" + PWMixerEscape(opts.nodeName) + "')\" title='Listen to this output'";
		h += " aria-label='Preview " + PWMixerEscape(opts.label) + "'>";
		h += "<i class='fas fa-fw fa-" + (previewing ? 'stop' : 'headphones') + "'></i></button>";
	}
	h += '</div>';

	h += '</div>';
	return h;
}

function PWMixerEscape(s) {
	return $('<div>').text(s === null || typeof s === 'undefined' ? '' : s).html().replace(/'/g, '&#39;');
}

function PWMixerSection(id, title, badge, bodyHtml) {
	var open = PWMixerSectionOpen(id);
	var h = "<section class='mb-3'>";
	h += "<button type='button' class='btn btn-link text-decoration-none w-100 d-flex align-items-center gap-2 px-0' onclick=\"PWMixerToggleSection('" + id + "')\" aria-expanded='" + open + "'>";
	h += "<i class='fas fa-fw fa-chevron-" + (open ? 'down' : 'right') + "'></i>";
	h += "<span class='fw-semibold'>" + PWMixerEscape(title) + '</span>';
	if (badge) {
		h += "<span class='badge text-bg-secondary ms-auto'>" + PWMixerEscape(badge) + '</span>';
	}
	h += '</button>';
	if (open) {
		h += "<div class='pw-mixer-strips d-flex flex-column flex-md-row flex-md-wrap gap-2 pt-1'>" + bodyHtml + '</div>';
	}
	return h + '</section>';
}

// force: redraw even mid-drag. A change the user just made has to show up at
// once -- skipping that redraw is what made buttons look like they needed a
// second press.
// The page's own master slider, repeated here so the dialog is a complete
// picture: every level below multiplies with it.
// The master has no node of its own -- it is applied across every output sink
// -- so its indicator reflects the graph as a whole: live when anything
// downstream is actually passing audio.
function PWMixerAggregateState() {
	var seen = false;
	for (var k in pwMixer.data.entities) {
		var st = pwMixer.data.entities[k];
		if (st === 'running') {
			return 'running';
		}
		if (st && st !== 'unknown') {
			seen = true;
		}
	}
	return seen ? 'idle' : 'unknown';
}

function PWMixerMasterSection() {
	var vol = PWMixerMasterVolume();
	var h = "<section class='mb-3'>";
	h += "<div class='d-flex align-items-center gap-2 mb-1'>";
	h += "<span class='fw-semibold'>Master</span>";
	h += '</div>';
	h += "<div class='pw-mixer-strips'>";
	h += PWMixerStrip({
		id: 'pwm_master',
		label: 'Master volume',
		sublabel: 'All outputs',
		title: 'Global master volume -- multiplies with every level below',
		volume: vol,
		state: PWMixerAggregateState(),
		noMute: true,
	});
	h += '</div></section>';
	return h;
}

// fppd owns the metering pipelines; tell it which nodes are on screen and keep
// that alive while the dialog is open. The TTL is what makes this self-healing:
// stop re-posting (dialog closed, tab gone, browser crashed) and fppd tears the
// pipelines down on its own a few seconds later.
function PWMixerSubscribeMeters() {
	if (!$('#pipewireMixerDialog').is(':visible')) {
		return;
	}
	var nodes = [];
	var seen = {};
	// Only what is actually on screen: a collapsed section costs nothing.
	$('#pwMixerBody')
		.find('[data-meter-node]')
		.each(function () {
			var n = $(this).attr('data-meter-node');
			if (!n || seen[n]) {
				return;
			}
			// Skip anything not currently in the graph. pipewiresrc cannot
			// report that a target does not exist -- it falls back to the
			// default sink and returns a believable level for the wrong node --
			// so filtering here is what stops a phantom meter appearing.
			if (typeof pwMixer.data.nodeStates[n] === 'undefined') {
				return;
			}
			seen[n] = true;
			nodes.push({ name: n, sink: $(this).attr('data-meter-sink') !== '0' });
		});

	$.ajax({
		url: 'api/pipewire/audio/meters',
		method: 'POST',
		contentType: 'application/json',
		data: JSON.stringify({ nodes: nodes, ttl: 6000 }),
	});

	pwMixer.meterSubTimer = setTimeout(PWMixerSubscribeMeters, 3000);
}

function PWMixerStopMeterSubscription() {
	if (pwMixer.meterSubTimer) {
		clearTimeout(pwMixer.meterSubTimer);
		pwMixer.meterSubTimer = null;
	}
	pwMixer.levels = {};
	// Release the pipelines now rather than waiting out the TTL.
	$.ajax({
		url: 'api/pipewire/audio/meters',
		method: 'POST',
		contentType: 'application/json',
		data: JSON.stringify({ nodes: [], ttl: 0 }),
	});
}

// Called from fpp.js for every "levels" frame on the status WebSocket. Writes
// straight to the bars rather than re-rendering: this arrives ten times a
// second and rebuilding the dialog at that rate would fight every interaction.
window.PWMixerOnLevels = function (levels) {
	pwMixer.levels = levels || {};
	$('#pwMixerBody')
		.find('[data-meter-node]')
		.each(function () {
			var lvl = pwMixer.levels[$(this).attr('data-meter-node')];
			var pct = typeof lvl === 'number' ? lvl : 0;
			var fill = this.firstChild;
			if (fill && fill.style) {
				fill.style.width = pct + '%';
				if (pct > 90) {
					fill.classList.add('pw-mixer-meter-hot');
				} else {
					fill.classList.remove('pw-mixer-meter-hot');
				}
			}
		});
};

function PWMixerRender(force) {
	// A poll landing mid-drag would rebuild the input under the user's finger
	// and snap the thumb back to the server's (older) value. Only a poll is
	// worth skipping, and only while a pointer really is down.
	if (!force && PWMixerDragging()) {
		return;
	}

	var d = pwMixer.data;
	var h = PWMixerMasterSection();

	// --- Media stream slots ---
	var slotHtml = '';
	var activeSlots = 0;
	var slots = Array.isArray(d.slots) ? d.slots : [];
	for (var i = 0; i < slots.length; i++) {
		var s = slots[i];
		var slotNum = s.slot;
		var playing = s.status === 'playing';
		if (playing) {
			activeSlots++;
		}
		var vol = d.slotVolumes[String(slotNum)];
		if (typeof vol === 'undefined') {
			vol = 100;
		}
		slotHtml += PWMixerStrip({
			id: 'pwm_slot_' + slotNum,
			label: 'Slot ' + slotNum,
			sublabel: s.mediaFilename || (playing ? 'Playing' : 'Idle'),
			title: s.nodeName,
			nodeName: s.nodeName,
			nodeIsStream: true,
			stateKey: 'slot:' + slotNum,
			volume: vol,
			noMute: true,
		});
	}
	if (!slotHtml) {
		slotHtml = "<div class='text-body-secondary small'>No media stream slots reported.</div>";
	}
	h += PWMixerSection('streams', 'Media Stream Slots', activeSlots + ' active', slotHtml);

	// --- Output groups (and their per-card members) ---
	var groupHtml = '';
	var enabledGroups = 0;
	for (var g = 0; g < d.groups.length; g++) {
		var grp = d.groups[g];
		if (!grp.enabled) {
			continue;
		}
		enabledGroups++;
		var groupNode = 'fpp_group_' + PWMixerSlug(grp.name);
		groupHtml += PWMixerStrip({
			id: 'pwm_group_' + g,
			label: grp.name,
			sublabel: 'Group',
			title: groupNode,
			nodeName: groupNode,
			stateKey: 'sink:' + groupNode,
			volume: typeof grp.volume === 'undefined' ? 100 : grp.volume,
			mute: grp.mute,
		});
		var members = grp.members || [];
		for (var m = 0; m < members.length; m++) {
			var mbr = members[m];
			if (!mbr.cardId) {
				continue;
			}
			var fxNode = 'fpp_fx_g' + grp.id + '_' + PWMixerSlug(mbr.cardId);
			groupHtml += PWMixerStrip({
				id: 'pwm_member_' + g + '_' + m,
				label: mbr.cardId,
				sublabel: grp.name,
				title: fxNode,
				nodeName: fxNode,
				stateKey: 'sink:' + fxNode,
				volume: typeof mbr.volume === 'undefined' ? 100 : mbr.volume,
				mute: mbr.mute,
			});
		}
	}
	if (!groupHtml) {
		groupHtml = "<div class='text-body-secondary small'>No enabled output groups.</div>";
	}
	h += PWMixerSection('groups', 'Output Groups', enabledGroups + ' enabled', groupHtml);

	// --- Input groups (mix buses) and their members ---
	var inputHtml = '';
	var inputCount = 0;
	for (var ig = 0; ig < d.inputGroups.length; ig++) {
		var grp2 = d.inputGroups[ig];
		if (!grp2.enabled) {
			continue;
		}
		inputCount++;
		var mbrs = grp2.members || [];
		for (var mi = 0; mi < mbrs.length; mi++) {
			var im = mbrs[mi];
			inputHtml += PWMixerStrip({
				id: 'pwm_input_' + grp2.id + '_' + mi,
				label: im.name || 'Member ' + mi,
				sublabel: grp2.name,
				stateKey: 'input:' + grp2.id + ':' + mi,
				volume: typeof im.volume === 'undefined' ? 100 : im.volume,
				mute: im.mute,
			});
		}
	}
	if (!inputHtml) {
		inputHtml = "<div class='text-body-secondary small'>No enabled input groups.</div>";
	}
	h += PWMixerSection('inputs', 'Input Groups', inputCount + ' enabled', inputHtml);

	// --- Routing matrix paths ---
	var routeHtml = '';
	var routeCount = 0;
	for (var r = 0; r < d.routing.length; r++) {
		var row = d.routing[r];
		var paths = row.paths || [];
		for (var p = 0; p < paths.length; p++) {
			var path = paths[p];
			if (!path.connected) {
				continue;
			}
			routeCount++;
			routeHtml += PWMixerStrip({
				id: 'pwm_route_' + row.inputGroupId + '_' + path.outputGroupId,
				label: row.inputGroupName + ' → ' + path.outputGroupName,
				sublabel: 'Route',
				stateKey: 'route:' + row.inputGroupId + ':' + path.outputGroupId,
				volume: typeof path.volume === 'undefined' ? 100 : path.volume,
				mute: path.mute,
			});
		}
	}
	if (!routeHtml) {
		routeHtml = "<div class='text-body-secondary small'>No connected routing paths.</div>";
	}
	h += PWMixerSection('routing', 'Routing Matrix', routeCount + ' connected', routeHtml);

	$('#pwMixerBody').html(h);
}

// Node names are slugged identically everywhere in the PHP side; mirror it here
// so the JS can address a node without another round trip.
// KEEP IN SYNC with the preg_replace slug in www/api/controllers/pipewire.php.
function PWMixerSlug(s) {
	return String(s)
		.toLowerCase()
		.replace(/[^a-z0-9_]/g, '_');
}

function PWMixerMasterVolume() {
	var v = parseInt($('#slider').val(), 10);
	return isNaN(v) ? 100 : v;
}

/////////////////////////////////////////////////////////////////////////////
// Interaction

function PWMixerDragStart(id) {
	pwMixer.dragging[id] = Date.now();
}

function PWMixerDragEnd(id) {
	delete pwMixer.dragging[id];
}

// True only while a drag is plausibly still in progress. Entries are stamped
// rather than flagged so one that never received its pointerup -- released off
// the control, or a gesture the browser cancelled without telling us -- ages
// out instead of blocking every future redraw for the life of the dialog.
function PWMixerDragging() {
	var now = Date.now();
	var active = false;
	for (var id in pwMixer.dragging) {
		if (now - pwMixer.dragging[id] > 3000) {
			delete pwMixer.dragging[id];
		} else {
			active = true;
		}
	}
	return active;
}

function PWMixerSlide(el, id) {
	$('#' + id + '_val').text(el.value + '%');
	// Held until the server reports it back, or briefly if it never does.
	pwMixer.pending[id] = { value: parseInt(el.value, 10), until: Date.now() + 3000 };
	var key = 'v_' + id;
	if (pwMixer.saveTimers[key]) {
		clearTimeout(pwMixer.saveTimers[key]);
	}
	pwMixer.saveTimers[key] = setTimeout(function () {
		PWMixerSend(id, parseInt(el.value, 10), null);
	}, 60);
}

function PWMixerSlideEnd(id) {
	PWMixerDragEnd(id);
}

function PWMixerToggleMute(id) {
	var target = PWMixerParseId(id);
	if (!target) {
		return;
	}
	var current = PWMixerCurrentMute(target);
	PWMixerSend(id, null, !current);
}

// Resolve a control id back to the model entry it came from, so a mute toggle
// knows the current state and a send knows which endpoint to call.
function PWMixerParseId(id) {
	var parts = id.split('_');
	var kind = parts[1];
	if (kind === 'master') {
		return { kind: 'master' };
	}
	if (kind === 'slot') {
		return { kind: 'slot', slot: parseInt(parts[2], 10) };
	}
	if (kind === 'group') {
		return { kind: 'group', groupIndex: parseInt(parts[2], 10) };
	}
	if (kind === 'member') {
		return { kind: 'member', groupIndex: parseInt(parts[2], 10), memberIndex: parseInt(parts[3], 10) };
	}
	if (kind === 'input') {
		return { kind: 'input', groupId: parseInt(parts[2], 10), memberIndex: parseInt(parts[3], 10) };
	}
	if (kind === 'route') {
		return { kind: 'route', inputGroupId: parseInt(parts[2], 10), outputGroupId: parseInt(parts[3], 10) };
	}
	return null;
}

function PWMixerCurrentMute(t) {
	var d = pwMixer.data;
	if (t.kind === 'group') {
		return !!(d.groups[t.groupIndex] && d.groups[t.groupIndex].mute);
	}
	if (t.kind === 'member') {
		var grp = d.groups[t.groupIndex];
		return !!(grp && grp.members && grp.members[t.memberIndex] && grp.members[t.memberIndex].mute);
	}
	if (t.kind === 'input') {
		for (var i = 0; i < d.inputGroups.length; i++) {
			if (d.inputGroups[i].id === t.groupId) {
				var m = (d.inputGroups[i].members || [])[t.memberIndex];
				return !!(m && m.mute);
			}
		}
	}
	if (t.kind === 'route') {
		for (var r = 0; r < d.routing.length; r++) {
			if (d.routing[r].inputGroupId !== t.inputGroupId) {
				continue;
			}
			var paths = d.routing[r].paths || [];
			for (var p = 0; p < paths.length; p++) {
				if (paths[p].outputGroupId === t.outputGroupId) {
					return !!paths[p].mute;
				}
			}
		}
	}
	return false;
}

// volume === null means "mute toggle only"; mute === null means "volume only".
function PWMixerSend(id, volume, mute) {
	var t = PWMixerParseId(id);
	if (!t) {
		return;
	}
	var d = pwMixer.data;
	var url = null;
	var body = {};
	// On a mute toggle the caller passes no volume; send the level the control
	// is currently showing so an endpoint that needs one (routing restores the
	// level on unmute) gets the real value rather than a placeholder.
	var curVol = volume;
	if (curVol === null) {
		curVol = parseInt($('#' + id).val(), 10);
		if (isNaN(curVol)) {
			curVol = 100;
		}
	}

	if (t.kind === 'master') {
		// The page's own master path, unchanged -- no target, so this is the
		// same call the main volume slider makes.
		url = 'api/system/volume';
		body = { volume: curVol };
		$('#slider').val(curVol);
		$('#volume').html(curVol);
	} else if (t.kind === 'slot') {
		// Slot 1 is an ordinary slot: its fader is that stream's own stage and
		// no longer doubles as the master, which is the Master control above.
		url = 'api/pipewire/audio/stream/volume';
		body = { slot: t.slot, volume: curVol };
	} else if (t.kind === 'group' || t.kind === 'member') {
		var grp = d.groups[t.groupIndex];
		if (!grp) {
			return;
		}
		var sink;
		if (t.kind === 'group') {
			sink = 'fpp_group_' + PWMixerSlug(grp.name);
			if (volume !== null) {
				grp.volume = volume;
			}
			if (mute !== null) {
				grp.mute = mute;
			}
		} else {
			var mbr = (grp.members || [])[t.memberIndex];
			if (!mbr) {
				return;
			}
			sink = 'fpp_fx_g' + grp.id + '_' + PWMixerSlug(mbr.cardId);
			if (volume !== null) {
				mbr.volume = volume;
			}
			if (mute !== null) {
				mbr.mute = mute;
			}
		}
		url = 'api/pipewire/audio/group/volume';
		body = { sink: sink, volume: curVol };
		if (mute !== null) {
			body.mute = mute;
		}
	} else if (t.kind === 'input') {
		url = 'api/pipewire/audio/input-groups/volume';
		body = { groupId: t.groupId, memberIndex: t.memberIndex, volume: curVol };
		if (mute !== null) {
			body.mute = mute;
		}
	} else if (t.kind === 'route') {
		url = 'api/pipewire/audio/routing/volume';
		body = {
			inputGroupId: t.inputGroupId,
			outputGroupId: t.outputGroupId,
			volume: curVol,
		};
		if (mute !== null) {
			body.mute = mute;
		}
	}

	if (!url) {
		return;
	}

	$.ajax({
		url: url,
		method: 'POST',
		contentType: 'application/json',
		data: JSON.stringify(body),
		error: function () {
			console.warn('PipeWire mixer: failed to set ' + id);
		},
	}).done(function () {
		if (mute !== null) {
			// Re-render so the button reflects the new state immediately
			// rather than waiting for the next poll.
			PWMixerRender(true);
		}
	});
}

/////////////////////////////////////////////////////////////////////////////
// Preview / audition

// Only one preview runs at a time -- the server enforces this too, but
// switching here keeps the UI honest about what is actually playing.
// The <audio> element is built inside the click handler's own turn so mobile
// browsers count playback as user-initiated rather than blocking autoplay.
function PWMixerTogglePreview(nodeName) {
	if (pwMixer.previewNode === nodeName) {
		PWMixerStopPreview();
		PWMixerRender(true);
		return;
	}
	PWMixerStopPreview();
	pwMixer.previewNode = nodeName;

	var host = document.getElementById('pwMixerPreviewHost');
	if (host) {
		// Built as elements rather than innerHTML so play() can be called
		// inside this click handler's own task -- mobile browsers only treat
		// playback as user-initiated then, and the autoplay attribute alone is
		// not reliably credited.
		var wrap = document.createElement('div');
		wrap.className = 'alert alert-secondary d-flex align-items-center flex-wrap gap-2 mt-2 mb-0';

		var icon = document.createElement('i');
		icon.className = 'fas fa-headphones';
		wrap.appendChild(icon);

		var label = document.createElement('span');
		label.className = 'small';
		label.innerHTML = 'Previewing <strong>' + PWMixerEscape(nodeName) + '</strong>';
		wrap.appendChild(label);

		var note = document.createElement('span');
		note.className = 'small text-body-secondary';
		note.id = 'pwMixerPreviewNote';
		note.textContent = 'connecting...';
		wrap.appendChild(note);

		// Real-time meter for what is actually being heard. Measured in the
		// browser off the stream it already has, so it costs the player
		// nothing and can run at animation rate -- unlike sampling each node
		// server-side, where every reading is a fresh capture.
		var meter = document.createElement('div');
		meter.className = 'pw-mixer-meter pw-mixer-meter-lg';
		meter.title = 'Live level';
		var fill = document.createElement('div');
		fill.className = 'pw-mixer-meter-fill';
		meter.appendChild(fill);
		wrap.appendChild(meter);

		var audio = document.createElement('audio');
		audio.className = 'ms-auto';
		audio.controls = true;
		audio.autoplay = true;
		audio.preload = 'auto';
		audio.src = 'api/pipewire/audio/preview?node=' + encodeURIComponent(nodeName);
		audio.addEventListener('playing', function () {
			note.textContent = '';
		});
		audio.addEventListener('error', function () {
			note.textContent = 'stream failed';
			note.className = 'small text-danger';
		});
		wrap.appendChild(audio);

		host.innerHTML = '';
		host.appendChild(wrap);

		PWMixerStartMeter(audio, fill);

		var p = audio.play();
		if (p && typeof p.catch === 'function') {
			p.catch(function (err) {
				// Only an autoplay block is the user's to resolve; anything
				// else is a genuine failure and the error handler above will
				// have said so.
				if (err && err.name === 'NotAllowedError') {
					note.textContent = 'press play to listen';
				}
			});
		}
	}
	PWMixerRender(true);
}

// Drives a level bar from the previewed stream using an AnalyserNode. The
// element has to be routed through the graph for this, so it is reconnected to
// the destination -- without that the audio would go silent.
function PWMixerStartMeter(audio, fill) {
	PWMixerStopMeter();

	var Ctx = window.AudioContext || window.webkitAudioContext;
	if (!Ctx) {
		return; // no Web Audio: the preview still plays, just without a meter
	}

	try {
		var ctx = new Ctx();
		var src = ctx.createMediaElementSource(audio);
		var analyser = ctx.createAnalyser();
		analyser.fftSize = 1024;
		// Ballistics: fast attack so transients register, slow release so the
		// bar falls back at a readable rate rather than flickering.
		analyser.smoothingTimeConstant = 0.3;
		src.connect(analyser);
		analyser.connect(ctx.destination);

		// Routing the element through the graph means the graph is now the
		// only path to the speakers, and a context starts suspended -- without
		// this resume the preview would play silently. Called here inside the
		// click handler so the gesture still counts.
		if (ctx.state === 'suspended' && typeof ctx.resume === 'function') {
			ctx.resume().catch(function () {
				// Could not start the graph: drop the analyser out of the path
				// so the element is audible again, meter or no meter.
				try {
					analyser.disconnect();
					src.disconnect();
					src.connect(ctx.destination);
				} catch (e2) {
					// Nothing further we can do; playback is the priority.
				}
			});
		}

		var buf = new Float32Array(analyser.fftSize);
		var shown = 0;
		pwMixer.meterCtx = ctx;

		var tick = function () {
			analyser.getFloatTimeDomainData(buf);
			var sum = 0;
			for (var i = 0; i < buf.length; i++) {
				sum += buf[i] * buf[i];
			}
			// RMS, not peak: peak sits pinned near full scale on almost any
			// programme material and reads as a static bar.
			var rms = Math.sqrt(sum / buf.length);
			var db = rms > 0 ? 20 * Math.log10(rms) : -100;
			var pct = Math.max(0, Math.min(100, ((db + 60) / 60) * 100));
			shown = pct > shown ? pct : shown + (pct - shown) * 0.2;
			fill.style.width = shown.toFixed(1) + '%';
			fill.classList.toggle('pw-mixer-meter-hot', shown > 90);
			pwMixer.meterRaf = requestAnimationFrame(tick);
		};
		tick();
	} catch (e) {
		// A browser that will not give us the stream (or an element already
		// bound to a context) simply gets no meter. Only the animation is
		// stopped here -- closing the context would permanently silence an
		// element that has already been routed into it, and playing the audio
		// matters more than drawing the bar.
		if (pwMixer.meterRaf) {
			cancelAnimationFrame(pwMixer.meterRaf);
			pwMixer.meterRaf = null;
		}
	}
}

function PWMixerStopMeter() {
	if (pwMixer.meterRaf) {
		cancelAnimationFrame(pwMixer.meterRaf);
		pwMixer.meterRaf = null;
	}
	if (pwMixer.meterCtx) {
		try {
			pwMixer.meterCtx.close();
		} catch (e) {
			// Already closed.
		}
		pwMixer.meterCtx = null;
	}
}

function PWMixerStopPreview() {
	PWMixerStopMeter();
	pwMixer.previewNode = null;
	var host = document.getElementById('pwMixerPreviewHost');
	if (host) {
		// Detach the source before dropping the element so the browser closes
		// the connection, which is what stops parec/ffmpeg server-side.
		var audio = host.querySelector('audio');
		if (audio) {
			audio.pause();
			audio.removeAttribute('src');
			audio.load();
		}
		host.innerHTML = '';
	}
}
