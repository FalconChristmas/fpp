<?php
                                                                  
# ============================================================================ #

/**
 *  L I M O N A D E
 * 
 *  a PHP micro framework.
 * 
 *  For more informations: {@link https://github.com/sofadesign/limonade}
 *  
 *  @author Fabrice Luraine
 *  @copyright Copyright (c) 2009 Fabrice Luraine
 *  @license http://opensource.org/licenses/mit-license.php The MIT License
 *  @package limonade
 */

#   -----------------------------------------------------------------------    #
#    Copyright (c) 2009 Fabrice Luraine                                        #
#                                                                              #
#    Permission is hereby granted, free of charge, to any person               #
#    obtaining a copy of this software and associated documentation            #
#    files (the "Software"), to deal in the Software without                   #
#    restriction, including without limitation the rights to use,              #
#    copy, modify, merge, publish, distribute, sublicense, and/or sell         #
#    copies of the Software, and to permit persons to whom the                 #
#    Software is furnished to do so, subject to the following                  #
#    conditions:                                                               #
#                                                                              #
#    The above copyright notice and this permission notice shall be            #
#    included in all copies or substantial portions of the Software.           #
#                                                                              #
#    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,           #
#    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES           #
#    OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND                  #
#    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT               #
#    HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,              #
#    WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING              #
#    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR             #
#    OTHER DEALINGS IN THE SOFTWARE.                                           #
# ============================================================================ # 








# ============================================================================ #
#    0. PREPARE                                                                #
# ============================================================================ #

## CONSTANTS __________________________________________________________________
/**
 * Limonade version
 */
define('LIMONADE',              '0.5.0');
define('LIM_NAME',              'Un grand cru qui sait se faire attendre');
define('LIM_START_MICROTIME',   microtime(true));
define('LIM_SESSION_NAME',      'LIMONADE'.str_replace('.','x',LIMONADE));
define('LIM_SESSION_FLASH_KEY', '_lim_flash_messages');
if(function_exists('memory_get_usage'))
	define('LIM_START_MEMORY',      memory_get_usage());
define('E_LIM_HTTP',            32768);
define('E_LIM_PHP',             65536);
define('E_LIM_DEPRECATED',      35000);
define('NOT_FOUND',             404);
define('SERVER_ERROR',          500);
define('ENV_PRODUCTION',        10);
define('ENV_DEVELOPMENT',       100);
define('X-SENDFILE',            10);
define('X-LIGHTTPD-SEND-FILE',  20);

# for PHP 5.3.0 <
if(!defined('E_DEPRECATED'))         define('E_DEPRECATED',        8192);
if(!defined('E_USER_DEPRECATED'))    define('E_USER_DEPRECATED',   16384);
# for PHP 5.2.0 <
if (!defined('E_RECOVERABLE_ERROR')) define('E_RECOVERABLE_ERROR', 4096);


## SETTING BASIC SECURITY _____________________________________________________

# A. Unsets all global variables set from a superglobal array

/**
 * @access private
 * @return void
 */
function unregister_globals()
{
  $args = func_get_args();
  foreach($args as $k => $v)
    if(array_key_exists($k, $GLOBALS)) unset($GLOBALS[$k]);
}

if(ini_get('register_globals'))
{
  unregister_globals( '_POST', '_GET', '_COOKIE', '_REQUEST', '_SERVER', 
                      '_ENV', '_FILES');
  ini_set('register_globals', 0);
}

# B. removing magic quotes

/**
 * @access private
 * @param string $array 
 * @return array
 */
function remove_magic_quotes($array)
{
  foreach ($array as $k => $v)
    $array[$k] = is_array($v) ? remove_magic_quotes($v) : stripslashes($v);
  return $array;
}

if (get_magic_quotes_gpc())
{
  $_GET    = remove_magic_quotes($_GET);
  $_POST   = remove_magic_quotes($_POST);
  $_COOKIE = remove_magic_quotes($_COOKIE);
  ini_set('magic_quotes_gpc', 0);
}

if(function_exists('set_magic_quotes_runtime') && get_magic_quotes_runtime()) set_magic_quotes_runtime(false);

# C. Disable error display
#    by default, no error reporting; it will be switched on later in run().
#    ini_set('display_errors', 1); must be called explicitly in app file
#    if you want to show errors before running app
ini_set('display_errors', 0);

## SETTING INTERNAL ROUTES _____________________________________________________

dispatch(array("/_lim_css/*.css", array('_lim_css_filename')), 'render_limonade_css');
  /**
   * Internal controller that responds to route /_lim_css/*.css
   *
   * @access private
   * @return string
   */
  function render_limonade_css()
  {
    option('views_dir', file_path(option('limonade_public_dir'), 'css'));
    $fpath = file_path(params('_lim_css_filename').".css");
    return css($fpath, null); // with no layout
  }

dispatch(array("/_lim_public/**", array('_lim_public_file')), 'render_limonade_file');
  /**
   * Internal controller that responds to route /_lim_public/**
   *
   * @access private
   * @return void
   */
  function render_limonade_file()
  {
    $fpath = file_path(option('limonade_public_dir'), params('_lim_public_file'));
    return render_file($fpath, true);
  }




                                     # # #




# ============================================================================ #
#    1. BASE                                                                   #
# ============================================================================ #
 
## ABSTRACTS ___________________________________________________________________

# Abstract methods that might be redefined by user:
#
# - function configure(){}
# - function initialize(){}
# - function autoload_controller($callback){}
# - function before($route){}
# - function after($output, $route){}
# - function not_found($errno, $errstr, $errfile=null, $errline=null){}
# - function server_error($errno, $errstr, $errfile=null, $errline=null){}
# - function route_missing($request_method, $request_uri){}
# - function before_exit(){}
# - function before_render($content_or_func, $layout, $locals, $view_path){}
# - function autorender($route){}
# - function before_sending_header($header){}
#
# See abstract.php for more details.


## MAIN PUBLIC FUNCTIONS _______________________________________________________

/**
 * Set and returns options values
 * 
 * If multiple values are provided, set $name option with an array of those values.
 * If there is only one value, set $name option with the provided $values
 *
 * @param string $name 
 * @param mixed  $values,... 
 * @return mixed option value for $name if $name argument is provided, else return all options
 */
function option($name = null, $values = null)
{
  static $options = array();
  $args = func_get_args();
  $name = array_shift($args);
  if(is_null($name)) return $options;
  if(!empty($args))
  {
    $options[$name] = count($args) > 1 ? $args : $args[0];
  }
  if(array_key_exists($name, $options)) return $options[$name];
  return;
}

/**
 * Set and returns params
 * 
 * Depending on provided arguments:
 * 
 *  * Reset params if first argument is null
 * 
 *  * If first argument is an array, merge it with current params
 * 
 *  * If there is a second argument $value, set param $name (first argument) with $value
 * <code>
 *  params('name', 'Doe') // set 'name' => 'Doe'
 * </code>
 *  * If there is more than 2 arguments, set param $name (first argument) value with
 *    an array of next arguments
 * <code>
 *  params('months', 'jan', 'feb', 'mar') // set 'month' => array('months', 'jan', 'feb', 'mar')
 * </code>
 * 
 * @param mixed $name_or_array_or_null could be null || array of params || name of a param (optional)
 * @param mixed $value,... for the $name param (optional)
 * @return mixed all params, or one if a first argument $name is provided
 */
function params($name_or_array_or_null = null, $value = null)
{
  static $params = array();
  $args = func_get_args();

  if(func_num_args() > 0)
  {
    $name = array_shift($args);
    if(is_null($name))
    {
      # Reset params
      $params = array();
      return $params;
    }
    if(is_array($name))
    {
      $params = array_merge($params, $name);
      return $params;
    }
    $nargs = count($args);
    if($nargs > 0)
    {
      $value = $nargs > 1 ? $args : $args[0];
      $params[$name] = $value;
    }
    return array_key_exists($name,$params) ? $params[$name] : null;
  }

  return $params;
}

/**
 * Set and returns template variables
 * 
 * If multiple values are provided, set $name variable with an array of those values.
 * If there is only one value, set $name variable with the provided $values
 *
 * @param string $name 
 * @param mixed  $values,... 
 * @return mixed variable value for $name if $name argument is provided, else return all variables
 */
function set($name = null, $values = null)
{
  static $vars = array();
  $args = func_get_args();
  $name = array_shift($args);
  if(is_null($name)) return $vars;
  if(!empty($args))
  {
    $vars[$name] = count($args) > 1 ? $args : $args[0];
  }
  if(array_key_exists($name, $vars)) return $vars[$name];
  return $vars;
}

/**
 * Sets a template variable with a value or a default value if value is empty
 *
 * @param string $name 
 * @param string $value 
 * @param string $default 
 * @return mixed setted value
 */
function set_or_default($name, $value, $default)
{
  return set($name, value_or_default($value, $default));
}

/**
 * Running application
 *
 * @param string $env 
 * @return void
 */
