<?php
/**
 * @package tests
 */
 
# ============================================================================ #
#    TESTS                                                                     #
# ============================================================================ #

/**
 * load assertions
 */
require_once dirname(__FILE__)."/assertions.php";


 
/**
 * Constants and globals
 */
if(!defined('DS')) define("DS", DIRECTORY_SEPARATOR);

if(!array_key_exists("limonade", $GLOBALS))
   $GLOBALS["limonade"] = array();
if(!array_key_exists("test_cases", $GLOBALS["limonade"]))
   $GLOBALS["limonade"]["test_cases"] = array();
if(!array_key_exists("test_errors", $GLOBALS["limonade"]))
   $GLOBALS["limonade"]["test_errors"] = array();
if(!array_key_exists("test_case_current", $GLOBALS["limonade"]))
   $GLOBALS["limonade"]["test_case_current"] = NULL;
if(!array_key_exists("test_suites", $GLOBALS["limonade"]))
   $GLOBALS["limonade"]["test_suites"] = NULL;

ini_set("display_errors", true);
error_reporting(E_ALL ^ (E_USER_WARNING | E_NOTICE | E_USER_NOTICE));
// error_reporting(E_ALL | E_STRICT);
assert_options(ASSERT_ACTIVE, 1);
assert_options(ASSERT_WARNING, 0);
assert_options(ASSERT_BAIL, 0);
assert_options(ASSERT_QUIET_EVAL, 0);
assert_options(ASSERT_CALLBACK, 'test_assert_failure');

# TODO: separate display from logic
# TODO: clean results output
# TODO: add all tests results

/**
 * Starts a test suite
 *
 * @param string $name 
 * @return void
 */
function test_suite($name)
{
  $GLOBALS["limonade"]["test_suites"] = $name;
  echo test_cli_format("===========================================================\n", 'white');
  echo test_cli_format(">>>> START $name tests suites\n", 'white');
  echo test_cli_format("-----------------------------------------------------------\n", 'white');
}

/**
 * Ends the last group of test suites
 *
 * @return void
 */
function end_test_suite()
{
  $name         = $GLOBALS["limonade"]["test_suites"];
  $failures     = 0;
  $tests        = 0;
  $passed_tests = 0;
  $assertions   = 0;

  foreach($GLOBALS["limonade"]["test_cases"] as $test)
  {
    $failures += $test['failures'];
    $assertions += $test['assertions'];
    if(empty($test['failures'])) $passed_tests++;
    $tests++;
  }
  echo ">> ENDING $name tests suites\n  ";
  echo $failures > 0 ? test_cli_format("|FAILED!|", "red") : test_cli_format("|PASSED|", "green");;
  echo " Passes ".$passed_tests."/".$tests.", ";
  echo " {$failures} failures for {$assertions} assertions.\n";
  echo test_cli_format("===========================================================\n", 'white');
}

/**
 * Starting a new test case
 *
 * @param string $name 
 * @return void
 */
function test_case($name)
{
   $name = strtolower($name); // TODO: normalize name
   
   if(!array_key_exists($name, $GLOBALS["limonade"]["test_cases"]))
   {
      $GLOBALS["limonade"]["test_cases"][$name] = array( 
                                                      "name" => $name,
                                                      "assertions" => 0, 
                                                      "failures" => 0,
                                                      "description" => NULL
                                                   );      
      $GLOBALS["limonade"]["test_case_current"] = $name;
   }
   else
   {
      
   }
}

/**
 * Displays and ending the current tests suite
 * 
 * @return void
 */
function end_test_case()
{
   $name = $GLOBALS["limonade"]["test_case_current"];
   echo "## ".strtoupper($name)."\n";
      
   $desc = test_case_describe();
   if(!is_null($desc)) echo $desc."\n";

   echo "- - - - - - - - - - - - - - - - - - - - - - - - - - - - - -\n";
   
   test_case_execute_current();
   
   
   if(!is_null($name))
   {
      $test = $GLOBALS["limonade"]["test_cases"][$name];
      // closing previous test
      echo "\n- - - - - - - - - - - - - - - - - - - - - - - - - - - - - -\n";
      echo $test['failures'] > 0 ? test_cli_format("|FAILED!|", "red") : test_cli_format("|PASSED|", "green");
      echo " Test case '$name' finished: ";
      echo count(test_case_all_func())." tests, ";
      echo " {$test['failures']} failures for {$test['assertions']} assertions.\n";
      
      echo "-----------------------------------------------------------\n";
   }
   $GLOBALS["limonade"]["test_case_current"] = null;
}

/**
 * Describes the current tests suite
 *
 * @param string $msg 
 * @return string tests description
 */
function test_case_describe($msg = NULL)
{
   $test =& test_case_current();
   if(!is_null($msg))
   {
      $test["description"] = $msg;
   }
   //var_dump($test["description"]);
   return $test["description"];
}

/**
 * Returns all user test case functions
 *
 * @access private
 * @return void
 */
