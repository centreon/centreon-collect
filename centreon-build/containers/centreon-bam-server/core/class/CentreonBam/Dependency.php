<?php
/*
 * Centreon
 *
 * Source Copyright 2005-2016 Centreon
 *
 * Unauthorized reproduction, copy and distribution
 * are not allowed.
 *
 * For more informations : contact@centreon.com
 *
 */

/**
 * Description of Dependency
 *
 * @author lionel
 */
class CentreonBam_Dependency
{
    /**
     *
     * @var type 
     */
    private $db;
    
    /**
     *
     * @var integer 
     */
    private $id;

    /**
     *
     * @var string 
     */
    private $name;
    
    /**
     *
     * @var string 
     */
    private $description;
    
    /**
     *
     * @var boolean 
     */
    private $inheritsParent;
    
    /**
     *
     * @var string 
     */
    private $executionFailureCriteria;
    
    /**
     *
     * @var string 
     */
    private $notificationFailureCriteria;
    
    /**
     *
     * @var array 
     */
    private $parentBusinessActivity;
    
    /**
     *
     * @var array 
     */
    private $childrenBusinessActivity;
    
    /**
     *
     * @var type 
     */
    private $baObj;


    /**
     * 
     * @param type $db
     * @param type $baObj
     */
    public function __construct($db, $baObj)
    {
        $this->db = $db;
        $this->baObj = $baObj;
    }
    
    /**
     * 
     * @return integer
     */
    public function getId()
    {
        return $this->id;
    }

    /**
     * 
     * @return string
     */
    public function getName()
    {
        return $this->name;
    }
    
    /**
     * 
     * @return string
     */
    public function getDescription()
    {
        return $this->description;
    }
    
    /**
     * 
     * @return boolean
     */
    public function getInheritsParent()
    {
        return $this->inheritsParent;
    }
    
    /**
     * 
     * @return string
     */
    public function getExecutionFailureCriteria()
    {
        return $this->executionFailureCriteria;
    }
    
    /**
     * 
     * @return string
     */
    public function getNotificationFailureCriteria()
    {
        return $this->notificationFailureCriteria;
    }
    
    /**
     * 
     * @return array
     */
    public function getParentBusinessActivity()
    {
        return $this->parentBusinessActivity;
    }
    
    /**
     * 
     * @return array
     */
    public function getChildrenBusinessActivity()
    {
        return $this->childrenBusinessActivity;
    }
    
    /**
     * 
     * @return array
     */
    public function getAllParametersAsArray()
    {
        $parametersList = array();
        
        $parametersList['dep_id'] = $this->id;
        $parametersList['dep_name'] = $this->name;
        $parametersList['dep_description'] = $this->description;
        $parametersList['inherits_parent'] = $this->inheritsParent;
        
        // 
        $executionFailureCriteria = array();
        $explodedExecutionFailureCriteria = explode(',', $this->executionFailureCriteria);
        foreach ($explodedExecutionFailureCriteria as $currentCriteria) {
            $executionFailureCriteria[$currentCriteria] = '1';
        }
        $parametersList['execution_failure_criteria'] = $executionFailureCriteria;
        
        // 
        $notificationFailureCriteria = array();
        $explodedNotificationFailureCriteria = explode(',', $this->notificationFailureCriteria);
        foreach ($explodedNotificationFailureCriteria as $currentCriteria) {
            $notificationFailureCriteria[$currentCriteria] = '1';
        }
        $parametersList['notification_failure_criteria'] = $notificationFailureCriteria;
        
        
        $parametersList['dep_bamParents'] = $this->parentBusinessActivity;
        $parametersList['dep_bamChilds'] = $this->childrenBusinessActivity;
        
        return $parametersList;
    }
    
    /**
     * 
     * @param integer $newId
     */
    public function setId($newId)
    {
        $this->id = $newId;
    }

    /**
     * 
     * @param string $newName
     */
    public function setName($newName)
    {
        $this->name = $newName;
    }

    /**
     * 
     * @param string $newDescription
     */
    public function setDescription($newDescription)
    {
        $this->description = $newDescription;
    }
    
    /**
     * 
     * @param boolean $inherit
     */
    public function setInheritsParent($inherit)
    {
        $this->inheritsParent = $inherit;
    }

