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

namespace CentreonAutomation\Discovery;

use CentreonAutomation\Discovery\EquipmentType;
use CentreonAutomation\Centreon\PluginPack;

/**
 * Description of CentreonDiscoveryEquipment
 *
 * @author Lionel Assepo <lassepo@centreon.com>
 */
class EquipmentType
{
    /**
     *
     * @var type
     */
    protected $db;

    /**
     *
     * @var int
     */
    protected $id;

    /**
     *
     */
    public function __construct($id)
    {
        $this->db = new \CentreonDB();
        $this->id = $id;
    }

    /**
     *
     * @return int
     */
    public function getId()
    {
        return $this->id;
    }

    /**
     *
     * @param int $newId
     */
    public function setId($newId)
    {
        $this->id = $newId;
    }

    /**
     *
     * @param type $db
     * @param bool $withPluginPacks
     * @param bool $withTemplates
     * @return array
     */
    public static function getList($db, $withPluginPacks = false, $withTemplates = false)
    {
        $queryEquipmentTypes = "SELECT * FROM mod_ppm_discovery_category";
        $resultEquipmentTypesQuery = $db->query($queryEquipmentTypes);

        $tempEquipmentTypeList = array();
        while ($resultEquipmentTypes = $resultEquipmentTypesQuery->fetchRow()) {
            $tempEquipmentTypeList[] = $resultEquipmentTypes;
        }

        //
        $finalEquipmentTypeList = array();
        $myEquipmentType = new EquipmentType(0);
        foreach ($tempEquipmentTypeList as $equipmentType) {
            $linkedPluginPack = array();
            if ($withPluginPacks) {
                $myEquipmentType->setId($equipmentType['discovery_category_id']);
                $linkedPluginPack = $myEquipmentType->getLinkedPluginPacks($withTemplates);
            }
            $equipmentType['pluginPacks'] = $linkedPluginPack;
            $finalEquipmentTypeList[] = $equipmentType;
        }

        return $finalEquipmentTypeList;
    }

    /**
     *
     * @param bool $withTemplates
     * @return array
     */
    public function getLinkedPluginPacks($withTemplates = false)
    {
        $queryPluginPacks = "SELECT pluginpack_id "
            . "FROM mod_ppm_pluginpack "
            . "WHERE discovery_category = $this->id";

        $resultPluginPacksQuery = $this->db->query($queryPluginPacks);

        $tempPluginPackList = array();
        while ($resultPluginPacks = $resultPluginPacksQuery->fetchRow()) {
            $tempPluginPackList[] = $resultPluginPacks;
        }

        $finalPluginPackList = array();
        if ($withTemplates) {
            $myPluginPack = new CentreonPluginPack(0);
            foreach ($tempPluginPackList as $pluginPack) {
                $myPluginPack->setId($pluginPack['pluginpack_id']);
                $linkedTemplates = $myPluginPack->getLinkedTemplates();
                $pluginPack['templates'] = $linkedTemplates;
                $finalPluginPackList[] = $pluginPack;
            }
        }

        return $finalPluginPackList;
    }
}
