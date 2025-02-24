<!DOCTYPE html>
<html lang="en">

<head>
	<?php
	include 'common/htmlMeta.inc';
	require_once('config.php');
	require_once('common.php');

	$helpPages = array(
		'help/pixeloverlaymodels.php' => 'Real-Time Pixel Overlay Models'
	);

	include 'common/menuHead.inc'; ?>
	<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
	<title><? echo $pageTitle; ?></title>
</head>

<body>
	<div id="bodyWrapper">
		<?php
		$activeParentMenuItem = 'help';
		include 'menu.inc'; ?>
		<div class="mainContainer">
			<h1 class="title">Help</h1>
			<div class="pageContent">
				<div style="margin:0 auto;"> <br>
					<fieldset class="fieldSection">
						<legend>Get Help</legend>
						<div style="overflow: hidden; padding: 10px;">
                        <h4>Places to get help</h4>
                                    <a class="<?= $externalLinkClass ?> link-to-fpp-manual"
                                    href="https://added-by-javascript.net" target="_blank" rel="noopener noreferrer"><i
                                        class="fas fa-book"></i>
                                    FPP has an extensive manual - FPP Manual <i class="fas fa-external-link-alt external-link"></i></a>
                                <br>
                                <a class="<?= $externalLinkClass ?>" href="https://www.facebook.com/groups/1554782254796752"
                                    target="_blank" rel="noopener noreferrer"><i class="fas fa-comments"></i> Got a question? Join the FPP Facebook group <i
                                        class="fas fa-external-link-alt external-link"></i></a>
                                <a class="<?= $externalLinkClass ?>" href="https://falconchristmas.com/forum"
                                    target="_blank" rel="noopener noreferrer"><i class="fas fa-comments"></i> or the FPP Forums <i
                                        class="fas fa-external-link-alt external-link"></i></a>
                                        <br>
                                <a class="<?= $externalLinkClass ?>"
                                    href="https://xlights.org/zoom/" target="_blank"
                                    rel="noopener noreferrer"><i class="fas fa-headphones"></i> Prefer to talk? Join the xLights Zoom room <i
                                        class="fas fa-external-link-alt external-link"></i></a>
                                        <br>
                                <a class="<?= $externalLinkClass ?>"
                                    href="https://github.com/FalconChristmas/fpp/issues" target="_blank"
                                    rel="noopener noreferrer"><i class="fas fa-bug"></i> Need to log a bug? - Issue Tracker <i
                                        class="fas fa-external-link-alt external-link"></i></a>
                                <br><br><h4>FPP API</h4>
                                <a class="dropdown-item" href="apihelp.php"><i class="fas fa-plug"></i> REST API
                                    Help</a>
                                <a class="dropdown-item" href="commandhelp.php"><i class="fas fa-cubes"></i> FPP
                                    Commands Help</a>

							<div>
                            <br><br><h4>Specific Topics</h4>
								<ul>
									<li><a href='javascript:void(0);'
											onClick="helpPage='help/backup.php'; DisplayHelp();">Backup & Restore</a>
									</li>
									<li><a href='javascript:void(0);'
											onClick="helpPage='help/channeloutputs.php'; DisplayHelp();">Channel
											Outputs</a></li>
									<li><a href='javascript:void(0);'
											onClick="helpPage='help/gpio.php'; DisplayHelp();">GPIO Input Triggers</a>
									</li>
									<li><a href='javascript:void(0);'
											onClick="helpPage='help/networkconfig.php'; DisplayHelp();">Network
											Config</a></li>
									<li><a href='javascript:void(0);'
											onClick="helpPage='help/outputprocessors.php'; DisplayHelp();">Output
											Processors</a></li>
									<li><a href='javascript:void(0);'
											onClick="helpPage='help/pixeloverlaymodels.php'; DisplayHelp();">Real-Time Pixel
											Overlay Models</a></li>
									<li><a href='javascript:void(0);'
											onClick="helpPage='help/scriptbrowser.php'; DisplayHelp();">Script
											Repository Browser</a></li>
								</ul>
							</div>
					</fieldset>
				</div>
			</div>
		</div>
		<?php include 'common/footer.inc'; ?>
	</div>
</body>

</html>
