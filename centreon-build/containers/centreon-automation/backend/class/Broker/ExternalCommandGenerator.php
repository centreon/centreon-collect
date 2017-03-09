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
class ExternalCommandGenerator
{
    /**
     *
     * @var array
     */
    protected $commandBuffer;

    /**
     *
     * @param string $key
     * @param mixed $value
     * @param boolean $override
     * @throws Exception
     */
    public function addCommandSection($key, $value, $override = false)
    {

        if (isset($this->commandBuffer[$key]) && !$override) {
            throw new \Exception("The key already exists", 500);
        } else {
            $this->commandBuffer[$key] = $value;
        }
    }

    /**
     *
     * @param string $key
     * @return mixed $value
     */
    public function getCommandSectionValue($key)
    {
        return $this->commandBuffer[$key];
    }

    /**
     *
     * @return array
     */
    public function generateCommand()
    {
        $finalCommand = stripslashes(json_encode($this->commandBuffer));
        return $finalCommand;
    }
}
