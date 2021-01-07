<?php
declare(strict_types=1);

namespace App\Handlers\Renderers;

use Slim\Error\AbstractErrorRenderer;
use Throwable;
use function get_class;
use function htmlentities;
use function sprintf;

class PlainTextErrorRenderer extends AbstractErrorRenderer
{

    /**
     * @param Throwable $exception
     * @param bool $displayErrorDetails
     * @return string
     */
    public function __invoke(Throwable $exception, bool $displayErrorDetails): string
    {
        $text = "Application Error\n\n";

        if ($displayErrorDetails) {
            $text .= $this->formatExceptionFragment($exception);

            while ($exception = $exception->getPrevious()) {
                $text .= "\nPrevious Error:\n";
                $text .= $this->formatExceptionFragment($exception);
            }
        }

        return $text;
    }

    /**
     * @param Throwable $exception
     * @return string
     */
    protected function formatExceptionFragment(Throwable $exception): string
    {
        $text = sprintf("Type: %s\n", get_class($exception));

        $code = $exception->getCode();
        if ($code !== null) {
            $text .= sprintf("Code: %s\n", $code);
        }

        $message = $exception->getMessage();
        if ($message !== null) {
            $text .= sprintf("Message: %s\n", ucfirst(htmlentities($message)));
        }

        $file = $exception->getFile();
        if ($file !== null) {
            $text .= sprintf("File: %s\n", $file);
        }

        $line = $exception->getLine();
        if ($line !== null) {
            $text .= sprintf("Line: %s\n", $line);
        }

        $trace = $exception->getTraceAsString();
        if ($trace !== null) {
            $text .= sprintf("\nTrace:\n%s", $trace);
        }

        return $text;
    }
}
