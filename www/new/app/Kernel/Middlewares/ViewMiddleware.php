<?php
declare(strict_types=1);

namespace App\Kernel\Middlewares;

use App\Kernel\Abstracts\TwigViewAbstract;
use Psr\Http\Message\ResponseInterface as Response;
use Psr\Http\Message\ServerRequestInterface as Request;
use Psr\Http\Server\MiddlewareInterface as Middleware;
use Psr\Http\Server\RequestHandlerInterface as RequestHandler;
use Slim\Views\TwigExtension;
use Slim\Views\TwigRuntimeLoader;

class ViewMiddleware implements Middleware
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
        $app = container('app');
        $basePath = $app->getSlimApp()->getBasePath();
        $routeParser = $app->getSlimApp()->getRouteCollector()->getRouteParser();

        $view = container('view');

        $runtimeLoader = new TwigRuntimeLoader($routeParser, $request->getUri(), $basePath);
        $view->addRuntimeLoader($runtimeLoader);

        $extension = new TwigExtension();
        $view->addExtension($extension);

        $customExtension = new TwigViewAbstract();
        $view->addExtension($customExtension);

        $view->getEnvironment()->addGlobal('flash', container('flash'));

        $request = $request->withAttribute('view', $view);

        return $handler->handle($request);
    }
}
