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

namespace CentreonAutomation\Broker;

use CentreonClapi\CentreonHost as ClapiHost;

/**
 * Description of CentreonBrokerDiscoveryCommand
 *
 * @author Lionel Assepo <lassepo@centreon.com>
 */
class DiscoveryCommand
{
    /**
     *
     * @var array
     */
    private $arguments;

    /**
     *
     * @var array
     */
    private $endpoint;

    /**
     *
     * @var array
     */
    private $discoveryCommand = array();

    /**
     *
     * @param type $externalObject
     * @param type $brokerId
     * @param type $externalCommandGenerator
     * @param type $db
     * @param type $endpoint
     */
    public function __construct($externalObject, $brokerId, $externalCommandGenerator, $endpoint, $hostObj)
    {
        $this->externalObject = $externalObject;
        $this->brokerId = $brokerId;
        $this->externalCommandGenerator = $externalCommandGenerator;
        $this->endpoint = $endpoint;
        $this->hostObj = $hostObj;
    }

    /**
     *
     * @param string $command
     * @return array
     */
    protected function getCommandResult($type, $parameters = array(), $appendCommandSection = true)
    {
        $this->commandBuffer = array();

        if ($appendCommandSection) {
            $this->externalCommandGenerator->addCommandSection('broker_id', (int)$this->brokerId);
            $this->externalCommandGenerator->addCommandSection('endpoint', $this->endpoint);
            $this->externalCommandGenerator->addCommandSection('command_type', $type);

            foreach ($parameters as $key => $value) {
                $this->externalCommandGenerator->addCommandSection($key, $value);
            }
        }

        $command = $this->externalCommandGenerator->generateCommand();
        $commandResult = $this->externalObject->writeCommand($command);
        $commandFormattedResult = $this->processScanCommandResult($commandResult);

        return $commandFormattedResult;
    }

    /**
     *
     * @param array $commandBody
     * @return array
     */
    protected function executeCommand($commandBody, $partialResult = true)
    {
        $commandParameters = array(
            'command' => $commandBody,
            'with_partial_result' => $partialResult
        );

        $commandResult = $this->getCommandResult('execute', $commandParameters);

        return $commandResult;
    }

    /**
     *
     * @param integer $commandId
     * @param bool $partialResult
     * @return array
     */
    protected function statusCommand($commandId)
    {
        $commandParameters = array(
            'command_id' => $commandId
        );

        $commandResult = $this->getCommandResult('status', $commandParameters);

        if ($commandResult['completed'] == true) {
            $tmpCommandResult = $this->getCommandResult('status', $commandParameters, false);

            while (!is_null($tmpCommandResult['objects']) && !empty($tmpCommandResult['objects']) &&
                count($commandResult['objects']) < 10) {
                $commandResult['objects'] = array_merge($commandResult['objects'], $tmpCommandResult['objects']);
                $tmpCommandResult = $this->getCommandResult('status', $commandParameters, false);
            }

            if (count($commandResult['objects']) >= 10) {
                $commandResult['completed'] = false;
            }
        }

        $this->checkObjectsInCentreon($commandResult['objects']);

        return $commandResult;
    }


    /**
     * @param array $hostList
     * @return array
     */
    protected function checkObjectsInCentreon(&$hostList)
    {
        $hostObj = $this->hostObj;
        $filter = array('host_id', 'host_name');

        foreach ($hostList as &$host) {
            $centreonHosts = $hostObj->getHostByAddress($host['ip'], $filter);

            if (count($centreonHosts) > 0) {
                $this->checkServicesInCentreonByHost($centreonHosts, $host);
            } else {
                if ($host["fqdn"] == '$fqdn$') {
                    $host["fqdn"] = $host["ip"];
                }
                $host['nbService'] = '';
                $host['inCentreon'] = 0;
            }
        }
    }

    /**
     * @param array $centreonHosts
     * @param array $host
     * @return array
     */
    protected function checkServicesInCentreonByHost($centreonHosts, &$host)
    {
        foreach ($centreonHosts as $centreonHost) {
            $host['nbService'] = count($this->hostObj->getServices($centreonHost['host_id']));
            $host['fqdn'] = $centreonHost['host_name'];
            $host['inCentreon'] = 1;
        }
    }

    /**
     *
     * @param array $discoveryCommand
     */
    protected function addDiscoveryCommand($discoveryCommand)
    {
        $this->discoveryCommand[] = $discoveryCommand;
    }

    /**
     *
     * @param string $range
     * @return array
     */
    public function launchNetworkScan($range, $partialResult = true)
    {
        $rangeArray = explode(',', $range);
        $this->arguments['NETWORK'] = $rangeArray;
        $this->arguments['CENTREONPLUGINS'] = array('/usr/lib/centreon/plugins/');

        $this->addDiscoveryCommand(array(
            'command_name' => 'centreon-discovery-nmap',
            'command_result' => array(
                'tp_id' => 1,
                'type' => 'os',
                'ip' => '$ip_address$',
                'fqdn' => '$fqdn$'
            )
        ));

        $finalDiscoveryCommand = array(
            'arguments' => $this->arguments,
            'commands' => $this->discoveryCommand
        );

        // Send Execute Command
        $networkScanResult = $this->executeCommand($finalDiscoveryCommand, $partialResult);

        return $networkScanResult;
    }

    /**
     *
     * @param integer $commandId
     * @param bool $partialResult
     * @return array
     */
    public function getStatusScan($commandId)
    {
        $statusScanResult = $this->statusCommand($commandId);
        return $statusScanResult;
    }

    /**
     *
     * @param array $commandResult
     * @return array
     */
    private function processScanCommandResult($commandResult)
    {
        $processedResult = array();
        $processedResult['id'] = $commandResult['id'];

        if ($commandResult['return_code'] == 0) {
            $processedResult['completed'] = true;
        } else {
            $processedResult['completed'] = false;
        }

        if (isset($commandResult['output']['command_results'])) {
            $processedResult['objects'] = $commandResult['output']['command_results'];
        } else {
            $processedResult['objects'] = array();
        }

        return $processedResult;
    }

    /**
     *
     * @param array $objectList
     */
    public function launchQualificationScan($objectList)
    {
        $objectList = array(
            'objects' => array(
                '10.30.0.1',
                '10.30.0.2',
                '10.30.0.3',
                '10.30.0.4',
                '10.30.0.5'
            ),
            'equipments' => array(
                'database' => array(5, 6, 7)
            ),
        );

        $this->arguments['NETWORK'] = explode(',', $objectList['objects']);
    }

    /**
     *
     * @param integer $equipmentId
     */
    protected function getEquipmentParameters($equipmentId, $equipmentType)
    {
        $equipmentParameters = array();
        return $equipmentParameters;
    }
}
