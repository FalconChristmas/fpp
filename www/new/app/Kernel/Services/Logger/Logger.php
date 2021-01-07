<?php
declare(strict_types=1);

namespace App\Kernel\Services\Logger;

use App\Kernel\Services\Logger\Handlers\HandlerInterface;
use App\Kernel\Services\Logger\Handlers\RotatingFileHandler;
use DateTime;
use Exception;
use Pimple\Container;
use Psr\Log\AbstractLogger;

class Logger extends AbstractLogger
{

    /**
     * Channel name
     *
     * @var string
     */
    protected $channel = null;

    /**
     * The handler stack
     *
     * @var HandlerInterface[]
     */
    protected $handlers = [];

    /**
     * Logging levels from syslog protocol defined in RFC 5424
     *
     * @var int[]
     */
    protected $levelCodes = [
        'DEBUG' => 100,
        'INFO' => 200,
        'NOTICE' => 250,
        'WARNING' => 300,
        'ERROR' => 400,
        'CRITICAL' => 500,
        'ALERT' => 550,
        'EMERGENCY' => 600,
    ];


    /**
     * Logger constructor
     *
     * Logger constructor.
     * @param Container $container
     * @throws Exception
     */
    public function __construct(Container $container)
    {
        $configs = $container['configs']->get('logger');

        $this->channel = $configs['name'];

        $this->pushHandler(new RotatingFileHandler($configs['path'], $configs['maxFiles']));
    }

    /**
     * Get channel name
     *
     * @return string
     */
    public function getChannel(): string
    {
        return $this->channel;
    }

    /**
     * Set channel name
     *
     * @param string $channel
     */
    public function setChannel(string $channel): void
    {
        $this->channel = $channel;
    }

    /**
     * Pushes a handler on to the stack
     *
     * @param HandlerInterface $handler
     */
    public function pushHandler(HandlerInterface $handler): void
    {
        $this->handlers[get_class($handler)] = $handler;
    }

    /**
     * Unload a handler on to the stack
     *
     * @param string $handler
     */
    public function unloadHandler(string $handler): void
    {
        if (!empty($this->handlers[$handler])) {
            unset($this->handlers[$handler]);
        }
    }

    /**
     * Get handlers
     *
     * @return HandlerInterface[]
     */
    public function getHandlers(): array
    {
        return $this->handlers;
    }

    /**
     * Logs with an arbitrary level
     *
     * @param mixed $level
     * @param string $message
     * @param array $context
     * @throws Exception
     */
    public function log($level, $message, array $context = []): void
    {
        $this->addRecord($level, $message, $context);
    }


    /**
     * Adds a log record
     *
     * @param mixed $level
     * @param string $message
     * @param mixed[] $context
     * @throws Exception
     */
    protected function addRecord($level, string $message, array $context = []): void
    {
        $time = new DateTime('now');

        $record = [
            'message' => (string)$message,
            'context' => array_values($context),
            'level' => $this->levelCodes[strtoupper($level)] ?? '',
            'level_name' => strtoupper($level),
            'channel' => (string)$this->channel,
            'recorded' => $time->format(DATE_ISO8601),
        ];

        foreach ($this->handlers as $handler) {
            $handler->handle($record);
        }
    }
}
