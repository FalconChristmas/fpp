<?php
declare(strict_types=1);

namespace App\Handlers\Renderers;

use Slim\Error\AbstractErrorRenderer;
use Slim\Exception\HttpException;
use Throwable;
use function get_class;
use function sprintf;
use function str_replace;

class XmlErrorRenderer extends AbstractErrorRenderer
{

    /**
     * @param Throwable $exception
     * @param bool $displayErrorDetails
     * @return string
     */
    public function __invoke(Throwable $exception, bool $displayErrorDetails): string
    {
        $xml = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\" ?>\n";
        $xml .= "<error>\n";
        $xml .= '<message>' . $this->createCdataSection($this->getErrorTitle($exception)) . "</message>\n";

        if ($displayErrorDetails) {
            $xml .= "  <exception>\n";
            $xml .= '    <type>' . get_class($exception) . "</type>\n";
            $xml .= '    <code>' . $exception->getCode() . "</code>\n";
            $xml .= '    <message>' . $this->createCdataSection($exception->getMessage()) . "</message>\n";
            $xml .= '    <file>' . $exception->getFile() . "</file>\n";
            $xml .= '    <line>' . $exception->getLine() . "</line>\n";
            $xml .= "  </exception>\n";

            $xml .= "  <traces>\n";
            $xml .= $this->traceToXml($exception);
            $xml .= "  </traces>\n";
        }

        $xml .= '</error>';

        return $xml;
    }

    /**
     * Get Error Title
     *
     * @param Throwable $exception
     * @return string
     */
    protected function getErrorTitle(Throwable $exception): string
    {
        if ($exception instanceof HttpException) {
            return $exception->getTitle();
        }

        return 'Application Error';
    }

    /**
     * Returns a CDATA section with the given content.
     *
     * @param string $content
     * @return string
     */
    protected function createCdataSection(string $content): string
    {
        return sprintf('<![CDATA[%s]]>', str_replace(']]>', ']]]]><![CDATA[>', $content));
    }

    /**
     * Convert Trace To Xml
     *
     * @param Throwable $exception
     * @return string
     */
    protected function traceToXml(Throwable $exception): string
    {
        $xml = '';

        foreach (explode("\n", $exception->getTraceAsString()) as $trace) {
            $trace = $this->createCdataSection($trace);

            $xml .= "    <trace>{$trace}</trace>\n";
        }

        return $xml;
    }
}