function run($env = null)
{
  if(is_null($env)) $env = env();
   
  # 0. Set default configuration
  $root_dir  = dirname(app_file());
  $lim_dir   = dirname(__FILE__);
  $base_path = dirname(file_path($env['SERVER']['SCRIPT_NAME']));
  $base_file = basename($env['SERVER']['SCRIPT_NAME']);
  $base_uri  = file_path($base_path, (($base_file == 'index.php') ? '?' : $base_file.'?'));
  
  option('root_dir',           $root_dir);
  option('limonade_dir',       file_path($lim_dir));
  option('limonade_views_dir', file_path($lim_dir, 'limonade', 'views'));
  option('limonade_public_dir',file_path($lim_dir, 'limonade', 'public'));
  option('public_dir',         file_path($root_dir, 'public'));
  option('views_dir',          file_path($root_dir, 'views'));
  option('controllers_dir',    file_path($root_dir, 'controllers'));
  option('lib_dir',            file_path($root_dir, 'lib'));
  option('error_views_dir',    option('limonade_views_dir'));
  option('base_path',          $base_path);
  option('base_uri',           $base_uri); // set it manually if you use url_rewriting
  option('env',                ENV_PRODUCTION);
  option('debug',              true);
  option('session',            LIM_SESSION_NAME); // true, false or the name of your session
  option('encoding',           'utf-8');
  option('signature',          LIM_NAME); // X-Limonade header value or false to hide it
  option('gzip',               false);
  option('x-sendfile',         0); // 0: disabled, 
                                   // X-SENDFILE: for Apache and Lighttpd v. >= 1.5,
                                   // X-LIGHTTPD-SEND-FILE: for Apache and Lighttpd v. < 1.5


  # 1. Set handlers
  # 1.1 Set error handling
  ini_set('display_errors', 1);
  set_error_handler('error_handler_dispatcher', E_ALL ^ E_NOTICE);
  
  # 1.2 Register shutdown function
  register_shutdown_function('stop_and_exit');

  # 2. Set user configuration
  call_if_exists('configure');
  
  # 2.1 Set gzip compression if defined
  if(is_bool(option('gzip')) && option('gzip'))
  {
    ini_set('zlib.output_compression', '1');
  }
  
  # 2.2 Set X-Limonade header
  if($signature = option('signature')) send_header("X-Limonade: $signature");

  # 3. Loading libs
  require_once_dir(option('lib_dir'));
  fallbacks_for_not_implemented_functions();

  # 4. Starting session
  if(!defined('SID') && option('session'))
  {
    if(!is_bool(option('session'))) session_name(option('session'));
    if(!session_start()) trigger_error("An error occured while trying to start the session", E_USER_WARNING);
  }

  # 5. Set some default methods if needed
  if(!function_exists('after'))
  {
    function after($output)
    {
      return $output;
    }
  }
  if(!function_exists('route_missing'))
  {
    function route_missing($request_method, $request_uri)
    {
      halt(NOT_FOUND, "($request_method) $request_uri");
    }
  }

  call_if_exists('initialize');

  # 6. Check request
  if($rm = request_method($env))
  {
    if(request_is_head($env)) ob_start(); // then no output

    if(!request_method_is_allowed($rm))
      halt(HTTP_NOT_IMPLEMENTED, "The requested method <code>'$rm'</code> is not implemented");

    # 6.1 Check matching route
    if($route = route_find($rm, request_uri($env)))
    {
      params($route['params']);

      # 6.2 Load controllers dir
      if(!function_exists('autoload_controller'))
      {
        function autoload_controller($callback)
        {
          require_once_dir(option('controllers_dir'));
        }
      }
      autoload_controller($route['callback']);

      if(is_callable($route['callback']))
      {
        # 6.3 Call before function
        call_if_exists('before', $route);

        # 6.4 Call matching controller function and output result
        $output = call_user_func_array($route['callback'], array_values($route['params']));
        if(is_null($output)) $output = call_if_exists('autorender', $route);
        echo after(error_notices_render() . $output, $route);
      }
      else halt(SERVER_ERROR, "Routing error: undefined function '{$route['callback']}'", $route);      
    }
    else route_missing($rm, request_uri($env));

  }
  else halt(HTTP_NOT_IMPLEMENTED, "The requested method <code>'$rm'</code> is not implemented");
}

/**
 * Stop and exit limonade application
 *
 * @access private 
 * @param boolean exit or not
 * @return void
 */
function stop_and_exit($exit = true)
{
  call_if_exists('before_exit', $exit);
  $headers = headers_list();
  if(request_is_head())
  { 
     ob_end_clean();
  } else {
    $flash_sweep = true;
    foreach($headers as $header)
    {
      // If a Content-Type header exists, flash_sweep only if is text/html
      // Else if there's no Content-Type header, flash_sweep by default
      if(stripos($header, 'Content-Type:') === 0)
      {
        $flash_sweep = stripos($header, 'Content-Type: text/html') === 0;
        break;
      }
    }
    if($flash_sweep) flash_sweep();
  }
  if(defined('SID')) session_write_close();
  if($exit) exit;
}

/**
 * Returns limonade environment variables:
 *
 * 'SERVER', 'FILES', 'REQUEST', 'SESSION', 'ENV', 'COOKIE', 
 * 'GET', 'POST', 'PUT', 'DELETE', 'PATCH'
 * 
 * If a null argument is passed, reset and rebuild environment
 *
 * @param null @reset reset and rebuild environment
 * @return array
 */
function env($reset = null)
{
  static $env = array();
  if(func_num_args() > 0)
  {
    $args = func_get_args();
    if(is_null($args[0])) $env = array();
  }

  if(empty($env))
  {
    if(empty($GLOBALS['_SERVER']))
    {
      // Fixing empty $GLOBALS['_SERVER'] bug 
      // http://sofadesign.lighthouseapp.com/projects/29612-limonade/tickets/29-env-is-empty
      $GLOBALS['_SERVER']  =& $_SERVER;
      $GLOBALS['_FILES']   =& $_FILES;
      $GLOBALS['_REQUEST'] =& $_REQUEST;
      $GLOBALS['_SESSION'] =& $_SESSION;
      $GLOBALS['_ENV']     =& $_ENV;
      $GLOBALS['_COOKIE']  =& $_COOKIE;
    }

    $glo_names = array('SERVER', 'FILES', 'REQUEST', 'SESSION', 'ENV', 'COOKIE');

    $vars = array_merge($glo_names, request_methods());
    foreach($vars as $var)
    {
      $varname = "_$var";
      if(!array_key_exists($varname, $GLOBALS)) $GLOBALS[$varname] = array();
      $env[$var] =& $GLOBALS[$varname];
    }

    $method = request_method($env);

    $varname = "_$method";
    if ((isset($_SERVER['CONTENT_TYPE'])) && (strpos($_SERVER['CONTENT_TYPE'], 'application/json') === 0))
    {
      // handle PUT/POST requests which have JSON in request body
      $GLOBALS[$varname] = json_decode(file_get_contents('php://input'), true);
    }
    elseif($method == 'PUT' || $method == 'DELETE')
    {
      if(array_key_exists('_method', $_POST) && $_POST['_method'] == $method)
      {
        foreach($_POST as $k => $v)
        {
          if($k == "_method") continue;
          $GLOBALS[$varname][$k] = $v;
        }
      }
      else
      {
        parse_str(file_get_contents('php://input'), $GLOBALS[$varname]);
      }
    }
  }
  return $env;
}

/**
 * Returns application root file path
 *
 * @return string
 */
function app_file()
{
  static $file;
  if(empty($file))
  {
    $debug_backtrace = debug_backtrace();
    $stacktrace = array_pop($debug_backtrace);
    $file = $stacktrace['file'];
  }
  return file_path($file);
}




                                     # # #




# ============================================================================ #
#    2. ERROR                                                                  #
# ============================================================================ #
 
/**
 * Associate a function with error code(s) and return all associations
 *
 * @param string $errno 
 * @param string $function 
 * @return array
 */
function error($errno = null, $function = null)
{
  static $errors = array();
  if(func_num_args() > 0)
  {
    $errors[] = array('errno'=>$errno, 'function'=> $function);
  }
  return $errors;
}

/**
 * Raise an error, passing a given error number and an optional message,
 * then exit.
 * Error number should be a HTTP status code or a php user error (E_USER...)
 * $errno and $msg arguments can be passsed in any order
 * If no arguments are passed, default $errno is SERVER_ERROR (500)
 *
 * @param int,string $errno Error number or message string
 * @param string,string $msg Message string or error number
 * @param mixed $debug_args extra data provided for debugging
 * @return void
 */
function halt($errno = SERVER_ERROR, $msg = '', $debug_args = null)
{
  $args = func_get_args();
  $error = array_shift($args);

  # switch $errno and $msg args
  # TODO cleanup / refactoring
  if(is_string($errno))
  {
   $msg = $errno;
   $oldmsg = array_shift($args);
   $errno = empty($oldmsg) ? SERVER_ERROR : $oldmsg;
  }
  else if(!empty($args)) $msg = array_shift($args);

  if(empty($msg) && $errno == NOT_FOUND) $msg = request_uri();
  if(empty($msg)) $msg = "";
  if(!empty($args)) $debug_args = $args;
  set('_lim_err_debug_args', $debug_args);

  error_handler_dispatcher($errno, $msg, null, null);

}

/**
 * Internal error handler dispatcher
 * Find and call matching error handler and exit
 * If no match found, call default error handler
 *
 * @access private
 * @param int $errno 
 * @param string $errstr 
 * @param string $errfile 
 * @param string $errline 
 * @return void
 */
function error_handler_dispatcher($errno, $errstr, $errfile, $errline)
{
  $back_trace = debug_backtrace();
  while($trace = array_shift($back_trace))
  {
    if($trace['function'] == 'halt')
    {
      $errfile = $trace['file'];
      $errline = $trace['line'];
      break;
    }
  }  

  # Notices and warning won't halt execution
  if(error_wont_halt_app($errno))
  {
    error_notice($errno, $errstr, $errfile, $errline);
  	return;
  }
  else
  {
    # Other errors will stop application
    static $handlers = array();
    if(empty($handlers))
    {
      error(E_LIM_PHP, 'error_default_handler');
      $handlers = error();
    }
    
    $is_http_err = http_response_status_is_valid($errno);
    while($handler = array_shift($handlers))
    {
      $e = is_array($handler['errno']) ? $handler['errno'] : array($handler['errno']);
      while($ee = array_shift($e))
      {
        if($ee == $errno || $ee == E_LIM_PHP || ($ee == E_LIM_HTTP && $is_http_err))
        {
          echo call_if_exists($handler['function'], $errno, $errstr, $errfile, $errline);
          exit;
        }
      }
    }
  }
}


