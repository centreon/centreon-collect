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
 * Description of Uninstall
 *
 * @author lionel
 */
class Uninstall extends OperationManager
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
        $pluginSuccessfullyUninstalled = array();
        $pluginFailedUninstallation = array();
        
        // Perform pre-operation task
        $this->prepareUninstall();
        
        // Activate Transaction
        $this->dbManager->autoCommit(false);

        foreach ($pluginPackList as $plugin) {
            try {
                $this->loadPluginPackJsonFile($plugin, 'uninstall');
                $this->uninstallObject();

                // Commit curent transaction
                $this->dbManager->commit();
                $pluginSuccessfullyUninstalled[] = $plugin;
            } catch (\Exception $ex) {
                $pluginFailedUninstallation['problematic'] = $plugin;
                $pluginFailedUninstallation['remaining'] = array_diff($pluginPackList, $pluginSuccessfullyUninstalled);
                $this->logManager->insertLog(2, $ex->getMessage(), 0, 'centreon-pp-manager');
                $this->dbManager->rollback();
                break;
            }
        }
        
        $this->dbManager->autoCommit(true);
        
        return array(
            'uninstalled' => $pluginSuccessfullyUninstalled,
            'failed' => $pluginFailedUninstallation
        );
    }
    
    /**
     * Perform pre-operation task
     */
    private function prepareUninstall()
    {
        foreach ($this->managedObjectList as $managedObject) {
            $managedObject->prepareUninstall();
        }
    }
    
    /**
     * Perform operation on each object
     */
    private function uninstallObject()
    {
        foreach ($this->managedObjectList as &$managedObject) {
            $this->setParamsToManagedObject($managedObject);
            $managedObject->launchUninstall();
        }
    }
    
    /**
     * 
     * @param array $pluginPackInfos
     * @return array
     */
    public function checkUsed($pluginPackInfos)
    {
        $this->loadPluginPackJsonFile($pluginPackInfos);
        
        $linkedObjects = array();
        foreach ($this->managedObjectList as $managedObject) {
            $this->setParamsToManagedObject($managedObject);
            if (method_exists($managedObject, 'checkLinkedObjects')) {
                $managedObject->checkLinkedObjects($linkedObjects);
            }
        }

        return $linkedObjects;
    }
}
