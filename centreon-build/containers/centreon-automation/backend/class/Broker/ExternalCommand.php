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

/**
 * Description of CentreonBrokerExternalCommand
 *
 * @author Lionel Assepo <lassepo@centreon.com>
 */
class ExternalCommand
{
    /**
     *
     * @var string
     */
    protected $commandFile;

    /**
     *
     * @var array
     */
    protected $commandBuffer;

    /**
     *
     * @param type $db
     */
    public function __construct($commandFile)
    {
        $this->commandFile = $commandFile;
    }

    /**
     *
     * @param string $command
     * @param string $commandFile
     * @return array
     * @throws \Exception
     */
    public function writeCommand($command)
    {
        global $oreon;
        $socketPath = 'unix://' . $this->commandFile;
        //ob_start();
        try {
            $stream = stream_socket_client($socketPath, $errno, $errstr, 10);
        } catch (\Exception $e) {
            $stream = false;
        }
        //ob_end_clean();
        if (false === $stream) {
            throw new \Exception("Error to connect to the socket.");
        }

        fwrite($stream, $command . "\n");
        $rStream = array($stream);
        $wStream = null;
        $eStream = null;
        $nbStream = stream_select($rStream, $wStream, $eStream, 5);
        if (false === $nbStream || 0 === $nbStream) {
            fclose($stream);
            throw new \Exception("Error to read the socket.");
        }

        // Reading Socket to get the command response
        $instantResponse = fread($stream, 4096);
        fclose($stream);

        if ($instantResponse === false) {
            throw new \Exception("Error while reading response from broker");
        }

        $commandReturn = json_decode($instantResponse, true);

        if (is_null($commandReturn)) {
            throw new \Exception("Bad response format from broker : " . $instantResponse);
        }

        // Throwing exception if the command is unsuccessful
        if ($commandReturn['command_code'] === -1) {
            throw new \Exception("Error when execute command : " . $commandReturn['command_output']);
        }

        return array(
            'id' => $commandReturn['command_id'],
            'output' => $commandReturn['command_output'],
            'return_code' => $commandReturn['command_code']
        );
    }
}
