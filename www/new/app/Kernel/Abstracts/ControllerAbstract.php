<?php
declare(strict_types=1);

namespace App\Kernel\Abstracts;

use App\Emitters\HtmlResponseEmitter;
use Psr\Http\Message\ResponseInterface as Response;
use Slim\Flash\Messages;
use Slim\Views\Twig;
use Twig\Error\LoaderError;
use Twig\Error\RuntimeError;
use Twig\Error\SyntaxError;

abstract class ControllerAbstract extends KernelAbstract
{

    /**
     * Set Emitter
     *
     * @param string $name
     * @param string $emitter
     */
    protected function setEmitter(string $name, string $emitter): void
    {
        $configs = $this->container['configs'];

        $configs->set('emitters', array_replace_recursive($configs->get('emitters'), [$name => $emitter]));
    }

    /**
     * Get View Engine
     *
     * @return Twig
     */
    protected function getView(): Twig
    {
        return $this->container['view'];
    }

    /**
     * Render view
     *
     * @param string $template
     * @param array $data
     * @param bool $sendHeaders
     * @return Response
     * @throws LoaderError
     * @throws RuntimeError
     * @throws SyntaxError
     */
    public function render(string $template, array $data = [], bool $sendHeaders = true): Response
    {
        if ($sendHeaders === true) {
            $this->setEmitter('html', HtmlResponseEmitter::class);
        }

        return $this->getView()->render($this->getResponse(), $template, $data);
    }

    /**
     * Get Flash
     *
     * @return Messages
     */
    protected function getFlash(): Messages
    {
        return $this->container['flash'];
    }

    /**
     * Set Flash Messages
     *
     * @param string $key
     * @param mixed $message
     */
    protected function setFlashMessage(string $key, $message): void
    {
        $this->container['flash']->addMessage($key, $message);
    }

    /**
     * Has Flash Message
     *
     * @param string $key
     * @return bool
     */
    protected function hasFlashMessage(string $key): bool
    {
        return $this->container['flash']->hasMessage($key);
    }

    /**
     * Get Flash Messages
     *
     * @param string|null $key
     * @return array|null
     */
    protected function getFlashMessages(string $key = null): ?array
    {
        $flash = $this->container['flash'];

        if ($key === null) {
            return $flash->getMessages();
        }

        return $flash->getMessage($key) ?? null;
    }

    /**
     * Get Flash First Message
     *
     * @param string $key
     * @param mixed $default
     * @return mixed|null
     */
    protected function getFlashFirstFMessage(string $key, $default = null)
    {
        return $this->container['flash']->getFirstMessage($key, $default);
    }
}