/**
 * Default error handler
 *
 * @param string $errno 
 * @param string $errstr 
 * @param string $errfile 
 * @param string $errline 
 * @return string error output
 */
function error_default_handler($errno, $errstr, $errfile, $errline)
{
  $is_http_err = http_response_status_is_valid($errno);
  $http_error_code = $is_http_err ? $errno : SERVER_ERROR;

  status($http_error_code);

  return $http_error_code == NOT_FOUND ?
            error_not_found_output($errno, $errstr, $errfile, $errline) :
            error_server_error_output($errno, $errstr, $errfile, $errline);                    
}

/**
 * Returns not found error output
 *
 * @access private
 * @param string $msg 
 * @return string
 */
function error_not_found_output($errno, $errstr, $errfile, $errline)
{
  if(!function_exists('not_found'))
  {
    /**
     * Default not found error output
     *
     * @param string $errno 
     * @param string $errstr 
     * @param string $errfile 
     * @param string $errline 
     * @return string
     */
    function not_found($errno, $errstr, $errfile=null, $errline=null)
    {
      option('views_dir', option('error_views_dir'));
      $msg = h(rawurldecode($errstr));
      return html("<h1>Page not found:</h1><p><code>{$msg}</code></p>", error_layout());
    }
  }
  return not_found($errno, $errstr, $errfile, $errline);
}

/**
 * Returns server error output
 *
 * @access private
 * @param int $errno 
 * @param string $errstr 
 * @param string $errfile 
 * @param string $errline 
 * @return string
 */
function error_server_error_output($errno, $errstr, $errfile, $errline)
{
  if(!function_exists('server_error'))
  {
    /**
     * Default server error output
     *
     * @param string $errno 
     * @param string $errstr 
     * @param string $errfile 
     * @param string $errline 
     * @return string
     */
    function server_error($errno, $errstr, $errfile=null, $errline=null)
    {
      $is_http_error = http_response_status_is_valid($errno);
      $args = compact('errno', 'errstr', 'errfile', 'errline', 'is_http_error');
      option('views_dir', option('limonade_views_dir'));
      $html = render('error.html.php', null, $args);	
      option('views_dir', option('error_views_dir'));
      return html($html, error_layout(), $args);
    }
  }
  return server_error($errno, $errstr, $errfile, $errline);
}

/**
 * Set and returns error output layout
 *
 * @param string $layout 
 * @return string
 */
function error_layout($layout = false)
{
  static $o_layout = 'default_layout.php';
  if($layout !== false)
  {
    option('error_views_dir', option('views_dir'));
    $o_layout = $layout;
  }
  return $o_layout;
}


/**
 * Set a notice if arguments are provided
 * Returns all stored notices.
 * If $errno argument is null, reset the notices array
 *
 * @access private
 * @param string, null $str 
 * @return array
 */
function error_notice($errno = false, $errstr = null, $errfile = null, $errline = null)
{
  static $notices = array();
  if($errno) $notices[] = compact('errno', 'errstr', 'errfile', 'errline');
  else if(is_null($errno)) $notices = array();
  return $notices;
}

/**
 * Returns notices output rendering and reset notices
 *
 * @return string
 */
function error_notices_render()
{
  if(option('debug') && option('env') > ENV_PRODUCTION)
  {
    $notices = error_notice();
    error_notice(null); // reset notices
    $c_view_dir = option('views_dir'); // keep for restore after render
    option('views_dir', option('limonade_views_dir'));
    $o = render('_notices.html.php', null, array('notices' => $notices));
    option('views_dir', $c_view_dir); // restore current views dir

    return $o;
  }
}

/**
 * Checks if an error is will halt application execution. 
 * Notices and warnings will not.
 *
 * @access private
 * @param string $num error code number
 * @return boolean
 */
function error_wont_halt_app($num)
{
  return $num == E_NOTICE ||
         $num == E_WARNING ||
         $num == E_CORE_WARNING ||
         $num == E_COMPILE_WARNING ||
         $num == E_USER_WARNING ||
         $num == E_USER_NOTICE ||
         $num == E_DEPRECATED ||
         $num == E_USER_DEPRECATED ||
         $num == E_LIM_DEPRECATED;
}



/**
 * return error code name for a given code num, or return all errors names
 *
 * @param string $num 
 * @return mixed
 */
function error_type($num = null)
{
  $types = array (
              E_ERROR              => 'ERROR',
              E_WARNING            => 'WARNING',
              E_PARSE              => 'PARSING ERROR',
              E_NOTICE             => 'NOTICE',
              E_CORE_ERROR         => 'CORE ERROR',
              E_CORE_WARNING       => 'CORE WARNING',
              E_COMPILE_ERROR      => 'COMPILE ERROR',
              E_COMPILE_WARNING    => 'COMPILE WARNING',
              E_USER_ERROR         => 'USER ERROR',
              E_USER_WARNING       => 'USER WARNING',
              E_USER_NOTICE        => 'USER NOTICE',
              E_STRICT             => 'STRICT NOTICE',
              E_RECOVERABLE_ERROR  => 'RECOVERABLE ERROR',
              E_DEPRECATED         => 'DEPRECATED WARNING',
              E_USER_DEPRECATED    => 'USER DEPRECATED WARNING',
              E_LIM_DEPRECATED     => 'LIMONADE DEPRECATED WARNING'
              );
  return is_null($num) ? $types : $types[$num];
}

/**
 * Returns http response status for a given error number
 *
 * @param string $errno 
 * @return int
 */
function error_http_status($errno)
{
  $code = http_response_status_is_valid($errno) ? $errno : SERVER_ERROR;
  return http_response_status($code);
}

                                     # # #




# ============================================================================ #
#    3. REQUEST                                                                #
# ============================================================================ #
 
/**
 * Returns current request method for a given environment or current one
 *
 * @param string $env 
 * @return string
 */
function request_method($env = null)
{
  if(is_null($env)) $env = env();
  $m = array_key_exists('REQUEST_METHOD', $env['SERVER']) ? $env['SERVER']['REQUEST_METHOD'] : null;
  if($m == "POST" && array_key_exists('_method', $env['POST'])) 
    $m = strtoupper($env['POST']['_method']);
  if(!in_array(strtoupper($m), request_methods()))
  {
    trigger_error("'$m' request method is unknown or unavailable.", E_USER_WARNING);
    $m = false;
  }
  return $m;
}

/**
 * Checks if a request method or current one is allowed
 *
 * @param string $m 
 * @return bool
 */
function request_method_is_allowed($m = null)
{
  if(is_null($m)) $m = request_method();
  return in_array(strtoupper($m), request_methods());
}

/**
 * Checks if request method is GET
 *
 * @param string $env 
 * @return bool
 */
function request_is_get($env = null)
{
  return request_method($env) == "GET";
}

/**
 * Checks if request method is POST
 *
 * @param string $env 
 * @return bool
 */
function request_is_post($env = null)
{
  return request_method($env) == "POST";
}

/**
 * Checks if request method is PUT
 *
 * @param string $env 
 * @return bool
 */
function request_is_put($env = null)
{
  return request_method($env) == "PUT";
}

/**
 * Checks if request method is DELETE
 *
 * @param string $env 
 * @return bool
 */
function request_is_delete($env = null)
{
  return request_method($env) == "DELETE";
}

/**
 * Checks if request method is HEAD
 *
 * @param string $env 
 * @return bool
 */
function request_is_head($env = null)
{
  return request_method($env) == "HEAD";
}

/**
 * Checks if request method is PATCH
 *
 * @param string $env
 * @return bool
 */
function request_is_patch($env = null)
{
  return request_method($env) == "PATCH";
}

/**
 * Returns allowed request methods
 *
 * @return array
 */
function request_methods()
{
  return array("GET","POST","PUT","DELETE","HEAD","PATCH");
}

/**
 * Returns current request uri (the path that will be compared with routes)
 * 
 * (Inspired from codeigniter URI::_fetch_uri_string method)
 *
 * @return string
 */
