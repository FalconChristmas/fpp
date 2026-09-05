<?php
/**
 * Resolve a plugin-relative path to a real path inside the plugin directory,
 * or null if it is not one.
 *
 * Both callers below need the same answer, and they need it to mean the same
 * thing: a page and a file are equally allowed to live in a subdirectory. The
 * `page` branch used basename(), which does not reject a bad path -- it
 * silently rewrites a good one, so a plugin whose help lives in help/ asked
 * for help/help.php and was told help.php does not exist.
 *
 * Traversal is refused component by component. Note that "\x2e\x2e" survives an
 * [A-Za-z0-9_.-] allow-list, since a dot is a legal filename character, so it
 * has to be rejected by name rather than by character class.
 */
function pluginRealPath($pluginDirectory, $pluginName, $rawPath)
{
    if (!is_string($rawPath) || strpos($rawPath, "\0") !== false) {
        return null;
    }
    $parts = explode('/', str_replace('\\', '/', $rawPath));
    $clean = array();
    foreach ($parts as $part) {
        if ($part === '' || $part === '.') {
            continue;
        }
        if ($part === '..') {
            return null;
        }
        if (preg_replace('/[^A-Za-z0-9_.-]/', '', $part) !== $part) {
            return null;
        }
        $clean[] = $part;
    }
    if (empty($clean)) {
        return null;
    }
    $realBase = realpath($pluginDirectory);
    $realPath = realpath($pluginDirectory . '/' . $pluginName . '/' . implode('/', $clean));
    if ($realBase === false || $realPath === false) {
        return null;
    }
    // The separator is part of the test on purpose: a plain prefix comparison
    // also accepts a sibling whose name merely starts with the base, so
    // ".../pluginsOther" would count as being inside ".../plugins".
    if (strpos($realPath, rtrim($realBase, '/') . '/') !== 0) {
        return null;
    }
    return $realPath;
}
?>
<?php
$pluginName = "";
$activeParentMenuItem = "status";
if (!isset($_GET['nopage'])):
    // The DOCTYPE has to be the first thing on the wire: config.php emits the
    // "Falcon Player - FPP" discovery comment and the settings <script> block as
    // soon as it is required, and any markup ahead of the DOCTYPE puts the
    // browser into Quirks Mode. Open the document first and pull the includes in
    // from inside <head>, the same way index.php and the other core pages do.
    ?>
    <!DOCTYPE html>
    <html lang="en">

    <head>
        <?php
        require_once "config.php";
        require_once "common.php";

    $pluginSettings = array();
    $pluginSettingInfos = array();

    if (isset($_GET['_menu'])) {
        $activeParentMenuItem = htmlspecialchars($_GET['_menu'], ENT_QUOTES, 'UTF-8');
    }

    if (isset($_GET['plugin'])) {
        $rawPlugin = $_GET['plugin'];
        // Strict allow-list for plugin names — prevents ../ traversal and shell metachars.
        // Valid names are like "my-plugin_1.2" — only A-Z, 0-9, _, ., -
        $pluginName = preg_replace('/[^A-Za-z0-9_.-]/', '', $rawPlugin);
        if ($pluginName !== $rawPlugin || $pluginName === '' || strpos($pluginName, '..') !== false) {
            $pluginName = '';
        } else {
            LoadPluginSettings($pluginName);
        }
    }

    $infoFile = $pluginDirectory . '/' . $pluginName . '/pluginInfo.json';
    if (file_exists($infoFile)) {
        $json = file_get_contents($infoFile);
        $pluginInfo = json_decode($json, true);
    } else {
        $pluginInfo = array();
        $pluginInfo["name"] = "Unknown";
    }

        include 'common/htmlMeta.inc';
        include 'common/menuHead.inc'; ?>
        <title><? echo $pageTitle; ?></title>
        <script type="text/javascript">
            function bindSettingsVisibilityListener() {
                var visProp = getHiddenProp();
                if (visProp) {
                    var evtname = visProp.replace(/[H|h]idden/, '') + 'visibilitychange';
                    document.addEventListener(evtname, handleSettingsVisibilityChange);
                }
            }

            function handleSettingsVisibilityChange() {
                if (isHidden() && statusTimeout != null) {
                    clearTimeout(statusTimeout);
                    statusTimeout = null;
                }
            }
            var hiddenChildren = {};
            function UpdateChildSettingsVisibility() {
                hiddenChildren = {};
                $('.parentSetting').each(function () {
                    var fn = 'Update' + $(this).attr('id') + 'Children';
                    window[fn](2); // Hide if necessary
                });
                $('.parentSetting').each(function () {
                    var fn = 'Update' + $(this).attr('id') + 'Children';
                    window[fn](1); // Show if not hidden
                });
            }
            $(document).ready(function () {
                UpdateChildSettingsVisibility();
                bindSettingsVisibilityListener();
            });

            var pluginSettings = new Array();

            <?
            foreach ($pluginSettings as $key => $value) {
                printf("	pluginSettings['%s'] = %s;\n", $key, json_encode((string) $value));
            }
            ?>
        </script>

        <?

        $jsDir = $pluginDirectory . "/" . $pluginName . "/js/";
        if (file_exists($jsDir)) {
            if ($handle = opendir($jsDir)) {
                while (($file = readdir($handle)) !== false) {
                    if (!in_array($file, array('.', '..')) && !is_dir($jsDir . $file)) {
                        printf(
                            "<script type='text/javascript' src='plugin.php?plugin=%s&file=js/%s&nopage=1'></script>\n",
                            $pluginName,
                            $file
                        );
                    }
                }
            }
        }

        $cssDir = $pluginDirectory . "/" . $pluginName . "/css/";
        if (file_exists($cssDir)) {
            if ($handle = opendir($cssDir)) {
                while (($file = readdir($handle)) !== false) {
                    if (!in_array($file, array('.', '..')) && !is_dir($cssDir . $file)) {
                        printf(
                            "<link rel='stylesheet' type='text/css' href='/plugin.php?plugin=%s&file=css/%s&nopage=1'>\n",
                            $pluginName,
                            $file
                        );
                    }
                }
            }
        }

        ?>
    </head>

    <body>
        <div id="bodyWrapper">
            <?php include 'menu.inc'; ?>
            <div class="mainContainer">
                <?php
                $pluginIconUrl = 'api/plugin/' . $pluginName . '/icon';
                $pluginIconFile = $pluginDirectory . '/' . $pluginName . '/icon.png';
                $pluginHasIcon = file_exists($pluginIconFile) || !empty($pluginInfo['iconURL']);
                if ($pluginHasIcon) {
                    if (file_exists($pluginIconFile)) {
                        $pluginIconUrl .= '?t=' . filemtime($pluginIconFile);
                    } else {
                        $pluginIconUrl .= '?t=' . time();
                    }
                }
                ?>
                <h1 class="title d-flex align-items-center gap-2">
                    <?php if ($pluginHasIcon): ?>
                    <div class="pluginIconWrap" style="width:2rem;height:2rem;border-radius:0.4rem;flex-shrink:0;">
                        <img class="pluginIcon" src="<?= $pluginIconUrl ?>" alt="">
                    </div>
                    <?php endif; ?>
                    <?= $pluginInfo['name'] ?>
                </h1>
                <div class="pageContent">


                    <?php
