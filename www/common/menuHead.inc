<?
// Switch to Initial Setup page if needed, but come back here when done
if (((!isset($settings['initialSetup-02'])) || ($settings['initialSetup-02'] == '0')) && (basename($_SERVER['PHP_SELF']) != "initialSetup.php")) {
    echo "<meta http-equiv='refresh' content='0;url=initialSetup.php?redirect=" . preg_replace('/^\//', '', $_SERVER['PHP_SELF']) . "' />\n";
    exit(0);
}
?>

	<!-- jquery -->
	<script type="text/javascript" src="js/jquery-latest.min.js?ref=<?= filemtime('js/jquery-latest.min.js'); ?>"></script>
	<script type="text/javascript" src="js/jquery-ui.min.js"></script>
	<script type="text/javascript" src="js/jquery.ui.touch-punch.js"></script>
	<script type="text/javascript" src="js/jquery.jgrowl.min.js"></script>
	<script type="text/javascript" src="js/zebra_pin.min.js"></script>
	<script type="text/javascript" src="js/bootstrap.bundle.min.js?ref=<?= filemtime('js/bootstrap.bundle.min.js'); ?>"></script>

	<link rel="stylesheet" href="css/jquery-ui.css" />
	<link rel="stylesheet" href="css/jquery.jgrowl.min.css" />
	<link rel="stylesheet" href="css/classicTheme/style.css" media="all" />
 
	<link rel="icon" href="favicon.ico" type="image/x-icon">

    <!--
		Font Awesome Free 5.15.2 by @fontawesome - https://fontawesome.com
		License - https://fontawesome.com/license/free (Icons: CC BY 4.0, Fonts: SIL OFL 1.1, Code: MIT License)
    -->
	<link rel="stylesheet" href="css/fontawesome.all.min.css" />
	<link rel="stylesheet" href="webfonts/fpp-icons/styles.css" />
 
 <?
    if (isset($themeInfo["preFPPStyleSheet"])) {
        foreach($themeInfo["preFPPStyleSheet"] as $ss) {
            if ($ss != "") {
                echo "<link rel='stylesheet' href='" . $ss . "' />";
            }
        }
    }
?>


    <!-- finally FPP stuff, here so our CSS is last so we can override anything above -->
	<meta name="viewport" content="width=device-width, initial-scale=1.0">
	<link rel="stylesheet" href="css/fpp-bootstrap/dist/fpp-bootstrap.css?ref=<?php echo filemtime('css/fpp-bootstrap/dist/fpp-bootstrap.css'); ?>" />
	<link rel="stylesheet" href="css/fpp.css?ref=<?php echo filemtime('css/fpp.css'); ?>" />

<?
    if (isset($themeInfo["postFPPStyleSheet"])) {
        foreach($themeInfo["postFPPStyleSheet"] as $ss) {
            if ($ss != "") {
                echo "<link rel='stylesheet' href='" . $ss . "' />";
            }
        }
    }
?>

<? if (file_exists($mediaDirectory . "/config/custom.css")) { ?>
    <link rel="stylesheet" href="api/configfile/custom.css?ref=<?php echo filemtime($mediaDirectory . '/config/custom.css'); ?>" />
<? } ?>
    <script type="text/javascript" src="js/fpp.js?ref=<?php echo filemtime('js/fpp.js'); ?>"></script>
<? if (file_exists($mediaDirectory . "/config/custom.js")) { ?>
    <script type="text/javascript" src="api/configfile/custom.js?ref=<?php echo filemtime($mediaDirectory . '/config/custom.js'); ?>"></script>
<? } ?>

<?
    if (isset($themeInfo["themeJS"])) {
        foreach($themeInfo["themeJS"] as $js) {
            if ($js != "") {
                echo "<script type='text/javascript' src='" . $js . "' />";
            }
        }
    }
	if (isset($settings['backgroundColor']) && $settings['backgroundColor'] != "")
	{
?>
		<link rel="icon" href="data:image/svg+xml,%3csvg xmlns='http://www.w3.org/2000/svg' width='96' height='96' viewBox='0 0 96 96'%3e%3cg id='app-icon' transform='translate(-1977.763 -385.763)'%3e%3crect id='Rectangle_79' data-name='Rectangle 79' width='96' height='96' rx='25' transform='translate(1977.763 385.763)' fill='%23<? echo $settings['backgroundColor'] ?>'/%3e%3cg id='Group_124' data-name='Group 124' transform='translate(1994.608 402.675)'%3e%3cpath id='Path_23' data-name='Path 23' d='M424.977%2c584H397.358c-7.446%2c0-14.721%2c6.056-16.216%2c13.5l-7.047%2c35.063a11.489%2c11.489%2c0%2c0%2c0%2c2.073%2c9.641%2c11%2c11%2c0%2c0%2c0%2c8.8%2c3.971h9.153a.669.669%2c0%2c0%2c0%2c.636-.51.524.524%2c0%2c0%2c0-.357-.646c-5.5-1.54-8.5-6.814-7.289-12.828l2.452-12.2h28.345a6.037%2c6.037%2c0%2c0%2c1%2c4.8%2c2.142.554.554%2c0%2c0%2c0%2c.613.155.678.678%2c0%2c0%2c0%2c.444-.524%2c11.351%2c11.351%2c0%2c0%2c0-2.329-9.254%2c11.031%2c11.031%2c0%2c0%2c0-8.637-3.778H391.83l2.043-10.165a4.253%2c4.253%2c0%2c0%2c1%2c3.973-3.308h23.9c6.6%2c0%2c11.3%2c4.71%2c11.163%2c11.2a.515.515%2c0%2c0%2c0%2c.524.538h0a.676.676%2c0%2c0%2c0%2c.643-.533l1.78-8.857a11.494%2c11.494%2c0%2c0%2c0-2.073-9.641A11%2c11%2c0%2c0%2c0%2c424.977%2c584Z' transform='translate(-373.818 -584)' fill='white'/%3e%3c/g%3e%3c/g%3e%3c/svg%3e" type="image/svg+xml" />
		<style>
			.header {
				background-color: #<? echo $settings['backgroundColor'] ?>;
			}
		</style>
<?
	}
?>
