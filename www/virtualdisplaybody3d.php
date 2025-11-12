<script src="js/three.min.js"></script>
<script src="js/OBJLoader.js"></script>
<script src="js/MTLLoader.js"></script>
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

    // 3D Objects variables
    var loadedObjects = [];
    var objLoader = null;
    var mtlLoader = null;

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
                echo "pixelLookup['" . $key . "'] = " . $pixelIndex . ";\n";
                $pixelIndex++;
            }

            fclose($f);
        }
    }

    // Load 3D asset configuration
    $assetsConfigFile = $settings['virtualDisplayAssetsDirectory'] . '/virtdisplay.json';
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
            foreach ($viewObjects as $obj) {
                if (isset($obj['DisplayAs']) && $obj['DisplayAs'] === 'Gridlines') {
                    $gridlinesOffsetX = (float) $obj['WorldPosX'];
                    $gridlinesOffsetY = (float) $obj['WorldPosY'];
                    $gridlinesOffsetZ = (float) $obj['WorldPosZ'];
                    echo "console.log('Found gridlines at X=$gridlinesOffsetX, Y=$gridlinesOffsetY, Z=$gridlinesOffsetZ');\n";
                    echo "console.log('Pixel center is X=$centerX, Y=$centerY, Z=$centerZ');\n";
                    break;
                }
            }
            echo "var gridlinesOffset = { x: $gridlinesOffsetX, y: $gridlinesOffsetY, z: $gridlinesOffsetZ };\n";
        } else {
            echo "console.warn('No view_objects found in virtdisplay.json');\n";
            echo "var gridlinesOffset = { x: 0, y: 0, z: 0 };\n";
        }
    } else {
        echo "console.warn('virtdisplay.json not found at: " . $assetsConfigFile . "');\n";
        echo "var gridlinesOffset = { x: 0, y: 0, z: 0 };\n";
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

    var canvasWidth = <?php echo $canvasWidth; ?>;
    var canvasHeight = <?php echo $canvasHeight; ?>;
    var objectCenterOffset = { x: 0, y: 0, z: 0 };  // Will be calculated from gridlines

    function calculateObjectCenterOffset() {
        // Objects and pixels are both in xLights 3D coordinate system
        // Gridlines define where (0,0,0) is in xLights coordinates
        // Both should be centered relative to the gridlines position to maintain their relationship

        // Use gridlines offset as the center point for both pixels and objects
        objectCenterOffset.x = gridlinesOffset.x;
        objectCenterOffset.y = gridlinesOffset.y;
        objectCenterOffset.z = gridlinesOffset.z;

        console.log('Object center offset (using gridlines):', objectCenterOffset);
        console.log('Pixel model center:', modelCenter);
        console.log('Gridlines offset:', gridlinesOffset);
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

        // Add additional lights for better 3D object visibility
        var directionalLight2 = new THREE.DirectionalLight(0xffffff, 0.5);
        directionalLight2.position.set(-1, -1, -1);
        scene.add(directionalLight2);

        var hemisphereLight = new THREE.HemisphereLight(0xffffff, 0x444444, 0.6);
        hemisphereLight.position.set(0, 1, 0);
        scene.add(hemisphereLight);

        // Use Points (point cloud) for efficient rendering of many pixels
        console.log('Creating point cloud with', pixelData.length, 'pixels');

        var geometry = new THREE.BufferGeometry();
        var positions = new Float32Array(pixelData.length * 3);
        var colors = new Float32Array(pixelData.length * 3);

        // Debug: log first few pixel positions
        console.log('First 3 pixels - RAW coordinates:');
        for (var i = 0; i < Math.min(3, pixelData.length); i++) {
            console.log('  Pixel', i, '- x:', pixelData[i].x, 'y:', pixelData[i].y, 'z:', pixelData[i].z);
        }
        console.log('Model center:', modelCenter);
        console.log('Center at 0:', view2DSettings.centerAt0);

        for (var i = 0; i < pixelData.length; i++) {
            // Pixels come from virtualdisplaymap which may have Display2DCenter0 applied
            // When Display2DCenter0 is enabled, pixel X coordinates are offset by preview center X
            // Objects (from view_objects) are in pure 3D space, not affected by Display2DCenter0
            // So we need to convert pixel X coordinates back to match object space

            var pixelX = pixelData[i].x;
            var pixelY = pixelData[i].y;
            var pixelZ = pixelData[i].z;

            if (view2DSettings.centerAt0) {
                // When Display2DCenter0 is enabled, pixels need to align with object coordinate system
                // Subtract modelCenter.x and apply additional correction factor
                pixelX = pixelX - modelCenter.x - 274;
                // Y and Z are fine as-is
            }

            // Now apply the same gridlines offset as objects
            positions[i * 3] = pixelX - gridlinesOffset.x;
            positions[i * 3 + 1] = pixelY - gridlinesOffset.y;
            positions[i * 3 + 2] = pixelZ - gridlinesOffset.z;

            // Start all black
            colors[i * 3] = 0;
            colors[i * 3 + 1] = 0;
            colors[i * 3 + 2] = 0;
        }

        // Debug: log first few pixel positions after centering
        console.log('First 3 pixels - CENTERED coordinates:');
        for (var i = 0; i < Math.min(3, pixelData.length); i++) {
            console.log('  Pixel', i, '- x:', positions[i * 3], 'y:', positions[i * 3 + 1], 'z:', positions[i * 3 + 2]);
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

        // Initialize 3D object loaders
        if (initObjectLoaders()) {
            // Calculate the coordinate offset from gridlines
            calculateObjectCenterOffset();

            // Load 3D objects from configuration
            load3DObjects();
        } else {
            console.warn('3D object loaders failed to initialize, skipping 3D object loading');
        }

        console.log('3D initialization complete');
    }

    function initObjectLoaders() {
        // Initialize the OBJ and MTL loaders
        try {
            if (typeof THREE.OBJLoader === 'undefined') {
                console.error('THREE.OBJLoader is not available. Make sure OBJLoader.js is loaded.');
                return false;
            }
            if (typeof THREE.MTLLoader === 'undefined') {
                console.error('THREE.MTLLoader is not available. Make sure MTLLoader.js is loaded.');
                return false;
            }

            objLoader = new THREE.OBJLoader();
            mtlLoader = new THREE.MTLLoader();
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
        // Try loading MTL file first
        mtlLoader.load(
            mtlPath,
            (materials) => {
                console.log('Successfully loaded materials for:', objConfig.name);
                
                try {
                    // Disable texture loading to avoid compatibility issues
                    // We'll use colors from the MTL but not textures
                    for (var materialName in materials.materials) {
                        var mat = materials.materials[materialName];
                        // Clear any texture references to avoid loader errors
                        if (mat.map) mat.map = null;
                        if (mat.normalMap) mat.normalMap = null;
                        if (mat.specularMap) mat.specularMap = null;
                        if (mat.bumpMap) mat.bumpMap = null;
                    }
                    
                    materials.preload();
                    
                    // Set materials to OBJ loader
                    objLoader.setMaterials(materials);
                    
                    // Now load the OBJ with materials
                    loadObjFile(objPath, objConfig, index, true);
                } catch (error) {
                    console.warn('Error processing MTL materials for', objConfig.name, ':', error);
                    console.log('Falling back to default materials');
                    loadObjFile(objPath, objConfig, index, false);
                }
            },
            (progress) => {
                // Progress callback
            },
            (error) => {
                console.warn('Could not load MTL file for', objConfig.name, '- using default materials');
                // Fall back to loading without materials
                loadObjFile(objPath, objConfig, index, false);
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

    function updatePixelOffset() {
        var xOffset = parseFloat(document.getElementById('pixelXSlider').value);
        var yOffset = parseFloat(document.getElementById('pixelYSlider').value);
        var zOffset = parseFloat(document.getElementById('pixelZSlider').value);

        // Update display values
        document.getElementById('pixelXValue').textContent = xOffset.toFixed(0);
        document.getElementById('pixelYValue').textContent = yOffset.toFixed(0);
        document.getElementById('pixelZValue').textContent = zOffset.toFixed(0);

        // Update pixel positions in the geometry
        if (window.pixelGeometry && pixelData) {
            var positions = window.pixelGeometry.attributes.position.array;

            for (var i = 0; i < pixelData.length; i++) {
                var pixelX = pixelData[i].x;
                var pixelY = pixelData[i].y;
                var pixelZ = pixelData[i].z;
                
                if (view2DSettings.centerAt0) {
                    // Convert X from Display2DCenter0 coordinate space to object space
                    pixelX = pixelX - modelCenter.x - 274;
                    // Y and Z are fine as-is
                }
                
                // Apply gridlines offset and manual adjustment
                positions[i * 3] = (pixelX - gridlinesOffset.x) + xOffset;
                positions[i * 3 + 1] = (pixelY - gridlinesOffset.y) + yOffset;
                positions[i * 3 + 2] = (pixelZ - gridlinesOffset.z) + zOffset;
            }            window.pixelGeometry.attributes.position.needsUpdate = true;
            console.log('Pixel offset adjusted to: X=' + xOffset + ', Y=' + yOffset + ', Z=' + zOffset);
        }
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
        }
    }

    function get3DObjectsList() {
        var list = '<div style="margin-top: 10px;"><strong>3D Objects:</strong><ul style="margin: 5px 0; padding-left: 20px;">';

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
        <input type='button' onClick='toggle3DObjects();' value='Toggle 3D Objects'>
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
    </div>
    <div style="margin-top: 8px;">
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
        <span style="margin-left: 15px; color: #666; font-style: italic;">
            Left-click: rotate | Middle-click: pan | Scroll: zoom
        </span>
    </div>
</div>
<div id='canvas-container'></div>
<div id='objectsList'
    style="margin-top: 10px; padding: 10px; background-color: #f9f9f9; border: 1px solid #ddd; border-radius: 4px;">
    <em>3D Objects will appear here once loaded...</em>
</div>
<div id='data'></div>