<script type="text/javascript" src="jquery/jcanvas.js"></script>
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
for ($i = 0; $i < 64; $i++)
{
	printf("base64['%s'] = '%02X';\n", substr($base64, $i, 1), $i << 2);
}

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
		$canvasHeight = (int)($canvasWidth * $previewHeight / $previewWidth);
	else
		$canvasWidth = (int)($canvasHeight * $previewWidth / $previewHeight);

	echo "var previewWidth = " . $previewWidth . ";\n";
	echo "var previewHeight = " . $previewHeight . ";\n";

	$scale = 1.0 * $canvasWidth / $previewWidth;

	$scaleMap = Array();

	while (!feof($f)) {
		$line = fgets($f);
		if (($line == "") || (preg_match('/^#/', $line)))
			continue;

		$line = trim($line);
		$entry = explode(",", $line, 6);

		$ox = $entry[0];
		$oy = $previewHeight - $entry[1];
		$oz = $entry[2];
		$x = (int)($ox * $scale);
		$y = (int)($oy * $scale);
		$z = (int)($oz * $scale);
		$ch = $entry[3];
		$colors = $entry[4];
		$type = $entry[5];
		$iy = $canvasHeight - $y;

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

		if (!isset($scaleMap[$key]))
		{
			$scaleMap[$key] = 1;
			echo "scaleMap['" . $key . "'] = { x: $x, y: $y, ox: $ox, oy: $oy };\n";
			echo "cellColors['$x,$y'] = { x: $x, y: $y, color: '#000000'};\n";
		}
	}
	fclose($f);
}

?>

var canvasWidth = <? echo $canvasWidth; ?>;
var canvasHeight = <? echo $canvasHeight; ?>;
var evtSource;
var ctx;

$.jCanvas.defaults.fromCenter = false;

function initCanvas()
{
	$('#vCanvas').removeLayers();
	$('#vCanvas').clearCanvas();

	$('#vCanvas').drawRect({
		layer: true,
		fillStyle: '#000',
		x: 0,
		y: 0,
		width: canvasWidth,
		height: canvasHeight
	});

	$('#vCanvas').drawImage({
		layer: true,
		opacity: 0.2,
		source: '/fppxml.php?command=getFile&filename=virtualdisplaybackground.jpg&dir=Images',
		width: canvasWidth,
		height: canvasHeight
	});

	var c = document.getElementById('vCanvas');
	ctx = c.getContext('2d');

	// Draw the black pixels
	ctx.fillStyle = '#000000';
	for (var key in cellColors)
	{
		ctx.fillRect(cellColors[key].x, cellColors[key].y, 1, 1);
	}
}

function processEvent(e)
{
	var pixels = e.data.split('|');

	for (i = 0; i < pixels.length; i++)
	{
		// color:pixel;pixel;pixel|color:pixel|color:pixel;pixel
		var data = pixels[i].split(':');

		var rgb = data[0];

		var r = base64[rgb.substring(0,1)];
		var g = base64[rgb.substring(1,2)];
		var b = base64[rgb.substring(2,3)];

		ctx.fillStyle = '#' + r + g + b;

		// Uncomment to see the incoming color and location data in real time
		// $('#data').html(ctx.fillStyle + ' => ' + data[1] + '<br>' + $('#data').html().substring(0,500));

		var locs = data[1].split(';');
		for (j = 0; j < locs.length; j++)
		{
			var s = scaleMap[locs[j]];

			ctx.fillRect(s.x, s.y, 1, 1);
		}
	}
}

function startSSE()
{
	evtSource = new EventSource('//<?php echo $_SERVER['SERVER_ADDR'] ?>:32328/');

	evtSource.onmessage = function(event) {
		processEvent(event);
	};
}

function stopSSE()
{
	$('#stopButton').hide();

	evtSource.close();
}

function setupSSEClient()
{
	initCanvas();

	startSSE();
}

$(document).ready(function() {
	setupSSEClient();
});

</script>

<input type='button' id='stopButton' onClick='stopSSE();' value='Stop Virtual Display'><br>
<table border=0>
<tr><td valign='top'>
<canvas id='vCanvas' width='<? echo $canvasWidth; ?>' height='<? echo $canvasHeight; ?>'></canvas></td>
<td id='data'></td></tr></table>

