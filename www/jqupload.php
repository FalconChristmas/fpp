<?php
//////////////////////////////////////////////////////////////////////////////
// This file is based on upload.php from jQuery-Upload-File.  It has been
// modified slightly to fit FPP usage requirements.
//
// jQuery-Upload-File is covered by the MIT license, so this file falls under
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
require_once('common.php');

$output_dir = $uploadDirectory . "/";

// jqupload.php is the multisync file-push transport, and MoveFile() routes
// many types onward (sequences, media, images, and event scripts -- including
// .sh/.pl/.pm/.php/.py into scriptDirectory, which runEventScript executes by
// design), so extensions cannot be allow-listed here without breaking core
// flows. Instead refuse the server-executable leftovers that have no FPP use
// at all: PHP would run them if they ever landed under the document root, and
// nothing in FPP (MoveFile routing, plugins, scripts) consumes them.
// Comparison is on the sanitized name, case-insensitive, matching both the
// full basename (".htaccess" has no pathinfo extension) and the extension.
function isUploadBlockedExtension($fileName)
{
    static $blocked = array(
        'htaccess', 'htpasswd', 'user.ini',
        'phtml', 'phar', 'cgi', 'fcgi', 'shtml', 'shtm', 'stm'
    );
    if (!is_string($fileName) || $fileName === '') {
        return true;
    }
    $lower = strtolower($fileName);
    if (in_array($lower, $blocked, true)) {
        return true;
    }
    $ext = pathinfo($lower, PATHINFO_EXTENSION);
    if (in_array($ext, $blocked, true)) {
        return true;
    }
    // php5/php7/... variants (plain "php" stays allowed: event scripts).
    if (preg_match('/^php[0-9]+$/', $ext)) {
        return true;
    }
    return false;
}

// sanitizeFilename() expects a string (array input would fatal); coerce
// anything else to an empty name, which isUploadBlockedExtension() rejects.
function sanitizeUploadFilename($rawName)
{
    if (!is_string($rawName)) {
        return '';
    }
    return sanitizeFilename($rawName);
}

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
  	 	$fileName = sanitizeUploadFilename($_FILES["myfile"]["name"]);
        if (!isUploadBlockedExtension($fileName)) {
            move_uploaded_file($_FILES["myfile"]["tmp_name"],$output_dir.$fileName);
     	    $ret[]= $fileName;
        } else {
            // jQuery-Upload-File's native per-file error contract (see the
            // commented sample above); remotePush surfaces the text and its
            // follow-up move then fails visibly instead of finalizing malware.
            $ret[]= array("jquery-upload-file-error" => "File type not allowed");
        }
 	}
	else  //Multiple files, file[]
	{
	  $fileCount = count($_FILES["myfile"]["name"]);
	  for($i=0; $i < $fileCount; $i++)
	  {
	  	$fileName = sanitizeUploadFilename($_FILES["myfile"]["name"][$i]);
        if (!isUploadBlockedExtension($fileName)) {
            move_uploaded_file($_FILES["myfile"]["tmp_name"][$i],$output_dir.$fileName);
     	    $ret[]= $fileName;
        } else {
     	    $ret[]= array("jquery-upload-file-error" => "File type not allowed");
        }
 	  }
	
	}
    echo json_encode($ret);
 }
?>
