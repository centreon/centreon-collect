<?php
/**
 * CENTREON
 *
 * Source Copyright 2005-2016 CENTREON
 *
 * Unauthorized reproduction, copy and distribution
 * are not allowed.
 *
 * For more information : contact@centreon.com
 *
 */

require_once dirname(__FILE__) . '/../../../centreon-pluginpack-manager.conf.php';
require_once 'Module.php';
require_once MODULE_PATH . '/core/class/centreonPluginPack.class.php';

class CentreonPluginPack_EMS extends CentreonPluginPack_Module
{
    /**
     * Constructor
     *
     * @param CentreonDB $database The database connection
     * @param EmsApi $emsApiLib The call to EMS API
     */
    public function __construct($database, $emsApiLib)
    {
        $this->emsApiLib = $emsApiLib;
        parent::__construct($database);
    }

    /**
     *
     * @return CentreonPluginPack
     */
    public function getCentreonPluginPackObj()
    {
        return $this->centreonPluginPackObj;
    }


    /**
     * Get the list of installed pluginpacks and ordered
     *
     * @param array $aFilters the filters
     * @return array
     */
    public function getListInstalledOrdered($aFilters)
    {
        $newestInstalledPpFiles = array();
        $installedToUpgrade = array();
        $installedUpToDate = array();

        $ppInstalled = $this->getListInstalled();

        // check installed to update
        foreach ($ppInstalled as $installed) {
            $pluginPackFiles = $this->emsApiLib->getPluginPackFiles(
                EMS_PP_PATH . '/pluginpack_' . $installed["slug"] . '-*.json'
            );
            foreach ($pluginPackFiles as $name => $file) {
                $newestInstalledPpFiles[$name] = $file;
            }
        }

        // check files and update
        foreach ($newestInstalledPpFiles as $pluginPackName => $pluginPackFile) {
            $pluginPackInformation = $this->emsApiLib->readPluginPackFile($pluginPackFile, $aFilters);

            if (count($pluginPackInformation) != 0) {

                $comparedVersion = version_compare(
                    $ppInstalled[$pluginPackInformation['slug']]['version'],
                    $pluginPackInformation['version'],
                    '<'
                );

                $pluginPackInformation['current_version'] = $ppInstalled[$pluginPackInformation['slug']]['version'];
                $pluginPackInformation['complete'] = $ppInstalled[$pluginPackInformation['slug']]['complete'];

                $pluginPackInformation['installed'] = true;

                if ($comparedVersion == false) {
                    $pluginPackInformation['uptodate'] = true;
                    $installedUpToDate[] = $pluginPackInformation;
                } else {
                    $installedToUpgrade[] = $pluginPackInformation;
                }
            }
        }

        $InstalledPpOrdered = array_merge($installedToUpgrade, $installedUpToDate);

        return array_reverse($InstalledPpOrdered);
    }

    /**
     * Get the list of pluginpack
     *
     * @param int $offset - The offset of start the listing for pagination
     * @param int $limit - The number of elements for the listing for pagination
     * @param array $aFilters
     * @param bool $onlyUninstalled
     * @return array
     */
    public function getList($offset, $limit, $aFilters, $onlyUninstalled = true)
    {
        $available = array();

        $pluginPackFiles = $this->emsApiLib->getPluginPackFiles(EMS_PP_PATH . '/*.json');
        foreach ($pluginPackFiles as $pluginPackName => $pluginPackFile) {
            $pluginPackInformation = $this->emsApiLib->readPluginPackFile($pluginPackFile, $aFilters);
            if (!empty($pluginPackInformation)) {
                $available[$pluginPackInformation['name']] = $pluginPackInformation;
            }
        }
        ksort($available);

        $list = array_slice(array_values($available), $offset, $limit);

        if (empty($list)) {
            return null;
        }

        $returnList = array();
        foreach ($list as $pluginPack) {
            $pluginPackRet = array(
                'name' => $pluginPack['name'],
                'slug' => $pluginPack['slug'],
                'icon_file' => $pluginPack['icon_file'],
                'version' => $pluginPack['version'],
                'status' => $pluginPack['status'],
                'description' => $pluginPack['description'],
                'can_be_installed' => $pluginPack['can_be_installed'],
                'installed' => false,
                'uptodate' => false
            );
            $isInstalled = $this->isInstalled($pluginPackRet);
            if (!$onlyUninstalled || ($onlyUninstalled && !$isInstalled)) {
                $returnList[] = $pluginPackRet;
            }
        }

        return $returnList;
    }
}
