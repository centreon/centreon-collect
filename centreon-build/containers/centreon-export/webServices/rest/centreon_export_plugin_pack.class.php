<?php
/*
 * Copyright 2005-2015 Centreon
 * Centreon is developped by : Julien Mathis and Romain Le Merlus under
 * GPL Licence 2.0.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation ; either version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, see <http://www.gnu.org/licenses>.
 *
 * Linking this program statically or dynamically with other modules is making a
 * combined work based on this program. Thus, the terms and conditions of the GNU
 * General Public License cover the whole combination.
 *
 * As a special exception, the copyright holders of this program give Centreon
 * permission to link this program with independent modules to produce an executable,
 * regardless of the license terms of these independent modules, and to copy and
 * distribute the resulting executable under terms of Centreon choice, provided that
 * Centreon also meet, for each linked independent module, the terms  and conditions
 * of the license of that module. An independent module is a module which is not
 * derived from this program. If you modify this program, you may extend this
 * exception to your version of the program, but you are not obliged to do so. If you
 * do not wish to do so, delete this exception statement from your version.
 *
 * For more information : contact@centreon.com
 *
 */


require_once realpath(dirname(__FILE__) . '/../common.php');
require_once _CENTREON_PATH_ . 'www/class/centreonHosttemplates.class.php';
require_once _CENTREON_PATH_ . 'www/class/centreonServicetemplates.class.php';

use \CentreonExport\Factory;
use \CentreonExport\Export;
use \CentreonExport\PluginPack;
use \CentreonExport\Dependency;
use \CentreonExport\Command;
use \CentreonExport\HostTemplate;
use \CentreonExport\ServiceTemplate;
use \CentreonExport\Services\ListCommand;
use \CentreonExport\Services\ListPluginPack;

class CentreonExportPluginPack extends CentreonWebService
{
    protected $hostTemplateObject;
    protected $serviceTemplateObject;

    /**
     * 
     */
    public function __construct()
    {
        $this->db = new \CentreonDB();
        $this->hostTemplateObject = new \CentreonHosttemplates($this->db);
        $this->serviceTemplateObject = new \CentreonServicetemplates($this->db);
        $this->ExportFactory = new Factory();
        parent::__construct();
    }

    /**
     *
     * @return array
     */
    public function getList()
    {
        // Check for select2 'q' argument
        if (false === isset($this->arguments['q'])) {
            $q = '';
        } else {
            $q = $this->arguments['q'];
        }

        if (false === isset($this->arguments['id'])) {
            $id = '';
        } else {
            $id = $this->arguments['id'];
        }

        if (isset($this->arguments['page_limit']) && isset($this->arguments['page'])) {
            $limit = ($this->arguments['page'] - 1) * $this->arguments['page_limit'];
            $range = 'LIMIT ' . $limit . ',' . $this->arguments['page_limit'];
        } else {
            $range = '';
        }

        $queryPlugin = "SELECT SQL_CALC_FOUND_ROWS DISTINCT plugin_id, plugin_name "
            . "FROM `mod_export_pluginpack` "
            . "WHERE plugin_name LIKE '%$q%' "
            . "ORDER BY plugin_name "
            . $range;

        $DBRESULT = $this->pearDB->query($queryPlugin);

        $total = $this->pearDB->numberRows();

        $pluginList = array();
        while ($data = $DBRESULT->fetchRow()) {
            $pluginList[] = array(
                'id' => $data['plugin_id'],
                'text' => $data['plugin_name']
            );
        }

        return $pluginList;
    }

    /**
     *
     * @return array
     */
    public function getAll()
    {
        // Check for select2 'q' argument
        if (false === isset($this->arguments['q'])) {
            $q = '';
        } else {
            $q = $this->arguments['q'];
        }

        if (isset($this->arguments['page_limit']) && isset($this->arguments['page'])) {
            $limit = ($this->arguments['page'] - 1) * $this->arguments['page_limit'];
            $range = 'LIMIT ' . $limit . ',' . $this->arguments['page_limit'];
        } else {
            $range = '';
        }

        $queryTags = "SELECT SQL_CALC_FOUND_ROWS DISTINCT tags_id, tags_name "
            . "FROM `mod_export_tags` "
            . "WHERE tags_name LIKE '%$q%' "
            . "ORDER BY tags_name "
            . $range;

        $DBRESULT = $this->pearDB->query($queryTags);

        $total = $this->pearDB->numberRows();

        $tagsList = array();
        while ($data = $DBRESULT->fetchRow()) {
            $tagsList[] = array(
                'id' => $data['tags_id'],
                'text' => $data['tags_name']
            );
        }

        return array(
            'items' => $tagsList,
            'total' => $total
        );
    }

