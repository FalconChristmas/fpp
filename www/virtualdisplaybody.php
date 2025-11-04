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

	$f = fopen($settings['configDirectory'] . '/virtualdisplaymap', "r");
	if ($f) {
		$line = fgets($f);
		if (preg_match('/^#/', $line))
			$line = fgets($f);
		$line = trim($line);
		$parts = explode(',', $line);
		$previewWidth = $parts[0];
		$previewHeight = $parts[1];

		if (($previewWidth / $previewHeight) > ($canvasWidth / $canvasHeight))
			$canvasHeight = (int) ($canvasWidth * $previewHeight / $previewWidth);
		else
			$canvasWidth = (int) ($canvasHeight * $previewWidth / $previewHeight);

		echo "var previewWidth = " . $previewWidth . ";\n";
		echo "var previewHeight = " . $previewHeight . ";\n";

		$scale = 1.0 * $canvasWidth / $previewWidth;

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
			$x = (int) ($ox * $scale);
			$y = (int) ($oy * $scale);
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
			ctx.drawImage(img, 0, 0, imgWidth, imgHeight);
		});
		img.src = 'api/file/Images/virtualdisplaybackground.jpg';

		buffer = document.createElement('canvas');
		buffer.width = canvas.width * window.devicePixelRatio;
		buffer.height = canvas.height * window.devicePixelRatio;
		bctx = buffer.getContext('2d');

		// Draw the black pixels
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

		// Replace pending update with latest (drop intermediate frames if behind)
		pendingUpdates = [pixels];

		// Request animation frame if not already pending
		if (!animationFrameId) {
			animationFrameId = requestAnimationFrame(renderFrame);
		}

		// Clear the display after 6 seconds if no more events.  Max update time in test mode is 5 seconds.
		clearTimer = setTimeout(function () { ctx.drawImage(img, 0, 0, imgWidth, imgHeight); }, 6000);
	}

	function renderFrame(timestamp) {
		animationFrameId = null;

		// Calculate FPS (optional, for debugging)
		// var fps = 1000 / (timestamp - lastFrameTime);
		// lastFrameTime = timestamp;

		// Process all pending updates in batch
		while (pendingUpdates.length > 0) {
			var pixels = pendingUpdates.shift();

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

				bctx.lineWidth = 0;
				bctx.strokeStyle = '#000000';

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

		// Single screen update per frame
		ctx.drawImage(buffer, 0, 0);
	}

	function startSSE() {
		evtSource = new EventSource('api/http-virtual-display/');

		evtSource.onmessage = function (event) {
			processEvent(event);
		};
	}

	function stopSSE() {
		$('#stopButton').hide();

		evtSource.close();
	}

	function setupSSEClient() {
		initCanvas();

		startSSE();
	}

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