<!DOCTYPE html>
<html lang="en">

<head>
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
    include 'common/htmlMeta.inc';
    require_once('common.php');
    require_once('config.php');
    include_once 'common/menuHead.inc';

    if (isset($_GET['width'])) {
        $canvasWidth = (int) ($_GET['width']);
        $canvasHeight = (int) ($canvasWidth * 3.0 / 4.0);  // 4:3 aspect ratio for 3D
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
    </script>
</head>

<body>
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
                    <li><b>Pixel Size:</b> <code>?pixelSize=5</code> - Pixel point size (default 1)</li>
                    <li><b>Show Grid:</b> <code>?showGrid=false</code> - Hide the grid helper</li>
                    <li><b>Show Axes:</b> <code>?showAxes=true</code> - Show the axis helper</li>
                    <li><b>Holiday Mode:</b> <code>?holidayMode=true</code> - Enable holiday animations</li>
                    <li><b>Combine multiple:</b> <code>?zoom=0.8&brightness=2.5&fullscreen=true&holidayMode=true</code>
                    </li>
                </ul>
                <br>
                <b>NOTE:</b> If the 3D Virtual Display is not working, you need to add the HTTP Virtual Display Channel
                Output on the
                'Other' tab of the Channel Outputs config page. Your pixel map must be copied to FPP's configuration
                directory from xLights
                as a file called 'virtualdisplaymap'.
            </div>
        </div>

        <?php include 'common/footer.inc'; ?>
    </div>
</body>

</html>