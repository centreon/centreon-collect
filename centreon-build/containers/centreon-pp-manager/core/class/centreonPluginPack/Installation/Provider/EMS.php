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
 * Description of EMS
 *
 * @author lionel
 */
class EMS implements \CentreonPluginPackManager\Installation\Provider\ProviderInterface
{
    /**
     *
     * @var CentreonDB
     */
    private $database;

    /**
     *
     * @var System
     */
    private $system;

    /**
     * 
     * @param \CentreonDB $database
     * @param \System $system
     */
    public function __construct($system = null)
    {
        $this->database = new \CentreonDB();

        if (is_null($system)) {
            $this->system = new \System();
        } else {
            $this->system = $system;
        }
    }
    
    /**
     *
     * @param array $pluginPackInfo Informations of the plugin pack
     */
    public function getPluginPackJsonFile($pluginPackInfo, $action = '')
    {
        $pluginPackSlug = $pluginPackInfo['slug'];
        $pluginPackVersion = $pluginPackInfo['version'];
        
        $pluginPackContent = '';

        $pluginPackJsonFileName = 'pluginpack_' .
            str_replace(' ', '_', $pluginPackSlug) .
            '-' .
            $pluginPackVersion . '.json';
        $pluginPackJsonFilePath = EMS_PP_PATH . $pluginPackJsonFileName;

        try {
            $pluginPackContent = json_decode($this->system->fileGetContents($pluginPackJsonFilePath), true);
        } catch (\Exception $e) {
            throw new \Exception("Can't parse plugin pack json file : " . $pluginPackJsonFilePath);
        }

        return $pluginPackContent;
    }
    
    /**
     *
     * @param array $pluginPackInfo Informations of the plugin pack
     */
    public function getPluginPackJsonFileOfLastVersion($pluginPackInfo)
    {
        $bFind = false;
        $sName = 'pluginpack_' . str_replace(' ', '_', $pluginPackInfo['slug']);

        if ($this->system->isDir(EMS_PP_PATH)) {
            foreach (array_reverse($this->system->glob(EMS_PP_PATH . $sName."*")) as $filename) {
                if (!$bFind) {
                    $rr = strpos($filename, strrchr($filename, "-")) + 1;
                    $sVersionFile = substr($filename, $rr, -5);
                    if (version_compare($sVersionFile, $pluginPackInfo['version']) >= 0) {
                        $sGoodVersion = $sVersionFile;
                        $bFind = true;
                    }
                }
            }
        }
        if (!$bFind) {
            throw new \Exception(
                "Can't find plugin pack slug " .
                $pluginPackInfo['slug'] .
                " minimal version " .
                $pluginPackInfo['version']
            );
        } else {
            $pluginPackInfo['version'] = $sGoodVersion;
            return $this->getPluginPackJsonFile($pluginPackInfo);
        }
    }
}
