<!DOCTYPE html>
<html lang="en">

<head>
    <!-- Prevent caching of the page to avoid SSE connection issues -->
    <meta http-equiv="Cache-Control" content="no-cache, no-store, must-revalidate" />
    <meta http-equiv="Pragma" content="no-cache" />
    <meta http-equiv="Expires" content="0" />

    <!-- Three.js import map MUST be absolutely first, before ANY other scripts -->
    <script type="importmap">
    {
        "imports": {
            "three": "/js/three.module.js",
            "three/addons/": "/js/"
        }
    }
    </script>

    <meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
    <title>FPP 3D Virtual Display</title>

    <?php
    require_once('common.php');
    require_once('config.php');

    // Check for standalone mode
    $standalone = isset($_GET['standalone']) && $_GET['standalone'] === 'true';

    if (!$standalone) {
        include 'common/htmlMeta.inc';
        include_once 'common/menuHead.inc';
    }

    if (isset($_GET['width'])) {
        $canvasWidth = (int) ($_GET['width']);
        $canvasHeight = (int) ($canvasWidth * 3.0 / 4.0);  // 4:3 aspect ratio for 3D
    } else if ($standalone) {
        // In standalone mode, use full window size
        $canvasWidth = 1920;  // Will be overridden by JavaScript
        $canvasHeight = 1080;
    }

    ?>

    <!-- Load Three.js modules after import map is defined -->
    <script type="module">
        import * as THREE from 'three';
        import { OBJLoader } from 'three/addons/OBJLoader.module.js';
        import { MTLLoader } from 'three/addons/MTLLoader.module.js';
        import { OrbitControls } from 'three/addons/OrbitControls.module.js';

        // Make THREE and loaders globally available
        window.THREE = THREE;
        window.OBJLoader = OBJLoader;
        window.MTLLoader = MTLLoader;
        window.OrbitControls = OrbitControls;

        // Signal that Three.js is fully loaded
        window.threeJsLoaded = true;
        console.log('Three.js modules loaded and ready');

        // Dispatch a custom event so any waiting code can be notified
        window.dispatchEvent(new Event('threejs-loaded'));
    </script>

    <?php if ($standalone): ?>
        <!-- Standalone mode: full window styling -->
        <style>
            body,
            html {
                margin: 0;
                padding: 0;
                width: 100%;
                height: 100%;
                overflow: hidden;
                background-color: #000;
            }

            #canvas-container {
                width: 100vw !important;
                height: 100vh !important;
                margin: 0 !important;
                padding: 0 !important;
            }

            #canvas-container canvas {
                display: block;
                width: 100% !important;
                height: 100% !important;
            }

            /* Hide ALL control buttons, UI elements, and text */
            .controlButtons,
            .controls,
            button,
            input,
            label,
            h1,
            h2,
            h3,
            h4,
            p,
            ul,
            li,
            div:not(#canvas-container) {
                display: none !important;
            }

            /* Only show the canvas container */
            #canvas-container {
                display: block !important;
            }
        </style>
        <script>
            // Standalone mode flag
            window.standaloneMode = true;

            // Set canvas to full window size
            window.addEventListener('DOMContentLoaded', function () {
                window.canvasWidth = window.innerWidth;
                window.canvasHeight = window.innerHeight;

                // Handle window resize
                window.addEventListener('resize', function () {
                    if (window.renderer && window.camera) {
                        window.renderer.setSize(window.innerWidth, window.innerHeight);
                        window.camera.aspect = window.innerWidth / window.innerHeight;
                        window.camera.updateProjectionMatrix();
                    }
                });
            });

            // Mock jQuery for standalone mode (avoid errors)
            window.$ = function (selector) {
                if (selector === document) {
                    return {
                        ready: function (fn) {
                            if (document.readyState === 'loading') {
                                document.addEventListener('DOMContentLoaded', fn);
                            } else {
                                fn();
                            }
                        }
                    };
                }
                return {
                    hide: function () { },
                    show: function () { },
                    val: function () { return ''; }
                };
            };
        </script>
    <?php endif; ?>
</head>

