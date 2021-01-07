<?php
declare(strict_types=1);

namespace App\Handlers;

use App\Handlers\Helpers\Severity;
use App\Handlers\Renderers\HtmlErrorRenderer;
use App\Handlers\Renderers\JsonErrorRenderer;
use App\Handlers\Renderers\PlainTextErrorRenderer;
use App\Handlers\Renderers\XmlErrorRenderer;
use Psr\Http\Message\ResponseInterface;
use Psr\Http\Message\ServerRequestInterface;
use Slim\Exception\HttpInternalServerErrorException;
use Slim\Factory\ServerRequestCreatorFactory;
use Slim\Handlers\ErrorHandler as SlimErrorHandler;
use Slim\ResponseEmitter;
use Throwable;

class ErrorHandler extends SlimErrorHandler
{

    /**
     * @var array
     */
    protected $errorRenderers = [
        'text/html' => HtmlErrorRenderer::class,
        'application/json' => JsonErrorRenderer::class,
        'text/json' => JsonErrorRenderer::class,
        'application/xml' => XmlErrorRenderer::class,
        'text/xml' => XmlErrorRenderer::class,
        'text/plain' => PlainTextErrorRenderer::class,
    ];


    /**
     * Better Handler for PHP Exceptions
     *
     * @param int $type
     * @param string $message
     * @param string $file
     * @param int $line
     */
    public function handleError(int $type, string $message, string $file, int $line): void
    {
        $configs = configs('app');

        $exceptionMsg = 'An error while processing your request';

        if ($configs['displayErrorDetails'] === true) {
            $exceptionMsg = (new Severity())->getSeverityMessage($type, $message, $file, $line);
        }

        $request = $this->createRequestObject();

        $response = $this->__invoke(
            $request,
            (new HttpInternalServerErrorException($request, $exceptionMsg)),
            $configs['displayErrorDetails'],
            $configs['logErrors'],
            $configs['logErrorDetails']
        );

        $this->registerResponseEmitters($response);

        exit(1);
    }

    /**
     * Better Handler for PHP Exceptions
     *
     * @param Throwable $exception
     */
    public function handleException(Throwable $exception): void
    {
        $configs = configs('app');

        $response = $this->__invoke(
            $this->createRequestObject(),
            $exception,
            $configs['displayErrorDetails'],
            $configs['logErrors'],
            $configs['logErrorDetails']
        );

        $this->registerResponseEmitters($response);
    }


    /**
     * Create Request Object
     *
     * @return ServerRequestInterface
     */
    protected function createRequestObject(): ServerRequestInterface
    {
        return $this->request ?? ServerRequestCreatorFactory::create()
                ->createServerRequestFromGlobals();
    }

    /**
     * Register Response Emitters
     *
     * @param ResponseInterface $response
     */
    protected function registerResponseEmitters(ResponseInterface $response): void
    {
        $emitters = configs('emitters');

        if (is_array($emitters) && !empty($emitters)) {
            foreach ($emitters as $emitter) {
                (new $emitter())->emit($response);
            }
        }

        if (ob_get_contents()) {
            ob_clean();
        }

        (new ResponseEmitter())->emit($response);
    }
}
