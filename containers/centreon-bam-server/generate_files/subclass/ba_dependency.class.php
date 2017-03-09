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

class BaDependency extends AbstractObject
{
    /**
     *
     * @var string 
     */
    protected $generate_filename = 'centreon-bam-dependencies.cfg';
    
    /**
     *
     * @var string 
     */
    protected $object_name = 'servicedependency';
    
    /**
     *
     * @var array 
     */
    protected $attributes_select = "
        dep_comment,
        execution_failure_criteria,
        notification_failure_criteria,
        inherits_parent
    ";
    
    /**
     *
     * @var array 
     */
    protected $attributes_write = array(
        'execution_failure_criteria',
        'notification_failure_criteria',
        'inherits_parent',
    );
    
    /**
     *
     * @var array 
     */
    protected $attributes_array = array(
        'dependent_host_name',
        'host_name',
        'dependent_service_description',
        'service_description',
    );
    
    /**
     *
     * @var int 
     */
    protected $ba_poller = null;
    
    /**
     *
     * @var int 
     */
    protected $ba_host_id = null;

    /**
     * 
     * @param string $localhost
     * @return int
     */
    public function generateObjects($localhost)
    {
        if ($this->checkGenerate(0)) {
            return 0;
        }

        if (is_null($this->ba_host_id)) {
            $this->ba_host_id = BackendBA::getInstance()->getBaHostId();
        }
        
        Engine::getInstance()->addCfgPath($this->generate_filename);

        $poller_id = Backend::getInstance()->getPollerId();
        $host_name = '_Module_BAM_' . $poller_id;
        $baInstance = BackendBa::getInstance();
        $stmt = $this->backend_instance->db->prepare("SELECT
              p.service_service_id as parent_ba_id, c.service_service_id as child_ba_id,
              $this->attributes_select
            FROM dependency d, dependency_serviceChild_relation c, dependency_serviceParent_relation p
            WHERE p.dependency_dep_id = d.dep_id
            AND c.dependency_dep_id = d.dep_id
            AND p.host_host_id IN (SELECT host_id FROM host WHERE host_name LIKE '_Module_BAM%')
            ");
        $stmt->execute();
        $result = $stmt->fetchAll(PDO::FETCH_ASSOC);
        
        // Load Service Object
        $this->service_instance = Service::getInstance();
        
        foreach ($result as $dependency) {
            try {
                $serviceDescription = $this->service_instance->getString($dependency['parent_ba_id'], 'service_description');
                $dependentServiceDescription = $this->service_instance->getString($dependency['child_ba_id'], 'service_description');
                
                $dependency['host_name'][] = $host_name ;
                $dependency['service_description'][] = $serviceDescription;

                $dependency['dependent_host_name'][] = $host_name;
                $dependency['dependent_service_description'][] = $dependentServiceDescription;

                    $this->generateObjectInFile($dependency, 0);
            } catch (Exception $ex) {
                
            }
        }
    }

    /**
     * 
     */
    public function reset()
    {
        parent::reset();
    }
}
