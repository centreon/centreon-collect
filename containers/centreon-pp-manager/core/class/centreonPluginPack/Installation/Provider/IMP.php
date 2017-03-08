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

namespace CentreonPluginPackManager\Installation\Provider;

/**
 * Description of IMP
 *
 * @author lionel
 */
class IMP implements \CentreonPluginPackManager\Installation\Provider\ProviderInterface
{
    /**
     *
     * @var CentreonDB
     */
    private $database;

    /**
     *
     * @var ImpAPI
     */
    private $impApi;

    /**
     * 
     * @param \CentreonDB $database
     */
    public function __construct($impApi)
    {
        $this->database = new \CentreonDB();
        $this->impApi = $impApi;
    }

    /**
     * 
     * @param array $pluginPackInfo
     * @return array
     */
    public function getPluginPackJsonFile($pluginPackInfo, $action = '')
    {
        $pluginPackVersion = $pluginPackInfo['version'];
        $pluginPackSlug = $pluginPackInfo['slug'];
        
        $pluginPackContent = $this->impApi->getPluginPack($pluginPackSlug, $pluginPackVersion, $action);
        if (is_string($pluginPackContent)) {
            $pluginPackContent = json_decode($pluginPackContent, true);
        }

        return $pluginPackContent;
    }
    
    /**
     * 
     * @param array $pluginPackInfo
     * @return array
     */
    public function getPluginPackJsonFileOfLastVersion($pluginPackInfo)
    {
        $pluginPackInfo['version'] = 'latest';
        return $this->getPluginPackJsonFile($pluginPackInfo);
    }
}
