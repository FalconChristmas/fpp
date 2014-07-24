
	<!-- jquery -->
	<script type="text/javascript" src="js/jquery-1.9.1.js"></script>
	<script type="text/javascript" src="js/jquery-ui.js"></script>
	<script type="text/javascript" src="js/jquery.jgrowl.min.js"></script>
	<link rel="stylesheet" href="css/jquery-ui.css" />
	<link rel="stylesheet" href="css/jquery.jgrowl.min.css" />
	<link rel="stylesheet" href="css/classicTheme/style.css" media="all" />

	<!-- menu -->
	<script type="text/javascript" src="js/ddsmoothmenu.js"></script>
	<link rel="stylesheet" href="css/ddsmoothmenu.css" />
	<link rel="stylesheet" href="css/ddsmoothmenu-v.css" />

	<!-- finally FPP stuff, here so our CSS is last so we can override anything above -->
	<meta name="viewport" content="width=device-width, initial-scale=1.0">
	<link rel="stylesheet" href="css/fpp.css" />
	<link rel="stylesheet" media="screen and (max-device-width: 480px)" href="../css/fpp_480px.css" />
	<script type="text/javascript" src="js/fpp.js"></script>

	<script type="text/javascript">
		ddsmoothmenu.init({
			mainmenuid: "fppMenu", //menu DIV id
			orientation: 'h', //Horizontal or vertical menu: Set to "h" or "v"
			classname: 'ddsmoothmenu', //class added to menu's outer DIV
			//customtheme: ["#1c5a80", "#18374a"],
			contentsource: "markup" //"markup" or ["container_id", "path_to_menu_file"]
		})
	</script>