else:
    $skipJSsettings = 1;
    require_once "config.php";
endif;

if (isset($_GET['plugin'])) {
    $rawPlugin = $_GET['plugin'];
    $pluginName = preg_replace('/[^A-Za-z0-9_.-]/', '', $rawPlugin);
    if ($pluginName !== $rawPlugin || $pluginName === '' || strpos($pluginName, '..') !== false) {
        $pluginName = '';
    }
}

if (!isset($_GET['plugin'])) {
    echo "Please don't access this page directly";
} elseif (empty($_GET['plugin'])) {
    echo "Plugin variable empty, please don't access this page directly";
} elseif (isset($_GET['page']) && !empty($_GET['page'])) {
    $pageName = $_GET['page'];
    $realPath = pluginRealPath($pluginDirectory, $pluginName, $pageName);
    if ($realPath !== null && is_file($realPath)) {
        include_once $realPath;
    } else {
        http_response_code(404);
        echo "Error with plugin, requesting a page that doesn't exist: $pluginName/" .
            htmlspecialchars($pageName, ENT_QUOTES, 'UTF-8');
    }
} elseif (isset($_GET['file']) && !empty($_GET['file'])) {
    $realPath = pluginRealPath($pluginDirectory, $pluginName, $_GET['file']);
    if ($realPath === null || !is_file($realPath)) {
        http_response_code(404);
        echo "Error with plugin, requesting a file that doesn't exist";
        return;
    }
    $file = $realPath;
    if (file_exists($file)) {
        $filename = basename($file);
        $file_extension = strtolower(substr(strrchr($filename, "."), 1));

        switch ($file_extension) {
            case "gif":
                $ctype = "image/gif;";
                break;
            case "png":
                $ctype = "image/png;";
                break;
            case "svg":
                $ctype = "image/svg;";
                break;
            case "jpeg":
            case "jpg":
                $ctype = "image/jpg;";
                break;
            case "js":
                $ctype = "text/javascript;";
                break;
            case "json":
                $ctype = "application/json;";
                break;
            case "css":
                $ctype = "text/css;";
                break;
            default:
                $ctype = "text/plain;";
                break;
        }

        header('Content-type: ' . $ctype);

        // Without the clean/flush we send two extra bytes that
        // cause the image to be corrupt.  This is similar to the
        // bug we had with an extra 2 bytes in our log zip
        ob_clean();
        flush();
        readfile($file);
        exit();
    } else {
        error_log("Error, could not find file $file");
        http_response_code(404);
        echo "Error with plugin, requesting a file that doesn't exist";
    }
} elseif (file_exists($pluginDirectory . "/" . $pluginName . "/plugin.php")) {
    -include_once $pluginDirectory . "/" . $pluginName . "/plugin.php";
} else {
    http_response_code(404);
    echo "Plugin invalid, no main page exists";
}

if (!isset($_GET['nopage'])): ?>
                </div>
            </div>
            <?php include 'common/footer.inc'; ?>
        </div>
    </body>

    </html>
<?php endif; ?>