    /**
     * 
     * @param string $newExecutionFailureCriteria
     */
    public function setExecutionFailureCriteria($newExecutionFailureCriteria)
    {
        $this->executionFailureCriteria = $newExecutionFailureCriteria;
    }
    
    /**
     * 
     * @param string $newNotificationFailureCriteria
     */
    public function setNotificationFailureCriteria($newNotificationFailureCriteria)
    {
        $this->notificationFailureCriteria = $newNotificationFailureCriteria;
    }
    
    /**
     * 
     * @param array $newParentBusinessActivityList
     */
    public function setParentBusinessActivity($newParentBusinessActivityList)
    {
        $this->parentBusinessActivity = $newParentBusinessActivityList;
    }
    
    /**
     * 
     * @param array $newChildrenBusinessActivityList
     */
    public function setChildrenBusinessActivity($newChildrenBusinessActivityList)
    {
        $this->childrenBusinessActivity = $newChildrenBusinessActivityList;
    }
    
    /**
     * 
     * @param string $newParentBusinessActivity
     */
    public function addParentBusinessActivity($newParentBusinessActivity)
    {
        $this->parentBusinessActivity[] = $newParentBusinessActivity;
    }
    
    /**
     * 
     * @param string $newChildrenBusinessActivity
     */
    public function addChildrenBusinessActivity($newChildrenBusinessActivity)
    {
        $this->childrenBusinessActivity[] = $newChildrenBusinessActivity;
    }
    
    /**
     * 
     * @param array $dependencyParameters
     */
    public function create($dependencyParameters)
    {
        $this->fillParameters($dependencyParameters);
        $this->save(true);
    }
    
    /**
     * 
     * @param array $dependencyParameters
     */
    public function update($dependencyParameters)
    {
        $this->fillParameters($dependencyParameters);
        $this->save();
    }

    /**
     * 
     * @param array $dependencyParameters
     */
    private function fillParameters($dependencyParameters)
    {
        $this->setName($dependencyParameters['name']);
        $this->setDescription($dependencyParameters['description']);
        $this->setInheritsParent($dependencyParameters['inherit']);
        $this->setExecutionFailureCriteria($dependencyParameters['execution_failure_criteria']);
        $this->setNotificationFailureCriteria($dependencyParameters['notification_failure_criteria']);
        $this->setParentBusinessActivity($dependencyParameters['parents']);
        $this->setChildrenBusinessActivity($dependencyParameters['children']);
    }
    
    /**
     * 
     * @param boolean $new
     */
    public function save($new = false)
    {
        $query = '';
        if ($new) {
            $query .= 'INSERT INTO dependency('
                . 'dep_name, '
                . 'dep_description, '
                . 'inherits_parent, '
                . 'execution_failure_criteria, '
                . 'notification_failure_criteria'
                . ') ';
            $query .= "VALUES("
                . "'$this->name', "
                . "'$this->description', "
                . "'$this->inheritsParent', "
                . "'$this->executionFailureCriteria', "
                . "'$this->notificationFailureCriteria'"
                . ")";
            
        } else {
            $query .= "UPDATE dependency SET "
                . "dep_name = '$this->name', "
                . "dep_description = '$this->description', "
                . "inherits_parent = '$this->inheritsParent', "
                . "execution_failure_criteria = '$this->executionFailureCriteria', "
                . "notification_failure_criteria = '$this->notificationFailureCriteria' "
                . "WHERE dep_id = $this->id";
        }
        
        $this->db->query($query);
        
        // get Id
        $queryGetId = "SELECT dep_id FROM dependency WHERE dep_name = '$this->name' LIMIT 1";
        $res = $this->db->query($queryGetId);
        $row = $res->fetchRow();
        $this->id = $row['dep_id'];
        
        // Save Parentship
        $this->saveBaParents();
        $this->saveBaChildren();
    }
    
    /**
     * 
     */
    private function saveBaParents()
    {
        $queryDeleteParents = "DELETE FROM dependency_serviceParent_relation WHERE dependency_dep_id = '$this->id'";
        $this->db->query($queryDeleteParents);
        
        $queryInsertParents = "INSERT INTO dependency_serviceParent_relation "
            . "(dependency_dep_id, service_service_id, host_host_id) "
            . "VALUES ";
        
        foreach ($this->parentBusinessActivity as $parent) {
            $hostService = $this->baObj->getCentreonConfigurationHostServiceIdByBaId($parent);
            $queryInsertParents .= "($this->id, $hostService[service], $hostService[host]), ";
        }
        
        $this->db->query(trim($queryInsertParents, ', '));
    }
    
