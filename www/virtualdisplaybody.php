<script>
	var cellColors = [];
	var scaleMap = [];
	var base64 = [];
	<?
	// 16:9 canvas by default, but can be overwridden in wrapper script
	if (!isset($canvasWidth))
		$canvasWidth = 1024;

	if (!isset($canvasHeight))
		$canvasHeight = 576;

	$base64 = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz+/"; // Base64 index table
	for ($i = 0; $i < 64; $i++) {
		printf("base64['%s'] = '%02X';\n", substr($base64, $i, 1), $i << 2);
	}

	$pixelSize = 1;
	$content = file_get_contents($settings['configDirectory'] . "/co-other.json");
	$json = json_decode($content, true);
	foreach ($json['channelOutputs'] as $output) {
		if ($output['type'] == 'HTTPVirtualDisplay') {
			$pixelSize = $output['pixelSize'];
		}
	}
	echo "var pixelSize = " . $pixelSize . ";\n";

	// Dots are drawn centred on their cell corner, so the edge column and row
	// hang half off a canvas that exactly fits the model.  Grow the canvas by
	// one dot diameter and centre each dot inside its cell.
	$pad = 2 * $pixelSize;
	$modelWidth = $canvasWidth;
	$modelHeight = $canvasHeight;

	$f = fopen($settings['configDirectory'] . '/virtualdisplaymap', "r");
	if ($f) {
		$line = fgets($f);
		if (preg_match('/^#/', $line))
			$line = fgets($f);
		$line = trim($line);
		$parts = explode(',', $line);
		$previewWidth = $parts[0];
		$previewHeight = $parts[1];

		if (($previewWidth / $previewHeight) > ($modelWidth / $modelHeight))
			$modelHeight = (int) ($modelWidth * $previewHeight / $previewWidth);
		else
			$modelWidth = (int) ($modelHeight * $previewWidth / $previewHeight);

		echo "var previewWidth = " . $previewWidth . ";\n";
		echo "var previewHeight = " . $previewHeight . ";\n";

		$scale = 1.0 * $modelWidth / $previewWidth;
		$canvasWidth = $modelWidth + $pad;
		$canvasHeight = $modelHeight + $pad;

		$scaleMap = array();

		while (!feof($f)) {
			$line = fgets($f);
			if (($line == "") || (preg_match('/^#/', $line)))
				continue;

			$line = trim($line);
			$entry = explode(",", $line);

			$ox = $entry[0];
			$oy = $previewHeight - $entry[1];
			$oz = $entry[2];
			$x = (int) ($ox * $scale + $scale / 2) + $pad;
			// $oy is a 1-based flip (previewHeight - y), so the top row sits one
			// cell above the bottom of the model area; subtracting half a cell
			// centres each dot in its row and keeps the bottom row on-canvas.
			$y = (int) ($oy * $scale - $scale / 2) + $pad;
			$z = (int) ($oz * $scale);
			$ch = $entry[3];
			$colors = $entry[4];
			$iy = $canvasHeight - $y;

			$ps = 1;
			if (sizeof($entry) >= 7) {
				$ps = (int) $entry[6];
			}

			$ps *= $pixelSize;

			if (($ox >= 4096) || ($oy >= 4096))
				$key = substr($base64, ($ox >> 12) & 0x3f, 1) .
					substr($base64, ($ox >> 6) & 0x3f, 1) .
					substr($base64, $ox & 0x3f, 1) .
					substr($base64, ($oy >> 12) & 0x3f, 1) .
					substr($base64, ($oy >> 6) & 0x3f, 1) .
					substr($base64, $oy & 0x3f, 1);
			else
				$key = substr($base64, ($ox >> 6) & 0x3f, 1) .
					substr($base64, $ox & 0x3f, 1) .
					substr($base64, ($oy >> 6) & 0x3f, 1) .
					substr($base64, $oy & 0x3f, 1);

			if (!isset($scaleMap[$key])) {
				$scaleMap[$key] = 1;
				echo "scaleMap['" . $key . "'] = { x: $x, y: $y, ox: $ox, oy: $oy, size: $ps };\n";
				echo "cellColors['$x,$y'] = { x: $x, y: $y, color: '#000000', size: $ps};\n";
			}
		}
		fclose($f);
	}

	?>

	var canvasWidth = <? echo $canvasWidth; ?>;
	var canvasHeight = <? echo $canvasHeight; ?>;
	var pad = <? echo isset($pad) ? $pad : 0; ?>;
	var evtSource;
	var ctx;
	var buffer;
	var bctx;
	var imgWidth;
	var imgHeight;
	const img = new Image();
	var clearTimer;
	var pendingUpdates = [];
	var animationFrameId = null;
	var lastFrameTime = 0;
	var sseStopped = false;

	// About a half second of backlog at the 40fps fppd defaults to.  Reaching
	// this means we are not keeping up at all, not just jittering.
	const MAX_PENDING_FRAMES = 20;
	const RESYNC_MIN_INTERVAL = 5000;
	var lastResyncTime = 0;

	function initCanvas() {
		const canvas = document.getElementById('vCanvas');
		ctx = canvas.getContext("2d");
		ctx.fillStyle = "black";
		ctx.fillRect(0, 0, canvasWidth * window.devicePixelRatio, canvasHeight * window.devicePixelRatio);

		img.addEventListener("load", () => {
			const cratio = canvas.width / canvas.height;
			const iratio = img.width / img.height;
			if (cratio > iratio) {
				var neww = canvasHeight * iratio;
				imgWidth = neww;
				imgHeight = canvasHeight;
			} else {
				var newh = canvasWidth / iratio;
				imgWidth = canvasWidth;
				imgHeight = newh;
			}
			ctx.drawImage(img, pad, pad, imgWidth, imgHeight);
		});
		img.src = 'api/file/Images/virtualdisplaybackground.jpg';

		buffer = document.createElement('canvas');
		buffer.width = canvas.width * window.devicePixelRatio;
		buffer.height = canvas.height * window.devicePixelRatio;
		bctx = buffer.getContext('2d');

		clearPixelBuffer();
	}

	// Reset every pixel to black.  fppd only sends us pixels that changed since
	// the last frame, and it zeroes its own copy of that state whenever a client
	// connects, so we have to start from the same black state it assumes - both
	// on first load and on any reconnect.
	function clearPixelBuffer() {
		if (!bctx)
			return;

		bctx.fillStyle = '#000000';

		for (var key in cellColors) {
			// Draw a circle if size is greater than one pixel
			if (cellColors[key].size > 1) {
				bctx.beginPath();
				bctx.arc(cellColors[key].x, cellColors[key].y, parseInt(cellColors[key].size / 2.0), 0, 2 * Math.PI);
				bctx.fill();
			} else {
				bctx.fillRect(cellColors[key].x, cellColors[key].y, 1, 1);
			}
		}
	}

	function processEvent(e) {
		var pixels = e.data.split('|');

		clearTimeout(clearTimer);

		// Queue the update - each frame only carries the pixels that changed, so
		// dropping an intermediate frame would leave those pixels permanently
		// stale.  renderFrame() drains the whole queue in one go.
		pendingUpdates.push(pixels);

		// Safety valve.  Because we can't discard a delta without corrupting the
		// display, a producer faster than we can draw would grow this queue
		// without bound and each frame would take longer than the last.
		if (pendingUpdates.length > MAX_PENDING_FRAMES) {
			var sinceResync = Date.now() - lastResyncTime;

			if (sinceResync > RESYNC_MIN_INTERVAL) {
				// Reconnect: fppd resets its pixel state for a new connection,
				// so this resyncs us from a clean slate.
				console.warn('Virtual display fell ' + pendingUpdates.length +
					' frames behind, resyncing');
				resyncSSE();
			} else {
				// We resynced moments ago and are behind again, so this client
				// simply can't draw at the rate fppd is sending.  Reconnecting
				// on a loop would only make it worse - each one triggers a full
				// resend.  Keep just the newest frame instead and accept that
				// some pixels are stale until they next change; the next
				// eligible resync will clean them up.
				pendingUpdates = [pendingUpdates[pendingUpdates.length - 1]];
			}
			return;
		}

		// Clear the display after 6 seconds if no more events.  Max update time in test mode is 5 seconds.
		clearTimer = setTimeout(function () { ctx.drawImage(img, pad, pad, imgWidth, imgHeight); }, 6000);
	}

	// Apply one frame's worth of changed pixels to the off-screen buffer
	function applyPixels(pixels) {
		bctx.lineWidth = 0;
		bctx.strokeStyle = '#000000';

		for (i = 0; i < pixels.length; i++) {
			// color:pixel;pixel;pixel|color:pixel|color:pixel;pixel
			var data = pixels[i].split(':');

			var rgb = data[0];

			var r = base64[rgb.substring(0, 1)];
			var g = base64[rgb.substring(1, 2)];
			var b = base64[rgb.substring(2, 3)];

			bctx.fillStyle = '#' + r + g + b;

			// Uncomment to see the incoming color and location data in real time
			// $('#data').html(bctx.fillStyle + ' => ' + data[1] + '<br>' + $('#data').html().substring(0,500));

			var locs = data[1].split(';');
			for (j = 0; j < locs.length; j++) {
				var s = scaleMap[locs[j]];

				// Draw a circle if size is greater than one pixel
				if (s.size > 1) {
					bctx.beginPath();
					bctx.arc(s.x, s.y, parseInt(s.size / 2.0), 0, 2 * Math.PI);
					bctx.stroke();
					bctx.fill();
				} else {
					bctx.fillRect(s.x, s.y, 1, 1);
				}
			}
		}
	}

	// Free-running render loop, driven by the display rather than by packet
	// arrival.  It applies at most one queued frame per displayed frame:
	// draining the whole backlog into a single present would collapse several
	// sequence frames into one visible update, which is what made playback look
	// jerky.  fppd is capped well below display refresh, so the queue still
	// drains faster than it fills and no delta is ever discarded.
	function renderFrame(timestamp) {
		animationFrameId = requestAnimationFrame(renderFrame);

		if (pendingUpdates.length == 0)
			return;

		applyPixels(pendingUpdates.shift());

		// Single screen update per displayed frame
		ctx.drawImage(buffer, 0, 0);
	}

	function startRenderLoop() {
		if (!animationFrameId)
			animationFrameId = requestAnimationFrame(renderFrame);
	}

	function stopRenderLoop() {
		if (animationFrameId) {
			cancelAnimationFrame(animationFrameId);
			animationFrameId = null;
		}
	}

	function startSSE() {
		if (evtSource)
			return;

		evtSource = new EventSource('api/http-virtual-display/');

		evtSource.onopen = function () {
			// fppd resets its per-pixel state for every new connection, so drop
			// anything queued from the old stream and match its black baseline.
			pendingUpdates = [];
			clearPixelBuffer();
			startRenderLoop();
		};

		evtSource.onmessage = function (event) {
			processEvent(event);
		};
	}

	// Tear the stream down and start a new one, which gives both ends a fresh
	// black baseline to diff against
	function resyncSSE() {
		lastResyncTime = Date.now();
		pendingUpdates = [];

		if (evtSource) {
			evtSource.close();
			evtSource = null;
		}

		if (!sseStopped && !document.hidden)
			startSSE();
	}

	function stopSSE() {
		$('#stopButton').hide();

		// Explicit stop - don't let the visibility handler bring it back
		sseStopped = true;

		if (evtSource) {
			evtSource.close();
			evtSource = null;
		}
		pendingUpdates = [];
		stopRenderLoop();
	}

	function setupSSEClient() {
		initCanvas();

		startSSE();
	}

	// Browsers stop servicing requestAnimationFrame on a hidden tab, so nothing
	// would drain pendingUpdates while we're in the background.  Drop the stream
	// instead of queueing frames nobody is going to draw.
	document.addEventListener('visibilitychange', function () {
		if (document.hidden) {
			if (evtSource) {
				evtSource.close();
				evtSource = null;
			}
			pendingUpdates = [];
			stopRenderLoop();
		} else if (!sseStopped) {
			startSSE();
		}
	});

	window.addEventListener('beforeunload', function () {
		if (evtSource) {
			evtSource.close();
			evtSource = null;
		}
	});

	$(document).ready(function () {
		setupSSEClient();
	});

</script>

<input type='button' id='stopButton' onClick='stopSSE();' value='Stop Virtual Display'><br>
<table border=0>
	<tr>
		<td valign='top'>
			<canvas id='vCanvas' width='<?= $canvasWidth ?>px' height='<?= $canvasHeight ?>px'></canvas>
		</td>
		<td id='data'></td>
	</tr>
</table>