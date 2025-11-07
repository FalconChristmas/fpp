<script src="js/three.min.js"></script>
<script>
    var pixelData = [];
    var pixelLookup = {};  // Fast lookup by key
    var base64 = [];
    var scene, camera, renderer, controls;
    var pixelMeshes = [];
    var evtSource;
    var clearTimer;
    var pendingUpdates = [];
    var animationFrameId = null;
    window.brightnessMultiplier = 2.0;  // Default brightness

    <?php
    // Canvas size for the 3D viewport
    if (!isset($canvasWidth))
        $canvasWidth = 1024;

    if (!isset($canvasHeight))
        $canvasHeight = 768;

    $base64 = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz+/"; // Base64 index table
    for ($i = 0; $i < 64; $i++) {
        printf("base64['%s'] = '%02X';\n", substr($base64, $i, 1), $i << 2);
    }

    $pixelSize = 1;
    $configFile = $settings['configDirectory'] . "/co-other.json";
    if (file_exists($configFile)) {
        $content = file_get_contents($configFile);
        $json = json_decode($content, true);
        if ($json && isset($json['channelOutputs'])) {
            foreach ($json['channelOutputs'] as $output) {
                if ($output['type'] == 'HTTPVirtualDisplay') {
                    $pixelSize = $output['pixelSize'];
                    break;
                }
            }
        }
    }
    echo "var pixelSize = " . $pixelSize . ";\n";

    $mapFile = $settings['configDirectory'] . '/virtualdisplaymap';
    if (!file_exists($mapFile)) {
        echo "console.error('virtualdisplaymap file not found at: " . $mapFile . "');\n";
        echo "var previewWidth = 1920;\n";
        echo "var previewHeight = 1080;\n";
        echo "var modelBounds = { minX: 0, maxX: 100, minY: 0, maxY: 100, minZ: 0, maxZ: 100 };\n";
        echo "var modelCenter = { x: 50, y: 50, z: 50 };\n";
    } else {
        $f = fopen($mapFile, "r");
        if ($f) {
            $line = fgets($f);
            if (preg_match('/^#/', $line))
                $line = fgets($f);
            $line = trim($line);
            $parts = explode(',', $line);
            $previewWidth = $parts[0];
            $previewHeight = $parts[1];

            echo "var previewWidth = " . $previewWidth . ";\n";
            echo "var previewHeight = " . $previewHeight . ";\n";

            // Find min/max for Z axis to center the model
            $minZ = PHP_INT_MAX;
            $maxZ = PHP_INT_MIN;
            $minX = PHP_INT_MAX;
            $maxX = PHP_INT_MIN;
            $minY = PHP_INT_MAX;
            $maxY = PHP_INT_MIN;
            $tempData = array();

            while (!feof($f)) {
                $line = fgets($f);
                if (($line == "") || (preg_match('/^#/', $line)))
                    continue;

                $line = trim($line);
                $entry = explode(",", $line);

                if (count($entry) < 3)
                    continue;

                $ox = (int) $entry[0];
                $oy = (int) $entry[1];
                $oz = (int) $entry[2];

                $minX = min($minX, $ox);
                $maxX = max($maxX, $ox);
                $minY = min($minY, $oy);
                $maxY = max($maxY, $oy);
                $minZ = min($minZ, $oz);
                $maxZ = max($maxZ, $oz);

                $tempData[] = $entry;
            }

            // Center coordinates horizontally, but align bottom to ground
            $centerX = ($minX + $maxX) / 2;
            $centerY = $minY;  // Align bottom to ground (Y=0)
            $centerZ = ($minZ + $maxZ) / 2;

            echo "var modelBounds = { minX: $minX, maxX: $maxX, minY: $minY, maxY: $maxY, minZ: $minZ, maxZ: $maxZ };\n";
            echo "var modelCenter = { x: $centerX, y: $centerY, z: $centerZ };\n";
            echo "console.log('Loaded " . count($tempData) . " pixels from virtualdisplaymap');\n";

            // Now output pixel data with centered coordinates
            $pixelIndex = 0;
            foreach ($tempData as $entry) {
                $ox = (int) $entry[0];
                $rawY = (int) $entry[1];
                // IMPORTANT: Must invert Y for key generation to match 2D version and SSE data
                $oy = $previewHeight - $rawY;
                $oz = (int) $entry[2];
                $ch = isset($entry[3]) ? $entry[3] : 0;
                $colors = isset($entry[4]) ? $entry[4] : '';

                // Center the model (use raw Y for 3D positioning, inverted oy for key)
                $x = $ox - $centerX;
                $y = $rawY - $centerY;
                $z = $oz - $centerZ;

                $ps = 1;
                if (sizeof($entry) >= 7) {
                    $ps = (int) $entry[6];
                }
                $ps *= $pixelSize;

                // Generate key for SSE lookup (same as 2D version - must use inverted oy)
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

                echo "pixelData.push({ key: '" . $key . "', x: $x, y: $y, z: $z, ox: $ox, oy: $oy, oz: $oz, size: $ps });\n";
                echo "pixelLookup['" . $key . "'] = " . $pixelIndex . ";\n";
                $pixelIndex++;
            }

            fclose($f);
        }
    }

    ?>

    var canvasWidth = <?php echo $canvasWidth; ?>;
    var canvasHeight = <?php echo $canvasHeight; ?>;

    function init3D() {
        console.log('Initializing 3D view with', pixelData.length, 'pixels');

        // Create scene
        scene = new THREE.Scene();
        scene.background = new THREE.Color(0x000000);

        // Create camera
        var modelSize = Math.max(
            modelBounds.maxX - modelBounds.minX,
            modelBounds.maxY - modelBounds.minY,
            modelBounds.maxZ - modelBounds.minZ
        );

        // Calculate tight zoom: find the bounding sphere radius
        var maxRadius = Math.sqrt(
            Math.pow(modelBounds.maxX - modelCenter.x, 2) +
            Math.pow(modelBounds.maxY - modelCenter.y, 2) +
            Math.pow(modelBounds.maxZ - modelCenter.z, 2)
        );

        // Position camera to fit all pixels with minimal padding
        // Factor of 1.2 gives ~10% padding, adjust FOV consideration
        var fov = 75;
        var fovRadians = fov * Math.PI / 180;
        var cameraDistance = maxRadius / Math.tan(fovRadians / 2) * 1.2;

        console.log('Model size:', modelSize, 'Camera distance:', cameraDistance);

        camera = new THREE.PerspectiveCamera(fov, canvasWidth / canvasHeight, 0.1, 10000);

        // Position camera: 20 degrees to the right, 10 degrees looking down
        // Convert angles to radians
        var horizontalAngle = 20 * Math.PI / 180;  // 20 degrees right
        var verticalAngle = 10 * Math.PI / 180;    // 10 degrees up (positive = looking down from above)

        // Calculate camera position using spherical coordinates
        var x = cameraDistance * Math.cos(verticalAngle) * Math.sin(horizontalAngle);
        var y = cameraDistance * Math.sin(verticalAngle);
        var z = cameraDistance * Math.cos(verticalAngle) * Math.cos(horizontalAngle);

        camera.position.set(x, y, z);
        camera.lookAt(0, 0, 0);

        // Create renderer
        renderer = new THREE.WebGLRenderer({ antialias: true });
        renderer.setSize(canvasWidth, canvasHeight);

        var container = document.getElementById('canvas-container');
        if (!container) {
            console.error('canvas-container element not found!');
            return;
        }
        container.appendChild(renderer.domElement);
        console.log('Renderer attached to DOM');

        // Add lights
        var ambientLight = new THREE.AmbientLight(0x404040);
        scene.add(ambientLight);

        var directionalLight = new THREE.DirectionalLight(0xffffff, 0.5);
        directionalLight.position.set(1, 1, 1);
        scene.add(directionalLight);

        // Use Points (point cloud) for efficient rendering of many pixels
        console.log('Creating point cloud with', pixelData.length, 'pixels');

        var geometry = new THREE.BufferGeometry();
        var positions = new Float32Array(pixelData.length * 3);
        var colors = new Float32Array(pixelData.length * 3);

        for (var i = 0; i < pixelData.length; i++) {
            positions[i * 3] = pixelData[i].x;
            positions[i * 3 + 1] = pixelData[i].y;
            positions[i * 3 + 2] = pixelData[i].z;

            // Start all black
            colors[i * 3] = 0;
            colors[i * 3 + 1] = 0;
            colors[i * 3 + 2] = 0;
        }

        geometry.setAttribute('position', new THREE.BufferAttribute(positions, 3));
        geometry.setAttribute('color', new THREE.BufferAttribute(colors, 3));

        var material = new THREE.PointsMaterial({
            size: 3,  // Default pixel size
            vertexColors: true,
            sizeAttenuation: true
        });

        // Store reference for updating size
        window.pixelMaterial = material;

        var points = new THREE.Points(geometry, material);
        scene.add(points);

        // Store reference for updates
        window.pixelGeometry = geometry;
        window.pixelColors = colors;

        console.log('Point cloud created');


        // Add axis helper for reference (default off)
        window.axesHelper = new THREE.AxesHelper(modelSize / 2);
        window.axesHelper.visible = false;
        scene.add(window.axesHelper);

        // Add grid helper
        window.gridHelper = new THREE.GridHelper(modelSize * 2, 20);
        scene.add(window.gridHelper);		// Simple orbit controls (mouse drag to rotate)
        setupOrbitControls();

        // Start render loop
        animate3D();

        console.log('3D initialization complete');
    }

    function setupOrbitControls() {
        // Simple manual orbit controls
        var isDragging = false;
        var isPanning = false;
        var previousMousePosition = { x: 0, y: 0 };
        var rotationSpeed = 0.005;
        var panSpeed = 0.5;
        var lookAtTarget = new THREE.Vector3(0, 0, 0);

        renderer.domElement.addEventListener('mousedown', function (e) {
            if (e.button === 0) {  // Left mouse button - rotate
                isDragging = true;
                previousMousePosition = { x: e.clientX, y: e.clientY };
            } else if (e.button === 1) {  // Middle mouse button - pan
                e.preventDefault();
                isPanning = true;
                previousMousePosition = { x: e.clientX, y: e.clientY };
            }
        });

        renderer.domElement.addEventListener('mousemove', function (e) {
            if (isDragging) {
                var deltaX = e.clientX - previousMousePosition.x;
                var deltaY = e.clientY - previousMousePosition.y;

                // Rotate around the look-at target
                var offset = new THREE.Vector3().subVectors(camera.position, lookAtTarget);

                // Rotate around Y axis (horizontal rotation)
                var angle = deltaX * rotationSpeed;
                offset.applyAxisAngle(new THREE.Vector3(0, 1, 0), -angle);

                // Rotate around camera's right axis (vertical rotation)
                var right = new THREE.Vector3(1, 0, 0).applyQuaternion(camera.quaternion);
                angle = deltaY * rotationSpeed;
                offset.applyAxisAngle(right, -angle);

                camera.position.copy(lookAtTarget).add(offset);
                camera.lookAt(lookAtTarget);

                previousMousePosition = { x: e.clientX, y: e.clientY };
            } else if (isPanning) {
                var deltaX = e.clientX - previousMousePosition.x;
                var deltaY = e.clientY - previousMousePosition.y;

                // Pan the camera and look-at target
                var right = new THREE.Vector3(1, 0, 0).applyQuaternion(camera.quaternion);
                var up = new THREE.Vector3(0, 1, 0).applyQuaternion(camera.quaternion);

                var distance = camera.position.distanceTo(lookAtTarget);
                var panAmount = distance * 0.001;

                right.multiplyScalar(-deltaX * panAmount * panSpeed);
                up.multiplyScalar(deltaY * panAmount * panSpeed);

                camera.position.add(right).add(up);
                lookAtTarget.add(right).add(up);
                camera.lookAt(lookAtTarget);

                previousMousePosition = { x: e.clientX, y: e.clientY };
            }
        });

        renderer.domElement.addEventListener('mouseup', function (e) {
            if (e.button === 0) {
                isDragging = false;
            } else if (e.button === 1) {
                isPanning = false;
            }
        });

        // Prevent context menu on middle click
        renderer.domElement.addEventListener('contextmenu', function (e) {
            e.preventDefault();
        });

        // Zoom with mouse wheel
        renderer.domElement.addEventListener('wheel', function (e) {
            e.preventDefault();
            var zoomSpeed = 0.1;
            var delta = e.deltaY > 0 ? 1 + zoomSpeed : 1 - zoomSpeed;
            camera.position.multiplyScalar(delta);
        });
    }

    function animate3D() {
        requestAnimationFrame(animate3D);

        // Process pending color updates
        if (pendingUpdates.length > 0) {
            var pixels = pendingUpdates.shift();
            updatePixelColors(pixels);
        }

        renderer.render(scene, camera);
    }

    function updatePixelColors(pixels) {
        var updated = false;
        var colorCount = 0;

        for (var i = 0; i < pixels.length; i++) {
            var data = pixels[i].split(':');
            if (!data[1]) continue; // Skip if no location data

            var rgb = data[0];

            // Convert from RGB666 (6-bit per channel) to 0.0-1.0 range
            // base64 array contains hex STRING values like '00', '04', '08', 'FC'
            // Same as 2D version - base64[char] returns a 2-character hex string
            var rHex = base64[rgb.charAt(0)];
            var gHex = base64[rgb.charAt(1)];
            var bHex = base64[rgb.charAt(2)];

            // Parse hex strings and normalize to 0.0-1.0, then apply brightness
            var r = Math.min(1.0, (parseInt(rHex, 16) / 255.0) * window.brightnessMultiplier);
            var g = Math.min(1.0, (parseInt(gHex, 16) / 255.0) * window.brightnessMultiplier);
            var b = Math.min(1.0, (parseInt(bHex, 16) / 255.0) * window.brightnessMultiplier);

            var locs = data[1].split(';');
            for (var j = 0; j < locs.length; j++) {
                // Fast lookup by key
                var pixelIndex = pixelLookup[locs[j]];
                if (pixelIndex !== undefined) {
                    // Update color in buffer
                    window.pixelColors[pixelIndex * 3] = r;
                    window.pixelColors[pixelIndex * 3 + 1] = g;
                    window.pixelColors[pixelIndex * 3 + 2] = b;
                    updated = true;
                }
            }
        }

        // Mark colors as needing update
        if (updated && window.pixelGeometry) {
            window.pixelGeometry.attributes.color.needsUpdate = true;
        }
    }

    function processEvent(e) {
        var pixels = e.data.split('|');
        clearTimeout(clearTimer);

        // Replace pending update with latest
        pendingUpdates = [pixels];

        // Clear the display after 6 seconds if no more events
        clearTimer = setTimeout(function () {
            // Set all pixels to black
            for (var i = 0; i < pixelData.length; i++) {
                window.pixelColors[i * 3] = 0;
                window.pixelColors[i * 3 + 1] = 0;
                window.pixelColors[i * 3 + 2] = 0;
            }
            if (window.pixelGeometry) {
                window.pixelGeometry.attributes.color.needsUpdate = true;
            }
        }, 6000);
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

    function toggleAxes() {
        if (window.axesHelper) {
            window.axesHelper.visible = !window.axesHelper.visible;
        }
    }

    function toggleGrid() {
        if (window.gridHelper) {
            window.gridHelper.visible = !window.gridHelper.visible;
        }
    }

    function updatePixelSize() {
        var size = parseFloat(document.getElementById('pixelSizeSlider').value);
        if (window.pixelMaterial) {
            window.pixelMaterial.size = size;
        }
        document.getElementById('pixelSizeValue').textContent = size.toFixed(1);
    }

    function updateBrightness() {
        var brightness = parseFloat(document.getElementById('brightnessSlider').value);
        window.brightnessMultiplier = brightness;
        document.getElementById('brightnessValue').textContent = brightness.toFixed(2) + 'x';
    }

    function setupClient() {
        init3D();
        startSSE();
    }

    $(document).ready(function () {
        setupClient();
    });

</script>

<style>
    #canvas-container {
        border: 1px solid #333;
        display: inline-block;
    }

    #controls {
        margin: 10px 0;
        padding: 10px;
        background-color: #f5f5f5;
        border: 1px solid #ddd;
        border-radius: 4px;
    }

    #controls input[type='button'] {
        padding: 6px 12px;
        margin-right: 8px;
        background-color: #fff;
        border: 1px solid #ccc;
        border-radius: 3px;
        cursor: pointer;
    }

    #controls input[type='button']:hover {
        background-color: #e6e6e6;
    }

    #controls span {
        margin-left: 15px;
        white-space: nowrap;
    }

    #controls input[type='range'] {
        width: 150px;
        vertical-align: middle;
    }

    .control-group {
        display: inline-block;
        margin-right: 20px;
    }
</style>

<div id="controls">
    <div style="margin-bottom: 8px;">
        <input type='button' id='stopButton' onClick='stopSSE();' value='Stop 3D Virtual Display'>
        <input type='button' onClick='toggleAxes();' value='Toggle Axes'>
        <input type='button' onClick='toggleGrid();' value='Toggle Grid'>
    </div>
    <div>
        <span class="control-group">
            Pixel Size: <input type='range' id='pixelSizeSlider' min='1' max='50' value='3' step='1'
                oninput='updatePixelSize();'>
            <span id='pixelSizeValue'>3.0</span>
        </span>
        <span class="control-group">
            Brightness: <input type='range' id='brightnessSlider' min='0.1' max='3.0' value='2.0' step='0.1'
                oninput='updateBrightness();'>
            <span id='brightnessValue'>2.00x</span>
        </span>
        <span style="margin-left: 15px; color: #666; font-style: italic;">
            Left-click: rotate | Middle-click: pan | Scroll: zoom
        </span>
    </div>
</div>
<div id='canvas-container'></div>
<div id='data'></div>