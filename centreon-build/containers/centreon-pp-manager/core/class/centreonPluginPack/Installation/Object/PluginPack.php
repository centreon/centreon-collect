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
 * Description of InstallPp
 *
 * @author lionel
 */
class PluginPack extends ObjectManager
{
    /**
     *
     * @var array 
     */
    private $pluginPackInformation;

    /**
     *
     * @var array
     */
    public $icons;

    /**
     *
     * @var type 
     */
    private $insertPpStatement;
    
    /**
     *
     * @var type 
     */
    private $idRetrievalStatement;
    
    /**
     *
     * @var type 
     */
    private $updatePpStatement;

    /**
     *
     * @var type 
     */
    private $removePpStatement;
    
    /**
     *
     * @var type 
     */
    private $dependencyStorageStatement;
    
    /**
     *
     * @var type 
     */
    private $dependencyRemovalStatement;


    /**
     * 
     * @param \CentreonDB $database
     */
    public function __construct($database)
    {
        parent::__construct($database);
        $this->keyInJson = 'information';
        $this->neededUtils = array('iconManager');
        $this->prepareIdRetrieval();
    }
    
    /**
     * 
     * @return array
     */
    public function getObjectParams()
    {
        return $this->pluginpackInformation;
    }
    
    /**
     * 
     * @param array $newObjectParams
     */
    public function setObjectParams($newObjectParams)
    {
        $this->pluginPackInformation = $newObjectParams;
    }
    
    /**
     * 
     * @throws \Exception
     */
    public function prepareIdRetrieval()
    {
        $queryIdRetrieval = "SELECT pluginpack_id FROM `mod_ppm_pluginpack` WHERE `slug` = ?";
        
        // Statement Preparation
        $this->idRetrievalStatement = $this->dbManager->prepare($queryIdRetrieval);
        if (\PEAR::isError($this->idRetrievalStatement)) {
            throw new \Exception($this->idRetrievalStatement->getMessage(), $this->idRetrievalStatement->getCode());
        }
    }

    /**
     * Prepare install
     */
    public function prepareInstall()
    {
        // Define Prepare Statement
        $insertPpQuery = "INSERT INTO `mod_ppm_pluginpack`("
            . "`name`, `slug`, `version`, `status`, `status_message`, "
            . "`changelog`, `icon`, `monitoring_procedure`"
            . ") VALUES("
            . "?, ?, ?, ?, ?,  "
            . "?, ?, ?"
            . ")";
        
        // Statement Preparation
        $this->insertPpStatement = $this->dbManager->prepare($insertPpQuery);
        if (\PEAR::isError($this->insertPpStatement)) {
            throw new \Exception($this->insertPpStatement->getMessage(), $this->insertPpStatement->getCode());
        }
        
        $this->prepareDependencyStorage();
    }

    /**
     * Prepare update
     */
    public function prepareUpdate()
    {
        $updateQuery = "UPDATE `mod_ppm_pluginpack` "
            . "SET "
            . "`name` = ?, `slug` = ?,`version` = ?, `status` = ?, `status_message` = ? "
            . ", `changelog` = ?, `icon` = ?,  monitoring_procedure = ?"
            . "WHERE `slug` = ? ";
        
        // Statement Preparation
        $this->updatePpStatement = $this->dbManager->prepare($updateQuery);
        if (\PEAR::isError($this->updatePpStatement)) {
            throw new \Exception($this->updatePpStatement->getMessage(), $this->updatePpStatement->getCode());
        }
        
        $this->prepareDependencyStorage();
        $this->prepareDependencyRemoval();
    }
    
    /**
     * Prepare uninstall
     * @throws \Exception
     */
    public function prepareUninstall()
    {
        // Define Prepare Statement
        $removePpQuery = $removeQuery = "DELETE FROM `mod_ppm_pluginpack` "
            . "WHERE `slug` = ? ";
        
        // Statement Preparation
        $this->removePpStatement = $this->dbManager->prepare($removePpQuery);
        if (\PEAR::isError($this->removePpStatement)) {
            throw new \Exception($this->removePpStatement->getMessage(), $this->removePpStatement->getCode());
        }
    }
    
    /**
     * 
     * @throws \Exception
     */
    private function prepareDependencyStorage()
    {
        $queryDependencyStorage = "INSERT INTO "
            . "`mod_ppm_pluginpack_dependency`(pluginpack_id, pluginpack_dep_id) "
            . "VALUES (?, ?)";
        
        // Statement Preparation
        $this->dependencyStorageStatement = $this->dbManager->prepare($queryDependencyStorage);
        if (\PEAR::isError($this->dependencyStorageStatement)) {
            throw new \Exception(
                $this->dependencyStorageStatement->getMessage(),
                $this->dependencyStorageStatement->getCode()
            );
        }
    }
    
    /**
     * 
     * @throws \Exception
     */
    private function prepareDependencyRemoval()
    {
        $queryDependencyRemoval = "DELETE FROM "
            . "`mod_ppm_pluginpack_dependency` "
            . "WHERE pluginpack_id = ?";
        
        // Statement Preparation
        $this->dependencyRemovalStatement = $this->dbManager->prepare($queryDependencyRemoval);
        if (\PEAR::isError($this->dependencyRemovalStatement)) {
            throw new \Exception(
                $this->dependencyRemovalStatement->getMessage(),
                $this->dependencyRemovalStatement->getCode()
            );
        }
    }
    
