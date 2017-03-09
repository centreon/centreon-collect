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
 * 
 */
class BaEscalation extends AbstractObject
{
    /**
     *
     * @var string 
     */
    protected $generate_filename = 'centreon-bam-escalations.cfg';
    
    /**
     *
     * @var string 
     */
    protected $object_name = 'serviceescalation';
    
    /**
     *
     * @var string 
     */
    protected $attributes_select = "
        esc_id,
        first_notification,
        last_notification,
        notification_interval,
        escalation_period as escalation_period_id,
        escalation_options1 as escalation_options_host,
        escalation_options2 as escalation_options_service
    ";
    
    /**
     *
     * @var array 
     */
    protected $attributes_write = array(
        'first_notification',
        'last_notification',
        'notification_interval',
        'escalation_period',
        'escalation_options',
    );
    
    /**
     *
     * @var array 
     */
    protected $attributes_array = array(
        'host_name',
        'service_description',
        'contact_groups',
    );
    
    /**
     *
     * @var type 
     */
    protected $ba_poller = null;
    
    /**
     *
     * @var type 
     */
    protected $ba_host_id = null;
    
    protected $escalation_cache = array();
    protected $escalation_linked_cg_cache = array();
    protected $escalation_linked_host_cache = array();
    protected $escalation_linked_hg_cache = array();
    protected $escalation_linked_service_cache = array();

    /**
     * 
     * @param type $localhost
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
        
        $this->getEscalationCache();

        $poller_id = Backend::getInstance()->getPollerId();
        $host_name = '_Module_BAM_' . $poller_id;

        $stmt = $this->backend_instance->db->prepare("SELECT
              ber.id_ba,
              $this->attributes_select
            FROM escalation e, mod_bam_escal_relation ber
            WHERE ber.id_esc = e.esc_id
            ");
        $stmt->execute();
        $result = $stmt->fetchAll(PDO::FETCH_ASSOC);
        foreach ($result as $escalation) {
            $escalation['host_name'][] = $host_name;
            $escalation['service_description'][] = 'ba_' . $escalation['id_ba'];
            $escalation['escalation_options'] = $escalation['escalation_options_service'];
            $this->generateSubObjects($escalation, $escalation['esc_id']);
            $this->generateObjectInFile($escalation, 0);
        }

    }
    
    /**
     * 
     * @param type $escalation
     * @param type $esc_id
     */
    private function generateSubObjects(&$escalation, $esc_id)
    {
        $period = Timeperiod::getInstance();
        $cg = BaContactgroup::getInstance();

        $escalation['escalation_period'] = $period->generateFromTimeperiodId($escalation['escalation_period_id']);
        $escalation['contact_groups'] = array();
        foreach ($this->escalation_linked_cg_cache[$esc_id] as $cg_id) {
            $escalation['contact_groups'][] = $cg->generateFromCgId($cg_id);
        }
    }
    
    /**
     * 
     */
    private function getEscalationCache()
    {
        $stmt = $this->backend_instance->db->prepare("SELECT 
                    $this->attributes_select
                FROM escalation
        ");
        $stmt->execute();
        $this->escalation_cache = $stmt->fetchAll(PDO::FETCH_GROUP|PDO::FETCH_UNIQUE|PDO::FETCH_ASSOC);
        
        $stmt = $this->backend_instance->db->prepare("SELECT 
                    escalation_esc_id, contactgroup_cg_id
                FROM escalation_contactgroup_relation
        ");
        $stmt->execute();
        $this->escalation_linked_cg_cache = $stmt->fetchAll(PDO::FETCH_GROUP|PDO::FETCH_COLUMN);
        
        if (count($this->escalation_cache) == 0) {
            $this->has_escalation = 0;
        }
    }

    /**
     * 
     */
    public function reset() {
        parent::reset();
    }
}
