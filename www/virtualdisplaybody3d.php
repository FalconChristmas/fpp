<?php
// Ensure $standalone is defined (default to false if not set)
if (!isset($standalone)) {
    $standalone = false;
}
?>
<script>
    var pixelData = [];
    var pixelLookup = {};  // Fast lookup by pixel index for 3D mode
    var base64 = [];
    var scene, camera, renderer, controls;
    var pixelMeshes = [];
    var evtSource;
    var clearTimer;
    var pendingUpdates = [];
    var animationFrameId = null;
    var lookAtTarget = null;  // Global camera target for orbit controls (initialized when THREE.js loads)
    var cameraControlState = { distanceMin: 100, distanceMax: 5000, initialized: false };
    var pixelOffsetAdvancedVisible = false;
    window.brightnessMultiplier = 2.0;  // Default brightness (can be overridden by URL param)
    var testMode = {
        enabled: false,
        color: '#ff0000',
        rgb: [1.0, 0.0, 0.0]
    };

    // 3D Objects variables
    var loadedObjects = [];
    var objLoader = null;
    var mtlLoader = null;

    // URL parameter configuration
    var urlParams = {};

    // Parse URL parameters
    function parseURLParams() {
        var params = {};
        var queryString = window.location.search.substring(1);
        if (queryString) {
            var pairs = queryString.split('&');
            for (var i = 0; i < pairs.length; i++) {
                var pair = pairs[i].split('=');
                var key = decodeURIComponent(pair[0]);
                var value = decodeURIComponent(pair[1] || '');
                params[key] = value;
            }
        }
        return params;
    }

    // Get URL parameter with default value
    function getURLParam(name, defaultValue) {
        if (urlParams.hasOwnProperty(name)) {
            var value = urlParams[name];

            // Handle boolean values
            if (value === 'true') return true;
            if (value === 'false') return false;

            // Handle numeric values
            if (!isNaN(value) && value !== '') {
                return parseFloat(value);
            }

            return value;
        }
        return defaultValue;
    }

    // Initialize URL parameters
    urlParams = parseURLParams();

    <?php
    // Canvas size for the 3D viewport
    if (!isset($canvasWidth))
        $canvasWidth = 1024;

    if (!isset($canvasHeight))
        $canvasHeight = 768;

    echo "var canvasWidth = " . $canvasWidth . ";\n";
    echo "var canvasHeight = " . $canvasHeight . ";\n";

    // Override with window size if available (standalone mode)
    echo "if (typeof window.canvasWidth !== 'undefined') canvasWidth = window.canvasWidth;\n";
    echo "if (typeof window.canvasHeight !== 'undefined') canvasHeight = window.canvasHeight;\n";

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

                // Store raw coordinates - we'll center them in JavaScript using gridlines offset
                $x = $ox;
                $y = $rawY;
                $z = $oz;

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
                echo "pixelLookup[" . $pixelIndex . "] = " . $pixelIndex . ";\n";
                $pixelIndex++;
            }

            fclose($f);
        }
    }

    // Load 3D asset configuration
    $assetsConfigFile = $settings['configDirectory'] . '/virtdisplay.json';
    $assetsConfig = null;
    $viewObjects = array();

    if (file_exists($assetsConfigFile)) {
        $assetsContent = file_get_contents($assetsConfigFile);
        $assetsConfig = json_decode($assetsContent, true);

        if ($assetsConfig && isset($assetsConfig['view_objects'])) {
            $viewObjects = $assetsConfig['view_objects'];
            echo "console.log('Loaded " . count($viewObjects) . " view objects from virtdisplay.json');\n";

            // Find gridlines to determine the coordinate system offset
            // Gridlines position tells us where (0,0,0) is in the xLights coordinate system
            $gridlinesOffsetX = 0;
            $gridlinesOffsetY = 0;
            $gridlinesOffsetZ = 0;

            // Calculate min/max object positions to determine the offset xLights applied to virtualmap
            // xLights subtracts minX and minY from all pixel coordinates when creating the map
            $objectMinX = 0;
            $objectMinY = 0;
            $objectMinZ = 0;
            $hasActiveObjects = false;

            foreach ($viewObjects as $obj) {
                if (isset($obj['DisplayAs']) && $obj['DisplayAs'] === 'Gridlines') {
                    $gridlinesOffsetX = (float) $obj['WorldPosX'];
                    $gridlinesOffsetY = (float) $obj['WorldPosY'];
                    $gridlinesOffsetZ = (float) $obj['WorldPosZ'];
                }

                // Track minimum object positions (active mesh objects only)
                if (
                    isset($obj['Active']) && $obj['Active'] === '1' &&
                    isset($obj['DisplayAs']) && $obj['DisplayAs'] === 'Mesh'
                ) {
                    $objX = (float) ($obj['WorldPosX'] ?? 0);
                    $objY = (float) ($obj['WorldPosY'] ?? 0);
                    $objZ = (float) ($obj['WorldPosZ'] ?? 0);

                    if (!$hasActiveObjects) {
                        $objectMinX = $objX;
                        $objectMinY = $objY;
                        $objectMinZ = $objZ;
                        $hasActiveObjects = true;
                    } else {
                        $objectMinX = min($objectMinX, $objX);
                        $objectMinY = min($objectMinY, $objY);
                        $objectMinZ = min($objectMinZ, $objZ);
                    }
                }
            }

            echo "console.log('Found gridlines at X=$gridlinesOffsetX, Y=$gridlinesOffsetY, Z=$gridlinesOffsetZ');\n";
            echo "console.log('Pixel center is X=$centerX, Y=$centerY, Z=$centerZ');\n";
            echo "console.log('Object min positions: X=$objectMinX, Y=$objectMinY, Z=$objectMinZ');\n";
            echo "var gridlinesOffset = { x: $gridlinesOffsetX, y: $gridlinesOffsetY, z: $gridlinesOffsetZ };\n";
            echo "var objectMinPosition = { x: $objectMinX, y: $objectMinY, z: $objectMinZ };\n";
        } else {
            echo "console.warn('No view_objects found in virtdisplay.json');\n";
            echo "var gridlinesOffset = { x: 0, y: 0, z: 0 };\n";
            echo "var objectMinPosition = { x: 0, y: 0, z: 0 };\n";
        }
    } else {
        echo "console.warn('virtdisplay.json not found at: " . $assetsConfigFile . "');\n";
        echo "var gridlinesOffset = { x: 0, y: 0, z: 0 };\n";
        echo "var objectMinPosition = { x: 0, y: 0, z: 0 };\n";
    }

    // Output the view objects configuration to JavaScript
    echo "var viewObjects = " . json_encode($viewObjects) . ";\n";
    echo "var assetsDirectory = '" . $settings['virtualDisplayAssetsDirectory'] . "';\n";

    // Get the 2D settings for coordinate system info
    $previewWidth2D = isset($assetsConfig['2D-settings']['previewWidth']) ? $assetsConfig['2D-settings']['previewWidth'] : $previewWidth;
    $previewHeight2D = isset($assetsConfig['2D-settings']['previewHeight']) ? $assetsConfig['2D-settings']['previewHeight'] : $previewHeight;
    $centerAt0 = isset($assetsConfig['2D-settings']['Display2DCenter0']) ? $assetsConfig['2D-settings']['Display2DCenter0'] == '1' : false;

    echo "var view2DSettings = { previewWidth: $previewWidth2D, previewHeight: $previewHeight2D, centerAt0: " . ($centerAt0 ? 'true' : 'false') . " };\n";
    echo "console.log('2D View settings:', view2DSettings);\n";

    // Also pass the 2D settings if available for coordinate system reference
    if ($assetsConfig && isset($assetsConfig['2D-settings'])) {
        $settings2D = $assetsConfig['2D-settings'];
        echo "var display2DSettings = " . json_encode($settings2D) . ";\n";
        echo "console.log('2D display settings:', display2DSettings);\n";
    }

    ?>

    var objectCenterOffset = { x: 0, y: 0, z: 0 };  // Will be calculated from gridlines

    function calculateObjectCenterOffset() {
        // Objects are positioned relative to the gridlines (0,0,0 in xLights)
        // They should NOT be offset - they use their original world coordinates
        // Only pixels need centering to bring them into this same coordinate space
        objectCenterOffset.x = 0;
        objectCenterOffset.y = 0;
        objectCenterOffset.z = 0;

        console.log('Object center offset (no offset - objects use world coords):', objectCenterOffset);
        console.log('Pixel model center (pixels will be centered by this):', modelCenter);
    }

    function init3D() {
        console.log('Initializing 3D view with', pixelData.length, 'pixels');
        console.log('Three.js version info:', THREE.REVISION || 'version unknown');

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
        var fov = getURLParam('fov', 75);
        var fovRadians = fov * Math.PI / 180;
        var cameraDistance = maxRadius / Math.tan(fovRadians / 2) * 1.2;

        // Allow URL parameter to override camera distance (zoom level)
        var zoomParam = getURLParam('zoom', null);
        if (zoomParam !== null) {
            // zoom parameter is a multiplier (1.0 = default, 0.5 = half distance, 2.0 = double distance)
            cameraDistance = cameraDistance * zoomParam;
        }

        console.log('Model size:', modelSize, 'Camera distance:', cameraDistance);

        camera = new THREE.PerspectiveCamera(fov, canvasWidth / canvasHeight, 0.1, 10000);

        // Check if camera position is specified in URL parameters
        var cameraX = getURLParam('cameraX', null);
        var cameraY = getURLParam('cameraY', null);
        var cameraZ = getURLParam('cameraZ', null);

        if (cameraX !== null && cameraY !== null && cameraZ !== null) {
            // Use explicit camera position from URL
            camera.position.set(cameraX, cameraY, cameraZ);
            console.log('Using camera position from URL:', cameraX, cameraY, cameraZ);
        } else {
            // Calculate default camera position: 20 degrees to the right, 10 degrees looking down
            // Allow override via angles in URL
            var horizontalAngle = getURLParam('cameraAngleH', 20) * Math.PI / 180;  // degrees right
            var verticalAngle = getURLParam('cameraAngleV', 10) * Math.PI / 180;    // degrees up (positive = looking down from above)

            // Calculate camera position using spherical coordinates
            var x = cameraDistance * Math.cos(verticalAngle) * Math.sin(horizontalAngle);
            var y = cameraDistance * Math.sin(verticalAngle);
            var z = cameraDistance * Math.cos(verticalAngle) * Math.cos(horizontalAngle);

            camera.position.set(x, y, z);
            console.log('Using calculated camera position:', x, y, z);
        }

        // Check if camera target (lookAt) is specified in URL parameters
        var targetX = getURLParam('targetX', 0);
        var targetY = getURLParam('targetY', 0);
        var targetZ = getURLParam('targetZ', 0);
        camera.lookAt(targetX, targetY, targetZ);
        console.log('Camera looking at:', targetX, targetY, targetZ);

        // Initialize lookAtTarget for orbit controls
        lookAtTarget = new THREE.Vector3(targetX, targetY, targetZ);

        // Create renderer with modern settings (r182 improvements)
        renderer = new THREE.WebGLRenderer({ antialias: true });
        renderer.setSize(canvasWidth, canvasHeight);

        // Enable accurate color space management
        renderer.outputColorSpace = THREE.SRGBColorSpace;

        // Enable physically accurate tone mapping for better color reproduction
        renderer.toneMapping = THREE.ACESFilmicToneMapping;
        renderer.toneMappingExposure = 1.0;

        // Use physical light units (more realistic)
        renderer.useLegacyLights = false;

        var container = document.getElementById('canvas-container');
        if (!container) {
            console.error('canvas-container element not found!');
            return;
        }
        container.appendChild(renderer.domElement);
        console.log('Renderer attached to DOM');

        // Add lights with physical units (lumens) for realistic lighting
        var ambientLight = new THREE.AmbientLight(0x404040, Math.PI * 0.5);
        scene.add(ambientLight);

        var directionalLight = new THREE.DirectionalLight(0xffffff, Math.PI * 0.3);
        directionalLight.position.set(1, 1, 1);
        scene.add(directionalLight);

        // Add additional lights for better 3D object visibility
        var directionalLight2 = new THREE.DirectionalLight(0xffffff, Math.PI * 0.3);
        directionalLight2.position.set(-1, -1, -1);
        scene.add(directionalLight2);

        var hemisphereLight = new THREE.HemisphereLight(0xffffff, 0x444444, Math.PI * 0.4);
        hemisphereLight.position.set(0, 1, 0);
        scene.add(hemisphereLight);

        // Store references for ambient light control (store base intensities in physical units)
        window.sceneLights = {
            ambient: { light: ambientLight, baseIntensity: Math.PI * 0.5 },
            directional1: { light: directionalLight, baseIntensity: Math.PI * 0.3 },
            directional2: { light: directionalLight2, baseIntensity: Math.PI * 0.3 },
            hemisphere: { light: hemisphereLight, baseIntensity: Math.PI * 0.4 }
        };

        // Use InstancedMesh for realistic 3D LED bulbs (r182 improvement)
        console.log('Creating instanced LED bulbs with', pixelData.length, 'pixels');

        // Debug: log first few pixel positions
        console.log('First 3 pixels - RAW coordinates:');
        for (var i = 0; i < Math.min(3, pixelData.length); i++) {
            console.log('  Pixel', i, '- x:', pixelData[i].x, 'y:', pixelData[i].y, 'z:', pixelData[i].z);
        }
        console.log('Model center:', modelCenter);
        console.log('Center at 0:', view2DSettings.centerAt0);
        console.log('Preview width:', view2DSettings.previewWidth);
        console.log('Gridlines offset:', gridlinesOffset);

        // Create a small sphere geometry for each LED (reused for all instances)
        var ledGeometry = new THREE.SphereGeometry(1.5, 8, 6);

        // Use MeshBasicMaterial for LEDs - unaffected by lighting, perfect for self-illuminating pixels
        var ledMaterial = new THREE.MeshBasicMaterial({
            color: 0xffffff,  // Base white for instance color multiplication
            transparent: false,
            depthWrite: false,   // Allow pixels behind each other to be visible
            depthTest: true
        });

        // Create instanced mesh
        var instancedMesh = new THREE.InstancedMesh(ledGeometry, ledMaterial, pixelData.length);

        // Set up position and color for each instance
        var matrix = new THREE.Matrix4();
        var color = new THREE.Color();
        var pixelPositions = [];

        for (var i = 0; i < pixelData.length; i++) {
            // Pixels may have Display2DCenter0 offset applied in the virtualmap file
            // If Display2DCenter0 is enabled, pixel X coordinates have previewWidth/2 subtracted
            // We need to add it back to get true xLights coordinates
            var pixelX = pixelData[i].x;
            var pixelY = pixelData[i].y;
            var pixelZ = pixelData[i].z;

            if (view2DSettings.centerAt0) {
                // Add back the 2D preview center to get true xLights X coordinate
                var offsetAmount = view2DSettings.previewWidth / 2;
                if (i === 0) {
                    console.log('Applying Display2DCenter0 correction: adding', offsetAmount, 'to X coordinates');
                }
                pixelX = pixelX + offsetAmount;
            }

            // Use raw pixel coordinates - the offset sliders can be used to align with objects
            // xLights virtualmap export subtracts minX/minY which we can't reliably recover
            // The user can adjust alignment using the offset sliders in the UI
            var finalX = pixelX;
            var finalY = pixelY;
            var finalZ = pixelZ;

            if (i < 3) {
                console.log('Pixel', i, 'coords: x=', finalX, 'y=', finalY, 'z=', finalZ);
            }

            pixelPositions.push({ x: finalX, y: finalY, z: finalZ });

            // Set position matrix for this instance
            matrix.setPosition(finalX, finalY, finalZ);
            instancedMesh.setMatrixAt(i, matrix);

            // Start all LEDs black
            color.setRGB(0, 0, 0);
            instancedMesh.setColorAt(i, color);
        }

        // Mark instance color as needing GPU update
        instancedMesh.instanceColor.needsUpdate = true;

        // Set render order high so pixels render on top of 3D objects
        instancedMesh.renderOrder = 999;

        // Debug: log first few pixel positions after centering
        console.log('First 3 pixels - CENTERED coordinates:');
        for (var i = 0; i < Math.min(3, pixelPositions.length); i++) {
            console.log('  Pixel', i, '- x:', pixelPositions[i].x, 'y:', pixelPositions[i].y, 'z:', pixelPositions[i].z);
        }

        scene.add(instancedMesh);

        // Store references for updates
        window.pixelMesh = instancedMesh;
        window.pixelMaterial = ledMaterial;
        window.pixelColors = new Float32Array(pixelData.length * 3);
        window.pixelPositions = pixelPositions;

        console.log('Instanced LED mesh created with realistic 3D bulbs');


        // Add axis helper for reference (default off, can be overridden by URL)
        window.axesHelper = new THREE.AxesHelper(modelSize / 2);
        window.axesHelper.visible = getURLParam('showAxes', false);
        scene.add(window.axesHelper);

        // Add grid helper (can be hidden via URL)
        window.gridHelper = new THREE.GridHelper(modelSize * 2, 20);
        window.gridHelper.visible = getURLParam('showGrid', true);
        scene.add(window.gridHelper);		// Simple orbit controls (mouse drag to rotate)
        setupOrbitControls();

        // Start render loop
        animate3D();

        // Initialize 3D object loaders
        if (initObjectLoaders()) {
            // Calculate the coordinate offset from gridlines
            calculateObjectCenterOffset();

            // Load 3D objects from configuration
            load3DObjects();
        } else {
            console.warn('3D object loaders failed to initialize, skipping 3D object loading');
        }

        // Load any saved pixel offset settings and apply them
        if (loadPixelOffsets()) {
            // Apply the loaded offsets to pixel positions
            setTimeout(function () {
                updatePixelOffset();
            }, 100);
        }

        console.log('3D initialization complete');
    }

    function initObjectLoaders() {
        // Initialize the OBJ and MTL loaders with new ES6 modules
        try {
            if (typeof window.OBJLoader === 'undefined') {
                console.error('OBJLoader is not available. Make sure Three.js modules are loaded.');
                return false;
            }
            if (typeof window.MTLLoader === 'undefined') {
                console.error('MTLLoader is not available. Make sure Three.js modules are loaded.');
                return false;
            }

            objLoader = new window.OBJLoader();
            mtlLoader = new window.MTLLoader();
            console.log('Successfully initialized 3D object loaders');
        } catch (error) {
            console.error('Failed to initialize 3D object loaders:', error);
            return false;
        }

        // Set the path for loading assets via web server
        if (typeof assetsDirectory !== 'undefined' && objLoader && mtlLoader) {
            var assetPath = '/virtualdisplay_assets/';
            console.log('Setting asset path to:', assetPath);
            mtlLoader.setPath(assetPath);
            objLoader.setPath(assetPath);
            return true;
        }
        return false;
    }

    function load3DObjects() {
        if (!viewObjects || viewObjects.length === 0) {
            console.log('No 3D objects to load');
            return;
        }

        console.log('Loading', viewObjects.length, '3D objects');

        // Check if loaders are available, otherwise use fallback
        var useLoaders = (objLoader && mtlLoader);

        viewObjects.forEach((objConfig, index) => {
            // Skip if not active
            if (objConfig.Active === "0") {
                console.log('Skipping inactive object:', objConfig.name);
                return;
            }

            // Handle different display types
            if (objConfig.DisplayAs === "Mesh" && objConfig.ObjFile) {
                if (useLoaders) {
                    loadMeshObject(objConfig, index);
                } else {
                    loadFallbackMesh(objConfig, index);
                }
            } else if (objConfig.DisplayAs === "Gridlines") {
                loadGridlines(objConfig, index);
            } else if (objConfig.DisplayAs === "Ruler") {
                loadRuler(objConfig, index);
            }
        });
    }

    function loadMeshObject(objConfig, index) {
        console.log('Loading mesh object:', objConfig.name, 'from', objConfig.ObjFile);

        // Check if there's a corresponding MTL file
        var mtlFile = objConfig.ObjFile.replace('.obj', '.mtl');

        // Try to load with MTL first
        console.log('Attempting to load materials from:', mtlFile);
        loadObjWithMaterial(objConfig.ObjFile, mtlFile, objConfig, index);
    }

    function loadObjWithMaterial(objPath, mtlPath, objConfig, index) {
        // Create a custom loading manager to intercept and fix texture paths
        var textureManager = new THREE.LoadingManager();

        // Intercept texture loading to fix paths
        textureManager.setURLModifier((url) => {
            // If URL contains a subdirectory path, strip it and use just the filename
            var filename = url.split('/').pop();
            var fixedUrl = '/virtualdisplay_assets/' + filename;

            if (url !== fixedUrl) {
                console.log('Fixed texture URL:', url, '->', fixedUrl);
            }

            return fixedUrl;
        });

        // Create a new MTL loader with the custom manager for texture path fixing
        var customMtlLoader = new window.MTLLoader(textureManager);
        customMtlLoader.setPath('/virtualdisplay_assets/');

        // Load MTL file with the custom loader
        customMtlLoader.load(
            mtlPath,
            (materials) => {
                console.log('Successfully loaded materials for:', objConfig.name);

                // Preload materials and textures
                materials.preload();

                console.log('Materials and textures preloaded for:', objConfig.name);

                // Create a new OBJ loader with the same texture manager
                var customObjLoader = new window.OBJLoader(textureManager);
                customObjLoader.setPath('/virtualdisplay_assets/');
                customObjLoader.setMaterials(materials);

                // Now load the OBJ with materials
                loadObjFileWithLoader(customObjLoader, objPath, objConfig, index, true);
            },
            (progress) => {
                // Progress callback
            },
            (error) => {
                console.warn('Could not load MTL file for', objConfig.name, '- using default materials');
                console.error('MTL load error:', error);
                // Fall back to loading without materials using the standard loader
                loadObjFile(objPath, objConfig, index, false);
            }
        );
    }

    function loadObjFileWithLoader(loader, objPath, objConfig, index, hasMaterials) {
        loader.load(
            objPath,
            (object) => {
                console.log('Successfully loaded 3D object:', objConfig.name);

                if (hasMaterials) {
                    console.log('Using loaded MTL materials for', objConfig.name);

                    // Log material details and enable anisotropic filtering for textures
                    var materialCount = 0;
                    var textureCount = 0;
                    var maxAnisotropy = renderer.capabilities.getMaxAnisotropy();
                    object.traverse((child) => {
                        if (child instanceof THREE.Mesh) {
                            materialCount++;
                            if (child.material.map) {
                                textureCount++;
                                // Enable anisotropic filtering for better texture quality at oblique angles
                                child.material.map.anisotropy = maxAnisotropy;
                                child.material.map.needsUpdate = true;
                                console.log('  Mesh has texture:', child.material.map.image?.src || 'loading...');
                            }
                        }
                    });
                    console.log('Applied', materialCount, 'materials with', textureCount, 'textures to', objConfig.name);
                }

                // Calculate and log bounding box for debugging
                var bbox = new THREE.Box3().setFromObject(object);
                console.log('Object bounding box:', objConfig.name, '- min:', bbox.min, 'max:', bbox.max);

                // Apply transformations
                applyObjectTransform(object, objConfig);

                // Add to scene
                scene.add(object);

                // Store reference
                loadedObjects[index] = {
                    object: object,
                    config: objConfig
                };

                console.log('Added 3D object to scene:', objConfig.name);

                // Update UI list
                updateObjectsList();
            },
            (progress) => {
                if (progress.lengthComputable) {
                    console.log('Loading progress for', objConfig.name, ':', (progress.loaded / progress.total * 100).toFixed(1) + '%');
                }
            },
            (error) => {
                console.error('Error loading 3D object', objConfig.name, ':', error);
            }
        );
    }

    function loadObjFile(objPath, objConfig, index, hasMaterials) {
        objLoader.load(
            objPath,
            (object) => {
                console.log('Successfully loaded 3D object:', objConfig.name);

                // If no materials were loaded, apply default materials
                if (!hasMaterials) {
                    var meshCount = 0;
                    object.traverse((child) => {
                        if (child instanceof THREE.Mesh) {
                            meshCount++;
                            // Apply a default material with reasonable properties
                            child.material = new THREE.MeshPhongMaterial({
                                color: 0xcccccc,  // Light gray default
                                flatShading: false,
                                side: THREE.DoubleSide,
                                shininess: 30
                            });
                        }
                    });
                    console.log('Applied default materials to', meshCount, 'meshes in', objConfig.name);
                } else {
                    console.log('Using loaded MTL materials for', objConfig.name);

                    // Log material details and enable anisotropic filtering for textures
                    var materialCount = 0;
                    var textureCount = 0;
                    var maxAnisotropy = renderer.capabilities.getMaxAnisotropy();
                    object.traverse((child) => {
                        if (child instanceof THREE.Mesh) {
                            materialCount++;
                            if (child.material.map) {
                                textureCount++;
                                // Enable anisotropic filtering for better texture quality at oblique angles
                                child.material.map.anisotropy = maxAnisotropy;
                                child.material.map.needsUpdate = true;
                                console.log('  Mesh has texture:', child.material.map.image?.src || 'loading...');
                            }
                        }
                    });
                    console.log('Applied', materialCount, 'materials with', textureCount, 'textures to', objConfig.name);

                    // Reset the materials on the loader for next object
                    objLoader.setMaterials(null);
                }

                // Calculate and log bounding box for debugging
                var bbox = new THREE.Box3().setFromObject(object);
                console.log('Object bounding box:', objConfig.name, '- min:', bbox.min, 'max:', bbox.max);

                // Apply transformations
                applyObjectTransform(object, objConfig);

                // Add to scene
                scene.add(object);

                // Store reference
                loadedObjects[index] = {
                    object: object,
                    config: objConfig
                };

                console.log('Added 3D object to scene:', objConfig.name);

                // Update UI list
                updateObjectsList();
            },
            (progress) => {
                if (progress.lengthComputable) {
                    console.log('Loading progress for', objConfig.name, ':', (progress.loaded / progress.total * 100).toFixed(1) + '%');
                }
            },
            (error) => {
                console.error('Error loading 3D object', objConfig.name, ':', error);
                // Fall back to simple mesh
                console.log('Falling back to placeholder mesh for', objConfig.name);
                loadFallbackMesh(objConfig, index);
            }
        );
    }

    function applyObjectTransform(object, config) {
        // Apply position
        // Objects from virtdisplay.json are in xLights coordinate system
        // Use the objectCenterOffset calculated from gridlines to align with pixel coordinates
        let x = parseFloat(config.WorldPosX || 0);
        let y = parseFloat(config.WorldPosY || 0);
        let z = parseFloat(config.WorldPosZ || 0);

        console.log('Original object position:', config.name, '- x:', x, 'y:', y, 'z:', z);
        console.log('Object center offset:', objectCenterOffset);

        // Subtract the offset to align with pixel coordinate system
        x = x - objectCenterOffset.x;
        y = y - objectCenterOffset.y;
        z = z - objectCenterOffset.z;

        console.log('Final object position:', config.name, '- x:', x, 'y:', y, 'z:', z);

        object.position.set(x, y, z);

        // Apply rotation (convert degrees to radians)
        const rotX = (parseFloat(config.RotateX || 0) * Math.PI) / 180;
        const rotY = (parseFloat(config.RotateY || 0) * Math.PI) / 180;
        const rotZ = (parseFloat(config.RotateZ || 0) * Math.PI) / 180;

        object.rotation.set(rotX, rotY, rotZ);

        // Apply scale - multiply by a large factor to match the pixel coordinate system
        // The pixel coordinates are in the range of hundreds/thousands, so we need to scale up significantly
        const scaleFactor = 100; // Objects are in model units (typically cm or inches), pixels are in virtual units
        const scaleX = (parseFloat(config.ScaleX || 100) / 100) * scaleFactor;
        const scaleY = (parseFloat(config.ScaleY || 100) / 100) * scaleFactor;
        const scaleZ = (parseFloat(config.ScaleZ || 100) / 100) * scaleFactor;

        object.scale.set(scaleX, scaleY, scaleZ);

        console.log('Applied transform to', config.name, '- Position:', { x, y, z }, 'Rotation:', { rotX, rotY, rotZ }, 'Scale:', { scaleX, scaleY, scaleZ });
    }

    function loadGridlines(config, index) {
        console.log('Creating gridlines:', config.name);

        const gridWidth = parseFloat(config.GridWidth || 1000);
        const gridHeight = parseFloat(config.GridHeight || 1000);
        const spacing = parseFloat(config.GridLineSpacing || 50);
        const color = new THREE.Color(config.GridColor || '#004a00');

        // Create grid - THREE.GridHelper is already horizontal (XZ plane)
        const grid = new THREE.GridHelper(Math.max(gridWidth, gridHeight), Math.max(gridWidth, gridHeight) / spacing, color, color);

        // Apply position and scale, but NOT rotation since GridHelper is already in the correct orientation
        // In xLights, gridlines are rotated -90 degrees to be horizontal
        // But THREE.GridHelper is already horizontal by default
        const x = parseFloat(config.WorldPosX || 0) - objectCenterOffset.x;
        const y = parseFloat(config.WorldPosY || 0) - objectCenterOffset.y;
        const z = parseFloat(config.WorldPosZ || 0) - objectCenterOffset.z;

        grid.position.set(x, y, z);

        // Apply scale if needed
        const scaleFactor = 100;
        const scaleX = (parseFloat(config.ScaleX || 100) / 100) * scaleFactor;
        const scaleY = (parseFloat(config.ScaleY || 100) / 100) * scaleFactor;
        const scaleZ = (parseFloat(config.ScaleZ || 100) / 100) * scaleFactor;
        grid.scale.set(scaleX, scaleY, scaleZ);

        console.log('Gridlines positioned at:', { x, y, z }, 'scale:', { scaleX, scaleY, scaleZ });

        scene.add(grid);

        loadedObjects[index] = {
            object: grid,
            config: config
        };

        console.log('Added gridlines to scene:', config.name);

        // Update UI list
        updateObjectsList();
    }

    function loadRuler(config, index) {
        console.log('Creating ruler:', config.name);

        // Create a simple line for the ruler
        const material = new THREE.LineBasicMaterial({ color: 0xff0000 });
        const points = [];

        const startX = parseFloat(config.WorldPosX || 0) - modelCenter.x;
        const startY = parseFloat(config.WorldPosY || 0) - modelCenter.y;
        const startZ = parseFloat(config.WorldPosZ || 0) - modelCenter.z;

        const endX = parseFloat(config.X2 || 0) - modelCenter.x;
        const endY = parseFloat(config.Y2 || 0) - modelCenter.y;
        const endZ = parseFloat(config.Z2 || 0) - modelCenter.z;

        points.push(new THREE.Vector3(startX, startY, startZ));
        points.push(new THREE.Vector3(endX, endY, endZ));

        const geometry = new THREE.BufferGeometry().setFromPoints(points);
        const line = new THREE.Line(geometry, material);

        scene.add(line);

        loadedObjects[index] = {
            object: line,
            config: config
        };

        console.log('Added ruler to scene:', config.name);

        // Update UI list
        updateObjectsList();
    }

    function loadFallbackMesh(config, index) {
        console.log('Creating fallback mesh for:', config.name);

        // Create a simple box as a placeholder for the 3D object
        var geometry = new THREE.BoxGeometry(50, 50, 50);
        var material = new THREE.MeshBasicMaterial({
            color: 0x888888,
            wireframe: true,
            opacity: 0.7,
            transparent: true
        });

        var mesh = new THREE.Mesh(geometry, material);

        // Apply transformations
        applyObjectTransform(mesh, config);

        scene.add(mesh);

        loadedObjects[index] = {
            object: mesh,
            config: config
        };

        console.log('Added fallback mesh to scene:', config.name);

        // Update UI list
        updateObjectsList();
    }

    function setupOrbitControls() {
        // Simple manual orbit controls
        var isDragging = false;
        var isPanning = false;
        var previousMousePosition = { x: 0, y: 0 };
        var rotationSpeed = 0.005;
        var panSpeed = 0.5;

        // Initialize lookAtTarget now that THREE.js is loaded
        if (!lookAtTarget) {
            lookAtTarget = new THREE.Vector3(0, 0, 0);
        }

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
                syncCameraControlSliders();
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
                syncCameraControlSliders();
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
            var relative = new THREE.Vector3().subVectors(camera.position, lookAtTarget);
            relative.multiplyScalar(delta);
            camera.position.copy(lookAtTarget).add(relative);
            camera.lookAt(lookAtTarget);
            syncCameraControlSliders();
        });
    }

    function computeCameraState() {
        if (!camera || !lookAtTarget) {
            return null;
        }

        var offset = new THREE.Vector3().subVectors(camera.position, lookAtTarget);
        var distance = offset.length();
        if (distance < 0.0001) {
            distance = 0.0001;
        }

        var yaw = Math.atan2(offset.x, offset.z) * (180 / Math.PI);
        if (yaw < 0) {
            yaw += 360;
        }

        var verticalRatio = offset.y / distance;
        verticalRatio = Math.min(1, Math.max(-1, verticalRatio));
        var pitch = Math.asin(verticalRatio) * (180 / Math.PI);

        return {
            yaw: yaw,
            pitch: pitch,
            distance: distance,
            target: {
                x: lookAtTarget.x,
                y: lookAtTarget.y,
                z: lookAtTarget.z
            }
        };
    }

    function updateCameraValueLabels(state) {
        if (!state) {
            return;
        }

        var yawLabel = document.getElementById('cameraYawValue');
        if (yawLabel) {
            yawLabel.textContent = state.yaw.toFixed(0) + '\u00B0';
        }

        var pitchLabel = document.getElementById('cameraPitchValue');
        if (pitchLabel) {
            pitchLabel.textContent = state.pitch.toFixed(0) + '\u00B0';
        }

        var zoomLabel = document.getElementById('cameraDistanceValue');
        if (zoomLabel) {
            zoomLabel.textContent = state.distance.toFixed(0) + ' units';
        }

        var panXLabel = document.getElementById('panXValue');
        if (panXLabel) {
            panXLabel.textContent = state.target.x.toFixed(0);
        }

        var panYLabel = document.getElementById('panYValue');
        if (panYLabel) {
            panYLabel.textContent = state.target.y.toFixed(0);
        }

        var panZLabel = document.getElementById('panZValue');
        if (panZLabel) {
            panZLabel.textContent = state.target.z.toFixed(0);
        }
    }

    function syncCameraControlSliders() {
        if (!camera || !lookAtTarget) {
            return;
        }

        var state = computeCameraState();
        if (!state) {
            return;
        }

        var yawSlider = document.getElementById('cameraYawSlider');
        if (yawSlider) {
            var normalizedYaw = ((state.yaw % 360) + 360) % 360;
            yawSlider.value = Math.round(normalizedYaw);
        }

        var pitchSlider = document.getElementById('cameraPitchSlider');
        if (pitchSlider) {
            pitchSlider.value = Math.round(state.pitch);
        }

        var distanceSlider = document.getElementById('cameraDistanceSlider');
        if (distanceSlider) {
            distanceSlider.value = Math.round(state.distance);
        }

        var panXSlider = document.getElementById('panXSlider');
        if (panXSlider) {
            panXSlider.value = Math.round(state.target.x);
        }

        var panYSlider = document.getElementById('panYSlider');
        if (panYSlider) {
            panYSlider.value = Math.round(state.target.y);
        }

        var panZSlider = document.getElementById('panZSlider');
        if (panZSlider) {
            panZSlider.value = Math.round(state.target.z);
        }

        updateCameraValueLabels(state);
    }

    function handleCameraControlInput() {
        if (!camera || !lookAtTarget) {
            return;
        }

        var yawSlider = document.getElementById('cameraYawSlider');
        var pitchSlider = document.getElementById('cameraPitchSlider');
        var distanceSlider = document.getElementById('cameraDistanceSlider');
        var panXSlider = document.getElementById('panXSlider');
        var panYSlider = document.getElementById('panYSlider');
        var panZSlider = document.getElementById('panZSlider');

        if (!yawSlider || !pitchSlider || !distanceSlider) {
            return;
        }

        var yaw = parseFloat(yawSlider.value) || 0;
        var pitch = parseFloat(pitchSlider.value) || 0;
        var distance = Math.max(cameraControlState.distanceMin, Math.min(cameraControlState.distanceMax, parseFloat(distanceSlider.value) || 0));

        var targetX = panXSlider ? parseFloat(panXSlider.value) || 0 : lookAtTarget.x;
        var targetY = panYSlider ? parseFloat(panYSlider.value) || 0 : lookAtTarget.y;
        var targetZ = panZSlider ? parseFloat(panZSlider.value) || 0 : lookAtTarget.z;

        lookAtTarget.set(targetX, targetY, targetZ);

        var yawRad = yaw * (Math.PI / 180);
        var pitchRad = pitch * (Math.PI / 180);
        var cosPitch = Math.cos(pitchRad);

        var offsetX = distance * cosPitch * Math.sin(yawRad);
        var offsetY = distance * Math.sin(pitchRad);
        var offsetZ = distance * cosPitch * Math.cos(yawRad);

        camera.position.set(
            lookAtTarget.x + offsetX,
            lookAtTarget.y + offsetY,
            lookAtTarget.z + offsetZ
        );
        camera.lookAt(lookAtTarget);

        syncCameraControlSliders();
    }

    function initializeCameraControlUI() {
        var distanceSlider = document.getElementById('cameraDistanceSlider');
        if (!distanceSlider || !camera) {
            return;
        }

        var maxDimension = Math.max(
            modelBounds.maxX - modelBounds.minX,
            modelBounds.maxY - modelBounds.minY,
            modelBounds.maxZ - modelBounds.minZ
        );

        cameraControlState.distanceMin = 100;
        cameraControlState.distanceMax = Math.max(1000, maxDimension * 4);

        distanceSlider.min = cameraControlState.distanceMin;
        distanceSlider.max = cameraControlState.distanceMax;

        cameraControlState.initialized = true;
        syncCameraControlSliders();
    }

    function togglePixelOffsetAdvanced() {
        var section = document.getElementById('pixelOffsetSection');
        var button = document.getElementById('pixelOffsetToggle');
        if (!section || !button) {
            return;
        }

        pixelOffsetAdvancedVisible = !pixelOffsetAdvancedVisible;
        section.style.display = pixelOffsetAdvancedVisible ? 'block' : 'none';
        button.value = pixelOffsetAdvancedVisible ? 'Hide Advanced Virtual Display Controls' : 'Show Advanced Virtual Display Controls';
    }

    // Holiday Animations
    var holidayAnimations = {
        enabled: false,
        snowParticles: null,
        groundSnow: null,
        santa: null,
        animationTime: 0,
        frameCounter: 0  // For optimizing particle updates
    };

    function initHolidayAnimations() {
        if (holidayAnimations.enabled) return;

        console.log('Initializing holiday animations...');

        // Create falling snow particles (reduced count for better performance)
        var snowCount = 500;
        var snowGeometry = new THREE.BufferGeometry();
        var snowPositions = new Float32Array(snowCount * 3);
        var snowVelocities = [];

        for (var i = 0; i < snowCount; i++) {
            snowPositions[i * 3] = (Math.random() - 0.5) * 2000;     // X
            snowPositions[i * 3 + 1] = Math.random() * 1000;         // Y
            snowPositions[i * 3 + 2] = (Math.random() - 0.5) * 2000; // Z
            snowVelocities.push(Math.random() * 0.5 + 0.5); // Fall speed
        }

        snowGeometry.setAttribute('position', new THREE.BufferAttribute(snowPositions, 3));

        var snowMaterial = new THREE.PointsMaterial({
            color: 0xffffff,
            size: 3,
            transparent: true,
            opacity: 0.8
        });

        holidayAnimations.snowParticles = new THREE.Points(snowGeometry, snowMaterial);
        holidayAnimations.snowParticles.velocities = snowVelocities;
        scene.add(holidayAnimations.snowParticles);

        // Create snow on ground
        var groundGeometry = new THREE.PlaneGeometry(2000, 2000);
        var groundMaterial = new THREE.MeshPhongMaterial({
            color: 0xffffff,
            transparent: true,
            opacity: 0.3,
            side: THREE.DoubleSide
        });
        holidayAnimations.groundSnow = new THREE.Mesh(groundGeometry, groundMaterial);
        holidayAnimations.groundSnow.rotation.x = -Math.PI / 2;
        holidayAnimations.groundSnow.position.y = -50;
        scene.add(holidayAnimations.groundSnow);

        // Create Santa and Sleigh
        var santaGroup = new THREE.Group();

        // Santa's body - more detailed
        var bodyGeometry = new THREE.SphereGeometry(25, 32, 32);
        bodyGeometry.scale(1, 1.2, 0.8); // Make it more oval
        var bodyMaterial = new THREE.MeshPhongMaterial({
            color: 0xcc0000,
            shininess: 10
        });
        var body = new THREE.Mesh(bodyGeometry, bodyMaterial);
        body.position.y = 10;
        santaGroup.add(body);

        // Santa's head
        var headGeometry = new THREE.SphereGeometry(15, 32, 32);
        var headMaterial = new THREE.MeshPhongMaterial({
            color: 0xffdbac,
            shininess: 30
        });
        var head = new THREE.Mesh(headGeometry, headMaterial);
        head.position.y = 35;
        santaGroup.add(head);

        // White beard
        var beardGeometry = new THREE.SphereGeometry(12, 16, 16);
        beardGeometry.scale(1, 0.8, 0.8);
        var beardMaterial = new THREE.MeshPhongMaterial({ color: 0xffffff });
        var beard = new THREE.Mesh(beardGeometry, beardMaterial);
        beard.position.set(0, 30, 8);
        santaGroup.add(beard);

        // Santa's hat
        var hatGeometry = new THREE.ConeGeometry(16, 35, 32);
        var hatMaterial = new THREE.MeshPhongMaterial({ color: 0xcc0000 });
        var hat = new THREE.Mesh(hatGeometry, hatMaterial);
        hat.position.y = 55;
        santaGroup.add(hat);

        // Hat pom-pom
        var pomGeometry = new THREE.SphereGeometry(8, 16, 16);
        var pomMaterial = new THREE.MeshPhongMaterial({ color: 0xffffff });
        var pom = new THREE.Mesh(pomGeometry, pomMaterial);
        pom.position.y = 72;
        santaGroup.add(pom);

        // White belt
        var beltGeometry = new THREE.TorusGeometry(25, 3, 8, 32);
        var beltMaterial = new THREE.MeshPhongMaterial({ color: 0xffffff });
        var belt = new THREE.Mesh(beltGeometry, beltMaterial);
        belt.rotation.x = Math.PI / 2;
        belt.position.y = 10;
        santaGroup.add(belt);

        // Gold buckle
        var buckleGeometry = new THREE.BoxGeometry(8, 8, 4);
        var buckleMaterial = new THREE.MeshPhongMaterial({
            color: 0xffd700,
            shininess: 100
        });
        var buckle = new THREE.Mesh(buckleGeometry, buckleMaterial);
        buckle.position.set(0, 10, 26);
        santaGroup.add(buckle);

        // Sleigh
        var sleighGroup = new THREE.Group();

        // Sleigh body - curved
        var sleighBodyGeometry = new THREE.BoxGeometry(60, 20, 40);
        var sleighBodyMaterial = new THREE.MeshPhongMaterial({
            color: 0x8B4513,
            shininess: 50
        });
        var sleighBody = new THREE.Mesh(sleighBodyGeometry, sleighBodyMaterial);
        sleighBody.position.y = -15;
        sleighGroup.add(sleighBody);

        // Sleigh front curved part
        var sleighFrontGeometry = new THREE.CylinderGeometry(20, 20, 40, 32, 1, false, 0, Math.PI);
        var sleighFrontMaterial = new THREE.MeshPhongMaterial({
            color: 0x8B4513,
            side: THREE.DoubleSide
        });
        var sleighFront = new THREE.Mesh(sleighFrontGeometry, sleighFrontMaterial);
        sleighFront.rotation.z = Math.PI / 2;
        sleighFront.rotation.y = Math.PI / 2;
        sleighFront.position.set(40, -5, 0);
        sleighGroup.add(sleighFront);

        // Sleigh runners (skis)
        var runnerGeometry = new THREE.BoxGeometry(80, 3, 8);
        var runnerMaterial = new THREE.MeshPhongMaterial({
            color: 0xcccccc,
            metalness: 0.8,
            shininess: 100
        });
        var runner1 = new THREE.Mesh(runnerGeometry, runnerMaterial);
        runner1.position.set(0, -28, -15);
        sleighGroup.add(runner1);

        var runner2 = new THREE.Mesh(runnerGeometry, runnerMaterial);
        runner2.position.set(0, -28, 15);
        sleighGroup.add(runner2);

        // Gift bags in sleigh
        var giftColors = [0xff0000, 0x00ff00, 0x0000ff, 0xffff00];
        for (var i = 0; i < 4; i++) {
            var giftGeo = new THREE.BoxGeometry(12, 12, 12);
            var giftMat = new THREE.MeshPhongMaterial({
                color: giftColors[i],
                shininess: 30
            });
            var gift = new THREE.Mesh(giftGeo, giftMat);
            gift.position.set(-10 + (i * 8), -8, -5 + Math.random() * 10);
            gift.rotation.y = Math.random() * 0.5;
            sleighGroup.add(gift);
        }

        // Reindeer (lead reindeer)
        var reindeerGroup = new THREE.Group();

        // Reindeer body
        var reindeerBodyGeo = new THREE.CylinderGeometry(8, 10, 30, 16);
        var reindeerBodyMat = new THREE.MeshPhongMaterial({ color: 0x8B4513 });
        var reindeerBody = new THREE.Mesh(reindeerBodyGeo, reindeerBodyMat);
        reindeerBody.rotation.z = Math.PI / 2;
        reindeerGroup.add(reindeerBody);

        // Reindeer head
        var reindeerHeadGeo = new THREE.ConeGeometry(6, 15, 16);
        var reindeerHead = new THREE.Mesh(reindeerHeadGeo, reindeerBodyMat);
        reindeerHead.rotation.z = Math.PI / 2;
        reindeerHead.position.set(20, 0, 0);
        reindeerGroup.add(reindeerHead);

        // Red nose (Rudolph!)
        var noseGeo = new THREE.SphereGeometry(4, 16, 16);
        var noseMat = new THREE.MeshPhongMaterial({
            color: 0xff0000,
            emissive: 0xff0000,
            emissiveIntensity: 0.5
        });
        var nose = new THREE.Mesh(noseGeo, noseMat);
        nose.position.set(27, 0, 0);
        reindeerGroup.add(nose);

        // Antlers
        var antlerGeo = new THREE.CylinderGeometry(0.5, 1, 15, 8);
        var antlerMat = new THREE.MeshPhongMaterial({ color: 0xD2691E });

        var antler1 = new THREE.Mesh(antlerGeo, antlerMat);
        antler1.position.set(15, 10, -5);
        antler1.rotation.z = -0.3;
        reindeerGroup.add(antler1);

        var antler2 = new THREE.Mesh(antlerGeo, antlerMat);
        antler2.position.set(15, 10, 5);
        antler2.rotation.z = -0.3;
        reindeerGroup.add(antler2);

        // Position reindeer in front of sleigh
        reindeerGroup.position.set(80, -10, 0);

        // Add Santa to sleigh
        santaGroup.position.set(-10, 15, 0);
        sleighGroup.add(santaGroup);
        sleighGroup.add(reindeerGroup);

        holidayAnimations.santa = sleighGroup;
        holidayAnimations.santa.position.set(0, 400, 0);
        scene.add(holidayAnimations.santa);

        holidayAnimations.enabled = true;
        console.log('Holiday animations initialized!');
    }

    function removeHolidayAnimations() {
        if (!holidayAnimations.enabled) return;

        if (holidayAnimations.snowParticles) {
            scene.remove(holidayAnimations.snowParticles);
            holidayAnimations.snowParticles = null;
        }
        if (holidayAnimations.groundSnow) {
            scene.remove(holidayAnimations.groundSnow);
            holidayAnimations.groundSnow = null;
        }
        if (holidayAnimations.santa) {
            scene.remove(holidayAnimations.santa);
            holidayAnimations.santa = null;
        }

        holidayAnimations.enabled = false;
        holidayAnimations.animationTime = 0;
        console.log('Holiday animations removed');
    }

    function updateHolidayAnimations() {
        if (!holidayAnimations.enabled) return;

        holidayAnimations.animationTime += 0.016; // Assume ~60fps
        holidayAnimations.frameCounter++;

        // Update falling snow (every other frame for better performance)
        if (holidayAnimations.snowParticles && holidayAnimations.frameCounter % 2 === 0) {
            var positions = holidayAnimations.snowParticles.geometry.attributes.position.array;
            var velocities = holidayAnimations.snowParticles.velocities;

            for (var i = 0; i < positions.length / 3; i++) {
                positions[i * 3 + 1] -= velocities[i]; // Fall down

                // Reset to top when it falls below ground
                if (positions[i * 3 + 1] < -50) {
                    positions[i * 3 + 1] = 1000;
                    positions[i * 3] = (Math.random() - 0.5) * 2000;
                    positions[i * 3 + 2] = (Math.random() - 0.5) * 2000;
                }
            }

            holidayAnimations.snowParticles.geometry.attributes.position.needsUpdate = true;
        }

        // Animate Santa and Sleigh flying in a circle
        if (holidayAnimations.santa) {
            var santaRadius = 700;
            var santaSpeed = 0.25;
            var angle = holidayAnimations.animationTime * santaSpeed;

            holidayAnimations.santa.position.x = Math.cos(angle) * santaRadius;
            holidayAnimations.santa.position.z = Math.sin(angle) * santaRadius;
            holidayAnimations.santa.position.y = 400 + Math.sin(angle * 2) * 40;

            // Point Santa in the direction of travel
            holidayAnimations.santa.rotation.y = angle + Math.PI / 2;

            // Add gentle bobbing motion
            holidayAnimations.santa.rotation.x = Math.sin(angle * 4) * 0.05;
            holidayAnimations.santa.rotation.z = Math.sin(angle * 3) * 0.03;
        }
    }

    function toggleHolidayAnimations() {
        if (holidayAnimations.enabled) {
            removeHolidayAnimations();
        } else {
            initHolidayAnimations();
        }
    }

    function getCurrentViewURL() {
        // Get base URL without query parameters
        var baseUrl = window.location.origin + window.location.pathname;

        // Build query parameters from current state
        var params = [];

        // Standalone mode (preserve if present)
        if (window.standaloneMode) {
            params.push('standalone=true');
        }

        // Camera position
        if (camera) {
            params.push('cameraX=' + camera.position.x.toFixed(2));
            params.push('cameraY=' + camera.position.y.toFixed(2));
            params.push('cameraZ=' + camera.position.z.toFixed(2));
        }

        // Camera target (what it's looking at)
        if (lookAtTarget) {
            params.push('targetX=' + lookAtTarget.x.toFixed(2));
            params.push('targetY=' + lookAtTarget.y.toFixed(2));
            params.push('targetZ=' + lookAtTarget.z.toFixed(2));
        }

        // FOV (if not default)
        if (camera && camera.fov && camera.fov !== 75) {
            params.push('fov=' + camera.fov.toFixed(0));
        }

        // Brightness
        if (window.brightnessMultiplier !== 2.0) {
            params.push('brightness=' + window.brightnessMultiplier.toFixed(1));
        }

        // Ambient light
        var ambientLightSlider = document.getElementById('ambientLightSlider');
        if (ambientLightSlider) {
            var ambientValue = parseFloat(ambientLightSlider.value);
            if (ambientValue !== 1.0) {
                params.push('ambientLight=' + ambientValue.toFixed(1));
            }
        }

        // Pixel size
        if (window.pixelMaterial && window.pixelMaterial.size !== 3) {
            params.push('pixelSize=' + window.pixelMaterial.size.toFixed(1));
        }

        // Grid visibility
        if (window.gridHelper && !window.gridHelper.visible) {
            params.push('showGrid=false');
        }

        // Axes visibility
        if (window.axesHelper && window.axesHelper.visible) {
            params.push('showAxes=true');
        }

        // Holiday animations
        if (holidayAnimations.enabled) {
            params.push('holidayMode=true');
        }

        // Fullscreen state (note: can't be auto-applied due to browser restrictions)
        var isFullscreen = !!(document.fullscreenElement || document.webkitFullscreenElement || document.mozFullScreenElement);
        if (isFullscreen) {
            params.push('fullscreen=true');
        }

        // Combine into full URL
        var fullUrl = baseUrl;
        if (params.length > 0) {
            fullUrl += '?' + params.join('&');
        }

        return fullUrl;
    }

    function copyCurrentViewURL() {
        var url = getCurrentViewURL();

        // Copy to clipboard
        if (navigator.clipboard && navigator.clipboard.writeText) {
            navigator.clipboard.writeText(url).then(function () {
                showCopyFeedback('URL copied to clipboard!');
                console.log('Copied URL:', url);
            }).catch(function (err) {
                // Fallback for clipboard API failure
                fallbackCopyToClipboard(url);
            });
        } else {
            // Fallback for older browsers
            fallbackCopyToClipboard(url);
        }
    }

    function fallbackCopyToClipboard(text) {
        var textArea = document.createElement('textarea');
        textArea.value = text;
        textArea.style.position = 'fixed';
        textArea.style.left = '-999999px';
        document.body.appendChild(textArea);
        textArea.focus();
        textArea.select();

        try {
            document.execCommand('copy');
            showCopyFeedback('URL copied to clipboard!');
            console.log('Copied URL:', text);
        } catch (err) {
            showCopyFeedback('Failed to copy. URL logged to console.');
            console.log('Copy this URL:', text);
        }

        document.body.removeChild(textArea);
    }

    function showCopyFeedback(message) {
        // Create or update feedback element
        var feedback = document.getElementById('copyFeedback');
        if (!feedback) {
            feedback = document.createElement('div');
            feedback.id = 'copyFeedback';
            feedback.style.position = 'fixed';
            feedback.style.top = '20px';
            feedback.style.right = '20px';
            feedback.style.backgroundColor = '#4CAF50';
            feedback.style.color = 'white';
            feedback.style.padding = '12px 24px';
            feedback.style.borderRadius = '4px';
            feedback.style.boxShadow = '0 2px 8px rgba(0,0,0,0.2)';
            feedback.style.zIndex = '10000';
            feedback.style.fontSize = '14px';
            feedback.style.fontWeight = 'bold';
            feedback.style.transition = 'opacity 0.3s';
            document.body.appendChild(feedback);
        }

        feedback.textContent = message;
        feedback.style.opacity = '1';
        feedback.style.display = 'block';

        // Auto-hide after 3 seconds
        setTimeout(function () {
            feedback.style.opacity = '0';
            setTimeout(function () {
                feedback.style.display = 'none';
            }, 300);
        }, 3000);
    }

    function toggleFullscreen() {
        var container = document.getElementById('canvas-container');

        if (!document.fullscreenElement &&
            !document.webkitFullscreenElement &&
            !document.mozFullScreenElement) {
            // Enter fullscreen
            if (container.requestFullscreen) {
                container.requestFullscreen();
            } else if (container.webkitRequestFullscreen) {
                container.webkitRequestFullscreen();
            } else if (container.mozRequestFullScreen) {
                container.mozRequestFullScreen();
            } else if (container.msRequestFullscreen) {
                container.msRequestFullscreen();
            }

            console.log('Entering fullscreen mode');
        } else {
            // Exit fullscreen
            exitFullscreen();
        }
    }

    function exitFullscreen() {
        if (document.exitFullscreen) {
            document.exitFullscreen();
        } else if (document.webkitExitFullscreen) {
            document.webkitExitFullscreen();
        } else if (document.mozCancelFullScreen) {
            document.mozCancelFullScreen();
        } else if (document.msExitFullscreen) {
            document.msExitFullscreen();
        }

        console.log('Exiting fullscreen mode');
    }

    // Handle fullscreen change events
    function handleFullscreenChange() {
        if (!document.fullscreenElement &&
            !document.webkitFullscreenElement &&
            !document.mozFullScreenElement) {
            // Exited fullscreen - resize renderer
            var container = document.getElementById('canvas-container');
            renderer.setSize(canvasWidth, canvasHeight);
            camera.aspect = canvasWidth / canvasHeight;
            camera.updateProjectionMatrix();
            console.log('Exited fullscreen, resized to:', canvasWidth, 'x', canvasHeight);
        } else {
            // Entered fullscreen - resize to screen
            renderer.setSize(window.innerWidth, window.innerHeight);
            camera.aspect = window.innerWidth / window.innerHeight;
            camera.updateProjectionMatrix();
            console.log('Entered fullscreen, resized to:', window.innerWidth, 'x', window.innerHeight);
        }
    }

    // Listen for fullscreen change events
    document.addEventListener('fullscreenchange', handleFullscreenChange);
    document.addEventListener('webkitfullscreenchange', handleFullscreenChange);
    document.addEventListener('mozfullscreenchange', handleFullscreenChange);
    document.addEventListener('MSFullscreenChange', handleFullscreenChange);

    // ESC key handler
    document.addEventListener('keydown', function (event) {
        if (event.key === 'Escape' || event.keyCode === 27) {
            if (document.fullscreenElement ||
                document.webkitFullscreenElement ||
                document.mozFullScreenElement) {
                exitFullscreen();
            }
        }
    });

    function animate3D() {
        requestAnimationFrame(animate3D);

        // Update holiday animations if enabled
        updateHolidayAnimations();

        // Process pending color updates
        if (pendingUpdates.length > 0) {
            var pixels = pendingUpdates.shift();
            updatePixelColors(pixels);
        }

        renderer.render(scene, camera);
    }

    function updatePixelColors(pixels) {
        // First, set ALL pixels to black (SSE only sends non-black pixels)
        var color = new THREE.Color(0, 0, 0);
        if (window.pixelMesh) {
            if (!window.clearDebugDone) {
                console.log('Clearing all', pixelData.length, 'pixels to black');
                window.clearDebugDone = true;
            }
            for (var i = 0; i < pixelData.length; i++) {
                window.pixelColors[i * 3] = 0;
                window.pixelColors[i * 3 + 1] = 0;
                window.pixelColors[i * 3 + 2] = 0;
                window.pixelMesh.setColorAt(i, color);
            }
            // Mark that we need to upload the black colors to GPU
            window.pixelMesh.instanceColor.needsUpdate = true;
        }

        var updated = false;
        var colorCount = 0;
        var debugCount = 0;

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

            // Debug first few color values
            if (debugCount < 3 && !window.colorDebugDone) {
                console.log('RGB string:', rgb, '-> R:', rHex, 'G:', gHex, 'B:', bHex, '-> Final:',
                    (parseInt(rHex, 16) / 255.0).toFixed(3),
                    (parseInt(gHex, 16) / 255.0).toFixed(3),
                    (parseInt(bHex, 16) / 255.0).toFixed(3));
                debugCount++;
                if (debugCount >= 3) window.colorDebugDone = true;
            }

            // Parse hex strings and normalize to 0.0-1.0, then apply brightness
            var r = Math.min(1.0, (parseInt(rHex, 16) / 255.0) * window.brightnessMultiplier);
            var g = Math.min(1.0, (parseInt(gHex, 16) / 255.0) * window.brightnessMultiplier);
            var b = Math.min(1.0, (parseInt(bHex, 16) / 255.0) * window.brightnessMultiplier);

            var locs = data[1].split(';');
            for (var j = 0; j < locs.length; j++) {
                // 3D mode: location is pixel index, not 2D coordinate
                var pixelIndex = parseInt(locs[j], 10);
                if (pixelIndex !== undefined && pixelIndex < pixelData.length) {
                    // Update color in buffer and InstancedMesh
                    window.pixelColors[pixelIndex * 3] = r;
                    window.pixelColors[pixelIndex * 3 + 1] = g;
                    window.pixelColors[pixelIndex * 3 + 2] = b;

                    // Update instanced mesh color
                    if (window.pixelMesh) {
                        var pixelColor = new THREE.Color();
                        pixelColor.setRGB(r, g, b);
                        window.pixelMesh.setColorAt(pixelIndex, pixelColor);
                    }
                    updated = true;
                }
            }
        }

        // Mark colors as needing update for InstancedMesh
        if (updated && window.pixelMesh) {
            window.pixelMesh.instanceColor.needsUpdate = true;
        }
    }

    function processEvent(e) {
        if (testMode.enabled) {
            return;
        }
        var pixels = e.data.split('|');
        clearTimeout(clearTimer);

        // Log first time we receive data
        if (!window.hasReceivedPixelData) {
            console.log('First pixel data received:', pixels.length, 'pixel updates');
            window.hasReceivedPixelData = true;
        }

        // Replace pending update with latest
        pendingUpdates = [pixels];

        // Clear the display after 6 seconds if no more events
        clearTimer = setTimeout(function () {
            var color = new THREE.Color(0, 0, 0);
            // Set all pixels to black
            for (var i = 0; i < pixelData.length; i++) {
                window.pixelColors[i * 3] = 0;
                window.pixelColors[i * 3 + 1] = 0;
                window.pixelColors[i * 3 + 2] = 0;

                // Update instanced mesh
                if (window.pixelMesh) {
                    window.pixelMesh.setColorAt(i, color);
                }
            }
            if (window.pixelMesh) {
                window.pixelMesh.instanceColor.needsUpdate = true;
            }
        }, 6000);
    }

    function startSSE() {
        console.log('Starting SSE connection to api/http-virtual-display-3d/');
        evtSource = new EventSource('api/http-virtual-display-3d/');

        evtSource.onmessage = function (event) {
            processEvent(event);
        };

        evtSource.onerror = function (error) {
            console.error('SSE connection error:', error);
        };

        evtSource.onopen = function () {
            console.log('SSE connection opened successfully');
        };
    }

    function stopSSE() {
        if (typeof $ !== 'undefined') {
            $('#stopButton').hide();
        }
        if (evtSource) {
            evtSource.close();
        }
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
        if (window.pixelMesh) {
            var matrix = new THREE.Matrix4();
            var scale = size / 3.0; // Base size is 3 (diameter 3 units)

            // Update scale for all instances
            for (var i = 0; i < pixelData.length; i++) {
                var pos = window.pixelPositions[i];
                matrix.compose(
                    new THREE.Vector3(pos.x, pos.y, pos.z),
                    new THREE.Quaternion(),
                    new THREE.Vector3(scale, scale, scale)
                );
                window.pixelMesh.setMatrixAt(i, matrix);
            }
            window.pixelMesh.instanceMatrix.needsUpdate = true;
        }
        document.getElementById('pixelSizeValue').textContent = size.toFixed(1);
    }

    function updateBrightness() {
        var brightness = parseFloat(document.getElementById('brightnessSlider').value);
        window.brightnessMultiplier = brightness;
        document.getElementById('brightnessValue').textContent = brightness.toFixed(2) + 'x';
        if (testMode.enabled) {
            applyTestColorToPixels();
        }
    }

    function updateAmbientLight() {
        var multiplier = parseFloat(document.getElementById('ambientLightSlider').value);
        document.getElementById('ambientLightValue').textContent = multiplier.toFixed(2) + 'x';

        // Apply multiplier to all scene lights proportionally
        if (window.sceneLights) {
            window.sceneLights.ambient.light.intensity = window.sceneLights.ambient.baseIntensity * multiplier;
            window.sceneLights.directional1.light.intensity = window.sceneLights.directional1.baseIntensity * multiplier;
            window.sceneLights.directional2.light.intensity = window.sceneLights.directional2.baseIntensity * multiplier;
            window.sceneLights.hemisphere.light.intensity = window.sceneLights.hemisphere.baseIntensity * multiplier;
        }
    }

    function hexToRgbFloats(hexColor) {
        if (!hexColor) {
            return [1.0, 0.0, 0.0];
        }
        var hex = hexColor.replace('#', '');
        if (hex.length === 3) {
            hex = hex.split('').map(function (c) { return c + c; }).join('');
        }
        var r = parseInt(hex.substring(0, 2), 16) / 255.0;
        var g = parseInt(hex.substring(2, 4), 16) / 255.0;
        var b = parseInt(hex.substring(4, 6), 16) / 255.0;
        return [r, g, b];
    }

    function applyTestColorToPixels() {
        if (!window.pixelColors || !window.pixelMesh) {
            return;
        }
        var brightness = window.brightnessMultiplier || 1.0;
        var r = Math.min(1.0, testMode.rgb[0] * brightness);
        var g = Math.min(1.0, testMode.rgb[1] * brightness);
        var b = Math.min(1.0, testMode.rgb[2] * brightness);

        var color = new THREE.Color(r, g, b);

        for (var i = 0; i < pixelData.length; i++) {
            window.pixelColors[i * 3] = r;
            window.pixelColors[i * 3 + 1] = g;
            window.pixelColors[i * 3 + 2] = b;
            window.pixelMesh.setColorAt(i, color);
        }

        window.pixelMesh.instanceColor.needsUpdate = true;
    }

    function setTestModeEnabled(enabled) {
        testMode.enabled = enabled;
        var status = document.getElementById('testModeStatus');
        if (enabled) {
            pendingUpdates = [];
            clearTimeout(clearTimer);
            applyTestColorToPixels();
            if (status) {
                status.textContent = 'Static color active';
            }
        } else {
            if (window.pixelColors && window.pixelMesh) {
                var color = new THREE.Color(0, 0, 0);
                for (var i = 0; i < pixelData.length; i++) {
                    window.pixelColors[i * 3] = 0;
                    window.pixelColors[i * 3 + 1] = 0;
                    window.pixelColors[i * 3 + 2] = 0;
                    window.pixelMesh.setColorAt(i, color);
                }
                window.pixelMesh.instanceColor.needsUpdate = true;
            }
            if (status) {
                status.textContent = 'Live mode';
            }
        }
    }

    function handleTestModeToggle() {
        var checkbox = document.getElementById('testModeToggle');
        if (!checkbox) {
            return;
        }
        setTestModeEnabled(checkbox.checked);
    }

    function handleTestColorChange(value) {
        if (!value) {
            return;
        }
        testMode.color = value;
        testMode.rgb = hexToRgbFloats(value);
        var label = document.getElementById('testModeColorValue');
        if (label) {
            label.textContent = value.toUpperCase();
        }
        if (testMode.enabled) {
            applyTestColorToPixels();
        }
    }

    function updatePixelOffset() {
        var xOffset = parseFloat(document.getElementById('pixelXSlider').value);
        var yOffset = parseFloat(document.getElementById('pixelYSlider').value);
        var zOffset = parseFloat(document.getElementById('pixelZSlider').value);

        // Update display values
        document.getElementById('pixelXValue').textContent = xOffset.toFixed(0);
        document.getElementById('pixelYValue').textContent = yOffset.toFixed(0);
        document.getElementById('pixelZValue').textContent = zOffset.toFixed(0);

        // Update pixel positions in the InstancedMesh
        if (window.pixelMesh && pixelData && window.pixelPositions) {
            var matrix = new THREE.Matrix4();
            var size = parseFloat(document.getElementById('pixelSizeSlider').value);
            var scale = size / 3.0; // Base size is 3 (diameter 3 units)

            for (var i = 0; i < pixelData.length; i++) {
                // Pixels may have Display2DCenter0 offset applied
                var pixelX = pixelData[i].x;
                var pixelY = pixelData[i].y;
                var pixelZ = pixelData[i].z;

                if (view2DSettings.centerAt0) {
                    // Add back the 2D preview center to get true xLights X coordinate
                    pixelX = pixelX + (view2DSettings.previewWidth / 2);
                }

                // Use raw coordinates with manual offset for alignment
                var finalX = pixelX + xOffset;
                var finalY = pixelY + yOffset;
                var finalZ = pixelZ + zOffset;

                // Update stored positions for reference
                window.pixelPositions[i] = { x: finalX, y: finalY, z: finalZ };

                // Update instance matrix with new position and current scale
                matrix.compose(
                    new THREE.Vector3(finalX, finalY, finalZ),
                    new THREE.Quaternion(),
                    new THREE.Vector3(scale, scale, scale)
                );
                window.pixelMesh.setMatrixAt(i, matrix);
            }

            window.pixelMesh.instanceMatrix.needsUpdate = true;
            console.log('Pixel offset adjusted to: X=' + xOffset + ', Y=' + yOffset + ', Z=' + zOffset);
        }
    }

    function savePixelOffsets() {
        var xOffset = parseFloat(document.getElementById('pixelXSlider').value) || 0;
        var yOffset = parseFloat(document.getElementById('pixelYSlider').value) || 0;
        var zOffset = parseFloat(document.getElementById('pixelZSlider').value) || 0;

        var offsets = {
            x: xOffset,
            y: yOffset,
            z: zOffset
        };

        // Save to browser's localStorage
        try {
            localStorage.setItem('fpp_3d_pixel_offsets', JSON.stringify(offsets));
            console.log('Pixel offsets saved:', offsets);
            alert('Pixel offsets saved successfully!\nX: ' + xOffset + '\nY: ' + yOffset + '\nZ: ' + zOffset);
        } catch (e) {
            console.error('Error saving pixel offsets:', e);
            alert('Error saving pixel offsets: ' + e.message);
        }
    }

    function resetPixelOffsets() {
        // Reset sliders to 0
        document.getElementById('pixelXSlider').value = 0;
        document.getElementById('pixelYSlider').value = 0;
        document.getElementById('pixelZSlider').value = 0;

        // Update display labels using correct IDs
        document.getElementById('pixelXValue').textContent = '0';
        document.getElementById('pixelYValue').textContent = '0';
        document.getElementById('pixelZValue').textContent = '0';

        // Apply the reset
        updatePixelOffset();

        console.log('Pixel offsets reset to 0');
    }

    function loadPixelOffsets() {
        try {
            var saved = localStorage.getItem('fpp_3d_pixel_offsets');
            if (saved) {
                var offsets = JSON.parse(saved);

                // Apply saved values to sliders using correct IDs
                if (offsets.x !== undefined) {
                    document.getElementById('pixelXSlider').value = offsets.x;
                    document.getElementById('pixelXValue').textContent = offsets.x.toString();
                }
                if (offsets.y !== undefined) {
                    document.getElementById('pixelYSlider').value = offsets.y;
                    document.getElementById('pixelYValue').textContent = offsets.y.toString();
                }
                if (offsets.z !== undefined) {
                    document.getElementById('pixelZSlider').value = offsets.z;
                    document.getElementById('pixelZValue').textContent = offsets.z.toString();
                }

                console.log('Pixel offsets loaded from saved settings:', offsets);
                return true;
            }
        } catch (e) {
            console.warn('Error loading saved pixel offsets:', e);
        }
        return false;
    }

    function toggle3DObjects() {
        var allVisible = true;

        // Check if all objects are currently visible
        for (var i = 0; i < loadedObjects.length; i++) {
            if (loadedObjects[i] && loadedObjects[i].object && !loadedObjects[i].object.visible) {
                allVisible = false;
                break;
            }
        }

        // Toggle all objects to opposite state
        for (var i = 0; i < loadedObjects.length; i++) {
            if (loadedObjects[i] && loadedObjects[i].object) {
                loadedObjects[i].object.visible = !allVisible;
            }
        }

        console.log('3D objects visibility set to:', !allVisible);
    }

    function toggleObject(index) {
        if (loadedObjects[index] && loadedObjects[index].object) {
            loadedObjects[index].object.visible = !loadedObjects[index].object.visible;
            console.log('Toggled', loadedObjects[index].config.name, 'visibility to:', loadedObjects[index].object.visible);
            // Update the objects list to refresh the checkmark
            updateObjectsList();
        }
    }

    function get3DObjectsList() {
        var list = '<div style="margin-top: 10px;"><strong>3D Objects from xLights:</strong><ul style="margin: 5px 0; padding-left: 20px;">';

        for (var i = 0; i < loadedObjects.length; i++) {
            if (loadedObjects[i] && loadedObjects[i].config) {
                var config = loadedObjects[i].config;
                var isVisible = loadedObjects[i].object ? loadedObjects[i].object.visible : false;
                var status = isVisible ? '✓' : '✗';
                list += '<li style="margin: 2px 0;"><button onclick="toggleObject(' + i + ')" style="margin-right: 5px; padding: 2px 6px;">' + status + '</button>' + config.name + ' (' + config.DisplayAs + ')</li>';
            }
        }

        list += '</ul></div>';
        return list;
    }

    function updateObjectsList() {
        var container = document.getElementById('objectsList');
        if (container) {
            container.innerHTML = get3DObjectsList();
        }
    }

    function setupClient() {
        // Verify THREE.js is available
        if (typeof window.THREE === 'undefined') {
            console.error('THREE.js not loaded! Cannot initialize 3D view.');
            return;
        }

        console.log('Setting up 3D Virtual Display client...');

        // Apply URL parameters for brightness
        var brightness = getURLParam('brightness', window.brightnessMultiplier);
        window.brightnessMultiplier = brightness;

        // Apply URL parameter for pixel size
        var urlPixelSize = getURLParam('pixelSize', null);
        if (urlPixelSize !== null) {
            pixelSize = urlPixelSize;
        }

        // Apply URL parameter for ambient light
        var urlAmbientLight = getURLParam('ambientLight', null);
        if (urlAmbientLight !== null) {
            window.urlAmbientLightValue = urlAmbientLight;
        }

        try {
            init3D();
            initializeCameraControlUI();
            startSSE();
            console.log('3D view initialized, SSE started');
        } catch (error) {
            console.error('Error initializing 3D view:', error);
        }

        // Apply URL parameters after DOM is ready
        setTimeout(function () {
            applyURLParameters();
        }, 100);
    }

    function applyURLParameters() {
        // Apply brightness slider if it exists
        var brightnessSlider = document.getElementById('brightnessSlider');
        if (brightnessSlider) {
            brightnessSlider.value = window.brightnessMultiplier;
        }

        // Apply pixel size slider if it exists
        var pixelSizeSlider = document.getElementById('pixelSizeSlider');
        if (pixelSizeSlider) {
            pixelSizeSlider.value = pixelSize;
            if (window.pixelMaterial) {
                window.pixelMaterial.size = pixelSize;
            }
        }

        // Apply ambient light slider if it exists and was set via URL
        if (window.urlAmbientLightValue !== undefined) {
            var ambientLightSlider = document.getElementById('ambientLightSlider');
            if (ambientLightSlider) {
                ambientLightSlider.value = window.urlAmbientLightValue;
                updateAmbientLight();
            }
        }

        var testColorInput = document.getElementById('testModeColor');
        if (testColorInput) {
            handleTestColorChange(testColorInput.value);
        }

        // Auto-enter fullscreen if requested
        // Note: Most browsers block automatic fullscreen without user interaction
        // We'll show a message and add a one-time click listener
        var fullscreen = getURLParam('fullscreen', false);
        if (fullscreen === true) {
            console.log('Fullscreen requested via URL parameter');

            // Try to enter fullscreen immediately (may be blocked by browser)
            toggleFullscreen();

            // Also add a fallback: show a message and enable click-anywhere to fullscreen
            if (!document.fullscreenElement && !document.webkitFullscreenElement && !document.mozFullScreenElement) {
                // If fullscreen didn't work, add a one-time click handler
                var clickToFullscreen = function () {
                    toggleFullscreen();
                    document.removeEventListener('click', clickToFullscreen);
                    document.removeEventListener('touchstart', clickToFullscreen);
                };

                document.addEventListener('click', clickToFullscreen, { once: true });
                document.addEventListener('touchstart', clickToFullscreen, { once: true });

                console.log('Fullscreen blocked by browser - click anywhere to enter fullscreen mode');
            }
        }

        // Initialize holiday animations if requested
        var holidayMode = getURLParam('holidayMode', false);
        if (holidayMode === true && !holidayAnimations.enabled) {
            initHolidayAnimations();
        }

        syncCameraControlSliders();
        console.log('URL parameters applied');
    }

    // Wait for DOM and THREE.js to be ready
    function initializeWhenReady() {
        function waitForThree() {
            if (typeof window.THREE !== 'undefined' &&
                typeof window.OBJLoader !== 'undefined' &&
                typeof window.MTLLoader !== 'undefined' &&
                typeof window.OrbitControls !== 'undefined') {
                console.log('Three.js modules loaded, initializing 3D view');
                setupClient();
            } else {
                console.log('Waiting for Three.js modules to load...');
                setTimeout(waitForThree, 50);
            }
        }
        waitForThree();
    }

    // Use native DOMContentLoaded (works with or without jQuery)
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', initializeWhenReady);
    } else {
        initializeWhenReady();
    }

