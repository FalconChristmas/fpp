<?php
declare(strict_types=1);

namespace App\Emitters;

use App\Kernel\Interfaces\ResponseEmitterInterface;
use Psr\Http\Message\ResponseInterface;

class JsonResponseEmitter implements ResponseEmitterInterface
{

    /**
     * Send the response to the client
     *
     * @param ResponseInterface $response
     * @return ResponseInterface
     */
    public function emit(ResponseInterface $response): ResponseInterface
    {
        $response = $response
            ->withHeader('Content-Type', 'application/json; charset=UTF-8');

        return $response;
    }
}