    /**
     * 
     * @param string $ppSlug
     * @return intger
     * @throws \Exception
     */
    private function getPpIdFromDb($ppSlug)
    {
        $resultId = $this->dbManager->execute($this->idRetrievalStatement, $ppSlug);
        if (\PEAR::isError($resultId)) {
            throw new \Exception(
                $resultId->getMessage() . "\n" . $resultId->getUserInfo(),
                $resultId->getCode()
            );
        }
        
        $pp = $resultId->fetchRow();
        
        return $pp['pluginpack_id'];
    }

    /**
     * 
     * @throws \Exception
     */
    public function launchInstall()
    {
        $pluginPackInfo = array(
            $this->pluginPackInformation['name'],
            $this->pluginPackInformation['slug'],
            $this->pluginPackInformation['version'],
            $this->pluginPackInformation['status'],
            $this->pluginPackInformation['status_message'],
            $this->pluginPackInformation['changelog'],
            $this->utilsManager['iconManager']->getIcon($this->pluginPackInformation['icon']),
            $this->pluginPackInformation['monitoring_procedure']
        );
        
        $result = $this->dbManager->execute($this->insertPpStatement, $pluginPackInfo);
        if (\PEAR::isError($result)) {
            throw new \Exception(
                $result->getMessage() . "\n" . $result->getUserInfo(),
                $result->getCode()
            );
        }
        
        $this->launchDependencyStorage(
            $this->getPpIdFromDb($this->pluginPackInformation['slug']),
            $this->pluginPackInformation['dependencies']
        );
    }
    
    /**
     * 
     * @throws \Exception
     */
    public function launchUpdate()
    {
        $pluginPackInfo = array(
            $this->pluginPackInformation['name'],
            $this->pluginPackInformation['slug'],
            $this->pluginPackInformation['version'],
            $this->pluginPackInformation['status'],
            $this->pluginPackInformation['status_message'],
            $this->pluginPackInformation['changelog'],
            $this->utilsManager['iconManager']->getIcon($this->pluginPackInformation['icon']),
            $this->pluginPackInformation['monitoring_procedure'],
            $this->pluginPackInformation['slug']
        );
        
        $result = $this->dbManager->execute($this->updatePpStatement, $pluginPackInfo);
        if (\PEAR::isError($result)) {
            throw new \Exception(
                $result->getMessage() . "\n" . $result->getUserInfo(),
                $result->getCode()
            );
        }
        
        $ppId = $this->getPpIdFromDb($this->pluginPackInformation['slug']);
        
        // Remove depencies
        $this->dbManager->execute($this->dependencyRemovalStatement, $ppId);
        $this->launchDependencyStorage(
            $this->getPpIdFromDb($this->pluginPackInformation['slug']),
            $this->pluginPackInformation['dependencies']
        );
    }
    
    /**
     * 
     * @throws \Exception
     */
    public function launchUninstall()
    {
        $pluginPackInfo = array($this->pluginPackInformation['slug']);
        
        $ppId = $this->getPpIdFromDb($this->pluginPackInformation['slug']);
        if (!$this->isPluginPackRemovable($ppId)) {
            throw new \Exception("The pluginpack can't be removed because of existing dependencies on it");
        }
        
        $result = $this->dbManager->execute($this->removePpStatement, $pluginPackInfo);
        if (\PEAR::isError($result)) {
            throw new \Exception(
                $result->getMessage() . "\n" . $result->getUserInfo(),
                $result->getCode()
            );
        }
    }
    
    /**
     * 
     * @param integer $ppId
     * @param array $dependencies
     * @throws \Exception
     */
    private function launchDependencyStorage($ppId, $dependencies)
    {
        foreach ($dependencies as $dependency) {
            $depId = $this->getPpIdFromDb($dependency['name']);
            $dependencyParams = array($ppId, $depId);
            $result = $this->dbManager->execute($this->dependencyStorageStatement, $dependencyParams);
            if (\PEAR::isError($result)) {
                throw new \Exception(
                    $result->getMessage() . "\n" . $result->getUserInfo(),
                    $result->getCode()
                );
            }
        }
    }
    
    /**
     * 
     * @param integer $ppId
     * @return boolean
     * @throws \Exception
     */
    private function isPluginPackRemovable($ppId)
    {
        $isRemovable = true;
        
        $queryCheckRemoval = "SELECT pluginpack_dep_id FROM mod_ppm_pluginpack_dependency WHERE pluginpack_dep_id = $ppId";
        $result = $this->dbManager->query($queryCheckRemoval);
        
        if (\PEAR::isError($result)) {
            throw new \Exception(
                $result->getMessage() . "\n" . $result->getUserInfo(),
                $result->getCode()
            );
        }

        if ($result->numRows() > 0) {
            $isRemovable = false;
        }
        
        return $isRemovable;
    }
}