function request_uri($env = null)
{
  static $uri = null;
  if(is_null($env))
  {
    if(!is_null($uri)) return $uri;
    $env = env();
  }

  if(array_key_exists('uri', $env['GET']))
  {
    $uri = $env['GET']['uri'];
  }
  else if(array_key_exists('u', $env['GET']))
  {
    $uri = $env['GET']['u'];
  }
  // bug: dot are converted to _... so we can't use it...
  // else if (count($env['GET']) == 1 && trim(key($env['GET']), '/') != '')
  // {
  //  $uri = key($env['GET']);
  // }
  else
  {
    $app_file = app_file();
    $path_info = isset($env['SERVER']['PATH_INFO']) ? $env['SERVER']['PATH_INFO'] : @getenv('PATH_INFO');
    $query_string =  isset($env['SERVER']['QUERY_STRING']) ? $env['SERVER']['QUERY_STRING'] : @getenv('QUERY_STRING');

    // Is there a PATH_INFO variable?
    // Note: some servers seem to have trouble with getenv() so we'll test it two ways
    if (trim($path_info, '/') != '' && $path_info != "/".$app_file)
    {
      if(strpos($path_info, '&') !== 0)
      {
        # exclude GET params
        $params = explode('&', $path_info);
        $path_info = array_shift($params);
        # populate $_GET
        foreach($params as $param)
        {
          if(strpos($param, '=') > 0)
          {
            list($k, $v) = explode('=', $param);
            $env['GET'][$k] = $v;
          }
        }
      }
      $uri = $path_info;
    }
    // No PATH_INFO?... What about QUERY_STRING?
    elseif (trim($query_string, '/') != '' && $query_string[0] == '/')
    {
      $uri = $query_string;
      $get = $env['GET'];
      if(count($get) > 0)
      {
        # exclude GET params
        $keys  = array_keys($get);
        $first = array_shift($keys);
        if(strpos($query_string, $first) === 0) $uri = $first;
      }
    }
    elseif(array_key_exists('REQUEST_URI', $env['SERVER']) && !empty($env['SERVER']['REQUEST_URI']))
    {
      $request_uri = rtrim($env['SERVER']['REQUEST_URI'], '?/').'/';
      $base_path = $env['SERVER']['SCRIPT_NAME'];

      if($request_uri."index.php" == $base_path) $request_uri .= "index.php";
      $uri = str_replace($base_path, '', $request_uri);
      if(option('base_uri') && strpos($uri, option('base_uri')) === 0) {
       $uri = substr($uri, strlen(option('base_uri')));
      }
      if(strpos($uri, '?') !== false) {
      	$uri = substr($uri, 0, strpos($uri, '?')) . '/';
      }
    }
    elseif($env['SERVER']['argc'] > 1 && trim($env['SERVER']['argv'][1], '/') != '')
    {
      $uri = $env['SERVER']['argv'][1];
    }
  }

  $uri = rtrim($uri, "/"); # removes ending /
  if(empty($uri))
  {
    $uri = '/';
  }
  else if($uri[0] != '/')
  {
    $uri = '/' . $uri; # add a leading slash
  }
  return rawurldecode($uri);
}




                                     # # #




# ============================================================================ #
#    4. ROUTER                                                                 #
# ============================================================================ #
 
/**
 * An alias of {@link dispatch_get()}
 *
 * @return void
 */
function dispatch($path_or_array, $callback, $options = array())
{
  dispatch_get($path_or_array, $callback, $options);
}

/**
 * Add a GET route. Also automatically defines a HEAD route.
 *
 * @param string $path_or_array 
 * @param string $callback
 * @param array $options (optional). See {@link route()} for available options.
 * @return void
 */
function dispatch_get($path_or_array, $callback, $options = array())
{
  route("GET", $path_or_array, $callback, $options);
  route("HEAD", $path_or_array, $callback, $options);
}

/**
 * Add a POST route
 *
 * @param string $path_or_array 
 * @param string $callback
 * @param array $options (optional). See {@link route()} for available options.
 * @return void
 */
function dispatch_post($path_or_array, $callback, $options = array())
{
  route("POST", $path_or_array, $callback, $options);
}

/**
 * Add a PUT route
 *
 * @param string $path_or_array 
 * @param string $callback
 * @param array $options (optional). See {@link route()} for available options.
 * @return void
 */
function dispatch_put($path_or_array, $callback, $options = array())
{
  route("PUT", $path_or_array, $callback, $options);
}

/**
 * Add a DELETE route
 *
 * @param string $path_or_array 
 * @param string $callback
 * @param array $options (optional). See {@link route()} for available options.
 * @return void
 */
function dispatch_delete($path_or_array, $callback, $options = array())
{
  route("DELETE", $path_or_array, $callback, $options);
}

/**
 * Add a PATCH route
 *
 * @param string $path_or_array
 * @param string $callback
 * @param array $options (optional). See {@link route()} for available options.
 * @return void
 */
function dispatch_patch($path_or_array, $callback, $options = array())
{
  route("PATCH", $path_or_array, $callback, $options);
}


/**
 * Add route if required params are provided.
 * Delete all routes if null is passed as a unique argument
 * Return all routes
 * 
 * @see route_build()
 * @access private
 * @param string $method 
 * @param string|array $path_or_array 
 * @param callback $func
 * @param array $options (optional). Available options: 
 *   - 'params' key with an array of parameters: for parametrized routes.
 *     those parameters will be merged with routes parameters.
 * @return array
 */
function route()
{
  static $routes = array();
  $nargs = func_num_args();
  if( $nargs > 0)
  {
    $args = func_get_args();
    if($nargs === 1 && is_null($args[0])) $routes = array();
    else if($nargs < 3) trigger_error("Missing arguments for route()", E_USER_ERROR);
    else
    {
      $method        = $args[0];
      $path_or_array = $args[1];
      $func          = $args[2];
      $options       = $nargs > 3 ? $args[3] : array();

      $routes[] = route_build($method, $path_or_array, $func, $options);
    }
  }
  return $routes;
}

/**
 * An alias of route(null): reset all routes
 * 
 * @access private
 * @return void
 */
function route_reset()
{
  route(null);
}

/**
 * Build a route and return it
 *
 * @access private
 * @param string $method allowed http method (one of those returned by {@link request_methods()})
 * @param string|array $path_or_array 
 * @param callback $callback callback called when route is found. It can be
 *   a function, an object method, a static method or a closure.
 *   See {@link http://php.net/manual/en/language.pseudo-types.php#language.types.callback php documentation}
 *   to learn more about callbacks.
 * @param array $options (optional). Available options: 
 *   - 'params' key with an array of parameters: for parametrized routes.
 *     those parameters will be merged with routes parameters.
 * @return array array with keys "method", "pattern", "names", "callback", "options"
 */
function route_build($method, $path_or_array, $callback, $options = array())
{
  $method = strtoupper($method);
  if(!in_array($method, request_methods())) 
    trigger_error("'$method' request method is unkown or unavailable.", E_USER_WARNING);

  if(is_array($path_or_array))
  {
    $path  = array_shift($path_or_array);
    $names = $path_or_array[0];
  }
  else
  {
    $path  = $path_or_array;
    $names = array();
  }

  $single_asterisk_subpattern   = "(?:/([^\/]*))?";
  $double_asterisk_subpattern   = "(?:/(.*))?";
  $optionnal_slash_subpattern   = "(?:/*?)";
  $no_slash_asterisk_subpattern = "(?:([^\/]*))?";

  if($path[0] == "^")
  {
    if($path{strlen($path) - 1} != "$") $path .= "$";
     $pattern = "#".$path."#i";
  }
  else if(empty($path) || $path == "/")
  {
    $pattern = "#^".$optionnal_slash_subpattern."$#";
  }
  else
  {
    $parsed = array();
    $elts = explode('/', $path);

    $parameters_count = 0;

    foreach($elts as $elt)
    {
      if(empty($elt)) continue;

      $name = null; 

      # extracting double asterisk **
      if($elt == "**"):
        $parsed[] = $double_asterisk_subpattern;
        $name = $parameters_count;

      # extracting single asterisk *
      elseif($elt == "*"):
        $parsed[] = $single_asterisk_subpattern;
        $name = $parameters_count;

      # extracting named parameters :my_param 
      elseif($elt[0] == ":"):
        if(preg_match('/^:([^\:]+)$/', $elt, $matches))
        {
          $parsed[] = $single_asterisk_subpattern;
          $name = $matches[1];
        };

      elseif(strpos($elt, '*') !== false):
        $sub_elts = explode('*', $elt);
        $parsed_sub = array();
        foreach($sub_elts as $sub_elt)
        {
          $parsed_sub[] = preg_quote($sub_elt, "#");
          $name = $parameters_count;
        }
        // 
        $parsed[] = "/".implode($no_slash_asterisk_subpattern, $parsed_sub);

      else:
        $parsed[] = "/".preg_quote($elt, "#");

      endif;

      /* set parameters names */ 
      if(is_null($name)) continue;
      if(!array_key_exists($parameters_count, $names) || is_null($names[$parameters_count]))
        $names[$parameters_count] = $name;
      $parameters_count++;
    }

    $pattern = "#^".implode('', $parsed).$optionnal_slash_subpattern."?$#i";
  }

  return array( "method"       => $method,
                "pattern"      => $pattern,
                "names"        => $names,
                "callback"     => $callback,
                "options"      => $options  );
}

/**
 * Find a route and returns it.
 * Parameters values extracted from the path are added and merged 
 * with the default 'params' option of the route
 * If not found, returns false.
 * Routes are checked from first added to last added.
 *
 * @access private
 * @param string $method 
 * @param string $path
 * @return array,false route array has same keys as route returned by 
 *  {@link route_build()} ("method", "pattern", "names", "callback", "options")
 *  + the processed "params" key
 */
function route_find($method, $path)
{
  $routes = route();
  $method = strtoupper($method);
  foreach($routes as $route)
  {
    if($method == $route["method"] && preg_match($route["pattern"], $path, $matches))
    {
      $options = $route["options"];
      $params = array_key_exists('params', $options) ? $options["params"] : array();
      if(count($matches) > 1)
      {
        array_shift($matches);
        $n_matches = count($matches);
        $names     = array_values($route["names"]);
        $n_names   = count($names);
        if( $n_matches < $n_names )
        {
          $a = array_fill(0, $n_names - $n_matches, null);
          $matches = array_merge($matches, $a);
        }
        else if( $n_matches > $n_names )
        {
          $names = range($n_names, $n_matches - 1);
        }
        $arr_comb = array_combine($names, $matches);
        $params = array_replace($params, $arr_comb);
      }
      $route["params"] = $params;
      return $route;
    }
  }
  return false;
}





# ============================================================================ #
#    5. OUTPUT AND RENDERING                                                   #
# ============================================================================ #

