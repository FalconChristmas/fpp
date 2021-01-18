<?php
declare(strict_types=1);

namespace App\Controllers;

use App\Emitters\JsonResponseEmitter;
use App\Emitters\PlainResponseEmitter;
use App\Kernel\Abstracts\ControllerAbstract;
use Psr\Http\Message\ResponseInterface as Response;
use Psr\Http\Message\ServerRequestInterface as Request;
use Twig\Error\LoaderError;
use Twig\Error\RuntimeError;
use Twig\Error\SyntaxError;

class DeviceInformationController extends ControllerAbstract
{

    /**
     * Deviceinformation Action
     *
     * @param Request $request
     * @param Response $response
     * @return Response
     * @throws LoaderError
     * @throws RuntimeError
     * @throws SyntaxError
     */
    public function index(Request $request, Response $response): Response
    {
        $guzzleClient = $this->getService('httpClient');
        $guzzleResponse = $guzzleClient->request('GET', 'http://192.168.178.19/fppjson.php?command=getFPPstatus');

        if ($response->getStatusCode() === 200) {
            return $this->render('components/deviceinformation/_deviceinformation.html.twig', [
                'items' => $this->mapdeviceinformation(json_decode($guzzleResponse->getBody()->getContents(), true))
            ]);
        }

        return $this->render('components/deviceinformation/_deviceinformation.html.twig', [
            'items' => []
        ]);
    }

    private function mapdeviceinformation(array $values): array
    {
        // Mode mapping
        switch ($values['mode']) {
            case 1:
                $values['mode'] = 'Bridge';
                break;
            case 2:
                $values['mode'] = 'Standalone';
                break;
            case 6:
                $values['mode'] = 'Master';
                break;
            case 8:
                $values['mode'] = 'Remote';
        }

        return [
            'mode' => [
                'label' => 'Mode',
                'value' => $values['mode'],
            ],
            'time' => [
                'label' => 'Time',
                'value' => $values['time'],
            ],
            'status' => [
                'label' => 'Status',
                'value' => '-',
            ],
            'host' => [
                'label' => 'Host',
                'value' => '-',
            ],
            'device' => [
                'label' => 'Device',
                'value' => '-',
            ],
            'cpu_temperature' => [
                'label' => 'CPU Temperature',
                'value' => '-',
            ],
            'player' => [
                'label' => 'Player',
                'value' => '-',
            ],
            'playlist' => [
                'label' => 'Next playlist',
                'value' => '-',
            ],
            'playlist_time' => [
                'label' => 'at time',
                'value' => '-',
            ],
        ];
    }
}