</script>

<style>
    #canvas-container {
        border: 1px solid #333;
        display: inline-block;
        position: relative;
    }

    /* Fullscreen styles */
    #canvas-container:fullscreen {
        width: 100vw !important;
        height: 100vh !important;
        background-color: #000;
        border: none;
    }

    #canvas-container:-webkit-full-screen {
        width: 100vw !important;
        height: 100vh !important;
        background-color: #000;
        border: none;
    }

    #canvas-container:-moz-full-screen {
        width: 100vw !important;
        height: 100vh !important;
        background-color: #000;
        border: none;
    }

    #canvas-container:-ms-fullscreen {
        width: 100vw !important;
        height: 100vh !important;
        background-color: #000;
        border: none;
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

    .control-section {
        margin-top: 10px;
        padding-top: 8px;
        border-top: 1px solid #e0e0e0;
    }

    .advanced-toggle {
        margin-top: 12px;
        display: inline-block;
    }

    .advanced-section {
        margin-top: 10px;
        padding-top: 10px;
        border-top: 1px dashed #ccc;
        display: none;
    }
</style>

<?php if (!isset($standalone) || !$standalone): ?>
    <div id="controls">
        <div style="margin-bottom: 8px;">
            <input type='button' id='stopButton' onClick='stopSSE();' value='Stop 3D Virtual Display'>
            <input type='button' onClick='toggleAxes();' value='Toggle Axes'>
            <input type='button' onClick='toggleGrid();' value='Toggle Grid'>
            <input type='button' onClick='toggle3DObjects();' value='Toggle 3D Objects'>
            <input type='button' onClick='toggleHolidayAnimations();' value='Toggle Holiday Fun!'
                style='background-color: #ff6b6b; color: white; font-weight: bold;'>
            <input type='button' onClick='toggleFullscreen();' value='Fullscreen'
                style='background-color: #4a90e2; color: white; font-weight: bold;'>
            <input type='button' onClick='copyCurrentViewURL();' value='📋 Copy View URL'
                style='background-color: #2ecc71; color: white; font-weight: bold;'
                title='Copy URL with current camera position and settings'>
        </div>
        <div>
            <span class="control-group">
                Pixel Size: <input type='range' id='pixelSizeSlider' min='1' max='50' value='3' step='1'
                    oninput='updatePixelSize();'>
                <span id='pixelSizeValue'>3.0</span>
            </span>
            <span class="control-group">
                Pixel Brightness: <input type='range' id='brightnessSlider' min='0.1' max='5.0' value='2.0' step='0.1'
                    oninput='updateBrightness();'>
                <span id='brightnessValue'>2.00x</span>
            </span>
            <span class="control-group">
                Ambient Light: <input type='range' id='ambientLightSlider' min='0.0' max='3.0' value='1.0' step='0.1'
                    oninput='updateAmbientLight();'>
                <span id='ambientLightValue'>1.00x</span>
            </span>
        </div>
        <div class="control-section">
            <span class="control-group">
                Camera Yaw: <input type='range' id='cameraYawSlider' min='0' max='360' value='20' step='1'
                    oninput='handleCameraControlInput();'>
                <span id='cameraYawValue'>20°</span>
            </span>
            <span class="control-group">
                Camera Pitch: <input type='range' id='cameraPitchSlider' min='-89' max='89' value='10' step='1'
                    oninput='handleCameraControlInput();'>
                <span id='cameraPitchValue'>10°</span>
            </span>
            <span class="control-group">
                Camera Distance: <input type='range' id='cameraDistanceSlider' min='100' max='5000' value='1000' step='10'
                    oninput='handleCameraControlInput();'>
                <span id='cameraDistanceValue'>1000 units</span>
            </span>
        </div>
        <div class="control-section">
            <span class="control-group">
                Pan X: <input type='range' id='panXSlider' min='-3000' max='3000' value='0' step='5'
                    oninput='handleCameraControlInput();'>
                <span id='panXValue'>0</span>
            </span>
            <span class="control-group">
                Pan Y: <input type='range' id='panYSlider' min='-3000' max='3000' value='0' step='5'
                    oninput='handleCameraControlInput();'>
                <span id='panYValue'>0</span>
            </span>
            <span class="control-group">
                Pan Z: <input type='range' id='panZSlider' min='-3000' max='3000' value='0' step='5'
                    oninput='handleCameraControlInput();'>
                <span id='panZValue'>0</span>
            </span>
            <span style="margin-left: 15px; color: #666; font-style: italic;">
                Left-click: rotate | Middle-click: pan | Scroll: zoom
            </span>
        </div>
        <div class="advanced-toggle">
            <input type='button' id='pixelOffsetToggle' value='Show Visualizer Advanced Settings'
                onclick='togglePixelOffsetAdvanced();'>
        </div>
        <div id='pixelOffsetSection' class='advanced-section'>
            <div style="margin-bottom: 8px; color: #555;">
                Adjust pixel position offsets to align with 3D objects. Click "Save Offsets" to remember your settings.
            </div>
            <span class="control-group">
                <strong>Pixel X Offset:</strong> <input type='range' id='pixelXSlider' min='-2000' max='2000' value='0'
                    step='1' oninput='updatePixelOffset();'>
                <span id='pixelXValue' style="font-weight: bold; color: #e74c3c;">0</span>
            </span>
            <span class="control-group">
                <strong>Pixel Y Offset:</strong> <input type='range' id='pixelYSlider' min='-2000' max='2000' value='0'
                    step='1' oninput='updatePixelOffset();'>
                <span id='pixelYValue' style="font-weight: bold; color: #e74c3c;">0</span>
            </span>
            <span class="control-group">
                <strong>Pixel Z Offset:</strong> <input type='range' id='pixelZSlider' min='-2000' max='2000' value='0'
                    step='1' oninput='updatePixelOffset();'>
                <span id='pixelZValue' style="font-weight: bold; color: #e74c3c;">0</span>
            </span>
            <span class="control-group">
                <button onclick='savePixelOffsets()'
                    style="padding: 5px 15px; margin-left: 10px; background: #27ae60; color: white; border: none; border-radius: 4px; cursor: pointer;">Save
                    Offsets</button>
                <button onclick='resetPixelOffsets()'
                    style="padding: 5px 15px; margin-left: 5px; background: #e74c3c; color: white; border: none; border-radius: 4px; cursor: pointer;">Reset</button>
            </span>
            <div class="control-section" style="margin-top:15px;">
                <span class="control-group" style="margin-bottom:6px; display:inline-block;">
                    <label style="font-weight:bold;">
                        <input type="checkbox" id="testModeToggle" onchange="handleTestModeToggle();">
                        Static Color Test Mode
                    </label>
                </span>
                <span class="control-group">
                    Color: <input type="color" id="testModeColor" value="#ff0000"
                        onchange="handleTestColorChange(this.value);">
                    <span id="testModeColorValue">#FF0000</span>
                </span>
                <span class="control-group" id="testModeStatus">Live data</span>
                <div style="margin-top: 5px; color: #666; font-style: italic;">
                    When enabled, incoming data is ignored and every pixel renders the selected color so you can inspect
                    spatial placement.
                </div>
            </div>
        </div>
    </div>
<?php endif; ?>
<div id='canvas-container'></div>
<?php if (!isset($standalone) || !$standalone): ?>
    <div id='objectsList'
        style="margin-top: 10px; padding: 10px; background-color: #f9f9f9; border: 1px solid #ddd; border-radius: 4px;">
        <em>3D Objects will appear here once loaded...</em>
    </div>
<?php endif; ?>
<div id='data'></div>