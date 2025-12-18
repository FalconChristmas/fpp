<?
// Switch to Initial Setup page if needed, but come back here when done
if (((!isset($settings['initialSetup-02'])) || ($settings['initialSetup-02'] == '0')) && (basename($_SERVER['PHP_SELF']) != "initialSetup.php")) {
    echo "<meta http-equiv='refresh' content='0;url=initialSetup.php?redirect=" . preg_replace('/^\//', '', $_SERVER['PHP_SELF']) . "' />\n";
    exit(0);
}
?>

<!-- jquery -->
<script type="text/javascript" src="js/jquery-latest.min.js?ref=<?= filemtime('js/jquery-latest.min.js'); ?>"></script>
<!-- <script src="https://code.jquery.com/jquery-migrate-3.4.1.js"></script> -->
<script type="text/javascript" src="js/jquery-ui.min.js"></script>
<script type="text/javascript" src="js/jquery.ui.touch-punch.js"></script>
<?
if ((!isset($settings['disableUIPopoverAlerts'])) || ($settings['disableUIPopoverAlerts'] == '0')) {
    // Enable popover alerts
    echo "<script type='text/javascript' src='js/jquery.jgrowl.min.js'></script>";
} elseif (isset($settings['disableUIPopoverAlerts']) && ($settings['disableUIPopoverAlerts'] == '1')) {
    echo "<script type='text/javascript' src='js/jquery.jgrowl.disabled.js'></script>";
}
?>

<script type="text/javascript" src="js/zebra_pin.min.js"></script>
<script type="text/javascript" src="js/popper.min.js"></script>
<script type="text/javascript" src="jquery/jquery.floatThead/jquery.floatThead.min.js"></script>
<script type="text/javascript"
    src="js/bootstrap.bundle.min.js?ref=<?= filemtime('js/bootstrap.bundle.min.js'); ?>"></script>

<link rel="stylesheet" href="css/jquery-ui.css">
<link rel="stylesheet" href="css/jquery.jgrowl.min.css">
<link rel="stylesheet" href="css/classicTheme/style.css" media="all">

<link rel="icon" href="favicon.ico" type="image/x-icon">

<!--
        Font Awesome Free 6.7.2 by @fontawesome - https://fontawesome.com
        License - https://fontawesome.com/license/free (Icons: CC BY 4.0, Fonts: SIL OFL 1.1, Code: MIT License)
    -->
<link rel="stylesheet" href="css/fontawesome.all.min.css">
<link rel="stylesheet" href="webfonts/fpp-icons/styles.css">

<!--
    ZERO-MD 3.1.2 - https://github.com/zerodevx/zero-md
    License - https://github.com/zerodevx/zero-md/blob/main/LICENSE (ISC License)
-->
<script type="module">
    import ZeroMdBase from '/js/zero-md/zero-md-base.js'
    import ZeroMd from '/js/zero-md/zero-md.js'
    import { STYLES, LOADERS } from '/js/zero-md/presets.js'

    customElements.define('zero-md', class extends ZeroMd {
        async load() {
            await super.load()
            this.template = `
<style>:host { display: block; }</style>
<link rel="stylesheet" href="/js/zero-md/css/github-markdown-light.min.css">
<link rel="stylesheet" href="/js/zero-md/css/github-markdown.min.css">
<link rel="stylesheet" href="/js/zero-md/css/katex.min.css">`
            //this.template = STYLES.preset('light') // or STYLES.preset('dark')
        }
    })

    export { ZeroMdBase, STYLES, LOADERS }
    export default ZeroMd
</script>

<?
if (isset($themeInfo["preFPPStyleSheet"])) {
    foreach ($themeInfo["preFPPStyleSheet"] as $ss) {
        if ($ss != "") {
            echo "<link rel='stylesheet' href='" . $ss . "' >";
        }
    }
}
?>


<!-- finally FPP stuff, here so our CSS is last so we can override anything above -->

<link rel="stylesheet"
    href="css/fpp-bootstrap/dist-new/fpp-bootstrap-5-3.css?ref=<?php echo filemtime('css/fpp-bootstrap/dist-new/fpp-bootstrap-5-3.css'); ?>">
<link rel="stylesheet" href="css/fpp.css?ref=<?php echo filemtime('css/fpp.css'); ?>">

<?
if (isset($themeInfo["postFPPStyleSheet"])) {
    foreach ($themeInfo["postFPPStyleSheet"] as $ss) {
        if ($ss != "") {
            echo "<link rel='stylesheet' href='" . $ss . "' >";
        }
    }
}
?>

<? if (file_exists($mediaDirectory . "/config/custom.css")) { ?>
<link rel="stylesheet"
    href="api/configfile/custom.css?ref=<?php echo filemtime($mediaDirectory . '/config/custom.css'); ?>">
<? } ?>
<script type="text/javascript" src="js/fpp.js?ref=<?php echo filemtime('js/fpp.js'); ?>"></script>
<? if (file_exists($mediaDirectory . "/config/custom.js")) { ?>
<script type="text/javascript"
    src="api/configfile/custom.js?ref=<?php echo filemtime($mediaDirectory . '/config/custom.js'); ?>"></script>
<? } ?>

