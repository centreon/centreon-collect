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
namespace CentreonPluginPackManager\Installation\Object;

/**
 *
 * @author lionel
 */
abstract class ObjectManager implements ObjectManagerInterface
{
    /**
     *
     * @var \CentreonDB 
     */
    protected $dbManager;

    /**
     *
     * @var int
     */
    protected $pluginPackId;
    
    /**
     *
     * @var string 
     */
    protected $pluginPackSlug;

    /**
     *
     * @var string 
     */
    protected $keyInJson;
    
    /**
     *
     * @var type 
     */
    protected $additionnalParamsKeysInJson = array();


    /**
     *
     * @var array 
     */
    protected $additionnalParams = array();
    
    /**
     *
     * @var array 
     */
    protected $utilsManager = array();
    
    /**
     *
     * @var array 
     */
    protected $neededUtils = array();

    /**
     *
     * @var array
     */
    protected $icons;

    /**
     * 
     * @param type $database
     */
    public function __construct($database)
    {
        $this->dbManager = $database;
        $this->additionnalParamsKeysInJson = array('information');
    }
    
    /**
     * 
     * @return string
     */
    public function getKeyInJson()
    {
        return $this->keyInJson;
    }

    /**
     * 
     * @param string $newKey
     */
    public function setKeyInJson($newKey)
    {
        $this->keyInJson = $newKey;
    }

    /**
     *
     * @return int
     */
    public function getPluginPackId()
    {
        if (!is_null($this->pluginPackId)) {
            return $this->pluginPackId;
        }

        $pluginpackIdQuery = "SELECT `pluginpack_id` "
            . "FROM `mod_ppm_pluginpack` "
            . "WHERE `name` ='".$this->additionnalParams['information']['name']."'";

        $result = $this->dbManager->query($pluginpackIdQuery);
        $row = $result->fetchRow();
        $this->pluginpackId = $row['pluginpack_id'];

        return $this->pluginpackId;
    }
    
    /**
     * 
     * @return string
     */
    public function getPluginPackSlug()
    {
        return $this->pluginPackSlug;
    }
    
    /**
     * 
     * @param string $newPluginPackSlug
     */
    public function setPluginPackSlug($newPluginPackSlug)
    {
        $this->pluginPackSlug = $newPluginPackSlug;
    }
    
    /**
     * 
     * @return array
     */
    public function getAdditionnalParamsKeysInJson()
    {
        return $this->additionnalParamsKeysInJson;
    }
    
    /**
     * 
     * @param type $newKey
    */
    public function setAdditionnalParamsKeyInJson($newKeys)
    {
        $this->additionnalParamsKeyInJson = $newKeys;
    }
    
    /**
     * 
     * @return array
     */
    public function getAdditionnalParams()
    {
        return $this->additionnalParams;
    }
    
    /**
     * 
     * @param array $newAdditionnalParams
     */
    public function setAdditionnalParams($newAdditionnalParams)
    {
        $this->additionnalParams = $newAdditionnalParams;
    }
    
    /**
     * 
     * @param string $key
     * @param type $values
     */
    public function addAdditionnalParams($key, $values)
    {
        $this->additionnalParams[$key] = $values;
    }

    /**
     *
     * @return array
     */
    public function getIcons()
    {
        return $this->icons;
    }

    /**
     *
     * @param array $icons
     */
    public function setIcons($icons)
    {
        $this->icons = $icons;
    }
    
    /**
     * 
     * @param array $availableUtils
     */
    public function initUtilsObjects(&$availableUtils)
    {
        foreach ($this->neededUtils as $util) {
            if (isset($availableUtils[$util])) {
                $this->utilsManager[$util] = $availableUtils[$util];
            }
        }
    }
}
