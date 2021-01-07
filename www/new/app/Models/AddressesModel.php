<?php
declare(strict_types=1);

namespace App\Models;

use App\Kernel\Abstracts\ModelAbstract;
use App\Kernel\Exceptions\ModelException;
use Medoo\Medoo;
use PDO;

class AddressesModel extends ModelAbstract
{

    /**
     * Get Last Addresses
     *
     * @param int $limit
     * @return array
     */
    public function getLast(int $limit = 25): array
    {
        /** @var $db Medoo */
        $db = $this->getDb();

        return $db->select(
            'address',
            [
                '[>]city' => ['city_id' => 'city_id'],
            ],
            [
                'address.address_id',
                'address.address',
                'address.address2',
                'address.district',
                'city.city',
                'address.postal_code',
                'address.phone'
            ],
            [
                'ORDER' =>
                    [
                        'address_id' => 'DESC'
                    ],
                'LIMIT' => $limit
            ]
        );
    }

    /**
     * Get Last Addresses with Pdo
     *
     * @param int $limit
     * @return array
     */
    public function getLastWithPdo(int $limit = 25): array
    {
        /** @var $db PDO */
        $db = $this->getDb()->pdo;

        $sql = 'SELECT `address`.`address_id`,`address`.`address`,`address`.`address2`,`address`.`district`,`city`.`city`,`address`.`postal_code`,`address`.`phone` FROM `address` ';
        $sql .= 'LEFT JOIN `city` ON `address`.`city_id` = `city`.`city_id` ';
        $sql .= 'ORDER BY `address_id` DESC LIMIT 10';

        return $db->query($sql)->fetchAll(PDO::FETCH_ASSOC);
    }

    /**
     * Model Exception
     *
     * @throws ModelException
     */
    public function exception(): void
    {
        throw new ModelException('Example Exception');
    }
}
