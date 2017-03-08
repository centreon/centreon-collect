<?php
/**
 * Copyright 2016 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

require_once realpath(dirname(__FILE__) . '/../common.php');
use CentreonAutomation\Webservice;
use CentreonAutomation\Factory;
use CentreonAutomation\Discovery\EquipmentType;

/**
 *
 */
class CentreonAutomationEquipment extends Webservice
{
    /**
     *
     * @var type
     */
    protected $db;

    /**
     *
     * @var type
     */
    protected $dbMonitoring;

    /**
     *
     */
    public function __construct()
    {
        $this->db = new \CentreonDB();
        $this->dbMonitoring = new \CentreonDB('centstorage');
        parent::__construct();
    }

    /**
     *
     * @return array
     */
    public function getList()
    {
        $equipmentList = EquipmentType::getList($this->db, true, true);
        return $equipmentList;
    }
}
