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
use CentreonAutomation\Broker\DiscoveryCommand;

/**
 *
 */
class CentreonAutomationQualificationScan extends Webservice
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
        $this->discoveryManager = new DiscoveryCommand($this->db);
        $this->action = 'scan';
        parent::__construct();
    }

    /**
     *
     * @return array
     * @throws \Exception
     */
    public function getScan()
    {
        if (isset($this->arguments['network_objects'])) {
            $networkObjects = $this->arguments['network_objects'];
        } else {
            throw new \RestBadRequestException("Missing 'Network Objects' parameter");
        }

        $networkObjects = array();

        $commandResult = $this->discoveryManager->launchQualificationScan($networkObjects);

        return $commandResult;
    }

    /**
     *
     * @return array
     * @throws RestBadRequestException
     */
    public function getStatus()
    {
        if (isset($this->arguments['command_id'])) {
            $commandId = $this->arguments['command_id'];
        } else {
            throw new \RestBadRequestException("Missing 'Command Id' parameter");
        }

        $commandResult = $this->discoveryManager->getStatusCan($commandId);

        return $commandResult;
    }
}
