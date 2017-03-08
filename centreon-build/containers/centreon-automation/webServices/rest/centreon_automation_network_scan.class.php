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

/**
 *
 */
class CentreonAutomationNetworkScan extends Webservice
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
     * @var type
     */
    protected $discoveryManager;

    /**
     *
     * @var type
     */
    protected $action;

    /**
     *
     */
    public function __construct()
    {
        $this->db = new \CentreonDB();
        $this->dbMonitoring = new \CentreonDB('centstorage');
        $this->AutomationFactory = new Factory($this->db, $this->dbMonitoring);
        $this->discoveryManager = $this->AutomationFactory->newCentreonBrokerDiscoveryCommand();
        $this->action = 'scan';
        parent::__construct();
    }

    /**
     *
     * @return array
     * @throws \Exception
     */
    public function postScan()
    {
        if (isset($this->arguments)) {
            $range = $this->arguments;
        } else {
            throw new \RestBadRequestException("Missing 'Range' parameter");
        }

        $commandResult = $this->discoveryManager->launchNetworkScan($range);
        return $commandResult;
    }

    /**
     *
     * @return array
     * @throws RestBadRequestException
     */
    public function postStatus()
    {
        if (isset($this->arguments["range"]['id'])) {
            $commandId = $this->arguments["range"]['id'];
        } else {
            throw new \RestBadRequestException("Missing 'Command Id' parameter");
        }

        $commandResult = $this->discoveryManager->getStatusScan($commandId);
        return $commandResult;
    }
}
