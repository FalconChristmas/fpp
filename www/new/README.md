# PHP Slim Framework v4 Skeleton

A super organized (with a sane folder structure) and very customizable skeleton for Slim Framework v4 **with Twig Views**. Use it to start working quickly on a new project.

**NOTE**: If you want this skeleton **without Twig**, go to: (https://github.com/ricardoper/slim4-skeleton).

This project use PHP [Composer](https://getcomposer.org/) for a fast installation without any trouble.

- PHP >= 7.2
- Customizable with an easy configuration:
  + Views
  + Logger
  + Routes
  + Handlers
  + Middlewares
  + Configurations
  + Service Providers
  + Response Emitters
  + Error Handler
  + Shutdown Handler
- [Twig](https://twig.symfony.com/) Views
- Sessions
- Controllers
- Global Helpers
- [PSR-3](https://www.php-fig.org/psr/psr-3/) for logging
- Environment variables with [PHP dotenv](https://github.com/vlucas/phpdotenv)
- [Pimple](https://pimple.symfony.com/) as Dependency Injection Container
- [ddumper](https://github.com/ricardoper/ddumper) for development (based on [Symfony VarDumper](https://github.com/symfony/var-dumper))
- Better Error Handlers (HTML, JSON, XML, PlainText - Accept / Content Type)
- [Medoo](https://medoo.in/) Database Framework (MySQL, PostgreSQL, SQLite, MS SQL Server, ...)

**NOTES**:
- *Medoo* is optional and is not enabled by default. You can use another library as a Service Provider. If you want to use this library, don't forget to install the required PDO extensions.

## Table of Contents
- [How to Install](#how-to-install)
- [Most Relevant Folders](#most-relevant-folders)
- [Global Helpers](#global-helpers)
  - [Development Only](#global-helpers-for-development-only)
- [Configurations](#configurations)
  - [Dot Notation](#configurations-dot-notation)
- [Routes](#routes)
- [Controllers](#controllers)
  - [Helpers](#controllers-helpers)
  - [Flash Messages Helpers](#flash-messages-helpers)
- [Views](#views)
  - [Helpers](#views-helpers)
- [Middlewares](#middlewares)
- [Response Emitters](#response-emitters)
- [Models](#models)
  - [Models Helpers](#models-helpers)
- [Services Providers](#services-providers)
- [Handlers](#handlers)
- [Database Support](#database-support)
- [Exceptions](#exceptions)
- [Logging](#logging)
- [Debugging](#debugging)
- [Demo](#demo)
- [Benchmarks](#benchmarks)

---

## How to Install

Run this command from the directory in which you want to install your new Slim Framework v4 Skeleton with Twig Views.

```bash
composer create-project ricardoper/slim4-twig-skeleton [my-app-name]
```

**NOTE**:
- Replace `[my-app-name]`with the desired directory name for your new application.
- Ensure `storage/` is web writeable.
- Point your virtual host document root to your new application's `public/` directory.

## Most Relevant Folders

- /app : *Application* code (**App** Namespace - [PSR-4](https://www.php-fig.org/psr/psr-4/))
  + ./Controllers : Accepts input and converts it to commands for the model. Add your *Controllers* here.
  + ./Emitters : Emits a response, including status line, headers, and the message body, according to the environment. Add your *Response Emitters* here.
  + ./Handlers : Handles specified behaviors of the application. Add your *Handlers* here.
  + ./Middlewares : Provide a convenient mechanism for filtering HTTP requests entering your application. Add your *Middlewares* here.
  + ./Models : Manages the data, logic and rules of the application. Add your *Models* here.
  + ./Routes : Maps an HTTP request to a request handler (controller). Add your *Routes* here.
  + ./Services : Define bindings and inject dependencies. Add your *Service Providers* here.
  + ./Views : Any representation of information such as a chart, diagram or table. Add your *Twig Views* here
- /configs : Add/modify your *Configurations* here
- /public : Add your *Assets* files here

## Global Helpers

- `env(string $variable, string $default)` - Returns *environment* variables (using DotEnv)
- `app()` - Returns *App* instance
- `container(string $name)` - Returns bindings and inject dependencies from *Container*
- `configs(string $variable, string $default)` - Returns *Configs* data
- `base_path(string $path)` - Returns *base path* location
- `app_path(string $path)` - Returns *app path* location
- `configs_path(string $path)` - Returns *configs path* location
- `public_path(string $path)` - Returns *public path* location
- `storage_path(string $path)` - Returns *storage path* location

### Global Helpers for Development Only
- `d($var1, $var2, ...)` - Dump vars in collapsed mode by default
- `ddd($var1, $var2, ...)` - Dump & die vars in collapsed mode by default
- `de($var1, $var2, ...)` - Dump vars in expanded mode by default
- `dde($var1, $var2, ...)` - Dump & die vars in expanded mode by default

## Configurations

You can add as many configurations files as you want (`/configs`). These files *will be automatically preload and merged* in the container based on selected environment.

If you have an environment called "sandbox" and you want to overwrite some configuration only for this environment, you need to create a subfolder "sandbox" in `/configs`. Something like that `/configs/sandbox`. Then create the file that includes the configuration that you need to replace and the respective keys and values inside it.

`/configs/logger.php`
```php
return [

    'name' => 'app',

    'maxFiles' => 7,

];
```

`/configs/local/logger.php`
```php
return [

    'name' => 'app-local',

];
```

Results of `name` for the environment:
- prod : 'app'
- sandbox : 'app'
- **local : 'app-local'**
- testing : 'app'

**NOTE**: You can see the example in this framework for the *local* environment.

### Configurations Dot Notation

You can use **dot notation** to get values from configurations.

`/configs/example.php`
```php
return [

    'types' => [
        'mysql' => [
            'host' => 'localhost',
            'port' => '3306',
        ],
        'postgre' => [
            'host' => 'localhost',
            'port' => '3306',
        ],
    ],

];
```

If you want the `host` value for MySQL type:
```php
$this->getConfigs('example.types.mysql.host')  => 'localhost'

configs('example.types.mysql.host') => 'localhost'

container('configs')->get('example.types.mysql.host') => 'localhost'
```

## Routes

*Maps an HTTP request to a request handler (Closures or Controllers).*

You can add as many routes files as you want (`/app/Routes`), but you need to enable these files in `/apps/Routes/app.php` file.

You can organize this routes as you like. There is a little Demo that you can see how to organize this files.

```php
use App\Controllers\Demo\AddressesController;
use App\Controllers\Demo\HelloController;
use App\Controllers\Demo\HomeController;
use Slim\App;

/**
 * @var $app App
 */

$app->get('/', [(new HomeController()), 'index']);

$app->get('/flash', [(new HomeController()), 'flash']);

$app->get('/dump', [(new HomeController()), 'dump']);

$app->get('/hello/{name}', [(new HelloController()), 'index'])->setName('namedRoute');

$app->get('/addresses', [(new AddressesController()), 'list']);

$app->get('/addresses/pdo', [(new AddressesController()), 'pdo']);
```

## Controllers

*Accepts input and converts it to commands for the Models.*

You can add as many *Controllers* as you want in a cleaning way (`/app/Controllers`).

After add your *Controller*, you can enable or disable it in your *Routes*.

**NOTE**: To have helpers you must extend the *Controllers* with **ControllerAbstract** located in `\App\Kernel\Abstracts`.

```php
use App\Kernel\Abstracts\ControllerAbstract;
use Psr\Http\Message\ResponseInterface as Response;
use Psr\Http\Message\ServerRequestInterface as Request;
use Twig\Error\LoaderError;
use Twig\Error\RuntimeError;
use Twig\Error\SyntaxError;

class HomeController extends ControllerAbstract
{

    /**
     * Index Action
     *
     * @param Request $request
     * @param Response $response
     * @return Response
     * @throws LoaderError
     * @throws RuntimeError
     * @throws SyntaxError
     */
    public function index(Request $request, Response $response): Response
    {
        unset($request, $response);

        return $this->render('Demo/Home/index.twig');
    }
```

### Controllers Helpers

- `getApp()` - Returns *App* object
- `getContainer(string $name)` - Returns the App *Container*
- `getConfigs(string $name)` - Returns App *Configs*
- `getService(string $service)` - Returns *Service Provider* from container by name
- `getRequest()` - Returns *HTTP Request*
- `getResponse()` - Returns *HTTP Response*
- `setEmitter(string $name, string $emitter)` : Set a new *Response Emitter*
- `getView()` - Returns *Twig* object
- `render(string $template, array $data, bool $sendHeaders)` - Render *Twig* view

#### Flash Messages Helpers

- `getFlash()` - Return *Flash Messages* object
- `setFlashMessage(string $key, $message)` - Set *Flash Message*
- `hasFlashMessage(string $key)` - Verify if key of *Flash Messages* exists
- `getFlashMessages(string $key)` - Get one or all *Flash Messages*
- `getFlashFirstFMessage(string $key, $default)` - Get only the first *Flash Messages* of key

## Views

You can add as many *Views* as you want in a cleaning way (`/app/Views`).

You can organize the folder structure as you like, too.

We recommend to use `twig` extension in your files to have code highlighting.

#### Views Helpers

- `url_for()` - returns the URL for a given route. e.g.: /hello/world
- `full_url_for()` - returns the URL for a given route. e.g.: http://www.example.com/hello/world
- `is_current_url()` - returns true is the provided route name and parameters are valid for the current path
- `current_url()` - returns the *current path*, with or without the query string
- `get_uri()` - returns the `UriInterface` object from the incoming `ServerRequestInterface` object
- `base_path()` - returns the *base path*

You can use `url_for` to generate complete URLs to any Slim application named route and use `is_current_url` to determine if you need to mark a link as active as shown in this example Twig template:

```php
{% extends "layout.html" %}

{% block body %}
<h1>User List</h1>
<ul>
    <li><a href="{{ url_for('profile', { 'name': 'josh' }) }}" {% if is_current_url('profile', { 'name': 'josh' }) %}class="active"{% endif %}>Josh</a></li>
    <li><a href="{{ url_for('profile', { 'name': 'andrew' }) }}">Andrew</a></li>
</ul>
{% endblock %}
```

## Middlewares

*Provide a convenient mechanism for filtering HTTP requests entering your application.*

You can add as many *Middlewares* as you want in a cleaning way (`/app/Middlewares`).

After add your *Middleware*, you can enable or disable it in `configs/middlewares.php` configuration file.

**NOTE**: *Middlewares* must respect the **MiddlewareInterface** located in `\Psr\Http\Message`.

```php
use Psr\Http\Message\ResponseInterface as Response;
use Psr\Http\Message\ServerRequestInterface as Request;
use Psr\Http\Server\MiddlewareInterface as Middleware;
use Psr\Http\Server\RequestHandlerInterface as RequestHandler;

class ExampleMiddleware implements Middleware
{

    /**
     * Process an incoming server request.
     *
     * Processes an incoming server request in order to produce a response.
     * If unable to produce the response itself, it may delegate to the provided
     * request handler to do so.
     *
     * @param Request $request
     * @param RequestHandler $handler
     * @return Response
     */
    public function process(Request $request, RequestHandler $handler): Response
    {
        $response = $handler->handle($request);

        $response = $response->withHeader('X-Example', 'Middleware');

        return $response;
    }
}
```

Enable it in `configs/middlewares.php`:
```php
use App\Middlewares\Demo\ExampleMiddleware;

return [

    'example' => ExampleMiddleware::class,

];
```

## Response Emitters

*Emits a response, including status line, headers, and the message body, according to the environment.*

You can add as many *Response Emitters* as you want in a cleaning way (`/app/Emitters`).

After add your *Response Emitter*, you can enable or disable it **globally** in `configs/emitters.php` configuration or you can add it in the Controller for a **specific action**.

**NOTE**: *Response Emitters* must respect the **ResponseEmitterInterface** located in `\App\Kernel\Interfaces`.

```php
use App\Kernel\Interfaces\ResponseEmitterInterface;
use Psr\Http\Message\ResponseInterface;

class JsonResponseEmitter implements ResponseEmitterInterface
{

    /**
     * Send the response to the client
     *
     * @param ResponseInterface $response
     * @return ResponseInterface
     */
    public function emit(ResponseInterface $response): ResponseInterface
    {
        $response = $response
            ->withHeader('Content-Type', 'application/json; charset=UTF-8');

        return $response;
    }
}
```

Enable it in `configs/emitters.php`:
```php
use App\Emitters\JsonResponseEmitter;

return [

    'json' => JsonResponseEmitter::class,

];
```

## Models

*Manages the data, logic and rules of the application.*

You can add as many *Models* as you want in a cleaning way (`/app/Models`).

After add your *Models*, you use it for, for example, in a *Controller*.

**NOTE**: To have helpers you must extend the *Model* with **ModelAbstract** located in `\App\Kernel\Abstracts`.

```php
use App\Kernel\Abstracts\ModelAbstract;
use PDO;

class AddressesModel extends ModelAbstract
{

    /**
     * Get Last Addresses with Pdo
     *
     * @param int $limit
     * @return array
     */
    public function getLastWithPdo(int $limit = 25): array
    {
        /** @var $db PDO */
        $db = $this->getDb()->pdo;

        $sql = 'SELECT `address`.`address_id`,`address`.`address`,`address`.`address2`,`address`.`district`,`city`.`city`,`address`.`postal_code`,`address`.`phone` FROM `address` ';
        $sql .= 'LEFT JOIN `city` ON `address`.`city_id` = `city`.`city_id` ';
        $sql .= 'ORDER BY `address_id` DESC LIMIT 10';

        return $db->query($sql)->fetchAll(PDO::FETCH_ASSOC);
    }
```

### Models Helpers

- `getApp()` - Returns *App* object
- `getContainer(string $name)` - Returns the App *Container*
- `getConfigs(string $name)` - Returns App *Configs*
- `getService(string $service)` - Returns *Service Provider* from container by name
- `getRequest()` - Returns *HTTP Request*
- `getResponse()` - Returns *HTTP Response*
- `getDb()` - Returns *Database* object

## Services Providers

*Define bindings and inject dependencies.*

You can add as many *Services Providers* as you want in a cleaning way (`/app/Services`).

After add your *Services Provider*, you can enable or disable it in `configs/services.php` configuration file.

**NOTE**: *Service Providers* must respect the **ServiceProviderInterface** located in `\App\Kernel\Interfaces`.

```php
use App\Kernel\Interfaces\ServiceProviderInterface;
use Pimple\Container;

class ExampleServiceProvider implements ServiceProviderInterface
{

    /**
     * Service register name
     */
    public function name(): string
    {
        return 'example';
    }

    /**
     * Register new service on dependency container
     *
     * @param Container $container
     * @return mixed
     */
    public function register(Container $container)
    {
        return function (Container $c) {
            unset($c);

            return new Example();
        };
    }
}
```

Enable it in `configs/services.php`:
```php
use App\Services\Demo\ExampleServiceProvider;

return [

    'example' => ExampleServiceProvider::class,

];
```

## Handlers

*Handles specified behaviors of the application.*

You can override the following Handlers in a cleaning way (`/app/Handlers`):
- *ErrorHandler* (default located in `/app/Handlers/ErrorHandler`)
- *ShutdownHandler* (default located in `/app/Handlers/ShutdownHandler`)

After add your *Handler*, you can enable or disable it in `configs/app.php` configuration file.

```php
use App\Handlers\ShutdownHandler;
use Slim\Handlers\ErrorHandler;

return [

    // Handlers //
    'errorHandler' => ErrorHandler::class,

    'shutdownHandler' => ShutdownHandler::class,
```

## Database Support

*Medoo* is implemented out of box as a *Service Provider*. The use **is optional** and is not enabled by default.

To enable database support with *Medoo* you need to add this library/vendor with Composer:
```bash
composer require catfan/medoo
```

Once installed you need to enable the *Service Provider* in `configs/services.php`:
```php
use App\Services\Database\DatabaseServiceProvider;

return [

    'database' => DatabaseServiceProvider::class,

];
```

Now you are ready to use it...

If you need more details, documentation, api reference, please visit Medoo webpage:
[https://medoo.in/](https://medoo.in/)

**NOTES**:
- Don't forget to load PDO extensions for your database. For example, if you need MySQL, you need to install `pdo_mysql` PHP extensions.
- You can use another library as a *Service Provider* (Native drivers for MySQLi, PostgreSQL, MongoDB, Redis, ...).

## Exceptions

You have some *Exceptions* out the box, located in `\App\Kernel\Exceptions`, than you can use it:
```text
ConfigsException  - For Configurations Exceptions
ModelException    - For Models Exceptions
ViewException     - For ViewsExceptions
```

## Logging

Logging is enabled by default and you can see all the output in `/storage/logs/app-{date}.log`.

You can set this parameters in `/.env` or in `/configs/app.php`.

```text
LOG_ERRORS |  logErrors  (bool)  - Enable/Disable logging.

LOG_ERRORS_DETAILS  |  logErrorDetails  (bool)  - Enable/disable extra details in the logging file.

LOG_TO_OUTPUT  | logToOutput  (bool)  - `true` to output the logs in console, `false` to output logs in file.

```

## Debugging

Debugging is disabled by default. You can set this parameters in `/.env` or in `/configs/app.php`.

```text
APP_DEBUG  |  displayErrorDetails  (bool)  - Enable/disable debugging.
```

## Demo

This skeleton has a little Demo that you can see all this points in action.

Demo URL's:
```text
 /               - Hello World Example.
 /flash          - Redirect with Flash Message.
 /hello/{name}   - Greet User. Replace {name} with your name.
 /addresses      - Database example with Medoo
 /addresses/pdo  - Database example with PDO from Medoo
 /dump           - See the source code of this action.
```

## Benchmarks

Nothing is free, so let's compare the performance loss with Slim Skeleton.

**Machine:**<br/>
Intel® Core™ i5-8400 CPU @ 2.80GHz × 6<br>
16Gb RAM<br>
SSD<br>

**Versions:**<br/>
Ubuntu 20.04 LTS<br/>
Docker v19.03.8<br>
nginx 1.17.10<br/>
PHP v7.4.3<br/>
Zend OPcache enabled<br/>
SIEGE 4.0.4

**Bench Details:**<br/>
25 concurrent connections<br/>
500 requests per thread<br/>
No delays between requests<br/>
Command: siege -c25 -b -r500 "URL"<br/>
<br/>

|  | This Skeleton | Slim Skeleton |
| --- | :----: | :---: |
| Transactions | 12500 hits | 12500 hits |
| Availability | 100.00 % | 100.00 % |
| Elapsed time | 9.16 secs | 8.80 secs |
| Data transferred | 0.45 MB | 0.45 MB |
| Response time | 0.02 secs | 0.02 secs |
| Transaction rate | 1364.63 trans/sec | 1420.45 trans/sec |
| Throughput | 0.05 MB/sec | 0.05 MB/sec |
| Concurrency | 24.49 | 24.51|
| Successful transactions | 12500 | 12500 |
| Failed transactions | 0 | 0 |
| Longest transaction | 0.05 | 0.05 |
| Shortest transaction | 0.00 | 0.00 |
<br/>

___

### Enjoy the simplicity :oP
