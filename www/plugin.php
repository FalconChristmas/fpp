<?php
$pluginName = "";
$activeParentMenuItem = "status";
if (!isset($_GET['nopage'])):

    require_once "config.php";
    require_once "common.php";

    $pluginSettings = array();

    if (isset($_GET['_menu'])) {
        $activeParentMenuItem = htmlspecialchars($_GET['_menu'], ENT_QUOTES, 'UTF-8');
    }

    if (isset($_GET['plugin'])) {
        $pluginName = htmlspecialchars($_GET['plugin'], ENT_QUOTES, 'UTF-8');
        $pluginConfigFile = $settings['configDirectory'] . "/plugin." . $pluginName;
        if (file_exists($pluginConfigFile)) {
            $pluginSettings = parse_ini_file($pluginConfigFile);
        }

    }

    $infoFile = $pluginDirectory . '/' . $pluginName . '/pluginInfo.json';

    if (file_exists($infoFile)) {
        $json = file_get_contents($infoFile);
        $pluginInfo = json_decode($json, true);
    }

    ?>

	<!DOCTYPE html>
	<html>
	<head>
	<?php include 'common/menuHead.inc';?>
	<title><?echo $pageTitle; ?></title>
	<script type="text/javascript">
function bindSettingsVisibilityListener() {
    var visProp = getHiddenProp();
    if (visProp) {
      var evtname = visProp.replace(/[H|h]idden/,'') + 'visibilitychange';
      document.addEventListener(evtname, handleSettingsVisibilityChange);
    }
}

function handleSettingsVisibilityChange() {
    if (isHidden() && statusTimeout != null) {
        clearTimeout(statusTimeout);
        statusTimeout = null;
    } else {
        UpdateCurrentTime();
    }
}    
var hiddenChildren = {};
function UpdateChildSettingsVisibility() {
    hiddenChildren = {};
    $('.parentSetting').each(function() {
        var fn = 'Update' + $(this).attr('id') + 'Children';
        window[fn](2); // Hide if necessary
    });
    $('.parentSetting').each(function() {
        var fn = 'Update' + $(this).attr('id') + 'Children';
        window[fn](1); // Show if not hidden
    });
}
$(document).ready(function() {
    UpdateChildSettingsVisibility();
    bindSettingsVisibilityListener();
});

	var pluginSettings = new Array();

		<?
    foreach ($pluginSettings as $key => $value) {
        printf("	pluginSettings['%s'] = \"%s\";\n", $key, $value);
    }
    ?>
										</script>

										<?

    $jsDir = $pluginDirectory . "/" . $pluginName . "/js/";
    if (file_exists($jsDir)) {
        if ($handle = opendir($jsDir)) {
            while (($file = readdir($handle)) !== false) {
                if (!in_array($file, array('.', '..')) && !is_dir($jsDir . $file)) {
                    printf("<script type='text/javascript' src='plugin.php?plugin=%s&file=js/%s&nopage=1'></script>\n",
                        $pluginName, $file);
                }
            }
        }
    }

    $cssDir = $pluginDirectory . "/" . $pluginName . "/css/";
    if (file_exists($cssDir)) {
        if ($handle = opendir($cssDir)) {
            while (($file = readdir($handle)) !== false) {
                if (!in_array($file, array('.', '..')) && !is_dir($cssDir . $file)) {
                    printf("<link rel='stylesheet' type='text/css' href='/plugin.php?plugin=%s&file=css/%s&nopage=1'>\n",
                        $pluginName, $file);
                }
            }
        }
    }

    ?>
	</head>
	<body>
	<div id="bodyWrapper">
	<?php include 'menu.inc';?>
	<div class="mainContainer">
	<h1 class="title"><?echo $pluginInfo['name']; ?></h1>
	<div class="pageContent">


	<?php
else:
    $skipJSsettings = 1;
    require_once "config.php";
endif;

if (isset($_GET['plugin'])) {
    $pluginName = htmlspecialchars($_GET['plugin'], ENT_QUOTES, 'UTF-8');
}

if (!isset($_GET['plugin'])) {
    echo "Please don't access this page directly";
} elseif (empty($_GET['plugin'])) {
    echo "Plugin variable empty, please don't access this page directly";
} elseif (isset($_GET['page']) && !empty($_GET['page'])) {
    $pageName = htmlspecialchars($_GET['page'], ENT_QUOTES, 'UTF-8');

    if (file_exists($pluginDirectory . "/" . $pluginName . "/" . $pageName)) {
        -include_once $pluginDirectory . "/" . $pluginName . "/" . $pageName;
    } else {
        echo "Error with plugin, requesting a page that doesn't exist: $pluginName/$pageName";
    }
} elseif (isset($_GET['file']) && !empty($_GET['file'])) {
    $fileName = htmlspecialchars($_GET['file'], ENT_QUOTES, 'UTF-8');

    $file = $pluginDirectory . "/" . $pluginName . "/" . $fileName;

    if (file_exists($file)) {
        $filename = basename($file);
        $file_extension = strtolower(substr(strrchr($filename, "."), 1));

        switch ($file_extension) {
            case "gif":$ctype = "image/gif;";
                break;
            case "png":$ctype = "image/png;";
                break;
            case "jpeg":
            case "jpg":$ctype = "image/jpg;";
                break;
            case "js":$ctype = "text/javascript;";
                break;
            case "json":$ctype = "application/json;";
                break;
            case "css":$ctype = "text/css;";
                break;
            default:$ctype = "text/plain;";
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
        echo "Error with plugin, requesting a file that doesn't exist";
    }
} elseif (file_exists($pluginDirectory . "/" . $pluginName . "/plugin.php")) {
    -include_once $pluginDirectory . "/" . $pluginName . "/plugin.php";
} else {
    echo "Plugin invalid, no main page exists";
}

if (!isset($_GET['nopage'])): ?>
</div>
</div>
<?php	include 'common/footer.inc';?>
</div>
</body>
</html>
<?php endif;?>