/**
 * Returns a string to output
 * 
 * It might use a template file, a function, or a formatted string (like {@link sprintf()}).
 * It could be embraced by a layout or not.
 * Local vars can be passed in addition to variables made available with the {@link set()}
 * function.
 *
 * @param string $content_or_func 
 * @param string $layout 
 * @param string $locals 
 * @return string
 */
function render($content_or_func, $layout = '', $locals = array())
{
  $args = func_get_args();
  $content_or_func = array_shift($args);
  $layout = count($args) > 0 ? array_shift($args) : layout();
  $view_path = file_path(option('views_dir'),$content_or_func);
  
  if(function_exists('before_render'))
    list($content_or_func, $layout, $locals, $view_path) = before_render($content_or_func, $layout, $locals, $view_path);    
  
  $vars = array_merge(set(), $locals);

  $flash = flash_now();
  if(array_key_exists('flash', $vars)) trigger_error('A $flash variable is already passed to view. Flash messages will only be accessible through flash_now()', E_USER_NOTICE);  
  else if(!empty($flash)) $vars['flash'] = $flash;

  $infinite_loop = false;

  # Avoid infinite loop: this function is in the backtrace ?
  if(function_exists($content_or_func))
  {
    $back_trace = debug_backtrace();
    while($trace = array_shift($back_trace))
    {
      if($trace['function'] == strtolower($content_or_func))
      {
        $infinite_loop = true;
        break;
      }
    }
  }

  if(function_exists($content_or_func) && !$infinite_loop)
  {
    ob_start();
    call_user_func($content_or_func, $vars);
    $content = ob_get_clean();
  }
  elseif(file_exists($view_path))
  {
    ob_start();
    extract($vars);
    include $view_path;
    $content = ob_get_clean();
  }
  else
  {
    if(substr_count($content_or_func, '%') !== count($vars)) $content = $content_or_func;
    else $content = vsprintf($content_or_func, $vars);
  }

  if(empty($layout)) return $content;

  return render($layout, null, array('content' => $content));
}

/**
 * Returns a string to output
 * 
 * Shortcut to render with no layout.
 *
 * @param string $content_or_func 
 * @param string $locals 
 * @return string
 */
function partial($content_or_func, $locals = array())
{
  return render($content_or_func, null, $locals);
}

/**
 * Returns html output with proper http headers
 *
 * @param string $content_or_func 
 * @param string $layout 
 * @param string $locals 
 * @return string
 */ 
function html($content_or_func, $layout = '', $locals = array())
{
  send_header('Content-Type: text/html; charset='.strtolower(option('encoding')));
  $args = func_get_args();
  return call_user_func_array('render', $args);
}

/**
 * Set and return current layout
 *
 * @param string $function_or_file 
 * @return string
 */
function layout($function_or_file = null)
{
  static $layout = null;
  if(func_num_args() > 0) $layout = $function_or_file;
  return $layout;
}

/**
 * Returns xml output with proper http headers
 *
 * @param string $content_or_func 
 * @param string $layout 
 * @param string $locals 
 * @return string
 */
function xml($data)
{
  send_header('Content-Type: text/xml; charset='.strtolower(option('encoding')));
  $args = func_get_args();
  return call_user_func_array('render', $args);
}

/**
 * Returns css output with proper http headers
 *
 * @param string $content_or_func 
 * @param string $layout 
 * @param string $locals 
 * @return string
 */
function css($content_or_func, $layout = '', $locals = array())
{
  send_header('Content-Type: text/css; charset='.strtolower(option('encoding')));
  $args = func_get_args();
  return call_user_func_array('render', $args);
}

/**
 * Returns javacript output with proper http headers
 *
 * @param string $content_or_func 
 * @param string $layout 
 * @param string $locals 
 * @return string
 */
function js($content_or_func, $layout = '', $locals = array())
{
  send_header('Content-Type: application/javascript; charset='.strtolower(option('encoding')));
  $args = func_get_args();
  return call_user_func_array('render', $args);
}

/**
 * Returns txt output with proper http headers
 *
 * @param string $content_or_func 
 * @param string $layout 
 * @param string $locals 
 * @return string
 */
function txt($content_or_func, $layout = '', $locals = array())
{
  send_header('Content-Type: text/plain; charset='.strtolower(option('encoding')));
  $args = func_get_args();
  return call_user_func_array('render', $args);
}

/**
 * Returns json representation of data with proper http headers.
 * On PHP 5 < PHP 5.2.0, you must provide your own implementation of the
 * <code>json_encode()</code> function beore using <code>json()</code>.
 *
 * @param mixed $data
 * @param int $json_option
 * @return string
 */
function json($data, $json_option = 0)
{
  send_header('Content-Type: application/json; charset='.strtolower(option('encoding')));
  return version_compare(PHP_VERSION, '5.3.0', '>=') ? json_encode($data, $json_option) : json_encode($data);
}

/**
 * undocumented function
 *
 * @param string $filename 
 * @param string $return 
 * @return mixed number of bytes delivered or file output if $return = true
 */
function render_file($filename, $return = false)
{
  # TODO implements X-SENDFILE headers
  // if($x-sendfile = option('x-sendfile'))
  // {
  //    // add a X-Sendfile header for apache and Lighttpd >= 1.5
  //    if($x-sendfile > X-SENDFILE) // add a X-LIGHTTPD-send-file header 
  //   
  // }
  // else
  // {
  //   
  // }
  $filename = str_replace('../', '', $filename);
  if(file_exists($filename))
  {
    $content_type = mime_type(file_extension($filename));
    $header = 'Content-type: '.$content_type;
    if(file_is_text($filename)) $header .= '; charset='.strtolower(option('encoding'));
    send_header($header);
    return file_read($filename, $return);
  }
  else halt(NOT_FOUND, "unknown filename $filename");
}

/**
 * Call before_sending_header() if it exists, then send headers
 * 
 * @param string $header
 * @return void
 */
function send_header($header = null, $replace = true, $code = false)
{
    if(!headers_sent()) 
    {
        call_if_exists('before_sending_header', $header);
        header($header, $replace, $code);
    }
}





                                     # # #




# ============================================================================ #
#    6. HELPERS                                                                #
# ============================================================================ #

/**
 * Returns an url composed of params joined with /
 * A param can be a string or an array.
 * If param is an array, its members will be added at the end of the return url
 * as GET parameters "&key=value".
 *
 * @param string or array $param1, $param2 ... 
 * @return string
 */ 
function url_for($params = null)
{
  $paths  = array();
  $params = func_get_args();
  $GET_params = array();
  foreach($params as $param)
  {
    if(is_array($param))
    {
      $GET_params = array_merge($GET_params, $param);
      continue;
    }
    if(filter_var_url($param))
    {
      $paths[] = $param;
      continue;
    }
    $p = explode('/',$param);
    foreach($p as $v)
    {
      if($v != "") $paths[] = str_replace('%23', '#', rawurlencode($v));
    }
  }

  $path = rtrim(implode('/', $paths), '/');
  
  if(!filter_var_url($path)) 
  {
    # it's a relative URL or an URL without a schema
    $base_uri = option('base_uri');
    $path = file_path($base_uri, $path);
  }
  
  if(!empty($GET_params))
  {
    $is_first_qs_param = true;
    $path_as_no_question_mark = strpos($path, '?') === false;
      
    foreach($GET_params as $k => $v)
    {
      $qs_separator = $is_first_qs_param && $path_as_no_question_mark ? 
                        '?' : '&amp;'; 
      $path .= $qs_separator . rawurlencode($k) . '=' . rawurlencode($v);
      $is_first_qs_param = false;
    }
  }
  
  if(DIRECTORY_SEPARATOR != '/') $path = str_replace(DIRECTORY_SEPARATOR, '/', $path);

  return $path;
}

/**
 * An alias of {@link htmlspecialchars()}.
 * If no $charset is provided, uses option('encoding') value
 *
 * @param string $str 
 * @param string $quote_style 
 * @param string $charset 
 * @return void
 */
function h($str, $quote_style = ENT_NOQUOTES, $charset = null)
{
  if(is_null($charset)) $charset = strtoupper(option('encoding'));
  return htmlspecialchars($str, $quote_style, $charset); 
}

/**
 * Set and returns flash messages that will be available in the next action
 * via the {@link flash_now()} function or the view variable <code>$flash</code>.
 * 
 * If multiple values are provided, set <code>$name</code> variable with an array of those values.
 * If there is only one value, set <code>$name</code> variable with the provided $values
 * or if it's <code>$name</code> is an array, merge it with current messages.
 *
 * @param string, array $name 
 * @param mixed  $values,... 
 * @return mixed variable value for $name if $name argument is provided, else return all variables
 */
function flash($name = null, $value = null)
{
  if(!defined('SID')) trigger_error("Flash messages can't be used because session isn't enabled", E_USER_WARNING);
  static $messages = array();
  $args = func_get_args();
  $name = array_shift($args);
  if(is_null($name)) return $messages;
  if(is_array($name)) return $messages = array_merge($messages, $name);
  if(!empty($args))
  {
    $messages[$name] = count($args) > 1 ? $args : $args[0];
  }
  if(!array_key_exists($name, $messages)) return null;
  else return $messages[$name];
  return $messages;
}

/**
 * Set and returns flash messages available for the current action, included those
 * defined in the previous action with {@link flash()}
 * Those messages will also be passed to the views and made available in the 
 * <code>$flash</code> variable.
 * 
 * If multiple values are provided, set <code>$name</code> variable with an array of those values.
 * If there is only one value, set <code>$name</code> variable with the provided $values
 * or if it's <code>$name</code> is an array, merge it with current messages.
 *
 * @param string, array $name 
 * @param mixed  $values,... 
 * @return mixed variable value for $name if $name argument is provided, else return all variables
 */