<body<?php if ($standalone)
    echo ' style="margin:0;padding:0;"'; ?>>
    <?php if ($standalone): ?>
        <!-- Standalone mode: just the canvas -->
        <?php require_once('virtualdisplaybody3d.php'); ?>
    <?php else: ?>
        <!-- Normal mode: full FPP UI -->
        <div id="bodyWrapper">
            <?php
            $activeParentMenuItem = 'status';
            include 'menu.inc';
            ?>
            <div class="mainContainer">
                <h2 class="title">3D Virtual Display</h2>
                <div class="pageContent">
                    <?
                    require_once('virtualdisplaybody3d.php');
                    ?>
                    <br>
                    <b>Instructions:</b>
                    <ul>
                        <li>Use your <b>left mouse button to drag</b> to rotate the 3D view</li>
                        <li>Use your <b>middle mouse button (scroll wheel press) to drag</b> to pan/move the view</li>
                        <li>Use your <b>scroll wheel</b> to zoom in/out</li>
                        <li>Click the <b>Fullscreen button</b> for an immersive full-screen experience (press <b>ESC</b> to
                            exit)</li>
                        <li>Click <b>📋 Copy View URL</b> to save your current camera position and settings as a shareable
                            URL</li>
                        <li>The display shows your pixel layout in <b>true 3D space</b> using the Z-axis data from your
                            virtualdisplaymap</li>
                    </ul>
                    <br>
                    <b>URL Parameters:</b> You can customize the view by adding parameters to the URL. Examples:
                    <ul style="font-size: 0.9em;">
                        <li><b>⭐ Standalone Mode:</b> <code>?standalone=true</code> - <b>Full window visualizer with NO UI
                                elements</b> (perfect for dedicated displays!)</li>
                        <li><b>Camera Position:</b> <code>?cameraX=500&cameraY=300&cameraZ=800</code> - Set exact camera
                            position</li>
                        <li><b>Camera Angles:</b> <code>?cameraAngleH=45&cameraAngleV=20</code> - Set horizontal/vertical
                            angles (degrees)</li>
                        <li><b>Camera Target:</b> <code>?targetX=0&targetY=100&targetZ=0</code> - Set what camera looks at
                        </li>
                        <li><b>Zoom Level:</b> <code>?zoom=0.5</code> - Zoom multiplier (0.5=closer, 2.0=farther)</li>
                        <li><b>Field of View:</b> <code>?fov=90</code> - Camera FOV in degrees (default 75)</li>
                        <li><b>Fullscreen:</b> <code>?fullscreen=true</code> - Auto-enter fullscreen (click page if browser
                            blocks it)</li>
                        <li><b>Brightness:</b> <code>?brightness=3.0</code> - Pixel brightness multiplier (default 2.0)</li>
                        <li><b>Ambient Light:</b> <code>?ambientLight=0.5</code> - Scene lighting intensity (default 1.0,
                            0.0=dark)</li>
                        <li><b>Pixel Size:</b> <code>?pixelSize=5</code> - Pixel point size (default 1)</li>
                        <li><b>Show Grid:</b> <code>?showGrid=false</code> - Hide the grid helper</li>
                        <li><b>Show Axes:</b> <code>?showAxes=true</code> - Show the axis helper</li>
                        <li><b>Holiday Mode:</b> <code>?holidayMode=true</code> - Enable holiday animations</li>
                        <li><b>Combine multiple:</b>
                            <code>?standalone=true&zoom=0.8&brightness=2.5&ambientLight=0.3&holidayMode=true</code>
                        </li>
                    </ul>
                    <br>
                    <b>NOTE:</b> If the 3D Virtual Display is not working, you need to add the HTTP Virtual Display Channel
                    Output on the
                    'Other' tab of the Channel Outputs config page. Your pixel map must be copied to FPP's configuration
                    directory from xLights
                    as a file called 'virtualdisplaymap'.
                    <br><br>
                    <b>Warning:</b> If your tree models appear tilted or off-vertical inside the 3D view, recreate those
                    trees in the current version of xLights. Older xLights releases generated tree model data with
                    alignment inaccuracies, and only a fresh tree generation and export will correct the geometry FPP
                    receives.
                </div>
            </div>

            <?php include 'common/footer.inc'; ?>
        </div>
    <?php endif; ?>
    </body>

</html>