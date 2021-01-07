<?php
declare(strict_types=1);

namespace App\Kernel\Services\Logger\Handlers;

use DateTime;
use Exception;

/**
 * Stores logs to files that are rotated every day and a limited number of files are kept.
 *
 * usage example:
 *
 *   $log = new Logger('application');
 *   $rotating = new RotatingFileHandler("file.log", 7);
 *   $log->pushHandler($rotating);
 */
class RotatingFileHandler implements HandlerInterface
{

    /**
     * Max rotation files
     *
     * @var int
     */
    protected $maxFiles = 0;

    /**
     * File date format
     *
     * @var string
     */
    protected $dateFormat = 'Y-m-d';

    /**
     * Filename format
     *
     * @var string
     */
    protected $filenameFormat = '{filename}-{date}.log';

    /**
     * Log filename
     *
     * @var string
     */
    protected $filename;

    /**
     * Timed Log filename
     *
     * @var string
     */
    protected $timedFilename;


    /**
     * RotatingFileHandler constructor
     *
     * @param string $filename
     * @param int $maxFiles
     * @throws Exception
     */
    public function __construct(string $filename, int $maxFiles = 0)
    {
        $this->maxFiles = $maxFiles;
        $this->filename = $filename;

        $this->getTimedFilename();
        $this->rotate();
    }

    /**
     * Handle record
     *
     * @param array $record
     * @return bool
     * @throws Exception
     */
    public function handle(array $record): bool
    {
        return $this->write($record);
    }


    /**
     * Write file
     *
     * @param array $record
     * @return bool
     * @throws Exception
     */
    protected function write(array $record): bool
    {
        $data = '[' . $record['recorded'] . ']  ' . $record['channel'] . '.' . $record['level'] . '  ';

        $data .= $record['message'] . "\n";

        $data .= trim(implode('', $record['context']), "\n") . "\n\n";

        return file_put_contents($this->timedFilename, $data, FILE_APPEND) !== false;
    }

    /**
     * Get timed filename
     */
    protected function getTimedFilename(): void
    {
        $date = (new DateTime('now'))->format($this->dateFormat);

        $filename = str_replace('.log', '', basename($this->filename));

        $this->timedFilename = strtr(
            dirname($this->filename) . '/' . $this->filenameFormat,
            [
                '{filename}' => $filename,
                '{date}' => $date,
            ]
        );
    }

    /**
     * Rotates the files
     *
     * @throws Exception
     */
    protected function rotate(): void
    {
        $filename = $this->timedFilename;

        if (file_exists($filename)) {
            return;
        } elseif ($this->maxFiles === 0) {
            return;
        } elseif (is_writable(dirname($filename))) {
            touch($filename);
        }


        $logFiles = glob($this->getGlobPattern());

        if ($this->maxFiles >= count($logFiles)) {
            return;
        }


        rsort($logFiles);

        foreach (array_slice($logFiles, $this->maxFiles) as $file) {
            if (file_exists($file)) {
                unlink($file);
            }
        }
    }

    /**
     * Get Glob function pattern
     *
     * @return string
     */
    protected function getGlobPattern(): string
    {
        $date = (new DateTime('now'))->format($this->dateFormat);

        $filename = str_replace([$date, '.log'], '', $this->timedFilename);

        return $filename . '*.log';
    }
}
