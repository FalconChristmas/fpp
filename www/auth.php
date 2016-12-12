<?php

require_once("config.php");

$thisdir = dirname(__FILE__);
$pw = file_exists("$mediaDirectory/uipasswd");
$auth_success = FALSE;

if($pw){
	//auth is needed
	if (isset($_SERVER['PHP_AUTH_USER'])) {
		$hashstr = file_get_contents("$mediaDirectory/uipasswd");
		if($hashstr === FALSE){
			die("Password file exists but cannot be read!");
		}
		if($_SERVER['PHP_AUTH_USER'] == "admin" && password_verify($_SERVER['PHP_AUTH_PW'], $hashstr) === TRUE){
			$auth_success = TRUE;
		}
	}
	if($auth_success == FALSE){
		header('WWW-Authenticate: Basic realm="FPP"');
    		header('HTTP/1.0 401 Unauthorized');
    		echo 'error: user not authenticated';
    		exit;
	}
}
?>
