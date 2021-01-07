<?php
declare(strict_types=1);

namespace App\Kernel\Interfaces;

use Psr\Http\Message\ResponseInterface;

interface ResponseEmitterInterface
{

    /**
     * Send the response to the client
     *
     * @param ResponseInterface $response
     * @return ResponseInterface
     */
    public function emit(ResponseInterface $response): ResponseInterface;
}