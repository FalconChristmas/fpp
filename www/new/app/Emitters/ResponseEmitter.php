<?php
declare(strict_types=1);

namespace App\Emitters;

use App\Kernel\Interfaces\ResponseEmitterInterface;
use Psr\Http\Message\ResponseInterface;

class ResponseEmitter implements ResponseEmitterInterface
{

    /**
     * Send the response to the client
     *
     * @param ResponseInterface $response
     * @return ResponseInterface
     */
    public function emit(ResponseInterface $response): ResponseInterface
    {
        // This variable should be set to the allowed host from which your API can be accessed with
        $origin = isset($_SERVER['HTTP_ORIGIN']) ? $_SERVER['HTTP_ORIGIN'] : '';

        $response = $response
            ->withHeader('Access-Control-Allow-Credentials', 'true')
            ->withHeader('Access-Control-Allow-Origin', $origin)
            ->withHeader('Access-Control-Allow-Headers', 'X-Requested-With, Content-Type, Accept, Origin, Authorization')
            ->withHeader('Access-Control-Allow-Methods', 'GET, POST, PUT, PATCH, DELETE, OPTIONS')
            ->withHeader('Cache-Control', 'no-store, no-cache, must-revalidate, max-age=0')
            ->withAddedHeader('Cache-Control', 'post-check=0, pre-check=0')
            ->withHeader('Pragma', 'no-cache');

        return $response;
    }
}
