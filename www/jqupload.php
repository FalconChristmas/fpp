<?php
//////////////////////////////////////////////////////////////////////////////
// This file is based on upload.php from jQuery-Upload-File.  It has been
// modified slightly to fit FPP usage requirements.
//
// jQuery-Uplaod-File is covered by the MIT license, so this file falls under
// that license instead of the GPL.  See
// fpp/www/jquery/jQuery-Upload-File/MIT-License.txt
//
// Here is the original copyright included in other jQuery-Upload-File sources:
//
//////////////////////////////////////////////////////////////////////////////
// jQuery Upload File Plugin
// version: 3.1.8
// @requires jQuery v1.5 or later & form plugin
// Copyright (c) 2013 Ravishanker Kusuma
// http://hayageek.com/
//////////////////////////////////////////////////////////////////////////////
$skipJSsettings = 1; // need this so config doesn't print out JavaScrip arrays
require_once('config.php');

$output_dir = $uploadDirectory . "/";

if(isset($_FILES["myfile"]))
{
	$ret = array();
	
//	This is for custom errors;	
/*	$custom_error= array();
	$custom_error['jquery-upload-file-error']="File already exists";
	echo json_encode($custom_error);
	die();
*/
	$error =$_FILES["myfile"]["error"];
	//You need to handle  both cases
	//If Any browser does not support serializing of multiple files using FormData() 
	if(!is_array($_FILES["myfile"]["name"])) //single file
	{
 	 	$fileName = $_FILES["myfile"]["name"];
 		move_uploaded_file($_FILES["myfile"]["tmp_name"],$output_dir.$fileName);
    	$ret[]= $fileName;
	}
	else  //Multiple files, file[]
	{
	  $fileCount = count($_FILES["myfile"]["name"]);
	  for($i=0; $i < $fileCount; $i++)
	  {
	  	$fileName = $_FILES["myfile"]["name"][$i];
		move_uploaded_file($_FILES["myfile"]["tmp_name"][$i],$output_dir.$fileName);
	  	$ret[]= $fileName;
	  }
	
	}
    echo json_encode($ret);
 }
?>
