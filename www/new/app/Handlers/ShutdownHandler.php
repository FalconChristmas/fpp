<?php
declare(strict_types=1);

namespace App\Handlers;

use App\Handlers\Helpers\Severity;
use Psr\Http\Message\ResponseInterface;
use Psr\Http\Message\ServerRequestInterface as Request;
use Slim\Exception\HttpInternalServerErrorException;
use Slim\Handlers\ErrorHandler;
use Slim\ResponseEmitter;

class ShutdownHandler
{

    /**
     * @var Request
     */
    protected $request;

    /**
     * @var ErrorHandler
     */
    protected $errorHandler;

    /**
     * @var bool
     */
    protected $displayErrorDetails;

    /**
     * @var bool
     */
    protected $logErrors;

    /**
     * @var bool
     */
    protected $logErrorDetails;


    /**
     * ShutdownHandler constructor
     *
     * @param Request $request
     * @param ErrorHandler $errorHandler
     * @param bool $displayErrorDetails
     * @param bool $logErrors
     * @param bool $logErrorDetails
     */
    public function __construct(
        Request $request,
        ErrorHandler $errorHandler,
        bool $displayErrorDetails,
        bool $logErrors,
        bool $logErrorDetails
    )
    {
        $this->request = $request;
        $this->errorHandler = $errorHandler;
        $this->displayErrorDetails = $displayErrorDetails;
        $this->logErrors = $logErrors;
        $this->logErrorDetails = $logErrorDetails;
    }

    /**
     * Class Invoke
     */
    public function __invoke(): void
    {
        $error = error_get_last();

        if ($error !== null) {
            $errorFile = $error['file'] ?? null;
            $errorLine = $error['line'] ?? null;
            $errorType = $error['type'];
            $errorMessage = $error['message'];

            $message = 'An error while processing your request';

            if ($this->displayErrorDetails === true) {
                $message = (new Severity())->getSeverityMessage($errorType, $errorMessage, $errorFile, $errorLine);
            }

            $response = $this->errorHandler->__invoke(
                $this->request,
                (new HttpInternalServerErrorException($this->request, $message)),
                $this->displayErrorDetails,
                $this->logErrors,
                $this->logErrorDetails
            );

            $this->registerResponseEmitters($response);
        }
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
