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
 * Description of Update
 *
 * @author lionel
 */
class Update extends OperationManager
{
    /**
     * 
     * @param type $impApi
     */
    public function __construct($impApi)
    {
        parent::__construct($impApi);
    }
    
    /**
     * 
     * @param array $pluginPackList
     * @return array
     */
    public function launchOperation($pluginPackList)
    {
        $pluginSuccessfullyUpdated = array();
        $pluginFailedUpdate = array();
        
        // Perform pre-operation task
        $this->prepareUpdate();
        
        // Activate Transaction
        $this->dbManager->autoCommit(false);
        
        foreach ($pluginPackList as $plugin) {
            try {
                $this->loadPluginPackJsonFile($plugin, 'update');
                $this->installIcons();
                $this->updateObject();
                $this->initUnmanagedObjects();

                // Commit curent transaction
                $this->dbManager->commit();
                $pluginSuccessfullyUpdated[] = $plugin;
            } catch (\Exception $ex) {
                $pluginFailedUpdate['problematic'] = $plugin;
                $pluginFailedUpdate['remaining'] = array_diff($pluginPackList, $pluginSuccessfullyUpdated);
                $this->logManager->insertLog(2, $ex->getMessage(), 0, 'centreon-pp-manager');
                $this->dbManager->rollback();
                break;
            }
        }
        
        $this->dbManager->autoCommit(true);
        
        return array(
            'updated' => $pluginSuccessfullyUpdated,
            'failed' => $pluginFailedUpdate,
            'unmanagedObjects' => $this->unmanagedObjects,
            'managedObjects' => $this->managedObjects
        );
    }
    
    /**
     * Perform pre-operation task
     */
    private function prepareUpdate()
    {
        foreach ($this->managedObjectList as $managedObject) {
            $managedObject->prepareUpdate();
        }
    }
    
    /**
     * Perform operation on each object
     */
    private function updateObject()
    {
        foreach ($this->managedObjectList as $managedObject) {
            $managedObject->setObjectParams($this->pluginPackJson[$managedObject->getKeyInJson()]);
            $managedObject->setPluginPackSlug($this->pluginPackJson['information']['slug']);
            $managedObject->setIcons($this->icons);
            $managedObject->launchUpdate();
        }
    }
}
