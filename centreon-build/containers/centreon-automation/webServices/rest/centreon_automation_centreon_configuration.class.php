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
class CentreonAutomationCentreonConfiguration extends Webservice
{
    /**
     *
     * @var CentreonConfiguration
     */
    private $CentreonConfigurationManager;

    /**
     *
     */
    public function __construct()
    {
        $this->db = new \CentreonDB();
        $this->dbMonitoring = new \CentreonDB('centstorage');
        $this->AutomationFactory = new Factory($this->db, $this->dbMonitoring);
        $this->CentreonConfigurationManager = $this->AutomationFactory->newCentreonConfiguration();
        parent::__construct();
    }

    /**
     *
     */
    public function postSave()
    {
        if (isset($this->arguments['centreon_objects'])) {
            $centreonObjects = $this->arguments['centreon_objects'];
        } else {
            throw new \RestBadRequestException("Missing 'Centreon Objects' parameter");
        }

        $this->CentreonConfigurationManager->saveConfiguration($centreonObjects);

        return array();
    }

    /**
     *
     */
    public function postApply()
    {
        if (isset($this->arguments['centreon_objects'])) {
            $centreonObjects = $this->arguments['centreon_objects'];
        } else {
            throw new \RestBadRequestException("Missing 'Centreon Objects' parameter");
        }

        return $this->CentreonConfigurationManager->applyConfiguration($centreonObjects);
    }
}