    /**
     * 
     */
    private function saveBaChildren()
    {
        $queryDeleteChildren = "DELETE FROM dependency_serviceChild_relation WHERE dependency_dep_id = '$this->id'";
        $this->db->query($queryDeleteChildren);
        
        $queryInsertChildren = "INSERT INTO dependency_serviceChild_relation "
            . "(dependency_dep_id, service_service_id, host_host_id) "
            . "VALUES ";
        
        foreach ($this->childrenBusinessActivity as $child) {
            $hostService = $this->baObj->getCentreonConfigurationHostServiceIdByBaId($child);
            $queryInsertChildren .= "($this->id, $hostService[service], $hostService[host]), ";
        }
        
        $this->db->query(trim($queryInsertChildren, ', '));
    }
    
    /**
     * 
     */
    public function delete()
    {
        $queryDelete = "DELETE FROM dependency WHERE dep_id = '$this->id'";
        $this->db->query($queryDelete);
    }
    
    /**
     * 
     * @param PearDB $db
     * @param array $ids
     */
    public static function deleteByIds($db, $ids = array())
    {
        foreach ($ids as $id) {
            $queryDelete = "DELETE FROM dependency WHERE dep_id = '$id'";
            $db->query($queryDelete);
        }
    }
    
    /**
     * 
     * @param PearDB $db
     * @param integer $num
     * @param integer $limit
     * @return array
     */
    public static function getList($db, $num = 0, $limit = 30)
    {
        $query = "SELECT DISTINCT dep_id, dep_name, dep_description FROM "
            . "dependency dep, dependency_serviceParent_relation depparent "
            . "WHERE depparent.dependency_dep_id = dep.dep_id "
            . "AND depparent.host_host_id IN (SELECT host_id FROM host WHERE host_name LIKE '_Module_BAM%') "
            . "ORDER BY dep_name, dep_description LIMIT ".$num * $limit.", ".$limit;
        
        $res = $db->query($query);
        
        $dependencyList = array();
        
        while($row = $res->fetchRow()) {
            $dependencyList[] = $row;
        }
        
        return $dependencyList;
    }
    
    /**
     * 
     * @param PearDB $db
     * @param integer $depId
     * @return \CentreonBam_Dependency
     */
    public static function loadById($db, $dbMon, $depId, $baObj)
    {
        $queryGetDependency = "SELECT * FROM dependency WHERE dep_id = $depId LIMIT 1";
        $res = $db->query($queryGetDependency);
        
        // 
        $row = $res->fetchRow();
        
        // 
        $myDependency = new CentreonBam_Dependency($db, $dbMon);
        $myDependency->setId($row['dep_id']);
        $myDependency->setName($row['dep_name']);
        $myDependency->setDescription($row['dep_description']);
        $myDependency->setInheritsParent($row['inherits_parent']);
        $myDependency->setExecutionFailureCriteria($row['execution_failure_criteria']);
        $myDependency->setNotificationFailureCriteria($row['notification_failure_criteria']);
        
        // 
        $queryGetParents = "SELECT service_service_id FROM dependency_serviceParent_relation where dependency_dep_id = $row[dep_id]";
        $parents = array();
        $resP = $db->query($queryGetParents);
        while($rowP = $resP->fetchRow()) {
            $parents[] = $baObj->getBaIdFromCentreonConfigurationService($rowP['service_service_id']);
        }
        $myDependency->setParentBusinessActivity($parents);
        
        // 
        $queryGetChildren = "SELECT service_service_id FROM dependency_serviceChild_relation where dependency_dep_id = $row[dep_id]";
        $children = array();
        $resC = $db->query($queryGetChildren);
        while($rowC = $resC->fetchRow()) {
            $children[] = $baObj->getBaIdFromCentreonConfigurationService($rowC['service_service_id']);
        }
        $myDependency->setChildrenBusinessActivity($children);
        
        return $myDependency;
    }
}
