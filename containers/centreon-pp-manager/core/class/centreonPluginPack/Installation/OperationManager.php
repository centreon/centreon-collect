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
namespace CentreonPluginPackManager\Installation;

/**
 * Description of InstallManager
 *
 * @author lionel
 */
abstract class OperationManager implements OperationManagerInterface
{
    /**
     *
     * @var array
     */
    protected $pluginPackJson;

    /**
     *
     * @var \CentreonDB
     */
    protected $dbManager;

    /**
     *
     * @var \CentreonPluginPackManager\Installation\iProvider
     */
    protected $pluginPackProvider;

    /**
     *
     * @var \CentreonMedia
     */
    protected $mediaManager;

    /**
     *
     * @var \CentreonLog
     */
    protected $logManager;

    /**
     *
     * @var type
     */
    protected $utilsManager;

    /**
     *
     * @var int
     */
    protected $mediaDirectoryId;

    /**
     *
     * @var array
     */
    protected $unmanagedObjects;

    /**
     *
     * @var array
     */
    protected $managedObjects;

    /**
     *
     * @var array
     */
    protected $preparedStatements = array();

    /**
     *
     * @var array
     */
    protected $managedObjectList = array();

    /**
     *
     * @var array
     */
    protected $icons;

    /**
     *
     */
    public function __construct($impApi)
    {
        $this->dbManager = new \CentreonDB();
        $this->mediaManager = new \CentreonMedia($this->dbManager);
        $this->logManager = new \CentreonLog();

        $this->mediaDirectoryId = $this->mediaManager->addDirectory('ppm');

        $this->pluginPackProvider = \CentreonPluginPackManager\Installation\Provider\ProviderFactory::newProvider($impApi);

        $objectToManage = array(
            'PluginPack',
            'Command',
            'ServiceTemplate',
            'HostTemplate'
        );

        $this->initUtilsObjects();
        $this->initObjectHandlers($objectToManage);
    }

    /**
     *
     * @return array
     */
    public function getPluginPackJson()
    {
        return $this->pluginPackJson;
    }

    /**
     *
     * @param array $newPluginPackJson
     */
    public function setPluginPackJson($newPluginPackJson)
    {
        $this->pluginPackJson = $newPluginPackJson;
    }

    /**
     *
     * @return \CentreonDB
     */
    public function getDbManager()
    {
        return $this->dbManager;
    }

    /**
     *
     * @param \CentreonDB $newDbManager
     */
    public function setDbManager(\CentreonDB $newDbManager)
    {
        $this->dbManager = $newDbManager;
    }

    /**
     *
     * @return \CentreonPluginPackManager\Installation\iProvider
     */
    public function getPluginPackProvider()
    {
        return $this->pluginPackProvider;
    }

    /**
     *
     * @param \CentreonPluginPackManager\Installation\iProvider $newPluginPackProvider
     */
    public function setPluginPackProvider(\CentreonPluginPackManager\Installation\iProvider $newPluginPackProvider)
    {
        $this->pluginPackProvider = $newPluginPackProvider;
    }

    /**
     *
     * @return \CentreonMedia
     */
    public function getMediaManager()
    {
        return $this->mediaManager;
    }

    /**
     *
     * @param \CentreonMedia $newMediaManager
     */
    public function setMediaManager(\CentreonMedia $newMediaManager)
    {
        $this->mediaManager = $newMediaManager;
    }

    /**
     * \CentreonLog
     */
    public function getLogManager()
    {
        return $this->logManager;
    }

    /**
     *
     * @param \CentreonLog $newLogManager
     */
    public function setLogManager($newLogManager)
    {
        $this->logManager = $newLogManager;
    }

    /**
     * @return array
     */
    public function getPreparedStatements()
    {
        return $this->preparedStatements;
    }

    /**
     *
     * @param string $name
     * @param string $query
     * @throws \Exception
     */
    public function addPreparedStatement($name, $query)
    {
        // Statement Preparation
        $statementPrepared = $this->dbManager->prepare($query);
        if (\PEAR::isError($statementPrepared)) {
            throw new \Exception($statementPrepared->getMessage(), $statementPrepared->getCode());
        }
        $this->preparedStatements[$name] = $statementPrepared;
    }

    /**
     *
     * @return array
     */
    public function getManagedObjectList()
    {
        return $this->managedObjectList;
    }

    /**
     *
     * @param array $newManagedObjectList
     */
    public function setManagedObjectList($newManagedObjectList)
    {
        $this->managedObjectList = $newManagedObjectList;
    }