function flash_now($name = null, $value = null)
{
  static $messages = null;
  if(is_null($messages))
  {
    $fkey = LIM_SESSION_FLASH_KEY;
    $messages = array();
    if(defined('SID') && array_key_exists($fkey, $_SESSION)) $messages = $_SESSION[$fkey];
  }
  $args = func_get_args();
  $name = array_shift($args);
  if(is_null($name)) return $messages;
  if(is_array($name)) return $messages = array_merge($messages, $name);
  if(!empty($args))
  {
    $messages[$name] = count($args) > 1 ? $args : $args[0];
  }
  if(!array_key_exists($name, $messages)) return null;
  else return $messages[$name];
  return $messages;
}

/**
 * Delete current flash messages in session, and set new ones stored with 
 * flash function.
 * Called before application exit.
 *
 * @access private
 * @return void
 */
function flash_sweep()
{
  if(defined('SID'))
  {
    $fkey = LIM_SESSION_FLASH_KEY;
    $_SESSION[$fkey] = flash();
  }
}

/**
 * Starts capturing block of text
 *
 * Calling without params stops capturing (same as end_content_for()).
 * After capturing the captured block is put into a variable
 * named $name for later use in layouts. If second parameter
 * is supplied, its content will be used instead of capturing
 * a block of text.
 *
 * @param string $name
 * @param string $content
 * @return void
 */
function content_for($name = null, $content = null)
{
  static $_name = null;
  if(is_null($name) && !is_null($_name))
  {
    set($_name, ob_get_clean());
    $_name = null;	
  }
  elseif(!is_null($name) && !isset($content))
  {
    $_name = $name;	
    ob_start();
  }
  elseif(isset($name, $content))
  {
    set($name, $content);
  }
}

/**
 * Stops capturing block of text
 *
 * @return void
 */
function end_content_for()
{
  content_for();
}

/**
 * Shows current memory and execution time of the application.
 * Returns only execution time if <code>memory_get_usage()</code> 
 * isn't available.
 * ( That's the case before PHP5.2.1 if PHP isn't compiled with option 
 *   <code>--enable-memory-limit</code>. )
 * 
 * @access public
 * @return array
 */
function benchmark()
{
	$res = array( 'execution_time' => (microtime(true) - LIM_START_MICROTIME) );
	if(defined('LIM_START_MEMORY'))
	{
		$current_mem_usage     = memory_get_usage();
		$res['current_memory'] = $current_mem_usage;
		$res['start_memory']   = LIM_START_MEMORY;
		$res['average_memory'] = (LIM_START_MEMORY + $current_mem_usage) / 2;
	}
	
	return $res;
}




                                     # # #




# ============================================================================ #
#    7. UTILS                                                                  #
# ============================================================================ #
 
/**
 * Calls a function if exists
 *
 * @param callback $callback a function stored in a string variable, 
 *   or an object and the name of a method within the object
 *   See {@link http://php.net/manual/en/language.pseudo-types.php#language.types.callback php documentation}
 *   to learn more about callbacks.
 * @param mixed $arg,.. (optional)
 * @return mixed
 */
function call_if_exists($callback)
{
  $args = func_get_args();
  $callback = array_shift($args);
  if(is_callable($callback)) return call_user_func_array($callback, $args);
  return;
}

/**
 * Define a constant unless it already exists
 *
 * @param string $name 
 * @param string $value 
 * @return void
 */
function define_unless_exists($name, $value)
{
  if(!defined($name)) define($name, $value);
}

/**
 * Return a default value if provided value is empty
 *
 * @param mixed $value 
 * @param mixed $default default value returned if $value is empty
 * @return mixed
 */
function value_or_default($value, $default)
{
  return empty($value) ? $default : $value;
}

/**
 * An alias of {@link value_or_default()}
 *
 * 
 * @param mixed $value 
 * @param mixed $default 
 * @return mixed
 */
function v($value, $default)
{
  return value_or_default($value, $default);
}

/**
 * Load php files with require_once in a given dir
 *
 * @param string $path Path in which are the file to load
 * @param string $pattern a regexp pattern that filter files to load
 * @param bool $prevents_output security option that prevents output
 * @return array paths of loaded files
 */
function require_once_dir($path, $pattern = "*.php", $prevents_output = true)
{
  if($path[strlen($path) - 1] != "/") $path .= "/";
  $filenames = glob($path.$pattern);
  if(!is_array($filenames)) $filenames = array();
  if($prevents_output) ob_start();
  foreach($filenames as $filename) require_once $filename;
  if($prevents_output) ob_end_clean();
  return $filenames;
}

/**
 * Dumps a variable into inspectable format
 *
 * @param anything $var the variable to debug
 * @param bool $output_as_html sets whether to wrap output in <pre> tags. default: true
 * @return string the variable with output
 */
function debug($var, $output_as_html = true)
{ 
  if ( is_null($var) ) { return '<span class="null-value">[NULL]</span>'; };
  $out = '';
  switch ($var) 
  { 
    case empty($var):
      $out = '[empty value]';
      break;
    
    case is_array($var):
      $out = var_export($var, true);
      break;
    
    case is_object($var):
      $out = var_export($var, true);
      break;
      
    case is_string($var):
      $out = $var;
      break;
    
    default:
      $out = var_export($var, true);
      break;
  }
  if ($output_as_html) { $out = "<pre>\n" . h($out) ."</pre>"; }
  return $out;
}


## HTTP utils  _________________________________________________________________


### Constants: HTTP status codes

define( 'HTTP_CONTINUE',                      100 );
define( 'HTTP_SWITCHING_PROTOCOLS',           101 );
define( 'HTTP_PROCESSING',                    102 );
define( 'HTTP_OK',                            200 );
define( 'HTTP_CREATED',                       201 );
define( 'HTTP_ACCEPTED',                      202 );
define( 'HTTP_NON_AUTHORITATIVE',             203 );
define( 'HTTP_NO_CONTENT',                    204 );
define( 'HTTP_RESET_CONTENT',                 205 );
define( 'HTTP_PARTIAL_CONTENT',               206 );
define( 'HTTP_MULTI_STATUS',                  207 );
                                              
define( 'HTTP_MULTIPLE_CHOICES',              300 );
define( 'HTTP_MOVED_PERMANENTLY',             301 );
define( 'HTTP_MOVED_TEMPORARILY',             302 );
define( 'HTTP_SEE_OTHER',                     303 );
define( 'HTTP_NOT_MODIFIED',                  304 );
define( 'HTTP_USE_PROXY',                     305 );
define( 'HTTP_TEMPORARY_REDIRECT',            307 );

define( 'HTTP_BAD_REQUEST',                   400 );
define( 'HTTP_UNAUTHORIZED',                  401 );
define( 'HTTP_PAYMENT_REQUIRED',              402 );
define( 'HTTP_FORBIDDEN',                     403 );
define( 'HTTP_NOT_FOUND',                     404 );
define( 'HTTP_METHOD_NOT_ALLOWED',            405 );
define( 'HTTP_NOT_ACCEPTABLE',                406 );
define( 'HTTP_PROXY_AUTHENTICATION_REQUIRED', 407 );
define( 'HTTP_REQUEST_TIME_OUT',              408 );
define( 'HTTP_CONFLICT',                      409 );
define( 'HTTP_GONE',                          410 );
define( 'HTTP_LENGTH_REQUIRED',               411 );
define( 'HTTP_PRECONDITION_FAILED',           412 );
define( 'HTTP_REQUEST_ENTITY_TOO_LARGE',      413 );
define( 'HTTP_REQUEST_URI_TOO_LARGE',         414 );
define( 'HTTP_UNSUPPORTED_MEDIA_TYPE',        415 );
define( 'HTTP_RANGE_NOT_SATISFIABLE',         416 );
define( 'HTTP_EXPECTATION_FAILED',            417 );
define( 'HTTP_UNPROCESSABLE_ENTITY',          422 );
define( 'HTTP_LOCKED',                        423 );
define( 'HTTP_FAILED_DEPENDENCY',             424 );
define( 'HTTP_UPGRADE_REQUIRED',              426 );

define( 'HTTP_INTERNAL_SERVER_ERROR',         500 );
define( 'HTTP_NOT_IMPLEMENTED',               501 );
define( 'HTTP_BAD_GATEWAY',                   502 );
define( 'HTTP_SERVICE_UNAVAILABLE',           503 );
define( 'HTTP_GATEWAY_TIME_OUT',              504 );
define( 'HTTP_VERSION_NOT_SUPPORTED',         505 );
define( 'HTTP_VARIANT_ALSO_VARIES',           506 );
define( 'HTTP_INSUFFICIENT_STORAGE',          507 );
define( 'HTTP_NOT_EXTENDED',                  510 );

/**
 * Output proper HTTP header for a given HTTP code
 *
 * @param string $code 
 * @return void
 */
function status($code = 500)
{
  if(!headers_sent())
  {
    $str = http_response_status_code($code);
    send_header($str);
  }
}

/**
 * Http redirection
 * 
 * Same use as {@link url_for()}
 * By default HTTP status code is 302, but a different code can be specified
 * with a status key in array parameter.
 * 
 * <code>
 * redirecto('new','url'); # 302 HTTP_MOVED_TEMPORARILY by default
 * redirecto('new','url', array('status' => HTTP_MOVED_PERMANENTLY));
 * </code>
 * 
 * @param string or array $param1, $param2... 
 * @return void
 */
