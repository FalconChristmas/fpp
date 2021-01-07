<?php
declare(strict_types=1);

namespace App\Kernel\Services\Logger\Handlers;

interface HandlerInterface
{

    /**
     * Handles a record
     *
     * @param array $record
     * @return bool
     */
    public function handle(array $record): bool;
}