    /**
     *
     */
    public function installIcons()
    {
        $this->utilsManager['iconManager']->setObjectParams($this->prepareImageName());
        $this->utilsManager['iconManager']->initUtilsObjects($this->utilsManager);
        $this->utilsManager['iconManager']->setMediaDirectory($this->mediaDirectoryId);
        $this->utilsManager['iconManager']->launchInstall();
        $this->icons = $this->utilsManager['iconManager']->getIcons();
    }

    /**
     *
     * @return array
     */
    private function prepareImageName()
    {
        $iconList = $this->pluginPackJson[$this->utilsManager['iconManager']->getKeyInJson()];
        foreach ($iconList as &$icon) {
            $icon['name'] = $this->pluginPackJson['information']['slug'] . '-' . $icon['name'];
        }
        return $iconList;
    }

    /**
     *
     * @param array $objectToManageList
     */
    private function initObjectHandlers($objectToManageList)
    {
        $objectClasNamespace = "\\CentreonPluginPackManager\\Installation\\Object\\";
        foreach ($objectToManageList as $objectToManage) {
            $objectName = $objectClasNamespace . $objectToManage;
            $currentObject = new $objectName($this->dbManager);
            $currentObject->setIcons($this->icons);
            $currentObject->initUtilsObjects($this->utilsManager);
            $this->managedObjectList[$objectName] = $currentObject;
        }
    }

    /**
     *
     * @param mixed $managedObject
     */
    protected function setParamsToManagedObject($managedObject)
    {
        $managedObject->setObjectParams($this->pluginPackJson[$managedObject->getKeyInJson()]);
        $additionalParamsForObjectKeys = $managedObject->getAdditionnalParamsKeysInJson();
        foreach ($additionalParamsForObjectKeys as $additionalParamsForObjectKey) {
            $managedObject->addAdditionnalParams(
                $additionalParamsForObjectKey,
                $this->pluginPackJson[$additionalParamsForObjectKey]
            );
        }
    }

    /**
     *
     */
    private function initUtilsObjects()
    {
        // Init Template Sorter
        $this->utilsManager['templateSorter'] = new Utils\Sorter();
        $this->utilsManager['mediaManager'] = $this->mediaManager;
        $this->utilsManager['iconManager'] = new Object\Icon($this->dbManager);
    }

    /**
     *
     * @param array $pluginPackInfo
     * @throws \Exception
     */
    public function loadPluginPackJsonFile($pluginPackInfo, $action = '')
    {
        if (is_null($pluginPackInfo['slug'])) {
            throw new \Exception("The slug of the plugin pack can't be retrieve");
        }
        $this->pluginPackJson = $this->pluginPackProvider->getPluginPackJsonFile($pluginPackInfo, $action);
    }

    /**
     * Check keys in json which are not managed
     */
    protected function initUnmanagedObjects()
    {
        $this->unmanagedObjects = array();
        $managedKeys = array();

        $utilsObjectList = array_values($this->utilsManager);
        $objectList = array_merge($utilsObjectList, $this->managedObjectList);

        foreach ($objectList as $object) {
            if (method_exists($object, 'getKeyInJson')) {
                $managedKeys[] = $object->getKeyInJson();
            }
        }

        $this->managedObjects = $managedKeys;

        foreach ($this->pluginPackJson as $key => $value) {
            if (!in_array($key, $managedKeys)) {
                $this->unmanagedObjects[] = $key;
            }
        }

        $pluginPack = $this->pluginPackJson['information']['slug'];

        if (count($this->unmanagedObjects) != 0) {
            $this->setUnmanagedObjects($pluginPack, 0);
        } else {
            $this->setUnmanagedObjects($pluginPack, 1);
        }

    }

    /**
     * Check keys in json which are not managed
     */
    protected function setUnmanagedObjects($pluginPack, $complete)
    {
        $pluginpackIdQuery = "SELECT `pluginpack_id` "
            . "FROM `mod_ppm_pluginpack` "
            . "WHERE `slug` ='" . $pluginPack . "'";

        $result = $this->dbManager->query($pluginpackIdQuery);
        $row = $result->fetchRow();
        $pluginpackId = $row['pluginpack_id'];

        $updatePP = "UPDATE `mod_ppm_pluginpack` SET `complete` = '" .
            $complete . "' WHERE `pluginpack_id` =" . $pluginpackId;
        $this->dbManager->query($updatePP);
    }


}
