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

require_once 'Module.php';

class CentreonPluginPack_IMP extends CentreonPluginPack_Module
{
    /**
     * Constructor
     *
     * @param CentreonDB $database The database connection
     * @param ImpApi $impApiLib The call to IMP Rest API
     */
    public function __construct($database, $impApiLib)
    {
        $this->impApiLib = $impApiLib;
        parent::__construct($database);
    }

    /**
     * Get the list of pluginpack
     *
     * @param int $offset - The offset of start the listing for pagination
     * @param int $limit - The number of elements for the listing for pagination
     * @param array $aFilters - The filters for pluginpack list
     * @param bool $onlyUninstalled - The plugin pack type to return
     *
     * @return array The list of pluginpacks
     */
    public function getList($offset, $limit, $aFilters, $onlyUninstalled = true)
    {
        $list = $this->impApiLib->getListPluginPack(
            array(
                'offset' => $offset,
                'limit' => $limit
            ),
            $aFilters
        );

        if (empty($list['pluginpack'])) {
            return null;
        }

        $returnList = array();
        foreach ($list['pluginpack'] as $pluginPack) {
            $pluginPackRet = array(
                'name' => $pluginPack['attributes']['name'],
                'slug' => $pluginPack['attributes']['slug'],
                'icon_file' => $pluginPack['attributes']['icon'],
                'version' => $pluginPack['attributes']['version'],
                'status' => $pluginPack['attributes']['status'],
                'description' => $pluginPack['attributes']['description']['description'],
                'can_be_installed' => $pluginPack['attributes']['can_be_installed'],
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

    /**
     * Get the list of installed pluginpacks and ordered
     *
     */
    public function getListInstalledOrdered($aFilters)
    {
        $installedToUpgrade = array();
        $installedUpToDate = array();
        $aFilters['slugs'] = array();

        $ppInstalled = $this->getListInstalled();

        foreach ($ppInstalled as $installed) {
            $aFilters['slugs'][] = $installed['slug'];
        };

        $pluginPackFiles = $this->getList(0, count($ppInstalled), $aFilters, false);

        // check files and update
        foreach ($pluginPackFiles as $pluginPackInformation) {
            if (isset($ppInstalled[$pluginPackInformation['slug']])) {

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

        return $reversed = array_reverse($InstalledPpOrdered);
    }

}