function redirect_to($params)
{
  # [NOTE]: (from php.net) HTTP/1.1 requires an absolute URI as argument to » Location:
  # including the scheme, hostname and absolute path, but some clients accept
  # relative URIs. You can usually use $_SERVER['HTTP_HOST'],
  # $_SERVER['PHP_SELF'] and dirname() to make an absolute URI from a relative
  # one yourself.

  # TODO make absolute uri
  if(!headers_sent())
  {
    $status = HTTP_MOVED_TEMPORARILY; # default for a redirection in PHP
    $params = func_get_args();
    $n_params = array();
    # extract status param if exists
    foreach($params as $param)
    {
      if(is_array($param))
      {
        if(array_key_exists('status', $param))
        {
          $status = $param['status'];
          unset($param['status']);
        }
      }
      $n_params[] = $param;
    }
    $uri = call_user_func_array('url_for', $n_params);
    $uri = htmlspecialchars_decode($uri, ENT_NOQUOTES);
    stop_and_exit(false);
    send_header('Location: '.$uri, true, $status);
    exit;
  }
}

/**
 * Http redirection
 *
 * @deprecated deprecated since version 0.4. Please use {@link redirect_to()} instead.
 * @param string $url 
 * @return void
 */
function redirect($uri)
{
  # halt('redirect() is deprecated. Please use redirect_to() instead.', E_LIM_DEPRECATED);
  # halt not necesary... it won't be visible because of http redirection...
  redirect_to($uri);
}

/**
 * Returns HTTP response status for a given code.
 * If no code provided, return an array of all status
 *
 * @param string $num 
 * @return string,array
 */
function http_response_status($num = null)
{
  $status =  array(
      100 => 'Continue',
      101 => 'Switching Protocols',
      102 => 'Processing',

      200 => 'OK',
      201 => 'Created',
      202 => 'Accepted',
      203 => 'Non-Authoritative Information',
      204 => 'No Content',
      205 => 'Reset Content',
      206 => 'Partial Content',
      207 => 'Multi-Status',
      226 => 'IM Used',

      300 => 'Multiple Choices',
      301 => 'Moved Permanently',
      302 => 'Found',
      303 => 'See Other',
      304 => 'Not Modified',
      305 => 'Use Proxy',
      306 => 'Reserved',
      307 => 'Temporary Redirect',

      400 => 'Bad Request',
      401 => 'Unauthorized',
      402 => 'Payment Required',
      403 => 'Forbidden',
      404 => 'Not Found',
      405 => 'Method Not Allowed',
      406 => 'Not Acceptable',
      407 => 'Proxy Authentication Required',
      408 => 'Request Timeout',
      409 => 'Conflict',
      410 => 'Gone',
      411 => 'Length Required',
      412 => 'Precondition Failed',
      413 => 'Request Entity Too Large',
      414 => 'Request-URI Too Long',
      415 => 'Unsupported Media Type',
      416 => 'Requested Range Not Satisfiable',
      417 => 'Expectation Failed',
      422 => 'Unprocessable Entity',
      423 => 'Locked',
      424 => 'Failed Dependency',
      426 => 'Upgrade Required',

      500 => 'Internal Server Error',
      501 => 'Not Implemented',
      502 => 'Bad Gateway',
      503 => 'Service Unavailable',
      504 => 'Gateway Timeout',
      505 => 'HTTP Version Not Supported',
      506 => 'Variant Also Negotiates',
      507 => 'Insufficient Storage',
      510 => 'Not Extended'
  );
  if(is_null($num)) return $status;
  return array_key_exists($num, $status) ? $status[$num] : '';
}

/**
 * Checks if an HTTP response code is valid
 *
 * @param string $num 
 * @return bool
 */
function http_response_status_is_valid($num)
{
  $r = http_response_status($num);
  return !empty($r);
}

/**
 * Returns an HTTP response status string for a given code
 *
 * @param string $num 
 * @return string
 */
function http_response_status_code($num)
{
  $protocole = empty($_SERVER["SERVER_PROTOCOL"]) ? "HTTP/1.1" : $_SERVER["SERVER_PROTOCOL"];
  if($str = http_response_status($num)) return "$protocole $num $str";
}

/**
 * Check if the _Accept_ header is present, and includes the given `type`.
 *
 * When the _Accept_ header is not present `true` is returned. Otherwise
 * the given `type` is matched by an exact match, and then subtypes. You
 * may pass the subtype such as "html" which is then converted internally
 * to "text/html" using the mime lookup table.
 *
 * @param string $type
 * @param string $env 
 * @return bool
 */
function http_ua_accepts($type, $env = null)
{
  if(is_null($env)) $env = env();
  $accept = array_key_exists('HTTP_ACCEPT', $env['SERVER']) ? $env['SERVER']['HTTP_ACCEPT'] : null;
  
  if(!$accept || $accept === '*/*') return true;
  
  if($type)
  {
    // Allow "html" vs "text/html" etc
    if(!strpos($type, '/')) $type = mime_type($type);
    
    // Check if we have a direct match
    if(strpos($accept, $type) > -1) return true;
    
    // Check if we have type/*  
    $type_parts = explode('/', $type); 
    $type = $type_parts[0].'/*';
    return (strpos($accept, $type) > -1);
  }
  
  return false; 
}

## FILE utils  _________________________________________________________________

/**
 * Returns mime type for a given extension or if no extension is provided,
 * all mime types in an associative array, with extensions as keys. 
 * If extension is unknown, returns null.
 * (extracted from Orbit source http://orbit.luaforge.net/).
 *
 *
 * @param string $ext
 * @return string, array, null
 */
function mime_type($ext = null)
{
  $types = array(
    'ai'      => 'application/postscript',
    'aif'     => 'audio/x-aiff',
    'aifc'    => 'audio/x-aiff',
    'aiff'    => 'audio/x-aiff',
    'asc'     => 'text/plain',
    'atom'    => 'application/atom+xml',
    'atom'    => 'application/atom+xml',
    'au'      => 'audio/basic',
    'avi'     => 'video/x-msvideo',
    'bcpio'   => 'application/x-bcpio',
    'bin'     => 'application/octet-stream',
    'bmp'     => 'image/bmp',
    'cdf'     => 'application/x-netcdf',
    'cgm'     => 'image/cgm',
    'class'   => 'application/octet-stream',
    'cpio'    => 'application/x-cpio',
    'cpt'     => 'application/mac-compactpro',
    'csh'     => 'application/x-csh',
    'css'     => 'text/css',
    'csv'     => 'text/csv',
    'dcr'     => 'application/x-director',
    'dir'     => 'application/x-director',
    'djv'     => 'image/vnd.djvu',
    'djvu'    => 'image/vnd.djvu',
    'dll'     => 'application/octet-stream',
    'dmg'     => 'application/octet-stream',
    'dms'     => 'application/octet-stream',
    'doc'     => 'application/msword',
    'dtd'     => 'application/xml-dtd',
    'dvi'     => 'application/x-dvi',
    'dxr'     => 'application/x-director',
    'eps'     => 'application/postscript',
    'etx'     => 'text/x-setext',
    'exe'     => 'application/octet-stream',
    'ez'      => 'application/andrew-inset',
    'gif'     => 'image/gif',
    'gram'    => 'application/srgs',
    'grxml'   => 'application/srgs+xml',
    'gtar'    => 'application/x-gtar',
    'hdf'     => 'application/x-hdf',
    'hqx'     => 'application/mac-binhex40',
    'htm'     => 'text/html',
    'html'    => 'text/html',
    'ice'     => 'x-conference/x-cooltalk',
    'ico'     => 'image/x-icon',
    'ics'     => 'text/calendar',
    'ief'     => 'image/ief',
    'ifb'     => 'text/calendar',
    'iges'    => 'model/iges',
    'igs'     => 'model/iges',
    'jpe'     => 'image/jpeg',
    'jpeg'    => 'image/jpeg',
    'jpg'     => 'image/jpeg',
    'js'      => 'application/x-javascript',
    'json'    => 'application/json',
    'kar'     => 'audio/midi',
    'latex'   => 'application/x-latex',
    'lha'     => 'application/octet-stream',
    'lzh'     => 'application/octet-stream',
    'm3u'     => 'audio/x-mpegurl',
    'man'     => 'application/x-troff-man',
    'mathml'  => 'application/mathml+xml',
    'me'      => 'application/x-troff-me',
    'mesh'    => 'model/mesh',
    'mid'     => 'audio/midi',
    'midi'    => 'audio/midi',
    'mif'     => 'application/vnd.mif',
    'mov'     => 'video/quicktime',
    'movie'   => 'video/x-sgi-movie',
    'mp2'     => 'audio/mpeg',
    'mp3'     => 'audio/mpeg',
    'mpe'     => 'video/mpeg',
    'mpeg'    => 'video/mpeg',
    'mpg'     => 'video/mpeg',
    'mpga'    => 'audio/mpeg',
    'ms'      => 'application/x-troff-ms',
    'msh'     => 'model/mesh',
    'mxu'     => 'video/vnd.mpegurl',
    'nc'      => 'application/x-netcdf',
    'oda'     => 'application/oda',
    'ogg'     => 'application/ogg',
    'pbm'     => 'image/x-portable-bitmap',
    'pdb'     => 'chemical/x-pdb',
    'pdf'     => 'application/pdf',
    'pgm'     => 'image/x-portable-graymap',
    'pgn'     => 'application/x-chess-pgn',
    'png'     => 'image/png',
    'pnm'     => 'image/x-portable-anymap',
    'ppm'     => 'image/x-portable-pixmap',
    'ppt'     => 'application/vnd.ms-powerpoint',
    'ps'      => 'application/postscript',
    'qt'      => 'video/quicktime',
    'ra'      => 'audio/x-pn-realaudio',
    'ram'     => 'audio/x-pn-realaudio',
    'ras'     => 'image/x-cmu-raster',
    'rdf'     => 'application/rdf+xml',
    'rgb'     => 'image/x-rgb',
    'rm'      => 'application/vnd.rn-realmedia',
    'roff'    => 'application/x-troff',
    'rss'     => 'application/rss+xml',
    'rtf'     => 'text/rtf',
    'rtx'     => 'text/richtext',
    'sgm'     => 'text/sgml',
    'sgml'    => 'text/sgml',
    'sh'      => 'application/x-sh',
    'shar'    => 'application/x-shar',
    'silo'    => 'model/mesh',
    'sit'     => 'application/x-stuffit',
    'skd'     => 'application/x-koan',
    'skm'     => 'application/x-koan',
    'skp'     => 'application/x-koan',
    'skt'     => 'application/x-koan',
    'smi'     => 'application/smil',
    'smil'    => 'application/smil',
    'snd'     => 'audio/basic',
    'so'      => 'application/octet-stream',
    'spl'     => 'application/x-futuresplash',
    'src'     => 'application/x-wais-source',
    'sv4cpio' => 'application/x-sv4cpio',
    'sv4crc'  => 'application/x-sv4crc',
    'svg'     => 'image/svg+xml',
    'svgz'    => 'image/svg+xml',
    'swf'     => 'application/x-shockwave-flash',
    't'       => 'application/x-troff',
    'tar'     => 'application/x-tar',
    'tcl'     => 'application/x-tcl',
    'tex'     => 'application/x-tex',
    'texi'    => 'application/x-texinfo',
    'texinfo' => 'application/x-texinfo',
    'tif'     => 'image/tiff',
    'tiff'    => 'image/tiff',
    'tr'      => 'application/x-troff',
    'tsv'     => 'text/tab-separated-values',
    'txt'     => 'text/plain',
    'ustar'   => 'application/x-ustar',
    'vcd'     => 'application/x-cdlink',
    'vrml'    => 'model/vrml',
    'vxml'    => 'application/voicexml+xml',
    'wav'     => 'audio/x-wav',
    'wbmp'    => 'image/vnd.wap.wbmp',
    'wbxml'   => 'application/vnd.wap.wbxml',
    'wml'     => 'text/vnd.wap.wml',
    'wmlc'    => 'application/vnd.wap.wmlc',
    'wmls'    => 'text/vnd.wap.wmlscript',
    'wmlsc'   => 'application/vnd.wap.wmlscriptc',
    'wrl'     => 'model/vrml',
    'xbm'     => 'image/x-xbitmap',
    'xht'     => 'application/xhtml+xml',
    'xhtml'   => 'application/xhtml+xml',
    'xls'     => 'application/vnd.ms-excel',
    'xml'     => 'application/xml',
    'xpm'     => 'image/x-xpixmap',
    'xsl'     => 'application/xml',
    'xslt'    => 'application/xslt+xml',
    'xul'     => 'application/vnd.mozilla.xul+xml',
    'xwd'     => 'image/x-xwindowdump',
    'xyz'     => 'chemical/x-xyz',
    'zip'     => 'application/zip'
  );

  if (is_null($ext)) return $types;

  $lower_ext = strtolower($ext);

  return isset($types[$lower_ext]) ? $types[$lower_ext] : null;
}

