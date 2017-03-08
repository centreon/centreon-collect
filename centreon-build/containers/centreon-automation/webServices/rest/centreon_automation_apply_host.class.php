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
class CentreonAutomationApplyHost extends Webservice
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
        $this->CentreonConfigurationManager = $this->AutomationFactory->newCentreonConfiguration();
        $this->action = 'apply';
        parent::__construct();
    }

    /**
     *
     * @return array
     * @throws \Exception
     */
    public function postApply()
    {
        if (isset($this->arguments)) {
            $hostIp = $this->arguments;
        } else {
            throw new \RestBadRequestException("Missing 'host' parameter");
        }

        return $this->CentreonConfigurationManager->applyConfiguration($hostIp, '601');
    }
}
