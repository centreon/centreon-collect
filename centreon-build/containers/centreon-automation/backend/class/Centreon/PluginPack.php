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

namespace CentreonAutomation\Centreon;

/**
 * Description of CentreonPluginPack
 *
 * @author Lionel Assepo <lassepo@centreon.com>
 */
class PluginPack
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
    protected $id;


    /**
     *
     */
    public function __construct($id)
    {
        $this->db = new CentreonDB();
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
     * @param bool $withTemplates
     * @return array
     */
    public static function getList($db, $withTemplates = false)
    {
        $finalPluginPackList = array();
        return $finalPluginPackList;
    }

    /**
     *
     * @return array
     */
    public function getLinkedTemplates()
    {
        $queryTemplates = "SELECT h.host_id, h.host_name "
            . "FROM host h, mod_ppm_pluginpack_host mpph "
            . "WHERE pluginpack_id = $this->id "
            . "AND mpph.host_id = h.host_id";

        $resultTemplatesQuery = $this->db->query($queryTemplates);

        $tempTemplatesList = array();
        while ($resultTemplates = $resultTemplatesQuery->fetchRow()) {
            $tempTemplatesList[] = $resultTemplates;
        }

        return $tempTemplatesList;
    }
}
