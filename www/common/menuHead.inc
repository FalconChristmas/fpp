
	<!-- jquery -->
	<script type="text/javascript" src="js/jquery-1.9.1.js"></script>
	<script type="text/javascript" src="js/jquery-ui.js"></script>
	<script type="text/javascript" src="js/jquery.ui.touch-punch.min.js"></script>
	<script type="text/javascript" src="js/jquery.jgrowl.min.js"></script>
	<link rel="stylesheet" href="css/jquery-ui.css" />
	<link rel="stylesheet" href="css/jquery.jgrowl.min.css" />
	<link rel="stylesheet" href="css/classicTheme/style.css" media="all" />

	<!-- finally FPP stuff, here so our CSS is last so we can override anything above -->
	<meta name="viewport" content="width=device-width, initial-scale=1.0">
	<link rel="stylesheet" href="css/fpp.css" />
	<script type="text/javascript" src="js/fpp.js?ref=<?php echo filemtime('js/fpp.js'); ?>"></script>

	<!-- menu -->
	<link href="jquery/smartmenus/src/css/sm-core-css.css" rel="stylesheet" type="text/css" />
	<link href="jquery/smartmenus/src/css/sm-clean/sm-clean.css" rel="stylesheet" type="text/css" />
	<script type="text/javascript" src="jquery/smartmenus/src/jquery.smartmenus.js"></script>

	<script type="text/javascript">
	$(function() {
		$('#fppMenu').smartmenus({subMenusMinWidth: "100%", showOnClick: true,
			showFunction: function($ul, complete) { $ul.slideDown(250, complete); },
			hideFunction: function($ul, complete) { $ul.slideUp(250, complete); },
		});
	});
	</script>

<?
	if (isset($settings['backgroundColor']))
	{
?>
		<style>
			#bodyWrapper {
				background-color: #<? echo $settings['backgroundColor'] ?>;
			}
		</style>
<?
	}
?>