/**
 * Detect MIME Content-type for a file
 *
 * @param string $filename Path to the tested file.
 * @return string
 */
function file_mime_content_type($filename)
{
  $ext = file_extension($filename); /* strtolower isn't necessary */
  if($mime = mime_type($ext)) return $mime;
  elseif (function_exists('finfo_open'))
  {
    if($finfo = finfo_open(FILEINFO_MIME))
    {
      if($mime = finfo_file($finfo, $filename))
      {
        finfo_close($finfo);
        return $mime;        
      }
    }
  }
  return 'application/octet-stream';
}


/**
 * Read and output file content and return filesize in bytes or status after 
 * closing file.
 * This function is very efficient for outputing large files without timeout
 * nor too expensive memory use
 *
 * @param string $filename 
 * @param string $retbytes 
 * @return bool, int
 */
function file_read_chunked($filename, $retbytes = true)
{
  $chunksize = 1*(1024*1024); // how many bytes per chunk
  $buffer    = '';
  $cnt       = 0;
  $handle    = fopen($filename, 'rb');
  if ($handle === false) return false;

  ob_start();
  while (!feof($handle)) {
    $buffer = fread($handle, $chunksize);
    echo $buffer;
    ob_flush();
    flush();
    if ($retbytes) $cnt += strlen($buffer);
    set_time_limit(0);
  }
  ob_end_flush();

  $status = fclose($handle);
  if ($retbytes && $status) return $cnt; // return num. bytes delivered like readfile() does.
  return $status;
}

/**
 * Create a file path by concatenation of given arguments.
 * Windows paths with backslash directory separators are normalized in *nix paths.
 *
 * @param string $path, ... 
 * @return string normalized path
 */
function file_path($path)
{
  $args = func_get_args();
  $ds = '/'; 
  $win_ds = '\\';
  $n_path = count($args) > 1 ? implode($ds, $args) : $path;
  if(strpos($n_path, $win_ds) !== false) $n_path = str_replace( $win_ds, $ds, $n_path );
  $n_path = preg_replace( "#$ds+#", $ds, $n_path);
  
  return $n_path;
}

/**
 * Returns file extension or false if none
 *
 * @param string $filename 
 * @return string, false
 */
function file_extension($filename)
{
  $pos = strrpos($filename, '.');
  if($pos !== false) return substr($filename, $pos + 1);
  return false;
}

/**
 * Checks if $filename is a text file
 *
 * @param string $filename 
 * @return bool
 */
function file_is_text($filename)
{
  if($mime = file_mime_content_type($filename)) return substr($mime,0,5) == "text/";
  return null;
}

/**
 * Checks if $filename is a binary file
 *
 * @param string $filename 
 * @return void
 */
function file_is_binary($filename)
{
  $is_text = file_is_text($filename);
  return is_null($is_text) ? null : !$is_text;
}

/**
 * Return or output file content
 *
 * @return 	string, int
 *				
 **/

function file_read($filename, $return = false)
{
  if(!file_exists($filename)) trigger_error("$filename doesn't exists", E_USER_ERROR);
  if($return) return file_get_contents($filename);
  return file_read_chunked($filename);
}

/**
 * Returns an array of files contained in a directory
 *
 * @param string $dir 
 * @return array
 */
function file_list_dir($dir)
{
  $files = array();
  if ($handle = opendir($dir))
  {
    while (false !== ($file = readdir($handle)))
    {
      if ($file[0] != "." && $file != "..") $files[] = $file;
    }
    closedir($handle);
  }
  return $files;
}

## Extra utils  ________________________________________________________________

if(!function_exists('array_replace'))
{
  /**
   * For PHP 5 < 5.3.0 (backward compatibility)
   * (from {@link http://www.php.net/manual/fr/function.array-replace.php#92549 this php doc. note})
   * 
   * @see array_replace()
   * @param string $array 
   * @param string $array1 
   * @return $array
   */
  function array_replace( array &$array, array &$array1 )
  {
    $args  = func_get_args();
    $count = func_num_args();

    for ($i = 0; $i < $count; ++$i)
    {
      if(is_array($args[$i]))
      {
        foreach ($args[$i] as $key => $val) $array[$key] = $val;
      }
      else
      {
        trigger_error(
          __FUNCTION__ . '(): Argument #' . ($i+1) . ' is not an array',
          E_USER_WARNING
        );
        return null;
      }
    }
    return $array;
  }
}

/**
 * Check if a string is an url
 *
 * This implementation no longer requires 
 * {@link http://www.php.net/manual/en/book.filter.php the filter extenstion}, 
 * so it will improve compatibility with older PHP versions.
 *
 * @param string $str 
 * @return false, str   the string if true, false instead
 */
function filter_var_url($str)
{
  $regexp = '@^https?://([-[:alnum:]]+\.)+[a-zA-Z]{2,6}(:[0-9]+)?(.*)?$@';
  $options = array( "options" => array("regexp" => $regexp ));
  return preg_match($regexp, $str) ? $str : false;
}


/**
 * For PHP 5 < 5.1.0 (backward compatibility)
 * (from {@link http://www.php.net/manual/en/function.htmlspecialchars-decode.php#82133})
 * 
 * @param string $string 
 * @param string $quote_style, one of: ENT_COMPAT, ENT_QUOTES, ENT_NOQUOTES 
 * @return the decoded string
 */
function limonade_htmlspecialchars_decode($string, $quote_style = ENT_COMPAT)
{
	$table = array_flip(get_html_translation_table(HTML_SPECIALCHARS, $quote_style));
	if($quote_style === ENT_QUOTES)
		$table['&#039;'] = $table['&#39;'] = '\'';
	return strtr($string, $table);
}

if(!function_exists('htmlspecialchars_decode'))
{
	function htmlspecialchars_decode($string, $quote_style = ENT_COMPAT)
	{
		return limonade_htmlspecialchars_decode($string, $quote_style);
	}
}

/**
 * Called just after loading libs, it provides fallback for some 
 * functions if they don't exists.
 *
 */
function fallbacks_for_not_implemented_functions()
{
  if(!function_exists('json_encode'))
  {
    /**
     * for PHP 5 < PHP 5.2.0
     *
     */
    function json_encode()
    {
      trigger_error(
        __FUNCTION__ . '(): no JSON functions available. Please provide your own implementation of ' . __FUNCTION__ . '() in order to use it.', E_USER_WARNING
      );
    }
  }
}



#   ================================= END ==================================   #
