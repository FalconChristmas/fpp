
	<!-- jquery -->
	<script type="text/javascript" src="js/jquery-latest.min.js"></script>
	<script type="text/javascript" src="js/jquery-ui.min.js"></script>
	<script type="text/javascript" src="js/jquery.ui.touch-punch.js"></script>
	<script type="text/javascript" src="js/jquery.jgrowl.min.js"></script>
	<script type="text/javascript" src="js/zebra_pin.min.js"></script>
	<script type="text/javascript" src="js/bootstrap.bundle.min.js"></script>

	<link rel="stylesheet" href="css/jquery-ui.css" />
	<link rel="stylesheet" href="css/jquery.jgrowl.min.css" />
	<link rel="stylesheet" href="css/classicTheme/style.css" media="all" />
 
    <!--
		Font Awesome Free 5.15.2 by @fontawesome - https://fontawesome.com
		License - https://fontawesome.com/license/free (Icons: CC BY 4.0, Fonts: SIL OFL 1.1, Code: MIT License)
    -->
	<link rel="stylesheet" href="css/fontawesome.all.min.css" />
	<link rel="stylesheet" href="webfonts/fpp-icons/styles.css" />

    <!-- finally FPP stuff, here so our CSS is last so we can override anything above -->
	<meta name="viewport" content="width=device-width, initial-scale=1.0">
	<link rel="stylesheet" href="css/fpp-bootstrap/dist/fpp-bootstrap.css?ref=<?php echo filemtime('css/fpp-bootstrap/dist/fpp-bootstrap.css'); ?>" />
	<link rel="stylesheet" href="css/fpp.css?ref=<?php echo filemtime('css/fpp.css'); ?>" />

<? if (file_exists("/home/fpp/media/config/custom.css")) { ?>
    <link rel="stylesheet" href="api/configfile/custom.css?ref=<?php echo filemtime('/home/fpp/media/config/custom.css'); ?>" />
<? } ?>
    <script type="text/javascript" src="js/fpp.js?ref=<?php echo filemtime('js/fpp.js'); ?>"></script>
<? if (file_exists("/home/fpp/media/config/custom.js")) { ?>
    <script type="text/javascript" src="api/configfile/custom.js?ref=<?php echo filemtime('/home/fpp/media/config/custom.js'); ?>"></script>
<? } ?>

	<script type="text/javascript">
	var bootstrapTooltip = $.fn.tooltip.noConflict() // return $.fn.button to previously assigned value
	$.fn.bootstrapTooltip = bootstrapTooltip // give $().bootstrapBtn the Bootstrap functionality
	</script>

<?
	if (isset($settings['backgroundColor']) && $settings['backgroundColor'] != "")
	{
?>
		<style>
			.header {
				background-color: #<? echo $settings['backgroundColor'] ?>;
			}
		</style>
<?
	}
?>