<?
if (isset($themeInfo["themeJS"])) {
    foreach ($themeInfo["themeJS"] as $js) {
        if ($js != "") {
            echo "<script type='text/javascript' src='" . $js . "' />";
        }
    }
}
if (isset($settings['backgroundColor']) && $settings['backgroundColor'] != "") {
    ?>
<link rel="icon"
    href="data:image/svg+xml,%3csvg xmlns='http://www.w3.org/2000/svg' width='96' height='96' viewBox='0 0 96 96'%3e%3cg id='app-icon' transform='translate(-1977.763 -385.763)'%3e%3crect id='Rectangle_79' data-name='Rectangle 79' width='96' height='96' rx='25' transform='translate(1977.763 385.763)' fill='%23<? echo $settings['backgroundColor'] ?>'/%3e%3cg id='Group_124' data-name='Group 124' transform='translate(1994.608 402.675)'%3e%3cpath id='Path_23' data-name='Path 23' d='M424.977%2c584H397.358c-7.446%2c0-14.721%2c6.056-16.216%2c13.5l-7.047%2c35.063a11.489%2c11.489%2c0%2c0%2c0%2c2.073%2c9.641%2c11%2c11%2c0%2c0%2c0%2c8.8%2c3.971h9.153a.669.669%2c0%2c0%2c0%2c.636-.51.524.524%2c0%2c0%2c0-.357-.646c-5.5-1.54-8.5-6.814-7.289-12.828l2.452-12.2h28.345a6.037%2c6.037%2c0%2c0%2c1%2c4.8%2c2.142.554.554%2c0%2c0%2c0%2c.613.155.678.678%2c0%2c0%2c0%2c.444-.524%2c11.351%2c11.351%2c0%2c0%2c0-2.329-9.254%2c11.031%2c11.031%2c0%2c0%2c0-8.637-3.778H391.83l2.043-10.165a4.253%2c4.253%2c0%2c0%2c1%2c3.973-3.308h23.9c6.6%2c0%2c11.3%2c4.71%2c11.163%2c11.2a.515.515%2c0%2c0%2c0%2c.524.538h0a.676.676%2c0%2c0%2c0%2c.643-.533l1.78-8.857a11.494%2c11.494%2c0%2c0%2c0-2.073-9.641A11%2c11%2c0%2c0%2c0%2c424.977%2c584Z' transform='translate(-373.818 -584)' fill='white'/%3e%3c/g%3e%3c/g%3e%3c/svg%3e"
    type="image/svg+xml">
<style>
    .header {
        background-color: #<? echo $settings['backgroundColor'] ?>;
    }
</style>
<?
}
?>

<?php
if (GetSettingValue('enableReadabilityFix')) {
?>
<script>
function hexToRgb(hex) {
    hex = hex.replace(/^#/, '');
    if (hex.length === 3) {
        hex = hex.split('').map(char => char + char).join('');
    }
    const num = parseInt(hex, 16);
    return { r: num >> 16, g: (num >> 8) & 255, b: num & 255 };
}

function getLuminance(rgb) {
    return (0.299 * rgb.r + 0.587 * rgb.g + 0.114 * rgb.b) / 255;
}

function adjustHeaderText() {
    const header = document.querySelector('.header');
    if (!header) return;

    let bgColor = window.getComputedStyle(header).backgroundColor;
    let rgb;
    if (bgColor.startsWith('rgb')) {
        const rgbStr = bgColor.match(/\d+/g);
        rgb = { r: parseInt(rgbStr[0]), g: parseInt(rgbStr[1]), b: parseInt(rgbStr[2]) };
    } else if (bgColor.startsWith('#')) {
        rgb = hexToRgb(bgColor);
    } else {
        return;
    }

    const luminance = getLuminance(rgb);
    const textElements = header.querySelectorAll('a, span, p, div, li, small, i');

    textElements.forEach(el => {
    if (luminance > 0.55) {
        el.style.color = 'black';
        el.style.textShadow = '1px 1px 1px rgba(255,255,255,0.7), -1px -1px 1px rgba(255,255,255,0.7), 1px -1px 1px rgba(255,255,255,0.7), -1px 1px 1px rgba(255,255,255,0.7)';  // Softer outline with blur and opacity
    } else {
        el.style.color = 'white';
        el.style.textShadow = 'none';
    }
});

    // Fix top left falcon logos (both falconlogo.svg with class="logo" and fpp-logo.svg with class="fpp-logo", excluding #platformLogo)
    const falconLogos = header.querySelectorAll('img.logo:not(#platformLogo), img.fpp-logo:not(#platformLogo)');
    falconLogos.forEach(logo => {
        if (luminance > 0.55) {
            logo.style.filter = 'invert(1)';  // Invert to black for light BG
        } else {
            logo.style.filter = 'none';  // Reset for dark BG
        }
    });

    // Fix update available button icon (<i class="fa-solid fa-download">)
    const updateIcon = header.querySelector('#navbarUpdateAvailIcon');
    if (updateIcon) {
        updateIcon.style.color = luminance > 0.55 ? 'black' : 'white';
        updateIcon.style.textShadow = luminance > 0.55 ? '1px 1px 0 white, -1px -1px 0 white, 1px -1px 0 white, -1px 1px 0 white' : 'none';
    }

}

window.addEventListener('load', adjustHeaderText);
</script>
<?php } ?>