    /**
     * Add a version
     * @return array
     */
    public function postAddVersion()
    {
        $pluginPack = $this->ExportFactory->newPluginPack();

        $aHostTemplate = $this->hostTemplateObject->getList(true, true);
        $aServiceTemplate = $this->serviceTemplateObject->getList(true);

        /* Update old version */
        $pluginPack->getDetail($this->arguments['pluginpack_id'], $aHostTemplate, $aServiceTemplate);

        /* Set version information */
        $pluginPack->setPlugin_version($this->arguments['version']);
        $pluginPack->setPlugin_status($this->arguments['status']);
        $pluginPack->setPlugin_status_message($this->arguments['status_message']);
        $pluginPack->setPlugin_change_log($this->arguments['changelog']);
        $pluginPack->setPlugin_time_generate(time());
        $pluginPack->update();

        $exportObject = $this->ExportFactory->newExport();
        return array(
            'success' => true,
            'data' => $exportObject->exportConfiguration($pluginPack)
        );
    }

    /**
     * Test is a version exists
     * @return boolean
     * @throws RestInternalServerErrorException
     * @throws RestNotFoundException
     */
    public function postHasVersion()
    {
        $query = "SELECT COUNT(*) as nb FROM mod_export_pluginpack WHERE plugin_version = '" .
            $this->pearDB->escape($this->arguments['version']) . "' AND plugin_id = " .
            $this->arguments['id'];

        $res = $this->pearDB->query($query);
        if (\PEAR::isError($res)) {
            throw new \RestInternalServerErrorException();
        }

        $row = $res->fetchRow();
        if ($row['nb'] == 0) {
            return false;
        }
        return true;
    }

    /**
     * Add a version
     * @return array
     */
    public function postExportAll()
    {
        $incrementVersion = isset($this->arguments['increment_version']) ? $this->arguments['increment_version'] : false;
        $changelog = isset($this->arguments['changelog']) ? $this->arguments['changelog'] : '';

        $exportParameters = array();

        $hostTemplates = $this->hostTemplateObject->getList(true, true);
        $serviceTemplates =  $this->serviceTemplateObject->getList(true);

        $pluginPackObj = $this->ExportFactory->newPluginPack();
        $pluginPacks = $pluginPackObj->getList(array('plugin_id'));
        foreach ($pluginPacks as $pluginPack) {
            $pluginPackObj = $this->ExportFactory->newPluginPack();

            /* Get PP information */
            $pluginPackObj->getDetail($pluginPack['plugin_id'], $hostTemplates, $serviceTemplates);

            /* Get current PP version */
            $version = $pluginPackObj->getPlugin_version();

            /* Do not export PP which have never been exported */
            if (!isset($version) || $version == '' || !preg_match('/^\d+\.\d+\.\d+$/', $version)) {
                continue;
            }

            /* Increment version if necessary */
            if ($incrementVersion) {
                $explodedVersion = explode('.', $version);
                $incrementedVersion = $explodedVersion[0] . '.' . $explodedVersion[1] . '.' . ($explodedVersion[2] + 1);
                $pluginPackObj->setPlugin_version($incrementedVersion);
                $pluginPackObj->setPlugin_change_log($changelog);
                $pluginPackObj->setPlugin_time_generate(time());
            }

            /* Update PP in database */
            $pluginPackObj->update();

            /* Format returned JSON */
            $exportObj = $this->ExportFactory->newExport();
            $exportParameters[] = $exportObj->exportConfiguration($pluginPackObj);
        }

        return array(
            'success' => true,
            'data' => $exportParameters
        );
    }

    /**
     * Get versions of a plugin pack
     * @return array
     */
    public function getVersions()
    {
        $oPluginPackList = new ListPluginPack();

        $versions = $oPluginPackList->getVersion($this->arguments['slug']);

        return array(
            'versions' => $versions,
            'count' => count($versions)
        );
    }
}
