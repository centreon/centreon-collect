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
 * Description of Install
 *
 * @author lionel
 */
class Install extends OperationManager
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
        $pluginSuccessfullyInstalled = array();
        $pluginFailedInstallation = array();

        // Perform pre-operation task
        $this->prepareInstall();

        // Activate Transaction
        $this->dbManager->autoCommit(false);

        foreach ($pluginPackList as $plugin) {
            try {
                $this->loadPluginPackJsonFile($plugin, 'install');
                $this->installIcons();
                $this->installObject();
                $this->initUnmanagedObjects();

                // Commit current transaction
                $this->dbManager->commit();
                $pluginSuccessfullyInstalled[] = $plugin;
            } catch (\Exception $ex) {
                $pluginFailedInstallation['problematic'] = $plugin;
                $pluginFailedInstallation['remaining'] = array_diff($pluginPackList, $pluginSuccessfullyInstalled);
                $this->logManager->insertLog(2, $ex->getMessage(), 0, 'centreon-pp-manager');
                $this->dbManager->rollback();
                break;
            }
        }

        $this->dbManager->autoCommit(true);

        return array(
            'installed' => $pluginSuccessfullyInstalled,
            'failed' => $pluginFailedInstallation,
            'unmanagedObjects' => $this->unmanagedObjects,
            'managedObjects' => $this->managedObjects
        );
    }

    /**
     * Perform pre-operation task
     */
    private function prepareInstall()
    {
        foreach ($this->managedObjectList as $managedObject) {
            $managedObject->prepareInstall();
        }
    }

    /**
     * Perform operation on each object
     */
    private function installObject()
    {
        foreach ($this->managedObjectList as $managedObject) {
            $this->setParamsToManagedObject($managedObject);
            $managedObject->setPluginPackSlug($this->pluginPackJson['information']['slug']);
            $managedObject->setIcons($this->icons);
            $managedObject->launchInstall();
        }
    }
}
