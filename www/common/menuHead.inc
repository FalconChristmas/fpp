
	<!-- jquery -->
	<script type="text/javascript" src="js/jquery-latest.min.js"></script>
	<script type="text/javascript" src="js/jquery-ui.min.js"></script>
	<script type="text/javascript" src="js/jquery.ui.touch-punch.js"></script>
	<script type="text/javascript" src="js/jquery.jgrowl.min.js"></script>
	<link rel="stylesheet" href="css/jquery-ui.css" />
	<link rel="stylesheet" href="css/jquery.jgrowl.min.css" />
	<link rel="stylesheet" href="css/classicTheme/style.css" media="all" />
 
 
    <!--
       Font Awesome Free 5.13.0 by @fontawesome - https://fontawesome.com
       License - https://fontawesome.com/license/free (Icons: CC BY 4.0, Fonts: SIL OFL 1.1, Code: MIT License)
    -->
    <script type="text/javascript" src="js/fontawesome.min.js"></script>
    <script type="text/javascript" src="js/fontawesome-solid.min.js"></script>
    <link rel="stylesheet" href="css/fontawesome-solid.min.css" />

	<!-- finally FPP stuff, here so our CSS is last so we can override anything above -->
	<meta name="viewport" content="width=device-width, initial-scale=1.0">
	<link rel="stylesheet" href="css/fpp.css?ref=<?php echo filemtime('css/fpp.css'); ?>" />
<? if (file_exists("/home/fpp/media/config/custom.css")) { ?>
    <link rel="stylesheet" href="api/configfile/custom.css?ref=<?php echo filemtime('/home/fpp/media/config/custom.css'); ?>" />
<? } ?>
    <script type="text/javascript" src="js/fpp.js?ref=<?php echo filemtime('js/fpp.js'); ?>"></script>
<? if (file_exists("/home/fpp/media/config/custom.js")) { ?>
    <script type="text/javascript" src="api/configfile/custom.js?ref=<?php echo filemtime('/home/fpp/media/config/custom.js'); ?>"></script>
<? } ?>

	<!-- menu -->
	<link href="jquery/smartmenus/src/css/sm-core-css.css" rel="stylesheet" type="text/css" />
	<link href="jquery/smartmenus/src/css/sm-clean/sm-clean.css" rel="stylesheet" type="text/css" />
	<script type="text/javascript" src="jquery/smartmenus/src/jquery.smartmenus.js"></script>

	<script type="text/javascript">
	$(function() {
		$('#fppMenu').smartmenus({subMenusMinWidth: "50%", showOnClick: true,
			showFunction: function($ul, complete) { $ul.slideDown(250, complete); },
			hideFunction: function($ul, complete) { $ul.slideUp(250, complete); },
		});
	});
	</script>

<?
	if (isset($settings['backgroundColor']) && $settings['backgroundColor'] != "")
	{
?>
		<style>
			#bodyWrapper {
				border: 50px;
				border-style: solid;
				border-color: #<? echo $settings['backgroundColor'] ?>;
			}
		</style>
<?
	}
?>
