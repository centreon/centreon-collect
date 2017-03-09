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

namespace CentreonExport;

require_once _CENTREON_PATH_ . '/www/class/centreonDB.class.php';
require_once _CENTREON_PATH_ . '/www/class/centreonCommand.class.php';
require_once _CENTREON_PATH_ . '/www/class/centreonHosttemplates.class.php';
require_once _CENTREON_PATH_ . '/www/class/centreonServicetemplates.class.php';

use \CentreonExport\Export;
use \CentreonExport\PluginPack;
use \CentreonExport\Icon;
use \CentreonExport\Tag;
use \CentreonExport\Mapper\MapperPluginPack;

class Factory
{
    /**
     * 
     * @param type $db
     * @param type $dbMonitoring
     */
    public function __construct()
    {
        $this->db = new \CentreonDB();
    }

    /**
     * 
     * @return \CentreonExport\Export
     */
    public function newExport()
    {
        $iconObj = new Icon();
        return new Export($iconObj);
    }

    /**
     *
     * @return \CentreonExport\Export
     */
    public function newPluginPack()
    {
        $commandObj = new \CentreonCommand($this->db);
        $hostObj = new \CentreonHosttemplates($this->db);
        $serviceObj = new \CentreonServicetemplates($this->db);

        return new PluginPack($commandObj, $hostObj, $serviceObj);
    }

    /**
     *
     * @return \CentreonExport\Tag
     */
    public function newTag()
    {
        return new Tag();
    }

    /**
     *
     * @return \CentreonExport\Mapper\MapperPluginPack
     */
    public function newMapperPluginPack()
    {
        $pluginPackObj = $this->newPluginPack();
        $hostTemplateObj = new \CentreonHosttemplates($this->db);
        $serviceTemplateObj = new \CentreonServicetemplates($this->db);

        return new MapperPluginPack($pluginPackObj, $hostTemplateObj, $serviceTemplateObj);
    }
}
