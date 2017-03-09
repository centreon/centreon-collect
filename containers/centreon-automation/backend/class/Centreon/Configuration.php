<?php

/*
 * Copyright 2016 lionel.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

namespace CentreonAutomation\Centreon;

/**
 * Description of CentreonConfiguration
 *
 * @author Lionel Assepo <lassepo@centreon.com>
 */
class Configuration
{
    /**
     *
     * @var type
     */
    private $hostManager;

    /**
     *
     * @var type
     */
    private $configurationManager;

    /**
     *
     * @var type
     */
    private $mainPollerId;

    /**
     *
     * @var type
     */
    private $db;

    /**
     *
     * @var type
     */
    private $dbMonitoring;

    /**
     *
     * @var type
     */
    public function __construct($db, $dbMonitoring)
    {
        global $oreon;

        $this->db = $db;
        $this->dbMonitoring = $dbMonitoring;

        $this->hostManager = new \CentreonClapi\CentreonHost();

        \CentreonClapi\CentreonUtils::setUserName($oreon->user->alias);

        $this->hostManager = new \CentreonClapi\CentreonHost();
        $this->configurationManager = new \CentreonClapi\CentreonConfigPoller(
            $this->db,
            _CENTREON_PATH_,
            $this->dbMonitoring
        );

        $this->loadMainPollerId();
    }

    /**
     *
     * @param array $objects
     */
    public function saveConfiguration($objects)
    {
        foreach ($objects as $object) {
            $hotsParam = explode(',', $object);
            $inCentreon = $hotsParam[0];

            $params = array();
            $params['host_name'] = $hotsParam[2];
            $params['host_alias'] = $hotsParam[2];
            $params['host_address'] = $hotsParam[1];
            $params['template'] = 'generic-active-host';
            $params['instance'] = 'Central';
            $params['hostgroup'] = 'Linux-Servers';

            if ($inCentreon == 0) {
                $this->hostManager->add(implode(';', $params));
                $this->hostManager->applytpl($params['host_name']);
            } else {
                $this->hostManager->addtemplate($params['host_name'] . ';' . $params['template']);
                $this->hostManager->applytpl($params['host_name']);
            }
        }
    }

    /**
     *
     * @param array $objects
     */
    public function applyConfiguration($objects, $redirection)
    {
        $this->saveConfiguration($objects);
        ob_start();
        $this->configurationManager->pollerGenerate($this->mainPollerId, '', '');
        $this->configurationManager->cfgMove($this->mainPollerId);
        $this->configurationManager->pollerTest('', $this->mainPollerId);
        $this->configurationManager->pollerReload($this->mainPollerId);
        //  $applyResult = explode("\n", ob_get_contents());
        $url = \CentreonUtils::visit($redirection, false);
        ob_end_clean();

        return $url;

        //  return $applyResult;
    }

    /**
     *
     */
    private function loadMainPollerId()
    {
        $pollerIdRequest = "SELECT id FROM nagios_server WHERE localhost = '1'";
        $resultPollerIdRequest = $this->db->query($pollerIdRequest);

        while ($resultPollerIdResult = $resultPollerIdRequest->fetchRow()) {
            $this->mainPollerId = (int)$resultPollerIdResult['id'];
        }

        if (is_null($this->mainPollerId)) {
            throw new \Exception("No central poller found", 500);
        }
    }
}
