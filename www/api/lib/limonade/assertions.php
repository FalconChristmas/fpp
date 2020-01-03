<?php
/**
 * @package tests
 * @subpackage assertions
 */
# ============================================================================ #
#    ASSERTIONS                                                                #
# ============================================================================ #

/**
 * assert_true
 *
 * @param string $value 
 * @param string $message 
 * @return boolean
 */
function assert_true($value, $message = '<1> should be TRUE')
{
   test_run_assertion();
   return assert('$value === TRUE; //'.$message);
}

function assert_false($value, $message = '<1> should be FALSE')
{
   test_run_assertion();
   return assert('$value === FALSE; //'.$message);
}

function assert_null($value, $message = '<1> should be NULL')
{
   test_run_assertion();
   return assert('$value === NULL; //'.$message);
}

function assert_not_null($value, $message = '<1> should not be NULL')
{
   test_run_assertion();
   return assert('$value !== NULL; //'.$message);
}

function assert_empty($value, $message = '<1> should be empty')
{
   test_run_assertion();
   return assert('empty($value); //'.$message);
}

function assert_not_empty($value, $message = '<1> should not be empty')
{
   test_run_assertion();
   return assert('!empty($value); //'.$message);
}

function assert_equal($expected, $value, $message = '<1> should be equal to <2>')
{
   test_run_assertion();
   return assert('$expected == $value; //'.$message);
}

function assert_not_equal($expected, $value, $message = '<1> should not equal to <2>')
{
   test_run_assertion();
   return assert('$expected != $value; //'.$message);
}

function assert_identical($expected, $value, $message = '<1> should be identical to <2>')
{
   test_run_assertion();
   return assert('$expected === $value; //'.$message);
}

function assert_not_identical($expected, $value, $message = '<1> should not be identical to <2>')
{
   test_run_assertion();
   return assert('$expected !== $value; //'.$message);
}

function assert_match($pattern, $string, $message = '<2> expected to match regular expression <1>') {
   test_run_assertion();
   return assert('preg_match($pattern, $string); //'.$message);
}
 
function assert_no_match($pattern, $string, $message = '<2> expected to not match regular expression <1>') {
   test_run_assertion();
   return assert('!preg_match($pattern, $string); //'.$message);
}

function assert_type($type, $value, $message = '<1> is not of type <2>') {
  test_run_assertion();
  $predicate = 'is_' . strtolower(is_string($type) ? $type : gettype($type));
  return assert('$predicate($value); //'.$message);
}
 
function assert_instance_of($class, $object, $message = '<2> is not an instance of class <1>') {
   test_run_assertion();
   return assert('$object instanceof $class; //'.$message);
}
 
function assert_length_of($value, $length, $message = '<1> expected to be of length <2>') {
   test_run_assertion();
   $count = is_string($value) ? 'strlen' : 'count';
   return assert('$count($value) == $length; //'.$message);
}

function assert_trigger_error($callable, $args = array(), $message = '<1> should trigger an error') {
  test_run_assertion();
  $trigger_errors = count($GLOBALS["limonade"]["test_errors"]);
  set_error_handler("test_error_handler");
  $result = call_user_func_array($callable, $args);
  restore_error_handler();
  return assert('$trigger_errors < count($GLOBALS["limonade"]["test_errors"]); //'.$message);
}

# TODO add web browser assertions assert_http_get, assert_http_response... as in SimpleTest (http://www.simpletest.org/en/web_tester_documentation.html)

function assert_header($response, $expected_name, $expected_value = null, $message = "expected header '%s' to be equal to '%s' but received '%s: %s'")
{
  test_run_assertion();
  # see assert_header in http://github.com/fnando/voodoo-test/blob/f3b0994ef138a6ba94d5e7cef6c1fb1720797a86/lib/assertions.php
  $headers = preg_split("/^\s*$/ms", $response);
  //var_dump($headers);    
  $headers = preg_replace("/\s*$/sm", "", $headers[0]);
  //var_dump($headers);   
  
  $regex_header = str_replace("/", "\\/", $expected_name);
  $regex_header = str_replace(".", "\\.", $regex_header);
  
  $header = $expected_name;
  
  # from http://www.faqs.org/rfcs/rfc2616
  # Field names are case-insensitive
  if ($expected_value) {
      $regex = "/^{$regex_header}:(.*?)$/ism";
      $header .= ": {$expected_value}";
  } else {
      $regex = "/^{$regex_header}(:.*?)?$/ism";
  }
  
  $has_header = preg_match($regex, $headers, $matches);    
  $sent_header = trim((string)$matches[1]);

  
  if(empty($sent_header))
  {
    if(is_null($expected_value))
    {
      $message = "expected header '%s' but header has not been sent";
    }
    else
    {
      $message = "expected header '%s' to be equal to '%s' but header has not been sent";
    }
    
    $message = sprintf($message, $expected_name, $expected_value);
    return assert("false; //".$message);
  }
  else if($expected_value)
  {
    $message = sprintf($message, $expected_name, $expected_value, $expected_name, $sent_header);
    return assert('$expected_value && $sent_header == $expected_value; //'.$message);
  }
  return assert("true; //");
}

function assert_status($response, $expected_status, $message = "expected status code to be equal to '%s' but received '%s'")
{
  $lines = explode('\n', trim($response));
  if (preg_match('/HTTP\/(\d+\.\d+)\s+(\d+)/i', $lines[0], $matches))
  {
      $status = $matches[2];
      return assert('$expected_status == $status; //'.sprintf($message, $expected_status, $status));
  }
  return assert("false; //no status code returned in this response string");
}
