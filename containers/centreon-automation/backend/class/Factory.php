<?php
/*
 * Copyright 2016 Centreon.
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


/**
 * Description of CentreonBrokerDiscoveryCommand
 *
 * @author Lionel Assepo <lassepo@centreon.com>
 */

namespace CentreonAutomation;

use CentreonAutomation\Broker\DiscoveryCommand;
use CentreonAutomation\Broker\ExternalCommand;
use CentreonAutomation\Broker\ExternalCommandGenerator;
use CentreonAutomation\Centreon\Configuration;

class Factory
{
    /**
     *
     * @param type $db
     * @param type $dbMonitoring
     */
    public function __construct($db, $dbMonitoring)
    {
        $this->db = $db;
        $this->dbMonitoring = $dbMonitoring;
    }

    /**
     *
     * @return \CentreonHost
     */
    public function newCentreonHost()
    {
        return new \CentreonHost($this->db);
    }


    /*************************************************************************************/
//                                         Broker                                    //
    /*************************************************************************************/

    /**
     *
     * @return \CentreonAutomation\CentreonBrokerExternalCommand
     */
    public function newCentreonBrokerExternalCommand()
    {

        $this->loadCentreonBrokerId();
        $endpointQuery = "SELECT config_value
            FROM cfg_centreonbroker_info
            WHERE config_key = 'name'
            AND config_group_id = (SELECT config_group_id
                                  FROM cfg_centreonbroker_info
                                  WHERE config_key = 'type'
                                  AND config_value ='discovery')";
        $stmt = $this->db->query($endpointQuery);
        $endpoint = $stmt->fetchRow();
        $this->endpoint = $endpoint["config_value"];
        return new ExternalCommand($this->commandFile);
    }

    /**
     *
     * @return ExternalCommandGenerator
     */
    public function newCentreonBrokerExternalCommandGenerator()
    {
        return new ExternalCommandGenerator();
    }

    /**
     *
     * @throws \Exception
     */
    public function loadCentreonBrokerId()
    {
        $pollerIdRequest = "SELECT id FROM nagios_server WHERE localhost = '1'";
        $resultPollerIdRequest = $this->db->query($pollerIdRequest);
        while ($resultPollerIdResult = $resultPollerIdRequest->fetchRow()) {
            $this->pollerId = $resultPollerIdResult['id'];
        }

        if (is_null($this->pollerId)) {
            throw new \Exception("No central poller found", 500);
        }

        //load brokerid
        $brokerIdRequest = "SELECT config_id, command_file"
            . " FROM cfg_centreonbroker"
            . " WHERE ns_nagios_server = $this->pollerId"
            . " AND config_name = 'central-broker-master'";

        $resultBrokerIdRequest = $this->db->query($brokerIdRequest);

        while ($resultBrokerIdResult = $resultBrokerIdRequest->fetchRow()) {
            $this->brokerId = $resultBrokerIdResult['config_id'];
            $this->commandFile = $resultBrokerIdResult['command_file'];
        }

        if (is_null($this->brokerId)) {
            throw new \Exception("No central broker found", 500);
        }
    }

    /**
     *
     * @return DiscoveryCommand
     */
    public function newCentreonBrokerDiscoveryCommand()
    {
        $this->loadCentreonBrokerId();
        $externalCommand = $this->newCentreonBrokerExternalCommand();
        $externalCommandGenerator = $this->newCentreonBrokerExternalCommandGenerator();
        $hostObj = $this->newCentreonHost();

        return new DiscoveryCommand(
            $externalCommand,
            $this->brokerId,
            $externalCommandGenerator,
            $this->endpoint,
            $hostObj
        );
    }


/*************************************************************************************/
//                                     Centreon                                      //
/*************************************************************************************/

    /**
     *
     * @return Configuration
     */
    public function newCentreonConfiguration()
    {
        return new Configuration($this->db, $this->dbMonitoring);
    }
}