function test_case_all_func()
{
   $functions = get_defined_functions();
   $functions = $functions['user'];
   $tests = array();
   $name = $GLOBALS["limonade"]["test_case_current"];
   while ($func = array_shift($functions)) {
      $regexp = "/^test_{$name}_(.*)$/";
      if(!preg_match($regexp, $func)) continue;
      if($func == test_before_func_name()) continue;
      // TODO: adding break for all test api methods
      
      $tests[] = $func;
   }
   return $tests;
}

/**
 * Execute current test case
 *
 * @access private
 * @return void
 */
function test_case_execute_current()
{
   $tests = test_case_all_func();
   while($func = array_shift($tests))
   {
      test_call_func(test_before_func_name());
      call_user_func($func);
   }
}


function &test_case_current()
{
   $name = $GLOBALS["limonade"]["test_case_current"];
   return $GLOBALS["limonade"]["test_cases"][$name];
}



function test_before_func_name()
{
   $test = test_case_current();
   $func = "before_each_test_in_".$test["name"];
   return $func;
}

function test_before_assert_func_name()
{
   $test = test_case_current();
   $func = "before_each_assert_in_$name".$test["name"];
   return $func;
}

function test_run_assertion()
{
   $name = $GLOBALS["limonade"]["test_case_current"];
   $GLOBALS["limonade"]["test_cases"][$name]['assertions']++;
   test_call_func(test_before_assert_func_name());
}

/**
 * Calls a function if exists
 *
 * @param string $func the function name
 * @param mixed $arg,.. (optional)
 * @return mixed
 */
function test_call_func($func)
{
  if(empty($func)) return;
  $args = func_get_args();
  $func = array_shift($args);
  if(function_exists($func)) return call_user_func_array($func, $args);
  return;
}

/**
 * Error handler 
 * 
 * @access private
 * @return boolean true
 */
function test_error_handler($errno, $errstr, $errfile, $errline)
{
	if($errno < E_USER_ERROR || $errno > E_USER_NOTICE) 
	   echo test_cli_format("!!! ERROR", "red") . " [$errno], $errstr in $errfile at line $errline\n";
	$GLOBALS["limonade"]["test_errors"][] = array($errno, $errstr, $errfile, $errline);
   return true;
}

/**
 * Assert callback
 * 
 * @access private
 * @param string $script 
 * @param string $line 
 * @param string $message 
 * @return void
 */
function test_assert_failure($script, $line, $message)
{
   // Using the stack trace, find the outermost assert*() call
   $stacktrace = array_slice(debug_backtrace(), 1); // skip self
   $assertion = reset($stacktrace);
   while ($stackframe = array_shift($stacktrace)) {
    if (!preg_match('/^assert/', $stackframe['function']))
      break;
    $assertion = $stackframe;
   }

   extract($assertion, EXTR_PREFIX_ALL, 'assert');
   $code = explode("\n", file_get_contents($assert_file));
   $code = trim($code[$assert_line - 1]);
   
   list($assert_code, $message) = explode("//", $message);
   echo test_cli_format("Assertion failed", "yellow");
   echo " in script *{$assert_file}* (line {$assert_line}):\n";
   echo "   * assertion: $code\n";
   echo "   * message:   $message\n";
   $name = $GLOBALS["limonade"]["test_case_current"];
   $GLOBALS["limonade"]["test_cases"][$name]['failures']++;
}

function test_cli_format($text, $format) {
    $formats = array(
        "blue"       => 34,
        "bold"       => 1,
        "green"      => 32,
        "highlight"  => 7,
        "light_blue" => 36,
        "purple"     => 35,
        "red"        => 31,
        "underline"  => 4,
        "white"      => 37,
        "yellow"     => 33
    );

    if (array_key_exists($format, $formats)) $format = $formats[$format];
    return chr(27) . "[01;{$format} m{$text}" . chr(27) . "[00m";
}

/**
 * Do HTTP request and return the response content.
 * 
 * @param string $url
 * @param string $method
 * @param bool $include_header
 * @return string
 * @author Nando Vieira
 */
function test_request($url, $method="GET", $include_header=false, $post_data=array(), $http_header=array()) {
    $method = strtoupper($method);
    $allowed_methods = array("GET", "PUT", "POST", "DELETE", "HEAD");
    if(!in_array($method, $allowed_methods))
    {
      $message = "The requested method '$method' is not allowed";
      return assert('false; //'.$message);
    }
    
    $curl = curl_init($url);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, 1);
    curl_setopt($curl, CURLOPT_HEADER, $include_header);
    curl_setopt($curl, CURLOPT_HTTPHEADER, $http_header);
    curl_setopt($curl, CURLOPT_CUSTOMREQUEST, $method);
    curl_setopt($curl, CURLOPT_FOLLOWLOCATION, 1); 
    if($method == 'POST' || $method == 'PUT')
    {
      curl_setopt($curl, CURLOPT_POST, 1);
      curl_setopt($curl, CURLOPT_POSTFIELDS, $post_data);
    }
    if($method == 'HEAD')
    {
      curl_setopt($curl, CURLOPT_NOBODY, true);
    }
    $response = curl_exec($curl);
    curl_close($curl);

    return $response;
}
